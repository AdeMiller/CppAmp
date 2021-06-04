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

using namespace concurrency;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Extras;

namespace ReverseStringTests
{		
    std::wstring Msg(std::string& expected, std::string& actual)
    {
        std::wostringstream msg;
        msg << "'" << std::wstring(expected.cbegin(), expected.cend()) <<  "' != '" << std::wstring(actual.cbegin(), actual.cend()) << "'" << std::endl;
        return msg.str();
    }

    TEST_CLASS(ReverseStrTests)
    {
    public:
        TEST_METHOD(ReverseStrTests_SimpleString)
        {
            std::string input("abc");
            std::string expected(input.rbegin(), input.rend());

            ReverseStr(const_cast<char*>(input.c_str()));

            Assert::AreEqual(0, expected.compare(input), Msg(expected, input).c_str());
        }

        TEST_METHOD(ReverseStrTests_SingleChar)
        {
            std::string input("a");
            std::string expected(input.rbegin(), input.rend());

            ReverseStr(const_cast<char*>(input.c_str()));

            Assert::AreEqual(0, expected.compare(input), Msg(expected, input).c_str());
        }

        TEST_METHOD(ReverseStrTests_OddNumberOfChars)
        {
            std::string input("abc");
            std::string expected(input.rbegin(), input.rend());

            ReverseStr(const_cast<char*>(input.c_str()));

            Assert::AreEqual(0, expected.compare(input), Msg(expected, input).c_str());
        }

        TEST_METHOD(ReverseStrTests_EmptyString)
        {
            std::string input("");
            std::string expected(input.rbegin(), input.rend());

            ReverseStr(const_cast<char*>(input.c_str()));

            Assert::AreEqual(0, expected.compare(input), Msg(expected, input).c_str());
        }
    };

    TEST_CLASS(ReverseStrAmpTests)
    {
    public:
        TEST_CLASS_INITIALIZE(ReverseStrAmpTests_InitializeAmp) 
        {
            // Initialize the C++ AMP runtime. This ensures that the first test run doesn't seem really slow.
            array<int> data(100);
            parallel_for_each(extent<1>(1), [&data](index<1> idx) restrict(amp) { data[idx] = 0; });
            accelerator(accelerator::default_accelerator).default_view.wait();
        }

        TEST_METHOD(ReverseStrAmpTests_SimpleString)
        {
            std::string input("abcdabcd");
            std::string expected(input.rbegin(), input.rend());

            ReverseStrAmp(const_cast<char*>(input.c_str()));
            
            Assert::AreEqual(0, expected.compare(input), Msg(expected, input).c_str());
        }

        TEST_METHOD(ReverseStrAmpTests_SimpleUnbalancedString)
        {
            std::string input("abcdabcdab");
            std::string expected(input.rbegin(), input.rend());

            ReverseStrAmp(const_cast<char*>(input.c_str()));

            Assert::AreEqual(0, expected.compare(input), Msg(expected, input).c_str());
        }

        TEST_METHOD(ReverseStrAmpTests_SingleChar)
        {
            std::string input("a");
            std::string expected(input.rbegin(), input.rend());

            ReverseStr(const_cast<char*>(input.c_str()));

            Assert::AreEqual(0, expected.compare(input), Msg(expected, input).c_str());
        }

        TEST_METHOD(ReverseStrAmpTests_OddNumberOfCharsSubBlockSize)
        {
            std::string input("abc");
            std::string expected(input.rbegin(), input.rend());

            ReverseStr(const_cast<char*>(input.c_str()));

            Assert::AreEqual(0, expected.compare(input), Msg(expected, input).c_str());
        }

        TEST_METHOD(ReverseStrAmpTests_OddNumberOfChars)
        {
            std::string input("abcdefghijklmnop");
            std::string expected(input.rbegin(), input.rend());

            ReverseStr(const_cast<char*>(input.c_str()));

            Assert::AreEqual(0, expected.compare(input), Msg(expected, input).c_str());
        }

        TEST_METHOD(ReverseStrAmpTests_EmptyString)
        {
            std::string input("");
            std::string expected(input.rbegin(), input.rend());

            ReverseStr(const_cast<char*>(input.c_str()));

            Assert::AreEqual(0, expected.compare(input), Msg(expected, input).c_str());
        }
    };
}