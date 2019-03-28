/*
 * bin-info.c
 *
 * Babeltrace - Executable and Shared Object Debug Info Reader
 *
 * Copyright 2015 Antoine Busque <abusque@efficios.com>
 *
 * Author: Antoine Busque <abusque@efficios.com>
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

#define BT_LOG_TAG "PLUGIN-CTF-LTTNG-UTILS-DEBUG-INFO-FLT-BIN-INFO"
#include "logging.h"

#include <fcntl.h>
#include <math.h>
#include <libgen.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dwarf.h>
#include <glib.h>
#include <errno.h>
#include "dwarf.h"
#include "bin-info.h"
#include "crc32.h"
#include "utils.h"

/*
 * An address printed in hex is at most 20 bytes (16 for 64-bits +
 * leading 0x + optional leading '+' if addr is an offset + null
 * character).
 */
#define ADDR_STR_LEN 20
#define BUILD_ID_NOTE_NAME "GNU"

BT_HIDDEN
int bin_info_init(void)
{
	int ret = 0;

	if (elf_version(EV_CURRENT) == EV_NONE) {
		BT_LOGD("ELF library initialization failed: %s.",
			elf_errmsg(-1));
		ret = -1;
	}

	return ret;
}

BT_HIDDEN
struct bin_info *bin_info_create(const char *path, uint64_t low_addr,
		uint64_t memsz, bool is_pic, const char *debug_info_dir,
		const char *target_prefix)
{
	struct bin_info *bin = NULL;

	if (!path) {
		goto error;
	}

	bin = g_new0(struct bin_info, 1);
	if (!bin) {
		goto error;
	}

	if (target_prefix) {
		bin->elf_path = g_build_path("/", target_prefix,
						path, NULL);
	} else {
		bin->elf_path = g_strdup(path);
	}

	if (!bin->elf_path) {
		goto error;
	}

	if (debug_info_dir) {
		bin->debug_info_dir = g_strdup(debug_info_dir);
		if (!bin->debug_info_dir) {
			goto error;
		}
	}

	bin->is_pic = is_pic;
	bin->memsz = memsz;
	bin->low_addr = low_addr;
	bin->high_addr = bin->low_addr + bin->memsz;
	bin->build_id = NULL;
	bin->build_id_len = 0;
	bin->file_build_id_matches = false;

	return bin;

error:
	bin_info_destroy(bin);
	return NULL;
}

BT_HIDDEN
void bin_info_destroy(struct bin_info *bin)
{
	if (!bin) {
		return;
	}

	dwarf_end(bin->dwarf_info);

	g_free(bin->debug_info_dir);
	g_free(bin->elf_path);
	g_free(bin->dwarf_path);
	g_free(bin->build_id);
	g_free(bin->dbg_link_filename);

	elf_end(bin->elf_file);

	close(bin->elf_fd);
	close(bin->dwarf_fd);

	g_free(bin);
}

/**
 * Initialize the ELF file for a given executable.
 *
 * @param bin	bin_info instance
 * @returns	0 on success, negative value on error.
 */
static
int bin_info_set_elf_file(struct bin_info *bin)
{
	int elf_fd = -1;
	Elf *elf_file = NULL;

	if (!bin) {
		goto error;
	}

	elf_fd = open(bin->elf_path, O_RDONLY);
	if (elf_fd < 0) {
		elf_fd = -errno;
		BT_LOGE("Failed to open %s\n", bin->elf_path);
		goto error;
	}

	elf_file = elf_begin(elf_fd, ELF_C_READ, NULL);
	if (!elf_file) {
		BT_LOGE("elf_begin failed: %s\n", elf_errmsg(-1));
		goto error;
	}

	if (elf_kind(elf_file) != ELF_K_ELF) {
		BT_LOGE("Error: %s is not an ELF object\n",
				bin->elf_path);
		goto error;
	}

	bin->elf_fd = elf_fd;
	bin->elf_file = elf_file;
	return 0;

error:
	if (elf_fd >= 0) {
		close(elf_fd);
		elf_fd = -1;
	}
	elf_end(elf_file);
	return elf_fd;
}

/**
 * From a note section data buffer, check if it is a build id note.
 *
 * @param buf			Pointer to a note section
 *
 * @returns			1 on match, 0 if `buf` does not contain a
 *				valid build id note
 */
static
int is_build_id_note_section(uint8_t *buf)
{
	int ret = 0;
	uint32_t name_sz, desc_sz, note_type;

	/* The note section header has 3 32bit integer for the following:
	 * - Section name size
	 * - Description size
	 * - Note type
	 */
	name_sz = (uint32_t) *buf;
	buf += sizeof(name_sz);

	buf += sizeof(desc_sz);

	note_type = (uint32_t) *buf;
	buf += sizeof(note_type);

	/* Check the note type. */
	if (note_type != NT_GNU_BUILD_ID) {
		goto invalid;
	}

	/* Check the note name. */
	if (memcmp(buf, BUILD_ID_NOTE_NAME, name_sz) != 0) {
		goto invalid;
	}

	ret = 1;

invalid:
	return ret;
}

/**
 *  From a build id note section data buffer, check if the build id it contains
 *  is identical to the build id passed as parameter.
 *
 * @param file_build_id_note	Pointer to the file build id note section.
 * @param build_id		Pointer to a build id to compare to.
 * @param build_id_len		length of the build id.
 *
 * @returns			1 on match, 0 otherwise.
 */
static
int is_build_id_note_section_matching(uint8_t *file_build_id_note,
		uint8_t *build_id, size_t build_id_len)
{
	uint32_t name_sz, desc_sz, note_type;

	if (build_id_len <= 0) {
		goto end;
	}

	/* The note section header has 3 32bit integer for the following:
	 * - Section name size
	 * - Description size
	 * - Note type
	 */
	name_sz = (uint32_t) *file_build_id_note;
	file_build_id_note += sizeof(name_sz);
	file_build_id_note += sizeof(desc_sz);
	file_build_id_note += sizeof(note_type);

	/*
	 * Move the pointer pass the name char array. This corresponds to the
	 * beginning of the description section. The description is the build
	 * id in the case of a build id note.
	 */
	file_build_id_note += name_sz;

	/*
	 * Compare the binary build id with the supplied build id.
	 */
	if (memcmp(build_id, file_build_id_note, build_id_len) == 0) {
		return 1;
	}
end:
	return 0;
}

/**
 * Checks if the build id stored in `bin` (bin->build_id) is matching the build
 * id of the ondisk file (bin->elf_file).
 *
 * @param bin			bin_info instance
 * @param build_id		build id to compare ot the on disk file
 * @param build_id_len		length of the build id
 *
 * @returns			1 on if the build id of stored in `bin` matches
 *				the build id of the ondisk file.
 *				0 on if they are different or an error occured.
 */
static
int is_build_id_matching(struct bin_info *bin)
{
	int ret, is_build_id, is_matching = 0;
	Elf_Scn *curr_section = NULL, *next_section = NULL;
	Elf_Data *note_data = NULL;
	GElf_Shdr *curr_section_hdr = NULL;

	if (!bin->build_id) {
		goto error;
	}

	/* Set ELF file if it hasn't been accessed yet. */
	if (!bin->elf_file) {
		ret = bin_info_set_elf_file(bin);
		if (ret) {
			/* Failed to set ELF file. */
			goto error;
		}
	}

	curr_section_hdr = g_new0(GElf_Shdr, 1);
	if (!curr_section_hdr) {
		goto error;
	}

	next_section = elf_nextscn(bin->elf_file, curr_section);
	if (!next_section) {
		goto error;
	}

	while (next_section) {
		curr_section = next_section;
		next_section = elf_nextscn(bin->elf_file, curr_section);

		curr_section_hdr = gelf_getshdr(curr_section, curr_section_hdr);

		if (!curr_section_hdr) {
			goto error;
		}

		if (curr_section_hdr->sh_type != SHT_NOTE) {
			continue;
		}

		note_data = elf_getdata(curr_section, NULL);
		if (!note_data) {
			goto error;
		}

		/* Check if the note is of the build-id type. */
		is_build_id = is_build_id_note_section(note_data->d_buf);
		if (!is_build_id) {
			continue;
		}

		/*
		 * Compare the build id of the on-disk file and
		 * the build id recorded in the trace.
		 */
		is_matching = is_build_id_note_section_matching(note_data->d_buf,
				bin->build_id, bin->build_id_len);
		if (!is_matching) {
			break;
		}
	}
error:
	g_free(curr_section_hdr);
	return is_matching;
}

BT_HIDDEN
int bin_info_set_build_id(struct bin_info *bin, uint8_t *build_id,
		size_t build_id_len)
{
	if (!bin || !build_id) {
		goto error;
	}

	/* Set the build id. */
	bin->build_id = g_new0(uint8_t, build_id_len);
	if (!bin->build_id) {
		goto error;
	}

	memcpy(bin->build_id, build_id, build_id_len);
	bin->build_id_len = build_id_len;

	/*
	 * Check if the file found on the file system has the same build id
	 * that what was recorded in the trace.
	 */
	bin->file_build_id_matches = is_build_id_matching(bin);
	if (!bin->file_build_id_matches) {
		BT_LOGD_STR("Supplied Build ID does not match Build ID of the "
				"binary or library found on the file system.");
		goto error;
	}

	/*
	 * Reset the is_elf_only flag in case it had been set
	 * previously, because we might find separate debug info using
	 * the new build id information.
	 */
	bin->is_elf_only = false;

	return 0;

error:
	return -1;
}

BT_HIDDEN
int bin_info_set_debug_link(struct bin_info *bin, const char *filename,
		uint32_t crc)
{
	if (!bin || !filename) {
		goto error;
	}

	bin->dbg_link_filename = g_strdup(filename);
	if (!bin->dbg_link_filename) {
		goto error;
	}

	bin->dbg_link_crc = crc;

	/*
	 * Reset the is_elf_only flag in case it had been set
	 * previously, because we might find separate debug info using
	 * the new build id information.
	 */
	bin->is_elf_only = false;

	return 0;

error:

	return -1;
}

/**
 * Tries to read DWARF info from the location given by path, and
 * attach it to the given bin_info instance if it exists.
 *
 * @param bin	bin_info instance for which to set DWARF info
 * @param path	Presumed location of the DWARF info
 * @returns	0 on success, negative value on failure
 */
static
int bin_info_set_dwarf_info_from_path(struct bin_info *bin, char *path)
{
	int fd = -1, ret = 0;
	struct bt_dwarf_cu *cu = NULL;
	Dwarf *dwarf_info = NULL;

	if (!bin || !path) {
		goto error;
	}

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		fd = -errno;
		goto error;
	}

	dwarf_info = dwarf_begin(fd, DWARF_C_READ);
	if (!dwarf_info) {
		goto error;
	}

	/*
	 * Check if the dwarf info has any CU. If not, the
	 * executable's object file contains no DWARF info.
	 */
	cu = bt_dwarf_cu_create(dwarf_info);
	if (!cu) {
		goto error;
	}

	ret = bt_dwarf_cu_next(cu);
	if (ret) {
		goto error;
	}

	bin->dwarf_fd = fd;
	bin->dwarf_path = g_strdup(path);
	if (!bin->dwarf_path) {
		goto error;
	}
	bin->dwarf_info = dwarf_info;
	free(cu);

	return 0;

error:
	if (fd >= 0) {
		close(fd);
		fd = -1;
	}
	dwarf_end(dwarf_info);
	g_free(dwarf_info);
	free(cu);

	return fd;
}

/**
 * Try to set the dwarf_info for a given bin_info instance via the
 * build ID method.
 *
 * @param bin		bin_info instance for which to retrieve the
 *			DWARF info via build ID
 * @returns		0 on success (i.e. dwarf_info set), -1 on failure
 */
static
int bin_info_set_dwarf_info_build_id(struct bin_info *bin)
{
	int i = 0, ret = 0;
	char *path = NULL, *build_id_file = NULL;
	const char *dbg_dir = NULL;
	size_t build_id_file_len;

	if (!bin || !bin->build_id) {
		goto error;
	}

	dbg_dir = bin->debug_info_dir ? bin->debug_info_dir : DEFAULT_DEBUG_DIR;

	/* 2 characters per byte printed in hex, +1 for '/' and +1 for '\0' */
	build_id_file_len = (2 * bin->build_id_len) + 1 +
			strlen(BUILD_ID_SUFFIX) + 1;
	build_id_file = g_new0(gchar, build_id_file_len);
	if (!build_id_file) {
		goto error;
	}

	g_snprintf(build_id_file, 4, "%02x/", bin->build_id[0]);
	for (i = 1; i < bin->build_id_len; ++i) {
		int path_idx = 3 + 2 * (i - 1);

		g_snprintf(&build_id_file[path_idx], 3, "%02x", bin->build_id[i]);
	}
	g_strconcat(build_id_file, BUILD_ID_SUFFIX, NULL);

	path = g_build_path("/", dbg_dir, BUILD_ID_SUBDIR, build_id_file, NULL);
	if (!path) {
		goto error;
	}

	ret = bin_info_set_dwarf_info_from_path(bin, path);
	if (ret) {
		goto error;
	}

	goto end;

error:
	ret = -1;
end:
	free(build_id_file);
	free(path);

	return ret;
}

/**
 * Tests whether the file located at path exists and has the expected
 * checksum.
 *
 * This predicate is used when looking up separate debug info via the
 * GNU debuglink method. The expected crc can be found .gnu_debuglink
 * section in the original ELF file, along with the filename for the
 * file containing the debug info.
 *
 * @param path	Full path at which to look for the debug file
 * @param crc	Expected checksum for the debug file
 * @returns	1 if the file exists and has the correct checksum,
 *		0 otherwise
 */
static
int is_valid_debug_file(char *path, uint32_t crc)
{
	int ret = 0, fd = -1;
	uint32_t _crc = 0;

	if (!path) {
		goto end_noclose;
	}

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		goto end_noclose;
	}

	ret = crc32(fd, &_crc);
	if (ret) {
		ret = 0;
		goto end;
	}

	ret = (crc == _crc);

end:
	close(fd);
end_noclose:
	return ret;
}

/**
 * Try to set the dwarf_info for a given bin_info instance via the
 * debug-link method.
 *
 * @param bin		bin_info instance for which to retrieve the
 *			DWARF info via debug link
 * @returns		0 on success (i.e. dwarf_info set), -1 on failure
 */
static
int bin_info_set_dwarf_info_debug_link(struct bin_info *bin)
{
	int ret = 0;
	const gchar *dbg_dir = NULL;
	gchar *bin_dir = NULL, *dir_name = NULL, *path = NULL;

	if (!bin || !bin->dbg_link_filename) {
		goto error;
	}

	dbg_dir = bin->debug_info_dir ? bin->debug_info_dir : DEFAULT_DEBUG_DIR;
	dir_name = g_path_get_dirname(bin->elf_path);
	if (!dir_name) {
		goto error;
	}

	bin_dir = g_strconcat(dir_name, "/", NULL);

	/* First look in the executable's dir */
	path = g_strconcat(bin_dir, bin->dbg_link_filename, NULL);

	if (is_valid_debug_file(path, bin->dbg_link_crc)) {
		goto found;
	}

	/* If not found, look in .debug subdir */
	g_free(path);
	path = g_strconcat(bin_dir, DEBUG_SUBDIR, bin->dbg_link_filename, NULL);

	if (is_valid_debug_file(path, bin->dbg_link_crc)) {
		goto found;
	}

	/* Lastly, look under the global debug directory */
	g_free(path);

	path = g_strconcat(dbg_dir, bin_dir, bin->dbg_link_filename, NULL);
	if (is_valid_debug_file(path, bin->dbg_link_crc)) {
		goto found;
	}

error:
	ret = -1;
end:
	g_free(dir_name);
	g_free(path);

	return ret;

found:
	ret = bin_info_set_dwarf_info_from_path(bin, path);
	if (ret) {
		goto error;
	}

	goto end;
}

/**
 * Initialize the DWARF info for a given executable.
 *
 * @param bin	bin_info instance
 * @returns	0 on success, negative value on failure
 */
static
int bin_info_set_dwarf_info(struct bin_info *bin)
{
	int ret = 0;

	if (!bin) {
		ret = -1;
		goto end;
	}

	/* First try to set the DWARF info from the ELF file */
	ret = bin_info_set_dwarf_info_from_path(bin, bin->elf_path);
	if (!ret) {
		goto end;
	}

	/*
	 * If that fails, try to find separate debug info via build ID
	 * and debug link.
	 */
	ret = bin_info_set_dwarf_info_build_id(bin);
	if (!ret) {
		goto end;
	}

	ret = bin_info_set_dwarf_info_debug_link(bin);
	if (!ret) {
		goto end;
	}

end:
	return ret;
}

BT_HIDDEN
void source_location_destroy(struct source_location *src_loc)
{
	if (!src_loc) {
		return;
	}

	free(src_loc->filename);
	g_free(src_loc);
}

/**
 * Append a string representation of an address offset to an existing
 * string.
 *
 * On success, the out parameter `result` will contain the base string
 * followed by the offset string of the form "+0x1234". On failure,
 * `result` remains unchanged.
 *
 * @param base_str	The string to which to append an offset string
 * @param low_addr	The lower virtual memory address, the base from
 *			which the offset is computed
 * @param high_addr	The higher virtual memory address
 * @param result	Out parameter, the base string followed by the
 * 			offset string
 * @returns		0 on success, -1 on failure
 */
static
int bin_info_append_offset_str(const char *base_str, uint64_t low_addr,
				uint64_t high_addr, char **result)
{
	uint64_t offset;
	char *_result = NULL;


	if (!base_str || !result) {
		goto error;
	}

	offset = high_addr - low_addr;

	_result = g_strdup_printf("%s+%#0" PRIx64, base_str, offset);
	if (!_result) {
		goto error;
	}
	*result = _result;

	return 0;

error:
	free(_result);
	return -1;
}

/**
 * Try to find the symbol closest to an address within a given ELF
 * section.
 *
 * Only function symbols are taken into account. The symbol's address
 * must precede `addr`. A symbol with a closer address might exist
 * after `addr` but is irrelevant because it cannot encompass `addr`.
 *
 * On success, if found, the out parameters `sym` and `shdr` are
 * set. On failure or if none are found, they remain unchanged.
 *
 * @param scn		ELF section in which to look for the address
 * @param addr		Virtual memory address for which to find the
 *			nearest function symbol
 * @param sym		Out parameter, the nearest function symbol
 * @param shdr		Out parameter, the section header for scn
 * @returns		0 on success, -1 on failure
 */
static
int bin_info_get_nearest_symbol_from_section(Elf_Scn *scn, uint64_t addr,
		GElf_Sym **sym, GElf_Shdr **shdr)
{
	int i;
	size_t symbol_count;
	Elf_Data *data = NULL;
	GElf_Shdr *_shdr = NULL;
	GElf_Sym *nearest_sym = NULL;

	if (!scn || !sym || !shdr) {
		goto error;
	}

	_shdr = g_new0(GElf_Shdr, 1);
	if (!_shdr) {
		goto error;
	}

	_shdr = gelf_getshdr(scn, _shdr);
	if (!_shdr) {
		goto error;
	}

	if (_shdr->sh_type != SHT_SYMTAB) {
		/*
		 * We are only interested in symbol table (symtab)
		 * sections, skip this one.
		 */
		goto end;
	}

	data = elf_getdata(scn, NULL);
	if (!data) {
		goto error;
	}

	symbol_count = _shdr->sh_size / _shdr->sh_entsize;

	for (i = 0; i < symbol_count; ++i) {
		GElf_Sym *cur_sym = NULL;

		cur_sym = g_new0(GElf_Sym, 1);
		if (!cur_sym) {
			goto error;
		}
		cur_sym = gelf_getsym(data, i, cur_sym);
		if (!cur_sym) {
			goto error;
		}
		if (GELF_ST_TYPE(cur_sym->st_info) != STT_FUNC) {
			/* We're only interested in the functions. */
			g_free(cur_sym);
			continue;
		}

		if (cur_sym->st_value <= addr &&
				(!nearest_sym ||
				cur_sym->st_value > nearest_sym->st_value)) {
			g_free(nearest_sym);
			nearest_sym = cur_sym;
		} else {
			g_free(cur_sym);
		}
	}

end:
	if (nearest_sym) {
		*sym = nearest_sym;
		*shdr = _shdr;
	} else {
		g_free(_shdr);
	}

	return 0;

error:
	g_free(nearest_sym);
	g_free(_shdr);
	return -1;
}

/**
 * Get the name of the function containing a given address within an
 * executable using ELF symbols.
 *
 * The function name is in fact the name of the nearest ELF symbol,
 * followed by the offset in bytes between the address and the symbol
 * (in hex), separated by a '+' character.
 *
 * If found, the out parameter `func_name` is set on success. On failure,
 * it remains unchanged.
 *
 * @param bin		bin_info instance for the executable containing
 *			the address
 * @param addr		Virtual memory address for which to find the
 *			function name
 * @param func_name	Out parameter, the function name
 * @returns		0 on success, -1 on failure
 */
static
int bin_info_lookup_elf_function_name(struct bin_info *bin, uint64_t addr,
		char **func_name)
{
	/*
	 * TODO (possible optimisation): if an ELF has no symtab
	 * section, it has been stripped. Therefore, it would be wise
	 * to store a flag indicating the stripped status after the
	 * first iteration to prevent subsequent ones.
	 */
	int ret = 0;
	Elf_Scn *scn = NULL;
	GElf_Sym *sym = NULL;
	GElf_Shdr *shdr = NULL;
	char *sym_name = NULL;

	/* Set ELF file if it hasn't been accessed yet. */
	if (!bin->elf_file) {
		ret = bin_info_set_elf_file(bin);
		if (ret) {
			/* Failed to set ELF file. */
			goto error;
		}
	}

	scn = elf_nextscn(bin->elf_file, scn);
	if (!scn) {
		goto error;
	}

	while (scn && !sym) {
		ret = bin_info_get_nearest_symbol_from_section(
				scn, addr, &sym, &shdr);
		if (ret) {
			goto error;
		}

		scn = elf_nextscn(bin->elf_file, scn);
	}

	if (sym) {
		sym_name = elf_strptr(bin->elf_file, shdr->sh_link,
				sym->st_name);
		if (!sym_name) {
			goto error;
		}

		ret = bin_info_append_offset_str(sym_name, sym->st_value, addr,
						func_name);
		if (ret) {
			goto error;
		}
	}

	g_free(shdr);
	g_free(sym);
	return 0;

error:
	g_free(shdr);
	g_free(sym);
	return ret;
}

/**
 * Get the name of the function containing a given address within a
 * given compile unit (CU).
 *
 * If found, the out parameter `func_name` is set on success. On
 * failure, it remains unchanged.
 *
 * @param cu		bt_dwarf_cu instance which may contain the address
 * @param addr		Virtual memory address for which to find the
 *			function name
 * @param func_name	Out parameter, the function name
 * @returns		0 on success, -1 on failure
 */
static
int bin_info_lookup_cu_function_name(struct bt_dwarf_cu *cu, uint64_t addr,
		char **func_name)
{
	int ret = 0;
	bool found = false;
	struct bt_dwarf_die *die = NULL;

	if (!cu || !func_name) {
		goto error;
	}

	die = bt_dwarf_die_create(cu);
	if (!die) {
		goto error;
	}

	while (bt_dwarf_die_next(die) == 0) {
		int tag;

		ret = bt_dwarf_die_get_tag(die, &tag);
		if (ret) {
			goto error;
		}

		if (tag == DW_TAG_subprogram) {
			ret = bt_dwarf_die_contains_addr(die, addr, &found);
			if (ret) {
				goto error;
			}

			if (found) {
				break;
			}
		}
	}

	if (found) {
		uint64_t low_addr = 0;
		char *die_name = NULL;

		ret = bt_dwarf_die_get_name(die, &die_name);
		if (ret) {
			goto error;
		}

		ret = dwarf_lowpc(die->dwarf_die, &low_addr);
		if (ret) {
			free(die_name);
			goto error;
		}

		ret = bin_info_append_offset_str(die_name, low_addr, addr,
						func_name);
		free(die_name);
		if (ret) {
			goto error;
		}
	}

	bt_dwarf_die_destroy(die);
	return 0;

error:
	bt_dwarf_die_destroy(die);
	return -1;
}

/**
 * Get the name of the function containing a given address within an
 * executable using DWARF debug info.
 *
 * If found, the out parameter `func_name` is set on success. On
 * failure, it remains unchanged.
 *
 * @param bin		bin_info instance for the executable containing
 *			the address
 * @param addr		Virtual memory address for which to find the
 *			function name
 * @param func_name	Out parameter, the function name
 * @returns		0 on success, -1 on failure
 */
static
int bin_info_lookup_dwarf_function_name(struct bin_info *bin, uint64_t addr,
		char **func_name)
{
	int ret = 0;
	char *_func_name = NULL;
	struct bt_dwarf_cu *cu = NULL;

	if (!bin || !func_name) {
		goto error;
	}

	cu = bt_dwarf_cu_create(bin->dwarf_info);
	if (!cu) {
		goto error;
	}

	while (bt_dwarf_cu_next(cu) == 0) {
		ret = bin_info_lookup_cu_function_name(cu, addr, &_func_name);
		if (ret) {
			goto error;
		}

		if (_func_name) {
			break;
		}
	}

	if (_func_name) {
		*func_name = _func_name;
	} else {
		goto error;
	}

	bt_dwarf_cu_destroy(cu);
	return 0;

error:
	bt_dwarf_cu_destroy(cu);
	return -1;
}

BT_HIDDEN
int bin_info_lookup_function_name(struct bin_info *bin,
		uint64_t addr, char **func_name)
{
	int ret = 0;
	char *_func_name = NULL;

	if (!bin || !func_name) {
		goto error;
	}

	/*
	 * If the bin_info has a build id but it does not match the build id
	 * that was found on the file system, return an error.
	 */
	if (bin->build_id && !bin->file_build_id_matches) {
		goto error;
	}

	/* Set DWARF info if it hasn't been accessed yet. */
	if (!bin->dwarf_info && !bin->is_elf_only) {
		ret = bin_info_set_dwarf_info(bin);
		if (ret) {
			BT_LOGD_STR("Failed to set bin dwarf info, falling back to ELF lookup.");
			/* Failed to set DWARF info, fallback to ELF. */
			bin->is_elf_only = true;
		}
	}

	if (!bin_info_has_address(bin, addr)) {
		goto error;
	}

	/*
	 * Addresses in ELF and DWARF are relative to base address for
	 * PIC, so make the address argument relative too if needed.
	 */
	if (bin->is_pic) {
		addr -= bin->low_addr;
	}

	if (bin->is_elf_only) {
		ret = bin_info_lookup_elf_function_name(bin, addr,
				&_func_name);
		if (ret) {
			BT_LOGD("Failed to lookup function name (ELF): "
				"ret=%d", ret);
		}
	} else {
		ret = bin_info_lookup_dwarf_function_name(bin, addr,
				&_func_name);
		if (ret) {
			BT_LOGD("Failed to lookup function name (DWARF): "
				"ret=%d", ret);
		}
	}

	*func_name = _func_name;
	return 0;

error:
	return -1;
}

BT_HIDDEN
int bin_info_get_bin_loc(struct bin_info *bin, uint64_t addr, char **bin_loc)
{
	gchar *_bin_loc = NULL;

	if (!bin || !bin_loc) {
		goto error;
	}

	/*
	 * If the bin_info has a build id but it does not match the build id
	 * that was found on the file system, return an error.
	 */
	if (bin->build_id && !bin->file_build_id_matches) {
		goto error;
	}

	if (bin->is_pic) {
		addr -= bin->low_addr;
		_bin_loc = g_strdup_printf("+%#0" PRIx64, addr);
	} else {
		_bin_loc = g_strdup_printf("@%#0" PRIx64, addr);
	}

	if (!_bin_loc) {
		goto error;
	}

	*bin_loc = _bin_loc;
	return 0;

error:
	return -1;
}

/**
 * Predicate used to determine whether the children of a given DIE
 * contain a specific address.
 *
 * More specifically, the parameter `die` is expected to be a
 * subprogram (function) DIE, and this predicate tells whether any
 * subroutines are inlined within this function and would contain
 * `addr`.
 *
 * On success, the out parameter `contains` is set with the boolean
 * value indicating whether the DIE's range covers `addr`. On failure,
 * it remains unchanged.
 *
 * Do note that this function advances the position of `die`. If the
 * address is found within one of its children, `die` will be pointing
 * to that child upon returning from the function, allowing to extract
 * the information deemed necessary.
 *
 * @param die		The parent DIE in whose children the address will be
 *			looked for
 * @param addr		The address for which to look for in the DIEs
 * @param contains	Out parameter, true if addr is contained,
 *			false if not
 * @returns		Returns 0 on success, -1 on failure
 */
static
int bin_info_child_die_has_address(struct bt_dwarf_die *die, uint64_t addr, bool *contains)
{
	int ret = 0;
	bool _contains = false;

	if (!die) {
		goto error;
	}

	ret = bt_dwarf_die_child(die);
	if (ret) {
		goto error;
	}

	do {
		ret = bt_dwarf_die_contains_addr(die, addr, &_contains);
		if (ret) {
			goto error;
		}

		if (_contains) {
			/*
			 * The address is within the range of the current DIE
			 * or its children.
			 */
			int tag;

			ret = bt_dwarf_die_get_tag(die, &tag);
			if (ret) {
				goto error;
			}

			if (tag == DW_TAG_inlined_subroutine) {
				/* Found the tracepoint. */
				goto end;
			}

			if (bt_dwarf_die_has_children(die)) {
				/*
				 * Look for the address in the children DIEs.
				 */
				ret = bt_dwarf_die_child(die);
				if (ret) {
					goto error;
				}
			}
		}
	} while (bt_dwarf_die_next(die) == 0);

end:
	*contains = _contains;
	return 0;

error:
	return -1;
}

/**
 * Lookup the source location for a given address within a CU, making
 * the assumption that it is contained within an inline routine in a
 * function.
 *
 * @param cu		bt_dwarf_cu instance in which to look for the address
 * @param addr		The address for which to look for
 * @param src_loc	Out parameter, the source location (filename and
 *			line number) for the address
 * @returns		0 on success, -1 on failure
 */
static
int bin_info_lookup_cu_src_loc_inl(struct bt_dwarf_cu *cu, uint64_t addr,
		struct source_location **src_loc)
{
	int ret = 0;
	bool found = false;
	struct bt_dwarf_die *die = NULL;
	struct source_location *_src_loc = NULL;

	if (!cu || !src_loc) {
		goto error;
	}

	die = bt_dwarf_die_create(cu);
	if (!die) {
		goto error;
	}

	while (bt_dwarf_die_next(die) == 0) {
		int tag;

		ret = bt_dwarf_die_get_tag(die, &tag);
		if (ret) {
			goto error;
		}

		if (tag == DW_TAG_subprogram) {
			bool contains = false;

			ret = bt_dwarf_die_contains_addr(die, addr, &contains);
			if (ret) {
				goto error;
			}

			if (contains) {
				/*
				 * Try to find an inlined subroutine
				 * child of this DIE containing addr.
				 */
				ret = bin_info_child_die_has_address(die, addr,
						&found);
				if(ret) {
					goto error;
				}

				goto end;
			}
		}
	}

end:
	if (found) {
		char *filename = NULL;
		uint64_t line_no;

		_src_loc = g_new0(struct source_location, 1);
		if (!_src_loc) {
			goto error;
		}

		ret = bt_dwarf_die_get_call_file(die, &filename);
		if (ret) {
			goto error;
		}
		ret = bt_dwarf_die_get_call_line(die, &line_no);
		if (ret) {
			free(filename);
			goto error;
		}

		_src_loc->filename = filename;
		_src_loc->line_no = line_no;
		*src_loc = _src_loc;
	}

	bt_dwarf_die_destroy(die);
	return 0;

error:
	source_location_destroy(_src_loc);
	bt_dwarf_die_destroy(die);
	return -1;
}

/**
 * Lookup the source location for a given address within a CU,
 * assuming that it is contained within an inlined function.
 *
 * A source location can be found regardless of inlining status for
 * this method, but in the case of an inlined function, the returned
 * source location will point not to the callsite but rather to the
 * definition site of the inline function.
 *
 * @param cu		bt_dwarf_cu instance in which to look for the address
 * @param addr		The address for which to look for
 * @param src_loc	Out parameter, the source location (filename and
 *			line number) for the address
 * @returns		0 on success, -1 on failure
 */
static
int bin_info_lookup_cu_src_loc_no_inl(struct bt_dwarf_cu *cu, uint64_t addr,
		struct source_location **src_loc)
{
	struct source_location *_src_loc = NULL;
	struct bt_dwarf_die *die = NULL;
	const char *filename = NULL;
	Dwarf_Line *line = NULL;
	Dwarf_Addr line_addr;
	int ret, line_no;

	if (!cu || !src_loc) {
		goto error;
	}

	die = bt_dwarf_die_create(cu);
	if (!die) {
		goto error;
	}

	line = dwarf_getsrc_die(die->dwarf_die, addr);
	if (!line) {
		goto error;
	}

	ret = dwarf_lineaddr(line, &line_addr);
	if (ret) {
		goto error;
	}

	filename = dwarf_linesrc(line, NULL, NULL);
	if (!filename) {
		goto error;
	}

	if (addr == line_addr) {
		_src_loc = g_new0(struct source_location, 1);
		if (!_src_loc) {
			goto error;
		}

		ret = dwarf_lineno(line, &line_no);
		if (ret) {
			goto error;
		}

		_src_loc->line_no = line_no;
		_src_loc->filename = g_strdup(filename);
	}

	bt_dwarf_die_destroy(die);

	if (_src_loc) {
		*src_loc = _src_loc;
	}

	return 0;

error:
	source_location_destroy(_src_loc);
	bt_dwarf_die_destroy(die);
	return -1;
}

/**
 * Get the source location (file name and line number) for a given
 * address within a compile unit (CU).
 *
 * On success, the out parameter `src_loc` is set if found. On
 * failure, it remains unchanged.
 *
 * @param cu		bt_dwarf_cu instance for the compile unit which
 *			may contain the address
 * @param addr		Virtual memory address for which to find the
 *			source location
 * @param src_loc	Out parameter, the source location
 * @returns		0 on success, -1 on failure
 */
static
int bin_info_lookup_cu_src_loc(struct bt_dwarf_cu *cu, uint64_t addr,
		struct source_location **src_loc)
{
	int ret = 0;
	struct source_location *_src_loc = NULL;

	if (!cu || !src_loc) {
		goto error;
	}

	ret = bin_info_lookup_cu_src_loc_inl(cu, addr, &_src_loc);
	if (ret) {
		goto error;
	}

	if (_src_loc) {
		goto end;
	}

	ret = bin_info_lookup_cu_src_loc_no_inl(cu, addr, &_src_loc);
	if (ret) {
		goto error;
	}

	if (_src_loc) {
		goto end;
	}

end:
	if (_src_loc) {
		*src_loc = _src_loc;
	}

	return 0;

error:
	source_location_destroy(_src_loc);
	return -1;
}

BT_HIDDEN
int bin_info_lookup_source_location(struct bin_info *bin, uint64_t addr,
		struct source_location **src_loc)
{
	struct bt_dwarf_cu *cu = NULL;
	struct source_location *_src_loc = NULL;

	if (!bin || !src_loc) {
		goto error;
	}

	/*
	 * If the bin_info has a build id but it does not match the build id
	 * that was found on the file system, return an error.
	 */
	if (bin->build_id && !bin->file_build_id_matches) {
		goto error;
	}

	/* Set DWARF info if it hasn't been accessed yet. */
	if (!bin->dwarf_info && !bin->is_elf_only) {
		if (bin_info_set_dwarf_info(bin)) {
			/* Failed to set DWARF info. */
			bin->is_elf_only = true;
		}
	}

	if (bin->is_elf_only) {
		/* We cannot lookup source location without DWARF info. */
		goto error;
	}

	if (!bin_info_has_address(bin, addr)) {
		goto error;
	}

	/*
	 * Addresses in ELF and DWARF are relative to base address for
	 * PIC, so make the address argument relative too if needed.
	 */
	if (bin->is_pic) {
		addr -= bin->low_addr;
	}

	cu = bt_dwarf_cu_create(bin->dwarf_info);
	if (!cu) {
		goto error;
	}

	while (bt_dwarf_cu_next(cu) == 0) {
		int ret;

		ret = bin_info_lookup_cu_src_loc(cu, addr, &_src_loc);
		if (ret) {
			goto error;
		}

		if (_src_loc) {
			break;
		}
	}

	bt_dwarf_cu_destroy(cu);
	if (_src_loc) {
		*src_loc = _src_loc;
	}

	return 0;

error:
	source_location_destroy(_src_loc);
	bt_dwarf_cu_destroy(cu);
	return -1;
}
