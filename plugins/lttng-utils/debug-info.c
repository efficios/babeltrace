/*
 * Babeltrace - Debug Information State Tracker
 *
 * Copyright (c) 2015 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2015 Philippe Proulx <pproulx@efficios.com>
 * Copyright (c) 2015 Antoine Busque <abusque@efficios.com>
 * Copyright (c) 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/assert-internal.h>
#include <glib.h>
#include "debug-info.h"
#include "bin-info.h"
#include "utils.h"
#include "copy.h"

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
			"lttng_ust_statedump:debug_link)");
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
	free(debug_info_src->src_path);
	free(debug_info_src->bin_path);
	free(debug_info_src->bin_loc);
	g_free(debug_info_src);
}

static
struct debug_info_source *debug_info_source_create_from_bin(struct bin_info *bin,
		uint64_t ip)
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
		BT_LOGD("Failed to lookup source location: ret=%d", ret);
	}

	if (src_loc) {
		debug_info_src->line_no = src_loc->line_no;

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
struct debug_info *debug_info_create(struct debug_info_component *comp)
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

end:
	return debug_info;
error:
	g_free(debug_info);
	return NULL;
}

BT_HIDDEN
void debug_info_destroy(struct debug_info *debug_info)
{
	if (!debug_info) {
		goto end;
	}

	if (debug_info->vpid_to_proc_dbg_info_src) {
		g_hash_table_destroy(debug_info->vpid_to_proc_dbg_info_src);
	}

	g_free(debug_info);
end:
	return;
}

static
void handle_statedump_build_id_event(FILE *err, struct debug_info *debug_info,
		struct bt_event *event)
{
	struct proc_debug_info_sources *proc_dbg_info_src;
	struct bin_info *bin = NULL;
	int ret;
	int64_t vpid;
	uint64_t baddr;
	uint64_t build_id_len;

	ret = get_stream_event_context_int_field_value(err,
			event, VPID_FIELD_NAME, &vpid);
	if (ret) {
		goto end;
	}

	ret = get_payload_unsigned_int_field_value(err,
			event, BADDR_FIELD_NAME, &baddr);
	if (ret) {
		BT_LOGE_STR("Failed to get unsigned int value for "
			VPID_FIELD_NAME " field.");
		goto end;
	}

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

	ret = get_payload_build_id_field_value(err, event, BUILD_ID_FIELD_NAME,
			&bin->build_id, &build_id_len);
	if (ret) {
		BT_LOGE_STR("Failed to get " BUILD_ID_FIELD_NAME
			" field value.");
		goto end;
	}
	if (build_id_len > SIZE_MAX) {
		bin->build_id_len = (size_t) build_id_len;
	}

	/*
	 * Reset the is_elf_only flag in case it had been set
	 * previously, because we might find separate debug info using
	 * the new build id information.
	 */
	bin->is_elf_only = false;

	// TODO
	//	bin_info_set_build_id(bin, build_id, build_id_len);

end:
	return;
}

static
void handle_statedump_debug_link_event(FILE *err, struct debug_info *debug_info,
		struct bt_event *event)
{
	struct proc_debug_info_sources *proc_dbg_info_src;
	struct bin_info *bin = NULL;
	int64_t vpid;
	uint64_t baddr;
	const char *filename = NULL;
	uint32_t crc32;
	uint64_t tmp;
	int ret;

	ret = get_stream_event_context_int_field_value(err, event,
			VPID_FIELD_NAME, &vpid);
	if (ret) {
		goto end;
	}

	ret = get_payload_unsigned_int_field_value(err,
			event, BADDR_FIELD_NAME, &baddr);
	if (ret) {
		BT_LOGE_STR("Failed to get unsigned int value for "
			BADDR_FIELD_NAME " field.");
		ret = -1;
		goto end;
	}

	ret = get_payload_unsigned_int_field_value(err, event, CRC32_FIELD_NAME,
		&tmp);
	if (ret) {
		BT_LOGE_STR("Failed to get unsigned int value for "
			CRC32_FIELD_NAME " field.");
		ret = -1;
		goto end;
	}
	crc32 = (uint32_t) tmp;

	ret = get_payload_string_field_value(err,
			event, FILENAME_FIELD_NAME, &filename);
	if (ret) {
		BT_LOGE_STR("Failed to get string value for "
			FILENAME_FIELD_NAME " field.");
		ret = -1;
		goto end;
	}

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
void handle_bin_info_event(FILE *err, struct debug_info *debug_info,
		struct bt_event *event, bool has_pic_field)
{
	struct proc_debug_info_sources *proc_dbg_info_src;
	struct bin_info *bin;
	uint64_t baddr, memsz;
	int64_t vpid;
	const char *path;
	gpointer key = NULL;
	bool is_pic;
	int ret;

	ret = get_payload_unsigned_int_field_value(err,
			event, BADDR_FIELD_NAME, &baddr);
	if (ret) {
		BT_LOGE_STR("Failed to get unsigned int value for "
			BADDR_FIELD_NAME " field.");
		goto end;
	}

	ret = get_payload_unsigned_int_field_value(err,
			event, MEMSZ_FIELD_NAME, &memsz);
	if (ret) {
		BT_LOGE_STR("Failed to get unsigned int value for "
			MEMSZ_FIELD_NAME " field.");
		goto end;
	}

	/*
	 * This field is not produced by the dlopen event emitted before
	 * lttng-ust 2.9.
	 */
	ret = get_payload_string_field_value(err,
			event, PATH_FIELD_NAME, &path);
	if (ret || !path) {
		goto end;
	}

	if (has_pic_field) {
		uint64_t tmp;

		ret = get_payload_unsigned_int_field_value(err,
				event, IS_PIC_FIELD_NAME, &tmp);
		if (ret) {
		BT_LOGE_STR("Failed to get unsigned int value for "
			IS_PIC_FIELD_NAME " field.");
			ret = -1;
			goto end;
		}
		is_pic = (tmp == 1);
	} else {
		/*
		 * dlopen has no is_pic field, because the shared
		 * object is always PIC.
		 */
		is_pic = true;
	}

	ret = get_stream_event_context_int_field_value(err, event,
		VPID_FIELD_NAME, &vpid);
	if (ret) {
		goto end;
	}

	if (memsz == 0) {
		/* Ignore VDSO. */
		goto end;
	}

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
void handle_statedump_bin_info_event(FILE *err, struct debug_info *debug_info,
		struct bt_event *event)
{
	handle_bin_info_event(err, debug_info, event, true);
}

static inline
void handle_lib_load_event(FILE *err, struct debug_info *debug_info,
		struct bt_event *event)
{
	handle_bin_info_event(err, debug_info, event, false);
}

static inline
void handle_lib_unload_event(FILE *err, struct debug_info *debug_info,
		struct bt_event *event)
{
	struct proc_debug_info_sources *proc_dbg_info_src;
	uint64_t baddr;
	int64_t vpid;
	gpointer key_ptr = NULL;
	int ret;

	ret = get_payload_unsigned_int_field_value(err,
			event, BADDR_FIELD_NAME, &baddr);
	if (ret) {
		BT_LOGE_STR("Failed to get unsigned int value for "
			BADDR_FIELD_NAME " field.");
		ret = -1;
		goto end;
	}

	ret = get_stream_event_context_int_field_value(err, event,
		VPID_FIELD_NAME, &vpid);
	if (ret) {
		goto end;
	}

	proc_dbg_info_src = proc_debug_info_sources_ht_get_entry(
			debug_info->vpid_to_proc_dbg_info_src, vpid);
	if (!proc_dbg_info_src) {
		goto end;
	}

	key_ptr = (gpointer) &baddr;
	(void) g_hash_table_remove(proc_dbg_info_src->baddr_to_bin_info,
			key_ptr);
end:
	return;
}

static
void handle_statedump_start(FILE *err, struct debug_info *debug_info,
		struct bt_event *event)
{
	struct proc_debug_info_sources *proc_dbg_info_src;
	int64_t vpid;
	int ret;

	ret = get_stream_event_context_int_field_value(err, event,
			VPID_FIELD_NAME, &vpid);
	if (ret) {
		goto end;
	}

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

BT_HIDDEN
void debug_info_handle_event(FILE *err, struct bt_event *event,
		struct debug_info *debug_info)
{
	struct bt_event_class *event_class;
	const char *event_name;
	GQuark q_event_name;

	if (!debug_info || !event) {
		goto end;
	}
	event_class = bt_event_get_class(event);
	if (!event_class) {
		goto end;
	}
	event_name = bt_event_class_get_name(event_class);
	if (!event_name) {
		goto end_put_class;
	}
	q_event_name = g_quark_try_string(event_name);

	if (q_event_name == debug_info->q_statedump_bin_info) {
		/* State dump */
		handle_statedump_bin_info_event(err, debug_info, event);
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
		handle_lib_load_event(err, debug_info, event);
	} else if (q_event_name == debug_info->q_statedump_start) {
		/* Start state dump */
		handle_statedump_start(err, debug_info, event);
	} else if (q_event_name == debug_info->q_statedump_debug_link) {
		/* Debug link info */
		handle_statedump_debug_link_event(err, debug_info, event);
	} else if (q_event_name == debug_info->q_statedump_build_id) {
		/* Build ID info */
		handle_statedump_build_id_event(err, debug_info, event);
	} else if (q_event_name == debug_info-> q_lib_unload) {
		handle_lib_unload_event(err, debug_info, event);
	}

end_put_class:
	bt_put(event_class);
end:
	return;
}
