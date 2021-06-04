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

#include <math.h>
#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <assert.h>
#include <io.h>
#include "GdiWrap.h"

#include <amp.h>
#include <amp_math.h>
#include <amp_graphics.h>
#include <mfobjects.h>

#include "RgbPixel.h"

using namespace concurrency::graphics;
using namespace concurrency::fast_math;

typedef std::shared_ptr<Gdiplus::Bitmap> BitmapPtr;

class BitmapUtils
{
public:
    static inline COLORREF GetPixel(const byte* const pFrame, int x, int y, int pitch, int bpp)
    {
        const int width = abs(static_cast<int>(pitch));
        const int bytesPerPixel = bpp / 8;
        const int byteIndex = y * width + x * bytesPerPixel;
        return RGB(pFrame[byteIndex + 2], pFrame[byteIndex + 1], pFrame[byteIndex]);
    }

    static inline void SetPixel(byte* const pFrame, int x, int y, int pitch, int bpp, COLORREF color)
    {
        const int width = abs(static_cast<int>(pitch));
        const int bytesPerPixel = bpp / 8;
        const int byteIndex = y * width + x * bytesPerPixel;
        pFrame[byteIndex + 2] = GetRValue(color);
        pFrame[byteIndex + 1] = GetGValue(color);
        pFrame[byteIndex] = GetBValue(color);
    }

    static void CopyBitmap(Gdiplus::Bitmap* const source, Gdiplus::Bitmap* const destination)
    {
        assert(nullptr != source);
        assert(nullptr != destination);
        assert(destination->GetWidth() == source->GetWidth());
        assert(destination->GetHeight() == source->GetHeight());

        Gdiplus::Rect rect(0, 0, source->GetWidth(), source->GetHeight());
        Gdiplus::BitmapData sourceBitmapData, destBitmapData;

        Gdiplus::Status st = source->LockBits(
            &rect,
            Gdiplus::ImageLockModeRead,
            PixelFormat32bppARGB,
            &sourceBitmapData);
        assert(Gdiplus::Ok == st);

        st = destination->LockBits(
            &rect,
            Gdiplus::ImageLockModeWrite,
            PixelFormat32bppARGB,
            &destBitmapData);
        assert(Gdiplus::Ok == st);

        int size = source->GetHeight() * destBitmapData.Stride;

        int ret = memcpy_s(destBitmapData.Scan0, size, sourceBitmapData.Scan0, size);
        assert(0 == ret);
        source->UnlockBits(&sourceBitmapData);
        destination->UnlockBits(&destBitmapData);
    }

    static BitmapPtr LoadBitmapAndConvert(const std::wstring& filePath)
    {
        std::unique_ptr<Gdiplus::Bitmap> temp = std::unique_ptr<Gdiplus::Bitmap>(new Gdiplus::Bitmap(filePath.c_str()));
        if (temp->GetWidth() == 0 || temp->GetHeight() == 0)
            AfxThrowFileException(CFileException::invalidFile, 0, filePath.c_str());
        return BitmapPtr(temp->Clone(0, 0, temp->GetWidth(), temp->GetHeight(), PixelFormat32bppARGB));
    }
};

class ImageUtils
{
public:
    static const float_3 W;

public:
    static inline void RGBToYUV(COLORREF color, float& y, float& u, float& v)
    {
        RGBToYUV(uint_3(GetRValue(color), GetGValue(color), GetBValue(color)), y, u, v, W);
    }

    template<typename T>
    static inline void RGBToYUV(const T& color, float& y, float& u, float& v,  const float_3& W) restrict(cpu, amp)
    {
        float r = color.r / 255.0f;
        float g = color.g / 255.0f;
        float b = color.b / 255.0f;

        y = W.r * r + W.g * g + W.b * b;
        u = 0.436f * (b - y) / (1 - W.b);
        v = 0.615f * (r - y) / (1 - W.g);
    }

    static inline float GetDistance(COLORREF color1, COLORREF color2)
    {
        return GetDistance(uint_3(GetRValue(color1), GetGValue(color1), GetBValue(color1)), uint_3(GetRValue(color2), GetGValue(color2), GetBValue(color2)), W);
    }

    template<typename T>
    static inline float GetDistance(const T& color1, const T& color2,  const float_3& W) restrict(cpu, amp)
    {
        float y1, u1, v1, y2, u2, v2;
        RGBToYUV(color1, y1, u1, v1, W);
        RGBToYUV(color2, y2, u2, v2, W);
        const float du = u1 - u2;
        const float dv = v1 - v2;
        return sqrt((du * du) + (dv * dv));
    }

    //  Implementation of direct3d::smoothstep for use by CPU based image processor.
    static inline float SmoothStep(float a, float b, float x)
    {
        if (x < a) return 0.0f;
        if (x >= b) return 1.0f;

        x = (x - a) / (b - a);
        return (x * x * (3.0f - 2.0f * x));
    }

    static RECT CorrectResize(const SIZE& srcSize, const SIZE& destSize)
    {
        MFRatio aspectRatio;
        aspectRatio.Denominator = aspectRatio.Numerator = 1; 
        return CorrectResize(srcSize, destSize, aspectRatio);
    }

    static RECT CorrectResize(const SIZE& srcSize, const SIZE& destSize, MFRatio pixelAspectRatio)
    {
        RECT rcClient = { 0, 0, destSize.cx, destSize.cy };
        SIZE rcSrc = CorrectAspectRatio(srcSize, pixelAspectRatio);
        return LetterBoxRect(srcSize, rcClient);
    }

private:
    static RECT LetterBoxRect(const SIZE& srcSize, const RECT& destRect)
    {
        // figure out src/dest scale ratios

        int destWidth  = destRect.right - destRect.left;
        int destHeight = destRect.bottom - destRect.top;
        int boxedWidth;
        int boxedHeight;

        if (MulDiv(srcSize.cx, destHeight, srcSize.cy) <= destWidth) 
        {
            // Column letter boxing ("pillar box")
            boxedWidth  = MulDiv(destHeight, srcSize.cx, srcSize.cy);
            boxedHeight = destHeight;
        }
        else 
        {
            // Row letter boxing.
            boxedWidth  = destWidth;
            boxedHeight = MulDiv(destWidth, srcSize.cy, srcSize.cx);
        }

        // Create a centered rectangle within the current destination rect

        RECT boxedRect;
        LONG left = destRect.left + ((destWidth - boxedWidth) / 2);
        LONG top = destRect.top + ((destHeight - boxedHeight) / 2);
        SetRect(&boxedRect, left, top, left + boxedWidth, top + boxedHeight);
        return boxedRect;
    }

    //-----------------------------------------------------------------------------
    // CorrectAspectRatio
    //
    // Converts a rectangle from the source's pixel aspect ratio (PAR) to 1:1 PAR.
    // Returns the corrected rectangle.
    //
    // For example, a 720 x 486 rect with a PAR of 9:10, when converted to 1x1 PAR,  
    // is stretched to 720 x 540. 
    //-----------------------------------------------------------------------------

    static SIZE CorrectAspectRatio(const SIZE& srcSize, const MFRatio& pixelAspectRatio)
    {
        // Start with a rectangle the same size as src, but offset to the origin (0,0).
        SIZE size = srcSize;

        if ((pixelAspectRatio.Numerator != 1) || (pixelAspectRatio.Denominator != 1))
        {
            // Correct for the source's PAR.

            if (pixelAspectRatio.Numerator > pixelAspectRatio.Denominator)
            {
                // The source has "wide" pixels, so stretch the width.
                size.cx = MulDiv(srcSize.cx, pixelAspectRatio.Numerator, pixelAspectRatio.Denominator);
            }
            else if (pixelAspectRatio.Numerator < pixelAspectRatio.Denominator)
            {
                // The source has "tall" pixels, so stretch the height.
                size.cx = MulDiv(srcSize.cy, pixelAspectRatio.Denominator, pixelAspectRatio.Numerator);
            }
            // else: PAR is 1:1, which is a no-op.
        }
        return size;
    }
};

class FileUtils
{
public:
    static std::wstring GetApplicationDirectory() 
    {
        std::wstring dir;
        WCHAR dir_raw[MAX_PATH];

        GetModuleFileNameW(nullptr, dir_raw, MAX_PATH); 
        dir.append(dir_raw);
        size_t n = dir.find_last_of(L'\\', dir.size());
        dir = dir.substr(0, n);
        dir.append(L"\\");
        return dir;
    }

    static std::wstring GetFilenameFromPath(const std::wstring& path)
    {
        size_t c = path.find_last_of(L"/\\");
        return (c == std::wstring::npos) ? path : path.substr(c + 1);
    }

    static std::vector<std::wstring> ListFilesInApplicationDirectory(std::wstring extn)
    {
        return ListFilesInDirectory(GetApplicationDirectory(), extn);
    }

    static std::vector<std::wstring> ListFilesInDirectory(std::wstring directoryPath, std::wstring extn)
    {
        std::vector<std::wstring> filenames;
        WIN32_FIND_DATAW ffd;
        HANDLE hFind;

        std::wstring dirRoot = GetApplicationDirectory();
        std::wstring searchMask = dirRoot;
        searchMask.append(L"*.").append(extn);
        if (searchMask.size() > MAX_PATH)
            return filenames;

        hFind = FindFirstFileW(&searchMask[0], &ffd);
        if (INVALID_HANDLE_VALUE == hFind) 
            return filenames;
        do
        {
            if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                std::wstring name;
                name.append(dirRoot).append(ffd.cFileName);
                filenames.push_back(name);
            }
        }
        while (FindNextFileW(hFind, &ffd) != 0);
        return filenames;
    }
};
