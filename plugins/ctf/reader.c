enum bt_plugin_type bt_plugin_get_type(void)
{
	return BT_PLUGIN_TYPE_SOURCE;
}

const char *bt_plugin_get_format_name(void)
{
	return "ctf";
}

static struct bt_notification_iterator *create_iterator(struct bt_plugin *plugin)
{
	return NULL;
}

static void destroy_mein_shitzle(struct bt_plugin *plugin)
{
	free(bt_plugin_get_data(plugin));
}

struct bt_plugin *bt_plugin_create(struct bt_ctf_field *params)
{
	void *mein_shit = 0x4589;

	return bt_plugin_source_create("ctf", mein_shit, destroy_func,
		create_iterator);
}

