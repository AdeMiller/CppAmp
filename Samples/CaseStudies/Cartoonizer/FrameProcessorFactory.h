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

#include <memory>
#include <amp.h>
#include <afx.h>
#include "AmpUtilities.h"
#include "IFrameProcessor.h"
#include "FrameProcessorAmp.h"
#include "FrameProcessorAmpSingle.h"
#include "FrameProcessorAmpTextureSingle.h"
#include "FrameProcessorAmpMulti.h"
#include "FrameProcessorCpuSingle.h"
#include "FrameProcessorCpuMulti.h"

using namespace concurrency;

// Note: These values correspond to the ordering of items in the UI combo box.

enum FrameProcessorType
{
    kNone = -1,

    //  These use the ImageCartoonizerAgent and run different cartoonizers on a 
    //  single GPU or CPU

    kCpuSingle = 0,
    kCpuMulti,
    kAmpSimple,
    kAmpTiled,
    kAmpTexture,
    kAmpWarpSimple,
    kAmpWarpTiled,

    //  These use ImageCartoonizerAgent to run a processor that processes 
    //  images by dividing the work for a single image across multiple GPUs.

    kAmpMulti = 7,              
    kAmpMultiSimple = 7,
    kAmpMultiTiled,

    //  These use the ImageCartoonizerAgentParallel to run either the 
    //  simple, tiled or textures processor on different pipeline agents targeting multiple GPUs.

    kAmpPipeline = 9,
    kAmpSimplePipeline = 9,
    kAmpTiledPipeline,
    kAmpTexturePipeline
};

//--------------------------------------------------------------------------------------
//  Factory for creating frame processors.
//--------------------------------------------------------------------------------------

class FrameProcessorFactory
{
public:
    static std::shared_ptr<IFrameProcessor> Create(FrameProcessorType processorType,
        const accelerator&  accel = accelerator(accelerator::default_accelerator))
    {
        ATLTRACE("Using frame processor: %d, ", processorType);
        switch (processorType)
        {
        case kAmpMultiTiled:
            ATLTRACE("C++ AMP multi-GPU, tiled.\n");
            return std::make_shared<FrameProcessorAmpMultiTiled>(AmpUtils::GetAccelerators());
            break;
        case kAmpMultiSimple:
            ATLTRACE("C++ AMP multi-GPU, simple.\n");
            return std::make_shared<FrameProcessorAmpMulti>(AmpUtils::GetAccelerators());
            break;
        case kAmpWarpTiled:
            ATLTRACE("WARP tiled.\n");
            return std::make_shared<FrameProcessorAmpSingleTiled>(accelerator(accelerator::direct3d_warp));
            break;
        case kAmpWarpSimple:
            ATLTRACE("WARP simple.\n");
            return std::make_shared<FrameProcessorAmpSingle>(accelerator(accelerator::direct3d_warp));
            break;
        case kAmpTexture:
            ATLTRACE("C++ AMP texture.\n");
            return std::make_shared<FrameProcessorAmpTextureSingle>(accel);
            break;
        case kAmpTiled:
            ATLTRACE("C++ AMP tiled.\n");
            return std::make_shared<FrameProcessorAmpSingleTiled>(accel);
            break;
        case kAmpSimple:
            ATLTRACE("C++ AMP simple.\n");
            return std::make_shared<FrameProcessorAmpSingle>(accel);
            break;
        case kCpuSingle:
            ATLTRACE("CPU single core.\n");
            return std::make_shared<FrameProcessorCpuSingle>();
            break;
        case kCpuMulti:
            ATLTRACE("CPU multi-core.\n");
            return std::make_shared<FrameProcessorCpuMulti>();
            break;
        default:
            assert(false);
            return nullptr;
            break;
        }
    }
};
