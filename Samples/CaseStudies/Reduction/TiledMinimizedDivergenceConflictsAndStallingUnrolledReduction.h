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
class TiledMinimizedDivergenceConflictsAndStallingUnrolledReduction : public IReduce
{
public:
    int Reduce(accelerator_view& view, const std::vector<int>& source, double& computeTime) const
    {
        if (accelerator(accelerator::default_accelerator).is_emulated)
            return -1;

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
                    int i = (tidx.tile[0] * TileSize * 2) + tid;
                    tileData[tid] = av[i] + av[i + TileSize];

                    //  Wait for all threads to finish copying
                    tidx.barrier.wait();

                    //  Example of partial unrolling. This makes no use of the TileSize template parameter and is easier to understand!

                    /*
                    for (int stride = (TileSize / 2); stride > 32; stride >>= 1)
                    {
                        //  Remember that this is a branch within a loop and all threads will have to execute this but only
                        //  threads with a tid < stride will do useful work.
                        if (tid < stride)
                            tileData[tid] += tileData[tid + stride];

                        tidx.barrier.wait();
                    }

                    if (tid < 32)
                    { 
                        tileData[tid] += tileData[tid + 32];
                        tile_static_memory_fence(tidx.barrier);
                        tileData[tid] += tileData[tid + 16];
                        tile_static_memory_fence(tidx.barrier);
                        tileData[tid] += tileData[tid + 8];
                        tile_static_memory_fence(tidx.barrier);
                        tileData[tid] += tileData[tid + 4];
                        tile_static_memory_fence(tidx.barrier);
                        tileData[tid] += tileData[tid + 2];
                        tile_static_memory_fence(tidx.barrier);
                        tileData[tid] += tileData[tid + 1];
                        tile_static_memory_fence(tidx.barrier);
                    }
                    */

                    //  Reduce values for data on this tile, unrolled. Compare this code to the CascadingReduction::Reduce implementation.
                    //  Because TileSize is a template parameter the if statements referencing it are evaluated at compile time.

                    if (TileSize >= 1024)
                    {
                        if (tid < 512)
                            tileData[tid] += tileData[tid + 512];
                        tidx.barrier.wait_with_tile_static_memory_fence();
                    }
                    if (TileSize >= 512)
                    {
                        if (tid < 256)
                            tileData[tid] += tileData[tid + 256];
                        tidx.barrier.wait_with_tile_static_memory_fence();
                    }
                    if (TileSize >= 256)
                    {
                        if (tid < 128)
                            tileData[tid] += tileData[tid + 128];
                        tidx.barrier.wait_with_tile_static_memory_fence();
                    }
                    if (TileSize >= 128)
                    {
                        if (tid < 64)
                            tileData[tid] += tileData[tid + 64];
                        tidx.barrier.wait_with_tile_static_memory_fence();
                    }

                    //  As the reduction advances each kernel uses half as many threads as the previous one. Eventually the last
                    //  six iterations are executing within a single warp. The code can handle this as a special case for further
                    //  performance gains. In the last warp the code no longer needs to check for tid < 32 at each step and can 
                    //  also forego the barrier. However a tile_static_memory_fence is required in order to prevent the compiler from
                    //  optimizing the code to:
                    //
                    //  tileData[tid] = tileData[tid + 32] + tileData[tid + 16] + ...
                    //
                    //  This would be a correctness bug and lead to the wrong answer being calculated. See the following post for more
                    //  information about memory fences: http://blogs.msdn.com/b/nativeconcurrency/archive/2011/12/25/tile-barrier-in-c-amp.aspx
                    //
                    //  This unrolling saves work on all warps not just the last one as in the loop case all warps much still execute 
                    //  the iterations even if their tid is > stride.
                    //
                    //  Because TileSize is a template parameter the if statements referencing it are evaluated at compile time.
                    //
                    //  HOWEVER! This code is now warp size aware! This means that it may not behave as expected on hardware with a 
                    //  warp size that is smaller than 32. Warp (NVIDIA) or wavefront (AMD) sizes can vary between 32 and 64 depending on the 
                    //  hardware being used. 

                    //  For the same reason this does not work on the REF or WARP accelerators because they have no notion of a warp.
                    //  You could consider them as having a warp width of 1.

                    if (tid < 32) 
                    {
                        if (TileSize >= 64) { tileData[tid] += tileData[tid + 32]; tile_static_memory_fence(tidx.barrier); }
                        if (TileSize >= 32) { tileData[tid] += tileData[tid + 16]; tile_static_memory_fence(tidx.barrier); }
                        if (TileSize >= 16) { tileData[tid] += tileData[tid + 8];  tile_static_memory_fence(tidx.barrier); }
                        if (TileSize >= 8)  { tileData[tid] += tileData[tid + 4];  tile_static_memory_fence(tidx.barrier); }
                        if (TileSize >= 4)  { tileData[tid] += tileData[tid + 2];  tile_static_memory_fence(tidx.barrier); }
                        if (TileSize >= 2)  { tileData[tid] += tileData[tid + 1];  tile_static_memory_fence(tidx.barrier); }
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
