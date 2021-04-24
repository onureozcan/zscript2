#pragma once

#include <cstddef>
#include <cstdint>

namespace zero {

    void *malloc_aligned(size_t size, unsigned int alignment = sizeof(uint64_t));

    void free_aligned_ptr(void *aligned_ptr);
}