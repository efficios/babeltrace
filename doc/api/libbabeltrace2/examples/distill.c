#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>
#include <babeltrace2/babeltrace.h>

/* Filter component's private data */
struct distill {
    /* Names of the classes of the events to discard (owned by this) */
    const bt_value *names_value;

    /* Component's input port (weak) */
    bt_self_component_port_input *in_port;
};

/*
 * Initializes the filter component.
 */
static
bt_component_class_initialize_method_status distill_initialize(
        bt_self_component_filter *self_component_filter,
        bt_self_component_filter_configuration *configuration,
        const bt_value *params, void *initialize_method_data)
{
    /* Allocate a private data structure */
    struct distill *distill = malloc(sizeof(*distill));

    /*
     * Keep a reference of the `names` array value parameter so that the
     * "next" method of a message iterator can access it to decide
     * whether or not to discard an event message.
     */
    distill->names_value =
        bt_value_map_borrow_entry_value_const(params, "names");
    bt_value_get_ref(distill->names_value);

    /* Set the component's user data to our private data structure */
    bt_self_component_set_data(
        bt_self_component_filter_as_self_component(self_component_filter),
        distill);

    /*
     * Add an input port named `in` to the filter component.
     *
     * This is needed so that this filter component can be connected to
     * a filter or a source component. With a connected upstream
     * component, this filter component's message iterator can create a
     * message iterator to consume messages.
     *
     * Add an output port named `out` to the filter component.
     *
     * This is needed so that this filter component can be connected to
     * a filter or a sink component. Once a downstream component is
     * connected, it can create our message iterator.
     */
    bt_self_component_filter_add_input_port(self_component_filter,
        "in", NULL, &distill->in_port);
    bt_self_component_filter_add_output_port(self_component_filter,
        "out", NULL, NULL);

    return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK;
}

/*
 * Finalizes the filter component.
 */
static
void distill_finalize(bt_self_component_filter *self_component_filter)
{
    /* Retrieve our private data from the component's user data */
    struct distill *distill = bt_self_component_get_data(
        bt_self_component_filter_as_self_component(self_component_filter));

    /* Put all references */
    bt_value_put_ref(distill->names_value);

    /* Free the allocated structure */
    free(distill);
}

/* Message iterator's private data */
struct distill_message_iterator {
    /* (Weak) link to the component's private data */
    struct distill *distill;

    /* Upstream message iterator (owned by this) */
    bt_message_iterator *message_iterator;
};

/*
 * Initializes the message iterator.
 */
static
bt_message_iterator_class_initialize_method_status
distill_message_iterator_initialize(
        bt_self_message_iterator *self_message_iterator,
        bt_self_message_iterator_configuration *configuration,
        bt_self_component_port_output *self_port)
{
    /* Allocate a private data structure */
    struct distill_message_iterator *distill_iter =
        malloc(sizeof(*distill_iter));

    /* Retrieve the component's private data from its user data */
    struct distill *distill = bt_self_component_get_data(
        bt_self_message_iterator_borrow_component(self_message_iterator));

    /* Keep a link to the component's private data */
    distill_iter->distill = distill;

    /* Create the uptream message iterator */
    bt_message_iterator_create_from_message_iterator(self_message_iterator,
        distill->in_port, &distill_iter->message_iterator);

    /* Set the message iterator's user data to our private data structure */
    bt_self_message_iterator_set_data(self_message_iterator, distill_iter);

    return BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_OK;
}

/*
 * Finalizes the message iterator.
 */
static
void distill_message_iterator_finalize(
        bt_self_message_iterator *self_message_iterator)
{
    /* Retrieve our private data from the message iterator's user data */
    struct distill_message_iterator *distill_iter =
        bt_self_message_iterator_get_data(self_message_iterator);

    /* Free the allocated structure */
    free(distill_iter);
}

/*
 * Returns `true` if `message` passes, that is, one of:
 *
 * * It's not an event message.
 * * The event message does not need to be discarded based on its event
 *   class's name.
 */
static
bool message_passes(struct distill_message_iterator *distill_iter,
        const bt_message *message)
{
    bool passes = true;

    /* Move as is if it's not an event message */
    if (bt_message_get_type(message) != BT_MESSAGE_TYPE_EVENT) {
        passes = false;
        goto end;
    }

    /* Borrow the event message's event and its class */
    const bt_event *event =
        bt_message_event_borrow_event_const(message);
    const bt_event_class *event_class = bt_event_borrow_class_const(event);

    /* Get the event class's name */
    const char *name = bt_event_class_get_name(event_class);

    for (uint64_t i = 0; i < bt_value_array_get_length(
            distill_iter->distill->names_value); i++) {
        const char *discard_name = bt_value_string_get(
            bt_value_array_borrow_element_by_index_const(
                distill_iter->distill->names_value, i));

        if (strcmp(name, discard_name) == 0) {
            passes = false;
            goto end;
        }
    }

end:
    return passes;
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
bt_message_iterator_class_next_method_status distill_message_iterator_next(
        bt_self_message_iterator *self_message_iterator,
        bt_message_array_const messages, uint64_t capacity,
        uint64_t *count)
{
    /* Retrieve our private data from the message iterator's user data */
    struct distill_message_iterator *distill_iter =
        bt_self_message_iterator_get_data(self_message_iterator);

    /* Consume a batch of messages from the upstream message iterator */
    bt_message_array_const upstream_messages;
    uint64_t upstream_message_count;
    bt_message_iterator_next_status next_status;

consume_upstream_messages:
    next_status = bt_message_iterator_next(distill_iter->message_iterator,
        &upstream_messages, &upstream_message_count);

    /* Initialize the return status to a success */
    bt_message_iterator_class_next_method_status status =
        BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;

    switch (next_status) {
    case BT_MESSAGE_ITERATOR_NEXT_STATUS_END:
        /* End of iteration: put the message iterator's reference */
        bt_message_iterator_put_ref(distill_iter->message_iterator);
        status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_END;
        goto end;
    case BT_MESSAGE_ITERATOR_NEXT_STATUS_AGAIN:
        status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_AGAIN;
        goto end;
    case BT_MESSAGE_ITERATOR_NEXT_STATUS_MEMORY_ERROR:
        status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_MEMORY_ERROR;
        goto end;
    case BT_MESSAGE_ITERATOR_NEXT_STATUS_ERROR:
        status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
        goto end;
    default:
        break;
    }

    /* Output message array index */
    uint64_t i = 0;

    /* For each consumed message */
    for (uint64_t upstream_i = 0; upstream_i < upstream_message_count;
            upstream_i++) {
        /* Current message */
        const bt_message *upstream_message = upstream_messages[upstream_i];

        /* Check if the upstream message passes */
        if (message_passes(distill_iter, upstream_message)) {
            /* Move upstream message to output message array */
            messages[i] = upstream_message;
            i++;
            continue;
        }

        /* Discard upstream message: put its reference */
        bt_message_put_ref(upstream_message);
    }

    if (i == 0) {
        /*
         * We discarded all the upstream messages: get a new batch of
         * messages, because this method _cannot_ return
         * `BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK` and put no
         * messages into its output message array.
         */
        goto consume_upstream_messages;
    }

    *count = i;

end:
    return status;
}

/* Mandatory */
BT_PLUGIN_MODULE();

/* Define the `distill` plugin */
BT_PLUGIN(distill);

/* Define the `theone` filter component class */
BT_PLUGIN_FILTER_COMPONENT_CLASS(theone, distill_message_iterator_next);

/* Set some of the `theone` filter component class's optional methods */
BT_PLUGIN_FILTER_COMPONENT_CLASS_INITIALIZE_METHOD(theone, distill_initialize);
BT_PLUGIN_FILTER_COMPONENT_CLASS_FINALIZE_METHOD(theone, distill_finalize);
BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD(
    theone, distill_message_iterator_initialize);
BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_FINALIZE_METHOD(theone,
    distill_message_iterator_finalize);
