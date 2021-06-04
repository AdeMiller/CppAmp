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
#include "GdiWrap.h"

#include"ImageInfo.h"
#include "VideoReader.h"
#include "VideoFormatConverter.h"

using namespace concurrency;
//--------------------------------------------------------------------------------------
//  Image readers for; a single (in memory) frame, a folder of images and a video stream.
//--------------------------------------------------------------------------------------

class IFrameReader
{
public:
    virtual ImageInfoPtr NextFrame(int sequence, const LARGE_INTEGER& clockOffset) = 0;
};

//  Read a single image and then terminate the pipeline.

class ImageSingleFileReader : public IFrameReader
{
private:
    bool m_isFirst;
    std::wstring m_filePath;
    int m_sequence;

public:
    ImageSingleFileReader(const std::wstring& directoryPath, ImageInfoPtr image) : 
        m_isFirst(true),
        m_filePath(directoryPath),
        m_sequence(image->GetSequence())
    { 
        m_filePath.append(image->GetName());
    }

    ImageInfoPtr NextFrame(int sequence, const LARGE_INTEGER& clockOffset)
    {
        //  Show the image once and then return a null to shut down the pipeline.
        if (!m_isFirst)
            return nullptr;
        m_isFirst = false;
        BitmapPtr img = BitmapUtils::LoadBitmapAndConvert(m_filePath);
        ImageInfoPtr pInfo = std::make_shared<ImageInfo>(m_sequence, FileUtils::GetFilenameFromPath(m_filePath), img.get(), clockOffset);
        ATLTRACE("Reading file: %d %S\n", pInfo->GetSequence(), pInfo->GetName().c_str());
        return pInfo;
    }
  
private:
    // Disable copy constructor and assignment.
    ImageSingleFileReader(const ImageSingleFileReader&);
    ImageSingleFileReader const & operator=(ImageSingleFileReader const&);
};

//  Read a sequence of images from files in a folder.

class ImageFileFolderReader : public IFrameReader
{
private:
    std::vector<std::wstring> m_filePaths;

public:
    ImageFileFolderReader(const std::wstring& directoryPath)
    {
        m_filePaths = FileUtils::ListFilesInDirectory(directoryPath, L"jpg");
        std::vector<std::wstring> jpegs = FileUtils::ListFilesInDirectory(directoryPath, L"jpeg");
        std::copy(jpegs.begin(), jpegs.end(), std::back_inserter(m_filePaths));
        std::sort(m_filePaths.begin(), m_filePaths.end());
    }

    ImageInfoPtr NextFrame(int sequence, const LARGE_INTEGER& clockOffset)
    {
        std::wstring filePath = m_filePaths[sequence % m_filePaths.size()];
        BitmapPtr img = BitmapUtils::LoadBitmapAndConvert(filePath);
        ImageInfoPtr pInfo = std::make_shared<ImageInfo>(sequence, FileUtils::GetFilenameFromPath(filePath), img.get(), clockOffset);
        ATLTRACE("Reading file: %d %S\n", pInfo->GetSequence(), pInfo->GetName().c_str());
        return pInfo;
    }
  
private:
    // Disable copy constructor and assignment.
    ImageFileFolderReader(const ImageFileFolderReader&);
    ImageFileFolderReader const & operator=(ImageFileFolderReader const&);
};

//  Read a sequence of video images, or a single image from a camera.

class VideoStreamReader : public IFrameReader
{
private:
    std::unique_ptr<VideoReader> m_camera;
    CComPtr<IMFActivate> m_source;
    VideoFormatConverter m_converter;
    int m_frameCount;

public:
    VideoStreamReader(CComPtr<IMFActivate> source, bool singleFrame = false) : 
        m_source(source),
        m_frameCount(singleFrame ? 0 : 2)
    {
        m_camera = std::unique_ptr<VideoReader>(new VideoReader(&m_converter));
        m_camera->SetDevice(m_source);
    }

    ImageInfoPtr NextFrame(int sequence, const LARGE_INTEGER& clockOffset)
    {
        if (m_frameCount++ == 1)
            return nullptr;
        BitmapPtr frame = m_camera->CaptureFrame();    
        ImageInfoPtr pInfo = std::make_shared<ImageInfo>(sequence++, L"Camera frame", frame.get(), clockOffset);
        ATLTRACE("Reading video: %d %S\n", pInfo->GetSequence(), pInfo->GetName().c_str());
        return pInfo;
    }

private:
    // Disable copy constructor and assignment.
    VideoStreamReader(const VideoStreamReader&);
    VideoStreamReader const & operator=(VideoStreamReader const&);
};
