/*
 * SPDX-FileCopyrightText: 2023 Philippe Proulx <pproulx@efficios.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2C_LOGGING_HPP
#define BABELTRACE_CPP_COMMON_BT2C_LOGGING_HPP

#include <cstring>
#include <iterator>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <fmt/core.h>
#include <glib.h>

#include <babeltrace2/babeltrace.h>

#include "common/assert.h"
#include "cpp-common/bt2/private-query-executor.hpp"
#include "cpp-common/bt2/self-component-class.hpp"
#include "cpp-common/bt2/self-component-port.hpp"
#include "cpp-common/bt2/self-message-iterator.hpp"
#include "cpp-common/vendor/wise-enum/wise_enum.h"
#include "logging/log-api.h"

#include "aliases.hpp"
#include "text-loc-str.hpp"

namespace bt2c {

/*
 * A logger contains an actor (self component class, self component,
 * self message iterator, or simple module name), a current logging
 * level, a logging tag, and a current text location string format.
 *
 * It offers:
 *
 * log():
 *     Logs a normal message.
 *
 * logMem():
 *     Logs a message with a hexadecimal view of memory bytes.
 *
 * logErrno():
 *     Logs a message with the error message corresponding to the
 *     current value of `errno`.
 *
 * logTextLoc():
 *     Logs a message with a text location using the current text
 *     location string format.
 *
 *     The initial text location string format is
 *     `TextLocStrFmt::LineColNosAndOffset`.
 *
 *     Change the default text location string format with
 *     textLocStrFmt().
 *
 * Some methods have their logError*AndThrow() and logError*AndThrow()
 * equivalents to append a cause to the error of the current thread
 * using the correct actor, and then throw or rethrow.
 *
 * The logging methods above expect a format string and zero or more
 * arguments to be formatted with fmt::format().
 *
 * Use the BT_CPPLOG*() macros to use `__FILE__`, `__func__`, `__LINE__`
 * as the file name, function name, and line number.
 */
class Logger final
{
public:
    /* clang-format off */

    /* Available log levels */
    WISE_ENUM_CLASS_MEMBER(Level,
        (Trace,     BT_LOG_TRACE),
        (Debug,     BT_LOG_DEBUG),
        (Info,      BT_LOG_INFO),
        (Warning,   BT_LOG_WARNING),
        (Error,     BT_LOG_ERROR),
        (Fatal,     BT_LOG_FATAL),
        (None,      BT_LOG_NONE)
    )

    /* clang-format on */

    /*
     * Builds a logger from the self component class `selfCompCls` using
     * the tag `tag` and the logging level `logLevel`.
     */
    explicit Logger(const bt2::SelfComponentClass selfCompCls, const bt2::LoggingLevel logLevel,
                    std::string tag) noexcept
        : _mActor {selfCompCls},
          _mLevel {static_cast<Level>(logLevel)},
          _mTag {std::move(tag)}
    {
    }

    /*
     * Builds a logger from the self component class `selfCompCls` using
     * the tag `tag` and the logging level of `privQueryExec`.
     */
    explicit Logger(const bt2::SelfComponentClass selfCompCls,
                    const bt2::PrivateQueryExecutor privQueryExec, std::string tag) noexcept
        : _mActor {selfCompCls},
          _mLevel {static_cast<Level>(privQueryExec.loggingLevel())},
          _mTag {std::move(tag)}
    {
    }

    /*
     * Builds a logger from the self component `selfComp` using the tag
     * `tag`.
     */
    explicit Logger(const bt2::SelfComponent selfComp, std::string tag) noexcept
        : _mActor {selfComp},
          _mLevel {static_cast<Level>(selfComp.loggingLevel())},
          _mTag {std::move(tag)}
    {
    }

    /*
     * Builds a logger from the self source component `selfComp` using
     * the tag `tag`.
     */
    explicit Logger(const bt2::SelfSourceComponent selfComp, std::string tag) noexcept
        : Logger {
              bt2::SelfComponent {bt_self_component_source_as_self_component(selfComp.libObjPtr())},
              std::move(tag)}
    {
    }

    /*
     * Builds a logger from the self filter component `selfComp` using
     * the tag `tag`.
     */
    explicit Logger(const bt2::SelfFilterComponent selfComp, std::string tag) noexcept
        : Logger {
              bt2::SelfComponent {bt_self_component_filter_as_self_component(selfComp.libObjPtr())},
              std::move(tag)}
    {
    }

    /*
     * Builds a logger from the self sink component `selfComp` using the
     * tag `tag`.
     */
    explicit Logger(const bt2::SelfSinkComponent selfComp, std::string tag) noexcept
        : Logger {
              bt2::SelfComponent {bt_self_component_sink_as_self_component(selfComp.libObjPtr())},
              std::move(tag)}
    {
    }

    /*
     * Builds a logger from the self message iterator `selfMsgIter`
     * using the tag `tag`.
     */
    explicit Logger(const bt2::SelfMessageIterator selfMsgIter, std::string tag) noexcept
        : _mActor {selfMsgIter},
          _mLevel {static_cast<Level>(selfMsgIter.component().loggingLevel())},
          _mTag {std::move(tag)}
    {
    }

    /*
     * Builds a logger from the module named `moduleName` using the tag
     * `tag` and logging level `logLevel`.
     */
    explicit Logger(std::string moduleName, std::string tag, const Level logLevel) noexcept
        : _mActor {std::move(moduleName)},
          _mLevel {logLevel},
          _mTag {std::move(tag)}
    {
    }

    /*
     * Builds a logger from another logger `other` using the new tag
     * `newTag`.
     */
    explicit Logger(const Logger& other, std::string newTag)
        : _mActor {other._mActor},
          _mLevel {other._mLevel},
          _mTag {std::move(newTag)},
          _mTextLocStrFmt {other._mTextLocStrFmt}
    {
    }

    /*
     * Current logging level.
     */
    Level level() const noexcept
    {
        return _mLevel;
    }

    /*
     * Whether or not this logger would log at the level `level`.
     */
    bool wouldLog(const Level level) const noexcept
    {
        return BT_LOG_ON_CUR_LVL(static_cast<int>(level), static_cast<int>(_mLevel));
    }

    /*
     * Whether or not this logger would log at the trace level.
     */
    bool wouldLogT() const noexcept
    {
        return this->wouldLog(Level::Trace);
    }

    /*
     * Whether or not this logger would log at the debug level.
     */
    bool wouldLogD() const noexcept
    {
        return this->wouldLog(Level::Debug);
    }

    /*
     * Whether or not this logger would log at the info level.
     */
    bool wouldLogI() const noexcept
    {
        return this->wouldLog(Level::Info);
    }

    /*
     * Whether or not this logger would log at the warning level.
     */
    bool wouldLogW() const noexcept
    {
        return this->wouldLog(Level::Warning);
    }

    /*
     * Whether or not this logger would log at the error level.
     */
    bool wouldLogE() const noexcept
    {
        return this->wouldLog(Level::Error);
    }

    /*
     * Whether or not this logger would log at the fatal level.
     */
    bool wouldLogF() const noexcept
    {
        return this->wouldLog(Level::Fatal);
    }

    /*
     * Logging tag.
     */
    const std::string& tag() const noexcept
    {
        return _mTag;
    }

    /*
     * Sets the text location string format to be used by logTextLoc(),
     * logErrorTextLocAndThrow(), and logErrorTextLocAndRethrow() to
     * `fmt`.
     */
    void textLocStrFmt(const TextLocStrFmt fmt) noexcept
    {
        _mTextLocStrFmt = fmt;
    }

    /*
     * Text location string format used by logTextLoc(),
     * logErrorTextLocAndThrow(), and logErrorTextLocAndRethrow().
     */
    TextLocStrFmt textLocStrFmt() const noexcept
    {
        return _mTextLocStrFmt;
    }

    void appendCauseStr(const char * const fileName, const int lineNo, const char * const initMsg,
                        const char * const msg) const noexcept
    {
        if (const auto selfMsgIter = std::get_if<bt2::SelfMessageIterator>(&_mActor)) {
            bt_current_thread_error_append_cause_from_message_iterator(
                selfMsgIter->libObjPtr(), fileName, lineNo, "%s%s", initMsg, msg);
        } else if (const auto selfComp = std::get_if<bt2::SelfComponent>(&_mActor)) {
            bt_current_thread_error_append_cause_from_component(selfComp->libObjPtr(), fileName,
                                                                lineNo, "%s%s", initMsg, msg);
        } else if (const auto selfCompCls = std::get_if<bt2::SelfComponentClass>(&_mActor)) {
            bt_current_thread_error_append_cause_from_component_class(
                selfCompCls->libObjPtr(), fileName, lineNo, "%s%s", initMsg, msg);
        } else {
            bt_current_thread_error_append_cause_from_unknown(
                std::get<std::string>(_mActor).c_str(), fileName, lineNo, "%s%s", initMsg, msg);
        }
    }

private:
    struct _StdLogWriter final
    {
        static void write(const char * const fileName, const char * const funcName,
                          const unsigned lineNo, const Level level, const char * const tag,
                          ConstBytes, const char * const initMsg, const char * const msg) noexcept
        {
            BT_ASSERT_DBG(initMsg && std::strcmp(initMsg, "") == 0);
            bt_log_write(fileName, funcName, lineNo, static_cast<bt_log_level>(level), tag, msg);
        }
    };

public:
    /*
     * Logs using the level `LevelV`.
     *
     * This method forwards `fmt` and `args` to fmt::format() to create
     * the log message.
     *
     * If `AppendCauseV` is true, this method also appends a cause to
     * the error of the current thread using the same message.
     */
    template <Level LevelV, bool AppendCauseV, typename... ArgTs>
    void log(const char * const fileName, const char * const funcName, const unsigned int lineNo,
             fmt::format_string<ArgTs...> fmt, ArgTs&&...args) const
    {
        this->_log<_StdLogWriter, LevelV, AppendCauseV>(
            fileName, funcName, lineNo, {}, "", std::move(fmt), std::forward<ArgTs>(args)...);
    }

    /*
     * Like log() with the `Level::Error` level, but also throws a
     * default-constructed instance of `ExcT`.
     */
    template <bool AppendCauseV, typename ExcT, typename... ArgTs>
    [[noreturn]] void logErrorAndThrow(const char * const fileName, const char * const funcName,
                                       const unsigned int lineNo, fmt::format_string<ArgTs...> fmt,
                                       ArgTs&&...args) const
    {
        this->log<Level::Error, AppendCauseV>(fileName, funcName, lineNo, std::move(fmt),
                                              std::forward<ArgTs>(args)...);
        throw ExcT {};
    }

    /*
     * Like log() with the `Level::Error` level, but also rethrows.
     */
    template <bool AppendCauseV, typename... ArgTs>
    [[noreturn]] void logErrorAndRethrow(const char * const fileName, const char * const funcName,
                                         const unsigned int lineNo,
                                         fmt::format_string<ArgTs...> fmt, ArgTs&&...args) const
    {
        this->log<Level::Error, AppendCauseV>(fileName, funcName, lineNo, std::move(fmt),
                                              std::forward<ArgTs>(args)...);
        throw;
    }

private:
    struct _InitMsgLogWriter final
    {
        static void write(const char * const fileName, const char * const funcName,
                          const unsigned lineNo, const Level level, const char * const tag,
                          ConstBytes, const char * const initMsg, const char * const msg) noexcept
        {
            bt_log_write_printf(funcName, fileName, lineNo, static_cast<bt_log_level>(level), tag,
                                "%s%s", initMsg, msg);
        }
    };

public:
    /*
     * Logs the message of `errno` using the level `LevelV`.
     *
     * The log message starts with `initMsg`, is followed with the
     * message for `errno`, and then with what fmt::format() creates
     * given `fmt` and `args`.
     *
     * If `AppendCauseV` is true, this method also appends a cause to
     * the error of the current thread using the same message.
     */
    template <Level LevelV, bool AppendCauseV, typename... ArgTs>
    void logErrno(const char * const fileName, const char * const funcName,
                  const unsigned int lineNo, const char * const initMsg,
                  fmt::format_string<ArgTs...> fmt, ArgTs&&...args) const
    {
        this->_log<_InitMsgLogWriter, LevelV, AppendCauseV>(
            fileName, funcName, lineNo, {}, this->_errnoIntroStr(initMsg).c_str(), std::move(fmt),
            std::forward<ArgTs>(args)...);
    }

    /*
     * Like logErrno() with the `Level::Error` level, but also throws a
     * default-constructed instance of `ExcT`.
     */
    template <bool AppendCauseV, typename ExcT, typename... ArgTs>
    [[noreturn]] void logErrorErrnoAndThrow(const char * const fileName,
                                            const char * const funcName, const unsigned int lineNo,
                                            const char * const initMsg,
                                            fmt::format_string<ArgTs...> fmt, ArgTs&&...args) const
    {
        this->logErrno<Level::Error, AppendCauseV>(fileName, funcName, lineNo, initMsg,
                                                   std::move(fmt), std::forward<ArgTs>(args)...);
        throw ExcT {};
    }

    /*
     * Like logErrno() with the `Level::Error` level, but also rethrows.
     */
    template <bool AppendCauseV, typename... ArgTs>
    [[noreturn]] void
    logErrorErrnoAndRethrow(const char * const fileName, const char * const funcName,
                            const unsigned int lineNo, const char * const initMsg,
                            fmt::format_string<ArgTs...> fmt, ArgTs&&...args) const
    {
        this->logErrno<Level::Error, AppendCauseV>(fileName, funcName, lineNo, initMsg,
                                                   std::move(fmt), std::forward<ArgTs>(args)...);
        throw;
    }

    /*
     * Logs the text location of `textLoc` followed with a message using
     * the level `LevelV`.
     *
     * The log message starts with the formatted text location and is
     * followed with what fmt::format() creates given `fmt` and `args`.
     *
     * This method uses the current text location string format
     * (see textLocStrFmt()) to format `textLoc`.
     *
     * If `AppendCauseV` is true, this method also appends a cause to
     * the error of the current thread using the same message.
     */
    template <Level LevelV, bool AppendCauseV, typename... ArgTs>
    void logTextLoc(const char * const fileName, const char * const funcName,
                    const unsigned int lineNo, const TextLoc& textLoc,
                    fmt::format_string<ArgTs...> fmt, ArgTs&&...args) const
    {
        this->_log<_InitMsgLogWriter, LevelV, AppendCauseV>(
            fileName, funcName, lineNo, {}, this->_textLocPrefixStr(textLoc).c_str(), fmt,
            std::forward<ArgTs>(args)...);
    }

    /*
     * Like logTextLoc() with the `Level::Error` level, but also throws
     * a default-constructed instance of `ExcT`.
     */
    template <bool AppendCauseV, typename ExcT, typename... ArgTs>
    [[noreturn]] void
    logErrorTextLocAndThrow(const char * const fileName, const char * const funcName,
                            const unsigned int lineNo, const TextLoc& textLoc,
                            fmt::format_string<ArgTs...> fmt, ArgTs&&...args) const
    {
        this->logTextLoc<Level::Error, AppendCauseV>(fileName, funcName, lineNo, textLoc, fmt,
                                                     std::forward<ArgTs>(args)...);
        throw ExcT {};
    }

    /*
     * Like logTextLoc() with the `Level::Error` level, but also
     * rethrows.
     */
    template <bool AppendCauseV, typename... ArgTs>
    [[noreturn]] void
    logErrorTextLocAndRethrow(const char * const fileName, const char * const funcName,
                              const unsigned int lineNo, const TextLoc& textLoc,
                              fmt::format_string<ArgTs...> fmt, ArgTs&&...args) const
    {
        this->logTextLoc<Level::Error, AppendCauseV>(fileName, funcName, lineNo, textLoc, fmt,
                                                     std::forward<ArgTs>(args)...);
        throw;
    }

private:
    struct _MemLogWriter final
    {
        static void write(const char * const fileName, const char * const funcName,
                          const unsigned lineNo, const Level level, const char * const tag,
                          const ConstBytes memData, const char *, const char * const msg) noexcept
        {
            bt_log_write_mem(funcName, fileName, lineNo, static_cast<bt_log_level>(level), tag,
                             memData.data(), memData.size(), msg);
        }
    };

public:
    /*
     * Logs memory data using the level `LevelV`.
     *
     * This method forwards `fmt` and `args` to fmt::format() to create
     * the log message.
     */
    template <Level LevelV, typename... ArgTs>
    void logMem(const char * const fileName, const char * const funcName, const unsigned int lineNo,
                const ConstBytes memData, fmt::format_string<ArgTs...> fmt, ArgTs&&...args) const
    {
        this->_log<_MemLogWriter, LevelV, false>(fileName, funcName, lineNo, memData, "",
                                                 std::move(fmt), std::forward<ArgTs>(args)...);
    }

private:
    /*
     * Formats a log message with fmt::format() given `fmt` and `args`,
     * and then:
     *
     * 1. Calls LogWriterT::write() with its arguments to log using the
     *    level `LevelV`.
     *
     * 2. If `AppendCauseV` is true, this method also appends a cause to
     *    the error of the current thread using the concatenation of
     *    `initMsg` and `msg` as the message.
     */
    template <typename LogWriterT, Level LevelV, bool AppendCauseV, typename... ArgTs>
    void _log(const char * const fileName, const char * const funcName, const unsigned int lineNo,
              const ConstBytes memData, const char * const initMsg,
              fmt::format_string<ArgTs...> fmt, ArgTs&&...args) const
    {
        const auto wouldLog = this->wouldLog(LevelV);

        /* Only format arguments if logging or appending an error cause */
        if (G_UNLIKELY(wouldLog || AppendCauseV)) {
            /*
             * Format arguments to our buffer (fmt::format_to() doesn't
             * append a null character).
             */
            _mBuf.clear();
            fmt::format_to(std::back_inserter(_mBuf), std::move(fmt), std::forward<ArgTs>(args)...);
            _mBuf.push_back('\0');
        }

        /* Initial message is required */
        BT_ASSERT(initMsg);

        /* Log if needed */
        if (wouldLog) {
            LogWriterT::write(fileName, funcName, lineNo, LevelV, _mTag.data(), memData, initMsg,
                              _mBuf.data());
        }

        /* Append an error cause if needed */
        if (AppendCauseV) {
            this->appendCauseStr(fileName, lineNo, initMsg, _mBuf.data());
        }
    }

    static std::string _errnoIntroStr(const char * const initMsg)
    {
        BT_ASSERT(errno != 0);
        return fmt::format("{}: {}", initMsg, g_strerror(errno));
    }

    std::string _textLocPrefixStr(const TextLoc& loc) const
    {
        return fmt::format("[{}] ", textLocStr(loc, _mTextLocStrFmt));
    }

    /* Actor (`std::string`: module name) */
    std::variant<bt2::SelfComponentClass, bt2::SelfComponent, bt2::SelfMessageIterator, std::string>
        _mActor;

    /* Current logging level */
    Level _mLevel;

    /* Logging tag */
    std::string _mTag;

    /* Current text location string format */
    TextLocStrFmt _mTextLocStrFmt = TextLocStrFmt::LineColNosAndOffset;

    /* Formatting buffer */
    mutable std::vector<char> _mBuf;
};

/*
 * Returns `s` if it's not `nullptr`, or the `(null)` string otherwise.
 */
inline const char *maybeNull(const char * const s) noexcept
{
    return s ? s : "(null)";
}

} /* namespace bt2c */

/* Internal: default logger name */
#define _BT_CPPLOG_DEF_LOGGER _mLogger

/*
 * Calls log() on `_logger` to log using the level `_lvl`.
 */
#define BT_CPPLOG_EX(_lvl, _logger, _fmt, ...)                                                     \
    do {                                                                                           \
        if (G_UNLIKELY((_logger).wouldLog(_lvl))) {                                                \
            (_logger).template log<(_lvl), false>(__FILE__, __func__, __LINE__, (_fmt),            \
                                                  ##__VA_ARGS__);                                  \
        }                                                                                          \
    } while (0)

/*
 * BT_CPPLOG_EX() with specific logging levels.
 */
#define BT_CPPLOGT_SPEC(_logger, _fmt, ...)                                                        \
    BT_CPPLOG_EX(bt2c::Logger::Level::Trace, (_logger), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGD_SPEC(_logger, _fmt, ...)                                                        \
    BT_CPPLOG_EX(bt2c::Logger::Level::Debug, (_logger), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGI_SPEC(_logger, _fmt, ...)                                                        \
    BT_CPPLOG_EX(bt2c::Logger::Level::Info, (_logger), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGW_SPEC(_logger, _fmt, ...)                                                        \
    BT_CPPLOG_EX(bt2c::Logger::Level::Warning, (_logger), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGE_SPEC(_logger, _fmt, ...)                                                        \
    BT_CPPLOG_EX(bt2c::Logger::Level::Error, (_logger), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGF_SPEC(_logger, _fmt, ...)                                                        \
    BT_CPPLOG_EX(bt2c::Logger::Level::Fatal, (_logger), (_fmt), ##__VA_ARGS__)

/*
 * BT_CPPLOG_EX() with specific logging levels and using the default
 * logger.
 */
#define BT_CPPLOGT(_fmt, ...) BT_CPPLOGT_SPEC(_BT_CPPLOG_DEF_LOGGER, (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGD(_fmt, ...) BT_CPPLOGD_SPEC(_BT_CPPLOG_DEF_LOGGER, (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGI(_fmt, ...) BT_CPPLOGI_SPEC(_BT_CPPLOG_DEF_LOGGER, (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGW(_fmt, ...) BT_CPPLOGW_SPEC(_BT_CPPLOG_DEF_LOGGER, (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGE(_fmt, ...) BT_CPPLOGE_SPEC(_BT_CPPLOG_DEF_LOGGER, (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGF(_fmt, ...) BT_CPPLOGF_SPEC(_BT_CPPLOG_DEF_LOGGER, (_fmt), ##__VA_ARGS__)

/*
 * Calls logMem() on `_logger` to log using the level `_lvl`.
 */
#define BT_CPPLOG_MEM_EX(_lvl, _logger, _memData, _fmt, ...)                                       \
    do {                                                                                           \
        if (G_UNLIKELY((_logger).wouldLog(_lvl))) {                                                \
            (_logger).template logMem<(_lvl)>(__FILE__, __func__, __LINE__, (_memData), (_fmt),    \
                                              ##__VA_ARGS__);                                      \
        }                                                                                          \
    } while (0)

/*
 * BT_CPPLOG_MEM_EX() with specific logging levels.
 */
#define BT_CPPLOGT_MEM_SPEC(_logger, _memData, _fmt, ...)                                          \
    BT_CPPLOG_MEM_EX(bt2c::Logger::Level::Trace, (_logger), (_memData), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGD_MEM_SPEC(_logger, _memData, _fmt, ...)                                          \
    BT_CPPLOG_MEM_EX(bt2c::Logger::Level::Debug, (_logger), (_memData), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGI_MEM_SPEC(_logger, _memData, _fmt, ...)                                          \
    BT_CPPLOG_MEM_EX(bt2c::Logger::Level::Info, (_logger), (_memData), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGW_MEM_SPEC(_logger, _memData, _fmt, ...)                                          \
    BT_CPPLOG_MEM_EX(bt2c::Logger::Level::Warning, (_logger), (_memData), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGE_MEM_SPEC(_logger, _memData, _fmt, ...)                                          \
    BT_CPPLOG_MEM_EX(bt2c::Logger::Level::Error, (_logger), (_memData), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGF_MEM_SPEC(_logger, _memData, _fmt, ...)                                          \
    BT_CPPLOG_MEM_EX(bt2c::Logger::Level::Fatal, (_logger), (_memData), (_fmt), ##__VA_ARGS__)

/*
 * BT_CPPLOG_MEM_EX() with specific logging levels and using the default
 * logger.
 */
#define BT_CPPLOGT_MEM(_memData, _fmt, ...)                                                        \
    BT_CPPLOGT_MEM_SPEC(_BT_CPPLOG_DEF_LOGGER, (_memData), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGD_MEM(_memData, _fmt, ...)                                                        \
    BT_CPPLOGD_MEM_SPEC(_BT_CPPLOG_DEF_LOGGER, (_memData), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGI_MEM(_memData, _fmt, ...)                                                        \
    BT_CPPLOGI_MEM_SPEC(_BT_CPPLOG_DEF_LOGGER, (_memData), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGW_MEM(_memData, _fmt, ...)                                                        \
    BT_CPPLOGW_MEM_SPEC(_BT_CPPLOG_DEF_LOGGER, (_memData), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGE_MEM(_memData, _fmt, ...)                                                        \
    BT_CPPLOGE_MEM_SPEC(_BT_CPPLOG_DEF_LOGGER, (_memData), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGF_MEM(_memData, _fmt, ...)                                                        \
    BT_CPPLOGF_MEM_SPEC(_BT_CPPLOG_DEF_LOGGER, (_memData), (_fmt), ##__VA_ARGS__)

/*
 * Calls logErrno() on `_logger` to log using the level `_lvl` and
 * initial message `_initMsg`.
 */
#define BT_CPPLOG_ERRNO_EX(_lvl, _logger, _initMsg, _fmt, ...)                                     \
    do {                                                                                           \
        if (G_UNLIKELY((_logger).wouldLog(_lvl))) {                                                \
            (_logger).template logErrno<(_lvl), false>(__FILE__, __func__, __LINE__, (_initMsg),   \
                                                       (_fmt), ##__VA_ARGS__);                     \
        }                                                                                          \
    } while (0)

/*
 * BT_CPPLOG_ERRNO_EX() with specific logging levels.
 */
#define BT_CPPLOGT_ERRNO_SPEC(_logger, _initMsg, _fmt, ...)                                        \
    BT_CPPLOG_ERRNO_EX(bt2c::Logger::Level::Trace, (_logger), (_initMsg), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGD_ERRNO_SPEC(_logger, _initMsg, _fmt, ...)                                        \
    BT_CPPLOG_ERRNO_EX(bt2c::Logger::Level::Debug, (_logger), (_initMsg), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGI_ERRNO_SPEC(_logger, _initMsg, _fmt, ...)                                        \
    BT_CPPLOG_ERRNO_EX(bt2c::Logger::Level::Info, (_logger), (_initMsg), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGW_ERRNO_SPEC(_logger, _initMsg, _fmt, ...)                                        \
    BT_CPPLOG_ERRNO_EX(bt2c::Logger::Level::Warning, (_logger), (_initMsg), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGE_ERRNO_SPEC(_logger, _initMsg, _fmt, ...)                                        \
    BT_CPPLOG_ERRNO_EX(bt2c::Logger::Level::Error, (_logger), (_initMsg), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGF_ERRNO_SPEC(_logger, _initMsg, _fmt, ...)                                        \
    BT_CPPLOG_ERRNO_EX(bt2c::Logger::Level::Fatal, (_logger), (_initMsg), (_fmt), ##__VA_ARGS__)

/*
 * BT_CPPLOG_ERRNO_EX() with specific logging levels and using the
 * default logger.
 */
#define BT_CPPLOGT_ERRNO(_initMsg, _fmt, ...)                                                      \
    BT_CPPLOGT_ERRNO_SPEC(_BT_CPPLOG_DEF_LOGGER, (_initMsg), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGD_ERRNO(_initMsg, _fmt, ...)                                                      \
    BT_CPPLOGD_ERRNO_SPEC(_BT_CPPLOG_DEF_LOGGER, (_initMsg), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGI_ERRNO(_initMsg, _fmt, ...)                                                      \
    BT_CPPLOGI_ERRNO_SPEC(_BT_CPPLOG_DEF_LOGGER, (_initMsg), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGW_ERRNO(_initMsg, _fmt, ...)                                                      \
    BT_CPPLOGW_ERRNO_SPEC(_BT_CPPLOG_DEF_LOGGER, (_initMsg), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGE_ERRNO(_initMsg, _fmt, ...)                                                      \
    BT_CPPLOGE_ERRNO_SPEC(_BT_CPPLOG_DEF_LOGGER, (_initMsg), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGF_ERRNO(_initMsg, _fmt, ...)                                                      \
    BT_CPPLOGF_ERRNO_SPEC(_BT_CPPLOG_DEF_LOGGER, (_initMsg), (_fmt), ##__VA_ARGS__)

/*
 * Calls logTextLoc() on `_logger` to log using the level `_lvl` and
 * text location `_textLoc`.
 */
#define BT_CPPLOG_TEXT_LOC_EX(_lvl, _logger, _textLoc, _fmt, ...)                                  \
    do {                                                                                           \
        if (G_UNLIKELY((_logger).wouldLog(_lvl))) {                                                \
            (_logger).template logTextLoc<(_lvl), false>(__FILE__, __func__, __LINE__, (_textLoc), \
                                                         (_fmt), ##__VA_ARGS__);                   \
        }                                                                                          \
    } while (0)

/*
 * BT_CPPLOG_TEXT_LOC_EX() with specific logging levels.
 */
#define BT_CPPLOGT_TEXT_LOC_SPEC(_logger, _textLoc, _fmt, ...)                                     \
    BT_CPPLOG_TEXT_LOC_EX(bt2c::Logger::Level::Trace, (_logger), (_textLoc), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGD_TEXT_LOC_SPEC(_logger, _textLoc, _fmt, ...)                                     \
    BT_CPPLOG_TEXT_LOC_EX(bt2c::Logger::Level::Debug, (_logger), (_textLoc), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGI_TEXT_LOC_SPEC(_logger, _textLoc, _fmt, ...)                                     \
    BT_CPPLOG_TEXT_LOC_EX(bt2c::Logger::Level::Info, (_logger), (_textLoc), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGW_TEXT_LOC_SPEC(_logger, _textLoc, _fmt, ...)                                     \
    BT_CPPLOG_TEXT_LOC_EX(bt2c::Logger::Level::Warning, (_logger), (_textLoc), (_fmt),             \
                          ##__VA_ARGS__)
#define BT_CPPLOGE_TEXT_LOC_SPEC(_logger, _textLoc, _fmt, ...)                                     \
    BT_CPPLOG_TEXT_LOC_EX(bt2c::Logger::Level::Error, (_logger), (_textLoc), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGF_TEXT_LOC_SPEC(_logger, _textLoc, _fmt, ...)                                     \
    BT_CPPLOG_TEXT_LOC_EX(bt2c::Logger::Level::Fatal, (_logger), (_textLoc), (_fmt), ##__VA_ARGS__)

/*
 * BT_CPPLOG_TEXT_LOC_EX() with specific logging levels and using the
 * default logger.
 */
#define BT_CPPLOGT_TEXT_LOC(_textLoc, _fmt, ...)                                                   \
    BT_CPPLOGT_TEXT_LOC_SPEC(_BT_CPPLOG_DEF_LOGGER, (_textLoc), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGD_TEXT_LOC(_textLoc, _fmt, ...)                                                   \
    BT_CPPLOGD_TEXT_LOC_SPEC(_BT_CPPLOG_DEF_LOGGER, (_textLoc), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGI_TEXT_LOC(_textLoc, _fmt, ...)                                                   \
    BT_CPPLOGI_TEXT_LOC_SPEC(_BT_CPPLOG_DEF_LOGGER, (_textLoc), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGW_TEXT_LOC(_textLoc, _fmt, ...)                                                   \
    BT_CPPLOGW_TEXT_LOC_SPEC(_BT_CPPLOG_DEF_LOGGER, (_textLoc), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGE_TEXT_LOC(_textLoc, _fmt, ...)                                                   \
    BT_CPPLOGE_TEXT_LOC_SPEC(_BT_CPPLOG_DEF_LOGGER, (_textLoc), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGF_TEXT_LOC(_textLoc, _fmt, ...)                                                   \
    BT_CPPLOGF_TEXT_LOC_SPEC(_BT_CPPLOG_DEF_LOGGER, (_textLoc), (_fmt), ##__VA_ARGS__)

/*
 * Calls log() on `_logger` with the `Error` level to log an error and
 * append a cause to the error of the current thread.
 */
#define BT_CPPLOGE_APPEND_CAUSE_SPEC(_logger, _fmt, ...)                                           \
    (_logger).template log<bt2c::Logger::Level::Error, true>(__FILE__, __func__, __LINE__, (_fmt), \
                                                             ##__VA_ARGS__)

/*
 * BT_CPPLOGE_APPEND_CAUSE_SPEC() using the default logger.
 */
#define BT_CPPLOGE_APPEND_CAUSE(_fmt, ...)                                                         \
    BT_CPPLOGE_APPEND_CAUSE_SPEC(_BT_CPPLOG_DEF_LOGGER, (_fmt), ##__VA_ARGS__)

/*
 * Calls logErrorAndThrow() on `_logger` to log an error, append a cause
 * to the error of the current thread, and throw an instance of
 * `_excCls`.
 */
#define BT_CPPLOGE_APPEND_CAUSE_AND_THROW_SPEC(_logger, _excCls, _fmt, ...)                        \
    (_logger).template logErrorAndThrow<true, _excCls>(__FILE__, __func__, __LINE__, (_fmt),       \
                                                       ##__VA_ARGS__)

/*
 * BT_CPPLOGE_APPEND_CAUSE_AND_THROW_SPEC() using the default logger.
 */
#define BT_CPPLOGE_APPEND_CAUSE_AND_THROW(_excCls, _fmt, ...)                                      \
    BT_CPPLOGE_APPEND_CAUSE_AND_THROW_SPEC(_BT_CPPLOG_DEF_LOGGER, _excCls, (_fmt), ##__VA_ARGS__)

/*
 * Calls logErrorAndRethrow() on `_logger` to log an error, append a
 * cause to the error of the current thread, and throw an instance of
 * `_excCls`.
 */
#define BT_CPPLOGE_APPEND_CAUSE_AND_RETHROW_SPEC(_logger, _fmt, ...)                               \
    (_logger).template logErrorAndRethrow<true>(__FILE__, __func__, __LINE__, (_fmt), ##__VA_ARGS__)

/*
 * BT_CPPLOGE_APPEND_CAUSE_AND_RETHROW_SPEC() using the default logger.
 */
#define BT_CPPLOGE_APPEND_CAUSE_AND_RETHROW(_fmt, ...)                                             \
    BT_CPPLOGE_APPEND_CAUSE_AND_RETHROW_SPEC(_BT_CPPLOG_DEF_LOGGER, (_fmt), ##__VA_ARGS__)

/*
 * Calls logErrno() on `_logger` with the `Level::Error` level to log an
 * error and append a cause to the error of the current thread.
 */
#define BT_CPPLOGE_ERRNO_APPEND_CAUSE_SPEC(_logger, _initMsg, _fmt, ...)                           \
    (_logger).template logErrno<bt2c::Logger::Level::Error, true>(                                 \
        __FILE__, __func__, __LINE__, (_initMsg), (_fmt), ##__VA_ARGS__)

/*
 * BT_CPPLOGE_ERRNO_APPEND_CAUSE_SPEC() using the default logger.
 */
#define BT_CPPLOGE_ERRNO_APPEND_CAUSE(_initMsg, _fmt, ...)                                         \
    BT_CPPLOGE_ERRNO_APPEND_CAUSE_SPEC(_BT_CPPLOG_DEF_LOGGER, (_initMsg), (_fmt), ##__VA_ARGS__)

/*
 * Calls logErrorErrnoAndThrow() on `_logger` to log an error, append a
 * cause to the error of the current thread, and throw an instance of
 * `_excCls`.
 */
#define BT_CPPLOGE_ERRNO_APPEND_CAUSE_AND_THROW_SPEC(_logger, _excCls, _initMsg, _fmt, ...)        \
    (_logger).template logErrorErrnoAndThrow<true, _excCls>(__FILE__, __func__, __LINE__,          \
                                                            (_initMsg), (_fmt), ##__VA_ARGS__)

/*
 * BT_CPPLOGE_ERRNO_APPEND_CAUSE_AND_THROW_SPEC() using the default
 * logger.
 */
#define BT_CPPLOGE_ERRNO_APPEND_CAUSE_AND_THROW(_excCls, _initMsg, _fmt, ...)                      \
    BT_CPPLOGE_ERRNO_APPEND_CAUSE_AND_THROW_SPEC(_BT_CPPLOG_DEF_LOGGER, _excCls, (_initMsg),       \
                                                 (_fmt), ##__VA_ARGS__)

/*
 * Calls logErrorErrnoAndRethrow() on `_logger` to log an error, append
 * a cause to the error of the current thread, and throw an instance of
 * `_excCls`.
 */
#define BT_CPPLOGE_ERRNO_APPEND_CAUSE_AND_RETHROW_SPEC(_logger, _initMsg, _fmt, ...)               \
    (_logger).template logErrorErrnoAndRethrow<true>(__FILE__, __func__, __LINE__, (_initMsg),     \
                                                     (_fmt), ##__VA_ARGS__)

/*
 * BT_CPPLOGE_ERRNO_APPEND_CAUSE_AND_RETHROW_SPEC() using the default
 * logger.
 */
#define BT_CPPLOGE_ERRNO_APPEND_CAUSE_AND_RETHROW(_initMsg, _fmt, ...)                             \
    BT_CPPLOGE_ERRNO_APPEND_CAUSE_AND_RETHROW_SPEC(_BT_CPPLOG_DEF_LOGGER, (_initMsg), (_fmt),      \
                                                   ##__VA_ARGS__)

/*
 * Calls logTextLoc() on `_logger` with the `Level::Error` level to log
 * an error and append a cause to the error of the current thread.
 */
#define BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_SPEC(_logger, _textLoc, _fmt, ...)                        \
    (_logger).template logTextLoc<bt2c::Logger::Level::Error, true>(                               \
        __FILE__, __func__, __LINE__, (_textLoc), (_fmt), ##__VA_ARGS__)

/*
 * BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_SPEC() using the default logger.
 */
#define BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE(_textLoc, _fmt, ...)                                      \
    BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_SPEC(_BT_CPPLOG_DEF_LOGGER, (_textLoc), (_fmt), ##__VA_ARGS__)

/*
 * Calls logErrorErrnoAndThrow() on `_logger` to log an error, append a
 * cause to the error of the current thread, and throw an instance of
 * `_excCls`.
 */
#define BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_THROW_SPEC(_logger, _excCls, _textLoc, _fmt, ...)     \
    (_logger).template logErrorTextLocAndThrow<true, _excCls>(__FILE__, __func__, __LINE__,        \
                                                              (_textLoc), (_fmt), ##__VA_ARGS__)

/*
 * BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_THROW_SPEC() using the default
 * logger.
 */
#define BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_THROW(_excCls, _textLoc, _fmt, ...)                   \
    BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_THROW_SPEC(_BT_CPPLOG_DEF_LOGGER, _excCls, (_textLoc),    \
                                                    (_fmt), ##__VA_ARGS__)

/*
 * Calls logErrorErrnoAndRethrow() on `_logger` to log an error, append
 * a cause to the error of the current thread, and throw an instance of
 * `_excCls`.
 */
#define BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(_logger, _textLoc, _fmt, ...)            \
    (_logger).template logErrorTextLocAndRethrow<true>(__FILE__, __func__, __LINE__, (_textLoc),   \
                                                       (_fmt), ##__VA_ARGS__)

/*
 * BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC() using the default
 * logger.
 */
#define BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW(_textLoc, _fmt, ...)                          \
    BT_CPPLOGE_TEXT_LOC_APPEND_CAUSE_AND_RETHROW_SPEC(_BT_CPPLOG_DEF_LOGGER, (_textLoc), (_fmt),   \
                                                      ##__VA_ARGS__)

#endif /* BABELTRACE_CPP_COMMON_BT2C_LOGGING_HPP */
