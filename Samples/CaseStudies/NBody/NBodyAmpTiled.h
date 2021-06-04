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

#include "NBodyAmp.h"

//--------------------------------------------------------------------------------------
//  Tiled integration implementation.
//--------------------------------------------------------------------------------------
//
//  This integrator explicitly defines the tiles to be used during the GPU kernel execution
//  and uses tile static memory to reduce the amount of access to the GPU's global memory.
//  It also unrolls loops to further improve performance.
//
//  The calculation is broken up into two halves as TiledBodyBodyInteraction is also used by 
//  NBodyAmpMultiTiled to execute a subset of the integration on different GPUs.

template <int TSize>
class NBodyAmpTiled : public INBodyAmp
{
private:
    float m_softeningSquared;
    float m_dampingFactor;
    float m_deltaTime;
    float m_particleMass;
    static const int m_tileSize = TSize;

public:
    NBodyAmpTiled(float softeningSquared, float dampingFactor, float deltaTime, float particleMass) :
        m_softeningSquared(softeningSquared),
        m_dampingFactor(dampingFactor),
        m_deltaTime(deltaTime),
        m_particleMass(particleMass)
    {
    }

    inline int TileSize() const { return m_tileSize; }

    inline void Integrate(const std::vector<std::shared_ptr<TaskData>>& particleData, int numParticles) const
    {
        TiledBodyBodyInteraction(*particleData[0]->DataOld, *particleData[0]->DataNew, 0, numParticles, numParticles);
    }

    //  Calculate interactions for a subset of particles in particlesIn, [rangeStart, rangeStart + rangeSize)

    void TiledBodyBodyInteraction(const ParticlesAmp& particlesIn, ParticlesAmp& particlesOut, 
        int rangeStart, int rangeSize, int numParticles) const
    {
        assert(particlesIn.size() == particlesOut.size());
        assert(rangeSize > 0);
        assert((numParticles % m_tileSize) == 0);
        assert((m_tileSize % 8) == 0);

        extent<1> computeDomain(rangeSize);
        const int numTiles = numParticles / m_tileSize;
        const float softeningSquared = m_softeningSquared;
        const float dampingFactor = m_dampingFactor;
        const float deltaTime = m_deltaTime;
        const float particleMass = m_particleMass;

        parallel_for_each(computeDomain.tile<m_tileSize>(), [=] (tiled_index<m_tileSize> ti) restrict(amp)
        {
            tile_static float_3 tilePosMemory[m_tileSize];

            const int idxLocal = ti.local[0];
            int idxGlobal = ti.global[0] + rangeStart;

            float_3 pos = particlesIn.pos[idxGlobal];
            float_3 vel = particlesIn.vel[idxGlobal];
            float_3 acc = 0.0f;

            // Update current Particle using all other particles
            int particleIdx = idxLocal;
            for (int tile = 0; tile < numTiles; tile++, particleIdx += m_tileSize)
            {
                // Cache current particle into shared memory to increase IO efficiency
                tilePosMemory[idxLocal] = particlesIn.pos[particleIdx];
                // Wait for caching on all threads in the tile to complete before calculation uses the data.
                ti.barrier.wait();

                // Unroll size should be multile of m_tileSize
                // Unrolling 4 helps improve perf on both ATI and nVidia cards
                // 4 is the sweet spot - increasing further adds no perf improvement while decreasing reduces perf
                for (int j = 0; j < m_tileSize; )
                {
                    BodyBodyInteraction(acc, pos, tilePosMemory[j++], softeningSquared, particleMass);
                    BodyBodyInteraction(acc, pos, tilePosMemory[j++], softeningSquared, particleMass);
                    BodyBodyInteraction(acc, pos, tilePosMemory[j++], softeningSquared, particleMass);
                    BodyBodyInteraction(acc, pos, tilePosMemory[j++], softeningSquared, particleMass);
                }

                // Wait for all threads to finish reading tile memory before allowing a new tile to start.
                ti.barrier.wait();
            }

            vel += acc * deltaTime;
            vel *= dampingFactor;
            pos += vel * deltaTime;

            particlesOut.pos[idxGlobal] = pos;
            particlesOut.vel[idxGlobal] = vel;
        });
    }
};
