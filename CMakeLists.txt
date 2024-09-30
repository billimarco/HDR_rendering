cmake_minimum_required(VERSION 3.10)

# Set project name
project(HDRProject)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include directories
include_directories(include include/glad include/GLFW include/glm include/KHR include/nlohmann include/stb)

# Source files
set(SOURCES src/hdr.cpp src/glad.c)

# Add executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Find OpenGL and link libraries
find_package(OpenGL REQUIRED)
target_link_libraries(${PROJECT_NAME} OpenGL::GL)

# Link GLFW library
target_link_libraries(${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/lib/libglfw3dll.a)

# Copy shaders and resources
file(COPY ${CMAKE_SOURCE_DIR}/shader DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/resources DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/settings DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/bin/glfw3.dll DESTINATION ${CMAKE_BINARY_DIR})
