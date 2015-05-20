#include <babeltrace/plugin/plugin-lib.h>
#include <babeltrace/plugin/plugin-system.h>
#include <glib.h>
#include <stdio.h>

const char *plugin_name = "ctf";

struct ctf_reader {
        FILE *err;
};

enum bt_plugin_type bt_plugin_lib_get_type(void)
{
	return BT_PLUGIN_TYPE_SOURCE;
}

const char *bt_plugin_lib_get_format_name(void)
{
	return plugin_name;
}

static
void ctf_reader_destroy(struct bt_plugin *plugin)
{
	struct ctf_reader *reader;

	if (!plugin) {
		return;
	}

	reader = bt_plugin_get_private_data(plugin);
	if (!reader) {
		return;
	}

	g_free(reader);
}

static
struct bt_notification_iterator *ctf_reader_iterator_create(
		struct bt_plugin *plugin)
{
	return NULL;
}

/* Move this to bt_plugin */
static
enum bt_plugin_status ctf_reader_set_error_stream(
		struct bt_plugin *plugin, FILE *stream)
{
	struct ctf_reader *reader;
	enum bt_plugin_status ret = BT_PLUGIN_STATUS_OK;

	if (!plugin) {
		ret = BT_PLUGIN_STATUS_INVAL;
		goto end;
	}

	reader = bt_plugin_get_private_data(plugin);
	if (!reader) {
		ret = BT_PLUGIN_STATUS_ERROR;
		goto end;
	}

	reader->stream = stream;
end:
	return ret;
}

struct bt_plugin *bt_plugin_lib_create(struct bt_object *params)
{
	enum bt_plugin_status ret;
	struct bt_plugin *plugin = NULL;
	struct ctf_reader *reader = g_new0(struct ctf_reader, 1);

	if (!reader) {
		goto error;
	}

	plugin = bt_plugin_source_create(plugin_name, reader,
		ctf_reader_destroy, ctf_reader_iterator_create);
	if (!plugin) {
		goto error;
	}
	reader = NULL;

	ret = bt_plugin_set_error_stream_cb(plugin,
		ctf_reader_set_error_stream);
	if (ret != BT_PLUGIN_STATUS_OK) {
		goto error;
	}
end:
	return plugin;
error:
	if (reader) {
		ctf_reader_destroy(reader);
	}
	if (plugin) {
		bt_plugin_put(plugin);
		plugin = NULL;
	}
	goto end;
}
