/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2017-2023 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_PLUGINS_UTILS_MUXER_COMP_HPP
#define BABELTRACE_PLUGINS_UTILS_MUXER_COMP_HPP

#include "cpp-common/bt2/plugin-dev.hpp"

#include "msg-iter.hpp"

namespace bt2mux {

class MsgIter;

class Comp final : public bt2::UserFilterComponent<Comp, MsgIter>
{
    friend class MsgIter;
    friend bt2::UserFilterComponent<Comp, MsgIter>;

public:
    explicit Comp(bt2::SelfFilterComponent selfComp, bt2::ConstMapValue params, void *);

private:
    void _inputPortConnected(bt2::SelfComponentInputPort selfPort, bt2::ConstOutputPort otherPort);
    void _addAvailInputPort();
};

} /* namespace bt2mux */

#endif /* BABELTRACE_PLUGINS_UTILS_MUXER_COMP_HPP */
