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

#include "pdfcreatecopy.h"

#include "cpl_vsi_virtual.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "ogr_srs_api.h"
#include <vector>

CPL_CVSID("$Id$");

typedef enum
{
    COMPRESS_NONE,
    COMPRESS_DEFLATE,
    COMPRESS_JPEG,
    COMPRESS_JPEG2000
} PDFCompressMethod;

/************************************************************************/
/*                          GDALPDFWriter                               */
/************************************************************************/

class GDALPDFWriter
{
    VSILFILE* fp;
    std::vector<vsi_l_offset> asOffsets;
    std::vector<int> asPageId;

    int nInfoId;
    int nPageResourceId;
    int nCatalogId;
    int bInWriteObj;

    void    StartObj(int nObjectId);
    void    EndObj();
    void    WriteXRefTableAndTrailer();
    void    WritePages();
    int     WriteSRS(int nWidth, int nHeight,
                     const char* pszWKT,
                     double adfGeoTransform[6]);
    int     WriteBlock( GDALDataset* poSrcDS,
                        int nXOff, int nYOff, int nReqXSize, int nReqYSize,
                        PDFCompressMethod eCompressMethod,
                        int nJPEGQuality,
                        const char* pszJPEG2000_DRIVER,
                        GDALProgressFunc pfnProgress,
                        void * pProgressData );

    public:
        GDALPDFWriter(VSILFILE* fpIn);
       ~GDALPDFWriter();

       void Close();

       int  AllocNewObject();
       int  WritePage(GDALDataset* poSrcDS,
                      PDFCompressMethod eCompressMethod,
                      int nJPEGQuality,
                      const char* pszJPEG2000_DRIVER,
                      int nBlockXSize, int nBlockYSize,
                      GDALProgressFunc pfnProgress,
                      void * pProgressData);
       void SetInfo();
};

/************************************************************************/
/*                         GDALPDFWriter()                              */
/************************************************************************/

GDALPDFWriter::GDALPDFWriter(VSILFILE* fpIn) : fp(fpIn)
{
    VSIFPrintfL(fp, "%%PDF-1.6\n");

    /* See PDF 1.7 reference, page 92. Write 4 non-ASCII bytes to indicate that the content will be binary */
    VSIFPrintfL(fp, "%%%c%c%c%c\n", 0xFF, 0xFF, 0xFF, 0xFF);

    nPageResourceId = AllocNewObject();
    nCatalogId = AllocNewObject();
    bInWriteObj = FALSE;
    nInfoId = 0;
}

/************************************************************************/
/*                         ~GDALPDFWriter()                             */
/************************************************************************/

GDALPDFWriter::~GDALPDFWriter()
{
    Close();
}

/************************************************************************/
/*                              Close()                                 */
/************************************************************************/

void GDALPDFWriter::Close()
{
    if (fp)
    {
        CPLAssert(!bInWriteObj);
        WritePages();
        WriteXRefTableAndTrailer();
        VSIFCloseL(fp);
    }
    fp = NULL;
}

/************************************************************************/
/*                           AllocNewObject()                           */
/************************************************************************/

int GDALPDFWriter::AllocNewObject()
{
    asOffsets.push_back(0);
    return (int)asOffsets.size();
}

/************************************************************************/
/*                        WriteXRefTableAndTrailer()                    */
/************************************************************************/

void GDALPDFWriter::WriteXRefTableAndTrailer()
{
    vsi_l_offset nOffsetXREF = VSIFTellL(fp);
    VSIFPrintfL(fp,
                "xref\n"
                "%d %d\n",
                0, (int)asOffsets.size() + 1);

    VSIFPrintfL(fp, "0000000000 65535 f \n");
    char buffer[16];
    for(size_t i=0;i<asOffsets.size();i++)
    {
        snprintf (buffer, sizeof(buffer), "%010ld", (long)asOffsets[i]);
        VSIFPrintfL(fp, "%s 00000 n \n", buffer);
    }

    VSIFPrintfL(fp,
                "trailer\n"
                "<< /Size %d\n"
                "   /Root %d 0 R\n",
                (int)asOffsets.size(),
                nCatalogId);
    if (nInfoId)
        VSIFPrintfL(fp, "   /Info %d 0 R\n", nInfoId);
    VSIFPrintfL(fp, ">>\n");

    VSIFPrintfL(fp,
                "startxref\n"
                "%ld\n"
                "%%%%EOF\n",
                (long)nOffsetXREF);
}

/************************************************************************/
/*                              StartObj()                              */
/************************************************************************/

void GDALPDFWriter::StartObj(int nObjectId)
{
    CPLAssert(!bInWriteObj);
    CPLAssert(nObjectId - 1 < (int)asOffsets.size());
    CPLAssert(asOffsets[nObjectId - 1] == 0);
    asOffsets[nObjectId - 1] = VSIFTellL(fp);
    VSIFPrintfL(fp, "%d 0 obj\n", nObjectId);
    bInWriteObj = TRUE;
}

/************************************************************************/
/*                               EndObj()                               */
/************************************************************************/

void GDALPDFWriter::EndObj()
{
    CPLAssert(bInWriteObj);
    VSIFPrintfL(fp, "endobj\n");
    bInWriteObj = FALSE;
}

/************************************************************************/
/*                              WriteSRS()                              */
/************************************************************************/

int  GDALPDFWriter::WriteSRS(int nWidth, int nHeight,
                             const char* pszWKT,
                             double adfGeoTransform[6])
{
    if( pszWKT == NULL || EQUAL(pszWKT, "") )
        return 0;

    OGRSpatialReferenceH hSRS = OSRNewSpatialReference(pszWKT);
    if( hSRS == NULL )
        return 0;
    OGRSpatialReferenceH hSRSGeog = OSRCloneGeogCS(hSRS);
    if( hSRSGeog == NULL )
    {
        OSRDestroySpatialReference(hSRS);
        return 0;
    }
    OGRCoordinateTransformationH hCT = OCTNewCoordinateTransformation( hSRS, hSRSGeog);
    if( hCT == NULL )
    {
        OSRDestroySpatialReference(hSRS);
        OSRDestroySpatialReference(hSRSGeog);
        return 0;
    }

    double adfGPTS[8];

    #define PIXEL_TO_GEO_X(x,y) adfGeoTransform[0] + x * adfGeoTransform[1] + y * adfGeoTransform[2]
    #define PIXEL_TO_GEO_Y(x,y) adfGeoTransform[3] + x * adfGeoTransform[4] + y * adfGeoTransform[5]

    int bSuccess = TRUE;

    /* Upper-left */
    adfGPTS[0] = PIXEL_TO_GEO_X(0, 0);
    adfGPTS[1] = PIXEL_TO_GEO_Y(0, 0);
    bSuccess &= (OCTTransform( hCT, 1, adfGPTS + 0, adfGPTS + 1, NULL ) == 1);

    /* Lower-left */
    adfGPTS[2] = PIXEL_TO_GEO_X(0, nHeight);
    adfGPTS[3] = PIXEL_TO_GEO_Y(0, nHeight);
    bSuccess &= (OCTTransform( hCT, 1, adfGPTS + 2, adfGPTS + 3, NULL ) == 1);

    /* Lower-right */
    adfGPTS[4] = PIXEL_TO_GEO_X(nWidth, nHeight);
    adfGPTS[5] = PIXEL_TO_GEO_Y(nWidth, nHeight);
    bSuccess &= (OCTTransform( hCT, 1, adfGPTS + 4, adfGPTS + 5, NULL ) == 1);

    /* Upper-right */
    adfGPTS[6] = PIXEL_TO_GEO_X(nWidth, 0);
    adfGPTS[7] = PIXEL_TO_GEO_Y(nWidth, 0);
    bSuccess &= (OCTTransform( hCT, 1, adfGPTS + 6, adfGPTS + 7, NULL ) == 1);

    if (!bSuccess)
    {
        OSRDestroySpatialReference(hSRS);
        OSRDestroySpatialReference(hSRSGeog);
        OCTDestroyCoordinateTransformation(hCT);
        return 0;
    }

    const char * pszAuthorityCode = OSRGetAuthorityCode( hSRS, NULL );
    const char * pszAuthorityName = OSRGetAuthorityName( hSRS, NULL );
    int nEPSGCode = 0;
    if( pszAuthorityName != NULL && EQUAL(pszAuthorityName, "EPSG") &&
        pszAuthorityCode != NULL )
        nEPSGCode = atoi(pszAuthorityCode);

    int bIsGeographic = OSRIsGeographic(hSRS);

    OSRMorphToESRI(hSRS);
    char* pszESRIWKT = NULL;
    OSRExportToWkt(hSRS, &pszESRIWKT);

    OSRDestroySpatialReference(hSRS);
    OSRDestroySpatialReference(hSRSGeog);
    OCTDestroyCoordinateTransformation(hCT);
    hSRS = NULL;
    hSRSGeog = NULL;
    hCT = NULL;

    if (pszESRIWKT == NULL)
        return 0;

    int nViewportId = AllocNewObject();
    int nMeasureId = AllocNewObject();
    int nGCSId = AllocNewObject();

    StartObj(nViewportId);
    VSIFPrintfL(fp,
                "<<\n"
                "  /Type /Viewport\n"
                "  /Name (Layer)\n"
                "  /BBox [ 0 0 %d %d ]\n"
                "  /Measure %d 0 R\n"
                ">>\n",
                nWidth, nHeight,
                nMeasureId);
    EndObj();

    StartObj(nMeasureId);
    VSIFPrintfL(fp,
                "<<\n"
                "  /Type /Measure\n"
                "  /Subtype /GEO\n"
                "  /Bounds [0 1 0 0 1 0 1 1 ]\n"
                "  /GPTS [%.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f]\n"
                "  /LPTS [0 1 0 0 1 0 1 1 ]\n"
                "  /GCS %d 0 R\n"
                ">>\n",
                adfGPTS[1], adfGPTS[0],
                adfGPTS[3], adfGPTS[2],
                adfGPTS[5], adfGPTS[4],
                adfGPTS[7], adfGPTS[6],
                nGCSId);
    EndObj();


    StartObj(nGCSId);
    VSIFPrintfL(fp,
                "<<\n"
                "  /Type /%s\n"
                "  /WKT (%s)\n",
                bIsGeographic ? "GEOGCS" : "PROJCS", pszESRIWKT);
    if (nEPSGCode)
        VSIFPrintfL(fp,
                    "  /EPSG %d\n",
                    nEPSGCode);
    VSIFPrintfL(fp,">>\n");
    EndObj();

    CPLFree(pszESRIWKT);

    return nViewportId;
}

/************************************************************************/
/*                             SetInfo()                                */
/************************************************************************/

void GDALPDFWriter::SetInfo()
{
    CPLAssert(nInfoId == 0);
    nInfoId = AllocNewObject();
    StartObj(nInfoId);
    VSIFPrintfL(fp,
                 "<< /Producer (GDAL)\n"
                 ">>\n");
    EndObj();
}

/************************************************************************/
/*                              WritePage()                             */
/************************************************************************/

int GDALPDFWriter::WritePage(GDALDataset* poSrcDS,
                             PDFCompressMethod eCompressMethod,
                             int nJPEGQuality,
                             const char* pszJPEG2000_DRIVER,
                             int nBlockXSize, int nBlockYSize,
                             GDALProgressFunc pfnProgress,
                             void * pProgressData)
{
    int  nWidth = poSrcDS->GetRasterXSize();
    int  nHeight = poSrcDS->GetRasterYSize();
    const char* pszWKT = poSrcDS->GetProjectionRef();
    double adfGeoTransform[6];
    poSrcDS->GetGeoTransform(adfGeoTransform);

    int nPageId = AllocNewObject();
    asPageId.push_back(nPageId);

    int nContentId = AllocNewObject();
    int nContentLengthId = AllocNewObject();
    int nResourcesId = AllocNewObject();

    int nViewportId = WriteSRS(nWidth, nHeight,
                               pszWKT, adfGeoTransform);

    StartObj(nPageId);
    VSIFPrintfL(fp,
                "<< /Type /Page\n"
                "   /Parent %d 0 R\n"
                "   /MediaBox [ 0 0 %d %d ]\n"
                "   /Contents %d 0 R\n"
                "   /Group <<\n"
                "      /Type /Group\n"
                "      /S /Transparency\n"
                "      /CS /DeviceRGB\n"
                "   >>\n"
                "   /Resources %d 0 R\n",
                nPageResourceId,
                nWidth, nHeight,
                nContentId,
                nResourcesId);
    if (nViewportId)
    {
        VSIFPrintfL(fp,
                    "   /VP [ %d 0 R ]\n",
                    nViewportId);
    }
    VSIFPrintfL(fp, ">>\n");
    EndObj();

    std::vector<int> asImageId;
    int nXBlocks = (nWidth + nBlockXSize - 1) / nBlockXSize;
    int nYBlocks = (nHeight + nBlockYSize - 1) / nBlockYSize;
    int nBlocks = nXBlocks * nYBlocks;
    int nBlockXOff, nBlockYOff;
    for(nBlockYOff = 0; nBlockYOff < nYBlocks; nBlockYOff ++)
    {
        for(nBlockXOff = 0; nBlockXOff < nXBlocks; nBlockXOff ++)
        {
            int nReqWidth = MIN(nBlockXSize, nWidth - nBlockXOff * nBlockXSize);
            int nReqHeight = MIN(nBlockYSize, nHeight - nBlockYOff * nBlockYSize);
            int iImage = nBlockYOff * nXBlocks + nBlockXOff;

            void* pScaledData = GDALCreateScaledProgress( iImage / (double)nBlocks,
                                                          (iImage + 1) / (double)nBlocks,
                                                          pfnProgress, pProgressData);

            int nImageId = WriteBlock(poSrcDS,
                                      nBlockXOff * nBlockXSize,
                                      nBlockYOff * nBlockYSize,
                                      nReqWidth, nReqHeight,
                                      eCompressMethod,
                                      nJPEGQuality,
                                      pszJPEG2000_DRIVER,
                                      GDALScaledProgress,
                                      pScaledData);

            GDALDestroyScaledProgress(pScaledData);

            if (nImageId == 0)
                return FALSE;

            asImageId.push_back(nImageId);
        }
    }

    StartObj(nContentId);
    VSIFPrintfL(fp, "<< /Length %d 0 R\n", nContentLengthId);
    VSIFPrintfL(fp, ">>\n");
    VSIFPrintfL(fp, "stream\n");
    vsi_l_offset nStreamStart = VSIFTellL(fp);
    for(nBlockYOff = 0; nBlockYOff < nYBlocks; nBlockYOff ++)
    {
        for(nBlockXOff = 0; nBlockXOff < nXBlocks; nBlockXOff ++)
        {
            int nReqWidth = MIN(nBlockXSize, nWidth - nBlockXOff * nBlockXSize);
            int nReqHeight = MIN(nBlockYSize, nHeight - nBlockYOff * nBlockYSize);

            int iImage = nBlockYOff * nXBlocks + nBlockXOff;

            VSIFPrintfL(fp, "q\n");
            VSIFPrintfL(fp, "%d 0 0 %d %d %d cm\n",
                        nReqWidth, nReqHeight,
                        nBlockXOff * nBlockXSize,
                        nHeight - nBlockYOff * nBlockYSize - nReqHeight);
            VSIFPrintfL(fp, "/Image%d Do\n",
                        asImageId[iImage]);
            VSIFPrintfL(fp, "Q\n");
        }
    }
    vsi_l_offset nStreamEnd = VSIFTellL(fp);
    VSIFPrintfL(fp, "endstream\n");
    EndObj();

    StartObj(nContentLengthId);
    VSIFPrintfL(fp,
                "   %ld\n",
                (long)(nStreamEnd - nStreamStart));
    EndObj();

    StartObj(nResourcesId);
    VSIFPrintfL(fp, "<< /XObject <<");
    for(size_t iImage = 0; iImage < asImageId.size(); iImage ++)
    {
        VSIFPrintfL(fp, "  /Image%d %d 0 R",
                    asImageId[iImage], asImageId[iImage]);
    }
    VSIFPrintfL(fp, "  >>\n");
    VSIFPrintfL(fp, ">>\n");
    EndObj();

    return TRUE;
}

/************************************************************************/
/*                             WriteBlock()                             */
/************************************************************************/

int GDALPDFWriter::WriteBlock(GDALDataset* poSrcDS,
                             int nXOff, int nYOff, int nReqXSize, int nReqYSize,
                             PDFCompressMethod eCompressMethod,
                             int nJPEGQuality,
                             const char* pszJPEG2000_DRIVER,
                             GDALProgressFunc pfnProgress,
                             void * pProgressData)
{
    int  nBands = poSrcDS->GetRasterCount();

    if (nBands == 4 && eCompressMethod != COMPRESS_JPEG2000)
        nBands = 3; /* TODO: alpha handling */

    CPLErr eErr = CE_None;
    GDALDataset* poBlockSrcDS = NULL;
    GDALDatasetH hMemDS = NULL;
    GByte* pabyMEMDSBuffer = NULL;

    if( nReqXSize == poSrcDS->GetRasterXSize() &&
        nReqYSize == poSrcDS->GetRasterYSize() )
    {
        poBlockSrcDS = poSrcDS;
    }
    else
    {
        GDALDriverH hMemDriver = GDALGetDriverByName("MEM");
        if( hMemDriver == NULL )
            return 0;

        hMemDS = GDALCreate(hMemDriver, "MEM:::",
                            nReqXSize, nReqYSize, 0,
                            GDT_Byte, NULL);
        if (hMemDS == NULL)
            return 0;

        pabyMEMDSBuffer =
            (GByte*)VSIMalloc3(nReqXSize, nReqYSize, nBands);
        if (pabyMEMDSBuffer == NULL)
        {
            GDALClose(hMemDS);
            return 0;
        }

        eErr = poSrcDS->RasterIO(GF_Read,
                                nXOff, nYOff,
                                nReqXSize, nReqYSize,
                                pabyMEMDSBuffer, nReqXSize, nReqYSize,
                                GDT_Byte, nBands, NULL,
                                0, 0, 0);

        if( eErr != CE_None )
        {
            CPLFree(pabyMEMDSBuffer);
            GDALClose(hMemDS);
            return 0;
        }

        int iBand;
        for(iBand = 0; iBand < nBands; iBand ++)
        {
            char** papszMEMDSOptions = NULL;
            char szTmp[64];
            memset(szTmp, 0, sizeof(szTmp));
            CPLPrintPointer(szTmp,
                            pabyMEMDSBuffer + iBand * nReqXSize * nReqYSize, sizeof(szTmp));
            papszMEMDSOptions = CSLSetNameValue(papszMEMDSOptions, "DATAPOINTER", szTmp);
            GDALAddBand(hMemDS, GDT_Byte, papszMEMDSOptions);
            CSLDestroy(papszMEMDSOptions);
        }

        poBlockSrcDS = (GDALDataset*) hMemDS;
    }

    int nImageId = AllocNewObject();
    int nImageLengthId = AllocNewObject();

    StartObj(nImageId);
    VSIFPrintfL(fp, "<< /Length %d 0 R\n",
                nImageLengthId);
    VSIFPrintfL(fp,
                "   /Type /XObject\n");
    if( eCompressMethod == COMPRESS_DEFLATE )
    {
        VSIFPrintfL(fp, "   /Filter /FlateDecode\n");
    }
    else if( eCompressMethod == COMPRESS_JPEG )
    {
        VSIFPrintfL(fp, "   /Filter /DCTDecode\n");
    }
    else if( eCompressMethod == COMPRESS_JPEG2000 )
    {
        VSIFPrintfL(fp, "   /Filter /JPXDecode\n");
    }
    VSIFPrintfL(fp,
                "   /Subtype /Image\n"
                "   /Width %d\n"
                "   /Height %d\n"
                "   /ColorSpace /%s\n"
                "   /BitsPerComponent 8\n",
                nReqXSize, nReqYSize,
                (nBands == 1) ? "DeviceGray" : "DeviceRGB");

    VSIFPrintfL(fp, ">>\n"
                "stream\n");

    vsi_l_offset nStreamStart = VSIFTellL(fp);

    if( eCompressMethod == COMPRESS_JPEG ||
        eCompressMethod == COMPRESS_JPEG2000 )
    {
        GDALDriver* poJPEGDriver = NULL;
        char szTmp[64];
        char** papszOptions = NULL;

        if( eCompressMethod == COMPRESS_JPEG )
        {
            poJPEGDriver = (GDALDriver*) GDALGetDriverByName("JPEG");
            if (poJPEGDriver != NULL && nJPEGQuality > 0)
                papszOptions = CSLAddString(papszOptions, CPLSPrintf("QUALITY=%d", nJPEGQuality));
            sprintf(szTmp, "/vsimem/pdftemp/%p.jpg", this);
        }
        else
        {
            if (pszJPEG2000_DRIVER == NULL || EQUAL(pszJPEG2000_DRIVER, "JP2KAK"))
                poJPEGDriver = (GDALDriver*) GDALGetDriverByName("JP2KAK");
            if (poJPEGDriver == NULL)
            {
                if (pszJPEG2000_DRIVER == NULL || EQUAL(pszJPEG2000_DRIVER, "JP2ECW"))
                    poJPEGDriver = (GDALDriver*) GDALGetDriverByName("JP2ECW");
                if (poJPEGDriver)
                {
                    papszOptions = CSLAddString(papszOptions, "PROFILE=NPJE");
                    papszOptions = CSLAddString(papszOptions, "LAYERS=1");
                    papszOptions = CSLAddString(papszOptions, "GeoJP2=OFF");
                    papszOptions = CSLAddString(papszOptions, "GMLJP2=OFF");
                }
            }
            if (poJPEGDriver == NULL)
            {
                if (pszJPEG2000_DRIVER == NULL || EQUAL(pszJPEG2000_DRIVER, "JP2OpenJPEG"))
                    poJPEGDriver = (GDALDriver*) GDALGetDriverByName("JP2OpenJPEG");
            }
            if (poJPEGDriver == NULL)
            {
                if (pszJPEG2000_DRIVER == NULL || EQUAL(pszJPEG2000_DRIVER, "JPEG2000"))
                    poJPEGDriver = (GDALDriver*) GDALGetDriverByName("JPEG2000");
            }
            sprintf(szTmp, "/vsimem/pdftemp/%p.jp2", this);
        }

        if( poJPEGDriver == NULL )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "No %s driver found",
                     ( eCompressMethod == COMPRESS_JPEG ) ? "JPEG" : "JPEG2000");
            CPLFree(pabyMEMDSBuffer);
            if( hMemDS != NULL )
                GDALClose(hMemDS);
            return 0;
        }

        GDALDataset* poJPEGDS = NULL;

        poJPEGDS = poJPEGDriver->CreateCopy(szTmp, poBlockSrcDS,
                                            FALSE, papszOptions,
                                            pfnProgress, pProgressData);

        CSLDestroy(papszOptions);
        if( poJPEGDS == NULL )
        {
            CPLFree(pabyMEMDSBuffer);
            if( hMemDS != NULL )
                GDALClose(hMemDS);
            return 0;
        }

        GDALClose(poJPEGDS);

        vsi_l_offset nJPEGDataSize = 0;
        GByte* pabyJPEGData = VSIGetMemFileBuffer(szTmp, &nJPEGDataSize, TRUE);
        VSIFWriteL(pabyJPEGData, nJPEGDataSize, 1, fp);
        CPLFree(pabyJPEGData);
    }
    else
    {
        VSILFILE* fpGZip = NULL;
        VSILFILE* fpBack = fp;
        if( eCompressMethod == COMPRESS_DEFLATE )
        {
            fpGZip = (VSILFILE* )VSICreateGZipWritable( (VSIVirtualHandle*) fp, TRUE, FALSE );
            fp = fpGZip;
        }

        GByte* pabyLine = (GByte*)CPLMalloc(nReqXSize * nBands);
        for(int iLine = 0; iLine < nReqYSize; iLine ++)
        {
            /* Get pixel interleaved data */
            eErr = poBlockSrcDS->RasterIO(GF_Read,
                                          0, iLine, nReqXSize, 1,
                                          pabyLine, nReqXSize, 1, GDT_Byte,
                                          nBands, NULL, nBands, 0, 1);
            if( eErr != CE_None )
                break;

            if( VSIFWriteL(pabyLine, nReqXSize * nBands, 1, fp) != 1 )
            {
                eErr = CE_Failure;
                break;
            }

            if( eErr == CE_None
                && !pfnProgress( (iLine+1) / (double)nReqYSize,
                                NULL, pProgressData ) )
            {
                CPLError( CE_Failure, CPLE_UserInterrupt,
                        "User terminated CreateCopy()" );
                eErr = CE_Failure;
                break;
            }
        }

        CPLFree(pabyLine);

        if (fpGZip)
            VSIFCloseL(fpGZip);

        fp = fpBack;
    }

    CPLFree(pabyMEMDSBuffer);
    pabyMEMDSBuffer = NULL;
    if( hMemDS != NULL )
    {
        GDALClose(hMemDS);
        hMemDS = NULL;
    }

    vsi_l_offset nStreamEnd = VSIFTellL(fp);
    VSIFPrintfL(fp,
                "\n"
                "endstream\n");
    EndObj();

    StartObj(nImageLengthId);
    VSIFPrintfL(fp,
                "   %ld\n",
                (long)(nStreamEnd - nStreamStart));
    EndObj();

    return eErr == CE_None ? nImageId : 0;
}

/************************************************************************/
/*                              WritePages()                            */
/************************************************************************/

void GDALPDFWriter::WritePages()
{
    StartObj(nPageResourceId);
    VSIFPrintfL(fp,
                "<< /Type /Pages\n"
                "   /Kids [ ");

    for(size_t i=0;i<asPageId.size();i++)
        VSIFPrintfL(fp, "%d 0 R ", asPageId[i]);

    VSIFPrintfL(fp, "]\n");
    VSIFPrintfL(fp, "   /Count %d\n", (int)asPageId.size());
    VSIFPrintfL(fp, ">>\n");
    EndObj();

    StartObj(nCatalogId);
    VSIFPrintfL(fp,
                 "<< /Type /Catalog\n"
                 "   /Pages %d 0 R\n"
                 ">>\n",
                 nPageResourceId);
    EndObj();
}

/************************************************************************/
/*                        GDALPDFGetJPEGQuality()                       */
/************************************************************************/

static int GDALPDFGetJPEGQuality(char** papszOptions)
{
    int nJpegQuality = -1;
    const char* pszValue = CSLFetchNameValue( papszOptions, "JPEG_QUALITY" );
    if( pszValue  != NULL )
    {
        nJpegQuality = atoi( pszValue );
        if (!(nJpegQuality >= 1 && nJpegQuality <= 100))
        {
            CPLError( CE_Warning, CPLE_IllegalArg,
                    "JPEG_QUALITY=%s value not recognised, ignoring.",
                    pszValue );
            nJpegQuality = -1;
        }
    }
    return nJpegQuality;
}

/************************************************************************/
/*                          GDALPDFCreateCopy()                         */
/************************************************************************/

GDALDataset *GDALPDFCreateCopy( const char * pszFilename,
                                GDALDataset *poSrcDS,
                                int bStrict,
                                char **papszOptions,
                                GDALProgressFunc pfnProgress,
                                void * pProgressData )
{
    int  nBands = poSrcDS->GetRasterCount();
    int  nWidth = poSrcDS->GetRasterXSize();
    int  nHeight = poSrcDS->GetRasterYSize();

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Some some rudimentary checks                                    */
/* -------------------------------------------------------------------- */
    if( nBands != 1 && nBands != 3 && nBands != 4 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "PDF driver doesn't support %d bands.  Must be 1 (grey), "
                  "3 (RGB) or 4 bands.\n", nBands );

        return NULL;
    }

    if (nBands == 1 &&
        poSrcDS->GetRasterBand(1)->GetColorTable() != NULL)
    {
        CPLError( (bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported,
                  "PDF driver ignores color table. "
                  "The source raster band will be considered as grey level.\n"
                  "Consider using color table expansion (-expand option in gdal_translate)\n");
        if (bStrict)
            return NULL;
    }

    GDALDataType eDT = poSrcDS->GetRasterBand(1)->GetRasterDataType();
    if( eDT != GDT_Byte )
    {
        CPLError( (bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported,
                  "PDF driver doesn't support data type %s. "
                  "Only eight bit byte bands supported.\n",
                  GDALGetDataTypeName(
                      poSrcDS->GetRasterBand(1)->GetRasterDataType()) );

        if (bStrict)
            return NULL;
    }

/* -------------------------------------------------------------------- */
/*     Read options.                                                    */
/* -------------------------------------------------------------------- */
    PDFCompressMethod eCompressMethod = COMPRESS_DEFLATE;
    const char* pszCompressMethod = CSLFetchNameValue(papszOptions, "COMPRESS");
    if (pszCompressMethod)
    {
        if( EQUAL(pszCompressMethod, "NONE") )
            eCompressMethod = COMPRESS_NONE;
        else if( EQUAL(pszCompressMethod, "DEFLATE") )
            eCompressMethod = COMPRESS_DEFLATE;
        else if( EQUAL(pszCompressMethod, "JPEG") )
            eCompressMethod = COMPRESS_JPEG;
        else if( EQUAL(pszCompressMethod, "JPEG2000") )
            eCompressMethod = COMPRESS_JPEG2000;
        else
        {
            CPLError( (bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported,
                    "Unsupported value for COMPRESS.");

            if (bStrict)
                return NULL;
        }
    }

    int nBlockXSize = nWidth;
    int nBlockYSize = nHeight;
    const char* pszValue;

    int bTiled = CSLFetchBoolean( papszOptions, "TILED", FALSE );
    if( bTiled )
        nBlockXSize = nBlockYSize = 256;

    pszValue = CSLFetchNameValue(papszOptions, "BLOCKXSIZE");
    if( pszValue != NULL )
    {
        nBlockXSize = atoi( pszValue );
        if (nBlockXSize < 0 || nBlockXSize >= nWidth)
            nBlockXSize = nWidth;
    }

    pszValue = CSLFetchNameValue(papszOptions, "BLOCKYSIZE");
    if( pszValue != NULL )
    {
        nBlockYSize = atoi( pszValue );
        if (nBlockYSize < 0 || nBlockYSize >= nHeight)
            nBlockYSize = nHeight;
    }

    int nJPEGQuality = GDALPDFGetJPEGQuality(papszOptions);

    const char* pszJPEG2000_DRIVER = CSLFetchNameValue(papszOptions, "JPEG2000_DRIVER");

/* -------------------------------------------------------------------- */
/*      Create file.                                                    */
/* -------------------------------------------------------------------- */
    VSILFILE* fp = VSIFOpenL(pszFilename, "wb");
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to create PDF file %s.\n",
                  pszFilename );
        return NULL;
    }


    GDALPDFWriter oWriter(fp);

    oWriter.SetInfo();
    int bRet = oWriter.WritePage(poSrcDS,
                                 eCompressMethod,
                                 nJPEGQuality,
                                 pszJPEG2000_DRIVER,
                                 nBlockXSize, nBlockYSize,
                                 pfnProgress, pProgressData);
    oWriter.Close();

    if (!bRet)
    {
        VSIUnlink(pszFilename);
        return NULL;
    }
    else
    {
        return (GDALDataset*) GDALOpen(pszFilename, GA_ReadOnly);
    }
}
