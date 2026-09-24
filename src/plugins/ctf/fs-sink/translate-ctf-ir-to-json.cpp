/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2024 Philippe Proulx <pproulx@efficios.com>
 */

#include <cstdint>
#include <functional>
#include <optional>

#include <glib.h>

#include <babeltrace2/babeltrace.h>

#include "common/assert.h"
#include "common/common.h"
#include "compat/endian.h" /* IWYU pragma: keep  */
#include "cpp-common/bt2/clock-class.hpp"
#include "cpp-common/bt2/field-class.hpp"
#include "cpp-common/bt2/field-location.hpp"
#include "cpp-common/bt2/trace-ir.hpp"
#include "cpp-common/bt2/value.hpp"
#include "cpp-common/bt2/wrap.hpp"
#include "cpp-common/bt2c/c-string-view.hpp"
#include "cpp-common/bt2c/uuid.hpp"
#include "cpp-common/vendor/nlohmann/json.hpp"

#include "../common/metadata/json-strings.hpp"
#include "fs-sink-ctf-meta.hpp"
#include "translate-ctf-ir-to-json.hpp"

namespace {

namespace jsonstr = ctf::jsonstr;
using nljson = nlohmann::json;

nljson jsonFromIrValue(const bt2::ConstValue val)
{
    switch (val.type()) {
    case bt2::ValueType::Null:
        return nullptr;

    case bt2::ValueType::Bool:
        return val.asBool().value();

    case bt2::ValueType::SignedInteger:
        return val.asSignedInteger().value();

    case bt2::ValueType::UnsignedInteger:
        return val.asUnsignedInteger().value();

    case bt2::ValueType::Real:
        return val.asReal().value();

    case bt2::ValueType::String:
        return *val.asString().value();

    case bt2::ValueType::Array:
        return std::invoke([val] {
            auto json = nljson::array();

            for (const auto elemVal : val.asArray()) {
                json.emplace_back(jsonFromIrValue(elemVal));
            }

            return json;
        });

    case bt2::ValueType::Map:
        return std::invoke([val] {
            auto json = nljson::object();

            val.asMap().forEach([&json](const auto name, const auto itemVal) {
                json[name] = jsonFromIrValue(itemVal);
            });

            return json;
        });

    default:
        bt_common_abort();
    }
}

template <typename IrObjT>
void tryAddAttrsProp(nljson& jsonObj, const IrObjT irObj)
{
    if (!irObj.userAttributes().isEmpty()) {
        jsonObj[jsonstr::attrs] = jsonFromIrValue(irObj.userAttributes());
    }
}

void tryAddOptStrProp(nljson& jsonObj, const char * const name, const bt2c::CStringView str)
{
    if (str) {
        jsonObj[name] = *str;
    }
}

template <typename IrObjT>
void tryAddIdentityProps(nljson& jsonObj, const IrObjT irObj)
{
    tryAddOptStrProp(jsonObj, jsonstr::ns, irObj.nameSpace());
    tryAddOptStrProp(jsonObj, jsonstr::name, irObj.name());
    tryAddOptStrProp(jsonObj, jsonstr::uid, irObj.uid());
}

nljson jsonClkClsFromIr(const bt2::ConstClockClass irClkCls, const std::string& id)
{
    nljson jsonClkCls {
        {jsonstr::type, jsonstr::clkCls},
        {jsonstr::id, id},
        {jsonstr::freq, irClkCls.frequency()},
    };

    tryAddIdentityProps(jsonClkCls, irClkCls);

    if (irClkCls.origin().isUnixEpoch()) {
        jsonClkCls[jsonstr::origin] = jsonstr::unixEpoch;
    } else if (irClkCls.origin().isKnown()) {
        jsonClkCls[jsonstr::origin] = {
            {jsonstr::name, *irClkCls.origin().name()},
            {jsonstr::uid, *irClkCls.origin().uid()},
        };

        tryAddOptStrProp(jsonClkCls, jsonstr::ns, irClkCls.origin().nameSpace());
    }

    if (irClkCls.offsetFromOrigin().seconds() != 0) {
        jsonClkCls[jsonstr::offsetFromOrigin][jsonstr::seconds] =
            irClkCls.offsetFromOrigin().seconds();
    }

    if (irClkCls.offsetFromOrigin().cycles() != 0) {
        jsonClkCls[jsonstr::offsetFromOrigin][jsonstr::cycles] =
            irClkCls.offsetFromOrigin().cycles();
    }

    if (irClkCls.precision()) {
        jsonClkCls[jsonstr::precision] = *irClkCls.precision();
    }

    if (irClkCls.accuracy()) {
        jsonClkCls[jsonstr::accuracy] = *irClkCls.accuracy();
    }

    tryAddOptStrProp(jsonClkCls, jsonstr::descr, irClkCls.description());
    tryAddAttrsProp(jsonClkCls, irClkCls);
    return jsonClkCls;
}

std::string nativeByteOrder()
{
    return BYTE_ORDER == LITTLE_ENDIAN ? jsonstr::littleEndian : jsonstr::bigEndian;
}

nljson jsonFc(const char * const type, const fs_sink_ctf_field_class& fsFc,
              nljson jsonExtra = nljson::object())
{
    jsonExtra.update({
        {jsonstr::type, type},
    });

    tryAddAttrsProp(jsonExtra, bt2::wrap(fsFc.ir_fc));
    return jsonExtra;
}

nljson jsonFixedLenBitArrayFc(const char * const type, const fs_sink_ctf_field_class& fsFc,
                              const unsigned int len, nljson jsonExtra = nljson::object())
{
    jsonExtra.update({
        {jsonstr::align, fsFc.alignment},
        {jsonstr::len, len},
        {jsonstr::byteOrder, nativeByteOrder()},
    });

    return jsonFc(type, fsFc, std::move(jsonExtra));
}

template <typename IrIntRangeSetT>
nljson jsonIntRangeSetFromIr(const IrIntRangeSetT irIntRangeSet)
{
    auto jsonIntRangeSet = nljson::array();

    for (auto irRange : irIntRangeSet) {
        jsonIntRangeSet.emplace_back(nljson::array({irRange.lower(), irRange.upper()}));
    }

    return jsonIntRangeSet;
}

nljson jsonFixedLenBitArrayFcFromFs(const fs_sink_ctf_field_class& fsFc)
{
    const auto irFc = bt2::wrap(fsFc.ir_fc).asBitArray();

    return jsonFixedLenBitArrayFc(
        irFc.flagCount() == 0 ? jsonstr::fixedLenBitArray : jsonstr::fixedLenBitMap, fsFc,
        irFc.length(), std::invoke([irFc] {
            if (irFc.flagCount() > 0) {
                nljson jsonFlags;

                for (const auto flag : irFc) {
                    jsonFlags[*flag.label()] = jsonIntRangeSetFromIr(flag.ranges());
                }

                return nljson {{jsonstr::flags, jsonFlags}};
            }

            return nljson::object();
        }));
}

template <typename IrEnumFcT>
nljson jsonFixedLenIntFcFromFs(const fs_sink_ctf_field_class& fsFc, const char * const intFcTypeStr)
{
    const auto irIntFc = bt2::wrap(fsFc.ir_fc).asInteger();

    return jsonFixedLenBitArrayFc(
        intFcTypeStr, fsFc, irIntFc.fieldValueRange(), std::invoke([irIntFc] {
            nljson jsonExtra;

            if (irIntFc.preferredDisplayBase() != bt2::DisplayBase::Decimal) {
                jsonExtra[jsonstr::prefDispBase] =
                    static_cast<unsigned int>(irIntFc.preferredDisplayBase());
            }

            if (irIntFc.isEnumeration()) {
                nljson jsonMappings;

                for (const auto mapping : irIntFc.as<IrEnumFcT>()) {
                    jsonMappings[*mapping.label()] = jsonIntRangeSetFromIr(mapping.ranges());
                }

                jsonExtra[jsonstr::mappings] = std::move(jsonMappings);
            }

            return jsonExtra;
        }));
}

nljson jsonBlobFcFromFs(const char * const type, const fs_sink_ctf_field_class& fsFc,
                        nljson jsonExtra)
{
    jsonExtra[jsonstr::mediaType] = *bt2::wrap(fsFc.ir_fc).asBlob().mediaType();
    return jsonFc(type, fsFc, std::move(jsonExtra));
}

std::string uniqueKeyMemberName(const fs_sink_ctf_trace& fsTrace,
                                const bt2c::CStringView depMemberName, const char * const type)
{
    return fmt::format("{}-{}-{}", bt2c::UuidView {fsTrace.uuid}.str(), depMemberName, type);
}

nljson jsonFcFromFs(const fs_sink_ctf_trace& fsTrace, bt2c::CStringView memberName,
                    const fs_sink_ctf_field_class& fsFc);

nljson keyFcAttrs()
{
    return {
        {jsonstr::btNs,
         {
             {"is-key-only", true},
         }},
    };
}

constexpr const char *keyTypeLen = "len";
constexpr const char *keyTypeSel = "sel";

nljson jsonStructFcMemberClassesFromFs(const fs_sink_ctf_trace& fsTrace,
                                       const fs_sink_ctf_field_class& fsFc)
{
    auto& fsStructFc = reinterpret_cast<const fs_sink_ctf_field_class_struct&>(fsFc);
    auto json = nljson::array();

    for (auto i = 0U; i < fsStructFc.members->len; ++i) {
        auto& member = g_array_index(fsStructFc.members, const fs_sink_ctf_named_field_class, i);

        if (member.fc->type == FS_SINK_CTF_FIELD_CLASS_TYPE_SEQUENCE ||
            member.fc->type == FS_SINK_CTF_FIELD_CLASS_TYPE_DYN_BLOB) {
            if (auto& fsSeqFc =
                    *reinterpret_cast<const fs_sink_ctf_field_class_sequence *>(member.fc);
                fsSeqFc.length_is_before) {
                /* Generate length field class */
                json.emplace_back(nljson {
                    /* clang-format off */
                    {jsonstr::name, uniqueKeyMemberName(fsTrace, member.name->str, keyTypeLen)},
                    {jsonstr::fc, {
                        {jsonstr::type, jsonstr::fixedLenUInt},
                        {jsonstr::len, 32},
                        {jsonstr::align, 8},
                        {jsonstr::byteOrder, nativeByteOrder()},
                        {jsonstr::attrs, keyFcAttrs()},
                    }},
                    /* clang-format on */
                });
            }
        } else if (member.fc->type == FS_SINK_CTF_FIELD_CLASS_TYPE_OPTION) {
            if (auto& fsOptFc =
                    *reinterpret_cast<const fs_sink_ctf_field_class_option *>(member.fc);
                fsOptFc.tag_is_before) {
                /* Generate selector field class */
                json.emplace_back(nljson {
                    /* clang-format off */
                    {jsonstr::name, uniqueKeyMemberName(fsTrace, member.name->str, keyTypeSel)},
                    {jsonstr::fc, {
                        {jsonstr::type, jsonstr::fixedLenBool},
                        {jsonstr::len, 8},
                        {jsonstr::align, 8},
                        {jsonstr::byteOrder, nativeByteOrder()},
                        {jsonstr::attrs, keyFcAttrs()},
                    }},
                    /* clang-format on */
                });
            }
        } else if (member.fc->type == FS_SINK_CTF_FIELD_CLASS_TYPE_VARIANT) {
            if (auto& fsVarFc =
                    *reinterpret_cast<const fs_sink_ctf_field_class_variant *>(member.fc);
                fsVarFc.tag_is_before) {
                /* Generate selector field class */
                json.emplace_back(nljson {
                    /* clang-format off */
                    {jsonstr::name, uniqueKeyMemberName(fsTrace, member.name->str, keyTypeSel)},
                    {jsonstr::fc, {
                        {jsonstr::type, jsonstr::fixedLenUInt},
                        {jsonstr::len, 16},
                        {jsonstr::align, 8},
                        {jsonstr::byteOrder, nativeByteOrder()},
                        {jsonstr::attrs, keyFcAttrs()},
                    }},
                    /* clang-format on */
                });
            }
        }

        nljson jsonMember {
            {jsonstr::name, member.name->str},
            {jsonstr::fc, jsonFcFromFs(fsTrace, member.name->str, *member.fc)},
        };

        tryAddAttrsProp(jsonMember, bt2::wrap(fsStructFc.base.ir_fc).asStructure()[i]);
        json.emplace_back(std::move(jsonMember));
    }

    return json;
}

nljson jsonStructFcFromFs(const fs_sink_ctf_trace& fsTrace, const fs_sink_ctf_field_class& fsFc)
{
    return jsonFc(jsonstr::structure, fsFc,
                  {
                      {jsonstr::minAlign, fsFc.alignment},
                      {jsonstr::memberClasses, jsonStructFcMemberClassesFromFs(fsTrace, fsFc)},
                  });
}

nljson jsonFieldLocFromIr(const bt2::ConstFieldLocation irFieldLoc)
{
    nljson json {
        {jsonstr::origin, std::invoke([&irFieldLoc] {
             switch (irFieldLoc.rootScope()) {
             case bt2::ConstFieldLocation::Scope::PacketContext:
                 return jsonstr::pktCtx;

             case bt2::ConstFieldLocation::Scope::EventCommonContext:
                 return jsonstr::eventRecordCommonCtx;

             case bt2::ConstFieldLocation::Scope::EventSpecificContext:
                 return jsonstr::eventRecordSpecCtx;

             case bt2::ConstFieldLocation::Scope::EventPayload:
                 return jsonstr::eventRecordPayload;

             default:
                 bt_common_abort();
             }
         })},
        {jsonstr::path, nljson::array()},
    };

    for (auto i = 0U; i < irFieldLoc.length(); ++i) {
        json[jsonstr::path].emplace_back(*irFieldLoc[i]);
    }

    return json;
}

nljson jsonFieldLocFromIrOrGenerated(const fs_sink_ctf_trace& fsTrace,
                                     const std::optional<bt2::ConstFieldLocation>& irFieldLoc,
                                     const bt2c::CStringView depMemberName,
                                     const char * const keyTypeStr)
{
    if (irFieldLoc) {
        return jsonFieldLocFromIr(*irFieldLoc);
    } else {
        return {
            {jsonstr::path, {uniqueKeyMemberName(fsTrace, depMemberName, keyTypeStr)}},
        };
    }
}

nljson jsonOptFcFromFs(const fs_sink_ctf_trace& fsTrace, const bt2c::CStringView memberName,
                       const fs_sink_ctf_field_class& fsFc)
{
    const auto irFc = bt2::wrap(fsFc.ir_fc).asOption();
    nljson jsonExtra {
        {jsonstr::fc,
         jsonFcFromFs(fsTrace, nullptr,
                      *reinterpret_cast<const fs_sink_ctf_field_class_option&>(fsFc).content_fc)},
        {jsonstr::selFieldLoc, std::invoke([&] {
             std::optional<bt2::ConstFieldLocation> irFieldLoc;

             if (irFc.isOptionWithSelector()) {
                 irFieldLoc = irFc.asOptionWithSelector().selectorFieldLocation();
             }

             return jsonFieldLocFromIrOrGenerated(fsTrace, irFieldLoc, memberName, keyTypeSel);
         })},
    };

    if (irFc.isOptionWithSignedIntegerSelector()) {
        jsonExtra[jsonstr::selFieldRanges] =
            jsonIntRangeSetFromIr(irFc.asOptionWithSignedIntegerSelector().ranges());
    } else if (irFc.isOptionWithUnsignedIntegerSelector()) {
        jsonExtra[jsonstr::selFieldRanges] =
            jsonIntRangeSetFromIr(irFc.asOptionWithUnsignedIntegerSelector().ranges());
    } else {
        /* Generated */
        BT_ASSERT(irFc.isOptionWithoutSelector());
        jsonExtra[jsonstr::selFieldRanges] = {{1, 1}};
    }

    return jsonFc(jsonstr::optional, fsFc, std::move(jsonExtra));
}

nljson jsonVarFcOptFromFs(const fs_sink_ctf_trace& fsTrace,
                          const fs_sink_ctf_named_field_class& fsVarFcOpt,
                          const bt2::ConstVariantFieldClassOption irVarFcOpt,
                          nljson jsonSelFieldRanges)
{
    nljson jsonVarFcOpt {
        {jsonstr::selFieldRanges, std::move(jsonSelFieldRanges)},
        {jsonstr::fc, jsonFcFromFs(fsTrace, nullptr, *fsVarFcOpt.fc)},
    };

    if (fsVarFcOpt.name->len > 0) {
        jsonVarFcOpt[jsonstr::name] = fsVarFcOpt.name->str;
    }

    tryAddAttrsProp(jsonVarFcOpt, irVarFcOpt);
    return jsonVarFcOpt;
}

nljson jsonVarFcFromFs(const fs_sink_ctf_trace& fsTrace, const bt2c::CStringView memberName,
                       const fs_sink_ctf_field_class& fsFc)
{
    const auto irVarFc = bt2::wrap(fsFc.ir_fc).asVariant();
    nljson jsonExtra {
        {jsonstr::selFieldLoc, std::invoke([&] {
             std::optional<bt2::ConstFieldLocation> irFieldLoc;

             if (irVarFc.isVariantWithSelector()) {
                 irFieldLoc = irVarFc.asVariantWithSelector().selectorFieldLocation();
             }

             return jsonFieldLocFromIrOrGenerated(fsTrace, irFieldLoc, memberName, keyTypeSel);
         })},
        {jsonstr::opts, nljson::array()},
    };

    /* Options */
    {
        auto& fsVarFc = reinterpret_cast<const fs_sink_ctf_field_class_variant&>(fsFc);

        for (auto i = 0U; i < fsVarFc.options->len; ++i) {
            auto& fsVarFcOpt =
                g_array_index(fsVarFc.options, const fs_sink_ctf_named_field_class, i);

            if (irVarFc.isVariantWithSignedIntegerSelector()) {
                const auto irVarFcOpt = irVarFc.asVariantWithSignedIntegerSelector()[i];

                jsonExtra[jsonstr::opts].emplace_back(
                    jsonVarFcOptFromFs(fsTrace, fsVarFcOpt, irVarFcOpt.asBaseOption(),
                                       jsonIntRangeSetFromIr(irVarFcOpt.ranges())));
            } else if (irVarFc.isVariantWithUnsignedIntegerSelector()) {
                const auto irVarFcOpt = irVarFc.asVariantWithUnsignedIntegerSelector()[i];

                jsonExtra[jsonstr::opts].emplace_back(
                    jsonVarFcOptFromFs(fsTrace, fsVarFcOpt, irVarFcOpt.asBaseOption(),
                                       jsonIntRangeSetFromIr(irVarFcOpt.ranges())));
            } else {
                /* Generated */
                BT_ASSERT(irVarFc.isVariantWithoutSelector());
                jsonExtra[jsonstr::opts].emplace_back(
                    jsonVarFcOptFromFs(fsTrace, fsVarFcOpt, irVarFc[i], {{i, i}}));
            }
        }
    }

    return jsonFc(jsonstr::variant, fsFc, std::move(jsonExtra));
}

nljson jsonFcFromFs(const fs_sink_ctf_trace& fsTrace, const bt2c::CStringView memberName,
                    const fs_sink_ctf_field_class& fsFc)
{
    switch (fsFc.type) {
    case FS_SINK_CTF_FIELD_CLASS_TYPE_BOOL:
        return jsonFixedLenBitArrayFc(jsonstr::fixedLenBool, fsFc, 8);

    case FS_SINK_CTF_FIELD_CLASS_TYPE_BIT_ARRAY:
        return jsonFixedLenBitArrayFcFromFs(fsFc);

    case FS_SINK_CTF_FIELD_CLASS_TYPE_INT:
        return std::invoke([&fsFc] {
            if (const auto irFc = bt2::wrap(fsFc.ir_fc); irFc.isUnsignedInteger()) {
                return jsonFixedLenIntFcFromFs<bt2::ConstUnsignedEnumerationFieldClass>(
                    fsFc, jsonstr::fixedLenUInt);
            } else {
                BT_ASSERT(irFc.isSignedInteger());
                return jsonFixedLenIntFcFromFs<bt2::ConstSignedEnumerationFieldClass>(
                    fsFc, jsonstr::fixedLenSInt);
            }
        });

    case FS_SINK_CTF_FIELD_CLASS_TYPE_FLOAT:
        return jsonFixedLenBitArrayFc(jsonstr::fixedLenFloat, fsFc,
                                      bt2::wrap(fsFc.ir_fc).isSinglePrecisionReal() ? 32 : 64);

    case FS_SINK_CTF_FIELD_CLASS_TYPE_STRING:
        return jsonFc(jsonstr::nullTerminatedStr, fsFc);

    case FS_SINK_CTF_FIELD_CLASS_TYPE_STRUCT:
        return jsonStructFcFromFs(fsTrace, fsFc);

    case FS_SINK_CTF_FIELD_CLASS_TYPE_ARRAY:
        return jsonFc(
            jsonstr::staticLenArray, fsFc,
            {
                {jsonstr::len, bt2::wrap(fsFc.ir_fc).asStaticArray().length()},
                {jsonstr::elemFc,
                 jsonFcFromFs(
                     fsTrace, nullptr,
                     *reinterpret_cast<const fs_sink_ctf_field_class_array_base&>(fsFc).elem_fc)},

            });

    case FS_SINK_CTF_FIELD_CLASS_TYPE_STATIC_BLOB:
        return jsonBlobFcFromFs(jsonstr::staticLenBlob, fsFc,
                                {{jsonstr::len, bt2::wrap(fsFc.ir_fc).asStaticBlob().length()}});

    case FS_SINK_CTF_FIELD_CLASS_TYPE_SEQUENCE:
        return jsonFc(
            jsonstr::dynLenArray, fsFc,
            {
                {jsonstr::lenFieldLoc, std::invoke([&] {
                     const auto irFc = bt2::wrap(fsFc.ir_fc);
                     std::optional<bt2::ConstFieldLocation> irFieldLoc;

                     if (irFc.isDynamicArrayWithLength()) {
                         irFieldLoc = irFc.asDynamicArrayWithLength().lengthFieldLocation();
                     }

                     return jsonFieldLocFromIrOrGenerated(fsTrace, irFieldLoc, memberName,
                                                          keyTypeLen);
                 })},
                {jsonstr::elemFc,
                 jsonFcFromFs(
                     fsTrace, nullptr,
                     *reinterpret_cast<const fs_sink_ctf_field_class_array_base&>(fsFc).elem_fc)},

            });

    case FS_SINK_CTF_FIELD_CLASS_TYPE_DYN_BLOB:
        return jsonBlobFcFromFs(
            jsonstr::dynLenBlob, fsFc,
            {
                {jsonstr::lenFieldLoc, std::invoke([&] {
                     const auto irFc = bt2::wrap(fsFc.ir_fc);
                     std::optional<bt2::ConstFieldLocation> irFieldLoc;

                     if (irFc.isDynamicBlobWithLength()) {
                         irFieldLoc = irFc.asDynamicBlobWithLength().lengthFieldLocation();
                     }

                     return jsonFieldLocFromIrOrGenerated(fsTrace, irFieldLoc, memberName,
                                                          keyTypeLen);
                 })},
            });

    case FS_SINK_CTF_FIELD_CLASS_TYPE_OPTION:
        return jsonOptFcFromFs(fsTrace, memberName, fsFc);

    case FS_SINK_CTF_FIELD_CLASS_TYPE_VARIANT:
        return jsonVarFcFromFs(fsTrace, memberName, fsFc);

    default:
        bt_common_abort();
    }
}

void tryAddScopeFcProp(nljson& jsonObj, const std::string& name, const fs_sink_ctf_trace& fsTrace,
                       const fs_sink_ctf_field_class * const fsFc)
{
    if (fsFc) {
        jsonObj[name] = jsonFcFromFs(fsTrace, nullptr, *fsFc);
    }
}

nljson jsonEventRecordClsFromFs(const fs_sink_ctf_event_class& fsEventRecordCls)
{
    nljson jsonEventRecordCls {
        {jsonstr::type, jsonstr::eventRecordCls},
    };

    /* Stuff from IR */
    {
        const auto irEventCls = bt2::wrap(fsEventRecordCls.ir_ec);

        jsonEventRecordCls[jsonstr::id] = irEventCls.id();
        jsonEventRecordCls[jsonstr::dataStreamClsId] = irEventCls.streamClass().id();
        tryAddIdentityProps(jsonEventRecordCls, irEventCls);
        tryAddAttrsProp(jsonEventRecordCls, irEventCls);

        /* Log level */
        if (irEventCls.logLevel()) {
            jsonEventRecordCls[jsonstr::attrs][jsonstr::btNs][jsonstr::logLevel] =
                std::invoke([irEventCls] {
                    switch (*irEventCls.logLevel()) {
                    case bt2::EventClassLogLevel::Emergency:
                        return jsonstr::logLevelEmergency;
                    case bt2::EventClassLogLevel::Alert:
                        return jsonstr::logLevelAlert;
                    case bt2::EventClassLogLevel::Critical:
                        return jsonstr::logLevelCritical;
                    case bt2::EventClassLogLevel::Error:
                        return jsonstr::logLevelError;
                    case bt2::EventClassLogLevel::Warning:
                        return jsonstr::logLevelWarning;
                    case bt2::EventClassLogLevel::Notice:
                        return jsonstr::logLevelNotice;
                    case bt2::EventClassLogLevel::Info:
                        return jsonstr::logLevelInfo;
                    case bt2::EventClassLogLevel::DebugSystem:
                        return jsonstr::logLevelDebugSystem;
                    case bt2::EventClassLogLevel::DebugProgram:
                        return jsonstr::logLevelDebugProgram;
                    case bt2::EventClassLogLevel::DebugProcess:
                        return jsonstr::logLevelDebugProcess;
                    case bt2::EventClassLogLevel::DebugModule:
                        return jsonstr::logLevelDebugModule;
                    case bt2::EventClassLogLevel::DebugUnit:
                        return jsonstr::logLevelDebugUnit;
                    case bt2::EventClassLogLevel::DebugFunction:
                        return jsonstr::logLevelDebugFunction;
                    case bt2::EventClassLogLevel::DebugLine:
                        return jsonstr::logLevelDebugLine;
                    case bt2::EventClassLogLevel::Debug:
                        return jsonstr::logLevelDebug;
                    default:
                        bt_common_abort();
                    }
                });
        }

        /* EMF URI */
        if (irEventCls.emfUri()) {
            jsonEventRecordCls[jsonstr::attrs][jsonstr::emfUri] = *irEventCls.emfUri();
        }
    }

    tryAddScopeFcProp(jsonEventRecordCls, jsonstr::specCtxFc, *fsEventRecordCls.sc->trace,
                      fsEventRecordCls.spec_context_fc);
    tryAddScopeFcProp(jsonEventRecordCls, jsonstr::payloadFc, *fsEventRecordCls.sc->trace,
                      fsEventRecordCls.payload_fc);
    return jsonEventRecordCls;
}

nljson jsonFixedLenUIntMember(std::string name, const unsigned int len, const char * const role,
                              const unsigned int prefDispBase = 10)
{
    /* clang-format off */
    return {
        {jsonstr::name, std::move(name)},
        {jsonstr::fc, std::invoke([&] {
            nljson json {
                {jsonstr::type, jsonstr::fixedLenUInt},
                {jsonstr::len, len},
                {jsonstr::align, 8},
                {jsonstr::byteOrder, nativeByteOrder()},
                {jsonstr::roles, {role}},
            };

            if (prefDispBase != 10) {
                json[jsonstr::prefDispBase] = prefDispBase;
            }

            return json;
        })},
    };
    /* clang-format on */
}

std::string uniqueMemberName(const fs_sink_ctf_trace& fsTrace, const char * const suffix)
{
    return fmt::format("{}-{}", bt2c::UuidView {fsTrace.uuid}.str(), suffix);
}

nljson jsonDataStreamClsFromFs(const fs_sink_ctf_stream_class& fsStreamCls)
{
    /* clang-format off */
    nljson jsonDataStreamCls {
        {jsonstr::type, jsonstr::dataStreamCls},
        {jsonstr::pktCtxFc, {
            {jsonstr::type, jsonstr::structure},
            {jsonstr::minAlign, fsStreamCls.packet_context_fc ? fsStreamCls.packet_context_fc->alignment : 8},
            {jsonstr::memberClasses, {
                 jsonFixedLenUIntMember(uniqueMemberName(*fsStreamCls.trace, "packet_size"), 64,
                                        jsonstr::pktTotalLen),
                 jsonFixedLenUIntMember(uniqueMemberName(*fsStreamCls.trace, "content_size"), 64,
                                        jsonstr::pktContentLen),
            }},
        }},
        {jsonstr::eventRecordHeaderFc, {
            {jsonstr::type, jsonstr::structure},
            {jsonstr::minAlign, 8},
            {jsonstr::memberClasses, {
                jsonFixedLenUIntMember("id", 64, jsonstr::eventRecordClsId),
            }},
        }},
    };
    /* clang-format on */

    /* Stuff from IR */
    {
        const auto irStreamCls = bt2::wrap(fsStreamCls.ir_sc);

        jsonDataStreamCls[jsonstr::id] = irStreamCls.id();
        tryAddIdentityProps(jsonDataStreamCls, irStreamCls);

        if (fsStreamCls.default_clock_class) {
            jsonDataStreamCls[jsonstr::defClkClsId] = fsStreamCls.default_clock_class->name->str;
        }

        tryAddAttrsProp(jsonDataStreamCls, irStreamCls);
    }

    /*
     * Packet context field class: optional beginning timestamp feature.
     */
    if (fsStreamCls.packets_have_ts_begin) {
        jsonDataStreamCls[jsonstr::pktCtxFc][jsonstr::memberClasses].emplace_back(
            jsonFixedLenUIntMember(uniqueMemberName(*fsStreamCls.trace, "timestamp_begin"), 64,
                                   jsonstr::defClkTs));
    }

    /*
     * Packet context field class: optional end timestamp feature.
     */
    if (fsStreamCls.packets_have_ts_end) {
        jsonDataStreamCls[jsonstr::pktCtxFc][jsonstr::memberClasses].emplace_back(
            jsonFixedLenUIntMember(uniqueMemberName(*fsStreamCls.trace, "timestamp_end"), 64,
                                   jsonstr::pktEndDefClkTs));
    }

    /*
     * Packet context field class: optional discarded event record
     * counter feature.
     */
    if (fsStreamCls.has_discarded_events) {
        jsonDataStreamCls[jsonstr::pktCtxFc][jsonstr::memberClasses].emplace_back(
            jsonFixedLenUIntMember(uniqueMemberName(*fsStreamCls.trace, "events_discarded"), 64,
                                   jsonstr::discEventRecordCounterSnap));
    }

    /* Packet context field class: packet sequence number feature */
    jsonDataStreamCls[jsonstr::pktCtxFc][jsonstr::memberClasses].emplace_back(
        jsonFixedLenUIntMember(uniqueMemberName(*fsStreamCls.trace, "packet_seq_num"), 64,
                               jsonstr::pktSeqNum));

    /* Packet context field class: user member classes */
    if (fsStreamCls.packet_context_fc) {
        auto& jsonMemberClasses = jsonDataStreamCls[jsonstr::pktCtxFc][jsonstr::memberClasses];
        const auto jsonUserMemberClasses =
            jsonStructFcMemberClassesFromFs(*fsStreamCls.trace, *fsStreamCls.packet_context_fc);

        jsonMemberClasses.insert(jsonMemberClasses.end(), jsonUserMemberClasses.begin(),
                                 jsonUserMemberClasses.end());
    }

    /* Event record header field class: optional timestamp feature */
    if (fsStreamCls.default_clock_class) {
        jsonDataStreamCls[jsonstr::eventRecordHeaderFc][jsonstr::memberClasses].emplace_back(
            jsonFixedLenUIntMember("timestamp", 64, jsonstr::defClkTs));
    }

    tryAddScopeFcProp(jsonDataStreamCls, jsonstr::eventRecordCommonCtxFc, *fsStreamCls.trace,
                      fsStreamCls.event_common_context_fc);
    return jsonDataStreamCls;
}

nljson jsonTraceClsFromFs(const fs_sink_ctf_trace& fsTrace)
{
    /* clang-format off */
    nljson jsonTraceCls {
        {jsonstr::type, jsonstr::traceCls},
        {jsonstr::pktHeaderFc, {
            {jsonstr::type, jsonstr::structure},
            {jsonstr::minAlign, 8},
            {jsonstr::memberClasses, {
                jsonFixedLenUIntMember("magic", 32, jsonstr::pktMagicNumber, 16),
                {
                    {jsonstr::name, "uuid"},
                    {jsonstr::fc, {
                        {jsonstr::type, jsonstr::staticLenBlob},
                        {jsonstr::len, 16},
                        {jsonstr::roles, {jsonstr::metadataStreamUuid}},
                    }},
                },
                jsonFixedLenUIntMember("stream_id", 64, jsonstr::dataStreamClsId),
                jsonFixedLenUIntMember("stream_instance_id", 64, jsonstr::dataStreamId),
            }},
        }},
    };
    /* clang-format on */

    /* Stuff from IR */
    {
        const auto irTrace = bt2::wrap(fsTrace.ir_trace);

        tryAddIdentityProps(jsonTraceCls, irTrace);
        tryAddAttrsProp(jsonTraceCls, irTrace.cls());

        /* Environment */
        if (irTrace.environmentSize() > 0) {
            auto jsonEnv = nljson::object();

            for (std::uint64_t i = 0; i < irTrace.environmentSize(); ++i) {
                if (const auto envEntry = irTrace.environmentEntry(i);
                    envEntry.value.isUnsignedInteger()) {
                    jsonEnv[*envEntry.name] = envEntry.value.asUnsignedInteger().value();
                } else if (envEntry.value.isSignedInteger()) {
                    jsonEnv[*envEntry.name] = envEntry.value.asSignedInteger().value();
                } else {
                    BT_ASSERT(envEntry.value.isString());
                    jsonEnv[*envEntry.name] = *envEntry.value.asString().value();
                }
            }

            jsonTraceCls[jsonstr::env] = std::move(jsonEnv);
        }
    }

    return jsonTraceCls;
}

void appendFragment(const nljson& jsonFrag, GString& metadata)
{
    g_string_append_c(&metadata, '\x1e');

    const auto json = jsonFrag.dump();

    g_string_append(&metadata, json.c_str());
}

} /* namespace */

void translate_trace_ctf_ir_to_json(fs_sink_ctf_trace * const trace, GString * const metadataStream)
{
    /* Preamble fragment */
    /* clang-format off */
    appendFragment({
        {jsonstr::type, jsonstr::preamble},
        {jsonstr::version, 2},
        {jsonstr::uuid, std::invoke([trace] {
            auto json = nljson::array();

            for (const auto byte : bt2c::UuidView {trace->uuid}) {
                json.push_back(byte);
            }

            return json;
        })},
        {jsonstr::attrs, {
            {jsonstr::btNs, {
                {"sink.ctf.fs", true},
            }},
        }},
    }, *metadataStream);
    /* clang-format on */

    /* Trace class */
    appendFragment(jsonTraceClsFromFs(*trace), *metadataStream);

    /* Clock classes */
    for (auto clkClsIdx = 0U; clkClsIdx < trace->clock_classes->len; ++clkClsIdx) {
        auto& fsClkCls =
            *static_cast<const fs_sink_ctf_clock_class *>(trace->clock_classes->pdata[clkClsIdx]);

        appendFragment(jsonClkClsFromIr(bt2::wrap(fsClkCls.ir_cc), fsClkCls.name->str),
                       *metadataStream);
    }

    /* Data stream classes and event record classes */
    for (auto streamClsIdx = 0U; streamClsIdx < trace->stream_classes->len; ++streamClsIdx) {
        auto& fsStreamCls = *static_cast<const fs_sink_ctf_stream_class *>(
            trace->stream_classes->pdata[streamClsIdx]);

        /* Data stream class */
        appendFragment(jsonDataStreamClsFromFs(fsStreamCls), *metadataStream);

        /* Event record classes */
        for (auto eventClsIdx = 0U; eventClsIdx < fsStreamCls.event_classes->len; ++eventClsIdx) {
            appendFragment(jsonEventRecordClsFromFs(*static_cast<fs_sink_ctf_event_class *>(
                               fsStreamCls.event_classes->pdata[eventClsIdx])),
                           *metadataStream);
        }
    }
}
