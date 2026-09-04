// ============================================================================
// Flip3DComp_Input.cpp — Keyboard, mouse, and wheel input handling
// ============================================================================
#include "Flip3DComp.h"
#include <cmath>

// ============================================================================
// Flip3DCompApp::OnWheel
// Modern smooth scroll: each WHEEL_DELTA notch nudges the scroll target by one
// slot. Wheel down (delta < 0) scrolls front→back; wheel up scrolls back→front.
// ============================================================================
bool Flip3DCompApp::OnWheel(int wheelDelta)
{
    if (wheelDelta == 0 ||
        m_state == ViewState::Exit ||
        m_state == ViewState::ExitRepeatedRotate)
        return false;

    const float scaled = (float)wheelDelta / (float)WHEEL_DELTA * kScrollWheelNotchFraction;
    int deltaSlots = (int)std::round(scaled);
    if (deltaSlots == 0 && wheelDelta != 0)
        deltaSlots = (wheelDelta > 0) ? 1 : -1;
    m_wheelPendingSlots += deltaSlots;
    m_lastWheelTime = std::chrono::steady_clock::now();
    return true;
}

// ============================================================================
// Flip3DCompApp::OnKey
// ============================================================================
bool Flip3DCompApp::OnKey(bool down, UINT vkCode, LPARAM lParam)
{
    if (!down)
    {
        if (vkCode == m_heldNavigationKey)
        {
            m_heldNavigationKey = 0;
            m_heldNavigationDirection = 0;
            m_heldNavigationStart = {};
            m_scrollTarget = std::round(m_scrollTarget);
        }
        return false;
    }

    if (m_state == ViewState::Exit ||
        m_state == ViewState::ExitRepeatedRotate)
        return false;

    // Detect Windows autorepeat: bit 30 of lParam is set if the key was
    // previously down. Throttle repeated keydown processing so holding an
    // arrow or tab key doesn't rotate the carousel too quickly.
    const bool isRepeat = (lParam & (1 << 30)) != 0;
    if (isRepeat)
        return true; // held navigation is driven from the frame update

    switch (vkCode)
    {
    case VK_ESCAPE:
        ExitView();
        return true;

    case VK_TAB:
    {
        const int direction = (GetAsyncKeyState(VK_SHIFT) & 0x8000) ? -1 : 1;
        m_heldNavigationKey = vkCode;
        m_heldNavigationDirection = direction;
        m_heldNavigationStart = std::chrono::steady_clock::now();
        RotateBy(direction);
        return true;
    }

    case VK_UP:
        m_heldNavigationKey = vkCode;
        m_heldNavigationDirection = -1;
        m_heldNavigationStart = std::chrono::steady_clock::now();
        RotateBy(-1);
        return true;

    case VK_DOWN:
        m_heldNavigationKey = vkCode;
        m_heldNavigationDirection = 1;
        m_heldNavigationStart = std::chrono::steady_clock::now();
        RotateBy(1);
        return true;

    case VK_LEFT:
        m_heldNavigationKey = vkCode;
        m_heldNavigationDirection = m_rtl ? 1 : -1;
        m_heldNavigationStart = std::chrono::steady_clock::now();
        RotateBy(m_rtl ? 1 : -1);
        return true;

    case VK_RIGHT:
        m_heldNavigationKey = vkCode;
        m_heldNavigationDirection = m_rtl ? -1 : 1;
        m_heldNavigationStart = std::chrono::steady_clock::now();
        RotateBy(m_rtl ? -1 : 1);
        return true;

    case VK_HOME:
        RotateToWindow(m_originalFrontHwnd);
        return true;

    case VK_F5:
        ReplayEnterAnimation();
        return true;

    case VK_RETURN:
    case VK_SPACE:
        SelectFront();
        return true;
    }

    return false;
}

// ============================================================================
// Flip3DCompApp::OnMouse
// ============================================================================
bool Flip3DCompApp::OnMouse(LONG x, LONG y, bool pressed)
{
    if (!pressed ||
        m_state == ViewState::Exit ||
        m_state == ViewState::ExitRepeatedRotate)
        return false;

    HWND hit = HitTest3DScene(x, y);
    if (hit)
        SelectWindow(hit);
    else
        ExitView();

    return true;
}
