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

#include <assert.h>
#include <glib.h>
#include <babeltrace/types.h>
#include <babeltrace/ctf-ir/metadata.h>
#include <babeltrace/debug-info.h>
#include <babeltrace/bin-info.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/utils.h>

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
		printf_verbose("Failed to lookup source location (err: %i)\n", ret);
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

	dbg_info_src = proc_debug_info_sources_get_entry(
			proc_dbg_info_src, ip);
	if (!dbg_info_src) {
		goto end;
	}

end:
	return dbg_info_src;
}

BT_HIDDEN
struct debug_info *debug_info_create(void)
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
void handle_statedump_build_id_event(struct debug_info *debug_info,
		struct ctf_event_definition *event_def)
{
	struct proc_debug_info_sources *proc_dbg_info_src;
	struct bt_definition *event_fields_def = NULL;
	struct bt_definition *sec_def = NULL;
	struct bt_definition *baddr_def = NULL;
	struct bt_definition *vpid_def = NULL;
	struct bt_definition *build_id_def = NULL;
	struct definition_sequence *build_id_seq;
	struct bin_info *bin = NULL;
	int i;
	int64_t vpid;
	uint64_t baddr;
	uint8_t *build_id = NULL;
	uint64_t build_id_len;

	event_fields_def = (struct bt_definition *) event_def->event_fields;
	sec_def = (struct bt_definition *)
			event_def->stream->stream_event_context;

	if (!event_fields_def || !sec_def) {
		goto end;
	}

	baddr_def = bt_lookup_definition(event_fields_def, "_baddr");
	if (!baddr_def) {
		goto end;
	}

	vpid_def = bt_lookup_definition(sec_def, "_vpid");
	if (!vpid_def) {
		goto end;
	}

	build_id_def = bt_lookup_definition(event_fields_def, "_build_id");
	if (!build_id_def) {
		goto end;
	}

	if (baddr_def->declaration->id != CTF_TYPE_INTEGER) {
		goto end;
	}

	if (vpid_def->declaration->id != CTF_TYPE_INTEGER) {
		goto end;
	}

	if (build_id_def->declaration->id != CTF_TYPE_SEQUENCE) {
		goto end;
	}

	baddr = bt_get_unsigned_int(baddr_def);
	vpid = bt_get_signed_int(vpid_def);
	build_id_seq = container_of(build_id_def,
			struct definition_sequence, p);
	build_id_len = build_id_seq->length->value._unsigned;

	build_id = g_malloc(build_id_len);
	if (!build_id) {
		goto end;
	}

	for (i = 0; i < build_id_len; ++i) {
		struct bt_definition **field;

		field = (struct bt_definition **) &g_ptr_array_index(
				build_id_seq->elems, i);
		build_id[i] = bt_get_unsigned_int(*field);
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

	bin_info_set_build_id(bin, build_id, build_id_len);

end:
	free(build_id);
	return;
}

static
void handle_statedump_debug_link_event(struct debug_info *debug_info,
		struct ctf_event_definition *event_def)
{
	struct proc_debug_info_sources *proc_dbg_info_src;
	struct bt_definition *event_fields_def = NULL;
	struct bt_definition *sec_def = NULL;
	struct bt_definition *baddr_def = NULL;
	struct bt_definition *vpid_def = NULL;
	struct bt_definition *filename_def = NULL;
	struct bt_definition *crc32_def = NULL;
	struct bin_info *bin = NULL;
	int64_t vpid;
	uint64_t baddr;
	char *filename = NULL;
	uint32_t crc32;

	event_fields_def = (struct bt_definition *) event_def->event_fields;
	sec_def = (struct bt_definition *)
			event_def->stream->stream_event_context;

	if (!event_fields_def || !sec_def) {
		goto end;
	}

	baddr_def = bt_lookup_definition(event_fields_def, "_baddr");
	if (!baddr_def) {
		goto end;
	}

	vpid_def = bt_lookup_definition(sec_def, "_vpid");
	if (!vpid_def) {
		goto end;
	}

	filename_def = bt_lookup_definition(event_fields_def, "_filename");
	if (!filename_def) {
		goto end;
	}

	crc32_def = bt_lookup_definition(event_fields_def, "_crc32");
	if (!crc32_def) {
		goto end;
	}

	if (baddr_def->declaration->id != CTF_TYPE_INTEGER) {
		goto end;
	}

	if (vpid_def->declaration->id != CTF_TYPE_INTEGER) {
		goto end;
	}

	if (filename_def->declaration->id != CTF_TYPE_STRING) {
		goto end;
	}

	if (crc32_def->declaration->id != CTF_TYPE_INTEGER) {
		goto end;
	}

	baddr = bt_get_unsigned_int(baddr_def);
	vpid = bt_get_signed_int(vpid_def);

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

	filename = bt_get_string(filename_def);
	crc32 = bt_get_unsigned_int(crc32_def);

	bin_info_set_debug_link(bin, filename, crc32);

end:
	return;
}

static
void handle_bin_info_event(struct debug_info *debug_info,
		struct ctf_event_definition *event_def, bool has_pic_field)
{
	struct bt_definition *baddr_def = NULL;
	struct bt_definition *memsz_def = NULL;
	struct bt_definition *path_def = NULL;
	struct bt_definition *is_pic_def = NULL;
	struct bt_definition *vpid_def = NULL;
	struct bt_definition *event_fields_def = NULL;
	struct bt_definition *sec_def = NULL;
	struct proc_debug_info_sources *proc_dbg_info_src;
	struct bin_info *bin;
	uint64_t baddr, memsz;
	int64_t vpid;
	const char *path;
	gpointer key = NULL;
	bool is_pic;

	event_fields_def = (struct bt_definition *) event_def->event_fields;
	sec_def = (struct bt_definition *)
		event_def->stream->stream_event_context;

	if (!event_fields_def || !sec_def) {
		goto end;
	}

	baddr_def = bt_lookup_definition(event_fields_def, "_baddr");
	if (!baddr_def) {
		goto end;
	}

	memsz_def = bt_lookup_definition(event_fields_def, "_memsz");
	if (!memsz_def) {
		goto end;
	}

	path_def = bt_lookup_definition(event_fields_def, "_path");
	if (!path_def) {
		goto end;
	}

	if (has_pic_field) {
		is_pic_def = bt_lookup_definition(event_fields_def, "_is_pic");
		if (!is_pic_def) {
			goto end;
		}

		if (is_pic_def->declaration->id != CTF_TYPE_INTEGER) {
			goto end;
		}

		is_pic = (bt_get_unsigned_int(is_pic_def) == 1);
	} else {
		/*
		 * dlopen has no is_pic field, because the shared
		 * object is always PIC.
		 */
		is_pic = true;
	}

	vpid_def = bt_lookup_definition(sec_def, "_vpid");
	if (!vpid_def) {
		goto end;
	}

	if (baddr_def->declaration->id != CTF_TYPE_INTEGER) {
		goto end;
	}

	if (memsz_def->declaration->id != CTF_TYPE_INTEGER) {
		goto end;
	}

	if (path_def->declaration->id != CTF_TYPE_STRING) {
		goto end;
	}

	if (vpid_def->declaration->id != CTF_TYPE_INTEGER) {
		goto end;
	}

	baddr = bt_get_unsigned_int(baddr_def);
	memsz = bt_get_unsigned_int(memsz_def);
	path = bt_get_string(path_def);
	vpid = bt_get_signed_int(vpid_def);

	if (!path) {
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

	bin = bin_info_create(path, baddr, memsz, is_pic);
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
void handle_statedump_bin_info_event(struct debug_info *debug_info,
		struct ctf_event_definition *event_def)
{
	handle_bin_info_event(debug_info, event_def, true);
}

static inline
void handle_dlopen_event(struct debug_info *debug_info,
		struct ctf_event_definition *event_def)
{
	handle_bin_info_event(debug_info, event_def, false);
}


static
void handle_statedump_start(struct debug_info *debug_info,
		struct ctf_event_definition *event_def)
{
	struct bt_definition *vpid_def = NULL;
	struct bt_definition *sec_def = NULL;
	struct proc_debug_info_sources *proc_dbg_info_src;
	int64_t vpid;

	sec_def = (struct bt_definition *)
			event_def->stream->stream_event_context;
	if (!sec_def) {
		goto end;
	}

	vpid_def = bt_lookup_definition(sec_def, "_vpid");
	if (!vpid_def) {
		goto end;
	}

	vpid = bt_get_signed_int(vpid_def);

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

static
void register_event_debug_infos(struct debug_info *debug_info,
		struct ctf_event_definition *event)
{
	struct bt_definition *ip_def, *vpid_def;
	int64_t vpid;
	uint64_t ip;
	struct bt_definition *sec_def;

	/* Get stream event context definition. */
	sec_def = (struct bt_definition *) event->stream->stream_event_context;
	if (!sec_def) {
		goto end;
	}

	/* Get "ip" and "vpid" definitions. */
	vpid_def = bt_lookup_definition((struct bt_definition *) sec_def,
			 "_vpid");
	ip_def = bt_lookup_definition((struct bt_definition *) sec_def, "_ip");

	if (!vpid_def || !ip_def) {
		 goto end;
	}

	vpid = bt_get_signed_int(vpid_def);
	ip = bt_get_unsigned_int(ip_def);

	/* Get debug info for this context. */
	((struct definition_integer *) ip_def)->debug_info_src =
			debug_info_query(debug_info, vpid, ip);

end:
	return;
}

BT_HIDDEN
void debug_info_handle_event(struct debug_info *debug_info,
		struct ctf_event_definition *event)
{
	struct ctf_event_declaration *event_class;
	struct ctf_stream_declaration *stream_class;

	if (!debug_info || !event) {
		goto end;
	}

	stream_class = event->stream->stream_class;
	event_class = g_ptr_array_index(stream_class->events_by_id,
			event->stream->event_id);

	if (event_class->name == debug_info->q_statedump_bin_info) {
		/* State dump */
		handle_statedump_bin_info_event(debug_info, event);
	} else if (event_class->name == debug_info->q_dl_open) {
		handle_dlopen_event(debug_info, event);
	} else if (event_class->name == debug_info->q_statedump_start) {
		/* Start state dump */
		handle_statedump_start(debug_info, event);
	} else if (event_class->name == debug_info->q_statedump_debug_link) {
		/* Debug link info */
		handle_statedump_debug_link_event(debug_info, event);
	} else if (event_class->name == debug_info->q_statedump_build_id) {
		/* Build ID info */
		handle_statedump_build_id_event(debug_info, event);
	}

	/* All events: register debug infos */
	register_event_debug_infos(debug_info, event);

end:
	return;
}
