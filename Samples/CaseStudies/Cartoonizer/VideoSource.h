//--------------------------------------------------------------------------------------
// File: VideoSource.h
//
// Demonstrates how to use C++ AMP to do image processing.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <afxwin.h>
#include <mfapi.h>
#include <mfidl.h>

class VideoSource
{
public:
    CComPtr<IMFActivate> Source;
    std::wstring Name;

    // Required for STL collections.
    VideoSource() {}

    VideoSource(CComPtr<IMFActivate> source) : Source(source)
    {
        WCHAR* pFriendlyName = nullptr;
        UINT32 nameLen = 0;
        HRESULT hr = Source->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &pFriendlyName, &nameLen);
        if (SUCCEEDED(hr))
            Name = std::wstring(pFriendlyName);
        CoTaskMemFree(pFriendlyName);
    }

    VideoSource(const std::wstring& name) : Name(name), Source(nullptr)
    {
        // Create a dummy video source. Used to add a dummy source for folder of bitmaps.
    }

    static std::vector<VideoSource> GetVideoSources()
    {
        std::vector<VideoSource> m_sources;
        CComPtr<IMFAttributes> pAttributes = nullptr;

        HRESULT hr = MFCreateAttributes(&pAttributes, 1);
        if (FAILED(hr)) throw hr;

        hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
        if (FAILED(hr)) throw hr;

        UINT32 numDevs = 0;
        IMFActivate **ppDevices = nullptr;
        hr = MFEnumDeviceSources(pAttributes, &ppDevices, &numDevs);
        if (FAILED(hr)) throw hr;
        m_sources.resize(numDevs);
        for (size_t i = 0; i < numDevs; ++i)
        {
            CComPtr<IMFActivate> p;
            p = ppDevices[i];
            m_sources[i] = p;
        }
        CoTaskMemFree(ppDevices);
        return m_sources;
    }
};
