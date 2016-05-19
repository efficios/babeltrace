#ifndef _BABELTRACE_TRACE_DEBUG_INFO_H
#define _BABELTRACE_TRACE_DEBUG_INFO_H

/*
 * Babeltrace - Debug information state tracker wrapper
 *
 * Copyright (c) 2015 EfficiOS Inc.
 * Copyright (c) 2015 Antoine Busque <abusque@efficios.com>
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

#include <babeltrace/ctf-ir/metadata.h>

#ifdef ENABLE_DEBUG_INFO

#include <babeltrace/debug-info.h>
#include <babeltrace/ctf-text/types.h>
#include <stdbool.h>

static inline
void ctf_text_integer_write_debug_info(struct bt_stream_pos *ppos,
		struct bt_definition *definition)
{
	struct definition_integer *integer_definition =
			container_of(definition, struct definition_integer, p);
	struct ctf_text_stream_pos *pos = ctf_text_pos(ppos);
	struct debug_info_source *debug_info_src =
			integer_definition->debug_info_src;

	/* Print debug info if available */
	if (debug_info_src) {
		if (debug_info_src->func || debug_info_src->src_path ||
				debug_info_src->bin_path) {
			bool add_comma = false;

			fprintf(pos->fp, ", debug_info = { ");

			if (debug_info_src->bin_path) {
				fprintf(pos->fp, "bin = \"%s%s\"",
						opt_debug_info_full_path ?
						debug_info_src->bin_path :
						debug_info_src->short_bin_path,
						debug_info_src->bin_loc);
				add_comma = true;
			}

			if (debug_info_src->func) {
				if (add_comma) {
					fprintf(pos->fp, ", ");
				}

				fprintf(pos->fp, "func = \"%s\"",
						debug_info_src->func);
			}

			if (debug_info_src->src_path) {
				if (add_comma) {
					fprintf(pos->fp, ", ");
				}

				fprintf(pos->fp, "src = \"%s:%" PRIu64
						"\"",
						opt_debug_info_full_path ?
						debug_info_src->src_path :
						debug_info_src->short_src_path,
						debug_info_src->line_no);
			}

			fprintf(pos->fp, " }");
		}
	}
}

static inline
int trace_debug_info_create(struct ctf_trace *trace)
{
	int ret = 0;

	if (strcmp(trace->env.domain, "ust") != 0) {
		goto end;
	}

	if (strcmp(trace->env.tracer_name, "lttng-ust") != 0) {
		goto end;
	}

	trace->debug_info = debug_info_create();
	if (!trace->debug_info) {
		ret = -1;
		goto end;
	}

end:
	return ret;
}

static inline
void trace_debug_info_destroy(struct ctf_trace *trace)
{
	debug_info_destroy(trace->debug_info);
}

static inline
void handle_debug_info_event(struct ctf_stream_declaration *stream_class,
		struct ctf_event_definition *event)
{
	debug_info_handle_event(stream_class->trace->debug_info, event);
}

#else  /* #ifdef ENABLE_DEBUG_INFO */

static inline
void ctf_text_integer_write_debug_info(struct bt_stream_pos *ppos,
		struct bt_definition *definition)
{
	/* Do nothing. */
}

static inline
int trace_debug_info_create(struct ctf_trace *trace)
{
	return 0;
}

static inline
void trace_debug_info_destroy(struct ctf_trace *trace)
{
	/* Do nothing. */
}

static inline
void handle_debug_info_event(struct ctf_stream_declaration *stream_class,
		struct ctf_event_definition *event)
{
	/* Do nothing. */
}

#endif	/* #else #ifdef ENABLE_DEBUG_INFO */

#endif /* _BABELTRACE_TRACE_DEBUG_INFO_H */
