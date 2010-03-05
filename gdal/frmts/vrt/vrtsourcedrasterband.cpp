/******************************************************************************
 * $Id$
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Implementation of VRTSourcedRasterBand
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
 ****************************************************************************/

#include "vrtdataset.h"
#include "cpl_minixml.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*                          VRTSourcedRasterBand                        */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                        VRTSourcedRasterBand()                        */
/************************************************************************/

VRTSourcedRasterBand::VRTSourcedRasterBand( GDALDataset *poDS, int nBand )

{
    Initialize( poDS->GetRasterXSize(), poDS->GetRasterYSize() );

    this->poDS = poDS;
    this->nBand = nBand;
}

/************************************************************************/
/*                        VRTSourcedRasterBand()                        */
/************************************************************************/

VRTSourcedRasterBand::VRTSourcedRasterBand( GDALDataType eType, 
                                            int nXSize, int nYSize )

{
    Initialize( nXSize, nYSize );

    eDataType = eType;
}

/************************************************************************/
/*                        VRTSourcedRasterBand()                        */
/************************************************************************/

VRTSourcedRasterBand::VRTSourcedRasterBand( GDALDataset *poDS, int nBand,
                                            GDALDataType eType, 
                                            int nXSize, int nYSize )

{
    Initialize( nXSize, nYSize );

    this->poDS = poDS;
    this->nBand = nBand;

    eDataType = eType;
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

void VRTSourcedRasterBand::Initialize( int nXSize, int nYSize )

{
    VRTRasterBand::Initialize( nXSize, nYSize );

    nSources = 0;
    papoSources = NULL;
    bEqualAreas = FALSE;
    bAlreadyInIRasterIO = FALSE;
}

/************************************************************************/
/*                       ~VRTSourcedRasterBand()                        */
/************************************************************************/

VRTSourcedRasterBand::~VRTSourcedRasterBand()

{
    for( int i = 0; i < nSources; i++ )
        delete papoSources[i];

    CPLFree( papoSources );
    nSources = 0;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr VRTSourcedRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                 int nXOff, int nYOff, int nXSize, int nYSize,
                                 void * pData, int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType,
                                 int nPixelSpace, int nLineSpace )

{
    int         iSource;
    CPLErr      eErr = CE_None;

    if( eRWFlag == GF_Write )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Writing through VRTSourcedRasterBand is not supported." );
        return CE_Failure;
    }
    
    /* When using GDALProxyPoolDataset for sources, the recusion will not be */
    /* detected at VRT opening but when doing RasterIO. As the proxy pool will */
    /* return the already opened dataset, we can just test a member variable. */
    if ( bAlreadyInIRasterIO )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "VRTSourcedRasterBand::IRasterIO() called recursively on the same band. "
                  "It looks like the VRT is referencing itself." );
        return CE_Failure;
    }

/* ==================================================================== */
/*      Do we have overviews that would be appropriate to satisfy       */
/*      this request?                                                   */
/* ==================================================================== */
    if( (nBufXSize < nXSize || nBufYSize < nYSize)
        && GetOverviewCount() > 0 )
    {
        if( OverviewRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize, 
                              pData, nBufXSize, nBufYSize, 
                              eBufType, nPixelSpace, nLineSpace ) == CE_None )
            return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Initialize the buffer to some background value. Use the         */
/*      nodata value if available.                                      */
/* -------------------------------------------------------------------- */
    if ( nPixelSpace == GDALGetDataTypeSize(eBufType)/8 &&
         (!bNoDataValueSet || dfNoDataValue == 0) )
    {
        if (nLineSpace == nBufXSize * nPixelSpace)
        {
             memset( pData, 0, nBufYSize * nLineSpace );
        }
        else
        {
            int    iLine;
            for( iLine = 0; iLine < nBufYSize; iLine++ )
            {
                memset( ((GByte*)pData) + iLine * nLineSpace, 0, nBufXSize * nPixelSpace );
            }
        }
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
    
    bAlreadyInIRasterIO = TRUE;

/* -------------------------------------------------------------------- */
/*      Overlay each source in turn over top this.                      */
/* -------------------------------------------------------------------- */
    for( iSource = 0; eErr == CE_None && iSource < nSources; iSource++ )
    {
        eErr = 
            papoSources[iSource]->RasterIO( nXOff, nYOff, nXSize, nYSize, 
                                            pData, nBufXSize, nBufYSize, 
                                            eBufType, nPixelSpace, nLineSpace);
    }
    
    bAlreadyInIRasterIO = FALSE;
    
    return eErr;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr VRTSourcedRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                   void * pImage )

{
    int nPixelSize = GDALGetDataTypeSize(eDataType)/8;
    int nReadXSize, nReadYSize;

    if( (nBlockXOff+1) * nBlockXSize > GetXSize() )
        nReadXSize = GetXSize() - nBlockXOff * nBlockXSize;
    else
        nReadXSize = nBlockXSize;

    if( (nBlockYOff+1) * nBlockYSize > GetYSize() )
        nReadYSize = GetYSize() - nBlockYOff * nBlockYSize;
    else
        nReadYSize = nBlockYSize;

    return IRasterIO( GF_Read, 
                      nBlockXOff * nBlockXSize, nBlockYOff * nBlockYSize, 
                      nReadXSize, nReadYSize, 
                      pImage, nReadXSize, nReadYSize, eDataType, 
                      nPixelSize, nPixelSize * nBlockXSize );
}

/************************************************************************/
/*                             AddSource()                              */
/************************************************************************/

CPLErr VRTSourcedRasterBand::AddSource( VRTSource *poNewSource )

{
    nSources++;

    papoSources = (VRTSource **) 
        CPLRealloc(papoSources, sizeof(void*) * nSources);
    papoSources[nSources-1] = poNewSource;

    ((VRTDataset *)poDS)->SetNeedsFlush();

    return CE_None;
}

/************************************************************************/
/*                              VRTAddSource()                          */
/************************************************************************/

/**
 * @see VRTSourcedRasterBand::AddSource().
 */

CPLErr CPL_STDCALL VRTAddSource( VRTSourcedRasterBandH hVRTBand,
                                 VRTSourceH hNewSource )
{
    VALIDATE_POINTER1( hVRTBand, "VRTAddSource", CE_Failure );

    return ((VRTSourcedRasterBand *) hVRTBand)->
        AddSource( (VRTSource *)hNewSource );
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr VRTSourcedRasterBand::XMLInit( CPLXMLNode * psTree, 
                                      const char *pszVRTPath )

{
    CPLErr eErr;

    eErr = VRTRasterBand::XMLInit( psTree, pszVRTPath );
    if( eErr != CE_None )
        return eErr;
    
/* -------------------------------------------------------------------- */
/*      Validate a bit.                                                 */
/* -------------------------------------------------------------------- */
    if( psTree == NULL || psTree->eType != CXT_Element
        || (!EQUAL(psTree->pszValue,"VRTSourcedRasterBand") 
            && !EQUAL(psTree->pszValue,"VRTRasterBand")
	    && !EQUAL(psTree->pszValue,"VRTDerivedRasterBand")) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Invalid node passed to VRTSourcedRasterBand::XMLInit()." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Process sources.                                                */
/* -------------------------------------------------------------------- */
    CPLXMLNode  *psChild;
    VRTDriver *poDriver = (VRTDriver *) GDALGetDriverByName( "VRT" );
    
    for( psChild = psTree->psChild; 
         psChild != NULL && poDriver != NULL; 
         psChild = psChild->psNext)
    {
        VRTSource *poSource;

        if( psChild->eType != CXT_Element )
            continue;

        CPLErrorReset();
        poSource = poDriver->ParseSource( psChild, pszVRTPath );
        if( poSource != NULL )
            AddSource( poSource );
        else if( CPLGetLastErrorType() != CE_None )
            return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Done.                                                           */
/* -------------------------------------------------------------------- */
    if( nSources == 0 )
        CPLDebug( "VRT", "No valid sources found for band in VRT file:\n%s",
                  pszVRTPath );

    return CE_None;
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTSourcedRasterBand::SerializeToXML( const char *pszVRTPath )

{
    CPLXMLNode *psTree;

    psTree = VRTRasterBand::SerializeToXML( pszVRTPath );

/* -------------------------------------------------------------------- */
/*      Process Sources.                                                */
/* -------------------------------------------------------------------- */
    for( int iSource = 0; iSource < nSources; iSource++ )
    {
        CPLXMLNode      *psXMLSrc;

        psXMLSrc = papoSources[iSource]->SerializeToXML( pszVRTPath );
        
        if( psXMLSrc != NULL )
            CPLAddXMLChild( psTree, psXMLSrc );
    }

    return psTree;
}

/************************************************************************/
/*                          AddSimpleSource()                           */
/************************************************************************/

CPLErr VRTSourcedRasterBand::AddSimpleSource( GDALRasterBand *poSrcBand, 
                                       int nSrcXOff, int nSrcYOff, 
                                       int nSrcXSize, int nSrcYSize, 
                                       int nDstXOff, int nDstYOff, 
                                       int nDstXSize, int nDstYSize,
                                       const char *pszResampling, 
                                       double dfNoDataValue )

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
    VRTSimpleSource *poSimpleSource;

    if( pszResampling != NULL && EQUALN(pszResampling,"aver",4) )
        poSimpleSource = new VRTAveragedSource();
    else
    {
        poSimpleSource = new VRTSimpleSource();
        if( dfNoDataValue != VRT_NODATA_UNSET )
            CPLError( 
                CE_Warning, CPLE_AppDefined, 
                "NODATA setting not currently supported for nearest\n"
                "neighbour sampled simple sources on Virtual Datasources." );
    }

    poSimpleSource->SetSrcBand( poSrcBand );
    poSimpleSource->SetSrcWindow( nSrcXOff, nSrcYOff, nSrcXSize, nSrcYSize );
    poSimpleSource->SetDstWindow( nDstXOff, nDstYOff, nDstXSize, nDstYSize );

    if( dfNoDataValue != VRT_NODATA_UNSET )
        poSimpleSource->SetNoDataValue( dfNoDataValue );

/* -------------------------------------------------------------------- */
/*      Default source and dest rectangles.                             */
/* -------------------------------------------------------------------- */
    if ( nSrcXOff == nDstXOff && nSrcYOff == nDstYOff &&
         nSrcXSize == nDstXSize && nSrcYSize == nRasterYSize )
        bEqualAreas = TRUE;

/* -------------------------------------------------------------------- */
/*      If we can get the associated GDALDataset, add a reference to it.*/
/* -------------------------------------------------------------------- */
    if( poSrcBand->GetDataset() != NULL )
        poSrcBand->GetDataset()->Reference();

/* -------------------------------------------------------------------- */
/*      add to list.                                                    */
/* -------------------------------------------------------------------- */
    return AddSource( poSimpleSource );
}

/************************************************************************/
/*                         VRTAddSimpleSource()                         */
/************************************************************************/

/**
 * @see VRTSourcedRasterBand::AddSimpleSource().
 */

CPLErr CPL_STDCALL VRTAddSimpleSource( VRTSourcedRasterBandH hVRTBand,
                                       GDALRasterBandH hSrcBand, 
                                       int nSrcXOff, int nSrcYOff, 
                                       int nSrcXSize, int nSrcYSize, 
                                       int nDstXOff, int nDstYOff, 
                                       int nDstXSize, int nDstYSize,
                                       const char *pszResampling,
                                       double dfNoDataValue )
{
    VALIDATE_POINTER1( hVRTBand, "VRTAddSimpleSource", CE_Failure );

    return ((VRTSourcedRasterBand *) hVRTBand)->AddSimpleSource(
                                            (GDALRasterBand *)hSrcBand, 
                                            nSrcXOff, nSrcYOff, 
                                            nSrcXSize, nSrcYSize, 
                                            nDstXOff, nDstYOff, 
                                            nDstXSize, nDstYSize,
                                            pszResampling, dfNoDataValue );
}

/************************************************************************/
/*                          AddComplexSource()                          */
/************************************************************************/

CPLErr VRTSourcedRasterBand::AddComplexSource( GDALRasterBand *poSrcBand, 
                                               int nSrcXOff, int nSrcYOff, 
                                               int nSrcXSize, int nSrcYSize, 
                                               int nDstXOff, int nDstYOff, 
                                               int nDstXSize, int nDstYSize,
                                               double dfScaleOff,
                                               double dfScaleRatio,
                                               double dfNoDataValue,
                                               int nColorTableComponent)

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
    VRTComplexSource *poSource;

    poSource = new VRTComplexSource();

    poSource->SetSrcBand( poSrcBand );
    poSource->SetSrcWindow( nSrcXOff, nSrcYOff, nSrcXSize, nSrcYSize );
    poSource->SetDstWindow( nDstXOff, nDstYOff, nDstXSize, nDstYSize );

/* -------------------------------------------------------------------- */
/*      Set complex parameters.                                         */
/* -------------------------------------------------------------------- */
    if( dfNoDataValue != VRT_NODATA_UNSET )
        poSource->SetNoDataValue( dfNoDataValue );

    if( dfScaleOff != 0.0 || dfScaleRatio != 1.0 )
    {
        poSource->bDoScaling = TRUE;
        poSource->dfScaleOff = dfScaleOff;
        poSource->dfScaleRatio = dfScaleRatio;
          
    }

    poSource->nColorTableComponent = nColorTableComponent;

/* -------------------------------------------------------------------- */
/*      If we can get the associated GDALDataset, add a reference to it.*/
/* -------------------------------------------------------------------- */
    if( poSrcBand->GetDataset() != NULL )
        poSrcBand->GetDataset()->Reference();

/* -------------------------------------------------------------------- */
/*      add to list.                                                    */
/* -------------------------------------------------------------------- */
    return AddSource( poSource );
}

/************************************************************************/
/*                         VRTAddComplexSource()                        */
/************************************************************************/

/**
 * @see VRTSourcedRasterBand::AddComplexSource().
 */

CPLErr CPL_STDCALL VRTAddComplexSource( VRTSourcedRasterBandH hVRTBand,
                                        GDALRasterBandH hSrcBand, 
                                        int nSrcXOff, int nSrcYOff, 
                                        int nSrcXSize, int nSrcYSize, 
                                        int nDstXOff, int nDstYOff, 
                                        int nDstXSize, int nDstYSize,
                                        double dfScaleOff, 
                                        double dfScaleRatio,
                                        double dfNoDataValue )
{
    VALIDATE_POINTER1( hVRTBand, "VRTAddComplexSource", CE_Failure );

    return ((VRTSourcedRasterBand *) hVRTBand)->AddComplexSource(
                                            (GDALRasterBand *)hSrcBand, 
                                            nSrcXOff, nSrcYOff, 
                                            nSrcXSize, nSrcYSize, 
                                            nDstXOff, nDstYOff, 
                                            nDstXSize, nDstYSize,
                                            dfScaleOff, dfScaleRatio,
                                            dfNoDataValue );
}

/************************************************************************/
/*                           AddFuncSource()                            */
/************************************************************************/

CPLErr VRTSourcedRasterBand::AddFuncSource( VRTImageReadFunc pfnReadFunc, 
                                     void *pCBData, double dfNoDataValue )

{
/* -------------------------------------------------------------------- */
/*      Create source.                                                  */
/* -------------------------------------------------------------------- */
    VRTFuncSource *poFuncSource = new VRTFuncSource;

    poFuncSource->fNoDataValue = (float) dfNoDataValue;
    poFuncSource->pfnReadFunc = pfnReadFunc;
    poFuncSource->pCBData = pCBData;
    poFuncSource->eType = GetRasterDataType();

/* -------------------------------------------------------------------- */
/*      add to list.                                                    */
/* -------------------------------------------------------------------- */
    return AddSource( poFuncSource );
}

/************************************************************************/
/*                          VRTAddFuncSource()                          */
/************************************************************************/

/**
 * @see VRTSourcedRasterBand::AddFuncSource().
 */

CPLErr CPL_STDCALL VRTAddFuncSource( VRTSourcedRasterBandH hVRTBand,
                                     VRTImageReadFunc pfnReadFunc, 
                                     void *pCBData, double dfNoDataValue )
{
    VALIDATE_POINTER1( hVRTBand, "VRTAddFuncSource", CE_Failure );

    return ((VRTSourcedRasterBand *) hVRTBand)->
        AddFuncSource( pfnReadFunc, pCBData, dfNoDataValue );
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **VRTSourcedRasterBand::GetMetadata( const char *pszDomain )

{
    if( pszDomain != NULL && EQUAL(pszDomain,"vrt_sources") )
    {
        char **papszSourceList = NULL;

/* -------------------------------------------------------------------- */
/*      Process SimpleSources.                                          */
/* -------------------------------------------------------------------- */
        for( int iSource = 0; iSource < nSources; iSource++ )
        {
            CPLXMLNode      *psXMLSrc;
            char            *pszXML;

            psXMLSrc = papoSources[iSource]->SerializeToXML( NULL );
            if( psXMLSrc == NULL )
                continue;

            pszXML = CPLSerializeXMLTree( psXMLSrc );

            papszSourceList = 
                CSLSetNameValue( papszSourceList, 
                                 CPLSPrintf( "source_%d", iSource ), pszXML );
            CPLFree( pszXML );
            CPLDestroyXMLNode( psXMLSrc );
        }
        
        return papszSourceList;
    }
    else
        return GDALRasterBand::GetMetadata( pszDomain );
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr VRTSourcedRasterBand::SetMetadataItem( const char *pszName, 
                                              const char *pszValue, 
                                              const char *pszDomain )

{
    CPLDebug( "VRT", "VRTSourcedRasterBand::SetMetadataItem(%s,%s,%s)\n",
              pszName, pszValue, pszDomain );
              
    if( pszDomain != NULL
        && EQUAL(pszDomain,"new_vrt_sources") )
    {
        VRTDriver *poDriver = (VRTDriver *) GDALGetDriverByName( "VRT" );

        CPLXMLNode *psTree = CPLParseXMLString( pszValue );
        VRTSource *poSource;
        
        if( psTree == NULL )
            return CE_Failure;
        
        poSource = poDriver->ParseSource( psTree, NULL );
        CPLDestroyXMLNode( psTree );
        
        if( poSource != NULL )
            return AddSource( poSource );
        else
            return CE_Failure;
    }
    else if( pszDomain != NULL
        && EQUAL(pszDomain,"vrt_sources") )
    {
        int iSource;
        if (sscanf(pszName, "source_%d", &iSource) != 1 || iSource < 0 ||
            iSource >= nSources)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s metadata item name is not recognized. "
                     "Should be between source_0 and source_%d",
                     pszName, nSources - 1);
            return CE_Failure;
        }

        VRTDriver *poDriver = (VRTDriver *) GDALGetDriverByName( "VRT" );

        CPLXMLNode *psTree = CPLParseXMLString( pszValue );
        VRTSource *poSource;
        
        if( psTree == NULL )
            return CE_Failure;
        
        poSource = poDriver->ParseSource( psTree, NULL );
        CPLDestroyXMLNode( psTree );
        
        if( poSource != NULL )
        {
            delete papoSources[iSource];
            papoSources[iSource] = poSource;
            ((VRTDataset *)poDS)->SetNeedsFlush();
            return CE_None;
        }
        else
            return CE_Failure;
    }
    else
        return VRTRasterBand::SetMetadataItem( pszName, pszValue, pszDomain );
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr VRTSourcedRasterBand::SetMetadata( char **papszNewMD, const char *pszDomain )

{
    if( pszDomain != NULL
        && (EQUAL(pszDomain,"new_vrt_sources") 
            || EQUAL(pszDomain,"vrt_sources")) )
    {
        VRTDriver *poDriver = (VRTDriver *) GDALGetDriverByName( "VRT" );
        CPLErr eErr;
        int    i;

        if( EQUAL(pszDomain,"vrt_sources") )
        {
            for( int i = 0; i < nSources; i++ )
                delete papoSources[i];
            CPLFree( papoSources );
            papoSources = NULL;
            nSources = 0;
        }

        for( i = 0; i < CSLCount(papszNewMD); i++ )
        {
            const char *pszXML = CPLParseNameValue( papszNewMD[i], NULL );
            CPLXMLNode *psTree = CPLParseXMLString( pszXML );
            VRTSource *poSource;
            
            if( psTree == NULL )
                return CE_Failure;

            poSource = poDriver->ParseSource( psTree, NULL );
            CPLDestroyXMLNode( psTree );

            if( poSource != NULL )
            {
                eErr = AddSource( poSource );
                if( eErr != CE_None )
                    return eErr;
            }
            else
                return CE_Failure;
        }

        return CE_None;
    }
    else
        return VRTRasterBand::SetMetadata( papszNewMD, pszDomain );
}

/************************************************************************/
/*                             GetFileList()                            */
/************************************************************************/

void VRTSourcedRasterBand::GetFileList(char*** ppapszFileList, int *pnSize,
                                       int *pnMaxSize, CPLHashSet* hSetFiles)
{
    for( int i = 0; i < nSources; i++ )
    {
        papoSources[i]->GetFileList(ppapszFileList, pnSize,
                                    pnMaxSize, hSetFiles);
    }

    VRTRasterBand::GetFileList( ppapszFileList, pnSize,
                                pnMaxSize, hSetFiles);
}
