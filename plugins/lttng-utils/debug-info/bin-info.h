#ifndef _BABELTRACE_BIN_INFO_H
#define _BABELTRACE_BIN_INFO_H

/*
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

#include <stdint.h>
#include <stdbool.h>
#include <gelf.h>
#include <elfutils/libdw.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/fd-cache-internal.h>

#define DEFAULT_DEBUG_DIR "/usr/lib/debug"
#define DEBUG_SUBDIR ".debug/"
#define BUILD_ID_SUBDIR ".build-id/"
#define BUILD_ID_SUFFIX ".debug"

struct bin_info {
	/* Base virtual memory address. */
	uint64_t low_addr;
	/* Upper bound of exec address space. */
	uint64_t high_addr;
	/* Size of exec address space. */
	uint64_t memsz;
	/* Paths to ELF and DWARF files. */
	gchar *elf_path;
	gchar *dwarf_path;
	/* libelf and libdw objects representing the files. */
	Elf *elf_file;
	Dwarf *dwarf_info;
	/* Optional build ID info. */
	uint8_t *build_id;
	size_t build_id_len;

	/* Optional debug link info. */
	gchar *dbg_link_filename;
	uint32_t dbg_link_crc;
	/* fd cache handles to ELF and DWARF files. */
	struct bt_fd_cache_handle *elf_handle;
	struct bt_fd_cache_handle *dwarf_handle;
	/* Configuration. */
	gchar *debug_info_dir;
	/* Denotes whether the executable is position independent code. */
	bool is_pic:1;
	/* Denotes whether the build id in the trace matches to one on disk. */
	bool file_build_id_matches:1;
	/*
	 * Denotes whether the executable only has ELF symbols and no
	 * DWARF info.
	 */
	bool is_elf_only:1;
	/* Weak ref. Owned by the iterator. */
	struct bt_fd_cache *fd_cache;
};

struct source_location {
	uint64_t line_no;
	gchar *filename;
};

/**
 * Initializes the bin_info framework. Call this before calling
 * anything else.
 *
 * @returns		0 on success, -1 on failure
 */
BT_HIDDEN
int bin_info_init(void);

/**
 * Instantiate a structure representing an ELF executable, possibly
 * with DWARF info, located at the given path.
 *
 * @param path		Path to the ELF file
 * @param low_addr	Base address of the executable
 * @param memsz	In-memory size of the executable
 * @param is_pic	Whether the executable is position independent
 *			code (PIC)
 * @param debug_info_dir Directory containing debug info or NULL.
 * @param target_prefix  Path to the root file system of the target
 *                       or NULL.
 * @returns		Pointer to the new bin_info on success,
 *			NULL on failure.
 */
BT_HIDDEN
struct bin_info *bin_info_create(struct bt_fd_cache *fdc, const char *path,
		uint64_t low_addr, uint64_t memsz, bool is_pic,
		const char *debug_info_dir, const char *target_prefix);

/**
 * Destroy the given bin_info instance
 *
 * @param bin	bin_info instance to destroy
 */
BT_HIDDEN
void bin_info_destroy(struct bin_info *bin);

/**
 * Sets the build ID information for a given bin_info instance.
 *
 * @param bin		The bin_info instance for which to set
 *			the build ID
 * @param build_id	Array of bytes containing the actual ID
 * @param build_id_len	Length in bytes of the build_id
 * @returns		0 on success, -1 on failure
 */
BT_HIDDEN
int bin_info_set_build_id(struct bin_info *bin, uint8_t *build_id,
		size_t build_id_len);

/**
 * Sets the debug link information for a given bin_info instance.
 *
 * @param bin		The bin_info instance for which to set
 *			the debug link
 * @param filename	Name of the separate debug info file
 * @param crc		Checksum for the debug info file
 * @returns		0 on success, -1 on failure
 */
BT_HIDDEN
int bin_info_set_debug_link(struct bin_info *bin, const char *filename,
		uint32_t crc);

/**
 * Returns whether or not the given bin info \p bin contains the
 * address \p addr.
 *
 * @param bin		bin_info instance
 * @param addr		Address to lookup
 * @returns		1 if \p bin contains \p addr, 0 if it does not,
 *			-1 on failure
 */
static inline
int bin_info_has_address(struct bin_info *bin, uint64_t addr)
{
	if (!bin) {
		return -1;
	}

	return addr >= bin->low_addr && addr < bin->high_addr;
}

/**
 * Get the name of the function containing a given address within an
 * executable.
 *
 * If no DWARF info is available, the function falls back to ELF
 * symbols and the "function name" is in fact the name of the closest
 * symbol, followed by the offset between the symbol and the address.
 *
 * On success, if found, the out parameter `func_name` is set. The ownership
 * of `func_name` is passed to the caller. On failure, `func_name` remains
 * unchanged.
 *
 * @param bin		bin_info instance for the executable containing
 *			the address
 * @param addr		Virtual memory address for which to find the
 *			function name
 * @param func_name	Out parameter, the function name.
 * @returns		0 on success, -1 on failure
 */
BT_HIDDEN
int bin_info_lookup_function_name(struct bin_info *bin, uint64_t addr,
		char **func_name);

/**
 * Get the source location (file name and line number) for a given
 * address within an executable.
 *
 * If no DWARF info is available, the source location cannot be found
 * and the function will return unsuccessfully.
 *
 * On success, if found, the out parameter `src_loc` is set. The ownership
 * of `src_loc` is passed to the caller. On failure, `src_loc` remains
 * unchanged.
 *
 * @param bin		bin_info instance for the executable containing
 *			the address
 * @param addr		Virtual memory address for which to find the
 *			source location
 * @param src_loc	Out parameter, the source location
 * @returns		0 on success, -1 on failure
 */
BT_HIDDEN
int bin_info_lookup_source_location(struct bin_info *bin, uint64_t addr,
		struct source_location **src_loc);
/**
 * Get a string representing the location within the binary of a given
 * address.
 *
 * In the  case of a PIC binary, the location is relative (+0x1234).
 * For a non-PIC binary, the location is absolute (@0x1234)
 *
 * On success, the out parameter `bin_loc` is set. The ownership is
 * passed to the caller. On failure, `bin_loc` remains unchanged.
 *
 * @param bin		bin_info instance for the executable containing
 *			the address
 * @param addr		Virtual memory address for which to find the
 *			binary location
 * @param bin_loc	Out parameter, the binary location
 * @returns		0 on success, -1 on failure
 */
BT_HIDDEN
int bin_info_get_bin_loc(struct bin_info *bin, uint64_t addr, char **bin_loc);

/**
 * Destroy the given source_location instance
 *
 * @param src_loc	source_location instance to destroy
 */
BT_HIDDEN
void source_location_destroy(struct source_location *src_loc);

#endif	/* _BABELTRACE_BIN_INFO_H */
