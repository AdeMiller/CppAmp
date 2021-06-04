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

#include <agents.h>

#include "VideoSource.h"
#include "VideoReader.h"
#include "AgentBase.h"

#include <shlwapi.h>

using namespace concurrency;

class ScopedCriticialSection
{
private:
    CRITICAL_SECTION* m_section;

public:
    ScopedCriticialSection(CRITICAL_SECTION* const section) : m_section(section)
    {
        EnterCriticalSection(m_section);
    }

    ~ScopedCriticialSection()
    {
        LeaveCriticalSection(m_section);
    }

private:
    // Hide assignment operator and copy constructor.
    ScopedCriticialSection const &operator =(ScopedCriticialSection const&);
    ScopedCriticialSection(ScopedCriticialSection const &);
};

//-------------------------------------------------------------------
// Test a proposed video format.
//-------------------------------------------------------------------

HRESULT VideoReader::TryMediaType(IMFMediaType* pType)
{
    HRESULT hr = S_OK;
    GUID subtype = { 0 };

    hr = pType->GetGUID(MF_MT_SUBTYPE, &subtype);

    if (FAILED(hr)) 
        return hr;    

    // Format supported natively, no need to search for a converter.

    if (m_converter->IsFormatSupported(subtype))
    {
        hr = m_converter->SetVideoType(pType);
        return hr;
    }

    // Can we decode this media type to one of our supported
    // output formats?

    bool bFound = false;
    for (DWORD i = 0; ; i++)
    {
        m_converter->GetFormat(i, &subtype);
        hr = pType->SetGUID(MF_MT_SUBTYPE, subtype);
            
        if (FAILED(hr)) 
            break;

        hr = m_pReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, pType);

        if (SUCCEEDED(hr))
        {
            bFound = true;
            break;
        }
    }

    if (bFound)
        hr = m_converter->SetVideoType(pType);
    return hr;
}

//-------------------------------------------------------------------
// Set up preview for a specified video capture device. 
//-------------------------------------------------------------------

HRESULT VideoReader::SetDevice(IMFActivate* pActivate)
{
    HRESULT hr = S_OK;
    CComPtr<IMFMediaSource> pSource;
    CComPtr<IMFAttributes> pAttributes;
    ScopedCriticialSection critSec(&m_critsec);

    // Release the current device, if any.

    m_pReader = nullptr;

    hr = pActivate->ActivateObject(__uuidof(IMFMediaSource), (void**)&pSource);

    // Get the symbolic link.
    if (SUCCEEDED(hr))
    {
        WCHAR* pwszSymbolicLink;
        UINT32 cchSymbolicLink;

        hr = pActivate->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &pwszSymbolicLink, &cchSymbolicLink);
        std::wstring m_symbolicLink;
        m_symbolicLink = pwszSymbolicLink;
        CoTaskMemFree(pwszSymbolicLink);
    }

    if (SUCCEEDED(hr))
        hr = MFCreateAttributes(&pAttributes, 2);

    if (SUCCEEDED(hr))
        hr = pAttributes->SetUINT32(MF_READWRITE_DISABLE_CONVERTERS, true);

    if (SUCCEEDED(hr))
        hr = pAttributes->SetUINT32(MF_SOURCE_READER_DISCONNECT_MEDIASOURCE_ON_SHUTDOWN, true);

    if (SUCCEEDED(hr))
    {
        hr = MFCreateSourceReaderFromMediaSource(
            pSource,
            pAttributes,
            &m_pReader
            );
    }

    // Try to find a suitable output type.
    if (SUCCEEDED(hr))
    {
        for (DWORD i = 0; ; i++)
        {
            CComPtr<IMFMediaType> pType;
            hr = m_pReader->GetNativeMediaType(
                (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                i,
                &pType
                );

            if (FAILED(hr))
                break;

            hr = TryMediaType(pType);

            if (SUCCEEDED(hr))
                break;
        }
    }

    if (FAILED(hr))
    {
        if (pSource)
        {
            pSource->Shutdown();

            // NOTE: The source reader shuts down the media source
            // by default, but we might not have gotten that far.
        }
        m_pReader = nullptr;
    }

    return hr;
}

BitmapPtr VideoReader::CaptureFrame()
{
    CComPtr<IMFSample> pSample;
    DWORD streamFlags;
    ScopedCriticialSection critSec(&m_critsec);

    do
    {
        HRESULT hr = m_pReader->ReadSample(
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            0,
            nullptr,
            &streamFlags,
            nullptr,
            &pSample
            );
        if (FAILED(hr))
            throw hr;
    } 
    while ((pSample == nullptr) || (streamFlags != 0));

    CComPtr<IMFMediaBuffer> pBuffer;
    HRESULT hr = pSample->GetBufferByIndex(0, &pBuffer);
    if (FAILED(hr))
        throw hr;
    BitmapPtr frame = std::make_shared<Gdiplus::Bitmap>(m_converter->GetWidth(), m_converter->GetHeight(), PixelFormat32bppARGB);
    hr = m_converter->ConvertFrame(pBuffer, *frame.get());
    return frame;
}
