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

#include "targetver.h"
#include <afxwin.h>
#include <ppl.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <assert.h>
#include <array>

#include "FrameProcessorCpuBase.h"
#include "utilities.h"

using namespace concurrency;

//--------------------------------------------------------------------------------------
//  Base class for frame processor for a CPU, single and multi core.
//--------------------------------------------------------------------------------------

void FrameProcessorCpuBase::ConfigureFrameBuffers(const Gdiplus::BitmapData& srcFrame)
{
    // Only recalculate frames if the heights have changed as this may be expensive.
    if ((m_height == srcFrame.Height) && (m_width == srcFrame.Width))
        return;
    m_height = srcFrame.Height;
    m_width = srcFrame.Width;

    ATLTRACE("Configure frame buffers: New image size %d x %d\n", m_height, m_width);
    const Gdiplus::Rect rect(0, 0, m_width, m_height);

    for (int i = 0; i < kBufSize; ++i)
    {
        m_frames[i] = std::make_shared<Gdiplus::BitmapData>();
        m_bitmaps[i] = std::make_shared<Gdiplus::Bitmap>(m_width, m_height, srcFrame.PixelFormat);

        Gdiplus::Status st = m_bitmaps[i]->LockBits(
            &rect,
            Gdiplus::ImageLockModeWrite,
            PixelFormat32bppARGB,
            m_frames[i].get());
        assert(st == Gdiplus::Ok);
    }
    auto frameMemSize = m_height * srcFrame.Stride;
    memcpy_s(m_frames[kCurrent]->Scan0, frameMemSize, srcFrame.Scan0, frameMemSize);
#if defined(_DEBUG)
    memset(m_frames[kNext]->Scan0, 0, m_frames[0]->Height * m_frames[0]->Stride);
#endif
}

void FrameProcessorCpuBase::ReleaseFrameBuffers()
{
    for (int i = 0; i < kBufSize; ++i)
    {
#if defined(_DEBUG)
        memset(m_frames[i]->Scan0, 0, m_frames[i]->Height * m_frames[i]->Stride);
#endif
        m_bitmaps[i]->UnlockBits(m_frames[i].get());
    }
}

//--------------------------------------------------------------------------------------
//  Image processing functions used by all CPU image processors.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
//  Color simplifier.
//-------------------------------------------------------------------------------------- 
//
// Simplify a portion of the image by removing features that fall below a given color contrast.
// We do this by calculating the difference in color between neighboring pixels and applying an
// exponential decay function then summing the results.
// We then set the R, G, B values to the normalized sums

void FrameProcessorCpuBase::ApplyColorSimplifierSingle(const Gdiplus::BitmapData& srcFrame, Gdiplus::BitmapData& destFrame, UINT neighborWindow,
                                             UINT startWidth, UINT startHeight, UINT endWidth, UINT endHeight)
{
    for(UINT y = startHeight; y < endHeight; ++y)
    {
        for(UINT x = startWidth; x < endWidth; ++x)
            SimplifyIndex(srcFrame, destFrame, neighborWindow, x, y);
    }
}

void FrameProcessorCpuBase::ApplyColorSimplifierMulti(const Gdiplus::BitmapData& srcFrame, Gdiplus::BitmapData& destFrame, UINT neighborWindow,
                                             UINT startWidth, UINT startHeight, UINT endWidth, UINT endHeight)
{
    parallel_for(startHeight, endHeight, [=, &srcFrame, &destFrame](UINT y)
    {
        for(UINT x = startWidth; x < endWidth; ++x)
            SimplifyIndex(srcFrame, destFrame, neighborWindow, x, y);
    });
}

void FrameProcessorCpuBase::SimplifyIndex(const Gdiplus::BitmapData& srcFrame, Gdiplus::BitmapData& destFrame, UINT neighborWindow, int idxX, int idxY)
{
    const UINT bpp = Gdiplus::GetPixelFormatSize(srcFrame.PixelFormat);
    COLORREF orgClr =  BitmapUtils::GetPixel(static_cast<byte*>(srcFrame.Scan0), idxX, idxY, srcFrame.Stride, bpp);

    const int shift = neighborWindow / 2;
    float sum = 0;
    float partialSumR = 0, partialSumG = 0, partialSumB = 0;
    const float standardDeviation = 0.025f;
    // k is the exponential decay constant and is calculated from a standard deviation of 0.025
    const float k = -0.5f / (standardDeviation * standardDeviation);

    for (int y = (idxY - shift); y <= (idxY + shift); ++y)
        for (int x = (idxX - shift); x <= (idxX + shift); ++x)
        {
            if (x == idxX && y == idxY) // don't apply filter to the requested index, only to the neighbors
                continue;

            COLORREF clr = BitmapUtils::GetPixel(static_cast<byte*>(srcFrame.Scan0), x, y, srcFrame.Stride, bpp);
            const float distance = ImageUtils::GetDistance(orgClr, clr);
            const float value = pow(float(M_E), k * distance * distance);
            sum += value;
            partialSumR += GetRValue(clr) * value;
            partialSumG += GetGValue(clr) * value;
            partialSumB += GetBValue(clr) * value;
        }

    int newR, newG, newB;
    newR = static_cast<int>(std::min(std::max(partialSumR / sum, 0.0f), 255.0f));
    newG = static_cast<int>(std::min(std::max(partialSumG / sum, 0.0f), 255.0f));
    newB = static_cast<int>(std::min(std::max(partialSumB / sum, 0.0f), 255.0f));

    BitmapUtils::SetPixel(static_cast<byte*>(destFrame.Scan0), idxX, idxY, destFrame.Stride, bpp, RGB(newR, newG, newB));
}

//--------------------------------------------------------------------------------------
//  Edge detection.
//--------------------------------------------------------------------------------------

void FrameProcessorCpuBase::ApplyEdgeDetectionSingle(const Gdiplus::BitmapData& srcFrame, Gdiplus::BitmapData& destFrame, 
                                           const Gdiplus::BitmapData& orgFrame, UINT startWidth, UINT startHeight, UINT endWidth, UINT endHeight)
{
    const UINT bpp = Gdiplus::GetPixelFormatSize(srcFrame.PixelFormat);

    const float alpha = 0.3f;       // Weighting of original frame for edge detection
    const float beta = 0.8f;        // Weighting of source (color simplied) frame for edge detection

    const float s0 = 0.054f;        // Minimum Threshold of source frame Sobel value to detect an edge
    const float s1 = 0.064f;        // Maximum Threshold of source frame Sobel value to effect the darkness of the edge
    const float a0 = 0.3f;          // Minimum Threshold of original frame Sobel value to detect an edge
    const float a1 = 0.7f;          // Maximum Threshold of original frame Sobel value to effect the darkness of the edge

    for (UINT y = startHeight; y < endHeight; ++y)
        for (UINT x = startWidth; x < endWidth; ++x)
        {
            float Sy, Su, Sv;
            float Ay, Au, Av;

            CalculateSobel(srcFrame, x, y, Sy, Su, Sv);
            CalculateSobel(orgFrame, x, y, Ay, Au, Av); 

            float edgeS = (1 - alpha) * Sy + alpha * (Su + Sv) / 2;
            float edgeA = (1 - alpha) * Ay + alpha * (Au + Av) / 2;
            float i = (1 - beta) * ImageUtils::SmoothStep(s0, s1, edgeS) + beta * ImageUtils::SmoothStep(a0, a1, edgeA);

            float oneMinusi = 1 - i;
            COLORREF srcClr = BitmapUtils::GetPixel(static_cast<byte*>(srcFrame.Scan0), x, y, srcFrame.Stride, bpp);
            COLORREF destClr = RGB(GetRValue(srcClr) * oneMinusi, GetGValue(srcClr) * oneMinusi, GetBValue(srcClr) * oneMinusi);
            BitmapUtils::SetPixel(static_cast<byte*>(destFrame.Scan0), x, y, destFrame.Stride, bpp, destClr);            
        }
}

void FrameProcessorCpuBase::ApplyEdgeDetectionMulti(const Gdiplus::BitmapData& srcFrame, Gdiplus::BitmapData& destFrame, 
                                           const Gdiplus::BitmapData& orgFrame, UINT startWidth, UINT startHeight, UINT endWidth, UINT endHeight)
{
    const UINT bpp = Gdiplus::GetPixelFormatSize(srcFrame.PixelFormat);

    const float alpha = 0.3f;       // Weighting of original frame for edge detection
    const float beta = 0.8f;        // Weighting of source (color simplied) frame for edge detection

    const float s0 = 0.054f;        // Minimum Threshold of source frame Sobel value to detect an edge
    const float s1 = 0.064f;        // Maximum Threshold of source frame Sobel value to effect the darkness of the edge
    const float a0 = 0.3f;          // Minimum Threshold of original frame Sobel value to detect an edge
    const float a1 = 0.7f;          // Maximum Threshold of original frame Sobel value to effect the darkness of the edge

    parallel_for(startHeight, endHeight,  [&srcFrame, &destFrame, &orgFrame, startWidth, endWidth, alpha, beta, s0, s1, a0, a1, bpp](int y)
    {
        for (UINT x = startWidth; x < endWidth; ++x)
        {
            float Sy, Su, Sv;
            float Ay, Au, Av;

            CalculateSobel(srcFrame, x, y, Sy, Su, Sv);
            CalculateSobel(orgFrame, x, y, Ay, Au, Av); 

            float edgeS = (1 - alpha) * Sy + alpha * (Su + Sv) / 2;
            float edgeA = (1 - alpha) * Ay + alpha * (Au + Av) / 2;
            float i = (1 - beta) * ImageUtils::SmoothStep(s0, s1, edgeS) + beta * ImageUtils::SmoothStep(a0, a1, edgeA);

            float oneMinusi = 1 - i;
            COLORREF srcClr = BitmapUtils::GetPixel(static_cast<byte*>(srcFrame.Scan0), x, y, srcFrame.Stride, bpp);
            COLORREF destClr = RGB(GetRValue(srcClr) * oneMinusi, GetGValue(srcClr) * oneMinusi, GetBValue(srcClr) * oneMinusi);
            BitmapUtils::SetPixel(static_cast<byte*>(destFrame.Scan0), x, y, destFrame.Stride, bpp, destClr);
        }
    });
}

void FrameProcessorCpuBase::CalculateSobel(const Gdiplus::BitmapData& srcFrame, int idxX, int idxY, float& dy, float& du, float& dv)
{
    const UINT bpp = Gdiplus::GetPixelFormatSize(srcFrame.PixelFormat);

    // Apply the Sobel operator to the image.  The Sobel operation is analogous
    // to a first derivative of the grayscale part of the image.  Portions of
    // the image that change rapidly (edges) have higher sobel values.

    // Gx is the matrix used to calculate the horizontal gradient of image
    // Gy is the matrix used to calculate the vertical gradient of image
    int gx[3][3] = { { -1, 0, 1 }, { -2, 0, 2 }, { -1, 0, 1 } };        //  The matrix Gx
    int gy[3][3] = { {  1, 2, 1 }, {  0, 0, 0 }, { -1, -2, -1 } };      //  The matrix Gy

    float new_yX = 0, new_yY = 0;
    float new_uX = 0, new_uY = 0;
    float new_vX = 0, new_vY = 0;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
        {
            const int gX = gx[x + 1][y + 1];
            const int gY = gy[x + 1][y + 1];
            float clrY, clrU, clrV;

            ImageUtils::RGBToYUV(BitmapUtils::GetPixel(static_cast<byte*>(srcFrame.Scan0), idxX + x, idxY + y, srcFrame.Stride, bpp), clrY, clrU, clrV);

            new_yX += gX * clrY;
            new_yY += gY * clrY;
            new_uX += gX * clrU;
            new_uY += gY * clrU;
            new_vX += gX * clrV;
            new_vY += gY * clrV;
        }

    // Calculate the magnitude of the gradient from the horizontal and vertical gradients
    dy = sqrt((new_yX * new_yX) + (new_yY * new_yY));
    du = sqrt((new_uX * new_uX) + (new_uY * new_uY));
    dv = sqrt((new_vX * new_vX) + (new_vY * new_vY));
}
