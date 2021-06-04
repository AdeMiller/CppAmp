//===============================================================================
//
// Microsoft Press
// C++ AMP: Accelerated Massive Parallelism with Microsoft Visual C++
//
//===============================================================================
// Copyright (c) 2012 Ade Miller & Kate Gregory.  All rights reserved.
// This code released under the terms of the 
// Microsoft Public License (Ms-PL), http://ampbook.codeplex.com/license.
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//===============================================================================

#pragma once

//===============================================================================
//  Sequential scan implementation running on CPU. Used for testing.
//===============================================================================

namespace ScanTests
{
    //===============================================================================
    // Exclusive scan, output element at i contains the sum of elements [0]...[i-1].
    //===============================================================================

    template <typename InIt, typename OutIt>
    void ExclusiveScan(InIt first, InIt last, OutIt outFirst)
    {
        typedef OutIt::value_type T;

        *outFirst = T(0);
        for (int i = 1; i < std::distance(first, last); ++i)
            outFirst[i] = first[i - 1] + outFirst[i - 1];
    }

    //===============================================================================
    // Inclusive scan, output element at i contains the sum of elements [0]...[i].
    //===============================================================================

    template <typename InIt, typename OutIt>
    void InclusiveScan(InIt first, InIt last, OutIt outFirst)
    {
        typedef OutIt::value_type T;

        *outFirst = T(*first);
        for (int i = 1; i < std::distance(first, last); ++i)
            outFirst[i] = first[i] + outFirst[i - 1];
    }
}
