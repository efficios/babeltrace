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
#include <babeltrace/babeltrace.h>
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

extern int yydebug;

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

/*
 * TODO: for now, we treat the metadata file as a simple text file
 * (without any header nor packets nor padding).
 */
static
int ctf_open_trace_metadata_read(struct trace_descriptor *td)
{
	struct ctf_scanner *scanner;
	FILE *fp;
	int ret = 0;

	td->ctf_trace.metadata.fd = openat(td->ctf_trace.dirfd,
			"metadata", O_RDONLY);
	if (td->ctf_trace.metadata.fd < 0) {
		fprintf(stdout, "Unable to open metadata.\n");
		return td->ctf_trace.metadata.fd;
	}

	if (babeltrace_debug)
		yydebug = 1;

	fp = fdopen(td->ctf_trace.metadata.fd, "r");
	if (!fp) {
		fprintf(stdout, "Unable to open metadata stream.\n");
		ret = -errno;
		goto end_stream;
	}

	scanner = ctf_scanner_alloc(fp);
	if (!scanner) {
		fprintf(stdout, "Error allocating scanner\n");
		ret = -ENOMEM;
		goto end_scanner_alloc;
	}
	ret = ctf_scanner_append_ast(scanner);
	if (ret) {
		fprintf(stdout, "Error creating AST\n");
		goto end;
	}

	if (babeltrace_debug) {
		ret = ctf_visitor_print_xml(stdout, 0, &scanner->ast->root);
		if (ret) {
			fprintf(stdout, "Error visiting AST for XML output\n");
			goto end;
		}
	}

	ret = ctf_visitor_semantic_check(stdout, 0, &scanner->ast->root);
	if (ret) {
		fprintf(stdout, "Error in CTF semantic validation %d\n", ret);
		goto end;
	}
	ret = ctf_visitor_construct_metadata(stdout, 0, &scanner->ast->root,
			&td->ctf_trace, BYTE_ORDER);
	if (ret) {
		fprintf(stdout, "Error in CTF metadata constructor %d\n", ret);
		goto end;
	}
end:
	ctf_scanner_free(scanner);
end_scanner_alloc:
	fclose(fp);
end_stream:
	close(td->ctf_trace.metadata.fd);
	return ret;
}

static
int ctf_open_trace_read(struct trace_descriptor *td, const char *path, int flags)
{
	int ret;
	struct dirent *dirent;
	struct dirent *diriter;
	size_t dirent_len;

	td->ctf_trace.flags = flags;

	/* Open trace directory */
	td->ctf_trace.dir = opendir(path);
	if (!td->ctf_trace.dir) {
		fprintf(stdout, "Unable to open trace directory.\n");
		ret = -ENOENT;
		goto error;
	}

	td->ctf_trace.dirfd = open(path, 0);
	if (td->ctf_trace.dirfd < 0) {
		fprintf(stdout, "Unable to open trace directory file descriptor.\n");
		ret = -ENOENT;
		goto error_dirfd;
	}
	/*
	 * Keep the metadata file separate.
	 */

	ret = ctf_open_trace_metadata_read(td);
	if (ret) {
		goto error_metadata;
	}

	/*
	 * Open each stream: for each file, try to open, check magic
	 * number, and get the stream ID to add to the right location in
	 * the stream array.
	 */

	dirent_len = offsetof(struct dirent, d_name) +
			fpathconf(td->ctf_trace.dirfd, _PC_NAME_MAX) + 1;

	dirent = malloc(dirent_len);

	for (;;) {
		ret = readdir_r(td->ctf_trace.dir, dirent, &diriter);
		if (ret) {
			fprintf(stdout, "Readdir error.\n");
			goto readdir_error;
			
		}
		if (!diriter)
			break;
		if (!strcmp(diriter->d_name, ".")
				|| !strcmp(diriter->d_name, "..")
				|| !strcmp(diriter->d_name, "metadata"))
			continue;
	}

	free(dirent);
	return 0;

readdir_error:
	free(dirent);
error_metadata:
	close(td->ctf_trace.dirfd);
error_dirfd:
	closedir(td->ctf_trace.dir);
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

	/* Open trace directory */
	td->ctf_trace.dir = opendir(path);
	if (!td->ctf_trace.dir) {
		fprintf(stdout, "Unable to open trace directory.\n");
		ret = -ENOENT;
		goto error;
	}
	

	return 0;

error:
	return ret;
}

struct trace_descriptor *ctf_open_trace(const char *path, int flags)
{
	struct trace_descriptor *td;
	int ret;

	td = g_new0(struct trace_descriptor, 1);

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
