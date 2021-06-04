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

#include <math.h>
#include <ppl.h>
#include <concrtrm.h>
#include <amprt.h>
#include <assert.h>
#include <atlbase.h>
#include <random>
#include <amp.h>
#include <amp_graphics.h>
#include <amp_math.h>

#include "Common.h"
#include "INBodyAmp.h"
#include "AmpUtilities.h"

using namespace concurrency;
using namespace concurrency::graphics;

//--------------------------------------------------------------------------------------
//  Particle data structures.
//--------------------------------------------------------------------------------------

//  Data structure for storing particles on the CPU. Used during data initialization, which is
//  executed on the CPU. Also used for swapping partial results between multiple accelerators.
//
//  This is an struct of arrays, rather than the more conventional array of structs used by
//  the n-body CPU example. In general structs of arrays are more efficient for GPU programming.

struct ParticlesCpu
{
    std::vector<float_3> pos;
    std::vector<float_3> vel;

    ParticlesCpu(int size) : pos(size), vel(size) { }

    inline int size() const
    {
        assert(pos.size() == vel.size());
        return static_cast<int>(pos.size());
    }
};

//  Data structure for storing particles on the C++ AMP accelerator.
//
//  This is an struct of arrays, rather than the more conventional array of structs used by
//  the n-body CPU example. In general structs of arrays are more efficient for GPU programming.

struct ParticlesAmp
{
    array<float_3, 1>& pos;
    array<float_3, 1>& vel;

public:
    ParticlesAmp(array<float_3, 1>& pos, array<float_3, 1>& vel) : pos(pos), vel(vel) { }

    inline int size() const { return pos.extent.size(); }
};

//  Structure storing all the data associated with processing a subset of 
//  particles on a single C++ AMP accelerator.

struct TaskData
{
public:
    accelerator Accelerator;
    std::shared_ptr<ParticlesAmp> DataOld;      // These hold references to the data
    std::shared_ptr<ParticlesAmp> DataNew;

private:
    array<float_3, 1> m_posOld;                 // These hold the actual data.
    array<float_3, 1> m_posNew;
    array<float_3, 1> m_velOld;
    array<float_3, 1> m_velNew;

public:
    TaskData(int size, accelerator_view view, accelerator acc) : 
        Accelerator(acc), 
        m_posOld(size, view),
        m_velOld(size, view),
        m_posNew(size, view),
        m_velNew(size, view),
        DataOld(new ParticlesAmp(m_posOld, m_velOld)), 
        DataNew(new ParticlesAmp(m_posNew, m_velNew))
    {
    }  
};

std::vector<std::shared_ptr<TaskData>> CreateTasks(int numParticles, 
    accelerator_view renderView)
{
    std::vector<accelerator> gpuAccelerators = AmpUtils::GetGpuAccelerators();
    std::vector<std::shared_ptr<TaskData>> tasks;
    tasks.reserve(gpuAccelerators.size());

    if (!gpuAccelerators.empty())
    {
        //  Create first accelerator attached to main view. This will attach the C++ AMP 
        //  array<float_3> to the D3D buffer on the first GPU.
        tasks.push_back(std::make_shared<TaskData>(numParticles, renderView, gpuAccelerators[0]));

        //  All other GPUs are associated with their default view.
        std::for_each(gpuAccelerators.cbegin() + 1, gpuAccelerators.cend(), 
            [=, &tasks](const accelerator& d)
        {
            tasks.push_back(std::make_shared<TaskData>(numParticles, d.default_view, d));
        });
    }

    if (tasks.empty())
    {
        OutputDebugStringW(L"WARNING: No C++ AMP capable accelerators available, using REF.");
        accelerator a = accelerator(accelerator::default_accelerator);
        tasks.push_back(std::make_shared<TaskData>(numParticles, renderView, a));
    }

    AmpUtils::DebugListAccelerators(gpuAccelerators);
    return tasks;
}

//--------------------------------------------------------------------------------------
//  Calculate the acceleration (force * mass) change for a pair of particles.
//--------------------------------------------------------------------------------------

void BodyBodyInteraction(float_3& acc, const float_3 particlePosition, 
    const float_3 otherParticlePosition, 
    float softeningSquared, float particleMass) restrict(amp)
{
    float_3 r = otherParticlePosition - particlePosition;

    float distSqr = SqrLength(r) + softeningSquared;
    float invDist = concurrency::fast_math::rsqrt(distSqr);
    float invDistCube =  invDist * invDist * invDist;

    float s = particleMass * invDistCube;

    acc += r * s;
}

//--------------------------------------------------------------------------------------
//  Utility functions.
//--------------------------------------------------------------------------------------

void LoadClusterParticles(ParticlesCpu& particles, int offset, int size, float_3 center, float_3 velocity, float spread)
{
    std::random_device rd; 
    std::default_random_engine engine(rd()); 
    std::uniform_real_distribution<float> randRadius(0.0f, spread);
    std::uniform_real_distribution<float> randTheta(-1.0f, 1.0f);
    std::uniform_real_distribution<float> randPhi(0.0f, 2.0f * static_cast<float>(std::_Pi));

    for (int i = offset; i < (offset + size); ++i)
    {
        float_3 delta = PolarToCartesian(randRadius(engine), acos(randTheta(engine)), randPhi(engine));
        particles.pos[i] = center + delta; 
        particles.vel[i] = velocity;
    };  
}
