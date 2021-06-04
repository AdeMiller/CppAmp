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

#pragma once

#include "GdiWrap.h"
#include <array>

#include "IFrameProcessor.h"
#include "utilities.h"

class FrameProcessorCpuBase
{
protected:
    static const size_t kBufSize = 2;
    std::array<BitmapPtr, kBufSize> m_bitmaps;
    std::array<std::shared_ptr<Gdiplus::BitmapData>, kBufSize> m_frames;
    UINT m_height;
    UINT m_width;

    void ConfigureFrameBuffers(const Gdiplus::BitmapData& srcFrame);
    void FrameProcessorCpuBase::ReleaseFrameBuffers();

    //  Color simplifier.

    static void ApplyColorSimplifierSingle(const Gdiplus::BitmapData& srcFrame, Gdiplus::BitmapData& destFrame,
        UINT neighborWindow, UINT startWidth, UINT startHeight, UINT endWidth, UINT endHeight);

    static void ApplyColorSimplifierMulti(const Gdiplus::BitmapData& srcFrame, Gdiplus::BitmapData& destFrame,
        UINT neighborWindow, UINT startWidth, UINT startHeight, UINT endWidth, UINT endHeight);

    static void SimplifyIndex(const Gdiplus::BitmapData& srcFrame, Gdiplus::BitmapData& destFrame, UINT neighborWindow, int x, int y);

    //  Edge detection.

    static void ApplyEdgeDetectionSingle(const Gdiplus::BitmapData& srcFrame, Gdiplus::BitmapData& destFrame,
        const Gdiplus::BitmapData& orgFrame, UINT startWidth, UINT startHeight, UINT endWidth, UINT endHeight);

    static void ApplyEdgeDetectionMulti(const Gdiplus::BitmapData& srcFrame, Gdiplus::BitmapData& destFrame,
        const Gdiplus::BitmapData& orgFrame, UINT startWidth, UINT startHeight, UINT endWidth, UINT endHeight);

    static void CalculateSobel(const Gdiplus::BitmapData& srcFrame, int idxX, int idxY, float& dy, float& du, float& dv);
};
