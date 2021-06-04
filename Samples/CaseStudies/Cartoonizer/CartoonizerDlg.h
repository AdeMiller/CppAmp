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

#include "targetver.h"
#include <afxwin.h>
#include <afxdialogex.h> 
#include <array>
#include "ImagePipeline.h"
#include "VideoReader.h"
#include "Resource.h"

enum PipelineState
{
    kPipelineStopped,
    kPipelineCartoonize,
    kPipelineRunning,
};

class CartoonizerDlg : public CDialogEx, IImagePipelineDialog
{
public:
    CartoonizerDlg(CWnd* pParent = nullptr);

    ~CartoonizerDlg()
    {
        MFShutdown();
        CoUninitialize();
    }

#pragma region IImagePipelineDialog implementation

    inline SIZE GetImageSize() const { return m_displaySize; }

    inline FilterSettings GetFilterSettings() const
    {
        return FilterSettings(m_simplifierPhases, m_simplifierNeighborBorder * 2);
    }

    inline VideoSource GetInputSource() const { return m_inputSources[m_currentSource]; }

    inline void NotifyImageUpdate() { PostMessageW(WM_UPDATEWINDOW, 0, 0); }

    inline void NotifyError() { PostMessageW(WM_REPORTERROR, 0, 0); }

#pragma endregion

    enum { IDD = IDD_IMAGEPIPELINE_DIALOG };
    enum { DDXWriteData = false, DDXReadData = true };
    enum { PictureSource = 0, VideoSources = 1 };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);

private:
    // Input sources, video or list of files on disk.

    int m_currentSource;
    std::vector<VideoSource> m_inputSources;
    std::vector<std::wstring> m_filePaths;

    // Pipeline control

    std::unique_ptr<ImagePipeline> m_pipeline;
    overwrite_buffer<bool> m_cancelMessage;
    unbounded_buffer<ErrorInfo> m_errorMessages;
    std::wstring m_imageName;
    bool m_singleImageMode;
    static const int kPipelineCapacity = 6;

    // Image processing settings

    FrameProcessorType m_frameProcessorType;
    int m_simplifierPhases;
    int m_simplifierNeighborBorder;

    // Cache current image for resizing etc

    ImageInfoPtr m_latestImage;

    // Performance statistics

    ImagePerformanceData m_currentImagePerformance;
    PipelinePerformanceData m_pipelinePerformance;

    // Dialog sizing and image display

    static const int m_imageTop = 10;
    static const int m_imageLeft = 10;
    static const int m_consoleWidth = 250;
    static const int m_consoleHeightWin8 = 760;
    static const int m_consoleHeightWin7 = 620;
    int m_consoleHeight;

    SIZE m_displaySize;
    SIZE m_previousBitmapSize;
    Gdiplus::SolidBrush m_backgroundBrush;

    bool ConfigureSources();
    void StopPipeline();
    void SetButtonState(PipelineState isRunning);
    void ReportError(const ErrorInfo& error);

    template<typename T>
    void DDX_TextFormatted(CDataExchange* pDX, int nIDC, T value, LPCTSTR format = _T("%4.1f"))
    {
        CString tmp;
        tmp.Format(format, value); 
        DDX_Text(pDX, nIDC, tmp);
    }

    int AddComboItem(CComboBox* pDropDown, std::wstring labels[], int index)
    {
        int itemIndex = pDropDown->AddString(labels[index].c_str());
        pDropDown->SetItemData(itemIndex, index);
        return itemIndex;
    }

    inline bool IsPipelineRunning() { return (m_pipeline != nullptr); }

    inline bool VideoEnabled() { return (m_currentSource != PictureSource) || (m_currentSource == PictureSource && m_filePaths.empty()); }

    inline bool PicturesEnabled() { return (m_currentSource == PictureSource) && (!m_filePaths.empty()); }

    inline bool IsPictureSource() { return (m_currentSource == PictureSource) && PicturesEnabled(); }

    void LoadVideoFrame();

    void LoadImage(int sequence);

protected:
    HICON m_hIcon;

    // Generated message map functions

    virtual BOOL OnInitDialog();
    afx_msg HCURSOR OnQueryDragIcon();
    afx_msg void OnPaint();
    Gdiplus::Color GetDialogBackgroundColor();
    afx_msg LRESULT OnHScroll(WPARAM wParam, LPARAM lParam);
    void ResizeControl(UINT nControlID, int cx, int cy, int shiftl, int shiftr);

    DECLARE_MESSAGE_MAP()

public:
    afx_msg void OnBnClickedButtonLoadNext();
    afx_msg void OnBnClickedButtonCartoonize();
    afx_msg void OnBnClickedButtonReload();    
    afx_msg void OnBnClickedButtonStart();
    afx_msg void OnBnClickedButtonStop();
    afx_msg void OnBnClickedCancel();

    afx_msg LRESULT OnReportError(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnUpdateWindow(WPARAM wParam, LPARAM lParam);

    void OnGetMinMaxInfo(MINMAXINFO FAR* lpMMI);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnCbnSelchangeComboInput();
    BOOL OnDeviceChange(UINT nEventType, DWORD_PTR dwData);
};
