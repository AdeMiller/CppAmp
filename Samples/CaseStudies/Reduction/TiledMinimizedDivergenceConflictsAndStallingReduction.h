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
// This is a version with reduced number of stalled threads in the first iteration.
//----------------------------------------------------------------------------

#pragma once

#include "IReduce.h"
#include "Timer.h"
#include <vector>
#include <amp.h>

using namespace concurrency;

template <int TileSize>
class TiledMinimizedDivergenceConflictsAndStallingReduction : public IReduce
{
public:
    int Reduce(accelerator_view& view, const std::vector<int>& source, double& computeTime) const
    {
        int elementCount = static_cast<int>(source.size());

        // Copy data
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
                //  Tile extent is now halved.
                extent<1> e(elementCount / 2);
                assert((e.size() % TileSize) == 0);

                parallel_for_each(view, e.tile<TileSize>(), [=] (tiled_index<TileSize> tidx) restrict(amp)
                {
                    // Instead of just loading from global memory perform the first reduction step during load,
                    // so load becomes load two elements and store the result.
                    int tid = tidx.local[0];
                    tile_static int tileData[TileSize];
                    // Partition input data among tiles, (2 * TileSize) because threads spawned is also halved
                    int relIdx = tidx.tile[0] * (TileSize * 2) + tid;
                    tileData[tid] = av[relIdx] + av[relIdx + TileSize];

                    //  Wait for all threads to finish copying
                    tidx.barrier.wait();

                    //  Reduce values for data on this tile, unrolled
                    for (int stride = (TileSize / 2); stride > 0; stride >>= 1)
                    {
                        //  Remember that this is a branch within a loop and all threads will have to execute this 
                        //  but onlythreads with a tid < stride will do useful work.
                        if (tid < stride)
                            tileData[tid] += tileData[tid + stride];

                        tidx.barrier.wait_with_tile_static_memory_fence();
                    }

                    //  Write the result for this tile back to global memory
                    if (tid == 0)
                        tmpAv[tidx.tile[0]] = tileData[0];
                });

                elementCount /= TileSize * 2;
                std::swap(tmpAv, av);
            }
            tmpAv.discard_data();

            //  Copy the final results from each tile to the CPU and accumulate them there
            std::vector<int> partialResult(elementCount);
            copy(av.section(0, elementCount), partialResult.begin());
            av.discard_data();
            result = std::accumulate(partialResult.cbegin(), partialResult.cend(), 0);
        });
        return result;
    }
};
