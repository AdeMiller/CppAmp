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
#include <vector>
#include <array>

#include "FrameProcessorAmp.h"
#include "IFrameProcessor.h"

//--------------------------------------------------------------------------------------
//  Frame processor for a multiple C++ AMP accelerators.
//--------------------------------------------------------------------------------------
//
//  This will execute using two or more hardware accelerators. It will ignore the software reference accelerator.
//
//  Data structure for tracking work running on each C++ AMP accelerator.

class TaskData
{
public:
    accelerator accel;
    UINT startHeight;
    UINT height;
    std::array<std::shared_ptr<array<ArgbPackedPixel, 2>>, 3> frames;

    TaskData(accelerator acc, size_t i) :
        accel(acc),
        startHeight(0),
        height(0)
    {
    }

    inline UINT EndHeight() const { return startHeight + height; };
};

class FrameProcessorAmpMultiBase : public IFrameProcessor
{
private:
    std::vector<TaskData> m_frameData;
    UINT m_neighborWindow;
    UINT m_height;
    UINT m_width;
    array<ArgbPackedPixel, 2> m_swapDataTop;
    array<ArgbPackedPixel, 2> m_swapDataBottom;
    array_view<ArgbPackedPixel, 2> m_swapViewTop;
    array_view<ArgbPackedPixel, 2> m_swapViewBottom;

public:
    FrameProcessorAmpMultiBase(const std::vector<accelerator>& accels) :
        m_neighborWindow(0),
        m_height(0),
        m_width(0),
        m_swapDataTop(extent<2>(1, 1)),
        m_swapViewTop(m_swapDataTop),
        m_swapDataBottom(extent<2>(1, 1)),
        m_swapViewBottom(m_swapDataBottom)
    {
        assert(accels.size() > 1);
        m_frameData.reserve(accels.size());
        size_t i = 0;
        std::for_each(accels.cbegin(), accels.cend(), [this, &i](const accelerator& a) { m_frameData.push_back(TaskData(a, i++)); });
    }

    void ProcessImage(const Gdiplus::BitmapData& srcFrame, Gdiplus::BitmapData& destFrame, UINT phases, UINT simplifierNeighborWindow)
    {
        assert(simplifierNeighborWindow % 2 == 0);
        assert(phases > 0);
        const UINT borderHeight = simplifierNeighborWindow / 2;

        ConfigureFrameBuffers(m_frameData, srcFrame, simplifierNeighborWindow + FrameProcessorAmp::EdgeBorderWidth);

        int current = kCurrent;
        int next = kNext;
        std::for_each(m_frameData.begin(), m_frameData.end(), [=] (TaskData& d) 
        {
            CopyIn(srcFrame, *d.frames[current].get(), d.startHeight, d.EndHeight());
            d.frames[current]->copy_to(*d.frames[kOriginal].get());
        });

        for (UINT i = 0; i < phases; ++i)
        {
            std::for_each(m_frameData.begin(), m_frameData.end(), [=](TaskData& d) 
            {
                ::ApplyColorSimplifierHelper(*d.frames[current].get(), *d.frames[next].get(), 
                    simplifierNeighborWindow);
            });

            for (UINT d = 0; d < m_frameData.size() - 1; ++d)
            {
                SwapEdges(m_frameData[d].frames[next].get(), m_frameData[d+1].frames[next].get(), 
                    borderHeight);
            }
            std::swap(current, next);
        }

        std::for_each(m_frameData.begin(), m_frameData.end(), [=](TaskData& d) 
        {
            ::ApplyEdgeDetectionHelper(*d.frames[current].get(), *d.frames[next].get(), *d.frames[kOriginal].get(), simplifierNeighborWindow);
        });
        std::swap(current, next);

        // Sync the resulting image data to CPU memory and merge data into final image. Do copies in reverse order 
        // so that bottom border of subframe n-1 covers top border area of frame n.
        // Use async copy to prevent blocking of other work due to process wide lock taken by DirectX during copy on Win7.

        UINT heightTrim = 0;
        std::vector<completion_future> copyResults(m_frameData.size());
        int i = 0;
        std::for_each(m_frameData.crbegin(), m_frameData.crend(), [=, &i, &copyResults, &destFrame, &heightTrim](const TaskData& d) 
        {
            copyResults[i++] = CopyOutAsync(*d.frames[current].get(), destFrame, d.startHeight, d.EndHeight() - heightTrim);
            heightTrim = (simplifierNeighborWindow + FrameProcessorAmp::EdgeBorderWidth) / 2;
        });
        parallel_for_each(copyResults.cbegin(), copyResults.cend(), [](const completion_future& f) { f.get(); });
    }

private:
    // Swap edges after each iteration of the color simplifier to avoid edge effects. This involves copying data back to
    // main memory, modifying it and then refreshing the accelerator memory with the new edge values. Use sections so
    // only the data that has changed needs to be synced back to the accelerator.

    void SwapEdges(array<ArgbPackedPixel, 2>* const top, 
        array<ArgbPackedPixel, 2>* const bottom, UINT borderHeight)
    {
        const UINT topHeight = top->extent[0];
        std::array<completion_future, 2> copyResults;
        copyResults[0] = copy_async(top->section(topHeight - borderHeight * 2, 0, borderHeight, m_width), 
            m_swapViewTop); 
        copyResults[1] = copy_async(
            bottom->section(borderHeight, 0, borderHeight, m_width), m_swapViewBottom);
        parallel_for_each(copyResults.begin(), copyResults.end(), [](completion_future& f)
            { f.get(); });

        copyResults[0] = copy_async(m_swapViewTop, 
            bottom->section(0, 0, borderHeight, m_width));
        copyResults[1] = copy_async(m_swapViewBottom, 
            top->section(topHeight - borderHeight, 0, borderHeight, m_width));
        parallel_for_each(copyResults.begin(), copyResults.end(), [](completion_future& f)
            { f.get(); });
    }

    void ConfigureFrameBuffers(std::vector<TaskData>& taskData, const Gdiplus::BitmapData& srcFrame, UINT neighborWindow)
    {
        bool neighborWindowChanged = m_neighborWindow != neighborWindow;
        bool widthChanged = m_width != srcFrame.Width;
        bool heightChanged = m_height != srcFrame.Height;
        m_height = srcFrame.Height;
        m_width = srcFrame.Width;
        m_neighborWindow = neighborWindow;

        // Only recalculate swap buffers if the neightborWindow or image width have changed as this may be expensive.

        if (neighborWindowChanged || widthChanged)
        {
            const UINT borderHeight = (neighborWindow - FrameProcessorAmp::EdgeBorderWidth) / 2;
            ATLTRACE("Reallocating swap buffers %u x %u\n", borderHeight, m_width);
            m_swapDataTop = array<ArgbPackedPixel, 2>(extent<2>(borderHeight, m_width), accelerator(accelerator::cpu_accelerator).default_view);
            m_swapViewTop = array_view<ArgbPackedPixel, 2>(m_swapDataTop);
            m_swapDataBottom = array<ArgbPackedPixel, 2>(extent<2>(borderHeight, m_width), accelerator(accelerator::cpu_accelerator).default_view);
            m_swapViewBottom = array_view<ArgbPackedPixel, 2>(m_swapDataBottom);
        }

        // Calculate heights for each subframe that makes up the whole frame. The upper subframe overlaps the one 
        // below it by simplifierNeighborWindow to account for halo pixels around each frame containing incorrect values because they
        // themselves to not have an edge.

        UINT heightOffset = 0;
        std::for_each(taskData.begin(), taskData.end(), [=, &heightOffset, &heightChanged] (TaskData& d) 
        { 
            d.startHeight = heightOffset;
            UINT newHeight = m_height / static_cast<UINT>(taskData.size());
            if (d.height == (newHeight + neighborWindow))
            {
                heightOffset += d.height - neighborWindow;
            }
            else 
            {
                heightChanged = true;
                d.height = newHeight + neighborWindow;
                heightOffset += newHeight;
            }
        });
        // Make sure that last accelerator has all the remaining frame rows.
        taskData.back().height = m_height - taskData.back().startHeight;

        // Only recalculate frames if the heights have changed as this may be expensive.

        if (!heightChanged && !widthChanged)
            return;

#ifdef _DEBUG
        ATLTRACE("Configure frame buffers: New image size %d x %d\n", m_height, m_width);
        UINT i = 0;
        std::for_each(taskData.begin(), taskData.end(), [=, &i] (TaskData& d) 
        {
            ATLTRACE("  %u: %u - %u (%u lines)\n", i++, d.startHeight, (d.startHeight + d.height), d.height);
        });
#endif
        std::for_each(m_frameData.begin(), m_frameData.end(), [=] (TaskData& d) 
        {
            std::generate(d.frames.begin(), d.frames.end(), [=]() 
            { 
                return std::make_shared<array<ArgbPackedPixel, 2>>(int(d.height), int(m_width), d.accel.default_view); 
            });
        });
    }
};

class FrameProcessorAmpMulti : public FrameProcessorAmpMultiBase
{
public:
    FrameProcessorAmpMulti(const std::vector<accelerator>& accels) : FrameProcessorAmpMultiBase(accels) { }

    virtual inline void ApplyColorSimplifier(accelerator& acc, const array<ArgbPackedPixel, 2>& srcFrame, array<ArgbPackedPixel, 2>& destFrame, UINT neighborWindow)
    {
        ::ApplyColorSimplifierHelper(srcFrame, destFrame, neighborWindow);
    }

    virtual inline void ApplyEdgeDetection(accelerator& acc, const array<ArgbPackedPixel, 2>& srcFrame, array<ArgbPackedPixel, 2>& destFrame, 
                        const array<ArgbPackedPixel, 2>& orgFrame, UINT simplifierNeighborWindow)
    {
        ::ApplyEdgeDetectionHelper(srcFrame, destFrame, orgFrame, simplifierNeighborWindow);
    }
};

class FrameProcessorAmpMultiTiled : public FrameProcessorAmpMultiBase
{
public:
    FrameProcessorAmpMultiTiled(const std::vector<accelerator>& accels) : FrameProcessorAmpMultiBase(accels) { }

    virtual inline void ApplyColorSimplifier(accelerator& acc, const array<ArgbPackedPixel, 2>& srcFrame, array<ArgbPackedPixel, 2>& destFrame, UINT neighborWindow)
    {
        ::ApplyColorSimplifierTiledHelper(srcFrame, destFrame, neighborWindow);
    }

    virtual inline void ApplyEdgeDetection(accelerator& acc, const array<ArgbPackedPixel, 2>& srcFrame, array<ArgbPackedPixel, 2>& destFrame, 
                        const array<ArgbPackedPixel, 2>& orgFrame, UINT simplifierNeighborWindow)
    {
        ::ApplyEdgeDetectionTiledHelper(srcFrame, destFrame, orgFrame, simplifierNeighborWindow);
    }
};