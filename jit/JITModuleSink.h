#pragma once

#include "ModuleSink.h"

#include "llvm/Support/Error.h"

namespace llvm {
namespace orc {
class KaleidoscopeJIT;
}
}

class JITModuleSink : public ModuleSink {
public:
  explicit JITModuleSink(llvm::orc::KaleidoscopeJIT &JIT) : JIT(JIT) {}

  void afterDefinition(CodeGenContext &CG) override;
  void afterTopLevelExpr(CodeGenContext &CG) override;

private:
  llvm::orc::KaleidoscopeJIT &JIT;
  llvm::ExitOnError ExitOnErr;
};
