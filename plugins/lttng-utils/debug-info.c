/*
 * Babeltrace - Debug Information State Tracker
 *
 * Copyright (c) 2015 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2015 Philippe Proulx <pproulx@efficios.com>
 * Copyright (c) 2015 Antoine Busque <abusque@efficios.com>
 * Copyright (c) 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright (c) 2019 Francis Deslauriers <francis.deslauriers@efficios.com>
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

#define BT_LOG_TAG "PLUGIN-CTF-LTTNG-UTILS-DEBUG-INFO-FLT"
#include "logging.h"

#include <glib.h>
#include <plugins-common.h>

#include <babeltrace/assert-internal.h>
#include <babeltrace/common-internal.h>

#include "bin-info.h"
#include "debug-info.h"
#include "trace-ir-data-copy.h"
#include "trace-ir-mapping.h"
#include "trace-ir-metadata-copy.h"
#include "utils.h"

#define DEFAULT_DEBUG_INFO_FIELD_NAME	"debug_info"
#define LTTNG_UST_STATEDUMP_PREFIX	"lttng_ust"
#define VPID_FIELD_NAME			"vpid"
#define IP_FIELD_NAME			"ip"
#define BADDR_FIELD_NAME		"baddr"
#define CRC32_FIELD_NAME		"crc"
#define BUILD_ID_FIELD_NAME		"build_id"
#define FILENAME_FIELD_NAME		"filename"
#define IS_PIC_FIELD_NAME		"is_pic"
#define MEMSZ_FIELD_NAME		"memsz"
#define PATH_FIELD_NAME			"path"

struct debug_info_component {
	char *arg_debug_info_field_name;
	const char *arg_debug_dir;
	bool arg_full_path;
	const char *arg_target_prefix;
};

struct debug_info_msg_iter {
	struct debug_info_component *debug_info_component;
	bt_self_message_iterator *input_iterator;
	bt_self_component *self_comp;
	bt_self_component_port_input_message_iterator *msg_iter;

	struct trace_ir_maps *ir_maps;
	/* in_trace -> debug_info_mapping. */
	GHashTable *debug_info_map;
};

struct debug_info_source {
	/* Strings are owned by debug_info_source. */
	char *func;
	/*
	 * Store the line number as a string so that the allocation and
	 * conversion to string is only done once.
	 */
	char *line_no;
	char *src_path;
	/* short_src_path points inside src_path, no need to free. */
	const char *short_src_path;
	char *bin_path;
	/* short_bin_path points inside bin_path, no need to free. */
	const char *short_bin_path;
	/*
	 * Location within the binary. Either absolute (@0x1234) or
	 * relative (+0x4321).
	 */
	char *bin_loc;
};

struct proc_debug_info_sources {
	/*
	 * Hash table: base address (pointer to uint64_t) to bin info; owned by
	 * proc_debug_info_sources.
	 */
	GHashTable *baddr_to_bin_info;

	/*
	 * Hash table: IP (pointer to uint64_t) to (struct debug_info_source *);
	 * owned by proc_debug_info_sources.
	 */
	GHashTable *ip_to_debug_info_src;
};

struct debug_info {
	struct debug_info_component *comp;
	const bt_trace *input_trace;
	uint64_t destruction_listener_id;

	/*
	 * Hash table of VPIDs (pointer to int64_t) to
	 * (struct ctf_proc_debug_infos*); owned by debug_info.
	 */
	GHashTable *vpid_to_proc_dbg_info_src;
	GQuark q_statedump_bin_info;
	GQuark q_statedump_debug_link;
	GQuark q_statedump_build_id;
	GQuark q_statedump_start;
	GQuark q_dl_open;
	GQuark q_lib_load;
	GQuark q_lib_unload;
};

static
int debug_info_init(struct debug_info *info)
{
	info->q_statedump_bin_info = g_quark_from_string(
			"lttng_ust_statedump:bin_info");
	info->q_statedump_debug_link = g_quark_from_string(
			"lttng_ust_statedump:debug_link");
	info->q_statedump_build_id = g_quark_from_string(
			"lttng_ust_statedump:build_id");
	info->q_statedump_start = g_quark_from_string(
			"lttng_ust_statedump:start");
	info->q_dl_open = g_quark_from_string("lttng_ust_dl:dlopen");
	info->q_lib_load = g_quark_from_string("lttng_ust_lib:load");
	info->q_lib_unload = g_quark_from_string("lttng_ust_lib:unload");

	return bin_info_init();
}

static
void debug_info_source_destroy(struct debug_info_source *debug_info_src)
{
	if (!debug_info_src) {
		return;
	}

	free(debug_info_src->func);
	free(debug_info_src->line_no);
	free(debug_info_src->src_path);
	free(debug_info_src->bin_path);
	free(debug_info_src->bin_loc);
	g_free(debug_info_src);
}

static
struct debug_info_source *debug_info_source_create_from_bin(
		struct bin_info *bin, uint64_t ip)
{
	int ret;
	struct debug_info_source *debug_info_src = NULL;
	struct source_location *src_loc = NULL;

	debug_info_src = g_new0(struct debug_info_source, 1);

	if (!debug_info_src) {
		goto end;
	}

	/* Lookup function name */
	ret = bin_info_lookup_function_name(bin, ip, &debug_info_src->func);
	if (ret) {
		goto error;
	}

	/* Can't retrieve src_loc from ELF, or could not find binary, skip. */
	if (!bin->is_elf_only || !debug_info_src->func) {
		/* Lookup source location */
		ret = bin_info_lookup_source_location(bin, ip, &src_loc);
		if (ret) {
			BT_LOGD("Failed to lookup source location: ret=%d", ret);
		}
	}

	if (src_loc) {
		ret = asprintf(&debug_info_src->line_no, "%"PRId64, src_loc->line_no);
		if (ret == -1) {
			BT_LOGD("Error occured when setting line_no field.");
			goto error;
		}

		if (src_loc->filename) {
			debug_info_src->src_path = strdup(src_loc->filename);
			if (!debug_info_src->src_path) {
				goto error;
			}

			debug_info_src->short_src_path = get_filename_from_path(
					debug_info_src->src_path);
		}
		source_location_destroy(src_loc);
	}

	if (bin->elf_path) {
		debug_info_src->bin_path = strdup(bin->elf_path);
		if (!debug_info_src->bin_path) {
			goto error;
		}

		debug_info_src->short_bin_path = get_filename_from_path(
				debug_info_src->bin_path);

		ret = bin_info_get_bin_loc(bin, ip, &(debug_info_src->bin_loc));
		if (ret) {
			goto error;
		}
	}

end:
	return debug_info_src;

error:
	debug_info_source_destroy(debug_info_src);
	return NULL;
}

static
void proc_debug_info_sources_destroy(
		struct proc_debug_info_sources *proc_dbg_info_src)
{
	if (!proc_dbg_info_src) {
		return;
	}

	if (proc_dbg_info_src->baddr_to_bin_info) {
		g_hash_table_destroy(proc_dbg_info_src->baddr_to_bin_info);
	}

	if (proc_dbg_info_src->ip_to_debug_info_src) {
		g_hash_table_destroy(proc_dbg_info_src->ip_to_debug_info_src);
	}

	g_free(proc_dbg_info_src);
}

static
struct proc_debug_info_sources *proc_debug_info_sources_create(void)
{
	struct proc_debug_info_sources *proc_dbg_info_src = NULL;

	proc_dbg_info_src = g_new0(struct proc_debug_info_sources, 1);
	if (!proc_dbg_info_src) {
		goto end;
	}

	proc_dbg_info_src->baddr_to_bin_info = g_hash_table_new_full(
			g_int64_hash, g_int64_equal, (GDestroyNotify) g_free,
			(GDestroyNotify) bin_info_destroy);
	if (!proc_dbg_info_src->baddr_to_bin_info) {
		goto error;
	}

	proc_dbg_info_src->ip_to_debug_info_src = g_hash_table_new_full(
			g_int64_hash, g_int64_equal, (GDestroyNotify) g_free,
			(GDestroyNotify) debug_info_source_destroy);
	if (!proc_dbg_info_src->ip_to_debug_info_src) {
		goto error;
	}

end:
	return proc_dbg_info_src;

error:
	proc_debug_info_sources_destroy(proc_dbg_info_src);
	return NULL;
}

static
struct proc_debug_info_sources *proc_debug_info_sources_ht_get_entry(
		GHashTable *ht, int64_t vpid)
{
	gpointer key = g_new0(int64_t, 1);
	struct proc_debug_info_sources *proc_dbg_info_src = NULL;

	if (!key) {
		goto end;
	}

	*((int64_t *) key) = vpid;

	/* Exists? Return it */
	proc_dbg_info_src = g_hash_table_lookup(ht, key);
	if (proc_dbg_info_src) {
		goto end;
	}

	/* Otherwise, create and return it */
	proc_dbg_info_src = proc_debug_info_sources_create();
	if (!proc_dbg_info_src) {
		goto end;
	}

	g_hash_table_insert(ht, key, proc_dbg_info_src);
	/* Ownership passed to ht */
	key = NULL;
end:
	g_free(key);
	return proc_dbg_info_src;
}

static inline
const bt_field *event_borrow_payload_field(const bt_event *event,
		const char *field_name)
{
	const bt_field *event_payload, *field;

	event_payload =  bt_event_borrow_payload_field_const(event);
	BT_ASSERT(event_payload);

	field = bt_field_structure_borrow_member_field_by_name_const(
			event_payload, field_name);
	return field;
}

static inline
const bt_field *event_borrow_common_context_field(const bt_event *event,
		const char *field_name)
{
	const bt_field *event_common_ctx, *field = NULL;

	event_common_ctx =  bt_event_borrow_common_context_field_const(event);
	if (!event_common_ctx) {
		goto end;
	}

	field = bt_field_structure_borrow_member_field_by_name_const(
			event_common_ctx, field_name);

end:
	return field;
}

static inline
void event_get_common_context_signed_integer_field_value(
		const bt_event *event, const char *field_name, int64_t *value)
{
	*value = bt_field_signed_integer_get_value(
			event_borrow_common_context_field(event, field_name));
}

static inline
int event_get_payload_build_id_length(const bt_event *event,
		const char *field_name, uint64_t *build_id_len)
{
	const bt_field *build_id_field;
	const bt_field_class *build_id_field_class;

	build_id_field = event_borrow_payload_field(event, field_name);
	build_id_field_class = bt_field_borrow_class_const(build_id_field);

	BT_ASSERT(bt_field_class_get_type(build_id_field_class) ==
			BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY);
	BT_ASSERT(bt_field_class_get_type(
			bt_field_class_array_borrow_element_field_class_const(
				build_id_field_class)) ==
			BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER);

	*build_id_len = bt_field_array_get_length(build_id_field);

	return 0;
}

static inline
int event_get_payload_build_id_value(const bt_event *event,
		const char *field_name, uint8_t *build_id)
{
	const bt_field *curr_field, *build_id_field;
	const bt_field_class *build_id_field_class;
	uint64_t i, build_id_len;
	int ret;

	ret = 0;

	build_id_field = event_borrow_payload_field(event, field_name);
	build_id_field_class = bt_field_borrow_class_const(build_id_field);

	BT_ASSERT(bt_field_class_get_type(build_id_field_class) ==
			BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY);
	BT_ASSERT(bt_field_class_get_type(
			bt_field_class_array_borrow_element_field_class_const(
				build_id_field_class)) ==
			BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER);

	build_id_len = bt_field_array_get_length(build_id_field);

	for (i = 0; i < build_id_len; i++) {
		curr_field =
			bt_field_array_borrow_element_field_by_index_const(
					build_id_field, i);

		build_id[i] = bt_field_unsigned_integer_get_value(curr_field);
	}

	return ret;
}

static
void event_get_payload_unsigned_integer_field_value(const bt_event *event,
		const char *field_name, uint64_t *value)
{
	*value = bt_field_unsigned_integer_get_value(
			event_borrow_payload_field(event, field_name));
}

static
void event_get_payload_string_field_value(const bt_event *event,
		const char *field_name, const char **value)
{
	*value = bt_field_string_get_value(
			event_borrow_payload_field(event, field_name));
}

static inline
bool event_has_payload_field(const bt_event *event,
		const char *field_name)
{
	return event_borrow_payload_field(event, field_name) != NULL;
}

static
struct debug_info_source *proc_debug_info_sources_get_entry(
		struct proc_debug_info_sources *proc_dbg_info_src, uint64_t ip)
{
	struct debug_info_source *debug_info_src = NULL;
	gpointer key = g_new0(uint64_t, 1);
	GHashTableIter iter;
	gpointer baddr, value;

	if (!key) {
		goto end;
	}

	*((uint64_t *) key) = ip;

	/* Look in IP to debug infos hash table first. */
	debug_info_src = g_hash_table_lookup(
			proc_dbg_info_src->ip_to_debug_info_src,
			key);
	if (debug_info_src) {
		goto end;
	}

	/* Check in all bin_infos. */
	g_hash_table_iter_init(&iter, proc_dbg_info_src->baddr_to_bin_info);

	while (g_hash_table_iter_next(&iter, &baddr, &value))
	{
		struct bin_info *bin = value;

		if (!bin_info_has_address(value, ip)) {
			continue;
		}

		/*
		 * Found; add it to cache.
		 *
		 * FIXME: this should be bounded in size (and implement
		 * a caching policy), and entries should be prunned when
		 * libraries are unmapped.
		 */
		debug_info_src = debug_info_source_create_from_bin(bin, ip);
		if (debug_info_src) {
			g_hash_table_insert(
					proc_dbg_info_src->ip_to_debug_info_src,
					key, debug_info_src);
			/* Ownership passed to ht. */
			key = NULL;
		}
		break;
	}

end:
	free(key);
	return debug_info_src;
}

BT_HIDDEN
struct debug_info_source *debug_info_query(struct debug_info *debug_info,
		int64_t vpid, uint64_t ip)
{
	struct debug_info_source *dbg_info_src = NULL;
	struct proc_debug_info_sources *proc_dbg_info_src;

	proc_dbg_info_src = proc_debug_info_sources_ht_get_entry(
			debug_info->vpid_to_proc_dbg_info_src, vpid);
	if (!proc_dbg_info_src) {
		goto end;
	}

	dbg_info_src = proc_debug_info_sources_get_entry(proc_dbg_info_src, ip);

end:
	return dbg_info_src;
}

BT_HIDDEN
struct debug_info *debug_info_create(struct debug_info_component *comp,
		const bt_trace *trace)
{
	int ret;
	struct debug_info *debug_info;

	debug_info = g_new0(struct debug_info, 1);
	if (!debug_info) {
		goto end;
	}

	debug_info->vpid_to_proc_dbg_info_src = g_hash_table_new_full(
			g_int64_hash, g_int64_equal, (GDestroyNotify) g_free,
			(GDestroyNotify) proc_debug_info_sources_destroy);
	if (!debug_info->vpid_to_proc_dbg_info_src) {
		goto error;
	}

	debug_info->comp = comp;
	ret = debug_info_init(debug_info);
	if (ret) {
		goto error;
	}

	debug_info->input_trace = trace;

end:
	return debug_info;
error:
	g_free(debug_info);
	return NULL;
}

BT_HIDDEN
void debug_info_destroy(struct debug_info *debug_info)
{
	bt_trace_status status;
	if (!debug_info) {
		goto end;
	}

	if (debug_info->vpid_to_proc_dbg_info_src) {
		g_hash_table_destroy(debug_info->vpid_to_proc_dbg_info_src);
	}

	status = bt_trace_remove_destruction_listener(debug_info->input_trace,
			debug_info->destruction_listener_id);
	if (status != BT_TRACE_STATUS_OK) {
		BT_LOGD("Trace destruction listener removal failed.");
	}

	g_free(debug_info);
end:
	return;
}

static
void handle_event_statedump_build_id(struct debug_info *debug_info,
		const bt_event *event)
{
	struct proc_debug_info_sources *proc_dbg_info_src;
	uint64_t build_id_len, baddr;
	uint8_t *build_id = NULL;
	struct bin_info *bin;
	int64_t vpid;
	int ret = 0;

	event_get_common_context_signed_integer_field_value(event,
			VPID_FIELD_NAME, &vpid);
	event_get_payload_unsigned_integer_field_value(event,
			BADDR_FIELD_NAME, &baddr);

	proc_dbg_info_src = proc_debug_info_sources_ht_get_entry(
			debug_info->vpid_to_proc_dbg_info_src, vpid);
	if (!proc_dbg_info_src) {
		goto end;
	}

	bin = g_hash_table_lookup(proc_dbg_info_src->baddr_to_bin_info,
			(gpointer) &baddr);
	if (!bin) {
		/*
		 * The build_id event comes after the bin has been
		 * created. If it isn't found, just ignore this event.
		 */
		goto end;
	}
	ret = event_get_payload_build_id_length(event, BUILD_ID_FIELD_NAME,
			&build_id_len);

	build_id = g_new0(uint8_t, build_id_len);
	if (!build_id) {
		goto end;
	}

	ret = event_get_payload_build_id_value(event, BUILD_ID_FIELD_NAME,
			build_id);
	if (ret) {
		goto end;
	}

	ret = bin_info_set_build_id(bin, build_id, build_id_len);
	if (ret) {
		goto end;
	}

	/*
	 * Reset the is_elf_only flag in case it had been set
	 * previously, because we might find separate debug info using
	 * the new build id information.
	 */
	bin->is_elf_only = false;

end:
	g_free(build_id);
	return;
}

static
void handle_event_statedump_debug_link(struct debug_info *debug_info,
		const bt_event *event)
{
	struct proc_debug_info_sources *proc_dbg_info_src;
	struct bin_info *bin = NULL;
	int64_t vpid;
	uint64_t baddr;
	const char *filename = NULL;
	uint32_t crc32;
	uint64_t crc_field_value;

	event_get_common_context_signed_integer_field_value(event,
			VPID_FIELD_NAME, &vpid);

	event_get_payload_unsigned_integer_field_value(event,
			BADDR_FIELD_NAME, &baddr);

	event_get_payload_unsigned_integer_field_value(event,
			CRC32_FIELD_NAME, &crc_field_value);

	crc32 = (uint32_t) crc_field_value;

	event_get_payload_string_field_value(event,
			FILENAME_FIELD_NAME, &filename);

	proc_dbg_info_src = proc_debug_info_sources_ht_get_entry(
			debug_info->vpid_to_proc_dbg_info_src, vpid);
	if (!proc_dbg_info_src) {
		goto end;
	}

	bin = g_hash_table_lookup(proc_dbg_info_src->baddr_to_bin_info,
			(gpointer) &baddr);
	if (!bin) {
		/*
		 * The debug_link event comes after the bin has been
		 * created. If it isn't found, just ignore this event.
		 */
		goto end;
	}

	bin_info_set_debug_link(bin, filename, crc32);

end:
	return;
}

static
void handle_bin_info_event(struct debug_info *debug_info,
		const bt_event *event, bool has_pic_field)
{
	struct proc_debug_info_sources *proc_dbg_info_src;
	struct bin_info *bin;
	uint64_t baddr, memsz;
	int64_t vpid;
	const char *path;
	gpointer key = NULL;
	bool is_pic;

	event_get_payload_unsigned_integer_field_value(event,
			MEMSZ_FIELD_NAME, &memsz);
	if (memsz == 0) {
		/* Ignore VDSO. */
		goto end;
	}

	event_get_payload_unsigned_integer_field_value(event,
			BADDR_FIELD_NAME, &baddr);

	/*
	 * This field is not produced by the dlopen event emitted before
	 * lttng-ust 2.9.
	 */
	if (!event_has_payload_field(event, PATH_FIELD_NAME)) {
		goto end;
	}
	event_get_payload_string_field_value(event, PATH_FIELD_NAME, &path);

	if (has_pic_field) {
		uint64_t is_pic_field_value;

		event_get_payload_unsigned_integer_field_value(event,
				IS_PIC_FIELD_NAME, &is_pic_field_value);
		is_pic = is_pic_field_value == 1;
	} else {
		/*
		 * dlopen has no is_pic field, because the shared
		 * object is always PIC.
		 */
		is_pic = true;
	}

	event_get_common_context_signed_integer_field_value(event,
		VPID_FIELD_NAME, &vpid);

	proc_dbg_info_src = proc_debug_info_sources_ht_get_entry(
			debug_info->vpid_to_proc_dbg_info_src, vpid);
	if (!proc_dbg_info_src) {
		goto end;
	}

	key = g_new0(uint64_t, 1);
	if (!key) {
		goto end;
	}

	*((uint64_t *) key) = baddr;

	bin = g_hash_table_lookup(proc_dbg_info_src->baddr_to_bin_info,
			key);
	if (bin) {
		goto end;
	}

	bin = bin_info_create(path, baddr, memsz, is_pic,
		debug_info->comp->arg_debug_dir,
		debug_info->comp->arg_target_prefix);
	if (!bin) {
		goto end;
	}

	g_hash_table_insert(proc_dbg_info_src->baddr_to_bin_info,
			key, bin);
	/* Ownership passed to ht. */
	key = NULL;

end:
	g_free(key);
	return;
}

static inline
void handle_event_statedump_bin_info(struct debug_info *debug_info,
		const bt_event *event)
{
	handle_bin_info_event(debug_info, event, true);
}

static inline
void handle_event_lib_load(struct debug_info *debug_info,
		const bt_event *event)
{
	handle_bin_info_event(debug_info, event, false);
}

static
void handle_event_lib_unload(struct debug_info *debug_info,
		const bt_event *event)
{
	gboolean ret;
	struct proc_debug_info_sources *proc_dbg_info_src;
	uint64_t baddr;
	int64_t vpid;

	event_get_payload_unsigned_integer_field_value(event, BADDR_FIELD_NAME,
			&baddr);

	event_get_common_context_signed_integer_field_value(event,
			VPID_FIELD_NAME, &vpid);

	proc_dbg_info_src = proc_debug_info_sources_ht_get_entry(
			debug_info->vpid_to_proc_dbg_info_src, vpid);
	if (!proc_dbg_info_src) {
		/*
		 * It's an unload event for a library for which no load event
		 * was previously received.
		 */
		goto end;
	}

	ret = g_hash_table_remove(proc_dbg_info_src->baddr_to_bin_info,
			(gpointer) &baddr);
	BT_ASSERT(ret);
end:
	return;
}

static
void handle_event_statedump_start(struct debug_info *debug_info,
		const bt_event *event)
{
	struct proc_debug_info_sources *proc_dbg_info_src;
	int64_t vpid;

	event_get_common_context_signed_integer_field_value(
			event, VPID_FIELD_NAME, &vpid);

	proc_dbg_info_src = proc_debug_info_sources_ht_get_entry(
			debug_info->vpid_to_proc_dbg_info_src, vpid);
	if (!proc_dbg_info_src) {
		goto end;
	}

	g_hash_table_remove_all(proc_dbg_info_src->baddr_to_bin_info);
	g_hash_table_remove_all(proc_dbg_info_src->ip_to_debug_info_src);

end:
	return;
}

void trace_debug_info_remove_func(const bt_trace *in_trace, void *data)
{
	struct debug_info_msg_iter *debug_it = data;
	if (debug_it->debug_info_map) {
		gboolean ret;
		ret = g_hash_table_remove(debug_it->debug_info_map,
				(gpointer) in_trace);
		BT_ASSERT(ret);
	}
}

static
void handle_event_statedump(struct debug_info_msg_iter *debug_it,
		const bt_event *event)
{
	const bt_event_class *event_class;
	const char *event_name;
	GQuark q_event_name;
	const bt_trace *trace;
	struct debug_info *debug_info;

	BT_ASSERT(debug_it);
	BT_ASSERT(event);

	event_class = bt_event_borrow_class_const(event);

	event_name = bt_event_class_get_name(event_class);

	trace = bt_stream_borrow_trace_const(
			bt_event_borrow_stream_const(event));

	debug_info = g_hash_table_lookup(debug_it->debug_info_map, trace);
	if (!debug_info) {
		debug_info = debug_info_create(debug_it->debug_info_component,
				trace);
		g_hash_table_insert(debug_it->debug_info_map, (gpointer) trace,
				debug_info);
		bt_trace_add_destruction_listener(trace,
				trace_debug_info_remove_func, debug_it,
				&debug_info->destruction_listener_id);
	}

	q_event_name = g_quark_try_string(event_name);

	if (q_event_name == debug_info->q_statedump_bin_info) {
		/* State dump */
		handle_event_statedump_bin_info(debug_info, event);
	} else if (q_event_name == debug_info->q_dl_open ||
			q_event_name == debug_info->q_lib_load) {
		/*
		 * dl_open and lib_load events are both checked for since
		 * only dl_open was produced as of lttng-ust 2.8.
		 *
		 * lib_load, which is produced from lttng-ust 2.9+, is a lot
		 * more reliable since it will be emitted when other functions
		 * of the dlopen family are called (e.g. dlmopen) and when
		 * library are transitively loaded.
		 */
		handle_event_lib_load(debug_info, event);
	} else if (q_event_name == debug_info->q_statedump_start) {
		/* Start state dump */
		handle_event_statedump_start(debug_info, event);
	} else if (q_event_name == debug_info->q_statedump_debug_link) {
		/* Debug link info */
		handle_event_statedump_debug_link(debug_info, event);
	} else if (q_event_name == debug_info->q_statedump_build_id) {
		/* Build ID info */
		handle_event_statedump_build_id(debug_info, event);
	} else if (q_event_name == debug_info-> q_lib_unload) {
		handle_event_lib_unload(debug_info, event);
	}

	return;
}

static
void destroy_debug_info_comp(struct debug_info_component *debug_info)
{
	free(debug_info->arg_debug_info_field_name);
	g_free(debug_info);
}

static
void fill_debug_info_bin_field(struct debug_info_source *dbg_info_src,
		bool full_path, bt_field *curr_field)
{
	bt_field_status status;

	BT_ASSERT(bt_field_get_class_type(curr_field) ==
			BT_FIELD_CLASS_TYPE_STRING);

	if (dbg_info_src) {
		if (full_path) {
			status = bt_field_string_set_value(curr_field,
					dbg_info_src->bin_path);
		} else {
			status = bt_field_string_set_value(curr_field,
					dbg_info_src->short_bin_path);
		}
		if (status != BT_FIELD_STATUS_OK) {
			BT_LOGE("Cannot set path component of \"bin\" "
				"curr_field field's value: str-fc-addr=%p",
				curr_field);
		}

		status = bt_field_string_append(curr_field, ":");
		if (status != BT_FIELD_STATUS_OK) {
			BT_LOGE("Cannot set colon component of \"bin\" "
				"curr_field field's value: str-fc-addr=%p",
				curr_field);
		}

		status = bt_field_string_append(curr_field, dbg_info_src->bin_loc);
		if (status != BT_FIELD_STATUS_OK) {
			BT_LOGE("Cannot set bin location component of \"bin\" "
				"curr_field field's value: str-fc-addr=%p",
				curr_field);
		}
	} else {
		status = bt_field_string_set_value(curr_field, "");
		if (status != BT_FIELD_STATUS_OK) {
			BT_LOGE("Cannot set \"bin\" curr_field field's value: "
				"str-fc-addr=%p", curr_field);
		}
	}
}

static
void fill_debug_info_func_field(struct debug_info_source *dbg_info_src,
		bt_field *curr_field)
{
	bt_field_status status;

	BT_ASSERT(bt_field_get_class_type(curr_field) ==
			BT_FIELD_CLASS_TYPE_STRING);
	if (dbg_info_src && dbg_info_src->func) {
		status = bt_field_string_set_value(curr_field,
				dbg_info_src->func);
	} else {
		status = bt_field_string_set_value(curr_field, "");
	}
	if (status != BT_FIELD_STATUS_OK) {
		BT_LOGE("Cannot set \"func\" curr_field field's value: "
			"str-fc-addr=%p", curr_field);
	}
}

static
void fill_debug_info_src_field(struct debug_info_source *dbg_info_src,
		bool full_path, bt_field *curr_field)
{
	bt_field_status status;

	BT_ASSERT(bt_field_get_class_type(curr_field) ==
			BT_FIELD_CLASS_TYPE_STRING);

	if (dbg_info_src && dbg_info_src->src_path) {
		if (full_path) {
			status = bt_field_string_set_value(curr_field,
					dbg_info_src->src_path);
		} else {
			status = bt_field_string_set_value(curr_field,
					dbg_info_src->short_src_path);
		}
		if (status != BT_FIELD_STATUS_OK) {
			BT_LOGE("Cannot set path component of \"src\" "
				"curr_field field's value: str-fc-addr=%p",
				curr_field);
		}

		status = bt_field_string_append(curr_field, ":");
		if (status != BT_FIELD_STATUS_OK) {
			BT_LOGE("Cannot set colon component of \"src\" "
				"curr_field field's value: str-fc-addr=%p",
				curr_field);
		}

		status = bt_field_string_append(curr_field, dbg_info_src->line_no);
		if (status != BT_FIELD_STATUS_OK) {
			BT_LOGE("Cannot set line number component of \"src\" "
				"curr_field field's value: str-fc-addr=%p",
				curr_field);
		}
	} else {
		status = bt_field_string_set_value(curr_field, "");
		if (status != BT_FIELD_STATUS_OK) {
			BT_LOGE("Cannot set \"src\" curr_field field's value: "
				"str-fc-addr=%p", curr_field);
		}
	}
}

void fill_debug_info_field_empty(bt_field *debug_info_field)
{
	bt_field_status status;
	bt_field *bin_field, *func_field, *src_field;

	BT_ASSERT(bt_field_get_class_type(debug_info_field) ==
			BT_FIELD_CLASS_TYPE_STRUCTURE);

	bin_field = bt_field_structure_borrow_member_field_by_name(
			debug_info_field, "bin");
	func_field = bt_field_structure_borrow_member_field_by_name(
			debug_info_field, "func");
	src_field = bt_field_structure_borrow_member_field_by_name(
			debug_info_field, "src");

	BT_ASSERT(bt_field_get_class_type(bin_field) ==
			BT_FIELD_CLASS_TYPE_STRING);
	BT_ASSERT(bt_field_get_class_type(func_field) ==
			BT_FIELD_CLASS_TYPE_STRING);
	BT_ASSERT(bt_field_get_class_type(src_field) ==
			BT_FIELD_CLASS_TYPE_STRING);

	status = bt_field_string_set_value(bin_field, "");
	if (status != BT_FIELD_STATUS_OK) {
		BT_LOGE("Cannot set \"bin\" bin_field field's value: "
			"str-fc-addr=%p", bin_field);
	}

	status = bt_field_string_set_value(func_field, "");
	if (status != BT_FIELD_STATUS_OK) {
		BT_LOGE("Cannot set \"func\" func_field field's value: "
			"str-fc-addr=%p", func_field);
	}

	status = bt_field_string_set_value(src_field, "");
	if (status != BT_FIELD_STATUS_OK) {
		BT_LOGE("Cannot set \"src\" src_field field's value: "
			"str-fc-addr=%p", src_field);
	}
}
static
void fill_debug_info_field(struct debug_info *debug_info, int64_t vpid,
		uint64_t ip, bt_field *debug_info_field)
{
	struct debug_info_source *dbg_info_src;
	const bt_field_class *debug_info_fc;

	BT_ASSERT(bt_field_get_class_type(debug_info_field) ==
			BT_FIELD_CLASS_TYPE_STRUCTURE);

	debug_info_fc = bt_field_borrow_class_const(debug_info_field);

	BT_ASSERT(bt_field_class_structure_get_member_count(debug_info_fc) == 3);

	dbg_info_src = debug_info_query(debug_info, vpid, ip);

	fill_debug_info_bin_field(dbg_info_src, debug_info->comp->arg_full_path,
			bt_field_structure_borrow_member_field_by_name(
				debug_info_field, "bin"));
	fill_debug_info_func_field(dbg_info_src,
			bt_field_structure_borrow_member_field_by_name(
				debug_info_field, "func"));
	fill_debug_info_src_field(dbg_info_src, debug_info->comp->arg_full_path,
			bt_field_structure_borrow_member_field_by_name(
				debug_info_field, "src"));
}

static
void fill_debug_info_event_if_needed(struct debug_info_msg_iter *debug_it,
		const bt_event *in_event, bt_event *out_event)
{
	bt_field *out_common_ctx_field, *out_debug_info_field;
	const bt_field *vpid_field, *ip_field, *in_common_ctx_field;
	const bt_field_class *in_common_ctx_fc;
	struct debug_info *debug_info;
	uint64_t vpid;
	int64_t ip;
	char *debug_info_field_name =
		debug_it->debug_info_component->arg_debug_info_field_name;

	in_common_ctx_field = bt_event_borrow_common_context_field_const(
			in_event);
	if (!in_common_ctx_field) {
		/*
		 * There is no event common context so no need to add debug
		 * info field.
		 */
		goto end;
	}

	in_common_ctx_fc = bt_field_borrow_class_const(in_common_ctx_field);
	if (!is_event_common_ctx_dbg_info_compatible(in_common_ctx_fc,
				debug_it->ir_maps->debug_info_field_class_name)) {
		/*
		 * The input event common context does not have the necessary
		 * fields to resolve debug information.
		 */
		goto end;
	}

	/* Borrow the debug-info field. */
	out_common_ctx_field = bt_event_borrow_common_context_field(out_event);
	if (!out_common_ctx_field) {
		goto end;
	}

	out_debug_info_field = bt_field_structure_borrow_member_field_by_name(
				out_common_ctx_field, debug_info_field_name);

	vpid_field = bt_field_structure_borrow_member_field_by_name_const(
			out_common_ctx_field, VPID_FIELD_NAME);
	ip_field = bt_field_structure_borrow_member_field_by_name_const(
			out_common_ctx_field, IP_FIELD_NAME);

	vpid = bt_field_signed_integer_get_value(vpid_field);
	ip = bt_field_unsigned_integer_get_value(ip_field);

	/*
	 * Borrow the debug_info structure needed for the source
	 * resolving.
	 */
	debug_info = g_hash_table_lookup(debug_it->debug_info_map,
			bt_stream_borrow_trace_const(
				bt_event_borrow_stream_const(in_event)));

	if (debug_info) {
		/*
		 * Perform the debug-info resolving and set the event fields
		 * accordingly.
		 */
		fill_debug_info_field(debug_info, vpid, ip, out_debug_info_field);
	} else {
		BT_LOGD("No debug information for this trace. Setting debug "
			"info fields to empty strings.");
		fill_debug_info_field_empty(out_debug_info_field);
	}
end:
	return;
}

static
void update_event_statedump_if_needed(struct debug_info_msg_iter *debug_it,
		const bt_event *in_event)
{
	const bt_field *event_common_ctx;
	const bt_field_class *event_common_ctx_fc;
	const bt_event_class *in_event_class = bt_event_borrow_class_const(in_event);

	/*
	 * If the event is an lttng_ust_statedump event AND has the right event
	 * common context fields update the debug-info view for this process.
	 */
	event_common_ctx = bt_event_borrow_common_context_field_const(in_event);
	if (!event_common_ctx) {
		goto end;
	}

	event_common_ctx_fc = bt_field_borrow_class_const(event_common_ctx);
	if (is_event_common_ctx_dbg_info_compatible(event_common_ctx_fc,
				debug_it->ir_maps->debug_info_field_class_name)) {
		/* Checkout if it might be a one of lttng ust statedump events. */
		const char *in_event_name = bt_event_class_get_name(in_event_class);
		if (strncmp(in_event_name, LTTNG_UST_STATEDUMP_PREFIX,
				strlen(LTTNG_UST_STATEDUMP_PREFIX)) == 0) {
			/* Handle statedump events. */
			handle_event_statedump(debug_it, in_event);
		}
	}
end:
	return;
}

static
bt_message *handle_event_message(struct debug_info_msg_iter *debug_it,
		const bt_message *in_message)
{
	const bt_clock_snapshot *cs;
	const bt_clock_class *default_cc;
	const bt_packet *in_packet;
	bt_clock_snapshot_state cs_state;
	bt_event_class *out_event_class;
	bt_packet *out_packet;
	bt_event *out_event;

	bt_message *out_message = NULL;

	/* Borrow the input event and its event class. */
	const bt_event *in_event =
		bt_message_event_borrow_event_const(in_message);
	const bt_event_class *in_event_class =
		bt_event_borrow_class_const(in_event);

	update_event_statedump_if_needed(debug_it, in_event);

	out_event_class = trace_ir_mapping_borrow_mapped_event_class(
			debug_it->ir_maps, in_event_class);
	if (!out_event_class) {
		out_event_class = trace_ir_mapping_create_new_mapped_event_class(
					debug_it->ir_maps, in_event_class);
	}
	BT_ASSERT(out_event_class);

	/* Borrow the input and output packets. */
	in_packet = bt_event_borrow_packet_const(in_event);
	out_packet = trace_ir_mapping_borrow_mapped_packet(debug_it->ir_maps,
			in_packet);

	default_cc = bt_stream_class_borrow_default_clock_class_const(
			bt_event_class_borrow_stream_class_const(in_event_class));
	if (default_cc) {
		/* Borrow event clock snapshot. */
		cs_state =
			bt_message_event_borrow_default_clock_snapshot_const(
				in_message, &cs);

		/* Create an output event message. */
		BT_ASSERT (cs_state == BT_CLOCK_SNAPSHOT_STATE_KNOWN);
		out_message = bt_message_event_create_with_default_clock_snapshot(
					debug_it->input_iterator,
					out_event_class, out_packet,
					bt_clock_snapshot_get_value(cs));
	} else {
		out_message = bt_message_event_create(debug_it->input_iterator,
				out_event_class, out_packet);
	}

	if (!out_message) {
		BT_LOGE("Error creating output event message.");
		goto error;
	}

	out_event = bt_message_event_borrow_event(out_message);

	/* Copy the original fields to the output event. */
	copy_event_content(in_event, out_event);

	/*
	 * Try to set the debug-info fields based on debug information that is
	 * gathered so far.
	 */
	fill_debug_info_event_if_needed(debug_it, in_event, out_event);

error:
	return out_message;
}

static
bt_message *handle_stream_begin_message(struct debug_info_msg_iter *debug_it,
		const bt_message *in_message)
{
	const bt_stream *in_stream;
	bt_message *out_message;
	bt_stream *out_stream;

	in_stream = bt_message_stream_beginning_borrow_stream_const(in_message);
	BT_ASSERT(in_stream);

	/* Create a duplicated output stream. */
	out_stream = trace_ir_mapping_create_new_mapped_stream(
			debug_it->ir_maps, in_stream);
	if (!out_stream) {
		out_message = NULL;
		goto error;
	}

	/* Create an output stream beginning message. */
	out_message = bt_message_stream_beginning_create(
			debug_it->input_iterator, out_stream);
	if (!out_message) {
		BT_LOGE("Error creating output stream beginning message: "
			"out-s-addr=%p", out_stream);
	}
error:
	return out_message;
}

static
bt_message *handle_stream_end_message(struct debug_info_msg_iter *debug_it,
		const bt_message *in_message)
{
	const bt_stream *in_stream;
	bt_message *out_message = NULL;
	bt_stream *out_stream;

	in_stream = bt_message_stream_end_borrow_stream_const(in_message);
	BT_ASSERT(in_stream);

	out_stream = trace_ir_mapping_borrow_mapped_stream(
			debug_it->ir_maps, in_stream);
	BT_ASSERT(out_stream);

	/* Create an output stream end message. */
	out_message = bt_message_stream_end_create(debug_it->input_iterator,
			out_stream);
	if (!out_message) {
		BT_LOGE("Error creating output stream end message: out-s-addr=%p",
				out_stream);
	}

	/* Remove stream from trace mapping hashtable. */
	trace_ir_mapping_remove_mapped_stream(debug_it->ir_maps, in_stream);

	return out_message;
}

static
bt_message *handle_packet_begin_message(struct debug_info_msg_iter *debug_it,
		const bt_message *in_message)
{
	const bt_clock_class *default_cc;
	bt_clock_snapshot_state cs_state;
	const bt_clock_snapshot *cs;
	bt_message *out_message = NULL;
	bt_packet *out_packet;

	const bt_packet *in_packet =
		bt_message_packet_beginning_borrow_packet_const(in_message);
	BT_ASSERT(in_packet);

	/* This packet should not be already mapped. */
	BT_ASSERT(!trace_ir_mapping_borrow_mapped_packet(
				debug_it->ir_maps, in_packet));

	out_packet = trace_ir_mapping_create_new_mapped_packet(debug_it->ir_maps,
			in_packet);

	BT_ASSERT(out_packet);

	default_cc = bt_stream_class_borrow_default_clock_class_const(
			bt_stream_borrow_class_const(
				bt_packet_borrow_stream_const(in_packet)));
	if (default_cc) {
		/* Borrow clock snapshot. */
		cs_state =
			bt_message_packet_beginning_borrow_default_clock_snapshot_const(
					in_message, &cs);

		/* Create an output packet beginning message. */
		BT_ASSERT(cs_state == BT_CLOCK_SNAPSHOT_STATE_KNOWN);
		out_message = bt_message_packet_beginning_create_with_default_clock_snapshot(
				debug_it->input_iterator, out_packet,
				bt_clock_snapshot_get_value(cs));
	} else {
		out_message = bt_message_packet_beginning_create(
				debug_it->input_iterator, out_packet);
	}
	if (!out_message) {
		BT_LOGE("Error creating output packet beginning message: "
			"out-p-addr=%p", out_packet);
	}

	return out_message;
}

static
bt_message *handle_packet_end_message(struct debug_info_msg_iter *debug_it,
		const bt_message *in_message)
{
	const bt_clock_snapshot *cs;
	const bt_packet *in_packet;
	const bt_clock_class *default_cc;
	bt_clock_snapshot_state cs_state;
	bt_message *out_message = NULL;
	bt_packet *out_packet;

	in_packet = bt_message_packet_end_borrow_packet_const(in_message);
	BT_ASSERT(in_packet);

	out_packet = trace_ir_mapping_borrow_mapped_packet(debug_it->ir_maps, in_packet);
	BT_ASSERT(out_packet);

	default_cc = bt_stream_class_borrow_default_clock_class_const(
			bt_stream_borrow_class_const(
				bt_packet_borrow_stream_const(in_packet)));
	if (default_cc) {
		/* Borrow clock snapshot. */
		cs_state =
			bt_message_packet_end_borrow_default_clock_snapshot_const(
					in_message, &cs);

		/* Create an outpute packet end message. */
		BT_ASSERT(cs_state == BT_CLOCK_SNAPSHOT_STATE_KNOWN);
		out_message = bt_message_packet_end_create_with_default_clock_snapshot(
				debug_it->input_iterator, out_packet,
				bt_clock_snapshot_get_value(cs));
	} else {
		out_message = bt_message_packet_end_create(
				debug_it->input_iterator, out_packet);
	}

	if (!out_message) {
		BT_LOGE("Error creating output packet end message: "
			"out-p-addr=%p", out_packet);
	}

	/* Remove packet from data mapping hashtable. */
	trace_ir_mapping_remove_mapped_packet(debug_it->ir_maps, in_packet);

	return out_message;
}

static
bt_message *handle_msg_iterator_inactivity(struct debug_info_msg_iter *debug_it,
		const bt_message *in_message)
{
	/*
	 * This message type can be forwarded directly because it does
	 * not refer to any objects in the trace class.
	 */
	bt_message_get_ref(in_message);
	return (bt_message*) in_message;
}

static
bt_message *handle_stream_act_begin_message(struct debug_info_msg_iter *debug_it,
		const bt_message *in_message)
{
	const bt_clock_snapshot *cs;
	const bt_clock_class *default_cc;
	bt_message *out_message = NULL;
	bt_stream *out_stream;
	uint64_t cs_value;
	bt_message_stream_activity_clock_snapshot_state cs_state;

	const bt_stream *in_stream =
		bt_message_stream_activity_beginning_borrow_stream_const(
			in_message);
	BT_ASSERT(in_stream);

	out_stream = trace_ir_mapping_borrow_mapped_stream(debug_it->ir_maps,
			in_stream);
	BT_ASSERT(out_stream);

	out_message = bt_message_stream_activity_beginning_create(
			debug_it->input_iterator, out_stream);
	if (!out_message) {
		BT_LOGE("Error creating output stream activity beginning "
			"message: out-s-addr=%p", out_stream);
		goto error;
	}

	default_cc = bt_stream_class_borrow_default_clock_class_const(
			bt_stream_borrow_class_const(in_stream));
	if (default_cc) {
		/* Borrow clock snapshot. */
		cs_state =
			bt_message_stream_activity_beginning_borrow_default_clock_snapshot_const(
					in_message, &cs);

		if (cs_state == BT_MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_KNOWN) {
			cs_value = bt_clock_snapshot_get_value(cs);
			bt_message_stream_activity_beginning_set_default_clock_snapshot(
					out_message, cs_value);
		} else {
			bt_message_stream_activity_beginning_set_default_clock_snapshot_state(
					out_message, cs_state);
		}
	}

error:
	return out_message;
}

static
bt_message *handle_stream_act_end_message(struct debug_info_msg_iter *debug_it,
		const bt_message *in_message)
{
	const bt_clock_snapshot *cs;
	const bt_clock_class *default_cc;
	const bt_stream *in_stream;
	bt_message *out_message;
	bt_stream *out_stream;
	uint64_t cs_value;
	bt_message_stream_activity_clock_snapshot_state cs_state;

	in_stream = bt_message_stream_activity_end_borrow_stream_const(
			in_message);
	BT_ASSERT(in_stream);

	out_stream = trace_ir_mapping_borrow_mapped_stream(debug_it->ir_maps, in_stream);
	BT_ASSERT(out_stream);

	out_message = bt_message_stream_activity_end_create(
			debug_it->input_iterator, out_stream);
	if (!out_message) {
		BT_LOGE("Error creating output stream activity end message: "
			"out-s-addr=%p", out_stream);
		goto error;
	}

	default_cc = bt_stream_class_borrow_default_clock_class_const(
			bt_stream_borrow_class_const(in_stream));

	if (default_cc) {
		cs_state =
			bt_message_stream_activity_end_borrow_default_clock_snapshot_const(
					in_message, &cs);

		if (cs_state == BT_MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_KNOWN ) {
			cs_value = bt_clock_snapshot_get_value(cs);
			bt_message_stream_activity_end_set_default_clock_snapshot(
					out_message, cs_value);
		} else {
			bt_message_stream_activity_end_set_default_clock_snapshot_state(
					out_message, cs_state);
		}
	}

error:
	return out_message;
}

static
bt_message *handle_discarded_events_message(struct debug_info_msg_iter *debug_it,
		const bt_message *in_message)
{
	const bt_clock_snapshot *begin_cs, *end_cs;
	const bt_stream *in_stream;
	const bt_clock_class *default_cc;
	uint64_t discarded_events, begin_cs_value, end_cs_value;
	bt_clock_snapshot_state begin_cs_state, end_cs_state;
	bt_property_availability prop_avail;
	bt_message *out_message = NULL;
	bt_stream *out_stream;

	in_stream = bt_message_discarded_events_borrow_stream_const(
			in_message);
	BT_ASSERT(in_stream);

	out_stream = trace_ir_mapping_borrow_mapped_stream(
				debug_it->ir_maps, in_stream);
	BT_ASSERT(out_stream);

	default_cc = bt_stream_class_borrow_default_clock_class_const(
			bt_stream_borrow_class_const(in_stream));
	if (default_cc) {
		begin_cs_state =
			bt_message_discarded_events_borrow_default_beginning_clock_snapshot_const(
					in_message, &begin_cs);
		end_cs_state =
			bt_message_discarded_events_borrow_default_end_clock_snapshot_const(
					in_message, &end_cs);
		/*
		 * Both clock snapshots should be known as we check that the
		 * all input stream classes have an always known clock. Unknown
		 * clock is not yet supported.
		 */
		BT_ASSERT(begin_cs_state == BT_CLOCK_SNAPSHOT_STATE_KNOWN &&
				end_cs_state == BT_CLOCK_SNAPSHOT_STATE_KNOWN);

		begin_cs_value = bt_clock_snapshot_get_value(begin_cs);
		end_cs_value = bt_clock_snapshot_get_value(end_cs);

		out_message =
			bt_message_discarded_events_create_with_default_clock_snapshots(
					debug_it->input_iterator, out_stream,
					begin_cs_value, end_cs_value);
	} else {
		out_message = bt_message_discarded_events_create(
				debug_it->input_iterator, out_stream);
	}
	if (!out_message) {
		BT_LOGE("Error creating output discarded events message: "
			"out-s-addr=%p", out_stream);
		goto error;
	}

	prop_avail = bt_message_discarded_events_get_count(in_message,
			&discarded_events);

	if (prop_avail == BT_PROPERTY_AVAILABILITY_NOT_AVAILABLE) {
		bt_message_discarded_events_set_count(out_message,
				discarded_events);
	}

error:
	return out_message;
}

static
bt_message *handle_discarded_packets_message(struct debug_info_msg_iter *debug_it,
		const bt_message *in_message)
{
	const bt_clock_snapshot *begin_cs, *end_cs;
	const bt_clock_class *default_cc;
	const bt_stream *in_stream;
	uint64_t discarded_packets, begin_cs_value, end_cs_value;
	bt_clock_snapshot_state begin_cs_state, end_cs_state;
	bt_property_availability prop_avail;
	bt_message *out_message = NULL;
	bt_stream *out_stream;

	in_stream = bt_message_discarded_packets_borrow_stream_const(
			in_message);
	BT_ASSERT(in_stream);

	out_stream = trace_ir_mapping_borrow_mapped_stream(
			debug_it->ir_maps, in_stream);
	BT_ASSERT(out_stream);

	default_cc = bt_stream_class_borrow_default_clock_class_const(
			bt_stream_borrow_class_const(in_stream));
	if (default_cc) {
		begin_cs_state =
			bt_message_discarded_packets_borrow_default_beginning_clock_snapshot_const(
					in_message, &begin_cs);

		end_cs_state =
			bt_message_discarded_packets_borrow_default_end_clock_snapshot_const(
					in_message, &end_cs);

		/*
		 * Both clock snapshots should be known as we check that the
		 * all input stream classes have an always known clock. Unknown
		 * clock is not yet supported.
		 */
		BT_ASSERT(begin_cs_state == BT_CLOCK_SNAPSHOT_STATE_KNOWN &&
				end_cs_state == BT_CLOCK_SNAPSHOT_STATE_KNOWN);

		begin_cs_value = bt_clock_snapshot_get_value(begin_cs);
		end_cs_value = bt_clock_snapshot_get_value(end_cs);

		out_message = bt_message_discarded_packets_create_with_default_clock_snapshots(
					debug_it->input_iterator, out_stream,
					begin_cs_value, end_cs_value);
	} else {
		out_message = bt_message_discarded_packets_create(
				debug_it->input_iterator, out_stream);
	}
	if (!out_message) {
		BT_LOGE("Error creating output discarded packet message: "
			"out-s-addr=%p", out_stream);
		goto error;
	}

	prop_avail = bt_message_discarded_packets_get_count(in_message,
			&discarded_packets);
	if (prop_avail == BT_PROPERTY_AVAILABILITY_NOT_AVAILABLE) {
		bt_message_discarded_packets_set_count(out_message,
				discarded_packets);
	}

error:
	return out_message;
}

static
const bt_message *handle_message(struct debug_info_msg_iter *debug_it,
		const bt_message *in_message)
{
	bt_message *out_message = NULL;

	switch (bt_message_get_type(in_message)) {
	case BT_MESSAGE_TYPE_EVENT:
		out_message = handle_event_message(debug_it,
				in_message);
		break;
	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
		out_message = handle_packet_begin_message(debug_it,
				in_message);
		break;
	case BT_MESSAGE_TYPE_PACKET_END:
		out_message = handle_packet_end_message(debug_it,
				in_message);
		break;
	case BT_MESSAGE_TYPE_STREAM_BEGINNING:
		out_message = handle_stream_begin_message(debug_it,
				in_message);
		break;
	case BT_MESSAGE_TYPE_STREAM_END:
		out_message = handle_stream_end_message(debug_it,
				in_message);
		break;
	case BT_MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY:
		out_message = handle_msg_iterator_inactivity(debug_it,
				in_message);
		break;
	case BT_MESSAGE_TYPE_STREAM_ACTIVITY_BEGINNING:
		out_message = handle_stream_act_begin_message(debug_it,
				in_message);
		break;
	case BT_MESSAGE_TYPE_STREAM_ACTIVITY_END:
		out_message = handle_stream_act_end_message(debug_it,
				in_message);
		break;
	case BT_MESSAGE_TYPE_DISCARDED_EVENTS:
		out_message = handle_discarded_events_message(debug_it,
				in_message);
		break;
	case BT_MESSAGE_TYPE_DISCARDED_PACKETS:
		out_message = handle_discarded_packets_message(debug_it,
				in_message);
		break;
	default:
		abort();
		break;
	}

	return out_message;
}

static
int init_from_params(struct debug_info_component *debug_info_component,
		const bt_value *params)
{
	const bt_value *value = NULL;
	int ret = 0;

	BT_ASSERT(params);

	value = bt_value_map_borrow_entry_value_const(params,
			"debug-info-field-name");
	if (value) {
		const char *debug_info_field_name;

		debug_info_field_name = bt_value_string_get(value);
		debug_info_component->arg_debug_info_field_name =
			strdup(debug_info_field_name);
	} else {
		debug_info_component->arg_debug_info_field_name =
			malloc(strlen(DEFAULT_DEBUG_INFO_FIELD_NAME) + 1);
		if (!debug_info_component->arg_debug_info_field_name) {
			ret = BT_SELF_COMPONENT_STATUS_NOMEM;
			BT_LOGE_STR("Missing field name.");
			goto end;
		}
		sprintf(debug_info_component->arg_debug_info_field_name,
				DEFAULT_DEBUG_INFO_FIELD_NAME);
	}
	if (ret) {
		goto end;
	}

	value = bt_value_map_borrow_entry_value_const(params, "debug-info-dir");
	if (value) {
		debug_info_component->arg_debug_dir = bt_value_string_get(value);
	}

	value = bt_value_map_borrow_entry_value_const(params, "target-prefix");
	if (value) {
		debug_info_component->arg_target_prefix =
			bt_value_string_get(value);
	}

	value = bt_value_map_borrow_entry_value_const(params, "full-path");
	if (value) {
		debug_info_component->arg_full_path = bt_value_bool_get(value);
	}

end:
	return ret;
}

BT_HIDDEN
bt_self_component_status debug_info_comp_init(
		bt_self_component_filter *self_comp,
		const bt_value *params, UNUSED_VAR void *init_method_data)
{
	int ret;
	struct debug_info_component *debug_info_comp;
	bt_self_component_status status = BT_SELF_COMPONENT_STATUS_OK;

	BT_LOGD("Initializing debug_info component: "
		"comp-addr=%p, params-addr=%p", self_comp, params);

	debug_info_comp = g_new0(struct debug_info_component, 1);
	if (!debug_info_comp) {
		BT_LOGE_STR("Failed to allocate one debug_info component.");
		goto error;
	}

	bt_self_component_set_data(
			bt_self_component_filter_as_self_component(self_comp),
			debug_info_comp);

	status = bt_self_component_filter_add_input_port(self_comp, "in",
			NULL, NULL);
	if (status != BT_SELF_COMPONENT_STATUS_OK) {
		goto error;
	}

	status = bt_self_component_filter_add_output_port(self_comp, "out",
			NULL, NULL);
	if (status != BT_SELF_COMPONENT_STATUS_OK) {
		goto error;
	}

	ret = init_from_params(debug_info_comp, params);
	if (ret) {
		BT_LOGE("Cannot configure debug_info component: "
			"debug_info-comp-addr=%p, params-addr=%p",
			debug_info_comp, params);
		goto error;
	}

	goto end;

error:
	destroy_debug_info_comp(debug_info_comp);
	bt_self_component_set_data(
		bt_self_component_filter_as_self_component(self_comp),
		NULL);

	if (status == BT_SELF_COMPONENT_STATUS_OK) {
		status = BT_SELF_COMPONENT_STATUS_ERROR;
	}
end:
	return status;
}

BT_HIDDEN
void debug_info_comp_finalize(bt_self_component_filter *self_comp)
{
	struct debug_info_component *debug_info =
		bt_self_component_get_data(
				bt_self_component_filter_as_self_component(
					self_comp));
	BT_LOGD("Finalizing debug_info self_component: comp-addr=%p",
		self_comp);

	destroy_debug_info_comp(debug_info);
}

BT_HIDDEN
bt_self_message_iterator_status debug_info_msg_iter_next(
		bt_self_message_iterator *self_msg_iter,
		const bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count)
{
	bt_self_component_port_input_message_iterator *upstream_iterator = NULL;
	bt_message_iterator_status upstream_iterator_ret_status;
	struct debug_info_msg_iter *debug_info_msg_iter;
	struct debug_info_component *debug_info = NULL;
	bt_self_message_iterator_status status;
	bt_self_component *self_comp = NULL;
	bt_message_array_const input_msgs;
	const bt_message *out_message;
	uint64_t curr_msg_idx, i;

	status = BT_SELF_MESSAGE_ITERATOR_STATUS_OK;

	self_comp = bt_self_message_iterator_borrow_component(self_msg_iter);
	BT_ASSERT(self_comp);

	debug_info = bt_self_component_get_data(self_comp);
	BT_ASSERT(debug_info);

	debug_info_msg_iter = bt_self_message_iterator_get_data(self_msg_iter);
	BT_ASSERT(debug_info_msg_iter);

	upstream_iterator = debug_info_msg_iter->msg_iter;
	BT_ASSERT(upstream_iterator);

	upstream_iterator_ret_status =
		bt_self_component_port_input_message_iterator_next(
				upstream_iterator, &input_msgs, count);
	if (upstream_iterator_ret_status != BT_MESSAGE_ITERATOR_STATUS_OK) {
		/*
		 * No messages were returned. Not necessarily an error. Convert
		 * the upstream message iterator status to a self status.
		 */
		status = bt_common_message_iterator_status_to_self(
				upstream_iterator_ret_status);
		goto end;
	}

	/*
	 * There should never be more received messages than the capacity we
	 * provided.
	 */
	BT_ASSERT(*count <= capacity);

	for (curr_msg_idx = 0; curr_msg_idx < *count; curr_msg_idx++) {
		out_message = handle_message(debug_info_msg_iter,
				input_msgs[curr_msg_idx]);
		if (!out_message) {
			goto handle_msg_error;
		}

		msgs[curr_msg_idx] = out_message;
		/*
		 * Drop our reference of the input message as we are done with
		 * it and created a output copy.
		 */
		bt_message_put_ref(input_msgs[curr_msg_idx]);
	}

	goto end;

handle_msg_error:
	/*
	 * Drop references of all the output messages created before the
	 * failure.
	 */
	for (i = 0; i < curr_msg_idx; i++) {
		bt_message_put_ref(msgs[i]);
	}

	status = BT_SELF_MESSAGE_ITERATOR_STATUS_NOMEM;
end:
	return status;
}

BT_HIDDEN
bt_self_message_iterator_status debug_info_msg_iter_init(
		bt_self_message_iterator *self_msg_iter,
		bt_self_component_filter *self_comp,
		bt_self_component_port_output *self_port)
{
	bt_self_message_iterator_status status = BT_SELF_MESSAGE_ITERATOR_STATUS_OK;
	struct bt_self_component_port_input *input_port;
	bt_self_component_port_input_message_iterator *upstream_iterator;
	struct debug_info_msg_iter *debug_info_msg_iter;

	/* Borrow the upstream input port. */
	input_port = bt_self_component_filter_borrow_input_port_by_name(
			self_comp, "in");
	if (!input_port) {
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_ERROR;
		goto end;
	}

	/* Create an iterator on the upstream component. */
	upstream_iterator = bt_self_component_port_input_message_iterator_create(
		input_port);
	if (!upstream_iterator) {
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_NOMEM;
		goto end;
	}

	debug_info_msg_iter = g_new0(struct debug_info_msg_iter, 1);
	if (!debug_info_msg_iter) {
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_NOMEM;
		goto end;
	}

	debug_info_msg_iter->debug_info_map = g_hash_table_new_full(
			g_direct_hash, g_direct_equal,
			(GDestroyNotify) NULL,
			(GDestroyNotify) debug_info_destroy);
	if (!debug_info_msg_iter->debug_info_map) {
		g_free(debug_info_msg_iter);
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_NOMEM;
		goto end;
	}

	debug_info_msg_iter->self_comp =
		bt_self_component_filter_as_self_component(self_comp);

	BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_MOVE_REF(
		debug_info_msg_iter->msg_iter, upstream_iterator);

	debug_info_msg_iter->debug_info_component = bt_self_component_get_data(
				bt_self_component_filter_as_self_component(
					self_comp));

	debug_info_msg_iter->ir_maps = trace_ir_maps_create(
			bt_self_component_filter_as_self_component(self_comp),
			debug_info_msg_iter->debug_info_component->arg_debug_info_field_name);
	if (!debug_info_msg_iter->ir_maps) {
		g_hash_table_destroy(debug_info_msg_iter->debug_info_map);
		g_free(debug_info_msg_iter);
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_NOMEM;
		goto end;
	}

	bt_self_message_iterator_set_data(self_msg_iter, debug_info_msg_iter);

	debug_info_msg_iter->input_iterator = self_msg_iter;

end:
	return status;
}

BT_HIDDEN
bt_bool debug_info_msg_iter_can_seek_beginning(
		bt_self_message_iterator *self_msg_iter)
{
	struct debug_info_msg_iter *debug_info_msg_iter =
		bt_self_message_iterator_get_data(self_msg_iter);
	BT_ASSERT(debug_info_msg_iter);

	return bt_self_component_port_input_message_iterator_can_seek_beginning(
			debug_info_msg_iter->msg_iter);
}

BT_HIDDEN
bt_self_message_iterator_status debug_info_msg_iter_seek_beginning(
		bt_self_message_iterator *self_msg_iter)
{
	struct debug_info_msg_iter *debug_info_msg_iter =
		bt_self_message_iterator_get_data(self_msg_iter);
	bt_message_iterator_status status = BT_MESSAGE_ITERATOR_STATUS_OK;

	BT_ASSERT(debug_info_msg_iter);

	/* Ask the upstream component to seek to the beginning. */
	status = bt_self_component_port_input_message_iterator_seek_beginning(
		debug_info_msg_iter->msg_iter);
	if (status != BT_MESSAGE_ITERATOR_STATUS_OK) {
		goto end;
	}

	/* Clear this iterator data. */
	trace_ir_maps_clear(debug_info_msg_iter->ir_maps);
	g_hash_table_remove_all(debug_info_msg_iter->debug_info_map);
end:
	return bt_common_message_iterator_status_to_self(status);
}

BT_HIDDEN
void debug_info_msg_iter_finalize(bt_self_message_iterator *it)
{
	struct debug_info_msg_iter *debug_info_msg_iter;

	debug_info_msg_iter = bt_self_message_iterator_get_data(it);
	BT_ASSERT(debug_info_msg_iter);

	bt_self_component_port_input_message_iterator_put_ref(
			debug_info_msg_iter->msg_iter);

	trace_ir_maps_destroy(debug_info_msg_iter->ir_maps);
	g_hash_table_destroy(debug_info_msg_iter->debug_info_map);

	g_free(debug_info_msg_iter);
}
