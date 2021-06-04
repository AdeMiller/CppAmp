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
#include <assert.h>
#include <atlbase.h>
#include <random>
#include <memory>
#include <algorithm>

#include "Common.h"
#include "NBodyAdvancedCpu.h"

using namespace concurrency;
using namespace concurrency::graphics;

//--------------------------------------------------------------------------------------
//  The interaction engine to update all particles.
//--------------------------------------------------------------------------------------
//
//  Each function takes a list of particles and updates the block of particles between 
//  [iBegin, iEnd) and [jBegin, jEnd). It stores the updated accelerations for each particle.
//
//  This is not discussed in detail in the book but more detail can be found here: 
//
//  http://software.intel.com/en-us/articles/a-cute-technique-for-avoiding-certain-race-conditions
//  http://software.intel.com/en-us/blogs/2010/07/01/n-bodies-a-parallel-tbb-solution-parallel-code-balanced-recursive-parallelism-with-parallel_invoke/

//  Select which interaction engine to use based on the available SSE support.

void NBodyAdvancedInteractionEngine::SelectCpuImplementation()
{
    switch (GetSSEType())
    {
    case kCpuSSE4:
        m_funcptr = &NBodyAdvancedInteractionEngine::BodyBodyInteractionSSE4;
        break;
    case kCpuSSE:
        m_funcptr = &NBodyAdvancedInteractionEngine::BodyBodyInteractionSSE;
        break;
    default:
        m_funcptr = &NBodyAdvancedInteractionEngine::BodyBodyInteraction;
    }
}

void NBodyAdvancedInteractionEngine::BodyBodyInteraction(ParticleCpu* const pParticles, const size_t iBegin, const size_t iEnd, const size_t jBegin, const size_t jEnd) const
{
    // The inner loop is not parallelized because Integrate and InteractionList are already running on all cores.

    for (size_t i = iBegin; i < iEnd; ++i)
    {
        for (size_t j = jBegin; j < jEnd; ++j)
        {
            const float_3 r = pParticles[j].pos - pParticles[i].pos;
            const float distSqr = SqrLength(r) + m_softeningSquared;

            float invDist = 1.0f / sqrt(distSqr);
            float invDistCube =  invDist * invDist * invDist;
            float s = m_particleMass * invDistCube;

            // Cache intermediate acceleration results for both particles in this interaction.
            pParticles[i].acc += r * s;
            pParticles[j].acc -= r * s;
        }
    }
}

void NBodyAdvancedInteractionEngine::BodyBodyInteractionSSE(ParticleCpu* const pParticles, const size_t iBegin, const size_t iEnd, const size_t jBegin, const size_t jEnd) const
{
    ParticleSSE* const pParticlesSSE = reinterpret_cast<ParticleSSE* const>(pParticles);
    const __m128 softeningSquared = _mm_load1_ps( &m_softeningSquared);
    const __m128 particleMass = _mm_load1_ps( &m_particleMass );

    // The inner loop is not parallelized because Integrate and InteractionList are already running on all cores.

    for (size_t i = iBegin; i < iEnd; ++i)
    {
        for (size_t j = jBegin; j < jEnd; ++j)
        {
            //const float_3 r = pParticles[j].pos - pParticles[i].pos;
            __m128 r = _mm_sub_ps(pParticlesSSE[j].pos, pParticlesSSE[i].pos);

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

            //m_pBodiesCache[i].acc += r * s;
            //m_pBodiesCache[j].acc -= r * s;
            __m128 k = _mm_mul_ps(r, s);
            pParticlesSSE[i].acc = _mm_add_ps(pParticlesSSE[i].acc, k);
            pParticlesSSE[j].acc = _mm_sub_ps(pParticlesSSE[j].acc, k);
        }
    }
}

void NBodyAdvancedInteractionEngine::BodyBodyInteractionSSE4(ParticleCpu* const pParticles, const size_t iBegin, const size_t iEnd, const size_t jBegin, const size_t jEnd) const
{
    ParticleSSE* const pParticlesSSE = reinterpret_cast<ParticleSSE* const>(pParticles);
    const __m128 softeningSquared = _mm_load1_ps( &m_softeningSquared);
    const __m128 particleMass = _mm_load1_ps( &m_particleMass );

    // The inner loop is not parallelized because Integrate and InteractionList are already running on all cores.

    for (size_t i = iBegin; i < iEnd; ++i)
    {
        for (size_t j = jBegin; j < jEnd; ++j)
        {
            //const float_3 r = m_pBodiesCache[j].pos - m_pBodiesCache[i].pos;
            __m128 r = _mm_sub_ps(pParticlesSSE[j].pos, pParticlesSSE[i].pos);
            //const float distSqr = SqrLength(r) + m_softeningSquared;
            //This uses the additional SSE4 _mm_dp_ps intrinsic.
            __m128 distSqr = _mm_add_ps(_mm_dp_ps(r, r, 0x7F), softeningSquared);

            //float invDist = 1.0f / sqrt(distSqr);
            //float invDistCube =  invDist * invDist * invDist;
            //float s = m_particleMass * invDistCube;
            __m128 invDistSqr = _mm_rsqrt_ps(distSqr);
            __m128 invDistCube = _mm_mul_ps(_mm_mul_ps(invDistSqr, invDistSqr), invDistSqr);            
            __m128 s = _mm_mul_ps(particleMass, invDistCube); 

            //m_pBodiesCache[i].acc += r * s;
            //m_pBodiesCache[j].acc -= r * s;
            __m128 k = _mm_mul_ps(r, s);
            pParticlesSSE[i].acc = _mm_add_ps(pParticlesSSE[i].acc, k);
            pParticlesSSE[j].acc = _mm_sub_ps(pParticlesSSE[j].acc, k);
        }
    }
}

//--------------------------------------------------------------------------------------
//  Advanced parallel, cache aware implementation of the n-body calculation.
//--------------------------------------------------------------------------------------
//
// Particles are updated in place so the particleOut parameter is unused.

#pragma warning(push)
#pragma warning(disable:4100)   // Ignore unused parameter warning.

void NBodyAdvanced::Integrate(ParticleCpu* const pParticles, ParticleCpu* const unused, int numParticles) const
{
    // Maintain local global reference to pBodies, saves pushing it on stack for each call.
    m_pBodiesCache = pParticles;
    // Break calculations down into chunks of interations whose particles fit into the L1 cache.
    InteractionList(0, numParticles);

    parallel_for_each(pParticles, pParticles + numParticles, [=](ParticleCpu& b)
    {
        b.vel += b.acc * m_deltaTime;
        b.vel *= m_dampingFactor;
        b.pos += b.vel * m_deltaTime;
        // Reset acceleration values before starting next integration step.
        b.acc = 0.0f;
    });
}

#pragma warning(pop)

//  Recursively break down the list into chunks that fit within the L1 cache.

void NBodyAdvanced::InteractionList(const size_t begin, const size_t end) const
{
    const size_t width = end - begin;

    if (width > m_tileSize)
    {
        const size_t middle = begin + (width / 2);
        parallel_invoke([=] { InteractionList(begin, middle); },
            [=] { InteractionList(middle, end); });
        InteractionCell(begin, middle, middle, end);
    }
    else if (width > 1)
    {
        const size_t middle = begin + (width / 2);
        InteractionList(begin, middle);
        InteractionList(middle, end);
        InteractionCell(begin, middle, middle, end);
    }
}

//  For each cell update the particles once they fit into L1 cache.

void NBodyAdvanced::InteractionCell(const size_t iBegin, const size_t iEnd, const size_t jBegin, const size_t jEnd) const
{
    const size_t iWidth = iEnd - iBegin;
    const size_t jWidth = jEnd - jBegin;

    if (iWidth > m_tileSize && jWidth > m_tileSize)
    {
        const size_t iMiddle = iBegin + (iWidth / 2);
        const size_t jMiddle = jBegin + (jWidth / 2);
        parallel_invoke([=] { InteractionCell(iBegin, iMiddle, jBegin, jMiddle); },
            [=] { InteractionCell(iMiddle, iEnd, jMiddle, jEnd); });
        parallel_invoke([=] { InteractionCell(iBegin, iMiddle, jMiddle, jEnd); },
            [=] { InteractionCell(iMiddle, iEnd, jBegin, jMiddle); });
    }
    else
    {
        m_engine->InvokeBodyBodyInteraction(m_pBodiesCache, iBegin, iEnd, jBegin, jEnd);
    }
}

//--------------------------------------------------------------------------------------
//  Utility functions.
//--------------------------------------------------------------------------------------

//  Get size of the L1 cache.
//
//  Assume that all L1 caches for each logical processor are the same size and return the first one.

typedef BOOL (WINAPI* GetProcInfoFunc)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, DWORD*);

int GetLevelOneCacheSize()
{
    //  If this code fails at any point then just default to 16k.
    const int defaultCacheSize = 1024 * 16; 
    GetProcInfoFunc funcptr = (GetProcInfoFunc)::GetProcAddress(GetModuleHandle(TEXT("kernel32")), "GetLogicalProcessorInformation");

    if (nullptr == funcptr) 
        return defaultCacheSize;

    typedef std::unique_ptr<SYSTEM_LOGICAL_PROCESSOR_INFORMATION, FreeDeleter<SYSTEM_LOGICAL_PROCESSOR_INFORMATION>> BufferType;

    BufferType buffer(nullptr);
    DWORD bufferSize = 0;

    // Loop through twice. First pass gets buffer size, second pass fills buffer.

    while (true)
    {
        DWORD ret = funcptr(buffer.get(), &bufferSize);
        if (0 != ret) 
            break;

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) 
            return defaultCacheSize;

        buffer = BufferType((SYSTEM_LOGICAL_PROCESSOR_INFORMATION*)std::malloc(bufferSize));
        if (nullptr == buffer.get()) 
            return defaultCacheSize;
    }

    // Get all L1 cache sizes.

    int bufferLen = bufferSize / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
    std::vector<DWORD> cacheSizes(0);
    std::for_each(buffer.get(), buffer.get() + bufferLen, [&cacheSizes](SYSTEM_LOGICAL_PROCESSOR_INFORMATION& r)
    {
        if ((RelationCache == r.Relationship) && (1 == r.Cache.Level))
            cacheSizes.push_back(r.Cache.Size);
    });

    assert(std::count_if(cacheSizes.begin(), cacheSizes.end(), [cacheSizes](DWORD r){ return (r != cacheSizes[0]); }) == 0);
    return cacheSizes[0];
}
