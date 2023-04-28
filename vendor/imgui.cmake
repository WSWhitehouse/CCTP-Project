# CMake file for adding imgui to the project
# https://github.com/ocornut/imgui

set(IMGUI_DIR ${PROJECT_SOURCE_DIR}/vendor/imgui)

set(IMGUI_SOURCES
  # Core
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp

  # Renderer Backends
  ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp

  # Platform Backends
  ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
)

target_sources(${PROJECT_NAME} PUBLIC ${IMGUI_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE ${IMGUI_DIR})

if (WIN32)
  target_link_libraries(${PROJECT_NAME} dwmapi)
endif()