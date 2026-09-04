// ============================================================================
// FlipMath.h — 3D math utilities (row-vector DirectComposition convention)
// ============================================================================
// All functions are inline to match the zero-overhead pattern from CFlip3D.h.
// Matrix multiplication follows DComp's DX-style 4×4 layout.
// ============================================================================
#pragma once

#include <algorithm>
#include <cmath>
#include "Config.h"
#include "MathTypes.h"

namespace Math
{

// ---- Identity / basic transforms ----

inline Matrix4x4 Identity()
{
    return { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
}

inline Matrix4x4 Translation(float x, float y, float z)
{
    return { 1,0,0,0, 0,1,0,0, 0,0,1,0, x,y,z,1 };
}

inline Matrix4x4 Scale(float sx, float sy, float sz)
{
    return { sx,0,0,0, 0,sy,0,0, 0,0,sz,0, 0,0,0,1 };
}

// ---- Composition (must precede Rotation — used by RotationRollPitchYaw) ----

inline Matrix4x4 Multiply(const Matrix4x4& a, const Matrix4x4& b)
{
    Matrix4x4 r = {};
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            r.m[i][j] = a.m[i][0] * b.m[0][j]
                      + a.m[i][1] * b.m[1][j]
                      + a.m[i][2] * b.m[2][j]
                      + a.m[i][3] * b.m[3][j];
        }
    }
    return r;
}

inline Matrix4x4 Transpose(const Matrix4x4& m)
{
    Matrix4x4 r = {};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            r.m[i][j] = m.m[j][i];
    return r;
}

// ---- Rotation ----

inline Matrix4x4 RotationYPR(float yaw, float pitch, float roll)
{
    float cy = cos(yaw),  sy = sin(yaw);
    float cp = cos(pitch), sp = sin(pitch);
    float cr = cos(roll),  sr = sin(roll);
    return {
        cr*cy + sr*sp*sy, sr*cp, -cr*sy + sr*sp*cy, 0,
       -sr*cy + cr*sp*sy, cr*cp,  sr*sy + cr*sp*cy, 0,
        cp*sy,            -sp,    cp*cy,             0,
        0, 0, 0, 1
    };
}

// Matches XMMatrixRotationRollPitchYaw(Pitch, Yaw, Roll) = Rz(Roll)*Rx(Pitch)*Ry(Yaw)
// Row-vector: v' = v * Ry * Rx * Rz
inline Matrix4x4 RotationRollPitchYaw(float pitch, float yaw, float roll)
{
    float cp = cos(pitch), sp = sin(pitch);
    float cy = cos(yaw),   sy = sin(yaw);
    float cr = cos(roll),  sr = sin(roll);

    Matrix4x4 Ry = { cy, 0, -sy, 0,  0, 1, 0, 0,  sy, 0, cy, 0,  0,0,0,1 };
    Matrix4x4 Rx = { 1, 0, 0, 0,  0, cp, sp, 0,  0, -sp, cp, 0,  0,0,0,1 };
    Matrix4x4 Rz = { cr, sr, 0, 0,  -sr, cr, 0, 0,  0, 0, 1, 0,  0,0,0,1 };

    Matrix4x4 RzRx = Multiply(Rz, Rx);
    return Multiply(RzRx, Ry);
}

// ---- Camera matrices ----

inline Matrix4x4 LookAtRH(Vec3 eye, Vec3 at, Vec3 up)
{
    Vec3 z = { eye.x - at.x, eye.y - at.y, eye.z - at.z };
    float zl = 1.0f / sqrt(z.x*z.x + z.y*z.y + z.z*z.z);
    z.x *= zl; z.y *= zl; z.z *= zl;

    Vec3 x = { up.y*z.z - up.z*z.y, up.z*z.x - up.x*z.z, up.x*z.y - up.y*z.x };
    float xl = 1.0f / sqrt(x.x*x.x + x.y*x.y + x.z*x.z);
    x.x *= xl; x.y *= xl; x.z *= xl;

    Vec3 y = { z.y*x.z - z.z*x.y, z.z*x.x - z.x*x.z, z.x*x.y - z.y*x.x };

    return {
        x.x, y.x, z.x, 0,
        x.y, y.y, z.y, 0,
        x.z, y.z, z.z, 0,
        -(x.x*eye.x + x.y*eye.y + x.z*eye.z),
        -(y.x*eye.x + y.y*eye.y + y.z*eye.z),
        -(z.x*eye.x + z.y*eye.y + z.z*eye.z),
        1
    };
}

inline Matrix4x4 PerspectiveRH(float nearExtent, float aspect, float nearZ, float farZ)
{
    float w = nearExtent;
    float h = nearExtent / aspect;
    float q = farZ / (nearZ - farZ);
    return {
        2.0f * nearZ / w, 0,              0,  0,
        0,                2.0f * nearZ / h, 0,  0,
        0, 0, q, -1,
        0, 0, q * nearZ, 0
    };
}

// ---- Bezier & interpolation ----

inline Vec3 Bezier(float t)
{
    float s = 1.0f - t;
    return {
        s*s*kBezierControls[0][0] + 2*s*t*kBezierControls[1][0] + t*t*kBezierControls[2][0],
        s*s*kBezierControls[0][1] + 2*s*t*kBezierControls[1][1] + t*t*kBezierControls[2][1],
        s*s*kBezierControls[0][2] + 2*s*t*kBezierControls[1][2] + t*t*kBezierControls[2][2],
    };
}

inline float NormalizationScale(float o)
{
    o = std::clamp(o, 0.0f, 1.0f);
    float inv = 1.0f - o;
    return inv*inv * kNormalizationBezier[0]
         + 2.0f * kNormalizationBezier[1] * o * inv
         + o*o   * kNormalizationBezier[2];
}

// uDWM CFlip3D::NormalizeWindowSize — pixel space fit + occupancy bezier.
inline void NormalizeWindowSize(float& w, float& h, float monW, float monH)
{
    monW = std::max(monW, 1.0f);
    monH = std::max(monH, 1.0f);

    if (w > monW || h > monH)
    {
        const float s = std::min(monW / w, monH / h);
        w *= s;
        h *= s;
    }

    float o = w / monW;
    const float oy = h / monH;
    if (oy > o)
        o = oy;

    const float ns = NormalizationScale(o);
    w *= ns;
    h *= ns;
}

// DWM thumbnail pixel size → monitor-world flat (2D) and carousel (3D) dimensions.
inline void WorldSizesFromThumbPixels(
    float thumbW, float thumbH,
    float normMonW, float normMonH,
    Vec2& flatSize, Vec2& targetSize, float& occupancy)
{
    thumbW = std::max(thumbW, 1.0f);
    thumbH = std::max(thumbH, 1.0f);

    flatSize = { thumbW / normMonW, thumbH / normMonH };

    float finalW = thumbW;
    float finalH = thumbH;
    float occW = finalW, occH = finalH;
    if (occW > normMonW || occH > normMonH)
    {
        const float s = std::min(normMonW / occW, normMonH / occH);
        occW *= s;
        occH *= s;
    }
    occupancy = std::clamp(std::max(occW / normMonW, occH / normMonH), 0.05f, 1.0f);
    NormalizeWindowSize(finalW, finalH, normMonW, normMonH);
    targetSize = { finalW / normMonW, -(finalH / normMonH) };
}

// uDWM GetFinalMinRect: taskbar button rect → small card above the button.
inline RECT BuildFinalMinRect(RECT minimizeRect, float aspectRatio)
{
    const float minW = (float)std::max(0L, minimizeRect.right - minimizeRect.left);
    const float finalW = minW * kFinalMinRectWidthPercentage;
    const float finalH = finalW * aspectRatio;
    const float finalLeft = (float)minimizeRect.left + finalW * kFinalMinRectLeftPercentage;
    const float finalTop  = (float)minimizeRect.top - finalH;

    RECT result = {};
    result.left   = (LONG)std::lround(finalLeft);
    result.top    = (LONG)std::lround(finalTop);
    result.right  = (LONG)std::lround(finalLeft + finalW);
    result.bottom = (LONG)std::lround(finalTop + finalH);
    return result;
}

inline float Lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

// CSS cubic-bezier(x1,y1,x2,y2): map linear time x in [0,1] to eased progress.
// Newton-Raphson first, then bisection (WebKit bezier-easing) for robustness.
inline float CubicBezierProgress(float x, float x1, float y1, float x2, float y2)
{
    x = std::clamp(x, 0.0f, 1.0f);
    if (x <= 0.0f) return 0.0f;
    if (x >= 1.0f) return 1.0f;

    auto sampleX = [&](float t) {
        const float inv = 1.0f - t;
        return 3.0f * inv * inv * t * x1
             + 3.0f * inv * t * t * x2
             + t * t * t;
    };
    auto sampleY = [&](float t) {
        const float inv = 1.0f - t;
        return 3.0f * inv * inv * t * y1
             + 3.0f * inv * t * t * y2
             + t * t * t;
    };
    auto sampleDx = [&](float t) {
        const float inv = 1.0f - t;
        return 3.0f * inv * inv * x1
             + 6.0f * inv * t * (x2 - x1)
             + 3.0f * t * t * (1.0f - x2);
    };

    constexpr float kEpsilon = 1e-6f;

    float t = x;
    for (int i = 0; i < 8; ++i)
    {
        const float cx = sampleX(t) - x;
        if (std::abs(cx) < kEpsilon)
            return sampleY(t);

        const float dx = sampleDx(t);
        if (std::abs(dx) < 1e-6f)
            break;

        t -= cx / dx;
    }

    float t0 = 0.0f;
    float t1 = 1.0f;
    t = x;
    for (int i = 0; i < 32; ++i)
    {
        t = (t0 + t1) * 0.5f;
        const float cx = sampleX(t);
        if (std::abs(cx - x) < kEpsilon)
            return sampleY(t);
        if (cx < x)
            t0 = t;
        else
            t1 = t;
    }

    return sampleY((t0 + t1) * 0.5f);
}

inline float TimelineEase(float t)
{
    return CubicBezierProgress(
        t,
        kTimelineBezierX1, kTimelineBezierY1,
        kTimelineBezierX2, kTimelineBezierY2);
}

inline Vec3 Lerp(const Vec3& a, const Vec3& b, float t)
{
    return { Lerp(a.x, b.x, t), Lerp(a.y, b.y, t), Lerp(a.z, b.z, t) };
}

// ---- Ray-triangle intersection (Möller–Trumbore) ----

// Returns distance along ray (t > 0) or -1.0f on miss.
inline float IntersectRayTriangle(
    Vec3 rayOrig, Vec3 rayDir,
    Vec3 v0, Vec3 v1, Vec3 v2)
{
    Vec3 e1 = { v1.x - v0.x, v1.y - v0.y, v1.z - v0.z };
    Vec3 e2 = { v2.x - v0.x, v2.y - v0.y, v2.z - v0.z };
    Vec3 h  = {
        rayDir.y * e2.z - rayDir.z * e2.y,
        rayDir.z * e2.x - rayDir.x * e2.z,
        rayDir.x * e2.y - rayDir.y * e2.x,
    };

    float a = e1.x*h.x + e1.y*h.y + e1.z*h.z;
    if (a > -1e-6f && a < 1e-6f) return -1.0f;

    float f = 1.0f / a;
    Vec3 s = { rayOrig.x - v0.x, rayOrig.y - v0.y, rayOrig.z - v0.z };
    float u = f * (s.x*h.x + s.y*h.y + s.z*h.z);
    if (u < 0.0f || u > 1.0f) return -1.0f;

    Vec3 q = {
        s.y*e1.z - s.z*e1.y,
        s.z*e1.x - s.x*e1.z,
        s.x*e1.y - s.y*e1.x,
    };
    float v = f * (rayDir.x*q.x + rayDir.y*q.y + rayDir.z*q.z);
    if (v < 0.0f || u + v > 1.0f) return -1.0f;

    float t = f * (e2.x*q.x + e2.y*q.y + e2.z*q.z);
    return (t > 1e-6f) ? t : -1.0f;
}

// uDWM GetMonitorToWorldTransform — primary rcWork (screen px) → centered world.
inline void MonitorToWorldTopLeft(float screenX, float screenY,
                                  float monX, float monY,
                                  float monW, float monH,
                                  float& worldX, float& worldY)
{
    monW = std::max(monW, 1.0f);
    monH = std::max(monH, 1.0f);
    worldX = (screenX - monX - monW * 0.5f) / monW;
    worldY = (monY + monH * 0.5f - screenY) / monH;
}

inline Matrix4x4 BuildViewportMatrix(float width, float height,
                                     float originX = 0.0f, float originY = 0.0f)
{
    return {
        width * 0.5f, 0,              0, 0,
        0,            -height * 0.5f, 0, 0,
        0,             0,             1, 0,
        originX + width * 0.5f, originY + height * 0.5f, 0, 1,
    };
}

} // namespace Math
