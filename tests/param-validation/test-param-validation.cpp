/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) EfficiOS Inc.
 */

#include <string>

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2/value.hpp"
#include "param-parse/param-parse.hpp"

#include "plugins/common/param-validation/param-validation.h"

#include "catch2/catch_test_macros.hpp"

namespace {

void runTest(const char * const paramsStr,
             const bt_param_validation_map_value_entry_descr * const entries,
             const char * const expectedError = nullptr)
{
    const auto err = g_string_new(nullptr);

    REQUIRE(err);

    const auto params =
        bt2::ConstMapValue::Shared::createWithoutRef(bt_param_parse(paramsStr, err));
    gchar *validateError = nullptr;
    const auto status = bt_param_validation_validate(params->libObjPtr(), entries, &validateError);

    if (expectedError) {
        REQUIRE(status == BT_PARAM_VALIDATION_STATUS_VALIDATION_ERROR);
        REQUIRE(validateError);

        const bool found = std::string {validateError}.find(expectedError) != std::string::npos;

        CAPTURE(validateError, expectedError, found);
        CHECK(found);
        g_free(validateError);
    } else {
        CHECK(status == BT_PARAM_VALIDATION_STATUS_OK);
        CHECK(!validateError);
    }

    g_string_free(err, TRUE);
}

} /* namespace */

TEST_CASE("Map: valid")
{
    static const bt_param_validation_map_value_entry_descr entries[] = {
        {"carotte", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY,
         bt_param_validation_value_descr::makeSignedInteger()},
        {"fenouil", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
         bt_param_validation_value_descr::makeString()},
        {"panais", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
         bt_param_validation_value_descr::makeBool()},
        BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END,
    };

    runTest("carotte=2,fenouil=\"miam\"", entries);
}

TEST_CASE("Map: missing key")
{
    static const bt_param_validation_map_value_entry_descr entries[] = {
        {"carotte", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY,
         bt_param_validation_value_descr::makeSignedInteger()},
        {"tomate", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY,
         bt_param_validation_value_descr::makeSignedInteger()},
        BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END,
    };

    runTest("carotte=2", entries, "Error validating parameters: missing mandatory entry `tomate`");
}

TEST_CASE("Map: unexpected key")
{
    static const bt_param_validation_map_value_entry_descr entries[] = {
        {"carotte", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY,
         bt_param_validation_value_descr::makeSignedInteger()},
        BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END,
    };

    runTest("tomate=2", entries, "unexpected key `tomate`");
}

TEST_CASE("Map: invalid entry value type")
{
    static const bt_param_validation_map_value_entry_descr entries[] = {
        {"carottes", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY,
         bt_param_validation_value_descr::makeSignedInteger()},
        BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END,
    };

    runTest("carottes=\"orange\"", entries,
            "Error validating parameter `carottes`: unexpected type: "
            "expected-type=SIGNED_INTEGER, actual-type=STRING");
}

TEST_CASE("Nested error")
{
    static const bt_param_validation_value_descr navetDescr =
        bt_param_validation_value_descr::makeSignedInteger();

    static const bt_param_validation_map_value_entry_descr poireauEntries[] = {
        {"navet", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY, navetDescr},
        BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END,
    };

    static const bt_param_validation_value_descr poireauDescr =
        bt_param_validation_value_descr::makeMap(poireauEntries);

    static const bt_param_validation_map_value_entry_descr carottesElemEntries[] = {
        {"poireau", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY, poireauDescr},
        BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END,
    };

    static const bt_param_validation_value_descr carottesElemDescr =
        bt_param_validation_value_descr::makeMap(carottesElemEntries);

    static const bt_param_validation_map_value_entry_descr entries[] = {
        {"carottes", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY,
         bt_param_validation_value_descr::makeArray(0, BT_PARAM_VALIDATION_INFINITE,
                                                    carottesElemDescr)},
        BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END,
    };

    runTest("carottes=[{poireau={navet=7}}, {poireau={}}]", entries,
            "Error validating parameter `carottes[1].poireau`: missing mandatory entry `navet`");
}

TEST_CASE("Array: valid")
{
    static const bt_param_validation_value_descr elemDescr =
        bt_param_validation_value_descr::makeBool();

    static const bt_param_validation_map_value_entry_descr entries[] = {
        {"carotte", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY,
         bt_param_validation_value_descr::makeArray(2, 22, elemDescr)},
        BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END,
    };

    runTest("carotte=[true, false, true]", entries);
}

TEST_CASE("Array: empty valid")
{
    static const bt_param_validation_value_descr elemDescr =
        bt_param_validation_value_descr::makeBool();

    static const bt_param_validation_map_value_entry_descr entries[] = {
        {"carotte", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY,
         bt_param_validation_value_descr::makeArray(0, 2, elemDescr)},
        BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END,
    };

    runTest("carotte=[]", entries);
}

TEST_CASE("Array: invalid (too small)")
{
    static const bt_param_validation_value_descr elemDescr =
        bt_param_validation_value_descr::makeBool();

    static const bt_param_validation_map_value_entry_descr entries[] = {
        {"carotte", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY,
         bt_param_validation_value_descr::makeArray(1, 100, elemDescr)},
        BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END,
    };

    runTest("carotte=[]", entries,
            "Error validating parameter `carotte`: array is smaller than the minimum length: "
            "array-length=0, min-length=1");
}

TEST_CASE("Array: invalid (too large)")
{
    static const bt_param_validation_value_descr elemDescr =
        bt_param_validation_value_descr::makeBool();

    static const bt_param_validation_map_value_entry_descr entries[] = {
        {"carotte", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY,
         bt_param_validation_value_descr::makeArray(2, 2, elemDescr)},
        BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END,
    };

    runTest("carotte=[true, false, false]", entries,
            "Error validating parameter `carotte`: array is larger than the maximum length: "
            "array-length=3, max-length=2");
}

TEST_CASE("Array: invalid element type")
{
    static const bt_param_validation_value_descr elemDescr =
        bt_param_validation_value_descr::makeBool();

    static const bt_param_validation_map_value_entry_descr entries[] = {
        {"carotte", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY,
         bt_param_validation_value_descr::makeArray(3, 3, elemDescr)},
        BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END,
    };

    runTest("carotte=[true, false, 2]", entries,
            "Error validating parameter `carotte[2]`: unexpected type: "
            "expected-type=BOOL, actual-type=SIGNED_INTEGER");
}

TEST_CASE("String: valid without choices")
{
    static const bt_param_validation_map_value_entry_descr entries[] = {
        {"haricot", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY,
         bt_param_validation_value_descr::makeString()},
        BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END,
    };

    runTest("haricot=\"vert\"", entries);
}

TEST_CASE("String: valid with choices")
{
    static const char *haricotChoices[] = {"vert", "jaune", "rouge", nullptr};

    static const bt_param_validation_map_value_entry_descr entries[] = {
        {"haricot", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY,
         bt_param_validation_value_descr::makeString(haricotChoices)},
        BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END,
    };

    runTest("haricot=\"jaune\"", entries);
}

TEST_CASE("String: invalid choice")
{
    static const char *haricotChoices[] = {"vert", "jaune", "rouge", nullptr};

    static const bt_param_validation_map_value_entry_descr entries[] = {
        {"haricot", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY,
         bt_param_validation_value_descr::makeString(haricotChoices)},
        BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END,
    };

    runTest("haricot=\"violet\"", entries,
            "Error validating parameter `haricot`: string is not amongst the available choices: "
            "string=violet, choices=[vert, jaune, rouge]");
}

namespace {

bt_param_validation_status customValidationFuncValid(const bt_value * const value,
                                                     bt_param_validation_context *)
{
    const bt2::ConstValue val {value};

    REQUIRE(val.isUnsignedInteger());
    CHECK(val.asUnsignedInteger().value() == 1234);
    return BT_PARAM_VALIDATION_STATUS_OK;
}

} /* namespace */

TEST_CASE("Custom validation function: valid")
{
    static const bt_param_validation_value_descr navetDescr =
        bt_param_validation_value_descr::makeCustom(customValidationFuncValid);

    static const bt_param_validation_map_value_entry_descr entries[] = {
        {"navet", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY, navetDescr},
        BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END,
    };

    runTest("navet=+1234", entries);
}

namespace {

bt_param_validation_status customValidationFuncInvalid(const bt_value *,
                                                       bt_param_validation_context * const ctx)
{
    return bt_param_validation_error(ctx, "wrooooong");
}

} /* namespace */

TEST_CASE("Custom validation function: invalid")
{
    static const bt_param_validation_value_descr navetDescr =
        bt_param_validation_value_descr::makeCustom(customValidationFuncInvalid);

    static const bt_param_validation_map_value_entry_descr entries[] = {
        {"navet", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY, navetDescr},
        BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END,
    };

    runTest("navet=+1234", entries, "Error validating parameter `navet`: wrooooong");
}
