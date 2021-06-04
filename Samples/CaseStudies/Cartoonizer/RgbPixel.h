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

//  This is how an ARGB pixel is stored in CPU memory. Compatible with the memory layout used by BitmapData.
//  For more information see: http://en.wikipedia.org/wiki/RGBA_color_space

typedef unsigned long ArgbPackedPixel;

//  This is how an RGB pixed is stored unpacked on the GPU.
//  Use lower case r, g, b here for template compatibility with uint_3/uint_4 used by texture version.
//
//  Note that we could use uint_3 here but short vectors aren't introduced until chapter 11.

struct RgbPixel 
{
    unsigned int r;
    unsigned int g;
    unsigned int b;
};

//--------------------------------------------------------------------------------------
//  Pixel conversion functions.
//--------------------------------------------------------------------------------------

//  Pack the low order bytes from four unsigned longs representing RGB into a single unsigned long representing ARGB.
//  The Luma/Alpha value is always set to 0xFF.

const int fixedAlpha = 0xFF;

inline ArgbPackedPixel PackPixel(const RgbPixel& rgb) restrict(amp) 
{
    return (rgb.b | (rgb.g << 8) | (rgb.r << 16) | (fixedAlpha << 24));
}
 
//  Unpack the bytes from an unsigned long into three unsigned longs for RGB. The forth Luma/Alpha value is ignored.

inline RgbPixel UnpackPixel(const ArgbPackedPixel& packedArgb) restrict(amp) 
{
    RgbPixel rgb;
    rgb.b = packedArgb & 0xFF;
    rgb.g = (packedArgb & 0xFF00) >> 8;
    rgb.r = (packedArgb & 0xFF0000) >> 16;
    return rgb;
}
