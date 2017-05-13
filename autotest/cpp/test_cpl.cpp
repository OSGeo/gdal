///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test general CPL features.
// Author:   Mateusz Loskot <mateusz@loskot.net>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2006, Mateusz Loskot <mateusz@loskot.net>
// Copyright (c) 2008-2012, Even Rouault <even dot rouault at mines-paris dot org>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the
// Free Software Foundation, Inc., 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA.
///////////////////////////////////////////////////////////////////////////////

#include "gdal_unit_test.h"

#include <cpl_error.h>
#include <cpl_hash_set.h>
#include <cpl_list.h>
#include <cpl_sha256.h>
#include <cpl_string.h>

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

        list = CPLListInsert(NULL, (void*)0, 0);
        ensure(CPLListCount(list) == 1);
        list = CPLListRemove(list, 2);
        ensure(CPLListCount(list) == 1);
        list = CPLListRemove(list, 1);
        ensure(CPLListCount(list) == 1);
        list = CPLListRemove(list, 0);
        ensure(CPLListCount(list) == 0);
        list = NULL;

        list = CPLListInsert(NULL, (void*)0, 2);
        ensure(CPLListCount(list) == 3);
        list = CPLListRemove(list, 2);
        ensure(CPLListCount(list) == 2);
        list = CPLListRemove(list, 1);
        ensure(CPLListCount(list) == 1);
        list = CPLListRemove(list, 0);
        ensure(CPLListCount(list) == 0);
        list = NULL;

        list = CPLListAppend(list, (void*)1);
        ensure(CPLListGet(list,0) == list);
        ensure(CPLListGet(list,1) == NULL);
        list = CPLListAppend(list, (void*)2);
        list = CPLListInsert(list, (void*)3, 2);
        ensure(CPLListCount(list) == 3);
        CPLListDestroy(list);
        list = NULL;

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
        list = NULL;

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
        ensure(list == NULL);
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

    int sumValues(void* elt, void* user_data)
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

        CPLHashSet* set = CPLHashSetNew(NULL, NULL, NULL);
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

        while ( !fin.eof() )
        {
            TestRecodeStruct oTestString;

            fin.read(oTestString.szEncoding, sizeof(oTestString.szEncoding));
            oTestString.szEncoding[sizeof(oTestString.szEncoding) - 1] = '\0';
            fin.read(oTestString.szString, sizeof(oTestString.szString));
            oTestString.szString[sizeof(oTestString.szString) - 1] = '\0';

            // Compare each string with the reference one
            CPLErrorReset();
            char    *pszDecodedString = CPLRecode( oTestString.szString,
                oTestString.szEncoding, oReferenceString.szEncoding);
            if( strstr(CPLGetLastErrorMsg(), "Recode from KOI8-R to UTF-8 not supported") != NULL )
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
            if( !bOK && (strstr(CPLGetConfigOption("TRAVIS_OS_NAME", ""), "osx") != NULL ||
                         strstr(CPLGetConfigOption("BUILD_NAME", ""), "osx") != NULL ||
                         getenv("DO_NOT_FAIL_ON_RECODE_ERRORS") != NULL))
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

        ensure( "7nil", oCSL.List() == NULL );

        oCSL.AddString( "def" );
        oCSL.AddString( "abc" );

        ensure_equals( "7", oCSL.Count(), 2 );
        ensure( "70", EQUAL(oCSL[0], "def") );
        ensure( "71", EQUAL(oCSL[1], "abc") );
        ensure( "72", oCSL[17] == NULL );
        ensure( "73", oCSL[-1] == NULL );
        ensure_equals( "74", oCSL.FindString("abc"), 1 );

        CSLDestroy( oCSL.StealList() );
        ensure_equals( "75", oCSL.Count(), 0 );
        ensure( "76", oCSL.List() == NULL );

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
        ensure( oNVL.FetchNameValue("MISSING") == NULL );

        oNVL.AddNameValue( "KEY1", "VALUE3" );
        ensure( EQUAL(oNVL.FetchNameValue("KEY1"),"VALUE1") );
        ensure( EQUAL(oNVL[2],"KEY1=VALUE3") );
        ensure( EQUAL(oNVL.FetchNameValueDef("MISSING","X"),"X") );

        oNVL.SetNameValue( "2KEY", "VALUE4" );
        ensure( EQUAL(oNVL.FetchNameValue("2KEY"),"VALUE4") );
        ensure_equals( oNVL.Count(), 3 );

        // make sure deletion works.
        oNVL.SetNameValue( "2KEY", NULL );
        ensure( oNVL.FetchNameValue("2KEY") == NULL );
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

        oCopy = oCopy;
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
        ensure_equals( "c8", oTestSort[4], (const char*)NULL );

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
        ensure_equals( "c18", oTestSort[5], (const char*)NULL );

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
        ensure_equals( "c26", oTestSort[6], (const char*)NULL );
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
        ensure( "94", oNVL.FetchNameValue("MISSING") == NULL );

        oNVL.AddNameValue( "KEY1", "VALUE3" );
        ensure_equals( "95", oNVL.Count(), 3 );
        ensure( "96", EQUAL(oNVL.FetchNameValue("KEY1"),"VALUE1") );
        ensure( "97", EQUAL(oNVL.FetchNameValueDef("MISSING","X"),"X") );

        oNVL.SetNameValue( "2KEY", "VALUE4" );
        ensure( "98", EQUAL(oNVL.FetchNameValue("2KEY"),"VALUE4") );
        ensure_equals( "99", oNVL.Count(), 3 );

        // make sure deletion works.
        oNVL.SetNameValue( "2KEY", NULL );
        ensure( "9a", oNVL.FetchNameValue("2KEY") == NULL );
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
            sprintf(szDigest + 2 * i, "%02x", abyDigest[i]);
        //fprintf(stderr, "%s\n", szDigest);
        ensure( "10.1", EQUAL(szDigest, "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8") );


        CPL_HMAC_SHA256("mysupersupersupersupersupersupersupersupersupersupersupersupersupersupersupersupersupersupersupersuperlongkey",
                        strlen("mysupersupersupersupersupersupersupersupersupersupersupersupersupersupersupersupersupersupersupersuperlongkey"),
                        "msg", 3,
                        abyDigest);
        for(int i=0;i<CPL_SHA256_HASH_SIZE;i++)
            sprintf(szDigest + 2 * i, "%02x", abyDigest[i]);
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
        ensure( "11.1", VSIMalloc2( ~(size_t)0, ~(size_t)0 ) == NULL );
        ensure( "11.1bis", CPLGetLastErrorType() != CE_None );

        CPLErrorReset();
        ensure( "11.2", VSIMalloc3( 1, ~(size_t)0, ~(size_t)0 ) == NULL );
        ensure( "11.2bis", CPLGetLastErrorType() != CE_None );

        CPLErrorReset();
        ensure( "11.3", VSIMalloc3( ~(size_t)0, 1, ~(size_t)0 ) == NULL );
        ensure( "11.3bis", CPLGetLastErrorType() != CE_None );

        CPLErrorReset();
        ensure( "11.4", VSIMalloc3( ~(size_t)0, ~(size_t)0, 1 ) == NULL );
        ensure( "11.4bis", CPLGetLastErrorType() != CE_None );

        if( !CSLTestBoolean(CPLGetConfigOption("SKIP_MEM_INTENSIVE_TEST", "NO")) )
        {
            // The following tests will fail because such allocations cannot succeed
#if SIZEOF_VOIDP == 8
            CPLErrorReset();
            ensure( "11.6", VSIMalloc( ~(size_t)0 ) == NULL );
            ensure( "11.6bis", CPLGetLastErrorType() == CE_None ); /* no error reported */

            CPLErrorReset();
            ensure( "11.7", VSIMalloc2( ~(size_t)0, 1 ) == NULL );
            ensure( "11.7bis", CPLGetLastErrorType() != CE_None );

            CPLErrorReset();
            ensure( "11.8", VSIMalloc3( ~(size_t)0, 1, 1 ) == NULL );
            ensure( "11.8bis", CPLGetLastErrorType() != CE_None );

            CPLErrorReset();
            ensure( "11.9", VSICalloc( ~(size_t)0, 1 ) == NULL );
            ensure( "11.9bis", CPLGetLastErrorType() == CE_None ); /* no error reported */

            CPLErrorReset();
            ensure( "11.10", VSIRealloc( NULL, ~(size_t)0 ) == NULL );
            ensure( "11.10bis", CPLGetLastErrorType() == CE_None ); /* no error reported */

            CPLErrorReset();
            ensure( "11.11", VSI_MALLOC_VERBOSE( ~(size_t)0 ) == NULL );
            ensure( "11.11bis", CPLGetLastErrorType() != CE_None );

            CPLErrorReset();
            ensure( "11.12", VSI_MALLOC2_VERBOSE( ~(size_t)0, 1 ) == NULL );
            ensure( "11.12bis", CPLGetLastErrorType() != CE_None );

            CPLErrorReset();
            ensure( "11.13", VSI_MALLOC3_VERBOSE( ~(size_t)0, 1, 1 ) == NULL );
            ensure( "11.13bis", CPLGetLastErrorType() != CE_None );

            CPLErrorReset();
            ensure( "11.14", VSI_CALLOC_VERBOSE( ~(size_t)0, 1 ) == NULL );
            ensure( "11.14bis", CPLGetLastErrorType() != CE_None );

            CPLErrorReset();
            ensure( "11.15", VSI_REALLOC_VERBOSE( NULL, ~(size_t)0 ) == NULL );
            ensure( "11.15bis", CPLGetLastErrorType() != CE_None );
#endif
        }

        CPLPopErrorHandler();

        // The following allocs will return NULL because of 0 byte alloc
        CPLErrorReset();
        ensure( "11.16", VSIMalloc2( 0, 1 ) == NULL );
        ensure( "11.16bis", CPLGetLastErrorType() == CE_None );
        ensure( "11.17", VSIMalloc2( 1, 0 ) == NULL );

        CPLErrorReset();
        ensure( "11.18", VSIMalloc3( 0, 1, 1 ) == NULL );
        ensure( "11.18bis", CPLGetLastErrorType() == CE_None );
        ensure( "11.19", VSIMalloc3( 1, 0, 1 ) == NULL );
        ensure( "11.20", VSIMalloc3( 1, 1, 0 ) == NULL );
    }

    template<>
    template<>
    void object::test<12>()
    {
        ensure( strcmp(CPLFormFilename("a", "b", NULL), "a/b") == 0 ||
                strcmp(CPLFormFilename("a", "b", NULL), "a\\b") == 0 );
        ensure( strcmp(CPLFormFilename("a/", "b", NULL), "a/b") == 0 ||
                strcmp(CPLFormFilename("a/", "b", NULL), "a\\b") == 0 );
        ensure( strcmp(CPLFormFilename("a\\", "b", NULL), "a/b") == 0 ||
                strcmp(CPLFormFilename("a\\", "b", NULL), "a\\b") == 0 );
        ensure_equals( CPLFormFilename(NULL, "a", "b"), "a.b");
        ensure_equals( CPLFormFilename(NULL, "a", ".b"), "a.b");
        ensure_equals( CPLFormFilename("/a", "..", NULL), "/");
        ensure_equals( CPLFormFilename("/a/", "..", NULL), "/");
        ensure_equals( CPLFormFilename("/a/b", "..", NULL), "/a");
        ensure_equals( CPLFormFilename("/a/b/", "..", NULL), "/a");
        ensure( EQUAL(CPLFormFilename("c:", "..", NULL), "c:/..") ||
                EQUAL(CPLFormFilename("c:", "..", NULL), "c:\\..") );
        ensure( EQUAL(CPLFormFilename("c:\\", "..", NULL), "c:/..") ||
                EQUAL(CPLFormFilename("c:\\", "..", NULL), "c:\\..") );
        ensure_equals( CPLFormFilename("c:\\a", "..", NULL), "c:");
        ensure_equals( CPLFormFilename("c:\\a\\", "..", NULL), "c:");
        ensure_equals( CPLFormFilename("c:\\a\\b", "..", NULL), "c:\\a");
        ensure_equals( CPLFormFilename("\\\\$\\c:\\a", "..", NULL), "\\\\$\\c:");
        ensure( EQUAL(CPLFormFilename("\\\\$\\c:", "..", NULL), "\\\\$\\c:/..") ||
                EQUAL(CPLFormFilename("\\\\$\\c:", "..", NULL), "\\\\$\\c:\\..") );
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

        CPLSetConfigOption("CPL_DEBUG", oldVal.size() ? oldVal.c_str() : NULL);
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
        ensure( ptr != NULL );
        ensure( ((size_t)ptr % sizeof(void*)) == 0 );
        *ptr = 1;
        VSIFreeAligned(ptr);

        ptr = static_cast<GByte*>(VSIMallocAligned(16, 1));
        ensure( ptr != NULL );
        ensure( ((size_t)ptr % 16) == 0 );
        *ptr = 1;
        VSIFreeAligned(ptr);

        VSIFreeAligned(NULL);

#ifndef WIN32
        // Illegal use of API. Returns non NULL on Windows
        ptr = static_cast<GByte*>(VSIMallocAligned(2, 1));
        ensure( ptr == NULL );

        // Illegal use of API. Crashes on Windows
        ptr = static_cast<GByte*>(VSIMallocAligned(5, 1));
        ensure( ptr == NULL );
#endif

        if( !CSLTestBoolean(CPLGetConfigOption("SKIP_MEM_INTENSIVE_TEST", "NO")) )
        {
            // The following tests will fail because such allocations cannot succeed
#if SIZEOF_VOIDP == 8
            ptr = static_cast<GByte*>(VSIMallocAligned(sizeof(void*), ~((size_t)0)));
            ensure( ptr == NULL );

            ptr = static_cast<GByte*>(VSIMallocAligned(sizeof(void*), (~((size_t)0)) - sizeof(void*)));
            ensure( ptr == NULL );
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
        CPLSetConfigOptions(NULL);
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
        CPLSetThreadLocalConfigOptions(NULL);
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
        ensure_equals ( CPLExpandTilde("~/bar"), "/foo/bar" );
        CPLSetConfigOption("HOME", NULL);
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
        char* pszText = CPLUnescapeString("&lt;&gt;&amp;&apos;&quot;&#x3f;&#x3F;&#63;", NULL, CPLES_XML);
        ensure_equals( CPLString(pszText), "<>&'\"???");
        CPLFree(pszText);

        // Integer overflow
        pszText = CPLUnescapeString("&10000000000000000;", NULL, CPLES_XML);
        // We do not really care about the return value
        CPLFree(pszText);

        // Integer overflow
        pszText = CPLUnescapeString("&#10000000000000000;", NULL, CPLES_XML);
        // We do not really care about the return value
        CPLFree(pszText);

        // Error case
        pszText = CPLUnescapeString("&foo", NULL, CPLES_XML);
        ensure_equals( CPLString(pszText), "");
        CPLFree(pszText);

        // Error case
        pszText = CPLUnescapeString("&#x", NULL, CPLES_XML);
        ensure_equals( CPLString(pszText), "");
        CPLFree(pszText);

        // Error case
        pszText = CPLUnescapeString("&#", NULL, CPLES_XML);
        ensure_equals( CPLString(pszText), "");
        CPLFree(pszText);
    }

} // namespace tut
