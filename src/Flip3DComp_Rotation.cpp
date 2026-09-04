// ============================================================================
// Flip3DComp_Rotation.cpp — uDWM-style discrete list rotation during exit
// ============================================================================
#include "Flip3DComp.h"

#include <algorithm>
#include <cmath>

namespace
{
struct RotationContext
{
    int   countInt      = 0;
    int   visibleCount  = 0;
    bool  hasHidden     = false;
    bool  isRotating    = false;
    int   rotationSteps = 0;   // +1 front→back, -1 back→front, 0 idle
    float rotationProgress = 1.0f;
    float hiddenFlipOut  = 1.0f;
    float hiddenFlipIn   = 0.0f;
    int   rotatingOutIndex = -1;
    int   rotatingInIndex  = -1;
    int   flipCardIndex    = -1;
    bool  useHiddenRotation = false;
    bool  showOutgoing      = false;
};

struct CardRotationEntry
{
    int  listIndex   = 0;
    int  startSlot   = 0;
    int  endSlot     = 0;
    bool isOutgoing  = false;
    bool isIncoming  = false;
    bool isBoundaryIn = false;
    bool isCycle     = false;
    bool shouldDraw  = false;
};

RotationContext BuildRotationContext(int countInt, bool isRotating,
                                     bool rotateBackward, float rotationProgress,
                                     bool showOutgoing)
{
    RotationContext ctx = {};
    ctx.countInt = countInt;
    if (ctx.countInt <= 0)
        return ctx;

    ctx.visibleCount = std::min(kMaxVisibleCards, ctx.countInt);
    ctx.hasHidden    = ctx.countInt > ctx.visibleCount;
    ctx.isRotating   = isRotating;
    ctx.rotationSteps = ctx.isRotating ? (rotateBackward ? -1 : 1) : 0;
    ctx.rotationProgress = rotationProgress;
    ctx.hiddenFlipOut = std::clamp(ctx.rotationProgress * 2.0f, 0.0f, 1.0f);
    ctx.hiddenFlipIn  = std::clamp((ctx.rotationProgress - 0.5f) * 2.0f, 0.0f, 1.0f);
    ctx.showOutgoing  = showOutgoing;

    if (ctx.rotationSteps != 0)
    {
        if (ctx.hasHidden)
        {
            if (rotateBackward)
            {
                ctx.rotatingOutIndex = ctx.visibleCount;
                ctx.rotatingInIndex  = 0;
                ctx.flipCardIndex    = ctx.rotatingInIndex;
            }
            else
            {
                ctx.rotatingOutIndex = ctx.countInt - 1;
                ctx.rotatingInIndex  = ctx.visibleCount - 1;
                ctx.flipCardIndex    = ctx.rotatingOutIndex;
            }
        }
        else
        {
            const int cycleIndex = rotateBackward ? 0 : (ctx.countInt - 1);
            ctx.rotatingInIndex  = cycleIndex;
            ctx.rotatingOutIndex = cycleIndex;
            ctx.flipCardIndex    = cycleIndex;
        }
        ctx.useHiddenRotation = ctx.hasHidden;
    }

    return ctx;
}

float SteadyOpacityForRelative(const RotationContext& ctx, float relative)
{
    if (!ctx.hasHidden)
        return 1.0f;
    if (relative < 0.0f)
        return 1.0f;
    if (relative >= (float)ctx.visibleCount)
        return 0.5f;
    return (relative >= (float)(ctx.visibleCount - 1)) ? 0.5f : 1.0f;
}

bool IsVisibleInRotationView(const RotationContext& ctx, int position,
                             bool isVisibleInFinalView)
{
    if (isVisibleInFinalView)
    {
        if (position == ctx.rotatingInIndex)
        {
            if (!ctx.showOutgoing)
                return true;
        }
        else if (position != ctx.rotatingOutIndex)
        {
            return true;
        }
    }
    else if (position == ctx.rotatingInIndex)
    {
        if (!ctx.showOutgoing)
            return true;
    }

    if (position == ctx.rotatingOutIndex && ctx.showOutgoing)
        return true;

    return false;
}

CardRotationEntry BuildCardRotationEntry(int listIndex, const RotationContext& ctx)
{
    CardRotationEntry entry = {};
    entry.listIndex = listIndex;
    entry.endSlot   = listIndex;

    const int n   = ctx.countInt;
    const int vis = ctx.visibleCount;
    entry.startSlot = listIndex;
    if (ctx.rotationSteps != 0)
    {
        if (ctx.rotationSteps < 0)
            entry.startSlot = (listIndex + n - 1) % n;
        else
            entry.startSlot = (listIndex + 1) % n;
    }

    const bool isVis  = entry.endSlot < vis;
    const bool wasVis = entry.startSlot < vis;

    if (!ctx.isRotating)
    {
        entry.shouldDraw = isVis;
        return entry;
    }

    entry.isOutgoing = wasVis && !isVis;
    entry.isBoundaryIn = ctx.useHiddenRotation && !wasVis && isVis;
    entry.isIncoming = !ctx.useHiddenRotation && !wasVis && isVis;
    entry.isCycle = !ctx.hasHidden
        && ((ctx.rotationSteps < 0 && listIndex == 0)
            || (ctx.rotationSteps > 0 && listIndex == n - 1));

    entry.shouldDraw = IsVisibleInRotationView(ctx, listIndex, isVis);
    return entry;
}

float ResolveRotationPathSlot(const CardRotationEntry& entry, const RotationContext& ctx)
{
    if (!ctx.isRotating || ctx.rotationSteps == 0)
        return (float)entry.endSlot;

    const float p = ctx.rotationProgress;

    if (entry.isCycle)
    {
        if (ctx.rotationSteps > 0)
        {
            if (p < 0.5f)
                return (1.0f - p * 2.0f) * 0.0f + (p * 2.0f) * -1.0f;
            {
                const float phase = (p - 0.5f) * 2.0f;
                return (1.0f - phase) * (float)ctx.visibleCount
                     + phase * (float)(ctx.visibleCount - 1);
            }
        }
        if (p < 0.5f)
            return ((1.0f - p * 2.0f) * (float)(ctx.visibleCount - 1))
                 + (p * 2.0f) * (float)ctx.visibleCount;
        return ((p - 0.5f) * 2.0f) * 0.0f
             + (1.0f - (p - 0.5f) * 2.0f) * -1.0f;
    }

    if (entry.isOutgoing)
    {
        const float start = (ctx.rotationSteps > 0) ? 0.0f : (float)(ctx.visibleCount - 1);
        const float end   = (ctx.rotationSteps > 0) ? -1.0f : (float)ctx.visibleCount;
        return start + (end - start) * p;
    }

    if (entry.isBoundaryIn || entry.isIncoming)
    {
        const float start = (ctx.rotationSteps > 0) ? (float)ctx.visibleCount : -1.0f;
        const float end   = (float)entry.endSlot;
        return start + (end - start) * p;
    }

    float startRelative = (float)entry.startSlot;
    const float endRelative = (float)entry.endSlot;

    if (ctx.useHiddenRotation)
    {
        startRelative = endRelative + ((ctx.rotationSteps > 0) ? 1.0f : -1.0f);
        return startRelative + (endRelative - startRelative) * p;
    }

    if (ctx.rotationSteps < 0 && entry.startSlot > entry.endSlot)
        startRelative -= (float)ctx.countInt;

    return startRelative + (endRelative - startRelative) * p;
}

float ResolveRotationTransitionOpacity(const CardRotationEntry& entry,
                                       const RotationContext& ctx)
{
    if (!ctx.isRotating || ctx.rotationSteps == 0)
        return 1.0f;

    if (entry.isCycle)
    {
        if (ctx.rotationProgress < 0.5f)
            return 1.0f - ctx.rotationProgress * 2.0f;
        return (ctx.rotationProgress - 0.5f) * 2.0f;
    }
    if (entry.isOutgoing)
    {
        if (ctx.useHiddenRotation)
            return 1.0f - ctx.hiddenFlipOut;
        return 1.0f - 2.0f * std::min(ctx.rotationProgress, 0.5f);
    }
    if (entry.isBoundaryIn || entry.isIncoming)
    {
        if (ctx.useHiddenRotation)
            return ctx.hiddenFlipIn;
        return std::clamp((ctx.rotationProgress - 0.5f) * 2.0f, 0.0f, 1.0f);
    }
    return 1.0f;
}

float ResolveRotationWindowOpacity(const CardRotationEntry& entry,
                                   const RotationContext& ctx, float pathSlot)
{
    if (!ctx.isRotating || ctx.rotationSteps == 0)
        return SteadyOpacityForRelative(ctx, pathSlot);

    const float startSteady = SteadyOpacityForRelative(ctx, (float)entry.startSlot);
    const float endSteady   = SteadyOpacityForRelative(ctx, (float)entry.endSlot);

    if (entry.isOutgoing)
        return startSteady;
    if (entry.isIncoming)
        return endSteady;

    if (ctx.useHiddenRotation)
    {
        if (ctx.rotationSteps > 0)
        {
            if (entry.endSlot >= ctx.visibleCount - 1)
                return 0.5f;
            if (entry.endSlot == ctx.visibleCount - 2)
                return 0.5f + (1.0f - 0.5f) * ctx.rotationProgress;
            return 1.0f;
        }
        if (entry.endSlot == ctx.visibleCount - 1)
            return 1.0f + (0.5f - 1.0f) * ctx.rotationProgress;
        return 1.0f;
    }

    return startSteady + (endSteady - startSteady) * ctx.rotationProgress;
}

} // namespace

// ============================================================================
int Flip3DCompApp::DistanceBetween(size_t sourcePos, size_t targetPos,
                                   bool forward) const
{
    const size_t count = m_cards.size();
    if (count <= 1)
        return 0;

    size_t dist = 0;
    size_t cur  = sourcePos;
    while (cur != targetPos)
    {
        if (forward)
            cur = (cur + count - 1) % count;
        else
            cur = (cur + 1) % count;
        ++dist;
    }
    return (int)dist;
}

float Flip3DCompApp::RotationDurationForRotateList() const
{
    if (m_state == ViewState::ExitRepeatedRotate)
        return std::max(std::abs(m_rRepeatedRotateRate), 0.005f);
    return kRotateListDurationSec;
}

void Flip3DCompApp::StartRotationStep(bool backward, float durationSec)
{
    m_rotateBackward             = backward;
    m_showOutgoingDuringRotation = true;
    m_rotateTimeline.Restart(0.0f, 1.0f, durationSec, InterpolationMode::Linear);
}

void Flip3DCompApp::OnRotationTimeUpdated()
{
    if (m_showOutgoingDuringRotation && m_rotateTimeline.IsActive()
        && m_rotateTimeline.RawProgress() > 0.5f)
    {
        m_showOutgoingDuringRotation = false;
    }
}

void Flip3DCompApp::TickRepeatedRotate()
{
    if (m_rotateTimeline.IsActive() || m_cards.empty())
        return;

    if (m_state == ViewState::ExitRepeatedRotate)
    {
        if (m_repeatedRotateStepsRemaining > 0)
        {
            constexpr bool backward = false;
            RotateListPhysically(backward);
            --m_repeatedRotateStepsRemaining;
            StartRotationStep(backward, RotationDurationForRotateList());

            for (int k = 0; k < (int)m_cards.size(); ++k)
            {
                m_cards[(size_t)k].m_displaySlot =
                    ComputeRotationDisplaySlot(k);
            }
        }
        else
        {
            m_rRepeatedRotateRate = 0.0f;
            m_exitScrollSnapshot  = 0.0f;
            m_scrollPos           = 0.0f;
            m_scrollTarget        = 0.0f;
        }
        return;
    }
}

float Flip3DCompApp::ComputeRotationDisplaySlot(int listIndex) const
{
    const bool isRotating = m_rotateTimeline.IsActive();
    const RotationContext ctx = BuildRotationContext(
        (int)m_cards.size(), isRotating, m_rotateBackward,
        isRotating ? m_rotateTimeline.RawProgress() : 1.0f,
        m_showOutgoingDuringRotation);
    if (!ctx.isRotating)
        return CardListSlot(listIndex);

    const CardRotationEntry entry = BuildCardRotationEntry(listIndex, ctx);
    const float scrollOffset = (m_state == ViewState::ExitRepeatedRotate)
        ? m_exitScrollSnapshot
        : m_scrollPos;
    return ResolveRotationPathSlot(entry, ctx) - scrollOffset;
}

bool Flip3DCompApp::ShouldDrawRotationCard(int listIndex) const
{
    const bool isRotating = m_rotateTimeline.IsActive();
    const RotationContext ctx = BuildRotationContext(
        (int)m_cards.size(), isRotating, m_rotateBackward,
        isRotating ? m_rotateTimeline.RawProgress() : 1.0f,
        m_showOutgoingDuringRotation);
    if (!ctx.isRotating)
        return listIndex < ctx.visibleCount;

    const CardRotationEntry entry = BuildCardRotationEntry(listIndex, ctx);
    return entry.shouldDraw;
}

float Flip3DCompApp::ComputeRotationAlpha(const CardModel& card, float enterProgress,
                                          int listIndex, float carouselSlot) const
{
    const bool isRotating = m_rotateTimeline.IsActive();
    const RotationContext ctx = BuildRotationContext(
        (int)m_cards.size(), isRotating, m_rotateBackward,
        isRotating ? m_rotateTimeline.RawProgress() : 1.0f,
        m_showOutgoingDuringRotation);
    const CardRotationEntry entry = BuildCardRotationEntry(listIndex, ctx);

    if (!entry.shouldDraw && ctx.isRotating)
        return 0.0f;

    const float transitionOpacity = ResolveRotationTransitionOpacity(entry, ctx);
    float windowOpacity = ResolveRotationWindowOpacity(entry, ctx, carouselSlot);

    if (std::abs(enterProgress - 1.0f) >= 1e-6f
        && std::abs(windowOpacity - 1.0f) >= 1e-6f)
    {
        windowOpacity = enterProgress * windowOpacity + (1.0f - enterProgress);
    }

    float enterScale = 1.0f;
    if (std::abs(enterProgress - 1.0f) >= 1e-6f)
    {
        if (card.m_isMinimized)
            enterScale = enterProgress * enterProgress;
        else if (card.m_isShellDesktop)
            enterScale = enterProgress;
    }

    return std::clamp(transitionOpacity * windowOpacity * enterScale, 0.0f, 1.0f);
}
