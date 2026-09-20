cmake_minimum_required(VERSION 3.21)
project(Flip3DComp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FLIP3D_SOURCES
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

function(configure_flip3d_target target easing)
    target_include_directories(${target} PRIVATE src)
    target_compile_definitions(${target} PRIVATE
        FLIP3D_ENABLE_ANIMATION_EASING=${easing}
    )
    target_link_libraries(${target}
        dwmapi
        dcomp
        dxgi
        d3d11         # DComp interop surface creation — no D3D rendering
        user32
        gdi32
        windowscodecs
        advapi32
        oleacc
    )
endfunction()

add_executable(Flip3D WIN32 ${FLIP3D_SOURCES})
configure_flip3d_target(Flip3D 0)

add_executable(Flip3DComp WIN32 ${FLIP3D_SOURCES})
configure_flip3d_target(Flip3DComp 1)
