# NOTE(WSWhitehouse):
# In my experience some toolchains fail to find the Vulkan SDK through the find_package() call,
# instead we check for the environment variable and do the search and fill out the variables
# manually. If the VULKAN_SDK variable is not set, we fall back onto the find package call to
# try and find it.

if (WIN32 AND DEFINED ENV{VULKAN_SDK})
  message(STATUS "Manually locating the Vulkan SDK...")
  set(Vulkan_FOUND True)
  set(Vulkan_LIBRARIES $ENV{VULKAN_SDK}/Lib/vulkan-1.lib)
  set(Vulkan_INCLUDE_DIRS $ENV{VULKAN_SDK}/Include)

  set(Vulkan_BINARY_DIR $ENV{VULKAN_SDK}/Bin)
  set(Vulkan_BINARY_DIR ${Vulkan_BINARY_DIR}/glslc.exe)

else()
  message(WARNING "Vulkan SDK environment variable not set! Please ensure the SDK is properly installed (it may require a restart)")
  message(STATUS "Using find_package() to locate Vulkan...")
  find_package(Vulkan REQUIRED COMPONENTS glslc)
endif()

if (NOT Vulkan_FOUND)
  message(FATAL_ERROR "Could not find Vulkan SDK! Please ensure the SDK is properly installed!")
endif ()

message(STATUS "Vulkan Libraries: ${Vulkan_LIBRARIES}")
message(STATUS "Vulkan Include: ${Vulkan_INCLUDE_DIRS}")
message(STATUS "Vulkan GLSLC: ${Vulkan_GLSLC_EXECUTABLE}")

target_include_directories(${PROJECT_NAME} PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${Vulkan_LIBRARIES})