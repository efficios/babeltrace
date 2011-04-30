/*
 * BabelTrace - Common Trace Format (CTF)
 *
 * Format registration.
 *
 * Copyright 2010, 2011 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

#include <babeltrace/format.h>
#include <babeltrace/ctf/types.h>
#include <babeltrace/ctf/metadata.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <glib.h>

struct trace_descriptor {
	struct ctf_trace ctf_trace;
};

struct trace_descriptor *ctf_open_trace(const char *path, int flags);
void ctf_close_trace(struct trace_descriptor *descriptor);

static struct format ctf_format = {
	.uint_read = ctf_uint_read,
	.int_read = ctf_int_read,
	.uint_write = ctf_uint_write,
	.int_write = ctf_int_write,
	.double_read = ctf_double_read,
	.double_write = ctf_double_write,
	.float_copy = ctf_float_copy,
	.string_copy = ctf_string_copy,
	.string_read = ctf_string_read,
	.string_write = ctf_string_write,
	.string_free_temp = ctf_string_free_temp,
	.enum_read = ctf_enum_read,
	.enum_write = ctf_enum_write,
	.struct_begin = ctf_struct_begin,
	.struct_end = ctf_struct_end,
	.variant_begin = ctf_variant_begin,
	.variant_end = ctf_variant_end,
	.array_begin = ctf_array_begin,
	.array_end = ctf_array_end,
	.sequence_begin = ctf_sequence_begin,
	.sequence_end = ctf_sequence_end,
	.open_trace = ctf_open_trace,
	.close_trace = ctf_close_trace,
};

static
int ctf_open_trace_read(struct trace_descriptor *td, const char *path, int flags)
{
	int ret;

	td->ctf_trace.flags = flags;

	/* Open trace directory */
	td->ctf_trace.dir = opendir(path);
	if (!td->ctf_trace.dir) {
		fprintf(stdout, "Unable to open trace directory.\n");
		ret = -ENOENT;
		goto error;
	}



	/*
	 * Open each stream: for each file, try to open, check magic
	 * number, and get the stream ID to add to the right location in
	 * the stream array.
	 *
	 * Keep the metadata file separate.
	 */



	/*
	 * Use the metadata file to populate the trace metadata.
	 */


	return 0;
error:
	return ret;
}

static
int ctf_open_trace_write(struct trace_descriptor *td, const char *path, int flags)
{
	int ret;

	ret = mkdir(path, S_IRWXU|S_IRWXG);
	if (ret)
		return ret;

	return 0;
}

struct trace_descriptor *ctf_open_trace(const char *path, int flags)
{
	struct trace_descriptor *td;
	int ret;

	td = g_new(struct trace_descriptor, 1);

	switch (flags) {
	case O_RDONLY:
		ret = ctf_open_trace_read(td, path, flags);
		if (ret)
			goto error;
		break;
	case O_WRONLY:
		ret = ctf_open_trace_write(td, path, flags);
		if (ret)
			goto error;
		break;
	default:
		fprintf(stdout, "Incorrect open flags.\n");
		goto error;
	}

	return td;
error:
	g_free(td);
	return NULL;
}

void ctf_close_trace(struct trace_descriptor *td)
{
	closedir(td->ctf_trace.dir);
	g_free(td);
}

void __attribute__((constructor)) ctf_init(void)
{
	int ret;

	ctf_format.name = g_quark_from_static_string("ctf");
	ret = bt_register_format(&ctf_format);
	assert(!ret);
}

/* TODO: finalize */
