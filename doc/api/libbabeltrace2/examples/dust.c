#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <babeltrace2/babeltrace.h>

/* Source component's private data */
struct dust_in {
    /* Input file path parameter value (owned by this) */
    const bt_value *path_value;

    /* Stream (owned by this) */
    bt_stream *stream;

    /* Event classes for each type of event (owned by this) */
    bt_event_class *send_msg_event_class;
    bt_event_class *recv_msg_event_class;
};

/*
 * Creates an event class within `stream_class` named `name`.
 */
static
bt_event_class *create_event_class(bt_stream_class *stream_class,
        const char *name)
{
    /* Borrow trace class from stream class */
    bt_trace_class *trace_class =
        bt_stream_class_borrow_trace_class(stream_class);

    /* Create a default event class */
    bt_event_class *event_class = bt_event_class_create(stream_class);

    /* Name the event class */
    bt_event_class_set_name(event_class, name);

    /*
     * Create an empty structure field class to be used as the
     * event class's payload field class.
     */
    bt_field_class *payload_field_class =
        bt_field_class_structure_create(trace_class);

    /*
     * Create a string field class to be used as the payload field
     * class's `msg` member.
     */
    bt_field_class *msg_field_class =
        bt_field_class_string_create(trace_class);

    /*
     * Append the string field class to the structure field class as the
     * `msg` member.
     */
    bt_field_class_structure_append_member(payload_field_class, "msg",
        msg_field_class);

    /* Set the event class's payload field class */
    bt_event_class_set_payload_field_class(event_class, payload_field_class);

    /* Put the references we don't need anymore */
    bt_field_class_put_ref(payload_field_class);
    bt_field_class_put_ref(msg_field_class);

    return event_class;
}

/*
 * Creates the source component's metadata and stream objects.
 */
static
void create_metadata_and_stream(bt_self_component *self_component,
        struct dust_in *dust_in)
{
    /* Create a default trace class */
    bt_trace_class *trace_class = bt_trace_class_create(self_component);

    /* Create a stream trace class within `trace_class` */
    bt_stream_class *stream_class = bt_stream_class_create(trace_class);

    /* Create a default clock class (1 GHz frequency) */
    bt_clock_class *clock_class = bt_clock_class_create(self_component);

    /*
     * Set `clock_class` as the default clock class of `stream_class`.
     *
     * This means all the streams created from `stream_class` have a
     * conceptual default clock which is an instance of `clock_class`.
     * Any event message created for such a stream has a snapshot of the
     * stream's default clock.
     */
    bt_stream_class_set_default_clock_class(stream_class, clock_class);

    /* Create the two event classes we need */
    dust_in->send_msg_event_class = create_event_class(stream_class,
        "send-msg");
    dust_in->recv_msg_event_class = create_event_class(stream_class,
        "recv-msg");

    /* Create a default trace from (instance of `trace_class`) */
    bt_trace *trace = bt_trace_create(trace_class);

    /*
     * Create the source component's stream (instance of `stream_class`
     * within `trace`).
     */
    dust_in->stream = bt_stream_create(stream_class, trace);

    /* Put the references we don't need anymore */
    bt_trace_put_ref(trace);
    bt_clock_class_put_ref(clock_class);
    bt_stream_class_put_ref(stream_class);
    bt_trace_class_put_ref(trace_class);
}

/*
 * Initializes the source component.
 */
static
bt_component_class_initialize_method_status dust_in_initialize(
        bt_self_component_source *self_component_source,
        bt_self_component_source_configuration *configuration,
        const bt_value *params, void *initialize_method_data)
{
    /* Allocate a private data structure */
    struct dust_in *dust_in = malloc(sizeof(*dust_in));

    /*
     * Keep a reference of the `path` string value parameter so that the
     * initialization method of a message iterator can read its string
     * value to open the file.
     */
    dust_in->path_value =
        bt_value_map_borrow_entry_value_const(params, "path");
    bt_value_get_ref(dust_in->path_value);

    /* Upcast `self_component_source` to the `bt_self_component` type */
    bt_self_component *self_component =
        bt_self_component_source_as_self_component(self_component_source);

    /* Create the source component's metadata and stream objects */
    create_metadata_and_stream(self_component, dust_in);

    /* Set the component's user data to our private data structure */
    bt_self_component_set_data(self_component, dust_in);

    /*
     * Add an output port named `out` to the source component.
     *
     * This is needed so that this source component can be connected to
     * a filter or a sink component. Once a downstream component is
     * connected, it can create our message iterator.
     */
    bt_self_component_source_add_output_port(self_component_source,
        "out", NULL, NULL);

    return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK;
}

/*
 * Finalizes the source component.
 */
static
void dust_in_finalize(bt_self_component_source *self_component_source)
{
    /* Retrieve our private data from the component's user data */
    struct dust_in *dust_in = bt_self_component_get_data(
        bt_self_component_source_as_self_component(self_component_source));

    /* Put all references */
    bt_value_put_ref(dust_in->path_value);
    bt_event_class_put_ref(dust_in->send_msg_event_class);
    bt_event_class_put_ref(dust_in->recv_msg_event_class);
    bt_stream_put_ref(dust_in->stream);

    /* Free the allocated structure */
    free(dust_in);
}

/* State of a message iterator */
enum dust_in_message_iterator_state {
    /* Emit a stream beginning message */
    DUST_IN_MESSAGE_ITERATOR_STATE_STREAM_BEGINNING,

    /* Emit an event message */
    DUST_IN_MESSAGE_ITERATOR_STATE_EVENT,

    /* Message iterator is ended */
    DUST_IN_MESSAGE_ITERATOR_STATE_ENDED,
};

/* Message iterator's private data */
struct dust_in_message_iterator {
    /* (Weak) link to the component's private data */
    struct dust_in *dust_in;

    /* Current message iterator's state */
    enum dust_in_message_iterator_state state;

    /* Input file */
    FILE *file;

    /* Buffers to read data from the input file */
    char name_buffer[32];
    char msg_buffer[1024];
};

/*
 * Initializes the message iterator.
 */
static
bt_message_iterator_class_initialize_method_status
dust_in_message_iterator_initialize(
        bt_self_message_iterator *self_message_iterator,
        bt_self_message_iterator_configuration *configuration,
        bt_self_component_port_output *self_port)
{
    /* Allocate a private data structure */
    struct dust_in_message_iterator *dust_in_iter =
        malloc(sizeof(*dust_in_iter));

    /* Retrieve the component's private data from its user data */
    struct dust_in *dust_in = bt_self_component_get_data(
        bt_self_message_iterator_borrow_component(self_message_iterator));

    /* Keep a link to the component's private data */
    dust_in_iter->dust_in = dust_in;

    /* Set the message iterator's initial state */
    dust_in_iter->state = DUST_IN_MESSAGE_ITERATOR_STATE_STREAM_BEGINNING;

    /* Get the raw value of the input file path string value */
    const char *path = bt_value_string_get(dust_in->path_value);

    /* Open the input file in text mode */
    dust_in_iter->file = fopen(path, "r");

    /* Set the message iterator's user data to our private data structure */
    bt_self_message_iterator_set_data(self_message_iterator, dust_in_iter);

    return BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_OK;
}

/*
 * Finalizes the message iterator.
 */
static
void dust_in_message_iterator_finalize(
        bt_self_message_iterator *self_message_iterator)
{
    /* Retrieve our private data from the message iterator's user data */
    struct dust_in_message_iterator *dust_in_iter =
        bt_self_message_iterator_get_data(self_message_iterator);

    /* Close the input file */
    fclose(dust_in_iter->file);

    /* Free the allocated structure */
    free(dust_in_iter);
}

/*
 * Creates a message from the message iterator's input file's current
 * line.
 *
 * If there's a line to process, this function creates an event message.
 * Otherwise it creates a stream end message and sets the message
 * iterator's state accordingly.
 */
static
bt_message *create_message_from_line(
        struct dust_in_message_iterator *dust_in_iter,
        bt_self_message_iterator *self_message_iterator)
{
    uint64_t timestamp;
    uint64_t extra_us;
    bt_message *message;

    /* Try to read a line from the input file into individual tokens */
    int count = fscanf(dust_in_iter->file, "%" PRIu64 " %" PRIu64 " %s %[^\n]",
        &timestamp, &extra_us, &dust_in_iter->name_buffer[0],
        &dust_in_iter->msg_buffer[0]);

    /* Reached the end of the file? */
    if (count == EOF || feof(dust_in_iter->file)) {
        /*
         * Reached the end of the file: create a stream end message and
         * set the message iterator's state to
         * `DUST_IN_MESSAGE_ITERATOR_STATE_ENDED` so that the next call
         * to dust_in_message_iterator_next() returns
         * `BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_END` (end of
         * iteration).
         */
        message = bt_message_stream_end_create(self_message_iterator,
            dust_in_iter->dust_in->stream);
        dust_in_iter->state = DUST_IN_MESSAGE_ITERATOR_STATE_ENDED;
        goto end;
    }

    /* Choose the correct event class, depending on the event name token */
    bt_event_class *event_class;

    if (strcmp(dust_in_iter->name_buffer, "send-msg") == 0) {
        event_class = dust_in_iter->dust_in->send_msg_event_class;
    } else {
        event_class = dust_in_iter->dust_in->recv_msg_event_class;
    }

    /*
     * At this point `timestamp` contains seconds since the Unix epoch.
     * Multiply it by 1,000,000,000 to get nanoseconds since the Unix
     * epoch because the stream's clock's frequency is 1 GHz.
     */
    timestamp *= UINT64_C(1000000000);

    /* Add the extra microseconds (as nanoseconds) to `timestamp` */
    timestamp += extra_us * UINT64_C(1000);

    /* Create the event message */
    message = bt_message_event_create_with_default_clock_snapshot(
        self_message_iterator, event_class, dust_in_iter->dust_in->stream,
        timestamp);

    /*
     * At this point `message` is an event message which contains
     * an empty event object.
     *
     * We need to fill its fields.
     *
     * The only field to fill is the payload field's `msg` field
     * which is the event record's message.
     *
     * All the references below are borrowed references, therefore we
     * don't need to put them.
     */
    bt_event *event = bt_message_event_borrow_event(message);
    bt_field *payload_field = bt_event_borrow_payload_field(event);
    bt_field *msg_field = bt_field_structure_borrow_member_field_by_index(
        payload_field, 0);

    bt_field_string_set_value(msg_field, dust_in_iter->msg_buffer);

end:
    return message;
}

/*
 * Returns the next message to the message iterator's user.
 *
 * This method can fill the `messages` array with up to `capacity`
 * messages.
 *
 * To keep this example simple, we put a single message into `messages`
 * and set `*count` to 1 (if the message iterator is not ended).
 */
static
bt_message_iterator_class_next_method_status dust_in_message_iterator_next(
        bt_self_message_iterator *self_message_iterator,
        bt_message_array_const messages, uint64_t capacity,
        uint64_t *count)
{
    /* Retrieve our private data from the message iterator's user data */
    struct dust_in_message_iterator *dust_in_iter =
        bt_self_message_iterator_get_data(self_message_iterator);

    /*
     * This is the message to return (by moving it to the `messages`
     * array).
     *
     * We initialize it to `NULL`. If it's not `NULL` after the
     * processing below, then we move it to the message array.
     */
    bt_message *message = NULL;

    /* Initialize the return status to a success */
    bt_message_iterator_class_next_method_status status =
        BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;

    switch (dust_in_iter->state) {
    case DUST_IN_MESSAGE_ITERATOR_STATE_STREAM_BEGINNING:
        /* Create a stream beginning message */
        message = bt_message_stream_beginning_create(self_message_iterator,
            dust_in_iter->dust_in->stream);

        /* Next state: an event message */
        dust_in_iter->state = DUST_IN_MESSAGE_ITERATOR_STATE_EVENT;
        break;
    case DUST_IN_MESSAGE_ITERATOR_STATE_EVENT:
        /*
         * Create an event or a stream end message from the message
         * iterator's input file's current line.
         *
         * This function also updates the message iterator's state if
         * needed.
         */
        message = create_message_from_line(dust_in_iter,
            self_message_iterator);
        break;
    case DUST_IN_MESSAGE_ITERATOR_STATE_ENDED:
        /* Message iterator is ended: return the corresponding status */
        status =
            BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_END;
        goto end;
    }

    if (message) {
        /*
         * We created a message above: move it to the beginning of the
         * `messages` array, setting `*count` to 1 to indicate that the
         * array contains a single message.
         */
        messages[0] = message;
        *count = 1;
    }

end:
    return status;
}

/* Mandatory */
BT_PLUGIN_MODULE();

/* Define the `dust` plugin */
BT_PLUGIN(dust);

/* Define the `input` source component class */
BT_PLUGIN_SOURCE_COMPONENT_CLASS(input, dust_in_message_iterator_next);

/* Set some of the `input` source component class's optional methods */
BT_PLUGIN_SOURCE_COMPONENT_CLASS_INITIALIZE_METHOD(input, dust_in_initialize);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_FINALIZE_METHOD(input, dust_in_finalize);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD(input,
    dust_in_message_iterator_initialize);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_FINALIZE_METHOD(input,
    dust_in_message_iterator_finalize);
