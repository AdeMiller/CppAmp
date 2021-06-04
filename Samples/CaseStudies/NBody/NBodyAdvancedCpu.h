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

#include <amp_short_vectors.h>
#include <concrtrm.h>

#include "ParticleCpu.h"
#include "NBodyCpu.h"


//--------------------------------------------------------------------------------------
//  An advanced integration engine.
//--------------------------------------------------------------------------------------
//
//  This is a much more sophisticated n-body implementation. It recursively divides up
//  the particles into chunks that can fit within the processor's L1 cache and then updates
//  each chunk in parallel. Itermediate values for each particle's acceleration are stored
//  during the calculation to take advantage of the reciprocal nature of the force calculation
//  where: F(a, b) = F(b, a).
//  
//  This is not discussed in detail in the book but more detail can be found here: 
//
//  http://software.intel.com/en-us/articles/a-cute-technique-for-avoiding-certain-race-conditions
//  http://software.intel.com/en-us/blogs/2010/07/01/n-bodies-a-parallel-tbb-solution-parallel-code-balanced-recursive-parallelism-with-parallel_invoke/
//
//  The SSE implementations also take advantage of the alignment of the __m128 data members to
//  avoid doing unaligned load operations.

class NBodyAdvancedInteractionEngine;

typedef void (NBodyAdvancedInteractionEngine::* NBodyAdvancedFunc)(ParticleCpu* const pParticles, const size_t iBegin, const size_t iEnd, const size_t jBegin, const size_t jEnd) const;

class NBodyAdvancedInteractionEngine
{
private:
    const float m_softeningSquared;
    const float m_particleMass;
    NBodyAdvancedFunc m_funcptr;

public:
    NBodyAdvancedInteractionEngine(float softeningSquared, float particleMass) :
        m_softeningSquared(softeningSquared),
        m_particleMass(particleMass),
        m_funcptr(nullptr)
    {
        SelectCpuImplementation();
    }

    inline void InvokeBodyBodyInteraction(ParticleCpu* const pParticles, const size_t iBegin, const size_t iEnd, const size_t jBegin, const size_t jEnd) const
    {
        assert(((uintptr_t)pParticles % SSE_ALIGNMENTBOUNDARY) == 0);
        (this->*m_funcptr)(pParticles, iBegin, iEnd, jBegin, jEnd); 
    };

private:
    void SelectCpuImplementation();

    // Different implementations of the body-body interaction.

    void BodyBodyInteraction(ParticleCpu* const pParticles, const size_t iBegin, const size_t iEnd, const size_t jBegin, const size_t jEnd) const;
    void BodyBodyInteractionSSE(ParticleCpu* const pParticles, const size_t iBegin, const size_t iEnd, const size_t jBegin, const size_t jEnd) const;
    void BodyBodyInteractionSSE4(ParticleCpu* const pParticles, const size_t iBegin, const size_t iEnd, const size_t jBegin, const size_t jEnd) const;
};

//--------------------------------------------------------------------------------------
//  Advanced parallel, cache aware implementation of the n-body calculation.
//--------------------------------------------------------------------------------------
//
//  This give a much better indication of what is possible on a CPU. When making direct
//  performance comparisons it is important to compare algorithms and implementations that
//  take advantage of the avainable hardware to the same degree.

class NBodyAdvanced : public INBodyCpu
{
private:
    std::shared_ptr<NBodyAdvancedInteractionEngine> m_engine;
    const float m_deltaTime;
    const float m_dampingFactor;
    size_t m_tileSize;                                          // Number of particles that fit into an L1 cache.
    mutable ParticleCpu* m_pBodiesCache;

public:
    NBodyAdvanced(float softeningSquared, float dampingFactor, float deltaTime, float particleMass, int tileSize) :
        INBodyCpu(),
        m_deltaTime(deltaTime),
        m_dampingFactor(dampingFactor),
        m_engine(new NBodyAdvancedInteractionEngine(softeningSquared, particleMass)),
        m_tileSize(tileSize),
        m_pBodiesCache(nullptr)
    {
    }

    void Integrate(ParticleCpu* const pParticles, ParticleCpu* const unused, int numParticles) const;

private:
    void InteractionList(const size_t begin, const size_t end) const;
    void InteractionCell(const size_t iBegin, const size_t iEnd, const size_t jBegin, const size_t jEnd) const;
};

//--------------------------------------------------------------------------------------
//  Utility functions.
//--------------------------------------------------------------------------------------

//  Get the size of the L1 cache.

int GetLevelOneCacheSize();