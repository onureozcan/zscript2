#include <vm/jit.h>

#define ASMJIT_STATIC

#include <asmjit/asmjit.h>
#include <common/program.h>

using namespace std;
using namespace asmjit;

namespace zero {

    JitRuntime rt;                    // Runtime specialized for JIT code execution.

    void compile_dispatch_function(Program *program, x86::Assembler &a, z_opcode_handler** handlers) {
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
            if (instruction->opCode == JMP) {
                // direct jumps can be compiled, no need to call
                auto target_label = labels.at(instruction->destination);
                a.jmp(target_label);

            } else if (opcode == JMP_TRUE && prev_opcode >= CMP_EQ && prev_opcode <= CMP_LTE_DECIMAL) {
                // cmp - jmp can be inlined
                auto target_label = labels.at(instruction->destination);
                a.cmp(x86::rax, 0);
                a.jne(target_label);

            } else if (opcode == JMP_FALSE && prev_opcode >= CMP_EQ && prev_opcode <= CMP_LTE_DECIMAL) {
                // cmp - jmp can be inlined
                auto target_label = labels.at(instruction->destination);
                a.cmp(x86::rax, 0);
                a.je(target_label);

            } else {
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
            prev_opcode = opcode;
            instruction++;
        }
    }


    z_jit_fnc baseline_jit(Program* program, z_opcode_handler** handlers) {

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