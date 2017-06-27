/******************************************************************************
 *
 * Project:  ECRG TOC read Translator
 * Purpose:  Implementation of ECRGTOCDataset and ECRGTOCSubDataset.
 * Author:   Even Rouault, even.rouault at mines-paris.org
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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

// g++ -g -Wall -fPIC frmts/nitf/ecrgtocdataset.cpp -shared -o gdal_ECRGTOC.so -Iport -Igcore -Iogr -Ifrmts/vrt -L. -lgdal

#include "cpl_port.h"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "gdal.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "gdal_proxy.h"
#include "ogr_srs_api.h"
#include "vrtdataset.h"

CPL_CVSID("$Id$")

/** Overview of used classes :
   - ECRGTOCDataset : lists the different subdatasets, listed in the .xml,
                      as subdatasets
   - ECRGTOCSubDataset : one of these subdatasets, implemented as a VRT, of
                         the relevant NITF tiles
   - ECRGTOCProxyRasterDataSet : a "proxy" dataset that maps to a NITF tile
*/

typedef struct
{
    const char* pszName;
    const char* pszPath;
    int         nScale;
    int         nZone;
} FrameDesc;

/************************************************************************/
/* ==================================================================== */
/*                            ECRGTOCDataset                            */
/* ==================================================================== */
/************************************************************************/

class ECRGTOCDataset : public GDALPamDataset
{
  char      **papszSubDatasets;
  double      adfGeoTransform[6];

  char      **papszFileList;

  public:
    ECRGTOCDataset()
    {
        papszSubDatasets = NULL;
        papszFileList = NULL;
        memset( adfGeoTransform, 0, sizeof(adfGeoTransform) );
    }

    virtual ~ECRGTOCDataset()
    {
        CSLDestroy( papszSubDatasets );
        CSLDestroy(papszFileList);
    }

    virtual char      **GetMetadata( const char * pszDomain = "" ) override;

    virtual char      **GetFileList() override { return CSLDuplicate(papszFileList); }

    void                AddSubDataset(const char* pszFilename,
                                      const char* pszProductTitle,
                                      const char* pszDiscId,
                                      const char* pszScale);

    virtual CPLErr GetGeoTransform( double * padfGeoTransform) override
    {
        memcpy(padfGeoTransform, adfGeoTransform, 6 * sizeof(double));
        return CE_None;
    }

    virtual const char *GetProjectionRef(void) override
    {
        return SRS_WKT_WGS84;
    }

    static GDALDataset* Build(  const char* pszTOCFilename,
                                CPLXMLNode* psXML,
                                CPLString osProduct,
                                CPLString osDiscId,
                                CPLString osScale,
                                const char* pszFilename);

    static int Identify( GDALOpenInfo * poOpenInfo );
    static GDALDataset* Open( GDALOpenInfo * poOpenInfo );
};

/************************************************************************/
/* ==================================================================== */
/*                            ECRGTOCSubDataset                          */
/* ==================================================================== */
/************************************************************************/

class ECRGTOCSubDataset : public VRTDataset
{
  char**       papszFileList;

  public:
    ECRGTOCSubDataset(int nXSize, int nYSize) : VRTDataset(nXSize, nYSize)
    {
        /* Don't try to write a VRT file */
        SetWritable(FALSE);

        /* The driver is set to VRT in VRTDataset constructor. */
        /* We have to set it to the expected value ! */
        poDriver = reinterpret_cast<GDALDriver *>(
            GDALGetDriverByName( "ECRGTOC" ) );

        papszFileList = NULL;
    }

    ~ECRGTOCSubDataset()
    {
        CSLDestroy(papszFileList);
    }

    virtual char      **GetFileList() override { return CSLDuplicate(papszFileList); }

    static GDALDataset* Build(  const char* pszProductTitle,
                                const char* pszDiscId,
                                int nScale,
                                int nCountSubDataset,
                                const char* pszTOCFilename,
                                const std::vector<FrameDesc>& aosFrameDesc,
                                double dfGlobalMinX,
                                double dfGlobalMinY,
                                double dfGlobalMaxX,
                                double dfGlobalMaxY,
                                double dfGlobalPixelXSize,
                                double dfGlobalPixelYSize);
};

/************************************************************************/
/*                           LaunderString()                            */
/************************************************************************/

static CPLString LaunderString(const char* pszStr)
{
    CPLString osRet(pszStr);
    for(size_t i=0;i<osRet.size();i++)
    {
        if( osRet[i] == ':' || osRet[i] == ' ' )
            osRet[i] = '_';
    }
    return osRet;
}

/************************************************************************/
/*                           AddSubDataset()                            */
/************************************************************************/

void ECRGTOCDataset::AddSubDataset( const char* pszFilename,
                                    const char* pszProductTitle,
                                    const char* pszDiscId,
                                    const char* pszScale)

{
    char szName[80];
    const int nCount = CSLCount(papszSubDatasets ) / 2;

    snprintf( szName, sizeof(szName), "SUBDATASET_%d_NAME", nCount+1 );
    papszSubDatasets =
        CSLSetNameValue( papszSubDatasets, szName,
              CPLSPrintf( "ECRG_TOC_ENTRY:%s:%s:%s:%s",
                          LaunderString(pszProductTitle).c_str(),
                          LaunderString(pszDiscId).c_str(),
                          LaunderString(pszScale).c_str(), pszFilename ) );

    snprintf( szName, sizeof(szName), "SUBDATASET_%d_DESC", nCount+1 );
    papszSubDatasets =
        CSLSetNameValue( papszSubDatasets, szName,
            CPLSPrintf( "Product %s, disc %s, scale %s", pszProductTitle, pszDiscId, pszScale));
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **ECRGTOCDataset::GetMetadata( const char *pszDomain )

{
    if( pszDomain != NULL && EQUAL(pszDomain,"SUBDATASETS") )
        return papszSubDatasets;

    return GDALPamDataset::GetMetadata( pszDomain );
}

/************************************************************************/
/*                         GetScaleFromString()                         */
/************************************************************************/

static int GetScaleFromString(const char* pszScale)
{
    const char* pszPtr = strstr(pszScale, "1:");
    if (pszPtr)
        pszPtr = pszPtr + 2;
    else
        pszPtr = pszScale;

    int nScale = 0;
    char ch;
    while((ch = *pszPtr) != '\0')
    {
        if (ch >= '0' && ch <= '9')
            nScale = nScale * 10 + ch - '0';
        else if (ch == ' ')
            ;
        else if (ch == 'k' || ch == 'K')
            return nScale * 1000;
        else if (ch == 'm' || ch == 'M')
            return nScale * 1000000;
        else
            return 0;
        pszPtr ++;
    }
    return nScale;
}

/************************************************************************/
/*                            GetFromBase34()                           */
/************************************************************************/

static GIntBig GetFromBase34(const char* pszVal, int nMaxSize)
{
    GIntBig nFrameNumber = 0;
    for(int i=0;i<nMaxSize;i++)
    {
        char ch = pszVal[i];
        if (ch == '\0')
            break;
        int chVal;
        if (ch >= 'A' && ch <= 'Z')
            ch += 'a' - 'A';
        /* i and o letters are excluded, */
        if (ch >= '0' && ch <= '9')
            chVal = ch - '0';
        else if (ch >= 'a' && ch <= 'h')
            chVal = ch - 'a' + 10;
        else if (ch >= 'j' && ch <= 'n')
            chVal = ch - 'a' + 10 - 1;
        else if (ch >= 'p' && ch <= 'z')
            chVal = ch - 'a' + 10 - 2;
        else
        {
            CPLDebug("ECRG", "Invalid base34 value : %s", pszVal);
            break;
        }
        nFrameNumber = nFrameNumber * 34 + chVal;
    }

    return nFrameNumber;
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

/* MIL-PRF-32283 - Table II. ECRG zone limits. */
/* starting with a fake zone 0 for convenience. */
static const int anZoneUpperLat[] = { 0, 32, 48, 56, 64, 68, 72, 76, 80 };

/* APPENDIX 70, TABLE III of MIL-A-89007 */
static const int anACst_ADRG[] =
    { 369664, 302592, 245760, 199168, 163328, 137216, 110080, 82432 };
static const int nBCst_ADRG = 400384;

// TODO: Why are these two functions done this way?
static int CEIL_ROUND(double a, double b)
{
    return static_cast<int>( ceil( a / b ) * b );
}

static int NEAR_ROUND(double a, double b)
{
    return static_cast<int>( floor( ( a / b ) + 0.5 ) * b );
}

static const int ECRG_PIXELS = 2304;

static
int GetExtent(const char* pszFrameName, int nScale, int nZone,
              double& dfMinX, double& dfMaxX, double& dfMinY, double& dfMaxY,
              double& dfPixelXSize, double& dfPixelYSize)
{
    const int nAbsZone = abs(nZone);

/************************************************************************/
/*  Compute east-west constant                                          */
/************************************************************************/
    /* MIL-PRF-89038 - 60.1.2 - East-west pixel constant. */
    const int nEW_ADRG = CEIL_ROUND(anACst_ADRG[nAbsZone-1] * (1e6 / nScale), 512);
    const int nEW_CADRG = NEAR_ROUND(nEW_ADRG / (150. / 100.), 256);
    /* MIL-PRF-32283 - D.2.1.2 - East-west pixel constant. */
    const int nEW = nEW_CADRG / 256 * 384;

/************************************************************************/
/*  Compute number of longitudinal frames                               */
/************************************************************************/
    /* MIL-PRF-32283 - D.2.1.7 - Longitudinal frames and subframes */
    const int nCols = static_cast<int>(
        ceil(static_cast<double>(nEW) / ECRG_PIXELS) );

/************************************************************************/
/*  Compute north-south constant                                        */
/************************************************************************/
    /* MIL-PRF-89038 - 60.1.1 -  North-south. pixel constant */
    const int nNS_ADRG = CEIL_ROUND(nBCst_ADRG * (1e6 / nScale), 512) / 4;
    const int nNS_CADRG = NEAR_ROUND(nNS_ADRG / (150. / 100.), 256);
    /* MIL-PRF-32283 - D.2.1.1 - North-south. pixel constant and Frame Width/Height */
    const int nNS = nNS_CADRG / 256 * 384;

/************************************************************************/
/*  Compute number of latitudinal frames and latitude of top of zone    */
/************************************************************************/
    dfPixelYSize = 90.0 / nNS;

    const double dfFrameLatHeight = dfPixelYSize * ECRG_PIXELS;

    /* MIL-PRF-32283 - D.2.1.5 - Equatorward and poleward zone extents. */
    int nUpperZoneFrames = static_cast<int>(
        ceil(anZoneUpperLat[nAbsZone] / dfFrameLatHeight) );
    int nBottomZoneFrames = static_cast<int>(
        floor(anZoneUpperLat[nAbsZone-1] / dfFrameLatHeight) );
    const int nRows = nUpperZoneFrames - nBottomZoneFrames;

    /* Not sure to really understand D.2.1.5.a. Testing needed */
    if (nZone < 0)
    {
        nUpperZoneFrames = -nBottomZoneFrames;
        /*nBottomZoneFrames = nUpperZoneFrames - nRows;*/
    }

    const double dfUpperZoneTopLat = dfFrameLatHeight * nUpperZoneFrames;

/************************************************************************/
/*  Compute coordinates of the frame in the zone                        */
/************************************************************************/

    /* Converts the first 10 characters into a number from base 34 */
    const GIntBig nFrameNumber = GetFromBase34(pszFrameName, 10);

    /*  MIL-PRF-32283 - A.2.6.1 */
    const GIntBig nY = nFrameNumber / nCols;
    const GIntBig nX = nFrameNumber % nCols;

/************************************************************************/
/*  Compute extent of the frame                                         */
/************************************************************************/

    /* The nY is counted from the bottom of the zone... Pfff */
    dfMaxY = dfUpperZoneTopLat - (nRows - 1 - nY) * dfFrameLatHeight;
    dfMinY = dfMaxY - dfFrameLatHeight;

    dfPixelXSize = 360.0 / nEW;

    const double dfFrameLongWidth = dfPixelXSize * ECRG_PIXELS;
    dfMinX = -180.0 + nX * dfFrameLongWidth;
    dfMaxX = dfMinX + dfFrameLongWidth;

#ifdef DEBUG_VERBOSE
    CPLDebug("ECRG", "Frame %s : minx=%.16g, maxy=%.16g, maxx=%.16g, miny=%.16g",
             pszFrameName, dfMinX, dfMaxY, dfMaxX, dfMinY);
#endif

    return TRUE;
}

/************************************************************************/
/* ==================================================================== */
/*                        ECRGTOCProxyRasterDataSet                       */
/* ==================================================================== */
/************************************************************************/

class ECRGTOCProxyRasterDataSet : public GDALProxyPoolDataset
{
    /* The following parameters are only for sanity checking */
    int checkDone;
    int checkOK;
    double dfMinX;
    double dfMaxY;
    double dfPixelXSize;
    double dfPixelYSize;

    public:
        ECRGTOCProxyRasterDataSet( ECRGTOCSubDataset* /* poSubDataset */,
                                   const char* fileName,
                                   int nXSize, int nYSize,
                                   double dfMinX, double dfMaxY,
                                   double dfPixelXSize, double dfPixelYSize );

        GDALDataset* RefUnderlyingDataset() override
        {
            GDALDataset* poSourceDS = GDALProxyPoolDataset::RefUnderlyingDataset();
            if (poSourceDS)
            {
                if (!checkDone)
                    SanityCheckOK(poSourceDS);
                if (!checkOK)
                {
                    GDALProxyPoolDataset::UnrefUnderlyingDataset(poSourceDS);
                    poSourceDS = NULL;
                }
            }
            return poSourceDS;
        }

        void UnrefUnderlyingDataset(GDALDataset* poUnderlyingDataset) override
        {
            GDALProxyPoolDataset::UnrefUnderlyingDataset(poUnderlyingDataset);
        }

        int SanityCheckOK(GDALDataset* poSourceDS);
};

/************************************************************************/
/*                    ECRGTOCProxyRasterDataSet()                       */
/************************************************************************/

ECRGTOCProxyRasterDataSet::ECRGTOCProxyRasterDataSet(
    ECRGTOCSubDataset* /* poSubDatasetIn */,
    const char* fileNameIn,
    int nXSizeIn, int nYSizeIn,
    double dfMinXIn, double dfMaxYIn,
    double dfPixelXSizeIn, double dfPixelYSizeIn ) :
    // Mark as shared since the VRT will take several references if we are in
    // RGBA mode (4 bands for this dataset).
    GDALProxyPoolDataset(fileNameIn, nXSizeIn, nYSizeIn, GA_ReadOnly,
                         TRUE, SRS_WKT_WGS84),
    checkDone(FALSE),
    checkOK(FALSE),
    dfMinX(dfMinXIn),
    dfMaxY(dfMaxYIn),
    dfPixelXSize(dfPixelXSizeIn),
    dfPixelYSize(dfPixelYSizeIn)
{

    for( int i = 0; i < 3; i++ )
    {
        SetBand(i + 1,
                new GDALProxyPoolRasterBand(this, i+1, GDT_Byte, nXSizeIn, 1));
    }
}

/************************************************************************/
/*                    SanityCheckOK()                                   */
/************************************************************************/

#define WARN_CHECK_DS(x) do { if (!(x)) { \
    CPLError(CE_Warning, CPLE_AppDefined,                             \
             "For %s, assert '" #x "' failed",                        \
             GetDescription()); checkOK = FALSE; } } while( false )

int ECRGTOCProxyRasterDataSet::SanityCheckOK( GDALDataset* poSourceDS )
{
    // int nSrcBlockXSize;
    // int nSrcBlockYSize;
    // int nBlockXSize;
    // int nBlockYSize;
    double l_adfGeoTransform[6] = {};
    if( checkDone )
        return checkOK;

    checkOK = TRUE;
    checkDone = TRUE;

    poSourceDS->GetGeoTransform(l_adfGeoTransform);
    WARN_CHECK_DS(fabs(l_adfGeoTransform[0] - dfMinX) < 1e-10);
    WARN_CHECK_DS(fabs(l_adfGeoTransform[3] - dfMaxY) < 1e-10);
    WARN_CHECK_DS(fabs(l_adfGeoTransform[1] - dfPixelXSize) < 1e-10);
    WARN_CHECK_DS(fabs(l_adfGeoTransform[5] - (-dfPixelYSize)) < 1e-10);
    WARN_CHECK_DS(l_adfGeoTransform[2] == 0 &&
                  l_adfGeoTransform[4] == 0);  // No rotation.
    WARN_CHECK_DS(poSourceDS->GetRasterCount() == 3);
    WARN_CHECK_DS(poSourceDS->GetRasterXSize() == nRasterXSize);
    WARN_CHECK_DS(poSourceDS->GetRasterYSize() == nRasterYSize);
    WARN_CHECK_DS(EQUAL(poSourceDS->GetProjectionRef(), SRS_WKT_WGS84));
    // poSourceDS->GetRasterBand(1)->GetBlockSize(&nSrcBlockXSize,
    //                                            &nSrcBlockYSize);
    // GetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);
    // WARN_CHECK_DS(nSrcBlockXSize == nBlockXSize);
    // WARN_CHECK_DS(nSrcBlockYSize == nBlockYSize);
    WARN_CHECK_DS(
        poSourceDS->GetRasterBand(1)->GetRasterDataType() == GDT_Byte);

    return checkOK;
}

/************************************************************************/
/*                           BuildFullName()                            */
/************************************************************************/

static const char* BuildFullName(const char* pszTOCFilename,
                                 const char* pszFramePath,
                                 const char* pszFrameName)
{
    char* pszPath = NULL;
    if (pszFramePath[0] == '.' &&
        (pszFramePath[1] == '/' ||pszFramePath[1] == '\\'))
        pszPath = CPLStrdup(pszFramePath + 2);
    else
        pszPath = CPLStrdup(pszFramePath);
    for(int i=0;pszPath[i] != '\0';i++)
    {
        if (pszPath[i] == '\\')
            pszPath[i] = '/';
    }
    const char* pszName = CPLFormFilename(pszPath, pszFrameName, NULL);
    CPLFree(pszPath);
    pszPath = NULL;
    const char* pszTOCPath = CPLGetDirname(pszTOCFilename);
    const char* pszFirstSlashInName = strchr(pszName, '/');
    if (pszFirstSlashInName != NULL)
    {
        int nFirstDirLen = static_cast<int>(pszFirstSlashInName - pszName);
        if (static_cast<int>( strlen(pszTOCPath) ) >= nFirstDirLen + 1 &&
            (pszTOCPath[strlen(pszTOCPath) - (nFirstDirLen + 1)] == '/' ||
                pszTOCPath[strlen(pszTOCPath) - (nFirstDirLen + 1)] == '\\') &&
            strncmp(pszTOCPath + strlen(pszTOCPath) - nFirstDirLen, pszName, nFirstDirLen) == 0)
        {
            pszTOCPath = CPLGetDirname(pszTOCPath);
        }
    }
    return CPLProjectRelativeFilename(pszTOCPath, pszName);
}

/************************************************************************/
/*                              Build()                                 */
/************************************************************************/

/* Builds a ECRGTOCSubDataset from the set of files of the toc entry */
GDALDataset* ECRGTOCSubDataset::Build(  const char* pszProductTitle,
                                        const char* pszDiscId,
                                        int nScale,
                                        int nCountSubDataset,
                                        const char* pszTOCFilename,
                                        const std::vector<FrameDesc>& aosFrameDesc,
                                        double dfGlobalMinX,
                                        double dfGlobalMinY,
                                        double dfGlobalMaxX,
                                        double dfGlobalMaxY,
                                        double dfGlobalPixelXSize,
                                        double dfGlobalPixelYSize)
{
    GDALDriver *poDriver = GetGDALDriverManager()->GetDriverByName("VRT");
    if( poDriver == NULL )
        return NULL;

    const int nSizeX = static_cast<int>(
        (dfGlobalMaxX - dfGlobalMinX) / dfGlobalPixelXSize + 0.5);
    const int nSizeY = static_cast<int>(
        (dfGlobalMaxY - dfGlobalMinY) / dfGlobalPixelYSize + 0.5);

    /* ------------------------------------ */
    /* Create the VRT with the overall size */
    /* ------------------------------------ */
    ECRGTOCSubDataset *poVirtualDS = new ECRGTOCSubDataset( nSizeX, nSizeY );

    poVirtualDS->SetProjection(SRS_WKT_WGS84);

    double adfGeoTransform[6] = {
      dfGlobalMinX,
      dfGlobalPixelXSize,
      0,
      dfGlobalMaxY,
      0,
      -dfGlobalPixelYSize
    };
    poVirtualDS->SetGeoTransform(adfGeoTransform);

    for (int i=0;i<3;i++)
    {
        poVirtualDS->AddBand(GDT_Byte, NULL);
        GDALRasterBand *poBand = poVirtualDS->GetRasterBand( i + 1 );
        poBand->SetColorInterpretation((GDALColorInterp)(GCI_RedBand+i));
    }

    poVirtualDS->SetDescription(pszTOCFilename);

    poVirtualDS->SetMetadataItem("PRODUCT_TITLE", pszProductTitle);
    poVirtualDS->SetMetadataItem("DISC_ID", pszDiscId);
    if (nScale != -1)
        poVirtualDS->SetMetadataItem("SCALE", CPLString().Printf("%d", nScale));

    /* -------------------------------------------------------------------- */
    /*      Check for overviews.                                            */
    /* -------------------------------------------------------------------- */

    poVirtualDS->oOvManager.Initialize( poVirtualDS,
                                        CPLString().Printf("%s.%d", pszTOCFilename, nCountSubDataset));

    poVirtualDS->papszFileList = poVirtualDS->GDALDataset::GetFileList();

    for( int i=0; i < static_cast<int>( aosFrameDesc.size() ); i++)
    {
        const char* pszName = BuildFullName(pszTOCFilename,
                                            aosFrameDesc[i].pszPath,
                                            aosFrameDesc[i].pszName);

        double dfMinX = 0.0;
        double dfMaxX = 0.0;
        double dfMinY = 0.0;
        double dfMaxY = 0.0;
        double dfPixelXSize = 0.0;
        double dfPixelYSize = 0.0;
        GetExtent(aosFrameDesc[i].pszName,
                  aosFrameDesc[i].nScale, aosFrameDesc[i].nZone,
                  dfMinX, dfMaxX, dfMinY, dfMaxY, dfPixelXSize, dfPixelYSize);

        const int nFrameXSize = static_cast<int>(
            (dfMaxX - dfMinX) / dfPixelXSize + 0.5);
        const int nFrameYSize = static_cast<int>(
            (dfMaxY - dfMinY) / dfPixelYSize + 0.5);

        poVirtualDS->papszFileList = CSLAddString(poVirtualDS->papszFileList, pszName);

        /* We create proxy datasets and raster bands */
        /* Using real datasets and raster bands is possible in theory */
        /* However for large datasets, a TOC entry can include several hundreds of files */
        /* and we finally reach the limit of maximum file descriptors open at the same time ! */
        /* So the idea is to warp the datasets into a proxy and open the underlying dataset only when it is */
        /* needed (IRasterIO operation). To improve a bit efficiency, we have a cache of opened */
        /* underlying datasets */
        ECRGTOCProxyRasterDataSet* poDS = new ECRGTOCProxyRasterDataSet(
            reinterpret_cast<ECRGTOCSubDataset *>( poVirtualDS), pszName,
            nFrameXSize, nFrameYSize,
            dfMinX, dfMaxY, dfPixelXSize, dfPixelYSize);

        for( int j=0; j<3; j++)
        {
            VRTSourcedRasterBand *poBand = reinterpret_cast<VRTSourcedRasterBand *>(
                poVirtualDS->GetRasterBand( j + 1 ) );
            /* Place the raster band at the right position in the VRT */
            poBand->AddSimpleSource(
                poDS->GetRasterBand(j + 1),
                0, 0, nFrameXSize, nFrameYSize,
                static_cast<int>((dfMinX - dfGlobalMinX) / dfGlobalPixelXSize + 0.5),
                static_cast<int>((dfGlobalMaxY - dfMaxY) / dfGlobalPixelYSize + 0.5),
                static_cast<int>((dfMaxX - dfMinX) / dfGlobalPixelXSize + 0.5),
                static_cast<int>((dfMaxY - dfMinY) / dfGlobalPixelYSize + 0.5));
        }

        /* The ECRGTOCProxyRasterDataSet will be destroyed when its last raster band will be */
        /* destroyed */
        poDS->Dereference();
    }

    poVirtualDS->SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");

    return poVirtualDS;
}

/************************************************************************/
/*                             Build()                                  */
/************************************************************************/

GDALDataset* ECRGTOCDataset::Build(const char* pszTOCFilename,
                                   CPLXMLNode* psXML,
                                   CPLString osProduct,
                                   CPLString osDiscId,
                                   CPLString osScale,
                                   const char* pszOpenInfoFilename)
{
    CPLXMLNode* psTOC = CPLGetXMLNode(psXML, "=Table_of_Contents");
    if (psTOC == NULL)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Cannot find Table_of_Contents element");
        return NULL;
    }

    double dfGlobalMinX = 0.0;
    double dfGlobalMinY = 0.0;
    double dfGlobalMaxX = 0.0;
    double dfGlobalMaxY = 0.0;
    double dfGlobalPixelXSize = 0.0;
    double dfGlobalPixelYSize = 0.0;
    bool bGlobalExtentValid = false;

    ECRGTOCDataset* poDS = new ECRGTOCDataset();
    int nSubDatasets = 0;

    int bLookForSubDataset = !osProduct.empty() && !osDiscId.empty();

    int nCountSubDataset = 0;

    poDS->SetDescription( pszOpenInfoFilename );
    poDS->papszFileList = poDS->GDALDataset::GetFileList();

    for(CPLXMLNode* psIter1 = psTOC->psChild;
                    psIter1 != NULL;
                    psIter1 = psIter1->psNext)
    {
        if (!(psIter1->eType == CXT_Element && psIter1->pszValue != NULL &&
              strcmp(psIter1->pszValue, "product") == 0))
            continue;

        const char* pszProductTitle =
            CPLGetXMLValue(psIter1, "product_title", NULL);
        if (pszProductTitle == NULL)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                        "Cannot find product_title attribute");
            continue;
        }

        if (bLookForSubDataset && strcmp(LaunderString(pszProductTitle), osProduct.c_str()) != 0)
            continue;

        for(CPLXMLNode* psIter2 = psIter1->psChild;
                        psIter2 != NULL;
                        psIter2 = psIter2->psNext)
        {
            if (!(psIter2->eType == CXT_Element && psIter2->pszValue != NULL &&
                  strcmp(psIter2->pszValue, "disc") == 0))
                continue;

            const char* pszDiscId = CPLGetXMLValue(psIter2, "id", NULL);
            if (pszDiscId == NULL)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                            "Cannot find id attribute");
                continue;
            }

            if (bLookForSubDataset && strcmp(LaunderString(pszDiscId), osDiscId.c_str()) != 0)
                continue;

            CPLXMLNode* psFrameList = CPLGetXMLNode(psIter2, "frame_list");
            if (psFrameList == NULL)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                            "Cannot find frame_list element");
                continue;
            }

            for(CPLXMLNode* psIter3 = psFrameList->psChild;
                            psIter3 != NULL;
                            psIter3 = psIter3->psNext)
            {
                if (!(psIter3->eType == CXT_Element &&
                      psIter3->pszValue != NULL &&
                      strcmp(psIter3->pszValue, "scale") == 0))
                    continue;

                const char* pszSize = CPLGetXMLValue(psIter3, "size", NULL);
                if (pszSize == NULL)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                                "Cannot find size attribute");
                    continue;
                }

                int nScale = GetScaleFromString(pszSize);
                if (nScale <= 0)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Invalid scale %s", pszSize);
                    continue;
                }

                if( bLookForSubDataset )
                {
                    if( !osScale.empty() )
                    {
                        if( strcmp(LaunderString(pszSize), osScale.c_str()) != 0 )
                        {
                            continue;
                        }
                    }
                    else
                    {
                        int nCountScales = 0;
                        for(CPLXMLNode* psIter4 = psFrameList->psChild;
                                psIter4 != NULL;
                                psIter4 = psIter4->psNext)
                        {
                            if (!(psIter4->eType == CXT_Element &&
                                psIter4->pszValue != NULL &&
                                strcmp(psIter4->pszValue, "scale") == 0))
                                continue;
                            nCountScales ++;
                        }
                        if( nCountScales > 1 )
                        {
                            CPLError( CE_Failure, CPLE_AppDefined,
                                      "Scale should be mentioned in "
                                      "subdatasets syntax since this disk "
                                      "contains several scales" );
                            delete poDS;
                            return NULL;
                        }
                    }
                }

                nCountSubDataset ++;

                std::vector<FrameDesc> aosFrameDesc;
                int nValidFrames = 0;

                for(CPLXMLNode* psIter4 = psIter3->psChild;
                                psIter4 != NULL;
                                psIter4 = psIter4->psNext)
                {
                    if (!(psIter4->eType == CXT_Element &&
                          psIter4->pszValue != NULL &&
                          strcmp(psIter4->pszValue, "frame") == 0))
                        continue;

                    const char* pszFrameName =
                        CPLGetXMLValue(psIter4, "name", NULL);
                    if (pszFrameName == NULL)
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Cannot find name element");
                        continue;
                    }

                    if (strlen(pszFrameName) != 18)
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Invalid value for name element : %s",
                                 pszFrameName);
                        continue;
                    }

                    const char* pszFramePath =
                        CPLGetXMLValue(psIter4, "frame_path", NULL);
                    if (pszFramePath == NULL)
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Cannot find frame_path element");
                        continue;
                    }

                    const char* pszFrameZone =
                        CPLGetXMLValue(psIter4, "frame_zone", NULL);
                    if (pszFrameZone == NULL)
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Cannot find frame_zone element");
                        continue;
                    }
                    if (strlen(pszFrameZone) != 1)
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Invalid value for frame_zone element : %s",
                                 pszFrameZone);
                        continue;
                    }
                    char chZone = pszFrameZone[0];
                    int nZone = 0;
                    if (chZone >= '1' && chZone <= '9')
                        nZone = chZone - '0';
                    else if (chZone >= 'a' && chZone <= 'h')
                        nZone = -(chZone - 'a' + 1);
                    else if (chZone >= 'A' && chZone <= 'H')
                        nZone = -(chZone - 'A' + 1);
                    else if (chZone == 'j' || chZone == 'J')
                        nZone = -9;
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Invalid value for frame_zone element : %s",
                                  pszFrameZone);
                        continue;
                    }
                    if (nZone == 9 || nZone == -9)
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Polar zones unhandled by current implementation");
                        continue;
                    }

                    double dfMinX = 0.0;
                    double dfMaxX = 0.0;
                    double dfMinY = 0.0;
                    double dfMaxY = 0.0;
                    double dfPixelXSize = 0.0;
                    double dfPixelYSize = 0.0;
                    if (!GetExtent(pszFrameName, nScale, nZone,
                                   dfMinX, dfMaxX, dfMinY, dfMaxY,
                                   dfPixelXSize, dfPixelYSize))
                    {
                        continue;
                    }

                    nValidFrames ++;

                    const char* pszFullName = BuildFullName(pszTOCFilename,
                                                        pszFramePath,
                                                        pszFrameName);
                    poDS->papszFileList = CSLAddString(poDS->papszFileList, pszFullName);

                    if (!bGlobalExtentValid)
                    {
                        dfGlobalMinX = dfMinX;
                        dfGlobalMinY = dfMinY;
                        dfGlobalMaxX = dfMaxX;
                        dfGlobalMaxY = dfMaxY;
                        dfGlobalPixelXSize = dfPixelXSize;
                        dfGlobalPixelYSize = dfPixelYSize;
                        bGlobalExtentValid = true;
                    }
                    else
                    {
                        if (dfMinX < dfGlobalMinX) dfGlobalMinX = dfMinX;
                        if (dfMinY < dfGlobalMinY) dfGlobalMinY = dfMinY;
                        if (dfMaxX > dfGlobalMaxX) dfGlobalMaxX = dfMaxX;
                        if (dfMaxY > dfGlobalMaxY) dfGlobalMaxY = dfMaxY;
                        if (dfPixelXSize < dfGlobalPixelXSize)
                            dfGlobalPixelXSize = dfPixelXSize;
                        if (dfPixelYSize < dfGlobalPixelYSize)
                            dfGlobalPixelYSize = dfPixelYSize;
                    }

                    nValidFrames ++;

                    if (bLookForSubDataset)
                    {
                        FrameDesc frameDesc;
                        frameDesc.pszName = pszFrameName;
                        frameDesc.pszPath = pszFramePath;
                        frameDesc.nScale = nScale;
                        frameDesc.nZone = nZone;
                        aosFrameDesc.push_back(frameDesc);
                    }
                }

                if (bLookForSubDataset)
                {
                    delete poDS;
                    if (nValidFrames == 0)
                        return NULL;
                    return ECRGTOCSubDataset::Build(pszProductTitle,
                                                    pszDiscId,
                                                    nScale,
                                                    nCountSubDataset,
                                                    pszTOCFilename,
                                                    aosFrameDesc,
                                                    dfGlobalMinX,
                                                    dfGlobalMinY,
                                                    dfGlobalMaxX,
                                                    dfGlobalMaxY,
                                                    dfGlobalPixelXSize,
                                                    dfGlobalPixelYSize);
                }

                if (nValidFrames)
                {
                    poDS->AddSubDataset(pszOpenInfoFilename,
                                        pszProductTitle, pszDiscId, pszSize);
                    nSubDatasets ++;
                }
            }
        }
    }

    if (!bGlobalExtentValid)
    {
        delete poDS;
        return NULL;
    }

    if (nSubDatasets == 1)
    {
        const char* pszSubDatasetName = CSLFetchNameValue(
            poDS->GetMetadata("SUBDATASETS"), "SUBDATASET_1_NAME");
        GDALOpenInfo oOpenInfo(pszSubDatasetName, GA_ReadOnly);
        delete poDS;
        GDALDataset* poRetDS = Open(&oOpenInfo);
        if (poRetDS)
            poRetDS->SetDescription(pszOpenInfoFilename);
        return poRetDS;
    }

    poDS->adfGeoTransform[0] = dfGlobalMinX;
    poDS->adfGeoTransform[1] = dfGlobalPixelXSize;
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[3] = dfGlobalMaxY;
    poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[5] = - dfGlobalPixelYSize;

    poDS->nRasterXSize = static_cast<int>(
        0.5 + (dfGlobalMaxX - dfGlobalMinX) / dfGlobalPixelXSize);
    poDS->nRasterYSize = static_cast<int>(
        0.5 + (dfGlobalMaxY - dfGlobalMinY) / dfGlobalPixelYSize);

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->TryLoadXML();

    return poDS;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int ECRGTOCDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    const char *pszFilename = poOpenInfo->pszFilename;

/* -------------------------------------------------------------------- */
/*      Is this a sub-dataset selector? If so, it is obviously ECRGTOC. */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszFilename, "ECRG_TOC_ENTRY:"))
        return TRUE;

/* -------------------------------------------------------------------- */
/*  First we check to see if the file has the expected header           */
/*  bytes.                                                              */
/* -------------------------------------------------------------------- */
    const char *pabyHeader
        = reinterpret_cast<const char *>( poOpenInfo->pabyHeader );
    if( pabyHeader == NULL )
        return FALSE;

    if ( strstr(pabyHeader, "<Table_of_Contents") != NULL &&
         strstr(pabyHeader, "<file_header ") != NULL)
        return TRUE;

    if ( strstr(pabyHeader, "<!DOCTYPE Table_of_Contents [") != NULL)
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *ECRGTOCDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( !Identify( poOpenInfo ) )
        return NULL;

    const char *pszFilename = poOpenInfo->pszFilename;
    CPLString osFilename;
    CPLString osProduct, osDiscId, osScale;

    if( STARTS_WITH_CI(pszFilename, "ECRG_TOC_ENTRY:") )
    {
        pszFilename += strlen("ECRG_TOC_ENTRY:");

        /* PRODUCT:DISK:SCALE:FILENAME (or PRODUCT:DISK:FILENAME historically) */
        /* with FILENAME potentially C:\BLA... */
        char** papszTokens = CSLTokenizeString2(pszFilename, ":", 0);
        int nTokens = CSLCount(papszTokens);
        if( nTokens != 3 && nTokens != 4 && nTokens != 5 )
        {
            CSLDestroy(papszTokens);
            return NULL;
        }

        osProduct = papszTokens[0];
        osDiscId = papszTokens[1];

        if( nTokens == 3 )
            osFilename = papszTokens[2];
        else if( nTokens == 4 )
        {
            if( strlen(papszTokens[2]) == 1 &&
                (papszTokens[3][0] == '\\' ||
                 papszTokens[3][0] == '/') )
            {
                osFilename = papszTokens[2];
                osFilename += ":";
                osFilename += papszTokens[3];
            }
            else
            {
                osScale = papszTokens[2];
                osFilename = papszTokens[3];
            }
        }
        else if( nTokens == 5 &&
                strlen(papszTokens[3]) == 1 &&
                (papszTokens[4][0] == '\\' ||
                 papszTokens[4][0] == '/') )
        {
            osScale = papszTokens[2];
            osFilename = papszTokens[3];
            osFilename += ":";
            osFilename += papszTokens[4];
        }
        else
        {
            CSLDestroy(papszTokens);
            return NULL;
        }

        CSLDestroy(papszTokens);
        pszFilename = osFilename.c_str();
    }

/* -------------------------------------------------------------------- */
/*      Parse the XML file                                              */
/* -------------------------------------------------------------------- */
    CPLXMLNode* psXML = CPLParseXMLFile(pszFilename);
    if (psXML == NULL)
    {
        return NULL;
    }

    GDALDataset* poDS = Build( pszFilename, psXML, osProduct, osDiscId,
                               osScale,
                               poOpenInfo->pszFilename);
    CPLDestroyXMLNode(psXML);

    if (poDS && poOpenInfo->eAccess == GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "ECRGTOC driver does not support update mode");
        delete poDS;
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                         GDALRegister_ECRGTOC()                       */
/************************************************************************/

void GDALRegister_ECRGTOC()

{
    if( GDALGetDriverByName( "ECRGTOC" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "ECRGTOC" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "ECRG TOC format" );

    poDriver->pfnIdentify = ECRGTOCDataset::Identify;
    poDriver->pfnOpen = ECRGTOCDataset::Open;

    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                               "frmt_various.html#ECRGTOC" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "xml" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
