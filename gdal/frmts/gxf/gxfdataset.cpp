/******************************************************************************
 *
 * Project:  GXF Reader
 * Purpose:  GDAL binding for GXF reader.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at spatialys.com>
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
#include "gxfopen.h"

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                              GXFDataset                              */
/* ==================================================================== */
/************************************************************************/

class GXFRasterBand;

class GXFDataset final: public GDALPamDataset
{
    friend class GXFRasterBand;

    GXFHandle   hGXF;

    char        *pszProjection;
    double      dfNoDataValue;
    GDALDataType eDataType;

  public:
                GXFDataset();
                ~GXFDataset();

    static GDALDataset *Open( GDALOpenInfo * );

    CPLErr      GetGeoTransform( double * padfTransform ) override;
    const char *_GetProjectionRef() override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }
};

/************************************************************************/
/* ==================================================================== */
/*                            GXFRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class GXFRasterBand final: public GDALPamRasterBand
{
    friend class GXFDataset;

  public:

                GXFRasterBand( GXFDataset *, int );
    double      GetNoDataValue( int* bGotNoDataValue ) override;

    virtual CPLErr IReadBlock( int, int, void * ) override;
};

/************************************************************************/
/*                           GXFRasterBand()                            */
/************************************************************************/

GXFRasterBand::GXFRasterBand( GXFDataset *poDSIn, int nBandIn )

{
    poDS = poDSIn;
    nBand = nBandIn;

    eDataType = poDSIn->eDataType;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                          GetNoDataValue()                          */
/************************************************************************/

double GXFRasterBand::GetNoDataValue(int* bGotNoDataValue)

{
    GXFDataset *poGXF_DS = (GXFDataset *) poDS;
    if( bGotNoDataValue )
        *bGotNoDataValue = (fabs(poGXF_DS->dfNoDataValue - -1e12) > .1);
    if( eDataType == GDT_Float32 )
        return (double)(float)poGXF_DS->dfNoDataValue;

    return poGXF_DS->dfNoDataValue;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GXFRasterBand::IReadBlock( CPL_UNUSED int nBlockXOff,
                                  int nBlockYOff,
                                  void * pImage )
{
    GXFDataset * const poGXF_DS = (GXFDataset *) poDS;

    if( eDataType == GDT_Float32)
    {
       double *padfBuffer = (double *) VSIMalloc2(sizeof(double), nBlockXSize);
       if( padfBuffer == nullptr )
           return CE_Failure;
       const CPLErr eErr =
           GXFGetScanline( poGXF_DS->hGXF, nBlockYOff, padfBuffer );

       float *pafBuffer = (float *) pImage;
       for( int i = 0; i < nBlockXSize; i++ )
           pafBuffer[i] = (float) padfBuffer[i];

       CPLFree( padfBuffer );

       return eErr;
    }

    const CPLErr eErr =
        eDataType == GDT_Float64
        ? GXFGetScanline( poGXF_DS->hGXF, nBlockYOff, (double*)pImage )
        : CE_Failure;

    return eErr;
}

/************************************************************************/
/* ==================================================================== */
/*                              GXFDataset                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             GXFDataset()                             */
/************************************************************************/

GXFDataset::GXFDataset() :
    hGXF(nullptr),
    pszProjection(nullptr),
    dfNoDataValue(0),
    eDataType(GDT_Float32)
{}

/************************************************************************/
/*                            ~GXFDataset()                             */
/************************************************************************/

GXFDataset::~GXFDataset()

{
    FlushCache();
    if( hGXF != nullptr )
        GXFClose( hGXF );
    CPLFree( pszProjection );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GXFDataset::GetGeoTransform( double * padfTransform )

{
    double dfXOrigin = 0.0;
    double dfYOrigin = 0.0;
    double dfXSize = 0.0;
    double dfYSize = 0.0;
    double dfRotation = 0.0;

    const CPLErr eErr =
        GXFGetPosition( hGXF, &dfXOrigin, &dfYOrigin,
                        &dfXSize, &dfYSize, &dfRotation );

    if( eErr != CE_None )
        return eErr;

    // Transform to radians.
    dfRotation = (dfRotation / 360.0) * 2.0 * M_PI;

    padfTransform[1] = dfXSize * cos(dfRotation);
    padfTransform[2] = dfYSize * sin(dfRotation);
    padfTransform[4] = dfXSize * sin(dfRotation);
    padfTransform[5] = -1 * dfYSize * cos(dfRotation);

    // take into account that GXF is point or center of pixel oriented.
    padfTransform[0] = dfXOrigin - 0.5*padfTransform[1] - 0.5*padfTransform[2];
    padfTransform[3] = dfYOrigin - 0.5*padfTransform[4] - 0.5*padfTransform[5];

    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *GXFDataset::_GetProjectionRef()

{
    return pszProjection;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GXFDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Before trying GXFOpen() we first verify that there is at        */
/*      least one "\n#keyword" type signature in the first chunk of     */
/*      the file.                                                       */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 50 || poOpenInfo->fpL == nullptr )
        return nullptr;

    bool bFoundKeyword = false;
    bool bFoundIllegal = false;
    for( int i = 0; i < poOpenInfo->nHeaderBytes-1; i++ )
    {
        if( (poOpenInfo->pabyHeader[i] == 10
             || poOpenInfo->pabyHeader[i] == 13)
            && poOpenInfo->pabyHeader[i+1] == '#' )
        {
            if( STARTS_WITH((const char*)poOpenInfo->pabyHeader + i + 2,
                            "include") )
                return nullptr;
            if( STARTS_WITH((const char*)poOpenInfo->pabyHeader + i + 2,
                            "define") )
                return nullptr;
            if( STARTS_WITH((const char*)poOpenInfo->pabyHeader + i + 2,
                            "ifdef") )
                return nullptr;
            bFoundKeyword = true;
        }
        if( poOpenInfo->pabyHeader[i] == 0 )
        {
            bFoundIllegal = true;
            break;
        }
    }

    if( !bFoundKeyword || bFoundIllegal )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      At this point it is plausible that this is a GXF file, but      */
/*      we also now verify that there is a #GRID keyword before         */
/*      passing it off to GXFOpen().  We check in the first 50K.        */
/* -------------------------------------------------------------------- */
    CPL_IGNORE_RET_VAL(poOpenInfo->TryToIngest(50000));
    bool bGotGrid = false;

    const char* pszBigBuf = (const char*)poOpenInfo->pabyHeader;
    for( int i = 0; i < poOpenInfo->nHeaderBytes - 5 && !bGotGrid; i++ )
    {
        if( pszBigBuf[i] == '#' && STARTS_WITH_CI(pszBigBuf+i+1, "GRID") )
            bGotGrid = true;
    }

    if( !bGotGrid )
        return nullptr;

    VSIFCloseL( poOpenInfo->fpL );
    poOpenInfo->fpL = nullptr;

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */

    GXFHandle l_hGXF = GXFOpen( poOpenInfo->pszFilename );

    if( l_hGXF == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        GXFClose(l_hGXF);
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The GXF driver does not support update access to existing"
                  " datasets." );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GXFDataset *poDS = new GXFDataset();

    const char* pszGXFDataType = CPLGetConfigOption("GXF_DATATYPE", "Float32");
    GDALDataType eDT = GDALGetDataTypeByName(pszGXFDataType);
    if( !(eDT == GDT_Float32 || eDT == GDT_Float64) )
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Unsupported value for GXF_DATATYPE : %s", pszGXFDataType);
        eDT = GDT_Float32;
    }

    poDS->hGXF = l_hGXF;
    poDS->eDataType = eDT;

/* -------------------------------------------------------------------- */
/*      Establish the projection.                                       */
/* -------------------------------------------------------------------- */
    poDS->pszProjection = GXFGetMapProjectionAsOGCWKT( l_hGXF );

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    GXFGetRawInfo( l_hGXF, &(poDS->nRasterXSize), &(poDS->nRasterYSize), nullptr,
                   nullptr, nullptr, &(poDS->dfNoDataValue) );

    if( poDS->nRasterXSize <= 0 || poDS->nRasterYSize <= 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid dimensions : %d x %d",
                  poDS->nRasterXSize, poDS->nRasterYSize);
        delete poDS;
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->nBands = 1;
    poDS->SetBand( 1, new GXFRasterBand( poDS, 1 ));

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

    return poDS;
}

/************************************************************************/
/*                          GDALRegister_GXF()                          */
/************************************************************************/

void GDALRegister_GXF()

{
    if( GDALGetDriverByName( "GXF" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "GXF" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "GeoSoft Grid Exchange Format" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/gxf.html" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "gxf" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = GXFDataset::Open;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
