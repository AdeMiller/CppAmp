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

#include <tchar.h>
#include <SDKDDKVer.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <amp.h>

using namespace concurrency;

// Note: This code is somewhat different from the code described in the book. It produces a more detailed
// output and accepts a /a switch that will show the REF and CPU accelerators. If you want the original 
// output, as show on page 22 then the /o switch will produce that.

int _tmain(int argc, _TCHAR* argv[])
{
    bool show_all = false;
    bool old_format = false;
    if (argc > 1) 
    {
        if (std::wstring(argv[1]).compare(L"/a") == 0)
        {
            show_all = true;
        }
        if (std::wstring(argv[1]).compare(L"/o") == 0)
        {
            show_all = false;
            old_format = true;
        }
    }

    std::vector<accelerator> accls = accelerator::get_all();
    if (!show_all)
    {
        accls.erase(std::remove_if(accls.begin(), accls.end(), [](accelerator& a) 
        { 
            return (a.device_path == accelerator::cpu_accelerator) || (a.device_path == accelerator::direct3d_ref); 
        }), accls.end());
    }

    if (accls.empty())
    {
        std::wcout << "No accelerators found that are compatible with C++ AMP" << std::endl << std::endl;
        return 0;
    }
    std::cout << "Show " << (show_all ? "all " : "") << "AMP Devices (";
#if defined(_DEBUG)
    std::cout << "DEBUG";
#else
    std::cout << "RELEASE";
#endif
    std::cout <<  " build)" << std::endl;
    std::wcout << "Found " << accls.size() 
        << " accelerator device(s) that are compatible with C++ AMP:" << std::endl;
    int n = 0;
    if (old_format)
    {
        std::for_each(accls.cbegin(), accls.cend(), [=, &n](const accelerator& a)
        {
            std::wcout << "  " << ++n << ": " << a.description 
                << ", has_display=" << (a.has_display ? "true" : "false") 
                << ", is_emulated=" << (a.is_emulated ? "true" : "false")
                << std::endl;
        });
        std::wcout << std::endl;
        return 1;
    }

    std::for_each(accls.cbegin(), accls.cend(), [=, &n](const accelerator& a)
    {
        std::wcout << "  " << ++n << ": " << a.description << " "  
            << std::endl << "       device_path                       = " << a.device_path
            << std::endl << "       dedicated_memory                  = " << std::setprecision(4) << float(a.dedicated_memory) / (1024.0f * 1024.0f) << " Mb"
            << std::endl << "       has_display                       = " << (a.has_display ? "true" : "false") 
            << std::endl << "       is_debug                          = " << (a.is_debug ? "true" : "false") 
            << std::endl << "       is_emulated                       = " << (a.is_emulated ? "true" : "false") 
            << std::endl << "       supports_double_precision         = " << (a.supports_double_precision ? "true" : "false") 
            << std::endl << "       supports_limited_double_precision = " << (a.supports_limited_double_precision ? "true" : "false") 
            << std::endl;
    });
    std::wcout << std::endl;
	return 1;
}
