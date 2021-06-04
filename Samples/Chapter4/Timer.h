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

inline double ElapsedTime(const LARGE_INTEGER& start, const LARGE_INTEGER& end)
{
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return (double(end.QuadPart) - double(start.QuadPart)) * 1000.0 / double(freq.QuadPart);
}

template <typename Func>
double TimeFunc(Func f)
{
    //  This assumes that the kernel runs on the default view. If it doesn't then this code 
    //  will need to be altered to account for that.
    accelerator_view view = accelerator(accelerator::default_accelerator).default_view;

    //  Ensure that the C++ AMP runtime is initialized.
    accelerator::get_all();

    //  Ensure that the C++ AMP kernel has been JITed.
    f();

    //  Wait for all accelerator work to end.
    view.wait();

    LARGE_INTEGER start, end;
    QueryPerformanceCounter(&start);

    f();

    //  Wait for all accelerator work to end.
    view.wait();
    QueryPerformanceCounter(&end);

    return ElapsedTime(start, end);
}
