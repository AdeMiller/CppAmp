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

#include <concrtrm.h>

#include "INBodyCpu.h"
#include "ParticleCpu.h"

using namespace concurrency;
using namespace concurrency::graphics;

//  User selected integration algorithm implementation.

enum ComputeType
{
    kCpuSingle = 0,
    kCpuMulti = 1,
    kCpuAdvanced = 2
};

//  Level of SSE support available. Determined dynamically at runtime.

enum CpuSSE
{
    kCpuNone = 0,
    kCpuSSE,
    kCpuSSE4
};

//--------------------------------------------------------------------------------------
//  A simple integration engine.
//--------------------------------------------------------------------------------------
//
//  On initialization this picks the most performant integration engine and sets a function
//  pointer. During calculations this is used to quickly call the correct integration code.

class NBodySimpleInteractionEngine;

typedef void (NBodySimpleInteractionEngine::* NBodySimpleFunc)(const ParticleCpu* const pParticlesIn, ParticleCpu& particleOut, int numParticles) const;

class NBodySimpleInteractionEngine
{
private:
    float m_softeningSquared;
    float m_dampingFactor;
    float m_deltaTime;
    float m_particleMass;
    NBodySimpleFunc m_funcptr;

public:
    NBodySimpleInteractionEngine(float softeningSquared, float dampingFactor, float deltaTime, float particleMass) :
        m_softeningSquared(softeningSquared),
        m_dampingFactor(dampingFactor),
        m_deltaTime(deltaTime),
        m_particleMass(particleMass),
        m_funcptr(nullptr)
    {
        SelectCpuImplementation();
    }

    inline void InvokeBodyBodyInteraction(const ParticleCpu* const pParticlesIn, ParticleCpu& particleOut, int numParticles) const
    {
        (this->*m_funcptr)(pParticlesIn, particleOut, numParticles); 
    };

private:
    void SelectCpuImplementation();

    // Different implementations of the body-body interaction.

    void BodyBodyInteraction(const ParticleCpu* const pParticlesIn, ParticleCpu& particleOut, int numParticles) const;
    void BodyBodyInteractionSSE(const ParticleCpu* const pParticlesIn, ParticleCpu& particleOut, int numParticles) const;
    void BodyBodyInteractionSSE4(const ParticleCpu* const pParticlesIn, ParticleCpu& particleOut, int numParticles) const;
};

//--------------------------------------------------------------------------------------
//  Very simple sequential implementation of the n-body calculation.
//--------------------------------------------------------------------------------------
//
//  This allows direct comparison of the approach used by the C++ AMP code with the equivalent CPU code.
//  It is a very inefficient implementation. For a much more efficient version see NBodyCpuAdvanced.

class NBodySimpleSingleCore : public INBodyCpu
{
private:
    std::shared_ptr<NBodySimpleInteractionEngine> m_engine;

public:
    NBodySimpleSingleCore(float softeningSquared, float dampingFactor, float deltaTime, float particleMass) : 
        INBodyCpu(),
        m_engine(std::make_shared<NBodySimpleInteractionEngine>(softeningSquared, dampingFactor, deltaTime, particleMass))
    {
    }

    void Integrate(ParticleCpu* const pParticlesIn, ParticleCpu*const pParticlesOut, int numParticles) const;
};

//--------------------------------------------------------------------------------------
//  Very simple parallel implementation of the n-body calculation.
//--------------------------------------------------------------------------------------
//
//  This allows direct comparison of the approach used by the C++ AMP code with the equivalent CPU code.
//  It is a very inefficient implementation. For a much more efficient version see NBodyCpuAdvanced.

class NBodySimpleMultiCore : public INBodyCpu
{
private:
    std::shared_ptr<NBodySimpleInteractionEngine> m_engine;

public:
    NBodySimpleMultiCore(float softeningSquared, float dampingFactor, float deltaTime, float particleMass) : 
        INBodyCpu(),
        m_engine(new NBodySimpleInteractionEngine(softeningSquared, dampingFactor, deltaTime, particleMass))
    {
    }

    void Integrate(ParticleCpu* const pParticlesIn, ParticleCpu* const pParticlesOut, int numParticles) const;
};

//--------------------------------------------------------------------------------------
//  Utility functions.
//--------------------------------------------------------------------------------------

//  Generate a cluster of particles uniformly distributed within a sphere.
//  This is not a physically realistic model but it is adequate for demonstration purposes.

void LoadClusterParticles(ParticleCpu* const pParticles, float_3 center, float_3 velocity, float spread, int numParticles);

//  Get the level of SSE support available on the current hardware. 

inline CpuSSE GetSSEType();
