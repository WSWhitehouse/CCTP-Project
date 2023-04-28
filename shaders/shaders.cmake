# Compile all shader source files into SPRIV binary files.

find_program(GLSLC NAMES glslc HINTS Vulkan_GLSLC_EXECUTABLE REQUIRED)
message(STATUS "Compiler: ${GLSLC}")

set(SHADER_SOURCE_DIR ${PROJECT_SOURCE_DIR}/shaders)
set(SHADER_BINARY_DIR ${CMAKE_BINARY_DIR}/shaders)

message(STATUS "src dir:  ${SHADER_SOURCE_DIR}")
message(STATUS "bin dir:  ${SHADER_BINARY_DIR}")

# Get all shader files for each extension
file(GLOB_RECURSE SHADERS_SRC
  ${SHADER_SOURCE_DIR}/**.vert
  ${SHADER_SOURCE_DIR}/**.frag
  ${SHADER_SOURCE_DIR}/**.comp
  ${SHADER_SOURCE_DIR}/**.geom
  ${SHADER_SOURCE_DIR}/**.tesc
  ${SHADER_SOURCE_DIR}/**.tese
  ${SHADER_SOURCE_DIR}/**.mesh
  ${SHADER_SOURCE_DIR}/**.task
  ${SHADER_SOURCE_DIR}/**.rgen
  ${SHADER_SOURCE_DIR}/**.rchit
  ${SHADER_SOURCE_DIR}/**.rmiss
)

# Create a custom target to compile all shaders
# https://stackoverflow.com/a/71317698/13195883
set(COMPILE_SHADERS shaders)
add_custom_target(${COMPILE_SHADERS})

# Create the shader binary output directory
add_custom_command(
  TARGET ${COMPILE_SHADERS}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_BINARY_DIR}
  COMMENT "Shader Output Directory: ${SHADER_BINAR_DIR}"
)

foreach(SHADER ${SHADERS_SRC})
  get_filename_component(SHADER_DIR ${SHADER} DIRECTORY)
  cmake_path(GET SHADER FILENAME SHADER_NAME)

  # NOTE(WSWhitehouse): We need to create subdirs if the shader
  # exists inside one. If not, we can proceed as normal.
  if(NOT ${SHADER_DIR} STREQUAL ${SHADER_SOURCE_DIR})
    string(REPLACE ${SHADER_SOURCE_DIR}/ "" SHADER_DIR ${SHADER_DIR})
    set(SPIRV_DIR "${SHADER_BINARY_DIR}/${SHADER_DIR}")

    add_custom_command(
      TARGET ${COMPILE_SHADERS}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${SPIRV_DIR}
      COMMENT "Creating Directory: ${SPIRV_DIR}"
    )

    set(SPIRV "${SPIRV_DIR}/${SHADER_NAME}.spv")
  else()
    set(SPIRV "${SHADER_BINARY_DIR}/${SHADER_NAME}.spv")
  endif()

  # Compile every glsl source file
  add_custom_command(
    TARGET ${COMPILE_SHADERS}
    COMMAND ${GLSLC} ${SHADER} -o ${SPIRV}
    COMMENT "Compiling glsl: ${SHADER}"
  )
endforeach()
