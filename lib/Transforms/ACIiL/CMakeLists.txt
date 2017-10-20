add_llvm_library(LLVMACIiL
  aciil.cpp
  FunctionCFG.cpp
  ModuleCFG.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms/ACIiL

  DEPENDS
  intrinsics_gen
  )
