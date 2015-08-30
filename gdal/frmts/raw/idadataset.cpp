/******************************************************************************
 * $Id$
 *
 * Project:  IDA Raster Driver
 * Purpose:  Implemenents IDA driver/dataset/rasterband.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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
#include "ogr_spatialref.h"
#include "gdal_rat.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_IDA(void);
CPL_C_END

// convert a Turbo Pascal real into a double
static double tp2c(GByte *r);

// convert a double into a Turbo Pascal real
static void c2tp(double n, GByte *r);

/************************************************************************/
/* ==================================================================== */
/*				IDADataset				*/
/* ==================================================================== */
/************************************************************************/

class IDADataset : public RawDataset
{
    friend class IDARasterBand;

    int         nImageType;
    int         nProjection;
    char        szTitle[81];
    double      dfLatCenter;
    double      dfLongCenter;
    double      dfXCenter;
    double      dfYCenter;
    double      dfDX;
    double      dfDY;
    double      dfParallel1;
    double      dfParallel2;
    int         nMissing;
    double      dfM;
    double      dfB;

    VSILFILE   *fpRaw;

    char       *pszProjection;
    double      adfGeoTransform[6];

    void        ProcessGeoref();

    GByte       abyHeader[512];
    int         bHeaderDirty;

    void        ReadColorTable();
    
  public:
    		IDADataset();
    	        ~IDADataset();
    
    virtual void FlushCache();
    virtual const char *GetProjectionRef(void);
    virtual CPLErr SetProjection( const char * );

    virtual CPLErr GetGeoTransform( double * );
    virtual CPLErr SetGeoTransform( double * );

    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType,
                                char ** /* papszParmList */ );

};

/************************************************************************/
/* ==================================================================== */
/*			        IDARasterBand                           */
/* ==================================================================== */
/************************************************************************/

class IDARasterBand : public RawRasterBand
{
    friend class IDADataset;

    GDALRasterAttributeTable *poRAT;
    GDALColorTable       *poColorTable;

  public:
    		IDARasterBand( IDADataset *poDSIn, VSILFILE *fpRaw, int nXSize );
    virtual     ~IDARasterBand();

    virtual GDALRasterAttributeTable *GetDefaultRAT();
    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
    virtual double GetOffset( int *pbSuccess = NULL );
    virtual CPLErr SetOffset( double dfNewValue );
    virtual double GetScale( int *pbSuccess = NULL );
    virtual CPLErr SetScale( double dfNewValue );
    virtual double GetNoDataValue( int *pbSuccess = NULL );
};

/************************************************************************/
/*                            IDARasterBand                             */
/************************************************************************/

IDARasterBand::IDARasterBand( IDADataset *poDSIn,
                              VSILFILE *fpRaw, int nXSize )
        : RawRasterBand( poDSIn, 1, fpRaw, 512, 1, nXSize, 
                         GDT_Byte, FALSE, TRUE )

{
    poColorTable = NULL;
    poRAT = NULL;
}

/************************************************************************/
/*                           ~IDARasterBand()                           */
/************************************************************************/

IDARasterBand::~IDARasterBand()

{
    delete poColorTable;
    delete poRAT;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double IDARasterBand::GetNoDataValue( int *pbSuccess )

{
    if( pbSuccess != NULL )
        *pbSuccess = TRUE;
    return ((IDADataset *) poDS)->nMissing;
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

double IDARasterBand::GetOffset( int *pbSuccess )

{
    if( pbSuccess != NULL )
        *pbSuccess = TRUE;
    return ((IDADataset *) poDS)->dfB;
}

/************************************************************************/
/*                             SetOffset()                              */
/************************************************************************/

CPLErr IDARasterBand::SetOffset( double dfNewValue )

{
    IDADataset *poIDS = (IDADataset *) poDS;

    if( dfNewValue == poIDS->dfB )
        return CE_None;

    if( poIDS->nImageType != 200 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Setting explicit offset only support for image type 200.");
        return CE_Failure;
    }

    poIDS->dfB = dfNewValue;
    c2tp( dfNewValue, poIDS->abyHeader + 177 );
    poIDS->bHeaderDirty = TRUE;

    return CE_None;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double IDARasterBand::GetScale( int *pbSuccess )

{
    if( pbSuccess != NULL )
        *pbSuccess = TRUE;
    return ((IDADataset *) poDS)->dfM;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr IDARasterBand::SetScale( double dfNewValue )

{
    IDADataset *poIDS = (IDADataset *) poDS;

    if( dfNewValue == poIDS->dfM )
        return CE_None;

    if( poIDS->nImageType != 200 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Setting explicit scale only support for image type 200.");
        return CE_Failure;
    }

    poIDS->dfM = dfNewValue;
    c2tp( dfNewValue, poIDS->abyHeader + 171 );
    poIDS->bHeaderDirty = TRUE;

    return CE_None;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *IDARasterBand::GetColorTable()

{
    if( poColorTable )
        return poColorTable;
    else
        return RawRasterBand::GetColorTable();
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp IDARasterBand::GetColorInterpretation()

{
    if( poColorTable )
        return GCI_PaletteIndex;
    else
        return RawRasterBand::GetColorInterpretation();
}

/************************************************************************/
/*                           GetDefaultRAT()                            */
/************************************************************************/

GDALRasterAttributeTable *IDARasterBand::GetDefaultRAT() 

{
    if( poRAT )
        return poRAT;
    else
        return RawRasterBand::GetDefaultRAT();
}

/************************************************************************/
/* ==================================================================== */
/*				IDADataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             IDADataset()                             */
/************************************************************************/

IDADataset::IDADataset() :
    nImageType(0), nProjection(0), dfLatCenter(0.0), dfLongCenter(0.0),
    dfXCenter(0.0), dfYCenter(0.0), dfDX(0.0), dfDY(0.0), dfParallel1(0.0),
    dfParallel2(0.0), nMissing(0), dfM(0.0), dfB(0.0), fpRaw(NULL),
    pszProjection(NULL), bHeaderDirty(FALSE)
{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~IDADataset()                             */
/************************************************************************/

IDADataset::~IDADataset()

{
    FlushCache();

    if( fpRaw != NULL )
        VSIFCloseL( fpRaw );
    CPLFree( pszProjection );
}

/************************************************************************/
/*                           ProcessGeoref()                            */
/************************************************************************/

void IDADataset::ProcessGeoref()

{
    OGRSpatialReference oSRS;

    if( nProjection == 3 ) 
    {
        oSRS.SetWellKnownGeogCS( "WGS84" );
    }
    else if( nProjection == 4 ) 
    {
        oSRS.SetLCC( dfParallel1, dfParallel2, 
                     dfLatCenter, dfLongCenter, 
                     0.0, 0.0 );
        oSRS.SetGeogCS( "Clarke 1866", "Clarke 1866", "Clarke 1866", 
                        6378206.4, 293.97869821389662 );
    }
    else if( nProjection == 6 ) 
    {
        oSRS.SetLAEA( dfLatCenter, dfLongCenter, 0.0, 0.0 );
        oSRS.SetGeogCS( "Sphere", "Sphere", "Sphere", 
                        6370997.0, 0.0 );
    }
    else if( nProjection == 8 )
    {
        oSRS.SetACEA( dfParallel1, dfParallel2, 
                      dfLatCenter, dfLongCenter, 
                      0.0, 0.0 );
        oSRS.SetGeogCS( "Clarke 1866", "Clarke 1866", "Clarke 1866", 
                        6378206.4, 293.97869821389662 );
    }
    else if( nProjection == 9 ) 
    {
        oSRS.SetGH( dfLongCenter, 0.0, 0.0 );
        oSRS.SetGeogCS( "Sphere", "Sphere", "Sphere", 
                        6370997.0, 0.0 );
    }

    if( oSRS.GetRoot() != NULL )
    {
        CPLFree( pszProjection );
        pszProjection = NULL;

        oSRS.exportToWkt( &pszProjection );
    }

    adfGeoTransform[0] = 0 - dfDX * dfXCenter;
    adfGeoTransform[1] = dfDX;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] =  dfDY * dfYCenter;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = -dfDY;

    if( nProjection == 3 )
    {
        adfGeoTransform[0] += dfLongCenter;
        adfGeoTransform[3] += dfLatCenter;
    }
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void IDADataset::FlushCache()

{
    RawDataset::FlushCache();
    
    if( bHeaderDirty )
    {
        VSIFSeekL( fpRaw, 0, SEEK_SET );
        VSIFWriteL( abyHeader, 512, 1, fpRaw );
        bHeaderDirty = FALSE;
    }
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr IDADataset::GetGeoTransform( double *padfGeoTransform )

{
    memcpy( padfGeoTransform, adfGeoTransform, sizeof(double) * 6 );
    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr IDADataset::SetGeoTransform( double *padfGeoTransform )

{
    if( padfGeoTransform[2] != 0.0 || padfGeoTransform[4] != 0.0 )
        return GDALPamDataset::SetGeoTransform( padfGeoTransform );

    memcpy( adfGeoTransform, padfGeoTransform, sizeof(double) * 6 );
    bHeaderDirty = TRUE;

    dfDX = adfGeoTransform[1];
    dfDY = -adfGeoTransform[5];
    dfXCenter = -adfGeoTransform[0] / dfDX;
    dfYCenter = adfGeoTransform[3] / dfDY;

    c2tp( dfDX, abyHeader + 144 );
    c2tp( dfDY, abyHeader + 150 );
    c2tp( dfXCenter, abyHeader + 132 );
    c2tp( dfYCenter, abyHeader + 138 );

    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *IDADataset::GetProjectionRef()

{
    if( pszProjection )
        return pszProjection;
    else
        return "";
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr IDADataset::SetProjection( const char *pszWKTIn )

{
    OGRSpatialReference oSRS;

    oSRS.importFromWkt( (char **) &pszWKTIn );

    if( !oSRS.IsGeographic() && !oSRS.IsProjected() )
        GDALPamDataset::SetProjection( pszWKTIn );

/* -------------------------------------------------------------------- */
/*      Clear projection parameters.                                    */
/* -------------------------------------------------------------------- */
    dfParallel1 = 0.0;
    dfParallel2 = 0.0;
    dfLatCenter = 0.0;
    dfLongCenter = 0.0;

/* -------------------------------------------------------------------- */
/*      Geographic.                                                     */
/* -------------------------------------------------------------------- */
    if( oSRS.IsGeographic() )
    {
        // If no change, just return. 
        if( nProjection == 3 )
            return CE_None;

        nProjection = 3;
    }

/* -------------------------------------------------------------------- */
/*      Verify we don't have a false easting or northing as these       */
/*      will be ignored for the projections we do support.              */
/* -------------------------------------------------------------------- */
    if( oSRS.GetProjParm( SRS_PP_FALSE_EASTING ) != 0.0
        || oSRS.GetProjParm( SRS_PP_FALSE_NORTHING ) != 0.0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to set a projection on an IDA file with a non-zero\n"
                  "false easting and/or northing.  This is not supported." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Lambert Conformal Conic.  Note that we don't support false      */
/*      eastings or nothings.                                           */
/* -------------------------------------------------------------------- */
    const char *pszProjection = oSRS.GetAttrValue( "PROJECTION" );

    if( pszProjection == NULL )
    {
        /* do nothing - presumably geographic  */;
    }
    else if( EQUAL(pszProjection,SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) )
    {
        nProjection = 4;
        dfParallel1 = oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0);
        dfParallel2 = oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_2,0.0);
        dfLatCenter = oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        dfLongCenter = oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
    }
    else if( EQUAL(pszProjection,SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA) )
    {
        nProjection = 6;
        dfLatCenter = oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        dfLongCenter = oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
    }
    else if( EQUAL(pszProjection,SRS_PT_ALBERS_CONIC_EQUAL_AREA) )
    {
        nProjection = 8;
        dfParallel1 = oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0);
        dfParallel2 = oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_2,0.0);
        dfLatCenter = oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        dfLongCenter = oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
    }
    else if( EQUAL(pszProjection,SRS_PT_GOODE_HOMOLOSINE) )
    {
        nProjection = 9;
        dfLongCenter = oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
    }
    else
    {
        return GDALPamDataset::SetProjection( pszWKTIn );
    }

/* -------------------------------------------------------------------- */
/*      Update header and mark it as dirty.                             */
/* -------------------------------------------------------------------- */
    bHeaderDirty = TRUE;

    abyHeader[23] = (GByte) nProjection;
    c2tp( dfLatCenter, abyHeader + 120 );
    c2tp( dfLongCenter, abyHeader + 126 );
    c2tp( dfParallel1, abyHeader + 156 );
    c2tp( dfParallel2, abyHeader + 162 );

    return CE_None;
}

/************************************************************************/
/*                           ReadColorTable()                           */
/************************************************************************/

void IDADataset::ReadColorTable()

{
/* -------------------------------------------------------------------- */
/*      Decide what .clr file to look for and try to open.              */
/* -------------------------------------------------------------------- */
    CPLString osCLRFilename;

    osCLRFilename = CPLGetConfigOption( "IDA_COLOR_FILE", "" );
    if( strlen(osCLRFilename) == 0 )
        osCLRFilename = CPLResetExtension(GetDescription(), "clr" );


    FILE *fp = VSIFOpen( osCLRFilename, "r" );
    if( fp == NULL )
    {
        osCLRFilename = CPLResetExtension(osCLRFilename, "CLR" );
        fp = VSIFOpen( osCLRFilename, "r" );
    }

    if( fp == NULL )
        return;

/* -------------------------------------------------------------------- */
/*      Skip first line, with the column titles.                        */
/* -------------------------------------------------------------------- */
    CPLReadLine( fp );

/* -------------------------------------------------------------------- */
/*      Create a RAT to populate.                                       */
/* -------------------------------------------------------------------- */
    GDALRasterAttributeTable *poRAT = new GDALDefaultRasterAttributeTable();

    poRAT->CreateColumn( "FROM", GFT_Integer, GFU_Min );
    poRAT->CreateColumn( "TO", GFT_Integer, GFU_Max );
    poRAT->CreateColumn( "RED", GFT_Integer, GFU_Red );
    poRAT->CreateColumn( "GREEN", GFT_Integer, GFU_Green );
    poRAT->CreateColumn( "BLUE", GFT_Integer, GFU_Blue );
    poRAT->CreateColumn( "LEGEND", GFT_String, GFU_Name );

/* -------------------------------------------------------------------- */
/*      Apply lines.                                                    */
/* -------------------------------------------------------------------- */
    const char *pszLine = CPLReadLine( fp );
    int iRow = 0;

    while( pszLine != NULL )
    {
        char **papszTokens = 
            CSLTokenizeStringComplex( pszLine, " \t", FALSE, FALSE );
        
        if( CSLCount( papszTokens ) >= 5 )
        {
            poRAT->SetValue( iRow, 0, atoi(papszTokens[0]) );
            poRAT->SetValue( iRow, 1, atoi(papszTokens[1]) );
            poRAT->SetValue( iRow, 2, atoi(papszTokens[2]) );
            poRAT->SetValue( iRow, 3, atoi(papszTokens[3]) );
            poRAT->SetValue( iRow, 4, atoi(papszTokens[4]) );

            // find name, first nonspace after 5th token. 
            const char *pszName = pszLine;

            // skip from
            while( *pszName == ' ' || *pszName == '\t' )
                pszName++;
            while( *pszName != ' ' && *pszName != '\t' && *pszName != '\0' )
                pszName++;
            
            // skip to
            while( *pszName == ' ' || *pszName == '\t' )
                pszName++;
            while( *pszName != ' ' && *pszName != '\t' && *pszName != '\0' )
                pszName++;
            
            // skip red
            while( *pszName == ' ' || *pszName == '\t' )
                pszName++;
            while( *pszName != ' ' && *pszName != '\t' && *pszName != '\0' )
                pszName++;
            
            // skip green
            while( *pszName == ' ' || *pszName == '\t' )
                pszName++;
            while( *pszName != ' ' && *pszName != '\t' && *pszName != '\0' )
                pszName++;
            
            // skip blue
            while( *pszName == ' ' || *pszName == '\t' )
                pszName++;
            while( *pszName != ' ' && *pszName != '\t' && *pszName != '\0' )
                pszName++;

            // skip pre-name white space
            while( *pszName == ' ' || *pszName == '\t' )
                pszName++;

            poRAT->SetValue( iRow, 5, pszName );
            
            iRow++;
        }

        CSLDestroy( papszTokens );
        pszLine = CPLReadLine( fp );
    }

    VSIFClose( fp );

/* -------------------------------------------------------------------- */
/*      Attach RAT to band.                                             */
/* -------------------------------------------------------------------- */
    ((IDARasterBand *) GetRasterBand( 1 ))->poRAT = poRAT;

/* -------------------------------------------------------------------- */
/*      Build a conventional color table from this.                     */
/* -------------------------------------------------------------------- */
    ((IDARasterBand *) GetRasterBand( 1 ))->poColorTable = 
        poRAT->TranslateToColorTable();
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *IDADataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Is this an IDA file?                                            */
/* -------------------------------------------------------------------- */
    int      nXSize, nYSize;
    GIntBig  nExpectedFileSize, nActualFileSize;

    if( poOpenInfo->fpL == NULL )
        return NULL;

    if( poOpenInfo->nHeaderBytes < 512 )
        return NULL;

    // projection legal? 
    if( poOpenInfo->pabyHeader[23] > 10 )
        return NULL;

    // imagetype legal? 
    if( (poOpenInfo->pabyHeader[22] > 14 
         && poOpenInfo->pabyHeader[22] < 100)
        || (poOpenInfo->pabyHeader[22] > 114 
            && poOpenInfo->pabyHeader[22] != 200 ) )
        return NULL;

    nXSize = poOpenInfo->pabyHeader[30] + poOpenInfo->pabyHeader[31] * 256;
    nYSize = poOpenInfo->pabyHeader[32] + poOpenInfo->pabyHeader[33] * 256;

    if( nXSize == 0 || nYSize == 0 )
        return NULL;

    // The file just be exactly the image size + header size in length.
    nExpectedFileSize = nXSize * nYSize + 512;
    
    VSIFSeekL( poOpenInfo->fpL, 0, SEEK_END );
    nActualFileSize = VSIFTellL( poOpenInfo->fpL );
    VSIRewindL( poOpenInfo->fpL );
    
    if( nActualFileSize != nExpectedFileSize )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */
    IDADataset *poDS = new IDADataset();				

    memcpy( poDS->abyHeader, poOpenInfo->pabyHeader, 512 );
        
/* -------------------------------------------------------------------- */
/*      Parse various values out of the header.                         */
/* -------------------------------------------------------------------- */
    poDS->nImageType = poOpenInfo->pabyHeader[22];
    poDS->nProjection = poOpenInfo->pabyHeader[23];

    poDS->nRasterYSize = poOpenInfo->pabyHeader[30] 
        + poOpenInfo->pabyHeader[31] * 256;
    poDS->nRasterXSize = poOpenInfo->pabyHeader[32] 
        + poOpenInfo->pabyHeader[33] * 256;

    strncpy( poDS->szTitle, (const char *) poOpenInfo->pabyHeader+38, 80 );
    poDS->szTitle[80] = '\0';

    int nLastTitleChar = strlen(poDS->szTitle)-1;
    while( nLastTitleChar > -1 
           && (poDS->szTitle[nLastTitleChar] == 10 
               || poDS->szTitle[nLastTitleChar] == 13 
               || poDS->szTitle[nLastTitleChar] == ' ') ) 
        poDS->szTitle[nLastTitleChar--] = '\0';

    poDS->dfLatCenter = tp2c( poOpenInfo->pabyHeader + 120 );
    poDS->dfLongCenter = tp2c( poOpenInfo->pabyHeader + 126 );
    poDS->dfXCenter = tp2c( poOpenInfo->pabyHeader + 132 );
    poDS->dfYCenter = tp2c( poOpenInfo->pabyHeader + 138 );
    poDS->dfDX = tp2c( poOpenInfo->pabyHeader + 144 );
    poDS->dfDY = tp2c( poOpenInfo->pabyHeader + 150 );
    poDS->dfParallel1 = tp2c( poOpenInfo->pabyHeader + 156 );
    poDS->dfParallel2 = tp2c( poOpenInfo->pabyHeader + 162 );

    poDS->ProcessGeoref();

    poDS->SetMetadataItem( "TITLE", poDS->szTitle );

/* -------------------------------------------------------------------- */
/*      Handle various image types.                                     */
/* -------------------------------------------------------------------- */

/*
GENERIC = 0
FEW S NDVI = 1
EROS NDVI = 6
ARTEMIS CUTOFF = 10
ARTEMIS RECODE = 11
ARTEMIS NDVI = 12
ARTEMIS FEWS = 13
ARTEMIS NEWNASA = 14
GENERIC DIFF = 100
FEW S NDVI DIFF = 101
EROS NDVI DIFF = 106
ARTEMIS CUTOFF DIFF = 110
ARTEMIS RECODE DIFF = 111
ARTEMIS NDVI DIFF = 112
ARTEMIS FEWS DIFF = 113
ARTEMIS NEWNASA DIFF = 114
CALCULATED =200
*/
 
    poDS->nMissing = 0;

    switch( poDS->nImageType )
    {
      case 1:
        poDS->SetMetadataItem( "IMAGETYPE", "1, FEWS NDVI" );
        poDS->dfM = 1/256.0;
        poDS->dfB = -82/256.0;
        break;

      case 6:
        poDS->SetMetadataItem( "IMAGETYPE", "6, EROS NDVI" );
        poDS->dfM = 1/100.0;
        poDS->dfB = -100/100.0;
        break;

      case 10:
        poDS->SetMetadataItem( "IMAGETYPE", "10, ARTEMIS CUTOFF" );
        poDS->dfM = 1.0;
        poDS->dfB = 0.0;
        poDS->nMissing = 254;
        break;

      case 11:
        poDS->SetMetadataItem( "IMAGETYPE", "11, ARTEMIS RECODE" );
        poDS->dfM = 4.0;
        poDS->dfB = 0.0;
        poDS->nMissing = 254;
        break;

      case 12: /* ANDVI */
        poDS->SetMetadataItem( "IMAGETYPE", "12, ARTEMIS NDVI" );
        poDS->dfM = 4/500.0;
        poDS->dfB = -3/500.0 - 1.0;
        poDS->nMissing = 254;
        break;

      case 13: /* AFEWS */
        poDS->SetMetadataItem( "IMAGETYPE", "13, ARTEMIS FEWS" );
        poDS->dfM = 1/256.0;
        poDS->dfB = -82/256.0;
        poDS->nMissing = 254;
        break;

      case 14: /* NEWNASA */
        poDS->SetMetadataItem( "IMAGETYPE", "13, ARTEMIS NEWNASA" );
        poDS->dfM = 0.75/250.0;
        poDS->dfB = 0.0;
        poDS->nMissing = 254;
        break;

      case 101: /* NDVI_DIFF (FEW S) */
        poDS->dfM = 1/128.0;
        poDS->dfB = -1.0;
        poDS->nMissing = 0;
        break;

      case 106: /* EROS_DIFF */
        poDS->dfM = 1/50.0;
        poDS->dfB = -128/50.0;
        poDS->nMissing = 0;
        break;

      case 110: /* CUTOFF_DIFF */
        poDS->dfM = 2.0;
        poDS->dfB = -128*2;
        poDS->nMissing = 254;
        break;

      case 111: /* RECODE_DIFF */
        poDS->dfM = 8;
        poDS->dfB = -128*8;
        poDS->nMissing = 254;
        break;

      case 112: /* ANDVI_DIFF */
        poDS->dfM = 8/1000.0;
        poDS->dfB = (-128*8)/1000.0;
        poDS->nMissing = 254;
        break;

      case 113: /* AFEWS_DIFF */
        poDS->dfM = 1/128.0;
        poDS->dfB = -1;
        poDS->nMissing = 254;
        break;

      case 114: /* NEWNASA_DIFF */
        poDS->dfM = 0.75/125.0;
        poDS->dfB = -128*poDS->dfM;
        poDS->nMissing = 254;
        break;

      case 200:
        /* we use the values from the header */
        poDS->dfM = tp2c( poOpenInfo->pabyHeader + 171 );
        poDS->dfB = tp2c( poOpenInfo->pabyHeader + 177 );
        poDS->nMissing = poOpenInfo->pabyHeader[170];
        break;

      default:
        poDS->dfM = 1.0;
        poDS->dfB = 0.0;
        break;
    }

/* -------------------------------------------------------------------- */
/*      Create the band.                                                */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_ReadOnly )
    {
        poDS->fpRaw = poOpenInfo->fpL;
        poOpenInfo->fpL = NULL;
    }
    else
    {
        poDS->fpRaw = VSIFOpenL( poOpenInfo->pszFilename, "rb+" );
        poDS->eAccess = GA_Update;
        if( poDS->fpRaw == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Failed to open %s for write access.", 
                      poOpenInfo->pszFilename );
            return NULL;
        }
    }

    poDS->SetBand( 1, new IDARasterBand( poDS, poDS->fpRaw, 
                                         poDS->nRasterXSize ) );

/* -------------------------------------------------------------------- */
/*      Check for a color table.                                        */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->ReadColorTable();

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

/************************************************************************/
/*                                tp2c()                                */
/*                                                                      */
/*      convert a Turbo Pascal real into a double                       */
/************************************************************************/

static double tp2c(GByte *r)
{
  double mant;
  int sign, exp, i;

  // handle 0 case
  if (r[0] == 0)
    return 0.0;

  // extract sign: bit 7 of byte 5
  sign = r[5] & 0x80 ? -1 : 1;

  // extract mantissa from first bit of byte 1 to bit 7 of byte 5
  mant = 0;
  for (i = 1; i < 5; i++)
    mant = (r[i] + mant) / 256;
  mant = (mant + (r[5] & 0x7F)) / 128 + 1;

   // extract exponent
  exp = r[0] - 129;

  // compute the damned number
  return sign * ldexp(mant, exp);
}

/************************************************************************/
/*                                c2tp()                                */
/*                                                                      */
/*      convert a double into a Turbo Pascal real                       */
/************************************************************************/

static void c2tp(double x, GByte *r)
{
  double mant, temp;
  int negative, exp, i;

  // handle 0 case
  if (x == 0.0)
  {
    for (i = 0; i < 6; r[i++] = 0);
    return;
  }

  // compute mantissa, sign and exponent
  mant = frexp(x, &exp) * 2 - 1;
  exp--;
  negative = 0;
  if (mant < 0)
  {
    mant = -mant;
    negative = 1;
  }
  // stuff mantissa into Turbo Pascal real
  mant = modf(mant * 128, &temp);
  r[5] = (unsigned char) (((int)temp) & 0xff);
  for (i = 4; i >= 1; i--)
  {
    mant = modf(mant * 256, &temp);
    r[i] = (unsigned char) temp;
  }
  // add sign
  if (negative)
    r[5] |= 0x80;

  // put exponent
  r[0] = (GByte) (exp + 129);
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *IDADataset::Create( const char * pszFilename,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType,
                                 char ** /* papszParmList */ )

{
/* -------------------------------------------------------------------- */
/*      Verify input options.                                           */
/* -------------------------------------------------------------------- */
    if( eType != GDT_Byte || nBands != 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Only 1 band, Byte datasets supported for IDA format." );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try to create the file.                                         */
/* -------------------------------------------------------------------- */
    FILE	*fp;

    fp = VSIFOpen( pszFilename, "wb" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file `%s' failed.\n",
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Prepare formatted header.                                       */
/* -------------------------------------------------------------------- */
    GByte abyHeader[512];
    
    memset( abyHeader, 0, sizeof(abyHeader) );
    
    abyHeader[22] = 200; /* image type - CALCULATED */
    abyHeader[23] = 0; /* projection - NONE */
    abyHeader[30] = nYSize % 256;
    abyHeader[31] = (GByte) (nYSize / 256);
    abyHeader[32] = nXSize % 256;
    abyHeader[33] = (GByte) (nXSize / 256);

    abyHeader[170] = 255; /* missing = 255 */
    c2tp( 1.0, abyHeader + 171 ); /* slope = 1.0 */
    c2tp( 0.0, abyHeader + 177 ); /* offset = 0 */
    abyHeader[168] = 0; // lower limit
    abyHeader[169] = 254; // upper limit

    // pixel size = 1.0
    c2tp( 1.0, abyHeader + 144 );
    c2tp( 1.0, abyHeader + 150 );

    if( VSIFWrite( abyHeader, 1, 512, fp ) != 512 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "IO error writing %s.\n%s", 
                  pszFilename, VSIStrerror( errno ) );
        VSIFClose( fp );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Now we need to extend the file to just the right number of      */
/*      bytes for the data we have to ensure it will open again         */
/*      properly.                                                       */
/* -------------------------------------------------------------------- */
    if( VSIFSeek( fp, nXSize * nYSize - 1, SEEK_CUR ) != 0
        || VSIFWrite( abyHeader, 1, 1, fp ) != 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "IO error writing %s.\n%s", 
                  pszFilename, VSIStrerror( errno ) );
        VSIFClose( fp );
        return NULL;
    }

    VSIFClose( fp );

    return (GDALDataset *) GDALOpen( pszFilename, GA_Update );
}

/************************************************************************/
/*                         GDALRegister_IDA()                          */
/************************************************************************/

void GDALRegister_IDA()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "IDA" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "IDA" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Image Data and Analysis" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#IDA" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Byte" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = IDADataset::Open;
        poDriver->pfnCreate = IDADataset::Create;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
