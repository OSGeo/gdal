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

/* hack for PDF driver and poppler >= 0.15.0 that defines incompatible "typedef bool GBool" */
/* in include/poppler/goo/gtypes.h with the one defined in cpl_port.h */
#define CPL_GBOOL_DEFINED

#include "pdfcreatecopy.h"

#include "cpl_vsi_virtual.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "ogr_srs_api.h"
#include "ogr_spatialref.h"

#include "pdfobject.h"

CPL_CVSID("$Id$");

#define PIXEL_TO_GEO_X(x,y) adfGeoTransform[0] + x * adfGeoTransform[1] + y * adfGeoTransform[2]
#define PIXEL_TO_GEO_Y(x,y) adfGeoTransform[3] + x * adfGeoTransform[4] + y * adfGeoTransform[5]

/************************************************************************/
/*                             Init()                                   */
/************************************************************************/

void GDALPDFWriter::Init()
{
    nPageResourceId = 0;
    nCatalogId = nCatalogGen = 0;
    bInWriteObj = FALSE;
    nInfoId = nInfoGen = 0;
    nXMPId = nXMPGen = 0;

    nLastStartXRef = 0;
    nLastXRefSize = 0;
    bCanUpdate = FALSE;
}

/************************************************************************/
/*                         GDALPDFWriter()                              */
/************************************************************************/

GDALPDFWriter::GDALPDFWriter(VSILFILE* fpIn, int bAppend) : fp(fpIn)
{
    Init();

    if (!bAppend)
    {
        VSIFPrintfL(fp, "%%PDF-1.6\n");

        /* See PDF 1.7 reference, page 92. Write 4 non-ASCII bytes to indicate that the content will be binary */
        VSIFPrintfL(fp, "%%%c%c%c%c\n", 0xFF, 0xFF, 0xFF, 0xFF);

        nPageResourceId = AllocNewObject();
        nCatalogId = AllocNewObject();
    }
}

/************************************************************************/
/*                         ~GDALPDFWriter()                             */
/************************************************************************/

GDALPDFWriter::~GDALPDFWriter()
{
    Close();
}

/************************************************************************/
/*                          ParseIndirectRef()                          */
/************************************************************************/

static int ParseIndirectRef(const char* pszStr, int& nNum, int &nGen)
{
    while(*pszStr == ' ')
        pszStr ++;

    nNum = atoi(pszStr);
    while(*pszStr >= '0' && *pszStr <= '9')
        pszStr ++;
    if (*pszStr != ' ')
        return FALSE;

    while(*pszStr == ' ')
        pszStr ++;

    nGen = atoi(pszStr);
    while(*pszStr >= '0' && *pszStr <= '9')
        pszStr ++;
    if (*pszStr != ' ')
        return FALSE;

    while(*pszStr == ' ')
        pszStr ++;

    return *pszStr == 'R';
}

/************************************************************************/
/*                       ParseTrailerAndXRef()                          */
/************************************************************************/

int GDALPDFWriter::ParseTrailerAndXRef()
{
    VSIFSeekL(fp, 0, SEEK_END);
    char szBuf[1024+1];
    vsi_l_offset nOffset = VSIFTellL(fp);

    if (nOffset > 128)
        nOffset -= 128;
    else
        nOffset = 0;

    /* Find startxref section */
    VSIFSeekL(fp, nOffset, SEEK_SET);
    int nRead = VSIFReadL(szBuf, 1, 128, fp);
    szBuf[nRead] = 0;
    if (nRead < 9)
        return FALSE;

    const char* pszStartXRef = NULL;
    int i;
    for(i = nRead - 9; i>= 0; i --)
    {
        if (strncmp(szBuf + i, "startxref", 9) == 0)
        {
            pszStartXRef = szBuf + i;
            break;
        }
    }
    if (pszStartXRef == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find startxref");
        return FALSE;
    }
    pszStartXRef += 9;
    while(*pszStartXRef == '\r' || *pszStartXRef == '\n')
        pszStartXRef ++;
    if (*pszStartXRef == '\0')
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find startxref");
        return FALSE;
    }

    nLastStartXRef = atoi(pszStartXRef);

    /* Skip to beginning of xref section */
    VSIFSeekL(fp, nLastStartXRef, SEEK_SET);

    /* And skip to trailer */
    const char* pszLine;
    while( (pszLine = CPLReadLineL(fp)) != NULL)
    {
        if (strncmp(pszLine, "trailer", 7) == 0)
            break;
    }

    if( pszLine == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find trailer");
        return FALSE;
    }

    /* Read trailer content */
    nRead = VSIFReadL(szBuf, 1, 1024, fp);
    szBuf[nRead] = 0;

    /* Find XRef size */
    const char* pszSize = strstr(szBuf, "/Size");
    if (pszSize == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find trailer /Size");
        return FALSE;
    }
    pszSize += 5;
    while(*pszSize == ' ')
        pszSize ++;
    nLastXRefSize = atoi(pszSize);

    /* Find Root object */
    const char* pszRoot = strstr(szBuf, "/Root");
    if (pszRoot == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find trailer /Root");
        return FALSE;
    }
    pszRoot += 5;
    while(*pszRoot == ' ')
        pszRoot ++;

    if (!ParseIndirectRef(pszRoot, nCatalogId, nCatalogGen))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot parse trailer /Root");
        return FALSE;
    }

    /* Find Info object */
    const char* pszInfo = strstr(szBuf, "/Info");
    if (pszInfo != NULL)
    {
        pszInfo += 5;
        while(*pszInfo == ' ')
            pszInfo ++;

        if (!ParseIndirectRef(pszInfo, nInfoId, nInfoGen))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot parse trailer /Info");
            nInfoId = nInfoGen = 0;
        }
    }

    VSIFSeekL(fp, 0, SEEK_END);

    return TRUE;
}

/************************************************************************/
/*                              Close()                                 */
/************************************************************************/

void GDALPDFWriter::Close()
{
    if (fp)
    {
        CPLAssert(!bInWriteObj);
        if (nPageResourceId)
        {
            WritePages();
            WriteXRefTableAndTrailer();
        }
        else if (bCanUpdate)
        {
            WriteXRefTableAndTrailer();
        }
        VSIFCloseL(fp);
    }
    fp = NULL;
}

/************************************************************************/
/*                           UpdateProj()                               */
/************************************************************************/

void GDALPDFWriter::UpdateProj(GDALDataset* poSrcDS,
                               double dfDPI,
                               GDALPDFDictionaryRW* poPageDict,
                               int nPageNum, int nPageGen)
{
    bCanUpdate = TRUE;
    if ((int)asXRefEntries.size() < nLastXRefSize - 1)
        asXRefEntries.resize(nLastXRefSize - 1);

    int nViewportId = 0;
    int nLGIDictId = 0;

    CPLAssert(nPageNum != 0);
    CPLAssert(poPageDict != NULL);

    const char* pszGEO_ENCODING = CPLGetConfigOption("GDAL_PDF_GEO_ENCODING", "ISO32000");
    if (EQUAL(pszGEO_ENCODING, "ISO32000") || EQUAL(pszGEO_ENCODING, "BOTH"))
        nViewportId = WriteSRS_ISO32000(poSrcDS, dfDPI / 72.0);
    if (EQUAL(pszGEO_ENCODING, "OGC_BP") || EQUAL(pszGEO_ENCODING, "BOTH"))
        nLGIDictId = WriteSRS_OGC_BP(poSrcDS, dfDPI / 72.0);

    poPageDict->Remove("VP");
    poPageDict->Remove("LGIDict");

    if (nViewportId)
    {
        poPageDict->Add("VP", &((new GDALPDFArrayRW())->
                Add(nViewportId, 0)));
    }

    if (nLGIDictId)
    {
        poPageDict->Add("LGIDict", nLGIDictId, 0);
    }

    StartObj(nPageNum, nPageGen);
    VSIFPrintfL(fp, "%s\n", poPageDict->Serialize().c_str());
    EndObj();
}

/************************************************************************/
/*                           UpdateInfo()                               */
/************************************************************************/

void GDALPDFWriter::UpdateInfo(GDALDataset* poSrcDS)
{
    bCanUpdate = TRUE;
    if ((int)asXRefEntries.size() < nLastXRefSize - 1)
        asXRefEntries.resize(nLastXRefSize - 1);

    int nNewInfoId = SetInfo(poSrcDS, NULL);
    /* Write empty info, because podofo driver will find the dangling info instead */
    if (nNewInfoId == 0 && nInfoId != 0)
    {
        StartObj(nInfoId, nInfoGen);
        VSIFPrintfL(fp, "<< >>\n");
        EndObj();
    }
}

/************************************************************************/
/*                           UpdateXMP()                                */
/************************************************************************/

void GDALPDFWriter::UpdateXMP(GDALDataset* poSrcDS,
                              GDALPDFDictionaryRW* poCatalogDict)
{
    bCanUpdate = TRUE;
    if ((int)asXRefEntries.size() < nLastXRefSize - 1)
        asXRefEntries.resize(nLastXRefSize - 1);

    CPLAssert(nCatalogId != 0);
    CPLAssert(poCatalogDict != NULL);

    GDALPDFObject* poMetadata = poCatalogDict->Get("Metadata");
    if (poMetadata)
    {
        nXMPId = poMetadata->GetRefNum();
        nXMPGen = poMetadata->GetRefGen();
    }

    poCatalogDict->Remove("Metadata");
    int nNewXMPId = SetXMP(poSrcDS, NULL);

    /* Write empty metadata, because podofo driver will find the dangling info instead */
    if (nNewXMPId == 0 && nXMPId != 0)
    {
        StartObj(nXMPId, nXMPGen);
        VSIFPrintfL(fp, "<< >>\n");
        EndObj();
    }

    if (nXMPId)
        poCatalogDict->Add("Metadata", nXMPId, 0);

    StartObj(nCatalogId, nCatalogGen);
    VSIFPrintfL(fp, "%s\n", poCatalogDict->Serialize().c_str());
    EndObj();
}

/************************************************************************/
/*                           AllocNewObject()                           */
/************************************************************************/

int GDALPDFWriter::AllocNewObject()
{
    asXRefEntries.push_back(GDALXRefEntry());
    return (int)asXRefEntries.size();
}

/************************************************************************/
/*                        WriteXRefTableAndTrailer()                    */
/************************************************************************/

void GDALPDFWriter::WriteXRefTableAndTrailer()
{
    vsi_l_offset nOffsetXREF = VSIFTellL(fp);
    VSIFPrintfL(fp, "xref\n");

    char buffer[16];
    if (bCanUpdate)
    {
        VSIFPrintfL(fp, "0 1\n");
        VSIFPrintfL(fp, "0000000000 65535 f \n");
        for(size_t i=0;i<asXRefEntries.size();)
        {
            if (asXRefEntries[i].nOffset != 0)
            {
                /* Find number of consecutive objects */
                size_t nCount = 1;
                while(i + nCount <asXRefEntries.size() && asXRefEntries[i + nCount].nOffset != 0)
                    nCount ++;

                VSIFPrintfL(fp, "%d %d\n", (int)i + 1, (int)nCount);
                size_t iEnd = i + nCount;
                for(; i < iEnd; i++)
                {
                    snprintf (buffer, sizeof(buffer), "%010ld", (long)asXRefEntries[i].nOffset);
                    VSIFPrintfL(fp, "%s %05d n \n", buffer, asXRefEntries[i].nGen);
                }
            }
            else
            {
                i++;
            }
        }
    }
    else
    {
        VSIFPrintfL(fp, "%d %d\n",
                    0, (int)asXRefEntries.size() + 1);
        VSIFPrintfL(fp, "0000000000 65535 f \n");
        for(size_t i=0;i<asXRefEntries.size();i++)
        {
            snprintf (buffer, sizeof(buffer), "%010ld", (long)asXRefEntries[i].nOffset);
            VSIFPrintfL(fp, "%s %05d n \n", buffer, asXRefEntries[i].nGen);
        }
    }

    VSIFPrintfL(fp, "trailer\n");
    GDALPDFDictionaryRW oDict;
    oDict.Add("Size", (int)asXRefEntries.size() + 1)
         .Add("Root", nCatalogId, nCatalogGen);
    if (nInfoId)
        oDict.Add("Info", nInfoId, nInfoGen);
    if (nLastStartXRef)
        oDict.Add("Prev", nLastStartXRef);
    VSIFPrintfL(fp, "%s\n", oDict.Serialize().c_str());

    VSIFPrintfL(fp,
                "startxref\n"
                "%ld\n"
                "%%%%EOF\n",
                (long)nOffsetXREF);
}

/************************************************************************/
/*                              StartObj()                              */
/************************************************************************/

void GDALPDFWriter::StartObj(int nObjectId, int nGen)
{
    CPLAssert(!bInWriteObj);
    CPLAssert(nObjectId - 1 < (int)asXRefEntries.size());
    CPLAssert(asXRefEntries[nObjectId - 1].nOffset == 0);
    asXRefEntries[nObjectId - 1].nOffset = VSIFTellL(fp);
    asXRefEntries[nObjectId - 1].nGen = nGen;
    VSIFPrintfL(fp, "%d %d obj\n", nObjectId, nGen);
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
/*                         WriteSRS_ISO32000()                          */
/************************************************************************/

int  GDALPDFWriter::WriteSRS_ISO32000(GDALDataset* poSrcDS,
                                      double dfUserUnit)
{
    int  nWidth = poSrcDS->GetRasterXSize();
    int  nHeight = poSrcDS->GetRasterYSize();
    const char* pszWKT = poSrcDS->GetProjectionRef();
    double adfGeoTransform[6];

    if( poSrcDS->GetGeoTransform(adfGeoTransform) != CE_None )
        return 0;

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
    GDALPDFDictionaryRW oViewPortDict;
    oViewPortDict.Add("Type", GDALPDFObjectRW::CreateName("Viewport"))
                 .Add("Name", "Layer")
                 .Add("BBox", &((new GDALPDFArrayRW())
                                ->Add(0)
                                .Add(0)
                                .Add(nWidth / dfUserUnit)
                                .Add(nHeight / dfUserUnit)))
                 .Add("Measure", nMeasureId, 0);
    VSIFPrintfL(fp, "%s\n", oViewPortDict.Serialize().c_str());
    EndObj();

    StartObj(nMeasureId);
    GDALPDFDictionaryRW oMeasureDict;
    oMeasureDict .Add("Type", GDALPDFObjectRW::CreateName("Measure"))
                 .Add("Subtype", GDALPDFObjectRW::CreateName("GEO"))
                 .Add("Bounds", &((new GDALPDFArrayRW())
                                ->Add(0).Add(1).
                                  Add(0).Add(0).
                                  Add(1).Add(0).
                                  Add(1).Add(1)))
                 .Add("GPTS", &((new GDALPDFArrayRW())
                                ->Add(adfGPTS[1]).Add(adfGPTS[0]).
                                  Add(adfGPTS[3]).Add(adfGPTS[2]).
                                  Add(adfGPTS[5]).Add(adfGPTS[4]).
                                  Add(adfGPTS[7]).Add(adfGPTS[6])))
                 .Add("LPTS", &((new GDALPDFArrayRW())
                                ->Add(0).Add(1).
                                  Add(0).Add(0).
                                  Add(1).Add(0).
                                  Add(1).Add(1)))
                 .Add("GCS", nGCSId, 0);
    VSIFPrintfL(fp, "%s\n", oMeasureDict.Serialize().c_str());
    EndObj();


    StartObj(nGCSId);
    GDALPDFDictionaryRW oGCSDict;
    oGCSDict.Add("Type", GDALPDFObjectRW::CreateName(bIsGeographic ? "GEOGCS" : "PROJCS"))
            .Add("WKT", pszESRIWKT);
    if (nEPSGCode)
        oGCSDict.Add("EPSG", nEPSGCode);
    VSIFPrintfL(fp, "%s\n", oGCSDict.Serialize().c_str());
    EndObj();

    CPLFree(pszESRIWKT);

    return nViewportId;
}

/************************************************************************/
/*                         GDALPDFGeoCoordToNL()                        */
/************************************************************************/

static int GDALPDFGeoCoordToNL(double adfGeoTransform[6], int nHeight,
                               double dfUserUnit,
                               double X, double Y,
                               double* padfNLX, double* padfNLY)
{
    double adfGeoTransformInv[6];
    GDALInvGeoTransform(adfGeoTransform, adfGeoTransformInv);
    double x = adfGeoTransformInv[0] + X * adfGeoTransformInv[1] + Y * adfGeoTransformInv[2];
    double y = adfGeoTransformInv[3] + X * adfGeoTransformInv[4] + Y * adfGeoTransformInv[5];
    *padfNLX = x / dfUserUnit;
    *padfNLY = nHeight - y / dfUserUnit;
    return TRUE;
}
/************************************************************************/
/*                           WriteSRS_OGC_BP()                          */
/************************************************************************/

int GDALPDFWriter::WriteSRS_OGC_BP(GDALDataset* poSrcDS,
                                   double dfUserUnit)
{
    int  nWidth = poSrcDS->GetRasterXSize();
    int  nHeight = poSrcDS->GetRasterYSize();
    const char* pszWKT = poSrcDS->GetProjectionRef();
    double adfGeoTransform[6];

    if( poSrcDS->GetGeoTransform(adfGeoTransform) != CE_None )
        return 0;

    if( pszWKT == NULL || EQUAL(pszWKT, "") )
        return 0;

    OGRSpatialReferenceH hSRS = OSRNewSpatialReference(pszWKT);
    const OGRSpatialReference* poSRS = (const OGRSpatialReference*)hSRS;
    if( hSRS == NULL )
        return 0;
    
    double adfCTM[6];
    double dfX1 = 0;
    double dfY2 = nHeight;

    adfCTM[0] = adfGeoTransform[1] * dfUserUnit;
    adfCTM[1] = adfGeoTransform[2] * dfUserUnit;
    adfCTM[2] = - adfGeoTransform[4] * dfUserUnit;
    adfCTM[3] = - adfGeoTransform[5] * dfUserUnit;
    adfCTM[4] = adfGeoTransform[0] - (adfCTM[0] * dfX1 + adfCTM[2] * dfY2);
    adfCTM[5] = adfGeoTransform[3] - (adfCTM[1] * dfX1 + adfCTM[3] * dfY2);
    
    const OGR_SRSNode* poDatumNode = poSRS->GetAttrNode("DATUM");
    const char* pszDatumDescription = NULL;
    if (poDatumNode && poDatumNode->GetChildCount() > 0)
        pszDatumDescription = poDatumNode->GetChild(0)->GetValue();

    GDALPDFObjectRW* poPDFDatum = NULL;

    if (pszDatumDescription)
    {
        double dfSemiMajor = poSRS->GetSemiMajor();
        double dfInvFlattening = poSRS->GetInvFlattening();
        int nEPSGDatum = -1;
        const char *pszAuthority = poSRS->GetAuthorityName( "DATUM" );
        if( pszAuthority != NULL && EQUAL(pszAuthority,"EPSG") )
            nEPSGDatum = atoi(poSRS->GetAuthorityCode( "DATUM" ));

        if( EQUAL(pszDatumDescription,SRS_DN_WGS84) || nEPSGDatum == 6326 )
            poPDFDatum = GDALPDFObjectRW::CreateString("WGE");
        else if( EQUAL(pszDatumDescription, SRS_DN_NAD27) || nEPSGDatum == 6267 )
            poPDFDatum = GDALPDFObjectRW::CreateString("NAS");
        else if( EQUAL(pszDatumDescription, SRS_DN_NAD83) || nEPSGDatum == 6269 )
            poPDFDatum = GDALPDFObjectRW::CreateString("NAR");
        else
        {
            CPLDebug("PDF",
                     "Unhandled datum name (%s). Write datum parameters then.",
                     pszDatumDescription);

            GDALPDFDictionaryRW* poPDFDatumDict = new GDALPDFDictionaryRW();
            poPDFDatum = GDALPDFObjectRW::CreateDictionary(poPDFDatumDict);

            const OGR_SRSNode* poSpheroidNode = poSRS->GetAttrNode("SPHEROID");
            if (poSpheroidNode && poSpheroidNode->GetChildCount() >= 3)
            {
                poPDFDatumDict->Add("Description", pszDatumDescription);

                const char* pszEllipsoidCode = NULL;
#ifdef disabled_because_terrago_toolbar_does_not_like_it
                if( ABS(dfSemiMajor-6378249.145) < 0.01
                    && ABS(dfInvFlattening-293.465) < 0.0001 )
                {
                    pszEllipsoidCode = "CD";     /* Clark 1880 */
                }
                else if( ABS(dfSemiMajor-6378245.0) < 0.01
                         && ABS(dfInvFlattening-298.3) < 0.0001 )
                {
                    pszEllipsoidCode = "KA";      /* Krassovsky */
                }
                else if( ABS(dfSemiMajor-6378388.0) < 0.01
                         && ABS(dfInvFlattening-297.0) < 0.0001 )
                {
                    pszEllipsoidCode = "IN";       /* International 1924 */
                }
                else if( ABS(dfSemiMajor-6378160.0) < 0.01
                         && ABS(dfInvFlattening-298.25) < 0.0001 )
                {
                    pszEllipsoidCode = "AN";    /* Australian */
                }
                else if( ABS(dfSemiMajor-6377397.155) < 0.01
                         && ABS(dfInvFlattening-299.1528128) < 0.0001 )
                {
                    pszEllipsoidCode = "BR";     /* Bessel 1841 */
                }
                else if( ABS(dfSemiMajor-6377483.865) < 0.01
                         && ABS(dfInvFlattening-299.1528128) < 0.0001 )
                {
                    pszEllipsoidCode = "BN";   /* Bessel 1841 (Namibia / Schwarzeck)*/
                }
#if 0
                else if( ABS(dfSemiMajor-6378160.0) < 0.01
                         && ABS(dfInvFlattening-298.247167427) < 0.0001 )
                {
                    pszEllipsoidCode = "GRS67";      /* GRS 1967 */
                }
#endif
                else if( ABS(dfSemiMajor-6378137) < 0.01
                         && ABS(dfInvFlattening-298.257222101) < 0.000001 )
                {
                    pszEllipsoidCode = "RF";      /* GRS 1980 */
                }
                else if( ABS(dfSemiMajor-6378206.4) < 0.01
                         && ABS(dfInvFlattening-294.9786982) < 0.0001 )
                {
                    pszEllipsoidCode = "CC";     /* Clarke 1866 */
                }
                else if( ABS(dfSemiMajor-6377340.189) < 0.01
                         && ABS(dfInvFlattening-299.3249646) < 0.0001 )
                {
                    pszEllipsoidCode = "AM";   /* Modified Airy */
                }
                else if( ABS(dfSemiMajor-6377563.396) < 0.01
                         && ABS(dfInvFlattening-299.3249646) < 0.0001 )
                {
                    pszEllipsoidCode = "AA";       /* Airy */
                }
                else if( ABS(dfSemiMajor-6378200) < 0.01
                         && ABS(dfInvFlattening-298.3) < 0.0001 )
                {
                    pszEllipsoidCode = "HE";    /* Helmert 1906 */
                }
                else if( ABS(dfSemiMajor-6378155) < 0.01
                         && ABS(dfInvFlattening-298.3) < 0.0001 )
                {
                    pszEllipsoidCode = "FA";   /* Modified Fischer 1960 */
                }
#if 0
                else if( ABS(dfSemiMajor-6377298.556) < 0.01
                         && ABS(dfInvFlattening-300.8017) < 0.0001 )
                {
                    pszEllipsoidCode = "evrstSS";    /* Everest (Sabah & Sarawak) */
                }
                else if( ABS(dfSemiMajor-6378165.0) < 0.01
                         && ABS(dfInvFlattening-298.3) < 0.0001 )
                {
                    pszEllipsoidCode = "WGS60";      
                }
                else if( ABS(dfSemiMajor-6378145.0) < 0.01
                         && ABS(dfInvFlattening-298.25) < 0.0001 )
                {
                    pszEllipsoidCode = "WGS66";      
                }
#endif
                else if( ABS(dfSemiMajor-6378135.0) < 0.01
                         && ABS(dfInvFlattening-298.26) < 0.0001 )
                {
                    pszEllipsoidCode = "WD";      
                }
                else if( ABS(dfSemiMajor-6378137.0) < 0.01
                         && ABS(dfInvFlattening-298.257223563) < 0.000001 )
                {
                    pszEllipsoidCode = "WE";
                }
#endif

                if( pszEllipsoidCode != NULL )
                {
                    poPDFDatumDict->Add("Ellipsoid", pszEllipsoidCode);
                }
                else
                {
                    const char* pszEllipsoidDescription =
                        poSpheroidNode->GetChild(0)->GetValue();

                    CPLDebug("PDF",
                         "Unhandled ellipsoid name (%s). Write ellipsoid parameters then.",
                         pszEllipsoidDescription);

                    poPDFDatumDict->Add("Ellipsoid",
                        &((new GDALPDFDictionaryRW())
                        ->Add("Description", pszEllipsoidDescription)
                         .Add("SemiMajorAxis", dfSemiMajor)
                         .Add("InvFlattening", dfInvFlattening)));
                }

                const OGR_SRSNode *poTOWGS84 = poSRS->GetAttrNode( "TOWGS84" );
                if( poTOWGS84 != NULL
                    && poTOWGS84->GetChildCount() >= 3
                    && (poTOWGS84->GetChildCount() < 7 
                    || (EQUAL(poTOWGS84->GetChild(3)->GetValue(),"")
                        && EQUAL(poTOWGS84->GetChild(4)->GetValue(),"")
                        && EQUAL(poTOWGS84->GetChild(5)->GetValue(),"")
                        && EQUAL(poTOWGS84->GetChild(6)->GetValue(),""))) )
                {
                    poPDFDatumDict->Add("ToWGS84",
                        &((new GDALPDFDictionaryRW())
                        ->Add("dx", poTOWGS84->GetChild(0)->GetValue())
                         .Add("dy", poTOWGS84->GetChild(1)->GetValue())
                         .Add("dz", poTOWGS84->GetChild(2)->GetValue())));
                }
                else if( poTOWGS84 != NULL && poTOWGS84->GetChildCount() >= 7)
                {
                    poPDFDatumDict->Add("ToWGS84",
                        &((new GDALPDFDictionaryRW())
                        ->Add("dx", poTOWGS84->GetChild(0)->GetValue())
                         .Add("dy", poTOWGS84->GetChild(1)->GetValue())
                         .Add("dz", poTOWGS84->GetChild(2)->GetValue())
                         .Add("rx", poTOWGS84->GetChild(3)->GetValue())
                         .Add("ry", poTOWGS84->GetChild(4)->GetValue())
                         .Add("rz", poTOWGS84->GetChild(5)->GetValue())
                         .Add("sf", poTOWGS84->GetChild(6)->GetValue())));
                }
            }
        }
    }
    else
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "No datum name. Defaulting to WGS84.");
    }

    if (poPDFDatum == NULL)
        poPDFDatum = GDALPDFObjectRW::CreateString("WGE");

    const OGR_SRSNode* poNode = poSRS->GetRoot();
    if( poNode != NULL )
        poNode = poNode->GetChild(0);
    const char* pszDescription = NULL;
    if( poNode != NULL )
        pszDescription = poNode->GetValue();

    const char* pszProjectionOGCBP = "GEOGRAPHIC";
    const char *pszProjection = OSRGetAttrValue(hSRS, "PROJECTION", 0);

    CPLString osProjParams;

    GDALPDFDictionaryRW* poProjectionDict = new GDALPDFDictionaryRW();
    poProjectionDict->Add("Type", GDALPDFObjectRW::CreateName("Projection"));
    poProjectionDict->Add("Datum", poPDFDatum);

    if( pszProjection == NULL )
    {
        if( OSRIsGeographic(hSRS) )
            pszProjectionOGCBP = "GEOGRAPHIC";
        else if( OSRIsLocal(hSRS) )
            pszProjectionOGCBP = "LOCAL CARTESIAN";
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported, "Unsupported SRS type");
            OSRDestroySpatialReference(hSRS);
            delete poProjectionDict;
            return 0;
        }
    }
    else if( EQUAL(pszProjection, SRS_PT_TRANSVERSE_MERCATOR) )
    {
        int bNorth;
        int nZone = OSRGetUTMZone( hSRS, &bNorth );

        if( nZone != 0 )
        {
            pszProjectionOGCBP = "UT";
            poProjectionDict->Add("Hemisphere", (bNorth) ? "N" : "S");
            poProjectionDict->Add("Zone", nZone);
        }
        else
        {
            double dfCenterLat = poSRS->GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,90.L);
            double dfCenterLong = poSRS->GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
            double dfScale = poSRS->GetNormProjParm(SRS_PP_SCALE_FACTOR,1.0);
            double dfFalseEasting = poSRS->GetNormProjParm(SRS_PP_FALSE_EASTING,0.0);
            double dfFalseNorthing = poSRS->GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0);

            pszProjectionOGCBP = "TC";
            poProjectionDict->Add("OriginLatitude", dfCenterLat);
            poProjectionDict->Add("CentralMeridian", dfCenterLong);
            poProjectionDict->Add("ScaleFactor", dfScale);
            poProjectionDict->Add("FalseEasting", dfFalseEasting);
            poProjectionDict->Add("FalseNorthing", dfFalseNorthing);
        }
    }
    else if( EQUAL(pszProjection,SRS_PT_POLAR_STEREOGRAPHIC) )
    {
        double dfCenterLat = poSRS->GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        double dfCenterLong = poSRS->GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        double dfScale = poSRS->GetNormProjParm(SRS_PP_SCALE_FACTOR,1.0);
        double dfFalseEasting = poSRS->GetNormProjParm(SRS_PP_FALSE_EASTING,0.0);
        double dfFalseNorthing = poSRS->GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0);

        if( fabs(dfCenterLat) == 90.0 && dfCenterLong == 0.0 &&
            dfScale == 0.994 && dfFalseEasting == 200000.0 && dfFalseNorthing == 200000.0)
        {
            pszProjectionOGCBP = "UP";
            poProjectionDict->Add("Hemisphere", (dfCenterLat > 0) ? "N" : "S");
        }
        else
        {
            pszProjectionOGCBP = "PG";
            poProjectionDict->Add("LatitudeTrueScale", dfCenterLat);
            poProjectionDict->Add("LongitudeDownFromPole", dfCenterLong);
            poProjectionDict->Add("ScaleFactor", dfScale);
            poProjectionDict->Add("FalseEasting", dfFalseEasting);
            poProjectionDict->Add("FalseNorthing", dfFalseNorthing);
        }
    }

    else if( EQUAL(pszProjection,SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP))
    {
        double dfStdP1 = poSRS->GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0);
        double dfStdP2 = poSRS->GetNormProjParm(SRS_PP_STANDARD_PARALLEL_2,0.0);
        double dfCenterLat = poSRS->GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        double dfCenterLong = poSRS->GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        double dfFalseEasting = poSRS->GetNormProjParm(SRS_PP_FALSE_EASTING,0.0);
        double dfFalseNorthing = poSRS->GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0);

        pszProjectionOGCBP = "LE";
        poProjectionDict->Add("StandardParallelOne", dfStdP1);
        poProjectionDict->Add("StandardParallelOne", dfStdP2);
        poProjectionDict->Add("OriginLatitude", dfCenterLat);
        poProjectionDict->Add("CentralMeridian", dfCenterLong);
        poProjectionDict->Add("FalseEasting", dfFalseEasting);
        poProjectionDict->Add("FalseNorthing", dfFalseNorthing);
    }
    else
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Unhandled projection type (%s) for now", pszProjection);
    }

    poProjectionDict->Add("ProjectionType", pszProjectionOGCBP);

    if( poSRS->IsProjected() )
    {
        char* pszUnitName = NULL;
        double dfLinearUnits = poSRS->GetLinearUnits(&pszUnitName);
        if (dfLinearUnits == 1.0)
            poProjectionDict->Add("Units", "M");
        else if (dfLinearUnits == 0.3048)
            poProjectionDict->Add("Units", "FT");
    }

    double adfNL[8];
    GDALPDFGeoCoordToNL(adfGeoTransform, nHeight, dfUserUnit,
                        PIXEL_TO_GEO_X(0, 0), PIXEL_TO_GEO_Y(0, 0),
                        adfNL + 0, adfNL + 1);
    GDALPDFGeoCoordToNL(adfGeoTransform, nHeight, dfUserUnit,
                        PIXEL_TO_GEO_X(0, nHeight), PIXEL_TO_GEO_Y(0, nHeight),
                        adfNL + 2, adfNL + 3);
    GDALPDFGeoCoordToNL(adfGeoTransform, nHeight, dfUserUnit,
                        PIXEL_TO_GEO_X(nWidth, nHeight), PIXEL_TO_GEO_Y(nWidth, nHeight),
                        adfNL + 4, adfNL + 5);
    GDALPDFGeoCoordToNL(adfGeoTransform, nHeight, dfUserUnit,
                        PIXEL_TO_GEO_X(nWidth, 0), PIXEL_TO_GEO_Y(nWidth, 0),
                        adfNL + 6, adfNL + 7);

    int nLGIDictId = AllocNewObject();
    StartObj(nLGIDictId);
    GDALPDFDictionaryRW oLGIDict;
    oLGIDict.Add("Type", GDALPDFObjectRW::CreateName("LGIDict"))
            .Add("Version", "2.1")
            .Add("CTM", &((new GDALPDFArrayRW())->Add(adfCTM, 6)))
            .Add("Neatline", &((new GDALPDFArrayRW())->Add(adfNL, 8)));
    if( pszDescription )
    {
        oLGIDict.Add("Description", pszDescription);
    }
    oLGIDict.Add("Projection", poProjectionDict);

    /* GDAL extension */
    if( CSLTestBoolean( CPLGetConfigOption("GDAL_PDF_OGC_BP_WRITE_WKT", "TRUE") ) )
        oLGIDict.Add("WKT", pszWKT);
    VSIFPrintfL(fp, "%s\n", oLGIDict.Serialize().c_str());
    EndObj();

    OSRDestroySpatialReference(hSRS);
    
    return nLGIDictId;
}

/************************************************************************/
/*                     GDALPDFGetValueFromDSOrOption()                  */
/************************************************************************/

static const char* GDALPDFGetValueFromDSOrOption(GDALDataset* poSrcDS,
                                                 char** papszOptions,
                                                 const char* pszKey)
{
    const char* pszValue = CSLFetchNameValue(papszOptions, pszKey);
    if (pszValue != NULL && pszValue[0] == '\0')
        return NULL;
    if (pszValue != NULL)
        return pszValue;
    return poSrcDS->GetMetadataItem(pszKey);
}

/************************************************************************/
/*                             SetInfo()                                */
/************************************************************************/

int GDALPDFWriter::SetInfo(GDALDataset* poSrcDS,
                           char** papszOptions)
{
    const char* pszAUTHOR = GDALPDFGetValueFromDSOrOption(poSrcDS, papszOptions, "AUTHOR");
    const char* pszPRODUCER = GDALPDFGetValueFromDSOrOption(poSrcDS, papszOptions, "PRODUCER");
    const char* pszCREATOR = GDALPDFGetValueFromDSOrOption(poSrcDS, papszOptions, "CREATOR");
    const char* pszCREATION_DATE = GDALPDFGetValueFromDSOrOption(poSrcDS, papszOptions, "CREATION_DATE");
    const char* pszSUBJECT = GDALPDFGetValueFromDSOrOption(poSrcDS, papszOptions, "SUBJECT");
    const char* pszTITLE = GDALPDFGetValueFromDSOrOption(poSrcDS, papszOptions, "TITLE");
    const char* pszKEYWORDS = GDALPDFGetValueFromDSOrOption(poSrcDS, papszOptions, "KEYWORDS");

    if (pszAUTHOR == NULL && pszPRODUCER == NULL && pszCREATOR == NULL && pszCREATION_DATE == NULL &&
        pszSUBJECT == NULL && pszTITLE == NULL && pszKEYWORDS == NULL)
        return 0;

    if (nInfoId == 0)
        nInfoId = AllocNewObject();
    StartObj(nInfoId, nInfoGen);
    GDALPDFDictionaryRW oDict;
    if (pszAUTHOR != NULL)
        oDict.Add("Author", pszAUTHOR);
    if (pszPRODUCER != NULL)
        oDict.Add("Producer", pszPRODUCER);
    if (pszCREATOR != NULL)
        oDict.Add("Creator", pszCREATOR);
    if (pszCREATION_DATE != NULL)
        oDict.Add("CreationDate", pszCREATION_DATE);
    if (pszSUBJECT != NULL)
        oDict.Add("Subject", pszSUBJECT);
    if (pszTITLE != NULL)
        oDict.Add("Title", pszTITLE);
    if (pszKEYWORDS != NULL)
        oDict.Add("Keywords", pszKEYWORDS);
    VSIFPrintfL(fp, "%s\n", oDict.Serialize().c_str());
    EndObj();

    return nInfoId;
}

/************************************************************************/
/*                             SetXMP()                                 */
/************************************************************************/

int  GDALPDFWriter::SetXMP(GDALDataset* poSrcDS,
                           const char* pszXMP)
{
    if (pszXMP != NULL && EQUALN(pszXMP, "NO", 2))
        return 0;
    if (pszXMP != NULL && pszXMP[0] == '\0')
        return 0;

    char** papszXMP = poSrcDS->GetMetadata("xml:XMP");
    if (pszXMP == NULL && papszXMP != NULL && papszXMP[0] != NULL)
        pszXMP = papszXMP[0];

    if (pszXMP == NULL)
        return 0;

    CPLXMLNode* psNode = CPLParseXMLString(pszXMP);
    if (psNode == NULL)
        return 0;
    CPLDestroyXMLNode(psNode);

    if(nXMPId == 0)
        nXMPId = AllocNewObject();
    StartObj(nXMPId, nXMPGen);
    GDALPDFDictionaryRW oDict;
    oDict.Add("Type", GDALPDFObjectRW::CreateName("Metadata"))
         .Add("Subtype", GDALPDFObjectRW::CreateName("XML"))
         .Add("Length", (int)strlen(pszXMP));
    VSIFPrintfL(fp, "%s\n", oDict.Serialize().c_str());
    VSIFPrintfL(fp, "stream\n");
    VSIFPrintfL(fp, "%s\n", pszXMP);
    VSIFPrintfL(fp, "endstream\n");
    EndObj();
    return nXMPId;
}

/************************************************************************/
/*                              WritePage()                             */
/************************************************************************/

int GDALPDFWriter::WritePage(GDALDataset* poSrcDS,
                             double dfDPI,
                             const char* pszGEO_ENCODING,
                             PDFCompressMethod eCompressMethod,
                             int nPredictor,
                             int nJPEGQuality,
                             const char* pszJPEG2000_DRIVER,
                             int nBlockXSize, int nBlockYSize,
                             GDALProgressFunc pfnProgress,
                             void * pProgressData)
{
    int  nWidth = poSrcDS->GetRasterXSize();
    int  nHeight = poSrcDS->GetRasterYSize();
    int  nBands = poSrcDS->GetRasterCount();

    double dfUserUnit = dfDPI / 72.0;
    double dfWidthInUserUnit = nWidth / dfUserUnit;
    double dfHeightInUserUnit = nHeight / dfUserUnit;

    int nPageId = AllocNewObject();
    asPageId.push_back(nPageId);

    int nContentId = AllocNewObject();
    int nContentLengthId = AllocNewObject();
    int nResourcesId = AllocNewObject();

    int bISO32000 = EQUAL(pszGEO_ENCODING, "ISO32000") ||
                    EQUAL(pszGEO_ENCODING, "BOTH");
    int bOGC_BP   = EQUAL(pszGEO_ENCODING, "OGC_BP") ||
                    EQUAL(pszGEO_ENCODING, "BOTH");

    int nViewportId = 0;
    if( bISO32000 )
        nViewportId = WriteSRS_ISO32000(poSrcDS, dfUserUnit);
        
    int nLGIDictId = 0;
    if( bOGC_BP )
        nLGIDictId = WriteSRS_OGC_BP(poSrcDS, dfUserUnit);

    StartObj(nPageId);
    GDALPDFDictionaryRW oDictPage;
    oDictPage.Add("Type", GDALPDFObjectRW::CreateName("Page"))
             .Add("Parent", nPageResourceId, 0)
             .Add("MediaBox", &((new GDALPDFArrayRW())
                               ->Add(0).Add(0).Add(dfWidthInUserUnit).Add(dfHeightInUserUnit)))
             .Add("UserUnit", dfUserUnit)
             .Add("Contents", nContentId, 0)
             .Add("Resources", nResourcesId, 0);

    if (nBands == 4)
    {
        oDictPage.Add("Group",
                      &((new GDALPDFDictionaryRW())
                        ->Add("Type", GDALPDFObjectRW::CreateName("Group"))
                         .Add("S", GDALPDFObjectRW::CreateName("Transparency"))
                         .Add("CS", GDALPDFObjectRW::CreateName("DeviceRGB"))));
    }
    if (nViewportId)
    {
        oDictPage.Add("VP", &((new GDALPDFArrayRW())
                               ->Add(nViewportId, 0)));
    }
    if (nLGIDictId)
    {
        oDictPage.Add("LGIDict", nLGIDictId, 0);
    }
    VSIFPrintfL(fp, "%s\n", oDictPage.Serialize().c_str());
    EndObj();

    /* Does the source image has a color table ? */
    GDALColorTable* poCT = poSrcDS->GetRasterBand(1)->GetColorTable();
    int nColorTableId = 0;
    if (poCT != NULL && poCT->GetColorEntryCount() <= 256)
    {
        int nColors = poCT->GetColorEntryCount();
        nColorTableId = AllocNewObject();

        int nLookupTableId = AllocNewObject();

        /* Index object */
        StartObj(nColorTableId);
        {
            GDALPDFArrayRW oArray;
            oArray.Add(GDALPDFObjectRW::CreateName("Indexed"))
                  .Add(&((new GDALPDFArrayRW())->Add(GDALPDFObjectRW::CreateName("DeviceRGB"))))
                  .Add(nColors-1)
                  .Add(nLookupTableId, 0);
            VSIFPrintfL(fp, "%s\n", oArray.Serialize().c_str());
        }
        EndObj();

        /* Lookup table object */
        StartObj(nLookupTableId);
        {
            GDALPDFDictionaryRW oDict;
            oDict.Add("Length", nColors * 3);
            VSIFPrintfL(fp, "%s %% Lookup table\n", oDict.Serialize().c_str());
        }
        VSIFPrintfL(fp, "stream\n");
        GByte pabyLookup[768];
        for(int i=0;i<nColors;i++)
        {
            const GDALColorEntry* poEntry = poCT->GetColorEntry(i);
            pabyLookup[3 * i + 0] = (GByte)poEntry->c1;
            pabyLookup[3 * i + 1] = (GByte)poEntry->c2;
            pabyLookup[3 * i + 2] = (GByte)poEntry->c3;
        }
        VSIFWriteL(pabyLookup, 3 * nColors, 1, fp);
        VSIFPrintfL(fp, "\n");
        VSIFPrintfL(fp, "endstream\n");
        EndObj();
    }

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
                                      nColorTableId,
                                      eCompressMethod,
                                      nPredictor,
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
    {
        GDALPDFDictionaryRW oDict;
        oDict.Add("Length", nContentLengthId, 0);
        VSIFPrintfL(fp, "%s\n", oDict.Serialize().c_str());
    }
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
            VSIFPrintfL(fp, "%.16f 0 0 %.16f %.16f %.16f cm\n",
                        ROUND_TO_INT_IF_CLOSE(nReqWidth / dfUserUnit),
                        ROUND_TO_INT_IF_CLOSE(nReqHeight / dfUserUnit),
                        ROUND_TO_INT_IF_CLOSE((nBlockXOff * nBlockXSize) / dfUserUnit),
                        ROUND_TO_INT_IF_CLOSE((nHeight - nBlockYOff * nBlockYSize - nReqHeight) / dfUserUnit));
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
    {
        GDALPDFDictionaryRW oDict;
        GDALPDFDictionaryRW* poDict2 = new GDALPDFDictionaryRW();
        oDict.Add("XObject", poDict2);
        for(size_t iImage = 0; iImage < asImageId.size(); iImage ++)
        {
            poDict2->Add(CPLSPrintf("Image%d", asImageId[iImage]), asImageId[iImage], 0);
        }
        VSIFPrintfL(fp, "%s\n", oDict.Serialize().c_str());
    }
    EndObj();

    return TRUE;
}

/************************************************************************/
/*                             WriteMask()                              */
/************************************************************************/

int GDALPDFWriter::WriteMask(GDALDataset* poSrcDS,
                             int nXOff, int nYOff, int nReqXSize, int nReqYSize,
                             PDFCompressMethod eCompressMethod)
{
    int nMaskSize = nReqXSize * nReqYSize;
    GByte* pabyMask = (GByte*)VSIMalloc(nMaskSize);
    if (pabyMask == NULL)
        return 0;

    CPLErr eErr;
    eErr = poSrcDS->GetRasterBand(4)->RasterIO(
            GF_Read,
            nXOff, nYOff,
            nReqXSize, nReqYSize,
            pabyMask, nReqXSize, nReqYSize, GDT_Byte,
            0, 0);
    if (eErr != CE_None)
    {
        VSIFree(pabyMask);
        return 0;
    }

    int bOnly0or255 = TRUE;
    int bOnly255 = TRUE;
    int bOnly0 = TRUE;
    int i;
    for(i=0;i<nReqXSize * nReqYSize;i++)
    {
        if (pabyMask[i] == 0)
            bOnly255 = FALSE;
        else if (pabyMask[i] == 255)
            bOnly0 = FALSE;
        else
        {
            bOnly0or255 = FALSE;
            break;
        }
    }

    if (bOnly255)
    {
        CPLFree(pabyMask);
        return 0;
    }

    if (bOnly0or255)
    {
        /* Translate to 1 bit */
        int nReqXSize1 = (nReqXSize + 7) / 8;
        GByte* pabyMask1 = (GByte*)VSICalloc(nReqXSize1, nReqYSize);
        if (pabyMask1 == NULL)
        {
            CPLFree(pabyMask);
            return 0;
        }
        for(int y=0;y<nReqYSize;y++)
        {
            for(int x=0;x<nReqXSize;x++)
            {
                if (pabyMask[y * nReqXSize + x])
                    pabyMask1[y * nReqXSize1 + x / 8] |= 1 << (7 - (x % 8));
            }
        }
        VSIFree(pabyMask);
        pabyMask = pabyMask1;
        nMaskSize = nReqXSize1 * nReqYSize;
    }

    int nMaskId = AllocNewObject();
    int nMaskLengthId = AllocNewObject();

    StartObj(nMaskId);
    GDALPDFDictionaryRW oDict;
    oDict.Add("Length", nMaskLengthId, 0)
         .Add("Type", GDALPDFObjectRW::CreateName("XObject"));
    if( eCompressMethod != COMPRESS_NONE )
    {
        oDict.Add("Filter", GDALPDFObjectRW::CreateName("FlateDecode"));
    }
    oDict.Add("Subtype", GDALPDFObjectRW::CreateName("Image"))
         .Add("Width", nReqXSize)
         .Add("Height", nReqYSize)
         .Add("ColorSpace", GDALPDFObjectRW::CreateName("DeviceGray"))
         .Add("BitsPerComponent", (bOnly0or255) ? 1 : 8);
    VSIFPrintfL(fp, "%s\n", oDict.Serialize().c_str());
    VSIFPrintfL(fp, "stream\n");
    vsi_l_offset nStreamStart = VSIFTellL(fp);

    VSILFILE* fpGZip = NULL;
    VSILFILE* fpBack = fp;
    if( eCompressMethod != COMPRESS_NONE )
    {
        fpGZip = (VSILFILE* )VSICreateGZipWritable( (VSIVirtualHandle*) fp, TRUE, FALSE );
        fp = fpGZip;
    }

    VSIFWriteL(pabyMask, nMaskSize, 1, fp);
    CPLFree(pabyMask);

    if (fpGZip)
        VSIFCloseL(fpGZip);

    fp = fpBack;

    vsi_l_offset nStreamEnd = VSIFTellL(fp);
    VSIFPrintfL(fp,
                "\n"
                "endstream\n");
    EndObj();

    StartObj(nMaskLengthId);
    VSIFPrintfL(fp,
                "   %ld\n",
                (long)(nStreamEnd - nStreamStart));
    EndObj();

    return nMaskId;
}

/************************************************************************/
/*                             WriteBlock()                             */
/************************************************************************/

int GDALPDFWriter::WriteBlock(GDALDataset* poSrcDS,
                             int nXOff, int nYOff, int nReqXSize, int nReqYSize,
                             int nColorTableId,
                             PDFCompressMethod eCompressMethod,
                             int nPredictor,
                             int nJPEGQuality,
                             const char* pszJPEG2000_DRIVER,
                             GDALProgressFunc pfnProgress,
                             void * pProgressData)
{
    int  nBands = poSrcDS->GetRasterCount();

    CPLErr eErr = CE_None;
    GDALDataset* poBlockSrcDS = NULL;
    GDALDatasetH hMemDS = NULL;
    GByte* pabyMEMDSBuffer = NULL;

    int nMaskId = 0;
    if (nBands == 4)
    {
        nMaskId = WriteMask(poSrcDS,
                            nXOff, nYOff, nReqXSize, nReqYSize,
                            eCompressMethod);
    }

    if( nReqXSize == poSrcDS->GetRasterXSize() &&
        nReqYSize == poSrcDS->GetRasterYSize() &&
        nBands != 4)
    {
        poBlockSrcDS = poSrcDS;
    }
    else
    {
        if (nBands == 4)
            nBands = 3;

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

    GDALPDFDictionaryRW oDict;
    oDict.Add("Length", nImageLengthId, 0)
         .Add("Type", GDALPDFObjectRW::CreateName("XObject"));

    if( eCompressMethod == COMPRESS_DEFLATE )
    {
        oDict.Add("Filter", GDALPDFObjectRW::CreateName("FlateDecode"));
        if( nPredictor == 2 )
            oDict.Add("Filter", &((new GDALPDFDictionaryRW())
                                  ->Add("Predictor", 2)
                                   .Add("Colors", nBands)
                                   .Add("Columns", nReqXSize)));
    }
    else if( eCompressMethod == COMPRESS_JPEG )
    {
        oDict.Add("Filter", GDALPDFObjectRW::CreateName("DCTDecode"));
    }
    else if( eCompressMethod == COMPRESS_JPEG2000 )
    {
        oDict.Add("Filter", GDALPDFObjectRW::CreateName("JPXDecode"));
    }

    oDict.Add("Subtype", GDALPDFObjectRW::CreateName("Image"))
         .Add("Width", nReqXSize)
         .Add("Height", nReqYSize)
         .Add("ColorSpace",
              (nColorTableId != 0) ? GDALPDFObjectRW::CreateIndirect(nColorTableId, 0) :
              (nBands == 1) ?        GDALPDFObjectRW::CreateName("DeviceGray") :
                                     GDALPDFObjectRW::CreateName("DeviceRGB"))
         .Add("BitsPerComponent", 8);
    if( nMaskId )
    {
        oDict.Add("SMask", nMaskId, 0);
    }
    VSIFPrintfL(fp, "%s\n", oDict.Serialize().c_str());
    VSIFPrintfL(fp, "stream\n");

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
            eErr = CE_Failure;
            goto end;
        }

        GDALDataset* poJPEGDS = NULL;

        poJPEGDS = poJPEGDriver->CreateCopy(szTmp, poBlockSrcDS,
                                            FALSE, papszOptions,
                                            pfnProgress, pProgressData);

        CSLDestroy(papszOptions);
        if( poJPEGDS == NULL )
        {
            eErr = CE_Failure;
            goto end;
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

            /* Apply predictor if needed */
            if( nPredictor == 2 )
            {
                if( nBands == 1 )
                {
                    int nPrevValue = pabyLine[0];
                    for(int iPixel = 1; iPixel < nReqXSize; iPixel ++)
                    {
                        int nCurValue = pabyLine[iPixel];
                        pabyLine[iPixel] = (GByte) (nCurValue - nPrevValue);
                        nPrevValue = nCurValue;
                    }
                }
                else if( nBands == 3 )
                {
                    int nPrevValueR = pabyLine[0];
                    int nPrevValueG = pabyLine[1];
                    int nPrevValueB = pabyLine[2];
                    for(int iPixel = 1; iPixel < nReqXSize; iPixel ++)
                    {
                        int nCurValueR = pabyLine[3 * iPixel + 0];
                        int nCurValueG = pabyLine[3 * iPixel + 1];
                        int nCurValueB = pabyLine[3 * iPixel + 2];
                        pabyLine[3 * iPixel + 0] = (GByte) (nCurValueR - nPrevValueR);
                        pabyLine[3 * iPixel + 1] = (GByte) (nCurValueG - nPrevValueG);
                        pabyLine[3 * iPixel + 2] = (GByte) (nCurValueB - nPrevValueB);
                        nPrevValueR = nCurValueR;
                        nPrevValueG = nCurValueG;
                        nPrevValueB = nCurValueB;
                    }
                }
            }

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

end:
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
    {
        GDALPDFDictionaryRW oDict;
        GDALPDFArrayRW* poKids = new GDALPDFArrayRW();
        oDict.Add("Type", GDALPDFObjectRW::CreateName("Pages"))
             .Add("Count", (int)asPageId.size())
             .Add("Kids", poKids);

        for(size_t i=0;i<asPageId.size();i++)
            poKids->Add(asPageId[i], 0);

        VSIFPrintfL(fp, "%s\n", oDict.Serialize().c_str());
    }
    EndObj();

    StartObj(nCatalogId);
    {
        GDALPDFDictionaryRW oDict;
        oDict.Add("Type", GDALPDFObjectRW::CreateName("Catalog"))
             .Add("Pages", nPageResourceId, 0);
        if (nXMPId)
            oDict.Add("Metadata", nXMPId, 0);
        VSIFPrintfL(fp, "%s\n", oDict.Serialize().c_str());
    }
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
                  "PDF driver doesn't support %d bands.  Must be 1 (grey or with color table), "
                  "3 (RGB) or 4 bands.\n", nBands );

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

    if (nBands == 1 &&
        poSrcDS->GetRasterBand(1)->GetColorTable() != NULL &&
        (eCompressMethod == COMPRESS_JPEG || eCompressMethod == COMPRESS_JPEG2000))
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "The source raster band has a color table, which is not appropriate with JPEG or JPEG2000 compression.\n"
                  "You should rather consider using color table expansion (-expand option in gdal_translate)");
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
    
    const char* pszGEO_ENCODING =
        CSLFetchNameValueDef(papszOptions, "GEO_ENCODING", "ISO32000");

    const char* pszXMP = CSLFetchNameValue(papszOptions, "XMP");

    double dfDPI = atof(CSLFetchNameValueDef(papszOptions, "DPI", "72"));
    if (dfDPI < 72.0)
        dfDPI = 72.0;

    const char* pszPredictor = CSLFetchNameValue(papszOptions, "PREDICTOR");
    int nPredictor = 1;
    if (pszPredictor)
    {
        if (eCompressMethod != COMPRESS_DEFLATE)
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "PREDICTOR option is only taken into account for DEFLATE compression");
        }
        else
        {
            nPredictor = atoi(pszPredictor);
            if (nPredictor != 1 && nPredictor != 2)
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                                    "Supported PREDICTOR values are 1 or 2");
                nPredictor = 1;
            }
        }
    }

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

    if( CSLFetchBoolean(papszOptions, "WRITE_INFO", TRUE) )
        oWriter.SetInfo(poSrcDS, papszOptions);
    oWriter.SetXMP(poSrcDS, pszXMP);

    int bRet = oWriter.WritePage(poSrcDS,
                                 dfDPI,
                                 pszGEO_ENCODING,
                                 eCompressMethod,
                                 nPredictor,
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
