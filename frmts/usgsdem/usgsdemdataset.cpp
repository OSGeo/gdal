/******************************************************************************
 * $Id$
 *
 * Project:  USGS DEM Driver
 * Purpose:  All reader for USGS DEM Reader
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 * Portions of this module derived from the VTP USGS DEM driver by Ben
 * Discoe, see http://www.vterrain.org
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam <warmerdam@pobox.com>
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
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_USGSDEM(void);
CPL_C_END

typedef struct {
    double	x;
    double	y;
} DPoint2;

#define USGSDEM_NODATA	-32767

GDALDataset *USGSDEMCreateCopy( const char *, GDALDataset *, int, char **,
                                GDALProgressFunc pfnProgress, 
                                void * pProgressData );


/************************************************************************/
/*                              ReadInt()                               */
/************************************************************************/

static int ReadInt( VSILFILE *fp )
{
    int nVal = 0;
    char c;
    int nRead = 0;
    vsi_l_offset nOffset = VSIFTellL(fp);

    while (TRUE)
    {
        if (VSIFReadL(&c, 1, 1, fp) != 1)
        {
            return 0;
        }
        else
            nRead ++;
        if (!isspace((int)c))
            break;
    }

    int nSign = 1;
    if (c == '-')
        nSign = -1;
    else if (c == '+')
        nSign = 1;
    else if (c >= '0' && c <= '9')
        nVal = c - '0';
    else
    {
        VSIFSeekL(fp, nOffset + nRead, SEEK_SET);
        return 0;
    }

    while (TRUE)
    {
        if (VSIFReadL(&c, 1, 1, fp) != 1)
            return nSign * nVal;
        nRead ++;
        if (c >= '0' && c <= '9')
            nVal = nVal * 10 + (c - '0');
        else
        {
            VSIFSeekL(fp, nOffset + (nRead - 1), SEEK_SET);
            return nSign * nVal;
        }
    }
}

typedef struct
{
    VSILFILE *fp;
    int max_size;
    char* buffer;
    int buffer_size;
    int cur_index;
} Buffer;

/************************************************************************/
/*                       USGSDEMRefillBuffer()                          */
/************************************************************************/

static void USGSDEMRefillBuffer( Buffer* psBuffer )
{
    memmove(psBuffer->buffer, psBuffer->buffer + psBuffer->cur_index,
            psBuffer->buffer_size - psBuffer->cur_index);

    psBuffer->buffer_size -= psBuffer->cur_index;
    psBuffer->buffer_size += VSIFReadL(psBuffer->buffer + psBuffer->buffer_size,
                                       1, psBuffer->max_size - psBuffer->buffer_size,
                                       psBuffer->fp);
    psBuffer->cur_index = 0;
}

/************************************************************************/
/*               USGSDEMReadIntFromBuffer()                             */
/************************************************************************/

static int USGSDEMReadIntFromBuffer( Buffer* psBuffer )
{
    int nVal = 0;
    char c;

    while (TRUE)
    {
        if (psBuffer->cur_index >= psBuffer->buffer_size)
        {
            USGSDEMRefillBuffer(psBuffer);
            if (psBuffer->cur_index >= psBuffer->buffer_size)
            {
                return 0;
            }
        }

        c = psBuffer->buffer[psBuffer->cur_index];
        psBuffer->cur_index ++;
        if (!isspace((int)c))
            break;
    }

    int nSign = 1;
    if (c == '-')
        nSign = -1;
    else if (c == '+')
        nSign = 1;
    else if (c >= '0' && c <= '9')
        nVal = c - '0';
    else
    {
        return 0;
    }

    while (TRUE)
    {
        if (psBuffer->cur_index >= psBuffer->buffer_size)
        {
            USGSDEMRefillBuffer(psBuffer);
            if (psBuffer->cur_index >= psBuffer->buffer_size)
            {
                return nSign * nVal;
            }
        }

        c = psBuffer->buffer[psBuffer->cur_index];
        if (c >= '0' && c <= '9')
        {
            psBuffer->cur_index ++;
            nVal = nVal * 10 + (c - '0');
        }
        else
            return nSign * nVal;
    }
}

/************************************************************************/
/*                USGSDEMReadDoubleFromBuffer()                         */
/************************************************************************/

static double USGSDEMReadDoubleFromBuffer( Buffer* psBuffer, int nCharCount )

{
    int     i;

    if (psBuffer->cur_index + nCharCount > psBuffer->buffer_size)
    {
        USGSDEMRefillBuffer(psBuffer);
        if (psBuffer->cur_index + nCharCount > psBuffer->buffer_size)
        {
            return 0;
        }
    }

    char* szPtr = psBuffer->buffer + psBuffer->cur_index;
    char backupC = szPtr[nCharCount];
    szPtr[nCharCount] = 0;
    for( i = 0; i < nCharCount; i++ )
    {
        if( szPtr[i] == 'D' )
            szPtr[i] = 'E';
    }

    double dfVal = CPLAtof(szPtr);
    szPtr[nCharCount] = backupC;
    psBuffer->cur_index += nCharCount;

    return dfVal;
}

/************************************************************************/
/*                              DConvert()                              */
/************************************************************************/

static double DConvert( VSILFILE *fp, int nCharCount )

{
    char	szBuffer[100];
    int		i;

    VSIFReadL( szBuffer, nCharCount, 1, fp );
    szBuffer[nCharCount] = '\0';

    for( i = 0; i < nCharCount; i++ )
    {
        if( szBuffer[i] == 'D' )
            szBuffer[i] = 'E';
    }

    return CPLAtof(szBuffer);
}

/************************************************************************/
/* ==================================================================== */
/*				USGSDEMDataset				*/
/* ==================================================================== */
/************************************************************************/

class USGSDEMRasterBand;

class USGSDEMDataset : public GDALPamDataset
{
    friend class USGSDEMRasterBand;

    int         nDataStartOffset;
    GDALDataType eNaturalDataFormat;

    double      adfGeoTransform[6];
    char        *pszProjection; 

    double      fVRes;

    const char  *pszUnits; 

    int         LoadFromFile( VSILFILE * );

    VSILFILE	*fp;

  public:
                USGSDEMDataset();
		~USGSDEMDataset();
    
    static int  Identify( GDALOpenInfo * );
    static GDALDataset *Open( GDALOpenInfo * );
    CPLErr 	GetGeoTransform( double * padfTransform );
    const char *GetProjectionRef();
};

/************************************************************************/
/* ==================================================================== */
/*                            USGSDEMRasterBand                         */
/* ==================================================================== */
/************************************************************************/

class USGSDEMRasterBand : public GDALPamRasterBand
{
    friend class USGSDEMDataset;

  public:

    		USGSDEMRasterBand( USGSDEMDataset * );
    
    virtual const char *GetUnitType();
    virtual double GetNoDataValue( int *pbSuccess = NULL );
    virtual CPLErr IReadBlock( int, int, void * );
};


/************************************************************************/
/*                           USGSDEMRasterBand()                            */
/************************************************************************/

USGSDEMRasterBand::USGSDEMRasterBand( USGSDEMDataset *poDS )

{
    this->poDS = poDS;
    this->nBand = 1;

    eDataType = poDS->eNaturalDataFormat;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = poDS->GetRasterYSize();

}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr USGSDEMRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                      void * pImage )

{
    double	dfYMin;
    int		bad = FALSE;
    USGSDEMDataset *poGDS = (USGSDEMDataset *) poDS;

/* -------------------------------------------------------------------- */
/*      Initialize image buffer to nodata value.                        */
/* -------------------------------------------------------------------- */
    for( int k = GetXSize() * GetYSize() - 1; k >= 0; k-- )
    {
        if( GetRasterDataType() == GDT_Int16 )
            ((GInt16 *) pImage)[k] = USGSDEM_NODATA;
        else
            ((float *) pImage)[k] = USGSDEM_NODATA;
    }

/* -------------------------------------------------------------------- */
/*      Seek to data.                                                   */
/* -------------------------------------------------------------------- */
    VSIFSeekL(poGDS->fp, poGDS->nDataStartOffset, 0);

    dfYMin = poGDS->adfGeoTransform[3] 
        + (GetYSize()-0.5) * poGDS->adfGeoTransform[5];

/* -------------------------------------------------------------------- */
/*      Read all the profiles into the image buffer.                    */
/* -------------------------------------------------------------------- */

    Buffer sBuffer;
    sBuffer.max_size = 32768;
    sBuffer.buffer = (char*) CPLMalloc(sBuffer.max_size + 1);
    sBuffer.fp = poGDS->fp;
    sBuffer.buffer_size = 0;
    sBuffer.cur_index = 0;

    for( int i = 0; i < GetXSize(); i++)
    {
        int	njunk, nCPoints, lygap;
        double	djunk, dxStart, dyStart, dfElevOffset;

        njunk = USGSDEMReadIntFromBuffer(&sBuffer);
        njunk = USGSDEMReadIntFromBuffer(&sBuffer);
        nCPoints = USGSDEMReadIntFromBuffer(&sBuffer);
        njunk = USGSDEMReadIntFromBuffer(&sBuffer);

        dxStart = USGSDEMReadDoubleFromBuffer(&sBuffer, 24);
        dyStart = USGSDEMReadDoubleFromBuffer(&sBuffer, 24);
        dfElevOffset = USGSDEMReadDoubleFromBuffer(&sBuffer, 24);
        djunk = USGSDEMReadDoubleFromBuffer(&sBuffer, 24);
        djunk = USGSDEMReadDoubleFromBuffer(&sBuffer, 24);

        if( EQUALN(poGDS->pszProjection,"GEOGCS",6) )
            dyStart = dyStart / 3600.0;

        lygap = (int)((dfYMin - dyStart)/poGDS->adfGeoTransform[5]+ 0.5);

        for (int j=lygap; j < (nCPoints+(int)lygap); j++)
        {
            int		iY = GetYSize() - j - 1;
            int         nElev;

            nElev = USGSDEMReadIntFromBuffer(&sBuffer);
            
            if (iY < 0 || iY >= GetYSize() )
                bad = TRUE;
            else if( nElev == USGSDEM_NODATA )
                /* leave in output buffer as nodata */;
            else
            {
                float fComputedElev = 
                    (float)(nElev * poGDS->fVRes + dfElevOffset);

                if( GetRasterDataType() == GDT_Int16 )
                {
                    ((GInt16 *) pImage)[i + iY*GetXSize()] = 
                        (GInt16) fComputedElev;
                }
                else
                {
                    ((float *) pImage)[i + iY*GetXSize()] = fComputedElev;
                }
            }
        }
    }
    CPLFree(sBuffer.buffer);

    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double USGSDEMRasterBand::GetNoDataValue( int *pbSuccess )

{
    if( pbSuccess != NULL )
        *pbSuccess = TRUE;

    return USGSDEM_NODATA;
}

/************************************************************************/
/*                            GetUnitType()                             */
/************************************************************************/
const char *USGSDEMRasterBand::GetUnitType()
{
    USGSDEMDataset *poGDS = (USGSDEMDataset *) poDS;

    return poGDS->pszUnits;
}

/************************************************************************/
/* ==================================================================== */
/*				USGSDEMDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           USGSDEMDataset()                           */
/************************************************************************/

USGSDEMDataset::USGSDEMDataset()

{
    fp = NULL;
    pszProjection = NULL;
}

/************************************************************************/
/*                            ~USGSDEMDataset()                         */
/************************************************************************/

USGSDEMDataset::~USGSDEMDataset()

{
    FlushCache();

    CPLFree( pszProjection );
    if( fp != NULL )
        VSIFCloseL( fp );
}

/************************************************************************/
/*                            LoadFromFile()                            */
/*                                                                      */
/*      If the data from DEM is in meters, then values are stored as    */
/*      shorts. If DEM data is in feet, then height data will be        */
/*      stored in float, to preserve the precision of the original      */
/*      data. returns true if the file was successfully opened and      */
/*      read.                                                           */
/************************************************************************/

int USGSDEMDataset::LoadFromFile(VSILFILE *InDem)
{
    int		i, j;
    int		nRow, nColumn;
    int		nVUnit, nGUnit;
    double 	dxdelta, dydelta;
    double	dElevMax, dElevMin;
    int 	bNewFormat;
    int		nCoordSystem;
    int		nProfiles;
    char	szDateBuffer[5];
    DPoint2	corners[4];			// SW, NW, NE, SE
    DPoint2	extent_min, extent_max;
    int		iUTMZone;

    // check for version of DEM format
    VSIFSeekL(InDem, 864, 0);

    // Read DEM into matrix
    nRow = ReadInt(InDem);
    nColumn = ReadInt(InDem);
    bNewFormat = ((nRow!=1)||(nColumn!=1));
    if (bNewFormat)
    {
        VSIFSeekL(InDem, 1024, 0); 	// New Format
        i = ReadInt(InDem);
        j = ReadInt(InDem);
        if ((i!=1)||(j!=1 && j != 0))	// File OK?
        {
            VSIFSeekL(InDem, 893, 0); 	// Undocumented Format (39109h1.dem)
            i = ReadInt(InDem);
            j = ReadInt(InDem);
            if ((i!=1)||(j!=1))			// File OK?
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Does not appear to be a USGS DEM file." );
                return FALSE;
            }
            else
                nDataStartOffset = 893;
        }
        else
            nDataStartOffset = 1024;
    }
    else
        nDataStartOffset = 864;

    VSIFSeekL(InDem, 156, 0);
    nCoordSystem = ReadInt(InDem);
    iUTMZone = ReadInt(InDem);

    VSIFSeekL(InDem, 528, 0);
    nGUnit = ReadInt(InDem);
    nVUnit = ReadInt(InDem);

    // Vertical Units in meters
    if (nVUnit==1)
        pszUnits = "ft";
    else
        pszUnits = "m";

    VSIFSeekL(InDem, 816, 0);
    dxdelta = DConvert(InDem, 12);
    dydelta = DConvert(InDem, 12);
    fVRes = DConvert(InDem, 12);

/* -------------------------------------------------------------------- */
/*      Should we treat this as floating point, or GInt16.              */
/* -------------------------------------------------------------------- */
    if (nVUnit==1 || fVRes < 1.0)
        eNaturalDataFormat = GDT_Float32;
    else
        eNaturalDataFormat = GDT_Int16;

/* -------------------------------------------------------------------- */
/*      Read four corner coordinates.                                   */
/* -------------------------------------------------------------------- */
    VSIFSeekL(InDem, 546, 0);
    for (i = 0; i < 4; i++)
    {
        corners[i].x = DConvert(InDem, 24);
        corners[i].y = DConvert(InDem, 24);
    }
    
    // find absolute extents of raw vales
    extent_min.x = MIN(corners[0].x, corners[1].x);
    extent_max.x = MAX(corners[2].x, corners[3].x);
    extent_min.y = MIN(corners[0].y, corners[3].y);
    extent_max.y = MAX(corners[1].y, corners[2].y);

    dElevMin = DConvert(InDem, 48);
    dElevMax = DConvert(InDem, 48);

    VSIFSeekL(InDem, 858, 0);
    nProfiles = ReadInt(InDem);

/* -------------------------------------------------------------------- */
/*      Collect the spatial reference system.                           */
/* -------------------------------------------------------------------- */
    OGRSpatialReference sr;
    int bNAD83 =TRUE;

    // OLD format header ends at byte 864
    if (bNewFormat)
    {
        char szHorzDatum[3];

        // year of data compilation
        VSIFSeekL(InDem, 876, 0);
        VSIFReadL(szDateBuffer, 4, 1, InDem);
        szDateBuffer[4] = 0;

        // Horizontal datum
        // 1=North American Datum 1927 (NAD 27)
        // 2=World Geodetic System 1972 (WGS 72)
        // 3=WGS 84
        // 4=NAD 83
        // 5=Old Hawaii Datum
        // 6=Puerto Rico Datum
        int datum;
        VSIFSeekL(InDem, 890, 0);
        VSIFReadL( szHorzDatum, 1, 2, InDem );
        szHorzDatum[2] = '\0';
        datum = atoi(szHorzDatum);
        switch (datum)
        {
          case 1:
            sr.SetWellKnownGeogCS( "NAD27" );
            bNAD83 = FALSE;
            break;

          case 2:
            sr.SetWellKnownGeogCS( "WGS72" );
            break;

          case 3:
            sr.SetWellKnownGeogCS( "WGS84" );
            break;

          case 4:
            sr.SetWellKnownGeogCS( "NAD83" );
            break;

          case -9:
            break;

          default:
            sr.SetWellKnownGeogCS( "NAD27" );
            break;
        }
    }
    else
    {
        sr.SetWellKnownGeogCS( "NAD27" );
        bNAD83 = FALSE;
    }

    if (nCoordSystem == 1)	// UTM
        sr.SetUTM( iUTMZone, TRUE );

    else if (nCoordSystem == 2)	// state plane
    {
        if( nGUnit == 1 )
            sr.SetStatePlane( iUTMZone, bNAD83,
                              "Foot", CPLAtof(SRS_UL_US_FOOT_CONV) );
        else
            sr.SetStatePlane( iUTMZone, bNAD83 );
    }

    sr.exportToWkt( &pszProjection );

/* -------------------------------------------------------------------- */
/*      For UTM we use the extents (really the UTM coordinates of       */
/*      the lat/long corners of the quad) to determine the size in      */
/*      pixels and lines, but we have to make the anchors be modulus    */
/*      the pixel size which what really gets used.                     */
/* -------------------------------------------------------------------- */
    if (nCoordSystem == 1          // UTM
        || nCoordSystem == 2 	   // State Plane
        || nCoordSystem == -9999 ) // unknown
    {
        int	njunk;
        double  dxStart;

        // expand extents modulus the pixel size.
        extent_min.y = floor(extent_min.y/dydelta) * dydelta;
        extent_max.y = ceil(extent_max.y/dydelta) * dydelta;

        // Forceably compute X extents based on first profile and pixelsize.
        VSIFSeekL(InDem, nDataStartOffset, 0);
        njunk = ReadInt(InDem);
        njunk = ReadInt(InDem);
        njunk = ReadInt(InDem);
        njunk = ReadInt(InDem);
        dxStart = DConvert(InDem, 24);
        
        nRasterYSize = (int) ((extent_max.y - extent_min.y)/dydelta + 1.5);
        nRasterXSize = nProfiles;

        adfGeoTransform[0] = dxStart - dxdelta/2.0;
        adfGeoTransform[1] = dxdelta;
        adfGeoTransform[2] = 0.0;
        adfGeoTransform[3] = extent_max.y + dydelta/2.0;
        adfGeoTransform[4] = 0.0;
        adfGeoTransform[5] = -dydelta;
    }
/* -------------------------------------------------------------------- */
/*      Geographic -- use corners directly.                             */
/* -------------------------------------------------------------------- */
    else
    {
        nRasterYSize = (int) ((extent_max.y - extent_min.y)/dydelta + 1.5);
        nRasterXSize = nProfiles;

        // Translate extents from arc-seconds to decimal degrees.
        adfGeoTransform[0] = (extent_min.x - dxdelta/2.0) / 3600.0;
        adfGeoTransform[1] = dxdelta / 3600.0;
        adfGeoTransform[2] = 0.0;
        adfGeoTransform[3] = (extent_max.y + dydelta/2.0) / 3600.0;
        adfGeoTransform[4] = 0.0;
        adfGeoTransform[5] = (-dydelta) / 3600.0;
    }

    if (!GDALCheckDatasetDimensions(nRasterXSize, nRasterYSize))
    {
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr USGSDEMDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *USGSDEMDataset::GetProjectionRef()

{
    return pszProjection;
}


/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int USGSDEMDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes < 200 )
        return FALSE;

    if( !EQUALN((const char *) poOpenInfo->pabyHeader+156, "     0",6)
        && !EQUALN((const char *) poOpenInfo->pabyHeader+156, "     1",6)
        && !EQUALN((const char *) poOpenInfo->pabyHeader+156, "     2",6) 
        && !EQUALN((const char *) poOpenInfo->pabyHeader+156, "     3",6)
        && !EQUALN((const char *) poOpenInfo->pabyHeader+156, " -9999",6) )
        return FALSE;

    if( !EQUALN((const char *) poOpenInfo->pabyHeader+150, "     1",6) 
        && !EQUALN((const char *) poOpenInfo->pabyHeader+150, "     4",6))
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *USGSDEMDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( !Identify( poOpenInfo ) )
        return NULL;

    VSILFILE* fp = VSIFOpenL(poOpenInfo->pszFilename, "rb");
    if (fp == NULL)
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    USGSDEMDataset 	*poDS;

    poDS = new USGSDEMDataset();

    poDS->fp = fp;
    
/* -------------------------------------------------------------------- */
/*	Read the file.							*/
/* -------------------------------------------------------------------- */
    if( !poDS->LoadFromFile( poDS->fp ) )
    {
        delete poDS;
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The USGSDEM driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->SetBand( 1, new USGSDEMRasterBand( poDS ));

    poDS->SetMetadataItem( GDALMD_AREA_OR_POINT, GDALMD_AOP_POINT );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Open overviews.                                                 */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

/************************************************************************/
/*                        GDALRegister_USGSDEM()                        */
/************************************************************************/

void GDALRegister_USGSDEM()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "USGSDEM" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "USGSDEM" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, 
                                   "dem" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "USGS Optional ASCII DEM (and CDED)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_usgsdem.html" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Int16" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, 
"<CreationOptionList>"
"   <Option name='PRODUCT' type='string-select' description='Specific Product Type'>"
"       <Value>DEFAULT</Value>"
"       <Value>CDED50K</Value>"
"   </Option>"
"   <Option name='TOPLEFT' type='string' description='Top left product corner (ie. 117d15w,52d30n'/>"
"   <Option name='RESAMPLE' type='string-select' description='Resampling kernel to use if resampled.'>"
"       <Value>Nearest</Value>"
"       <Value>Bilinear</Value>"
"       <Value>Cubic</Value>"
"       <Value>CubicSpline</Value>"
"   </Option>"
"   <Option name='TEMPLATE' type='string' description='File to default metadata from.'/>"
"   <Option name='DEMLevelCode' type='int' description='DEM Level (1, 2 or 3 if set)'/>"
"   <Option name='DataSpecVersion' type='int' description='Data and Specification version/revision (eg. 1020)'/>"
"   <Option name='PRODUCER' type='string' description='Producer Agency (up to 60 characters)'/>"
"   <Option name='OriginCode' type='string' description='Origin code (up to 4 characters, YT for Yukon)'/>"
"   <Option name='ProcessCode' type='string' description='Processing Code (8=ANUDEM, 9=FME, A=TopoGrid)'/>"
"   <Option name='ZRESOLUTION' type='float' description='Scaling factor for elevation values'/>"
"   <Option name='NTS' type='string' description='NTS Mapsheet name, used to derive TOPLEFT.'/>"
"   <Option name='INTERNALNAME' type='string' description='Dataset name written into file header.'/>"
"</CreationOptionList>" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = USGSDEMDataset::Open;
        poDriver->pfnCreateCopy = USGSDEMCreateCopy;
        poDriver->pfnIdentify = USGSDEMDataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
