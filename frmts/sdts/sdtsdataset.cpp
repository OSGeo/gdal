/******************************************************************************
 * $Id$
 *
 * Project:  SDTS Translator
 * Purpose:  GDALDataset driver for SDTS Raster translator.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.18  2006/08/23 19:12:08  fwarmerdam
 * Don't require a valid file handle.
 *
 * Revision 1.17  2006/04/10 16:34:18  fwarmerdam
 * updated contact info
 *
 * Revision 1.16  2005/05/05 15:54:49  fwarmerdam
 * PAM Enabled
 *
 * Revision 1.15  2003/02/06 03:18:49  warmerda
 * use Fixup() on SRS to set linear units
 *
 * Revision 1.14  2002/09/04 06:50:37  warmerda
 * avoid static driver pointers
 *
 * Revision 1.13  2002/06/12 21:12:25  warmerda
 * update to metadata based driver info
 *
 * Revision 1.12  2002/04/12 20:20:29  warmerda
 * make vector sdts transfers just a warning
 *
 * Revision 1.11  2001/11/11 23:51:00  warmerda
 * added required class keyword to friend declarations
 *
 * Revision 1.10  2001/09/10 19:27:36  warmerda
 * added GetMinMax() and raster data types
 *
 * Revision 1.9  2001/07/18 04:51:57  warmerda
 * added CPL_CVSID
 *
 * Revision 1.8  2001/07/09 18:14:45  warmerda
 * upgraded projection support to use WKT
 *
 * Revision 1.7  2001/07/05 13:12:40  warmerda
 * added UnitType support
 *
 * Revision 1.6  2001/01/19 21:20:29  warmerda
 * expanded tabs
 *
 * Revision 1.5  2000/08/22 17:58:04  warmerda
 * added floating point, and nodata support
 *
 * Revision 1.4  2000/08/15 19:28:26  warmerda
 * added help topic
 *
 * Revision 1.3  1999/09/03 13:01:39  warmerda
 * added docs
 *
 * Revision 1.2  1999/06/03 21:14:01  warmerda
 * added GetGeoTransform() and GetProjectionRef() support
 *
 * Revision 1.1  1999/06/03 13:46:07  warmerda
 * New
 *
 */

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
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

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
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "SDTS Raster" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#SDTS" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "ddf" );

        poDriver->pfnOpen = SDTSDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

