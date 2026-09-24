/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2016 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 * Copyright 2026 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_PLUGINS_TEXT_PRETTY_WRITER_HPP
#define BABELTRACE_PLUGINS_TEXT_PRETTY_WRITER_HPP

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <functional>
#include <iostream>
#include <iterator>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/compile.h>
#include <fmt/format.h>

#include "common/assert.h"
#include "common/common.h"
#include "compat/time.h"
#include "cpp-common/bt2/message.hpp"
#include "cpp-common/bt2c/c-string-view.hpp"
#include "cpp-common/bt2c/exc.hpp"
#include "cpp-common/bt2c/logging.hpp"

namespace bt2pretty {

enum class ClkFormat
{
    /* Raw cycle count since clock start */
    Cycles,

    /* Decimal seconds since clock origin (`[-]SEC.NS`) */
    SecFromOrigin,

    /* `HH:MM:SS.NSEC` (local time) */
    LocalTime,

    /* `YYYY-MM-DD HH:MM:SS.NSEC` (local time) */
    LocalDateTime,

    /* `HH:MM:SS.NSEC` in (UTC) */
    UtcTime,

    /* `YYYY-MM-DD HH:MM:SS.NSEC` in (UTC) */
    UtcDateTime,
};

struct WriterOpts final
{
    bool writeScopeNames = false;
    bool writeInfoNames = false;
    bool writeCtxFieldMemberNames = false;
    bool writeEventPayloadFieldMemberNames = false;
    bool writeTsDelta = false;
    bool writeEnumFieldFlags = false;
    bool writeEventClsLogLevel = false;
    bool writeEventClsEmf = false;
    bool writeTraceName = false;
    bool writeTraceEnvDomainName = false;
    bool writeTraceEnvProcname = false;
    bool writeTraceEnvVpid = false;
    bool writeTraceEnvHostname = false;
    ClkFormat clkFmt = ClkFormat::LocalTime;

    /*
     * Terminal ANSI color codes to use when writing.
     *
     * Each entry is an empty string when colors are disabled, therefore
     * appending unconditionally is always safe.
     */
    bt_common_color_codes colorCodes = {};
};

/*
 * Pretty-printing writer.
 *
 * When `EmitTermCodesV` is true, the writer emits terminal ANSI color
 * escape sequences taken from `WriterOpts::colorCodes`. When false, all
 * color-related code paths are eliminated at compile time and the
 * writer instance doesn't write any escape sequence ever.
 */
template <bool EmitTermCodesV>
class Writer final
{
public:
    /*
     * Builds a pretty-printing writer using `opts` as its options,
     * writing to the `out` stream, assuming a MIP version of
     * `mipVersion`, and using a logger derived from `parentLogger`
     * (same logging level).
     */
    explicit Writer(const WriterOpts& opts, std::ostream& out, const std::uint64_t mipVersion,
                    const bt2c::Logger& parentLogger)
        : _mOpts {opts},
          _mOut {&out},
          _mMipVersion {mipVersion},
          _mLogger {parentLogger, fmt::format("{}/WRITER", parentLogger.tag())}
    {
        if constexpr (EmitTermCodesV) {
            const auto& c = _mOpts.colorCodes;

            _mTermCodes.infoName = c.bold;
            _mTermCodes.fieldName = c.fg_cyan;
            _mTermCodes.rst = c.reset;
            _mTermCodes.strVal = c.bold;
            _mTermCodes.numberVal = c.bold;
            _mTermCodes.enumMappingName = c.bold;
            _mTermCodes.unknown = fmt::format("{}{}", c.bold, c.fg_bright_red);
            _mTermCodes.eventName = fmt::format("{}{}", c.bold, c.fg_bright_magenta);
            _mTermCodes.ts = fmt::format("{}{}", c.bold, c.fg_bright_yellow);
            _mTermCodes.warn = c.fg_yellow;
            _mTermCodes.warnTitle = fmt::format("{}{}", c.fg_yellow, c.bold);
        }

        if (_mOpts.writeEnumFieldFlags) {
            /*
             * Allocate all label arrays once and reuse the same set of
             * arrays for all enumerations.
             */
            for (auto& v : _mEnumBitLabels) {
                v.reserve(8);
            }
        }
    }

    ~Writer() = default;
    Writer(const Writer&) = delete;
    Writer(Writer&&) = delete;
    Writer& operator=(const Writer&) = delete;
    Writer& operator=(Writer&&) = delete;

    /*
     * Writes a pretty-printed representation of `msg` to the
     * output stream.
     *
     * Handles event messages and discarded-events/packets messages;
     * ignores other message types.
     */
    void writeMsg(const bt2::ConstMessage msg)
    {
        _mBuf.clear();
        _mWroteFirstItem = false;

        switch (msg.type()) {
        case bt2::MessageType::Event:
            this->_writeEventMsg(msg.asEvent());
            break;

        case bt2::MessageType::DiscardedEvents:
        case bt2::MessageType::DiscardedPackets:
            this->_writeDiscardedItemsMsg(msg);
            break;

        default:
            break;
        }
    }

private:
    /*
     * `bt_field_*_enumeration` are backed by 64-bit integers, therefore
     * the maximum number of bit flags in any enumeration field is 64.
     */
    static constexpr std::uint64_t _enumMaxBitFlagCount = 64;

    static constexpr std::int64_t _nsPerS = 1'000'000'000LL;

    /*
     * Returns `u` with all bits above the lowest `nBits` cleared
     * (handles `nBits == 64` safely).
     */
    static std::uint64_t _maskTo(const std::uint64_t u, const std::uint64_t nBits) noexcept
    {
        return u & ((nBits < 64) ? ((UINT64_C(1) << nBits) - 1) : UINT64_MAX);
    }

    static bt2c::CStringView _orUnknown(const bt2c::CStringView name) noexcept
    {
        return name ? name : "(unknown)";
    }

    /*
     * Get the labels of the enumeration field class `fc` that map to
     * `value`, appending them to `labels`.
     */
    template <typename FcT, typename ValT>
    static void _getEnumLabelsForValue(const FcT fc, const ValT value,
                                       std::vector<bt2c::CStringView>& labels)
    {
        for (const auto mapping : fc) {
            for (const auto range : mapping.ranges()) {
                const auto lower = range.lower();
                const auto upper = range.upper();

                /*
                 * Flag is active if this range represents a single value
                 * (lower equal to upper) and the lower value is the same as
                 * the bit value to test against.
                 */
                if (lower == upper && lower == value) {
                    labels.push_back(mapping.label());
                    break;
                }
            }
        }
    }

    /*
     * Appends `str` to the output buffer.
     */
    void _appendToBuf(const std::string_view str)
    {
        _mBuf.append(str.data(), str.data() + str.size());
    }

    /*
     * Appends the terminal escape sequence `termCode` to the output
     * buffer, calls `func()`, and then appends the reset
     * escape sequence.
     *
     * When `EmitTermCodesV` is false, the escape-sequence appends are
     * eliminated at compile time and only `func()` runs.
     */
    template <typename FuncT>
    void _withColor([[maybe_unused]] const std::string_view termCode, FuncT&& func)
    {
        if constexpr (EmitTermCodesV) {
            this->_appendToBuf(termCode);
            func();
            this->_appendToBuf(_mTermCodes.rst);
        } else {
            func();
        }
    }

    /*
     * Formats `args` according to `fmtStr` and appends the result to
     * the output buffer.
     *
     * `fmtStr` may be either a regular `fmt::format_string` or, on hot
     * paths, an `FMT_COMPILE(...)`-wrapped string so the format spec is
     * parsed at compile time.
     */
    template <typename FmtT, typename... ArgsT>
    void _appendFmtToBuf(const FmtT& fmtStr, ArgsT&&...args)
    {
        fmt::format_to(std::back_inserter(_mBuf), fmtStr, std::forward<ArgsT>(args)...);
    }

    /*
     * Appends `, ` to the output buffer to separate the next item from
     * the previous one in the current group, if a prior item was
     * already written, then marks `_mWroteFirstItem` so subsequent
     * calls get the separator.
     */
    void _appendItemSep()
    {
        if (_mWroteFirstItem) {
            this->_appendToBuf(", ");
        }

        _mWroteFirstItem = true;
    }

    /*
     * Writes `NAME = ` to the output buffer.
     *
     * Use this version for writer-chosen output keys such as
     * `stream.packet.context`, `timestamp`, and `loglevel`.
     */
    void _writeInfoNameEqual(const bt2c::CStringView name)
    {
        this->_withColor(_mTermCodes.infoName, [&] {
            this->_appendToBuf(name.data());
        });

        this->_appendToBuf(" = ");
    }

    /*
     * Writes `NAME = ` to the output buffer.
     *
     * Use this version for structure field member names.
     */
    void _writeFieldNameEqual(const bt2c::CStringView name)
    {
        this->_withColor(_mTermCodes.fieldName, [&] {
            this->_appendToBuf(name.data());
        });

        this->_appendToBuf(" = ");
    }

    /*
     * Writes the raw cycle value of `clkSnapshot` to the output buffer
     * as a zero-padded 20-digit decimal number.
     *
     * If `updateLast` is true, then this function also updates
     * `_mLastTs` and `_mTsDelta` so the next call can produce a delta.
     */
    void _writeTsCycles(const bt2::ConstClockSnapshot clockSnapshot, const bool updateLast)
    {
        const auto cycles = clockSnapshot.value();

        this->_appendFmtToBuf(FMT_COMPILE("{:020}"), cycles);

        if (updateLast) {
            if (_mLastTs) {
                _mTsDelta = cycles - *_mLastTs;
            }

            _mLastTs = cycles;
        }
    }

    /*
     * Writes a wall-clock representation of `*clkSnapshot` to the
     * output buffer, using the format that `_mOpts.clkFmt` selects.
     *
     * Writes a placeholder if `clkSnapshot` holds no clock snapshot.
     *
     * If `updateLast` is true, then this function also updates
     * `_mLastTs` and `_mTsDelta` so the next call can produce a delta.
     */
    void _writeTsWall(const bt2::OptionalBorrowedObject<bt2::ConstClockSnapshot> clkSnapshot,
                      const bool updateLast)
    {
        if (!clkSnapshot) {
            this->_appendToBuf("??:??:??.?????????");
            return;
        }

        std::int64_t tsNsec;

        try {
            tsNsec = clkSnapshot->nsFromOrigin();
        } catch (const bt2::OverflowError&) {
            const auto clkCls = clkSnapshot->clockClass();

            BT_CPPLOGW("Overflow computing nanoseconds from origin of clock snapshot: "
                       "clock-class-name=\"{}\", val={}, freq={}",
                       bt2c::maybeNull(clkCls.name().data()), clkSnapshot->value(),
                       clkCls.frequency());
            this->_appendToBuf("Error");
            return;
        }

        if (updateLast) {
            if (_mLastTs) {
                _mTsDelta = static_cast<std::uint64_t>(tsNsec) - *_mLastTs;
            }

            _mLastTs = static_cast<std::uint64_t>(tsNsec);
        }

        std::int64_t tsSec = 0;

        tsSec += tsNsec / _nsPerS;
        tsNsec = tsNsec % _nsPerS;

        std::uint64_t tsSecAbs;
        std::uint64_t tsNsAbs;
        bool isNeg;

        /*
         * `/` truncates toward zero and `%` carries the sign of the
         * dividend, therefore `tsSec` and `tsNsec` share a sign (with zero
         * compatible with either).
         */
        if (tsSec >= 0 && tsNsec >= 0) {
            isNeg = false;
            tsSecAbs = tsSec;
            tsNsAbs = tsNsec;
        } else {
            BT_ASSERT_DBG(tsSec <= 0);
            BT_ASSERT_DBG(tsNsec <= 0);
            isNeg = true;
            tsSecAbs = -tsSec;
            tsNsAbs = -tsNsec;
        }

        if (this->_tryWriteTsDateTime(tsSecAbs, tsNsAbs, isNeg)) {
            return;
        }

        /* Fall back to seconds from origin */
        this->_appendFmtToBuf(FMT_COMPILE("{}{}.{:09}"), isNeg ? "-" : "", tsSecAbs, tsNsAbs);
    }

    /*
     * Tries to write a date/time (depending on `_mOpts.clkFmt`) to the
     * output buffer for `tsSecAbs` and `tsNsAbs` (the absolute parts of
     * a timestamp, with `isNeg` indicating its sign) to the
     * output buffer.
     *
     * Returns false (writing nothing to the output buffer) if
     * `_mOpts.clkFmt` is `ClkFormat::SecFromOrigin`, the timestamp is
     * negative, or one of the underlying timing/format calls fails.
     * Callers should fall back to the `ClkFormat::SecFromOrigin` format
     * in that case.
     */
    bool _tryWriteTsDateTime(const std::uint64_t tsSecAbs, const std::uint64_t tsNsAbs,
                             const bool isNeg)
    {
        if (_mOpts.clkFmt == ClkFormat::SecFromOrigin) {
            return false;
        }

        if (isNeg && !_mNegTsWarnDone) {
            BT_CPPLOGW("Falling back to seconds from origin format for negative timestamp: "
                       "ts-s={}, ts-ns={}",
                       -static_cast<std::int64_t>(tsSecAbs), tsNsAbs);
            _mNegTsWarnDone = true;
            return false;
        }

        const auto isUtc =
            _mOpts.clkFmt == ClkFormat::UtcTime || _mOpts.clkFmt == ClkFormat::UtcDateTime;

        tm tm;
        const time_t timeS = static_cast<time_t>(tsSecAbs);

        if (const auto res = isUtc ? bt_gmtime_r(&timeS, &tm) : bt_localtime_r(&timeS, &tm); !res) {
            BT_CPPLOGW("Failed to convert seconds to date/time with {}():, ts-s-abs={}",
                       isUtc ? "gmtime" : "localtime", tsSecAbs);
            return false;
        }

        if (_mOpts.clkFmt == ClkFormat::LocalDateTime || _mOpts.clkFmt == ClkFormat::UtcDateTime) {
            char timeStr[26];

            if (const auto res = std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d ", &tm); !res) {
                BT_CPPLOGW("Failed to format date with std::strftime(): ts-s-abs={}", tsSecAbs);
                return false;
            }

            this->_appendToBuf(timeStr);
        }

        /* Write time */
        this->_appendFmtToBuf(FMT_COMPILE("{:02}:{:02}:{:02}.{:09}"), tm.tm_hour, tm.tm_min,
                              tm.tm_sec, tsNsAbs);
        return true;
    }

    /*
     * Writes the timestamp (and optional delta) of `eventMsg` to the
     * output buffer.
     *
     * Updates `_mWroteFirstItem` to reflect whether a previous item was
     * already written in the current group.
     */
    void _writeEventMsgTsAndDelta(const bt2::ConstEventMessage eventMsg)
    {
        if (!eventMsg.streamClassDefaultClockClass()) {
            /* No default clock class: skip the timestamp without error */
            return;
        }

        /* Timestamp */
        {
            const bt2::ConstClockSnapshot clkSnapshot = eventMsg.defaultClockSnapshot();

            if (_mOpts.writeInfoNames) {
                this->_writeInfoNameEqual("timestamp");
            } else {
                this->_appendToBuf("[");
            }

            this->_withColor(_mTermCodes.ts, [&] {
                if (_mOpts.clkFmt == ClkFormat::Cycles) {
                    this->_writeTsCycles(clkSnapshot, true);
                } else {
                    this->_writeTsWall(clkSnapshot, true);
                }
            });

            if (!_mOpts.writeInfoNames) {
                this->_appendToBuf("] ");
            }
        }

        /* Timestamp delta */
        if (_mOpts.writeTsDelta) {
            if (_mOpts.writeInfoNames) {
                this->_appendToBuf(", ");
                this->_writeInfoNameEqual("delta");
            } else {
                this->_appendToBuf("(");
            }

            if (_mOpts.clkFmt == ClkFormat::Cycles) {
                if (_mTsDelta) {
                    this->_appendFmtToBuf(FMT_COMPILE("+{:012}"), *_mTsDelta);
                } else {
                    /* NOT a trigraph */
                    this->_appendToBuf("+??????????\?\?");
                }
            } else {
                if (_mTsDelta) {
                    this->_appendFmtToBuf(FMT_COMPILE("+{}.{:09}"), *_mTsDelta / _nsPerS,
                                          *_mTsDelta % _nsPerS);
                } else {
                    this->_appendToBuf("+?.?????????");
                }
            }
            if (!_mOpts.writeInfoNames) {
                this->_appendToBuf(") ");
            }
        }

        _mWroteFirstItem = _mOpts.writeInfoNames;
    }

    /*
     * Writes the prefix for the next item of the event info "domain
     * chain" (hostname, domain name, process name, VPID, log level, EMF
     * URI), and sets `wroteDomItem` so subsequent items get the proper
     * inter-item separator.
     */
    void _writeEventInfoDomItem(bool& wroteDomItem, const bt2c::CStringView itemName)
    {
        if (_mOpts.writeInfoNames) {
            this->_appendItemSep();
            this->_writeInfoNameEqual(itemName);
        } else if (wroteDomItem) {
            this->_appendToBuf(":");
        }

        wroteDomItem = true;
    }

    /*
     * If `opt` is true and `trace` has an environment entry named
     * `envKey` holding a string, then this function writes it as the
     * event info domain item `itemName` using _writeEventInfoDomItem().
     */
    void _writeEventInfoStrEnvDomItem(bool& wroteDomItem, const bt2::ConstTrace trace,
                                      const bool opt, const bt2c::CStringView envKey,
                                      const bt2c::CStringView itemName)
    {
        if (!opt) {
            return;
        }

        if (const auto entry = trace.environmentEntry(envKey)) {
            this->_writeEventInfoDomItem(wroteDomItem, itemName);
            this->_appendToBuf(entry->asString().value());
        }
    }

    /*
     * Writes the event "info" header of `eventMsg` to the output buffer.
     */
    void _writeEventInfo(const bt2::ConstEventMessage eventMsg)
    {
        auto wroteDomItem = false;
        const auto event = eventMsg.event();
        const auto eventCls = event.cls();
        const auto trace = event.stream().trace();

        /* Timestamp and delta */
        this->_writeEventMsgTsAndDelta(eventMsg);

        /* Trace name */
        if (_mOpts.writeTraceName) {
            if (const bt2c::CStringView name = trace.name()) {
                if (_mOpts.writeInfoNames) {
                    this->_appendItemSep();
                    this->_writeInfoNameEqual("trace");
                }

                this->_appendToBuf(name);

                if (!_mOpts.writeInfoNames) {
                    this->_appendToBuf(" ");
                }
            }
        }

        /* Hostname (environment) */
        this->_writeEventInfoStrEnvDomItem(wroteDomItem, trace, _mOpts.writeTraceEnvHostname,
                                           "hostname", "trace:hostname");

        /* Domain name (environment) */
        this->_writeEventInfoStrEnvDomItem(wroteDomItem, trace, _mOpts.writeTraceEnvDomainName,
                                           "domain", "trace:domain");

        /* Process name (environment) */
        this->_writeEventInfoStrEnvDomItem(wroteDomItem, trace, _mOpts.writeTraceEnvProcname,
                                           "procname", "trace:procname");

        /* VPID (environment) */
        if (_mOpts.writeTraceEnvVpid) {
            if (const auto vpidVal = trace.environmentEntry("vpid")) {
                this->_writeEventInfoDomItem(wroteDomItem, "trace:vpid");
                this->_appendFmtToBuf(FMT_COMPILE("({})"), vpidVal->asSignedInteger().value());
            }
        }

        /* Event class log level */
        if (_mOpts.writeEventClsLogLevel) {
            if (const auto logLevel = eventCls.logLevel()) {
                const auto logLevelStr = std::invoke([&]() -> bt2c::CStringView {
                    switch (*logLevel) {
                    case bt2::EventClassLogLevel::Emergency:
                        return "TRACE_EMERG";

                    case bt2::EventClassLogLevel::Alert:
                        return "TRACE_ALERT";

                    case bt2::EventClassLogLevel::Critical:
                        return "TRACE_CRIT";

                    case bt2::EventClassLogLevel::Error:
                        return "TRACE_ERR";

                    case bt2::EventClassLogLevel::Warning:
                        return "TRACE_WARNING";

                    case bt2::EventClassLogLevel::Notice:
                        return "TRACE_NOTICE";

                    case bt2::EventClassLogLevel::Info:
                        return "TRACE_INFO";

                    case bt2::EventClassLogLevel::DebugSystem:
                        return "TRACE_DEBUG_SYSTEM";

                    case bt2::EventClassLogLevel::DebugProgram:
                        return "TRACE_DEBUG_PROGRAM";

                    case bt2::EventClassLogLevel::DebugProcess:
                        return "TRACE_DEBUG_PROCESS";

                    case bt2::EventClassLogLevel::DebugModule:
                        return "TRACE_DEBUG_MODULE";

                    case bt2::EventClassLogLevel::DebugUnit:
                        return "TRACE_DEBUG_UNIT";

                    case bt2::EventClassLogLevel::DebugFunction:
                        return "TRACE_DEBUG_FUNCTION";

                    case bt2::EventClassLogLevel::DebugLine:
                        return "TRACE_DEBUG_LINE";

                    case bt2::EventClassLogLevel::Debug:
                        return "TRACE_DEBUG";
                    }

                    bt_common_abort();
                });

                this->_writeEventInfoDomItem(wroteDomItem, "loglevel");
                this->_appendToBuf(logLevelStr);
                this->_appendFmtToBuf(FMT_COMPILE(" ({})"), static_cast<int>(*logLevel));
            }
        }

        /* Event class EMF URI */
        if (_mOpts.writeEventClsEmf) {
            if (const auto emfUri = eventCls.emfUri()) {
                this->_writeEventInfoDomItem(wroteDomItem, "model.emf.uri");
                this->_appendToBuf(emfUri);
            }
        }

        if (wroteDomItem && !_mOpts.writeInfoNames) {
            this->_appendToBuf(" ");
        }

        this->_appendItemSep();

        /* End of group */
        _mWroteFirstItem = false;

        /* Event class name */
        {
            if (_mOpts.writeInfoNames) {
                this->_writeInfoNameEqual("name");
            }

            const auto eventClsName = eventCls.name();

            this->_withColor(eventClsName ? _mTermCodes.eventName : _mTermCodes.unknown, [&] {
                if (eventClsName) {
                    this->_appendToBuf(eventClsName);
                } else {
                    this->_appendToBuf("<unknown>");
                }
            });
        }

        if (_mOpts.writeInfoNames) {
            this->_appendToBuf(", ");
        } else {
            this->_appendToBuf(": ");
        }
    }

    /*
     * Writes the numeric value of the integer field `field` to the
     * output buffer, using the preferred display base of the class
     * of `field`.
     */
    void _writeIntField(const bt2::ConstField field)
    {
        const auto fc = field.cls();
        const auto intFc = fc.asInteger();
        const bool isUnsigned = fc.isUnsignedInteger();
        const auto len = intFc.fieldValueRange();
        std::uint64_t u;
        std::int64_t s;

        if (isUnsigned) {
            u = field.asUnsignedInteger().value();
        } else {
            s = field.asSignedInteger().value();
            std::memcpy(&u, &s, sizeof(u));
        }

        this->_withColor(_mTermCodes.numberVal, [&] {
            const auto base = intFc.preferredDisplayBase();

            switch (base) {
            case bt2::DisplayBase::Binary:
                this->_appendFmtToBuf(FMT_COMPILE("{:#0{}b}"), _maskTo(u, len),
                                      static_cast<int>(len) + 2);
                break;

            case bt2::DisplayBase::Octal:
                BT_ASSERT_DBG(len != 0);

                /* Round length up to the nearest 3-bit boundary */
                this->_appendFmtToBuf(FMT_COMPILE("0{:o}"), _maskTo(u, ((len - 1) / 3 + 1) * 3));
                break;

            case bt2::DisplayBase::Decimal:
                if (isUnsigned) {
                    this->_appendFmtToBuf(FMT_COMPILE("{}"), u);
                } else {
                    this->_appendFmtToBuf(FMT_COMPILE("{}"), s);
                }

                break;

            case bt2::DisplayBase::Hexadecimal:
                /* Round length up to the nearest nibble */
                this->_appendFmtToBuf(FMT_COMPILE("0x{:X}"), _maskTo(u, (len + 3) & ~UINT64_C(3)));
                break;

            default:
                bt_common_abort();
            }
        });
    }

    /*
     * Writes `str` to the output buffer between double quotes, escaping
     * control characters and special characters along the way.
     */
    void _writeEscapedStr(const bt2c::CStringView str)
    {
        _mBuf.push_back('"');

        for (const auto ch : str) {
            /* Escape sequences not recognized by iscntrl(). */
            switch (ch) {
            case '\\':
                this->_appendToBuf("\\\\");
                continue;

            case '\'':
                this->_appendToBuf("\\\'");
                continue;

            case '\"':
                this->_appendToBuf("\\\"");
                continue;

            case '\?':
                this->_appendToBuf("\\\?");
                continue;
            }

            /* Standard characters */
            if (!std::iscntrl(static_cast<unsigned char>(ch))) {
                _mBuf.push_back(ch);
                continue;
            }

            switch (ch) {
            case '\a':
                this->_appendToBuf("\\a");
                break;

            case '\b':
                this->_appendToBuf("\\b");
                break;

            case '\e':
                this->_appendToBuf("\\e");
                break;

            case '\f':
                this->_appendToBuf("\\f");
                break;

            case '\n':
                this->_appendToBuf("\\n");
                break;

            case '\r':
                this->_appendToBuf("\\r");
                break;

            case '\t':
                this->_appendToBuf("\\t");
                break;

            case '\v':
                this->_appendToBuf("\\v");
                break;

            default:
                /* Unhandled control sequence: write as hex */
                this->_appendFmtToBuf(FMT_COMPILE("\\x{:02x}"), static_cast<unsigned char>(ch));
                break;
            }
        }

        _mBuf.push_back('"');
    }

    /*
     * Writes the unknown-label marker (`<unknown>`) to the
     * output buffer.
     */
    void _writeEnumFieldValLabelUnknown()
    {
        this->_withColor(_mTermCodes.unknown, [&] {
            this->_appendToBuf("<unknown>");
        });
    }

    /*
     * Writes `labelCount` labels from `labelArray` to the output buffer
     * in alphabetical order, separating multiple labels with `,` and
     * wrapping them between `{` and `}`.
     */
    template <typename LabelArrayT>
    void _writeEnumFieldValLabelArray(const std::uint64_t labelCount, const LabelArrayT labelArray)
    {
        _mSortedLabels.clear();
        _mSortedLabels.reserve(labelCount);

        for (std::uint64_t i = 0; i < labelCount; ++i) {
            _mSortedLabels.push_back(labelArray[i]);
        }

        this->_sortLabels();

        const auto wrap = _mSortedLabels.size() > 1;

        if (wrap) {
            this->_appendToBuf("{ ");
        }

        this->_writeSortedLabels();

        if (wrap) {
            this->_appendToBuf(" }");
        }
    }

    /*
     * Sorts `_mSortedLabels` alphabetically (using std::strcmp()).
     */
    void _sortLabels()
    {
        std::sort(_mSortedLabels.begin(), _mSortedLabels.end(),
                  [](const bt2c::CStringView a, const bt2c::CStringView b) noexcept {
                      return std::strcmp(a, b) < 0;
                  });
    }

    /*
     * Writes the contents of `_mSortedLabels` to the output buffer,
     * coloring each label with `_mTermCodes.enumMappingName` and
     * separating consecutive labels with `, `.
     */
    void _writeSortedLabels()
    {
        auto wroteFirst = false;

        for (const auto label : _mSortedLabels) {
            if (wroteFirst) {
                this->_appendToBuf(", ");
            }

            this->_withColor(_mTermCodes.enumMappingName, [&] {
                this->_writeEscapedStr(label);
            });

            wroteFirst = true;
        }
    }

    /*
     * Writes the per-bit labels that _tryWriteUEnumFieldBitFlags() or
     * _tryWriteSEnumFieldBitFlags() gathered into `_mEnumBitLabels` to
     * the output buffer, joining the bit groups with ` | `.
     */
    void _writeEnumValBitFlagLabelArrays()
    {
        auto wroteFirstLabel = false;

        for (const auto& bitLabels : _mEnumBitLabels) {
            if (const auto labelCount = bitLabels.size(); labelCount > 0) {
                if (wroteFirstLabel) {
                    this->_appendToBuf(" | ");
                }

                this->_writeEnumFieldValLabelArray(labelCount, bitLabels.data());
                wroteFirstLabel = true;
            }
        }
    }

    /*
     * Splits the unsigned enumeration field `field` into its set bits
     * and, when every set bit matches a label, writes them as ORed
     * bit flags.
     *
     * Falls back to writing the unknown label marker otherwise.
     */
    void _tryWriteUEnumFieldBitFlags(const bt2::ConstField field)
    {
        const auto val = field.asUnsignedInteger().value();

        if (val == 0) {
            /*
             * Value is 0: if there was a label for it, we'd know
             * by now.
             */
            this->_writeEnumFieldValLabelUnknown();
            return;
        }

        const auto fc = field.cls().asUnsignedEnumeration();

        for (std::uint64_t i = 0; i < _enumMaxBitFlagCount; ++i) {
            const std::uint64_t bitValue = UINT64_C(1) << i;

            if ((val & bitValue) != 0) {
                _getEnumLabelsForValue(fc, bitValue, _mEnumBitLabels[i]);

                if (_mEnumBitLabels[i].empty()) {
                    /*
                     * This bit has no matching label, therefore this
                     * field is not a bit flag field: write unknown
                     * and return.
                     */
                    this->_writeEnumFieldValLabelUnknown();
                    return;
                }
            }
        }

        this->_writeEnumValBitFlagLabelArrays();
    }

    /*
     * Splits the signed enumeration field `field` into its set bits
     * and, when every set bit matches a label, writes them as ORed
     * bit flags.
     *
     * Falls back to writing the unknown label marker otherwise.
     */
    void _tryWriteSEnumFieldBitFlags(const bt2::ConstField field)
    {
        const auto val = field.asSignedInteger().value();

        if (val <= 0) {
            /*
             * Negative value: not a bit flag enumeration field.
             *
             * For 0, if there was a value, we'd know by now.
             */
            this->_writeEnumFieldValLabelUnknown();
            return;
        }

        const auto fc = field.cls().asSignedEnumeration();

        for (std::uint64_t i = 0; i < _enumMaxBitFlagCount; ++i) {
            const std::uint64_t bitValue = UINT64_C(1) << i;

            if ((static_cast<std::uint64_t>(val) & bitValue) != 0) {
                _getEnumLabelsForValue(fc, static_cast<std::int64_t>(bitValue), _mEnumBitLabels[i]);

                if (_mEnumBitLabels[i].empty()) {
                    this->_writeEnumFieldValLabelUnknown();
                    return;
                }
            }
        }

        this->_writeEnumValBitFlagLabelArrays();
    }

    /*
     * Dispatches to either _tryWriteUEnumFieldBitFlags() or
     * _tryWriteSEnumFieldBitFlags() based on the signedness of the
     * enumeration field `field`.
     */
    void _tryWriteEnumFieldBitFlags(const bt2::ConstField field)
    {
        const auto fc = field.cls().asEnumeration();
        const auto intRange = fc.fieldValueRange();

        BT_ASSERT_DBG(intRange <= _enumMaxBitFlagCount);

        /* Remove all labels from the previous enumeration field */
        for (auto& v : _mEnumBitLabels) {
            v.clear();
        }

        /* Get the mapping labels for the bit value */
        switch (fc.type()) {
        case bt2::FieldClassType::UnsignedEnumeration:
            this->_tryWriteUEnumFieldBitFlags(field);
            break;

        case bt2::FieldClassType::SignedEnumeration:
            this->_tryWriteSEnumFieldBitFlags(field);
            break;

        default:
            bt_common_abort();
        }
    }

    /*
     * Writes the enumeration field `field` to the output buffer.
     */
    void _writeEnumField(const bt2::ConstField field)
    {
        const auto labels = std::invoke([field] {
            switch (field.cls().type()) {
            case bt2::FieldClassType::UnsignedEnumeration:
                return field.asUnsignedEnumeration().labels();

            case bt2::FieldClassType::SignedEnumeration:
                return field.asSignedEnumeration().mappingLabels();

            default:
                bt_common_abort();
            }
        });

        this->_appendToBuf("( ");

        if (labels.length() != 0) {
            /* The integral value matches some labels: write them */
            this->_writeEnumFieldValLabelArray(labels.length(), labels);
        } else if (_mOpts.writeEnumFieldFlags) {
            /*
             * The integral value of the enumeration field does _not_
             * match any label, but the `_mOpts.writeEnumFieldFlags`
             * option is true, so try to decompose the enumeration field
             * value into bits and write it as bit flags.
             */
            this->_tryWriteEnumFieldBitFlags(field);
        } else {
            this->_writeEnumFieldValLabelUnknown();
        }

        /* Write the integral value */
        this->_appendToBuf(" : container = ");
        this->_writeIntField(field);
        this->_appendToBuf(" )");
    }

    /*
     * Writes the bit array field `field` to the output buffer.
     */
    void _writeBitArrayField(const bt2::ConstField field)
    {
        const auto bitArrayField = field.asBitArray();

        this->_withColor(_mTermCodes.numberVal, [&] {
            this->_appendFmtToBuf(FMT_COMPILE("0x{:X}"), bitArrayField.valueAsInteger());
        });

        const auto labels = bitArrayField.activeFlagLabels();

        if (labels.length() > 0) {
            _mSortedLabels.clear();
            _mSortedLabels.reserve(labels.length());

            for (std::uint64_t i = 0; i < labels.length(); ++i) {
                _mSortedLabels.push_back(labels[i]);
            }

            this->_sortLabels();
            this->_appendToBuf(" { ");
            this->_writeSortedLabels();
            this->_appendToBuf(" }");
        }
    }

    /*
     * Writes the member at index `i` of `field` to the output buffer.
     */
    void _writeStructFieldMember(const bt2::ConstStructureField field, const std::uint64_t i,
                                 const bool writeNames, std::uint64_t& writtenFieldCount)
    {
        if (writtenFieldCount > 0) {
            this->_appendToBuf(", ");
        } else {
            this->_appendToBuf(" ");
        }

        if (writeNames) {
            this->_writeFieldNameEqual(field.cls()[i].name());
        }

        this->_writeField(field[i], writeNames);
        writtenFieldCount += 1;
    }

    /*
     * Writes the structure field `field` to the output buffer.
     */
    void _writeStructField(const bt2::ConstField field, const bool writeNames)
    {
        this->_appendToBuf("{");

        std::uint64_t nrWrittenFields = 0;
        const auto structField = field.asStructure();
        const auto nrFields = structField.length();

        for (std::uint64_t i = 0; i < nrFields; ++i) {
            this->_writeStructFieldMember(structField, i, writeNames, nrWrittenFields);
        }

        this->_appendToBuf(" }");
    }

    /*
     * Writes the element at index `i` of `field` to the output buffer.
     */
    void _writeArrayFieldElem(const bt2::ConstArrayField field, const std::uint64_t i,
                              const bool writeNames)
    {
        if (i != 0) {
            this->_appendToBuf(", ");
        } else {
            this->_appendToBuf(" ");
        }

        if (writeNames) {
            this->_appendFmtToBuf(FMT_COMPILE("[{}] = "), i);
        }

        this->_writeField(field[i], writeNames);
    }

    /*
     * Writes the array field `field` to the output buffer.
     */
    void _writeArrayField(const bt2::ConstField field, const bool writeNames)
    {
        this->_appendToBuf("[");

        const auto arrayField = field.asArray();
        const auto len = arrayField.length();

        for (std::uint64_t i = 0; i < len; ++i) {
            this->_writeArrayFieldElem(arrayField, i, writeNames);
        }

        this->_appendToBuf(" ]");
    }

    /*
     * Writes the option field `field` to the output buffer.
     */
    void _writeOptionField(const bt2::ConstField field, const bool writeNames)
    {
        if (const auto innerField = field.asOption().field()) {
            this->_appendToBuf("{ ");

            /*
             * TODO: If `writeNames`, then find selector name using field
             * path/location to print it here.
             */

            this->_writeField(*innerField, writeNames);
            this->_appendToBuf(" }");
        } else {
            this->_appendToBuf("<none>");
        }
    }

    /*
     * Writes the variant field `field` to the output buffer.
     */
    void _writeVariantField(const bt2::ConstField field, const bool writeNames)
    {
        const auto variantField = field.asVariant();

        this->_appendToBuf("{ ");

        /*
         * TODO: If `writeNames`, then find selector name using field
         * path/location to print it here.
         */

        this->_writeField(variantField.selectedOptionField(), writeNames);
        this->_appendToBuf(" }");
    }

    /*
     * Writes the BLOB field `field` to the output buffer.
     */
    void _writeBlobField(const bt2::ConstField field)
    {
        const auto data = field.asBlob().data();

        this->_appendToBuf("{ ");

        for (const auto b : data) {
            this->_appendFmtToBuf(FMT_COMPILE("{:02x} "), b);
        }

        this->_appendToBuf("}");
    }

    /*
     * Writes `field` to the output buffer, dispatching to the
     * type-specific writer based on its field class.
     */
    void _writeField(const bt2::ConstField field, const bool writeNames)
    {
        switch (field.cls().type()) {
        case bt2::FieldClassType::Bool:
            this->_withColor(_mTermCodes.numberVal, [&] {
                this->_appendToBuf(field.asBool().value() ? "true" : "false");
            });

            break;

        case bt2::FieldClassType::BitArray:
            this->_writeBitArrayField(field);
            break;

        case bt2::FieldClassType::UnsignedInteger:
        case bt2::FieldClassType::SignedInteger:
            this->_writeIntField(field);
            break;

        case bt2::FieldClassType::UnsignedEnumeration:
        case bt2::FieldClassType::SignedEnumeration:
            this->_writeEnumField(field);
            break;

        case bt2::FieldClassType::SinglePrecisionReal:
            this->_withColor(_mTermCodes.numberVal, [&] {
                this->_appendFmtToBuf(FMT_COMPILE("{:g}"), field.asSinglePrecisionReal().value());
            });

            break;

        case bt2::FieldClassType::DoublePrecisionReal:
            this->_withColor(_mTermCodes.numberVal, [&] {
                this->_appendFmtToBuf(FMT_COMPILE("{:g}"), field.asDoublePrecisionReal().value());
            });

            break;

        case bt2::FieldClassType::String:
            this->_withColor(_mTermCodes.strVal, [&] {
                this->_writeEscapedStr(field.asString().value());
            });

            break;

        case bt2::FieldClassType::Structure:
            this->_writeStructField(field, writeNames);
            break;

        case bt2::FieldClassType::StaticArray:
        case bt2::FieldClassType::DynamicArrayWithoutLengthField:
        case bt2::FieldClassType::DynamicArrayWithLengthField:
            this->_writeArrayField(field, writeNames);
            break;

        case bt2::FieldClassType::OptionWithoutSelectorField:
        case bt2::FieldClassType::OptionWithBoolSelectorField:
        case bt2::FieldClassType::OptionWithUnsignedIntegerSelectorField:
        case bt2::FieldClassType::OptionWithSignedIntegerSelectorField:
            this->_writeOptionField(field, writeNames);
            break;

        case bt2::FieldClassType::VariantWithoutSelectorField:
        case bt2::FieldClassType::VariantWithUnsignedIntegerSelectorField:
        case bt2::FieldClassType::VariantWithSignedIntegerSelectorField:
            this->_writeVariantField(field, writeNames);
            break;

        case bt2::FieldClassType::StaticBlob:
        case bt2::FieldClassType::DynamicBlobWithoutLengthField:
        case bt2::FieldClassType::DynamicBlobWithLengthField:
            this->_writeBlobField(field);
            break;

        default:
            bt_common_abort();
        }
    }

    /*
     * Writes the packet context field of `event`, if any, to the
     * output buffer.
     */
    void _writePktCtxField(const bt2::ConstEvent event)
    {
        const auto pkt = event.packet();

        if (!pkt) {
            return;
        }

        const auto field = pkt->contextField();

        if (!field) {
            return;
        }

        this->_appendItemSep();

        if (_mOpts.writeScopeNames) {
            this->_writeInfoNameEqual("stream.packet.context");
        }

        this->_writeField(*field, _mOpts.writeCtxFieldMemberNames);
    }

    /*
     * Writes the common event context field of `event`, if any, to the
     * output buffer.
     */
    void _writeCommonEventCtxField(const bt2::ConstEvent event)
    {
        const auto field = event.commonContextField();

        if (!field) {
            return;
        }

        this->_appendItemSep();

        if (_mOpts.writeScopeNames) {
            this->_writeInfoNameEqual("stream.event.context");
        }

        this->_writeField(*field, _mOpts.writeCtxFieldMemberNames);
    }

    /*
     * Writes the specific context field of `event`, if any, to the
     * output buffer.
     */
    void _writeSpecEventCtxField(const bt2::ConstEvent event)
    {
        const auto field = event.specificContextField();

        if (!field) {
            return;
        }

        this->_appendItemSep();

        if (_mOpts.writeScopeNames) {
            this->_writeInfoNameEqual("event.context");
        }

        this->_writeField(*field, _mOpts.writeCtxFieldMemberNames);
    }

    /*
     * Writes the payload field of `event`, if any, to the
     * output buffer.
     */
    void _writeEventPayloadField(const bt2::ConstEvent event)
    {
        const auto field = event.payloadField();

        if (!field) {
            return;
        }

        this->_appendItemSep();

        if (_mOpts.writeScopeNames) {
            this->_writeInfoNameEqual("event.fields");
        }

        this->_writeField(*field, _mOpts.writeEventPayloadFieldMemberNames);
    }

    /*
     * Writes the contents of the output buffer to `stream` and clears
     * the buffer.
     *
     * Throws `bt2c::Error` on write failure.
     */
    void _flushBuf(std::ostream& stream)
    {
        if (_mBuf.size() == 0) {
            return;
        }

        stream.write(_mBuf.data(), static_cast<std::streamsize>(_mBuf.size()));

        if (!stream) {
            BT_CPPLOGE_APPEND_CAUSE_AND_THROW(bt2c::Error, "Failed to write to output stream.");
        }
    }

    /*
     * Writes a single "tracer discarded N events/packets" warning line
     * to the standard error stream.
     */
    void _writeDiscardedItemsMsgDetails(
        const bt2::ConstStream stream,
        const bt2::OptionalBorrowedObject<bt2::ConstClockSnapshot> beginClkSnapshot,
        const bt2::OptionalBorrowedObject<bt2::ConstClockSnapshot> endClkSnapshot,
        const std::optional<std::uint64_t> count, const std::string_view elemType)
    {
        const auto trace = stream.trace();
        const auto streamName = _orUnknown(stream.name());
        const auto traceName = _orUnknown(trace.name());

        this->_withColor(_mTermCodes.warnTitle, [&] {
            this->_appendToBuf("WARNING");
        });

        this->_withColor(_mTermCodes.warn, [&] {
            this->_appendFmtToBuf(FMT_COMPILE(": {} "),
                                  count ? "Tracer discarded" : "Tracer may have discarded");

            if (count) {
                this->_appendFmtToBuf(FMT_COMPILE("{} {}{}"), *count, elemType,
                                      *count == 1 ? "" : "s");
            } else {
                this->_appendFmtToBuf(FMT_COMPILE("{}s"), elemType);
            }

            this->_appendToBuf(" ");

            if (beginClkSnapshot && endClkSnapshot) {
                const auto writeOneTs = [this](const bt2::ConstClockSnapshot cs) {
                    if (_mOpts.clkFmt == ClkFormat::Cycles) {
                        this->_writeTsCycles(cs, false);
                    } else {
                        this->_writeTsWall(cs, false);
                    }
                };

                this->_appendToBuf("between [");
                writeOneTs(*beginClkSnapshot);
                this->_appendToBuf("] and [");
                writeOneTs(*endClkSnapshot);
                this->_appendToBuf("]");
            } else {
                this->_appendToBuf("(unknown time range)");
            }

            this->_appendFmtToBuf(FMT_COMPILE(" in trace \"{}\" "), traceName.str());

            if (_mMipVersion == 0) {
                if (const auto uuid = trace.uuid()) {
                    this->_appendFmtToBuf(FMT_COMPILE("(UUID: {}) "), uuid->str());
                } else {
                    this->_appendToBuf("(no UUID) ");
                }
            } else {
                if (const auto uid = trace.uid()) {
                    this->_appendFmtToBuf(FMT_COMPILE("(UID: {}) "), uid.str());
                } else {
                    this->_appendToBuf("(no UID) ");
                }
            }

            this->_appendFmtToBuf(
                FMT_COMPILE("within stream \"{}\" (stream class ID: {}, stream ID: {})."),
                streamName.str(), stream.cls().id(), stream.id());
        });

        this->_appendToBuf("\n");

        /*
         * Write to standard error stream, and not as a standard log
         * line, to remain backward compatible with the
         * Babeltrace 1 format.
         */
        this->_flushBuf(std::cerr);
    }

    /*
     * Writes the full event message `eventMsg` to the output stream.
     */
    void _writeEventMsg(const bt2::ConstEventMessage eventMsg)
    {
        const auto event = eventMsg.event();

        this->_writeEventInfo(eventMsg);
        this->_writePktCtxField(event);
        this->_writeCommonEventCtxField(event);
        this->_writeSpecEventCtxField(event);
        this->_writeEventPayloadField(event);
        _mBuf.push_back('\n');
        this->_flushBuf(*_mOut);
    }

    /*
     * Extracts the stream, clock snapshots, and count from the
     * discarded events/packets message `msg`, and then calls
     * _writeDiscardedItemsMsgDetails() to write the warning.
     */
    void _writeDiscardedItemsMsg(const bt2::ConstMessage msg)
    {
        bt2::OptionalBorrowedObject<bt2::ConstStream> stream;
        bt2::OptionalBorrowedObject<bt2::ConstClockSnapshot> beginCs;
        bt2::OptionalBorrowedObject<bt2::ConstClockSnapshot> endCs;
        std::optional<std::uint64_t> count;
        std::string_view elemType;

        const auto extractVars = [&](const auto discMsg, const auto hasClkSnapshots,
                                     const auto name) {
            stream = discMsg.stream();
            count = discMsg.count();
            elemType = name;

            if (hasClkSnapshots) {
                beginCs = discMsg.beginningDefaultClockSnapshot();
                endCs = discMsg.endDefaultClockSnapshot();
            }
        };

        if (msg.isDiscardedEvents()) {
            const auto discMsg = msg.asDiscardedEvents();

            extractVars(discMsg, discMsg.stream().cls().discardedEventsHaveDefaultClockSnapshots(),
                        "event");
        } else {
            BT_ASSERT(msg.isDiscardedPackets());
            const auto discMsg = msg.asDiscardedPackets();

            extractVars(discMsg, discMsg.stream().cls().discardedPacketsHaveDefaultClockSnapshots(),
                        "packet");
        }

        BT_ASSERT(stream);
        this->_writeDiscardedItemsMsgDetails(*stream, beginCs, endCs, count, elemType);
    }

    /* Pretty-printing options */
    WriterOpts _mOpts;

    /* Output stream */
    std::ostream *_mOut;

    /* Effective MIP version to consider */
    std::uint64_t _mMipVersion;

    /* Logger */
    bt2c::Logger _mLogger;

    /*
     * Whether at least one top-level item has already been written in
     * the current group, meaning that a leading `, ` separator should
     * be emitted before the next one.
     *
     * Reset to false at the start of each message write.
     */
    bool _mWroteFirstItem = false;

    /* Output buffer; _flushBuf() empties this into `_mOut` */
    fmt::memory_buffer _mBuf;

    /*
     * Last timestamp, if any.
     *
     * Cycles when `_mOpts.clkFmt` is `ClkFormat::Cycles`, otherwise
     * nanoseconds from origin.
     */
    std::optional<std::uint64_t> _mLastTs;

    /* Delta between the last two timestamps, if any */
    std::optional<std::uint64_t> _mTsDelta;

    /*
     * Whether _writeTsWall() already emitted the one-shot
     * negative timestamp warning.
     */
    bool _mNegTsWarnDone = false;

    /*
     * For each bit of the integer backing the enumeration field value:
     * the labels for that bit.
     *
     * The vectors are allocated once during construction and reused
     * across enumeration fields to avoid
     * repeated allocation/deallocation.
     */
    std::array<std::vector<bt2c::CStringView>, _enumMaxBitFlagCount> _mEnumBitLabels;

    /*
     * Reusable, alphabetically ordered list of labels.
     *
     * Cleared and refilled before each label group is written; the
     * underlying buffer is retained across groups so subsequent fills
     * avoid reallocation.
     */
    std::vector<bt2c::CStringView> _mSortedLabels;

    /*
     * Terminal escape sequences used to colorize the output.
     *
     * When `EmitTermCodesV` is false, the entries are never read and
     * remain default-constructed (empty); the constructor skips their
     * initialization entirely.
     */
    struct
    {
        std::string infoName;
        std::string fieldName;
        std::string rst;
        std::string strVal;
        std::string numberVal;
        std::string enumMappingName;
        std::string unknown;
        std::string eventName;
        std::string ts;
        std::string warn;
        std::string warnTitle;
    } _mTermCodes;
};

} /* namespace bt2pretty */

#endif /* BABELTRACE_PLUGINS_TEXT_PRETTY_WRITER_HPP */
