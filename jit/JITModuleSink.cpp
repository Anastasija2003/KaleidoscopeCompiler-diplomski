#include "JITModuleSink.h"

#include "CodeGenContext.h"
#include "KaleidoscopeJIT.h"

#include "llvm/ExecutionEngine/Orc/Core.h"

#include <cstdio>

void JITModuleSink::afterDefinition(CodeGenContext &CG) {
  ExitOnErr(JIT.addModule(CG.takeModule()));
  CG.resetModule(JIT.getDataLayout());
}

void JITModuleSink::afterTopLevelExpr(CodeGenContext &CG) {
  auto RT = JIT.getMainJITDylib().createResourceTracker();

  ExitOnErr(JIT.addModule(CG.takeModule(), RT));
  CG.resetModule(JIT.getDataLayout());

  auto ExprSymbol = ExitOnErr(JIT.lookup("__anon_expr"));

  double (*FP)() = ExprSymbol.toPtr<double (*)()>();
  fprintf(stderr, "Evaluated to %f\n", FP());

  ExitOnErr(RT->remove());
}
