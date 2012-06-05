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
#include "ogr_spatialref.h"
#include "ogr_geometry.h"
#include "vrt/vrtdataset.h"

#include "pdfobject.h"

#ifndef M_PI
#define M_PI       3.14159265358979323846
#endif

CPL_CVSID("$Id$");

#define PIXEL_TO_GEO_X(x,y) adfGeoTransform[0] + x * adfGeoTransform[1] + y * adfGeoTransform[2]
#define PIXEL_TO_GEO_Y(x,y) adfGeoTransform[3] + x * adfGeoTransform[4] + y * adfGeoTransform[5]

class GDALFakePDFDataset : public GDALDataset
{
    public:
        GDALFakePDFDataset() {}
};

/************************************************************************/
/*                             Init()                                   */
/************************************************************************/

void GDALPDFWriter::Init()
{
    nPageResourceId = 0;
    nStructTreeRootId = 0;
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

    PDFMargins sMargins = {0, 0, 0, 0};

    const char* pszGEO_ENCODING = CPLGetConfigOption("GDAL_PDF_GEO_ENCODING", "ISO32000");
    if (EQUAL(pszGEO_ENCODING, "ISO32000") || EQUAL(pszGEO_ENCODING, "BOTH"))
        nViewportId = WriteSRS_ISO32000(poSrcDS, dfDPI / 72.0, NULL, &sMargins);
    if (EQUAL(pszGEO_ENCODING, "OGC_BP") || EQUAL(pszGEO_ENCODING, "BOTH"))
        nLGIDictId = WriteSRS_OGC_BP(poSrcDS, dfDPI / 72.0, NULL, &sMargins);

#ifdef invalidate_xref_entry
    GDALPDFObject* poVP = poPageDict->Get("VP");
    if (poVP)
    {
        if (poVP->GetType() == PDFObjectType_Array &&
            poVP->GetArray()->GetLength() == 1)
            poVP = poVP->GetArray()->Get(0);

        int nVPId = poVP->GetRefNum();
        if (nVPId)
        {
            asXRefEntries[nVPId - 1].bFree = TRUE;
            asXRefEntries[nVPId - 1].nGen ++;
        }
    }
#endif

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
#ifdef invalidate_xref_entry
        asXRefEntries[nInfoId - 1].bFree = TRUE;
        asXRefEntries[nInfoId - 1].nGen ++;
#else
        StartObj(nInfoId, nInfoGen);
        VSIFPrintfL(fp, "<< >>\n");
        EndObj();
#endif
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
            if (asXRefEntries[i].nOffset != 0 || asXRefEntries[i].bFree)
            {
                /* Find number of consecutive objects */
                size_t nCount = 1;
                while(i + nCount <asXRefEntries.size() &&
                    (asXRefEntries[i + nCount].nOffset != 0 || asXRefEntries[i + nCount].bFree))
                    nCount ++;

                VSIFPrintfL(fp, "%d %d\n", (int)i + 1, (int)nCount);
                size_t iEnd = i + nCount;
                for(; i < iEnd; i++)
                {
                    snprintf (buffer, sizeof(buffer), "%010ld", (long)asXRefEntries[i].nOffset);
                    VSIFPrintfL(fp, "%s %05d %c \n",
                                buffer, asXRefEntries[i].nGen,
                                asXRefEntries[i].bFree ? 'f' : 'n');
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
/*                         GDALPDFFind4Corners()                        */
/************************************************************************/

static
void GDALPDFFind4Corners(const GDAL_GCP* pasGCPList,
                         int& iUL, int& iUR, int& iLR, int& iLL)
{
    double dfMeanX = 0, dfMeanY = 0;
    int i;

    iUL = 0;
    iUR = 0;
    iLR = 0;
    iLL = 0;

    for(i = 0; i < 4; i++ )
    {
        dfMeanX += pasGCPList[i].dfGCPPixel;
        dfMeanY += pasGCPList[i].dfGCPLine;
    }
    dfMeanX /= 4;
    dfMeanY /= 4;

    for(i = 0; i < 4; i++ )
    {
        if (pasGCPList[i].dfGCPPixel < dfMeanX &&
            pasGCPList[i].dfGCPLine  < dfMeanY )
            iUL = i;

        else if (pasGCPList[i].dfGCPPixel > dfMeanX &&
                    pasGCPList[i].dfGCPLine  < dfMeanY )
            iUR = i;

        else if (pasGCPList[i].dfGCPPixel > dfMeanX &&
                    pasGCPList[i].dfGCPLine  > dfMeanY )
            iLR = i;

        else if (pasGCPList[i].dfGCPPixel < dfMeanX &&
                    pasGCPList[i].dfGCPLine  > dfMeanY )
            iLL = i;
    }
}

/************************************************************************/
/*                         WriteSRS_ISO32000()                          */
/************************************************************************/

int  GDALPDFWriter::WriteSRS_ISO32000(GDALDataset* poSrcDS,
                                      double dfUserUnit,
                                      const char* pszNEATLINE,
                                      PDFMargins* psMargins)
{
    int  nWidth = poSrcDS->GetRasterXSize();
    int  nHeight = poSrcDS->GetRasterYSize();
    const char* pszWKT = poSrcDS->GetProjectionRef();
    double adfGeoTransform[6];

    int bHasGT = (poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None);
    const GDAL_GCP* pasGCPList = (poSrcDS->GetGCPCount() == 4) ? poSrcDS->GetGCPs() : NULL;
    if (pasGCPList != NULL)
        pszWKT = poSrcDS->GetGCPProjection();

    if( !bHasGT && pasGCPList == NULL )
        return 0;

    if( pszWKT == NULL || EQUAL(pszWKT, "") )
        return 0;

    double adfGPTS[8];

    double dfULPixel = 0;
    double dfULLine = 0;
    double dfLRPixel = nWidth;
    double dfLRLine = nHeight;

    GDAL_GCP asNeatLineGCPs[4];
    if (pszNEATLINE == NULL)
        pszNEATLINE = poSrcDS->GetMetadataItem("NEATLINE");
    if( bHasGT && pszNEATLINE != NULL && pszNEATLINE[0] != '\0' )
    {
        OGRGeometry* poGeom = NULL;
        OGRGeometryFactory::createFromWkt( (char**)&pszNEATLINE, NULL, &poGeom );
        if ( poGeom != NULL && wkbFlatten(poGeom->getGeometryType()) == wkbPolygon )
        {
            OGRLineString* poLS = ((OGRPolygon*)poGeom)->getExteriorRing();
            double adfGeoTransformInv[6];
            if( poLS != NULL && poLS->getNumPoints() == 5 &&
                GDALInvGeoTransform(adfGeoTransform, adfGeoTransformInv) )
            {
                for(int i=0;i<4;i++)
                {
                    double X = asNeatLineGCPs[i].dfGCPX = poLS->getX(i);
                    double Y = asNeatLineGCPs[i].dfGCPY = poLS->getY(i);
                    double x = adfGeoTransformInv[0] + X * adfGeoTransformInv[1] + Y * adfGeoTransformInv[2];
                    double y = adfGeoTransformInv[3] + X * adfGeoTransformInv[4] + Y * adfGeoTransformInv[5];
                    asNeatLineGCPs[i].dfGCPPixel = x;
                    asNeatLineGCPs[i].dfGCPLine = y;
                }

                int iUL = 0, iUR = 0, iLR = 0, iLL = 0;
                GDALPDFFind4Corners(asNeatLineGCPs,
                                    iUL,iUR, iLR, iLL);

                if (fabs(asNeatLineGCPs[iUL].dfGCPPixel - asNeatLineGCPs[iLL].dfGCPPixel) > .5 ||
                    fabs(asNeatLineGCPs[iUR].dfGCPPixel - asNeatLineGCPs[iLR].dfGCPPixel) > .5 ||
                    fabs(asNeatLineGCPs[iUL].dfGCPLine - asNeatLineGCPs[iUR].dfGCPLine) > .5 ||
                    fabs(asNeatLineGCPs[iLL].dfGCPLine - asNeatLineGCPs[iLR].dfGCPLine) > .5)
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                            "Neatline coordinates should form a rectangle in pixel space. Ignoring it");
                    for(int i=0;i<4;i++)
                    {
                        CPLDebug("PDF", "pixel[%d] = %.1f, line[%d] = %.1f",
                                i, asNeatLineGCPs[i].dfGCPPixel,
                                i, asNeatLineGCPs[i].dfGCPLine);
                    }
                }
                else
                {
                    pasGCPList = asNeatLineGCPs;
                }
            }
        }
        delete poGeom;
    }

    if( pasGCPList )
    {
        int iUL = 0, iUR = 0, iLR = 0, iLL = 0;
        GDALPDFFind4Corners(pasGCPList,
                            iUL,iUR, iLR, iLL);

        if (fabs(pasGCPList[iUL].dfGCPPixel - pasGCPList[iLL].dfGCPPixel) > .5 ||
            fabs(pasGCPList[iUR].dfGCPPixel - pasGCPList[iLR].dfGCPPixel) > .5 ||
            fabs(pasGCPList[iUL].dfGCPLine - pasGCPList[iUR].dfGCPLine) > .5 ||
            fabs(pasGCPList[iLL].dfGCPLine - pasGCPList[iLR].dfGCPLine) > .5)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "GCPs should form a rectangle in pixel space");
            return 0;
        }
        
        dfULPixel = pasGCPList[iUL].dfGCPPixel;
        dfULLine = pasGCPList[iUL].dfGCPLine;
        dfLRPixel = pasGCPList[iLR].dfGCPPixel;
        dfLRLine = pasGCPList[iLR].dfGCPLine;
        
        /* Upper-left */
        adfGPTS[0] = pasGCPList[iUL].dfGCPX;
        adfGPTS[1] = pasGCPList[iUL].dfGCPY;
        
        /* Lower-left */
        adfGPTS[2] = pasGCPList[iLL].dfGCPX;
        adfGPTS[3] = pasGCPList[iLL].dfGCPY;
        
        /* Lower-right */
        adfGPTS[4] = pasGCPList[iLR].dfGCPX;
        adfGPTS[5] = pasGCPList[iLR].dfGCPY;
        
        /* Upper-right */
        adfGPTS[6] = pasGCPList[iUR].dfGCPX;
        adfGPTS[7] = pasGCPList[iUR].dfGCPY;
    }
    else
    {
        /* Upper-left */
        adfGPTS[0] = PIXEL_TO_GEO_X(0, 0);
        adfGPTS[1] = PIXEL_TO_GEO_Y(0, 0);

        /* Lower-left */
        adfGPTS[2] = PIXEL_TO_GEO_X(0, nHeight);
        adfGPTS[3] = PIXEL_TO_GEO_Y(0, nHeight);

        /* Lower-right */
        adfGPTS[4] = PIXEL_TO_GEO_X(nWidth, nHeight);
        adfGPTS[5] = PIXEL_TO_GEO_Y(nWidth, nHeight);

        /* Upper-right */
        adfGPTS[6] = PIXEL_TO_GEO_X(nWidth, 0);
        adfGPTS[7] = PIXEL_TO_GEO_Y(nWidth, 0);
    }
    
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

    int bSuccess = TRUE;
    
    bSuccess &= (OCTTransform( hCT, 1, adfGPTS + 0, adfGPTS + 1, NULL ) == 1);
    bSuccess &= (OCTTransform( hCT, 1, adfGPTS + 2, adfGPTS + 3, NULL ) == 1);
    bSuccess &= (OCTTransform( hCT, 1, adfGPTS + 4, adfGPTS + 5, NULL ) == 1);
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
                                ->Add(dfULPixel / dfUserUnit + psMargins->nLeft)
                                .Add((nHeight - dfLRLine) / dfUserUnit + psMargins->nBottom)
                                .Add(dfLRPixel / dfUserUnit + psMargins->nLeft)
                                .Add((nHeight - dfULLine) / dfUserUnit + psMargins->nBottom)))
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
/*                     GDALPDFBuildOGC_BP_Datum()                       */
/************************************************************************/

static GDALPDFObject* GDALPDFBuildOGC_BP_Datum(const OGRSpatialReference* poSRS)
{
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
                         .Add("SemiMajorAxis", dfSemiMajor, TRUE)
                         .Add("InvFlattening", dfInvFlattening, TRUE)));
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
                         .Add("dz", poTOWGS84->GetChild(2)->GetValue())) );
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
                         .Add("sf", poTOWGS84->GetChild(6)->GetValue())) );
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

    return poPDFDatum;
}

/************************************************************************/
/*                   GDALPDFBuildOGC_BP_Projection()                    */
/************************************************************************/

static GDALPDFDictionaryRW* GDALPDFBuildOGC_BP_Projection(const OGRSpatialReference* poSRS)
{

    const char* pszProjectionOGCBP = "GEOGRAPHIC";
    const char *pszProjection = poSRS->GetAttrValue("PROJECTION");

    GDALPDFDictionaryRW* poProjectionDict = new GDALPDFDictionaryRW();
    poProjectionDict->Add("Type", GDALPDFObjectRW::CreateName("Projection"));
    poProjectionDict->Add("Datum", GDALPDFBuildOGC_BP_Datum(poSRS));

    if( pszProjection == NULL )
    {
        if( poSRS->IsGeographic() )
            pszProjectionOGCBP = "GEOGRAPHIC";
        else if( poSRS->IsLocal() )
            pszProjectionOGCBP = "LOCAL CARTESIAN";
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported, "Unsupported SRS type");
            delete poProjectionDict;
            return NULL;
        }
    }
    else if( EQUAL(pszProjection, SRS_PT_TRANSVERSE_MERCATOR) )
    {
        int bNorth;
        int nZone = poSRS->GetUTMZone( &bNorth );

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

            /* OGC_BP supports representing numbers as strings for better precision */
            /* so use it */

            pszProjectionOGCBP = "TC";
            poProjectionDict->Add("OriginLatitude", dfCenterLat, TRUE);
            poProjectionDict->Add("CentralMeridian", dfCenterLong, TRUE);
            poProjectionDict->Add("ScaleFactor", dfScale, TRUE);
            poProjectionDict->Add("FalseEasting", dfFalseEasting, TRUE);
            poProjectionDict->Add("FalseNorthing", dfFalseNorthing, TRUE);
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
            poProjectionDict->Add("LatitudeTrueScale", dfCenterLat, TRUE);
            poProjectionDict->Add("LongitudeDownFromPole", dfCenterLong, TRUE);
            poProjectionDict->Add("ScaleFactor", dfScale, TRUE);
            poProjectionDict->Add("FalseEasting", dfFalseEasting, TRUE);
            poProjectionDict->Add("FalseNorthing", dfFalseNorthing, TRUE);
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
        poProjectionDict->Add("StandardParallelOne", dfStdP1, TRUE);
        poProjectionDict->Add("StandardParallelTwo", dfStdP2, TRUE);
        poProjectionDict->Add("OriginLatitude", dfCenterLat, TRUE);
        poProjectionDict->Add("CentralMeridian", dfCenterLong, TRUE);
        poProjectionDict->Add("FalseEasting", dfFalseEasting, TRUE);
        poProjectionDict->Add("FalseNorthing", dfFalseNorthing, TRUE);
    }

    else if( EQUAL(pszProjection,SRS_PT_MERCATOR_1SP) )
    {
        double dfCenterLong = poSRS->GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        double dfCenterLat = poSRS->GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        double dfScale = poSRS->GetNormProjParm(SRS_PP_SCALE_FACTOR,1.0);
        double dfFalseEasting = poSRS->GetNormProjParm(SRS_PP_FALSE_EASTING,0.0);
        double dfFalseNorthing = poSRS->GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0);

        pszProjectionOGCBP = "MC";
        poProjectionDict->Add("CentralMeridian", dfCenterLong, TRUE);
        poProjectionDict->Add("OriginLatitude", dfCenterLat, TRUE);
        poProjectionDict->Add("ScaleFactor", dfScale, TRUE);
        poProjectionDict->Add("FalseEasting", dfFalseEasting, TRUE);
        poProjectionDict->Add("FalseNorthing", dfFalseNorthing, TRUE);
    }

#ifdef not_supported
    else if( EQUAL(pszProjection,SRS_PT_MERCATOR_2SP) )
    {
        double dfStdP1 = poSRS->GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0);
        double dfCenterLong = poSRS->GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        double dfFalseEasting = poSRS->GetNormProjParm(SRS_PP_FALSE_EASTING,0.0);
        double dfFalseNorthing = poSRS->GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0);

        pszProjectionOGCBP = "MC";
        poProjectionDict->Add("StandardParallelOne", dfStdP1, TRUE);
        poProjectionDict->Add("CentralMeridian", dfCenterLong, TRUE);
        poProjectionDict->Add("FalseEasting", dfFalseEasting, TRUE);
        poProjectionDict->Add("FalseNorthing", dfFalseNorthing, TRUE);
    }
#endif

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

    return poProjectionDict;
}

/************************************************************************/
/*                           WriteSRS_OGC_BP()                          */
/************************************************************************/

int GDALPDFWriter::WriteSRS_OGC_BP(GDALDataset* poSrcDS,
                                   double dfUserUnit,
                                   const char* pszNEATLINE,
                                   PDFMargins* psMargins)
{
    int  nWidth = poSrcDS->GetRasterXSize();
    int  nHeight = poSrcDS->GetRasterYSize();
    const char* pszWKT = poSrcDS->GetProjectionRef();
    double adfGeoTransform[6];

    int bHasGT = (poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None);
    int nGCPCount = poSrcDS->GetGCPCount();
    const GDAL_GCP* pasGCPList = (nGCPCount >= 4) ? poSrcDS->GetGCPs() : NULL;
    if (pasGCPList != NULL)
        pszWKT = poSrcDS->GetGCPProjection();

    if( !bHasGT && pasGCPList == NULL )
        return 0;

    if( pszWKT == NULL || EQUAL(pszWKT, "") )
        return 0;
    
    if( !bHasGT )
    {
        if (!GDALGCPsToGeoTransform( nGCPCount, pasGCPList,
                                     adfGeoTransform, FALSE ))
        {
            CPLDebug("PDF", "Could not compute GT with exact match. Writing Registration then");
        }
        else
        {
            bHasGT = TRUE;
        }
    }

    OGRSpatialReferenceH hSRS = OSRNewSpatialReference(pszWKT);
    if( hSRS == NULL )
        return 0;

    const OGRSpatialReference* poSRS = (const OGRSpatialReference*)hSRS;
    GDALPDFDictionaryRW* poProjectionDict = GDALPDFBuildOGC_BP_Projection(poSRS);
    if (poProjectionDict == NULL)
    {
        OSRDestroySpatialReference(hSRS);
        return 0;
    }

    GDALPDFArrayRW* poNeatLineArray = NULL;

    if (pszNEATLINE == NULL)
        pszNEATLINE = poSrcDS->GetMetadataItem("NEATLINE");
    if( bHasGT && pszNEATLINE != NULL && !EQUAL(pszNEATLINE, "NO") && pszNEATLINE[0] != '\0' )
    {
        OGRGeometry* poGeom = NULL;
        OGRGeometryFactory::createFromWkt( (char**)&pszNEATLINE, NULL, &poGeom );
        if ( poGeom != NULL && wkbFlatten(poGeom->getGeometryType()) == wkbPolygon )
        {
            OGRLineString* poLS = ((OGRPolygon*)poGeom)->getExteriorRing();
            double adfGeoTransformInv[6];
            if( poLS != NULL && poLS->getNumPoints() >= 5 &&
                GDALInvGeoTransform(adfGeoTransform, adfGeoTransformInv) )
            {
                poNeatLineArray = new GDALPDFArrayRW();

                 // FIXME : ensure that they are in clockwise order ?
                for(int i=0;i<poLS->getNumPoints() - 1;i++)
                {
                    double X = poLS->getX(i);
                    double Y = poLS->getY(i);
                    double x = adfGeoTransformInv[0] + X * adfGeoTransformInv[1] + Y * adfGeoTransformInv[2];
                    double y = adfGeoTransformInv[3] + X * adfGeoTransformInv[4] + Y * adfGeoTransformInv[5];
                    poNeatLineArray->Add(x / dfUserUnit + psMargins->nLeft, TRUE);
                    poNeatLineArray->Add((nHeight - y) / dfUserUnit + psMargins->nBottom, TRUE);
                }
            }
        }
        delete poGeom;
    }

    if( pszNEATLINE != NULL && EQUAL(pszNEATLINE, "NO") )
    {
        // Do nothing
    }
    else if( pasGCPList && poNeatLineArray == NULL)
    {
        if (nGCPCount == 4)
        {
            int iUL = 0, iUR = 0, iLR = 0, iLL = 0;
            GDALPDFFind4Corners(pasGCPList,
                                iUL,iUR, iLR, iLL);

            double adfNL[8];
            adfNL[0] = pasGCPList[iUL].dfGCPPixel / dfUserUnit + psMargins->nLeft;
            adfNL[1] = (nHeight - pasGCPList[iUL].dfGCPLine) / dfUserUnit + psMargins->nBottom;
            adfNL[2] = pasGCPList[iLL].dfGCPPixel / dfUserUnit + psMargins->nLeft;
            adfNL[3] = (nHeight - pasGCPList[iLL].dfGCPLine) / dfUserUnit + psMargins->nBottom;
            adfNL[4] = pasGCPList[iLR].dfGCPPixel / dfUserUnit + psMargins->nLeft;
            adfNL[5] = (nHeight - pasGCPList[iLR].dfGCPLine) / dfUserUnit + psMargins->nBottom;
            adfNL[6] = pasGCPList[iUR].dfGCPPixel / dfUserUnit + psMargins->nLeft;
            adfNL[7] = (nHeight - pasGCPList[iUR].dfGCPLine) / dfUserUnit + psMargins->nBottom;

            poNeatLineArray = new GDALPDFArrayRW();
            poNeatLineArray->Add(adfNL, 8, TRUE);
        }
        else
        {
            poNeatLineArray = new GDALPDFArrayRW();

            // FIXME : ensure that they are in clockwise order ?
            int i;
            for(i = 0; i < nGCPCount; i++)
            {
                poNeatLineArray->Add(pasGCPList[i].dfGCPPixel / dfUserUnit + psMargins->nLeft, TRUE);
                poNeatLineArray->Add((nHeight - pasGCPList[i].dfGCPLine) / dfUserUnit + psMargins->nBottom, TRUE);
            }
        }
    }
    else if (poNeatLineArray == NULL)
    {
        poNeatLineArray = new GDALPDFArrayRW();

        poNeatLineArray->Add(0 / dfUserUnit + psMargins->nLeft, TRUE);
        poNeatLineArray->Add((nHeight - 0) / dfUserUnit + psMargins->nBottom, TRUE);

        poNeatLineArray->Add(0 / dfUserUnit + psMargins->nLeft, TRUE);
        poNeatLineArray->Add((nHeight -nHeight) / dfUserUnit + psMargins->nBottom, TRUE);

        poNeatLineArray->Add(nWidth / dfUserUnit + psMargins->nLeft, TRUE);
        poNeatLineArray->Add((nHeight -nHeight) / dfUserUnit + psMargins->nBottom, TRUE);

        poNeatLineArray->Add(nWidth / dfUserUnit + psMargins->nLeft, TRUE);
        poNeatLineArray->Add((nHeight - 0) / dfUserUnit + psMargins->nBottom, TRUE);
    }

    int nLGIDictId = AllocNewObject();
    StartObj(nLGIDictId);
    GDALPDFDictionaryRW oLGIDict;
    oLGIDict.Add("Type", GDALPDFObjectRW::CreateName("LGIDict"))
            .Add("Version", "2.1");
    if( bHasGT )
    {
        double adfCTM[6];
        double dfX1 = psMargins->nLeft;
        double dfY2 = nHeight / dfUserUnit + psMargins->nBottom ;

        adfCTM[0] = adfGeoTransform[1] * dfUserUnit;
        adfCTM[1] = adfGeoTransform[2] * dfUserUnit;
        adfCTM[2] = - adfGeoTransform[4] * dfUserUnit;
        adfCTM[3] = - adfGeoTransform[5] * dfUserUnit;
        adfCTM[4] = adfGeoTransform[0] - (adfCTM[0] * dfX1 + adfCTM[2] * dfY2);
        adfCTM[5] = adfGeoTransform[3] - (adfCTM[1] * dfX1 + adfCTM[3] * dfY2);

        oLGIDict.Add("CTM", &((new GDALPDFArrayRW())->Add(adfCTM, 6, TRUE)));
    }
    else
    {
        GDALPDFArrayRW* poRegistrationArray = new GDALPDFArrayRW();
        int i;
        for(i = 0; i < nGCPCount; i++)
        {
            GDALPDFArrayRW* poPTArray = new GDALPDFArrayRW();
            poPTArray->Add(pasGCPList[i].dfGCPPixel / dfUserUnit + psMargins->nLeft, TRUE);
            poPTArray->Add((nHeight - pasGCPList[i].dfGCPLine) / dfUserUnit + psMargins->nBottom, TRUE);
            poPTArray->Add(pasGCPList[i].dfGCPX, TRUE);
            poPTArray->Add(pasGCPList[i].dfGCPY, TRUE);
            poRegistrationArray->Add(poPTArray);
        }
        oLGIDict.Add("Registration", poRegistrationArray);
    }
    if( poNeatLineArray )
    {
        oLGIDict.Add("Neatline", poNeatLineArray);
    }

    const OGR_SRSNode* poNode = poSRS->GetRoot();
    if( poNode != NULL )
        poNode = poNode->GetChild(0);
    const char* pszDescription = NULL;
    if( poNode != NULL )
        pszDescription = poNode->GetValue();
    if( pszDescription )
    {
        oLGIDict.Add("Description", pszDescription);
    }

    oLGIDict.Add("Projection", poProjectionDict);

    /* GDAL extension */
    if( CSLTestBoolean( CPLGetConfigOption("GDAL_PDF_OGC_BP_WRITE_WKT", "TRUE") ) )
        poProjectionDict->Add("WKT", pszWKT);

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
    if (pszValue == NULL)
        pszValue = poSrcDS->GetMetadataItem(pszKey);
    if (pszValue != NULL && pszValue[0] == '\0')
        return NULL;
    else
        return pszValue;
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
/*                              WriteOCG()                              */
/************************************************************************/

int GDALPDFWriter::WriteOCG(const char* pszLayerName, int nParentId)
{
    if (pszLayerName == NULL || pszLayerName[0] == '\0')
        return 0;

    int nOGCId = AllocNewObject();

    GDALPDFOCGDesc oOCGDesc;
    oOCGDesc.nId = nOGCId;
    oOCGDesc.nParentId = nParentId;

    asOCGs.push_back(oOCGDesc);

    StartObj(nOGCId);
    {
        GDALPDFDictionaryRW oDict;
        oDict.Add("Type", GDALPDFObjectRW::CreateName("OCG"));
        oDict.Add("Name", pszLayerName);
        VSIFPrintfL(fp, "%s\n", oDict.Serialize().c_str());
    }
    EndObj();

    return nOGCId;
}

/************************************************************************/
/*                              StartPage()                             */
/************************************************************************/

int GDALPDFWriter::StartPage(GDALDataset* poSrcDS,
                             double dfDPI,
                             const char* pszGEO_ENCODING,
                             const char* pszNEATLINE,
                             PDFMargins* psMargins,
                             PDFCompressMethod eStreamCompressMethod,
                             int bHasOGRData)
{
    int  nWidth = poSrcDS->GetRasterXSize();
    int  nHeight = poSrcDS->GetRasterYSize();
    int  nBands = poSrcDS->GetRasterCount();

    double dfUserUnit = dfDPI / 72.0;
    double dfWidthInUserUnit = nWidth / dfUserUnit + psMargins->nLeft + psMargins->nRight;
    double dfHeightInUserUnit = nHeight / dfUserUnit + psMargins->nBottom + psMargins->nTop;

    int nPageId = AllocNewObject();
    asPageId.push_back(nPageId);

    int nContentId = AllocNewObject();
    int nResourcesId = AllocNewObject();

    int bISO32000 = EQUAL(pszGEO_ENCODING, "ISO32000") ||
                    EQUAL(pszGEO_ENCODING, "BOTH");
    int bOGC_BP   = EQUAL(pszGEO_ENCODING, "OGC_BP") ||
                    EQUAL(pszGEO_ENCODING, "BOTH");

    int nViewportId = 0;
    if( bISO32000 )
        nViewportId = WriteSRS_ISO32000(poSrcDS, dfUserUnit, pszNEATLINE, psMargins);

    int nLGIDictId = 0;
    if( bOGC_BP )
        nLGIDictId = WriteSRS_OGC_BP(poSrcDS, dfUserUnit, pszNEATLINE, psMargins);

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

    if (bHasOGRData)
        oDictPage.Add("StructParents", 0);

    VSIFPrintfL(fp, "%s\n", oDictPage.Serialize().c_str());
    EndObj();

    oPageContext.poSrcDS = poSrcDS;
    oPageContext.nPageId = nPageId;
    oPageContext.nContentId = nContentId;
    oPageContext.nResourcesId = nResourcesId;
    oPageContext.dfDPI = dfDPI;
    oPageContext.sMargins = *psMargins;
    oPageContext.nOCGRasterId = 0;
    oPageContext.eStreamCompressMethod = eStreamCompressMethod;

    return TRUE;
}

/************************************************************************/
/*                             WriteColorTable()                        */
/************************************************************************/

int GDALPDFWriter::WriteColorTable(GDALDataset* poSrcDS)
{
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

    return nColorTableId;
}

/************************************************************************/
/*                             WriteImagery()                           */
/************************************************************************/

int GDALPDFWriter::WriteImagery(const char* pszLayerName,
                                PDFCompressMethod eCompressMethod,
                                int nPredictor,
                                int nJPEGQuality,
                                const char* pszJPEG2000_DRIVER,
                                int nBlockXSize, int nBlockYSize,
                                GDALProgressFunc pfnProgress,
                                void * pProgressData)
{
    GDALDataset* poSrcDS = oPageContext.poSrcDS;
    int  nWidth = poSrcDS->GetRasterXSize();
    int  nHeight = poSrcDS->GetRasterYSize();
    double dfUserUnit = oPageContext.dfDPI / 72.0;

    oPageContext.nOCGRasterId = WriteOCG(pszLayerName);

    /* Does the source image has a color table ? */
    int nColorTableId = WriteColorTable(poSrcDS);

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

            GDALPDFImageDesc oImageDesc;
            oImageDesc.nImageId = nImageId;
            oImageDesc.dfXOff = (nBlockXOff * nBlockXSize) / dfUserUnit + oPageContext.sMargins.nLeft;
            oImageDesc.dfYOff = (nHeight - nBlockYOff * nBlockYSize - nReqHeight) / dfUserUnit + oPageContext.sMargins.nBottom;
            oImageDesc.dfXSize = nReqWidth / dfUserUnit;
            oImageDesc.dfYSize = nReqHeight / dfUserUnit;

            oPageContext.asImageDesc.push_back(oImageDesc);
        }
    }

    return TRUE;
}

#ifdef OGR_ENABLED

/************************************************************************/
/*                          WriteOGRDataSource()                        */
/************************************************************************/

int GDALPDFWriter::WriteOGRDataSource(const char* pszOGRDataSource,
                                      const char* pszOGRDisplayField,
                                      const char* pszOGRDisplayLayerNames,
                                      int bWriteOGRAttributes)
{
    if (OGRGetDriverCount() == 0)
        OGRRegisterAll();

    OGRDataSourceH hDS = OGROpen(pszOGRDataSource, 0, NULL);
    if (hDS == NULL)
        return FALSE;

    int iObj = 0;

    int nLayers = OGR_DS_GetLayerCount(hDS);

    char** papszLayerNames = CSLTokenizeString2(pszOGRDisplayLayerNames,",",0);

    for(int iLayer = 0; iLayer < nLayers; iLayer ++)
    {
        CPLString osLayerName;
        if (CSLCount(papszLayerNames) < nLayers)
            osLayerName = CPLSPrintf("Layer%d", iLayer + 1);
        else
            osLayerName = papszLayerNames[iLayer];

        WriteOGRLayer(hDS, iLayer,
                      pszOGRDisplayField,
                      osLayerName,
                      bWriteOGRAttributes,
                      iObj);
    }

    OGRReleaseDataSource(hDS);

    CSLDestroy(papszLayerNames);

    return TRUE;
}

/************************************************************************/
/*                           StartOGRLayer()                            */
/************************************************************************/

GDALPDFLayerDesc GDALPDFWriter::StartOGRLayer(CPLString osLayerName,
                                              int bWriteOGRAttributes)
{
    GDALPDFLayerDesc osVectorDesc;
    osVectorDesc.osLayerName = osLayerName;
    osVectorDesc.bWriteOGRAttributes = bWriteOGRAttributes;
    osVectorDesc.nOGCId = WriteOCG(osLayerName);
    osVectorDesc.nFeatureLayerId = (bWriteOGRAttributes) ? AllocNewObject() : 0;
    osVectorDesc.nOCGTextId = 0;

    return osVectorDesc;
}

/************************************************************************/
/*                           EndOGRLayer()                              */
/************************************************************************/

void GDALPDFWriter::EndOGRLayer(GDALPDFLayerDesc& osVectorDesc)
{
    if (osVectorDesc.bWriteOGRAttributes)
    {
        StartObj(osVectorDesc.nFeatureLayerId);

        GDALPDFDictionaryRW oDict;
        oDict.Add("A", &(new GDALPDFDictionaryRW())->Add("O",
                GDALPDFObjectRW::CreateName("UserProperties")));

        GDALPDFArrayRW* poArray = new GDALPDFArrayRW();
        oDict.Add("K", poArray);

        for(int i = 0; i < (int)osVectorDesc.aUserPropertiesIds.size(); i++)
        {
            poArray->Add(osVectorDesc.aUserPropertiesIds[i], 0);
        }

        if (nStructTreeRootId == 0)
            nStructTreeRootId = AllocNewObject();

        oDict.Add("P", nStructTreeRootId, 0);
        oDict.Add("S", GDALPDFObjectRW::CreateName(osVectorDesc.osLayerName));

        VSIFPrintfL(fp, "%s\n", oDict.Serialize().c_str());

        EndObj();
    }

    oPageContext.asVectorDesc.push_back(osVectorDesc);
}

/************************************************************************/
/*                           WriteOGRLayer()                            */
/************************************************************************/

int GDALPDFWriter::WriteOGRLayer(OGRDataSourceH hDS,
                                 int iLayer,
                                 const char* pszOGRDisplayField,
                                 CPLString osLayerName,
                                 int bWriteOGRAttributes,
                                 int& iObj)
{
    GDALPDFLayerDesc osVectorDesc = StartOGRLayer(osLayerName,
                                                  bWriteOGRAttributes);
    OGRLayerH hLyr = OGR_DS_GetLayer(hDS, iLayer);

    OGRFeatureH hFeat;
    int iObjLayer = 0;
    while( (hFeat = OGR_L_GetNextFeature(hLyr)) != NULL)
    {
        WriteOGRFeature(osVectorDesc,
                        hFeat,
                        pszOGRDisplayField,
                        bWriteOGRAttributes,
                        iObj,
                        iObjLayer);

        OGR_F_Destroy(hFeat);
    }

    EndOGRLayer(osVectorDesc);

    return TRUE;
}

/************************************************************************/
/*                             DrawGeometry()                           */
/************************************************************************/

static void DrawGeometry(VSILFILE* fp, OGRGeometryH hGeom, double adfMatrix[4], int bPaint = TRUE)
{
    switch(wkbFlatten(OGR_G_GetGeometryType(hGeom)))
    {
        case wkbLineString:
        {
            int nPoints = OGR_G_GetPointCount(hGeom);
            for(int i=0;i<nPoints;i++)
            {
                double dfX = OGR_G_GetX(hGeom, i) * adfMatrix[1] + adfMatrix[0];
                double dfY = OGR_G_GetY(hGeom, i) * adfMatrix[3] + adfMatrix[2];
                VSIFPrintfL(fp, "%f %f %c\n", dfX, dfY, (i == 0) ? 'm' : 'l');
            }
            if (bPaint)
                VSIFPrintfL(fp, "S\n");
            break;
        }

        case wkbPolygon:
        {
            int nParts = OGR_G_GetGeometryCount(hGeom);
            for(int i=0;i<nParts;i++)
            {
                DrawGeometry(fp, OGR_G_GetGeometryRef(hGeom, i), adfMatrix, FALSE);
                VSIFPrintfL(fp, "h\n");
            }
            if (bPaint)
                VSIFPrintfL(fp, "b*\n");
            break;
        }

        case wkbMultiLineString:
        {
            int nParts = OGR_G_GetGeometryCount(hGeom);
            for(int i=0;i<nParts;i++)
            {
                DrawGeometry(fp, OGR_G_GetGeometryRef(hGeom, i), adfMatrix, FALSE);
            }
            if (bPaint)
                VSIFPrintfL(fp, "S\n");
            break;
        }

        case wkbMultiPolygon:
        {
            int nParts = OGR_G_GetGeometryCount(hGeom);
            for(int i=0;i<nParts;i++)
            {
                DrawGeometry(fp, OGR_G_GetGeometryRef(hGeom, i), adfMatrix, FALSE);
            }
            if (bPaint)
                VSIFPrintfL(fp, "b*\n");
            break;
        }

        default:
            break;
    }
}

/************************************************************************/
/*                          WriteOGRFeature()                           */
/************************************************************************/

int GDALPDFWriter::WriteOGRFeature(GDALPDFLayerDesc& osVectorDesc,
                                   OGRFeatureH hFeat,
                                   const char* pszOGRDisplayField,
                                   int bWriteOGRAttributes,
                                   int& iObj,
                                   int& iObjLayer)
{
    GDALDataset* poSrcDS = oPageContext.poSrcDS;
    int  nHeight = poSrcDS->GetRasterYSize();
    double dfUserUnit = oPageContext.dfDPI / 72.0;
    double adfGeoTransform[6];
    poSrcDS->GetGeoTransform(adfGeoTransform);

    double adfMatrix[4];
    adfMatrix[0] = - adfGeoTransform[0] / (adfGeoTransform[1] * dfUserUnit) + oPageContext.sMargins.nLeft;
    adfMatrix[1] = 1.0 / (adfGeoTransform[1] * dfUserUnit);
    adfMatrix[2] = - (adfGeoTransform[3] + adfGeoTransform[5] * nHeight) / (-adfGeoTransform[5] * dfUserUnit) + oPageContext.sMargins.nBottom;
    adfMatrix[3] = 1.0 / (-adfGeoTransform[5] * dfUserUnit);

    OGRGeometryH hGeom = OGR_F_GetGeometryRef(hFeat);
    if (hGeom == NULL)
    {
        return TRUE;
    }

    /* -------------------------------------------------------------- */
    /*  Get envelope                                                  */
    /* -------------------------------------------------------------- */
    OGREnvelope sEnvelope;
    OGR_G_GetEnvelope(hGeom, &sEnvelope);


    /* -------------------------------------------------------------- */
    /*  Get style                                                     */
    /* -------------------------------------------------------------- */
    int nPenR = 0, nPenG = 0, nPenB = 0, nPenA = 255;
    int nBrushR = 127, nBrushG = 127, nBrushB = 127, nBrushA = 127;
    int nTextR = 0, nTextG = 0, nTextB = 0, nTextA = 255;
    int bSymbolColorDefined = FALSE;
    int nSymbolR = 0, nSymbolG = 0, nSymbolB = 0, nSymbolA = 255;
    double dfTextSize = 12, dfTextAngle = 0;
    double dfPenWidth = 1;
    double dfSymbolSize = 5;
    CPLString osDashArray;
    CPLString osLabelText;
    CPLString osSymbolId;

    OGRStyleMgrH hSM = OGR_SM_Create(NULL);
    OGR_SM_InitFromFeature(hSM, hFeat);
    int nCount = OGR_SM_GetPartCount(hSM, NULL);
    for(int iPart = 0; iPart < nCount; iPart++)
    {
        OGRStyleToolH hTool = OGR_SM_GetPart(hSM, iPart, NULL);
        if (hTool)
        {
            if (OGR_ST_GetType(hTool) == OGRSTCPen)
            {
                int bIsNull = TRUE;
                const char* pszColor = OGR_ST_GetParamStr(hTool, OGRSTPenColor, &bIsNull);
                if (pszColor && !bIsNull)
                {
                    int nRed = 0, nGreen = 0, nBlue = 0, nAlpha = 255;
                    int nVals = sscanf(pszColor,"#%2x%2x%2x%2x",&nRed,&nGreen,&nBlue,&nAlpha);
                    if (nVals >= 3)
                    {
                        nPenR = nRed;
                        nPenG = nGreen;
                        nPenB = nBlue;
                        if (nVals == 4)
                            nPenA = nAlpha;
                    }
                }

                const char* pszDash = OGR_ST_GetParamStr(hTool, OGRSTPenPattern, &bIsNull);
                if (pszDash && !bIsNull)
                {
                    char** papszTokens = CSLTokenizeString2(pszDash, " ", 0);
                    int nTokens = CSLCount(papszTokens);
                    if ((nTokens % 2) == 0)
                    {
                        for(int i=0;i<nTokens;i++)
                        {
                            osDashArray += CPLSPrintf("%d ", atoi(papszTokens[i]));
                        }
                    }
                    CSLDestroy(papszTokens);
                }

                //OGRSTUnitId eUnit = OGR_ST_GetUnit(hTool);
                double dfWidth = OGR_ST_GetParamDbl(hTool, OGRSTPenWidth, &bIsNull);
                if (!bIsNull)
                    dfPenWidth = dfWidth;
            }
            else if (OGR_ST_GetType(hTool) == OGRSTCBrush)
            {
                int bIsNull;
                const char* pszColor = OGR_ST_GetParamStr(hTool, OGRSTBrushFColor, &bIsNull);
                if (pszColor)
                {
                    int nRed = 0, nGreen = 0, nBlue = 0, nAlpha = 255;
                    int nVals = sscanf(pszColor,"#%2x%2x%2x%2x",&nRed,&nGreen,&nBlue,&nAlpha);
                    if (nVals >= 3)
                    {
                        nBrushR = nRed;
                        nBrushG = nGreen;
                        nBrushB = nBlue;
                        if (nVals == 4)
                            nBrushA = nAlpha;
                    }
                }
            }
            else if (OGR_ST_GetType(hTool) == OGRSTCLabel)
            {
                int bIsNull;
                const char* pszStr = OGR_ST_GetParamStr(hTool, OGRSTLabelTextString, &bIsNull);
                if (pszStr)
                {
                    osLabelText = pszStr;
                }

                const char* pszColor = OGR_ST_GetParamStr(hTool, OGRSTLabelFColor, &bIsNull);
                if (pszColor && !bIsNull)
                {
                    int nRed = 0, nGreen = 0, nBlue = 0, nAlpha = 255;
                    int nVals = sscanf(pszColor,"#%2x%2x%2x%2x",&nRed,&nGreen,&nBlue,&nAlpha);
                    if (nVals >= 3)
                    {
                        nTextR = nRed;
                        nTextG = nGreen;
                        nTextB = nBlue;
                        if (nVals == 4)
                            nTextA = nAlpha;
                    }
                }

                double dfVal = OGR_ST_GetParamDbl(hTool, OGRSTLabelSize, &bIsNull);
                if (!bIsNull)
                {
                    dfTextSize = dfVal;
                }

                dfVal = OGR_ST_GetParamDbl(hTool, OGRSTLabelAngle, &bIsNull);
                if (!bIsNull)
                {
                    dfTextAngle = dfVal;
                }
            }
            else if (OGR_ST_GetType(hTool) == OGRSTCSymbol)
            {
                int bIsNull;
                const char* pszSymbolId = OGR_ST_GetParamStr(hTool, OGRSTSymbolId, &bIsNull);
                if (pszSymbolId && !bIsNull)
                    osSymbolId = pszSymbolId;

                double dfVal = OGR_ST_GetParamDbl(hTool, OGRSTSymbolSize, &bIsNull);
                if (!bIsNull)
                {
                    dfSymbolSize = dfVal;
                }

                const char* pszColor = OGR_ST_GetParamStr(hTool, OGRSTSymbolColor, &bIsNull);
                if (pszColor && !bIsNull)
                {
                    int nRed = 0, nGreen = 0, nBlue = 0, nAlpha = 255;
                    int nVals = sscanf(pszColor,"#%2x%2x%2x%2x",&nRed,&nGreen,&nBlue,&nAlpha);
                    if (nVals >= 3)
                    {
                        bSymbolColorDefined = TRUE;
                        nSymbolR = nRed;
                        nSymbolG = nGreen;
                        nSymbolB = nBlue;
                        if (nVals == 4)
                            nSymbolA = nAlpha;
                    }
                }
            }

            OGR_ST_Destroy(hTool);
        }
    }
    OGR_SM_Destroy(hSM);

    if (wkbFlatten(OGR_G_GetGeometryType(hGeom)) == wkbPoint && bSymbolColorDefined)
    {
        nPenR = nSymbolR;
        nPenG = nSymbolG;
        nPenB = nSymbolB;
        nPenA = nSymbolA;
        nBrushR = nSymbolR;
        nBrushG = nSymbolG;
        nBrushB = nSymbolB;
        nBrushA = nSymbolA;
    }

    double dfRadius = dfSymbolSize * dfUserUnit;

    /* -------------------------------------------------------------- */
    /*  Write object dictionary                                       */
    /* -------------------------------------------------------------- */
    int nObjectId = AllocNewObject();
    int nObjectLengthId = AllocNewObject();

    osVectorDesc.aIds.push_back(nObjectId);

    StartObj(nObjectId);

    {
        GDALPDFDictionaryRW oDict;
        double dfMargin = (wkbFlatten(OGR_G_GetGeometryType(hGeom)) == wkbPoint) ? dfRadius + dfPenWidth : dfPenWidth;
        oDict.Add("Length", nObjectLengthId, 0)
            .Add("Type", GDALPDFObjectRW::CreateName("XObject"))
            .Add("BBox", &((new GDALPDFArrayRW())
                            ->Add((int)floor(sEnvelope.MinX * adfMatrix[1] + adfMatrix[0] - dfMargin)).
                                Add((int)floor(sEnvelope.MinY * adfMatrix[3] + adfMatrix[2] - dfMargin)).
                                Add((int)ceil(sEnvelope.MaxX * adfMatrix[1] + adfMatrix[0] + dfMargin)).
                                Add((int)ceil(sEnvelope.MaxY * adfMatrix[3] + adfMatrix[2] + dfMargin))))
            .Add("Subtype", GDALPDFObjectRW::CreateName("Form"));
        if( oPageContext.eStreamCompressMethod != COMPRESS_NONE )
        {
            oDict.Add("Filter", GDALPDFObjectRW::CreateName("FlateDecode"));
        }

        GDALPDFDictionaryRW* poGS1 = new GDALPDFDictionaryRW();
        poGS1->Add("Type", GDALPDFObjectRW::CreateName("ExtGState"));
        if (nPenA != 255)
            poGS1->Add("CA", (nPenA == 127 || nPenA == 128) ? 0.5 : nPenA / 255.0);
        if (nBrushA != 255)
            poGS1->Add("ca", (nBrushA == 127 || nBrushA == 128) ? 0.5 : nBrushA / 255.0 );

        GDALPDFDictionaryRW* poExtGState = new GDALPDFDictionaryRW();
        poExtGState->Add("GS1", poGS1);

        GDALPDFDictionaryRW* poResources = new GDALPDFDictionaryRW();
        poResources->Add("ExtGState", poExtGState);

        oDict.Add("Resources", poResources);

        VSIFPrintfL(fp, "%s\n", oDict.Serialize().c_str());
    }

    /* -------------------------------------------------------------- */
    /*  Write object stream                                           */
    /* -------------------------------------------------------------- */
    VSIFPrintfL(fp, "stream\n");

    vsi_l_offset nStreamStart = VSIFTellL(fp);

    VSILFILE* fpGZip = NULL;
    VSILFILE* fpBack = fp;
    if( oPageContext.eStreamCompressMethod != COMPRESS_NONE )
    {
        fpGZip = (VSILFILE* )VSICreateGZipWritable( (VSIVirtualHandle*) fp, TRUE, FALSE );
        fp = fpGZip;
    }

    VSIFPrintfL(fp, "q\n");

    VSIFPrintfL(fp, "/GS1 gs\n");

    VSIFPrintfL(fp, "%f w\n"
                    "0 J\n"
                    "0 j\n"
                    "10 M\n"
                    "[%s]0 d\n",
                    dfPenWidth,
                    osDashArray.c_str());

    VSIFPrintfL(fp, "%f %f %f RG\n", nPenR / 255.0, nPenG / 255.0, nPenB / 255.0);
    VSIFPrintfL(fp, "%f %f %f rg\n", nBrushR / 255.0, nBrushG / 255.0, nBrushB / 255.0);

    if (wkbFlatten(OGR_G_GetGeometryType(hGeom)) == wkbPoint)
    {
        double dfX = OGR_G_GetX(hGeom, 0) * adfMatrix[1] + adfMatrix[0];
        double dfY = OGR_G_GetY(hGeom, 0) * adfMatrix[3] + adfMatrix[2];

        if (osSymbolId == "")
            osSymbolId = "ogr-sym-3"; /* symbol by default */
        else if ( !(osSymbolId == "ogr-sym-0" ||
                    osSymbolId == "ogr-sym-1" ||
                    osSymbolId == "ogr-sym-2" ||
                    osSymbolId == "ogr-sym-3" ||
                    osSymbolId == "ogr-sym-4" ||
                    osSymbolId == "ogr-sym-5" ||
                    osSymbolId == "ogr-sym-6" ||
                    osSymbolId == "ogr-sym-7" ||
                    osSymbolId == "ogr-sym-8" ||
                    osSymbolId == "ogr-sym-9") )
        {
            CPLDebug("PDF", "Unhandled symbol id : %s. Using ogr-sym-3 instead", osSymbolId.c_str());
            osSymbolId = "ogr-sym-3";
        }

        if (osSymbolId == "ogr-sym-0") /* cross (+)  */
        {
            VSIFPrintfL(fp, "%f %f m\n", dfX - dfRadius, dfY);
            VSIFPrintfL(fp, "%f %f l\n", dfX + dfRadius, dfY);
            VSIFPrintfL(fp, "%f %f m\n", dfX, dfY - dfRadius);
            VSIFPrintfL(fp, "%f %f l\n", dfX, dfY + dfRadius);
            VSIFPrintfL(fp, "s\n");
        }
        else if (osSymbolId == "ogr-sym-1") /* diagcross (X) */
        {
            VSIFPrintfL(fp, "%f %f m\n", dfX - dfRadius, dfY - dfRadius);
            VSIFPrintfL(fp, "%f %f l\n", dfX + dfRadius, dfY + dfRadius);
            VSIFPrintfL(fp, "%f %f m\n", dfX - dfRadius, dfY + dfRadius);
            VSIFPrintfL(fp, "%f %f l\n", dfX + dfRadius, dfY - dfRadius);
            VSIFPrintfL(fp, "s\n");
        }
        else if (osSymbolId == "ogr-sym-2" ||
                 osSymbolId == "ogr-sym-3") /* circle */
        {
            /* See http://www.whizkidtech.redprince.net/bezier/circle/kappa/ */
            const double dfKappa = 0.5522847498;

            VSIFPrintfL(fp, "%f %f m\n", dfX - dfRadius, dfY);
            VSIFPrintfL(fp, "%f %f %f %f %f %f c\n",
                        dfX - dfRadius, dfY - dfRadius * dfKappa,
                        dfX - dfRadius * dfKappa, dfY - dfRadius,
                        dfX, dfY - dfRadius);
            VSIFPrintfL(fp, "%f %f %f %f %f %f c\n",
                        dfX + dfRadius * dfKappa, dfY - dfRadius,
                        dfX + dfRadius, dfY - dfRadius * dfKappa,
                        dfX + dfRadius, dfY);
            VSIFPrintfL(fp, "%f %f %f %f %f %f c\n",
                        dfX + dfRadius, dfY + dfRadius * dfKappa,
                        dfX + dfRadius * dfKappa, dfY + dfRadius,
                        dfX, dfY + dfRadius);
            VSIFPrintfL(fp, "%f %f %f %f %f %f c\n",
                        dfX - dfRadius * dfKappa, dfY + dfRadius,
                        dfX - dfRadius, dfY + dfRadius * dfKappa,
                        dfX - dfRadius, dfY);
            if (osSymbolId == "ogr-sym-2") 
                VSIFPrintfL(fp, "s\n"); /* not filled */
            else
                VSIFPrintfL(fp, "b*\n"); /* filled */
        }
        else if (osSymbolId == "ogr-sym-4" ||
                 osSymbolId == "ogr-sym-5") /* square */
        {
            VSIFPrintfL(fp, "%f %f m\n", dfX - dfRadius, dfY + dfRadius);
            VSIFPrintfL(fp, "%f %f l\n", dfX + dfRadius, dfY + dfRadius);
            VSIFPrintfL(fp, "%f %f l\n", dfX + dfRadius, dfY - dfRadius);
            VSIFPrintfL(fp, "%f %f l\n", dfX - dfRadius, dfY - dfRadius);
            if (osSymbolId == "ogr-sym-4")
                VSIFPrintfL(fp, "s\n"); /* not filled */
            else
                VSIFPrintfL(fp, "b*\n"); /* filled */
        }
        else if (osSymbolId == "ogr-sym-6" ||
                 osSymbolId == "ogr-sym-7") /* triangle */
        {
            const double dfSqrt3 = 1.73205080757;
            VSIFPrintfL(fp, "%f %f m\n", dfX - dfRadius, dfY - dfRadius * dfSqrt3 / 3);
            VSIFPrintfL(fp, "%f %f l\n", dfX, dfY + 2 * dfRadius * dfSqrt3 / 3);
            VSIFPrintfL(fp, "%f %f l\n", dfX + dfRadius, dfY - dfRadius * dfSqrt3 / 3);
            if (osSymbolId == "ogr-sym-6")
                VSIFPrintfL(fp, "s\n"); /* not filled */
            else
                VSIFPrintfL(fp, "b*\n"); /* filled */
        }
        else if (osSymbolId == "ogr-sym-8" ||
                 osSymbolId == "ogr-sym-9") /* star */
        {
            const double dfSin18divSin126 = 0.38196601125;
            VSIFPrintfL(fp, "%f %f m\n", dfX, dfY + dfRadius);
            for(int i=1; i<10;i++)
            {
                double dfFactor = ((i % 2) == 1) ? dfSin18divSin126 : 1.0;
                VSIFPrintfL(fp, "%f %f l\n",
                            dfX + cos(M_PI / 2 - i * M_PI * 36 / 180) * dfRadius * dfFactor,
                            dfY + sin(M_PI / 2 - i * M_PI * 36 / 180) * dfRadius * dfFactor);
            }
            if (osSymbolId == "ogr-sym-8")
                VSIFPrintfL(fp, "s\n"); /* not filled */
            else
                VSIFPrintfL(fp, "b*\n"); /* filled */
        }
    }
    else
    {
        DrawGeometry(fp, hGeom, adfMatrix);
    }

    VSIFPrintfL(fp, "Q");

    if (fpGZip)
        VSIFCloseL(fpGZip);
    fp = fpBack;

    vsi_l_offset nStreamEnd = VSIFTellL(fp);
    VSIFPrintfL(fp, "\n");
    VSIFPrintfL(fp, "endstream\n");
    EndObj();

    StartObj(nObjectLengthId);
    VSIFPrintfL(fp,
                "   %ld\n",
                (long)(nStreamEnd - nStreamStart));
    EndObj();

    /* -------------------------------------------------------------- */
    /*  Write label                                                   */
    /* -------------------------------------------------------------- */
    if (osLabelText.size() && wkbFlatten(OGR_G_GetGeometryType(hGeom)) == wkbPoint)
    {
        if (osVectorDesc.nOCGTextId == 0)
            osVectorDesc.nOCGTextId = WriteOCG("Text", osVectorDesc.nOGCId);

        /* -------------------------------------------------------------- */
        /*  Write object dictionary                                       */
        /* -------------------------------------------------------------- */
        nObjectId = AllocNewObject();
        nObjectLengthId = AllocNewObject();

        osVectorDesc.aIdsText.push_back(nObjectId);

        StartObj(nObjectId);
        {
            GDALPDFDictionaryRW oDict;

            GDALDataset* poSrcDS = oPageContext.poSrcDS;
            int  nWidth = poSrcDS->GetRasterXSize();
            int  nHeight = poSrcDS->GetRasterYSize();
            double dfUserUnit = oPageContext.dfDPI / 72.0;
            double dfWidthInUserUnit = nWidth / dfUserUnit + oPageContext.sMargins.nLeft + oPageContext.sMargins.nRight;
            double dfHeightInUserUnit = nHeight / dfUserUnit + oPageContext.sMargins.nBottom + oPageContext.sMargins.nTop;

            oDict.Add("Length", nObjectLengthId, 0)
                .Add("Type", GDALPDFObjectRW::CreateName("XObject"))
                .Add("BBox", &((new GDALPDFArrayRW())
                                ->Add(0).Add(0)).Add(dfWidthInUserUnit).Add(dfHeightInUserUnit))
                .Add("Subtype", GDALPDFObjectRW::CreateName("Form"));
            if( oPageContext.eStreamCompressMethod != COMPRESS_NONE )
            {
                oDict.Add("Filter", GDALPDFObjectRW::CreateName("FlateDecode"));
            }

            GDALPDFDictionaryRW* poResources = new GDALPDFDictionaryRW();

            if (nTextA != 255)
            {
                GDALPDFDictionaryRW* poGS1 = new GDALPDFDictionaryRW();
                poGS1->Add("Type", GDALPDFObjectRW::CreateName("ExtGState"));
                poGS1->Add("ca", (nTextA == 127 || nTextA == 128) ? 0.5 : nTextA / 255.0);

                GDALPDFDictionaryRW* poExtGState = new GDALPDFDictionaryRW();
                poExtGState->Add("GS1", poGS1);

                poResources->Add("ExtGState", poExtGState);
            }

            GDALPDFDictionaryRW* poDictFTimesRoman = NULL;
            poDictFTimesRoman = new GDALPDFDictionaryRW();
            poDictFTimesRoman->Add("Type", GDALPDFObjectRW::CreateName("Font"));
            poDictFTimesRoman->Add("BaseFont", GDALPDFObjectRW::CreateName("Times-Roman"));
            poDictFTimesRoman->Add("Encoding", GDALPDFObjectRW::CreateName("WinAnsiEncoding"));
            poDictFTimesRoman->Add("Subtype", GDALPDFObjectRW::CreateName("Type1"));

            GDALPDFDictionaryRW* poDictFont = new GDALPDFDictionaryRW();
            if (poDictFTimesRoman)
                poDictFont->Add("FTimesRoman", poDictFTimesRoman);
            poResources->Add("Font", poDictFont);

            oDict.Add("Resources", poResources);

            VSIFPrintfL(fp, "%s\n", oDict.Serialize().c_str());
        }

        /* -------------------------------------------------------------- */
        /*  Write object stream                                           */
        /* -------------------------------------------------------------- */
        VSIFPrintfL(fp, "stream\n");

        vsi_l_offset nStreamStart = VSIFTellL(fp);

        VSILFILE* fpGZip = NULL;
        VSILFILE* fpBack = fp;
        if( oPageContext.eStreamCompressMethod != COMPRESS_NONE )
        {
            fpGZip = (VSILFILE* )VSICreateGZipWritable( (VSIVirtualHandle*) fp, TRUE, FALSE );
            fp = fpGZip;
        }

        double dfX = OGR_G_GetX(hGeom, 0) * adfMatrix[1] + adfMatrix[0];
        double dfY = OGR_G_GetY(hGeom, 0) * adfMatrix[3] + adfMatrix[2];

        VSIFPrintfL(fp, "q\n");
        VSIFPrintfL(fp, "BT\n");
        if (nTextA != 255)
        {
            VSIFPrintfL(fp, "/GS1 gs\n");
        }
        if (dfTextAngle == 0)
        {
            VSIFPrintfL(fp, "%f %f Td\n", dfX, dfY);
        }
        else
        {
            dfTextAngle = - dfTextAngle * M_PI / 180.0;
            VSIFPrintfL(fp, "%f %f %f %f %f %f Tm\n",
                        cos(dfTextAngle), -sin(dfTextAngle),
                        sin(dfTextAngle), cos(dfTextAngle),
                        dfX, dfY);
        }
        VSIFPrintfL(fp, "%f %f %f rg\n", nTextR / 255.0, nTextG / 255.0, nTextB / 255.0);
        VSIFPrintfL(fp, "/FTimesRoman %f Tf\n", dfTextSize);
        VSIFPrintfL(fp, "(");
        for(size_t i=0;i<osLabelText.size();i++)
        {
            /*if (osLabelText[i] == '\n')
                VSIFPrintfL(fp, ") Tj T* (");
            else */if (osLabelText[i] >= 32 && osLabelText[i] <= 127)
                VSIFPrintfL(fp, "%c", osLabelText[i]);
            else
                VSIFPrintfL(fp, "_");
        }
        VSIFPrintfL(fp, ") Tj\n");
        VSIFPrintfL(fp, "ET\n");
        VSIFPrintfL(fp, "Q");

        if (fpGZip)
            VSIFCloseL(fpGZip);
        fp = fpBack;

        vsi_l_offset nStreamEnd = VSIFTellL(fp);
        VSIFPrintfL(fp, "\n");
        VSIFPrintfL(fp, "endstream\n");
        EndObj();

        StartObj(nObjectLengthId);
        VSIFPrintfL(fp,
                    "   %ld\n",
                    (long)(nStreamEnd - nStreamStart));
        EndObj();
    }
    else
    {
        osVectorDesc.aIdsText.push_back(0);
    }

    /* -------------------------------------------------------------- */
    /*  Write feature attributes                                      */
    /* -------------------------------------------------------------- */
    int nFeatureUserProperties = 0;

    int iField = -1;
    CPLString osFeatureName;

    if (bWriteOGRAttributes)
    {
        if (pszOGRDisplayField &&
            (iField = OGR_FD_GetFieldIndex(OGR_F_GetDefnRef(hFeat), pszOGRDisplayField)) >= 0)
            osFeatureName = OGR_F_GetFieldAsString(hFeat, iField);
        else
            osFeatureName = CPLSPrintf("feature%d", iObjLayer + 1);

        nFeatureUserProperties = AllocNewObject();
        StartObj(nFeatureUserProperties);

        GDALPDFDictionaryRW oDict;
        GDALPDFDictionaryRW* poDictA = new GDALPDFDictionaryRW();
        oDict.Add("A", poDictA);
        poDictA->Add("O", GDALPDFObjectRW::CreateName("UserProperties"));

        int nFields = OGR_F_GetFieldCount(hFeat);
        GDALPDFArrayRW* poArray = new GDALPDFArrayRW();
        for(int i = 0; i < nFields; i++)
        {
            if (OGR_F_IsFieldSet(hFeat, i))
            {
                OGRFieldDefnH hFDefn = OGR_F_GetFieldDefnRef( hFeat, i );
                GDALPDFDictionaryRW* poKV = new GDALPDFDictionaryRW();
                poKV->Add("N", OGR_Fld_GetNameRef(hFDefn));
                if (OGR_Fld_GetType(hFDefn) == OFTInteger)
                    poKV->Add("V", OGR_F_GetFieldAsInteger(hFeat, i));
                else if (OGR_Fld_GetType(hFDefn) == OFTReal)
                    poKV->Add("V", OGR_F_GetFieldAsDouble(hFeat, i));
                else
                    poKV->Add("V", OGR_F_GetFieldAsString(hFeat, i));
                poArray->Add(poKV);
            }
        }

        poDictA->Add("P", poArray);

        oDict.Add("K", iObj);
        oDict.Add("P", osVectorDesc.nFeatureLayerId, 0);
        oDict.Add("Pg", oPageContext.nPageId, 0);
        oDict.Add("S", GDALPDFObjectRW::CreateName("feature"));
        oDict.Add("T", osFeatureName);

        VSIFPrintfL(fp, "%s\n", oDict.Serialize().c_str());

        EndObj();
    }

    iObj ++;
    iObjLayer ++;

    osVectorDesc.aUserPropertiesIds.push_back(nFeatureUserProperties);
    osVectorDesc.aFeatureNames.push_back(osFeatureName);

    return TRUE;
}

#endif

/************************************************************************/
/*                               EndPage()                              */
/************************************************************************/

int GDALPDFWriter::EndPage(const char* pszExtraImages,
                           const char* pszExtraStream,
                           const char* pszExtraLayerName)
{
    int nLayerExtraId = WriteOCG(pszExtraLayerName);

    int bHasTimesRoman = pszExtraStream && strstr(pszExtraStream, "/FTimesRoman");
    int bHasTimesBold = pszExtraStream && strstr(pszExtraStream, "/FTimesBold");

    /* -------------------------------------------------------------- */
    /*  Write extra images                                            */
    /* -------------------------------------------------------------- */
    std::vector<GDALPDFImageDesc> asExtraImageDesc;
    if (pszExtraImages)
    {
        char** papszExtraImagesTokens = CSLTokenizeString2(pszExtraImages, ",", 0);
        int nCount = CSLCount(papszExtraImagesTokens);
        if ((nCount % 4) == 0)
        {
            double dfUserUnit = oPageContext.dfDPI / 72.0;
            for(int i=0;i<nCount;i+=4)
            {
                const char* pszImageFilename = papszExtraImagesTokens[i+0];
                double dfX = atof(papszExtraImagesTokens[i+1]);
                double dfY = atof(papszExtraImagesTokens[i+2]);
                double dfScale = atof(papszExtraImagesTokens[i+3]);
                GDALDataset* poImageDS = (GDALDataset* )GDALOpen(pszImageFilename, GA_ReadOnly);
                if (poImageDS)
                {
                    int nColorTableId = WriteColorTable(poImageDS);
                    int nImageId = WriteBlock( poImageDS,
                                               0, 0,
                                               poImageDS->GetRasterXSize(),
                                               poImageDS->GetRasterYSize(),
                                               nColorTableId,
                                               COMPRESS_DEFAULT,
                                               0,
                                               -1,
                                               NULL,
                                               NULL,
                                               NULL );

                    if (nImageId)
                    {
                        GDALPDFImageDesc oImageDesc;
                        oImageDesc.nImageId = nImageId;
                        oImageDesc.dfXSize = poImageDS->GetRasterXSize() / dfUserUnit * dfScale;
                        oImageDesc.dfYSize = poImageDS->GetRasterYSize() / dfUserUnit * dfScale;
                        oImageDesc.dfXOff = dfX;
                        oImageDesc.dfYOff = dfY - oImageDesc.dfYSize;

                        asExtraImageDesc.push_back(oImageDesc);
                    }

                    GDALClose(poImageDS);
                }
            }
        }
        CSLDestroy(papszExtraImagesTokens);
    }

    /* -------------------------------------------------------------- */
    /*  Write content dictionary                                      */
    /* -------------------------------------------------------------- */
    int nContentLengthId = AllocNewObject();

    StartObj(oPageContext.nContentId);
    {
        GDALPDFDictionaryRW oDict;
        oDict.Add("Length", nContentLengthId, 0);
        if( oPageContext.eStreamCompressMethod != COMPRESS_NONE )
        {
            oDict.Add("Filter", GDALPDFObjectRW::CreateName("FlateDecode"));
        }
        VSIFPrintfL(fp, "%s\n", oDict.Serialize().c_str());
    }

    /* -------------------------------------------------------------- */
    /*  Write content stream                                          */
    /* -------------------------------------------------------------- */
    VSIFPrintfL(fp, "stream\n");
    vsi_l_offset nStreamStart = VSIFTellL(fp);

    VSILFILE* fpGZip = NULL;
    VSILFILE* fpBack = fp;
    if( oPageContext.eStreamCompressMethod != COMPRESS_NONE )
    {
        fpGZip = (VSILFILE* )VSICreateGZipWritable( (VSIVirtualHandle*) fp, TRUE, FALSE );
        fp = fpGZip;
    }

    /* -------------------------------------------------------------- */
    /*  Write drawing instructions for raster blocks                  */
    /* -------------------------------------------------------------- */
    if (oPageContext.nOCGRasterId)
        VSIFPrintfL(fp, "/OC /Lyr%d BDC\n", oPageContext.nOCGRasterId);

    for(size_t iImage = 0; iImage < oPageContext.asImageDesc.size(); iImage ++)
    {
        VSIFPrintfL(fp, "q\n");
        GDALPDFObjectRW* poXSize = GDALPDFObjectRW::CreateReal(oPageContext.asImageDesc[iImage].dfXSize);
        GDALPDFObjectRW* poYSize = GDALPDFObjectRW::CreateReal(oPageContext.asImageDesc[iImage].dfYSize);
        GDALPDFObjectRW* poXOff = GDALPDFObjectRW::CreateReal(oPageContext.asImageDesc[iImage].dfXOff);
        GDALPDFObjectRW* poYOff = GDALPDFObjectRW::CreateReal(oPageContext.asImageDesc[iImage].dfYOff);
        VSIFPrintfL(fp, "%s 0 0 %s %s %s cm\n",
                    poXSize->Serialize().c_str(),
                    poYSize->Serialize().c_str(),
                    poXOff->Serialize().c_str(),
                    poYOff->Serialize().c_str());
        delete poXSize;
        delete poYSize;
        delete poXOff;
        delete poYOff;
        VSIFPrintfL(fp, "/Image%d Do\n",
                    oPageContext.asImageDesc[iImage].nImageId);
        VSIFPrintfL(fp, "Q\n");
    }

    if (oPageContext.nOCGRasterId)
        VSIFPrintfL(fp, "EMC\n");

    /* -------------------------------------------------------------- */
    /*  Write drawing instructions for vector features                */
    /* -------------------------------------------------------------- */
    int iObj = 0;
    for(size_t iLayer = 0; iLayer < oPageContext.asVectorDesc.size(); iLayer ++)
    {
        GDALPDFLayerDesc& oLayerDesc = oPageContext.asVectorDesc[iLayer];

        VSIFPrintfL(fp, "/OC /Lyr%d BDC\n", oLayerDesc.nOGCId);

        for(size_t iVector = 0; iVector < oLayerDesc.aIds.size(); iVector ++)
        {
            CPLString osName = oLayerDesc.aFeatureNames[iVector];
            if (osName.size())
            {
                VSIFPrintfL(fp, "/feature <</MCID %d>> BDC\n",
                            iObj);
            }

            iObj ++;

            VSIFPrintfL(fp, "/Vector%d Do\n", oLayerDesc.aIds[iVector]);

            if (osName.size())
            {
                VSIFPrintfL(fp, "EMC\n");
            }
        }

        VSIFPrintfL(fp, "EMC\n");
    }

    /* -------------------------------------------------------------- */
    /*  Write drawing instructions for labels of vector features      */
    /* -------------------------------------------------------------- */
    iObj = 0;
    for(size_t iLayer = 0; iLayer < oPageContext.asVectorDesc.size(); iLayer ++)
    {
        GDALPDFLayerDesc& oLayerDesc = oPageContext.asVectorDesc[iLayer];
        if (oLayerDesc.nOCGTextId)
        {
            VSIFPrintfL(fp, "/OC /Lyr%d BDC\n", oLayerDesc.nOGCId);
            VSIFPrintfL(fp, "/OC /Lyr%d BDC\n", oLayerDesc.nOCGTextId);

            for(size_t iVector = 0; iVector < oLayerDesc.aIds.size(); iVector ++)
            {
                if (oLayerDesc.aIdsText[iVector])
                {
                    CPLString osName = oLayerDesc.aFeatureNames[iVector];
                    if (osName.size())
                    {
                        VSIFPrintfL(fp, "/feature <</MCID %d>> BDC\n",
                                    iObj);
                    }

                    VSIFPrintfL(fp, "/Text%d Do\n", oLayerDesc.aIdsText[iVector]);

                    if (osName.size())
                    {
                        VSIFPrintfL(fp, "EMC\n");
                    }
                }

                iObj ++;
            }

            VSIFPrintfL(fp, "EMC\n");
            VSIFPrintfL(fp, "EMC\n");
        }
        else
            iObj += oLayerDesc.aIds.size();
    }

    /* -------------------------------------------------------------- */
    /*  Write drawing instructions for extra content.                 */
    /* -------------------------------------------------------------- */
    if (pszExtraStream || asExtraImageDesc.size())
    {
        if (nLayerExtraId)
            VSIFPrintfL(fp, "/OC /Lyr%d BDC\n", nLayerExtraId);

        /* -------------------------------------------------------------- */
        /*  Write drawing instructions for extra images.                  */
        /* -------------------------------------------------------------- */
        for(size_t iImage = 0; iImage < asExtraImageDesc.size(); iImage ++)
        {
            VSIFPrintfL(fp, "q\n");
            GDALPDFObjectRW* poXSize = GDALPDFObjectRW::CreateReal(asExtraImageDesc[iImage].dfXSize);
            GDALPDFObjectRW* poYSize = GDALPDFObjectRW::CreateReal(asExtraImageDesc[iImage].dfYSize);
            GDALPDFObjectRW* poXOff = GDALPDFObjectRW::CreateReal(asExtraImageDesc[iImage].dfXOff);
            GDALPDFObjectRW* poYOff = GDALPDFObjectRW::CreateReal(asExtraImageDesc[iImage].dfYOff);
            VSIFPrintfL(fp, "%s 0 0 %s %s %s cm\n",
                        poXSize->Serialize().c_str(),
                        poYSize->Serialize().c_str(),
                        poXOff->Serialize().c_str(),
                        poYOff->Serialize().c_str());
            delete poXSize;
            delete poYSize;
            delete poXOff;
            delete poYOff;
            VSIFPrintfL(fp, "/Image%d Do\n",
                        asExtraImageDesc[iImage].nImageId);
            VSIFPrintfL(fp, "Q\n");
        }

        if (pszExtraStream)
            VSIFPrintfL(fp, "%s\n", pszExtraStream);

        if (nLayerExtraId)
            VSIFPrintfL(fp, "EMC\n");
    }

    if (fpGZip)
        VSIFCloseL(fpGZip);
    fp = fpBack;

    vsi_l_offset nStreamEnd = VSIFTellL(fp);
    if (fpGZip)
        VSIFPrintfL(fp, "\n");
    VSIFPrintfL(fp, "endstream\n");
    EndObj();

    StartObj(nContentLengthId);
    VSIFPrintfL(fp,
                "   %ld\n",
                (long)(nStreamEnd - nStreamStart));
    EndObj();

    /* -------------------------------------------------------------- */
    /*  Write objects for feature tree.                               */
    /* -------------------------------------------------------------- */
    if (nStructTreeRootId)
    {
        int nParentTreeId = AllocNewObject();
        StartObj(nParentTreeId);
        VSIFPrintfL(fp, "<< /Nums [ 0 ");
        VSIFPrintfL(fp, "[ ");
        for(size_t iLayer = 0; iLayer < oPageContext.asVectorDesc.size(); iLayer ++)
        {
            GDALPDFLayerDesc& oLayerDesc = oPageContext.asVectorDesc[iLayer];
            for(size_t iVector = 0; iVector < oLayerDesc.aIds.size(); iVector ++)
            {
                int nId = oLayerDesc.aUserPropertiesIds[iVector];
                if (nId)
                    VSIFPrintfL(fp, "%d 0 R ", nId);
            }
        }
        VSIFPrintfL(fp, " ]\n");
        VSIFPrintfL(fp, " ] >> \n");
        EndObj();

        StartObj(nStructTreeRootId);
        VSIFPrintfL(fp,
                    "<< "
                    "/Type /StructTreeRoot "
                    "/ParentTree %d 0 R "
                    "/K [ ", nParentTreeId);
        for(size_t iLayer = 0; iLayer < oPageContext.asVectorDesc.size(); iLayer ++)
        {
            VSIFPrintfL(fp, "%d 0 R ", oPageContext.asVectorDesc[iLayer]. nFeatureLayerId);
        }
        VSIFPrintfL(fp,"] >>\n");
        EndObj();
    }

    /* -------------------------------------------------------------- */
    /*  Write page resource dictionary.                               */
    /* -------------------------------------------------------------- */
    StartObj(oPageContext.nResourcesId);
    {
        GDALPDFDictionaryRW oDict;
        GDALPDFDictionaryRW* poDictXObject = new GDALPDFDictionaryRW();
        oDict.Add("XObject", poDictXObject);
        size_t iImage;
        for(iImage = 0; iImage < oPageContext.asImageDesc.size(); iImage ++)
        {
            poDictXObject->Add(CPLSPrintf("Image%d", oPageContext.asImageDesc[iImage].nImageId),
                               oPageContext.asImageDesc[iImage].nImageId, 0);
        }
        for(iImage = 0; iImage < asExtraImageDesc.size(); iImage ++)
        {
            poDictXObject->Add(CPLSPrintf("Image%d", asExtraImageDesc[iImage].nImageId),
                               asExtraImageDesc[iImage].nImageId, 0);
        }
        for(size_t iLayer = 0; iLayer < oPageContext.asVectorDesc.size(); iLayer ++)
        {
            GDALPDFLayerDesc& oLayerDesc = oPageContext.asVectorDesc[iLayer];
            for(size_t iVector = 0; iVector < oLayerDesc.aIds.size(); iVector ++)
            {
                poDictXObject->Add(CPLSPrintf("Vector%d", oLayerDesc.aIds[iVector]),
                                oLayerDesc.aIds[iVector], 0);
                if (oLayerDesc.aIdsText[iVector])
                    poDictXObject->Add(CPLSPrintf("Text%d", oLayerDesc.aIdsText[iVector]),
                                oLayerDesc.aIdsText[iVector], 0);
            }
        }

        GDALPDFDictionaryRW* poDictFTimesRoman = NULL;
        if (bHasTimesRoman)
        {
            poDictFTimesRoman = new GDALPDFDictionaryRW();
            poDictFTimesRoman->Add("Type", GDALPDFObjectRW::CreateName("Font"));
            poDictFTimesRoman->Add("BaseFont", GDALPDFObjectRW::CreateName("Times-Roman"));
            poDictFTimesRoman->Add("Encoding", GDALPDFObjectRW::CreateName("WinAnsiEncoding"));
            poDictFTimesRoman->Add("Subtype", GDALPDFObjectRW::CreateName("Type1"));
        }

        GDALPDFDictionaryRW* poDictFTimesBold = NULL;
        if (bHasTimesBold)
        {
            poDictFTimesBold = new GDALPDFDictionaryRW();
            poDictFTimesBold->Add("Type", GDALPDFObjectRW::CreateName("Font"));
            poDictFTimesBold->Add("BaseFont", GDALPDFObjectRW::CreateName("Times-Bold"));
            poDictFTimesBold->Add("Encoding", GDALPDFObjectRW::CreateName("WinAnsiEncoding"));
            poDictFTimesBold->Add("Subtype", GDALPDFObjectRW::CreateName("Type1"));
        }

        if (poDictFTimesRoman != NULL || poDictFTimesBold != NULL)
        {
            GDALPDFDictionaryRW* poDictFont = new GDALPDFDictionaryRW();
            if (poDictFTimesRoman)
                poDictFont->Add("FTimesRoman", poDictFTimesRoman);
            if (poDictFTimesBold)
                poDictFont->Add("FTimesBold", poDictFTimesBold);
            oDict.Add("Font", poDictFont);
        }

        if (asOCGs.size())
        {
            GDALPDFDictionaryRW* poDictProperties = new GDALPDFDictionaryRW();
            for(size_t i=0; i<asOCGs.size(); i++)
                poDictProperties->Add(CPLSPrintf("Lyr%d", asOCGs[i].nId),
                                      asOCGs[i].nId, 0);
            oDict.Add("Properties", poDictProperties);
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

    if (eCompressMethod == COMPRESS_DEFAULT)
    {
        GDALDataset* poSrcDSToTest = poSrcDS;

        /* Test if we can directly copy original JPEG content */
        /* if available */
        if (poSrcDS->GetDriver() != NULL &&
            poSrcDS->GetDriver() == GDALGetDriverByName("VRT"))
        {
            VRTDataset* poVRTDS = (VRTDataset* )poSrcDS;
            poSrcDSToTest = poVRTDS->GetSingleSimpleSource();
        }

        if (poSrcDSToTest != NULL &&
            poSrcDSToTest->GetDriver() != NULL &&
            poSrcDSToTest->GetDriver() == GDALGetDriverByName("JPEG") &&
            nXOff == 0 && nYOff == 0 &&
            nReqXSize == poSrcDSToTest->GetRasterXSize() &&
            nReqYSize == poSrcDSToTest->GetRasterYSize() &&
            nJPEGQuality < 0)
        {
            VSILFILE* fpSrc = VSIFOpenL(poSrcDSToTest->GetDescription(), "rb");
            if (fpSrc != NULL)
            {
                CPLDebug("PDF", "Copying directly original JPEG file");

                VSIFSeekL(fpSrc, 0, SEEK_END);
                int nLength = (int)VSIFTellL(fpSrc);
                VSIFSeekL(fpSrc, 0, SEEK_SET);

                int nImageId = AllocNewObject();

                StartObj(nImageId);

                GDALPDFDictionaryRW oDict;
                oDict.Add("Length", nLength)
                     .Add("Type", GDALPDFObjectRW::CreateName("XObject"))
                     .Add("Filter", GDALPDFObjectRW::CreateName("DCTDecode"))
                     .Add("Subtype", GDALPDFObjectRW::CreateName("Image"))
                     .Add("Width", nReqXSize)
                     .Add("Height", nReqYSize)
                     .Add("ColorSpace",
                        (nBands == 1) ?        GDALPDFObjectRW::CreateName("DeviceGray") :
                                                GDALPDFObjectRW::CreateName("DeviceRGB"))
                     .Add("BitsPerComponent", 8);
                VSIFPrintfL(fp, "%s\n", oDict.Serialize().c_str());
                VSIFPrintfL(fp, "stream\n");

                GByte abyBuffer[1024];
                for(int i=0;i<nLength;i += 1024)
                {
                    int nRead = (int) VSIFReadL(abyBuffer, 1, 1024, fpSrc);
                    if ((int)VSIFWriteL(abyBuffer, 1, nRead, fp) != nRead)
                    {
                        eErr = CE_Failure;
                        break;
                    }

                    if( eErr == CE_None && pfnProgress != NULL
                        && !pfnProgress( (i + nRead) / (double)nLength,
                                        NULL, pProgressData ) )
                    {
                        CPLError( CE_Failure, CPLE_UserInterrupt,
                                "User terminated CreateCopy()" );
                        eErr = CE_Failure;
                        break;
                    }
                }

                VSIFPrintfL(fp, "\nendstream\n");

                EndObj();

                VSIFCloseL(fpSrc);

                return eErr == CE_None ? nImageId : 0;
            }
        }

        eCompressMethod = COMPRESS_DEFLATE;
    }

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
            oDict.Add("DecodeParms", &((new GDALPDFDictionaryRW())
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

            if( eErr == CE_None && pfnProgress != NULL
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
        if (asOCGs.size())
        {
            GDALPDFDictionaryRW* poDictOCProperties = new GDALPDFDictionaryRW();
            oDict.Add("OCProperties", poDictOCProperties);

            GDALPDFDictionaryRW* poDictD = new GDALPDFDictionaryRW();
            poDictOCProperties->Add("D", poDictD);

            GDALPDFArrayRW* poArrayOrder = new GDALPDFArrayRW();
            size_t i;
            for(i=0;i<asOCGs.size();i++)
            {
                poArrayOrder->Add(asOCGs[i].nId, 0);
                if (i + 1 < asOCGs.size() && asOCGs[i+1].nParentId == asOCGs[i].nId)
                {
                    GDALPDFArrayRW* poSubArrayOrder = new GDALPDFArrayRW();
                    poSubArrayOrder->Add(asOCGs[i+1].nId, 0);
                    poArrayOrder->Add(poSubArrayOrder);
                    i ++;
                }
            }
            poDictD->Add("Order", poArrayOrder);


            GDALPDFArrayRW* poArrayOGCs = new GDALPDFArrayRW();
            for(i=0;i<asOCGs.size();i++)
                poArrayOGCs->Add(asOCGs[i].nId, 0);
            poDictOCProperties->Add("OCGs", poArrayOGCs);
        }

        if (nStructTreeRootId)
        {
            GDALPDFDictionaryRW* poDictMarkInfo = new GDALPDFDictionaryRW();
            oDict.Add("MarkInfo", poDictMarkInfo);
            poDictMarkInfo->Add("UserProperties", GDALPDFObjectRW::CreateBool(TRUE));

            oDict.Add("StructTreeRoot", nStructTreeRootId, 0);
        }

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
    PDFCompressMethod eCompressMethod = COMPRESS_DEFAULT;
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

    PDFCompressMethod eStreamCompressMethod = COMPRESS_DEFLATE;
    const char* pszStreamCompressMethod = CSLFetchNameValue(papszOptions, "STREAM_COMPRESS");
    if (pszStreamCompressMethod)
    {
        if( EQUAL(pszStreamCompressMethod, "NONE") )
            eStreamCompressMethod = COMPRESS_NONE;
        else if( EQUAL(pszStreamCompressMethod, "DEFLATE") )
            eStreamCompressMethod = COMPRESS_DEFLATE;
        else
        {
            CPLError( (bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported,
                    "Unsupported value for STREAM_COMPRESS.");

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
        if (eCompressMethod == COMPRESS_DEFAULT)
            eCompressMethod = COMPRESS_DEFLATE;

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

    const char* pszNEATLINE = CSLFetchNameValue(papszOptions, "NEATLINE");

    int nMargin = atoi(CSLFetchNameValueDef(papszOptions, "MARGIN", "0"));

    PDFMargins sMargins;
    sMargins.nLeft = nMargin;
    sMargins.nRight = nMargin;
    sMargins.nTop = nMargin;
    sMargins.nBottom = nMargin;

    const char* pszLeftMargin = CSLFetchNameValue(papszOptions, "LEFT_MARGIN");
    if (pszLeftMargin) sMargins.nLeft = atoi(pszLeftMargin);

    const char* pszRightMargin = CSLFetchNameValue(papszOptions, "RIGHT_MARGIN");
    if (pszRightMargin) sMargins.nRight = atoi(pszRightMargin);

    const char* pszTopMargin = CSLFetchNameValue(papszOptions, "TOP_MARGIN");
    if (pszTopMargin) sMargins.nTop = atoi(pszTopMargin);

    const char* pszBottomMargin = CSLFetchNameValue(papszOptions, "BOTTOM_MARGIN");
    if (pszBottomMargin) sMargins.nBottom = atoi(pszBottomMargin);

    const char* pszLayerName = CSLFetchNameValue(papszOptions, "LAYER_NAME");

    const char* pszExtraImages = CSLFetchNameValue(papszOptions, "EXTRA_IMAGES");
    const char* pszExtraStream = CSLFetchNameValue(papszOptions, "EXTRA_STREAM");
    const char* pszExtraLayerName = CSLFetchNameValue(papszOptions, "EXTRA_LAYER_NAME");

    const char* pszOGRDataSource = CSLFetchNameValue(papszOptions, "OGR_DATASOURCE");
    const char* pszOGRDisplayField = CSLFetchNameValue(papszOptions, "OGR_DISPLAY_FIELD");
    const char* pszOGRDisplayLayerNames = CSLFetchNameValue(papszOptions, "OGR_DISPLAY_LAYER_NAMES");
    int bWriteOGRAttributes = CSLFetchBoolean(papszOptions, "OGR_WRITE_ATTRIBUTES", TRUE);

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

    oWriter.StartPage(poSrcDS,
                      dfDPI,
                      pszGEO_ENCODING,
                      pszNEATLINE,
                      &sMargins,
                      eStreamCompressMethod,
                      pszOGRDataSource != NULL && bWriteOGRAttributes);

    int bRet = oWriter.WriteImagery(pszLayerName,
                                    eCompressMethod,
                                    nPredictor,
                                    nJPEGQuality,
                                    pszJPEG2000_DRIVER,
                                    nBlockXSize, nBlockYSize,
                                    pfnProgress, pProgressData);

#ifdef OGR_ENABLED
    if (bRet && pszOGRDataSource != NULL)
        oWriter.WriteOGRDataSource(pszOGRDataSource,
                                   pszOGRDisplayField,
                                   pszOGRDisplayLayerNames,
                                   bWriteOGRAttributes);
#endif

    if (bRet)
        oWriter.EndPage(pszExtraImages,
                        pszExtraStream,
                        pszExtraLayerName);
    oWriter.Close();

    if (!bRet)
    {
        VSIUnlink(pszFilename);
        return NULL;
    }
    else
    {
#if defined(HAVE_POPPLER) || defined(HAVE_PODOFO)
        return (GDALDataset*) GDALOpen(pszFilename, GA_ReadOnly);
#else
        return new GDALFakePDFDataset();
#endif
    }
}
