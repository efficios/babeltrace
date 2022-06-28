#ifndef SRC_CPP_COMMON_VECTOR_HPP
#define SRC_CPP_COMMON_VECTOR_HPP

#include <vector>
#include "common/assert.h"

namespace bt2_common {

/*
 * Moves the last entry of `vec` to the index `idx`, then removes the last entry.
 *
 * Meant to be a direct replacement for g_ptr_array_remove_index_fast(), but for
 * `std::vector`.
 */
template <typename T, typename AllocatorT>
void vectorFastRemove(std::vector<T, AllocatorT>& vec,
                      const typename std::vector<T, AllocatorT>::size_type idx)
{
    BT_ASSERT_DBG(idx < vec.size());

    if (idx < vec.size() - 1) {
        vec[idx] = std::move(vec.back());
    }

    vec.pop_back();
}

} /* namespace bt2_common */

#endif /* SRC_CPP_COMMON_VECTOR_HPP */
