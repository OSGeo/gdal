/******************************************************************************
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Implementation of a sourced raster band that derives its raster
 *           by applying an algorithm (GDALDerivedPixelFunc) to the sources.
 * Author:   Pete Nagy
 *
 ******************************************************************************
 * Copyright (c) 2005 Vexcel Corp.
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
 *****************************************************************************/

#include "vrtdataset.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include <map>

static std::map<CPLString, GDALDerivedPixelFunc> osMapPixelFunction;

/************************************************************************/
/* ==================================================================== */
/*                          VRTDerivedRasterBand                        */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                        VRTDerivedRasterBand()                        */
/************************************************************************/

VRTDerivedRasterBand::VRTDerivedRasterBand(GDALDataset *poDS, int nBand)
    : VRTSourcedRasterBand( poDS, nBand )

{
    this->pszFuncName = NULL;
    this->eSourceTransferType = GDT_Unknown;
}

/************************************************************************/
/*                        VRTDerivedRasterBand()                        */
/************************************************************************/

VRTDerivedRasterBand::VRTDerivedRasterBand(GDALDataset *poDS, int nBand,
					   GDALDataType eType, 
					   int nXSize, int nYSize)
    : VRTSourcedRasterBand(poDS, nBand, eType, nXSize, nYSize)

{
    this->pszFuncName = NULL;
    this->eSourceTransferType = GDT_Unknown;
}

/************************************************************************/
/*                       ~VRTDerivedRasterBand()                        */
/************************************************************************/

VRTDerivedRasterBand::~VRTDerivedRasterBand()

{
    if (this->pszFuncName != NULL) {
        CPLFree(this->pszFuncName);
        this->pszFuncName = NULL;
    }
}

/************************************************************************/
/*                           AddPixelFunction()                         */
/************************************************************************/

/**
 * This adds a pixel function to the global list of available pixel
 * functions for derived bands.  Pixel functions must be registered
 * in this way before a derived band tries to access data.
 *
 * Derived bands are stored with only the name of the pixel function 
 * that it will apply, and if a pixel function matching the name is not
 * found the IRasterIO() call will do nothing.
 *
 * @param pszFuncName Name used to access pixel function
 * @param pfnNewFunction Pixel function associated with name.  An
 *  existing pixel function registered with the same name will be
 *  replaced with the new one.
 *
 * @return CE_None, invalid (NULL) parameters are currently ignored.
 */
CPLErr CPL_STDCALL GDALAddDerivedBandPixelFunc
(const char *pszFuncName, GDALDerivedPixelFunc pfnNewFunction)
{
    /* ---- Init ---- */
    if ((pszFuncName == NULL) || (pszFuncName[0] == '\0') ||
        (pfnNewFunction == NULL))
    {
      return CE_None;
    }

    osMapPixelFunction[pszFuncName] = pfnNewFunction;

    return CE_None;
}
/**
 * This adds a pixel function to the global list of available pixel
 * functions for derived bands.
 *
 * This is the same as the c function GDALAddDerivedBandPixelFunc()
 *
 * @param pszFuncName Name used to access pixel function
 * @param pfnNewFunction Pixel function associated with name.  An
 *  existing pixel function registered with the same name will be
 *  replaced with the new one.
 *
 * @return CE_None, invalid (NULL) parameters are currently ignored.
 */
CPLErr VRTDerivedRasterBand::AddPixelFunction
(const char *pszFuncName, GDALDerivedPixelFunc pfnNewFunction)
{
    return GDALAddDerivedBandPixelFunc(pszFuncName, pfnNewFunction);
}

/************************************************************************/
/*                           GetPixelFunction()                         */
/************************************************************************/

/**
 * Get a pixel function previously registered using the global
 * AddPixelFunction.
 *
 * @param pszFuncName The name associated with the pixel function.
 *
 * @return A derived band pixel function, or NULL if none have been
 * registered for pszFuncName.
 */
GDALDerivedPixelFunc VRTDerivedRasterBand::GetPixelFunction
(const char *pszFuncName)
{
    /* ---- Init ---- */
    if ((pszFuncName == NULL) || (pszFuncName[0] == '\0'))
    {
        return NULL;
    }

    std::map<CPLString, GDALDerivedPixelFunc>::iterator oIter =
        osMapPixelFunction.find(pszFuncName);

    if( oIter == osMapPixelFunction.end())
        return NULL;

    return oIter->second;
}

/************************************************************************/
/*                         SetPixelFunctionName()                       */
/************************************************************************/

/**
 * Set the pixel function name to be applied to this derived band.  The
 * name should match a pixel function registered using AddPixelFunction.
 *
 * @param pszFuncName Name of pixel function to be applied to this derived
 * band.
 */
void VRTDerivedRasterBand::SetPixelFunctionName(const char *pszFuncName)
{
    this->pszFuncName = CPLStrdup( pszFuncName );
}

/************************************************************************/
/*                         SetSourceTransferType()                      */
/************************************************************************/

/**
 * Set the transfer type to be used to obtain pixel information from
 * all of the sources.  If unset, the transfer type used will be the
 * same as the derived band data type.  This makes it possible, for
 * example, to pass CFloat32 source pixels to the pixel function, even
 * if the pixel function generates a raster for a derived band that
 * is of type Byte.
 *
 * @param eDataType Data type to use to obtain pixel information from
 * the sources to be passed to the derived band pixel function.
 */
void VRTDerivedRasterBand::SetSourceTransferType(GDALDataType eDataType)
{
    this->eSourceTransferType = eDataType;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

/**
 * Read/write a region of image data for this band.
 *
 * Each of the sources for this derived band will be read and passed to
 * the derived band pixel function.  The pixel function is responsible
 * for applying whatever algorithm is necessary to generate this band's
 * pixels from the sources.
 *
 * The sources will be read using the transfer type specified for sources
 * using SetSourceTransferType().  If no transfer type has been set for
 * this derived band, the band's data type will be used as the transfer type.
 *
 * @see gdalrasterband
 *
 * @param eRWFlag Either GF_Read to read a region of data, or GT_Write to
 * write a region of data.
 *
 * @param nXOff The pixel offset to the top left corner of the region
 * of the band to be accessed.  This would be zero to start from the left side.
 *
 * @param nYOff The line offset to the top left corner of the region
 * of the band to be accessed.  This would be zero to start from the top.
 *
 * @param nXSize The width of the region of the band to be accessed in pixels.
 *
 * @param nYSize The height of the region of the band to be accessed in lines.
 *
 * @param pData The buffer into which the data should be read, or from which
 * it should be written.  This buffer must contain at least nBufXSize *
 * nBufYSize words of type eBufType.  It is organized in left to right,
 * top to bottom pixel order.  Spacing is controlled by the nPixelSpace,
 * and nLineSpace parameters.
 *
 * @param nBufXSize The width of the buffer image into which the desired
 * region is to be read, or from which it is to be written.
 *
 * @param nBufYSize The height of the buffer image into which the desired
 * region is to be read, or from which it is to be written.
 *
 * @param eBufType The type of the pixel values in the pData data buffer.  The
 * pixel values will automatically be translated to/from the GDALRasterBand
 * data type as needed.
 *
 * @param nPixelSpace The byte offset from the start of one pixel value in
 * pData to the start of the next pixel value within a scanline.  If defaulted
 * (0) the size of the datatype eBufType is used.
 *
 * @param nLineSpace The byte offset from the start of one scanline in
 * pData to the start of the next.  If defaulted the size of the datatype
 * eBufType * nBufXSize is used.
 *
 * @return CE_Failure if the access fails, otherwise CE_None.
 */
CPLErr VRTDerivedRasterBand::IRasterIO(GDALRWFlag eRWFlag,
				       int nXOff, int nYOff, int nXSize, 
				       int nYSize, void * pData, int nBufXSize,
				       int nBufYSize, GDALDataType eBufType,
				       int nPixelSpace, int nLineSpace )
{
    GDALDerivedPixelFunc pfnPixelFunc;
    void **pBuffers;
    CPLErr eErr = CE_None;
    int iSource, ii, typesize, sourcesize;
    GDALDataType eSrcType;

    if( eRWFlag == GF_Write )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Writing through VRTSourcedRasterBand is not supported." );
        return CE_Failure;
    }

    typesize = GDALGetDataTypeSize(eBufType) / 8;
    if (GDALGetDataTypeSize(eBufType) % 8 > 0) typesize++;
    eSrcType = this->eSourceTransferType;
    if ((eSrcType == GDT_Unknown) || (eSrcType >= GDT_TypeCount)) {
	eSrcType = eBufType;
    }
    sourcesize = GDALGetDataTypeSize(eSrcType) / 8;

/* -------------------------------------------------------------------- */
/*      Initialize the buffer to some background value. Use the         */
/*      nodata value if available.                                      */
/* -------------------------------------------------------------------- */
    if ( nPixelSpace == typesize &&
         (!bNoDataValueSet || dfNoDataValue == 0) ) {
        memset( pData, 0, nBufXSize * nBufYSize * nPixelSpace );
    }
    else if ( !bEqualAreas || bNoDataValueSet )
    {
        double dfWriteValue = 0.0;
        int    iLine;

        if( bNoDataValueSet )
            dfWriteValue = dfNoDataValue;

        for( iLine = 0; iLine < nBufYSize; iLine++ )
        {
            GDALCopyWords( &dfWriteValue, GDT_Float64, 0, 
                           ((GByte *)pData) + nLineSpace * iLine, 
                           eBufType, nPixelSpace, nBufXSize );
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we have overviews that would be appropriate to satisfy       */
/*      this request?                                                   */
/* -------------------------------------------------------------------- */
    if( (nBufXSize < nXSize || nBufYSize < nYSize)
        && GetOverviewCount() > 0 )
    {
        if( OverviewRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize, 
                              pData, nBufXSize, nBufYSize, 
                              eBufType, nPixelSpace, nLineSpace ) == CE_None )
            return CE_None;
    }

    /* ---- Get pixel function for band ---- */
    pfnPixelFunc = VRTDerivedRasterBand::GetPixelFunction(this->pszFuncName);
    if (pfnPixelFunc == NULL) {
	CPLError( CE_Failure, CPLE_IllegalArg, 
		  "VRTDerivedRasterBand::IRasterIO:" \
		  "Derived band pixel function '%s' not registered.\n",
		  this->pszFuncName);
	return CE_Failure;
    }

    /* TODO: It would be nice to use a MallocBlock function for each
       individual buffer that would recycle blocks of memory from a
       cache by reassigning blocks that are nearly the same size.
       A corresponding FreeBlock might only truly free if the total size
       of freed blocks gets to be too great of a percentage of the size
       of the allocated blocks. */

    /* ---- Get buffers for each source ---- */
    pBuffers = (void **) CPLMalloc(sizeof(void *) * nSources);
    for (iSource = 0; iSource < nSources; iSource++) {
        pBuffers[iSource] = (void *) 
            VSIMalloc(sourcesize * nBufXSize * nBufYSize);
        if (pBuffers[iSource] == NULL)
        {
            for (ii = 0; ii < iSource; ii++) {
                VSIFree(pBuffers[iSource]);
            }
            CPLError( CE_Failure, CPLE_OutOfMemory,
                "VRTDerivedRasterBand::IRasterIO:" \
                "Out of memory allocating %d bytes.\n",
                nPixelSpace * nBufXSize * nBufYSize);
            return CE_Failure;
        }

        /* ------------------------------------------------------------ */
        /* #4045: Initialize the newly allocated buffers before handing */
        /* them off to the sources. These buffers are packed, so we     */
        /* don't need any special line-by-line handling when a nonzero  */
        /* nodata value is set.                                         */
        /* ------------------------------------------------------------ */
        if ( !bNoDataValueSet || dfNoDataValue == 0 )
        {
            memset( pBuffers[iSource], 0, sourcesize * nBufXSize * nBufYSize );
        }
        else
        {
            GDALCopyWords( &dfNoDataValue, GDT_Float64, 0,
                           (GByte *) pBuffers[iSource], eSrcType, sourcesize,
                           nBufXSize * nBufYSize);
        }
    }

    /* ---- Load values for sources into packed buffers ---- */
    for(iSource = 0; iSource < nSources; iSource++) {
        eErr = ((VRTSource *)papoSources[iSource])->RasterIO
	    (nXOff, nYOff, nXSize, nYSize, 
	     pBuffers[iSource], nBufXSize, nBufYSize, 
	     eSrcType, GDALGetDataTypeSize( eSrcType ) / 8,
             (GDALGetDataTypeSize( eSrcType ) / 8) * nBufXSize);
    }

    /* ---- Apply pixel function ---- */
    if (eErr == CE_None) {
	eErr = pfnPixelFunc((void **)pBuffers, nSources,
			    pData, nBufXSize, nBufYSize,
			    eSrcType, eBufType, nPixelSpace, nLineSpace);
    }

    /* ---- Release buffers ---- */
    for (iSource = 0; iSource < nSources; iSource++) {
        VSIFree(pBuffers[iSource]);
    }
    CPLFree(pBuffers);

    return eErr;
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr VRTDerivedRasterBand::XMLInit(CPLXMLNode *psTree, 
				     const char *pszVRTPath)

{
    CPLErr eErr;
    const char *pszTypeName;

    eErr = VRTSourcedRasterBand::XMLInit( psTree, pszVRTPath );
    if( eErr != CE_None )
        return eErr;

    /* ---- Read derived pixel function type ---- */
    this->SetPixelFunctionName
	(CPLGetXMLValue(psTree, "PixelFunctionType", NULL));

    /* ---- Read optional source transfer data type ---- */
    pszTypeName = CPLGetXMLValue(psTree, "SourceTransferType", NULL);
    if (pszTypeName != NULL) {
	this->eSourceTransferType = GDALGetDataTypeByName(pszTypeName);
    }

    return CE_None;
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTDerivedRasterBand::SerializeToXML(const char *pszVRTPath)
{
    CPLXMLNode *psTree;

    psTree = VRTSourcedRasterBand::SerializeToXML( pszVRTPath );

/* -------------------------------------------------------------------- */
/*      Set subclass.                                                   */
/* -------------------------------------------------------------------- */
    CPLCreateXMLNode( 
        CPLCreateXMLNode( psTree, CXT_Attribute, "subClass" ), 
        CXT_Text, "VRTDerivedRasterBand" );

    /* ---- Encode DerivedBand-specific fields ---- */
    if( pszFuncName != NULL && strlen(pszFuncName) > 0 )
        CPLSetXMLValue(psTree, "PixelFunctionType", this->pszFuncName);
    if( this->eSourceTransferType != GDT_Unknown)
        CPLSetXMLValue(psTree, "SourceTransferType", 
		       GDALGetDataTypeName(this->eSourceTransferType));

    return psTree;
}

