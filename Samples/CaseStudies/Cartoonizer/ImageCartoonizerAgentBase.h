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

#include <queue>
#include <vector>
#include "GdiWrap.h"

#include "ImageInfo.h"
#include "AgentBase.h"
#include "FrameProcessorFactory.h"

//--------------------------------------------------------------------------------------
//  Base class implementing core cartoonize functionality.
//--------------------------------------------------------------------------------------

class ImageCartoonizerAgentBase : public AgentBase
{
public:
    ImageCartoonizerAgentBase(IImagePipelineDialog* const pDialog, 
        ISource<bool>& cancellationSource, ITarget<ErrorInfo>& errorTarget) :
        AgentBase(pDialog, cancellationSource, errorTarget)
    {
    }

protected:
    void CartoonizeImage(const ImageInfoPtr& pInfo, 
        std::shared_ptr<IFrameProcessor>& processor, 
        const FilterSettings& settings) const
    {
#ifdef _DEBUG
        if (nullptr == pInfo)
            ATLTRACE("Cartoonize image: empty frame%S\n", IsCancellationPending() ? L" (skipped)" : L"");
        else
            ATLTRACE("Cartoonize image: frame %d%S\n", pInfo->GetSequence(), IsCancellationPending() ? L" (skipped)" : L"");
#endif
        try 
        {
            if (IsCancellationPending() || (nullptr == pInfo))
                return;
 
            pInfo->PhaseStart(kCartoonize);

            // Create bitmap to store result image, reuse existing bitmap if the size hasn't changed.

            BitmapPtr inBitmap = pInfo->GetBitmapPtr();
            BitmapPtr outBitmap = BitmapPtr(inBitmap->Clone(0, 0, inBitmap->GetWidth(), 
                                            inBitmap->GetHeight(), PixelFormat32bppARGB));

            //  Lock buffers on input and output bitmaps.

            Gdiplus::Rect rect(0, 0, inBitmap->GetWidth(), inBitmap->GetHeight());
            Gdiplus::BitmapData originalImage;
            Gdiplus::Status st = inBitmap->LockBits(&rect, Gdiplus::ImageLockModeWrite, 
                                                    PixelFormat32bppARGB, &originalImage);
            assert(st == Gdiplus::Ok);
            Gdiplus::BitmapData processedImage;
            st = outBitmap->LockBits(&rect, Gdiplus::ImageLockModeWrite, 
                                     PixelFormat32bppARGB, &processedImage);
            assert(st == Gdiplus::Ok);

            //  Process the image and update the ImageInfo.

            processor->ProcessImage(originalImage, processedImage,
                                    GetPhases(settings), GetNeighborWindow(settings));
            pInfo->SetBitmap(outBitmap);

            //  Unlock the bitmap buffers.

            inBitmap->UnlockBits(&originalImage);
            outBitmap->UnlockBits(&processedImage);

            pInfo->PhaseEnd(kCartoonize);
        }
        catch (CException* e)
        {
            ShutdownOnError(kCartoonize, pInfo, e);
            e->Delete();
        }
        catch (std::exception& e)
        {
            ShutdownOnError(kCartoonize, pInfo, e);
        }
    }
};
