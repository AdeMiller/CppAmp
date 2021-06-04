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

#include "targetver.h"
#include <afxwin.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <string>
#include <assert.h>

#include "AgentBase.h"
#include "ImageInfo.h"
#include "utilities.h"

ImageInfo::ImageInfo(int sequenceNumber, const std::wstring& fileName, Gdiplus::Bitmap* const originalImage, const LARGE_INTEGER& clockOffset) :
    m_sequenceNumber(sequenceNumber),
    m_imageName(fileName),
    m_pBitmap(nullptr),
    m_currentImagePerformance(sequenceNumber)
{
    Initialize(originalImage);
    m_currentImagePerformance.SetClockOffset(clockOffset);
}

ImageInfo::ImageInfo(int sequenceNumber, const std::wstring& fileName, Gdiplus::Bitmap* const originalImage) :
    m_sequenceNumber(sequenceNumber),
    m_imageName(fileName),
    m_pBitmap(nullptr),
    m_currentImagePerformance(sequenceNumber)
{
    Initialize(originalImage);
    LARGE_INTEGER clockOffset;
    QueryPerformanceCounter(&clockOffset);
    m_currentImagePerformance.SetClockOffset(clockOffset);
}

void ImageInfo::Initialize(Gdiplus::Bitmap* const originalImage)
{
    if (originalImage == nullptr)
    {
        m_isEmpty = true;
        m_pBitmap = std::make_shared<Gdiplus::Bitmap>(1, 1, PixelFormat32bppARGB);
        return;
    }
    m_isEmpty = false;
    m_pBitmap = std::make_shared<Gdiplus::Bitmap>(originalImage->GetWidth(), originalImage->GetHeight(), PixelFormat32bppARGB);
    BitmapUtils::CopyBitmap(originalImage, m_pBitmap.get());
}

void ImageInfo::ResizeImage(const RECT& rect)
{
    SIZE size = { rect.right - rect.left, rect.bottom - rect.top };
    if (nullptr == m_pBitmap.get())
    {
        m_pBitmap = std::make_shared<Gdiplus::Bitmap>(size.cx, size.cy, PixelFormat32bppARGB);
        return;
    }

    SIZE srcSize = { m_pBitmap->GetWidth(), m_pBitmap->GetHeight() };
    if ((srcSize.cx == size.cx) && (srcSize.cy == size.cy))
        return;

    BitmapPtr pNewBitmap = std::make_shared<Gdiplus::Bitmap>(size.cx, size.cy, PixelFormat32bppARGB); 
    Gdiplus::Graphics graphics(pNewBitmap.get());
    graphics.DrawImage(m_pBitmap.get(), 0, 0, size.cx, size.cy);
    m_pBitmap = BitmapPtr(pNewBitmap);
}

void ImageInfo::PhaseStart(int phase)
{
    m_currentImagePerformance.SetStartTick(phase);
}

void ImageInfo::PhaseEnd(int phase)
{
    m_currentImagePerformance.SetEndTick(phase);
}

// Special case for first phase which creates ImageInfo after starting.
void ImageInfo::PhaseEnd(int phase, const LARGE_INTEGER& start)
{
    m_currentImagePerformance.SetStartTick(phase, start);
    m_currentImagePerformance.SetEndTick(phase);
}
