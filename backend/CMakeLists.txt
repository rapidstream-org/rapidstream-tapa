include(FetchContent)
FetchContent_Declare(
  llvm
  URL "https://github.com/llvm/llvm-project/archive/llvmorg-8.0.1.tar.gz"
  URL_HASH
    "SHA512=6098efa217d6c5fcdf6d6cbc6051020e728bdbf6190ca3d5be90e9fc382b8d666f1649506f7b4b70abff6084481799eba0fb0a44ad82a0bedfab98ee855e8d22"
  )

FetchContent_GetProperties(llvm)
if(NOT llvm_POPULATED)
  function(add_llvm)
    FetchContent_Populate(llvm)
    set(CMAKE_BUILD_TYPE Release)
    set(LLVM_ENABLE_PROJECTS clang CACHE STRING "enable clang in llvm")
    set(LLVM_TARGETS_TO_BUILD X86)
    set(LLVM_INCLUDE_TOOLS ON)
    set(LLVM_INCLUDE_EXAMPLES OFF)
    set(LLVM_INCLUDE_TESTS OFF)
    set(LLVM_INCLUDE_BENCHMlRKS OFF)
    unset(CMAKE_CXX_CLANG_TIDY)

    add_subdirectory(${llvm_SOURCE_DIR}/llvm ${llvm_BINARY_DIR}
                     EXCLUDE_FROM_ALL)

    set(CLANG_INCLUDE_DIRS ${llvm_SOURCE_DIR}/clang/include
                           ${llvm_BINARY_DIR}/tools/clang/include PARENT_SCOPE)
    set(LLVM_INCLUDE_DIRS ${llvm_SOURCE_DIR}/llvm/include
                          ${llvm_BINARY_DIR}/include PARENT_SCOPE)
  endfunction()
  add_llvm()
endif()

add_executable(tlpc)
target_sources(tlpc
               PRIVATE
               tlpc.cpp
               tlp/stream.cpp
               tlp/task.cpp
               tlp/stream.h
               tlp/task.h)
target_compile_options(tlpc PRIVATE -fno-rtti)
target_include_directories(tlpc
                           PRIVATE ${CLANG_INCLUDE_DIRS} ${LLVM_INCLUDE_DIRS})
target_link_libraries(tlpc PRIVATE clangTooling)
