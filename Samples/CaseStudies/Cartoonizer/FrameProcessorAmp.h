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
#include "GdiWrap.h"
#include <memory>
#include <assert.h>

#include "AmpUtilities.h"
#include "RgbPixel.h"

using namespace concurrency;
using namespace concurrency::graphics;

//--------------------------------------------------------------------------------------
//  Constants for use by all C++ AMP frame processors.
//--------------------------------------------------------------------------------------

class FrameProcessorAmp
{
public:
    static const UINT MaxNeighborWindow = 16;
    static const UINT MaxSimplifierPhases = 32;
    static const UINT TileSize = 16;
    static const UINT EdgeBorderWidth = 2;
};

//--------------------------------------------------------------------------------------
//  Image processing functions used by all C++ AMP image processors.
//--------------------------------------------------------------------------------------

tiled_extent<FrameProcessorAmp::TileSize, FrameProcessorAmp::TileSize> GetTiledExtent(const extent<2>& ext);

//  Copy images on and off the accelerator.

void CopyIn(const Gdiplus::BitmapData& srcFrame, array<ArgbPackedPixel, 2>& currentImg, UINT startHeight, UINT endHeight);

inline void CopyIn(const Gdiplus::BitmapData& srcFrame, array<ArgbPackedPixel, 2>& currentImg)
{
    CopyIn(srcFrame, currentImg, 0, srcFrame.Height);
}

void CopyOut(array<ArgbPackedPixel, 2>& currentImg, Gdiplus::BitmapData& destFrame);

completion_future CopyOutAsync(array<ArgbPackedPixel, 2>& currentImg, Gdiplus::BitmapData& destFrame, UINT startHeight, UINT endHeight);

//  Color simplifier.

void ApplyColorSimplifierHelper(const array<ArgbPackedPixel, 2>& srcFrame, array<ArgbPackedPixel, 2>& destFrame, UINT neighborWindow);

void ApplyColorSimplifierTiledHelper(const array<ArgbPackedPixel, 2>& srcFrame, array<ArgbPackedPixel, 2>& destFrame, UINT neighborWindow);

void SimplifyIndex(const array<ArgbPackedPixel, 2>& srcFrame, array<ArgbPackedPixel, 2>& destFrame, index<2> idx, UINT neighborWindow, const float_3& W) restrict(amp);

void SimplifyIndexTiled(const array<ArgbPackedPixel, 2>& srcFrame, array<ArgbPackedPixel, 2>& destFrame, tiled_index<FrameProcessorAmp::TileSize, FrameProcessorAmp::TileSize> idx, 
    UINT neighborWindow, const float_3& W) restrict(amp);

//  Edge detection.

void ApplyEdgeDetectionHelper(const array<ArgbPackedPixel, 2>& srcFrame, array<ArgbPackedPixel, 2>& destFrame, 
    const array<ArgbPackedPixel, 2>& orgFrame, UINT simplifierNeighborWindow);
void ApplyEdgeDetectionTiledHelper(const array<ArgbPackedPixel, 2>& srcFrame, array<ArgbPackedPixel, 2>& destFrame, 
    const array<ArgbPackedPixel, 2>& orgFrame, UINT simplifierNeighborWindow);

void DetectEdge(index<2> idx, const array<ArgbPackedPixel, 2>& srcFrame, array<ArgbPackedPixel, 2>& destFrame,  
    const array<ArgbPackedPixel, 2>& orgFrame, UINT simplifierNeighborWindow, const float_3& W) restrict(amp);

void DetectEdgeTiled(tiled_index<FrameProcessorAmp::TileSize, FrameProcessorAmp::TileSize> idx, const array<ArgbPackedPixel, 2>& srcFrame, array<ArgbPackedPixel, 2>& destFrame, 
    const array<ArgbPackedPixel, 2>& orgFrame, UINT simplifierNeighborWindow, const float_3& W) restrict(amp);

void CalculateSobel(const array<ArgbPackedPixel, 2>& source, index<2> idx, float& dy, float& du, float& dv, const float_3& W) restrict(amp);

void CalculateSobelTiled(const RgbPixel source[FrameProcessorAmp::TileSize + 2][FrameProcessorAmp::TileSize + 2], index<2> idx, float& dy, float& du, float& dv, const float_3& W) restrict(amp);
