#ifndef _BABELTRACE_CTF_METADATA_H
#define _BABELTRACE_CTF_METADATA_H

/*
 * BabelTrace
 *
 * CTF Metadata Header
 *
 * Copyright 2011 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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
 */

#include <babeltrace/types.h>
#include <babeltrace/format.h>
#include <babeltrace/ctf/types.h>
#include <babeltrace/ctf-ir/metadata.h>
#include <babeltrace/trace-handle-internal.h>
#include <babeltrace/context-internal.h>
#include <sys/types.h>
#include <dirent.h>
#include <assert.h>
#include <glib.h>

#define CTF_MAGIC	0xC1FC1FC1
#define TSDL_MAGIC	0x75D11D57

struct ctf_file_stream {
	struct ctf_stream_definition parent;
	struct ctf_stream_pos pos;	/* current stream position */
};

#define HEADER_END		char end_field
#define header_sizeof(type)	offsetof(typeof(type), end_field)

struct metadata_packet_header {
	uint32_t magic;			/* 0x75D11D57 */
	uint8_t  uuid[16];		/* Unique Universal Identifier */
	uint32_t checksum;		/* 0 if unused */
	uint32_t content_size;		/* in bits */
	uint32_t packet_size;		/* in bits */
	uint8_t  compression_scheme;	/* 0 if unused */
	uint8_t  encryption_scheme;	/* 0 if unused */
	uint8_t  checksum_scheme;	/* 0 if unused */
	uint8_t  major;			/* CTF spec major version number */
	uint8_t  minor;			/* CTF spec minor version number */
	HEADER_END;
};

#endif /* _BABELTRACE_CTF_METADATA_H */
