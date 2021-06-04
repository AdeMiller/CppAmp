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

using namespace concurrency;

//--------------------------------------------------------------------------------------
//  Agent for resizing images and correcting aspect ratios.
//--------------------------------------------------------------------------------------

class ImageResizeAgent : public AgentBase
{
private:
    ISource<ImageInfoPtr>& m_imageInput;
    ITarget<ImageInfoPtr>& m_imageOutput;
    MFRatio m_aspectRatio;

public:
    ImageResizeAgent(IImagePipelineDialog* const pDialog, ISource<bool>& cancellationSource, ITarget<ErrorInfo>& errorTarget,
            ISource<ImageInfoPtr>& imageInput, ITarget<ImageInfoPtr>& imageOutput, MFRatio aspectRatio) :
        AgentBase(pDialog, cancellationSource, errorTarget),
        m_imageInput(imageInput),
        m_imageOutput(imageOutput),
        m_aspectRatio(aspectRatio)
    {
    }

    void run()
    {
        ImageInfoPtr pInfo = nullptr;
        do
        {
            pInfo = receive(m_imageInput);
            SIZE outputSize = m_dialogWindow->GetImageSize();
            ResizeImage(pInfo, outputSize, m_aspectRatio);
            asend(m_imageOutput, pInfo);
        }
        while (nullptr != pInfo);
        ATLTRACE("Resize agent shutting down.\n");
        done();
    }

private:
    void ResizeImage(const ImageInfoPtr& pInfo, const SIZE& size, const MFRatio& aspectRatio) const
    {
#ifdef _DEBUG
        if (nullptr == pInfo)
            ATLTRACE("Resize image: empty frame%S.\n", IsCancellationPending() ? L" (skipped)" : L"");
        else
            ATLTRACE("Resize image: frame %d%S.\n", pInfo->GetSequence(), IsCancellationPending() ? L" (skipped)" : L"");
#endif

        try
        {
            if (IsCancellationPending() || (nullptr == pInfo))
                return;

            pInfo->PhaseStart(kResize);

            RECT correctedSize = ImageUtils::CorrectResize(pInfo->GetSize(), size, aspectRatio);
            pInfo->ResizeImage(correctedSize);

            pInfo->PhaseEnd(kResize);
        }
        catch (CException* e)
        {
            ShutdownOnError(kResize, pInfo, e);
            e->Delete();
        }
        catch (std::exception& e)
        {
            ShutdownOnError(kCartoonize, pInfo, e);
        }
    }
};