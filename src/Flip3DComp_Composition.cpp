// ============================================================================
// Flip3DComp_Composition.cpp — DirectComposition device init, desktop wash
// ============================================================================
#include "Flip3DComp.h"

#include <algorithm>
#include <vector>

namespace {

struct EnumMonitorsContext
{
    std::vector<MONITORINFO> monitors;
};

BOOL CALLBACK EnumMonitorsProc(HMONITOR hMon, HDC, LPRECT, LPARAM lParam)
{
    auto* ctx = reinterpret_cast<EnumMonitorsContext*>(lParam);
    MONITORINFO mi = { sizeof(mi) };
    if (GetMonitorInfoW(hMon, &mi))
        ctx->monitors.push_back(mi);
    return TRUE;
}

std::vector<MONITORINFO> EnumerateMonitors()
{
    EnumMonitorsContext ctx;
    EnumDisplayMonitors(nullptr, nullptr, EnumMonitorsProc, (LPARAM)&ctx);
    return ctx.monitors;
}

} // namespace

namespace {

HRESULT CreateSharedWashSurface(ID3D11Device* d3d,
                                IDCompositionDesktopDevice* dcomp,
                                ComPtr<IDCompositionSurface>& outSurface)
{
    if (!d3d || !dcomp)
        return E_INVALIDARG;

    ComPtr<IDCompositionSurfaceFactory> sf;
    HRESULT hr = dcomp->CreateSurfaceFactory(d3d, &sf);
    if (FAILED(hr))
        return hr;

    ComPtr<IDCompositionSurface> bg;
    hr = sf->CreateSurface(1, 1, DXGI_FORMAT_B8G8R8A8_UNORM,
                           DXGI_ALPHA_MODE_IGNORE, &bg);
    if (FAILED(hr))
        return hr;

    ComPtr<IDXGISurface> dxgiSurf;
    POINT offset = {};
    hr = bg->BeginDraw(nullptr, IID_PPV_ARGS(&dxgiSurf), &offset);
    if (SUCCEEDED(hr))
    {
        ComPtr<ID3D11Texture2D> tex;
        ComPtr<ID3D11RenderTargetView> rtv;
        if (SUCCEEDED(dxgiSurf.As(&tex)) &&
            SUCCEEDED(d3d->CreateRenderTargetView(tex.Get(), nullptr, &rtv)))
        {
            ComPtr<ID3D11DeviceContext> ctx;
            d3d->GetImmediateContext(&ctx);
            const float wash[4] = { 0.04f, 0.05f, 0.08f, 1.0f };
            ctx->ClearRenderTargetView(rtv.Get(), wash);
        }
        bg->EndDraw();
    }

    outSurface = std::move(bg);
    return outSurface ? S_OK : E_FAIL;
}

} // namespace

// ============================================================================
// Flip3DCompApp::InitComposition
// ============================================================================
HRESULT Flip3DCompApp::InitComposition()
{
    if (!m_d3dDevice)
        return E_FAIL;

    ComPtr<IDXGIDevice> dxgi;
    HRESULT hr = m_d3dDevice.As(&dxgi);
    if (FAILED(hr))
        return hr;

    hr = DCompositionCreateDevice2(dxgi.Get(), IID_PPV_ARGS(&m_dcompDevice));
    if (FAILED(hr))
        return hr;

    hr = m_dcompDevice->CreateTargetForHwnd(m_hwnd, TRUE, &m_dcompTarget);
    if (FAILED(hr))
        return hr;

    ComPtr<IDCompositionVisual2> root;
    hr = m_dcompDevice->CreateVisual(&root);
    if (FAILED(hr))
        return hr;
    m_rootVisual = root;
    m_dcompTarget->SetRoot(root.Get());

    ComPtr<IDCompositionVisual2> sceneBase;
    hr = m_dcompDevice->CreateVisual(&sceneBase);
    if (FAILED(hr))
        return hr;
    sceneBase.As(&m_sceneVisual);
    m_sceneVisual->SetDepthMode(DCOMPOSITION_DEPTH_MODE_TREE);

    ComPtr<IDCompositionVisual> rootBase;
    root.As(&rootBase);
    rootBase->AddVisual(m_sceneVisual.Get(), FALSE, nullptr);

    if (m_d3dDevice)
        CreateSharedWashSurface(m_d3dDevice.Get(), m_dcompDevice.Get(), m_washSurface);

    return m_dcompDevice->Commit();
}

// ============================================================================
// Flip3DCompApp::DestroyMonitorBackdrops
// ============================================================================
void Flip3DCompApp::DestroyMonitorBackdrops()
{
    if (!m_rootVisual)
    {
        m_monitorBackdrops.clear();
        return;
    }

    ComPtr<IDCompositionVisual> rootBase;
    if (SUCCEEDED(m_rootVisual.As(&rootBase)))
    {
        for (auto& mon : m_monitorBackdrops)
        {
            if (mon.shellContainer)
                rootBase->RemoveVisual(mon.shellContainer.Get());
            if (mon.washVisual)
                rootBase->RemoveVisual(mon.washVisual.Get());
            if (mon.hShellThumb)
            {
                DwmUnregisterThumbnail(mon.hShellThumb);
                mon.hShellThumb = nullptr;
            }
        }
    }

    m_monitorBackdrops.clear();
}

// ============================================================================
// Flip3DCompApp::UpdateBackdropLayout
// Per-monitor wash (rcMonitor) and shell thumbnail (rcWork crop).
// Client coordinates match the virtual-desktop–sized Flip3D window.
// ============================================================================
void Flip3DCompApp::UpdateBackdropLayout()
{
    const int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);

    HWND shell = GetShellWindow();
    RECT shellWnd = {};
    if (shell)
        GetWindowRect(shell, &shellWnd);

    for (auto& mon : m_monitorBackdrops)
    {
        const LONG washW = mon.rcMonitor.right - mon.rcMonitor.left;
        const LONG washH = mon.rcMonitor.bottom - mon.rcMonitor.top;
        const float washX = (float)(mon.rcMonitor.left - vx);
        const float washY = (float)(mon.rcMonitor.top  - vy);

        if (mon.washVisual)
        {
            ComPtr<IDCompositionVisual2> wash2;
            if (SUCCEEDED(mon.washVisual.As(&wash2)))
            {
                const D2D_MATRIX_3X2_F xform = {
                    (float)std::max(washW, 1L), 0.f,
                    0.f, (float)std::max(washH, 1L),
                    washX, washY,
                };
                wash2->SetTransform(xform);
            }
        }

        const LONG shellW = mon.rcWork.right - mon.rcWork.left;
        const LONG shellH = mon.rcWork.bottom - mon.rcWork.top;
        const float shellX = (float)(mon.rcWork.left - vx);
        const float shellY = (float)(mon.rcWork.top  - vy);

        if (mon.shellContainer)
        {
            ComPtr<IDCompositionVisual2> shell2;
            if (SUCCEEDED(mon.shellContainer.As(&shell2)))
            {
                const D2D_MATRIX_3X2_F xform = {
                    1.f, 0.f,
                    0.f, 1.f,
                    shellX, shellY,
                };
                shell2->SetTransform(xform);
            }
        }

        if (mon.hShellThumb && shellW > 0 && shellH > 0)
        {
            RECT rcSource = mon.rcWork;
            OffsetRect(&rcSource, -shellWnd.left, -shellWnd.top);

            DWM_THUMBNAIL_PROPERTIES tp = {};
            tp.dwFlags   = DWM_TNP_VISIBLE | DWM_TNP_RECTDESTINATION | DWM_TNP_RECTSOURCE
                         | DWM_TNP_DISABLEFORCECVI;
            tp.fVisible  = TRUE;
            tp.rcSource       = rcSource;
            tp.rcDestination  = { 0, 0, shellW, shellH };
            DwmUpdateThumbnailProperties(mon.hShellThumb, &tp);
        }
    }

    if (m_dcompDevice)
        m_dcompDevice->Commit();
}

// ============================================================================
// Flip3DCompApp::RebuildMonitorBackdropsIfNeeded
// ============================================================================
bool Flip3DCompApp::RebuildMonitorBackdropsIfNeeded()
{
    const std::vector<MONITORINFO> monitors = EnumerateMonitors();
    if (monitors.empty())
        return false;

    bool layoutSame = monitors.size() == m_monitorBackdrops.size();
    if (layoutSame)
    {
        for (size_t i = 0; i < monitors.size(); ++i)
        {
            const MONITORINFO& a = monitors[i];
            const MonitorBackdrop& b = m_monitorBackdrops[i];
            if (a.rcMonitor.left   != b.rcMonitor.left
             || a.rcMonitor.top    != b.rcMonitor.top
             || a.rcMonitor.right  != b.rcMonitor.right
             || a.rcMonitor.bottom != b.rcMonitor.bottom
             || a.rcWork.left      != b.rcWork.left
             || a.rcWork.top       != b.rcWork.top
             || a.rcWork.right     != b.rcWork.right
             || a.rcWork.bottom    != b.rcWork.bottom)
            {
                layoutSame = false;
                break;
            }
        }
    }

    if (layoutSame)
    {
        UpdateBackdropLayout();
        return false;
    }

    DestroyMonitorBackdrops();

    if (!m_washSurface && m_d3dDevice && m_dcompDevice)
        CreateSharedWashSurface(m_d3dDevice.Get(), m_dcompDevice.Get(), m_washSurface);

    HWND shell = GetShellWindow();
    if (!shell || !m_dcompDevice || !m_rootVisual || !m_washSurface)
        return false;

    ComPtr<IDCompositionVisual> rootBase;
    if (FAILED(m_rootVisual.As(&rootBase)))
        return false;

    RECT shellWnd = {};
    GetWindowRect(shell, &shellWnd);

    for (const MONITORINFO& mi : monitors)
    {
        MonitorBackdrop mon = {};
        mon.rcMonitor = mi.rcMonitor;
        mon.rcWork    = mi.rcWork;

        const LONG shellW = mon.rcWork.right - mon.rcWork.left;
        const LONG shellH = mon.rcWork.bottom - mon.rcWork.top;
        if (shellW <= 0 || shellH <= 0)
            continue;

        RECT rcSource = mon.rcWork;
        OffsetRect(&rcSource, -shellWnd.left, -shellWnd.top);

        DWM_THUMBNAIL_PROPERTIES tp = {};
        tp.dwFlags   = DWM_TNP_VISIBLE | DWM_TNP_RECTDESTINATION | DWM_TNP_RECTSOURCE
                     | DWM_TNP_DISABLEFORCECVI;
        tp.fVisible  = TRUE;
        tp.rcSource      = rcSource;
        tp.rcDestination = mon.rcWork;

        void* pv = nullptr;
        HRESULT hr = m_pfnCreateSharedThumbVisual(
            m_hwnd,
            shell,
            DWM_TNF_DWMWINDOW,
            &tp,
            m_dcompDevice.Get(),
            &pv,
            &mon.hShellThumb);

        if (FAILED(hr) || !pv)
            continue;

        ComPtr<IDCompositionVisual> thumbBase;
        thumbBase.Attach((IDCompositionVisual*)pv);
        hr = thumbBase.As(&mon.shellThumb);
        if (FAILED(hr))
        {
            DwmUnregisterThumbnail(mon.hShellThumb);
            continue;
        }

        ComPtr<IDCompositionVisual2> shellContainer;
        hr = m_dcompDevice->CreateVisual(&shellContainer);
        if (FAILED(hr))
            continue;
        hr = shellContainer.As(&mon.shellContainer);
        if (FAILED(hr))
            continue;

        hr = mon.shellContainer->AddVisual(mon.shellThumb.Get(), FALSE, nullptr);
        if (FAILED(hr))
            continue;

        ComPtr<IDCompositionVisual2> washVis;
        hr = m_dcompDevice->CreateVisual(&washVis);
        if (FAILED(hr))
            continue;
        hr = washVis->SetContent(m_washSurface.Get());
        if (FAILED(hr))
            continue;
        hr = washVis.As(&mon.washVisual);
        if (FAILED(hr))
            continue;

        // Z-order back→front: shell desktop, wash, then scene (added first in InitComposition).
        rootBase->AddVisual(mon.shellContainer.Get(), FALSE, m_sceneVisual.Get());
        rootBase->AddVisual(mon.washVisual.Get(), FALSE, m_sceneVisual.Get());

        mon.shellContainer->SetOpacity(1.0f);
        m_monitorBackdrops.push_back(std::move(mon));
    }

    UpdateBackdropLayout();
    return true;
}

// ============================================================================
// Flip3DCompApp::CreateShellBackdrop
// ============================================================================
HRESULT Flip3DCompApp::CreateShellBackdrop()
{
    if (!RebuildMonitorBackdropsIfNeeded())
    {
        if (m_monitorBackdrops.empty())
            return E_FAIL;
    }
    return S_OK;
}
