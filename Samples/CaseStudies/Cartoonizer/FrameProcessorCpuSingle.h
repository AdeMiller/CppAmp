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

#include "FrameProcessorCpuBase.h"
#include "utilities.h"

class FrameProcessorCpuSingle : public FrameProcessorCpuBase, public IFrameProcessor
{
    void ProcessImage(const Gdiplus::BitmapData& srcFrame, Gdiplus::BitmapData& destFrame, 
        UINT phases, UINT neighborWindow)
    {
        assert(srcFrame.Height == destFrame.Height);
        assert(srcFrame.Stride == destFrame.Stride);
        assert(phases > 0);
        assert(neighborWindow > 0);

        ConfigureFrameBuffers(srcFrame);

        //  Process the image. After each step swap the frame buffer indices.

        int current = kCurrent;
        int next = kNext;
        UINT shift = neighborWindow / 2;

        for (UINT i = 0; i < phases; ++i)
        {
            ApplyColorSimplifierSingle(*m_frames[current].get(), *m_frames[next].get(), 
                neighborWindow, shift, shift, (srcFrame.Width - shift), 
                (srcFrame.Height - shift));
            std::swap(current, next);
        }

        ++shift;
        ApplyEdgeDetectionSingle(*m_frames[current].get(), destFrame, srcFrame,
            shift, shift, (srcFrame.Width - shift), (srcFrame.Height - shift));

        ReleaseFrameBuffers();
    }
};
