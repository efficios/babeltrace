/*
 * This is zf_log.h, modified with Babeltrace prefixes.
 * See <https://github.com/wonder-mice/zf_log/>.
 * See logging/LICENSE in the Babeltrace source tree.
 */

#pragma once

#ifndef BABELTRACE_LOGGING_INTERNAL_H
#define BABELTRACE_LOGGING_INTERNAL_H

#include <stdlib.h>
#include <stdio.h>
#include <babeltrace/logging.h>
#include <babeltrace/babeltrace-internal.h>

/* To detect incompatible changes you can define BT_LOG_VERSION_REQUIRED to be
 * the current value of BT_LOG_VERSION before including this file (or via
 * compiler command line):
 *
 *   #define BT_LOG_VERSION_REQUIRED 4
 *   #include <babeltrace/logging-internal.h>
 *
 * Compilation will fail when included file has different version.
 */
#define BT_LOG_VERSION 4
#if defined(BT_LOG_VERSION_REQUIRED)
	#if BT_LOG_VERSION_REQUIRED != BT_LOG_VERSION
		#error different bt_log version required
	#endif
#endif

/* Log level guideline:
 * - BT_LOG_FATAL - happened something impossible and absolutely unexpected.
 *   Process can't continue and must be terminated.
 *   Example: division by zero, unexpected modifications from other thread.
 * - BT_LOG_ERROR - happened something possible, but highly unexpected. The
 *   process is able to recover and continue execution.
 *   Example: out of memory (could also be FATAL if not handled properly).
 * - BT_LOG_WARN - happened something that *usually* should not happen and
 *   significantly changes application behavior for some period of time.
 *   Example: configuration file not found, auth error.
 * - BT_LOG_INFO - happened significant life cycle event or major state
 *   transition.
 *   Example: app started, user logged in.
 * - BT_LOG_DEBUG - minimal set of events that could help to reconstruct the
 *   execution path. Usually disabled in release builds.
 * - BT_LOG_VERBOSE - all other events. Usually disabled in release builds.
 *
 * *Ideally*, log file of debugged, well tested, production ready application
 * should be empty or very small. Choosing a right log level is as important as
 * providing short and self descriptive log message.
 */
#define BT_LOG_VERBOSE BT_LOGGING_LEVEL_VERBOSE
#define BT_LOG_DEBUG   BT_LOGGING_LEVEL_DEBUG
#define BT_LOG_INFO    BT_LOGGING_LEVEL_INFO
#define BT_LOG_WARN    BT_LOGGING_LEVEL_WARN
#define BT_LOG_ERROR   BT_LOGGING_LEVEL_ERROR
#define BT_LOG_FATAL   BT_LOGGING_LEVEL_FATAL
#define BT_LOG_NONE    BT_LOGGING_LEVEL_NONE

/* "Current" log level is a compile time check and has no runtime overhead. Log
 * level that is below current log level it said to be "disabled". Otherwise,
 * it's "enabled". Log messages that are disabled has no runtime overhead - they
 * are converted to no-op by preprocessor and then eliminated by compiler.
 * Current log level is configured per compilation module (.c/.cpp/.m file) by
 * defining BT_LOG_DEF_LEVEL or BT_LOG_LEVEL. BT_LOG_LEVEL has higer priority
 * and when defined overrides value provided by BT_LOG_DEF_LEVEL.
 *
 * Common practice is to define default current log level with BT_LOG_DEF_LEVEL
 * in build script (e.g. Makefile, CMakeLists.txt, gyp, etc.) for the entire
 * project or target:
 *
 *   CC_ARGS := -DBT_LOG_DEF_LEVEL=BT_LOG_INFO
 *
 * And when necessary to override it with BT_LOG_LEVEL in .c/.cpp/.m files
 * before including bt_log.h:
 *
 *   #define BT_LOG_LEVEL BT_LOG_VERBOSE
 *   #include <babeltrace/logging-internal.h>
 *
 * If both BT_LOG_DEF_LEVEL and BT_LOG_LEVEL are undefined, then BT_LOG_INFO
 * will be used for release builds (NDEBUG is defined) and BT_LOG_DEBUG
 * otherwise (NDEBUG is not defined).
 */
#if defined(BT_LOG_LEVEL)
	#define _BT_LOG_LEVEL BT_LOG_LEVEL
#elif defined(BT_LOG_DEF_LEVEL)
	#define _BT_LOG_LEVEL BT_LOG_DEF_LEVEL
#else
	#ifdef NDEBUG
		#define _BT_LOG_LEVEL BT_LOG_INFO
	#else
		#define _BT_LOG_LEVEL BT_LOG_DEBUG
	#endif
#endif

/* "Output" log level is a runtime check. When log level is below output log
 * level it said to be "turned off" (or just "off" for short). Otherwise it's
 * "turned on" (or just "on"). Log levels that were "disabled" (see
 * BT_LOG_LEVEL and BT_LOG_DEF_LEVEL) can't be "turned on", but "enabled" log
 * levels could be "turned off". Only messages with log level which is
 * "turned on" will reach output facility. All other messages will be ignored
 * (and their arguments will not be evaluated). Output log level is a global
 * property and configured per process using bt_log_set_output_level() function
 * which can be called at any time.
 *
 * Though in some cases it could be useful to configure output log level per
 * compilation module or per library. There are two ways to achieve that:
 * - Define BT_LOG_OUTPUT_LEVEL to expresion that evaluates to desired output
 *   log level.
 * - Copy bt_log.h and bt_log.c files into your library and build it with
 *   BT_LOG_LIBRARY_PREFIX defined to library specific prefix. See
 *   BT_LOG_LIBRARY_PREFIX for more details.
 *
 * When defined, BT_LOG_OUTPUT_LEVEL must evaluate to integral value that
 * corresponds to desired output log level. Use it only when compilation module
 * is required to have output log level which is different from global output
 * log level set by bt_log_set_output_level() function. For other cases,
 * consider defining BT_LOG_LEVEL or using bt_log_set_output_level() function.
 *
 * Example:
 *
 *   #define BT_LOG_OUTPUT_LEVEL g_module_log_level
 *   #include <babeltrace/logging-internal.h>
 *   static int g_module_log_level = BT_LOG_INFO;
 *   static void foo() {
 *       BT_LOGI("Will check g_module_log_level for output log level");
 *   }
 *   void debug_log(bool on) {
 *       g_module_log_level = on? BT_LOG_DEBUG: BT_LOG_INFO;
 *   }
 *
 * Note on performance. This expression will be evaluated each time message is
 * logged (except when message log level is "disabled" - see BT_LOG_LEVEL for
 * details). Keep this expression as simple as possible, otherwise it will not
 * only add runtime overhead, but also will increase size of call site (which
 * will result in larger executable). The prefered way is to use integer
 * variable (as in example above). If structure must be used, log_level field
 * must be the first field in this structure:
 *
 *   #define BT_LOG_OUTPUT_LEVEL (g_config.log_level)
 *   #include <babeltrace/logging-internal.h>
 *   struct config {
 *       int log_level;
 *       unsigned other_field;
 *       [...]
 *   };
 *   static config g_config = {BT_LOG_INFO, 0, ...};
 *
 * This allows compiler to generate more compact load instruction (no need to
 * specify offset since it's zero). Calling a function to get output log level
 * is generaly a bad idea, since it will increase call site size and runtime
 * overhead even further.
 */
#if defined(BT_LOG_OUTPUT_LEVEL)
	#define _BT_LOG_OUTPUT_LEVEL BT_LOG_OUTPUT_LEVEL
#else
	/*
	 * We disallow this to make sure Babeltrace modules always
	 * have their own local log level.
	 */
	#error No log level symbol specified: please define BT_LOG_OUTPUT_LEVEL before including this header.
#endif

/* "Tag" is a compound string that could be associated with a log message. It
 * consists of tag prefix and tag (both are optional).
 *
 * Tag prefix is a global property and configured per process using
 * bt_log_set_tag_prefix() function. Tag prefix identifies context in which
 * component or module is running (e.g. process name). For example, the same
 * library could be used in both client and server processes that work on the
 * same machine. Tag prefix could be used to easily distinguish between them.
 * For more details about tag prefix see bt_log_set_tag_prefix() function. Tag
 * prefix
 *
 * Tag identifies component or module. It is configured per compilation module
 * (.c/.cpp/.m file) by defining BT_LOG_TAG or BT_LOG_DEF_TAG. BT_LOG_TAG has
 * higer priority and when defined overrides value provided by BT_LOG_DEF_TAG.
 * When defined, value must evaluate to (const char *), so for strings double
 * quotes must be used.
 *
 * Default tag could be defined with BT_LOG_DEF_TAG in build script (e.g.
 * Makefile, CMakeLists.txt, gyp, etc.) for the entire project or target:
 *
 *   CC_ARGS := -DBT_LOG_DEF_TAG=\"MISC\"
 *
 * And when necessary could be overriden with BT_LOG_TAG in .c/.cpp/.m files
 * before including bt_log.h:
 *
 *   #define BT_LOG_TAG "MAIN"
 *   #include <babeltrace/logging-internal.h>
 *
 * If both BT_LOG_DEF_TAG and BT_LOG_TAG are undefined no tag will be added to
 * the log message (tag prefix still could be added though).
 *
 * Output example:
 *
 *   04-29 22:43:20.244 40059  1299 I hello.MAIN Number of arguments: 1
 *                                    |     |
 *                                    |     +- tag (e.g. module)
 *                                    +- tag prefix (e.g. process name)
 */
#if defined(BT_LOG_TAG)
	#define _BT_LOG_TAG BT_LOG_TAG
#elif defined(BT_LOG_DEF_TAG)
	#define _BT_LOG_TAG BT_LOG_DEF_TAG
#else
	#define _BT_LOG_TAG 0
#endif

/* Source location is part of a log line that describes location (function or
 * method name, file name and line number, e.g. "runloop@main.cpp:68") of a
 * log statement that produced it.
 * Source location formats are:
 * - BT_LOG_SRCLOC_NONE - don't add source location to log line.
 * - BT_LOG_SRCLOC_SHORT - add source location in short form (file and line
 *   number, e.g. "@main.cpp:68").
 * - BT_LOG_SRCLOC_LONG - add source location in long form (function or method
 *   name, file and line number, e.g. "runloop@main.cpp:68").
 */
#define BT_LOG_SRCLOC_NONE  0
#define BT_LOG_SRCLOC_SHORT 1
#define BT_LOG_SRCLOC_LONG  2

/* Source location format is configured per compilation module (.c/.cpp/.m
 * file) by defining BT_LOG_DEF_SRCLOC or BT_LOG_SRCLOC. BT_LOG_SRCLOC has
 * higer priority and when defined overrides value provided by
 * BT_LOG_DEF_SRCLOC.
 *
 * Common practice is to define default format with BT_LOG_DEF_SRCLOC in
 * build script (e.g. Makefile, CMakeLists.txt, gyp, etc.) for the entire
 * project or target:
 *
 *   CC_ARGS := -DBT_LOG_DEF_SRCLOC=BT_LOG_SRCLOC_LONG
 *
 * And when necessary to override it with BT_LOG_SRCLOC in .c/.cpp/.m files
 * before including bt_log.h:
 *
 *   #define BT_LOG_SRCLOC BT_LOG_SRCLOC_NONE
 *   #include <babeltrace/logging-internal.h>
 *
 * If both BT_LOG_DEF_SRCLOC and BT_LOG_SRCLOC are undefined, then
 * BT_LOG_SRCLOC_NONE will be used for release builds (NDEBUG is defined) and
 * BT_LOG_SRCLOC_LONG otherwise (NDEBUG is not defined).
 */
#if defined(BT_LOG_SRCLOC)
	#define _BT_LOG_SRCLOC BT_LOG_SRCLOC
#elif defined(BT_LOG_DEF_SRCLOC)
	#define _BT_LOG_SRCLOC BT_LOG_DEF_SRCLOC
#else
	#ifdef NDEBUG
		#define _BT_LOG_SRCLOC BT_LOG_SRCLOC_NONE
	#else
		#define _BT_LOG_SRCLOC BT_LOG_SRCLOC_LONG
	#endif
#endif
#if BT_LOG_SRCLOC_LONG == _BT_LOG_SRCLOC
	#define _BT_LOG_SRCLOC_FUNCTION _BT_LOG_FUNCTION
#else
	#define _BT_LOG_SRCLOC_FUNCTION 0
#endif

/* Censoring provides conditional logging of secret information, also known as
 * Personally Identifiable Information (PII) or Sensitive Personal Information
 * (SPI). Censoring can be either enabled (BT_LOG_CENSORED) or disabled
 * (BT_LOG_UNCENSORED). When censoring is enabled, log statements marked as
 * "secrets" will be ignored and will have zero overhead (arguments also will
 * not be evaluated).
 */
#define BT_LOG_CENSORED   1
#define BT_LOG_UNCENSORED 0

/* Censoring is configured per compilation module (.c/.cpp/.m file) by defining
 * BT_LOG_DEF_CENSORING or BT_LOG_CENSORING. BT_LOG_CENSORING has higer priority
 * and when defined overrides value provided by BT_LOG_DEF_CENSORING.
 *
 * Common practice is to define default censoring with BT_LOG_DEF_CENSORING in
 * build script (e.g. Makefile, CMakeLists.txt, gyp, etc.) for the entire
 * project or target:
 *
 *   CC_ARGS := -DBT_LOG_DEF_CENSORING=BT_LOG_CENSORED
 *
 * And when necessary to override it with BT_LOG_CENSORING in .c/.cpp/.m files
 * before including bt_log.h (consider doing it only for debug purposes and be
 * very careful not to push such temporary changes to source control):
 *
 *   #define BT_LOG_CENSORING BT_LOG_UNCENSORED
 *   #include <babeltrace/logging-internal.h>
 *
 * If both BT_LOG_DEF_CENSORING and BT_LOG_CENSORING are undefined, then
 * BT_LOG_CENSORED will be used for release builds (NDEBUG is defined) and
 * BT_LOG_UNCENSORED otherwise (NDEBUG is not defined).
 */
#if defined(BT_LOG_CENSORING)
	#define _BT_LOG_CENSORING BT_LOG_CENSORING
#elif defined(BT_LOG_DEF_CENSORING)
	#define _BT_LOG_CENSORING BT_LOG_DEF_CENSORING
#else
	#ifdef NDEBUG
		#define _BT_LOG_CENSORING BT_LOG_CENSORED
	#else
		#define _BT_LOG_CENSORING BT_LOG_UNCENSORED
	#endif
#endif

/* Check censoring at compile time. Evaluates to true when censoring is disabled
 * (i.e. when secrets will be logged). For example:
 *
 *   #if BT_LOG_SECRETS
 *       char ssn[16];
 *       getSocialSecurityNumber(ssn);
 *       BT_LOGI("Customer ssn: %s", ssn);
 *   #endif
 *
 * See BT_LOG_SECRET() macro for a more convenient way of guarding single log
 * statement.
 */
#define BT_LOG_SECRETS (BT_LOG_UNCENSORED == _BT_LOG_CENSORING)

/* Static (compile-time) initialization support allows to configure logging
 * before entering main() function. This mostly useful in C++ where functions
 * and methods could be called during initialization of global objects. Those
 * functions and methods could record log messages too and for that reason
 * static initialization of logging configuration is customizable.
 *
 * Macros below allow to specify values to use for initial configuration:
 * - BT_LOG_EXTERN_TAG_PREFIX - tag prefix (default: none)
 * - BT_LOG_EXTERN_GLOBAL_FORMAT - global format options (default: see
 *   BT_LOG_MEM_WIDTH in bt_log.c)
 * - BT_LOG_EXTERN_GLOBAL_OUTPUT - global output facility (default: stderr or
 *   platform specific, see BT_LOG_USE_XXX macros in bt_log.c)
 * - BT_LOG_EXTERN_GLOBAL_OUTPUT_LEVEL - global output log level (default: 0 -
 *   all levals are "turned on")
 *
 * For example, in log_config.c:
 *
 *   #include <babeltrace/logging-internal.h>
 *   BT_LOG_DEFINE_TAG_PREFIX = "MyApp";
 *   BT_LOG_DEFINE_GLOBAL_FORMAT = {CUSTOM_MEM_WIDTH};
 *   BT_LOG_DEFINE_GLOBAL_OUTPUT = {BT_LOG_PUT_STD, custom_output_callback, 0};
 *   BT_LOG_DEFINE_GLOBAL_OUTPUT_LEVEL = BT_LOG_INFO;
 *
 * However, to use any of those macros bt_log library must be compiled with
 * following macros defined:
 * - to use BT_LOG_DEFINE_TAG_PREFIX define BT_LOG_EXTERN_TAG_PREFIX
 * - to use BT_LOG_DEFINE_GLOBAL_FORMAT define BT_LOG_EXTERN_GLOBAL_FORMAT
 * - to use BT_LOG_DEFINE_GLOBAL_OUTPUT define BT_LOG_EXTERN_GLOBAL_OUTPUT
 * - to use BT_LOG_DEFINE_GLOBAL_OUTPUT_LEVEL define
 *   BT_LOG_EXTERN_GLOBAL_OUTPUT_LEVEL
 *
 * When bt_log library compiled with one of BT_LOG_EXTERN_XXX macros defined,
 * corresponding BT_LOG_DEFINE_XXX macro MUST be used exactly once somewhere.
 * Otherwise build will fail with link error (undefined symbol).
 */
#define BT_LOG_DEFINE_TAG_PREFIX const char *_bt_log_tag_prefix
#define BT_LOG_DEFINE_GLOBAL_FORMAT bt_log_format _bt_log_global_format
#define BT_LOG_DEFINE_GLOBAL_OUTPUT bt_log_output _bt_log_global_output
#define BT_LOG_DEFINE_GLOBAL_OUTPUT_LEVEL int _bt_log_global_output_lvl

/* Pointer to global format options. Direct modification is not allowed. Use
 * bt_log_set_mem_width() instead. Could be used to initialize bt_log_spec
 * structure:
 *
 *   const bt_log_output g_output = {BT_LOG_PUT_STD, output_callback, 0};
 *   const bt_log_spec g_spec = {BT_LOG_GLOBAL_FORMAT, &g_output};
 *   BT_LOGI_AUX(&g_spec, "Hello");
 */
#define BT_LOG_GLOBAL_FORMAT ((const bt_log_format *)&_bt_log_global_format)

/* Pointer to global output variable. Direct modification is not allowed. Use
 * bt_log_set_output_v() or bt_log_set_output_p() instead. Could be used to
 * initialize bt_log_spec structure:
 *
 *   const bt_log_format g_format = {40};
 *   const bt_log_spec g_spec = {g_format, BT_LOG_GLOBAL_OUTPUT};
 *   BT_LOGI_AUX(&g_spec, "Hello");
 */
#define BT_LOG_GLOBAL_OUTPUT ((const bt_log_output *)&_bt_log_global_output)

/* When defined, all library symbols produced by linker will be prefixed with
 * provided value. That allows to use bt_log library privately in another
 * libraries without exposing bt_log symbols in their original form (to avoid
 * possible conflicts with other libraries / components that also could use
 * bt_log for logging). Value must be without quotes, for example:
 *
 *   CC_ARGS := -DBT_LOG_LIBRARY_PREFIX=my_lib_
 *
 * Note, that in this mode BT_LOG_LIBRARY_PREFIX must be defined when building
 * bt_log library AND it also must be defined to the same value when building
 * a library that uses it. For example, consider fictional KittyHttp library
 * that wants to use bt_log for logging. First approach that could be taken is
 * to add bt_log.h and bt_log.c to the KittyHttp's source code tree directly.
 * In that case it will be enough just to define BT_LOG_LIBRARY_PREFIX in
 * KittyHttp's build script:
 *
 *   // KittyHttp/CMakeLists.txt
 *   target_compile_definitions(KittyHttp PRIVATE
 *                              "BT_LOG_LIBRARY_PREFIX=KittyHttp_")
 *
 * If KittyHttp doesn't want to include bt_log source code in its source tree
 * and wants to build bt_log as a separate library than bt_log library must be
 * built with BT_LOG_LIBRARY_PREFIX defined to KittyHttp_ AND KittyHttp library
 * itself also needs to define BT_LOG_LIBRARY_PREFIX to KittyHttp_. It can do
 * so either in its build script, as in example above, or by providing a
 * wrapper header that KittyHttp library will need to use instead of bt_log.h:
 *
 *   // KittyHttpLogging.h
 *   #define BT_LOG_LIBRARY_PREFIX KittyHttp_
 *   #include <babeltrace/logging-internal.h>
 *
 * Regardless of the method chosen, the end result is that bt_log symbols will
 * be prefixed with "KittyHttp_", so if a user of KittyHttp (say DogeBrowser)
 * also uses bt_log for logging, they will not interferer with each other. Both
 * will have their own log level, output facility, format options etc.
 */
#ifdef BT_LOG_LIBRARY_PREFIX
	#define _BT_LOG_DECOR__(prefix, name) prefix ## name
	#define _BT_LOG_DECOR_(prefix, name) _BT_LOG_DECOR__(prefix, name)
	#define _BT_LOG_DECOR(name) _BT_LOG_DECOR_(BT_LOG_LIBRARY_PREFIX, name)

	#define bt_log_set_tag_prefix _BT_LOG_DECOR(bt_log_set_tag_prefix)
	#define bt_log_set_mem_width _BT_LOG_DECOR(bt_log_set_mem_width)
	#define bt_log_set_output_level _BT_LOG_DECOR(bt_log_set_output_level)
	#define bt_log_set_output_v _BT_LOG_DECOR(bt_log_set_output_v)
	#define bt_log_set_output_p _BT_LOG_DECOR(bt_log_set_output_p)
	#define bt_log_out_stderr_callback _BT_LOG_DECOR(bt_log_out_stderr_callback)
	#define _bt_log_tag_prefix _BT_LOG_DECOR(_bt_log_tag_prefix)
	#define _bt_log_global_format _BT_LOG_DECOR(_bt_log_global_format)
	#define _bt_log_global_output _BT_LOG_DECOR(_bt_log_global_output)
	#define _bt_log_global_output_lvl _BT_LOG_DECOR(_bt_log_global_output_lvl)
	#define _bt_log_write_d _BT_LOG_DECOR(_bt_log_write_d)
	#define _bt_log_write_aux_d _BT_LOG_DECOR(_bt_log_write_aux_d)
	#define _bt_log_write _BT_LOG_DECOR(_bt_log_write)
	#define _bt_log_write_aux _BT_LOG_DECOR(_bt_log_write_aux)
	#define _bt_log_write_mem_d _BT_LOG_DECOR(_bt_log_write_mem_d)
	#define _bt_log_write_mem_aux_d _BT_LOG_DECOR(_bt_log_write_mem_aux_d)
	#define _bt_log_write_mem _BT_LOG_DECOR(_bt_log_write_mem)
	#define _bt_log_write_mem_aux _BT_LOG_DECOR(_bt_log_write_mem_aux)
	#define _bt_log_stderr_spec _BT_LOG_DECOR(_bt_log_stderr_spec)
#endif

#if defined(__printflike)
	#define _BT_LOG_PRINTFLIKE(str_index, first_to_check) \
		__printflike(str_index, first_to_check)
#elif defined(__MINGW_PRINTF_FORMAT)
	#define _BT_LOG_PRINTFLIKE(str_index, first_to_check) \
		__attribute__((format(__MINGW_PRINTF_FORMAT, str_index, first_to_check)))
#elif defined(__GNUC__)
	#define _BT_LOG_PRINTFLIKE(str_index, first_to_check) \
		__attribute__((format(__printf__, str_index, first_to_check)))
#else
	#define _BT_LOG_PRINTFLIKE(str_index, first_to_check)
#endif

#if (defined(_WIN32) || defined(_WIN64)) && !defined(__GNUC__)
	#define _BT_LOG_FUNCTION __FUNCTION__
#else
	#define _BT_LOG_FUNCTION __func__
#endif

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
	#define _BT_LOG_INLINE __inline
	#define _BT_LOG_IF(cond) \
		__pragma(warning(push)) \
		__pragma(warning(disable:4127)) \
		if(cond) \
		__pragma(warning(pop))
	#define _BT_LOG_WHILE(cond) \
		__pragma(warning(push)) \
		__pragma(warning(disable:4127)) \
		while(cond) \
		__pragma(warning(pop))
#else
	#define _BT_LOG_INLINE inline
	#define _BT_LOG_IF(cond) if(cond)
	#define _BT_LOG_WHILE(cond) while(cond)
#endif
#define _BT_LOG_NEVER _BT_LOG_IF(0)
#define _BT_LOG_ONCE _BT_LOG_WHILE(0)

#ifdef __cplusplus
extern "C" {
#endif

/* Set tag prefix. Prefix will be separated from the tag with dot ('.').
 * Use 0 or empty string to disable (default). Common use is to set it to
 * the process (or build target) name (e.g. to separate client and server
 * processes). Function will NOT copy provided prefix string, but will store the
 * pointer. Hence specified prefix string must remain valid. See
 * BT_LOG_DEFINE_TAG_PREFIX for a way to set it before entering main() function.
 * See BT_LOG_TAG for more information about tag and tag prefix.
 */
void bt_log_set_tag_prefix(const char *const prefix);

/* Set number of bytes per log line in memory (ASCII-HEX) output. Example:
 *
 *   I hello.MAIN 4c6f72656d20697073756d20646f6c6f  Lorem ipsum dolo
 *                |<-          w bytes         ->|  |<-  w chars ->|
 *
 * See BT_LOGF_MEM and BT_LOGF_MEM_AUX for more details.
 */
void bt_log_set_mem_width(const unsigned w);

/* Set "output" log level. See BT_LOG_LEVEL and BT_LOG_OUTPUT_LEVEL for more
 * info about log levels.
 */
void bt_log_set_output_level(const int lvl);

/* Put mask is a set of flags that define what fields will be added to each
 * log message. Default value is BT_LOG_PUT_STD and other flags could be used to
 * alter its behavior. See bt_log_set_output_v() for more details.
 *
 * Note about BT_LOG_PUT_SRC: it will be added only in debug builds (NDEBUG is
 * not defined).
 */
enum
{
	BT_LOG_PUT_CTX = 1 << 0, /* context (time, pid, tid, log level) */
	BT_LOG_PUT_TAG = 1 << 1, /* tag (including tag prefix) */
	BT_LOG_PUT_SRC = 1 << 2, /* source location (file, line, function) */
	BT_LOG_PUT_MSG = 1 << 3, /* message text (formatted string) */
	BT_LOG_PUT_STD = 0xffff, /* everything (default) */
};

typedef struct bt_log_message
{
	int lvl; /* Log level of the message */
	const char *tag; /* Associated tag (without tag prefix) */
	char *buf; /* Buffer start */
	char *e; /* Buffer end (last position where EOL with 0 could be written) */
	char *p; /* Buffer content end (append position) */
	char *tag_b; /* Prefixed tag start */
	char *tag_e; /* Prefixed tag end (if != tag_b, points to msg separator) */
	char *msg_b; /* Message start (expanded format string) */
}
bt_log_message;

/* Type of output callback function. It will be called for each log line allowed
 * by both "current" and "output" log levels ("enabled" and "turned on").
 * Callback function is allowed to modify content of the buffers pointed by the
 * msg, but it's not allowed to modify any of msg fields. Buffer pointed by msg
 * is UTF-8 encoded (no BOM mark).
 */
typedef void (*bt_log_output_cb)(const bt_log_message *msg, void *arg);

/* Format options. For more details see bt_log_set_mem_width().
 */
typedef struct bt_log_format
{
	unsigned mem_width; /* Bytes per line in memory (ASCII-HEX) dump */
}
bt_log_format;

/* Output facility.
 */
typedef struct bt_log_output
{
	unsigned mask; /* What to put into log line buffer (see BT_LOG_PUT_XXX) */
	void *arg; /* User provided output callback argument */
	bt_log_output_cb callback; /* Output callback function */
}
bt_log_output;

/* Set output callback function.
 *
 * Mask allows to control what information will be added to the log line buffer
 * before callback function is invoked. Default mask value is BT_LOG_PUT_STD.
 */
void bt_log_set_output_v(const unsigned mask, void *const arg,
						 const bt_log_output_cb callback);
static _BT_LOG_INLINE void bt_log_set_output_p(const bt_log_output *const output)
{
	bt_log_set_output_v(output->mask, output->arg, output->callback);
}

/* Used with _AUX macros and allows to override global format and output
 * facility. Use BT_LOG_GLOBAL_FORMAT and BT_LOG_GLOBAL_OUTPUT for values from
 * global configuration. Example:
 *
 *   static const bt_log_output module_output = {
 *       BT_LOG_PUT_STD, 0, custom_output_callback
 *   };
 *   static const bt_log_spec module_spec = {
 *       BT_LOG_GLOBAL_FORMAT, &module_output
 *   };
 *   BT_LOGI_AUX(&module_spec, "Position: %ix%i", x, y);
 *
 * See BT_LOGF_AUX and BT_LOGF_MEM_AUX for details.
 */
typedef struct bt_log_spec
{
	const bt_log_format *format;
	const bt_log_output *output;
}
bt_log_spec;

#ifdef __cplusplus
}
#endif

/* Execute log statement if condition is true. Example:
 *
 *   BT_LOG_IF(1 < 2, BT_LOGI("Log this"));
 *   BT_LOG_IF(1 > 2, BT_LOGI("Don't log this"));
 *
 * Keep in mind though, that if condition can't be evaluated at compile time,
 * then it will be evaluated at run time. This will increase exectuable size
 * and can have noticeable performance overhead. Try to limit conditions to
 * expressions that can be evaluated at compile time.
 */
#define BT_LOG_IF(cond, f) do { _BT_LOG_IF((cond)) { f; } } _BT_LOG_ONCE

/* Mark log statement as "secret". Log statements that are marked as secrets
 * will NOT be executed when censoring is enabled (see BT_LOG_CENSORED).
 * Example:
 *
 *   BT_LOG_SECRET(BT_LOGI("Credit card: %s", credit_card));
 *   BT_LOG_SECRET(BT_LOGD_MEM(cipher, cipher_sz, "Cipher bytes:"));
 */
#define BT_LOG_SECRET(f) BT_LOG_IF(BT_LOG_SECRETS, f)

/* Check "current" log level at compile time (ignoring "output" log level).
 * Evaluates to true when specified log level is enabled. For example:
 *
 *   #if BT_LOG_ENABLED_DEBUG
 *       const char *const g_enum_strings[] = {
 *           "enum_value_0", "enum_value_1", "enum_value_2"
 *       };
 *   #endif
 *   // ...
 *   #if BT_LOG_ENABLED_DEBUG
 *       BT_LOGD("enum value: %s", g_enum_strings[v]);
 *   #endif
 *
 * See BT_LOG_LEVEL for details.
 */
#define BT_LOG_ENABLED(lvl)     ((lvl) >= _BT_LOG_LEVEL)
#define BT_LOG_ENABLED_VERBOSE  BT_LOG_ENABLED(BT_LOG_VERBOSE)
#define BT_LOG_ENABLED_DEBUG    BT_LOG_ENABLED(BT_LOG_DEBUG)
#define BT_LOG_ENABLED_INFO     BT_LOG_ENABLED(BT_LOG_INFO)
#define BT_LOG_ENABLED_WARN     BT_LOG_ENABLED(BT_LOG_WARN)
#define BT_LOG_ENABLED_ERROR    BT_LOG_ENABLED(BT_LOG_ERROR)
#define BT_LOG_ENABLED_FATAL    BT_LOG_ENABLED(BT_LOG_FATAL)

/* Check "output" log level at run time (taking into account "current" log
 * level as well). Evaluates to true when specified log level is turned on AND
 * enabled. For example:
 *
 *   if (BT_LOG_ON_DEBUG)
 *   {
 *       char hash[65];
 *       sha256(data_ptr, data_sz, hash);
 *       BT_LOGD("data: len=%u, sha256=%s", data_sz, hash);
 *   }
 *
 * See BT_LOG_OUTPUT_LEVEL for details.
 */
#define BT_LOG_ON(lvl) \
		(BT_LOG_ENABLED((lvl)) && (lvl) >= _BT_LOG_OUTPUT_LEVEL)
#define BT_LOG_ON_VERBOSE   BT_LOG_ON(BT_LOG_VERBOSE)
#define BT_LOG_ON_DEBUG     BT_LOG_ON(BT_LOG_DEBUG)
#define BT_LOG_ON_INFO      BT_LOG_ON(BT_LOG_INFO)
#define BT_LOG_ON_WARN      BT_LOG_ON(BT_LOG_WARN)
#define BT_LOG_ON_ERROR     BT_LOG_ON(BT_LOG_ERROR)
#define BT_LOG_ON_FATAL     BT_LOG_ON(BT_LOG_FATAL)

#ifdef __cplusplus
extern "C" {
#endif

extern const char *_bt_log_tag_prefix;
extern bt_log_format _bt_log_global_format;
extern bt_log_output _bt_log_global_output;
extern int _bt_log_global_output_lvl;
extern const bt_log_spec _bt_log_stderr_spec;

BT_HIDDEN
void _bt_log_write_d(
		const char *const func, const char *const file, const unsigned line,
		const int lvl, const char *const tag,
		const char *const fmt, ...) _BT_LOG_PRINTFLIKE(6, 7);

BT_HIDDEN
void _bt_log_write_aux_d(
		const char *const func, const char *const file, const unsigned line,
		const bt_log_spec *const log, const int lvl, const char *const tag,
		const char *const fmt, ...) _BT_LOG_PRINTFLIKE(7, 8);

BT_HIDDEN
void _bt_log_write(
		const int lvl, const char *const tag,
		const char *const fmt, ...) _BT_LOG_PRINTFLIKE(3, 4);

BT_HIDDEN
void _bt_log_write_aux(
		const bt_log_spec *const log, const int lvl, const char *const tag,
		const char *const fmt, ...) _BT_LOG_PRINTFLIKE(4, 5);

BT_HIDDEN
void _bt_log_write_mem_d(
		const char *const func, const char *const file, const unsigned line,
		const int lvl, const char *const tag,
		const void *const d, const unsigned d_sz,
		const char *const fmt, ...) _BT_LOG_PRINTFLIKE(8, 9);

BT_HIDDEN
void _bt_log_write_mem_aux_d(
		const char *const func, const char *const file, const unsigned line,
		const bt_log_spec *const log, const int lvl, const char *const tag,
		const void *const d, const unsigned d_sz,
		const char *const fmt, ...) _BT_LOG_PRINTFLIKE(9, 10);

BT_HIDDEN
void _bt_log_write_mem(
		const int lvl, const char *const tag,
		const void *const d, const unsigned d_sz,
		const char *const fmt, ...) _BT_LOG_PRINTFLIKE(5, 6);

BT_HIDDEN
void _bt_log_write_mem_aux(
		const bt_log_spec *const log, const int lvl, const char *const tag,
		const void *const d, const unsigned d_sz,
		const char *const fmt, ...) _BT_LOG_PRINTFLIKE(6, 7);

#ifdef __cplusplus
}
#endif

/* Message logging macros:
 * - BT_LOGV("format string", args, ...)
 * - BT_LOGD("format string", args, ...)
 * - BT_LOGI("format string", args, ...)
 * - BT_LOGW("format string", args, ...)
 * - BT_LOGE("format string", args, ...)
 * - BT_LOGF("format string", args, ...)
 *
 * Memory logging macros:
 * - BT_LOGV_MEM(data_ptr, data_sz, "format string", args, ...)
 * - BT_LOGD_MEM(data_ptr, data_sz, "format string", args, ...)
 * - BT_LOGI_MEM(data_ptr, data_sz, "format string", args, ...)
 * - BT_LOGW_MEM(data_ptr, data_sz, "format string", args, ...)
 * - BT_LOGE_MEM(data_ptr, data_sz, "format string", args, ...)
 * - BT_LOGF_MEM(data_ptr, data_sz, "format string", args, ...)
 *
 * Auxiliary logging macros:
 * - BT_LOGV_AUX(&log_instance, "format string", args, ...)
 * - BT_LOGD_AUX(&log_instance, "format string", args, ...)
 * - BT_LOGI_AUX(&log_instance, "format string", args, ...)
 * - BT_LOGW_AUX(&log_instance, "format string", args, ...)
 * - BT_LOGE_AUX(&log_instance, "format string", args, ...)
 * - BT_LOGF_AUX(&log_instance, "format string", args, ...)
 *
 * Auxiliary memory logging macros:
 * - BT_LOGV_MEM_AUX(&log_instance, data_ptr, data_sz, "format string", args, ...)
 * - BT_LOGD_MEM_AUX(&log_instance, data_ptr, data_sz, "format string", args, ...)
 * - BT_LOGI_MEM_AUX(&log_instance, data_ptr, data_sz, "format string", args, ...)
 * - BT_LOGW_MEM_AUX(&log_instance, data_ptr, data_sz, "format string", args, ...)
 * - BT_LOGE_MEM_AUX(&log_instance, data_ptr, data_sz, "format string", args, ...)
 * - BT_LOGF_MEM_AUX(&log_instance, data_ptr, data_sz, "format string", args, ...)
 *
 * Preformatted string logging macros:
 * - BT_LOGV_STR("preformatted string");
 * - BT_LOGD_STR("preformatted string");
 * - BT_LOGI_STR("preformatted string");
 * - BT_LOGW_STR("preformatted string");
 * - BT_LOGE_STR("preformatted string");
 * - BT_LOGF_STR("preformatted string");
 *
 * Explicit log level and tag macros:
 * - BT_LOG_WRITE(level, tag, "format string", args, ...)
 * - BT_LOG_WRITE_MEM(level, tag, data_ptr, data_sz, "format string", args, ...)
 * - BT_LOG_WRITE_AUX(&log_instance, level, tag, "format string", args, ...)
 * - BT_LOG_WRITE_MEM_AUX(&log_instance, level, tag, data_ptr, data_sz,
 *                        "format string", args, ...)
 *
 * Format string follows printf() conventions. Both data_ptr and data_sz could
 * be 0. Tag can be 0 as well. Most compilers will verify that type of arguments
 * match format specifiers in format string.
 *
 * Library assuming UTF-8 encoding for all strings (char *), including format
 * string itself.
 */
#if BT_LOG_SRCLOC_NONE == _BT_LOG_SRCLOC
	#define BT_LOG_WRITE(lvl, tag, ...) \
			do { \
				if (BT_LOG_ON(lvl)) \
					_bt_log_write(lvl, tag, __VA_ARGS__); \
			} _BT_LOG_ONCE
	#define BT_LOG_WRITE_MEM(lvl, tag, d, d_sz, ...) \
			do { \
				if (BT_LOG_ON(lvl)) \
					_bt_log_write_mem(lvl, tag, d, d_sz, __VA_ARGS__); \
			} _BT_LOG_ONCE
	#define BT_LOG_WRITE_AUX(log, lvl, tag, ...) \
			do { \
				if (BT_LOG_ON(lvl)) \
					_bt_log_write_aux(log, lvl, tag, __VA_ARGS__); \
			} _BT_LOG_ONCE
	#define BT_LOG_WRITE_MEM_AUX(log, lvl, tag, d, d_sz, ...) \
			do { \
				if (BT_LOG_ON(lvl)) \
					_bt_log_write_mem_aux(log, lvl, tag, d, d_sz, __VA_ARGS__); \
			} _BT_LOG_ONCE
#else
	#define BT_LOG_WRITE(lvl, tag, ...) \
			do { \
				if (BT_LOG_ON(lvl)) \
					_bt_log_write_d(_BT_LOG_SRCLOC_FUNCTION, __FILE__, __LINE__, \
							lvl, tag, __VA_ARGS__); \
			} _BT_LOG_ONCE
	#define BT_LOG_WRITE_MEM(lvl, tag, d, d_sz, ...) \
			do { \
				if (BT_LOG_ON(lvl)) \
					_bt_log_write_mem_d(_BT_LOG_SRCLOC_FUNCTION, __FILE__, __LINE__, \
							lvl, tag, d, d_sz, __VA_ARGS__); \
			} _BT_LOG_ONCE
	#define BT_LOG_WRITE_AUX(log, lvl, tag, ...) \
			do { \
				if (BT_LOG_ON(lvl)) \
					_bt_log_write_aux_d(_BT_LOG_SRCLOC_FUNCTION, __FILE__, __LINE__, \
							log, lvl, tag, __VA_ARGS__); \
			} _BT_LOG_ONCE
	#define BT_LOG_WRITE_MEM_AUX(log, lvl, tag, d, d_sz, ...) \
			do { \
				if (BT_LOG_ON(lvl)) \
					_bt_log_write_mem_aux_d(_BT_LOG_SRCLOC_FUNCTION, __FILE__, __LINE__, \
							log, lvl, tag, d, d_sz, __VA_ARGS__); \
			} _BT_LOG_ONCE
#endif

static _BT_LOG_INLINE void _bt_log_unused(const int dummy, ...) {(void)dummy;}

#define _BT_LOG_UNUSED(...) \
		do { _BT_LOG_NEVER _bt_log_unused(0, __VA_ARGS__); } _BT_LOG_ONCE

#if BT_LOG_ENABLED_VERBOSE
	#define BT_LOGV(...) \
			BT_LOG_WRITE(BT_LOG_VERBOSE, _BT_LOG_TAG, __VA_ARGS__)
	#define BT_LOGV_AUX(log, ...) \
			BT_LOG_WRITE_AUX(log, BT_LOG_VERBOSE, _BT_LOG_TAG, __VA_ARGS__)
	#define BT_LOGV_MEM(d, d_sz, ...) \
			BT_LOG_WRITE_MEM(BT_LOG_VERBOSE, _BT_LOG_TAG, d, d_sz, __VA_ARGS__)
	#define BT_LOGV_MEM_AUX(log, d, d_sz, ...) \
			BT_LOG_WRITE_MEM(log, BT_LOG_VERBOSE, _BT_LOG_TAG, d, d_sz, __VA_ARGS__)
#else
	#define BT_LOGV(...) _BT_LOG_UNUSED(__VA_ARGS__)
	#define BT_LOGV_AUX(...) _BT_LOG_UNUSED(__VA_ARGS__)
	#define BT_LOGV_MEM(...) _BT_LOG_UNUSED(__VA_ARGS__)
	#define BT_LOGV_MEM_AUX(...) _BT_LOG_UNUSED(__VA_ARGS__)
#endif

#if BT_LOG_ENABLED_DEBUG
	#define BT_LOGD(...) \
			BT_LOG_WRITE(BT_LOG_DEBUG, _BT_LOG_TAG, __VA_ARGS__)
	#define BT_LOGD_AUX(log, ...) \
			BT_LOG_WRITE_AUX(log, BT_LOG_DEBUG, _BT_LOG_TAG, __VA_ARGS__)
	#define BT_LOGD_MEM(d, d_sz, ...) \
			BT_LOG_WRITE_MEM(BT_LOG_DEBUG, _BT_LOG_TAG, d, d_sz, __VA_ARGS__)
	#define BT_LOGD_MEM_AUX(log, d, d_sz, ...) \
			BT_LOG_WRITE_MEM_AUX(log, BT_LOG_DEBUG, _BT_LOG_TAG, d, d_sz, __VA_ARGS__)
#else
	#define BT_LOGD(...) _BT_LOG_UNUSED(__VA_ARGS__)
	#define BT_LOGD_AUX(...) _BT_LOG_UNUSED(__VA_ARGS__)
	#define BT_LOGD_MEM(...) _BT_LOG_UNUSED(__VA_ARGS__)
	#define BT_LOGD_MEM_AUX(...) _BT_LOG_UNUSED(__VA_ARGS__)
#endif

#if BT_LOG_ENABLED_INFO
	#define BT_LOGI(...) \
			BT_LOG_WRITE(BT_LOG_INFO, _BT_LOG_TAG, __VA_ARGS__)
	#define BT_LOGI_AUX(log, ...) \
			BT_LOG_WRITE_AUX(log, BT_LOG_INFO, _BT_LOG_TAG, __VA_ARGS__)
	#define BT_LOGI_MEM(d, d_sz, ...) \
			BT_LOG_WRITE_MEM(BT_LOG_INFO, _BT_LOG_TAG, d, d_sz, __VA_ARGS__)
	#define BT_LOGI_MEM_AUX(log, d, d_sz, ...) \
			BT_LOG_WRITE_MEM_AUX(log, BT_LOG_INFO, _BT_LOG_TAG, d, d_sz, __VA_ARGS__)
#else
	#define BT_LOGI(...) _BT_LOG_UNUSED(__VA_ARGS__)
	#define BT_LOGI_AUX(...) _BT_LOG_UNUSED(__VA_ARGS__)
	#define BT_LOGI_MEM(...) _BT_LOG_UNUSED(__VA_ARGS__)
	#define BT_LOGI_MEM_AUX(...) _BT_LOG_UNUSED(__VA_ARGS__)
#endif

#if BT_LOG_ENABLED_WARN
	#define BT_LOGW(...) \
			BT_LOG_WRITE(BT_LOG_WARN, _BT_LOG_TAG, __VA_ARGS__)
	#define BT_LOGW_AUX(log, ...) \
			BT_LOG_WRITE_AUX(log, BT_LOG_WARN, _BT_LOG_TAG, __VA_ARGS__)
	#define BT_LOGW_MEM(d, d_sz, ...) \
			BT_LOG_WRITE_MEM(BT_LOG_WARN, _BT_LOG_TAG, d, d_sz, __VA_ARGS__)
	#define BT_LOGW_MEM_AUX(log, d, d_sz, ...) \
			BT_LOG_WRITE_MEM_AUX(log, BT_LOG_WARN, _BT_LOG_TAG, d, d_sz, __VA_ARGS__)
#else
	#define BT_LOGW(...) _BT_LOG_UNUSED(__VA_ARGS__)
	#define BT_LOGW_AUX(...) _BT_LOG_UNUSED(__VA_ARGS__)
	#define BT_LOGW_MEM(...) _BT_LOG_UNUSED(__VA_ARGS__)
	#define BT_LOGW_MEM_AUX(...) _BT_LOG_UNUSED(__VA_ARGS__)
#endif

#if BT_LOG_ENABLED_ERROR
	#define BT_LOGE(...) \
			BT_LOG_WRITE(BT_LOG_ERROR, _BT_LOG_TAG, __VA_ARGS__)
	#define BT_LOGE_AUX(log, ...) \
			BT_LOG_WRITE_AUX(log, BT_LOG_ERROR, _BT_LOG_TAG, __VA_ARGS__)
	#define BT_LOGE_MEM(d, d_sz, ...) \
			BT_LOG_WRITE_MEM(BT_LOG_ERROR, _BT_LOG_TAG, d, d_sz, __VA_ARGS__)
	#define BT_LOGE_MEM_AUX(log, d, d_sz, ...) \
			BT_LOG_WRITE_MEM_AUX(log, BT_LOG_ERROR, _BT_LOG_TAG, d, d_sz, __VA_ARGS__)
#else
	#define BT_LOGE(...) _BT_LOG_UNUSED(__VA_ARGS__)
	#define BT_LOGE_AUX(...) _BT_LOG_UNUSED(__VA_ARGS__)
	#define BT_LOGE_MEM(...) _BT_LOG_UNUSED(__VA_ARGS__)
	#define BT_LOGE_MEM_AUX(...) _BT_LOG_UNUSED(__VA_ARGS__)
#endif

#if BT_LOG_ENABLED_FATAL
	#define BT_LOGF(...) \
			BT_LOG_WRITE(BT_LOG_FATAL, _BT_LOG_TAG, __VA_ARGS__)
	#define BT_LOGF_AUX(log, ...) \
			BT_LOG_WRITE_AUX(log, BT_LOG_FATAL, _BT_LOG_TAG, __VA_ARGS__)
	#define BT_LOGF_MEM(d, d_sz, ...) \
			BT_LOG_WRITE_MEM(BT_LOG_FATAL, _BT_LOG_TAG, d, d_sz, __VA_ARGS__)
	#define BT_LOGF_MEM_AUX(log, d, d_sz, ...) \
			BT_LOG_WRITE_MEM_AUX(log, BT_LOG_FATAL, _BT_LOG_TAG, d, d_sz, __VA_ARGS__)
#else
	#define BT_LOGF(...) _BT_LOG_UNUSED(__VA_ARGS__)
	#define BT_LOGF_AUX(...) _BT_LOG_UNUSED(__VA_ARGS__)
	#define BT_LOGF_MEM(...) _BT_LOG_UNUSED(__VA_ARGS__)
	#define BT_LOGF_MEM_AUX(...) _BT_LOG_UNUSED(__VA_ARGS__)
#endif

#define BT_LOGV_STR(s) BT_LOGV("%s", (s))
#define BT_LOGD_STR(s) BT_LOGD("%s", (s))
#define BT_LOGI_STR(s) BT_LOGI("%s", (s))
#define BT_LOGW_STR(s) BT_LOGW("%s", (s))
#define BT_LOGE_STR(s) BT_LOGE("%s", (s))
#define BT_LOGF_STR(s) BT_LOGF("%s", (s))

#ifdef __cplusplus
extern "C" {
#endif

/* Output to standard error stream. Library uses it by default, though in few
 * cases it could be necessary to specify it explicitly. For example, when
 * bt_log library is compiled with BT_LOG_EXTERN_GLOBAL_OUTPUT, application must
 * define and initialize global output variable:
 *
 *   BT_LOG_DEFINE_GLOBAL_OUTPUT = {BT_LOG_OUT_STDERR};
 *
 * Another example is when using custom output, stderr could be used as a
 * fallback when custom output facility failed to initialize:
 *
 *   bt_log_set_output_v(BT_LOG_OUT_STDERR);
 */
enum { BT_LOG_OUT_STDERR_MASK = BT_LOG_PUT_STD };
void bt_log_out_stderr_callback(const bt_log_message *const msg, void *arg);
#define BT_LOG_OUT_STDERR BT_LOG_OUT_STDERR_MASK, 0, bt_log_out_stderr_callback

/* Predefined spec for stderr. Uses global format options (BT_LOG_GLOBAL_FORMAT)
 * and BT_LOG_OUT_STDERR. Could be used to force output to stderr for a
 * particular message. Example:
 *
 *   f = fopen("foo.log", "w");
 *   if (!f)
 *       BT_LOGE_AUX(BT_LOG_STDERR, "Failed to open log file");
 */
#define BT_LOG_STDERR (&_bt_log_stderr_spec)

static inline
int bt_log_get_level_from_env(const char *var)
{
	const char *varval = getenv(var);
	int level = BT_LOG_NONE;

	if (!varval) {
		goto end;
	}

	if (strcmp(varval, "VERBOSE") == 0 ||
			strcmp(varval, "V") == 0) {
		level = BT_LOG_VERBOSE;
	} else if (strcmp(varval, "DEBUG") == 0 ||
			strcmp(varval, "D") == 0) {
		level = BT_LOG_DEBUG;
	} else if (strcmp(varval, "INFO") == 0 ||
			strcmp(varval, "I") == 0) {
		level = BT_LOG_INFO;
	} else if (strcmp(varval, "WARN") == 0 ||
			strcmp(varval, "WARNING") == 0 ||
			strcmp(varval, "W") == 0) {
		level = BT_LOG_WARN;
	} else if (strcmp(varval, "ERROR") == 0 ||
			strcmp(varval, "E") == 0) {
		level = BT_LOG_ERROR;
	} else if (strcmp(varval, "FATAL") == 0 ||
			strcmp(varval, "F") == 0) {
		level = BT_LOG_FATAL;
	} else if (strcmp(varval, "NONE") == 0 ||
			strcmp(varval, "N") == 0) {
		level = BT_LOG_NONE;
	} else {
		/* Should we warn here? How? */
	}

end:
	return level;
}

#define BT_LOG_LEVEL_EXTERN_SYMBOL(_level_sym)				\
	extern int _level_sym

#define BT_LOG_INIT_LOG_LEVEL(_level_sym, _env_var)			\
	BT_HIDDEN int _level_sym = BT_LOG_NONE;				\
	static								\
	void __attribute__((constructor)) _bt_log_level_ctor(void)	\
	{								\
		_level_sym = bt_log_get_level_from_env(_env_var);	\
	}

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_LOGGING_INTERNAL_H */
