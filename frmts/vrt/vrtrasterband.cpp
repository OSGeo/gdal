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
 * Revision 1.16  2003/12/10 15:44:42  warmerda
 * Fix problem with NULL pszDomain values.
 *
 * Revision 1.15  2003/07/17 20:29:15  warmerda
 * Moved all the sources implementations out.
 * Improved error checking and returning when initializing from XML.
 * Added vrt_sources (and new_vrt_sources) metadata domains to permit
 * management of sources via XML passed in/out of the metadata methods.
 *
 * Revision 1.14  2003/06/10 19:59:33  warmerda
 * added new Func based source type for passthrough to a callback
 *
 * Revision 1.13  2003/03/14 10:31:22  dron
 * Check return value of the VRTSimpleSource::XMLInit() in VRTRasterBand::XMLInit().
 *
 * Revision 1.12  2003/03/13 20:39:25  dron
 * Improved block initialization logic in IRasterIO().
 *
 * Revision 1.11  2003/01/15 21:30:48  warmerda
 * some rounding tweaks when adjusting output size in GetSrcDstWindow
 *
 * Revision 1.10  2002/12/13 15:07:07  warmerda
 * fixed bug in IReadBlock
 *
 * Revision 1.9  2002/12/04 03:12:57  warmerda
 * Fixed some bugs in last change.
 *
 * Revision 1.8  2002/12/03 15:21:19  warmerda
 * finished up window computations for odd cases
 *
 * Revision 1.7  2002/11/24 04:29:02  warmerda
 * Substantially rewrote VRTSimpleSource.  Now VRTSource is base class, and
 * sources do their own SerializeToXML(), and XMLInit().  New VRTComplexSource
 * supports scaling and nodata values.
 *
 * Revision 1.6  2002/07/15 18:51:46  warmerda
 * ensure even unshared datasets are dereferenced properly
 *
 * Revision 1.5  2002/05/29 18:13:44  warmerda
 * added nodata handling for averager
 *
 * Revision 1.4  2002/05/29 16:06:05  warmerda
 * complete detailed band metadata
 *
 * Revision 1.3  2001/12/06 19:53:58  warmerda
 * added write check to rasterio
 *
 * Revision 1.2  2001/11/16 21:38:07  warmerda
 * try to work even if bModified
 *
 * Revision 1.1  2001/11/16 21:14:31  warmerda
 * New
 *
 */

#include "vrtdataset.h"
#include "cpl_minixml.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*                          VRTRasterBand                               */
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

VRTRasterBand::VRTRasterBand( GDALDataType eType, 
                              int nXSize, int nYSize )

{
    Initialize( nXSize, nYSize );

    eDataType = eType;
}

/************************************************************************/
/*                           VRTRasterBand()                            */
/************************************************************************/

VRTRasterBand::VRTRasterBand( GDALDataset *poDS, int nBand,
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

    bEqualAreas = FALSE;
    bNoDataValueSet = FALSE;
    dfNoDataValue = -10000.0;
    poColorTable = NULL;
    eColorInterp = GCI_Undefined;
}


/************************************************************************/
/*                           ~VRTRasterBand()                           */
/************************************************************************/

VRTRasterBand::~VRTRasterBand()

{
    for( int i = 0; i < nSources; i++ )
        delete papoSources[i];

    CPLFree( papoSources );
    nSources = 0;

    if( poColorTable != NULL )
        delete poColorTable;
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
    int         iSource;
    CPLErr      eErr = CE_Failure;

    if( eRWFlag == GF_Write )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Writing through VRTRasterBand is not supported." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Initialize the buffer to some background value. Use the         */
/*      nodata value if available.                                      */
/* -------------------------------------------------------------------- */
    if ( nPixelSpace == GDALGetDataTypeSize(eBufType)/8 &&
         (!bNoDataValueSet || dfNoDataValue == 0) )
        memset( pData, 0, nBufXSize * nBufYSize * nPixelSpace );
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
/*      Overlay each source in turn over top this.                      */
/* -------------------------------------------------------------------- */
    for( iSource = 0; iSource < nSources; iSource++ )
    {
        eErr = 
            papoSources[iSource]->RasterIO( nXOff, nYOff, nXSize, nYSize, 
                                            pData, nBufXSize, nBufYSize, 
                                            eBufType, nPixelSpace, nLineSpace);
    }
    
    return eErr;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr VRTRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
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

CPLErr VRTRasterBand::AddSource( VRTSource *poNewSource )

{
    nSources++;

    papoSources = (VRTSource **) 
        CPLRealloc(papoSources, sizeof(void*) * nSources);
    papoSources[nSources-1] = poNewSource;

    ((VRTDataset *)poDS)->SetNeedsFlush();

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
/*      Set the band if provided as an attribute.                       */
/* -------------------------------------------------------------------- */
    const char *pszDataType = CPLGetXMLValue( psTree, "dataType", NULL);
    if( pszDataType != NULL )
    {
        for( int iType = 0; iType < GDT_TypeCount; iType++ )
        {
            const char *pszThisName = GDALGetDataTypeName((GDALDataType)iType);

            if( pszThisName != NULL && EQUAL(pszDataType,pszThisName) )
            {
                eDataType = (GDALDataType) iType;
                break;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Apply any band level metadata.                                  */
/* -------------------------------------------------------------------- */
    VRTApplyMetadata( psTree, this );

/* -------------------------------------------------------------------- */
/*      Collect various other items of metadata.                        */
/* -------------------------------------------------------------------- */
    SetDescription( CPLGetXMLValue( psTree, "Description", "" ) );
    
    if( CPLGetXMLValue( psTree, "NoDataValue", NULL ) != NULL )
        SetNoDataValue( atof(CPLGetXMLValue( psTree, "NoDataValue", "0" )) );

    if( CPLGetXMLValue( psTree, "ColorInterp", NULL ) != NULL )
    {
        const char *pszInterp = CPLGetXMLValue( psTree, "ColorInterp", NULL );
        int        iInterp;
        
        for( iInterp = 0; iInterp < 13; iInterp++ )
        {
            const char *pszCandidate 
                = GDALGetColorInterpretationName( (GDALColorInterp) iInterp );

            if( pszCandidate != NULL && EQUAL(pszCandidate,pszInterp) )
            {
                SetColorInterpretation( (GDALColorInterp) iInterp );
                break;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Collect a color table.                                          */
/* -------------------------------------------------------------------- */
    if( CPLGetXMLNode( psTree, "ColorTable" ) != NULL )
    {
        CPLXMLNode *psEntry;
        GDALColorTable oTable;
        int        iEntry = 0;

        for( psEntry = CPLGetXMLNode( psTree, "ColorTable" )->psChild;
             psEntry != NULL; psEntry = psEntry->psNext )
        {
            GDALColorEntry sCEntry;

            sCEntry.c1 = (short) atoi(CPLGetXMLValue( psEntry, "c1", "0" ));
            sCEntry.c2 = (short) atoi(CPLGetXMLValue( psEntry, "c2", "0" ));
            sCEntry.c3 = (short) atoi(CPLGetXMLValue( psEntry, "c3", "0" ));
            sCEntry.c4 = (short) atoi(CPLGetXMLValue( psEntry, "c4", "255" ));

            oTable.SetColorEntry( iEntry++, &sCEntry );
        }
        
        SetColorTable( &oTable );
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
        
        CPLErrorReset();
        poSource = poDriver->ParseSource( psChild );
        if( poSource != NULL )
            AddSource( poSource );
        else if( CPLGetLastErrorType() != CE_None )
            return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Done.                                                           */
/* -------------------------------------------------------------------- */
    if( nSources > 0 )
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTRasterBand::SerializeToXML()

{
    CPLXMLNode *psTree;

    psTree = CPLCreateXMLNode( NULL, CXT_Element, "VRTRasterBand" );

/* -------------------------------------------------------------------- */
/*      Various kinds of metadata.                                      */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psMD;

    CPLSetXMLValue( psTree, "#dataType", 
                    GDALGetDataTypeName( GetRasterDataType() ) );

    if( nBand > 0 )
        CPLSetXMLValue( psTree, "#band", CPLSPrintf( "%d", GetBand() ) );

    psMD = VRTSerializeMetadata( this );
    if( psMD != NULL )
        CPLAddXMLChild( psTree, psMD );

    if( strlen(GetDescription()) > 0 )
        CPLSetXMLValue( psTree, "Description", GetDescription() );

    if( bNoDataValueSet )
        CPLSetXMLValue( psTree, "NoDataValue", 
                        CPLSPrintf( "%.14E", dfNoDataValue ) );

    if( eColorInterp != GCI_Undefined )
        CPLSetXMLValue( psTree, "ColorInterp", 
                        GDALGetColorInterpretationName( eColorInterp ) );

/* -------------------------------------------------------------------- */
/*      Color Table.                                                    */
/* -------------------------------------------------------------------- */
    if( poColorTable != NULL )
    {
        CPLXMLNode *psCT_XML = CPLCreateXMLNode( psTree, CXT_Element, 
                                                 "ColorTable" );

        for( int iEntry=0; iEntry < poColorTable->GetColorEntryCount(); 
             iEntry++ )
        {
            GDALColorEntry sEntry;
            CPLXMLNode *psEntry_XML = CPLCreateXMLNode( psCT_XML, CXT_Element, 
                                                        "Entry" );

            poColorTable->GetColorEntryAsRGB( iEntry, &sEntry );
            
            CPLSetXMLValue( psEntry_XML, "#c1", CPLSPrintf("%d",sEntry.c1) );
            CPLSetXMLValue( psEntry_XML, "#c2", CPLSPrintf("%d",sEntry.c2) );
            CPLSetXMLValue( psEntry_XML, "#c3", CPLSPrintf("%d",sEntry.c3) );
            CPLSetXMLValue( psEntry_XML, "#c4", CPLSPrintf("%d",sEntry.c4) );
        }
    }

/* -------------------------------------------------------------------- */
/*      Process SimpleSources.                                          */
/* -------------------------------------------------------------------- */
    for( int iSource = 0; iSource < nSources; iSource++ )
    {
        CPLXMLNode      *psXMLSrc;

        psXMLSrc = papoSources[iSource]->SerializeToXML();
        
        if( psXMLSrc != NULL )
            CPLAddXMLChild( psTree, psXMLSrc );
    }

    return psTree;
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **VRTRasterBand::GetMetadata( const char *pszDomain )

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

            psXMLSrc = papoSources[iSource]->SerializeToXML();
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
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr VRTRasterBand::SetMetadata( char **papszNewMD, const char *pszDomain )

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

            poSource = poDriver->ParseSource( psTree );
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
        return GDALRasterBand::SetMetadata( papszNewMD, pszDomain );
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr VRTRasterBand::SetNoDataValue( double dfNewValue )

{
    bNoDataValueSet = TRUE;
    dfNoDataValue = dfNewValue;
    
    ((VRTDataset *)poDS)->SetNeedsFlush();

    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double VRTRasterBand::GetNoDataValue( int *pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = bNoDataValueSet;

    return dfNoDataValue;
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr VRTRasterBand::SetColorTable( GDALColorTable *poTableIn )

{
    if( poColorTable != NULL )
    {
        delete poColorTable;
        poColorTable = NULL;
    }

    if( poTableIn )
    {
        poColorTable = poTableIn->Clone();
        eColorInterp = GCI_PaletteIndex;
    }

    ((VRTDataset *)poDS)->SetNeedsFlush();

    return CE_None;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *VRTRasterBand::GetColorTable()

{
    return poColorTable;
}

/************************************************************************/
/*                       SetColorInterpretation()                       */
/************************************************************************/

CPLErr VRTRasterBand::SetColorInterpretation( GDALColorInterp eInterpIn )

{
    ((VRTDataset *)poDS)->SetNeedsFlush();

    eColorInterp = eInterpIn;

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp VRTRasterBand::GetColorInterpretation()

{
    return eColorInterp;
}

/************************************************************************/
/*                          AddSimpleSource()                           */
/************************************************************************/

CPLErr VRTRasterBand::AddSimpleSource( GDALRasterBand *poSrcBand, 
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
/*                          AddComplexSource()                          */
/************************************************************************/

CPLErr VRTRasterBand::AddComplexSource( GDALRasterBand *poSrcBand, 
                                        int nSrcXOff, int nSrcYOff, 
                                        int nSrcXSize, int nSrcYSize, 
                                        int nDstXOff, int nDstYOff, 
                                        int nDstXSize, int nDstYSize,
                                        double dfScaleOff, 
                                        double dfScaleRatio,
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
/*                           AddFuncSource()                            */
/************************************************************************/

CPLErr VRTRasterBand::AddFuncSource( VRTImageReadFunc pfnReadFunc, 
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

