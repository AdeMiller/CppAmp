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

//  In order to remove code markers from the project comment out the definition below.
//
//#define MARKERS

#include <amp.h>
#include <iostream>
#include <iomanip>
#include <numeric> 
#include <algorithm>
#include <assert.h>

#include "Timer.h"
#include "IReduce.h"
#include "DummyReduction.h"
#include "SequentialReduction.h"
#include "ParallelReduction.h"
#include "SimpleReduction.h"
#include "SimpleArrayViewReduction.h"
#include "SimpleOptimizedReduction.h"
#include "TiledReduction.h"
#include "TiledSharedMemoryReduction.h"
#include "TiledMinimizedDivergenceReduction.h"
#include "TiledMinimizedDivergenceAndConflictsReduction.h"
#include "TiledMinimizedDivergenceConflictsAndStallingReduction.h"
#include "TiledMinimizedDivergenceConflictsAndStallingUnrolledReduction.h"
#include "CascadingReduction.h"
#include "CascadingUnrolledReduction.h"

#ifdef MARKERS
#include <cvmarkersobj.h>
using namespace concurrency::diagnostic;
#endif

using namespace concurrency;

#ifdef MARKERS
marker_series g_markerSeries(L"Reducer Application");
#endif

typedef std::pair<std::shared_ptr<IReduce>, std::wstring> ReducerDescription;

inline bool validateSizes(unsigned tileSize, unsigned elementCount);

void main()
{
    //  Uncomment this to use the WARP accelerator even if a GPU is present.
    //accelerator::set_default(accelerator::direct3d_warp);

    const size_t elementCount = 16 * 1024 * 1024; 
    const int tileSize = 512;
    const int tileCount = 128;                     // Used in cascading reductions

    // Make sure that elements can be split into tiles so the number of
    // tiles in any dimension is less than 65536. 
    static_assert((elementCount / tileSize < 65536), 
        "Workload is too large or tiles are too small. This will cause runtime errors.");
    static_assert((elementCount % (tileSize * tileCount) == 0), 
        "Tile size and count are not matched to element count. This will cause runtime errors.");
    static_assert((elementCount != 0), "Number of elements cannot be zero.");
    static_assert((elementCount <= UINT_MAX), "Number of elements is too large.");

    std::wcout << "Running kernels with " << elementCount << " elements, " 
        << elementCount * sizeof(int) / 1024 << " KB of data ..."  << std::endl;    
    std::wcout << "Tile size:     " << tileSize << std::endl;
    std::wcout << "Tile count:    " << tileCount << std::endl;

    if (!validateSizes(tileSize, elementCount))
        std::wcout << "Tile size is not factor of element count. This will cause runtime errors." 
        << std::endl; 

    accelerator defaultDevice;
    std::wcout << L"Using device : " << defaultDevice.get_description() << std::endl;
    if (defaultDevice == accelerator(accelerator::direct3d_ref))
        std::wcout << "WARNING!! No C++ AMP hardware accelerator detected, using the REF accelerator." << std::endl << 
            "To see better performance run on C++ AMP" << std::endl << "capable hardware." << std::endl;

    std::vector<int> source(elementCount);

    // Data size is smaller to avoid overflow or underflow
    int i = 0;
    std::generate(source.begin(), source.end(), [&i]() { return (i++ & 0xf); });

    // The data is generated in a pattern and its sum can be computed using the following function
    const int expectedResult = int((elementCount / 16) * ((15 * 16) / 2));

    std::vector<ReducerDescription> reducers;
    reducers.reserve(14);
    reducers.push_back(ReducerDescription(std::make_shared<DummyReduction>(),                                                           L"Overhead"));
    reducers.push_back(ReducerDescription(std::make_shared<SequentialReduction>(),                                                      L"CPU sequential"));
    reducers.push_back(ReducerDescription(std::make_shared<ParallelReduction>(),                                                        L"CPU parallel"));
    reducers.push_back(ReducerDescription(std::make_shared<SimpleReduction>(),                                                          L"C++ AMP simple model"));
    reducers.push_back(ReducerDescription(std::make_shared<SimpleArrayViewReduction>(),                                                 L"C++ AMP simple model using array_view"));
    reducers.push_back(ReducerDescription(std::make_shared<SimpleOptimizedReduction>(),                                                 L"C++ AMP simple model optimized"));
    reducers.push_back(ReducerDescription(std::make_shared<TiledReduction<tileSize>>(),                                                 L"C++ AMP tiled model")); 
    reducers.push_back(ReducerDescription(std::make_shared<TiledSharedMemoryReduction<tileSize>>(),                                     L"C++ AMP tiled model & shared memory"));
    reducers.push_back(ReducerDescription(std::make_shared<TiledMinimizedDivergenceReduction<tileSize>>(),                              L"C++ AMP tiled model & minimized divergence"));
    reducers.push_back(ReducerDescription(std::make_shared<TiledMinimizedDivergenceAndConflictsReduction<tileSize>>(),                  L"C++ AMP tiled model & no bank conflicts"));
    reducers.push_back(ReducerDescription(std::make_shared<TiledMinimizedDivergenceConflictsAndStallingReduction<tileSize>>(),          L"C++ AMP tiled model & reduced stalled threads"));
    reducers.push_back(ReducerDescription(std::make_shared<TiledMinimizedDivergenceConflictsAndStallingUnrolledReduction<tileSize>>(),  L"C++ AMP tiled model & unrolling"));
    reducers.push_back(ReducerDescription(std::make_shared<CascadingReduction<tileSize, tileCount>>(),                                  L"C++ AMP cascading reduction"));
    reducers.push_back(ReducerDescription(std::make_shared<CascadingUnrolledReduction<tileSize, tileCount>>(),                          L"C++ AMP cascading reduction & unrolling"));

    std::wcout << std::endl << "                                                           Total : Calc" << std::endl << std::endl;

    accelerator_view view = accelerator(accelerator::default_accelerator).default_view;
    for (size_t  i = 0; i < reducers.size(); ++i)
    {
        int result = 0;
        IReduce* reducerImpl = reducers[i].first.get();
        std::wstring reducerName = reducers[i].second;
        
        double computeTime = 0.0, totalTime = 0.0;
        totalTime = JitAndTimeFunc(view, [&]() 
        {
#ifdef MARKERS
            span funcSpan(g_markerSeries, reducerName.c_str());
#endif
            result = reducerImpl->Reduce(view, source, computeTime);
        });

        if (result == -1)
        {
            std::wcout << "SKIPPED: " << reducerName << " - Accelerator not supported." << std::endl;
            continue;
        }
        if (expectedResult != result)
        {
            std::wcout << "FAILED:  " << reducerName << " expected " << expectedResult << std::endl 
                << "         but found " << result << std::endl;
            continue;
        }
        
        std::wcout << "SUCCESS: " << reducerName;
        std::wcout.width(max(0, 55 - reducerName.length()));
        std::wcout << std::right << std::fixed << std::setprecision(2) << totalTime << " : " << computeTime << " (ms)" << std::endl;        
    }
    std::wcout << std::endl;
}

//----------------------------------------------------------------------------
//  Ensure that the reduction can repeatedly divide the elements by tile size.
//----------------------------------------------------------------------------
inline bool validateSizes(unsigned tileSize, unsigned elementCount)
{
    while ((elementCount % tileSize) == 0) 
    {
        elementCount /= tileSize;
    }
    return elementCount < tileSize;
}