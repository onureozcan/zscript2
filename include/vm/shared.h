#pragma once

#include <vm/vm.h>

/**
 * This contains common functions across different vm implementations
 */
namespace zero {

    z_value_t native_print();

    inline void push(z_value_t value);

    inline z_value_t pop();

    inline z_value_t *alloc(unsigned int size);

    inline z_value_t uvalue(uint64_t _val);

    inline z_value_t ivalue(int32_t _val);

    inline z_value_t bvalue(int32_t _val);

    inline z_value_t dvalue(float _val);

    inline z_value_t pvalue(void *_val);

    inline z_value_t svalue(string *_val);

    inline z_value_t fvalue(unsigned int instruction_index, z_value_t *context_object);

    void init_native_functions();

    z_native_fnc_t get_native_fnc_at(uint64_t index);

    vector<z_native_fnc_t> get_native_functions();

}