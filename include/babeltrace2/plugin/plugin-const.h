#ifndef BABELTRACE2_PLUGIN_PLUGIN_CONST_H
#define BABELTRACE2_PLUGIN_PLUGIN_CONST_H

/*
 * Copyright (c) 2010-2019 EfficiOS Inc. and Linux Foundation
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

#ifndef __BT_IN_BABELTRACE_H
# error "Please include <babeltrace2/babeltrace.h> instead."
#endif

#include <stdint.h>
#include <stddef.h>

#include <babeltrace2/types.h>
#include <babeltrace2/property.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum bt_plugin_find_status {
	BT_PLUGIN_FIND_STATUS_OK		= __BT_FUNC_STATUS_OK,
	BT_PLUGIN_FIND_STATUS_NOT_FOUND		= __BT_FUNC_STATUS_NOT_FOUND,
	BT_PLUGIN_FIND_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
	BT_PLUGIN_FIND_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_plugin_find_status;

extern bt_plugin_find_status bt_plugin_find(const char *plugin_name,
		bt_bool find_in_std_env_var, bt_bool find_in_user_dir,
		bt_bool find_in_sys_dir, bt_bool find_in_static,
		bt_bool fail_on_load_error, const bt_plugin **plugin);

typedef enum bt_plugin_find_all_status {
	BT_PLUGIN_FIND_ALL_STATUS_OK		= __BT_FUNC_STATUS_OK,
	BT_PLUGIN_FIND_ALL_STATUS_NOT_FOUND	= __BT_FUNC_STATUS_NOT_FOUND,
	BT_PLUGIN_FIND_ALL_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
	BT_PLUGIN_FIND_ALL_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_plugin_find_all_status;

bt_plugin_find_all_status bt_plugin_find_all(bt_bool find_in_std_env_var,
		bt_bool find_in_user_dir, bt_bool find_in_sys_dir,
		bt_bool find_in_static, bt_bool fail_on_load_error,
		const bt_plugin_set **plugin_set);

typedef enum bt_plugin_find_all_from_file_status {
	BT_PLUGIN_FIND_ALL_FROM_FILE_STATUS_OK			= __BT_FUNC_STATUS_OK,
	BT_PLUGIN_FIND_ALL_FROM_FILE_STATUS_NOT_FOUND		= __BT_FUNC_STATUS_NOT_FOUND,
	BT_PLUGIN_FIND_ALL_FROM_FILE_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
	BT_PLUGIN_FIND_ALL_FROM_FILE_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_plugin_find_all_from_file_status;

extern bt_plugin_find_all_from_file_status bt_plugin_find_all_from_file(
		const char *path, bt_bool fail_on_load_error,
		const bt_plugin_set **plugin_set);

typedef enum bt_plugin_find_all_from_dir_status {
	BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_OK			= __BT_FUNC_STATUS_OK,
	BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_NOT_FOUND		= __BT_FUNC_STATUS_NOT_FOUND,
	BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
	BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_MEMORY_ERROR		= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_plugin_find_all_from_dir_status;

extern bt_plugin_find_all_from_dir_status bt_plugin_find_all_from_dir(
		const char *path, bt_bool recurse, bt_bool fail_on_load_error,
		const bt_plugin_set **plugin_set);

typedef enum bt_plugin_find_all_from_static_status {
	BT_PLUGIN_FIND_ALL_FROM_STATIC_STATUS_OK			= __BT_FUNC_STATUS_OK,
	BT_PLUGIN_FIND_ALL_FROM_STATIC_STATUS_NOT_FOUND			= __BT_FUNC_STATUS_NOT_FOUND,
	BT_PLUGIN_FIND_ALL_FROM_STATIC_STATUS_ERROR			= __BT_FUNC_STATUS_ERROR,
	BT_PLUGIN_FIND_ALL_FROM_STATIC_STATUS_MEMORY_ERROR		= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_plugin_find_all_from_static_status;

extern bt_plugin_find_all_from_static_status bt_plugin_find_all_from_static(
		bt_bool fail_on_load_error, const bt_plugin_set **plugin_set);

extern const char *bt_plugin_get_name(const bt_plugin *plugin);

extern const char *bt_plugin_get_author(const bt_plugin *plugin);

extern const char *bt_plugin_get_license(const bt_plugin *plugin);

extern const char *bt_plugin_get_description(const bt_plugin *plugin);

extern const char *bt_plugin_get_path(const bt_plugin *plugin);

extern bt_property_availability bt_plugin_get_version(
		const bt_plugin *plugin, unsigned int *major,
		unsigned int *minor, unsigned int *patch, const char **extra);

extern uint64_t bt_plugin_get_source_component_class_count(
		const bt_plugin *plugin);

extern uint64_t bt_plugin_get_filter_component_class_count(
		const bt_plugin *plugin);

extern uint64_t bt_plugin_get_sink_component_class_count(
		const bt_plugin *plugin);

extern const bt_component_class_source *
bt_plugin_borrow_source_component_class_by_index_const(
		const bt_plugin *plugin, uint64_t index);

extern const bt_component_class_filter *
bt_plugin_borrow_filter_component_class_by_index_const(
		const bt_plugin *plugin, uint64_t index);

extern const bt_component_class_sink *
bt_plugin_borrow_sink_component_class_by_index_const(
		const bt_plugin *plugin, uint64_t index);

extern const bt_component_class_source *
bt_plugin_borrow_source_component_class_by_name_const(
		const bt_plugin *plugin, const char *name);

extern const bt_component_class_filter *
bt_plugin_borrow_filter_component_class_by_name_const(
		const bt_plugin *plugin, const char *name);

extern const bt_component_class_sink *
bt_plugin_borrow_sink_component_class_by_name_const(
		const bt_plugin *plugin, const char *name);

extern void bt_plugin_get_ref(const bt_plugin *plugin);

extern void bt_plugin_put_ref(const bt_plugin *plugin);

#define BT_PLUGIN_PUT_REF_AND_RESET(_var)		\
	do {						\
		bt_plugin_put_ref(_var);		\
		(_var) = NULL;				\
	} while (0)

#define BT_PLUGIN_MOVE_REF(_var_dst, _var_src)		\
	do {						\
		bt_plugin_put_ref(_var_dst);		\
		(_var_dst) = (_var_src);		\
		(_var_src) = NULL;			\
	} while (0)

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_PLUGIN_PLUGIN_CONST_H */
