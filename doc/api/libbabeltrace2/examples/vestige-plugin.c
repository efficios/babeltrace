/* Component class method declarations */
#include "vestige.h"

/* Always start with this line */
BT_PLUGIN_MODULE();

/* Declare the `vestige` plugin */
BT_PLUGIN(vestige);

/* Set some optional plugin properties */
BT_PLUGIN_DESCRIPTION("Input and output for the Vestige format.");
BT_PLUGIN_AUTHOR("Denis Rondeau");
BT_PLUGIN_LICENSE("MIT");

/* Add the `input` source component class */
BT_PLUGIN_SOURCE_COMPONENT_CLASS(input, vestige_in_iter_next);

/* Set the source component class's optional description */
BT_PLUGIN_SOURCE_COMPONENT_CLASS_DESCRIPTION(input,
    "Read a Vestige trace file.");

/* Set some optional methods of the source component class */
BT_PLUGIN_SOURCE_COMPONENT_CLASS_INITIALIZE_METHOD(input,
    vestige_in_init);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_FINALIZE_METHOD(input,
    vestige_in_finalize);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD(
    input, vestige_in_iter_init);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_FINALIZE_METHOD(
    input, vestige_in_iter_fini);

/* Add the `output` sink component class */
BT_PLUGIN_SINK_COMPONENT_CLASS(output, vestige_out_consume);

/* Set the sink component class's optional description */
BT_PLUGIN_SINK_COMPONENT_CLASS_DESCRIPTION(output,
    "Write a Vestige trace file.");

/* Set some optional methods of the sink component class */
BT_PLUGIN_SINK_COMPONENT_CLASS_INITIALIZE_METHOD(output,
    vestige_out_init);
BT_PLUGIN_SINK_COMPONENT_CLASS_FINALIZE_METHOD(output,
    vestige_out_finalize);
BT_PLUGIN_SINK_COMPONENT_CLASS_GRAPH_IS_CONFIGURED_METHOD(output,
    vestige_out_graph_is_configured);
