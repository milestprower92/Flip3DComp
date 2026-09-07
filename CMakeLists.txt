cmake_minimum_required(VERSION 3.21)
project(Flip3DComp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Flip3DComp WIN32
    src/main.cpp
    src/Flip3DComp.cpp
    src/Flip3DComp_Window.cpp
    src/Flip3DComp_Cards.cpp
    src/Flip3DComp_Composition.cpp
    src/Flip3DComp_Update.cpp
    src/Flip3DComp_Rotation.cpp
    src/Flip3DComp_Input.cpp
    src/Flip3DComp_View.cpp
    src/Flip3DComp_ShellHook.cpp
    src/Flip3DComp_Accessible.cpp
    src/Flip3DAccessible.cpp
    src/Timeline.cpp
)

set_target_properties(Flip3DComp PROPERTIES
    OUTPUT_NAME "Flip3D"
)

target_include_directories(Flip3DComp PRIVATE src)
target_link_libraries(Flip3DComp
    dwmapi
    dcomp
    dxgi
    d3d11         # DComp interop surface creation — no D3D rendering
    user32
    gdi32
    advapi32
    oleacc
)
