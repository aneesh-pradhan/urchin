cmake_minimum_required(VERSION 3.20)
project(urchin LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(LLVM REQUIRED CONFIG)
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
