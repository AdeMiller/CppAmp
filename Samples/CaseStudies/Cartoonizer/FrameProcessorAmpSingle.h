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
#include <array>

#include "FrameProcessorAmp.h"
#include "IFrameProcessor.h"
#include "FrameProcessorAmpBase.h"

//--------------------------------------------------------------------------------------
//  Frame processor for a single C++ AMP accelerator.
//--------------------------------------------------------------------------------------
//
//  This runs on a single accelerator. If no hardware accelerator is available it will use the 
//  reference accelerator, this may be very slow.

class FrameProcessorAmpSingle : public FrameProcessorAmpBase
{
public:
    FrameProcessorAmpSingle(const accelerator& accel) : FrameProcessorAmpBase(accel) { }

    virtual inline void ApplyColorSimplifier(const array<ArgbPackedPixel, 2>& srcFrame, 
        array<ArgbPackedPixel, 2>& destFrame, UINT neighborWindow)
    {
        ::ApplyColorSimplifierHelper(srcFrame, destFrame, neighborWindow);
    }

    virtual inline void ApplyEdgeDetection(
        const array<ArgbPackedPixel, 2>& srcFrame, array<ArgbPackedPixel, 2>& destFrame, 
        const array<ArgbPackedPixel, 2>& orgFrame, UINT simplifierNeighborWindow)
    {
        ::ApplyEdgeDetectionHelper(srcFrame, destFrame, orgFrame, simplifierNeighborWindow);
    }
};

class FrameProcessorAmpSingleTiled : public FrameProcessorAmpBase
{
public:
    FrameProcessorAmpSingleTiled(const accelerator& accel) : FrameProcessorAmpBase(accel) { }

    virtual inline void ApplyColorSimplifier(
        const array<ArgbPackedPixel, 2>& srcFrame, array<ArgbPackedPixel, 2>& destFrame, 
        UINT neighborWindow)
    {
        ::ApplyColorSimplifierTiledHelper(srcFrame, destFrame, neighborWindow);
    }

    virtual inline void ApplyEdgeDetection(
        const array<ArgbPackedPixel, 2>& srcFrame, array<ArgbPackedPixel, 2>& destFrame, 
        const array<ArgbPackedPixel, 2>& orgFrame, UINT simplifierNeighborWindow)
    {
        ::ApplyEdgeDetectionTiledHelper(srcFrame, destFrame, orgFrame, simplifierNeighborWindow);
    }
};
