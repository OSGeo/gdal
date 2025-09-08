/******************************************************************************
 *
 * Project:  GDAL algorithms
 * Purpose:  Test Delaunay triangulation
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_virtualmem.h"
#include "gdal_alg.h"
#include "gdal.h"

#include "gtest_include.h"

#include "test_data.h"

namespace
{

// ---------------------------------------------------------------------------

#ifdef notdef
static void test_huge_mapping_cbk(CPLVirtualMem *ctxt, size_t nOffset,
                                  void *pPageToFill, size_t nPageSize,
                                  void *pUserData)
{
    /*fprintfstderr("requesting page %lu (nPageSize=%d), nLRUSize=%d\n",
            (unsigned long)(nOffset / nPageSize),
            (int)nPageSize,
            ctxt->nLRUSize);*/
    memset(pPageToFill, 0x7F, nPageSize);
}

static void test_huge_mapping()
{
    CPLVirtualMem *ctxt;
    char *addr;
    int i;

    ctxt = CPLVirtualMemNew(
        (size_t)10000 * 1024 * 1024, (size_t)2000 * 1024 * 1024, 0, TRUE,
        VIRTUALMEM_READONLY, test_huge_mapping_cbk, NULL, NULL, NULL);
    ASSERT_TRUE(ctxt != nullptr);
    addr = (char *)CPLVirtualMemGetAddr(ctxt);
    for (i = 0; i < 50 * 1000; i++)
    {
        unsigned int seedp;
        size_t idx = (size_t)rand_r(&seedp) * 3000 * 1024 / RAND_MAX * 1024;
        char val = addr[idx];
        /*printf("i=%d, val[%ld] = %d\n", i, (long int)idx, val);*/
        ASSERT_EQ(val, 0x7F);
    }
    CPLVirtualMemFree(ctxt);
}
#endif

static void test_two_pages_cbk(CPLVirtualMem * /* ctxt */, size_t nOffset,
                               void *pPageToFill, size_t nPageSize,
                               void * /* pUserData */)
{
    /*fprintfstderr("requesting page %lu (nPageSize=%d), nLRUSize=%d\n",
            (unsigned long)(nOffset / nPageSize),
            (int)nPageSize,
            ctxt->nLRUSize);*/
    memset(pPageToFill,
           (nOffset == 0)      ? 0x3F
           : (nOffset == 4096) ? 0x5F
                               : 0x7F,
           nPageSize);
}

#define MINIMUM_PAGE_SIZE 4096

static void test_two_pages_thread(void *p)
{
    CPLVirtualMem *ctxt = (CPLVirtualMem *)p;
    char *addr = (char *)CPLVirtualMemGetAddr(ctxt);
    int i;
    CPLVirtualMemDeclareThread(ctxt);
    /*fprintfstderr("aux thread is %X\n", pthread_self());*/

    for (i = 0; i < 50 * 1000; i++)
    {
        char val =
            addr[MINIMUM_PAGE_SIZE * (i % 3) + MINIMUM_PAGE_SIZE / 2 - 1];
        /*fprintfstderr("T2: val[%d] = %d\n", MINIMUM_PAGE_SIZE * (i % 2) +
         * MINIMUM_PAGE_SIZE/2 - 1, val);*/
        ASSERT_EQ(val, (((i % 3) == 0) ? 0x3F : ((i % 3) == 1) ? 0x5F : 0x7F));
    }
    CPLVirtualMemUnDeclareThread(ctxt);
}

static void test_two_pages(CPLVirtualMem *ctxt)
{
    volatile char *addr;
    CPLJoinableThread *hThread;

    printf("test_two_pages()\n");

    addr = (char *)CPLVirtualMemGetAddr(ctxt);
    ASSERT_EQ(CPLVirtualMemGetPageSize(ctxt), MINIMUM_PAGE_SIZE);
    ASSERT_TRUE(CPLVirtualMemIsAccessThreadSafe(ctxt));
    /*fprintfstderr("main thread is %X, addr=%p\n", pthread_self(), addr);*/
    hThread = CPLCreateJoinableThread(test_two_pages_thread, ctxt);
    CPLVirtualMemDeclareThread(ctxt);
    {
        int i = 0;
        for (i = 0; i < 50 * 1000; i++)
        {
            char val = addr[MINIMUM_PAGE_SIZE * (i % 3)];
            /*fprintfstderr("T1: val[%d] = %d\n", MINIMUM_PAGE_SIZE * (i % 2),
             * val);*/
            ASSERT_EQ(val, (((i % 3) == 0)   ? 0x3F
                            : ((i % 3) == 1) ? 0x5F
                                             : 0x7F));
        }
    }
    CPLVirtualMemUnDeclareThread(ctxt);
    CPLJoinThread(hThread);
    CPLVirtualMemFree(ctxt);
}

static void test_raw_auto(const char *pszFormat, int bFileMapping)
{
    GDALAllRegister();

    printf("test_raw_auto(format=%s, bFileMapping=%d)\n", pszFormat,
           bFileMapping);
    if (GDALGetDriverByName(pszFormat) == nullptr)
    {
        GTEST_SKIP() << pszFormat << " driver missing";
    }
    else
    {
        CPLString osTmpFile;

        if (bFileMapping)
            osTmpFile = CPLResetExtensionSafe(
                CPLGenerateTempFilenameSafe(pszFormat).c_str(), "img");
        else
            osTmpFile = "/vsimem/tmp.img";
        GDALDatasetH hDS =
            GDALCreate(GDALGetDriverByName(pszFormat), osTmpFile.c_str(), 400,
                       300, 2, GDT_Byte, nullptr);
        ASSERT_TRUE(hDS != nullptr);

        int nPixelSpace1;
        GIntBig nLineSpace1;
        int nPixelSpace2;
        GIntBig nLineSpace2;
        if (!bFileMapping)
        {
            char **papszOptions =
                CSLSetNameValue(nullptr, "USE_DEFAULT_IMPLEMENTATION", "NO");
            ASSERT_EQ(GDALGetVirtualMemAuto(GDALGetRasterBand(hDS, 1), GF_Write,
                                            &nPixelSpace1, &nLineSpace1,
                                            papszOptions),
                      nullptr);
            CSLDestroy(papszOptions);
        }
        CPLVirtualMem *pVMem1 =
            GDALGetVirtualMemAuto(GDALGetRasterBand(hDS, 1), GF_Write,
                                  &nPixelSpace1, &nLineSpace1, nullptr);
        char **papszOptions =
            CSLSetNameValue(nullptr, "USE_DEFAULT_IMPLEMENTATION",
                            (bFileMapping) ? "NO" : "YES");
        CPLVirtualMem *pVMem2 =
            GDALGetVirtualMemAuto(GDALGetRasterBand(hDS, 2), GF_Write,
                                  &nPixelSpace2, &nLineSpace2, papszOptions);
        CSLDestroy(papszOptions);
        ASSERT_TRUE(pVMem1 != nullptr);
        ASSERT_TRUE(pVMem2 != nullptr);
        ASSERT_EQ(CPLVirtualMemIsFileMapping(pVMem1), bFileMapping);
        ASSERT_EQ(nPixelSpace1,
                  ((EQUAL(pszFormat, "GTIFF") && bFileMapping) ? 2 : 1));
        if (bFileMapping)
            ASSERT_EQ(nLineSpace1, 400 * 2);
        else
            ASSERT_EQ(nLineSpace1, 400 * nPixelSpace1);

        GByte *pBase1 = (GByte *)CPLVirtualMemGetAddr(pVMem1);
        GByte *pBase2 = (GByte *)CPLVirtualMemGetAddr(pVMem2);
        for (int j = 0; j < 300; j++)
        {
            for (int i = 0; i < 400; i++)
            {
                pBase1[j * static_cast<int>(nLineSpace1) + i * nPixelSpace1] =
                    127;
                pBase2[j * static_cast<int>(nLineSpace2) + i * nPixelSpace2] =
                    255;
            }
        }

        CPLVirtualMemFree(pVMem1);
        CPLVirtualMemFree(pVMem2);
        GDALClose(hDS);

        hDS = GDALOpen(osTmpFile.c_str(), GA_ReadOnly);
        ASSERT_EQ(GDALChecksumImage(GDALGetRasterBand(hDS, 1), 0, 0, 400, 300),
                  52906);
        ASSERT_EQ(GDALChecksumImage(GDALGetRasterBand(hDS, 2), 0, 0, 400, 300),
                  30926);
        GDALClose(hDS);

        GDALDeleteDataset(nullptr, osTmpFile.c_str());
    }
}

TEST(testvitualmem, test)
{
    if (getenv("SKIP_TESTVIRTUALMEM") != nullptr)
    {
        fprintf(stderr, "Skipping testvitualmem\n");  // ok
        return;
    }

    /*printf("test_huge_mapping\n");
    test_huge_mapping();*/

    printf("Physical memory : " CPL_FRMT_GIB " bytes\n", CPLGetPhysicalRAM());

    if (CPLIsVirtualMemFileMapAvailable())
    {
        printf("Testing CPLVirtualMemFileMapNew()\n");
        VSILFILE *fp = VSIFOpenL(GCORE_DATA_DIR "byte.tif", "rb");
        ASSERT_TRUE(fp != nullptr);
        VSIFSeekL(fp, 0, SEEK_END);
        size_t nSize = (size_t)VSIFTellL(fp);
        VSIFSeekL(fp, 0, SEEK_SET);
        void *pRefBuf = CPLMalloc(nSize);
        VSIFReadL(pRefBuf, 1, nSize, fp);
        CPLVirtualMem *psMem = CPLVirtualMemFileMapNew(
            fp, 0, nSize, VIRTUALMEM_READONLY, nullptr, nullptr);
        ASSERT_TRUE(psMem != nullptr);
        void *pMemBuf = CPLVirtualMemGetAddr(psMem);
        ASSERT_TRUE(memcmp(pRefBuf, pMemBuf, nSize) == 0);
        CPLFree(pRefBuf);
        CPLVirtualMemFree(psMem);
        VSIFCloseL(fp);
    }

    auto ctxt = CPLVirtualMemNew(3 * MINIMUM_PAGE_SIZE, MINIMUM_PAGE_SIZE,
                                 MINIMUM_PAGE_SIZE, FALSE, VIRTUALMEM_READONLY,
                                 test_two_pages_cbk, nullptr, nullptr, nullptr);
    if (ctxt)
    {
        test_two_pages(ctxt);
        test_raw_auto("EHDR", TRUE);
        test_raw_auto("EHDR", FALSE);
        test_raw_auto("GTIFF", TRUE);
        test_raw_auto("GTIFF", FALSE);
    }

    CPLVirtualMemManagerTerminate();
    GDALDestroyDriverManager();
}

}  // namespace
