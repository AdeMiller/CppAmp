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

#include "stdafx.h"
#include "ReverseStr.h"

// Problem 1: Write a method to reverse an arbitrary string provided as a null terminated char*.

namespace Extras
{
    //  You can use this instead of std::swap(). It's ugly IMO but removes the temp
    //  used by std::swap() but doesn't support other types.
    //
    //  See: http://en.wikipedia.org/wiki/XOR_swap_algorithm

    inline void XorSwap(char& left, char& right)
    {
        left = left ^ right;
        right = left ^ right;
        left = left ^ right;
    }

    inline char* FindEnd(char* const pStr)
    {
        char* pEnd = pStr;
        while (*pEnd != '\0') { ++pEnd; }
        return pEnd;
    }

    void ReverseStr(char* const pStr)
    {
        char* pEnd = FindEnd(pStr) - 1;
        char* pStart = pStr;

        while (pStart < pEnd)
        {
            XorSwap(*pStart, *pEnd);
            //std::swap(*pStart, *pEnd);
            --pEnd;
            ++pStart;
        }
    }

    //  C++ AMP version. This packs the char data into unsigned int.

    using namespace concurrency;

    typedef unsigned long PackedChars;

    struct CharBlock
    {
        unsigned A, B, C, D;

        CharBlock(const PackedChars& chrs) restrict(cpu, amp)
        {
            A =  chrs & 0xFF;
            B = (chrs & 0xFF00) >> 8;
            C = (chrs & 0xFF0000) >> 16;
            D = (chrs & 0xFF000000) >> 24; 
        }

        inline PackedChars ReversePack() restrict(cpu, amp)
        {
            return (D | (C << 8) | (B << 16) | (A << 24));
        }
    };

    //  This not only swaps left and right it also swaps the order of the bytes within 
    //  left and right. So 0x01020304 swapped with 0x0a0b0c0d becomes
    //  0x0d0c0b0a and 0x04030201.

    inline void Swap(PackedChars& left, PackedChars& right) restrict(amp)
    {
        PackedChars tmp = right;
        right = CharBlock(left).ReversePack();
        left = CharBlock(tmp).ReversePack();
    }

    void ReverseStrAmp(char* const pStr)
    {
        const unsigned blkSize = 4 * sizeof(char);
        const unsigned charLen = 
            static_cast<unsigned>(std::distance(pStr, FindEnd(pStr)));
        unsigned blkLen = charLen / blkSize;
        blkLen += ((blkLen % blkSize) > 0) ? 1 : 0; // Pad blocks.
        blkLen += (blkLen % 2);                     // Even out blocks count.
        const unsigned blkChars = blkLen * blkSize;

        std::vector<char> strData(blkChars);
        std::copy(pStr, pStr + charLen, strData.begin());
        array_view<PackedChars, 1> str(extent<1>(blkLen), 
            reinterpret_cast<PackedChars*>(strData.data()));

        const unsigned offset = blkLen - 1;
        parallel_for_each(extent<1>(blkLen / 2), [str, offset](index<1> idx) 
            restrict(amp)
        {
            Swap(str[idx], str[offset - idx]);
        });
        str.synchronize();

        copy(strData.cbegin() + blkChars - charLen, strData.cend(), 
            stdext::make_checked_array_iterator<char*>(pStr, charLen));
    }
}