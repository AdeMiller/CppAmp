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

#include <assert.h>
#include <afxcoll.h>
#include "GdiWrap.h"
#include <windef.h>
#include <d3d9types.h>

#include "VideoSource.h"
#include "VideoFormatConverter.h"
#include "VideoBufferLock.h"

//-------------------------------------------------------------------
// RGB-32 to RGB-32
//
// Note: This function is needed to copy the image from system
// memory to the Direct3D surface.
//-------------------------------------------------------------------

void TransformImage_RGB32(BYTE* const pDest, LONG destStride, const BYTE* const pSrc, LONG srcStride,
    DWORD widthInPixels, DWORD heightInPixels)
{
    MFCopyImage(pDest, destStride, pSrc, srcStride, widthInPixels * 4, heightInPixels);
}

//-------------------------------------------------------------------
// RGB-24 to RGB-32 
//-------------------------------------------------------------------

void TransformImage_RGB24(BYTE* pDest, LONG destStride, const BYTE* pSrc, LONG srcStride,
    DWORD widthInPixels, DWORD heightInPixels)
{
    for (DWORD y = 0; y < heightInPixels; y++)
    {
        RGBTRIPLE *pSrcPel = (RGBTRIPLE*)pSrc;
        DWORD *pDestPel = (DWORD*)pDest;

        for (DWORD x = 0; x < widthInPixels; x++)
        {
            pDestPel[x] = D3DCOLOR_XRGB(
                pSrcPel[x].rgbtRed,
                pSrcPel[x].rgbtGreen,
                pSrcPel[x].rgbtBlue
                );
        }

        pSrc += srcStride;
        pDest += destStride;
    }

}

//-------------------------------------------------------------------
// YUY2 to RGB-32
//-------------------------------------------------------------------

__forceinline BYTE Clip(int clr)
{
    return (BYTE)(clr < 0 ? 0 : ( clr > 255 ? 255 : clr ));
}

__forceinline RGBQUAD ConvertYCrCbToRGB(int y, int cr, int cb)
{
    RGBQUAD rgb;

    int c = y - 16;
    int d = cb - 128;
    int e = cr - 128;

    rgb.rgbRed =   Clip(( 298 * c           + 409 * e + 128) >> 8);
    rgb.rgbGreen = Clip(( 298 * c - 100 * d - 208 * e + 128) >> 8);
    rgb.rgbBlue =  Clip(( 298 * c + 516 * d           + 128) >> 8);
    rgb.rgbReserved = 0xFF;
    return rgb;
}

void TransformImage_YUY2(BYTE* pDest, LONG destStride, const BYTE* pSrc, LONG srcStride,
    DWORD widthInPixels, DWORD heightInPixels)
{
    for (DWORD y = 0; y < heightInPixels; y++)
    {
        RGBQUAD *pDestPel = (RGBQUAD*)pDest;
        WORD    *pSrcPel = (WORD*)pSrc;

        for (DWORD x = 0; x < widthInPixels; x += 2)
        {
            // Byte order is U0 Y0 V0 Y1

            int y0 = (int)LOBYTE(pSrcPel[x]);
            int u0 = (int)HIBYTE(pSrcPel[x]);
            int y1 = (int)LOBYTE(pSrcPel[x + 1]);
            int v0 = (int)HIBYTE(pSrcPel[x + 1]);

            pDestPel[x] = ConvertYCrCbToRGB(y0, v0, u0);
            pDestPel[x + 1] = ConvertYCrCbToRGB(y1, v0, u0);
        }

        pSrc += srcStride;
        pDest += destStride;
    }
}

//-------------------------------------------------------------------
// NV12 to RGB-32
//-------------------------------------------------------------------

void TransformImage_NV12(BYTE* pDest, LONG destStride, const BYTE* const pSrc, LONG srcStride,
    DWORD widthInPixels, DWORD heightInPixels)
{
    const BYTE* lpBitsY = pSrc;
    const BYTE* lpBitsCb = lpBitsY  + (heightInPixels * srcStride);;
    const BYTE* lpBitsCr = lpBitsCb + 1;

    for (UINT y = 0; y < heightInPixels; y += 2)
    {
        const BYTE* lpLineY1 = lpBitsY;
        const BYTE* lpLineY2 = lpBitsY + srcStride;
        const BYTE* lpLineCr = lpBitsCr;
        const BYTE* lpLineCb = lpBitsCb;

        LPBYTE lpDibLine1 = pDest;
        LPBYTE lpDibLine2 = pDest + destStride;

        for (UINT x = 0; x < widthInPixels; x += 2)
        {
            int  y0 = (int)lpLineY1[0];
            int  y1 = (int)lpLineY1[1];
            int  y2 = (int)lpLineY2[0];
            int  y3 = (int)lpLineY2[1];
            int  cb = (int)lpLineCb[0];
            int  cr = (int)lpLineCr[0];

            RGBQUAD r = ConvertYCrCbToRGB(y0, cr, cb);
            lpDibLine1[0] = r.rgbBlue;
            lpDibLine1[1] = r.rgbGreen;
            lpDibLine1[2] = r.rgbRed;
            lpDibLine1[3] = 0; // Alpha

            r = ConvertYCrCbToRGB(y1, cr, cb);
            lpDibLine1[4] = r.rgbBlue;
            lpDibLine1[5] = r.rgbGreen;
            lpDibLine1[6] = r.rgbRed;
            lpDibLine1[7] = 0; // Alpha

            r = ConvertYCrCbToRGB(y2, cr, cb);
            lpDibLine2[0] = r.rgbBlue;
            lpDibLine2[1] = r.rgbGreen;
            lpDibLine2[2] = r.rgbRed;
            lpDibLine2[3] = 0; // Alpha

            r = ConvertYCrCbToRGB(y3, cr, cb);
            lpDibLine2[4] = r.rgbBlue;
            lpDibLine2[5] = r.rgbGreen;
            lpDibLine2[6] = r.rgbRed;
            lpDibLine2[7] = 0; // Alpha

            lpLineY1 += 2;
            lpLineY2 += 2;
            lpLineCr += 2;
            lpLineCb += 2;

            lpDibLine1 += 8;
            lpDibLine2 += 8;
        }

        pDest += (2 * destStride);
        lpBitsY   += (2 * srcStride);
        lpBitsCr  += srcStride;
        lpBitsCb  += srcStride;
    }
}


std::wstring StringFromGUID(const GUID& guid)
{
    WCHAR* str;
    HRESULT hr = StringFromCLSID(guid, &str);
    std::wstring wstr(str);
    CoTaskMemFree(str);
    return wstr;
}

HRESULT GetDefaultStride(IMFMediaType *pType, LONG *plStride);

// Static table of output formats and conversion functions.

struct ConversionFunction
{
    GUID               subtype;
    ImageTransformFunc xform;
    WCHAR*             name;
};

ConversionFunction   g_FormatConversions[] =
{
    { MFVideoFormat_RGB32, TransformImage_RGB32, L"RGB32 to RGB32" },
    { MFVideoFormat_YUY2,  TransformImage_YUY2,  L"YUY2 to RGB32" },      
    { MFVideoFormat_NV12,  TransformImage_NV12,  L"NV12 to RGB32" },
    { MFVideoFormat_RGB24, TransformImage_RGB24, L"RGB24 to RGB32" }
};

const DWORD   g_cFormats = ARRAYSIZE(g_FormatConversions);

//-------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------

VideoFormatConverter::VideoFormatConverter() :
    m_width(0),
    m_height(0),
    m_lDefaultStride(0),
    m_interlace(MFVideoInterlace_Unknown),
    m_convertFn(nullptr)
{
    m_PixelAR.Denominator = m_PixelAR.Numerator = 0; 
}

//-------------------------------------------------------------------
// Get a supported output format by index.
//-------------------------------------------------------------------

HRESULT VideoFormatConverter::GetFormat(DWORD index, GUID *pSubtype) const
{
    if (index < g_cFormats)
    {
        *pSubtype = g_FormatConversions[index].subtype;
        return S_OK;
    }
    return MF_E_NO_MORE_TYPES;
}

//-------------------------------------------------------------------
//  Query if a format is supported.
//-------------------------------------------------------------------

BOOL VideoFormatConverter::IsFormatSupported(REFGUID subtype) const
{
    ATLTRACE("Checking for native camera format support:\n");
    for (DWORD i = 0; i < g_cFormats; i++)
    {
        if (subtype == g_FormatConversions[i].subtype)
        {
            ATLTRACE("  Media type supported: %S %S\n", StringFromGUID(subtype).c_str(), g_FormatConversions[i].name);
            return TRUE;
        }
    }
    ATLTRACE("  Media type not supported: %S\n", StringFromGUID(subtype).c_str());
    return FALSE;
}

//-------------------------------------------------------------------
// SetConversionFunction
//
// Set the conversion function for the specified video format.
//-------------------------------------------------------------------

HRESULT VideoFormatConverter::SetConversionFunction(REFGUID subtype)
{
    m_convertFn = nullptr;

    for (DWORD i = 0; i < g_cFormats; i++)
    {
        ATLTRACE("Checking media type conversion: %S\n", StringFromGUID(g_FormatConversions[i].subtype).c_str());
        if (g_FormatConversions[i].subtype == subtype)
        {
            ATLTRACE("  Media type conversion supported: %s %s\n", StringFromGUID(subtype).c_str(), g_FormatConversions[i].name );
            m_convertFn = g_FormatConversions[i].xform;
            return S_OK;
        }
    }
    ATLTRACE("  Media type conversion not supported: %S\n", StringFromGUID(subtype).c_str());
    return MF_E_INVALIDMEDIATYPE;
}

//-------------------------------------------------------------------
// Set the video format.  
//-------------------------------------------------------------------

HRESULT VideoFormatConverter::SetVideoType(IMFMediaType *pType)
{
    HRESULT hr = S_OK;
    GUID subtype = { 0 };
    MFRatio PAR = { 0 };

    // Find the video subtype.
    hr = pType->GetGUID(MF_MT_SUBTYPE, &subtype);

    if (FAILED(hr)) { goto done; }

    // Choose a conversion function.
    // (This also validates the format type.)

    hr = SetConversionFunction(subtype); 
    
    if (FAILED(hr)) { goto done; }

    //
    // Get some video attributes.
    //

    // Get the frame size.
    hr = MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &m_width, &m_height);
    
    if (FAILED(hr)) { goto done; }
    TRACE2("Video Frame size is %d x %d\n", m_width, m_height);
    // Get the interlace mode. Default: assume progressive.
    m_interlace = (MFVideoInterlaceMode)MFGetAttributeUINT32(
        pType,
        MF_MT_INTERLACE_MODE, 
        MFVideoInterlace_Progressive
        );

    // Get the image stride.
    hr = GetDefaultStride(pType, &m_lDefaultStride);

    if (FAILED(hr)) { goto done; }

    // Get the pixel aspect ratio. Default: Assume square pixels (1:1)
    hr = MFGetAttributeRatio(
        pType, 
        MF_MT_PIXEL_ASPECT_RATIO, 
        (UINT32*)&PAR.Numerator, 
        (UINT32*)&PAR.Denominator
        );

    if (SUCCEEDED(hr))
    {
        m_PixelAR = PAR;
    }
    else
    {
        m_PixelAR.Numerator = m_PixelAR.Denominator = 1;
    }

done:
    if (FAILED(hr))
        m_convertFn = nullptr;

    return hr;
}

//-------------------------------------------------------------------
// Draw the video frame.
//-------------------------------------------------------------------

HRESULT VideoFormatConverter::ConvertFrame(IMFMediaBuffer *pBuffer, Gdiplus::Bitmap& bitmap)
{
    if (m_convertFn == nullptr)
        return MF_E_INVALIDREQUEST;

    HRESULT hr = S_OK;
    BYTE *pbScanline0 = nullptr;
    LONG lStride = 0;

    VideoBufferLock buffer(pBuffer);    // Helper object to lock the video buffer.

    hr = buffer.LockBuffer(m_lDefaultStride, m_height, &pbScanline0, &lStride);

    Gdiplus::Rect rect(0, 0, m_width, m_height);

    Gdiplus::BitmapData data;
    Gdiplus::Status st = bitmap.LockBits(
        &rect,
        Gdiplus::ImageLockModeWrite,
        PixelFormat32bppARGB,
        &data);
    assert(st == Gdiplus::Ok);
    
    m_convertFn(
        (BYTE*)data.Scan0,
        data.Stride,
        pbScanline0,
        lStride,
        m_width,
        m_height
        );

    buffer.UnlockBuffer();
    bitmap.UnlockBits(&data);

    return hr;
}

//-----------------------------------------------------------------------------
// Gets the default stride for a video frame, assuming no extra padding bytes.
//-----------------------------------------------------------------------------

HRESULT GetDefaultStride(IMFMediaType *pType, LONG *plStride)
{
    LONG lStride = 0;

    // Try to get the default stride from the media type.
    HRESULT hr = pType->GetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32*)&lStride);
    if (FAILED(hr))
    {
        // Attribute not set. Try to calculate the default stride.
        GUID subtype = GUID_NULL;

        UINT32 width = 0;
        UINT32 height = 0;

        // Get the subtype and the image size.
        hr = pType->GetGUID(MF_MT_SUBTYPE, &subtype);
        if (SUCCEEDED(hr))
        {
            hr = MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &width, &height);
        }
        if (SUCCEEDED(hr))
        {
            hr = MFGetStrideForBitmapInfoHeader(subtype.Data1, width, &lStride);
        }

        // Set the attribute for later reference.
        if (SUCCEEDED(hr))
        {
            (void)pType->SetUINT32(MF_MT_DEFAULT_STRIDE, UINT32(lStride));
        }
    }

    if (SUCCEEDED(hr))
    {
        *plStride = lStride;
    }
    return hr;
}
