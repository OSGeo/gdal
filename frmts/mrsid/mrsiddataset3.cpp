/******************************************************************************
 * $Id$
 *
 * Project:  Multi-resolution Seamless Image Database (MrSID)
 * Purpose:  Read LizardTech's MrSID file format - Version 3 SDK.
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
 * Revision 1.1  2005/02/16 22:07:22  fwarmerdam
 * New
 *
 */

#include "gdal_priv.h"
#include "ogr_spatialref.h"
#include "cpl_string.h"

#include <geo_normalize.h>
#include <geovalues.h>

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_MrSID(void);
double GTIFAngleToDD( double dfAngle, int nUOMAngle );
CPL_C_END

/************************************************************************/
/* There are several different MrSID DSDK releases with incompatible    */
/* APIs. We will try to support all of them. Below is a code for DSDK   */
/* v. 3.1.x and 3.2.x.                                                  */
/************************************************************************/

#if defined(MRSID_DSDK_VERSION_32) || defined(MRSID_DSDK_VERSION_31)

#ifdef MRSID_DSDK_VERSION_31
# include "lt_colorSpace.h"
#endif

#include "lt_fileUtils.h"
#include "lt_xTrans.h"
#include "lt_imageBufferInfo.h"
#include "lt_imageBuffer.h"
#include "lt_pixel.h"

#include "MrSIDImageFile.h"
#include "MrSIDNavigator.h"
#include "MetadataReader.h"
#include "MetadataElement.h"

LT_USE_NAMESPACE(LizardTech)

/************************************************************************/
/* ==================================================================== */
/*                              MrSIDDataset                            */
/* ==================================================================== */
/************************************************************************/

class MrSIDDataset : public GDALDataset
{
    friend class MrSIDRasterBand;

    MrSIDImageFile      *poMrSidFile;
    MrSIDNavigator      *poMrSidNav;
    Pixel               *poDefaultPixel;
    MetadataReader      *poMrSidMetadata;

    ImageBufferInfo::SampleType eSampleType;
    GDALDataType        eDataType;
#ifdef MRSID_DSDK_VERSION_31
    ColorSpace          *poColorSpace;
#else
    MrSIDColorSpace     *poColorSpace;
#endif

    int                 nCurrentZoomLevel;

    int                 bHasGeoTransform;
    double              adfGeoTransform[6];
    char                *pszProjection;
    GTIFDefn            *psDefn;

    int                 bIsOverview;
    int		        nOverviewCount;
    MrSIDDataset        **papoOverviewDS;

    CPLErr              OpenZoomLevel( int iZoom );
    char                *SerializeMetadataElement( const MetadataElement * );
    int                 GetMetadataElement( const char *, void *, int );
    void                FetchProjParms();
    void                GetGTIFDefn();
    char                *GetOGISDefn( GTIFDefn * );



  public:
                MrSIDDataset();
                ~MrSIDDataset();

    static GDALDataset  *Open( GDALOpenInfo * );

    virtual CPLErr      GetGeoTransform( double * padfTransform );
    const char          *GetProjectionRef();
};

#include "mrsidcomon.h"

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

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int );

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
    nBlockYSize = ( nBlockXSize * poDS->GetRasterYSize() < 1000000 )?
        poDS->GetRasterYSize():1000000/nBlockXSize + 1;
    nBlockSize = nBlockXSize * nBlockYSize;
    poDS->poMrSidNav->zoomTo( poDS->nCurrentZoomLevel ); 
    poDS->poMrSidNav->resize( nBlockXSize, nBlockYSize, IntRect::TOP_LEFT );

    poImageBufInfo = new ImageBufferInfo( ImageBufferInfo::BIP,
                                          *poDS->poColorSpace,
                                          poDS->eSampleType );
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
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr MrSIDRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                   int nXOff, int nYOff, int nXSize, int nYSize,
                                   void * pData, int nBufXSize, int nBufYSize,
                                   GDALDataType eBufType,
                                   int nPixelSpace, int nLineSpace )
    
{
    MrSIDDataset *poGDS = (MrSIDDataset *) poDS;

/* -------------------------------------------------------------------- */
/*      Fallback to default implementation if the whole scanline        */
/*      without subsampling requested.                                  */
/* -------------------------------------------------------------------- */
    if ( nXSize == poGDS->GetRasterXSize()
         && nXSize == nBufXSize
         && nYSize == nBufYSize )
    {
        return GDALRasterBand::IRasterIO( eRWFlag, nXOff, nYOff,
                                          nXSize, nYSize, pData,
                                          nBufXSize, nBufYSize, eBufType,
                                          nPixelSpace, nLineSpace );
    }

/* -------------------------------------------------------------------- */
/*      Use MrSID zooming/panning abilities otherwise.                  */
/* -------------------------------------------------------------------- */
    int         nBufDataSize = GDALGetDataTypeSize( eBufType ) / 8;
    int         iLine;
    int         nNewXSize, nNewYSize, iSrcOffset;

    ImgRect     imageSupport( nXOff, nYOff, nXOff + nXSize, nYOff + nYSize );
    IntDimension targetDims( nBufXSize, nBufYSize );

    /* Again, fallback to default if we can't zoom/pan */
    if ( !poGDS->poMrSidNav->fitWithin( imageSupport, targetDims ) )
    {
        return GDALRasterBand::IRasterIO( eRWFlag, nXOff, nYOff,
                                          nXSize, nYSize, pData,
                                          nBufXSize, nBufYSize, eBufType,
                                          nPixelSpace, nLineSpace );
    }

    poImageBuf = new ImageBuffer( *poImageBufInfo );
    try
    {
        poGDS->poMrSidNav->loadImage( *poImageBuf );
    }
    catch ( ... )
    {
        delete poImageBuf;
        CPLError( CE_Failure, CPLE_AppDefined,
                  "MrSIDRasterBand::IRasterIO(): Failed to load image." );
        return CE_Failure;
    }

    nNewXSize = poImageBuf->getBounds().width();
    nNewYSize = poImageBuf->getBounds().height();
    iSrcOffset = (nBand - 1) * poImageBufInfo->bytesPerSample();

    for ( iLine = 0; iLine < nBufYSize; iLine++ )
    {
        int     iDstLineOff = iLine * nLineSpace;

        if ( nNewXSize == nBufXSize && nNewYSize == nBufYSize )
        {
            GDALCopyWords( (GByte *)poImageBuf->getData()
                           + iSrcOffset + iLine * poImageBuf->getRowBytes(),
                           eDataType, poImageBufInfo->pixelIncrement(),
                           (GByte *)pData + iDstLineOff, eBufType, nPixelSpace,
                           nBufXSize );
        }
        else
        {
            double  dfSrcXInc = (double)nNewXSize / nBufXSize;
            double  dfSrcYInc = (double)nNewYSize / nBufYSize;

            int     iSrcLineOff = iSrcOffset
                + (int)(iLine * dfSrcYInc) * poImageBuf->getRowBytes();
            int     iPixel;

            for ( iPixel = 0; iPixel < nBufXSize; iPixel++ )
            {
                GDALCopyWords( (GByte *)poImageBuf->getData() + iSrcLineOff
                               + (int)(iPixel * dfSrcXInc)
                                      * poImageBufInfo->pixelIncrement(),
                               eDataType, poImageBufInfo->pixelIncrement(),
                               (GByte *)pData + iDstLineOff +
                               iPixel * nBufDataSize,
                               eBufType, nPixelSpace, 1 );
            }
        }
    }

    delete poImageBuf;

    return CE_None;
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
    poGDS->poMrSidNav->panTo( nBlockXOff * nBlockXSize,
                              nBlockYOff * nBlockYSize,
                              IntRect::TOP_LEFT );

    try
    {
        poGDS->poMrSidNav->loadImage( *poImageBuf );
    }
    catch ( ... )
    {
        delete poImageBuf;
        CPLError( CE_Failure, CPLE_AppDefined,
                  "MrSIDRasterBand::IReadBlock(): Failed to load image." );
        return CE_Failure;
    }

    for ( i = 0, j = nBand - 1; i < nBlockSize; i++, j+=poGDS->nBands )
    {
        switch( eDataType )
        {
            case GDT_UInt16:
                ((GUInt16*)pImage)[i] = ((GUInt16*)poImageBuf->getData())[j];
            	break;
            case GDT_Float32:
                ((float*)pImage)[i] = ((float*)poImageBuf->getData())[j];
            	break;
            case GDT_Byte:
            default:
                ((GByte*)pImage)[i] = ((GByte*)poImageBuf->getData())[j];
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

#ifdef MRSID_DSDK_VERSION_31

# define GDAL_MRSID_COLOR_SCHEME poGDS->poColorSpace->scheme()
# define GDAL_MRSID_COLORSPACE_RGB ColorSpace::RGB
# define GDAL_MRSID_COLORSPACE_CMYK ColorSpace::CMYK
# define GDAL_MRSID_COLORSPACE_GRAYSCALE ColorSpace::GRAYSCALE
# define GDAL_MRSID_COLORSPACE_RGBK ColorSpace::RGBK

#else

# define GDAL_MRSID_COLOR_SCHEME (int)poGDS->poColorSpace
# define GDAL_MRSID_COLORSPACE_RGB MRSID_COLORSPACE_RGB
# define GDAL_MRSID_COLORSPACE_CMYK MRSID_COLORSPACE_CMYK
# define GDAL_MRSID_COLORSPACE_GRAYSCALE MRSID_COLORSPACE_GRAYSCALE
# define GDAL_MRSID_COLORSPACE_RGBK MRSID_COLORSPACE_RGBK

#endif

    switch( GDAL_MRSID_COLOR_SCHEME )
    {
        case GDAL_MRSID_COLORSPACE_RGB:
            if( nBand == 1 )
                return GCI_RedBand;
            else if( nBand == 2 )
                return GCI_GreenBand;
            else if( nBand == 3 )
                return GCI_BlueBand;
            else
                return GCI_Undefined;
        case GDAL_MRSID_COLORSPACE_CMYK:
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
        case GDAL_MRSID_COLORSPACE_GRAYSCALE:
            return GCI_GrayIndex;
        case GDAL_MRSID_COLORSPACE_RGBK:
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

#undef GDAL_MRSID_COLORSPACE_RGB
#undef GDAL_MRSID_COLORSPACE_CMYK
#undef GDAL_MRSID_COLORSPACE_GRAYSCALE
#undef GDAL_MRSID_COLORSPACE_RGBK

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
    poMrSidFile = NULL;
    poMrSidNav = NULL;
    poDefaultPixel = NULL;
    poMrSidMetadata = NULL;
    poColorSpace = NULL;
    nBands = 0;
    eSampleType = ImageBufferInfo::UINT8;
    eDataType = GDT_Byte;
    pszProjection = CPLStrdup( "" );
    bHasGeoTransform = FALSE;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
    psDefn = NULL;
    nCurrentZoomLevel = 0;
    bIsOverview = FALSE;
    nOverviewCount = 0;
    papoOverviewDS = NULL;

}

/************************************************************************/
/*                            ~MrSIDDataset()                           */
/************************************************************************/

MrSIDDataset::~MrSIDDataset()
{
    // Delete MrSID file object only in base dataset object and don't delete
    // it in overviews.
    if ( poMrSidFile && !bIsOverview )
        delete poMrSidFile;
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
    if ( psDefn )
        delete psDefn;
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

    if ( !bHasGeoTransform )
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

char *MrSIDDataset::SerializeMetadataElement( const MetadataElement *poElement )
{
#ifdef MRSID_DSDK_VERSION_31
    IntDimension oDim = poElement->getDimensions();
#else
    IntDimension oDim( poElement->getDimensionWidth(),
                       poElement->getDimensionHeight() );
#endif
    int         i, j, iLength;
    const char  *pszTemp = NULL;
    char        *pszMetadata = CPLStrdup( "" );

    for ( i = 0; i < oDim.height; i++ )
        for ( j = 0; j < oDim.width; j++ )
        {
            switch( poElement->type() )
            {
                case MetadataValue::BYTE:
                case MetadataValue::SHORT:
                case MetadataValue::LONG:
                    pszTemp = CPLSPrintf( "%lu", (unsigned long)(*poElement)[i][j] );
                    break;
                case MetadataValue::SBYTE:
                case MetadataValue::SSHORT:
                case MetadataValue::SLONG:
                    pszTemp = CPLSPrintf( "%ld", (long)(*poElement)[i][j] );
                    break;
                case MetadataValue::FLOAT:
                    pszTemp = CPLSPrintf( "%f", (float)(*poElement)[i][j] );
                    break;
                case MetadataValue::DOUBLE:
                    pszTemp = CPLSPrintf( "%lf", (double)(*poElement)[i][j] );
                    break;
                case MetadataValue::ASCII:
                    pszTemp = (const char*)poElement->getMetadataValue();
                    break;
                default:
                    pszTemp = "";
                    break;
            }

            iLength = strlen(pszMetadata) + strlen(pszTemp) + 2;

            pszMetadata = (char *)CPLRealloc( pszMetadata, iLength );
            if ( !EQUAL( pszMetadata, "" ) )
                strncat( pszMetadata, ",", 1 );
            strncat( pszMetadata, pszTemp, iLength );
        }

    return pszMetadata;
}

/************************************************************************/
/*                          GetMetadataElement()                        */
/************************************************************************/

int MrSIDDataset::GetMetadataElement( const char *pszKey, void *pValue,
                                      int iSize = 0 )
{
    if ( !poMrSidMetadata->keyExists( pszKey ) )
        return FALSE;

    MetadataElement oElement( poMrSidMetadata->getValue( pszKey ) );

    /* XXX: return FALSE if we have more than one element in metadata record */
    if ( oElement.getDimensionality() != MetadataElement::SINGLE_VALUE )
        return FALSE;

    switch( oElement.type() )
    {
        case MetadataValue::BYTE:
        {
            unsigned char iValue = oElement[0][0];
            memcpy( pValue, &iValue, sizeof( iValue ) );
        }
        break;
        case MetadataValue::SHORT:
        {
            unsigned short iValue = oElement[0][0];
            memcpy( pValue, &iValue, sizeof( iValue ) );
        }
        break;
        case MetadataValue::LONG:
        {
            unsigned long iValue = oElement[0][0];
            memcpy( pValue, &iValue, sizeof( iValue ) );
        }
        break;
        case MetadataValue::SBYTE:
        {
            char iValue = oElement[0][0];
            memcpy( pValue, &iValue, sizeof( iValue ) );
        }
        break;
        case MetadataValue::SSHORT:
        {
            signed short iValue = oElement[0][0];
            memcpy( pValue, &iValue, sizeof( iValue ) );
        }
        break;
        case MetadataValue::SLONG:
        {
            signed long iValue = oElement[0][0];
            memcpy( pValue, &iValue, sizeof( iValue ) );
        }
        break;
        case MetadataValue::FLOAT:
        {
            float fValue = oElement[0][0];
            memcpy( pValue, &fValue, sizeof( fValue ) );
        }
        break;
        case MetadataValue::DOUBLE:
        {
            double dfValue = oElement[0][0];
            memcpy( pValue, &dfValue, sizeof( dfValue ) );
        }
        break;
        case MetadataValue::ASCII:
        {
            if ( iSize )
            {
                const char *pszValue =
                    (const char *)oElement.getMetadataValue();
                strncpy( (char *)pValue, pszValue, iSize );
                ((char *)pValue)[iSize - 1] = '\0';
            }
        }
        break;
        default:
        break;
    }

    return TRUE;
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
    catch ( ... )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "MrSIDDataset::OpenZoomLevel(): "
                  "Failed to create MrSIDNavigator object." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Handle sample type and color space.                             */
/* -------------------------------------------------------------------- */
    poDefaultPixel = new Pixel( poMrSidFile->getDefaultPixelValue() );
    eSampleType = poMrSidFile->getSampleType();
#ifdef MRSID_DSDK_VERSION_31
    poColorSpace = new ColorSpace( poMrSidFile->colorSpace() );
#else
    poColorSpace = new MrSIDColorSpace( poMrSidFile->colorSpace() );
#endif
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

#ifdef MRSID_DSDK_VERSION_31
    CPLAssert( poColorSpace->samplesPerPixel () == nBands);
#endif

/* -------------------------------------------------------------------- */
/*      Take georeferencing.                                            */
/* -------------------------------------------------------------------- */
    if ( poMrSidFile->hasWorldInfo()
         && poMrSidFile->xu(adfGeoTransform[0])
         && poMrSidFile->yu(adfGeoTransform[3])
         && poMrSidFile->xres(adfGeoTransform[1])
         && poMrSidFile->yres(adfGeoTransform[5])
         && poMrSidFile->xrot(adfGeoTransform[2])
         && poMrSidFile->yrot(adfGeoTransform[4]) )
    {
        adfGeoTransform[5] = - adfGeoTransform[5];
        adfGeoTransform[0] = adfGeoTransform[0] - adfGeoTransform[1] / 2;
        adfGeoTransform[3] = adfGeoTransform[3] - adfGeoTransform[5] / 2;
        bHasGeoTransform = TRUE;
    }

    if ( iZoom != 0 )
    {
        nCurrentZoomLevel = iZoom;
        nRasterXSize =
            poMrSidFile->getDimensionsAtLevel( nCurrentZoomLevel ).width;
        nRasterYSize =
            poMrSidFile->getDimensionsAtLevel( nCurrentZoomLevel ).height;
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

    VSIFClose( poOpenInfo->fp );
    poOpenInfo->fp = NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    MrSIDDataset      *poDS;

    poDS = new MrSIDDataset();
    try
    {
#ifdef MRSID_DSDK_VERSION_31
        FileSpecification oFilename( poOpenInfo->pszFilename );
        poDS->poMrSidFile = new MrSIDImageFile( oFilename );
#else
        LTFileSpec      oFilename( poOpenInfo->pszFilename );
        poDS->poMrSidFile = new MrSIDImageFile( oFilename, (LTCallbacks *)0 );
#endif
    }
    catch ( ... )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_AppDefined,
                  "MrSIDDataset::Open(): Failed to open file %s",
                  poOpenInfo->pszFilename );
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
            char    *pszElement = poDS->SerializeMetadataElement( &(*i) );
            char    *pszKey = CPLStrdup( (*i).getKey().c_str() );
            char    *pszTemp = pszKey;

            // GDAL metadata keys should not contain ':' and '=' characters.
            // We will replace them with '_'.
            do
            {
                if ( *pszTemp == ':' || *pszTemp == '=' )
                    *pszTemp = '_';
            }
            while ( *++pszTemp );

            poDS->SetMetadataItem( pszKey, pszElement );

            CPLFree( pszElement );
            CPLFree( pszKey );
            i++;
        }
    }

    poDS->GetGTIFDefn();

/* -------------------------------------------------------------------- */
/*   Take number of resolution levels (we will use them as overviews).  */
/* -------------------------------------------------------------------- */
    poDS->nOverviewCount = poDS->poMrSidFile->nlev() - 1;

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
            poDS->papoOverviewDS[i]->bIsOverview = TRUE;
            
        }
    }

/* -------------------------------------------------------------------- */
/*      Band objects will be created in separate function.              */
/* -------------------------------------------------------------------- */
    poDS->OpenZoomLevel( 0 );

    CPLDebug( "MrSID",
              "Opened image: width %d, height %d, bands %d, overviews %d",
              poDS->nRasterXSize, poDS->nRasterYSize, poDS->nBands,
              poDS->nOverviewCount );

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
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_mrsid.html" );

        poDriver->pfnOpen = MrSIDDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

