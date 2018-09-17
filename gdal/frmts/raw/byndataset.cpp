/******************************************************************************
 *
 * Project:  National Resources Canada - Vertical Datum Transformation
 * Purpose:  Implementation of NRCan's BYN vertical datum shift file format.
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

CPL_CVSID("$Id$")

const BYNEllipsoids EllipsoidTable[8] = {
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
    return eDataType == GDT_Int16 ? 32767.0 : 9999.0 * GetScale();
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double BYNRasterBand::GetScale( int *pbSuccess )
{
    if( pbSuccess != nullptr )
        *pbSuccess = TRUE;
    return 1.0 / reinterpret_cast<BYNDataset *>( poDS )->hHeader.dfFactor;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr BYNRasterBand::SetScale( double dfNewValue )
{
    BYNDataset *poIDS = reinterpret_cast<BYNDataset *>( poDS );

    dfNewValue = 1.0 / dfNewValue;

    if( dfNewValue == poIDS->hHeader.dfFactor )
        return CE_None;

    poIDS->hHeader.dfFactor = dfNewValue;
    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                              BYNDataset                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            ~BYNDataset()                             */
/************************************************************************/

BYNDataset::~BYNDataset()

{
    FlushCache();

    if( GetAccess() == GA_Update)
        UpdateHeader();

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

    if( fpImage != nullptr )
    {
        if( VSIFCloseL( fpImage ) != 0 )
        {
            CPLError( CE_Failure, CPLE_FileIO, "I/O error" );
        }
    }
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int BYNDataset::Identify( GDALOpenInfo *poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes < BYN_HDR_SZ )
        return FALSE;

    if( ! ( EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "byn") ||
            EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "err") ))
        return FALSE;

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

    memcpy( &poDS->hHeader.nSouth,     poOpenInfo->pabyHeader,      4 );
    memcpy( &poDS->hHeader.nNorth,     poOpenInfo->pabyHeader + 4,  4 );
    memcpy( &poDS->hHeader.nWest,      poOpenInfo->pabyHeader + 8,  4 );
    memcpy( &poDS->hHeader.nEast,      poOpenInfo->pabyHeader + 12, 4 );
    memcpy( &poDS->hHeader.nDLat,      poOpenInfo->pabyHeader + 16, 2 );
    memcpy( &poDS->hHeader.nDLon,      poOpenInfo->pabyHeader + 18, 2 );
    memcpy( &poDS->hHeader.nGlobal,    poOpenInfo->pabyHeader + 20, 2 );
    memcpy( &poDS->hHeader.nType,      poOpenInfo->pabyHeader + 22, 2 );
    memcpy( &poDS->hHeader.dfFactor,   poOpenInfo->pabyHeader + 24, 8 );
    memcpy( &poDS->hHeader.nSizeOf,    poOpenInfo->pabyHeader + 32, 2 );
    memcpy( &poDS->hHeader.nVDatum,    poOpenInfo->pabyHeader + 34, 2 );
    memcpy( &poDS->hHeader.nDescrip,   poOpenInfo->pabyHeader + 40, 2 );
    memcpy( &poDS->hHeader.nSubType,   poOpenInfo->pabyHeader + 42, 2 );
    memcpy( &poDS->hHeader.nDatum,     poOpenInfo->pabyHeader + 44, 2 );
    memcpy( &poDS->hHeader.nEllipsoid, poOpenInfo->pabyHeader + 46, 2 );
    memcpy( &poDS->hHeader.nByteOrder, poOpenInfo->pabyHeader + 48, 2 );
    memcpy( &poDS->hHeader.nScale,     poOpenInfo->pabyHeader + 50, 2 );
    memcpy( &poDS->hHeader.dfWo,       poOpenInfo->pabyHeader + 52, 8 );
    memcpy( &poDS->hHeader.dfGM,       poOpenInfo->pabyHeader + 60, 8 );
    memcpy( &poDS->hHeader.nTideSys,   poOpenInfo->pabyHeader + 68, 2 );
    memcpy( &poDS->hHeader.nRealiz,    poOpenInfo->pabyHeader + 70, 2 );
    memcpy( &poDS->hHeader.dEpoch,     poOpenInfo->pabyHeader + 72, 4 );
    memcpy( &poDS->hHeader.nPtType,    poOpenInfo->pabyHeader + 76, 2 );

    /********************************/
    /* Scale boundaries and spacing */
    /********************************/

    double dfSouth = (double) poDS->hHeader.nSouth;
    double dfNorth = (double) poDS->hHeader.nNorth;
    double dfWest  = (double) poDS->hHeader.nWest;
    double dfEast  = (double) poDS->hHeader.nEast;
    double dfDLat  = (double) poDS->hHeader.nDLat;
    double dfDLon  = (double) poDS->hHeader.nDLon;

    if( poDS->hHeader.nScale == 1 ) 
    {
        dfSouth /= 1000;
        dfNorth /= 1000;
        dfWest  /= 1000;
        dfEast  /= 1000;
        dfDLat  /= 1000;
        dfDLon  /= 1000;
    }

    /******************************/
    /* Calculate rows and columns */
    /******************************/

    poDS->nRasterYSize = (GInt32) ( ( ( dfNorth - dfSouth ) / dfDLat ) + 1.0 );
    poDS->nRasterXSize = (GInt32) ( ( ( dfEast  - dfWest  ) / dfDLon ) + 1.0 );

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        delete poDS;
        return nullptr;
    }

    /*****************************/
    /* Build GeoTransform matrix */
    /*****************************/

    poDS->adfGeoTransform[0] = ( dfWest - ( dfDLon / 2 ) ) / 3600.0;
    poDS->adfGeoTransform[1] = dfDLon / 3600.0;
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[3] = ( dfNorth + ( dfDLat / 2 ) ) / 3600.0;
    poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[5] = -1 * dfDLat / 3600.0;

    CPLDebug("BYN","South           = %d",poDS->hHeader.nSouth);
    CPLDebug("BYN","North           = %d",poDS->hHeader.nNorth);
    CPLDebug("BYN","West            = %d",poDS->hHeader.nWest);
    CPLDebug("BYN","East            = %d",poDS->hHeader.nEast);
    CPLDebug("BYN","DLat            = %d",poDS->hHeader.nDLat);
    CPLDebug("BYN","DLon            = %d",poDS->hHeader.nDLon);
    CPLDebug("BYN","DGlobal         = %d",poDS->hHeader.nGlobal);
    CPLDebug("BYN","DType           = %d",poDS->hHeader.nType);
    CPLDebug("BYN","Factor          = %f",poDS->hHeader.dfFactor);
    CPLDebug("BYN","SizeOf          = %d",poDS->hHeader.nSizeOf);
    CPLDebug("BYN","VDatum          = %d",poDS->hHeader.nVDatum);
    CPLDebug("BYN","Decription      = %d",poDS->hHeader.nDescrip);
    CPLDebug("BYN","SubType         = %d",poDS->hHeader.nSubType);
    CPLDebug("BYN","Datum           = %d",poDS->hHeader.nDatum);
    CPLDebug("BYN","Ellipsoid       = %d",poDS->hHeader.nEllipsoid);
    CPLDebug("BYN","ByteOrder       = %d",poDS->hHeader.nByteOrder);
    CPLDebug("BYN","Scale           = %d",poDS->hHeader.nScale);
    CPLDebug("BYN","Wo              = %f",poDS->hHeader.dfWo);
    CPLDebug("BYN","GM              = %f",poDS->hHeader.dfGM);
    CPLDebug("BYN","TideSystem      = %d",poDS->hHeader.nTideSys);
    CPLDebug("BYN","RefRealzation   = %d",poDS->hHeader.nRealiz);
    CPLDebug("BYN","Epoch           = %f",poDS->hHeader.dEpoch);
    CPLDebug("BYN","PtType          = %d",poDS->hHeader.nPtType);
    CPLDebug("BYN","RasterXSize     = %d ", poDS->nRasterXSize );
    CPLDebug("BYN","RasterYSize     = %d ", poDS->nRasterYSize );
    CPLDebug("BYN","GeoTransform[0] = %f ", poDS->adfGeoTransform[0] );
    CPLDebug("BYN","GeoTransform[1] = %f ", poDS->adfGeoTransform[1] );
    CPLDebug("BYN","GeoTransform[2] = %f ", poDS->adfGeoTransform[2] );
    CPLDebug("BYN","GeoTransform[3] = %f ", poDS->adfGeoTransform[3] );
    CPLDebug("BYN","GeoTransform[4] = %f ", poDS->adfGeoTransform[4] );
    CPLDebug("BYN","GeoTransform[5] = %f ", poDS->adfGeoTransform[5] );

    /*****************/
    /* Data type     */
    /*****************/
 
    GDALDataType eDT = GDT_Unknown;

    if ( poDS->hHeader.nSizeOf == 2 )
       eDT = GDT_Int16;
    else
        if ( poDS->hHeader.nSizeOf == 4 )
            eDT = GDT_Int32;
        else
        {
            delete poDS;
            return nullptr;
        };

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

const char *BYNDataset::GetProjectionRef()

{
    OGRSpatialReference oSRS;

    char* pszRefSystem = nullptr;

    /* Try to use a prefefined EPSG compound CS */

    if( hHeader.nDatum == 1 && hHeader.nVDatum == 2 ) 
    {
        oSRS.importFromEPSG( BYN_DATAM_1_VDATUM_2 );
        oSRS.exportToWkt( &pszRefSystem );
        return pszRefSystem;
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
            hHeader.nEllipsoid < 9 )
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

        oSRS.exportToWkt( &pszRefSystem );
        return pszRefSystem;
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

        oSRSComp.exportToWkt( &pszRefSystem );
        return pszRefSystem;
    }

    return nullptr;
}

/************************************************************************/
/*                          SetProjectionRef()                          */
/************************************************************************/

CPLErr BYNDataset::SetProjection( const char* pszProjString )

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
            atoi( pszAuthCode ) == BYN_DATAM_1_VDATUM_2 )
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
/*                           header2buffer()                            */
/*----------------------------------------------------------------------*/

void CPL_STDCALL header2buffer( const BYNHeader* poHeader, GByte* pabyBuf )

{
    memcpy( pabyBuf,      &poHeader->nSouth,     4 );
    memcpy( pabyBuf + 4,  &poHeader->nNorth,     4 );
    memcpy( pabyBuf + 8,  &poHeader->nWest,      4 );
    memcpy( pabyBuf + 12, &poHeader->nEast,      4 );
    memcpy( pabyBuf + 16, &poHeader->nDLat,      2 );
    memcpy( pabyBuf + 18, &poHeader->nDLon,      2 );
    memcpy( pabyBuf + 20, &poHeader->nGlobal,    2 );
    memcpy( pabyBuf + 22, &poHeader->nType,      2 );
    memcpy( pabyBuf + 24, &poHeader->dfFactor,   8 );
    memcpy( pabyBuf + 32, &poHeader->nSizeOf,    2 );
    memcpy( pabyBuf + 34, &poHeader->nVDatum,    2 );
    memcpy( pabyBuf + 40, &poHeader->nDescrip,   2 );
    memcpy( pabyBuf + 42, &poHeader->nSubType,   2 );
    memcpy( pabyBuf + 44, &poHeader->nDatum,     2 );
    memcpy( pabyBuf + 46, &poHeader->nEllipsoid, 2 );
    memcpy( pabyBuf + 48, &poHeader->nByteOrder, 2 );
    memcpy( pabyBuf + 50, &poHeader->nScale,     2 );
    memcpy( pabyBuf + 52, &poHeader->dfWo,       8 );
    memcpy( pabyBuf + 60, &poHeader->dfGM,       8 );
    memcpy( pabyBuf + 68, &poHeader->nTideSys,   2 );
    memcpy( pabyBuf + 70, &poHeader->nRealiz,    2 );
    memcpy( pabyBuf + 72, &poHeader->dEpoch,     4 );
    memcpy( pabyBuf + 76, &poHeader->nPtType,    2 );
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

    if( !EQUAL(CPLGetExtension(pszFilename),"byn") &&
        !EQUAL(CPLGetExtension(pszFilename),"err") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Attempt to create byn file with extension other than byn/err." );
        return nullptr;
    }

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

    BYNHeader hHeader = { 36060,   /* South  10.0 */
                          323940,  /* North  90.0 */
                          -611940, /* West -170.0 */
                          -36060,  /* East  -10.0 */
                          0, 0, 0, 1, 1.0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0,
                          0.0, 1, 0, 0.0, 0 }; /* LSB */

    /* Match the requested nXSize, nYSize, and type */

    hHeader.nDLat   = ( ( hHeader.nNorth - hHeader.nSouth ) / nYSize ) + 1;;
    hHeader.nDLon   = ( ( hHeader.nEast  - hHeader.nWest  ) / nXSize ) + 1;; 
    hHeader.nSizeOf = GDALGetDataTypeSizeBytes( eType ); /* {2,4} */

    /* Set up undefined Datum, VDatum and Ellipsoid */

    hHeader.nVDatum    = 0;
    hHeader.nDatum     = 9;
    hHeader.nEllipsoid = 9;

    /* Avoid platform misaligment when writing directly from struct 
       by writing from a buffer */

    header2buffer( &hHeader, abyBuf );

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
        dfSouth *= 1000;
        dfNorth *= 1000;
        dfWest  *= 1000;
        dfEast  *= 1000;
        dfDLat  *= 1000;
        dfDLon  *= 1000;
    }

    hHeader.nSouth = (GInt32) dfSouth;
    hHeader.nNorth = (GInt32) dfNorth;
    hHeader.nWest  = (GInt32) dfWest;
    hHeader.nEast  = (GInt32) dfEast;
    hHeader.nDLat  = (GInt32) dfDLat;
    hHeader.nDLon  = (GInt32) dfDLon;

    GByte abyBuf[BYN_HDR_SZ];

    header2buffer( &hHeader, abyBuf );

    const char* pszValue = nullptr;

    pszValue = GetMetadataItem("GLOBAL");
    if(pszValue != nullptr) hHeader.nGlobal  = atoi( pszValue );

    pszValue = GetMetadataItem("TYPE");
    if(pszValue != nullptr) hHeader.nType    = atoi( pszValue );

    pszValue = GetMetadataItem("DESCRIPTION");
    if(pszValue != nullptr) hHeader.nDescrip = atoi( pszValue );

    pszValue = GetMetadataItem("SUBTYPE");
    if(pszValue != nullptr) hHeader.nSubType = atoi( pszValue );

    pszValue = GetMetadataItem("WO");
    if(pszValue != nullptr) hHeader.dfWo     = CPLAtof( pszValue );

    pszValue = GetMetadataItem("GM");
    if(pszValue != nullptr) hHeader.dfGM     = CPLAtof( pszValue );

    pszValue = GetMetadataItem("TIDESYSTEM");
    if(pszValue != nullptr) hHeader.nTideSys = atoi( pszValue );

    pszValue = GetMetadataItem("REALIZATION");
    if(pszValue != nullptr) hHeader.nRealiz  = atoi( pszValue );

    pszValue = GetMetadataItem("EPOCH");
    if(pszValue != nullptr) hHeader.dEpoch   = CPLAtof( pszValue );

    pszValue = GetMetadataItem("PTTYPE");
    if(pszValue != nullptr) hHeader.nPtType  = atoi( pszValue );


    CPL_IGNORE_RET_VAL(VSIFSeekL( fpImage, 0, SEEK_SET ));
    CPL_IGNORE_RET_VAL(VSIFWriteL( abyBuf, BYN_HDR_SZ, 1, fpImage ));
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
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "NRCan's Vertical Datum .BYN" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "byn" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Int16, Int32" );

    poDriver->pfnOpen = BYNDataset::Open;
    poDriver->pfnIdentify = BYNDataset::Identify;
    poDriver->pfnCreate = BYNDataset::Create;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
