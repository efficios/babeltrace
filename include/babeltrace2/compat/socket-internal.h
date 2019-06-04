#ifndef _BABELTRACE_COMPAT_SOCKET_H
#define _BABELTRACE_COMPAT_SOCKET_H

/*
 * Copyright (C) 2015-2017  Michael Jeanson <mjeanson@efficios.com>
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

#ifdef __MINGW32__

#include <winsock2.h>

#define BT_INVALID_SOCKET INVALID_SOCKET
#define BT_SOCKET_ERROR SOCKET_ERROR
#define BT_SOCKET SOCKET

static inline
int bt_socket_init(void)
{
	WORD verreq;
	WSADATA wsa;
	int ret;

	/* Request winsock 2.2 support */
	verreq = MAKEWORD(2, 2);

	ret = WSAStartup(verreq, &wsa);
	if (ret != 0) {
#ifdef BT_LOGE
		BT_LOGE("Winsock init failed with error: %d", ret);
#endif
		goto end;
	}

	if (LOBYTE(wsa.wVersion) != 2 || HIBYTE(wsa.wVersion) != 2) {
#ifdef BT_LOGE_STR
		BT_LOGE_STR("Could not init winsock 2.2 support");
#endif
		WSACleanup();
		ret = -1;
	}

end:
	return ret;
}

static inline
int bt_socket_fini(void)
{
	return WSACleanup();
}

static inline
int bt_socket_send(int sockfd, const void *buf, size_t len, int flags)
{
	return send(sockfd, buf, len, flags);
}

static inline
int bt_socket_recv(int sockfd, void *buf, size_t len, int flags)
{
	return recv(sockfd, buf, len, flags);
}

static inline
int bt_socket_close(int fd)
{
	return closesocket(fd);
}

static inline
bool bt_socket_interrupted(void)
{
	/* There is no equivalent to EINTR in winsock 2.2 */
	return false;
}

static inline
const char *bt_socket_errormsg(void)
{
	const char *errstr;
	int error = WSAGetLastError();

	switch (error) {
	case WSAEINTR:
		errstr = "Call interrupted";
		break;
	case WSAEBADF:
		errstr = "Bad file";
		break;
	case WSAEACCES:
		errstr = "Bad access";
		break;
	case WSAEFAULT:
		errstr = "Bad argument";
		break;
	case WSAEINVAL:
		errstr = "Invalid arguments";
		break;
	case WSAEMFILE:
		errstr = "Out of file descriptors";
		break;
	case WSAEWOULDBLOCK:
		errstr = "Call would block";
		break;
	case WSAEINPROGRESS:
	case WSAEALREADY:
		errstr = "Blocking call in progress";
		break;
	case WSAENOTSOCK:
		errstr = "Descriptor is not a socket";
		break;
	case WSAEDESTADDRREQ:
		errstr = "Need destination address";
		break;
	case WSAEMSGSIZE:
		errstr = "Bad message size";
		break;
	case WSAEPROTOTYPE:
		errstr = "Bad protocol";
		break;
	case WSAENOPROTOOPT:
		errstr = "Protocol option is unsupported";
		break;
	case WSAEPROTONOSUPPORT:
		errstr = "Protocol is unsupported";
		break;
	case WSAESOCKTNOSUPPORT:
		errstr = "Socket is unsupported";
		break;
	case WSAEOPNOTSUPP:
		errstr = "Operation not supported";
		break;
	case WSAEAFNOSUPPORT:
		errstr = "Address family not supported";
		break;
	case WSAEPFNOSUPPORT:
		errstr = "Protocol family not supported";
		break;
	case WSAEADDRINUSE:
		errstr = "Address already in use";
		break;
	case WSAEADDRNOTAVAIL:
		errstr = "Address not available";
		break;
	case WSAENETDOWN:
		errstr = "Network down";
		break;
	case WSAENETUNREACH:
		errstr = "Network unreachable";
		break;
	case WSAENETRESET:
		errstr = "Network has been reset";
		break;
	case WSAECONNABORTED:
		errstr = "Connection was aborted";
		break;
	case WSAECONNRESET:
		errstr = "Connection was reset";
		break;
	case WSAENOBUFS:
		errstr = "No buffer space";
		break;
	case WSAEISCONN:
		errstr = "Socket is already connected";
		break;
	case WSAENOTCONN:
		errstr = "Socket is not connected";
		break;
	case WSAESHUTDOWN:
		errstr = "Socket has been shut down";
		break;
	case WSAETOOMANYREFS:
		errstr = "Too many references";
		break;
	case WSAETIMEDOUT:
		errstr = "Timed out";
		break;
	case WSAECONNREFUSED:
		errstr = "Connection refused";
		break;
	case WSAELOOP:
		errstr = "Loop??";
		break;
	case WSAENAMETOOLONG:
		errstr = "Name too long";
		break;
	case WSAEHOSTDOWN:
		errstr = "Host down";
		break;
	case WSAEHOSTUNREACH:
		errstr = "Host unreachable";
		break;
	case WSAENOTEMPTY:
		errstr = "Not empty";
		break;
	case WSAEPROCLIM:
		errstr = "Process limit reached";
		break;
	case WSAEUSERS:
		errstr = "Too many users";
		break;
	case WSAEDQUOT:
		errstr = "Bad quota";
		break;
	case WSAESTALE:
		errstr = "Something is stale";
		break;
	case WSAEREMOTE:
		errstr = "Remote error";
		break;
	case WSAEDISCON:
		errstr = "Disconnected";
		break;

	/* Extended Winsock errors */
	case WSASYSNOTREADY:
		errstr = "Winsock library is not ready";
		break;
	case WSANOTINITIALISED:
		errstr = "Winsock library not initialised";
		break;
	case WSAVERNOTSUPPORTED:
		errstr = "Winsock version not supported";
		break;

	/* getXbyY() errors (already handled in herrmsg):
	 * Authoritative Answer: Host not found */
	case WSAHOST_NOT_FOUND:
		errstr = "Host not found";
		break;

	/* Non-Authoritative: Host not found, or SERVERFAIL */
	case WSATRY_AGAIN:
		errstr = "Host not found, try again";
		break;

	/* Non recoverable errors, FORMERR, REFUSED, NOTIMP */
	case WSANO_RECOVERY:
		errstr = "Unrecoverable error in call to nameserver";
		break;

	/* Valid name, no data record of requested type */
	case WSANO_DATA:
		errstr = "No data record of requested type";
		break;

	default:
		errstr = "Unknown error";
	}

	return errstr;
}

#else /* __MINGW32__ */

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define BT_INVALID_SOCKET -1
#define BT_SOCKET_ERROR -1
#define BT_SOCKET int

static inline
int bt_socket_init(void)
{
	return 0;
}

static inline
int bt_socket_fini(void)
{
	return 0;
}

static inline
int bt_socket_send(int sockfd, const void *buf, size_t len, int flags)
{
	return send(sockfd, buf, len, flags);
}

static inline
int bt_socket_recv(int sockfd, void *buf, size_t len, int flags)
{
	return recv(sockfd, buf, len, flags);
}

static inline
int bt_socket_close(int fd)
{
	return close(fd);
}

static inline
bool bt_socket_interrupted(void)
{
	return (errno == EINTR);
}

static inline
const char *bt_socket_errormsg(void)
{
	return g_strerror(errno);
}
#endif


/*
 * This wrapper is used on platforms that have no way of ignoring SIGPIPE
 * during a send().
 */

#ifndef MSG_NOSIGNAL
# ifdef SO_NOSIGPIPE
#   define MSG_NOSIGNAL SO_NOSIGPIPE
# elif defined(__MINGW32__)
#   define MSG_NOSIGNAL 0
# endif
#endif

#if defined(MSG_NOSIGNAL)
static inline
ssize_t bt_socket_send_nosigpipe(int fd, const void *buffer, size_t size)
{
	return bt_socket_send(fd, buffer, size, MSG_NOSIGNAL);
}
#else

#include <signal.h>

static inline
ssize_t bt_socket_send_nosigpipe(int fd, const void *buffer, size_t size)
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
	sent = bt_socket_send(fd, buffer, size, 0);
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


#endif /* _BABELTRACE_COMPAT_SOCKET_H */
