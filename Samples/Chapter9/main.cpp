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

#define NOMINMAX        // Use STL min and max.
#include <amp.h>
#include <amp_math.h>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <iterator>
#include <numeric>
#include <concurrent_queue.h>

#include "Timer.h"

using namespace concurrency;

void EnumeratingAcceleratorsExample();

void MatrixSingleGpuExample(const int rows, const int cols, const int shift);

struct TaskData;
void MatrixMultiGpuExample(const std::vector<accelerator>& accls, const int rows, const int cols, const int shift);
void MatrixMultiGpuSequentialExample(const std::vector<accelerator>& accls, const int rows, const int cols, const int shift);

void LoopedMatrixMultiGpuExample(const std::vector<accelerator>& accls, const int rows, const int cols, const int shift, const int iter);

void WorkStealingExample(const std::vector<accelerator>& accls, const int rows, const int cols, const int shift);

void CompletionFutureExample();

float WeightedAverage(index<2> idx, const array_view<const float, 2>& data, int shift) restrict(amp);

void PrintMatrix(const float* const mat, int rows, int cols);
void PrintMatrix(const std::vector<TaskData>& tasks, std::vector<array<float, 2>> sC, int rows, int cols);

int main()
{
#ifndef _DEBUG
    accelerator defaultDevice;
    std::wcout << L" Using device : " << defaultDevice.get_description() << std::endl;
    if (defaultDevice == accelerator(accelerator::direct3d_ref))
        std::wcout << " WARNING!! No C++ AMP hardware accelerator detected, using the REF accelerator." << std::endl << 
            "To see better performance run on C++ AMP\ncapable hardware." << std::endl;
#endif

    std::wcout << std::endl << "Enumerating accelerators" << std::endl << std::endl;

    EnumeratingAcceleratorsExample();

    //  Matrix weighted average examples.

#ifdef _DEBUG
    const int rows = 20, cols = 10, shift = 1;
#else
    const int rows = 2000, cols = 2000, shift = 60;
#endif
    std::wcout << std::endl << std::endl << " Matrix weighted average " << rows << " x " << cols << " matrix, with " 
        << shift * 2 + 1 << " x " << shift * 2 + 1 << " window" << std::endl 
        << " Matrix size " << (rows * cols * sizeof(float) / 1024) << " KB" << std::endl << std::endl;

    MatrixSingleGpuExample(rows, cols, shift);

    std::vector<accelerator> accls = accelerator::get_all();
    accls.erase(std::remove_if(accls.begin(), accls.end(), [](accelerator& a){ return a.is_emulated; }), accls.end());

    if (accls.empty())
        accls.push_back(accls.empty() ? accelerator(accelerator::direct3d_ref) : accls[0]);

    if (accls.size() < 2)
    {
        std::wcout << "Only one GPU accelerator available, duplicating available accelerator." << std::endl;
        accls.push_back(accls[0]);
    }

    // Configure tasks for multi-GPU examples.

    MatrixMultiGpuExample(accls, rows, cols, shift);
    MatrixMultiGpuSequentialExample(accls, rows, cols, shift);

    const int iter = 10;
    std::wcout << std::endl << " Weighted average executing " << iter << " times" << std::endl << std::endl;

    LoopedMatrixMultiGpuExample(accls, rows, cols, shift, iter);

    WorkStealingExample(accls, rows, cols, shift);

    CompletionFutureExample();
    
    std::wcout << std::endl << std::endl;
}

//--------------------------------------------------------------------------------------
//  Examples of enumerating and choosing accelerators
//--------------------------------------------------------------------------------------

void EnumeratingAcceleratorsExample()
{
    //  List all the accelerators

    std::vector<accelerator> accls = accelerator::get_all();

    std::wcout << "Found " << accls.size() << " C++ AMP accelerator(s):" << std::endl; 
    std::for_each(accls.cbegin(), accls.cend(), [](const accelerator& a)
    {
        std::wcout << "  " << a.device_path << std::endl 
            << "    " << a.description << std::endl << std::endl;
    });

    //  List all the available GPU accelerators and remove emulated ones.

    accls.erase(std::remove_if(accls.begin(), accls.end(), [](accelerator& a)
    {
        return a.is_emulated; 
    }), accls.end());

    std::wcout << "Found " << accls.size() << " C++ AMP hardware accelerator(s):" << std::endl; 
    std::for_each(accls.cbegin(), accls.cend(), [](const accelerator& a)
    {
        std::wcout << "  " << a.device_path << std::endl;
    });

    //  Look for a specific accelerator, the WARP accelerator

    accls = accelerator::get_all();
    bool hasWarp = std::find_if(accls.begin(), accls.end(), [=](accelerator& a) 
        { 
            return a.device_path.compare(accelerator::direct3d_warp) == 0; 
        }) != accls.end();
    std::wcout << "Has WARP accelerator: " << (hasWarp ? "true" : "false") << std::endl;

    //  Look for accelerators with specific properties; 1MB memory and connected to a display

    std::wcout << std::endl << "Looking for accelerator with display and 1MB of dedicated memory..." << std::endl;
    bool found = std::find_if(accls.begin(), accls.end(), [=](accelerator& a) 
        { 
            return !a.is_emulated && a.dedicated_memory >= 2048 && 
                a.supports_limited_double_precision && a.has_display; 
        }) != accls.end();
    std::wcout << "  Suitable accelerator " << (found ? "found." : "not found.") << std::endl;

    //  Look for an accelerator with specific properties and set it as the default

    std::wcout << std::endl << "Setting default accelerator to one with display and 1MB of dedicated memory..." << std::endl;
    std::vector<accelerator>::iterator usefulAccls = std::find_if(accls.begin(), accls.end(), 
        [=](accelerator& a) 
        { 
            return !a.is_emulated && (a.dedicated_memory >= 1024) && a.has_display; 
        });
    if (usefulAccls != accls.end())
    {
        std::wcout << "  Default accelerator is now: " 
            << accelerator(accelerator::default_accelerator).description << std::endl;
        accelerator::set_default(usefulAccls->device_path);
    }
    else
        std::wcout << "  No suitable accelerator available" << std::endl;

    accls = accelerator::get_all();
    accls.erase(std::remove_if(accls.begin(), accls.end(), [](accelerator& a)
    {
        return a.is_emulated; 
    }), accls.end());

    if (accls.size() < 2)
        return;

    //  Make sure accelerator 0 is really the default accelerator.

    accelerator::set_default(accls[0].device_path); // Accelerator 0 is now the default

    //  parallel_for_each can infer the accelerator from the array and will execute on 1
    array<int> dataOn1(10000, accls[1].default_view);
    parallel_for_each(dataOn1.extent, [&dataOn1](index<1> idx) restrict(amp) 
    { 
        dataOn1[idx] = 1; 
    });

    //  parallel_for_each must be passed the array_view explicitly
    std::vector<int> dataOnCpu(10000, 1);
    array_view<int, 1> dataView(1, dataOnCpu);
    parallel_for_each(accls[1].default_view, dataView.extent, [dataView](index<1> idx) restrict(amp) 
    { 
        dataView[idx] = 1;
    });
    dataView.synchronize();
    accls[1].default_view.wait();
}

//--------------------------------------------------------------------------------------
//  Example of executing the problem on a single GPU
//--------------------------------------------------------------------------------------

void MatrixSingleGpuExample(const int rows, const int cols, const int shift)
{
    //  Initialize matrices

    std::vector<float> vA(rows * cols);
    std::vector<float> vC(rows * cols);
    std::iota(vA.begin(), vA.end(), 0.0f);

    //  Calculation

    accelerator_view view = accelerator(accelerator::default_accelerator).default_view;
    double time = TimeFunc(view, [&]()
    {
        array_view<const float, 2> a(rows, cols, vA); 
        array_view<float, 2> c(rows, cols, vC);
        c.discard_data();

        extent<2> ext(rows - shift * 2, cols - shift * 2);
        parallel_for_each(view, ext, [=](index<2> idx) restrict(amp)
        {
            index<2> idc(idx[0] + shift, idx[1] + shift);
            c[idc] = WeightedAverage(idc, a, shift);
        });
        c.synchronize();
    });

    std::wcout << " Single GPU matrix weighted average took                          " << time << " (ms)" << std::endl;
#ifdef _DEBUG
    PrintMatrix(vC.data(), rows, cols);
#endif
}

//--------------------------------------------------------------------------------------
//  Data structure for storing work assigned to different accelerators.
//--------------------------------------------------------------------------------------

struct TaskData
{
    int id;
    accelerator_view view;
    int startRow;
    extent<2> readExt;
    int writeOffset;
    extent<2> writeExt;

    TaskData(accelerator a, int i) : view(a.default_view), id(i) {}

    static std::vector<TaskData> Configure(const std::vector<accelerator>& accls,  int rows, int cols, int shift)
    {
        std::vector<TaskData> tasks;
        int startRow = 0;
        int rowsPerTask = int(rows / accls.size());
        int i = 0;
        std::for_each(accls.cbegin(), accls.cend(), [=, &tasks, &i, &startRow](const accelerator& a)
        {
            TaskData t(a, i++);
            t.startRow = std::max(0, startRow - shift);
            int endRow = std::min(startRow + rowsPerTask + shift, rows);
            t.readExt = extent<2>(endRow - t.startRow, cols);
            t.writeOffset = shift;
            t.writeExt = 
                extent<2>(t.readExt[0] - shift - ((endRow == rows || startRow == 0) ? shift : 0), cols);
            tasks.push_back(t);
            startRow += rowsPerTask;
        });
        return tasks;
    }
};

//--------------------------------------------------------------------------------------
//  Example of partitioning the problem across two or more GPUs using a parallel_for_each
//--------------------------------------------------------------------------------------

void MatrixMultiGpuExample(const std::vector<accelerator>& accls, const int rows, const int cols, const int shift)
{
    std::vector<TaskData> tasks = TaskData::Configure(accls, rows, cols, shift);

    //  Initialize matrices

    std::vector<float> vA(rows * cols);
    std::vector<float> vC(rows * cols);
    std::iota(vA.begin(), vA.end(), 0.0f);

    //  Calculation

    double time = TimeFunc(tasks[0].view, [&]()
    {   
        parallel_for_each(tasks.begin(), tasks.end(), [=, &vC](TaskData& t)
        {
            array_view<const float, 2> a(t.readExt, &vA[t.startRow * cols]); 
            array<float, 2> c(t.readExt, t.view);
            index<2> writeOffset(t.writeOffset, shift);
            parallel_for_each(t.view, t.writeExt, [=, &c](index<2> idx) restrict(amp)
            {
                index<2> idc = idx + writeOffset;
                c[idc] = WeightedAverage(idc, a, shift);
            });
            // Wait is required as copy_to may cause all GPUs to block.
            t.view.wait();
            array_view<float, 2> outData(t.writeExt, &vC[(t.startRow + t.writeOffset) * cols]);
            c.section(index<2>(t.writeOffset, 0), t.writeExt).copy_to(outData);
        });
    });

    std::wcout << " " << tasks.size() << " GPU matrix weighted average (p_f_e) took                       " << time << " (ms)" << std::endl;
#ifdef _DEBUG
    PrintMatrix(vC.data(), rows, cols);
#endif
}

//--------------------------------------------------------------------------------------
//  Example of partitioning the problem across two or more GPUs using a std::for_each
//--------------------------------------------------------------------------------------

void MatrixMultiGpuSequentialExample(const std::vector<accelerator>& accls, const int rows, const int cols, const int shift)
{
    std::vector<TaskData> tasks = TaskData::Configure(accls, rows, cols, shift);

    //  Initialize matrices

    std::vector<float> vA(rows * cols);
    std::vector<float> vC(rows * cols);
    std::iota(vA.begin(), vA.end(), 0.0f);
    std::vector<array_view<float, 2>> avCs;

    //  Calculation

    std::for_each(tasks.cbegin(), tasks.cend(), [&avCs](const TaskData& t)
    {
        avCs.push_back(array<float, 2>(t.readExt, t.view));
    });

    double time = TimeFunc(tasks[0].view, [&]()
    {   
        std::for_each(tasks.cbegin(), tasks.cend(), [=](const TaskData& t)
        {
            array_view<const float, 2> a(t.readExt, &vA[t.startRow * cols]); 
            array_view<float, 2> c = avCs[t.id];
            index<2> writeOffset(t.writeOffset, shift);
            parallel_for_each(t.view, t.writeExt, [=](index<2> idx) restrict(amp)
            {
                index<2> idc = idx + writeOffset;
                c[idc] = WeightedAverage(idc, a, shift);
            });
        });
        std::for_each(tasks.cbegin(), tasks.cend(), [=, &vC](const TaskData& t)
        {
            array_view<float, 2> outData(t.writeExt, &vC[(t.startRow + t.writeOffset) * cols]);
            avCs[t.id].section(index<2>(t.writeOffset, 0), t.writeExt).copy_to(outData);
        });
    });

    std::wcout << " " << tasks.size() << " GPU matrix weighted average took                               " << time << " (ms)" << std::endl;
#ifdef _DEBUG
    PrintMatrix(vC.data(), rows, cols);
#endif
}

//--------------------------------------------------------------------------------------
//  Example similar to MatrixMultiGpuExample but with multiple iterations.
//--------------------------------------------------------------------------------------

void LoopedMatrixMultiGpuExample(const std::vector<accelerator>& accls, const int rows, const int cols, const int shift, const int iter)
{
    std::vector<TaskData> tasks = TaskData::Configure(accls, rows, cols, shift);

    //  Initialize matrices

    std::vector<float> vA(rows * cols);
    std::vector<float> vC(rows * cols);
    std::iota(vA.begin(), vA.end(), 0.0f);

    std::vector<array<float, 2>> arrAs;
    std::vector<array<float, 2>> arrCs;
    
    std::for_each(tasks.begin(), tasks.end(), [&](const TaskData& t)
    {
        arrAs.push_back(array<float, 2>(t.readExt, &vA[t.startRow * cols], t.view));
        arrCs.push_back(array<float, 2>(t.readExt, t.view));
    });

    // Create swap array on CPU accelerator

    array<float, 2> swapTop = array<float, 2>(extent<2>(shift, cols), 
        accelerator(accelerator::cpu_accelerator).default_view);
    array_view<float, 2> swapViewTop = array_view<float, 2>(swapTop);
    array<float, 2> swapBottom = array<float, 2>(extent<2>(shift, cols), 
        accelerator(accelerator::cpu_accelerator).default_view);
    array_view<float, 2> swapViewBottom = array_view<float, 2>(swapBottom);

    //  Calculation
    double time = TimeFunc(tasks[0].view, [&]()
    {      
        for (int i = 0 ; i < iter; ++i)
        {
            //  Calculate a portion of the result on each GPU

            std::for_each(tasks.cbegin(), tasks.cend(), [=, &arrAs, &arrCs, &vC](const TaskData& t)
            {
                array<float, 2>& a = arrAs[t.id];
                array<float, 2>& c = arrCs[t.id];

                parallel_for_each(t.view, t.readExt, [=, &a, &c](index<2> idx) restrict(amp)
                {
                    c[idx] = a[idx];
                    if ((idx[0] >= shift) && (idx[0] < (rows - shift)) && 
                        (idx[1] >= shift) && (idx[1] < (cols - shift)))
                        c[idx] = WeightedAverage(idx, a, shift);
                });
            });

            //  Swap edges

            std::vector<completion_future> copyResults((tasks.size() - 1) * 2);
            parallel_for(0, int(tasks.size() - 1), [=, &arrCs, &copyResults](size_t i)
            {
                array_view<float, 2> topEdge = 
                    arrCs[i].section(index<2>(tasks[i].writeOffset + tasks[i].writeExt[0] - shift, 0), 
                        swapViewTop.extent);
                array_view<float, 2> bottomEdge = arrCs[i + 1].section(index<2>(tasks[i + 1].writeOffset, 0), 
                    swapViewBottom.extent);
                copyResults[i] = copy_async(topEdge, swapViewTop);
                copyResults[i + 1] = copy_async(bottomEdge, swapViewBottom);
            });

            parallel_for_each(copyResults.begin(), copyResults.end(), [=](completion_future& f) { f.get(); });

            parallel_for(0, int(tasks.size() - 1), [=, &arrCs, &copyResults](size_t i)
            {
                array_view<float, 2> topEdge = 
                    arrCs[i].section(index<2>(tasks[i].writeOffset + tasks[i].writeExt[0] - shift, 0), 
                        swapViewTop.extent);
                array_view<float, 2> bottomEdge = arrCs[i + 1].section(swapViewTop.extent);
                copyResults[i] = copy_async(swapViewTop, bottomEdge);
                copyResults[i + 1] = copy_async(swapViewBottom, topEdge);
            });

            parallel_for_each(copyResults.begin(), copyResults.end(), [=](completion_future& f) { f.get(); });

            // Sequential version of the above swapping code. This may lead to contention on
            // Windows 7 due to blocking copy operations.
            /*
            for (size_t d = 0; d < tasks.size() - 1; ++d)
            {
                // Copy bottom edge of write extent in upper accelerator to top edge on lower accelerator.
                arrCs[d].section(index<2>(tasks[d].writeOffset + tasks[d].writeExt[0] - shift, 0), 
                                swapViewTop.extent).copy_to(swapViewTop);
                swapViewTop.copy_to(arrCs[d + 1].section(swapViewTop.extent));
                // Copy top edge of write extent in lower accelerator to bottom edge on upper accelerator. 
                arrCs[d + 1].section(index<2>(tasks[d+1].writeOffset, 0), 
                                    swapViewTop.extent).copy_to(swapViewTop);
                swapViewTop.copy_to(arrCs[d].section(index<2>(arrCs[d].extent[0] - shift, 0), 
                                 swapViewTop.extent));
            }
            */

            //  Swap results of this iteration with the input matrix
            std::swap(arrAs, arrCs);
        }

        // Copy final results back into CPU outData.

        array_view<float, 2> c(rows, cols, vC);
        std::for_each(tasks.crbegin(), tasks.crend(), [=, &arrAs, &c](const TaskData& t)
        {
            index<2> ind(t.writeOffset, shift);
            extent<2> ext(t.writeExt[0], t.writeExt[1] - shift * 2);
            array_view<float, 2> outData = c.section(ind, ext); 
            arrAs[t.id].section(ind, ext).copy_to(outData);
        });
    });
    std::wcout << " " << tasks.size() << " GPU matrix weighted average took                               " << time << " (ms)" << std::endl;
#ifdef _DEBUG
   PrintMatrix(vC.data(), rows, cols);
#endif
}

//--------------------------------------------------------------------------------------
//  Small example showing asynchronous copy pattern.
//--------------------------------------------------------------------------------------

void AsyncCopyExample()
{
    std::vector<float> resultData(100000, 0.0f);
    array<float, 1> resultArr(int(resultData.size()));

    //  Synchronous version, may be performance impact when using multi-GPU on Windows 7.

    // parallel_for_each calculates resultArr data...
    copy(resultArr, resultData.begin());

    //  Better version, considerably better performance on multi-GPU on Windows 7.

    // parallel_for_each calculates resultArr data...
    completion_future f = copy_async(resultArr, resultData.begin());
    resultArr.accelerator_view.wait();
    f.get();
}

//--------------------------------------------------------------------------------------
//  Master worker example for load balancing across multiple accelerators
//--------------------------------------------------------------------------------------

typedef std::pair<int, int> Task;
inline int GetStart(Task t) { return t.first; }
inline int GetEnd(Task t) { return t.first + t.second; }
inline int GetSize(Task t) { return t.second; }

void WorkStealingExample(const std::vector<accelerator>& accls, const int rows, const int cols, const int shift)
{
    critical_section critSec;               // Only required for correct wcout formatting.

#ifdef _DEBUG
    const size_t dataSize = 101000;     
    const size_t taskSize = dataSize / 20;
#else
    const size_t dataSize = 1000000;
    const size_t taskSize = 10000;
#endif
    std::vector<int> theData(dataSize, 1);

    //  Divide the data up into tasks

    concurrent_queue<Task> tasks;
    for (size_t i = 0; i < theData.size(); i += taskSize)
        tasks.push(Task(int(i), int(std::min(i + taskSize, theData.size()) - i)));
    std::wcout << std::endl << std::endl << "Queued " << tasks.unsafe_size() << " tasks" << std::endl;

    //  Start a task for each accelerator

    parallel_for(0u, unsigned(accls.size()), [=, &theData, &tasks, &critSec](const unsigned i)
    {
        int taskCount = 0;
        std::wcout << " Starting tasks on " << i << ": " << accls[i].description << std::endl;

        //  Each accelerator works on tasks on a shared queue

        Task t;
        while (tasks.try_pop(t))
        {
            array_view<int> work(extent<1>(GetSize(t)), theData.data() + GetStart(t));
            parallel_for_each(accls[i].default_view, extent<1>(GetSize(t)), 
                [=](index<1> idx) restrict(amp)
            {
                work[idx] += 1;
            });
            accls[i].default_view.wait();  // Wait in order to stop the synchronize from blocking the process
            work.synchronize();

            taskCount++;
#ifdef _DEBUG
            {
                critical_section::scoped_lock lock(critSec);
                std::wcout << "  Finished task " << GetStart(t) << " - " << GetEnd(t) << " on " << i << std::endl; 
            }
#endif
        }
        std::wcout << " Finished " << taskCount << " tasks on " << i << std::endl;
    });

#ifdef _DEBUG
    std::wcout << std::endl << "Results:" << std::endl;
    for (size_t i = 0; i < theData.size(); i += taskSize)
    {
        std::wcout << i << ": ";
        for (int j = 0; j < 10; ++j)
            std::wcout << "  " << theData[i] << ", ";
        std::wcout << std::endl;
    }
#endif
}

//--------------------------------------------------------------------------------------
//  Example showing the use of a completion_future.
//--------------------------------------------------------------------------------------

void CompletionFutureExample()
{
    accelerator accl = accelerator(accelerator::default_accelerator);
    const int size = !accl.is_emulated ? int(accl.dedicated_memory * 1024 / sizeof(float) * 0.5f) : 1024;

    std::vector<float> vA(size, 0.0f);
    array<float, 1> arrA(size);

    std::cout << "Data copy of " << size << " bytes starting." << std::endl;
    completion_future f = copy_async(vA.cbegin(), vA.cend(), arrA);
    f.then([=] ()
    { 
        std::cout << "  Finished asynchronous copy!" << std::endl; 
    });
    std::cout << "Do more work on this thread..." << std::endl;
    f.get();
    std::cout << "Data copy completed." << std::endl;
}

//--------------------------------------------------------------------------------------
//  Worker function to calculate the weighted average of the surrounding cells
//--------------------------------------------------------------------------------------

float WeightedAverage(index<2> idx, const array_view<const float, 2>& data, int shift) restrict(amp)
{
    if (idx[1] < shift || idx[1] >= data.extent[1] - shift)
        return 0.0f;
    float max = fast_math::sqrtf((float)(shift * shift * 2));
    float avg = 0.0;
    float n = 0.0f;
    for (int i = -shift; i <= shift; ++i)
        for (int j = -shift; j <= shift; ++j)
        {
            int row = idx[0] + i;
            int col = idx[1] + i;
            float scale = 1 - fast_math::sqrtf((float)((i * i) * (j * j))) / max;
            avg += data(row,col) * scale;
            n += 1.0f;
        }
    avg /= n;
    return avg;
}

//--------------------------------------------------------------------------------------
//  Print a subsection of a matrix. 
//  The top left 10 x 10 region or the whole matrix, whichever is smaller.
//--------------------------------------------------------------------------------------

void PrintMatrix(const float* const mat, int rows, int cols)
{
    for (int i = 0; i < std::min(20, rows); ++i)
    {
        std::wcout << "  ";
        for (int j = 0; j < std::min(20, cols); ++j)
            std::wcout << mat[i * cols + j] << " ";
        std::wcout << std::endl;
    }
}

void PrintMatrix(const std::vector<TaskData>& tasks, std::vector<array<float, 2>> mat, int rows, int cols)
{
    std::vector<float> c(rows * cols, 0.0f);
    std::for_each(tasks.cbegin(), tasks.cend(), [=, &mat, &c](const TaskData& t)
    {
       std::copy(stdext::checked_array_iterator<float*>(mat[t.id].data(), rows * cols), 
           stdext::checked_array_iterator<float*>(mat[t.id].data() + t.writeExt[0] * cols, rows * cols), 
           stdext::checked_array_iterator<float*>(&c[(t.startRow + t.writeOffset) * cols], rows * cols));
    });
    PrintMatrix(c.data(), rows, cols);
}
