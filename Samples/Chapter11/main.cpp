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

#include <amp.h>
#include <amp_graphics.h>
#include <vector>
#include <array>
#include <iostream>
#include <d3d11.h>
#include <atlbase.h>
#include <assert.h>

#include "AmpStreamUtils.h"
#include "AmpVectorUtils.h"

using namespace concurrency;
using namespace concurrency::graphics;
using namespace concurrency::graphics::direct3d;
using namespace concurrency::direct3d;

void NormAndUnormExample();
void ShortVectorsExample();
void TextureCopyExample();
void TextureReadingExample();
void TextureReadingCharsExample();
void TextureWritingExample();
void TextureReadingAndWritingExample();
void TextureReadingAndWritingWithViewsExample();
void InteropFromD3DExample();
void InteropToD3DExample();

int main()
{
#ifndef _DEBUG
    accelerator defaultDevice;
    std::wcout << L" Using device : " << defaultDevice.get_description() << std::endl;
    if (defaultDevice == accelerator(accelerator::direct3d_ref))
        std::wcout << " WARNING!! No C++ AMP hardware accelerator detected, using the REF accelerator." << std::endl << 
            "To see better performance run on C++ AMP\ncapable hardware." << std::endl;
#endif

    NormAndUnormExample();
    ShortVectorsExample();
    TextureCopyExample();
    TextureReadingExample();
    TextureReadingCharsExample();
    TextureWritingExample();
    TextureReadingAndWritingExample();
    TextureReadingAndWritingWithViewsExample();
    InteropFromD3DExample();
    InteropToD3DExample();
    return 0;
}

void NormAndUnormExample()
{
    unorm val1;
    std::wcout << "val1 = " << val1 << std::endl;
    norm val2(2.0f);
    std::wcout << "val2 = " << val2 << std::endl;
    unorm val3(-2.0f);
    std::wcout << "val3 = " << val3 << std::endl;
    unorm val4(2u);
    std::wcout << "val4 = " << val4 << std::endl;
    float val5 = norm(0.25f) + unorm(1.5f) ;
    std::wcout << "val5 = " << val5 << std::endl;
    auto val6 = -norm(0.25f);
    std::wcout << "val6 = " << val6 << std::endl;
    auto val7 = -unorm(0.25f);
    std::wcout << "val7 = " << val7 << std::endl;
}

void ShortVectorsExample()
{
    //  Construction

    float_3 vec1;
    float_3 vec2(1.0f);
    float_3 vec3(1.0f, 2.0f, 3.0f);

    //  Swizzling

    int_4 vec4(1, 2, 3, 4);
    std::wcout << "vec4.x = " << vec4.x << std::endl;
    int_2 vec5 = vec4.br;
    std::wcout << "vec4.br = " << vec5 << std::endl;

    //  Template metaprogramming

    double_2 vec6(1.0);
    std::wcout << "length(vec6) = " << length(vec6) << std::endl;

    int vec7(2);
    std::wcout << "length(vec7) = " << length(vec7) << std::endl;

    std::wcout << "length(vec4) = " << length(vec4) << std::endl;
    std::wcout << "length(vec5) = " << length(vec5) << std::endl;
}

void TextureCopyExample()
{
    const int cols = 32;
    const int rows = 64;
    std::vector<uint> uintData((rows * cols), 1);

    accelerator acc = accelerator();
    const texture<int, 2> text0(rows, cols, acc.default_view);

    texture<uint, 2> text1(rows, cols, uintData.cbegin(), uintData.cend());

    uint bitsPerScalarElement = 8u;
    uint dataSize = rows * cols;
    std::vector<char> byteData((rows * cols), 1);
    texture<uint, 2> text2(rows, cols, byteData.data(), dataSize, bitsPerScalarElement);

    texture<uint, 2> text3(rows, cols, bitsPerScalarElement);
    copy(uintData.data(), dataSize, text3);

    // Visual Studio 2013 depricates writeonly_texture_view<N, T>. It is replaced with a new 
    // texture_view<T, N> that implements additional functionality. The following blog post has
    // additional details:
    //
    // http://blogs.msdn.com/b/nativeconcurrency/archive/2013/07/25/overview-of-the-texture-view-design-in-c-amp.aspx

#if (_MSC_VER >= 1800)
    texture_view<uint, 2> textVw3(text3);
#else
    writeonly_texture_view<uint, 2> textVw3(text3);
#endif
    copy(uintData.data(), dataSize, textVw3);

    copy(text3, byteData.data(), dataSize);

    texture<uint, 2> text4(rows, cols, bitsPerScalarElement);
    text3.copy_to(text4);

    //  Asynchronous copy

    completion_future f = copy_async(text3, byteData.data(), dataSize);
    
    // Do other work... 

    f.then([=](){ std::wcout << "Copy complete" << std::endl; });

    f.get();
    std::wcout << "Copy complete" << std::endl;
}

void TextureReadingExample()
{
    const int cols = 32;
    const int rows = 64;
    std::vector<int> input((rows * cols), 1);
    
    const texture<int, 2> inputTx(rows, cols, input.cbegin(), input.cend());
    // Other examples:
    //const texture<int, 2> inputTx(rows, cols, accelerator(accelerator::default_accelerator).default_view);
    //const texture<unorm, 2> inputTx(rows, cols, 8u);
    std::vector<int> output((rows * cols), 0);
    array_view<int, 2> outputAv(rows, cols, output);
    outputAv.discard_data();

    parallel_for_each(outputAv.extent, [&inputTx, outputAv](index<2> idx) restrict(amp)
    {
        outputAv[idx] = inputTx[idx];                   // subscript [index<2>] operator
        outputAv[idx] = inputTx(idx);                   // function (index<2>) operator 
        outputAv[idx] = inputTx.get(idx);               // get(index<2>) method     
        outputAv[idx] = inputTx(idx[0], idx[1]);        // function (int, int) operator 
    });

    std::wcout << "extent:      (" << inputTx.extent[0] << ", " << inputTx.extent[1] << ")" << std::endl;
    std::wcout << "size:        " << inputTx.data_length << std::endl;
    std::wcout << "BPSE:        " << inputTx.bits_per_scalar_element << std::endl;
    std::wcout << "accelerator: " << inputTx.accelerator_view.accelerator.description << std::endl;
}

void TextureReadingCharsExample()
{
    const UINT bitsPerScalarElement = 8u;
    const int size = 1024;
    std::vector<char> input(size, 'a');

    const texture<int, 1> inputTx(size, input.data(), size, bitsPerScalarElement);
    std::vector<int> output(size, 0);
    array_view<int, 1> outputAv(size, output);
    outputAv.discard_data();

    parallel_for_each(outputAv.extent, [&inputTx, outputAv](index<1> idx) restrict(amp)
    {
        int element = inputTx[idx];
        outputAv[idx] = element; // ... Calculate based on 8 bit element value holding a char value
    });
}

void TextureWritingExample()
{
    const int cols = 32;
    const int rows = 64;
    std::vector<int> input((rows * cols), 1);

    texture<int, 2> outputTx(rows, cols, input.cbegin(), input.cend());
    array_view<int, 2> inputAv(rows, cols, input);

    parallel_for_each(outputTx.extent, [inputAv, &outputTx](index<2> idx) restrict(amp)
    {
        outputTx.set(idx, inputAv[idx]);
    });
}

void TextureReadingAndWritingExample()
{
    const int cols = 32;
    const int rows = 64;
    std::vector<int> input((rows * cols), 1);

    texture<int, 2> outputTx(rows, cols, input.cbegin(), input.cend());
    parallel_for_each(outputTx.extent, [&outputTx](index<2> idx) restrict(amp)
    {
        outputTx.set(idx, outputTx[idx] + 1);
    });

    parallel_for_each(outputTx.extent, [&outputTx](index<2> idx) restrict(amp)
    {
        // Visual Studio 2013 depricates writeonly_texture_view<N, T>. It is replaced with a new 
        // texture_view<T, N> that implements additional functionality. The following blog post has
        // additional details:
        //
        // http://blogs.msdn.com/b/nativeconcurrency/archive/2013/07/25/overview-of-the-texture-view-design-in-c-amp.aspx

#if (_MSC_VER >= 1800)
        texture_view<int, 2> outputTxVw(outputTx);
#else
        writeonly_texture_view<int, 2> outputTxVw(outputTx);
#endif
        outputTxVw.set(idx, outputTx[idx] + 1);
    });

    // DO NOT USE THIS CODE. IT IS AN EXAMPLE OF SOMETHING THAT IS NOT SUPPORTED!
    /*
    texture<int, 2> text2(rows, cols, input.data(), input.size() * sizeof(int), 32u);
    writeonly_texture_view<int, 2> outputTxVw(text2); 
    parallel_for_each(outputTxVw.extent, [outputTxVw, &text2](index<2> idx) restrict(amp)
    {
       outputTxVw.set(idx, text2[idx] +1);
    });
    */
}

void TextureReadingAndWritingWithViewsExample()
{
    const int cols = 32;
    const int rows = 64;

    texture<int_2, 2> text1(rows, cols); 

    // Visual Studio 2013 depricates writeonly_texture_view<N, T>. It is replaced with a new 
    // texture_view<T, N> that implements additional functionality. The following blog post has
    // additional details:
    //
    // http://blogs.msdn.com/b/nativeconcurrency/archive/2013/07/25/overview-of-the-texture-view-design-in-c-amp.aspx

#if (_MSC_VER >= 1800)
    texture_view<int_2, 2> textVw(text1); 
#else
    writeonly_texture_view<int_2, 2> textVw(text1);
#endif
    parallel_for_each(textVw.extent, [textVw] (index<2> idx) restrict(amp) 
    {
        textVw.set(idx, int_2(1, 1)); 
    });

    // THIS CODE WILL THROW A runtime_exception
    /*
    std::vector<int> input((rows * cols), 1);
    texture<int, 2> text2(rows, cols, input.data(), input.size() * sizeof(int), 32u);
    writeonly_texture_view<int, 2> outputTxVw(text2); 
    parallel_for_each(outputTxVw.extent, [outputTxVw, &text2](index<2> idx) restrict(amp)
    {
        outputTxVw.set(idx, text2[idx] + 1);
    }); 
    */
}

void InteropFromD3DExample()
{ 
    //  Get a D3D device from an accelerator_view.

    HRESULT hr = S_OK;
    CComPtr<ID3D11Device> device;
    IUnknown* unkDev = get_device(accelerator(accelerator::default_accelerator).default_view);
    hr = unkDev->QueryInterface(__uuidof(ID3D11Device), reinterpret_cast<LPVOID*>(&device));   
    assert(SUCCEEDED(hr));

    //  Get a D3D buffer from an array.

    array<int, 1> arr(1024);
    CComPtr<ID3D11Buffer> buffer;
    IUnknown* unkBuf = get_buffer(arr);
    hr = unkBuf->QueryInterface(__uuidof(ID3D11Buffer), reinterpret_cast<LPVOID*>(&buffer));   
    assert(SUCCEEDED(hr));

    //  Get a D3D texture resource from a texture.

    texture<int, 2> text(100, 100);
    CComPtr<ID3D11Texture2D> texture;
    IUnknown* unkRes = get_texture(text); 
    hr = unkRes->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<LPVOID*>(&texture));   
    assert(SUCCEEDED(hr));
}

void InteropToD3DExample()
{
    //  Create an accelerator_view from a D3D device.

    HRESULT hr = S_OK;
    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    std::array<D3D_FEATURE_LEVEL, 1> featureLevels = { D3D_FEATURE_LEVEL_11_0 };
    
    CComPtr<ID3D11Device> device;
    D3D_FEATURE_LEVEL featureLevel;
    CComPtr<ID3D11DeviceContext> immediateContext;

    hr = D3D11CreateDevice(nullptr /* default adapter */,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr /* No software rasterizer */,
        createDeviceFlags,
        featureLevels.data(),
        UINT(featureLevels.size()),
        D3D11_SDK_VERSION,
        &device,
        &featureLevel,
        &immediateContext);
    assert(SUCCEEDED(hr));

    accelerator_view dxView = create_accelerator_view(device);
    std::wcout << "Created accelerator_view on " 
        << dxView.accelerator.description << std::endl;

    //  Create an array from a D3D buffer

    hr = S_OK; 
    UINT bufferSize = 1024;
    D3D11_BUFFER_DESC bufferDesc =
    {
        bufferSize * sizeof(float),
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
        0 /* no CPU access */,
        D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS /* misc flags */,
        sizeof(float)
    };
    D3D11_SUBRESOURCE_DATA resourceData;
    ZeroMemory(&resourceData, sizeof(D3D11_SUBRESOURCE_DATA));

    std::vector<float> vertices(bufferSize, 1.0f);

    resourceData.pSysMem = &vertices[0];
    CComPtr<ID3D11Buffer> buffer;
    hr = device->CreateBuffer(&bufferDesc, &resourceData, &buffer);
    assert(SUCCEEDED(hr));

    array<float, 1> arr = make_array<float, 1>(extent<1>(bufferSize), dxView, buffer);
    std::wcout << "Created array<float,1> on " 
        << arr.accelerator_view.accelerator.description << std::endl;

    // Create a texture from a D3D texture resource

    const int height = 100; 
    const int width = 100;

    D3D11_TEXTURE2D_DESC desc; 
    ZeroMemory(&desc, sizeof(desc)); 
    desc.Height = height; 
    desc.Width = width; 
    desc.MipLevels = 1; 
    desc.ArraySize = 1; 
    desc.Format = DXGI_FORMAT_R8G8B8A8_UINT; 
    desc.SampleDesc.Count = 1; 
    desc.SampleDesc.Quality = 0; 
    desc.Usage = D3D11_USAGE_DEFAULT; 
    desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE; 
    desc.CPUAccessFlags = 0; 
    desc.MiscFlags = 0;

    CComPtr<ID3D11Texture2D> dxTexture = nullptr; 
    hr = device->CreateTexture2D(&desc, nullptr, &dxTexture);
    assert(SUCCEEDED(hr));

    texture<uint4, 2> ampTexture = make_texture<uint4, 2>(dxView, dxTexture);
    std::wcout << "Created texture<uint4, 2> on " << ampTexture.accelerator_view.accelerator.description << std::endl;
}