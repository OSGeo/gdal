/******************************************************************************
 * $Id$
 *
 * Project:  LCP Driver
 * Purpose:  FARSITE v.4 Landscape file (.lcp) reader for GDAL
 * Author:   Chris Toney
 *
 ******************************************************************************
 * Copyright (c) 2008, Chris Toney
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

#include "rawdataset.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_LCP(void);
CPL_C_END

#define LCP_HEADER_SIZE 7316

/************************************************************************/
/* ==================================================================== */
/*                              LCPDataset                              */
/* ==================================================================== */
/************************************************************************/

class LCPDataset : public RawDataset
{
    VSILFILE    *fpImage;       // image data file.
    char	pachHeader[LCP_HEADER_SIZE];

    CPLString   osPrjFilename;
    char        *pszProjection;

  public:
                LCPDataset();
                ~LCPDataset();

    virtual char **GetFileList(void);

    virtual CPLErr GetGeoTransform( double * );

    static int          Identify( GDALOpenInfo * );
    static GDALDataset *Open( GDALOpenInfo * );

    virtual const char *GetProjectionRef(void);

    int bHaveProjection;
};

/************************************************************************/
/*                            LCPDataset()                             */
/************************************************************************/

LCPDataset::LCPDataset()
{
    fpImage = NULL;
    pszProjection = CPLStrdup( "" );
    bHaveProjection = FALSE;
}

/************************************************************************/
/*                            ~LCPDataset()                            */
/************************************************************************/

LCPDataset::~LCPDataset()

{
    FlushCache();
    if( fpImage != NULL )
        VSIFCloseL( fpImage );
    CPLFree(pszProjection);
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr LCPDataset::GetGeoTransform( double * padfTransform )
{
    double      dfEast, dfWest, dfNorth, dfSouth, dfCellX, dfCellY;

    memcpy(&dfEast, pachHeader + 4172, sizeof(double));
    memcpy(&dfWest, pachHeader + 4180, sizeof(double));
    memcpy(&dfNorth, pachHeader + 4188, sizeof(double));
    memcpy(&dfSouth, pachHeader + 4196, sizeof(double));
    memcpy(&dfCellX, pachHeader + 4208, sizeof(double));
    memcpy(&dfCellY, pachHeader + 4216, sizeof(double));
    CPL_LSBPTR64(&dfEast);
    CPL_LSBPTR64(&dfWest);
    CPL_LSBPTR64(&dfNorth);
    CPL_LSBPTR64(&dfSouth);
    CPL_LSBPTR64(&dfCellX);
    CPL_LSBPTR64(&dfCellY);

    padfTransform[0] = dfWest;
    padfTransform[3] = dfNorth;
    padfTransform[1] = dfCellX;
    padfTransform[2] = 0.0;

    padfTransform[4] = 0.0;
    padfTransform[5] = -1 * dfCellY;

    return CE_None;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int LCPDataset::Identify( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Verify that this is a FARSITE v.4 LCP file                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 50 )
        return FALSE;

    /* check if first three fields have valid data */
    if( (CPL_LSBINT32PTR(poOpenInfo->pabyHeader) != 20
          && CPL_LSBINT32PTR(poOpenInfo->pabyHeader) != 21)
        || (CPL_LSBINT32PTR(poOpenInfo->pabyHeader+4) != 20
          && CPL_LSBINT32PTR(poOpenInfo->pabyHeader+4) != 21)
        || (CPL_LSBINT32PTR(poOpenInfo->pabyHeader+8) < -90
             || CPL_LSBINT32PTR(poOpenInfo->pabyHeader+8) > 90) )
    {
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **LCPDataset::GetFileList()

{
    char **papszFileList = GDALPamDataset::GetFileList();

    if( bHaveProjection )
    {
        papszFileList = CSLAddString( papszFileList, osPrjFilename );
    }

    return papszFileList;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *LCPDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Verify that this is a FARSITE LCP file    */
/* -------------------------------------------------------------------- */
    if( !Identify( poOpenInfo ) )
        return NULL;
        
/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The LCP driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    LCPDataset  *poDS;
    VSILFILE        *fpImage;

    fpImage = VSIFOpenL(poOpenInfo->pszFilename, "rb");
    if (fpImage == NULL)
        return NULL;

    poDS = new LCPDataset();
    poDS->fpImage = fpImage;

/* -------------------------------------------------------------------- */
/*      Read the header and extract some information.                   */
/* -------------------------------------------------------------------- */
   int bHaveCrownFuels, bHaveGroundFuels;
   int nBands, i;
   long nWidth = -1, nHeight = -1;
   int nTemp, nTemp2;
   char szTemp[32];
   char* pszList;

   VSIFSeekL( poDS->fpImage, 0, SEEK_SET );
   if (VSIFReadL( poDS->pachHeader, 1, LCP_HEADER_SIZE, poDS->fpImage ) != LCP_HEADER_SIZE)
   {
       CPLError(CE_Failure, CPLE_FileIO, "File too short");
       delete poDS;
       return NULL;
   }

   nWidth = CPL_LSBINT32PTR (poDS->pachHeader + 4164);
   nHeight = CPL_LSBINT32PTR (poDS->pachHeader + 4168);

   poDS->nRasterXSize = nWidth;
   poDS->nRasterYSize = nHeight;

   if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
   {
       delete poDS;
       return NULL;
   }

   // crown fuels = canopy height, canopy base height, canopy bulk density
   // 21 = have them, 20 = don't have them
   bHaveCrownFuels = ( CPL_LSBINT32PTR (poDS->pachHeader + 0) - 20 );
   // ground fuels = duff loading, coarse woody
   bHaveGroundFuels = ( CPL_LSBINT32PTR (poDS->pachHeader + 4) - 20 );

   if( bHaveCrownFuels )
   {
       if( bHaveGroundFuels )
           nBands = 10;
       else
           nBands = 8;
   }
   else
   {
       if( bHaveGroundFuels )
           nBands = 7;
       else
           nBands = 5;
   }

   // add dataset-level metadata

   nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 8);
   sprintf(szTemp, "%d", nTemp);
   poDS->SetMetadataItem( "LATITUDE", szTemp );

   nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 4204);
   if ( nTemp == 0 )
      poDS->SetMetadataItem( "LINEAR_UNIT", "Meters" );
   if ( nTemp == 1 )
      poDS->SetMetadataItem( "LINEAR_UNIT", "Feet" );

   poDS->pachHeader[LCP_HEADER_SIZE-1] = '\0';
   poDS->SetMetadataItem( "DESCRIPTION", poDS->pachHeader + 6804 );


/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */

   int          iPixelSize;
   iPixelSize = nBands * 2;
   int          bNativeOrder;

   if (nWidth > INT_MAX / iPixelSize)
   {
       CPLError( CE_Failure, CPLE_AppDefined,  "Int overflow occured");
       delete poDS;
       return NULL;
   }

#ifdef CPL_LSB
   bNativeOrder = TRUE;
#else
   bNativeOrder = FALSE;
#endif

   pszList = (char*)CPLMalloc(2048);

   for( int iBand = 1; iBand <= nBands; iBand++ )
   {
        GDALRasterBand  *poBand = NULL;

        poBand = new RawRasterBand(
                     poDS, iBand, poDS->fpImage, LCP_HEADER_SIZE + ((iBand-1)*2),
                     iPixelSize, iPixelSize * nWidth, GDT_Int16, bNativeOrder, TRUE );

        poDS->SetBand(iBand, poBand);

        switch ( iBand ) {
        case 1:
           poBand->SetDescription("Elevation");

           nTemp = CPL_LSBINT16PTR (poDS->pachHeader + 4224);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "ELEVATION_UNIT", szTemp );

           if ( nTemp == 0 )
              poBand->SetMetadataItem( "ELEVATION_UNIT_NAME", "Meters" );
           if ( nTemp == 1 )
              poBand->SetMetadataItem( "ELEVATION_UNIT_NAME", "Feet" );

           nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 44);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "ELEVATION_MIN", szTemp );

           nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 48);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "ELEVATION_MAX", szTemp );

           nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 52);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "ELEVATION_NUM_CLASSES", szTemp );

           *(poDS->pachHeader + 4244 + 255) = '\0';
           poBand->SetMetadataItem( "ELEVATION_FILE", poDS->pachHeader + 4244 );

           break;

        case 2:
           poBand->SetDescription("Slope");

           nTemp = CPL_LSBINT16PTR (poDS->pachHeader + 4226);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "SLOPE_UNIT", szTemp );

           if ( nTemp == 0 )
              poBand->SetMetadataItem( "SLOPE_UNIT_NAME", "Degrees" );
           if ( nTemp == 1 )
              poBand->SetMetadataItem( "SLOPE_UNIT_NAME", "Percent" );

           nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 456);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "SLOPE_MIN", szTemp );

           nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 460);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "SLOPE_MAX", szTemp );

           nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 464);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "SLOPE_NUM_CLASSES", szTemp );

           *(poDS->pachHeader + 4500 + 255) = '\0';
           poBand->SetMetadataItem( "SLOPE_FILE", poDS->pachHeader + 4500 );

           break;

        case 3:
           poBand->SetDescription("Aspect");

           nTemp = CPL_LSBINT16PTR (poDS->pachHeader + 4228);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "ASPECT_UNIT", szTemp );

           if ( nTemp == 0 )
              poBand->SetMetadataItem( "ASPECT_UNIT_NAME", "Grass categories" );
           if ( nTemp == 1 )
              poBand->SetMetadataItem( "ASPECT_UNIT_NAME", "Grass degrees" );
           if ( nTemp == 2 )
              poBand->SetMetadataItem( "ASPECT_UNIT_NAME", "Azimuth degrees" );

           nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 868);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "ASPECT_MIN", szTemp );

           nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 872);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "ASPECT_MAX", szTemp );

           nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 876);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "ASPECT_NUM_CLASSES", szTemp );

           *(poDS->pachHeader + 4756 + 255) = '\0';
           poBand->SetMetadataItem( "ASPECT_FILE", poDS->pachHeader + 4756 );

           break;

        case 4:
           int nMinFM, nMaxFM;

           poBand->SetDescription("Fuel models");

           nTemp = CPL_LSBINT16PTR (poDS->pachHeader + 4230);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "FUEL_MODEL_OPTION", szTemp );

           if ( nTemp == 0 )
              poBand->SetMetadataItem( "FUEL_MODEL_OPTION_DESC", "no custom models AND no conversion file needed" );
           if ( nTemp == 1 )
              poBand->SetMetadataItem( "FUEL_MODEL_OPTION_DESC", "custom models BUT no conversion file needed" );
           if ( nTemp == 2 )
              poBand->SetMetadataItem( "FUEL_MODEL_OPTION_DESC", "no custom models BUT conversion file needed" );
           if ( nTemp == 3 )
              poBand->SetMetadataItem( "FUEL_MODEL_OPTION_DESC", "custom models AND conversion file needed" );

           nMinFM = CPL_LSBINT32PTR (poDS->pachHeader + 1280);
           sprintf(szTemp, "%d", nMinFM);
           poBand->SetMetadataItem( "FUEL_MODEL_MIN", szTemp );

           nMaxFM = CPL_LSBINT32PTR (poDS->pachHeader + 1284);
           sprintf(szTemp, "%d", nMaxFM);
           poBand->SetMetadataItem( "FUEL_MODEL_MAX", szTemp );

           nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 1288);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "FUEL_MODEL_NUM_CLASSES", szTemp );

           if (nTemp > 0 && nTemp <= 100) {
              strcpy(pszList, "");
              for ( i = 0; i <= nTemp; i++ ) {
                  nTemp2 = CPL_LSBINT32PTR (poDS->pachHeader + (1292+(i*4))) ;
                  if ( nTemp2 >= nMinFM && nTemp2 <= nMaxFM ) {
                     sprintf(szTemp, "%d", nTemp2);
                     strcat(pszList, szTemp);
                     if (i < (nTemp) )
                        strcat(pszList, ",");
                  }
              }
           }
           poBand->SetMetadataItem( "FUEL_MODEL_VALUES", pszList );

           *(poDS->pachHeader + 5012 + 255) = '\0';
           poBand->SetMetadataItem( "FUEL_MODEL_FILE", poDS->pachHeader + 5012 );

           break;

        case 5:
           poBand->SetDescription("Canopy cover");

           nTemp = CPL_LSBINT16PTR (poDS->pachHeader + 4232);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "CANOPY_COV_UNIT", szTemp );

           if ( nTemp == 0 )
              poBand->SetMetadataItem( "CANOPY_COV_UNIT_NAME", "Categories (0-4)" );
           if ( nTemp == 1 )
              poBand->SetMetadataItem( "CANOPY_COV_UNIT_NAME", "Percent" );

           nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 1692);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "CANOPY_COV_MIN", szTemp );

           nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 1696);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "CANOPY_COV_MAX", szTemp );

           nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 1700);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "CANOPY_COV_NUM_CLASSES", szTemp );

           *(poDS->pachHeader + 5268 + 255) = '\0';
           poBand->SetMetadataItem( "CANOPY_COV_FILE", poDS->pachHeader + 5268 );

           break;

        case 6:
           if(bHaveCrownFuels) {
              poBand->SetDescription("Canopy height");

              nTemp = CPL_LSBINT16PTR (poDS->pachHeader + 4234);
              sprintf(szTemp, "%d", nTemp);
              poBand->SetMetadataItem( "CANOPY_HT_UNIT", szTemp );

              if ( nTemp == 1 )
                 poBand->SetMetadataItem( "CANOPY_HT_UNIT_NAME", "Meters" );
              if ( nTemp == 2 )
                 poBand->SetMetadataItem( "CANOPY_HT_UNIT_NAME", "Feet" );
              if ( nTemp == 3 )
                 poBand->SetMetadataItem( "CANOPY_HT_UNIT_NAME", "Meters x 10" );
              if ( nTemp == 4 )
                 poBand->SetMetadataItem( "CANOPY_HT_UNIT_NAME", "Feet x 10" );

              nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 2104);
              sprintf(szTemp, "%d", nTemp);
              poBand->SetMetadataItem( "CANOPY_HT_MIN", szTemp );

              nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 2108);
              sprintf(szTemp, "%d", nTemp);
              poBand->SetMetadataItem( "CANOPY_HT_MAX", szTemp );

              nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 2112);
              sprintf(szTemp, "%d", nTemp);
              poBand->SetMetadataItem( "CANOPY_HT_NUM_CLASSES", szTemp );

              *(poDS->pachHeader + 5524 + 255) = '\0';
              poBand->SetMetadataItem( "CANOPY_HT_FILE", poDS->pachHeader + 5524 );
           }
           else {
              poBand->SetDescription("Duff");

              nTemp = CPL_LSBINT16PTR (poDS->pachHeader + 4240);
              sprintf(szTemp, "%d", nTemp);
              poBand->SetMetadataItem( "DUFF_UNIT", szTemp );

              if ( nTemp == 1 )
                 poBand->SetMetadataItem( "DUFF_UNIT_NAME", "Mg/ha" );
              if ( nTemp == 2 )
                 poBand->SetMetadataItem( "DUFF_UNIT_NAME", "t/ac" );

              nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 3340);
              sprintf(szTemp, "%d", nTemp);
              poBand->SetMetadataItem( "DUFF_MIN", szTemp );

              nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 3344);
              sprintf(szTemp, "%d", nTemp);
              poBand->SetMetadataItem( "DUFF_MAX", szTemp );

              nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 3348);
              sprintf(szTemp, "%d", nTemp);
              poBand->SetMetadataItem( "DUFF_NUM_CLASSES", szTemp );

              *(poDS->pachHeader + 6292 + 255) = '\0';
              poBand->SetMetadataItem( "DUFF_FILE", poDS->pachHeader + 6292 );
           }
           break;

        case 7:
           if(bHaveCrownFuels) {
              poBand->SetDescription("Canopy base height");

              nTemp = CPL_LSBINT16PTR (poDS->pachHeader + 4236);
              sprintf(szTemp, "%d", nTemp);
              poBand->SetMetadataItem( "CBH_UNIT", szTemp );

              if ( nTemp == 1 )
                 poBand->SetMetadataItem( "CBH_UNIT_NAME", "Meters" );
              if ( nTemp == 2 )
                 poBand->SetMetadataItem( "CBH_UNIT_NAME", "Feet" );
              if ( nTemp == 3 )
                 poBand->SetMetadataItem( "CBH_UNIT_NAME", "Meters x 10" );
              if ( nTemp == 4 )
                 poBand->SetMetadataItem( "CBH_UNIT_NAME", "Feet x 10" );

              nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 2516);
              sprintf(szTemp, "%d", nTemp);
              poBand->SetMetadataItem( "CBH_MIN", szTemp );

              nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 2520);
              sprintf(szTemp, "%d", nTemp);
              poBand->SetMetadataItem( "CBH_MAX", szTemp );

              nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 2524);
              sprintf(szTemp, "%d", nTemp);
              poBand->SetMetadataItem( "CBH_NUM_CLASSES", szTemp );

              *(poDS->pachHeader + 5780 + 255) = '\0';
              poBand->SetMetadataItem( "CBH_FILE", poDS->pachHeader + 5780 );
           }
           else {
              poBand->SetDescription("Coarse woody debris");

              nTemp = CPL_LSBINT16PTR (poDS->pachHeader + 4242);
              sprintf(szTemp, "%d", nTemp);
              poBand->SetMetadataItem( "CWD_OPTION", szTemp );

              //if ( nTemp == 1 )
              //   poBand->SetMetadataItem( "CWD_UNIT_DESC", "?" );
              //if ( nTemp == 2 )
              //   poBand->SetMetadataItem( "CWD_UNIT_DESC", "?" );

              nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 3752);
              sprintf(szTemp, "%d", nTemp);
              poBand->SetMetadataItem( "CWD_MIN", szTemp );

              nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 3756);
              sprintf(szTemp, "%d", nTemp);
              poBand->SetMetadataItem( "CWD_MAX", szTemp );

              nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 3760);
              sprintf(szTemp, "%d", nTemp);
              poBand->SetMetadataItem( "CWD_NUM_CLASSES", szTemp );

              *(poDS->pachHeader + 6548 + 255) = '\0';
              poBand->SetMetadataItem( "CWD_FILE", poDS->pachHeader + 6548 );
           }
           break;

        case 8:
           poBand->SetDescription("Canopy bulk density");

           nTemp = CPL_LSBINT16PTR (poDS->pachHeader + 4238);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "CBD_UNIT", szTemp );

           if ( nTemp == 1 )
              poBand->SetMetadataItem( "CBD_UNIT_NAME", "kg/m^3" );
           if ( nTemp == 2 )
              poBand->SetMetadataItem( "CBD_UNIT_NAME", "lb/ft^3" );
           if ( nTemp == 3 )
              poBand->SetMetadataItem( "CBD_UNIT_NAME", "kg/m^3 x 100" );
           if ( nTemp == 4 )
              poBand->SetMetadataItem( "CBD_UNIT_NAME", "lb/ft^3 x 1000" );

           nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 2928);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "CBD_MIN", szTemp );

           nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 2932);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "CBD_MAX", szTemp );

           nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 2936);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "CBD_NUM_CLASSES", szTemp );

           *(poDS->pachHeader + 6036 + 255) = '\0';
           poBand->SetMetadataItem( "CBD_FILE", poDS->pachHeader + 6036 );

           break;

        case 9:
           poBand->SetDescription("Duff");

           nTemp = CPL_LSBINT16PTR (poDS->pachHeader + 4240);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "DUFF_UNIT", szTemp );

           if ( nTemp == 1 )
              poBand->SetMetadataItem( "DUFF_UNIT_NAME", "Mg/ha" );
           if ( nTemp == 2 )
              poBand->SetMetadataItem( "DUFF_UNIT_NAME", "t/ac" );

           nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 3340);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "DUFF_MIN", szTemp );

           nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 3344);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "DUFF_MAX", szTemp );

           nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 3348);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "DUFF_NUM_CLASSES", szTemp );

           *(poDS->pachHeader + 6292 + 255) = '\0';
           poBand->SetMetadataItem( "DUFF_FILE", poDS->pachHeader + 6292 );

           break;

        case 10:
           poBand->SetDescription("Coarse woody debris");

           nTemp = CPL_LSBINT16PTR (poDS->pachHeader + 4242);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "CWD_OPTION", szTemp );

           //if ( nTemp == 1 )
           //   poBand->SetMetadataItem( "CWD_UNIT_DESC", "?" );
           //if ( nTemp == 2 )
           //   poBand->SetMetadataItem( "CWD_UNIT_DESC", "?" );

           nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 3752);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "CWD_MIN", szTemp );

           nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 3756);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "CWD_MAX", szTemp );

           nTemp = CPL_LSBINT32PTR (poDS->pachHeader + 3760);
           sprintf(szTemp, "%d", nTemp);
           poBand->SetMetadataItem( "CWD_NUM_CLASSES", szTemp );

           *(poDS->pachHeader + 6548 + 255) = '\0';
           poBand->SetMetadataItem( "CWD_FILE", poDS->pachHeader + 6548 );

           break;
        }
   }
   
/* -------------------------------------------------------------------- */
/*      Try to read projection file.                                    */
/* -------------------------------------------------------------------- */
    char        *pszDirname, *pszBasename;
    VSIStatBufL   sStatBuf;

    pszDirname = CPLStrdup(CPLGetPath(poOpenInfo->pszFilename));
    pszBasename = CPLStrdup(CPLGetBasename(poOpenInfo->pszFilename));

    poDS->osPrjFilename = CPLFormFilename( pszDirname, pszBasename, "prj" );
    int nRet = VSIStatL( poDS->osPrjFilename, &sStatBuf );

    if( nRet != 0 && VSIIsCaseSensitiveFS(poDS->osPrjFilename))
    {
        poDS->osPrjFilename = CPLFormFilename( pszDirname, pszBasename, "PRJ" );
        nRet = VSIStatL( poDS->osPrjFilename, &sStatBuf );
    }

    if( nRet == 0 )
    {
        OGRSpatialReference     oSRS;

        char** papszPrj = CSLLoad( poDS->osPrjFilename );

        CPLDebug( "LCP", "Loaded SRS from %s", 
                  poDS->osPrjFilename.c_str() );

        if( oSRS.importFromESRI( papszPrj ) == OGRERR_NONE )
        {
            CPLFree( poDS->pszProjection );
            oSRS.exportToWkt( &(poDS->pszProjection) );
            poDS->bHaveProjection = TRUE;
        }

        CSLDestroy(papszPrj);
    }

    CPLFree( pszDirname );
    CPLFree( pszBasename );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for external overviews.                                   */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename, poOpenInfo->papszSiblingFiles );

    CPLFree(pszList);

    return( poDS );
}
 
/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/
 
const char *LCPDataset::GetProjectionRef()

{
    return pszProjection;
}

/************************************************************************/
/*                         GDALRegister_LCP()                           */
/************************************************************************/

void GDALRegister_LCP()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "LCP" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "LCP" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "FARSITE v.4 Landscape File (.lcp)" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "lcp" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_lcp.html" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = LCPDataset::Open;
        poDriver->pfnIdentify = LCPDataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
