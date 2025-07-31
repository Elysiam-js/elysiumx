cmake_minimum_required(VERSION 3.10)
project(elysiumx VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add executable
add_executable(elysiumx
    src/main.cpp
    src/parser.cpp
)

# Include directories
target_include_directories(elysiumx PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Link libraries (if needed)
# target_link_libraries(elysiumx PRIVATE some_library)

# Installation (optional)
install(TARGETS elysiumx DESTINATION bin)
