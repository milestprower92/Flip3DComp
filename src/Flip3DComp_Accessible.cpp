// ============================================================================
// Flip3DComp_Accessible.cpp — Accessibility helpers + NotifyWinEvent (uDWM port)
// ============================================================================
#include "Flip3DComp.h"
#include "Flip3DAccessible.h"

#include <algorithm>
#include <cmath>

// ============================================================================
// Flip3DCompApp::InitAccessibility
// ============================================================================
bool Flip3DCompApp::InitAccessibility()
{
    if (m_comInitialized)
        return true;

    const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (hr == RPC_E_CHANGED_MODE)
    {
        // Another module initialized COM with a different model; still usable.
        m_comInitialized = true;
        return true;
    }

    if (FAILED(hr))
        return false;

    m_comInitialized    = true;
    m_comNeedsUninit    = true;
    return true;
}

void Flip3DCompApp::ShutdownAccessibility()
{
    if (m_pAccessible)
    {
        m_pAccessible->Release();
        m_pAccessible = nullptr;
    }

    if (m_comNeedsUninit)
    {
        CoUninitialize();
        m_comNeedsUninit = false;
    }
    m_comInitialized = false;
}

IAccessible* Flip3DCompApp::GetAccessibleObject()
{
    if (!m_pAccessible)
    {
        IAccessible* pAcc = nullptr;
        if (FAILED(Flip3DAccessible::Create(this, &pAcc)))
            return nullptr;
        m_pAccessible = pAcc;
    }
    return m_pAccessible;
}

// ============================================================================
// Flip3DCompApp::NotifyAccessibilityEvent
// uDWM: 0x14 dialog start, 0x15 dialog end, 0x8005 focus on front child.
// ============================================================================
void Flip3DCompApp::NotifyAccessibilityEvent(DWORD event, LONG childId)
{
    if (!m_hwnd)
        return;

    NotifyWinEvent(event, m_hwnd, OBJID_CLIENT, childId);
}

void Flip3DCompApp::NotifyAccessibilityFocusFront()
{
    NotifyAccessibilityEvent(EVENT_OBJECT_FOCUS, 1);
}

// ============================================================================
// Flip3DCompApp::AccessibleChildCount
// uDWM CFlip3DAccessible::GetChildrenCount — front→back inclusive stack.
// ============================================================================
int Flip3DCompApp::AccessibleChildCount() const
{
    return (int)m_cards.size();
}

// ============================================================================
// Flip3DCompApp::AccessibleWindowName
// ============================================================================
HRESULT Flip3DCompApp::AccessibleWindowName(int index, BSTR* pszName) const
{
    if (!pszName)
        return E_POINTER;

    *pszName = nullptr;

    if (index < 0 || index >= (int)m_cards.size())
        return E_INVALIDARG;

    const CardModel& card = m_cards[(size_t)index];
    wchar_t title[512] = {};
    if (card.m_hwnd && IsWindow(card.m_hwnd))
        GetWindowTextW(card.m_hwnd, title, (int)std::size(title));

    *pszName = SysAllocString(title);
    return *pszName ? S_OK : E_OUTOFMEMORY;
}

// ============================================================================
// Flip3DCompApp::AccessibleCardScreenRect
// Project the live 3D card quad to screen pixels (uDWM GetFlip3DWindowBoundingBox).
// ============================================================================
bool Flip3DCompApp::AccessibleCardScreenRect(int index, long* pxLeft, long* pyTop,
                                             long* pcxWidth, long* pcyHeight) const
{
    if (!pxLeft || !pyTop || !pcxWidth || !pcyHeight
        || index < 0 || index >= (int)m_cards.size())
        return false;

    const CardModel& c = m_cards[(size_t)index];

    const float p         = EnterProgress();
    const auto  camera    = BuildCameraMatrix(p);
    const float carouselSlot = GetCardDisplaySlot(index);

    const float t        = ComputeCarouselBezierT(carouselSlot);
    const float flatRank = ComputeFlatDepthRank(carouselSlot, p, index);
    const auto  model    = BuildModelMatrix(c, t, p, flatRank);
    const auto  mvp      = Math::Multiply(model, camera);

    const float sw = (float)std::max(c.m_srcWidth,  1);
    const float sh = (float)std::max(c.m_srcHeight, 1);

    auto project = [&](float px, float py) -> Vec2
    {
        const float x = px * mvp.m[0][0] + py * mvp.m[1][0] + mvp.m[3][0];
        const float y = px * mvp.m[0][1] + py * mvp.m[1][1] + mvp.m[3][1];
        float       w = px * mvp.m[0][3] + py * mvp.m[1][3] + mvp.m[3][3];
        if (std::fabs(w) < 1e-6f)
            w = 1e-6f;
        return { x / w, y / w };
    };

    const Vec2 corners[4] = {
        project(0.0f, 0.0f),
        project(sw,    0.0f),
        project(sw,    sh),
        project(0.0f,  sh),
    };

    float minX =  1e10f, minY =  1e10f;
    float maxX = -1e10f, maxY = -1e10f;
    for (const Vec2& v : corners)
    {
        minX = std::min(minX, v.x);
        minY = std::min(minY, v.y);
        maxX = std::max(maxX, v.x);
        maxY = std::max(maxY, v.y);
    }

    POINT origin = { 0, 0 };
    ClientToScreen(m_hwnd, &origin);

    *pxLeft   = origin.x + (long)std::floor(minX);
    *pyTop    = origin.y + (long)std::floor(minY);
    *pcxWidth  = (long)std::ceil(maxX - minX);
    *pcyHeight = (long)std::ceil(maxY - minY);
    if (*pcxWidth < 0)  *pcxWidth  = 0;
    if (*pcyHeight < 0) *pcyHeight = 0;
    return true;
}

// ============================================================================
// Flip3DCompApp::AccessibleHitTest
// Screen coordinates → carousel list index, or -1 if no card.
// ============================================================================
int Flip3DCompApp::AccessibleHitTest(long screenX, long screenY) const
{
    POINT pt = { (int)screenX, (int)screenY };
    ScreenToClient(m_hwnd, &pt);

    HWND hit = HitTest3DScene((LONG)pt.x, (LONG)pt.y);
    if (!hit)
        return -1;

    // uDWM: child id = DistanceBetween(front, hit, false); list index matches.
    return FindCardIndex(hit);
}

// ============================================================================
// Flip3DCompApp::AccessiblePointInView
// ============================================================================
bool Flip3DCompApp::AccessiblePointInView(POINT screenPt) const
{
    RECT rc = {};
    if (!GetWindowRect(m_hwnd, &rc))
        return false;

    return PtInRect(&rc, screenPt) != FALSE;
}

// ============================================================================
// Flip3DCompApp::AccessibleRotateToIndex
// accSelect → smooth scroll the window to the carousel front.
// ============================================================================
HRESULT Flip3DCompApp::AccessibleRotateToIndex(int index)
{
    if (index < 0 || index >= (int)m_cards.size())
        return E_INVALIDARG;

    if (m_state == ViewState::Exit || m_state == ViewState::ExitRepeatedRotate)
        return DISP_E_MEMBERNOTFOUND;

    RotateToWindow(m_cards[(size_t)index].m_hwnd);
    return S_OK;
}

// ============================================================================
// Flip3DCompApp::AccessibleSelectIndex
// uDWM accDoDefaultAction → SelectWindow.
// ============================================================================
HRESULT Flip3DCompApp::AccessibleSelectIndex(int index)
{
    if (index < 0 || index >= (int)m_cards.size())
        return E_INVALIDARG;

    SelectWindow(m_cards[(size_t)index].m_hwnd);
    return S_OK;
}
