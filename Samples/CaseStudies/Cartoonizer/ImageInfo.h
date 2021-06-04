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

#include <string>
#include <vector>
#include <numeric>
#include <algorithm>
#include "GdiWrap.h"

#include "utilities.h"

enum PipelineStage
{
    kLoad,
    kResize,    
    kCartoonize,
    kDisplay,
};

enum Sequence
{
    kFirstImage = 0,
    kLastImageSentinel = -1,
};

//--------------------------------------------------------------------------------------
//  Tracking image progress for each phase of the pipeline.
//--------------------------------------------------------------------------------------

class ImagePerformanceData
{
private:
    int m_sequenceNumber;
    LARGE_INTEGER m_clockOffset;
    std::vector<LONGLONG> m_phaseStartTick;
    std::vector<LONGLONG> m_phaseEndTick;

public:
    ImagePerformanceData(int sequenceNumber) :  
        m_sequenceNumber(sequenceNumber), 
        m_phaseStartTick(4, 0),
        m_phaseEndTick(4, 0)
    {
    }

    void SetStartTick(int phase)
    {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        m_phaseStartTick[phase] = now.QuadPart - m_clockOffset.QuadPart;
    }

    void SetStartTick(int phase, const LARGE_INTEGER& start)
    {
        m_phaseStartTick[phase] = start.QuadPart - m_clockOffset.QuadPart;
    }

    void SetEndTick(int phase)
    {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        m_phaseEndTick[phase] = now.QuadPart - m_clockOffset.QuadPart;
    }

    void SetClockOffset(const LARGE_INTEGER& offset) { m_clockOffset = offset; }

    int GetSequence() const { return m_sequenceNumber; }

    LONGLONG GetPhaseDuration(int phase) const { return m_phaseEndTick[phase] - m_phaseStartTick[phase]; }
};

//--------------------------------------------------------------------------------------
//  Tracking image performance over the whole pipeline.
//--------------------------------------------------------------------------------------

class PipelinePerformanceData
{
private:
    int m_imageCount;
    LARGE_INTEGER m_startTime;
    LARGE_INTEGER m_currentTime;
    LARGE_INTEGER m_clockFrequency;
    std::vector<LONGLONG> m_totalPhaseTime;
    int m_cartoonizerParallelism;

public:
    PipelinePerformanceData(int cartoonizerParallelism = 1) :
        m_imageCount(1),
        m_startTime(),
        m_currentTime(),
        m_clockFrequency(),
        m_totalPhaseTime(4, 0),
        m_cartoonizerParallelism(cartoonizerParallelism)
    {
        m_currentTime.QuadPart = m_startTime.QuadPart = 0;
        QueryPerformanceFrequency(&m_clockFrequency);
    }

    inline double GetAveragePhaseTime(int phase) const 
    {
        //  Correct phase time for cartoonizer when there are multiple cartoonizers so 
        //  that UI displays the apparent time per frame.
        double correction = (phase == kCartoonize) ? double(m_cartoonizerParallelism) : 1.0;

        return (m_imageCount == 0) ? 0 : (1000.0 * double(m_totalPhaseTime[phase]) / double(correction * m_imageCount * m_clockFrequency.QuadPart)); 
    }

    inline double GetElapsedTime() const { return double(m_currentTime.QuadPart - m_startTime.QuadPart) / double(m_clockFrequency.QuadPart); }

    inline double GetTimePerImage() const { return (m_imageCount == 0) ? 0 : (1000.0 * GetElapsedTime() / double(m_imageCount)); }

    void Start()
    {
        QueryPerformanceCounter(&m_startTime);
        QueryPerformanceCounter(&m_currentTime);
        m_imageCount = 0;
        std::fill(m_totalPhaseTime.begin(), m_totalPhaseTime.end(), 0);
    }

    void Update(const ImagePerformanceData& data)
    {
        QueryPerformanceCounter(&m_currentTime);
        m_imageCount++;
        for (int i = 0; i < 4; i++)
            m_totalPhaseTime[i] += data.GetPhaseDuration(i);
    }
};

//--------------------------------------------------------------------------------------
//  Container for bitmap in the pipeline, its associated data and performance data.
//--------------------------------------------------------------------------------------

class ImageInfo
{
private:
    int m_sequenceNumber;
    std::wstring m_imageName;
    BitmapPtr m_pBitmap;

    ImagePerformanceData m_currentImagePerformance;
    bool m_isEmpty;

public:
    inline void SetBitmap(const BitmapPtr& pBitmap) { m_pBitmap = pBitmap; };

    ImageInfo(int sequenceNumber, const std::wstring& fileName, Gdiplus::Bitmap* const originalImage);

    ImageInfo(int sequenceNumber, const std::wstring& fileName, Gdiplus::Bitmap* const originalImage, const LARGE_INTEGER& clockOffset);

    inline BitmapPtr GetBitmapPtr() const { return m_pBitmap; }

    inline std::wstring GetName() const { return m_imageName; }

    inline bool IsEmpty() const { return m_isEmpty; }

    inline void ResetSequence() { m_sequenceNumber = kFirstImage; }

    SIZE GetSize() const
    {
        SIZE size;
        if (m_pBitmap == nullptr)
        {
            size.cx = size.cy = 0;
            return size;
        }
        size.cx = m_pBitmap->GetWidth();
        size.cy = m_pBitmap->GetHeight();
        return size;
    }

    inline int GetSequence() const { return m_sequenceNumber; }

    inline ImagePerformanceData GetPerformanceData() const { return m_currentImagePerformance; }

    void ResizeImage(const RECT& rect);

    void PhaseStart(int phase);

    void PhaseEnd(int phase);

    // Special case for first phase which creates ImageInfo after starting.
    void PhaseEnd(int phase, const LARGE_INTEGER& start);

private:
    // Hide copy constructor.
    ImageInfo(const ImageInfo& rhs);

    void ImageInfo::Initialize(Gdiplus::Bitmap* const originalImage);
};

typedef std::shared_ptr<ImageInfo> ImageInfoPtr;
