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
add_subdirectory(external/entt)

# ========================
# SDL_shadercross (pre-built)
# ========================
set(SDL_SHADERCROSS_LIB_DIR "${CMAKE_SOURCE_DIR}/lib")
set(SDL_SHADERCROSS_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include/SDL3_shadercross")

find_library(SDL_SHADERCROSS_LIB
    NAMES SDL3_shadercross
    PATHS ${SDL_SHADERCROSS_LIB_DIR}
    NO_DEFAULT_PATH
)

if(NOT SDL_SHADERCROSS_LIB)
    message(FATAL_ERROR "SDL_shadercross library not found in ${SDL_SHADERCROSS_LIB_DIR}")
endif()

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

# ========================
# Includes
# ========================
target_include_directories(VideoGame PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SDL_SHADERCROSS_INCLUDE_DIR}  # Add SDL_shadercross headers
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/external
)

# ========================
# Link libraries
# ========================
target_link_libraries(VideoGame PRIVATE
    ${SDL_SHADERCROSS_LIB}  # Link SDL_shadercross first
    EnTT::EnTT
    SDL3::SDL3
)

# Force static linking of libstdc++ and libgcc
target_link_options(VideoGame PRIVATE -static-libstdc++ -static-libgcc -static)

# Preprocessor defines if needed
target_compile_definitions(VideoGame PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)

# ========================
# Post-build: copy DLLs and assets
# ========================
if(WIN32)
    # Copy SDL_shadercross and DXC DLLs
    add_custom_command(TARGET VideoGame POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/bin/SDL3_shadercross.dll"
        "${CMAKE_SOURCE_DIR}/bin/dxcompiler.dll"
        "${CMAKE_SOURCE_DIR}/bin/dxil.dll"
        "${CMAKE_SOURCE_DIR}/bin/libspirv-cross-c-shared.dll"
        $<TARGET_FILE_DIR:VideoGame>
    )
endif()

# Copy assets
add_custom_command(TARGET VideoGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:VideoGame>/assets
)