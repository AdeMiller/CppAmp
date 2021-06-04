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
// This is an improved implementation of the reduction algorithm using
// a simple parallel_for_each. Each thread is reducing more elements,
// decreasing the total number of memory accesses.
//----------------------------------------------------------------------------

#pragma once

#include <vector>
#include <algorithm>
#include <numeric>
#include <amp.h>

#include "IReduce.h"
#include "Timer.h"

using namespace concurrency;

class SimpleOptimizedReduction : public IReduce
{
public:
    int Reduce(accelerator_view& view, const std::vector<int>& source, double& computeTime) const
    {
        const int windowWidth = 8;
        int elementCount = static_cast<unsigned>(source.size());

        // Using array as temporary memory.
        array<int, 1> a(elementCount, source.cbegin(), source.cend(), view);

        // Takes care of the sum of tail elements.
        int tailSum = 0;
        if ((elementCount % windowWidth) != 0 && elementCount > windowWidth)
            tailSum = 
                std::accumulate(source.begin() + ((elementCount - 1) / windowWidth) * windowWidth, 
                    source.end(), 0);

        array_view<int, 1> avTailSum(1, &tailSum);

        // Each thread reduces windowWidth elements.
        int prevStride = elementCount;
        int result;
        computeTime = TimeFunc(view, [&]() 
        {
            for (int stride = (elementCount / windowWidth); stride > 0; stride /= windowWidth)
            {
                parallel_for_each(view, extent<1>(stride), [=, &a] (index<1> idx) restrict(amp)
                {
                    int sum = 0;
                    for (int i = 0; i < windowWidth; i++)
                        sum += a[idx + i * stride];
                    a[idx] = sum;

                    // Reduce the tail in cases where the number of elements is not divisible.
                    // Note: execution of this section may negatively affect the performance.
                    // In production code the problem size passed to the reduction should
                    // be a power of the windowWidth. 
                    if ((idx[0] == (stride - 1)) && ((stride % windowWidth) != 0) && (stride > windowWidth))
                    {
                        for(int i = ((stride - 1) / windowWidth) * windowWidth; i < stride; i++)
                            avTailSum[0] += a[i];
                    }
                });
                prevStride = stride;
            }

            // Perform any remaining reduction on the CPU.
            std::vector<int> partialResult(prevStride);
            copy(a.section(0, prevStride), partialResult.begin());
            avTailSum.synchronize();
            result = std::accumulate(partialResult.begin(), partialResult.end(), tailSum);
        });
        return result;
    }
};
