/******************************************************************************
 *
 * Project:  GRC Reader
 * Purpose:  GDAL driver for Northwood Classified Format
 * Author:   Perry Casson
 *
 ******************************************************************************
 * Copyright (c) 2007, Waypoint Information Technology
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

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                             NWT_GRCDataset                           */
/* ==================================================================== */
/************************************************************************/
class NWT_GRCRasterBand;

class NWT_GRCDataset : public GDALPamDataset
{
  friend class NWT_GRCRasterBand;

  private:
    VSILFILE * fp;
    GByte abyHeader[1024];
    NWT_GRID *pGrd;
    char **papszCategories;
    char *pszProjection;

  protected:
    GDALColorTable * poColorTable;

  public:
    NWT_GRCDataset();
    ~NWT_GRCDataset();

    static GDALDataset *Open( GDALOpenInfo * );
    static int Identify( GDALOpenInfo * poOpenInfo );

    CPLErr GetGeoTransform( double *padfTransform ) override;
    const char *GetProjectionRef() override;
};

/************************************************************************/
/* ==================================================================== */
/*                            NWT_GRCRasterBand                         */
/* ==================================================================== */
/************************************************************************/

class NWT_GRCRasterBand : public GDALPamRasterBand
{
  friend class NWT_GRCDataset;

  public:

    NWT_GRCRasterBand( NWT_GRCDataset *, int );
    virtual ~NWT_GRCRasterBand();

    virtual CPLErr IReadBlock( int, int, void * ) override;
    virtual double GetNoDataValue( int *pbSuccess ) override;

    virtual GDALColorInterp GetColorInterpretation() override;
    virtual char **GetCategoryNames() override;
    virtual GDALColorTable *GetColorTable() override;
};

/************************************************************************/
/*                           NWT_GRCRasterBand()                        */
/************************************************************************/

NWT_GRCRasterBand::NWT_GRCRasterBand( NWT_GRCDataset * poDSIn, int nBandIn )
{
    poDS = poDSIn;
    nBand = nBandIn;
    NWT_GRCDataset *poGDS = reinterpret_cast<NWT_GRCDataset *>( poDS );

    if( poGDS->pGrd->nBitsPerPixel == 8 )
        eDataType = GDT_Byte;
    else if( poGDS->pGrd->nBitsPerPixel == 16 )
        eDataType = GDT_UInt16;
    else /* if( poGDS->pGrd->nBitsPerPixel == 32 ) */
        eDataType = GDT_UInt32;        // this would be funny

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

    // load the color table and might as well to the ClassNames
    poGDS->poColorTable = new GDALColorTable();

    GDALColorEntry oEntry = { 255, 255, 255, 0 };
    // null value = 0 is transparent
    // alpha 0 = transparent

    poGDS->poColorTable->SetColorEntry( 0, &oEntry );

    for( int i = 0;
         i < static_cast<int>( poGDS->pGrd->stClassDict->nNumClassifiedItems );
         i++ )
    {
        oEntry.c1 = poGDS->pGrd->stClassDict->stClassifedItem[i]->r;
        oEntry.c2 = poGDS->pGrd->stClassDict->stClassifedItem[i]->g;
        oEntry.c3 = poGDS->pGrd->stClassDict->stClassifedItem[i]->b;
        oEntry.c4 = 255;            // alpha 255 = solid

        poGDS->poColorTable->SetColorEntry( poGDS->pGrd->
                                          stClassDict->stClassifedItem[i]->
                                          usPixVal, &oEntry );
    }

    // find the max value used in the grc
    int maxValue = 0;
    for( int i=0; i < static_cast<int>( poGDS->pGrd->stClassDict->nNumClassifiedItems ); i++ )
    {
        if( poGDS->pGrd->stClassDict->stClassifedItem[i]->usPixVal > maxValue )
            maxValue = poGDS->pGrd->stClassDict->stClassifedItem[i]->usPixVal;
    }

    // load a value for the null value
    poGDS->papszCategories = CSLAddString( poGDS->papszCategories, "No Data" );

    // for the class names we need to load nulls string for all classes that
    // are not defined
    for( int val = 1; val <= maxValue; val++ )
    {
        int i = 0;
        // Loop through the GRC dictionary to see if the value is defined.
        for( ;
             i < static_cast<int>( poGDS->pGrd->stClassDict->nNumClassifiedItems );
             i++ )
        {
            if( static_cast<int>( poGDS->pGrd->stClassDict->stClassifedItem[i]->usPixVal ) ==
                val )
            {
                poGDS->papszCategories =
                    CSLAddString( poGDS->papszCategories,
                                    poGDS->pGrd->stClassDict->
                                    stClassifedItem[i]->szClassName );
                break;
            }
        }
        if( i >= static_cast<int>( poGDS->pGrd->stClassDict->nNumClassifiedItems ) )
            poGDS->papszCategories = CSLAddString( poGDS->papszCategories, "" );
    }
}

NWT_GRCRasterBand::~NWT_GRCRasterBand() {}

double NWT_GRCRasterBand::GetNoDataValue( int *pbSuccess )
{
    if( pbSuccess != NULL )
        *pbSuccess = TRUE;

    return 0.0;  // Northwood grid 0 is always null.
}

// return an array of null terminated strings for the class names
char **NWT_GRCRasterBand::GetCategoryNames()
{
    NWT_GRCDataset *poGDS = reinterpret_cast<NWT_GRCDataset *>( poDS );

    return poGDS->papszCategories;
}

// return the color table
GDALColorTable *NWT_GRCRasterBand::GetColorTable()
{
    NWT_GRCDataset *poGDS = reinterpret_cast<NWT_GRCDataset *>( poDS );

    return poGDS->poColorTable;
}

GDALColorInterp NWT_GRCRasterBand::GetColorInterpretation()
{
    if( nBand == 1 )
        return GCI_PaletteIndex;

    return GCI_Undefined;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/
CPLErr NWT_GRCRasterBand::IReadBlock( CPL_UNUSED int nBlockXOff,
                                      int nBlockYOff,
                                      void *pImage )
{
    NWT_GRCDataset *poGDS = reinterpret_cast<NWT_GRCDataset *>( poDS );
    const int nBytesPerPixel = poGDS->pGrd->nBitsPerPixel / 8;
    if( nBytesPerPixel <= 0 || nBlockXSize > INT_MAX / nBytesPerPixel )
        return CE_Failure;
    const int nRecordSize = nBlockXSize * nBytesPerPixel;

    if( nBand == 1 )
    {                            //grc's are just one band of indices
        VSIFSeekL( poGDS->fp, 1024 + nRecordSize * (vsi_l_offset)nBlockYOff, SEEK_SET );
        if( (int)VSIFReadL( pImage, 1, nRecordSize, poGDS->fp ) != nRecordSize )
            return CE_Failure;
    }
    else
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "No band number %d",
                  nBand );
        return CE_Failure;
    }
    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                          NWT_GRCDataset                              */
/* ==================================================================== */
/************************************************************************/
NWT_GRCDataset::NWT_GRCDataset() :
    fp(NULL),
    pGrd(NULL),
    papszCategories(NULL),
    pszProjection(NULL),
    poColorTable(NULL)
{
    memset(abyHeader, 0, sizeof(abyHeader) );
}

/************************************************************************/
/*                            ~NWT_GRCDataset()                         */
/************************************************************************/
NWT_GRCDataset::~NWT_GRCDataset()
{
    delete poColorTable;
    CSLDestroy( papszCategories );

    FlushCache();
    pGrd->fp = NULL;       // this prevents nwtCloseGrid from closing the fp
    nwtCloseGrid( pGrd );

    if( fp != NULL )
        VSIFCloseL( fp );

    CPLFree( pszProjection );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/
CPLErr NWT_GRCDataset::GetGeoTransform( double *padfTransform )
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
const char *NWT_GRCDataset::GetProjectionRef()
{
    if (pszProjection == NULL)
    {
        OGRSpatialReference *poSpatialRef
          = MITABCoordSys2SpatialRef( pGrd->cMICoordSys );
        if (poSpatialRef)
        {
            poSpatialRef->exportToWkt( &pszProjection );
            poSpatialRef->Release();
        }
    }
    return (const char *) pszProjection;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int NWT_GRCDataset::Identify( GDALOpenInfo * poOpenInfo )
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
        poOpenInfo->pabyHeader[4] != '8' )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *NWT_GRCDataset::Open( GDALOpenInfo * poOpenInfo )
{
    if( !Identify(poOpenInfo) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    NWT_GRCDataset *poDS = new NWT_GRCDataset();

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
    poDS->pGrd = reinterpret_cast<NWT_GRID *>( malloc( sizeof (NWT_GRID) ) );

    poDS->pGrd->fp = poDS->fp;

    if (!nwt_ParseHeader( poDS->pGrd, reinterpret_cast<char *>( poDS->abyHeader ) ) ||
        !GDALCheckDatasetDimensions(poDS->pGrd->nXSide, poDS->pGrd->nYSide) ||
        poDS->pGrd->stClassDict == NULL)
    {
        delete poDS;
        return NULL;
    }

    if( poDS->pGrd->nBitsPerPixel != 8 &&
        poDS->pGrd->nBitsPerPixel != 16 &&
        poDS->pGrd->nBitsPerPixel != 32 )
    {
        delete poDS;
        return NULL;
    }

    poDS->nRasterXSize = poDS->pGrd->nXSide;
    poDS->nRasterYSize = poDS->pGrd->nYSide;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->SetBand( 1, new NWT_GRCRasterBand( poDS, 1) );    //Class Indexes

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for external overviews.                                   */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS,
                                 poOpenInfo->pszFilename,
                                 poOpenInfo->GetSiblingFiles() );

    return poDS;
}

/************************************************************************/
/*                          GDALRegister_GRC()                          */
/************************************************************************/

void GDALRegister_NWT_GRC()

{
    if( GDALGetDriverByName( "NWT_GRC" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "NWT_GRC" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "Northwood Classified Grid Format .grc/.tab");
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                               "frmt_various.html#northwood_grc" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "grc" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = NWT_GRCDataset::Open;
    poDriver->pfnIdentify = NWT_GRCDataset::Identify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
