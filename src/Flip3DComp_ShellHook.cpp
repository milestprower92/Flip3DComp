        // ============================================================================
// Flip3DComp_ShellHook.cpp — dynamic card list via RegisterShellHookWindow
// ============================================================================
#include "Flip3DComp.h"

#include <algorithm>

// ============================================================================
// Flip3DCompApp::QualifiesForView
// ============================================================================
bool Flip3DCompApp::QualifiesForView(HWND hwnd) const
{
    if (!hwnd || hwnd == m_hwnd || hwnd == GetDesktopWindow())
        return false;

    if (!IsWindowVisible(hwnd))
        return false;

    if (m_selectedHwnd && hwnd == m_selectedHwnd)
        return true;

    const LONG_PTR style   = GetWindowLongPtrW(hwnd, GWL_STYLE);
    const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);

    if ((style & WS_CHILD) != 0)
        return false;

    const bool isShellDesktop = (hwnd == GetShellWindow());

    if (!isShellDesktop
        && (exStyle & 0x08000080) != 0
        && (exStyle & WS_EX_APPWINDOW) == 0)
        return false;

    if (!isShellDesktop && (exStyle & WS_EX_TOOLWINDOW) != 0)
        return false;

    if (!isShellDesktop
        && (exStyle & WS_EX_APPWINDOW) == 0
        && GetWindow(hwnd, GW_OWNER) != nullptr)
        return false;

    if (IsNeverHiddenWindow(hwnd))
        return false;

    if (!isShellDesktop)
    {
        DWORD cloaked = 0;
        if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))
            && cloaked != 0)
            return false;
    }

    return true;
}

// ============================================================================
bool Flip3DCompApp::IsFlip3DViewActive() const
{
    return m_state != ViewState::Inactive;
}

bool Flip3DCompApp::IsNeverHiddenWindow(HWND hwnd) const
{
    if (!hwnd)
        return true;

    if (hwnd == m_hwnd)
        return true;

    wchar_t cls[64] = {};
    if (!GetClassNameW(hwnd, cls, 63))
        return false;

    return !_wcsicmp(cls, L"Shell_TrayWnd")
        || !_wcsicmp(cls, L"Shell_SecondaryTrayWnd")
        || !_wcsicmp(cls, L"WorkerW");
}

// ============================================================================
void Flip3DCompApp::EnterFlip3DWindowMode()
{
    if (!m_hwnd || m_shellHookRegistered)
        return;

    if (!m_wmShellHook)
        m_wmShellHook = RegisterWindowMessageW(L"SHELLHOOK");

    if (RegisterShellHookWindow(m_hwnd))
        m_shellHookRegistered = true;
}

void Flip3DCompApp::LeaveFlip3DWindowMode()
{
    if (m_hwnd && m_shellHookRegistered)
    {
        DeregisterShellHookWindow(m_hwnd);
        m_shellHookRegistered = false;
    }
}

// ============================================================================
void Flip3DCompApp::OnShellHookMessage(WPARAM wParam, LPARAM lParam)
{
    if (!IsFlip3DViewActive())
        return;

    const UINT code = (UINT)(wParam & ~HSHELL_HIGHBIT);
    const HWND hwnd = (HWND)lParam;

    switch (code)
    {
    case HSHELL_WINDOWCREATED:
    case HSHELL_WINDOWACTIVATED:
    case HSHELL_WINDOWREPLACED:
    case HSHELL_RUDEAPPACTIVATED:
        if (hwnd)
            OnWindowShowHide(hwnd);
        break;

    case HSHELL_WINDOWDESTROYED:
        if (hwnd)
        {
            const int idx = FindCardIndex(hwnd);
            if (idx >= 0)
                RemoveCardAt((size_t)idx);
        }
        break;

    default:
        break;
    }
}

// ============================================================================
void Flip3DCompApp::OnWindowShowHide(HWND hwnd)
{
    if (!IsFlip3DViewActive() || !hwnd || !IsWindow(hwnd))
        return;

    if (hwnd == m_hwnd || IsNeverHiddenWindow(hwnd))
        return;

    const bool qualifies = QualifiesForView(hwnd);
    const int  cardIdx   = FindCardIndex(hwnd);

    if (qualifies)
    {
        if (cardIdx < 0)
            AddCardForWindow(hwnd);
    }
    else if (cardIdx >= 0)
    {
        RemoveCardAt((size_t)cardIdx);
    }
}

// ============================================================================
int Flip3DCompApp::FindCardIndex(HWND hwnd) const
{
    if (!hwnd)
        return -1;

    for (int i = 0; i < (int)m_cards.size(); ++i)
    {
        if (m_cards[(size_t)i].m_hwnd == hwnd)
            return i;
    }
    return -1;
}

// ============================================================================
HRESULT Flip3DCompApp::CreateCardVisual(CardModel& card)
{
    if (!m_dcompDevice || !m_sceneVisual || !card.m_hwnd)
        return E_INVALIDARG;

    DWM_THUMBNAIL_PROPERTIES tp = {};
    tp.dwFlags   = DWM_TNP_VISIBLE | DWM_TNP_RECTDESTINATION
                 | DWM_TNP_ENABLE3D | DWM_TNP_DISABLEFORCECVI;
    tp.fVisible  = TRUE;
    tp.rcDestination = { 0, 0, card.m_srcWidth, card.m_srcHeight };

    void* pv = nullptr;
    HRESULT hr = m_pfnCreateSharedThumbVisual(
        m_hwnd,
        card.m_hwnd,
        DWM_TNF_DWMWINDOW,
        &tp,
        m_dcompDevice.Get(),
        &pv,
        &card.m_hThumb);

    if (FAILED(hr) || !pv)
        return FAILED(hr) ? hr : E_FAIL;

    ComPtr<IDCompositionVisual> thumbBase;
    thumbBase.Attach((IDCompositionVisual*)pv);
    hr = thumbBase.As(&card.m_visual);
    if (FAILED(hr))
        return hr;

    ComPtr<IDCompositionVisual2> container;
    hr = m_dcompDevice->CreateVisual(&container);
    if (FAILED(hr))
        return hr;

    hr = container->AddVisual(card.m_visual.Get(), FALSE, nullptr);
    if (FAILED(hr))
        return hr;

    hr = container.As(&card.m_containerVisual);
    if (FAILED(hr))
        return hr;

    hr = m_sceneVisual->AddVisual(container.Get(), TRUE, nullptr);
    return hr;
}

// ============================================================================
bool Flip3DCompApp::AddCardForWindow(HWND hwnd)
{
    if (!hwnd || m_cards.size() >= (size_t)kMaxCards)
        return false;

    if (FindCardIndex(hwnd) >= 0)
        return false;

    CardModel card;
    card.m_hwnd                 = hwnd;
    card.m_initialCarouselIndex = (int)m_cards.size();
    UpdateCardGeometry(card, m_monW, m_monH);

    if (FAILED(CreateCardVisual(card)))
        return false;

    m_cards.push_back(std::move(card));

    if (m_dcompDevice)
        m_dcompDevice->Commit();

    return true;
}

// ============================================================================
void Flip3DCompApp::RemoveCardAt(size_t index)
{
    if (index >= m_cards.size())
        return;

    CardModel& card = m_cards[index];

    if (card.m_containerVisual && m_sceneVisual)
    {
        ComPtr<IDCompositionVisual> sceneBase;
        if (SUCCEEDED(m_sceneVisual.As(&sceneBase)))
            sceneBase->RemoveVisual(card.m_containerVisual.Get());
    }

    if (card.m_hThumb)
    {
        DwmUnregisterThumbnail(card.m_hThumb);
        card.m_hThumb = nullptr;
    }

    card.m_visual.Reset();
    card.m_containerVisual.Reset();

    m_cards.erase(m_cards.begin() + (ptrdiff_t)index);

    if (m_dcompDevice)
        m_dcompDevice->Commit();
}
