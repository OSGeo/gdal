/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALPamRasterBand, a raster band base class
 *           that knows how to persistently store auxilary metadata in an
 *           external xml file. 
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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

#include "gdal_pam.h"
#include "gdal_rat.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         GDALPamRasterBand()                          */
/************************************************************************/

GDALPamRasterBand::GDALPamRasterBand()

{
    psPam = NULL;
    SetMOFlags( GetMOFlags() | GMO_PAM_CLASS );
}

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

CPLXMLNode *GDALPamRasterBand::SerializeToXML( const char *pszVRTPath )

{
    if( psPam == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Setup root node and attributes.                                 */
/* -------------------------------------------------------------------- */
    CPLString oFmt;

    CPLXMLNode *psTree;

    psTree = CPLCreateXMLNode( NULL, CXT_Element, "PAMRasterBand" );

    if( GetBand() > 0 )
        CPLSetXMLValue( psTree, "#band", oFmt.Printf( "%d", GetBand() ) );

/* -------------------------------------------------------------------- */
/*      Serialize information of interest.                              */
/* -------------------------------------------------------------------- */
    if( strlen(GetDescription()) > 0 )
        CPLSetXMLValue( psTree, "Description", GetDescription() );

    if( psPam->bNoDataValueSet )
    {
        CPLSetXMLValue( psTree, "NoDataValue", 
                        oFmt.Printf( "%.14E", psPam->dfNoDataValue ) );

        /* hex encode real floating point values */
        if( psPam->dfNoDataValue != floor(psPam->dfNoDataValue) 
            || psPam->dfNoDataValue != atof(oFmt) )
        {
            double dfNoDataLittleEndian;

            dfNoDataLittleEndian = psPam->dfNoDataValue;
            CPL_LSBPTR64( &dfNoDataLittleEndian );

            char *pszHexEncoding = 
                CPLBinaryToHex( 8, (GByte *) &dfNoDataLittleEndian );
            CPLSetXMLValue( psTree, "NoDataValue.#le_hex_equiv",pszHexEncoding);
            CPLFree( pszHexEncoding );
        }
    }

    if( psPam->pszUnitType != NULL )
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
    if( psPam->papszCategoryNames != NULL )
    {
        CPLXMLNode *psCT_XML = CPLCreateXMLNode( psTree, CXT_Element, 
                                                 "CategoryNames" );

        for( int iEntry=0; psPam->papszCategoryNames[iEntry] != NULL; iEntry++)
        {
            CPLCreateXMLElementAndValue( psCT_XML, "Category", 
                                         psPam->papszCategoryNames[iEntry] );
        }
    }

/* -------------------------------------------------------------------- */
/*      Color Table.                                                    */
/* -------------------------------------------------------------------- */
    if( psPam->poColorTable != NULL )
    {
        CPLXMLNode *psCT_XML = CPLCreateXMLNode( psTree, CXT_Element, 
                                                 "ColorTable" );

        for( int iEntry=0; iEntry < psPam->poColorTable->GetColorEntryCount(); 
             iEntry++ )
        {
            GDALColorEntry sEntry;
            CPLXMLNode *psEntry_XML = CPLCreateXMLNode( psCT_XML, CXT_Element, 
                                                        "Entry" );

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
    if( psPam->psSavedHistograms != NULL )
        CPLAddXMLChild( psTree, CPLCloneXMLTree( psPam->psSavedHistograms ) );

/* -------------------------------------------------------------------- */
/*      Raster Attribute Table                                          */
/* -------------------------------------------------------------------- */
    if( psPam->poDefaultRAT != NULL )
        CPLAddXMLChild( psTree, psPam->poDefaultRAT->Serialize() );

/* -------------------------------------------------------------------- */
/*      Metadata.                                                       */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psMD;

    psMD = oMDMD.Serialize();
    if( psMD != NULL )
    {
        if( psMD->psChild == NULL )
            CPLDestroyXMLNode( psMD );
        else
            CPLAddXMLChild( psTree, psMD );
    }

/* -------------------------------------------------------------------- */
/*      We don't want to return anything if we had no metadata to       */
/*      attach.                                                         */
/* -------------------------------------------------------------------- */
    if( psTree->psChild == NULL || psTree->psChild->psNext == NULL )
    {
        CPLDestroyXMLNode( psTree );
        psTree = NULL;
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

    GDALPamDataset *poParentDS = (GDALPamDataset *) GetDataset();

    if( poParentDS == NULL || !(poParentDS->GetMOFlags() & GMO_PAM_CLASS) )
        return;

    poParentDS->PamInitialize();
    if( poParentDS->psPam == NULL )
        return;

    // Often (always?) initializing our parent will have initialized us. 
    if( psPam != NULL )
        return;

    psPam = (GDALRasterBandPamInfo *)
        CPLCalloc(sizeof(GDALRasterBandPamInfo),1);

    psPam->dfScale = 1.0;
    psPam->poParentDS = poParentDS;
    psPam->dfNoDataValue = -1e10;
    psPam->poDefaultRAT = NULL;
}

/************************************************************************/
/*                              PamClear()                              */
/************************************************************************/

void GDALPamRasterBand::PamClear()

{
    if( psPam )
    {
        if( psPam->poColorTable )
            delete psPam->poColorTable;
        psPam->poColorTable = NULL;
        
        CPLFree( psPam->pszUnitType );
        CSLDestroy( psPam->papszCategoryNames );

        if( psPam->poDefaultRAT != NULL )
        {
            delete psPam->poDefaultRAT;
            psPam->poDefaultRAT = NULL;
        }

        if (psPam->psSavedHistograms != NULL)
        {
            CPLDestroyXMLNode (psPam->psSavedHistograms );
            psPam->psSavedHistograms = NULL;
        }

        CPLFree( psPam );
        psPam = NULL;
    }
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr GDALPamRasterBand::XMLInit( CPLXMLNode *psTree, const char *pszVRTPath )

{
    PamInitialize();

/* -------------------------------------------------------------------- */
/*      Apply any dataset level metadata.                               */
/* -------------------------------------------------------------------- */
    oMDMD.XMLInit( psTree, TRUE );

/* -------------------------------------------------------------------- */
/*      Collect various other items of metadata.                        */
/* -------------------------------------------------------------------- */
    GDALMajorObject::SetDescription( CPLGetXMLValue( psTree, "Description", "" ) );
    
    if( CPLGetXMLValue( psTree, "NoDataValue", NULL ) != NULL )
    {
        const char *pszLEHex = 
            CPLGetXMLValue( psTree, "NoDataValue.le_hex_equiv", NULL );
        if( pszLEHex != NULL )
        {
            int nBytes;
            GByte *pabyBin = CPLHexToBinary( pszLEHex, &nBytes );
            if( nBytes == 8 )
            {
                CPL_LSBPTR64( pabyBin );
                
                GDALPamRasterBand::SetNoDataValue( *((double *) pabyBin) );
            }
            else
            {
                GDALPamRasterBand::SetNoDataValue( 
                    atof(CPLGetXMLValue( psTree, "NoDataValue", "0" )) );
            }
            CPLFree( pabyBin );
        }
        else
        {
            GDALPamRasterBand::SetNoDataValue( 
                atof(CPLGetXMLValue( psTree, "NoDataValue", "0" )) );
        }
    }

    GDALPamRasterBand::SetOffset( 
        atof(CPLGetXMLValue( psTree, "Offset", "0.0" )) );
    GDALPamRasterBand::SetScale( 
        atof(CPLGetXMLValue( psTree, "Scale", "1.0" )) );

    GDALPamRasterBand::SetUnitType( CPLGetXMLValue( psTree, "UnitType", NULL));

    if( CPLGetXMLValue( psTree, "ColorInterp", NULL ) != NULL )
    {
        const char *pszInterp = CPLGetXMLValue( psTree, "ColorInterp", NULL );
        GDALPamRasterBand::SetColorInterpretation(
                                GDALGetColorInterpretationByName(pszInterp));
    }

/* -------------------------------------------------------------------- */
/*      Category names.                                                 */
/* -------------------------------------------------------------------- */
    if( CPLGetXMLNode( psTree, "CategoryNames" ) != NULL )
    {
        CPLXMLNode *psEntry;
        char **papszCategoryNames = NULL;

        for( psEntry = CPLGetXMLNode( psTree, "CategoryNames" )->psChild;
             psEntry != NULL; psEntry = psEntry->psNext )
        {
            /* Don't skeep <Category> tag with empty content */
            if( psEntry->eType != CXT_Element 
                || !EQUAL(psEntry->pszValue,"Category") 
                || (psEntry->psChild != NULL && psEntry->psChild->eType != CXT_Text) )
                continue;
            
            papszCategoryNames = CSLAddString( papszCategoryNames, 
                                 (psEntry->psChild) ? psEntry->psChild->pszValue : "" );
        }
        
        GDALPamRasterBand::SetCategoryNames( papszCategoryNames );
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
        
        GDALPamRasterBand::SetColorTable( &oTable );
    }

/* -------------------------------------------------------------------- */
/*      Do we have a complete set of stats?                             */
/* -------------------------------------------------------------------- */
    if( CPLGetXMLNode( psTree, "Minimum" ) != NULL 
        && CPLGetXMLNode( psTree, "Maximum" ) != NULL )
    {
        psPam->bHaveMinMax = TRUE;
        psPam->dfMin = atof(CPLGetXMLValue(psTree, "Minimum","0"));
        psPam->dfMax = atof(CPLGetXMLValue(psTree, "Maximum","0"));
    }

    if( CPLGetXMLNode( psTree, "Mean" ) != NULL 
        && CPLGetXMLNode( psTree, "StandardDeviation" ) != NULL )
    {
        psPam->bHaveStats = TRUE;
        psPam->dfMean = atof(CPLGetXMLValue(psTree, "Mean","0"));
        psPam->dfStdDev = atof(CPLGetXMLValue(psTree,"StandardDeviation","0"));
    }

/* -------------------------------------------------------------------- */
/*      Histograms                                                      */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psHist = CPLGetXMLNode( psTree, "Histograms" );
    if( psHist != NULL )
    {
        CPLXMLNode *psNext = psHist->psNext;
        psHist->psNext = NULL;

        psPam->psSavedHistograms = CPLCloneXMLTree( psHist );
        psHist->psNext = psNext;
    }

/* -------------------------------------------------------------------- */
/*      Raster Attribute Table                                          */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psRAT = CPLGetXMLNode( psTree, "GDALRasterAttributeTable" );
    if( psRAT != NULL )
    {
        psPam->poDefaultRAT = new GDALRasterAttributeTable();
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
    int bOnlyIfMissing = nCloneFlags & GCIF_ONLY_IF_MISSING;
    int bSuccess;
    int nSavedMOFlags = GetMOFlags();

    PamInitialize();

/* -------------------------------------------------------------------- */
/*      Supress NotImplemented error messages - mainly needed if PAM    */
/*      disabled.                                                       */
/* -------------------------------------------------------------------- */
    SetMOFlags( nSavedMOFlags | GMO_IGNORE_UNIMPLEMENTED );

/* -------------------------------------------------------------------- */
/*      Metadata                                                        */
/* -------------------------------------------------------------------- */
    if( nCloneFlags & GCIF_BAND_METADATA )
    {
        if( poSrcBand->GetMetadata() != NULL )
        {
            if( !bOnlyIfMissing 
             || CSLCount(GetMetadata()) != CSLCount(poSrcBand->GetMetadata()) )
            {
                SetMetadata( poSrcBand->GetMetadata() );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      NODATA                                                          */
/* -------------------------------------------------------------------- */
    if( nCloneFlags & GCIF_NODATA )
    {
        double dfNoData = poSrcBand->GetNoDataValue( &bSuccess );
        
        if( bSuccess )
        {
            if( !bOnlyIfMissing 
                || GetNoDataValue( &bSuccess ) != dfNoData 
                || !bSuccess )
                GDALPamRasterBand::SetNoDataValue( dfNoData );
        }
    }

/* -------------------------------------------------------------------- */
/*      Offset/scale                                                    */
/* -------------------------------------------------------------------- */
    if( nCloneFlags & GCIF_SCALEOFFSET )
    {
        double dfOffset = poSrcBand->GetOffset( &bSuccess );
        
        if( bSuccess )
        {
            if( !bOnlyIfMissing || GetOffset() != dfOffset )
                GDALPamRasterBand::SetOffset( dfOffset );
        }

        double dfScale = poSrcBand->GetScale( &bSuccess );
        
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
    if( nCloneFlags && GCIF_COLORTABLE )
    {
        if( poSrcBand->GetColorTable() != NULL )
        {
            if( !bOnlyIfMissing || GetColorTable() == NULL )
            {
                GDALPamRasterBand::SetColorTable( 
                    poSrcBand->GetColorTable() );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Raster Attribute Table.                                         */
/* -------------------------------------------------------------------- */
    if( nCloneFlags && GCIF_RAT )
    {
        const GDALRasterAttributeTable *poRAT = poSrcBand->GetDefaultRAT();

        if( poRAT != NULL )
        {
            if( !bOnlyIfMissing || GetDefaultRAT() == NULL )
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

    if( psPam )
    {
        psPam->bNoDataValueSet = TRUE;
        psPam->dfNoDataValue = dfNewValue;
        psPam->poParentDS->MarkPamDirty();
        return CE_None;
    }
    else
        return GDALRasterBand::SetNoDataValue( dfNewValue );
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double GDALPamRasterBand::GetNoDataValue( int *pbSuccess )

{
    if( psPam != NULL )
    {
        if( pbSuccess )
            *pbSuccess = psPam->bNoDataValueSet;

        return psPam->dfNoDataValue;
    }
    else
        return GDALRasterBand::GetNoDataValue( pbSuccess );
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

double GDALPamRasterBand::GetOffset( int *pbSuccess )

{
    if( psPam )
    {
        if( pbSuccess != NULL )
            *pbSuccess = TRUE;
        
        return psPam->dfOffset;
    }
    else
        return GDALRasterBand::GetOffset( pbSuccess );
}

/************************************************************************/
/*                             SetOffset()                              */
/************************************************************************/

CPLErr GDALPamRasterBand::SetOffset( double dfNewOffset )

{
    PamInitialize();

    if( psPam != NULL )
    {
        if( psPam->dfOffset != dfNewOffset )
        {
            psPam->dfOffset = dfNewOffset;
            psPam->poParentDS->MarkPamDirty();
        }

        return CE_None;
    }
    else
        return GDALRasterBand::SetOffset( dfNewOffset );
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double GDALPamRasterBand::GetScale( int *pbSuccess )

{
    if( psPam )
    {
        if( pbSuccess != NULL )
            *pbSuccess = TRUE;
        
        return psPam->dfScale;
    }
    else
        return GDALRasterBand::GetScale( pbSuccess );
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr GDALPamRasterBand::SetScale( double dfNewScale )

{
    PamInitialize();

    if( psPam != NULL )
    {
        if( dfNewScale != psPam->dfScale )
        {
            psPam->dfScale = dfNewScale;
            psPam->poParentDS->MarkPamDirty();
        }
        return CE_None;
    }
    else
        return GDALRasterBand::SetScale( dfNewScale );
}

/************************************************************************/
/*                            GetUnitType()                             */
/************************************************************************/

const char *GDALPamRasterBand::GetUnitType()

{
    if( psPam != NULL )
    {
        if( psPam->pszUnitType == NULL )
            return "";
        else
            return psPam->pszUnitType;
    }
    else
        return GDALRasterBand::GetUnitType();
}

/************************************************************************/
/*                            SetUnitType()                             */
/************************************************************************/

CPLErr GDALPamRasterBand::SetUnitType( const char *pszNewValue )

{
    PamInitialize();

    if( psPam )
    {
        CPLFree( psPam->pszUnitType );
        
        if( pszNewValue == NULL )
            psPam->pszUnitType = NULL;
        else
            psPam->pszUnitType = CPLStrdup(pszNewValue);

        return CE_None;
    }
    else
        return GDALRasterBand::SetUnitType( pszNewValue );
}

/************************************************************************/
/*                          GetCategoryNames()                          */
/************************************************************************/

char **GDALPamRasterBand::GetCategoryNames()

{
    if( psPam )
        return psPam->papszCategoryNames;
    else
        return GDALRasterBand::GetCategoryNames();
}

/************************************************************************/
/*                          SetCategoryNames()                          */
/************************************************************************/

CPLErr GDALPamRasterBand::SetCategoryNames( char ** papszNewNames )

{
    PamInitialize();

    if( psPam ) 
    {
        CSLDestroy( psPam->papszCategoryNames );
        psPam->papszCategoryNames = CSLDuplicate( papszNewNames );
        psPam->poParentDS->MarkPamDirty();
        return CE_None;
    }
    else 
        return GDALRasterBand::SetCategoryNames( papszNewNames );

}


/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *GDALPamRasterBand::GetColorTable()

{
    if( psPam )
        return psPam->poColorTable;
    else
        return GDALRasterBand::GetColorTable();
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr GDALPamRasterBand::SetColorTable( GDALColorTable *poTableIn )

{
    PamInitialize();

    if( psPam )
    {
        if( psPam->poColorTable != NULL )
        {
            delete psPam->poColorTable;
            psPam->poColorTable = NULL;
        }
        
        if( poTableIn )
        {
            psPam->poColorTable = poTableIn->Clone();
            psPam->eColorInterp = GCI_PaletteIndex;
        }

        psPam->poParentDS->MarkPamDirty();

        return CE_None;
    }
    else
        return GDALRasterBand::SetColorTable( poTableIn );

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
    else
        return GDALRasterBand::SetColorInterpretation( eInterpIn );
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp GDALPamRasterBand::GetColorInterpretation()

{
    if( psPam )
        return psPam->eColorInterp;
    else
        return GDALRasterBand::GetColorInterpretation();
}

/************************************************************************/
/*                         PamParseHistogram()                          */
/************************************************************************/

int 
PamParseHistogram( CPLXMLNode *psHistItem, 
                   double *pdfMin, double *pdfMax, 
                   int *pnBuckets, int **ppanHistogram, 
                   int *pbIncludeOutOfRange, int *pbApproxOK )

{
    if( psHistItem == NULL )
        return FALSE;

    *pdfMin = atof(CPLGetXMLValue( psHistItem, "HistMin", "0"));
    *pdfMax = atof(CPLGetXMLValue( psHistItem, "HistMax", "1"));
    *pnBuckets = atoi(CPLGetXMLValue( psHistItem, "BucketCount","2"));
    if (*pnBuckets <= 0)
        return FALSE;

    if( ppanHistogram == NULL )
        return TRUE;

    // Fetch the histogram and use it. 
    int iBucket;
    const char *pszHistCounts = CPLGetXMLValue( psHistItem, 
                                                "HistCounts", "" );

    *ppanHistogram = (int *) VSICalloc(sizeof(int),*pnBuckets);
    if (*ppanHistogram == NULL)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Cannot allocate memory for %d buckets", *pnBuckets);
        return FALSE;
    }

    for( iBucket = 0; iBucket < *pnBuckets; iBucket++ )
    {
        (*ppanHistogram)[iBucket] = atoi(pszHistCounts);
        
        // skip to next number.
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
    if( psSavedHistograms == NULL )
        return NULL;

    CPLXMLNode *psXMLHist;
    for( psXMLHist = psSavedHistograms->psChild;
         psXMLHist != NULL; psXMLHist = psXMLHist->psNext )
    {
        if( psXMLHist->eType != CXT_Element
            || !EQUAL(psXMLHist->pszValue,"HistItem") )
            continue;

        // should try and make min/max test a bit fuzzy.

        if( atof(CPLGetXMLValue( psXMLHist, "HistMin", "0")) != dfMin 
            || atof(CPLGetXMLValue( psXMLHist, "HistMax", "0")) != dfMax
            || atoi(CPLGetXMLValue( psXMLHist, 
                                    "BucketCount","0")) != nBuckets
            || !atoi(CPLGetXMLValue( psXMLHist, 
                                     "IncludeOutOfRange","0")) != !bIncludeOutOfRange 
            || (!bApproxOK && atoi(CPLGetXMLValue( psXMLHist, 
                                                   "Approximate","0"))) )

            continue;

        return psXMLHist;
    }

    return NULL;
}

/************************************************************************/
/*                       PamHistogramToXMLTree()                        */
/************************************************************************/

CPLXMLNode *
PamHistogramToXMLTree( double dfMin, double dfMax,
                       int nBuckets, int * panHistogram,
                       int bIncludeOutOfRange, int bApprox )

{
    char *pszHistCounts = (char *) CPLMalloc(12 * nBuckets + 10);
    int iBucket, iHistOffset;
    CPLXMLNode *psXMLHist;
    CPLString oFmt;

    psXMLHist = CPLCreateXMLNode( NULL, CXT_Element, "HistItem" );

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

    iHistOffset = 0;
    pszHistCounts[0] = '\0';
    for( iBucket = 0; iBucket < nBuckets; iBucket++ )
    {
        sprintf( pszHistCounts + iHistOffset, "%d", panHistogram[iBucket] );
        if( iBucket < nBuckets-1 )
            strcat( pszHistCounts + iHistOffset, "|" );
        iHistOffset += strlen(pszHistCounts+iHistOffset);
    }
        
    CPLSetXMLValue( psXMLHist, "HistCounts", pszHistCounts );
    CPLFree( pszHistCounts );

    return psXMLHist;
}

/************************************************************************/
/*                            GetHistogram()                            */
/************************************************************************/

CPLErr GDALPamRasterBand::GetHistogram( double dfMin, double dfMax,
                                        int nBuckets, int * panHistogram,
                                        int bIncludeOutOfRange, int bApproxOK,
                                        GDALProgressFunc pfnProgress, 
                                        void *pProgressData )

{
    PamInitialize();

    if( psPam == NULL )
        return GDALRasterBand::GetHistogram( dfMin, dfMax, 
                                             nBuckets, panHistogram, 
                                             bIncludeOutOfRange, bApproxOK,
                                             pfnProgress, pProgressData );

/* -------------------------------------------------------------------- */
/*      Check if we have a matching histogram.                          */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psHistItem;

    psHistItem = PamFindMatchingHistogram( psPam->psSavedHistograms, 
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
            psPam->poParentDS->MarkPamDirty();

            if( psPam->psSavedHistograms == NULL )
                psPam->psSavedHistograms = CPLCreateXMLNode( NULL, CXT_Element,
                                                             "Histograms" );
            
            CPLAddXMLChild( psPam->psSavedHistograms, psXMLHist );
        }
    }

    return eErr;
}

/************************************************************************/
/*                        SetDefaultHistogram()                         */
/************************************************************************/

CPLErr GDALPamRasterBand::SetDefaultHistogram( double dfMin, double dfMax, 
                                               int nBuckets, int *panHistogram)

{
    CPLXMLNode *psNode;

    PamInitialize();

    if( psPam == NULL )
        return GDALRasterBand::SetDefaultHistogram( dfMin, dfMax, 
                                                    nBuckets, panHistogram );

/* -------------------------------------------------------------------- */
/*      Do we have a matching histogram we should replace?              */
/* -------------------------------------------------------------------- */
    psNode = PamFindMatchingHistogram( psPam->psSavedHistograms, 
                                       dfMin, dfMax, nBuckets,
                                       TRUE, TRUE );
    if( psNode != NULL )
    {
        /* blow this one away */
        CPLRemoveXMLChild( psPam->psSavedHistograms, psNode );
        CPLDestroyXMLNode( psNode );
    }

/* -------------------------------------------------------------------- */
/*      Translate into a histogram XML tree.                            */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psHistItem;

    psHistItem = PamHistogramToXMLTree( dfMin, dfMax, nBuckets, 
                                        panHistogram, TRUE, FALSE );

/* -------------------------------------------------------------------- */
/*      Insert our new default histogram at the front of the            */
/*      histogram list so that it will be the default histogram.        */
/* -------------------------------------------------------------------- */
    psPam->poParentDS->MarkPamDirty();

    if( psPam->psSavedHistograms == NULL )
        psPam->psSavedHistograms = CPLCreateXMLNode( NULL, CXT_Element,
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
                                        int *pnBuckets, int **ppanHistogram, 
                                        int bForce,
                                        GDALProgressFunc pfnProgress, 
                                        void *pProgressData )
    
{
    if( psPam && psPam->psSavedHistograms != NULL )
    {
        CPLXMLNode *psXMLHist;

        for( psXMLHist = psPam->psSavedHistograms->psChild;
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
                                                pfnProgress, pProgressData );
}

/************************************************************************/
/*                           GetDefaultRAT()                            */
/************************************************************************/

const GDALRasterAttributeTable *GDALPamRasterBand::GetDefaultRAT()

{
    PamInitialize();

    if( psPam == NULL )
        return GDALRasterBand::GetDefaultRAT();

    return psPam->poDefaultRAT;
}

/************************************************************************/
/*                           SetDefaultRAT()                            */
/************************************************************************/

CPLErr GDALPamRasterBand::SetDefaultRAT( const GDALRasterAttributeTable *poRAT)

{
    PamInitialize();

    if( psPam == NULL )
        return GDALRasterBand::SetDefaultRAT( poRAT );

    psPam->poParentDS->MarkPamDirty();

    if( psPam->poDefaultRAT != NULL )
    {
        delete psPam->poDefaultRAT;
        psPam->poDefaultRAT = NULL;
    }

    if( poRAT == NULL )
        psPam->poDefaultRAT = NULL;
    else
        psPam->poDefaultRAT = poRAT->Clone();

    return CE_None;
}

