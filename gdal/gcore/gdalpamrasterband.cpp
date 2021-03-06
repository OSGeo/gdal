/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALPamRasterBand, a raster band base class
 *           that knows how to persistently store auxiliary metadata in an
 *           external xml file.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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
#include "gdal_pam.h"

#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_rat.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                         GDALPamRasterBand()                          */
/************************************************************************/

GDALPamRasterBand::GDALPamRasterBand()

{
    SetMOFlags( GetMOFlags() | GMO_PAM_CLASS );
}

/************************************************************************/
/*                         GDALPamRasterBand()                          */
/************************************************************************/

//! @cond Doxygen_Suppress
GDALPamRasterBand::GDALPamRasterBand( int bForceCachedIOIn ) :
    GDALRasterBand(bForceCachedIOIn)
{
    SetMOFlags( GetMOFlags() | GMO_PAM_CLASS );
}
//! @endcond

/************************************************************************/
/*                         ~GDALPamRasterBand()                         */
/************************************************************************/

GDALPamRasterBand::~GDALPamRasterBand()

{
    PamClear();
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

//! @cond Doxygen_Suppress
CPLXMLNode *GDALPamRasterBand::SerializeToXML( const char * /* pszUnused */ )
{
    if( psPam == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Setup root node and attributes.                                 */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psTree = CPLCreateXMLNode( nullptr, CXT_Element, "PAMRasterBand" );

    CPLString oFmt;
    if( GetBand() > 0 )
        CPLSetXMLValue( psTree, "#band", oFmt.Printf( "%d", GetBand() ) );

/* -------------------------------------------------------------------- */
/*      Serialize information of interest.                              */
/* -------------------------------------------------------------------- */
    if( strlen(GetDescription()) > 0 )
        CPLSetXMLValue( psTree, "Description", GetDescription() );

    if( psPam->bNoDataValueSet )
    {
        if( CPLIsNan(psPam->dfNoDataValue) )
            CPLSetXMLValue( psTree, "NoDataValue",  "nan" );
        else
            CPLSetXMLValue( psTree, "NoDataValue",
                            oFmt.Printf( "%.14E", psPam->dfNoDataValue ) );

        // Hex encode real floating point values.
        if( psPam->dfNoDataValue != floor(psPam->dfNoDataValue )
            || psPam->dfNoDataValue != CPLAtof(oFmt) )
        {
            double dfNoDataLittleEndian = psPam->dfNoDataValue;
            CPL_LSBPTR64( &dfNoDataLittleEndian );

            char *pszHexEncoding =
                CPLBinaryToHex(
                    8, reinterpret_cast<GByte *>( &dfNoDataLittleEndian ) );
            CPLSetXMLValue( psTree, "NoDataValue.#le_hex_equiv",
                            pszHexEncoding );
            CPLFree( pszHexEncoding );
        }
    }

    if( psPam->pszUnitType != nullptr )
        CPLSetXMLValue( psTree, "UnitType", psPam->pszUnitType );

    if( psPam->dfOffset != 0.0 )
        CPLSetXMLValue( psTree, "Offset",
                        oFmt.Printf( "%.16g", psPam->dfOffset ) );

    if( psPam->dfScale != 1.0 )
        CPLSetXMLValue( psTree, "Scale",
                        oFmt.Printf( "%.16g", psPam->dfScale ) );

    if( psPam->eColorInterp != GCI_Undefined )
        CPLSetXMLValue( psTree, "ColorInterp",
                        GDALGetColorInterpretationName( psPam->eColorInterp ));

/* -------------------------------------------------------------------- */
/*      Category names.                                                 */
/* -------------------------------------------------------------------- */
    if( psPam->papszCategoryNames != nullptr )
    {
        CPLXMLNode *psCT_XML = CPLCreateXMLNode( psTree, CXT_Element,
                                                 "CategoryNames" );
        CPLXMLNode* psLastChild = nullptr;

        for( int iEntry=0; psPam->papszCategoryNames[iEntry] != nullptr; iEntry++)
        {
            CPLXMLNode *psNode = CPLCreateXMLElementAndValue(
                nullptr, "Category", psPam->papszCategoryNames[iEntry] );
            if( psLastChild == nullptr )
                psCT_XML->psChild = psNode;
            else
                psLastChild->psNext = psNode;
            psLastChild = psNode;
        }
    }

/* -------------------------------------------------------------------- */
/*      Color Table.                                                    */
/* -------------------------------------------------------------------- */
    if( psPam->poColorTable != nullptr )
    {
        CPLXMLNode *psCT_XML = CPLCreateXMLNode( psTree, CXT_Element,
                                                 "ColorTable" );
        CPLXMLNode* psLastChild = nullptr;

        for( int iEntry = 0;
             iEntry < psPam->poColorTable->GetColorEntryCount();
             iEntry++ )
        {
            CPLXMLNode *psEntry_XML = CPLCreateXMLNode( nullptr, CXT_Element,
                                                        "Entry" );
            if( psLastChild == nullptr )
                psCT_XML->psChild = psEntry_XML;
            else
                psLastChild->psNext = psEntry_XML;
            psLastChild = psEntry_XML;

            GDALColorEntry sEntry;
            psPam->poColorTable->GetColorEntryAsRGB( iEntry, &sEntry );

            CPLSetXMLValue( psEntry_XML, "#c1", oFmt.Printf("%d",sEntry.c1) );
            CPLSetXMLValue( psEntry_XML, "#c2", oFmt.Printf("%d",sEntry.c2) );
            CPLSetXMLValue( psEntry_XML, "#c3", oFmt.Printf("%d",sEntry.c3) );
            CPLSetXMLValue( psEntry_XML, "#c4", oFmt.Printf("%d",sEntry.c4) );
        }
    }

/* -------------------------------------------------------------------- */
/*      Min/max.                                                        */
/* -------------------------------------------------------------------- */
    if( psPam->bHaveMinMax )
    {
        CPLSetXMLValue( psTree, "Minimum",
                        oFmt.Printf( "%.16g", psPam->dfMin ) );
        CPLSetXMLValue( psTree, "Maximum",
                        oFmt.Printf( "%.16g", psPam->dfMax ) );
    }

/* -------------------------------------------------------------------- */
/*      Statistics                                                      */
/* -------------------------------------------------------------------- */
    if( psPam->bHaveStats )
    {
        CPLSetXMLValue( psTree, "Mean",
                        oFmt.Printf( "%.16g", psPam->dfMean ) );
        CPLSetXMLValue( psTree, "StandardDeviation",
                        oFmt.Printf( "%.16g", psPam->dfStdDev ) );
    }

/* -------------------------------------------------------------------- */
/*      Histograms.                                                     */
/* -------------------------------------------------------------------- */
    if( psPam->psSavedHistograms != nullptr )
        CPLAddXMLChild( psTree, CPLCloneXMLTree( psPam->psSavedHistograms ) );

/* -------------------------------------------------------------------- */
/*      Raster Attribute Table                                          */
/* -------------------------------------------------------------------- */
    if( psPam->poDefaultRAT != nullptr )
    {
        CPLXMLNode* psSerializedRAT = psPam->poDefaultRAT->Serialize();
        if( psSerializedRAT != nullptr )
            CPLAddXMLChild( psTree, psSerializedRAT );
    }

/* -------------------------------------------------------------------- */
/*      Metadata.                                                       */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psMD = oMDMD.Serialize();
    if( psMD != nullptr )
    {
        CPLAddXMLChild( psTree, psMD );
    }

/* -------------------------------------------------------------------- */
/*      We don't want to return anything if we had no metadata to       */
/*      attach.                                                         */
/* -------------------------------------------------------------------- */
    if( psTree->psChild == nullptr || psTree->psChild->psNext == nullptr )
    {
        CPLDestroyXMLNode( psTree );
        psTree = nullptr;
    }

    return psTree;
}

/************************************************************************/
/*                           PamInitialize()                            */
/************************************************************************/

void GDALPamRasterBand::PamInitialize()

{
    if( psPam )
        return;

    GDALDataset* poNonPamParentDS = GetDataset();
    if( poNonPamParentDS == nullptr ||
        !(poNonPamParentDS->GetMOFlags() & GMO_PAM_CLASS) )
        return;

    GDALPamDataset *poParentDS =
        dynamic_cast<GDALPamDataset *>( poNonPamParentDS );
    if( poParentDS == nullptr ) {
        // Should never happen.
        CPLAssert(false);
        return;
    }

    poParentDS->PamInitialize();
    if( poParentDS->psPam == nullptr )
        return;

    // Often (always?) initializing our parent will have initialized us.
    if( psPam != nullptr )
        return;

    psPam = static_cast<GDALRasterBandPamInfo *>(
        VSI_CALLOC_VERBOSE(sizeof(GDALRasterBandPamInfo), 1) );
    if( psPam == nullptr )
        return;

    psPam->dfScale = 1.0;
    psPam->poParentDS = poParentDS;
    psPam->dfNoDataValue = -1e10;
    psPam->poDefaultRAT = nullptr;
}

/************************************************************************/
/*                              PamClear()                              */
/************************************************************************/

void GDALPamRasterBand::PamClear()

{
    if( !psPam )
        return;

    if( psPam->poColorTable )
        delete psPam->poColorTable;
    psPam->poColorTable = nullptr;

    CPLFree( psPam->pszUnitType );
    CSLDestroy( psPam->papszCategoryNames );

    if( psPam->poDefaultRAT != nullptr )
    {
        delete psPam->poDefaultRAT;
        psPam->poDefaultRAT = nullptr;
    }

    if( psPam->psSavedHistograms != nullptr )
    {
        CPLDestroyXMLNode (psPam->psSavedHistograms );
        psPam->psSavedHistograms = nullptr;
    }

    CPLFree( psPam );
    psPam = nullptr;
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr GDALPamRasterBand::XMLInit( CPLXMLNode *psTree,
                                   const char * /* pszUnused */ )
{
    PamInitialize();

/* -------------------------------------------------------------------- */
/*      Apply any dataset level metadata.                               */
/* -------------------------------------------------------------------- */
    oMDMD.XMLInit( psTree, TRUE );

/* -------------------------------------------------------------------- */
/*      Collect various other items of metadata.                        */
/* -------------------------------------------------------------------- */
    GDALMajorObject::SetDescription(
        CPLGetXMLValue( psTree, "Description", "" ) );

    if( CPLGetXMLValue( psTree, "NoDataValue", nullptr ) != nullptr )
    {
        const char *pszLEHex =
            CPLGetXMLValue( psTree, "NoDataValue.le_hex_equiv", nullptr );
        if( pszLEHex != nullptr )
        {
            int nBytes;
            GByte *pabyBin = CPLHexToBinary( pszLEHex, &nBytes );
            if( nBytes == 8 )
            {
                CPL_LSBPTR64( pabyBin );

                GDALPamRasterBand::SetNoDataValue( *reinterpret_cast<const double*>(pabyBin) );
            }
            else
            {
                GDALPamRasterBand::SetNoDataValue(
                    CPLAtof(CPLGetXMLValue( psTree, "NoDataValue", "0" )) );
            }
            CPLFree( pabyBin );
        }
        else
        {
            GDALPamRasterBand::SetNoDataValue(
                CPLAtof(CPLGetXMLValue( psTree, "NoDataValue", "0" )) );
        }
    }

    GDALPamRasterBand::SetOffset(
        CPLAtof(CPLGetXMLValue( psTree, "Offset", "0.0" )) );
    GDALPamRasterBand::SetScale(
        CPLAtof(CPLGetXMLValue( psTree, "Scale", "1.0" )) );

    GDALPamRasterBand::SetUnitType( CPLGetXMLValue( psTree, "UnitType", nullptr));

    if( CPLGetXMLValue( psTree, "ColorInterp", nullptr ) != nullptr )
    {
        const char *pszInterp = CPLGetXMLValue( psTree, "ColorInterp", nullptr );
        GDALPamRasterBand::SetColorInterpretation(
            GDALGetColorInterpretationByName(pszInterp));
    }

/* -------------------------------------------------------------------- */
/*      Category names.                                                 */
/* -------------------------------------------------------------------- */
    if( CPLGetXMLNode( psTree, "CategoryNames" ) != nullptr )
    {
        CPLStringList oCategoryNames;

        for( CPLXMLNode *psEntry =
                 CPLGetXMLNode( psTree, "CategoryNames" )->psChild;
             psEntry != nullptr;
             psEntry = psEntry->psNext )
        {
            /* Don't skip <Category> tag with empty content */
            if( psEntry->eType != CXT_Element
                || !EQUAL(psEntry->pszValue,"Category")
                || (psEntry->psChild != nullptr &&
                    psEntry->psChild->eType != CXT_Text) )
                continue;

            oCategoryNames.AddString(
                psEntry->psChild ? psEntry->psChild->pszValue : "" );
        }

        GDALPamRasterBand::SetCategoryNames( oCategoryNames.List() );
    }

/* -------------------------------------------------------------------- */
/*      Collect a color table.                                          */
/* -------------------------------------------------------------------- */
    if( CPLGetXMLNode( psTree, "ColorTable" ) != nullptr )
    {
        GDALColorTable oTable;
        int iEntry = 0;

        for( CPLXMLNode *psEntry =
                 CPLGetXMLNode( psTree, "ColorTable" )->psChild;
             psEntry != nullptr;
             psEntry = psEntry->psNext )
        {
            if( !(psEntry->eType == CXT_Element &&
                  EQUAL(psEntry->pszValue, "Entry")) )
            {
                continue;
            }

            GDALColorEntry sCEntry = {
                static_cast<short>(atoi(CPLGetXMLValue( psEntry, "c1", "0" ))),
                static_cast<short>(atoi(CPLGetXMLValue( psEntry, "c2", "0" ))),
                static_cast<short>(atoi(CPLGetXMLValue( psEntry, "c3", "0" ))),
                static_cast<short>(atoi(CPLGetXMLValue( psEntry, "c4", "255" )))
            };

            oTable.SetColorEntry( iEntry++, &sCEntry );
        }

        GDALPamRasterBand::SetColorTable( &oTable );
    }

/* -------------------------------------------------------------------- */
/*      Do we have a complete set of stats?                             */
/* -------------------------------------------------------------------- */
    if( CPLGetXMLNode( psTree, "Minimum" ) != nullptr
        && CPLGetXMLNode( psTree, "Maximum" ) != nullptr )
    {
        psPam->bHaveMinMax = TRUE;
        psPam->dfMin = CPLAtofM(CPLGetXMLValue(psTree, "Minimum","0"));
        psPam->dfMax = CPLAtofM(CPLGetXMLValue(psTree, "Maximum","0"));
    }

    if( CPLGetXMLNode( psTree, "Mean" ) != nullptr
        && CPLGetXMLNode( psTree, "StandardDeviation" ) != nullptr )
    {
        psPam->bHaveStats = TRUE;
        psPam->dfMean = CPLAtofM(CPLGetXMLValue(psTree, "Mean","0"));
        psPam->dfStdDev =
            CPLAtofM(CPLGetXMLValue(psTree, "StandardDeviation", "0"));
    }

/* -------------------------------------------------------------------- */
/*      Histograms                                                      */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psHist = CPLGetXMLNode( psTree, "Histograms" );
    if( psHist != nullptr )
    {
        CPLXMLNode *psNext = psHist->psNext;
        psHist->psNext = nullptr;

        if( psPam->psSavedHistograms != nullptr )
        {
            CPLDestroyXMLNode (psPam->psSavedHistograms );
            psPam->psSavedHistograms = nullptr;
        }
        psPam->psSavedHistograms = CPLCloneXMLTree( psHist );
        psHist->psNext = psNext;
    }

/* -------------------------------------------------------------------- */
/*      Raster Attribute Table                                          */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psRAT = CPLGetXMLNode( psTree, "GDALRasterAttributeTable" );
    if( psRAT != nullptr )
    {
        if( psPam->poDefaultRAT != nullptr )
        {
            delete psPam->poDefaultRAT;
            psPam->poDefaultRAT = nullptr;
        }
        psPam->poDefaultRAT = new GDALDefaultRasterAttributeTable();
        psPam->poDefaultRAT->XMLInit( psRAT, "" );
    }

    return CE_None;
}

/************************************************************************/
/*                             CloneInfo()                              */
/************************************************************************/

CPLErr GDALPamRasterBand::CloneInfo( GDALRasterBand *poSrcBand,
                                     int nCloneFlags )

{
    const bool bOnlyIfMissing = (nCloneFlags & GCIF_ONLY_IF_MISSING) != 0;
    const int nSavedMOFlags = GetMOFlags();

    PamInitialize();

/* -------------------------------------------------------------------- */
/*      Suppress NotImplemented error messages - mainly needed if PAM   */
/*      disabled.                                                       */
/* -------------------------------------------------------------------- */
    SetMOFlags( nSavedMOFlags | GMO_IGNORE_UNIMPLEMENTED );

/* -------------------------------------------------------------------- */
/*      Metadata                                                        */
/* -------------------------------------------------------------------- */
    if( nCloneFlags & GCIF_BAND_METADATA )
    {
        if( poSrcBand->GetMetadata() != nullptr )
        {
            if( !bOnlyIfMissing
                || CSLCount(GetMetadata()) !=
                CSLCount(poSrcBand->GetMetadata()) )
            {
                SetMetadata( poSrcBand->GetMetadata() );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Band description.                                               */
/* -------------------------------------------------------------------- */
    if( nCloneFlags & GCIF_BAND_DESCRIPTION )
    {
        if( strlen(poSrcBand->GetDescription()) > 0 )
        {
            if( !bOnlyIfMissing || strlen(GetDescription()) == 0 )
                GDALPamRasterBand::SetDescription( poSrcBand->GetDescription());
        }
    }

/* -------------------------------------------------------------------- */
/*      NODATA                                                          */
/* -------------------------------------------------------------------- */
    if( nCloneFlags & GCIF_NODATA )
    {
        int bSuccess = FALSE;  // TODO(schwehr): int -> bool.
        const double dfNoData = poSrcBand->GetNoDataValue( &bSuccess );

        if( bSuccess )
        {
            if( !bOnlyIfMissing
                || GetNoDataValue( &bSuccess ) != dfNoData
                || !bSuccess )
                GDALPamRasterBand::SetNoDataValue( dfNoData );
        }
    }

/* -------------------------------------------------------------------- */
/*      Category names                                                  */
/* -------------------------------------------------------------------- */
    if( nCloneFlags & GCIF_CATEGORYNAMES )
    {
        if( poSrcBand->GetCategoryNames() != nullptr )
        {
            if( !bOnlyIfMissing || GetCategoryNames() == nullptr )
                GDALPamRasterBand::SetCategoryNames(
                    poSrcBand->GetCategoryNames() );
        }
    }

/* -------------------------------------------------------------------- */
/*      Offset/scale                                                    */
/* -------------------------------------------------------------------- */
    if( nCloneFlags & GCIF_SCALEOFFSET )
    {
        int bSuccess = FALSE;  // TODO(schwehr): int -> bool.
        const double dfOffset = poSrcBand->GetOffset( &bSuccess );

        if( bSuccess )
        {
            if( !bOnlyIfMissing || GetOffset() != dfOffset )
                GDALPamRasterBand::SetOffset( dfOffset );
        }

        const double dfScale = poSrcBand->GetScale( &bSuccess );

        if( bSuccess )
        {
            if( !bOnlyIfMissing || GetScale() != dfScale )
                GDALPamRasterBand::SetScale( dfScale );
        }
    }

/* -------------------------------------------------------------------- */
/*      Unittype.                                                       */
/* -------------------------------------------------------------------- */
    if( nCloneFlags & GCIF_UNITTYPE )
    {
        if( strlen(poSrcBand->GetUnitType()) > 0 )
        {
            if( !bOnlyIfMissing
                || !EQUAL(GetUnitType(),poSrcBand->GetUnitType()) )
            {
                GDALPamRasterBand::SetUnitType( poSrcBand->GetUnitType() );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      ColorInterp                                                     */
/* -------------------------------------------------------------------- */
    if( nCloneFlags & GCIF_COLORINTERP )
    {
        if( poSrcBand->GetColorInterpretation() != GCI_Undefined )
        {
            if( !bOnlyIfMissing
                || poSrcBand->GetColorInterpretation()
                != GetColorInterpretation() )
                GDALPamRasterBand::SetColorInterpretation(
                    poSrcBand->GetColorInterpretation() );
        }
    }

/* -------------------------------------------------------------------- */
/*      color table.                                                    */
/* -------------------------------------------------------------------- */
    if( nCloneFlags & GCIF_COLORTABLE )
    {
        if( poSrcBand->GetColorTable() != nullptr )
        {
            if( !bOnlyIfMissing || GetColorTable() == nullptr )
            {
                GDALPamRasterBand::SetColorTable(
                    poSrcBand->GetColorTable() );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Raster Attribute Table.                                         */
/* -------------------------------------------------------------------- */
    if( nCloneFlags & GCIF_RAT )
    {
        const GDALRasterAttributeTable *poRAT = poSrcBand->GetDefaultRAT();

        if( poRAT != nullptr &&
            (poRAT->GetRowCount() != 0 || poRAT->GetColumnCount() != 0) )
        {
            if( !bOnlyIfMissing || GetDefaultRAT() == nullptr )
            {
                GDALPamRasterBand::SetDefaultRAT( poRAT );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Restore MO flags.                                               */
/* -------------------------------------------------------------------- */
    SetMOFlags( nSavedMOFlags );

    return CE_None;
}
//! @endcond

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr GDALPamRasterBand::SetMetadata( char **papszMetadata,
                                       const char *pszDomain )

{
    PamInitialize();

    if( psPam )
        psPam->poParentDS->MarkPamDirty();

    return GDALRasterBand::SetMetadata( papszMetadata, pszDomain );
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr GDALPamRasterBand::SetMetadataItem( const char *pszName,
                                           const char *pszValue,
                                           const char *pszDomain )

{
    PamInitialize();

    if( psPam )
        psPam->poParentDS->MarkPamDirty();

    return GDALRasterBand::SetMetadataItem( pszName, pszValue, pszDomain );
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr GDALPamRasterBand::SetNoDataValue( double dfNewValue )

{
    PamInitialize();

    if( !psPam )
        return GDALRasterBand::SetNoDataValue( dfNewValue );

    psPam->bNoDataValueSet = TRUE;
    psPam->dfNoDataValue = dfNewValue;
    psPam->poParentDS->MarkPamDirty();
    return CE_None;
}

/************************************************************************/
/*                          DeleteNoDataValue()                         */
/************************************************************************/

CPLErr GDALPamRasterBand::DeleteNoDataValue()

{
    PamInitialize();

    if( !psPam )
        return GDALRasterBand::DeleteNoDataValue();

    psPam->bNoDataValueSet = FALSE;
    psPam->dfNoDataValue = 0.0;
    psPam->poParentDS->MarkPamDirty();
    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double GDALPamRasterBand::GetNoDataValue( int *pbSuccess )

{
    if( psPam == nullptr )
        return GDALRasterBand::GetNoDataValue( pbSuccess );

    if( pbSuccess )
        *pbSuccess = psPam->bNoDataValueSet;

    return psPam->dfNoDataValue;
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

double GDALPamRasterBand::GetOffset( int *pbSuccess )

{
    if( !psPam )
        return GDALRasterBand::GetOffset( pbSuccess );

    if( pbSuccess != nullptr )
        *pbSuccess = psPam->bOffsetSet;

    return psPam->dfOffset;
}

/************************************************************************/
/*                             SetOffset()                              */
/************************************************************************/

CPLErr GDALPamRasterBand::SetOffset( double dfNewOffset )

{
    PamInitialize();

    if( psPam == nullptr )
        return GDALRasterBand::SetOffset( dfNewOffset );

    if( psPam->dfOffset != dfNewOffset )
    {
        psPam->dfOffset = dfNewOffset;
        psPam->bOffsetSet = true;
        psPam->poParentDS->MarkPamDirty();
    }

    return CE_None;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double GDALPamRasterBand::GetScale( int *pbSuccess )

{
    if( !psPam )
        return GDALRasterBand::GetScale( pbSuccess );

    if( pbSuccess != nullptr )
        *pbSuccess = psPam->bScaleSet;

    return psPam->dfScale;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr GDALPamRasterBand::SetScale( double dfNewScale )

{
    PamInitialize();

    if( psPam == nullptr )
        return GDALRasterBand::SetScale( dfNewScale );

    if( dfNewScale != psPam->dfScale )
    {
        psPam->dfScale = dfNewScale;
        psPam->bScaleSet = true;
        psPam->poParentDS->MarkPamDirty();
    }
    return CE_None;
}

/************************************************************************/
/*                            GetUnitType()                             */
/************************************************************************/

const char *GDALPamRasterBand::GetUnitType()

{
    if( psPam == nullptr )
        return GDALRasterBand::GetUnitType();

    if( psPam->pszUnitType == nullptr )
        return "";

    return psPam->pszUnitType;
}

/************************************************************************/
/*                            SetUnitType()                             */
/************************************************************************/

CPLErr GDALPamRasterBand::SetUnitType( const char *pszNewValue )

{
    PamInitialize();

    if( !psPam )
        return GDALRasterBand::SetUnitType( pszNewValue );

    if( pszNewValue == nullptr || pszNewValue[0] == '\0' )
    {
        if( psPam->pszUnitType != nullptr )
            psPam->poParentDS->MarkPamDirty();
        CPLFree( psPam->pszUnitType );
        psPam->pszUnitType = nullptr;
    }
    else
    {
        if( psPam->pszUnitType == nullptr ||
            strcmp(psPam->pszUnitType, pszNewValue) != 0 )
            psPam->poParentDS->MarkPamDirty();
        CPLFree( psPam->pszUnitType );
        psPam->pszUnitType = CPLStrdup(pszNewValue);
    }

    return CE_None;
}

/************************************************************************/
/*                          GetCategoryNames()                          */
/************************************************************************/

char **GDALPamRasterBand::GetCategoryNames()

{
    if( psPam )
        return psPam->papszCategoryNames;

    return GDALRasterBand::GetCategoryNames();
}

/************************************************************************/
/*                          SetCategoryNames()                          */
/************************************************************************/

CPLErr GDALPamRasterBand::SetCategoryNames( char ** papszNewNames )

{
    PamInitialize();

    if( !psPam )
        return GDALRasterBand::SetCategoryNames( papszNewNames );

    CSLDestroy( psPam->papszCategoryNames );
    psPam->papszCategoryNames = CSLDuplicate( papszNewNames );
    psPam->poParentDS->MarkPamDirty();
    return CE_None;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *GDALPamRasterBand::GetColorTable()

{
    if( psPam )
        return psPam->poColorTable;

    return GDALRasterBand::GetColorTable();
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr GDALPamRasterBand::SetColorTable( GDALColorTable *poTableIn )

{
    PamInitialize();

    if( !psPam )
        return GDALRasterBand::SetColorTable( poTableIn );

    if( psPam->poColorTable != nullptr )
    {
        delete psPam->poColorTable;
        psPam->poColorTable = nullptr;
    }

    if( poTableIn )
    {
        psPam->poColorTable = poTableIn->Clone();
        psPam->eColorInterp = GCI_PaletteIndex;
    }

    psPam->poParentDS->MarkPamDirty();

    return CE_None;
}

/************************************************************************/
/*                       SetColorInterpretation()                       */
/************************************************************************/

CPLErr GDALPamRasterBand::SetColorInterpretation( GDALColorInterp eInterpIn )

{
    PamInitialize();

    if( psPam )
    {
        psPam->poParentDS->MarkPamDirty();

        psPam->eColorInterp = eInterpIn;

        return CE_None;
    }

    return GDALRasterBand::SetColorInterpretation( eInterpIn );
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp GDALPamRasterBand::GetColorInterpretation()

{
    if( psPam )
        return psPam->eColorInterp;

    return GDALRasterBand::GetColorInterpretation();
}

/************************************************************************/
/*                           SetDescription()                           */
/*                                                                      */
/*      We let the GDALMajorObject hold the description, but we keep    */
/*      track of whether it has been changed so we know to save it.     */
/************************************************************************/

void GDALPamRasterBand::SetDescription( const char *pszDescription )

{
    PamInitialize();

    if( psPam && strcmp(pszDescription,GetDescription()) != 0 )
        psPam->poParentDS->MarkPamDirty();

    GDALRasterBand::SetDescription( pszDescription );
}

/************************************************************************/
/*                         PamParseHistogram()                          */
/************************************************************************/

//! @cond Doxygen_Suppress
int
PamParseHistogram( CPLXMLNode *psHistItem,
                   double *pdfMin, double *pdfMax,
                   int *pnBuckets, GUIntBig **ppanHistogram,
                   int * /* pbIncludeOutOfRange */,
                   int * /* pbApproxOK */ )
{
    if( psHistItem == nullptr )
        return FALSE;

    *pdfMin = CPLAtofM(CPLGetXMLValue( psHistItem, "HistMin", "0"));
    *pdfMax = CPLAtofM(CPLGetXMLValue( psHistItem, "HistMax", "1"));
    *pnBuckets = atoi(CPLGetXMLValue( psHistItem, "BucketCount","2"));

    if( *pnBuckets <= 0 || *pnBuckets > INT_MAX / 2 )
        return FALSE;

    if( ppanHistogram == nullptr )
        return TRUE;

    // Fetch the histogram and use it.
    const char *pszHistCounts = CPLGetXMLValue( psHistItem,
                                                "HistCounts", "" );

    // Sanity check to test consistency of BucketCount and HistCounts.
    if( strlen(pszHistCounts) < 2 * static_cast<size_t>(*pnBuckets) - 1 )
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "HistCounts content isn't consistent with BucketCount value" );
        return FALSE;
    }

    *ppanHistogram = static_cast<GUIntBig *>(
        VSICalloc(sizeof(GUIntBig),*pnBuckets) );
    if( *ppanHistogram == nullptr )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                  "Cannot allocate memory for %d buckets", *pnBuckets );
        return FALSE;
    }

    for( int iBucket = 0; iBucket < *pnBuckets; iBucket++ )
    {
        (*ppanHistogram)[iBucket] = CPLAtoGIntBig(pszHistCounts);

        // Skip to next number.
        while( *pszHistCounts != '\0' && *pszHistCounts != '|' )
            pszHistCounts++;
        if( *pszHistCounts == '|' )
            pszHistCounts++;
    }

    return TRUE;
}

/************************************************************************/
/*                      PamFindMatchingHistogram()                      */
/************************************************************************/
CPLXMLNode *
PamFindMatchingHistogram( CPLXMLNode *psSavedHistograms,
                          double dfMin, double dfMax, int nBuckets,
                          int bIncludeOutOfRange, int bApproxOK )

{
    if( psSavedHistograms == nullptr )
        return nullptr;

    for( CPLXMLNode *psXMLHist = psSavedHistograms->psChild;
         psXMLHist != nullptr;
         psXMLHist = psXMLHist->psNext )
    {
        if( psXMLHist->eType != CXT_Element
            || !EQUAL(psXMLHist->pszValue,"HistItem") )
            continue;

        const double dfHistMin =
            CPLAtofM(CPLGetXMLValue( psXMLHist, "HistMin", "0"));
        const double dfHistMax =
            CPLAtofM(CPLGetXMLValue( psXMLHist, "HistMax", "0"));

        if( !(ARE_REAL_EQUAL(dfHistMin, dfMin) )
            || !(ARE_REAL_EQUAL(dfHistMax, dfMax) )
            || atoi(CPLGetXMLValue( psXMLHist,
                                    "BucketCount","0")) != nBuckets
            || !atoi(CPLGetXMLValue( psXMLHist,
                                     "IncludeOutOfRange","0")) !=
                !bIncludeOutOfRange
            || (!bApproxOK && atoi(CPLGetXMLValue( psXMLHist,
                                                   "Approximate","0"))) )

            continue;

        return psXMLHist;
    }

    return nullptr;
}

/************************************************************************/
/*                       PamHistogramToXMLTree()                        */
/************************************************************************/

CPLXMLNode *
PamHistogramToXMLTree( double dfMin, double dfMax,
                       int nBuckets, GUIntBig * panHistogram,
                       int bIncludeOutOfRange, int bApprox )

{
    if( nBuckets > (INT_MAX - 10) / 12 )
        return nullptr;

    const size_t nLen = 22 * static_cast<size_t>(nBuckets) + 10;
    char *pszHistCounts = static_cast<char *>( VSIMalloc(nLen) );
    if( pszHistCounts == nullptr )
        return nullptr;

    CPLXMLNode *psXMLHist = CPLCreateXMLNode( nullptr, CXT_Element, "HistItem" );

    CPLString oFmt;
    CPLSetXMLValue( psXMLHist, "HistMin",
                    oFmt.Printf( "%.16g", dfMin ));
    CPLSetXMLValue( psXMLHist, "HistMax",
                    oFmt.Printf( "%.16g", dfMax ));
    CPLSetXMLValue( psXMLHist, "BucketCount",
                    oFmt.Printf( "%d", nBuckets ));
    CPLSetXMLValue( psXMLHist, "IncludeOutOfRange",
                    oFmt.Printf( "%d", bIncludeOutOfRange ));
    CPLSetXMLValue( psXMLHist, "Approximate",
                    oFmt.Printf( "%d", bApprox ));

    size_t iHistOffset = 0;
    pszHistCounts[0] = '\0';
    for( int iBucket = 0; iBucket < nBuckets; iBucket++ )
    {
        snprintf( pszHistCounts + iHistOffset,
                  nLen - iHistOffset,
                  CPL_FRMT_GUIB, panHistogram[iBucket] );
        if( iBucket < nBuckets-1 )
            strcat( pszHistCounts + iHistOffset, "|" );
        iHistOffset += strlen(pszHistCounts+iHistOffset);
    }

    CPLSetXMLValue( psXMLHist, "HistCounts", pszHistCounts );
    CPLFree( pszHistCounts );

    return psXMLHist;
}
//! @endcond

/************************************************************************/
/*                            GetHistogram()                            */
/************************************************************************/

CPLErr GDALPamRasterBand::GetHistogram( double dfMin, double dfMax,
                                        int nBuckets, GUIntBig * panHistogram,
                                        int bIncludeOutOfRange, int bApproxOK,
                                        GDALProgressFunc pfnProgress,
                                        void *pProgressData )

{
    PamInitialize();

    if( psPam == nullptr )
        return GDALRasterBand::GetHistogram( dfMin, dfMax,
                                             nBuckets, panHistogram,
                                             bIncludeOutOfRange, bApproxOK,
                                             pfnProgress, pProgressData );

/* -------------------------------------------------------------------- */
/*      Check if we have a matching histogram.                          */
/* -------------------------------------------------------------------- */
    CPLXMLNode * const psHistItem =
        PamFindMatchingHistogram( psPam->psSavedHistograms,
                                  dfMin, dfMax, nBuckets,
                                  bIncludeOutOfRange, bApproxOK );
    if( psHistItem != nullptr )
    {
        GUIntBig *panTempHist = nullptr;

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
    CPLErr eErr;

    eErr = GDALRasterBand::GetHistogram( dfMin, dfMax,
                                         nBuckets, panHistogram,
                                         bIncludeOutOfRange, bApproxOK,
                                         pfnProgress, pProgressData );

/* -------------------------------------------------------------------- */
/*      Save an XML description of this histogram.                      */
/* -------------------------------------------------------------------- */
    if( eErr != CE_None )
        return eErr;

    CPLXMLNode *psXMLHist = PamHistogramToXMLTree( dfMin, dfMax, nBuckets,
                                                   panHistogram,
                                                   bIncludeOutOfRange,
                                                   bApproxOK );
    if( psXMLHist != nullptr )
    {
        psPam->poParentDS->MarkPamDirty();

        if( psPam->psSavedHistograms == nullptr )
            psPam->psSavedHistograms = CPLCreateXMLNode( nullptr, CXT_Element,
                                                         "Histograms" );

        CPLAddXMLChild( psPam->psSavedHistograms, psXMLHist );
    }

    return CE_None;
}

/************************************************************************/
/*                        SetDefaultHistogram()                         */
/************************************************************************/

CPLErr GDALPamRasterBand::SetDefaultHistogram( double dfMin, double dfMax,
                                               int nBuckets,
                                               GUIntBig *panHistogram )

{
    PamInitialize();

    if( psPam == nullptr )
        return GDALRasterBand::SetDefaultHistogram( dfMin, dfMax,
                                                    nBuckets, panHistogram );

/* -------------------------------------------------------------------- */
/*      Do we have a matching histogram we should replace?              */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psNode = PamFindMatchingHistogram( psPam->psSavedHistograms,
                                                   dfMin, dfMax, nBuckets,
                                                   TRUE, TRUE );
    if( psNode != nullptr )
    {
        /* blow this one away */
        CPLRemoveXMLChild( psPam->psSavedHistograms, psNode );
        CPLDestroyXMLNode( psNode );
    }

/* -------------------------------------------------------------------- */
/*      Translate into a histogram XML tree.                            */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psHistItem = PamHistogramToXMLTree( dfMin, dfMax, nBuckets,
                                                    panHistogram, TRUE, FALSE );
    if( psHistItem == nullptr )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Insert our new default histogram at the front of the            */
/*      histogram list so that it will be the default histogram.        */
/* -------------------------------------------------------------------- */
    psPam->poParentDS->MarkPamDirty();

    if( psPam->psSavedHistograms == nullptr )
        psPam->psSavedHistograms = CPLCreateXMLNode( nullptr, CXT_Element,
                                                     "Histograms" );

    psHistItem->psNext = psPam->psSavedHistograms->psChild;
    psPam->psSavedHistograms->psChild = psHistItem;

    return CE_None;
}

/************************************************************************/
/*                        GetDefaultHistogram()                         */
/************************************************************************/

CPLErr
GDALPamRasterBand::GetDefaultHistogram( double *pdfMin, double *pdfMax,
                                        int *pnBuckets,
                                        GUIntBig **ppanHistogram,
                                        int bForce,
                                        GDALProgressFunc pfnProgress,
                                        void *pProgressData )

{
    if( psPam && psPam->psSavedHistograms != nullptr )
    {
        CPLXMLNode *psXMLHist = psPam->psSavedHistograms->psChild;

        for( ; psXMLHist != nullptr; psXMLHist = psXMLHist->psNext )
        {
            if( psXMLHist->eType != CXT_Element
                || !EQUAL(psXMLHist->pszValue,"HistItem") )
                continue;

            // TODO(schwehr): int -> bool.
            int bApprox = FALSE;
            int bIncludeOutOfRange = FALSE;
            if( PamParseHistogram( psXMLHist, pdfMin, pdfMax, pnBuckets,
                                   ppanHistogram, &bIncludeOutOfRange,
                                   &bApprox ) )
                return CE_None;

            return CE_Failure;
        }
    }

    return GDALRasterBand::GetDefaultHistogram( pdfMin, pdfMax, pnBuckets,
                                                ppanHistogram, bForce,
                                                pfnProgress, pProgressData );
}

/************************************************************************/
/*                           GetDefaultRAT()                            */
/************************************************************************/

GDALRasterAttributeTable *GDALPamRasterBand::GetDefaultRAT()

{
    PamInitialize();

    if( psPam == nullptr )
        return GDALRasterBand::GetDefaultRAT();

    return psPam->poDefaultRAT;
}

/************************************************************************/
/*                           SetDefaultRAT()                            */
/************************************************************************/

CPLErr GDALPamRasterBand::SetDefaultRAT( const GDALRasterAttributeTable *poRAT )

{
    PamInitialize();

    if( psPam == nullptr )
        return GDALRasterBand::SetDefaultRAT( poRAT );

    psPam->poParentDS->MarkPamDirty();

    if( psPam->poDefaultRAT != nullptr )
    {
        delete psPam->poDefaultRAT;
        psPam->poDefaultRAT = nullptr;
    }

    if( poRAT == nullptr )
        psPam->poDefaultRAT = nullptr;
    else
        psPam->poDefaultRAT = poRAT->Clone();

    return CE_None;
}
