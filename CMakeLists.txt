cmake_minimum_required(VERSION 3.20)
project(urchin LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Tested majors: configure, build, and the test suite pass on each one.
# LLVM 21 takes a Triple in Module::setTargetTriple; 18–20 take the string.
# LLVM 23's BasicBlock::getTerminator() asserts on an unfinished block.
set(URCHIN_LLVM_MIN_MAJOR 18)
set(URCHIN_LLVM_MAX_MAJOR 23)

set(URCHIN_LLVM_INSTALL_HINT [=[
Supported versions are LLVM 18, 19, 20, 21, 22, and 23.

Ubuntu 24.04's archive ships LLVM 18:

  sudo apt-get install -y build-essential cmake llvm-dev clang
  cmake -S . -B build -DLLVM_DIR="$(llvm-config-18 --cmakedir)"

Any other supported major can be installed from https://apt.llvm.org.
LLVM 22 is a good default; use 19, 20, 21, or 23 the same way:

  sudo apt-get install -y build-essential cmake wget ca-certificates lsb-release software-properties-common gnupg
  wget -O /tmp/llvm.sh https://apt.llvm.org/llvm.sh
  chmod +x /tmp/llvm.sh
  sudo /tmp/llvm.sh 22
  sudo apt-get install -y llvm-22-dev
  export PATH="/usr/lib/llvm-22/bin:$PATH"
  cmake -S . -B build -DLLVM_DIR="$(llvm-config --cmakedir)"
]=])

find_package(LLVM CONFIG QUIET)
if(NOT LLVM_FOUND)
  message(FATAL_ERROR "Urchin could not find LLVM.\n${URCHIN_LLVM_INSTALL_HINT}")
endif()

if(NOT DEFINED LLVM_VERSION_MAJOR
   OR LLVM_VERSION_MAJOR LESS URCHIN_LLVM_MIN_MAJOR
   OR LLVM_VERSION_MAJOR GREATER URCHIN_LLVM_MAX_MAJOR)
  message(FATAL_ERROR
    "Urchin supports LLVM ${URCHIN_LLVM_MIN_MAJOR} through ${URCHIN_LLVM_MAX_MAJOR}, "
    "but this configuration found LLVM ${LLVM_PACKAGE_VERSION}.\n"
    "${URCHIN_LLVM_INSTALL_HINT}")
endif()

message(STATUS "Using LLVM ${LLVM_PACKAGE_VERSION} from ${LLVM_DIR}")

add_executable(urchin
  src/main.cpp
  src/lexer.cpp
  src/parser.cpp
  src/codegen.cpp
)

target_include_directories(urchin SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
target_compile_definitions(urchin PRIVATE ${LLVM_DEFINITIONS_LIST})
target_compile_options(urchin PRIVATE -Wall -Wextra)

if(LLVM_LINK_LLVM_DYLIB)
  target_link_libraries(urchin PRIVATE LLVM)
else()
  llvm_map_components_to_libnames(LLVM_LIBS core support targetparser)
  target_link_libraries(urchin PRIVATE ${LLVM_LIBS})
endif()

enable_testing()
add_test(NAME urchin-tests
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_tests.sh $<TARGET_FILE:urchin>
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
