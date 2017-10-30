/******************************************************************************
 *
 * Project:  SRTM HGT Driver
 * Purpose:  SRTM HGT File Read Support.
 *           http://dds.cr.usgs.gov/srtm/version2_1/Documentation/SRTM_Topo.pdf
 *           http://www2.jpl.nasa.gov/srtm/faq.html
 *           http://dds.cr.usgs.gov/srtm/version2_1
 * Authors:  Michael Mazzella, Even Rouault
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_port.h"
#include "cpl_string.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "ogr_spatialref.h"

#include <cmath>

static const GInt16 SRTMHG_NODATA_VALUE = -32768;

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                              SRTMHGTDataset                          */
/* ==================================================================== */
/************************************************************************/

class SRTMHGTRasterBand;

class SRTMHGTDataset : public GDALPamDataset
{
    friend class SRTMHGTRasterBand;

    VSILFILE*  fpImage;
    double adfGeoTransform[6];
    GInt16* panBuffer;

  public:
    SRTMHGTDataset();
    virtual ~SRTMHGTDataset();

    virtual const char *GetProjectionRef(void) override;
    virtual CPLErr GetGeoTransform(double*) override;

    static int Identify( GDALOpenInfo * poOpenInfo );
    static GDALDataset* Open(GDALOpenInfo*);
    static GDALDataset* CreateCopy( const char * pszFilename, GDALDataset *poSrcDS,
                                    int bStrict, char ** papszOptions,
                                    GDALProgressFunc pfnProgress, void * pProgressData );
};

/************************************************************************/
/* ==================================================================== */
/*                            SRTMHGTRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class SRTMHGTRasterBand : public GDALPamRasterBand
{
    friend class SRTMHGTDataset;

    int         bNoDataSet;
    double      dfNoDataValue;

  public:
    SRTMHGTRasterBand(SRTMHGTDataset*, int);

    virtual CPLErr IReadBlock(int, int, void*) override;
    virtual CPLErr IWriteBlock(int nBlockXOff, int nBlockYOff, void* pImage) override;

    virtual GDALColorInterp GetColorInterpretation() override;

    virtual double  GetNoDataValue( int *pbSuccess = NULL ) override;

    virtual const char* GetUnitType() override { return "m"; }
};

/************************************************************************/
/*                           SRTMHGTRasterBand()                            */
/************************************************************************/

SRTMHGTRasterBand::SRTMHGTRasterBand( SRTMHGTDataset* poDSIn, int nBandIn ) :
    bNoDataSet(TRUE),
    dfNoDataValue(SRTMHG_NODATA_VALUE)
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType = GDT_Int16;
    nBlockXSize = poDSIn->nRasterXSize;
    nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr SRTMHGTRasterBand::IReadBlock(int /*nBlockXOff*/, int nBlockYOff,
                                     void* pImage)
{
  SRTMHGTDataset* poGDS = reinterpret_cast<SRTMHGTDataset *>( poDS );

/* -------------------------------------------------------------------- */
/*      Load the desired data into the working buffer.                  */
/* -------------------------------------------------------------------- */
  VSIFSeekL(poGDS->fpImage, nBlockYOff*nBlockXSize*2, SEEK_SET);
  VSIFReadL((unsigned char*)pImage, nBlockXSize, 2, poGDS->fpImage);
#ifdef CPL_LSB
  GDALSwapWords(pImage, 2, nBlockXSize, 2);
#endif

  return CE_None;
}

/************************************************************************/
/*                             IWriteBlock()                            */
/************************************************************************/

CPLErr SRTMHGTRasterBand::IWriteBlock(int /*nBlockXOff*/, int nBlockYOff, void* pImage)
{
    SRTMHGTDataset* poGDS = reinterpret_cast<SRTMHGTDataset *>( poDS );

    if( poGDS->eAccess != GA_Update )
        return CE_Failure;

    VSIFSeekL(poGDS->fpImage, nBlockYOff*nBlockXSize*2, SEEK_SET);

#ifdef CPL_LSB
    memcpy(poGDS->panBuffer, pImage, nBlockXSize*sizeof(GInt16));
    GDALSwapWords(poGDS->panBuffer, 2, nBlockXSize, 2);
    VSIFWriteL( reinterpret_cast<unsigned char *>( poGDS->panBuffer ),
                nBlockXSize, 2, poGDS->fpImage );
#else
    VSIFWriteL( reinterpret_cast<unsigned char *>( pImage ),
                nBlockXSize, 2, poGDS->fpImage );
#endif

    return CE_None;
}
/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double SRTMHGTRasterBand::GetNoDataValue( int * pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = bNoDataSet;

    return dfNoDataValue;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp SRTMHGTRasterBand::GetColorInterpretation()
{
  return GCI_Undefined;
}

/************************************************************************/
/* ==================================================================== */
/*                             SRTMHGTDataset                               */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            SRTMHGTDataset()                              */
/************************************************************************/

SRTMHGTDataset::SRTMHGTDataset() :
    fpImage(NULL),
    panBuffer(NULL)
{
  adfGeoTransform[0] = 0.0;
  adfGeoTransform[1] = 1.0;
  adfGeoTransform[2] = 0.0;
  adfGeoTransform[3] = 0.0;
  adfGeoTransform[4] = 0.0;
  adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                           ~SRTMHGTDataset()                            */
/************************************************************************/

SRTMHGTDataset::~SRTMHGTDataset()
{
  FlushCache();
  if(fpImage != NULL)
    VSIFCloseL(fpImage);
  CPLFree(panBuffer);
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr SRTMHGTDataset::GetGeoTransform(double * padfTransform)
{
  memcpy(padfTransform, adfGeoTransform, sizeof(double)*6);
  return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *SRTMHGTDataset::GetProjectionRef()

{
        if (CPLTestBool( CPLGetConfigOption("REPORT_COMPD_CS", "NO") ) )
        {
                return "COMPD_CS[\"WGS 84 + EGM96 geoid height\", GEOGCS[\"WGS 84\", DATUM[\"WGS_1984\", SPHEROID[\"WGS 84\",6378137,298.257223563, AUTHORITY[\"EPSG\",\"7030\"]], AUTHORITY[\"EPSG\",\"6326\"]], PRIMEM[\"Greenwich\",0, AUTHORITY[\"EPSG\",\"8901\"]], UNIT[\"degree\",0.0174532925199433, AUTHORITY[\"EPSG\",\"9122\"]], AUTHORITY[\"EPSG\",\"4326\"]], VERT_CS[\"EGM96 geoid height\", VERT_DATUM[\"EGM96 geoid\",2005, AUTHORITY[\"EPSG\",\"5171\"]], UNIT[\"metre\",1, AUTHORITY[\"EPSG\",\"9001\"]], AXIS[\"Up\",UP], AUTHORITY[\"EPSG\",\"5773\"]]]";

        }
        else
        {
            return SRS_WKT_WGS84;
        }
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int SRTMHGTDataset::Identify( GDALOpenInfo * poOpenInfo )

{
  const char* fileName = CPLGetFilename(poOpenInfo->pszFilename);
  if( strlen(fileName) < 11 || fileName[7] != '.' )
    return FALSE;
  if( !STARTS_WITH(fileName, "/vsizip/") &&
      EQUAL(fileName + strlen(fileName) - strlen(".hgt.zip"), ".hgt.zip") )
  {
    CPLString osNewName("/vsizip/");
    osNewName += poOpenInfo->pszFilename;
    osNewName += "/";
    osNewName += CPLString(fileName).substr(0, 7);
    osNewName += ".hgt";
    GDALOpenInfo oOpenInfo(osNewName, GA_ReadOnly);
    return Identify(&oOpenInfo);
  }

  if( !EQUAL(fileName + strlen(fileName) - strlen(".hgt"), ".hgt") &&
      !EQUAL(fileName + strlen(fileName) - strlen(".hgt.gz"), ".hgt.gz") )
    return FALSE;

/* -------------------------------------------------------------------- */
/*      We check the file size to see if it is                          */
/*      SRTM1 (below or above lat 50) or SRTM 3                         */
/* -------------------------------------------------------------------- */
  VSIStatBufL fileStat;

  if(VSIStatL(poOpenInfo->pszFilename, &fileStat) != 0)
      return FALSE;
  if(fileStat.st_size != 3601 * 3601 * 2 &&
     fileStat.st_size != 1801 * 3601 * 2 &&
     fileStat.st_size != 1201 * 1201 * 2 )
      return FALSE;

  return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset* SRTMHGTDataset::Open(GDALOpenInfo* poOpenInfo)
{
  if (!Identify(poOpenInfo))
      return NULL;

  const char* fileName = CPLGetFilename(poOpenInfo->pszFilename);
  if( !STARTS_WITH(fileName, "/vsizip/") &&
      EQUAL(fileName + strlen(fileName) - strlen(".hgt.zip"), ".hgt.zip") )
  {
      CPLString osFilename ("/vsizip/");
      osFilename += poOpenInfo->pszFilename;
      osFilename += "/";
      osFilename += CPLString(fileName).substr(0, 7);
      osFilename += ".hgt";
      GDALOpenInfo oOpenInfo(osFilename, poOpenInfo->eAccess);
      GDALDataset* poDS = Open(&oOpenInfo);
      if( poDS != NULL )
      {
          // override description with the main one
          poDS->SetDescription(poOpenInfo->pszFilename);
      }
      return poDS;
  }

  char latLonValueString[4];
  memset(latLonValueString, 0, 4);
  strncpy(latLonValueString, &fileName[1], 2);
  int southWestLat = atoi(latLonValueString);
  memset(latLonValueString, 0, 4);
  // cppcheck-suppress redundantCopy
  strncpy(latLonValueString, &fileName[4], 3);
  int southWestLon = atoi(latLonValueString);

  if(fileName[0] == 'N' || fileName[0] == 'n')
    /*southWestLat = southWestLat */;
  else if(fileName[0] == 'S' || fileName[0] == 's')
    southWestLat = southWestLat * -1;
  else
    return NULL;

  if(fileName[3] == 'E' || fileName[3] == 'e')
    /*southWestLon = southWestLon */;
  else if(fileName[3] == 'W' || fileName[3] == 'w')
    southWestLon = southWestLon * -1;
  else
    return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
  SRTMHGTDataset* poDS  = new SRTMHGTDataset();

  poDS->fpImage = poOpenInfo->fpL;
  poOpenInfo->fpL = NULL;

  VSIStatBufL fileStat;
  if(VSIStatL(poOpenInfo->pszFilename, &fileStat) != 0)
  {
      delete poDS;
      return NULL;
  }

  int numPixels_x, numPixels_y;

  switch (fileStat.st_size) {
  case 3601 * 3601 * 2:
    numPixels_x = numPixels_y = 3601;
    break;
  case 1801 * 3601 * 2:
    numPixels_x = 1801;
    numPixels_y = 3601;
    break;
  case 1201 * 1201 * 2:
    numPixels_x = numPixels_y = 1201;
    break;
  default:
    numPixels_x = numPixels_y = 0;
    break;
  }

  poDS->eAccess = poOpenInfo->eAccess;
#ifdef CPL_LSB
  if(poDS->eAccess == GA_Update)
  {
      poDS->panBuffer
          = reinterpret_cast<GInt16 *>( CPLMalloc(numPixels_x * sizeof(GInt16)) );
  }
#endif

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
  poDS->nRasterXSize = numPixels_x;
  poDS->nRasterYSize = numPixels_y;
  poDS->nBands = 1;

  poDS->adfGeoTransform[0] = southWestLon - 0.5 / (numPixels_x - 1);
  poDS->adfGeoTransform[1] = 1.0 / (numPixels_x-1);
  poDS->adfGeoTransform[2] = 0.0;
  poDS->adfGeoTransform[3] = southWestLat + 1 + 0.5 / (numPixels_y - 1);
  poDS->adfGeoTransform[4] = 0.0;
  poDS->adfGeoTransform[5] = -1.0 / (numPixels_y-1);

  poDS->SetMetadataItem( GDALMD_AREA_OR_POINT, GDALMD_AOP_POINT );

/* -------------------------------------------------------------------- */
/*      Create band information object.                                 */
/* -------------------------------------------------------------------- */
  SRTMHGTRasterBand* tmpBand = new SRTMHGTRasterBand(poDS, 1);
  poDS->SetBand(1, tmpBand);

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
  poDS->SetDescription(poOpenInfo->pszFilename);
  poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Support overviews.                                              */
/* -------------------------------------------------------------------- */
  poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

  return poDS;
}

/************************************************************************/
/*                              CreateCopy()                            */
/************************************************************************/

GDALDataset * SRTMHGTDataset::CreateCopy( const char * pszFilename,
                                          GDALDataset *poSrcDS,
                                          int bStrict,
                                          char ** /* papszOptions*/,
                                          GDALProgressFunc pfnProgress,
                                          void * pProgressData )
{
/* -------------------------------------------------------------------- */
/*      Some some rudimentary checks                                    */
/* -------------------------------------------------------------------- */
    const int nBands = poSrcDS->GetRasterCount();
    if (nBands == 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "SRTMHGT driver does not support source dataset with zero band.\n");
        return NULL;
    }
    else if (nBands != 1)
    {
        CPLError( (bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported,
                  "SRTMHGT driver only uses the first band of the dataset.\n");
        if (bStrict)
            return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Checks the input SRS                                            */
/* -------------------------------------------------------------------- */
    OGRSpatialReference ogrsr_input;
    char* c = const_cast<char *>( poSrcDS->GetProjectionRef() );
    ogrsr_input.importFromWkt(&c);

    OGRSpatialReference ogrsr_wgs84;
    ogrsr_wgs84.SetWellKnownGeogCS( "WGS84" );

    if ( ogrsr_input.IsSameGeogCS(&ogrsr_wgs84) == FALSE)
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "The source projection coordinate system is %s. Only WGS 84 "
                  "is supported.\nThe SRTMHGT driver will generate a file as "
                  "if the source was WGS 84 projection coordinate system.",
                  poSrcDS->GetProjectionRef() );
    }

/* -------------------------------------------------------------------- */
/*      Work out the LL origin.                                         */
/* -------------------------------------------------------------------- */
    double adfGeoTransform[6];
    if (poSrcDS->GetGeoTransform( adfGeoTransform ) != CE_None)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Source image must have a geo transform matrix.");
        return NULL;
    }

    const int nLLOriginLat = static_cast<int>(
        std::floor(adfGeoTransform[3]
              + poSrcDS->GetRasterYSize() * adfGeoTransform[5] + 0.5) );

    int nLLOriginLong = static_cast<int>(
        std::floor(adfGeoTransform[0] + 0.5) );

    if (std::abs(nLLOriginLat - (
            adfGeoTransform[3] + (poSrcDS->GetRasterYSize() - 0.5 )
            * adfGeoTransform[5] ) ) > 1e-10 ||
        std::abs(nLLOriginLong - (
            adfGeoTransform[0] + 0.5 * adfGeoTransform[1])) > 1e-10 )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
               "The corner coordinates of the source are not properly "
               "aligned on plain latitude/longitude boundaries.");
    }

/* -------------------------------------------------------------------- */
/*      Check image dimensions.                                         */
/* -------------------------------------------------------------------- */
    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();

    if (!((nXSize == 1201 && nYSize == 1201) ||
          (nXSize == 3601 && nYSize == 3601) ||
          (nXSize == 1801 && nYSize == 3601)))
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Image dimensions should be 1201x1201, 3601x3601 or 1801x3601.");
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Check filename.                                                 */
/* -------------------------------------------------------------------- */
    char expectedFileName[12];

    CPLsnprintf(expectedFileName, sizeof(expectedFileName), "%c%02d%c%03d.HGT",
             (nLLOriginLat >= 0) ? 'N' : 'S',
             (nLLOriginLat >= 0) ? nLLOriginLat : -nLLOriginLat,
             (nLLOriginLong >= 0) ? 'E' : 'W',
             (nLLOriginLong >= 0) ? nLLOriginLong : -nLLOriginLong);

    if (!EQUAL(expectedFileName, CPLGetFilename(pszFilename)))
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Expected output filename is %s.", expectedFileName);
    }

/* -------------------------------------------------------------------- */
/*      Write output file.                                              */
/* -------------------------------------------------------------------- */
    VSILFILE* fp = VSIFOpenL(pszFilename, "wb");
    if (fp == NULL)
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Cannot create file %s", pszFilename );
        return NULL;
    }

    GInt16* panData
        = reinterpret_cast<GInt16 *>( CPLMalloc(sizeof(GInt16) * nXSize) );
    GDALRasterBand* poSrcBand = poSrcDS->GetRasterBand(1);

    int bSrcBandHasNoData;
    double srcBandNoData = poSrcBand->GetNoDataValue(&bSrcBandHasNoData);

    for( int iY = 0; iY < nYSize; iY++ )
    {
        if( poSrcBand->RasterIO( GF_Read, 0, iY, nXSize, 1,
                                 reinterpret_cast<void *>( panData ), nXSize, 1,
                                 GDT_Int16, 0, 0, NULL ) != CE_None )
        {
            VSIFCloseL(fp);
            CPLFree( panData );
            return NULL;
        }

        /* Translate nodata values */
        if (bSrcBandHasNoData && srcBandNoData != SRTMHG_NODATA_VALUE)
        {
            for( int iX = 0; iX < nXSize; iX++ )
            {
                if (panData[iX] == srcBandNoData)
                    panData[iX] = SRTMHG_NODATA_VALUE;
            }
        }

#ifdef CPL_LSB
        GDALSwapWords(panData, 2, nXSize, 2);
#endif

        if( VSIFWriteL( panData,sizeof(GInt16) * nXSize,1,fp ) != 1)
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Failed to write line %d in SRTMHGT dataset.\n",
                      iY );
            VSIFCloseL(fp);
            CPLFree( panData );
            return NULL;
        }

        if( pfnProgress && !pfnProgress( (iY+1) / static_cast<double>( nYSize ),
                                         NULL, pProgressData ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt,
                        "User terminated CreateCopy()" );
            VSIFCloseL(fp);
            CPLFree( panData );
            return NULL;
        }
    }

    CPLFree( panData );
    VSIFCloseL(fp);

/* -------------------------------------------------------------------- */
/*      Reopen and copy missing information into a PAM file.            */
/* -------------------------------------------------------------------- */
    GDALPamDataset *poDS = reinterpret_cast<GDALPamDataset *>(
        GDALOpen( pszFilename, GA_ReadOnly ) );

    if( poDS )
        poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT);

    return poDS;
}

/************************************************************************/
/*                         GDALRegister_SRTMHGT()                       */
/************************************************************************/
void GDALRegister_SRTMHGT()
{
    if( GDALGetDriverByName( "SRTMHGT" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "SRTMHGT" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "SRTMHGT File Format");
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "hgt");
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                               "frmt_various.html#SRTMHGT" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte Int16 UInt16" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnIdentify = SRTMHGTDataset::Identify;
    poDriver->pfnOpen = SRTMHGTDataset::Open;
    poDriver->pfnCreateCopy = SRTMHGTDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
