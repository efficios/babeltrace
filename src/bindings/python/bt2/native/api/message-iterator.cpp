/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include <cstdint>

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2/message-array.hpp"
#include "cpp-common/bt2/message-iterator.hpp"
#include "cpp-common/bt2/self-component-port.hpp"
#include "cpp-common/bt2/self-message-iterator.hpp"
#include "cpp-common/bt2c/exc.hpp"

#include "../api.hpp"
#include "../runtime/ptr-obj.hpp"
#include "../runtime/py-module-builder.hpp"
#include "../runtime/utils.hpp"

namespace bt2py {
namespace {

/*
 * Returns the next messages of `msgIter` as a Python list of
 * `PtrObj` objects.
 *
 * The Python layer becomes the owner of the returned messages.
 *
 * Throws `bt2c::End` when `msgIter` is ended.
 */
PyObjUP msgIterNext(const bt2::MessageIterator msgIter)
{
    auto msgs = msgIter.next();

    if (!msgs) {
        throw bt2c::End {};
    }

    auto pyMsgs = createPyList(msgs->length(), [&msgs](const auto index) -> PyObject& {
        return createPtrObj((*msgs)[index].libObjPtr());
    });

    /* Transfer the message references to the Python layer */
    msgs->release();
    return pyMsgs;
}

bt2::MessageIterator::Shared
createMsgIterFromSelfMsgIter(const bt2::SelfMessageIterator selfMsgIter,
                             const bt2::SelfComponentInputPort port)
{
    return selfMsgIter.createMessageIterator(port);
}

bt2::MessageIterator::Shared createMsgIterFromSelfSinkComp(const bt2::SelfSinkComponent selfComp,
                                                           const bt2::SelfComponentInputPort port)
{
    return selfComp.createMessageIterator(port);
}

bool msgIterCanSeekBeginning(const bt2::MessageIterator msgIter)
{
    return msgIter.canSeekBeginning();
}

void msgIterSeekBeginning(const bt2::MessageIterator msgIter)
{
    msgIter.seekBeginning();
}

bool msgIterCanSeekNsFromOrigin(const bt2::MessageIterator msgIter, const std::int64_t nsFromOrigin)
{
    return msgIter.canSeekNsFromOrigin(nsFromOrigin);
}

void msgIterSeekNsFromOrigin(const bt2::MessageIterator msgIter, const std::int64_t nsFromOrigin)
{
    msgIter.seekNsFromOrigin(nsFromOrigin);
}

} /* namespace */

void registerMessageIteratorApi(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_message_iterator_get_ref>();
    mb.addLibFunc<bt_message_iterator_put_ref>();

    mb.addFunc<createMsgIterFromSelfMsgIter>("message_iterator_create_from_message_iterator");
    mb.addFunc<createMsgIterFromSelfSinkComp>("message_iterator_create_from_sink_component");

    mb.addFunc<msgIterNext>("message_iterator_next");
    mb.addFunc<msgIterCanSeekBeginning>("message_iterator_can_seek_beginning");
    mb.addFunc<msgIterSeekBeginning>("message_iterator_seek_beginning");
    mb.addFunc<msgIterCanSeekNsFromOrigin>("message_iterator_can_seek_ns_from_origin");
    mb.addFunc<msgIterSeekNsFromOrigin>("message_iterator_seek_ns_from_origin");
    mb.addLibBoolRetFunc<bt_message_iterator_can_seek_forward>();
}

} /* namespace bt2py */
