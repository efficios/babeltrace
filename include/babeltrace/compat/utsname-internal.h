
#ifndef UTSNAME_H
#define UTSNAME_H

/*
 * Copyright 2011 Martin T. Sandsmark <sandsmark@samfundet.no>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef __MINGW32__

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>

#define UTSNAME_LENGTH 257
struct utsname {
	char sysname[UTSNAME_LENGTH];
	char nodename[UTSNAME_LENGTH];
	char release[UTSNAME_LENGTH];
	char version[UTSNAME_LENGTH];
	char machine[UTSNAME_LENGTH];
};

static inline
int uname(struct utsname *name)
{
	OSVERSIONINFO versionInfo;
	SYSTEM_INFO sysInfo;
	int ret = 0;

	/* Get Windows version info */
	memset(&versionInfo, 0, sizeof(OSVERSIONINFO));
	versionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&versionInfo);

	/* Get hardware info */
	ZeroMemory(&sysInfo, sizeof(SYSTEM_INFO));
	GetSystemInfo(&sysInfo);

	/* Fill the sysname */
	strcpy(name->sysname, "Windows");

	/* Fill the release */
	snprintf(name->release, UTSNAME_LENGTH, "%lu",
			versionInfo.dwBuildNumber);

	/* Fill the version */
	snprintf(name->version, UTSNAME_LENGTH, "%lu.%lu",
			versionInfo.dwMajorVersion,
			versionInfo.dwMinorVersion);

	/* Fill ithe nodename */
	if (gethostname(name->nodename, UTSNAME_LENGTH) != 0) {
		if (WSAGetLastError() == WSANOTINITIALISED) {
			/* WinSock not initialized */
			WSADATA WSAData;
			WSAStartup(MAKEWORD(1, 0), &WSAData);
			gethostname(name->nodename, UTSNAME_LENGTH);
			WSACleanup();
		} else {
			ret = -1;
			goto end;
		}
	}

	/* Fill the machine */
	switch(sysInfo.wProcessorArchitecture) {
	case PROCESSOR_ARCHITECTURE_AMD64:
		strcpy(name->machine, "x86_64");
		break;
	case PROCESSOR_ARCHITECTURE_IA64:
		strcpy(name->machine, "ia64");
		break;
	case PROCESSOR_ARCHITECTURE_INTEL:
		strcpy(name->machine, "x86");
		break;
	case PROCESSOR_ARCHITECTURE_UNKNOWN:
	default:
		strcpy(name->machine, "unknown");
	}

end:
	return ret;
}
#else /* __MINGW32__ */
#include <sys/utsname.h>
#endif /* __MINGW32__ */

#endif //UTSNAME_H
