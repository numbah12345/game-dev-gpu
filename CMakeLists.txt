cmake_minimum_required(VERSION 3.26)
project(VideoGame LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ========================
# SDL3 & other dependencies
# ========================

find_package(SDL3 REQUIRED)    # Uses your system's SDL3

# ImGui: If you use it as a subfolder
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)

# ========================
# Source files
# ========================
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)


set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlgpu3.cpp
)

# ========================
# Executable
# ========================
add_executable(VideoGame 
    ${SRC_FILES}
    ${IMGUI_SOURCES}
)

# Includes
target_include_directories(VideoGame PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# Link SDL3 and ImGui
target_link_libraries(VideoGame PRIVATE SDL3::SDL3)

# Force static linking of libstdc++ and libgcc
target_link_options(VideoGame PRIVATE -static-libstdc++ -static-libgcc -static)


# Preprocessor defines if needed
target_compile_definitions(VideoGame PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)

# ========================
# Post-build: copy assets (optional)
# ========================
add_custom_command(TARGET VideoGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:VideoGame>/assets
)
