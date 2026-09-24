/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2026 Philippe Proulx <pproulx@efficios.com>
 */

#include "dummy.hpp"

namespace bt2dummy {

Comp::Comp(const bt2::SelfSinkComponent selfComp, const bt2::ConstMapValue params, void *)
    : bt2::UserSinkComponent<Comp> {selfComp, "PLUGIN/SINK.UTILS.DUMMY"}
{
    BT_CPPLOGI("Initializing component.");

    /* No parameters expected */
    if (!params.isEmpty()) {
        BT_CPPLOGE_APPEND_CAUSE_AND_THROW(
            bt2c::Error, "This component expects no parameters: param-count={}", params.length());
    }

    try {
        this->_addInputPort("in");
    } catch (const bt2c::Error&) {
        BT_CPPLOGE_APPEND_CAUSE_AND_RETHROW("Failed to add a single input port.");
    }

    BT_CPPLOGI("Initialized component.");
}

void Comp::_getSupportedMipVersions(bt2::SelfComponentClass, bt2::ConstValue, void *,
                                    bt2::LoggingLevel, const bt2::UnsignedIntegerRangeSet ranges)
{
    ranges.addRange(0, 1);
}

void Comp::_graphIsConfigured()
{
    _mMsgIter = this->_createMessageIterator(this->_inputPorts()["in"]);
}

bool Comp::_consume()
{
    try {
        return _mMsgIter->next().has_value();
    } catch (const bt2c::Error&) {
        BT_CPPLOGE_APPEND_CAUSE_AND_RETHROW("Failed to get messages from upstream component.");
    }
}

} /* namespace bt2dummy */
