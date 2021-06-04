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

using namespace concurrency::graphics;

//--------------------------------------------------------------------------------------
//  Template specialization for wostream output operator.
//--------------------------------------------------------------------------------------
//
//  See also: http://blogs.msdn.com/b/nativeconcurrency/archive/2012/04/03/short-vector-types-in-c-amp.aspx

template <typename ScalarType, int N>  
class stream_helper; 

//  Specialization for ScalerType_2

template<typename ScalarType>
class stream_helper<ScalarType, 2> 
{ 
public:
    typedef typename short_vector<ScalarType, 2>::type short_vector_type;
    static std::wostream& stream(std::wostream &os, const short_vector_type& vec)
    {
        os << "(" << vec.x << ", " << vec.y << ")";
        return os;
    } 
};

//  Specialization for ScalerType_3

template<typename ScalarType>
class stream_helper<ScalarType, 3> 
{ 
public:
    typedef typename short_vector<ScalarType, 3>::type short_vector_type;
    static std::wostream& stream(std::wostream &os, const short_vector_type& vec)
    {
        os << "(" << vec.x << ", " << vec.y << ", " << vec.z << ")";
        return os;
    } 
};

//  Specialization for ScalerType_4

template<typename ScalarType>
class stream_helper<ScalarType, 4> 
{ 
public:
    typedef typename short_vector<ScalarType, 4>::type short_vector_type;
    static std::wostream& stream(std::wostream &os, const short_vector_type& vec)
    {
        os << "(" << vec.x << ", " << vec.y << ", " << vec.z << ", " << vec.w << ")";
        return os;
    } 
};

template <typename ShortVectorType> 
inline typename std::enable_if<(short_vector_traits<ShortVectorType>::size > 1), std::wostream&>::type 
    operator<<(std::wostream &os, const ShortVectorType& vec) 
{
    return stream_helper<typename short_vector_traits<ShortVectorType>::value_type,
        short_vector_traits<ShortVectorType>::size>::stream(os, vec);
};