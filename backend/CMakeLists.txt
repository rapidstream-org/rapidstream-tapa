include(FetchContent)
FetchContent_Declare(
  llvm
  URL "https://github.com/llvm/llvm-project/archive/llvmorg-8.0.1.tar.gz"
  URL_HASH
    "SHA512=6098efa217d6c5fcdf6d6cbc6051020e728bdbf6190ca3d5be90e9fc382b8d666f1649506f7b4b70abff6084481799eba0fb0a44ad82a0bedfab98ee855e8d22"
  PATCH_COMMAND
    ln -sf ${CMAKE_CURRENT_LIST_DIR}/clang/Attr.td clang/include/clang/Basic/Attr.td
  COMMAND
    ln -sf ${CMAKE_CURRENT_LIST_DIR}/clang/SemaDeclAttr.cpp clang/lib/Sema/SemaDeclAttr.cpp
  COMMAND
    ln -sf ${CMAKE_CURRENT_LIST_DIR}/clang/SemaStmtAttr.cpp clang/lib/Sema/SemaStmtAttr.cpp
)

FetchContent_Declare(
  json
  GIT_REPOSITORY "https://github.com/nlohmann/json.git"
  GIT_TAG "master")

FetchContent_GetProperties(llvm)
if(NOT llvm_POPULATED)
  function(add_llvm)
    FetchContent_Populate(llvm)
    set(CMAKE_BUILD_TYPE Release)
    set(LLVM_ENABLE_PROJECTS
        clang
        CACHE STRING "enable clang in llvm")
    set(LLVM_APPEND_VC_REV OFF)
    set(LLVM_ENABLE_BINDINGS OFF)
    set(LLVM_TARGETS_TO_BUILD X86)
    set(LLVM_INCLUDE_TOOLS ON)
    set(LLVM_INCLUDE_EXAMPLES OFF)
    set(LLVM_INCLUDE_TESTS OFF)
    set(LLVM_INCLUDE_BENCHMlRKS OFF)
    unset(CMAKE_CXX_CLANG_TIDY)
    set(CMAKE_CXX_STANDARD 11)

    add_subdirectory(${llvm_SOURCE_DIR}/llvm ${llvm_BINARY_DIR}
                     EXCLUDE_FROM_ALL)

    set(CLANG_INCLUDE_DIRS
        ${llvm_SOURCE_DIR}/clang/include ${llvm_BINARY_DIR}/tools/clang/include
        PARENT_SCOPE)
    set(LLVM_INCLUDE_DIRS
        ${llvm_SOURCE_DIR}/llvm/include ${llvm_BINARY_DIR}/include
        PARENT_SCOPE)
  endfunction()
  add_llvm()
endif()

FetchContent_GetProperties(json)
if(NOT json_POPULATED)
  FetchContent_Populate(json)
  add_subdirectory(${json_SOURCE_DIR} ${json_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

add_library(type)
target_sources(
  type
  PUBLIC tapa/type.h
  PRIVATE tapa/type.cpp)
target_compile_features(type PUBLIC cxx_std_11)
target_include_directories(type PUBLIC ${CLANG_INCLUDE_DIRS}
                                       ${LLVM_INCLUDE_DIRS})
target_link_libraries(type PUBLIC clangTooling)

add_library(stream)
target_sources(
  stream
  PUBLIC tapa/stream.h
  PRIVATE tapa/stream.cpp)
target_link_libraries(stream PUBLIC type)

add_library(mmap)
target_sources(
  mmap
  PUBLIC tapa/mmap.h
  PRIVATE tapa/mmap.cpp)
target_link_libraries(mmap PUBLIC type)

add_library(target)
target_sources(
  target
  PUBLIC target/all_targets.h
  PUBLIC target/base_target.h
  PUBLIC target/xilinx_hls_target.h
  PRIVATE target/base_target.cpp
  PRIVATE target/xilinx_hls_target.cpp)
target_link_libraries(target PUBLIC type stream mmap)

add_library(task)
target_sources(
  task
  PUBLIC tapa/task.h
  PRIVATE tapa/task.cpp)
target_link_libraries(task PUBLIC stream mmap target nlohmann_json::nlohmann_json)

add_executable(tapacc)
target_sources(tapacc PRIVATE tapacc.cpp)
target_compile_options(tapacc PRIVATE -fno-rtti)
target_link_libraries(tapacc PRIVATE task nlohmann_json::nlohmann_json)
