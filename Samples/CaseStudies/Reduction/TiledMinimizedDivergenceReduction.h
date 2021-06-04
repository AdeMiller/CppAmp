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
// This is a version with better resource utilization - minimize divergence
// A group of consecutive threads become are active - 
// minimize divergence within a scheduling units
//----------------------------------------------------------------------------

#pragma once

#include "IReduce.h"
#include "Timer.h"
#include <vector>
#include <amp.h>

using namespace concurrency;

template <int TileSize>
class TiledMinimizedDivergenceReduction : public IReduce
{
public:
    int Reduce(accelerator_view& view, const std::vector<int>& source, double& computeTime) const
    {
        int elementCount = static_cast<int>(source.size());

        array<int, 1> a(elementCount, source.cbegin(), source.cend(), view);
        array<int, 1> temp(elementCount / TileSize, view);        
        array_view<int, 1> av(a);
        array_view<int, 1> tmpAv(temp);
        tmpAv.discard_data();

        int result;
        computeTime = TimeFunc(view, [&]() 
        {
            while (elementCount >= TileSize)
            {
                extent<1> e(elementCount);

                parallel_for_each(view, e.tile<TileSize>(), [=] (tiled_index<TileSize> tidx) restrict(amp)
                {
                    //  Copy data onto tile static memory
                    int tid = tidx.local[0];
                    tile_static int tileData[TileSize];
                    tileData[tid] = av[tidx.global[0]];

                    //  Wait for all threads to finish copying
                    tidx.barrier.wait(); 

                    //  Reduce values for data on this tile
                    for (int stride = 1; stride < TileSize; stride *= 2)
                    {
                        // More efficient code without complex branching but still has bank conflicts due to interleaved addressing
                        int index = 2 * stride * tid;
                        if (index < TileSize)
                            tileData[index] += tileData[index + stride];

                        tidx.barrier.wait_with_tile_static_memory_fence();
                    }

                    //  Write the result for this tile back to global memory
                    if (tid == 0)
                        tmpAv[tidx.tile[0]] = tileData[0];
                });

                elementCount /= TileSize;
                std::swap(tmpAv, av);
            }
            tmpAv.discard_data();

            //  Copy the final results from each tile to the CPU and accumulate them there.   
            std::vector<int> partialResult(elementCount);
            copy(av.section(0, elementCount), partialResult.begin());
            av.discard_data();
            result = std::accumulate(partialResult.cbegin(), partialResult.cend(), 0);
        });
        return result;
    }
};
