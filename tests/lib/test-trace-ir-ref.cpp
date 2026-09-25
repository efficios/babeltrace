/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Trace IR Reference Count test
 */

#include <cstdint>
#include <optional>

#include <babeltrace2-ctf-writer/clock.h>
#include <babeltrace2-ctf-writer/event-fields.h>
#include <babeltrace2-ctf-writer/event-types.h>
#include <babeltrace2-ctf-writer/event.h>
#include <babeltrace2-ctf-writer/stream-class.h>
#include <babeltrace2-ctf-writer/stream.h>
#include <babeltrace2-ctf-writer/trace.h>
#include <babeltrace2-ctf-writer/writer.h>

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2/trace-ir.hpp"

/* clang-format off */
extern "C" {

#include "common.h"
#include "common/object-struct.h"

}
/* clang-format on */

#include "catch2/catch_test_macros.hpp"
#include "utils/run-in.hpp"

namespace {

unsigned long long getRefCount(const void * const obj)
{
    return static_cast<const bt_object *>(obj)->ref_count;
}

template <typename LibObjT>
unsigned long long getRefCount(const bt2::BorrowedObject<LibObjT> obj)
{
    return getRefCount(obj.libObjPtr());
}

bt2::StructureFieldClass::Shared createIntegerStruct(const bt2::TraceClass tc)
{
    const auto structure = tc.createStructureFieldClass();

    structure->appendMember("payload_8", *tc.createUnsignedIntegerFieldClass());
    structure->appendMember("payload_16", *tc.createUnsignedIntegerFieldClass());
    structure->appendMember("payload_32", *tc.createUnsignedIntegerFieldClass());
    return structure;
}

bt2::EventClass::Shared createSimpleEc(const bt2::StreamClass sc, const char * const name)
{
    const auto event = sc.createEventClass();

    event->name(name);
    event->payloadFieldClass(*createIntegerStruct(sc.traceClass()));
    return event;
}

bt2::EventClass::Shared createComplexEc(const bt2::StreamClass sc, const char * const name)
{
    const auto tc = sc.traceClass();
    const auto event = sc.createEventClass();

    event->name(name);

    const auto outerStruct = createIntegerStruct(tc);

    outerStruct->appendMember("payload_struct", *createIntegerStruct(tc));
    event->payloadFieldClass(*outerStruct);
    return event;
}

void createSc1(const bt2::TraceClass tc)
{
    const auto sc = tc.createStreamClass();

    sc->name("sc1");

    {
        const auto ec = createComplexEc(*sc, "ec1");

        REQUIRE(ec->streamClass().libObjPtr() == sc->libObjPtr());
    }

    {
        const auto ec = createSimpleEc(*sc, "ec2");

        REQUIRE(ec->streamClass().libObjPtr() == sc->libObjPtr());
    }
}

void createSc2(const bt2::TraceClass tc)
{
    const auto sc = tc.createStreamClass();

    sc->name("sc2");

    const auto ec = createSimpleEc(*sc, "ec3");

    REQUIRE(ec->streamClass().libObjPtr() == sc->libObjPtr());
}

bt2::TraceClass::Shared createTc1(const bt2::SelfComponent selfComp)
{
    const auto tc = selfComp.createTraceClass();

    createSc1(*tc);
    createSc2(*tc);
    return tc;
}

void testExampleScenario(const bt2::SelfComponent selfComp)
{
    bt_trace_class *weakTc1 = nullptr;
    bt_stream_class *weakSc1 = nullptr, *weakSc2 = nullptr;
    bt_event_class *weakEc1 = nullptr, *weakEc2 = nullptr, *weakEc3 = nullptr;

    struct User final
    {
        std::optional<bt2::TraceClass::Shared> tc;
        std::optional<bt2::StreamClass::Shared> sc;
        std::optional<bt2::EventClass::Shared> ec;
    };

    User userA, userB, userC;

    /* The only reference which exists at this point is on TC1 */
    {
        const auto tc1 = createTc1(selfComp);

        REQUIRE(tc1);

        /* Initialize weak refs */
        weakTc1 = tc1->libObjPtr();

        const auto sc1 = (*tc1)[0];
        const auto sc2 = (*tc1)[1];

        weakSc1 = sc1.libObjPtr();
        weakSc2 = sc2.libObjPtr();
        weakEc1 = sc1[0].libObjPtr();
        weakEc2 = sc1[1].libObjPtr();
        weakEc3 = sc2[0].libObjPtr();

        CHECK(getRefCount(weakSc1) == 0);
        CHECK(getRefCount(weakSc2) == 0);
        CHECK(getRefCount(weakEc1) == 0);
        CHECK(getRefCount(weakEc2) == 0);
        CHECK(getRefCount(weakEc3) == 0);

        /* User A has ownership of the trace */
        userA.tc = tc1->shared();
    }

    CHECK(getRefCount(**userA.tc) == 1);

    /* User A acquires a reference to SC2 from TC1 */
    {
        userA.sc = (**userA.tc)[1].shared();
        REQUIRE(userA.sc);
        CHECK(getRefCount(weakTc1) == 2);
        CHECK(getRefCount(weakSc2) == 1);
    }

    /* User A acquires a reference to EC3 from SC2 */
    {
        userA.ec = (**userA.sc)[0].shared();
        REQUIRE(userA.ec);
        CHECK(getRefCount(weakTc1) == 2);
        CHECK(getRefCount(weakSc2) == 2);
        CHECK(getRefCount(weakEc3) == 1);
    }

    /* User A releases its reference to SC2 */
    {
        INFO("User A releases SC2");
        userA.sc.reset();
        CHECK(getRefCount(weakTc1) == 2);
        CHECK(getRefCount(weakSc2) == 1);
        CHECK(getRefCount(weakEc3) == 1);
    }

    /* User A releases its reference to TC1 */
    {
        INFO("User A releases TC1");
        userA.tc.reset();
        /*
         * We keep the pointer to TC1 around to validate its reference
         * count.
         */
        CHECK(getRefCount(weakTc1) == 1);
        CHECK(getRefCount(weakSc2) == 1);
        CHECK(getRefCount(weakEc3) == 1);
    }

    /* User B acquires a reference to SC1 */
    {
        INFO("User B acquires a reference to SC1");
        userB.sc = bt2::StreamClass {weakSc1}.shared();
        CHECK(getRefCount(weakTc1) == 2);
        CHECK(getRefCount(weakSc1) == 1);
    }

    /* User C acquires a reference to EC1 */
    {
        INFO("User C acquires a reference to EC1");
        userC.ec = (**userB.sc)[0].shared();
        CHECK(getRefCount(weakEc1) == 1);
        CHECK(getRefCount(weakSc1) == 2);
    }

    /* User A releases its reference on EC3 */
    {
        INFO("User A releases its reference on EC3");
        userA.ec.reset();
        CHECK(getRefCount(weakEc3) == 0);
        CHECK(getRefCount(weakSc2) == 0);
        CHECK(getRefCount(weakTc1) == 1);
    }

    /* User B releases its reference on SC1 */
    {
        INFO("User B releases its reference on SC1");
        userB.sc.reset();
        CHECK(getRefCount(weakSc1) == 1);
    }

    /*
     * User C is the sole owner of an object and is keeping the whole
     * trace hierarchy "alive" by holding a reference to EC1.
     */
    CHECK(getRefCount(weakTc1) == 1);
    CHECK(getRefCount(weakSc1) == 1);
    CHECK(getRefCount(weakSc2) == 0);
    CHECK(getRefCount(weakEc1) == 1);
    CHECK(getRefCount(weakEc2) == 0);
    CHECK(getRefCount(weakEc3) == 0);
}

class TestRunIn final : public RunIn
{
    void onCompInit(const bt2::SelfComponent self) override
    {
        testExampleScenario(self);
    }
};

} /* namespace */

TEST_CASE("Trace IR reference counting")
{
    TestRunIn testRunIn;

    runIn(testRunIn, 0);
}

namespace {

/*
 * Unsigned integer members of the event payload of the
 * "CTF writer user" test case.
 */
struct WriterIntegerMember final
{
    const char *name;
    unsigned int size;
    std::uint64_t val;
};

constexpr WriterIntegerMember writerIntegerMembers[] = {
    {"payload_8", 8, 10},
    {"payload_16", 16, 20},
    {"payload_32", 32, 30},
};

bt_ctf_field_type *createWriterIntegerStruct()
{
    const auto structure = bt_ctf_field_type_structure_create();

    REQUIRE(structure);

    for (const auto& member : writerIntegerMembers) {
        auto ft = bt_ctf_field_type_integer_create(member.size);

        REQUIRE(ft);
        REQUIRE(bt_ctf_field_type_structure_add_field(structure, ft, member.name) == 0);
        BT_CTF_OBJECT_PUT_REF_AND_RESET(ft);
    }

    return structure;
}

} /* namespace */

TEST_CASE("CTF writer user")
{
    const auto tracePath = g_build_filename(g_get_tmp_dir(), "ctfwriter_XXXXXX", NULL);

    REQUIRE(g_mkdtemp(tracePath));

    auto writer = bt_ctf_writer_create(tracePath);

    REQUIRE(writer);
    REQUIRE(bt_ctf_writer_set_byte_order(writer, BT_CTF_BYTE_ORDER_LITTLE_ENDIAN) == 0);

    auto tc = bt_ctf_writer_get_trace(writer);

    REQUIRE(tc);

    auto sc = bt_ctf_stream_class_create("sc");

    REQUIRE(sc);

    {
        auto clock = bt_ctf_clock_create("the_clock");

        REQUIRE(clock);
        REQUIRE(bt_ctf_writer_add_clock(writer, clock) == 0);
        REQUIRE(bt_ctf_stream_class_set_clock(sc, clock) == 0);
        BT_CTF_OBJECT_PUT_REF_AND_RESET(clock);
    }

    auto stream = bt_ctf_writer_create_stream(writer, sc);

    REQUIRE(stream);

    auto ec = bt_ctf_event_class_create("ec");

    REQUIRE(ec);

    {
        auto ft = createWriterIntegerStruct();

        REQUIRE(ft);
        REQUIRE(bt_ctf_event_class_set_payload_field_type(ec, ft) == 0);
        BT_CTF_OBJECT_PUT_REF_AND_RESET(ft);
    }

    REQUIRE(bt_ctf_stream_class_add_event_class(sc, ec) == 0);

    auto event = bt_ctf_event_create(ec);

    REQUIRE(event);

    for (const auto& member : writerIntegerMembers) {
        auto field = bt_ctf_event_get_payload(event, member.name);

        REQUIRE(field);
        REQUIRE(bt_ctf_field_integer_unsigned_set_value(field, member.val) == 0);
        BT_CTF_OBJECT_PUT_REF_AND_RESET(field);
    }

    REQUIRE(bt_ctf_stream_append_event(stream, event) == 0);
    BT_CTF_OBJECT_PUT_REF_AND_RESET(event);
    BT_CTF_OBJECT_PUT_REF_AND_RESET(ec);
    BT_CTF_OBJECT_PUT_REF_AND_RESET(stream);
    BT_CTF_OBJECT_PUT_REF_AND_RESET(sc);
    BT_CTF_OBJECT_PUT_REF_AND_RESET(tc);
    BT_CTF_OBJECT_PUT_REF_AND_RESET(writer);
    recursive_rmdir(tracePath);
    g_free(tracePath);
}
