add_llvm_library(LLVMACIiL
  aciil.cpp
  CFGFunction.cpp
  CFGModule.cpp
  CFGUse.cpp
  CFGNode.cpp
  CFGUtils.cpp
  ACIiLAllocaManager.cpp
  ACRIiLPointerAlias.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms/ACIiL

  DEPENDS
  intrinsics_gen
  )
