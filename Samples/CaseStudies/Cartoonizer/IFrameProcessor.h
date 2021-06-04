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

enum InitialIndexes 
{
    kCurrent = 0,
    kNext = 1,
    kOriginal = 2
};

class IFrameProcessor 
{
public:
    virtual void ProcessImage(const Gdiplus::BitmapData& srcFrame, 
        Gdiplus::BitmapData& destFrame, UINT phases, 
        UINT neighborWindow) = 0;
};
