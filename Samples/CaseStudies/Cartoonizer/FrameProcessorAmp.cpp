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
#include <algorithm>
#include <numeric>
#include <amp.h>
#include <memory>
#include <assert.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <amp_math.h>

#include "FrameProcessorAmp.h"
#include "FrameProcessorAmpSingle.h"
#include "FrameProcessorAmpMulti.h"
#include "utilities.h"
#include "AmpUtilities.h"

using namespace concurrency;
using namespace concurrency::graphics;
using namespace concurrency::direct3d;

const float_3 ImageUtils::W(0.299f, 0.114f, (1.000f - 0.299f - 0.114f));

//  Memory is always accessed most efficiently by contiguous accesses both in CPU and GPU.
//  Images are stored in row-major order so pixels are addressed as [y][x] or [row][col]. Accessing over the
//  least significant index first (x) is more efficient. In the following methods height always comes first 
//  as it corresponds to rows.

//--------------------------------------------------------------------------------------
//  Pad the extent of the raw image to a tiled_extent that fits the tile size.
//--------------------------------------------------------------------------------------

tiled_extent<FrameProcessorAmp::TileSize, FrameProcessorAmp::TileSize> GetTiledExtent(const extent<2>& ext)
{
    tiled_extent<FrameProcessorAmp::TileSize, FrameProcessorAmp::TileSize> text(ext + extent<2>(1, 1));
    return text.pad();
}

//--------------------------------------------------------------------------------------
//  Copy images or ranges of image rows to and from the accelerator.
//--------------------------------------------------------------------------------------

void CopyIn(const Gdiplus::BitmapData& srcFrame, array<ArgbPackedPixel, 2>& currentImg, UINT startHeight, UINT endHeight)
{
    // Because ARGB is stored in four bytes the Bitmap::Width will always be equal to the Bitmap::Stride / sizeof(ArgbPackedPixel), no padding.
    const int height = endHeight - startHeight;
    ArgbPackedPixel* const startAddr = static_cast<ArgbPackedPixel*>(srcFrame.Scan0) + startHeight * srcFrame.Width;
    copy(startAddr, startAddr + (height * srcFrame.Width), currentImg);
}

void CopyOut(array<ArgbPackedPixel, 2>& currentImg, Gdiplus::BitmapData& destFrame)
{
    // Because ARGB is stored in four bytes the Bitmap::Width will always be equal to the Bitmap::Stride / sizeof(ArgbPackedPixel), no padding.
    auto iter = stdext::make_checked_array_iterator<ArgbPackedPixel*>(static_cast<ArgbPackedPixel*>(destFrame.Scan0), destFrame.Height * destFrame.Width);

    // Make sure that and preceeding kernel is finished before starting to copy data from the GPU and reduce the time copy may take a lock for.
    completion_future f = copy_async(currentImg.section(0, 0, destFrame.Height, destFrame.Width), iter);
    currentImg.accelerator_view.wait();
    f.get();
}

completion_future CopyOutAsync(array<ArgbPackedPixel, 2>& currentImg, Gdiplus::BitmapData& destFrame, UINT startHeight, UINT endHeight)
{
    // Because ARGB is stored in four bytes the Bitmap::Width will always be equal to the Bitmap::Stride / sizeof(ArgbPackedPixel), no padding.
    const int height = endHeight - startHeight;
    stdext::checked_array_iterator<ArgbPackedPixel*> iter = stdext::make_checked_array_iterator<ArgbPackedPixel*>(static_cast<ArgbPackedPixel*>(destFrame.Scan0) + startHeight * destFrame.Width, height * destFrame.Width);
    return copy_async(currentImg.section(0, 0, height, destFrame.Width), iter);
}

//--------------------------------------------------------------------------------------
//  Color simplifier.
//--------------------------------------------------------------------------------------
//
// Simplify a portion of the image by removing features that fall below a given color contrast.
// We do this by calculating the difference in color between neighboring pixels and applying an
// exponential decay function then summing the results.
// We then set the R, G, B values to the normalized sums

void ApplyColorSimplifierHelper(const array<ArgbPackedPixel, 2>& srcFrame, array<ArgbPackedPixel, 2>& destFrame, UINT neighborWindow)
{
    const float_3 W(ImageUtils::W);

    extent<2> computeDomain(srcFrame.extent - extent<2>(neighborWindow, neighborWindow));
    parallel_for_each(computeDomain, [=, &srcFrame, &destFrame](index<2> idx) restrict(amp)
    {
        SimplifyIndex(srcFrame, destFrame, idx, neighborWindow, W);
    });
}

void ApplyColorSimplifierTiledHelper(const array<ArgbPackedPixel, 2>& srcFrame, array<ArgbPackedPixel, 2>& destFrame, UINT neighborWindow)
{
    const float_3 W(ImageUtils::W);

    assert(neighborWindow <= FrameProcessorAmp::MaxNeighborWindow);

    tiled_extent<FrameProcessorAmp::TileSize, FrameProcessorAmp::TileSize> computeDomain = GetTiledExtent(srcFrame.extent);
    parallel_for_each(computeDomain, [=, &srcFrame, &destFrame](tiled_index<FrameProcessorAmp::TileSize, FrameProcessorAmp::TileSize> idx) restrict(amp)
    {
        SimplifyIndexTiled(srcFrame, destFrame, idx, neighborWindow, W);
    });
}

void SimplifyIndex(const array<ArgbPackedPixel, 2>& srcFrame, array<ArgbPackedPixel, 2>& destFrame, index<2> idx, 
                   UINT neighborWindow, const float_3& W) restrict(amp)
{
    const int shift = neighborWindow / 2;
    float sum = 0;
    float_3 partialSum(0.0f, 0.0f,0.0f);
    const float standardDeviation = 0.025f;
    // k is the exponential decay constant and is calculated from a standard deviation of 0.025
    const float k = -0.5f / (standardDeviation * standardDeviation);

    const int idxY = idx[0] + shift;         // Corrected index for border offset.
    const int idxX = idx[1] + shift;
    const int y_start = idxY - shift;
    const int y_end = idxY + shift;
    const int x_start = idxX - shift;
    const int x_end = idxX + shift;

    RgbPixel orgClr = UnpackPixel(srcFrame(idxY, idxX));

    for (int y = y_start; y <= y_end; ++y)
        for (int x = x_start; x <= x_end; ++x)
        {
            if (x != idxX || y != idxY) // don't apply filter to the requested index, only to the neighbors
            {
                RgbPixel clr = UnpackPixel(srcFrame(y, x));
                float distance = ImageUtils::GetDistance(orgClr, clr, W);
                float value = concurrency::fast_math::pow(float(M_E), k * distance * distance);
                sum += value;
                partialSum.r += clr.r * value;
                partialSum.g += clr.g * value;
                partialSum.b += clr.b * value;
            }
        }

    RgbPixel newClr;
    newClr.r = static_cast<UINT>(clamp(partialSum.r / sum, 0.0f, 255.0f));
    newClr.g = static_cast<UINT>(clamp(partialSum.g / sum, 0.0f, 255.0f));
    newClr.b = static_cast<UINT>(clamp(partialSum.b / sum, 0.0f, 255.0f));
    destFrame(idxY, idxX) = PackPixel(newClr);
}

void SimplifyIndexTiled(const array<ArgbPackedPixel, 2>& srcFrame, array<ArgbPackedPixel, 2>& destFrame, tiled_index<FrameProcessorAmp::TileSize, FrameProcessorAmp::TileSize> idx, 
                        UINT neighborWindow, const float_3& W) restrict(amp)
{
    const UINT shift = neighborWindow / 2;
    UINT startHeight = shift;
    UINT startWidth = shift;
    UINT endHeight = srcFrame.extent[0] - shift;    
    UINT endWidth = srcFrame.extent[1] - shift;
    tile_static RgbPixel src_local[FrameProcessorAmp::TileSize + FrameProcessorAmp::MaxNeighborWindow][FrameProcessorAmp::TileSize + FrameProcessorAmp::MaxNeighborWindow];

    const UINT global_idxY = idx.global[0];
    const UINT global_idxX = idx.global[1];
    const UINT local_idxY = idx.local[0];
    const UINT local_idxX = idx.local[1];

    const UINT local_idx_tsY = local_idxY + shift;
    const UINT local_idx_tsX = local_idxX + shift;

    // Copy image data to tile_static memory. The if clauses are required to deal with threads that own a
    // pixel close to the edge of the tile and need to copy additional halo data.

    // This pixel
    src_local[local_idx_tsY][local_idx_tsX] = UnpackPixel(srcFrame[global_idxY][global_idxX]);

    // Left edge
    if (local_idxX < shift)
        src_local[local_idx_tsY][local_idx_tsX - shift] = UnpackPixel(srcFrame[index<2>(global_idxY, global_idxX - shift)]);

    // Right edge
    if (local_idxX >= FrameProcessorAmp::TileSize - shift)
        src_local[local_idx_tsY][local_idx_tsX + shift] = UnpackPixel(srcFrame[index<2>(global_idxY, global_idxX + shift)]);

    // Top edge
    if (local_idxY < shift)
        src_local[local_idx_tsY - shift][local_idx_tsX] = UnpackPixel(srcFrame[index<2>(global_idxY - shift, global_idxX)]);

    // Bottom edge
    if (local_idxY >= FrameProcessorAmp::TileSize - shift)
        src_local[local_idx_tsY + shift][local_idx_tsX] = UnpackPixel(srcFrame[index<2>(global_idxY + shift, global_idxX)]);

    // Top Left corner
    if ((local_idxY < shift) && (local_idxX < shift))
        src_local[local_idx_tsY-shift][local_idx_tsX - shift] = UnpackPixel(srcFrame[index<2>(global_idxY - shift, global_idxX - shift)]);

    // Bottom Left corner
    if ((local_idxY >= FrameProcessorAmp::TileSize - shift) && (local_idxX < shift))
        src_local[local_idx_tsY+shift][local_idx_tsX - shift] = UnpackPixel(srcFrame[index<2>(global_idxY + shift, global_idxX - shift)]);

    // Bottom Right corner
    if ((local_idxY >= FrameProcessorAmp::TileSize - shift) && (local_idxX >= FrameProcessorAmp::TileSize - shift))
        src_local[local_idx_tsY+shift][local_idx_tsX + shift] = UnpackPixel(srcFrame[index<2>(global_idxY + shift, global_idxX + shift)]);

    // Top Right corner
    if ((local_idxY < shift) && (local_idxX >= FrameProcessorAmp::TileSize - shift))
        src_local[local_idx_tsY-shift][local_idx_tsX + shift] = UnpackPixel(srcFrame[index<2>(global_idxY - shift, global_idxX + shift)]);

    //  Synchronize all threads so that none of them start calculation before all data is copied onto the current tile.
    idx.barrier.wait();

    if ((global_idxY >= startHeight && global_idxY <= endHeight) && (global_idxX >= startWidth && global_idxX <= endWidth))
    {
        RgbPixel orgClr = src_local[local_idx_tsY][local_idx_tsX];

        float sum = 0;
        float_3 partialSum;
        const float standardDeviation = 0.025f;
        // k is the exponential decay constant and is calculated from a standard deviation of 0.025
        const float k = -0.5f / (standardDeviation * standardDeviation);

        const int yStart = local_idx_tsY - shift;
        const int yEnd = local_idx_tsY + shift;
        const int xStart = local_idx_tsX - shift;
        const int xEnd = local_idx_tsX + shift;

        for(int y = yStart; y <= yEnd; ++y)
            for(int x = xStart; x <= xEnd; ++x)
            {
                if(x != local_idx_tsX || y != local_idx_tsY) // don't apply filter to the requested index, only to the neighbors
                {
                    RgbPixel clr = src_local[y][x];
                    float distance = ImageUtils::GetDistance(orgClr, clr, W);
                    float value = concurrency::fast_math::pow(float(M_E), k * distance * distance);
                    sum += value;
                    partialSum.r += clr.r * value;
                    partialSum.g += clr.g * value;
                    partialSum.b += clr.b * value;
                }
            }
        RgbPixel newClr;
        newClr.r = static_cast<UINT>(clamp(partialSum.r / sum, 0.0f, 255.0f));
        newClr.g = static_cast<UINT>(clamp(partialSum.g / sum, 0.0f, 255.0f));
        newClr.b = static_cast<UINT>(clamp(partialSum.b / sum, 0.0f, 255.0f));
        destFrame[index<2>(global_idxY, global_idxX)] = PackPixel(newClr);
    }
 }

//--------------------------------------------------------------------------------------
//  Edge detection.
//--------------------------------------------------------------------------------------
//
//  See the following Wikipedia page on the Canny edge detector for a further description 
//  of the algorithm.

void ApplyEdgeDetectionHelper(const array<ArgbPackedPixel, 2>& srcFrame, 
                              array<ArgbPackedPixel, 2>& destFrame, const array<ArgbPackedPixel, 2>& orgFrame, 
                              UINT simplifierNeighborWindow)
{
    const float_3 W(ImageUtils::W);
    extent<2> ext(srcFrame.extent - extent<2>(simplifierNeighborWindow, simplifierNeighborWindow));

    extent<2> computeDomain(ext - 
        extent<2>(FrameProcessorAmp::EdgeBorderWidth, FrameProcessorAmp::EdgeBorderWidth));
    parallel_for_each(computeDomain, 
        [=, &srcFrame, &destFrame, &orgFrame](index<2> idx) restrict(amp) 
    {
        DetectEdge(idx, srcFrame, destFrame, orgFrame, simplifierNeighborWindow, W);
    });
}

void ApplyEdgeDetectionTiledHelper(const array<ArgbPackedPixel, 2>& srcFrame, 
                                   array<ArgbPackedPixel, 2>& destFrame, const array<ArgbPackedPixel, 2>& orgFrame, 
                                   UINT simplifierNeighborWindow)
{
    const float_3 W(ImageUtils::W);
    extent<2> ext(srcFrame.extent - extent<2>(simplifierNeighborWindow, simplifierNeighborWindow));

    tiled_extent<FrameProcessorAmp::TileSize, FrameProcessorAmp::TileSize> computeDomain = GetTiledExtent(srcFrame.extent);
    parallel_for_each(computeDomain.tile<FrameProcessorAmp::TileSize, FrameProcessorAmp::TileSize>(), [=, &srcFrame, &destFrame, &orgFrame](tiled_index<FrameProcessorAmp::TileSize, FrameProcessorAmp::TileSize> idx) restrict(amp) 
    {
        DetectEdgeTiled(idx, srcFrame, destFrame, orgFrame, simplifierNeighborWindow, W);
    });
}

void DetectEdge(index<2> idx, const array<ArgbPackedPixel, 2>& srcFrame, 
    array<ArgbPackedPixel, 2>& destFrame, const array<ArgbPackedPixel, 2>& orgFrame, 
    UINT simplifierNeighborWindow, const float_3& W) restrict(amp)
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
    const float i = (1 - beta) * smoothstep(s0, s1, edgeS) + beta * smoothstep(a0, a1, edgeA);

    const RgbPixel srcClr = UnpackPixel(srcFrame[idc]);
    RgbPixel destClr;
    const float oneMinusi = 1 - i;
    destClr.r = static_cast<UINT>(srcClr.r * oneMinusi);
    destClr.g = static_cast<UINT>(srcClr.g * oneMinusi);
    destClr.b = static_cast<UINT>(srcClr.b * oneMinusi);
    destFrame[idc] = PackPixel(destClr);
}

void DetectEdgeTiled(tiled_index<FrameProcessorAmp::TileSize, FrameProcessorAmp::TileSize> idx, const array<ArgbPackedPixel, 2>& srcFrame, array<ArgbPackedPixel, 2>& destFrame,
                     const array<ArgbPackedPixel, 2>& orgFrame, UINT simplifierNeighborWindow, const float_3& W) restrict(amp)
{
    const UINT shift = FrameProcessorAmp::EdgeBorderWidth / 2;
    const UINT offset = simplifierNeighborWindow / 2;   // Don't apply edge detection to pixels at image edges that were not color simplified.
    const UINT startHeight = offset;
    const UINT startWidth = offset;
    const UINT endHeight = srcFrame.extent[0] - offset;    
    const UINT endWidth = srcFrame.extent[1] - offset;

    tile_static RgbPixel localSrc[FrameProcessorAmp::TileSize + FrameProcessorAmp::EdgeBorderWidth][FrameProcessorAmp::TileSize + FrameProcessorAmp::EdgeBorderWidth];
    tile_static RgbPixel localOrg[FrameProcessorAmp::TileSize + FrameProcessorAmp::EdgeBorderWidth][FrameProcessorAmp::TileSize + FrameProcessorAmp::EdgeBorderWidth];

    const UINT global_idxY = idx.global[0] + offset;
    const UINT global_idxX = idx.global[1] + offset;
    const UINT local_idxY = idx.local[0];
    const UINT local_idxX = idx.local[1];

    const UINT local_idx_tsY = local_idxY + shift;      // Corrected index for edge detection border offset.
    const UINT local_idx_tsX = local_idxX + shift;

    // Copy image data to tile_static memory. The if clauses are required to deal with threads that own a
    // pixel close to the edge of the tile and need to copy additional halo data.

    // This pixel
    index<2> gNew = index<2>(global_idxY, global_idxX);
    localSrc[local_idx_tsY][local_idx_tsX] = UnpackPixel(srcFrame[gNew]);
    localOrg[local_idx_tsY][local_idx_tsX] = UnpackPixel(orgFrame[gNew]);

    // Left edge
    if (local_idxX < shift)
    {
        index<2> gNew = index<2>(global_idxY, global_idxX - shift);
        localSrc[local_idx_tsY][local_idx_tsX-shift] = UnpackPixel(srcFrame[gNew]);
        localOrg[local_idx_tsY][local_idx_tsX-shift] = UnpackPixel(orgFrame[gNew]);
    }
    // Right edge
    if (local_idxX >= FrameProcessorAmp::TileSize - shift)
    {
        index<2> gNew = index<2>(global_idxY, global_idxX + shift);
        localSrc[local_idx_tsY][local_idx_tsX+shift] = UnpackPixel(srcFrame[gNew]);
        localOrg[local_idx_tsY][local_idx_tsX+shift] = UnpackPixel(orgFrame[gNew]);
    }
    // Top edge
    if (local_idxY < shift)
    {
        index<2> gNew = index<2>(global_idxY - shift, global_idxX);
        localSrc[local_idx_tsY-shift][local_idx_tsX] = UnpackPixel(srcFrame[gNew]);
        localOrg[local_idx_tsY-shift][local_idx_tsX] = UnpackPixel(orgFrame[gNew]);
    }
    // Bottom edge
    if (local_idxY >= FrameProcessorAmp::TileSize - shift)
    {
        index<2> gNew = index<2>(global_idxY + shift, global_idxX);
        localSrc[local_idx_tsY+shift][local_idx_tsX] = UnpackPixel(srcFrame[gNew]);
        localOrg[local_idx_tsY+shift][local_idx_tsX] = UnpackPixel(orgFrame[gNew]);
    }
    // Top Left corner
    if (local_idxX < shift && local_idxY < shift)
    {
        index<2> gNew = index<2>(global_idxY - shift, global_idxX - shift);
        localSrc[local_idx_tsY-shift][local_idx_tsX-shift] = UnpackPixel(srcFrame[gNew]);
        localOrg[local_idx_tsY-shift][local_idx_tsX-shift] = UnpackPixel(orgFrame[gNew]);
    }
    // Bottom Left corner
    if (local_idxX < shift && local_idxY >= FrameProcessorAmp::TileSize - shift)
    {
        index<2> gNew = index<2>(global_idxY + shift, global_idxX - shift);
        localSrc[local_idx_tsY+shift][local_idx_tsX-shift] = UnpackPixel(srcFrame[gNew]);
        localOrg[local_idx_tsY+shift][local_idx_tsX-shift] = UnpackPixel(orgFrame[gNew]);
    }
    // Bottom Right corner
    if ((local_idxY >= FrameProcessorAmp::TileSize - shift) && (local_idxX >= FrameProcessorAmp::TileSize - shift))
    {
        index<2> gNew = index<2>(global_idxY + shift, global_idxX + shift);
        localSrc[local_idx_tsY+shift][local_idx_tsX+shift] = UnpackPixel(srcFrame[gNew]);
        localOrg[local_idx_tsY+shift][local_idx_tsX+shift] = UnpackPixel(orgFrame[gNew]);
    }
    // Top Right corner
    if (local_idxX >= FrameProcessorAmp::TileSize - shift && local_idxY < shift)
    {
        index<2> gNew = index<2>(global_idxY - shift, global_idxX + shift);
        localSrc[local_idx_tsY-shift][local_idx_tsX+shift] = UnpackPixel(srcFrame[gNew]);
        localOrg[local_idx_tsY-shift][local_idx_tsX+shift] = UnpackPixel(orgFrame[gNew]);
    }

    //  Synchronize all threads so that none of them start calculation before all data is copied onto the current tile.
    idx.barrier.wait();

    if ((global_idxY >= startHeight + 1 && global_idxY <= endHeight  - 1) && (global_idxX >= startWidth + 1 && global_idxX <= endWidth - 1))
    {
        const float alpha = 0.3f;       // Weighting of original frame for edge detection
        const float beta = 0.8f;        // Weighting of source (color simplied) frame for edge detection

        const float s0 = 0.054f;        // Minimum Threshold of source frame Sobel value to detect an edge
        const float s1 = 0.064f;        // Maximum Threshold of source frame Sobel value to effect the darkness of the edge
        const float a0 = 0.3f;          // Minimum Threshold of original frame Sobel value to detect an edge
        const float a1 = 0.7f;          // Maximum Threshold of original frame Sobel value to effect the darkness of the edge

        float Sy, Su, Sv;
        float Ay, Au, Av;
        Sy = Su = Sv = 0.0f;
        Ay = Au = Av = 0.0f;

        CalculateSobelTiled(localSrc, index<2>(local_idx_tsY, local_idx_tsX), Sy, Su, Sv, W);
        CalculateSobelTiled(localOrg, index<2>(local_idx_tsY, local_idx_tsX), Ay, Au, Av, W);

        const float edgeS = (1 - alpha) * Sy + alpha * (Su + Sv) / 2;
        const float edgeA = (1 - alpha) * Ay + alpha * (Au + Av) / 2;
        const float i = (1 - beta) * smoothstep(s0, s1, edgeS) + beta * smoothstep(a0, a1, edgeA);

        const RgbPixel srcClr =  localSrc[local_idx_tsY][local_idx_tsX];
        RgbPixel destClr;

        const float oneMinusi = 1 - i;
        destClr.r = static_cast<UINT>(srcClr.r * oneMinusi);
        destClr.g = static_cast<UINT>(srcClr.g * oneMinusi);
        destClr.b = static_cast<UINT>(srcClr.b * oneMinusi);
        destFrame[index<2>(global_idxY, global_idxX)] = PackPixel(destClr);
    }
}

void CalculateSobel(const array<ArgbPackedPixel, 2>& srcFrame, index<2> idx, 
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
            ImageUtils::RGBToYUV(UnpackPixel(srcFrame[idxNew]), clrY, clrU, clrV, W);

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

void CalculateSobelTiled(const RgbPixel source[FrameProcessorAmp::TileSize+2][FrameProcessorAmp::TileSize+2], index<2> idx, float& dy, float& du, float& dv, const float_3& W) restrict(amp)
{
    // Apply the Sobel operator to the image.  The Sobel operation is analogous
    // to a first derivative of the grayscale part of the image.  Portions of
    // the image that change rapidly (edges) have higher sobel values.

    // Gx is the matrix used to calculate the horizontal gradient of image
    // Gy is the matrix used to calculate the vertical gradient of image
    const int gx[3][3] = { { -1, 0, 1 }, { -2, 0, 2 }, { -1,  0,  1 } };
    const int gy[3][3] = { {  1, 2, 1 }, {  0, 0, 0 }, { -1, -2, -1 } };

    const int idxY = idx[0];
    const int idxX = idx[1];
    float new_yX = 0, new_yY = 0;
    float new_uX = 0, new_uY = 0;
    float new_vX = 0, new_vY = 0;

    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++)
        {
            const int gX = gx[x + 1][y + 1];
            const int gY = gy[x + 1][y + 1];
            float clrY, clrU, clrV;
            int newY = idxY + y;
            int newX = idxX + x;

            ImageUtils::RGBToYUV(source[newY][newX], clrY, clrU, clrV, W);

            new_yX += gX * clrY;
            new_yY += gY * clrY;
            new_uX += gX * clrU;
            new_uY += gY * clrU;
            new_vX += gX * clrV;
            new_vY += gY * clrV;
        }

    // Calculate the magnitude of the gradient from the horizontal and vertical gradients
    dy = fast_math::sqrt((new_yX * new_yX) + (new_yY * new_yY));
    du = fast_math::sqrt((new_uX * new_uX) + (new_uY * new_uY));
    dv = fast_math::sqrt((new_vX * new_vX) + (new_vY * new_vY));
}
