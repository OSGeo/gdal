/******************************************************************************
 * $Id$
 *
 * Project:  Multi-resolution Seamless Image Database (MrSID)
 * Purpose:  Read LizardTech's MrSID file format
 * Author:   Andrey Kiselev, dron@remotesensing.org
 *
 ******************************************************************************
 * Copyright (c) 2003, Andrey Kiselev <dron@remotesensing.org>
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
 * Revision 1.1  2003/04/23 12:21:56  dron
 * New.
 *
 */

#include "gdal_priv.h"
#include "cpl_string.h"

#include "lt_fileUtils.h"
#include "lt_xTrans.h"
#include "lt_imageBufferInfo.h"
#include "lt_pixel.h"
#include "lt_colorSpace.h"
#include "MrSIDImageFile.h"
#include "MrSIDNavigator.h"
#include "MetadataReader.h"
#include "MetadataElement.h"

LT_USE_NAMESPACE(LizardTech)

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_MrSID(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*                              MrSIDDataset                            */
/* ==================================================================== */
/************************************************************************/

class MrSIDDataset : public GDALDataset
{
    friend class MrSIDRasterBand;

    FileSpecification   *poFilename;
    MrSIDImageFile      *poMrSidFile;
    MrSIDNavigator      *poMrSidNav;
    Pixel               *poDefaultPixel;
    MetadataReader      *poMrSidMetadata;

    ImageBufferInfo::SampleType eSampleType;
    GDALDataType        eDataType;
    ColorSpace          *poColorSpace;

    int                 nCurrentZoomLevel;

    int                 bHasGeoTransfom;
    double              adfGeoTransform[6];
    char                *pszProjection;

    int		        nOverviewCount;
    MrSIDDataset        **papoOverviewDS;

  public:
                MrSIDDataset();
                ~MrSIDDataset();

    CPLErr              OpenZoomLevel( int iZoom );
    static GDALDataset  *Open( GDALOpenInfo * );

    CPLErr              GetGeoTransform( double * padfTransform );
    const char          *GetProjectionRef();
    const char          *SerializeMetadataElement( const MetadataElement *poElem );
};

/************************************************************************/
/* ==================================================================== */
/*                           MrSIDRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class MrSIDRasterBand : public GDALRasterBand
{
    friend class MrSIDDataset;

    ImageBufferInfo *poImageBufInfo;
    ImageBuffer     *poImageBuf;

    int             nBlockSize;

  public:

                MrSIDRasterBand( MrSIDDataset *, int );
                ~MrSIDRasterBand();

    virtual CPLErr          IReadBlock( int, int, void * );
    virtual GDALColorInterp GetColorInterpretation();
    virtual int             GetOverviewCount();
    virtual GDALRasterBand  *GetOverview( int );
};

/************************************************************************/
/*                           MrSIDRasterBand()                          */
/************************************************************************/

MrSIDRasterBand::MrSIDRasterBand( MrSIDDataset *poDS, int nBand )
{
    this->poDS = poDS;
    this->nBand = nBand;
    this->eDataType = poDS->eDataType;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = poDS->GetRasterYSize();
    nBlockSize = nBlockXSize * nBlockYSize;
    poDS->poMrSidNav->zoomTo( poDS->nCurrentZoomLevel ); 
    poDS->poMrSidNav->resize( nBlockXSize, nBlockYSize, IntRect::TOP_LEFT );

    poImageBufInfo = new ImageBufferInfo( ImageBufferInfo::BIP,
                                          *poDS->poColorSpace,
                                          poDS->eSampleType );

    CPLDebug( "MrSID",
              "Band %d: set nBlockXSize=%d, nBlockYSize=%d, nBlockSize=%d",
              nBand, nBlockXSize, nBlockYSize, nBlockSize );
}

/************************************************************************/
/*                            ~MrSIDRasterBand()                        */
/************************************************************************/

MrSIDRasterBand::~MrSIDRasterBand()
{
    if ( poImageBufInfo )
        delete poImageBufInfo;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr MrSIDRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                    void * pImage )
{
    MrSIDDataset    *poGDS = (MrSIDDataset *)poDS;
    int             i, j;

    poImageBuf = new ImageBuffer( *poImageBufInfo );
    poGDS->poMrSidNav->panTo( nBlockXOff * nBlockYSize,
                              nBlockYOff * nBlockXSize,
                              IntRect::TOP_LEFT );
    poImageBuf->setStripHeight( nBlockYSize );

    try
    {
        poGDS->poMrSidNav->loadImage( *poImageBuf );
    }
    catch ( Exception oException )
    {
        delete poImageBuf;
        CPLError( CE_Failure, CPLE_AppDefined, oException.what() );
        return CE_Failure;
    }

    for ( i = 0, j = nBand - 1; i < nBlockSize; i++, j+=poGDS->nBands )
    {
        switch( eDataType )
        {
            case GDT_UInt16:
            {
                ((GUInt16*)pImage)[i] = ((GUInt16*)poImageBuf->getData())[j];
            }
            break;
            case GDT_Float32:
            {
                ((float*)pImage)[i] = ((float*)poImageBuf->getData())[j];
            }
            break;
            case GDT_Float64:
            {
                ((double*)pImage)[i] = ((double*)poImageBuf->getData())[j];
            }
            break;
            case GDT_Byte:
            default:
            {
                ((GByte*)pImage)[i] = ((GByte*)poImageBuf->getData())[j];
            }
            break;
            }
    }
    delete poImageBuf;

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp MrSIDRasterBand::GetColorInterpretation()
{
    MrSIDDataset      *poGDS = (MrSIDDataset *) poDS;

    switch( poGDS->poColorSpace->scheme() )
    {
        case ColorSpace::RGB:
            if( nBand == 1 )
                return GCI_RedBand;
            else if( nBand == 2 )
                return GCI_GreenBand;
            else if( nBand == 3 )
                return GCI_BlueBand;
            else
                return GCI_Undefined;
        case ColorSpace::CMYK:
            if( nBand == 1 )
                return GCI_CyanBand;
            else if( nBand == 2 )
                return GCI_MagentaBand;
            else if( nBand == 3 )
                return GCI_YellowBand;
            else if( nBand == 4 )
                return GCI_BlackBand;
            else
                return GCI_Undefined;
        case ColorSpace::GRAYSCALE:
            return GCI_GrayIndex;
        case ColorSpace::RGBK:
            if( nBand == 1 )
                return GCI_RedBand;
            else if( nBand == 2 )
                return GCI_GreenBand;
            else if( nBand == 3 )
                return GCI_BlueBand;
            else if( nBand == 4 )
                return GCI_AlphaBand;
            else
                return GCI_Undefined;
        default:
            return GCI_Undefined;
    }
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int MrSIDRasterBand::GetOverviewCount()

{
    MrSIDDataset        *poGDS = (MrSIDDataset *) poDS;

    return poGDS->nOverviewCount;
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *MrSIDRasterBand::GetOverview( int i )

{
    MrSIDDataset        *poGDS = (MrSIDDataset *) poDS;

    if( i < 0 || i >= poGDS->nOverviewCount )
        return NULL;
    else
        return poGDS->papoOverviewDS[i]->GetRasterBand( nBand );
}

/************************************************************************/
/*                           MrSIDDataset()                             */
/************************************************************************/

MrSIDDataset::MrSIDDataset()
{
    poFilename = NULL;
    poMrSidFile = NULL;
    poMrSidNav = NULL;
    poDefaultPixel = NULL;
    poMrSidMetadata = NULL;
    poColorSpace = NULL;
    nBands = 0;
    eSampleType = ImageBufferInfo::UINT8;
    eDataType = GDT_Byte;
    pszProjection = CPLStrdup( "" );
    bHasGeoTransfom = FALSE;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
    nCurrentZoomLevel = 0;
    nOverviewCount = 0;
    papoOverviewDS = NULL;
}

/************************************************************************/
/*                            ~MrSIDDataset()                           */
/************************************************************************/

MrSIDDataset::~MrSIDDataset()
{
    // Delete MrSID file object only in base dataset object and don't delete
    // it in overviews. Filename object in overviews will be empty.
    if ( poMrSidFile && poFilename )
        delete poMrSidFile;
    if ( poFilename )
        delete poFilename;
    if ( poMrSidNav )
        delete poMrSidNav;
    if ( poDefaultPixel )
        delete poDefaultPixel;
    if ( poMrSidMetadata )
        delete poMrSidMetadata;
    if ( poColorSpace )
        delete poColorSpace;
    if ( pszProjection )
        CPLFree( pszProjection );
    if ( papoOverviewDS )
    {
        for( int i = 0; i < nOverviewCount; i++ )
            delete papoOverviewDS[i];
        CPLFree( papoOverviewDS );
    }
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr MrSIDDataset::GetGeoTransform( double * padfTransform )
{
    memcpy( padfTransform, adfGeoTransform, sizeof(adfGeoTransform[0]) * 6 );

    if ( !bHasGeoTransfom )
        return CE_Failure;

    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *MrSIDDataset::GetProjectionRef()
{
    if( pszProjection )
        return pszProjection;
    else
        return "";
}

/************************************************************************/
/*                      SerializeMetadataElement()                      */
/************************************************************************/

const char *MrSIDDataset::SerializeMetadataElement( const MetadataElement
                                                    *poElem )
{
    switch( poElem->type() )
    {
        case MetadataValue::BYTE:
        case MetadataValue::SHORT:
        case MetadataValue::LONG:
            return CPLSPrintf( "%lu", (unsigned long)poElem->getMetadataValue() );
        break;
        case MetadataValue::SBYTE:
        case MetadataValue::SSHORT:
        case MetadataValue::SLONG:
            return CPLSPrintf( "%ld", (long)poElem->getMetadataValue() );
        break;
        case MetadataValue::FLOAT:
            return CPLSPrintf( "%f", (float)poElem->getMetadataValue() );
        break;
        case MetadataValue::DOUBLE:
            return CPLSPrintf( "%lf", (double)poElem->getMetadataValue() );
        break;
        case MetadataValue::ASCII:
            return (const char*)poElem->getMetadataValue();
        break;
        default:
            return "";
        break;
    }
}

/************************************************************************/
/*                             OpenZoomLevel()                          */
/************************************************************************/

CPLErr MrSIDDataset::OpenZoomLevel( int iZoom )
{
    try
    {
        poMrSidNav = new MrSIDNavigator( *poMrSidFile );
    }
    catch ( Exception oException )
    {
        CPLError( CE_Failure, CPLE_AppDefined, oException.what() );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Handle sample type and color space.                             */
/* -------------------------------------------------------------------- */
    poDefaultPixel =
        new Pixel( poMrSidFile->getDefaultPixelValue() );
    eSampleType = poDefaultPixel->getProperties().getSampleType();
    poColorSpace =
        new ColorSpace( poDefaultPixel->getProperties().colorSpace() );
    switch ( eSampleType )
    {
        case ImageBufferInfo::UINT16:
        eDataType = GDT_UInt16;
        break;
        case ImageBufferInfo::UINT32:
        eDataType = GDT_UInt32;
        break;
        case ImageBufferInfo::FLOAT32:
        eDataType = GDT_Float32;
        break;
        case ImageBufferInfo::FLOAT64:
        eDataType = GDT_Float64;
        break;
        case ImageBufferInfo::UINT8:
        default:
        eDataType = GDT_Byte;
        break;
    }

/* -------------------------------------------------------------------- */
/*      Take image geometry.                                            */
/* -------------------------------------------------------------------- */
    nRasterXSize = poMrSidFile->width();
    nRasterYSize = poMrSidFile->height();
    nBands = poMrSidFile->nband();

    CPLAssert( poColorSpace->samplesPerPixel () == nBands);

/* -------------------------------------------------------------------- */
/*      Take georeferencing.                                            */
/* -------------------------------------------------------------------- */
    if ( poMrSidNav->hasWorldInfo()
         && poMrSidNav->xu(adfGeoTransform[0])
         && poMrSidNav->yu(adfGeoTransform[3])
         && poMrSidNav->xres(adfGeoTransform[1])
         && poMrSidNav->yres(adfGeoTransform[5])
         && poMrSidNav->xrot(adfGeoTransform[2])
         && poMrSidNav->yrot(adfGeoTransform[4]) )
    {
        bHasGeoTransfom = TRUE;
    }

    if ( iZoom != 0 )
    {
        nCurrentZoomLevel = iZoom;
        nRasterXSize /= 1<<iZoom;
        nRasterYSize /= 1<<iZoom;
    }
    else
    {
        nCurrentZoomLevel = 0;
    }

    CPLDebug( "MrSID", "Opened zoom level %d with size %dx%d.\n",
              iZoom, nRasterXSize, nRasterYSize );

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int             iBand;

    for( iBand = 1; iBand <= nBands; iBand++ )
        SetBand( iBand, new MrSIDRasterBand( this, iBand ) );

    return CE_None;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *MrSIDDataset::Open( GDALOpenInfo * poOpenInfo )
{
    if( poOpenInfo->fp == NULL )
        return NULL;

    if( !EQUALN((const char *) poOpenInfo->pabyHeader, "msid", 4) )
        return NULL;

    VSIFCloseL( poOpenInfo->fp );
    poOpenInfo->fp = NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    MrSIDDataset      *poDS;

    poDS = new MrSIDDataset();
    try
    {
        poDS->poFilename = new FileSpecification( poOpenInfo->pszFilename );
        poDS->poMrSidFile = new MrSIDImageFile( *poDS->poFilename );
    }
    catch ( Exception oException )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_AppDefined, oException.what() );
        return NULL;
    }

    XTrans::initialize();

/* -------------------------------------------------------------------- */
/*      Take metadata.                                                  */
/* -------------------------------------------------------------------- */
    poDS->poMrSidMetadata = new MetadataReader( poDS->poMrSidFile->metadata() );
    if ( !poDS->poMrSidMetadata->empty() )
    {
        MetadataReader::const_iterator i = poDS->poMrSidMetadata->begin();

        while( i != poDS->poMrSidMetadata->end() )
        {
            poDS->SetMetadataItem( (*i).getKey().c_str(),
                                   poDS->SerializeMetadataElement( &(*i) ) );
            i++;
        }
    }

/* -------------------------------------------------------------------- */
/*   Take number of resolution levels (we will use them as overviews).  */
/* -------------------------------------------------------------------- */
    poDS->nOverviewCount = poDS->poMrSidFile->nlev() - 1;

    CPLDebug( "MrSID",
              "Opened image: width %d, height %d, bands %d, overviews %d",
              poDS->nRasterXSize, poDS->nRasterYSize, poDS->nBands,
              poDS->nOverviewCount );

    if ( poDS->nOverviewCount > 0 )
    {
        int         i;

        poDS->papoOverviewDS = (MrSIDDataset **)
            CPLMalloc( poDS->nOverviewCount * (sizeof(void*)) );

        for ( i = 0; i < poDS->nOverviewCount; i++ )
        {
            poDS->papoOverviewDS[i] = new MrSIDDataset();

            poDS->papoOverviewDS[i]->poMrSidFile = poDS->poMrSidFile;

            poDS->papoOverviewDS[i]->OpenZoomLevel( i + 1 );
            
        }
    }

/* -------------------------------------------------------------------- */
/*      Band objects will be created in separate function.              */
/* -------------------------------------------------------------------- */
    poDS->OpenZoomLevel( 0 );

    return( poDS );
}

/************************************************************************/
/*                        GDALRegister_MrSID()                          */
/************************************************************************/

void GDALRegister_MrSID()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "MrSID" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "MrSID" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Multi-resolution Seamless Image Database (MrSID)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "frmt_mrsid.html" );

        poDriver->pfnOpen = MrSIDDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

