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
#include <string>
#include <memory>
#include "GdiWrap.h"
#include <assert.h>
#include <afxdialogex.h>

#include "FrameProcessorFactory.h"
#include "CartoonizerDlg.h"
#include "ImagePipeline.h"
#include "VideoSource.h"

CartoonizerDlg::CartoonizerDlg(CWnd* pParent /*=nullptr*/) : CDialogEx(CartoonizerDlg::IDD, pParent),
    m_inputSources(0),
    m_currentSource(0),
    m_filePaths(0),
    m_pipeline(nullptr),
    m_cancelMessage(),
    m_imageName(L""),
    m_singleImageMode(false),
    m_pipelinePerformance(),
    m_currentImagePerformance(0),
    m_simplifierPhases(11),                                     // Default values for sliders.
    m_simplifierNeighborBorder(6),
    m_frameProcessorType(kNone),
    m_backgroundBrush(GetDialogBackgroundColor())
{
    //  COM must be initialized and Media Foundation started for camera capture to work.

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) throw hr;
    hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) throw hr;

    m_displaySize.cx = 840;
    m_displaySize.cy = 700;

    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

    // Create a latest image with an empty small image in it. This allows us to remove checks
    // for null bitmaps within the main rendering loop.

    m_latestImage = std::make_shared<ImageInfo>(kFirstImage, L"No image", nullptr);
    m_pipelinePerformance.Start();
}

void CartoonizerDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);

    CString str(L"");
    if (!m_imageName.empty())
        str.Append(&m_imageName[0], static_cast<int>(m_imageName.size()));
    DDX_Text(pDX, IDC_EDIT_IMAGENAME, str);
    DDX_TextFormatted(pDX, IDC_EDIT_LOADTIME, m_pipelinePerformance.GetAveragePhaseTime(kLoad));
    DDX_TextFormatted(pDX, IDC_EDIT_FILTERTIME, m_pipelinePerformance.GetAveragePhaseTime(kCartoonize));
    DDX_TextFormatted(pDX, IDC_EDIT_RESIZETIME, m_pipelinePerformance.GetAveragePhaseTime(kResize));
    DDX_TextFormatted(pDX, IDC_EDIT_DISPLAYTIME, m_pipelinePerformance.GetAveragePhaseTime(kDisplay));
    DDX_TextFormatted(pDX, IDC_EDIT_TIMEPERIMAGE, m_pipelinePerformance.GetTimePerImage());

    // More pipeline performance data is available but in this sample it is not displayed in the UI.

    DDX_CBIndex(pDX, IDC_COMBO_INPUT, m_currentSource);
    if (pDX->m_bSaveAndValidate == DDXReadData)
    {
        CComboBox* pDropDown =(CComboBox*)GetDlgItem(IDC_COMBO_ENGINE);
        m_frameProcessorType = (FrameProcessorType)pDropDown->GetItemData(pDropDown->GetCurSel());
    }
    DDX_Slider(pDX, IDC_SLIDER_PHASES, m_simplifierPhases);
    DDX_TextFormatted(pDX, IDC_EDIT_IMAGEPHASES, m_simplifierPhases, L"%d");
    DDX_Slider(pDX, IDC_SLIDER_NEIGHBORWINDOW, m_simplifierNeighborBorder);
    DDX_TextFormatted(pDX, IDC_EDIT_NEIGHBORWINDOW, m_simplifierNeighborBorder, L"%d");
}

#pragma region Message Map

BEGIN_MESSAGE_MAP(CartoonizerDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_BN_CLICKED(IDC_BUTTON_START, &CartoonizerDlg::OnBnClickedButtonStart)
    ON_BN_CLICKED(IDC_BUTTON_STOP, &CartoonizerDlg::OnBnClickedButtonStop)
    ON_BN_CLICKED(IDCANCEL, &CartoonizerDlg::OnBnClickedCancel)
    ON_MESSAGE(WM_REPORTERROR, &CartoonizerDlg::OnReportError)
    ON_MESSAGE(WM_UPDATEWINDOW, &CartoonizerDlg::OnUpdateWindow)
    ON_MESSAGE(WM_HSCROLL, &CartoonizerDlg::OnHScroll)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_BN_CLICKED(IDC_BUTTON_LOADNEXT, &CartoonizerDlg::OnBnClickedButtonLoadNext)
    ON_BN_CLICKED(IDC_BUTTON_CARTOONIZE, &CartoonizerDlg::OnBnClickedButtonCartoonize)
    ON_BN_CLICKED(IDC_BUTTON_RELOAD, &CartoonizerDlg::OnBnClickedButtonReload)
    ON_CBN_SELCHANGE(IDC_COMBO_INPUT, &CartoonizerDlg::OnCbnSelchangeComboInput)
    ON_WM_DEVICECHANGE()
END_MESSAGE_MAP()

#pragma endregion

//--------------------------------------------------------------------------------------
//  Initialize the dialog
//--------------------------------------------------------------------------------------

BOOL CartoonizerDlg::OnInitDialog()
{
    // NOTE: This application leaks memory on shutdown due DLL unload ordering issues.
    //
    // {250} normal block at 0x000000953A400980, 152 bytes long.
    //  Data: <  ?:    0M<=    > 10 D0 3F 3A 95 00 00 00 30 4D 3C 3D 95 00 00 00 

    CDialogEx::OnInitDialog();
    SetIcon(m_hIcon, true);
    SetIcon(m_hIcon, false);

    m_consoleHeight = (IsWindows8()) ? m_consoleHeightWin8 : m_consoleHeightWin7;

    bool hasCameras = ConfigureSources();
    AmpUtils::DebugListAccelerators(AmpUtils::GetAccelerators(true));

    //  Dynamically populate the dropdown at runtime to select the best C++ AMP integrator for the available hardware.

    CComboBox* pDropDown =(CComboBox*)GetDlgItem(IDC_COMBO_ENGINE);

    // The ordering of these names must match the FrameProcessorType enumeration.

    std::wstring processorNames[] =
    {
         std::wstring(L" CPU Single Core"),                             // kCpuSingle
         std::wstring(L" CPU Multi-core"),                              // kCpuMulti
         std::wstring(L" C++ AMP Simple Model: "),                      // kAmpSimple
         std::wstring(L" C++ AMP Tiled Model: "),                       // kAmpTiled
         std::wstring(L" C++ AMP Textures: "),                          // kAmpTexture
         std::wstring(L" C++ AMP Simple Model: WARP"),                  // kAmpWarpSimple
         std::wstring(L" C++ AMP Tiled Model: WARP"),                   // kAmpWarpTiled
         std::wstring(L" C++ AMP Simple Model: xx GPUs block split"),   // kAmpMultiSimple
         std::wstring(L" C++ AMP Tiled Model:  xx GPUs block split"),   // kAmpMultiTiled
         std::wstring(L" C++ AMP Simple Model: xx GPUs forked"),        // kAmpSimplePipeline
         std::wstring(L" C++ AMP Tiled Model:  xx GPUs forked"),        // kAmpTiledPipeline
         std::wstring(L" C++ AMP Textures:     xx GPUs forked"),        // kAmpTexturePipeline
    };

    //  Fix up processor names.

    WCHAR buf[3];
    if (_itow_s(static_cast<int>(AmpUtils::GetAccelerators().size()), buf, 3, 10) == 0)
        for (int i = kAmpMultiSimple; i <= kAmpTexturePipeline; ++i)
            processorNames[i].replace(23, 2, buf);
    std::wstring path = accelerator(accelerator::default_accelerator).device_path;
    if (path.compare(accelerator::direct3d_ref) == 0)
        path = L"REF";
    else if (path.compare(accelerator::direct3d_warp) == 0)
        path = L"WARP";
    else 
        path =L"single GPU";
    processorNames[kAmpSimple].append(path);
    processorNames[kAmpTiled].append(path);
    processorNames[kAmpTexture].append(path);

    //  Always enable CPU frame processors.

    AddComboItem(pDropDown, processorNames, kCpuSingle);
    AddComboItem(pDropDown, processorNames, kCpuMulti);

    //  If there is a GPU or WARP accelerator then use it. Otherwise add a REF accelerator and display warning.

    AddComboItem(pDropDown, processorNames, kAmpSimple);
    pDropDown->SetCurSel(AddComboItem(pDropDown, processorNames, kAmpTiled));
    AddComboItem(pDropDown, processorNames, kAmpTexture);

#ifndef _DEBUG
    if (AmpUtils::GetAccelerators().empty())
    {
        MessageBox(L"No C++ AMP hardware accelerator detected,\nusing the REF accelerator.\n\nTo see better performance run on C++ AMP\ncapable hardware.", 
            L"No C++ AMP Hardware Accelerator Detected", 
            MB_ICONEXCLAMATION);
    }
#endif

    //  If the default accelerator isn't WARP (because a GPU is available) give the user the option to run WARP. Don't select it as the default.

    if (AmpUtils::HasAccelerator(accelerator::direct3d_warp) && accelerator(accelerator::default_accelerator).device_path.compare(accelerator::direct3d_warp) != 0)
    {
        AddComboItem(pDropDown, processorNames, kAmpWarpSimple);
        AddComboItem(pDropDown, processorNames, kAmpWarpTiled);
    }

    //  If there us more than one GPU then allow the user to use them together.

    if (AmpUtils::GetAccelerators().size() >= 2)
    {
        AddComboItem(pDropDown, processorNames, kAmpMultiSimple);
        pDropDown->SetCurSel(AddComboItem(pDropDown, processorNames, kAmpMultiTiled));
        AddComboItem(pDropDown, processorNames, kAmpSimplePipeline);
        pDropDown->SetCurSel(AddComboItem(pDropDown, processorNames, kAmpTiledPipeline));
        AddComboItem(pDropDown, processorNames, kAmpTexturePipeline);
    }

    //  Dynamically set slider ranges.

    CSliderCtrl* pSlider = (CSliderCtrl*)GetDlgItem(IDC_SLIDER_PHASES);
    pSlider->SetRange(1, FrameProcessorAmp::MaxSimplifierPhases, true);
    pSlider = (CSliderCtrl*) GetDlgItem(IDC_SLIDER_NEIGHBORWINDOW);
    pSlider->SetRange(1, FrameProcessorAmp::MaxNeighborWindow / 2, true);

    if (m_inputSources.empty())
    {
        MessageBox(L"No .JPG or .JPEG images files found in the application folder.\nNo camera detected.", L"No Input Sources Found.", MB_ICONERROR);
        OnBnClickedCancel();
        return false;
    }
    if (m_filePaths.empty())
        MessageBox(L"No .JPG or .JPEG images files found in the application folder.\nThe image cartoonizer feature will not be available.", L"No JPEG Images Found.", MB_ICONEXCLAMATION);

    if (!hasCameras)
        MessageBox(L"No camera detected.\nThe video cartoonizer feature will not be available.", L"No Camera Detected.", MB_ICONEXCLAMATION);

    SetButtonState(kPipelineStopped);
    return true;
}

bool CartoonizerDlg::ConfigureSources()
{
    m_inputSources.clear();

    // Find all the images. Warn the user if there are none.

    m_filePaths = FileUtils::ListFilesInApplicationDirectory(L"jpg");
    std::vector<std::wstring> jpegs = FileUtils::ListFilesInApplicationDirectory(L"jpeg");
    std::copy(jpegs.begin(), jpegs.end(), std::back_inserter(m_filePaths));
    std::sort(m_filePaths.begin(), m_filePaths.end());
    if (!m_filePaths.empty())
        m_inputSources.push_back(VideoSource(L"Images from folder"));

    //  Configure video capture, search for available camera and add them to the dropdown.

    std::vector<VideoSource> cameras = VideoSource::GetVideoSources();
    m_inputSources.insert(m_inputSources.end(), cameras.begin(), cameras.end());

    CComboBox* pDropDown = (CComboBox*)GetDlgItem(IDC_COMBO_INPUT);
    pDropDown->ResetContent();
    pDropDown->Clear();
    std::for_each(m_inputSources.cbegin(), m_inputSources.cend(), [&pDropDown](const VideoSource& d) 
    {
        std::wstring name = L" " + d.Name;
        pDropDown->AddString(name.c_str());
    });
    m_currentSource = 0;
    pDropDown->SetCurSel(m_currentSource);

    return !cameras.empty();
}

HCURSOR CartoonizerDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}

//--------------------------------------------------------------------------------------
//  Redraw the latest image and update the performance data.
//--------------------------------------------------------------------------------------

void CartoonizerDlg::OnPaint()
{
    // Use the latest image from the agent or a cached image if no agent is active.

    ImageInfoPtr pInfo = (nullptr != m_pipeline) ? m_pipeline->GetCurrentImage() : m_latestImage;
    if (pInfo == nullptr)
    {
        CDialogEx::OnPaint();
        return;
    }

    //  if the image is larger than the available screen area then crop it.
    //  After image resize all new images added to the pipeline will be resized 
    //  correctly but existing images in the pipeline may be too large. Later images will
    //  be the correct size.

    CPaintDC dc(this);
    BITMAP bm;
    HDC hdcMem = ::CreateCompatibleDC(dc);
    HBITMAP hb;
    pInfo->GetBitmapPtr()->GetHBITMAP(Gdiplus::Color(0,0,0), &hb);
    HBITMAP hbmOld = (HBITMAP)::SelectObject(hdcMem, hb);
    ::GetObject(hb, sizeof(bm), &bm);

    Gdiplus::Graphics graphics(dc);

    if ((m_previousBitmapSize.cy != bm.bmHeight) || (m_previousBitmapSize.cx = bm.bmWidth))
        graphics.FillRectangle(&m_backgroundBrush, m_imageLeft, m_imageTop, m_displaySize.cx, m_displaySize.cy);
    const int xOffset = m_imageLeft + m_simplifierNeighborBorder;
    const int yOffset = m_imageTop + m_simplifierNeighborBorder;
    BitBlt(dc, xOffset, yOffset, 
        std::min(bm.bmWidth, m_displaySize.cx) - m_simplifierNeighborBorder * 2, std::min(bm.bmHeight, m_displaySize.cy) - m_simplifierNeighborBorder * 2, 
        hdcMem, m_simplifierNeighborBorder, m_simplifierNeighborBorder, SRCCOPY);
    m_previousBitmapSize.cy = bm.bmHeight;
    m_previousBitmapSize.cx = bm.bmWidth;

    ::SelectObject(hdcMem, hbmOld);
    ::DeleteDC(hdcMem);
    ::DeleteObject(hb);

    m_imageName = pInfo->GetName();
    m_currentImagePerformance = pInfo->GetPerformanceData();

    //// The display phase actually ends here. Right before updating the pipeline's performance
    m_currentImagePerformance.SetEndTick(kDisplay);
    if (IsPipelineRunning() || m_singleImageMode)
    {
        m_pipelinePerformance.Update(m_currentImagePerformance);
        m_singleImageMode = false;
    }
    UpdateData(DDXReadData);
    UpdateData(DDXWriteData);
    CDialogEx::OnPaint();
}

//  Helper method to get the dialog's background color for painting out the previous image.

Gdiplus::Color CartoonizerDlg::GetDialogBackgroundColor()
{
    HBRUSH hbr = GetSysColorBrush(COLOR_BTNFACE);
    LOGBRUSH logBrush;
    GetObject(hbr, sizeof(LOGBRUSH), &logBrush);
    return Gdiplus::Color(255, GetRValue(logBrush.lbColor), GetBValue(logBrush.lbColor), GetGValue(logBrush.lbColor));
}

//--------------------------------------------------------------------------------------
//  Custom message handlers.
//--------------------------------------------------------------------------------------
//
//  Pipeline configuration has changed, update UI.

LRESULT CartoonizerDlg::OnHScroll(WPARAM wParam, LPARAM lParam)
{
    UpdateData(DDXReadData);
    UpdateData(DDXWriteData);
    return 0L;
}

//  Pipeline has reported an error. Process the error message.

LRESULT CartoonizerDlg::OnReportError(WPARAM wParam, LPARAM lParam)
{
    ErrorInfo err = receive(m_errorMessages);
    ReportError(err);
    return 0L;
}

//  Pipeline has finished processing an image. Force a repaint.

LRESULT CartoonizerDlg::OnUpdateWindow(WPARAM wParam, LPARAM lParam)
{   
    AfxGetMainWnd()->Invalidate(false);
    return 0L;
}

//  Pipeline source changed. Update the rest of the UI (enable/disable buttons).

void CartoonizerDlg::OnCbnSelchangeComboInput()
{
    SetButtonState(kPipelineStopped);
}

//  Device has been added/removed. Update devices and warn user if appropriate.
//
//  This is a minimal implementation of this functionality.

BOOL CartoonizerDlg::OnDeviceChange(UINT nEventType, DWORD_PTR dwData)
{
    ATLTRACE("Device change %u\n", nEventType);

    // If the video pipeline isn't running then just refresh the dropdown.

    if ((m_pipeline != nullptr) && !IsPictureSource())
    {
        ConfigureSources();
        CDialogEx::Invalidate();
        return false;
    }

    switch (nEventType)
    {
    case DBT_DEVICEARRIVAL:
        ConfigureSources();
        CDialogEx::Invalidate();
        break;
    case DBT_DEVNODES_CHANGED:
        StopPipeline();
        SetButtonState(kPipelineStopped);
        MessageBox(L"One or more devices have been removed. Please ensure that your camera is connected and restart image capture.", L"Camera Removed", MB_ICONEXCLAMATION);
        ConfigureSources();
        CDialogEx::Invalidate();
        break;
    }
    return false;
}

//--------------------------------------------------------------------------------------
//  Button event handlers.
//--------------------------------------------------------------------------------------

//  Single image mode: Load the next image from disk or capture a video frame.

void CartoonizerDlg::OnBnClickedButtonLoadNext()
{
    assert(m_latestImage != nullptr);

    if (IsPictureSource())
        LoadImage((m_latestImage->GetSequence() + 1) % m_filePaths.size());
    else
        LoadVideoFrame();
}

//  Single image mode: Reload the current image from disk (reloading a video frame not supported).

void CartoonizerDlg::OnBnClickedButtonReload()
{
    assert(m_latestImage != nullptr);
    LoadImage(m_latestImage->GetSequence() % m_filePaths.size());
}

//  Helper functions to do the actual load or capture.

void CartoonizerDlg::LoadImage(int sequence)
{
    assert(!m_filePaths.empty());

    std::wstring filePath = m_filePaths[sequence];

    BitmapPtr img = BitmapUtils::LoadBitmapAndConvert(filePath);
    std::wstring name = FileUtils::GetFilenameFromPath(filePath);
    m_latestImage = std::make_shared<ImageInfo>(sequence, name, img.get());
    RECT correctedSize = ImageUtils::CorrectResize(m_latestImage->GetSize(), m_displaySize);
    m_latestImage->ResizeImage(correctedSize);
    SetButtonState(kPipelineStopped);
    CDialogEx::Invalidate();
}

void CartoonizerDlg::LoadVideoFrame()
{
    VideoFormatConverter converter;
    CComPtr<IMFActivate> source = GetInputSource().Source;
    VideoReader camera(&converter);
    camera.SetDevice(source);
    BitmapPtr frame = camera.CaptureFrame();  
    m_latestImage = std::make_shared<ImageInfo>(kFirstImage, L"Camera frame", frame.get());
    RECT correctedSize = ImageUtils::CorrectResize(m_latestImage->GetSize(), GetImageSize(), converter.GetAspectRatio());
    m_latestImage->ResizeImage(correctedSize);
    SetButtonState(kPipelineStopped);
    CDialogEx::Invalidate();
}

//  Single image mode: Create a pipeline and pass it the latest captured image.
//
//  ImagePipeline will process the image and then immediately shut itself down.

void CartoonizerDlg::OnBnClickedButtonCartoonize()
{
    assert(!m_filePaths.empty());
    assert(m_latestImage != nullptr);

    StopPipeline();
    UpdateData(DDXReadData);
    SetButtonState(kPipelineCartoonize);

    std::shared_ptr<IFrameReader> reader;
    if (IsPictureSource())
        reader = std::make_shared<ImageSingleFileReader>(FileUtils::GetApplicationDirectory(), m_latestImage);
    else
        reader = std::make_shared<VideoStreamReader>(GetInputSource().Source, true);

    // It makes no sense to send a single image through the multiplexed pipeline so use the equivalent single w/o the multiplexor.
    FrameProcessorType pipelineType = (m_frameProcessorType < kAmpPipeline) ?  m_frameProcessorType : FrameProcessorType(m_frameProcessorType - 7);
    m_pipeline = std::unique_ptr<ImagePipeline>(new ImagePipeline(this, reader, pipelineType, 1, m_cancelMessage, m_errorMessages));

    m_pipelinePerformance = PipelinePerformanceData(m_pipeline->GetCartoonizerProcessorCount());
    m_pipeline->start();
    m_pipelinePerformance.Start();
    agent::wait(m_pipeline.get());
    m_singleImageMode = true;

    SetButtonState(kPipelineStopped);
    m_latestImage = m_pipeline->GetCurrentImage();
    m_pipeline = nullptr;

    UpdateData(DDXWriteData);
    CDialogEx::Invalidate();
}

//  Sequenced image/video mode: Start a pipeline for the currently selected input source.
//
//  The pipelines will continue to process images until shut down by the UI.

void CartoonizerDlg::OnBnClickedButtonStart()
{
    UpdateData(DDXReadData);
    StopPipeline();

    std::shared_ptr<IFrameReader> reader;
    if (IsPictureSource())
        reader = std::make_shared<ImageFileFolderReader>(FileUtils::GetApplicationDirectory());
    else
        reader = std::make_shared<VideoStreamReader>(GetInputSource().Source);   

    m_pipeline = std::unique_ptr<ImagePipeline>(
        new ImagePipeline(this, reader, m_frameProcessorType, 
            kPipelineCapacity, m_cancelMessage, m_errorMessages));
    m_pipelinePerformance = PipelinePerformanceData(m_pipeline->GetCartoonizerProcessorCount());
    m_pipeline->start();
    m_pipelinePerformance.Start();
    SetButtonState(kPipelineRunning);
}

//  Sequenced image/video mode: Stop the pipeline.

void CartoonizerDlg::OnBnClickedButtonStop()
{
    StopPipeline();
    Invalidate(false);
}

// Shut down application.

void CartoonizerDlg::OnBnClickedCancel()
{
    StopPipeline();
    CDialogEx::OnCancel();
}

//--------------------------------------------------------------------------------------
//  Stop the pipeline. Send a cancel message and wait for the pipeline to terminate.
//--------------------------------------------------------------------------------------

void CartoonizerDlg::StopPipeline()
{
    if (nullptr == m_pipeline)
    {
        send(m_cancelMessage, false);
        return;
    }
    send(m_cancelMessage, true);
    agent::wait(m_pipeline.get());

    m_latestImage = m_pipeline->GetCurrentImage();
    m_pipeline = nullptr;
    SetButtonState(kPipelineStopped);

    // Reset cancel signal.
    send(m_cancelMessage, false);
}

//--------------------------------------------------------------------------------------
//  Set the button enabled states according to whether the pipeline is running.
//--------------------------------------------------------------------------------------

void CartoonizerDlg::SetButtonState(PipelineState state)
{
    UpdateData(DDXReadData);

    bool isEnabled = PicturesEnabled() || VideoEnabled();

    GetDlgItem(IDC_BUTTON_LOADNEXT)->EnableWindow(state == kPipelineStopped);
    GetDlgItem(IDC_BUTTON_RELOAD)->EnableWindow((state == kPipelineStopped) && IsPictureSource());
    GetDlgItem(IDC_BUTTON_CARTOONIZE)->EnableWindow((state == kPipelineStopped) && ((m_latestImage != nullptr) && !m_latestImage->IsEmpty()));
    GetDlgItem(IDC_BUTTON_START)->EnableWindow((state == kPipelineStopped) && isEnabled);
    GetDlgItem(IDC_BUTTON_STOP)->EnableWindow((state == kPipelineRunning) && isEnabled);
    GetDlgItem(IDC_COMBO_INPUT)->EnableWindow((state == kPipelineStopped) && isEnabled);
    GetDlgItem(IDC_COMBO_ENGINE)->EnableWindow(state == kPipelineStopped);
    GetDlgItem(IDC_SLIDER_PHASES)->EnableWindow(state == kPipelineStopped);
    GetDlgItem(IDC_SLIDER_NEIGHBORWINDOW)->EnableWindow(state == kPipelineStopped);
}

//--------------------------------------------------------------------------------------
//  Dialog resizing
//--------------------------------------------------------------------------------------

//  Fix the minimum dialog size.

void CartoonizerDlg::OnGetMinMaxInfo(MINMAXINFO FAR* lpMMI)
{
    CRect rect(0, 0, m_consoleWidth * 2, m_consoleHeight);
    CalcWindowRect(rect);

    lpMMI->ptMinTrackSize.x = rect.Width();
    lpMMI->ptMinTrackSize.y = rect.Height();
}

//  Reposition the controls on resize.

void CartoonizerDlg::OnSize(UINT nType, int cx, int cy)
{
    __super::OnSize(nType, cx, cy);

    m_displaySize.cx = cx - m_consoleWidth - m_imageLeft;
    m_displaySize.cy = cy - (m_imageTop * 2);

    const int rightBorder = 10;
    const int rightEdge = rightBorder + 10;
    const int leftBorder = m_consoleWidth - 10;
    const int leftEdge = leftBorder - 10;
    const int center = rightBorder + (leftBorder - rightBorder) / 2;

    ResizeControl(IDC_STATIC_TPP, cx, cy, leftBorder, rightBorder);
    ResizeControl(IDC_STATIC_LOAD, cx, cy, leftEdge, rightEdge + 80);
    ResizeControl(IDC_EDIT_LOADTIME, cx, cy, rightEdge + 70, rightEdge);
    ResizeControl(IDC_STATIC_RESIZE, cx, cy, leftEdge, rightEdge + 80);
    ResizeControl(IDC_EDIT_RESIZETIME, cx, cy, rightEdge + 70, rightEdge);
    ResizeControl(IDC_STATIC_FILTER, cx, cy, leftEdge, rightEdge + 80);
    ResizeControl(IDC_EDIT_FILTERTIME, cx, cy, rightEdge + 70, rightEdge);
    ResizeControl(IDC_STATIC_DISPLAY, cx, cy, leftEdge, rightEdge + 80);
    ResizeControl(IDC_EDIT_DISPLAYTIME, cx, cy, rightEdge + 70, rightEdge);

    ResizeControl(IDC_STATIC_IMG, cx, cy, leftBorder, rightBorder);
    ResizeControl(IDC_STATIC_TPI, cx, cy, leftEdge, rightEdge + 80);
    ResizeControl(IDC_EDIT_TIMEPERIMAGE, cx, cy, rightEdge + 70, rightEdge);
    ResizeControl(IDC_EDIT_IMAGENAME, cx, cy, leftEdge, rightEdge);

    ResizeControl(IDC_STATIC_IPS, cx, cy, leftBorder, rightBorder);
    ResizeControl(IDC_STATIC_IS, cx, cy, leftEdge, rightEdge + 20);
    ResizeControl(IDC_COMBO_INPUT, cx, cy, leftEdge, rightEdge);
    ResizeControl(IDC_STATIC_FP, cx, cy, leftEdge, rightEdge + 20);
    ResizeControl(IDC_COMBO_ENGINE, cx, cy, leftEdge, rightEdge);
    ResizeControl(IDC_STATIC_CSP, cx, cy, leftEdge, rightEdge + 20);
    ResizeControl(IDC_SLIDER_PHASES, cx, cy, leftBorder, rightEdge + 30);
    ResizeControl(IDC_EDIT_IMAGEPHASES, cx, cy, rightEdge + 25, rightEdge);
    ResizeControl(IDC_STATIC_CSBW, cx, cy, leftEdge, rightEdge);
    ResizeControl(IDC_SLIDER_NEIGHBORWINDOW, cx, cy, leftBorder, 60);
    ResizeControl(IDC_EDIT_NEIGHBORWINDOW, cx, cy, rightEdge + 25, rightEdge);

    ResizeControl(IDC_STATIC_SI, cx, cy, leftBorder, rightBorder);
    ResizeControl(IDC_BUTTON_LOADNEXT, cx, cy, leftEdge, center + 5);
    ResizeControl(IDC_BUTTON_RELOAD, cx, cy, center - 5, rightEdge);
    ResizeControl(IDC_BUTTON_CARTOONIZE, cx, cy, leftEdge, center + 5);

    ResizeControl(IDC_STATIC_MI, cx, cy, leftBorder, rightBorder);
    ResizeControl(IDC_BUTTON_START, cx, cy, leftEdge, center + 5);
    ResizeControl(IDC_BUTTON_STOP, cx, cy, center - 5, rightEdge);

    Invalidate();
}

//  Helper method to reposition an individual control.

void CartoonizerDlg::ResizeControl(UINT nControlID, int cx, int cy, int shiftLeft, int shiftRight)
{
    RECT windowRect; 
    GetClientRect(&windowRect);

    CWnd* controlWindow = GetDlgItem(nControlID); 
    RECT controlRect; 
    controlWindow->GetWindowRect(&controlRect);             //control rectangle
    ScreenToClient(&controlRect);                           //control rectangle in the coordinate system of the parent

    controlRect.left = cx - shiftLeft;
    controlRect.right = cx - shiftRight; 
    controlWindow->MoveWindow(&controlRect); 
}

//--------------------------------------------------------------------------------------
//  Report error from a phase in the pipeline.
//--------------------------------------------------------------------------------------

void CartoonizerDlg::ReportError(const ErrorInfo& error)
{
    std::array<std::wstring, 5> phaseNames = { L"loading", L"scaling", L"filtering", L"displaying", L"processing" };

    SetButtonState(kPipelineStopped);

    std::wstring message(L"Error while ");
    message.append(phaseNames[static_cast<int>(GetStage(error))]).append(L" image");
    std::wstring imageName = GetImageName(error);
    if (!imageName.empty())
        message.append(L" \"").append(imageName).append(L"\"");
    message.append(L"\n\nException message is \"").append(GetMessage(error)).append(L"\"");

    ATLTRACE("Exception: '%S'\n", GetMessage(error));
    AfxMessageBox(message.c_str(), MB_ICONERROR | MB_OK);
}