// ============================================================================
// Flip3DComp.h — DirectComposition 3D Window Switcher Application
// ============================================================================
//
//   Visual tree layout:
//     Root (DesktopWindowTarget)
//       └── SceneVisual (IDCompositionVisual3)  ← 3D carousel
//       └── WashVisual (dark overlay)
//       └── Per-monitor shell thumbnails + wash (virtual desktop layout)
//
//   Thumbnails: DwmpCreateSharedThumbnailVisual (dwmapi.dll ord 147)
//   3D carousel: parent Visual carries camera matrix; children carry model matrices.
//
// ============================================================================
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <dwmapi.h>
#include <dcomp.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <dxgi1_2.h>

#include <chrono>
#include <string>
#include <vector>

#include <oleacc.h>

#include "Config.h"
#include "MathTypes.h"
#include "FlipMath.h"
#include "Timeline.h"
#include "CardModel.h"

using Microsoft::WRL::ComPtr;

class Flip3DAccessible;

// ============================================================================
// Flip3DCompApp — Main application class
// ============================================================================
class Flip3DCompApp
{
    friend class Flip3DAccessible;

public:
    // ========================================================================
    // Public interface
    // ========================================================================

    bool Initialize(HINSTANCE hInstance);
    HWND WindowHandle() const { return m_hwnd; }
    const wchar_t* InitErrorMessage() const { return m_initError.c_str(); }
    int  Run();

private:
    // ========================================================================
    // Window procedure forwarding
    // ========================================================================

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    // ========================================================================
    // Lifecycle
    // ========================================================================

    bool CreateAppWindow();
    void    ApplyFullscreenLayout();

    // ========================================================================
    // DWM Thumbnail API (dwmapi.dll ordinals 147, 162)
    // ========================================================================

    bool    LoadThumbApi();
    void    UnloadThumbApi();

    // ========================================================================
    // DirectComposition composition
    // ========================================================================

    HRESULT InitComposition();
    HRESULT CreateShellBackdrop();
    void    DestroyMonitorBackdrops();
    void    UpdateBackdropLayout();
    bool    RebuildMonitorBackdropsIfNeeded();

    // ========================================================================
    // Window enumeration
    // ========================================================================

    std::vector<HWND> EnumerateWindows();
    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
    struct EnumContext;
    bool    QualifiesForView(HWND hwnd) const;
    bool    IsFlip3DViewActive() const;
    bool    IsNeverHiddenWindow(HWND hwnd) const;

    // Shell Hook drives dynamic card add/remove (no ShowWindow on live windows).
    void    EnterFlip3DWindowMode();
    void    LeaveFlip3DWindowMode();
    void    OnShellHookMessage(WPARAM wParam, LPARAM lParam);
    void    OnWindowShowHide(HWND hwnd);

    int     FindCardIndex(HWND hwnd) const;
    bool    AddCardForWindow(HWND hwnd);
    void    RemoveCardAt(size_t index);
    HRESULT CreateCardVisual(CardModel& card);

    // ========================================================================
    // Card building
    // ========================================================================

    void    BuildCards();
    void    UpdateMonitorRect();
    void    UpdateCardGeometry(CardModel& card, float normMonW, float normMonH,
                               bool selectedRestore = false);
    void    OnThumbnailSourceSizeChanged();
    void    UpdateCardThumbnailDest(CardModel& card);
    HRESULT CreateCardVisuals();

    // ========================================================================
    // Per-frame update
    // ========================================================================

    void    Update(float dtSeconds);

    // ========================================================================
    // View state management
    // ========================================================================

    float   EnterProgress() const;
    void    ReplayEnterAnimation();
    void    ExitView(bool commitScroll = true,
                     float exitDurationSec = kExitDurationSec);
    void    BeginExitView();
    void    TickSmoothScroll(float dtSeconds);
    void    TickRepeatedRotate();
    void    StartRotationStep(bool backward, float durationSec);
    void    OnRotationTimeUpdated();
    int     DistanceBetween(size_t sourcePos, size_t targetPos, bool forward) const;
    float   RotationDurationForRotateList() const;
    void    SelectWindow(HWND hwndTarget);
    void    SelectFront();

    // ========================================================================
    // Rotation (uDWM m_leWindows linked-list model)
    // ========================================================================

    void    RotateBy(int deltaSteps);
    void    RotateListPhysically(bool backward);
    void    RotateToWindow(HWND targetHwnd);
    float   ComputeScrollTargetForCard(int listIndex) const;
    float   CardListSlot(int listIndex) const;
    float   CaptureCarouselVisualSlot(int listIndex) const;
    float   GetCardDisplaySlot(int listIndex) const;
    void    InvalidateDisplaySlots();
    void    SyncDisplaySlotsToList();
    void    AdvanceWrapDisplaySlots(float deltaScrollPos);
    void    FinishWrapPhase(CardModel& card, int listIndex);
    void    BeginEnteringBackWrap(CardModel& card, int listIndex);
    void    StepEnteringBackWrap(CardModel& card, int listIndex);
    void    FreezeCarouselVisuals();
    void    UpdateVisualSlots(float enterProgress);
    void    SettleWrapDisplaySlots();
    void    OnCarouselWrapForward(HWND outgoingHwnd, float outgoingSlot);
    void    OnCarouselWrapBackward(HWND incomingHwnd);
    float   CarouselEdgeSpan() const;
    void    WrapCarouselScroll();
    void    RotateSelectedToListFront();
    void    AlignCarouselScrollSettled();
    void    CommitCarouselScroll();
    bool    IsSelectionAtCarouselFront() const;
    void    StepCarouselScroll(float dtSeconds, bool notifyFrontChange);

    // ========================================================================
    // Input processing
    // ========================================================================

    bool    OnKey(bool down, UINT vkCode, LPARAM lParam);
    bool    OnWheel(int wheelDelta);
    bool    OnMouse(LONG x, LONG y, bool pressed);

    // ========================================================================
    // Hit testing (3D ray-triangle intersection)
    // ========================================================================

    HWND    HitTest3DScene(LONG screenX, LONG screenY) const;

    // ========================================================================
    // Camera & transforms
    // ========================================================================

    void    UpdateCamera(float enterProgress);
    void    UpdateCards(float enterProgress, float dtSeconds);
    Matrix4x4 BuildCameraMatrix(float enterProgress) const;
    Matrix4x4 BuildViewMatrix(float enterProgress) const;
    Matrix4x4 BuildProjMatrix(float aspect, float enterProgress) const;
    Matrix4x4 BuildModelMatrix(const CardModel& card, float paramT,
                               float enterProgress, float flatDepthRank) const;

    float ComputeCarouselEdgeOpacity(float slot) const;
    float ComputeUpdateAlpha(const CardModel& card, float enterProgress,
                             float carouselSlot) const;
    float ComputeRotationDisplaySlot(int listIndex) const;
    float ComputeRotationAlpha(const CardModel& card, float enterProgress,
                               int listIndex, float carouselSlot) const;
    bool  ShouldDrawRotationCard(int listIndex) const;
    float ComputeCarouselPathDenom() const;
    float ComputeCarouselBezierT(float slot) const;

    float   ComputeFlatDepthRank(float slot, float enterProgress,
                                 int listIndex) const;
    float   ComputePaintKey(float slot, float enterProgress, int listIndex) const;

    // Accessibility (uDWM CFlip3DAccessible + NotifyWinEvent)
    bool    InitAccessibility();
    void    ShutdownAccessibility();
    IAccessible* GetAccessibleObject();
    void    NotifyAccessibilityEvent(DWORD event, LONG childId = CHILDID_SELF);
    void    NotifyAccessibilityFocusFront();
    int     AccessibleChildCount() const;
    HRESULT AccessibleWindowName(int index, BSTR* pszName) const;
    bool    AccessibleCardScreenRect(int index, long* pxLeft, long* pyTop,
                                     long* pcxWidth, long* pcyHeight) const;
    int     AccessibleHitTest(long screenX, long screenY) const;
    bool    AccessiblePointInView(POINT screenPt) const;
    HRESULT AccessibleRotateToIndex(int index);
    HRESULT AccessibleSelectIndex(int index);

    // ========================================================================
    // Member variables
    // ========================================================================

    // ---- Window / instance ----
    HINSTANCE               m_hInstance     = nullptr;
    HWND                    m_hwnd          = nullptr;
    std::wstring            m_initError;

    // Per-monitor shell thumbnail + dark wash (client coords = virtual desktop).
    struct MonitorBackdrop
    {
        RECT                        rcMonitor = {};
        RECT                        rcWork    = {};
        ComPtr<IDCompositionVisual3> washVisual;
        ComPtr<IDCompositionVisual3> shellContainer;
        ComPtr<IDCompositionVisual3> shellThumb;
        HTHUMBNAIL                  hShellThumb = nullptr;
    };

    // ---- Dimensions ----
    UINT                    m_width         = 1600;
    UINT                    m_height        = 900;
    float                   m_monW          = 1920.0f;
    float                   m_monH          = 1080.0f;
    float                   m_monOriginX    = 0.0f;   // primary rcWork.left (screen px)
    float                   m_monOriginY    = 0.0f;   // primary rcWork.top  (screen px)
    float                   m_viewX         = 0.0f;   // primary rcWork origin in client px
    float                   m_viewY         = 0.0f;
    std::vector<MonitorBackdrop> m_monitorBackdrops;
    ComPtr<IDCompositionSurface> m_washSurface;
    bool                    m_minimized     = false;
    bool                    m_rtl           = false;
    bool                    m_thumbnailsDirty = false; // coalesce WM 0x327 bursts
    UINT                    m_wmShellHook     = 0;
    bool                    m_shellHookRegistered = false;

    // ---- Frame timing ----
    std::chrono::steady_clock::time_point   m_prevFrame;
    std::chrono::steady_clock::time_point   m_lastKeyProcessed{};
    int                                       m_wheelPendingSlots = 0;
    std::chrono::steady_clock::time_point     m_lastWheelTime{};
    UINT                                      m_heldNavigationKey = 0;
    int                                       m_heldNavigationDirection = 0;
    std::chrono::steady_clock::time_point     m_heldNavigationStart{};
    bool                                      m_openingTabPending = false;
    std::chrono::steady_clock::time_point     m_openingTabStart{};

    // ---- Cards & view state ----
    std::vector<CardModel>  m_cards;
    ViewState               m_state         = ViewState::Inactive;
    Timeline                m_animEnter;

    // ---- Smooth carousel scroll ----
    float                   m_scrollPos     = 0.0f;
    float                   m_scrollTarget  = 0.0f;
    float                   m_wrapScrollAdjustThisFrame = 0.0f;

    // Frozen browse scroll at exit start; held constant during ExitRepeatedRotate
    float                   m_exitScrollSnapshot    = 0.0f;

    // ---- Discrete list rotation (uDWM m_ptlRotateListTimeline) ----
    Timeline                m_rotateTimeline;
    bool                    m_rotateBackward = false;
    bool                    m_showOutgoingDuringRotation = false;
    float                   m_rRepeatedRotateRate = 0.0f;
    int                     m_repeatedRotateStepsRemaining = 0;

    // ---- Selection ----
    HWND                    m_originalFrontHwnd = nullptr;
    HWND                    m_selectedHwnd   = nullptr;
    std::vector<HWND>       m_lastPaintOrder;
    HWND                    m_hitHwnd        = nullptr;

    // ---- Accessibility ----
    IAccessible*            m_pAccessible    = nullptr;
    bool                    m_comInitialized = false;
    bool                    m_comNeedsUninit = false;
    // ---- D3D11 device ----
    ComPtr<ID3D11Device>              m_d3dDevice;

    // ---- DirectComposition resources ----
    ComPtr<IDCompositionDesktopDevice>  m_dcompDevice;
    ComPtr<IDCompositionTarget>         m_dcompTarget;
    ComPtr<IDCompositionVisual2>        m_rootVisual;
    ComPtr<IDCompositionVisual3>        m_sceneVisual;

    // ---- DWM thumbnail API ----
    HMODULE                               m_dwmapi        = nullptr;
    // Required at startup (LoadThumbApi); always non-null after Initialize succeeds.
    DwmpCreateSharedThumbnailVisual_fn    m_pfnCreateSharedThumbVisual = nullptr;
    DwmpQueryWindowThumbnailSourceSize_fn m_pfnQueryThumbSize          = nullptr;
    GetWindowMinimizeRect_fn              m_pfnGetWindowMinimizeRect    = nullptr;
};
