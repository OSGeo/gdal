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
#include "ogr_spatialref.h"
#include <vector>

CPL_CVSID("$Id$");

typedef enum
{
    COMPRESS_NONE,
    COMPRESS_DEFLATE,
    COMPRESS_JPEG,
    COMPRESS_JPEG2000
} PDFCompressMethod;

#define PIXEL_TO_GEO_X(x,y) adfGeoTransform[0] + x * adfGeoTransform[1] + y * adfGeoTransform[2]
#define PIXEL_TO_GEO_Y(x,y) adfGeoTransform[3] + x * adfGeoTransform[4] + y * adfGeoTransform[5]

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
    int nXMPId;

    void    StartObj(int nObjectId);
    void    EndObj();
    void    WriteXRefTableAndTrailer();
    void    WritePages();
    int     WriteSRS_ISO32000(GDALDataset* poSrcDS);
    int     WriteSRS_OGC_BP(GDALDataset* poSrcDS);
    int     WriteBlock( GDALDataset* poSrcDS,
                        int nXOff, int nYOff, int nReqXSize, int nReqYSize,
                        PDFCompressMethod eCompressMethod,
                        int nJPEGQuality,
                        const char* pszJPEG2000_DRIVER,
                        GDALProgressFunc pfnProgress,
                        void * pProgressData );
    int     WriteMask(GDALDataset* poSrcDS,
                      int nXOff, int nYOff, int nReqXSize, int nReqYSize,
                      PDFCompressMethod eCompressMethod);

    public:
        GDALPDFWriter(VSILFILE* fpIn);
       ~GDALPDFWriter();

       void Close();

       int  AllocNewObject();
       int  WritePage(GDALDataset* poSrcDS,
                      const char* pszGEO_ENCODING,
                      PDFCompressMethod eCompressMethod,
                      int nJPEGQuality,
                      const char* pszJPEG2000_DRIVER,
                      int nBlockXSize, int nBlockYSize,
                      GDALProgressFunc pfnProgress,
                      void * pProgressData);
       void SetInfo(GDALDataset* poSrcDS,
                    char** papszOptions);
       void SetXMP(GDALDataset* poSrcDS,
                   const char* pszXMP);
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
    nXMPId = 0;
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
                (int)asOffsets.size() + 1,
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
/*                         WriteSRS_ISO32000()                          */
/************************************************************************/

int  GDALPDFWriter::WriteSRS_ISO32000(GDALDataset* poSrcDS)
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
                "  /GPTS [%.16g %.16g %.16g %.16g %.16g %.16g %.16g %.16g]\n"
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
/*                         GDALPDFGeoCoordToNL()                        */
/************************************************************************/

static int GDALPDFGeoCoordToNL(double adfGeoTransform[6], int nHeight,
                               double dfPixelPerPt,
                               double X, double Y,
                               double* padfNLX, double* padfNLY)
{
    double adfGeoTransformInv[6];
    GDALInvGeoTransform(adfGeoTransform, adfGeoTransformInv);
    double x = adfGeoTransformInv[0] + X * adfGeoTransformInv[1] + Y * adfGeoTransformInv[2];
    double y = adfGeoTransformInv[3] + X * adfGeoTransformInv[4] + Y * adfGeoTransformInv[5];
    *padfNLX = x / dfPixelPerPt;
    *padfNLY = nHeight - y / dfPixelPerPt;
    return TRUE;
}
/************************************************************************/
/*                           WriteSRS_OGC_BP()                          */
/************************************************************************/

int GDALPDFWriter::WriteSRS_OGC_BP(GDALDataset* poSrcDS)
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
    double dfPixelPerPt = 1;
    double dfX1 = 0;
    double dfY2 = nHeight;

    adfCTM[0] = adfGeoTransform[1] * dfPixelPerPt;
    adfCTM[1] = adfGeoTransform[2] * dfPixelPerPt;
    adfCTM[2] = - adfGeoTransform[4] * dfPixelPerPt;
    adfCTM[3] = - adfGeoTransform[5] * dfPixelPerPt;
    adfCTM[4] = adfGeoTransform[0] - (adfCTM[0] * dfX1 + adfCTM[2] * dfY2);
    adfCTM[5] = adfGeoTransform[3] - (adfCTM[1] * dfX1 + adfCTM[3] * dfY2);
    
    const OGR_SRSNode* poDatumNode = poSRS->GetAttrNode("DATUM");
    const char* pszDatumDescription = NULL;
    if (poDatumNode && poDatumNode->GetChildCount() > 0)
        pszDatumDescription = poDatumNode->GetChild(0)->GetValue();

    const char* pszDatumOGCBP = "(WGE)";
    CPLString osDatum;
    if (pszDatumDescription)
    {
        double dfSemiMajor = poSRS->GetSemiMajor();
        double dfInvFlattening = poSRS->GetInvFlattening();
        int nEPSGDatum = -1;
        const char *pszAuthority = poSRS->GetAuthorityName( "DATUM" );
        if( pszAuthority != NULL && EQUAL(pszAuthority,"EPSG") )
            nEPSGDatum = atoi(poSRS->GetAuthorityCode( "DATUM" ));

        if( EQUAL(pszDatumDescription,SRS_DN_WGS84) || nEPSGDatum == 6326 )
            pszDatumOGCBP = "(WGE)";
        else if( EQUAL(pszDatumDescription, SRS_DN_NAD27) || nEPSGDatum == 6267 )
            pszDatumOGCBP = "(NAS)";
        else if( EQUAL(pszDatumDescription, SRS_DN_NAD83) || nEPSGDatum == 6269 )
            pszDatumOGCBP = "(NAR)";
        else
        {
            CPLDebug("PDF",
                     "Unhandled datum name (%s). Write datum parameters then.",
                     pszDatumDescription);

            const OGR_SRSNode* poSpheroidNode = poSRS->GetAttrNode("SPHEROID");
            if (poSpheroidNode && poSpheroidNode->GetChildCount() >= 3)
            {
                osDatum.Printf("<<\n"
                               "         /Description (%s)\n",
                               pszDatumDescription);

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
                    osDatum += CPLString().Printf(
                        "         /Ellipsoid (%s)\n",
                        pszEllipsoidCode);
                }
                else
                {
                    const char* pszEllipsoidDescription =
                        poSpheroidNode->GetChild(0)->GetValue();

                    CPLDebug("PDF",
                         "Unhandled ellipsoid name (%s). Write ellipsoid parameters then.",
                         pszEllipsoidDescription);

                    osDatum += CPLString().Printf(
                                   "         /Ellipsoid <<\n"
                                   "            /Description (%s)\n"
                                   "            /SemiMajorAxis (%.16g)\n"
                                   "            /InvFlattening (%.16g)\n"
                                   "         >>\n",
                                   pszEllipsoidDescription,
                                   dfSemiMajor,
                                   dfInvFlattening);
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
                    osDatum += CPLString().Printf(
                        "         /ToWGS84 <<\n"
                        "           /dx (%s)\n"
                        "           /dy (%s)\n"
                        "           /dz (%s)\n"
                        "         >>\n",
                        poTOWGS84->GetChild(0)->GetValue(),
                        poTOWGS84->GetChild(1)->GetValue(),
                        poTOWGS84->GetChild(2)->GetValue());
                }
                else if( poTOWGS84 != NULL && poTOWGS84->GetChildCount() >= 7)
                {
                    osDatum += CPLString().Printf(
                        "         /ToWGS84 <<\n"
                        "           /dx (%s)\n"
                        "           /dy (%s)\n"
                        "           /dz (%s)\n"
                        "           /rx (%s)\n"
                        "           /ry (%s)\n"
                        "           /rz (%s)\n"
                        "           /sf (%s)\n"
                        "         >>\n",
                        poTOWGS84->GetChild(0)->GetValue(),
                        poTOWGS84->GetChild(1)->GetValue(),
                        poTOWGS84->GetChild(2)->GetValue(),
                        poTOWGS84->GetChild(3)->GetValue(),
                        poTOWGS84->GetChild(4)->GetValue(),
                        poTOWGS84->GetChild(5)->GetValue(),
                        poTOWGS84->GetChild(6)->GetValue());
                }

                osDatum += "      >>";
                pszDatumOGCBP = osDatum.c_str();
            }
        }
    }
    else
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "No datum name. Defaulting to WGS84.");
    }

    const OGR_SRSNode* poNode = poSRS->GetRoot();
    if( poNode != NULL )
        poNode = poNode->GetChild(0);
    const char* pszDescription = NULL;
    if( poNode != NULL )
        pszDescription = poNode->GetValue();

    const char* pszProjectionOGCBP = "GEOGRAPHIC";
    const char *pszProjection = OSRGetAttrValue(hSRS, "PROJECTION", 0);

    CPLString osProjParams;

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
            osProjParams += CPLSPrintf("     /Hemisphere (%s)\n", (bNorth) ? "N" : "S");
            osProjParams += CPLSPrintf("     /Zone %d\n", nZone);
        }
        else
        {
            double dfCenterLat = poSRS->GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,90.L);
            double dfCenterLong = poSRS->GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
            double dfScale = poSRS->GetNormProjParm(SRS_PP_SCALE_FACTOR,1.0);
            double dfFalseEasting = poSRS->GetNormProjParm(SRS_PP_FALSE_EASTING,0.0);
            double dfFalseNorthing = poSRS->GetNormProjParm(SRS_PP_FALSE_NORTHING,0.0);

            pszProjectionOGCBP = "TC";
            osProjParams += CPLSPrintf("     /OriginLatitude (%.16g)\n", dfCenterLat);
            osProjParams += CPLSPrintf("     /CentralMeridian (%.16g)\n", dfCenterLong);
            osProjParams += CPLSPrintf("     /ScaleFactor (%.16g)\n", dfScale);
            osProjParams += CPLSPrintf("     /FalseEasting (%.16g)\n", dfFalseEasting);
            osProjParams += CPLSPrintf("     /FalseNorthing (%.16g)\n", dfFalseNorthing);
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
            osProjParams += CPLSPrintf("     /Hemisphere (%s)\n", (dfCenterLat > 0) ? "N" : "S");
        }
        else
        {
            pszProjectionOGCBP = "PG";
            osProjParams += CPLSPrintf("     /LatitudeTrueScale (%.16g)\n", dfCenterLat);
            osProjParams += CPLSPrintf("     /LongitudeDownFromPole (%.16g)\n", dfCenterLong);
            osProjParams += CPLSPrintf("     /ScaleFactor (%.16g)\n", dfScale);
            osProjParams += CPLSPrintf("     /FalseEasting (%.16g)\n", dfFalseEasting);
            osProjParams += CPLSPrintf("     /FalseNorthing (%.16g)\n", dfFalseNorthing);
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
        osProjParams += CPLSPrintf("     /StandardParallelOne (%.16g)\n", dfStdP1);
        osProjParams += CPLSPrintf("     /StandardParallelTwo (%.16g)\n", dfStdP2);
        osProjParams += CPLSPrintf("     /OriginLatitude (%.16g)\n", dfCenterLat);
        osProjParams += CPLSPrintf("     /CentralMeridian (%.16g)\n", dfCenterLong);
        osProjParams += CPLSPrintf("     /FalseEasting (%.16g)\n", dfFalseEasting);
        osProjParams += CPLSPrintf("     /FalseNorthing (%.16g)\n", dfFalseNorthing);
    }
    else
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Unhandled projection type (%s) for now", pszProjection);
    }

    double adfNL[8];
    GDALPDFGeoCoordToNL(adfGeoTransform, nHeight, dfPixelPerPt,
                        PIXEL_TO_GEO_X(0, 0), PIXEL_TO_GEO_Y(0, 0),
                        adfNL + 0, adfNL + 1);
    GDALPDFGeoCoordToNL(adfGeoTransform, nHeight, dfPixelPerPt,
                        PIXEL_TO_GEO_X(0, nHeight), PIXEL_TO_GEO_Y(0, nHeight),
                        adfNL + 2, adfNL + 3);
    GDALPDFGeoCoordToNL(adfGeoTransform, nHeight, dfPixelPerPt,
                        PIXEL_TO_GEO_X(nWidth, nHeight), PIXEL_TO_GEO_Y(nWidth, nHeight),
                        adfNL + 4, adfNL + 5);
    GDALPDFGeoCoordToNL(adfGeoTransform, nHeight, dfPixelPerPt,
                        PIXEL_TO_GEO_X(nWidth, 0), PIXEL_TO_GEO_Y(nWidth, 0),
                        adfNL + 6, adfNL + 7);

    int nLGIDictId = AllocNewObject();
    StartObj(nLGIDictId);
    VSIFPrintfL(fp,
                "<< /Type /LGIDict\n"
                "   /Version (2.1)\n"
                "   /CTM [ %.16g %.16g %.16g %.16g %.16g %.16g ]\n"
                "   /Neatline [ %.16g %.16g %.16g %.16g %.16g %.16g %.16g %.16g ]\n",
                adfCTM[0], adfCTM[1], adfCTM[2], adfCTM[3], adfCTM[4], adfCTM[5],
                adfNL[0],adfNL[1],adfNL[2],adfNL[3],adfNL[4],adfNL[5],adfNL[6],adfNL[7]);
    if( pszDescription )
    {
        VSIFPrintfL(fp,
                    "   /Description (%s)\n",
                    pszDescription);
    }
    VSIFPrintfL(fp,
                "   /Projection <<\n"
                "     /Type /Projection\n"
                "     /Datum %s\n"
                "     /ProjectionType (%s)\n"
                "%s",
                pszDatumOGCBP,
                pszProjectionOGCBP,
                osProjParams.c_str()
                );

    if( poSRS->IsProjected() )
    {
        char* pszUnitName = NULL;
        double dfLinearUnits = poSRS->GetLinearUnits(&pszUnitName);
        if (dfLinearUnits == 1.0)
            VSIFPrintfL(fp, "     /Units (M)\n");
        else if (dfLinearUnits == 0.3048)
            VSIFPrintfL(fp, "     /Units (FT)\n");
    }

    /* GDAL extension */
    if( CSLTestBoolean( CPLGetConfigOption("GDAL_PDF_OGC_BP_WRITE_WKT", "TRUE") ) )
        VSIFPrintfL(fp, "     /WKT (%s) %%This attribute is a GDAL extension\n", pszWKT);

    VSIFPrintfL(fp, "  >>\n");

    VSIFPrintfL(fp, ">>\n");

    EndObj();

    OSRDestroySpatialReference(hSRS);
    
    return nLGIDictId;
}

/************************************************************************/
/*                             SetInfo()                                */
/************************************************************************/

static const char* GDALPDFGetValueFromDSOrOption(GDALDataset* poSrcDS, char** papszOptions,
                            const char* pszKey)
{
    const char* pszValue = CSLFetchNameValue(papszOptions, pszKey);
    if (pszValue != NULL && pszValue[0] == '\0')
        return NULL;
    if (pszValue != NULL)
        return pszValue;
    return poSrcDS->GetMetadataItem(pszKey);
}

static CPLString GDALPDFGetUTF8String(const char* pszStr)
{
    GByte* pabyData = (GByte*)pszStr;
    int i;
    GByte ch;
    for(i=0;(ch = pabyData[i]) != '\0';i++)
    {
        if (ch < 32 || ch > 127 ||
            ch == '(' || ch == ')' || ch == '<' || ch == '>' ||
            ch == '[' || ch == ']' || ch == '{' || ch == '}' ||
            ch == '/' || ch == '%' || ch == '#')
            break;
    }
    CPLString osStr;
    if (ch == 0)
    {
        osStr = "(";
        osStr += pszStr;
        osStr += ")";
        return osStr;
    }

    wchar_t* pwszDest = CPLRecodeToWChar( pszStr, CPL_ENC_UTF8, CPL_ENC_UCS2 );
    osStr = "<FEFF";
    for(i=0;pwszDest[i] != 0;i++)
    {
        osStr += CPLSPrintf("%02X", (pwszDest[i] >> 8) & 0xff);
        osStr += CPLSPrintf("%02X", (pwszDest[i]) & 0xff);
    }
    osStr += ">";
    CPLFree(pwszDest);
    return osStr;
}

void GDALPDFWriter::SetInfo(GDALDataset* poSrcDS,
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
        return;

    CPLAssert(nInfoId == 0);
    nInfoId = AllocNewObject();
    StartObj(nInfoId);
    VSIFPrintfL(fp, "<<\n");
    if (pszAUTHOR != NULL)
        VSIFPrintfL(fp, "  /Author %s\n", GDALPDFGetUTF8String(pszAUTHOR).c_str());
    if (pszPRODUCER != NULL)
        VSIFPrintfL(fp, "  /Producer %s\n", GDALPDFGetUTF8String(pszPRODUCER).c_str());
    if (pszCREATOR != NULL)
        VSIFPrintfL(fp, "  /Creator %s\n", GDALPDFGetUTF8String(pszCREATOR).c_str());
    if (pszCREATION_DATE != NULL)
        VSIFPrintfL(fp, "  /CreationDate %s\n", GDALPDFGetUTF8String(pszCREATION_DATE).c_str());
    if (pszSUBJECT != NULL)
        VSIFPrintfL(fp, "  /Subject %s\n",GDALPDFGetUTF8String(pszSUBJECT).c_str());
    if (pszTITLE != NULL)
        VSIFPrintfL(fp, "  /Title %s\n", GDALPDFGetUTF8String(pszTITLE).c_str());
    if (pszKEYWORDS != NULL)
        VSIFPrintfL(fp, "  /Keywords %s\n", GDALPDFGetUTF8String(pszKEYWORDS).c_str());
    VSIFPrintfL(fp, ">>\n");

    EndObj();
}

/************************************************************************/
/*                             SetInfo()                                */
/************************************************************************/

void GDALPDFWriter::SetXMP(GDALDataset* poSrcDS,
                           const char* pszXMP)
{
    if (pszXMP != NULL && EQUALN(pszXMP, "NO", 2))
        return;
    if (pszXMP != NULL && pszXMP[0] == '\0')
        return;

    char** papszXMP = poSrcDS->GetMetadata("xml:XMP");
    if (pszXMP == NULL && papszXMP != NULL && papszXMP[0] != NULL)
        pszXMP = papszXMP[0];

    if (pszXMP == NULL)
        return;

    CPLXMLNode* psNode = CPLParseXMLString(pszXMP);
    if (psNode == NULL)
        return;
    CPLDestroyXMLNode(psNode);

    CPLAssert(nXMPId == 0);
    nXMPId = AllocNewObject();
    StartObj(nXMPId);
#if 1
    VSIFPrintfL(fp,
                "<<\n"
                "/Type /Metadata\n"
                "/Subtype /XML\n"
                "/Length %d\n"
                ">>\n", (int)strlen(pszXMP));
    VSIFPrintfL(fp, "stream\n");
    VSIFPrintfL(fp, "%s\n", pszXMP);
    VSIFPrintfL(fp, "endstream\n");
    EndObj();
#else
    int nLengthId = AllocNewObject();
    VSIFPrintfL(fp,
                "<<\n"
                "/Type /Metadata\n"
                "/Filter /FlateDecode\n"
                "/Subtype /XML\n"
                "/Length %d 0 R\n"
                ">>\n", nLengthId );
    VSIFPrintfL(fp, "stream\n");

    vsi_l_offset nStreamStart = VSIFTellL(fp);

    VSILFILE* fpBack = fp;
    VSILFILE* fpGZip = (VSILFILE* )VSICreateGZipWritable( (VSIVirtualHandle*) fp, TRUE, FALSE );
    fp = fpGZip;

    VSIFWriteL(pszXMP, strlen(pszXMP), 1, fp);

    VSIFCloseL(fpGZip);

    fp = fpBack;

    vsi_l_offset nStreamEnd = VSIFTellL(fp);
    VSIFPrintfL(fp,
                "\n"
                "endstream\n");
    EndObj();

    StartObj(nLengthId);
    VSIFPrintfL(fp,
                "   %ld\n",
                (long)(nStreamEnd - nStreamStart));
    EndObj();
#endif
}

/************************************************************************/
/*                              WritePage()                             */
/************************************************************************/

int GDALPDFWriter::WritePage(GDALDataset* poSrcDS,
                             const char* pszGEO_ENCODING,
                             PDFCompressMethod eCompressMethod,
                             int nJPEGQuality,
                             const char* pszJPEG2000_DRIVER,
                             int nBlockXSize, int nBlockYSize,
                             GDALProgressFunc pfnProgress,
                             void * pProgressData)
{
    int  nWidth = poSrcDS->GetRasterXSize();
    int  nHeight = poSrcDS->GetRasterYSize();
    int  nBands = poSrcDS->GetRasterCount();

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
        nViewportId = WriteSRS_ISO32000(poSrcDS);
        
    int nLGIDictId = 0;
    if( bOGC_BP )
        nLGIDictId = WriteSRS_OGC_BP(poSrcDS);

    StartObj(nPageId);
    VSIFPrintfL(fp,
                "<< /Type /Page\n"
                "   /Parent %d 0 R\n"
                "   /MediaBox [ 0 0 %d %d ]\n"
                "   /Contents %d 0 R\n"
                "   /Resources %d 0 R\n",
                nPageResourceId,
                nWidth, nHeight,
                nContentId,
                nResourcesId);
    if (nBands == 4)
    {
        VSIFPrintfL(fp,
                    "   /Group <<\n"
                    "      /Type /Group\n"
                    "      /S /Transparency\n"
                    "      /CS /DeviceRGB\n"
                    "   >>\n");
    }
    if (nViewportId)
    {
        VSIFPrintfL(fp,
                    "   /VP [ %d 0 R ]\n",
                    nViewportId);
    }
    if (nLGIDictId)
    {
        VSIFPrintfL(fp, "   /LGIDict %d 0 R \n", nLGIDictId);
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
    VSIFPrintfL(fp, "<< /Length %d 0 R\n",
                nMaskLengthId);
    VSIFPrintfL(fp,
                "   /Type /XObject\n");
    if( eCompressMethod != COMPRESS_NONE )
    {
        VSIFPrintfL(fp, "   /Filter /FlateDecode\n");
    }
    VSIFPrintfL(fp,
                "   /Subtype /Image\n"
                "   /Width %d\n"
                "   /Height %d\n"
                "   /ColorSpace /DeviceGray\n"
                "   /BitsPerComponent %d\n",
                nReqXSize, nReqYSize,
                (bOnly0or255) ? 1 : 8);
    VSIFPrintfL(fp, ">>\n"
                "stream\n");
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
                             PDFCompressMethod eCompressMethod,
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
    if( nMaskId )
    {
        VSIFPrintfL(fp, "  /SMask %d 0 R\n", nMaskId);
    }
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
                 "   /Pages %d 0 R\n",
                nPageResourceId);
    if (nXMPId)
        VSIFPrintfL(fp,"   /Metadata %d 0 R\n", nXMPId);
    VSIFPrintfL(fp, ">>\n");
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
    
    const char* pszGEO_ENCODING =
        CSLFetchNameValueDef(papszOptions, "GEO_ENCODING", "ISO32000");

    const char* pszXMP = CSLFetchNameValue(papszOptions, "XMP");

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
                                 pszGEO_ENCODING,
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
