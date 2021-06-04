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

//--------------------------------------------------------------------------------------
// Data structures for storing particles.
//--------------------------------------------------------------------------------------

// Data structure for storing the position, velocity for each particle.
// The advanced integrator also requires the acceleration during each integration step.
//
// Note: Changes to the layout or size of this structure require that the HLSL renderer 
// also be changed to match. The particleSize value should equal the number of floats 
// stored in ParticleCpu.
//
// To improve memory access performance and limit false sharing of cache lines this 
// struct is padded to be the size if a single cache line.
//
// You can use the COREINFO tool to display information about your processor's cache size 
// and modify this code accordingly. COREINFO can be downloaded from TechNet
//
// http://technet.microsoft.com/en-us/sysinternals/cc835722
//
// In addition to padding for cache line size each float_3 element is also padded with an 
// additional float so that it is the same size as the __m128 SSE type used by the SSE based
// calculation engines.

#define SSE_ALIGNMENTBOUNDARY 16

__declspec(align(SSE_ALIGNMENTBOUNDARY))
struct ParticleCpu
{
    float_3 pos;
    float ssePpadding1;
    float_3 vel;
    float ssePpadding2;
    float_3 acc;
    float ssePpadding3;
    float_4 cacheLinePadding;
};

// The advanced integrator makes use of __m128 values directly. By ensuring alignement the 
// SSE code in NBodyAdvancedInteractionEngine can load values directly into SSE registers
// saving calls to explicitly load unaligned data (_mm_loadu_ps). This structure is also 
// padded to be the same size as ParticleCpu and fill a whole cache line.
//
// These two types could have been combined using a union but are kept separate here for 
// clarity and a cast is used when access to the __m128 values is needed.

__declspec(align(SSE_ALIGNMENTBOUNDARY))
struct ParticleSSE
{
    __m128 pos;
    __m128 vel;
    __m128 acc;
    __m128 cacheLinePadding;
};
