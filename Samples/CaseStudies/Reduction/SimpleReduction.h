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
// This is a simple implementation of reduction algorithm.
// Multiple kernel launches are required to synchronize memory access among
// threads in different thread groups.
//----------------------------------------------------------------------------

#pragma once

#include "IReduce.h"
#include "Timer.h"
#include <vector>
#include <amp.h>

using namespace concurrency;

class SimpleReduction : public IReduce
{
public:
    int Reduce(accelerator_view& view, const std::vector<int>& source, double& computeTime) const
    {
        assert(source.size() <= UINT_MAX);
        int elementCount = static_cast<int>(source.size());

        // Copy data
        array<int, 1> a(elementCount, source.cbegin(), source.cend(), view);
        std::vector<int> result(1);
        int tailResult = (elementCount % 2) ? source[elementCount - 1] : 0;
        array_view<int, 1> tailResultView(1, &tailResult);
        computeTime = TimeFunc(view, [&]() 
        {
           for (int stride = (elementCount / 2); stride > 0; stride /= 2)
            {
                parallel_for_each(view, extent<1>(stride), [=, &a] (index<1> idx) restrict(amp)
                {
                    a[idx] += a[idx + stride];

                    // If there are an odd number of elements then the first thread adds the last element.
                    if ((idx[0] == 0) && (stride & 0x1) && (stride != 1))
                        tailResultView[idx] += a[stride - 1];
                });
            }

            //  Only copy out the first element in the array as this contains the final answer.
            copy(a.section(0, 1), result.begin());
        });
        tailResultView.synchronize();
        return result[0] + tailResult;
    }
};
