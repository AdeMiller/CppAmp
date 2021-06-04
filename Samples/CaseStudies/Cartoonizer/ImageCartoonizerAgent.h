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

#include "ImageCartoonizerAgentBase.h"

//--------------------------------------------------------------------------------------
//  Sequential cartoonizer agent. Processes images sequentially on a single GPU.
//--------------------------------------------------------------------------------------

class ImageCartoonizerAgent : public ImageCartoonizerAgentBase
{
private:
    ISource<ImageInfoPtr>& m_imageInput;
    ITarget<ImageInfoPtr>& m_imageOutput;
    std::shared_ptr<IFrameProcessor> m_processor;

public:
    ImageCartoonizerAgent(IImagePipelineDialog* const pDialog, FrameProcessorType processorType, 
        ISource<bool>& cancellationSource, ITarget<ErrorInfo>& errorTarget, 
        ISource<ImageInfoPtr>& imageInput, ITarget<ImageInfoPtr>& imageOutput) :
        ImageCartoonizerAgentBase(pDialog, cancellationSource, errorTarget),
        m_imageInput(imageInput), m_imageOutput(imageOutput),
        m_processor(FrameProcessorFactory::Create(processorType))
    {
    }

    void run()
    {
        ImageInfoPtr pInfo = nullptr;
        do
        {
            pInfo = receive(m_imageInput);
            CartoonizeImage(pInfo, m_processor, m_dialogWindow->GetFilterSettings());
            asend(m_imageOutput, pInfo);
        }
        while(nullptr != pInfo);
        ATLTRACE("Cartoonizer shutting down\n");
        done();
    }
};
