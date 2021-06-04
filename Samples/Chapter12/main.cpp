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
#include <assert.h>
#include <amp_math.h>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <random>
#include <numeric>
#include <amp_graphics.h>

#include <d3d11.h>
#include <D3Dcommon.h>

#include "Timer.h"

using namespace concurrency;

void TransposeExample(int matrixSize);
void TransposeSimpleExample(int matrixSize);
void TransposePaddedExample(int matrixSize);
void TransposeTruncatedMarginThreadsExample(int matrixSize);
void TransPoseTruncatedSectionsExample(int matrixSize);

void FillExample();
void FunctorExample();
void AtomicExample();
void TdrExample();

//--------------------------------------------------------------------------------------
//  Print a subsection of a matrix. 
//  The top left 10 x 10 region or the whole matrix, whichever is smaller.
//--------------------------------------------------------------------------------------

template<typename T>
void PrintMatrix(const T* const data, int size)
{
#ifndef _DEBUG
    return;
#endif
    const int maxSize = 20;
    for (int i = 0; i < min(maxSize, size); ++i)
    {
        for (int j = 0; j < min(maxSize, size); ++j)
            std::wcout << data[i * size + j] << " ";
        std::wcout << std::endl;
    }
}

template<typename T>
void CheckMatrix(const T* const data, int size)
{
    for (int i = 0; i < size; ++i)
        for (int j = 0; j < size; ++j)
        {
            T expected = j * size + i;
            if (data[i * size + j] != expected)
            {
                std::wcout << "Error! [" << i << ", " << j << "] expected " 
                    << expected << " but found " << data[i * size + j] << std::endl;
                return;
            }
        }
}

#ifdef _DEBUG
static const int tileSize = 4;
#else
static const int tileSize = 16;
#endif

int main()
{
    std::wcout << std::fixed << std::setprecision(3);

    // Most of the samples do this check first but that would alter the FullTimingExample.
   
#ifndef _DEBUG
    accelerator defaultDevice;
    std::wcout << L"Using device : " << defaultDevice.get_description() << std::endl;
    if (defaultDevice == accelerator(accelerator::direct3d_ref))
        std::wcout << " WARNING!! No C++ AMP hardware accelerator detected, using the REF accelerator." << std::endl << 
            "To see better performance run on C++ AMP\ncapable hardware." << std::endl;
#endif

    // TODO_AMP: Reinstate this when the driver bug is fixed.
    //TdrExample();

#ifdef _DEBUG
    int size = tileSize * 3;
#else
    int size = 8800;
#endif

    TransposeSimpleExample(size);
    TransposeSimpleExample(size + tileSize);

    TransposeExample(size);
    TransposeExample(size + tileSize);

    TransposePaddedExample(size + 1);
    TransposePaddedExample(size + tileSize / 2);
    TransposePaddedExample(size + tileSize - 1);

    TransposeTruncatedMarginThreadsExample(size + 1);
    TransposeTruncatedMarginThreadsExample(size + tileSize / 2);
    TransposeTruncatedMarginThreadsExample(size + tileSize - 1);

    TransPoseTruncatedSectionsExample(size + 1);
    TransPoseTruncatedSectionsExample(size + tileSize / 2);
    TransPoseTruncatedSectionsExample(size + tileSize - 1);

    std::wcout << std::endl << std::endl;

    FillExample();

    FunctorExample();
    AtomicExample();
}

//--------------------------------------------------------------------------------------
//  Simple matrix transpose example. Included here for camparison timing.
//--------------------------------------------------------------------------------------

void TransposeSimpleExample(int matrixSize)
{ 
    if (matrixSize % tileSize != 0)
        throw std::exception("matrix is not a multiple of tile size.");

    std::vector<unsigned int> inData(matrixSize * matrixSize);
    std::vector<unsigned int> outData(matrixSize * matrixSize, 0u);
    std::iota(inData.begin(), inData.end(), 0u);

    array_view<const unsigned int, 2> inDataView(matrixSize, matrixSize, inData);
    array_view<unsigned int, 2> outDataView(matrixSize, matrixSize, outData);
    outDataView.discard_data();

    tiled_extent<tileSize, tileSize> computeDomain = inDataView.extent.tile<tileSize, tileSize>();
    accelerator_view view = accelerator(accelerator::default_accelerator).default_view;
    double elapsedTime = TimeFunc(view, [&]() 
    {
        parallel_for_each(computeDomain, [=](tiled_index<tileSize, tileSize> tidx) restrict(amp)
        {    
            outDataView[tidx.global] = inDataView[tidx.global[1]][tidx.global[0]];
        });
    });
    outDataView.synchronize();
    std::wcout << "Transpose simple exact size" << std::endl;
    std::wcout << "  Matrix size " << matrixSize << " x " << matrixSize << std::endl;
    std::wcout << "  Elapsed time " << elapsedTime << " ms" << std::endl;
    CheckMatrix(static_cast<const unsigned int* const>(outData.data()), matrixSize);
    PrintMatrix(static_cast<const unsigned int* const>(outData.data()), matrixSize);
    std::wcout << std::endl;
}

//--------------------------------------------------------------------------------------
//  Tiled matrix transpose example.
//--------------------------------------------------------------------------------------

void TransposeExample(int matrixSize)
{ 
    if (matrixSize % tileSize != 0)
        throw std::exception("matrix is not a multiple of tile size.");

    std::vector<unsigned int> inData(matrixSize * matrixSize);
    std::vector<unsigned int> outData(matrixSize * matrixSize, 0u);
    std::iota(inData.begin(), inData.end(), 0u);

    array_view<const unsigned int, 2> inDataView(matrixSize, matrixSize, inData);
    array_view<unsigned int, 2> outDataView(matrixSize, matrixSize, outData);
    outDataView.discard_data();

    tiled_extent<tileSize, tileSize> computeDomain = inDataView.extent.tile<tileSize, tileSize>();
    accelerator_view view = accelerator(accelerator::default_accelerator).default_view;
    double elapsedTime = TimeFunc(view, [&]() 
    {
        parallel_for_each(computeDomain, [=](tiled_index<tileSize, tileSize> tidx) restrict(amp)
        {     
            tile_static unsigned int localData[tileSize][tileSize];
            localData[tidx.local[1]][tidx.local[0]] = inDataView[tidx.global];

            tidx.barrier.wait();

            index<2> outIdx(index<2>(tidx.tile_origin[1], tidx.tile_origin[0]) + tidx.local);
            outDataView[outIdx] = localData[tidx.local[0]][tidx.local[1]];
        });
    });
    outDataView.synchronize();
    std::wcout << "Transpose exact size" << std::endl;
    std::wcout << "  Matrix size " << matrixSize << " x " << matrixSize << std::endl;
    std::wcout << "  Elapsed time " << elapsedTime << " ms" << std::endl;
    CheckMatrix(static_cast<const unsigned int* const>(outData.data()), matrixSize);
    PrintMatrix(static_cast<const unsigned int* const>(outData.data()), matrixSize);
    std::wcout << std::endl;
}

//--------------------------------------------------------------------------------------
//  Tiled and padded matrix transpose example.
//--------------------------------------------------------------------------------------

template <typename T, unsigned int Rank>
T PaddedRead(const array_view<const T, Rank>& A, const index<Rank>& idx) restrict(cpu, amp)
{
    return A.extent.contains(idx) ? A[idx] : T();
}

template <typename T, unsigned int Rank>
void PaddedWrite(const array_view<T, Rank>& A, const index<Rank>& idx, const T& val) restrict(cpu, amp)
{
    if (A.extent.contains(idx))
        A[idx] = val;
}

void TransposePaddedExample(int matrixSize)
{
    std::vector<unsigned int> inData(matrixSize * matrixSize);
    std::vector<unsigned int> outData(matrixSize * matrixSize, 0u);
    std::iota(inData.begin(), inData.end(), 0u);

    array_view<const unsigned int, 2> inDataView(matrixSize, matrixSize, inData);
    array_view<unsigned int, 2> outDataView(matrixSize, matrixSize, outData);
    outDataView.discard_data();

    tiled_extent<tileSize, tileSize> computeDomain = inDataView.extent.tile<tileSize, tileSize>();
    computeDomain = computeDomain.pad();
    accelerator_view view = accelerator(accelerator::default_accelerator).default_view;
    double elapsedTime = TimeFunc(view, [&]() 
    {
        parallel_for_each(view, computeDomain, 
            [=](tiled_index<tileSize, tileSize> tidx) restrict(amp)
        {     
            tile_static unsigned int localData[tileSize][tileSize];
            localData[tidx.local[1]][tidx.local[0]] = PaddedRead(inDataView, tidx.global);

            tidx.barrier.wait();

            index<2> outIdx(index<2>(tidx.tile_origin[1], tidx.tile_origin[0]) + tidx.local);
            PaddedWrite(outDataView, outIdx, localData[tidx.local[0]][tidx.local[1]]);
        });
    });
    
    outDataView.synchronize();
    std::wcout << "Transpose padded" << std::endl;
    std::wcout << "  Matrix size " << matrixSize << " x " << matrixSize << ", padded size " 
        << computeDomain[0] << " x " << computeDomain[1] << std::endl;
    std::wcout << "  Elapsed time " << elapsedTime << " ms" << std::endl;
    CheckMatrix(static_cast<const unsigned int* const>(outData.data()), matrixSize);
    PrintMatrix(static_cast<const unsigned int* const>(outData.data()), matrixSize);
    std::wcout << std::endl;
}

//--------------------------------------------------------------------------------------
//  Tiled and truncated matrix transpose example.
//--------------------------------------------------------------------------------------

void TransposeTruncatedMarginThreadsExample(int matrixSize)
{
    std::vector<unsigned int> inData(matrixSize * matrixSize);
    std::vector<unsigned int> outData(matrixSize * matrixSize, 0);
    std::iota(inData.begin(), inData.end(), 0u);

    array_view<const unsigned int, 2> inDataView(matrixSize, matrixSize, inData);
    array_view<unsigned int, 2> outDataView(matrixSize, matrixSize, outData);
    outDataView.discard_data();

    tiled_extent<tileSize, tileSize> computeDomain = inDataView.extent.tile<tileSize, tileSize>();
    computeDomain = computeDomain.truncate();

    const int rightMargin = inDataView.extent[1] - computeDomain[1];
    const int bottomMargin = inDataView.extent[0] - computeDomain[0];
    accelerator_view view = accelerator(accelerator::default_accelerator).default_view;
    double elapsedTime = TimeFunc(view, [&]() 
    {
        parallel_for_each(view, computeDomain, [=](tiled_index<tileSize, tileSize> tidx) restrict(amp)
        {     
            tile_static unsigned int localData[tileSize][tileSize];
            localData[tidx.local[1]][tidx.local[0]] = inDataView[tidx.global];
            tidx.barrier.wait();
            index<2> outIdx(index<2>(tidx.tile_origin[1], tidx.tile_origin[0]) + tidx.local);
            outDataView[outIdx] = localData[tidx.local[0]][tidx.local[1]];

            // Handle truncated elements using threads in the margins.

            bool isRightMost = tidx.global[1] >= computeDomain[1] - rightMargin; 
            bool isBottomMost = tidx.global[0] >= computeDomain[0] - bottomMargin; 

            // Exit branching as quickly as possible as the majority of threads will not meet the boundary criteria
            if (isRightMost | isBottomMost)
            {
                int idx0, idx1;
                if (isRightMost)
                {
                    idx0 = tidx.global[0]; 
                    idx1 = tidx.global[1] + rightMargin;
                    outDataView(idx1, idx0) = inDataView(idx0, idx1);    
                }
                if (isBottomMost)
                {
                    idx1 = tidx.global[1];
                    idx0 = tidx.global[0] + bottomMargin;
                    outDataView(idx1, idx0) = inDataView(idx0, idx1);
                }
                if (isRightMost & isBottomMost) 
                { 
                    idx0 = tidx.global[0] + bottomMargin;
                    idx1 = tidx.global[1] + rightMargin;
                    outDataView(idx1, idx0) = inDataView(idx0, idx1);
                }
            }
        });
    });

    outDataView.synchronize();
    std::wcout << "Transpose truncated, margin threads handle truncated elements" << std::endl;
    std::wcout << "  Matrix size " << matrixSize << " x " << matrixSize << ", truncated size " 
        << computeDomain[0] << " x " << computeDomain[1] << std::endl;
    std::wcout << "  Elapsed time " << elapsedTime << " ms" << std::endl;
    CheckMatrix(static_cast<const unsigned int* const>(outData.data()), matrixSize);
    PrintMatrix(static_cast<const unsigned int* const>(outData.data()), matrixSize);
    std::wcout << std::endl;
}

void SimpleTranspose(const array_view<const unsigned int, 2>& inDataView, 
                     const array_view<unsigned int, 2>& outDataView)
{
    outDataView.discard_data();
    parallel_for_each(outDataView.extent, [=] (index<2> idx) restrict(amp) 
    {
        outDataView(idx[0], idx[1]) = inDataView(idx[1], idx[0]);
    });
}

template <int TileSize>
void TiledTranspose(const array_view<const unsigned int, 2>& inDataView, 
                    const array_view<unsigned int, 2>& outDataView)
{
    outDataView.discard_data();
    parallel_for_each(outDataView.extent.tile<TileSize, TileSize>(), 
        [=] (tiled_index<TileSize, TileSize> tidx) restrict(amp) 
    {
        tile_static unsigned int localData[tileSize][tileSize];
        localData[tidx.local[1]][tidx.local[0]] = inDataView[tidx.global];
        tidx.barrier.wait();
        index<2> outIdx(index<2>(tidx.tile_origin[1], tidx.tile_origin[0]) + tidx.local);
        outDataView[outIdx] = localData[tidx.local[0]][tidx.local[1]];
    });
}

void TransPoseTruncatedSectionsExample(int matrixSize)
{
    std::vector<unsigned int> inData(matrixSize * matrixSize);
    std::vector<unsigned int> outData(matrixSize * matrixSize, 0);
    std::iota(inData.begin(), inData.end(), 0u);

    array_view<const unsigned int, 2> inDataView(matrixSize, matrixSize, inData);
    array_view<unsigned int, 2> outDataView(matrixSize, matrixSize, outData);
    outDataView.discard_data();

    tiled_extent<tileSize, tileSize> computeDomain = inDataView.extent.tile<tileSize, tileSize>();
    tiled_extent<tileSize, tileSize> truncatedDomain = computeDomain.truncate();
    bool isBottomTruncated = truncatedDomain[0] < computeDomain[0];
    bool isRightTruncated = truncatedDomain[1] < computeDomain[1];
    accelerator_view view = accelerator(accelerator::default_accelerator).default_view;
    double elapsedTime = TimeFunc(view, [&]() 
    {
        array_view<const unsigned int, 2> fromData  = inDataView.section(index<2>(0, 0), truncatedDomain);
        array_view<unsigned int, 2> toData = outDataView.section(index<2>(0, 0), extent<2>(truncatedDomain[1], truncatedDomain[0]));
        TiledTranspose<tileSize>(fromData, toData);

        if (isBottomTruncated)                  // Area B.
        {
            index<2> offset(truncatedDomain[0], 0);
            extent<2> ext(inDataView.extent[0] - truncatedDomain[0], truncatedDomain[1]);
            fromData  = inDataView.section(offset, ext);
            toData = outDataView.section(index<2>(offset[1], offset[0]), extent<2>(ext[1], ext[0]));
            SimpleTranspose(fromData, toData);
        }
        if (isRightTruncated)                   // Area A & C.
        {
            index<2> offset(0, truncatedDomain[1]);
            fromData  = inDataView.section(offset);
            toData = outDataView.section(index<2>(offset[1], offset[0]));
            SimpleTranspose(fromData, toData);
        }

        outDataView.synchronize();
    });

    std::wcout << "Transpose truncated, using sections handle each area" << std::endl;
    std::wcout << "  Matrix size " << matrixSize << " x " << matrixSize << ", truncated size " 
        << computeDomain[0] << " x " << computeDomain[1] << std::endl;
    std::wcout << "  Elapsed time " << elapsedTime << " ms" << std::endl;
    CheckMatrix(static_cast<const unsigned int* const>(outData.data()), matrixSize);
    PrintMatrix(static_cast<const unsigned int* const>(outData.data()), matrixSize);
    std::wcout << std::endl;
}

//--------------------------------------------------------------------------------------
//  Fill example.
//--------------------------------------------------------------------------------------

template<typename T, int Rank>
void Fill(array<T, Rank>& arr, T value) 
{ 
    parallel_for_each(arr.extent, [&arr, value](index<Rank> idx) restrict(amp)
    {
        arr[idx] = value; 
    }); 
}

void FillExample()
{
    array<float, 2> theData(100, 100);
    Fill(theData, 1.5f);
}

//--------------------------------------------------------------------------------------
//  GPU simple matrix multiply functor
//--------------------------------------------------------------------------------------

class Multiply
{
private:
    array_view<const float, 2> m_mA; 
    array_view<const float, 2> m_mB; 
    array_view<float, 2> m_mC;
    int m_W;

public:
    Multiply(const array_view<const float, 2>& a,
             const array_view<const float, 2>& b,
             const array_view<float, 2>& c,
             int w) : m_mA(a), m_mB(b), m_mC(c), m_W(w)
    {}

    void operator()(index<2> idx) const restrict(amp)
    {
        int row = idx[0]; int col = idx[1];
        float sum = 0.0f;
        for(int i = 0; i < m_W; i++)
            sum += m_mA(row, i) * m_mB(i, col);
        m_mC[idx] = sum;
    }
};

void FunctorExample()
{
    const int M = 64;
    const int N = 512;
    const int W = 256;

    std::vector<float> vA(M * W);
    std::vector<float> vB(W * N);
    std::vector<float> vC(M * N);

    extent<2> eA(M, W), eB(W, N), eC(M, N);
    array_view<float, 2> mA(eA, vA); 
    array_view<float, 2> mB(eB, vB); 
    array_view<float, 2> mC(eC, vC);
    mC.discard_data();

    parallel_for_each(extent<2>(eC), Multiply(mA, mB, mC, W));

    mC.synchronize();
}

//--------------------------------------------------------------------------------------
//  Atomic example.
//--------------------------------------------------------------------------------------

void AtomicExample()
{
    std::random_device rd; 
    std::default_random_engine engine(rd()); 
    std::uniform_real_distribution<float> randDist(0.0f, 1.0f);
    std::vector<float> theData(100000);
    std::generate(theData.begin(), theData.end(), [=, &engine, &randDist]() { return randDist(engine); });
    array_view<float, 1> theDataView(int(theData.size()), theData);

    int exceptionalOccurrences = 0;
    array_view<int> count(1, &exceptionalOccurrences);
    parallel_for_each(theDataView.extent, [=] (index<1> idx) restrict(amp)
    {
        if (theDataView[idx] >= 0.9999f)  // Exceptional occurence.
        {
            atomic_fetch_inc(&count(0));
        }
        theDataView[idx] = fast_math::sqrt(theDataView[idx]);
    });
    count.synchronize();
    std::wcout << "Calculating values for " << theData.size() << " elements " << std::endl;
    std::wcout << "A total of " << exceptionalOccurrences << " exceptional occurrences were detected." 
        << std::endl << std::endl;
}

//--------------------------------------------------------------------------------------
//  TDR example.
//--------------------------------------------------------------------------------------

//  Only available on Windows 8.

void DisableTdrExample()
{
#if defined(D3D_FEATURE_LEVEL_11_1)
    IDXGIAdapter* pAdapter = nullptr; // Use default adapter
    unsigned int createDeviceFlags = D3D11_CREATE_DEVICE_DISABLE_GPU_TIMEOUT;
    ID3D11Device *pDevice = nullptr;
    ID3D11DeviceContext *pContext = nullptr;
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(pAdapter,
        D3D_DRIVER_TYPE_UNKNOWN,
        NULL,
        createDeviceFlags,
        NULL,
        0,
        D3D11_SDK_VERSION,
        &pDevice,
        &featureLevel,
        &pContext);
    if (FAILED(hr) ||
        ((featureLevel != D3D_FEATURE_LEVEL_11_1) &&
         (featureLevel != D3D_FEATURE_LEVEL_11_0)))
    {
        std::wcerr << "Failed to create Direct3D 11 device" << std::endl;
        return;
    }
    accelerator_view noTimeoutAcclView =
    concurrency::direct3d::create_accelerator_view(pDevice);
#endif
}

void Compute(std::vector<float>& inData, std::vector<float>& outData, int start, 
             accelerator& device, queuing_mode mode = queuing_mode::queuing_mode_automatic)
{
    array_view<const float, 1> inDataView(int(inData.size()), inData);
    array_view<float, 1> outDataView(int(outData.size()), outData);

    accelerator_view view = device.create_view(mode);
    parallel_for_each(view, outDataView.extent, [=](index<1> idx) restrict(amp)
    {
        int i = start;
        while (i < 1024)
        {
            outDataView[idx] = inDataView[idx];
            i *= 2;
            i = i % 2048;
        }
    }); 
}

void TdrExample()
{
    std::vector<float> inData(10000);
    std::vector<float> outData(10000, 0.0f);
    accelerator accel = accelerator();
    try
    {
        Compute(inData, outData, -1, accel);
    }
    catch (accelerator_view_removed& ex)
    {
        std::wcout << "TDR exception: " << ex.what(); 
        std::wcout << "  Error code:" << std::hex << ex.get_error_code(); 
        std::wcout << "  Reason:" << std::hex << ex.get_view_removed_reason();

        std::wcout << "Retrying..." << std::endl;
        try
        {
            Compute(inData, outData, 1, accel, queuing_mode::queuing_mode_immediate);
        }
        catch (accelerator_view_removed& ex)
        {
            std::wcout << "TDR exception: " << ex.what(); 
            std::wcout << "  Error code:" << std::hex << ex.get_error_code(); 
            std::wcout << "  Reason:" << std::hex << ex.get_view_removed_reason();
            std::wcout << "FAILED." << std::endl;
        }
    }
}
