/******************************************************************************
 * $Id: $
 *
 * Name:     georaster_rasterband.cpp
 * Project:  Oracle Spatial GeoRaster Driver
 * Purpose:  Implement GeoRasterRasterBand methods
 * Author:   Ivan Lucena [ivan.lucena@pmldnet.com]
 *
 ******************************************************************************
 * Copyright (c) 2008, Ivan Lucena
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files ( the "Software" ),
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

#include "georaster_priv.h"

//  ---------------------------------------------------------------------------
//                                                        GeoRasterRasterBand()
//  ---------------------------------------------------------------------------

GeoRasterRasterBand::GeoRasterRasterBand( GeoRasterDataset *poGDS, int nBand )
{
    poDS                = (GDALDataset*) poGDS;
    nBand               = nBand;
    poGeoRaster         = poGDS->poGeoRaster;
    poColorTable        = new GDALColorTable();
    poDefaultRAT        = NULL;
    pszVATName          = NULL;
    nRasterXSize        = poGeoRaster->nRasterColumns;
    nRasterYSize        = poGeoRaster->nRasterRows;
    nBlockXSize         = poGeoRaster->nColumnBlockSize;
    nBlockYSize         = poGeoRaster->nRowBlockSize;
    nBlocksPerColumn    = poGeoRaster->nTotalColumnBlocks;
    nBlocksPerRow       = poGeoRaster->nTotalRowBlocks;
    dfNoData            = 0.0;
    bValidStats         = false;
}

//  ---------------------------------------------------------------------------
//                                                       ~GeoRasterRasterBand()
//  ---------------------------------------------------------------------------

GeoRasterRasterBand::~GeoRasterRasterBand()
{
    ObjFree_nt( poColorTable );
    ObjFree_nt( poDefaultRAT );
    CPLFree_nt( pszVATName );
}

//  ---------------------------------------------------------------------------
//                                                                 IReadBlock()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterRasterBand::IReadBlock( int nBlockXOff,
                                        int nBlockYOff,
                                        void *pImage )
{
    if( poDS->GetAccess() == GA_Update )
    {
        return CE_None;
    }

    if( poGeoRaster->GetBandBlock( nBand, nBlockXOff, nBlockYOff, pImage ) )
    {
        return CE_None;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Error reading GeoRaster ofsett X (%d) offset Y (%d) band (%d)",
            nBlockXOff, nBlockYOff, nBand );

        return CE_Failure;
    }
}

//  ---------------------------------------------------------------------------
//                                                                IWriteBlock()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterRasterBand::IWriteBlock( int nBlockXOff,
                                         int nBlockYOff,
                                         void *pImage )
{
    if( poGeoRaster->SetBandBlock( nBand, nBlockXOff, nBlockYOff, pImage ) )
    {
        return CE_None;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Error updating GeoRaster ofsett X (%d) offset Y (%d) band (%d)",
            nBlockXOff, nBlockYOff, nBand );

        return CE_Failure;
    }
}
//  ---------------------------------------------------------------------------
//                                                     GetColorInterpretation()
//  ---------------------------------------------------------------------------

GDALColorInterp GeoRasterRasterBand::GetColorInterpretation()
{
    GeoRasterDataset* poGDS = (GeoRasterDataset*) poDS;

    if( eDataType == GDT_Byte && poGDS->nBands > 2 )
    {
        if( nBand == poGeoRaster->iDefaultRedBand )
        {
            return GCI_RedBand;
        }
        else if ( nBand == poGeoRaster->iDefaultGreenBand )
        {
            return GCI_GreenBand;
        }
        else if ( nBand == poGeoRaster->iDefaultBlueBand )
        {
            return GCI_BlueBand;
        }
        else
        {
            return GCI_GrayIndex;
        }
    }

    if( poGeoRaster->HasColorMap( nBand ) )
    {
        return GCI_PaletteIndex;
    }
    else
    {
        return GCI_GrayIndex;
    }
}

//  ---------------------------------------------------------------------------
//                                                              GetColorTable()
//  ---------------------------------------------------------------------------

GDALColorTable *GeoRasterRasterBand::GetColorTable()
{
    poGeoRaster->GetColorMap( nBand, poColorTable );

    if( poColorTable->GetColorEntryCount() == 0 )
    {
        return NULL;
    }

    return poColorTable;
}

//  ---------------------------------------------------------------------------
//                                                              SetColorTable()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterRasterBand::SetColorTable( GDALColorTable *poInColorTable )
{
    if( poInColorTable == NULL )
    {
        return CE_None;
    }

    if( poInColorTable->GetColorEntryCount() == 0 )
    {
        return CE_None;
    }

    delete poColorTable;

    poColorTable = poInColorTable->Clone();

    poGeoRaster->SetColorMap( nBand, poColorTable );

    return CE_None;
}

//  ---------------------------------------------------------------------------
//                                                                 GetMinimum()
//  ---------------------------------------------------------------------------

double GeoRasterRasterBand::GetMinimum( int *pbSuccess )
{
    *pbSuccess = (int) bValidStats;

    return dfMin;
}

//  ---------------------------------------------------------------------------
//                                                                 GetMaximum()
//  ---------------------------------------------------------------------------

double GeoRasterRasterBand::GetMaximum( int *pbSuccess )
{
    *pbSuccess = (int) bValidStats;

    return dfMax;
}

//  ---------------------------------------------------------------------------
//                                                              GetStatistics()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterRasterBand::GetStatistics( int bApproxOK, int bForce,
                                           double *pdfMin, double *pdfMax,
                                           double *pdfMean, double *pdfStdDev )
{
    (void) bForce;
    (void) bApproxOK;

    if( ! bValidStats )
    {
        bValidStats = poGeoRaster->GetStatistics( nBand,
                          dfMin, dfMax, dfMean, dfStdDev );
    }

    if( bValidStats )
    {
        *pdfMin     = dfMin;
        *pdfMax     = dfMax;
        *pdfMean    = dfMean;
        *pdfStdDev  = dfStdDev;

        return CE_None;
    }

    return CE_Failure;
}

//  ---------------------------------------------------------------------------
//                                                              SetStatistics()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterRasterBand::SetStatistics( double dfMin, double dfMax,
                                           double dfMean, double dfStdDev )
{
    dfMin       = dfMin;
    dfMax       = dfMax;
    dfMean      = dfMax;
    dfStdDev    = dfStdDev;
    bValidStats = true;

    poGeoRaster->SetStatistics( dfMin, dfMax, dfMean, dfStdDev, nBand );

    return CE_None;
}

//  ---------------------------------------------------------------------------
//                                                             GetNoDataValue()
//  ---------------------------------------------------------------------------

double GeoRasterRasterBand::GetNoDataValue( int *pbSuccess )
{
    if( pbSuccess )
    {
        *pbSuccess = (int) poGeoRaster->GetNoData( &dfNoData );
    }

    return dfNoData;
}

//  ---------------------------------------------------------------------------
//                                                             SetNoDataValue()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterRasterBand::SetNoDataValue( double dfNoDataValue )
{
    poGeoRaster->SetNoData( dfNoDataValue );

    return CE_None;
}

//  ---------------------------------------------------------------------------
//                                                              SetDefaultRAT()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterRasterBand::SetDefaultRAT( const GDALRasterAttributeTable *poRAT )
{
    GeoRasterDataset* poGDS = (GeoRasterDataset*) poDS;

    if( ! poRAT )
    {
        return CE_Failure;
    }

    if( poDefaultRAT )
    {
        delete poDefaultRAT;
    }

    poDefaultRAT = poRAT->Clone();

    if( ! pszVATName )
    {
        pszVATName = CPLStrdup( CPLSPrintf(
            "RAT_%s_%d", poGeoRaster->pszDataTable, poGeoRaster->nRasterId ) );
    }

    // ----------------------------------------------------------
    // Format Table description
    // ----------------------------------------------------------

    char szDescription[OWTEXT];
    int  iCol = 0;

    strcpy( szDescription, "( ID NUMBER" );

    for( iCol = 0; iCol < poRAT->GetColumnCount(); iCol++ )
    {
        strcpy( szDescription, CPLSPrintf( "%s, %s",
            szDescription, poRAT->GetNameOfCol( iCol ) ) );

        if( poRAT->GetTypeOfCol( iCol ) == GFT_Integer )
        {
            strcpy( szDescription, CPLSPrintf( "%s NUMBER",
                szDescription ) );
        }
        if( poRAT->GetTypeOfCol( iCol ) == GFT_Real )
        {
            strcpy( szDescription, CPLSPrintf( "%s NUMBER",
                szDescription ) );
        }
        if( poRAT->GetTypeOfCol( iCol ) == GFT_String )
        {
            strcpy( szDescription, CPLSPrintf( "%s VARCHAR2(128)",
                szDescription ) );
        }
    }
    strcpy( szDescription, CPLSPrintf( "%s )", szDescription ) );

    // ----------------------------------------------------------
    // Create table
    // ----------------------------------------------------------

    char szUser[OWCODE];
    
    strcpy( szUser, CPLStrdup( poGeoRaster->poConnection->GetUser() ) );

    OWStatement* poStmt = poGeoRaster->poConnection->CreateStatement( CPLSPrintf(
        "DECLARE\n"
        "  TAB  VARCHAR2(68)    := UPPER(:1);\n"
        "  USR  VARCHAR2(68)    := UPPER(:3);\n"
        "  CNT  NUMBER          := 0;\n"
        "  GR1  SDO_GEORASTER   := NULL;\n"
        "BEGIN\n"
        "  EXECUTE IMMEDIATE 'SELECT COUNT(*) FROM ALL_TABLES\n"
        "    WHERE TABLE_NAME = :1 AND OWNER = :2 ' INTO CNT USING TAB, USR;\n"
        "  IF CNT = 0 THEN\n"
        "    EXECUTE IMMEDIATE 'CREATE TABLE '||TAB||' %s';\n"
        "  END IF;\n"
        "\n"
        "  SELECT %s INTO GR1 FROM %s T WHERE"
        "    T.%s.RasterDataTable = :rdt AND"
        "    T.%s.RasterId = :rid FOR UPDATE;\n"
        "  SDO_GEOR.setVAT(GR1, %d, '%s');\n"
        "  UPDATE %s T SET %s = GR1 WHERE"
        "    T.%s.RasterDataTable = :rdt AND"
        "    T.%s.RasterId = :rid;\n"
        "END;", 
        szDescription,
        poGeoRaster->pszColumn, poGeoRaster->pszTable,
        poGeoRaster->pszColumn, poGeoRaster->pszColumn, nBand, pszVATName,
        poGeoRaster->pszTable,  poGeoRaster->pszColumn,
        poGeoRaster->pszColumn, poGeoRaster->pszColumn  ) );

    poStmt->BindName( ":rdt", poGDS->poGeoRaster->pszDataTable );
    poStmt->BindName( ":rid", &poGDS->poGeoRaster->nRasterId );
    poStmt->Bind( pszVATName );
    poStmt->Bind( szUser );

    if( ! poStmt->Execute() )
    {
        delete poStmt;
        CPLError( CE_Failure, CPLE_AppDefined, "Create VAT Table Error!" );
        return CE_Failure;
    }

    delete poStmt;

    // ----------------------------------------------------------
    // Create virtual file with INSERT instatements
    // ----------------------------------------------------------

    char* pszInserts = NULL;
    int nSize        = 0;
    int iEntry       = 0;
    int nEntryCount  = poRAT->GetRowCount();
    int nColunsCount = poRAT->GetColumnCount();
    int nBytesCount  = 0;

    for( iEntry = 0; iEntry < nEntryCount; iEntry++ )
    {
        strcpy( szDescription, CPLSPrintf ( "INSERT INTO %s VALUES (%d",
            pszVATName, iEntry ) );

        for( iCol = 0; iCol < nColunsCount; iCol++ )
        {
            if( poRAT->GetTypeOfCol( iCol ) == GFT_String )
            {
                strcpy( szDescription, CPLSPrintf ( "%s, '%s'", szDescription,
                    poRAT->GetValueAsString( iEntry, iCol ) ) );
            }
            if( poRAT->GetTypeOfCol( iCol ) == GFT_Integer ||
                poRAT->GetTypeOfCol( iCol ) == GFT_Real )
            {
                strcpy( szDescription, CPLSPrintf ( "%s, %s", szDescription,
                    poRAT->GetValueAsString( iEntry, iCol ) ) );
            }
        }
        strcpy( szDescription, CPLSPrintf ( "%s);\n", szDescription ) );
        nBytesCount += strlen( szDescription );
        pszInserts = (char*) CPLRealloc( pszInserts, sizeof(char) * ( nBytesCount + 1 ));
        strcpy( &pszInserts[nSize], szDescription );
        nSize = nBytesCount;
    }

    // ----------------------------------------------------------
    // Insert Data to VAT
    // ----------------------------------------------------------

    poStmt = poGeoRaster->poConnection->CreateStatement( CPLSPrintf (
        "BEGIN\n"
        "%s"
        "END;",
        pszInserts ) );

    if( ! poStmt->Execute() )
    {
        CPLFree_nt( pszInserts );
        CPLError( CE_Failure, CPLE_AppDefined, "Insert on VAT Table Error!" );
        return CE_Failure;
    }

    CPLFree_nt( pszInserts );

    delete poStmt;

    poGDS->poGeoRaster->SetVAT( nBand, pszVATName );

    return CE_None;
}

//  ---------------------------------------------------------------------------
//                                                              GetDefaultRAT()
//  ---------------------------------------------------------------------------

const GDALRasterAttributeTable *GeoRasterRasterBand::GetDefaultRAT()
{
//TODO: This code is not finished

    GeoRasterDataset* poGDS = (GeoRasterDataset*) poDS;

    char* pszVATName = NULL;

    if( poGDS->poGeoRaster->GetVAT( nBand, pszVATName ) == NULL )
    {
        return NULL;
    }

    if( poDefaultRAT ) 
    {
        return poDefaultRAT->Clone();
    }

    OWConnection* phCntx = poGDS->poGeoRaster->poConnection;

    OCIParam* phDesc = phCntx->GetDescription( pszVATName );

    if( phDesc == NULL )
    {
        return NULL;
    }

    int i = 0;
    const char* pszFName = NULL;
    const char* pszFType = NULL;
    const char* pszFSize = NULL;

    while( phCntx->GetNextField( phDesc, i, pszFName, pszFType, pszFSize ) )
    {      
        poDefaultRAT->CreateColumn( pszFName, GFT_Integer, GFU_Generic );
        i++;
    }

    CPLFree_nt( pszVATName );

    return poDefaultRAT;
}
