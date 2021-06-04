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

#pragma once

#include "NBodyAmpTiled.h"

//--------------------------------------------------------------------------------------
//  Tiled, multi-accelerator integration implementation.
//--------------------------------------------------------------------------------------
//
//  This implementation loads all particles onto each available GPU but each GPU only 
//  updates a range of particles. After updates are complete each updated range is copied 
//  into the CPU memory (the m_hostPos and m_hostVel vectors). Once all the data is available 
//  in CPU memory it is copied back to each GPU, which is now ready for the next integration.
//
//  The tile size is passed in as a template parameter allowing the calling code to easily create new 
//  instances with different tile sizes. See NBodyFactory() in NBodyGravityApp.cpp for examples.

template <int TSize>
class NBodyAmpMultiTiled : public INBodyAmp
{
    // These are considered mutable because they are cache arrays for accelerator/host copies.
    // They are member variables so they can be allocated once outside of the Integrate method.
    mutable std::vector<float_3> m_hostPos;
    mutable std::vector<float_3> m_hostVel;

    NBodyAmpTiled<TSize> m_engine;

public:
    NBodyAmpMultiTiled(float softeningSquared, float dampingFactor, float deltaTime, float particleMass, int maxParticles) :
        m_hostPos(maxParticles),
        m_hostVel(maxParticles),
        m_engine(softeningSquared, dampingFactor, deltaTime, particleMass)
    {
    }

    inline int TileSize() const { return m_engine.TileSize(); }

    void Integrate(const std::vector<std::shared_ptr<TaskData>>& particleData, int numParticles) const
    {
        assert(particleData.size() > 1);

        const int tileSize = m_engine.TileSize();
        const int numAccs = int(particleData.size());
        const int rangeSize = ((numParticles / tileSize) / int(numAccs)) * tileSize;
        std::vector<completion_future> copyResults(2 * numAccs);

        // Update range of particles on each accelerator using the same tiled implementation as NBodyAmpTiled
        // Copy the results back to the CPU so they can be swapped with other GPUs.

        parallel_for(0, numAccs, [=, this, &copyResults](int i)
        {
            const int rangeStart = static_cast<int>(i) * rangeSize;
            m_engine.TiledBodyBodyInteraction((*particleData[i]->DataOld), (*particleData[i]->DataNew), rangeStart, rangeSize, numParticles);
            array_view<float_3, 1> posSrc = particleData[i]->DataNew->pos.section(rangeStart, rangeSize);
            copyResults[i] = copy_async(posSrc, m_hostPos.begin() + rangeStart); 
            array_view<float_3, 1> velSrc = particleData[i]->DataNew->vel.section(rangeStart, rangeSize);
            copyResults[i + numAccs] = copy_async(velSrc, m_hostVel.begin() + rangeStart); 
        });

        parallel_for_each(copyResults.cbegin(), copyResults.cend(), [](const completion_future& f) { f.get(); });

        // Sync updated particles back onto all accelerators. Even for N=58368 simple copy is faster than
        // only copying updated data to individual accelerator.

        // TODO_AMP: Is this really the case? Try re-writing to only copy the required data and see if this is faster.

        parallel_for(0, numAccs, [=, this, &copyResults] (int i)
        {
            copyResults[i] = copy_async(m_hostPos.begin(), particleData[i]->DataNew->pos);
            copyResults[i + numAccs] = copy_async(m_hostVel.begin(), particleData[i]->DataNew->vel);
        });

        parallel_for_each(copyResults.cbegin(), copyResults.cend(), [] (const completion_future& f) { f.get(); });
    }
};
