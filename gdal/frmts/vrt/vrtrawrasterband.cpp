/******************************************************************************
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Implementation of VRTRawRasterBand
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
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
    m_poRawRaster(nullptr),
    m_pszSourceFilename(nullptr),
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
    FlushCache(true);
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
    if( m_poRawRaster == nullptr )
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
    if( m_poRawRaster == nullptr )
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
    if( m_poRawRaster == nullptr )
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

    static_cast<VRTDataset *>( poDS )->SetNeedsFlush();

/* -------------------------------------------------------------------- */
/*      Prepare filename.                                               */
/* -------------------------------------------------------------------- */
    if( pszFilename == nullptr )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Missing <SourceFilename> element in VRTRasterBand." );
        return CE_Failure;
    }

    char *pszExpandedFilename = nullptr;
    if( pszVRTPath != nullptr && bRelativeToVRTIn )
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
    CPLPushErrorHandler(CPLQuietErrorHandler);
    FILE *fp = CPLOpenShared( pszExpandedFilename, "rb+", TRUE );

    if( fp == nullptr )
        fp = CPLOpenShared( pszExpandedFilename, "rb", TRUE );

    if( fp == nullptr
        && static_cast<VRTDataset *>( poDS )->GetAccess() == GA_Update )
    {
        fp = CPLOpenShared( pszExpandedFilename, "wb+", TRUE );
    }
    CPLPopErrorHandler();
    CPLErrorReset();

    if( fp == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to open %s.%s",
                  pszExpandedFilename, VSIStrerror( errno ) );

        CPLFree( pszExpandedFilename );
        return CE_Failure;
    }

    CPLFree( pszExpandedFilename );

    if( !RAWDatasetCheckMemoryUsage(
                        nRasterXSize, nRasterYSize, 1,
                        GDALGetDataTypeSizeBytes(GetRasterDataType()),
                        nPixelOffset, nLineOffset, nImageOffset, 0,
                        reinterpret_cast<VSILFILE*>(fp)) )
    {
        CPLCloseShared(fp);
        return CE_Failure;
    }

    m_pszSourceFilename = CPLStrdup(pszFilename);
    m_bRelativeToVRT = bRelativeToVRTIn;

/* -------------------------------------------------------------------- */
/*      Work out if we are in native mode or not.                       */
/* -------------------------------------------------------------------- */
    RawRasterBand::ByteOrder eByteOrder =
#if CPL_IS_LSB
        RawRasterBand::ByteOrder::ORDER_LITTLE_ENDIAN;
#else
        RawRasterBand::ByteOrder::ORDER_BIG_ENDIAN;
#endif

    if( pszByteOrder != nullptr )
    {
        if( EQUAL(pszByteOrder,"LSB") )
            eByteOrder = RawRasterBand::ByteOrder::ORDER_LITTLE_ENDIAN;
        else if( EQUAL(pszByteOrder,"MSB") )
            eByteOrder = RawRasterBand::ByteOrder::ORDER_BIG_ENDIAN;
        else if( EQUAL(pszByteOrder,"VAX") )
            eByteOrder = RawRasterBand::ByteOrder::ORDER_VAX;
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Illegal ByteOrder value '%s', should be LSB, MSB or VAX.",
                      pszByteOrder );
            CPLCloseShared(fp);
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding RawRasterBand.                           */
/* -------------------------------------------------------------------- */
    m_poRawRaster = new RawRasterBand( reinterpret_cast<VSILFILE*>(fp),
                                       nImageOffset, nPixelOffset,
                                       nLineOffset, GetRasterDataType(),
                                       eByteOrder, GetXSize(), GetYSize(),
                                       RawRasterBand::OwnFP::NO );

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
    if( m_poRawRaster != nullptr )
    {
        VSILFILE* fp = m_poRawRaster->GetFPL();
        delete m_poRawRaster;
        m_poRawRaster = nullptr;
        // We close the file after deleting the raster band
        // since data can be flushed in the destructor.
        if( fp != nullptr )
        {
            CPLCloseShared( reinterpret_cast<FILE*>( fp ) );
        }
    }
    CPLFree( m_pszSourceFilename );
    m_pszSourceFilename = nullptr;
}

/************************************************************************/
/*                            GetVirtualMemAuto()                       */
/************************************************************************/

CPLVirtualMem * VRTRawRasterBand::GetVirtualMemAuto( GDALRWFlag eRWFlag,
                                                     int *pnPixelSpace,
                                                     GIntBig *pnLineSpace,
                                                     char **papszOptions )

{
    // check the pointer to RawRasterBand
    if( m_poRawRaster == nullptr )
    {
        // use the super class method
        return VRTRasterBand::GetVirtualMemAuto(eRWFlag, pnPixelSpace, pnLineSpace, papszOptions);
    }
    // if available, use the RawRasterBand method (use mmap if available)
    return m_poRawRaster->GetVirtualMemAuto(eRWFlag, pnPixelSpace, pnLineSpace, papszOptions);
}


/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr VRTRawRasterBand::XMLInit( CPLXMLNode * psTree,
                                  const char *pszVRTPath,
                                  std::map<CPLString, GDALDataset*>& oMapSharedSources )

{
    const CPLErr eErr = VRTRasterBand::XMLInit( psTree, pszVRTPath,
                                                oMapSharedSources );
    if( eErr != CE_None )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Validate a bit.                                                 */
/* -------------------------------------------------------------------- */
    if( psTree == nullptr || psTree->eType != CXT_Element
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
        CPLGetXMLValue(psTree, "SourceFilename", nullptr);

    if( pszFilename == nullptr )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Missing <SourceFilename> element in VRTRasterBand." );
        return CE_Failure;
    }

    const bool l_bRelativeToVRT = CPLTestBool(
        CPLGetXMLValue( psTree, "SourceFilename.relativeToVRT", "1" ));

/* -------------------------------------------------------------------- */
/*      Collect layout information.                                     */
/* -------------------------------------------------------------------- */
    int nWordDataSize = GDALGetDataTypeSizeBytes( GetRasterDataType() );

    const char* pszImageOffset = CPLGetXMLValue( psTree, "ImageOffset", "0");
    const vsi_l_offset nImageOffset = CPLScanUIntBig(
        pszImageOffset, static_cast<int>(strlen(pszImageOffset)) );

    int nPixelOffset = nWordDataSize;
    const char* pszPixelOffset =
                            CPLGetXMLValue( psTree, "PixelOffset", nullptr );
    if( pszPixelOffset != nullptr )
    {
        nPixelOffset = atoi(pszPixelOffset);
    }
    if (nPixelOffset <= 0)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid value for <PixelOffset> element : %d",
                  nPixelOffset );
        return CE_Failure;
    }

    int nLineOffset = 0;
    const char* pszLineOffset = CPLGetXMLValue( psTree, "LineOffset", nullptr );
    if( pszLineOffset == nullptr )
    {
        if( nPixelOffset > INT_MAX / GetXSize() )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Int overflow");
            return CE_Failure;
        }
        nLineOffset = nPixelOffset * GetXSize();
    }
    else
        nLineOffset = atoi(pszLineOffset);

    const char *pszByteOrder = CPLGetXMLValue( psTree, "ByteOrder", nullptr );

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
    if( m_poRawRaster == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "VRTRawRasterBand::SerializeToXML() fails because "
                  "m_poRawRaster is NULL." );
        return nullptr;
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

    switch( m_poRawRaster->GetByteOrder() )
    {
        case RawRasterBand::ByteOrder::ORDER_LITTLE_ENDIAN:
            CPLCreateXMLElementAndValue( psTree, "ByteOrder", "LSB" );
            break;
        case RawRasterBand::ByteOrder::ORDER_BIG_ENDIAN:
            CPLCreateXMLElementAndValue( psTree, "ByteOrder", "MSB" );
            break;
        case RawRasterBand::ByteOrder::ORDER_VAX:
            CPLCreateXMLElementAndValue( psTree, "ByteOrder", "VAX" );
            break;
    }

    return psTree;
}

/************************************************************************/
/*                             GetFileList()                            */
/************************************************************************/

void VRTRawRasterBand::GetFileList( char*** ppapszFileList, int *pnSize,
                                    int *pnMaxSize, CPLHashSet* hSetFiles )
{
    if (m_pszSourceFilename == nullptr)
        return;

/* -------------------------------------------------------------------- */
/*      Is it already in the list ?                                     */
/* -------------------------------------------------------------------- */
    CPLString osSourceFilename;
    if( m_bRelativeToVRT && strlen(poDS->GetDescription()) > 0 )
        osSourceFilename = CPLFormFilename(
              CPLGetDirname(poDS->GetDescription()), m_pszSourceFilename, nullptr );
    else
        osSourceFilename = m_pszSourceFilename;

    if( CPLHashSetLookup(hSetFiles, osSourceFilename) != nullptr )
        return;

/* -------------------------------------------------------------------- */
/*      Grow array if necessary                                         */
/* -------------------------------------------------------------------- */
    if (*pnSize + 1 >= *pnMaxSize)
    {
        *pnMaxSize = 2 + 2 * (*pnMaxSize);
        *ppapszFileList = static_cast<char **>(
            CPLRealloc( *ppapszFileList, sizeof(char*) * (*pnMaxSize) ) );
    }

/* -------------------------------------------------------------------- */
/*      Add the string to the list                                      */
/* -------------------------------------------------------------------- */
    (*ppapszFileList)[*pnSize] = CPLStrdup(osSourceFilename);
    (*ppapszFileList)[(*pnSize + 1)] = nullptr;
    CPLHashSetInsert(hSetFiles, (*ppapszFileList)[*pnSize]);

    (*pnSize)++;

    VRTRasterBand::GetFileList( ppapszFileList, pnSize,
                                pnMaxSize, hSetFiles);
}

/*! @endcond */
