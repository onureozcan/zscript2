#include <vm/jit.h>

#define ASMJIT_STATIC

using namespace std;
using namespace asmjit;

namespace zero {

    typedef void (jit_opcode_compiler)(uint64_t op1, uint64_t op2, uint64_t dest, vector<Label> *labels,
                                       x86::Assembler &);

    void compile_add_int(uint64_t op1, uint64_t op2, uint64_t dest, vector<Label> *labels, x86::Assembler &a) {
        a.mov(x86::edx, x86::dword_ptr(x86::r12, op1 + 4));
        a.add(x86::edx, x86::dword_ptr(x86::r12, op2 + 4));
        a.mov(x86::dword_ptr(x86::r12, dest), 0x1);
        a.mov(x86::dword_ptr(x86::r12, dest + 4), x86::edx);
    }

    void compile_mod_int(uint64_t op1, uint64_t op2, uint64_t dest, vector<Label> *labels, x86::Assembler &a) {
        a.mov(x86::eax, x86::dword_ptr(x86::r12, op1 + 4));
        a.cdq();
        a.idiv(x86::dword_ptr(x86::r12, op2 + 4));
        a.mov(x86::dword_ptr(x86::r12, dest), 0x1);
        a.mov(x86::dword_ptr(x86::r12, dest + 4), x86::edx);
    }

    void compile_cmp_lt_int(uint64_t op1, uint64_t op2, uint64_t dest, vector<Label> *labels, x86::Assembler &a) {
        a.mov(x86::eax, x86::dword_ptr(x86::r12, op2 + 4));
        a.cmp(x86::dword_ptr(x86::r12, op1 + 4), x86::eax);
        a.setl(x86::al);
        a.movsx(x86::eax, x86::al);
        a.mov(x86::dword_ptr(x86::r12, dest), 0x3);
        a.mov(x86::dword_ptr(x86::r12, dest + 4), x86::eax);
    }

    void compile_cmp_lte_int(uint64_t op1, uint64_t op2, uint64_t dest, vector<Label> *labels, x86::Assembler &a) {
        a.mov(x86::eax, x86::dword_ptr(x86::r12, op2 + 4));
        a.cmp(x86::dword_ptr(x86::r12, op1 + 4), x86::eax);
        a.setle(x86::al);
        a.movsx(x86::eax, x86::al);
        a.mov(x86::dword_ptr(x86::r12, dest), 0x3);
        a.mov(x86::dword_ptr(x86::r12, dest + 4), x86::eax);
    }

    void compile_cmp_eq(uint64_t op1, uint64_t op2, uint64_t dest, vector<Label> *labels, x86::Assembler &a) {
        a.mov(x86::eax, x86::dword_ptr(x86::r12, op2 + 4));
        a.cmp(x86::dword_ptr(x86::r12, op1 + 4), x86::eax);
        a.sete(x86::al);
        a.movsx(x86::eax, x86::al);
        a.mov(x86::dword_ptr(x86::r12, dest), 0x3);
        a.mov(x86::dword_ptr(x86::r12, dest + 4), x86::eax);
    }

    void compile_mov(uint64_t op1, uint64_t op2, uint64_t dest, vector<Label> *labels, x86::Assembler &a) {
        a.mov(x86::rdx, x86::ptr(x86::r12, op1, 8));
        a.mov(x86::ptr(x86::r12, dest, 8), x86::rdx);
    }

    void compile_mov_int(uint64_t op1, uint64_t op2, uint64_t dest, vector<Label> *labels, x86::Assembler &a) {
        a.mov(x86::dword_ptr(x86::r12, dest), 0x1);
        a.mov(x86::dword_ptr(x86::r12, dest + 4), op1);
    }

    void compile_jmp(uint64_t op1, uint64_t op2, uint64_t dest, vector<Label> *labels, x86::Assembler &a) {
        auto target_label = labels->at(dest);
        a.jmp(target_label);
    }

    void compile_mov_decimal(uint64_t op1, uint64_t op2, uint64_t dest, vector<Label> *labels, x86::Assembler &a) {
        auto bit_representation_64 = op1;
        double dvalue = *reinterpret_cast<double*>(&bit_representation_64);
        auto fvalue = (float) dvalue;
        auto bit_representation_32 = *reinterpret_cast<uint32_t *>(&fvalue);

        a.mov(x86::eax, bit_representation_32);
        a.movq(x86::xmm(0), x86::eax);
        a.mov(x86::dword_ptr(x86::r12, dest), 0x2);
        a.movss(x86::dword_ptr(x86::r12, dest + 4), x86::xmm(0));
    }

    // some opcodes are just too simple that we can inline them
    static map<int, jit_opcode_compiler *> opcode_compilers_map = {
            {ADD_INT,     compile_add_int},
            {CMP_LT_INT,  compile_cmp_lt_int},
            {CMP_LTE_INT, compile_cmp_lte_int},
            {CMP_EQ,      compile_cmp_eq},
            {MOV,         compile_mov},
            {MOV_INT,     compile_mov_int},
            {MOD_INT,     compile_mod_int},
            {JMP,         compile_jmp},
            {MOV_DECIMAL, compile_mov_decimal}
    };

    JitRuntime rt;                    // Runtime specialized for JIT code execution.

    void compile_dispatch_function(Program *program, x86::Assembler &a, z_opcode_handler **handlers) {
#ifdef linux
        auto op1_reg = x86::rdi;
        auto op2_reg = x86::rsi;
        auto dest_reg = x86::rdx;
#else
        auto op1_reg = x86::rcx;
        auto op2_reg = x86::rdx;
        auto dest_reg = x86::r8;
#endif

        auto instructions = program->getInstructions();

        uint64_t count = instructions.size();
        vector<Label> labels;
        // bind all labels first
        for (int i = 0; i < count; i++) {
            auto label = a.newLabel();
            label.setId(i);
            labels.push_back(label);
        }

        int prev_opcode = 0;
        for (int i = 0; i < count; i++) {

            auto *instruction = instructions.at(i);
            auto descriptor = instructionDescriptionTable.find(instruction->opCode)->second;
            auto label = labels.at(i);

            auto opcode = instruction->opCode;
            auto op1 = instruction->operand1;
            auto op2 = instruction->operand2;
            auto destination = instruction->destination;

            auto handler_address = (uintptr_t) handlers[opcode - 2];

            if (descriptor.destType == INDEX) {
                // destination offset pre-calculate
                destination *= sizeof(z_value_t);
            }
            // value offset pre-calculate
            if (descriptor.op1Type == INDEX) {
                op1 *= sizeof(z_value_t);
            }
            if (descriptor.op2Type == INDEX) {
                op2 *= sizeof(z_value_t);
            }

            a.bind(label);
            if (descriptor.opcodeType == FUNCTION_ENTER) {
                // prepare a stack frame
                a.push(x86::rbp);
                a.mov(x86::rbp, x86::rsp);
                a.sub(x86::rsp, sizeof(uint64_t) * 4);
            }

            if ((opcode == JMP_TRUE || opcode == JMP_FALSE) &&
                (prev_opcode >= CMP_EQ &&
                 prev_opcode <= CMP_LTE_DECIMAL)) {
                // cmp - jmp can be inlined
                auto target_label = labels.at(instruction->destination);
                a.cmp(x86::rax, 0);
                if (opcode == JMP_TRUE)
                    a.jne(target_label);
                else
                    a.je(target_label);

            } else {

                auto opcode_compile_handler = opcode_compilers_map.find(opcode);
                // inlineable
                if (opcode_compile_handler != opcode_compilers_map.end()) {
                    opcode_compile_handler->second(op1, op2, destination, &labels, a);
                } else {
                    // standard compilation path
                    // bind parameters
                    if (descriptor.op1Type == IMM_ADDRESS) {
                        // we need to convert it to real address
                        auto target_label = labels.at(instruction->operand1);
                        a.lea(op1_reg, x86::ptr(target_label));
                    } else if (descriptor.op1Type != UNUSED) {
                        a.mov(op1_reg, op1);
                    }
                    if (descriptor.op2Type != UNUSED) {
                        a.mov(op2_reg, op2);
                    }
                    if (descriptor.destType != UNUSED) {
                        a.mov(dest_reg, destination);
                    }
                    a.call(handler_address);
                    if (descriptor.opcodeType == JUMP) {
                        auto target_label = labels.at(instruction->destination);
                        a.cmp(x86::rax, 0);
                        a.jne(target_label);
                    } else if (opcode == CALL) {
                        a.call(x86::rax);
                    } else if (opcode == RET) {
                        a.add(x86::rsp, sizeof(uint64_t) * 4);
                        a.pop(x86::rbp);
                        a.ret();
                    }
                }
            }
            prev_opcode = opcode;
        }
    }


    z_jit_fnc baseline_jit(Program *program, z_opcode_handler **handlers) {

        CodeHolder code;                  // Holds code and relocation information.
        code.init(rt.environment());      // Initialize code to match the JIT environment.
        x86::Assembler a(&code);          // Create and attach x86::Assembler to code.

        //StringLogger logger;         // Logger should always survive CodeHolder.
        //code.setLogger(&logger);     // Attach the `logger` to `code` holder.

        compile_dispatch_function(program, a, handlers);

        //printf("generated dispatch program: %s\n", logger.data());

        z_jit_fnc fn;                          // Holds address to the generated function.
        Error err = rt.add(&fn, &code);   // Add the generated code to the runtime.
        if (err) {// Handle a possible error returned by AsmJit.
            exit(1);
        }
        return fn;
    }
}