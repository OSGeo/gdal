/******************************************************************************
 * $Id$
 *
 * Project:  PDF driver
 * Purpose:  GDALDataset driver for PDF dataset.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault
 *
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

#ifndef PDFCREATECOPY_H_INCLUDED
#define PDFCREATECOPY_H_INCLUDED

#include "pdfobject.h"
#include "gdal_priv.h"
#include <vector>

#ifdef OGR_ENABLED
#include "ogr_api.h"
#endif

typedef enum
{
    COMPRESS_NONE,
    COMPRESS_DEFLATE,
    COMPRESS_JPEG,
    COMPRESS_JPEG2000,
    COMPRESS_DEFAULT
} PDFCompressMethod;

typedef struct
{
    int nLeft;
    int nRight;
    int nTop;
    int nBottom;
} PDFMargins;

/************************************************************************/
/*                          GDALPDFWriter                               */
/************************************************************************/

class GDALXRefEntry
{
    public:
        vsi_l_offset    nOffset;
        int             nGen;
        int             bFree;

        GDALXRefEntry() : nOffset(0), nGen(0), bFree(FALSE) {}
        GDALXRefEntry(vsi_l_offset nOffsetIn, int nGenIn = 0) : nOffset(nOffsetIn), nGen(nGenIn), bFree(FALSE) {}
        GDALXRefEntry(const GDALXRefEntry& oOther) : nOffset(oOther.nOffset), nGen(oOther.nGen), bFree(oOther.bFree) {}
        GDALXRefEntry& operator= (const GDALXRefEntry& oOther) { nOffset = oOther.nOffset; nGen = oOther.nGen; bFree = oOther.bFree; return *this; }
};

class GDALPDFImageDesc
{
    public:
        int          nImageId;
        double       dfXOff;
        double       dfYOff;
        double       dfXSize;
        double       dfYSize;
};

class GDALPDFLayerDesc
{
    public:
        int          nOGCId;
        int          nOCGTextId;
        int          nFeatureLayerId;
        CPLString    osLayerName;
        int          bWriteOGRAttributes;
        std::vector<int> aIds;
        std::vector<int> aIdsText;
        std::vector<int> aUserPropertiesIds;
        std::vector<CPLString> aFeatureNames;
};

class GDALPDFPageContext
{
    public:
        GDALDataset* poSrcDS;
        PDFCompressMethod eStreamCompressMethod;
        double       dfDPI;
        PDFMargins   sMargins;
        int          nPageId;
        int          nContentId;
        int          nResourcesId;
        std::vector<GDALPDFImageDesc> asImageDesc;
        std::vector<GDALPDFLayerDesc> asVectorDesc;
        int          nOCGRasterId;
};

class GDALPDFOCGDesc
{
    public:
        int          nId;
        int          nParentId;
};

class GDALPDFWriter
{
    VSILFILE* fp;
    std::vector<GDALXRefEntry> asXRefEntries;
    std::vector<int> asPageId;
    std::vector<GDALPDFOCGDesc> asOCGs;

    int nInfoId;
    int nInfoGen;
    int nPageResourceId;
    int nStructTreeRootId;
    int nCatalogId;
    int nCatalogGen;
    int nXMPId;
    int nXMPGen;
    int bInWriteObj;

    int nLastStartXRef;
    int nLastXRefSize;
    int bCanUpdate;

    GDALPDFPageContext oPageContext;

    void    Init();

    void    StartObj(int nObjectId, int nGen = 0);
    void    EndObj();
    void    WriteXRefTableAndTrailer();
    void    WritePages();
    int     WriteBlock( GDALDataset* poSrcDS,
                        int nXOff, int nYOff, int nReqXSize, int nReqYSize,
                        int nColorTableId,
                        PDFCompressMethod eCompressMethod,
                        int nPredictor,
                        int nJPEGQuality,
                        const char* pszJPEG2000_DRIVER,
                        GDALProgressFunc pfnProgress,
                        void * pProgressData );
    int     WriteMask(GDALDataset* poSrcDS,
                      int nXOff, int nYOff, int nReqXSize, int nReqYSize,
                      PDFCompressMethod eCompressMethod);
    int     WriteOCG(const char* pszLayerName, int nParentId = 0);

    int     WriteColorTable(GDALDataset* poSrcDS);

    int     AllocNewObject();

    public:
        GDALPDFWriter(VSILFILE* fpIn, int bAppend = FALSE);
       ~GDALPDFWriter();

       void Close();

       int  GetCatalogNum() { return nCatalogId; }
       int  GetCatalogGen() { return nCatalogGen; }

       int  ParseTrailerAndXRef();
       void UpdateProj(GDALDataset* poSrcDS,
                       double dfDPI,
                       GDALPDFDictionaryRW* poPageDict,
                       int nPageNum, int nPageGen);
       void UpdateInfo(GDALDataset* poSrcDS);
       void UpdateXMP (GDALDataset* poSrcDS,
                       GDALPDFDictionaryRW* poCatalogDict);

       int     WriteSRS_ISO32000(GDALDataset* poSrcDS,
                                 double dfUserUnit,
                                 const char* pszNEATLINE,
                                 PDFMargins* psMargins,
                                 int bWriteViewport);
       int     WriteSRS_OGC_BP(GDALDataset* poSrcDS,
                                double dfUserUnit,
                                const char* pszNEATLINE,
                                PDFMargins* psMargins);

       int  StartPage(GDALDataset* poSrcDS,
                      double dfDPI,
                      const char* pszGEO_ENCODING,
                      const char* pszNEATLINE,
                      PDFMargins* psMargins,
                      PDFCompressMethod eStreamCompressMethod,
                      int bHasOGRData);

       int WriteImagery(const char* pszLayerName,
                        PDFCompressMethod eCompressMethod,
                        int nPredictor,
                        int nJPEGQuality,
                        const char* pszJPEG2000_DRIVER,
                        int nBlockXSize, int nBlockYSize,
                        GDALProgressFunc pfnProgress,
                        void * pProgressData);

#ifdef OGR_ENABLED
       int WriteOGRDataSource(const char* pszOGRDataSource,
                              const char* pszOGRDisplayField,
                              const char* pszOGRDisplayLayerNames,
                              int bWriteOGRAttributes);

       GDALPDFLayerDesc StartOGRLayer(CPLString osLayerName,
                                      int bWriteOGRAttributes);
       void EndOGRLayer(GDALPDFLayerDesc& osVectorDesc);

       int WriteOGRLayer(OGRDataSourceH hDS,
                         int iLayer,
                         const char* pszOGRDisplayField,
                         CPLString osLayerName,
                         int bWriteOGRAttributes,
                         int& iObj);

       int WriteOGRFeature(GDALPDFLayerDesc& osVectorDesc,
                           OGRFeatureH hFeat,
                           const char* pszOGRDisplayField,
                           int bWriteOGRAttributes,
                           int& iObj,
                           int& iObjLayer);
#endif

       int  EndPage(const char* pszExtraImages,
                    const char* pszExtraStream,
                    const char* pszExtraLayerName);

       int  SetInfo(GDALDataset* poSrcDS,
                    char** papszOptions);
       int  SetXMP(GDALDataset* poSrcDS,
                   const char* pszXMP);
};

GDALDataset         *GDALPDFCreateCopy( const char *, GDALDataset *,
                                        int, char **,
                                        GDALProgressFunc pfnProgress,
                                        void * pProgressData );

#endif // PDFCREATECOPY_H_INCLUDED
