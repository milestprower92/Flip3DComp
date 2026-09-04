// ============================================================================
// Flip3DComp_View.cpp — View state: Exit, Select, HitTest3DScene
// ============================================================================
#include "Flip3DComp.h"

#include <algorithm>
#include <cmath>

// ============================================================================
// Flip3DCompApp::ExitView
// ============================================================================
void Flip3DCompApp::ExitView(bool commitScroll, float exitDurationSec)
{
    if (m_state == ViewState::Exit || m_state == ViewState::ExitRepeatedRotate)
        return;

    if (commitScroll)
        CommitCarouselScroll();

    m_lastPaintOrder.clear();
    m_state = ViewState::Exit;
    NotifyAccessibilityEvent(EVENT_SYSTEM_DIALOGEND);
    m_animEnter.Restart(EnterProgress(), 0.0f, exitDurationSec,
                        kEnableAnimationEasing
                            ? InterpolationMode::CubicBezier
                            : InterpolationMode::Linear);
}

// ============================================================================
// Flip3DCompApp::BeginExitView — uDWM CFlip3D::BeginExitView (parallel flatten)
// ============================================================================
void Flip3DCompApp::BeginExitView()
{
    if (m_state == ViewState::Exit || m_state == ViewState::ExitRepeatedRotate)
        return;

    m_lastPaintOrder.clear();
    NotifyAccessibilityEvent(EVENT_SYSTEM_DIALOGEND);
    m_animEnter.Restart(EnterProgress(), 0.0f, kExitDurationSec,
                        kEnableAnimationEasing
                            ? InterpolationMode::CubicBezier
                            : InterpolationMode::Linear);
    m_state = ViewState::Exit;
}

// ============================================================================
// Flip3DCompApp::SelectFront
// ============================================================================
void Flip3DCompApp::SelectFront()
{
    if (m_cards.empty())
        return;

    int   bestIdx  = 0;
    float bestSlot = 1e9f;
    for (int i = 0; i < (int)m_cards.size(); ++i)
    {
        const float slot = GetCardDisplaySlot(i);
        if (slot < -0.5f)
            continue;
        if (slot < bestSlot)
        {
            bestSlot = slot;
            bestIdx  = i;
        }
    }

    SelectWindow(m_cards[(size_t)bestIdx].m_hwnd);
}

// ============================================================================
// Flip3DCompApp::SelectWindow — uDWM: BeginExitView then ExitRepeatedRotate
// ============================================================================
void Flip3DCompApp::SelectWindow(HWND hwndTarget)
{
    if (!hwndTarget || !IsWindow(hwndTarget))
        return;

    const bool isShell = (hwndTarget == GetShellWindow());

    if (isShell)
    {
        if (HWND shellTray = FindWindowW(L"Shell_TrayWnd", nullptr))
            PostMessageW(shellTray, 0x579, 1, 0);
    }
    else if (!IsWindowEnabled(hwndTarget))
    {
        SwitchToThisWindow(GetLastActivePopup(GetAncestor(hwndTarget, GA_ROOTOWNER)), TRUE);
    }
    else
    {
        SwitchToThisWindow(hwndTarget, TRUE);
    }

    const int selIdx = FindCardIndex(hwndTarget);
    if (selIdx < 0)
    {
        ExitView();
        return;
    }

    if (m_cards[(size_t)selIdx].m_isMinimized)
        UpdateCardGeometry(m_cards[(size_t)selIdx], m_monW, m_monH, /*selectedRestore=*/true);

    m_selectedHwnd = hwndTarget;
    m_lastPaintOrder.clear();

    FreezeCarouselVisuals();

    BeginExitView();

    // Re-index after wrap may have rotated the list; step count = list index of selection.
    const int selIdxAfter = FindCardIndex(hwndTarget);
    if (selIdxAfter > 0)
    {
        m_rRepeatedRotateRate = -(kExitDurationSec / (float)selIdxAfter);
        m_repeatedRotateStepsRemaining = selIdxAfter;
        m_state = ViewState::ExitRepeatedRotate;
        TickRepeatedRotate();
    }
}

// ============================================================================
// Flip3DCompApp::HitTest3DScene
// ============================================================================
HWND Flip3DCompApp::HitTest3DScene(LONG screenX, LONG screenY) const
{
    if (m_cards.empty())
        return nullptr;

    const float p    = EnterProgress();
    const auto  cam  = BuildCameraMatrix(p);

    float bestNdcZ = 1e10f;
    HWND  bestHwnd = nullptr;

    for (int ki = (int)m_cards.size() - 1; ki >= 0; --ki)
    {
        const CardModel& c = m_cards[(size_t)ki];
        if (!c.m_containerVisual)
            continue;

        const float slot = GetCardDisplaySlot(ki);
        if (slot <= -0.5f || slot >= (float)kMaxVisibleCards)
            continue;

        float t   = ComputeCarouselBezierT(slot);
        const float flatRank = ComputeFlatDepthRank(slot, p, ki);
        auto  MVP = Math::Multiply(BuildModelMatrix(c, t, p, flatRank), cam);

        float sw = (float)std::max(c.m_srcWidth,  1);
        float sh = (float)std::max(c.m_srcHeight, 1);

        auto project = [&](float px, float py) -> Vec2
        {
            float x = px*MVP.m[0][0] + py*MVP.m[1][0] + MVP.m[3][0];
            float y = px*MVP.m[0][1] + py*MVP.m[1][1] + MVP.m[3][1];
            float w = px*MVP.m[0][3] + py*MVP.m[1][3] + MVP.m[3][3];
            if (fabsf(w) < 1e-6f) w = 1e-6f;
            return { x / w, y / w };
        };

        Vec2 c0 = project(0.0f, 0.0f);
        Vec2 c1 = project(sw,    0.0f);
        Vec2 c2 = project(sw,    sh);
        Vec2 c3 = project(0.0f,  sh);

        float sx = (float)screenX;
        float sy = (float)screenY;

        auto cross = [](float x1, float y1, float x2, float y2) {
            return x1 * y2 - y1 * x2;
        };

        float d0 = cross(c1.x - c0.x, c1.y - c0.y, sx - c0.x, sy - c0.y);
        float d1 = cross(c2.x - c1.x, c2.y - c1.y, sx - c1.x, sy - c1.y);
        float d2 = cross(c3.x - c2.x, c3.y - c2.y, sx - c2.x, sy - c2.y);
        float d3 = cross(c0.x - c3.x, c0.y - c3.y, sx - c3.x, sy - c3.y);

        bool inside = (d0 >= 0 && d1 >= 0 && d2 >= 0 && d3 >= 0)
                   || (d0 <= 0 && d1 <= 0 && d2 <= 0 && d3 <= 0);

        if (!inside)
            continue;

        float pixZ = 0.0f*MVP.m[0][2] + 0.0f*MVP.m[1][2] + MVP.m[3][2];
        float pixW = 0.0f*MVP.m[0][3] + 0.0f*MVP.m[1][3] + MVP.m[3][3];
        float ndcZ = pixW != 0.0f ? pixZ / pixW : 0.0f;

        if (ndcZ < bestNdcZ)
        {
            bestNdcZ = ndcZ;
            bestHwnd = c.m_hwnd;
        }
    }

    return bestHwnd;
}
        