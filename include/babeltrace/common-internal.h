#ifndef BABELTRACE_COMMON_INTERNAL_H
#define BABELTRACE_COMMON_INTERNAL_H

#include <babeltrace/babeltrace-internal.h>

BT_HIDDEN
bool bt_common_is_setuid_setgid(void);

BT_HIDDEN
const char *bt_common_get_system_plugin_path(void);

BT_HIDDEN
char *bt_common_get_home_plugin_path(void);

BT_HIDDEN
int bt_common_append_plugin_path_dirs(const char *paths, GPtrArray *dirs);

#endif /* BABELTRACE_COMMON_INTERNAL_H */
