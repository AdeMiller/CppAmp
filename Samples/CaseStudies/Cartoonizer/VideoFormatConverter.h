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

#include "GdiWrap.h"
#include <windef.h>
#include <mfapi.h>
#include <mferror.h>

// Function pointer for the function that transforms the image.

typedef void (*ImageTransformFunc)(
    BYTE*       pDest,
    LONG        lDestStride,
    const BYTE* pSrc,
    LONG        lSrcStride,
    DWORD       dwWidthInPixels,
    DWORD       dwHeightInPixels
    );

class VideoFormatConverter
{
private:

    // Format information
    UINT                    m_width;
    UINT                    m_height;
    LONG                    m_lDefaultStride;
    MFRatio                 m_PixelAR;
    MFVideoInterlaceMode    m_interlace;
    RECT                    m_rcDest;       // Destination rectangle

    // Drawing
    ImageTransformFunc      m_convertFn;    // Function to convert the video to RGB32

private:
    HRESULT SetConversionFunction(REFGUID subtype);
    void    UpdateDestinationRect();
    
public:
    VideoFormatConverter();

    HRESULT SetVideoType(IMFMediaType *pType);
    HRESULT ConvertFrame(IMFMediaBuffer *pBuffer, Gdiplus::Bitmap& bitmap);

    // What video formats we accept
    BOOL     IsFormatSupported(REFGUID subtype) const;
    HRESULT  GetFormat(DWORD index, GUID *pSubtype)  const;

    inline UINT GetWidth() const { return m_width; }
    inline UINT GetHeight() const { return m_height; }
    inline MFRatio GetAspectRatio() const { return m_PixelAR; }
};
