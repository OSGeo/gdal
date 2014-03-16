/******************************************************************************
 * $Id$
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Implementation of VRTRasterBand
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
/*                          VRTRasterBand                               */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           VRTRasterBand()                            */
/************************************************************************/

VRTRasterBand::VRTRasterBand()

{
    Initialize( 0, 0 );
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
    
    nBlockXSize = MIN(128,nXSize);
    nBlockYSize = MIN(128,nYSize);

    bIsMaskBand = FALSE;
    bNoDataValueSet = FALSE;
    bHideNoDataValue = FALSE;
    dfNoDataValue = -10000.0;
    poColorTable = NULL;
    eColorInterp = GCI_Undefined;

    pszUnitType = NULL;
    papszCategoryNames = NULL;
    dfOffset = 0.0;
    dfScale = 1.0;

    psSavedHistograms = NULL;

    poMaskBand = NULL;
}

/************************************************************************/
/*                           ~VRTRasterBand()                           */
/************************************************************************/

VRTRasterBand::~VRTRasterBand()

{
    CPLFree( pszUnitType );

    if( poColorTable != NULL )
        delete poColorTable;

    CSLDestroy( papszCategoryNames );
    if( psSavedHistograms != NULL )
        CPLDestroyXMLNode( psSavedHistograms );

    delete poMaskBand;
}

/************************************************************************/
/*                         CopyCommonInfoFrom()                         */
/*                                                                      */
/*      Copy common metadata, pixel descriptions, and color             */
/*      interpretation from the provided source band.                   */
/************************************************************************/

CPLErr VRTRasterBand::CopyCommonInfoFrom( GDALRasterBand * poSrcBand )

{
    int bSuccess;
    double dfNoData;

    SetMetadata( poSrcBand->GetMetadata() );
    SetColorTable( poSrcBand->GetColorTable() );
    SetColorInterpretation(poSrcBand->GetColorInterpretation());
    if( strlen(poSrcBand->GetDescription()) > 0 )
        SetDescription( poSrcBand->GetDescription() );
    dfNoData = poSrcBand->GetNoDataValue( &bSuccess );
    if( bSuccess )
        SetNoDataValue( dfNoData );

    SetOffset( poSrcBand->GetOffset() );
    SetScale( poSrcBand->GetScale() );
    SetCategoryNames( poSrcBand->GetCategoryNames() );
    if( !EQUAL(poSrcBand->GetUnitType(),"") )
        SetUnitType( poSrcBand->GetUnitType() );

    return CE_None;
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr VRTRasterBand::SetMetadata( char **papszMetadata, 
                                   const char *pszDomain )

{
    ((VRTDataset *) poDS)->SetNeedsFlush();

    return GDALRasterBand::SetMetadata( papszMetadata, pszDomain );
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr VRTRasterBand::SetMetadataItem( const char *pszName, 
                                       const char *pszValue, 
                                       const char *pszDomain )

{
    ((VRTDataset *) poDS)->SetNeedsFlush();

    if( EQUAL(pszName,"HideNoDataValue") )
    {
        bHideNoDataValue = CSLTestBoolean( pszValue );
        return CE_None;
    }
    else
        return GDALRasterBand::SetMetadataItem( pszName, pszValue, pszDomain );
}

/************************************************************************/
/*                            GetUnitType()                             */
/************************************************************************/

const char *VRTRasterBand::GetUnitType()

{
    if( pszUnitType == NULL )
        return "";
    else
        return pszUnitType;
}

/************************************************************************/
/*                            SetUnitType()                             */
/************************************************************************/

CPLErr VRTRasterBand::SetUnitType( const char *pszNewValue )

{
    ((VRTDataset *) poDS)->SetNeedsFlush();

    CPLFree( pszUnitType );
    
    if( pszNewValue == NULL )
        pszUnitType = NULL;
    else
        pszUnitType = CPLStrdup(pszNewValue);

    return CE_None;
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

double VRTRasterBand::GetOffset( int *pbSuccess )

{
    if( pbSuccess != NULL )
        *pbSuccess = TRUE;

    return dfOffset;
}

/************************************************************************/
/*                             SetOffset()                              */
/************************************************************************/

CPLErr VRTRasterBand::SetOffset( double dfNewOffset )

{
    ((VRTDataset *) poDS)->SetNeedsFlush();

    dfOffset = dfNewOffset;
    return CE_None;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double VRTRasterBand::GetScale( int *pbSuccess )

{
    if( pbSuccess != NULL )
        *pbSuccess = TRUE;

    return dfScale;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr VRTRasterBand::SetScale( double dfNewScale )

{
    ((VRTDataset *) poDS)->SetNeedsFlush();

    dfScale = dfNewScale;
    return CE_None;
}

/************************************************************************/
/*                          GetCategoryNames()                          */
/************************************************************************/

char **VRTRasterBand::GetCategoryNames()

{
    return papszCategoryNames;
}

/************************************************************************/
/*                          SetCategoryNames()                          */
/************************************************************************/

CPLErr VRTRasterBand::SetCategoryNames( char ** papszNewNames )

{
    ((VRTDataset *) poDS)->SetNeedsFlush();

    CSLDestroy( papszCategoryNames );
    papszCategoryNames = CSLDuplicate( papszNewNames );

    return CE_None;
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr VRTRasterBand::XMLInit( CPLXMLNode * psTree, 
                               const char *pszVRTPath )

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
    const char* pszBand = CPLGetXMLValue( psTree, "band", NULL);
    if( pszBand != NULL )
    {
        nBand = atoi(pszBand);
    }

/* -------------------------------------------------------------------- */
/*      Set the band if provided as an attribute.                       */
/* -------------------------------------------------------------------- */
    const char *pszDataType = CPLGetXMLValue( psTree, "dataType", NULL);
    if( pszDataType != NULL )
    {
        eDataType = GDALGetDataTypeByName(pszDataType);
    }

/* -------------------------------------------------------------------- */
/*      Apply any band level metadata.                                  */
/* -------------------------------------------------------------------- */
    oMDMD.XMLInit( psTree, TRUE );

/* -------------------------------------------------------------------- */
/*      Collect various other items of metadata.                        */
/* -------------------------------------------------------------------- */
    SetDescription( CPLGetXMLValue( psTree, "Description", "" ) );
    
    if( CPLGetXMLValue( psTree, "NoDataValue", NULL ) != NULL )
        SetNoDataValue( CPLAtofM(CPLGetXMLValue( psTree, "NoDataValue", "0" )) );

    if( CPLGetXMLValue( psTree, "HideNoDataValue", NULL ) != NULL )
        bHideNoDataValue = CSLTestBoolean( CPLGetXMLValue( psTree, "HideNoDataValue", "0" ) );

    SetUnitType( CPLGetXMLValue( psTree, "UnitType", NULL ) );

    SetOffset( atof(CPLGetXMLValue( psTree, "Offset", "0.0" )) );
    SetScale( atof(CPLGetXMLValue( psTree, "Scale", "1.0" )) );

    if( CPLGetXMLValue( psTree, "ColorInterp", NULL ) != NULL )
    {
        const char *pszInterp = CPLGetXMLValue( psTree, "ColorInterp", NULL );
        SetColorInterpretation(GDALGetColorInterpretationByName(pszInterp));
    }

/* -------------------------------------------------------------------- */
/*      Category names.                                                 */
/* -------------------------------------------------------------------- */
    if( CPLGetXMLNode( psTree, "CategoryNames" ) != NULL )
    {
        CPLXMLNode *psEntry;

        CSLDestroy( papszCategoryNames );
        papszCategoryNames = NULL;

        CPLStringList oCategoryNames;

        for( psEntry = CPLGetXMLNode( psTree, "CategoryNames" )->psChild;
             psEntry != NULL; psEntry = psEntry->psNext )
        {
            if( psEntry->eType != CXT_Element 
                || !EQUAL(psEntry->pszValue,"Category") 
                || (psEntry->psChild != NULL && psEntry->psChild->eType != CXT_Text) )
                continue;
            
            oCategoryNames.AddString(
                                (psEntry->psChild) ? psEntry->psChild->pszValue : "");
        }

        papszCategoryNames = oCategoryNames.StealList();
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
/*      Histograms                                                      */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psHist = CPLGetXMLNode( psTree, "Histograms" );
    if( psHist != NULL )
    {
        CPLXMLNode *psNext = psHist->psNext;
        psHist->psNext = NULL;

        psSavedHistograms = CPLCloneXMLTree( psHist );
        psHist->psNext = psNext;
    }

/* ==================================================================== */
/*      Overviews                                                       */
/* ==================================================================== */
    CPLXMLNode *psNode;

    for( psNode = psTree->psChild; psNode != NULL; psNode = psNode->psNext )
    {
        if( psNode->eType != CXT_Element
            || !EQUAL(psNode->pszValue,"Overview") )
            continue;

/* -------------------------------------------------------------------- */
/*      Prepare filename.                                               */
/* -------------------------------------------------------------------- */
        char *pszSrcDSName = NULL;
        CPLXMLNode* psFileNameNode=CPLGetXMLNode(psNode,"SourceFilename");
        const char *pszFilename = 
            psFileNameNode ? CPLGetXMLValue(psFileNameNode,NULL, NULL) : NULL;

        if( pszFilename == NULL )
        {
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "Missing <SourceFilename> element in Overview." );
            return CE_Failure;
        }

        if (EQUALN(pszFilename, "MEM:::", 6) && pszVRTPath != NULL &&
            !CSLTestBoolean(CPLGetConfigOption("VRT_ALLOW_MEM_DRIVER", "NO")))
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "<SourceFilename> points to a MEM dataset, which is rather suspect! "
                    "If you know what you are doing, define the VRT_ALLOW_MEM_DRIVER configuration option to YES" );
            return CE_Failure;
        }

        if( pszVRTPath != NULL
            && atoi(CPLGetXMLValue( psFileNameNode, "relativetoVRT", "0")) )
        {
            pszSrcDSName = CPLStrdup(
                CPLProjectRelativeFilename( pszVRTPath, pszFilename ) );
        }
        else
            pszSrcDSName = CPLStrdup( pszFilename );

/* -------------------------------------------------------------------- */
/*      Get the raster band.                                            */
/* -------------------------------------------------------------------- */
        int nSrcBand = atoi(CPLGetXMLValue(psNode,"SourceBand","1"));

        apoOverviews.resize( apoOverviews.size() + 1 );
        apoOverviews[apoOverviews.size()-1].osFilename = pszSrcDSName;
        apoOverviews[apoOverviews.size()-1].nBand = nSrcBand;
        
        CPLFree( pszSrcDSName );
    }

/* ==================================================================== */
/*      Mask band (specific to that raster band)                        */
/* ==================================================================== */
    CPLXMLNode* psMaskBandNode = CPLGetXMLNode(psTree, "MaskBand");
    if (psMaskBandNode)
        psNode = psMaskBandNode->psChild;
    else
        psNode = NULL;
    for( ; psNode != NULL; psNode = psNode->psNext )
    {
        if( psNode->eType != CXT_Element
            || !EQUAL(psNode->pszValue,"VRTRasterBand") )
            continue;

        if( ((VRTDataset*)poDS)->poMaskBand != NULL)
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                       "Illegal mask band at raster band level when a dataset mask band already exists." );
            break;
        }

        const char *pszSubclass = CPLGetXMLValue( psNode, "subclass",
                                                    "VRTSourcedRasterBand" );
        VRTRasterBand  *poBand = NULL;

        if( EQUAL(pszSubclass,"VRTSourcedRasterBand") )
            poBand = new VRTSourcedRasterBand( GetDataset(), 0 );
        else if( EQUAL(pszSubclass, "VRTDerivedRasterBand") )
            poBand = new VRTDerivedRasterBand( GetDataset(), 0 );
        else if( EQUAL(pszSubclass, "VRTRawRasterBand") )
            poBand = new VRTRawRasterBand( GetDataset(), 0 );
        else if( EQUAL(pszSubclass, "VRTWarpedRasterBand") )
            poBand = new VRTWarpedRasterBand( GetDataset(), 0 );
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                        "VRTRasterBand of unrecognised subclass '%s'.",
                        pszSubclass );
            break;
        }


        if( poBand->XMLInit( psNode, pszVRTPath ) == CE_None )
        {
            SetMaskBand(poBand);
        }

        break;
    }

    return CE_None;
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTRasterBand::SerializeToXML( const char *pszVRTPath )

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

    psMD = oMDMD.Serialize();
    if( psMD != NULL )
        CPLAddXMLChild( psTree, psMD );

    if( strlen(GetDescription()) > 0 )
        CPLSetXMLValue( psTree, "Description", GetDescription() );

    if( bNoDataValueSet )
    {
        if (CPLIsNan(dfNoDataValue))
            CPLSetXMLValue( psTree, "NoDataValue", "nan");
        else
            CPLSetXMLValue( psTree, "NoDataValue", 
                            CPLSPrintf( "%.14E", dfNoDataValue ) );
    }
    
    if( bHideNoDataValue )
        CPLSetXMLValue( psTree, "HideNoDataValue", 
                        CPLSPrintf( "%d", bHideNoDataValue ) );

    if( pszUnitType != NULL )
        CPLSetXMLValue( psTree, "UnitType", pszUnitType );

    if( dfOffset != 0.0 )
        CPLSetXMLValue( psTree, "Offset", 
                        CPLSPrintf( "%.16g", dfOffset ) );

    if( dfScale != 1.0 )
        CPLSetXMLValue( psTree, "Scale", 
                        CPLSPrintf( "%.16g", dfScale ) );

    if( eColorInterp != GCI_Undefined )
        CPLSetXMLValue( psTree, "ColorInterp", 
                        GDALGetColorInterpretationName( eColorInterp ) );

/* -------------------------------------------------------------------- */
/*      Category names.                                                 */
/* -------------------------------------------------------------------- */
    if( papszCategoryNames != NULL )
    {
        CPLXMLNode *psCT_XML = CPLCreateXMLNode( psTree, CXT_Element, 
                                                 "CategoryNames" );
        CPLXMLNode* psLastChild = NULL;

        for( int iEntry=0; papszCategoryNames[iEntry] != NULL; iEntry++ )
        {
            CPLXMLNode *psNode = CPLCreateXMLElementAndValue( NULL, "Category",
                                         papszCategoryNames[iEntry] );
            if( psLastChild == NULL )
                psCT_XML->psChild = psNode;
            else
                psLastChild->psNext = psNode;
            psLastChild = psNode;
        }
    }

/* -------------------------------------------------------------------- */
/*      Histograms.                                                     */
/* -------------------------------------------------------------------- */
    if( psSavedHistograms != NULL )
        CPLAddXMLChild( psTree, CPLCloneXMLTree( psSavedHistograms ) );

/* -------------------------------------------------------------------- */
/*      Color Table.                                                    */
/* -------------------------------------------------------------------- */
    if( poColorTable != NULL )
    {
        CPLXMLNode *psCT_XML = CPLCreateXMLNode( psTree, CXT_Element, 
                                                 "ColorTable" );
        CPLXMLNode* psLastChild = NULL;

        for( int iEntry=0; iEntry < poColorTable->GetColorEntryCount(); 
             iEntry++ )
        {
            GDALColorEntry sEntry;
            CPLXMLNode *psEntry_XML = CPLCreateXMLNode( NULL, CXT_Element,
                                                        "Entry" );
            if( psLastChild == NULL )
                psCT_XML->psChild = psEntry_XML;
            else
                psLastChild->psNext = psEntry_XML;
            psLastChild = psEntry_XML;

            poColorTable->GetColorEntryAsRGB( iEntry, &sEntry );
            
            CPLSetXMLValue( psEntry_XML, "#c1", CPLSPrintf("%d",sEntry.c1) );
            CPLSetXMLValue( psEntry_XML, "#c2", CPLSPrintf("%d",sEntry.c2) );
            CPLSetXMLValue( psEntry_XML, "#c3", CPLSPrintf("%d",sEntry.c3) );
            CPLSetXMLValue( psEntry_XML, "#c4", CPLSPrintf("%d",sEntry.c4) );
        }
    }

/* ==================================================================== */
/*      Overviews                                                       */
/* ==================================================================== */

    for( int iOvr = 0; iOvr < (int)apoOverviews.size(); iOvr ++ )
    {
        CPLXMLNode *psOVR_XML = CPLCreateXMLNode( psTree, CXT_Element,
                                                 "Overview" );

        int              bRelativeToVRT;
        const char      *pszRelativePath;
        VSIStatBufL sStat;

        if( VSIStatExL( apoOverviews[iOvr].osFilename, &sStat, VSI_STAT_EXISTS_FLAG ) != 0 )
        {
            pszRelativePath = apoOverviews[iOvr].osFilename;
            bRelativeToVRT = FALSE;
        }
        else
        {
            pszRelativePath =
                CPLExtractRelativePath( pszVRTPath, apoOverviews[iOvr].osFilename,
                                        &bRelativeToVRT );
        }

        CPLSetXMLValue( psOVR_XML, "SourceFilename", pszRelativePath );

        CPLCreateXMLNode(
            CPLCreateXMLNode( CPLGetXMLNode( psOVR_XML, "SourceFilename" ),
                            CXT_Attribute, "relativeToVRT" ),
            CXT_Text, bRelativeToVRT ? "1" : "0" );

        CPLSetXMLValue( psOVR_XML, "SourceBand",
                        CPLSPrintf("%d",apoOverviews[iOvr].nBand) );
    }
    
/* ==================================================================== */
/*      Mask band (specific to that raster band)                        */
/* ==================================================================== */

    if( poMaskBand != NULL )
    {
        CPLXMLNode *psBandTree =
            poMaskBand->SerializeToXML(pszVRTPath);

        if( psBandTree != NULL )
        {
            CPLXMLNode *psMaskBandElement = CPLCreateXMLNode( psTree, CXT_Element, 
                                                              "MaskBand" );
            CPLAddXMLChild( psMaskBandElement, psBandTree );
        }
    }

    return psTree;
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
/*                         UnsetNoDataValue()                           */
/************************************************************************/

CPLErr VRTRasterBand::UnsetNoDataValue()
{
    bNoDataValueSet = FALSE;
    dfNoDataValue = -10000.0;

    ((VRTDataset *)poDS)->SetNeedsFlush();

    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double VRTRasterBand::GetNoDataValue( int *pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = bNoDataValueSet && !bHideNoDataValue;

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
/*                            GetHistogram()                            */
/************************************************************************/

CPLErr VRTRasterBand::GetHistogram( double dfMin, double dfMax,
                                    int nBuckets, int * panHistogram,
                                    int bIncludeOutOfRange, int bApproxOK,
                                    GDALProgressFunc pfnProgress, 
                                    void *pProgressData )

{
/* -------------------------------------------------------------------- */
/*      Check if we have a matching histogram.                          */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psHistItem;

    psHistItem = PamFindMatchingHistogram( psSavedHistograms, 
                                           dfMin, dfMax, nBuckets, 
                                           bIncludeOutOfRange, bApproxOK );
    if( psHistItem != NULL )
    {
        int *panTempHist = NULL;

        if( PamParseHistogram( psHistItem, &dfMin, &dfMax, &nBuckets, 
                               &panTempHist,
                               &bIncludeOutOfRange, &bApproxOK ) )
        {
            memcpy( panHistogram, panTempHist, sizeof(int) * nBuckets );
            CPLFree( panTempHist );
            return CE_None;
        }
    }

/* -------------------------------------------------------------------- */
/*      We don't have an existing histogram matching the request, so    */
/*      generate one manually.                                          */
/* -------------------------------------------------------------------- */
    CPLErr eErr;

    eErr = GDALRasterBand::GetHistogram( dfMin, dfMax, 
                                         nBuckets, panHistogram, 
                                         bIncludeOutOfRange, bApproxOK,
                                         pfnProgress, pProgressData );

/* -------------------------------------------------------------------- */
/*      Save an XML description of this histogram.                      */
/* -------------------------------------------------------------------- */
    if( eErr == CE_None )
    {
        CPLXMLNode *psXMLHist;

        psXMLHist = PamHistogramToXMLTree( dfMin, dfMax, nBuckets, 
                                           panHistogram, 
                                           bIncludeOutOfRange, bApproxOK );
        if( psXMLHist != NULL )
        {
            ((VRTDataset *) poDS)->SetNeedsFlush();

            if( psSavedHistograms == NULL )
                psSavedHistograms = CPLCreateXMLNode( NULL, CXT_Element,
                                                      "Histograms" );
            
            CPLAddXMLChild( psSavedHistograms, psXMLHist );
        }
    }

    return eErr;
}

/************************************************************************/
/*                        SetDefaultHistogram()                         */
/************************************************************************/

CPLErr VRTRasterBand::SetDefaultHistogram( double dfMin, double dfMax, 
                                           int nBuckets, int *panHistogram)

{
    CPLXMLNode *psNode;

/* -------------------------------------------------------------------- */
/*      Do we have a matching histogram we should replace?              */
/* -------------------------------------------------------------------- */
    psNode = PamFindMatchingHistogram( psSavedHistograms, 
                                       dfMin, dfMax, nBuckets,
                                       TRUE, TRUE );
    if( psNode != NULL )
    {
        /* blow this one away */
        CPLRemoveXMLChild( psSavedHistograms, psNode );
        CPLDestroyXMLNode( psNode );
    }

/* -------------------------------------------------------------------- */
/*      Translate into a histogram XML tree.                            */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psHistItem;

    psHistItem = PamHistogramToXMLTree( dfMin, dfMax, nBuckets, 
                                        panHistogram, TRUE, FALSE );
    if( psHistItem == NULL )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Insert our new default histogram at the front of the            */
/*      histogram list so that it will be the default histogram.        */
/* -------------------------------------------------------------------- */
    ((VRTDataset *) poDS)->SetNeedsFlush();

    if( psSavedHistograms == NULL )
        psSavedHistograms = CPLCreateXMLNode( NULL, CXT_Element,
                                              "Histograms" );
            
    psHistItem->psNext = psSavedHistograms->psChild;
    psSavedHistograms->psChild = psHistItem;
    
    return CE_None;
}

/************************************************************************/
/*                        GetDefaultHistogram()                         */
/************************************************************************/

CPLErr 
VRTRasterBand::GetDefaultHistogram( double *pdfMin, double *pdfMax, 
                                    int *pnBuckets, int **ppanHistogram, 
                                    int bForce,
                                    GDALProgressFunc pfnProgress, 
                                    void *pProgressData )
    
{
    if( psSavedHistograms != NULL )
    {
        CPLXMLNode *psXMLHist;

        for( psXMLHist = psSavedHistograms->psChild;
             psXMLHist != NULL; psXMLHist = psXMLHist->psNext )
        {
            int bApprox, bIncludeOutOfRange;

            if( psXMLHist->eType != CXT_Element
                || !EQUAL(psXMLHist->pszValue,"HistItem") )
                continue;

            if( PamParseHistogram( psXMLHist, pdfMin, pdfMax, pnBuckets, 
                                   ppanHistogram, &bIncludeOutOfRange,
                                   &bApprox ) )
                return CE_None;
            else
                return CE_Failure;
        }
    }

    return GDALRasterBand::GetDefaultHistogram( pdfMin, pdfMax, pnBuckets, 
                                                ppanHistogram, bForce, 
                                                pfnProgress,pProgressData);
}

/************************************************************************/
/*                             GetFileList()                            */
/************************************************************************/

void VRTRasterBand::GetFileList(char*** ppapszFileList, int *pnSize,
                                int *pnMaxSize, CPLHashSet* hSetFiles)
{
    for( unsigned int iOver = 0; iOver < apoOverviews.size(); iOver++ )
    {
        CPLString &osFilename = apoOverviews[iOver].osFilename;

/* -------------------------------------------------------------------- */
/*      Is the filename even a real filesystem object?                  */
/* -------------------------------------------------------------------- */
        VSIStatBufL  sStat;
        if( VSIStatL( osFilename, &sStat ) != 0 )
            return;
        
/* -------------------------------------------------------------------- */
/*      Is it already in the list ?                                     */
/* -------------------------------------------------------------------- */
        if( CPLHashSetLookup(hSetFiles, osFilename) != NULL )
            return;
        
/* -------------------------------------------------------------------- */
/*      Grow array if necessary                                         */
/* -------------------------------------------------------------------- */
        if (*pnSize + 1 >= *pnMaxSize)
        {
            *pnMaxSize = 2 + 2 * (*pnMaxSize);
            *ppapszFileList = (char **) CPLRealloc(
                *ppapszFileList, sizeof(char*)  * (*pnMaxSize) );
        }
            
/* -------------------------------------------------------------------- */
/*      Add the string to the list                                      */
/* -------------------------------------------------------------------- */
        (*ppapszFileList)[*pnSize] = CPLStrdup(osFilename);
        (*ppapszFileList)[(*pnSize + 1)] = NULL;
        CPLHashSetInsert(hSetFiles, (*ppapszFileList)[*pnSize]);
        
        (*pnSize) ++;
    }
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int VRTRasterBand::GetOverviewCount()

{
    if( apoOverviews.size() > 0 )
        return apoOverviews.size();
    else
        return GDALRasterBand::GetOverviewCount();
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *VRTRasterBand::GetOverview( int iOverview )

{
    if( apoOverviews.size() > 0 )
    {
        if( iOverview < 0 || iOverview >= (int) apoOverviews.size() )
            return NULL;

        if( apoOverviews[iOverview].poBand == NULL 
            && !apoOverviews[iOverview].bTriedToOpen )
        {
            apoOverviews[iOverview].bTriedToOpen = TRUE;

            GDALDataset *poSrcDS = (GDALDataset *)
                GDALOpenShared( apoOverviews[iOverview].osFilename, GA_ReadOnly );
            
            if( poSrcDS == NULL )
                return NULL;

            apoOverviews[iOverview].poBand = poSrcDS->GetRasterBand( 
                apoOverviews[iOverview].nBand );

            if (apoOverviews[iOverview].poBand == NULL)
            {
                GDALClose( (GDALDatasetH)poSrcDS );
            }
        }

        return apoOverviews[iOverview].poBand;
    }
    else
        return GDALRasterBand::GetOverview( iOverview );
}

/************************************************************************/
/*                          SetDescription()                            */
/************************************************************************/

void VRTRasterBand::SetDescription(const char* pszDescription)

{
    ((VRTDataset *)poDS)->SetNeedsFlush();

    GDALRasterBand::SetDescription(pszDescription);
}

/************************************************************************/
/*                          CreateMaskBand()                            */
/************************************************************************/

CPLErr VRTRasterBand::CreateMaskBand( int nFlags )
{
    VRTDataset* poGDS = (VRTDataset *)poDS;
    
    if (poGDS->poMaskBand)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Cannot create mask band at raster band level when a dataset mask band already exists." );
        return CE_Failure;
    }
    
    if (poMaskBand != NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "This VRT band has already a mask band");
        return CE_Failure;
    }
    
    if ((nFlags & GMF_PER_DATASET) != 0)
        return poGDS->CreateMaskBand(nFlags);

    SetMaskBand(new VRTSourcedRasterBand( poGDS, 0 ));

    return CE_None;
}

/************************************************************************/
/*                           GetMaskBand()                              */
/************************************************************************/

GDALRasterBand* VRTRasterBand::GetMaskBand()
{
    VRTDataset* poGDS = (VRTDataset *)poDS;
    if (poGDS->poMaskBand)
        return poGDS->poMaskBand;
    else if (poMaskBand)
        return poMaskBand;
    else
        return GDALRasterBand::GetMaskBand();
}

/************************************************************************/
/*                            GetMaskFlags()                            */
/************************************************************************/

int VRTRasterBand::GetMaskFlags()
{
    VRTDataset* poGDS = (VRTDataset *)poDS;
    if (poGDS->poMaskBand)
        return GMF_PER_DATASET;
    else if (poMaskBand)
        return 0;
    else
        return GDALRasterBand::GetMaskFlags();
}

/************************************************************************/
/*                           SetMaskBand()                              */
/************************************************************************/

void VRTRasterBand::SetMaskBand(VRTRasterBand* poMaskBand)
{
    delete this->poMaskBand;
    this->poMaskBand = poMaskBand;
    poMaskBand->SetIsMaskBand();
}

/************************************************************************/
/*                          SetIsMaskBand()                             */
/************************************************************************/

void VRTRasterBand::SetIsMaskBand()
{
    nBand = 0;
    bIsMaskBand = TRUE;
}

/************************************************************************/
/*                        CloseDependentDatasets()                      */
/************************************************************************/

int VRTRasterBand::CloseDependentDatasets()
{
    return FALSE;
}
