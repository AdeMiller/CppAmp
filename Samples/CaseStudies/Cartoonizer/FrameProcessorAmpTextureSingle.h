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
#include <amp_graphics.h>
#include <array>

#include "FrameProcessorAmp.h"
#include "IFrameProcessor.h"

using namespace concurrency::graphics;

//--------------------------------------------------------------------------------------
//  Frame processor for a single C++ AMP accelerator.
//--------------------------------------------------------------------------------------
//
//  This runs on a single accelerator. If no hardware accelerator is available it will use the 
//  reference accelerator, this may be very slow.

class FrameProcessorAmpTextureSingle : public IFrameProcessor
{
private:
    accelerator m_accelerator;
    std::array<std::shared_ptr<texture<uint_4, 2>>, 3> m_frames;
#if (_MSC_VER >= 1800)
    std::unique_ptr<texture_view<uint_4, 2>> m_originalFrameView;
#else
    std::unique_ptr<writeonly_texture_view<uint_4, 2>> m_originalFrameView;
#endif
    UINT m_height;
    UINT m_width;

public:
    FrameProcessorAmpTextureSingle(accelerator accel) :
        m_accelerator(accel),
        m_height(0),
        m_width(0)
    {
    }

    void ProcessImage(const Gdiplus::BitmapData& srcFrame, Gdiplus::BitmapData& destFrame, UINT phases, UINT neighborWindow);

private:
    void ConfigureFrameBuffers(const Gdiplus::BitmapData& srcFrame);
};
