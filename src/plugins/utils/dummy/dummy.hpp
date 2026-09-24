/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2026 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_PLUGINS_UTILS_DUMMY_DUMMY_HPP
#define BABELTRACE_PLUGINS_UTILS_DUMMY_DUMMY_HPP

#include "cpp-common/bt2/component-class-dev.hpp"
#include "cpp-common/bt2/message-iterator.hpp"

namespace bt2dummy {

class Comp final : public bt2::UserSinkComponent<Comp>
{
    friend bt2::UserSinkComponent<Comp>;

public:
    explicit Comp(bt2::SelfSinkComponent selfComp, bt2::ConstMapValue params, void *);

protected:
    static void _getSupportedMipVersions(bt2::SelfComponentClass, bt2::ConstValue, void *,
                                         bt2::LoggingLevel, bt2::UnsignedIntegerRangeSet ranges);

private:
    void _graphIsConfigured();
    bool _consume();

    bt2::MessageIterator::Shared _mMsgIter;
};

} /* namespace bt2dummy */

#endif /* BABELTRACE_PLUGINS_UTILS_DUMMY_DUMMY_HPP */
