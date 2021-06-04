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
class CascadingUnrolledReduction : public IReduce
{
public:
    int Reduce(accelerator_view& view, const std::vector<int>& source, double& computeTime) const
    {
        static_assert(((TileSize % 64) == 0), "");

        if (accelerator(accelerator::default_accelerator).is_emulated)
            return -1;

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

                //  Reduce values for data on this tile, unrolled.
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
                //  warp size that is not 32. Warp (NVIDIA) or wavefront (AMD) sizes can vary between 16 and 64 depending on the 
                //  hardware being used.

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
