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

//--------------------------------------------------------------------------------------
//  Interface for all classes that implement n-body calculations.
//--------------------------------------------------------------------------------------
//
//  Each class implements the Integrate method. Some update pParticlesIn in place, others
//  leave the input array unchanged and write the new values to pParticlesOut.

struct ParticleCpu;

class INBodyCpu
{
public:
    virtual void Integrate(ParticleCpu* const pParticlesIn, 
        ParticleCpu* const pParticlesOut, int numParticles) const = 0;
};
