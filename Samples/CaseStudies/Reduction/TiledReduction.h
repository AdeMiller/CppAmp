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

//----------------------------------------------------------------------------
// This is a simple tiled implementation of reduction algorithm where only
// single thread in a tile is doing the work.
//----------------------------------------------------------------------------

#pragma once

#include "IReduce.h"
#include "Timer.h"
#include <vector>
#include <amp.h>

using namespace concurrency;

template <int TileSize>
class TiledReduction : public IReduce
{
public:
    int Reduce(accelerator_view& view, const std::vector<int>& source, double& computeTime) const
    {
        int elementCount = static_cast<int>(source.size());

        // Copy data
        array<int, 1> arr(elementCount, source.cbegin(), source.cend(), view);

        int result;
        computeTime = TimeFunc(view, [&]() 
        {
            while (elementCount >= TileSize)
            {
                extent<1> e(elementCount);
                array<int, 1> tmpArr(elementCount / TileSize);

                parallel_for_each(view, e.tile<TileSize>(), 
                    [=, &arr, &tmpArr] (tiled_index<TileSize> tidx) restrict(amp)
                {
                    //  For each tile do the reduction on the first thread of the tile. This isn't 
                    //  expected to be very efficient as all the other threads in the tile are idle.
                    if (tidx.local[0] == 0)
                    {
                        int tid = tidx.global[0];
                        int tempResult = arr[tid];
                        for (int i = 1; i < TileSize; ++i)
                            tempResult += arr[tid + i];
                        //  Take the result from each tile and create a new array. This will be used in 
                        //  the next iteration. Use temporary array to avoid race condition.
                        tmpArr[tidx.tile[0]] = tempResult;
                    }
                });

                elementCount /= TileSize;
                std::swap(tmpArr, arr);
            }

            //  Copy the final results from each tile to the CPU and accumulate them there
            std::vector<int> partialResult(elementCount);
            copy(arr.section(0, elementCount), partialResult.begin());
            result = std::accumulate(partialResult.cbegin(), partialResult.cend(), 0);
        });
        return result;
    }
};
