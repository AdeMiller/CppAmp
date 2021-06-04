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

#include <amp.h>
#include <assert.h>
#include <array>

#include "IFrameProcessor.h"

using namespace concurrency;

class FrameProcessorAmpBase : public IFrameProcessor
{
private:
    accelerator m_accelerator;
    std::array<std::shared_ptr<array<ArgbPackedPixel, 2>>, 3> m_frames;
    UINT m_height;
    UINT m_width;

public:
    FrameProcessorAmpBase(const accelerator& accel) :
        m_accelerator(accel),
        m_height(0),
        m_width(0)
    {
    }

    virtual inline void ApplyEdgeDetection(
        const array<ArgbPackedPixel, 2>& srcFrame, array<ArgbPackedPixel, 2>& destFrame, 
        const array<ArgbPackedPixel, 2>& orgFrame, UINT simplifierNeighborWindow) = 0;
    virtual inline void ApplyColorSimplifier(
        const array<ArgbPackedPixel, 2>& srcFrame, array<ArgbPackedPixel, 2>& destFrame, 
        UINT neighborWindow) = 0;

    void ProcessImage(const Gdiplus::BitmapData& srcFrame, 
        Gdiplus::BitmapData& destFrame, 
        UINT phases, UINT simplifierNeighborWindow)
    {
        assert(simplifierNeighborWindow % 2 == 0);
        assert(phases > 0);

        ConfigureFrameBuffers(srcFrame);

        int current = kCurrent;
        int next = kNext;
        CopyIn(srcFrame, *m_frames[current].get());
        m_frames[current]->copy_to(*m_frames[kOriginal].get());
        for (UINT i = 0; i < phases; ++i)
        {
            ApplyColorSimplifier(*m_frames[current].get(), 
                *m_frames[next].get(), simplifierNeighborWindow);
            std::swap(current, next);
        }

        ApplyEdgeDetection(*m_frames[current].get(), *m_frames[next].get(), 
             *m_frames[kOriginal].get(), simplifierNeighborWindow);
        std::swap(current, next);
        CopyOut(*m_frames[current].get(), destFrame);
    }

private:
    void ConfigureFrameBuffers(const Gdiplus::BitmapData& srcFrame)
    {
        // Only recalculate frames if the heights have changed as this may be expensive.
        if ((m_height == srcFrame.Height) && (m_width == srcFrame.Width))
            return;
        m_height = srcFrame.Height;
        m_width = srcFrame.Width;
        ATLTRACE("Configure frame buffers: New image size %d x %d\n", m_height, m_width);

        std::generate(m_frames.begin(), m_frames.end(), [=]()->std::shared_ptr<array<ArgbPackedPixel, 2>> 
        { 
            return std::make_shared<array<ArgbPackedPixel, 2>>(int(m_height), int(m_width), m_accelerator.default_view); 
        });
    }
};
