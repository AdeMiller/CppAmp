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

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <Dbt.h>
#include <agents.h>

#include "VideoFormatConverter.h"
#include "ImageInfo.h"
#include "ImageInfo.h"
#include "PipelineGovernor.h"
#include "AgentBase.h"

using namespace concurrency;

class VideoReader
{
private:
    CRITICAL_SECTION m_critsec;
    CComPtr<IMFSourceReader> m_pReader;
    VideoFormatConverter* m_converter;
    std::wstring m_symbolicLink;

public:
    VideoReader(VideoFormatConverter* const converter) : 
        m_pReader(nullptr),
        m_symbolicLink(L""),
        m_converter(converter)
    {
        InitializeCriticalSection(&m_critsec);
    }

    virtual ~VideoReader()
    {
        DeleteCriticalSection(&m_critsec);
    }

    HRESULT SetDevice(IMFActivate* pActivate);

    BitmapPtr CaptureFrame();

private:
    HRESULT TryMediaType(IMFMediaType *pType);
};
