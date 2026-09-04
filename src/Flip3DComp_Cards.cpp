// ============================================================================
// Flip3DComp_Cards.cpp — Card building + thumbnail visual creation + DWM API
// ============================================================================
#include "Flip3DComp.h"

namespace {

MONITORINFO QueryPrimaryMonitor()
{
    MONITORINFO mi = { sizeof(mi) };
    HMONITOR hPrimary = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
    if (hPrimary)
        GetMonitorInfoW(hPrimary, &mi);
    return mi;
}

// 2D screen anchor for non-minimized-tile layouts (extended frame or restore rect).
bool FillRestoredScreenRect(HWND h, const MONITORINFO& mi, RECT& out)
{
    bool maximized = IsZoomed(h);

    if (IsIconic(h))
    {
        WINDOWPLACEMENT wp = { sizeof(wp) };
        if (GetWindowPlacement(h, &wp) && !IsRectEmpty(&wp.rcNormalPosition))
        {
            out = wp.rcNormalPosition;
            OffsetRect(&out, mi.rcWork.left, mi.rcWork.top);
            if (wp.flags & WPF_RESTORETOMAXIMIZED)
            {
                maximized = true;
                out = mi.rcWork;
            }
            goto leave;
        }
    }

    DwmGetWindowAttribute(h, DWMWA_EXTENDED_FRAME_BOUNDS, &out, sizeof(out));
leave:
    if (maximized)
    {
        OffsetRect(&out, mi.rcWork.left - out.left, mi.rcWork.top - out.top);
    }

    return true;
}

} // namespace

// ============================================================================
// Flip3DCompApp::LoadThumbApi
// ============================================================================
bool Flip3DCompApp::LoadThumbApi()
{
    m_initError.clear();

    m_dwmapi = LoadLibraryW(L"dwmapi.dll");
    if (!m_dwmapi)
    {
        m_initError = L"Failed to load dwmapi.dll.";
        return false;
    }

    m_pfnCreateSharedThumbVisual = (DwmpCreateSharedThumbnailVisual_fn)
        GetProcAddress(m_dwmapi, MAKEINTRESOURCEA(147));
    m_pfnQueryThumbSize = (DwmpQueryWindowThumbnailSourceSize_fn)
        GetProcAddress(m_dwmapi, MAKEINTRESOURCEA(162));
    m_pfnGetWindowMinimizeRect = (GetWindowMinimizeRect_fn)
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetWindowMinimizeRect");

    if (!m_pfnCreateSharedThumbVisual)
    {
        m_initError = L"DwmpCreateSharedThumbnailVisual (dwmapi ord 147) is required.";
        UnloadThumbApi();
        return false;
    }
    if (!m_pfnQueryThumbSize)
    {
        m_initError = L"DwmpQueryWindowThumbnailSourceSize (dwmapi ord 162) is required.";
        UnloadThumbApi();
        return false;
    }
    if (!m_pfnGetWindowMinimizeRect)
    {
        m_initError = L"GetWindowMinimizeRect (user32) is required.";
        UnloadThumbApi();
        return false;
    }

    return true;
}

// ============================================================================
// Flip3DCompApp::UnloadThumbApi
// ============================================================================
void Flip3DCompApp::UnloadThumbApi()
{
    m_pfnCreateSharedThumbVisual = nullptr;
    m_pfnQueryThumbSize          = nullptr;
    m_pfnGetWindowMinimizeRect     = nullptr;

    if (m_dwmapi)
    {
        FreeLibrary(m_dwmapi);
        m_dwmapi = nullptr;
    }
}

// ============================================================================
// Flip3DCompApp::UpdateCardGeometry
// uDWM Flip3DWindow::OnOriginalRectUpdated:
//   - flatBounds in screen pixels (per-window monitor for taskbar/minimize)
//   - NormalizeWindowSize + world mapping via shared PRIMARY rcWork (normMon*)
//   - GetMonitorToWorldTransform on primary m_rcMonitor for all cards
// ============================================================================
void Flip3DCompApp::UpdateCardGeometry(CardModel& c, float normMonW, float normMonH,
                                       bool selectedRestore)
{
    HWND h = c.m_hwnd;
    if (!h)
        return;

    normMonW = std::max(normMonW, 1.0f);
    normMonH = std::max(normMonH, 1.0f);

    c.m_isMinimized    = !!IsIconic(h) && !selectedRestore;
    c.m_isShellDesktop = (h == GetShellWindow());

    HMONITOR mon = MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST);
    if (!mon)
        mon = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);

    MONITORINFO mi = { sizeof(mi) };
    if (mon)
        GetMonitorInfoW(mon, &mi);
    else
        mi = QueryPrimaryMonitor();

    SIZE srcSize = {};
    if (FAILED(m_pfnQueryThumbSize(h, FALSE, &srcSize))
        || srcSize.cx < 1 || srcSize.cy < 1)
        return;

    float thumbW = (float)srcSize.cx;
    float thumbH = (float)srcSize.cy;
    const float thumbAspect = thumbH / thumbW;

    RECT flatBounds = {};

    if (c.m_isShellDesktop)
    {
        // uDWM shell: relative origin {0,0} on primary - use primary rcWork.
        MONITORINFO primaryMi = QueryPrimaryMonitor();
        flatBounds = primaryMi.rcWork;
    }
    else if (c.m_isMinimized)
    {
        // 2D minimize destination: taskbar tile position only.
        RECT minRect = {};
        if (m_pfnGetWindowMinimizeRect(h, &minRect) && !IsRectEmpty(&minRect))
            flatBounds = Math::BuildFinalMinRect(minRect, thumbAspect);
    }
    else if (!FillRestoredScreenRect(h, mi, flatBounds))
    {
        flatBounds = mi.rcWork;
    }

    if (IsRectEmpty(&flatBounds))
        flatBounds = mi.rcWork;

    c.m_srcWidth  = (int)thumbW;
    c.m_srcHeight = (int)thumbH;

    // targetSize / occupancy = 3D carousel (uDWM finalSize).
    Math::WorldSizesFromThumbPixels(
        thumbW, thumbH, normMonW, normMonH,
        c.m_flatSize, c.m_targetSize, c.m_occupancy);

    c.m_aspectRatio = thumbW / thumbH;

    // 2D flat: position from flatBounds; size from QueryThumbSize (restored pixels).
    // Exceptions: shell uses rcWork; iconic minimize uses taskbar tile dimensions.
    float flatW = thumbW;
    float flatH = thumbH;
    if (c.m_isShellDesktop || c.m_isMinimized)
    {
        flatW = (float)std::max(1L, flatBounds.right  - flatBounds.left);
        flatH = (float)std::max(1L, flatBounds.bottom - flatBounds.top);
    }

    c.m_flatSize = { flatW / normMonW, flatH / normMonH };

    float anchorX = (float)flatBounds.left;
    float anchorY = (float)flatBounds.top;
    if (m_rtl)
    {
        const float relX = anchorX - m_monOriginX;
        anchorX = m_monOriginX + (normMonW - (flatW + relX));
    }

    float worldX = 0.0f;
    float worldY = 0.0f;
    Math::MonitorToWorldTopLeft(
        anchorX, anchorY,
        m_monOriginX, m_monOriginY, normMonW, normMonH,
        worldX, worldY);

    c.m_originalPos = { worldX, worldY, 0.0f };
    c.m_flatPos     = { worldX, worldY, 0.0f };
}

// ============================================================================
// Flip3DCompApp::UpdateMonitorRect
// uDWM UpdateMonitorRect: normMon from primary rcWork; SetSize uses work-area
// pixels. The input window covers the virtual desktop, but the 3D viewport must
// match rcWork (size + origin), not the full client rect.
// ============================================================================
void Flip3DCompApp::UpdateMonitorRect()
{
    HMONITOR hMon = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
    if (!hMon)
        return;

    MONITORINFO mi = { sizeof(mi) };
    if (!GetMonitorInfoW(hMon, &mi))
        return;

    const int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    m_viewX = (float)(mi.rcWork.left - vx);
    m_viewY = (float)(mi.rcWork.top  - vy);

    const float newMonW     = (float)std::max(1L, mi.rcWork.right  - mi.rcWork.left);
    const float newMonH     = (float)std::max(1L, mi.rcWork.bottom - mi.rcWork.top);
    const float newOriginX  = (float)mi.rcWork.left;
    const float newOriginY  = (float)mi.rcWork.top;

    const bool layoutChanged =
        newMonW    != m_monW       ||
        newMonH    != m_monH       ||
        newOriginX != m_monOriginX ||
        newOriginY != m_monOriginY;

    m_monW       = newMonW;
    m_monH       = newMonH;
    m_monOriginX = newOriginX;
    m_monOriginY = newOriginY;

    if (layoutChanged)
    {
        for (auto& card : m_cards)
            UpdateCardGeometry(card, m_monW, m_monH);
    }

    RebuildMonitorBackdropsIfNeeded();
    UpdateBackdropLayout();
}

// ============================================================================
// Flip3DCompApp::BuildCards
// ============================================================================
void Flip3DCompApp::BuildCards()
{
    m_cards.clear();

    if (!m_d3dDevice)
    {
        D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;
        HRESULT hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            &fl, 1, D3D11_SDK_VERSION,
            &m_d3dDevice, nullptr, nullptr);

        if (FAILED(hr))
        {
            D3D11CreateDevice(
                nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                &fl, 1, D3D11_SDK_VERSION,
                &m_d3dDevice, nullptr, nullptr);
        }
    }

    auto hwnds = EnumerateWindows();

    MONITORINFO primaryMi = QueryPrimaryMonitor();
    m_monW       = (float)std::max(1L, primaryMi.rcWork.right  - primaryMi.rcWork.left);
    m_monH       = (float)std::max(1L, primaryMi.rcWork.bottom - primaryMi.rcWork.top);
    m_monOriginX = (float)primaryMi.rcWork.left;
    m_monOriginY = (float)primaryMi.rcWork.top;

    int carouselIndex = 0;
    for (auto h : hwnds)
    {
        CardModel c;
        c.m_hwnd                 = h;
        c.m_initialCarouselIndex = carouselIndex++;
        UpdateCardGeometry(c, m_monW, m_monH);
        m_cards.push_back(std::move(c));
    }

    m_originalFrontHwnd = m_cards.empty() ? nullptr : m_cards[0].m_hwnd;
}

// ============================================================================
// Flip3DCompApp::CreateCardVisuals
// ============================================================================
HRESULT Flip3DCompApp::CreateCardVisuals()
{
    if (!m_dcompDevice || !m_sceneVisual)
        return E_FAIL;

    for (auto& card : m_cards)
    {
        if (!card.m_hwnd || card.m_containerVisual)
            continue;

        if (FAILED(CreateCardVisual(card)))
            continue;
    }

    return S_OK;
}

// ============================================================================
// Flip3DCompApp::UpdateCardThumbnailDest
// Sync DWM thumbnail rcDestination to the current source pixel size.
// ============================================================================
void Flip3DCompApp::UpdateCardThumbnailDest(CardModel& card)
{
    if (!card.m_hThumb || card.m_srcWidth <= 0 || card.m_srcHeight <= 0)
        return;

    DWM_THUMBNAIL_PROPERTIES tp = {};
    tp.dwFlags   = DWM_TNP_VISIBLE | DWM_TNP_RECTDESTINATION
                    | DWM_TNP_ENABLE3D | DWM_TNP_DISABLEFORCECVI;
    tp.fVisible  = TRUE;
    tp.rcDestination   = { 0, 0, card.m_srcWidth, card.m_srcHeight };
    DwmUpdateThumbnailProperties(card.m_hThumb, &tp);
}

// ============================================================================
// Flip3DCompApp::OnThumbnailSourceSizeChanged
// uDWM CThumbnailVisual::OnSizeChanged posts WM 0x327 to hwndDestination when
// DWM_TNF_DWMWINDOW is set and the live preview bitmap changes size. Every
// card thumbnail posts independently, so the WndProc only sets m_thumbnailsDirty
// and this runs once per frame, touching cards whose queried source size differs.
// ============================================================================
void Flip3DCompApp::OnThumbnailSourceSizeChanged()
{
    m_thumbnailsDirty = false;

    bool anyChange = false;
    for (auto& card : m_cards)
    {
        if (!card.m_hwnd)
            continue;

        SIZE querySize = {};
        if (FAILED(m_pfnQueryThumbSize(card.m_hwnd, FALSE, &querySize)))
            continue;

        const int queryW = (int)std::max(0L, querySize.cx);
        const int queryH = (int)std::max(0L, querySize.cy);
        if (queryW == card.m_srcWidth && queryH == card.m_srcHeight)
            continue;

        const bool selectedRestore = card.m_hwnd == m_selectedHwnd;
        UpdateCardGeometry(card, m_monW, m_monH, selectedRestore);
        UpdateCardThumbnailDest(card);
        anyChange = true;
    }

    if (anyChange && m_dcompDevice)
        m_dcompDevice->Commit();
}
