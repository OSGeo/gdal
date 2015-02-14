#include <assert.h>

#include "cpl_virtualmem.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "gdal.h"
#include "gdal_alg.h"

#ifdef notdef
static void test_huge_mapping_cbk(CPLVirtualMem* ctxt, 
                  size_t nOffset,
                  void* pPageToFill,
                  size_t nPageSize,
                  void* pUserData)
{
    /*fprintfstderr("requesting page %lu (nPageSize=%d), nLRUSize=%d\n",
            (unsigned long)(nOffset / nPageSize),
            (int)nPageSize,
            ctxt->nLRUSize);*/
    memset(pPageToFill, 0x7F, nPageSize);
}

static void test_huge_mapping()
{
    CPLVirtualMem* ctxt;
    char* addr;
    int i;

    ctxt = CPLVirtualMemNew((size_t)10000*1024*1024,
                               (size_t)2000*1024*1024,
                               0,
                               TRUE,
                               VIRTUALMEM_READONLY,
                               test_huge_mapping_cbk, NULL, NULL, NULL);
    assert(ctxt);
    addr = (char*) CPLVirtualMemGetAddr(ctxt);
    for(i=0;i<50*1000;i++)
    {
        unsigned int seedp;
        size_t idx = (size_t)rand_r(&seedp)*3000*1024 / RAND_MAX * 1024;
        char val = addr[idx];
        /*printf("i=%d, val[%ld] = %d\n", i, (long int)idx, val);*/
        assert(val == 0x7F);
    }
    CPLVirtualMemFree(ctxt);
}
#endif

static void test_two_pages_cbk(CPLVirtualMem* ctxt, 
                  size_t nOffset,
                  void* pPageToFill,
                  size_t nPageSize,
                  void* pUserData)
{
    /*fprintfstderr("requesting page %lu (nPageSize=%d), nLRUSize=%d\n",
            (unsigned long)(nOffset / nPageSize),
            (int)nPageSize,
            ctxt->nLRUSize);*/
    memset(pPageToFill, (nOffset == 0) ? 0x3F : (nOffset == 4096) ? 0x5F : 0x7F, nPageSize);
}

#define MINIMUM_PAGE_SIZE 4096

static void test_two_pages_thread(void* p)
{
    CPLVirtualMem* ctxt = (CPLVirtualMem*)p;
    char* addr = (char*) CPLVirtualMemGetAddr(ctxt);
    int i;
    CPLVirtualMemDeclareThread(ctxt);
    /*fprintfstderr("aux thread is %X\n", pthread_self());*/

    for(i=0;i<50*1000;i++)
    {
        char val = addr[MINIMUM_PAGE_SIZE * (i % 3) + MINIMUM_PAGE_SIZE/2 - 1];
        /*fprintfstderr("T2: val[%d] = %d\n", MINIMUM_PAGE_SIZE * (i % 2) + MINIMUM_PAGE_SIZE/2 - 1, val);*/
        assert(val == (((i % 3) == 0) ? 0x3F : ((i % 3) == 1) ? 0x5F : 0x7F));
    }
    CPLVirtualMemUnDeclareThread(ctxt);
}

static int test_two_pages()
{
    CPLVirtualMem* ctxt;
    volatile char* addr;
    void* hThread;

    ctxt = CPLVirtualMemNew(3*MINIMUM_PAGE_SIZE,
                        MINIMUM_PAGE_SIZE,
                        MINIMUM_PAGE_SIZE,
                        FALSE,
                        VIRTUALMEM_READONLY,
                        test_two_pages_cbk,
                        NULL,
                        NULL, NULL);
    if( ctxt == NULL )
        return FALSE;

    addr = (char*) CPLVirtualMemGetAddr(ctxt);
    assert(CPLVirtualMemGetPageSize(ctxt) == MINIMUM_PAGE_SIZE);
    assert(CPLVirtualMemIsAccessThreadSafe(ctxt));
    /*fprintfstderr("main thread is %X, addr=%p\n", pthread_self(), addr);*/
    hThread = CPLCreateJoinableThread(test_two_pages_thread, ctxt);
    CPLVirtualMemDeclareThread(ctxt);
    {
        int i=0;
        for(i=0;i<50*1000;i++)
        {
            char val = addr[MINIMUM_PAGE_SIZE * (i % 3)];
            /*fprintfstderr("T1: val[%d] = %d\n", MINIMUM_PAGE_SIZE * (i % 2), val);*/
            assert(val == (((i % 3) == 0) ? 0x3F : ((i % 3) == 1) ? 0x5F : 0x7F));
        }
    }
    CPLVirtualMemUnDeclareThread(ctxt);
    CPLJoinThread(hThread);
    CPLVirtualMemFree(ctxt);
    
    return TRUE;
}

static void test_raw_auto(int bFileMapping)
{
    GDALAllRegister();
    
    CPLString osTmpFile;
    
    if( bFileMapping )
        osTmpFile = CPLResetExtension(CPLGenerateTempFilename("ehdr"), "img");
    else
        osTmpFile = "/vsimem/tmp.img";
    GDALDatasetH hDS = GDALCreate(GDALGetDriverByName("EHdr"),
                                  osTmpFile.c_str(),
                                  400, 300, 2, GDT_Byte, NULL );
    assert(hDS);

    int nPixelSpace1;
    GIntBig nLineSpace1;
    int nPixelSpace2;
    GIntBig nLineSpace2;
    CPLVirtualMem* pVMem1 = GDALGetVirtualMemAuto(GDALGetRasterBand(hDS, 1),
                                                  GF_Write,
                                                  &nPixelSpace1,
                                                  &nLineSpace1,
                                                  NULL);
    CPLVirtualMem* pVMem2 = GDALGetVirtualMemAuto(GDALGetRasterBand(hDS, 2),
                                                  GF_Write,
                                                  &nPixelSpace2,
                                                  &nLineSpace2,
                                                  NULL);
    assert(pVMem1 != NULL);
    assert(pVMem2 != NULL);
    assert(CPLVirtualMemIsFileMapping(pVMem1) == bFileMapping);
    assert(nPixelSpace1 == 1);
    if( bFileMapping )
        assert(nLineSpace1 == 400 * 2);
    else
        assert(nLineSpace1 == 400);

    GByte* pBase1 = (GByte*) CPLVirtualMemGetAddr(pVMem1);
    GByte* pBase2 = (GByte*) CPLVirtualMemGetAddr(pVMem2);
    for(int j=0;j<300;j++)
    {
        for(int i=0;i<400;i++)
        {
            pBase1[j * nLineSpace1 + i * nPixelSpace1] = 127;
            pBase2[j * nLineSpace2 + i * nPixelSpace2] = 255;
        }
    }

    CPLVirtualMemFree(pVMem1);
    CPLVirtualMemFree(pVMem2);
    GDALClose(hDS);
    
    hDS = GDALOpen(osTmpFile.c_str(), GA_ReadOnly);
    assert(GDALChecksumImage(GDALGetRasterBand(hDS, 1), 0, 0, 400, 300) == 52906);
    assert(GDALChecksumImage(GDALGetRasterBand(hDS, 2), 0, 0, 400, 300) == 30926);
    GDALClose(hDS);
    
    GDALDeleteDataset(NULL, osTmpFile.c_str());

}

int main(int argc, char* argv[])
{
    /*printf("test_huge_mapping\n");
    test_huge_mapping();*/
    
    printf("Physical memory : " CPL_FRMT_GIB " bytes\n", CPLGetPhysicalRAM());

    if( !test_two_pages() )
        return 0;

    test_raw_auto(TRUE);
    test_raw_auto(FALSE);

    CPLVirtualMemManagerTerminate();
    GDALDestroyDriverManager();

    return 0;
}
