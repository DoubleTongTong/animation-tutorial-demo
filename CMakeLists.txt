cmake_minimum_required(VERSION 3.20)
project(AnimationTutorialDemo LANGUAGES CXX C)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 强制使用 UTF-8 编码以支持中文注释，解决 MSVC 在中文环境下的 C4819 编码编译错误
if(MSVC)
    add_compile_options(/utf-8)
endif()

# ---------------------------------------------------------------------------
# 1. 配置 Vulkan SDK (指向用户指定的本地安装路径)
# ---------------------------------------------------------------------------
set(Vulkan_ROOT "D:/VulkanSDK/1.4.328.1")
list(APPEND CMAKE_PREFIX_PATH "${Vulkan_ROOT}")

find_package(Vulkan REQUIRED)

message(STATUS "Found Vulkan:")
message(STATUS "  Libraries: ${Vulkan_LIBRARIES}")
message(STATUS "  Include Dir: ${Vulkan_INCLUDE_DIRS}")

# ---------------------------------------------------------------------------
# 2. 通过 FetchContent 自动下载并配置 GLFW 库
#    下载到项目根目录下的 externals/glfw 文件夹中
# ---------------------------------------------------------------------------
include(FetchContent)

# 禁用编译 GLFW 的文档、测试和示例程序，以加快编译速度
set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "")
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "")
set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(GLFW_INSTALL OFF CACHE INTERNAL "")

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4 # 使用 3.4 稳定版本
    SOURCE_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/externals/glfw"
)

# 自动下载、解压并将其加入到 CMake 构建系统中
FetchContent_MakeAvailable(glfw)

# ---------------------------------------------------------------------------
# 3. 通过 FetchContent 自动下载并配置 vk-bootstrap 库
# ---------------------------------------------------------------------------
FetchContent_Declare(
    vk-bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
    GIT_TAG        main
    SOURCE_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/externals/vk-bootstrap"
)

FetchContent_MakeAvailable(vk-bootstrap)

# ---------------------------------------------------------------------------
# 3.5 通过 FetchContent 自动下载并配置 VulkanMemoryAllocator 库
# ---------------------------------------------------------------------------
FetchContent_Declare(
    VulkanMemoryAllocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG        v3.1.0
    SOURCE_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/externals/vma"
)

FetchContent_MakeAvailable(VulkanMemoryAllocator)

# ---------------------------------------------------------------------------
# 3.6 通过 FetchContent 自动下载并配置 stb 库
# ---------------------------------------------------------------------------
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    SOURCE_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/externals/stb"
)
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_Populate(stb)
endif()

# ---------------------------------------------------------------------------
# 3.7 通过 FetchContent 自动下载并配置 ImGui 库
# ---------------------------------------------------------------------------
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.0
    SOURCE_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/externals/imgui"
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

# ---------------------------------------------------------------------------
# 3.8 通过 FetchContent 自动下载并配置 tinygltf 库
# ---------------------------------------------------------------------------
FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG        v2.9.3
    SOURCE_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/externals/tinygltf"
)
FetchContent_GetProperties(tinygltf)
if(NOT tinygltf_POPULATED)
    FetchContent_Populate(tinygltf)
endif()


# ---------------------------------------------------------------------------
# 4. 创建可执行文件目标
# ---------------------------------------------------------------------------
add_executable(AnimationTutorialDemo
    main.cpp
    Window.cpp
    Logger.cpp
    vma.cpp
    renderer/VkRenderer.cpp
    renderer/Swapchain.cpp
    renderer/RenderPass.cpp
    renderer/Mesh.cpp
    renderer/Pipeline.cpp
    renderer/Texture.cpp
    renderer/Framebuffer.cpp
    renderer/UserInterface.cpp
    renderer/GltfModel.cpp
    renderer/GltfNode.cpp
    renderer/GltfAnimationChannel.cpp
    renderer/GltfAnimationClip.cpp
    tinygltf.cpp
    tools/Timer.cpp
    tools/Camera.cpp
    # ImGui sources
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp"
)

# ---------------------------------------------------------------------------
# 5. 编译着色器为 SPIR-V (构建时编译并处理依赖)
# ---------------------------------------------------------------------------
find_program(GLSL_COMPILER glslc HINTS "${Vulkan_ROOT}/Bin" ENV VULKAN_SDK)
if(NOT GLSL_COMPILER)
    message(FATAL_ERROR "glslc shader compiler not found! Please check Vulkan SDK installation.")
endif()

# 定义着色器源文件和输出的 SPV 文件
set(SHADER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/shader/triangle.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/shader/triangle.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/shader/gltf.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/shader/gltf.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/shader/gltf_dqs.vert"
)

set(SPV_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders_spv")
file(MAKE_DIRECTORY "${SPV_OUTPUT_DIR}")

set(SHADER_SPVS)
foreach(SHADER_SRC ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER_SRC} NAME)
    set(SPV_OUT "${SPV_OUTPUT_DIR}/${SHADER_NAME}.spv")

    add_custom_command(
        OUTPUT ${SPV_OUT}
        COMMAND ${GLSL_COMPILER} ${SHADER_SRC} -o ${SPV_OUT}
        DEPENDS ${SHADER_SRC}
        COMMENT "Compiling shader ${SHADER_NAME} to SPIR-V..."
    )
    list(APPEND SHADER_SPVS ${SPV_OUT})
endforeach()

# 创建自定义目标以触发着色器编译
add_custom_target(CompileShaders DEPENDS ${SHADER_SPVS})
add_dependencies(AnimationTutorialDemo CompileShaders)

# 链接 Vulkan、GLFW、vk-bootstrap 和 VulkanMemoryAllocator
target_link_libraries(AnimationTutorialDemo PRIVATE
    glfw
    Vulkan::Vulkan
    vk-bootstrap::vk-bootstrap
    GPUOpen::VulkanMemoryAllocator
)

# 包含 Vulkan 的头文件路径、stb 库及 ImGui 路径
target_include_directories(AnimationTutorialDemo PRIVATE
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer
    ${CMAKE_CURRENT_SOURCE_DIR}/tools
    ${stb_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${tinygltf_SOURCE_DIR}
)

# 定义编译后 SPV 二进制文件的目录，传递给代码使用
target_compile_definitions(AnimationTutorialDemo PRIVATE
    SHADER_SPV_DIR="${SPV_OUTPUT_DIR}/"
)

# 自动将编译出的 SPV 文件复制到可执行文件同级目录 (POST_BUILD)
add_custom_command(TARGET AnimationTutorialDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${SPV_OUTPUT_DIR}"
        "$<TARGET_FILE_DIR:AnimationTutorialDemo>"
    COMMENT "Copying compiled shaders to executable directory..."
)

# 自动将 texture 目录复制到可执行文件同级目录 (POST_BUILD)
add_custom_command(TARGET AnimationTutorialDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/texture"
        "$<TARGET_FILE_DIR:AnimationTutorialDemo>/texture"
    COMMENT "Copying texture directory to executable directory..."
)

# 自动将 assets 目录复制到可执行文件同级目录 (POST_BUILD)
add_custom_command(TARGET AnimationTutorialDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:AnimationTutorialDemo>/assets"
    COMMENT "Copying assets directory to executable directory..."
)
