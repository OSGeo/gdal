/******************************************************************************
 * $Id$
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Implementation of VRTRasterBand
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.2  2001/11/16 21:38:07  warmerda
 * try to work even if bModified
 *
 * Revision 1.1  2001/11/16 21:14:31  warmerda
 * New
 *
 */

#include "vrtdataset.h"
#include "cpl_minixml.h"

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*			    VRTSimpleSource                             */
/* ==================================================================== */
/************************************************************************/

class VRTSimpleSource 
{
public:

    virtual CPLErr  RasterIO( int nXOff, int nYOff, int nXSize, int nYSize, 
                              void *pData, int nBufXSize, int nBufYSize, 
                              GDALDataType eBufType, 
                              int nPixelSpace, int nLineSpace );

    GDALRasterBand	*poRasterBand;

    int                 nSrcXOff;
    int                 nSrcYOff;
    int                 nSrcXSize;
    int                 nSrcYSize;

    int                 nDstXOff;
    int                 nDstYOff;
    int                 nDstXSize;
    int                 nDstYSize;
};

/************************************************************************/
/*                              RasterIO()                              */
/************************************************************************/

CPLErr 
VRTSimpleSource::RasterIO( int nXOff, int nYOff, int nXSize, int nYSize,
                           void *pData, int nBufXSize, int nBufYSize, 
                           GDALDataType eBufType, 
                           int nPixelSpace, int nLineSpace )

{
    // The window we will actually request from the source raster band.
    int nReqXOff, nReqYOff, nReqXSize, nReqYSize;

    // The window we will actual set _within_ the pData buffer.
    int nOutXOff, nOutYOff, nOutXSize, nOutYSize;

/* -------------------------------------------------------------------- */
/*      Translate requested region in virtual file into the source      */
/*      band coordinates.                                               */
/* -------------------------------------------------------------------- */
    double	dfScaleX = nSrcXSize / (double) nDstXSize;
    double	dfScaleY = nSrcYSize / (double) nDstYSize;

    nReqXOff = (int) floor((nXOff - nDstXOff) * dfScaleX + nSrcXOff);
    nReqYOff = (int) floor((nYOff - nDstYOff) * dfScaleY + nSrcYOff);

    nReqXSize = (int) floor(nXSize * dfScaleX + 0.5);
    nReqYSize = (int) floor(nYSize * dfScaleY + 0.5);

/* -------------------------------------------------------------------- */
/*      Clamp within the bounds of the available source data.           */
/* -------------------------------------------------------------------- */
    int	bModified = FALSE;

    if( nReqXOff < 0 )
    {
        nReqXSize += nReqXOff;
        nReqXOff = 0;
        bModified = TRUE;
    }
    
    if( nReqYOff < 0 )
    {
        nReqYSize += nReqYOff;
        nReqYOff = 0;
        bModified = TRUE;
    }

    if( nReqXSize == 0 )
        nReqXSize = 1;
    if( nReqYSize == 0 )
        nReqYSize = 1;

    if( nReqXOff + nReqXSize > poRasterBand->GetXSize() )
    {
        nReqXSize = poRasterBand->GetXSize() - nReqXOff;
        bModified = TRUE;
    }

    if( nReqYOff + nReqYSize > poRasterBand->GetYSize() )
    {
        nReqYSize = poRasterBand->GetYSize() - nReqYOff;
        bModified = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Don't do anything if the requesting request is right of the     */
/*      source image.                                                   */
/* -------------------------------------------------------------------- */
    if( nReqXOff >= poRasterBand->GetXSize()
        || nReqYOff >= poRasterBand->GetYSize()
        || nReqXSize <= 0 || nReqYSize <= 0 )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Now transform this possibly reduced request back into the       */
/*      destination buffer coordinates in case the output region is     */
/*      less than the whole buffer.                                     */
/*                                                                      */
/*      This is kind of hard, so for now we will assume that no         */
/*      serious modification took place.                                */
/* -------------------------------------------------------------------- */
    CPLAssert( !bModified );

    nOutXOff = 0;
    nOutYOff = 0;
    nOutXSize = nBufXSize;
    nOutYSize = nBufYSize;

/* -------------------------------------------------------------------- */
/*      Actually perform the IO request.                                */
/* -------------------------------------------------------------------- */
    return 
        poRasterBand->RasterIO( GF_Read, 
                                nReqXOff, nReqYOff, nReqXSize, nReqYSize,
                                ((unsigned char *) pData) 
                                + nOutXOff * nPixelSpace
                                + nOutYOff * nLineSpace, 
                                nOutXSize, nOutYSize, 
                                eBufType, nPixelSpace, nLineSpace );
}

/************************************************************************/
/* ==================================================================== */
/*			    VRTRasterBand                               */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           VRTRasterBand()                            */
/************************************************************************/

VRTRasterBand::VRTRasterBand( GDALDataset *poDS, int nBand )

{
    Initialize( poDS->GetRasterXSize(), poDS->GetRasterYSize() );

    this->poDS = poDS;
    this->nBand = nBand;
}

/************************************************************************/
/*                           VRTRasterBand()                            */
/************************************************************************/

VRTRasterBand::VRTRasterBand( GDALDataType eType, int nXSize, int nYSize )

{
    Initialize( nXSize, nYSize );
    eDataType = eType;
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

void VRTRasterBand::Initialize( int nXSize, int nYSize )

{
    poDS = NULL;
    nBand = 0;
    eAccess = GA_ReadOnly;
    eDataType = GDT_Byte;

    nRasterXSize = nXSize;
    nRasterYSize = nYSize;
    
    nSources = 0;
    papoSources = NULL;

    nBlockXSize = MIN(128,nXSize);
    nBlockYSize = MIN(128,nYSize);
}


/************************************************************************/
/*                           ~VRTRasterBand()                           */
/************************************************************************/

VRTRasterBand::~VRTRasterBand()

{
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr VRTRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                 int nXOff, int nYOff, int nXSize, int nYSize,
                                 void * pData, int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType,
                                 int nPixelSpace, int nLineSpace )

{
    int		iSource;
    CPLErr      eErr = CE_Failure;

    /* We should initialize the buffer to some background value here */

    /* Apply each source in turn. */

    for( iSource = 0; iSource < nSources; iSource++ )
    {
        eErr = 
            papoSources[iSource]->RasterIO( nXOff, nYOff, nXSize, nYSize, 
                                            pData, nBufXSize, nBufYSize, 
                                            eBufType, nPixelSpace, nLineSpace );
    }
    
    return eErr;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr VRTRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                   void * pImage )

{
    return IRasterIO( GF_Read, 0, nBlockYOff, nBlockXSize, 1, 
                      pImage, nBlockXSize, 1, eDataType, 
                      GDALGetDataTypeSize(eDataType)/8, 0 );
}

/************************************************************************/
/*                          AddSimpleSource()                           */
/************************************************************************/

CPLErr VRTRasterBand::AddSimpleSource( GDALRasterBand *poSrcBand, 
                                       int nSrcXOff, int nSrcYOff, 
                                       int nSrcXSize, int nSrcYSize, 
                                       int nDstXOff, int nDstYOff, 
                                       int nDstXSize, int nDstYSize )

{
/* -------------------------------------------------------------------- */
/*      Default source and dest rectangles.                             */
/* -------------------------------------------------------------------- */
    if( nSrcYSize == -1 )
    {
        nSrcXOff = 0;
        nSrcYOff = 0;
        nSrcXSize = poSrcBand->GetXSize();
        nSrcYSize = poSrcBand->GetYSize();
    }

    if( nDstYSize == -1 )
    {
        nDstXOff = 0;
        nDstYOff = 0;
        nDstXSize = nRasterXSize;
        nDstYSize = nRasterYSize;
    }

/* -------------------------------------------------------------------- */
/*      Create source.                                                  */
/* -------------------------------------------------------------------- */
    VRTSimpleSource *poSimpleSource = new VRTSimpleSource();

    poSimpleSource->poRasterBand = poSrcBand;

    poSimpleSource->nSrcXOff  = nSrcXOff;
    poSimpleSource->nSrcYOff  = nSrcYOff;
    poSimpleSource->nSrcXSize = nSrcXSize;
    poSimpleSource->nSrcYSize = nSrcYSize;

    poSimpleSource->nDstXOff  = nDstXOff;
    poSimpleSource->nDstYOff  = nDstYOff;
    poSimpleSource->nDstXSize = nDstXSize;
    poSimpleSource->nDstYSize = nDstYSize;

/* -------------------------------------------------------------------- */
/*      add to list.                                                    */
/* -------------------------------------------------------------------- */
    nSources++;

    papoSources = (VRTSimpleSource **) 
        CPLRealloc(papoSources, sizeof(void*) * nSources);
    papoSources[nSources-1] = poSimpleSource;

    return CE_None;
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr VRTRasterBand::XMLInit( CPLXMLNode * psTree )

{
/* -------------------------------------------------------------------- */
/*      Validate a bit.                                                 */
/* -------------------------------------------------------------------- */
    if( psTree == NULL || psTree->eType != CXT_Element
        || !EQUAL(psTree->pszValue,"VRTRasterBand") )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Invalid node passed to VRTRasterBand::XMLInit()." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Set the band if provided as an attribute.                       */
/* -------------------------------------------------------------------- */
    if( CPLGetXMLValue( psTree, "band", NULL) != NULL )
        nBand = atoi(CPLGetXMLValue( psTree, "band", "0"));

/* -------------------------------------------------------------------- */
/*      Process SimpleSources.                                          */
/* -------------------------------------------------------------------- */
    CPLXMLNode	*psChild;

    for( psChild = psTree->psChild; psChild != NULL; psChild = psChild->psNext)
    {
        if( !EQUAL(psChild->pszValue,"SimpleSource") 
            || psChild->eType != CXT_Element )
            continue;

        const char *pszFilename = 
            CPLGetXMLValue(psChild, "SourceFilename", NULL);

        if( pszFilename == NULL )
        {
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "Missing <SourceFilename> element in VRTRasterBand." );
            continue;
        }

        int nSrcBand = atoi(CPLGetXMLValue(psChild,"SourceBand","1"));

        GDALDataset *poSrcDS = (GDALDataset *) 
            GDALOpen( pszFilename, GA_ReadOnly );

        if( poDS == NULL )
            continue;

        if( poSrcDS->GetRasterBand(nSrcBand) == NULL )
        {
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "Unable to fetch SourceBand %d from %s.",
                      nSrcBand, pszFilename );
            continue;
        }

        AddSimpleSource(poSrcDS->GetRasterBand(nSrcBand),
                        atoi(CPLGetXMLValue(psChild,"SrcRect.xOff","-1")),
                        atoi(CPLGetXMLValue(psChild,"SrcRect.yOff","-1")),
                        atoi(CPLGetXMLValue(psChild,"SrcRect.xSize","-1")),
                        atoi(CPLGetXMLValue(psChild,"SrcRect.ySize","-1")),
                        atoi(CPLGetXMLValue(psChild,"DstRect.xOff","-1")),
                        atoi(CPLGetXMLValue(psChild,"DstRect.yOff","-1")),
                        atoi(CPLGetXMLValue(psChild,"DstRect.xSize","-1")),
                        atoi(CPLGetXMLValue(psChild,"DstRect.ySize","-1")));
    }

/* -------------------------------------------------------------------- */
/*      Done.                                                           */
/* -------------------------------------------------------------------- */
    if( nSources > 0 )
        return CE_None;
    else
        return CE_Failure;
}
