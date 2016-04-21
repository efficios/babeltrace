#ifndef _BABELTRACE_DWARF_H
#define _BABELTRACE_DWARF_H

/*
 * Babeltrace - DWARF Information Reader
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

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <dwarf.h>
#include <elfutils/libdw.h>
#include <babeltrace/babeltrace-internal.h>

/*
 * bt_dwarf is a wrapper over libdw providing a nicer, higher-level
 * interface, to access basic debug information.
 */

/*
 * This structure corresponds to a single compilation unit (CU) for a
 * given set of debug information (Dwarf type).
 */
struct bt_dwarf_cu {
	Dwarf *dwarf_info;
	/* Offset in bytes in the DWARF file to current CU header. */
	Dwarf_Off offset;
	/* Offset in bytes in the DWARF file to next CU header. */
	Dwarf_Off next_offset;
	/* Size in bytes of CU header */
	size_t header_size;
};

/*
 * This structure represents a single debug information entry (DIE),
 * within a compilation unit (CU).
 */
struct bt_dwarf_die {
	struct bt_dwarf_cu *cu;
	Dwarf_Die *dwarf_die;
	/*
	 * A depth of 0 represents a root DIE, located in the DWARF
	 * layout on the same level as its corresponding CU entry. Its
	 * children DIEs will have a depth of 1, and so forth.
	 */
	unsigned int depth;
};

/**
 * Instantiate a structure to access compile units (CU) from a given
 * `dwarf_info`.
 *
 * @param dwarf_info	Dwarf instance
 * @returns		Pointer to the new bt_dwarf_cu on success,
 *			NULL on failure.
 */
BT_HIDDEN
struct bt_dwarf_cu *bt_dwarf_cu_create(Dwarf *dwarf_info);

/**
 * Destroy the given bt_dwarf_cu instance.
 *
 * @param cu	bt_dwarf_cu instance
 */
BT_HIDDEN
void bt_dwarf_cu_destroy(struct bt_dwarf_cu *cu);

/**
 * Advance the compile unit `cu` to the next one.
 *
 * On success, `cu`'s offset is set to that of the current compile
 * unit in the executable. On failure, `cu` remains unchanged.
 *
 * @param cu	bt_dwarf_cu instance
 * @returns	0 on success, 1 if no next CU is available,
 *		-1 on failure
 */
BT_HIDDEN
int bt_dwarf_cu_next(struct bt_dwarf_cu *cu);

/**
 * Instantiate a structure to access debug information entries (DIE)
 * for the given compile unit `cu`.
 *
 * @param cu	bt_dwarf_cu instance
 * @returns	Pointer to the new bt_dwarf_die on success,
 *		NULL on failure.
 */
BT_HIDDEN
struct bt_dwarf_die *bt_dwarf_die_create(struct bt_dwarf_cu *cu);

/**
 * Destroy the given bt_dwarf_die instance.
 *
 * @param die	bt_dwarf_die instance
 */
BT_HIDDEN
void bt_dwarf_die_destroy(struct bt_dwarf_die *die);

/**
 * Advance the debug information entry `die` to its first child, if
 * any.
 *
 * @param die	bt_dwarf_die instance
 * @returns	0 on success, 1 if no child DIE is available,
 * 		-1 on failure
 */
BT_HIDDEN
int bt_dwarf_die_child(struct bt_dwarf_die *die);

/**
 * Advance the debug information entry `die` to the next one.
 *
 * The next DIE is considered to be its sibling on the same level. The
 * only exception is when the depth of the given DIE is 0, i.e. a
 * newly created bt_dwarf_die, in which case next returns the first
 * DIE at depth 1.
 *
 * The reason for staying at a depth of 1 is that this is where all
 * the function DIEs (those with a tag value of DW_TAG_subprogram) are
 * located, from which more specific child DIEs can then be accessed
 * if needed via bt_dwarf_die_child.
 *
 * @param die	bt_dwarf_die instance
 * @returns	0 on success, 1 if no other siblings are available, -1 on
 *		failure
 */
BT_HIDDEN
int bt_dwarf_die_next(struct bt_dwarf_die *die);

/**
 * Get a DIE's tag.
 *
 * On success, the `tag` out parameter is set to the `die`'s tag's
 * value. It remains unchanged on failure.
 *
 * @param die	bt_dwarf_die instance
 * @param tag	Out parameter, the DIE's tag value
 * @returns	0 on success, -1 on failure.
 */
BT_HIDDEN
int bt_dwarf_die_get_tag(struct bt_dwarf_die *die, int *tag);

/**
 * Get a DIE's name.
 *
 * On success, the `name` out parameter is set to the DIE's name. It
 * remains unchanged on failure.
 *
 * @param die	bt_dwarf_die instance
 * @param name	Out parameter, the DIE's name
 * @returns	0 on success, -1 on failure
 */
BT_HIDDEN
int bt_dwarf_die_get_name(struct bt_dwarf_die *die, char **name);

/**
 * Get the full path to the DIE's callsite file.
 *
 * Only applies to DW_TAG_inlined_subroutine entries. The out
 * parameter `filename` is set on success, unchanged on failure.
 *
 * @param die		bt_dwarf_die instance
 * @param filename	Out parameter, the filename for the subroutine's
 *			callsite
 * @returns		0 on success, -1 on failure
 */
BT_HIDDEN
int bt_dwarf_die_get_call_file(struct bt_dwarf_die *die, char **filename);

/**
 * Get line number for the DIE's callsite.
 *
 * Only applies to DW_TAG_inlined_subroutine entries. The out
 * parameter `line_no` is set on success, unchanged on failure.
 *
 * @param die		bt_dwarf_die instance
 * @param line_no	Out parameter, the line number for the
 *			subroutine's callsite
 * @returns		0 on success, -1 on failure
 */
BT_HIDDEN
int bt_dwarf_die_get_call_line(struct bt_dwarf_die *die,
		uint64_t *line_no);

/**
 * Verifies whether a given DIE contains the virtual memory address
 * `addr`.
 *
 * On success, the out parameter `contains` is set with the boolean
 * value indicating whether the DIE's range covers `addr`. On failure,
 * it remains unchanged.
 *
 * @param die		bt_dwarf_die instance
 * @param addr		The memory address to verify
 * @param contains	Out parameter, true if addr is contained,
 *			false if not
 * @returns		0 on succes, -1 on failure
 */
BT_HIDDEN
int bt_dwarf_die_contains_addr(struct bt_dwarf_die *die, uint64_t addr,
		bool *contains);

#endif	/* _BABELTRACE_DWARF_H */
