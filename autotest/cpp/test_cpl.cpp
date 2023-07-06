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
#include "cpl_mask.h"
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
#include "cpl_vsi_virtual.h"
#include "cpl_threadsafe_queue.hpp"

#include <atomic>
#include <limits>
#include <fstream>
#include <string>

#include "gtest_include.h"

static bool gbGotError = false;
static void CPL_STDCALL myErrorHandler(CPLErr, CPLErrorNum, const char *)
{
    gbGotError = true;
}

namespace
{

// Common fixture with test data
struct test_cpl : public ::testing::Test
{
    std::string data_;

    test_cpl()
    {
        // Compose data path for test group
        data_ = tut::common::data_basedir;
    }

    void SetUp() override
    {
        CPLSetConfigOptions(nullptr);
        CPLSetThreadLocalConfigOptions(nullptr);
    }
};

// Test cpl_list API
TEST_F(test_cpl, CPLList)
{
    CPLList *list;

    list = CPLListInsert(nullptr, (void *)nullptr, 0);
    EXPECT_TRUE(CPLListCount(list) == 1);
    list = CPLListRemove(list, 2);
    EXPECT_TRUE(CPLListCount(list) == 1);
    list = CPLListRemove(list, 1);
    EXPECT_TRUE(CPLListCount(list) == 1);
    list = CPLListRemove(list, 0);
    EXPECT_TRUE(CPLListCount(list) == 0);
    list = nullptr;

    list = CPLListInsert(nullptr, (void *)nullptr, 2);
    EXPECT_TRUE(CPLListCount(list) == 3);
    list = CPLListRemove(list, 2);
    EXPECT_TRUE(CPLListCount(list) == 2);
    list = CPLListRemove(list, 1);
    EXPECT_TRUE(CPLListCount(list) == 1);
    list = CPLListRemove(list, 0);
    EXPECT_TRUE(CPLListCount(list) == 0);
    list = nullptr;

    list = CPLListAppend(list, (void *)1);
    EXPECT_TRUE(CPLListGet(list, 0) == list);
    EXPECT_TRUE(CPLListGet(list, 1) == nullptr);
    list = CPLListAppend(list, (void *)2);
    list = CPLListInsert(list, (void *)3, 2);
    EXPECT_TRUE(CPLListCount(list) == 3);
    CPLListDestroy(list);
    list = nullptr;

    list = CPLListAppend(list, (void *)1);
    list = CPLListAppend(list, (void *)2);
    list = CPLListInsert(list, (void *)4, 3);
    CPLListGet(list, 2)->pData = (void *)3;
    EXPECT_TRUE(CPLListCount(list) == 4);
    EXPECT_TRUE(CPLListGet(list, 0)->pData == (void *)1);
    EXPECT_TRUE(CPLListGet(list, 1)->pData == (void *)2);
    EXPECT_TRUE(CPLListGet(list, 2)->pData == (void *)3);
    EXPECT_TRUE(CPLListGet(list, 3)->pData == (void *)4);
    CPLListDestroy(list);
    list = nullptr;

    list = CPLListInsert(list, (void *)4, 1);
    CPLListGet(list, 0)->pData = (void *)2;
    list = CPLListInsert(list, (void *)1, 0);
    list = CPLListInsert(list, (void *)3, 2);
    EXPECT_TRUE(CPLListCount(list) == 4);
    EXPECT_TRUE(CPLListGet(list, 0)->pData == (void *)1);
    EXPECT_TRUE(CPLListGet(list, 1)->pData == (void *)2);
    EXPECT_TRUE(CPLListGet(list, 2)->pData == (void *)3);
    EXPECT_TRUE(CPLListGet(list, 3)->pData == (void *)4);
    list = CPLListRemove(list, 1);
    list = CPLListRemove(list, 1);
    list = CPLListRemove(list, 0);
    list = CPLListRemove(list, 0);
    EXPECT_TRUE(list == nullptr);
}

typedef struct
{
    const char *testString;
    CPLValueType expectedResult;
} TestStringStruct;

// Test CPLGetValueType
TEST_F(test_cpl, CPLGetValueType)
{
    TestStringStruct apszTestStrings[] = {
        {"+25.e+3", CPL_VALUE_REAL},   {"-25.e-3", CPL_VALUE_REAL},
        {"25.e3", CPL_VALUE_REAL},     {"25e3", CPL_VALUE_REAL},
        {" 25e3 ", CPL_VALUE_REAL},    {".1e3", CPL_VALUE_REAL},

        {"25", CPL_VALUE_INTEGER},     {"-25", CPL_VALUE_INTEGER},
        {"+25", CPL_VALUE_INTEGER},

        {"25e 3", CPL_VALUE_STRING},   {"25e.3", CPL_VALUE_STRING},
        {"-2-5e3", CPL_VALUE_STRING},  {"2-5e3", CPL_VALUE_STRING},
        {"25.25.3", CPL_VALUE_STRING}, {"25e25e3", CPL_VALUE_STRING},
        {"25e2500", CPL_VALUE_STRING}, /* #6128 */

        {"d1", CPL_VALUE_STRING} /* #6305 */
    };

    size_t i;
    for (i = 0; i < sizeof(apszTestStrings) / sizeof(apszTestStrings[0]); i++)
    {
        EXPECT_EQ(CPLGetValueType(apszTestStrings[i].testString),
                  apszTestStrings[i].expectedResult)
            << apszTestStrings[i].testString;
    }
}

// Test cpl_hash_set API
TEST_F(test_cpl, CPLHashSet)
{
    CPLHashSet *set =
        CPLHashSetNew(CPLHashSetHashStr, CPLHashSetEqualStr, CPLFree);
    EXPECT_TRUE(CPLHashSetInsert(set, CPLStrdup("hello")) == TRUE);
    EXPECT_TRUE(CPLHashSetInsert(set, CPLStrdup("good morning")) == TRUE);
    EXPECT_TRUE(CPLHashSetInsert(set, CPLStrdup("bye bye")) == TRUE);
    EXPECT_TRUE(CPLHashSetSize(set) == 3);
    EXPECT_TRUE(CPLHashSetInsert(set, CPLStrdup("bye bye")) == FALSE);
    EXPECT_TRUE(CPLHashSetSize(set) == 3);
    EXPECT_TRUE(CPLHashSetRemove(set, "bye bye") == TRUE);
    EXPECT_TRUE(CPLHashSetSize(set) == 2);
    EXPECT_TRUE(CPLHashSetRemove(set, "good afternoon") == FALSE);
    EXPECT_TRUE(CPLHashSetSize(set) == 2);
    CPLHashSetDestroy(set);
}

static int sumValues(void *elt, void *user_data)
{
    int *pnSum = (int *)user_data;
    *pnSum += *(int *)elt;
    return TRUE;
}

// Test cpl_hash_set API
TEST_F(test_cpl, CPLHashSet2)
{
    const int HASH_SET_SIZE = 1000;

    int data[HASH_SET_SIZE];
    for (int i = 0; i < HASH_SET_SIZE; ++i)
    {
        data[i] = i;
    }

    CPLHashSet *set = CPLHashSetNew(nullptr, nullptr, nullptr);
    for (int i = 0; i < HASH_SET_SIZE; i++)
    {
        EXPECT_TRUE(CPLHashSetInsert(set, (void *)&data[i]) == TRUE);
    }
    EXPECT_EQ(CPLHashSetSize(set), HASH_SET_SIZE);

    for (int i = 0; i < HASH_SET_SIZE; i++)
    {
        EXPECT_TRUE(CPLHashSetInsert(set, (void *)&data[i]) == FALSE);
    }
    EXPECT_EQ(CPLHashSetSize(set), HASH_SET_SIZE);

    for (int i = 0; i < HASH_SET_SIZE; i++)
    {
        EXPECT_TRUE(CPLHashSetLookup(set, (const void *)&data[i]) ==
                    (const void *)&data[i]);
    }

    int sum = 0;
    CPLHashSetForeach(set, sumValues, &sum);
    EXPECT_EQ(sum, (HASH_SET_SIZE - 1) * HASH_SET_SIZE / 2);

    for (int i = 0; i < HASH_SET_SIZE; i++)
    {
        EXPECT_TRUE(CPLHashSetRemove(set, (void *)&data[i]) == TRUE);
    }
    EXPECT_EQ(CPLHashSetSize(set), 0);

    CPLHashSetDestroy(set);
}

// Test cpl_string API
TEST_F(test_cpl, CSLTokenizeString2)
{
    {
        CPLStringList aosStringList(
            CSLTokenizeString2("one two three", " ", 0));
        ASSERT_EQ(aosStringList.size(), 3);
        ASSERT_TRUE(EQUAL(aosStringList[0], "one"));
        ASSERT_TRUE(EQUAL(aosStringList[1], "two"));
        ASSERT_TRUE(EQUAL(aosStringList[2], "three"));

        // Test range-based for loop
        int i = 0;
        for (const char *pszVal : aosStringList)
        {
            EXPECT_STREQ(pszVal, aosStringList[i]);
            ++i;
        }
        EXPECT_EQ(i, 3);
    }
    {
        CPLStringList aosStringList;
        // Test range-based for loop on empty list
        int i = 0;
        for (const char *pszVal : aosStringList)
        {
            EXPECT_EQ(pszVal, nullptr);  // should not reach that point...
            ++i;
        }
        EXPECT_EQ(i, 0);
    }
    {
        CPLStringList aosStringList(
            CSLTokenizeString2("one two, three;four,five; six", " ;,", 0));
        ASSERT_EQ(aosStringList.size(), 6);
        ASSERT_TRUE(EQUAL(aosStringList[0], "one"));
        ASSERT_TRUE(EQUAL(aosStringList[1], "two"));
        ASSERT_TRUE(EQUAL(aosStringList[2], "three"));
        ASSERT_TRUE(EQUAL(aosStringList[3], "four"));
        ASSERT_TRUE(EQUAL(aosStringList[4], "five"));
        ASSERT_TRUE(EQUAL(aosStringList[5], "six"));
    }

    {
        CPLStringList aosStringList(CSLTokenizeString2(
            "one two,,,five,six", " ,", CSLT_ALLOWEMPTYTOKENS));
        ASSERT_EQ(aosStringList.size(), 6);
        ASSERT_TRUE(EQUAL(aosStringList[0], "one"));
        ASSERT_TRUE(EQUAL(aosStringList[1], "two"));
        ASSERT_TRUE(EQUAL(aosStringList[2], ""));
        ASSERT_TRUE(EQUAL(aosStringList[3], ""));
        ASSERT_TRUE(EQUAL(aosStringList[4], "five"));
        ASSERT_TRUE(EQUAL(aosStringList[5], "six"));
    }

    {
        CPLStringList aosStringList(CSLTokenizeString2(
            "one two,\"three,four ,\",five,six", " ,", CSLT_HONOURSTRINGS));
        ASSERT_EQ(aosStringList.size(), 5);
        ASSERT_TRUE(EQUAL(aosStringList[0], "one"));
        ASSERT_TRUE(EQUAL(aosStringList[1], "two"));
        ASSERT_TRUE(EQUAL(aosStringList[2], "three,four ,"));
        ASSERT_TRUE(EQUAL(aosStringList[3], "five"));
        ASSERT_TRUE(EQUAL(aosStringList[4], "six"));
    }

    {
        CPLStringList aosStringList(CSLTokenizeString2(
            "one two,\"three,four ,\",five,six", " ,", CSLT_PRESERVEQUOTES));
        ASSERT_EQ(aosStringList.size(), 7);
        ASSERT_TRUE(EQUAL(aosStringList[0], "one"));
        ASSERT_TRUE(EQUAL(aosStringList[1], "two"));
        ASSERT_TRUE(EQUAL(aosStringList[2], "\"three"));
        ASSERT_TRUE(EQUAL(aosStringList[3], "four"));
        ASSERT_TRUE(EQUAL(aosStringList[4], "\""));
        ASSERT_TRUE(EQUAL(aosStringList[5], "five"));
        ASSERT_TRUE(EQUAL(aosStringList[6], "six"));
    }

    {
        CPLStringList aosStringList(
            CSLTokenizeString2("one two,\"three,four ,\",five,six", " ,",
                               CSLT_HONOURSTRINGS | CSLT_PRESERVEQUOTES));
        ASSERT_EQ(aosStringList.size(), 5);
        ASSERT_TRUE(EQUAL(aosStringList[0], "one"));
        ASSERT_TRUE(EQUAL(aosStringList[1], "two"));
        ASSERT_TRUE(EQUAL(aosStringList[2], "\"three,four ,\""));
        ASSERT_TRUE(EQUAL(aosStringList[3], "five"));
        ASSERT_TRUE(EQUAL(aosStringList[4], "six"));
    }

    {
        CPLStringList aosStringList(
            CSLTokenizeString2("one \\two,\"three,\\four ,\",five,six", " ,",
                               CSLT_PRESERVEESCAPES));
        ASSERT_EQ(aosStringList.size(), 7);
        ASSERT_TRUE(EQUAL(aosStringList[0], "one"));
        ASSERT_TRUE(EQUAL(aosStringList[1], "\\two"));
        ASSERT_TRUE(EQUAL(aosStringList[2], "\"three"));
        ASSERT_TRUE(EQUAL(aosStringList[3], "\\four"));
        ASSERT_TRUE(EQUAL(aosStringList[4], "\""));
        ASSERT_TRUE(EQUAL(aosStringList[5], "five"));
        ASSERT_TRUE(EQUAL(aosStringList[6], "six"));
    }

    {
        CPLStringList aosStringList(
            CSLTokenizeString2("one \\two,\"three,\\four ,\",five,six", " ,",
                               CSLT_PRESERVEQUOTES | CSLT_PRESERVEESCAPES));
        ASSERT_EQ(aosStringList.size(), 7);
        ASSERT_TRUE(EQUAL(aosStringList[0], "one"));
        ASSERT_TRUE(EQUAL(aosStringList[1], "\\two"));
        ASSERT_TRUE(EQUAL(aosStringList[2], "\"three"));
        ASSERT_TRUE(EQUAL(aosStringList[3], "\\four"));
        ASSERT_TRUE(EQUAL(aosStringList[4], "\""));
        ASSERT_TRUE(EQUAL(aosStringList[5], "five"));
        ASSERT_TRUE(EQUAL(aosStringList[6], "six"));
    }

    {
        CPLStringList aosStringList(
            CSLTokenizeString2("one ,two, three, four ,five  ", ",", 0));
        ASSERT_EQ(aosStringList.size(), 5);
        ASSERT_TRUE(EQUAL(aosStringList[0], "one "));
        ASSERT_TRUE(EQUAL(aosStringList[1], "two"));
        ASSERT_TRUE(EQUAL(aosStringList[2], " three"));
        ASSERT_TRUE(EQUAL(aosStringList[3], " four "));
        ASSERT_TRUE(EQUAL(aosStringList[4], "five  "));
    }

    {
        CPLStringList aosStringList(CSLTokenizeString2(
            "one ,two, three, four ,five  ", ",", CSLT_STRIPLEADSPACES));
        ASSERT_EQ(aosStringList.size(), 5);
        ASSERT_TRUE(EQUAL(aosStringList[0], "one "));
        ASSERT_TRUE(EQUAL(aosStringList[1], "two"));
        ASSERT_TRUE(EQUAL(aosStringList[2], "three"));
        ASSERT_TRUE(EQUAL(aosStringList[3], "four "));
        ASSERT_TRUE(EQUAL(aosStringList[4], "five  "));
    }

    {
        CPLStringList aosStringList(CSLTokenizeString2(
            "one ,two, three, four ,five  ", ",", CSLT_STRIPENDSPACES));
        ASSERT_EQ(aosStringList.size(), 5);
        ASSERT_TRUE(EQUAL(aosStringList[0], "one"));
        ASSERT_TRUE(EQUAL(aosStringList[1], "two"));
        ASSERT_TRUE(EQUAL(aosStringList[2], " three"));
        ASSERT_TRUE(EQUAL(aosStringList[3], " four"));
        ASSERT_TRUE(EQUAL(aosStringList[4], "five"));
    }

    {
        CPLStringList aosStringList(
            CSLTokenizeString2("one ,two, three, four ,five  ", ",",
                               CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES));
        ASSERT_EQ(aosStringList.size(), 5);
        ASSERT_TRUE(EQUAL(aosStringList[0], "one"));
        ASSERT_TRUE(EQUAL(aosStringList[1], "two"));
        ASSERT_TRUE(EQUAL(aosStringList[2], "three"));
        ASSERT_TRUE(EQUAL(aosStringList[3], "four"));
        ASSERT_TRUE(EQUAL(aosStringList[4], "five"));
    }
}

typedef struct
{
    char szEncoding[24];
    char szString[1024 - 24];
} TestRecodeStruct;

// Test cpl_recode API
TEST_F(test_cpl, CPLRecode)
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
     * ENCODINGS="UTF-8 CP1251 KOI8-R UCS-2 UCS-2BE UCS-2LE UCS-4 UCS-4BE
     * UCS-4LE UTF-16 UTF-32" # The test string itself in UTF-8 encoding. # This
     * means "Improving GDAL internationalization." in Russian.
     * TESTSTRING="\u0423\u043b\u0443\u0447\u0448\u0430\u0435\u043c
     * \u0438\u043d\u0442\u0435\u0440\u043d\u0430\u0446\u0438\u043e\u043d\u0430\u043b\u0438\u0437\u0430\u0446\u0438\u044e
     * GDAL."
     *
     * RECORDSIZE=1024
     * ENCSIZE=24
     *
     * i=0
     * for enc in ${ENCODINGS}; do
     *  env printf "${enc}" | dd ibs=${RECORDSIZE} conv=sync obs=1
     * seek=$((${RECORDSIZE}*${i})) of="recode-rus.dat" status=noxfer env printf
     * "${TESTSTRING}" | iconv -t ${enc} | dd ibs=${RECORDSIZE} conv=sync obs=1
     * seek=$((${RECORDSIZE}*${i}+${ENCSIZE})) of="recode-rus.dat" status=noxfer
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
    fin.read(oReferenceString.szEncoding, sizeof(oReferenceString.szEncoding));
    oReferenceString.szEncoding[sizeof(oReferenceString.szEncoding) - 1] = '\0';
    fin.read(oReferenceString.szString, sizeof(oReferenceString.szString));
    oReferenceString.szString[sizeof(oReferenceString.szString) - 1] = '\0';

    while (true)
    {
        TestRecodeStruct oTestString;

        fin.read(oTestString.szEncoding, sizeof(oTestString.szEncoding));
        oTestString.szEncoding[sizeof(oTestString.szEncoding) - 1] = '\0';
        if (fin.eof())
            break;
        fin.read(oTestString.szString, sizeof(oTestString.szString));
        oTestString.szString[sizeof(oTestString.szString) - 1] = '\0';

        // Compare each string with the reference one
        CPLErrorReset();
        char *pszDecodedString =
            CPLRecode(oTestString.szString, oTestString.szEncoding,
                      oReferenceString.szEncoding);
        if (strstr(CPLGetLastErrorMsg(),
                   "Recode from CP1251 to UTF-8 not supported") != nullptr ||
            strstr(CPLGetLastErrorMsg(),
                   "Recode from KOI8-R to UTF-8 not supported") != nullptr)
        {
            CPLFree(pszDecodedString);
            break;
        }

        size_t nLength =
            MIN(strlen(pszDecodedString), sizeof(oReferenceString.szEncoding));
        bool bOK =
            (memcmp(pszDecodedString, oReferenceString.szString, nLength) == 0);
        // FIXME Some tests fail on Mac. Not sure why, but do not error out just
        // for that
        if (!bOK &&
            (strstr(CPLGetConfigOption("TRAVIS_OS_NAME", ""), "osx") !=
                 nullptr ||
             strstr(CPLGetConfigOption("BUILD_NAME", ""), "osx") != nullptr ||
             getenv("DO_NOT_FAIL_ON_RECODE_ERRORS") != nullptr))
        {
            fprintf(stderr, "Recode from %s failed\n", oTestString.szEncoding);
        }
        else
        {
#ifdef CPL_MSB
            if (!bOK && strcmp(oTestString.szEncoding, "UCS-2") == 0)
            {
                // Presumably the content in the test file is UCS-2LE, but
                // there's no way to know the byte order without a BOM
                fprintf(stderr, "Recode from %s failed\n",
                        oTestString.szEncoding);
            }
            else
#endif
            {
                EXPECT_TRUE(bOK) << "Recode from " << oTestString.szEncoding;
            }
        }
        CPLFree(pszDecodedString);
    }

    fin.close();
}

/************************************************************************/
/*                         CPLStringList tests                          */
/************************************************************************/
TEST_F(test_cpl, CPLStringList_Base)
{
    CPLStringList oCSL;

    ASSERT_TRUE(oCSL.List() == nullptr);

    oCSL.AddString("def");
    oCSL.AddString("abc");

    ASSERT_EQ(oCSL.Count(), 2);
    ASSERT_TRUE(EQUAL(oCSL[0], "def"));
    ASSERT_TRUE(EQUAL(oCSL[1], "abc"));
    ASSERT_TRUE(oCSL[17] == nullptr);
    ASSERT_TRUE(oCSL[-1] == nullptr);
    ASSERT_EQ(oCSL.FindString("abc"), 1);

    CSLDestroy(oCSL.StealList());
    ASSERT_EQ(oCSL.Count(), 0);
    ASSERT_TRUE(oCSL.List() == nullptr);

    // Test that the list will make an internal copy when needed to
    // modify a read-only list.

    oCSL.AddString("def");
    oCSL.AddString("abc");

    CPLStringList oCopy(oCSL.List(), FALSE);

    ASSERT_EQ(oCSL.List(), oCopy.List());
    ASSERT_EQ(oCSL.Count(), oCopy.Count());

    oCopy.AddString("xyz");
    ASSERT_TRUE(oCSL.List() != oCopy.List());
    ASSERT_EQ(oCopy.Count(), 3);
    ASSERT_EQ(oCSL.Count(), 2);
    ASSERT_TRUE(EQUAL(oCopy[2], "xyz"));
}

TEST_F(test_cpl, CPLStringList_NameValue)
{
    // Test some name=value handling stuff.
    CPLStringList oNVL;

    oNVL.AddNameValue("KEY1", "VALUE1");
    oNVL.AddNameValue("2KEY", "VALUE2");
    ASSERT_EQ(oNVL.Count(), 2);
    ASSERT_TRUE(EQUAL(oNVL.FetchNameValue("2KEY"), "VALUE2"));
    ASSERT_TRUE(oNVL.FetchNameValue("MISSING") == nullptr);

    oNVL.AddNameValue("KEY1", "VALUE3");
    ASSERT_TRUE(EQUAL(oNVL.FetchNameValue("KEY1"), "VALUE1"));
    ASSERT_TRUE(EQUAL(oNVL[2], "KEY1=VALUE3"));
    ASSERT_TRUE(EQUAL(oNVL.FetchNameValueDef("MISSING", "X"), "X"));

    oNVL.SetNameValue("2KEY", "VALUE4");
    ASSERT_TRUE(EQUAL(oNVL.FetchNameValue("2KEY"), "VALUE4"));
    ASSERT_EQ(oNVL.Count(), 3);

    // make sure deletion works.
    oNVL.SetNameValue("2KEY", nullptr);
    ASSERT_TRUE(oNVL.FetchNameValue("2KEY") == nullptr);
    ASSERT_EQ(oNVL.Count(), 2);

    // Test boolean support.
    ASSERT_EQ(oNVL.FetchBoolean("BOOL", TRUE), TRUE);
    ASSERT_EQ(oNVL.FetchBoolean("BOOL", FALSE), FALSE);

    oNVL.SetNameValue("BOOL", "YES");
    ASSERT_EQ(oNVL.FetchBoolean("BOOL", TRUE), TRUE);
    ASSERT_EQ(oNVL.FetchBoolean("BOOL", FALSE), TRUE);

    oNVL.SetNameValue("BOOL", "1");
    ASSERT_EQ(oNVL.FetchBoolean("BOOL", FALSE), TRUE);

    oNVL.SetNameValue("BOOL", "0");
    ASSERT_EQ(oNVL.FetchBoolean("BOOL", TRUE), FALSE);

    oNVL.SetNameValue("BOOL", "FALSE");
    ASSERT_EQ(oNVL.FetchBoolean("BOOL", TRUE), FALSE);

    oNVL.SetNameValue("BOOL", "ON");
    ASSERT_EQ(oNVL.FetchBoolean("BOOL", FALSE), TRUE);

    // Test assignment operator.
    CPLStringList oCopy;

    {
        CPLStringList oTemp;
        oTemp.AddString("test");
        oCopy = oTemp;
    }
    EXPECT_STREQ(oCopy[0], "test");

    auto &oCopyRef(oCopy);
    oCopy = oCopyRef;
    EXPECT_STREQ(oCopy[0], "test");

    // Test copy constructor.
    CPLStringList oCopy2(oCopy);
    EXPECT_EQ(oCopy2.Count(), oCopy.Count());
    oCopy.Clear();
    EXPECT_STREQ(oCopy2[0], "test");

    // Test move constructor
    CPLStringList oMoved(std::move(oCopy2));
    EXPECT_STREQ(oMoved[0], "test");

    // Test move assignment operator
    CPLStringList oMoved2;
    oMoved2 = std::move(oMoved);
    EXPECT_STREQ(oMoved2[0], "test");

    // Test sorting
    CPLStringList oTestSort;
    oTestSort.AddNameValue("Z", "1");
    oTestSort.AddNameValue("L", "2");
    oTestSort.AddNameValue("T", "3");
    oTestSort.AddNameValue("A", "4");
    oTestSort.Sort();
    EXPECT_STREQ(oTestSort[0], "A=4");
    EXPECT_STREQ(oTestSort[1], "L=2");
    EXPECT_STREQ(oTestSort[2], "T=3");
    EXPECT_STREQ(oTestSort[3], "Z=1");
    ASSERT_EQ(oTestSort[4], (const char *)nullptr);

    // Test FetchNameValue() in a sorted list
    EXPECT_STREQ(oTestSort.FetchNameValue("A"), "4");
    EXPECT_STREQ(oTestSort.FetchNameValue("L"), "2");
    EXPECT_STREQ(oTestSort.FetchNameValue("T"), "3");
    EXPECT_STREQ(oTestSort.FetchNameValue("Z"), "1");

    // Test AddNameValue() in a sorted list
    oTestSort.AddNameValue("B", "5");
    EXPECT_STREQ(oTestSort[0], "A=4");
    EXPECT_STREQ(oTestSort[1], "B=5");
    EXPECT_STREQ(oTestSort[2], "L=2");
    EXPECT_STREQ(oTestSort[3], "T=3");
    EXPECT_STREQ(oTestSort[4], "Z=1");
    ASSERT_EQ(oTestSort[5], (const char *)nullptr);

    // Test SetNameValue() of an existing item in a sorted list
    oTestSort.SetNameValue("Z", "6");
    EXPECT_STREQ(oTestSort[4], "Z=6");

    // Test SetNameValue() of a non-existing item in a sorted list
    oTestSort.SetNameValue("W", "7");
    EXPECT_STREQ(oTestSort[0], "A=4");
    EXPECT_STREQ(oTestSort[1], "B=5");
    EXPECT_STREQ(oTestSort[2], "L=2");
    EXPECT_STREQ(oTestSort[3], "T=3");
    EXPECT_STREQ(oTestSort[4], "W=7");
    EXPECT_STREQ(oTestSort[5], "Z=6");
    ASSERT_EQ(oTestSort[6], (const char *)nullptr);
}

TEST_F(test_cpl, CPLStringList_Sort)
{
    // Test some name=value handling stuff *with* sorting active.
    CPLStringList oNVL;

    oNVL.Sort();

    oNVL.AddNameValue("KEY1", "VALUE1");
    oNVL.AddNameValue("2KEY", "VALUE2");
    ASSERT_EQ(oNVL.Count(), 2);
    EXPECT_STREQ(oNVL.FetchNameValue("KEY1"), "VALUE1");
    EXPECT_STREQ(oNVL.FetchNameValue("2KEY"), "VALUE2");
    ASSERT_TRUE(oNVL.FetchNameValue("MISSING") == nullptr);

    oNVL.AddNameValue("KEY1", "VALUE3");
    ASSERT_EQ(oNVL.Count(), 3);
    EXPECT_STREQ(oNVL.FetchNameValue("KEY1"), "VALUE1");
    EXPECT_STREQ(oNVL.FetchNameValueDef("MISSING", "X"), "X");

    oNVL.SetNameValue("2KEY", "VALUE4");
    EXPECT_STREQ(oNVL.FetchNameValue("2KEY"), "VALUE4");
    ASSERT_EQ(oNVL.Count(), 3);

    // make sure deletion works.
    oNVL.SetNameValue("2KEY", nullptr);
    ASSERT_TRUE(oNVL.FetchNameValue("2KEY") == nullptr);
    ASSERT_EQ(oNVL.Count(), 2);

    // Test insertion logic pretty carefully.
    oNVL.Clear();
    ASSERT_TRUE(oNVL.IsSorted() == TRUE);

    oNVL.SetNameValue("B", "BB");
    oNVL.SetNameValue("A", "AA");
    oNVL.SetNameValue("D", "DD");
    oNVL.SetNameValue("C", "CC");

    // items should be in sorted order.
    EXPECT_STREQ(oNVL[0], "A=AA");
    EXPECT_STREQ(oNVL[1], "B=BB");
    EXPECT_STREQ(oNVL[2], "C=CC");
    EXPECT_STREQ(oNVL[3], "D=DD");

    EXPECT_STREQ(oNVL.FetchNameValue("A"), "AA");
    EXPECT_STREQ(oNVL.FetchNameValue("B"), "BB");
    EXPECT_STREQ(oNVL.FetchNameValue("C"), "CC");
    EXPECT_STREQ(oNVL.FetchNameValue("D"), "DD");
}

TEST_F(test_cpl, CPL_HMAC_SHA256)
{
    GByte abyDigest[CPL_SHA256_HASH_SIZE];
    char szDigest[2 * CPL_SHA256_HASH_SIZE + 1];

    CPL_HMAC_SHA256("key", 3, "The quick brown fox jumps over the lazy dog",
                    strlen("The quick brown fox jumps over the lazy dog"),
                    abyDigest);
    for (int i = 0; i < CPL_SHA256_HASH_SIZE; i++)
        snprintf(szDigest + 2 * i, sizeof(szDigest) - 2 * i, "%02x",
                 abyDigest[i]);
    // fprintf(stderr, "%s\n", szDigest);
    EXPECT_STREQ(
        szDigest,
        "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8");

    CPL_HMAC_SHA256(
        "mysupersupersupersupersupersupersupersupersupersupersupersupersupersup"
        "ersupersupersupersupersupersuperlongkey",
        strlen("mysupersupersupersupersupersupersupersupersupersupersupersupers"
               "upersupersupersupersupersupersupersuperlongkey"),
        "msg", 3, abyDigest);
    for (int i = 0; i < CPL_SHA256_HASH_SIZE; i++)
        snprintf(szDigest + 2 * i, sizeof(szDigest) - 2 * i, "%02x",
                 abyDigest[i]);
    // fprintf(stderr, "%s\n", szDigest);
    EXPECT_STREQ(
        szDigest,
        "a3051520761ed3cb43876b35ce2dd93ac5b332dc3bad898bb32086f7ac71ffc1");
}

TEST_F(test_cpl, VSIMalloc)
{
    CPLPushErrorHandler(CPLQuietErrorHandler);

    // The following tests will fail because of overflows

    {
        CPLErrorReset();
        void *ptr = VSIMalloc2(~(size_t)0, ~(size_t)0);
        EXPECT_EQ(ptr, nullptr);
        VSIFree(ptr);
        EXPECT_NE(CPLGetLastErrorType(), CE_None);
    }

    {
        CPLErrorReset();
        void *ptr = VSIMalloc3(1, ~(size_t)0, ~(size_t)0);
        EXPECT_EQ(ptr, nullptr);
        VSIFree(ptr);
        EXPECT_NE(CPLGetLastErrorType(), CE_None);
    }

    {
        CPLErrorReset();
        void *ptr = VSIMalloc3(~(size_t)0, 1, ~(size_t)0);
        EXPECT_EQ(ptr, nullptr);
        VSIFree(ptr);
        EXPECT_NE(CPLGetLastErrorType(), CE_None);
    }

    {
        CPLErrorReset();
        void *ptr = VSIMalloc3(~(size_t)0, ~(size_t)0, 1);
        EXPECT_EQ(ptr, nullptr);
        VSIFree(ptr);
        EXPECT_NE(CPLGetLastErrorType(), CE_None);
    }

    if (!CSLTestBoolean(CPLGetConfigOption("SKIP_MEM_INTENSIVE_TEST", "NO")))
    {
        // The following tests will fail because such allocations cannot succeed
#if SIZEOF_VOIDP == 8
        {
            CPLErrorReset();
            void *ptr = VSIMalloc(~(size_t)0);
            EXPECT_EQ(ptr, nullptr);
            VSIFree(ptr);
            EXPECT_EQ(CPLGetLastErrorType(), CE_None); /* no error reported */
        }

        {
            CPLErrorReset();
            void *ptr = VSIMalloc2(~(size_t)0, 1);
            EXPECT_EQ(ptr, nullptr);
            VSIFree(ptr);
            EXPECT_NE(CPLGetLastErrorType(), CE_None);
        }

        {
            CPLErrorReset();
            void *ptr = VSIMalloc3(~(size_t)0, 1, 1);
            EXPECT_EQ(ptr, nullptr);
            VSIFree(ptr);
            EXPECT_NE(CPLGetLastErrorType(), CE_None);
        }

        {
            CPLErrorReset();
            void *ptr = VSICalloc(~(size_t)0, 1);
            EXPECT_EQ(ptr, nullptr);
            VSIFree(ptr);
            EXPECT_EQ(CPLGetLastErrorType(), CE_None); /* no error reported */
        }

        {
            CPLErrorReset();
            void *ptr = VSIRealloc(nullptr, ~(size_t)0);
            EXPECT_EQ(ptr, nullptr);
            VSIFree(ptr);
            EXPECT_EQ(CPLGetLastErrorType(), CE_None); /* no error reported */
        }

        {
            CPLErrorReset();
            void *ptr = VSI_MALLOC_VERBOSE(~(size_t)0);
            EXPECT_EQ(ptr, nullptr);
            VSIFree(ptr);
            EXPECT_NE(CPLGetLastErrorType(), CE_None);
        }

        {
            CPLErrorReset();
            void *ptr = VSI_MALLOC2_VERBOSE(~(size_t)0, 1);
            EXPECT_EQ(ptr, nullptr);
            VSIFree(ptr);
            EXPECT_NE(CPLGetLastErrorType(), CE_None);
        }

        {
            CPLErrorReset();
            void *ptr = VSI_MALLOC3_VERBOSE(~(size_t)0, 1, 1);
            EXPECT_EQ(ptr, nullptr);
            VSIFree(ptr);
            EXPECT_NE(CPLGetLastErrorType(), CE_None);
        }

        {
            CPLErrorReset();
            void *ptr = VSI_CALLOC_VERBOSE(~(size_t)0, 1);
            EXPECT_EQ(ptr, nullptr);
            VSIFree(ptr);
            EXPECT_NE(CPLGetLastErrorType(), CE_None);
        }

        {
            CPLErrorReset();
            void *ptr = VSI_REALLOC_VERBOSE(nullptr, ~(size_t)0);
            EXPECT_EQ(ptr, nullptr);
            VSIFree(ptr);
            EXPECT_NE(CPLGetLastErrorType(), CE_None);
        }
#endif
    }

    CPLPopErrorHandler();

    // The following allocs will return NULL because of 0 byte alloc
    {
        CPLErrorReset();
        void *ptr = VSIMalloc2(0, 1);
        EXPECT_EQ(ptr, nullptr);
        VSIFree(ptr);
        EXPECT_EQ(CPLGetLastErrorType(), CE_None);
    }

    {
        void *ptr = VSIMalloc2(1, 0);
        EXPECT_EQ(ptr, nullptr);
        VSIFree(ptr);
    }

    {
        CPLErrorReset();
        void *ptr = VSIMalloc3(0, 1, 1);
        EXPECT_EQ(ptr, nullptr);
        VSIFree(ptr);
        EXPECT_EQ(CPLGetLastErrorType(), CE_None);
    }

    {
        void *ptr = VSIMalloc3(1, 0, 1);
        EXPECT_EQ(ptr, nullptr);
        VSIFree(ptr);
    }

    {
        void *ptr = VSIMalloc3(1, 1, 0);
        EXPECT_EQ(ptr, nullptr);
        VSIFree(ptr);
    }
}

TEST_F(test_cpl, CPLFormFilename)
{
    EXPECT_TRUE(strcmp(CPLFormFilename("a", "b", nullptr), "a/b") == 0 ||
                strcmp(CPLFormFilename("a", "b", nullptr), "a\\b") == 0);
    EXPECT_TRUE(strcmp(CPLFormFilename("a/", "b", nullptr), "a/b") == 0 ||
                strcmp(CPLFormFilename("a/", "b", nullptr), "a\\b") == 0);
    EXPECT_TRUE(strcmp(CPLFormFilename("a\\", "b", nullptr), "a/b") == 0 ||
                strcmp(CPLFormFilename("a\\", "b", nullptr), "a\\b") == 0);
    EXPECT_STREQ(CPLFormFilename(nullptr, "a", "b"), "a.b");
    EXPECT_STREQ(CPLFormFilename(nullptr, "a", ".b"), "a.b");
    EXPECT_STREQ(CPLFormFilename("/a", "..", nullptr), "/");
    EXPECT_STREQ(CPLFormFilename("/a/", "..", nullptr), "/");
    EXPECT_STREQ(CPLFormFilename("/a/b", "..", nullptr), "/a");
    EXPECT_STREQ(CPLFormFilename("/a/b/", "..", nullptr), "/a");
    EXPECT_TRUE(EQUAL(CPLFormFilename("c:", "..", nullptr), "c:/..") ||
                EQUAL(CPLFormFilename("c:", "..", nullptr), "c:\\.."));
    EXPECT_TRUE(EQUAL(CPLFormFilename("c:\\", "..", nullptr), "c:/..") ||
                EQUAL(CPLFormFilename("c:\\", "..", nullptr), "c:\\.."));
    EXPECT_STREQ(CPLFormFilename("c:\\a", "..", nullptr), "c:");
    EXPECT_STREQ(CPLFormFilename("c:\\a\\", "..", nullptr), "c:");
    EXPECT_STREQ(CPLFormFilename("c:\\a\\b", "..", nullptr), "c:\\a");
    EXPECT_STREQ(CPLFormFilename("\\\\$\\c:\\a", "..", nullptr), "\\\\$\\c:");
    EXPECT_TRUE(
        EQUAL(CPLFormFilename("\\\\$\\c:", "..", nullptr), "\\\\$\\c:/..") ||
        EQUAL(CPLFormFilename("\\\\$\\c:", "..", nullptr), "\\\\$\\c:\\.."));
}

TEST_F(test_cpl, VSIGetDiskFreeSpace)
{
    ASSERT_TRUE(VSIGetDiskFreeSpace("/vsimem/") > 0);
    ASSERT_TRUE(VSIGetDiskFreeSpace(".") == -1 ||
                VSIGetDiskFreeSpace(".") >= 0);
}

TEST_F(test_cpl, CPLsscanf)
{
    double a, b, c;

    a = b = 0;
    ASSERT_EQ(CPLsscanf("1 2", "%lf %lf", &a, &b), 2);
    ASSERT_EQ(a, 1.0);
    ASSERT_EQ(b, 2.0);

    a = b = 0;
    ASSERT_EQ(CPLsscanf("1\t2", "%lf %lf", &a, &b), 2);
    ASSERT_EQ(a, 1.0);
    ASSERT_EQ(b, 2.0);

    a = b = 0;
    ASSERT_EQ(CPLsscanf("1 2", "%lf\t%lf", &a, &b), 2);
    ASSERT_EQ(a, 1.0);
    ASSERT_EQ(b, 2.0);

    a = b = 0;
    ASSERT_EQ(CPLsscanf("1  2", "%lf %lf", &a, &b), 2);
    ASSERT_EQ(a, 1.0);
    ASSERT_EQ(b, 2.0);

    a = b = 0;
    ASSERT_EQ(CPLsscanf("1 2", "%lf  %lf", &a, &b), 2);
    ASSERT_EQ(a, 1.0);
    ASSERT_EQ(b, 2.0);

    a = b = c = 0;
    ASSERT_EQ(CPLsscanf("1 2", "%lf %lf %lf", &a, &b, &c), 2);
    ASSERT_EQ(a, 1.0);
    ASSERT_EQ(b, 2.0);
}

TEST_F(test_cpl, CPLSetErrorHandler)
{
    CPLString oldVal = CPLGetConfigOption("CPL_DEBUG", "");
    CPLSetConfigOption("CPL_DEBUG", "TEST");

    CPLErrorHandler oldHandler = CPLSetErrorHandler(myErrorHandler);
    gbGotError = false;
    CPLDebug("TEST", "Test");
    ASSERT_EQ(gbGotError, true);
    gbGotError = false;
    CPLSetErrorHandler(oldHandler);

    CPLPushErrorHandler(myErrorHandler);
    gbGotError = false;
    CPLDebug("TEST", "Test");
    ASSERT_EQ(gbGotError, true);
    gbGotError = false;
    CPLPopErrorHandler();

    oldHandler = CPLSetErrorHandler(myErrorHandler);
    CPLSetCurrentErrorHandlerCatchDebug(FALSE);
    gbGotError = false;
    CPLDebug("TEST", "Test");
    ASSERT_EQ(gbGotError, false);
    gbGotError = false;
    CPLSetErrorHandler(oldHandler);

    CPLPushErrorHandler(myErrorHandler);
    CPLSetCurrentErrorHandlerCatchDebug(FALSE);
    gbGotError = false;
    CPLDebug("TEST", "Test");
    ASSERT_EQ(gbGotError, false);
    gbGotError = false;
    CPLPopErrorHandler();

    CPLSetConfigOption("CPL_DEBUG", oldVal.size() ? oldVal.c_str() : nullptr);

    oldHandler = CPLSetErrorHandler(nullptr);
    CPLDebug("TEST", "Test");
    CPLError(CE_Failure, CPLE_AppDefined, "test");
    CPLErrorHandler newOldHandler = CPLSetErrorHandler(nullptr);
    ASSERT_EQ(newOldHandler, static_cast<CPLErrorHandler>(nullptr));
    CPLDebug("TEST", "Test");
    CPLError(CE_Failure, CPLE_AppDefined, "test");
    CPLSetErrorHandler(oldHandler);
}

/************************************************************************/
/*                         CPLString::replaceAll()                      */
/************************************************************************/

TEST_F(test_cpl, CPLString_replaceAll)
{
    CPLString osTest;
    osTest = "foobarbarfoo";
    osTest.replaceAll("bar", "was_bar");
    ASSERT_EQ(osTest, "foowas_barwas_barfoo");

    osTest = "foobarbarfoo";
    osTest.replaceAll("X", "was_bar");
    ASSERT_EQ(osTest, "foobarbarfoo");

    osTest = "foobarbarfoo";
    osTest.replaceAll("", "was_bar");
    ASSERT_EQ(osTest, "foobarbarfoo");

    osTest = "foobarbarfoo";
    osTest.replaceAll("bar", "");
    ASSERT_EQ(osTest, "foofoo");

    osTest = "foobarbarfoo";
    osTest.replaceAll('b', 'B');
    ASSERT_EQ(osTest, "fooBarBarfoo");

    osTest = "foobarbarfoo";
    osTest.replaceAll('b', "B");
    ASSERT_EQ(osTest, "fooBarBarfoo");

    osTest = "foobarbarfoo";
    osTest.replaceAll("b", 'B');
    ASSERT_EQ(osTest, "fooBarBarfoo");
}

/************************************************************************/
/*                        VSIMallocAligned()                            */
/************************************************************************/
TEST_F(test_cpl, VSIMallocAligned)
{
    GByte *ptr = static_cast<GByte *>(VSIMallocAligned(sizeof(void *), 1));
    ASSERT_TRUE(ptr != nullptr);
    ASSERT_TRUE(((size_t)ptr % sizeof(void *)) == 0);
    *ptr = 1;
    VSIFreeAligned(ptr);

    ptr = static_cast<GByte *>(VSIMallocAligned(16, 1));
    ASSERT_TRUE(ptr != nullptr);
    ASSERT_TRUE(((size_t)ptr % 16) == 0);
    *ptr = 1;
    VSIFreeAligned(ptr);

    VSIFreeAligned(nullptr);

#ifndef WIN32
    // Illegal use of API. Returns non NULL on Windows
    ptr = static_cast<GByte *>(VSIMallocAligned(2, 1));
    EXPECT_TRUE(ptr == nullptr);
    VSIFree(ptr);

    // Illegal use of API. Crashes on Windows
    ptr = static_cast<GByte *>(VSIMallocAligned(5, 1));
    EXPECT_TRUE(ptr == nullptr);
    VSIFree(ptr);
#endif

    if (!CSLTestBoolean(CPLGetConfigOption("SKIP_MEM_INTENSIVE_TEST", "NO")))
    {
        // The following tests will fail because such allocations cannot succeed
#if SIZEOF_VOIDP == 8
        ptr = static_cast<GByte *>(
            VSIMallocAligned(sizeof(void *), ~((size_t)0)));
        EXPECT_TRUE(ptr == nullptr);
        VSIFree(ptr);

        ptr = static_cast<GByte *>(
            VSIMallocAligned(sizeof(void *), (~((size_t)0)) - sizeof(void *)));
        EXPECT_TRUE(ptr == nullptr);
        VSIFree(ptr);
#endif
    }
}

/************************************************************************/
/*             CPLGetConfigOptions() / CPLSetConfigOptions()            */
/************************************************************************/
TEST_F(test_cpl, CPLGetConfigOptions)
{
    CPLSetConfigOption("FOOFOO", "BAR");
    char **options = CPLGetConfigOptions();
    EXPECT_STREQ(CSLFetchNameValue(options, "FOOFOO"), "BAR");
    CPLSetConfigOptions(nullptr);
    EXPECT_STREQ(CPLGetConfigOption("FOOFOO", "i_dont_exist"), "i_dont_exist");
    CPLSetConfigOptions(options);
    EXPECT_STREQ(CPLGetConfigOption("FOOFOO", "i_dont_exist"), "BAR");
    CSLDestroy(options);
}

/************************************************************************/
/*  CPLGetThreadLocalConfigOptions() / CPLSetThreadLocalConfigOptions() */
/************************************************************************/
TEST_F(test_cpl, CPLGetThreadLocalConfigOptions)
{
    CPLSetThreadLocalConfigOption("FOOFOO", "BAR");
    char **options = CPLGetThreadLocalConfigOptions();
    EXPECT_STREQ(CSLFetchNameValue(options, "FOOFOO"), "BAR");
    CPLSetThreadLocalConfigOptions(nullptr);
    EXPECT_STREQ(CPLGetThreadLocalConfigOption("FOOFOO", "i_dont_exist"),
                 "i_dont_exist");
    CPLSetThreadLocalConfigOptions(options);
    EXPECT_STREQ(CPLGetThreadLocalConfigOption("FOOFOO", "i_dont_exist"),
                 "BAR");
    CSLDestroy(options);
}

TEST_F(test_cpl, CPLExpandTilde)
{
    EXPECT_STREQ(CPLExpandTilde("/foo/bar"), "/foo/bar");

    CPLSetConfigOption("HOME", "/foo");
    ASSERT_TRUE(EQUAL(CPLExpandTilde("~/bar"), "/foo/bar") ||
                EQUAL(CPLExpandTilde("~/bar"), "/foo\\bar"));
    CPLSetConfigOption("HOME", nullptr);
}

TEST_F(test_cpl, CPLString_constructors)
{
    // CPLString(std::string) constructor
    ASSERT_STREQ(CPLString(std::string("abc")).c_str(), "abc");

    // CPLString(const char*) constructor
    ASSERT_STREQ(CPLString("abc").c_str(), "abc");

    // CPLString(const char*, n) constructor
    ASSERT_STREQ(CPLString("abc", 1).c_str(), "a");
}

TEST_F(test_cpl, CPLErrorSetState)
{
    // NOTE: Assumes cpl_error.cpp defines DEFAULT_LAST_ERR_MSG_SIZE=500
    char pszMsg[] = "0abcdefghijklmnopqrstuvwxyz0123456789!@#$%&*()_+=|"
                    "1abcdefghijklmnopqrstuvwxyz0123456789!@#$%&*()_+=|"
                    "2abcdefghijklmnopqrstuvwxyz0123456789!@#$%&*()_+=|"
                    "3abcdefghijklmnopqrstuvwxyz0123456789!@#$%&*()_+=|"
                    "4abcdefghijklmnopqrstuvwxyz0123456789!@#$%&*()_+=|"
                    "5abcdefghijklmnopqrstuvwxyz0123456789!@#$%&*()_+=|"
                    "6abcdefghijklmnopqrstuvwxyz0123456789!@#$%&*()_+=|"
                    "7abcdefghijklmnopqrstuvwxyz0123456789!@#$%&*()_+=|"
                    "8abcdefghijklmnopqrstuvwxyz0123456789!@#$%&*()_+=|"
                    "9abcdefghijklmnopqrstuvwxyz0123456789!@#$%&*()_+=|"  // 500
                    "0abcdefghijklmnopqrstuvwxyz0123456789!@#$%&*()_+=|"  // 550
        ;

    CPLErrorReset();
    CPLErrorSetState(CE_Warning, 1, pszMsg);
    ASSERT_EQ(strlen(pszMsg) - 50 - 1,  // length - 50 - 1 (null-terminator)
              strlen(CPLGetLastErrorMsg()));  // DEFAULT_LAST_ERR_MSG_SIZE - 1
}

TEST_F(test_cpl, CPLUnescapeString)
{
    char *pszText = CPLUnescapeString(
        "&lt;&gt;&amp;&apos;&quot;&#x3f;&#x3F;&#63;", nullptr, CPLES_XML);
    EXPECT_STREQ(pszText, "<>&'\"???");
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
    EXPECT_STREQ(pszText, "");
    CPLFree(pszText);

    // Error case
    pszText = CPLUnescapeString("&#x", nullptr, CPLES_XML);
    EXPECT_STREQ(pszText, "");
    CPLFree(pszText);

    // Error case
    pszText = CPLUnescapeString("&#", nullptr, CPLES_XML);
    EXPECT_STREQ(pszText, "");
    CPLFree(pszText);
}

// Test signed int safe maths
TEST_F(test_cpl, CPLSM_signed)
{
    ASSERT_EQ((CPLSM(-2) + CPLSM(3)).v(), 1);
    ASSERT_EQ((CPLSM(-2) + CPLSM(1)).v(), -1);
    ASSERT_EQ((CPLSM(-2) + CPLSM(-1)).v(), -3);
    ASSERT_EQ((CPLSM(2) + CPLSM(-3)).v(), -1);
    ASSERT_EQ((CPLSM(2) + CPLSM(-1)).v(), 1);
    ASSERT_EQ((CPLSM(2) + CPLSM(1)).v(), 3);
    ASSERT_EQ((CPLSM(INT_MAX - 1) + CPLSM(1)).v(), INT_MAX);
    ASSERT_EQ((CPLSM(1) + CPLSM(INT_MAX - 1)).v(), INT_MAX);
    ASSERT_EQ((CPLSM(INT_MAX) + CPLSM(-1)).v(), INT_MAX - 1);
    ASSERT_EQ((CPLSM(-1) + CPLSM(INT_MAX)).v(), INT_MAX - 1);
    ASSERT_EQ((CPLSM(INT_MIN + 1) + CPLSM(-1)).v(), INT_MIN);
    ASSERT_EQ((CPLSM(-1) + CPLSM(INT_MIN + 1)).v(), INT_MIN);
    try
    {
        (CPLSM(INT_MAX) + CPLSM(1)).v();
        ASSERT_TRUE(false);
    }
    catch (...)
    {
    }
    try
    {
        (CPLSM(1) + CPLSM(INT_MAX)).v();
        ASSERT_TRUE(false);
    }
    catch (...)
    {
    }
    try
    {
        (CPLSM(INT_MIN) + CPLSM(-1)).v();
        ASSERT_TRUE(false);
    }
    catch (...)
    {
    }
    try
    {
        (CPLSM(-1) + CPLSM(INT_MIN)).v();
        ASSERT_TRUE(false);
    }
    catch (...)
    {
    }

    ASSERT_EQ((CPLSM(-2) - CPLSM(1)).v(), -3);
    ASSERT_EQ((CPLSM(-2) - CPLSM(-1)).v(), -1);
    ASSERT_EQ((CPLSM(-2) - CPLSM(-3)).v(), 1);
    ASSERT_EQ((CPLSM(2) - CPLSM(-1)).v(), 3);
    ASSERT_EQ((CPLSM(2) - CPLSM(1)).v(), 1);
    ASSERT_EQ((CPLSM(2) - CPLSM(3)).v(), -1);
    ASSERT_EQ((CPLSM(INT_MAX) - CPLSM(1)).v(), INT_MAX - 1);
    ASSERT_EQ((CPLSM(INT_MIN + 1) - CPLSM(1)).v(), INT_MIN);
    ASSERT_EQ((CPLSM(0) - CPLSM(INT_MIN + 1)).v(), INT_MAX);
    ASSERT_EQ((CPLSM(0) - CPLSM(INT_MAX)).v(), -INT_MAX);
    try
    {
        (CPLSM(INT_MIN) - CPLSM(1)).v();
        ASSERT_TRUE(false);
    }
    catch (...)
    {
    }
    try
    {
        (CPLSM(0) - CPLSM(INT_MIN)).v();
        ASSERT_TRUE(false);
    }
    catch (...)
    {
    }
    try
    {
        (CPLSM(INT_MIN) - CPLSM(1)).v();
        ASSERT_TRUE(false);
    }
    catch (...)
    {
    }

    ASSERT_EQ((CPLSM(INT_MIN + 1) * CPLSM(-1)).v(), INT_MAX);
    ASSERT_EQ((CPLSM(-1) * CPLSM(INT_MIN + 1)).v(), INT_MAX);
    ASSERT_EQ((CPLSM(INT_MIN) * CPLSM(1)).v(), INT_MIN);
    ASSERT_EQ((CPLSM(1) * CPLSM(INT_MIN)).v(), INT_MIN);
    ASSERT_EQ((CPLSM(1) * CPLSM(INT_MAX)).v(), INT_MAX);
    ASSERT_EQ((CPLSM(INT_MIN / 2) * CPLSM(2)).v(), INT_MIN);
    ASSERT_EQ((CPLSM(INT_MAX / 2) * CPLSM(2)).v(), INT_MAX - 1);
    ASSERT_EQ((CPLSM(INT_MAX / 2 + 1) * CPLSM(-2)).v(), INT_MIN);
    ASSERT_EQ((CPLSM(0) * CPLSM(INT_MIN)).v(), 0);
    ASSERT_EQ((CPLSM(INT_MIN) * CPLSM(0)).v(), 0);
    ASSERT_EQ((CPLSM(0) * CPLSM(INT_MAX)).v(), 0);
    ASSERT_EQ((CPLSM(INT_MAX) * CPLSM(0)).v(), 0);
    try
    {
        (CPLSM(INT_MAX / 2 + 1) * CPLSM(2)).v();
        ASSERT_TRUE(false);
    }
    catch (...)
    {
    }
    try
    {
        (CPLSM(2) * CPLSM(INT_MAX / 2 + 1)).v();
        ASSERT_TRUE(false);
    }
    catch (...)
    {
    }
    try
    {
        (CPLSM(INT_MIN) * CPLSM(-1)).v();
        ASSERT_TRUE(false);
    }
    catch (...)
    {
    }
    try
    {
        (CPLSM(INT_MIN) * CPLSM(2)).v();
        ASSERT_TRUE(false);
    }
    catch (...)
    {
    }
    try
    {
        (CPLSM(2) * CPLSM(INT_MIN)).v();
        ASSERT_TRUE(false);
    }
    catch (...)
    {
    }

    ASSERT_EQ((CPLSM(4) / CPLSM(2)).v(), 2);
    ASSERT_EQ((CPLSM(4) / CPLSM(-2)).v(), -2);
    ASSERT_EQ((CPLSM(-4) / CPLSM(2)).v(), -2);
    ASSERT_EQ((CPLSM(-4) / CPLSM(-2)).v(), 2);
    ASSERT_EQ((CPLSM(0) / CPLSM(2)).v(), 0);
    ASSERT_EQ((CPLSM(0) / CPLSM(-2)).v(), 0);
    ASSERT_EQ((CPLSM(INT_MAX) / CPLSM(1)).v(), INT_MAX);
    ASSERT_EQ((CPLSM(INT_MAX) / CPLSM(-1)).v(), -INT_MAX);
    ASSERT_EQ((CPLSM(INT_MIN) / CPLSM(1)).v(), INT_MIN);
    try
    {
        (CPLSM(-1) * CPLSM(INT_MIN)).v();
        ASSERT_TRUE(false);
    }
    catch (...)
    {
    }
    try
    {
        (CPLSM(INT_MIN) / CPLSM(-1)).v();
        ASSERT_TRUE(false);
    }
    catch (...)
    {
    }
    try
    {
        (CPLSM(1) / CPLSM(0)).v();
        ASSERT_TRUE(false);
    }
    catch (...)
    {
    }

    ASSERT_EQ(CPLSM_TO_UNSIGNED(1).v(), 1U);
    try
    {
        CPLSM_TO_UNSIGNED(-1);
        ASSERT_TRUE(false);
    }
    catch (...)
    {
    }
}

// Test unsigned int safe maths
TEST_F(test_cpl, CPLSM_unsigned)
{
    ASSERT_EQ((CPLSM(2U) + CPLSM(3U)).v(), 5U);
    ASSERT_EQ((CPLSM(UINT_MAX - 1) + CPLSM(1U)).v(), UINT_MAX);
    try
    {
        (CPLSM(UINT_MAX) + CPLSM(1U)).v();
        ASSERT_TRUE(false);
    }
    catch (...)
    {
    }

    ASSERT_EQ((CPLSM(4U) - CPLSM(3U)).v(), 1U);
    ASSERT_EQ((CPLSM(4U) - CPLSM(4U)).v(), 0U);
    ASSERT_EQ((CPLSM(UINT_MAX) - CPLSM(1U)).v(), UINT_MAX - 1);
    try
    {
        (CPLSM(4U) - CPLSM(5U)).v();
        ASSERT_TRUE(false);
    }
    catch (...)
    {
    }

    ASSERT_EQ((CPLSM(0U) * CPLSM(UINT_MAX)).v(), 0U);
    ASSERT_EQ((CPLSM(UINT_MAX) * CPLSM(0U)).v(), 0U);
    ASSERT_EQ((CPLSM(UINT_MAX) * CPLSM(1U)).v(), UINT_MAX);
    ASSERT_EQ((CPLSM(1U) * CPLSM(UINT_MAX)).v(), UINT_MAX);
    try
    {
        (CPLSM(UINT_MAX) * CPLSM(2U)).v();
        ASSERT_TRUE(false);
    }
    catch (...)
    {
    }
    try
    {
        (CPLSM(2U) * CPLSM(UINT_MAX)).v();
        ASSERT_TRUE(false);
    }
    catch (...)
    {
    }

    ASSERT_EQ((CPLSM(4U) / CPLSM(2U)).v(), 2U);
    ASSERT_EQ((CPLSM(UINT_MAX) / CPLSM(1U)).v(), UINT_MAX);
    try
    {
        (CPLSM(1U) / CPLSM(0U)).v();
        ASSERT_TRUE(false);
    }
    catch (...)
    {
    }

    ASSERT_EQ((CPLSM(static_cast<GUInt64>(2) * 1000 * 1000 * 1000) +
               CPLSM(static_cast<GUInt64>(3) * 1000 * 1000 * 1000))
                  .v(),
              static_cast<GUInt64>(5) * 1000 * 1000 * 1000);
    ASSERT_EQ((CPLSM(std::numeric_limits<GUInt64>::max() - 1) +
               CPLSM(static_cast<GUInt64>(1)))
                  .v(),
              std::numeric_limits<GUInt64>::max());
    try
    {
        (CPLSM(std::numeric_limits<GUInt64>::max()) +
         CPLSM(static_cast<GUInt64>(1)));
    }
    catch (...)
    {
    }

    ASSERT_EQ((CPLSM(static_cast<GUInt64>(2) * 1000 * 1000 * 1000) *
               CPLSM(static_cast<GUInt64>(3) * 1000 * 1000 * 1000))
                  .v(),
              static_cast<GUInt64>(6) * 1000 * 1000 * 1000 * 1000 * 1000 *
                  1000);
    ASSERT_EQ((CPLSM(std::numeric_limits<GUInt64>::max()) *
               CPLSM(static_cast<GUInt64>(1)))
                  .v(),
              std::numeric_limits<GUInt64>::max());
    try
    {
        (CPLSM(std::numeric_limits<GUInt64>::max()) *
         CPLSM(static_cast<GUInt64>(2)));
    }
    catch (...)
    {
    }
}

// Test CPLParseRFC822DateTime()
TEST_F(test_cpl, CPLParseRFC822DateTime)
{
    int year, month, day, hour, min, sec, tz, weekday;
    ASSERT_TRUE(!CPLParseRFC822DateTime("", &year, &month, &day, &hour, &min,
                                        &sec, &tz, &weekday));

    ASSERT_EQ(CPLParseRFC822DateTime("Thu, 15 Jan 2017 12:34:56 +0015", nullptr,
                                     nullptr, nullptr, nullptr, nullptr,
                                     nullptr, nullptr, nullptr),
              TRUE);

    ASSERT_EQ(CPLParseRFC822DateTime("Thu, 15 Jan 2017 12:34:56 +0015", &year,
                                     &month, &day, &hour, &min, &sec, &tz,
                                     &weekday),
              TRUE);
    ASSERT_EQ(year, 2017);
    ASSERT_EQ(month, 1);
    ASSERT_EQ(day, 15);
    ASSERT_EQ(hour, 12);
    ASSERT_EQ(min, 34);
    ASSERT_EQ(sec, 56);
    ASSERT_EQ(tz, 101);
    ASSERT_EQ(weekday, 4);

    ASSERT_EQ(CPLParseRFC822DateTime("Thu, 15 Jan 2017 12:34:56 GMT", &year,
                                     &month, &day, &hour, &min, &sec, &tz,
                                     &weekday),
              TRUE);
    ASSERT_EQ(year, 2017);
    ASSERT_EQ(month, 1);
    ASSERT_EQ(day, 15);
    ASSERT_EQ(hour, 12);
    ASSERT_EQ(min, 34);
    ASSERT_EQ(sec, 56);
    ASSERT_EQ(tz, 100);
    ASSERT_EQ(weekday, 4);

    // Without day of week, second and timezone
    ASSERT_EQ(CPLParseRFC822DateTime("15 Jan 2017 12:34", &year, &month, &day,
                                     &hour, &min, &sec, &tz, &weekday),
              TRUE);
    ASSERT_EQ(year, 2017);
    ASSERT_EQ(month, 1);
    ASSERT_EQ(day, 15);
    ASSERT_EQ(hour, 12);
    ASSERT_EQ(min, 34);
    ASSERT_EQ(sec, -1);
    ASSERT_EQ(tz, 0);
    ASSERT_EQ(weekday, 0);

    ASSERT_EQ(CPLParseRFC822DateTime("XXX, 15 Jan 2017 12:34:56 GMT", &year,
                                     &month, &day, &hour, &min, &sec, &tz,
                                     &weekday),
              TRUE);
    ASSERT_EQ(weekday, 0);

    ASSERT_TRUE(!CPLParseRFC822DateTime("Sun, 01 Jan 2017 12", &year, &month,
                                        &day, &hour, &min, &sec, &tz,
                                        &weekday));

    ASSERT_TRUE(!CPLParseRFC822DateTime("00 Jan 2017 12:34:56 GMT", &year,
                                        &month, &day, &hour, &min, &sec, &tz,
                                        &weekday));

    ASSERT_TRUE(!CPLParseRFC822DateTime("32 Jan 2017 12:34:56 GMT", &year,
                                        &month, &day, &hour, &min, &sec, &tz,
                                        &weekday));

    ASSERT_TRUE(!CPLParseRFC822DateTime("01 XXX 2017 12:34:56 GMT", &year,
                                        &month, &day, &hour, &min, &sec, &tz,
                                        &weekday));

    ASSERT_TRUE(!CPLParseRFC822DateTime("01 Jan 2017 -1:34:56 GMT", &year,
                                        &month, &day, &hour, &min, &sec, &tz,
                                        &weekday));

    ASSERT_TRUE(!CPLParseRFC822DateTime("01 Jan 2017 24:34:56 GMT", &year,
                                        &month, &day, &hour, &min, &sec, &tz,
                                        &weekday));

    ASSERT_TRUE(!CPLParseRFC822DateTime("01 Jan 2017 12:-1:56 GMT", &year,
                                        &month, &day, &hour, &min, &sec, &tz,
                                        &weekday));

    ASSERT_TRUE(!CPLParseRFC822DateTime("01 Jan 2017 12:60:56 GMT", &year,
                                        &month, &day, &hour, &min, &sec, &tz,
                                        &weekday));

    ASSERT_TRUE(!CPLParseRFC822DateTime("01 Jan 2017 12:34:-1 GMT", &year,
                                        &month, &day, &hour, &min, &sec, &tz,
                                        &weekday));

    ASSERT_TRUE(!CPLParseRFC822DateTime("01 Jan 2017 12:34:61 GMT", &year,
                                        &month, &day, &hour, &min, &sec, &tz,
                                        &weekday));

    ASSERT_TRUE(!CPLParseRFC822DateTime("15 Jan 2017 12:34:56 XXX", &year,
                                        &month, &day, &hour, &min, &sec, &tz,
                                        &weekday));

    ASSERT_TRUE(!CPLParseRFC822DateTime("15 Jan 2017 12:34:56 +-100", &year,
                                        &month, &day, &hour, &min, &sec, &tz,
                                        &weekday));

    ASSERT_TRUE(!CPLParseRFC822DateTime("15 Jan 2017 12:34:56 +9900", &year,
                                        &month, &day, &hour, &min, &sec, &tz,
                                        &weekday));
}

// Test CPLCopyTree()
TEST_F(test_cpl, CPLCopyTree)
{
    CPLString osTmpPath(CPLGetDirname(CPLGenerateTempFilename(nullptr)));
    CPLString osSrcDir(CPLFormFilename(osTmpPath, "src_dir", nullptr));
    CPLString osNewDir(CPLFormFilename(osTmpPath, "new_dir", nullptr));
    ASSERT_TRUE(VSIMkdir(osSrcDir, 0755) == 0);
    CPLString osSrcFile(CPLFormFilename(osSrcDir, "my.bin", nullptr));
    VSILFILE *fp = VSIFOpenL(osSrcFile, "wb");
    ASSERT_TRUE(fp != nullptr);
    VSIFCloseL(fp);

    CPLPushErrorHandler(CPLQuietErrorHandler);
    ASSERT_TRUE(CPLCopyTree(osNewDir, "/i/do_not/exist") < 0);
    CPLPopErrorHandler();

    ASSERT_TRUE(CPLCopyTree(osNewDir, osSrcDir) == 0);
    VSIStatBufL sStat;
    CPLString osNewFile(CPLFormFilename(osNewDir, "my.bin", nullptr));
    ASSERT_TRUE(VSIStatL(osNewFile, &sStat) == 0);

    CPLPushErrorHandler(CPLQuietErrorHandler);
    ASSERT_TRUE(CPLCopyTree(osNewDir, osSrcDir) < 0);
    CPLPopErrorHandler();

    VSIUnlink(osNewFile);
    VSIRmdir(osNewDir);
    VSIUnlink(osSrcFile);
    VSIRmdir(osSrcDir);
}

class CPLJSonStreamingParserDump : public CPLJSonStreamingParser
{
    std::vector<bool> m_abFirstMember;
    CPLString m_osSerialized;
    CPLString m_osException;

  public:
    CPLJSonStreamingParserDump()
    {
    }

    virtual void Reset() CPL_OVERRIDE
    {
        m_osSerialized.clear();
        m_osException.clear();
        CPLJSonStreamingParser::Reset();
    }

    virtual void String(const char *pszValue, size_t) CPL_OVERRIDE;
    virtual void Number(const char *pszValue, size_t) CPL_OVERRIDE;
    virtual void Boolean(bool bVal) CPL_OVERRIDE;
    virtual void Null() CPL_OVERRIDE;

    virtual void StartObject() CPL_OVERRIDE;
    virtual void EndObject() CPL_OVERRIDE;
    virtual void StartObjectMember(const char *pszKey, size_t) CPL_OVERRIDE;

    virtual void StartArray() CPL_OVERRIDE;
    virtual void EndArray() CPL_OVERRIDE;
    virtual void StartArrayMember() CPL_OVERRIDE;

    virtual void Exception(const char *pszMessage) CPL_OVERRIDE;

    const CPLString &GetSerialized() const
    {
        return m_osSerialized;
    }
    const CPLString &GetException() const
    {
        return m_osException;
    }
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

void CPLJSonStreamingParserDump::StartObjectMember(const char *pszKey, size_t)
{
    if (!m_abFirstMember.back())
        m_osSerialized += ", ";
    m_osSerialized += CPLSPrintf("\"%s\": ", pszKey);
    m_abFirstMember.back() = false;
}

void CPLJSonStreamingParserDump::String(const char *pszValue, size_t)
{
    m_osSerialized += GetSerializedString(pszValue);
}

void CPLJSonStreamingParserDump::Number(const char *pszValue, size_t)
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
    if (!m_abFirstMember.back())
        m_osSerialized += ", ";
    m_abFirstMember.back() = false;
}

void CPLJSonStreamingParserDump::Exception(const char *pszMessage)
{
    m_osException = pszMessage;
}

// Test CPLJSonStreamingParser()
TEST_F(test_cpl, CPLJSonStreamingParser)
{
    // nominal cases
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "true";
        ASSERT_TRUE(oParser.Parse(sText, strlen(sText), true));
        ASSERT_EQ(oParser.GetSerialized(), sText);

        oParser.Reset();
        for (size_t i = 0; sText[i]; i++)
            ASSERT_TRUE(oParser.Parse(sText + i, 1, sText[i + 1] == 0));
        ASSERT_EQ(oParser.GetSerialized(), sText);
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "false";
        ASSERT_TRUE(oParser.Parse(sText, strlen(sText), true));
        ASSERT_EQ(oParser.GetSerialized(), sText);

        oParser.Reset();
        for (size_t i = 0; sText[i]; i++)
            ASSERT_TRUE(oParser.Parse(sText + i, 1, sText[i + 1] == 0));
        ASSERT_EQ(oParser.GetSerialized(), sText);
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "null";
        ASSERT_TRUE(oParser.Parse(sText, strlen(sText), true));
        ASSERT_EQ(oParser.GetSerialized(), sText);

        oParser.Reset();
        for (size_t i = 0; sText[i]; i++)
            ASSERT_TRUE(oParser.Parse(sText + i, 1, sText[i + 1] == 0));
        ASSERT_EQ(oParser.GetSerialized(), sText);
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "10";
        ASSERT_TRUE(oParser.Parse(sText, strlen(sText), true));
        ASSERT_EQ(oParser.GetSerialized(), sText);

        oParser.Reset();
        for (size_t i = 0; sText[i]; i++)
            ASSERT_TRUE(oParser.Parse(sText + i, 1, sText[i + 1] == 0));
        ASSERT_EQ(oParser.GetSerialized(), sText);
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "123eE-34";
        ASSERT_TRUE(oParser.Parse(sText, strlen(sText), true));
        ASSERT_EQ(oParser.GetSerialized(), sText);

        oParser.Reset();
        for (size_t i = 0; sText[i]; i++)
            ASSERT_TRUE(oParser.Parse(sText + i, 1, sText[i + 1] == 0));
        ASSERT_EQ(oParser.GetSerialized(), sText);
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "\"\"";
        ASSERT_TRUE(oParser.Parse(sText, strlen(sText), true));
        ASSERT_EQ(oParser.GetSerialized(), sText);

        oParser.Reset();
        for (size_t i = 0; sText[i]; i++)
            ASSERT_TRUE(oParser.Parse(sText + i, 1, sText[i + 1] == 0));
        ASSERT_EQ(oParser.GetSerialized(), sText);
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "\"\\\\a\\b\\f\\n\\r\\t\\u0020\\u0001\\\"\"";
        ASSERT_TRUE(oParser.Parse(sText, strlen(sText), true));
        ASSERT_EQ(oParser.GetSerialized(),
                  "\"\\\\a\\b\\f\\n\\r\\t \\u0001\\\"\"");

        oParser.Reset();
        for (size_t i = 0; sText[i]; i++)
            ASSERT_TRUE(oParser.Parse(sText + i, 1, sText[i + 1] == 0));
        ASSERT_EQ(oParser.GetSerialized(),
                  "\"\\\\a\\b\\f\\n\\r\\t \\u0001\\\"\"");
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] =
            "\"\\u0001\\u0020\\ud834\\uDD1E\\uDD1E\\uD834\\uD834\\uD834\"";
        ASSERT_TRUE(oParser.Parse(sText, strlen(sText), true));
        ASSERT_EQ(
            oParser.GetSerialized(),
            "\"\\u0001 \xf0\x9d\x84\x9e\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd\"");
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "\"\\ud834\"";
        ASSERT_TRUE(oParser.Parse(sText, strlen(sText), true));
        ASSERT_EQ(oParser.GetSerialized(), "\"\xef\xbf\xbd\"");
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "\"\\ud834\\t\"";
        ASSERT_TRUE(oParser.Parse(sText, strlen(sText), true));
        ASSERT_EQ(oParser.GetSerialized(), "\"\xef\xbf\xbd\\t\"");
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "\"\\u00e9\"";
        ASSERT_TRUE(oParser.Parse(sText, strlen(sText), true));
        ASSERT_EQ(oParser.GetSerialized(), "\"\xc3\xa9\"");
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "{}";
        ASSERT_TRUE(oParser.Parse(sText, strlen(sText), true));
        ASSERT_EQ(oParser.GetSerialized(), sText);

        oParser.Reset();
        for (size_t i = 0; sText[i]; i++)
            ASSERT_TRUE(oParser.Parse(sText + i, 1, sText[i + 1] == 0));
        ASSERT_EQ(oParser.GetSerialized(), sText);
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "[]";
        ASSERT_TRUE(oParser.Parse(sText, strlen(sText), true));
        ASSERT_EQ(oParser.GetSerialized(), sText);

        oParser.Reset();
        for (size_t i = 0; sText[i]; i++)
            ASSERT_TRUE(oParser.Parse(sText + i, 1, sText[i + 1] == 0));
        ASSERT_EQ(oParser.GetSerialized(), sText);
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "[[]]";
        ASSERT_TRUE(oParser.Parse(sText, strlen(sText), true));
        ASSERT_EQ(oParser.GetSerialized(), sText);

        oParser.Reset();
        for (size_t i = 0; sText[i]; i++)
            ASSERT_TRUE(oParser.Parse(sText + i, 1, sText[i + 1] == 0));
        ASSERT_EQ(oParser.GetSerialized(), sText);
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "[1]";
        ASSERT_TRUE(oParser.Parse(sText, strlen(sText), true));
        ASSERT_EQ(oParser.GetSerialized(), sText);

        oParser.Reset();
        for (size_t i = 0; sText[i]; i++)
            ASSERT_TRUE(oParser.Parse(sText + i, 1, sText[i + 1] == 0));
        ASSERT_EQ(oParser.GetSerialized(), sText);
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "[1,2]";
        ASSERT_TRUE(oParser.Parse(sText, strlen(sText), true));
        ASSERT_EQ(oParser.GetSerialized(), "[1, 2]");

        oParser.Reset();
        for (size_t i = 0; sText[i]; i++)
            ASSERT_TRUE(oParser.Parse(sText + i, 1, sText[i + 1] == 0));
        ASSERT_EQ(oParser.GetSerialized(), "[1, 2]");
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "{\"a\":null}";
        ASSERT_TRUE(oParser.Parse(sText, strlen(sText), true));
        ASSERT_EQ(oParser.GetSerialized(), "{\"a\": null}");

        oParser.Reset();
        for (size_t i = 0; sText[i]; i++)
            ASSERT_TRUE(oParser.Parse(sText + i, 1, sText[i + 1] == 0));
        ASSERT_EQ(oParser.GetSerialized(), "{\"a\": null}");
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] =
            " { \"a\" : null ,\r\n\t\"b\": {\"c\": 1}, \"d\": [1] }";
        ASSERT_TRUE(oParser.Parse(sText, strlen(sText), true));
        const char sExpected[] = "{\"a\": null, \"b\": {\"c\": 1}, \"d\": [1]}";
        ASSERT_EQ(oParser.GetSerialized(), sExpected);

        oParser.Reset();
        ASSERT_TRUE(oParser.Parse(sText, strlen(sText), true));
        ASSERT_EQ(oParser.GetSerialized(), sExpected);

        oParser.Reset();
        for (size_t i = 0; sText[i]; i++)
            ASSERT_TRUE(oParser.Parse(sText + i, 1, sText[i + 1] == 0));
        ASSERT_EQ(oParser.GetSerialized(), sExpected);
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "infinity";
        ASSERT_TRUE(oParser.Parse(sText, strlen(sText), true));
        ASSERT_EQ(oParser.GetSerialized(), sText);

        oParser.Reset();
        for (size_t i = 0; sText[i]; i++)
            ASSERT_TRUE(oParser.Parse(sText + i, 1, sText[i + 1] == 0));
        ASSERT_EQ(oParser.GetSerialized(), sText);
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "-infinity";
        ASSERT_TRUE(oParser.Parse(sText, strlen(sText), true));
        ASSERT_EQ(oParser.GetSerialized(), sText);

        oParser.Reset();
        for (size_t i = 0; sText[i]; i++)
            ASSERT_TRUE(oParser.Parse(sText + i, 1, sText[i + 1] == 0));
        ASSERT_EQ(oParser.GetSerialized(), sText);
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "nan";
        ASSERT_TRUE(oParser.Parse(sText, strlen(sText), true));
        ASSERT_EQ(oParser.GetSerialized(), sText);

        oParser.Reset();
        for (size_t i = 0; sText[i]; i++)
            ASSERT_TRUE(oParser.Parse(sText + i, 1, sText[i + 1] == 0));
        ASSERT_EQ(oParser.GetSerialized(), sText);
    }

    // errors
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "tru";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "tru1";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "truxe";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "truex";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "fals";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "falsxe";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "falsex";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "nul";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "nulxl";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "nullx";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "na";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "nanx";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "infinit";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "infinityx";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "-infinit";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "-infinityx";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "true false";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "x";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "{";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "}";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "[";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "[1";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "[,";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "[|";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "]";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "{ :";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "{ ,";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "{ |";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "{ 1";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "{ \"x\"";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "{ \"x\": ";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "{ \"x\": 1 2";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "{ \"x\", ";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "{ \"x\" }";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "{\"a\" x}";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "1x";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "\"";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "\"\\";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "\"\\x\"";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "\"\\u";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "\"\\ux";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "\"\\u000";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "\"\\uD834\\ux\"";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "\"\\\"";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "\"too long\"";
        oParser.SetMaxStringSize(2);
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "[[]]";
        oParser.SetMaxDepth(1);
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "{ \"x\": {} }";
        oParser.SetMaxDepth(1);
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "[,]";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "[true,]";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "[true,,true]";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
    {
        CPLJSonStreamingParserDump oParser;
        const char sText[] = "[true true]";
        ASSERT_TRUE(!oParser.Parse(sText, strlen(sText), true));
        ASSERT_TRUE(!oParser.GetException().empty());
    }
}

// Test cpl_mem_cache
TEST_F(test_cpl, cpl_mem_cache)
{
    lru11::Cache<int, int> cache(2, 1);
    ASSERT_EQ(cache.size(), 0U);
    ASSERT_TRUE(cache.empty());
    cache.clear();
    int val;
    ASSERT_TRUE(!cache.tryGet(0, val));
    try
    {
        cache.get(0);
        ASSERT_TRUE(false);
    }
    catch (const lru11::KeyNotFound &)
    {
        ASSERT_TRUE(true);
    }
    ASSERT_TRUE(!cache.remove(0));
    ASSERT_TRUE(!cache.contains(0));
    ASSERT_EQ(cache.getMaxSize(), 2U);
    ASSERT_EQ(cache.getElasticity(), 1U);
    ASSERT_EQ(cache.getMaxAllowedSize(), 3U);
    int out;
    ASSERT_TRUE(!cache.removeAndRecycleOldestEntry(out));

    cache.insert(0, 1);
    val = 0;
    ASSERT_TRUE(cache.tryGet(0, val));
    int *ptr = cache.getPtr(0);
    ASSERT_TRUE(ptr);
    ASSERT_EQ(*ptr, 1);
    ASSERT_TRUE(cache.getPtr(-1) == nullptr);
    ASSERT_EQ(val, 1);
    ASSERT_EQ(cache.get(0), 1);
    ASSERT_EQ(cache.getCopy(0), 1);
    ASSERT_EQ(cache.size(), 1U);
    ASSERT_TRUE(!cache.empty());
    ASSERT_TRUE(cache.contains(0));
    bool visited = false;
    auto lambda = [&visited](const lru11::KeyValuePair<int, int> &kv)
    {
        if (kv.key == 0 && kv.value == 1)
            visited = true;
    };
    cache.cwalk(lambda);
    ASSERT_TRUE(visited);

    out = -1;
    ASSERT_TRUE(cache.removeAndRecycleOldestEntry(out));
    ASSERT_EQ(out, 1);

    cache.insert(0, 1);
    cache.insert(0, 2);
    ASSERT_EQ(cache.get(0), 2);
    ASSERT_EQ(cache.size(), 1U);
    cache.insert(1, 3);
    cache.insert(2, 4);
    ASSERT_EQ(cache.size(), 3U);
    cache.insert(3, 5);
    ASSERT_EQ(cache.size(), 2U);
    ASSERT_TRUE(cache.contains(2));
    ASSERT_TRUE(cache.contains(3));
    ASSERT_TRUE(!cache.contains(0));
    ASSERT_TRUE(!cache.contains(1));
    ASSERT_TRUE(cache.remove(2));
    ASSERT_TRUE(!cache.contains(2));
    ASSERT_EQ(cache.size(), 1U);

    {
        // Check that MyObj copy constructor and copy-assignment operator
        // are not needed
        struct MyObj
        {
            int m_v;
            MyObj(int v) : m_v(v)
            {
            }
            MyObj(const MyObj &) = delete;
            MyObj &operator=(const MyObj &) = delete;
            MyObj(MyObj &&) = default;
            MyObj &operator=(MyObj &&) = default;
        };
        lru11::Cache<int, MyObj> cacheMyObj(2, 0);
        ASSERT_EQ(cacheMyObj.insert(0, MyObj(0)).m_v, 0);
        cacheMyObj.getPtr(0);
        ASSERT_EQ(cacheMyObj.insert(1, MyObj(1)).m_v, 1);
        ASSERT_EQ(cacheMyObj.insert(2, MyObj(2)).m_v, 2);
        MyObj outObj(-1);
        cacheMyObj.removeAndRecycleOldestEntry(outObj);
    }

    {
        // Check that MyObj copy constructor and copy-assignment operator
        // are not triggered
        struct MyObj
        {
            int m_v;
            MyObj(int v) : m_v(v)
            {
            }
            static void should_not_happen()
            {
                ASSERT_TRUE(false);
            }
            MyObj(const MyObj &) : m_v(-1)
            {
                should_not_happen();
            }
            MyObj &operator=(const MyObj &)
            {
                should_not_happen();
                return *this;
            }
            MyObj(MyObj &&) = default;
            MyObj &operator=(MyObj &&) = default;
        };
        lru11::Cache<int, MyObj> cacheMyObj(2, 0);
        ASSERT_EQ(cacheMyObj.insert(0, MyObj(0)).m_v, 0);
        cacheMyObj.getPtr(0);
        ASSERT_EQ(cacheMyObj.insert(1, MyObj(1)).m_v, 1);
        ASSERT_EQ(cacheMyObj.insert(2, MyObj(2)).m_v, 2);
        MyObj outObj(-1);
        cacheMyObj.removeAndRecycleOldestEntry(outObj);
    }
}

// Test CPLJSONDocument
TEST_F(test_cpl, CPLJSONDocument)
{
    {
        // Test Json document LoadUrl
        CPLJSONDocument oDocument;
        const char *options[5] = {"CONNECTTIMEOUT=15", "TIMEOUT=20",
                                  "MAX_RETRY=5", "RETRY_DELAY=1", nullptr};

        oDocument.GetRoot().Add("foo", "bar");

        if (CPLHTTPEnabled())
        {
            CPLSetConfigOption("CPL_CURL_ENABLE_VSIMEM", "YES");
            VSILFILE *fpTmp = VSIFOpenL("/vsimem/test.json", "wb");
            const char *pszContent = "{ \"foo\": \"bar\" }";
            VSIFWriteL(pszContent, 1, strlen(pszContent), fpTmp);
            VSIFCloseL(fpTmp);
            ASSERT_TRUE(oDocument.LoadUrl("/vsimem/test.json",
                                          const_cast<char **>(options)));
            CPLSetConfigOption("CPL_CURL_ENABLE_VSIMEM", nullptr);
            VSIUnlink("/vsimem/test.json");

            CPLJSONObject oJsonRoot = oDocument.GetRoot();
            ASSERT_TRUE(oJsonRoot.IsValid());

            CPLString value = oJsonRoot.GetString("foo", "");
            ASSERT_STRNE(value, "bar");  // not equal
        }
    }
    {
        // Test Json document LoadChunks
        CPLJSONDocument oDocument;

        CPLPushErrorHandler(CPLQuietErrorHandler);
        ASSERT_TRUE(!oDocument.LoadChunks("/i_do/not/exist", 512));
        CPLPopErrorHandler();

        CPLPushErrorHandler(CPLQuietErrorHandler);
        ASSERT_TRUE(!oDocument.LoadChunks("test_cpl.cpp", 512));
        CPLPopErrorHandler();

        oDocument.GetRoot().Add("foo", "bar");

        ASSERT_TRUE(
            oDocument.LoadChunks((data_ + SEP + "test.json").c_str(), 512));

        CPLJSONObject oJsonRoot = oDocument.GetRoot();
        ASSERT_TRUE(oJsonRoot.IsValid());
        ASSERT_EQ(oJsonRoot.GetInteger("resource/id", 10), 0);

        CPLJSONObject oJsonResource = oJsonRoot.GetObj("resource");
        ASSERT_TRUE(oJsonResource.IsValid());
        std::vector<CPLJSONObject> children = oJsonResource.GetChildren();
        ASSERT_TRUE(children.size() == 11);

        CPLJSONArray oaScopes = oJsonRoot.GetArray("resource/scopes");
        ASSERT_TRUE(oaScopes.IsValid());
        ASSERT_EQ(oaScopes.Size(), 2);

        CPLJSONObject oHasChildren = oJsonRoot.GetObj("resource/children");
        ASSERT_TRUE(oHasChildren.IsValid());
        ASSERT_EQ(oHasChildren.ToBool(), true);

        ASSERT_EQ(oJsonResource.GetBool("children", false), true);

        CPLJSONObject oJsonId = oJsonRoot["resource/owner_user/id"];
        ASSERT_TRUE(oJsonId.IsValid());
    }
    {
        CPLJSONDocument oDocument;
        ASSERT_TRUE(!oDocument.LoadMemory(nullptr, 0));
        ASSERT_TRUE(!oDocument.LoadMemory(CPLString()));
        ASSERT_TRUE(oDocument.LoadMemory(std::string("true")));
        ASSERT_TRUE(oDocument.GetRoot().GetType() ==
                    CPLJSONObject::Type::Boolean);
        ASSERT_TRUE(oDocument.GetRoot().ToBool());
        ASSERT_TRUE(oDocument.LoadMemory(std::string("false")));
        ASSERT_TRUE(oDocument.GetRoot().GetType() ==
                    CPLJSONObject::Type::Boolean);
        ASSERT_TRUE(!oDocument.GetRoot().ToBool());
    }
    {
        // Copy constructor
        CPLJSONDocument oDocument;
        ASSERT_TRUE(oDocument.LoadMemory(std::string("true")));
        oDocument.GetRoot();
        CPLJSONDocument oDocument2(oDocument);
        CPLJSONObject oObj(oDocument.GetRoot());
        ASSERT_TRUE(oObj.ToBool());
        CPLJSONObject oObj2(oObj);
        ASSERT_TRUE(oObj2.ToBool());
        // Assignment operator
        oDocument2 = oDocument;
        auto &oDocument2Ref(oDocument2);
        oDocument2 = oDocument2Ref;
        oObj2 = oObj;
        auto &oObj2Ref(oObj2);
        oObj2 = oObj2Ref;
        CPLJSONObject oObj3(std::move(oObj2));
        ASSERT_TRUE(oObj3.ToBool());
        CPLJSONObject oObj4;
        oObj4 = std::move(oObj3);
        ASSERT_TRUE(oObj4.ToBool());
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
        ASSERT_TRUE(!oDocument.Save("/i_do/not/exist"));
        CPLPopErrorHandler();
    }
    {
        CPLJSONObject oObj;
        oObj.Add("string", std::string("my_string"));
        ASSERT_EQ(oObj.GetString("string"), std::string("my_string"));
        ASSERT_EQ(oObj.GetString("inexisting_string", "default"),
                  std::string("default"));
        oObj.Add("const_char_star", nullptr);
        oObj.Add("const_char_star", "my_const_char_star");
        ASSERT_TRUE(oObj.GetObj("const_char_star").GetType() ==
                    CPLJSONObject::Type::String);
        oObj.Add("int", 1);
        ASSERT_EQ(oObj.GetInteger("int"), 1);
        ASSERT_EQ(oObj.GetInteger("inexisting_int", -987), -987);
        ASSERT_TRUE(oObj.GetObj("int").GetType() ==
                    CPLJSONObject::Type::Integer);
        oObj.Add("int64", GINT64_MAX);
        ASSERT_EQ(oObj.GetLong("int64"), GINT64_MAX);
        ASSERT_EQ(oObj.GetLong("inexisting_int64", GINT64_MIN), GINT64_MIN);
        ASSERT_TRUE(oObj.GetObj("int64").GetType() ==
                    CPLJSONObject::Type::Long);
        oObj.Add("double", 1.25);
        ASSERT_EQ(oObj.GetDouble("double"), 1.25);
        ASSERT_EQ(oObj.GetDouble("inexisting_double", -987.0), -987.0);
        ASSERT_TRUE(oObj.GetObj("double").GetType() ==
                    CPLJSONObject::Type::Double);
        oObj.Add("array", CPLJSONArray());
        ASSERT_TRUE(oObj.GetObj("array").GetType() ==
                    CPLJSONObject::Type::Array);
        oObj.Add("obj", CPLJSONObject());
        ASSERT_TRUE(oObj.GetObj("obj").GetType() ==
                    CPLJSONObject::Type::Object);
        oObj.Add("bool", true);
        ASSERT_EQ(oObj.GetBool("bool"), true);
        ASSERT_EQ(oObj.GetBool("inexisting_bool", false), false);
        ASSERT_TRUE(oObj.GetObj("bool").GetType() ==
                    CPLJSONObject::Type::Boolean);
        oObj.AddNull("null_field");
        ASSERT_TRUE(oObj.GetObj("null_field").GetType() ==
                    CPLJSONObject::Type::Null);
        ASSERT_TRUE(oObj.GetObj("inexisting").GetType() ==
                    CPLJSONObject::Type::Unknown);
        oObj.Set("string", std::string("my_string"));
        oObj.Set("const_char_star", nullptr);
        oObj.Set("const_char_star", "my_const_char_star");
        oObj.Set("int", 1);
        oObj.Set("int64", GINT64_MAX);
        oObj.Set("double", 1.25);
        // oObj.Set("array", CPLJSONArray());
        // oObj.Set("obj", CPLJSONObject());
        oObj.Set("bool", true);
        oObj.SetNull("null_field");
        ASSERT_TRUE(CPLJSONArray().GetChildren().empty());
        oObj.ToArray();
        ASSERT_EQ(CPLJSONObject().Format(CPLJSONObject::PrettyFormat::Spaced),
                  std::string("{ }"));
        ASSERT_EQ(CPLJSONObject().Format(CPLJSONObject::PrettyFormat::Pretty),
                  std::string("{\n}"));
        ASSERT_EQ(CPLJSONObject().Format(CPLJSONObject::PrettyFormat::Plain),
                  std::string("{}"));
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
        ASSERT_EQ(oArray.Size(), 7);

        int nCount = 0;
        for (const auto &obj : oArray)
        {
            ASSERT_EQ(obj.GetInternalHandle(),
                      oArray[nCount].GetInternalHandle());
            nCount++;
        }
        ASSERT_EQ(nCount, 7);
    }
    {
        CPLJSONDocument oDocument;
        ASSERT_TRUE(oDocument.LoadMemory(CPLString("{ \"/foo\" : \"bar\" }")));
        ASSERT_EQ(oDocument.GetRoot().GetString("/foo"), std::string("bar"));
    }
}

// Test CPLRecodeIconv() with re-allocation
TEST_F(test_cpl, CPLRecodeIconv)
{
#ifdef CPL_RECODE_ICONV
    int N = 32800;
    char *pszIn = static_cast<char *>(CPLMalloc(N + 1));
    for (int i = 0; i < N; i++)
        pszIn[i] = '\xE9';
    pszIn[N] = 0;
    char *pszExpected = static_cast<char *>(CPLMalloc(N * 2 + 1));
    for (int i = 0; i < N; i++)
    {
        pszExpected[2 * i] = '\xC3';
        pszExpected[2 * i + 1] = '\xA9';
    }
    pszExpected[N * 2] = 0;
    char *pszRet = CPLRecode(pszIn, "ISO-8859-2", CPL_ENC_UTF8);
    EXPECT_EQ(memcmp(pszExpected, pszRet, N * 2 + 1), 0);
    CPLFree(pszIn);
    CPLFree(pszRet);
    CPLFree(pszExpected);
#else
    GTEST_SKIP() << "CPL_RECODE_ICONV missing";
#endif
}

// Test CPLHTTPParseMultipartMime()
TEST_F(test_cpl, CPLHTTPParseMultipartMime)
{
    CPLHTTPResult *psResult;

    psResult =
        static_cast<CPLHTTPResult *>(CPLCalloc(1, sizeof(CPLHTTPResult)));
    CPLPushErrorHandler(CPLQuietErrorHandler);
    EXPECT_TRUE(!CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)));
    CPLPopErrorHandler();
    CPLHTTPDestroyResult(psResult);

    // Missing boundary value
    psResult =
        static_cast<CPLHTTPResult *>(CPLCalloc(1, sizeof(CPLHTTPResult)));
    psResult->pszContentType = CPLStrdup("multipart/form-data; boundary=");
    CPLPushErrorHandler(CPLQuietErrorHandler);
    EXPECT_TRUE(!CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)));
    CPLPopErrorHandler();
    CPLHTTPDestroyResult(psResult);

    // No content
    psResult =
        static_cast<CPLHTTPResult *>(CPLCalloc(1, sizeof(CPLHTTPResult)));
    psResult->pszContentType =
        CPLStrdup("multipart/form-data; boundary=myboundary");
    CPLPushErrorHandler(CPLQuietErrorHandler);
    EXPECT_TRUE(!CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)));
    CPLPopErrorHandler();
    CPLHTTPDestroyResult(psResult);

    // No part
    psResult =
        static_cast<CPLHTTPResult *>(CPLCalloc(1, sizeof(CPLHTTPResult)));
    psResult->pszContentType =
        CPLStrdup("multipart/form-data; boundary=myboundary");
    {
        const char *pszText = "--myboundary  some junk\r\n";
        psResult->pabyData = reinterpret_cast<GByte *>(CPLStrdup(pszText));
        psResult->nDataLen = static_cast<int>(strlen(pszText));
    }
    CPLPushErrorHandler(CPLQuietErrorHandler);
    EXPECT_TRUE(!CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)));
    CPLPopErrorHandler();
    CPLHTTPDestroyResult(psResult);

    // Missing end boundary
    psResult =
        static_cast<CPLHTTPResult *>(CPLCalloc(1, sizeof(CPLHTTPResult)));
    psResult->pszContentType =
        CPLStrdup("multipart/form-data; boundary=myboundary");
    {
        const char *pszText = "--myboundary  some junk\r\n"
                              "\r\n"
                              "Bla";
        psResult->pabyData = reinterpret_cast<GByte *>(CPLStrdup(pszText));
        psResult->nDataLen = static_cast<int>(strlen(pszText));
    }
    CPLPushErrorHandler(CPLQuietErrorHandler);
    EXPECT_TRUE(!CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)));
    CPLPopErrorHandler();
    CPLHTTPDestroyResult(psResult);

    // Truncated header
    psResult =
        static_cast<CPLHTTPResult *>(CPLCalloc(1, sizeof(CPLHTTPResult)));
    psResult->pszContentType =
        CPLStrdup("multipart/form-data; boundary=myboundary");
    {
        const char *pszText = "--myboundary  some junk\r\n"
                              "Content-Type: foo";
        psResult->pabyData = reinterpret_cast<GByte *>(CPLStrdup(pszText));
        psResult->nDataLen = static_cast<int>(strlen(pszText));
    }
    CPLPushErrorHandler(CPLQuietErrorHandler);
    EXPECT_TRUE(!CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)));
    CPLPopErrorHandler();
    CPLHTTPDestroyResult(psResult);

    // Invalid end boundary
    psResult =
        static_cast<CPLHTTPResult *>(CPLCalloc(1, sizeof(CPLHTTPResult)));
    psResult->pszContentType =
        CPLStrdup("multipart/form-data; boundary=myboundary");
    {
        const char *pszText = "--myboundary  some junk\r\n"
                              "\r\n"
                              "Bla"
                              "\r\n"
                              "--myboundary";
        psResult->pabyData = reinterpret_cast<GByte *>(CPLStrdup(pszText));
        psResult->nDataLen = static_cast<int>(strlen(pszText));
    }
    CPLPushErrorHandler(CPLQuietErrorHandler);
    EXPECT_TRUE(!CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)));
    CPLPopErrorHandler();
    CPLHTTPDestroyResult(psResult);

    // Invalid end boundary
    psResult =
        static_cast<CPLHTTPResult *>(CPLCalloc(1, sizeof(CPLHTTPResult)));
    psResult->pszContentType =
        CPLStrdup("multipart/form-data; boundary=myboundary");
    {
        const char *pszText = "--myboundary  some junk\r\n"
                              "\r\n"
                              "Bla"
                              "\r\n"
                              "--myboundary";
        psResult->pabyData = reinterpret_cast<GByte *>(CPLStrdup(pszText));
        psResult->nDataLen = static_cast<int>(strlen(pszText));
    }
    CPLPushErrorHandler(CPLQuietErrorHandler);
    EXPECT_TRUE(!CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)));
    CPLPopErrorHandler();
    CPLHTTPDestroyResult(psResult);

    // Valid single part, no header
    psResult =
        static_cast<CPLHTTPResult *>(CPLCalloc(1, sizeof(CPLHTTPResult)));
    psResult->pszContentType =
        CPLStrdup("multipart/form-data; boundary=myboundary");
    {
        const char *pszText = "--myboundary  some junk\r\n"
                              "\r\n"
                              "Bla"
                              "\r\n"
                              "--myboundary--\r\n";
        psResult->pabyData = reinterpret_cast<GByte *>(CPLStrdup(pszText));
        psResult->nDataLen = static_cast<int>(strlen(pszText));
    }
    EXPECT_TRUE(CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)));
    EXPECT_EQ(psResult->nMimePartCount, 1);
    if (psResult->nMimePartCount == 1)
    {
        EXPECT_EQ(psResult->pasMimePart[0].papszHeaders,
                  static_cast<char **>(nullptr));
        EXPECT_EQ(psResult->pasMimePart[0].nDataLen, 3);
        EXPECT_TRUE(
            strncmp(reinterpret_cast<char *>(psResult->pasMimePart[0].pabyData),
                    "Bla", 3) == 0);
    }
    EXPECT_TRUE(CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)));
    CPLHTTPDestroyResult(psResult);

    // Valid single part, with header
    psResult =
        static_cast<CPLHTTPResult *>(CPLCalloc(1, sizeof(CPLHTTPResult)));
    psResult->pszContentType =
        CPLStrdup("multipart/form-data; boundary=myboundary");
    {
        const char *pszText = "--myboundary  some junk\r\n"
                              "Content-Type: bla\r\n"
                              "\r\n"
                              "Bla"
                              "\r\n"
                              "--myboundary--\r\n";
        psResult->pabyData = reinterpret_cast<GByte *>(CPLStrdup(pszText));
        psResult->nDataLen = static_cast<int>(strlen(pszText));
    }
    EXPECT_TRUE(CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)));
    EXPECT_EQ(psResult->nMimePartCount, 1);
    if (psResult->nMimePartCount == 1)
    {
        EXPECT_EQ(CSLCount(psResult->pasMimePart[0].papszHeaders), 1);
        EXPECT_STREQ(psResult->pasMimePart[0].papszHeaders[0],
                     "Content-Type=bla");
        EXPECT_EQ(psResult->pasMimePart[0].nDataLen, 3);
        EXPECT_TRUE(
            strncmp(reinterpret_cast<char *>(psResult->pasMimePart[0].pabyData),
                    "Bla", 3) == 0);
    }
    EXPECT_TRUE(CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)));
    CPLHTTPDestroyResult(psResult);

    // Valid single part, 2 headers
    psResult =
        static_cast<CPLHTTPResult *>(CPLCalloc(1, sizeof(CPLHTTPResult)));
    psResult->pszContentType =
        CPLStrdup("multipart/form-data; boundary=myboundary");
    {
        const char *pszText = "--myboundary  some junk\r\n"
                              "Content-Type: bla\r\n"
                              "Content-Disposition: bar\r\n"
                              "\r\n"
                              "Bla"
                              "\r\n"
                              "--myboundary--\r\n";
        psResult->pabyData = reinterpret_cast<GByte *>(CPLStrdup(pszText));
        psResult->nDataLen = static_cast<int>(strlen(pszText));
    }
    EXPECT_TRUE(CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)));
    EXPECT_EQ(psResult->nMimePartCount, 1);
    if (psResult->nMimePartCount == 1)
    {
        EXPECT_EQ(CSLCount(psResult->pasMimePart[0].papszHeaders), 2);
        EXPECT_STREQ(psResult->pasMimePart[0].papszHeaders[0],
                     "Content-Type=bla");
        EXPECT_STREQ(psResult->pasMimePart[0].papszHeaders[1],
                     "Content-Disposition=bar");
        EXPECT_EQ(psResult->pasMimePart[0].nDataLen, 3);
        EXPECT_TRUE(
            strncmp(reinterpret_cast<char *>(psResult->pasMimePart[0].pabyData),
                    "Bla", 3) == 0);
    }
    EXPECT_TRUE(CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)));
    CPLHTTPDestroyResult(psResult);

    // Single part, but with header without extra terminating \r\n
    // (invalid normally, but apparently necessary for some ArcGIS WCS
    // implementations)
    psResult =
        static_cast<CPLHTTPResult *>(CPLCalloc(1, sizeof(CPLHTTPResult)));
    psResult->pszContentType =
        CPLStrdup("multipart/form-data; boundary=myboundary");
    {
        const char *pszText = "--myboundary  some junk\r\n"
                              "Content-Type: bla\r\n"
                              "Bla"
                              "\r\n"
                              "--myboundary--\r\n";
        psResult->pabyData = reinterpret_cast<GByte *>(CPLStrdup(pszText));
        psResult->nDataLen = static_cast<int>(strlen(pszText));
    }
    EXPECT_TRUE(CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)));
    EXPECT_EQ(psResult->nMimePartCount, 1);
    if (psResult->nMimePartCount == 1)
    {
        EXPECT_STREQ(psResult->pasMimePart[0].papszHeaders[0],
                     "Content-Type=bla");
        EXPECT_EQ(psResult->pasMimePart[0].nDataLen, 3);
        EXPECT_TRUE(
            strncmp(reinterpret_cast<char *>(psResult->pasMimePart[0].pabyData),
                    "Bla", 3) == 0);
    }
    EXPECT_TRUE(CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)));
    CPLHTTPDestroyResult(psResult);

    // Valid 2 parts, no header
    psResult =
        static_cast<CPLHTTPResult *>(CPLCalloc(1, sizeof(CPLHTTPResult)));
    psResult->pszContentType =
        CPLStrdup("multipart/form-data; boundary=myboundary");
    {
        const char *pszText = "--myboundary  some junk\r\n"
                              "\r\n"
                              "Bla"
                              "\r\n"
                              "--myboundary\r\n"
                              "\r\n"
                              "second part"
                              "\r\n"
                              "--myboundary--\r\n";
        psResult->pabyData = reinterpret_cast<GByte *>(CPLStrdup(pszText));
        psResult->nDataLen = static_cast<int>(strlen(pszText));
    }
    EXPECT_TRUE(CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)));
    EXPECT_EQ(psResult->nMimePartCount, 2);
    if (psResult->nMimePartCount == 2)
    {
        EXPECT_EQ(psResult->pasMimePart[0].papszHeaders,
                  static_cast<char **>(nullptr));
        EXPECT_EQ(psResult->pasMimePart[0].nDataLen, 3);
        EXPECT_TRUE(
            strncmp(reinterpret_cast<char *>(psResult->pasMimePart[0].pabyData),
                    "Bla", 3) == 0);
        EXPECT_EQ(psResult->pasMimePart[1].nDataLen, 11);
        EXPECT_TRUE(
            strncmp(reinterpret_cast<char *>(psResult->pasMimePart[1].pabyData),
                    "second part", 11) == 0);
    }
    EXPECT_TRUE(CPL_TO_BOOL(CPLHTTPParseMultipartMime(psResult)));
    CPLHTTPDestroyResult(psResult);
}

// Test cpl::down_cast
TEST_F(test_cpl, down_cast)
{
    struct Base
    {
        virtual ~Base()
        {
        }
    };
    struct Derived : public Base
    {
    };
    Base b;
    Derived d;
    Base *p_b_d = &d;

#ifdef wont_compile
    struct OtherBase
    {
    };
    OtherBase ob;
    ASSERT_EQ(cpl::down_cast<OtherBase *>(p_b_d), &ob);
#endif
#ifdef compile_with_warning
    ASSERT_EQ(cpl::down_cast<Base *>(p_b_d), p_b_d);
#endif
    ASSERT_EQ(cpl::down_cast<Derived *>(p_b_d), &d);
    ASSERT_EQ(cpl::down_cast<Derived *>(static_cast<Base *>(nullptr)),
              static_cast<Derived *>(nullptr));
}

// Test CPLPrintTime() in particular case of RFC822 formatting in C locale
TEST_F(test_cpl, CPLPrintTime_RFC822)
{
    char szDate[64];
    struct tm tm;
    tm.tm_sec = 56;
    tm.tm_min = 34;
    tm.tm_hour = 12;
    tm.tm_mday = 20;
    tm.tm_mon = 6 - 1;
    tm.tm_year = 2018 - 1900;
    tm.tm_wday = 3;   // Wednesday
    tm.tm_yday = 0;   // unused
    tm.tm_isdst = 0;  // unused
    int nRet = CPLPrintTime(szDate, sizeof(szDate) - 1,
                            "%a, %d %b %Y %H:%M:%S GMT", &tm, "C");
    szDate[nRet] = 0;
    ASSERT_STREQ(szDate, "Wed, 20 Jun 2018 12:34:56 GMT");
}

// Test CPLAutoClose
TEST_F(test_cpl, CPLAutoClose)
{
    static int counter = 0;
    class AutoCloseTest
    {
      public:
        AutoCloseTest()
        {
            counter += 222;
        }
        virtual ~AutoCloseTest()
        {
            counter -= 22;
        }
        static AutoCloseTest *Create()
        {
            return new AutoCloseTest;
        }
        static void Destroy(AutoCloseTest *p)
        {
            delete p;
        }
    };
    {
        AutoCloseTest *p1 = AutoCloseTest::Create();
        CPL_AUTO_CLOSE_WARP(p1, AutoCloseTest::Destroy);

        AutoCloseTest *p2 = AutoCloseTest::Create();
        CPL_AUTO_CLOSE_WARP(p2, AutoCloseTest::Destroy);
    }
    ASSERT_EQ(counter, 400);
}

// Test cpl_minixml
TEST_F(test_cpl, cpl_minixml)
{
    CPLXMLNode *psRoot = CPLCreateXMLNode(nullptr, CXT_Element, "Root");
    CPLXMLNode *psElt = CPLCreateXMLElementAndValue(psRoot, "Elt", "value");
    CPLAddXMLAttributeAndValue(psElt, "attr1", "val1");
    CPLAddXMLAttributeAndValue(psElt, "attr2", "val2");
    char *str = CPLSerializeXMLTree(psRoot);
    CPLDestroyXMLNode(psRoot);
    ASSERT_STREQ(
        str,
        "<Root>\n  <Elt attr1=\"val1\" attr2=\"val2\">value</Elt>\n</Root>\n");
    CPLFree(str);
}

// Test CPLCharUniquePtr
TEST_F(test_cpl, CPLCharUniquePtr)
{
    CPLCharUniquePtr x;
    ASSERT_TRUE(x.get() == nullptr);
    x.reset(CPLStrdup("foo"));
    ASSERT_STREQ(x.get(), "foo");
}

// Test CPLJSonStreamingWriter
TEST_F(test_cpl, CPLJSonStreamingWriter)
{
    {
        CPLJSonStreamingWriter x(nullptr, nullptr);
        ASSERT_EQ(x.GetString(), std::string());
    }
    {
        CPLJSonStreamingWriter x(nullptr, nullptr);
        x.Add(true);
        ASSERT_EQ(x.GetString(), std::string("true"));
    }
    {
        std::string res;
        struct MyCallback
        {
            static void f(const char *pszText, void *user_data)
            {
                *static_cast<std::string *>(user_data) += pszText;
            }
        };
        CPLJSonStreamingWriter x(&MyCallback::f, &res);
        x.Add(true);
        ASSERT_EQ(x.GetString(), std::string());
        ASSERT_EQ(res, std::string("true"));
    }
    {
        CPLJSonStreamingWriter x(nullptr, nullptr);
        x.Add(false);
        ASSERT_EQ(x.GetString(), std::string("false"));
    }
    {
        CPLJSonStreamingWriter x(nullptr, nullptr);
        x.AddNull();
        ASSERT_EQ(x.GetString(), std::string("null"));
    }
    {
        CPLJSonStreamingWriter x(nullptr, nullptr);
        x.Add(1);
        ASSERT_EQ(x.GetString(), std::string("1"));
    }
    {
        CPLJSonStreamingWriter x(nullptr, nullptr);
        x.Add(4200000000U);
        ASSERT_EQ(x.GetString(), std::string("4200000000"));
    }
    {
        CPLJSonStreamingWriter x(nullptr, nullptr);
        x.Add(static_cast<std::int64_t>(-10000) * 1000000);
        ASSERT_EQ(x.GetString(), std::string("-10000000000"));
    }
    {
        CPLJSonStreamingWriter x(nullptr, nullptr);
        x.Add(static_cast<std::uint64_t>(10000) * 1000000);
        ASSERT_EQ(x.GetString(), std::string("10000000000"));
    }
    {
        CPLJSonStreamingWriter x(nullptr, nullptr);
        x.Add(1.5f);
        ASSERT_EQ(x.GetString(), std::string("1.5"));
    }
    {
        CPLJSonStreamingWriter x(nullptr, nullptr);
        x.Add(std::numeric_limits<float>::quiet_NaN());
        ASSERT_EQ(x.GetString(), std::string("\"NaN\""));
    }
    {
        CPLJSonStreamingWriter x(nullptr, nullptr);
        x.Add(std::numeric_limits<float>::infinity());
        ASSERT_EQ(x.GetString(), std::string("\"Infinity\""));
    }
    {
        CPLJSonStreamingWriter x(nullptr, nullptr);
        x.Add(-std::numeric_limits<float>::infinity());
        ASSERT_EQ(x.GetString(), std::string("\"-Infinity\""));
    }
    {
        CPLJSonStreamingWriter x(nullptr, nullptr);
        x.Add(1.25);
        ASSERT_EQ(x.GetString(), std::string("1.25"));
    }
    {
        CPLJSonStreamingWriter x(nullptr, nullptr);
        x.Add(std::numeric_limits<double>::quiet_NaN());
        ASSERT_EQ(x.GetString(), std::string("\"NaN\""));
    }
    {
        CPLJSonStreamingWriter x(nullptr, nullptr);
        x.Add(std::numeric_limits<double>::infinity());
        ASSERT_EQ(x.GetString(), std::string("\"Infinity\""));
    }
    {
        CPLJSonStreamingWriter x(nullptr, nullptr);
        x.Add(-std::numeric_limits<double>::infinity());
        ASSERT_EQ(x.GetString(), std::string("\"-Infinity\""));
    }
    {
        CPLJSonStreamingWriter x(nullptr, nullptr);
        x.Add(std::string("foo\\bar\"baz\b\f\n\r\t"
                          "\x01"
                          "boo"));
        ASSERT_EQ(
            x.GetString(),
            std::string("\"foo\\\\bar\\\"baz\\b\\f\\n\\r\\t\\u0001boo\""));
    }
    {
        CPLJSonStreamingWriter x(nullptr, nullptr);
        x.Add("foo\\bar\"baz\b\f\n\r\t"
              "\x01"
              "boo");
        ASSERT_EQ(
            x.GetString(),
            std::string("\"foo\\\\bar\\\"baz\\b\\f\\n\\r\\t\\u0001boo\""));
    }
    {
        CPLJSonStreamingWriter x(nullptr, nullptr);
        x.SetPrettyFormatting(false);
        {
            auto ctxt(x.MakeObjectContext());
        }
        ASSERT_EQ(x.GetString(), std::string("{}"));
    }
    {
        CPLJSonStreamingWriter x(nullptr, nullptr);
        {
            auto ctxt(x.MakeObjectContext());
        }
        ASSERT_EQ(x.GetString(), std::string("{}"));
    }
    {
        CPLJSonStreamingWriter x(nullptr, nullptr);
        x.SetPrettyFormatting(false);
        {
            auto ctxt(x.MakeObjectContext());
            x.AddObjKey("key");
            x.Add("value");
        }
        ASSERT_EQ(x.GetString(), std::string("{\"key\":\"value\"}"));
    }
    {
        CPLJSonStreamingWriter x(nullptr, nullptr);
        {
            auto ctxt(x.MakeObjectContext());
            x.AddObjKey("key");
            x.Add("value");
        }
        ASSERT_EQ(x.GetString(), std::string("{\n  \"key\": \"value\"\n}"));
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
        ASSERT_EQ(
            x.GetString(),
            std::string("{\n  \"key\": \"value\",\n  \"key2\": \"value2\"\n}"));
    }
    {
        CPLJSonStreamingWriter x(nullptr, nullptr);
        {
            auto ctxt(x.MakeArrayContext());
        }
        ASSERT_EQ(x.GetString(), std::string("[]"));
    }
    {
        CPLJSonStreamingWriter x(nullptr, nullptr);
        {
            auto ctxt(x.MakeArrayContext());
            x.Add(1);
        }
        ASSERT_EQ(x.GetString(), std::string("[\n  1\n]"));
    }
    {
        CPLJSonStreamingWriter x(nullptr, nullptr);
        {
            auto ctxt(x.MakeArrayContext());
            x.Add(1);
            x.Add(2);
        }
        ASSERT_EQ(x.GetString(), std::string("[\n  1,\n  2\n]"));
    }
    {
        CPLJSonStreamingWriter x(nullptr, nullptr);
        {
            auto ctxt(x.MakeArrayContext(true));
            x.Add(1);
            x.Add(2);
        }
        ASSERT_EQ(x.GetString(), std::string("[1, 2]"));
    }
}

// Test CPLWorkerThreadPool
TEST_F(test_cpl, CPLWorkerThreadPool)
{
    CPLWorkerThreadPool oPool;
    ASSERT_TRUE(oPool.Setup(2, nullptr, nullptr, false));

    const auto myJob = [](void *pData) { (*static_cast<int *>(pData))++; };

    {
        std::vector<int> res(1000);
        for (int i = 0; i < 1000; i++)
        {
            res[i] = i;
            oPool.SubmitJob(myJob, &res[i]);
        }
        oPool.WaitCompletion();
        for (int i = 0; i < 1000; i++)
        {
            ASSERT_EQ(res[i], i + 1);
        }
    }

    {
        std::vector<int> res(1000);
        std::vector<void *> resPtr(1000);
        for (int i = 0; i < 1000; i++)
        {
            res[i] = i;
            resPtr[i] = res.data() + i;
        }
        oPool.SubmitJobs(myJob, resPtr);
        oPool.WaitEvent();
        oPool.WaitCompletion();
        for (int i = 0; i < 1000; i++)
        {
            ASSERT_EQ(res[i], i + 1);
        }
    }

    {
        auto jobQueue1 = oPool.CreateJobQueue();
        auto jobQueue2 = oPool.CreateJobQueue();

        ASSERT_EQ(jobQueue1->GetPool(), &oPool);

        std::vector<int> res(1000);
        for (int i = 0; i < 1000; i++)
        {
            res[i] = i;
            if (i % 2)
                jobQueue1->SubmitJob(myJob, &res[i]);
            else
                jobQueue2->SubmitJob(myJob, &res[i]);
        }
        jobQueue1->WaitCompletion();
        jobQueue2->WaitCompletion();
        for (int i = 0; i < 1000; i++)
        {
            ASSERT_EQ(res[i], i + 1);
        }
    }
}

// Test CPLHTTPFetch
TEST_F(test_cpl, CPLHTTPFetch)
{
#ifdef HAVE_CURL
    CPLStringList oOptions;
    oOptions.AddNameValue("FORM_ITEM_COUNT", "5");
    oOptions.AddNameValue("FORM_KEY_0", "qqq");
    oOptions.AddNameValue("FORM_VALUE_0", "www");
    CPLHTTPResult *pResult = CPLHTTPFetch("http://example.com", oOptions);
    EXPECT_EQ(pResult->nStatus, 34);
    CPLHTTPDestroyResult(pResult);
    pResult = nullptr;
    oOptions.Clear();

    oOptions.AddNameValue("FORM_FILE_PATH", "not_existed");
    pResult = CPLHTTPFetch("http://example.com", oOptions);
    EXPECT_EQ(pResult->nStatus, 34);
    CPLHTTPDestroyResult(pResult);
#else
    GTEST_SKIP() << "CURL not available";
#endif  // HAVE_CURL
}

// Test CPLHTTPPushFetchCallback
TEST_F(test_cpl, CPLHTTPPushFetchCallback)
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

    const auto myCbk = [](const char *pszURL, CSLConstList papszOptions,
                          GDALProgressFunc pfnProgress, void *pProgressArg,
                          CPLHTTPFetchWriteFunc pfnWrite, void *pWriteArg,
                          void *pUserData)
    {
        myCbkUserDataStruct *pCbkUserData =
            static_cast<myCbkUserDataStruct *>(pUserData);
        pCbkUserData->osURL = pszURL;
        pCbkUserData->papszOptions = papszOptions;
        pCbkUserData->pfnProgress = pfnProgress;
        pCbkUserData->pProgressArg = pProgressArg;
        pCbkUserData->pfnWrite = pfnWrite;
        pCbkUserData->pWriteArg = pWriteArg;
        auto psResult =
            static_cast<CPLHTTPResult *>(CPLCalloc(sizeof(CPLHTTPResult), 1));
        psResult->nStatus = 123;
        return psResult;
    };

    myCbkUserDataStruct userData;
    EXPECT_TRUE(CPLHTTPPushFetchCallback(myCbk, &userData));

    int progressArg = 0;
    const auto myWriteCbk = [](void *, size_t, size_t, void *) -> size_t
    { return 0; };
    int writeCbkArg = 00;

    CPLStringList aosOptions;
    GDALProgressFunc pfnProgress = GDALTermProgress;
    CPLHTTPFetchWriteFunc pfnWriteCbk = myWriteCbk;
    CPLHTTPResult *pResult =
        CPLHTTPFetchEx("http://example.com", aosOptions.List(), pfnProgress,
                       &progressArg, pfnWriteCbk, &writeCbkArg);
    ASSERT_TRUE(pResult != nullptr);
    EXPECT_EQ(pResult->nStatus, 123);
    CPLHTTPDestroyResult(pResult);

    EXPECT_TRUE(CPLHTTPPopFetchCallback());
    CPLPushErrorHandler(CPLQuietErrorHandler);
    EXPECT_TRUE(!CPLHTTPPopFetchCallback());
    CPLPopErrorHandler();

    EXPECT_STREQ(userData.osURL, "http://example.com");
    EXPECT_EQ(userData.papszOptions, aosOptions.List());
    EXPECT_EQ(userData.pfnProgress, pfnProgress);
    EXPECT_EQ(userData.pProgressArg, &progressArg);
    EXPECT_EQ(userData.pfnWrite, pfnWriteCbk);
    EXPECT_EQ(userData.pWriteArg, &writeCbkArg);
}

// Test CPLHTTPSetFetchCallback
TEST_F(test_cpl, CPLHTTPSetFetchCallback)
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

    const auto myCbk2 = [](const char *pszURL, CSLConstList papszOptions,
                           GDALProgressFunc pfnProgress, void *pProgressArg,
                           CPLHTTPFetchWriteFunc pfnWrite, void *pWriteArg,
                           void *pUserData)
    {
        myCbkUserDataStruct *pCbkUserData =
            static_cast<myCbkUserDataStruct *>(pUserData);
        pCbkUserData->osURL = pszURL;
        pCbkUserData->papszOptions = papszOptions;
        pCbkUserData->pfnProgress = pfnProgress;
        pCbkUserData->pProgressArg = pProgressArg;
        pCbkUserData->pfnWrite = pfnWrite;
        pCbkUserData->pWriteArg = pWriteArg;
        auto psResult =
            static_cast<CPLHTTPResult *>(CPLCalloc(sizeof(CPLHTTPResult), 1));
        psResult->nStatus = 124;
        return psResult;
    };
    myCbkUserDataStruct userData2;
    CPLHTTPSetFetchCallback(myCbk2, &userData2);

    int progressArg = 0;
    const auto myWriteCbk = [](void *, size_t, size_t, void *) -> size_t
    { return 0; };
    int writeCbkArg = 00;

    CPLStringList aosOptions;
    GDALProgressFunc pfnProgress = GDALTermProgress;
    CPLHTTPFetchWriteFunc pfnWriteCbk = myWriteCbk;
    CPLHTTPResult *pResult =
        CPLHTTPFetchEx("http://example.com", aosOptions.List(), pfnProgress,
                       &progressArg, pfnWriteCbk, &writeCbkArg);
    ASSERT_TRUE(pResult != nullptr);
    EXPECT_EQ(pResult->nStatus, 124);
    CPLHTTPDestroyResult(pResult);

    CPLHTTPSetFetchCallback(nullptr, nullptr);

    EXPECT_STREQ(userData2.osURL, "http://example.com");
    EXPECT_EQ(userData2.papszOptions, aosOptions.List());
    EXPECT_EQ(userData2.pfnProgress, pfnProgress);
    EXPECT_EQ(userData2.pProgressArg, &progressArg);
    EXPECT_EQ(userData2.pfnWrite, pfnWriteCbk);
    EXPECT_EQ(userData2.pWriteArg, &writeCbkArg);
}

// Test CPLLoadConfigOptionsFromFile() and
// CPLLoadConfigOptionsFromPredefinedFiles()
TEST_F(test_cpl, CPLLoadConfigOptionsFromFile)
{
    CPLLoadConfigOptionsFromFile("/i/do/not/exist", false);

    VSILFILE *fp = VSIFOpenL("/vsimem/.gdal/gdalrc", "wb");
    VSIFPrintfL(fp, "# some comment\n");
    VSIFPrintfL(fp, "\n");    // blank line
    VSIFPrintfL(fp, "  \n");  // blank line
    VSIFPrintfL(fp, "[configoptions]\n");
    VSIFPrintfL(fp, "# some comment\n");
    VSIFPrintfL(fp, "FOO_CONFIGOPTION=BAR\n");
    VSIFCloseL(fp);

    // Try CPLLoadConfigOptionsFromFile()
    CPLLoadConfigOptionsFromFile("/vsimem/.gdal/gdalrc", false);
    ASSERT_TRUE(EQUAL(CPLGetConfigOption("FOO_CONFIGOPTION", ""), "BAR"));
    CPLSetConfigOption("FOO_CONFIGOPTION", nullptr);

    // Try CPLLoadConfigOptionsFromPredefinedFiles() with GDAL_CONFIG_FILE set
    CPLSetConfigOption("GDAL_CONFIG_FILE", "/vsimem/.gdal/gdalrc");
    CPLLoadConfigOptionsFromPredefinedFiles();
    ASSERT_TRUE(EQUAL(CPLGetConfigOption("FOO_CONFIGOPTION", ""), "BAR"));
    CPLSetConfigOption("FOO_CONFIGOPTION", nullptr);

    // Try CPLLoadConfigOptionsFromPredefinedFiles() with $HOME/.gdal/gdalrc
    // file
#ifdef WIN32
    const char *pszHOMEEnvVarName = "USERPROFILE";
#else
    const char *pszHOMEEnvVarName = "HOME";
#endif
    CPLString osOldVal(CPLGetConfigOption(pszHOMEEnvVarName, ""));
    CPLSetConfigOption(pszHOMEEnvVarName, "/vsimem/");
    CPLLoadConfigOptionsFromPredefinedFiles();
    ASSERT_TRUE(EQUAL(CPLGetConfigOption("FOO_CONFIGOPTION", ""), "BAR"));
    CPLSetConfigOption("FOO_CONFIGOPTION", nullptr);
    if (!osOldVal.empty())
        CPLSetConfigOption(pszHOMEEnvVarName, osOldVal.c_str());
    else
        CPLSetConfigOption(pszHOMEEnvVarName, nullptr);

    VSIUnlink("/vsimem/.gdal/gdalrc");
}

// Test decompressor side of cpl_compressor.h
TEST_F(test_cpl, decompressor)
{
    const auto compressionLambda =
        [](const void * /* input_data */, size_t /* input_size */,
           void ** /* output_data */, size_t * /* output_size */,
           CSLConstList /* options */, void * /* compressor_user_data */)
    { return false; };
    int dummy = 0;

    CPLCompressor sComp;
    sComp.nStructVersion = 1;
    sComp.eType = CCT_COMPRESSOR;
    sComp.pszId = "my_comp";
    const char *const apszMetadata[] = {"FOO=BAR", nullptr};
    sComp.papszMetadata = apszMetadata;
    sComp.pfnFunc = compressionLambda;
    sComp.user_data = &dummy;

    ASSERT_TRUE(CPLRegisterDecompressor(&sComp));

    CPLPushErrorHandler(CPLQuietErrorHandler);
    ASSERT_TRUE(!CPLRegisterDecompressor(&sComp));
    CPLPopErrorHandler();

    char **decompressors = CPLGetDecompressors();
    ASSERT_TRUE(decompressors != nullptr);
    EXPECT_TRUE(CSLFindString(decompressors, sComp.pszId) >= 0);
    for (auto iter = decompressors; *iter; ++iter)
    {
        const auto pCompressor = CPLGetDecompressor(*iter);
        EXPECT_TRUE(pCompressor);
        if (pCompressor)
        {
            const char *pszOptions =
                CSLFetchNameValue(pCompressor->papszMetadata, "OPTIONS");
            if (pszOptions)
            {
                auto psNode = CPLParseXMLString(pszOptions);
                EXPECT_TRUE(psNode);
                CPLDestroyXMLNode(psNode);
            }
            else
            {
                CPLDebug("TEST", "Decompressor %s has no OPTIONS", *iter);
            }
        }
    }
    CSLDestroy(decompressors);

    EXPECT_TRUE(CPLGetDecompressor("invalid") == nullptr);
    const auto pCompressor = CPLGetDecompressor(sComp.pszId);
    ASSERT_TRUE(pCompressor);
    EXPECT_STREQ(pCompressor->pszId, sComp.pszId);
    EXPECT_EQ(CSLCount(pCompressor->papszMetadata),
              CSLCount(sComp.papszMetadata));
    EXPECT_TRUE(pCompressor->pfnFunc != nullptr);
    EXPECT_EQ(pCompressor->user_data, sComp.user_data);

    CPLDestroyCompressorRegistry();
    EXPECT_TRUE(CPLGetDecompressor(sComp.pszId) == nullptr);
}

// Test compressor side of cpl_compressor.h
TEST_F(test_cpl, compressor)
{
    const auto compressionLambda =
        [](const void * /* input_data */, size_t /* input_size */,
           void ** /* output_data */, size_t * /* output_size */,
           CSLConstList /* options */, void * /* compressor_user_data */)
    { return false; };
    int dummy = 0;

    CPLCompressor sComp;
    sComp.nStructVersion = 1;
    sComp.eType = CCT_COMPRESSOR;
    sComp.pszId = "my_comp";
    const char *const apszMetadata[] = {"FOO=BAR", nullptr};
    sComp.papszMetadata = apszMetadata;
    sComp.pfnFunc = compressionLambda;
    sComp.user_data = &dummy;

    ASSERT_TRUE(CPLRegisterCompressor(&sComp));

    CPLPushErrorHandler(CPLQuietErrorHandler);
    ASSERT_TRUE(!CPLRegisterCompressor(&sComp));
    CPLPopErrorHandler();

    char **compressors = CPLGetCompressors();
    ASSERT_TRUE(compressors != nullptr);
    EXPECT_TRUE(CSLFindString(compressors, sComp.pszId) >= 0);
    for (auto iter = compressors; *iter; ++iter)
    {
        const auto pCompressor = CPLGetCompressor(*iter);
        EXPECT_TRUE(pCompressor);
        if (pCompressor)
        {
            const char *pszOptions =
                CSLFetchNameValue(pCompressor->papszMetadata, "OPTIONS");
            if (pszOptions)
            {
                auto psNode = CPLParseXMLString(pszOptions);
                EXPECT_TRUE(psNode);
                CPLDestroyXMLNode(psNode);
            }
            else
            {
                CPLDebug("TEST", "Compressor %s has no OPTIONS", *iter);
            }
        }
    }
    CSLDestroy(compressors);

    EXPECT_TRUE(CPLGetCompressor("invalid") == nullptr);
    const auto pCompressor = CPLGetCompressor(sComp.pszId);
    ASSERT_TRUE(pCompressor);
    if (pCompressor == nullptr)
        return;
    EXPECT_STREQ(pCompressor->pszId, sComp.pszId);
    EXPECT_EQ(CSLCount(pCompressor->papszMetadata),
              CSLCount(sComp.papszMetadata));
    EXPECT_TRUE(pCompressor->pfnFunc != nullptr);
    EXPECT_EQ(pCompressor->user_data, sComp.user_data);

    CPLDestroyCompressorRegistry();
    EXPECT_TRUE(CPLGetDecompressor(sComp.pszId) == nullptr);
}

// Test builtin compressors/decompressor
TEST_F(test_cpl, builtin_compressors)
{
    for (const char *id : {"blosc", "zlib", "gzip", "lzma", "zstd", "lz4"})
    {
        const auto pCompressor = CPLGetCompressor(id);
        if (pCompressor == nullptr)
        {
            CPLDebug("TEST", "%s not available", id);
            if (strcmp(id, "zlib") == 0 || strcmp(id, "gzip") == 0)
            {
                ASSERT_TRUE(false);
            }
            continue;
        }
        CPLDebug("TEST", "Testing %s", id);

        const char my_str[] = "my string to compress";
        const char *const options[] = {"TYPESIZE=1", nullptr};

        // Compressor side

        // Just get output size
        size_t out_size = 0;
        ASSERT_TRUE(pCompressor->pfnFunc(my_str, strlen(my_str), nullptr,
                                         &out_size, options,
                                         pCompressor->user_data));
        ASSERT_TRUE(out_size != 0);

        // Let it alloc the output buffer
        void *out_buffer2 = nullptr;
        size_t out_size2 = 0;
        ASSERT_TRUE(pCompressor->pfnFunc(my_str, strlen(my_str), &out_buffer2,
                                         &out_size2, options,
                                         pCompressor->user_data));
        ASSERT_TRUE(out_buffer2 != nullptr);
        ASSERT_TRUE(out_size2 != 0);
        ASSERT_TRUE(out_size2 <= out_size);

        std::vector<GByte> out_buffer3(out_size);

        // Provide not large enough buffer size
        size_t out_size3 = 1;
        void *out_buffer3_ptr = &out_buffer3[0];
        ASSERT_TRUE(!(pCompressor->pfnFunc(my_str, strlen(my_str),
                                           &out_buffer3_ptr, &out_size3,
                                           options, pCompressor->user_data)));

        // Provide the output buffer
        out_size3 = out_buffer3.size();
        out_buffer3_ptr = &out_buffer3[0];
        ASSERT_TRUE(pCompressor->pfnFunc(my_str, strlen(my_str),
                                         &out_buffer3_ptr, &out_size3, options,
                                         pCompressor->user_data));
        ASSERT_TRUE(out_buffer3_ptr != nullptr);
        ASSERT_TRUE(out_buffer3_ptr == &out_buffer3[0]);
        ASSERT_TRUE(out_size3 != 0);
        ASSERT_EQ(out_size3, out_size2);

        out_buffer3.resize(out_size3);
        out_buffer3_ptr = &out_buffer3[0];

        ASSERT_TRUE(memcmp(out_buffer3_ptr, out_buffer2, out_size2) == 0);

        CPLFree(out_buffer2);

        const std::vector<GByte> compressedData(out_buffer3);

        // Decompressor side
        const auto pDecompressor = CPLGetDecompressor(id);
        ASSERT_TRUE(pDecompressor != nullptr);

        out_size = 0;
        ASSERT_TRUE(pDecompressor->pfnFunc(
            compressedData.data(), compressedData.size(), nullptr, &out_size,
            nullptr, pDecompressor->user_data));
        ASSERT_TRUE(out_size != 0);
        ASSERT_TRUE(out_size >= strlen(my_str));

        out_buffer2 = nullptr;
        out_size2 = 0;
        ASSERT_TRUE(pDecompressor->pfnFunc(
            compressedData.data(), compressedData.size(), &out_buffer2,
            &out_size2, options, pDecompressor->user_data));
        ASSERT_TRUE(out_buffer2 != nullptr);
        ASSERT_TRUE(out_size2 != 0);
        ASSERT_EQ(out_size2, strlen(my_str));
        ASSERT_TRUE(memcmp(out_buffer2, my_str, strlen(my_str)) == 0);
        CPLFree(out_buffer2);

        out_buffer3.clear();
        out_buffer3.resize(out_size);
        out_size3 = out_buffer3.size();
        out_buffer3_ptr = &out_buffer3[0];
        ASSERT_TRUE(pDecompressor->pfnFunc(
            compressedData.data(), compressedData.size(), &out_buffer3_ptr,
            &out_size3, options, pDecompressor->user_data));
        ASSERT_TRUE(out_buffer3_ptr != nullptr);
        ASSERT_TRUE(out_buffer3_ptr == &out_buffer3[0]);
        ASSERT_EQ(out_size3, strlen(my_str));
        ASSERT_TRUE(memcmp(out_buffer3.data(), my_str, strlen(my_str)) == 0);
    }
}

template <class T> struct TesterDelta
{
    static void test(const char *dtypeOption)
    {
        const auto pCompressor = CPLGetCompressor("delta");
        ASSERT_TRUE(pCompressor);
        if (pCompressor == nullptr)
            return;
        const auto pDecompressor = CPLGetDecompressor("delta");
        ASSERT_TRUE(pDecompressor);
        if (pDecompressor == nullptr)
            return;

        const T tabIn[] = {static_cast<T>(-2), 3, 1};
        T tabCompress[3];
        T tabOut[3];
        const char *const apszOptions[] = {dtypeOption, nullptr};

        void *outPtr = &tabCompress[0];
        size_t outSize = sizeof(tabCompress);
        ASSERT_TRUE(pCompressor->pfnFunc(&tabIn[0], sizeof(tabIn), &outPtr,
                                         &outSize, apszOptions,
                                         pCompressor->user_data));
        ASSERT_EQ(outSize, sizeof(tabCompress));

        // ASSERT_EQ(tabCompress[0], 2);
        // ASSERT_EQ(tabCompress[1], 1);
        // ASSERT_EQ(tabCompress[2], -2);

        outPtr = &tabOut[0];
        outSize = sizeof(tabOut);
        ASSERT_TRUE(pDecompressor->pfnFunc(&tabCompress[0], sizeof(tabCompress),
                                           &outPtr, &outSize, apszOptions,
                                           pDecompressor->user_data));
        ASSERT_EQ(outSize, sizeof(tabOut));
        ASSERT_EQ(tabOut[0], tabIn[0]);
        ASSERT_EQ(tabOut[1], tabIn[1]);
        ASSERT_EQ(tabOut[2], tabIn[2]);
    }
};

// Test delta compressor/decompressor
TEST_F(test_cpl, delta_compressor)
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
TEST_F(test_cpl, CPLQuadTree)
{
    unsigned next = 0;

    const auto DummyRandInit = [&next](unsigned initValue)
    { next = initValue; };

    constexpr int MAX_RAND_VAL = 32767;

    // Slightly improved version of https://xkcd.com/221/, as suggested by
    // "man srand"
    const auto DummyRand = [&]()
    {
        next = next * 1103515245 + 12345;
        return ((unsigned)(next / 65536) % (MAX_RAND_VAL + 1));
    };

    CPLRectObj globalbounds;
    globalbounds.minx = 0;
    globalbounds.miny = 0;
    globalbounds.maxx = 1;
    globalbounds.maxy = 1;

    auto hTree = CPLQuadTreeCreate(&globalbounds, nullptr);
    ASSERT_TRUE(hTree != nullptr);

    const auto GenerateRandomRect = [&](CPLRectObj &rect)
    {
        rect.minx = double(DummyRand()) / MAX_RAND_VAL;
        rect.miny = double(DummyRand()) / MAX_RAND_VAL;
        rect.maxx =
            rect.minx + double(DummyRand()) / MAX_RAND_VAL * (1 - rect.minx);
        rect.maxy =
            rect.miny + double(DummyRand()) / MAX_RAND_VAL * (1 - rect.miny);
    };

    for (int j = 0; j < 2; j++)
    {
        DummyRandInit(j);
        for (int i = 0; i < 1000; i++)
        {
            CPLRectObj rect;
            GenerateRandomRect(rect);
            void *hFeature =
                reinterpret_cast<void *>(static_cast<uintptr_t>(i));
            CPLQuadTreeInsertWithBounds(hTree, hFeature, &rect);
        }

        {
            int nFeatureCount = 0;
            CPLFree(CPLQuadTreeSearch(hTree, &globalbounds, &nFeatureCount));
            ASSERT_EQ(nFeatureCount, 1000);
        }

        DummyRandInit(j);
        for (int i = 0; i < 1000; i++)
        {
            CPLRectObj rect;
            GenerateRandomRect(rect);
            void *hFeature =
                reinterpret_cast<void *>(static_cast<uintptr_t>(i));
            CPLQuadTreeRemove(hTree, hFeature, &rect);
        }

        {
            int nFeatureCount = 0;
            CPLFree(CPLQuadTreeSearch(hTree, &globalbounds, &nFeatureCount));
            ASSERT_EQ(nFeatureCount, 0);
        }
    }

    CPLQuadTreeDestroy(hTree);
}

// Test bUnlinkAndSize on VSIGetMemFileBuffer
TEST_F(test_cpl, VSIGetMemFileBuffer_unlink_and_size)
{
    VSILFILE *fp = VSIFOpenL("/vsimem/test_unlink_and_seize.tif", "wb");
    VSIFWriteL("test", 5, 1, fp);
    GByte *pRawData =
        VSIGetMemFileBuffer("/vsimem/test_unlink_and_seize.tif", nullptr, true);
    ASSERT_TRUE(EQUAL(reinterpret_cast<const char *>(pRawData), "test"));
    ASSERT_TRUE(VSIGetMemFileBuffer("/vsimem/test_unlink_and_seize.tif",
                                    nullptr, false) == nullptr);
    ASSERT_TRUE(VSIFOpenL("/vsimem/test_unlink_and_seize.tif", "r") == nullptr);
    ASSERT_TRUE(VSIFReadL(pRawData, 5, 1, fp) == 0);
    ASSERT_TRUE(VSIFWriteL(pRawData, 5, 1, fp) == 0);
    ASSERT_TRUE(VSIFSeekL(fp, 0, SEEK_END) == 0);
    CPLFree(pRawData);
    VSIFCloseL(fp);
}

// Test CPLLoadConfigOptionsFromFile() for VSI credentials
TEST_F(test_cpl, CPLLoadConfigOptionsFromFile_VSI_credentials)
{
    VSILFILE *fp = VSIFOpenL("/vsimem/credentials.txt", "wb");
    VSIFPrintfL(fp, "[credentials]\n");
    VSIFPrintfL(fp, "\n");
    VSIFPrintfL(fp, "[.my_subsection]\n");
    VSIFPrintfL(fp, "path=/vsi_test/foo/bar\n");
    VSIFPrintfL(fp, "FOO=BAR\n");
    VSIFPrintfL(fp, "FOO2=BAR2\n");
    VSIFPrintfL(fp, "\n");
    VSIFPrintfL(fp, "[.my_subsection2]\n");
    VSIFPrintfL(fp, "path=/vsi_test/bar/baz\n");
    VSIFPrintfL(fp, "BAR=BAZ\n");
    VSIFPrintfL(fp, "[configoptions]\n");
    VSIFPrintfL(fp, "configoptions_FOO=BAR\n");
    VSIFCloseL(fp);

    CPLErrorReset();
    CPLLoadConfigOptionsFromFile("/vsimem/credentials.txt", false);
    ASSERT_EQ(CPLGetLastErrorType(), CE_None);

    {
        const char *pszVal =
            VSIGetPathSpecificOption("/vsi_test/foo/bar", "FOO", nullptr);
        ASSERT_TRUE(pszVal != nullptr);
        ASSERT_EQ(std::string(pszVal), std::string("BAR"));
    }

    {
        const char *pszVal =
            VSIGetPathSpecificOption("/vsi_test/foo/bar", "FOO2", nullptr);
        ASSERT_TRUE(pszVal != nullptr);
        ASSERT_EQ(std::string(pszVal), std::string("BAR2"));
    }

    {
        const char *pszVal =
            VSIGetPathSpecificOption("/vsi_test/bar/baz", "BAR", nullptr);
        ASSERT_TRUE(pszVal != nullptr);
        ASSERT_EQ(std::string(pszVal), std::string("BAZ"));
    }

    {
        const char *pszVal = CPLGetConfigOption("configoptions_FOO", nullptr);
        ASSERT_TRUE(pszVal != nullptr);
        ASSERT_EQ(std::string(pszVal), std::string("BAR"));
    }

    VSIClearPathSpecificOptions("/vsi_test/bar/baz");
    CPLSetConfigOption("configoptions_FOO", nullptr);

    {
        const char *pszVal =
            VSIGetPathSpecificOption("/vsi_test/bar/baz", "BAR", nullptr);
        ASSERT_TRUE(pszVal == nullptr);
    }

    VSIUnlink("/vsimem/credentials.txt");
}

// Test CPLLoadConfigOptionsFromFile() for VSI credentials, warning case
TEST_F(test_cpl, CPLLoadConfigOptionsFromFile_VSI_credentials_warning)
{
    VSILFILE *fp = VSIFOpenL("/vsimem/credentials.txt", "wb");
    VSIFPrintfL(fp, "[credentials]\n");
    VSIFPrintfL(fp, "\n");
    VSIFPrintfL(fp, "FOO=BAR\n");  // content outside of subsection
    VSIFCloseL(fp);

    CPLErrorReset();
    CPLPushErrorHandler(CPLQuietErrorHandler);
    CPLLoadConfigOptionsFromFile("/vsimem/credentials.txt", false);
    CPLPopErrorHandler();
    ASSERT_EQ(CPLGetLastErrorType(), CE_Warning);

    VSIUnlink("/vsimem/credentials.txt");
}

// Test CPLLoadConfigOptionsFromFile() for VSI credentials, warning case
TEST_F(test_cpl,
       CPLLoadConfigOptionsFromFile_VSI_credentials_subsection_warning)
{
    VSILFILE *fp = VSIFOpenL("/vsimem/credentials.txt", "wb");
    VSIFPrintfL(fp, "[credentials]\n");
    VSIFPrintfL(fp, "[.subsection]\n");
    VSIFPrintfL(fp, "FOO=BAR\n");  // first key is not 'path'
    VSIFCloseL(fp);

    CPLErrorReset();
    CPLPushErrorHandler(CPLQuietErrorHandler);
    CPLLoadConfigOptionsFromFile("/vsimem/credentials.txt", false);
    CPLPopErrorHandler();
    ASSERT_EQ(CPLGetLastErrorType(), CE_Warning);

    VSIUnlink("/vsimem/credentials.txt");
}

// Test CPLLoadConfigOptionsFromFile() for VSI credentials, warning case
TEST_F(test_cpl,
       CPLLoadConfigOptionsFromFile_VSI_credentials_warning_path_specific)
{
    VSILFILE *fp = VSIFOpenL("/vsimem/credentials.txt", "wb");
    VSIFPrintfL(fp, "[credentials]\n");
    VSIFPrintfL(fp, "[.subsection]\n");
    VSIFPrintfL(fp, "path=/vsi_test/foo\n");
    VSIFPrintfL(fp, "path=/vsi_test/bar\n");  // duplicated path
    VSIFPrintfL(fp, "FOO=BAR\n");             // first key is not 'path'
    VSIFPrintfL(fp, "[unrelated_section]");
    VSIFPrintfL(fp, "BAR=BAZ\n");  // first key is not 'path'
    VSIFCloseL(fp);

    CPLErrorReset();
    CPLPushErrorHandler(CPLQuietErrorHandler);
    CPLLoadConfigOptionsFromFile("/vsimem/credentials.txt", false);
    CPLPopErrorHandler();
    ASSERT_EQ(CPLGetLastErrorType(), CE_Warning);

    {
        const char *pszVal =
            VSIGetPathSpecificOption("/vsi_test/foo", "FOO", nullptr);
        ASSERT_TRUE(pszVal != nullptr);
    }

    {
        const char *pszVal =
            VSIGetPathSpecificOption("/vsi_test/foo", "BAR", nullptr);
        ASSERT_TRUE(pszVal == nullptr);
    }

    VSIUnlink("/vsimem/credentials.txt");
}

// Test CPLRecodeFromWCharIconv() with 2 bytes/char source encoding
TEST_F(test_cpl, CPLRecodeFromWCharIconv_2byte_source_encoding)
{
#ifdef CPL_RECODE_ICONV
    int N = 2048;
    wchar_t *pszIn =
        static_cast<wchar_t *>(CPLMalloc((N + 1) * sizeof(wchar_t)));
    for (int i = 0; i < N; i++)
        pszIn[i] = L'A';
    pszIn[N] = L'\0';
    char *pszExpected = static_cast<char *>(CPLMalloc(N + 1));
    for (int i = 0; i < N; i++)
        pszExpected[i] = 'A';
    pszExpected[N] = '\0';
    char *pszRet = CPLRecodeFromWChar(pszIn, CPL_ENC_UTF16, CPL_ENC_UTF8);
    const bool bOK = memcmp(pszExpected, pszRet, N + 1) == 0;
    // FIXME Some tests fail on Mac. Not sure why, but do not error out just for
    // that
    if (!bOK &&
        (strstr(CPLGetConfigOption("TRAVIS_OS_NAME", ""), "osx") != nullptr ||
         strstr(CPLGetConfigOption("BUILD_NAME", ""), "osx") != nullptr ||
         getenv("DO_NOT_FAIL_ON_RECODE_ERRORS") != nullptr))
    {
        fprintf(stderr, "Recode from CPL_ENC_UTF16 to CPL_ENC_UTF8 failed\n");
    }
    else
    {
        EXPECT_TRUE(bOK);
    }
    CPLFree(pszIn);
    CPLFree(pszRet);
    CPLFree(pszExpected);
#else
    GTEST_SKIP() << "iconv support missing";
#endif
}

// VERY MINIMAL testing of VSI plugin functionality
TEST_F(test_cpl, VSI_plugin_minimal_testing)
{
    auto psCallbacks = VSIAllocFilesystemPluginCallbacksStruct();
    psCallbacks->open = [](void *pUserData, const char *pszFilename,
                           const char *pszAccess) -> void *
    {
        (void)pUserData;
        if (strcmp(pszFilename, "test") == 0 && strcmp(pszAccess, "rb") == 0)
            return const_cast<char *>("ok");
        return nullptr;
    };
    EXPECT_EQ(VSIInstallPluginHandler("/vsimyplugin/", psCallbacks), 0);
    VSIFreeFilesystemPluginCallbacksStruct(psCallbacks);
    VSILFILE *fp = VSIFOpenL("/vsimyplugin/test", "rb");
    EXPECT_TRUE(fp != nullptr);

    // Check it doesn't crash
    vsi_l_offset nOffset = 5;
    size_t nSize = 10;
    reinterpret_cast<VSIVirtualHandle *>(fp)->AdviseRead(1, &nOffset, &nSize);

    VSIFCloseL(fp);
    EXPECT_TRUE(VSIFOpenL("/vsimyplugin/i_dont_exist", "rb") == nullptr);
}

TEST_F(test_cpl, VSI_plugin_advise_read)
{
    auto psCallbacks = VSIAllocFilesystemPluginCallbacksStruct();

    struct UserData
    {
        int nRanges = 0;
        const vsi_l_offset *panOffsets = nullptr;
        const size_t *panSizes = nullptr;
    };
    UserData userData;

    psCallbacks->pUserData = &userData;
    psCallbacks->open = [](void *pUserData, const char * /*pszFilename*/,
                           const char * /*pszAccess*/) -> void *
    { return pUserData; };

    psCallbacks->advise_read = [](void *pFile, int nRanges,
                                  const vsi_l_offset *panOffsets,
                                  const size_t *panSizes)
    {
        static_cast<UserData *>(pFile)->nRanges = nRanges;
        static_cast<UserData *>(pFile)->panOffsets = panOffsets;
        static_cast<UserData *>(pFile)->panSizes = panSizes;
    };
    EXPECT_EQ(VSIInstallPluginHandler("/VSI_plugin_advise_read/", psCallbacks),
              0);
    VSIFreeFilesystemPluginCallbacksStruct(psCallbacks);
    VSILFILE *fp = VSIFOpenL("/VSI_plugin_advise_read/test", "rb");
    EXPECT_TRUE(fp != nullptr);

    vsi_l_offset nOffset = 5;
    size_t nSize = 10;
    reinterpret_cast<VSIVirtualHandle *>(fp)->AdviseRead(1, &nOffset, &nSize);
    EXPECT_EQ(userData.nRanges, 1);
    EXPECT_EQ(userData.panOffsets, &nOffset);
    EXPECT_EQ(userData.panSizes, &nSize);

    VSIFCloseL(fp);
}

// Test CPLIsASCII()
TEST_F(test_cpl, CPLIsASCII)
{
    ASSERT_TRUE(CPLIsASCII("foo", 3));
    ASSERT_TRUE(CPLIsASCII("foo", static_cast<size_t>(-1)));
    ASSERT_TRUE(!CPLIsASCII("\xFF", 1));
}

// Test VSIIsLocal()
TEST_F(test_cpl, VSIIsLocal)
{
    ASSERT_TRUE(VSIIsLocal("/vsimem/"));
    ASSERT_TRUE(VSIIsLocal("/vsigzip//vsimem/tmp.gz"));
#ifdef HAVE_CURL
    ASSERT_TRUE(!VSIIsLocal("/vsicurl/http://example.com"));
#endif
    VSIStatBufL sStat;
#ifdef _WIN32
    if (VSIStatL("c:\\", &sStat) == 0)
    {
        ASSERT_TRUE(VSIIsLocal("c:\\i_do_not_exist"));
    }
#else
    if (VSIStatL("/tmp", &sStat) == 0)
    {
        ASSERT_TRUE(VSIIsLocal("/tmp/i_do_not_exist"));
    }
#endif
}

// Test VSISupportsSequentialWrite()
TEST_F(test_cpl, VSISupportsSequentialWrite)
{
    ASSERT_TRUE(VSISupportsSequentialWrite("/vsimem/", false));
#ifdef HAVE_CURL
    ASSERT_TRUE(
        !VSISupportsSequentialWrite("/vsicurl/http://example.com", false));
    ASSERT_TRUE(VSISupportsSequentialWrite("/vsis3/test_bucket/", false));
#endif
    ASSERT_TRUE(VSISupportsSequentialWrite("/vsigzip//vsimem/tmp.gz", false));
#ifdef HAVE_CURL
    ASSERT_TRUE(!VSISupportsSequentialWrite(
        "/vsigzip//vsicurl/http://example.com/tmp.gz", false));
#endif
    VSIStatBufL sStat;
#ifdef _WIN32
    if (VSIStatL("c:\\", &sStat) == 0)
    {
        ASSERT_TRUE(VSISupportsSequentialWrite("c:\\", false));
    }
#else
    if (VSIStatL("/tmp", &sStat) == 0)
    {
        ASSERT_TRUE(VSISupportsSequentialWrite("/tmp/i_do_not_exist", false));
    }
#endif
}

// Test VSISupportsRandomWrite()
TEST_F(test_cpl, VSISupportsRandomWrite)
{
    ASSERT_TRUE(VSISupportsRandomWrite("/vsimem/", false));
#ifdef HAVE_CURL
    ASSERT_TRUE(!VSISupportsRandomWrite("/vsicurl/http://example.com", false));
    ASSERT_TRUE(!VSISupportsRandomWrite("/vsis3/test_bucket/", false));
    ASSERT_TRUE(!VSISupportsRandomWrite("/vsis3/test_bucket/", true));
    CPLSetConfigOption("CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE", "YES");
    ASSERT_TRUE(VSISupportsRandomWrite("/vsis3/test_bucket/", true));
    CPLSetConfigOption("CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE", nullptr);
#endif
    ASSERT_TRUE(!VSISupportsRandomWrite("/vsigzip//vsimem/tmp.gz", false));
#ifdef HAVE_CURL
    ASSERT_TRUE(!VSISupportsRandomWrite(
        "/vsigzip//vsicurl/http://example.com/tmp.gz", false));
#endif
    VSIStatBufL sStat;
#ifdef _WIN32
    if (VSIStatL("c:\\", &sStat) == 0)
    {
        ASSERT_TRUE(VSISupportsRandomWrite("c:\\", false));
    }
#else
    if (VSIStatL("/tmp", &sStat) == 0)
    {
        ASSERT_TRUE(VSISupportsRandomWrite("/tmp", false));
    }
#endif
}

// Test ignore-env-vars = yes of configuration file
TEST_F(test_cpl, config_file_ignore_env_vars)
{
    char szEnvVar[] = "SOME_ENV_VAR_FOR_TEST_CPL_61=FOO";
    putenv(szEnvVar);
    ASSERT_STREQ(CPLGetConfigOption("SOME_ENV_VAR_FOR_TEST_CPL_61", nullptr),
                 "FOO");

    VSILFILE *fp = VSIFOpenL("/vsimem/.gdal/gdalrc", "wb");
    VSIFPrintfL(fp, "[directives]\n");
    VSIFPrintfL(fp, "ignore-env-vars=yes\n");
    VSIFPrintfL(fp, "[configoptions]\n");
    VSIFPrintfL(fp, "CONFIG_OPTION_FOR_TEST_CPL_61=BAR\n");
    VSIFCloseL(fp);

    // Load configuration file
    constexpr bool bOverrideEnvVars = false;
    CPLLoadConfigOptionsFromFile("/vsimem/.gdal/gdalrc", bOverrideEnvVars);

    // Check that reading configuration option works
    ASSERT_STREQ(CPLGetConfigOption("CONFIG_OPTION_FOR_TEST_CPL_61", ""),
                 "BAR");

    // Check that environment variables are not read as configuration options
    ASSERT_TRUE(CPLGetConfigOption("SOME_ENV_VAR_FOR_TEST_CPL_61", nullptr) ==
                nullptr);

    // Reset ignore-env-vars=no
    fp = VSIFOpenL("/vsimem/.gdal/gdalrc", "wb");
    VSIFPrintfL(fp, "[directives]\n");
    VSIFPrintfL(fp, "ignore-env-vars=no\n");
    VSIFPrintfL(fp, "[configoptions]\n");
    VSIFPrintfL(fp, "SOME_ENV_VAR_FOR_TEST_CPL_61=BAR\n");
    VSIFCloseL(fp);

    // Reload configuration file
    CPLLoadConfigOptionsFromFile("/vsimem/.gdal/gdalrc", false);

    // Check that environment variables override configuration options defined
    // in the file (config file was loaded with bOverrideEnvVars = false)
    ASSERT_TRUE(CPLGetConfigOption("SOME_ENV_VAR_FOR_TEST_CPL_61", nullptr) !=
                nullptr);
    ASSERT_STREQ(CPLGetConfigOption("SOME_ENV_VAR_FOR_TEST_CPL_61", ""), "FOO");

    VSIUnlink("/vsimem/.gdal/gdalrc");
}

// Test that explicitly defined configuration options override environment variables
// with the same name
TEST_F(test_cpl, test_config_overrides_environment)
{
    char szEnvVar[] = "TEST_CONFIG_OVERRIDES_ENVIRONMENT=123";
    putenv(szEnvVar);

    ASSERT_STREQ(
        CPLGetConfigOption("TEST_CONFIG_OVERRIDES_ENVIRONMENT", nullptr),
        "123");

    CPLSetConfigOption("TEST_CONFIG_OVERRIDES_ENVIRONMENT", "456");

    ASSERT_STREQ(
        CPLGetConfigOption("TEST_CONFIG_OVERRIDES_ENVIRONMENT", nullptr),
        "456");

    CPLSetConfigOption("TEST_CONFIG_OVERRIDES_ENVIRONMENT", nullptr);

    ASSERT_STREQ(
        CPLGetConfigOption("TEST_CONFIG_OVERRIDES_ENVIRONMENT", nullptr),
        "123");
}

// Test CPLWorkerThreadPool recursion
TEST_F(test_cpl, CPLWorkerThreadPool_recursion)
{
    struct Context
    {
        CPLWorkerThreadPool oThreadPool{};
        std::atomic<int> nCounter{0};
        std::mutex mutex{};
        std::condition_variable cv{};
        bool you_can_leave = false;
        int threadStarted = 0;
    };
    Context ctxt;
    ctxt.oThreadPool.Setup(2, nullptr, nullptr, /* waitAllStarted = */ true);

    struct Data
    {
        Context *psCtxt;
        int iJob;
        GIntBig nThreadLambda = 0;

        Data(Context *psCtxtIn, int iJobIn) : psCtxt(psCtxtIn), iJob(iJobIn)
        {
        }
        Data(const Data &) = default;
    };
    const auto lambda = [](void *pData)
    {
        auto psData = static_cast<Data *>(pData);
        if (psData->iJob > 0)
        {
            // wait for both threads to be started
            std::unique_lock<std::mutex> guard(psData->psCtxt->mutex);
            psData->psCtxt->threadStarted++;
            psData->psCtxt->cv.notify_one();
            while (psData->psCtxt->threadStarted < 2)
            {
                psData->psCtxt->cv.wait(guard);
            }
        }

        psData->nThreadLambda = CPLGetPID();
        // fprintf(stderr, "lambda %d: " CPL_FRMT_GIB "\n",
        //         psData->iJob, psData->nThreadLambda);
        const auto lambda2 = [](void *pData2)
        {
            const auto psData2 = static_cast<Data *>(pData2);
            const int iJob = psData2->iJob;
            const int nCounter = psData2->psCtxt->nCounter++;
            CPL_IGNORE_RET_VAL(nCounter);
            const auto nThreadLambda2 = CPLGetPID();
            // fprintf(stderr, "lambda2 job=%d, counter(before)=%d, thread="
            // CPL_FRMT_GIB "\n", iJob, nCounter, nThreadLambda2);
            if (iJob == 100 + 0)
            {
                ASSERT_TRUE(nThreadLambda2 != psData2->nThreadLambda);
                // make sure that job 0 run in the other thread
                // takes sufficiently long that job 2 has been submitted
                // before it completes
                std::unique_lock<std::mutex> guard(psData2->psCtxt->mutex);
                while (!psData2->psCtxt->you_can_leave)
                {
                    psData2->psCtxt->cv.wait(guard);
                }
            }
            else if (iJob == 100 + 1 || iJob == 100 + 2)
            {
                ASSERT_TRUE(nThreadLambda2 == psData2->nThreadLambda);
            }
        };
        auto poQueue = psData->psCtxt->oThreadPool.CreateJobQueue();
        Data d0(*psData);
        d0.iJob = 100 + d0.iJob * 3 + 0;
        Data d1(*psData);
        d1.iJob = 100 + d1.iJob * 3 + 1;
        Data d2(*psData);
        d2.iJob = 100 + d2.iJob * 3 + 2;
        poQueue->SubmitJob(lambda2, &d0);
        poQueue->SubmitJob(lambda2, &d1);
        poQueue->SubmitJob(lambda2, &d2);
        if (psData->iJob == 0)
        {
            std::lock_guard<std::mutex> guard(psData->psCtxt->mutex);
            psData->psCtxt->you_can_leave = true;
            psData->psCtxt->cv.notify_one();
        }
    };
    {
        auto poQueue = ctxt.oThreadPool.CreateJobQueue();
        Data data0(&ctxt, 0);
        poQueue->SubmitJob(lambda, &data0);
    }
    {
        auto poQueue = ctxt.oThreadPool.CreateJobQueue();
        Data data1(&ctxt, 1);
        Data data2(&ctxt, 2);
        poQueue->SubmitJob(lambda, &data1);
        poQueue->SubmitJob(lambda, &data2);
    }
    ASSERT_EQ(ctxt.nCounter, 3 * 3);
}

// Test /vsimem/ PRead() implementation
TEST_F(test_cpl, vsimem_pread)
{
    char szContent[] = "abcd";
    VSILFILE *fp = VSIFileFromMemBuffer(
        "", reinterpret_cast<GByte *>(szContent), 4, FALSE);
    VSIVirtualHandle *poHandle = reinterpret_cast<VSIVirtualHandle *>(fp);
    ASSERT_TRUE(poHandle->HasPRead());
    {
        char szBuffer[5] = {0};
        ASSERT_EQ(poHandle->PRead(szBuffer, 2, 1), 2U);
        ASSERT_EQ(std::string(szBuffer), std::string("bc"));
    }
    {
        char szBuffer[5] = {0};
        ASSERT_EQ(poHandle->PRead(szBuffer, 4, 1), 3U);
        ASSERT_EQ(std::string(szBuffer), std::string("bcd"));
    }
    {
        char szBuffer[5] = {0};
        ASSERT_EQ(poHandle->PRead(szBuffer, 1, 4), 0U);
        ASSERT_EQ(std::string(szBuffer), std::string());
    }
    VSIFCloseL(fp);
}

// Test regular file system PRead() implementation
TEST_F(test_cpl, file_system_pread)
{
    VSILFILE *fp = VSIFOpenL("temp_test_64.bin", "wb+");
    if (fp == nullptr)
        return;
    VSIVirtualHandle *poHandle = reinterpret_cast<VSIVirtualHandle *>(fp);
    poHandle->Write("abcd", 4, 1);
    if (poHandle->HasPRead())
    {
        poHandle->Flush();
        {
            char szBuffer[5] = {0};
            ASSERT_EQ(poHandle->PRead(szBuffer, 2, 1), 2U);
            ASSERT_EQ(std::string(szBuffer), std::string("bc"));
        }
        {
            char szBuffer[5] = {0};
            ASSERT_EQ(poHandle->PRead(szBuffer, 4, 1), 3U);
            ASSERT_EQ(std::string(szBuffer), std::string("bcd"));
        }
        {
            char szBuffer[5] = {0};
            ASSERT_EQ(poHandle->PRead(szBuffer, 1, 4), 0U);
            ASSERT_EQ(std::string(szBuffer), std::string());
        }
    }
    VSIFCloseL(fp);
    VSIUnlink("temp_test_64.bin");
}

// Test CPLMask implementation
TEST_F(test_cpl, CPLMask)
{
    constexpr std::size_t sz = 71;
    auto m = CPLMaskCreate(sz, true);

    // Mask is set by default
    for (std::size_t i = 0; i < sz; i++)
    {
        EXPECT_EQ(CPLMaskGet(m, i), true) << "bit " << i;
    }

    VSIFree(m);
    m = CPLMaskCreate(sz, false);
    auto m2 = CPLMaskCreate(sz, false);

    // Mask is unset by default
    for (std::size_t i = 0; i < sz; i++)
    {
        EXPECT_EQ(CPLMaskGet(m, i), false) << "bit " << i;
    }

    // Set a few bits
    CPLMaskSet(m, 10);
    CPLMaskSet(m, 33);
    CPLMaskSet(m, 70);

    // Check all bits
    for (std::size_t i = 0; i < sz; i++)
    {
        if (i == 10 || i == 33 || i == 70)
        {
            EXPECT_EQ(CPLMaskGet(m, i), true) << "bit " << i;
        }
        else
        {
            EXPECT_EQ(CPLMaskGet(m, i), false) << "bit " << i;
        }
    }

    // Unset some bits
    CPLMaskClear(m, 10);
    CPLMaskClear(m, 70);

    // Check all bits
    for (std::size_t i = 0; i < sz; i++)
    {
        if (i == 33)
        {
            EXPECT_EQ(CPLMaskGet(m, i), true) << "bit " << i;
        }
        else
        {
            EXPECT_EQ(CPLMaskGet(m, i), false) << "bit " << i;
        }
    }

    CPLMaskSet(m2, 36);
    CPLMaskMerge(m2, m, sz);

    // Check all bits
    for (std::size_t i = 0; i < sz; i++)
    {
        if (i == 36 || i == 33)
        {
            ASSERT_EQ(CPLMaskGet(m2, i), true) << "bit " << i;
        }
        else
        {
            ASSERT_EQ(CPLMaskGet(m2, i), false) << "bit " << i;
        }
    }

    CPLMaskClearAll(m, sz);
    CPLMaskSetAll(m2, sz);

    // Check all bits
    for (std::size_t i = 0; i < sz; i++)
    {
        EXPECT_EQ(CPLMaskGet(m, i), false) << "bit " << i;
        EXPECT_EQ(CPLMaskGet(m2, i), true) << "bit " << i;
    }

    VSIFree(m);
    VSIFree(m2);
}

// Test cpl::ThreadSafeQueue
TEST_F(test_cpl, ThreadSafeQueue)
{
    cpl::ThreadSafeQueue<int> queue;
    ASSERT_TRUE(queue.empty());
    ASSERT_EQ(queue.size(), 0U);
    queue.push(1);
    ASSERT_TRUE(!queue.empty());
    ASSERT_EQ(queue.size(), 1U);
    queue.clear();
    ASSERT_TRUE(queue.empty());
    ASSERT_EQ(queue.size(), 0U);
    int val = 10;
    queue.push(std::move(val));
    ASSERT_TRUE(!queue.empty());
    ASSERT_EQ(queue.size(), 1U);
    ASSERT_EQ(queue.get_and_pop_front(), 10);
    ASSERT_TRUE(queue.empty());
}

TEST_F(test_cpl, CPLGetExecPath)
{
    std::vector<char> achBuffer(1024, 'x');
    if (!CPLGetExecPath(achBuffer.data(), static_cast<int>(achBuffer.size())))
    {
        GTEST_SKIP() << "CPLGetExecPath() not implemented for this platform";
        return;
    }

    bool bFoundNulTerminatedChar = false;
    for (char ch : achBuffer)
    {
        if (ch == '\0')
        {
            bFoundNulTerminatedChar = true;
            break;
        }
    }
    ASSERT_TRUE(bFoundNulTerminatedChar);

    // Check that the file exists
    VSIStatBufL sStat;
    EXPECT_EQ(VSIStatL(achBuffer.data(), &sStat), 0);

    const std::string osStrBefore(achBuffer.data());

    // Resize the buffer to just the minimum size
    achBuffer.resize(strlen(achBuffer.data()) + 1);
    EXPECT_TRUE(
        CPLGetExecPath(achBuffer.data(), static_cast<int>(achBuffer.size())));

    EXPECT_STREQ(osStrBefore.c_str(), achBuffer.data());

    // Too small buffer
    achBuffer.resize(achBuffer.size() - 1);
    EXPECT_FALSE(
        CPLGetExecPath(achBuffer.data(), static_cast<int>(achBuffer.size())));
}

TEST_F(test_cpl, VSIDuplicateFileSystemHandler)
{
    {
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        EXPECT_FALSE(VSIDuplicateFileSystemHandler(
            "/vsi_i_dont_exist/", "/vsi_i_will_not_be_created/"));
    }
    {
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        EXPECT_FALSE(
            VSIDuplicateFileSystemHandler("/", "/vsi_i_will_not_be_created/"));
    }
    EXPECT_EQ(VSIFileManager::GetHandler("/vsi_test_clone_vsimem/"),
              VSIFileManager::GetHandler("/"));
    EXPECT_TRUE(
        VSIDuplicateFileSystemHandler("/vsimem/", "/vsi_test_clone_vsimem/"));
    EXPECT_NE(VSIFileManager::GetHandler("/vsi_test_clone_vsimem/"),
              VSIFileManager::GetHandler("/"));
    {
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        EXPECT_FALSE(VSIDuplicateFileSystemHandler("/vsimem/",
                                                   "/vsi_test_clone_vsimem/"));
    }
}

TEST_F(test_cpl, CPLAtoGIntBigEx)
{
    {
        int bOverflow = 0;
        EXPECT_EQ(CPLAtoGIntBigEx("9223372036854775807", false, &bOverflow),
                  std::numeric_limits<int64_t>::max());
        EXPECT_EQ(bOverflow, FALSE);
    }
    {
        int bOverflow = 0;
        EXPECT_EQ(CPLAtoGIntBigEx("9223372036854775808", false, &bOverflow),
                  std::numeric_limits<int64_t>::max());
        EXPECT_EQ(bOverflow, TRUE);
    }
    {
        int bOverflow = 0;
        EXPECT_EQ(CPLAtoGIntBigEx("-9223372036854775808", false, &bOverflow),
                  std::numeric_limits<int64_t>::min());
        EXPECT_EQ(bOverflow, FALSE);
    }
    {
        int bOverflow = 0;
        EXPECT_EQ(CPLAtoGIntBigEx("-9223372036854775809", false, &bOverflow),
                  std::numeric_limits<int64_t>::min());
        EXPECT_EQ(bOverflow, TRUE);
    }
}

TEST_F(test_cpl, CPLSubscribeToSetConfigOption)
{
    struct Event
    {
        std::string osKey;
        std::string osValue;
        bool bThreadLocal;
    };
    std::vector<Event> events;
    const auto cbk = +[](const char *pszKey, const char *pszValue,
                         bool bThreadLocal, void *pUserData)
    {
        std::vector<Event> *pEvents =
            static_cast<std::vector<Event> *>(pUserData);
        Event ev;
        ev.osKey = pszKey;
        ev.osValue = pszValue ? pszValue : "";
        ev.bThreadLocal = bThreadLocal;
        pEvents->emplace_back(ev);
    };

    // Subscribe and unsubscribe immediately
    {
        int nId = CPLSubscribeToSetConfigOption(cbk, &events);
        CPLSetConfigOption("CPLSubscribeToSetConfigOption", "bar");
        EXPECT_EQ(events.size(), 1U);
        if (!events.empty())
        {
            EXPECT_STREQ(events[0].osKey.c_str(),
                         "CPLSubscribeToSetConfigOption");
            EXPECT_STREQ(events[0].osValue.c_str(), "bar");
            EXPECT_FALSE(events[0].bThreadLocal);
        }
        CPLUnsubscribeToSetConfigOption(nId);
    }
    events.clear();

    // Subscribe and unsubscribe in non-nested order
    {
        int nId1 = CPLSubscribeToSetConfigOption(cbk, &events);
        int nId2 = CPLSubscribeToSetConfigOption(cbk, &events);
        CPLUnsubscribeToSetConfigOption(nId1);
        int nId3 = CPLSubscribeToSetConfigOption(cbk, &events);

        CPLSetConfigOption("CPLSubscribeToSetConfigOption", nullptr);
        EXPECT_EQ(events.size(), 2U);

        CPLUnsubscribeToSetConfigOption(nId2);
        CPLUnsubscribeToSetConfigOption(nId3);

        CPLSetConfigOption("CPLSubscribeToSetConfigOption", nullptr);
        EXPECT_EQ(events.size(), 2U);
    }
}

}  // namespace
