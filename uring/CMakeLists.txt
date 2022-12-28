cmake_minimum_required(VERSION 3.11)
include(FetchContent)
project(zio
  LANGUAGES CXX
  VERSION 3.0.0
)

# option(FTXUI_BUILD_DOCS "Set to ON to build docs" ON)
option(FTXUI_BUILD_EXAMPLES "Set to ON to build examples" ON)

# option(FTXUI_BUILD_TESTS "Set to ON to build tests" OFF)
# option(FTXUI_BUILD_TESTS_FUZZER "Set to ON to enable fuzzing" OFF)
# option(FTXUI_ENABLE_INSTALL "Generate the install target" ON)
# option(FTXUI_CLANG_TIDY "Execute clang-tidy" ON)

# option(FTXUI_ENABLE_COVERAGE "Execute code coverage" OFF)


FetchContent_Declare(ftxui
  GIT_REPOSITORY "https://github.com/ArthurSonzogni/ftxui"
  GIT_TAG "a52b959f660bff301cf8e03d9fcfa491fcc0f0dc"
)

FetchContent_GetProperties(ftxui)

if(NOT ftxui_POPULATED)
  FetchContent_Populate(ftxui)
  add_subdirectory(${ftxui_SOURCE_DIR} ${ftxui_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

add_library(zio
  include/uringtest.h
  include/zio_buffer.hpp
  include/zio_http.hpp
  include/zio_ip.hpp
  include/zio_types.hpp
  include/cardinal.hpp
  src/uringtest.cpp
)
# add_definitions(-DDEBUG=1)
# add_compile_options(-fsanitize=address)
# add_link_options(-fsanitize=address)

set_target_properties(zio PROPERTIES CXX_STANDARD 20)

target_include_directories(zio PUBLIC include/)

target_link_libraries(zio PUBLIC uring)

# find_package(Threads)
add_subdirectory(examples)

# if(FTXUI_ENABLE_INSTALL)
# include(cmake/ftxui_install.cmake)
# include(cmake/ftxui_package.cmake)
# endif()