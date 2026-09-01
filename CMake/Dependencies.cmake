set(NE_THIRD_PARTY_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/Linking/include)
set(NE_THIRD_PARTY_LIB_DIR ${CMAKE_SOURCE_DIR}/Linking/lib)
set(NE_THIRD_PARTY_SRC_DIR ${CMAKE_SOURCE_DIR}/Linking/src)

#glfw 预编译库
add_library(glfw STATIC IMPORTED)

set_target_properties(glfw PROPERTIES
        IMPORTED_LOCATION "${NE_THIRD_PARTY_LIB_DIR}/GLFW/glfw3.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${NE_THIRD_PARTY_INCLUDE_DIR}")  


#glad
add_library(glad STATIC
    ${NE_THIRD_PARTY_SRC_DIR}/glad/glad.c
)
target_include_directories(glad PRIVATE ${NE_THIRD_PARTY_INCLUDE_DIR})
set_target_properties(glad PROPERTIES LINKER_LANGUAGE C)


#glm库  仅头文件
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE
    ${NE_THIRD_PARTY_INCLUDE_DIR}
)

#imgui
file(GLOB IMGUI_SOURCES
    ${NE_THIRD_PARTY_SRC_DIR}/imgui/*.cpp
    ${NE_THIRD_PARTY_SRC_DIR}/imgui/backends/*.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})

target_include_directories(imgui PUBLIC 
    ${NE_THIRD_PARTY_INCLUDE_DIR}/imgui
    ${NE_THIRD_PARTY_INCLUDE_DIR}/imgui/backends
)

target_link_libraries(imgui PUBLIC glfw glad)