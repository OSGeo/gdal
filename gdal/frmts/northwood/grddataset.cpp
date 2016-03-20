/******************************************************************************
 * $Id$
 *
 * Project:  GRD Reader
 * Purpose:  GDAL driver for Northwood Grid Format
 * Author:   Perry Casson
 *
 ******************************************************************************
 * Copyright (c) 2006, Waypoint Information Technology
 * Copyright (c) 2009-2012, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "northwood.h"

#ifdef MSVC
#include "..\..\ogr\ogrsf_frmts\mitab\mitab.h"
#else
#include "../../ogr/ogrsf_frmts/mitab/mitab.h"
#endif

/************************************************************************/
/* ==================================================================== */
/*                      NWT_GRDDataset                                  */
/* ==================================================================== */
/************************************************************************/
class NWT_GRDRasterBand;

class NWT_GRDDataset:public GDALPamDataset
{
  friend class NWT_GRDRasterBand;

    VSILFILE *fp;
    GByte abyHeader[1024];
    NWT_GRID *pGrd;
    NWT_RGB ColorMap[4096];
    char *pszProjection;

  public:
    NWT_GRDDataset();
    ~NWT_GRDDataset();

    static GDALDataset *Open( GDALOpenInfo * );
    static int Identify( GDALOpenInfo * );

    CPLErr GetGeoTransform( double *padfTransform );
    const char *GetProjectionRef();
};

/************************************************************************/
/* ==================================================================== */
/*                            NWT_GRDRasterBand                         */
/* ==================================================================== */
/************************************************************************/

class NWT_GRDRasterBand:public GDALPamRasterBand
{
  friend class NWT_GRDDataset;

    int bHaveOffsetScale;
    double dfOffset;
    double dfScale;

  public:

    NWT_GRDRasterBand( NWT_GRDDataset *, int );

    virtual CPLErr IReadBlock( int, int, void * );
    virtual double GetNoDataValue( int *pbSuccess );

    virtual GDALColorInterp GetColorInterpretation();
};


/************************************************************************/
/*                           NWT_GRDRasterBand()                        */
/************************************************************************/
NWT_GRDRasterBand::NWT_GRDRasterBand( NWT_GRDDataset * poDSIn, int nBandIn )
{
    this->poDS = poDSIn;
    this->nBand = nBandIn;

    if( nBand == 4 )
    {
        bHaveOffsetScale = TRUE;
        dfOffset = poDSIn->pGrd->fZMin;

        if( poDSIn->pGrd->cFormat == 0x01 )
        {
            eDataType = GDT_Float32;
            dfScale =( poDSIn->pGrd->fZMax - poDSIn->pGrd->fZMin ) / 4294967294.0;
        }
        else
        {
            eDataType = GDT_Float32;
            dfScale =( poDSIn->pGrd->fZMax - poDSIn->pGrd->fZMin ) / 65534.0;
        }
    }
    else
    {
        bHaveOffsetScale = FALSE;
        dfOffset = 0;
        dfScale = 1.0;
        eDataType = GDT_Byte;
    }
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

double NWT_GRDRasterBand::GetNoDataValue( int *pbSuccess )
{
    if (nBand == 4)
    {
        if( pbSuccess != NULL )
            *pbSuccess = TRUE;

        return -1.e37f;
    }

    if( pbSuccess != NULL )
        *pbSuccess = FALSE;

    return 0;
}

GDALColorInterp NWT_GRDRasterBand::GetColorInterpretation()
{
    //return GCI_RGB;
    if( nBand == 4 )
        return GCI_Undefined;
    else if( nBand == 1 )
        return GCI_RedBand;
    else if( nBand == 2 )
        return GCI_GreenBand;
    else if( nBand == 3 )
        return GCI_BlueBand;

    return GCI_Undefined;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/
CPLErr NWT_GRDRasterBand::IReadBlock( CPL_UNUSED int nBlockXOff,
                                      int nBlockYOff,
                                      void *pImage )
{
    NWT_GRDDataset *poGDS = reinterpret_cast<NWT_GRDDataset *>( poDS );
    if( nBlockXSize > INT_MAX / 2 )
        return CE_Failure;
    const int nRecordSize = nBlockXSize * 2;
    unsigned short raw1;

    VSIFSeekL( poGDS->fp,
               1024 + nRecordSize
               * static_cast<vsi_l_offset>( nBlockYOff ),
               SEEK_SET );

    GByte *pabyRecord = reinterpret_cast<GByte *>( VSI_MALLOC_VERBOSE( nRecordSize ) );
	if( pabyRecord == NULL )
		return CE_Failure;
    if( (int)VSIFReadL( pabyRecord, 1, nRecordSize, poGDS->fp ) != nRecordSize )
    {
        CPLFree( pabyRecord );
        return CE_Failure;
    }

    if( nBand == 4 )                //Z values
    {
        for( int i = 0; i < nBlockXSize; i++ )
        {
            memcpy( reinterpret_cast<void *>( &raw1 ),
                    reinterpret_cast<void *>(pabyRecord + 2 * i), 2 );
            CPL_LSBPTR16(&raw1);
            if( raw1 == 0 )
            {
              reinterpret_cast<float *>( pImage )[i] = -1.e37f;    // null value
            }
            else
            {
                reinterpret_cast<float *>( pImage )[i]
                  = static_cast<float>( dfOffset + ((raw1 - 1) * dfScale) );
            }
        }
    }
    else if( nBand == 1 )            // red values
    {
        for( int i = 0; i < nBlockXSize; i++ )
        {
            memcpy( reinterpret_cast<void *>( &raw1 ),
                    reinterpret_cast<void *>(pabyRecord + 2 * i),
                    2 );
            CPL_LSBPTR16(&raw1);
            reinterpret_cast<char *>( pImage )[i]
                = poGDS->ColorMap[raw1 / 16].r;
        }
    }
    else if( nBand == 2 )            // green
    {
        for( int i = 0; i < nBlockXSize; i++ )
        {
            memcpy( reinterpret_cast<void *> ( &raw1 ),
                    reinterpret_cast<void *> ( pabyRecord + 2 * i ),
                    2 );
            CPL_LSBPTR16(&raw1);
            reinterpret_cast<char *>( pImage )[i] = poGDS->ColorMap[raw1 / 16].g;
        }
    }
    else if( nBand == 3 )            // blue
    {
        for( int i = 0; i < nBlockXSize; i++ )
        {
            memcpy( reinterpret_cast<void *>( &raw1 ),
                    reinterpret_cast<void *>( pabyRecord + 2 * i ),
                    2 );
            CPL_LSBPTR16(&raw1);
            reinterpret_cast<char *>( pImage )[i] = poGDS->ColorMap[raw1 / 16].b;
        }
    }
    else
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "No band number %d",
                  nBand );
        CPLFree( pabyRecord );
        return CE_Failure;
    }

    CPLFree( pabyRecord );

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                             NWT_GRDDataset                           */
/* ==================================================================== */
/************************************************************************/

NWT_GRDDataset::NWT_GRDDataset() :
    fp(NULL),
    pGrd(NULL),
    pszProjection(NULL)
{
    //poCT = NULL;
    for( size_t i=0; i < CPL_ARRAYSIZE(ColorMap); ++i )
    {
        ColorMap[i].r = 0;
        ColorMap[i].g = 0;
        ColorMap[i].b = 0;
    }
}


/************************************************************************/
/*                            ~NWT_GRDDataset()                         */
/************************************************************************/

NWT_GRDDataset::~NWT_GRDDataset()
{
    FlushCache();
    pGrd->fp = NULL;       // this prevents nwtCloseGrid from closing the fp
    nwtCloseGrid( pGrd );

    if( fp != NULL )
        VSIFCloseL( fp );

    if( pszProjection != NULL )
    {
        CPLFree( pszProjection );
    }
    /*if( poCT != NULL )
        delete poCT;*/
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr NWT_GRDDataset::GetGeoTransform( double *padfTransform )
{
    padfTransform[0] = pGrd->dfMinX - ( pGrd->dfStepSize * 0.5 );
    padfTransform[3] = pGrd->dfMaxY + ( pGrd->dfStepSize * 0.5 );
    padfTransform[1] = pGrd->dfStepSize;
    padfTransform[2] = 0.0;

    padfTransform[4] = 0.0;
    padfTransform[5] = -1 * pGrd->dfStepSize;

    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *NWT_GRDDataset::GetProjectionRef()
{
    if (pszProjection == NULL)
    {
        OGRSpatialReference *poSpatialRef;
        poSpatialRef = MITABCoordSys2SpatialRef( pGrd->cMICoordSys );
        if (poSpatialRef)
        {
            poSpatialRef->exportToWkt( &pszProjection );
            poSpatialRef->Release();
        }
    }
    return reinterpret_cast<const char *>( pszProjection );
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int NWT_GRDDataset::Identify( GDALOpenInfo * poOpenInfo )
{
/* -------------------------------------------------------------------- */
/*  Look for the header                                                 */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 1024 )
        return FALSE;

    if( poOpenInfo->pabyHeader[0] != 'H' ||
        poOpenInfo->pabyHeader[1] != 'G' ||
        poOpenInfo->pabyHeader[2] != 'P' ||
        poOpenInfo->pabyHeader[3] != 'C' ||
        poOpenInfo->pabyHeader[4] != '1' )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/
GDALDataset *NWT_GRDDataset::Open( GDALOpenInfo * poOpenInfo )
{
    if( !Identify(poOpenInfo) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    NWT_GRDDataset *poDS;

    poDS = new NWT_GRDDataset();

    poDS->fp = VSIFOpenL(poOpenInfo->pszFilename, "rb");
    if (poDS->fp == NULL)
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */
    VSIFSeekL( poDS->fp, 0, SEEK_SET );
    VSIFReadL( poDS->abyHeader, 1, 1024, poDS->fp );
    poDS->pGrd = reinterpret_cast<NWT_GRID *>( malloc( sizeof( NWT_GRID ) ) );

    poDS->pGrd->fp = poDS->fp;

    if (!nwt_ParseHeader( poDS->pGrd, reinterpret_cast<char *>( poDS->abyHeader ) ) ||
        !GDALCheckDatasetDimensions(poDS->pGrd->nXSide, poDS->pGrd->nYSide) )
    {
        delete poDS;
        return NULL;
    }

    poDS->nRasterXSize = poDS->pGrd->nXSide;
    poDS->nRasterYSize = poDS->pGrd->nYSide;

    // create a colorTable
    // if( poDS->pGrd->iNumColorInflections > 0 )
    //   poDS->CreateColorTable();
    nwt_LoadColors( poDS->ColorMap, 4096, poDS->pGrd );
/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->SetBand( 1, new NWT_GRDRasterBand( poDS, 1 ) );    //r
    poDS->SetBand( 2, new NWT_GRDRasterBand( poDS, 2 ) );    //g
    poDS->SetBand( 3, new NWT_GRDRasterBand( poDS, 3 ) );    //b
    poDS->SetBand( 4, new NWT_GRDRasterBand( poDS, 4 ) );    //z

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for external overviews.                                   */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename,
                                 poOpenInfo->GetSiblingFiles() );

    return (poDS);
}


/************************************************************************/
/*                          GDALRegister_GRD()                          */
/************************************************************************/
void GDALRegister_NWT_GRD()
{
    if( GDALGetDriverByName( "NWT_GRD" ) != NULL )
      return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "NWT_GRD" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "Northwood Numeric Grid Format .grd/.tab" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_various.html#grd");
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "grd" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = NWT_GRDDataset::Open;
    poDriver->pfnIdentify = NWT_GRDDataset::Identify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
