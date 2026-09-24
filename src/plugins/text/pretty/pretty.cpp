/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2016 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 * Copyright 2026 Philippe Proulx <pproulx@efficios.com>
 */

#include <fstream>
#include <iostream>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <variant>

#include <glib.h>

#include "common/common.h"
#include "cpp-common/bt2c/c-string-view.hpp"

#include "plugins/common/param-validation/param-validation.h"

#include "pretty.hpp"

namespace bt2pretty {

namespace {

const char *colorChoices[] = {"never", "auto", "always", nullptr};
const char *showHideChoices[] = {"show", "hide", nullptr};

bt_param_validation_map_value_entry_descr prettyParams[] = {
    {"color", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeString(colorChoices)},
    {"path", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeString()},
    {"no-delta", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeBool()},
    {"clock-cycles", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeBool()},
    {"clock-seconds", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeBool()},
    {"clock-date", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeBool()},
    {"clock-gmt", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeBool()},
    {"verbose", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeBool()},

    {"name-default", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeString(showHideChoices)},
    {"name-payload", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeBool()},
    {"name-context", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeBool()},
    {"name-scope", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeBool()},
    {"name-header", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeBool()},

    {"field-default", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeString(showHideChoices)},
    {"field-trace", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeBool()},
    {"field-trace:hostname", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeBool()},
    {"field-trace:domain", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeBool()},
    {"field-trace:procname", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeBool()},
    {"field-trace:vpid", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeBool()},
    {"field-loglevel", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeBool()},
    {"field-emf", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeBool()},
    {"field-callsite", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeBool()},
    {"print-enum-flags", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeBool()},
    BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END};

/*
 * Sets `option` to the boolean value of the `key` entry of `params`, or
 * to `def` when `params` has no such entry.
 */
void applyOneBoolWithDef(const bt2::ConstMapValue params, const bt2c::CStringView key, bool& option,
                         const bool def)
{
    if (const auto val = params[key]) {
        option = val->asBool().value();
    } else {
        option = def;
    }
}

/*
 * Sets `option` to the boolean value of the `key` entry of `params`,
 * leaving it untouched when `params` has no such entry.
 */
void applyOneBoolIfSpecified(const bt2::ConstMapValue params, const bt2c::CStringView key,
                             bool& option)
{
    if (const auto val = params[key]) {
        option = val->asBool().value();
    }
}

/*
 * Builds and creates `Writer` options from the initialization
 * parameters `params`, considering `out` as the effective output stream
 * of the future `Writer` instance.
 */
WriterOpts writerOptsFromParams(const bt2::ConstMapValue params, const std::ostream& out)
{
    /* Validate parameters */
    {
        gchar *validateError = nullptr;

        if (const auto validationStatus =
                bt_param_validation_validate(params.libObjPtr(), prettyParams, &validateError);
            validationStatus == BT_PARAM_VALIDATION_STATUS_MEMORY_ERROR) {
            g_free(validateError);
            throw bt2::MemoryError {};
        } else if (validationStatus == BT_PARAM_VALIDATION_STATUS_VALIDATION_ERROR) {
            const std::string errStr {validateError ? validateError : ""};

            g_free(validateError);
            throw bt2c::Error {errStr};
        }

        g_free(validateError);
    }

    enum class DefaultMode
    {
        Unset,
        Show,
        Hide,
    };

    /* Default writer options */
    WriterOpts opts;

    /* Color support */
    {
        bt2c::CStringView color = "auto";

        if (const auto val = params["color"]) {
            color = val->asString().value();
        }

        bt_common_color_when paramWhen;

        if (color == "never") {
            paramWhen = BT_COMMON_COLOR_WHEN_NEVER;
        } else if (color == "always") {
            paramWhen = BT_COMMON_COLOR_WHEN_ALWAYS;
        } else {
            BT_ASSERT(color == "auto");

            /*
             * For `auto`, only consider the connected terminal when the
             * output stream is the standard output: if the user is
             * redirecting the output to a file with the "path"
             * parameter, it's not a terminal at all.
             */
            paramWhen = &out == &std::cout ? BT_COMMON_COLOR_WHEN_AUTO : BT_COMMON_COLOR_WHEN_NEVER;
        }

        /*
         * Let the `BABELTRACE_TERM_COLOR` and `NO_COLOR` environment
         * variables override the `color` parameter, as the manual
         * page promises.
         */
        opts.colorCodes = bt_common_color_get_codes(bt_common_color_when_from_param(paramWhen));
    }

    /* Reverse logic here */
    {
        applyOneBoolWithDef(params, "no-delta", opts.writeTsDelta, false);
        opts.writeTsDelta = !opts.writeTsDelta;
    }

    /* Basic options */
    /* Timestamp format: clock-cycles overrides everything else. */
    {
        bool clockCycles = false;
        bool clockSeconds = false;
        bool clockDate = false;
        bool clockGmt = false;

        applyOneBoolWithDef(params, "clock-cycles", clockCycles, false);
        applyOneBoolWithDef(params, "clock-seconds", clockSeconds, false);
        applyOneBoolWithDef(params, "clock-date", clockDate, false);
        applyOneBoolWithDef(params, "clock-gmt", clockGmt, false);

        if (clockCycles) {
            opts.clkFmt = ClkFormat::Cycles;
        } else if (clockSeconds) {
            opts.clkFmt = ClkFormat::SecFromOrigin;
        } else if (clockGmt) {
            opts.clkFmt = clockDate ? ClkFormat::UtcDateTime : ClkFormat::UtcTime;
        } else {
            opts.clkFmt = clockDate ? ClkFormat::LocalDateTime : ClkFormat::LocalTime;
        }
    }

    applyOneBoolWithDef(params, "print-enum-flags", opts.writeEnumFieldFlags, false);

    /* Names */
    {
        auto def = DefaultMode::Unset;

        if (const auto val = params["name-default"]) {
            if (const bt2c::CStringView str = val->asString().value(); str == "show") {
                def = DefaultMode::Show;
            } else {
                BT_ASSERT(str == "hide");
                def = DefaultMode::Hide;
            }
        }

        switch (def) {
        case DefaultMode::Unset:
            opts.writeEventPayloadFieldMemberNames = true;
            opts.writeCtxFieldMemberNames = true;
            opts.writeInfoNames = false;
            opts.writeScopeNames = false;
            break;

        case DefaultMode::Show:
            opts.writeEventPayloadFieldMemberNames = true;
            opts.writeCtxFieldMemberNames = true;
            opts.writeInfoNames = true;
            opts.writeScopeNames = true;
            break;

        case DefaultMode::Hide:
            opts.writeEventPayloadFieldMemberNames = false;
            opts.writeCtxFieldMemberNames = false;
            opts.writeInfoNames = false;
            opts.writeScopeNames = false;
            break;
        }

        applyOneBoolIfSpecified(params, "name-payload", opts.writeEventPayloadFieldMemberNames);
        applyOneBoolIfSpecified(params, "name-context", opts.writeCtxFieldMemberNames);
        applyOneBoolIfSpecified(params, "name-header", opts.writeInfoNames);
        applyOneBoolIfSpecified(params, "name-scope", opts.writeScopeNames);
    }

    /* Info items ("field" is the parameter term) */
    {
        auto def = DefaultMode::Unset;

        if (const auto val = params["field-default"]) {
            if (const bt2c::CStringView str = val->asString().value(); str == "show") {
                def = DefaultMode::Show;
            } else {
                BT_ASSERT(str == "hide");
                def = DefaultMode::Hide;
            }
        }

        switch (def) {
        case DefaultMode::Unset:
            opts.writeTraceName = false;
            opts.writeTraceEnvHostname = true;
            opts.writeTraceEnvDomainName = false;
            opts.writeTraceEnvProcname = true;
            opts.writeTraceEnvVpid = true;
            opts.writeEventClsLogLevel = false;
            opts.writeEventClsEmf = false;
            break;

        case DefaultMode::Show:
            opts.writeTraceName = true;
            opts.writeTraceEnvHostname = true;
            opts.writeTraceEnvDomainName = true;
            opts.writeTraceEnvProcname = true;
            opts.writeTraceEnvVpid = true;
            opts.writeEventClsLogLevel = true;
            opts.writeEventClsEmf = true;
            break;

        case DefaultMode::Hide:
            opts.writeTraceName = false;
            opts.writeTraceEnvHostname = false;
            opts.writeTraceEnvDomainName = false;
            opts.writeTraceEnvProcname = false;
            opts.writeTraceEnvVpid = false;
            opts.writeEventClsLogLevel = false;
            opts.writeEventClsEmf = false;
            break;
        }

        applyOneBoolIfSpecified(params, "field-trace", opts.writeTraceName);
        applyOneBoolIfSpecified(params, "field-trace:hostname", opts.writeTraceEnvHostname);
        applyOneBoolIfSpecified(params, "field-trace:domain", opts.writeTraceEnvDomainName);
        applyOneBoolIfSpecified(params, "field-trace:procname", opts.writeTraceEnvProcname);
        applyOneBoolIfSpecified(params, "field-trace:vpid", opts.writeTraceEnvVpid);
        applyOneBoolIfSpecified(params, "field-loglevel", opts.writeEventClsLogLevel);
        applyOneBoolIfSpecified(params, "field-emf", opts.writeEventClsEmf);
    }

    return opts;
}

constexpr const char *inPortName = "in";

} /* namespace */

Comp::Comp(const bt2::SelfSinkComponent selfComp, const bt2::ConstMapValue params, void *)
    : bt2::UserSinkComponent<Comp> {selfComp, "PLUGIN/SINK.TEXT.PRETTY"},

      /*
     * Open the output file (if any) before binding `_mOut` so color
     * resolution can decide based on the actual output stream.
     */
      _mOutFile {std::invoke([&] {
          std::ofstream out;

          if (const auto pathVal = params["path"]) {
              const auto path = pathVal->asString().value();

              out.open(path.data());

              if (!out) {
                  BT_CPPLOGE_APPEND_CAUSE_AND_THROW(
                      bt2c::Error, "Failed to open output file: path=\"{}\"", path);
              }
          }

          return out;
      })},
      _mOut {_mOutFile.is_open() ? &_mOutFile : &std::cout}
{
    try {
        this->_addInputPort(inPortName);
    } catch (const bt2c::Error&) {
        BT_CPPLOGE_APPEND_CAUSE_AND_RETHROW("Failed to add a single input port.");
    }

    auto opts = writerOptsFromParams(params, *_mOut);

    /*
     * Detect whether colors are enabled to pick the right
     * `Writer` specialization.
     *
     * bt_common_color_get_codes() returns a structure with empty
     * strings when colors are disabled and with non-empty escape
     * sequences (for example, `reset` is `\033[0m`) otherwise.
     */
    const auto emitTermCodes = opts.colorCodes.reset && opts.colorCodes.reset[0] != '\0';

    if (emitTermCodes) {
        _mWriter.emplace(std::in_place_type<Writer<true>>, opts, *_mOut, this->_graphMipVersion(),
                         _mLogger);
    } else {
        _mWriter.emplace(std::in_place_type<Writer<false>>, opts, *_mOut, this->_graphMipVersion(),
                         _mLogger);
    }
}

void Comp::_getSupportedMipVersions(bt2::SelfComponentClass, bt2::ConstValue, void *,
                                    bt2::LoggingLevel, const bt2::UnsignedIntegerRangeSet ranges)
{
    ranges.addRange(0, 1);
}

void Comp::_graphIsConfigured()
{
    const auto inPort = this->_inputPorts()[inPortName];

    if (!inPort.isConnected()) {
        BT_CPPLOGE_APPEND_CAUSE_AND_THROW(
            bt2c::Error, "Single input port is not connected: port-name=\"{}\"", inPortName);
    }

    _mMsgIter = this->_createMessageIterator(inPort);
}

bool Comp::_consume()
{
    std::optional<bt2::ConstMessageArray> msgs;

    try {
        msgs = _mMsgIter->next();
    } catch (const bt2c::Error&) {
        BT_CPPLOGE_APPEND_CAUSE_AND_RETHROW("Failed to get messages from upstream component.");
    }

    if (!msgs) {
        return false;
    }

    try {
        for (const auto msg : *msgs) {
            /* Select the active writer instance */
            std::visit(
                [msg](auto& writer) {
                    writer.writeMsg(msg);
                },
                *_mWriter);
        }
    } catch (const bt2c::Error&) {
        BT_CPPLOGE_APPEND_CAUSE_AND_RETHROW("Failed to write one message.");
    }

    return true;
}

} /* namespace bt2pretty */
