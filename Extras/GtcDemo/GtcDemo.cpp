//===============================================================================
//
// Microsoft Press
// C++ AMP: Accelerated Massive Parallelism with Microsoft Visual C++
//
//===============================================================================
// Copyright (c) 2012 Ade Miller & Kate Gregory.  All rights reserved.
// This code released under the terms of the 
// Microsoft Public License (Ms-PL), http://ampbook.codeplex.com/license.
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//===============================================================================

#include "stdafx.h"

#include <amp.h>
#include <ppl.h>
using namespace concurrency;

static float Func(float val) restrict(cpu, amp)
{
    return ++val;
}

void HelloWorldCpu()
{
    std::cout << std::endl << "Hello World (Parallel CPU)" << std::endl;

    std::vector<float> arr(10000);
    std::iota(begin(arr), end(arr), 1.0f);

    array_view<float> arr_av(arr.size(), arr);
    parallel_for_each(begin(arr), end(arr), [=](float& v) 
    { 
        v = Func(v); 
    });

    for(int i = 0; i < 10; i++)
        std::cout << arr_av[i] << ",";
    std::cout << std::endl;
}

void HelloWorldGpu()
{
    std::cout << std::endl << "Hello World (GPU)" << std::endl;

    std::vector<float> arr(10000);
    std::iota(begin(arr), end(arr), 1.0f);
    array_view<float> arr_av(arr.size(), arr);
    parallel_for_each(arr_av.extent, [=](index<1> idx) restrict(amp)
    { 
        arr_av[idx] = Func(arr_av[idx]); 
    });

    for(int i = 0; i < 10; i++)
        std::cout << arr_av[i] << ",";
    std::cout << std::endl;
}

void MovingAverage(const std::vector<float>& arr)
{
    std::cout << std::endl << "Moving Average (GPU)" <<  std::endl;

    array_view<const float> arr_av(arr.size(), arr);
    std::vector<float> avg(arr.size() - 2);
    array_view<float> avg_av(avg.size(), avg);

    avg_av.discard_data();
    parallel_for_each(avg_av.extent, [=](index<1> idx) 
        restrict(amp)
    { 
        const int cIdx = idx[0] + 1;

        avg_av[cIdx - 1] = (arr_av[idx] + 
            arr_av[idx + 1] + 
            arr_av[idx + 2]) / 3;
    });

    std::cout << "N/A, ";
    for (int i = 0; i < avg_av.extent[0]; ++i)
        std::cout << avg_av[i] << ", ";
    std::cout << "N/A" << std::endl;
}

template <typename T>
T PaddedRead(const array_view<const T, 1>& A, int idx) restrict(cpu, amp)
{
    return A.extent.contains(index<1>(idx)) ? A[idx] : T();
}

template <typename T>
void PaddedWrite(const array_view<T, 1>& A, int idx, T val) restrict(cpu, amp)
{
    if (A.extent.contains(index<1>(idx))) A[idx] = val;
}

void MovingAverageTiled(const std::vector<float>& arr)
{
    std::cout << std::endl << "Moving Average (GPU Tiled)" << std::endl;

    array_view<const float> arr_av(arr.size(), arr);
    std::vector<float> avg(arr.size() - 2);
    array_view<float> avg_av(avg.size(), avg);
    avg_av.discard_data();

    static const int tileSize = 4; // 256
    tiled_extent<tileSize> computeDomain = avg_av.extent;
    computeDomain = computeDomain.pad();
    parallel_for_each(computeDomain, 
        [=](tiled_index<tileSize> idx) restrict(amp)
    { 
        const int gIdx = idx.global[0];
        const int tIdx = idx.local[0]; 

        tile_static float local[tileSize + 2];

        local[tIdx + 1] = PaddedRead(arr_av, gIdx);
        if (tIdx == 0)
            local[0] = PaddedRead(arr_av, gIdx - 1);
        if (tIdx == (tileSize - 1))
            local[tileSize + 1] = 
            PaddedRead(arr_av, gIdx + 1);

        idx.barrier.wait();

        float val = (local[tIdx] + 
            local[tIdx + 1] + 
            local[tIdx + 2]) / 3;
        PaddedWrite(avg_av, gIdx - 1, val);
    });

    std::cout << "N/A, ";
    for (int i = 0; i < avg_av.extent[0]; ++i)
        std::cout << avg_av[i] << ", ";
    std::cout << "N/A" << std::endl;
}

int _tmain(int argc, _TCHAR* argv[])
{
    HelloWorldCpu();
    HelloWorldGpu();

    std::vector<float> arr(13, 0.0f);
    std::iota(begin(arr), end(arr), 0.0f);
    for (auto& v : arr)
    {
        v = float(int(v) % 6);
    }

    MovingAverage(arr);
    MovingAverageTiled(arr);
    return 0;
}

