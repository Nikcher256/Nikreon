cmake_minimum_required(VERSION 3.24)

project(NikreonEngine LANGUAGES CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(NIKREON_UI_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/NikreonUI" CACHE PATH "Path to a NikreonUI checkout")
add_subdirectory("${NIKREON_UI_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/external/NikreonUI")

add_executable(NikreonEngine
    src/Engine/Editor/EditorLayer.cpp
    src/Engine/Editor/EditorUI.cpp
    src/Engine/Editor/EditorViewport.cpp
    src/Engine/Core/Application.cpp
    src/Engine/Core/Input.cpp
    src/Engine/Core/Layer.cpp
    src/Engine/Core/LayerStack.cpp
    src/Engine/Core/Log.cpp
    src/Engine/Core/Time.cpp
    src/Engine/Core/Window.cpp
    src/Engine/Renderer/Renderer.cpp
    src/Engine/Renderer/Vulkan/VulkanContext.cpp
    src/Engine/Renderer/Vulkan/VulkanViewportRenderTarget.cpp
    src/main.cpp
)

add_executable(NikreonEditorViewportTests
    src/Engine/Editor/EditorViewport.cpp
    tests/editor_viewport_tests.cpp
)

find_package(Vulkan QUIET)
find_package(VulkanHeaders CONFIG QUIET)
find_package(volk CONFIG QUIET)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Freetype REQUIRED)
find_package(Stb REQUIRED)

find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")
find_path(MINIAUDIO_INCLUDE_DIRS "miniaudio.h")
find_package(OpenAL CONFIG QUIET)
find_package(Jolt CONFIG QUIET)

target_include_directories(NikreonEngine PRIVATE
    src
    ${Stb_INCLUDE_DIR}
    ${TINYGLTF_INCLUDE_DIRS}
    ${MINIAUDIO_INCLUDE_DIRS}
)

target_include_directories(NikreonEditorViewportTests PRIVATE src)
target_link_libraries(NikreonEditorViewportTests PRIVATE glm::glm)
add_test(NAME NikreonEditorViewportTests COMMAND NikreonEditorViewportTests)

target_link_libraries(NikreonEngine PRIVATE
    glfw
    glm::glm
    EnTT::EnTT
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    NikreonUI::Vulkan
)

if(TARGET volk::volk)
    target_link_libraries(NikreonEngine PRIVATE volk::volk)
    if(TARGET volk::volk_headers)
        target_link_libraries(NikreonEngine PRIVATE volk::volk_headers)
    endif()
    target_compile_definitions(NikreonEngine PRIVATE NIKREON_HAS_VOLK=1)
elseif(TARGET Vulkan::Headers)
    target_link_libraries(NikreonEngine PRIVATE Vulkan::Headers)
    target_compile_definitions(NikreonEngine PRIVATE NIKREON_HAS_VULKAN_HEADERS=1)
elseif(TARGET Vulkan::Vulkan)
    target_link_libraries(NikreonEngine PRIVATE Vulkan::Vulkan)
    target_compile_definitions(NikreonEngine PRIVATE NIKREON_HAS_VULKAN_SDK=1)
else()
    message(FATAL_ERROR "Vulkan headers/loader not found. Install the Vulkan SDK or run vcpkg install so volk is available.")
endif()

if(TINYGLTF_INCLUDE_DIRS)
    target_compile_definitions(NikreonEngine PRIVATE NIKREON_HAS_TINYGLTF=1)
endif()

if(MINIAUDIO_INCLUDE_DIRS)
    target_compile_definitions(NikreonEngine PRIVATE NIKREON_HAS_MINIAUDIO=1)
endif()

if(TARGET OpenAL::OpenAL)
    target_link_libraries(NikreonEngine PRIVATE OpenAL::OpenAL)
    target_compile_definitions(NikreonEngine PRIVATE NIKREON_HAS_OPENAL=1)
endif()

if(TARGET Jolt::Jolt)
    target_link_libraries(NikreonEngine PRIVATE Jolt::Jolt)
    target_compile_definitions(NikreonEngine PRIVATE NIKREON_HAS_JOLT=1)
endif()

target_compile_definitions(NikreonEngine PRIVATE
    GLFW_INCLUDE_NONE
    NIKREON_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)
