# =============================================================================
# Arm Simulator Service - 机械臂模拟器服务
# =============================================================================

cmake_minimum_required(VERSION 3.16)

# 如果是独立构建（不是作为子项目被包含），则定义项目
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(arm VERSION 1.0.0 LANGUAGES CXX)
    
    # 独立构建时，设置 RPATH 以便能找到依赖库
    if(CMAKE_PREFIX_PATH)
        set(CMAKE_BUILD_RPATH "${CMAKE_PREFIX_PATH}/lib")
    endif()
endif()

# 确保能找到 perception_app 的工具函数
if(NOT COMMAND setup_executable)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../perception_app/cmake/PerceptionAppUtils.cmake")
        include("${CMAKE_CURRENT_SOURCE_DIR}/../perception_app/cmake/PerceptionAppUtils.cmake")
    endif()
endif()

# 收集源文件 (避免递归，防止包含 build 目录中的临时文件)
file(GLOB ARM_SOURCES "*.cpp")

# 机械臂模拟器可执行文件
add_executable(arm ${ARM_SOURCES})

# 如果定义了 setup_executable 则使用，否则手动配置
if(COMMAND setup_executable)
    setup_executable(arm)
else()
    # Fallback minimal setup
    target_compile_features(arm PRIVATE cxx_std_17)
endif()

# 设置包含目录
# 1. 自身目录 (以便能 include "ArmController.hpp")
target_include_directories(arm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 2. 依赖库的包含目录
# 由于 runtime 和 message 已经配置了 PUBLIC 包含目录（包括构建目录和安装目录），
# 链接它们时会自动传递包含路径。
# 如果是独立构建且使用 find_package，这里也不需要手动指定，因为 target 属性会传递。

# 链接依赖
# runtime, message 是 perception_app 定义的目标
if(TARGET runtime AND TARGET message)
    target_link_libraries(arm runtime message pthread)
else()
    message(STATUS "Building in standalone mode")
    
    # 查找依赖库
    find_library(RUNTIME_LIB NAMES runtime PATHS ${CMAKE_PREFIX_PATH}/lib NO_DEFAULT_PATH)
    find_library(MESSAGE_LIB NAMES message PATHS ${CMAKE_PREFIX_PATH}/lib NO_DEFAULT_PATH)
    
    if(NOT RUNTIME_LIB)
        message(FATAL_ERROR "runtime library not found in ${CMAKE_PREFIX_PATH}/lib")
    endif()
    
    if(NOT MESSAGE_LIB)
        message(FATAL_ERROR "message library not found in ${CMAKE_PREFIX_PATH}/lib")
    endif()
    
    message(STATUS "Found runtime: ${RUNTIME_LIB}")
    message(STATUS "Found message: ${MESSAGE_LIB}")
    
    target_link_libraries(arm ${RUNTIME_LIB} ${MESSAGE_LIB} pthread)
    
    # 添加包含目录
    target_include_directories(arm PRIVATE ${CMAKE_PREFIX_PATH}/include)
endif()


# 安装
install(TARGETS arm RUNTIME DESTINATION bin COMPONENT arm_server)

