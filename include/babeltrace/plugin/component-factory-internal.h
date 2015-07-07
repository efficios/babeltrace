
/** Component initialization functions */
/**
 * Allocate a source component.
 *
 * @param name			Component instance name (will be copied)
 * @param private_data		Private component implementation data
 * @param destroy_cb		Component private data clean-up callback
 * @param iterator_create_cb	Iterator creation callback
 * @returns			A source component instance
 */
extern struct bt_component *bt_component_source_create(const char *name,
		void *private_data, bt_component_destroy_cb destroy_func,
		bt_component_source_iterator_create_cb iterator_create_cb);

/**
 * Allocate a sink component.
 *
 * @param name			Component instance name (will be copied)
 * @param private_data		Private component implementation data
 * @param destroy_cb		Component private data clean-up callback
 * @param notification_cb	Notification handling callback
 * @returns			A sink component instance
 */
extern struct bt_component *bt_component_sink_create(const char *name,
		void *private_data, bt_component_destroy_cb destroy_func,
		bt_component_sink_handle_notification_cb notification_cb);
