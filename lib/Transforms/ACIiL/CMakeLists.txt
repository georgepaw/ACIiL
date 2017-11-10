add_llvm_library(LLVMACIiL
  aciil.cpp
  CFGFunction.cpp
  CFGModule.cpp
  CFGOperand.cpp
  CFGNode.cpp
  CFGUtils.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms/ACIiL

  DEPENDS
  intrinsics_gen
  )
