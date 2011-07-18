///////////////////////////////////////////////////////////////////////////////
// $Id$
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test general CPL features.
// Author:   Mateusz Loskot <mateusz@loskot.net>
// 
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2006, Mateusz Loskot <mateusz@loskot.net>
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

#include <tut.h>
#include <tut_gdal.h>
#include <gdal_common.h>
#include <string>
#include <fstream>
#include "cpl_list.h"
#include "cpl_hash_set.h"
#include "cpl_string.h"

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
    
            { "25", CPL_VALUE_INTEGER },
            { "-25", CPL_VALUE_INTEGER },
            { "+25", CPL_VALUE_INTEGER },
    
            { "25e 3", CPL_VALUE_STRING },
            { "25e.3", CPL_VALUE_STRING },
            { "-2-5e3", CPL_VALUE_STRING },
            { "2-5e3", CPL_VALUE_STRING },
            { "25.25.3", CPL_VALUE_STRING },
            { "25e25e3", CPL_VALUE_STRING },
        };
    
        size_t i;
        for(i=0;i < sizeof(apszTestStrings) / sizeof(apszTestStrings[0]); i++)
        {
            ensure(CPLGetValueType(apszTestStrings[i].testString) == apszTestStrings[i].expectedResult);
            if (CPLGetValueType(apszTestStrings[i].testString) != apszTestStrings[i].expectedResult)
                fprintf(stderr, "mismatch on item %d : value=\"%s\", expect_result=%d, result=%d\n", i,
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
        *pnSum += (int)(long)elt;
        return TRUE;
    }

    // Test cpl_hash_set API
    template<>
    template<>
    void object::test<4>()
    {
#define HASH_SET_SIZE   1000
        CPLHashSet* set = CPLHashSetNew(NULL, NULL, NULL);
        for(int i=0;i<HASH_SET_SIZE;i++)
        {
            ensure(CPLHashSetInsert(set, (void*)i) == TRUE);
        }
        ensure(CPLHashSetSize(set) == HASH_SET_SIZE);

        for(int i=0;i<HASH_SET_SIZE;i++)
        {
            ensure(CPLHashSetInsert(set, (void*)i) == FALSE);
        }
        ensure(CPLHashSetSize(set) == HASH_SET_SIZE);

        for(int i=0;i<HASH_SET_SIZE;i++)
        {
            ensure(CPLHashSetLookup(set, (const void*)i) == (const void*)i);
        }

        int sum = 0;
        CPLHashSetForeach(set, sumValues, &sum);
        ensure(sum == (HASH_SET_SIZE-1) * HASH_SET_SIZE / 2);

        for(int i=0;i<HASH_SET_SIZE;i++)
        {
            ensure(CPLHashSetRemove(set, (void*)i) == TRUE);
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
            char    *pszDecodedString = CPLRecode( oTestString.szString,
                oTestString.szEncoding, oReferenceString.szEncoding);
            size_t  nLength =
                MIN( strlen(pszDecodedString),
                     sizeof(oReferenceString.szEncoding) );
            ensure( memcmp(pszDecodedString, oReferenceString.szString,
                           nLength) == 0 );
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

        ensure( oCSL.List() == NULL );
        
        oCSL.AddString( "def" );
        oCSL.AddString( "abc" );

        ensure_equals( oCSL.Count(), 2 );
        ensure( EQUAL(oCSL[0], "def") );
        ensure( EQUAL(oCSL[1], "abc") );
        ensure( oCSL[17] == NULL );
        ensure( oCSL[-1] == NULL );
        ensure_equals( oCSL.FindString("abc"), 1 );

        CSLDestroy( oCSL.StealList() );
        ensure_equals( oCSL.Count(), 0 );
        ensure( oCSL.List() == NULL );
        
        // Test that the list will make an internal copy when needed to
        // modify a read-only list. 

        oCSL.AddString( "def" );
        oCSL.AddString( "abc" );

        CPLStringList  oCopy( oCSL.List(), FALSE );

        ensure_equals( oCSL.List(), oCopy.List() );
        ensure_equals( oCSL.Count(), oCopy.Count() );

        oCopy.AddString( "xyz" );
        ensure( oCSL.List() != oCopy.List() );
        ensure_equals( oCopy.Count(), 3 );
        ensure_equals( oCSL.Count(), 2 );
        ensure( EQUAL(oCopy[2], "xyz") );

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
        ensure_equals( oNVL.FetchBoolean( "BOOL", TRUE ), TRUE );
        ensure_equals( oNVL.FetchBoolean( "BOOL", FALSE ), FALSE );

        oNVL.SetNameValue( "BOOL", "YES" );
        ensure_equals( oNVL.FetchBoolean( "BOOL", TRUE ), TRUE );
        ensure_equals( oNVL.FetchBoolean( "BOOL", FALSE ), TRUE );

        oNVL.SetNameValue( "BOOL", "1" );
        ensure_equals( oNVL.FetchBoolean( "BOOL", FALSE ), TRUE );

        oNVL.SetNameValue( "BOOL", "0" );
        ensure_equals( oNVL.FetchBoolean( "BOOL", TRUE ), FALSE );

        oNVL.SetNameValue( "BOOL", "FALSE" );
        ensure_equals( oNVL.FetchBoolean( "BOOL", TRUE ), FALSE );

        oNVL.SetNameValue( "BOOL", "ON" );
        ensure_equals( oNVL.FetchBoolean( "BOOL", FALSE ), TRUE );
        
        // Test assignmenet operator.
        {
            CPLStringList oTemp;
            oTemp.AddString("test");
            oCopy = oTemp;
        }
        ensure( EQUAL(oCopy[0],"test") );
        
        oCopy = oCopy;
        ensure( EQUAL(oCopy[0],"test") );
        
        // Test copy constructor.
        CPLStringList oCopy2(oCopy);
        oCopy.Clear();
        ensure( EQUAL(oCopy2[0],"test") );
        
        // Test sorting
        CPLStringList oTestSort;
        oTestSort.AddNameValue("Z", "1");
        oTestSort.AddNameValue("L", "2");
        oTestSort.AddNameValue("T", "3");
        oTestSort.AddNameValue("A", "4");
        oTestSort.Sort();
        ensure( EQUAL(oTestSort[0],"A=4") );
        ensure( EQUAL(oTestSort[1],"L=2") );
        ensure( EQUAL(oTestSort[2],"T=3") );
        ensure( EQUAL(oTestSort[3],"Z=1") );
        ensure_equals( oTestSort[4], (const char*)NULL );
        
        // Test FetchNameValue() in a sorted list
        ensure( EQUAL(oTestSort.FetchNameValue("A"),"4") );
        ensure( EQUAL(oTestSort.FetchNameValue("L"),"2") );
        ensure( EQUAL(oTestSort.FetchNameValue("T"),"3") );
        ensure( EQUAL(oTestSort.FetchNameValue("Z"),"1") );
        
        // Test AddNameValue() in a sorted list
        oTestSort.AddNameValue("B", "5");
        ensure( EQUAL(oTestSort[0],"A=4") );
        ensure( EQUAL(oTestSort[1],"B=5") );
        ensure( EQUAL(oTestSort[2],"L=2") );
        ensure( EQUAL(oTestSort[3],"T=3") );
        ensure( EQUAL(oTestSort[4],"Z=1") );
        ensure_equals( oTestSort[5], (const char*)NULL );
        
        // Test SetNameValue() of an existing item in a sorted list
        oTestSort.SetNameValue("Z", "6");
        ensure( EQUAL(oTestSort[4],"Z=6") );
        
        // Test SetNameValue() of a non-existing item in a sorted list
        oTestSort.SetNameValue("W", "7");
        ensure( EQUAL(oTestSort[0],"A=4") );
        ensure( EQUAL(oTestSort[1],"B=5") );
        ensure( EQUAL(oTestSort[2],"L=2") );
        ensure( EQUAL(oTestSort[3],"T=3") );
        ensure( EQUAL(oTestSort[4],"W=7") );
        ensure( EQUAL(oTestSort[5],"Z=6") );
        ensure_equals( oTestSort[6], (const char*)NULL );
    }

} // namespace tut

