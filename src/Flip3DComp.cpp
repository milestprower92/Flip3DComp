// ============================================================================
// Flip3DComp.cpp — Application lifecycle: Initialize, Run, WndProc, HandleMessage
// ============================================================================
#include "Flip3DComp.h"
#include "Flip3DAccessible.h"

#include <windowsx.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

// ============================================================================
// Flip3DCompApp::Initialize
// ============================================================================
bool Flip3DCompApp::Initialize(HINSTANCE hInstance)
{
    m_hInstance = hInstance;

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    if (!LoadThumbApi())
        return false;

    BuildCards();

    if (!CreateAppWindow())
    {
        if (m_initError.empty())
            m_initError = L"Failed to create the Flip3D input window.";
        return false;
    }

    UpdateMonitorRect();

    if (FAILED(InitComposition()))
    {
        if (m_initError.empty())
            m_initError = L"Failed to initialize DirectComposition.";
        return false;
    }

    if (FAILED(CreateCardVisuals()))
    {
        if (m_initError.empty())
            m_initError = L"Failed to create DWM thumbnail visuals.";
        return false;
    }

    if (FAILED(CreateShellBackdrop()))
    {
        if (m_initError.empty())
            m_initError = L"Failed to create the shell desktop backdrop.";
        return false;
    }
    
    m_state = ViewState::Enter;
    m_animEnter.Restart(0.0f, 1.0f, kEnterExitDurationSec,
                        kEnableAnimationEasing
                            ? InterpolationMode::CubicBezier
                            : InterpolationMode::Linear);
    m_prevFrame = std::chrono::steady_clock::now();
    m_openingTabPending = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;
    m_openingTabStart = m_openingTabPending ? m_prevFrame
                                            : std::chrono::steady_clock::time_point{};

    EnterFlip3DWindowMode();
    InitAccessibility();

    // First-frame layout + DComp commit before ShowWindow (avoids blank flash).
    Update(0.0f);

    return true;
}

// ============================================================================
// Flip3DCompApp::Run
// ============================================================================
int Flip3DCompApp::Run()
{
    MSG msg = {};

    while (msg.message != WM_QUIT)
    {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (msg.message == WM_QUIT)
            break;

        if (!m_minimized)
        {
            auto now = std::chrono::steady_clock::now();
            float dt = std::chrono::duration<float>(now - m_prevFrame).count();
            m_prevFrame = now;
            Update(std::min(dt, 0.05f));
        }
        else
        {
            WaitMessage();
            m_prevFrame = std::chrono::steady_clock::now();
        }
    }

    UnloadThumbApi();
    return (int)msg.wParam;
}

// ============================================================================
// Flip3DCompApp::WndProc — static window procedure
// ============================================================================
LRESULT CALLBACK Flip3DCompApp::WndProc(HWND hwnd, UINT msg,
                                         WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCREATE)
    {
        auto* self = (Flip3DCompApp*)((CREATESTRUCTW*)lParam)->lpCreateParams;
        if (self)
        {
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
            self->m_hwnd = hwnd;
        }
    }

    auto* self = (Flip3DCompApp*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    return self
        ? self->HandleMessage(msg, wParam, lParam)
        : DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ============================================================================
// Flip3DCompApp::HandleMessage
// ============================================================================
LRESULT Flip3DCompApp::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
        {
            m_minimized = true;
            return 0;
        }
        m_minimized = false;
        m_width     = std::max(1u, (UINT)LOWORD(lParam));
        m_height    = std::max(1u, (UINT)HIWORD(lParam));
        UpdateMonitorRect();
        return 0;

    case WM_DISPLAYCHANGE:
        ApplyFullscreenLayout();
        UpdateMonitorRect();
        return 0;

    case WM_DWMTHUMBNAILSOURCESIZECHANGED:
        // Each registered thumbnail posts 0x327 independently; coalesce to one
        // refresh per frame in Update() (wParam = adapter LUID low part).
        m_thumbnailsDirty = true;
        return 0;

    case WM_MOUSEWHEEL:
        OnWheel(GET_WHEEL_DELTA_WPARAM(wParam));
        return 0;

    case WM_MOUSEMOVE:
        m_hitHwnd = HitTest3DScene(
            (LONG)(short)LOWORD(lParam),
            (LONG)(short)HIWORD(lParam));
        SetCursor(LoadCursorW(nullptr, m_hitHwnd ? IDC_HAND : IDC_ARROW));
        return 0;

    case WM_LBUTTONDOWN:
        OnMouse((LONG)(short)LOWORD(lParam),
                (LONG)(short)HIWORD(lParam), true);
        return 0;

    case WM_KEYDOWN:
        if (OnKey(true, (UINT)wParam, lParam))
            return 0;
        break;

    case WM_KEYUP:
        if (OnKey(false, (UINT)wParam, lParam))
            return 0;
        break;

    case WM_CLOSE:
        if (m_state == ViewState::Exit ||
            m_state == ViewState::ExitRepeatedRotate)
        {
            DestroyWindow(m_hwnd);
        }
        else
        {
            ExitView();
        }
        return 0;

    case WM_GETOBJECT:
    {
        if ((DWORD)lParam != OBJID_CLIENT && (DWORD)lParam != 0)
            break;

        IAccessible* pAccessible = GetAccessibleObject();
        if (!pAccessible)
            break;

        return LresultFromObject(IID_IAccessible, wParam, pAccessible);
    }

    case WM_DESTROY:
        ShutdownAccessibility();
        LeaveFlip3DWindowMode();
        PostQuitMessage(0);
        return 0;
    }

    if (m_wmShellHook && msg == m_wmShellHook)
    {
        OnShellHookMessage(wParam, lParam);
        return 0;
    }

    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}
