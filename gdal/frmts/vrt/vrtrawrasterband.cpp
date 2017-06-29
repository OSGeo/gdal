/******************************************************************************
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Implementation of VRTRawRasterBand
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_port.h"
#include "rawdataset.h"
#include "vrtdataset.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_hash_set.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"

CPL_CVSID("$Id$")

/*! @cond Doxygen_Suppress */

/************************************************************************/
/* ==================================================================== */
/*                          VRTRawRasterBand                            */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                          VRTRawRasterBand()                          */
/************************************************************************/

VRTRawRasterBand::VRTRawRasterBand( GDALDataset *poDSIn, int nBandIn,
                                    GDALDataType eType ) :
    m_poRawRaster(NULL),
    m_pszSourceFilename(NULL),
    m_bRelativeToVRT(FALSE)
{
    Initialize( poDSIn->GetRasterXSize(), poDSIn->GetRasterYSize() );

    // Declared in GDALRasterBand.
    poDS = poDSIn;
    nBand = nBandIn;

    if( eType != GDT_Unknown )
        eDataType = eType;
}

/************************************************************************/
/*                         ~VRTRawRasterBand()                          */
/************************************************************************/

VRTRawRasterBand::~VRTRawRasterBand()

{
    FlushCache();
    ClearRawLink();
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr VRTRawRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                    int nXOff, int nYOff,
                                    int nXSize, int nYSize,
                                    void * pData, int nBufXSize, int nBufYSize,
                                    GDALDataType eBufType,
                                    GSpacing nPixelSpace,
                                    GSpacing nLineSpace,
                                    GDALRasterIOExtraArg* psExtraArg )
{
    if( m_poRawRaster == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "No raw raster band configured on VRTRawRasterBand." );
        return CE_Failure;
    }

    if( eRWFlag == GF_Write && eAccess == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Attempt to write to read only dataset in"
                  "VRTRawRasterBand::IRasterIO()." );

        return CE_Failure;
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
                              eBufType, nPixelSpace,
                              nLineSpace, psExtraArg ) == CE_None )
            return CE_None;
    }

    m_poRawRaster->SetAccess(eAccess);

    return m_poRawRaster->RasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                    pData, nBufXSize, nBufYSize,
                                    eBufType, nPixelSpace,
                                    nLineSpace, psExtraArg );
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr VRTRawRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                     void * pImage )

{
    if( m_poRawRaster == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "No raw raster band configured on VRTRawRasterBand." );
        return CE_Failure;
    }

    return m_poRawRaster->ReadBlock( nBlockXOff, nBlockYOff, pImage );
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr VRTRawRasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                      void * pImage )

{
    if( m_poRawRaster == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "No raw raster band configured on VRTRawRasterBand." );
        return CE_Failure;
    }

    m_poRawRaster->SetAccess(eAccess);

    return m_poRawRaster->WriteBlock( nBlockXOff, nBlockYOff, pImage );
}

/************************************************************************/
/*                             SetRawLink()                             */
/************************************************************************/

CPLErr VRTRawRasterBand::SetRawLink( const char *pszFilename,
                                     const char *pszVRTPath,
                                     int bRelativeToVRTIn,
                                     vsi_l_offset nImageOffset,
                                     int nPixelOffset, int nLineOffset,
                                     const char *pszByteOrder )

{
    ClearRawLink();

    reinterpret_cast<VRTDataset *>( poDS )->SetNeedsFlush();

/* -------------------------------------------------------------------- */
/*      Prepare filename.                                               */
/* -------------------------------------------------------------------- */
    if( pszFilename == NULL )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Missing <SourceFilename> element in VRTRasterBand." );
        return CE_Failure;
    }

    char *pszExpandedFilename = NULL;
    if( pszVRTPath != NULL && bRelativeToVRTIn )
    {
        pszExpandedFilename = CPLStrdup(
            CPLProjectRelativeFilename( pszVRTPath, pszFilename ) );
    }
    else
    {
        pszExpandedFilename = CPLStrdup( pszFilename );
    }

/* -------------------------------------------------------------------- */
/*      Try and open the file.  We always use the large file API.       */
/* -------------------------------------------------------------------- */
    FILE *fp = CPLOpenShared( pszExpandedFilename, "rb+", TRUE );

    if( fp == NULL )
        fp = CPLOpenShared( pszExpandedFilename, "rb", TRUE );

    if( fp == NULL
        && reinterpret_cast<VRTDataset *>( poDS )->GetAccess() == GA_Update )
    {
        fp = CPLOpenShared( pszExpandedFilename, "wb+", TRUE );
    }

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to open %s.%s",
                  pszExpandedFilename, VSIStrerror( errno ) );

        CPLFree( pszExpandedFilename );
        return CE_Failure;
    }

    CPLFree( pszExpandedFilename );

    m_pszSourceFilename = CPLStrdup(pszFilename);
    m_bRelativeToVRT = bRelativeToVRTIn;

/* -------------------------------------------------------------------- */
/*      Work out if we are in native mode or not.                       */
/* -------------------------------------------------------------------- */
    bool bNative = true;

    if( pszByteOrder != NULL )
    {
        if( EQUAL(pszByteOrder,"LSB") )
            bNative = CPL_TO_BOOL(CPL_IS_LSB);
        else if( EQUAL(pszByteOrder,"MSB") )
            bNative = !CPL_IS_LSB;
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Illegal ByteOrder value '%s', should be LSB or MSB.",
                      pszByteOrder );
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding RawRasterBand.                           */
/* -------------------------------------------------------------------- */
    m_poRawRaster = new RawRasterBand( fp, nImageOffset, nPixelOffset,
                                       nLineOffset, GetRasterDataType(),
                                       bNative, GetXSize(), GetYSize(), TRUE );

/* -------------------------------------------------------------------- */
/*      Reset block size to match the raw raster.                       */
/* -------------------------------------------------------------------- */
    m_poRawRaster->GetBlockSize( &nBlockXSize, &nBlockYSize );

    return CE_None;
}

/************************************************************************/
/*                            ClearRawLink()                            */
/************************************************************************/

void VRTRawRasterBand::ClearRawLink()

{
    if( m_poRawRaster != NULL )
    {
        VSILFILE* fp = m_poRawRaster->GetFPL();
        delete m_poRawRaster;
        m_poRawRaster = NULL;
        // We close the file after deleting the raster band
        // since data can be flushed in the destructor.
        if( fp != NULL )
        {
            CPLCloseShared( reinterpret_cast<FILE*>( fp ) );
        }
    }
    CPLFree( m_pszSourceFilename );
    m_pszSourceFilename = NULL;
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr VRTRawRasterBand::XMLInit( CPLXMLNode * psTree,
                                  const char *pszVRTPath,
                                  void* pUniqueHandle )

{
    const CPLErr eErr = VRTRasterBand::XMLInit( psTree, pszVRTPath, pUniqueHandle );
    if( eErr != CE_None )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Validate a bit.                                                 */
/* -------------------------------------------------------------------- */
    if( psTree == NULL || psTree->eType != CXT_Element
        || !EQUAL(psTree->pszValue, "VRTRasterBand")
        || !EQUAL(CPLGetXMLValue(psTree,"subClass",""), "VRTRawRasterBand") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid node passed to VRTRawRasterBand::XMLInit()." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Prepare filename.                                               */
/* -------------------------------------------------------------------- */
    const char *pszFilename =
        CPLGetXMLValue(psTree, "SourceFilename", NULL);

    if( pszFilename == NULL )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Missing <SourceFilename> element in VRTRasterBand." );
        return CE_Failure;
    }

    // TODO(schwehr): Should this be a bool?
    const int l_bRelativeToVRT =
        atoi(CPLGetXMLValue( psTree, "SourceFilename.relativeToVRT", "1" ) );

/* -------------------------------------------------------------------- */
/*      Collect layout information.                                     */
/* -------------------------------------------------------------------- */
    int nWordDataSize = GDALGetDataTypeSizeBytes( GetRasterDataType() );

    const char* pszImageOffset = CPLGetXMLValue( psTree, "ImageOffset", "0");
    const vsi_l_offset nImageOffset = CPLScanUIntBig(
        pszImageOffset, static_cast<int>(strlen(pszImageOffset)) );

    int nPixelOffset = nWordDataSize;
    if( CPLGetXMLValue( psTree, "PixelOffset", NULL ) != NULL )
    {
        nPixelOffset = atoi(CPLGetXMLValue( psTree, "PixelOffset", "0") );
    }
    if (nPixelOffset <= 0)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid value for <PixelOffset> element : %d",
                  nPixelOffset );
        return CE_Failure;
    }

    int nLineOffset = 0;
    if( CPLGetXMLValue( psTree, "LineOffset", NULL ) == NULL )
        nLineOffset = nWordDataSize * GetXSize();
    else
        nLineOffset = atoi(CPLGetXMLValue( psTree, "LineOffset", "0") );

    const char *pszByteOrder = CPLGetXMLValue( psTree, "ByteOrder", NULL );

/* -------------------------------------------------------------------- */
/*      Open the file, and setup the raw layer access to the data.      */
/* -------------------------------------------------------------------- */
    return SetRawLink( pszFilename, pszVRTPath, l_bRelativeToVRT,
                       nImageOffset, nPixelOffset, nLineOffset,
                       pszByteOrder );
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTRawRasterBand::SerializeToXML( const char *pszVRTPath )

{

/* -------------------------------------------------------------------- */
/*      We can't set the layout if there is no open rawband.            */
/* -------------------------------------------------------------------- */
    if( m_poRawRaster == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "VRTRawRasterBand::SerializeToXML() fails because "
                  "m_poRawRaster is NULL." );
        return NULL;
    }

    CPLXMLNode *psTree = VRTRasterBand::SerializeToXML( pszVRTPath );

/* -------------------------------------------------------------------- */
/*      Set subclass.                                                   */
/* -------------------------------------------------------------------- */
    CPLCreateXMLNode(
        CPLCreateXMLNode( psTree, CXT_Attribute, "subClass" ),
        CXT_Text, "VRTRawRasterBand" );

/* -------------------------------------------------------------------- */
/*      Setup the filename with relative flag.                          */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psNode
        = CPLCreateXMLElementAndValue( psTree, "SourceFilename",
                                       m_pszSourceFilename );

    CPLCreateXMLNode(
        CPLCreateXMLNode( psNode, CXT_Attribute, "relativeToVRT" ),
        CXT_Text, m_bRelativeToVRT ? "1" : "0"  );

/* -------------------------------------------------------------------- */
/*      Set other layout information.                                   */
/* -------------------------------------------------------------------- */

    CPLCreateXMLElementAndValue( psTree, "ImageOffset",
                                 CPLSPrintf( CPL_FRMT_GUIB,
                                             m_poRawRaster->GetImgOffset()) );

    CPLCreateXMLElementAndValue( psTree, "PixelOffset",
                                 CPLSPrintf( "%d",
                                             m_poRawRaster->GetPixelOffset()) );

    CPLCreateXMLElementAndValue( psTree, "LineOffset",
                                 CPLSPrintf( "%d",
                                             m_poRawRaster->GetLineOffset()) );

#if CPL_IS_LSB == 1
    if( m_poRawRaster->GetNativeOrder() )
#else
    if( !m_poRawRaster->GetNativeOrder() )
#endif
        CPLCreateXMLElementAndValue( psTree, "ByteOrder", "LSB" );
    else
        CPLCreateXMLElementAndValue( psTree, "ByteOrder", "MSB" );

    return psTree;
}

/************************************************************************/
/*                             GetFileList()                            */
/************************************************************************/

void VRTRawRasterBand::GetFileList( char*** ppapszFileList, int *pnSize,
                                    int *pnMaxSize, CPLHashSet* hSetFiles )
{
    if (m_pszSourceFilename == NULL)
        return;

/* -------------------------------------------------------------------- */
/*      Is it already in the list ?                                     */
/* -------------------------------------------------------------------- */
    CPLString osSourceFilename;
    if( m_bRelativeToVRT && strlen(poDS->GetDescription()) > 0 )
        osSourceFilename = CPLFormFilename(
              CPLGetDirname(poDS->GetDescription()), m_pszSourceFilename, NULL );
    else
        osSourceFilename = m_pszSourceFilename;

    if( CPLHashSetLookup(hSetFiles, osSourceFilename) != NULL )
        return;

/* -------------------------------------------------------------------- */
/*      Grow array if necessary                                         */
/* -------------------------------------------------------------------- */
    if (*pnSize + 1 >= *pnMaxSize)
    {
        *pnMaxSize = 2 + 2 * (*pnMaxSize);
        *ppapszFileList = reinterpret_cast<char **>(
            CPLRealloc( *ppapszFileList, sizeof(char*) * (*pnMaxSize) ) );
    }

/* -------------------------------------------------------------------- */
/*      Add the string to the list                                      */
/* -------------------------------------------------------------------- */
    (*ppapszFileList)[*pnSize] = CPLStrdup(osSourceFilename);
    (*ppapszFileList)[(*pnSize + 1)] = NULL;
    CPLHashSetInsert(hSetFiles, (*ppapszFileList)[*pnSize]);

    (*pnSize)++;

    VRTRasterBand::GetFileList( ppapszFileList, pnSize,
                                pnMaxSize, hSetFiles);
}

/*! @endcond */
