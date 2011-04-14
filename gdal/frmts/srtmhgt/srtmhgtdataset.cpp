/******************************************************************************
 * $Id$
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

#include "gdal_pam.h"
#include "cpl_port.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"

#define SRTMHG_NODATA_VALUE -32768

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_SRTMHGT(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*				SRTMHGTDataset				*/
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
    ~SRTMHGTDataset();

    virtual const char *GetProjectionRef(void);
    virtual CPLErr GetGeoTransform(double*);
    
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

    int	        bNoDataSet;
    double	dfNoDataValue;

  public:
    SRTMHGTRasterBand(SRTMHGTDataset*, int);

    virtual CPLErr IReadBlock(int, int, void*);
    virtual CPLErr IWriteBlock(int nBlockXOff, int nBlockYOff, void* pImage);

    virtual GDALColorInterp GetColorInterpretation();

    virtual double  GetNoDataValue( int *pbSuccess = NULL );

    virtual const char* GetUnitType() { return "m"; }
};

/************************************************************************/
/*                           SRTMHGTRasterBand()                            */
/************************************************************************/

SRTMHGTRasterBand::SRTMHGTRasterBand(SRTMHGTDataset* poDS, int nBand)
{
  this->poDS = poDS;
  this->nBand = nBand;
  eDataType = GDT_Int16;
  nBlockXSize = poDS->nRasterXSize;
  nBlockYSize = 1;
  bNoDataSet = TRUE;
  dfNoDataValue = SRTMHG_NODATA_VALUE;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr SRTMHGTRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                     void* pImage)
{
  SRTMHGTDataset* poGDS = (SRTMHGTDataset*) poDS;

  CPLAssert(nBlockXOff == 0);
  if(nBlockXOff != 0)
  {
      CPLError(CE_Failure, CPLE_NotSupported,
               "unhandled nBlockXOff value : %d", nBlockXOff);
      return CE_Failure;
  }

  if((poGDS == NULL) || (poGDS->fpImage == NULL))
    return CE_Failure;

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

CPLErr SRTMHGTRasterBand::IWriteBlock(int nBlockXOff, int nBlockYOff, void* pImage)
{
    SRTMHGTDataset* poGDS = (SRTMHGTDataset*) poDS;

    CPLAssert(nBlockXOff == 0);
    if(nBlockXOff != 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "unhandled nBlockXOff value : %d", nBlockXOff);
        return CE_Failure;
    }

    if((poGDS == NULL) || (poGDS->fpImage == NULL) || (poGDS->eAccess != GA_Update))
        return CE_Failure;

    VSIFSeekL(poGDS->fpImage, nBlockYOff*nBlockXSize*2, SEEK_SET);

#ifdef CPL_LSB
    memcpy(poGDS->panBuffer, pImage, nBlockXSize*sizeof(GInt16));
    GDALSwapWords(poGDS->panBuffer, 2, nBlockXSize, 2);
    VSIFWriteL((unsigned char*)poGDS->panBuffer, nBlockXSize, 2, poGDS->fpImage);
#else
    VSIFWriteL((unsigned char*)pImage, nBlockXSize, 2, poGDS->fpImage);
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

SRTMHGTDataset::SRTMHGTDataset()
{
  adfGeoTransform[0] = 0.0;
  adfGeoTransform[1] = 1.0;
  adfGeoTransform[2] = 0.0;
  adfGeoTransform[3] = 0.0;
  adfGeoTransform[4] = 0.0;
  adfGeoTransform[5] = 1.0;
  fpImage = NULL;
  panBuffer = NULL;
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
    return( SRS_WKT_WGS84 );
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int SRTMHGTDataset::Identify( GDALOpenInfo * poOpenInfo )

{
  const char* fileName = CPLGetFilename(poOpenInfo->pszFilename);
  if( strlen(fileName) < 11 || !EQUALN(&fileName[7], ".hgt", 4) )
    return FALSE;

/* -------------------------------------------------------------------- */
/*	We check the file size to see if it is 25,934,402 bytes	        */
/*	(SRTM 1) or 2,884,802 bytes (SRTM 3)				*/    
/* -------------------------------------------------------------------- */
  VSIStatBufL fileStat;

  if(VSIStatL(poOpenInfo->pszFilename, &fileStat) != 0)
      return FALSE;
  if(fileStat.st_size != 25934402 && fileStat.st_size != 2884802)
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

  char latLonValueString[4];
  memset(latLonValueString, 0, 4);
  strncpy(latLonValueString, &fileName[1], 2);
  int southWestLat = atoi(latLonValueString);
  memset(latLonValueString, 0, 4);
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
  SRTMHGTDataset* poDS;

  poDS = new SRTMHGTDataset();

/* -------------------------------------------------------------------- */
/*      Open the file using the large file api.                         */
/* -------------------------------------------------------------------- */
  poDS->fpImage = VSIFOpenL(poOpenInfo->pszFilename, (poOpenInfo->eAccess == GA_Update) ? "rb+" : "rb");
  if(poDS->fpImage == NULL)
  {
    CPLError(CE_Failure, CPLE_OpenFailed, "VSIFOpenL(%s) failed unexpectedly in srtmhgtdataset.cpp", poOpenInfo->pszFilename);
    return NULL;
  }

  VSIStatBufL fileStat;
  if(VSIStatL(poOpenInfo->pszFilename, &fileStat) != 0)
  {
      return NULL;
  }
  int numPixels = (fileStat.st_size == 25934402) ? 3601 : /* 2884802 */ 1201;

  poDS->eAccess = poOpenInfo->eAccess;
#ifdef CPL_LSB
  if(poDS->eAccess == GA_Update)
  {
      poDS->panBuffer = (GInt16*) CPLMalloc(numPixels * sizeof(GInt16));
  }
#endif

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
  poDS->nRasterXSize = numPixels;
  poDS->nRasterYSize = numPixels;
  poDS->nBands = 1;

  poDS->adfGeoTransform[0] = southWestLon - 0.5 / (numPixels - 1);
  poDS->adfGeoTransform[1] = 1.0 / (numPixels-1);
  poDS->adfGeoTransform[2] = 0.0000000000;
  poDS->adfGeoTransform[3] = southWestLat + 1 + 0.5 / (numPixels - 1);
  poDS->adfGeoTransform[4] = 0.0000000000;
  poDS->adfGeoTransform[5] = -1.0 / (numPixels-1);

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

GDALDataset * SRTMHGTDataset::CreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                                          int bStrict, char ** papszOptions, 
                                          GDALProgressFunc pfnProgress, void * pProgressData )

{
    int  nBands = poSrcDS->GetRasterCount();
    int  nXSize = poSrcDS->GetRasterXSize();
    int  nYSize = poSrcDS->GetRasterYSize();

    if( pfnProgress && !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Some some rudimentary checks                                    */
/* -------------------------------------------------------------------- */
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
    OGRSpatialReference ogrsr_wgs84;
    char* c = (char*)poSrcDS->GetProjectionRef();
    ogrsr_input.importFromWkt(&c);
    ogrsr_wgs84.SetWellKnownGeogCS( "WGS84" );
    if ( ogrsr_input.IsSameGeogCS(&ogrsr_wgs84) == FALSE)
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "The source projection coordinate system is %s. Only WGS 84 is supported.\n"
                  "The SRTMHGT driver will generate a file as if the source was WGS 84 projection coordinate system.",
                  poSrcDS->GetProjectionRef() );
    }

/* -------------------------------------------------------------------- */
/*      Work out the LL origin.                                         */
/* -------------------------------------------------------------------- */
    int  nLLOriginLat, nLLOriginLong;
    double adfGeoTransform[6];

    if (poSrcDS->GetGeoTransform( adfGeoTransform ) != CE_None)
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Source image must have a geo transform matrix.");
        return NULL;
    }

    nLLOriginLat = (int) 
        floor(adfGeoTransform[3] 
              + poSrcDS->GetRasterYSize() * adfGeoTransform[5] + 0.5);

    nLLOriginLong = (int) floor(adfGeoTransform[0] + 0.5);

    if (fabs(nLLOriginLat - (adfGeoTransform[3] 
              + (poSrcDS->GetRasterYSize() - 0.5) * adfGeoTransform[5])) > 1e-10 ||
        fabs(nLLOriginLong - (adfGeoTransform[0] + 0.5 * adfGeoTransform[1])) > 1e-10)
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
               "The corner coordinates of the source are not properly "
               "aligned on plain latitude/longitude boundaries.");
    }

/* -------------------------------------------------------------------- */
/*      Check image dimensions.                                         */
/* -------------------------------------------------------------------- */
    if (!((nXSize == 1201 && nYSize == 1201) || (nXSize == 3601 && nYSize == 3601)))
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Image dimensions should be 1201x1201 or 3601x3601.");
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Check filename.                                                 */
/* -------------------------------------------------------------------- */
    char expectedFileName[12];
    snprintf(expectedFileName, sizeof(expectedFileName), "%c%02d%c%03d.HGT",
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

    GInt16* panData = (GInt16*) CPLMalloc(sizeof(GInt16) * nXSize);
    GDALRasterBand* poSrcBand = poSrcDS->GetRasterBand(1);

    int bSrcBandHasNoData;
    double srcBandNoData = poSrcBand->GetNoDataValue(&bSrcBandHasNoData);

    for( int iY = 0; iY < nYSize; iY++ )
    {
        poSrcBand->RasterIO( GF_Read, 0, iY, nXSize, 1,
                            (void *) panData, nXSize, 1,
                            GDT_Int16, 0, 0 );

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

        if( pfnProgress && !pfnProgress((iY+1) / (double) nYSize, NULL, pProgressData ) )
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
    GDALPamDataset *poDS = (GDALPamDataset *) 
        GDALOpen( pszFilename, GA_ReadOnly );

    if( poDS )
        poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT);

    return poDS;
}

/************************************************************************/
/*                         GDALRegister_SRTMHGT()                          */
/************************************************************************/
void GDALRegister_SRTMHGT()
{
  GDALDriver*  poDriver;

  if(GDALGetDriverByName("SRTMHGT") == NULL)
  {
    poDriver = new GDALDriver();
    poDriver->SetDescription("SRTMHGT");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "SRTMHGT File Format");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "hgt");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, 
                              "frmt_various.html#SRTMHGT" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte Int16 UInt16" );

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnIdentify = SRTMHGTDataset::Identify;
    poDriver->pfnOpen = SRTMHGTDataset::Open;
    poDriver->pfnCreateCopy = SRTMHGTDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
  }
}

