// ============================================================================
// CardModel.h — Per-window data for the Flip3D carousel
// ============================================================================
// Each CardModel represents one window in the 3D carousel.
// Modeled after Flip3DWindow from CFlip3D.h — annotated with logical groupings.
// ============================================================================
#pragma once

#include <wrl/client.h>
#include <dcomp.h>
#include "Config.h"
#include "MathTypes.h"

using Microsoft::WRL::ComPtr;

// ============================================================================
// CardModel
// ============================================================================
struct CardModel
{
    // ---- Window identity ----
    HWND                m_hwnd        = nullptr;   // source window handle
    int                 m_initialCarouselIndex = 0;  // order at Flip3D enter (before rotates)
    HTHUMBNAIL          m_hThumb      = nullptr;   // DWM shared thumbnail handle

    // ---- DirectComposition visuals ----
    ComPtr<IDCompositionVisual3> m_visual;          // thumbnail content visual (DWM)
    ComPtr<IDCompositionVisual3> m_containerVisual; // baked Model×Camera + opacity

    // ---- Geometry (monitor-world units) ----
    Vec2                m_targetSize   = {};        // 3D carousel size (thumb px + NormalizeWindowSize)
    Vec3                m_originalPos  = {};        // desktop top-left (from window rect)
    Vec3                m_flatPos      = {};        // same anchor for 2D enter layout
    Vec2                m_flatSize     = {};        // 2D size (thumb px, or tile/rcWork)
    float               m_scrLeft      = 0.0f;      // raw screen pixel: left
    float               m_scrTop       = 0.0f;      // raw screen pixel: top
    float               m_scrW         = 0.0f;      // raw screen pixel: width
    float               m_scrH         = 0.0f;      // raw screen pixel: height

    // ---- Source content ----
    float               m_aspectRatio  = 16.0f / 10.0f;
    float               m_occupancy    = 0.7f;       // normalized occupancy factor
    int                 m_nativeSrcWidth  = 400;     // queried DWM source width (pixels)
    int                 m_nativeSrcHeight = 300;     // queried DWM source height (pixels)
    int                 m_srcWidth     = 400;        // thumbnail source width (pixels)
    int                 m_srcHeight    = 300;        // thumbnail source height (pixels)
    bool                m_isMinimized  = false;      // window is iconic
    bool                m_isShellDesktop = false;    // Progman / GetShellWindow()

    // ---- Carousel display slot ----
    // Continuous slot for rendering (index - scrollPos), decoupled across list wrap
    // while a card completes the front-edge opacity ramp (slot in [-1, 0]).
    float               m_displaySlot     = 0.0f;
    bool                m_displaySlotValid = false;
    CarouselWrapPhase   m_wrapPhase     = CarouselWrapPhase::None;
    float               m_wrapFadeStartListSlot = 0.0f; // list slot when EnteringBack began
    // Progress [0..1] of the wrap fade (entering from back or entering
    // front). Updated each frame so ComputeUpdateAlpha can drive a smooth
    // fade-in when a card wraps into view.
    float               m_wrapProgress = 0.0f;
    // Actual opacity used while a card is entering through a wrap.
    float               m_wrapOpacity = 1.0f;
    bool                m_wrapFadeActive = false;
    bool                m_opacityInitialized = false;
    float               m_lastTargetOpacity = 0.0f;
};
