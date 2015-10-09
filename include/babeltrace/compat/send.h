#ifndef _BABELTRACE_COMPAT_SEND_H
#define _BABELTRACE_COMPAT_SEND_H

/*
 * babeltrace/compat/send.h
 *
 * Copyright (C) 2015  Michael Jeanson <mjeanson@efficios.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * This wrapper is used on platforms that have no way of ignoring SIGPIPE
 * during a send(). Instead, we set the signal action to ignore. This is OK
 * in a single-threaded app, but would be problematic in a multi-threaded app
 * since sigaction applies to all threads.
 */

#ifndef MSG_NOSIGNAL
# ifdef SO_NOSIGPIPE
#   define MSG_NOSIGNAL SO_NOSIGPIPE
# endif
#endif

#if defined(MSG_NOSIGNAL)
static inline
ssize_t bt_send_nosigpipe(int fd, const void *buffer, size_t size)
{
	return send(fd, buffer, size, MSG_NOSIGNAL);
}
#else
static inline
ssize_t bt_send_nosigpipe(int fd, const void *buffer, size_t size)
{
	ssize_t sent;
	int saved_err;
	struct sigaction act, oldact;

	/* Set SIGPIPE action to ignore and save current signal action */
	act.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &act, &oldact)) {
		perror("sigaction");
		sent = -1;
		goto end;
	}

	/* Send and save errno */
	sent = send(fd, buffer, size, 0);
	saved_err = errno;

	/* Restore original signal action */
	if (sigaction(SIGPIPE, &oldact, NULL)) {
		perror("sigaction");
		sent = -1;
		goto end;
	}

	/* Restore send() errno */
	errno = saved_err;
end:
	return sent;
}
#endif

#endif /* _BABELTRACE_COMPAT_SEND_H */
