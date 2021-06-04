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

#include "AgentBase.h"
#include "PipelineGovernor.h"

//--------------------------------------------------------------------------------------
//  Agent for updating latest image and notifying UI that the current image is stale.
//--------------------------------------------------------------------------------------

class ImageDisplayAgent : public AgentBase
{
private:
    IImagePipelineDialog* const m_pDialog;
    PipelineGovernor& m_governor;
    ISource<ImageInfoPtr>& m_imageInput;
    ImageInfoPtr m_pLatestImage;
    // Marked mutable as this is completely internal not public state.
    mutable critical_section m_latestImageLock; 

public:
    ImageDisplayAgent(IImagePipelineDialog* const pDialog, ISource<bool>& cancellationSource, ITarget<ErrorInfo>& errorTarget, 
            PipelineGovernor& governor, ISource<ImageInfoPtr>& imageInput) :
        AgentBase(pDialog, cancellationSource, errorTarget),
        m_pDialog(pDialog),
        m_governor(governor),
        m_imageInput(imageInput),
        m_pLatestImage(nullptr)
    {
    }

    void run()
    {
        ImageInfoPtr pInfo = nullptr;
        int imageProcessedCount = 0;
        do
        {
            pInfo = receive(m_imageInput);
            DisplayImage(pInfo);
            m_governor.FreePipelineSlot();
        }
        while(nullptr != pInfo);
        ATLTRACE("Display agent shutting down.\n");
        done();
    }

    ImageInfoPtr GetCurrentImage() const
    {
        ImageInfoPtr pInfo;
        {
            critical_section::scoped_lock lock(m_latestImageLock);
            pInfo = m_pLatestImage;
        }
        return pInfo;
    }

private:
    void DisplayImage(const ImageInfoPtr& pInfo)
    {
#ifdef _DEBUG
        if (nullptr == pInfo)
            ATLTRACE("Display image: empty frame%S.\n", IsCancellationPending() ? L" (skipped)" : L"");
        else
            ATLTRACE("Display image: frame %d%S.\n", pInfo->GetSequence(), IsCancellationPending() ? L" (skipped)" : L"");
#endif
        try
        {
            if (IsCancellationPending() || (nullptr == pInfo))
                return;

            pInfo->PhaseStart(kDisplay);
            {
                critical_section::scoped_lock lock(m_latestImageLock);
                m_pLatestImage = pInfo;                                         // Side effect; sets the latest image.
            }
            m_dialogWindow->NotifyImageUpdate();

            // Display phase ends in CartoonizerDlg::OnPaint after the image has been drawn.
        }
        catch (CException* e)
        {
            ShutdownOnError(kDisplay, pInfo, e);
            e->Delete();
        }
        catch (std::exception& e)
        {
            ShutdownOnError(kCartoonize, pInfo, e);
        }
    }
};