#pragma once

class CodeGenContext;

class ModuleSink {
public:
  virtual ~ModuleSink() = default;

  virtual void afterDefinition(CodeGenContext &CG) = 0;
  virtual void afterTopLevelExpr(CodeGenContext &CG) = 0;
};
