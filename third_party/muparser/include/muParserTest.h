/*

	 _____  __ _____________ _______  ______ ___________
	/     \|  |  \____ \__  \\_  __ \/  ___// __ \_  __ \
   |  Y Y  \  |  /  |_> > __ \|  | \/\___ \\  ___/|  | \/
   |__|_|  /____/|   __(____  /__|  /____  >\___  >__|
		 \/      |__|       \/           \/     \/
   Copyright (C) 2004 - 2022 Ingo Berg

	Redistribution and use in source and binary forms, with or without modification, are permitted
	provided that the following conditions are met:

	  * Redistributions of source code must retain the above copyright notice, this list of
		conditions and the following disclaimer.
	  * Redistributions in binary form must reproduce the above copyright notice, this list of
		conditions and the following disclaimer in the documentation and/or other materials provided
		with the distribution.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
	IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
	FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
	CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
	DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
	IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
	OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef MU_PARSER_TEST_H
#define MU_PARSER_TEST_H

#include <string>
#include <cstdlib>
#include <cstdint>
#include <numeric> // for accumulate
#include "muParser.h"
#include "muParserInt.h"

#if defined(_MSC_VER)
	#pragma warning(push)
	#pragma warning(disable : 4251)  // ...needs to have dll-interface to be used by clients of class ...
#endif

/** \file
	\brief This file contains the parser test class.
*/

namespace mu
{
	/** \brief Namespace for test cases. */
	namespace Test
	{
		/** \brief Test cases for unit testing.	*/
		class API_EXPORT_CXX ParserTester final
		{
		private:
			static int c_iCount;

			static value_type f0() { return 42; };

			// Multiarg callbacks
			static value_type f1of1(value_type v) { return v; };

			static value_type f1of2(value_type v, value_type) { return v; };
			static value_type f2of2(value_type, value_type v) { return v; };

			static value_type f1of3(value_type v, value_type, value_type) { return v; };
			static value_type f2of3(value_type, value_type v, value_type) { return v; };
			static value_type f3of3(value_type, value_type, value_type v) { return v; };

			static value_type f1of4(value_type v, value_type, value_type, value_type) { return v; }
			static value_type f2of4(value_type, value_type v, value_type, value_type) { return v; }
			static value_type f3of4(value_type, value_type, value_type v, value_type) { return v; }
			static value_type f4of4(value_type, value_type, value_type, value_type v) { return v; }

			static value_type f1of5(value_type v, value_type, value_type, value_type, value_type) { return v; }
			static value_type f2of5(value_type, value_type v, value_type, value_type, value_type) { return v; }
			static value_type f3of5(value_type, value_type, value_type v, value_type, value_type) { return v; }
			static value_type f4of5(value_type, value_type, value_type, value_type v, value_type) { return v; }
			static value_type f5of5(value_type, value_type, value_type, value_type, value_type v) { return v; }

			static value_type Min(value_type a_fVal1, value_type a_fVal2) { return (a_fVal1 < a_fVal2) ? a_fVal1 : a_fVal2; }
			static value_type Max(value_type a_fVal1, value_type a_fVal2) { return (a_fVal1 > a_fVal2) ? a_fVal1 : a_fVal2; }

			static value_type plus2(value_type v1) { return v1 + 2; }
			static value_type times3(value_type v1) { return v1 * 3; }
			static value_type sqr(value_type v1) { return v1 * v1; }
			static value_type sign(value_type v) { return -v; }
			static value_type add(value_type v1, value_type v2) { return v1 + v2; }
			static value_type land(value_type v1, value_type v2) { return (int)v1 & (int)v2; }


			static value_type FirstArg(const value_type* a_afArg, int a_iArgc)
			{
				if (!a_iArgc)
					throw mu::Parser::exception_type(_T("too few arguments for function FirstArg."));

				return  a_afArg[0];
			}

			static value_type LastArg(const value_type* a_afArg, int a_iArgc)
			{
				if (!a_iArgc)
					throw mu::Parser::exception_type(_T("too few arguments for function LastArg."));

				return  a_afArg[a_iArgc - 1];
			}

			static value_type Sum(const value_type* a_afArg, int a_iArgc)
			{
				if (!a_iArgc)
					throw mu::Parser::exception_type(_T("too few arguments for function sum."));

				value_type fRes = 0;
				for (int i = 0; i < a_iArgc; ++i) fRes += a_afArg[i];
				return fRes;
			}

			static value_type Rnd(value_type v)
			{
				return (value_type)(1 + (v * std::rand() / (RAND_MAX + 1.0)));
			}

			static value_type RndWithString(const char_type*)
			{
				return (value_type)(1.0 + (1000.0 * std::rand() / (RAND_MAX + 1.0)));
			}

			static value_type Ping()
			{
				return 10;
			}

			static value_type ValueOf(const char_type*)
			{
				return 123;
			}

			static value_type StrFun1(const char_type* v1)
			{
				int val(0);
				stringstream_type(v1) >> val;
				return (value_type)val;
			}

			static value_type StrFun2(const char_type* v1, value_type v2)
			{
				int val(0);
				stringstream_type(v1) >> val;
				return (value_type)(val + v2);
			}

			static value_type StrFun3(const char_type* v1, value_type v2, value_type v3)
			{
				int val(0);
				stringstream_type(v1) >> val;
				return val + v2 + v3;
			}

			static value_type StrFun4(const char_type* v1, value_type v2, value_type v3, value_type v4)
			{
				int val(0);
				stringstream_type(v1) >> val;
				return val + v2 + v3 + v4;
			}

			static value_type StrFun5(const char_type* v1, value_type v2, value_type v3, value_type v4, value_type v5)
			{
				int val(0);
				stringstream_type(v1) >> val;
				return val + v2 + v3 + v4 + v5;
			}

			static value_type StrFun6(const char_type* v1, value_type v2, value_type v3, value_type v4, value_type v5, value_type v6)
			{
				int val(0);
				stringstream_type(v1) >> val;
				return val + v2 + v3 + v4 + v5 + v6;
			}

			static value_type StrToFloat(const char_type* a_szMsg)
			{
				value_type val(0);
				stringstream_type(a_szMsg) >> val;
				return val;
			}

			// postfix operator callback
			static value_type Mega(value_type a_fVal) 
			{
				return a_fVal * (value_type)1e6; 
			}
			
			static value_type Micro(value_type a_fVal)
			{
				return a_fVal * (value_type)1e-6; 
			}

			static value_type Milli(value_type a_fVal) 
			{
				return a_fVal / (value_type)1e3; 
			}

			// Custom value recognition
			static int IsHexVal(const char_type* a_szExpr, int* a_iPos, value_type* a_fVal);

			// With user data
			static value_type FunUd0(void* data) 
			{
				return reinterpret_cast<std::intptr_t>(data); 
			}

			static value_type FunUd1(void* data, value_type v) 
			{
				return reinterpret_cast<std::intptr_t>(data) + v; 
			}
			
			static value_type FunUd2(void* data, value_type v1, value_type v2) 
			{
				return reinterpret_cast<std::intptr_t>(data) + v1 + v2; 
			}

			static value_type FunUd10(void* data, value_type v1, value_type v2, value_type v3, value_type v4, value_type v5, value_type v6, value_type v7, value_type v8, value_type v9, value_type v10)
			{
				return reinterpret_cast<std::intptr_t>(data) + v1 + v2 + v3 + v4 + v5 + v6 + v7 + v8 + v9 + v10;
			}

			static value_type StrFunUd3(void* data, const char_type* v1, value_type v2, value_type v3)
			{
				int val(0);
				stringstream_type(v1) >> val;
				return reinterpret_cast<std::intptr_t>(data) + val + v2 + v3;
			}

			static value_type SumUd(void* data, const value_type* a_afArg, int a_iArgc)
			{
				if (!a_iArgc)
					throw mu::Parser::exception_type(_T("too few arguments for function sum."));

				value_type fRes = 0;
				for (int i = 0; i < a_iArgc; ++i) 
					fRes += a_afArg[i];

				return reinterpret_cast<std::intptr_t>(data) + fRes;
			}

			int TestNames();
			int TestSyntax();
			int TestMultiArg();
			int TestPostFix();
			int TestExpression();
			int TestInfixOprt();
			int TestBinOprt();
			int TestVarConst();
			int TestInterface();
			int TestException();
			int TestStrArg();
			int TestIfThenElse();
			int TestBulkMode();
			int TestOssFuzzTestCases();
			int TestOptimizer();
			int TestLocalization();
			
			void Abort() const;

		public:
			typedef int (ParserTester::* testfun_type)();

			ParserTester();
			int Run();

		private:
			std::vector<testfun_type> m_vTestFun;
			void AddTest(testfun_type a_pFun);

			// Test Double Parser
			int EqnTest(const string_type& a_str, double a_fRes, bool a_fPass);
			int EqnTestWithVarChange(const string_type& a_str, double a_fRes1, double a_fVar1,	double a_fRes2,	double a_fVar2);
			int EqnTestLocalized(const string_type& a_str, double a_fRes, bool a_fPass);
			int ThrowTest(const string_type& a_str, int a_iErrc, bool a_bFail = true);

			// Test Int Parser
			int EqnTestInt(const string_type& a_str, double a_fRes, bool a_fPass);

			// Test Bulkmode
			int EqnTestBulk(const string_type& a_str, double a_fRes[4], bool a_fPass);

		};
	} // namespace Test
} // namespace mu


#if defined(_MSC_VER)
	#pragma warning(pop)
#endif

#endif

