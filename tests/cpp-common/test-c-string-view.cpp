/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2024 EfficiOS, Inc.
 */

#include <string>

#include "cpp-common/bt2c/c-string-view.hpp"

#include "tap/tap.h"

namespace {

template <typename StrT>
const char *asConstCharPtr(StrT&& val)
{
    return val.data();
}

const char *asConstCharPtr(const char * const val)
{
    return val;
}

const char *typeName(bt2c::CStringView)
{
    return "bt2c::CStringView";
}

const char *typeName(const char *)
{
    return "const char *";
}

const char *typeName(const std::string&)
{
    return "std::string";
}

template <typename Str1T, typename Str2T>
void testEq(Str1T&& lhs, Str2T&& rhs)
{
    BT_ASSERT(asConstCharPtr(lhs) != asConstCharPtr(rhs));
    ok(lhs == rhs, "`%s` == `%s`", typeName(lhs), typeName(rhs));
}

template <typename Str1T, typename Str2T>
void testNe(Str1T&& lhs, Str2T&& rhs)
{
    BT_ASSERT(asConstCharPtr(lhs) != asConstCharPtr(rhs));
    ok(lhs != rhs, "`%s` != `%s`", typeName(lhs), typeName(rhs));
}

void testEquality()
{
    const std::string foo1 = "foo", foo2 = "foo";
    const std::string bar = "bar";

    /* `CStringView` vs `CStringView` */
    testEq(bt2c::CStringView {foo1}, bt2c::CStringView {foo2});
    testNe(bt2c::CStringView {foo1}, bt2c::CStringView {bar});

    /* `CStringView` vs `const char *` */
    testEq(bt2c::CStringView {foo1}, foo2.c_str());
    testNe(bt2c::CStringView {foo1}, bar.c_str());
    testEq(foo1.c_str(), bt2c::CStringView {foo2});
    testNe(foo1.c_str(), bt2c::CStringView {bar});

    /* `StringView` vs `std::string` */
    testEq(bt2c::CStringView {foo1}, foo2);
    testNe(bt2c::CStringView {foo1}, bar);
    testEq(foo1, bt2c::CStringView {foo2});
    testNe(foo1, bt2c::CStringView {bar});
}

} /* namespace */

int main()
{
    plan_tests(10);
    testEquality();
    return exit_status();
}
