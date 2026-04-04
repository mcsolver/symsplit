cmake_minimum_required(VERSION 3.26)
project(main)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable sanitizers
add_compile_options(-fsanitize=address)
add_link_options(-fsanitize=address)

# Debugging/profiling flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg -O0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg -O0")

# Release optimization
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# --------------------------
# BOOST
# --------------------------
set(BOOST_ROOT "/opt/homebrew/Cellar/boost/1.85.0")
set(BOOST_INCLUDEDIR "${BOOST_ROOT}/include")
set(BOOST_LIBRARYDIR "${BOOST_ROOT}/lib")

find_package(Boost 1.85.0 REQUIRED COMPONENTS system filesystem)

if(Boost_FOUND)
    message(STATUS "Boost include dirs: ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost libraries: ${Boost_LIBRARIES}")
    include_directories(${Boost_INCLUDE_DIRS})
    link_directories(${Boost_LIBRARY_DIRS})
endif()

# --------------------------
# ARGP
# --------------------------
set(ARGP_INCLUDE_DIR "/opt/homebrew/Cellar/argp-standalone/1.3/include")
set(ARGP_LIBRARY_DIR "/opt/homebrew/Cellar/argp-standalone/1.3/lib")

include_directories(${ARGP_INCLUDE_DIR})
link_directories(${ARGP_LIBRARY_DIR})

# --------------------------
# EXECUTABLE
# --------------------------
add_executable(main symsplit.cpp solver.cpp solver.h graph.cpp graph.h)

# Link Boost + argp
target_link_libraries(main
        PRIVATE
        ${Boost_LIBRARIES}
        argp
)
