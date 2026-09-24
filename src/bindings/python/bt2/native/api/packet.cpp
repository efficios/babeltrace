/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include <babeltrace2/babeltrace.h>

#include "../api.hpp"
#include "../runtime/py-module-builder.hpp"

namespace bt2py {

void registerPacketApi(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_packet_create>();
    mb.addLibFunc<bt_packet_get_ref>();
    mb.addLibFunc<bt_packet_put_ref>();
    mb.addLibFunc<bt_packet_borrow_stream>();
    mb.addLibFunc<bt_packet_borrow_stream_const>();
    mb.addLibFunc<bt_packet_borrow_context_field>();
    mb.addLibFunc<bt_packet_borrow_context_field_const>();
}

} /* namespace bt2py */
