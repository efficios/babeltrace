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
#include <vector>

#include <glib.h>

#include <babeltrace2/babeltrace.h>

#include "common/assert.h"
#include "cpp-common/bt2/private-query-executor.hpp"
#include "cpp-common/bt2/self-component-class.hpp"
#include "cpp-common/bt2/self-component-port.hpp"
#include "cpp-common/bt2/self-message-iterator.hpp"
#include "cpp-common/bt2s/optional.hpp"
#include "cpp-common/vendor/fmt/core.h"
#include "logging/log-api.h"

namespace bt2c {

/*
 * A logger contains an actor (self component class, self component,
 * self message iterator, or simple module name), a current logging
 * level, and a logging tag.
 *
 * It offers the logNoThrow(), logMemNoThrow(), logErrnoNoThrow(),
 * logErrorAndThrow(), logErrorAndRethrow(), logErrorErrnoAndThrow(),
 * and logErrorErrnoAndRethrow() method templates to log using a given
 * level, optionally append a cause to the error of the current thread
 * using the correct actor, and optionally throw or rethrow.
 *
 * The methods above expect a format string and zero or more arguments
 * to be formatted with fmt::format().
 */
class Logger final
{
public:
    /* Available log levels */
    enum class Level
    {
        TRACE = BT_LOG_TRACE,
        DEBUG = BT_LOG_DEBUG,
        INFO = BT_LOG_INFO,
        WARNING = BT_LOG_WARNING,
        ERROR = BT_LOG_ERROR,
        FATAL = BT_LOG_FATAL,
        NONE = BT_LOG_NONE,
    };

    /*
     * Builds a logger from the self component class `selfCompCls` using
     * the tag `tag` and the logging level of `privQueryExec`.
     */
    explicit Logger(const bt2::SelfComponentClass selfCompCls,
                    const bt2::PrivateQueryExecutor privQueryExec, std::string tag) noexcept :
        _mSelfCompCls {selfCompCls},
        _mLevel {static_cast<Level>(privQueryExec.loggingLevel())}, _mTag {std::move(tag)}
    {
    }

    /*
     * Builds a logger from the self component `selfComp` using the tag
     * `tag`.
     */
    explicit Logger(const bt2::SelfComponent selfComp, std::string tag) noexcept :
        _mSelfComp {selfComp}, _mLevel {static_cast<Level>(selfComp.loggingLevel())}, _mTag {
                                                                                          std::move(
                                                                                              tag)}
    {
    }

    /*
     * Builds a logger from the self source component `selfComp` using
     * the tag `tag`.
     */
    explicit Logger(const bt2::SelfSourceComponent selfComp, std::string tag) noexcept :
        Logger {
            bt2::SelfComponent {bt_self_component_source_as_self_component(selfComp.libObjPtr())},
            std::move(tag)}
    {
    }

    /*
     * Builds a logger from the self filter component `selfComp` using
     * the tag `tag`.
     */
    explicit Logger(const bt2::SelfFilterComponent selfComp, std::string tag) noexcept :
        Logger {
            bt2::SelfComponent {bt_self_component_filter_as_self_component(selfComp.libObjPtr())},
            std::move(tag)}
    {
    }

    /*
     * Builds a logger from the self sink component `selfComp` using the
     * tag `tag`.
     */
    explicit Logger(const bt2::SelfSinkComponent selfComp, std::string tag) noexcept :
        Logger {bt2::SelfComponent {bt_self_component_sink_as_self_component(selfComp.libObjPtr())},
                std::move(tag)}
    {
    }

    /*
     * Builds a logger from the self message iterator `selfMsgIter`
     * using the tag `tag`.
     */
    explicit Logger(const bt2::SelfMessageIterator selfMsgIter, std::string tag) noexcept :
        Logger {selfMsgIter.component(), std::move(tag)}
    {
        _mSelfMsgIter = selfMsgIter;
    }

    /*
     * Builds a logger from the module named `moduleName` using the tag
     * `tag` and logging level `logLevel`.
     */
    explicit Logger(std::string moduleName, std::string tag, const Level logLevel) noexcept :
        _mModuleName {std::move(moduleName)}, _mLevel {logLevel}, _mTag {std::move(tag)}
    {
    }

    /*
     * Builds a logger from another logger `other` using the new tag
     * `newTag`.
     */
    explicit Logger(const Logger& other, std::string newTag) :
        _mSelfComp {other._mSelfComp}, _mSelfMsgIter {other._mSelfMsgIter},
        _mModuleName {other._mModuleName}, _mLevel {other._mLevel}, _mTag {std::move(newTag)}
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
     * Current logging level converted to a `bt_log_level` value.
     *
     * For legacy code.
     */
    bt_log_level cLevel() const noexcept
    {
        return static_cast<bt_log_level>(_mLevel);
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
        return this->wouldLog(Level::TRACE);
    }

    /*
     * Whether or not this logger would log at the debug level.
     */
    bool wouldLogD() const noexcept
    {
        return this->wouldLog(Level::DEBUG);
    }

    /*
     * Whether or not this logger would log at the info level.
     */
    bool wouldLogI() const noexcept
    {
        return this->wouldLog(Level::INFO);
    }

    /*
     * Whether or not this logger would log at the warning level.
     */
    bool wouldLogW() const noexcept
    {
        return this->wouldLog(Level::WARNING);
    }

    /*
     * Whether or not this logger would log at the error level.
     */
    bool wouldLogE() const noexcept
    {
        return this->wouldLog(Level::ERROR);
    }

    /*
     * Whether or not this logger would log at the fatal level.
     */
    bool wouldLogF() const noexcept
    {
        return this->wouldLog(Level::FATAL);
    }

    /*
     * Logging tag.
     */
    const std::string& tag() const noexcept
    {
        return _mTag;
    }

    /*
     * Self component class actor, or `bt2s::nullopt` if none.
     */
    const bt2s::optional<bt2::SelfComponentClass>& selfCompCls() const noexcept
    {
        return _mSelfCompCls;
    }

    /*
     * Self component actor, or `bt2s::nullopt` if none.
     */
    const bt2s::optional<bt2::SelfComponent>& selfComp() const noexcept
    {
        return _mSelfComp;
    }

    /*
     * Self message iterator actor, or `bt2s::nullopt` if none.
     */
    const bt2s::optional<bt2::SelfMessageIterator>& selfMsgIter() const noexcept
    {
        return _mSelfMsgIter;
    }

    /*
     * Name of module actor, or `bt2s::nullopt` if none.
     */
    const bt2s::optional<std::string>& moduleName() const noexcept
    {
        return _mModuleName;
    }

private:
    struct _StdLogWriter final
    {
        static void write(const char * const fileName, const char * const funcName,
                          const unsigned lineNo, const Level level, const char * const tag,
                          const void *, unsigned int, const char * const initMsg,
                          const char * const msg) noexcept
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
    void logNoThrow(const char * const fileName, const char * const funcName,
                    const unsigned int lineNo, const char * const fmt, ArgTs&&...args) const
    {
        this->_logNoThrow<_StdLogWriter, LevelV, AppendCauseV>(
            fileName, funcName, lineNo, nullptr, 0, "", fmt, std::forward<ArgTs>(args)...);
    }

    /*
     * Logs `msg` using the level `LevelV`.
     *
     * If `AppendCauseV` is true, this method also appends a cause to
     * the error of the current thread using the same message.
     */
    template <Level LevelV, bool AppendCauseV>
    void logStrNoThrow(const char * const fileName, const char * const funcName,
                       const unsigned int lineNo, const char * const msg) const
    {
        this->_logStrNoThrow<_StdLogWriter, LevelV, AppendCauseV>(fileName, funcName, lineNo,
                                                                  nullptr, 0, "", msg);
    }

    /*
     * Like logAndNoThrow() with the `Level::ERROR` level, but also
     * throws a default-constructed instance of `ExcT`.
     */
    template <bool AppendCauseV, typename ExcT, typename... ArgTs>
    [[noreturn]] void logErrorAndThrow(const char * const fileName, const char * const funcName,
                                       const unsigned int lineNo, const char * const fmt,
                                       ArgTs&&...args) const
    {
        this->logNoThrow<Level::ERROR, AppendCauseV>(fileName, funcName, lineNo, fmt,
                                                     std::forward<ArgTs>(args)...);
        throw ExcT {};
    }

    /*
     * Like logStrAndNoThrow() with the `Level::ERROR` level, but also
     * throws a default-constructed instance of `ExcT`.
     */
    template <bool AppendCauseV, typename ExcT>
    [[noreturn]] void logErrorStrAndThrow(const char * const fileName, const char * const funcName,
                                          const unsigned int lineNo, const char * const msg) const
    {
        this->logStrNoThrow<Level::ERROR, AppendCauseV>(fileName, funcName, lineNo, msg);
        throw ExcT {};
    }

    /*
     * Like logAndNoThrow() with the `Level::ERROR` level, but also
     * rethrows.
     */
    template <bool AppendCauseV, typename... ArgTs>
    [[noreturn]] void logErrorAndRethrow(const char * const fileName, const char * const funcName,
                                         const unsigned int lineNo, const char * const fmt,
                                         ArgTs&&...args) const
    {
        this->logNoThrow<Level::ERROR, AppendCauseV>(fileName, funcName, lineNo, fmt,
                                                     std::forward<ArgTs>(args)...);
        throw;
    }

    /*
     * Like logStrAndNoThrow() with the `Level::ERROR` level, but also
     * rethrows.
     */
    template <bool AppendCauseV>
    [[noreturn]] void logErrorStrAndRethrow(const char * const fileName,
                                            const char * const funcName, const unsigned int lineNo,
                                            const char * const msg) const
    {
        this->logStrNoThrow<Level::ERROR, AppendCauseV>(fileName, funcName, lineNo, msg);
        throw;
    }

private:
    struct _InitMsgLogWriter final
    {
        static void write(const char * const fileName, const char * const funcName,
                          const unsigned lineNo, const Level level, const char * const tag,
                          const void *, unsigned int, const char * const initMsg,
                          const char * const msg) noexcept
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
    void logErrnoNoThrow(const char * const fileName, const char * const funcName,
                         const unsigned int lineNo, const char * const initMsg,
                         const char * const fmt, ArgTs&&...args) const
    {
        this->_logNoThrow<_InitMsgLogWriter, LevelV, AppendCauseV>(
            fileName, funcName, lineNo, nullptr, 0, this->_errnoIntroStr(initMsg).c_str(), fmt,
            std::forward<ArgTs>(args)...);
    }

    /*
     * Logs the message of `errno` using the level `LevelV`.
     *
     * The log message starts with `initMsg`, is followed with the
     * message for `errno`, and then with `msg`.
     *
     * If `AppendCauseV` is true, this method also appends a cause to
     * the error of the current thread using the same message.
     */
    template <Level LevelV, bool AppendCauseV>
    void logErrnoStrNoThrow(const char * const fileName, const char * const funcName,
                            const unsigned int lineNo, const char * const initMsg,
                            const char * const msg) const
    {
        this->_logStrNoThrow<_InitMsgLogWriter, LevelV, AppendCauseV>(
            fileName, funcName, lineNo, nullptr, 0, this->_errnoIntroStr(initMsg).c_str(), msg);
    }

    /*
     * Like logErrnoNoThrow() with the `Level::ERROR` level, but also
     * throws a default-constructed instance of `ExcT`.
     */
    template <bool AppendCauseV, typename ExcT, typename... ArgTs>
    [[noreturn]] void logErrorErrnoAndThrow(const char * const fileName,
                                            const char * const funcName, const unsigned int lineNo,
                                            const char * const initMsg, const char * const fmt,
                                            ArgTs&&...args) const
    {
        this->logErrnoNoThrow<Level::ERROR, AppendCauseV>(fileName, funcName, lineNo, initMsg, fmt,
                                                          std::forward<ArgTs>(args)...);
        throw ExcT {};
    }

    /*
     * Like logErrnoStrNoThrow() with the `Level::ERROR` level, but also
     * throws a default-constructed instance of `ExcT`.
     */
    template <bool AppendCauseV, typename ExcT>
    [[noreturn]] void
    logErrorErrnoStrAndThrow(const char * const fileName, const char * const funcName,
                             const unsigned int lineNo, const char * const initMsg,
                             const char * const msg) const
    {
        this->logErrnoStrNoThrow<Level::ERROR, AppendCauseV>(fileName, funcName, lineNo, initMsg,
                                                             msg);
        throw ExcT {};
    }

    /*
     * Like logErrnoNoThrow() with the `Level::ERROR` level, but also
     * rethrows.
     */
    template <bool AppendCauseV, typename... ArgTs>
    [[noreturn]] void logErrorErrnoAndRethrow(const char * const fileName,
                                              const char * const funcName,
                                              const unsigned int lineNo, const char * const initMsg,
                                              const char * const fmt, ArgTs&&...args) const
    {
        this->logErrnoNoThrow<Level::ERROR, AppendCauseV>(fileName, funcName, lineNo, initMsg, fmt,
                                                          std::forward<ArgTs>(args)...);
        throw;
    }

    /*
     * Like logErrnoStrNoThrow() with the `Level::ERROR` level, but also
     * rethrows.
     */
    template <bool AppendCauseV>
    [[noreturn]] void
    logErrorErrnoStrAndRethrow(const char * const fileName, const char * const funcName,
                               const unsigned int lineNo, const char * const initMsg,
                               const char * const msg) const
    {
        this->logErrnoStrNoThrow<Level::ERROR, AppendCauseV>(fileName, funcName, lineNo, initMsg,
                                                             msg);
        throw;
    }

private:
    struct _MemLogWriter final
    {
        static void write(const char * const fileName, const char * const funcName,
                          const unsigned lineNo, const Level level, const char * const tag,
                          const void * const memData, const unsigned int memLen, const char *,
                          const char * const msg) noexcept
        {
            bt_log_write_mem(funcName, fileName, lineNo, static_cast<bt_log_level>(level), tag,
                             memData, memLen, msg);
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
    void logMemNoThrow(const char * const fileName, const char * const funcName,
                       const unsigned int lineNo, const void * const memData,
                       const unsigned int memLen, const char * const fmt, ArgTs&&...args) const
    {
        this->_logNoThrow<_MemLogWriter, LevelV, false>(fileName, funcName, lineNo, memData, memLen,
                                                        "", fmt, std::forward<ArgTs>(args)...);
    }

    /*
     * Logs memory data using the level `LevelV`, starting with the
     * message `msg`.
     */
    template <Level LevelV>
    void logMemStrNoThrow(const char * const fileName, const char * const funcName,
                          const unsigned int lineNo, const void * const memData,
                          const unsigned int memLen, const char * const msg) const
    {
        this->_logStrNoThrow<_MemLogWriter, LevelV, false>(fileName, funcName, lineNo, memData,
                                                           memLen, "", msg);
    }

private:
    /*
     * Formats a log message with fmt::format() given `fmt` and `args`,
     * and then forwards everything to _logStrNoThrow().
     */
    template <typename LogWriterT, Level LevelV, bool AppendCauseV, typename... ArgTs>
    void _logNoThrow(const char * const fileName, const char * const funcName,
                     const unsigned int lineNo, const void * const memData,
                     const std::size_t memLen, const char * const initMsg, const char * const fmt,
                     ArgTs&&...args) const
    {
        /* Only format arguments if logging or appending an error cause */
        if (G_UNLIKELY(this->wouldLog(LevelV) || AppendCauseV)) {
            /*
             * Format arguments to our buffer (fmt::format_to() doesn't
             * append a null character).
             */
            _mBuf.clear();
            fmt::format_to(std::back_inserter(_mBuf), fmt, std::forward<ArgTs>(args)...);
            _mBuf.push_back('\0');
        }

        this->_logStrNoThrow<LogWriterT, LevelV, AppendCauseV>(fileName, funcName, lineNo, memData,
                                                               memLen, initMsg, _mBuf.data());
    }

    /*
     * Calls LogWriterT::write() with its arguments to log using the
     * level `LevelV`.
     *
     * If `AppendCauseV` is true, this method also appends a cause to
     * the error of the current thread using the concatenation of
     * `initMsg` and `msg` as the message.
     */
    template <typename LogWriterT, Level LevelV, bool AppendCauseV>
    void _logStrNoThrow(const char * const fileName, const char * const funcName,
                        const unsigned int lineNo, const void * const memData,
                        const std::size_t memLen, const char * const initMsg,
                        const char * const msg) const
    {
        /* Initial message and main message are required */
        BT_ASSERT(initMsg);
        BT_ASSERT(msg);

        /* Log if needed */
        if (this->wouldLog(LevelV)) {
            LogWriterT::write(fileName, funcName, lineNo, LevelV, _mTag.data(), memData, memLen,
                              initMsg, msg);
        }

        /* Append an error cause if needed */
        if (AppendCauseV) {
            if (_mSelfMsgIter) {
                bt_current_thread_error_append_cause_from_message_iterator(
                    _mSelfMsgIter->libObjPtr(), fileName, lineNo, "%s%s", initMsg, msg);
            } else if (_mSelfComp) {
                bt_current_thread_error_append_cause_from_component(
                    _mSelfComp->libObjPtr(), fileName, lineNo, "%s%s", initMsg, msg);
            } else if (_mSelfCompCls) {
                bt_current_thread_error_append_cause_from_component_class(
                    _mSelfCompCls->libObjPtr(), fileName, lineNo, "%s%s", initMsg, msg);
            } else {
                BT_ASSERT(_mModuleName);
                bt_current_thread_error_append_cause_from_unknown(_mModuleName->data(), fileName,
                                                                  lineNo, "%s%s", initMsg, msg);
            }
        }
    }

    static std::string _errnoIntroStr(const char * const initMsg)
    {
        BT_ASSERT(errno != 0);
        return fmt::format("{}: {}", initMsg, g_strerror(errno));
    }

    /* At least one of the following four members has a value */
    bt2s::optional<bt2::SelfComponentClass> _mSelfCompCls;
    bt2s::optional<bt2::SelfComponent> _mSelfComp;
    bt2s::optional<bt2::SelfMessageIterator> _mSelfMsgIter;
    bt2s::optional<std::string> _mModuleName;

    /* Current logging level */
    Level _mLevel;

    /* Logging tag */
    std::string _mTag;

    /* Formatting buffer */
    mutable std::vector<char> _mBuf;
};

} /* namespace bt2c */

/* Internal: default logger name */
#define _BT_CPPLOG_DEF_LOGGER _mLogger

/*
 * Calls logNoThrow() on `_logger` to log using the level `_lvl` without
 * appending nor throwing.
 */
#define BT_CPPLOG_EX(_lvl, _logger, _fmt, ...)                                                     \
    do {                                                                                           \
        if (G_UNLIKELY((_logger).wouldLog(_lvl))) {                                                \
            (_logger).logNoThrow<(_lvl), false>(__FILE__, __func__, __LINE__, (_fmt),              \
                                                ##__VA_ARGS__);                                    \
        }                                                                                          \
    } while (0)

/*
 * BT_CPPLOG_EX() with specific logging levels.
 */
#define BT_CPPLOGT_SPEC(_logger, _fmt, ...)                                                        \
    BT_CPPLOG_EX(bt2c::Logger::Level::TRACE, (_logger), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGD_SPEC(_logger, _fmt, ...)                                                        \
    BT_CPPLOG_EX(bt2c::Logger::Level::DEBUG, (_logger), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGI_SPEC(_logger, _fmt, ...)                                                        \
    BT_CPPLOG_EX(bt2c::Logger::Level::INFO, (_logger), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGW_SPEC(_logger, _fmt, ...)                                                        \
    BT_CPPLOG_EX(bt2c::Logger::Level::WARNING, (_logger), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGE_SPEC(_logger, _fmt, ...)                                                        \
    BT_CPPLOG_EX(bt2c::Logger::Level::ERROR, (_logger), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGF_SPEC(_logger, _fmt, ...)                                                        \
    BT_CPPLOG_EX(bt2c::Logger::Level::FATAL, (_logger), (_fmt), ##__VA_ARGS__)

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
 * Calls logStrNoThrow() on `_logger` to log using the level `_lvl`
 * without appending nor throwing.
 */
#define BT_CPPLOG_STR_EX(_lvl, _logger, _msg)                                                      \
    (_logger).logStrNoThrow<(_lvl), false>(__FILE__, __func__, __LINE__, (_msg))

/*
 * BT_CPPLOG_STR_EX() with specific logging levels.
 */
#define BT_CPPLOGT_STR_SPEC(_logger, _msg)                                                         \
    BT_CPPLOG_STR_EX(bt2c::Logger::Level::TRACE, (_logger), (_msg))
#define BT_CPPLOGD_STR_SPEC(_logger, _msg)                                                         \
    BT_CPPLOG_STR_EX(bt2c::Logger::Level::DEBUG, (_logger), (_msg))
#define BT_CPPLOGI_STR_SPEC(_logger, _msg)                                                         \
    BT_CPPLOG_STR_EX(bt2c::Logger::Level::INFO, (_logger), (_msg))
#define BT_CPPLOGW_STR_SPEC(_logger, _msg)                                                         \
    BT_CPPLOG_STR_EX(bt2c::Logger::Level::WARNING, (_logger), (_msg))
#define BT_CPPLOGE_STR_SPEC(_logger, _msg)                                                         \
    BT_CPPLOG_STR_EX(bt2c::Logger::Level::ERROR, (_logger), (_msg))
#define BT_CPPLOGF_STR_SPEC(_logger, _msg)                                                         \
    BT_CPPLOG_STR_EX(bt2c::Logger::Level::FATAL, (_logger), (_msg))

/*
 * BT_CPPLOG_STR_EX() with specific logging levels and using the default
 * logger.
 */
#define BT_CPPLOGT_STR(_msg) BT_CPPLOGT_STR_SPEC(_BT_CPPLOG_DEF_LOGGER, (_msg))
#define BT_CPPLOGD_STR(_msg) BT_CPPLOGD_STR_SPEC(_BT_CPPLOG_DEF_LOGGER, (_msg))
#define BT_CPPLOGI_STR(_msg) BT_CPPLOGI_STR_SPEC(_BT_CPPLOG_DEF_LOGGER, (_msg))
#define BT_CPPLOGW_STR(_msg) BT_CPPLOGW_STR_SPEC(_BT_CPPLOG_DEF_LOGGER, (_msg))
#define BT_CPPLOGE_STR(_msg) BT_CPPLOGE_STR_SPEC(_BT_CPPLOG_DEF_LOGGER, (_msg))
#define BT_CPPLOGF_STR(_msg) BT_CPPLOGF_STR_SPEC(_BT_CPPLOG_DEF_LOGGER, (_msg))

/*
 * Calls logMemNoThrow() on `_logger` to log using the level `_lvl`
 * without appending nor throwing.
 */
#define BT_CPPLOG_MEM_EX(_lvl, _logger, _mem_data, _mem_len, _fmt, ...)                            \
    do {                                                                                           \
        if (G_UNLIKELY((_logger).wouldLog(_lvl))) {                                                \
            (_logger).logMemNoThrow<(_lvl)>(__FILE__, __func__, __LINE__, (_mem_data), (_mem_len), \
                                            (_fmt), ##__VA_ARGS__);                                \
        }                                                                                          \
    } while (0)

/*
 * BT_CPPLOG_MEM_EX() with specific logging levels.
 */
#define BT_CPPLOGT_MEM_SPEC(_logger, _mem_data, _mem_len, _fmt, ...)                               \
    BT_CPPLOG_MEM_EX(bt2c::Logger::Level::TRACE, (_logger), (_mem_data), (_mem_len), (_fmt),       \
                     ##__VA_ARGS__)
#define BT_CPPLOGD_MEM_SPEC(_logger, _mem_data, _mem_len, _fmt, ...)                               \
    BT_CPPLOG_MEM_EX(bt2c::Logger::Level::DEBUG, (_logger), (_mem_data), (_mem_len), (_fmt),       \
                     ##__VA_ARGS__)
#define BT_CPPLOGI_MEM_SPEC(_logger, _mem_data, _mem_len, _fmt, ...)                               \
    BT_CPPLOG_MEM_EX(bt2c::Logger::Level::INFO, (_logger), (_mem_data), (_mem_len), (_fmt),        \
                     ##__VA_ARGS__)
#define BT_CPPLOGW_MEM_SPEC(_logger, _mem_data, _mem_len, _fmt, ...)                               \
    BT_CPPLOG_MEM_EX(bt2c::Logger::Level::WARNING, (_logger), (_mem_data), (_mem_len), (_fmt),     \
                     ##__VA_ARGS__)
#define BT_CPPLOGE_MEM_SPEC(_logger, _mem_data, _mem_len, _fmt, ...)                               \
    BT_CPPLOG_MEM_EX(bt2c::Logger::Level::ERROR, (_logger), (_mem_data), (_mem_len), (_fmt),       \
                     ##__VA_ARGS__)
#define BT_CPPLOGF_MEM_SPEC(_logger, _mem_data, _mem_len, _fmt, ...)                               \
    BT_CPPLOG_MEM_EX(bt2c::Logger::Level::FATAL, (_logger), (_mem_data), (_mem_len), (_fmt),       \
                     ##__VA_ARGS__)

/*
 * BT_CPPLOG_MEM_EX() with specific logging levels and using the default
 * logger.
 */
#define BT_CPPLOGT_MEM(_mem_data, _mem_len, _fmt, ...)                                             \
    BT_CPPLOGT_MEM_SPEC(_BT_CPPLOG_DEF_LOGGER, (_mem_data), (_mem_len), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGD_MEM(_mem_data, _mem_len, _fmt, ...)                                             \
    BT_CPPLOGD_MEM_SPEC(_BT_CPPLOG_DEF_LOGGER, (_mem_data), (_mem_len), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGI_MEM(_mem_data, _mem_len, _fmt, ...)                                             \
    BT_CPPLOGI_MEM_SPEC(_BT_CPPLOG_DEF_LOGGER, (_mem_data), (_mem_len), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGW_MEM(_mem_data, _mem_len, _fmt, ...)                                             \
    BT_CPPLOGW_MEM_SPEC(_BT_CPPLOG_DEF_LOGGER, (_mem_data), (_mem_len), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGE_MEM(_mem_data, _mem_len, _fmt, ...)                                             \
    BT_CPPLOGE_MEM_SPEC(_BT_CPPLOG_DEF_LOGGER, (_mem_data), (_mem_len), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGF_MEM(_mem_data, _mem_len, _fmt, ...)                                             \
    BT_CPPLOGF_MEM_SPEC(_BT_CPPLOG_DEF_LOGGER, (_mem_data), (_mem_len), (_fmt), ##__VA_ARGS__)

/*
 * Calls logMemStrNoThrow() on `_logger` to log using the level `_lvl`
 * without appending nor throwing.
 */
#define BT_CPPLOG_MEM_STR_EX(_lvl, _logger, _mem_data, _mem_len, _msg)                             \
    (_logger).logMemStrNoThrow<(_lvl)>(__FILE__, __func__, __LINE__, (_mem_data), (_mem_len),      \
                                       (_msg))

/*
 * BT_CPPLOG_MEM_STR_EX() with specific logging levels.
 */
#define BT_CPPLOGT_MEM_STR_SPEC(_logger, _mem_data, _mem_len, _msg)                                \
    BT_CPPLOG_MEM_STR_EX(bt2c::Logger::Level::TRACE, (_logger), (_mem_data), (_mem_len), (_msg))
#define BT_CPPLOGD_MEM_STR_SPEC(_logger, _mem_data, _mem_len, _msg)                                \
    BT_CPPLOG_MEM_STR_EX(bt2c::Logger::Level::DEBUG, (_logger), (_mem_data), (_mem_len), (_msg))
#define BT_CPPLOGI_MEM_STR_SPEC(_logger, _mem_data, _mem_len, _msg)                                \
    BT_CPPLOG_MEM_STR_EX(bt2c::Logger::Level::INFO, (_logger), (_mem_data), (_mem_len), (_msg))
#define BT_CPPLOGW_MEM_STR_SPEC(_logger, _mem_data, _mem_len, _msg)                                \
    BT_CPPLOG_MEM_STR_EX(bt2c::Logger::Level::WARNING, (_logger), (_mem_data), (_mem_len), (_msg))
#define BT_CPPLOGE_MEM_STR_SPEC(_logger, _mem_data, _mem_len, _msg)                                \
    BT_CPPLOG_MEM_STR_EX(bt2c::Logger::Level::ERROR, (_logger), (_mem_data), (_mem_len), (_msg))
#define BT_CPPLOGF_MEM_STR_SPEC(_logger, _mem_data, _mem_len, _msg)                                \
    BT_CPPLOG_MEM_STR_EX(bt2c::Logger::Level::FATAL, (_logger), (_mem_data), (_mem_len), (_msg))

/*
 * BT_CPPLOG_MEM_STR_EX() with specific logging levels and using the
 * default logger.
 */
#define BT_CPPLOGT_MEM_STR(_mem_data, _mem_len, _msg)                                              \
    BT_CPPLOGT_MEM_STR_SPEC(_BT_CPPLOG_DEF_LOGGER, (_mem_data), (_mem_len), (_msg))
#define BT_CPPLOGD_MEM_STR(_mem_data, _mem_len, _msg)                                              \
    BT_CPPLOGD_MEM_STR_SPEC(_BT_CPPLOG_DEF_LOGGER, (_mem_data), (_mem_len), (_msg))
#define BT_CPPLOGI_MEM_STR(_mem_data, _mem_len, _msg)                                              \
    BT_CPPLOGI_MEM_STR_SPEC(_BT_CPPLOG_DEF_LOGGER, (_mem_data), (_mem_len), (_msg))
#define BT_CPPLOGW_MEM_STR(_mem_data, _mem_len, _msg)                                              \
    BT_CPPLOGW_MEM_STR_SPEC(_BT_CPPLOG_DEF_LOGGER, (_mem_data), (_mem_len), (_msg))
#define BT_CPPLOGE_MEM_STR(_mem_data, _mem_len, _msg)                                              \
    BT_CPPLOGE_MEM_STR_SPEC(_BT_CPPLOG_DEF_LOGGER, (_mem_data), (_mem_len), (_msg))
#define BT_CPPLOGF_MEM_STR(_mem_data, _mem_len, _msg)                                              \
    BT_CPPLOGF_MEM_STR_SPEC(_BT_CPPLOG_DEF_LOGGER, (_mem_data), (_mem_len), (_msg))

/*
 * Calls logErrnoNoThrow() on `_logger` to log using the level `_lvl`
 * and initial message `_init_msg` without appending nor throwing.
 */
#define BT_CPPLOG_ERRNO_EX(_lvl, _logger, _init_msg, _fmt, ...)                                    \
    do {                                                                                           \
        if (G_UNLIKELY((_logger).wouldLog(_lvl))) {                                                \
            (_logger).logErrnoNoThrow<(_lvl), false>(__FILE__, __func__, __LINE__, (_init_msg),    \
                                                     (_fmt), ##__VA_ARGS__);                       \
        }                                                                                          \
    } while (0)

/*
 * BT_CPPLOG_ERRNO_EX() with specific logging levels.
 */
#define BT_CPPLOGT_ERRNO_SPEC(_logger, _init_msg, _fmt, ...)                                       \
    BT_CPPLOG_ERRNO_EX(bt2c::Logger::Level::TRACE, (_logger), (_init_msg), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGD_ERRNO_SPEC(_logger, _init_msg, _fmt, ...)                                       \
    BT_CPPLOG_ERRNO_EX(bt2c::Logger::Level::DEBUG, (_logger), (_init_msg), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGI_ERRNO_SPEC(_logger, _init_msg, _fmt, ...)                                       \
    BT_CPPLOG_ERRNO_EX(bt2c::Logger::Level::INFO, (_logger), (_init_msg), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGW_ERRNO_SPEC(_logger, _init_msg, _fmt, ...)                                       \
    BT_CPPLOG_ERRNO_EX(bt2c::Logger::Level::WARNING, (_logger), (_init_msg), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGE_ERRNO_SPEC(_logger, _init_msg, _fmt, ...)                                       \
    BT_CPPLOG_ERRNO_EX(bt2c::Logger::Level::ERROR, (_logger), (_init_msg), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGF_ERRNO_SPEC(_logger, _init_msg, _fmt, ...)                                       \
    BT_CPPLOG_ERRNO_EX(bt2c::Logger::Level::FATAL, (_logger), (_init_msg), (_fmt), ##__VA_ARGS__)

/*
 * BT_CPPLOG_ERRNO_EX() with specific logging levels and using the
 * default logger.
 */
#define BT_CPPLOGT_ERRNO(_init_msg, _fmt, ...)                                                     \
    BT_CPPLOGT_ERRNO_SPEC(_BT_CPPLOG_DEF_LOGGER, (_init_msg), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGD_ERRNO(_init_msg, _fmt, ...)                                                     \
    BT_CPPLOGD_ERRNO_SPEC(_BT_CPPLOG_DEF_LOGGER, (_init_msg), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGI_ERRNO(_init_msg, _fmt, ...)                                                     \
    BT_CPPLOGI_ERRNO_SPEC(_BT_CPPLOG_DEF_LOGGER, (_init_msg), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGW_ERRNO(_init_msg, _fmt, ...)                                                     \
    BT_CPPLOGW_ERRNO_SPEC(_BT_CPPLOG_DEF_LOGGER, (_init_msg), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGE_ERRNO(_init_msg, _fmt, ...)                                                     \
    BT_CPPLOGE_ERRNO_SPEC(_BT_CPPLOG_DEF_LOGGER, (_init_msg), (_fmt), ##__VA_ARGS__)
#define BT_CPPLOGF_ERRNO(_init_msg, _fmt, ...)                                                     \
    BT_CPPLOGF_ERRNO_SPEC(_BT_CPPLOG_DEF_LOGGER, (_init_msg), (_fmt), ##__VA_ARGS__)

/*
 * Calls logErrnoStrNoThrow() on `_logger` to log using the level `_lvl`
 * and initial message `_init_msg` without appending nor throwing.
 */
#define BT_CPPLOG_ERRNO_STR_EX(_lvl, _logger, _init_msg, _msg)                                     \
    (_logger).logErrnoStrNoThrow<(_lvl), false>(__FILE__, __func__, __LINE__, (_init_msg), (_msg))

/*
 * BT_CPPLOG_ERRNO_STR_EX() with specific logging levels.
 */
#define BT_CPPLOGT_ERRNO_STR_SPEC(_logger, _init_msg, _msg)                                        \
    BT_CPPLOG_ERRNO_STR_EX(bt2c::Logger::Level::TRACE, (_logger), (_init_msg), (_msg))
#define BT_CPPLOGD_ERRNO_STR_SPEC(_logger, _init_msg, _msg)                                        \
    BT_CPPLOG_ERRNO_STR_EX(bt2c::Logger::Level::DEBUG, (_logger), (_init_msg), (_msg))
#define BT_CPPLOGI_ERRNO_STR_SPEC(_logger, _init_msg, _msg)                                        \
    BT_CPPLOG_ERRNO_STR_EX(bt2c::Logger::Level::INFO, (_logger), (_init_msg), (_msg))
#define BT_CPPLOGW_ERRNO_STR_SPEC(_logger, _init_msg, _msg)                                        \
    BT_CPPLOG_ERRNO_STR_EX(bt2c::Logger::Level::WARNING, (_logger), (_init_msg), (_msg))
#define BT_CPPLOGE_ERRNO_STR_SPEC(_logger, _init_msg, _msg)                                        \
    BT_CPPLOG_ERRNO_STR_EX(bt2c::Logger::Level::ERROR, (_logger), (_init_msg), (_msg))
#define BT_CPPLOGF_ERRNO_STR_SPEC(_logger, _init_msg, _msg)                                        \
    BT_CPPLOG_ERRNO_STR_EX(bt2c::Logger::Level::FATAL, (_logger), (_init_msg), (_msg))

/*
 * BT_CPPLOG_ERRNO_STR_EX() with specific logging levels and using the
 * default logger.
 */
#define BT_CPPLOGT_ERRNO_STR(_init_msg, _msg)                                                      \
    BT_CPPLOGT_ERRNO_STR_SPEC(_BT_CPPLOG_DEF_LOGGER, (_init_msg), (_msg))
#define BT_CPPLOGD_ERRNO_STR(_init_msg, _msg)                                                      \
    BT_CPPLOGD_ERRNO_STR_SPEC(_BT_CPPLOG_DEF_LOGGER, (_init_msg), (_msg))
#define BT_CPPLOGI_ERRNO_STR(_init_msg, _msg)                                                      \
    BT_CPPLOGI_ERRNO_STR_SPEC(_BT_CPPLOG_DEF_LOGGER, (_init_msg), (_msg))
#define BT_CPPLOGW_ERRNO_STR(_init_msg, _msg)                                                      \
    BT_CPPLOGW_ERRNO_STR_SPEC(_BT_CPPLOG_DEF_LOGGER, (_init_msg), (_msg))
#define BT_CPPLOGE_ERRNO_STR(_init_msg, _msg)                                                      \
    BT_CPPLOGE_ERRNO_STR_SPEC(_BT_CPPLOG_DEF_LOGGER, (_init_msg), (_msg))
#define BT_CPPLOGF_ERRNO_STR(_init_msg, _msg)                                                      \
    BT_CPPLOGF_ERRNO_STR_SPEC(_BT_CPPLOG_DEF_LOGGER, (_init_msg), (_msg))

/*
 * Calls logErrorAndThrow() on `_logger` to log an error, append a cause
 * to the error of the current thread, and throw an instance of
 * `_exc_cls`.
 */
#define BT_CPPLOGE_APPEND_CAUSE_AND_THROW_SPEC(_logger, _exc_cls, _fmt, ...)                       \
    (_logger).logErrorAndThrow<true, _exc_cls>(__FILE__, __func__, __LINE__, (_fmt), ##__VA_ARGS__)

/*
 * Calls logErrorAndThrow() on `_logger` to log an error, append a cause
 * to the error of the current thread, and throw an instance of
 * `_exc_cls`.
 */
#define BT_CPPLOGE_APPEND_CAUSE_AND_THROW_SPEC(_logger, _exc_cls, _fmt, ...)                       \
    (_logger).logErrorAndThrow<true, _exc_cls>(__FILE__, __func__, __LINE__, (_fmt), ##__VA_ARGS__)

/*
 * BT_CPPLOGE_APPEND_CAUSE_AND_THROW_SPEC() using the default logger.
 */
#define BT_CPPLOGE_APPEND_CAUSE_AND_THROW(_exc_cls, _fmt, ...)                                     \
    BT_CPPLOGE_APPEND_CAUSE_AND_THROW_SPEC(_BT_CPPLOG_DEF_LOGGER, _exc_cls, (_fmt), ##__VA_ARGS__)

/*
 * Calls logErrorStrAndThrow() on `_logger` to log an error, append a
 * cause to the error of the current thread, and throw an instance of
 * `_exc_cls`.
 */
#define BT_CPPLOGE_STR_APPEND_CAUSE_AND_THROW_SPEC(_logger, _exc_cls, _msg)                        \
    (_logger).logErrorStrAndThrow<true, _exc_cls>(__FILE__, __func__, __LINE__, (_msg))

/*
 * BT_CPPLOGE_STR_APPEND_CAUSE_AND_THROW_SPEC() using the default
 * logger.
 */
#define BT_CPPLOGE_STR_APPEND_CAUSE_AND_THROW(_exc_cls, _msg)                                      \
    BT_CPPLOGE_STR_APPEND_CAUSE_AND_THROW_SPEC(_BT_CPPLOG_DEF_LOGGER, _exc_cls, (_msg))

/*
 * Calls logErrorAndRethrow() on `_logger` to log an error, append a
 * cause to the error of the current thread, and throw an instance of
 * `_exc_cls`.
 */
#define BT_CPPLOGE_APPEND_CAUSE_AND_RETHROW_SPEC(_logger, _fmt, ...)                               \
    (_logger).logErrorAndRethrow<true>(__FILE__, __func__, __LINE__, (_fmt), ##__VA_ARGS__)

/*
 * BT_CPPLOGE_APPEND_CAUSE_AND_RETHROW_SPEC() using the default logger.
 */
#define BT_CPPLOGE_APPEND_CAUSE_AND_RETHROW(_fmt, ...)                                             \
    BT_CPPLOGE_APPEND_CAUSE_AND_RETHROW_SPEC(_BT_CPPLOG_DEF_LOGGER, (_fmt), ##__VA_ARGS__)

/*
 * Calls logErrorStrAndRethrow() on `_logger` to log an error, append a
 * cause to the error of the current thread, and throw an instance of
 * `_exc_cls`.
 */
#define BT_CPPLOGE_STR_APPEND_CAUSE_AND_RETHROW_SPEC(_logger, _msg)                                \
    (_logger).logErrorStrAndRethrow<true>(__FILE__, __func__, __LINE__, (_msg))

/*
 * BT_CPPLOGE_STR_APPEND_CAUSE_AND_RETHROW_SPEC() using the default
 * logger.
 */
#define BT_CPPLOGE_STR_APPEND_CAUSE_AND_RETHROW(_msg)                                              \
    BT_CPPLOGE_STR_APPEND_CAUSE_AND_RETHROW_SPEC(_BT_CPPLOG_DEF_LOGGER, (_msg))

/*
 * Calls logErrorErrnoAndThrow() on `_logger` to log an error, append a
 * cause to the error of the current thread, and throw an instance of
 * `_exc_cls`.
 */
#define BT_CPPLOGE_ERRNO_APPEND_CAUSE_AND_THROW_SPEC(_logger, _exc_cls, _init_msg, _fmt, ...)      \
    (_logger).logErrorErrnoAndThrow<true, _exc_cls>(__FILE__, __func__, __LINE__, (_init_msg),     \
                                                    (_fmt), ##__VA_ARGS__)

/*
 * BT_CPPLOGE_ERRNO_APPEND_CAUSE_AND_THROW_SPEC() using the default
 * logger.
 */
#define BT_CPPLOGE_ERRNO_APPEND_CAUSE_AND_THROW(_exc_cls, _init_msg, _fmt, ...)                    \
    BT_CPPLOGE_ERRNO_APPEND_CAUSE_AND_THROW_SPEC(_BT_CPPLOG_DEF_LOGGER, _exc_cls, (_init_msg),     \
                                                 (_fmt), ##__VA_ARGS__)

/*
 * Calls logErrorErrnoStrAndThrow() on `_logger` to log an error, append
 * a cause to the error of the current thread, and throw an instance of
 * `_exc_cls`.
 */
#define BT_CPPLOGE_ERRNO_STR_APPEND_CAUSE_AND_THROW_SPEC(_logger, _exc_cls, _init_msg, _msg)       \
    (_logger).logErrorErrnoStrAndThrow<true, _exc_cls>(__FILE__, __func__, __LINE__, (_init_msg),  \
                                                       (_msg))

/*
 * BT_CPPLOGE_ERRNO_STR_APPEND_CAUSE_AND_THROW_SPEC() using the default
 * logger.
 */
#define BT_CPPLOGE_ERRNO_STR_APPEND_CAUSE_AND_THROW(_exc_cls, _init_msg, _msg)                     \
    BT_CPPLOGE_ERRNO_STR_APPEND_CAUSE_AND_THROW_SPEC(_BT_CPPLOG_DEF_LOGGER, _exc_cls, (_init_msg), \
                                                     (_msg))

/*
 * Calls logErrorErrnoAndRethrow() on `_logger` to log an error, append
 * a cause to the error of the current thread, and throw an instance of
 * `_exc_cls`.
 */
#define BT_CPPLOGE_ERRNO_APPEND_CAUSE_AND_RETHROW_SPEC(_logger, _init_msg, _fmt, ...)              \
    (_logger).logErrorErrnoAndRethrow<true>(__FILE__, __func__, __LINE__, (_init_msg), (_fmt),     \
                                            ##__VA_ARGS__)

/*
 * BT_CPPLOGE_ERRNO_APPEND_CAUSE_AND_RETHROW_SPEC() using the default
 * logger.
 */
#define BT_CPPLOGE_ERRNO_APPEND_CAUSE_AND_RETHROW(_init_msg, _fmt, ...)                            \
    BT_CPPLOGE_ERRNO_APPEND_CAUSE_AND_RETHROW_SPEC(_BT_CPPLOG_DEF_LOGGER, (_init_msg), (_fmt),     \
                                                   ##__VA_ARGS__)

/*
 * Calls logErrorErrnoStrAndRethrow() on `_logger` to log an error,
 * append a cause to the error of the current thread, and throw an
 * instance of `_exc_cls`.
 */
#define BT_CPPLOGE_ERRNO_STR_APPEND_CAUSE_AND_RETHROW_SPEC(_logger, _init_msg, _msg)               \
    (_logger).logErrorErrnoStrAndRethrow<true>(__FILE__, __func__, __LINE__, (_init_msg), (_msg))

/*
 * BT_CPPLOGE_ERRNO_STR_APPEND_CAUSE_AND_RETHROW_SPEC() using the
 * default logger.
 */
#define BT_CPPLOGE_ERRNO_STR_APPEND_CAUSE_AND_RETHROW(_init_msg, _msg)                             \
    BT_CPPLOGE_ERRNO_STR_APPEND_CAUSE_AND_RETHROW_SPEC(_BT_CPPLOG_DEF_LOGGER, (_init_msg), (_msg))

#endif /* BABELTRACE_CPP_COMMON_BT2C_LOGGING_HPP */
