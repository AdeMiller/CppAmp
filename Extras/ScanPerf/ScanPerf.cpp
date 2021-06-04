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

#include "..\Scan\ScanSimple.h"
#include "..\Scan\ScanTiled.h"
#include "..\Scan\ScanTiledOptimized.h"

using namespace Extras;

// TODO: Can I get rid of all this?
class IScan
{
public:
    virtual void Scan(array_view<int, 1>(in), array_view<int, 1>(out)) const = 0;
};

class DummyScan : public IScan
{
public:
    void Scan(array_view<int, 1>(in), array_view<int, 1>(out)) const { }
};

class SimpleScan : public IScan
{
public:
    void Scan(array_view<int, 1>(in), array_view<int, 1>(out)) const
    {
        InclusiveScanSimple(array_view<int, 1>(in), array_view<int, 1>(out));
    }
};

template <int TileSize>
class TiledScan : public IScan
{
public:
    void Scan(array_view<int, 1>(in), array_view<int, 1>(out)) const
    {
        InclusiveScanTiled<TileSize>(array_view<int, 1>(in), array_view<int, 1>(out));
    }
};

template <int TileSize>
class TiledOptScan : public IScan
{
public:
    void Scan(array_view<int, 1>(in), array_view<int, 1>(out)) const
    {
        InclusiveScanOptimized<TileSize>(array_view<int, 1>(in), array_view<int, 1>(out));
    }
};

typedef std::pair<std::shared_ptr<IScan>, std::wstring> ScanDescription;

inline bool ValidateSizes(unsigned tileSize, unsigned elementCount);

int _tmain(int argc, _TCHAR* argv[])
{
#ifdef _DEBUG
    const size_t elementCount = 1024;
    const int tileSize = 64;
#else
    const size_t elementCount = 2 * 1024 * 1024;
    const int tileSize = 64;
#endif

    // Make sure that elements can be split into tiles so the number of
    // tiles less than 65536. 
    static_assert((elementCount / tileSize < 65536), 
        "Workload is too large or tiles are too small. This will cause runtime errors.");
    static_assert((elementCount != 0), "Number of elements cannot be zero.");
    static_assert((elementCount <= UINT_MAX), "Number of elements is too large.");

    std::wcout << "Running kernels with " << elementCount << " elements, " 
        << elementCount * sizeof(int) / 1024 << " KB of data ..."  << std::endl;    
    std::wcout << "Tile size:     " << tileSize << std::endl;

    if (!ValidateSizes(2, elementCount))
        std::wcout << "Tile size is not factor of element count. This will cause runtime errors." 
        << std::endl;

    accelerator defaultDevice;
    std::wcout << L"Using device : " << defaultDevice.get_description() << std::endl;
    if (defaultDevice == accelerator(accelerator::direct3d_ref))
        std::wcout << "WARNING!! No C++ AMP hardware accelerator detected, using the REF accelerator." << std::endl << 
        "To see better performance run on C++ AMP capable hardware." << std::endl;

    std::vector<int> input(elementCount, 1);
    std::vector<int> result(input.size());
    std::vector<int> expected(input.size());
    std::iota(begin(expected), end(expected), 1);

    std::array<ScanDescription, 4> scans = {
        ScanDescription(std::make_shared<DummyScan>(),                      L"Overhead"),
        ScanDescription(std::make_shared<SimpleScan>(),                     L"Simple"),
        ScanDescription(std::make_shared<TiledScan<tileSize>>(),            L"Tiled"),
        ScanDescription(std::make_shared<TiledOptScan<tileSize>>(),         L"Tiled Optimized") };

    std::wcout << std::endl << "                                                           Total : Calc" << std::endl << std::endl;

    accelerator_view view = accelerator(accelerator::default_accelerator).default_view;
    for (ScanDescription s : scans)
    {
        IScan* scanImpl = s.first.get();
        std::wstring scanName = s.second;

        std::fill(begin(input), end(input), 1);
        std::fill(begin(result), end(result), 0);

        double computeTime = 0.0, totalTime = 0.0;
        totalTime = JitAndTimeFunc(view, [&]()
        {
            concurrency::array<int, 1> in(input.size());
            concurrency::array<int, 1> out(input.size());
            copy(begin(input), end(input), in);

            computeTime = TimeFunc(view, [&]()
            {
                scanImpl->Scan(array_view<int, 1>(in), array_view<int, 1>(out));
            });
            copy(out, begin(result));
        });
        if (!std::equal(begin(result), end(result), begin(expected)) && (scanName.compare(L"Overhead") != 0))
            std::wcout << "FAILED:  " << scanName;
        else     
            std::wcout << "SUCCESS: " << scanName;

        std::wcout.width(std::max(0U, 55 - scanName.length()));
        std::wcout << std::right << std::fixed << std::setprecision(2) << totalTime << " : " << computeTime << " (ms)"  << std::endl;        
    }
    std::wcout << std::endl;
    return 0;
}

//----------------------------------------------------------------------------
//  Ensure that the reduction can repeatedly divide the elements by tile size.
//----------------------------------------------------------------------------

inline bool ValidateSizes(unsigned tileSize, unsigned elementCount)
{
    while ((elementCount % tileSize) == 0) 
    {
        elementCount /= tileSize;
    }
    return elementCount < tileSize;
}
