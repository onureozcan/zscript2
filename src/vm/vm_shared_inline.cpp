#include <vm/vm.h>
#include <vm/shared.h>
#include <vm/object_manager.h>

#include <common/util.h>

using namespace std;

namespace zero {

    inline z_value_t *alloc(unsigned int size) {
        auto *ptr = static_cast<z_value_t *>(malloc_aligned(size * sizeof(z_value_t)));
        if (ptr == nullptr) {
            vm_log.error("could not allocate %d size frame!", size);
            exit(1);
        }
        return ptr;
    }

    inline z_value_t uvalue(uint64_t _val) {
        z_value_t val;
        val.uint_value = _val;
        return val;
    }

    inline z_value_t ivalue(int32_t _val) {
        z_value_t val;
        val.uint_value = PRIMITIVE_TYPE_INT | (((uint64_t) _val) << 32);
        return val;
    }

    inline z_value_t bvalue(int32_t _val) {
        z_value_t val;
        val.uint_value = PRIMITIVE_TYPE_BOOLEAN | (((uint64_t) _val) << 32);
        return val;
    }

    inline z_value_t dvalue(float _val) {
        z_value_t val;
        val.arithmetic_decimal_value = _val;
        val.primitive_type = PRIMITIVE_TYPE_DOUBLE;
        return val;
    }

    inline z_value_t pvalue(void *_val) {
        z_value_t val;
        val.ptr_value = _val;
        return val;
    }

    inline z_value_t svalue(string *_val) {
        z_value_t val;
        val.string_value = _val;
        object_manager_register_string(val);
        return val;
    }

    inline z_value_t fvalue(unsigned int instruction_index, z_value_t *context_object) {
        z_value_t val;
        val.ptr_value = object_manager_create_fn_ref(instruction_index, context_object);
        return val;
    }

    inline void push(z_value_t value) {
        if (stack_pointer > STACK_MAX) {
            vm_log.error("stack overflow!", stack_pointer);
            exit(1);
        }
        value_stack[stack_pointer] = value;
        stack_pointer++;
    }

    inline z_value_t pop() {
        stack_pointer--;
        if (stack_pointer < 0) {
            vm_log.error("stack underflow!", stack_pointer);
            exit(1);
        }
        auto ret = value_stack[stack_pointer];
        return ret;
    }

    inline void init_call_context(z_value_t *context_object, z_value_t *parent_context) {
        context_object[0] = pvalue(parent_context); // 0th index is a pointer to parent
        if (parent_context == nullptr) {
            vector<z_native_fnc_t> functions = get_native_functions();
            // init native functions
            for (unsigned int i = 0; i < functions.size(); i++) {
                context_object[i + 1] = uvalue(i);
            }
        }
    }
}