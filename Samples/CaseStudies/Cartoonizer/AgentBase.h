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

#include <sstream>
#include <exception>
#include <stdexcept>
#include <agents.h>
#include <ppl.h>
#include <tuple>
#include <memory>
#include <assert.h>
#include <atltrace.h>

#include "ImageInfo.h"
#include "IFrameProcessor.h"
#include "VideoSource.h"

#define WM_REPORTERROR (WM_USER + 1)
#define WM_UPDATEWINDOW (WM_USER + 2)

using namespace concurrency;


//--------------------------------------------------------------------------------------
//  Data transfer objects for passing information to and from the agent pipeline to the UI.
//--------------------------------------------------------------------------------------

typedef std::tuple<PipelineStage, std::wstring, std::wstring> ErrorInfo;

inline PipelineStage GetStage(ErrorInfo error) { return std::get<0>(error); }
inline std::wstring GetImageName(ErrorInfo error) { return std::get<1>(error); }
inline std::wstring GetMessage(ErrorInfo error) { return std::get<2>(error); }

typedef std::tuple<UINT, UINT> FilterSettings; 
inline UINT GetPhases(const FilterSettings& settings) { return std::get<0>(settings); }
inline UINT GetNeighborWindow(const FilterSettings& settings) { return std::get<1>(settings); }

//--------------------------------------------------------------------------------------
//  Interface to decouple dialog view from pipeline model.
//--------------------------------------------------------------------------------------

class IImagePipelineDialog
{
public:
    virtual SIZE GetImageSize() const = 0;
    virtual FilterSettings GetFilterSettings() const = 0;
    virtual VideoSource GetInputSource() const = 0;

    virtual void NotifyImageUpdate() = 0;
    virtual void NotifyError() = 0;
};

//--------------------------------------------------------------------------------------
//  Base class for all agents. Provides support for error propagation and cancellation.
//--------------------------------------------------------------------------------------

class AgentBase : public agent
{
protected:
    IImagePipelineDialog* m_dialogWindow;
    // Signal used to cancel the pipeline from the main UI.
    ISource<bool>& m_cancellationSource;
    // Used to pass error message details back to the UI.
    ITarget<ErrorInfo>& m_errorTarget;
    // Signal used by the pipeline to internally shut itself down on error.
    // Marked mutable to allow the agent work functions to be declared as const. This is true 
    // except in the error case where ShutdownOnError is called and m_errorPending is set to true.
    mutable overwrite_buffer<bool> m_errorPending;

    AgentBase(IImagePipelineDialog* const dialog, ISource<bool>& cancellationSource, ITarget<ErrorInfo>& errorTarget) : 
        m_dialogWindow(dialog), 
        m_cancellationSource(cancellationSource),
        m_errorTarget(errorTarget)
    {
        send(m_errorPending, false);
    }

public:
    bool IsCancellationPending() const { return receive(m_errorPending) || receive(m_cancellationSource); }

    // Only the ImageDisplayAgent, ImagePipeline and VideoPipeline implement this. Other agents do not support curent image.
    virtual ImageInfoPtr GetCurrentImage() const { assert(false); return nullptr; }

protected:
    // Shutdown on error: Signal shutdown pending to pipeline, send message to error buffer and notify dialog that an error has been reported.

    void ShutdownOnError(PipelineStage phase, const ImageInfoPtr& pInfo, const CException* const e) const
    {
        std::wstring message = GetExceptionMessage(e);
        SendError(phase, (pInfo != nullptr) ? pInfo->GetName() : L"Unknown", message);
    }

    void ShutdownOnError(PipelineStage phase, const ImageInfoPtr& pInfo, const std::exception& e) const
    {
        std::wostringstream message(std::stringstream::in | std::stringstream::out);
        message << e.what();
        SendError(phase, (pInfo != nullptr) ? pInfo->GetName() : L"Unknown", message.str());
    }

    void SendError(PipelineStage phase, const std::wstring& filePath, const std::wstring& message) const
    {
        ATLTRACE("Exception thrown for image %d\n", filePath.c_str());
        send(m_errorPending, true);
        send(m_errorTarget, ErrorInfo(phase, filePath, message));
        m_dialogWindow->NotifyError();
    }

private:
    std::wstring GetExceptionMessage(const CException* const e) const
    {
        const size_t maxLength = 255;
        TCHAR szCause[maxLength];
        e->GetErrorMessage(szCause, maxLength);
        std::wstring message(szCause);
        return message;
    }
};
