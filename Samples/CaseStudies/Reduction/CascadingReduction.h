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
// Here we take completely different approach by using algorithm cascading
// by combining sequential and parallel reduction.
//----------------------------------------------------------------------------

#pragma once

#include "IReduce.h"
#include "Timer.h"
#include <vector>
#include <amp.h>

using namespace concurrency;

template <int TileSize, int TileCount>
class CascadingReduction : public IReduce
{
public:
    int Reduce(accelerator_view& view, const std::vector<int>& source, double& computeTime) const
    {
        static_assert(((TileSize % 64) == 0), "");

        int elementCount = static_cast<int>(source.size());
        assert((elementCount % (TileCount * TileSize) == 0));
 
        // Copy data
        array<int, 1> a(elementCount, source.cbegin(), source.cend(), view);
        extent<1> e(TileCount * TileSize);

        int result;
        computeTime = TimeFunc(view, [&]() 
        {
            array<int, 1> partial(TileCount);
            parallel_for_each(view, e.tile<TileSize>(), [=, &a, &partial] (tiled_index<TileSize> tidx) restrict(amp)
            {
                int tid = tidx.local[0];
                tile_static int tileData[TileSize];
                int i = (tidx.tile[0] * 2 * TileSize) + tid;
                int stride = TileSize * 2 * TileCount;

                //  Load and add many elements, rather than just two
                int sum = 0;
                do
                {
                    sum += a[i] + a[i + TileSize];
                    i += stride;
                } 
                while (i < elementCount);
                tileData[tid] = sum;

                //  Wait for all threads to finish copying and adding
                tidx.barrier.wait();

                //  Reduce values for data on this tile
                for (stride = (TileSize / 2); stride > 0; stride >>= 1)
                {
                    //  Remember that this is a branch within a loop and all threads will have to execute 
                    //  this but only threads with a tid < stride will do useful work.
                    if (tid < stride)
                        tileData[tid] += tileData[tid + stride];

                    tidx.barrier.wait_with_tile_static_memory_fence();
                }

                //  Write the result for this tile back to global memory
                if (tid == 0)
                    partial[tidx.tile[0]] = tileData[tid];
            });

            //  Copy the final results from each tile to the CPU and accumulate them there.   
            std::vector<int> partialResult(TileCount);
            copy(partial, partialResult.begin());
            result = std::accumulate(partialResult.cbegin(), partialResult.cend(), 0);
        });
        return result;
    }
};
