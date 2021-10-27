/******************************************************************************
 *
 * Project:  SDTS Translator
 * Purpose:  GDALDataset driver for SDTS Raster translator.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2011, Even Rouault <even dot rouault at spatialys.com>
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
#include "sdts_al.h"

CPL_CVSID("$Id$")

/**
 \file sdtsdataset.cpp

 exclude
*/

/************************************************************************/
/* ==================================================================== */
/*                              SDTSDataset                             */
/* ==================================================================== */
/************************************************************************/

class SDTSRasterBand;

class SDTSDataset final: public GDALPamDataset
{
    friend class SDTSRasterBand;

    SDTSTransfer *poTransfer;
    SDTSRasterReader *poRL;

    char        *pszProjection;

  public:
                 SDTSDataset();
    virtual     ~SDTSDataset();

    static GDALDataset *Open( GDALOpenInfo * );

    virtual const char *_GetProjectionRef(void) override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }
    virtual CPLErr GetGeoTransform( double * ) override;
};

class SDTSRasterBand final: public GDALPamRasterBand
{
    friend class SDTSDataset;

    SDTSRasterReader *poRL;

  public:

                SDTSRasterBand( SDTSDataset *, int, SDTSRasterReader * );

    virtual CPLErr IReadBlock( int, int, void * ) override;

    virtual double GetNoDataValue( int *pbSuccess ) override;
    virtual const char *GetUnitType() override;
};


/************************************************************************/
/*                             SDTSDataset()                            */
/************************************************************************/

SDTSDataset::SDTSDataset() :
    poTransfer( nullptr ),
    poRL( nullptr ),
    pszProjection( nullptr )

{
}

/************************************************************************/
/*                            ~SDTSDataset()                            */
/************************************************************************/

SDTSDataset::~SDTSDataset()

{
    FlushCache(true);

    if( poTransfer != nullptr )
        delete poTransfer;

    if( poRL != nullptr )
        delete poRL;

    CPLFree( pszProjection );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *SDTSDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Before trying SDTSOpen() we first verify that the first         */
/*      record is in fact a SDTS file descriptor record.                */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 24 )
        return nullptr;

    char *pachLeader = reinterpret_cast<char *>( poOpenInfo->pabyHeader );
    if( pachLeader[5] != '1' && pachLeader[5] != '2' && pachLeader[5] != '3' )
        return nullptr;

    if( pachLeader[6] != 'L' )
        return nullptr;

    if( pachLeader[8] != '1' && pachLeader[8] != ' ' )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    SDTSTransfer *poTransfer = new SDTSTransfer;

    if( !poTransfer->Open( poOpenInfo->pszFilename ) )
    {
        delete poTransfer;
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        delete poTransfer;
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The SDTS driver does not support update access to existing"
                  " datasets.\n" );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Find the first raster layer.  If there are none, abort          */
/*      returning an error.                                             */
/* -------------------------------------------------------------------- */
    SDTSRasterReader    *poRL = nullptr;

    for( int i = 0; i < poTransfer->GetLayerCount(); i++ )
    {
        if( poTransfer->GetLayerType( i ) == SLTRaster )
        {
            poRL = poTransfer->GetLayerRasterReader( i );
            break;
        }
    }

    if( poRL == nullptr )
    {
        delete poTransfer;

        CPLError( CE_Warning, CPLE_AppDefined,
                  "%s is an SDTS transfer, but has no raster cell layers.\n"
                  "Perhaps it is a vector transfer?\n",
                  poOpenInfo->pszFilename );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Initialize a corresponding GDALDataset.                         */
/* -------------------------------------------------------------------- */
    SDTSDataset *poDS = new SDTSDataset();

    poDS->poTransfer = poTransfer;
    poDS->poRL = poRL;

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = poRL->GetXSize();
    poDS->nRasterYSize = poRL->GetYSize();

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->nBands = 1;
    poDS->papoBands = reinterpret_cast<GDALRasterBand **>(
        VSICalloc( sizeof(GDALRasterBand *), poDS->nBands ) );

    for( int i = 0; i < poDS->nBands; i++ )
        poDS->SetBand( i+1, new SDTSRasterBand( poDS, i+1, poRL ) );

/* -------------------------------------------------------------------- */
/*      Try to establish the projection string.  For now we only        */
/*      support UTM and GEO.                                            */
/* -------------------------------------------------------------------- */
    OGRSpatialReference   oSRS;
    SDTS_XREF   *poXREF = poTransfer->GetXREF();

    if( EQUAL(poXREF->pszSystemName,"UTM") )
    {
        oSRS.SetUTM( poXREF->nZone );
    }
    else if( EQUAL(poXREF->pszSystemName,"GEO") )
    {
        /* we set datum later */
    }
    else
        oSRS.SetLocalCS( poXREF->pszSystemName );

    if( oSRS.IsLocal() )
        /* don't try to set datum. */;
    else if( EQUAL(poXREF->pszDatum,"NAS") )
        oSRS.SetWellKnownGeogCS( "NAD27" );
    else if( EQUAL(poXREF->pszDatum, "NAX") )
        oSRS.SetWellKnownGeogCS( "NAD83" );
    else if( EQUAL(poXREF->pszDatum, "WGC") )
        oSRS.SetWellKnownGeogCS( "WGS72" );
    else /* if( EQUAL(poXREF->pszDatum, "WGE") ) or default */
        oSRS.SetWellKnownGeogCS( "WGS84" );

    poDS->pszProjection = nullptr;
    if( oSRS.exportToWkt( &poDS->pszProjection ) != OGRERR_NONE )
        poDS->pszProjection = CPLStrdup("");

/* -------------------------------------------------------------------- */
/*      Get metadata from the IDEN file.                                */
/* -------------------------------------------------------------------- */
    const char* pszIDENFilePath = poTransfer->GetCATD()->GetModuleFilePath("IDEN");
    if (pszIDENFilePath)
    {
        DDFModule   oIDENFile;
        if( oIDENFile.Open( pszIDENFilePath ) )
        {
            DDFRecord* poRecord = nullptr;

            while( (poRecord = oIDENFile.ReadRecord()) != nullptr )
            {

                if( poRecord->GetStringSubfield( "IDEN", 0, "MODN", 0 ) == nullptr )
                    continue;

                static const char* const fields[][2] = { { "TITL", "TITLE" },
                                                   { "DAID", "DATASET_ID" },
                                                   { "DAST", "DATA_STRUCTURE" },
                                                   { "MPDT", "MAP_DATE" },
                                                   { "DCDT", "DATASET_CREATION_DATE" } };

                for ( int i = 0;
                      i < static_cast<int>( sizeof(fields) )
                          / static_cast<int>( sizeof(fields[0] ) );
                      i++)
                {
                    const char* pszFieldValue =
                            poRecord->GetStringSubfield( "IDEN", 0, fields[i][0], 0 );
                    if ( pszFieldValue )
                        poDS->SetMetadataItem(fields[i][1], pszFieldValue);
                }

                break;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for external overviews.                                   */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename, poOpenInfo->GetSiblingFiles() );

    return poDS;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr SDTSDataset::GetGeoTransform( double * padfTransform )

{
    if( poRL->GetTransform( padfTransform ) )
        return CE_None;

    return CE_Failure;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *SDTSDataset::_GetProjectionRef()

{
    return pszProjection;
}

/************************************************************************/
/* ==================================================================== */
/*                            SDTSRasterBand                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           SDTSRasterBand()                            */
/************************************************************************/

SDTSRasterBand::SDTSRasterBand( SDTSDataset *poDSIn, int nBandIn,
                                SDTSRasterReader * poRLIn ) :
    poRL(poRLIn)
{
    poDS = poDSIn;
    nBand = nBandIn;

    if( poRL->GetRasterType() == SDTS_RT_INT16 )
        eDataType = GDT_Int16;
    else
        eDataType = GDT_Float32;

    nBlockXSize = poRL->GetBlockXSize();
    nBlockYSize = poRL->GetBlockYSize();
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr SDTSRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    if( poRL->GetBlock( nBlockXOff, nBlockYOff, pImage ) )
        return CE_None;

    return CE_Failure;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double SDTSRasterBand::GetNoDataValue( int *pbSuccess )

{
    if( pbSuccess != nullptr )
        *pbSuccess = TRUE;

    return -32766.0;
}

/************************************************************************/
/*                            GetUnitType()                             */
/************************************************************************/

const char *SDTSRasterBand::GetUnitType()

{
    if( EQUAL(poRL->szUNITS,"FEET") )
        return "ft";
    else if( STARTS_WITH_CI(poRL->szUNITS, "MET") )
        return "m";

    return poRL->szUNITS;
}

/************************************************************************/
/*                         GDALRegister_SDTS()                          */
/************************************************************************/

void GDALRegister_SDTS()

{
    if( GDALGetDriverByName( "SDTS" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "SDTS" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "SDTS Raster" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/sdts.html" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "ddf" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = SDTSDataset::Open;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
