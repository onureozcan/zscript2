#include <common/program.h>
#include <compiler/compiler.h>
#include <common/logger.h>

using namespace std;

namespace zero {

    class ByteCodeGenerator::Impl {

    public:
        TypeMetadataRepository *typeMetadataRepository;

        Program *generate(ProgramAstNode *programAstNode) {
            return doGeneratorCode(programAstNode);
        }

    private:
        Logger log = Logger("bytecode_generator");
        BaseAstNode *currentAstNode = nullptr;
        Program *program;

        void errorExit(string error) {
            log.error(error.c_str());
            exit(1);
        }

        string currentNodeInfoStr() {
            return " at " + currentAstNode->fileName + " line " +
                   to_string(currentAstNode->line);
        }

        TypeInfo *type(string name) {
            return typeMetadataRepository->findTypeByName(name);
        }

        Program *doGeneratorCode(ProgramAstNode *programAstNode) {
            program = new Program(programAstNode->fileName);
            auto statements = programAstNode->statements;
            for (auto stmt: *statements) {
                visitStatement(stmt);
            }
            return program;
        }

        void visitExpression(ExpressionAstNode *expression) {

        }

        void visitReturn(StatementAstNode *stmt) {

        }

        void cast(TypeInfo *t1, TypeInfo *t2) {
            if (t1 != t2) {
                if (t1 == &TypeInfo::INT && t2 == &TypeInfo::DECIMAL) {
                    // TODO
                } else {
                    errorExit("cast from `" + t1->name + "` to `" + t2->name + "` is not yet implemented, sorry");
                }
            }
        }

        unsigned short opType(string typeName) {
            OpType ret;
            auto typeObj = type(typeName);
            if (typeObj == &TypeInfo::DECIMAL) {
                ret = OpType::DECIMAL;
            } else if (typeObj == &TypeInfo::INT) {
                ret = OpType::INT;
            } else if (typeObj == &TypeInfo::STRING) {
                ret = OpType::STRING;
            } else if (typeObj->isNative) {
                ret = OpType::NATIVE;
            } else if (typeObj->isCallable) {
                ret = OpType::FNC;
            } else {
                ret = OpType::ANY;
            }
            return (unsigned short) ret;
        }

        void visitVariable(VariableAstNode *variable) {
            auto destinationIndex = variable->memoryIndex;
            auto expectedType = type(variable->typeName);
            if (variable->initialValue != nullptr) {
                visitExpression(variable->initialValue);
                auto actualType = type(variable->initialValue->typeName);
                cast(actualType, expectedType);
                program->addInstruction({MOV, NA, variable->initialValue->memoryIndex, 0, destinationIndex});
            } else {
                program->addInstruction({MOV_I, opType(variable->typeName), variable->initialValue->memoryIndex, 0,
                                         destinationIndex});
            }
        }

        void visitStatement(StatementAstNode *stmt) {
            if (stmt->type == StatementAstNode::TYPE_EXPRESSION) {
                visitExpression(stmt->expression);
            } else if (stmt->type == StatementAstNode::TYPE_RETURN) {
                visitReturn(stmt);
            } else {
                visitVariable(stmt->variable);
            }
        }
    };

    Program *ByteCodeGenerator::generate(ProgramAstNode *programAstNode) {
        return impl->generate(programAstNode);
    }

    ByteCodeGenerator::ByteCodeGenerator(TypeMetadataRepository *typeMetadataRepository) {
        this->impl = new ByteCodeGenerator::Impl();
        this->impl->typeMetadataRepository = typeMetadataRepository;
    }
}