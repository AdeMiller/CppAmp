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

#include <iostream>
#include <string>
#include <sstream>
#include <amp.h>

//--------------------------------------------------------------------------------------
//  Return a filtered list of C++ AMP accelerators.
//--------------------------------------------------------------------------------------

class AmpUtils
{
public:
    static inline std::vector<concurrency::accelerator> GetAccelerators(bool includeWarp = false) { return GetAccelerators(IsAmpAccelerator(includeWarp)); }

    static inline bool HasAccelerator(const std::wstring& devicePath)
    {
        std::vector<concurrency::accelerator> accls = concurrency::accelerator::get_all();
        return std::find_if(accls.begin(), accls.end(), [=](concurrency::accelerator& a) { return devicePath.compare(a.device_path) == 0; }) != accls.end();
    }

    static void DebugListAccelerators(std::vector<concurrency::accelerator> accelerators)
    {
        if (accelerators.empty())
            return;

        std::wostringstream oss;  
        oss << "Found these C++ AMP accelerators:" << std::endl;
        std::for_each(accelerators.cbegin(), accelerators.cend(), [&oss](const concurrency::accelerator& a) {
            oss << "  " << a.device_path << std::endl;                   
        });
        OutputDebugStringW(oss.str().c_str());
    }

private:

    //  Return only C++ AMP capable descrete GPUs and WARP accelerator if includeWarp is true.

    class IsAmpAccelerator
    { 
    private: 
        bool m_includeWarp;

    public: 
        IsAmpAccelerator(bool includeWarp) : m_includeWarp(includeWarp) {}

        bool operator() (const concurrency::accelerator& a) 
        { 
            return (a.is_emulated || ((a.device_path.compare(concurrency::accelerator::direct3d_warp) == 0) && !m_includeWarp)); 
        }  
    };

    template<typename Func>
    static std::vector<concurrency::accelerator> GetAccelerators(Func filter)
    {
        std::vector<accelerator> accls = accelerator::get_all();
        accls.erase(std::remove_if(accls.begin(), accls.end(), filter), accls.end());
        return accls;
    }
};

//  AMP restricted versions of STL functions.

template<typename T> 
inline T min(T a, T b) restrict(amp) { return ((a < b) ? a : b); }

template<typename T> 
inline T max(T a, T b) restrict(amp) { return ((a > b) ? a : b); }

//--------------------------------------------------------------------------------------
//  Detect if this is a Windows 8 OS.
//--------------------------------------------------------------------------------------

inline bool IsWindows8()
{
    OSVERSIONINFO verInfo;
    ZeroMemory(&verInfo, sizeof(OSVERSIONINFO));
    verInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    // GetVersionEx deprecated in VS 2013.
    #pragma warning (push)
    #pragma warning (disable: 4996)
    GetVersionEx(&verInfo);
    #pragma warning (pop)
    return (verInfo.dwMajorVersion >= 6) && (verInfo.dwMinorVersion >= 2);
}
