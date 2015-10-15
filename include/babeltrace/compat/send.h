#ifndef _BABELTRACE_COMPAT_SEND_H
#define _BABELTRACE_COMPAT_SEND_H

/*
 * babeltrace/compat/send.h
 *
 * Copyright (C) 2015  Michael Jeanson <mjeanson@efficios.com>
 *               2015  Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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
 * during a send().
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

#include <signal.h>

static inline
ssize_t bt_send_nosigpipe(int fd, const void *buffer, size_t size)
{
	ssize_t sent;
	int saved_err;
	sigset_t sigpipe_set, pending_set, old_set;
	int sigpipe_was_pending;

	/*
	 * Discard the SIGPIPE from send(), not disturbing any SIGPIPE
	 * that might be already pending. If a bogus SIGPIPE is sent to
	 * the entire process concurrently by a malicious user, it may
	 * be simply discarded.
	 */
	if (sigemptyset(&pending_set)) {
		return -1;
	}
	/*
	 * sigpending returns the mask of signals that are _both_
	 * blocked for the thread _and_ pending for either the thread or
	 * the entire process.
	 */
	if (sigpending(&pending_set)) {
		return -1;
	}
	sigpipe_was_pending = sigismember(&pending_set, SIGPIPE);
	/*
	 * If sigpipe was pending, it means it was already blocked, so
	 * no need to block it.
	 */
	if (!sigpipe_was_pending) {
		if (sigemptyset(&sigpipe_set)) {
			return -1;
		}
		if (sigaddset(&sigpipe_set, SIGPIPE)) {
			return -1;
		}
		if (pthread_sigmask(SIG_BLOCK, &sigpipe_set, &old_set)) {
			return -1;
		}
	}

	/* Send and save errno. */
	sent = send(fd, buffer, size, 0);
	saved_err = errno;

	if (sent == -1 && errno == EPIPE && !sigpipe_was_pending) {
		struct timespec timeout = { 0, 0 };
		int ret;

		do {
			ret = sigtimedwait(&sigpipe_set, NULL,
				&timeout);
		} while (ret == -1 && errno == EINTR);
	}
	if (!sigpipe_was_pending) {
		if (pthread_sigmask(SIG_SETMASK, &old_set, NULL)) {
			return -1;
		}
	}
	/* Restore send() errno */
	errno = saved_err;

	return sent;
}
#endif

#endif /* _BABELTRACE_COMPAT_SEND_H */
