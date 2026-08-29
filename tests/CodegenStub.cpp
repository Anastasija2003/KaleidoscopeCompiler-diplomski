#include "AST.h"

llvm::Value *NumberExprAST::codegen(CodeGenContext &) { return nullptr; }
llvm::Value *VariableExprAST::codegen(CodeGenContext &) { return nullptr; }
llvm::Value *UnaryExprAST::codegen(CodeGenContext &) { return nullptr; }
llvm::Value *BinaryExprAST::codegen(CodeGenContext &) { return nullptr; }
llvm::Value *CallExprAST::codegen(CodeGenContext &) { return nullptr; }
llvm::Value *IfExprAST::codegen(CodeGenContext &) { return nullptr; }
llvm::Value *ForExprAST::codegen(CodeGenContext &) { return nullptr; }
llvm::Value *VarExprAST::codegen(CodeGenContext &) { return nullptr; }
llvm::Function *PrototypeAST::codegen(CodeGenContext &) { return nullptr; }
llvm::Function *FunctionAST::codegen(CodeGenContext &) { return nullptr; }
