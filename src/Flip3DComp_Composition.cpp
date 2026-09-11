// ============================================================================
// Flip3DComp_Composition.cpp — DirectComposition device init, desktop wash
// ============================================================================
#include "Flip3DComp.h"

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <vector>
#include <wincodec.h>

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

int ReadDesktopSetting(const wchar_t* name, int defaultValue)
{
    wchar_t value[32] = {};
    DWORD size = sizeof(value);
    DWORD type = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, L"Control Panel\\Desktop", name,
                     RRF_RT_REG_SZ, &type, value, &size) != ERROR_SUCCESS)
    {
        return defaultValue;
    }

    wchar_t* end = nullptr;
    const long parsed = std::wcstol(value, &end, 10);
    return end != value ? static_cast<int>(parsed) : defaultValue;
}

WallpaperPlacement GetWallpaperPlacement()
{
    if (ReadDesktopSetting(L"TileWallpaper", 0) != 0)
        return WallpaperPlacement::Tile;

    switch (ReadDesktopSetting(L"WallpaperStyle", 2))
    {
    case 0:  return WallpaperPlacement::Center;
    case 6:  return WallpaperPlacement::Fit;
    case 10: return WallpaperPlacement::Fill;
    case 22: return WallpaperPlacement::Span;
    case 2:
    default: return WallpaperPlacement::Stretch;
    }
}

std::vector<MONITORINFO> EnumerateMonitors()
{
    EnumMonitorsContext ctx;
    EnumDisplayMonitors(nullptr, nullptr, EnumMonitorsProc, (LPARAM)&ctx);
    return ctx.monitors;
}

struct TaskbarSearchContext
{
    HMONITOR monitor = nullptr;
    HWND taskbar = nullptr;
};

BOOL CALLBACK FindTaskbarProc(HWND hwnd, LPARAM lParam)
{
    auto* ctx = reinterpret_cast<TaskbarSearchContext*>(lParam);
    wchar_t className[64] = {};
    if (!GetClassNameW(hwnd, className, ARRAYSIZE(className)))
        return TRUE;

    const bool isTaskbar = !_wcsicmp(className, L"Shell_TrayWnd")   
                        || !_wcsicmp(className, L"Shell_SecondaryTrayWnd");
    if (isTaskbar && MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST) == ctx->monitor)
    {
        ctx->taskbar = hwnd;
        return FALSE;
    }
    return TRUE;
}

HWND FindTaskbarForMonitor(HMONITOR monitor)
{
    TaskbarSearchContext ctx = { monitor, nullptr };
    EnumWindows(FindTaskbarProc, reinterpret_cast<LPARAM>(&ctx));
    return ctx.taskbar;
}

HRESULT CreateWallpaperSurface(ID3D11Device* d3d,
                               IDCompositionDesktopDevice* dcomp,
                               ComPtr<IDCompositionSurface>& outSurface,
                               UINT& outWidth,
                               UINT& outHeight)
{
    if (!d3d || !dcomp)
        return E_INVALIDARG;

    struct ComScope
    {
        HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ~ComScope()
        {
            if (result == S_OK)
                CoUninitialize();
        }
    } comScope;
    if (FAILED(comScope.result) && comScope.result != RPC_E_CHANGED_MODE)
        return comScope.result;

    wchar_t wallpaperPath[MAX_PATH] = {};
    if (!SystemParametersInfoW(SPI_GETDESKWALLPAPER, ARRAYSIZE(wallpaperPath),
                                wallpaperPath, 0)
        || wallpaperPath[0] == L'\0')
    {
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                  CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&factory));
    if (FAILED(hr))
        return hr;

    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(
        wallpaperPath, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad,
        &decoder);
    if (FAILED(hr))
        return hr;

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr))
        return hr;

    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr))
        return hr;

    hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA,
                               WICBitmapDitherTypeNone, nullptr, 0.0,
                               WICBitmapPaletteTypeCustom);
    if (FAILED(hr))
        return hr;

    UINT width = 0;
    UINT height = 0;
    hr = converter->GetSize(&width, &height);
    if (FAILED(hr) || width == 0 || height == 0)
        return FAILED(hr) ? hr : E_FAIL;

    std::vector<BYTE> pixels(static_cast<size_t>(width) * height * 4);
    hr = converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size()),
                               pixels.data());
    if (FAILED(hr))
        return hr;

    ComPtr<IDCompositionSurfaceFactory> surfaceFactory;
    hr = dcomp->CreateSurfaceFactory(d3d, &surfaceFactory);
    if (FAILED(hr))
        return hr;

    ComPtr<IDCompositionSurface> surface;
    hr = surfaceFactory->CreateSurface(width, height, DXGI_FORMAT_B8G8R8A8_UNORM,
                                       DXGI_ALPHA_MODE_IGNORE, &surface);
    if (FAILED(hr))
        return hr;

    ComPtr<IDXGISurface> dxgiSurface;
    POINT offset = {};
    hr = surface->BeginDraw(nullptr, IID_PPV_ARGS(&dxgiSurface), &offset);
    if (FAILED(hr))
        return hr;

    ComPtr<ID3D11Texture2D> destination;
    hr = dxgiSurface.As(&destination);
    if (SUCCEEDED(hr))
    {
        ComPtr<ID3D11DeviceContext> context;
        d3d->GetImmediateContext(&context);
        context->UpdateSubresource(destination.Get(), 0, nullptr,
                                   pixels.data(), width * 4, 0);
        context->Flush();
    }

    HRESULT endDrawHr = surface->EndDraw();
    if (SUCCEEDED(hr))
        hr = endDrawHr;
    if (FAILED(hr))
        return hr;

    outSurface = std::move(surface);
    outWidth = width;
    outHeight = height;
    return S_OK;
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
            if (mon.taskbarContainer)
                rootBase->RemoveVisual(mon.taskbarContainer.Get());
            if (mon.wallpaperVisual)
                rootBase->RemoveVisual(mon.wallpaperVisual.Get());
            for (const auto& tile : mon.wallpaperTiles)
            {
                if (tile)
                    rootBase->RemoveVisual(tile.Get());
            }
            if (mon.hTaskbarThumb)
            {
                DwmUnregisterThumbnail(mon.hTaskbarThumb);
                mon.hTaskbarThumb = nullptr;
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

        const float shellX = (float)(mon.rcMonitor.left - vx);
        const float shellY = (float)(mon.rcMonitor.top  - vy);

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

        if (mon.wallpaperVisual)
        {
            ComPtr<IDCompositionVisual2> wallpaper2;
            if (SUCCEEDED(mon.wallpaperVisual.As(&wallpaper2)))
            {
                const float imageW = (float)std::max(mon.wallpaperWidth, 1u);
                const float imageH = (float)std::max(mon.wallpaperHeight, 1u);
                float scaleX = 1.0f;
                float scaleY = 1.0f;
                float wallpaperX = washX;
                float wallpaperY = washY;

                switch (mon.wallpaperPlacement)
                {
                case WallpaperPlacement::Center:
                    wallpaperX += (washW - imageW) * 0.5f;
                    wallpaperY += (washH - imageH) * 0.5f;
                    break;
                case WallpaperPlacement::Fit:
                {
                    const float scale = std::min(washW / imageW, washH / imageH);
                    scaleX = scaleY = scale;
                    wallpaperX += (washW - imageW * scale) * 0.5f;
                    wallpaperY += (washH - imageH * scale) * 0.5f;
                    break;
                }
                case WallpaperPlacement::Fill:
                case WallpaperPlacement::Span:
                {
                    const float scale = std::max(washW / imageW, washH / imageH);
                    scaleX = scaleY = scale;
                    wallpaperX += (washW - imageW * scale) * 0.5f;
                    wallpaperY += (washH - imageH * scale) * 0.5f;
                    break;
                }
                case WallpaperPlacement::Stretch:
                    scaleX = washW / imageW;
                    scaleY = washH / imageH;
                    break;
                case WallpaperPlacement::Tile:
                {
                    const LONG rootX = mon.rcMonitor.left - vx;
                    const LONG rootY = mon.rcMonitor.top - vy;
                    const LONG tileW = static_cast<LONG>(mon.wallpaperWidth);
                    const LONG tileH = static_cast<LONG>(mon.wallpaperHeight);
                    wallpaperX = (float)(rootX - ((rootX % tileW) + tileW) % tileW);
                    wallpaperY = (float)(rootY - ((rootY % tileH) + tileH) % tileH);
                    break;
                }
                }

                const D2D_MATRIX_3X2_F xform = {
                    scaleX, 0.f,
                    0.f, scaleY,
                    wallpaperX, wallpaperY,
                };
                wallpaper2->SetTransform(xform);

                for (size_t i = 0; i < mon.wallpaperTiles.size(); ++i)
                {
                    ComPtr<IDCompositionVisual2> tile2;
                    if (i < mon.wallpaperTileOrigins.size()
                        && SUCCEEDED(mon.wallpaperTiles[i].As(&tile2)))
                    {
                        const POINT origin = mon.wallpaperTileOrigins[i];
                        const D2D_MATRIX_3X2_F tileTransform = {
                            1.f, 0.f,
                            0.f, 1.f,
                            (float)origin.x, (float)origin.y,
                        };
                        tile2->SetTransform(tileTransform);
                    }
                }
            }
        }

        if (mon.taskbarContainer)
        {
            ComPtr<IDCompositionVisual2> taskbar2;
            if (SUCCEEDED(mon.taskbarContainer.As(&taskbar2)))
            {
                const D2D_MATRIX_3X2_F xform = {
                    1.f, 0.f,
                    0.f, 1.f,
                    (float)(mon.rcTaskbar.left - vx),
                    (float)(mon.rcTaskbar.top - vy),
                };
                taskbar2->SetTransform(xform);
            }
        }

        const LONG shellW = mon.rcMonitor.right - mon.rcMonitor.left;
        const LONG shellH = mon.rcMonitor.bottom - mon.rcMonitor.top;
        if (mon.hShellThumb && shellW > 0 && shellH > 0)
        {
            RECT rcSource = mon.rcMonitor;
            OffsetRect(&rcSource, -shellWnd.left, -shellWnd.top);

            DWM_THUMBNAIL_PROPERTIES tp = {};
            tp.dwFlags   = DWM_TNP_VISIBLE | DWM_TNP_RECTDESTINATION | DWM_TNP_RECTSOURCE
                         | DWM_TNP_DISABLEFORCECVI;
            tp.fVisible  = TRUE;
            tp.rcSource       = rcSource;
            tp.rcDestination  = {
                0, 0,
                mon.rcMonitor.right - mon.rcMonitor.left,
                mon.rcMonitor.bottom - mon.rcMonitor.top,
            };
            DwmUpdateThumbnailProperties(mon.hShellThumb, &tp);
        }

        const LONG taskbarW = mon.rcTaskbar.right - mon.rcTaskbar.left;
        const LONG taskbarH = mon.rcTaskbar.bottom - mon.rcTaskbar.top;
        if (mon.hTaskbarThumb && taskbarW > 0 && taskbarH > 0)
        {
            DWM_THUMBNAIL_PROPERTIES tp = {};
            tp.dwFlags = DWM_TNP_VISIBLE | DWM_TNP_RECTDESTINATION
                       | DWM_TNP_RECTSOURCE | DWM_TNP_DISABLEFORCECVI;
            tp.fVisible = TRUE;
            tp.rcSource = { 0, 0, taskbarW, taskbarH };
            tp.rcDestination = { 0, 0, taskbarW, taskbarH };
            DwmUpdateThumbnailProperties(mon.hTaskbarThumb, &tp);
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
    if (!m_dcompDevice || !m_rootVisual || !m_washSurface)
        return false;

    ComPtr<IDCompositionVisual> rootBase;
    if (FAILED(m_rootVisual.As(&rootBase)))
        return false;

    ComPtr<IDCompositionSurface> wallpaperSurface;
    UINT wallpaperWidth = 0;
    UINT wallpaperHeight = 0;
    if (!shell
        && FAILED(CreateWallpaperSurface(m_d3dDevice.Get(), m_dcompDevice.Get(),
                                          wallpaperSurface, wallpaperWidth,
                                          wallpaperHeight)))
    {
        return false;
    }

    RECT shellWnd = {};
    if (shell)
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

        HRESULT hr = S_OK;
        if (shell)
        {
            RECT rcSource = mon.rcMonitor;
            OffsetRect(&rcSource, -shellWnd.left, -shellWnd.top);

            DWM_THUMBNAIL_PROPERTIES tp = {};
            tp.dwFlags   = DWM_TNP_VISIBLE | DWM_TNP_RECTDESTINATION | DWM_TNP_RECTSOURCE
                         | DWM_TNP_DISABLEFORCECVI;
            tp.fVisible  = TRUE;
            tp.rcSource      = rcSource;
            tp.rcDestination = {
                0, 0,
                mon.rcMonitor.right - mon.rcMonitor.left,
                mon.rcMonitor.bottom - mon.rcMonitor.top,
            };

            void* pv = nullptr;
            hr = m_pfnCreateSharedThumbVisual(
                m_hwnd,
                shell,
                DWM_TNF_DWMWINDOW,
                &tp,
                m_dcompDevice.Get(),
                &pv,
                &mon.hShellThumb);

            if (FAILED(hr) || !pv)
                continue;

            HWND taskbar = FindTaskbarForMonitor(MonitorFromRect(&mon.rcMonitor,
                                                                 MONITOR_DEFAULTTONEAREST));
            if (taskbar)
            {
            RECT taskbarRect = {};
            if (GetWindowRect(taskbar, &taskbarRect)
                && taskbarRect.right > taskbarRect.left
                && taskbarRect.bottom > taskbarRect.top)
            {
                DWM_THUMBNAIL_PROPERTIES taskbarProperties = {};
                taskbarProperties.dwFlags = DWM_TNP_VISIBLE | DWM_TNP_RECTDESTINATION
                                           | DWM_TNP_RECTSOURCE | DWM_TNP_DISABLEFORCECVI;
                taskbarProperties.fVisible = TRUE;
                taskbarProperties.rcSource = {
                    0, 0,
                    taskbarRect.right - taskbarRect.left,
                    taskbarRect.bottom - taskbarRect.top,
                };
                taskbarProperties.rcDestination = taskbarProperties.rcSource;

                HTHUMBNAIL hTaskbarThumb = nullptr;
                void* taskbarPv = nullptr;
                hr = m_pfnCreateSharedThumbVisual(
                    m_hwnd, taskbar, DWM_TNF_DWMWINDOW, &taskbarProperties,
                    m_dcompDevice.Get(), &taskbarPv, &hTaskbarThumb);
                if (SUCCEEDED(hr) && taskbarPv)
                {
                    mon.rcTaskbar = taskbarRect;
                    mon.hTaskbarThumb = hTaskbarThumb;

                    ComPtr<IDCompositionVisual3> taskbarVisual;
                    taskbarVisual.Attach((IDCompositionVisual3*)taskbarPv);
                    mon.taskbarThumb = taskbarVisual;
                    ComPtr<IDCompositionVisual2> taskbarContainer;
                    hr = m_dcompDevice->CreateVisual(&taskbarContainer);
                    if (SUCCEEDED(hr))
                        hr = taskbarContainer.As(&mon.taskbarContainer);
                    if (SUCCEEDED(hr))
                        hr = mon.taskbarContainer->AddVisual(mon.taskbarThumb.Get(), FALSE, nullptr);
                    if (FAILED(hr))
                    {
                        DwmUnregisterThumbnail(mon.hTaskbarThumb);
                        mon.hTaskbarThumb = nullptr;
                        mon.taskbarThumb.Reset();
                        mon.taskbarContainer.Reset();
                        mon.rcTaskbar = {};
                    }
                }
            }
            }

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
        }
        else
        {
            ComPtr<IDCompositionVisual2> wallpaperVisual;
            HRESULT hr = m_dcompDevice->CreateVisual(&wallpaperVisual);
            if (FAILED(hr) || FAILED(wallpaperVisual->SetContent(wallpaperSurface.Get())))
                continue;
            if (FAILED(wallpaperVisual.As(&mon.wallpaperVisual)))
                continue;
            mon.wallpaperSurface = wallpaperSurface;
            mon.wallpaperWidth = wallpaperWidth;
            mon.wallpaperHeight = wallpaperHeight;
            mon.wallpaperPlacement = GetWallpaperPlacement();

            if (mon.wallpaperPlacement == WallpaperPlacement::Tile)
            {
                const int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
                const int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
                const LONG monitorWidth = mi.rcMonitor.right - mi.rcMonitor.left;
                const LONG monitorHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;
                const LONG imageWidth = static_cast<LONG>(wallpaperWidth);
                const LONG imageHeight = static_cast<LONG>(wallpaperHeight);
                const LONG rootX = mi.rcMonitor.left - vx;
                const LONG rootY = mi.rcMonitor.top - vy;
                const LONG firstX = rootX - ((rootX % imageWidth) + imageWidth) % imageWidth;
                const LONG firstY = rootY - ((rootY % imageHeight) + imageHeight) % imageHeight;

                for (LONG y = firstY; y < rootY + monitorHeight; y += imageHeight)
                {
                    for (LONG x = firstX; x < rootX + monitorWidth; x += imageWidth)
                    {
                        if (x == firstX && y == firstY)
                            continue;

                        ComPtr<IDCompositionVisual2> tileVisual;
                        if (FAILED(m_dcompDevice->CreateVisual(&tileVisual))
                            || FAILED(tileVisual->SetContent(wallpaperSurface.Get())))
                            continue;

                        ComPtr<IDCompositionVisual3> tileVisual3;
                        if (FAILED(tileVisual.As(&tileVisual3)))
                            continue;
                        mon.wallpaperTiles.push_back(tileVisual3);
                        mon.wallpaperTileOrigins.push_back({ x, y });
                    }
                }
            }
        }

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
        if (mon.shellContainer)
            rootBase->AddVisual(mon.shellContainer.Get(), FALSE, m_sceneVisual.Get());
        if (mon.wallpaperVisual)
        {
            ComPtr<IDCompositionVisual> wallpaperBase;
            if (SUCCEEDED(mon.wallpaperVisual.As(&wallpaperBase)))
                rootBase->AddVisual(wallpaperBase.Get(), FALSE, m_sceneVisual.Get());
        }
        for (const auto& tile : mon.wallpaperTiles)
        {
            if (tile)
                rootBase->AddVisual(tile.Get(), FALSE, m_sceneVisual.Get());
        }
        if (mon.taskbarContainer)
            rootBase->AddVisual(mon.taskbarContainer.Get(), FALSE, m_sceneVisual.Get());
        rootBase->AddVisual(mon.washVisual.Get(), FALSE, m_sceneVisual.Get());

        if (mon.shellContainer)
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
