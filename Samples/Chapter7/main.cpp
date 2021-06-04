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

#include <amp.h>
#include <amp_math.h>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <random>
#include <numeric>
#include <amp_graphics.h>
#include <d3d11.h>

#include "Timer.h"

using namespace concurrency;
using namespace concurrency::precise_math;

void DoWork(array<float, 1>& input, array<float, 1>& output);

void SimpleTimingExample();
void FullTimingExample();
void ArrayAliasingExample();
void TextureAliasingExample();
void ArrayViewAliasingExample();
void EfficientCopyingExample();
void AsyncCopyingExample();
void MemoryAccessExample();
void UseArrayConstantExample();
void DivergentDataExample();
void DivergentKernelExample();
void ApplyDivergentStencil(accelerator_view& view, const array<float, 2>& input, array<float, 2>& output);
void ApplyImprovedStencil(accelerator_view& view, const array<float, 2>& input, array<float, 2>& output);
void ApplyImprovedStencilMask(accelerator_view& view, const array<float, 2>& input, array<float, 2>& output);
void ApplyImprovedUnrolledStencil(accelerator_view& view, const array<float, 2>& input, array<float, 2>& output);

void PrintMatrix(const float* const data, int size);
bool CheckStencilResult(const std::vector<float>& output, int cols, int rows);

using namespace concurrency::graphics;

int main()
{
    std::wcout << std::fixed << std::setprecision(3);

    // This has come first before any other C++ AMP code so that it can measure the runtime initialization
    FullTimingExample();
    SimpleTimingExample();

    // Most of the samples do this check first but that would alter the FullTimingExample.
   
#ifndef _DEBUG
    accelerator defaultDevice;
    std::wcout << L" Using device : " << defaultDevice.get_description() << std::endl;
    if (defaultDevice == accelerator(accelerator::direct3d_ref))
        std::wcout << " WARNING!! No C++ AMP hardware accelerator detected, using the REF accelerator." << std::endl << 
            "To see better performance run on C++ AMP\ncapable hardware." << std::endl;
#endif
    ArrayAliasingExample();
    ArrayViewAliasingExample();
    // TextureAliasingExample();
    EfficientCopyingExample();
    AsyncCopyingExample();
    MemoryAccessExample();
    UseArrayConstantExample();
    DivergentDataExample();
    DivergentKernelExample();

    std::wcout << std::endl << std::endl;
}

//--------------------------------------------------------------------------------------
//  Work function for demonstrating kernel timing.
//--------------------------------------------------------------------------------------

inline void DoWork(array<float, 1>& input, array<float, 1>& output)
{
    const float k = 1.0f;
    parallel_for_each(output.extent, [=, &input, &output](index<1> idx) restrict(amp)
    {
        output[idx] = input[idx] + k;
    });
}

//--------------------------------------------------------------------------------------
//  Simple timing example
//--------------------------------------------------------------------------------------

void SimpleTimingExample()
{
    std::wcout << std::endl << " Measuring overall execution time" << std::endl << std::endl;

    std::vector<float> hostInput(20000000, 1.0f);
    std::vector<float> hostOutput(hostInput.size());

    array<float, 1> gpuInput(int(hostInput.size()));
    array<float, 1> gpuOutput(int(hostInput.size()));

    LARGE_INTEGER start, end;

    gpuOutput.accelerator_view.wait();
    QueryPerformanceCounter(&start);

    copy(hostInput.cbegin(), hostInput.cend(), gpuInput);
    DoWork(gpuInput, gpuOutput);
    copy(gpuOutput, hostOutput.begin());

    gpuOutput.accelerator_view.wait();
    QueryPerformanceCounter(&end);  

    double elapsedTime = ElapsedTime(start, end);
    std::wcout << "   Total time:  " << elapsedTime << " (ms)" << std::endl;
}

//--------------------------------------------------------------------------------------
//  Full timing example (runs first)
//--------------------------------------------------------------------------------------

void FullTimingExample()
{
    std::wcout << std::endl << " Measuring breakdown kernel execution overhead" << std::endl << std::endl;

    LARGE_INTEGER initStart, initEnd, copyStart, copyEnd, kernelStart, kernelEnd;

    std::vector<float> hostInput(20000000, 1.0f);
    std::vector<float> hostOutput(hostInput.size());

    QueryPerformanceCounter(&initStart);
    array<float, 1> gpuInput(int(hostInput.size()));
    array<float, 1> gpuOutput(int(hostInput.size()));

    gpuOutput.accelerator_view.wait();                  
    QueryPerformanceCounter(&copyStart);                
    initEnd = copyStart;

    copy(hostInput.cbegin(), hostInput.cend(), gpuInput);

    gpuOutput.accelerator_view.wait();                    
    QueryPerformanceCounter(&kernelStart);              

    DoWork(gpuInput, gpuOutput);                        

    gpuOutput.accelerator_view.wait();
    QueryPerformanceCounter(&kernelEnd);                

    copy(gpuOutput, hostOutput.begin());

    QueryPerformanceCounter(&copyEnd);

    std::wcout << "   Initialize time:    " << ElapsedTime(initStart, initEnd) << " (ms)" << std::endl;
    std::wcout << "   Kernel & copy time: " << ElapsedTime(copyStart, copyEnd) << " (ms)" << std::endl;
    std::wcout << "   Kernel time:        " << ElapsedTime(kernelStart, kernelEnd) << " (ms)" << std::endl;
}

//--------------------------------------------------------------------------------------
//  Aliasing example
//--------------------------------------------------------------------------------------

void CopyArray(accelerator_view& view, const array<int, 1>& src, array<int, 1>& dest) 
{     
    parallel_for_each(view, dest.extent, [&src, &dest] (index<1> idx) restrict(amp) 
    {
        dest[idx] = src[idx];
    }); 
}

void ArrayAliasingExample()
{
    std::wcout << std::endl << " Measuring the impact of aliased invocations" << std::endl << std::endl;

    const int size = 100000000;

    array<int, 1> src(size);
    array<int, 1> dest(size);

    accelerator_view view = accelerator().default_view;
    double computeTime = TimeFunc(view, [&]()
    {
        parallel_for_each(view, dest.extent, [&src, &dest] (index<1> idx) restrict(amp) {
            dest[idx] = src[idx];
        });
    });
    std::wcout << "   Unaliased time:  " << computeTime << " (ms)" << std::endl;

    computeTime = TimeFunc(view, [&]()
    {
        CopyArray(view, src, dest);
    });
    std::wcout << "   Aliased time:    " << computeTime << " (ms)" << std::endl;

    computeTime = TimeFunc(view, [&]()
    {
        CopyArray(view, src, src);
    });
    std::wcout << "   Aliased time:    " << computeTime << " (ms)" << std::endl;
}

void CopyTexture(const texture<int, 1>& src, texture<int, 1>& dest) 
{     
    parallel_for_each(dest.extent, [&src, &dest] (index<1> idx) restrict(amp) 
    {
        dest.set(idx, src[idx]);
    }); 
}

void TextureAliasingExample() //  Results in runtime errors.
{
    texture<int, 1> tex1(16);
    CopyTexture(tex1, tex1);
}

void ArrayViewAliasingExample()
{
    //  An aliased invocation. Both array_views refer to the same array.

    const int size = 100000000;
    const int halfSize = size / 2;

    {
        array<int, 1> allData(size); 
        array_view<int, 1> firstHalf = allData.section(0, halfSize);            // Refers to allData.
        array_view<int, 1> secondHalf = allData.section(halfSize, halfSize);    // Refers to allData.
        parallel_for_each(secondHalf.extent, [=] (index<1> idx) restrict(amp) {
            secondHalf[idx] = firstHalf[idx];
        });
    }

    //  Not an aliased invocation. The array_views are created directly on top of host memory.

    {
        std::vector<int> vec(size, 0);
        array_view<int, 1> allData(size, vec);
        array_view<int, 1> firstHalf(halfSize, vec);                            // The array_view created directly on top of host memory.
        parallel_for_each(firstHalf.extent, [=] (index<1> idx) restrict(amp) {
            allData[idx + size] = firstHalf[idx];
        });
    }

    //  Aliased invocation. The array_views 

    {
        std::vector<int> vec(size, 0);
        array_view<int, 1> allData(size, vec);
        array_view<int, 1> firstHalf = allData.section(0, halfSize);            // The array_view is created from an array_view (section) not host memory.
        parallel_for_each(firstHalf.extent, [=] (index<1> idx) restrict(amp) {
            allData[idx + size] = firstHalf[idx];
        });
    }
}

//--------------------------------------------------------------------------------------
//  Improving copy efficiency
//--------------------------------------------------------------------------------------

void EfficientCopyingExample()
{
    // Limit data being copied in and out using const and discard_data.

    std::vector<float> cpuData(20000000, 0.0f);
    array_view<const float, 1> inputView(int(cpuData.size()), cpuData);

    const array<float, 1> inputData(int(cpuData.size()));

    array_view<float, 1> outputView(int(cpuData.size()), cpuData);
    outputView.discard_data();
}

//--------------------------------------------------------------------------------------
//  Asynchronous copying
//--------------------------------------------------------------------------------------

void AsyncCopyingExample()
{
    std::vector<float> cpuData(20000000, 0.0f);
    array<float, 1> gpuData(int(cpuData.size()));

    completion_future f = copy_async(cpuData.begin(), cpuData.end(), gpuData);

    // Do other work on CPU or GPU that does not modify cpuData or access gpuData

    f.get();

    parallel_for_each(gpuData.extent, [=, &gpuData](index<1> idx) restrict(amp)
    {
        gpuData[idx] = 1.0;
    });

    f = copy_async(gpuData, cpuData.begin());

    // Do other work...

    f.get();
}

//--------------------------------------------------------------------------------------
//  Comparison of coalesced and uncoalesced memory access patterns
//--------------------------------------------------------------------------------------

void MemoryAccessExample()
{
    std::wcout << std::endl << " Comparison of memory access patterns" << std::endl << std::endl;
 
    static const int tileSize = 32;
    const int matrixSize = tileSize * 200;
    array<float, 2> inData(matrixSize, matrixSize);
    array<float, 2> outData(matrixSize, matrixSize);

    std::vector<float> data(matrixSize * matrixSize);
    std::iota(data.begin(), data.end(), 0.0f);
    copy(data.cbegin(), data.cend(), inData);

    accelerator_view view = accelerator(accelerator::default_accelerator).default_view;
    double elapsedKernelTime = TimeFunc(view, [&]() 
    {
        parallel_for_each(view, inData.extent, [=, &inData, &outData](index<2> idx) restrict(amp)
        {
            outData(idx[0], idx[1]) = inData(idx[0], idx[1]);
        });
    });

    std::wcout << "   Matrix copy time:                       " 
        << elapsedKernelTime << " (ms)" << std::endl;

    elapsedKernelTime = TimeFunc(view, [&]() 
    {
        parallel_for_each(view, inData.extent, [=, &inData, &outData](index<2> idx) restrict(amp)
        {     
            outData(idx[1], idx[0]) = inData(idx[0], idx[1]);
        });
    });

    std::wcout << "   Matrix transpose time:                  " 
        << elapsedKernelTime << " (ms)" << std::endl;

    elapsedKernelTime = TimeFunc(view, [&]() 
    { 
        parallel_for_each(view, inData.extent.tile<tileSize, tileSize>(), [=, &inData, &outData](tiled_index<tileSize, tileSize> tidx) restrict(amp)
        {     
            tile_static float localData[tileSize][tileSize];
            localData[tidx.local[1]][tidx.local[0]] = inData[tidx.global];

            tidx.barrier.wait();

            index<2> outIdx(index<2>(tidx.tile_origin[1], tidx.tile_origin[0]) + tidx.local);
            outData[outIdx] = localData[tidx.local[0]][tidx.local[1]];
        });
    });

    std::wcout << "   Matrix coalesced transpose time:        " 
        << elapsedKernelTime << " (ms)" << std::endl;

    elapsedKernelTime = TimeFunc(view, [&]()
    { 
        parallel_for_each(view, inData.extent.tile<tileSize, tileSize>(), 
            [=, &inData, &outData](tiled_index<tileSize, tileSize> tidx) restrict(amp)
        {     
            tile_static float localData[tileSize][tileSize + 1];
            localData[tidx.local[1]][tidx.local[0]] = inData[tidx.global];

            tidx.barrier.wait();

            index<2> outIdx(index<2>(tidx.tile_origin[1], tidx.tile_origin[0]) + tidx.local);
            outData[outIdx] = localData[tidx.local[0]][tidx.local[1]];
        });
    });

    std::wcout << "   Matrix coalesced padded transpose time: " 
        << elapsedKernelTime << " (ms)" << std::endl;
}

//--------------------------------------------------------------------------------------
//  Example of passing an array into a kernel using constant memory.    
//--------------------------------------------------------------------------------------

struct Wrapper
{
    int data[3];
};

void UseArrayConstantExample()
{
    Wrapper wrap;
    wrap.data[0] = 1;
    // ...

    array<float, 1> input(1000);
    parallel_for_each(input.extent, [wrap, &input](index<1> idx) restrict(amp)
    {
        //... = wrap.data[0];
    });
}

//--------------------------------------------------------------------------------------
//  Comparison of divergent and non-divergent kernels
//--------------------------------------------------------------------------------------

void DivergentDataExample()
{
    std::wcout << std::endl << " Comparison of optimizing divergent kernel data" << std::endl << std::endl;

    std::random_device rd; 
    std::default_random_engine engine(rd()); 
    std::uniform_real_distribution<float> randDist(-10.0f, 10.0f);
    std::vector<float> data(20000000);

    std::generate(data.begin(), data.end(), [=, &engine, &randDist]() { return randDist(engine); });

    array<float, 1> gpuData(int(data.size()), data.begin(), data.end());

    accelerator_view view = accelerator(accelerator::default_accelerator).default_view;
    double elapsedTime = TimeFunc(view, [&]() 
    {
        parallel_for_each(view, gpuData.extent, [=, &gpuData](index<1> idx) restrict(amp)
        {
            if (gpuData[idx] > 0.0f)
                gpuData[idx] = fast_math::sqrt(fast_math::pow(gpuData[idx], gpuData[idx]));
        });
    });
    std::wcout << "   Random data time:  " << elapsedTime << " (ms)" << std::endl;

    std::sort(data.begin(), data.end(), std::greater<float>());

    gpuData = array<float, 1>(int(data.size()), data.begin(), data.end());
    elapsedTime = TimeFunc(view, [&]() 
    {
        parallel_for_each(view, gpuData.extent, [=, &gpuData](index<1> idx) restrict(amp)
        {
            if (gpuData[idx] > 0.0f)
                gpuData[idx] = fast_math::sqrt(fast_math::pow(gpuData[idx], gpuData[idx]));
        });
    });
    std::wcout << "   Sorted data time:  " << elapsedTime << " (ms)" << std::endl;
}

void DivergentKernelExample()
{
    std::wcout << std::endl << " Comparison of optimizing divergent kernels" << std::endl << std::endl;

    const int dim = 4000;
    array<float, 2> gpuInput(dim, dim);
    array<float, 2> gpuOutput(gpuInput.extent);
    std::vector<float> hostInput(gpuInput.extent.size(), 1.0f);
    std::vector<float> hostOutput(gpuOutput.extent.size(), 0.0f);

    copy(hostInput.begin(), gpuInput);

    accelerator_view view = accelerator(accelerator::default_accelerator).default_view;
    double elapsedKernelTime = TimeFunc(view, [&]() {
        ApplyDivergentStencil(view, gpuInput, gpuOutput);
    });
    copy(gpuOutput, hostOutput.begin());

    std::wcout << "   Divergent kernel elapsed time:         "
            << elapsedKernelTime << " (ms)" 
            << (CheckStencilResult(hostOutput, dim, dim) ? "" : "FAILED") << std::endl;

    elapsedKernelTime = TimeFunc(view, [&]() {
        ApplyImprovedStencil(view, gpuInput, gpuOutput);
    });
    copy(gpuOutput, hostOutput.begin());

    std::wcout << "   Improved kernel elapsed time:          " 
        << elapsedKernelTime << " (ms)" 
        << (CheckStencilResult(hostOutput, dim, dim) ? "" : "FAILED") << std::endl;

    elapsedKernelTime = TimeFunc(view, [&]() {
        ApplyImprovedStencilMask(view, gpuInput, gpuOutput);
    });
    copy(gpuOutput, hostOutput.begin());

    std::wcout << "   Improved mask kernel elapsed time:     " 
        << elapsedKernelTime << " (ms)" 
        << (CheckStencilResult(hostOutput, dim, dim) ? "" : "FAILED") << std::endl;

    elapsedKernelTime = TimeFunc(view, [&]() {
        ApplyImprovedUnrolledStencil(view, gpuInput, gpuOutput);
    });
    copy(gpuOutput, hostOutput.begin());

    std::wcout << "   Improved unrolled kernel elapsed time: " 
        << elapsedKernelTime << " (ms)" 
        << (CheckStencilResult(hostOutput, dim, dim) ? "" : "FAILED") << std::endl;
}

//--------------------------------------------------------------------------------------
//  Choosing the appropriate precision, sqrt example
//--------------------------------------------------------------------------------------

double PreciseSqrt(double x) restrict(amp, cpu)
{
    return sqrt(x);
}

double PreciseSqrtAmp(double x) restrict(amp)
{
    return concurrency::precise_math::sqrt(x);
}

double PreciseSqrtCpu(double x) // restrict(cpu) implicit
{
    return sqrt(x); // cmath math function.
}

float FastSqrt(float x) restrict(amp)
{
    return concurrency::fast_math::sqrtf(x);
}

//--------------------------------------------------------------------------------------
//  Basic stencil kernel that includes lots of branching.
//--------------------------------------------------------------------------------------

void ApplyDivergentStencil(accelerator_view& view, const array<float, 2>& input, array<float, 2>& output)
{
    parallel_for_each(view, output.extent, [&input, &output](index<2> idx) restrict(amp)
    {
        if ((idx[0] >= 1) && (idx[0] < (output.extent[0] - 1)) && 
            (idx[1] >= 1) && (idx[1] < (output.extent[1] - 1)))     // Ignore halo
        {
            output[idx] = 0.0f;
            for (int y = -1; y <= 1; ++y)                           // Loop over stencil
                for (int x = -1; x <= 1; ++x)
                    if ((y != 0) || (x != 0))
                        output[idx] += input(idx[0] + y, idx[1] + x);
        }
    });
}

void ApplyImprovedStencil(accelerator_view& view, const array<float, 2>& input, array<float, 2>& output)
{
    extent<2> computeDomain(output.extent[0] - 2, output.extent[1] - 2);
    parallel_for_each(view, computeDomain, [&input, &output](index<2> idx) restrict(amp)
    {
        const index<2> idc = idx + index<2>(1, 1);
        output[idc] = -input[idc];
        for (int y = -1; y <= 1; ++y)                           // Loop over stencil
            for (int x = -1; x <= 1; ++x)
                output[idc] += input(idc[0] + y, idc[1] + x);
    });
}

//--------------------------------------------------------------------------------------
//  Improved stencil kernel with no branching.
//--------------------------------------------------------------------------------------

void ApplyImprovedStencilMask(accelerator_view& view, const array<float, 2>& input, array<float, 2>& output)
{
    extent<2> computeDomain(output.extent[0] - 2, output.extent[1] - 2);
    parallel_for_each(view, computeDomain, [&input, &output](index<2> idx) restrict(amp)
    {
        int mask[8][2] = { {-1, -1}, {-1, 0}, {-1, 1}, 
                           { 0, -1},          { 0, 1}, 
                           { 1, -1}, { 1, 0}, { 1, 1} };

        const index<2> idc = idx + index<2>(1, 1);
        output[idc] = 0.0f;
        for (int i = 0; i < 8; ++i)
            output[idc] += input(idc + index<2>(mask[i]));
    });
}

//--------------------------------------------------------------------------------------
//  Improved stencil kernel with less branching and loop unrolling.
//--------------------------------------------------------------------------------------
//
//  Loop unrolling may make no difference if the JIT compiler automatically unrolls the loop.

void ApplyImprovedUnrolledStencil(accelerator_view& view, const array<float, 2>& input, array<float, 2>& output)
{
    extent<2> computeDomain(input.extent[0] - 2, input.extent[1] - 2);
    parallel_for_each(view, computeDomain, [&input, &output](index<2> idx) restrict(amp)
    {
        const index<2> idc = idx + index<2>(1, 1);
        output[idc] =  input(idx[0], idx[1]);
        output[idc] += input(idx[0], idx[1] + 1);
        output[idc] += input(idx[0], idx[1] + 2);
        output[idc] += input(idx[0] + 1, idx[1] + 1);
        output[idc] += input(idx[0] + 1, idx[1] + 2);
        output[idc] += input(idx[0] + 2, idx[1]);
        output[idc] += input(idx[0] + 2, idx[1] + 1);
        output[idc] += input(idx[0] + 2, idx[1] + 2);
    });
}

//--------------------------------------------------------------------------------------
//  Print a subsection of a matrix. 
//  The top left 10 x 10 region or the whole matrix, whichever is smaller.
//--------------------------------------------------------------------------------------

void PrintMatrix(const float* const data, int size)
{
    for (int i = 0; i < min(10, size); ++i)
    {
        for (int j = 0; j < min(10, size); ++j)
            std::wcout << data[i * size + j] << " ";
        std::wcout << std::endl;
    }
}

//--------------------------------------------------------------------------------------
//  Check that the stencil generated the correct result.
//--------------------------------------------------------------------------------------

bool CheckStencilResult(const std::vector<float>& output, int cols, int rows)
{
    for (int i = 0; i < rows; ++i)
        for (int j = 0; j < cols; ++j)
        {
            float v = output[i * rows + j];
            if ((i == 0) || (i == cols - 1) || (j == 0) || (j == rows - 1)) 
            {
                if (v != 0.0f) 
                    return false;
            }
            else
                if (v != 8.0f)
                    return false;
        }
    return true;
}
