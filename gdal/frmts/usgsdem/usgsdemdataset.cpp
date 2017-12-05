/******************************************************************************
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
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "ogr_spatialref.h"

#include <algorithm>

CPL_CVSID("$Id$")

typedef struct {
    double      x;
    double      y;
} DPoint2;

static const int USGSDEM_NODATA = -32767;

GDALDataset *USGSDEMCreateCopy( const char *, GDALDataset *, int, char **,
                                GDALProgressFunc pfnProgress,
                                void * pProgressData );

/************************************************************************/
/*                              ReadInt()                               */
/************************************************************************/

static int ReadInt( VSILFILE *fp )
{
    char c;
    int nRead = 0;
    char szBuffer[12];
    bool bInProlog = true;

    while( true )
    {
        if (VSIFReadL(&c, 1, 1, fp) != 1)
        {
            return 0;
        }
        if( bInProlog )
        {
            if ( !isspace( static_cast<int>(c) ) )
            {
                bInProlog = false;
            }
        }
        if( !bInProlog )
        {
            if( c != '-' && c != '+' && !(c >= '0' && c <= '9') )
            {
                CPL_IGNORE_RET_VAL(VSIFSeekL(fp, VSIFTellL(fp) - 1, SEEK_SET));
                break;
            }
            if( nRead < 11 )
                szBuffer[nRead] = c;
            nRead ++;
        }
    }
    szBuffer[std::min(nRead, 11)] = 0;
    return atoi(szBuffer);
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
    psBuffer->buffer_size += static_cast<int>(VSIFReadL(psBuffer->buffer + psBuffer->buffer_size,
                                       1, psBuffer->max_size - psBuffer->buffer_size,
                                       psBuffer->fp));
    psBuffer->cur_index = 0;
}

/************************************************************************/
/*               USGSDEMReadIntFromBuffer()                             */
/************************************************************************/

static int USGSDEMReadIntFromBuffer( Buffer* psBuffer, int* pbSuccess = NULL )
{
    char c;

    while (true)
    {
        if (psBuffer->cur_index >= psBuffer->buffer_size)
        {
            USGSDEMRefillBuffer(psBuffer);
            if (psBuffer->cur_index >= psBuffer->buffer_size)
            {
                if( pbSuccess ) *pbSuccess = FALSE;
                return 0;
            }
        }

        c = psBuffer->buffer[psBuffer->cur_index];
        psBuffer->cur_index ++;
        if ( !isspace( static_cast<int>(c) ) )
            break;
    }

    GIntBig nVal = 0;
    int nSign = 1;
    if (c == '-')
        nSign = -1;
    else if (c == '+')
        nSign = 1;
    else if (c >= '0' && c <= '9')
        nVal = c - '0';
    else
    {
        if( pbSuccess ) *pbSuccess = FALSE;
        return 0;
    }

    while (true)
    {
        if (psBuffer->cur_index >= psBuffer->buffer_size)
        {
            USGSDEMRefillBuffer(psBuffer);
            if (psBuffer->cur_index >= psBuffer->buffer_size)
            {
                if( pbSuccess ) *pbSuccess = TRUE;
                return static_cast<int>(nSign * nVal);
            }
        }

        c = psBuffer->buffer[psBuffer->cur_index];
        if (c >= '0' && c <= '9')
        {
            psBuffer->cur_index ++;
            if( nVal * nSign < INT_MAX && nVal * nSign > INT_MIN )
            {
                nVal = nVal * 10 + (c - '0');
                if( nVal * nSign > INT_MAX )
                {
                    nVal = INT_MAX;
                    nSign = 1;
                }
                else if( nVal * nSign < INT_MIN )
                {
                    nVal = INT_MIN;
                    nSign = 1;
                }
            }
        }
        else
        {
            if( pbSuccess ) *pbSuccess = TRUE;
            return static_cast<int>(nSign * nVal);
        }
    }
}

/************************************************************************/
/*                USGSDEMReadDoubleFromBuffer()                         */
/************************************************************************/

static double USGSDEMReadDoubleFromBuffer( Buffer* psBuffer, int nCharCount, int *pbSuccess = NULL)

{
    if (psBuffer->cur_index + nCharCount > psBuffer->buffer_size)
    {
        USGSDEMRefillBuffer(psBuffer);
        if (psBuffer->cur_index + nCharCount > psBuffer->buffer_size)
        {
            if( pbSuccess ) *pbSuccess = FALSE;
            return 0;
        }
    }

    char* szPtr = psBuffer->buffer + psBuffer->cur_index;
    char backupC = szPtr[nCharCount];
    szPtr[nCharCount] = 0;
    for( int i = 0; i < nCharCount; i++ )
    {
        if( szPtr[i] == 'D' )
            szPtr[i] = 'E';
    }

    double dfVal = CPLAtof(szPtr);
    szPtr[nCharCount] = backupC;
    psBuffer->cur_index += nCharCount;

    if( pbSuccess ) *pbSuccess = TRUE;
    return dfVal;
}

/************************************************************************/
/*                              DConvert()                              */
/************************************************************************/

static double DConvert( VSILFILE *fp, int nCharCount )

{
    char szBuffer[100];

    CPL_IGNORE_RET_VAL(VSIFReadL( szBuffer, nCharCount, 1, fp ));
    szBuffer[nCharCount] = '\0';

    for( int i = 0; i < nCharCount; i++ )
    {
        if( szBuffer[i] == 'D' )
            szBuffer[i] = 'E';
    }

    return CPLAtof(szBuffer);
}

/************************************************************************/
/* ==================================================================== */
/*                              USGSDEMDataset                          */
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

    VSILFILE    *fp;

  public:
                USGSDEMDataset();
                ~USGSDEMDataset();

    static int  Identify( GDALOpenInfo * );
    static GDALDataset *Open( GDALOpenInfo * );
    CPLErr GetGeoTransform( double * padfTransform ) override;
    const char *GetProjectionRef() override;
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
                explicit USGSDEMRasterBand( USGSDEMDataset * );

    virtual const char *GetUnitType() override;
    virtual double GetNoDataValue( int *pbSuccess = NULL ) override;
    virtual CPLErr IReadBlock( int, int, void * ) override;
};

/************************************************************************/
/*                           USGSDEMRasterBand()                            */
/************************************************************************/

USGSDEMRasterBand::USGSDEMRasterBand( USGSDEMDataset *poDSIn )

{
    this->poDS = poDSIn;
    this->nBand = 1;

    eDataType = poDSIn->eNaturalDataFormat;

    nBlockXSize = poDSIn->GetRasterXSize();
    nBlockYSize = poDSIn->GetRasterYSize();
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr USGSDEMRasterBand::IReadBlock( CPL_UNUSED int nBlockXOff,
                                      CPL_UNUSED int nBlockYOff,
                                      void * pImage )

{
    /* int bad = FALSE; */
    USGSDEMDataset *poGDS = reinterpret_cast<USGSDEMDataset *>( poDS );

/* -------------------------------------------------------------------- */
/*      Initialize image buffer to nodata value.                        */
/* -------------------------------------------------------------------- */
    for( int k = GetXSize() * GetYSize() - 1; k >= 0; k-- )
    {
        if( GetRasterDataType() == GDT_Int16 )
            reinterpret_cast<GInt16 *>( pImage )[k] = USGSDEM_NODATA;
        else
            reinterpret_cast<float *>( pImage )[k] = USGSDEM_NODATA;
    }

/* -------------------------------------------------------------------- */
/*      Seek to data.                                                   */
/* -------------------------------------------------------------------- */
    CPL_IGNORE_RET_VAL(VSIFSeekL(poGDS->fp, poGDS->nDataStartOffset, 0));

    double dfYMin = poGDS->adfGeoTransform[3]
        + (GetYSize()-0.5) * poGDS->adfGeoTransform[5];

/* -------------------------------------------------------------------- */
/*      Read all the profiles into the image buffer.                    */
/* -------------------------------------------------------------------- */

    Buffer sBuffer;
    sBuffer.max_size = 32768;
    sBuffer.buffer
        = reinterpret_cast<char *>( CPLMalloc( sBuffer.max_size + 1 ) );
    sBuffer.fp = poGDS->fp;
    sBuffer.buffer_size = 0;
    sBuffer.cur_index = 0;

    for( int i = 0; i < GetXSize(); i++)
    {
        int bSuccess;
        /* njunk = */ USGSDEMReadIntFromBuffer(&sBuffer, &bSuccess);
        if( bSuccess )
        /* njunk = */ USGSDEMReadIntFromBuffer(&sBuffer, &bSuccess);
        const int nCPoints = (bSuccess) ? USGSDEMReadIntFromBuffer(&sBuffer, &bSuccess) : 0;
        /* njunk = */ USGSDEMReadIntFromBuffer(&sBuffer, &bSuccess);

        if( bSuccess )
        /* dxStart = */ USGSDEMReadDoubleFromBuffer(&sBuffer, 24, &bSuccess);
        double dyStart = (bSuccess) ? USGSDEMReadDoubleFromBuffer(&sBuffer, 24, &bSuccess) : 0;
        const double dfElevOffset = (bSuccess) ? USGSDEMReadDoubleFromBuffer(&sBuffer, 24, &bSuccess) : 0;
        if( bSuccess )
        /* djunk = */ USGSDEMReadDoubleFromBuffer(&sBuffer, 24, &bSuccess);
        if( bSuccess )
        /* djunk = */ USGSDEMReadDoubleFromBuffer(&sBuffer, 24, &bSuccess);
        if( !bSuccess )
        {
            CPLFree(sBuffer.buffer);
            return CE_Failure;
        }

        if( STARTS_WITH_CI(poGDS->pszProjection, "GEOGCS") )
            dyStart = dyStart / 3600.0;

        double dygap = (dfYMin - dyStart)/poGDS->adfGeoTransform[5]+ 0.5;
        if( dygap <= INT_MIN || dygap >= INT_MAX || !CPLIsFinite(dygap) )
        {
            CPLFree(sBuffer.buffer);
            return CE_Failure;
        }
        int lygap = static_cast<int>(dygap);
        if( nCPoints <= 0 )
            continue;
        if( lygap > INT_MAX - nCPoints )
            lygap = INT_MAX - nCPoints;

        for (int j=lygap; j < (nCPoints + lygap); j++)
        {
            const int iY = GetYSize() - j - 1;

            const int nElev = USGSDEMReadIntFromBuffer(&sBuffer, &bSuccess);
            if( !bSuccess )
            {
                CPLFree(sBuffer.buffer);
                return CE_Failure;
            }

            if (iY < 0 || iY >= GetYSize() ) {
                /* bad = TRUE; */
            } else if( nElev == USGSDEM_NODATA )
                /* leave in output buffer as nodata */;
            else
            {
                const float fComputedElev =
                    static_cast<float>(nElev * poGDS->fVRes + dfElevOffset);

                if( GetRasterDataType() == GDT_Int16 )
                {
                    GUInt16 nVal = ( fComputedElev < -32768 ) ? -32768 :
                                   ( fComputedElev > 32767 ) ? 32767 :
                                   static_cast<GInt16>( fComputedElev );
                    reinterpret_cast<GInt16 *>( pImage )[i + iY*GetXSize()]
                        = nVal;
                }
                else
                {
                    reinterpret_cast<float *>( pImage )[i + iY*GetXSize()]
                        = fComputedElev;
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
    USGSDEMDataset *poGDS = reinterpret_cast<USGSDEMDataset *>( poDS );

    return poGDS->pszUnits;
}

/************************************************************************/
/* ==================================================================== */
/*                              USGSDEMDataset                          */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           USGSDEMDataset()                           */
/************************************************************************/

USGSDEMDataset::USGSDEMDataset() :
    nDataStartOffset(0),
    eNaturalDataFormat(GDT_Unknown),
    pszProjection(NULL),
    fVRes(0.0),
    pszUnits(NULL),
    fp(NULL)
{
    memset( adfGeoTransform, 0, sizeof(adfGeoTransform) );
}

/************************************************************************/
/*                            ~USGSDEMDataset()                         */
/************************************************************************/

USGSDEMDataset::~USGSDEMDataset()

{
    FlushCache();

    CPLFree( pszProjection );
    if( fp != NULL )
        CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));
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
    // check for version of DEM format
    CPL_IGNORE_RET_VAL(VSIFSeekL(InDem, 864, 0));

    // Read DEM into matrix
    const int nRow = ReadInt(InDem);
    const int nColumn = ReadInt(InDem);
    const bool bNewFormat = nRow != 1 || nColumn != 1;
    if (bNewFormat)
    {
        CPL_IGNORE_RET_VAL(VSIFSeekL(InDem, 1024, 0));  // New Format
        int i = ReadInt(InDem);
        int j = ReadInt(InDem);
        if ( i != 1 || ( j != 1 && j != 0 ) )  // File OK?
        {
            CPL_IGNORE_RET_VAL(VSIFSeekL(InDem, 893, 0));  // Undocumented Format (39109h1.dem)
            i = ReadInt(InDem);
            j = ReadInt(InDem);
            if ( i != 1 || j != 1 )  // File OK?
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

    CPL_IGNORE_RET_VAL(VSIFSeekL(InDem, 156, 0));
    const int nCoordSystem = ReadInt(InDem);
    const int iUTMZone = ReadInt(InDem);

    CPL_IGNORE_RET_VAL(VSIFSeekL(InDem, 528, 0));
    const int nGUnit = ReadInt(InDem);
    const int nVUnit = ReadInt(InDem);

    // Vertical Units in meters
    if (nVUnit==1)
        pszUnits = "ft";
    else
        pszUnits = "m";

    CPL_IGNORE_RET_VAL(VSIFSeekL(InDem, 816, 0));
    const double dxdelta = DConvert(InDem, 12);
    const double dydelta = DConvert(InDem, 12);
    if( dydelta == 0 )
        return FALSE;
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
    CPL_IGNORE_RET_VAL(VSIFSeekL(InDem, 546, 0));
    DPoint2 corners[4];  // SW, NW, NE, SE
    for (int i = 0; i < 4; i++)
    {
        corners[i].x = DConvert(InDem, 24);
        corners[i].y = DConvert(InDem, 24);
    }

    // find absolute extents of raw vales
    DPoint2 extent_min, extent_max;
    extent_min.x = std::min(corners[0].x, corners[1].x);
    extent_max.x = std::max(corners[2].x, corners[3].x);
    extent_min.y = std::min(corners[0].y, corners[3].y);
    extent_max.y = std::max(corners[1].y, corners[2].y);

    /* dElevMin = */ DConvert(InDem, 48);
    /* dElevMax = */ DConvert(InDem, 48);

    CPL_IGNORE_RET_VAL(VSIFSeekL(InDem, 858, 0));
    const int nProfiles = ReadInt(InDem);

/* -------------------------------------------------------------------- */
/*      Collect the spatial reference system.                           */
/* -------------------------------------------------------------------- */
    OGRSpatialReference sr;
    bool bNAD83 = true;

    // OLD format header ends at byte 864
    if (bNewFormat)
    {
        // year of data compilation
        CPL_IGNORE_RET_VAL(VSIFSeekL(InDem, 876, 0));
        char szDateBuffer[5];
        CPL_IGNORE_RET_VAL(VSIFReadL(szDateBuffer, 4, 1, InDem));
        /* szDateBuffer[4] = 0; */

        // Horizontal datum
        // 1=North American Datum 1927 (NAD 27)
        // 2=World Geodetic System 1972 (WGS 72)
        // 3=WGS 84
        // 4=NAD 83
        // 5=Old Hawaii Datum
        // 6=Puerto Rico Datum
        CPL_IGNORE_RET_VAL(VSIFSeekL(InDem, 890, 0));

        char szHorzDatum[3];
        CPL_IGNORE_RET_VAL(VSIFReadL( szHorzDatum, 1, 2, InDem ));
        szHorzDatum[2] = '\0';
        const int datum = atoi(szHorzDatum);
        switch (datum)
        {
          case 1:
            sr.SetWellKnownGeogCS( "NAD27" );
            bNAD83 = false;
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
        bNAD83 = false;
    }

    if (nCoordSystem == 1)  // UTM
    {
        sr.SetUTM( iUTMZone, TRUE );
        if( nGUnit == 1 )
        {
            sr.SetLinearUnitsAndUpdateParameters( SRS_UL_US_FOOT, CPLAtof(SRS_UL_US_FOOT_CONV) );
            char    szUTMName[128];
            snprintf( szUTMName, sizeof(szUTMName), "UTM Zone %d, Northern Hemisphere, us-ft", iUTMZone );
            sr.SetNode( "PROJCS", szUTMName );
        }
    }

    else if (nCoordSystem == 2)  // state plane
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
        || nCoordSystem == 2       // State Plane
        || nCoordSystem == -9999 ) // unknown
    {
        // expand extents modulus the pixel size.
        extent_min.y = floor(extent_min.y/dydelta) * dydelta;
        extent_max.y = ceil(extent_max.y/dydelta) * dydelta;

        // Forcibly compute X extents based on first profile and pixelsize.
        CPL_IGNORE_RET_VAL(VSIFSeekL(InDem, nDataStartOffset, 0));
        /* njunk = */ ReadInt(InDem);
        /* njunk = */ ReadInt(InDem);
        /* njunk = */ ReadInt(InDem);
        /* njunk = */ ReadInt(InDem);
        const double dxStart = DConvert(InDem, 24);

        nRasterYSize = static_cast<int>(
            ( extent_max.y - extent_min.y ) / dydelta + 1.5 );
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
        nRasterYSize = static_cast<int>(
            ( extent_max.y - extent_min.y ) / dydelta + 1.5 );
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

    if( !STARTS_WITH_CI((const char *) poOpenInfo->pabyHeader+156, "     0")
        && !STARTS_WITH_CI((const char *) poOpenInfo->pabyHeader+156, "     1")
        && !STARTS_WITH_CI((const char *) poOpenInfo->pabyHeader+156, "     2")
        && !STARTS_WITH_CI((const char *) poOpenInfo->pabyHeader+156, "     3")
        && !STARTS_WITH_CI((const char *) poOpenInfo->pabyHeader+156, " -9999") )
        return FALSE;

    if( !STARTS_WITH_CI((const char *) poOpenInfo->pabyHeader+150, "     1")
        && !STARTS_WITH_CI((const char *) poOpenInfo->pabyHeader+150, "     4"))
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
    USGSDEMDataset *poDS = new USGSDEMDataset();

    poDS->fp = fp;

/* -------------------------------------------------------------------- */
/*      Read the file.                                                  */
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

    return poDS;
}

/************************************************************************/
/*                        GDALRegister_USGSDEM()                        */
/************************************************************************/

void GDALRegister_USGSDEM()

{
    if( GDALGetDriverByName( "USGSDEM" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "USGSDEM" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "dem" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "USGS Optional ASCII DEM (and CDED)" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_usgsdem.html" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Int16" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='PRODUCT' type='string-select' description='Specific Product Type'>"
"       <Value>DEFAULT</Value>"
"       <Value>CDED50K</Value>"
"   </Option>"
"   <Option name='TOPLEFT' type='string' description='Top left product corner (i.e. 117d15w,52d30n'/>"
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
