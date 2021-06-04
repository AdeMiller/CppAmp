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
//  Simple integration implementation.
//--------------------------------------------------------------------------------------
//
//  This integrator does not specify a tiling or use tile static memory.

class NBodyAmpSimple : public INBodyAmp
{
private:
    float m_softeningSquared;
    float m_dampingFactor;
    float m_deltaTime;
    float m_particleMass;

public:
    NBodyAmpSimple(float softeningSquared, float dampingFactor, float deltaTime, float particleMass) :
        m_softeningSquared(softeningSquared),
        m_dampingFactor(dampingFactor),
        m_deltaTime(deltaTime),
        m_particleMass(particleMass)
    {
    }

    //  No tiling.
    inline int TileSize() const { return 1; }

    void Integrate(const std::vector<std::shared_ptr<TaskData>>& particleData, 
        int numParticles) const
    {
        assert(numParticles > 0);
        assert((numParticles % 4) == 0);

        ParticlesAmp particlesIn = *particleData[0]->DataOld;
        ParticlesAmp particlesOut = *particleData[0]->DataNew;

        extent<1> computeDomain(numParticles);
        const float softeningSquared = m_softeningSquared;
        const float dampingFactor = m_dampingFactor;
        const float deltaTime = m_deltaTime;
        const float particleMass = m_particleMass;

        parallel_for_each(computeDomain, [=] (index<1> idx) restrict(amp)
        {
            float_3 pos = particlesIn.pos[idx];
            float_3 vel = particlesIn.vel[idx];
            float_3 acc = 0.0f;

            // Update current Particle using all other particles
            for (int j = 0; j < numParticles; ++j)                
                BodyBodyInteraction(acc, pos, particlesIn.pos[j], softeningSquared, particleMass);

            vel += acc * deltaTime;
            vel *= dampingFactor;
            pos += vel * deltaTime;

            particlesOut.pos[idx] = pos;
            particlesOut.vel[idx] = vel;
        });
    }
};
