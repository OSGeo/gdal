/******************************************************************************
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

#include "cpl_port.h"
#include "vrtdataset.h"

#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <memory>
#include <vector>

#include "gdal.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_hash_set.h"
#include "cpl_minixml.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"

/*! @cond Doxygen_Suppress */

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*                          VRTRasterBand                               */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           VRTRasterBand()                            */
/************************************************************************/

VRTRasterBand::VRTRasterBand() :
    m_bIsMaskBand(FALSE),
    m_bNoDataValueSet(FALSE),
    m_bHideNoDataValue(FALSE),
    m_dfNoDataValue(-10000.0),
    m_poColorTable(NULL),
    m_eColorInterp(GCI_Undefined),
    m_pszUnitType(NULL),
    m_papszCategoryNames(NULL),
    m_dfOffset(0.0),
    m_dfScale(1.0),
    m_psSavedHistograms(NULL),
    m_poMaskBand(NULL)
{
    // Initialize( 0, 0 );
    poDS = NULL;
    nBand = 0;
    eAccess = GA_ReadOnly;
    eDataType = GDT_Byte;
    nRasterXSize = 0;
    nRasterYSize = 0;
    nBlockXSize = 0;
    nBlockYSize = 0;
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

    nBlockXSize = std::min( 128, nXSize );
    nBlockYSize = std::min( 128, nYSize );

    m_bIsMaskBand = FALSE;
    m_bNoDataValueSet = FALSE;
    m_bHideNoDataValue = FALSE;
    m_dfNoDataValue = -10000.0;
    m_poColorTable = NULL;
    m_eColorInterp = GCI_Undefined;

    m_pszUnitType = NULL;
    m_papszCategoryNames = NULL;
    m_dfOffset = 0.0;
    m_dfScale = 1.0;

    m_psSavedHistograms = NULL;

    m_poMaskBand = NULL;
}

/************************************************************************/
/*                           ~VRTRasterBand()                           */
/************************************************************************/

VRTRasterBand::~VRTRasterBand()

{
    CPLFree( m_pszUnitType );

    if( m_poColorTable != NULL )
        delete m_poColorTable;

    CSLDestroy( m_papszCategoryNames );
    if( m_psSavedHistograms != NULL )
        CPLDestroyXMLNode( m_psSavedHistograms );

    delete m_poMaskBand;
}

/************************************************************************/
/*                         CopyCommonInfoFrom()                         */
/*                                                                      */
/*      Copy common metadata, pixel descriptions, and color             */
/*      interpretation from the provided source band.                   */
/************************************************************************/

CPLErr VRTRasterBand::CopyCommonInfoFrom( GDALRasterBand * poSrcBand )

{
    SetMetadata( poSrcBand->GetMetadata() );
    const char* pszNBits = poSrcBand->GetMetadataItem("NBITS", "IMAGE_STRUCTURE");
    SetMetadataItem( "NBITS", pszNBits, "IMAGE_STRUCTURE" );
    const char* pszPixelType = poSrcBand->GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
    SetMetadataItem( "PIXELTYPE", pszPixelType, "IMAGE_STRUCTURE" );
    SetColorTable( poSrcBand->GetColorTable() );
    SetColorInterpretation(poSrcBand->GetColorInterpretation());
    if( strlen(poSrcBand->GetDescription()) > 0 )
        SetDescription( poSrcBand->GetDescription() );

    int bSuccess;
    double dfNoData;
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
    reinterpret_cast<VRTDataset *>( poDS )->SetNeedsFlush();

    return GDALRasterBand::SetMetadata( papszMetadata, pszDomain );
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr VRTRasterBand::SetMetadataItem( const char *pszName,
                                       const char *pszValue,
                                       const char *pszDomain )

{
    reinterpret_cast<VRTDataset *>( poDS )->SetNeedsFlush();

    if( EQUAL(pszName,"HideNoDataValue") )
    {
        m_bHideNoDataValue = CPLTestBool( pszValue );
        return CE_None;
    }

    return GDALRasterBand::SetMetadataItem( pszName, pszValue, pszDomain );
}

/************************************************************************/
/*                            GetUnitType()                             */
/************************************************************************/

const char *VRTRasterBand::GetUnitType()

{
    if( m_pszUnitType == NULL )
        return "";

    return m_pszUnitType;
}

/************************************************************************/
/*                            SetUnitType()                             */
/************************************************************************/

CPLErr VRTRasterBand::SetUnitType( const char *pszNewValue )

{
    reinterpret_cast<VRTDataset *>( poDS )->SetNeedsFlush();

    CPLFree( m_pszUnitType );

    if( pszNewValue == NULL )
        m_pszUnitType = NULL;
    else
        m_pszUnitType = CPLStrdup(pszNewValue);

    return CE_None;
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

double VRTRasterBand::GetOffset( int *pbSuccess )

{
    if( pbSuccess != NULL )
        *pbSuccess = TRUE;

    return m_dfOffset;
}

/************************************************************************/
/*                             SetOffset()                              */
/************************************************************************/

CPLErr VRTRasterBand::SetOffset( double dfNewOffset )

{
    reinterpret_cast<VRTDataset *>( poDS )->SetNeedsFlush();

    m_dfOffset = dfNewOffset;
    return CE_None;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double VRTRasterBand::GetScale( int *pbSuccess )

{
    if( pbSuccess != NULL )
        *pbSuccess = TRUE;

    return m_dfScale;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr VRTRasterBand::SetScale( double dfNewScale )

{
    reinterpret_cast<VRTDataset *>( poDS )->SetNeedsFlush();

    m_dfScale = dfNewScale;
    return CE_None;
}

/************************************************************************/
/*                          GetCategoryNames()                          */
/************************************************************************/

char **VRTRasterBand::GetCategoryNames()

{
    return m_papszCategoryNames;
}

/************************************************************************/
/*                          SetCategoryNames()                          */
/************************************************************************/

CPLErr VRTRasterBand::SetCategoryNames( char ** papszNewNames )

{
    reinterpret_cast<VRTDataset *>( poDS )->SetNeedsFlush();

    CSLDestroy( m_papszCategoryNames );
    m_papszCategoryNames = CSLDuplicate( papszNewNames );

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
        int nNewBand = atoi(pszBand);
        if( nNewBand != nBand )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Invalid band number. Got %s, expected %d. Ignoring "
                     "provided one, and using %d instead",
                     pszBand, nBand, nBand);
        }
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
        m_bHideNoDataValue = CPLTestBool( CPLGetXMLValue( psTree, "HideNoDataValue", "0" ) );

    SetUnitType( CPLGetXMLValue( psTree, "UnitType", NULL ) );

    SetOffset( CPLAtof(CPLGetXMLValue( psTree, "Offset", "0.0" )) );
    SetScale( CPLAtof(CPLGetXMLValue( psTree, "Scale", "1.0" )) );

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
        CSLDestroy( m_papszCategoryNames );
        m_papszCategoryNames = NULL;

        CPLStringList oCategoryNames;

        for( CPLXMLNode *psEntry = CPLGetXMLNode( psTree, "CategoryNames" )->psChild;
             psEntry != NULL; psEntry = psEntry->psNext )
        {
            if( psEntry->eType != CXT_Element
                || !EQUAL(psEntry->pszValue,"Category")
                || (psEntry->psChild != NULL && psEntry->psChild->eType != CXT_Text) )
                continue;

            oCategoryNames.AddString(
                                (psEntry->psChild) ? psEntry->psChild->pszValue : "");
        }

        m_papszCategoryNames = oCategoryNames.StealList();
    }

/* -------------------------------------------------------------------- */
/*      Collect a color table.                                          */
/* -------------------------------------------------------------------- */
    if( CPLGetXMLNode(psTree, "ColorTable") != NULL )
    {
        GDALColorTable oTable;
        int iEntry = 0;

        for( CPLXMLNode *psEntry = CPLGetXMLNode(psTree, "ColorTable")->psChild;
             psEntry != NULL; psEntry = psEntry->psNext )
        {
            if( psEntry->eType != CXT_Element ||
                !EQUAL(psEntry->pszValue, "Entry") )
            {
                continue;
            }

            const GDALColorEntry sCEntry = {
                static_cast<short>(atoi(CPLGetXMLValue(psEntry, "c1", "0"))),
                static_cast<short>(atoi(CPLGetXMLValue(psEntry, "c2", "0"))),
                static_cast<short>(atoi(CPLGetXMLValue(psEntry, "c3", "0"))),
                static_cast<short>(atoi(CPLGetXMLValue(psEntry, "c4", "255")))
            };

            oTable.SetColorEntry(iEntry++, &sCEntry);
        }

        SetColorTable(&oTable);
    }

/* -------------------------------------------------------------------- */
/*      Histograms                                                      */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psHist = CPLGetXMLNode( psTree, "Histograms" );
    if( psHist != NULL )
    {
        CPLXMLNode *psNext = psHist->psNext;
        psHist->psNext = NULL;

        m_psSavedHistograms = CPLCloneXMLTree( psHist );
        psHist->psNext = psNext;
    }

/* ==================================================================== */
/*      Overviews                                                       */
/* ==================================================================== */
    CPLXMLNode *psNode = psTree->psChild;

    for( ; psNode != NULL; psNode = psNode->psNext )
    {
        if( psNode->eType != CXT_Element
            || !EQUAL(psNode->pszValue,"Overview") )
            continue;

/* -------------------------------------------------------------------- */
/*      Prepare filename.                                               */
/* -------------------------------------------------------------------- */
        CPLXMLNode* psFileNameNode=CPLGetXMLNode(psNode,"SourceFilename");
        const char *pszFilename =
            psFileNameNode ? CPLGetXMLValue(psFileNameNode,NULL, NULL) : NULL;

        if( pszFilename == NULL )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Missing <SourceFilename> element in Overview." );
            return CE_Failure;
        }

        if (STARTS_WITH_CI(pszFilename, "MEM:::") && pszVRTPath != NULL &&
            !CPLTestBool(CPLGetConfigOption("VRT_ALLOW_MEM_DRIVER", "NO")))
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "<SourceFilename> points to a MEM dataset, which is rather suspect! "
                    "If you know what you are doing, define the VRT_ALLOW_MEM_DRIVER configuration option to YES" );
            return CE_Failure;
        }

        char *pszSrcDSName = NULL;
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
        const int nSrcBand = atoi(CPLGetXMLValue( psNode, "SourceBand", "1" ) );

        m_apoOverviews.resize( m_apoOverviews.size() + 1 );
        m_apoOverviews.back().osFilename = pszSrcDSName;
        m_apoOverviews.back().nBand = nSrcBand;

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

        if( reinterpret_cast<VRTDataset *>( poDS )->m_poMaskBand != NULL)
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
                        "VRTRasterBand of unrecognized subclass '%s'.",
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
    CPLXMLNode *psTree = CPLCreateXMLNode( NULL, CXT_Element, "VRTRasterBand" );

/* -------------------------------------------------------------------- */
/*      Various kinds of metadata.                                      */
/* -------------------------------------------------------------------- */
    CPLSetXMLValue( psTree, "#dataType",
                    GDALGetDataTypeName( GetRasterDataType() ) );

    if( nBand > 0 )
        CPLSetXMLValue( psTree, "#band", CPLSPrintf( "%d", GetBand() ) );

    CPLXMLNode *psMD = oMDMD.Serialize();
    if( psMD != NULL )
    {
        CPLAddXMLChild( psTree, psMD );
    }

    if( strlen(GetDescription()) > 0 )
        CPLSetXMLValue( psTree, "Description", GetDescription() );

    if( m_bNoDataValueSet )
    {
        if (CPLIsNan(m_dfNoDataValue))
            CPLSetXMLValue( psTree, "NoDataValue", "nan");
        else
            CPLSetXMLValue( psTree, "NoDataValue",
                            CPLSPrintf( "%.16g", m_dfNoDataValue ) );
    }

    if( m_bHideNoDataValue )
        CPLSetXMLValue( psTree, "HideNoDataValue",
                        CPLSPrintf( "%d", m_bHideNoDataValue ) );

    if( m_pszUnitType != NULL )
        CPLSetXMLValue( psTree, "UnitType", m_pszUnitType );

    if( m_dfOffset != 0.0 )
        CPLSetXMLValue( psTree, "Offset",
                        CPLSPrintf( "%.16g", m_dfOffset ) );

    if( m_dfScale != 1.0 )
        CPLSetXMLValue( psTree, "Scale",
                        CPLSPrintf( "%.16g", m_dfScale ) );

    if( m_eColorInterp != GCI_Undefined )
        CPLSetXMLValue( psTree, "ColorInterp",
                        GDALGetColorInterpretationName( m_eColorInterp ) );

/* -------------------------------------------------------------------- */
/*      Category names.                                                 */
/* -------------------------------------------------------------------- */
    if( m_papszCategoryNames != NULL )
    {
        CPLXMLNode *psCT_XML = CPLCreateXMLNode( psTree, CXT_Element,
                                                 "CategoryNames" );
        CPLXMLNode* psLastChild = NULL;

        for( int iEntry=0; m_papszCategoryNames[iEntry] != NULL; iEntry++ )
        {
            CPLXMLNode *psNode = CPLCreateXMLElementAndValue( NULL, "Category",
                                         m_papszCategoryNames[iEntry] );
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
    if( m_psSavedHistograms != NULL )
        CPLAddXMLChild( psTree, CPLCloneXMLTree( m_psSavedHistograms ) );

/* -------------------------------------------------------------------- */
/*      Color Table.                                                    */
/* -------------------------------------------------------------------- */
    if( m_poColorTable != NULL )
    {
        CPLXMLNode *psCT_XML = CPLCreateXMLNode( psTree, CXT_Element,
                                                 "ColorTable" );
        CPLXMLNode* psLastChild = NULL;

        for( int iEntry=0; iEntry < m_poColorTable->GetColorEntryCount();
             iEntry++ )
        {
            CPLXMLNode *psEntry_XML = CPLCreateXMLNode( NULL, CXT_Element,
                                                        "Entry" );
            if( psLastChild == NULL )
                psCT_XML->psChild = psEntry_XML;
            else
                psLastChild->psNext = psEntry_XML;
            psLastChild = psEntry_XML;

            GDALColorEntry sEntry;
            m_poColorTable->GetColorEntryAsRGB( iEntry, &sEntry );

            CPLSetXMLValue( psEntry_XML, "#c1", CPLSPrintf("%d",sEntry.c1) );
            CPLSetXMLValue( psEntry_XML, "#c2", CPLSPrintf("%d",sEntry.c2) );
            CPLSetXMLValue( psEntry_XML, "#c3", CPLSPrintf("%d",sEntry.c3) );
            CPLSetXMLValue( psEntry_XML, "#c4", CPLSPrintf("%d",sEntry.c4) );
        }
    }

/* ==================================================================== */
/*      Overviews                                                       */
/* ==================================================================== */

    for( int iOvr = 0; iOvr < static_cast<int>( m_apoOverviews.size() ); iOvr ++ )
    {
        CPLXMLNode *psOVR_XML = CPLCreateXMLNode( psTree, CXT_Element,
                                                 "Overview" );

        int bRelativeToVRT = FALSE;
        const char *pszRelativePath = NULL;
        VSIStatBufL sStat;

        if( VSIStatExL( m_apoOverviews[iOvr].osFilename, &sStat, VSI_STAT_EXISTS_FLAG ) != 0 )
        {
            pszRelativePath = m_apoOverviews[iOvr].osFilename;
            bRelativeToVRT = FALSE;
        }
        else
        {
            pszRelativePath =
                CPLExtractRelativePath( pszVRTPath, m_apoOverviews[iOvr].osFilename,
                                        &bRelativeToVRT );
        }

        CPLSetXMLValue( psOVR_XML, "SourceFilename", pszRelativePath );

        CPLCreateXMLNode(
            CPLCreateXMLNode( CPLGetXMLNode( psOVR_XML, "SourceFilename" ),
                            CXT_Attribute, "relativeToVRT" ),
            CXT_Text, bRelativeToVRT ? "1" : "0" );

        CPLSetXMLValue( psOVR_XML, "SourceBand",
                        CPLSPrintf("%d",m_apoOverviews[iOvr].nBand) );
    }

/* ==================================================================== */
/*      Mask band (specific to that raster band)                        */
/* ==================================================================== */

    if( m_poMaskBand != NULL )
    {
        CPLXMLNode *psBandTree =
            m_poMaskBand->SerializeToXML(pszVRTPath);

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
    m_bNoDataValueSet = TRUE;
    m_dfNoDataValue = dfNewValue;

    reinterpret_cast<VRTDataset *>( poDS )->SetNeedsFlush();

    return CE_None;
}

/************************************************************************/
/*                         DeleteNoDataValue()                          */
/************************************************************************/

CPLErr VRTRasterBand::DeleteNoDataValue()
{
    m_bNoDataValueSet = FALSE;
    m_dfNoDataValue = -10000.0;

    reinterpret_cast<VRTDataset *>( poDS )->SetNeedsFlush();

    return CE_None;
}
/************************************************************************/
/*                         UnsetNoDataValue()                           */
/************************************************************************/

CPLErr VRTRasterBand::UnsetNoDataValue()
{
    return DeleteNoDataValue();
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double VRTRasterBand::GetNoDataValue( int *pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = m_bNoDataValueSet && !m_bHideNoDataValue;

    return m_dfNoDataValue;
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr VRTRasterBand::SetColorTable( GDALColorTable *poTableIn )

{
    if( m_poColorTable != NULL )
    {
        delete m_poColorTable;
        m_poColorTable = NULL;
    }

    if( poTableIn )
    {
        m_poColorTable = poTableIn->Clone();
        m_eColorInterp = GCI_PaletteIndex;
    }

    reinterpret_cast<VRTDataset *>( poDS )->SetNeedsFlush();

    return CE_None;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *VRTRasterBand::GetColorTable()

{
    return m_poColorTable;
}

/************************************************************************/
/*                       SetColorInterpretation()                       */
/************************************************************************/

CPLErr VRTRasterBand::SetColorInterpretation( GDALColorInterp eInterpIn )

{
    reinterpret_cast<VRTDataset *>( poDS )->SetNeedsFlush();

    m_eColorInterp = eInterpIn;

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp VRTRasterBand::GetColorInterpretation()

{
    return m_eColorInterp;
}

/************************************************************************/
/*                            GetHistogram()                            */
/************************************************************************/

CPLErr VRTRasterBand::GetHistogram( double dfMin, double dfMax,
                                    int nBuckets, GUIntBig * panHistogram,
                                    int bIncludeOutOfRange, int bApproxOK,
                                    GDALProgressFunc pfnProgress,
                                    void *pProgressData )

{
/* -------------------------------------------------------------------- */
/*      Check if we have a matching histogram.                          */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psHistItem = PamFindMatchingHistogram( m_psSavedHistograms,
                                           dfMin, dfMax, nBuckets,
                                           bIncludeOutOfRange, bApproxOK );
    if( psHistItem != NULL )
    {
        GUIntBig *panTempHist = NULL;

        if( PamParseHistogram( psHistItem, &dfMin, &dfMax, &nBuckets,
                               &panTempHist,
                               &bIncludeOutOfRange, &bApproxOK ) )
        {
            memcpy( panHistogram, panTempHist, sizeof(GUIntBig) * nBuckets );
            CPLFree( panTempHist );
            return CE_None;
        }
    }

/* -------------------------------------------------------------------- */
/*      We don't have an existing histogram matching the request, so    */
/*      generate one manually.                                          */
/* -------------------------------------------------------------------- */
    CPLErr eErr = GDALRasterBand::GetHistogram( dfMin, dfMax,
                                         nBuckets, panHistogram,
                                         bIncludeOutOfRange, bApproxOK,
                                         pfnProgress, pProgressData );

/* -------------------------------------------------------------------- */
/*      Save an XML description of this histogram.                      */
/* -------------------------------------------------------------------- */
    if( eErr == CE_None )
    {
        CPLXMLNode *psXMLHist = PamHistogramToXMLTree( dfMin, dfMax, nBuckets,
                                           panHistogram,
                                           bIncludeOutOfRange, bApproxOK );
        if( psXMLHist != NULL )
        {
            reinterpret_cast<VRTDataset *>( poDS )->SetNeedsFlush();

            if( m_psSavedHistograms == NULL )
                m_psSavedHistograms = CPLCreateXMLNode( NULL, CXT_Element,
                                                      "Histograms" );

            CPLAddXMLChild( m_psSavedHistograms, psXMLHist );
        }
    }

    return eErr;
}

/************************************************************************/
/*                        SetDefaultHistogram()                         */
/************************************************************************/

CPLErr VRTRasterBand::SetDefaultHistogram( double dfMin, double dfMax,
                                           int nBuckets, GUIntBig *panHistogram)

{
/* -------------------------------------------------------------------- */
/*      Do we have a matching histogram we should replace?              */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psNode = PamFindMatchingHistogram( m_psSavedHistograms,
                                       dfMin, dfMax, nBuckets,
                                       TRUE, TRUE );
    if( psNode != NULL )
    {
        /* blow this one away */
        CPLRemoveXMLChild( m_psSavedHistograms, psNode );
        CPLDestroyXMLNode( psNode );
    }

/* -------------------------------------------------------------------- */
/*      Translate into a histogram XML tree.                            */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psHistItem = PamHistogramToXMLTree( dfMin, dfMax, nBuckets,
                                        panHistogram, TRUE, FALSE );
    if( psHistItem == NULL )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Insert our new default histogram at the front of the            */
/*      histogram list so that it will be the default histogram.        */
/* -------------------------------------------------------------------- */
    reinterpret_cast<VRTDataset *>( poDS )->SetNeedsFlush();

    if( m_psSavedHistograms == NULL )
        m_psSavedHistograms = CPLCreateXMLNode( NULL, CXT_Element,
                                              "Histograms" );

    psHistItem->psNext = m_psSavedHistograms->psChild;
    m_psSavedHistograms->psChild = psHistItem;

    return CE_None;
}

/************************************************************************/
/*                        GetDefaultHistogram()                         */
/************************************************************************/

CPLErr
VRTRasterBand::GetDefaultHistogram( double *pdfMin, double *pdfMax,
                                    int *pnBuckets, GUIntBig **ppanHistogram,
                                    int bForce,
                                    GDALProgressFunc pfnProgress,
                                    void *pProgressData )

{
    if( m_psSavedHistograms != NULL )
    {
        for( CPLXMLNode *psXMLHist = m_psSavedHistograms->psChild;
             psXMLHist != NULL; psXMLHist = psXMLHist->psNext )
        {
            if( psXMLHist->eType != CXT_Element
                || !EQUAL(psXMLHist->pszValue,"HistItem") )
                continue;

            int bIncludeOutOfRange;
            int bApprox;
            if( PamParseHistogram( psXMLHist, pdfMin, pdfMax, pnBuckets,
                                   ppanHistogram, &bIncludeOutOfRange,
                                   &bApprox ) )
                return CE_None;

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
    for( unsigned int iOver = 0; iOver < m_apoOverviews.size(); iOver++ )
    {
        const CPLString &osFilename = m_apoOverviews[iOver].osFilename;

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
            *ppapszFileList = reinterpret_cast<char **>( CPLRealloc(
                *ppapszFileList, sizeof(char*)  * (*pnMaxSize) ) );
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
    // First: overviews declared in <Overview> element
    if( !m_apoOverviews.empty() )
        return static_cast<int>(m_apoOverviews.size());

    // If not found, external .ovr overviews
    const int nOverviewCount = GDALRasterBand::GetOverviewCount();
    if( nOverviewCount )
        return nOverviewCount;

    // If not found, implicit virtual overviews
    VRTDataset* poVRTDS = reinterpret_cast<VRTDataset *>( poDS );
    poVRTDS->BuildVirtualOverviews();
    if( !poVRTDS->m_apoOverviews.empty() && poVRTDS->m_apoOverviews[0] )
        return static_cast<int>( poVRTDS->m_apoOverviews.size() );

    return 0;
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *VRTRasterBand::GetOverview( int iOverview )

{
    // First: overviews declared in <Overview> element
    if( !m_apoOverviews.empty() )
    {
        if( iOverview < 0
            || iOverview >= static_cast<int>( m_apoOverviews.size() ) )
            return NULL;

        if( m_apoOverviews[iOverview].poBand == NULL
            && !m_apoOverviews[iOverview].bTriedToOpen )
        {
            m_apoOverviews[iOverview].bTriedToOpen = TRUE;
            CPLConfigOptionSetter oSetter("CPL_ALLOW_VSISTDIN", "NO", true);
            GDALDataset *poSrcDS = reinterpret_cast<GDALDataset *>(
                GDALOpenShared( m_apoOverviews[iOverview].osFilename,
                                GA_ReadOnly ) );

            if( poSrcDS == NULL )
                return NULL;

            m_apoOverviews[iOverview].poBand = poSrcDS->GetRasterBand(
                m_apoOverviews[iOverview].nBand );

            if (m_apoOverviews[iOverview].poBand == NULL)
            {
                GDALClose( (GDALDatasetH)poSrcDS );
            }
        }

        return m_apoOverviews[iOverview].poBand;
    }

    // If not found, external .ovr overviews
    GDALRasterBand* poRet = GDALRasterBand::GetOverview( iOverview );
    if( poRet )
        return poRet;

    // If not found, implicit virtual overviews
    VRTDataset* poVRTDS = reinterpret_cast<VRTDataset *>( poDS );
    poVRTDS->BuildVirtualOverviews();
    if( !poVRTDS->m_apoOverviews.empty() && poVRTDS->m_apoOverviews[0] )
    {
        if( iOverview < 0
            || iOverview >= static_cast<int>( poVRTDS->m_apoOverviews.size() ) )
            return NULL;

        return poVRTDS->m_apoOverviews[iOverview]->GetRasterBand(nBand);
    }

    return NULL;
}

/************************************************************************/
/*                          SetDescription()                            */
/************************************************************************/

void VRTRasterBand::SetDescription(const char* pszDescription)

{
    reinterpret_cast<VRTDataset *>( poDS )->SetNeedsFlush();

    GDALRasterBand::SetDescription(pszDescription);
}

/************************************************************************/
/*                          CreateMaskBand()                            */
/************************************************************************/

CPLErr VRTRasterBand::CreateMaskBand( int nFlagsIn )
{
    VRTDataset* poGDS = reinterpret_cast<VRTDataset *>( poDS );

    if (poGDS->m_poMaskBand)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Cannot create mask band at raster band level when a dataset mask band already exists." );
        return CE_Failure;
    }

    if (m_poMaskBand != NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "This VRT band has already a mask band");
        return CE_Failure;
    }

    if ((nFlagsIn & GMF_PER_DATASET) != 0)
        return poGDS->CreateMaskBand(nFlagsIn);

    SetMaskBand(new VRTSourcedRasterBand( poGDS, 0 ));

    return CE_None;
}

/************************************************************************/
/*                           GetMaskBand()                              */
/************************************************************************/

GDALRasterBand* VRTRasterBand::GetMaskBand()
{
    VRTDataset* poGDS = reinterpret_cast<VRTDataset *>( poDS );

    if (poGDS->m_poMaskBand)
        return poGDS->m_poMaskBand;
    else if (m_poMaskBand)
        return m_poMaskBand;
    else
        return GDALRasterBand::GetMaskBand();
}

/************************************************************************/
/*                            GetMaskFlags()                            */
/************************************************************************/

int VRTRasterBand::GetMaskFlags()
{
    VRTDataset* poGDS = reinterpret_cast<VRTDataset *>( poDS );

    if (poGDS->m_poMaskBand)
        return GMF_PER_DATASET;
    else if (m_poMaskBand)
        return 0;
    else
        return GDALRasterBand::GetMaskFlags();
}

/************************************************************************/
/*                           SetMaskBand()                              */
/************************************************************************/

void VRTRasterBand::SetMaskBand(VRTRasterBand* poMaskBandIn)
{
    delete m_poMaskBand;
    m_poMaskBand = poMaskBandIn;
    m_poMaskBand->SetIsMaskBand();
}

/************************************************************************/
/*                          SetIsMaskBand()                             */
/************************************************************************/

void VRTRasterBand::SetIsMaskBand()
{
    nBand = 0;
    m_bIsMaskBand = TRUE;
}

/************************************************************************/
/*                        CloseDependentDatasets()                      */
/************************************************************************/

int VRTRasterBand::CloseDependentDatasets()
{
    return FALSE;
}

/*! @endcond */
