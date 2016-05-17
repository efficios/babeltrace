/*
 * BabelTrace - Common Trace Format (CTF)
 *
 * Format registration.
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
 */

#include <babeltrace/format.h>
#include <babeltrace/format-internal.h>
#include <babeltrace/ctf/types.h>
#include <babeltrace/ctf/metadata.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/ctf/events-internal.h>
#include <babeltrace/trace-handle-internal.h>
#include <babeltrace/context-internal.h>
#include <babeltrace/compat/uuid.h>
#include <babeltrace/endian.h>
#include <babeltrace/trace-debug-info.h>
#include <babeltrace/ctf/ctf-index.h>
#include <inttypes.h>
#include <stdio.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <glib.h>
#include <unistd.h>
#include <stdlib.h>

#include "metadata/ctf-scanner.h"
#include "metadata/ctf-parser.h"
#include "metadata/ctf-ast.h"
#include "events-private.h"
#include <babeltrace/compat/memstream.h>
#include <babeltrace/compat/fcntl.h>

#define LOG2_CHAR_BIT	3

/*
 * Length of first attempt at mapping a packet header, in bits.
 */
#define DEFAULT_HEADER_LEN	(getpagesize() * CHAR_BIT)

/*
 * Lenght of packet to write, in bits.
 */
#define WRITE_PACKET_LEN	(getpagesize() * 8 * CHAR_BIT)

#define NSEC_PER_SEC 1000000000ULL

#define INDEX_PATH "./index/%s.idx"

int opt_clock_cycles,
	opt_clock_seconds,
	opt_clock_date,
	opt_clock_gmt;

uint64_t opt_clock_offset;
uint64_t opt_clock_offset_ns;

extern int yydebug;
char *opt_debug_info_dir;
char *opt_debug_info_target_prefix;

/*
 * TODO: babeltrace_ctf_console_output ensures that we only print
 * discarded events when ctf-text plugin is used. Should be cleaned up
 * with the plugin system redesign.
 */
int babeltrace_ctf_console_output;

static
struct bt_trace_descriptor *ctf_open_trace(const char *path, int flags,
		void (*packet_seek)(struct bt_stream_pos *pos, size_t index,
			int whence),
		FILE *metadata_fp);
static
struct bt_trace_descriptor *ctf_open_mmap_trace(
		struct bt_mmap_stream_list *mmap_list,
		void (*packet_seek)(struct bt_stream_pos *pos, size_t index,
			int whence),
		FILE *metadata_fp);
static
void ctf_set_context(struct bt_trace_descriptor *descriptor,
		struct bt_context *ctx);
static
void ctf_set_handle(struct bt_trace_descriptor *descriptor,
		struct bt_trace_handle *handle);

static
int ctf_close_trace(struct bt_trace_descriptor *descriptor);
static
uint64_t ctf_timestamp_begin(struct bt_trace_descriptor *descriptor,
		struct bt_trace_handle *handle, enum bt_clock_type type);
static
uint64_t ctf_timestamp_end(struct bt_trace_descriptor *descriptor,
		struct bt_trace_handle *handle, enum bt_clock_type type);
static
int ctf_convert_index_timestamp(struct bt_trace_descriptor *tdp);

static
rw_dispatch read_dispatch_table[] = {
	[ CTF_TYPE_INTEGER ] = ctf_integer_read,
	[ CTF_TYPE_FLOAT ] = ctf_float_read,
	[ CTF_TYPE_ENUM ] = ctf_enum_read,
	[ CTF_TYPE_STRING ] = ctf_string_read,
	[ CTF_TYPE_STRUCT ] = ctf_struct_rw,
	[ CTF_TYPE_VARIANT ] = ctf_variant_rw,
	[ CTF_TYPE_ARRAY ] = ctf_array_read,
	[ CTF_TYPE_SEQUENCE ] = ctf_sequence_read,
};

static
rw_dispatch write_dispatch_table[] = {
	[ CTF_TYPE_INTEGER ] = ctf_integer_write,
	[ CTF_TYPE_FLOAT ] = ctf_float_write,
	[ CTF_TYPE_ENUM ] = ctf_enum_write,
	[ CTF_TYPE_STRING ] = ctf_string_write,
	[ CTF_TYPE_STRUCT ] = ctf_struct_rw,
	[ CTF_TYPE_VARIANT ] = ctf_variant_rw,
	[ CTF_TYPE_ARRAY ] = ctf_array_write,
	[ CTF_TYPE_SEQUENCE ] = ctf_sequence_write,
};

static
struct bt_format ctf_format = {
	.open_trace = ctf_open_trace,
	.open_mmap_trace = ctf_open_mmap_trace,
	.close_trace = ctf_close_trace,
	.set_context = ctf_set_context,
	.set_handle = ctf_set_handle,
	.timestamp_begin = ctf_timestamp_begin,
	.timestamp_end = ctf_timestamp_end,
	.convert_index_timestamp = ctf_convert_index_timestamp,
};

static
uint64_t ctf_timestamp_begin(struct bt_trace_descriptor *descriptor,
		struct bt_trace_handle *handle, enum bt_clock_type type)
{
	struct ctf_trace *tin;
	uint64_t begin = ULLONG_MAX;
	int i, j;

	tin = container_of(descriptor, struct ctf_trace, parent);

	if (!tin)
		goto error;

	/* for each stream_class */
	for (i = 0; i < tin->streams->len; i++) {
		struct ctf_stream_declaration *stream_class;

		stream_class = g_ptr_array_index(tin->streams, i);
		if (!stream_class)
			continue;
		/* for each file_stream */
		for (j = 0; j < stream_class->streams->len; j++) {
			struct ctf_stream_definition *stream;
			struct ctf_file_stream *cfs;
			struct ctf_stream_pos *stream_pos;
			struct packet_index *index;

			stream = g_ptr_array_index(stream_class->streams, j);
			cfs = container_of(stream, struct ctf_file_stream,
					parent);
			stream_pos = &cfs->pos;

			if (!stream_pos->packet_index)
				goto error;

			if (stream_pos->packet_index->len <= 0)
				continue;

			index = &g_array_index(stream_pos->packet_index,
					struct packet_index,
					stream_pos->packet_index->len - 1);
			if (type == BT_CLOCK_REAL) {
				if (index->ts_real.timestamp_begin < begin)
					begin = index->ts_real.timestamp_begin;
			} else if (type == BT_CLOCK_CYCLES) {
				if (index->ts_cycles.timestamp_begin < begin)
					begin = index->ts_cycles.timestamp_begin;
			} else {
				goto error;
			}
		}
	}

	return begin;

error:
	return -1ULL;
}

static
uint64_t ctf_timestamp_end(struct bt_trace_descriptor *descriptor,
		struct bt_trace_handle *handle, enum bt_clock_type type)
{
	struct ctf_trace *tin;
	uint64_t end = 0;
	int i, j;

	tin = container_of(descriptor, struct ctf_trace, parent);

	if (!tin)
		goto error;

	/* for each stream_class */
	for (i = 0; i < tin->streams->len; i++) {
		struct ctf_stream_declaration *stream_class;

		stream_class = g_ptr_array_index(tin->streams, i);
		if (!stream_class)
			continue;
		/* for each file_stream */
		for (j = 0; j < stream_class->streams->len; j++) {
			struct ctf_stream_definition *stream;
			struct ctf_file_stream *cfs;
			struct ctf_stream_pos *stream_pos;
			struct packet_index *index;

			stream = g_ptr_array_index(stream_class->streams, j);
			cfs = container_of(stream, struct ctf_file_stream,
					parent);
			stream_pos = &cfs->pos;

			if (!stream_pos->packet_index)
				goto error;

			if (stream_pos->packet_index->len <= 0)
				continue;

			index = &g_array_index(stream_pos->packet_index,
					struct packet_index,
					stream_pos->packet_index->len - 1);
			if (type == BT_CLOCK_REAL) {
				if (index->ts_real.timestamp_end > end)
					end = index->ts_real.timestamp_end;
			} else if (type == BT_CLOCK_CYCLES) {
				if (index->ts_cycles.timestamp_end > end)
					end = index->ts_cycles.timestamp_end;
			} else {
				goto error;
			}
		}
	}

	return end;

error:
	return -1ULL;
}

/*
 * Update stream current timestamp
 */
static
void ctf_update_timestamp(struct ctf_stream_definition *stream,
			  struct definition_integer *integer_definition)
{
	struct declaration_integer *integer_declaration =
		integer_definition->declaration;
	uint64_t oldval, newval, updateval;

	if (unlikely(integer_declaration->len == 64)) {
		stream->cycles_timestamp = integer_definition->value._unsigned;
		stream->real_timestamp = ctf_get_real_timestamp(stream,
				stream->cycles_timestamp);
		return;
	}
	/* keep low bits */
	oldval = stream->cycles_timestamp;
	oldval &= (1ULL << integer_declaration->len) - 1;
	newval = integer_definition->value._unsigned;
	/* Test for overflow by comparing low bits */
	if (newval < oldval)
		newval += 1ULL << integer_declaration->len;
	/* updateval contains old high bits, and new low bits (sum) */
	updateval = stream->cycles_timestamp;
	updateval &= ~((1ULL << integer_declaration->len) - 1);
	updateval += newval;
	stream->cycles_timestamp = updateval;

	/* convert to real timestamp */
	stream->real_timestamp = ctf_get_real_timestamp(stream,
			stream->cycles_timestamp);
}

/*
 * Print timestamp, rescaling clock frequency to nanoseconds and
 * applying offsets as needed (unix time).
 */
static
void ctf_print_timestamp_real(FILE *fp,
			struct ctf_stream_definition *stream,
			uint64_t timestamp)
{
	uint64_t ts_sec = 0, ts_nsec;

	ts_nsec = timestamp;

	/* Add command-line offset in ns*/
        ts_nsec += opt_clock_offset_ns;

	/* Add command-line offset */
	ts_sec += opt_clock_offset;

	ts_sec += ts_nsec / NSEC_PER_SEC;
	ts_nsec = ts_nsec % NSEC_PER_SEC;

	if (!opt_clock_seconds) {
		struct tm tm;
		time_t time_s = (time_t) ts_sec;

		if (!opt_clock_gmt) {
			struct tm *res;

			res = localtime_r(&time_s, &tm);
			if (!res) {
				fprintf(stderr, "[warning] Unable to get localtime.\n");
				goto seconds;
			}
		} else {
			struct tm *res;

			res = gmtime_r(&time_s, &tm);
			if (!res) {
				fprintf(stderr, "[warning] Unable to get gmtime.\n");
				goto seconds;
			}
		}
		if (opt_clock_date) {
			char timestr[26];
			size_t res;

			/* Print date and time */
			res = strftime(timestr, sizeof(timestr),
				"%F ", &tm);
			if (!res) {
				fprintf(stderr, "[warning] Unable to print ascii time.\n");
				goto seconds;
			}
			fprintf(fp, "%s", timestr);
		}
		/* Print time in HH:MM:SS.ns */
		fprintf(fp, "%02d:%02d:%02d.%09" PRIu64,
			tm.tm_hour, tm.tm_min, tm.tm_sec, ts_nsec);
		goto end;
	}
seconds:
	fprintf(fp, "%3" PRIu64 ".%09" PRIu64,
		ts_sec, ts_nsec);

end:
	return;
}

/*
 * Print timestamp, in cycles
 */
static
void ctf_print_timestamp_cycles(FILE *fp,
		struct ctf_stream_definition *stream,
		uint64_t timestamp)
{
	fprintf(fp, "%020" PRIu64, timestamp);
}

void ctf_print_timestamp(FILE *fp,
		struct ctf_stream_definition *stream,
		uint64_t timestamp)
{
	if (opt_clock_cycles) {
		ctf_print_timestamp_cycles(fp, stream, timestamp);
	} else {
		ctf_print_timestamp_real(fp, stream, timestamp);
	}
}

static
void print_uuid(FILE *fp, unsigned char *uuid)
{
	int i;

	for (i = 0; i < BABELTRACE_UUID_LEN; i++)
		fprintf(fp, "%x", (unsigned int) uuid[i]);
}

/*
 * Discarded events can be either:
 * - discarded after end of previous buffer due to buffer full:
 *     happened within range: [ prev_timestamp_end, timestamp_begin ]
 * - discarded within current buffer due to either event too large or
 *   nested wrap-around:
 *     happened within range: [ timestamp_begin, timestamp_end ]
 *
 * Given we have discarded counters of those two types merged into the
 * events_discarded counter, we need to use the union of those ranges:
 *   [ prev_timestamp_end, timestamp_end ]
 *
 * Lost packets occur if the tracer overwrote some subbuffer(s) before the
 * consumer had time to extract them. We keep track of those gaps with the
 * packet sequence number in each packet.
 */
static
void ctf_print_discarded_lost(FILE *fp, struct ctf_stream_definition *stream)
{
	if ((!stream->events_discarded && !stream->packets_lost) ||
			!babeltrace_ctf_console_output) {
		return;
	}
	fflush(stdout);
	if (stream->events_discarded) {
		fprintf(fp, "[warning] Tracer discarded %" PRIu64 " events between [",
				stream->events_discarded);
	} else if (stream->packets_lost) {
		fprintf(fp, "[warning] Tracer lost %" PRIu64 " trace packets between [",
				stream->packets_lost);
	}
	if (opt_clock_cycles) {
		ctf_print_timestamp(fp, stream,
				stream->prev.cycles.end);
		fprintf(fp, "] and [");
		ctf_print_timestamp(fp, stream,
				stream->current.cycles.end);
	} else {
		ctf_print_timestamp(fp, stream,
				stream->prev.real.end);
		fprintf(fp, "] and [");
		ctf_print_timestamp(fp, stream,
				stream->current.real.end);
	}
	fprintf(fp, "] in trace UUID ");
	print_uuid(fp, stream->stream_class->trace->uuid);
	if (stream->stream_class->trace->parent.path[0])
		fprintf(fp, ", at path: \"%s\"",
			stream->stream_class->trace->parent.path);

	fprintf(fp, ", within stream id %" PRIu64, stream->stream_id);
	if (stream->path[0])
		fprintf(fp, ", at relative path: \"%s\"", stream->path);
	fprintf(fp, ". ");
	fprintf(fp, "You should consider recording a new trace with larger "
		"buffers or with fewer events enabled.\n");
	fflush(fp);
}

static
int ctf_read_event(struct bt_stream_pos *ppos, struct ctf_stream_definition *stream)
{
	struct ctf_stream_pos *pos =
		container_of(ppos, struct ctf_stream_pos, parent);
	struct ctf_stream_declaration *stream_class = stream->stream_class;
	struct ctf_event_definition *event;
	uint64_t id = 0;
	int ret;

	/* We need to check for EOF here for empty files. */
	if (unlikely(pos->offset == EOF))
		return EOF;

	ctf_pos_get_event(pos);

	/* save the current position as a restore point */
	pos->last_offset = pos->offset;

	/*
	 * This is the EOF check after we've advanced the position in
	 * ctf_pos_get_event.
	 */
	if (unlikely(pos->offset == EOF))
		return EOF;

	/* Stream is inactive for now (live reading). */
	if (unlikely(pos->content_size == 0))
		return EAGAIN;

	/*
	 * Packet seeked to by ctf_pos_get_event() only contains
	 * headers, no event. Consider stream as inactive (live
	 * reading).
	 */
	if (unlikely(pos->data_offset == pos->content_size))
		return EAGAIN;

	assert(pos->offset < pos->content_size);

	/* Read event header */
	if (likely(stream->stream_event_header)) {
		struct definition_integer *integer_definition;
		struct bt_definition *variant;

		ret = generic_rw(ppos, &stream->stream_event_header->p);
		if (unlikely(ret))
			goto error;
		/* lookup event id */
		integer_definition = bt_lookup_integer(&stream->stream_event_header->p, "id", FALSE);
		if (integer_definition) {
			id = integer_definition->value._unsigned;
		} else {
			struct definition_enum *enum_definition;

			enum_definition = bt_lookup_enum(&stream->stream_event_header->p, "id", FALSE);
			if (enum_definition) {
				id = enum_definition->integer->value._unsigned;
			}
		}

		variant = bt_lookup_variant(&stream->stream_event_header->p, "v");
		if (variant) {
			integer_definition = bt_lookup_integer(variant, "id", FALSE);
			if (integer_definition) {
				id = integer_definition->value._unsigned;
			}
		}
		stream->event_id = id;

		/* lookup timestamp */
		stream->has_timestamp = 0;
		integer_definition = bt_lookup_integer(&stream->stream_event_header->p, "timestamp", FALSE);
		if (integer_definition) {
			ctf_update_timestamp(stream, integer_definition);
			stream->has_timestamp = 1;
		} else {
			if (variant) {
				integer_definition = bt_lookup_integer(variant, "timestamp", FALSE);
				if (integer_definition) {
					ctf_update_timestamp(stream, integer_definition);
					stream->has_timestamp = 1;
				}
			}
		}
	}

	/* Read stream-declared event context */
	if (stream->stream_event_context) {
		ret = generic_rw(ppos, &stream->stream_event_context->p);
		if (ret)
			goto error;
	}

	if (unlikely(id >= stream_class->events_by_id->len)) {
		fprintf(stderr, "[error] Event id %" PRIu64 " is outside range.\n", id);
		return -EINVAL;
	}
	event = g_ptr_array_index(stream->events_by_id, id);
	if (unlikely(!event)) {
		fprintf(stderr, "[error] Event id %" PRIu64 " is unknown.\n", id);
		return -EINVAL;
	}

	/* Read event-declared event context */
	if (event->event_context) {
		ret = generic_rw(ppos, &event->event_context->p);
		if (ret)
			goto error;
	}

	/* Read event payload */
	if (likely(event->event_fields)) {
		ret = generic_rw(ppos, &event->event_fields->p);
		if (ret)
			goto error;
	}

	if (pos->last_offset == pos->offset) {
		fprintf(stderr, "[error] Invalid 0 byte event encountered.\n");
		return -EINVAL;
	}

	return 0;

error:
	fprintf(stderr, "[error] Unexpected end of packet. Either the trace data stream is corrupted or metadata description does not match data layout.\n");
	return ret;
}

static
int ctf_write_event(struct bt_stream_pos *pos, struct ctf_stream_definition *stream)
{
	struct ctf_stream_declaration *stream_class = stream->stream_class;
	struct ctf_event_definition *event;
	uint64_t id;
	int ret;

	id = stream->event_id;

	/* print event header */
	if (likely(stream->stream_event_header)) {
		ret = generic_rw(pos, &stream->stream_event_header->p);
		if (ret)
			goto error;
	}

	/* print stream-declared event context */
	if (stream->stream_event_context) {
		ret = generic_rw(pos, &stream->stream_event_context->p);
		if (ret)
			goto error;
	}

	if (unlikely(id >= stream_class->events_by_id->len)) {
		fprintf(stderr, "[error] Event id %" PRIu64 " is outside range.\n", id);
		return -EINVAL;
	}
	event = g_ptr_array_index(stream->events_by_id, id);
	if (unlikely(!event)) {
		fprintf(stderr, "[error] Event id %" PRIu64 " is unknown.\n", id);
		return -EINVAL;
	}

	/* print event-declared event context */
	if (event->event_context) {
		ret = generic_rw(pos, &event->event_context->p);
		if (ret)
			goto error;
	}

	/* Read and print event payload */
	if (likely(event->event_fields)) {
		ret = generic_rw(pos, &event->event_fields->p);
		if (ret)
			goto error;
	}

	return 0;

error:
	fprintf(stderr, "[error] Unexpected end of stream. Either the trace data stream is corrupted or metadata description does not match data layout.\n");
	return ret;
}

/*
 * One side-effect of this function is to unmap pos mmap base if one is
 * mapped.
 */
static
int find_data_offset(struct ctf_stream_pos *pos,
		struct ctf_file_stream *file_stream,
		struct packet_index *packet_index)
{
	uint64_t packet_map_len = DEFAULT_HEADER_LEN, tmp_map_len;
	struct stat filestats;
	size_t filesize;
	int ret;

	pos = &file_stream->pos;

	ret = fstat(pos->fd, &filestats);
	if (ret < 0)
		return ret;
	filesize = filestats.st_size;

	/* Deal with empty files */
	if (!filesize) {
		return 0;
	}

begin:
	if (filesize - pos->mmap_offset < (packet_map_len >> LOG2_CHAR_BIT)) {
		packet_map_len = (filesize - pos->mmap_offset) << LOG2_CHAR_BIT;
	}

	if (pos->base_mma) {
		/* unmap old base */
		ret = munmap_align(pos->base_mma);
		if (ret) {
			fprintf(stderr, "[error] Unable to unmap old base: %s.\n",
					strerror(errno));
			return ret;
		}
		pos->base_mma = NULL;
	}
	/* map new base. Need mapping length from header. */
	pos->base_mma = mmap_align(packet_map_len >> LOG2_CHAR_BIT, PROT_READ,
			MAP_PRIVATE, pos->fd, pos->mmap_offset);
	assert(pos->base_mma != MAP_FAILED);

	pos->content_size = packet_map_len;
	pos->packet_size = packet_map_len;
	pos->offset = 0;	/* Position of the packet header */

	/* update trace_packet_header and stream_packet_context */
	if (pos->prot == PROT_READ && file_stream->parent.trace_packet_header) {
		/* Read packet header */
		ret = generic_rw(&pos->parent, &file_stream->parent.trace_packet_header->p);
		if (ret) {
			if (ret == -EFAULT)
				goto retry;
		}
	}
	if (pos->prot == PROT_READ && file_stream->parent.stream_packet_context) {
		/* Read packet context */
		ret = generic_rw(&pos->parent, &file_stream->parent.stream_packet_context->p);
		if (ret) {
			if (ret == -EFAULT)
				goto retry;
		}
	}
	packet_index->data_offset = pos->offset;

	/* unmap old base */
	ret = munmap_align(pos->base_mma);
	if (ret) {
		fprintf(stderr, "[error] Unable to unmap old base: %s.\n",
				strerror(errno));
		return ret;
	}
	pos->base_mma = NULL;

	return 0;

	/* Retry with larger mapping */
retry:
	if (packet_map_len == ((filesize - pos->mmap_offset) << LOG2_CHAR_BIT)) {
		/*
		 * Reached EOF, but still expecting header/context data.
		 */
		fprintf(stderr, "[error] Reached end of file, but still expecting header or context fields.\n");
		return -EFAULT;
	}
	/* Double the mapping len, and retry */
	tmp_map_len = packet_map_len << 1;
	if (tmp_map_len >> 1 != packet_map_len) {
		/* Overflow */
		fprintf(stderr, "[error] Packet mapping length overflow\n");
		return -EFAULT;
	}
	packet_map_len = tmp_map_len;
	goto begin;
}


int ctf_init_pos(struct ctf_stream_pos *pos, struct bt_trace_descriptor *trace,
		int fd, int open_flags)
{
	pos->fd = fd;
	if (fd >= 0) {
		pos->packet_index = g_array_new(FALSE, TRUE,
				sizeof(struct packet_index));
	} else {
		pos->packet_index = NULL;
	}
	switch (open_flags & O_ACCMODE) {
	case O_RDONLY:
		pos->prot = PROT_READ;
		pos->flags = MAP_PRIVATE;
		pos->parent.rw_table = read_dispatch_table;
		pos->parent.event_cb = ctf_read_event;
		pos->parent.trace = trace;
		break;
	case O_RDWR:
		pos->prot = PROT_READ | PROT_WRITE;
		pos->flags = MAP_SHARED;
		pos->parent.rw_table = write_dispatch_table;
		pos->parent.event_cb = ctf_write_event;
		pos->parent.trace = trace;
		if (fd >= 0)
			ctf_packet_seek(&pos->parent, 0, SEEK_SET);	/* position for write */
		break;
	default:
		assert(0);
	}
	return 0;
}

int ctf_fini_pos(struct ctf_stream_pos *pos)
{
	if ((pos->prot & PROT_WRITE) && pos->content_size_loc)
		*pos->content_size_loc = pos->offset;
	if (pos->base_mma) {
		int ret;

		/* unmap old base */
		ret = munmap_align(pos->base_mma);
		if (ret) {
			fprintf(stderr, "[error] Unable to unmap old base: %s.\n",
				strerror(errno));
			return -1;
		}
	}
	if (pos->packet_index)
		(void) g_array_free(pos->packet_index, TRUE);
	return 0;
}

void ctf_update_current_packet_index(struct ctf_stream_definition *stream,
		struct packet_index *prev_index,
		struct packet_index *cur_index)
{
	uint64_t events_discarded_diff;
	uint64_t packets_lost_diff = 0;

	/* Update packet index time information */

	/* Current packet begin/end */
	stream->current.real.begin =
		cur_index->ts_real.timestamp_begin;
	stream->current.cycles.begin =
		cur_index->ts_cycles.timestamp_begin;
	stream->current.real.end =
		cur_index->ts_real.timestamp_end;
	stream->current.cycles.end =
		cur_index->ts_cycles.timestamp_end;

	/* Update packet index discarded event information */
	events_discarded_diff = cur_index->events_discarded;
	if (prev_index) {
		/* Previous packet begin/end */
		stream->prev.cycles.begin =
			prev_index->ts_cycles.timestamp_begin;
		stream->prev.real.begin =
			prev_index->ts_real.timestamp_begin;
		stream->prev.cycles.end =
			prev_index->ts_cycles.timestamp_end;
		stream->prev.real.end =
			prev_index->ts_real.timestamp_end;

		events_discarded_diff -= prev_index->events_discarded;
		/* packet_seq_num stays at 0 if not produced by the tracer */
		if (cur_index->packet_seq_num) {
			packets_lost_diff = cur_index->packet_seq_num -
				prev_index->packet_seq_num - 1;
		}
		/*
		 * Deal with 32-bit wrap-around if the tracer provided a
		 * 32-bit field.
		 */
		if (prev_index->events_discarded_len == 32) {
			events_discarded_diff = (uint32_t) events_discarded_diff;
		}
	} else {
		/*
		 * First packet: use current packet info as limits for
		 * previous packet.
		 */
		stream->prev.cycles.begin =
			stream->prev.cycles.end =
				stream->current.cycles.begin;
		stream->prev.real.begin =
			stream->prev.real.end =
				stream->current.real.begin;
	}
	stream->events_discarded = events_discarded_diff;
	stream->packets_lost = packets_lost_diff;
}

/*
 * Find the timerange where all the streams in the trace are active
 * simultaneously.
 *
 * Return 0 and update begin/end if necessary on success, return 1 for
 * empty streams and return a negative value on error.
 */
static
int ctf_find_stream_intersection(struct bt_trace_descriptor *td_read,
		struct packet_index_time *_real,
		struct packet_index_time *_cycles)
{
	int stream_id, ret = 0;
	struct packet_index_time real = { 0, UINT64_MAX },
			cycles = { 0, UINT64_MAX };
	struct ctf_trace *tin = container_of(td_read, struct ctf_trace, parent);

	/* At least one of the two return args must be provided. */
	if (!_real && !_cycles) {
		ret = -1;
		goto end;
	}

	if (tin->streams->len == 0) {
		ret = 1;
		goto end;
	}

	for (stream_id = 0; stream_id < tin->streams->len;
			stream_id++) {
		int filenr;
		struct ctf_stream_declaration *stream_class;

		stream_class = g_ptr_array_index(tin->streams, stream_id);
		if (!stream_class) {
			continue;
		}
		for (filenr = 0; filenr < stream_class->streams->len; filenr++) {
			struct ctf_file_stream *file_stream;
			struct ctf_stream_pos *stream_pos;
			struct packet_index *index;

			file_stream = g_ptr_array_index(stream_class->streams,
					filenr);
			if (!file_stream) {
				continue;
			}
			stream_pos = &file_stream->pos;
			if (!stream_pos->packet_index ||
					stream_pos->packet_index->len <= 0) {
				ret = 1;
				goto end;
			}
			index = &g_array_index(stream_pos->packet_index,
					struct packet_index, 0);
			real.timestamp_begin = max(real.timestamp_begin,
					index->ts_real.timestamp_begin);
			cycles.timestamp_begin = max(cycles.timestamp_begin,
					index->ts_cycles.timestamp_begin);

			index = &g_array_index(stream_pos->packet_index,
					struct packet_index,
					stream_pos->packet_index->len - 1);
			real.timestamp_end = min(real.timestamp_end,
					index->ts_real.timestamp_end);
			cycles.timestamp_end = min(cycles.timestamp_end,
					index->ts_cycles.timestamp_end);
		}
	}
end:
	if (ret == 0) {
		if (_real) {
			*_real = real;
		}
		if (_cycles) {
			*_cycles = cycles;
		}
	}
	return ret;
}

/*
 * Find the union of all active regions in the trace collection's traces.
 * Returns "real" timestamps.
 *
 * Return 0 on success.
 * Return 1 if no intersections are found.
 * Return a negative value on error.
 */
int ctf_find_tc_stream_packet_intersection_union(struct bt_context *ctx,
		uint64_t *_ts_begin, uint64_t *_ts_end)
{
	int ret = 0, i;
	uint64_t ts_begin = UINT64_MAX, ts_end = 0;

	if (!ctx || !ctx->tc || !ctx->tc->array || !_ts_begin || !_ts_end) {
		ret = -EINVAL;
		goto end;
	}

	for (i = 0; i < ctx->tc->array->len; i++) {
		struct bt_trace_descriptor *td_read;
		struct packet_index_time intersection_real;

		td_read = g_ptr_array_index(ctx->tc->array, i);
		if (!td_read) {
			continue;
		}
		ret = ctf_find_stream_intersection(td_read, &intersection_real,
				NULL);
		if (ret == 1) {
			/* Empty trace or no stream intersection. */
			continue;
		} else if (ret < 0) {
			goto end;
		}

		ts_begin = min(intersection_real.timestamp_begin, ts_begin);
		ts_end = max(intersection_real.timestamp_end, ts_end);
	}

	if (ts_end < ts_begin) {
		ret = 1;
		goto end;
	}
	*_ts_begin = ts_begin;
	*_ts_end = ts_end;
end:
	return ret;
}

int ctf_tc_set_stream_intersection_mode(struct bt_context *ctx)
{
	int ret = 0, i;

	if (!ctx || !ctx->tc || !ctx->tc->array) {
		ret = -EINVAL;
		goto end;
	}

	for (i = 0; i < ctx->tc->array->len; i++) {
		struct bt_trace_descriptor *td_read;
		struct packet_index_time intersection_real;

		td_read = g_ptr_array_index(ctx->tc->array, i);
		if (!td_read) {
			continue;
		}

		ret = ctf_find_stream_intersection(td_read, &intersection_real,
				NULL);
		if (ret == 1) {
			/* Empty trace or no stream intersection. */
			continue;
		} else if (ret < 0) {
			goto end;
		}

		td_read->interval_real = intersection_real;
		td_read->interval_set = true;
	}
end:
	return ret;
}

/*
 * for SEEK_CUR: go to next packet.
 * for SEEK_SET: go to packet numer (index).
 */
void ctf_packet_seek(struct bt_stream_pos *stream_pos, size_t index, int whence)
{
	struct ctf_stream_pos *pos =
		container_of(stream_pos, struct ctf_stream_pos, parent);
	struct ctf_file_stream *file_stream =
		container_of(pos, struct ctf_file_stream, pos);
	int ret;
	struct packet_index *packet_index, *prev_index;

	switch (whence) {
	case SEEK_CUR:
	case SEEK_SET:	/* Fall-through */
		break;	/* OK */
	default:
		assert(0);
	}

	if ((pos->prot & PROT_WRITE) && pos->content_size_loc)
		*pos->content_size_loc = pos->offset;

	if (pos->base_mma) {
		/* unmap old base */
		ret = munmap_align(pos->base_mma);
		if (ret) {
			fprintf(stderr, "[error] Unable to unmap old base: %s.\n",
				strerror(errno));
			assert(0);
		}
		pos->base_mma = NULL;
	}

	/*
	 * The caller should never ask for ctf_move_pos across packets,
	 * except to get exactly at the beginning of the next packet.
	 */
	if (pos->prot & PROT_WRITE) {
		switch (whence) {
		case SEEK_CUR:
			/* The writer will add padding */
			pos->mmap_offset += pos->packet_size / CHAR_BIT;
			break;
		case SEEK_SET:
			assert(index == 0);	/* only seek supported for now */
			pos->cur_index = 0;
			break;
		default:
			assert(0);
		}
		pos->content_size = -1U;	/* Unknown at this point */
		pos->packet_size = WRITE_PACKET_LEN;
		do {
			ret = bt_posix_fallocate(pos->fd, pos->mmap_offset,
					      pos->packet_size / CHAR_BIT);
		} while (ret == EINTR);
		assert(ret == 0);
		pos->offset = 0;
	} else {
	read_next_packet:
		switch (whence) {
		case SEEK_CUR:
		{
			if (pos->offset == EOF) {
				return;
			}
			assert(pos->cur_index < pos->packet_index->len);
			/* The reader will expect us to skip padding */
			++pos->cur_index;
			break;
		}
		case SEEK_SET:
			if (index >= pos->packet_index->len) {
				pos->offset = EOF;
				return;
			}
			pos->cur_index = index;
			break;
		default:
			assert(0);
		}

		if (pos->cur_index >= pos->packet_index->len) {
			pos->offset = EOF;
			return;
		}

		packet_index = &g_array_index(pos->packet_index,
				struct packet_index, pos->cur_index);
		if (pos->cur_index > 0) {
			prev_index = &g_array_index(pos->packet_index,
					struct packet_index,
					pos->cur_index - 1);
		} else {
			prev_index = NULL;
		}
		ctf_update_current_packet_index(&file_stream->parent,
				prev_index, packet_index);

		/*
		 * We need to check if we are in trace read or called
		 * from packet indexing.  In this last case, the
		 * collection is not there, so we cannot print the
		 * timestamps.
		 */
		if ((&file_stream->parent)->stream_class->trace->parent.collection) {
			ctf_print_discarded_lost(stderr, &file_stream->parent);
		}

		packet_index = &g_array_index(pos->packet_index,
				struct packet_index,
				pos->cur_index);
		file_stream->parent.cycles_timestamp = packet_index->ts_cycles.timestamp_begin;

		file_stream->parent.real_timestamp = packet_index->ts_real.timestamp_begin;

		/* Lookup context/packet size in index */
		if (packet_index->data_offset == -1) {
			ret = find_data_offset(pos, file_stream, packet_index);
			if (ret < 0) {
				return;
			}
		}
		pos->content_size = packet_index->content_size;
		pos->packet_size = packet_index->packet_size;
		pos->mmap_offset = packet_index->offset;
		pos->data_offset = packet_index->data_offset;
		if (pos->data_offset < packet_index->content_size) {
			pos->offset = 0;	/* will read headers */
		} else if (pos->data_offset == packet_index->content_size) {
			/* empty packet */
			pos->offset = packet_index->data_offset;
			whence = SEEK_CUR;
			goto read_next_packet;
		} else {
			pos->offset = EOF;
			return;
		}
	}
	/* map new base. Need mapping length from header. */
	pos->base_mma = mmap_align(pos->packet_size / CHAR_BIT, pos->prot,
			pos->flags, pos->fd, pos->mmap_offset);
	if (pos->base_mma == MAP_FAILED) {
		fprintf(stderr, "[error] mmap error %s.\n",
			strerror(errno));
		assert(0);
	}

	/* update trace_packet_header and stream_packet_context */
	if (!(pos->prot & PROT_WRITE) &&
		file_stream->parent.trace_packet_header) {
		/* Read packet header */
		ret = generic_rw(&pos->parent, &file_stream->parent.trace_packet_header->p);
		assert(!ret);
	}
	if (!(pos->prot & PROT_WRITE) &&
		file_stream->parent.stream_packet_context) {
		/* Read packet context */
		ret = generic_rw(&pos->parent, &file_stream->parent.stream_packet_context->p);
		assert(!ret);
	}
}

static
int packet_metadata(struct ctf_trace *td, FILE *fp)
{
	uint32_t magic;
	size_t len;
	int ret = 0;

	len = fread(&magic, sizeof(magic), 1, fp);
	if (len != 1) {
		goto end;
	}
	if (magic == TSDL_MAGIC) {
		ret = 1;
		td->byte_order = BYTE_ORDER;
		CTF_TRACE_SET_FIELD(td, byte_order);
	} else if (magic == GUINT32_SWAP_LE_BE(TSDL_MAGIC)) {
		ret = 1;
		td->byte_order = (BYTE_ORDER == BIG_ENDIAN) ?
					LITTLE_ENDIAN : BIG_ENDIAN;
		CTF_TRACE_SET_FIELD(td, byte_order);
	}
end:
	rewind(fp);
	return ret;
}

/*
 * Returns 0 on success, -1 on error.
 */
static
int check_version(unsigned int major, unsigned int minor)
{
	switch (major) {
	case 1:
		switch (minor) {
		case 8:
			return 0;
		default:
			goto warning;
		}
	default:
		goto warning;
		
	}

	/* eventually return an error instead of warning */
warning:
	fprintf(stderr, "[warning] Unsupported CTF specification version %u.%u. Trying anyway.\n",
		major, minor);
	return 0;
}

static
int ctf_trace_metadata_packet_read(struct ctf_trace *td, FILE *in,
					FILE *out)
{
	struct metadata_packet_header header;
	size_t readlen, writelen, toread;
	char buf[4096 + 1];	/* + 1 for debug-mode \0 */
	int ret = 0;

	readlen = fread(&header, header_sizeof(header), 1, in);
	if (readlen < 1)
		return -EINVAL;

	if (td->byte_order != BYTE_ORDER) {
		header.magic = GUINT32_SWAP_LE_BE(header.magic);
		header.checksum = GUINT32_SWAP_LE_BE(header.checksum);
		header.content_size = GUINT32_SWAP_LE_BE(header.content_size);
		header.packet_size = GUINT32_SWAP_LE_BE(header.packet_size);
	}
	if (header.checksum)
		fprintf(stderr, "[warning] checksum verification not supported yet.\n");
	if (header.compression_scheme) {
		fprintf(stderr, "[error] compression (%u) not supported yet.\n",
			header.compression_scheme);
		return -EINVAL;
	}
	if (header.encryption_scheme) {
		fprintf(stderr, "[error] encryption (%u) not supported yet.\n",
			header.encryption_scheme);
		return -EINVAL;
	}
	if (header.checksum_scheme) {
		fprintf(stderr, "[error] checksum (%u) not supported yet.\n",
			header.checksum_scheme);
		return -EINVAL;
	}
	if (check_version(header.major, header.minor) < 0)
		return -EINVAL;
	if (!CTF_TRACE_FIELD_IS_SET(td, uuid)) {
		memcpy(td->uuid, header.uuid, sizeof(header.uuid));
		CTF_TRACE_SET_FIELD(td, uuid);
	} else {
		if (bt_uuid_compare(header.uuid, td->uuid))
			return -EINVAL;
	}

	if ((header.content_size / CHAR_BIT) < header_sizeof(header))
		return -EINVAL;

	toread = (header.content_size / CHAR_BIT) - header_sizeof(header);

	for (;;) {
		readlen = fread(buf, sizeof(char), min(sizeof(buf) - 1, toread), in);
		if (ferror(in)) {
			ret = -EINVAL;
			break;
		}
		if (babeltrace_debug) {
			buf[readlen] = '\0';
			fprintf(stderr, "[debug] metadata packet read: %s\n",
				buf);
		}

		writelen = fwrite(buf, sizeof(char), readlen, out);
		if (writelen < readlen) {
			ret = -EIO;
			break;
		}
		if (ferror(out)) {
			ret = -EINVAL;
			break;
		}
		toread -= readlen;
		if (!toread) {
			ret = 0;	/* continue reading next packet */
			goto read_padding;
		}
	}
	return ret;

read_padding:
	toread = (header.packet_size - header.content_size) / CHAR_BIT;
	ret = fseek(in, toread, SEEK_CUR);
	if (ret < 0) {
		fprintf(stderr, "[warning] Missing padding at end of file\n");
		ret = 0;
	}
	return ret;
}

static
int ctf_trace_metadata_stream_read(struct ctf_trace *td, FILE **fp,
					char **buf)
{
	FILE *in, *out;
	size_t size, buflen;
	int ret;

	in = *fp;
	/*
	 * Using strlen on *buf instead of size of open_memstream
	 * because its size includes garbage at the end (after final
	 * \0). This is the allocated size, not the actual string size.
	 */
	out = babeltrace_open_memstream(buf, &size);
	if (out == NULL) {
		perror("Metadata open_memstream");
		return -errno;
	}
	for (;;) {
		ret = ctf_trace_metadata_packet_read(td, in, out);
		if (ret) {
			break;
		}
		if (feof(in)) {
			ret = 0;
			break;
		}
	}
	/* close to flush the buffer */
	ret = babeltrace_close_memstream(buf, &size, out);
	if (ret < 0) {
		int closeret;

		perror("babeltrace_flush_memstream");
		ret = -errno;
		closeret = fclose(in);
		if (closeret) {
			perror("Error in fclose");
		}
		return ret;
	}
	ret = fclose(in);
	if (ret) {
		perror("Error in fclose");
	}
	/* open for reading */
	buflen = strlen(*buf);
	if (!buflen) {
		*fp = NULL;
		return -ENOENT;
	}
	*fp = babeltrace_fmemopen(*buf, buflen, "rb");
	if (!*fp) {
		perror("Metadata fmemopen");
		return -errno;
	}
	return 0;
}

static
int ctf_trace_metadata_read(struct ctf_trace *td, FILE *metadata_fp,
		struct ctf_scanner *scanner, int append)
{
	struct ctf_file_stream *metadata_stream;
	FILE *fp;
	char *buf = NULL;
	int ret = 0, closeret;

	metadata_stream = g_new0(struct ctf_file_stream, 1);
	metadata_stream->pos.last_offset = LAST_OFFSET_POISON;

	if (metadata_fp) {
		fp = metadata_fp;
		metadata_stream->pos.fd = -1;
	} else {
		td->metadata = &metadata_stream->parent;
		metadata_stream->pos.fd = openat(td->dirfd, "metadata", O_RDONLY);
		if (metadata_stream->pos.fd < 0) {
			fprintf(stderr, "Unable to open metadata.\n");
			ret = -1;
			goto end_free;
		}

		fp = fdopen(metadata_stream->pos.fd, "r");
		if (!fp) {
			fprintf(stderr, "[error] Unable to open metadata stream.\n");
			perror("Metadata stream open");
			ret = -errno;
			goto end_stream;
		}
		/* fd now belongs to fp */
		metadata_stream->pos.fd = -1;
	}
	if (babeltrace_debug)
		yydebug = 1;

	if (packet_metadata(td, fp)) {
		ret = ctf_trace_metadata_stream_read(td, &fp, &buf);
		if (ret) {
			goto end;
		}
		td->metadata_string = buf;
		td->metadata_packetized = 1;
	} else {
		if (!append) {
			unsigned int major, minor;
			ssize_t nr_items;

			td->byte_order = BYTE_ORDER;

			/* Check text-only metadata header and version */
			nr_items = fscanf(fp, "/* CTF %10u.%10u", &major, &minor);
			if (nr_items < 2)
				fprintf(stderr, "[warning] Ill-shapen or missing \"/* CTF x.y\" header for text-only metadata.\n");
			if (check_version(major, minor) < 0) {
				ret = -EINVAL;
				goto end;
			}
			rewind(fp);
		}
	}

	ret = ctf_scanner_append_ast(scanner, fp);
	if (ret) {
		fprintf(stderr, "[error] Error creating AST\n");
		goto end;
	}

	if (babeltrace_debug) {
		ret = ctf_visitor_print_xml(stderr, 0, &scanner->ast->root);
		if (ret) {
			fprintf(stderr, "[error] Error visiting AST for XML output\n");
			goto end;
		}
	}

	ret = ctf_visitor_semantic_check(stderr, 0, &scanner->ast->root);
	if (ret) {
		fprintf(stderr, "[error] Error in CTF semantic validation %d\n", ret);
		goto end;
	}
	ret = ctf_visitor_construct_metadata(stderr, 0, &scanner->ast->root,
			td, td->byte_order);
	if (ret) {
		fprintf(stderr, "[error] Error in CTF metadata constructor %d\n", ret);
		goto end;
	}
end:
	if (fp) {
		closeret = fclose(fp);
		if (closeret) {
			perror("Error on fclose");
		}
	}
end_stream:
	if (metadata_stream->pos.fd >= 0) {
		closeret = close(metadata_stream->pos.fd);
		if (closeret) {
			perror("Error on metadata stream fd close");
		}
	}
end_free:
	if (ret)
		g_free(metadata_stream);
	return ret;
}

static
struct ctf_event_definition *create_event_definitions(struct ctf_trace *td,
						  struct ctf_stream_definition *stream,
						  struct ctf_event_declaration *event)
{
	struct ctf_event_definition *stream_event = g_new0(struct ctf_event_definition, 1);

	if (event->context_decl) {
		struct bt_definition *definition =
			event->context_decl->p.definition_new(&event->context_decl->p,
				stream->parent_def_scope, 0, 0, "event.context");
		if (!definition) {
			goto error;
		}
		stream_event->event_context = container_of(definition,
					struct definition_struct, p);
		stream->parent_def_scope = stream_event->event_context->p.scope;
	}
	if (event->fields_decl) {
		struct bt_definition *definition =
			event->fields_decl->p.definition_new(&event->fields_decl->p,
				stream->parent_def_scope, 0, 0, "event.fields");
		if (!definition) {
			goto error;
		}
		stream_event->event_fields = container_of(definition,
					struct definition_struct, p);
		stream->parent_def_scope = stream_event->event_fields->p.scope;
	}
	stream_event->stream = stream;
	return stream_event;

error:
	if (stream_event->event_fields)
		bt_definition_unref(&stream_event->event_fields->p);
	if (stream_event->event_context)
		bt_definition_unref(&stream_event->event_context->p);
	fprintf(stderr, "[error] Unable to create event definition for event \"%s\".\n",
		g_quark_to_string(event->name));
	return NULL;
}

static
int copy_event_declarations_stream_class_to_stream(struct ctf_trace *td,
		struct ctf_stream_declaration *stream_class,
		struct ctf_stream_definition *stream)
{
	size_t def_size, class_size, i;
	int ret = 0;

	def_size = stream->events_by_id->len;
	class_size = stream_class->events_by_id->len;

	g_ptr_array_set_size(stream->events_by_id, class_size);
	for (i = def_size; i < class_size; i++) {
		struct ctf_event_declaration *event =
			g_ptr_array_index(stream_class->events_by_id, i);
		struct ctf_event_definition *stream_event;

		if (!event)
			continue;
		stream_event = create_event_definitions(td, stream, event);
		if (!stream_event) {
			ret = -EINVAL;
			goto error;
		}
		g_ptr_array_index(stream->events_by_id, i) = stream_event;
	}
error:
	return ret;
}

static
int create_stream_definitions(struct ctf_trace *td, struct ctf_stream_definition *stream)
{
	struct ctf_stream_declaration *stream_class;
	int ret;
	int i;

	if (stream->stream_definitions_created)
		return 0;

	stream_class = stream->stream_class;

	if (stream_class->packet_context_decl) {
		struct bt_definition *definition =
			stream_class->packet_context_decl->p.definition_new(&stream_class->packet_context_decl->p,
				stream->parent_def_scope, 0, 0, "stream.packet.context");
		if (!definition) {
			ret = -EINVAL;
			goto error;
		}
		stream->stream_packet_context = container_of(definition,
						struct definition_struct, p);
		stream->parent_def_scope = stream->stream_packet_context->p.scope;
	}
	if (stream_class->event_header_decl) {
		struct bt_definition *definition =
			stream_class->event_header_decl->p.definition_new(&stream_class->event_header_decl->p,
				stream->parent_def_scope, 0, 0, "stream.event.header");
		if (!definition) {
			ret = -EINVAL;
			goto error;
		}
		stream->stream_event_header =
			container_of(definition, struct definition_struct, p);
		stream->parent_def_scope = stream->stream_event_header->p.scope;
	}
	if (stream_class->event_context_decl) {
		struct bt_definition *definition =
			stream_class->event_context_decl->p.definition_new(&stream_class->event_context_decl->p,
				stream->parent_def_scope, 0, 0, "stream.event.context");
		if (!definition) {
			ret = -EINVAL;
			goto error;
		}
		stream->stream_event_context =
			container_of(definition, struct definition_struct, p);
		stream->parent_def_scope = stream->stream_event_context->p.scope;
	}
	stream->events_by_id = g_ptr_array_new();
	ret = copy_event_declarations_stream_class_to_stream(td,
			stream_class, stream);
	if (ret)
		goto error_event;
	return 0;

error_event:
	for (i = 0; i < stream->events_by_id->len; i++) {
		struct ctf_event_definition *stream_event = g_ptr_array_index(stream->events_by_id, i);
		if (stream_event)
			g_free(stream_event);
	}
	g_ptr_array_free(stream->events_by_id, TRUE);
error:
	if (stream->stream_event_context)
		bt_definition_unref(&stream->stream_event_context->p);
	if (stream->stream_event_header)
		bt_definition_unref(&stream->stream_event_header->p);
	if (stream->stream_packet_context)
		bt_definition_unref(&stream->stream_packet_context->p);
	fprintf(stderr, "[error] Unable to create stream (%" PRIu64 ") definitions: %s\n",
		stream_class->stream_id, strerror(-ret));
	return ret;
}

static
int stream_assign_class(struct ctf_trace *td,
		struct ctf_file_stream *file_stream,
		uint64_t stream_id)
{
	struct ctf_stream_declaration *stream;
	int ret;

	file_stream->parent.stream_id = stream_id;
	if (stream_id >= td->streams->len) {
		fprintf(stderr, "[error] Stream %" PRIu64 " is not declared in metadata.\n", stream_id);
		return -EINVAL;
	}
	stream = g_ptr_array_index(td->streams, stream_id);
	if (!stream) {
		fprintf(stderr, "[error] Stream %" PRIu64 " is not declared in metadata.\n", stream_id);
		return -EINVAL;
	}
	file_stream->parent.stream_class = stream;
	ret = create_stream_definitions(td, &file_stream->parent);
	if (ret)
		return ret;
	return 0;
}

static
int create_stream_one_packet_index(struct ctf_stream_pos *pos,
			struct ctf_trace *td,
			struct ctf_file_stream *file_stream,
			size_t filesize)
{
	struct packet_index packet_index;
	uint64_t stream_id = 0;
	uint64_t packet_map_len = DEFAULT_HEADER_LEN, tmp_map_len;
	int first_packet = 0;
	int len_index;
	int ret;

begin:
	memset(&packet_index, 0, sizeof(packet_index));
	if (!pos->mmap_offset) {
		first_packet = 1;
	}

	if (filesize - pos->mmap_offset < (packet_map_len >> LOG2_CHAR_BIT)) {
		packet_map_len = (filesize - pos->mmap_offset) << LOG2_CHAR_BIT;
	}

	if (pos->base_mma) {
		/* unmap old base */
		ret = munmap_align(pos->base_mma);
		if (ret) {
			fprintf(stderr, "[error] Unable to unmap old base: %s.\n",
				strerror(errno));
			return ret;
		}
		pos->base_mma = NULL;
	}
	/* map new base. Need mapping length from header. */
	pos->base_mma = mmap_align(packet_map_len >> LOG2_CHAR_BIT, PROT_READ,
			 MAP_PRIVATE, pos->fd, pos->mmap_offset);
	assert(pos->base_mma != MAP_FAILED);
	/*
	 * Use current mapping size as temporary content and packet
	 * size.
	 */
	pos->content_size = packet_map_len;
	pos->packet_size = packet_map_len;
	pos->offset = 0;	/* Position of the packet header */

	packet_index.offset = pos->mmap_offset;

	/* read and check header, set stream id (and check) */
	if (file_stream->parent.trace_packet_header) {
		/* Read packet header */
		ret = generic_rw(&pos->parent, &file_stream->parent.trace_packet_header->p);
		if (ret) {
			if (ret == -EFAULT)
				goto retry;
			fprintf(stderr, "[error] Unable to read packet header: %s\n", strerror(-ret));
			return ret;
		}
		len_index = bt_struct_declaration_lookup_field_index(file_stream->parent.trace_packet_header->declaration, g_quark_from_static_string("magic"));
		if (len_index >= 0) {
			struct bt_definition *field;
			uint64_t magic;

			field = bt_struct_definition_get_field_from_index(file_stream->parent.trace_packet_header, len_index);
			magic = bt_get_unsigned_int(field);
			if (magic != CTF_MAGIC) {
				fprintf(stderr, "[error] Invalid magic number 0x%" PRIX64 " at packet %u (file offset %zd).\n",
						magic,
						file_stream->pos.packet_index->len,
						(ssize_t) pos->mmap_offset);
				return -EINVAL;
			}
		}

		/* check uuid */
		len_index = bt_struct_declaration_lookup_field_index(file_stream->parent.trace_packet_header->declaration, g_quark_from_static_string("uuid"));
		if (len_index >= 0) {
			struct definition_array *defarray;
			struct bt_definition *field;
			uint64_t i;
			uint8_t uuidval[BABELTRACE_UUID_LEN];

			field = bt_struct_definition_get_field_from_index(file_stream->parent.trace_packet_header, len_index);
			assert(field->declaration->id == CTF_TYPE_ARRAY);
			defarray = container_of(field, struct definition_array, p);
			assert(bt_array_len(defarray) == BABELTRACE_UUID_LEN);

			for (i = 0; i < BABELTRACE_UUID_LEN; i++) {
				struct bt_definition *elem;

				elem = bt_array_index(defarray, i);
				uuidval[i] = bt_get_unsigned_int(elem);
			}
			ret = bt_uuid_compare(td->uuid, uuidval);
			if (ret) {
				fprintf(stderr, "[error] Unique Universal Identifiers do not match.\n");
				return -EINVAL;
			}
		}

		len_index = bt_struct_declaration_lookup_field_index(file_stream->parent.trace_packet_header->declaration, g_quark_from_static_string("stream_id"));
		if (len_index >= 0) {
			struct bt_definition *field;

			field = bt_struct_definition_get_field_from_index(file_stream->parent.trace_packet_header, len_index);
			stream_id = bt_get_unsigned_int(field);
		}
	}

	if (!first_packet && file_stream->parent.stream_id != stream_id) {
		fprintf(stderr, "[error] Stream ID is changing within a stream: expecting %" PRIu64 ", but packet has %" PRIu64 "\n",
			stream_id,
			file_stream->parent.stream_id);
		return -EINVAL;
	}
	if (first_packet) {
		ret = stream_assign_class(td, file_stream, stream_id);
		if (ret)
			return ret;
	}

	if (file_stream->parent.stream_packet_context) {
		/* Read packet context */
		ret = generic_rw(&pos->parent, &file_stream->parent.stream_packet_context->p);
		if (ret) {
			if (ret == -EFAULT)
				goto retry;
			fprintf(stderr, "[error] Unable to read packet context: %s\n", strerror(-ret));
			return ret;
		}
		/* read packet size from header */
		len_index = bt_struct_declaration_lookup_field_index(file_stream->parent.stream_packet_context->declaration, g_quark_from_static_string("packet_size"));
		if (len_index >= 0) {
			struct bt_definition *field;

			field = bt_struct_definition_get_field_from_index(file_stream->parent.stream_packet_context, len_index);
			packet_index.packet_size = bt_get_unsigned_int(field);
		} else {
			/* Use file size for packet size */
			packet_index.packet_size = filesize * CHAR_BIT;
		}

		/* read content size from header */
		len_index = bt_struct_declaration_lookup_field_index(file_stream->parent.stream_packet_context->declaration, g_quark_from_static_string("content_size"));
		if (len_index >= 0) {
			struct bt_definition *field;

			field = bt_struct_definition_get_field_from_index(file_stream->parent.stream_packet_context, len_index);
			packet_index.content_size = bt_get_unsigned_int(field);
		} else {
			/* Use packet size if non-zero, else file size */
			packet_index.content_size = packet_index.packet_size ? : filesize * CHAR_BIT;
		}

		/* read timestamp begin from header */
		len_index = bt_struct_declaration_lookup_field_index(file_stream->parent.stream_packet_context->declaration, g_quark_from_static_string("timestamp_begin"));
		if (len_index >= 0) {
			struct bt_definition *field;

			field = bt_struct_definition_get_field_from_index(file_stream->parent.stream_packet_context, len_index);
			packet_index.ts_cycles.timestamp_begin = bt_get_unsigned_int(field);
			if (file_stream->parent.stream_class->trace->parent.collection) {
				packet_index.ts_real.timestamp_begin =
					ctf_get_real_timestamp(
						&file_stream->parent,
						packet_index.ts_cycles.timestamp_begin);
			}
		}

		/* read timestamp end from header */
		len_index = bt_struct_declaration_lookup_field_index(file_stream->parent.stream_packet_context->declaration, g_quark_from_static_string("timestamp_end"));
		if (len_index >= 0) {
			struct bt_definition *field;

			field = bt_struct_definition_get_field_from_index(file_stream->parent.stream_packet_context, len_index);
			packet_index.ts_cycles.timestamp_end = bt_get_unsigned_int(field);
			if (file_stream->parent.stream_class->trace->parent.collection) {
				packet_index.ts_real.timestamp_end =
					ctf_get_real_timestamp(
						&file_stream->parent,
						packet_index.ts_cycles.timestamp_end);
			}
		}

		/* read events discarded from header */
		len_index = bt_struct_declaration_lookup_field_index(file_stream->parent.stream_packet_context->declaration, g_quark_from_static_string("events_discarded"));
		if (len_index >= 0) {
			struct bt_definition *field;

			field = bt_struct_definition_get_field_from_index(file_stream->parent.stream_packet_context, len_index);
			packet_index.events_discarded = bt_get_unsigned_int(field);
			packet_index.events_discarded_len = bt_get_int_len(field);
		}

		/* read packet_seq_num from header */
		len_index = bt_struct_declaration_lookup_field_index(
				file_stream->parent.stream_packet_context->declaration,
				g_quark_from_static_string("packet_seq_num"));
		if (len_index >= 0) {
			struct bt_definition *field;

			field = bt_struct_definition_get_field_from_index(
					file_stream->parent.stream_packet_context,
					len_index);
			packet_index.packet_seq_num = bt_get_unsigned_int(field);
		}
	} else {
		/* Use file size for packet size */
		packet_index.packet_size = filesize * CHAR_BIT;
		/* Use packet size if non-zero, else file size */
		packet_index.content_size = packet_index.packet_size ? : filesize * CHAR_BIT;
	}

	/* Validate content size and packet size values */
	if (packet_index.content_size > packet_index.packet_size) {
		fprintf(stderr, "[error] Content size (%" PRIu64 " bits) is larger than packet size (%" PRIu64 " bits).\n",
			packet_index.content_size, packet_index.packet_size);
		return -EINVAL;
	}

	if (packet_index.packet_size > ((uint64_t) filesize - packet_index.offset) * CHAR_BIT) {
		fprintf(stderr, "[error] Packet size (%" PRIu64 " bits) is larger than remaining file size (%" PRIu64 " bits).\n",
			packet_index.packet_size, ((uint64_t) filesize - packet_index.offset) * CHAR_BIT);
		return -EINVAL;
	}

	if (packet_index.content_size < pos->offset) {
		fprintf(stderr, "[error] Invalid CTF stream: content size is smaller than packet headers.\n");
		return -EINVAL;
	}

	if ((packet_index.packet_size >> LOG2_CHAR_BIT) == 0) {
		fprintf(stderr, "[error] Invalid CTF stream: packet size needs to be at least one byte\n");
		return -EINVAL;
	}

	/* Save position after header and context */
	packet_index.data_offset = pos->offset;

	/* add index to packet array */
	g_array_append_val(file_stream->pos.packet_index, packet_index);

	pos->mmap_offset += packet_index.packet_size >> LOG2_CHAR_BIT;

	return 0;

	/* Retry with larger mapping */
retry:
	if (packet_map_len == ((filesize - pos->mmap_offset) << LOG2_CHAR_BIT)) {
		/*
		 * Reached EOF, but still expecting header/context data.
		 */
		fprintf(stderr, "[error] Reached end of file, but still expecting header or context fields.\n");
		return -EFAULT;
	}
	/* Double the mapping len, and retry */
	tmp_map_len = packet_map_len << 1;
	if (tmp_map_len >> 1 != packet_map_len) {
		/* Overflow */
		fprintf(stderr, "[error] Packet mapping length overflow\n");
		return -EFAULT;
	}
	packet_map_len = tmp_map_len;
	goto begin;
}

static
int create_stream_packet_index(struct ctf_trace *td,
			struct ctf_file_stream *file_stream)
{
	struct ctf_stream_pos *pos;
	struct stat filestats;
	int ret;

	pos = &file_stream->pos;

	ret = fstat(pos->fd, &filestats);
	if (ret < 0)
		return ret;

	/* Deal with empty files */
	if (!filestats.st_size) {
		if (file_stream->parent.trace_packet_header
				|| file_stream->parent.stream_packet_context) {
			/*
			 * We expect a trace packet header and/or stream packet
			 * context. Since a trace needs to have at least one
			 * packet, empty files are therefore not accepted.
			 */
			fprintf(stderr, "[error] Encountered an empty file, but expecting a trace packet header.\n");
			return -EINVAL;
		} else {
			/*
			 * Without trace packet header nor stream packet
			 * context, a one-packet trace can indeed be empty. This
			 * is only valid if there is only one stream class: 0.
			 */
			ret = stream_assign_class(td, file_stream, 0);
			if (ret)
				return ret;
			return 0;
		}
	}

	for (pos->mmap_offset = 0; pos->mmap_offset < filestats.st_size; ) {
		ret = create_stream_one_packet_index(pos, td, file_stream,
			filestats.st_size);
		if (ret)
			return ret;
	}
	return 0;
}

static
int create_trace_definitions(struct ctf_trace *td, struct ctf_stream_definition *stream)
{
	int ret;

	if (td->packet_header_decl) {
		struct bt_definition *definition =
			td->packet_header_decl->p.definition_new(&td->packet_header_decl->p,
				stream->parent_def_scope, 0, 0, "trace.packet.header");
		if (!definition) {
			ret = -EINVAL;
			goto error;
		}
		stream->trace_packet_header = 
			container_of(definition, struct definition_struct, p);
		stream->parent_def_scope = stream->trace_packet_header->p.scope;
	}

	return 0;

error:
	fprintf(stderr, "[error] Unable to create trace definitions: %s\n", strerror(-ret));
	return ret;
}

static
int import_stream_packet_index(struct ctf_trace *td,
		struct ctf_file_stream *file_stream)
{
	struct ctf_stream_pos *pos;
	struct ctf_packet_index *ctf_index = NULL;
	struct ctf_packet_index_file_hdr index_hdr;
	struct packet_index index;
	uint32_t packet_index_len, index_minor;
	int ret = 0;
	int first_packet = 1;
	size_t len;

	pos = &file_stream->pos;

	len = fread(&index_hdr, sizeof(index_hdr), 1, pos->index_fp);
	if (len != 1) {
		perror("read index file header");
		goto error;
	}

	/* Check the index header */
	if (be32toh(index_hdr.magic) != CTF_INDEX_MAGIC) {
		fprintf(stderr, "[error] wrong index magic\n");
		ret = -1;
		goto error;
	}
	if (be32toh(index_hdr.index_major) != CTF_INDEX_MAJOR) {
		fprintf(stderr, "[error] Incompatible index file %" PRIu32
				".%" PRIu32 ", supported %d.%d\n",
				be32toh(index_hdr.index_major),
				be32toh(index_hdr.index_minor), CTF_INDEX_MAJOR,
				CTF_INDEX_MINOR);
		ret = -1;
		goto error;
	}
	index_minor = be32toh(index_hdr.index_minor);

	packet_index_len = be32toh(index_hdr.packet_index_len);
	if (packet_index_len == 0) {
		fprintf(stderr, "[error] Packet index length cannot be 0.\n");
		ret = -1;
		goto error;
	}
	/*
	 * Allocate the index length found in header, not internal
	 * representation.
	 */
	ctf_index = g_malloc0(packet_index_len);
	while (fread(ctf_index, packet_index_len, 1,
			pos->index_fp) == 1) {
		uint64_t stream_id;
		struct ctf_stream_declaration *stream = NULL;

		memset(&index, 0, sizeof(index));
		index.offset = be64toh(ctf_index->offset);
		index.packet_size = be64toh(ctf_index->packet_size);
		index.content_size = be64toh(ctf_index->content_size);
		index.ts_cycles.timestamp_begin = be64toh(ctf_index->timestamp_begin);
		index.ts_cycles.timestamp_end = be64toh(ctf_index->timestamp_end);
		index.events_discarded = be64toh(ctf_index->events_discarded);
		index.events_discarded_len = 64;
		index.data_offset = -1;
		stream_id = be64toh(ctf_index->stream_id);
		if (index_minor >= 1) {
			index.stream_instance_id = be64toh(ctf_index->stream_instance_id);
			index.packet_seq_num = be64toh(ctf_index->packet_seq_num);
		}

		if (!first_packet) {
			/* add index to packet array */
			g_array_append_val(file_stream->pos.packet_index, index);
			continue;
		}

		file_stream->parent.stream_id = stream_id;
		if (stream_id < td->streams->len) {
			stream = g_ptr_array_index(td->streams, stream_id);
		}
		if (!stream) {
			fprintf(stderr, "[error] Stream %" PRIu64
					" is not declared in metadata.\n",
					stream_id);
			ret = -EINVAL;
			goto error;
		}
		file_stream->parent.stream_class = stream;
		ret = create_stream_definitions(td, &file_stream->parent);
		if (ret)
			goto error;
		first_packet = 0;
		/* add index to packet array */
		g_array_append_val(file_stream->pos.packet_index, index);
	}

	/* Index containing only the header. */
	if (!file_stream->parent.stream_class) {
		ret = -1;
		goto error;
	}

	ret = 0;

error:
	g_free(ctf_index);
	return ret;
}

/*
 * Note: many file streams can inherit from the same stream class
 * description (metadata).
 */
static
int ctf_open_file_stream_read(struct ctf_trace *td, const char *path, int flags,
		void (*packet_seek)(struct bt_stream_pos *pos, size_t index,
			int whence))
{
	int ret, fd, closeret;
	struct ctf_file_stream *file_stream;
	struct stat statbuf;
	char *index_name;

	fd = openat(td->dirfd, path, flags);
	if (fd < 0) {
		perror("File stream openat()");
		ret = fd;
		goto error;
	}

	/* Don't try to mmap subdirectories. Skip them, return success. */
	ret = fstat(fd, &statbuf);
	if (ret) {
		perror("File stream fstat()");
		goto fstat_error;
	}
	if (S_ISDIR(statbuf.st_mode)) {
		if (strncmp(path, "index", 5) != 0) {
			fprintf(stderr, "[warning] Skipping directory '%s' "
					"found in trace\n", path);
		}
		ret = 0;
		goto fd_is_dir_ok;
	}
	if (!statbuf.st_size) {
		/** Skip empty files. */
		ret = 0;
		goto fd_is_empty_file;
	}

	file_stream = g_new0(struct ctf_file_stream, 1);
	file_stream->pos.last_offset = LAST_OFFSET_POISON;
	file_stream->pos.fd = -1;
	file_stream->pos.index_fp = NULL;

	strncpy(file_stream->parent.path, path, PATH_MAX);
	file_stream->parent.path[PATH_MAX - 1] = '\0';

	if (packet_seek) {
		file_stream->pos.packet_seek = packet_seek;
	} else {
		fprintf(stderr, "[error] packet_seek function undefined.\n");
		ret = -1;
		goto error_def;
	}

	ret = ctf_init_pos(&file_stream->pos, &td->parent, fd, flags);
	if (ret)
		goto error_def;
	ret = create_trace_definitions(td, &file_stream->parent);
	if (ret)
		goto error_def;
	/*
	 * For now, only a single clock per trace is supported.
	 */
	file_stream->parent.current_clock = td->parent.single_clock;

	/*
	 * Allocate the index name for this stream and try to open it.
	 */
	index_name = malloc((strlen(path) + sizeof(INDEX_PATH)) * sizeof(char));
	if (!index_name) {
		fprintf(stderr, "[error] Cannot allocate index filename\n");
		ret = -ENOMEM;
		goto error_def;
	}
	snprintf(index_name, strlen(path) + sizeof(INDEX_PATH),
			INDEX_PATH, path);

	if (bt_faccessat(td->dirfd, td->parent.path, index_name, O_RDONLY, 0) < 0) {
		ret = create_stream_packet_index(td, file_stream);
		if (ret) {
			fprintf(stderr, "[error] Stream index creation error.\n");
			goto error_index;
		}
	} else {
		ret = openat(td->dirfd, index_name, flags);
		if (ret < 0) {
			perror("Index file openat()");
			ret = -1;
			goto error_free;
		}
		file_stream->pos.index_fp = fdopen(ret, "r");
		if (!file_stream->pos.index_fp) {
			perror("fdopen() error");
			goto error_free;
		}
		ret = import_stream_packet_index(td, file_stream);
		if (ret) {
			ret = -1;
			goto error_index;
		}
		ret = fclose(file_stream->pos.index_fp);
		if (ret < 0) {
			perror("close index");
			goto error_free;
		}
	}
	free(index_name);

	/* Add stream file to stream class */
	g_ptr_array_add(file_stream->parent.stream_class->streams,
			&file_stream->parent);
	return 0;

error_index:
	if (file_stream->pos.index_fp) {
		ret = fclose(file_stream->pos.index_fp);
		if (ret < 0) {
			perror("close index");
		}
	}
	if (file_stream->parent.trace_packet_header)
		bt_definition_unref(&file_stream->parent.trace_packet_header->p);
error_free:
	free(index_name);
error_def:
	closeret = ctf_fini_pos(&file_stream->pos);
	if (closeret) {
		fprintf(stderr, "Error on ctf_fini_pos\n");
	}
	g_free(file_stream);
fd_is_empty_file:
fd_is_dir_ok:
fstat_error:
	closeret = close(fd);
	if (closeret) {
		perror("Error on fd close");
	}
error:
	return ret;
}

static
int ctf_open_trace_read(struct ctf_trace *td,
		const char *path, int flags,
		void (*packet_seek)(struct bt_stream_pos *pos, size_t index,
			int whence), FILE *metadata_fp)
{
	struct ctf_scanner *scanner;
	int ret, closeret;
	struct dirent *dirent;
	struct dirent *diriter;
	size_t dirent_len;
	char *ext;

	td->flags = flags;

	/* Open trace directory */
	td->dir = opendir(path);
	if (!td->dir) {
		fprintf(stderr, "[error] Unable to open trace directory \"%s\".\n", path);
		ret = -ENOENT;
		goto error;
	}

	td->dirfd = open(path, 0);
	if (td->dirfd < 0) {
		fprintf(stderr, "[error] Unable to open trace directory file descriptor for path \"%s\".\n", path);
		perror("Trace directory open");
		ret = -errno;
		goto error_dirfd;
	}
	strncpy(td->parent.path, path, sizeof(td->parent.path));
	td->parent.path[sizeof(td->parent.path) - 1] = '\0';

	/*
	 * Keep the metadata file separate.
	 * Keep scanner object local to the open. We don't support
	 * incremental metadata append for on-disk traces.
	 */
	scanner = ctf_scanner_alloc();
	if (!scanner) {
		fprintf(stderr, "[error] Error allocating scanner\n");
		ret = -ENOMEM;
		goto error_metadata;
	}
	ret = ctf_trace_metadata_read(td, metadata_fp, scanner, 0);
	ctf_scanner_free(scanner);
	if (ret) {
		if (ret == -ENOENT) {
			fprintf(stderr, "[warning] Empty metadata.\n");
		}
		fprintf(stderr, "[warning] Unable to open trace metadata for path \"%s\".\n", path);
		goto error_metadata;
	}

	/*
	 * Open each stream: for each file, try to open, check magic
	 * number, and get the stream ID to add to the right location in
	 * the stream array.
	 */

	dirent_len = offsetof(struct dirent, d_name) +
			fpathconf(td->dirfd, _PC_NAME_MAX) + 1;

	dirent = malloc(dirent_len);

	for (;;) {
		ret = readdir_r(td->dir, dirent, &diriter);
		if (ret) {
			fprintf(stderr, "[error] Readdir error.\n");
			goto readdir_error;
		}
		if (!diriter)
			break;
		/* Ignore hidden files, ., .. and metadata. */
		if (!strncmp(diriter->d_name, ".", 1)
				|| !strcmp(diriter->d_name, "..")
				|| !strcmp(diriter->d_name, "metadata"))
			continue;

		/* Ignore index files : *.idx */
		ext = strrchr(diriter->d_name, '.');
		if (ext && (!strcmp(ext, ".idx"))) {
			continue;
		}

		ret = ctf_open_file_stream_read(td, diriter->d_name,
					flags, packet_seek);
		if (ret) {
			fprintf(stderr, "[error] Open file stream error.\n");
			goto readdir_error;
		}
	}

	free(dirent);
	return 0;

readdir_error:
	free(dirent);
error_metadata:
	closeret = close(td->dirfd);
	if (closeret) {
		perror("Error on fd close");
	}
error_dirfd:
	closeret = closedir(td->dir);
	if (closeret) {
		perror("Error on closedir");
	}
error:
	return ret;
}

/*
 * ctf_open_trace: Open a CTF trace and index it.
 * Note that the user must seek the trace after the open (using the iterator)
 * since the index creation read it entirely.
 */
static
struct bt_trace_descriptor *ctf_open_trace(const char *path, int flags,
		void (*packet_seek)(struct bt_stream_pos *pos, size_t index,
			int whence), FILE *metadata_fp)
{
	struct ctf_trace *td;
	int ret;

	/*
	 * If packet_seek is NULL, we provide our default version.
	 */
	if (!packet_seek)
		packet_seek = ctf_packet_seek;

	td = g_new0(struct ctf_trace, 1);
	if (!td) {
		goto error;
	}
	init_trace_descriptor(&td->parent);

	switch (flags & O_ACCMODE) {
	case O_RDONLY:
		if (!path) {
			fprintf(stderr, "[error] Path missing for input CTF trace.\n");
			goto error;
		}
		ret = ctf_open_trace_read(td, path, flags, packet_seek, metadata_fp);
		if (ret)
			goto error;
		break;
	case O_RDWR:
		fprintf(stderr, "[error] Opening CTF traces for output is not supported yet.\n");
		goto error;
	default:
		fprintf(stderr, "[error] Incorrect open flags.\n");
		goto error;
	}

	ret = trace_debug_info_create(td);
	if (ret) {
		goto error;
	}

	return &td->parent;
error:
	if (td) {
		trace_debug_info_destroy(td);
		g_free(td);
	}
	return NULL;
}

static
void ctf_init_mmap_pos(struct ctf_stream_pos *pos,
		struct bt_mmap_stream *mmap_info)
{
	pos->mmap_offset = 0;
	pos->packet_size = 0;
	pos->content_size = 0;
	pos->content_size_loc = NULL;
	pos->fd = mmap_info->fd;
	pos->base_mma = NULL;
	pos->offset = 0;
	pos->dummy = false;
	pos->cur_index = 0;
	pos->prot = PROT_READ;
	pos->flags = MAP_PRIVATE;
	pos->parent.rw_table = read_dispatch_table;
	pos->parent.event_cb = ctf_read_event;
	pos->priv = mmap_info->priv;
	pos->packet_index = g_array_new(FALSE, TRUE,
			sizeof(struct packet_index));
}

static
int prepare_mmap_stream_definition(struct ctf_trace *td,
		struct ctf_file_stream *file_stream,
		void (*packet_seek)(struct bt_stream_pos *pos, size_t index,
			int whence))
{
	struct ctf_stream_declaration *stream;
	uint64_t stream_id;
	int ret;

	/* Ask for the first packet to get the stream_id. */
	packet_seek(&file_stream->pos.parent, 0, SEEK_SET);
	stream_id = file_stream->parent.stream_id;
	if (stream_id >= td->streams->len) {
		fprintf(stderr, "[error] Stream %" PRIu64 " is not declared "
				"in metadata.\n", stream_id);
		ret = -EINVAL;
		goto end;
	}
	stream = g_ptr_array_index(td->streams, stream_id);
	if (!stream) {
		fprintf(stderr, "[error] Stream %" PRIu64 " is not declared "
				"in metadata.\n", stream_id);
		ret = -EINVAL;
		goto end;
	}
	file_stream->parent.stream_class = stream;
	ret = create_stream_definitions(td, &file_stream->parent);
end:
	return ret;
}

static
int ctf_open_mmap_stream_read(struct ctf_trace *td,
		struct bt_mmap_stream *mmap_info,
		void (*packet_seek)(struct bt_stream_pos *pos, size_t index,
			int whence))
{
	int ret;
	struct ctf_file_stream *file_stream;

	file_stream = g_new0(struct ctf_file_stream, 1);
	file_stream->parent.stream_id = -1ULL;
	file_stream->pos.last_offset = LAST_OFFSET_POISON;
	ctf_init_mmap_pos(&file_stream->pos, mmap_info);

	file_stream->pos.packet_seek = packet_seek;

	ret = create_trace_definitions(td, &file_stream->parent);
	if (ret) {
		goto error_def;
	}

	ret = prepare_mmap_stream_definition(td, file_stream, packet_seek);
	if (ret)
		goto error_index;

	/*
	 * For now, only a single clock per trace is supported.
	 */
	file_stream->parent.current_clock = td->parent.single_clock;

	/* Add stream file to stream class */
	g_ptr_array_add(file_stream->parent.stream_class->streams,
			&file_stream->parent);
	return 0;

error_index:
	if (file_stream->parent.trace_packet_header)
		bt_definition_unref(&file_stream->parent.trace_packet_header->p);
error_def:
	g_free(file_stream);
	return ret;
}

static
int ctf_open_mmap_trace_read(struct ctf_trace *td,
		struct bt_mmap_stream_list *mmap_list,
		void (*packet_seek)(struct bt_stream_pos *pos, size_t index,
			int whence),
		FILE *metadata_fp)
{
	int ret;
	struct bt_mmap_stream *mmap_info;

	td->scanner = ctf_scanner_alloc();
	if (!td->scanner) {
		fprintf(stderr, "[error] Error allocating scanner\n");
		ret = -ENOMEM;
		goto error;
	}
	ret = ctf_trace_metadata_read(td, metadata_fp, td->scanner, 0);
	if (ret) {
		if (ret == -ENOENT) {
			fprintf(stderr, "[warning] Empty metadata.\n");
		}
		goto error;
	}

	/*
	 * for each stream, try to open, check magic number, and get the
	 * stream ID to add to the right location in the stream array.
	 */
	bt_list_for_each_entry(mmap_info, &mmap_list->head, list) {
		ret = ctf_open_mmap_stream_read(td, mmap_info, packet_seek);
		if (ret) {
			fprintf(stderr, "[error] Open file mmap stream error.\n");
			goto error;
		}
	}
	return 0;

error:
	ctf_scanner_free(td->scanner);
	return ret;
}

static
struct bt_trace_descriptor *ctf_open_mmap_trace(
		struct bt_mmap_stream_list *mmap_list,
		void (*packet_seek)(struct bt_stream_pos *pos, size_t index,
			int whence),
		FILE *metadata_fp)
{
	struct ctf_trace *td;
	int ret;

	if (!metadata_fp) {
		fprintf(stderr, "[error] No metadata file pointer associated, "
				"required for mmap parsing\n");
		goto error;
	}
	if (!packet_seek) {
		fprintf(stderr, "[error] packet_seek function undefined.\n");
		goto error;
	}
	td = g_new0(struct ctf_trace, 1);
	td->dirfd = -1;
	ret = ctf_open_mmap_trace_read(td, mmap_list, packet_seek, metadata_fp);
	if (ret)
		goto error_free;

	ret = trace_debug_info_create(td);
	if (ret) {
		goto error_free;
	}

	return &td->parent;

error_free:
	g_free(td);
error:
	return NULL;
}

int ctf_append_trace_metadata(struct bt_trace_descriptor *tdp,
		FILE *metadata_fp)
{
	struct ctf_trace *td = container_of(tdp, struct ctf_trace, parent);
	int i, j;
	int ret;

	if (!td->scanner)
		return -EINVAL;
	ret = ctf_trace_metadata_read(td, metadata_fp, td->scanner, 1);
	if (ret)
		return ret;
	/* for each stream_class */
	for (i = 0; i < td->streams->len; i++) {
		struct ctf_stream_declaration *stream_class;

		stream_class = g_ptr_array_index(td->streams, i);
		if (!stream_class)
			continue;
		/* for each stream */
		for (j = 0; j < stream_class->streams->len; j++) {
			struct ctf_stream_definition *stream;

			stream = g_ptr_array_index(stream_class->streams, j);
			if (!stream)
				continue;
			ret = copy_event_declarations_stream_class_to_stream(td,
				stream_class, stream);
			if (ret)
				return ret;
		}
	}
	return 0;
}

static
int ctf_convert_index_timestamp(struct bt_trace_descriptor *tdp)
{
	int i, j, k;
	struct ctf_trace *td = container_of(tdp, struct ctf_trace, parent);

	/* for each stream_class */
	for (i = 0; i < td->streams->len; i++) {
		struct ctf_stream_declaration *stream_class;

		stream_class = g_ptr_array_index(td->streams, i);
		if (!stream_class)
			continue;
		/* for each file_stream */
		for (j = 0; j < stream_class->streams->len; j++) {
			struct ctf_stream_definition *stream;
			struct ctf_stream_pos *stream_pos;
			struct ctf_file_stream *cfs;

			stream = g_ptr_array_index(stream_class->streams, j);
			if (!stream)
				continue;
			cfs = container_of(stream, struct ctf_file_stream,
					parent);
			stream_pos = &cfs->pos;
			if (!stream_pos->packet_index)
				continue;

			for (k = 0; k < stream_pos->packet_index->len; k++) {
				struct packet_index *index;

				index = &g_array_index(stream_pos->packet_index,
						struct packet_index, k);
				index->ts_real.timestamp_begin =
					ctf_get_real_timestamp(stream,
							index->ts_cycles.timestamp_begin);
				index->ts_real.timestamp_end =
					ctf_get_real_timestamp(stream,
							index->ts_cycles.timestamp_end);
			}
		}
	}
	return 0;
}

static
int ctf_close_file_stream(struct ctf_file_stream *file_stream)
{
	int ret;

	ret = ctf_fini_pos(&file_stream->pos);
	if (ret) {
		fprintf(stderr, "Error on ctf_fini_pos\n");
		return -1;
	}
	if (file_stream->pos.fd >= 0) {
		ret = close(file_stream->pos.fd);
		if (ret) {
			perror("Error closing file fd");
			return -1;
		}
	}
	return 0;
}

static
int ctf_close_trace(struct bt_trace_descriptor *tdp)
{
	struct ctf_trace *td = container_of(tdp, struct ctf_trace, parent);
	int ret;

	if (td->streams) {
		int i;

		for (i = 0; i < td->streams->len; i++) {
			struct ctf_stream_declaration *stream;
			int j;

			stream = g_ptr_array_index(td->streams, i);
			if (!stream)
				continue;
			for (j = 0; j < stream->streams->len; j++) {
				struct ctf_file_stream *file_stream;
				file_stream = container_of(g_ptr_array_index(stream->streams, j),
						struct ctf_file_stream, parent);
				ret = ctf_close_file_stream(file_stream);
				if (ret)
					return ret;
			}
		}
	}
	ctf_destroy_metadata(td);
	ctf_scanner_free(td->scanner);
	if (td->dirfd >= 0) {
		ret = close(td->dirfd);
		if (ret) {
			perror("Error closing dirfd");
			return ret;
		}
	}
	if (td->dir) {
		ret = closedir(td->dir);
		if (ret) {
			perror("Error closedir");
			return ret;
		}
	}
	free(td->metadata_string);
	trace_debug_info_destroy(td);
	g_free(td);
	return 0;
}

static
void ctf_set_context(struct bt_trace_descriptor *descriptor,
		struct bt_context *ctx)
{
	struct ctf_trace *td = container_of(descriptor, struct ctf_trace,
			parent);

	td->parent.ctx = ctx;
}

static
void ctf_set_handle(struct bt_trace_descriptor *descriptor,
		struct bt_trace_handle *handle)
{
	struct ctf_trace *td = container_of(descriptor, struct ctf_trace,
			parent);

	td->parent.handle = handle;
}

static
void __attribute__((constructor)) ctf_init(void)
{
	int ret;

	ctf_format.name = g_quark_from_static_string("ctf");
	ret = bt_register_format(&ctf_format);
	assert(!ret);
}

static
void __attribute__((destructor)) ctf_exit(void)
{
	bt_unregister_format(&ctf_format);
}
