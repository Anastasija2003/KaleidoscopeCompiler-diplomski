#pragma once

#include <string>

class Parser;
class CodeGenContext;

namespace llvm {
class TargetMachine;
}

void runIncrementalCompile(Parser &P, CodeGenContext &CG,
                            llvm::TargetMachine &TM,
                            const std::string &CacheDir);
