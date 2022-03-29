/******************************************************************************
 *
 * Project:  PDF driver
 * Purpose:  GDALDataset driver for PDF dataset.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2012-2019, Even Rouault <even dot rouault at spatialys.com>
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

#include "gdal_pdf.h"
#include "pdfcreatecopy.h"

#include "cpl_vsi_virtual.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "ogr_spatialref.h"
#include "ogr_geometry.h"
#include "vrtdataset.h"

#include "pdfobject.h"

#include <cmath>
#include <algorithm>
#include <utility>
#include <vector>

CPL_CVSID("$Id$")

/************************************************************************/
/*                        GDALPDFBaseWriter()                           */
/************************************************************************/

GDALPDFBaseWriter::GDALPDFBaseWriter(VSILFILE* fp):
    m_fp(fp)
{}


/************************************************************************/
/*                       ~GDALPDFBaseWriter()                           */
/************************************************************************/

GDALPDFBaseWriter::~GDALPDFBaseWriter()
{
    Close();
}

/************************************************************************/
/*                              ~Close()                                */
/************************************************************************/

void GDALPDFBaseWriter::Close()
{
    if( m_fp )
    {
        VSIFCloseL(m_fp);
        m_fp = nullptr;
    }
}

/************************************************************************/
/*                           GDALPDFUpdateWriter()                      */
/************************************************************************/

GDALPDFUpdateWriter::GDALPDFUpdateWriter(VSILFILE* fp):
    GDALPDFBaseWriter(fp)
{}

/************************************************************************/
/*                          ~GDALPDFUpdateWriter()                      */
/************************************************************************/

GDALPDFUpdateWriter::~GDALPDFUpdateWriter()
{
    Close();
}

/************************************************************************/
/*                              ~Close()                                */
/************************************************************************/

void GDALPDFUpdateWriter::Close()
{
    if (m_fp)
    {
        CPLAssert(!m_bInWriteObj);
        if (m_bUpdateNeeded)
        {
            WriteXRefTableAndTrailer(true, m_nLastStartXRef);
        }
    }
    GDALPDFBaseWriter::Close();
}


/************************************************************************/
/*                          StartNewDoc()                               */
/************************************************************************/

void GDALPDFBaseWriter::StartNewDoc()
{
    VSIFPrintfL(m_fp, "%%PDF-1.6\n");

    // See PDF 1.7 reference, page 92. Write 4 non-ASCII bytes to indicate
    // that the content will be binary.
    VSIFPrintfL(m_fp, "%%%c%c%c%c\n", 0xFF, 0xFF, 0xFF, 0xFF);

    m_nPageResourceId = AllocNewObject();
    m_nCatalogId = AllocNewObject();
}

/************************************************************************/
/*                         GDALPDFWriter()                              */
/************************************************************************/

GDALPDFWriter::GDALPDFWriter( VSILFILE* fpIn ) :
    GDALPDFBaseWriter(fpIn)
{
    StartNewDoc();
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

static int ParseIndirectRef(const char* pszStr, GDALPDFObjectNum& nNum, int &nGen)
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

int GDALPDFUpdateWriter::ParseTrailerAndXRef()
{
    VSIFSeekL(m_fp, 0, SEEK_END);
    char szBuf[1024+1];
    vsi_l_offset nOffset = VSIFTellL(m_fp);

    if (nOffset > 128)
        nOffset -= 128;
    else
        nOffset = 0;

    /* Find startxref section */
    VSIFSeekL(m_fp, nOffset, SEEK_SET);
    int nRead = (int) VSIFReadL(szBuf, 1, 128, m_fp);
    szBuf[nRead] = 0;
    if (nRead < 9)
        return FALSE;

    const char* pszStartXRef = nullptr;
    int i;
    for(i = nRead - 9; i>= 0; i --)
    {
        if (STARTS_WITH(szBuf + i, "startxref"))
        {
            pszStartXRef = szBuf + i;
            break;
        }
    }
    if (pszStartXRef == nullptr)
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

    m_nLastStartXRef = CPLScanUIntBig(pszStartXRef,16);

    /* Skip to beginning of xref section */
    VSIFSeekL(m_fp, m_nLastStartXRef, SEEK_SET);

    /* And skip to trailer */
    const char* pszLine = nullptr;
    while( (pszLine = CPLReadLineL(m_fp)) != nullptr)
    {
        if (STARTS_WITH(pszLine, "trailer"))
            break;
    }

    if( pszLine == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find trailer");
        return FALSE;
    }

    /* Read trailer content */
    nRead = (int) VSIFReadL(szBuf, 1, 1024, m_fp);
    szBuf[nRead] = 0;

    /* Find XRef size */
    const char* pszSize = strstr(szBuf, "/Size");
    if (pszSize == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find trailer /Size");
        return FALSE;
    }
    pszSize += 5;
    while(*pszSize == ' ')
        pszSize ++;
    m_nLastXRefSize = atoi(pszSize);

    /* Find Root object */
    const char* pszRoot = strstr(szBuf, "/Root");
    if (pszRoot == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find trailer /Root");
        return FALSE;
    }
    pszRoot += 5;
    while(*pszRoot == ' ')
        pszRoot ++;

    if (!ParseIndirectRef(pszRoot, m_nCatalogId, m_nCatalogGen))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot parse trailer /Root");
        return FALSE;
    }

    /* Find Info object */
    const char* pszInfo = strstr(szBuf, "/Info");
    if (pszInfo != nullptr)
    {
        pszInfo += 5;
        while(*pszInfo == ' ')
            pszInfo ++;

        if (!ParseIndirectRef(pszInfo, m_nInfoId, m_nInfoGen))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot parse trailer /Info");
            m_nInfoId = 0;
            m_nInfoGen = 0;
        }
    }

    VSIFSeekL(m_fp, 0, SEEK_END);

    return TRUE;
}

/************************************************************************/
/*                              Close()                                 */
/************************************************************************/

void GDALPDFWriter::Close()
{
    if (m_fp)
    {
        CPLAssert(!m_bInWriteObj);
        if (m_nPageResourceId.toBool())
        {
            WritePages();
            WriteXRefTableAndTrailer(false, 0);
        }
    }
    GDALPDFBaseWriter::Close();
}

/************************************************************************/
/*                           UpdateProj()                               */
/************************************************************************/

void GDALPDFUpdateWriter::UpdateProj(GDALDataset* poSrcDS,
                               double dfDPI,
                               GDALPDFDictionaryRW* poPageDict,
                               const GDALPDFObjectNum& nPageId, int nPageGen)
{
    m_bUpdateNeeded = true;
    if ((int)m_asXRefEntries.size() < m_nLastXRefSize - 1)
        m_asXRefEntries.resize(m_nLastXRefSize - 1);

    GDALPDFObjectNum nViewportId;
    GDALPDFObjectNum nLGIDictId;

    CPLAssert(nPageId.toBool());
    CPLAssert(poPageDict != nullptr);

    PDFMargins sMargins;

    const char* pszGEO_ENCODING = CPLGetConfigOption("GDAL_PDF_GEO_ENCODING", "ISO32000");
    if (EQUAL(pszGEO_ENCODING, "ISO32000") || EQUAL(pszGEO_ENCODING, "BOTH"))
        nViewportId = WriteSRS_ISO32000(poSrcDS, dfDPI * USER_UNIT_IN_INCH, nullptr, &sMargins, TRUE);
    if (EQUAL(pszGEO_ENCODING, "OGC_BP") || EQUAL(pszGEO_ENCODING, "BOTH"))
        nLGIDictId = WriteSRS_OGC_BP(poSrcDS, dfDPI * USER_UNIT_IN_INCH, nullptr, &sMargins);

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
            m_asXRefEntries[nVPId - 1].bFree = TRUE;
            m_asXRefEntries[nVPId - 1].nGen ++;
        }
    }
#endif

    poPageDict->Remove("VP");
    poPageDict->Remove("LGIDict");

    if (nViewportId.toBool())
    {
        poPageDict->Add("VP", &((new GDALPDFArrayRW())->
                Add(nViewportId, 0)));
    }

    if (nLGIDictId.toBool())
    {
        poPageDict->Add("LGIDict", nLGIDictId, 0);
    }

    StartObj(nPageId, nPageGen);
    VSIFPrintfL(m_fp, "%s\n", poPageDict->Serialize().c_str());
    EndObj();
}

/************************************************************************/
/*                           UpdateInfo()                               */
/************************************************************************/

void GDALPDFUpdateWriter::UpdateInfo(GDALDataset* poSrcDS)
{
    m_bUpdateNeeded = true;
    if ((int)m_asXRefEntries.size() < m_nLastXRefSize - 1)
        m_asXRefEntries.resize(m_nLastXRefSize - 1);

    auto nNewInfoId = SetInfo(poSrcDS, nullptr);
    /* Write empty info, because podofo driver will find the dangling info instead */
    if (!nNewInfoId.toBool() && m_nInfoId.toBool())
    {
#ifdef invalidate_xref_entry
        m_asXRefEntries[m_nInfoId.toInt() - 1].bFree = TRUE;
        m_asXRefEntries[m_nInfoId.toInt() - 1].nGen ++;
#else
        StartObj(m_nInfoId, m_nInfoGen);
        VSIFPrintfL(m_fp, "<< >>\n");
        EndObj();
#endif
    }
}

/************************************************************************/
/*                           UpdateXMP()                                */
/************************************************************************/

void GDALPDFUpdateWriter::UpdateXMP(GDALDataset* poSrcDS,
                              GDALPDFDictionaryRW* poCatalogDict)
{
    m_bUpdateNeeded = true;
    if ((int)m_asXRefEntries.size() < m_nLastXRefSize - 1)
        m_asXRefEntries.resize(m_nLastXRefSize - 1);

    CPLAssert(m_nCatalogId.toBool());
    CPLAssert(poCatalogDict != nullptr);

    GDALPDFObject* poMetadata = poCatalogDict->Get("Metadata");
    if (poMetadata)
    {
        m_nXMPId = poMetadata->GetRefNum();
        m_nXMPGen = poMetadata->GetRefGen();
    }

    poCatalogDict->Remove("Metadata");
    auto nNewXMPId = SetXMP(poSrcDS, nullptr);

    /* Write empty metadata, because podofo driver will find the dangling info instead */
    if (!nNewXMPId.toBool() && m_nXMPId.toBool())
    {
        StartObj(m_nXMPId, m_nXMPGen);
        VSIFPrintfL(m_fp, "<< >>\n");
        EndObj();
    }

    if (m_nXMPId.toBool())
        poCatalogDict->Add("Metadata", m_nXMPId, 0);

    StartObj(m_nCatalogId, m_nCatalogGen);
    VSIFPrintfL(m_fp, "%s\n", poCatalogDict->Serialize().c_str());
    EndObj();
}

/************************************************************************/
/*                           AllocNewObject()                           */
/************************************************************************/

GDALPDFObjectNum GDALPDFBaseWriter::AllocNewObject()
{
    m_asXRefEntries.push_back(GDALXRefEntry());
    return GDALPDFObjectNum(static_cast<int>(m_asXRefEntries.size()));
}

/************************************************************************/
/*                        WriteXRefTableAndTrailer()                    */
/************************************************************************/

void GDALPDFBaseWriter::WriteXRefTableAndTrailer(bool bUpdate,
                                                 vsi_l_offset nLastStartXRef)
{
    vsi_l_offset nOffsetXREF = VSIFTellL(m_fp);
    VSIFPrintfL(m_fp, "xref\n");

    char buffer[16];
    if (bUpdate)
    {
        VSIFPrintfL(m_fp, "0 1\n");
        VSIFPrintfL(m_fp, "0000000000 65535 f \n");
        for(size_t i=0;i<m_asXRefEntries.size();)
        {
            if (m_asXRefEntries[i].nOffset != 0 || m_asXRefEntries[i].bFree)
            {
                /* Find number of consecutive objects */
                size_t nCount = 1;
                while(i + nCount <m_asXRefEntries.size() &&
                    (m_asXRefEntries[i + nCount].nOffset != 0 || m_asXRefEntries[i + nCount].bFree))
                    nCount ++;

                VSIFPrintfL(m_fp, "%d %d\n", (int)i + 1, (int)nCount);
                size_t iEnd = i + nCount;
                for(; i < iEnd; i++)
                {
                    snprintf (buffer, sizeof(buffer),
                              "%010" CPL_FRMT_GB_WITHOUT_PREFIX "u",
                              m_asXRefEntries[i].nOffset);
                    VSIFPrintfL(m_fp, "%s %05d %c \n",
                                buffer, m_asXRefEntries[i].nGen,
                                m_asXRefEntries[i].bFree ? 'f' : 'n');
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
        VSIFPrintfL(m_fp, "%d %d\n",
                    0, (int)m_asXRefEntries.size() + 1);
        VSIFPrintfL(m_fp, "0000000000 65535 f \n");
        for(size_t i=0;i<m_asXRefEntries.size();i++)
        {
            snprintf (buffer, sizeof(buffer),
                      "%010" CPL_FRMT_GB_WITHOUT_PREFIX "u",
                      m_asXRefEntries[i].nOffset);
            VSIFPrintfL(m_fp, "%s %05d n \n", buffer, m_asXRefEntries[i].nGen);
        }
    }

    VSIFPrintfL(m_fp, "trailer\n");
    GDALPDFDictionaryRW oDict;
    oDict.Add("Size", (int)m_asXRefEntries.size() + 1)
         .Add("Root", m_nCatalogId, m_nCatalogGen);
    if (m_nInfoId.toBool())
        oDict.Add("Info", m_nInfoId, m_nInfoGen);
    if (nLastStartXRef)
        oDict.Add("Prev", (double)nLastStartXRef);
    VSIFPrintfL(m_fp, "%s\n", oDict.Serialize().c_str());

    VSIFPrintfL(m_fp,
                "startxref\n"
                CPL_FRMT_GUIB "\n"
                "%%%%EOF\n",
                nOffsetXREF);
}

/************************************************************************/
/*                              StartObj()                              */
/************************************************************************/

void GDALPDFBaseWriter::StartObj(const GDALPDFObjectNum& nObjectId, int nGen)
{
    CPLAssert(!m_bInWriteObj);
    CPLAssert(nObjectId.toInt() - 1 < (int)m_asXRefEntries.size());
    CPLAssert(m_asXRefEntries[nObjectId.toInt() - 1].nOffset == 0);
    m_asXRefEntries[nObjectId.toInt() - 1].nOffset = VSIFTellL(m_fp);
    m_asXRefEntries[nObjectId.toInt() - 1].nGen = nGen;
    VSIFPrintfL(m_fp, "%d %d obj\n", nObjectId.toInt(), nGen);
    m_bInWriteObj = true;
}

/************************************************************************/
/*                               EndObj()                               */
/************************************************************************/

void GDALPDFBaseWriter::EndObj()
{
    CPLAssert(m_bInWriteObj);
    CPLAssert(!m_fpBack);
    VSIFPrintfL(m_fp, "endobj\n");
    m_bInWriteObj = false;
}

/************************************************************************/
/*                         StartObjWithStream()                         */
/************************************************************************/

void GDALPDFBaseWriter::StartObjWithStream(const GDALPDFObjectNum& nObjectId,
                                           GDALPDFDictionaryRW& oDict,
                                           bool bDeflate)
{
    CPLAssert(!m_nContentLengthId.toBool());
    CPLAssert(!m_fpGZip);
    CPLAssert(!m_fpBack);
    CPLAssert(m_nStreamStart == 0);

    m_nContentLengthId = AllocNewObject();

    StartObj(nObjectId);
    {
        oDict.Add("Length", m_nContentLengthId, 0);
        if( bDeflate )
        {
            oDict.Add("Filter", GDALPDFObjectRW::CreateName("FlateDecode"));
        }
        VSIFPrintfL(m_fp, "%s\n", oDict.Serialize().c_str());
    }

    /* -------------------------------------------------------------- */
    /*  Write content stream                                          */
    /* -------------------------------------------------------------- */
    VSIFPrintfL(m_fp, "stream\n");
    m_nStreamStart = VSIFTellL(m_fp);

    m_fpGZip = nullptr;
    m_fpBack = m_fp;
    if( bDeflate )
    {
        m_fpGZip = reinterpret_cast<VSILFILE*>(VSICreateGZipWritable(
            reinterpret_cast<VSIVirtualHandle*>(m_fp), TRUE, FALSE ));
        m_fp = m_fpGZip;
    }
}

/************************************************************************/
/*                          EndObjWithStream()                          */
/************************************************************************/

void GDALPDFBaseWriter::EndObjWithStream()
{
    if (m_fpGZip)
        VSIFCloseL(m_fpGZip);
    m_fp = m_fpBack;
    m_fpBack = nullptr;

    vsi_l_offset nStreamEnd = VSIFTellL(m_fp);
    if (m_fpGZip)
        VSIFPrintfL(m_fp, "\n");
    m_fpGZip = nullptr;
    VSIFPrintfL(m_fp, "endstream\n");
    EndObj();

    StartObj(m_nContentLengthId);
    VSIFPrintfL(m_fp,
                "   %ld\n",
                static_cast<long>(nStreamEnd - m_nStreamStart));
    EndObj();

    m_nContentLengthId = GDALPDFObjectNum();
    m_nStreamStart = 0;
}

/************************************************************************/
/*                         GDALPDFFind4Corners()                        */
/************************************************************************/

static
void GDALPDFFind4Corners(const GDAL_GCP* pasGCPList,
                         int& iUL, int& iUR, int& iLR, int& iLL)
{
    double dfMeanX = 0.0;
    double dfMeanY = 0.0;
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

GDALPDFObjectNum GDALPDFBaseWriter::WriteSRS_ISO32000(GDALDataset* poSrcDS,
                                      double dfUserUnit,
                                      const char* pszNEATLINE,
                                      PDFMargins* psMargins,
                                      int bWriteViewport)
{
    int  nWidth = poSrcDS->GetRasterXSize();
    int  nHeight = poSrcDS->GetRasterYSize();
    const char* pszWKT = poSrcDS->GetProjectionRef();
    double adfGeoTransform[6];

    int bHasGT = (poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None);
    const GDAL_GCP* pasGCPList = (poSrcDS->GetGCPCount() == 4) ? poSrcDS->GetGCPs() : nullptr;
    if (pasGCPList != nullptr)
        pszWKT = poSrcDS->GetGCPProjection();

    if( !bHasGT && pasGCPList == nullptr )
        return GDALPDFObjectNum();

    if( pszWKT == nullptr || EQUAL(pszWKT, "") )
        return GDALPDFObjectNum();

    double adfGPTS[8];

    double dfULPixel = 0;
    double dfULLine = 0;
    double dfLRPixel = nWidth;
    double dfLRLine = nHeight;

    GDAL_GCP asNeatLineGCPs[4];
    if (pszNEATLINE == nullptr)
        pszNEATLINE = poSrcDS->GetMetadataItem("NEATLINE");
    if( bHasGT && pszNEATLINE != nullptr && pszNEATLINE[0] != '\0' )
    {
        OGRGeometry* poGeom = nullptr;
        OGRGeometryFactory::createFromWkt( pszNEATLINE, nullptr, &poGeom );
        if ( poGeom != nullptr && wkbFlatten(poGeom->getGeometryType()) == wkbPolygon )
        {
            OGRLineString* poLS = poGeom->toPolygon()->getExteriorRing();
            double adfGeoTransformInv[6];
            if( poLS != nullptr && poLS->getNumPoints() == 5 &&
                GDALInvGeoTransform(adfGeoTransform, adfGeoTransformInv) )
            {
                for(int i=0;i<4;i++)
                {
                    const double X = poLS->getX(i);
                    const double Y = poLS->getY(i);
                    asNeatLineGCPs[i].dfGCPX = X;
                    asNeatLineGCPs[i].dfGCPY = Y;
                    const double x =
                        adfGeoTransformInv[0] +
                        X * adfGeoTransformInv[1] +
                        Y * adfGeoTransformInv[2];
                    const double y =
                        adfGeoTransformInv[3] +
                        X * adfGeoTransformInv[4] +
                        Y * adfGeoTransformInv[5];
                    asNeatLineGCPs[i].dfGCPPixel = x;
                    asNeatLineGCPs[i].dfGCPLine = y;
                }

                int iUL = 0;
                int iUR = 0;
                int iLR = 0;
                int iLL = 0;
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
        int iUL = 0;
        int iUR = 0;
        int iLR = 0;
        int iLL = 0;
        GDALPDFFind4Corners(pasGCPList,
                            iUL,iUR, iLR, iLL);

        if (fabs(pasGCPList[iUL].dfGCPPixel - pasGCPList[iLL].dfGCPPixel) > .5 ||
            fabs(pasGCPList[iUR].dfGCPPixel - pasGCPList[iLR].dfGCPPixel) > .5 ||
            fabs(pasGCPList[iUL].dfGCPLine - pasGCPList[iUR].dfGCPLine) > .5 ||
            fabs(pasGCPList[iLL].dfGCPLine - pasGCPList[iLR].dfGCPLine) > .5)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "GCPs should form a rectangle in pixel space");
            return GDALPDFObjectNum();
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
        adfGPTS[0] = APPLY_GT_X(adfGeoTransform, 0, 0);
        adfGPTS[1] = APPLY_GT_Y(adfGeoTransform, 0, 0);

        /* Lower-left */
        adfGPTS[2] = APPLY_GT_X(adfGeoTransform, 0, nHeight);
        adfGPTS[3] = APPLY_GT_Y(adfGeoTransform, 0, nHeight);

        /* Lower-right */
        adfGPTS[4] = APPLY_GT_X(adfGeoTransform, nWidth, nHeight);
        adfGPTS[5] = APPLY_GT_Y(adfGeoTransform, nWidth, nHeight);

        /* Upper-right */
        adfGPTS[6] = APPLY_GT_X(adfGeoTransform, nWidth, 0);
        adfGPTS[7] = APPLY_GT_Y(adfGeoTransform, nWidth, 0);
    }

    OGRSpatialReferenceH hSRS = OSRNewSpatialReference(pszWKT);
    if( hSRS == nullptr )
        return GDALPDFObjectNum();
    OSRSetAxisMappingStrategy(hSRS, OAMS_TRADITIONAL_GIS_ORDER);
    OGRSpatialReferenceH hSRSGeog = OSRCloneGeogCS(hSRS);
    if( hSRSGeog == nullptr )
    {
        OSRDestroySpatialReference(hSRS);
        return GDALPDFObjectNum();
    }
    OSRSetAxisMappingStrategy(hSRSGeog, OAMS_TRADITIONAL_GIS_ORDER);
    OGRCoordinateTransformationH hCT = OCTNewCoordinateTransformation( hSRS, hSRSGeog);
    if( hCT == nullptr )
    {
        OSRDestroySpatialReference(hSRS);
        OSRDestroySpatialReference(hSRSGeog);
        return GDALPDFObjectNum();
    }

    int bSuccess = TRUE;

    bSuccess &= (OCTTransform( hCT, 1, adfGPTS + 0, adfGPTS + 1, nullptr ) == 1);
    bSuccess &= (OCTTransform( hCT, 1, adfGPTS + 2, adfGPTS + 3, nullptr ) == 1);
    bSuccess &= (OCTTransform( hCT, 1, adfGPTS + 4, adfGPTS + 5, nullptr ) == 1);
    bSuccess &= (OCTTransform( hCT, 1, adfGPTS + 6, adfGPTS + 7, nullptr ) == 1);

    if (!bSuccess)
    {
        OSRDestroySpatialReference(hSRS);
        OSRDestroySpatialReference(hSRSGeog);
        OCTDestroyCoordinateTransformation(hCT);
        return GDALPDFObjectNum();
    }

    const char * pszAuthorityCode = OSRGetAuthorityCode( hSRS, nullptr );
    const char * pszAuthorityName = OSRGetAuthorityName( hSRS, nullptr );
    int nEPSGCode = 0;
    if( pszAuthorityName != nullptr && EQUAL(pszAuthorityName, "EPSG") &&
        pszAuthorityCode != nullptr )
        nEPSGCode = atoi(pszAuthorityCode);

    int bIsGeographic = OSRIsGeographic(hSRS);

    OSRMorphToESRI(hSRS);
    char* pszESRIWKT = nullptr;
    OSRExportToWkt(hSRS, &pszESRIWKT);

    OSRDestroySpatialReference(hSRS);
    OSRDestroySpatialReference(hSRSGeog);
    OCTDestroyCoordinateTransformation(hCT);
    hSRS = nullptr;
    hSRSGeog = nullptr;
    hCT = nullptr;

    if (pszESRIWKT == nullptr)
        return GDALPDFObjectNum();

    auto nViewportId = (bWriteViewport) ? AllocNewObject() : GDALPDFObjectNum();
    auto nMeasureId = AllocNewObject();
    auto nGCSId = AllocNewObject();

    if (nViewportId.toBool())
    {
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
        VSIFPrintfL(m_fp, "%s\n", oViewPortDict.Serialize().c_str());
        EndObj();
    }

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
    VSIFPrintfL(m_fp, "%s\n", oMeasureDict.Serialize().c_str());
    EndObj();

    StartObj(nGCSId);
    GDALPDFDictionaryRW oGCSDict;
    oGCSDict.Add("Type", GDALPDFObjectRW::CreateName(bIsGeographic ? "GEOGCS" : "PROJCS"))
            .Add("WKT", pszESRIWKT);
    if (nEPSGCode)
        oGCSDict.Add("EPSG", nEPSGCode);
    VSIFPrintfL(m_fp, "%s\n", oGCSDict.Serialize().c_str());
    EndObj();

    CPLFree(pszESRIWKT);

    return nViewportId.toBool() ? nViewportId : nMeasureId;
}

/************************************************************************/
/*                     GDALPDFBuildOGC_BP_Datum()                       */
/************************************************************************/

static GDALPDFObject* GDALPDFBuildOGC_BP_Datum(const OGRSpatialReference* poSRS)
{
    const OGR_SRSNode* poDatumNode = poSRS->GetAttrNode("DATUM");
    const char* pszDatumDescription = nullptr;
    if (poDatumNode && poDatumNode->GetChildCount() > 0)
        pszDatumDescription = poDatumNode->GetChild(0)->GetValue();

    GDALPDFObjectRW* poPDFDatum = nullptr;

    if (pszDatumDescription)
    {
        double dfSemiMajor = poSRS->GetSemiMajor();
        double dfInvFlattening = poSRS->GetInvFlattening();
        int nEPSGDatum = -1;
        const char *pszAuthority = poSRS->GetAuthorityName( "DATUM" );
        if( pszAuthority != nullptr && EQUAL(pszAuthority,"EPSG") )
            nEPSGDatum = atoi(poSRS->GetAuthorityCode( "DATUM" ));

        if( EQUAL(pszDatumDescription,SRS_DN_WGS84) || nEPSGDatum == 6326 )
            poPDFDatum = GDALPDFObjectRW::CreateString("WGE");
        else if( EQUAL(pszDatumDescription, SRS_DN_NAD27) || nEPSGDatum == 6267 )
            poPDFDatum = GDALPDFObjectRW::CreateString("NAS");
        else if( EQUAL(pszDatumDescription, SRS_DN_NAD83) || nEPSGDatum == 6269 )
            poPDFDatum = GDALPDFObjectRW::CreateString("NAR");
        else if( nEPSGDatum == 6135 )
            poPDFDatum = GDALPDFObjectRW::CreateString("OHA-M");
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

#ifdef disabled_because_terrago_toolbar_does_not_like_it
                const char* pszEllipsoidCode = NULL;
                if( std::abs(dfSemiMajor - 6378249.145) < 0.01
                    && std::abs(dfInvFlattening-293.465) < 0.0001 )
                {
                    pszEllipsoidCode = "CD";     /* Clark 1880 */
                }
                else if( std::abs(dfSemiMajor-6378245.0) < 0.01
                         x&& std::abs(dfInvFlattening-298.3) < 0.0001 )
                {
                    pszEllipsoidCode = "KA";      /* Krassovsky */
                }
                else if( std::abs(dfSemiMajor-6378388.0) < 0.01
                         && std::abs(dfInvFlattening-297.0) < 0.0001 )
                {
                    pszEllipsoidCode = "IN";       /* International 1924 */
                }
                else if( std::abs(dfSemiMajor-6378160.0) < 0.01
                         && std::abs(dfInvFlattening-298.25) < 0.0001 )
                {
                    pszEllipsoidCode = "AN";    /* Australian */
                }
                else if( std::abs(dfSemiMajor-6377397.155) < 0.01
                         && std::abs(dfInvFlattening-299.1528128) < 0.0001 )
                {
                    pszEllipsoidCode = "BR";     /* Bessel 1841 */
                }
                else if( std::abs(dfSemiMajor-6377483.865) < 0.01
                         && std::abs(dfInvFlattening-299.1528128) < 0.0001 )
                {
                    pszEllipsoidCode = "BN";   /* Bessel 1841 (Namibia / Schwarzeck)*/
                }
#if 0
                else if( std::abs(dfSemiMajor-6378160.0) < 0.01
                         && std::abs(dfInvFlattening-298.247167427) < 0.0001 )
                {
                    pszEllipsoidCode = "GRS67";      /* GRS 1967 */
                }
#endif
                else if( std::abs(dfSemiMajor-6378137) < 0.01
                         && std::abs(dfInvFlattening-298.257222101) < 0.000001 )
                {
                    pszEllipsoidCode = "RF";      /* GRS 1980 */
                }
                else if( std::abs(dfSemiMajor-6378206.4) < 0.01
                         && std::abs(dfInvFlattening-294.9786982) < 0.0001 )
                {
                    pszEllipsoidCode = "CC";     /* Clarke 1866 */
                }
                else if( std::abs(dfSemiMajor-6377340.189) < 0.01
                         && std::abs(dfInvFlattening-299.3249646) < 0.0001 )
                {
                    pszEllipsoidCode = "AM";   /* Modified Airy */
                }
                else if( std::abs(dfSemiMajor-6377563.396) < 0.01
                         && std::abs(dfInvFlattening-299.3249646) < 0.0001 )
                {
                    pszEllipsoidCode = "AA";       /* Airy */
                }
                else if( std::abs(dfSemiMajor-6378200) < 0.01
                         && std::abs(dfInvFlattening-298.3) < 0.0001 )
                {
                    pszEllipsoidCode = "HE";    /* Helmert 1906 */
                }
                else if( std::abs(dfSemiMajor-6378155) < 0.01
                         && std::abs(dfInvFlattening-298.3) < 0.0001 )
                {
                    pszEllipsoidCode = "FA";   /* Modified Fischer 1960 */
                }
#if 0
                else if( std::abs(dfSemiMajor-6377298.556) < 0.01
                         && std::abs(dfInvFlattening-300.8017) < 0.0001 )
                {
                    pszEllipsoidCode = "evrstSS";    /* Everest (Sabah & Sarawak) */
                }
                else if( std::abs(dfSemiMajor-6378165.0) < 0.01
                         && std::abs(dfInvFlattening-298.3) < 0.0001 )
                {
                    pszEllipsoidCode = "WGS60";
                }
                else if( std::abs(dfSemiMajor-6378145.0) < 0.01
                         && std::abs(dfInvFlattening-298.25) < 0.0001 )
                {
                    pszEllipsoidCode = "WGS66";
                }
#endif
                else if( std::abs(dfSemiMajor-6378135.0) < 0.01
                         && std::abs(dfInvFlattening-298.26) < 0.0001 )
                {
                    pszEllipsoidCode = "WD";
                }
                else if( std::abs(dfSemiMajor-6378137.0) < 0.01
                         && std::abs(dfInvFlattening-298.257223563) < 0.000001 )
                {
                    pszEllipsoidCode = "WE";
                }

                if( pszEllipsoidCode != NULL )
                {
                    poPDFDatumDict->Add("Ellipsoid", pszEllipsoidCode);
                }
                else
#endif /* disabled_because_terrago_toolbar_does_not_like_it */
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
                if( poTOWGS84 != nullptr
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
                else if( poTOWGS84 != nullptr && poTOWGS84->GetChildCount() >= 7)
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

    if (poPDFDatum == nullptr)
        poPDFDatum = GDALPDFObjectRW::CreateString("WGE");

    return poPDFDatum;
}

/************************************************************************/
/*                   GDALPDFBuildOGC_BP_Projection()                    */
/************************************************************************/

GDALPDFDictionaryRW* GDALPDFBaseWriter::GDALPDFBuildOGC_BP_Projection(const OGRSpatialReference* poSRS)
{

    const char* pszProjectionOGCBP = "GEOGRAPHIC";
    const char *pszProjection = poSRS->GetAttrValue("PROJECTION");

    GDALPDFDictionaryRW* poProjectionDict = new GDALPDFDictionaryRW();
    poProjectionDict->Add("Type", GDALPDFObjectRW::CreateName("Projection"));
    poProjectionDict->Add("Datum", GDALPDFBuildOGC_BP_Datum(poSRS));

    if( pszProjection == nullptr )
    {
        if( poSRS->IsGeographic() )
            pszProjectionOGCBP = "GEOGRAPHIC";
        else if( poSRS->IsLocal() )
            pszProjectionOGCBP = "LOCAL CARTESIAN";
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported, "Unsupported SRS type");
            delete poProjectionDict;
            return nullptr;
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
        const char* pszUnitName = nullptr;
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

GDALPDFObjectNum GDALPDFBaseWriter::WriteSRS_OGC_BP(GDALDataset* poSrcDS,
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
    const GDAL_GCP* pasGCPList = (nGCPCount >= 4) ? poSrcDS->GetGCPs() : nullptr;
    if (pasGCPList != nullptr)
        pszWKT = poSrcDS->GetGCPProjection();

    if( !bHasGT && pasGCPList == nullptr )
        return GDALPDFObjectNum();

    if( pszWKT == nullptr || EQUAL(pszWKT, "") )
        return GDALPDFObjectNum();

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
    if( hSRS == nullptr )
        return GDALPDFObjectNum();
    OSRSetAxisMappingStrategy(hSRS, OAMS_TRADITIONAL_GIS_ORDER);

    const OGRSpatialReference* poSRS = OGRSpatialReference::FromHandle(hSRS);
    GDALPDFDictionaryRW* poProjectionDict = GDALPDFBuildOGC_BP_Projection(poSRS);
    if (poProjectionDict == nullptr)
    {
        OSRDestroySpatialReference(hSRS);
        return GDALPDFObjectNum();
    }

    GDALPDFArrayRW* poNeatLineArray = nullptr;

    if (pszNEATLINE == nullptr)
        pszNEATLINE = poSrcDS->GetMetadataItem("NEATLINE");
    if( bHasGT && pszNEATLINE != nullptr && !EQUAL(pszNEATLINE, "NO") && pszNEATLINE[0] != '\0' )
    {
        OGRGeometry* poGeom = nullptr;
        OGRGeometryFactory::createFromWkt( pszNEATLINE, nullptr, &poGeom );
        if ( poGeom != nullptr && wkbFlatten(poGeom->getGeometryType()) == wkbPolygon )
        {
            OGRLineString* poLS = poGeom->toPolygon()->getExteriorRing();
            double adfGeoTransformInv[6];
            if( poLS != nullptr && poLS->getNumPoints() >= 5 &&
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

    if( pszNEATLINE != nullptr && EQUAL(pszNEATLINE, "NO") )
    {
        // Do nothing
    }
    else if( pasGCPList && poNeatLineArray == nullptr)
    {
        if (nGCPCount == 4)
        {
            int iUL = 0;
            int iUR = 0;
            int iLR = 0;
            int iLL = 0;
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
    else if (poNeatLineArray == nullptr)
    {
        poNeatLineArray = new GDALPDFArrayRW();

        poNeatLineArray->Add(0 / dfUserUnit + psMargins->nLeft, TRUE);
        poNeatLineArray->Add((nHeight - 0) / dfUserUnit + psMargins->nBottom, TRUE);

        poNeatLineArray->Add(0 / dfUserUnit + psMargins->nLeft, TRUE);
        poNeatLineArray->Add((/*nHeight -nHeight*/ 0) / dfUserUnit + psMargins->nBottom, TRUE);

        poNeatLineArray->Add(nWidth / dfUserUnit + psMargins->nLeft, TRUE);
        poNeatLineArray->Add((/*nHeight -nHeight*/ 0) / dfUserUnit + psMargins->nBottom, TRUE);

        poNeatLineArray->Add(nWidth / dfUserUnit + psMargins->nLeft, TRUE);
        poNeatLineArray->Add((nHeight - 0) / dfUserUnit + psMargins->nBottom, TRUE);
    }

    auto nLGIDictId = AllocNewObject();
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
    if( poNode != nullptr )
        poNode = poNode->GetChild(0);
    const char* pszDescription = nullptr;
    if( poNode != nullptr )
        pszDescription = poNode->GetValue();
    if( pszDescription )
    {
        oLGIDict.Add("Description", pszDescription);
    }

    oLGIDict.Add("Projection", poProjectionDict);

    /* GDAL extension */
    if( CPLTestBool( CPLGetConfigOption("GDAL_PDF_OGC_BP_WRITE_WKT", "TRUE") ) )
        poProjectionDict->Add("WKT", pszWKT);

    VSIFPrintfL(m_fp, "%s\n", oLGIDict.Serialize().c_str());
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
    if (pszValue == nullptr)
        pszValue = poSrcDS->GetMetadataItem(pszKey);
    if (pszValue != nullptr && pszValue[0] == '\0')
        return nullptr;
    else
        return pszValue;
}

/************************************************************************/
/*                             SetInfo()                                */
/************************************************************************/

GDALPDFObjectNum GDALPDFBaseWriter::SetInfo(GDALDataset* poSrcDS,
                               char** papszOptions)
{
    const char* pszAUTHOR = GDALPDFGetValueFromDSOrOption(poSrcDS, papszOptions, "AUTHOR");
    const char* pszPRODUCER = GDALPDFGetValueFromDSOrOption(poSrcDS, papszOptions, "PRODUCER");
    const char* pszCREATOR = GDALPDFGetValueFromDSOrOption(poSrcDS, papszOptions, "CREATOR");
    const char* pszCREATION_DATE = GDALPDFGetValueFromDSOrOption(poSrcDS, papszOptions, "CREATION_DATE");
    const char* pszSUBJECT = GDALPDFGetValueFromDSOrOption(poSrcDS, papszOptions, "SUBJECT");
    const char* pszTITLE = GDALPDFGetValueFromDSOrOption(poSrcDS, papszOptions, "TITLE");
    const char* pszKEYWORDS = GDALPDFGetValueFromDSOrOption(poSrcDS, papszOptions, "KEYWORDS");
    return SetInfo(pszAUTHOR,
                   pszPRODUCER,
                   pszCREATOR,
                   pszCREATION_DATE,
                   pszSUBJECT,
                   pszTITLE,
                   pszKEYWORDS);
}

/************************************************************************/
/*                             SetInfo()                                */
/************************************************************************/

GDALPDFObjectNum GDALPDFBaseWriter::SetInfo(const char* pszAUTHOR,
                               const char* pszPRODUCER,
                               const char* pszCREATOR,
                               const char* pszCREATION_DATE,
                               const char* pszSUBJECT,
                               const char* pszTITLE,
                               const char* pszKEYWORDS)
{
    if (pszAUTHOR == nullptr && pszPRODUCER == nullptr && pszCREATOR == nullptr && pszCREATION_DATE == nullptr &&
        pszSUBJECT == nullptr && pszTITLE == nullptr && pszKEYWORDS == nullptr)
        return GDALPDFObjectNum();

    if (!m_nInfoId.toBool())
        m_nInfoId = AllocNewObject();
    StartObj(m_nInfoId, m_nInfoGen);
    GDALPDFDictionaryRW oDict;
    if (pszAUTHOR != nullptr)
        oDict.Add("Author", pszAUTHOR);
    if (pszPRODUCER != nullptr)
        oDict.Add("Producer", pszPRODUCER);
    if (pszCREATOR != nullptr)
        oDict.Add("Creator", pszCREATOR);
    if (pszCREATION_DATE != nullptr)
        oDict.Add("CreationDate", pszCREATION_DATE);
    if (pszSUBJECT != nullptr)
        oDict.Add("Subject", pszSUBJECT);
    if (pszTITLE != nullptr)
        oDict.Add("Title", pszTITLE);
    if (pszKEYWORDS != nullptr)
        oDict.Add("Keywords", pszKEYWORDS);
    VSIFPrintfL(m_fp, "%s\n", oDict.Serialize().c_str());
    EndObj();

    return m_nInfoId;
}

/************************************************************************/
/*                             SetXMP()                                 */
/************************************************************************/

GDALPDFObjectNum  GDALPDFBaseWriter::SetXMP(GDALDataset* poSrcDS,
                           const char* pszXMP)
{
    if (pszXMP != nullptr && STARTS_WITH_CI(pszXMP, "NO"))
        return GDALPDFObjectNum();
    if (pszXMP != nullptr && pszXMP[0] == '\0')
        return GDALPDFObjectNum();

    if( poSrcDS && pszXMP == nullptr )
    {
        char** papszXMP = poSrcDS->GetMetadata("xml:XMP");
        if( papszXMP != nullptr && papszXMP[0] != nullptr)
            pszXMP = papszXMP[0];
    }

    if (pszXMP == nullptr)
        return GDALPDFObjectNum();

    CPLXMLNode* psNode = CPLParseXMLString(pszXMP);
    if (psNode == nullptr)
        return GDALPDFObjectNum();
    CPLDestroyXMLNode(psNode);

    if(!m_nXMPId.toBool())
        m_nXMPId = AllocNewObject();
    StartObj(m_nXMPId, m_nXMPGen);
    GDALPDFDictionaryRW oDict;
    oDict.Add("Type", GDALPDFObjectRW::CreateName("Metadata"))
         .Add("Subtype", GDALPDFObjectRW::CreateName("XML"))
         .Add("Length", (int)strlen(pszXMP));
    VSIFPrintfL(m_fp, "%s\n", oDict.Serialize().c_str());
    VSIFPrintfL(m_fp, "stream\n");
    VSIFPrintfL(m_fp, "%s\n", pszXMP);
    VSIFPrintfL(m_fp, "endstream\n");
    EndObj();
    return m_nXMPId;
}

/************************************************************************/
/*                              WriteOCG()                              */
/************************************************************************/

GDALPDFObjectNum GDALPDFBaseWriter::WriteOCG(const char* pszLayerName,
                                             const GDALPDFObjectNum& nParentId)
{
    if (pszLayerName == nullptr || pszLayerName[0] == '\0')
        return GDALPDFObjectNum();

    auto nOCGId = AllocNewObject();

    GDALPDFOCGDesc oOCGDesc;
    oOCGDesc.nId = nOCGId;
    oOCGDesc.nParentId = nParentId;
    oOCGDesc.osLayerName = pszLayerName;

    m_asOCGs.push_back(oOCGDesc);

    StartObj(nOCGId);
    {
        GDALPDFDictionaryRW oDict;
        oDict.Add("Type", GDALPDFObjectRW::CreateName("OCG"));
        oDict.Add("Name", pszLayerName);
        VSIFPrintfL(m_fp, "%s\n", oDict.Serialize().c_str());
    }
    EndObj();

    return nOCGId;
}

/************************************************************************/
/*                              StartPage()                             */
/************************************************************************/

bool GDALPDFWriter::StartPage(GDALDataset* poClippingDS,
                             double dfDPI,
                             bool bWriteUserUnit,
                             const char* pszGEO_ENCODING,
                             const char* pszNEATLINE,
                             PDFMargins* psMargins,
                             PDFCompressMethod eStreamCompressMethod,
                             int bHasOGRData)
{
    int  nWidth = poClippingDS->GetRasterXSize();
    int  nHeight = poClippingDS->GetRasterYSize();
    int  nBands = poClippingDS->GetRasterCount();

    double dfUserUnit = dfDPI * USER_UNIT_IN_INCH;
    double dfWidthInUserUnit = nWidth / dfUserUnit + psMargins->nLeft + psMargins->nRight;
    double dfHeightInUserUnit = nHeight / dfUserUnit + psMargins->nBottom + psMargins->nTop;

    auto nPageId = AllocNewObject();
    m_asPageId.push_back(nPageId);

    auto nContentId = AllocNewObject();
    auto nResourcesId = AllocNewObject();

    auto nAnnotsId = AllocNewObject();

    const bool bISO32000 = EQUAL(pszGEO_ENCODING, "ISO32000") ||
                    EQUAL(pszGEO_ENCODING, "BOTH");
    const bool bOGC_BP   = EQUAL(pszGEO_ENCODING, "OGC_BP") ||
                    EQUAL(pszGEO_ENCODING, "BOTH");

    GDALPDFObjectNum nViewportId;
    if( bISO32000 )
        nViewportId = WriteSRS_ISO32000(poClippingDS, dfUserUnit, pszNEATLINE, psMargins, TRUE);

    GDALPDFObjectNum nLGIDictId;
    if( bOGC_BP )
        nLGIDictId = WriteSRS_OGC_BP(poClippingDS, dfUserUnit, pszNEATLINE, psMargins);

    StartObj(nPageId);
    GDALPDFDictionaryRW oDictPage;
    oDictPage.Add("Type", GDALPDFObjectRW::CreateName("Page"))
             .Add("Parent", m_nPageResourceId, 0)
             .Add("MediaBox", &((new GDALPDFArrayRW())
                               ->Add(0).Add(0).Add(dfWidthInUserUnit).Add(dfHeightInUserUnit)));
    if( bWriteUserUnit )
      oDictPage.Add("UserUnit", dfUserUnit);
    oDictPage.Add("Contents", nContentId, 0)
             .Add("Resources", nResourcesId, 0)
             .Add("Annots", nAnnotsId, 0);

    if (nBands == 4)
    {
        oDictPage.Add("Group",
                      &((new GDALPDFDictionaryRW())
                        ->Add("Type", GDALPDFObjectRW::CreateName("Group"))
                         .Add("S", GDALPDFObjectRW::CreateName("Transparency"))
                         .Add("CS", GDALPDFObjectRW::CreateName("DeviceRGB"))));
    }
    if (nViewportId.toBool())
    {
        oDictPage.Add("VP", &((new GDALPDFArrayRW())
                               ->Add(nViewportId, 0)));
    }
    if (nLGIDictId.toBool())
    {
        oDictPage.Add("LGIDict", nLGIDictId, 0);
    }

    if (bHasOGRData)
        oDictPage.Add("StructParents", 0);

    VSIFPrintfL(m_fp, "%s\n", oDictPage.Serialize().c_str());
    EndObj();

    oPageContext.poClippingDS = poClippingDS;
    oPageContext.nPageId = nPageId;
    oPageContext.nContentId = nContentId;
    oPageContext.nResourcesId = nResourcesId;
    oPageContext.nAnnotsId = nAnnotsId;
    oPageContext.dfDPI = dfDPI;
    oPageContext.sMargins = *psMargins;
    oPageContext.eStreamCompressMethod = eStreamCompressMethod;

    return true;
}

/************************************************************************/
/*                             WriteColorTable()                        */
/************************************************************************/

GDALPDFObjectNum GDALPDFBaseWriter::WriteColorTable(GDALDataset* poSrcDS)
{
    /* Does the source image has a color table ? */
    GDALColorTable* poCT = nullptr;
    if (poSrcDS->GetRasterCount() > 0)
        poCT = poSrcDS->GetRasterBand(1)->GetColorTable();
    GDALPDFObjectNum nColorTableId;
    if (poCT != nullptr && poCT->GetColorEntryCount() <= 256)
    {
        int nColors = poCT->GetColorEntryCount();
        nColorTableId = AllocNewObject();

        auto nLookupTableId = AllocNewObject();

        /* Index object */
        StartObj(nColorTableId);
        {
            GDALPDFArrayRW oArray;
            oArray.Add(GDALPDFObjectRW::CreateName("Indexed"))
                  .Add(&((new GDALPDFArrayRW())->Add(GDALPDFObjectRW::CreateName("DeviceRGB"))))
                  .Add(nColors-1)
                  .Add(nLookupTableId, 0);
            VSIFPrintfL(m_fp, "%s\n", oArray.Serialize().c_str());
        }
        EndObj();

        /* Lookup table object */
        StartObj(nLookupTableId);
        {
            GDALPDFDictionaryRW oDict;
            oDict.Add("Length", nColors * 3);
            VSIFPrintfL(m_fp, "%s %% Lookup table\n", oDict.Serialize().c_str());
        }
        VSIFPrintfL(m_fp, "stream\n");
        GByte pabyLookup[768];
        for(int i=0;i<nColors;i++)
        {
            const GDALColorEntry* poEntry = poCT->GetColorEntry(i);
            pabyLookup[3 * i + 0] = (GByte)poEntry->c1;
            pabyLookup[3 * i + 1] = (GByte)poEntry->c2;
            pabyLookup[3 * i + 2] = (GByte)poEntry->c3;
        }
        VSIFWriteL(pabyLookup, 3 * nColors, 1, m_fp);
        VSIFPrintfL(m_fp, "\n");
        VSIFPrintfL(m_fp, "endstream\n");
        EndObj();
    }

    return nColorTableId;
}

/************************************************************************/
/*                             WriteImagery()                           */
/************************************************************************/

bool GDALPDFWriter::WriteImagery(GDALDataset* poDS,
                                const char* pszLayerName,
                                PDFCompressMethod eCompressMethod,
                                int nPredictor,
                                int nJPEGQuality,
                                const char* pszJPEG2000_DRIVER,
                                int nBlockXSize, int nBlockYSize,
                                GDALProgressFunc pfnProgress,
                                void * pProgressData)
{
    int  nWidth = poDS->GetRasterXSize();
    int  nHeight = poDS->GetRasterYSize();
    double dfUserUnit = oPageContext.dfDPI * USER_UNIT_IN_INCH;

    GDALPDFRasterDesc oRasterDesc;

    if( pfnProgress == nullptr )
        pfnProgress = GDALDummyProgress;

    oRasterDesc.nOCGRasterId = WriteOCG(pszLayerName);

    /* Does the source image has a color table ? */
    auto nColorTableId = WriteColorTable(poDS);

    int nXBlocks = DIV_ROUND_UP(nWidth, nBlockXSize);
    int nYBlocks = DIV_ROUND_UP(nHeight, nBlockYSize);
    int nBlocks = nXBlocks * nYBlocks;
    int nBlockXOff, nBlockYOff;
    for(nBlockYOff = 0; nBlockYOff < nYBlocks; nBlockYOff ++)
    {
        for(nBlockXOff = 0; nBlockXOff < nXBlocks; nBlockXOff ++)
        {
            const int nReqWidth =
                std::min(nBlockXSize, nWidth - nBlockXOff * nBlockXSize);
            const int nReqHeight =
                std::min(nBlockYSize, nHeight - nBlockYOff * nBlockYSize);
            int iImage = nBlockYOff * nXBlocks + nBlockXOff;

            void* pScaledData = GDALCreateScaledProgress( iImage / (double)nBlocks,
                                                          (iImage + 1) / (double)nBlocks,
                                                          pfnProgress, pProgressData);
            int nX = nBlockXOff * nBlockXSize;
            int nY = nBlockYOff * nBlockYSize;

            auto nImageId = WriteBlock(poDS,
                                    nX,
                                    nY,
                                    nReqWidth, nReqHeight,
                                    nColorTableId,
                                    eCompressMethod,
                                    nPredictor,
                                    nJPEGQuality,
                                    pszJPEG2000_DRIVER,
                                    GDALScaledProgress,
                                    pScaledData);

            GDALDestroyScaledProgress(pScaledData);

            if (!nImageId.toBool())
                return false;

            GDALPDFImageDesc oImageDesc;
            oImageDesc.nImageId = nImageId;
            oImageDesc.dfXOff = nX / dfUserUnit + oPageContext.sMargins.nLeft;
            oImageDesc.dfYOff = (nHeight - nY - nReqHeight) / dfUserUnit + oPageContext.sMargins.nBottom;
            oImageDesc.dfXSize = nReqWidth / dfUserUnit;
            oImageDesc.dfYSize = nReqHeight / dfUserUnit;

            oRasterDesc.asImageDesc.push_back(oImageDesc);
        }
    }

    oPageContext.asRasterDesc.push_back(oRasterDesc);

    return true;
}

/************************************************************************/
/*                        WriteClippedImagery()                         */
/************************************************************************/

bool GDALPDFWriter::WriteClippedImagery(
                                GDALDataset* poDS,
                                const char* pszLayerName,
                                PDFCompressMethod eCompressMethod,
                                int nPredictor,
                                int nJPEGQuality,
                                const char* pszJPEG2000_DRIVER,
                                int nBlockXSize, int nBlockYSize,
                                GDALProgressFunc pfnProgress,
                                void * pProgressData)
{
    double dfUserUnit = oPageContext.dfDPI * USER_UNIT_IN_INCH;

    GDALPDFRasterDesc oRasterDesc;

    /* Get clipping dataset bounding-box */
    double adfClippingGeoTransform[6];
    GDALDataset* poClippingDS = oPageContext.poClippingDS;
    poClippingDS->GetGeoTransform(adfClippingGeoTransform);
    int  nClippingWidth = poClippingDS->GetRasterXSize();
    int  nClippingHeight = poClippingDS->GetRasterYSize();
    double dfClippingMinX = adfClippingGeoTransform[0];
    double dfClippingMaxX = dfClippingMinX + nClippingWidth * adfClippingGeoTransform[1];
    double dfClippingMaxY = adfClippingGeoTransform[3];
    double dfClippingMinY = dfClippingMaxY + nClippingHeight * adfClippingGeoTransform[5];

    if( dfClippingMaxY < dfClippingMinY )
    {
        std::swap(dfClippingMinY, dfClippingMaxY);
    }

    /* Get current dataset dataset bounding-box */
    double adfGeoTransform[6];
    poDS->GetGeoTransform(adfGeoTransform);
    int  nWidth = poDS->GetRasterXSize();
    int  nHeight = poDS->GetRasterYSize();
    double dfRasterMinX = adfGeoTransform[0];
    //double dfRasterMaxX = dfRasterMinX + nWidth * adfGeoTransform[1];
    double dfRasterMaxY = adfGeoTransform[3];
    double dfRasterMinY = dfRasterMaxY + nHeight * adfGeoTransform[5];

    if( dfRasterMaxY < dfRasterMinY )
    {
        std::swap(dfRasterMinY, dfRasterMaxY);
    }

    if( pfnProgress == nullptr )
        pfnProgress = GDALDummyProgress;

    oRasterDesc.nOCGRasterId = WriteOCG(pszLayerName);

    /* Does the source image has a color table ? */
    auto nColorTableId = WriteColorTable(poDS);

    int nXBlocks = DIV_ROUND_UP(nWidth, nBlockXSize);
    int nYBlocks = DIV_ROUND_UP(nHeight, nBlockYSize);
    int nBlocks = nXBlocks * nYBlocks;
    int nBlockXOff, nBlockYOff;
    for(nBlockYOff = 0; nBlockYOff < nYBlocks; nBlockYOff ++)
    {
        for(nBlockXOff = 0; nBlockXOff < nXBlocks; nBlockXOff ++)
        {
            int nReqWidth =
                std::min(nBlockXSize, nWidth - nBlockXOff * nBlockXSize);
            int nReqHeight =
                std::min(nBlockYSize, nHeight - nBlockYOff * nBlockYSize);
            int iImage = nBlockYOff * nXBlocks + nBlockXOff;

            void* pScaledData = GDALCreateScaledProgress( iImage / (double)nBlocks,
                                                          (iImage + 1) / (double)nBlocks,
                                                          pfnProgress, pProgressData);

            int nX = nBlockXOff * nBlockXSize;
            int nY = nBlockYOff * nBlockYSize;

            /* Compute extent of block to write */
            double dfBlockMinX = adfGeoTransform[0] + nX * adfGeoTransform[1];
            double dfBlockMaxX = adfGeoTransform[0] + (nX + nReqWidth) * adfGeoTransform[1];
            double dfBlockMinY = adfGeoTransform[3] + (nY + nReqHeight) * adfGeoTransform[5];
            double dfBlockMaxY = adfGeoTransform[3] + nY * adfGeoTransform[5];

            if( dfBlockMaxY < dfBlockMinY )
            {
                std::swap(dfBlockMinY, dfBlockMaxY);
            }

            // Clip the extent of the block with the extent of the main raster.
            const double dfIntersectMinX =
                std::max(dfBlockMinX, dfClippingMinX);
            const double dfIntersectMinY =
                std::max(dfBlockMinY, dfClippingMinY);
            const double dfIntersectMaxX =
                std::min(dfBlockMaxX, dfClippingMaxX);
            const double dfIntersectMaxY =
                std::min(dfBlockMaxY, dfClippingMaxY);

            if( dfIntersectMinX < dfIntersectMaxX &&
                dfIntersectMinY < dfIntersectMaxY )
            {
                /* Re-compute (x,y,width,height) subwindow of current raster from */
                /* the extent of the clipped block */
                nX = (int)((dfIntersectMinX - dfRasterMinX) / adfGeoTransform[1] + 0.5);
                if( adfGeoTransform[5] < 0 )
                    nY = (int)((dfRasterMaxY - dfIntersectMaxY) / (-adfGeoTransform[5]) + 0.5);
                else
                    nY = (int)((dfIntersectMinY - dfRasterMinY) / adfGeoTransform[5] + 0.5);
                nReqWidth = (int)((dfIntersectMaxX - dfRasterMinX) / adfGeoTransform[1] + 0.5) - nX;
                if( adfGeoTransform[5] < 0 )
                    nReqHeight = (int)((dfRasterMaxY - dfIntersectMinY) / (-adfGeoTransform[5]) + 0.5) - nY;
                else
                    nReqHeight = (int)((dfIntersectMaxY - dfRasterMinY) / adfGeoTransform[5] + 0.5) - nY;

                if( nReqWidth > 0 && nReqHeight > 0)
                {
                    auto nImageId = WriteBlock(poDS,
                                            nX,
                                            nY,
                                            nReqWidth, nReqHeight,
                                            nColorTableId,
                                            eCompressMethod,
                                            nPredictor,
                                            nJPEGQuality,
                                            pszJPEG2000_DRIVER,
                                            GDALScaledProgress,
                                            pScaledData);

                    if (!nImageId.toBool())
                    {
                        GDALDestroyScaledProgress(pScaledData);
                        return false;
                    }

                    /* Compute the subwindow in image coordinates of the main raster corresponding */
                    /* to the extent of the clipped block */
                    double dfXInClippingUnits, dfYInClippingUnits, dfReqWidthInClippingUnits, dfReqHeightInClippingUnits;

                    dfXInClippingUnits = (dfIntersectMinX - dfClippingMinX) / adfClippingGeoTransform[1];
                    if( adfClippingGeoTransform[5] < 0 )
                        dfYInClippingUnits = (dfClippingMaxY - dfIntersectMaxY) / (-adfClippingGeoTransform[5]);
                    else
                        dfYInClippingUnits = (dfIntersectMinY - dfClippingMinY) / adfClippingGeoTransform[5];
                    dfReqWidthInClippingUnits = (dfIntersectMaxX - dfClippingMinX) / adfClippingGeoTransform[1] - dfXInClippingUnits;
                    if( adfClippingGeoTransform[5] < 0 )
                        dfReqHeightInClippingUnits = (dfClippingMaxY - dfIntersectMinY) / (-adfClippingGeoTransform[5]) - dfYInClippingUnits;
                    else
                        dfReqHeightInClippingUnits = (dfIntersectMaxY - dfClippingMinY) / adfClippingGeoTransform[5] - dfYInClippingUnits;

                    GDALPDFImageDesc oImageDesc;
                    oImageDesc.nImageId = nImageId;
                    oImageDesc.dfXOff = dfXInClippingUnits / dfUserUnit + oPageContext.sMargins.nLeft;
                    oImageDesc.dfYOff = (nClippingHeight - dfYInClippingUnits - dfReqHeightInClippingUnits) / dfUserUnit + oPageContext.sMargins.nBottom;
                    oImageDesc.dfXSize = dfReqWidthInClippingUnits / dfUserUnit;
                    oImageDesc.dfYSize = dfReqHeightInClippingUnits / dfUserUnit;

                    oRasterDesc.asImageDesc.push_back(oImageDesc);
                }
            }

            GDALDestroyScaledProgress(pScaledData);
        }
    }

    oPageContext.asRasterDesc.push_back(oRasterDesc);

    return true;
}

/************************************************************************/
/*                          WriteOGRDataSource()                        */
/************************************************************************/

bool GDALPDFWriter::WriteOGRDataSource(const char* pszOGRDataSource,
                                      const char* pszOGRDisplayField,
                                      const char* pszOGRDisplayLayerNames,
                                      const char* pszOGRLinkField,
                                      int bWriteOGRAttributes)
{
    if (OGRGetDriverCount() == 0)
        OGRRegisterAll();

    OGRDataSourceH hDS = OGROpen(pszOGRDataSource, 0, nullptr);
    if (hDS == nullptr)
        return false;

    int iObj = 0;

    int nLayers = OGR_DS_GetLayerCount(hDS);

    char** papszLayerNames = CSLTokenizeString2(pszOGRDisplayLayerNames,",",0);

    for(int iLayer = 0; iLayer < nLayers; iLayer ++)
    {
        CPLString osLayerName;
        if (CSLCount(papszLayerNames) < nLayers)
            osLayerName = OGR_L_GetName(OGR_DS_GetLayer(hDS, iLayer));
        else
            osLayerName = papszLayerNames[iLayer];

        WriteOGRLayer(hDS, iLayer,
                      pszOGRDisplayField,
                      pszOGRLinkField,
                      osLayerName,
                      bWriteOGRAttributes,
                      iObj);
    }

    OGRReleaseDataSource(hDS);

    CSLDestroy(papszLayerNames);

    return true;
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
    osVectorDesc.nOCGId = WriteOCG(osLayerName);
    if( bWriteOGRAttributes )
        osVectorDesc.nFeatureLayerId = AllocNewObject();

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

        if (!m_nStructTreeRootId.toBool())
            m_nStructTreeRootId = AllocNewObject();

        oDict.Add("P", m_nStructTreeRootId, 0);
        oDict.Add("S", GDALPDFObjectRW::CreateName("Feature"));
        oDict.Add("T", osVectorDesc.osLayerName);

        VSIFPrintfL(m_fp, "%s\n", oDict.Serialize().c_str());

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
                                 const char* pszOGRLinkField,
                                 CPLString osLayerName,
                                 int bWriteOGRAttributes,
                                 int& iObj)
{
    GDALDataset* poClippingDS = oPageContext.poClippingDS;
    double adfGeoTransform[6];
    if (poClippingDS->GetGeoTransform(adfGeoTransform) != CE_None)
        return FALSE;

    GDALPDFLayerDesc osVectorDesc = StartOGRLayer(osLayerName,
                                                  bWriteOGRAttributes);
    OGRLayerH hLyr = OGR_DS_GetLayer(hDS, iLayer);

    const auto poLayerDefn = OGRLayer::FromHandle(hLyr)->GetLayerDefn();
    for( int i = 0; i < poLayerDefn->GetFieldCount(); i++ )
    {
        const auto poFieldDefn = poLayerDefn->GetFieldDefn(i);
        const char* pszName = poFieldDefn->GetNameRef();
        osVectorDesc.aosIncludedFields.push_back(pszName);
    }

    OGRSpatialReferenceH hGDAL_SRS = OGRSpatialReference::ToHandle(
        const_cast<OGRSpatialReference*>(poClippingDS->GetSpatialRef()));
    OGRSpatialReferenceH hOGR_SRS = OGR_L_GetSpatialRef(hLyr);
    OGRCoordinateTransformationH hCT = nullptr;

    if( hGDAL_SRS == nullptr && hOGR_SRS != nullptr )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Vector layer has a SRS set, but Raster layer has no SRS set. Assuming they are the same.");
    }
    else if( hGDAL_SRS != nullptr && hOGR_SRS == nullptr )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Vector layer has no SRS set, but Raster layer has a SRS set. Assuming they are the same.");
    }
    else if( hGDAL_SRS != nullptr && hOGR_SRS != nullptr )
    {
        if (!OSRIsSame(hGDAL_SRS, hOGR_SRS))
        {
            hCT = OCTNewCoordinateTransformation( hOGR_SRS, hGDAL_SRS );
            if( hCT == nullptr )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Cannot compute coordinate transformation from vector SRS to raster SRS");
            }
        }
    }

    if( hCT == nullptr )
    {
        double dfXMin = adfGeoTransform[0];
        double dfYMin = adfGeoTransform[3] + poClippingDS->GetRasterYSize() * adfGeoTransform[5];
        double dfXMax = adfGeoTransform[0] + poClippingDS->GetRasterXSize() * adfGeoTransform[1];
        double dfYMax = adfGeoTransform[3];
        OGR_L_SetSpatialFilterRect(hLyr, dfXMin, dfYMin, dfXMax, dfYMax);
    }

    OGRFeatureH hFeat;

    while( (hFeat = OGR_L_GetNextFeature(hLyr)) != nullptr)
    {
        WriteOGRFeature(osVectorDesc,
                        hFeat,
                        hCT,
                        pszOGRDisplayField,
                        pszOGRLinkField,
                        bWriteOGRAttributes,
                        iObj);

        OGR_F_Destroy(hFeat);
    }

    EndOGRLayer(osVectorDesc);

    if( hCT != nullptr )
        OCTDestroyCoordinateTransformation(hCT);

    return TRUE;
}

/************************************************************************/
/*                             DrawGeometry()                           */
/************************************************************************/

static void DrawGeometry(CPLString& osDS, OGRGeometryH hGeom,
                         const double adfMatrix[4], bool bPaint = true)
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
                osDS += CPLOPrintf("%f %f %c\n", dfX, dfY, (i == 0) ? 'm' : 'l');
            }
            if (bPaint)
                osDS += CPLOPrintf("S\n");
            break;
        }

        case wkbPolygon:
        {
            int nParts = OGR_G_GetGeometryCount(hGeom);
            for(int i=0;i<nParts;i++)
            {
                DrawGeometry(osDS, OGR_G_GetGeometryRef(hGeom, i), adfMatrix, false);
                osDS += CPLOPrintf("h\n");
            }
            if (bPaint)
                osDS += CPLOPrintf("b*\n");
            break;
        }

        case wkbMultiLineString:
        {
            int nParts = OGR_G_GetGeometryCount(hGeom);
            for(int i=0;i<nParts;i++)
            {
                DrawGeometry(osDS, OGR_G_GetGeometryRef(hGeom, i), adfMatrix, false);
            }
            if (bPaint)
                osDS += CPLOPrintf("S\n");
            break;
        }

        case wkbMultiPolygon:
        {
            int nParts = OGR_G_GetGeometryCount(hGeom);
            for(int i=0;i<nParts;i++)
            {
                DrawGeometry(osDS, OGR_G_GetGeometryRef(hGeom, i), adfMatrix, false);
            }
            if (bPaint)
                osDS += CPLOPrintf("b*\n");
            break;
        }

        default:
            break;
    }
}

/************************************************************************/
/*                           CalculateText()                            */
/************************************************************************/

static void CalculateText( const CPLString& osText, CPLString& osFont,
    const double dfSize, const bool bBold, const bool bItalic,
    double& dfWidth, double& dfHeight )
{
    // Character widths of Helvetica, Win-1252 characters 32 to 255
    // Helvetica bold, oblique and bold oblique have their own widths,
    // but for now we will put up with these widths on all Helvetica variants
    constexpr GUInt16 anHelveticaCharWidths[] = {
        569, 569, 727, 1139,1139,1821,1366,391, 682, 682, 797, 1196,569, 682, 569, 569,
        1139,1139,1139,1139,1139,1139,1139,1139,1139,1139,569, 569, 1196,1196,1196,1139,
        2079,1366,1366,1479,1479,1366,1251,1593,1479,569, 1024,1366,1139,1706,1479,1593,
        1366,1593,1479,1366,1251,1479,1366,1933,1366,1366,1251,569, 569, 569, 961, 1139,
        682, 1139,1139,1024,1139,1139,569, 1139,1139,455, 455, 1024,455, 1706,1139,1139,
        1139,1139,682, 1024,569, 1139,1024,1479,1024,1024,1024,684, 532, 684, 1196,1536,
        1139,2048,455, 1139,682, 2048,1139,1139,682, 2048,1366,682, 2048,2048,1251,2048,
        2048,455, 455, 682, 682, 717, 1139,2048,682, 2048,1024,682, 1933,2048,1024,1366,
        569, 682, 1139,1139,1139,1139,532, 1139,682, 1509,758, 1139,1196,682, 1509,1131,
        819, 1124,682, 682, 682, 1180,1100,682, 682, 682, 748, 1139,1708,1708,1708,1251,
        1366,1366,1366,1366,1366,1366,2048,1479,1366,1366,1366,1366,569, 569, 569, 569,
        1479,1479,1593,1593,1593,1593,1593,1196,1593,1479,1479,1479,1479,1366,1366,1251,
        1139,1139,1139,1139,1139,1139,1821,1024,1139,1139,1139,1139,569, 569, 569, 569,
        1139,1139,1139,1139,1139,1139,1139,1124,1251,1139,1139,1139,1139,1024,1139,1024
    };

    // Character widths of Times-Roman, Win-1252 characters 32 to 255
    // Times bold, italic and bold italic have their own widths,
    // but for now we will put up with these widths on all Times variants
    constexpr GUInt16 anTimesCharWidths[] = {
        512, 682, 836, 1024,1024,1706,1593,369, 682, 682, 1024,1155,512, 682, 512, 569,
        1024,1024,1024,1024,1024,1024,1024,1024,1024,1024,569, 569, 1155,1155,1155,909,
        1886,1479,1366,1366,1479,1251,1139,1479,1479,682, 797, 1479,1251,1821,1479,1479,
        1139,1479,1366,1139,1251,1479,1479,1933,1479,1479,1251,682, 569, 682, 961, 1024,
        682, 909, 1024,909, 1024,909, 682, 1024,1024,569, 569, 1024,569, 1593,1024,1024,
        1024,1024,682, 797, 569, 1024,1024,1479,1024,1024,909, 983, 410, 983, 1108,0,
        1024,2048,682, 1024,909, 2048,1024,1024,682 ,2048,1139,682 ,1821,2048,1251,2048,
        2048,682, 682, 909, 909, 717 ,1024,2048,682 ,2007,797, 682 ,1479,2048,909, 1479,
        512, 682, 1024,1024,1024,1024,410, 1024,682, 1556,565, 1024,1155,682, 1556,1024,
        819, 1124,614, 614, 682, 1180,928, 682, 682, 614, 635, 1024,1536,1536,1536,909,
        1479,1479,1479,1479,1479,1479,1821,1366,1251,1251,1251,1251,682, 682, 682, 682,
        1479,1479,1479,1479,1479,1479,1479,1155,1479,1479,1479,1479,1479,1479,1139,1024,
        909, 909, 909, 909, 909, 909, 1366,909, 909, 909, 909, 909, 569, 569, 569, 569,
        1024,1024,1024,1024,1024,1024,1024,1124,1024,1024,1024,1024,1024,1024,1024,1024
    };

    const GUInt16* panCharacterWidths = nullptr;

    if( STARTS_WITH_CI( osFont, "times" ) ||
        osFont.find( "Serif", 0 ) != std::string::npos )
    {
        if( bBold && bItalic )
            osFont = "Times-BoldItalic";
        else if( bBold )
            osFont = "Times-Bold";
        else if( bItalic )
            osFont = "Times-Italic";
        else
            osFont = "Times-Roman";

        panCharacterWidths = anTimesCharWidths;
        dfHeight = dfSize * 1356.0 / 2048;
    }
    else if( STARTS_WITH_CI( osFont, "courier" ) ||
        osFont.find( "Mono", 0 ) != std::string::npos )
    {
        if( bBold && bItalic )
            osFont = "Courier-BoldOblique";
        else if( bBold )
            osFont = "Courier-Bold";
        else if( bItalic )
            osFont = "Courier-Oblique";
        else
            osFont = "Courier";

        dfHeight = dfSize * 1170.0 / 2048;
    }
    else
    {
        if( bBold && bItalic )
            osFont = "Helvetica-BoldOblique";
        else if( bBold )
            osFont = "Helvetica-Bold";
        else if( bItalic )
            osFont = "Helvetica-Oblique";
        else
            osFont = "Helvetica";

        panCharacterWidths = anHelveticaCharWidths;
        dfHeight = dfSize * 1467.0 / 2048;
    }

    dfWidth = 0.0;
    for( const char& ch : osText )
    {
        const int nCh = static_cast<int>( ch );
        if( nCh < 32 )
            continue;

        dfWidth += ( panCharacterWidths ?
            panCharacterWidths[nCh - 32] :
            1229 ); // Courier's fixed character width
    }
    dfWidth *= dfSize / 2048;
}

/************************************************************************/
/*                          GetObjectStyle()                            */
/************************************************************************/

void GDALPDFBaseWriter::GetObjectStyle(const char* pszStyleString,
                                       OGRFeatureH hFeat,
                                       const double adfMatrix[4],
                                       std::map<CPLString,GDALPDFImageDesc> oMapSymbolFilenameToDesc,
                                       ObjectStyle& os)
{
    OGRStyleMgrH hSM = OGR_SM_Create(nullptr);
    if( pszStyleString )
        OGR_SM_InitStyleString(hSM, pszStyleString);
    else
        OGR_SM_InitFromFeature(hSM, hFeat);
    int nCount = OGR_SM_GetPartCount(hSM, nullptr);
    for(int iPart = 0; iPart < nCount; iPart++)
    {
        OGRStyleToolH hTool = OGR_SM_GetPart(hSM, iPart, nullptr);
        if (hTool)
        {
            // Figure out how to involve adfMatrix[3] here and below
            OGR_ST_SetUnit( hTool, OGRSTUMM, 1000.0 / adfMatrix[1] );
            if (OGR_ST_GetType(hTool) == OGRSTCPen)
            {
                os.bHasPenBrushOrSymbol = true;

                int bIsNull = TRUE;
                const char* pszColor = OGR_ST_GetParamStr(hTool, OGRSTPenColor, &bIsNull);
                if (pszColor && !bIsNull)
                {
                    unsigned int nRed = 0;
                    unsigned int nGreen = 0;
                    unsigned int nBlue = 0;
                    unsigned int nAlpha = 255;
                    int nVals = sscanf(pszColor,"#%2x%2x%2x%2x",&nRed,&nGreen,&nBlue,&nAlpha);
                    if (nVals >= 3)
                    {
                        os.nPenR = nRed;
                        os.nPenG = nGreen;
                        os.nPenB = nBlue;
                        if (nVals == 4)
                            os.nPenA = nAlpha;
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
                            double dfElement = CPLAtof(papszTokens[i]);
                            dfElement *= adfMatrix[1]; // should involve adfMatrix[3] too
                            os.osDashArray += CPLSPrintf("%f ", dfElement);
                        }
                    }
                    CSLDestroy(papszTokens);
                }

                //OGRSTUnitId eUnit = OGR_ST_GetUnit(hTool);
                double dfWidth = OGR_ST_GetParamDbl(hTool, OGRSTPenWidth, &bIsNull);
                if (!bIsNull)
                    os.dfPenWidth = dfWidth;
            }
            else if (OGR_ST_GetType(hTool) == OGRSTCBrush)
            {
                os.bHasPenBrushOrSymbol = true;

                int bIsNull;
                const char* pszColor = OGR_ST_GetParamStr(hTool, OGRSTBrushFColor, &bIsNull);
                if (pszColor)
                {
                    unsigned int nRed = 0;
                    unsigned int nGreen = 0;
                    unsigned int nBlue = 0;
                    unsigned int nAlpha = 255;
                    int nVals = sscanf(pszColor,"#%2x%2x%2x%2x",&nRed,&nGreen,&nBlue,&nAlpha);
                    if (nVals >= 3)
                    {
                        os.nBrushR = nRed;
                        os.nBrushG = nGreen;
                        os.nBrushB = nBlue;
                        if (nVals == 4)
                            os.nBrushA = nAlpha;
                    }
                }
            }
            else if (OGR_ST_GetType(hTool) == OGRSTCLabel)
            {
                int bIsNull;
                const char* pszStr = OGR_ST_GetParamStr(hTool, OGRSTLabelTextString, &bIsNull);
                if (pszStr)
                {
                    os.osLabelText = pszStr;

                    /* If the text is of the form {stuff}, then it means we want to fetch */
                    /* the value of the field "stuff" in the feature */
                    if( !os.osLabelText.empty() && os.osLabelText[0] == '{' &&
                        os.osLabelText.back() == '}' )
                    {
                        os.osLabelText = pszStr + 1;
                        os.osLabelText.resize(os.osLabelText.size() - 1);

                        int nIdxField = OGR_F_GetFieldIndex(hFeat, os.osLabelText);
                        if( nIdxField >= 0 )
                            os.osLabelText = OGR_F_GetFieldAsString(hFeat, nIdxField);
                        else
                            os.osLabelText = "";
                    }
                }

                const char* pszColor = OGR_ST_GetParamStr(hTool, OGRSTLabelFColor, &bIsNull);
                if (pszColor && !bIsNull)
                {
                    unsigned int nRed = 0;
                    unsigned int nGreen = 0;
                    unsigned int nBlue = 0;
                    unsigned int nAlpha = 255;
                    int nVals = sscanf(pszColor,"#%2x%2x%2x%2x",&nRed,&nGreen,&nBlue,&nAlpha);
                    if (nVals >= 3)
                    {
                        os.nTextR = nRed;
                        os.nTextG = nGreen;
                        os.nTextB = nBlue;
                        if (nVals == 4)
                            os.nTextA = nAlpha;
                    }
                }

                pszStr = OGR_ST_GetParamStr(hTool, OGRSTLabelFontName, &bIsNull);
                if (pszStr && !bIsNull)
                    os.osTextFont = pszStr;

                double dfVal = OGR_ST_GetParamDbl(hTool, OGRSTLabelSize, &bIsNull);
                if (!bIsNull)
                    os.dfTextSize = dfVal;

                dfVal = OGR_ST_GetParamDbl(hTool, OGRSTLabelAngle, &bIsNull);
                if (!bIsNull)
                    os.dfTextAngle = dfVal * M_PI / 180.0;

                dfVal = OGR_ST_GetParamDbl(hTool, OGRSTLabelStretch, &bIsNull);
                if (!bIsNull)
                    os.dfTextStretch = dfVal / 100.0;

                dfVal = OGR_ST_GetParamDbl(hTool, OGRSTLabelDx, &bIsNull);
                if (!bIsNull)
                    os.dfTextDx = dfVal;

                dfVal = OGR_ST_GetParamDbl(hTool, OGRSTLabelDy, &bIsNull);
                if (!bIsNull)
                    os.dfTextDy = dfVal;

                int nVal = OGR_ST_GetParamNum(hTool, OGRSTLabelAnchor, &bIsNull);
                if (!bIsNull)
                    os.nTextAnchor = nVal;

                nVal = OGR_ST_GetParamNum(hTool, OGRSTLabelBold, &bIsNull);
                if (!bIsNull)
                    os.bTextBold = (nVal != 0);

                nVal = OGR_ST_GetParamNum(hTool, OGRSTLabelItalic, &bIsNull);
                if (!bIsNull)
                    os.bTextItalic = (nVal != 0);
            }
            else if (OGR_ST_GetType(hTool) == OGRSTCSymbol)
            {
                os.bHasPenBrushOrSymbol = true;

                int bIsNull;
                const char* pszSymbolId = OGR_ST_GetParamStr(hTool, OGRSTSymbolId, &bIsNull);
                if (pszSymbolId && !bIsNull)
                {
                    os.osSymbolId = pszSymbolId;

                    if (strstr(pszSymbolId, "ogr-sym-") == nullptr)
                    {
                        if (oMapSymbolFilenameToDesc.find(os.osSymbolId) == oMapSymbolFilenameToDesc.end())
                        {
                            CPLPushErrorHandler(CPLQuietErrorHandler);
                            GDALDatasetH hImageDS = GDALOpen(os.osSymbolId, GA_ReadOnly);
                            CPLPopErrorHandler();
                            if (hImageDS != nullptr)
                            {
                                os.nImageWidth = GDALGetRasterXSize(hImageDS);
                                os.nImageHeight = GDALGetRasterYSize(hImageDS);

                                os.nImageSymbolId = WriteBlock(GDALDataset::FromHandle(hImageDS),
                                                        0, 0,
                                                        os.nImageWidth,
                                                        os.nImageHeight,
                                                        GDALPDFObjectNum(),
                                                        COMPRESS_DEFAULT,
                                                        0,
                                                        -1,
                                                        nullptr,
                                                        nullptr,
                                                        nullptr);
                                GDALClose(hImageDS);
                            }

                            GDALPDFImageDesc oDesc;
                            oDesc.nImageId = os.nImageSymbolId;
                            oDesc.dfXOff = 0;
                            oDesc.dfYOff = 0;
                            oDesc.dfXSize = os.nImageWidth;
                            oDesc.dfYSize = os.nImageHeight;
                            oMapSymbolFilenameToDesc[os.osSymbolId] = oDesc;
                        }
                        else
                        {
                            GDALPDFImageDesc& oDesc = oMapSymbolFilenameToDesc[os.osSymbolId];
                            os.nImageSymbolId = oDesc.nImageId;
                            os.nImageWidth = (int)oDesc.dfXSize;
                            os.nImageHeight = (int)oDesc.dfYSize;
                        }
                    }
                }

                double dfVal = OGR_ST_GetParamDbl(hTool, OGRSTSymbolSize, &bIsNull);
                if (!bIsNull)
                {
                    os.dfSymbolSize = dfVal;
                }

                const char* pszColor = OGR_ST_GetParamStr(hTool, OGRSTSymbolColor, &bIsNull);
                if (pszColor && !bIsNull)
                {
                    unsigned int nRed = 0;
                    unsigned int nGreen = 0;
                    unsigned int nBlue = 0;
                    unsigned int nAlpha = 255;
                    int nVals = sscanf(pszColor,"#%2x%2x%2x%2x",&nRed,&nGreen,&nBlue,&nAlpha);
                    if (nVals >= 3)
                    {
                        os.bSymbolColorDefined = TRUE;
                        os.nSymbolR = nRed;
                        os.nSymbolG = nGreen;
                        os.nSymbolB = nBlue;
                        if (nVals == 4)
                            os.nSymbolA = nAlpha;
                    }
                }
            }

            OGR_ST_Destroy(hTool);
        }
    }
    OGR_SM_Destroy(hSM);

    OGRGeometryH hGeom = OGR_F_GetGeometryRef(hFeat);
    if (wkbFlatten(OGR_G_GetGeometryType(hGeom)) == wkbPoint &&
        os.bSymbolColorDefined)
    {
        os.nPenR = os.nSymbolR;
        os.nPenG = os.nSymbolG;
        os.nPenB = os.nSymbolB;
        os.nPenA = os.nSymbolA;
        os.nBrushR = os.nSymbolR;
        os.nBrushG = os.nSymbolG;
        os.nBrushB = os.nSymbolB;
        os.nBrushA = os.nSymbolA;
    }
}

/************************************************************************/
/*                           ComputeIntBBox()                           */
/************************************************************************/

void GDALPDFBaseWriter::ComputeIntBBox(OGRGeometryH hGeom,
                           const OGREnvelope& sEnvelope,
                           const double adfMatrix[4],
                           const GDALPDFWriter::ObjectStyle& os,
                           double dfRadius,
                           int& bboxXMin,
                           int& bboxYMin,
                           int& bboxXMax,
                           int& bboxYMax)
{
    if (wkbFlatten(OGR_G_GetGeometryType(hGeom)) == wkbPoint && os.nImageSymbolId.toBool())
    {
        const double dfSemiWidth = (os.nImageWidth >= os.nImageHeight) ? dfRadius : dfRadius * os.nImageWidth / os.nImageHeight;
        const double dfSemiHeight = (os.nImageWidth >= os.nImageHeight) ? dfRadius * os.nImageHeight / os.nImageWidth : dfRadius;
        bboxXMin = (int)floor(sEnvelope.MinX * adfMatrix[1] + adfMatrix[0] - dfSemiWidth);
        bboxYMin = (int)floor(sEnvelope.MinY * adfMatrix[3] + adfMatrix[2] - dfSemiHeight);
        bboxXMax = (int)ceil(sEnvelope.MaxX * adfMatrix[1] + adfMatrix[0] + dfSemiWidth);
        bboxYMax = (int)ceil(sEnvelope.MaxY * adfMatrix[3] + adfMatrix[2] + dfSemiHeight);
    }
    else
    {
        double dfMargin = os.dfPenWidth;
        if( wkbFlatten(OGR_G_GetGeometryType(hGeom)) == wkbPoint )
        {
            if (os.osSymbolId == "ogr-sym-6" ||
                os.osSymbolId == "ogr-sym-7")
            {
                const double dfSqrt3 = 1.73205080757;
                dfMargin += dfRadius * 2 * dfSqrt3 / 3;
            }
            else
                dfMargin += dfRadius;
        }
        bboxXMin = (int)floor(sEnvelope.MinX * adfMatrix[1] + adfMatrix[0] - dfMargin);
        bboxYMin = (int)floor(sEnvelope.MinY * adfMatrix[3] + adfMatrix[2] - dfMargin);
        bboxXMax = (int)ceil(sEnvelope.MaxX * adfMatrix[1] + adfMatrix[0] + dfMargin);
        bboxYMax = (int)ceil(sEnvelope.MaxY * adfMatrix[3] + adfMatrix[2] + dfMargin);
    }
}

/************************************************************************/
/*                              WriteLink()                             */
/************************************************************************/

GDALPDFObjectNum GDALPDFBaseWriter::WriteLink(OGRFeatureH hFeat,
                              const char* pszOGRLinkField,
                              const double adfMatrix[4],
                              int bboxXMin,
                              int bboxYMin,
                              int bboxXMax,
                              int bboxYMax)
{
    GDALPDFObjectNum nAnnotId;
    int iField = -1;
    const char* pszLinkVal = nullptr;
    if (pszOGRLinkField != nullptr &&
        (iField = OGR_FD_GetFieldIndex(OGR_F_GetDefnRef(hFeat), pszOGRLinkField)) >= 0 &&
        OGR_F_IsFieldSetAndNotNull(hFeat, iField) &&
        strcmp((pszLinkVal = OGR_F_GetFieldAsString(hFeat, iField)), "") != 0)
    {
        nAnnotId = AllocNewObject();
        StartObj(nAnnotId);
        {
            GDALPDFDictionaryRW oDict;
            oDict.Add("Type", GDALPDFObjectRW::CreateName("Annot"));
            oDict.Add("Subtype", GDALPDFObjectRW::CreateName("Link"));
            oDict.Add("Rect", &(new GDALPDFArrayRW())->Add(bboxXMin).Add(bboxYMin).Add(bboxXMax).Add(bboxYMax));
            oDict.Add("A", &(new GDALPDFDictionaryRW())->
                Add("S", GDALPDFObjectRW::CreateName("URI")).
                Add("URI", pszLinkVal));
            oDict.Add("BS", &(new GDALPDFDictionaryRW())->
                Add("Type", GDALPDFObjectRW::CreateName("Border")).
                Add("S", GDALPDFObjectRW::CreateName("S")).
                Add("W", 0));
            oDict.Add("Border", &(new GDALPDFArrayRW())->Add(0).Add(0).Add(0));
            oDict.Add("H", GDALPDFObjectRW::CreateName("I"));

            OGRGeometryH hGeom = OGR_F_GetGeometryRef(hFeat);
            if( wkbFlatten(OGR_G_GetGeometryType(hGeom)) == wkbPolygon &&
                OGR_G_GetGeometryCount(hGeom) == 1 )
            {
                OGRGeometryH hSubGeom = OGR_G_GetGeometryRef(hGeom, 0);
                int nPoints = OGR_G_GetPointCount(hSubGeom);
                if( nPoints == 4 || nPoints == 5 )
                {
                    std::vector<double> adfX, adfY;
                    for(int i=0;i<nPoints;i++)
                    {
                        double dfX = OGR_G_GetX(hSubGeom, i) * adfMatrix[1] + adfMatrix[0];
                        double dfY = OGR_G_GetY(hSubGeom, i) * adfMatrix[3] + adfMatrix[2];
                        adfX.push_back(dfX);
                        adfY.push_back(dfY);
                    }
                    if( nPoints == 4 )
                    {
                        oDict.Add("QuadPoints", &(new GDALPDFArrayRW())->
                            Add(adfX[0]).Add(adfY[0]).
                            Add(adfX[1]).Add(adfY[1]).
                            Add(adfX[2]).Add(adfY[2]).
                            Add(adfX[0]).Add(adfY[0]));
                    }
                    else if( nPoints == 5 )
                    {
                        oDict.Add("QuadPoints", &(new GDALPDFArrayRW())->
                            Add(adfX[0]).Add(adfY[0]).
                            Add(adfX[1]).Add(adfY[1]).
                            Add(adfX[2]).Add(adfY[2]).
                            Add(adfX[3]).Add(adfY[3]));
                    }
                }
            }

            VSIFPrintfL(m_fp, "%s\n", oDict.Serialize().c_str());
        }
        EndObj();
    }
    return nAnnotId;
}

/************************************************************************/
/*                        GenerateDrawingStream()                       */
/************************************************************************/

CPLString GDALPDFBaseWriter::GenerateDrawingStream(OGRGeometryH hGeom,
                                                   const double adfMatrix[4],
                                                   ObjectStyle& os,
                                                   double dfRadius)
{
    CPLString osDS;

    if (!os.nImageSymbolId.toBool())
    {
        osDS += CPLOPrintf("%f w\n"
                        "0 J\n"
                        "0 j\n"
                        "10 M\n"
                        "[%s]0 d\n",
                        os.dfPenWidth,
                        os.osDashArray.c_str());

        osDS += CPLOPrintf("%f %f %f RG\n", os.nPenR / 255.0, os.nPenG / 255.0, os.nPenB / 255.0);
        osDS += CPLOPrintf("%f %f %f rg\n", os.nBrushR / 255.0, os.nBrushG / 255.0, os.nBrushB / 255.0);
    }

    if ((os.bHasPenBrushOrSymbol || os.osLabelText.empty()) &&
        wkbFlatten(OGR_G_GetGeometryType(hGeom)) == wkbPoint)
    {
        double dfX = OGR_G_GetX(hGeom, 0) * adfMatrix[1] + adfMatrix[0];
        double dfY = OGR_G_GetY(hGeom, 0) * adfMatrix[3] + adfMatrix[2];

        if (os.nImageSymbolId.toBool())
        {
            const double dfSemiWidth = (os.nImageWidth >= os.nImageHeight) ? dfRadius : dfRadius * os.nImageWidth / os.nImageHeight;
            const double dfSemiHeight = (os.nImageWidth >= os.nImageHeight) ? dfRadius * os.nImageHeight / os.nImageWidth : dfRadius;
            osDS += CPLOPrintf("%f 0 0 %f %f %f cm\n",
                        2 * dfSemiWidth,
                        2 * dfSemiHeight,
                        dfX - dfSemiWidth,
                        dfY - dfSemiHeight);
            osDS += CPLOPrintf("/SymImage%d Do\n", os.nImageSymbolId.toInt());
        }
        else if (os.osSymbolId == "")
            os.osSymbolId = "ogr-sym-3"; /* symbol by default */
        else if ( !(os.osSymbolId == "ogr-sym-0" ||
                    os.osSymbolId == "ogr-sym-1" ||
                    os.osSymbolId == "ogr-sym-2" ||
                    os.osSymbolId == "ogr-sym-3" ||
                    os.osSymbolId == "ogr-sym-4" ||
                    os.osSymbolId == "ogr-sym-5" ||
                    os.osSymbolId == "ogr-sym-6" ||
                    os.osSymbolId == "ogr-sym-7" ||
                    os.osSymbolId == "ogr-sym-8" ||
                    os.osSymbolId == "ogr-sym-9") )
        {
            CPLDebug("PDF", "Unhandled symbol id : %s. Using ogr-sym-3 instead", os.osSymbolId.c_str());
            os.osSymbolId = "ogr-sym-3";
        }

        if (os.osSymbolId == "ogr-sym-0") /* cross (+)  */
        {
            osDS += CPLOPrintf("%f %f m\n", dfX - dfRadius, dfY);
            osDS += CPLOPrintf("%f %f l\n", dfX + dfRadius, dfY);
            osDS += CPLOPrintf("%f %f m\n", dfX, dfY - dfRadius);
            osDS += CPLOPrintf("%f %f l\n", dfX, dfY + dfRadius);
            osDS += CPLOPrintf("S\n");
        }
        else if (os.osSymbolId == "ogr-sym-1") /* diagcross (X) */
        {
            osDS += CPLOPrintf("%f %f m\n", dfX - dfRadius, dfY - dfRadius);
            osDS += CPLOPrintf("%f %f l\n", dfX + dfRadius, dfY + dfRadius);
            osDS += CPLOPrintf("%f %f m\n", dfX - dfRadius, dfY + dfRadius);
            osDS += CPLOPrintf("%f %f l\n", dfX + dfRadius, dfY - dfRadius);
            osDS += CPLOPrintf("S\n");
        }
        else if (os.osSymbolId == "ogr-sym-2" ||
                    os.osSymbolId == "ogr-sym-3") /* circle */
        {
            /* See http://www.whizkidtech.redprince.net/bezier/circle/kappa/ */
            const double dfKappa = 0.5522847498;

            osDS += CPLOPrintf("%f %f m\n", dfX - dfRadius, dfY);
            osDS += CPLOPrintf("%f %f %f %f %f %f c\n",
                        dfX - dfRadius, dfY - dfRadius * dfKappa,
                        dfX - dfRadius * dfKappa, dfY - dfRadius,
                        dfX, dfY - dfRadius);
            osDS += CPLOPrintf("%f %f %f %f %f %f c\n",
                        dfX + dfRadius * dfKappa, dfY - dfRadius,
                        dfX + dfRadius, dfY - dfRadius * dfKappa,
                        dfX + dfRadius, dfY);
            osDS += CPLOPrintf("%f %f %f %f %f %f c\n",
                        dfX + dfRadius, dfY + dfRadius * dfKappa,
                        dfX + dfRadius * dfKappa, dfY + dfRadius,
                        dfX, dfY + dfRadius);
            osDS += CPLOPrintf("%f %f %f %f %f %f c\n",
                        dfX - dfRadius * dfKappa, dfY + dfRadius,
                        dfX - dfRadius, dfY + dfRadius * dfKappa,
                        dfX - dfRadius, dfY);
            if (os.osSymbolId == "ogr-sym-2")
                osDS += CPLOPrintf("s\n"); /* not filled */
            else
                osDS += CPLOPrintf("b*\n"); /* filled */
        }
        else if (os.osSymbolId == "ogr-sym-4" ||
                    os.osSymbolId == "ogr-sym-5") /* square */
        {
            osDS += CPLOPrintf("%f %f m\n", dfX - dfRadius, dfY + dfRadius);
            osDS += CPLOPrintf("%f %f l\n", dfX + dfRadius, dfY + dfRadius);
            osDS += CPLOPrintf("%f %f l\n", dfX + dfRadius, dfY - dfRadius);
            osDS += CPLOPrintf("%f %f l\n", dfX - dfRadius, dfY - dfRadius);
            if (os.osSymbolId == "ogr-sym-4")
                osDS += CPLOPrintf("s\n"); /* not filled */
            else
                osDS += CPLOPrintf("b*\n"); /* filled */
        }
        else if (os.osSymbolId == "ogr-sym-6" ||
                    os.osSymbolId == "ogr-sym-7") /* triangle */
        {
            const double dfSqrt3 = 1.73205080757;
            osDS += CPLOPrintf("%f %f m\n", dfX - dfRadius, dfY - dfRadius * dfSqrt3 / 3);
            osDS += CPLOPrintf("%f %f l\n", dfX, dfY + 2 * dfRadius * dfSqrt3 / 3);
            osDS += CPLOPrintf("%f %f l\n", dfX + dfRadius, dfY - dfRadius * dfSqrt3 / 3);
            if (os.osSymbolId == "ogr-sym-6")
                osDS += CPLOPrintf("s\n"); /* not filled */
            else
                osDS += CPLOPrintf("b*\n"); /* filled */
        }
        else if (os.osSymbolId == "ogr-sym-8" ||
                    os.osSymbolId == "ogr-sym-9") /* star */
        {
            const double dfSin18divSin126 = 0.38196601125;
            osDS += CPLOPrintf("%f %f m\n", dfX, dfY + dfRadius);
            for(int i=1; i<10;i++)
            {
                double dfFactor = ((i % 2) == 1) ? dfSin18divSin126 : 1.0;
                osDS += CPLOPrintf("%f %f l\n",
                            dfX + cos(M_PI / 2 - i * M_PI * 36 / 180) * dfRadius * dfFactor,
                            dfY + sin(M_PI / 2 - i * M_PI * 36 / 180) * dfRadius * dfFactor);
            }
            if (os.osSymbolId == "ogr-sym-8")
                osDS += CPLOPrintf("s\n"); /* not filled */
            else
                osDS += CPLOPrintf("b*\n"); /* filled */
        }
    }
    else
    {
        DrawGeometry(osDS, hGeom, adfMatrix);
    }

    return osDS;
}

/************************************************************************/
/*                          WriteAttributes()                           */
/************************************************************************/

GDALPDFObjectNum GDALPDFBaseWriter::WriteAttributes(
        OGRFeatureH hFeat,
        const std::vector<CPLString>& aosIncludedFields,
        const char* pszOGRDisplayField,
        int nMCID,
        const GDALPDFObjectNum& oParent,
        const GDALPDFObjectNum& oPage,
        CPLString& osOutFeatureName)
{

    int iField = -1;
    if (pszOGRDisplayField )
        iField = OGR_FD_GetFieldIndex(OGR_F_GetDefnRef(hFeat), pszOGRDisplayField);
    if( iField >= 0 )
        osOutFeatureName = OGR_F_GetFieldAsString(hFeat, iField);
    else
        osOutFeatureName = CPLSPrintf("feature" CPL_FRMT_GIB, OGR_F_GetFID(hFeat));

    auto nFeatureUserProperties = AllocNewObject();
    StartObj(nFeatureUserProperties);

    GDALPDFDictionaryRW oDict;

    GDALPDFDictionaryRW* poDictA = new GDALPDFDictionaryRW();
    oDict.Add("A", poDictA);
    poDictA->Add("O", GDALPDFObjectRW::CreateName("UserProperties"));

    GDALPDFArrayRW* poArray = new GDALPDFArrayRW();
    for( const auto& fieldName: aosIncludedFields )
    {
        int i = OGR_F_GetFieldIndex(hFeat, fieldName);
        if (i >= 0 && OGR_F_IsFieldSetAndNotNull(hFeat, i))
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

    oDict.Add("K", nMCID);
    oDict.Add("P", oParent, 0);
    oDict.Add("Pg", oPage, 0);
    oDict.Add("S", GDALPDFObjectRW::CreateName("feature"));
    oDict.Add("T", osOutFeatureName);

    VSIFPrintfL(m_fp, "%s\n", oDict.Serialize().c_str());

    EndObj();

    return nFeatureUserProperties;
}

/************************************************************************/
/*                            WriteLabel()                              */
/************************************************************************/

GDALPDFObjectNum GDALPDFBaseWriter::WriteLabel(OGRGeometryH hGeom,
                                           const double adfMatrix[4],
                                           ObjectStyle& os,
                                           PDFCompressMethod eStreamCompressMethod,
                                           double bboxXMin,
                                           double bboxYMin,
                                           double bboxXMax,
                                           double bboxYMax)
{
    /* -------------------------------------------------------------- */
    /*  Work out the text metrics for alignment purposes              */
    /* -------------------------------------------------------------- */
    double dfWidth, dfHeight;
    CalculateText(os.osLabelText, os.osTextFont, os.dfTextSize,
        os.bTextBold, os.bTextItalic, dfWidth, dfHeight);
    dfWidth *= os.dfTextStretch;

    if (os.nTextAnchor % 3 == 2) // horizontal center
    {
        os.dfTextDx -= (dfWidth / 2) * cos(os.dfTextAngle);
        os.dfTextDy -= (dfWidth / 2) * sin(os.dfTextAngle);
    }
    else if (os.nTextAnchor % 3 == 0) // right
    {
        os.dfTextDx -= dfWidth * cos(os.dfTextAngle);
        os.dfTextDy -= dfWidth * sin(os.dfTextAngle);
    }

    if (os.nTextAnchor >= 4 && os.nTextAnchor <= 6) // vertical center
    {
        os.dfTextDx += (dfHeight / 2) * sin(os.dfTextAngle);
        os.dfTextDy -= (dfHeight / 2) * cos(os.dfTextAngle);
    }
    else if (os.nTextAnchor >= 7 && os.nTextAnchor <= 9) // top
    {
        os.dfTextDx += dfHeight * sin(os.dfTextAngle);
        os.dfTextDy -= dfHeight * cos(os.dfTextAngle);
    }
    // modes 10,11,12 (baseline) unsupported for the time being

    /* -------------------------------------------------------------- */
    /*  Write object dictionary                                       */
    /* -------------------------------------------------------------- */
    auto nObjectId = AllocNewObject();
    GDALPDFDictionaryRW oDict;

    oDict.Add("Type", GDALPDFObjectRW::CreateName("XObject"))
        .Add("BBox", &((new GDALPDFArrayRW())
                        ->Add(bboxXMin).Add(bboxYMin)).Add(bboxXMax).Add(bboxYMax))
        .Add("Subtype", GDALPDFObjectRW::CreateName("Form"));

    GDALPDFDictionaryRW* poResources = new GDALPDFDictionaryRW();

    if (os.nTextA != 255)
    {
        GDALPDFDictionaryRW* poGS1 = new GDALPDFDictionaryRW();
        poGS1->Add("Type", GDALPDFObjectRW::CreateName("ExtGState"));
        poGS1->Add("ca", (os.nTextA == 127 || os.nTextA == 128) ? 0.5 : os.nTextA / 255.0);

        GDALPDFDictionaryRW* poExtGState = new GDALPDFDictionaryRW();
        poExtGState->Add("GS1", poGS1);

        poResources->Add("ExtGState", poExtGState);
    }

    GDALPDFDictionaryRW* poDictF1 = new GDALPDFDictionaryRW();
    poDictF1->Add("Type", GDALPDFObjectRW::CreateName("Font"));
    poDictF1->Add("BaseFont", GDALPDFObjectRW::CreateName(os.osTextFont));
    poDictF1->Add("Encoding", GDALPDFObjectRW::CreateName("WinAnsiEncoding"));
    poDictF1->Add("Subtype", GDALPDFObjectRW::CreateName("Type1"));

    GDALPDFDictionaryRW* poDictFont = new GDALPDFDictionaryRW();
    poDictFont->Add("F1", poDictF1);
    poResources->Add("Font", poDictFont);

    oDict.Add("Resources", poResources);

    StartObjWithStream(nObjectId, oDict,
                        eStreamCompressMethod != COMPRESS_NONE);

    /* -------------------------------------------------------------- */
    /*  Write object stream                                           */
    /* -------------------------------------------------------------- */

    double dfX = OGR_G_GetX(hGeom, 0) * adfMatrix[1] + adfMatrix[0] + os.dfTextDx;
    double dfY = OGR_G_GetY(hGeom, 0) * adfMatrix[3] + adfMatrix[2] + os.dfTextDy;

    VSIFPrintfL(m_fp, "q\n");
    VSIFPrintfL(m_fp, "BT\n");
    if (os.nTextA != 255)
    {
        VSIFPrintfL(m_fp, "/GS1 gs\n");
    }

    VSIFPrintfL(m_fp, "%f %f %f %f %f %f Tm\n",
                cos(os.dfTextAngle) * adfMatrix[1] * os.dfTextStretch,
                sin(os.dfTextAngle) * adfMatrix[3] * os.dfTextStretch,
                -sin(os.dfTextAngle) * adfMatrix[1],
                cos(os.dfTextAngle) * adfMatrix[3],
                dfX, dfY);

    VSIFPrintfL(m_fp, "%f %f %f rg\n", os.nTextR / 255.0, os.nTextG / 255.0, os.nTextB / 255.0);
    // The factor of adfMatrix[1] is introduced in the call to SetUnit near the top
    // of this function. Because we are handling the 2D stretch correctly in Tm above,
    // we don't need that factor here
    VSIFPrintfL(m_fp, "/F1 %f Tf\n", os.dfTextSize / adfMatrix[1]);
    VSIFPrintfL(m_fp, "(");
    for(size_t i=0;i<os.osLabelText.size();i++)
    {
        if (os.osLabelText[i] == '(' || os.osLabelText[i] == ')' ||
            os.osLabelText[i] == '\\')
        {
            VSIFPrintfL(m_fp, "\\%c", os.osLabelText[i]);
        }
        else
        {
            VSIFPrintfL(m_fp, "%c", os.osLabelText[i]);
        }
    }
    VSIFPrintfL(m_fp, ") Tj\n");
    VSIFPrintfL(m_fp, "ET\n");
    VSIFPrintfL(m_fp, "Q");

    EndObjWithStream();

    return nObjectId;
}

/************************************************************************/
/*                          WriteOGRFeature()                           */
/************************************************************************/

int GDALPDFWriter::WriteOGRFeature(GDALPDFLayerDesc& osVectorDesc,
                                   OGRFeatureH hFeat,
                                   OGRCoordinateTransformationH hCT,
                                   const char* pszOGRDisplayField,
                                   const char* pszOGRLinkField,
                                   int bWriteOGRAttributes,
                                   int& iObj)
{
    GDALDataset* const  poClippingDS = oPageContext.poClippingDS;
    const int  nHeight = poClippingDS->GetRasterYSize();
    const double dfUserUnit = oPageContext.dfDPI * USER_UNIT_IN_INCH;
    double adfGeoTransform[6];
    poClippingDS->GetGeoTransform(adfGeoTransform);

    double adfMatrix[4];
    adfMatrix[0] = - adfGeoTransform[0] / (adfGeoTransform[1] * dfUserUnit) + oPageContext.sMargins.nLeft;
    adfMatrix[1] = 1.0 / (adfGeoTransform[1] * dfUserUnit);
    adfMatrix[2] = - (adfGeoTransform[3] + adfGeoTransform[5] * nHeight) / (-adfGeoTransform[5] * dfUserUnit) + oPageContext.sMargins.nBottom;
    adfMatrix[3] = 1.0 / (-adfGeoTransform[5] * dfUserUnit);

    OGRGeometryH hGeom = OGR_F_GetGeometryRef(hFeat);
    if (hGeom == nullptr)
    {
        return TRUE;
    }

    OGREnvelope sEnvelope;

    if( hCT != nullptr )
    {
        /* Reproject */
        if( OGR_G_Transform(hGeom, hCT) != OGRERR_NONE )
        {
            return TRUE;
        }

        OGREnvelope sRasterEnvelope;
        sRasterEnvelope.MinX = adfGeoTransform[0];
        sRasterEnvelope.MinY = adfGeoTransform[3]
            + poClippingDS->GetRasterYSize() * adfGeoTransform[5];
        sRasterEnvelope.MaxX = adfGeoTransform[0]
            + poClippingDS->GetRasterXSize() * adfGeoTransform[1];
        sRasterEnvelope.MaxY = adfGeoTransform[3];

        // Check that the reprojected geometry intersects the raster envelope.
        OGR_G_GetEnvelope(hGeom, &sEnvelope);
        if( !(sRasterEnvelope.Intersects(sEnvelope)) )
        {
            return TRUE;
        }
    }
    else
    {
        OGR_G_GetEnvelope(hGeom, &sEnvelope);
    }

    /* -------------------------------------------------------------- */
    /*  Get style                                                     */
    /* -------------------------------------------------------------- */
    ObjectStyle os;
    GetObjectStyle(nullptr, hFeat, adfMatrix, m_oMapSymbolFilenameToDesc, os);

    double dfRadius = os.dfSymbolSize * dfUserUnit;

    // For a POINT with only a LABEL style string and non-empty text, we do not
    // output any geometry other than the text itself.
    const bool bLabelOnly = wkbFlatten(OGR_G_GetGeometryType(hGeom)) == wkbPoint &&
        !os.bHasPenBrushOrSymbol && !os.osLabelText.empty();

    /* -------------------------------------------------------------- */
    /*  Write object dictionary                                       */
    /* -------------------------------------------------------------- */
    if (!bLabelOnly)
    {
        auto nObjectId = AllocNewObject();

        osVectorDesc.aIds.push_back(nObjectId);

        int bboxXMin, bboxYMin, bboxXMax, bboxYMax;
        ComputeIntBBox(hGeom, sEnvelope, adfMatrix, os, dfRadius,
                       bboxXMin, bboxYMin, bboxXMax, bboxYMax);

        auto nLinkId = WriteLink(hFeat, pszOGRLinkField, adfMatrix,
                  bboxXMin, bboxYMin, bboxXMax, bboxYMax);
        if( nLinkId.toBool() )
            oPageContext.anAnnotationsId.push_back(nLinkId);

        GDALPDFDictionaryRW oDict;
        GDALPDFArrayRW* poBBOX = new GDALPDFArrayRW();
        poBBOX->Add(bboxXMin).Add(bboxYMin).Add(bboxXMax). Add(bboxYMax);
        oDict.Add("Type", GDALPDFObjectRW::CreateName("XObject"))
            .Add("BBox", poBBOX)
            .Add("Subtype", GDALPDFObjectRW::CreateName("Form"));

        GDALPDFDictionaryRW* poGS1 = new GDALPDFDictionaryRW();
        poGS1->Add("Type", GDALPDFObjectRW::CreateName("ExtGState"));
        if (os.nPenA != 255)
            poGS1->Add("CA", (os.nPenA == 127 || os.nPenA == 128) ? 0.5 : os.nPenA / 255.0);
        if (os.nBrushA != 255)
            poGS1->Add("ca", (os.nBrushA == 127 || os.nBrushA == 128) ? 0.5 : os.nBrushA / 255.0 );

        GDALPDFDictionaryRW* poExtGState = new GDALPDFDictionaryRW();
        poExtGState->Add("GS1", poGS1);

        GDALPDFDictionaryRW* poResources = new GDALPDFDictionaryRW();
        poResources->Add("ExtGState", poExtGState);

        if( os.nImageSymbolId.toBool() )
        {
            GDALPDFDictionaryRW* poDictXObject = new GDALPDFDictionaryRW();
            poResources->Add("XObject", poDictXObject);

            poDictXObject->Add(CPLSPrintf("SymImage%d", os.nImageSymbolId.toInt()), os.nImageSymbolId, 0);
        }

        oDict.Add("Resources", poResources);

        StartObjWithStream(nObjectId, oDict,
                           oPageContext.eStreamCompressMethod != COMPRESS_NONE);

        /* -------------------------------------------------------------- */
        /*  Write object stream                                           */
        /* -------------------------------------------------------------- */
        VSIFPrintfL(m_fp, "q\n");

        VSIFPrintfL(m_fp, "/GS1 gs\n");

        VSIFPrintfL(m_fp, "%s",
                GenerateDrawingStream(hGeom, adfMatrix, os, dfRadius).c_str());

        VSIFPrintfL(m_fp, "Q");

        EndObjWithStream();
    }
    else
    {
        osVectorDesc.aIds.push_back(GDALPDFObjectNum());
    }

    /* -------------------------------------------------------------- */
    /*  Write label                                                   */
    /* -------------------------------------------------------------- */
    if (!os.osLabelText.empty() && wkbFlatten(OGR_G_GetGeometryType(hGeom)) == wkbPoint)
    {
        if (!osVectorDesc.nOCGTextId.toBool())
            osVectorDesc.nOCGTextId = WriteOCG("Text", osVectorDesc.nOCGId);

        int  nWidth = poClippingDS->GetRasterXSize();
        double dfWidthInUserUnit = nWidth / dfUserUnit + oPageContext.sMargins.nLeft + oPageContext.sMargins.nRight;
        double dfHeightInUserUnit = nHeight / dfUserUnit + oPageContext.sMargins.nBottom + oPageContext.sMargins.nTop;
        auto nObjectId = WriteLabel(hGeom, adfMatrix, os,
                                    oPageContext.eStreamCompressMethod,
                                    0,0,dfWidthInUserUnit,dfHeightInUserUnit);

        osVectorDesc.aIdsText.push_back(nObjectId);
    }
    else
    {
        osVectorDesc.aIdsText.push_back(GDALPDFObjectNum());
    }

    /* -------------------------------------------------------------- */
    /*  Write feature attributes                                      */
    /* -------------------------------------------------------------- */
    GDALPDFObjectNum nFeatureUserProperties;

    CPLString osFeatureName;

    if (bWriteOGRAttributes)
    {
        nFeatureUserProperties = WriteAttributes(
            hFeat,
            osVectorDesc.aosIncludedFields,
            pszOGRDisplayField, iObj,
            osVectorDesc.nFeatureLayerId,
            oPageContext.nPageId,
            osFeatureName);
    }

    iObj ++;

    osVectorDesc.aUserPropertiesIds.push_back(nFeatureUserProperties);
    osVectorDesc.aFeatureNames.push_back(osFeatureName);

    return TRUE;
}

/************************************************************************/
/*                               EndPage()                              */
/************************************************************************/

int GDALPDFWriter::EndPage(const char* pszExtraImages,
                           const char* pszExtraStream,
                           const char* pszExtraLayerName,
                           const char* pszOffLayers,
                           const char* pszExclusiveLayers)
{
    auto nLayerExtraId = WriteOCG(pszExtraLayerName);
    if( pszOffLayers )
        m_osOffLayers = pszOffLayers;
    if( pszExclusiveLayers )
        m_osExclusiveLayers = pszExclusiveLayers;

    /* -------------------------------------------------------------- */
    /*  Write extra images                                            */
    /* -------------------------------------------------------------- */
    std::vector<GDALPDFImageDesc> asExtraImageDesc;
    if (pszExtraImages)
    {
        if( GDALGetDriverCount() == 0 )
            GDALAllRegister();

        char** papszExtraImagesTokens = CSLTokenizeString2(pszExtraImages, ",", 0);
        double dfUserUnit = oPageContext.dfDPI * USER_UNIT_IN_INCH;
        int nCount = CSLCount(papszExtraImagesTokens);
        for(int i=0;i+4<=nCount; /* */)
        {
            const char* pszImageFilename = papszExtraImagesTokens[i+0];
            double dfX = CPLAtof(papszExtraImagesTokens[i+1]);
            double dfY = CPLAtof(papszExtraImagesTokens[i+2]);
            double dfScale = CPLAtof(papszExtraImagesTokens[i+3]);
            const char* pszLinkVal = nullptr;
            i += 4;
            if( i < nCount && STARTS_WITH_CI(papszExtraImagesTokens[i], "link=") )
            {
                pszLinkVal = papszExtraImagesTokens[i] + 5;
                i++;
            }
            GDALDataset* poImageDS = (GDALDataset* )GDALOpen(pszImageFilename, GA_ReadOnly);
            if (poImageDS)
            {
                auto nImageId = WriteBlock( poImageDS,
                                            0, 0,
                                            poImageDS->GetRasterXSize(),
                                            poImageDS->GetRasterYSize(),
                                            GDALPDFObjectNum(),
                                            COMPRESS_DEFAULT,
                                            0,
                                            -1,
                                            nullptr,
                                            nullptr,
                                            nullptr );

                if (nImageId.toBool())
                {
                    GDALPDFImageDesc oImageDesc;
                    oImageDesc.nImageId = nImageId;
                    oImageDesc.dfXSize = poImageDS->GetRasterXSize() / dfUserUnit * dfScale;
                    oImageDesc.dfYSize = poImageDS->GetRasterYSize() / dfUserUnit * dfScale;
                    oImageDesc.dfXOff = dfX;
                    oImageDesc.dfYOff = dfY;

                    asExtraImageDesc.push_back(oImageDesc);

                    if( pszLinkVal != nullptr )
                    {
                        auto nAnnotId = AllocNewObject();
                        oPageContext.anAnnotationsId.push_back(nAnnotId);
                        StartObj(nAnnotId);
                        {
                            GDALPDFDictionaryRW oDict;
                            oDict.Add("Type", GDALPDFObjectRW::CreateName("Annot"));
                            oDict.Add("Subtype", GDALPDFObjectRW::CreateName("Link"));
                            oDict.Add("Rect", &(new GDALPDFArrayRW())->
                                Add(oImageDesc.dfXOff).
                                Add(oImageDesc.dfYOff).
                                Add(oImageDesc.dfXOff + oImageDesc.dfXSize).
                                Add(oImageDesc.dfYOff + oImageDesc.dfYSize));
                            oDict.Add("A", &(new GDALPDFDictionaryRW())->
                                Add("S", GDALPDFObjectRW::CreateName("URI")).
                                Add("URI", pszLinkVal));
                            oDict.Add("BS", &(new GDALPDFDictionaryRW())->
                                Add("Type", GDALPDFObjectRW::CreateName("Border")).
                                Add("S", GDALPDFObjectRW::CreateName("S")).
                                Add("W", 0));
                            oDict.Add("Border", &(new GDALPDFArrayRW())->Add(0).Add(0).Add(0));
                            oDict.Add("H", GDALPDFObjectRW::CreateName("I"));

                            VSIFPrintfL(m_fp, "%s\n", oDict.Serialize().c_str());
                        }
                        EndObj();
                    }
                }

                GDALClose(poImageDS);
            }
        }
        CSLDestroy(papszExtraImagesTokens);
    }

    /* -------------------------------------------------------------- */
    /*  Write content stream                                          */
    /* -------------------------------------------------------------- */
    GDALPDFDictionaryRW oDictContent;
    StartObjWithStream(oPageContext.nContentId, oDictContent,
                       oPageContext.eStreamCompressMethod != COMPRESS_NONE );

    /* -------------------------------------------------------------- */
    /*  Write drawing instructions for raster blocks                  */
    /* -------------------------------------------------------------- */
    for(size_t iRaster = 0; iRaster < oPageContext.asRasterDesc.size(); iRaster++)
    {
        const GDALPDFRasterDesc& oDesc = oPageContext.asRasterDesc[iRaster];
        if (oDesc.nOCGRasterId.toBool())
            VSIFPrintfL(m_fp, "/OC /Lyr%d BDC\n", oDesc.nOCGRasterId.toInt());

        for(size_t iImage = 0; iImage < oDesc.asImageDesc.size(); iImage ++)
        {
            VSIFPrintfL(m_fp, "q\n");
            GDALPDFObjectRW* poXSize = GDALPDFObjectRW::CreateReal(oDesc.asImageDesc[iImage].dfXSize);
            GDALPDFObjectRW* poYSize = GDALPDFObjectRW::CreateReal(oDesc.asImageDesc[iImage].dfYSize);
            GDALPDFObjectRW* poXOff = GDALPDFObjectRW::CreateReal(oDesc.asImageDesc[iImage].dfXOff);
            GDALPDFObjectRW* poYOff = GDALPDFObjectRW::CreateReal(oDesc.asImageDesc[iImage].dfYOff);
            VSIFPrintfL(m_fp, "%s 0 0 %s %s %s cm\n",
                        poXSize->Serialize().c_str(),
                        poYSize->Serialize().c_str(),
                        poXOff->Serialize().c_str(),
                        poYOff->Serialize().c_str());
            delete poXSize;
            delete poYSize;
            delete poXOff;
            delete poYOff;
            VSIFPrintfL(m_fp, "/Image%d Do\n",
                        oDesc.asImageDesc[iImage].nImageId.toInt());
            VSIFPrintfL(m_fp, "Q\n");
        }

        if (oDesc.nOCGRasterId.toBool())
            VSIFPrintfL(m_fp, "EMC\n");
    }

    /* -------------------------------------------------------------- */
    /*  Write drawing instructions for vector features                */
    /* -------------------------------------------------------------- */
    int iObj = 0;
    for(size_t iLayer = 0; iLayer < oPageContext.asVectorDesc.size(); iLayer ++)
    {
        GDALPDFLayerDesc& oLayerDesc = oPageContext.asVectorDesc[iLayer];

        VSIFPrintfL(m_fp, "/OC /Lyr%d BDC\n", oLayerDesc.nOCGId.toInt());

        for(size_t iVector = 0; iVector < oLayerDesc.aIds.size(); iVector ++)
        {
            if (oLayerDesc.aIds[iVector].toBool())
            {
                CPLString osName = oLayerDesc.aFeatureNames[iVector];
                if (!osName.empty() )
                {
                    VSIFPrintfL(m_fp, "/feature <</MCID %d>> BDC\n",
                                iObj);
                }

                VSIFPrintfL(m_fp, "/Vector%d Do\n", oLayerDesc.aIds[iVector].toInt());

                if (!osName.empty() )
                {
                    VSIFPrintfL(m_fp, "EMC\n");
                }
            }

            iObj ++;
        }

        VSIFPrintfL(m_fp, "EMC\n");
    }

    /* -------------------------------------------------------------- */
    /*  Write drawing instructions for labels of vector features      */
    /* -------------------------------------------------------------- */
    iObj = 0;
    for(size_t iLayer = 0; iLayer < oPageContext.asVectorDesc.size(); iLayer ++)
    {
        GDALPDFLayerDesc& oLayerDesc = oPageContext.asVectorDesc[iLayer];
        if (oLayerDesc.nOCGTextId.toBool())
        {
            VSIFPrintfL(m_fp, "/OC /Lyr%d BDC\n", oLayerDesc.nOCGId.toInt());
            VSIFPrintfL(m_fp, "/OC /Lyr%d BDC\n", oLayerDesc.nOCGTextId.toInt());

            for(size_t iVector = 0; iVector < oLayerDesc.aIdsText.size(); iVector ++)
            {
                if (oLayerDesc.aIdsText[iVector].toBool())
                {
                    CPLString osName = oLayerDesc.aFeatureNames[iVector];
                    if (!osName.empty() )
                    {
                        VSIFPrintfL(m_fp, "/feature <</MCID %d>> BDC\n",
                                    iObj);
                    }

                    VSIFPrintfL(m_fp, "/Text%d Do\n", oLayerDesc.aIdsText[iVector].toInt());

                    if (!osName.empty() )
                    {
                        VSIFPrintfL(m_fp, "EMC\n");
                    }
                }

                iObj ++;
            }

            VSIFPrintfL(m_fp, "EMC\n");
            VSIFPrintfL(m_fp, "EMC\n");
        }
        else
            iObj += (int) oLayerDesc.aIds.size();
    }

    /* -------------------------------------------------------------- */
    /*  Write drawing instructions for extra content.                 */
    /* -------------------------------------------------------------- */
    if (pszExtraStream || !asExtraImageDesc.empty() )
    {
        if (nLayerExtraId.toBool())
            VSIFPrintfL(m_fp, "/OC /Lyr%d BDC\n", nLayerExtraId.toInt());

        /* -------------------------------------------------------------- */
        /*  Write drawing instructions for extra images.                  */
        /* -------------------------------------------------------------- */
        for(size_t iImage = 0; iImage < asExtraImageDesc.size(); iImage ++)
        {
            VSIFPrintfL(m_fp, "q\n");
            GDALPDFObjectRW* poXSize = GDALPDFObjectRW::CreateReal(asExtraImageDesc[iImage].dfXSize);
            GDALPDFObjectRW* poYSize = GDALPDFObjectRW::CreateReal(asExtraImageDesc[iImage].dfYSize);
            GDALPDFObjectRW* poXOff = GDALPDFObjectRW::CreateReal(asExtraImageDesc[iImage].dfXOff);
            GDALPDFObjectRW* poYOff = GDALPDFObjectRW::CreateReal(asExtraImageDesc[iImage].dfYOff);
            VSIFPrintfL(m_fp, "%s 0 0 %s %s %s cm\n",
                        poXSize->Serialize().c_str(),
                        poYSize->Serialize().c_str(),
                        poXOff->Serialize().c_str(),
                        poYOff->Serialize().c_str());
            delete poXSize;
            delete poYSize;
            delete poXOff;
            delete poYOff;
            VSIFPrintfL(m_fp, "/Image%d Do\n",
                        asExtraImageDesc[iImage].nImageId.toInt());
            VSIFPrintfL(m_fp, "Q\n");
        }

        if (pszExtraStream)
            VSIFPrintfL(m_fp, "%s\n", pszExtraStream);

        if (nLayerExtraId.toBool())
            VSIFPrintfL(m_fp, "EMC\n");
    }

    EndObjWithStream();

    /* -------------------------------------------------------------- */
    /*  Write objects for feature tree.                               */
    /* -------------------------------------------------------------- */
    if (m_nStructTreeRootId.toBool())
    {
        auto nParentTreeId = AllocNewObject();
        StartObj(nParentTreeId);
        VSIFPrintfL(m_fp, "<< /Nums [ 0 ");
        VSIFPrintfL(m_fp, "[ ");
        for(size_t iLayer = 0; iLayer < oPageContext.asVectorDesc.size(); iLayer ++)
        {
            GDALPDFLayerDesc& oLayerDesc = oPageContext.asVectorDesc[iLayer];
            for(size_t iVector = 0; iVector < oLayerDesc.aIds.size(); iVector ++)
            {
                const auto& nId = oLayerDesc.aUserPropertiesIds[iVector];
                if (nId.toBool())
                    VSIFPrintfL(m_fp, "%d 0 R ", nId.toInt());
            }
        }
        VSIFPrintfL(m_fp, " ]\n");
        VSIFPrintfL(m_fp, " ] >> \n");
        EndObj();

        StartObj(m_nStructTreeRootId);
        VSIFPrintfL(m_fp,
                    "<< "
                    "/Type /StructTreeRoot "
                    "/ParentTree %d 0 R "
                    "/K [ ", nParentTreeId.toInt());
        for(size_t iLayer = 0; iLayer < oPageContext.asVectorDesc.size(); iLayer ++)
        {
            VSIFPrintfL(m_fp, "%d 0 R ", oPageContext.asVectorDesc[iLayer]. nFeatureLayerId.toInt());
        }
        VSIFPrintfL(m_fp,"] >>\n");
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
        for(size_t iRaster = 0; iRaster < oPageContext.asRasterDesc.size(); iRaster++)
        {
            const GDALPDFRasterDesc& oDesc = oPageContext.asRasterDesc[iRaster];
            for(iImage = 0; iImage < oDesc.asImageDesc.size(); iImage ++)
            {
                poDictXObject->Add(CPLSPrintf("Image%d", oDesc.asImageDesc[iImage].nImageId.toInt()),
                                oDesc.asImageDesc[iImage].nImageId, 0);
            }
        }
        for(iImage = 0; iImage < asExtraImageDesc.size(); iImage ++)
        {
            poDictXObject->Add(CPLSPrintf("Image%d", asExtraImageDesc[iImage].nImageId.toInt()),
                               asExtraImageDesc[iImage].nImageId, 0);
        }
        for(size_t iLayer = 0; iLayer < oPageContext.asVectorDesc.size(); iLayer ++)
        {
            GDALPDFLayerDesc& oLayerDesc = oPageContext.asVectorDesc[iLayer];
            for(size_t iVector = 0; iVector < oLayerDesc.aIds.size(); iVector ++)
            {
                if (oLayerDesc.aIds[iVector].toBool())
                    poDictXObject->Add(CPLSPrintf("Vector%d", oLayerDesc.aIds[iVector].toInt()),
                        oLayerDesc.aIds[iVector], 0);
            }
            for(size_t iVector = 0; iVector < oLayerDesc.aIdsText.size(); iVector ++)
            {
                if (oLayerDesc.aIdsText[iVector].toBool())
                    poDictXObject->Add(CPLSPrintf("Text%d", oLayerDesc.aIdsText[iVector].toInt()),
                        oLayerDesc.aIdsText[iVector], 0);
            }
        }

        if (pszExtraStream)
        {
            std::vector<CPLString> aosNeededFonts;
            if (strstr(pszExtraStream, "/FTimes"))
            {
                aosNeededFonts.push_back("Times-Roman");
                aosNeededFonts.push_back("Times-Bold");
                aosNeededFonts.push_back("Times-Italic");
                aosNeededFonts.push_back("Times-BoldItalic");
            }
            if (strstr(pszExtraStream, "/FHelvetica"))
            {
                aosNeededFonts.push_back("Helvetica");
                aosNeededFonts.push_back("Helvetica-Bold");
                aosNeededFonts.push_back("Helvetica-Oblique");
                aosNeededFonts.push_back("Helvetica-BoldOblique");
            }
            if (strstr(pszExtraStream, "/FCourier"))
            {
                aosNeededFonts.push_back("Courier");
                aosNeededFonts.push_back("Courier-Bold");
                aosNeededFonts.push_back("Courier-Oblique");
                aosNeededFonts.push_back("Courier-BoldOblique");
            }
            if (strstr(pszExtraStream, "/FSymbol"))
                aosNeededFonts.push_back("Symbol");
            if (strstr(pszExtraStream, "/FZapfDingbats"))
                aosNeededFonts.push_back("ZapfDingbats");

            if (!aosNeededFonts.empty())
            {
                GDALPDFDictionaryRW* poDictFont = new GDALPDFDictionaryRW();

                for (CPLString& osFont : aosNeededFonts)
                {
                    GDALPDFDictionaryRW* poDictFontInner = new GDALPDFDictionaryRW();
                    poDictFontInner->Add("Type",
                        GDALPDFObjectRW::CreateName("Font"));
                    poDictFontInner->Add("BaseFont",
                        GDALPDFObjectRW::CreateName(osFont));
                    poDictFontInner->Add("Encoding",
                        GDALPDFObjectRW::CreateName("WinAnsiEncoding"));
                    poDictFontInner->Add("Subtype",
                        GDALPDFObjectRW::CreateName("Type1"));

                    osFont = "F" + osFont;
                    const size_t nHyphenPos = osFont.find('-');
                    if (nHyphenPos != std::string::npos)
                        osFont.erase(nHyphenPos, 1);
                    poDictFont->Add(osFont, poDictFontInner);
                }

                oDict.Add("Font", poDictFont);
            }
        }

        if (!m_asOCGs.empty() )
        {
            GDALPDFDictionaryRW* poDictProperties = new GDALPDFDictionaryRW();
            for(size_t i=0; i<m_asOCGs.size(); i++)
                poDictProperties->Add(CPLSPrintf("Lyr%d", m_asOCGs[i].nId.toInt()),
                                      m_asOCGs[i].nId, 0);
            oDict.Add("Properties", poDictProperties);
        }

        VSIFPrintfL(m_fp, "%s\n", oDict.Serialize().c_str());
    }
    EndObj();

    /* -------------------------------------------------------------- */
    /*  Write annotation arrays.                                      */
    /* -------------------------------------------------------------- */
    StartObj(oPageContext.nAnnotsId);
    {
        GDALPDFArrayRW oArray;
        for(size_t i = 0; i < oPageContext.anAnnotationsId.size(); i++)
        {
            oArray.Add(oPageContext.anAnnotationsId[i], 0);
        }
        VSIFPrintfL(m_fp, "%s\n", oArray.Serialize().c_str());
    }
    EndObj();

    return TRUE;
}

/************************************************************************/
/*                             WriteMask()                              */
/************************************************************************/

GDALPDFObjectNum GDALPDFBaseWriter::WriteMask(GDALDataset* poSrcDS,
                             int nXOff, int nYOff, int nReqXSize, int nReqYSize,
                             PDFCompressMethod eCompressMethod)
{
    int nMaskSize = nReqXSize * nReqYSize;
    GByte* pabyMask = (GByte*)VSIMalloc(nMaskSize);
    if (pabyMask == nullptr)
        return GDALPDFObjectNum();

    CPLErr eErr;
    eErr = poSrcDS->GetRasterBand(4)->RasterIO(
            GF_Read,
            nXOff, nYOff,
            nReqXSize, nReqYSize,
            pabyMask, nReqXSize, nReqYSize, GDT_Byte,
            0, 0, nullptr);
    if (eErr != CE_None)
    {
        VSIFree(pabyMask);
        return GDALPDFObjectNum();
    }

    int bOnly0or255 = TRUE;
    int bOnly255 = TRUE;
    /* int bOnly0 = TRUE; */
    int i;
    for(i=0;i<nReqXSize * nReqYSize;i++)
    {
        if (pabyMask[i] == 0)
            bOnly255 = FALSE;
        else if (pabyMask[i] == 255)
        {
            /* bOnly0 = FALSE; */
        }
        else
        {
            /* bOnly0 = FALSE; */
            bOnly255 = FALSE;
            bOnly0or255 = FALSE;
            break;
        }
    }

    if (bOnly255)
    {
        CPLFree(pabyMask);
        return GDALPDFObjectNum();
    }

    if (bOnly0or255)
    {
        /* Translate to 1 bit */
        int nReqXSize1 = (nReqXSize + 7) / 8;
        GByte* pabyMask1 = (GByte*)VSICalloc(nReqXSize1, nReqYSize);
        if (pabyMask1 == nullptr)
        {
            CPLFree(pabyMask);
            return GDALPDFObjectNum();
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

    auto nMaskId = AllocNewObject();

    GDALPDFDictionaryRW oDict;
    oDict.Add("Type", GDALPDFObjectRW::CreateName("XObject"))
         .Add("Subtype", GDALPDFObjectRW::CreateName("Image"))
         .Add("Width", nReqXSize)
         .Add("Height", nReqYSize)
         .Add("ColorSpace", GDALPDFObjectRW::CreateName("DeviceGray"))
         .Add("BitsPerComponent", (bOnly0or255) ? 1 : 8);

    StartObjWithStream(nMaskId, oDict, eCompressMethod != COMPRESS_NONE );

    VSIFWriteL(pabyMask, nMaskSize, 1, m_fp);
    CPLFree(pabyMask);

    EndObjWithStream();

    return nMaskId;
}

/************************************************************************/
/*                             WriteBlock()                             */
/************************************************************************/

GDALPDFObjectNum GDALPDFBaseWriter::WriteBlock(GDALDataset* poSrcDS,
                             int nXOff, int nYOff, int nReqXSize, int nReqYSize,
                             const GDALPDFObjectNum& nColorTableIdIn,
                             PDFCompressMethod eCompressMethod,
                             int nPredictor,
                             int nJPEGQuality,
                             const char* pszJPEG2000_DRIVER,
                             GDALProgressFunc pfnProgress,
                             void * pProgressData)
{
    int  nBands = poSrcDS->GetRasterCount();
    if (nBands == 0)
        return GDALPDFObjectNum();

    GDALPDFObjectNum nColorTableId(nColorTableIdIn);
    if (!nColorTableId.toBool())
        nColorTableId = WriteColorTable(poSrcDS);

    CPLErr eErr = CE_None;
    GDALDataset* poBlockSrcDS = nullptr;
    GDALDatasetH hMemDS = nullptr;
    GByte* pabyMEMDSBuffer = nullptr;

    if (eCompressMethod == COMPRESS_DEFAULT)
    {
        GDALDataset* poSrcDSToTest = poSrcDS;

        /* Test if we can directly copy original JPEG content */
        /* if available */
        if (poSrcDS->GetDriver() != nullptr &&
            poSrcDS->GetDriver() == GDALGetDriverByName("VRT"))
        {
            VRTDataset* poVRTDS = (VRTDataset* )poSrcDS;
            poSrcDSToTest = poVRTDS->GetSingleSimpleSource();
        }

        if (poSrcDSToTest != nullptr &&
            poSrcDSToTest->GetDriver() != nullptr &&
            EQUAL(poSrcDSToTest->GetDriver()->GetDescription(), "JPEG") &&
            nXOff == 0 && nYOff == 0 &&
            nReqXSize == poSrcDSToTest->GetRasterXSize() &&
            nReqYSize == poSrcDSToTest->GetRasterYSize() &&
            nJPEGQuality < 0)
        {
            VSILFILE* fpSrc = VSIFOpenL(poSrcDSToTest->GetDescription(), "rb");
            if (fpSrc != nullptr)
            {
                CPLDebug("PDF", "Copying directly original JPEG file");

                VSIFSeekL(fpSrc, 0, SEEK_END);
                int nLength = (int)VSIFTellL(fpSrc);
                VSIFSeekL(fpSrc, 0, SEEK_SET);

                auto nImageId = AllocNewObject();

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
                VSIFPrintfL(m_fp, "%s\n", oDict.Serialize().c_str());
                VSIFPrintfL(m_fp, "stream\n");

                GByte abyBuffer[1024];
                for(int i=0;i<nLength;i += 1024)
                {
                    int nRead = (int) VSIFReadL(abyBuffer, 1, 1024, fpSrc);
                    if ((int)VSIFWriteL(abyBuffer, 1, nRead, m_fp) != nRead)
                    {
                        eErr = CE_Failure;
                        break;
                    }

                    if( eErr == CE_None && pfnProgress != nullptr
                        && !pfnProgress( (i + nRead) / (double)nLength,
                                        nullptr, pProgressData ) )
                    {
                        CPLError( CE_Failure, CPLE_UserInterrupt,
                                "User terminated CreateCopy()" );
                        eErr = CE_Failure;
                        break;
                    }
                }

                VSIFPrintfL(m_fp, "\nendstream\n");

                EndObj();

                VSIFCloseL(fpSrc);

                return eErr == CE_None ? nImageId : GDALPDFObjectNum();
            }
        }

        eCompressMethod = COMPRESS_DEFLATE;
    }

    GDALPDFObjectNum nMaskId;
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
        if( hMemDriver == nullptr )
            return GDALPDFObjectNum();

        hMemDS = GDALCreate(hMemDriver, "MEM:::",
                            nReqXSize, nReqYSize, 0,
                            GDT_Byte, nullptr);
        if (hMemDS == nullptr)
            return GDALPDFObjectNum();

        pabyMEMDSBuffer =
            (GByte*)VSIMalloc3(nReqXSize, nReqYSize, nBands);
        if (pabyMEMDSBuffer == nullptr)
        {
            GDALClose(hMemDS);
            return GDALPDFObjectNum();
        }

        eErr = poSrcDS->RasterIO(GF_Read,
                                nXOff, nYOff,
                                nReqXSize, nReqYSize,
                                pabyMEMDSBuffer, nReqXSize, nReqYSize,
                                GDT_Byte, nBands, nullptr,
                                0, 0, 0, nullptr);

        if( eErr != CE_None )
        {
            CPLFree(pabyMEMDSBuffer);
            GDALClose(hMemDS);
            return GDALPDFObjectNum();
        }

        int iBand;
        for(iBand = 0; iBand < nBands; iBand ++)
        {
            char** papszMEMDSOptions = nullptr;
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

    auto nImageId = AllocNewObject();

    GDALPDFObjectNum nMeasureId;
    if( CPLTestBool(CPLGetConfigOption("GDAL_PDF_WRITE_GEOREF_ON_IMAGE", "FALSE")) &&
        nReqXSize == poSrcDS->GetRasterXSize() &&
        nReqYSize == poSrcDS->GetRasterYSize() )
    {
        PDFMargins sMargins;
        nMeasureId = WriteSRS_ISO32000(poSrcDS, 1, nullptr, &sMargins, FALSE);
    }

    GDALPDFDictionaryRW oDict;
    oDict.Add("Type", GDALPDFObjectRW::CreateName("XObject"));

    if( eCompressMethod == COMPRESS_DEFLATE )
    {
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
              (nColorTableId.toBool()) ? GDALPDFObjectRW::CreateIndirect(nColorTableId, 0) :
              (nBands == 1) ?        GDALPDFObjectRW::CreateName("DeviceGray") :
                                     GDALPDFObjectRW::CreateName("DeviceRGB"))
         .Add("BitsPerComponent", 8);
    if( nMaskId.toBool() )
    {
        oDict.Add("SMask", nMaskId, 0);
    }
    if( nMeasureId.toBool() )
    {
        oDict.Add("Measure", nMeasureId, 0);
    }

    StartObjWithStream(nImageId, oDict,
                       eCompressMethod == COMPRESS_DEFLATE );

    if( eCompressMethod == COMPRESS_JPEG ||
        eCompressMethod == COMPRESS_JPEG2000 )
    {
        GDALDriver* poJPEGDriver = nullptr;
        char szTmp[64];
        char** papszOptions = nullptr;

        if( eCompressMethod == COMPRESS_JPEG )
        {
            poJPEGDriver = (GDALDriver*) GDALGetDriverByName("JPEG");
            if (poJPEGDriver != nullptr && nJPEGQuality > 0)
                papszOptions = CSLAddString(papszOptions, CPLSPrintf("QUALITY=%d", nJPEGQuality));
            snprintf(szTmp, sizeof(szTmp), "/vsimem/pdftemp/%p.jpg", this);
        }
        else
        {
            if (pszJPEG2000_DRIVER == nullptr || EQUAL(pszJPEG2000_DRIVER, "JP2KAK"))
                poJPEGDriver = (GDALDriver*) GDALGetDriverByName("JP2KAK");
            if (poJPEGDriver == nullptr)
            {
                if (pszJPEG2000_DRIVER == nullptr || EQUAL(pszJPEG2000_DRIVER, "JP2ECW"))
                {
                    poJPEGDriver = (GDALDriver*) GDALGetDriverByName("JP2ECW");
                    if( poJPEGDriver &&
                        poJPEGDriver->GetMetadataItem(GDAL_DMD_CREATIONDATATYPES) == nullptr )
                    {
                        poJPEGDriver = nullptr;
                    }
                }
                if (poJPEGDriver)
                {
                    papszOptions = CSLAddString(papszOptions, "PROFILE=NPJE");
                    papszOptions = CSLAddString(papszOptions, "LAYERS=1");
                    papszOptions = CSLAddString(papszOptions, "GeoJP2=OFF");
                    papszOptions = CSLAddString(papszOptions, "GMLJP2=OFF");
                }
            }
            if (poJPEGDriver == nullptr)
            {
                if (pszJPEG2000_DRIVER == nullptr || EQUAL(pszJPEG2000_DRIVER, "JP2OpenJPEG"))
                    poJPEGDriver = (GDALDriver*) GDALGetDriverByName("JP2OpenJPEG");
                if (poJPEGDriver)
                {
                    papszOptions = CSLAddString(papszOptions, "GeoJP2=OFF");
                    papszOptions = CSLAddString(papszOptions, "GMLJP2=OFF");
                }
            }
            snprintf(szTmp, sizeof(szTmp), "/vsimem/pdftemp/%p.jp2", this);
        }

        if( poJPEGDriver == nullptr )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "No %s driver found",
                     ( eCompressMethod == COMPRESS_JPEG ) ? "JPEG" : "JPEG2000");
            eErr = CE_Failure;
            goto end;
        }

        GDALDataset* poJPEGDS = poJPEGDriver->CreateCopy(szTmp, poBlockSrcDS,
                                            FALSE, papszOptions,
                                            pfnProgress, pProgressData);

        CSLDestroy(papszOptions);
        if( poJPEGDS == nullptr )
        {
            eErr = CE_Failure;
            goto end;
        }

        GDALClose(poJPEGDS);

        vsi_l_offset nJPEGDataSize = 0;
        GByte* pabyJPEGData = VSIGetMemFileBuffer(szTmp, &nJPEGDataSize, TRUE);
        VSIFWriteL(pabyJPEGData, static_cast<size_t>(nJPEGDataSize), 1, m_fp);
        CPLFree(pabyJPEGData);
    }
    else
    {
        GByte* pabyLine = (GByte*)CPLMalloc(nReqXSize * nBands);
        for(int iLine = 0; iLine < nReqYSize; iLine ++)
        {
            /* Get pixel interleaved data */
            eErr = poBlockSrcDS->RasterIO(GF_Read,
                                          0, iLine, nReqXSize, 1,
                                          pabyLine, nReqXSize, 1, GDT_Byte,
                                          nBands, nullptr, nBands, 0, 1, nullptr);
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

            if( VSIFWriteL(pabyLine, nReqXSize * nBands, 1, m_fp) != 1 )
            {
                eErr = CE_Failure;
                break;
            }

            if( pfnProgress != nullptr
                && !pfnProgress( (iLine+1) / (double)nReqYSize,
                                nullptr, pProgressData ) )
            {
                CPLError( CE_Failure, CPLE_UserInterrupt,
                        "User terminated CreateCopy()" );
                eErr = CE_Failure;
                break;
            }
        }

        CPLFree(pabyLine);
    }

end:
    CPLFree(pabyMEMDSBuffer);
    pabyMEMDSBuffer = nullptr;
    if( hMemDS != nullptr )
    {
        GDALClose(hMemDS);
        hMemDS = nullptr;
    }

    EndObjWithStream();

    return eErr == CE_None ? nImageId : GDALPDFObjectNum();
}

/************************************************************************/
/*                          WriteJavascript()                           */
/************************************************************************/

GDALPDFObjectNum GDALPDFBaseWriter::WriteJavascript(const char* pszJavascript,
                                                    bool bDeflate)
{
    auto nJSId = AllocNewObject();
    {
        GDALPDFDictionaryRW oDict;
        StartObjWithStream(nJSId, oDict, bDeflate);

        VSIFWriteL(pszJavascript, strlen(pszJavascript), 1, m_fp);
        VSIFPrintfL(m_fp, "\n");

        EndObjWithStream();
    }

    m_nNamesId = AllocNewObject();
    StartObj(m_nNamesId);
    {
        GDALPDFDictionaryRW oDict;
        GDALPDFDictionaryRW* poJavaScriptDict = new GDALPDFDictionaryRW();
        oDict.Add("JavaScript", poJavaScriptDict);

        GDALPDFArrayRW* poNamesArray = new GDALPDFArrayRW();
        poJavaScriptDict->Add("Names", poNamesArray);

        poNamesArray->Add("GDAL");

        GDALPDFDictionaryRW* poJSDict = new GDALPDFDictionaryRW();
        poNamesArray->Add(poJSDict);

        poJSDict->Add("JS", nJSId, 0);
        poJSDict->Add("S", GDALPDFObjectRW::CreateName("JavaScript"));

        VSIFPrintfL(m_fp, "%s\n", oDict.Serialize().c_str());
    }
    EndObj();

    return m_nNamesId;
}

GDALPDFObjectNum GDALPDFWriter::WriteJavascript(const char* pszJavascript)
{
    return GDALPDFBaseWriter::WriteJavascript(pszJavascript,
                           oPageContext.eStreamCompressMethod != COMPRESS_NONE);
}

/************************************************************************/
/*                        WriteJavascriptFile()                         */
/************************************************************************/

GDALPDFObjectNum GDALPDFWriter::WriteJavascriptFile(const char* pszJavascriptFile)
{
    GDALPDFObjectNum nId;
    char* pszJavascriptToFree = (char*)CPLMalloc(65536);
    VSILFILE* fpJS = VSIFOpenL(pszJavascriptFile, "rb");
    if( fpJS != nullptr )
    {
        int nRead = (int)VSIFReadL(pszJavascriptToFree, 1, 65536, fpJS);
        if( nRead < 65536 )
        {
            pszJavascriptToFree[nRead] = '\0';
            nId = WriteJavascript(pszJavascriptToFree);
        }
        VSIFCloseL(fpJS);
    }
    CPLFree(pszJavascriptToFree);
    return nId;
}

/************************************************************************/
/*                              WritePages()                            */
/************************************************************************/

void GDALPDFWriter::WritePages()
{
    StartObj(m_nPageResourceId);
    {
        GDALPDFDictionaryRW oDict;
        GDALPDFArrayRW* poKids = new GDALPDFArrayRW();
        oDict.Add("Type", GDALPDFObjectRW::CreateName("Pages"))
             .Add("Count", (int)m_asPageId.size())
             .Add("Kids", poKids);

        for(size_t i=0;i<m_asPageId.size();i++)
            poKids->Add(m_asPageId[i], 0);

        VSIFPrintfL(m_fp, "%s\n", oDict.Serialize().c_str());
    }
    EndObj();

    StartObj(m_nCatalogId);
    {
        GDALPDFDictionaryRW oDict;
        oDict.Add("Type", GDALPDFObjectRW::CreateName("Catalog"))
             .Add("Pages", m_nPageResourceId, 0);
        if (m_nXMPId.toBool())
            oDict.Add("Metadata", m_nXMPId, 0);
        if (!m_asOCGs.empty() )
        {
            GDALPDFDictionaryRW* poDictOCProperties = new GDALPDFDictionaryRW();
            oDict.Add("OCProperties", poDictOCProperties);

            GDALPDFDictionaryRW* poDictD = new GDALPDFDictionaryRW();
            poDictOCProperties->Add("D", poDictD);

            /* Build "Order" array of D dict */
            GDALPDFArrayRW* poArrayOrder = new GDALPDFArrayRW();
            for(size_t i=0;i<m_asOCGs.size();i++)
            {
                poArrayOrder->Add(m_asOCGs[i].nId, 0);
                if (i + 1 < m_asOCGs.size() && m_asOCGs[i+1].nParentId == m_asOCGs[i].nId)
                {
                    GDALPDFArrayRW* poSubArrayOrder = new GDALPDFArrayRW();
                    poSubArrayOrder->Add(m_asOCGs[i+1].nId, 0);
                    poArrayOrder->Add(poSubArrayOrder);
                    i ++;
                }
            }
            poDictD->Add("Order", poArrayOrder);

            /* Build "OFF" array of D dict */
            if( !m_osOffLayers.empty() )
            {
                GDALPDFArrayRW* poArrayOFF = new GDALPDFArrayRW();
                char** papszTokens = CSLTokenizeString2(m_osOffLayers, ",", 0);
                for(int i=0; papszTokens[i] != nullptr; i++)
                {
                    size_t j;
                    int bFound = FALSE;
                    for(j=0;j<m_asOCGs.size();j++)
                    {
                        if( strcmp(papszTokens[i], m_asOCGs[j].osLayerName) == 0)
                        {
                            poArrayOFF->Add(m_asOCGs[j].nId, 0);
                            bFound = TRUE;
                        }
                        if (j + 1 < m_asOCGs.size() && m_asOCGs[j+1].nParentId == m_asOCGs[j].nId)
                        {
                            j ++;
                        }
                    }
                    if( !bFound )
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Unknown layer name (%s) specified in OFF_LAYERS",
                                 papszTokens[i]);
                    }
                }
                CSLDestroy(papszTokens);

                poDictD->Add("OFF", poArrayOFF);
            }

            /* Build "RBGroups" array of D dict */
            if( !m_osExclusiveLayers.empty() )
            {
                GDALPDFArrayRW* poArrayRBGroups = new GDALPDFArrayRW();
                char** papszTokens = CSLTokenizeString2(m_osExclusiveLayers, ",", 0);
                for(int i=0; papszTokens[i] != nullptr; i++)
                {
                    size_t j;
                    int bFound = FALSE;
                    for(j=0;j<m_asOCGs.size();j++)
                    {
                        if( strcmp(papszTokens[i], m_asOCGs[j].osLayerName) == 0)
                        {
                            poArrayRBGroups->Add(m_asOCGs[j].nId, 0);
                            bFound = TRUE;
                        }
                        if (j + 1 < m_asOCGs.size() && m_asOCGs[j+1].nParentId == m_asOCGs[j].nId)
                        {
                            j ++;
                        }
                    }
                    if( !bFound )
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                    "Unknown layer name (%s) specified in EXCLUSIVE_LAYERS",
                                    papszTokens[i]);
                    }
                }
                CSLDestroy(papszTokens);

                if( poArrayRBGroups->GetLength() )
                {
                    GDALPDFArrayRW* poMainArrayRBGroups = new GDALPDFArrayRW();
                    poMainArrayRBGroups->Add(poArrayRBGroups);
                    poDictD->Add("RBGroups", poMainArrayRBGroups);
                }
                else
                    delete poArrayRBGroups;
            }

            GDALPDFArrayRW* poArrayOGCs = new GDALPDFArrayRW();
            for(size_t i=0;i<m_asOCGs.size();i++)
                poArrayOGCs->Add(m_asOCGs[i].nId, 0);
            poDictOCProperties->Add("OCGs", poArrayOGCs);
        }

        if (m_nStructTreeRootId.toBool())
        {
            GDALPDFDictionaryRW* poDictMarkInfo = new GDALPDFDictionaryRW();
            oDict.Add("MarkInfo", poDictMarkInfo);
            poDictMarkInfo->Add("UserProperties", GDALPDFObjectRW::CreateBool(TRUE));

            oDict.Add("StructTreeRoot", m_nStructTreeRootId, 0);
        }

        if (m_nNamesId.toBool())
            oDict.Add("Names", m_nNamesId, 0);

        VSIFPrintfL(m_fp, "%s\n", oDict.Serialize().c_str());
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
    if( pszValue  != nullptr )
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
/*                         GDALPDFClippingDataset                       */
/************************************************************************/

class GDALPDFClippingDataset final: public GDALDataset
{
        GDALDataset* poSrcDS;
        double adfGeoTransform[6];

    public:
        GDALPDFClippingDataset(GDALDataset* poSrcDSIn, double adfClippingExtent[4]) : poSrcDS(poSrcDSIn)
        {
            double adfSrcGeoTransform[6];
            poSrcDS->GetGeoTransform(adfSrcGeoTransform);
            adfGeoTransform[0] = adfClippingExtent[0];
            adfGeoTransform[1] = adfSrcGeoTransform[1];
            adfGeoTransform[2] = 0.0;
            adfGeoTransform[3] = adfSrcGeoTransform[5] < 0 ? adfClippingExtent[3] : adfClippingExtent[1];
            adfGeoTransform[4] = 0.0;
            adfGeoTransform[5] = adfSrcGeoTransform[5];
            nRasterXSize = (int)((adfClippingExtent[2] - adfClippingExtent[0]) / adfSrcGeoTransform[1]);
            nRasterYSize = (int)((adfClippingExtent[3] - adfClippingExtent[1]) / fabs(adfSrcGeoTransform[5]));
        }

        virtual CPLErr GetGeoTransform( double * padfGeoTransform ) override
        {
            memcpy(padfGeoTransform, adfGeoTransform, 6 * sizeof(double));
            return CE_None;
        }

        virtual const OGRSpatialReference* GetSpatialRef() const override
        {
            return poSrcDS->GetSpatialRef();
        }
};

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

    if( !pfnProgress( 0.0, nullptr, pProgressData ) )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Some some rudimentary checks                                    */
/* -------------------------------------------------------------------- */
    if( nBands != 1 && nBands != 3 && nBands != 4 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "PDF driver doesn't support %d bands.  Must be 1 (grey or with color table), "
                  "3 (RGB) or 4 bands.\n", nBands );

        return nullptr;
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
            return nullptr;
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
                return nullptr;
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
                return nullptr;
        }
    }

    if (nBands == 1 &&
        poSrcDS->GetRasterBand(1)->GetColorTable() != nullptr &&
        (eCompressMethod == COMPRESS_JPEG || eCompressMethod == COMPRESS_JPEG2000))
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "The source raster band has a color table, which is not appropriate with JPEG or JPEG2000 compression.\n"
                  "You should rather consider using color table expansion (-expand option in gdal_translate)");
    }

    int nBlockXSize = nWidth;
    int nBlockYSize = nHeight;

    const bool bTiled = CPLFetchBool( papszOptions, "TILED", false );
    if( bTiled )
    {
        nBlockXSize = 256;
        nBlockYSize = 256;
    }

    const char* pszValue = CSLFetchNameValue(papszOptions, "BLOCKXSIZE");
    if( pszValue != nullptr )
    {
        nBlockXSize = atoi( pszValue );
        if (nBlockXSize <= 0 || nBlockXSize >= nWidth)
            nBlockXSize = nWidth;
    }

    pszValue = CSLFetchNameValue(papszOptions, "BLOCKYSIZE");
    if( pszValue != nullptr )
    {
        nBlockYSize = atoi( pszValue );
        if (nBlockYSize <= 0 || nBlockYSize >= nHeight)
            nBlockYSize = nHeight;
    }

    int nJPEGQuality = GDALPDFGetJPEGQuality(papszOptions);

    const char* pszJPEG2000_DRIVER = CSLFetchNameValue(papszOptions, "JPEG2000_DRIVER");

    const char* pszGEO_ENCODING =
        CSLFetchNameValueDef(papszOptions, "GEO_ENCODING", "ISO32000");

    const char* pszXMP = CSLFetchNameValue(papszOptions, "XMP");

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

    const char* pszDPI = CSLFetchNameValue(papszOptions, "DPI");
    double dfDPI = DEFAULT_DPI;
    if( pszDPI != nullptr )
        dfDPI = CPLAtof(pszDPI);

    const char* pszWriteUserUnit = CSLFetchNameValue(papszOptions, "WRITE_USERUNIT");
    bool bWriteUserUnit;
    if( pszWriteUserUnit != nullptr )
        bWriteUserUnit = CPLTestBool( pszWriteUserUnit );
    else
        bWriteUserUnit = ( pszDPI == nullptr );

    double dfUserUnit = dfDPI * USER_UNIT_IN_INCH;
    double dfWidthInUserUnit = nWidth / dfUserUnit + sMargins.nLeft + sMargins.nRight;
    double dfHeightInUserUnit = nHeight / dfUserUnit + sMargins.nBottom + sMargins.nTop;
    if( dfWidthInUserUnit > MAXIMUM_SIZE_IN_UNITS ||
        dfHeightInUserUnit > MAXIMUM_SIZE_IN_UNITS )
    {
        if( pszDPI == nullptr )
        {
            if( sMargins.nLeft + sMargins.nRight >= MAXIMUM_SIZE_IN_UNITS ||
                sMargins.nBottom + sMargins.nTop >= MAXIMUM_SIZE_IN_UNITS )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Margins too big compared to maximum page dimension (%d) "
                         "in user units allowed by Acrobat",
                         MAXIMUM_SIZE_IN_UNITS);
            }
            else
            {
                if( dfWidthInUserUnit >= dfHeightInUserUnit )
                {
                    dfDPI = ceil((double)nWidth / (MAXIMUM_SIZE_IN_UNITS -
                            (sMargins.nLeft + sMargins.nRight)) / USER_UNIT_IN_INCH);
                }
                else
                {
                    dfDPI = ceil((double)nHeight / (MAXIMUM_SIZE_IN_UNITS -
                            (sMargins.nBottom + sMargins.nTop)) / USER_UNIT_IN_INCH);
                }
                CPLDebug("PDF", "Adjusting DPI to %d so that page dimension in "
                        "user units remain in what is accepted by Acrobat", (int)dfDPI);
            }
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "The page dimension in user units is %d x %d whereas the "
                     "maximum allowed by Acrobat is %d x %d",
                     (int)(dfWidthInUserUnit + 0.5),
                     (int)(dfHeightInUserUnit + 0.5),
                     MAXIMUM_SIZE_IN_UNITS, MAXIMUM_SIZE_IN_UNITS);
        }
    }

    if (dfDPI < DEFAULT_DPI)
        dfDPI = DEFAULT_DPI;

    const char* pszClippingExtent = CSLFetchNameValue(papszOptions, "CLIPPING_EXTENT");
    int bUseClippingExtent = FALSE;
    double adfClippingExtent[4] = { 0.0, 0.0, 0.0, 0.0 };
    if( pszClippingExtent != nullptr )
    {
        char** papszTokens = CSLTokenizeString2(pszClippingExtent, ",", 0);
        if( CSLCount(papszTokens) == 4 )
        {
            bUseClippingExtent = TRUE;
            adfClippingExtent[0] = CPLAtof(papszTokens[0]);
            adfClippingExtent[1] = CPLAtof(papszTokens[1]);
            adfClippingExtent[2] = CPLAtof(papszTokens[2]);
            adfClippingExtent[3] = CPLAtof(papszTokens[3]);
            if( adfClippingExtent[0] > adfClippingExtent[2] ||
                adfClippingExtent[1] > adfClippingExtent[3] )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Invalid value for CLIPPING_EXTENT. Should be xmin,ymin,xmax,ymax");
                bUseClippingExtent = FALSE;
            }

            if( bUseClippingExtent )
            {
                double adfGeoTransform[6];
                if( poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None )
                {
                    if( adfGeoTransform[2] != 0.0 || adfGeoTransform[4] != 0.0 )
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                "Cannot use CLIPPING_EXTENT because main raster has a rotated geotransform");
                        bUseClippingExtent = FALSE;
                    }
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                                "Cannot use CLIPPING_EXTENT because main raster has no geotransform");
                    bUseClippingExtent = FALSE;
                }
            }
        }
        CSLDestroy(papszTokens);
    }

    const char* pszLayerName = CSLFetchNameValue(papszOptions, "LAYER_NAME");

    const char* pszExtraImages = CSLFetchNameValue(papszOptions, "EXTRA_IMAGES");
    const char* pszExtraStream = CSLFetchNameValue(papszOptions, "EXTRA_STREAM");
    const char* pszExtraLayerName = CSLFetchNameValue(papszOptions, "EXTRA_LAYER_NAME");

    const char* pszOGRDataSource = CSLFetchNameValue(papszOptions, "OGR_DATASOURCE");
    const char* pszOGRDisplayField = CSLFetchNameValue(papszOptions, "OGR_DISPLAY_FIELD");
    const char* pszOGRDisplayLayerNames = CSLFetchNameValue(papszOptions, "OGR_DISPLAY_LAYER_NAMES");
    const char* pszOGRLinkField = CSLFetchNameValue(papszOptions, "OGR_LINK_FIELD");
    const bool bWriteOGRAttributes =
        CPLFetchBool(papszOptions, "OGR_WRITE_ATTRIBUTES", true);

    const char* pszExtraRasters = CSLFetchNameValue(papszOptions, "EXTRA_RASTERS");
    const char* pszExtraRastersLayerName = CSLFetchNameValue(papszOptions, "EXTRA_RASTERS_LAYER_NAME");

    const char* pszOffLayers = CSLFetchNameValue(papszOptions, "OFF_LAYERS");
    const char* pszExclusiveLayers = CSLFetchNameValue(papszOptions, "EXCLUSIVE_LAYERS");

    const char* pszJavascript = CSLFetchNameValue(papszOptions, "JAVASCRIPT");
    const char* pszJavascriptFile = CSLFetchNameValue(papszOptions, "JAVASCRIPT_FILE");

/* -------------------------------------------------------------------- */
/*      Create file.                                                    */
/* -------------------------------------------------------------------- */
    VSILFILE* fp = VSIFOpenL(pszFilename, "wb");
    if( fp == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to create PDF file %s.\n",
                  pszFilename );
        return nullptr;
    }

    GDALPDFWriter oWriter(fp);

    GDALDataset* poClippingDS = poSrcDS;
    if( bUseClippingExtent )
        poClippingDS = new GDALPDFClippingDataset(poSrcDS, adfClippingExtent);

    if( CPLFetchBool(papszOptions, "WRITE_INFO", true) )
        oWriter.SetInfo(poSrcDS, papszOptions);
    oWriter.SetXMP(poClippingDS, pszXMP);

    oWriter.StartPage(poClippingDS,
                      dfDPI,
                      bWriteUserUnit,
                      pszGEO_ENCODING,
                      pszNEATLINE,
                      &sMargins,
                      eStreamCompressMethod,
                      pszOGRDataSource != nullptr && bWriteOGRAttributes);

    int bRet;

    if( !bUseClippingExtent )
    {
        bRet = oWriter.WriteImagery(poSrcDS,
                                    pszLayerName,
                                    eCompressMethod,
                                    nPredictor,
                                    nJPEGQuality,
                                    pszJPEG2000_DRIVER,
                                    nBlockXSize, nBlockYSize,
                                    pfnProgress, pProgressData);
    }
    else
    {
        bRet = oWriter.WriteClippedImagery(poSrcDS,
                                           pszLayerName,
                                           eCompressMethod,
                                           nPredictor,
                                           nJPEGQuality,
                                           pszJPEG2000_DRIVER,
                                           nBlockXSize, nBlockYSize,
                                           pfnProgress, pProgressData);
    }

    char** papszExtraRasters = CSLTokenizeString2(
        pszExtraRasters ? pszExtraRasters : "", ",", 0);
    char** papszExtraRastersLayerName = CSLTokenizeString2(
        pszExtraRastersLayerName ? pszExtraRastersLayerName : "", ",", 0);
    int bUseExtraRastersLayerName = (CSLCount(papszExtraRasters) ==
                                     CSLCount(papszExtraRastersLayerName));
    int bUseExtraRasters = TRUE;

    const char* pszClippingProjectionRef = poSrcDS->GetProjectionRef();
    if( CSLCount(papszExtraRasters) != 0 )
    {
        double adfGeoTransform[6];
        if( poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None )
        {
            if( adfGeoTransform[2] != 0.0 || adfGeoTransform[4] != 0.0 )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Cannot use EXTRA_RASTERS because main raster has a rotated geotransform");
                bUseExtraRasters = FALSE;
            }
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                         "Cannot use EXTRA_RASTERS because main raster has no geotransform");
            bUseExtraRasters = FALSE;
        }
        if( bUseExtraRasters &&
            (pszClippingProjectionRef == nullptr ||
             pszClippingProjectionRef[0] == '\0') )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Cannot use EXTRA_RASTERS because main raster has no projection");
            bUseExtraRasters = FALSE;
        }
    }

    for(int i=0; bRet && bUseExtraRasters && papszExtraRasters[i] != nullptr; i++)
    {
        GDALDataset* poDS = (GDALDataset*)GDALOpen(papszExtraRasters[i], GA_ReadOnly);
        if( poDS != nullptr )
        {
            double adfGeoTransform[6];
            int bUseRaster = TRUE;
            if( poDS->GetGeoTransform(adfGeoTransform) == CE_None )
            {
                if( adfGeoTransform[2] != 0.0 || adfGeoTransform[4] != 0.0 )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                            "Cannot use %s because it has a rotated geotransform",
                             papszExtraRasters[i]);
                    bUseRaster = FALSE;
                }
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                            "Cannot use %s because it has no geotransform",
                         papszExtraRasters[i]);
                bUseRaster = FALSE;
            }
            const char* pszProjectionRef = poDS->GetProjectionRef();
            if( bUseRaster &&
                (pszProjectionRef == nullptr || pszProjectionRef[0] == '\0')  )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Cannot use %s because it has no projection",
                         papszExtraRasters[i]);
                bUseRaster = FALSE;
            }
            if( bUseRaster )
            {
                if( pszClippingProjectionRef != nullptr &&
                    pszProjectionRef != nullptr &&
                    !EQUAL(pszClippingProjectionRef, pszProjectionRef) )
                {
                    OGRSpatialReferenceH hClippingSRS =
                        OSRNewSpatialReference(pszClippingProjectionRef);
                    OGRSpatialReferenceH hSRS =
                        OSRNewSpatialReference(pszProjectionRef);
                    if (!OSRIsSame(hClippingSRS, hSRS))
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                "Cannot use %s because it has a different projection than main dataset",
                                papszExtraRasters[i]);
                        bUseRaster = FALSE;
                    }
                    OSRDestroySpatialReference(hClippingSRS);
                    OSRDestroySpatialReference(hSRS);
                }
            }
            if( bUseRaster )
            {
                bRet = oWriter.WriteClippedImagery(poDS,
                                    bUseExtraRastersLayerName ?
                                        papszExtraRastersLayerName[i] : nullptr,
                                    eCompressMethod,
                                    nPredictor,
                                    nJPEGQuality,
                                    pszJPEG2000_DRIVER,
                                    nBlockXSize, nBlockYSize,
                                    nullptr, nullptr);
            }

            GDALClose(poDS);
        }
    }

    CSLDestroy(papszExtraRasters);
    CSLDestroy(papszExtraRastersLayerName);

    if (bRet && pszOGRDataSource != nullptr)
        oWriter.WriteOGRDataSource(pszOGRDataSource,
                                   pszOGRDisplayField,
                                   pszOGRDisplayLayerNames,
                                   pszOGRLinkField,
                                   bWriteOGRAttributes);

    if (bRet)
        oWriter.EndPage(pszExtraImages,
                        pszExtraStream,
                        pszExtraLayerName,
                        pszOffLayers,
                        pszExclusiveLayers);

    if (pszJavascript)
        oWriter.WriteJavascript(pszJavascript);
    else if (pszJavascriptFile)
        oWriter.WriteJavascriptFile(pszJavascriptFile);

    oWriter.Close();

    if (poClippingDS != poSrcDS)
        delete poClippingDS;

    if (!bRet)
    {
        VSIUnlink(pszFilename);
        return nullptr;
    }
    else
    {
#ifdef HAVE_PDF_READ_SUPPORT
        GDALDataset* poDS = GDALPDFOpen(pszFilename, GA_ReadOnly);
        if( poDS == nullptr )
            return nullptr;
        char** papszMD = CSLDuplicate( poSrcDS->GetMetadata() );
        papszMD = CSLMerge( papszMD, poDS->GetMetadata() );
        const char* pszAOP = CSLFetchNameValue(papszMD, GDALMD_AREA_OR_POINT);
        if( pszAOP != nullptr && EQUAL(pszAOP, GDALMD_AOP_AREA) )
            papszMD = CSLSetNameValue(papszMD, GDALMD_AREA_OR_POINT, nullptr);
        poDS->SetMetadata( papszMD );
        if( EQUAL(pszGEO_ENCODING, "NONE") )
        {
            double adfGeoTransform[6];
            if( poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None )
            {
                poDS->SetGeoTransform( adfGeoTransform );
            }
            const char* pszProjectionRef = poSrcDS->GetProjectionRef();
            if( pszProjectionRef != nullptr && pszProjectionRef[0] != '\0' )
            {
                poDS->SetProjection( pszProjectionRef );
            }
        }
        CSLDestroy(papszMD);
        return poDS;
#else
        return new GDALFakePDFDataset();
#endif
    }
}
