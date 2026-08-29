cmake_minimum_required (VERSION 3.10)


file(GLOB_RECURSE ENGINE_SOURCES CMAKE_CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/*.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/*.tpp"
)

list(FILTER ENGINE_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")
add_library(NovaEngineCore STATIC ${ENGINE_SOURCES})
target_compile_options(NovaEngineCore PRIVATE "/utf-8")
set_target_properties(NovaEngineCore PROPERTIES LINKER_LANGUAGE CXX)

#引用本地头文件
target_include_directories(NovaEngineCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
#引用第三方头文件
target_include_directories(NovaEngineCore PUBLIC ${NE_THIRD_PARTY_INCLUDE_DIR})

target_link_libraries(NovaEngineCore PUBLIC glfw glad glm)

add_executable(NovaEngine "main.cpp" "render/opengl/camera.h")
target_compile_options(NovaEngine PRIVATE "/utf-8")
target_link_libraries(NovaEngine PUBLIC NovaEngineCore)



#拷贝着色器文件
set(SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Render/OpenGL/shaders")

add_custom_command(TARGET NovaEngine POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	"${SHADER_SRC_DIR}"
	"$<TARGET_FILE_DIR:NovaEngine>/shaders"
	COMMENT "Copying shaders to executable output directory"
)

add_custom_command(TARGET NovaEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/assets"          # 源文件夹（根据实际资源位置调整）
    "$<TARGET_FILE_DIR:NovaEngine>/assets" # 目标：可执行文件所在目录下的 assets 子文件夹
    COMMENT "Copying assets to executable output directory"
)
