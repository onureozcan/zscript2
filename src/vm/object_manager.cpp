#include <vm/object_manager.h>

#include <map>

using namespace std;

namespace zero {

    Logger object_man_log("object_manager");

    map<uintptr_t, z_object_type_info> type_knowledge_map;

    z_fnc_ref_t *object_manager_create_fn_ref(uint64_t instruction_index, z_value_t *context_object) {
        auto fun_ref = (z_fnc_ref_t *) malloc(sizeof(z_fnc_ref_t));
        if (fun_ref == nullptr) {
            object_man_log.error("could not allocate memory for a function reference");
            exit(1);
        }
        fun_ref->parent_context_ptr = context_object;
        fun_ref->instruction_index = instruction_index;

        type_knowledge_map[(uintptr_t) fun_ref] = VM_VALUE_TYPE_FUNCTION_REF;
        return fun_ref;
    }

    void object_manager_register_string(z_value_t value) {
        type_knowledge_map[(uintptr_t) value.uint_value] = VM_VALUE_TYPE_STRING;
    }

    z_object_type_info object_manager_guess_type(z_value_t value) {
        uint64_t type = value.uint_value & 7;
        if (type == 0) { // because pointers are 8 byte aligned, it means this is a pointer and not a primitive
            if (type_knowledge_map.find(value.uint_value) != type_knowledge_map.end()) {
                return type_knowledge_map[value.uint_value];
            } else return -1;
        }
        return type;
    }

    bool object_manager_is_null(z_value_t value) {
        return (value.uint_value & 7) == VM_VALUE_TYPE_NULL;
    }
}