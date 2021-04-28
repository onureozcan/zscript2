#include <vm/vm.h>

using namespace std;

/**
 * Object manager is responsible from
 *      - allocating - deallocating objects
 *      - holding a reference to them (later will be used in garbage collection)
 *      - detecting types (because after compilation phase, we lost type information
 */
namespace zero {

    typedef struct {
        // because parent and callee are not always the same!.
        // the context, which creates the function reference, by MOV_FNC, is the parent of the function that was referred to!
        // let's say the function A contains function B and B requires some variable in A's context
        // in that case, B, has to have a reference to A's context, and this is how it is achieved
        // A creates a z_value of type function_ref, and this ref contains both the index and the A's reference to it
        // when some other function, say C, calls B, it has to use the value created by A, because there is no other way for it to access B
        // and this way, upon calling B, C pushes parent context, which is A to the stack
        void* parent_context_ptr;
        unsigned int instruction_index; // not the pointer but the index, like 54th instruction
    } z_fnc_ref_t;

    typedef int z_object_type_info;

    static const int VM_VALUE_TYPE_INT = 0;
    static const int VM_VALUE_TYPE_DECIMAL = 1;
    static const int VM_VALUE_TYPE_STRING = 2;
    static const int VM_VALUE_TYPE_BOOLEAN = 3;
    static const int VM_VALUE_TYPE_FUNCTION_REF = 4;
    static const int VM_VALUE_TYPE_TYPE_OBJECT = 5;

    z_fnc_ref_t* object_manager_create_fn_ref(unsigned int instruction_index, z_value_t *context_object);

    void object_manager_register_string(z_value_t value);

    z_value_t* object_manager_create_object(string *type_ident, unsigned int size);

    z_object_type_info object_manager_guess_type(z_value_t value);

}