/*-
 * Copyright (c) 2004 Nik Clayton
 *               2017 Jérémie Galarneau
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* '## __VA_ARGS__' is a gcc'ism. C99 doesn't allow the token pasting
   and requires the caller to add the final comma if they've ommitted
   the optional arguments */
#ifdef __GNUC__
# define ok(e, test, ...) ((e) ?					\
			   _gen_result(1, __func__, __FILE__, __LINE__,	\
				       test, ## __VA_ARGS__) :		\
			   _gen_result(0, __func__, __FILE__, __LINE__,	\
				       test, ## __VA_ARGS__))

# define ok1(e) ((e) ?							\
		 _gen_result(1, __func__, __FILE__, __LINE__, "%s", #e) : \
		 _gen_result(0, __func__, __FILE__, __LINE__, "%s", #e))

# define pass(test, ...) ok(1, test, ## __VA_ARGS__);
# define fail(test, ...) ok(0, test, ## __VA_ARGS__);

# define skip_start(test, n, fmt, ...)			\
	do {						\
		if((test)) {				\
			skip(n, fmt, ## __VA_ARGS__);	\
			continue;			\
		}
#elif __STDC_VERSION__ >= 199901L /* __GNUC__ */
# define ok(e, ...) ((e) ?						\
		     _gen_result(1, __func__, __FILE__, __LINE__,	\
				 __VA_ARGS__) :				\
		     _gen_result(0, __func__, __FILE__, __LINE__,	\
				 __VA_ARGS__))

# define ok1(e) ((e) ?							\
		 _gen_result(1, __func__, __FILE__, __LINE__, "%s", #e) : \
		 _gen_result(0, __func__, __FILE__, __LINE__, "%s", #e))

# define pass(...) ok(1, __VA_ARGS__);
# define fail(...) ok(0, __VA_ARGS__);

# define skip_start(test, n, ...)			\
	do {						\
		if((test)) {				\
			skip(n,  __VA_ARGS__);		\
			continue;			\
		}
#else /* __STDC_VERSION__ */
# error "Needs gcc or C99 compiler for variadic macros."
#endif /* __STDC_VERSION__ */

#define skip_end() } while(0);

#ifdef __MINGW_PRINTF_FORMAT
# define TAP_PRINTF_FORMAT __MINGW_PRINTF_FORMAT
#else
# define TAP_PRINTF_FORMAT printf
#endif

__attribute__((format(TAP_PRINTF_FORMAT, 5, 6)))
unsigned int _gen_result(int, const char *, const char *, unsigned int, const char *, ...);

int plan_no_plan(void);
__attribute__((noreturn))
int plan_skip_all(const char *);
int plan_tests(unsigned int);

__attribute__((format(TAP_PRINTF_FORMAT, 1, 2)))
unsigned int diag(const char *, ...);
void diag_multiline(const char *);

__attribute__((format(TAP_PRINTF_FORMAT, 2, 3)))
int skip(unsigned int, const char *, ...);

__attribute__((format(TAP_PRINTF_FORMAT, 1, 2)))
void todo_start(const char *, ...);
void todo_end(void);

int exit_status(void);
