/******************************************************************************
 * $Id: $
 *
 * Name:     georaster_rasterband.cpp
 * Project:  Oracle Spatial GeoRaster Driver
 * Purpose:  Implement GeoRasterRasterBand methods
 * Author:   Ivan Lucena [ivan.lucena at oracle.com]
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

#include "gdal_priv.h"

#include <string.h>

#include "georaster_priv.h"
#include "cpl_vsi.h"
#include "cpl_error.h"

//  ---------------------------------------------------------------------------
//                                                        GeoRasterRasterBand()
//  ---------------------------------------------------------------------------

GeoRasterRasterBand::GeoRasterRasterBand( GeoRasterDataset *poGDS,
                                          int nBand,
                                          int nLevel )
{
    poDS                = (GDALDataset*) poGDS;
    poGeoRaster         = poGDS->poGeoRaster;
    this->nBand         = nBand;
    this->eDataType     = OWGetDataType( poGeoRaster->sCellDepth.c_str() );
    poColorTable        = new GDALColorTable();
    poDefaultRAT        = NULL;
    pszVATName          = NULL;
    nRasterXSize        = poGeoRaster->nRasterColumns;
    nRasterYSize        = poGeoRaster->nRasterRows;
    nBlockXSize         = poGeoRaster->nColumnBlockSize;
    nBlockYSize         = poGeoRaster->nRowBlockSize;
    dfNoData            = 0.0;
    bValidStats         = false;
    nOverviewLevel      = nLevel;
    papoOverviews       = NULL;
    nOverviewCount      = 0;
    pahNoDataArray      = NULL;
    nNoDataArraySz      = 0;
    bHasNoDataArray     = false;
    
    //  -----------------------------------------------------------------------
    //  Initialize overview list
    //  -----------------------------------------------------------------------

    if( nLevel == 0 && poGeoRaster->nPyramidMaxLevel > 0 )
    {
        nOverviewCount  = poGeoRaster->nPyramidMaxLevel;
        papoOverviews   = (GeoRasterRasterBand**) VSIMalloc(
                sizeof(GeoRasterRasterBand*) * nOverviewCount );
        for( int i = 0; i < nOverviewCount; i++ )
        {
          papoOverviews[i] = new GeoRasterRasterBand(
                (GeoRasterDataset*) poDS, nBand, i + 1 );
        }
    }

    //  -----------------------------------------------------------------------
    //  Initialize this band as an overview
    //  -----------------------------------------------------------------------

    if( nLevel )
    {
        double dfScale  = pow( (double) 2.0, (double) nLevel );

        nRasterXSize    = (int) floor( nRasterXSize / dfScale );
        nRasterYSize    = (int) floor( nRasterYSize / dfScale );

        if( nRasterXSize <= ( nBlockXSize / 2.0 ) &&
            nRasterYSize <= ( nBlockYSize / 2.0 ) )
        {
            nBlockXSize = nRasterXSize;
            nBlockYSize = nRasterYSize;
        }
    }

    //  -----------------------------------------------------------------------
    //  Load NoData values and value ranges for this band (layer)
    //  -----------------------------------------------------------------------

    if( ( (GeoRasterDataset*) poDS)->bApplyNoDataArray )
    {
        CPLList* psList = NULL;
        int nLayerCount = 0;
        int nObjCount = 0;

        /*
         *  Count the number of NoData values and value ranges
         */

        for( psList = poGeoRaster->psNoDataList; psList ; psList = psList->psNext )
        {
            hNoDataItem* phItem = (hNoDataItem*) psList->pData;

            if( phItem->nBand == nBand )
            {
                nLayerCount++;
            }

            if( phItem->nBand == 0 )
            {
                nObjCount++;
            }

            if( phItem->nBand > nBand )
            {
                break;
            }
        }

        /*
         * Join the object nodata values to layer NoData values
         */

        nNoDataArraySz = nLayerCount + nObjCount;

        pahNoDataArray = (hNoDataItem*) VSIMalloc2( sizeof(hNoDataItem),
                nNoDataArraySz );

        int i = 0;
        bool bFirst = true;

        for( psList = poGeoRaster->psNoDataList ; psList && i < nNoDataArraySz;
             psList = psList->psNext )
        {
            hNoDataItem* phItem = (hNoDataItem*) psList->pData;

            if( phItem->nBand == nBand || phItem->nBand == 0 )
            {
                pahNoDataArray[i].nBand = nBand;
                pahNoDataArray[i].dfLower = phItem->dfLower;
                pahNoDataArray[i].dfUpper = phItem->dfUpper;
                i++;

                if( bFirst )
                {
                    bFirst = false;

                    /*
                     * Use the first value to assigned pixel values
                     * on method ApplyNoDataArray()
                     */
                    
                    dfNoData = phItem->dfLower;
                }
            }
        }

        bHasNoDataArray = nNoDataArraySz > 0;
    }
}

//  ---------------------------------------------------------------------------
//                                                       ~GeoRasterRasterBand()
//  ---------------------------------------------------------------------------

GeoRasterRasterBand::~GeoRasterRasterBand()
{
    delete poColorTable;
    delete poDefaultRAT;
    
    CPLFree( pszVATName );
    CPLFree( pahNoDataArray );

    if( nOverviewCount && papoOverviews )
    {
        for( int i = 0; i < nOverviewCount; i++ )
        {
            delete papoOverviews[i];
        }

        CPLFree( papoOverviews );
    }
}

//  ---------------------------------------------------------------------------
//                                                                 IReadBlock()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterRasterBand::IReadBlock( int nBlockXOff,
                                        int nBlockYOff,
                                        void *pImage )
{
    if( poGeoRaster->GetDataBlock( nBand,
                                   nOverviewLevel,
                                   nBlockXOff,
                                   nBlockYOff,
                                   pImage ) )
    {
        if( bHasNoDataArray )
        {
            ApplyNoDataArry( pImage );
        }

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
    if( poGeoRaster->SetDataBlock( nBand,
                                   nOverviewLevel,
                                   nBlockXOff,
                                   nBlockYOff,
                                   pImage ) )
    {
        return CE_None;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Error writing GeoRaster ofsett X (%d) offset Y (%d) band (%d)",
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
            if( nBand == 4 && poGDS->nBands == 4 &&
                poGeoRaster->iDefaultRedBand == 1 &&
                poGeoRaster->iDefaultGreenBand == 2 &&
                poGeoRaster->iDefaultBlueBand == 3 )
            {
                return GCI_AlphaBand;
            }
            else
            {
                return GCI_GrayIndex;
            }
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
    this->dfMin       = dfMin;
    this->dfMax       = dfMax;
    this->dfMean      = dfMean;
    this->dfStdDev    = dfStdDev;
    this->bValidStats = true;

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
        if( nNoDataArraySz )
        {
            *pbSuccess = true;
        }
        else
        {
            *pbSuccess = (int) poGeoRaster->GetNoData( nBand, &dfNoData );
        }
    }

    return dfNoData;
}

//  ---------------------------------------------------------------------------
//                                                             SetNoDataValue()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterRasterBand::SetNoDataValue( double dfNoDataValue )
{
    const char* pszFormat = 
        (eDataType == GDT_Float32 || eDataType == GDT_Float64) ? "%f" : "%.0f";

    poGeoRaster->SetNoData( (poDS->GetRasterCount() == 1) ? 0 : nBand,
        CPLSPrintf( pszFormat, dfNoDataValue ) );

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

    // ----------------------------------------------------------
    // Check if RAT is just colortable and/or histogram
    // ----------------------------------------------------------

    CPLString sColName = "";
    int  iCol = 0;
    int  nColCount = poRAT->GetColumnCount();

    for( iCol = 0; iCol < poRAT->GetColumnCount(); iCol++ )
    {
        sColName = poRAT->GetNameOfCol( iCol );

        if( EQUAL( sColName, "histogram" ) ||
            EQUAL( sColName, "red" ) ||
            EQUAL( sColName, "green" ) ||
            EQUAL( sColName, "blue" ) ||
            EQUAL( sColName, "opacity" ) )
        {
            nColCount--;
        }
    }

    if( nColCount < 2 )
    {
        return CE_None;
    }

    // ----------------------------------------------------------
    // Format Table description
    // ----------------------------------------------------------

    char szName[OWTEXT];
    char szDescription[OWTEXT];

    strcpy( szDescription, "( ID NUMBER" );

    for( iCol = 0; iCol < poRAT->GetColumnCount(); iCol++ )
    {
        strcpy( szName, poRAT->GetNameOfCol( iCol ) );

        strcpy( szDescription, CPLSPrintf( "%s, %s",
            szDescription, szName ) );

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
            strcpy( szDescription, CPLSPrintf( "%s VARCHAR2(%d)",
                szDescription, MAXLEN_VATSTR) );
        }
    }
    strcpy( szDescription, CPLSPrintf( "%s )", szDescription ) );

    // ----------------------------------------------------------
    // Create VAT named based on RDT and RID and Layer (nBand)
    // ----------------------------------------------------------

    if ( poGeoRaster->sValueAttributeTab.length() > 0 )
    {
        pszVATName = CPLStrdup( poGeoRaster->sValueAttributeTab.c_str() );
    }

    if( ! pszVATName )
    {
        pszVATName = CPLStrdup( CPLSPrintf(
            "RAT_%s_%d_%d", 
            poGeoRaster->sDataTable.c_str(),
            poGeoRaster->nRasterId,
            nBand ) );
    }

    // ----------------------------------------------------------
    // Create VAT table
    // ----------------------------------------------------------

    OWStatement* poStmt = poGeoRaster->poConnection->CreateStatement( CPLSPrintf(
        "DECLARE\n"
        "  TAB VARCHAR2(68)  := UPPER(:1);\n"
        "  CNT NUMBER        := 0;\n"
        "BEGIN\n"
        "  EXECUTE IMMEDIATE 'SELECT COUNT(*) FROM USER_TABLES\n"
        "    WHERE TABLE_NAME = :1' INTO CNT USING TAB;\n"
        "\n"
        "  IF NOT CNT = 0 THEN\n"
        "    EXECUTE IMMEDIATE 'DROP TABLE '||TAB||' PURGE';\n"
        "  END IF;\n"
        "\n"
        "  EXECUTE IMMEDIATE 'CREATE TABLE '||TAB||' %s';\n"
        "END;", szDescription ) );

    poStmt->Bind( pszVATName );

    if( ! poStmt->Execute() )
    {
        delete poStmt;
        CPLError( CE_Failure, CPLE_AppDefined, "Create VAT Table Error!" );
        return CE_Failure;
    }

    delete poStmt;

    // ----------------------------------------------------------
    // Insert Data to VAT
    // ----------------------------------------------------------

    int iEntry       = 0;
    int nEntryCount  = poRAT->GetRowCount();
    int nColunsCount = poRAT->GetColumnCount();
    int nVATStrSize  = MAXLEN_VATSTR * poGeoRaster->poConnection->GetCharSize();

    // ---------------------------
    // Allocate array of buffers
    // ---------------------------

    void** papWriteFields = (void**) VSIMalloc2(sizeof(void*), nColunsCount + 1);

    papWriteFields[0] = 
        (void*) VSIMalloc3(sizeof(int), sizeof(int), nEntryCount ); // ID field

    for(iCol = 0; iCol < nColunsCount; iCol++)
    {
        if( poRAT->GetTypeOfCol( iCol ) == GFT_String )
        {
            papWriteFields[iCol + 1] =
                (void*) VSIMalloc3(sizeof(char), nVATStrSize, nEntryCount );
        }
        if( poRAT->GetTypeOfCol( iCol ) == GFT_Integer )
        {
            papWriteFields[iCol + 1] =
                (void*) VSIMalloc3(sizeof(int), sizeof(int), nEntryCount );
        }
        if( poRAT->GetTypeOfCol( iCol ) == GFT_Real )
        {
            papWriteFields[iCol + 1] =
                 (void*) VSIMalloc3(sizeof(double), sizeof(double), nEntryCount );
        }
    }
    
    // ---------------------------
    // Load data to buffers
    // ---------------------------

    for( iEntry = 0; iEntry < nEntryCount; iEntry++ )
    {
        ((int *)(papWriteFields[0]))[iEntry] = iEntry; // ID field

        for(iCol = 0; iCol < nColunsCount; iCol++)
        {
            if( poRAT->GetTypeOfCol( iCol ) == GFT_String )
            {

                int nOffset = iEntry * nVATStrSize;
                char* pszTarget = ((char*)papWriteFields[iCol + 1]) + nOffset;
                const char *pszStrValue = poRAT->GetValueAsString(iEntry, iCol);
                int nLen = strlen( pszStrValue );
                nLen = nLen > ( nVATStrSize - 1 ) ? nVATStrSize : ( nVATStrSize - 1 );
                strncpy( pszTarget, pszStrValue, nLen );
                pszTarget[nLen] = '\0';
            }
            if( poRAT->GetTypeOfCol( iCol ) == GFT_Integer )
            {
                ((int *)(papWriteFields[iCol + 1]))[iEntry] =
                    poRAT->GetValueAsInt(iEntry, iCol);
            }
            if( poRAT->GetTypeOfCol( iCol ) == GFT_Real )
            {
                ((double *)(papWriteFields[iCol + 1]))[iEntry] =
                    poRAT->GetValueAsDouble(iEntry, iCol);
            }
        }
    }

    // ---------------------------
    // Prepare insert statement
    // ---------------------------

    CPLString osInsert = CPLSPrintf( "INSERT INTO %s VALUES (", pszVATName );
    
    for( iCol = 0; iCol < ( nColunsCount + 1); iCol++ )
    {
        if( iCol > 0 )
        {
            osInsert.append(", ");
        }
        osInsert.append( CPLSPrintf(":%d", iCol + 1) );
    }
    osInsert.append(")");

    poStmt = poGeoRaster->poConnection->CreateStatement( osInsert.c_str() );

    // ---------------------------
    // Bind buffers to columns
    // ---------------------------

    poStmt->Bind((int*) papWriteFields[0]); // ID field
    
    for(iCol = 0; iCol < nColunsCount; iCol++)
    {
        if( poRAT->GetTypeOfCol( iCol ) == GFT_String )
        {
            poStmt->Bind( (char*) papWriteFields[iCol + 1], nVATStrSize );
        }
        if( poRAT->GetTypeOfCol( iCol ) == GFT_Integer )
        {
            poStmt->Bind( (int*) papWriteFields[iCol + 1]);
        }
        if( poRAT->GetTypeOfCol( iCol ) == GFT_Real )
        {
            poStmt->Bind( (double*) papWriteFields[iCol + 1]);
        }
    }

    if( poStmt->Execute( iEntry ) )
    {
        poGDS->poGeoRaster->SetVAT( nBand, pszVATName );
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Insert VAT Error!" );
    }

    // ---------------------------
    // Clean up
    // ---------------------------

    for(iCol = 0; iCol < ( nColunsCount + 1); iCol++)
    {
        CPLFree( papWriteFields[iCol] );
    }
    
    CPLFree( papWriteFields );

    delete poStmt;

    return CE_None;
}

//  ---------------------------------------------------------------------------
//                                                              GetDefaultRAT()
//  ---------------------------------------------------------------------------

GDALRasterAttributeTable *GeoRasterRasterBand::GetDefaultRAT()
{
    if( poDefaultRAT )
    {
        return poDefaultRAT;
    }
    else
    {
        poDefaultRAT = new GDALDefaultRasterAttributeTable();
    }

    GeoRasterDataset* poGDS = (GeoRasterDataset*) poDS;

    // ----------------------------------------------------------
    // Get the name of the VAT Table
    // ----------------------------------------------------------

    char* pszVATName = poGDS->poGeoRaster->GetVAT( nBand );

    if( pszVATName == NULL )
    {
        return NULL;
    }

    OCIParam* phDesc = NULL;

    phDesc = poGDS->poGeoRaster->poConnection->GetDescription( pszVATName );

    if( phDesc == NULL )
    {
        return NULL;
    }

    // ----------------------------------------------------------
    // Create the RAT and the SELECT statemet based on fields description
    // ----------------------------------------------------------

    int   iCol = 0;
    char  szField[OWNAME];
    int   hType = 0;
    int   nSize = 0;
    int   nPrecision = 0;
    signed short nScale = 0;

    char szColumnList[OWTEXT];
    szColumnList[0] = '\0';

    while( poGDS->poGeoRaster->poConnection->GetNextField(
                phDesc, iCol, szField, &hType, &nSize, &nPrecision, &nScale ) )
    {
        switch( hType )
        {
            case SQLT_FLT:
                poDefaultRAT->CreateColumn( szField, GFT_Real, GFU_Generic );
                break;
            case SQLT_NUM:
                if( nPrecision == 0 )
                {
                    poDefaultRAT->CreateColumn( szField, GFT_Integer,
                        GFU_Generic );
                }
                else
                {
                    poDefaultRAT->CreateColumn( szField, GFT_Real,
                        GFU_Generic );
                }
                break;
            case SQLT_CHR:
            case SQLT_AFC:
            case SQLT_DAT:
            case SQLT_DATE:
            case SQLT_TIMESTAMP:
            case SQLT_TIMESTAMP_TZ:
            case SQLT_TIMESTAMP_LTZ:
            case SQLT_TIME:
            case SQLT_TIME_TZ:
                    poDefaultRAT->CreateColumn( szField, GFT_String, 
                        GFU_Generic );
                break;
            default:
                CPLDebug("GEORASTER", "VAT (%s) Column (%s) type (%d) not supported"
                    "as GDAL RAT", pszVATName, szField, hType );
                continue;
        }
        strcpy( szColumnList, CPLSPrintf( "%s substr(%s,1,%d),",
            szColumnList, szField, MIN(nSize,OWNAME) ) );

        iCol++;
    }

    szColumnList[strlen(szColumnList) - 1] = '\0'; // remove the last comma

    // ----------------------------------------------------------
    // Read VAT and load RAT
    // ----------------------------------------------------------

    OWStatement* poStmt = NULL;

    poStmt = poGeoRaster->poConnection->CreateStatement( CPLSPrintf (
        "SELECT %s FROM %s", szColumnList, pszVATName ) );

    char** papszValue = (char**) CPLMalloc( sizeof(char**) * iCol );

    int i = 0;

    for( i = 0; i < iCol; i++ )
    {
        papszValue[i] = (char*) CPLMalloc( sizeof(char*) * OWNAME );
        poStmt->Define( papszValue[i] );
    }

    if( ! poStmt->Execute() )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Error reading VAT %s",
            pszVATName );
        return NULL;
    }

    int iRow = 0;

    while( poStmt->Fetch() )
    {
        for( i = 0; i < iCol; i++ )
        {
           poDefaultRAT->SetValue( iRow, i, papszValue[i] );
        }
        iRow++;
    }

    for( i = 0; i < iCol; i++ )
    {
        CPLFree( papszValue[i] );
    }
    CPLFree( papszValue );

    delete poStmt;

    CPLFree( pszVATName );

    return poDefaultRAT;
}

//  ---------------------------------------------------------------------------
//                                                           GetOverviewCount()
//  ---------------------------------------------------------------------------

int GeoRasterRasterBand::GetOverviewCount()
{
    return nOverviewCount;
}

//  ---------------------------------------------------------------------------
//                                                           GetOverviewCount()
//  ---------------------------------------------------------------------------

GDALRasterBand* GeoRasterRasterBand::GetOverview( int nLevel )
{
    if( nLevel < nOverviewCount && papoOverviews[ nLevel ] )
    {
        return (GDALRasterBand*) papoOverviews[ nLevel ];
    }
    return (GDALRasterBand*) NULL;
}

//  ---------------------------------------------------------------------------
//                                                             CreateMaskBand()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterRasterBand::CreateMaskBand( int nFlags )
{
    (void) nFlags;

    if( ! poGeoRaster->bHasBitmapMask )
    {
        return CE_Failure;
    }

    return CE_None;
}

//  ---------------------------------------------------------------------------
//                                                                GetMaskBand()
//  ---------------------------------------------------------------------------

GDALRasterBand* GeoRasterRasterBand::GetMaskBand()
{
    GeoRasterDataset* poGDS = (GeoRasterDataset*) this->poDS;

    if( poGDS->poMaskBand != NULL )
    {
        return (GDALRasterBand*) poGDS->poMaskBand;
    }

    return (GDALRasterBand*) NULL;
}

//  ---------------------------------------------------------------------------
//                                                               GetMaskFlags()
//  ---------------------------------------------------------------------------

int GeoRasterRasterBand::GetMaskFlags()
{
    GeoRasterDataset* poGDS = (GeoRasterDataset*) this->poDS;

    if( poGDS->poMaskBand != NULL )
    {
        return GMF_PER_DATASET;
    }

    return GMF_ALL_VALID;
}

//  ---------------------------------------------------------------------------
//                                                            ApplyNoDataArry()
//  ---------------------------------------------------------------------------

void GeoRasterRasterBand::ApplyNoDataArry(void* pBuffer)
{
    int i = 0;
    int j = 0;
    long n = nBlockXSize * nBlockYSize;

    switch( eDataType )
    {
        case GDT_Byte:
        {
            GByte* pbBuffer = (GByte*) pBuffer;

            for( i = 0; i < n; i++ )
            {
                for( j = 0; j < nNoDataArraySz; j++ )
                {
                    if( pbBuffer[i] == (GByte) pahNoDataArray[j].dfLower ||
                      ( pbBuffer[i] >  (GByte) pahNoDataArray[j].dfLower &&
                        pbBuffer[i] <  (GByte) pahNoDataArray[j].dfUpper ) )
                    {
                        pbBuffer[i] = (GByte) dfNoData;
                    }
                }
            }

            break;
        }
        case GDT_Float32:
        case GDT_CFloat32:
        {
            float* pfBuffer = (float*) pBuffer;

            for( i = 0; i < n; i++ )
            {
                for( j = 0; j < nNoDataArraySz; j++ )
                {
                    if( pfBuffer[i] == (float) pahNoDataArray[j].dfLower ||
                      ( pfBuffer[i] >  (float) pahNoDataArray[j].dfLower &&
                        pfBuffer[i] <  (float) pahNoDataArray[j].dfUpper ) )
                    {
                        pfBuffer[i] = (float) dfNoData;
                    }
                }
            }

            break;
        }
        case GDT_Float64:
        case GDT_CFloat64:
        {
            double* pdfBuffer = (double*) pBuffer;

            for( i = 0; i < n; i++ )
            {
                for( j = 0; j < nNoDataArraySz; j++ )
                {
                    if( pdfBuffer[i] == (double) pahNoDataArray[j].dfLower ||
                      ( pdfBuffer[i] >  (double) pahNoDataArray[j].dfLower &&
                        pdfBuffer[i] <  (double) pahNoDataArray[j].dfUpper ) )
                    {
                        pdfBuffer[i] = (double) dfNoData;
                    }
                }
            }

            break;
        }
        case GDT_Int16:
        case GDT_CInt16:
        {
            GInt16* pnBuffer = (GInt16*) pBuffer;

            for( i = 0; i < n; i++ )
            {
                for( j = 0; j < nNoDataArraySz; j++ )
                {
                    if( pnBuffer[i] == (GInt16) pahNoDataArray[j].dfLower ||
                      ( pnBuffer[i] >  (GInt16) pahNoDataArray[j].dfLower &&
                        pnBuffer[i] <  (GInt16) pahNoDataArray[j].dfUpper ) )
                    {
                        pnBuffer[i] = (GInt16) dfNoData;
                    }
                }
            }

            break;
        }
        case GDT_Int32:
        case GDT_CInt32:
        {
            GInt32* pnBuffer = (GInt32*) pBuffer;

            for( i = 0; i < n; i++ )
            {
                for( j = 0; j < nNoDataArraySz; j++ )
                {
                    if( pnBuffer[i] == (GInt32) pahNoDataArray[j].dfLower ||
                      ( pnBuffer[i] >  (GInt32) pahNoDataArray[j].dfLower &&
                        pnBuffer[i] <  (GInt32) pahNoDataArray[j].dfUpper ) )
                    {
                        pnBuffer[i] = (GInt32) dfNoData;
                    }
                }
            }

            break;
        }
        case GDT_UInt16:
        {
            GUInt16* pnBuffer = (GUInt16*) pBuffer;

            for( i = 0; i < n; i++ )
            {
                for( j = 0; j < nNoDataArraySz; j++ )
                {
                    if( pnBuffer[i] == (GUInt16) pahNoDataArray[j].dfLower ||
                      ( pnBuffer[i] >  (GUInt16) pahNoDataArray[j].dfLower &&
                        pnBuffer[i] <  (GUInt16) pahNoDataArray[j].dfUpper ) )
                    {
                        pnBuffer[i] = (GUInt16) dfNoData;
                    }
                }
            }

            break;
        }
        case GDT_UInt32:
        {
            GUInt32* pnBuffer = (GUInt32*) pBuffer;

            for( i = 0; i < n; i++ )
            {
                for( j = 0; j < nNoDataArraySz; j++ )
                {
                    if( pnBuffer[i] == (GUInt32) pahNoDataArray[j].dfLower ||
                      ( pnBuffer[i] >  (GUInt32) pahNoDataArray[j].dfLower &&
                        pnBuffer[i] <  (GUInt32) pahNoDataArray[j].dfUpper ) )
                    {
                        pnBuffer[i] = (GUInt32) dfNoData;
                    }
                }
            }

            break;
        }
        default:
            ;
    }
}
