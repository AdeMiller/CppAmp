//===============================================================================
//
// Microsoft Press
// C++ AMP: Accelerated Massive Parallelism with Microsoft Visual C++
//
//===============================================================================
// Copyright (c) 2012-2013 Ade Miller & Kate Gregory.  All rights reserved.
// This code released under the terms of the 
// Microsoft Public License (Ms-PL), http://ampbook.codeplex.com/license.
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//===============================================================================

#define _USE_MATH_DEFINES
#include <math.h>
#include <afx.h>
#include <minwindef.h>

#include "utilities.h"
#include "FrameProcessorAmpTextureSingle.h"

using namespace concurrency::direct3d;

// Visual Studio 2013 depricates writeonly_texture_view<N, T>. It is replaced with a new 
// texture_view<T, N> that implements additional functionality. The following blog post has
// additional details:
//
// http://blogs.msdn.com/b/nativeconcurrency/archive/2013/07/25/overview-of-the-texture-view-design-in-c-amp.aspx

//  Color simplifier

void ApplyColorSimplifier(const texture<uint_4, 2>& srcFrame, texture<uint_4, 2>& destFrame, UINT neighborWindow);
#if (_MSC_VER >= 1800)
void SimplifyIndex(const texture<uint_4, 2>& srcFrame, const texture_view<uint_4, 2>& destFrame, index<2> idx, UINT neighborWindow, const float_3& W) restrict(amp);
#else
void SimplifyIndex(const texture<uint_4, 2>& srcFrame, const writeonly_texture_view<uint_4, 2>& destFrame, index<2> idx, UINT neighborWindow, const float_3& W) restrict(amp);
#endif

//  Edge detection.

void ApplyEdgeDetection(const texture<uint_4, 2>& srcFrame, texture<uint_4, 2>& destFrame, 
    const texture<uint_4, 2>& orgFrame, UINT simplifierNeighborWindow);
#if (_MSC_VER >= 1800)
void DetectEdge(index<2> idx, const texture<uint_4, 2>& srcFrame, const texture_view<uint_4, 2>& destFrame,  
    const texture<uint_4, 2>& orgFrame, UINT simplifierNeighborWindow, const float_3& W) restrict(amp);
#else
void DetectEdge(index<2> idx, const texture<uint_4, 2>& srcFrame, const writeonly_texture_view<uint_4, 2>& destFrame,  
    const texture<uint_4, 2>& orgFrame, UINT simplifierNeighborWindow, const float_3& W) restrict(amp);
#endif
void CalculateSobel(const texture<uint_4, 2>& source, index<2> idx, float& dy, float& du, float& dv,  const float_3& W) restrict(amp);

//--------------------------------------------------------------------------------------
//  Frame processor for a single C++ AMP accelerator using textures.
//--------------------------------------------------------------------------------------

void FrameProcessorAmpTextureSingle::ProcessImage(const Gdiplus::BitmapData& srcFrame, 
    Gdiplus::BitmapData& destFrame, UINT phases, UINT simplifierNeighborWindow)
{
    assert(simplifierNeighborWindow % 2 == 0);
    assert(phases > 0);

    ConfigureFrameBuffers(srcFrame);

    int current = kCurrent;
    int next = kNext;
    const UINT frame_size = srcFrame.Stride * m_height;
    copy_async(srcFrame.Scan0, frame_size, *m_originalFrameView.get());
    m_frames[kOriginal]->copy_to(*m_frames[current].get());
    for (UINT i = 0; i < phases; ++i)
    {
        ApplyColorSimplifier(*m_frames[current].get(), *m_frames[next].get(), 
            simplifierNeighborWindow);
        std::swap(current, next);
    }

    ApplyEdgeDetection(
        *m_frames[current].get(), *m_frames[next].get(), *m_frames[kOriginal].get(), 
        simplifierNeighborWindow);
    std::swap(current, next);

    // Make sure that and preceeding kernel is finished before starting to copy data from the GPU and reduce the time copy may take a lock for.
    completion_future f =  copy_async(*m_frames[current].get(), destFrame.Scan0, frame_size);
    m_accelerator.default_view.wait();
    f.get();
}

void FrameProcessorAmpTextureSingle::ConfigureFrameBuffers(const Gdiplus::BitmapData& srcFrame)
{
    // Only recalculate frames if the heights have changed as this may be expensive.
    if ((m_height == srcFrame.Height) && (m_width == srcFrame.Width))
        return;
    m_height = srcFrame.Height;
    m_width = srcFrame.Width;
    ATLTRACE("Configure frame buffers: New image size %d x %d\n", m_height, m_width);

    std::generate(m_frames.begin(), m_frames.end(), [=]()->std::shared_ptr<texture<uint_4, 2>> 
    { 
        return std::make_shared<texture<uint_4, 2>>(int(m_height), int(m_width), 8u, 
            m_accelerator.default_view); 
    });
#if (_MSC_VER >= 1800)
    m_originalFrameView = std::unique_ptr<texture_view<uint_4, 2>>(
        new texture_view<uint_4, 2>(*m_frames[kOriginal].get()));
#else
    m_originalFrameView = std::unique_ptr<writeonly_texture_view<uint_4, 2>>(
        new writeonly_texture_view<uint_4, 2>(*m_frames[kOriginal].get()));
#endif
}

//--------------------------------------------------------------------------------------
//  Color simplifier.
//--------------------------------------------------------------------------------------
//
// Simplify a portion of the image by removing features that fall below a given color contrast.
// We do this by calculating the difference in color between neighboring pixels and applying an
// exponential decay function then summing the results.
// We then set the R, G, B values to the normalized sums

void ApplyColorSimplifier(const texture<uint_4, 2>& srcFrame, texture<uint_4, 2>& destFrame, UINT neighborWindow)
{
    const float_3 W(ImageUtils::W);

#if (_MSC_VER >= 1800)
    texture_view<uint_4, 2> destView(destFrame);
#else
    writeonly_texture_view<uint_4, 2> destView(destFrame);
#endif
    extent<2> computeDomain(srcFrame.extent - extent<2>(neighborWindow, neighborWindow));
    parallel_for_each(computeDomain, [=, &srcFrame](index<2> idx) restrict(amp)
    {
        SimplifyIndex(srcFrame, destView, idx, neighborWindow, W);
    });
}

#if (_MSC_VER >= 1800)
void SimplifyIndex(const texture<uint_4, 2>& srcFrame, const texture_view<uint_4, 2>& destFrame, index<2> idx, 
    UINT neighborWindow, const float_3& W) restrict(amp)
#else
void SimplifyIndex(const texture<uint_4, 2>& srcFrame, const writeonly_texture_view<uint_4, 2>& destFrame, index<2> idx, 
    UINT neighborWindow, const float_3& W) restrict(amp)
#endif
{
    const int shift = neighborWindow / 2;
    float sum = 0;
    float_3 partialSum;
    const float standardDeviation = 0.025f;
    // k is the exponential decay constant and is calculated from a standard deviation of 0.025
    const float k = -0.5f / (standardDeviation * standardDeviation);

    const int idxY = idx[0] + shift;         // Corrected index for border offset.
    const int idxX = idx[1] + shift;
    const int y_start = idxY - shift;
    const int y_end = idxY + shift;
    const int x_start = idxX - shift;
    const int x_end = idxX + shift;

    uint_4 orgClr = srcFrame(idxY, idxX);

    for (int y = y_start; y <= y_end; ++y)
        for (int x = x_start; x <= x_end; ++x)
        {
            if (x != idxX || y != idxY) // don't apply filter to the requested index, only to the neighbors
            {
                uint_4 clr = srcFrame(y, x);
                float distance = ImageUtils::GetDistance(orgClr, clr, W);
                float value = concurrency::fast_math::pow(float(M_E), k * distance * distance);
                sum += value;
                partialSum.r += clr.r * value;
                partialSum.g += clr.g * value;
                partialSum.b += clr.b * value;
            }
        }

        uint_4 newClr;
        newClr.r = static_cast<uint>(clamp(partialSum.r / sum, 0.0f, 255.0f));
        newClr.g = static_cast<uint>(clamp(partialSum.g / sum, 0.0f, 255.0f));
        newClr.b = static_cast<uint>(clamp(partialSum.b / sum, 0.0f, 255.0f));
        newClr.a = 0xFF;
        destFrame.set(index<2>(idxY, idxX), newClr);
}

//--------------------------------------------------------------------------------------
//  Edge detection.
//--------------------------------------------------------------------------------------
//
//  See the following Wikipedia page on the Canny edge detector for a further description 
//  of the algorithm.

void ApplyEdgeDetection(const texture<uint_4, 2>& srcFrame, 
    texture<uint_4, 2>& destFrame, const texture<uint_4, 2>& orgFrame, 
    UINT simplifierNeighborWindow)
{
    const float_3 W(ImageUtils::W);
    const float alpha = 0.3f;
    const float beta = 0.8f;
    const float s0 = 0.054f;
    const float s1 = 0.064f;
    const float a0 = 0.3f;
    const float a1 = 0.7f;
    extent<2> ext(srcFrame.extent - extent<2>(simplifierNeighborWindow, simplifierNeighborWindow));
#if (_MSC_VER >= 1800)
    texture_view<uint_4, 2> destView(destFrame);
#else
    writeonly_texture_view<uint_4, 2> destView(destFrame);
#endif
    extent<2> computeDomain(ext - extent<2>(FrameProcessorAmp::EdgeBorderWidth, FrameProcessorAmp::EdgeBorderWidth));
    parallel_for_each(computeDomain, 
        [=, &srcFrame, &orgFrame](index<2> idx) restrict(amp) 
    {
        DetectEdge(idx, srcFrame, destView, orgFrame, simplifierNeighborWindow, W);
    });
}

#if (_MSC_VER >= 1800)
void DetectEdge(index<2> idx, const texture<uint_4, 2>& srcFrame, 
    const texture_view<uint_4, 2>& destFrame, const texture<uint_4, 2>& orgFrame, 
    UINT simplifierNeighborWindow, const float_3& W) restrict(amp)
#else
void DetectEdge(index<2> idx, const texture<uint_4, 2>& srcFrame, 
    const writeonly_texture_view<uint_4, 2>& destFrame, const texture<uint_4, 2>& orgFrame, 
    UINT simplifierNeighborWindow, const float_3& W) restrict(amp)
#endif
{
    const float alpha = 0.3f;       // Weighting of original frame for edge detection
    const float beta = 0.8f;        // Weighting of source (color simplified) frame for edge detection
    const float s0 = 0.054f;        // Minimum Threshold of source frame Sobel value to detect an edge
    const float s1 = 0.064f;        // Maximum Threshold of source frame Sobel value to effect the darkness of the edge
    const float a0 = 0.3f;          // Minimum Threshold of original frame Sobel value to detect an edge
    const float a1 = 0.7f;          // Maximum Threshold of original frame Sobel value to effect the darkness of the edge
    const int neighborWindow = 2;
    const int offset = (simplifierNeighborWindow + neighborWindow) / 2;

    index<2> idc(idx[0] + offset, idx[1] + offset);  // Corrected index for border offset.
    float Sy, Su, Sv;
    float Ay, Au, Av;
    Sy = Su = Sv = 0.0f;
    Ay = Au = Av = 0.0f;
    CalculateSobel(srcFrame, idc, Sy, Su, Sv, W);
    CalculateSobel(orgFrame, idc, Ay, Au, Av, W);

    const float edgeS = (1 - alpha) * Sy + alpha * (Su + Sv) / 2;
    const float edgeA = (1 - alpha) * Ay + alpha * (Au + Av) / 2;
    const float i = (1 - beta) * 
        smoothstep(s0, s1, edgeS) + beta * smoothstep(a0, a1, edgeA);

    const uint_4 srcClr = srcFrame[idc];
    uint_4 destClr;
    const float oneMinusi = 1 - i;
    destClr.r = static_cast<uint>(srcClr.r * oneMinusi);
    destClr.g = static_cast<uint>(srcClr.g * oneMinusi);
    destClr.b = static_cast<uint>(srcClr.b * oneMinusi);
    destClr.a = 0xFF;
    destFrame.set(idc, destClr);
}

void CalculateSobel(const texture<uint_4, 2>& srcFrame, index<2> idx, 
    float& dy, float& du, float& dv,  const float_3& W) restrict(amp)
{
    // Apply the Sobel operator to the image.  The Sobel operation is analogous
    // to a first derivative of the grayscale part of the image.  Portions of
    // the image that change rapidly (edges) have higher sobel values.

    // Gx is the matrix used to calculate the horizontal gradient of image
    // Gy is the matrix used to calculate the vertical gradient of image
    const int gx[3][3] = { { -1, 0, 1 }, { -2, 0, 2 }, { -1,  0,  1 } };      //  The matrix Gx
    const int gy[3][3] = { {  1, 2, 1 }, {  0, 0, 0 }, { -1, -2, -1 } };      //  The matrix Gy

    float new_yX = 0, new_yY = 0, new_uX = 0, new_uY = 0, new_vX = 0, new_vY = 0;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++)
        {
            const int gX = gx[x + 1][y + 1];
            const int gY = gy[x + 1][y + 1];
            float clrY, clrU, clrV;
            index<2> idxNew(idx[0] + x, idx[1] + y);
            ImageUtils::RGBToYUV(srcFrame[idxNew], clrY, clrU, clrV, W);

            new_yX += gX * clrY;
            new_yY += gY * clrY;
            new_uX += gX * clrU;
            new_uY += gY * clrU;
            new_vX += gX * clrV;
            new_vY += gY * clrV;
        }

        dy = fast_math::sqrt((new_yX * new_yX) + (new_yY * new_yY));
        du = fast_math::sqrt((new_uX * new_uX) + (new_uY * new_uY));
        dv = fast_math::sqrt((new_vX * new_vX) + (new_vY * new_vY));
}
