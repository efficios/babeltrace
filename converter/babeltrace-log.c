/*
 * babeltrace-log.c
 *
 * BabelTrace - Convert Text Log to CTF
 *
 * Copyright 2010-2011 EfficiOS Inc. and Linux Foundation
 *
 * Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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
 *
 * Depends on glibc 2.10 for getline().
 */

#define _GNU_SOURCE
#include <config.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>

#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/ctf/types.h>
#include <babeltrace/uuid.h>
#include <babeltrace/endian.h>

#define USEC_PER_SEC 1000000UL

int babeltrace_debug, babeltrace_verbose;

static char *s_outputname;
static int s_timestamp;
static int s_help;
static unsigned char s_uuid[BABELTRACE_UUID_LEN];

/* Metadata format string */
static const char metadata_fmt[] =
"/* CTF 1.8 */\n"
"typealias integer { size = 8; align = 8; signed = false; } := uint8_t;\n"
"typealias integer { size = 32; align = 32; signed = false; } := uint32_t;\n"
"typealias integer { size = 64; align = 64; signed = false; } := uint64_t;\n"
"\n"
"trace {\n"
"	major = %u;\n"			/* major (e.g. 0) */
"	minor = %u;\n"			/* minor (e.g. 1) */
"	uuid = \"%s\";\n"		/* UUID */
"	byte_order = %s;\n"		/* be or le */
"	packet.header := struct {\n"
"		uint32_t magic;\n"
"		uint8_t  uuid[16];\n"
"	};\n"
"};\n"
"\n"
"stream {\n"
"	packet.context := struct {\n"
"		uint64_t content_size;\n"
"		uint64_t packet_size;\n"
"	};\n"
"%s"					/* Stream event header (opt.) */
"};\n"
"\n"
"event {\n"
"	name = string;\n"
"	fields := struct { string str; };\n"
"};\n";

static const char metadata_stream_event_header_timestamp[] =
"	typealias integer { size = 64; align = 64; signed = false; } := uint64_t;\n"
"	event.header := struct {\n"
"		uint64_t timestamp;\n"
"	};\n";

static
void print_metadata(FILE *fp)
{
	char uuid_str[BABELTRACE_UUID_STR_LEN];
	unsigned int major = 0, minor = 0;
	int ret;

	ret = sscanf(VERSION, "%u.%u", &major, &minor);
	if (ret != 2)
		fprintf(stderr, "[warning] Incorrect babeltrace version format\n.");
	babeltrace_uuid_unparse(s_uuid, uuid_str);
	fprintf(fp, metadata_fmt,
		major,
		minor,
		uuid_str,
		BYTE_ORDER == LITTLE_ENDIAN ? "le" : "be",
		s_timestamp ? metadata_stream_event_header_timestamp : "");
}

static
void write_packet_header(struct ctf_stream_pos *pos, unsigned char *uuid)
{
	struct ctf_stream_pos dummy;

	/* magic */
	ctf_dummy_pos(pos, &dummy);
	ctf_align_pos(&dummy, sizeof(uint32_t) * CHAR_BIT);
	ctf_move_pos(&dummy, sizeof(uint32_t) * CHAR_BIT);
	assert(!ctf_pos_packet(&dummy));
	
	ctf_align_pos(pos, sizeof(uint32_t) * CHAR_BIT);
	*(uint32_t *) ctf_get_pos_addr(pos) = 0xC1FC1FC1;
	ctf_move_pos(pos, sizeof(uint32_t) * CHAR_BIT);

	/* uuid */
	ctf_dummy_pos(pos, &dummy);
	ctf_align_pos(&dummy, sizeof(uint8_t) * CHAR_BIT);
	ctf_move_pos(&dummy, 16 * CHAR_BIT);
	assert(!ctf_pos_packet(&dummy));

	ctf_align_pos(pos, sizeof(uint8_t) * CHAR_BIT);
	memcpy(ctf_get_pos_addr(pos), uuid, BABELTRACE_UUID_LEN);
	ctf_move_pos(pos, BABELTRACE_UUID_LEN * CHAR_BIT);
}

static
void write_packet_context(struct ctf_stream_pos *pos)
{
	struct ctf_stream_pos dummy;

	/* content_size */
	ctf_dummy_pos(pos, &dummy);
	ctf_align_pos(&dummy, sizeof(uint64_t) * CHAR_BIT);
	ctf_move_pos(&dummy, sizeof(uint64_t) * CHAR_BIT);
	assert(!ctf_pos_packet(&dummy));
	
	ctf_align_pos(pos, sizeof(uint64_t) * CHAR_BIT);
	*(uint64_t *) ctf_get_pos_addr(pos) = ~0ULL;	/* Not known yet */
	pos->content_size_loc = (uint64_t *) ctf_get_pos_addr(pos);
	ctf_move_pos(pos, sizeof(uint64_t) * CHAR_BIT);

	/* packet_size */
	ctf_dummy_pos(pos, &dummy);
	ctf_align_pos(&dummy, sizeof(uint64_t) * CHAR_BIT);
	ctf_move_pos(&dummy, sizeof(uint64_t) * CHAR_BIT);
	assert(!ctf_pos_packet(&dummy));
	
	ctf_align_pos(pos, sizeof(uint64_t) * CHAR_BIT);
	*(uint64_t *) ctf_get_pos_addr(pos) = pos->packet_size;
	ctf_move_pos(pos, sizeof(uint64_t) * CHAR_BIT);
}

static
void write_event_header(struct ctf_stream_pos *pos, char *line,
			char **tline, size_t len, size_t *tlen,
			uint64_t *ts)
{
	unsigned long sec, usec;

	if (!s_timestamp)
		return;

	/* Only need to be executed on first pass (dummy) */
	if (pos->dummy) {
		int ret;

		/* Extract time from input line */
		ret = sscanf(line, "[%lu.%lu] ", &sec, &usec);
		if (ret == 2) {
			*tline = strchr(line, ']');
			assert(*tline);
			(*tline)++;
			if ((*tline)[0] == ' ') {
				(*tline)++;
			}
			*tlen = len + line - *tline;
			*ts = (uint64_t) sec * USEC_PER_SEC + (uint64_t) usec;
		}
	}
	/* timestamp */
	ctf_align_pos(pos, sizeof(uint64_t) * CHAR_BIT);
	if (!pos->dummy)
		*(uint64_t *) ctf_get_pos_addr(pos) = *ts;
	ctf_move_pos(pos, sizeof(uint64_t) * CHAR_BIT);
}

static
void trace_string(char *line, struct ctf_stream_pos *pos, size_t len)
{
	struct ctf_stream_pos dummy;
	int attempt = 0;
	char *tline = line;	/* tline is start of text, after timestamp */
	size_t tlen = len;
	uint64_t ts = 0;

	printf_debug("read: %s\n", line);

	for (;;) {
		ctf_dummy_pos(pos, &dummy);
		write_event_header(&dummy, line, &tline, len, &tlen, &ts);
		ctf_align_pos(&dummy, sizeof(uint8_t) * CHAR_BIT);
		ctf_move_pos(&dummy, tlen * CHAR_BIT);
		if (ctf_pos_packet(&dummy)) {
			ctf_pos_pad_packet(pos);
			write_packet_header(pos, s_uuid);
			write_packet_context(pos);
			if (attempt++ == 1) {
				fprintf(stderr, "[Error] Line too large for packet size (%" PRIu64 "kB) (discarded)\n",
					pos->packet_size / CHAR_BIT / 1024);
				return;
			}
			continue;
		} else {
			break;
		}
	}

	write_event_header(pos, line, &tline, len, &tlen, &ts);
	ctf_align_pos(pos, sizeof(uint8_t) * CHAR_BIT);
	memcpy(ctf_get_pos_addr(pos), tline, tlen);
	ctf_move_pos(pos, tlen * CHAR_BIT);
}

static
void trace_text(FILE *input, int output)
{
	struct ctf_stream_pos pos;
	ssize_t len;
	char *line = NULL, *nl;
	size_t linesize;
	int ret;

	memset(&pos, 0, sizeof(pos));
	ret = ctf_init_pos(&pos, output, O_RDWR);
	if (ret) {
		fprintf(stderr, "Error in ctf_init_pos\n");
		return;
	}
	write_packet_header(&pos, s_uuid);
	write_packet_context(&pos);
	for (;;) {
		len = getline(&line, &linesize, input);
		if (len < 0)
			break;
		nl = strrchr(line, '\n');
		if (nl) {
			*nl = '\0';
			trace_string(line, &pos, nl - line + 1);
		} else {
			trace_string(line, &pos, strlen(line) + 1);
		}
	}
	ret = ctf_fini_pos(&pos);
	if (ret) {
		fprintf(stderr, "Error in ctf_fini_pos\n");
	}
}

static
void usage(FILE *fp)
{
	fprintf(fp, "BabelTrace Log Converter %s\n", VERSION);
	fprintf(fp, "\n");
	fprintf(fp, "Convert for a text log (read from standard input) to CTF.\n");
	fprintf(fp, "\n");
	fprintf(fp, "usage : babeltrace-log [OPTIONS] OUTPUT\n");
	fprintf(fp, "\n");
	fprintf(fp, "  OUTPUT                         Output trace path\n");
	fprintf(fp, "\n");
	fprintf(fp, "  -t                             With timestamps (format: [sec.usec] string\\n)\n");
	fprintf(fp, "\n");
}

static
int parse_args(int argc, char **argv)
{
	int i;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-t"))
			s_timestamp = 1;
		else if (!strcmp(argv[i], "-h")) {
			s_help = 1;
			return 0;
		} else if (argv[i][0] == '-')
			return -EINVAL;
		else
			s_outputname = argv[i];
	}
	if (!s_outputname)
		return -EINVAL;
	return 0;
}

int main(int argc, char **argv)
{
	int fd, metadata_fd, ret;
	DIR *dir;
	int dir_fd;
	FILE *metadata_fp;

	ret = parse_args(argc, argv);
	if (ret) {
		fprintf(stderr, "Error: invalid argument.\n");
		usage(stderr);
		goto error;
	}

	if (s_help) {
		usage(stdout);
		exit(EXIT_SUCCESS);
	}

	ret = mkdir(s_outputname, S_IRWXU|S_IRWXG);
	if (ret) {
		perror("mkdir");
		goto error;
	}

	dir = opendir(s_outputname);
	if (!dir) {
		perror("opendir");
		goto error_rmdir;
	}
	dir_fd = dirfd(dir);
	if (dir_fd < 0) {
		perror("dirfd");
		goto error_closedir;
	}

	fd = openat(dir_fd, "datastream", O_RDWR|O_CREAT,
		    S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
	if (fd < 0) {
		perror("openat");
		goto error_closedirfd;
	}

	metadata_fd = openat(dir_fd, "metadata", O_RDWR|O_CREAT,
			     S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
	if (fd < 0) {
		perror("openat");
		goto error_closedatastream;
	}
	metadata_fp = fdopen(metadata_fd, "w");
	if (!metadata_fp) {
		perror("fdopen");
		goto error_closemetadatafd;
	}

	babeltrace_uuid_generate(s_uuid);
	print_metadata(metadata_fp);
	trace_text(stdin, fd);

	ret = close(fd);
	if (ret)
		perror("close");
	exit(EXIT_SUCCESS);

	/* error handling */
error_closemetadatafd:
	ret = close(metadata_fd);
	if (ret)
		perror("close");
error_closedatastream:
	ret = close(fd);
	if (ret)
		perror("close");
error_closedirfd:
	ret = close(dir_fd);
	if (ret)
		perror("close");
error_closedir:
	ret = closedir(dir);
	if (ret)
		perror("closedir");
error_rmdir:
	ret = rmdir(s_outputname);
	if (ret)
		perror("rmdir");
error:
	exit(EXIT_FAILURE);
}
