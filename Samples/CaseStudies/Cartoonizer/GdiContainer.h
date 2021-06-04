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

// RAII container for GDI Plus handle.

class GdiContainer
{
private:
    Gdiplus::GdiplusStartupInput m_gdiplusStartupInput;
    ULONG_PTR m_gdiplusToken;

public:
    GdiContainer() : m_gdiplusToken(NULL)
    {
        Gdiplus::GdiplusStartup(&m_gdiplusToken, &m_gdiplusStartupInput, nullptr);
    }

    ~GdiContainer()
    {
        Gdiplus::GdiplusShutdown(m_gdiplusToken);
    }

private:
    // Hide assignment operator and copy constructor.
    GdiContainer const &operator =(GdiContainer const&);
    GdiContainer(GdiContainer const &);
};

