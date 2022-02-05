///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test general CPL features.
// Author:   Mateusz Loskot <mateusz@loskot.net>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2006, Mateusz Loskot <mateusz@loskot.net>
// Copyright (c) 2008-2012, Even Rouault <even dot rouault at spatialys.com>
// Copyright (c) 2017, Dmitry Baryshnikov <polimax@mail.ru>
// Copyright (c) 2017, NextGIS <info@nextgis.com>
/*
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef GDAL_COMPILATION
#define GDAL_COMPILATION
#endif

#include "gdal_unit_test.h"

#include "cpl_compressor.h"
#include "cpl_error.h"
#include "cpl_hash_set.h"
#include "cpl_list.h"
#include "cpl_sha256.h"
#include "cpl_string.h"
#include "cpl_safemaths.hpp"
#include "cpl_time.h"
#include "cpl_json.h"
#include "cpl_json_streaming_parser.h"
#include "cpl_json_streaming_writer.h"
#include "cpl_mem_cache.h"
#include "cpl_http.h"
#include "cpl_auto_close.h"
#include "cpl_minixml.h"
#include "cpl_quad_tree.h"
#include "cpl_worker_thread_pool.h"

#include <fstream>
#include <string>

static bool gbGotError = false;
static void CPL_STDCALL myErrorHandler(CPLErr, CPLErrorNum, const char*)
{
    gbGotError = true;
}

namespace tut
{

    // Common fixture with test data
    struct test_cpl_data
    {
        std::string data_;

        test_cpl_data()
        {
            // Compose data path for test group
            data_ = tut::common::data_basedir;
        }
    };

    // Register test group
    typedef test_group<test_cpl_data> group;
    typedef group::object object;
    group test_cpl_group("CPL");

    // Test cpl_list API
    template<>
    template<>
    void object::test<1>()
    {
        CPLList* list;

        list = CPLListInsert(nullptr, (void*)nullptr, 0);
        ensure(CPLListCount(list) == 1);
        list = CPLListRemove(list, 2);
        ensure(CPLListCount(list) == 1);
        list = CPLListRemove(list, 1);
        ensure(CPLListCount(list) == 1);
        list = CPLListRemove(list, 0);
        ensure(CPLListCount(list) == 0);
        list = nullptr;

        list = CPLListInsert(nullptr, (void*)nullptr, 2);
        ensure(CPLListCount(list) == 3);
        list = CPLListRemove(list, 2);
        ensure(CPLListCount(list) == 2);
        list = CPLListRemove(list, 1);
        ensure(CPLListCount(list) == 1);
        list = CPLListRemove(list, 0);
        ensure(CPLListCount(list) == 0);
        list = nullptr;

        list = CPLListAppend(list, (void*)1);
        ensure(CPLListGet(list,0) == list);
        ensure(CPLListGet(list,1) == nullptr);
        list = CPLListAppend(list, (void*)2);
        list = CPLListInsert(list, (void*)3, 2);
        ensure(CPLListCount(list) == 3);
        CPLListDestroy(list);
        list = nullptr;

        list = CPLListAppend(list, (void*)1);
        list = CPLListAppend(list, (void*)2);
        list = CPLListInsert(list, (void*)4, 3);
        CPLListGet(list,2)->pData = (void*)3;
        ensure(CPLListCount(list) == 4);
        ensure(CPLListGet(list,0)->pData == (void*)1);
        ensure(CPLListGet(list,1)->pData == (void*)2);
        ensure(CPLListGet(list,2)->pData == (void*)3);
        ensure(CPLListGet(list,3)->pData == (void*)4);
        CPLListDestroy(list);
        list = nullptr;

        list = CPLListInsert(list, (void*)4, 1);
        CPLListGet(list,0)->pData = (void*)2;
        list = CPLListInsert(list, (void*)1, 0);
        list = CPLListInsert(list, (void*)3, 2);
        ensure(CPLListCount(list) == 4);
        ensure(CPLListGet(list,0)->pData == (void*)1);
        ensure(CPLListGet(list,1)->pData == (void*)2);
        ensure(CPLListGet(list,2)->pData == (void*)3);
        ensure(CPLListGet(list,3)->pData == (void*)4);
        list = CPLListRemove(list, 1);
        list = CPLListRemove(list, 1);
        list = CPLListRemove(list, 0);
        list = CPLListRemove(list, 0);
        ensure(list == nullptr);
    }

    typedef struct
    {
        const char* testString;
        CPLValueType expectedResult;
    } TestStringStruct;

    // Test CPLGetValueType
    template<>
    template<>
    void object::test<2>()
    {
        TestStringStruct apszTestStrings[] =
        {
            { "+25.e+3", CPL_VALUE_REAL },
            { "-25.e-3", CPL_VALUE_REAL },
            { "25.e3", CPL_VALUE_REAL },
            { "25e3", CPL_VALUE_REAL },
            { " 25e3 ", CPL_VALUE_REAL },
            { ".1e3", CPL_VALUE_REAL },

            { "25", CPL_VALUE_INTEGER },
            { "-25", CPL_VALUE_INTEGER },
            { "+25", CPL_VALUE_INTEGER },

            { "25e 3", CPL_VALUE_STRING },
            { "25e.3", CPL_VALUE_STRING },
            { "-2-5e3", CPL_VALUE_STRING },
            { "2-5e3", CPL_VALUE_STRING },
            { "25.25.3", CPL_VALUE_STRING },
            { "25e25e3", CPL_VALUE_STRING },
            { "25e2500", CPL_VALUE_STRING }, /* #6128 */

            { "d1", CPL_VALUE_STRING } /* #6305 */
        };

        size_t i;
        for(i=0;i < sizeof(apszTestStrings) / sizeof(apszTestStrings[0]); i++)
        {
            ensure(CPLGetValueType(apszTestStrings[i].testString) == apszTestStrings[i].expectedResult);
            if (CPLGetValueType(apszTestStrings[i].testString) != apszTestStrings[i].expectedResult)
                fprintf(stderr, "mismatch on item %d : value=\"%s\", expect_result=%d, result=%d\n", (int)i,
                        apszTestStrings[i].testString,
                        apszTestStrings[i].expectedResult,
                        CPLGetValueType(apszTestStrings[i].testString));
        }
    }


    // Test cpl_hash_set API
    template<>
    template<>
    void object::test<3>()
    {
        CPLHashSet* set = CPLHashSetNew(CPLHashSetHashStr, CPLHashSetEqualStr, CPLFree);
        ensure(CPLHashSetInsert(set, CPLStrdup("hello")) == TRUE);
        ensure(CPLHashSetInsert(set, CPLStrdup("good morning")) == TRUE);
        ensure(CPLHashSetInsert(set, CPLStrdup("bye bye")) == TRUE);
        ensure(CPLHashSetSize(set) == 3);
        ensure(CPLHashSetInsert(set, CPLStrdup("bye bye")) == FALSE);
        ensure(CPLHashSetSize(set) == 3);
        ensure(CPLHashSetRemove(set, "bye bye") == TRUE);
        ensure(CPLHashSetSize(set) == 2);
        ensure(CPLHashSetRemove(set, "good afternoon") == FALSE);
        ensure(CPLHashSetSize(set) == 2);
        CPLHashSetDestroy(set);
    }

    static int sumValues(void* elt, void* user_data)
    {
        int* pnSum = (int*)user_data;
        *pnSum += *(int*)elt;
        return TRUE;
    }

    // Test cpl_hash_set API
    template<>
    template<>
    void object::test<4>()
    {
        const int HASH_SET_SIZE = 1000;

        int data[HASH_SET_SIZE];
        for(int i=0; i<HASH_SET_SIZE; ++i)
        {
          data[i] = i;
        }

        CPLHashSet* set = CPLHashSetNew(nullptr, nullptr, nullptr);
        for(int i=0;i<HASH_SET_SIZE;i++)
        {
            ensure(CPLHashSetInsert(set, (void*)&data[i]) == TRUE);
        }
        ensure(CPLHashSetSize(set) == HASH_SET_SIZE);

        for(int i=0;i<HASH_SET_SIZE;i++)
        {
            ensure(CPLHashSetInsert(set, (void*)&data[i]) == FALSE);
        }
        ensure(CPLHashSetSize(set) == HASH_SET_SIZE);

        for(int i=0;i<HASH_SET_SIZE;i++)
        {
            ensure(CPLHashSetLookup(set, (const void*)&data[i]) == (const void*)&data[i]);
        }

        int sum = 0;
        CPLHashSetForeach(set, sumValues, &sum);
        ensure(sum == (HASH_SET_SIZE-1) * HASH_SET_SIZE / 2);

        for(int i=0;i<HASH_SET_SIZE;i++)
        {
            ensure(CPLHashSetRemove(set, (void*)&data[i]) == TRUE);
        }
        ensure(CPLHashSetSize(set) == 0);

        CPLHashSetDestroy(set);
    }

    // Test cpl_string API
    template<>
    template<>
    void object::test<5>()
    {
        // CSLTokenizeString2();
        char    **papszStringList;

        papszStringList = CSLTokenizeString2("one two three", " ", 0);
        ensure(CSLCount(papszStringList) == 3);
        ensure(EQUAL(papszStringList[0], "one"));
        ensure(EQUAL(papszStringList[1], "two"));
        ensure(EQUAL(papszStringList[2], "three"));
        CSLDestroy(papszStringList);

        papszStringList = CSLTokenizeString2("one two, three;four,five; six", " ;,", 0);
        ensure(CSLCount(papszStringList) == 6);
        ensure(EQUAL(papszStringList[0], "one"));
        ensure(EQUAL(papszStringList[1], "two"));
        ensure(EQUAL(papszStringList[2], "three"));
        ensure(EQUAL(papszStringList[3], "four"));
        ensure(EQUAL(papszStringList[4], "five"));
        ensure(EQUAL(papszStringList[5], "six"));
        CSLDestroy(papszStringList);

        papszStringList = CSLTokenizeString2("one two,,,five,six", " ,",
                                             CSLT_ALLOWEMPTYTOKENS);
        ensure(CSLCount(papszStringList) == 6);
        ensure(EQUAL(papszStringList[0], "one"));
        ensure(EQUAL(papszStringList[1], "two"));
        ensure(EQUAL(papszStringList[2], ""));
        ensure(EQUAL(papszStringList[3], ""));
        ensure(EQUAL(papszStringList[4], "five"));
        ensure(EQUAL(papszStringList[5], "six"));
        CSLDestroy(papszStringList);

        papszStringList = CSLTokenizeString2("one two,\"three,four ,\",five,six", " ,",
                                             CSLT_HONOURSTRINGS);
        ensure(CSLCount(papszStringList) == 5);
        ensure(EQUAL(papszStringList[0], "one"));
        ensure(EQUAL(papszStringList[1], "two"));
        ensure(EQUAL(papszStringList[2], "three,four ,"));
        ensure(EQUAL(papszStringList[3], "five"));
        ensure(EQUAL(papszStringList[4], "six"));
        CSLDestroy(papszStringList);

        papszStringList = CSLTokenizeString2("one two,\"three,four ,\",five,six", " ,",
                                             CSLT_PRESERVEQUOTES);
        ensure(CSLCount(papszStringList) == 7);
        ensure(EQUAL(papszStringList[0], "one"));
        ensure(EQUAL(papszStringList[1], "two"));
        ensure(EQUAL(papszStringList[2], "\"three"));
        ensure(EQUAL(papszStringList[3], "four"));
        ensure(EQUAL(papszStringList[4], "\""));
        ensure(EQUAL(papszStringList[5], "five"));
        ensure(EQUAL(papszStringList[6], "six"));
        CSLDestroy(papszStringList);

        papszStringList = CSLTokenizeString2("one two,\"three,four ,\",five,six", " ,",
                                             CSLT_HONOURSTRINGS | CSLT_PRESERVEQUOTES);
        ensure(CSLCount(papszStringList) == 5);
        ensure(EQUAL(papszStringList[0], "one"));
        ensure(EQUAL(papszStringList[1], "two"));
        ensure(EQUAL(papszStringList[2], "\"three,four ,\""));
        ensure(EQUAL(papszStringList[3], "five"));
        ensure(EQUAL(papszStringList[4], "six"));
        CSLDestroy(papszStringList);

        papszStringList = CSLTokenizeString2("one \\two,\"three,\\four ,\",five,six",
                                             " ,", CSLT_PRESERVEESCAPES);
        ensure(CSLCount(papszStringList) == 7);
        ensure(EQUAL(papszStringList[0], "one"));
        ensure(EQUAL(papszStringList[1], "\\two"));
        ensure(EQUAL(papszStringList[2], "\"three"));
        ensure(EQUAL(papszStringList[3], "\\four"));
        ensure(EQUAL(papszStringList[4], "\""));
        ensure(EQUAL(papszStringList[5], "five"));
        ensure(EQUAL(papszStringList[6], "six"));
        CSLDestroy(papszStringList);

        papszStringList = CSLTokenizeString2("one \\two,\"three,\\four ,\",five,six",
                                             " ,",
                                             CSLT_PRESERVEQUOTES | CSLT_PRESERVEESCAPES);
        ensure(CSLCount(papszStringList) == 7);
        ensure(EQUAL(papszStringList[0], "one"));
        ensure(EQUAL(papszStringList[1], "\\two"));
        ensure(EQUAL(papszStringList[2], "\"three"));
        ensure(EQUAL(papszStringList[3], "\\four"));
        ensure(EQUAL(papszStringList[4], "\""));
        ensure(EQUAL(papszStringList[5], "five"));
        ensure(EQUAL(papszStringList[6], "six"));
        CSLDestroy(papszStringList);

        papszStringList = CSLTokenizeString2("one ,two, three, four ,five  ", ",", 0);
        ensure(CSLCount(papszStringList) == 5);
        ensure(EQUAL(papszStringList[0], "one "));
        ensure(EQUAL(papszStringList[1], "two"));
        ensure(EQUAL(papszStringList[2], " three"));
        ensure(EQUAL(papszStringList[3], " four "));
        ensure(EQUAL(papszStringList[4], "five  "));
        CSLDestroy(papszStringList);

        papszStringList = CSLTokenizeString2("one ,two, three, four ,five  ", ",",
                                             CSLT_STRIPLEADSPACES);
        ensure(CSLCount(papszStringList) == 5);
        ensure(EQUAL(papszStringList[0], "one "));
        ensure(EQUAL(papszStringList[1], "two"));
        ensure(EQUAL(papszStringList[2], "three"));
        ensure(EQUAL(papszStringList[3], "four "));
        ensure(EQUAL(papszStringList[4], "five  "));
        CSLDestroy(papszStringList);

        papszStringList = CSLTokenizeString2("one ,two, three, four ,five  ", ",",
                                             CSLT_STRIPENDSPACES);
        ensure(CSLCount(papszStringList) == 5);
        ensure(EQUAL(papszStringList[0], "one"));
        ensure(EQUAL(papszStringList[1], "two"));
        ensure(EQUAL(papszStringList[2], " three"));
        ensure(EQUAL(papszStringList[3], " four"));
        ensure(EQUAL(papszStringList[4], "five"));
        CSLDestroy(papszStringList);

        papszStringList = CSLTokenizeString2("one ,two, three, four ,five  ", ",",
                                             CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES);
        ensure(CSLCount(papszStringList) == 5);
        ensure(EQUAL(papszStringList[0], "one"));
        ensure(EQUAL(papszStringList[1], "two"));
        ensure(EQUAL(papszStringList[2], "three"));
        ensure(EQUAL(papszStringList[3], "four"));
        ensure(EQUAL(papszStringList[4], "five"));
        CSLDestroy(papszStringList);
    }

    typedef struct
    {
        char szEncoding[24];
        char szString[1024 - 24];
    } TestRecodeStruct;

    // Test cpl_recode API
    template<>
    template<>
    void object::test<6>()
    {
        /*
         * NOTE: This test will generally fail if iconv() is not
         *       linked in.
         *
         * CPLRecode() will be tested using the test file containing
         * a list of strings of the same text in different encoding. The
         * string is non-ASCII to avoid trivial transformations. Test file
         * has a simple binary format: a table of records, each record
         * is 1024 bytes long. The first 24 bytes of each record contain
         * encoding name (ASCII, zero padded), the last 1000 bytes contain
         * encoded string, zero padded.
         *
         * NOTE 1: We can't use a test file in human readable text format
         *         here because of multiple different encodings including
         *         multibyte ones.
         *
         * The test file could be generated with the following simple shell
         * script:
         *
         * #!/bin/sh
         *
         * # List of encodings to convert the test string into
         * ENCODINGS="UTF-8 CP1251 KOI8-R UCS-2 UCS-2BE UCS-2LE UCS-4 UCS-4BE UCS-4LE UTF-16 UTF-32"
         * # The test string itself in UTF-8 encoding.
         * # This means "Improving GDAL internationalization." in Russian.
         * TESTSTRING="\u0423\u043b\u0443\u0447\u0448\u0430\u0435\u043c \u0438\u043d\u0442\u0435\u0440\u043d\u0430\u0446\u0438\u043e\u043d\u0430\u043b\u0438\u0437\u0430\u0446\u0438\u044e GDAL."
         *
         * RECORDSIZE=1024
         * ENCSIZE=24
         *
         * i=0
         * for enc in ${ENCODINGS}; do
         *  env printf "${enc}" | dd ibs=${RECORDSIZE} conv=sync obs=1 seek=$((${RECORDSIZE}*${i})) of="recode-rus.dat" status=noxfer
         *  env printf "${TESTSTRING}" | iconv -t ${enc} | dd ibs=${RECORDSIZE} conv=sync obs=1 seek=$((${RECORDSIZE}*${i}+${ENCSIZE})) of="recode-rus.dat" status=noxfer
         *  i=$((i+1))
         * done
         *
         * NOTE 2: The test string is encoded with the special format
         *         "\uXXXX" sequences, so we able to paste it here.
         *
         * NOTE 3: We need a printf utility from the coreutils because of
         *         that. "env printf" should work avoiding the shell
         *         built-in.
         *
         * NOTE 4: "iconv" utility without the "-f" option will work with
         *         encoding read from the current locale.
         *
         *  TODO: 1. Add more encodings maybe more test files.
         *        2. Add test for CPLRecodeFromWChar()/CPLRecodeToWChar().
         *        3. Test translation between each possible pair of
         *        encodings in file, not only into the UTF-8.
         */

        std::ifstream fin((data_ + SEP + "recode-rus.dat").c_str(),
                          std::ifstream::binary);
        TestRecodeStruct oReferenceString;

        // Read reference string (which is the first one in the file)
        fin.read(oReferenceString.szEncoding,
                 sizeof(oReferenceString.szEncoding));
        oReferenceString.szEncoding[sizeof(oReferenceString.szEncoding) - 1] = '\0';
        fin.read(oReferenceString.szString,
                 sizeof(oReferenceString.szString));
        oReferenceString.szString[sizeof(oReferenceString.szString) - 1] = '\0';

        while (true)
        {
            TestRecodeStruct oTestString;

            fin.read(oTestString.szEncoding, sizeof(oTestString.szEncoding));
            oTestString.szEncoding[sizeof(oTestString.szEncoding) - 1] = '\0';
            if( fin.eof() )
                break;
            fin.read(oTestString.szString, sizeof(oTestString.szString));
            oTestString.szString[sizeof(oTestString.szString) - 1] = '\0';

            // Compare each string with the reference one
            CPLErrorReset();
            char    *pszDecodedString = CPLRecode( oTestString.szString,
                oTestString.szEncoding, oReferenceString.szEncoding);
            if( strstr(CPLGetLastErrorMsg(), "Recode from CP1251 to UTF-8 not supported") != nullptr ||
                strstr(CPLGetLastErrorMsg(), "Recode from KOI8-R to UTF-8 not supported") != nullptr )
            {
                CPLFree( pszDecodedString );
                break;
            }

            size_t  nLength =
                MIN( strlen(pszDecodedString),
                     sizeof(oReferenceString.szEncoding) );
            bool bOK = (memcmp(pszDecodedString, oReferenceString.szString,
                           nLength) == 0);
            // FIXME Some tests fail on Mac. Not sure why, but do not error out just for that
            if( !bOK && (strstr(CPLGetConfigOption("TRAVIS_OS_NAME", ""), "osx") != nullptr ||
                         strstr(CPLGetConfigOption("BUILD_NAME", ""), "osx") != nullptr ||
                         getenv("DO_NOT_FAIL_ON_RECODE_ERRORS") != nullptr))
            {
                fprintf(stderr, "Recode from %s failed\n", oTestString.szEncoding);
            }
            else
            {
                ensure( std::string("Recode from ") + oTestString.szEncoding, bOK );
            }
            CPLFree( pszDecodedString );
        }

        fin.close();
    }

/************************************************************************/
/*                         CPLStringList tests                          */
/************************************************************************/
    template<>
    template<>
    void object::test<7>()
    {
        CPLStringList  oCSL;

        ensure( "7nil", oCSL.List() == nullptr );

        oCSL.AddString( "def" );
        oCSL.AddString( "abc" );

        ensure_equals( "7", oCSL.Count(), 2 );
        ensure( "70", EQUAL(oCSL[0], "def") );
        ensure( "71", EQUAL(oCSL[1], "abc") );
        ensure( "72", oCSL[17] == nullptr );
        ensure( "73", oCSL[-1] == nullptr );
        ensure_equals( "74", oCSL.FindString("abc"), 1 );

        CSLDestroy( oCSL.StealList() );
        ensure_equals( "75", oCSL.Count(), 0 );
        ensure( "76", oCSL.List() == nullptr );

        // Test that the list will make an internal copy when needed to
        // modify a read-only list.

        oCSL.AddString( "def" );
        oCSL.AddString( "abc" );

        CPLStringList  oCopy( oCSL.List(), FALSE );

        ensure_equals( "77", oCSL.List(), oCopy.List() );
        ensure_equals( "78", oCSL.Count(), oCopy.Count() );

        oCopy.AddString( "xyz" );
        ensure( "79", oCSL.List() != oCopy.List() );
        ensure_equals( "7a", oCopy.Count(), 3 );
        ensure_equals( "7b", oCSL.Count(), 2 );
        ensure( "7c", EQUAL(oCopy[2], "xyz") );
    }

    template<>
    template<>
    void object::test<8>()
    {
        // Test some name=value handling stuff.
        CPLStringList oNVL;

        oNVL.AddNameValue( "KEY1", "VALUE1" );
        oNVL.AddNameValue( "2KEY", "VALUE2" );
        ensure_equals( oNVL.Count(), 2 );
        ensure( EQUAL(oNVL.FetchNameValue("2KEY"),"VALUE2") );
        ensure( oNVL.FetchNameValue("MISSING") == nullptr );

        oNVL.AddNameValue( "KEY1", "VALUE3" );
        ensure( EQUAL(oNVL.FetchNameValue("KEY1"),"VALUE1") );
        ensure( EQUAL(oNVL[2],"KEY1=VALUE3") );
        ensure( EQUAL(oNVL.FetchNameValueDef("MISSING","X"),"X") );

        oNVL.SetNameValue( "2KEY", "VALUE4" );
        ensure( EQUAL(oNVL.FetchNameValue("2KEY"),"VALUE4") );
        ensure_equals( oNVL.Count(), 3 );

        // make sure deletion works.
        oNVL.SetNameValue( "2KEY", nullptr );
        ensure( oNVL.FetchNameValue("2KEY") == nullptr );
        ensure_equals( oNVL.Count(), 2 );

        // Test boolean support.
        ensure_equals( "b1", oNVL.FetchBoolean( "BOOL", TRUE ), TRUE );
        ensure_equals( "b2", oNVL.FetchBoolean( "BOOL", FALSE ), FALSE );

        oNVL.SetNameValue( "BOOL", "YES" );
        ensure_equals( "b3", oNVL.FetchBoolean( "BOOL", TRUE ), TRUE );
        ensure_equals( "b4", oNVL.FetchBoolean( "BOOL", FALSE ), TRUE );

        oNVL.SetNameValue( "BOOL", "1" );
        ensure_equals( "b5", oNVL.FetchBoolean( "BOOL", FALSE ), TRUE );

        oNVL.SetNameValue( "BOOL", "0" );
        ensure_equals( "b6", oNVL.FetchBoolean( "BOOL", TRUE ), FALSE );

        oNVL.SetNameValue( "BOOL", "FALSE" );
        ensure_equals( "b7", oNVL.FetchBoolean( "BOOL", TRUE ), FALSE );

        oNVL.SetNameValue( "BOOL", "ON" );
        ensure_equals( "b8", oNVL.FetchBoolean( "BOOL", FALSE ), TRUE );

        // Test assignment operator.
        CPLStringList oCopy;

        {
            CPLStringList oTemp;
            oTemp.AddString("test");
            oCopy = oTemp;
        }
        ensure( "c1", EQUAL(oCopy[0],"test") );

        auto& oCopyRef(oCopy);
        oCopy = oCopyRef;
        ensure( "c2", EQUAL(oCopy[0],"test") );

        // Test copy constructor.
        CPLStringList oCopy2(oCopy);
        oCopy.Clear();
        ensure( "c3", EQUAL(oCopy2[0],"test") );

        // Test sorting
        CPLStringList oTestSort;
        oTestSort.AddNameValue("Z", "1");
        oTestSort.AddNameValue("L", "2");
        oTestSort.AddNameValue("T", "3");
        oTestSort.AddNameValue("A", "4");
        oTestSort.Sort();
        ensure( "c4", EQUAL(oTestSort[0],"A=4") );
        ensure( "c5", EQUAL(oTestSort[1],"L=2") );
        ensure( "c6", EQUAL(oTestSort[2],"T=3") );
        ensure( "c7", EQUAL(oTestSort[3],"Z=1") );
        ensure_equals( "c8", oTestSort[4], (const char*)nullptr );

        // Test FetchNameValue() in a sorted list
        ensure( "c9", EQUAL(oTestSort.FetchNameValue("A"),"4") );
        ensure( "c10", EQUAL(oTestSort.FetchNameValue("L"),"2") );
        ensure( "c11", EQUAL(oTestSort.FetchNameValue("T"),"3") );
        ensure( "c12", EQUAL(oTestSort.FetchNameValue("Z"),"1") );

        // Test AddNameValue() in a sorted list
        oTestSort.AddNameValue("B", "5");
        ensure( "c13", EQUAL(oTestSort[0],"A=4") );
        ensure( "c14", EQUAL(oTestSort[1],"B=5") );
        ensure( "c15", EQUAL(oTestSort[2],"L=2") );
        ensure( "c16", EQUAL(oTestSort[3],"T=3") );
        ensure( "c17", EQUAL(oTestSort[4],"Z=1") );
        ensure_equals( "c18", oTestSort[5], (const char*)nullptr );

        // Test SetNameValue() of an existing item in a sorted list
        oTestSort.SetNameValue("Z", "6");
        ensure( "c19", EQUAL(oTestSort[4],"Z=6") );

        // Test SetNameValue() of a non-existing item in a sorted list
        oTestSort.SetNameValue("W", "7");
        ensure( "c20", EQUAL(oTestSort[0],"A=4") );
        ensure( "c21", EQUAL(oTestSort[1],"B=5") );
        ensure( "c22", EQUAL(oTestSort[2],"L=2") );
        ensure( "c23", EQUAL(oTestSort[3],"T=3") );
        ensure( "c24", EQUAL(oTestSort[4],"W=7") );
        ensure( "c25", EQUAL(oTestSort[5],"Z=6") );
        ensure_equals( "c26", oTestSort[6], (const char*)nullptr );
    }

    template<>
    template<>
    void object::test<9>()
    {
        // Test some name=value handling stuff *with* sorting active.
        CPLStringList oNVL;

        oNVL.Sort();

        oNVL.AddNameValue( "KEY1", "VALUE1" );
        oNVL.AddNameValue( "2KEY", "VALUE2" );
        ensure_equals( "91", oNVL.Count(), 2 );
        ensure( "92", EQUAL(oNVL.FetchNameValue("KEY1"),"VALUE1") );
        ensure( "93", EQUAL(oNVL.FetchNameValue("2KEY"),"VALUE2") );
        ensure( "94", oNVL.FetchNameValue("MISSING") == nullptr );

        oNVL.AddNameValue( "KEY1", "VALUE3" );
        ensure_equals( "95", oNVL.Count(), 3 );
        ensure( "96", EQUAL(oNVL.FetchNameValue("KEY1"),"VALUE1") );
        ensure( "97", EQUAL(oNVL.FetchNameValueDef("MISSING","X"),"X") );

        oNVL.SetNameValue( "2KEY", "VALUE4" );
        ensure( "98", EQUAL(oNVL.FetchNameValue("2KEY"),"VALUE4") );
        ensure_equals( "99", oNVL.Count(), 3 );

        // make sure deletion works.
        oNVL.SetNameValue( "2KEY", nullptr );
        ensure( "9a", oNVL.FetchNameValue("2KEY") == nullptr );
        ensure_equals( "9b", oNVL.Count(), 2 );

        // Test insertion logic pretty carefully.
        oNVL.Clear();
        ensure( "9c", oNVL.IsSorted() == TRUE );

        oNVL.SetNameValue( "B", "BB" );
        oNVL.SetNameValue( "A", "AA" );
        oNVL.SetNameValue( "D", "DD" );
        oNVL.SetNameValue( "C", "CC" );

        // items should be in sorted order.
        ensure( "9c1", EQUAL(oNVL[0],"A=AA") );
        ensure( "9c2", EQUAL(oNVL[1],"B=BB") );
        ensure( "9c3", EQUAL(oNVL[2],"C=CC") );
        ensure( "9c4", EQUAL(oNVL[3],"D=DD") );

        ensure( "9d", EQUAL(oNVL.FetchNameValue("A"),"AA") );
        ensure( "9e", EQUAL(oNVL.FetchNameValue("B"),"BB") );
        ensure( "9f", EQUAL(oNVL.FetchNameValue("C"),"CC") );
        ensure( "9g", EQUAL(oNVL.FetchNameValue("D"),"DD") );
    }

    template<>
    template<>
    void object::test<10>()
    {
        GByte abyDigest[CPL_SHA256_HASH_SIZE];
        char szDigest[2*CPL_SHA256_HASH_SIZE+1];

        CPL_HMAC_SHA256("key", 3,
                        "The quick brown fox jumps over the lazy dog", strlen("The quick brown fox jumps over the lazy dog"),
                        abyDigest);
        for(int i=0;i<CPL_SHA256_HASH_SIZE;i++)
            snprintf(szDigest + 2 * i, sizeof(szDigest)-2*i, "%02x", abyDigest[i]);
        //fprintf(stderr, "%s\n", szDigest);
        ensure( "10.1", EQUAL(szDigest, "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8") );


        CPL_HMAC_SHA256("mysupersupersupersupersupersupersupersupersupersupersupersupersupersupersupersupersupersupersupersuperlongkey",
                        strlen("mysupersupersupersupersupersupersupersupersupersupersupersupersupersupersupersupersupersupersupersuperlongkey"),
                        "msg", 3,
                        abyDigest);
        for(int i=0;i<CPL_SHA256_HASH_SIZE;i++)
            snprintf(szDigest + 2 * i, sizeof(szDigest)-2*i, "%02x", abyDigest[i]);
        //fprintf(stderr, "%s\n", szDigest);
        ensure( "10.2", EQUAL(szDigest, "a3051520761ed3cb43876b35ce2dd93ac5b332dc3bad898bb32086f7ac71ffc1") );
    }

    template<>
    template<>
    void object::test<11>()
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);

        // The following tests will fail because of overflows
        CPLErrorReset();
        ensure( "11.1", VSIMalloc2( ~(size_t)0, ~(size_t)0 ) == nullptr );
        ensure( "11.1bis", CPLGetLastErrorType() != CE_None );

        CPLErrorReset();
        ensure( "11.2", VSIMalloc3( 1, ~(size_t)0, ~(size_t)0 ) == nullptr );
        ensure( "11.2bis", CPLGetLastErrorType() != CE_None );

        CPLErrorReset();
        ensure( "11.3", VSIMalloc3( ~(size_t)0, 1, ~(size_t)0 ) == nullptr );
        ensure( "11.3bis", CPLGetLastErrorType() != CE_None );

        CPLErrorReset();
        ensure( "11.4", VSIMalloc3( ~(size_t)0, ~(size_t)0, 1 ) == nullptr );
        ensure( "11.4bis", CPLGetLastErrorType() != CE_None );

        if( !CSLTestBoolean(CPLGetConfigOption("SKIP_MEM_INTENSIVE_TEST", "NO")) )
        {
            // The following tests will fail because such allocations cannot succeed
#if SIZEOF_VOIDP == 8
            CPLErrorReset();
            ensure( "11.6", VSIMalloc( ~(size_t)0 ) == nullptr );
            ensure( "11.6bis", CPLGetLastErrorType() == CE_None ); /* no error reported */

            CPLErrorReset();
            ensure( "11.7", VSIMalloc2( ~(size_t)0, 1 ) == nullptr );
            ensure( "11.7bis", CPLGetLastErrorType() != CE_None );

            CPLErrorReset();
            ensure( "11.8", VSIMalloc3( ~(size_t)0, 1, 1 ) == nullptr );
            ensure( "11.8bis", CPLGetLastErrorType() != CE_None );

            CPLErrorReset();
            ensure( "11.9", VSICalloc( ~(size_t)0, 1 ) == nullptr );
            ensure( "11.9bis", CPLGetLastErrorType() == CE_None ); /* no error reported */

            CPLErrorReset();
            ensure( "11.10", VSIRealloc( nullptr, ~(size_t)0 ) == nullptr );
            ensure( "11.10bis", CPLGetLastErrorType() == CE_None ); /* no error reported */

            CPLErrorReset();
            ensure( "11.11", VSI_MALLOC_VERBOSE( ~(size_t)0 ) == nullptr );
            ensure( "11.11bis", CPLGetLastErrorType() != CE_None );

            CPLErrorReset();
            ensure( "11.12", VSI_MALLOC2_VERBOSE( ~(size_t)0, 1 ) == nullptr );
            ensure( "11.12bis", CPLGetLastErrorType() != CE_None );

            CPLErrorReset();
            ensure( "11.13", VSI_MALLOC3_VERBOSE( ~(size_t)0, 1, 1 ) == nullptr );
            ensure( "11.13bis", CPLGetLastErrorType() != CE_None );

            CPLErrorReset();
            ensure( "11.14", VSI_CALLOC_VERBOSE( ~(size_t)0, 1 ) == nullptr );
            ensure( "11.14bis", CPLGetLastErrorType() != CE_None );

            CPLErrorReset();
            ensure( "11.15", VSI_REALLOC_VERBOSE( nullptr, ~(size_t)0 ) == nullptr );
            ensure( "11.15bis", CPLGetLastErrorType() != CE_None );
#endif
        }

        CPLPopErrorHandler();

        // The following allocs will return NULL because of 0 byte alloc
        CPLErrorReset();
        ensure( "11.16", VSIMalloc2( 0, 1 ) == nullptr );
        ensure( "11.16bis", CPLGetLastErrorType() == CE_None );
        ensure( "11.17", VSIMalloc2( 1, 0 ) == nullptr );

        CPLErrorReset();
        ensure( "11.18", VSIMalloc3( 0, 1, 1 ) == nullptr );
        ensure( "11.18bis", CPLGetLastErrorType() == CE_None );
        ensure( "11.19", VSIMalloc3( 1, 0, 1 ) == nullptr );
        ensure( "11.20", VSIMalloc3( 1, 1, 0 ) == nullptr );
    }

    template<>
    template<>
    void object::test<12>()
    {
        ensure( strcmp(CPLFormFilename("a", "b", nullptr), "a/b") == 0 ||
                strcmp(CPLFormFilename("a", "b", nullptr), "a\\b") == 0 );
        ensure( strcmp(CPLFormFilename("a/", "b", nullptr), "a/b") == 0 ||
                strcmp(CPLFormFilename("a/", "b", nullptr), "a\\b") == 0 );
        ensure( strcmp(CPLFormFilename("a\\", "b", nullptr), "a/b") == 0 ||
                strcmp(CPLFormFilename("a\\", "b", nullptr), "a\\b") == 0 );
        ensure_equals( CPLFormFilename(nullptr, "a", "b"), "a.b");
        ensure_equals( CPLFormFilename(nullptr, "a", ".b"), "a.b");
        ensure_equals( CPLFormFilename("/a", "..", nullptr), "/");
        ensure_equals( CPLFormFilename("/a/", "..", nullptr), "/");
        ensure_equals( CPLFormFilename("/a/b", "..", nullptr), "/a");
        ensure_equals( CPLFormFilename("/a/b/", "..", nullptr), "/a");
        ensure( EQUAL(CPLFormFilename("c:", "..", nullptr), "c:/..") ||
                EQUAL(CPLFormFilename("c:", "..", nullptr), "c:\\..") );
        ensure( EQUAL(CPLFormFilename("c:\\", "..", nullptr), "c:/..") ||
                EQUAL(CPLFormFilename("c:\\", "..", nullptr), "c:\\..") );
        ensure_equals( CPLFormFilename("c:\\a", "..", nullptr), "c:");
        ensure_equals( CPLFormFilename("c:\\a\\", "..", nullptr), "c:");
        ensure_equals( CPLFormFilename("c:\\a\\b", "..", nullptr), "c:\\a");
        ensure_equals( CPLFormFilename("\\\\$\\c:\\a", "..", nullptr), "\\\\$\\c:");
        ensure( EQUAL(CPLFormFilename("\\\\$\\c:", "..", nullptr), "\\\\$\\c:/..") ||
                EQUAL(CPLFormFilename("\\\\$\\c:", "..", nullptr), "\\\\$\\c:\\..") );
    }

    template<>
    template<>
    void object::test<13>()
    {
        ensure( VSIGetDiskFreeSpace("/vsimem/") > 0 );
        ensure( VSIGetDiskFreeSpace(".") == -1 || VSIGetDiskFreeSpace(".") >= 0 );
    }

    template<>
    template<>
    void object::test<14>()
    {
        double a, b, c;

        a = b = 0;
        ensure_equals( CPLsscanf("1 2", "%lf %lf", &a, &b), 2 );
        ensure_equals( a, 1.0 );
        ensure_equals( b, 2.0 );

        a = b = 0;
        ensure_equals( CPLsscanf("1\t2", "%lf %lf", &a, &b), 2 );
        ensure_equals( a, 1.0 );
        ensure_equals( b, 2.0 );

        a = b = 0;
        ensure_equals( CPLsscanf("1 2", "%lf\t%lf", &a, &b), 2 );
        ensure_equals( a, 1.0 );
        ensure_equals( b, 2.0 );

        a = b = 0;
        ensure_equals( CPLsscanf("1  2", "%lf %lf", &a, &b), 2 );
        ensure_equals( a, 1.0 );
        ensure_equals( b, 2.0 );

        a = b = 0;
        ensure_equals( CPLsscanf("1 2", "%lf  %lf", &a, &b), 2 );
        ensure_equals( a, 1.0 );
        ensure_equals( b, 2.0 );

        a = b = c = 0;
        ensure_equals( CPLsscanf("1 2", "%lf %lf %lf", &a, &b, &c), 2 );
        ensure_equals( a, 1.0 );
        ensure_equals( b, 2.0 );
    }

    template<>
    template<>
    void object::test<15>()
    {
        CPLString oldVal = CPLGetConfigOption("CPL_DEBUG", "");
        CPLSetConfigOption("CPL_DEBUG", "TEST");

        CPLErrorHandler oldHandler = CPLSetErrorHandler(myErrorHandler);
        gbGotError = false;
        CPLDebug("TEST", "Test");
        ensure_equals( gbGotError, true );
        gbGotError = false;
        CPLSetErrorHandler(oldHandler);

        CPLPushErrorHandler(myErrorHandler);
        gbGotError = false;
        CPLDebug("TEST", "Test");
        ensure_equals( gbGotError, true );
        gbGotError = false;
        CPLPopErrorHandler();

        oldHandler = CPLSetErrorHandler(myErrorHandler);
        CPLSetCurrentErrorHandlerCatchDebug( FALSE );
        gbGotError = false;
        CPLDebug("TEST", "Test");
        ensure_equals( gbGotError, false );
        gbGotError = false;
        CPLSetErrorHandler(oldHandler);

        CPLPushErrorHandler(myErrorHandler);
        CPLSetCurrentErrorHandlerCatchDebug( FALSE );
        gbGotError = false;
        CPLDebug("TEST", "Test");
        ensure_equals( gbGotError, false );
        gbGotError = false;
        CPLPopErrorHandler();

        CPLSetConfigOption("CPL_DEBUG", oldVal.size() ? oldVal.c_str() : nullptr);

        oldHandler = CPLSetErrorHandler(nullptr);
        CPLDebug("TEST", "Test");
        CPLError(CE_Failure, CPLE_AppDefined, "test");
        CPLErrorHandler newOldHandler = CPLSetErrorHandler(nullptr);
        ensure_equals(newOldHandler, static_cast<CPLErrorHandler>(nullptr));
        CPLDebug("TEST", "Test");
        CPLError(CE_Failure, CPLE_AppDefined, "test");
        CPLSetErrorHandler(oldHandler);
    }

/************************************************************************/
/*                         CPLString::replaceAll()                      */
/************************************************************************/
    template<>
    template<>
    void object::test<16>()
    {
        CPLString osTest;
        osTest = "foobarbarfoo";
        osTest.replaceAll("bar", "was_bar");
        ensure_equals( osTest, "foowas_barwas_barfoo" );

        osTest = "foobarbarfoo";
        osTest.replaceAll("X", "was_bar");
        ensure_equals( osTest, "foobarbarfoo" );

        osTest = "foobarbarfoo";
        osTest.replaceAll("", "was_bar");
        ensure_equals( osTest, "foobarbarfoo" );

        osTest = "foobarbarfoo";
        osTest.replaceAll("bar", "");
        ensure_equals( osTest, "foofoo" );

        osTest = "foobarbarfoo";
        osTest.replaceAll('b', 'B');
        ensure_equals( osTest, "fooBarBarfoo" );

        osTest = "foobarbarfoo";
        osTest.replaceAll('b', "B");
        ensure_equals( osTest, "fooBarBarfoo" );

        osTest = "foobarbarfoo";
        osTest.replaceAll("b", 'B');
        ensure_equals( osTest, "fooBarBarfoo" );
    }

/************************************************************************/
/*                        VSIMallocAligned()                            */
/************************************************************************/
    template<>
    template<>
    void object::test<17>()
    {
        GByte* ptr = static_cast<GByte*>(VSIMallocAligned(sizeof(void*), 1));
        ensure( ptr != nullptr );
        ensure( ((size_t)ptr % sizeof(void*)) == 0 );
        *ptr = 1;
        VSIFreeAligned(ptr);

        ptr = static_cast<GByte*>(VSIMallocAligned(16, 1));
        ensure( ptr != nullptr );
        ensure( ((size_t)ptr % 16) == 0 );
        *ptr = 1;
        VSIFreeAligned(ptr);

        VSIFreeAligned(nullptr);

#ifndef WIN32
        // Illegal use of API. Returns non NULL on Windows
        ptr = static_cast<GByte*>(VSIMallocAligned(2, 1));
        ensure( ptr == nullptr );

        // Illegal use of API. Crashes on Windows
        ptr = static_cast<GByte*>(VSIMallocAligned(5, 1));
        ensure( ptr == nullptr );
#endif

        if( !CSLTestBoolean(CPLGetConfigOption("SKIP_MEM_INTENSIVE_TEST", "NO")) )
        {
            // The following tests will fail because such allocations cannot succeed
#if SIZEOF_VOIDP == 8
            ptr = static_cast<GByte*>(VSIMallocAligned(sizeof(void*), ~((size_t)0)));
            ensure( ptr == nullptr );

            ptr = static_cast<GByte*>(VSIMallocAligned(sizeof(void*), (~((size_t)0)) - sizeof(void*)));
            ensure( ptr == nullptr );
#endif
        }
    }

/************************************************************************/
/*             CPLGetConfigOptions() / CPLSetConfigOptions()            */
/************************************************************************/
    template<>
    template<>
    void object::test<18>()
    {
        CPLSetConfigOption("FOOFOO", "BAR");
        char** options = CPLGetConfigOptions();
        ensure_equals (CSLFetchNameValue(options, "FOOFOO"), "BAR");
        CPLSetConfigOptions(nullptr);
        ensure_equals (CPLGetConfigOption("FOOFOO", "i_dont_exist"), "i_dont_exist");
        CPLSetConfigOptions(options);
        ensure_equals (CPLGetConfigOption("FOOFOO", "i_dont_exist"), "BAR");
        CSLDestroy(options);
    }

/************************************************************************/
/*  CPLGetThreadLocalConfigOptions() / CPLSetThreadLocalConfigOptions() */
/************************************************************************/
    template<>
    template<>
    void object::test<19>()
    {
        CPLSetThreadLocalConfigOption("FOOFOO", "BAR");
        char** options = CPLGetThreadLocalConfigOptions();
        ensure_equals (CSLFetchNameValue(options, "FOOFOO"), "BAR");
        CPLSetThreadLocalConfigOptions(nullptr);
        ensure_equals (CPLGetThreadLocalConfigOption("FOOFOO", "i_dont_exist"), "i_dont_exist");
        CPLSetThreadLocalConfigOptions(options);
        ensure_equals (CPLGetThreadLocalConfigOption("FOOFOO", "i_dont_exist"), "BAR");
        CSLDestroy(options);
    }

    template<>
    template<>
    void object::test<20>()
    {
        ensure_equals ( CPLExpandTilde("/foo/bar"), "/foo/bar" );

        CPLSetConfigOption("HOME", "/foo");
        ensure ( EQUAL(CPLExpandTilde("~/bar"), "/foo/bar") || EQUAL(CPLExpandTilde("~/bar"), "/foo\\bar") );
        CPLSetConfigOption("HOME", nullptr);
    }

    template<>
    template<>
    void object::test<21>()
    {
        // CPLString(std::string) constructor
        ensure_equals ( CPLString(std::string("abc")).c_str(), "abc" );

        // CPLString(const char*) constructor
        ensure_equals ( CPLString("abc").c_str(), "abc" );

        // CPLString(const char*, n) constructor
        ensure_equals ( CPLString("abc",1).c_str(), "a" );
    }

    template<>
    template<>
    void object::test<22>()
    {
        // NOTE: Assumes cpl_error.cpp defines DEFAULT_LAST_ERR_MSG_SIZE=500
        char pszMsg[] =
            "0abcdefghijklmnopqrstuvwxyz0123456789!@#$%&*()_+=|"
            "1abcdefghijklmnopqrstuvwxyz0123456789!@#$%&*()_+=|"
            "2abcdefghijklmnopqrstuvwxyz0123456789!@#$%&*()_+=|"
            "3abcdefghijklmnopqrstuvwxyz0123456789!@#$%&*()_+=|"
            "4abcdefghijklmnopqrstuvwxyz0123456789!@#$%&*()_+=|"
            "5abcdefghijklmnopqrstuvwxyz0123456789!@#$%&*()_+=|"
            "6abcdefghijklmnopqrstuvwxyz0123456789!@#$%&*()_+=|"
            "7abcdefghijklmnopqrstuvwxyz0123456789!@#$%&*()_+=|"
            "8abcdefghijklmnopqrstuvwxyz0123456789!@#$%&*()_+=|"
            "9abcdefghijklmnopqrstuvwxyz0123456789!@#$%&*()_+=|" // 500
            "0abcdefghijklmnopqrstuvwxyz0123456789!@#$%&*()_+=|" // 550
            ;

        CPLErrorReset();
        CPLErrorSetState(CE_Warning, 1, pszMsg);
        ensure_equals(strlen(pszMsg) - 50 - 1,       // length - 50 - 1 (null-terminator)
                      strlen(CPLGetLastErrorMsg())); // DEFAULT_LAST_ERR_MSG_SIZE - 1
    }

    template<>
    template<>
    void object::test<23>()
    {
        char* pszText = CPLUnescapeString("&lt;&gt;&amp;&apos;&quot;&#x3f;&#x3F;&#63;", nullptr, CPLES_XML);
        ensure_equals( CPLString(pszText), "<>&'\"???");
        CPLFree(pszText);

        // Integer overflow
        pszText = CPLUnescapeString("&10000000000000000;", nullptr, CPLES_XML);
        // We do not really care about the return value
        CPLFree(pszText);

        // Integer overflow
        pszText = CPLUnescapeString("&#10000000000000000;", nullptr, CPLES_XML);
        // We do not really care about the return value
        CPLFree(pszText);

        // Error case
        pszText = CPLUnescapeString("&foo", nullptr, CPLES_XML);
        ensure_equals( CPLString(pszText), "");
        CPLFree(pszText);

        // Error case
        pszText = CPLUnescapeString("&#x", nullptr, CPLES_XML);
        ensure_equals( CPLString(pszText), "");
        CPLFree(pszText);

        // Error case
        pszText = CPLUnescapeString("&#", nullptr, CPLES_XML);
        ensure_equals( CPLString(pszText), "");
        CPLFree(pszText);
    }

    template<>
    template<>
    void object::test<24>()
    {
        // No longer used
    }


    // Test signed int safe maths
    template<>
    template<>
    void object::test<25>()
    {
        ensure_equals( (CPLSM(-2) + CPLSM(3)).v(), 1 );
        ensure_equals( (CPLSM(-2) + CPLSM(1)).v(), -1 );
        ensure_equals( (CPLSM(-2) + CPLSM(-1)).v(), -3 );
        ensure_equals( (CPLSM(2) + CPLSM(-3)).v(), -1 );
        ensure_equals( (CPLSM(2) + CPLSM(-1)).v(), 1 );
        ensure_equals( (CPLSM(2) + CPLSM(1)).v(), 3 );
        ensure_equals( (CPLSM(INT_MAX-1) + CPLSM(1)).v(), INT_MAX );
        ensure_equals( (CPLSM(1) + CPLSM(INT_MAX-1)).v(), INT_MAX );
        ensure_equals( (CPLSM(INT_MAX) + CPLSM(-1)).v(), INT_MAX - 1 );
        ensure_equals( (CPLSM(-1) + CPLSM(INT_MAX)).v(), INT_MAX - 1 );
        ensure_equals( (CPLSM(INT_MIN+1) + CPLSM(-1)).v(), INT_MIN );
        ensure_equals( (CPLSM(-1) + CPLSM(INT_MIN+1)).v(), INT_MIN );
        try { (CPLSM(INT_MAX) + CPLSM(1)).v(); ensure(false); } catch (...) {}
        try { (CPLSM(1) + CPLSM(INT_MAX)).v(); ensure(false); } catch (...) {}
        try { (CPLSM(INT_MIN) + CPLSM(-1)).v(); ensure(false); } catch (...) {}
        try { (CPLSM(-1) + CPLSM(INT_MIN)).v(); ensure(false); } catch (...) {}

        ensure_equals( (CPLSM(-2) - CPLSM(1)).v(), -3 );
        ensure_equals( (CPLSM(-2) - CPLSM(-1)).v(), -1 );
        ensure_equals( (CPLSM(-2) - CPLSM(-3)).v(), 1 );
        ensure_equals( (CPLSM(2) - CPLSM(-1)).v(), 3 );
        ensure_equals( (CPLSM(2) - CPLSM(1)).v(), 1 );
        ensure_equals( (CPLSM(2) - CPLSM(3)).v(), -1 );
        ensure_equals( (CPLSM(INT_MAX) - CPLSM(1)).v(), INT_MAX - 1 );
        ensure_equals( (CPLSM(INT_MIN+1) - CPLSM(1)).v(), INT_MIN );
        ensure_equals( (CPLSM(0) - CPLSM(INT_MIN+1)).v(), INT_MAX );
        ensure_equals( (CPLSM(0) - CPLSM(INT_MAX)).v(), -INT_MAX );
        try { (CPLSM(INT_MIN) - CPLSM(1)).v(); ensure(false); } catch (...) {}
        try { (CPLSM(0) - CPLSM(INT_MIN)).v(); ensure(false); } catch (...) {}
        try { (CPLSM(INT_MIN) - CPLSM(1)).v(); ensure(false); } catch (...) {}

        ensure_equals( (CPLSM(INT_MIN+1) * CPLSM(-1)).v(), INT_MAX );
        ensure_equals( (CPLSM(-1) * CPLSM(INT_MIN+1)).v(), INT_MAX );
        ensure_equals( (CPLSM(INT_MIN) * CPLSM(1)).v(), INT_MIN );
        ensure_equals( (CPLSM(1) * CPLSM(INT_MIN)).v(), INT_MIN );
        ensure_equals( (CPLSM(1) * CPLSM(INT_MAX)).v(), INT_MAX );
        ensure_equals( (CPLSM(INT_MIN/2) * CPLSM(2)).v(), INT_MIN );
        ensure_equals( (CPLSM(INT_MAX/2) * CPLSM(2)).v(), INT_MAX-1 );
        ensure_equals( (CPLSM(INT_MAX/2+1) * CPLSM(-2)).v(), INT_MIN );
        ensure_equals( (CPLSM(0) * CPLSM(INT_MIN)).v(), 0 );
        ensure_equals( (CPLSM(INT_MIN) * CPLSM(0)).v(), 0 );
        ensure_equals( (CPLSM(0) * CPLSM(INT_MAX)).v(), 0 );
        ensure_equals( (CPLSM(INT_MAX) * CPLSM(0)).v(), 0 );
        try { (CPLSM(INT_MAX/2+1) * CPLSM(2)).v(); ensure(false); } catch (...) {}
        try { (CPLSM(2) * CPLSM(INT_MAX/2+1)).v(); ensure(false); } catch (...) {}
        try { (CPLSM(INT_MIN) * CPLSM(-1)).v(); ensure(false); } catch (...) {}
        try { (CPLSM(INT_MIN) * CPLSM(2)).v(); ensure(false); } catch (...) {}
        try { (CPLSM(2) * CPLSM(INT_MIN)).v(); ensure(false); } catch (...) {}

        ensure_equals( (CPLSM(4) / CPLSM(2)).v(), 2 );
        ensure_equals( (CPLSM(4) / CPLSM(-2)).v(), -2 );
        ensure_equals( (CPLSM(-4) / CPLSM(2)).v(), -2 );
        ensure_equals( (CPLSM(-4) / CPLSM(-2)).v(), 2 );
        ensure_equals( (CPLSM(0) / CPLSM(2)).v(), 0 );
        ensure_equals( (CPLSM(0) / CPLSM(-2)).v(), 0 );
        ensure_equals( (CPLSM(INT_MAX) / CPLSM(1)).v(), INT_MAX );
        ensure_equals( (CPLSM(INT_MAX) / CPLSM(-1)).v(), -INT_MAX );
        ensure_equals( (CPLSM(INT_MIN) / CPLSM(1)).v(), INT_MIN );
        try { (CPLSM(-1) * CPLSM(INT_MIN)).v(); ensure(false); } catch (...) {}
        try { (CPLSM(INT_MIN) / CPLSM(-1)).v(); ensure(false); } catch (...) {}
        try { (CPLSM(1) / CPLSM(0)).v(); ensure(false); } catch (...) {}

        ensure_equals( CPLSM_TO_UNSIGNED(1).v(), 1U );
        try { CPLSM_TO_UNSIGNED(-1); ensure(false); } catch (...) {}
    }


    // Test unsigned int safe maths
    template<>
    template<>
    void object::test<26>()
    {
        ensure_equals( (CPLSM(2U) + CPLSM(3U)).v(), 5U );
        ensure_equals( (CPLSM(UINT_MAX-1) + CPLSM(1U)).v(), UINT_MAX );
        try { (CPLSM(UINT_MAX) + CPLSM(1U)).v(); ensure(false); } catch (...) {}

        ensure_equals( (CPLSM(4U) - CPLSM(3U)).v(), 1U );
        ensure_equals( (CPLSM(4U) - CPLSM(4U)).v(), 0U );
        ensure_equals( (CPLSM(UINT_MAX) - CPLSM(1U)).v(), UINT_MAX-1 );
        try { (CPLSM(4U) - CPLSM(5U)).v(); ensure(false); } catch (...) {}

        ensure_equals( (CPLSM(0U) * CPLSM(UINT_MAX)).v(), 0U );
        ensure_equals( (CPLSM(UINT_MAX) * CPLSM(0U)).v(), 0U );
        ensure_equals( (CPLSM(UINT_MAX) * CPLSM(1U)).v(), UINT_MAX );
        ensure_equals( (CPLSM(1U) * CPLSM(UINT_MAX)).v(), UINT_MAX );
        try { (CPLSM(UINT_MAX) * CPLSM(2U)).v(); ensure(false); } catch (...) {}
        try { (CPLSM(2U) * CPLSM(UINT_MAX)).v(); ensure(false); } catch (...) {}

        ensure_equals( (CPLSM(4U) / CPLSM(2U)).v(), 2U );
        ensure_equals( (CPLSM(UINT_MAX) / CPLSM(1U)).v(), UINT_MAX );
        try { (CPLSM(1U) / CPLSM(0U)).v(); ensure(false); } catch (...) {}

        ensure_equals( (CPLSM(static_cast<GUInt64>(2)*1000*1000*1000) +
                        CPLSM(static_cast<GUInt64>(3)*1000*1000*1000)).v(),
                       static_cast<GUInt64>(5)*1000*1000*1000 );
        ensure_equals( (CPLSM(std::numeric_limits<GUInt64>::max() - 1) +
                        CPLSM(static_cast<GUInt64>(1))).v(),
                       std::numeric_limits<GUInt64>::max() );
        try { (CPLSM(std::numeric_limits<GUInt64>::max()) +
                        CPLSM(static_cast<GUInt64>(1))); } catch (...) {}

        ensure_equals( (CPLSM(static_cast<GUInt64>(2)*1000*1000*1000) *
                        CPLSM(static_cast<GUInt64>(3)*1000*1000*1000)).v(),
                       static_cast<GUInt64>(6)*1000*1000*1000*1000*1000*1000 );
        ensure_equals( (CPLSM(std::numeric_limits<GUInt64>::max()) *
                        CPLSM(static_cast<GUInt64>(1))).v(),
                       std::numeric_limits<GUInt64>::max() );
        try { (CPLSM(std::numeric_limits<GUInt64>::max()) *
                        CPLSM(static_cast<GUInt64>(2))); } catch (...) {}
    }

    // Test CPLParseRFC822DateTime()
    template<>
    template<>
    void object::test<27>()
    {
        int year, month, day, hour, min, sec, tz, weekday;
        ensure( !CPLParseRFC822DateTime("", &year, &month, &day, &hour, &min, &sec, &tz, &weekday) );

        ensure_equals( CPLParseRFC822DateTime("Thu, 15 Jan 2017 12:34:56 +0015", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr), TRUE );

        ensure_equals( CPLParseRFC822DateTime("Thu, 15 Jan 2017 12:34:56 +0015", &year, &month, &day, &hour, &min, &sec, &tz, &weekday), TRUE );
        ensure_equals( year, 2017 );
        ensure_equals( month, 1 );
        ensure_equals( day, 15 );
        ensure_equals( hour, 12 );
        ensure_equals( min, 34 );
        ensure_equals( sec, 56 );
        ensure_equals( tz, 101 );
        ensure_equals( weekday, 4 );

        ensure_equals( CPLParseRFC822DateTime("Thu, 15 Jan 2017 12:34:56 GMT", &year, &month, &day, &hour, &min, &sec, &tz, &weekday), TRUE );
        ensure_equals( year, 2017 );
        ensure_equals( month, 1 );
        ensure_equals( day, 15 );
        ensure_equals( hour, 12 );
        ensure_equals( min, 34 );
        ensure_equals( sec, 56 );
        ensure_equals( tz, 100 );
        ensure_equals( weekday, 4 );

        // Without day of week, second and timezone
        ensure_equals( CPLParseRFC822DateTime("15 Jan 2017 12:34", &year, &month, &day, &hour, &min, &sec, &tz, &weekday), TRUE );
        ensure_equals( year, 2017 );
        ensure_equals( month, 1 );
        ensure_equals( day, 15 );
        ensure_equals( hour, 12 );
        ensure_equals( min, 34 );
        ensure_equals( sec, -1 );
        ensure_equals( tz, 0 );
        ensure_equals( weekday, 0 );

        ensure_equals( CPLParseRFC822DateTime("XXX, 15 Jan 2017 12:34:56 GMT", &year, &month, &day, &hour, &min, &sec, &tz, &weekday), TRUE );
        ensure_equals( weekday, 0 );

        ensure( !CPLParseRFC822DateTime("Sun, 01 Jan 2017 12", &year, &month, &day, &hour, &min, &sec, &tz, &weekday) );

        ensure( !CPLParseRFC822DateTime("00 Jan 2017 12:34:56 GMT", &year, &month, &day, &hour, &min, &sec, &tz, &weekday) );

        ensure( !CPLParseRFC822DateTime("32 Jan 2017 12:34:56 GMT", &year, &month, &day, &hour, &min, &sec, &tz, &weekday) );

        ensure( !CPLParseRFC822DateTime("01 XXX 2017 12:34:56 GMT", &year, &month, &day, &hour, &min, &sec, &tz, &weekday) );

        ensure( !CPLParseRFC822DateTime("01 Jan 2017 -1:34:56 GMT", &year, &month, &day, &hour, &min, &sec, &tz, &weekday) );

        ensure( !CPLParseRFC822DateTime("01 Jan 2017 24:34:56 GMT", &year, &month, &day, &hour, &min, &sec, &tz, &weekday) );

        ensure( !CPLParseRFC822DateTime("01 Jan 2017 12:-1:56 GMT", &year, &month, &day, &hour, &min, &sec, &tz, &weekday) );

        ensure( !CPLParseRFC822DateTime("01 Jan 2017 12:60:56 GMT", &year, &month, &day, &hour, &min, &sec, &tz, &weekday) );

        ensure( !CPLParseRFC822DateTime("01 Jan 2017 12:34:-1 GMT", &year, &month, &day, &hour, &min, &sec, &tz, &weekday) );

        ensure( !CPLParseRFC822DateTime("01 Jan 2017 12:34:61 GMT", &year, &month, &day, &hour, &min, &sec, &tz, &weekday) );

        ensure( !CPLParseRFC822DateTime("15 Jan 2017 12:34:56 XXX", &year, &month, &day, &hour, &min, &sec, &tz, &weekday) );

        ensure( !CPLParseRFC822DateTime("15 Jan 2017 12:34:56 +-100", &year, &month, &day, &hour, &min, &sec, &tz, &weekday) );

        ensure( !CPLParseRFC822DateTime("15 Jan 2017 12:34:56 +9900", &year, &month, &day, &hour, &min, &sec, &tz, &weekday) );

    }

    // Test CPLCopyTree()
    template<>
    template<>
    void object::test<28>()
    {
        CPLString osTmpPath(CPLGetDirname(CPLGenerateTempFilename(nullptr)));
        CPLString osSrcDir(CPLFormFilename(osTmpPath, "src_dir", nullptr));
        CPLString osNewDir(CPLFormFilename(osTmpPath, "new_dir", nullptr));
        ensure( VSIMkdir(osSrcDir, 0755) == 0 );
        CPLString osSrcFile(CPLFormFilename(osSrcDir, "my.bin", nullptr));
        VSILFILE* fp = VSIFOpenL(osSrcFile, "wb");
        ensure( fp != nullptr );
        VSIFCloseL(fp);

        CPLPushErrorHandler(CPLQuietErrorHandler);
        ensure( CPLCopyTree(osNewDir, "/i/do_not/exist") < 0 );
        CPLPopErrorHandler();

        ensure( CPLCopyTree(osNewDir, osSrcDir) == 0 );
        VSIStatBufL sStat;
        CPLString osNewFile(CPLFormFilename(osNewDir, "my.bin", nullptr));
        ensure( VSIStatL(osNewFile, &sStat) == 0 );

        CPLPushErrorHandler(CPLQuietErrorHandler);
        ensure( CPLCopyTree(osNewDir, osSrcDir) < 0 );
        CPLPopErrorHandler();

        VSIUnlink( osNewFile );
        VSIRmdir( osNewDir );
        VSIUnlink( osSrcFile );
        VSIRmdir( osSrcDir );
    }

    class CPLJSonStreamingParserDump: public CPLJSonStreamingParser
    {
            std::vector<bool> m_abFirstMember;
            CPLString m_osSerialized;
            CPLString m_osException;

        public:
            CPLJSonStreamingParserDump() {}

            virtual void Reset() CPL_OVERRIDE
            {
                m_osSerialized.clear();
                m_osException.clear();
                CPLJSonStreamingParser::Reset();
            }

            virtual void String(const char* pszValue, size_t) CPL_OVERRIDE;
            virtual void Number(const char* pszValue, size_t) CPL_OVERRIDE;
            virtual void Boolean(bool bVal) CPL_OVERRIDE;
            virtual void Null() CPL_OVERRIDE;

            virtual void StartObject() CPL_OVERRIDE;
            virtual void EndObject() CPL_OVERRIDE;
            virtual void StartObjectMember(const char* pszKey, size_t) CPL_OVERRIDE;

            virtual void StartArray() CPL_OVERRIDE;
            virtual void EndArray() CPL_OVERRIDE;
            virtual void StartArrayMember() CPL_OVERRIDE;

            virtual void Exception(const char* pszMessage) CPL_OVERRIDE;

            const CPLString& GetSerialized() const { return m_osSerialized; }
            const CPLString& GetException() const { return m_osException; }
    };

    void CPLJSonStreamingParserDump::StartObject()
    {
        m_osSerialized += "{";
        m_abFirstMember.push_back(true);
    }
    void CPLJSonStreamingParserDump::EndObject()
    {
        m_osSerialized += "}";
        m_abFirstMember.pop_back();
    }

    void CPLJSonStreamingParserDump::StartObjectMember(const char* pszKey,
                                                       size_t)
    {
        if( !m_abFirstMember.back() )
            m_osSerialized += ", ";
        m_osSerialized += CPLSPrintf("\"%s\": ", pszKey);
        m_abFirstMember.back() = false;
    }

    void CPLJSonStreamingParserDump::String(const char* pszValue, size_t)
    {
        m_osSerialized += GetSerializedString(pszValue);
    }

    void CPLJSonStreamingParserDump::Number(const char* pszValue, size_t)
    {
        m_osSerialized += pszValue;
    }

    void CPLJSonStreamingParserDump::Boolean(bool bVal)
    {
        m_osSerialized += bVal ? "true" : "false";
    }

    void CPLJSonStreamingParserDump::Null()
    {
        m_osSerialized += "null";
    }

    void CPLJSonStreamingParserDump::StartArray()
    {
        m_osSerialized += "[";
        m_abFirstMember.push_back(true);
    }

    void CPLJSonStreamingParserDump::EndArray()
    {
        m_osSerialized += "]";
        m_abFirstMember.pop_back();
    }

    void CPLJSonStreamingParserDump::StartArrayMember()
    {
        if( !m_abFirstMember.back() )
            m_osSerialized += ", ";
        m_abFirstMember.back() = false;
    }

    void CPLJSonStreamingParserDump::Exception(const char* pszMessage)
    {
        m_osException = pszMessage;
    }

    // Test CPLJSonStreamingParser()
    template<>
    template<>
    void object::test<29>()
    {
        // nominal cases
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "true";
            ensure( oParser.Parse( sText, strlen(sText), true ) );
            ensure_equals( oParser.GetSerialized(), sText );

            oParser.Reset();
            for( size_t i = 0; sText[i]; i++ )
                ensure( oParser.Parse( sText + i, 1, sText[i+1] == 0 ) );
            ensure_equals( oParser.GetSerialized(), sText );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "false";
            ensure( oParser.Parse( sText, strlen(sText), true ) );
            ensure_equals( oParser.GetSerialized(), sText );

            oParser.Reset();
            for( size_t i = 0; sText[i]; i++ )
                ensure( oParser.Parse( sText + i, 1, sText[i+1] == 0 ) );
            ensure_equals( oParser.GetSerialized(), sText );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "null";
            ensure( oParser.Parse( sText, strlen(sText), true ) );
            ensure_equals( oParser.GetSerialized(), sText );

            oParser.Reset();
            for( size_t i = 0; sText[i]; i++ )
                ensure( oParser.Parse( sText + i, 1, sText[i+1] == 0 ) );
            ensure_equals( oParser.GetSerialized(), sText );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "10";
            ensure( oParser.Parse( sText, strlen(sText), true ) );
            ensure_equals( oParser.GetSerialized(), sText );

            oParser.Reset();
            for( size_t i = 0; sText[i]; i++ )
                ensure( oParser.Parse( sText + i, 1, sText[i+1] == 0 ) );
            ensure_equals( oParser.GetSerialized(), sText );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "123eE-34";
            ensure( oParser.Parse( sText, strlen(sText), true ) );
            ensure_equals( oParser.GetSerialized(), sText );

            oParser.Reset();
            for( size_t i = 0; sText[i]; i++ )
                ensure( oParser.Parse( sText + i, 1, sText[i+1] == 0 ) );
            ensure_equals( oParser.GetSerialized(), sText );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "\"\"";
            ensure( oParser.Parse( sText, strlen(sText), true ) );
            ensure_equals( oParser.GetSerialized(), sText );

            oParser.Reset();
            for( size_t i = 0; sText[i]; i++ )
                ensure( oParser.Parse( sText + i, 1, sText[i+1] == 0 ) );
            ensure_equals( oParser.GetSerialized(), sText );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "\"\\\\a\\b\\f\\n\\r\\t\\u0020\\u0001\\\"\"";
            ensure( oParser.Parse( sText, strlen(sText), true ) );
            ensure_equals( oParser.GetSerialized(), "\"\\\\a\\b\\f\\n\\r\\t \\u0001\\\"\"" );

            oParser.Reset();
            for( size_t i = 0; sText[i]; i++ )
                ensure( oParser.Parse( sText + i, 1, sText[i+1] == 0 ) );
            ensure_equals( oParser.GetSerialized(), "\"\\\\a\\b\\f\\n\\r\\t \\u0001\\\"\"" );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "\"\\u0001\\u0020\\ud834\\uDD1E\\uDD1E\\uD834\\uD834\\uD834\"";
            ensure( oParser.Parse( sText, strlen(sText), true ) );
            ensure_equals( oParser.GetSerialized(), "\"\\u0001 \xf0\x9d\x84\x9e\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd\"" );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "\"\\ud834\"";
            ensure( oParser.Parse( sText, strlen(sText), true ) );
            ensure_equals( oParser.GetSerialized(), "\"\xef\xbf\xbd\"" );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "\"\\ud834\\t\"";
            ensure( oParser.Parse( sText, strlen(sText), true ) );
            ensure_equals( oParser.GetSerialized(), "\"\xef\xbf\xbd\\t\"" );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "\"\\u00e9\"";
            ensure( oParser.Parse( sText, strlen(sText), true ) );
            ensure_equals( oParser.GetSerialized(), "\"\xc3\xa9\"" );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "{}";
            ensure( oParser.Parse( sText, strlen(sText), true ) );
            ensure_equals( oParser.GetSerialized(), sText );

            oParser.Reset();
            for( size_t i = 0; sText[i]; i++ )
                ensure( oParser.Parse( sText + i, 1, sText[i+1] == 0 ) );
            ensure_equals( oParser.GetSerialized(), sText );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "[]";
            ensure( oParser.Parse( sText, strlen(sText), true ) );
            ensure_equals( oParser.GetSerialized(), sText );

            oParser.Reset();
            for( size_t i = 0; sText[i]; i++ )
                ensure( oParser.Parse( sText + i, 1, sText[i+1] == 0 ) );
            ensure_equals( oParser.GetSerialized(), sText );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "[[]]";
            ensure( oParser.Parse( sText, strlen(sText), true ) );
            ensure_equals( oParser.GetSerialized(), sText );

            oParser.Reset();
            for( size_t i = 0; sText[i]; i++ )
                ensure( oParser.Parse( sText + i, 1, sText[i+1] == 0 ) );
            ensure_equals( oParser.GetSerialized(), sText );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "[1]";
            ensure( oParser.Parse( sText, strlen(sText), true ) );
            ensure_equals( oParser.GetSerialized(), sText );

            oParser.Reset();
            for( size_t i = 0; sText[i]; i++ )
                ensure( oParser.Parse( sText + i, 1, sText[i+1] == 0 ) );
            ensure_equals( oParser.GetSerialized(), sText );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "[1,2]";
            ensure( oParser.Parse( sText, strlen(sText), true ) );
            ensure_equals( oParser.GetSerialized(), "[1, 2]" );

            oParser.Reset();
            for( size_t i = 0; sText[i]; i++ )
                ensure( oParser.Parse( sText + i, 1, sText[i+1] == 0 ) );
            ensure_equals( oParser.GetSerialized(), "[1, 2]" );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "{\"a\":null}";
            ensure( oParser.Parse( sText, strlen(sText), true ) );
            ensure_equals( oParser.GetSerialized(), "{\"a\": null}" );

            oParser.Reset();
            for( size_t i = 0; sText[i]; i++ )
                ensure( oParser.Parse( sText + i, 1, sText[i+1] == 0 ) );
            ensure_equals( oParser.GetSerialized(), "{\"a\": null}" );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = " { \"a\" : null ,\r\n\t\"b\": {\"c\": 1}, \"d\": [1] }";
            ensure( oParser.Parse( sText, strlen(sText), true ) );
            const char sExpected[] = "{\"a\": null, \"b\": {\"c\": 1}, \"d\": [1]}";
            ensure_equals( oParser.GetSerialized(), sExpected );

            oParser.Reset();
            ensure( oParser.Parse( sText, strlen(sText), true ) );
            ensure_equals( oParser.GetSerialized(), sExpected );

            oParser.Reset();
            for( size_t i = 0; sText[i]; i++ )
                ensure( oParser.Parse( sText + i, 1, sText[i+1] == 0 ) );
            ensure_equals( oParser.GetSerialized(), sExpected );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "infinity";
            ensure( oParser.Parse( sText, strlen(sText), true ) );
            ensure_equals( oParser.GetSerialized(), sText );

            oParser.Reset();
            for( size_t i = 0; sText[i]; i++ )
                ensure( oParser.Parse( sText + i, 1, sText[i+1] == 0 ) );
            ensure_equals( oParser.GetSerialized(), sText );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "-infinity";
            ensure( oParser.Parse( sText, strlen(sText), true ) );
            ensure_equals( oParser.GetSerialized(), sText );

            oParser.Reset();
            for( size_t i = 0; sText[i]; i++ )
                ensure( oParser.Parse( sText + i, 1, sText[i+1] == 0 ) );
            ensure_equals( oParser.GetSerialized(), sText );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "nan";
            ensure( oParser.Parse( sText, strlen(sText), true ) );
            ensure_equals( oParser.GetSerialized(), sText );

            oParser.Reset();
            for( size_t i = 0; sText[i]; i++ )
                ensure( oParser.Parse( sText + i, 1, sText[i+1] == 0 ) );
            ensure_equals( oParser.GetSerialized(), sText );
        }

        // errors
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "tru";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "tru1";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "truxe";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "truex";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "fals";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "falsxe";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "falsex";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "nul";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "nulxl";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "nullx";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "na";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "nanx";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "infinit";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "infinityx";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "-infinit";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "-infinityx";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "true false";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "x";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "{";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "}";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "[";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "[1";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "[,";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "[|";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "]";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "{ :";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "{ ,";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "{ |";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "{ 1";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "{ \"x\"";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "{ \"x\": ";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "{ \"x\": 1 2";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "{ \"x\", ";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "{ \"x\" }";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "{\"a\" x}";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "1x";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "\"";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "\"\\";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "\"\\x\"";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "\"\\u";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "\"\\ux";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "\"\\u000";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "\"\\uD834\\ux\"";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "\"\\\"";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "\"too long\"";
            oParser.SetMaxStringSize(2);
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "[[]]";
            oParser.SetMaxDepth(1);
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "{ \"x\": {} }";
            oParser.SetMaxDepth(1);
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "[,]";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "[true,]";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "[true,,true]";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
        {
            CPLJSonStreamingParserDump oParser;
            const char sText[] = "[true true]";
            ensure( !oParser.Parse( sText, strlen(sText), true ) );
            ensure( !oParser.GetException().empty() );
        }
    }

    // Test cpl_mem_cache
    template<>
    template<>
    void object::test<30>()
    {
        lru11::Cache<int,int> cache(2,1);
        ensure_equals( cache.size(), 0U );
        ensure( cache.empty() );
        cache.clear();
        int val;
        ensure( !cache.tryGet(0, val) );
        try
        {
            cache.get(0);
            ensure( false );
        }
        catch( const lru11::KeyNotFound& )
        {
            ensure( true );
        }
        ensure( !cache.remove(0) );
        ensure( !cache.contains(0) );
        ensure_equals( cache.getMaxSize(), 2U );
        ensure_equals( cache.getElasticity(), 1U );
        ensure_equals( cache.getMaxAllowedSize(), 3U );

        cache.insert(0, 1);
        val = 0;
        ensure( cache.tryGet(0, val) );
        ensure_equals( val, 1 );
        ensure_equals( cache.get(0), 1 );
        ensure_equals( cache.getCopy(0), 1);
        ensure_equals( cache.size(), 1U );
        ensure( !cache.empty() );
        ensure( cache.contains(0) );
        bool visited = false;
        auto lambda = [&visited] (const lru11::KeyValuePair<int, int>& kv)
        {
            if(kv.key == 0 && kv.value == 1)
                visited = true;
        };
        cache.cwalk( lambda );
        ensure( visited) ;
        cache.insert(0, 2);
        ensure_equals( cache.get(0), 2 );
        ensure_equals( cache.size(), 1U );
        cache.insert(1, 3);
        cache.insert(2, 4);
        ensure_equals( cache.size(), 3U );
        cache.insert(3, 5);
        ensure_equals( cache.size(), 2U );
        ensure( cache.contains(2) );
        ensure( cache.contains(3) );
        ensure( !cache.contains(0) );
        ensure( !cache.contains(1) );
        ensure( cache.remove(2) );
        ensure( !cache.contains(2) );
        ensure_equals( cache.size(), 1U );
    }

    // Test CPLJSONDocument
    template<>
    template<>
    void object::test<31>()
    {
        {
            // Test Json document LoadUrl
            CPLJSONDocument oDocument;
            const char *options[5] = {
              "CONNECTTIMEOUT=15",
              "TIMEOUT=20",
              "MAX_RETRY=5",
              "RETRY_DELAY=1",
              nullptr
            };

            oDocument.GetRoot().Add("foo", "bar");

            if( CPLHTTPEnabled() )
            {
                CPLSetConfigOption("CPL_CURL_ENABLE_VSIMEM", "YES");
                VSILFILE* fpTmp = VSIFOpenL("/vsimem/test.json", "wb");
                const char* pszContent = "{ \"foo\": \"bar\" }";
                VSIFWriteL(pszContent, 1, strlen(pszContent), fpTmp);
                VSIFCloseL(fpTmp);
                ensure( oDocument.LoadUrl(
                    "/vsimem/test.json",
                    const_cast<char**>(options) ) );
                CPLSetConfigOption("CPL_CURL_ENABLE_VSIMEM", nullptr);
                VSIUnlink("/vsimem/test.json");

                CPLJSONObject oJsonRoot = oDocument.GetRoot();
                ensure( oJsonRoot.IsValid() );

                CPLString value = oJsonRoot.GetString("foo", "");
                ensure_not( EQUAL(value, "bar") );
            }
        }
        {
            // Test Json document LoadChunks
            CPLJSONDocument oDocument;

            CPLPushErrorHandler(CPLQuietErrorHandler);
            ensure( !oDocument.LoadChunks("/i_do/not/exist", 512) );
            CPLPopErrorHandler();

            CPLPushErrorHandler(CPLQuietErrorHandler);
            ensure( !oDocument.LoadChunks("test_cpl.cpp", 512) );
            CPLPopErrorHandler();

            oDocument.GetRoot().Add("foo", "bar");

            ensure( oDocument.LoadChunks((data_ + SEP + "test.json").c_str(), 512) );

            CPLJSONObject oJsonRoot = oDocument.GetRoot();
            ensure( oJsonRoot.IsValid() );
            ensure_equals( oJsonRoot.GetInteger("resource/id", 10), 0 );

            CPLJSONObject oJsonResource = oJsonRoot.GetObj("resource");
            ensure( oJsonResource.IsValid() );
            std::vector<CPLJSONObject> children = oJsonResource.GetChildren();
            ensure(children.size() == 11);

            CPLJSONArray oaScopes = oJsonRoot.GetArray("resource/scopes");
            ensure( oaScopes.IsValid() );
            ensure_equals( oaScopes.Size(), 2);

            CPLJSONObject oHasChildren = oJsonRoot.GetObj("resource/children");
            ensure( oHasChildren.IsValid() );
            ensure_equals( oHasChildren.ToBool(), true );

            ensure_equals( oJsonResource.GetBool( "children", false ), true );

            CPLJSONObject oJsonId = oJsonRoot["resource/owner_user/id"];
            ensure( oJsonId.IsValid() );
        }
        {
            CPLJSONDocument oDocument;
            ensure( !oDocument.LoadMemory(nullptr, 0) );
            ensure( !oDocument.LoadMemory(CPLString()) );
            ensure( oDocument.LoadMemory(std::string("true")) );
            ensure( oDocument.GetRoot().GetType() == CPLJSONObject::Type::Boolean );
            ensure( oDocument.GetRoot().ToBool() );
            ensure( oDocument.LoadMemory(std::string("false")) );
            ensure( oDocument.GetRoot().GetType() == CPLJSONObject::Type::Boolean );
            ensure( !oDocument.GetRoot().ToBool() );
        }
        {
            // Copy constructor
            CPLJSONDocument oDocument;
            oDocument.GetRoot();
            CPLJSONDocument oDocument2(oDocument);
            CPLJSONObject oObj;
            CPLJSONObject oObj2(oObj);
            // Assignment operator
            oDocument2 = oDocument;
            auto& oDocument2Ref(oDocument2);
            oDocument2 = oDocument2Ref;
            oObj2 = oObj;
            auto& oObj2Ref(oObj2);
            oObj2 = oObj2Ref;
        }
        {
            // Move constructor
            CPLJSONDocument oDocument;
            oDocument.GetRoot();
            CPLJSONDocument oDocument2(std::move(oDocument));
        }
        {
            // Move assignment
            CPLJSONDocument oDocument;
            oDocument.GetRoot();
            CPLJSONDocument oDocument2;
            oDocument2 = std::move(oDocument);
        }
        {
            // Save
            CPLJSONDocument oDocument;
            CPLPushErrorHandler(CPLQuietErrorHandler);
            ensure( !oDocument.Save("/i_do/not/exist") );
            CPLPopErrorHandler();
        }
        {
            CPLJSONObject oObj;
            oObj.Add("string", std::string("my_string"));
            ensure_equals( oObj.GetString("string"), std::string("my_string"));
            ensure_equals( oObj.GetString("inexisting_string", "default"),
                           std::string("default"));
            oObj.Add("const_char_star", nullptr);
            oObj.Add("const_char_star", "my_const_char_star");
            ensure( oObj.GetObj("const_char_star").GetType() == CPLJSONObject::Type::String );
            oObj.Add("int", 1);
            ensure_equals( oObj.GetInteger("int"), 1 );
            ensure_equals( oObj.GetInteger("inexisting_int", -987), -987 );
            ensure( oObj.GetObj("int").GetType() == CPLJSONObject::Type::Integer );
            oObj.Add("int64", GINT64_MAX);
            ensure_equals( oObj.GetLong("int64"), GINT64_MAX );
            ensure_equals( oObj.GetLong("inexisting_int64", GINT64_MIN), GINT64_MIN );
            ensure( oObj.GetObj("int64").GetType() == CPLJSONObject::Type::Long );
            oObj.Add("double", 1.25);
            ensure_equals( oObj.GetDouble("double"), 1.25 );
            ensure_equals( oObj.GetDouble("inexisting_double", -987.0), -987.0 );
            ensure( oObj.GetObj("double").GetType() == CPLJSONObject::Type::Double );
            oObj.Add("array", CPLJSONArray());
            ensure( oObj.GetObj("array").GetType() == CPLJSONObject::Type::Array );
            oObj.Add("obj", CPLJSONObject());
            ensure( oObj.GetObj("obj").GetType() == CPLJSONObject::Type::Object );
            oObj.Add("bool", true);
            ensure_equals( oObj.GetBool("bool"), true );
            ensure_equals( oObj.GetBool("inexisting_bool", false), false );
            ensure( oObj.GetObj("bool").GetType() == CPLJSONObject::Type::Boolean );
            oObj.AddNull("null_field");
            ensure( oObj.GetObj("null_field").GetType() == CPLJSONObject::Type::Null );
            ensure( oObj.GetObj("inexisting").GetType() == CPLJSONObject::Type::Unknown );
            oObj.Set("string", std::string("my_string"));
            oObj.Set("const_char_star", nullptr);
            oObj.Set("const_char_star", "my_const_char_star");
            oObj.Set("int", 1);
            oObj.Set("int64", GINT64_MAX);
            oObj.Set("double", 1.25);
            //oObj.Set("array", CPLJSONArray());
            //oObj.Set("obj", CPLJSONObject());
            oObj.Set("bool", true);
            oObj.SetNull("null_field");
            ensure( CPLJSONArray().GetChildren().empty() );
            oObj.ToArray();
            ensure_equals( CPLJSONObject().Format(CPLJSONObject::PrettyFormat::Spaced), std::string("{ }") );
            ensure_equals( CPLJSONObject().Format(CPLJSONObject::PrettyFormat::Pretty), std::string("{\n}") );
            ensure_equals( CPLJSONObject().Format(CPLJSONObject::PrettyFormat::Plain), std::string("{}") );
        }
        {
            CPLJSONArray oArrayConstructorString(std::string("foo"));
            CPLJSONArray oArray;
            oArray.Add(CPLJSONObject());
            oArray.Add(std::string("str"));
            oArray.Add("const_char_star");
            oArray.Add(1.25);
            oArray.Add(1);
            oArray.Add(GINT64_MAX);
            oArray.Add(true);
            ensure_equals(oArray.Size(), 7);

            int nCount = 0;
            for(const auto& obj: oArray)
            {
                ensure_equals(obj.GetInternalHandle(), oArray[nCount].GetInternalHandle());
                nCount++;
            }
            ensure_equals(nCount, 7);
        }
        {
            CPLJSONDocument oDocument;
            ensure( oDocument.LoadMemory(CPLString("{ \"/foo\" : \"bar\" }")) );
            ensure_equals( oDocument.GetRoot().GetString("/foo"), std::string("bar") );
        }
    }

    // Test CPLRecodeIconv() with re-allocation
    template<>
    template<>
    void object::test<32>()
    {
#ifdef CPL_RECODE_ICONV
        int N = 32800;
        char* pszIn = static_cast<char*>(CPLMalloc(N + 1));
        for(int i=0;i<N;i++)
            pszIn[i] = '\xE9';
        pszIn[N] = 0;
        char* pszExpected = static_cast<char*>(CPLMalloc(N * 2 + 1));
        for(int i=0;i<N;i++)
        {
            pszExpected[2*i] = '\xC3';
            pszExpected[2*i+1] = '\xA9';
        }
        pszExpected[N * 2] = 0;
        char* pszRet = CPLRecode(pszIn, "ISO-8859-2", CPL_ENC_UTF8);
        ensure_equals( memcmp(pszExpected, pszRet, N * 2 + 1), 0 );
        CPLFree(pszIn);
        CPLFree(pszRet);
        CPLFree(pszExpected);
#endif
    }

    // Test CPLHTTPParseMultipartMime()
    template<>
    template<>
    void object::test<33>()
    {
        CPLHTTPResult* psResult;

        psResult =
            static_cast<CPLHTTPResult*>(CPLCalloc(1, sizeof(CPLHTTPResult)));
        CPLPushErrorHandler(CPLQuietErrorHandler);
        ensure( !CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)) );
        CPLPopErrorHandler();
        CPLHTTPDestroyResult(psResult);

        // Missing boundary value
        psResult =
            static_cast<CPLHTTPResult*>(CPLCalloc(1, sizeof(CPLHTTPResult)));
        psResult->pszContentType =
            CPLStrdup("multipart/form-data; boundary=");
        CPLPushErrorHandler(CPLQuietErrorHandler);
        ensure( !CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)) );
        CPLPopErrorHandler();
        CPLHTTPDestroyResult(psResult);

        // No content
        psResult =
            static_cast<CPLHTTPResult*>(CPLCalloc(1, sizeof(CPLHTTPResult)));
        psResult->pszContentType =
            CPLStrdup("multipart/form-data; boundary=myboundary");
        CPLPushErrorHandler(CPLQuietErrorHandler);
        ensure( !CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)) );
        CPLPopErrorHandler();
        CPLHTTPDestroyResult(psResult);

        // No part
        psResult =
            static_cast<CPLHTTPResult*>(CPLCalloc(1, sizeof(CPLHTTPResult)));
        psResult->pszContentType =
            CPLStrdup("multipart/form-data; boundary=myboundary");
        {
            const char* pszText = "--myboundary  some junk\r\n";
            psResult->pabyData = reinterpret_cast<GByte*>(CPLStrdup(pszText));
            psResult->nDataLen = static_cast<int>(strlen(pszText));
        }
        CPLPushErrorHandler(CPLQuietErrorHandler);
        ensure( !CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)) );
        CPLPopErrorHandler();
        CPLHTTPDestroyResult(psResult);

        // Missing end boundary
        psResult =
            static_cast<CPLHTTPResult*>(CPLCalloc(1, sizeof(CPLHTTPResult)));
        psResult->pszContentType =
            CPLStrdup("multipart/form-data; boundary=myboundary");
        {
            const char* pszText = "--myboundary  some junk\r\n"
                "\r\n"
                "Bla";
            psResult->pabyData = reinterpret_cast<GByte*>(CPLStrdup(pszText));
            psResult->nDataLen = static_cast<int>(strlen(pszText));
        }
        CPLPushErrorHandler(CPLQuietErrorHandler);
        ensure( !CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)) );
        CPLPopErrorHandler();
        CPLHTTPDestroyResult(psResult);

        // Truncated header
        psResult =
            static_cast<CPLHTTPResult*>(CPLCalloc(1, sizeof(CPLHTTPResult)));
        psResult->pszContentType =
            CPLStrdup("multipart/form-data; boundary=myboundary");
        {
            const char* pszText = "--myboundary  some junk\r\n"
                "Content-Type: foo";
            psResult->pabyData = reinterpret_cast<GByte*>(CPLStrdup(pszText));
            psResult->nDataLen = static_cast<int>(strlen(pszText));
        }
        CPLPushErrorHandler(CPLQuietErrorHandler);
        ensure( !CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)) );
        CPLPopErrorHandler();
        CPLHTTPDestroyResult(psResult);

        // Invalid end boundary
        psResult =
            static_cast<CPLHTTPResult*>(CPLCalloc(1, sizeof(CPLHTTPResult)));
        psResult->pszContentType =
            CPLStrdup("multipart/form-data; boundary=myboundary");
        {
            const char* pszText = "--myboundary  some junk\r\n"
                "\r\n"
                "Bla"
                "\r\n"
                "--myboundary";
            psResult->pabyData = reinterpret_cast<GByte*>(CPLStrdup(pszText));
            psResult->nDataLen = static_cast<int>(strlen(pszText));
        }
        CPLPushErrorHandler(CPLQuietErrorHandler);
        ensure( !CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)) );
        CPLPopErrorHandler();
        CPLHTTPDestroyResult(psResult);

        // Invalid end boundary
        psResult =
            static_cast<CPLHTTPResult*>(CPLCalloc(1, sizeof(CPLHTTPResult)));
        psResult->pszContentType =
            CPLStrdup("multipart/form-data; boundary=myboundary");
        {
            const char* pszText = "--myboundary  some junk\r\n"
                "\r\n"
                "Bla"
                "\r\n"
                "--myboundary";
            psResult->pabyData = reinterpret_cast<GByte*>(CPLStrdup(pszText));
            psResult->nDataLen = static_cast<int>(strlen(pszText));
        }
        CPLPushErrorHandler(CPLQuietErrorHandler);
        ensure( !CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)) );
        CPLPopErrorHandler();
        CPLHTTPDestroyResult(psResult);

        // Valid single part, no header
        psResult =
            static_cast<CPLHTTPResult*>(CPLCalloc(1, sizeof(CPLHTTPResult)));
        psResult->pszContentType =
            CPLStrdup("multipart/form-data; boundary=myboundary");
        {
            const char* pszText =
                "--myboundary  some junk\r\n"
                "\r\n"
                "Bla"
                "\r\n"
                "--myboundary--\r\n";
            psResult->pabyData = reinterpret_cast<GByte*>(CPLStrdup(pszText));
            psResult->nDataLen = static_cast<int>(strlen(pszText));
        }
        ensure( CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)) );
        ensure_equals( psResult->nMimePartCount, 1 );
        ensure_equals( psResult->pasMimePart[0].papszHeaders,
                       static_cast<char**>(nullptr) );
        ensure_equals( psResult->pasMimePart[0].nDataLen, 3 );
        ensure( strncmp(reinterpret_cast<char*>(psResult->pasMimePart[0].pabyData),
                       "Bla", 3) == 0 );
        ensure( CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)) );
        CPLHTTPDestroyResult(psResult);

        // Valid single part, with header
        psResult =
            static_cast<CPLHTTPResult*>(CPLCalloc(1, sizeof(CPLHTTPResult)));
        psResult->pszContentType =
            CPLStrdup("multipart/form-data; boundary=myboundary");
        {
            const char* pszText =
                "--myboundary  some junk\r\n"
                "Content-Type: bla\r\n"
                "\r\n"
                "Bla"
                "\r\n"
                "--myboundary--\r\n";
            psResult->pabyData = reinterpret_cast<GByte*>(CPLStrdup(pszText));
            psResult->nDataLen = static_cast<int>(strlen(pszText));
        }
        ensure( CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)) );
        ensure_equals( psResult->nMimePartCount, 1 );
        ensure_equals( CSLCount(psResult->pasMimePart[0].papszHeaders), 1 );
        ensure_equals( CPLString(psResult->pasMimePart[0].papszHeaders[0]),
                       CPLString("Content-Type=bla") );
        ensure_equals( psResult->pasMimePart[0].nDataLen, 3 );
        ensure( strncmp(reinterpret_cast<char*>(psResult->pasMimePart[0].pabyData),
                       "Bla", 3) == 0 );
        ensure( CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)) );
        CPLHTTPDestroyResult(psResult);

        // Valid single part, 2 headers
        psResult =
            static_cast<CPLHTTPResult*>(CPLCalloc(1, sizeof(CPLHTTPResult)));
        psResult->pszContentType =
            CPLStrdup("multipart/form-data; boundary=myboundary");
        {
            const char* pszText =
                "--myboundary  some junk\r\n"
                "Content-Type: bla\r\n"
                "Content-Disposition: bar\r\n"
                "\r\n"
                "Bla"
                "\r\n"
                "--myboundary--\r\n";
            psResult->pabyData = reinterpret_cast<GByte*>(CPLStrdup(pszText));
            psResult->nDataLen = static_cast<int>(strlen(pszText));
        }
        ensure( CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)) );
        ensure_equals( psResult->nMimePartCount, 1 );
        ensure_equals( CSLCount(psResult->pasMimePart[0].papszHeaders), 2 );
        ensure_equals( CPLString(psResult->pasMimePart[0].papszHeaders[0]),
                       CPLString("Content-Type=bla") );
        ensure_equals( CPLString(psResult->pasMimePart[0].papszHeaders[1]),
                       CPLString("Content-Disposition=bar") );
        ensure_equals( psResult->pasMimePart[0].nDataLen, 3 );
        ensure( strncmp(reinterpret_cast<char*>(psResult->pasMimePart[0].pabyData),
                       "Bla", 3) == 0 );
        ensure( CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)) );
        CPLHTTPDestroyResult(psResult);

        // Single part, but with header without extra terminating \r\n
        // (invalid normally, but apparently necessary for some ArcGIS WCS implementations)
        psResult =
            static_cast<CPLHTTPResult*>(CPLCalloc(1, sizeof(CPLHTTPResult)));
        psResult->pszContentType =
            CPLStrdup("multipart/form-data; boundary=myboundary");
        {
            const char* pszText =
                "--myboundary  some junk\r\n"
                "Content-Type: bla\r\n"
                "Bla"
                "\r\n"
                "--myboundary--\r\n";
            psResult->pabyData = reinterpret_cast<GByte*>(CPLStrdup(pszText));
            psResult->nDataLen = static_cast<int>(strlen(pszText));
        }
        ensure( CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)) );
        ensure_equals( psResult->nMimePartCount, 1 );
        ensure_equals( CPLString(psResult->pasMimePart[0].papszHeaders[0]),
                       CPLString("Content-Type=bla") );
        ensure_equals( psResult->pasMimePart[0].nDataLen, 3 );
        ensure( strncmp(reinterpret_cast<char*>(psResult->pasMimePart[0].pabyData),
                       "Bla", 3) == 0 );
        ensure( CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)) );
        CPLHTTPDestroyResult(psResult);

        // Valid 2 parts, no header
        psResult =
            static_cast<CPLHTTPResult*>(CPLCalloc(1, sizeof(CPLHTTPResult)));
        psResult->pszContentType =
            CPLStrdup("multipart/form-data; boundary=myboundary");
        {
            const char* pszText =
                "--myboundary  some junk\r\n"
                "\r\n"
                "Bla"
                "\r\n"
                "--myboundary\r\n"
                "\r\n"
                "second part"
                "\r\n"
                "--myboundary--\r\n";
            psResult->pabyData = reinterpret_cast<GByte*>(CPLStrdup(pszText));
            psResult->nDataLen = static_cast<int>(strlen(pszText));
        }
        ensure( CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)) );
        ensure_equals( psResult->nMimePartCount, 2 );
        ensure_equals( psResult->pasMimePart[0].papszHeaders,
                       static_cast<char**>(nullptr) );
        ensure_equals( psResult->pasMimePart[0].nDataLen, 3 );
        ensure( strncmp(reinterpret_cast<char*>(psResult->pasMimePart[0].pabyData),
                       "Bla", 3) == 0 );
        ensure_equals( psResult->pasMimePart[1].nDataLen, 11 );
        ensure( strncmp(reinterpret_cast<char*>(psResult->pasMimePart[1].pabyData),
                       "second part", 11) == 0 );
        ensure( CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)) );
        CPLHTTPDestroyResult(psResult);

    }

    // Test cpl::down_cast
    template<>
    template<>
    void object::test<34>()
    {
        struct Base{
            virtual ~Base() {}
        };
        struct Derived: public Base {};
        Base b;
        Derived d;
        Base* p_b_d = &d;

#ifdef wont_compile
        struct OtherBase {};
        OtherBase ob;
        ensure_equals(cpl::down_cast<OtherBase*>(p_b_d), &ob);
#endif
#ifdef compile_with_warning
        ensure_equals(cpl::down_cast<Base*>(p_b_d), p_b_d);
#endif
        ensure_equals(cpl::down_cast<Derived*>(p_b_d), &d);
        ensure_equals(cpl::down_cast<Derived*>(static_cast<Base*>(nullptr)), static_cast<Derived*>(nullptr));
    }

    // Test CPLPrintTime() in particular case of RFC822 formatting in C locale
    template<>
    template<>
    void object::test<35>()
    {
        char szDate[64];
        struct tm tm;
        tm.tm_sec = 56;
        tm.tm_min = 34;
        tm.tm_hour = 12;
        tm.tm_mday = 20;
        tm.tm_mon = 6-1;
        tm.tm_year = 2018 - 1900;
        tm.tm_wday = 3; // Wednesday
        tm.tm_yday = 0; // unused
        tm.tm_isdst = 0; // unused
        int nRet = CPLPrintTime(szDate, sizeof(szDate)-1,
                     "%a, %d %b %Y %H:%M:%S GMT", &tm, "C");
        szDate[nRet] = 0;
        ensure_equals( std::string(szDate), std::string("Wed, 20 Jun 2018 12:34:56 GMT") );
    }

    // Test CPLAutoClose
    template<>
    template<>
    void object::test<36>()
    {
        static int counter = 0;
        class AutoCloseTest{
        public:
            AutoCloseTest() {
                counter += 222;
            }
            virtual ~AutoCloseTest() {
                counter -= 22;
            }
            static AutoCloseTest* Create() {
                return new AutoCloseTest;
            }
            static void Destroy(AutoCloseTest* p) {
                delete p;
            }
        };
        {
            AutoCloseTest* p1 = AutoCloseTest::Create();
            CPL_AUTO_CLOSE_WARP(p1,AutoCloseTest::Destroy);

            AutoCloseTest* p2 = AutoCloseTest::Create();
            CPL_AUTO_CLOSE_WARP(p2,AutoCloseTest::Destroy);

        }
        ensure_equals(counter,400);
    }

    // Test cpl_minixml
    template<>
    template<>
    void object::test<37>()
    {
        CPLXMLNode* psRoot = CPLCreateXMLNode(nullptr, CXT_Element, "Root");
        CPLXMLNode* psElt = CPLCreateXMLElementAndValue(psRoot, "Elt", "value");
        CPLAddXMLAttributeAndValue(psElt, "attr1", "val1");
        CPLAddXMLAttributeAndValue(psElt, "attr2", "val2");
        char* str = CPLSerializeXMLTree(psRoot);
        CPLDestroyXMLNode(psRoot);
        ensure_equals( std::string(str), std::string("<Root>\n  <Elt attr1=\"val1\" attr2=\"val2\">value</Elt>\n</Root>\n") );
        CPLFree(str);
    }

    // Test CPLCharUniquePtr
    template<>
    template<>
    void object::test<38>()
    {
        CPLCharUniquePtr x;
        ensure( x.get() == nullptr );
        x.reset(CPLStrdup("foo"));
        ensure_equals( std::string(x.get()), "foo");
    }

    // Test CPLJSonStreamingWriter
    template<>
    template<>
    void object::test<39>()
    {
        {
            CPLJSonStreamingWriter x(nullptr, nullptr);
            ensure_equals( x.GetString(), std::string() );
        }
        {
            CPLJSonStreamingWriter x(nullptr, nullptr);
            x.Add(true);
            ensure_equals( x.GetString(), std::string("true") );
        }
        {
            std::string res;
            struct MyCallback
            {
                static void f(const char* pszText, void* user_data)
                {
                    *static_cast<std::string*>(user_data) += pszText;
                }
            };
            CPLJSonStreamingWriter x(&MyCallback::f, &res);
            x.Add(true);
            ensure_equals( x.GetString(), std::string() );
            ensure_equals( res, std::string("true") );
        }
        {
            CPLJSonStreamingWriter x(nullptr, nullptr);
            x.Add(false);
            ensure_equals( x.GetString(), std::string("false") );
        }
        {
            CPLJSonStreamingWriter x(nullptr, nullptr);
            x.AddNull();
            ensure_equals( x.GetString(), std::string("null") );
        }
        {
            CPLJSonStreamingWriter x(nullptr, nullptr);
            x.Add(1);
            ensure_equals( x.GetString(), std::string("1") );
        }
        {
            CPLJSonStreamingWriter x(nullptr, nullptr);
            x.Add(4200000000U);
            ensure_equals( x.GetString(), std::string("4200000000") );
        }
        {
            CPLJSonStreamingWriter x(nullptr, nullptr);
            x.Add(static_cast<std::int64_t>(-10000) * 1000000);
            ensure_equals( x.GetString(), std::string("-10000000000") );
        }
        {
            CPLJSonStreamingWriter x(nullptr, nullptr);
            x.Add(static_cast<std::uint64_t>(10000) * 1000000);
            ensure_equals( x.GetString(), std::string("10000000000") );
        }
        {
            CPLJSonStreamingWriter x(nullptr, nullptr);
            x.Add(1.5f);
            ensure_equals( x.GetString(), std::string("1.5") );
        }
        {
            CPLJSonStreamingWriter x(nullptr, nullptr);
            x.Add(std::numeric_limits<float>::quiet_NaN());
            ensure_equals( x.GetString(), std::string("\"NaN\"") );
        }
        {
            CPLJSonStreamingWriter x(nullptr, nullptr);
            x.Add(std::numeric_limits<float>::infinity());
            ensure_equals( x.GetString(), std::string("\"Infinity\"") );
        }
        {
            CPLJSonStreamingWriter x(nullptr, nullptr);
            x.Add(-std::numeric_limits<float>::infinity());
            ensure_equals( x.GetString(), std::string("\"-Infinity\"") );
        }
        {
            CPLJSonStreamingWriter x(nullptr, nullptr);
            x.Add(1.25);
            ensure_equals( x.GetString(), std::string("1.25") );
        }
        {
            CPLJSonStreamingWriter x(nullptr, nullptr);
            x.Add(std::numeric_limits<double>::quiet_NaN());
            ensure_equals( x.GetString(), std::string("\"NaN\"") );
        }
        {
            CPLJSonStreamingWriter x(nullptr, nullptr);
            x.Add(std::numeric_limits<double>::infinity());
            ensure_equals( x.GetString(), std::string("\"Infinity\"") );
        }
        {
            CPLJSonStreamingWriter x(nullptr, nullptr);
            x.Add(-std::numeric_limits<double>::infinity());
            ensure_equals( x.GetString(), std::string("\"-Infinity\"") );
        }
        {
            CPLJSonStreamingWriter x(nullptr, nullptr);
            x.Add(std::string("foo\\bar\"baz\b\f\n\r\t" "\x01" "boo"));
            ensure_equals( x.GetString(), std::string("\"foo\\\\bar\\\"baz\\b\\f\\n\\r\\t\\u0001boo\"") );
        }
        {
            CPLJSonStreamingWriter x(nullptr, nullptr);
            x.Add("foo\\bar\"baz\b\f\n\r\t" "\x01" "boo");
            ensure_equals( x.GetString(), std::string("\"foo\\\\bar\\\"baz\\b\\f\\n\\r\\t\\u0001boo\"") );
        }
        {
            CPLJSonStreamingWriter x(nullptr, nullptr);
            x.SetPrettyFormatting(false);
            {
                auto ctxt(x.MakeObjectContext());
            }
            ensure_equals( x.GetString(), std::string("{}") );
        }
        {
            CPLJSonStreamingWriter x(nullptr, nullptr);
            {
                auto ctxt(x.MakeObjectContext());
            }
            ensure_equals( x.GetString(), std::string("{}") );
        }
        {
            CPLJSonStreamingWriter x(nullptr, nullptr);
            x.SetPrettyFormatting(false);
            {
                auto ctxt(x.MakeObjectContext());
                x.AddObjKey("key");
                x.Add("value");
            }
            ensure_equals( x.GetString(), std::string("{\"key\":\"value\"}") );
        }
        {
            CPLJSonStreamingWriter x(nullptr, nullptr);
            {
                auto ctxt(x.MakeObjectContext());
                x.AddObjKey("key");
                x.Add("value");
            }
            ensure_equals( x.GetString(), std::string("{\n  \"key\": \"value\"\n}") );
        }
        {
            CPLJSonStreamingWriter x(nullptr, nullptr);
            {
                auto ctxt(x.MakeObjectContext());
                x.AddObjKey("key");
                x.Add("value");
                x.AddObjKey("key2");
                x.Add("value2");
            }
            ensure_equals( x.GetString(), std::string("{\n  \"key\": \"value\",\n  \"key2\": \"value2\"\n}") );
        }
        {
            CPLJSonStreamingWriter x(nullptr, nullptr);
            {
                auto ctxt(x.MakeArrayContext());
            }
            ensure_equals( x.GetString(), std::string("[]") );
        }
        {
            CPLJSonStreamingWriter x(nullptr, nullptr);
            {
                auto ctxt(x.MakeArrayContext());
                x.Add(1);
            }
            ensure_equals( x.GetString(), std::string("[\n  1\n]") );
        }
        {
            CPLJSonStreamingWriter x(nullptr, nullptr);
            {
                auto ctxt(x.MakeArrayContext());
                x.Add(1);
                x.Add(2);
            }
            ensure_equals( x.GetString(), std::string("[\n  1,\n  2\n]") );
        }
        {
            CPLJSonStreamingWriter x(nullptr, nullptr);
            {
                auto ctxt(x.MakeArrayContext(true));
                x.Add(1);
                x.Add(2);
            }
            ensure_equals( x.GetString(), std::string("[1, 2]") );
        }
    }

    // Test CPLWorkerThreadPool
    template<>
    template<>
    void object::test<40>()
    {
        CPLWorkerThreadPool oPool;
        ensure(oPool.Setup(2, nullptr, nullptr));

        const auto myJob = [](void* pData)
        {
            (*static_cast<int*>(pData))++;
        };

        {
            std::vector<int> res(1000);
            for( int i = 0; i < 1000; i++ )
            {
                res[i] = i;
                oPool.SubmitJob(myJob, &res[i]);
            }
            oPool.WaitCompletion();
            for( int i = 0; i < 1000; i++ )
            {
                ensure_equals(res[i], i + 1);
            }
        }

        {
            std::vector<int> res(1000);
            std::vector<void*> resPtr(1000);
            for( int i = 0; i < 1000; i++ )
            {
                res[i] = i;
                resPtr[i] = &res[i];
            }
            oPool.SubmitJobs(myJob, resPtr);
            oPool.WaitEvent();
            oPool.WaitCompletion();
            for( int i = 0; i < 1000; i++ )
            {
                ensure_equals(res[i], i + 1);
            }
        }

        {
            auto jobQueue1 = oPool.CreateJobQueue();
            auto jobQueue2 = oPool.CreateJobQueue();

            ensure_equals(jobQueue1->GetPool(), &oPool);

            std::vector<int> res(1000);
            for( int i = 0; i < 1000; i++ )
            {
                res[i] = i;
                if( i % 2 )
                    jobQueue1->SubmitJob(myJob, &res[i]);
                else
                    jobQueue2->SubmitJob(myJob, &res[i]);
            }
            jobQueue1->WaitCompletion();
            jobQueue2->WaitCompletion();
            for( int i = 0; i < 1000; i++ )
            {
                ensure_equals(res[i], i + 1);
            }
        }
    }

    // Test CPLHTTPFetch
    template<>
    template<>
    void object::test<41>()
    {
#ifdef HAVE_CURL
        CPLStringList oOptions;
        oOptions.AddNameVlue("FORM_ITEM_COUNT", "5");
        oOptions.AddNameVlue("FORM_KEY_0", "qqq");
        oOptions.AddNameVlue("FORM_VALUE_0", "www");
        CPLHTTPResult *pResult = CPLHTTPFetch("http://example.com", oOptions);
        ensure_equals(pResult->nStatus, 34);
        CPLHTTPDestroyResult(pResult);
        pResult = nullptr;
        oOptions.Clear();

        oOptions.AddNameVlue("FORM_FILE_PATH", "not_existed");
        pResult = CPLHTTPFetch("http://example.com", oOptions);
        ensure_equals(pResult->nStatus, 34);
        CPLHTTPDestroyResult(pResult);

#endif // HAVE_CURL
    }

    // Test CPLHTTPPushFetchCallback
    template<>
    template<>
    void object::test<42>()
    {
        struct myCbkUserDataStruct
        {
            CPLString osURL{};
            CSLConstList papszOptions = nullptr;
            GDALProgressFunc pfnProgress = nullptr;
            void *pProgressArg = nullptr;
            CPLHTTPFetchWriteFunc pfnWrite = nullptr;
            void *pWriteArg = nullptr;
        };

        const auto myCbk = [](const char *pszURL,
                              CSLConstList papszOptions,
                              GDALProgressFunc pfnProgress,
                              void *pProgressArg,
                              CPLHTTPFetchWriteFunc pfnWrite,
                              void *pWriteArg,
                              void* pUserData )
        {
            myCbkUserDataStruct* pCbkUserData = static_cast<myCbkUserDataStruct*>(pUserData);
            pCbkUserData->osURL = pszURL;
            pCbkUserData->papszOptions = papszOptions;
            pCbkUserData->pfnProgress = pfnProgress;
            pCbkUserData->pProgressArg = pProgressArg;
            pCbkUserData->pfnWrite = pfnWrite;
            pCbkUserData->pWriteArg = pWriteArg;
            auto psResult = static_cast<CPLHTTPResult*>(CPLCalloc(sizeof(CPLHTTPResult), 1));
            psResult->nStatus = 123;
            return psResult;
        };

        myCbkUserDataStruct userData;
        ensure( CPLHTTPPushFetchCallback(myCbk, &userData) );

        int progressArg = 0;
        const auto myWriteCbk = [](void *, size_t, size_t, void *) -> size_t { return 0; };
        int writeCbkArg = 00;

        CPLStringList aosOptions;
        GDALProgressFunc pfnProgress = GDALTermProgress;
        CPLHTTPFetchWriteFunc pfnWriteCbk = myWriteCbk;
        CPLHTTPResult* pResult = CPLHTTPFetchEx("http://example.com",
                                                aosOptions.List(),
                                                pfnProgress,
                                                &progressArg,
                                                pfnWriteCbk,
                                                &writeCbkArg);
        ensure(pResult != nullptr);
        ensure_equals(pResult->nStatus, 123);
        CPLHTTPDestroyResult(pResult);

        ensure( CPLHTTPPopFetchCallback() );
        CPLPushErrorHandler(CPLQuietErrorHandler);
        ensure( !CPLHTTPPopFetchCallback() );
        CPLPopErrorHandler();

        ensure_equals( userData.osURL, std::string("http://example.com") );
        ensure_equals( userData.papszOptions, aosOptions.List() );
        ensure_equals( userData.pfnProgress, pfnProgress );
        ensure_equals( userData.pProgressArg, &progressArg );
        ensure_equals( userData.pfnWrite, pfnWriteCbk );
        ensure_equals( userData.pWriteArg, &writeCbkArg );
    }

    // Test CPLHTTPSetFetchCallback
    template<>
    template<>
    void object::test<43>()
    {
        struct myCbkUserDataStruct
        {
            CPLString osURL{};
            CSLConstList papszOptions = nullptr;
            GDALProgressFunc pfnProgress = nullptr;
            void *pProgressArg = nullptr;
            CPLHTTPFetchWriteFunc pfnWrite = nullptr;
            void *pWriteArg = nullptr;
        };

        const auto myCbk2 = [](const char *pszURL,
                              CSLConstList papszOptions,
                              GDALProgressFunc pfnProgress,
                              void *pProgressArg,
                              CPLHTTPFetchWriteFunc pfnWrite,
                              void *pWriteArg,
                              void* pUserData )
        {
            myCbkUserDataStruct* pCbkUserData = static_cast<myCbkUserDataStruct*>(pUserData);
            pCbkUserData->osURL = pszURL;
            pCbkUserData->papszOptions = papszOptions;
            pCbkUserData->pfnProgress = pfnProgress;
            pCbkUserData->pProgressArg = pProgressArg;
            pCbkUserData->pfnWrite = pfnWrite;
            pCbkUserData->pWriteArg = pWriteArg;
            auto psResult = static_cast<CPLHTTPResult*>(CPLCalloc(sizeof(CPLHTTPResult), 1));
            psResult->nStatus = 124;
            return psResult;
        };
        myCbkUserDataStruct userData2;
        CPLHTTPSetFetchCallback(myCbk2, &userData2);

        int progressArg = 0;
        const auto myWriteCbk = [](void *, size_t, size_t, void *) -> size_t { return 0; };
        int writeCbkArg = 00;

        CPLStringList aosOptions;
        GDALProgressFunc pfnProgress = GDALTermProgress;
        CPLHTTPFetchWriteFunc pfnWriteCbk = myWriteCbk;
        CPLHTTPResult* pResult = CPLHTTPFetchEx("http://example.com",
                                                aosOptions.List(),
                                                pfnProgress,
                                                &progressArg,
                                                pfnWriteCbk,
                                                &writeCbkArg);
        ensure(pResult != nullptr);
        ensure_equals(pResult->nStatus, 124);
        CPLHTTPDestroyResult(pResult);

        CPLHTTPSetFetchCallback(nullptr, nullptr);

        ensure_equals( userData2.osURL, std::string("http://example.com") );
        ensure_equals( userData2.papszOptions, aosOptions.List() );
        ensure_equals( userData2.pfnProgress, pfnProgress );
        ensure_equals( userData2.pProgressArg, &progressArg );
        ensure_equals( userData2.pfnWrite, pfnWriteCbk );
        ensure_equals( userData2.pWriteArg, &writeCbkArg );
    }

    // Test CPLLoadConfigOptionsFromFile() and CPLLoadConfigOptionsFromPredefinedFiles()
    template<>
    template<>
    void object::test<44>()
    {
        CPLLoadConfigOptionsFromFile("/i/do/not/exist", false);

        VSILFILE* fp = VSIFOpenL("/vsimem/.gdal/gdalrc", "wb");
        VSIFPrintfL(fp, "[configoptions]\n");
        VSIFPrintfL(fp, "# some comment\n");
        VSIFPrintfL(fp, "FOO_CONFIGOPTION=BAR\n");
        VSIFCloseL(fp);

        // Try CPLLoadConfigOptionsFromFile()
        CPLLoadConfigOptionsFromFile("/vsimem/.gdal/gdalrc", false);
        ensure( EQUAL(CPLGetConfigOption("FOO_CONFIGOPTION", ""), "BAR") );
        CPLSetConfigOption("FOO_CONFIGOPTION", nullptr);

        // Try CPLLoadConfigOptionsFromPredefinedFiles() with GDAL_CONFIG_FILE set
        CPLSetConfigOption("GDAL_CONFIG_FILE", "/vsimem/.gdal/gdalrc");
        CPLLoadConfigOptionsFromPredefinedFiles();
        ensure( EQUAL(CPLGetConfigOption("FOO_CONFIGOPTION", ""), "BAR") );
        CPLSetConfigOption("FOO_CONFIGOPTION", nullptr);

        // Try CPLLoadConfigOptionsFromPredefinedFiles() with $HOME/.gdal/gdalrc file
#ifdef WIN32
        const char* pszHOMEEnvVarName = "USERPROFILE";
#else
        const char* pszHOMEEnvVarName = "HOME";
#endif
        CPLString osOldVal(CPLGetConfigOption(pszHOMEEnvVarName, ""));
        CPLSetConfigOption(pszHOMEEnvVarName, "/vsimem/");
        CPLLoadConfigOptionsFromPredefinedFiles();
        ensure( EQUAL(CPLGetConfigOption("FOO_CONFIGOPTION", ""), "BAR") );
        CPLSetConfigOption("FOO_CONFIGOPTION", nullptr);
        if( !osOldVal.empty() )
            CPLSetConfigOption(pszHOMEEnvVarName, osOldVal.c_str());
        else
            CPLSetConfigOption(pszHOMEEnvVarName, nullptr);

        VSIUnlink("/vsimem/.gdal/gdalrc");
    }

    // Test decompressor side of cpl_compressor.h
    template<>
    template<>
    void object::test<45>()
    {
        const auto compressionLambda = [](const void* /* input_data */,
                                          size_t /* input_size */,
                                          void** /* output_data */,
                                          size_t* /* output_size */,
                                          CSLConstList /* options */,
                                          void* /* compressor_user_data */)
        {
            return false;
        };
        int dummy = 0;

        CPLCompressor sComp;
        sComp.nStructVersion = 1;
        sComp.eType = CCT_COMPRESSOR;
        sComp.pszId = "my_comp";
        const char* const apszMetadata[] = { "FOO=BAR", nullptr };
        sComp.papszMetadata = apszMetadata;
        sComp.pfnFunc = compressionLambda;
        sComp.user_data = &dummy;

        ensure( CPLRegisterDecompressor(&sComp) );

        CPLPushErrorHandler(CPLQuietErrorHandler);
        ensure( !CPLRegisterDecompressor(&sComp) );
        CPLPopErrorHandler();

        char** decompressors = CPLGetDecompressors();
        ensure( decompressors != nullptr );
        ensure( CSLFindString(decompressors, sComp.pszId) >= 0 );
        for( auto iter = decompressors; *iter; ++iter )
        {
            const auto pCompressor = CPLGetDecompressor(*iter);
            ensure( pCompressor );
            const char* pszOptions = CSLFetchNameValue(pCompressor->papszMetadata, "OPTIONS");
            if( pszOptions )
            {
                auto psNode = CPLParseXMLString(pszOptions);
                ensure(psNode);
                CPLDestroyXMLNode(psNode);
            }
            else
            {
                CPLDebug("TEST", "Decompressor %s has no OPTIONS", *iter);
            }
        }
        CSLDestroy( decompressors );

        ensure( CPLGetDecompressor("invalid") == nullptr );
        const auto pCompressor = CPLGetDecompressor(sComp.pszId);
        ensure( pCompressor );
        ensure_equals( std::string(pCompressor->pszId), std::string(sComp.pszId) );
        ensure_equals( CSLCount(pCompressor->papszMetadata), CSLCount(sComp.papszMetadata) );
        ensure( pCompressor->pfnFunc != nullptr );
        ensure_equals( pCompressor->user_data, sComp.user_data );

        CPLDestroyCompressorRegistry();
        ensure( CPLGetDecompressor(sComp.pszId) == nullptr );
    }

    // Test compressor side of cpl_compressor.h
    template<>
    template<>
    void object::test<46>()
    {
        const auto compressionLambda = [](const void* /* input_data */,
                                          size_t /* input_size */,
                                          void** /* output_data */,
                                          size_t* /* output_size */,
                                          CSLConstList /* options */,
                                          void* /* compressor_user_data */)
        {
            return false;
        };
        int dummy = 0;

        CPLCompressor sComp;
        sComp.nStructVersion = 1;
        sComp.eType = CCT_COMPRESSOR;
        sComp.pszId = "my_comp";
        const char* const apszMetadata[] = { "FOO=BAR", nullptr };
        sComp.papszMetadata = apszMetadata;
        sComp.pfnFunc = compressionLambda;
        sComp.user_data = &dummy;

        ensure( CPLRegisterCompressor(&sComp) );

        CPLPushErrorHandler(CPLQuietErrorHandler);
        ensure( !CPLRegisterCompressor(&sComp) );
        CPLPopErrorHandler();

        char** compressors = CPLGetCompressors();
        ensure( compressors != nullptr );
        ensure( CSLFindString(compressors, sComp.pszId) >= 0 );
        for( auto iter = compressors; *iter; ++iter )
        {
            const auto pCompressor = CPLGetCompressor(*iter);
            ensure( pCompressor );
            const char* pszOptions = CSLFetchNameValue(pCompressor->papszMetadata, "OPTIONS");
            if( pszOptions )
            {
                auto psNode = CPLParseXMLString(pszOptions);
                ensure(psNode);
                CPLDestroyXMLNode(psNode);
            }
            else
            {
                CPLDebug("TEST", "Compressor %s has no OPTIONS", *iter);
            }
        }
        CSLDestroy( compressors );

        ensure( CPLGetCompressor("invalid") == nullptr );
        const auto pCompressor = CPLGetCompressor(sComp.pszId);
        ensure( pCompressor );
        ensure_equals( std::string(pCompressor->pszId), std::string(sComp.pszId) );
        ensure_equals( CSLCount(pCompressor->papszMetadata), CSLCount(sComp.papszMetadata) );
        ensure( pCompressor->pfnFunc != nullptr );
        ensure_equals( pCompressor->user_data, sComp.user_data );

        CPLDestroyCompressorRegistry();
        ensure( CPLGetDecompressor(sComp.pszId) == nullptr );
    }

    // Test builtin compressors/decompressor
    template<>
    template<>
    void object::test<47>()
    {
        for( const char* id : { "blosc", "zlib", "gzip", "lzma", "zstd", "lz4" } )
        {
            const auto pCompressor = CPLGetCompressor(id);
            if( pCompressor == nullptr )
            {
                CPLDebug("TEST", "%s not available", id);
                if( strcmp(id, "zlib") == 0 || strcmp(id, "gzip") == 0 )
                {
                    ensure( false );
                }
                continue;
            }
            CPLDebug("TEST", "Testing %s", id);

            const char my_str[] = "my string to compress";
            const char* const options[] = { "TYPESIZE=1", nullptr };

            // Compressor side

            // Just get output size
            size_t out_size = 0;
            ensure( pCompressor->pfnFunc( my_str, strlen(my_str),
                                          nullptr, &out_size,
                                          options, pCompressor->user_data ) );
            ensure( out_size != 0 );

            // Let it alloc the output buffer
            void* out_buffer2 = nullptr;
            size_t out_size2 = 0;
            ensure( pCompressor->pfnFunc( my_str, strlen(my_str),
                                          &out_buffer2, &out_size2,
                                          options, pCompressor->user_data ) );
            ensure( out_buffer2 != nullptr );
            ensure( out_size2 != 0 );
            ensure( out_size2 <= out_size );

            std::vector<GByte> out_buffer3(out_size);

            // Provide not large enough buffer size
            size_t out_size3 = 1;
            void* out_buffer3_ptr = &out_buffer3[0];
            ensure( !(pCompressor->pfnFunc( my_str, strlen(my_str),
                                          &out_buffer3_ptr, &out_size3,
                                          options, pCompressor->user_data )) );

            // Provide the output buffer
            out_size3 = out_buffer3.size();
            out_buffer3_ptr = &out_buffer3[0];
            ensure( pCompressor->pfnFunc( my_str, strlen(my_str),
                                          &out_buffer3_ptr, &out_size3,
                                          options, pCompressor->user_data ) );
            ensure( out_buffer3_ptr != nullptr );
            ensure( out_buffer3_ptr == &out_buffer3[0] );
            ensure( out_size3 != 0 );
            ensure_equals( out_size3, out_size2 );

            out_buffer3.resize( out_size3 );
            out_buffer3_ptr = &out_buffer3[0];

            ensure( memcmp(out_buffer3_ptr, out_buffer2, out_size2) == 0 );

            CPLFree(out_buffer2);

            const std::vector<GByte> compressedData(out_buffer3);

            // Decompressor side
            const auto pDecompressor = CPLGetDecompressor(id);
            ensure( pDecompressor != nullptr );

            out_size = 0;
            ensure( pDecompressor->pfnFunc( compressedData.data(), compressedData.size(),
                                            nullptr, &out_size,
                                            nullptr, pDecompressor->user_data ) );
            ensure( out_size != 0 );
            ensure( out_size >= strlen(my_str) );

            out_buffer2 = nullptr;
            out_size2 = 0;
            ensure( pDecompressor->pfnFunc( compressedData.data(), compressedData.size(),
                                            &out_buffer2, &out_size2,
                                            options, pDecompressor->user_data ) );
            ensure( out_buffer2 != nullptr );
            ensure( out_size2 != 0 );
            ensure_equals( out_size2, strlen(my_str) );
            ensure( memcmp(out_buffer2, my_str, strlen(my_str)) == 0 );
            CPLFree(out_buffer2);

            out_buffer3.clear();
            out_buffer3.resize(out_size);
            out_size3 = out_buffer3.size();
            out_buffer3_ptr = &out_buffer3[0];
            ensure( pDecompressor->pfnFunc( compressedData.data(), compressedData.size(),
                                            &out_buffer3_ptr, &out_size3,
                                            options, pDecompressor->user_data ) );
            ensure( out_buffer3_ptr != nullptr );
            ensure( out_buffer3_ptr == &out_buffer3[0] );
            ensure_equals( out_size3, strlen(my_str) );
            ensure( memcmp(out_buffer3.data(), my_str, strlen(my_str)) == 0 );
        }
    }

    template<class T> struct TesterDelta
    {
        static void test(const char* dtypeOption)
        {
            const auto pCompressor = CPLGetCompressor("delta");
            ensure(pCompressor);
            const auto pDecompressor = CPLGetDecompressor("delta");
            ensure(pDecompressor);

            const T tabIn[] = { static_cast<T>(-2), 3, 1 };
            T tabCompress[3];
            T tabOut[3];
            const char* const apszOptions[] = { dtypeOption, nullptr };

            void* outPtr = &tabCompress[0];
            size_t outSize = sizeof(tabCompress);
            ensure( pCompressor->pfnFunc( &tabIn[0], sizeof(tabIn),
                                          &outPtr, &outSize,
                                          apszOptions, pCompressor->user_data) );
            ensure_equals(outSize, sizeof(tabCompress));

            // ensure_equals(tabCompress[0], 2);
            // ensure_equals(tabCompress[1], 1);
            // ensure_equals(tabCompress[2], -2);

            outPtr = &tabOut[0];
            outSize = sizeof(tabOut);
            ensure( pDecompressor->pfnFunc( &tabCompress[0], sizeof(tabCompress),
                                            &outPtr, &outSize,
                                            apszOptions, pDecompressor->user_data) );
            ensure_equals(outSize, sizeof(tabOut));
            ensure_equals(tabOut[0], tabIn[0]);
            ensure_equals(tabOut[1], tabIn[1]);
            ensure_equals(tabOut[2], tabIn[2]);
        }
    };

    // Test delta compressors/decompressor
    template<>
    template<>
    void object::test<48>()
    {
        TesterDelta<int8_t>::test("DTYPE=i1");

        TesterDelta<uint8_t>::test("DTYPE=u1");

        TesterDelta<int16_t>::test("DTYPE=i2");
        TesterDelta<int16_t>::test("DTYPE=<i2");
        TesterDelta<int16_t>::test("DTYPE=>i2");

        TesterDelta<uint16_t>::test("DTYPE=u2");
        TesterDelta<uint16_t>::test("DTYPE=<u2");
        TesterDelta<uint16_t>::test("DTYPE=>u2");

        TesterDelta<int32_t>::test("DTYPE=i4");
        TesterDelta<int32_t>::test("DTYPE=<i4");
        TesterDelta<int32_t>::test("DTYPE=>i4");

        TesterDelta<uint32_t>::test("DTYPE=u4");
        TesterDelta<uint32_t>::test("DTYPE=<u4");
        TesterDelta<uint32_t>::test("DTYPE=>u4");

        TesterDelta<int64_t>::test("DTYPE=i8");
        TesterDelta<int64_t>::test("DTYPE=<i8");
        TesterDelta<int64_t>::test("DTYPE=>i8");

        TesterDelta<uint64_t>::test("DTYPE=u8");
        TesterDelta<uint64_t>::test("DTYPE=<u8");
        TesterDelta<uint64_t>::test("DTYPE=>u8");

        TesterDelta<float>::test("DTYPE=f4");
#ifdef CPL_MSB
        TesterDelta<float>::test("DTYPE=>f4");
#else
        TesterDelta<float>::test("DTYPE=<f4");
#endif

        TesterDelta<double>::test("DTYPE=f8");
#ifdef CPL_MSB
        TesterDelta<double>::test("DTYPE=>f8");
#else
        TesterDelta<double>::test("DTYPE=<f8");
#endif

    }

    // Test CPLQuadTree
    template<>
    template<>
    void object::test<49>()
    {
        unsigned next = 0;

        const auto DummyRandInit = [&next](unsigned initValue)
        {
            next = initValue;
        };

        constexpr int MAX_RAND_VAL = 32767;

        // Slightly improved version of https://xkcd.com/221/, as suggested by
        // "man srand"
        const auto DummyRand = [&]()
        {
           next = next * 1103515245 + 12345;
           return((unsigned)(next/65536) % (MAX_RAND_VAL+1));
        };

        CPLRectObj globalbounds;
        globalbounds.minx = 0;
        globalbounds.miny = 0;
        globalbounds.maxx = 1;
        globalbounds.maxy = 1;

        auto hTree = CPLQuadTreeCreate(&globalbounds, nullptr);
        ensure(hTree != nullptr);

        const auto GenerateRandomRect = [&](CPLRectObj& rect)
        {
            rect.minx = double(DummyRand()) / MAX_RAND_VAL;
            rect.miny = double(DummyRand()) / MAX_RAND_VAL;
            rect.maxx = rect.minx + double(DummyRand()) / MAX_RAND_VAL * (1 - rect.minx);
            rect.maxy = rect.miny + double(DummyRand()) / MAX_RAND_VAL * (1 - rect.miny);
        };

        for( int j = 0; j < 2; j++ )
        {
            DummyRandInit(j);
            for( int i = 0; i < 1000; i++ )
            {
                CPLRectObj rect;
                GenerateRandomRect(rect);
                void* hFeature = reinterpret_cast<void*>(static_cast<uintptr_t>(i));
                CPLQuadTreeInsertWithBounds(hTree, hFeature, &rect);
            }

            {
                int nFeatureCount = 0;
                CPLFree(CPLQuadTreeSearch(hTree, &globalbounds, &nFeatureCount));
                ensure_equals(nFeatureCount, 1000);
            }

            DummyRandInit(j);
            for( int i = 0; i < 1000; i++ )
            {
                CPLRectObj rect;
                GenerateRandomRect(rect);
                void* hFeature = reinterpret_cast<void*>(static_cast<uintptr_t>(i));
                CPLQuadTreeRemove(hTree, hFeature, &rect);
            }

            {
                int nFeatureCount = 0;
                CPLFree(CPLQuadTreeSearch(hTree, &globalbounds, &nFeatureCount));
                ensure_equals(nFeatureCount, 0);
            }
        }

        CPLQuadTreeDestroy(hTree);
    }
    // Test bUnlinkAndSize on VSIGetMemFileBuffer
    template<>
    template<>
    void object::test<50>()
    {
        VSILFILE *fp = VSIFOpenL("/vsimem/test_unlink_and_seize.tif", "wb");
        VSIFWriteL("test", 5, 1, fp);
        GByte *pRawData = VSIGetMemFileBuffer("/vsimem/test_unlink_and_seize.tif", nullptr, true);
        ensure(EQUAL(reinterpret_cast<const char *>(pRawData), "test"));
        ensure(VSIGetMemFileBuffer("/vsimem/test_unlink_and_seize.tif", nullptr, false) == nullptr);
        ensure(VSIFOpenL("/vsimem/test_unlink_and_seize.tif", "r") == nullptr);
        ensure(VSIFReadL(pRawData, 5, 1, fp) == 0);
        ensure(VSIFWriteL(pRawData, 5, 1, fp) == 0);
        ensure(VSIFSeekL(fp, 0, SEEK_END) == 0);
        CPLFree(pRawData);
        VSIFCloseL(fp);
    }
} // namespace tut
