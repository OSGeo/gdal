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
#include <string>
#include "cpl_list.h"
#include "cpl_hash_set.h"
#include "cpl_string.h"

namespace tut
{

    // Common fixture with test data
    struct test_cpl_data
    {
        test_cpl_data()
        {
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
    
        int i;
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

} // namespace tut

