// ============================================================================
// Config.h — Constants, enums, type aliases, and DWM thumbnail API definitions
// ============================================================================
// Mirrors the constant / enum organization pattern from CFlip3D.h
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

// ============================================================================
// Type aliases
// ============================================================================
using Microsoft::WRL::ComPtr;
using Matrix4x4 = D2D_MATRIX_4X4_F;

// ============================================================================
// uDWM animation / camera constants
// ============================================================================
constexpr float  kEnterExitDurationSec     = 0.25f;    // uDWM binary: 0.25s
constexpr float  kExitDurationSec          = 0.25f;    // uDWM: same timeline for exit + exit-rotate
constexpr bool   kEnableAnimationEasing    = false;    // controls if Flip3D should use easing or not

// Timeline easing: CSS cubic-bezier(0, 0, 0, 1) — fast start, soft landing
constexpr float  kTimelineBezierX1 = 0.0f;
constexpr float  kTimelineBezierY1 = 0.0f;
constexpr float  kTimelineBezierX2 = 0.0f;
constexpr float  kTimelineBezierY2 = 1.0f;

constexpr float  kScrollSmoothTimeSec      = 0.15f;    // smooth-scroll ease time constant
constexpr float  kAnimationRate            = 1.0f;     // animation speed multiplier (1 = normal)
constexpr float  kShiftAnimationRate       = 0.1f;     // Windows/DWM Shift slowdown (10x slower)
constexpr float  kScrollSettleEpsilon      = 0.002f;   // scrollPos≈scrollTarget threshold (browse
constexpr float  kFrontCardExitSlot        = -0.5f;    // front card slot when exiting (fade-out)
constexpr float  kFrontCardFadeSpeed       = 0.0f;     // front card fade speed (0 = instant, 1 = linear)
constexpr float  kScrollWheelNotchFraction = 1.0f;     // carousel slots per mouse wheel notch
constexpr bool   kScrollWheelSnapToSlots   = true;     // snap scroll target to integer slots after wheel input
constexpr float  kWheelActiveTimeoutSec    = 0.15f;    // throttle repeated wheel processing (uDWM CFlip3D::OnWheel)
constexpr float  kKeyRepeatIntervalSec     = 0.135f;   // throttle repeated keydown processing (uDWM CFlip3D::OnKey)
constexpr float  kHeldKeyStartDelaySec     = 0.20f;    // delay continuous rotation so a key tap moves one card
constexpr float  kHeldKeyRotateSpeed       = 6.0f;     // carousel slots per second while a navigation key is held
constexpr float  kOpeningTabDelaySec       = 0.40f;    // delay the first automatic Tab move after opening
constexpr float  kRotateListDurationSec    = 0.175f;   // uDWM g_secRotateListDuration
constexpr float  kNearPlaneEdgeSize        = 1.15f;    // near plane half-extent
constexpr float  kNearPlaneDistance        = 1.0f;     // near plane Z distance
constexpr int    kMaxVisibleCards          = 10;       // max simultaneously visible cards
constexpr int    kMaxCards                 = 24;       // absolute max cards in carousel

enum class CardThumbnailQuality
{
Low,MediumLow,Medium,MediumHigh,High,
};

constexpr CardThumbnailQuality kCardThumbnailQuality = CardThumbnailQuality::Medium; // This controls the DWM thumbnail resolution used by 3D window cards. By default Windows Vista & 7 have it set to a Medium value (0.50f)

constexpr float  kBackCardOpacityScale     = 1.0f;     // scale opacity for cards near the back edge (slot ~ span-2..span)
constexpr float  kBackCardMinOpacity       = 0.60f;    // minimum opacity for cards entering from the back (wrap fade)
constexpr float  kBackCardFadeTimeSec      = 0.015f;    // fade-in time for cards entering from the back (wrap fade)
constexpr bool   kEnableBackCardOpacityEasing = true;

// Pre-computed camera poses (radians)
constexpr float kCameraFinalTranslateX = -1.1f;             
constexpr float kCameraFinalTranslateY =  0.35f;   // uDWM InitializeModelAndCamera
constexpr float kCameraFinalTranslateZ =  0.35f;
constexpr float kCameraFinalRotateX    =  0.08726646f;   //  5.0 deg pitch
constexpr float kCameraFinalRotateY    =  0.43633232f;   // 25.0 deg yaw
constexpr float kCameraFinalRotateZ    =  0.061086524f;  //  3.5 deg roll

// Bezier control points for carousel path
constexpr float kBezierControls[3][3] = {
    { -2.0f,   0.5f,   -2.25f },
    { -1.8f,   0.55f,  -1.25f },
    { -1.45f, -0.1f,   -0.3f  },        
};

constexpr float CardThumbnailQualityScale()
{
    switch (kCardThumbnailQuality)
    {
    case CardThumbnailQuality::Low:    return 0.10f;
    case CardThumbnailQuality::MediumLow:    return 0.25f;
    case CardThumbnailQuality::Medium: return 0.50f;
    case CardThumbnailQuality::MediumHigh:    return 0.75f;
    case CardThumbnailQuality::High:   return 1.00f;
    }
    return 1.00f;
}

// Normalization Bezier coefficients (quadratic)
constexpr float kNormalizationBezier[3] = { 1.0f, 0.85f, 0.75f };

// uDWM c_dFlipRotationPercent — steady desktop wash while Flip3D is open
constexpr float kDesktopWashOpacityScale = 0.55f;

// uDWM CTopLevelWindow3D::GetFinalMinRect / flip3d BuildFinalMinRect
constexpr float kFinalMinRectWidthPercentage = 0.6f;
constexpr float kFinalMinRectLeftPercentage  = 0.3f;

// ============================================================================
// Enums
// ============================================================================

// Interpolation mode for timeline-driven animations
enum class InterpolationMode
{
    Linear      = 0,
    Cubic       = 1,  // uDWM CTimeline: smoothstep t²(3-2t)
    CubicBezier = 2,  // CSS cubic-bezier (Config.h kTimelineBezier*)
};

// Decoupled display-slot phase during smooth-scroll list wrap (uDWM cycle card).
enum class CarouselWrapPhase
{
    None          = 0,
    ExitingFront  = 1,  // slot → [-1, 0]   front fade-out (forward scroll)
    EnteringBack  = 2,  // opacity slot span→listSlot; position stays on list slot
    EnteringFront = 3,  // slot → [-1, 0]   front fade-in (backward scroll)
};

// Flip3D view state machine (uDWM CFlip3D::Flip3DState ordering preserved)
enum class ViewState
{
    Inactive                  = 0,  // not visible
    Enter                     = 1,  // animating in
    Exit                      = 2,  // animating out (stationary carousel)
    Interactive               = 3,  // fully visible, accepting input
    ExitRepeatedRotate        = 4,  // exit flatten + discrete list rotation in parallel
};

// ============================================================================
// Private DWM thumbnail API (dwmapi.dll ordinal exports)
// ============================================================================
constexpr DWORD DWM_TNP_FORCECVI         = 0x40000000;
constexpr DWORD DWM_TNP_DISABLEFORCECVI  = 0x80000000;
constexpr DWORD DWM_TNP_ENABLE3D         = 0x04000000;

// DwmpCreateSharedThumbnailVisual dwThumbnailFlags (see dwthumbnailflags-reference.md)
constexpr DWORD DWM_TNF_DWMWINDOW       = 0x2;   // fDwmWindow → PostMessage(0x327) on size change

// Posted to hwndDestination when a DWM_TNF_DWMWINDOW thumbnail source resizes.
constexpr UINT WM_DWMTHUMBNAILSOURCESIZECHANGED = 0x327;

using DwmpCreateSharedThumbnailVisual_fn = HRESULT (WINAPI *)(
    HWND hwndDestination, HWND hwndSource, DWORD dwThumbnailFlags,
    DWM_THUMBNAIL_PROPERTIES* pThumbnailProperties,
    void* pDCompDevice, void** ppVisual, HTHUMBNAIL* phThumbnailId);

using DwmpQueryWindowThumbnailSourceSize_fn = HRESULT (WINAPI *)(
    HWND hwndSource, BOOL fSourceClientAreaOnly, SIZE* pSize);

using GetWindowMinimizeRect_fn = BOOL (WINAPI *)(HWND, LPRECT);

using DwmpUpdateDesktopThumbnail_fn = HRESULT (WINAPI *)(
    HWND hwnd, LPCRECT rcDest, LPCRECT rcSrc, BYTE opacity, DWORD dwFlags);
