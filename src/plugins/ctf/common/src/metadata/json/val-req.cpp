/*
 * Copyright (c) 2022-2024 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <string>
#include <unordered_set>

#include "common/assert.h"
#include "cpp-common/bt2c/contains.hpp"
#include "cpp-common/bt2c/exc.hpp"
#include "cpp-common/bt2c/json-val-req.hpp"
#include "cpp-common/bt2c/logging.hpp"

#include "../../../metadata/json-strings.hpp"
#include "../ctf-ir.hpp"
#include "val-req.hpp"

namespace ctf::src {
namespace {

/*
 * CTF 2 JSON alignment value requirement.
 */
class AlignValReq final : public bt2c::JsonValHasTypeReq
{
public:
    explicit AlignValReq(const bt2c::Logger& parentLogger) noexcept
        : bt2c::JsonValHasTypeReq {bt2c::ValType::UInt, parentLogger}
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<AlignValReq>(parentLogger);
    }

protected:
    static bool _isPowOfTwo(const unsigned long long val) noexcept
    {
        return ((val & (val - 1)) == 0) && val > 0;
    }

    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        const auto val = *jsonVal.asUInt();

        if (!this->_isPowOfTwo(val)) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_THROW_SPEC(
                this->_logger(), bt2c::Error, jsonVal.loc(), "{} is not a power of two.", val);
        }

        if (val > maxAlign) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_THROW_SPEC(
                this->_logger(), bt2c::Error, jsonVal.loc(),
                "{} is greater than the maximum supported alignment ({}).", val, maxAlign);
        }
    }
};

/*
 * CTF 2 JSON byte order value requirement.
 */
class ByteOrderValReq final : public bt2c::JsonStrValInSetReq
{
public:
    explicit ByteOrderValReq(const bt2c::Logger& parentLogger)
        : bt2c::JsonStrValInSetReq {
              bt2c::JsonStrValInSetReq::Set {jsonstr::bigEndian, jsonstr::littleEndian},
              parentLogger}
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<ByteOrderValReq>(parentLogger);
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            bt2c::JsonStrValInSetReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(this->_logger(), jsonVal.loc(),
                                                              "Invalid byte order.");
        }
    }
};

/*
 * CTF 2 JSON bit order value requirement.
 */
class BitOrderValReq final : public bt2c::JsonStrValInSetReq
{
public:
    explicit BitOrderValReq(const bt2c::Logger& parentLogger)
        : bt2c::JsonStrValInSetReq {bt2c::JsonStrValInSetReq::Set {jsonstr::ftl, jsonstr::ltf},
                                    parentLogger}
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<BitOrderValReq>(parentLogger);
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            bt2c::JsonStrValInSetReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(this->_logger(), jsonVal.loc(),
                                                              "Invalid bit order.");
        }
    }
};

/*
 * CTF 2 JSON UUID value requirement.
 */
class UuidValReq final : public bt2c::JsonArrayValReq
{
public:
    explicit UuidValReq(const bt2c::Logger& parentLogger)
        : bt2c::JsonArrayValReq {16, bt2c::JsonUIntValInRangeReq::shared(0, 255, parentLogger),
                                 parentLogger}
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<UuidValReq>(parentLogger);
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            bt2c::JsonArrayValReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(this->_logger(), jsonVal.loc(),
                                                              "Invalid UUID.");
        }
    }
};

/*
 * CTF 2 JSON field location path element value requirement.
 */
class FieldLocPathElemValReq final : public bt2c::JsonValReq
{
public:
    explicit FieldLocPathElemValReq(const bt2c::Logger& parentLogger)
        : bt2c::JsonValReq {parentLogger}
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<FieldLocPathElemValReq>(parentLogger);
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        if (jsonVal.isNull() || jsonVal.isStr()) {
            /* Valid */
            return;
        }

        BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_THROW_SPEC(this->_logger(), bt2c::Error, jsonVal.loc(),
                                                        "Expecting a string or `null`.");
    }
};

/*
 * CTF 2 JSON field location value requirement.
 */
class FieldLocValReq final : public bt2c::JsonObjValReq
{
public:
    explicit FieldLocValReq(const bt2c::Logger& parentLogger)
        : /* clang-format off */
        bt2c::JsonObjValReq {{
            {jsonstr::origin, {
                bt2c::JsonStrValInSetReq::shared({
                    jsonstr::pktHeader,
                    jsonstr::pktCtx,
                    jsonstr::eventRecordHeader,
                    jsonstr::eventRecordCommonCtx,
                    jsonstr::eventRecordSpecCtx,
                    jsonstr::eventRecordPayload,
                }, parentLogger)
            }},
            {jsonstr::path, {
                bt2c::JsonArrayValReq::shared(1, std::nullopt,
                                              FieldLocPathElemValReq::shared(parentLogger), parentLogger),
                true
            }},
        }, parentLogger}
    /* clang-format on */
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<FieldLocValReq>(parentLogger);
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            bt2c::JsonObjValReq::_validate(jsonVal);

            /* Validate that the last path element is not `null` */
            {
                const auto& jsonLastPathElem =
                    **(jsonVal.asObj().val<bt2c::JsonArrayVal>(jsonstr::path).end() - 1);

                if (jsonLastPathElem.isNull()) {
                    BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_THROW_SPEC(this->_logger(), bt2c::Error,
                                                                    jsonLastPathElem.loc(),
                                                                    "Path ends with `null`.");
                }
            }
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(this->_logger(), jsonVal.loc(),
                                                              "Invalid field location.");
        }
    }
};

/*
 * CTF 2 JSON user attributes value requirement.
 */
class UserAttrsValReq final : public bt2c::JsonObjValReq
{
public:
    explicit UserAttrsValReq(const bt2c::Logger& parentLogger)
        : bt2c::JsonObjValReq {{}, true, parentLogger}
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<UserAttrsValReq>(parentLogger);
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            bt2c::JsonObjValReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(this->_logger(), jsonVal.loc(),
                                                              "Invalid user attributes.");
        }
    }
};

/*
 * CTF 2 JSON trace environment value requirement.
 */
class TraceEnvValReq final : public bt2c::JsonObjValReq
{
public:
    explicit TraceEnvValReq(const bt2c::Logger& parentLogger)
        : bt2c::JsonObjValReq {{}, true, parentLogger}
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<TraceEnvValReq>(parentLogger);
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            bt2c::JsonObjValReq::_validate(jsonVal);

            /* Validate types of entries */
            for (auto& [key, jsonEntry] : jsonVal.asObj()) {
                if (!jsonEntry->isUInt() && !jsonEntry->isSInt() && !jsonEntry->isStr()) {
                    BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_THROW_SPEC(
                        this->_logger(), bt2c::Error, jsonEntry->loc(),
                        "Entry `{}`: expecting an integer or a string.", key);
                }
            }
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(this->_logger(), jsonVal.loc(),
                                                              "Invalid trace environment.");
        }
    }
};

/*
 * CTF 2 JSON extensions value requirement.
 */
class ExtValReq final : public bt2c::JsonObjValReq
{
public:
    explicit ExtValReq(const bt2c::Logger& parentLogger)
        : bt2c::JsonObjValReq {{}, true, parentLogger}
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<ExtValReq>(parentLogger);
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            bt2c::JsonObjValReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(this->_logger(), jsonVal.loc(),
                                                              "Invalid extensions.");
        }

        if (jsonVal.asObj().size() > 0) {
            /* Never valid */
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_THROW_SPEC(
                this->_logger(), bt2c::Error, jsonVal.loc(),
                "This version of the `ctf` plugin doesn't support any CTF 2 extension.");
        }
    }
};

/*
 * CTF 2 JSON roles value requirement.
 */
class RolesValReq final : public bt2c::JsonArrayValReq
{
public:
    /*
     * Builds a CTF 2 JSON roles value requirement: _validate()
     * validates that a given JSON array value only contains the roles
     * `validRoles`.
     */
    explicit RolesValReq(bt2c::JsonStrValInSetReq::Set validRoles, const bt2c::Logger& parentLogger)
        : bt2c::JsonArrayValReq {
              bt2c::JsonStrValInSetReq::shared(std::move(validRoles), parentLogger), parentLogger}
    {
    }

    static SP shared(bt2c::JsonStrValInSetReq::Set validRoles, const bt2c::Logger& parentLogger)
    {
        return std::make_shared<RolesValReq>(std::move(validRoles), parentLogger);
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            bt2c::JsonArrayValReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(this->_logger(), jsonVal.loc(),
                                                              "Invalid roles.");
        }
    }
};

/*
 * Adds a JSON object value property requirement having the key `key` to
 * `propReqs`, passing `valReq` and `isRequired` to its constructor.
 */
void addToPropReqs(bt2c::JsonObjValReq::PropReqs& propReqs, std::string&& key,
                   bt2c::JsonValReq::SP valReq, const bool isRequired = false)
{
    propReqs.emplace(
        std::pair {std::move(key), bt2c::JsonObjValPropReq {std::move(valReq), isRequired}});
}

/*
 * Returns the pair (suitable for insertion into a
 * `bt2c::JsonObjValReq::PropReqs` instance) for the CTF 2 JSON type
 * object property requirement.
 */
bt2c::JsonObjValReq::PropReqsEntry objTypePropReqEntry(std::string&& type,
                                                       const bt2c::Logger& parentLogger)
{
    return {jsonstr::type, {bt2c::JsonStrValInSetReq::shared(std::move(type), parentLogger), true}};
}

/*
 * Returns the pair (suitable for insertion into a
 * `bt2c::JsonObjValReq::PropReqs` instance) for the CTF 2 attributes
 * object property requirement.
 */
bt2c::JsonObjValReq::PropReqsEntry attrsPropReqEntry(const bt2c::Logger& parentLogger)
{
    return {jsonstr::attrs, {UserAttrsValReq::shared(parentLogger)}};
}

/*
 * Returns the pair (suitable for insertion into a
 * `bt2c::JsonObjValReq::PropReqs` instance) for the CTF 2 extensions object
 * property requirement.
 */
bt2c::JsonObjValReq::PropReqsEntry extPropReqEntry(const bt2c::Logger& parentLogger)
{
    return {jsonstr::extensions, {ExtValReq::shared(parentLogger)}};
}

/*
 * CTF 2 JSON field class value abstract requirement.
 *
 * All derived classes are required to implement a static typeStr()
 * method which returns the type string of the field class.
 */
class FcValReq : public bt2c::JsonObjValReq
{
protected:
    /*
     * Builds a CTF 2 JSON field class value requirement of type `type`,
     * adding `propReqs` to the base JSON object value property
     * requirements.
     */
    explicit FcValReq(std::string&& type, PropReqs&& propReqs, const bt2c::Logger& parentLogger)
        : bt2c::JsonObjValReq {
              this->_buildPropReqs(std::move(type), std::move(propReqs), parentLogger),
              parentLogger}
    {
    }

    /*
     * Builds a CTF 2 JSON field class value requirement of type `type`.
     */
    explicit FcValReq(std::string&& type, const bt2c::Logger& parentLogger)
        : FcValReq {std::move(type), {}, parentLogger}
    {
    }

private:
    static PropReqs _buildPropReqs(std::string&& type, PropReqs&& propReqs,
                                   const bt2c::Logger& parentLogger)
    {
        propReqs.insert(objTypePropReqEntry(std::move(type), parentLogger));
        propReqs.insert(attrsPropReqEntry(parentLogger));
        propReqs.insert(extPropReqEntry(parentLogger));
        return std::move(propReqs);
    }
};

/*
 * CTF 2 JSON fixed-length bit array field class value requirement.
 */
class FixedLenBitArrayFcValReq : public FcValReq
{
protected:
    /*
     * Builds a CTF 2 JSON fixed-length bit array field class value
     * requirement of type `type`, adding `propReqs` to the base JSON
     * object value property requirements.
     */
    explicit FixedLenBitArrayFcValReq(std::string&& type, PropReqs&& propReqs,
                                      const bt2c::Logger& parentLogger)
        : FcValReq {std::move(type), this->_buildPropReqs(std::move(propReqs), parentLogger),
                    parentLogger}
    {
    }

    /*
     * Builds a CTF 2 JSON fixed-length bit array field class value
     * requirement of type `type`.
     */
    explicit FixedLenBitArrayFcValReq(std::string&& type, const bt2c::Logger& parentLogger)
        : FixedLenBitArrayFcValReq {std::move(type), {}, parentLogger}
    {
    }

public:
    explicit FixedLenBitArrayFcValReq(const bt2c::Logger& parentLogger)
        : FixedLenBitArrayFcValReq {this->typeStr(), parentLogger}
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<FixedLenBitArrayFcValReq>(parentLogger);
    }

    static const char *typeStr() noexcept
    {
        return jsonstr::fixedLenBitArray;
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            FcValReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(
                this->_logger(), jsonVal.loc(), "Invalid fixed-length bit array field class.");
        }
    }

    static PropReqs _buildPropReqs(PropReqs&& propReqs, const bt2c::Logger& parentLogger)
    {
        addToPropReqs(propReqs, jsonstr::len,
                      bt2c::JsonUIntValInRangeReq::shared(1, 64, parentLogger), true);
        addToPropReqs(propReqs, jsonstr::byteOrder, ByteOrderValReq::shared(parentLogger), true);
        addToPropReqs(propReqs, jsonstr::bitOrder, BitOrderValReq::shared(parentLogger));
        addToPropReqs(propReqs, jsonstr::align, AlignValReq::shared(parentLogger));
        return std::move(propReqs);
    }
};

/*
 * CTF 2 JSON fixed-length bit map field class flags value requirement.
 *
 * An instance of this class validates that a given JSON value is a CTF
 * 2 fixed-length bit map field class flags object.
 */
class FixedLenBitMapFcFlagsValReq final : public bt2c::JsonObjValReq
{
public:
    explicit FixedLenBitMapFcFlagsValReq(const bt2c::Logger& parentLogger)
        : bt2c::JsonObjValReq {{}, true, parentLogger},
          _mRangeSetReq {parentLogger}
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<FixedLenBitMapFcFlagsValReq>(parentLogger);
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            bt2c::JsonObjValReq::_validate(jsonVal);

            /* Require at least one flag */
            if (jsonVal.asObj().size() < 1) {
                BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_THROW_SPEC(
                    this->_logger(), bt2c::Error, jsonVal.loc(), "Expecting at least one flag.");
            }

            /* Validate range sets */
            for (auto& [key, jsonEntry] : jsonVal.asObj()) {
                try {
                    _mRangeSetReq.validate(*jsonEntry);
                } catch (const bt2c::Error&) {
                    BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(
                        this->_logger(), jsonVal.loc(), "Invalid flag `{}`.", key);
                }
            }
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(
                this->_logger(), jsonVal.loc(), "Invalid enumeration field class mappings.");
        }
    }

    Ctf2JsonIntRangeSetValReqBase<bt2c::JsonUIntValReq> _mRangeSetReq;
};

/*
 * CTF 2 JSON fixed-length bit map field class value requirement.
 */
class FixedLenBitMapFcValReq final : public FixedLenBitArrayFcValReq
{
public:
    explicit FixedLenBitMapFcValReq(const bt2c::Logger& parentLogger)
        : /* clang-format off */
        FixedLenBitArrayFcValReq {this->typeStr(), {
            {jsonstr::flags, {FixedLenBitMapFcFlagsValReq::shared(parentLogger), true}},
        }, parentLogger}
    /* clang-format on */
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<FixedLenBitMapFcValReq>(parentLogger);
    }

    static const char *typeStr() noexcept
    {
        return jsonstr::fixedLenBitMap;
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            FcValReq::_validate(jsonVal);

            /*
             * Validate that the upper value of each flag bit range is
             * less than the length of instances.
             */
            const auto len = jsonVal.asObj().rawUIntVal(jsonstr::len);

            for (auto& [key, jsonRanges] : jsonVal.asObj()[jsonstr::flags]->asObj()) {
                for (auto& jsonRange : jsonRanges->asArray()) {
                    if (auto& jsonRangeUpper = jsonRange->asArray()[1].asUInt();
                        *jsonRangeUpper >= len) {
                        BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_THROW_SPEC(
                            this->_logger(), bt2c::Error, jsonRangeUpper.loc(),
                            "Flag `{}`: bit index {} is greater than or equal to "
                            "the value of the `{}` property ({} bits).",
                            key, *jsonRangeUpper, jsonstr::len, len);
                    }
                }
            }
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(
                this->_logger(), jsonVal.loc(), "Invalid fixed-length bit map field class.");
        }
    }
};

/*
 * CTF 2 JSON fixed-length boolean field class value requirement.
 */
class FixedLenBoolFcValReq final : public FixedLenBitArrayFcValReq
{
public:
    explicit FixedLenBoolFcValReq(const bt2c::Logger& parentLogger)
        : FixedLenBitArrayFcValReq {this->typeStr(), parentLogger}
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<FixedLenBoolFcValReq>(parentLogger);
    }

    static const char *typeStr() noexcept
    {
        return jsonstr::fixedLenBool;
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            FcValReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(
                this->_logger(), jsonVal.loc(), "Invalid fixed-length boolean field class.");
        }
    }
};

/*
 * Returns the pair (suitable for insertion into a
 * `bt2c::JsonObjValReq::PropReqs` instance) for the CTF 2 integer field
 * class preferred display base object property requirement.
 */
bt2c::JsonObjValReq::PropReqsEntry intFcPrefDispBasePropReqEntry(const bt2c::Logger& parentLogger)
{
    return {jsonstr::prefDispBase,
            {bt2c::JsonUIntValInSetReq::shared({2, 8, 10, 16}, parentLogger)}};
}

/*
 * CTF 2 JSON fixed-length integer field class value abstract
 * requirement.
 */
class FixedLenIntFcValReq : public FixedLenBitArrayFcValReq
{
protected:
    /*
     * Builds a CTF 2 JSON fixed-length integer field class value
     * requirement of type `type`, adding `propReqs` to the base JSON
     * object value property requirements.
     */
    explicit FixedLenIntFcValReq(std::string&& type, PropReqs&& propReqs,
                                 const bt2c::Logger& parentLogger)
        : FixedLenBitArrayFcValReq {std::move(type),
                                    this->_buildPropReqs(std::move(propReqs), parentLogger),
                                    parentLogger}
    {
    }

private:
    static PropReqs _buildPropReqs(PropReqs&& propReqs, const bt2c::Logger& parentLogger)
    {
        propReqs.insert(intFcPrefDispBasePropReqEntry(parentLogger));
        return std::move(propReqs);
    }
};

/*
 * Returns the pair (suitable for insertion into a
 * `bt2c::JsonObjValReq::PropReqs` instance) for the CTF 2 unsigned
 * integer field class roles object property requirement.
 */
bt2c::JsonObjValReq::PropReqsEntry uIntFcRolesPropReqEntry(const bt2c::Logger& parentLogger)
{
    /* clang-format off */
    return {jsonstr::roles, {
        RolesValReq::shared({
            jsonstr::dataStreamClsId,
            jsonstr::dataStreamId,
            jsonstr::defClkTs,
            jsonstr::discEventRecordCounterSnap,
            jsonstr::eventRecordClsId,
            jsonstr::pktContentLen,
            jsonstr::pktEndDefClkTs,
            jsonstr::pktMagicNumber,
            jsonstr::pktSeqNum,
            jsonstr::pktTotalLen,
        }, parentLogger)
    }};
    /* clang-format on */
}

/*
 * CTF 2 JSON integer field class mappings value requirement.
 *
 * An instance of this class validates that a given JSON value is
 * a CTF 2 integer field class mappings object, each integer value
 * within the integer ranges satisfying an instance of `JsonIntValReqT`.
 */
template <typename JsonIntValReqT>
class IntFcMappingsValReq final : public bt2c::JsonObjValReq
{
public:
    explicit IntFcMappingsValReq(const bt2c::Logger& parentLogger)
        : bt2c::JsonObjValReq {{}, true, parentLogger},
          _mRangeSetReq {parentLogger}
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<IntFcMappingsValReq>(parentLogger);
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            bt2c::JsonObjValReq::_validate(jsonVal);

            /* Validate range sets */
            for (auto& [key, jsonEntry] : jsonVal.asObj()) {
                try {
                    _mRangeSetReq.validate(*jsonEntry);
                } catch (const bt2c::Error&) {
                    BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(
                        this->_logger(), jsonVal.loc(), "Invalid mapping `{}`.", key);
                }
            }
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(
                this->_logger(), jsonVal.loc(), "Invalid enumeration field class mappings.");
        }
    }

    Ctf2JsonIntRangeSetValReqBase<JsonIntValReqT> _mRangeSetReq;
};

/*
 * Returns the pair (suitable for insertion into a
 * `bt2c::JsonObjValReq::PropReqs` instance) for the CTF 2 integer field
 * class mappings object property requirement.
 */
template <typename JsonIntValReqT>
bt2c::JsonObjValReq::PropReqsEntry intFcMappingsPropReqEntry(const bt2c::Logger& parentLogger)
{
    return {jsonstr::mappings, {IntFcMappingsValReq<JsonIntValReqT>::shared(parentLogger)}};
}

/*
 * CTF 2 JSON fixed-length unsigned integer field class value
 * requirement.
 */
class FixedLenUIntFcValReq final : public FixedLenIntFcValReq
{
public:
    /*
     * Builds a CTF 2 JSON fixed-length unsigned integer field class
     * value requirement.
     */
    explicit FixedLenUIntFcValReq(const bt2c::Logger& parentLogger)
        : FixedLenIntFcValReq {this->typeStr(), this->_buildPropReqs(parentLogger), parentLogger}
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<FixedLenUIntFcValReq>(parentLogger);
    }

    static const char *typeStr() noexcept
    {
        return jsonstr::fixedLenUInt;
    }

private:
    static PropReqs _buildPropReqs(const bt2c::Logger& parentLogger)
    {
        PropReqs propReqs;

        propReqs.insert(intFcMappingsPropReqEntry<bt2c::JsonUIntValReq>(parentLogger));
        propReqs.insert(uIntFcRolesPropReqEntry(parentLogger));
        return propReqs;
    }

    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            FcValReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(
                this->_logger(), jsonVal.loc(),
                "Invalid fixed-length unsigned integer field class.");
        }
    }
};

/*
 * CTF 2 JSON fixed-length signed integer field class value requirement.
 */
class FixedLenSIntFcValReq final : public FixedLenIntFcValReq
{
public:
    /*
     * Builds a CTF 2 JSON fixed-length signed integer field class
     * value requirement.
     */
    explicit FixedLenSIntFcValReq(const bt2c::Logger& parentLogger)
        : FixedLenIntFcValReq {this->typeStr(),
                               {
                                   intFcMappingsPropReqEntry<bt2c::JsonSIntValReq>(parentLogger),
                               },
                               parentLogger}
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<FixedLenSIntFcValReq>(parentLogger);
    }

    static const char *typeStr() noexcept
    {
        return jsonstr::fixedLenSInt;
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            FcValReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(
                this->_logger(), jsonVal.loc(), "Invalid fixed-length signed integer field class.");
        }
    }
};

/*
 * CTF 2 JSON fixed-length floating-point number field class value
 * requirement.
 */
class FixedLenFloatFcValReq final : public FixedLenBitArrayFcValReq
{
public:
    explicit FixedLenFloatFcValReq(const bt2c::Logger& parentLogger)
        : FixedLenBitArrayFcValReq {this->typeStr(), parentLogger}
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<FixedLenFloatFcValReq>(parentLogger);
    }

    static const char *typeStr() noexcept
    {
        return jsonstr::fixedLenFloat;
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            FcValReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(
                this->_logger(), jsonVal.loc(),
                "Invalid fixed-length floating-point number field class.");
        }
    }
};

/*
 * CTF 2 JSON variable-length integer field class value abstract
 * requirement.
 */
class VarLenIntFcValReq : public FcValReq
{
protected:
    /*
     * Builds a CTF 2 JSON variable-length integer field class value
     * requirement of type `type`, adding `propReqs` to the base JSON
     * object value property requirements.
     */
    explicit VarLenIntFcValReq(std::string&& type, PropReqs&& propReqs,
                               const bt2c::Logger& parentLogger)
        : FcValReq {std::move(type), this->_buildPropReqs(std::move(propReqs), parentLogger),
                    parentLogger}
    {
    }

private:
    static PropReqs _buildPropReqs(PropReqs&& propReqs, const bt2c::Logger& parentLogger)
    {
        propReqs.insert(intFcPrefDispBasePropReqEntry(parentLogger));
        return std::move(propReqs);
    }
};

/*
 * CTF 2 JSON variable-length unsigned integer field class value
 * requirement.
 */
class VarLenUIntFcValReq : public VarLenIntFcValReq
{
public:
    /*
     * Builds a CTF 2 JSON variable-length unsigned integer field class
     * value requirement.
     */
    explicit VarLenUIntFcValReq(const bt2c::Logger& parentLogger)
        : VarLenIntFcValReq {this->typeStr(), this->_buildPropReqs(parentLogger), parentLogger}
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<VarLenUIntFcValReq>(parentLogger);
    }

    static const char *typeStr() noexcept
    {
        return jsonstr::varLenUInt;
    }

private:
    static PropReqs _buildPropReqs(const bt2c::Logger& parentLogger)
    {
        PropReqs propReqs;

        propReqs.insert(intFcMappingsPropReqEntry<bt2c::JsonUIntValReq>(parentLogger));
        propReqs.insert(uIntFcRolesPropReqEntry(parentLogger));
        return propReqs;
    }

    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            FcValReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(
                this->_logger(), jsonVal.loc(),
                "Invalid variable-length unsigned integer field class.");
        }
    }
};

/*
 * CTF 2 JSON variable-length signed integer field class
 * value requirement.
 */
class VarLenSIntFcValReq : public VarLenIntFcValReq
{
public:
    /*
     * Builds a CTF 2 JSON variable-length unsigned integer field class
     * value requirement.
     */
    explicit VarLenSIntFcValReq(const bt2c::Logger& parentLogger)
        : VarLenIntFcValReq {this->typeStr(),
                             {
                                 intFcMappingsPropReqEntry<bt2c::JsonSIntValReq>(parentLogger),
                             },
                             parentLogger}
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<VarLenSIntFcValReq>(parentLogger);
    }

    static const char *typeStr() noexcept
    {
        return jsonstr::varLenSInt;
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            FcValReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(
                this->_logger(), jsonVal.loc(),
                "Invalid variable-length signed integer field class.");
        }
    }
};

/*
 * CTF 2 JSON string encoding value requirement.
 */
class StrEncodingValReq final : public bt2c::JsonStrValInSetReq
{
public:
    explicit StrEncodingValReq(const bt2c::Logger& parentLogger)
        : bt2c::JsonStrValInSetReq {bt2c::JsonStrValInSetReq::Set {
                                        jsonstr::utf8,
                                        jsonstr::utf16Be,
                                        jsonstr::utf16Le,
                                        jsonstr::utf32Be,
                                        jsonstr::utf32Le,
                                    },
                                    parentLogger}
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<StrEncodingValReq>(parentLogger);
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            bt2c::JsonStrValInSetReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(this->_logger(), jsonVal.loc(),
                                                              "Invalid string encoding.");
        }
    }
};

/*
 * CTF 2 JSON string field class value requirement.
 */
class StrFcValReq : public FcValReq
{
protected:
    explicit StrFcValReq(std::string&& type, PropReqs&& propReqs, const bt2c::Logger& parentLogger)
        : FcValReq {std::move(type), this->_buildPropReqs(std::move(propReqs), parentLogger),
                    parentLogger}
    {
    }

    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            FcValReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(this->_logger(), jsonVal.loc(),
                                                              "Invalid string field class.");
        }
    }

private:
    static PropReqs _buildPropReqs(PropReqs&& propReqs, const bt2c::Logger& parentLogger)
    {
        addToPropReqs(propReqs, jsonstr::encoding, StrEncodingValReq::shared(parentLogger));
        return std::move(propReqs);
    }
};

/*
 * CTF 2 JSON null-terminated string field class value requirement.
 */
class NullTerminatedStrFcValReq final : public StrFcValReq
{
public:
    explicit NullTerminatedStrFcValReq(const bt2c::Logger& parentLogger)
        : StrFcValReq {this->typeStr(), {}, parentLogger}
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<NullTerminatedStrFcValReq>(parentLogger);
    }

    static const char *typeStr() noexcept
    {
        return jsonstr::nullTerminatedStr;
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            FcValReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(
                this->_logger(), jsonVal.loc(), "Invalid null-terminated string field class.");
        }
    }
};

/*
 * Returns the pair (suitable for insertion into a
 * `bt2c::JsonObjValReq::PropReqs` instance) for the CTF 2 static-length
 * field class length object property requirement.
 */
bt2c::JsonObjValReq::PropReqsEntry staticLenFcLenPropReqEntry(const bt2c::Logger& parentLogger)
{
    return {jsonstr::len,
            {bt2c::JsonValHasTypeReq::shared(bt2c::ValType::UInt, parentLogger), true}};
}

/*
 * Returns the pair (suitable for insertion into a
 * `bt2c::JsonObjValReq::PropReqs` instance) for the CTF 2
 * dynamic-length field class length field location object
 * property requirement.
 */
bt2c::JsonObjValReq::PropReqsEntry dynLenFcLenFieldLocPropReqEntry(const bt2c::Logger& parentLogger)
{
    return {jsonstr::lenFieldLoc, {FieldLocValReq::shared(parentLogger), true}};
}

/*
 * CTF 2 JSON static-length string field class value requirement.
 */
class StaticLenStrFcValReq final : public StrFcValReq
{
public:
    explicit StaticLenStrFcValReq(const bt2c::Logger& parentLogger)
        : StrFcValReq {this->typeStr(), {staticLenFcLenPropReqEntry(parentLogger)}, parentLogger}
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<StaticLenStrFcValReq>(parentLogger);
    }

    static const char *typeStr() noexcept
    {
        return jsonstr::staticLenStr;
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            StrFcValReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(
                this->_logger(), jsonVal.loc(), "Invalid static-length string field class.");
        }
    }
};

/*
 * CTF 2 JSON dynamic-length string field class value requirement.
 */
class DynLenStrFcValReq final : public StrFcValReq
{
public:
    explicit DynLenStrFcValReq(const bt2c::Logger& parentLogger)
        : StrFcValReq {this->typeStr(),
                       {dynLenFcLenFieldLocPropReqEntry(parentLogger)},
                       parentLogger}
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<DynLenStrFcValReq>(parentLogger);
    }

    static const char *typeStr() noexcept
    {
        return jsonstr::dynLenStr;
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            StrFcValReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(
                this->_logger(), jsonVal.loc(), "Invalid dynamic-length string field class.");
        }
    }
};

/*
 * CTF 2 JSON BLOB field class value abstract requirement.
 */
class BlobFcValReq : public FcValReq
{
protected:
    /*
     * Builds a CTF 2 JSON BLOB field class value requirement of type
     * `type`, adding `propReqs` to the base JSON object value property
     * requirements.
     */
    explicit BlobFcValReq(std::string&& type, PropReqs&& propReqs, const bt2c::Logger& parentLogger)
        : FcValReq {std::move(type), this->_buildPropReqs(std::move(propReqs), parentLogger),
                    parentLogger}
    {
    }

    /*
     * Builds a CTF 2 JSON BLOB field class value requirement of type
     * `type`.
     */
    explicit BlobFcValReq(std::string&& type, const bt2c::Logger& parentLogger)
        : BlobFcValReq {std::move(type), {}, parentLogger}
    {
    }

private:
    static PropReqs _buildPropReqs(PropReqs&& propReqs, const bt2c::Logger& parentLogger)
    {
        addToPropReqs(propReqs, jsonstr::mediaType,
                      bt2c::JsonValHasTypeReq::shared(bt2c::ValType::Str, parentLogger));
        return std::move(propReqs);
    }
};

/*
 * CTF 2 JSON static-length BLOB field class value requirement.
 */
class StaticLenBlobFcValReq final : public BlobFcValReq
{
public:
    explicit StaticLenBlobFcValReq(const bt2c::Logger& parentLogger)
        : BlobFcValReq {this->typeStr(), this->_buildPropReqs(parentLogger), parentLogger}
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<StaticLenBlobFcValReq>(parentLogger);
    }

    static const char *typeStr() noexcept
    {
        return jsonstr::staticLenBlob;
    }

private:
    static PropReqs _buildPropReqs(const bt2c::Logger& parentLogger)
    {
        PropReqs propReqs;

        propReqs.insert(staticLenFcLenPropReqEntry(parentLogger));
        propReqs.insert(
            {jsonstr::roles, RolesValReq::shared({jsonstr::metadataStreamUuid}, parentLogger)});
        return propReqs;
    }

    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            FcValReq::_validate(jsonVal);

            if (const auto jsonRoles = jsonVal.asObj()[jsonstr::roles];
                jsonRoles && !jsonRoles->asArray().isEmpty()) {
                /* The only valid role is the metadata stream UUID */
                if (auto& jsonLen = jsonVal.asObj()[jsonstr::len]->asUInt(); *jsonLen != 16) {
                    BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_THROW_SPEC(
                        this->_logger(), bt2c::Error, jsonLen.loc(),
                        "`{}` property: expecting 16, not {}, because the field class has the `{}` role.",
                        jsonstr::len, *jsonLen, jsonstr::metadataStreamUuid);
                }
            }
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(
                this->_logger(), jsonVal.loc(), "Invalid static-length BLOB field class.");
        }
    }
};

/*
 * CTF 2 JSON dynamic-length BLOB field class value requirement.
 */
class DynLenBlobFcValReq final : public BlobFcValReq
{
public:
    explicit DynLenBlobFcValReq(const bt2c::Logger& parentLogger)
        : BlobFcValReq {this->typeStr(),
                        {dynLenFcLenFieldLocPropReqEntry(parentLogger)},
                        parentLogger}
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<DynLenBlobFcValReq>(parentLogger);
    }

    static const char *typeStr() noexcept
    {
        return jsonstr::dynLenBlob;
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            FcValReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(
                this->_logger(), jsonVal.loc(), "Invalid dynamic-length BLOB field class.");
        }
    }
};

class AnyFullBlownFcValReq;

/*
 * CTF 2 field classes are recursive, in that some field classes may
 * contain other field classes.
 *
 * To make it possible to build a `AnyFullBlownFcValReq` instance
 * without a shared pointer, the constructor of compound field class
 * requirements accepts a `const AnyFullBlownFcValReq&` (raw reference)
 * parameter. The raw reference must therefore remain valid as long as
 * the compound field class using it exists.
 *
 * Because JSON value requirements work with shared pointers to
 * `const Ctf2JsonValReq` (`bt2c::JsonValReq::SP`), this
 * `AnyFcValReqWrapper` class simply wraps such a
 * `const AnyFullBlownFcValReq *` value: its _validate() method forwards
 * the call. An `AnyFcValReqWrapper` instance doesn't own the
 * raw pointer.
 */
class AnyFcValReqWrapper final : public bt2c::JsonValReq
{
public:
    explicit AnyFcValReqWrapper(const AnyFullBlownFcValReq& anyFcValReq,
                                const bt2c::Logger& parentLogger)
        : bt2c::JsonValReq {parentLogger},
          _mAnyFullBlownFcValReq {&anyFcValReq}
    {
    }

    static SP shared(const AnyFullBlownFcValReq& anyFullBlownFcValReq,
                     const bt2c::Logger& parentLogger)
    {
        return std::make_shared<AnyFcValReqWrapper>(anyFullBlownFcValReq, parentLogger);
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override;

    const AnyFullBlownFcValReq *_mAnyFullBlownFcValReq;
};

/*
 * Returns the pair (suitable for insertion into a
 * `bt2c::JsonObjValReq::PropReqs` instance) for the CTF 2 field class
 * object property requirement having the key `key`.
 */
bt2c::JsonObjValReq::PropReqsEntry
anyFcPropReqEntry(std::string key, const AnyFullBlownFcValReq& anyFullBlownFcValReq,
                  const bool isRequired, const bt2c::Logger& parentLogger)
{
    return {std::move(key),
            {AnyFcValReqWrapper::shared(anyFullBlownFcValReq, parentLogger), isRequired}};
}

/*
 * Calls the other anyFcPropReqEntry() with `isRequired` set to `false`.
 */
bt2c::JsonObjValReq::PropReqsEntry
anyFcPropReqEntry(std::string key, const AnyFullBlownFcValReq& anyFullBlownFcValReq,
                  const bt2c::Logger& parentLogger)
{
    return anyFcPropReqEntry(std::move(key), anyFullBlownFcValReq, false, parentLogger);
}

/*
 * Returns the pair (suitable for insertion into a
 * `bt2c::JsonObjValReq::PropReqs` instance) for the CTF 2 object name
 * object property requirement.
 */
bt2c::JsonObjValReq::PropReqsEntry namePropReqEntry(const bool isRequired,
                                                    const bt2c::Logger& parentLogger)
{
    return {jsonstr::name,
            {bt2c::JsonValHasTypeReq::shared(bt2c::ValType::Str, parentLogger), isRequired}};
}

/*
 * CTF 2 JSON structure field member class value requirement.
 */
class StructFieldMemberClsValReq final : public bt2c::JsonObjValReq
{
public:
    explicit StructFieldMemberClsValReq(const AnyFullBlownFcValReq& anyFullBlownFcValReq,
                                        const bt2c::Logger& parentLogger)
        : /* clang-format off */
        bt2c::JsonObjValReq {{
            namePropReqEntry(true, parentLogger),
            anyFcPropReqEntry(jsonstr::fc, anyFullBlownFcValReq, parentLogger),
            attrsPropReqEntry(parentLogger),
            extPropReqEntry(parentLogger),
        }, parentLogger}
    /* clang-format on */
    {
    }

    static SP shared(const AnyFullBlownFcValReq& anyFullBlownFcValReq,
                     const bt2c::Logger& parentLogger)
    {
        return std::make_shared<StructFieldMemberClsValReq>(anyFullBlownFcValReq, parentLogger);
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            bt2c::JsonObjValReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(
                this->_logger(), jsonVal.loc(), "Invalid structure field member class.");
        }
    }
};

/*
 * Returns the pair (suitable for insertion into a
 * `bt2c::JsonObjValReq::PropReqs` instance) for the CTF 2 minimum
 * alignment object property requirement.
 */
bt2c::JsonObjValReq::PropReqsEntry minAlignPropReqEntry(const bt2c::Logger& parentLogger)
{
    return {jsonstr::minAlign, AlignValReq::shared(parentLogger)};
}

class UniqueEntryNamesValidator final
{
public:
    explicit UniqueEntryNamesValidator(const bt2c::Logger& parentLogger)
        : _mLogger {parentLogger}
    {
    }

    /*
     * Validates that, within the JSON array value having the key
     * `propName` within the JSON object value `jsonVal`, the `name`
     * property of each (JSON object value) element, if it exists,
     * is unique.
     *
     * Throws `TextParseError` on failure, using `elemName` to name the
     * element having a duplicate name.
     */
    void validate(const bt2c::JsonVal& jsonVal, const char * const propName,
                  const char * const elemName) const
    {
        const auto jsonEntries = jsonVal.asObj()[propName];

        if (!jsonEntries) {
            /* Empty */
            return;
        }

        /* Use a set to accumulate unique names */
        std::unordered_set<std::string> names;

        for (auto& jsonEntry : jsonEntries->asArray()) {
            const auto jsonName = jsonEntry->asObj()[jsonstr::name];

            if (!jsonName) {
                /* No `name` property */
                continue;
            }

            auto& jsonNameStr = jsonName->asStr();

            if (bt2c::contains(names, *jsonNameStr)) {
                /* Already in set */
                BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_THROW(bt2c::Error, jsonName->loc(),
                                                           "Duplicate {} name `{}`.", elemName,
                                                           (*jsonNameStr));
            }

            /* Add to set */
            names.insert(*jsonNameStr);
        }
    }

private:
    bt2c::Logger _mLogger;
};

/*
 * CTF 2 JSON structure field class value requirement.
 */
class StructFcValReq final : public FcValReq
{
public:
    explicit StructFcValReq(const AnyFullBlownFcValReq& anyFullBlownFcValReq,
                            const bt2c::Logger& parentLogger)
        : /* clang-format off */
        FcValReq {this->typeStr(), {
            {jsonstr::memberClasses, {
                bt2c::JsonArrayValReq::shared(
                    StructFieldMemberClsValReq::shared(anyFullBlownFcValReq, parentLogger),
                    parentLogger
                )
            }},
            minAlignPropReqEntry(parentLogger),
        }, parentLogger},
        _mUniqueEntryNamesValidator {parentLogger}
    /* clang-format on */
    {
    }

    static SP shared(const AnyFullBlownFcValReq& anyFullBlownFcValReq,
                     const bt2c::Logger& parentLogger)
    {
        return std::make_shared<StructFcValReq>(anyFullBlownFcValReq, parentLogger);
    }

    static const char *typeStr() noexcept
    {
        return jsonstr::structure;
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            FcValReq::_validate(jsonVal);

            /* Validate that member class names are unique */
            _mUniqueEntryNamesValidator.validate(jsonVal, jsonstr::memberClasses,
                                                 "structure field member class");
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(this->_logger(), jsonVal.loc(),
                                                              "Invalid structure field class.");
        }
    }

    UniqueEntryNamesValidator _mUniqueEntryNamesValidator;
};

/*
 * CTF 2 JSON array field class value abstract requirement.
 */
class ArrayFcValReq : public FcValReq
{
protected:
    /*
     * Builds a CTF 2 JSON array field class value requirement of type
     * `type`, adding `propReqs` to the base JSON object value property
     * requirements.
     */
    explicit ArrayFcValReq(std::string&& type, const AnyFullBlownFcValReq& anyFullBlownFcValReq,
                           PropReqs&& propReqs, const bt2c::Logger& parentLogger)
        : FcValReq {std::move(type),
                    this->_buildPropReqs(anyFullBlownFcValReq, std::move(propReqs), parentLogger),
                    parentLogger}
    {
    }

    /*
     * Builds a CTF 2 JSON array field class value requirement of type
     * `type`.
     */
    explicit ArrayFcValReq(std::string&& type, const AnyFullBlownFcValReq& anyFullBlownFcValReq,
                           const bt2c::Logger& parentLogger)
        : ArrayFcValReq {std::move(type), anyFullBlownFcValReq, {}, parentLogger}
    {
    }

private:
    static PropReqs _buildPropReqs(const AnyFullBlownFcValReq& anyFullBlownFcValReq,
                                   PropReqs&& propReqs, const bt2c::Logger& parentLogger)
    {
        propReqs.insert(anyFcPropReqEntry(jsonstr::elemFc, anyFullBlownFcValReq, parentLogger));
        propReqs.insert(minAlignPropReqEntry(parentLogger));
        return std::move(propReqs);
    }
};

/*
 * CTF 2 JSON static-length array field class value requirement.
 */
class StaticLenArrayFcValReq final : public ArrayFcValReq
{
public:
    explicit StaticLenArrayFcValReq(const AnyFullBlownFcValReq& anyFullBlownFcValReq,
                                    const bt2c::Logger& parentLogger)
        : /* clang-format off */
        ArrayFcValReq {this->typeStr(), anyFullBlownFcValReq, {
            staticLenFcLenPropReqEntry(parentLogger)
        }, parentLogger}
    /* clang-format on */
    {
    }

    static SP shared(const AnyFullBlownFcValReq& anyFullBlownFcValReq,
                     const bt2c::Logger& parentLogger)
    {
        return std::make_shared<StaticLenArrayFcValReq>(anyFullBlownFcValReq, parentLogger);
    }

    static const char *typeStr() noexcept
    {
        return jsonstr::staticLenArray;
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            FcValReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(
                this->_logger(), jsonVal.loc(), "Invalid static-length array field class.");
        }
    }
};

/*
 * CTF 2 JSON dynamic-length array field class value requirement.
 */
class DynLenArrayFcValReq final : public ArrayFcValReq
{
public:
    explicit DynLenArrayFcValReq(const AnyFullBlownFcValReq& anyFullBlownFcValReq,
                                 const bt2c::Logger& parentLogger)
        : /* clang-format off */
        ArrayFcValReq {this->typeStr(), anyFullBlownFcValReq, {
            dynLenFcLenFieldLocPropReqEntry(parentLogger)
        }, parentLogger}
    /* clang-format on */
    {
    }

    static SP shared(const AnyFullBlownFcValReq& anyFullBlownFcValReq,
                     const bt2c::Logger& parentLogger)
    {
        return std::make_shared<DynLenArrayFcValReq>(anyFullBlownFcValReq, parentLogger);
    }

    static const char *typeStr() noexcept
    {
        return jsonstr::dynLenArray;
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            FcValReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(
                this->_logger(), jsonVal.loc(), "Invalid dynamic-length array field class.");
        }
    }
};

/*
 * Returns the pair (suitable for insertion into a
 * `bt2c::JsonObjValReq::PropReqs` instance) for the CTF 2 selector
 * field location object property requirement.
 */
bt2c::JsonObjValReq::PropReqsEntry selFieldLocPropReqEntry(const bt2c::Logger& parentLogger)
{
    return {jsonstr::selFieldLoc, {FieldLocValReq::shared(parentLogger), true}};
}

/*
 * Returns the pair (suitable for insertion into a
 * `bt2c::JsonObjValReq::PropReqs` instance) for the CTF 2 selector
 * field ranges object property requirement.
 */
bt2c::JsonObjValReq::PropReqsEntry selFieldRangesPropReqEntry(const bool isRequired,
                                                              const bt2c::Logger& parentLogger)
{
    return {jsonstr::selFieldRanges,
            {ctf::src::Ctf2JsonIntRangeSetValReq::shared(parentLogger), isRequired}};
}

/*
 * CTF 2 JSON optional field class value requirement.
 */
class OptionalFcValReq final : public FcValReq
{
public:
    explicit OptionalFcValReq(const AnyFullBlownFcValReq& anyFullBlownFcValReq,
                              const bt2c::Logger& parentLogger)
        : /* clang-format off */
        FcValReq {this->typeStr(), {
            anyFcPropReqEntry(jsonstr::fc, anyFullBlownFcValReq, parentLogger),
            selFieldLocPropReqEntry(parentLogger),
            selFieldRangesPropReqEntry(false, parentLogger),
        }, parentLogger}
    /* clang-format on */
    {
    }

    static SP shared(const AnyFullBlownFcValReq& anyFullBlownFcValReq,
                     const bt2c::Logger& parentLogger)
    {
        return std::make_shared<OptionalFcValReq>(anyFullBlownFcValReq, parentLogger);
    }

    static const char *typeStr() noexcept
    {
        return jsonstr::optional;
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            FcValReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(this->_logger(), jsonVal.loc(),
                                                              "Invalid optional field class.");
        }
    }
};

/*
 * CTF 2 JSON variant field class option class value requirement.
 */
class VariantFcOptValReq final : public bt2c::JsonObjValReq
{
public:
    explicit VariantFcOptValReq(const AnyFullBlownFcValReq& anyFullBlownFcValReq,
                                const bt2c::Logger& parentLogger)
        : /* clang-format off */
        bt2c::JsonObjValReq {{
            namePropReqEntry(false, parentLogger),
            anyFcPropReqEntry(jsonstr::fc, anyFullBlownFcValReq, parentLogger),
            selFieldRangesPropReqEntry(true, parentLogger),
            attrsPropReqEntry(parentLogger),
            extPropReqEntry(parentLogger),
        }, parentLogger}
    /* clang-format on */
    {
    }

    static SP shared(const AnyFullBlownFcValReq& anyFullBlownFcValReq,
                     const bt2c::Logger& parentLogger)
    {
        return std::make_shared<VariantFcOptValReq>(anyFullBlownFcValReq, parentLogger);
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        /*
         * Not checking for integer range overlaps here because we don't
         * know the signedness of those ranges yet (depends on the
         * effective selector field class(es)).
         *
         * This will be easier to do once we know the signedness,
         * comparing only integers having the same type.
         */
        try {
            bt2c::JsonObjValReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(
                this->_logger(), jsonVal.loc(), "Invalid variant field class option.");
        }
    }
};

/*
 * CTF 2 JSON variant field class value requirement.
 */
class VariantFcValReq final : public FcValReq
{
public:
    explicit VariantFcValReq(const AnyFullBlownFcValReq& anyFullBlownFcValReq,
                             const bt2c::Logger& parentLogger)
        : /* clang-format off */
        FcValReq {this->typeStr(), {
            {jsonstr::opts, {
                bt2c::JsonArrayValReq::shared(1, std::nullopt,
                                              VariantFcOptValReq::shared(anyFullBlownFcValReq, parentLogger),
                                              parentLogger),
                true
            }},
            selFieldLocPropReqEntry(parentLogger),
        }, parentLogger},
        _mUniqueEntryNamesValidator {parentLogger}
    /* clang-format on */
    {
    }

    static SP shared(const AnyFullBlownFcValReq& anyFullBlownFcValReq,
                     const bt2c::Logger& parentLogger)
    {
        return std::make_shared<VariantFcValReq>(anyFullBlownFcValReq, parentLogger);
    }

    static const char *typeStr() noexcept
    {
        return jsonstr::variant;
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            FcValReq::_validate(jsonVal);

            /* Validate that option names are unique */
            _mUniqueEntryNamesValidator.validate(jsonVal, jsonstr::opts,
                                                 "variant field class option");
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(this->_logger(), jsonVal.loc(),
                                                              "Invalid variant field class.");
        }
    }

    UniqueEntryNamesValidator _mUniqueEntryNamesValidator;
};

/*
 * CTF 2 JSON (any) full-blown field class value requirement.
 */
class AnyFullBlownFcValReq final : public bt2c::JsonObjValReq
{
public:
    explicit AnyFullBlownFcValReq(const bt2c::Logger& parentLogger)
        : /* clang-format off */
        bt2c::JsonObjValReq {{
            {
                jsonstr::type,
                {
                    bt2c::JsonStrValInSetReq::shared({
                        FixedLenBitArrayFcValReq::typeStr(),
                        FixedLenBoolFcValReq::typeStr(),
                        FixedLenBitMapFcValReq::typeStr(),
                        FixedLenUIntFcValReq::typeStr(),
                        FixedLenSIntFcValReq::typeStr(),
                        FixedLenFloatFcValReq::typeStr(),
                        VarLenUIntFcValReq::typeStr(),
                        VarLenSIntFcValReq::typeStr(),
                        NullTerminatedStrFcValReq::typeStr(),
                        StaticLenStrFcValReq::typeStr(),
                        DynLenStrFcValReq::typeStr(),
                        StaticLenBlobFcValReq::typeStr(),
                        DynLenBlobFcValReq::typeStr(),
                        StructFcValReq::typeStr(),
                        StaticLenArrayFcValReq::typeStr(),
                        DynLenArrayFcValReq::typeStr(),
                        OptionalFcValReq::typeStr(),
                        VariantFcValReq::typeStr(),
                    }, parentLogger), true
                }
            }
        }, true, parentLogger},
        _mFlBitArrayFcValReq {parentLogger},
        _mFlBoolFcValReq {parentLogger},
        _mFlBitMapFcValReq {parentLogger},
        _mFlUIntFcValReq {parentLogger},
        _mFlSIntFcValReq {parentLogger},
        _mFlFloatFcValReq {parentLogger},
        _mVlUIntFcValReq {parentLogger},
        _mVlSIntFcValReq {parentLogger},
        _mNtStrFcValReq {parentLogger},
        _mStaticLenStrFcValReq {parentLogger},
        _mDynLenStrFcValReq {parentLogger},
        _mStaticLenBlobFcValReq {parentLogger},
        _mDynLenBlobFcValReq {parentLogger},
        _mStructFcValReq {*this, parentLogger},
        _mStaticLenArrayFcValReq {*this, parentLogger},
        _mDynLenArrayFcValReq {*this, parentLogger},
        _mOptionalFcValReq {*this, parentLogger},
        _mVariantFcValReq {*this, parentLogger}
    /* clang-format on */
    {
        this->_addToFcValReqs(_mFlBitArrayFcValReq);
        this->_addToFcValReqs(_mFlBoolFcValReq);
        this->_addToFcValReqs(_mFlBitMapFcValReq);
        this->_addToFcValReqs(_mFlUIntFcValReq);
        this->_addToFcValReqs(_mFlSIntFcValReq);
        this->_addToFcValReqs(_mFlFloatFcValReq);
        this->_addToFcValReqs(_mVlUIntFcValReq);
        this->_addToFcValReqs(_mVlSIntFcValReq);
        this->_addToFcValReqs(_mNtStrFcValReq);
        this->_addToFcValReqs(_mStaticLenStrFcValReq);
        this->_addToFcValReqs(_mDynLenStrFcValReq);
        this->_addToFcValReqs(_mStaticLenBlobFcValReq);
        this->_addToFcValReqs(_mDynLenBlobFcValReq);
        this->_addToFcValReqs(_mStructFcValReq);
        this->_addToFcValReqs(_mStaticLenArrayFcValReq);
        this->_addToFcValReqs(_mDynLenArrayFcValReq);
        this->_addToFcValReqs(_mOptionalFcValReq);
        this->_addToFcValReqs(_mVariantFcValReq);
    }

private:
    template <typename JsonValReqT>
    void _addToFcValReqs(const JsonValReqT& valReq)
    {
        const auto typeStr = JsonValReqT::typeStr();

        BT_ASSERT(!bt2c::contains(_mFcValReqs, typeStr));
        _mFcValReqs.insert(std::pair {typeStr, &valReq});
    }

    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            bt2c::JsonObjValReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(this->_logger(), jsonVal.loc(),
                                                              "Invalid field class.");
        }

        /*
         * This part doesn't need to be catched because the specific
         * _validate() method already appends a message like "Invalid
         * xyz field class:" to the exception.
         */
        const auto it = _mFcValReqs.find(*jsonVal.asObj()[jsonstr::type]->asStr());

        BT_ASSERT(it != _mFcValReqs.end());
        it->second->validate(jsonVal);
    }

    /* Subrequirements */
    FixedLenBitArrayFcValReq _mFlBitArrayFcValReq;
    FixedLenBoolFcValReq _mFlBoolFcValReq;
    FixedLenBitMapFcValReq _mFlBitMapFcValReq;
    FixedLenUIntFcValReq _mFlUIntFcValReq;
    FixedLenSIntFcValReq _mFlSIntFcValReq;
    FixedLenFloatFcValReq _mFlFloatFcValReq;
    VarLenUIntFcValReq _mVlUIntFcValReq;
    VarLenSIntFcValReq _mVlSIntFcValReq;
    NullTerminatedStrFcValReq _mNtStrFcValReq;
    StaticLenStrFcValReq _mStaticLenStrFcValReq;
    DynLenStrFcValReq _mDynLenStrFcValReq;
    StaticLenBlobFcValReq _mStaticLenBlobFcValReq;
    DynLenBlobFcValReq _mDynLenBlobFcValReq;
    StructFcValReq _mStructFcValReq;
    StaticLenArrayFcValReq _mStaticLenArrayFcValReq;
    DynLenArrayFcValReq _mDynLenArrayFcValReq;
    OptionalFcValReq _mOptionalFcValReq;
    VariantFcValReq _mVariantFcValReq;

    /*
     * Field class type string to JSON field class requirement.
     *
     * Values are owned by the members above.
     */
    std::unordered_map<std::string, const bt2c::JsonValReq *> _mFcValReqs;
};

void AnyFcValReqWrapper::_validate(const bt2c::JsonVal& jsonVal) const
{
    /* Check for field class alias name first (JSON string) */
    if (jsonVal.isStr()) {
        /*
         * Always valid: Ctf2FcBuilder::buildFcFromJsonVal() will
         * validate that the field class alias exists.
         */
        return;
    }

    /* Delegate to AnyFullBlownFcValReq::validate() */
    _mAnyFullBlownFcValReq->validate(jsonVal);
}

/*
 * CTF 2 JSON fragment value abstract requirement.
 */
class FragmentValReq : public bt2c::JsonObjValReq
{
protected:
    /*
     * Builds a CTF 2 JSON fragment value requirement of type `type`,
     * adding `propReqs` to the base JSON object value property
     * requirements.
     */
    explicit FragmentValReq(std::string&& type, PropReqs&& propReqs,
                            const bt2c::Logger& parentLogger)
        : bt2c::JsonObjValReq {
              this->_buildPropReqs(std::move(type), std::move(propReqs), parentLogger),
              parentLogger}
    {
    }

    /*
     * Builds a CTF 2 JSON fragment value requirement of type `type`.
     */
    explicit FragmentValReq(std::string&& type, const bt2c::Logger& parentLogger)
        : FragmentValReq {std::move(type), {}, parentLogger}
    {
    }

private:
    static PropReqs _buildPropReqs(std::string&& type, PropReqs&& propReqs,
                                   const bt2c::Logger& parentLogger)
    {
        propReqs.insert(objTypePropReqEntry(std::move(type), parentLogger));
        propReqs.insert(attrsPropReqEntry(parentLogger));
        propReqs.insert(extPropReqEntry(parentLogger));
        return std::move(propReqs);
    }
};

/*
 * CTF 2 preamble fragment value requirement.
 */
class PreambleFragmentValReq final : public FragmentValReq
{
public:
    explicit PreambleFragmentValReq(const bt2c::Logger& parentLogger)
        : /* clang-format off */
        FragmentValReq {this->typeStr(), {
            {jsonstr::version, {bt2c::JsonUIntValInSetReq::shared(2, parentLogger), true}},
            {jsonstr::uuid, {UuidValReq::shared(parentLogger)}},
        }, parentLogger}
    /* clang-format on */
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<PreambleFragmentValReq>(parentLogger);
    }

    static const char *typeStr() noexcept
    {
        return jsonstr::preamble;
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            FragmentValReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(this->_logger(), jsonVal.loc(),
                                                              "Invalid preamble fragment.");
        }
    }
};

/*
 * CTF 2 field class alias fragment value requirement.
 */
class FcAliasFragmentValReq final : public FragmentValReq
{
public:
    explicit FcAliasFragmentValReq(const bt2c::Logger& parentLogger)
        : /* clang-format off */
        FragmentValReq {this->typeStr(), {
            namePropReqEntry(true, parentLogger),
            anyFcPropReqEntry(jsonstr::fc, _mAnyFullBlownFcValReq, true, parentLogger),
        }, parentLogger},
        _mAnyFullBlownFcValReq {parentLogger}
    /* clang-format on */
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<FcAliasFragmentValReq>(parentLogger);
    }

    static const char *typeStr() noexcept
    {
        return jsonstr::fcAlias;
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            FragmentValReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(
                this->_logger(), jsonVal.loc(), "Invalid field class alias fragment.");
        }
    }

private:
    AnyFullBlownFcValReq _mAnyFullBlownFcValReq;
};

/*
 * CTF 2 JSON clock offset value requirement.
 */
class ClkOffsetValReq final : public bt2c::JsonObjValReq
{
public:
    explicit ClkOffsetValReq(const bt2c::Logger& parentLogger)
        : /* clang-format off */
        bt2c::JsonObjValReq {{
            {jsonstr::seconds, {bt2c::JsonAnyIntValReq::shared(parentLogger)}},
            {jsonstr::cycles, {bt2c::JsonValHasTypeReq::shared(bt2c::ValType::UInt, parentLogger)}},
        }, parentLogger}
    /* clang-format on */
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<ClkOffsetValReq>(parentLogger);
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            bt2c::JsonObjValReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(this->_logger(), jsonVal.loc(),
                                                              "Invalid clock offset.");
        }
    }
};

/*
 * Returns the pair (suitable for insertion into a
 * `bt2c::JsonObjValReq::PropReqs` instance) for the CTF 2 object
 * namespace object property requirement.
 */
bt2c::JsonObjValReq::PropReqsEntry nsPropReqEntry(const bt2c::Logger& parentLogger)
{
    return {jsonstr::ns, {bt2c::JsonValHasTypeReq::shared(bt2c::ValType::Str, parentLogger)}};
}

/*
 * Returns the pair (suitable for insertion into a
 * `bt2c::JsonObjValReq::PropReqs` instance) for the CTF 2 object UID
 * object property requirement.
 */
bt2c::JsonObjValReq::PropReqsEntry uidPropReqEntry(const bool isRequired,
                                                   const bt2c::Logger& parentLogger)
{
    return {jsonstr::uid,
            {bt2c::JsonValHasTypeReq::shared(bt2c::ValType::Str, parentLogger), isRequired}};
}

/*
 * CTF 2 JSON clock origin object value requirement.
 */
class ClkOriginObjValReq final : public bt2c::JsonObjValReq
{
public:
    explicit ClkOriginObjValReq(const bt2c::Logger& parentLogger)
        : /* clang-format off */
        bt2c::JsonObjValReq {{
            nsPropReqEntry(parentLogger),
            namePropReqEntry(true, parentLogger),
            uidPropReqEntry(true, parentLogger),
        }, parentLogger}
    /* clang-format on */
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<ClkOffsetValReq>(parentLogger);
    }
};

/*
 * CTF 2 JSON clock class origin property requirement.
 */
class ClkClsOriginValReq final : public bt2c::JsonValReq
{
public:
    explicit ClkClsOriginValReq(const bt2c::Logger& parentLogger)
        : bt2c::JsonValReq {parentLogger},
          _mObjReq {parentLogger}
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<ClkClsOriginValReq>(parentLogger);
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            /* Check for `unix-epoch` string */
            if (jsonVal.isStr()) {
                if (*jsonVal.asStr() != jsonstr::unixEpoch) {
                    BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_THROW_SPEC(
                        this->_logger(), bt2c::Error, jsonVal.loc(), "Expecting `{}`.",
                        jsonstr::unixEpoch);
                }
            } else {
                /* Must be a valid clock origin object */
                if (!jsonVal.isObj()) {
                    /* Make a clear message about the expected type */
                    BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_THROW_SPEC(
                        this->_logger(), bt2c::Error, jsonVal.loc(),
                        "Expecting a string or an object.");
                }

                /* Delegate to ClkOriginObjValReq::validate() */
                _mObjReq.validate(jsonVal);
            }
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(this->_logger(), jsonVal.loc(),
                                                              "Invalid clock origin.");
        }
    }

private:
    ClkOriginObjValReq _mObjReq;
};

/*
 * CTF 2 JSON clock class fragment value requirement.
 */
class ClkClsFragmentValReq final : public FragmentValReq
{
public:
    explicit ClkClsFragmentValReq(const bt2c::Logger& parentLogger)
        : /* clang-format off */
        FragmentValReq {this->typeStr(), {
            {jsonstr::id, {bt2c::JsonValHasTypeReq::shared(bt2c::ValType::Str, parentLogger), true}},
            nsPropReqEntry(parentLogger),
            namePropReqEntry(false, parentLogger),
            uidPropReqEntry(false, parentLogger),
            {jsonstr::freq, {bt2c::JsonUIntValInRangeReq::shared(1, std::nullopt, parentLogger), true}},
            {jsonstr::descr, {bt2c::JsonValHasTypeReq::shared(bt2c::ValType::Str, parentLogger)}},
            {jsonstr::origin, {ClkClsOriginValReq::shared(parentLogger)}},
            {jsonstr::offsetFromOrigin, {ClkOffsetValReq::shared(parentLogger)}},
            {jsonstr::precision, {bt2c::JsonValHasTypeReq::shared(bt2c::ValType::UInt, parentLogger)}},
            {jsonstr::accuracy, {bt2c::JsonValHasTypeReq::shared(bt2c::ValType::UInt, parentLogger)}},
        }, parentLogger}
    /* clang-format on */
    {
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<ClkClsFragmentValReq>(parentLogger);
    }

    static const char *typeStr() noexcept
    {
        return jsonstr::clkCls;
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            FragmentValReq::_validate(jsonVal);

            /*
             * Validate that `seconds` within `offset-from-origin`, if
             * it exists, is less than `frequency`.
             */
            auto& jsonObj = jsonVal.asObj();

            if (const auto jsonOffset = jsonObj[jsonstr::offsetFromOrigin]) {
                if (const auto jsonCycles = jsonOffset->asObj()[jsonstr::cycles]) {
                    const auto cycles = *jsonCycles->asUInt();
                    const auto freq = *jsonObj[jsonstr::freq]->asUInt();

                    if (cycles >= freq) {
                        BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_THROW_SPEC(
                            this->_logger(), bt2c::Error, jsonCycles->loc(),
                            "Invalid `{}` property of `{}` property: "
                            "value {} is greater than the value of the `{}` property ({}).",
                            jsonstr::cycles, jsonstr::offsetFromOrigin, cycles, jsonstr::freq,
                            freq);
                    }
                }
            }
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(this->_logger(), jsonVal.loc(),
                                                              "Invalid clock class fragment.");
        }
    }
};

/*
 * CTF 2 JSON trace class fragment value requirement.
 */
class TraceClsFragmentValReq final : public FragmentValReq
{
public:
    explicit TraceClsFragmentValReq(const AnyFullBlownFcValReq& anyFullBlownFcValReq,
                                    const bt2c::Logger& parentLogger)
        : /* clang-format off */
        FragmentValReq {this->typeStr(), {
            nsPropReqEntry(parentLogger),
            namePropReqEntry(false, parentLogger),
            uidPropReqEntry(false, parentLogger),
            {jsonstr::env, {TraceEnvValReq::shared(parentLogger)}},
            anyFcPropReqEntry(jsonstr::pktHeaderFc, anyFullBlownFcValReq, parentLogger),
        }, parentLogger}
    /* clang-format on */
    {
    }

    static SP shared(const AnyFullBlownFcValReq& anyFullBlownFcValReq,
                     const bt2c::Logger& parentLogger)
    {
        return std::make_shared<TraceClsFragmentValReq>(anyFullBlownFcValReq, parentLogger);
    }

    static const char *typeStr() noexcept
    {
        return jsonstr::traceCls;
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            FragmentValReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(this->_logger(), jsonVal.loc(),
                                                              "Invalid trace class fragment.");
        }
    }
};

/*
 * Returns the pair (suitable for insertion into a
 * `bt2c::JsonObjValReq::PropReqs` instance) for the CTF 2 object
 * numeric ID object property requirement.
 */
bt2c::JsonObjValReq::PropReqsEntry idPropReqEntry(const bt2c::Logger& parentLogger)
{
    return {jsonstr::id, {bt2c::JsonValHasTypeReq::shared(bt2c::ValType::UInt, parentLogger)}};
}

/*
 * CTF 2 JSON data stream class fragment value requirement.
 */
class DataStreamClsFragmentValReq final : public FragmentValReq
{
public:
    explicit DataStreamClsFragmentValReq(const AnyFullBlownFcValReq& anyFullBlownFcValReq,
                                         const bt2c::Logger& parentLogger)
        : /* clang-format off */
        FragmentValReq {this->typeStr(), {
            idPropReqEntry(parentLogger),
            nsPropReqEntry(parentLogger),
            namePropReqEntry(false, parentLogger),
            uidPropReqEntry(false, parentLogger),
            {jsonstr::defClkClsId, {bt2c::JsonValHasTypeReq::shared(bt2c::ValType::Str, parentLogger)}},
            anyFcPropReqEntry(jsonstr::pktCtxFc, anyFullBlownFcValReq, parentLogger),
            anyFcPropReqEntry(jsonstr::eventRecordHeaderFc, anyFullBlownFcValReq, parentLogger),
            anyFcPropReqEntry(jsonstr::eventRecordCommonCtxFc, anyFullBlownFcValReq, parentLogger),
        }, parentLogger}
    /* clang-format on */
    {
    }

    static SP shared(const AnyFullBlownFcValReq& anyFullBlownFcValReq,
                     const bt2c::Logger& parentLogger)
    {
        return std::make_shared<DataStreamClsFragmentValReq>(anyFullBlownFcValReq, parentLogger);
    }

    static const char *typeStr() noexcept
    {
        return jsonstr::dataStreamCls;
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            FragmentValReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(
                this->_logger(), jsonVal.loc(), "Invalid data stream class fragment.");
        }
    }
};

/*
 * CTF 2 JSON event record class fragment value requirement.
 */
class EventRecordClsFragmentValReq final : public FragmentValReq
{
public:
    explicit EventRecordClsFragmentValReq(const AnyFullBlownFcValReq& anyFullBlownFcValReq,
                                          const bt2c::Logger& parentLogger)
        : /* clang-format off */
        FragmentValReq {this->typeStr(), {
            idPropReqEntry(parentLogger),
            nsPropReqEntry(parentLogger),
            namePropReqEntry(false, parentLogger),
            uidPropReqEntry(false, parentLogger),
            {jsonstr::dataStreamClsId, {bt2c::JsonValHasTypeReq::shared(bt2c::ValType::UInt, parentLogger)}},
            anyFcPropReqEntry(jsonstr::specCtxFc, anyFullBlownFcValReq, parentLogger),
            anyFcPropReqEntry(jsonstr::payloadFc, anyFullBlownFcValReq, parentLogger),
        }, parentLogger}
    /* clang-format on */
    {
    }

    static SP shared(const AnyFullBlownFcValReq& anyFullBlownFcValReq,
                     const bt2c::Logger& parentLogger)
    {
        return std::make_shared<EventRecordClsFragmentValReq>(anyFullBlownFcValReq, parentLogger);
    }

    static const char *typeStr() noexcept
    {
        return jsonstr::eventRecordCls;
    }

private:
    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            FragmentValReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(
                this->_logger(), jsonVal.loc(), "Invalid event record class fragment.");
        }
    }
};

} /* namespace */

namespace internal {

/*
 * CTF 2 JSON (any) fragment value requirement (implementation).
 */
class Ctf2JsonAnyFragmentValReqImpl final : public bt2c::JsonObjValReq
{
public:
    explicit Ctf2JsonAnyFragmentValReqImpl(const bt2c::Logger& parentLogger)
        : /* clang-format off */
        bt2c::JsonObjValReq {{
            {
                jsonstr::type, {
                    bt2c::JsonStrValInSetReq::shared({
                        PreambleFragmentValReq::typeStr(),
                        FcAliasFragmentValReq::typeStr(),
                        TraceClsFragmentValReq::typeStr(),
                        ClkClsFragmentValReq::typeStr(),
                        DataStreamClsFragmentValReq::typeStr(),
                        EventRecordClsFragmentValReq::typeStr(),
                    }, parentLogger), true
                }
            }
        }, true, parentLogger},
        _mAnyFullBlownFcValReq {parentLogger},
        _preambleFragmentValReq {parentLogger},
        _fcAliasFragmentValReq {parentLogger},
        _traceClsFragmentValReq {_mAnyFullBlownFcValReq, parentLogger},
        _clkClsFragmentValReq {parentLogger},
        _dataStreamClsFragmentValReq {_mAnyFullBlownFcValReq, parentLogger},
        _eventRecordClsFragmentValReq {_mAnyFullBlownFcValReq, parentLogger}
    /* clang-format on */
    {
        this->_addToFcValReqs(_preambleFragmentValReq);
        this->_addToFcValReqs(_fcAliasFragmentValReq);
        this->_addToFcValReqs(_traceClsFragmentValReq);
        this->_addToFcValReqs(_clkClsFragmentValReq);
        this->_addToFcValReqs(_dataStreamClsFragmentValReq);
        this->_addToFcValReqs(_eventRecordClsFragmentValReq);
    }

    static SP shared(const bt2c::Logger& parentLogger)
    {
        return std::make_shared<Ctf2JsonAnyFragmentValReqImpl>(parentLogger);
    }

private:
    template <typename JsonValReqT>
    void _addToFcValReqs(const JsonValReqT& valReq)
    {
        const auto typeStr = JsonValReqT::typeStr();

        BT_ASSERT(!bt2c::contains(_fragValReqs, typeStr));
        _fragValReqs.insert(std::pair {typeStr, &valReq});
    }

    void _validate(const bt2c::JsonVal& jsonVal) const override
    {
        try {
            bt2c::JsonObjValReq::_validate(jsonVal);
        } catch (const bt2c::Error&) {
            BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(this->_logger(), jsonVal.loc(),
                                                              "Invalid fragment.");
        }

        /*
         * This part doesn't need to be catched because the specific
         * _validate() method already appends a message like
         * "Invalid xyz fragment:" to the exception.
         */
        const auto it = _fragValReqs.find(*jsonVal.asObj()[jsonstr::type]->asStr());

        BT_ASSERT(it != _fragValReqs.end());
        it->second->validate(jsonVal);
    }

    /* Single any full-blown field class value requirement instance */
    AnyFullBlownFcValReq _mAnyFullBlownFcValReq;

    /* Subrequirements */
    PreambleFragmentValReq _preambleFragmentValReq;
    FcAliasFragmentValReq _fcAliasFragmentValReq;
    TraceClsFragmentValReq _traceClsFragmentValReq;
    ClkClsFragmentValReq _clkClsFragmentValReq;
    DataStreamClsFragmentValReq _dataStreamClsFragmentValReq;
    EventRecordClsFragmentValReq _eventRecordClsFragmentValReq;

    /*
     * Fragment type string to JSON fragment requirement.
     *
     * Values are owned by the members above.
     */
    std::unordered_map<std::string, const bt2c::JsonValReq *> _fragValReqs;
};

} /* namespace internal */

Ctf2JsonAnyFragmentValReq::Ctf2JsonAnyFragmentValReq(const bt2c::Logger& parentLogger)
    : bt2c::JsonValReq {parentLogger},
      _mImpl {new internal::Ctf2JsonAnyFragmentValReqImpl {parentLogger}}
{
}

Ctf2JsonAnyFragmentValReq::~Ctf2JsonAnyFragmentValReq()
{
}

void Ctf2JsonAnyFragmentValReq::_validate(const bt2c::JsonVal& jsonVal) const
{
    _mImpl->validate(jsonVal);
}

} /* namespace ctf::src */
