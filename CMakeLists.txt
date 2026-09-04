# CMakeList.txt: NovaEngine 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.10)

# 如果支持，请为 MSVC 编译器启用热重载。
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

if(CMAKE_GENERATOR MATCHES "Visual Studio")
    set(CMAKE_CONFIGURATION_TYPES "Release;RelWithDebInfo;MinSizeRel" CACHE STRING "" FORCE)
endif()

if(MSVC)
    target_compile_options(YOUR_TARGET PRIVATE
        /O2          # 最大化速度
        /Ob2         # 强制内联
        /Oi          # 使用内建函数
        /Ot          # 优先速度而非大小
        /arch:AVX2   # 如果 CPU 支持，否则 /arch:SSE2
        /fp:fast     # 快速浮点运算
        /GL          # 全程序优化（配合 LTCG）
    )
    target_link_options(YOUR_TARGET PRIVATE /LTCG) # 链接时代码生成
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project ("NovaEngine")

include(${CMAKE_SOURCE_DIR}/CMake/Dependencies.cmake)

# 将源代码添加到此项目的可执行文件。
add_subdirectory(Engine)


# TODO: 如有需要，请添加测试并安装目标。
add_subdirectory(Tests)