/*
 * Copyright (c) 2024 EfficiOS, Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_QUERY_EXECUTOR_HPP
#define BABELTRACE_CPP_COMMON_BT2_QUERY_EXECUTOR_HPP

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2c/c-string-view.hpp"

#include "borrowed-object.hpp"
#include "component-class.hpp"
#include "shared-object.hpp"
#include "value.hpp"

namespace bt2 {
namespace internal {

struct QueryExecutorRefFuncs final
{
    static void get(const bt_query_executor * const libObjPtr) noexcept
    {
        bt_query_executor_get_ref(libObjPtr);
    }

    static void put(const bt_query_executor * const libObjPtr) noexcept
    {
        bt_query_executor_put_ref(libObjPtr);
    }
};

} /* namespace internal */

template <typename LibObjT>
class CommonQueryExecutor final : public BorrowedObject<LibObjT>
{
private:
    using typename BorrowedObject<LibObjT>::_ThisBorrowedObject;

public:
    using typename _ThisBorrowedObject::LibObjPtr;
    using Shared = SharedObject<CommonQueryExecutor, LibObjT, internal::QueryExecutorRefFuncs>;

    explicit CommonQueryExecutor(const LibObjPtr libObjPtr) noexcept :
        _ThisBorrowedObject {libObjPtr}
    {
    }

    template <typename OtherLibObjT>
    CommonQueryExecutor(const CommonQueryExecutor<OtherLibObjT> queryExec) noexcept :
        _ThisBorrowedObject {queryExec}
    {
    }

    template <typename OtherLibObjT>
    CommonQueryExecutor operator=(const CommonQueryExecutor<OtherLibObjT> queryExec) noexcept
    {
        _ThisBorrowedObject::operator=(queryExec);
        return *this;
    }

    static Shared create(const ConstComponentClass compCls, const bt2c::CStringView objectName,
                         const OptionalBorrowedObject<ConstMapValue> params = {})
    {
        return CommonQueryExecutor::_create(compCls, objectName, params,
                                            static_cast<void *>(nullptr));
    }

    template <typename QueryDataT>
    static Shared create(const ConstComponentClass compCls, const bt2c::CStringView objectName,
                         QueryDataT& queryData,
                         const OptionalBorrowedObject<ConstMapValue> params = {})
    {
        return CommonQueryExecutor::_create(compCls, objectName, params, &queryData);
    }

    ConstValue::Shared query() const
    {
        static_assert(!std::is_const<LibObjT>::value,
                      "Not available with `bt2::ConstQueryExecutor`.");

        const bt_value *res;
        const auto status = bt_query_executor_query(this->libObjPtr(), &res);

        if (status == BT_QUERY_EXECUTOR_QUERY_STATUS_ERROR) {
            throw Error {};
        } else if (status == BT_QUERY_EXECUTOR_QUERY_STATUS_MEMORY_ERROR) {
            throw MemoryError {};
        } else if (status == BT_QUERY_EXECUTOR_QUERY_STATUS_AGAIN) {
            throw TryAgain {};
        } else if (status == BT_QUERY_EXECUTOR_QUERY_STATUS_UNKNOWN_OBJECT) {
            throw UnknownObject {};
        }

        return ConstValue::Shared::createWithoutRef(res);
    }

private:
    template <typename QueryDataT>
    static Shared _create(const ConstComponentClass compCls, const bt2c::CStringView objectName,
                          const OptionalBorrowedObject<ConstMapValue> params,
                          QueryDataT * const queryData)
    {
        const auto libObjPtr = bt_query_executor_create_with_method_data(
            compCls.libObjPtr(), objectName, params ? params->libObjPtr() : nullptr,
            const_cast<void *>(static_cast<const void *>(queryData)));

        if (!libObjPtr) {
            throw MemoryError {};
        }

        return Shared::createWithoutRef(libObjPtr);
    }
};

using QueryExecutor = CommonQueryExecutor<bt_query_executor>;
using ConstQueryExecutor = CommonQueryExecutor<const bt_query_executor>;

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_QUERY_EXECUTOR_HPP  */
