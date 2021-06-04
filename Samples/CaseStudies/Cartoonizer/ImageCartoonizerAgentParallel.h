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
//  Parallel cartoonizer agent. Processes images in parallel on a multiple GPUs.
//--------------------------------------------------------------------------------------

class ImageCartoonizerAgentParallel : public ImageCartoonizerAgentBase
{
private:
    ISource<ImageInfoPtr>& m_imageInput;
    ITarget<ImageInfoPtr>& m_imageOutput;
    std::vector<std::shared_ptr<IFrameProcessor>> m_processors;
    unbounded_buffer<ImageInfoPtr> m_inputBuffer;
    int m_multiplexSequence;
    std::unique_ptr<call<ImageInfoPtr>> m_multiplexer;
    unbounded_buffer<ImageInfoPtr> m_multiplexBuffer;

    // Functor for ordering ImageInfoPtr. Used by the multiplexer's priority queue.

    struct CompareImageInfoPtr
    {
        bool operator()(const ImageInfoPtr& lhs, const ImageInfoPtr& rhs) const
        {
            return (lhs->GetSequence() > rhs->GetSequence());
        }
    };
    std::priority_queue<ImageInfoPtr, std::vector<ImageInfoPtr>, CompareImageInfoPtr> 
        m_multiplexQueue;

public:
    ImageCartoonizerAgentParallel(IImagePipelineDialog* const pDialog, 
        FrameProcessorType processorType, 
        ISource<bool>& cancellationSource, ITarget<ErrorInfo>& errorTarget, 
        ISource<ImageInfoPtr>& imageInput, ITarget<ImageInfoPtr>& imageOutput) :
        ImageCartoonizerAgentBase(pDialog, cancellationSource, errorTarget),
        m_multiplexSequence(kFirstImage), m_processors(),
        m_imageInput(imageInput), m_imageOutput(imageOutput)
    {
        Initialize(processorType);
        m_imageInput.link_target(&m_inputBuffer);
    }

    void run()
    {
        parallel_for_each(m_processors.begin(), m_processors.end(), 
            [=](std::shared_ptr<IFrameProcessor>& p)
        {
            ImageInfoPtr pInfo = nullptr;
            do
            {
                pInfo = receive(m_inputBuffer);
                CartoonizeImage(pInfo, p, m_dialogWindow->GetFilterSettings());
                //  Each processor pushes another nullptr into the input to make sure that all of the processors shut down.
                asend((pInfo == nullptr) ? m_inputBuffer : m_multiplexBuffer, pInfo);
            }
            while (nullptr != pInfo);
            ATLTRACE("Cartoonizer frame processor shutting down.\n");
        });
        asend<ImageInfoPtr>(m_multiplexBuffer, nullptr);
        ATLTRACE("Cartoonizer agent shutting down.\n");
        done();
    }

private:
    void Initialize(FrameProcessorType processorType)
    {
        std::vector<accelerator> accels = AmpUtils::GetAccelerators();
        m_processors.resize(accels.size());
        std::transform(accels.cbegin(), accels.cend(), m_processors.begin(), 
            [=](const accelerator& acc)->std::shared_ptr<IFrameProcessor>
        {
            ATLTRACE("Creating cartoonizer for %S.\n", acc.description.c_str()); 
            return FrameProcessorFactory::Create(processorType, acc); 
        });

        m_multiplexer = std::unique_ptr<call<ImageInfoPtr>>(new call<ImageInfoPtr>(
            [this](ImageInfoPtr pInfo)
            {
                ATLTRACE("Multiplexer: Recieving frame %d.\n", (pInfo == nullptr) ? -1 : pInfo->GetSequence());
                if (pInfo == nullptr)
                {
                    asend<ImageInfoPtr>(m_imageOutput, nullptr);
                    ATLTRACE("Multiplexer shutting down.\n");
                    return;
                }
                // Push new image into queue
                m_multiplexQueue.push(pInfo);

                //  If the top image in the queue is the next expected one in the sequence then send it.
                //  Need to send as many images as are in seqence to maintain order. 
                while (!m_multiplexQueue.empty() && 
                       (m_multiplexQueue.top()->GetSequence() == m_multiplexSequence))
                {
                    ATLTRACE("Multiplexer: Sending frame %d.\n", m_multiplexQueue.top()->GetSequence());
                    asend(m_imageOutput, m_multiplexQueue.top());
                    m_multiplexQueue.pop();
                    ++m_multiplexSequence;
                }
            }
        ));
        m_multiplexBuffer.link_target(m_multiplexer.get());
    }
};
