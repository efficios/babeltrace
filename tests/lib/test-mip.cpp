/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2022 EfficiOS, Inc.
 */

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2/component-class-dev.hpp"

#include "catch2/catch_test_macros.hpp"

namespace {

template <std::uint64_t LowerV, std::uint64_t UpperV, bool WithErrorV = false>
class TestSink final : public bt2::UserSinkComponent<TestSink<LowerV, UpperV, WithErrorV>>
{
public:
    static constexpr const char *name = "test-sink";

    explicit TestSink(const bt2::SelfSinkComponent selfComp, bt2::ConstMapValue, void *)
        : bt2::UserSinkComponent<TestSink> {selfComp, "TEST-SINK"}
    {
    }

    bool _consume()
    {
        return false;
    }

    static void _getSupportedMipVersions(bt2::SelfComponentClass, bt2::ConstMapValue, void *,
                                         bt2::LoggingLevel,
                                         const bt2::UnsignedIntegerRangeSet ranges)
    {
        if (WithErrorV) {
            throw bt2::Error {};
        }

        ranges.addRange(LowerV, UpperV);
    }
};

template <typename SinkAT, typename SinkBT>
void testCommon(const bt_get_greatest_operative_mip_version_status expectedStatus,
                const std::uint64_t expectedMipVersion)
{
    const auto descrs = bt_component_descriptor_set_create();

    REQUIRE(descrs);

    {
        REQUIRE(bt_component_descriptor_set_add_descriptor(
                    descrs,
                    bt_component_class_sink_as_component_class(
                        bt2::createComponentClass<SinkAT>()->libObjPtr()),
                    nullptr) == BT_COMPONENT_DESCRIPTOR_SET_ADD_DESCRIPTOR_STATUS_OK);
    }

    {
        REQUIRE(bt_component_descriptor_set_add_descriptor(
                    descrs,
                    bt_component_class_sink_as_component_class(
                        bt2::createComponentClass<SinkBT>()->libObjPtr()),
                    nullptr) == BT_COMPONENT_DESCRIPTOR_SET_ADD_DESCRIPTOR_STATUS_OK);
    }

    std::uint64_t mipVersion;

    CHECK(bt_get_greatest_operative_mip_version(descrs, BT_LOGGING_LEVEL_INFO, &mipVersion) ==
          expectedStatus);

    if (expectedStatus == BT_GET_GREATEST_OPERATIVE_MIP_VERSION_STATUS_OK) {
        CHECK(mipVersion == expectedMipVersion);
    }

    bt_component_descriptor_set_put_ref(descrs);
}

template <typename SinkAT, typename SinkBT>
void testOk(const std::uint64_t expectedMipVersion)
{
    testCommon<SinkAT, SinkBT>(BT_GET_GREATEST_OPERATIVE_MIP_VERSION_STATUS_OK, expectedMipVersion);
}

template <typename SinkAT, typename SinkBT>
void testNoMatch()
{
    testCommon<SinkAT, SinkBT>(BT_GET_GREATEST_OPERATIVE_MIP_VERSION_STATUS_NO_MATCH, 0);
}

template <typename SinkAT, typename SinkBT>
void testError()
{
    testCommon<SinkAT, SinkBT>(BT_GET_GREATEST_OPERATIVE_MIP_VERSION_STATUS_ERROR, 0);
}

using TestSink00 = TestSink<0, 0>;
using TestSink01 = TestSink<0, 1>;
using TestSink11 = TestSink<1, 1>;

} /* namespace */

TEST_CASE("No match (non-existent only)")
{
    testNoMatch<TestSink00, TestSink<0xffffffffff0, 0xfffffffffff>>();
}

TEST_CASE("No match (disjoint ranges)")
{
    testNoMatch<TestSink00, TestSink11>();
}

TEST_CASE("OK (contains non-existent)")
{
    testOk<TestSink00, TestSink<0, 0xfffffffffff>>(0);
}

TEST_CASE("OK (both [0, 0])")
{
    testOk<TestSink00, TestSink00>(0);
}

TEST_CASE("OK ([0, 0] and [0, 1])")
{
    testOk<TestSink00, TestSink01>(0);
}

TEST_CASE("OK (both [0, 1])")
{
    testOk<TestSink01, TestSink01>(1);
}

TEST_CASE("OK ([0, 1] and [1, 1])")
{
    testOk<TestSink01, TestSink11>(1);
}

TEST_CASE("Error")
{
    testError<TestSink01, TestSink<0, 0, true>>();
}
