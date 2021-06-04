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

#include <ppl.h>
#include <agents.h>
#include <mfobjects.h>

#include "AgentBase.h"
#include "ImageResizeAgent.h"
#include "CartoonizerFactory.h"
#include "ImageDisplayAgent.h"
#include "IFrameProcessor.h"
#include "IFrameReader.h"
#include "PipelineGovernor.h"
#include "ImageInfo.h"
#include "utilities.h"

// The ImagePipeline pipeline uses control flow agents to process messages containing ImageInfoPtr.
//
// To shutdown the pipeline the m_cancelMessage buffer is set to true. Once m_cancelMessage is true the 
// head of the pipeline stops generating data. Each pipeline stage passes messages but does no processing 
// on them and waits for a nullptr message before shutting down. Finally, the head of the pipeline sends 
// a message of nullptr causing each agent's run() method to exit its message processing loop and call 
// done(). This ensures that all buffers are completely empty on shutdown.
//
// If one of the stages throws an exception then the AgentBase::ShutdownOnError method will notify the UI and 
// send a cancel message to shutdown the pipeline.

using namespace concurrency;

//--------------------------------------------------------------------------------------
//  Pipeline agent for processing images.
//--------------------------------------------------------------------------------------

class ImagePipeline : public AgentBase
{
private:
    std::shared_ptr<IFrameReader> m_frameReader;
    FrameProcessorType m_processorType;
    PipelineGovernor m_governor;
    unbounded_buffer<ImageInfoPtr> m_buffer1, m_buffer2, m_buffer3;
    std::unique_ptr<ImageResizeAgent> m_imageResizer;
    std::shared_ptr<ImageCartoonizerAgentBase> m_imageCartoonizer;
    std::unique_ptr<ImageDisplayAgent> m_imageDisplayer;

public:
    ImagePipeline(IImagePipelineDialog* const dialog, std::shared_ptr<IFrameReader> reader, 
        FrameProcessorType processorType, int pipelineCapacity,
        ISource<bool>& cancel, ITarget<ErrorInfo>& errorTarget) :
        AgentBase(dialog, cancel, errorTarget),
        m_frameReader(reader), m_processorType(processorType),
        m_governor(pipelineCapacity), m_imageResizer(nullptr), m_imageCartoonizer(nullptr), 
        m_imageDisplayer(nullptr)
    {
        Initialize();
    }

    ImageInfoPtr GetCurrentImage() const { return m_imageDisplayer->GetCurrentImage(); }

    int GetCartoonizerProcessorCount() const
    {
        return (m_processorType >= kAmpPipeline) ? static_cast<int>(AmpUtils::GetAccelerators().size()) : 1;
    }

    void run()
    {
        m_imageResizer->start();
        m_imageCartoonizer->start();
        m_imageDisplayer->start();
        
        LARGE_INTEGER clockOffset;
        QueryPerformanceCounter(&clockOffset);
        int sequence = kFirstImage;
        ImageInfoPtr pInfo;
        try
        {
            do
            {
                // Record the start time of the image reading phase before loading the image.
                LARGE_INTEGER start;
                QueryPerformanceCounter(&start);
                pInfo = m_frameReader->NextFrame(sequence++, clockOffset);
                if (pInfo != nullptr)
                {
                    pInfo->PhaseEnd(kLoad, start);
                    // Don't push more data into the pipeline if it is already full to capacity.
                    m_governor.WaitForAvailablePipelineSlot();
                }
                asend(m_buffer1, pInfo);
            }
            while ((pInfo != nullptr) && !IsCancellationPending());
        }
        catch (CException* e)
        {
            ShutdownOnError(kLoad, pInfo, e);
            e->Delete();
        }
        catch (std::exception& e)
        {
            ShutdownOnError(kLoad, pInfo, e);
        }

        // Wait for empty pipeline and if a nullptr hasn't been sent then send final nullptr through the pipeline to ensure all stages shutdown.

        ATLTRACE("Image pipeline waiting for pipeline to empty...\n");
        m_governor.WaitForEmptyPipeline();
        ATLTRACE("Image pipeline is empty.\n");
        if (pInfo != nullptr)
            asend<ImageInfoPtr>(m_buffer1, nullptr);

        //  Wait for all the agents to shut down and then shut down this agent.

        agent* agents[3] = { m_imageResizer.get(), m_imageCartoonizer.get(), m_imageDisplayer.get() };
        agent::wait_for_all(3, agents);
        ATLTRACE("Image pipeline agents done.\n");
        done();
        ATLTRACE("Image pipeline shutdown complete.\n");
    }

private:
    void Initialize()
    {
        // Aspect ratio for JPEG images is always 1:1
        MFRatio aspectRatio;
        aspectRatio.Numerator = aspectRatio.Denominator = 1;

        m_imageResizer = 
            std::unique_ptr<ImageResizeAgent>(new ImageResizeAgent(m_dialogWindow, 
                m_cancellationSource, m_errorTarget, m_buffer1, m_buffer2, aspectRatio));       
        m_imageCartoonizer = 
            CartoonizerFactory::Create(m_dialogWindow, m_processorType, 
                m_cancellationSource, m_errorTarget, m_buffer2, m_buffer3);
        m_imageDisplayer = std::unique_ptr<ImageDisplayAgent>(new ImageDisplayAgent(
            m_dialogWindow, m_cancellationSource, m_errorTarget, m_governor, m_buffer3));
    }
};
