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

#include <string.h>
#include <math.h>
#include <ppl.h>
#include <concrtrm.h>
#include <amprt.h>
#include <assert.h>
#include <atlbase.h>
#include <random>
#include <memory>

#include "Common.h"
#include "NBodyCpu.h"

using namespace concurrency;
using namespace concurrency::graphics;

//--------------------------------------------------------------------------------------
//  The interaction engine to update a single particle.
//--------------------------------------------------------------------------------------
//
//  Each interaction function takes a list of particles as input and updates a single particle.
//  This implementation does not store intermediate acceleration values. For a more efficient implementation
//  see the advanced integrator.

//  Select which interaction engine to use based on the available SSE support.

void NBodySimpleInteractionEngine::SelectCpuImplementation()
{
    switch (GetSSEType())
    {
    case kCpuSSE4:
        m_funcptr = &NBodySimpleInteractionEngine::BodyBodyInteractionSSE4;
        break;
    case kCpuSSE:
        m_funcptr = &NBodySimpleInteractionEngine::BodyBodyInteractionSSE;
        break;
    default:
        m_funcptr = &NBodySimpleInteractionEngine::BodyBodyInteraction;
    }
}

void NBodySimpleInteractionEngine::BodyBodyInteraction(const ParticleCpu* const pParticlesIn, 
    ParticleCpu& particleOut, int numParticles) const 
{
    float_3 pos(particleOut.pos);
    float_3 vel(particleOut.vel);
    float_3 acc(0.0f);

    std::for_each(pParticlesIn, pParticlesIn + numParticles, [=, &acc](const ParticleCpu& p)
    {  
        const float_3 r = p.pos - pos;

        float distSqr = SqrLength(r) + m_softeningSquared;
        float invDist = 1.0f / sqrt(distSqr);
        float invDistCube =  invDist * invDist * invDist;
        float s = m_particleMass * invDistCube;

        // Note: The book code contains typos, the = operator is used instead of +=. 
        // The code below is correct.
        acc += r * s;
    });

    vel += acc * m_deltaTime;
    vel *= m_dampingFactor;
    pos += vel * m_deltaTime;

    particleOut.pos = pos;
    particleOut.vel = vel;
}

void NBodySimpleInteractionEngine::BodyBodyInteractionSSE(const ParticleCpu* const pParticlesIn, ParticleCpu& particleOut, int numParticles) const 
{
    const __m128 softeningSquared = _mm_load1_ps( &m_softeningSquared);
    const __m128 dampingFactor = _mm_load_ps1(&m_dampingFactor);
    const __m128 deltaTime = _mm_load_ps1(&m_deltaTime);
    const __m128 particleMass = _mm_load1_ps( &m_particleMass );

    //float_3 pos(particleOut.pos);
    //float_3 vel(particleOut.vel);
    //float_3 acc(0.0f);
    __m128 pos = _mm_loadu_ps((float*)&particleOut.pos);
    __m128 vel = _mm_loadu_ps((float*)&particleOut.vel);
    __m128 acc = _mm_setzero_ps();

    // Cannot use lambdas here because __m128 is aligned.
    for (int j = 0; j < numParticles; ++j)
    {    
        //float_3 r = p.pos - pos;
        __m128 pos1 = _mm_loadu_ps((float*)&pParticlesIn[j].pos);
        __m128 r = _mm_sub_ps(pos1, pos);

        //float distSqr = float_3::SqrLength(r) + m_softeningSquared;
        __m128 distSqr = _mm_mul_ps(r, r);    //x    y    z    ?
        __m128 rshuf = _mm_shuffle_ps(distSqr, distSqr, _MM_SHUFFLE(0,3,2,1));
        distSqr = _mm_add_ps(distSqr, rshuf);  //x+y, y+z, z+?, ?+x
        rshuf = _mm_shuffle_ps(distSqr, distSqr, _MM_SHUFFLE(1,0,3,2));
        distSqr = _mm_add_ps(rshuf, distSqr);  //x+y+z+0, y+z+0+X, z+0+x+y, 0+x+y+z
        distSqr = _mm_add_ps(distSqr, softeningSquared); 

        //float invDist = 1.0f / sqrt(distSqr);
        //float invDistCube =  invDist * invDist * invDist;
        //float s = m_particleMass * invDistCube;
        __m128 invDistSqr = _mm_rsqrt_ps(distSqr);
        __m128 invDistCube = _mm_mul_ps(_mm_mul_ps(invDistSqr, invDistSqr), invDistSqr);
        __m128 s = _mm_mul_ps(particleMass, invDistCube); 

        //acc += r * s;
        acc = _mm_add_ps( _mm_mul_ps(r, s), acc ); 
    }

    //vel += acc * m_deltaTime;
    vel = _mm_add_ps( _mm_mul_ps(acc, deltaTime), vel ); 

    //vel *= m_dampingFactor;   
    vel = _mm_mul_ps(vel, dampingFactor); 

    //pos += vel * m_deltaTime;
    pos = _mm_add_ps( _mm_mul_ps(vel, deltaTime), pos ); 

    // The r3 word in each register has an undefined value at this point but
    // this isn't used elsewhere so there is no need to clear it.
    //particleOut.pos = pos;
    //particleOut.vel = vel;
    _mm_storeu_ps((float*)&particleOut.pos, pos);
    _mm_storeu_ps((float*)&particleOut.vel, vel);
}

void NBodySimpleInteractionEngine::BodyBodyInteractionSSE4(const ParticleCpu* const pParticlesIn, ParticleCpu& particleOut, int numParticles) const 
{
    const __m128 softeningSquared = _mm_load1_ps( &m_softeningSquared);
    const __m128 dampingFactor = _mm_load_ps1(&m_dampingFactor);
    const __m128 deltaTime = _mm_load_ps1(&m_deltaTime);
    const __m128 particleMass = _mm_load1_ps( &m_particleMass );

    //float_3 pos(particleOut.pos);
    //float_3 vel(particleOut.vel);
    //float_3 acc(0.0f);
    __m128 pos = _mm_loadu_ps((float*)&particleOut.pos);
    __m128 vel = _mm_loadu_ps((float*)&particleOut.vel);
    __m128 acc = _mm_setzero_ps();

    // Cannot use lambdas here because __m128 is aligned.
    for (int j = 0; j < numParticles; ++j)
    {
        //float_3 r = p.pos - pos;
        __m128 pos1 = _mm_loadu_ps((float*)&pParticlesIn[j].pos);
        __m128 r = _mm_sub_ps(pos1, pos);

        //float distSqr = float_3::SqrLength(r) + m_softeningSquared;
        //This uses the additional SSE4 _mm_dp_ps intrinsic.
        __m128 distSqr = _mm_dp_ps(r, r, 0x7F);
        distSqr = _mm_add_ps(distSqr, softeningSquared);

        //float invDist = 1.0f / sqrt(distSqr);
        //float invDistCube =  invDist * invDist * invDist;
        //float s = m_particleMass * invDistCube;
        __m128 invDistSqr = _mm_rsqrt_ps(distSqr);
        __m128 invDistCube = _mm_mul_ps(_mm_mul_ps(invDistSqr, invDistSqr), invDistSqr);            
        __m128 s = _mm_mul_ps(particleMass, invDistCube); 

        //acc += r * s;
        acc = _mm_add_ps( _mm_mul_ps(r, s), acc ); 
    }

    //vel += acc * m_deltaTime;
    vel = _mm_add_ps( _mm_mul_ps(acc, deltaTime), vel ); 

    //vel *= m_dampingFactor;
    vel = _mm_mul_ps(vel, dampingFactor); 

    //pos += vel * m_deltaTime;
    pos = _mm_add_ps( _mm_mul_ps(vel, deltaTime), pos ); 

    // The r3 word in each register has an undefined value at this point but
    // this isn't used elsewhere so there is no need to clear it.
    //particleOut.pos = pos;
    //particleOut.vel = vel;
    _mm_storeu_ps((float*)&particleOut.pos, pos);
    _mm_storeu_ps((float*)&particleOut.vel, vel);   
}

//--------------------------------------------------------------------------------------
//  The sequential integration engine to update all particles.
//--------------------------------------------------------------------------------------
//
//  This updates all particles by calling the integration engine for each particle in the 
//  list.

void NBodySimpleSingleCore::Integrate(ParticleCpu* const pParticlesIn, 
    ParticleCpu* const pParticlesOut, int numParticles) const
{
    for (int i = 0; i < numParticles; ++i)
    {
        pParticlesOut[i] = pParticlesIn[i];    
        m_engine->InvokeBodyBodyInteraction(pParticlesIn, pParticlesOut[i], numParticles);
    }
}

//--------------------------------------------------------------------------------------
//  The parallel integration engine to update all particles.
//--------------------------------------------------------------------------------------
//
//  This uses the PPL to update chunks of particles in parallel on different threads.
//  This is thread safe because all threads read from a readonly copy of the particles
//  stored in pParticlesIn and only one thread writes to a given array element in pParticlesOut.
//  Ensuring that the ParticleCpu struct is aligned and occupies a whole cache line reduces the 
//  amount of false cache line shareing and improves performance. 

void NBodySimpleMultiCore::Integrate(ParticleCpu* const pParticlesIn, ParticleCpu* const pParticlesOut, int numParticles) const
{
    parallel_for(0, numParticles, [=, this, &pParticlesOut](int i)
    {
        pParticlesOut[i] = pParticlesIn[i];
        m_engine->InvokeBodyBodyInteraction(pParticlesIn, pParticlesOut[i], numParticles);
    });
}

//--------------------------------------------------------------------------------------
//  Utility functions.
//--------------------------------------------------------------------------------------

void LoadClusterParticles(ParticleCpu* const pParticles, float_3 center, float_3 velocity, 
    float spread, int numParticles)
{
    std::random_device rd; 
    std::default_random_engine engine(rd()); 
    std::uniform_real_distribution<float> randRadius(0.0f, spread);
    std::uniform_real_distribution<float> randTheta(-1.0f, 1.0f);
    std::uniform_real_distribution<float> randPhi(0.0f, 2.0f * static_cast<float>(std::_Pi));

    std::for_each(pParticles, pParticles + numParticles, 
        [=, &engine, &randRadius, &randTheta, &randPhi](ParticleCpu& p)
    {
        float_3 delta = PolarToCartesian(randRadius(engine), 
            acos(randTheta(engine)), randPhi(engine));
        p.pos = center + delta; 
        p.vel = velocity;
        p.acc = 0.0f;
    });  
}

inline CpuSSE GetSSEType()
{
    int CpuInfo[4] = { -1 };
    __cpuid(CpuInfo, 1);

    // Note: The book code contains typos, the && operator is used instead of & and 
    // CpuInfo is capitalized incorrectly. The code below is correct.

    if (CpuInfo[2] >> 19 & 0x1) return kCpuSSE4;
    if (CpuInfo[3] >> 24 & 0x1) return kCpuSSE;
    return kCpuNone;
}
