/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2023 EfficiOS Inc.
 */

#include <cstring>

#include "common/assert.h"

#include "utils/run-in.hpp"

#include "tap/tap.h"

static const int NR_TESTS = 2;

static void test_string_clear()
{
    runInMsgIterClsInit([](const bt2::SelfMessageIterator self) {
        /* Boilerplate to get a string field */
        const auto traceCls =
            bt_trace_class_create(bt_self_message_iterator_borrow_component(self.libObjPtr()));
        const auto streamCls = bt_stream_class_create(traceCls);
        const auto eventCls = bt_event_class_create(streamCls);
        const auto payloadCls = bt_field_class_structure_create(traceCls);

        {
            const auto stringFieldCls = bt_field_class_string_create(traceCls);
            const auto status =
                bt_field_class_structure_append_member(payloadCls, "str", stringFieldCls);
            BT_ASSERT(status == BT_FIELD_CLASS_STRUCTURE_APPEND_MEMBER_STATUS_OK);
            bt_field_class_put_ref(stringFieldCls);
        }

        {
            const auto status = bt_event_class_set_payload_field_class(eventCls, payloadCls);
            BT_ASSERT(status == BT_EVENT_CLASS_SET_FIELD_CLASS_STATUS_OK);
        }

        const auto trace = bt_trace_create(traceCls);
        const auto stream = bt_stream_create(streamCls, trace);
        const auto msg = bt_message_event_create(self.libObjPtr(), eventCls, stream);
        const auto field = bt_field_structure_borrow_member_field_by_name(
            bt_event_borrow_payload_field(bt_message_event_borrow_event(msg)), "str");

        /* Set the field to a known non-empty value */
        {
            const auto status = bt_field_string_set_value(field, "pomme");
            BT_ASSERT(status == BT_FIELD_STRING_SET_VALUE_STATUS_OK);
            BT_ASSERT(std::strcmp(bt_field_string_get_value(field), "pomme") == 0);
        }

        /* Clear the field, verify its value and length */
        bt_field_string_clear(field);
        ok(std::strcmp(bt_field_string_get_value(field), "") == 0, "string field is empty");
        ok(bt_field_string_get_length(field) == 0, "string field length is 0");

        bt_message_put_ref(msg);
        bt_stream_put_ref(stream);
        bt_trace_put_ref(trace);
        bt_field_class_put_ref(payloadCls);
        bt_event_class_put_ref(eventCls);
        bt_stream_class_put_ref(streamCls);
        bt_trace_class_put_ref(traceCls);
    });
}

int main()
{
    plan_tests(NR_TESTS);

    test_string_clear();

    return exit_status();
}
