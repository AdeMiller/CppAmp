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

#include <amp_graphics.h>
#include <amp_math.h>

using namespace concurrency;
using namespace concurrency::graphics;

//===============================================================================
//  Templatized vector length function.
//===============================================================================

//  The length() function for N > 1.

template<typename T>
inline static typename std::enable_if<(short_vector_traits<typename T>::size > 0), float>::type 
    length(const T& vec) restrict(cpu, amp)
{
    return length_helper<short_vector_traits<typename T>::value_type, 
        short_vector_traits<typename T>::size>::length(vec);
}

//  Template specializations for ScalarType_N short vectors.

template<typename ScalarType, int N>
class length_helper 
{
public:
    inline static float length(const typename short_vector<ScalarType, N>::type& vec) 
        restrict(cpu, amp)
    {
        static_assert(false, "length() is not supported for this type.");
    }
};

template<typename ScalarType>
class length_helper<ScalarType, 1>
{
public:
    inline static float length(const typename short_vector<ScalarType, 1>::type& vec) 
        restrict(cpu, amp)
    {
        return static_cast<float>(vec);
    }
};

template<typename ScalarType>
class length_helper<ScalarType, 2>
{
public:
    inline static float length(const typename short_vector<ScalarType, 2>::type& vec) 
        restrict(cpu, amp)
    {
        return fast_math::sqrtf(static_cast<float>(vec.x * vec.x + vec.y * vec.y));
    }
};

template<typename ScalarType>
class length_helper<ScalarType, 3>
{
public:
    inline static float length(const typename short_vector<ScalarType, 4>::type& vec) restrict(cpu, amp)
    {
        return fast_math::sqrtf(static_cast<float>(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z));
    } 
};

template<typename ScalarType>
class length_helper<ScalarType, 4>
{
public:
    inline static float length(const typename short_vector<ScalarType, 4>::type& vec) restrict(cpu, amp)
    {
        return fast_math::sqrtf(static_cast<float>(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z + vec.w * vec.w));
    } 
};

//  Additional specializations for double_N vectors, these use precise_math::sqrt and return a double.

inline static double length(const double& vec) restrict(cpu, amp)
{
    return vec;
}

inline static double length(const double_2& vec) restrict(cpu, amp)
{
    return precise_math::sqrt(vec.x * vec.x + vec.y * vec.y);
}

inline static double length(const double_3& vec) restrict(cpu, amp)
{
    return precise_math::sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
}

inline static double length(const double_4& vec) restrict(cpu, amp)
{
    return precise_math::sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z + vec.w * vec.w);
}
