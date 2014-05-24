/******************************************************************************
 * $Id$
 *
 * Project:  SDTS Translator
 * Purpose:  GDALDataset driver for SDTS Raster translator.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "sdts_al.h"
#include "gdal_pam.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

/**
 \file sdtsdataset.cpp

 exclude
*/

CPL_C_START
void    GDALRegister_SDTS(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*                              SDTSDataset                             */
/* ==================================================================== */
/************************************************************************/

class SDTSRasterBand;

class SDTSDataset : public GDALPamDataset
{
    friend class SDTSRasterBand;
    
    SDTSTransfer *poTransfer;
    SDTSRasterReader *poRL;

    char        *pszProjection;

  public:
    virtual     ~SDTSDataset();
    
    static GDALDataset *Open( GDALOpenInfo * );

    virtual const char *GetProjectionRef(void);
    virtual CPLErr GetGeoTransform( double * );
};

class SDTSRasterBand : public GDALPamRasterBand
{
    friend class SDTSDataset;

    SDTSRasterReader *poRL;
    
  public:

                SDTSRasterBand( SDTSDataset *, int, SDTSRasterReader * );
    
    virtual CPLErr IReadBlock( int, int, void * );

    virtual double GetNoDataValue( int *pbSuccess );
    virtual const char *GetUnitType();
};


/************************************************************************/
/*                            ~SDTSDataset()                            */
/************************************************************************/

SDTSDataset::~SDTSDataset()

{
    FlushCache();

    if( poTransfer != NULL )
        delete poTransfer;

    if( poRL != NULL )
        delete poRL;

    if( pszProjection != NULL )
        CPLFree( pszProjection );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *SDTSDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int         i;
    
/* -------------------------------------------------------------------- */
/*      Before trying SDTSOpen() we first verify that the first         */
/*      record is in fact a SDTS file descriptor record.                */
/* -------------------------------------------------------------------- */
    char        *pachLeader = (char *) poOpenInfo->pabyHeader;
    
    if( poOpenInfo->nHeaderBytes < 24 )
        return NULL;

    if( pachLeader[5] != '1' && pachLeader[5] != '2' && pachLeader[5] != '3' )
        return NULL;

    if( pachLeader[6] != 'L' )
        return NULL;

    if( pachLeader[8] != '1' && pachLeader[8] != ' ' )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    SDTSTransfer        *poTransfer = new SDTSTransfer;
    
    if( !poTransfer->Open( poOpenInfo->pszFilename ) )
    {
        delete poTransfer;
        return NULL;
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
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Find the first raster layer.  If there are none, abort          */
/*      returning an error.                                             */
/* -------------------------------------------------------------------- */
    SDTSRasterReader    *poRL = NULL;

    for( i = 0; i < poTransfer->GetLayerCount(); i++ )
    {
        if( poTransfer->GetLayerType( i ) == SLTRaster )
        {
            poRL = poTransfer->GetLayerRasterReader( i );
            break;
        }
    }

    if( poRL == NULL )
    {
        delete poTransfer;
        
        CPLError( CE_Warning, CPLE_AppDefined,
                  "%s is an SDTS transfer, but has no raster cell layers.\n"
                  "Perhaps it is a vector transfer?\n",
                  poOpenInfo->pszFilename );
        return NULL;
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
    poDS->papoBands = (GDALRasterBand **)
        VSICalloc(sizeof(GDALRasterBand *),poDS->nBands);

    for( i = 0; i < poDS->nBands; i++ )
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
    else if( EQUAL(poXREF->pszDatum, "WGE") )
        oSRS.SetWellKnownGeogCS( "WGS84" );
    else
        oSRS.SetWellKnownGeogCS( "WGS84" );

    oSRS.Fixup();

    poDS->pszProjection = NULL;
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
            DDFRecord* poRecord;

            while( (poRecord = oIDENFile.ReadRecord()) != NULL )
            {

                if( poRecord->GetStringSubfield( "IDEN", 0, "MODN", 0 ) == NULL )
                    continue;

                static const char* fields[][2] = { { "TITL", "TITLE" },
                                                   { "DAID", "DATASET_ID" },
                                                   { "DAST", "DATA_STRUCTURE" },
                                                   { "MPDT", "MAP_DATE" },
                                                   { "DCDT", "DATASET_CREATION_DATE" } };

                for (i = 0; i < (int)sizeof(fields) / (int)sizeof(fields[0]) ; i++)
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

    return( poDS );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr SDTSDataset::GetGeoTransform( double * padfTransform )

{
    if( poRL->GetTransform( padfTransform ) )
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *SDTSDataset::GetProjectionRef()

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

SDTSRasterBand::SDTSRasterBand( SDTSDataset *poDS, int nBand,
                                SDTSRasterReader * poRL )

{
    this->poDS = poDS;
    this->nBand = nBand;
    this->poRL = poRL;

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
    else
        return CE_Failure;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double SDTSRasterBand::GetNoDataValue( int *pbSuccess )

{
    if( pbSuccess != NULL )
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
    else if( EQUALN(poRL->szUNITS,"MET",3) )
        return "m";
    else
        return poRL->szUNITS;
}

/************************************************************************/
/*                         GDALRegister_SDTS()                          */
/************************************************************************/

void GDALRegister_SDTS()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "SDTS" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "SDTS" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "SDTS Raster" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#SDTS" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "ddf" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = SDTSDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

