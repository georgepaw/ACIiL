add_llvm_library(LLVMACRIiL
  acriil.cpp
  CFGFunction.cpp
  CFGModule.cpp
  CFGUse.cpp
  CFGNode.cpp
  CFGUtils.cpp
  ACRIiLAllocaManager.cpp
  ACRIiLPointerAlias.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms/ACRIiL

  DEPENDS
  intrinsics_gen
  )
