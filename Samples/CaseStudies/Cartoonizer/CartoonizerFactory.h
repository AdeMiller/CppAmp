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

#include "ImageCartoonizerAgentBase.h"
#include "ImageCartoonizerAgent.h"
#include "ImageCartoonizerAgentParallel.h"

//--------------------------------------------------------------------------------------
//  Factory for creating cartoonizer  processors.
//--------------------------------------------------------------------------------------

class CartoonizerFactory
{
public:
    static std::shared_ptr<ImageCartoonizerAgentBase> Create(IImagePipelineDialog* const pDialog, FrameProcessorType processorType, ISource<bool>& cancellationSource, ITarget<ErrorInfo>& errorTarget, 
        ISource<ImageInfoPtr>& imageInput, ITarget<ImageInfoPtr>& imageOutput)
    {
        ATLTRACE("Using cartoonizer processor: %d, ", processorType);
        switch (processorType)
        {
        //  Create a parallel pipeline with different image processors.
        case kAmpSimplePipeline:
            ATLTRACE("Parallel pipeline agent, simple.\n");
            return std::shared_ptr<ImageCartoonizerAgentBase>(new ImageCartoonizerAgentParallel(pDialog, kAmpSimple, cancellationSource, errorTarget, imageInput, imageOutput));
            break;
        case kAmpTiledPipeline:
            ATLTRACE("Parallel pipeline agent, tiled.\n");
            return std::shared_ptr<ImageCartoonizerAgentBase>(new ImageCartoonizerAgentParallel(pDialog, kAmpTiled, cancellationSource, errorTarget, imageInput, imageOutput));
            break;
        case kAmpTexturePipeline:
            ATLTRACE("Parallel pipeline agent, textures.\n");
            return std::shared_ptr<ImageCartoonizerAgentBase>(new ImageCartoonizerAgentParallel(pDialog, kAmpTexture, cancellationSource, errorTarget, imageInput, imageOutput));
            break;
        //  Create a sequential pipeline with different image processors.
        case kAmpMultiTiled:
        case kAmpMultiSimple:
        case kAmpWarpTiled:
        case kAmpWarpSimple:
        case kAmpTexture:
        case kAmpTiled:
        case kAmpSimple:
        case kCpuSingle:
        case kCpuMulti:
            ATLTRACE("Sequential pipeline agent, simple.\n");
            return std::shared_ptr<ImageCartoonizerAgentBase>(new ImageCartoonizerAgent(pDialog, processorType, cancellationSource, errorTarget, imageInput, imageOutput));
            break;
        default:
            assert(false);
            return nullptr;
            break;
        }
    }
};
