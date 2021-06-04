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

#include <iostream>
#include <xutility>

//===============================================================================
//  Check that a parameter known at compile time is a power of two.
//===============================================================================

namespace Extras
{
    enum BitWidth
    {
        Bit08 = 0x80,
        Bit16 = 0x8000,
        Bit32 = 0x80000000
    } ;

    template<unsigned int N>
    struct IsPowerOfTwoStatic
    {
        enum 
        { 
            result = ((CountBitsStatic<N, Bit32>::result == 1) ? TRUE : FALSE)
        };
    };

    // While 1 is technically 2^0, for the purposes of calculating 
    // tile size it isn't useful.
    template <>
    struct IsPowerOfTwoStatic<1>
    {
        enum { result = FALSE };
    };

    template<unsigned int N, unsigned int MaxBit>
    struct CountBitsStatic
    {
        enum
        { 
            result = (IsBitSetStatic<N, MaxBit>::result + CountBitsStatic<N, (MaxBit >> 1)>::result) 
        };
    };

    // Ensure that template program terminates.
    template<unsigned int N>
    struct CountBitsStatic<N, 0>
    {
        enum { result = FALSE };
    };

    template<unsigned int N, int Bit>
    struct IsBitSetStatic
    {
        enum { result = (N & Bit) ? 1 : 0 };
    };

//===============================================================================
//  Check that a parameter known at runtime is a power of two.
//===============================================================================

    template <unsigned int MaxBit>
    unsigned int CountBits(unsigned int n) 
    {
        return (n & 0x1) + CountBits<MaxBit-1>(n >> 1);
    }

    // template specialization to terminate the recursion when there's only one bit left
    template<>
    unsigned int CountBits<1>(unsigned int n) 
    {
        return n & 0x1;
    }

    BOOL IsPowerOfTwo(unsigned int n)
    {
        return (CountBits<32>(n) == 1);
    };

    //===============================================================================
    //  Stream output overloads for std::vector, array and array_view.
    //===============================================================================

    class ContainerWidth 
    {
    public:
        explicit ContainerWidth(size_t width) : m_width(width) { }

    private:
        size_t m_width;

        template <class T, class Traits>
        inline friend std::basic_ostream<T, Traits>& operator << 
            (std::basic_ostream<T, Traits>& os, const ContainerWidth& container)
        { 
            os.iword(details::geti()) = container.m_width; 
            return os;
        }
    };

    template<typename StrmType, typename Traits, typename VecT>
    std::basic_ostream<StrmType, Traits>& operator<< (std::basic_ostream<StrmType, Traits>& os, const std::vector<VecT>& vec)
    {
        size_t i = std::min<size_t>(details::GetWidth(os), vec.size());
        std::copy(std::begin(vec), std::begin(vec) + i, std::ostream_iterator<VecT, Traits::char_type>(os, details::GetDelimiter<Traits::char_type>()));
        return os;
    }

    template<typename StrmType, typename Traits, typename VecT>
    std::basic_ostream<StrmType, Traits>& operator<< (std::basic_ostream<StrmType, Traits>& os, concurrency::array<VecT, 1>& vec)
    {
        size_t i = std::min<size_t>(details::GetWidth(os), vec.extent[0]);
        std::vector<const VecT> buffer(i);
        copy(vec.section(0, i), std::begin(buffer));
        std::copy(std::begin(buffer), std::begin(buffer) + i, std::ostream_iterator<VecT, Traits::char_type>(os, details::GetDelimiter<Traits::char_type>()));
        return os;
    }

    template<typename StrmType, typename Traits, typename VecT>
    std::basic_ostream<StrmType, Traits>& operator<< (std::basic_ostream<StrmType, Traits>& os, const concurrency::array_view<VecT, 1>& vec)
    {
        size_t i = std::min<size_t>(details::GetWidth(os), vec.extent[0]);
        std::vector<VecT> buffer(i);
        copy(vec.section(0, i), std::begin(buffer));  
        std::copy(std::begin(buffer), std::begin(buffer) + i, std::ostream_iterator<VecT, Traits::char_type>(os, details::GetDelimiter<Traits::char_type>()));
        return os;
    }

    //===============================================================================
    //  Implementation. Not supposed to be called directly.
    //===============================================================================

    namespace details
    {
        inline int geti() 
        { 
            static int i = std::ios_base::xalloc();
            return i; 
        }

        template <typename STREAM>
        size_t GetWidth(STREAM& os)
        {
            const size_t kDefaultWidth = 10;
            size_t width = os.iword(geti());
            if (width == 0)
                width = kDefaultWidth;
            return width;
        }

        template <typename T>
        T* GetDelimiter()
        {
            assert(false);
            return nullptr;
        }

        template <>
        char* GetDelimiter()
        {
            static char delim(',');
            return &delim;
        }

        template <>
        wchar_t* GetDelimiter()
        {
            static wchar_t delim(L',');
            return &delim;
        }
    }
}
