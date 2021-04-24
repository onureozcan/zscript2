#include <common/util.h>

#include <malloc.h>
#include <cstdint>

namespace zero {

    void *malloc_aligned(size_t size, unsigned int alignment) {
        size_t size_of_an_address = sizeof(uintptr_t);
        // lets ask for something bigger, just in case
        auto address = (uintptr_t) malloc(size + alignment + size_of_an_address);
        if (address == 0) {
            return nullptr;
        }
        auto aligned_address = address + address % alignment;
        auto address_to_ret = aligned_address + size_of_an_address;
        *((uintptr_t *) aligned_address) = address; // save the original address, will be useful when freeing
        return (void *) address_to_ret;
    }

    void free_aligned_ptr(void *aligned_ptr) {
        auto address = (uintptr_t) aligned_ptr;
        auto ptr_to_the_original_address = address - sizeof(uintptr_t);
        void *original_address = (void *) *((uintptr_t *) ptr_to_the_original_address);
        free(original_address);
    }
}