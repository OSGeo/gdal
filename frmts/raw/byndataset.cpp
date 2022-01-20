/******************************************************************************
 *
 * Project:  Natural Resources Canada's Geoid BYN file format
 * Purpose:  Implementation of BYN format
 * Author:   Ivan Lucena, ivan.lucena@outlook.com
 *
 ******************************************************************************
 * Copyright (c) 2018, Ivan Lucena
 * Copyright (c) 2018, Even Rouault
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
#include "byndataset.h"
#include "rawdataset.h"

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "ogr_spatialref.h"
#include "ogr_srs_api.h"

#include <cstdlib>

CPL_CVSID("$Id$")

// Specification at
// https://www.nrcan.gc.ca/sites/www.nrcan.gc.ca/files/earthsciences/pdf/gpshgrid_e.pdf

const static BYNEllipsoids EllipsoidTable[] = {
    { "GRS80",       6378137.0,  298.257222101 },
    { "WGS84",       6378137.0,  298.257223564 },
    { "ALT1",        6378136.3,  298.256415099 },
    { "GRS67",       6378160.0,  298.247167427 },
    { "ELLIP1",      6378136.46, 298.256415099 },
    { "ALT2",        6378136.3,  298.257 },
    { "ELLIP2",      6378136.0,  298.257 },
    { "CLARKE 1866", 6378206.4,  294.9786982 }
};

/************************************************************************/
/*                            BYNRasterBand()                           */
/************************************************************************/

BYNRasterBand::BYNRasterBand( GDALDataset *poDSIn, int nBandIn,
                                VSILFILE * fpRawIn, vsi_l_offset nImgOffsetIn,
                                int nPixelOffsetIn, int nLineOffsetIn,
                                GDALDataType eDataTypeIn, int bNativeOrderIn ) :
    RawRasterBand( poDSIn, nBandIn, fpRawIn,
                   nImgOffsetIn, nPixelOffsetIn, nLineOffsetIn,
                   eDataTypeIn, bNativeOrderIn, RawRasterBand::OwnFP::NO )
{
}

/************************************************************************/
/*                           ~BYNRasterBand()                           */
/************************************************************************/

BYNRasterBand::~BYNRasterBand()
{
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double BYNRasterBand::GetNoDataValue( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = TRUE;
    int bSuccess = FALSE;
    double dfNoData = GDALPamRasterBand::GetNoDataValue(&bSuccess);
    if( bSuccess )
    {
        return dfNoData;
    }
    const double dfFactor =
        reinterpret_cast<BYNDataset*>(poDS)->hHeader.dfFactor;
    return eDataType == GDT_Int16 ? 32767.0 : 9999.0 * dfFactor;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double BYNRasterBand::GetScale( int *pbSuccess )
{
    if( pbSuccess != nullptr )
        *pbSuccess = TRUE;
    const double dfFactor =
        reinterpret_cast<BYNDataset*>(poDS)->hHeader.dfFactor;
    return (dfFactor != 0.0) ? 1.0 / dfFactor : 0.0;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr BYNRasterBand::SetScale( double dfNewValue )
{
    BYNDataset *poIDS = reinterpret_cast<BYNDataset*>(poDS);
    poIDS->hHeader.dfFactor = 1.0 / dfNewValue;
    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                              BYNDataset                              */
/* ==================================================================== */
/************************************************************************/

BYNDataset::BYNDataset() :
        fpImage(nullptr),
        pszProjection(nullptr),
        hHeader{0,0,0,0,0,0,0,0,0.0,0,0,0,0,0,0,0,0,0.0,0.0,0,0,0.0,0}
{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~BYNDataset()                             */
/************************************************************************/

BYNDataset::~BYNDataset()

{
    FlushCache(true);

    if( GetAccess() == GA_Update)
        UpdateHeader();

    if( fpImage != nullptr )
    {
        if( VSIFCloseL( fpImage ) != 0 )
        {
            CPLError( CE_Failure, CPLE_FileIO, "I/O error" );
        }
    }

    CPLFree( pszProjection );
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int BYNDataset::Identify( GDALOpenInfo *poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes < BYN_HDR_SZ )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Check file extension (.byn/.err)                                */
/* -------------------------------------------------------------------- */
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    const char* pszFileExtension =
                             CPLGetExtension( poOpenInfo->pszFilename );

    if( ! EQUAL( pszFileExtension, "byn" ) &&
        ! EQUAL( pszFileExtension, "err" ) )
    {
        return FALSE;
    }
#endif

/* -------------------------------------------------------------------- */
/*      Check some value's ranges on header                             */
/* -------------------------------------------------------------------- */

    BYNHeader hHeader = {0,0,0,0,0,0,0,0,0.0,0,0,0,0,0,0,0,0,0.0,0.0,0,0,0.0,0};

    buffer2header( poOpenInfo->pabyHeader, &hHeader );

    if( hHeader.nGlobal    < 0 || hHeader.nGlobal    > 1 ||
        hHeader.nType      < 0 || hHeader.nType      > 9 ||
      ( hHeader.nSizeOf   != 2 && hHeader.nSizeOf   != 4 ) ||
        hHeader.nVDatum    < 0 || hHeader.nVDatum    > 3 ||
        hHeader.nDescrip   < 0 || hHeader.nDescrip   > 3 ||
        hHeader.nSubType   < 0 || hHeader.nSubType   > 9 ||
        hHeader.nDatum     < 0 || hHeader.nDatum     > 1 ||
        hHeader.nEllipsoid < 0 || hHeader.nEllipsoid > 7 ||
        hHeader.nByteOrder < 0 || hHeader.nByteOrder > 1 ||
        hHeader.nScale     < 0 || hHeader.nScale     > 1 )
        return FALSE;

#if 0
    // We have disabled those checks as invalid values are often found in some
    // datasets, such as http://s3.microsurvey.com/os/fieldgenius/geoids/Lithuania.zip
    // We don't use those fields, so we may just ignore them.
    if((hHeader.nTideSys   < 0 || hHeader.nTideSys   > 2 ||
        hHeader.nPtType    < 0 || hHeader.nPtType    > 1 ))
    {
        // Some datasets use 0xCC as a marker for invalidity for
        // records starting from Geopotential Wo
        for( int i = 52; i < 78; i++ )
        {
            if( poOpenInfo->pabyHeader[i] != 0xCC )
                return FALSE;
        }
    }
#endif

    if( hHeader.nScale == 0 )
    {
        if( ( std::abs( static_cast<GIntBig>( hHeader.nSouth ) -
                        ( hHeader.nDLat / 2 ) ) > BYN_MAX_LAT ) ||
            ( std::abs( static_cast<GIntBig>( hHeader.nNorth ) +
                        ( hHeader.nDLat / 2 ) ) > BYN_MAX_LAT ) ||
            ( std::abs( static_cast<GIntBig>( hHeader.nWest ) -
                        ( hHeader.nDLon / 2 ) ) > BYN_MAX_LON ) ||
            ( std::abs( static_cast<GIntBig>( hHeader.nEast ) +
                        ( hHeader.nDLon / 2 ) ) > BYN_MAX_LON ) )
            return FALSE;
    }
    else
    {
        if( ( std::abs( static_cast<GIntBig>( hHeader.nSouth ) -
                        ( hHeader.nDLat / 2 ) ) > BYN_MAX_LAT_SCL ) ||
            ( std::abs( static_cast<GIntBig>( hHeader.nNorth ) +
                        ( hHeader.nDLat / 2 ) ) > BYN_MAX_LAT_SCL ) ||
            ( std::abs( static_cast<GIntBig>( hHeader.nWest ) -
                        ( hHeader.nDLon / 2 ) ) > BYN_MAX_LON_SCL ) ||
            ( std::abs( static_cast<GIntBig>( hHeader.nEast ) +
                        ( hHeader.nDLon / 2 ) ) > BYN_MAX_LON_SCL ) )
            return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *BYNDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( !Identify( poOpenInfo ) || poOpenInfo->fpL == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */

    BYNDataset *poDS = new BYNDataset();

    poDS->eAccess = poOpenInfo->eAccess;
    poDS->fpImage = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;

/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */

    buffer2header( poOpenInfo->pabyHeader, &poDS->hHeader );

    /********************************/
    /* Scale boundaries and spacing */
    /********************************/

    double dfSouth = poDS->hHeader.nSouth;
    double dfNorth = poDS->hHeader.nNorth;
    double dfWest  = poDS->hHeader.nWest;
    double dfEast  = poDS->hHeader.nEast;
    double dfDLat  = poDS->hHeader.nDLat;
    double dfDLon  = poDS->hHeader.nDLon;

    if( poDS->hHeader.nScale == 1 )
    {
        dfSouth *= BYN_SCALE;
        dfNorth *= BYN_SCALE;
        dfWest  *= BYN_SCALE;
        dfEast  *= BYN_SCALE;
        dfDLat  *= BYN_SCALE;
        dfDLon  *= BYN_SCALE;
    }

    /******************************/
    /* Calculate rows and columns */
    /******************************/

    double dfXSize = -1;
    double dfYSize = -1;

    poDS->nRasterXSize = -1;
    poDS->nRasterYSize = -1;

    if( dfDLat != 0.0 && dfDLon != 0.0 )
    {
        dfXSize = ( ( dfEast  - dfWest  + 1.0 ) / dfDLon ) + 1.0;
        dfYSize = ( ( dfNorth - dfSouth + 1.0 ) / dfDLat ) + 1.0;
    }

    if( dfXSize > 0.0 && dfXSize < std::numeric_limits<double>::max() &&
        dfYSize > 0.0 && dfYSize < std::numeric_limits<double>::max() )
    {
        poDS->nRasterXSize = static_cast<GInt32>(dfXSize);
        poDS->nRasterYSize = static_cast<GInt32>(dfYSize);
    }

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        delete poDS;
        return nullptr;
    }

    /*****************************/
    /* Build GeoTransform matrix */
    /*****************************/

    poDS->adfGeoTransform[0] = ( dfWest - ( dfDLon / 2.0 ) ) / 3600.0;
    poDS->adfGeoTransform[1] = dfDLon / 3600.0;
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[3] = ( dfNorth + ( dfDLat / 2.0 ) ) / 3600.0;
    poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[5] = -1 * dfDLat / 3600.0;

    /*********************/
    /* Set data type     */
    /*********************/

    GDALDataType eDT = GDT_Unknown;

    if ( poDS->hHeader.nSizeOf == 2 )
       eDT = GDT_Int16;
    else if ( poDS->hHeader.nSizeOf == 4 )
       eDT = GDT_Int32;
    else
    {
        delete poDS;
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create band information object.                                 */
/* -------------------------------------------------------------------- */

    const int nDTSize = GDALGetDataTypeSizeBytes( eDT );

    int bIsLSB = poDS->hHeader.nByteOrder == 1 ? 1 : 0;

    BYNRasterBand *poBand = new BYNRasterBand(
        poDS, 1, poDS->fpImage, BYN_HDR_SZ,
        nDTSize, poDS->nRasterXSize * nDTSize, eDT,
        CPL_IS_LSB == bIsLSB );

    poDS->SetBand( 1, poBand );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */

    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */

    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return poDS;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr BYNDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr BYNDataset::SetGeoTransform( double * padfTransform )

{
    if( padfTransform[2] != 0.0 || padfTransform[4] != 0.0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to write skewed or rotated geotransform to byn." );
        return CE_Failure;
    }
    memcpy( adfGeoTransform, padfTransform, sizeof(double) * 6 );
    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *BYNDataset::_GetProjectionRef()

{
    if( pszProjection )
        return pszProjection;

    OGRSpatialReference oSRS;

    /* Try to use a prefefined EPSG compound CS */

    if( hHeader.nDatum == 1 && hHeader.nVDatum == 2 )
    {
        oSRS.importFromEPSG( BYN_DATUM_1_VDATUM_2 );
        oSRS.exportToWkt( &pszProjection );
        return pszProjection;
    }

    /* Build the GEOGCS based on Datum ( or Ellipsoid )*/

    bool bNoGeogCS = false;

    if( hHeader.nDatum == 0 )
        oSRS.importFromEPSG( BYN_DATUM_0 );
    else if( hHeader.nDatum == 1 )
        oSRS.importFromEPSG( BYN_DATUM_1 );
    else
    {
        /* Build GEOGCS based on Ellipsoid (Table 3) */

        if( hHeader.nEllipsoid > -1 &&
            hHeader.nEllipsoid < static_cast<GInt16>
                                 (CPL_ARRAYSIZE(EllipsoidTable)))
            oSRS.SetGeogCS(
                CPLSPrintf("BYN Ellipsoid(%d)", hHeader.nEllipsoid),
                "Unspecified",
                EllipsoidTable[ hHeader.nEllipsoid ].pszName,
                EllipsoidTable[ hHeader.nEllipsoid ].dfSemiMajor,
                EllipsoidTable[ hHeader.nEllipsoid ].dfInvFlattening );
        else
           bNoGeogCS = true;
    }

    /* Build the VERT_CS based on VDatum */

    OGRSpatialReference oSRSComp;
    OGRSpatialReference oSRSVert;

    int nVertCS = 0;

    if( hHeader.nVDatum == 1 )
        nVertCS = BYN_VDATUM_1;
    else if ( hHeader.nVDatum == 2 )
        nVertCS = BYN_VDATUM_2;
    else if ( hHeader.nVDatum == 3 )
        nVertCS = BYN_VDATUM_3;
    else
    {
        /* Return GEOGCS ( .err files ) */

        if( bNoGeogCS )
            return nullptr;

        oSRS.exportToWkt( &pszProjection );
        return pszProjection;
    }

    oSRSVert.importFromEPSG( nVertCS );

    /* Create CPMPD_CS with GEOGCS and VERT_CS */

    if( oSRSComp.SetCompoundCS(
            CPLSPrintf("BYN Datum(%d) & VDatum(%d)",
            hHeader.nDatum, hHeader.nDatum),
            &oSRS,
            &oSRSVert ) == CE_None )
    {
        /* Return COMPD_CS with GEOGCS and VERT_CS */

        oSRSComp.exportToWkt( &pszProjection );
        return pszProjection;
    }

    return "";
}

/************************************************************************/
/*                          SetProjectionRef()                          */
/************************************************************************/

CPLErr BYNDataset::_SetProjection( const char* pszProjString )

{
    OGRSpatialReference oSRS;

    OGRErr eOGRErr = oSRS.importFromWkt( pszProjString );

    if( eOGRErr != OGRERR_NONE )
    {
        return CE_Failure;
    }

    /* Try to recognize prefefined EPSG compound CS */

    if( oSRS.IsCompound() )
    {
        const char* pszAuthName = oSRS.GetAuthorityName( "COMPD_CS" );
        const char* pszAuthCode = oSRS.GetAuthorityCode( "COMPD_CS" );

        if( pszAuthName != nullptr &&
            pszAuthCode != nullptr &&
            EQUAL( pszAuthName, "EPSG" ) &&
            atoi( pszAuthCode ) == BYN_DATUM_1_VDATUM_2 )
        {
            hHeader.nVDatum    = 2;
            hHeader.nDatum     = 1;
            return CE_None;
        }
    }

    OGRSpatialReference oSRSTemp;

    /* Try to match GEOGCS */

    if( oSRS.IsGeographic() )
    {
        oSRSTemp.importFromEPSG( BYN_DATUM_0 );
        if( oSRS.IsSameGeogCS( &oSRSTemp ) )
            hHeader.nDatum = 0;
        else
        {
            oSRSTemp.importFromEPSG( BYN_DATUM_1 );
            if( oSRS.IsSameGeogCS( &oSRSTemp ) )
                hHeader.nDatum = 1;
        }
    }

    /* Try to match VERT_CS */

    if( oSRS.IsVertical() )
    {
        oSRSTemp.importFromEPSG( BYN_VDATUM_1 );
        if( oSRS.IsSameVertCS( &oSRSTemp ) )
            hHeader.nVDatum = 1;
        else
        {
            oSRSTemp.importFromEPSG( BYN_VDATUM_2 );
            if( oSRS.IsSameVertCS( &oSRSTemp ) )
                hHeader.nVDatum = 2;
            else
            {
                oSRSTemp.importFromEPSG( BYN_VDATUM_3 );
                if( oSRS.IsSameVertCS( &oSRSTemp ) )
                    hHeader.nVDatum = 3;
            }
        }
    }

    return CE_None;
}

/*----------------------------------------------------------------------*/
/*                           buffer2header()                            */
/*----------------------------------------------------------------------*/

void BYNDataset::buffer2header( const GByte* pabyBuf, BYNHeader* pohHeader )

{
    memcpy( &pohHeader->nSouth,     pabyBuf,      4 );
    memcpy( &pohHeader->nNorth,     pabyBuf + 4,  4 );
    memcpy( &pohHeader->nWest,      pabyBuf + 8,  4 );
    memcpy( &pohHeader->nEast,      pabyBuf + 12, 4 );
    memcpy( &pohHeader->nDLat,      pabyBuf + 16, 2 );
    memcpy( &pohHeader->nDLon,      pabyBuf + 18, 2 );
    memcpy( &pohHeader->nGlobal,    pabyBuf + 20, 2 );
    memcpy( &pohHeader->nType,      pabyBuf + 22, 2 );
    memcpy( &pohHeader->dfFactor,   pabyBuf + 24, 8 );
    memcpy( &pohHeader->nSizeOf,    pabyBuf + 32, 2 );
    memcpy( &pohHeader->nVDatum,    pabyBuf + 34, 2 );
    memcpy( &pohHeader->nDescrip,   pabyBuf + 40, 2 );
    memcpy( &pohHeader->nSubType,   pabyBuf + 42, 2 );
    memcpy( &pohHeader->nDatum,     pabyBuf + 44, 2 );
    memcpy( &pohHeader->nEllipsoid, pabyBuf + 46, 2 );
    memcpy( &pohHeader->nByteOrder, pabyBuf + 48, 2 );
    memcpy( &pohHeader->nScale,     pabyBuf + 50, 2 );
    memcpy( &pohHeader->dfWo,       pabyBuf + 52, 8 );
    memcpy( &pohHeader->dfGM,       pabyBuf + 60, 8 );
    memcpy( &pohHeader->nTideSys,   pabyBuf + 68, 2 );
    memcpy( &pohHeader->nRealiz,    pabyBuf + 70, 2 );
    memcpy( &pohHeader->dEpoch,     pabyBuf + 72, 4 );
    memcpy( &pohHeader->nPtType,    pabyBuf + 76, 2 );

#if defined(CPL_MSB)
    CPL_LSBPTR32( &pohHeader->nSouth );
    CPL_LSBPTR32( &pohHeader->nNorth );
    CPL_LSBPTR32( &pohHeader->nWest );
    CPL_LSBPTR32( &pohHeader->nEast );
    CPL_LSBPTR16( &pohHeader->nDLat );
    CPL_LSBPTR16( &pohHeader->nDLon );
    CPL_LSBPTR16( &pohHeader->nGlobal );
    CPL_LSBPTR16( &pohHeader->nType );
    CPL_LSBPTR64( &pohHeader->dfFactor );
    CPL_LSBPTR16( &pohHeader->nSizeOf );
    CPL_LSBPTR16( &pohHeader->nVDatum );
    CPL_LSBPTR16( &pohHeader->nDescrip );
    CPL_LSBPTR16( &pohHeader->nSubType );
    CPL_LSBPTR16( &pohHeader->nDatum );
    CPL_LSBPTR16( &pohHeader->nEllipsoid );
    CPL_LSBPTR16( &pohHeader->nByteOrder );
    CPL_LSBPTR16( &pohHeader->nScale );
    CPL_LSBPTR64( &pohHeader->dfWo );
    CPL_LSBPTR64( &pohHeader->dfGM );
    CPL_LSBPTR16( &pohHeader->nTideSys );
    CPL_LSBPTR16( &pohHeader->nRealiz );
    CPL_LSBPTR32( &pohHeader->dEpoch );
    CPL_LSBPTR16( &pohHeader->nPtType );
#endif

#if DEBUG
    CPLDebug("BYN","South         = %d",pohHeader->nSouth);
    CPLDebug("BYN","North         = %d",pohHeader->nNorth);
    CPLDebug("BYN","West          = %d",pohHeader->nWest);
    CPLDebug("BYN","East          = %d",pohHeader->nEast);
    CPLDebug("BYN","DLat          = %d",pohHeader->nDLat);
    CPLDebug("BYN","DLon          = %d",pohHeader->nDLon);
    CPLDebug("BYN","DGlobal       = %d",pohHeader->nGlobal);
    CPLDebug("BYN","DType         = %d",pohHeader->nType);
    CPLDebug("BYN","Factor        = %f",pohHeader->dfFactor);
    CPLDebug("BYN","SizeOf        = %d",pohHeader->nSizeOf);
    CPLDebug("BYN","VDatum        = %d",pohHeader->nVDatum);
    CPLDebug("BYN","Data          = %d",pohHeader->nDescrip);
    CPLDebug("BYN","SubType       = %d",pohHeader->nSubType);
    CPLDebug("BYN","Datum         = %d",pohHeader->nDatum);
    CPLDebug("BYN","Ellipsoid     = %d",pohHeader->nEllipsoid);
    CPLDebug("BYN","ByteOrder     = %d",pohHeader->nByteOrder);
    CPLDebug("BYN","Scale         = %d",pohHeader->nScale);
    CPLDebug("BYN","Wo            = %f",pohHeader->dfWo);
    CPLDebug("BYN","GM            = %f",pohHeader->dfGM);
    CPLDebug("BYN","TideSystem    = %d",pohHeader->nTideSys);
    CPLDebug("BYN","RefRealzation = %d",pohHeader->nRealiz);
    CPLDebug("BYN","Epoch         = %f",pohHeader->dEpoch);
    CPLDebug("BYN","PtType        = %d",pohHeader->nPtType);
#endif
}

/*----------------------------------------------------------------------*/
/*                           header2buffer()                            */
/*----------------------------------------------------------------------*/

void BYNDataset::header2buffer( const BYNHeader* pohHeader, GByte* pabyBuf )

{
    memcpy( pabyBuf,      &pohHeader->nSouth,     4 );
    memcpy( pabyBuf + 4,  &pohHeader->nNorth,     4 );
    memcpy( pabyBuf + 8,  &pohHeader->nWest,      4 );
    memcpy( pabyBuf + 12, &pohHeader->nEast,      4 );
    memcpy( pabyBuf + 16, &pohHeader->nDLat,      2 );
    memcpy( pabyBuf + 18, &pohHeader->nDLon,      2 );
    memcpy( pabyBuf + 20, &pohHeader->nGlobal,    2 );
    memcpy( pabyBuf + 22, &pohHeader->nType,      2 );
    memcpy( pabyBuf + 24, &pohHeader->dfFactor,   8 );
    memcpy( pabyBuf + 32, &pohHeader->nSizeOf,    2 );
    memcpy( pabyBuf + 34, &pohHeader->nVDatum,    2 );
    memcpy( pabyBuf + 40, &pohHeader->nDescrip,   2 );
    memcpy( pabyBuf + 42, &pohHeader->nSubType,   2 );
    memcpy( pabyBuf + 44, &pohHeader->nDatum,     2 );
    memcpy( pabyBuf + 46, &pohHeader->nEllipsoid, 2 );
    memcpy( pabyBuf + 48, &pohHeader->nByteOrder, 2 );
    memcpy( pabyBuf + 50, &pohHeader->nScale,     2 );
    memcpy( pabyBuf + 52, &pohHeader->dfWo,       8 );
    memcpy( pabyBuf + 60, &pohHeader->dfGM,       8 );
    memcpy( pabyBuf + 68, &pohHeader->nTideSys,   2 );
    memcpy( pabyBuf + 70, &pohHeader->nRealiz,    2 );
    memcpy( pabyBuf + 72, &pohHeader->dEpoch,     4 );
    memcpy( pabyBuf + 76, &pohHeader->nPtType,    2 );

#if defined(CPL_MSB)
    CPL_LSBPTR32( pabyBuf );
    CPL_LSBPTR32( pabyBuf + 4 );
    CPL_LSBPTR32( pabyBuf + 8 );
    CPL_LSBPTR32( pabyBuf + 12 );
    CPL_LSBPTR16( pabyBuf + 16 );
    CPL_LSBPTR16( pabyBuf + 18 );
    CPL_LSBPTR16( pabyBuf + 20 );
    CPL_LSBPTR16( pabyBuf + 22 );
    CPL_LSBPTR64( pabyBuf + 24 );
    CPL_LSBPTR16( pabyBuf + 32 );
    CPL_LSBPTR16( pabyBuf + 34 );
    CPL_LSBPTR16( pabyBuf + 40 );
    CPL_LSBPTR16( pabyBuf + 42 );
    CPL_LSBPTR16( pabyBuf + 44 );
    CPL_LSBPTR16( pabyBuf + 46 );
    CPL_LSBPTR16( pabyBuf + 48 );
    CPL_LSBPTR16( pabyBuf + 50 );
    CPL_LSBPTR64( pabyBuf + 52 );
    CPL_LSBPTR64( pabyBuf + 60 );
    CPL_LSBPTR16( pabyBuf + 68 );
    CPL_LSBPTR16( pabyBuf + 70 );
    CPL_LSBPTR32( pabyBuf + 72 );
    CPL_LSBPTR16( pabyBuf + 76 );
#endif
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *BYNDataset::Create( const char * pszFilename,
                                 int nXSize,
                                 int nYSize,
                                 int /* nBands */,
                                 GDALDataType eType,
                                 char ** /* papszOptions */ )
{
    if( eType != GDT_Int16 &&
        eType != GDT_Int32 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Attempt to create byn file with unsupported data type '%s'.",
            GDALGetDataTypeName( eType ) );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Check file extension (.byn/.err)                                */
/* -------------------------------------------------------------------- */

    char* pszFileExtension = CPLStrdup( CPLGetExtension( pszFilename ) );

    if( ! EQUAL( pszFileExtension, "byn" ) &&
        ! EQUAL( pszFileExtension, "err" ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Attempt to create byn file with extension other than byn/err." );
        CPLFree( pszFileExtension );
        return nullptr;
    }

    CPLFree( pszFileExtension );

/* -------------------------------------------------------------------- */
/*      Try to create the file.                                         */
/* -------------------------------------------------------------------- */

    VSILFILE *fp = VSIFOpenL( pszFilename, "wb+" );
    if( fp == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file `%s' failed.\n",
                  pszFilename );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Write an empty header.                                          */
/* -------------------------------------------------------------------- */

    GByte abyBuf[BYN_HDR_SZ] = { '\0' };

    /* Load header with some commum values */

    BYNHeader hHeader = {0,0,0,0,0,0,0,0,0.0,0,0,0,0,0,0,0,0,0.0,0.0,0,0,0.0,0};

    /* Set temporary values on header */

    hHeader.nSouth  = 0;
    hHeader.nNorth  = nYSize - 2;
    hHeader.nWest   = 0;
    hHeader.nEast   = nXSize - 2;
    hHeader.nDLat   = 1;
    hHeader.nDLon   = 1;
    hHeader.nSizeOf = static_cast<GInt16>(GDALGetDataTypeSizeBytes( eType ));

    /* Prepare buffer for writing */

    header2buffer( &hHeader, abyBuf );

    /* Write initial header */

    VSIFWriteL( abyBuf, BYN_HDR_SZ, 1, fp );
    VSIFCloseL( fp );

    return reinterpret_cast<GDALDataset*> (
        GDALOpen( pszFilename, GA_Update ) );
}

/************************************************************************/
/*                          UpdateHeader()                              */
/************************************************************************/

void BYNDataset::UpdateHeader()
{
    double dfDLon  = ( adfGeoTransform[1] * 3600.0 );
    double dfDLat  = ( adfGeoTransform[5] * 3600.0 * -1 );
    double dfWest  = ( adfGeoTransform[0] * 3600.0 ) + ( dfDLon / 2 );
    double dfNorth = ( adfGeoTransform[3] * 3600.0 ) - ( dfDLat / 2 );
    double dfSouth = dfNorth - ( ( nRasterYSize - 1 ) * dfDLat );
    double dfEast  = dfWest  + ( ( nRasterXSize - 1 ) * dfDLon );

    if( hHeader.nScale == 1 )
    {
        dfSouth /= BYN_SCALE;
        dfNorth /= BYN_SCALE;
        dfWest  /= BYN_SCALE;
        dfEast  /= BYN_SCALE;
        dfDLat  /= BYN_SCALE;
        dfDLon  /= BYN_SCALE;
    }

    hHeader.nSouth = static_cast<GInt32>(dfSouth);
    hHeader.nNorth = static_cast<GInt32>(dfNorth);
    hHeader.nWest  = static_cast<GInt32>(dfWest);
    hHeader.nEast  = static_cast<GInt32>(dfEast);
    hHeader.nDLat  = static_cast<GInt16>(dfDLat);
    hHeader.nDLon  = static_cast<GInt16>(dfDLon);

    GByte abyBuf[BYN_HDR_SZ];

    header2buffer( &hHeader, abyBuf );

    const char* pszValue = GetMetadataItem("GLOBAL");
    if(pszValue != nullptr)
        hHeader.nGlobal  = static_cast<GInt16>( atoi( pszValue ) );

    pszValue = GetMetadataItem("TYPE");
    if(pszValue != nullptr)
        hHeader.nType    = static_cast<GInt16>( atoi( pszValue ) );

    pszValue = GetMetadataItem("DESCRIPTION");
    if(pszValue != nullptr)
        hHeader.nDescrip = static_cast<GInt16>( atoi( pszValue ) );

    pszValue = GetMetadataItem("SUBTYPE");
    if(pszValue != nullptr)
        hHeader.nSubType = static_cast<GInt16>( atoi( pszValue ) );

    pszValue = GetMetadataItem("WO");
    if(pszValue != nullptr)
        hHeader.dfWo     = CPLAtof( pszValue );

    pszValue = GetMetadataItem("GM");
    if(pszValue != nullptr)
        hHeader.dfGM     = CPLAtof( pszValue );

    pszValue = GetMetadataItem("TIDESYSTEM");
    if(pszValue != nullptr)
        hHeader.nTideSys = static_cast<GInt16>( atoi( pszValue ) );

    pszValue = GetMetadataItem("REALIZATION");
    if(pszValue != nullptr)
        hHeader.nRealiz  = static_cast<GInt16>( atoi( pszValue ) );

    pszValue = GetMetadataItem("EPOCH");
    if(pszValue != nullptr)
        hHeader.dEpoch   = static_cast<float>( CPLAtof( pszValue ) );

    pszValue = GetMetadataItem("PTTYPE");
    if(pszValue != nullptr)
        hHeader.nPtType  = static_cast<GInt16>( atoi( pszValue ) );

    CPL_IGNORE_RET_VAL(VSIFSeekL( fpImage, 0, SEEK_SET ));
    CPL_IGNORE_RET_VAL(VSIFWriteL( abyBuf, BYN_HDR_SZ, 1, fpImage ));

    /* GDALPam metadata update */

    SetMetadataItem("GLOBAL",     CPLSPrintf("%d",hHeader.nGlobal), "BYN");
    SetMetadataItem("TYPE",       CPLSPrintf("%d",hHeader.nType),   "BYN");
    SetMetadataItem("DESCRIPTION",CPLSPrintf("%d",hHeader.nDescrip),"BYN");
    SetMetadataItem("SUBTYPE",    CPLSPrintf("%d",hHeader.nSubType),"BYN");
    SetMetadataItem("WO",         CPLSPrintf("%g",hHeader.dfWo),    "BYN");
    SetMetadataItem("GM",         CPLSPrintf("%g",hHeader.dfGM),    "BYN");
    SetMetadataItem("TIDESYSTEM", CPLSPrintf("%d",hHeader.nTideSys),"BYN");
    SetMetadataItem("REALIZATION",CPLSPrintf("%d",hHeader.nRealiz), "BYN");
    SetMetadataItem("EPOCH",      CPLSPrintf("%g",hHeader.dEpoch),  "BYN");
    SetMetadataItem("PTTYPE",     CPLSPrintf("%d",hHeader.nPtType), "BYN");
}

/************************************************************************/
/*                          GDALRegister_BYN()                          */
/************************************************************************/

void GDALRegister_BYN()

{
    if( GDALGetDriverByName( "BYN" ) != nullptr )
      return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "BYN" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Natural Resources Canada's Geoid" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "byn err" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/byn.html" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Int16 Int32" );

    poDriver->pfnOpen = BYNDataset::Open;
    poDriver->pfnIdentify = BYNDataset::Identify;
    poDriver->pfnCreate = BYNDataset::Create;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
