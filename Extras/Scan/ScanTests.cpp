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

#include <CppUnitTest.h>

#include "ScanSequential.h"
#include "ScanSimple.h"
#include "ScanTiled.h"
#include "ScanTiledOptimized.h"
#include "Utilities.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Extras;

namespace ScanTests
{
    std::wstring Msg(std::vector<int>& expected, std::vector<int>& actual, size_t width = 8)
    {
        std::wostringstream msg;
        msg << ContainerWidth(width) << L"[" << expected << L"] != [" << actual << L"]" << std::endl;
        return msg.str();
    }

    TEST_CLASS(ScanTests)
    {
    public:
        TEST_METHOD(ExclusiveScanTests_Simple)
        {
            std::vector<int> input(8, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 0);

            ExclusiveScan(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
        }

        TEST_METHOD(ExclusiveScanTests_Complex)
        {
            std::array<int, 8> input =    { 1, 3, 6,  2,  7,  9,  0,  5 };
            std::vector<int> result(input.size());
            std::array<int, 8> expected = { 0, 1, 4, 10, 12, 19, 28, 28 };

            ExclusiveScan(begin(input), end(input), result.begin());
            
            std::vector<int> exp(begin(expected), end(expected));
            Assert::IsTrue(exp == result, Msg(exp, result).c_str());
        }

        TEST_METHOD(InclusiveScanTests_Simple)
        {
            std::vector<int> input(8, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 1);

            InclusiveScan(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
        }

        TEST_METHOD(InclusiveScanTests_Complex)
        {
            std::array<int, 8> input =    { 1, 3,  6,  2,  7,  9,  0,  5 };
            std::vector<int> result(input.size());
            std::array<int, 8> expected = { 1, 4, 10, 12, 19, 28, 28, 33 };

            InclusiveScan(begin(input), end(input), result.begin());
            
            std::vector<int> exp(begin(expected), end(expected));
            Assert::IsTrue(exp == result, Msg(exp, result).c_str());
        }
    };

    TEST_CLASS(ScanSimpleTests)
    {
    public:
        TEST_METHOD(ExclusiveScanSimpleTests_Simple)
        {
            std::vector<int> input(8, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 0);

            ExclusiveScanSimple(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
        }

        TEST_METHOD(ExclusiveScanSimpleTests_Large)
        {
            std::vector<int> input(2048, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 0);

            ExclusiveScanSimple(begin(input), end(input), result.begin());

            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
        }

        TEST_METHOD(InclusiveScanSimpleTests_Simple)
        {
            std::vector<int> input(8, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 1);

            InclusiveScanSimple(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
        }

        TEST_METHOD(InclusiveScanSimpleTests_Complex)
        {
            std::array<int, 8> input =    { 1, 3,  6,  2,  7,  9,  0,  5 };
            std::vector<int> result(input.size());
            std::array<int, 8> expected = { 1, 4, 10, 12, 19, 28, 28, 33 };

            InclusiveScanSimple(begin(input), end(input), result.begin());
            
            std::vector<int> exp(begin(expected), end(expected));
            Assert::IsTrue(exp == result, Msg(exp, result).c_str());
        }

        TEST_METHOD(InclusiveScanSimpleTests_Large)
        {
            std::vector<int> input(2048, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 1);

            InclusiveScanSimple(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
        }
    };

    TEST_CLASS(ScanTiledTests)
    {
    public:
        TEST_METHOD(ExclusiveScanTiledTests_Simple_Two_Tiles)
        {
            std::vector<int> input(8, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 0);

            ExclusiveScanTiled<4>(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
        }

        TEST_METHOD(InclusiveScanTiledTests_Simple_Two_Tiles)
        {
            std::vector<int> input(8, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 1);

            InclusiveScanTiled<4>(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
        }

        TEST_METHOD(InclusiveScanTiledTests_Sequential_One_Tile)
        {
            std::array<int, 8> input =    { 1, 2, 3,  4,  5,  6,  7,  8 };
            std::vector<int> result(input.size());
            //std::array<int, 8> expected = {  1, 3, 3,  7,  5, 11,  7, 15  };    // Load only
            //std::array<int, 8> expected = {  1, 3, 4, 10,  8, 18, 12, 26  };    // Offset == 2
            std::array<int, 8> expected = { 1, 3, 6, 10, 15, 21, 28, 36 };

            InclusiveScanTiled<8>(begin(input), end(input), result.begin());
            
            std::vector<int> exp(begin(expected), end(expected));
            Assert::IsTrue(exp == result, Msg(exp, result).c_str());
        }

        TEST_METHOD(ExclusiveScanTiledTests_Sequential_One_Tile)
        {
            std::array<int, 8> input =    { 1, 2, 3,  4,  5,  6,  7,  8 };
            std::vector<int> result(input.size());
            std::array<int, 8> expected = { 0, 1, 3, 6, 10, 15, 21, 28 };

            ExclusiveScanTiled<8>(begin(input), end(input), result.begin());
            
            std::vector<int> exp(begin(expected), end(expected));
            Assert::IsTrue(exp == result, Msg(exp, result).c_str());
        }

        TEST_METHOD(InclusiveScanTiledTests_Complex_Two_Tiles)
        {
            std::array<int, 8> input =    { 1, 3,  6,  2,  7,  9,  0,  5 };
            std::vector<int> result(input.size());
            std::array<int, 8> expected = { 1, 4, 10, 12, 19, 28, 28, 33 };

            InclusiveScanTiled<4>(begin(input), end(input), result.begin());
            
            std::vector<int> exp(begin(expected), end(expected));
            Assert::IsTrue(exp == result, Msg(exp, result).c_str());
        }

        TEST_METHOD(ExclusiveScanTiledTests_Complex_Two_Tiles)
        {
            std::array<int, 8> input =    { 1, 3, 6,  2,  7,  9,  0,  5 };
            std::vector<int> result(input.size());
            std::array<int, 8> expected = { 0, 1, 4, 10, 12, 19, 28, 28 };

            ExclusiveScanTiled<4>(begin(input), end(input), result.begin());
            
            std::vector<int> exp(begin(expected), end(expected));
            Assert::IsTrue(exp == result, Msg(exp, result).c_str());
        }

        TEST_METHOD(InclusiveScanTiledTests_Large)
        {
            std::vector<int> input(2048, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 1);

            InclusiveScanTiled<4>(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
        }

        TEST_METHOD(ExclusiveScanTiledTests_Large)
        {
            std::vector<int> input(2048, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 0);

            ExclusiveScanTiled<256>(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
        }

        TEST_METHOD(InclusiveScanTiledTests_Simple_Overlapped_Tiles)
        {
            std::vector<int> input(10, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 1);

            InclusiveScanTiled<4>(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
        }
    };

    TEST_CLASS(ScanOptimizedTests)
    {
    public:
        TEST_METHOD(ExclusiveScanOptimizedTests_Simple_One_Tile)
        {
            std::vector<int> input(8, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 0);

            ExclusiveScanOptimized<4>(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
        }

        TEST_METHOD(ExclusiveScanOptimizedTests_Simple_Two_Tiles)
        {
            std::vector<int> input(16, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 0);

            ExclusiveScanOptimized<4>(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result, 16).c_str());
        }

        TEST_METHOD(ExclusiveScanOptimizedTests_Sequential_One_Tile)
        {
            std::array<int, 8> input =      {  1, 2,  3,  4,  5,  6,  7,  8 };
            std::vector<int> result(input.size());
            //std::array<int, 8> expected = { +1, 3, +3, 10, +5, 11, +7, 36 }; // Up sweep only
            //std::array<int, 8> expected = { +1, 3, +3, 10, +5, 11, +7,  0 }; // Down sweep depth = 0
            //std::array<int, 8> expected = { +1, 3, +3,  0, +5, 11, +7, 10 }; // Down sweep depth = 1
            //std::array<int, 8> expected = { +1, 0, +3,  3, +5, 10, +7, 21 }; // Down sweep depth = 2
            std::array<int, 8> expected =   {  0, 1,  3,  6, 10, 15, 21, 28 }; // Final Result

            ExclusiveScanOptimized<4>(begin(input), end(input), result.begin());
            
            std::vector<int> exp(begin(expected), end(expected));
            Assert::IsTrue(exp == result, Msg(exp, result).c_str());
        }

        TEST_METHOD(InclusiveScanOptimizedTests_Simple_One_Tile)
        {
            std::vector<int> input(8, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 1);

            InclusiveScanOptimized<4>(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
        }

        TEST_METHOD(InclusiveScanOptimizedTests_Simple_Two_Tiles)
        {
            std::vector<int> input(16, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 1);

            InclusiveScanOptimized<4>(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result, 16).c_str());
        }

        TEST_METHOD(InclusiveScanOptimizedTests_Complex_One_Tile)
        {
            std::array<int, 8> input =    { 1, 3,  6,  2,  7,  9,  0,  5 };
            std::vector<int> result(input.size());
            std::array<int, 8> expected = { 1, 4, 10, 12, 19, 28, 28, 33 };

            InclusiveScanOptimized<4>(begin(input), end(input), result.begin());
            
            std::vector<int> exp(begin(expected), end(expected));
            Assert::IsTrue(exp == result, Msg(exp, result, 8).c_str());
        }

        TEST_METHOD(ExclusiveScanOptimizedTests_Sequential_Two_Tiles)
        {
            // Use REF accelerator for a warp width of 4.
            //concurrency::accelerator::set_default(concurrency::accelerator::direct3d_ref);
            std::array<int, 16> input =    {  1, 2,  3,  4, 5,  6,  7,  8,  9, 10, 11, 12, 13, 14 ,15 ,16 };
            std::vector<int> result(input.size());
            //std::array<int, 16> expected =   {  1, 3, 3, 10,  5, 11,  7, 26, 9, 19, 11, 42 }; // Up sweep only
            //std::array<int, 16> expected = {  1, 3, 3,  0,  5, 11,  7,  0, 9, 19, 11,  0 }; // Down sweep depth = 0
            //std::array<int, 8> expected =  {  1, 0, 3,  3,  5,  0,  7, 11 }; // Down sweep depth = 1
            //std::array<int, 8> expected =  {  0, 1, 3,  6,  0,  5, 11, 18 }; // Down sweep depth = 2
            //std::array<int, 16> expected = {  0, 1, 3,  6,  0,  5, 11, 18,  0,  9, 19, 30 };  // Down sweep depth = 2
            std::array<int, 16> expected =   {  0, 1, 3,  6, 10, 15, 21, 28, 36, 45, 55, 66, 78, 91, 105, 120 }; // Final Result

            ExclusiveScanOptimized<4>(begin(input), end(input), result.begin());
            
            std::vector<int> exp(begin(expected), end(expected));
            Assert::IsTrue(exp == result, Msg(exp, result, 16).c_str());
        }

        TEST_METHOD(InclusiveScanOptimizedTests_Complex_Two_Tiles)
        {
            std::array<int, 8> input =    { 1, 3,  6,  2,  7,  9,  0,  5 };
            std::vector<int> result(input.size());
            std::array<int, 8> expected = { 1, 4, 10, 12, 19, 28, 28, 33 };

            InclusiveScanOptimized<2>(begin(input), end(input), result.begin());
            
            std::vector<int> exp(begin(expected), end(expected));
            Assert::IsTrue(exp == result, Msg(exp, result).c_str());
        }

        TEST_METHOD(ExclusiveScanOptimizedTests_Large)
        {
            std::vector<int> input(4096, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 0);
            // Does not work for tiles sizes greater than 32. Relying on warp sync.
            ExclusiveScanOptimized<256>(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result, 24).c_str());
        }

        TEST_METHOD(InclusiveScanOptimizedTests_Large)
        {
            std::vector<int> input(4096, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 1);
            // Does not work for tiles sizes greater than 32. Relying on warp sync.
            InclusiveScanOptimized<256>(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result, 24).c_str());
        }

        TEST_METHOD(ExclusiveScanOptimizedTests_Simple_Overlapped_Tiles)
        {
            std::vector<int> input(10, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 0);

            ExclusiveScanOptimized<4>(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result, 16).c_str());
        }
    };
}
