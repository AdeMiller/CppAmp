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

class SimpleArrayViewReduction : public IReduce
{
public:
    int Reduce(accelerator_view& view, const std::vector<int>& source, double& computeTime) const
    {
        int elementCount = static_cast<int>(source.size());

        // Copy data, create a writable copy that can be associated with the array_view.
        std::vector<int> writableSource(source.size());
        std::copy(source.cbegin(), source.cend(), writableSource.begin());
        array_view<int, 1> av(elementCount, writableSource);
        int tailResult = (elementCount % 2) ? source[elementCount - 1] : 0;
        array_view<int, 1> tailResultView(1, &tailResult);

        std::vector<int> result(1);
        computeTime = TimeFunc(view, [&]() 
        {
            for (int stride = (elementCount / 2); stride > 0; stride /= 2)
            {
                parallel_for_each(view, extent<1>(stride), [=] (index<1> idx) restrict(amp)
                {
                    av[idx] += av[idx + stride];

                    // If there are an odd number of elements then the first thread adds the last element.
                    if ((idx[0] == 0) && (stride & 0x1) && (stride != 1))
                        tailResultView[idx] += av[stride - 1];
                });
            }

            //  Only copy out the first element in the array as this contains the final answer.
            copy(av.section(0, 1), result.begin());
            av.discard_data();
        });
        tailResultView.synchronize();
        return result[0] + tailResult;
    }
};
