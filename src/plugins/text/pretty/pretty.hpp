/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2016 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 * Copyright 2026 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_PLUGINS_TEXT_PRETTY_PRETTY_HPP
#define BABELTRACE_PLUGINS_TEXT_PRETTY_PRETTY_HPP

#include <fstream>
#include <iostream>
#include <optional>
#include <ostream>
#include <variant>

#include "cpp-common/bt2/component-class-dev.hpp"
#include "cpp-common/bt2/message-iterator.hpp"

#include "writer.hpp"

namespace bt2pretty {

class Comp final : public bt2::UserSinkComponent<Comp>
{
    friend bt2::UserSinkComponent<Comp>;

public:
    static constexpr auto name = "pretty";
    static constexpr auto description = "Pretty-print messages (`text` format of Babeltrace 1).";
    static constexpr auto help = "See the babeltrace2-sink.text.pretty(7) manual page.";

    explicit Comp(bt2::SelfSinkComponent selfComp, bt2::ConstMapValue params, void *);
    Comp(const Comp&) = delete;
    Comp(Comp&&) = delete;
    Comp& operator=(const Comp&) = delete;
    Comp& operator=(Comp&&) = delete;

protected:
    static void _getSupportedMipVersions(bt2::SelfComponentClass, bt2::ConstValue, void *,
                                         bt2::LoggingLevel, bt2::UnsignedIntegerRangeSet ranges);

private:
    void _graphIsConfigured();
    bool _consume();

    /* Upstream message iterator */
    bt2::MessageIterator::Shared _mMsgIter;

    /* Open if a `path` parameter was provided; otherwise closed */
    std::ofstream _mOutFile;

    /* Output stream: either `&_mOutFile` or `&std::cout` */
    std::ostream *_mOut;

    /*
     * Writer; emplaced once during construction with either
     * `Writer<true>` (when terminal color codes are enabled) or
     * `Writer<false>` (when they are not).
     */
    std::optional<std::variant<Writer<true>, Writer<false>>> _mWriter;
};

} /* namespace bt2pretty */

#endif /* BABELTRACE_PLUGINS_TEXT_PRETTY_PRETTY_HPP */
