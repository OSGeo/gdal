/******************************************************************************
 *
 * Project:  Polarimetric Workstation
 * Purpose:  Convair PolGASP data (.img/.hdr format).
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "ogr_spatialref.h"
#include "rawdataset.h"

#include <vector>

CPL_CVSID("$Id$")

enum Interleave { BSQ, BIL, BIP };

/************************************************************************/
/* ==================================================================== */
/*                              CPGDataset                              */
/* ==================================================================== */
/************************************************************************/

class SIRC_QSLCRasterBand;
class CPG_STOKESRasterBand;

class CPGDataset final: public RawDataset
{
    friend class SIRC_QSLCRasterBand;
    friend class CPG_STOKESRasterBand;

    VSILFILE *afpImage[4];
    std::vector<CPLString> aosImageFilenames{};

    int nGCPCount;
    GDAL_GCP *pasGCPList;
    char *pszGCPProjection{};

    double adfGeoTransform[6];
    char *pszProjection{};

    int nLoadedStokesLine;
    float *padfStokesMatrix;

    int nInterleave;
    static int  AdjustFilename( char **, const char *, const char * );
    static int FindType1( const char *pszWorkname );
    static int FindType2( const char *pszWorkname );
#ifdef notdef
    static int FindType3( const char *pszWorkname );
#endif
    static GDALDataset *InitializeType1Or2Dataset( const char *pszWorkname );
#ifdef notdef
    static GDALDataset *InitializeType3Dataset( const char *pszWorkname );
#endif
  CPLErr LoadStokesLine( int iLine, int bNativeOrder );

    CPL_DISALLOW_COPY_ASSIGN(CPGDataset)

  public:
    CPGDataset();
    ~CPGDataset() override;

    int GetGCPCount() override;
    const char *_GetGCPProjection() override;
    const OGRSpatialReference* GetGCPSpatialRef() const override {
        return GetGCPSpatialRefFromOldGetGCPProjection();
    }
    const GDAL_GCP *GetGCPs() override;

    const char *_GetProjectionRef() override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }
    CPLErr GetGeoTransform( double * ) override;

    char **GetFileList() override;

    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/*                            CPGDataset()                             */
/************************************************************************/

CPGDataset::CPGDataset() :
    nGCPCount(0),
    pasGCPList(nullptr),
    nLoadedStokesLine(-1),
    padfStokesMatrix(nullptr),
    nInterleave(0)
{
    pszProjection = CPLStrdup("");
    pszGCPProjection = CPLStrdup("");
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;

    for( int iBand = 0; iBand < 4; iBand++ )
        afpImage[iBand] = nullptr;
}

/************************************************************************/
/*                            ~CPGDataset()                            */
/************************************************************************/

CPGDataset::~CPGDataset()

{
    FlushCache();

    for( int iBand = 0; iBand < 4; iBand++ )
    {
        if( afpImage[iBand] != nullptr )
            VSIFCloseL( afpImage[iBand] );
    }

    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }

    CPLFree( pszProjection );
    CPLFree( pszGCPProjection );
    CPLFree( padfStokesMatrix );
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **CPGDataset::GetFileList()

{
    char **papszFileList = RawDataset::GetFileList();
    for( size_t i = 0; i < aosImageFilenames.size(); ++i )
        papszFileList = CSLAddString(papszFileList, aosImageFilenames[i]);
    return papszFileList;
}

/************************************************************************/
/* ==================================================================== */
/*                          SIRC_QSLCPRasterBand                        */
/* ==================================================================== */
/************************************************************************/

class SIRC_QSLCRasterBand final: public GDALRasterBand
{
    friend class CPGDataset;

  public:
    SIRC_QSLCRasterBand( CPGDataset *, int, GDALDataType );
    ~SIRC_QSLCRasterBand() override {}

    CPLErr IReadBlock( int, int, void * ) override;
};

constexpr int M11 = 0;
//constexpr int M12 = 1;
constexpr int M13 = 2;
constexpr int M14 = 3;
//constexpr int M21 = 4;
constexpr int M22 = 5;
constexpr int M23 = 6;
constexpr int M24 = 7;
constexpr int M31 = 8;
constexpr int M32 = 9;
constexpr int M33 = 10;
constexpr int M34 = 11;
constexpr int M41 = 12;
constexpr int M42 = 13;
constexpr int M43 = 14;
constexpr int M44 = 15;

/************************************************************************/
/* ==================================================================== */
/*                          CPG_STOKESRasterBand                        */
/* ==================================================================== */
/************************************************************************/

class CPG_STOKESRasterBand final: public GDALRasterBand
{
    friend class CPGDataset;

    int bNativeOrder;

  public:
    CPG_STOKESRasterBand( GDALDataset *poDS,
                          GDALDataType eType,
                          int bNativeOrder );
    ~CPG_STOKESRasterBand() override {}

    CPLErr IReadBlock( int, int, void * ) override;
};

/************************************************************************/
/*                           AdjustFilename()                           */
/*                                                                      */
/*      Try to find the file with the request polarization and          */
/*      extension and update the passed filename accordingly.           */
/*                                                                      */
/*      Return TRUE if file found otherwise FALSE.                      */
/************************************************************************/

int CPGDataset::AdjustFilename( char **pszFilename,
                                const char *pszPolarization,
                                const char *pszExtension )

{
    // TODO: Eventually we should handle upper/lower case.
    if ( EQUAL(pszPolarization,"stokes") )
    {
        const char *pszNewName =
            CPLResetExtension(*pszFilename,
                              pszExtension);
        CPLFree(*pszFilename);
        *pszFilename = CPLStrdup(pszNewName);
    }
    else if (strlen(pszPolarization) == 2)
    {
        char *subptr = strstr(*pszFilename,"hh");
        if (subptr == nullptr)
            subptr = strstr(*pszFilename,"hv");
        if (subptr == nullptr)
            subptr = strstr(*pszFilename,"vv");
        if (subptr == nullptr)
            subptr = strstr(*pszFilename,"vh");
        if (subptr == nullptr)
          return FALSE;

        strncpy( subptr, pszPolarization, 2);
        const char *pszNewName =
            CPLResetExtension(*pszFilename,
                              pszExtension);
        CPLFree(*pszFilename);
        *pszFilename = CPLStrdup(pszNewName);
    }
    else
    {
        const char *pszNewName =
            CPLResetExtension(*pszFilename,
                              pszExtension);
        CPLFree(*pszFilename);
        *pszFilename = CPLStrdup(pszNewName);
    }
    VSIStatBufL sStatBuf;
    return VSIStatL( *pszFilename, &sStatBuf ) == 0;
}

/************************************************************************/
/*         Search for the various types of Convair filesets             */
/*         Return TRUE for a match, FALSE for no match                  */
/************************************************************************/
int CPGDataset::FindType1( const char *pszFilename )
{
  const int nNameLen = static_cast<int>(strlen(pszFilename));

  if ((strstr(pszFilename,"sso") == nullptr) &&
      (strstr(pszFilename,"polgasp") == nullptr))
      return FALSE;

  if (( strlen(pszFilename) < 5) ||
      (!EQUAL(pszFilename+nNameLen-4,".hdr")
       && !EQUAL(pszFilename+nNameLen-4,".img")))
      return FALSE;

  /* Expect all bands and headers to be present */
  char* pszTemp = CPLStrdup(pszFilename);

  const bool bNotFound = !AdjustFilename( &pszTemp, "hh", "img" )
      || !AdjustFilename( &pszTemp, "hh", "hdr" )
      || !AdjustFilename( &pszTemp, "hv", "img" )
      || !AdjustFilename( &pszTemp, "hv", "hdr" )
      || !AdjustFilename( &pszTemp, "vh", "img" )
      || !AdjustFilename( &pszTemp, "vh", "hdr" )
      || !AdjustFilename( &pszTemp, "vv", "img" )
      || !AdjustFilename( &pszTemp, "vv", "hdr" );

  CPLFree(pszTemp);

  return !bNotFound;
}

int CPGDataset::FindType2( const char *pszFilename )
{
  const int nNameLen = static_cast<int>(strlen( pszFilename ));

  if (( strlen(pszFilename) < 9) ||
      (!EQUAL(pszFilename+nNameLen-8,"SIRC.hdr")
       && !EQUAL(pszFilename+nNameLen-8,"SIRC.img")))
      return FALSE;

  char* pszTemp = CPLStrdup(pszFilename);
  const bool bNotFound = !AdjustFilename( &pszTemp, "", "img" )
      || !AdjustFilename( &pszTemp, "", "hdr" );
  CPLFree(pszTemp);

  return !bNotFound;
}

#ifdef notdef
int CPGDataset::FindType3( const char *pszFilename )
{
  const int nNameLen = static_cast<int>(strlen( pszFilename ));

  if ((strstr(pszFilename,"sso") == NULL) &&
      (strstr(pszFilename,"polgasp") == NULL))
      return FALSE;

  if (( strlen(pszFilename) < 9) ||
      (!EQUAL(pszFilename+nNameLen-4,".img")
       && !EQUAL(pszFilename+nNameLen-8,".img_def")))
      return FALSE;

  char* pszTemp = CPLStrdup(pszFilename);
  const bool bNotFound = !AdjustFilename( &pszTemp, "stokes", "img" )
      || !AdjustFilename( &pszTemp, "stokes", "img_def" );
  CPLFree(pszTemp);

  return !bNotFound;
}
#endif

/************************************************************************/
/*                        LoadStokesLine()                              */
/************************************************************************/

CPLErr CPGDataset::LoadStokesLine( int iLine, int bNativeOrder )

{
    if( iLine == nLoadedStokesLine )
        return CE_None;

    const int nDataSize = GDALGetDataTypeSize(GDT_Float32) / 8;

/* -------------------------------------------------------------------- */
/*      allocate working buffers if we don't have them already.         */
/* -------------------------------------------------------------------- */
    if( padfStokesMatrix == nullptr )
    {
        padfStokesMatrix = reinterpret_cast<float *>(
            CPLMalloc( sizeof(float) * nRasterXSize * 16 ) );
    }

/* -------------------------------------------------------------------- */
/*      Load all the pixel data associated with this scanline.          */
/*      Retains same interleaving as original dataset.                  */
/* -------------------------------------------------------------------- */
    if ( nInterleave == BIP )
    {
        const int offset = nRasterXSize * iLine * nDataSize * 16;
        const int nBytesToRead = nDataSize * nRasterXSize*16;
        if (( VSIFSeekL( afpImage[0], offset, SEEK_SET ) != 0 ) ||
            static_cast<int>( VSIFReadL(
                reinterpret_cast<GByte *>( padfStokesMatrix ),
                1, nBytesToRead, afpImage[0] ) ) != nBytesToRead )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                  "Error reading %d bytes of Stokes Convair at offset %d.\n"
                  "Reading file %s failed.",
                  nBytesToRead, offset, GetDescription() );
            CPLFree( padfStokesMatrix );
            padfStokesMatrix = nullptr;
            nLoadedStokesLine = -1;
            return CE_Failure;
        }
    }
    else if ( nInterleave == BIL )
    {
        for ( int band_index = 0; band_index < 16; band_index++)
        {
            const int offset = nDataSize * (nRasterXSize * iLine +
                                            nRasterXSize*band_index);
            const int nBytesToRead = nDataSize * nRasterXSize;
            if (( VSIFSeekL( afpImage[0], offset, SEEK_SET ) != 0 ) ||
               static_cast<int>( VSIFReadL(
                   reinterpret_cast<GByte *>(
                       padfStokesMatrix + nBytesToRead*band_index ),
                   1, nBytesToRead, afpImage[0] ) ) != nBytesToRead )
            {
                CPLError( CE_Failure, CPLE_FileIO,
                  "Error reading %d bytes of Stokes Convair at offset %d.\n"
                  "Reading file %s failed.",
                  nBytesToRead, offset, GetDescription() );
                CPLFree( padfStokesMatrix );
                padfStokesMatrix = nullptr;
                nLoadedStokesLine = -1;
                return CE_Failure;
            }
        }
    }
    else
    {
        for ( int band_index = 0; band_index < 16; band_index++)
        {
            const int offset =
                nDataSize * ( nRasterXSize * iLine +
                              nRasterXSize * nRasterYSize * band_index );
            const int nBytesToRead = nDataSize * nRasterXSize;
            if (( VSIFSeekL( afpImage[0], offset, SEEK_SET ) != 0 ) ||
               static_cast<int>( VSIFReadL(
                   reinterpret_cast<GByte *>(
                       padfStokesMatrix + nBytesToRead * band_index ),
                   1, nBytesToRead, afpImage[0] ) ) != nBytesToRead )
            {
                CPLError( CE_Failure, CPLE_FileIO,
                  "Error reading %d bytes of Stokes Convair at offset %d.\n"
                  "Reading file %s failed.",
                  nBytesToRead, offset, GetDescription() );
                CPLFree( padfStokesMatrix );
                padfStokesMatrix = nullptr;
                nLoadedStokesLine = -1;
                return CE_Failure;
            }
        }
    }

    if (!bNativeOrder)
        GDALSwapWords( padfStokesMatrix,nDataSize,nRasterXSize*16, nDataSize );

    nLoadedStokesLine = iLine;

    return CE_None;
}

/************************************************************************/
/*       Parse header information and initialize dataset for the        */
/*       appropriate Convair dataset style.                             */
/*       Returns dataset if successful; NULL if there was a problem.    */
/************************************************************************/

GDALDataset* CPGDataset::InitializeType1Or2Dataset( const char *pszFilename )
{

/* -------------------------------------------------------------------- */
/*      Read the .hdr file (the hh one for the .sso and polgasp cases)  */
/*      and parse it.                                                   */
/* -------------------------------------------------------------------- */
    int nLines = 0;
    int nSamples = 0;
    int nError = 0;

    /* Parameters required for pseudo-geocoding.  GCPs map */
    /* slant range to ground range at 16 points.           */
    int iGeoParamsFound = 0;
    int itransposed = 0;
    double dfaltitude = 0.0;
    double dfnear_srd = 0.0;
    double dfsample_size = 0.0;
    double dfsample_size_az = 0.0;

    /* Parameters in geogratis geocoded images */
    int iUTMParamsFound = 0;
    int iUTMZone = 0;
    // int iCorner = 0;
    double dfnorth = 0.0;
    double dfeast = 0.0;

    char* pszWorkname = CPLStrdup(pszFilename);
    AdjustFilename( &pszWorkname, "hh", "hdr" );
    char **papszHdrLines = CSLLoad( pszWorkname );

    for( int iLine = 0; papszHdrLines && papszHdrLines[iLine] != nullptr; iLine++ )
    {
        char **papszTokens = CSLTokenizeString( papszHdrLines[iLine] );

        /* Note: some cv580 file seem to have comments with #, hence the >=
         *       instead of = for token checking, and the equalN for the corner.
         */

        if( CSLCount( papszTokens ) < 2 )
        {
          /* ignore */;
        }
        else if ( ( CSLCount( papszTokens ) >= 3 ) &&
                 EQUAL(papszTokens[0],"reference") &&
                 EQUAL(papszTokens[1],"north") )
        {
            dfnorth = CPLAtof(papszTokens[2]);
            iUTMParamsFound++;
        }
        else if ( ( CSLCount( papszTokens ) >= 3 ) &&
               EQUAL(papszTokens[0],"reference") &&
               EQUAL(papszTokens[1],"east") )
        {
            dfeast = CPLAtof(papszTokens[2]);
            iUTMParamsFound++;
        }
        else if ( ( CSLCount( papszTokens ) >= 5 ) &&
               EQUAL(papszTokens[0],"reference") &&
               EQUAL(papszTokens[1],"projection") &&
               EQUAL(papszTokens[2],"UTM") &&
               EQUAL(papszTokens[3],"zone") )
        {
            iUTMZone = atoi(papszTokens[4]);
            iUTMParamsFound++;
        }
        else if ( ( CSLCount( papszTokens ) >= 3 ) &&
               EQUAL(papszTokens[0],"reference") &&
               EQUAL(papszTokens[1],"corner") &&
               STARTS_WITH_CI(papszTokens[2], "Upper_Left") )
        {
            /* iCorner = 0; */
            iUTMParamsFound++;
        }
        else if( EQUAL(papszTokens[0],"number_lines") )
            nLines = atoi(papszTokens[1]);

        else if( EQUAL(papszTokens[0],"number_samples") )
            nSamples = atoi(papszTokens[1]);

        else if( (EQUAL(papszTokens[0],"header_offset")
                  && atoi(papszTokens[1]) != 0)
                 || (EQUAL(papszTokens[0],"number_channels")
                     && (atoi(papszTokens[1]) != 1)
                     && (atoi(papszTokens[1]) != 10))
                 || (EQUAL(papszTokens[0],"datatype")
                     && atoi(papszTokens[1]) != 1)
                 || (EQUAL(papszTokens[0],"number_format")
                     && !EQUAL(papszTokens[1],"float32")
                     && !EQUAL(papszTokens[1],"int8")))
        {
            CPLError( CE_Failure, CPLE_AppDefined,
       "Keyword %s has value %s which does not match CPG driver expectation.",
                      papszTokens[0], papszTokens[1] );
            nError = 1;
        }
        else if( EQUAL(papszTokens[0],"altitude") )
        {
            dfaltitude = CPLAtof(papszTokens[1]);
            iGeoParamsFound++;
        }
        else if( EQUAL(papszTokens[0],"near_srd") )
        {
            dfnear_srd = CPLAtof(papszTokens[1]);
            iGeoParamsFound++;
        }

        else if( EQUAL(papszTokens[0],"sample_size") )
        {
            dfsample_size = CPLAtof(papszTokens[1]);
            iGeoParamsFound++;
            iUTMParamsFound++;
        }
        else if( EQUAL(papszTokens[0],"sample_size_az") )
        {
            dfsample_size_az = CPLAtof(papszTokens[1]);
            iGeoParamsFound++;
            iUTMParamsFound++;
        }
        else if( EQUAL(papszTokens[0],"transposed") )
        {
            itransposed = atoi(papszTokens[1]);
            iGeoParamsFound++;
            iUTMParamsFound++;
        }

        CSLDestroy( papszTokens );
    }
    CSLDestroy( papszHdrLines );
/* -------------------------------------------------------------------- */
/*      Check for successful completion.                                */
/* -------------------------------------------------------------------- */
    if( nError )
    {
        CPLFree(pszWorkname);
        return nullptr;
    }

    if( nLines <= 0 || nSamples <= 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
          "Did not find valid number_lines or number_samples keywords in %s.",
                  pszWorkname );
        CPLFree(pszWorkname);
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Initialize dataset.                                             */
/* -------------------------------------------------------------------- */
    CPGDataset *poDS = new CPGDataset();

    poDS->nRasterXSize = nSamples;
    poDS->nRasterYSize = nLines;

/* -------------------------------------------------------------------- */
/*      Open the four bands.                                            */
/* -------------------------------------------------------------------- */
    static const char * const apszPolarizations[4] = { "hh", "hv", "vv", "vh" };

    const int nNameLen = static_cast<int>(strlen(pszWorkname));

    if ( EQUAL(pszWorkname+nNameLen-7,"IRC.hdr") ||
         EQUAL(pszWorkname+nNameLen-7,"IRC.img") )
    {

        AdjustFilename( &pszWorkname, "" , "img" );
        poDS->afpImage[0] = VSIFOpenL( pszWorkname, "rb" );
        if( poDS->afpImage[0] == nullptr )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Failed to open .img file: %s",
                      pszWorkname );
            CPLFree(pszWorkname);
            delete poDS;
            return nullptr;
        }
        poDS->aosImageFilenames.push_back(pszWorkname);
        for( int iBand = 0; iBand < 4; iBand++ )
        {
            SIRC_QSLCRasterBand *poBand =
                new SIRC_QSLCRasterBand( poDS, iBand+1, GDT_CFloat32 );
            poDS->SetBand( iBand+1, poBand );
            poBand->SetMetadataItem( "POLARIMETRIC_INTERP",
                                 apszPolarizations[iBand] );
        }
    }
    else
    {
        for( int iBand = 0; iBand < 4; iBand++ )
        {
            AdjustFilename( &pszWorkname, apszPolarizations[iBand], "img" );

            poDS->afpImage[iBand] = VSIFOpenL( pszWorkname, "rb" );
            if( poDS->afpImage[iBand] == nullptr )
            {
                CPLError( CE_Failure, CPLE_OpenFailed,
                          "Failed to open .img file: %s",
                          pszWorkname );
                CPLFree(pszWorkname);
                delete poDS;
                return nullptr;
            }
            poDS->aosImageFilenames.push_back(pszWorkname);

            RawRasterBand *poBand
                = new RawRasterBand( poDS, iBand+1, poDS->afpImage[iBand],
                                     0, 8, 8*nSamples,
                                     GDT_CFloat32, !CPL_IS_LSB,
                                     RawRasterBand::OwnFP::NO );
            poDS->SetBand( iBand+1, poBand );

            poBand->SetMetadataItem( "POLARIMETRIC_INTERP",
                                 apszPolarizations[iBand] );
        }
    }

    /* Set an appropriate matrix representation metadata item for the set */
    if ( poDS->GetRasterCount() == 4 ) {
        poDS->SetMetadataItem( "MATRIX_REPRESENTATION", "SCATTERING" );
    }

/* ------------------------------------------------------------------------- */
/*  Add georeferencing or pseudo-geocoding, if enough information found.     */
/* ------------------------------------------------------------------------- */
    if (iUTMParamsFound == 7)
    {
        poDS->adfGeoTransform[1] = 0.0;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = 0.0;

        double dfnorth_center;
        if (itransposed == 1)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Did not have a convair SIRC-style test dataset\n"
                    "with transposed=1 for testing.  Georeferencing may be "
                    "wrong.\n" );
            dfnorth_center = dfnorth - nSamples*dfsample_size/2.0;
            poDS->adfGeoTransform[0] = dfeast;
            poDS->adfGeoTransform[2] = dfsample_size_az;
            poDS->adfGeoTransform[3] = dfnorth;
            poDS->adfGeoTransform[4] = -1*dfsample_size;
        }
        else
        {
            dfnorth_center = dfnorth - nLines*dfsample_size/2.0;
            poDS->adfGeoTransform[0] = dfeast;
            poDS->adfGeoTransform[1] = dfsample_size_az;
            poDS->adfGeoTransform[3] = dfnorth;
            poDS->adfGeoTransform[5] = -1*dfsample_size;
        }

        OGRSpatialReference oUTM;
        if (dfnorth_center < 0)
            oUTM.SetUTM(iUTMZone, 0);
        else
            oUTM.SetUTM(iUTMZone, 1);

        /* Assuming WGS84 */
        oUTM.SetWellKnownGeogCS( "WGS84" );
        CPLFree( poDS->pszProjection );
        poDS->pszProjection = nullptr;
        oUTM.exportToWkt( &(poDS->pszProjection) );
    }
    else if (iGeoParamsFound == 5)
    {
        double dfgcpLine, dfgcpPixel, dfgcpX, dfgcpY, dftemp;

        poDS->nGCPCount = 16;
        poDS->pasGCPList = reinterpret_cast<GDAL_GCP *>(
            CPLCalloc( sizeof(GDAL_GCP), poDS->nGCPCount ) );
        GDALInitGCPs(poDS->nGCPCount, poDS->pasGCPList);

        for( int ngcp = 0; ngcp < 16; ngcp ++ )
        {
            char szID[32];

            snprintf( szID, sizeof(szID), "%d",ngcp+1);
            if (itransposed == 1)
            {
                if (ngcp < 4)
                    dfgcpPixel = 0.0;
                else if (ngcp < 8)
                    dfgcpPixel = nSamples/3.0;
                else if (ngcp < 12)
                    dfgcpPixel = 2.0*nSamples/3.0;
                else
                    dfgcpPixel = nSamples;

                dfgcpLine = nLines*( ngcp % 4 )/3.0;

                dftemp = dfnear_srd + (dfsample_size*dfgcpLine);
                /* -1 so that 0,0 maps to largest Y */
                dfgcpY = -1*sqrt( dftemp*dftemp - dfaltitude*dfaltitude );
                dfgcpX = dfgcpPixel*dfsample_size_az;
            }
            else
            {
                if (ngcp < 4)
                    dfgcpLine = 0.0;
                else if (ngcp < 8)
                    dfgcpLine = nLines/3.0;
                else if (ngcp < 12)
                    dfgcpLine = 2.0*nLines/3.0;
                else
                    dfgcpLine = nLines;

                dfgcpPixel = nSamples*( ngcp % 4 )/3.0;

                dftemp = dfnear_srd + (dfsample_size*dfgcpPixel);
                dfgcpX = sqrt( dftemp*dftemp - dfaltitude*dfaltitude );
                dfgcpY = (nLines - dfgcpLine)*dfsample_size_az;
            }
            poDS->pasGCPList[ngcp].dfGCPX = dfgcpX;
            poDS->pasGCPList[ngcp].dfGCPY = dfgcpY;
            poDS->pasGCPList[ngcp].dfGCPZ = 0.0;

            poDS->pasGCPList[ngcp].dfGCPPixel = dfgcpPixel;
            poDS->pasGCPList[ngcp].dfGCPLine = dfgcpLine;

            CPLFree(poDS->pasGCPList[ngcp].pszId);
            poDS->pasGCPList[ngcp].pszId = CPLStrdup( szID );
        }

        CPLFree(poDS->pszGCPProjection);
        poDS->pszGCPProjection = CPLStrdup(
            "LOCAL_CS[\"Ground range view / unreferenced meters\","
            "UNIT[\"Meter\",1.0]]");
    }

    CPLFree(pszWorkname);

    return poDS;
}

#ifdef notdef
GDALDataset *CPGDataset::InitializeType3Dataset( const char *pszFilename )
{
    int iBytesPerPixel = 0;
    int iInterleave = -1;
    int nLines = 0;
    int nSamples = 0;
    int nBands = 0;
    int nError = 0;

    /* Parameters in geogratis geocoded images */
    int iUTMParamsFound = 0;
    int iUTMZone = 0;
    double dfnorth = 0.0;
    double dfeast = 0.0;
    double dfOffsetX = 0.0;
    double dfOffsetY = 0.0;
    double dfxsize = 0.0;
    double dfysize = 0.0;

    char* pszWorkname = CPLStrdup(pszFilename);
    AdjustFilename( &pszWorkname, "stokes", "img_def" );
    char **papszHdrLines = CSLLoad( pszWorkname );

    for( int iLine = 0; papszHdrLines && papszHdrLines[iLine] != NULL; iLine++ )
    {
        char **papszTokens
            = CSLTokenizeString2( papszHdrLines[iLine], " \t",
                                  CSLT_HONOURSTRINGS & CSLT_ALLOWEMPTYTOKENS );

        /* Note: some cv580 file seem to have comments with #, hence the >=
         * instead of = for token checking, and the equalN for the corner.
         */

        if ( ( CSLCount( papszTokens ) >= 3 ) &&
               EQUAL(papszTokens[0],"data") &&
               EQUAL(papszTokens[1],"organization:"))
        {

            if( STARTS_WITH_CI(papszTokens[2], "BSQ") )
                iInterleave = BSQ;
            else if( STARTS_WITH_CI(papszTokens[2], "BIL") )
                iInterleave = BIL;
            else if( STARTS_WITH_CI(papszTokens[2], "BIP") )
                iInterleave = BIP;
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                  "The interleaving type of the file (%s) is not supported.",
                  papszTokens[2] );
                nError = 1;
            }
        }
        else if ( ( CSLCount( papszTokens ) >= 3 ) &&
               EQUAL(papszTokens[0],"data") &&
               EQUAL(papszTokens[1],"state:") )
        {

            if( !STARTS_WITH_CI(papszTokens[2], "RAW") &&
                !STARTS_WITH_CI(papszTokens[2], "GEO") )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "The data state of the file (%s) is not "
                          "supported.\n.  Only RAW and GEO are currently "
                          "recognized.",
                  papszTokens[2] );
                nError = 1;
            }
        }
        else if ( ( CSLCount( papszTokens ) >= 4 ) &&
               EQUAL(papszTokens[0],"data") &&
               EQUAL(papszTokens[1],"origin") &&
               EQUAL(papszTokens[2],"point:")  )
        {
          if (!STARTS_WITH_CI(papszTokens[3], "Upper_Left"))
            {
                CPLError( CE_Failure, CPLE_AppDefined,
            "Unexpected value (%s) for data origin point- expect Upper_Left.",
                  papszTokens[3] );
                nError = 1;
            }
            iUTMParamsFound++;
        }
        else if ( ( CSLCount( papszTokens ) >= 5 ) &&
               EQUAL(papszTokens[0],"map") &&
               EQUAL(papszTokens[1],"projection:") &&
               EQUAL(papszTokens[2],"UTM") &&
               EQUAL(papszTokens[3],"zone") )
        {
            iUTMZone = atoi(papszTokens[4]);
            iUTMParamsFound++;
        }
        else if ( ( CSLCount( papszTokens ) >= 4 ) &&
                 EQUAL(papszTokens[0],"project") &&
                 EQUAL(papszTokens[1],"origin:") )
        {
            dfeast = CPLAtof(papszTokens[2]);
            dfnorth = CPLAtof(papszTokens[3]);
            iUTMParamsFound+=2;
        }
        else if ( ( CSLCount( papszTokens ) >= 4 ) &&
               EQUAL(papszTokens[0],"file") &&
               EQUAL(papszTokens[1],"start:"))
        {
            dfOffsetX =  CPLAtof(papszTokens[2]);
            dfOffsetY = CPLAtof(papszTokens[3]);
            iUTMParamsFound+=2;
        }
        else if ( ( CSLCount( papszTokens ) >= 6 ) &&
               EQUAL(papszTokens[0],"pixel") &&
               EQUAL(papszTokens[1],"size") &&
               EQUAL(papszTokens[2],"on") &&
               EQUAL(papszTokens[3],"ground:"))
        {
            dfxsize = CPLAtof(papszTokens[4]);
            dfysize = CPLAtof(papszTokens[5]);
            iUTMParamsFound+=2;
        }
        else if ( ( CSLCount( papszTokens ) >= 4 ) &&
               EQUAL(papszTokens[0],"number") &&
               EQUAL(papszTokens[1],"of") &&
               EQUAL(papszTokens[2],"pixels:"))
        {
            nSamples = atoi(papszTokens[3]);
        }
        else if ( ( CSLCount( papszTokens ) >= 4 ) &&
               EQUAL(papszTokens[0],"number") &&
               EQUAL(papszTokens[1],"of") &&
               EQUAL(papszTokens[2],"lines:"))
        {
            nLines = atoi(papszTokens[3]);
        }
        else if ( ( CSLCount( papszTokens ) >= 4 ) &&
               EQUAL(papszTokens[0],"number") &&
               EQUAL(papszTokens[1],"of") &&
               EQUAL(papszTokens[2],"bands:"))
        {
            nBands = atoi(papszTokens[3]);
            if ( nBands != 16)
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Number of bands has a value %s which does not match "
                          "CPG driver\nexpectation (expect a value of 16).",
                      papszTokens[3] );
                nError = 1;
            }
        }
        else if ( ( CSLCount( papszTokens ) >= 4 ) &&
               EQUAL(papszTokens[0],"bytes") &&
               EQUAL(papszTokens[1],"per") &&
               EQUAL(papszTokens[2],"pixel:"))
        {
            iBytesPerPixel = atoi(papszTokens[3]);
            if (iBytesPerPixel != 4)
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Bytes per pixel has a value %s which does not match "
                          "CPG driver\nexpectation (expect a value of 4).",
                      papszTokens[1] );
                nError = 1;
            }
        }
        CSLDestroy( papszTokens );
    }

    CSLDestroy( papszHdrLines );

/* -------------------------------------------------------------------- */
/*      Check for successful completion.                                */
/* -------------------------------------------------------------------- */
    if( nError )
    {
        CPLFree(pszWorkname);
        return NULL;
    }

    if (!GDALCheckDatasetDimensions(nSamples, nLines) ||
        !GDALCheckBandCount(nBands, FALSE) || iInterleave == -1 ||
        iBytesPerPixel == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s is missing a required parameter (number of pixels, "
                  "number of lines,\number of bands, bytes per pixel, or "
                  "data organization).",
                  pszWorkname );
        CPLFree(pszWorkname);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Initialize dataset.                                             */
/* -------------------------------------------------------------------- */
    CPGDataset *poDS = new CPGDataset();

    poDS->nRasterXSize = nSamples;
    poDS->nRasterYSize = nLines;

    if( iInterleave == BSQ )
        poDS->nInterleave = BSQ;
    else if( iInterleave == BIL )
        poDS->nInterleave = BIL;
    else
        poDS->nInterleave = BIP;

/* -------------------------------------------------------------------- */
/*      Open the 16 bands.                                              */
/* -------------------------------------------------------------------- */

    AdjustFilename( &pszWorkname, "stokes" , "img" );
    poDS->afpImage[0] = VSIFOpenL( pszWorkname, "rb" );
    if( poDS->afpImage[0] == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to open .img file: %s",
                  pszWorkname );
        CPLFree(pszWorkname);
        delete poDS;
        return NULL;
    }
    aosImageFilenames.push_back(pszWorkname);
    for( int iBand = 0; iBand < 16; iBand++ )
    {
        CPG_STOKESRasterBand *poBand
            = new CPG_STOKESRasterBand( poDS, GDT_CFloat32,
                                        !CPL_IS_LSB );
        poDS->SetBand( iBand+1, poBand );
    }

/* -------------------------------------------------------------------- */
/*      Set appropriate MATRIX_REPRESENTATION.                          */
/* -------------------------------------------------------------------- */
    if ( poDS->GetRasterCount() == 6 ) {
        poDS->SetMetadataItem( "MATRIX_REPRESENTATION", "COVARIANCE" );
    }

/* ------------------------------------------------------------------------- */
/*  Add georeferencing, if enough information found.                         */
/* ------------------------------------------------------------------------- */
    if (iUTMParamsFound == 8)
    {
        double dfnorth_center = dfnorth - nLines*dfysize/2.0;
        poDS->adfGeoTransform[0] = dfeast + dfOffsetX;
        poDS->adfGeoTransform[1] = dfxsize;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = dfnorth + dfOffsetY;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = -1*dfysize;

        OGRSpatialReference oUTM;
        if (dfnorth_center < 0)
            oUTM.SetUTM(iUTMZone, 0);
        else
            oUTM.SetUTM(iUTMZone, 1);

        /* Assuming WGS84 */
        oUTM.SetWellKnownGeogCS( "WGS84" );
        CPLFree( poDS->pszProjection );
        poDS->pszProjection = NULL;
        oUTM.exportToWkt( &(poDS->pszProjection) );
    }

    return poDS;
}
#endif

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *CPGDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Is this a PolGASP fileset?  We expect fileset to follow         */
/*      one of these patterns:                                          */
/*               1) <stuff>hh<stuff2>.img, <stuff>hh<stuff2>.hdr,       */
/*                  <stuff>hv<stuff2>.img, <stuff>hv<stuff2>.hdr,       */
/*                  <stuff>vh<stuff2>.img, <stuff>vh<stuff2>.hdr,       */
/*                  <stuff>vv<stuff2>.img, <stuff>vv<stuff2>.hdr,       */
/*                  where <stuff> or <stuff2> should contain the        */
/*                  substring "sso" or "polgasp"                        */
/*               2) <stuff>SIRC.hdr and <stuff>SIRC.img                 */
/*               3) <stuff>.img and <stuff>.img_def                     */
/*                  where <stuff> should contain the                    */
/*                  substring "sso" or "polgasp"                        */
/* -------------------------------------------------------------------- */
    int CPGType = 0;
    if ( FindType1( poOpenInfo->pszFilename ))
      CPGType = 1;
    else if ( FindType2( poOpenInfo->pszFilename ))
      CPGType = 2;

    /* Stokes matrix convair data: not quite working yet- something
     * is wrong in the interpretation of the matrix elements in terms
     * of hh, hv, vv, vh.  Data will load if the next two lines are
     * uncommented, but values will be incorrect.  Expect C11 = hh*conj(hh),
     * C12 = hh*conj(hv), etc.  Used geogratis data in both scattering
     * matrix and stokes format for comparison.
     */
    //else if ( FindType3( poOpenInfo->pszFilename ))
    //  CPGType = 3;

    /* Set working name back to original */

    if ( CPGType == 0 )
    {
      int nNameLen = static_cast<int>(strlen(poOpenInfo->pszFilename));
      if ( (nNameLen > 8) &&
           ( ( strstr(poOpenInfo->pszFilename,"sso") != nullptr ) ||
             ( strstr(poOpenInfo->pszFilename,"polgasp") != nullptr ) ) &&
           ( EQUAL(poOpenInfo->pszFilename+nNameLen-4,"img") ||
             EQUAL(poOpenInfo->pszFilename+nNameLen-4,"hdr") ||
             EQUAL(poOpenInfo->pszFilename+nNameLen-7,"img_def") ) )
      {
        CPLError( CE_Failure, CPLE_OpenFailed,
              "Apparent attempt to open Convair PolGASP data failed as\n"
              "one or more of the required files is missing (eight files\n"
              "are expected for scattering matrix format, two for Stokes)." );
      }
      else if ( (nNameLen > 8) &&
                ( strstr(poOpenInfo->pszFilename,"SIRC") != nullptr )  &&
           ( EQUAL(poOpenInfo->pszFilename+nNameLen-4,"img") ||
             EQUAL(poOpenInfo->pszFilename+nNameLen-4,"hdr")))
      {
          CPLError( CE_Failure, CPLE_OpenFailed,
                "Apparent attempt to open SIRC Convair PolGASP data failed \n"
                "as one of the expected files is missing (hdr or img)!" );
      }
      return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The CPG driver does not support update access to existing"
                  " datasets.\n" );
        return nullptr;
    }

    /* Read the header info and create the dataset */
#ifdef notdef
    if ( CPGType < 3 )
#endif
    CPGDataset* poDS = reinterpret_cast<CPGDataset *>(
          InitializeType1Or2Dataset( poOpenInfo->pszFilename ) );
#ifdef notdef
    else
      poDS = reinterpret_cast<CPGDataset *>(
          InitializeType3Dataset( poOpenInfo->pszFilename ) );
#endif
    if( poDS == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    // Need to think about this.
    // poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

    return poDS;
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int CPGDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *CPGDataset::_GetGCPProjection()

{
  return pszGCPProjection;
}

/************************************************************************/
/*                               GetGCPs()                               */
/************************************************************************/

const GDAL_GCP *CPGDataset::GetGCPs()

{
    return pasGCPList;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *CPGDataset::_GetProjectionRef()

{
    return pszProjection;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr CPGDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform,  adfGeoTransform, sizeof(double) * 6 );
    return CE_None;
}

/************************************************************************/
/*                           SIRC_QSLCRasterBand()                      */
/************************************************************************/

SIRC_QSLCRasterBand::SIRC_QSLCRasterBand( CPGDataset *poGDSIn, int nBandIn,
                                          GDALDataType eType )

{
    poDS = poGDSIn;
    nBand = nBandIn;

    eDataType = eType;

    nBlockXSize = poGDSIn->nRasterXSize;
    nBlockYSize = 1;

    if( nBand == 1 )
        SetMetadataItem( "POLARIMETRIC_INTERP", "HH" );
    else if( nBand == 2 )
        SetMetadataItem( "POLARIMETRIC_INTERP", "HV" );
    else if( nBand == 3 )
        SetMetadataItem( "POLARIMETRIC_INTERP", "VH" );
    else if( nBand == 4 )
        SetMetadataItem( "POLARIMETRIC_INTERP", "VV" );
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

/* From: http://southport.jpl.nasa.gov/software/dcomp/dcomp.html

ysca = sqrt{ [ (Byte(2) / 254 ) + 1.5] 2Byte(1) }
Re(SHH) = byte(3) ysca/127
Im(SHH) = byte(4) ysca/127
Re(SHV) = byte(5) ysca/127
Im(SHV) = byte(6) ysca/127
Re(SVH) = byte(7) ysca/127
Im(SVH) = byte(8) ysca/127
Re(SVV) = byte(9) ysca/127
Im(SVV) = byte(10) ysca/127
*/

CPLErr SIRC_QSLCRasterBand::IReadBlock( CPL_UNUSED int nBlockXOff,
                                        int nBlockYOff,
                                        void * pImage )
{
    const int nBytesPerSample = 10;
    CPGDataset *poGDS = reinterpret_cast<CPGDataset *>( poDS );
    const int offset = nBlockXSize* nBlockYOff*nBytesPerSample;

/* -------------------------------------------------------------------- */
/*      Load all the pixel data associated with this scanline.          */
/* -------------------------------------------------------------------- */
    const int nBytesToRead = nBytesPerSample * nBlockXSize;

    GByte *pabyRecord = reinterpret_cast<GByte *>(
        CPLMalloc( nBytesToRead ) );

    if( VSIFSeekL( poGDS->afpImage[0], offset, SEEK_SET ) != 0
        || static_cast<int>( VSIFReadL(
            pabyRecord, 1, nBytesToRead, poGDS->afpImage[0] ) )
        != nBytesToRead )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Error reading %d bytes of SIRC Convair at offset %d.\n"
                  "Reading file %s failed.",
                  nBytesToRead, offset, poGDS->GetDescription() );
        CPLFree( pabyRecord );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Initialize our power table if this is our first time through.   */
/* -------------------------------------------------------------------- */
    static float afPowTable[256];
    static bool bPowTableInitialized = false;

    if( !bPowTableInitialized )
    {
        bPowTableInitialized = true;

        for( int i = 0; i < 256; i++ )
        {
            afPowTable[i] = static_cast<float>( pow( 2.0, i-128 ) );
        }
    }

/* -------------------------------------------------------------------- */
/*      Copy the desired band out based on the size of the type, and    */
/*      the interleaving mode.                                          */
/* -------------------------------------------------------------------- */
    for( int iX = 0; iX < nBlockXSize; iX++ )
    {
        unsigned char *pabyGroup = pabyRecord + iX * nBytesPerSample;
        const signed char *Byte = reinterpret_cast<signed char *>(
            pabyGroup - 1 ); /* A ones based alias */

        /* coverity[tainted_data] */
        const double dfScale
            = sqrt( (static_cast<double>(Byte[2]) / 254 + 1.5) * afPowTable[Byte[1] + 128] );

        float *pafImage = reinterpret_cast<float *>( pImage );

        if( nBand == 1 )
        {
            const float fReSHH = static_cast<float>(Byte[3] * dfScale / 127.0);
            const float fImSHH = static_cast<float>(Byte[4] * dfScale / 127.0);

            pafImage[iX*2  ] = fReSHH;
            pafImage[iX*2+1] = fImSHH;
        }
        else if( nBand == 2 )
        {
            const float fReSHV = static_cast<float>(Byte[5] * dfScale / 127.0);
            const float fImSHV = static_cast<float>(Byte[6] * dfScale / 127.0);

            pafImage[iX*2  ] = fReSHV;
            pafImage[iX*2+1] = fImSHV;
        }
        else if( nBand == 3 )
        {
            const float fReSVH = static_cast<float>(Byte[7] * dfScale / 127.0);
            const float fImSVH = static_cast<float>(Byte[8] * dfScale / 127.0);

            pafImage[iX*2  ] = fReSVH;
            pafImage[iX*2+1] = fImSVH;
        }
        else if( nBand == 4 )
        {
            const float fReSVV = static_cast<float>(Byte[9] * dfScale / 127.0);
            const float fImSVV = static_cast<float>(Byte[10]* dfScale / 127.0);

            pafImage[iX*2  ] = fReSVV;
            pafImage[iX*2+1] = fImSVV;
        }
    }

    CPLFree( pabyRecord );

    return CE_None;
}

/************************************************************************/
/*                        CPG_STOKESRasterBand()                        */
/************************************************************************/

CPG_STOKESRasterBand::CPG_STOKESRasterBand( GDALDataset *poDSIn,
                                            GDALDataType eType,
                                            int bNativeOrderIn ) :
    bNativeOrder(bNativeOrderIn)
{
    static const char * const apszPolarizations[16] = {
        "Covariance_11",
        "Covariance_12",
        "Covariance_13",
        "Covariance_14",
        "Covariance_21",
        "Covariance_22",
        "Covariance_23",
        "Covariance_24",
        "Covariance_31",
        "Covariance_32",
        "Covariance_33",
        "Covariance_34",
        "Covariance_41",
        "Covariance_42",
        "Covariance_43",
        "Covariance_44" };

    poDS = poDSIn;
    eDataType = eType;

    nBlockXSize = poDSIn->GetRasterXSize();
    nBlockYSize = 1;

    SetMetadataItem( "POLARIMETRIC_INTERP", apszPolarizations[nBand-1] );
    SetDescription( apszPolarizations[nBand-1] );
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

/* Convert from Stokes to Covariance representation */

CPLErr CPG_STOKESRasterBand::IReadBlock( CPL_UNUSED int nBlockXOff,
                                         int nBlockYOff,
                                         void * pImage )

{
    CPLAssert( nBlockXOff == 0 );

    int m11, /* m12, */ m13, m14, /* m21, */ m22, m23, m24, step;
    int m31, m32, m33, m34, m41, m42, m43, m44;
    CPGDataset *poGDS = reinterpret_cast<CPGDataset *>( poDS );

    CPLErr eErr = poGDS->LoadStokesLine(nBlockYOff, bNativeOrder);
    if( eErr != CE_None )
        return eErr;

    float *M = poGDS->padfStokesMatrix;
    float *pafLine = reinterpret_cast<float *>( pImage );

    if ( poGDS->nInterleave == BIP)
    {
        step = 16;
        m11 = M11;
        // m12 = M12;
        m13 = M13;
        m14 = M14;
        // m21 = M21;
        m22 = M22;
        m23 = M23;
        m24 = M24;
        m31 = M31;
        m32 = M32;
        m33 = M33;
        m34 = M34;
        m41 = M41;
        m42 = M42;
        m43 = M43;
        m44 = M44;
    }
    else
    {
        step = 1;
        m11=0;
        // m12=nRasterXSize;
        m13=nRasterXSize*2;
        m14=nRasterXSize*3;
        // m21=nRasterXSize*4;
        m22=nRasterXSize*5;
        m23=nRasterXSize*6;
        m24=nRasterXSize*7;
        m31=nRasterXSize*8;
        m32=nRasterXSize*9;
        m33=nRasterXSize*10;
        m34=nRasterXSize*11;
        m41=nRasterXSize*12;
        m42=nRasterXSize*13;
        m43=nRasterXSize*14;
        m44=nRasterXSize*15;
    }
    if ( nBand == 1 ) /* C11 */
    {
        for ( int iPixel = 0; iPixel < nRasterXSize; iPixel++ )
        {
            pafLine[iPixel*2+0] = M[m11]-M[m22]-M[m33]+M[m44];
            pafLine[iPixel*2+1] = 0.0;
            m11 += step;
            m22 += step;
            m33 += step;
            m44 += step;
        }
    }
    else if ( nBand == 2 ) /* C12 */
    {
        for ( int iPixel = 0; iPixel < nRasterXSize; iPixel++ )
        {
            pafLine[iPixel*2+0] = M[m13]-M[m23];
            pafLine[iPixel*2+1] = M[m14]-M[m24];
            m13 += step;
            m23 += step;
            m14 += step;
            m24 += step;
        }
    }
    else if ( nBand == 3 ) /* C13 */
    {
        for ( int iPixel = 0; iPixel < nRasterXSize; iPixel++ )
        {
            pafLine[iPixel*2+0] = M[m33]-M[m44];
            pafLine[iPixel*2+1] = M[m43]+M[m34];
            m33 += step;
            m44 += step;
            m43 += step;
            m34 += step;
        }
    }
    else if ( nBand == 4 ) /* C14 */
    {
        for ( int iPixel = 0; iPixel < nRasterXSize; iPixel++ )
        {
            pafLine[iPixel*2+0] = M[m31]-M[m32];
            pafLine[iPixel*2+1] = M[m41]-M[m42];
            m31 += step;
            m32 += step;
            m41 += step;
            m42 += step;
        }
    }
    else if ( nBand == 5 ) /* C21 */
    {
        for ( int iPixel = 0; iPixel < nRasterXSize; iPixel++ )
        {
            pafLine[iPixel*2+0] = M[m13]-M[m23];
            pafLine[iPixel*2+1] = M[m24]-M[m14];
            m13 += step;
            m23 += step;
            m14 += step;
            m24 += step;
        }
    }
    else if ( nBand == 6 ) /* C22 */
    {
        for ( int iPixel = 0; iPixel < nRasterXSize; iPixel++ )
        {
            pafLine[iPixel*2+0] = M[m11]+M[m22]-M[m33]-M[m44];
            pafLine[iPixel*2+1] = 0.0;
            m11 += step;
            m22 += step;
            m33 += step;
            m44 += step;
        }
    }
    else if ( nBand == 7 ) /* C23 */
    {
        for ( int iPixel = 0; iPixel < nRasterXSize; iPixel++ )
        {
            pafLine[iPixel*2+0] = M[m31]+M[m32];
            pafLine[iPixel*2+1] = M[m41]+M[m42];
            m31 += step;
            m32 += step;
            m41 += step;
            m42 += step;
        }
    }
    else if ( nBand == 8 ) /* C24 */
    {
        for ( int iPixel = 0; iPixel < nRasterXSize; iPixel++ )
        {
            pafLine[iPixel*2+0] = M[m33]+M[m44];
            pafLine[iPixel*2+1] = M[m43]-M[m34];
            m33 += step;
            m44 += step;
            m43 += step;
            m34 += step;
        }
    }
    else if ( nBand == 9 ) /* C31 */
    {
        for ( int iPixel = 0; iPixel < nRasterXSize; iPixel++ )
        {
            pafLine[iPixel*2+0] = M[m33]-M[m44];
            pafLine[iPixel*2+1] = -1*M[m43]-M[m34];
            m33 += step;
            m44 += step;
            m43 += step;
            m34 += step;
        }
    }
    else if ( nBand == 10 ) /* C32 */
    {
        for ( int iPixel = 0; iPixel < nRasterXSize; iPixel++ )
        {
            pafLine[iPixel*2+0] = M[m31]+M[m32];
            pafLine[iPixel*2+1] = -1*M[m41]-M[m42];
            m31 += step;
            m32 += step;
            m41 += step;
            m42 += step;
        }
    }
    else if ( nBand == 11 ) /* C33 */
    {
        for ( int iPixel = 0; iPixel < nRasterXSize; iPixel++ )
        {
            pafLine[iPixel*2+0] = M[m11]+M[m22]+M[m33]+M[m44];
            pafLine[iPixel*2+1] = 0.0;
            m11 += step;
            m22 += step;
            m33 += step;
            m44 += step;
        }
    }
    else if ( nBand == 12 ) /* C34 */
    {
        for ( int iPixel = 0; iPixel < nRasterXSize; iPixel++ )
        {
            pafLine[iPixel*2+0] = M[m13]-M[m23];
            pafLine[iPixel*2+1] = -1*M[m14]-M[m24];
            m13 += step;
            m23 += step;
            m14 += step;
            m24 += step;
        }
    }
    else if ( nBand == 13 ) /* C41 */
    {
        for ( int iPixel = 0; iPixel < nRasterXSize; iPixel++ )
        {
            pafLine[iPixel*2+0] = M[m31]-M[m32];
            pafLine[iPixel*2+1] = M[m42]-M[m41];
            m31 += step;
            m32 += step;
            m41 += step;
            m42 += step;
        }
    }
    else if ( nBand == 14 ) /* C42 */
    {
        for ( int iPixel = 0; iPixel < nRasterXSize; iPixel++ )
        {
            pafLine[iPixel*2+0] = M[m33]+M[m44];
            pafLine[iPixel*2+1] = M[m34]-M[m43];
            m33 += step;
            m44 += step;
            m43 += step;
            m34 += step;
        }
    }
    else if ( nBand == 15 ) /* C43 */
    {
        for ( int iPixel = 0; iPixel < nRasterXSize; iPixel++ )
        {
            pafLine[iPixel*2+0] = M[m13]-M[m23];
            pafLine[iPixel*2+1] = M[m14]+M[m24];
            m13 += step;
            m23 += step;
            m14 += step;
            m24 += step;
        }
    }
    else /* C44 */
    {
        for ( int iPixel = 0; iPixel < nRasterXSize; iPixel++ )
        {
            pafLine[iPixel*2+0] = M[m11]-M[m22]+M[m33]-M[m44];
            pafLine[iPixel*2+1] = 0.0;
            m11 += step;
            m22 += step;
            m33 += step;
            m44 += step;
        }
    }

    return CE_None;
}

/************************************************************************/
/*                         GDALRegister_CPG()                           */
/************************************************************************/

void GDALRegister_CPG()

{
    if( GDALGetDriverByName( "CPG" ) != nullptr )
      return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "CPG" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Convair PolGASP" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = CPGDataset::Open;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
