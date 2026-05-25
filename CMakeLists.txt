cmake_minimum_required(VERSION 3.20)
project(VulkanApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Options ----
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ---- Find Vulkan ----
find_package(Vulkan REQUIRED)

# ---- Find GLFW ----
# Option 1: via vcpkg (recommended on Windows)
find_package(glfw3 REQUIRED)

find_package(glm REQUIRED)

# ---- ImGui ----
file(GLOB IMGUI_CORE
    imgui/*.cpp
)

file(GLOB IMGUI_BACKENDS
    imgui/backends/*.cpp
)

set(IMGUI_SOURCES
    ${IMGUI_CORE}
    ${IMGUI_BACKENDS}
)

add_library(imgui STATIC ${IMGUI_SOURCES})

target_include_directories(imgui PUBLIC
    imgui
    imgui/backends
)

target_link_libraries(imgui PUBLIC
    Vulkan::Vulkan
    glfw
    glm::glm
)

# ---- ImGui Vulkan shader compilation (.u32) ----
set(IMGUI_SHADER_SRC
    imgui/backends/vulkan/glsl_shader.vert
    imgui/backends/vulkan/glsl_shader.frag
)

set(IMGUI_SHADER_OUTPUTS "")

foreach(shader ${IMGUI_SHADER_SRC})
    get_filename_component(shader ${shader} ABSOLUTE)
    get_filename_component(FILE_NAME ${shader} NAME)

    set(output_file "${CMAKE_BINARY_DIR}/imgui_shaders/${FILE_NAME}.u32")

    add_custom_command(
        OUTPUT ${output_file}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/imgui_shaders
        COMMAND glslangValidator -V -x -o ${output_file} ${shader}
        DEPENDS ${shader}
        COMMENT "Compiling ImGui shader ${FILE_NAME}"
        VERBATIM
    )

    list(APPEND IMGUI_SHADER_OUTPUTS ${output_file})
endforeach()

add_custom_target(imgui_shaders ALL DEPENDS ${IMGUI_SHADER_OUTPUTS})
add_dependencies(imgui imgui_shaders)

# ---- Main sources ----
file(GLOB SRC_FILES src/*.cpp)

add_executable(brdf-viewer ${SRC_FILES})

target_include_directories(brdf-viewer PRIVATE
    include
    imgui
)

target_link_libraries(brdf-viewer PRIVATE
    Vulkan::Vulkan
    glfw
    imgui
    glm::glm
)

# ---- Shader compilation ----
file(GLOB VERT_SHADERS shaders/*.vert)
file(GLOB FRAG_SHADERS shaders/*.frag)
file(GLOB COMP_SHADERS shaders/*.comp)

set(SHADER_OUTPUT_DIR ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

set(SHADER_OUTPUTS "")

foreach(shader ${VERT_SHADERS} ${FRAG_SHADERS} ${COMP_SHADERS})
    get_filename_component(FILE_NAME ${shader} NAME)
    set(output_file ${SHADER_OUTPUT_DIR}/${FILE_NAME}.spv)

    add_custom_command(
        OUTPUT ${output_file}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUTPUT_DIR}
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${shader} -o ${output_file}
        DEPENDS ${shader}
        COMMENT "Compiling shader ${FILE_NAME}"
        VERBATIM
    )

    list(APPEND SHADER_OUTPUTS ${output_file})

endforeach()

add_custom_target(shaders ALL DEPENDS ${SHADER_OUTPUTS})
add_dependencies(brdf-viewer shaders)