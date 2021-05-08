#include <vm/vm.h>
#include <vm/shared.h>
#include <vm/object_manager.h>

#include <common/util.h>
#include <iostream>

#include "vm_shared_inline.cpp"

using namespace std;

namespace zero {

    Logger vm_log = Logger("vm");

    // for parameter passing, return address etc
    int64_t stack_pointer;
    z_value_t value_stack[STACK_MAX];

    vector<z_native_fnc_t> native_function_map;

    void init_native_functions() {
        native_function_map.push_back(native_print);
    }

    z_native_fnc_t get_native_fnc_at(uint64_t index) {
        return native_function_map[index];
    }

    vector<z_native_fnc_t> get_native_functions() {
        return native_function_map;
    }

    z_value_t native_print() {
        z_value_t z_value = pop();
        z_object_type_info type = object_manager_guess_type(z_value);
        string str_value;
        switch (type) {
            case VM_VALUE_TYPE_STRING:
                str_value = *z_value.string_value;
                break;
            case VM_VALUE_TYPE_DECIMAL:
                str_value = to_string(z_value.arithmetic_decimal_value);
                break;
            case VM_VALUE_TYPE_BOOLEAN:
                str_value = z_value.arithmetic_int_value ? "true" : "false";
                break;
            case VM_VALUE_TYPE_INT:
                str_value = to_string(z_value.arithmetic_int_value);
                break;
            case VM_VALUE_TYPE_FUNCTION_REF:
                str_value = "[function ref]";
                break;
            case VM_VALUE_TYPE_TYPE_OBJECT:
                str_value = "[object ref]";
                break;
            default:
                str_value = "[?]";
                break;
        }
        cout << str_value << "\n";
        return ivalue(0);
    }
}
