cmake_minimum_required(VERSION 3.26)
project(main)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# AddressSanitizer is opt-in (-DENABLE_ASAN=ON). Homebrew LLVM's ASan runtime can
# deadlock at startup on recent macOS; Apple clang (/usr/bin/clang++) works.
option(ENABLE_ASAN "Build with AddressSanitizer" OFF)
if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

find_package(Threads REQUIRED)

# --------------------------
# ARGP (glibc provides it on Linux; macOS needs `brew install argp-standalone`)
# --------------------------
if(APPLE)
    execute_process(COMMAND brew --prefix argp-standalone
            OUTPUT_VARIABLE ARGP_BREW_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
    find_path(ARGP_INCLUDE_DIR argp.h
            HINTS ${ARGP_BREW_PREFIX}/include /opt/homebrew/include /usr/local/include)
    find_library(ARGP_LIBRARY argp
            HINTS ${ARGP_BREW_PREFIX}/lib /opt/homebrew/lib /usr/local/lib)
    if(NOT ARGP_INCLUDE_DIR OR NOT ARGP_LIBRARY)
        message(FATAL_ERROR "argp not found. Run: brew install argp-standalone")
    endif()
endif()

# --------------------------
# EXECUTABLE
# --------------------------
add_executable(main symsplit.cpp solver.cpp solver.h graph.cpp graph.h)
target_link_libraries(main PRIVATE Threads::Threads)

if(APPLE)
    target_include_directories(main PRIVATE ${ARGP_INCLUDE_DIR})
    target_link_libraries(main PRIVATE ${ARGP_LIBRARY})
endif()
