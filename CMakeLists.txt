### CMAKE SETUP ###
cmake_minimum_required(VERSION 3.22)

message(STATUS "### ------- CMake ------- ###")
message(STATUS "\tVersion:   ${CMAKE_VERSION}")
message(STATUS "\tGenerator: ${CMAKE_GENERATOR}")
message(STATUS "\tCompilers")
message(STATUS "\t - C   ${CMAKE_C_COMPILER}")
message(STATUS "\t - CXX ${CMAKE_CXX_COMPILER}")
message(STATUS "")

# Force a build type if none has been provided!
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

### OPTIONS ###
option(USE_PCH "Use precompiled header" OFF)

### PROJECT ###
project(snowflake VERSION 0.1.0)

message(STATUS "### ------ Project ------ ###")
message(STATUS "\tName:    ${PROJECT_NAME}")
message(STATUS "\tVersion: ${PROJECT_VERSION}")
message(STATUS "")

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp)
#file(GLOB_RECURSE HEADERS ${PROJECT_SOURCE_DIR}/src/*.hpp)

message(STATUS "### ---- Source Files --- ###")
message(STATUS "${SOURCES}")
message(STATUS "")

add_compile_options(
  # Enable all errors/warnings
  -Wall -Wextra -Werror

  # Disable these specific errors/warnings
  -Wno-missing-field-initializers
  -Wno-gnu-zero-variadic-macro-arguments
  -Wno-gnu-anonymous-struct
  -Wno-nested-anon-types
  -Wno-cast-function-type

  # C specific errors/warnings
  $<$<COMPILE_LANGUAGE:C>:-Wno-strict-prototypes>

  # Disable these errors but keep them as warnings
  -Wno-error=missing-braces
  -Wno-error=unused-variable
  -Wno-error=unused-parameter
  -Wno-error=unused-function
  -Wno-error=unused-but-set-variable
  -Wno-error=type-limits
)

add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/src)

# Add "_DEBUG" & "_RELEASE" for appropriate build config
target_compile_definitions(${PROJECT_NAME} PRIVATE
  $<$<CONFIG:Debug>:_DEBUG>
  $<$<CONFIG:RelWithDebInfo>:_RELEASE>
  $<$<CONFIG:RelWithDebInfo>:_REL_DEBUG>
  $<$<CONFIG:Release>:_RELEASE>
)

if (WIN32)
  target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN)

  target_link_libraries(${PROJECT_NAME} Shlwapi.lib ws2_32)
endif()

if (USE_PCH)
  # Only compile a PCH with C++
  target_precompile_headers(${PROJECT_NAME} PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:${PROJECT_SOURCE_DIR}/src/pch.hpp>
  )
endif ()

# Data Directory
set(DATA_SOURCE_DIR ${PROJECT_SOURCE_DIR}/data)
set(DATA_BINARY_DIR ${CMAKE_BINARY_DIR}/data)

add_custom_target(data)

add_custom_command(
  TARGET data PRE_BUILD
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${DATA_BINARY_DIR}
)

add_custom_command(
  TARGET data POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${DATA_SOURCE_DIR} ${DATA_BINARY_DIR}
  COMMENT "Data Output Directory: ${DATA_BINARY_DIR}"
)

message(STATUS "### --- Build Options --- ###")
message(STATUS "\tBuild Type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "\tC Standard:   ${CMAKE_C_STANDARD}")
message(STATUS "\tCXX Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "\tUse Std Lib:  ${CMAKE_CXX_STANDARD_REQUIRED}")
message(STATUS "\tUse PCH:      ${USE_PCH}")
message(STATUS "")

### C-Common ###
message(STATUS "### C-Common ###")
file(GLOB_RECURSE C_COMMON_SOURCES ${PROJECT_SOURCE_DIR}/vendor/c-common/src/*.c)
target_sources(${PROJECT_NAME} PUBLIC ${C_COMMON_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/vendor/c-common/include)
message(STATUS "")

### stb ###
message(STATUS "### stb ###")
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/vendor/stb)
message(STATUS "")

### rapidjson ###
message(STATUS "### rapidjson ###")
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/vendor/rapidjson)
message(STATUS "")

### GLM ###
message(STATUS "### GLM ###")
add_subdirectory(${PROJECT_SOURCE_DIR}/vendor/glm)
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/vendor/glm/glm)
message(STATUS "")

### Vulkan SDK ###
message(STATUS "### Vulkan SDK ###")
include(${CMAKE_CURRENT_SOURCE_DIR}/vendor/VulkanSDK.cmake)
message(STATUS "")

### Shaders (SPIR-V) ###
message(STATUS "### Shaders (SPIR-V) ###")
include("${CMAKE_CURRENT_SOURCE_DIR}/shaders/shaders.cmake")
message(STATUS "")

### imgui ###
message(STATUS "### imgui ###")
include("${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui.cmake")
message(STATUS "")
