/******************************************************************************
 * $Id$
 *
 * Project:  VTP .bt Driver
 * Purpose:  Implementation of VTP .bt elevation format read/write support.
 *           http://www.vterrain.org/Implementation/Formats/BT.html
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam
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

#include "rawdataset.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_BT(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*                              BTDataset                               */
/* ==================================================================== */
/************************************************************************/

class BTDataset : public GDALPamDataset
{
    friend class BTRasterBand;

    VSILFILE        *fpImage;       // image data file.

    int         bGeoTransformValid;
    double      adfGeoTransform[6];

    char        *pszProjection;
    
    int         nVersionCode;  // version times 10.

    int         bHeaderModified;
    unsigned char abyHeader[256];

    float        m_fVscale;

  public:

                BTDataset();
                ~BTDataset();

    virtual const char *GetProjectionRef(void);
    virtual CPLErr SetProjection( const char * );
    virtual CPLErr GetGeoTransform( double * );
    virtual CPLErr SetGeoTransform( double * );

    virtual void   FlushCache();

    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszOptions );
};

/************************************************************************/
/* ==================================================================== */
/*                            BTRasterBand                              */
/* ==================================================================== */
/************************************************************************/

class BTRasterBand : public GDALPamRasterBand
{
    VSILFILE          *fpImage;

  public:

                   BTRasterBand( GDALDataset * poDS, VSILFILE * fp,
                                 GDALDataType eType );

    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );

    virtual const char* GetUnitType();
    virtual CPLErr SetUnitType(const char*);
	virtual double GetNoDataValue( int* = NULL );
    virtual CPLErr SetNoDataValue( double );
};


/************************************************************************/
/*                           BTRasterBand()                             */
/************************************************************************/

BTRasterBand::BTRasterBand( GDALDataset *poDS, VSILFILE *fp, GDALDataType eType )

{
    this->poDS = poDS;
    this->nBand = 1;
    this->eDataType = eType;
    this->fpImage = fp;

    nBlockXSize = 1;
    nBlockYSize = poDS->GetRasterYSize();
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr BTRasterBand::IReadBlock( int nBlockXOff,
                                 CPL_UNUSED int nBlockYOff,
                                 void * pImage )
{
    int nDataSize = GDALGetDataTypeSize( eDataType ) / 8;
    int i;

    CPLAssert( nBlockYOff == 0  );

/* -------------------------------------------------------------------- */
/*      Seek to profile.                                                */
/* -------------------------------------------------------------------- */
    if( VSIFSeekL( fpImage, 
                   256 + nBlockXOff * nDataSize * (vsi_l_offset) nRasterYSize, 
                   SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  ".bt Seek failed:%s", VSIStrerror( errno ) );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Read the profile.                                               */
/* -------------------------------------------------------------------- */
    if( VSIFReadL( pImage, nDataSize, nRasterYSize, fpImage ) != 
        (size_t) nRasterYSize )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  ".bt Read failed:%s", VSIStrerror( errno ) );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Swap on MSB platforms.                                          */
/* -------------------------------------------------------------------- */
#ifdef CPL_MSB 
    GDALSwapWords( pImage, nDataSize, nRasterYSize, nDataSize );
#endif    

/* -------------------------------------------------------------------- */
/*      Vertical flip, since GDAL expects values from top to bottom,    */
/*      but in .bt they are bottom to top.                              */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nRasterYSize / 2; i++ )
    {
        GByte abyWrk[8];

        memcpy( abyWrk, ((GByte *) pImage) + i * nDataSize, nDataSize );
        memcpy( ((GByte *) pImage) + i * nDataSize, 
                ((GByte *) pImage) + (nRasterYSize - i - 1) * nDataSize, 
                nDataSize );
        memcpy( ((GByte *) pImage) + (nRasterYSize - i - 1) * nDataSize, 
                abyWrk, nDataSize );
    }

    return CE_None;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr BTRasterBand::IWriteBlock( int nBlockXOff, 
                                  CPL_UNUSED int nBlockYOff,
                                  void * pImage )
{
    int nDataSize = GDALGetDataTypeSize( eDataType ) / 8;
    GByte *pabyWrkBlock;
    int i;

    CPLAssert( nBlockYOff == 0  );

/* -------------------------------------------------------------------- */
/*      Seek to profile.                                                */
/* -------------------------------------------------------------------- */
    if( VSIFSeekL( fpImage, 
                   256 + nBlockXOff * nDataSize * nRasterYSize, 
                   SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  ".bt Seek failed:%s", VSIStrerror( errno ) );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Allocate working buffer.                                        */
/* -------------------------------------------------------------------- */
    pabyWrkBlock = (GByte *) CPLMalloc(nDataSize * nRasterYSize);

/* -------------------------------------------------------------------- */
/*      Vertical flip data into work buffer, since GDAL expects         */
/*      values from top to bottom, but in .bt they are bottom to        */
/*      top.                                                            */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nRasterYSize; i++ )
    {
        memcpy( pabyWrkBlock + (nRasterYSize - i - 1) * nDataSize, 
                ((GByte *) pImage) + i * nDataSize, nDataSize );
    }

/* -------------------------------------------------------------------- */
/*      Swap on MSB platforms.                                          */
/* -------------------------------------------------------------------- */
#ifdef CPL_MSB 
    GDALSwapWords( pabyWrkBlock, nDataSize, nRasterYSize, nDataSize );
#endif    

/* -------------------------------------------------------------------- */
/*      Read the profile.                                               */
/* -------------------------------------------------------------------- */
    if( VSIFWriteL( pabyWrkBlock, nDataSize, nRasterYSize, fpImage ) != 
        (size_t) nRasterYSize )
    {
        CPLFree( pabyWrkBlock );
        CPLError( CE_Failure, CPLE_FileIO, 
                  ".bt Write failed:%s", VSIStrerror( errno ) );
        return CE_Failure;
    }

    CPLFree( pabyWrkBlock );

    return CE_None;
}


double BTRasterBand::GetNoDataValue( int* pbSuccess /*= NULL */ )
{
	if(pbSuccess != NULL)
		*pbSuccess = TRUE;

	return -32768;
}

CPLErr BTRasterBand::SetNoDataValue( double )

{
    return CE_None;
}

/************************************************************************/
/*                            GetUnitType()                             */
/************************************************************************/

static bool approx_equals(float a, float b)
{
    const float epsilon = (float)1e-5;
    return (fabs(a-b) <= epsilon);
}

const char* BTRasterBand::GetUnitType(void)
{
    const BTDataset& ds = *(BTDataset*)poDS;
    float f = ds.m_fVscale;
    if(f == 1.0f)
        return "m";
    if(approx_equals(f, 0.3048f))
        return "ft";
    if(approx_equals(f, 1200.0f/3937.0f))
        return "sft";

    // todo: the BT spec allows for any value for
    // ds.m_fVscale, so rigorous adherence would
    // require testing for all possible units you
    // may want to support, such as km, yards, miles, etc.
    // But m/ft/sft seem to be the top three.

    return "";
} 

/************************************************************************/
/*                            SetUnitType()                             */
/************************************************************************/

CPLErr BTRasterBand::SetUnitType(const char* psz)
{
    BTDataset& ds = *(BTDataset*)poDS;
    if(EQUAL(psz, "m"))
        ds.m_fVscale = 1.0f;
    else if(EQUAL(psz, "ft"))
        ds.m_fVscale = 0.3048f;
    else if(EQUAL(psz, "sft"))
        ds.m_fVscale = 1200.0f / 3937.0f;
    else
        return CE_Failure;


    float fScale = ds.m_fVscale;

    CPL_LSBPTR32(&fScale);

    // Update header's elevation scale field.
    memcpy(ds.abyHeader + 62, &fScale, sizeof(fScale));

    ds.bHeaderModified = TRUE;
    return CE_None;
} 

/************************************************************************/
/* ==================================================================== */
/*                              BTDataset                               */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             BTDataset()                              */
/************************************************************************/

BTDataset::BTDataset()
{
    fpImage = NULL;
    bGeoTransformValid = FALSE;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
    pszProjection = NULL;

    bHeaderModified = FALSE;
}

/************************************************************************/
/*                             ~BTDataset()                             */
/************************************************************************/

BTDataset::~BTDataset()

{
    FlushCache();
    if( fpImage != NULL )
        VSIFCloseL( fpImage );
    CPLFree( pszProjection );
}

/************************************************************************/
/*                             FlushCache()                             */
/*                                                                      */
/*      We override this to include flush out the header block.         */
/************************************************************************/

void BTDataset::FlushCache()

{
    GDALDataset::FlushCache();

    if( !bHeaderModified )
        return;

    bHeaderModified = FALSE;

    VSIFSeekL( fpImage, 0, SEEK_SET );
    VSIFWriteL( abyHeader, 256, 1, fpImage );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr BTDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );

    if( bGeoTransformValid )
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr BTDataset::SetGeoTransform( double *padfTransform )

{
    CPLErr eErr = CE_None;

    memcpy( adfGeoTransform, padfTransform, sizeof(double) * 6 );
    if( adfGeoTransform[2] != 0.0 || adfGeoTransform[4] != 0.0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  ".bt format does not support rotational coefficients in geotransform, ignoring." );
        eErr = CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Compute bounds, and update header info.                         */
/* -------------------------------------------------------------------- */
    double dfLeft, dfRight, dfTop, dfBottom;

    dfLeft = adfGeoTransform[0];
    dfRight = dfLeft + adfGeoTransform[1] * nRasterXSize;
    dfTop = adfGeoTransform[3];
    dfBottom = dfTop + adfGeoTransform[5] * nRasterYSize;

    memcpy( abyHeader + 28, &dfLeft, 8 );
    memcpy( abyHeader + 36, &dfRight, 8 );
    memcpy( abyHeader + 44, &dfBottom, 8 );
    memcpy( abyHeader + 52, &dfTop, 8 );

    CPL_LSBPTR64( abyHeader + 28 );
    CPL_LSBPTR64( abyHeader + 36 );
    CPL_LSBPTR64( abyHeader + 44 );
    CPL_LSBPTR64( abyHeader + 52 );
    
    bHeaderModified = TRUE;

    return eErr;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *BTDataset::GetProjectionRef()

{
    if( pszProjection == NULL )
        return "";
    else
        return pszProjection;
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr BTDataset::SetProjection( const char *pszNewProjection )

{
    CPLErr eErr = CE_None;

    CPLFree( pszProjection );
    pszProjection = CPLStrdup( pszNewProjection );

    bHeaderModified = TRUE;

/* -------------------------------------------------------------------- */
/*      Parse projection.                                               */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS( pszProjection );
    GInt16  nShortTemp;

/* -------------------------------------------------------------------- */
/*      Linear units.                                                   */
/* -------------------------------------------------------------------- */
    if( oSRS.IsGeographic() )
        nShortTemp = 0;
    else 
    {
        double dfLinear = oSRS.GetLinearUnits();

        if( ABS(dfLinear - 0.3048) < 0.0000001 )
            nShortTemp = 2;
        else if( ABS(dfLinear - atof(SRS_UL_US_FOOT_CONV)) < 0.00000001 )
            nShortTemp = 3;
        else
            nShortTemp = 1;
    }

    nShortTemp = CPL_LSBWORD16( 1 );
    memcpy( abyHeader + 22, &nShortTemp, 2 );
    
/* -------------------------------------------------------------------- */
/*      UTM Zone                                                        */
/* -------------------------------------------------------------------- */
    int bNorth;

    nShortTemp = (GInt16) oSRS.GetUTMZone( &bNorth );
    if( bNorth )
        nShortTemp = -nShortTemp;

    nShortTemp = CPL_LSBWORD16( nShortTemp );
    memcpy( abyHeader + 24, &nShortTemp, 2 );
    
/* -------------------------------------------------------------------- */
/*      Datum                                                           */
/* -------------------------------------------------------------------- */
    if( oSRS.GetAuthorityName( "GEOGCS|DATUM" ) != NULL
        && EQUAL(oSRS.GetAuthorityName( "GEOGCS|DATUM" ),"EPSG") )
        nShortTemp = (GInt16) (atoi(oSRS.GetAuthorityCode( "GEOGCS|DATUM" )) + 2000);
    else
        nShortTemp = -2;
    nShortTemp = CPL_LSBWORD16( nShortTemp ); /* datum unknown */
    memcpy( abyHeader + 26, &nShortTemp, 2 );

/* -------------------------------------------------------------------- */
/*      Write out the projection to a .prj file.                        */
/* -------------------------------------------------------------------- */
    const char  *pszPrjFile = CPLResetExtension( GetDescription(), "prj" );
    VSILFILE * fp;

    fp = VSIFOpenL( pszPrjFile, "wt" );
    if( fp != NULL )
    {
        VSIFPrintfL( fp, "%s\n", pszProjection );
        VSIFCloseL( fp );
        abyHeader[60] = 1;
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to write out .prj file." );
        eErr = CE_Failure;
    }

    return eErr;
}


/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *BTDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Verify that this is some form of binterr file.                  */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 256)
        return NULL;

    if( strncmp( (const char *) poOpenInfo->pabyHeader, "binterr", 7 ) != 0 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */
    BTDataset  *poDS;

    poDS = new BTDataset();
 
    memcpy( poDS->abyHeader, poOpenInfo->pabyHeader, 256 );

/* -------------------------------------------------------------------- */
/*      Get the version.                                                */
/* -------------------------------------------------------------------- */
    char szVersion[4];

    strncpy( szVersion, (char *) (poDS->abyHeader + 7), 3 );
    szVersion[3] = '\0';
    poDS->nVersionCode = (int) (atof(szVersion) * 10);

/* -------------------------------------------------------------------- */
/*      Extract core header information, being careful about the        */
/*      version.                                                        */
/* -------------------------------------------------------------------- */
    GInt32 nIntTemp;
    GInt16 nDataSize;
    GDALDataType eType;

    memcpy( &nIntTemp, poDS->abyHeader + 10, 4 );
    poDS->nRasterXSize = CPL_LSBWORD32( nIntTemp );
    
    memcpy( &nIntTemp, poDS->abyHeader + 14, 4 );
    poDS->nRasterYSize = CPL_LSBWORD32( nIntTemp );

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        delete poDS;
        return NULL;
    }

    memcpy( &nDataSize, poDS->abyHeader+18, 2 );
    nDataSize = CPL_LSBWORD16( nDataSize );

    if( poDS->abyHeader[20] != 0 && nDataSize == 4 )
        eType = GDT_Float32;
    else if( poDS->abyHeader[20] == 0 && nDataSize == 4 )
        eType = GDT_Int32;
    else if( poDS->abyHeader[20] == 0 && nDataSize == 2 )
        eType = GDT_Int16;
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  ".bt file data type unknown, got datasize=%d.", 
                  nDataSize );
        delete poDS;
        return NULL;
    }

    /*
        rcg, apr 7/06: read offset 62 for vert. units.
        If zero, assume 1.0 as per spec.

    */
    memcpy( &poDS->m_fVscale, poDS->abyHeader + 62, 4 );
    CPL_LSBPTR32(&poDS->m_fVscale);
    if(poDS->m_fVscale == 0.0f)
        poDS->m_fVscale = 1.0f; 

/* -------------------------------------------------------------------- */
/*      Try to read a .prj file if it is indicated.                     */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS;						

    if( poDS->nVersionCode >= 12 && poDS->abyHeader[60] != 0 )
    {
        const char  *pszPrjFile = CPLResetExtension( poOpenInfo->pszFilename, 
                                                     "prj" );
        VSILFILE *fp;

        fp = VSIFOpenL( pszPrjFile, "rt" );
        if( fp != NULL )
        {
            char *pszBuffer, *pszBufPtr;
            int  nBufMax = 10000;
            int nBytes;

            pszBuffer = (char *) CPLMalloc(nBufMax);
            nBytes = VSIFReadL( pszBuffer, 1, nBufMax-1, fp );
            VSIFCloseL( fp );

            pszBuffer[nBytes] = '\0';

            pszBufPtr = pszBuffer;
            if( oSRS.importFromWkt( &pszBufPtr ) != OGRERR_NONE )
            {
                CPLError( CE_Warning, CPLE_AppDefined, 
                          "Unable to parse .prj file, coordinate system missing." );
            }
            CPLFree( pszBuffer );
        }
    }

/* -------------------------------------------------------------------- */
/*      If we didn't find a .prj file, try to use internal info.        */
/* -------------------------------------------------------------------- */
    if( oSRS.GetRoot() == NULL )
    {
        GInt16 nUTMZone, nDatum, nHUnits;
        
        memcpy( &nUTMZone, poDS->abyHeader + 24, 2 );
        nUTMZone = CPL_LSBWORD16( nUTMZone );
        
        memcpy( &nDatum, poDS->abyHeader + 26, 2 );
        nDatum = CPL_LSBWORD16( nDatum );
        
        memcpy( &nHUnits, poDS->abyHeader + 22, 2 );
        nHUnits = CPL_LSBWORD16( nHUnits );

        if( nUTMZone != 0 )
            oSRS.SetUTM( ABS(nUTMZone), nUTMZone > 0 );
        else if( nHUnits != 0 )
            oSRS.SetLocalCS( "Unknown" );
        
        if( nHUnits == 1 )
            oSRS.SetLinearUnits( SRS_UL_METER, 1.0 );
        else if( nHUnits == 2 )
            oSRS.SetLinearUnits( SRS_UL_FOOT, atof(SRS_UL_FOOT_CONV) );
        else if( nHUnits == 3 )
            oSRS.SetLinearUnits( SRS_UL_US_FOOT, atof(SRS_UL_US_FOOT_CONV) );

        // Translate some of the more obvious old USGS datum codes 
        if( nDatum == 0 )
            nDatum = 6201;
        else if( nDatum == 1 )
            nDatum = 6209;
        else if( nDatum == 2 )
            nDatum = 6210;
        else if( nDatum == 3 )
            nDatum = 6202;
        else if( nDatum == 4 )
            nDatum = 6203;
        else if( nDatum == 6 )
            nDatum = 6222;
        else if( nDatum == 7 )
            nDatum = 6230;
        else if( nDatum == 13 )
            nDatum = 6267;
        else if( nDatum == 14 )
            nDatum = 6269;
        else if( nDatum == 17 )
            nDatum = 6277;
        else if( nDatum == 19 )
            nDatum = 6284;
        else if( nDatum == 21 )
            nDatum = 6301;
        else if( nDatum == 22 )
            nDatum = 6322;
        else if( nDatum == 23 )
            nDatum = 6326;

        if( !oSRS.IsLocal() )
        {
            if( nDatum >= 6000 )
            {
                char szName[32];
                sprintf( szName, "EPSG:%d", nDatum-2000 );
                oSRS.SetWellKnownGeogCS( szName );
            }
            else
                oSRS.SetWellKnownGeogCS( "WGS84" );
        }                
    }

/* -------------------------------------------------------------------- */
/*      Convert coordinate system back to WKT.                          */
/* -------------------------------------------------------------------- */
    if( oSRS.GetRoot() != NULL )
        oSRS.exportToWkt( &poDS->pszProjection );

/* -------------------------------------------------------------------- */
/*      Get georeferencing bounds.                                      */
/* -------------------------------------------------------------------- */
    if( poDS->nVersionCode >= 11 )
    {
        double dfLeft, dfRight, dfTop, dfBottom;

        memcpy( &dfLeft, poDS->abyHeader + 28, 8 );
        CPL_LSBPTR64( &dfLeft );
        
        memcpy( &dfRight, poDS->abyHeader + 36, 8 );
        CPL_LSBPTR64( &dfRight );
        
        memcpy( &dfBottom, poDS->abyHeader + 44, 8 );
        CPL_LSBPTR64( &dfBottom );
        
        memcpy( &dfTop, poDS->abyHeader + 52, 8 );
        CPL_LSBPTR64( &dfTop );

        poDS->adfGeoTransform[0] = dfLeft;
        poDS->adfGeoTransform[1] = (dfRight - dfLeft) / poDS->nRasterXSize;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = dfTop;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = (dfBottom - dfTop) / poDS->nRasterYSize;
        
        poDS->bGeoTransformValid = TRUE;
    }
    
/* -------------------------------------------------------------------- */
/*      Re-open the file with the desired access.                       */
/* -------------------------------------------------------------------- */

    if( poOpenInfo->eAccess == GA_Update )
        poDS->fpImage = VSIFOpenL( poOpenInfo->pszFilename, "rb+" );
    else
        poDS->fpImage = VSIFOpenL( poOpenInfo->pszFilename, "rb" );

    if( poDS->fpImage == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to re-open %s within BT driver.\n",
                  poOpenInfo->pszFilename );
        return NULL;
    }
    poDS->eAccess = poOpenInfo->eAccess;

/* -------------------------------------------------------------------- */
/*      Create band information objects                                 */
/* -------------------------------------------------------------------- */
    poDS->SetBand( 1, new BTRasterBand( poDS, poDS->fpImage, eType ) );

#ifdef notdef
    poDS->bGeoTransformValid = 
        GDALReadWorldFile( poOpenInfo->pszFilename, ".wld", 
                           poDS->adfGeoTransform );
#endif

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *BTDataset::Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType,
                                CPL_UNUSED char ** papszOptions )

{

/* -------------------------------------------------------------------- */
/*      Verify input options.                                           */
/* -------------------------------------------------------------------- */
    if( eType != GDT_Int16 && eType != GDT_Int32 && eType != GDT_Float32 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
              "Attempt to create .bt dataset with an illegal\n"
              "data type (%s), only Int16, Int32 and Float32 supported.\n",
              GDALGetDataTypeName(eType) );

        return NULL;
    }

    if( nBands != 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
               "Attempt to create .bt dataset with %d bands, only 1 supported",
                  nBands );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try to create the file.                                         */
/* -------------------------------------------------------------------- */
    VSILFILE        *fp;

    fp = VSIFOpenL( pszFilename, "wb" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file `%s' failed.\n",
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Setup base header.                                              */
/* -------------------------------------------------------------------- */
    unsigned char abyHeader[256];
    GInt32 nTemp;
    GInt16 nShortTemp;

    memset( abyHeader, 0, 256 );
    memcpy( abyHeader, "binterr1.3", 10 );
    
    nTemp = CPL_LSBWORD32( nXSize );
    memcpy( abyHeader+10, &nTemp, 4 );
    
    nTemp = CPL_LSBWORD32( nYSize );
    memcpy( abyHeader+14, &nTemp, 4 );

    nShortTemp = (GInt16) (CPL_LSBWORD16( GDALGetDataTypeSize( eType ) / 8 ));
    memcpy( abyHeader + 18, &nShortTemp, 2 );
    
    if( eType == GDT_Float32 )
        abyHeader[20] = 1;
    else
        abyHeader[20] = 0;
    
    nShortTemp = CPL_LSBWORD16( 1 ); /* meters */
    memcpy( abyHeader + 22, &nShortTemp, 2 );
    
    nShortTemp = CPL_LSBWORD16( 0 ); /* not utm */
    memcpy( abyHeader + 24, &nShortTemp, 2 );
    
    nShortTemp = CPL_LSBWORD16( -2 ); /* datum unknown */
    memcpy( abyHeader + 26, &nShortTemp, 2 );
    
/* -------------------------------------------------------------------- */
/*      Set dummy extents.                                              */
/* -------------------------------------------------------------------- */
    double dfLeft=0, dfRight=nXSize, dfTop=nYSize, dfBottom=0;

    memcpy( abyHeader + 28, &dfLeft, 8 );
    memcpy( abyHeader + 36, &dfRight, 8 );
    memcpy( abyHeader + 44, &dfBottom, 8 );
    memcpy( abyHeader + 52, &dfTop, 8 );

    CPL_LSBPTR64( abyHeader + 28 );
    CPL_LSBPTR64( abyHeader + 36 );
    CPL_LSBPTR64( abyHeader + 44 );
    CPL_LSBPTR64( abyHeader + 52 );
    
/* -------------------------------------------------------------------- */
/*      Set dummy scale.                                                */
/* -------------------------------------------------------------------- */
    float fScale = 1.0;

    memcpy( abyHeader + 62, &fScale, 4 );
    CPL_LSBPTR32( abyHeader + 62 );
    
/* -------------------------------------------------------------------- */
/*      Write to disk.                                                  */
/* -------------------------------------------------------------------- */
    VSIFWriteL( (void *) abyHeader, 256, 1, fp );
    if( VSIFSeekL( fp, (GDALGetDataTypeSize(eType)/8) * nXSize * (vsi_l_offset)nYSize - 1, 
                   SEEK_CUR ) != 0 
        || VSIFWriteL( abyHeader+255, 1, 1, fp ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Failed to extent file to its full size, out of disk space?"
                  );

        VSIFCloseL( fp );
        VSIUnlink( pszFilename );
        return NULL;
    }

    VSIFCloseL( fp );

    return (GDALDataset *) GDALOpen( pszFilename, GA_Update );
}

/************************************************************************/
/*                          GDALRegister_BT()                           */
/************************************************************************/

void GDALRegister_BT()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "BT" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "BT" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "VTP .bt (Binary Terrain) 1.3 Format" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "frmt_various.html#BT" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "bt" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                                   "Int16 Int32 Float32" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = BTDataset::Open;
        poDriver->pfnCreate = BTDataset::Create;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
