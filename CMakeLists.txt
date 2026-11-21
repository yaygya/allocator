cmake_minimum_required(VERSION 3.28)
project(
  yAlloc
  VERSION 1.0
  DESCRIPTION "An incredibly basic memory allocator."
  LANGUAGES C
)
include_directories(include)
file(GLOB SOURCES "src/*.c")
add_executable(main ${SOURCES})
# Sanitizers
# set(SANITIZER_FLAGS "-fsanitize=address,leak,undefined")
# add_compile_options(${SANITIZER_FLAGS})
# add_link_options(${SANITIZER_FLAGS})
