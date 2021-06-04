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

// Demonstrates how to use C++ AMP to do matrix multiply.

#include <iostream>
#include <algorithm>
#include <random>
#include <amp.h>

#include "Timer.h"

using namespace concurrency;

//--------------------------------------------------------------------------------------
//  GPU simple matrix multiply
//--------------------------------------------------------------------------------------

void MatrixMultiply(std::vector<float>& vC, 
    const std::vector<float>& vA, 
    const std::vector<float>& vB, int M, int N, int W)
{
    array_view<const float,2> a(M, W, vA);
    array_view<const float,2> b(W, N, vB);
    array_view<float,2> c(M, N, vC);
    c.discard_data(); 
    parallel_for_each(c.extent, [=](index<2> idx) restrict(amp) 
    {
        int row = idx[0]; 
        int col = idx[1];
        float sum = 0.0f;
        for(int i = 0; i < W; i++)
            sum += a(row, i) * b(i, col);
        c[idx] = sum;
    });
    c.synchronize();
}

//--------------------------------------------------------------------------------------
//  GPU tiled matrix multiply
//--------------------------------------------------------------------------------------

static const int TileSize = 16;

void MatrixMultiplyTiled(std::vector<float>& vC,
    const std::vector<float>& vA,
    const std::vector<float>& vB,
    int M, int N, int W)
{
    array_view<const float,2> a(M, W, vA);
    array_view<const float,2> b(W, N, vB);
    array_view<float,2> c(M, N, vC);
    c.discard_data();
    parallel_for_each(c.extent.tile<TileSize,TileSize>(),
        [=](tiled_index<TileSize, TileSize> tidx) restrict(amp)
    {
        int row = tidx.global[0];
        int col = tidx.global[1];
        float sum = 0.0f;
        for(int i = 0; i < W; i++)
            sum += a(row, i) * b(i, col);
        c[tidx] = sum;
    });
    c.synchronize();
}

void MatrixMultiplyTiledWithTileStatic(std::vector<float>& vC, 
    const std::vector<float>& vA, 
    const std::vector<float>& vB, 
    int M, int N, int W)
{
    array_view<const float,2> a(M, W, vA);
    array_view<const float,2> b(W, N, vB);
    array_view<float,2> c(M, N, vC);
    c.discard_data();

    parallel_for_each(c.extent.tile<TileSize, TileSize>(),
        [=] (tiled_index<TileSize, TileSize> tidx) restrict(amp) 
    {
        int row = tidx.local[0]; 
        int col = tidx.local[1];
        float sum = 0.0f;
        for (int i = 0; i < W; i += TileSize) 
        {
            tile_static float sA[TileSize][TileSize];
            tile_static float sB[TileSize][TileSize];
            sA[row][col] = a(tidx.global[0], col + i);
            sB[row][col] = b(row + i, tidx.global[1]);

            tidx.barrier.wait();

            for (int k = 0; k < TileSize; k++)
                sum += sA[row][k] * sB[k][col];

            tidx.barrier.wait();
        }
        c[tidx.global] = sum;
    });
    c.synchronize();
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

int main()
{
    const int M = 64;
    const int N = 512;
    const int W = 256;

    accelerator defaultDevice(accelerator::default_accelerator);
    accelerator_view defaultView = defaultDevice.default_view;

    std::wcout << L" Using device : " << defaultDevice.get_description() << std::endl;
#ifndef _DEBUG
    if (defaultDevice == accelerator(accelerator::direct3d_ref))
        std::wcout << " WARNING!! No C++ AMP hardware accelerator detected, using the REF accelerator." << std::endl << 
            "To see better performance run on C++ AMP\ncapable hardware." << std::endl;
#endif
    std::vector<float> vA(M * W);
    std::vector<float> vB(W * N);
    std::vector<float> vC(M * N);
    std::vector<float> vRef(M * N);

    std::random_device rd; 
    std::default_random_engine engine(rd()); 
    std::uniform_real_distribution<float> rand(0.0f, 1.0f);

    std::generate(vA.begin(), vA.end(), [&rand, &engine](){ return rand(engine); });
    std::generate(vB.begin(), vB.end(), [&rand, &engine](){ return rand(engine); });

    //--------------------------------------------------------------------------------------
    //  CPU matrix multiply
    //--------------------------------------------------------------------------------------

    double elapsedTime = TimeFunc([&]()
    {
        for(int row = 0; row < M; ++row)
        {
            for(int col = 0; col < N; ++col)
            {
                float result = 0.0f;
                for(int i = 0; i < W; ++i)
                {
                    int idxA = row * W + i;
                    int idxB = i * N + col;
                    result += vA[idxA] * vB[idxB];
                }
                vRef[row * N + col] = result;
            }
        }
    });

    std::wcout << "CPU exec time: " << elapsedTime << " (ms)" << std::endl;

    //--------------------------------------------------------------------------------------
    //  GPU simple matrix multiply
    //--------------------------------------------------------------------------------------

    elapsedTime = TimeFunc([&]()
    {
        MatrixMultiply(vC, vA, vB, M, N, W);
    });

    std::wcout << std::endl << "GPU exec time (non tiled) including copy-in/out: " << elapsedTime << " (ms)" << std::endl;

    // Compare GPU non tiled and CPU resulTileSize

    auto firstMismatch = std::mismatch(vC.cbegin(), vC.cend(), vRef.cbegin(), [](float c, float r) { return (fabs(c - r) < 0.01); });
    if (firstMismatch.first != vC.end())
    {
        size_t i = std::distance(vC.cbegin(), firstMismatch.first);
        std::wcout << "vC[" << i << "] = " << *firstMismatch.first << ", vRef[" << i << "] = " << *firstMismatch.second << std::endl;
    }
    std::wcout << " non tiled " << ((firstMismatch.first == vC.end()) ? "PASSED" : "FAILED") << std::endl;

    //--------------------------------------------------------------------------------------
    //  GPU tiled matrix multiply
    //--------------------------------------------------------------------------------------

    static_assert((M % TileSize == 0) && (W % TileSize == 0) && (N % TileSize == 0), "Matrix dimensions must be a multiple of tile size.");

    elapsedTime = TimeFunc([&]()
    {
        MatrixMultiplyTiled(vC, vA, vB, M, N, W);
    });

    std::wcout << std::endl << "GPU exec time (tiled - tile size is " << TileSize << ") " << std::endl << " including copy-in/out: " <<
        elapsedTime << " (ms)" << std::endl;

    // Compare tiled GPU and CPU results

    firstMismatch = std::mismatch(vC.cbegin(), vC.cend(), vRef.cbegin(), [](float c, float r) { return (fabs(c - r) < 0.01); });
    if (firstMismatch.first != vC.end())
    {
        size_t i = std::distance(vC.cbegin(), firstMismatch.first);
        std::wcout << "vC[" << i << "] = " << *firstMismatch.first << ", vRef[" << i << "] = " << *firstMismatch.second << std::endl;
    }
    std::wcout << " tiled " << ((firstMismatch.first == vC.end()) ? "PASSED" : "FAILED") << std::endl;

    //--------------------------------------------------------------------------------------
    //  GPU tiled matrix multiply with tile static memory
    //--------------------------------------------------------------------------------------

    static_assert((M % TileSize == 0) && (W % TileSize == 0) && (N % TileSize == 0), "Matrix dimensions must be a multiple of tile size.");

    elapsedTime = TimeFunc([&]()
    {
        MatrixMultiplyTiledWithTileStatic(vC, vA, vB, M, N, W);
    });

    std::wcout << std::endl << "GPU exec time (tiled - tile size is " << TileSize << ") using tile_static memory " << std::endl << " including copy-in/out: " <<
        elapsedTime << " (ms)" << std::endl;

    // Compare tiled GPU and CPU results

    firstMismatch = std::mismatch(vC.cbegin(), vC.cend(), vRef.cbegin(), [](float c, float r) { return (fabs(c - r) < 0.01); });
    if (firstMismatch.first != vC.end())
    {
        size_t i = std::distance(vC.cbegin(), firstMismatch.first);
        std::wcout << "vC[" << i << "] = " << *firstMismatch.first << ", vRef[" << i << "] = " << *firstMismatch.second << std::endl;
    }
    std::wcout << " tiled with tile_static " << ((firstMismatch.first == vC.end()) ? "PASSED" : "FAILED") << std::endl;

    //--------------------------------------------------------------------------------------
    //  GPU simple matrix multiply with functors
    //--------------------------------------------------------------------------------------

    elapsedTime = TimeFunc([&]()
    {
        extent<2> eA(M, W), eB(W, N), eC(M, N);
        array_view<float, 2> mA(eA, vA); 
        array_view<float, 2> mB(eB, vB); 
        array_view<float, 2> mC(eC, vC);
        mC.discard_data();

        parallel_for_each(defaultView, extent<2>(eC), Multiply(mA, mB, mC, W));

        mC.synchronize();
    });

    std::wcout << std::endl << "GPU functor (non tiled) exec time including copy-in/out: " <<
        elapsedTime << " (ms)" << std::endl;

    // Compare simple GPU functor and CPU results

    firstMismatch = std::mismatch(vC.cbegin(), vC.cend(), vRef.cbegin(), [](float c, float r) { return (fabs(c - r) < 0.01); });
    if (firstMismatch.first != vC.end())
    {
        size_t i = std::distance(vC.cbegin(), firstMismatch.first);
        std::wcout << "vC[" << i << "] = " << *firstMismatch.first << ", vRef[" << i << "] = " << *firstMismatch.second << std::endl;
    }
    std::wcout << " functor " << ((firstMismatch.first == vC.end()) ? "PASSED" : "FAILED") << std::endl;
}
