/******************************************************************************
 * $Id: $
 *
 * Name:     georaster_wrapper.cpp
 * Project:  Oracle Spatial GeoRaster Driver
 * Purpose:  Implement GeoRasterWrapper methods
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
//                                                           GeoRasterWrapper()
//  ---------------------------------------------------------------------------

GeoRasterWrapper::GeoRasterWrapper()
{
    pszTable            = NULL;
    pszColumn           = NULL;
    pszDataTable        = NULL;
    nRasterId           = -1;
    pszWhere            = NULL;
    phMetadata          = NULL;
    nRasterRows         = 0;
    nRasterColumns      = 0;
    nRasterBands        = 0;
    nRowBlockSize       = 0;
    nColumnBlockSize    = 0;
    nBandBlockSize      = 0;
    nTotalColumnBlocks  = 0;
    nTotalRowBlocks     = 0;
    nTotalBandBlocks    = 0;
    nCellSize           = 0;
    nCellSizeGDAL       = 0;
    dfXCoefficient[0]   = 0;
    dfXCoefficient[1]   = 0;
    dfXCoefficient[2]   = 1;
    dfYCoefficient[0]   = 0;
    dfYCoefficient[1]   = -1;
    dfYCoefficient[2]   = 0;
    pszCellDepth        = NULL;
    pahLocator          = NULL;
    pabyBlockBuf        = NULL;
    nPyraLevel          = 0;
    bIsReferenced       = false;
    poStmtIO            = NULL;
    nCurrentBandBlock   = -1;
    nCurrentXOffset     = -1;
    nCurrentYOffset     = -1;
    szInterleaving[0]   = 'B';
    szInterleaving[1]   = 'S';
    szInterleaving[2]   = 'Q';
    szInterleaving[3]   = '\0';
    bIOInitialized      = false;
    bBlobInitialized    = false;
    nSRID               = 0;
}

//  ---------------------------------------------------------------------------
//                                                           GeoRasterDataset()
//  ---------------------------------------------------------------------------

GeoRasterWrapper::~GeoRasterWrapper()
{
    CPLFree_nt( pszTable );
    CPLFree_nt( pszColumn );
    CPLFree_nt( pszDataTable );
    CPLFree_nt( pszWhere );
    XMLFree_nt( phMetadata );
    CPLFree_nt( pszCellDepth );
    OWStatement::Free( pahLocator, nBlockCount );
    CPLFree_nt( pabyBlockBuf );
    CPLFree_nt( poStmtIO );
}

//  ---------------------------------------------------------------------------
//                                                                       Open()
//  ---------------------------------------------------------------------------

GeoRasterWrapper* GeoRasterWrapper::Open( const char* pszStringID, 
                                          GDALAccess eAccess )
{
    (void) eAccess;

    //  -------------------------------------------------------------------
    //  Parse arguments
    //  -------------------------------------------------------------------

    char **papszParam = CSLTokenizeString2( 
                            strstr( pszStringID, ":" ) + 1, 
                            ID_SEPARATORS, 
                            CSLT_HONOURSTRINGS | CSLT_ALLOWEMPTYTOKENS );

    int nArgc = CSLCount( papszParam );

    // --------------------------------------------------------------------
    // Assume the default database
    // --------------------------------------------------------------------

    if( nArgc == 2 ) 
    {
        papszParam = CSLAddString( papszParam, "" );
    }

    //  ---------------------------------------------------------------
    //  Create a GeoRaster object
    //  ---------------------------------------------------------------

    GeoRasterWrapper* poGRW = new GeoRasterWrapper();

    if( ! poGRW )
    {
        return NULL;
    }

    //  ---------------------------------------------------------------
    //  Get a connection with Oracle server
    //  ---------------------------------------------------------------

    GeoRasterDriver* poDriver = NULL;
    
    poDriver = (GeoRasterDriver*) GDALGetDriverByName( "GeoRaster" );

    if( ! poDriver )
    {
        return NULL;
    }

    poGRW->poConnection = poDriver->GetConnection( 
        papszParam[0], 
        papszParam[1],
        papszParam[2] );

    if( ! poGRW->poConnection->Succed() )
    {
        CPLFree_nt( papszParam );
        delete poGRW;
        return NULL;
    }

    //  -------------------------------------------------------------------
    //  Assign parameters from Identification string
    //  -------------------------------------------------------------------

    switch( nArgc) 
    {
    case 6:
        poGRW->pszTable       = CPLStrdup( papszParam[3] );
        poGRW->pszColumn      = CPLStrdup( papszParam[4] );
        poGRW->pszWhere       = CPLStrdup( papszParam[5] );
        break;             // continue and try to catch a GeoRaster
    case 5 :
        if( OWIsNumeric( papszParam[4] ) )
        {
            poGRW->pszDataTable   = CPLStrdup( papszParam[3] );
            poGRW->nRasterId      = atoi( papszParam[4]);
            break;         // continue and try to catch a GeoRaster
        }
        else
        {
            poGRW->pszTable   = CPLStrdup( papszParam[3] );
            poGRW->pszColumn  = CPLStrdup( papszParam[4] );
            return poGRW;   // GeoRaster not uniquely defined
        }
    case 4 :
        poGRW->pszTable       = CPLStrdup( papszParam[3] );
        return poGRW;       // GeoRaster not uniquely defined
    default :
        return poGRW;       // GeoRaster not uniquely defined
    }

    //  -------------------------------------------------------------------
    //  Find Georaster Table/Column that uses the given RDT/RID
    //  -------------------------------------------------------------------

    if( nArgc == 5 )
    {
        OWStatement* poStmt = NULL;

        char  szTable[OWNAME];
        char  szColumn[OWNAME];

        poStmt = poGRW->poConnection->CreateStatement( 
            "SELECT TABLE_NAME, COLUMN_NAME\n"
            "FROM   USER_SDO_GEOR_SYSDATA\n"
            "WHERE  RDT_TABLE_NAME = UPPER(:1) AND RASTER_ID = :2 " );

        poStmt->Bind( poGRW->pszDataTable ); 
        poStmt->Bind( &poGRW->nRasterId );

        poStmt->Define( szTable );
        poStmt->Define( szColumn );

        if( poStmt->Execute() == false ||
            poStmt->Fetch()   == false )
        {
            delete poStmt;
            delete poGRW;
            return NULL;
        }

        delete poStmt;

        //  ---------------------------------------------------------------
        //  Take the first Table/Column found as a reference
        //  ---------------------------------------------------------------

        poGRW->pszTable   = CPLStrdup( szTable );
        poGRW->pszColumn  = CPLStrdup( szColumn );

        //  ---------------------------------------------------------------
        //  Make a where clause based on RTD and RID
        //  ---------------------------------------------------------------

        poGRW->pszWhere= CPLStrdup( CPLSPrintf( 
            "T.%s.RasterDataTable = UPPER('%s') AND T.%s.RasterId = %d",
            poGRW->pszColumn, 
            poGRW->pszDataTable, 
            poGRW->pszColumn, 
            poGRW->nRasterId ) );
    }

    //  -------------------------------------------------------------------
    //  Fetch an sdo_georaster object
    //  -------------------------------------------------------------------

    OWStatement* poStmt         = NULL;
    OCILobLocator* phLocator    = NULL;

    char szDataTable[OWNAME];
    int  nRasterId;

    poStmt = poGRW->poConnection->CreateStatement( CPLSPrintf( 
        "SELECT T.%s.RASTERDATATABLE,\n"
        "       T.%s.RASTERID,\n"
        "       T.%s.METADATA.getClobVal()\n"
        "FROM   %s T\n"
        "WHERE  %s", 
        poGRW->pszColumn, poGRW->pszColumn, poGRW->pszColumn, 
        poGRW->pszTable, poGRW->pszWhere ) );

    poStmt->Define( szDataTable );
    poStmt->Define( &nRasterId );
    poStmt->Define( &phLocator );

    if( poStmt->Execute() == false ||
        poStmt->Fetch()   == false )
    {
        delete poStmt;
        delete poGRW;
        return NULL;
    }

    poGRW->pszDataTable  = CPLStrdup( szDataTable );
    poGRW->nRasterId     = nRasterId;

    //  -------------------------------------------------------------------
    //  Check if there are more rows in that query result
    //  -------------------------------------------------------------------

    if( poStmt->Fetch() == true )
    {
    CPLDebug("GEOR","GeoRaster not unique (\"%s\")",poGRW->pszWhere); 
        delete poStmt;
        return poGRW;
    }

    //  -------------------------------------------------------------------
    //  Read Metadata XML in text form
    //  -------------------------------------------------------------------

    char* pszXML = NULL;

    pszXML = poStmt->ReadClob( phLocator );

    if( pszXML == NULL )
    {
        CPLDebug( "GEOR", "Error reading Metadata!" );
    }

    //  -------------------------------------------------------------------
    //  Get basic information from text metadata
    //  -------------------------------------------------------------------

    poGRW->GetRasterInfo( pszXML );

    //  -------------------------------------------------------------------
    //  Clean up and leave
    //  -------------------------------------------------------------------

    OCIDescriptorFree( phLocator, OCI_DTYPE_LOB );
    CPLFree( pszXML );
    delete poStmt;
    return poGRW;
}

//  ---------------------------------------------------------------------------
//                                                                CreateTable()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::CreateTable( const char* pszDescription )
{
    char szDescription[OWTEXT];

    if ( pszDescription  )
    {
        strcpy( szDescription, pszDescription );
    }
    else
    {
        strcpy( szDescription, 
           CPLSPrintf("(%s MDSYS.SDO_GEORASTER)", pszColumn ) );
    }

    //  -------------------------------------------------------------------
    //  If GeoRaster table doesn't exist, create it.
    //  -------------------------------------------------------------------

    OWStatement* poStmt = poConnection->CreateStatement( CPLSPrintf(
        "DECLARE\n"
        "  C_TABLE_NAME VARCHAR2(50) := UPPER(:1);\n"
        "  V_TABLE_NAME VARCHAR2(50);\n"
        "  CURSOR C1 IS SELECT TABLE_NAME\n"
        "     FROM ALL_TABLES WHERE TABLE_NAME = C_TABLE_NAME;\n"
        "BEGIN\n"
        "  OPEN C1;\n"
        "  FETCH C1 INTO V_TABLE_NAME;\n"
        "  IF C1%%NOTFOUND THEN\n"
        "    EXECUTE IMMEDIATE\n"
        "      'CREATE TABLE %s %s';\n"
        "      SDO_GEOR_UTL.createDMLTrigger(:1, :2);\n"
        "  END IF;\n"
        "  CLOSE C1;\n"
        "END;", pszTable, szDescription ) );

    CPLDebug("GEOR","%s-%s", pszTable, pszColumn );

    poStmt->Bind( pszTable );
    poStmt->Bind( pszColumn );

    if( ! poStmt->Execute() )
    {
        ObjFree_nt( poStmt );
        CPLError( CE_Failure, 
            CPLE_AppDefined, "Failure to create table (%s)", pszTable );
        return false;
    }

    delete poStmt;

    return true;
}

//  ---------------------------------------------------------------------------
//                                                                CreateBlank()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::CreateBlank( const char* pszInsert )
{
    char szInsert[OWTEXT];
    char szRDT[OWNAME];
    char szRID[OWCODE];
    char szValues[OWNAME];

    if( pszInsert )
    {
        strcpy( szValues, pszInsert );
    }
    else
    {
        strcpy( szValues, "VALUES (*)" );
    }

    if( pszDataTable )
    {
        strcpy( szRDT, pszDataTable );
    }
    else
    {
        strcpy( szRDT, "NULL" );
    }

    if ( nRasterId >= 0 )
    {
        strcpy( szRID, CPLSPrintf( "%d", nRasterId ) );
    }
    else
    {
        strcpy( szRID, "NULL" );
    }

    if( nRasterBands == 1 )
    {
        strcpy( szInsert, OWReplaceToken( szValues, '*', CPLSPrintf(
            "SDO_GEOR.createBlank(20001, SDO_NUMBER_ARRAY(0, 0), "
            "SDO_NUMBER_ARRAY(%d, %d), 0, %s, %s)",
            nRasterRows, nRasterColumns, szRDT, szRID ) ) );
    }
    else
    {
        strcpy( szInsert, OWReplaceToken( szValues, '*', CPLSPrintf(
            "SDO_GEOR.createBlank(21001, SDO_NUMBER_ARRAY(0, 0, 0), "
            "SDO_NUMBER_ARRAY(%d, %d, %d), 0, %s, %s)", 
            nRasterRows, nRasterColumns, nRasterBands, szRDT, szRID ) ) );
    }

    //  -----------------------------------------------------------
    //  Insert row with a GeoRaster
    //  -----------------------------------------------------------

    char szBindRDT[OWNAME];
    int  nBindRID = 0;

    szBindRDT[0]  = '\0';

    OWStatement* poStmt = poConnection->CreateStatement( CPLSPrintf( 
        "DECLARE\n"
        "  GR SDO_GEORASTER;\n"
        "BEGIN\n"
        "  INSERT INTO %s %s\n"
        "    RETURNING %s INTO GR;\n"
        "  SELECT GR.RASTERDATATABLE INTO :1 FROM DUAL;\n"
        "  SELECT GR.RASTERID        INTO :2 FROM DUAL;\n"
        "END;", 
        pszTable, szInsert, pszColumn ) );

    poStmt->Bind( szBindRDT );
    poStmt->Bind( &nBindRID );

    if( ! poStmt->Execute() )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Failure to insert a row" );
        ObjFree_nt( poStmt );
        return false;
    }

    delete poStmt;

    //  -----------------------------------------------------------
    //  Re-assign attributes
    //  -----------------------------------------------------------

    CPLFree_nt( pszDataTable );
    CPLFree_nt( pszWhere );

    pszDataTable    = CPLStrdup( szBindRDT );
    nRasterId       = nBindRID;
    pszWhere        = CPLStrdup( CPLSPrintf( 
        "T.%s.RasterDataTable = '%s' AND T.%s.RasterId = %d",
        pszColumn, pszDataTable, pszColumn, nRasterId ) );

    return true;
}

//  ---------------------------------------------------------------------------
//                                                                  SetFormat()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::ChangeFormat( const char* pszBlockSize,
                                     const char* pszInterleaving,
                                     const char* pszCellDepth,
                                     const char* pszPyramid,
                                     const char* pszCompression )
{
    char szParam[OWTEXT];

    if( pszCellDepth )
    {
        strcpy( szParam, CPLSPrintf( "cellDepth=%s", pszCellDepth ) );
    }

// Interleaving

    if( pszInterleaving )
    {
        if( EQUAL( pszInterleaving, "BAND" ) )
        {
            strcpy( szParam, CPLSPrintf( "%s interleaving=BSQ", szParam ) );
        }
        if( EQUAL( pszInterleaving, "LINE" ) )
        {
            strcpy( szParam, CPLSPrintf( "%s interleaving=BIL", szParam ) );
        }
        if( EQUAL( pszInterleaving, "PIXEL" ) )
        {
            strcpy( szParam, CPLSPrintf( "%s interleaving=BIP", szParam ) );
        }
    }

// Pyramid

    if( pszPyramid )
    {
        strcpy( szParam, 
            CPLSPrintf( "%s pyramid=%s", szParam, pszPyramid ) );
    }
    else
    {
        strcpy( szParam, CPLSPrintf( "%s pyramid=FALSE", szParam ) );
    }

// Compression

    if( pszCompression )
    {
        strcpy( szParam, 
            CPLSPrintf( "%s compression=%s", szParam, pszCompression ) );
    }
    else
    {
        strcpy( szParam, CPLSPrintf( "%s compression=NONE", szParam ) );
    }

// BlockSize

    if( pszBlockSize )
    {
        strcpy( szParam, 
            CPLSPrintf( "blockSize=%s %s", pszBlockSize, szParam ) );

    }
    else
    {
        if ( nRasterBands == 1 )
        {
            strcpy( szParam, CPLSPrintf( 
                "blockSize=(256, 256) %s", szParam ) );
        }
        else if ( nRasterBands == 3 )
        {
            strcpy( szParam, CPLSPrintf( 
                "blockSize=(256, 256, %d) %s", nRasterBands, szParam ) );
        }
        else
        {
            strcpy( szParam, CPLSPrintf( 
                "blockSize=(256, 256, 1) %s", szParam ) );
        }
    }

    //  -----------------------------------------------------------
    //  Change GeoRaster Storage Parameters
    //  -----------------------------------------------------------

    OWStatement* poStmt = poConnection->CreateStatement( CPLSPrintf( 
        "DECLARE\n"
        "  GR1 sdo_georaster;\n"
        "  GR2 sdo_georaster;\n"
        "BEGIN\n"
        "  SELECT %s INTO GR2 FROM %s T WHERE %s FOR UPDATE;\n"
        "  SELECT %s INTO GR1 FROM %s T WHERE %s;\n"
        "  SDO_GEOR.changeFormatCopy(GR1, '%s', GR2);\n"
        "  UPDATE %s T SET %s = GR2 WHERE %s;\n"
        "  COMMIT;\n"
        "END;",
        pszColumn, pszTable, pszWhere,
        pszColumn, pszTable, pszWhere, szParam,
        pszTable, pszColumn, pszWhere ) );

    if( ! poStmt->Execute() )
    {
        ObjFree_nt( poStmt );
        CPLError( CE_Failure, 
            CPLE_AppDefined, "Failure to set storage parameters" );
        return false;
    }

    delete poStmt;

    return true;
}

//  ---------------------------------------------------------------------------
//                                                      CreateRasterDataTable()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::CreateDataTable()
{
//  If Raster Data Table doesn't exist, create it.
//  If it exist, delete the rows with the same RasterID.

    OWStatement* poStmt = poConnection->CreateStatement( CPLSPrintf(
        "DECLARE\n"
        "  C_TABLE_NAME VARCHAR2(50) := UPPER('%s');\n"
        "  V_TABLE_NAME VARCHAR2(50);\n"
        "  CURSOR C1 IS SELECT TABLE_NAME\n"
        "    FROM ALL_TABLES WHERE TABLE_NAME = C_TABLE_NAME;\n"
        "BEGIN\n"
        "  OPEN C1;\n"
        "  FETCH C1 INTO V_TABLE_NAME;\n"
        "  IF C1%%NOTFOUND THEN\n"
        "    EXECUTE IMMEDIATE\n"
        "      'CREATE TABLE %s OF MDSYS.SDO_RASTER\n"
        "        (PRIMARY KEY (RASTERID,\n"
        "                  PYRAMIDLEVEL,\n"
        "                  BANDBLOCKNUMBER,\n"
        "                  ROWBLOCKNUMBER,\n"
        "                  COLUMNBLOCKNUMBER))\n"
        "         LOB(RASTERBLOCK) STORE AS (NOCACHE NOLOGGING)';\n"
        "  ELSE\n"
        "    EXECUTE IMMEDIATE\n"
        "      'DELETE FROM C1 WHERE RASTERID = %d';\n"
        "  END IF;\n"
        "  CLOSE C1;\n"
        "END;", 
        pszDataTable, pszDataTable, nRasterId ) );

    if( ! poStmt->Execute() )
    {
        delete poStmt;
        CPLError( CE_Failure, 
            CPLE_AppDefined, "Failure to create raster data table " );
        return false;
    }

    delete poStmt;

    return true;
}

//  ---------------------------------------------------------------------------
//                                                             CreateDataRows()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::CreateDataRows()
{
    OWStatement* poStmt = poConnection->CreateStatement( CPLSPrintf( 
        "DECLARE\n"
        "  GR sdo_georaster;\n"
        "BEGIN\n"
        "  SELECT %s INTO GR FROM %s T WHERE %s FOR UPDATE;\n"
        "  SDO_GEOR.changeCellValue( GR, sdo_number_array(0, 0, 1, 1), '0', 1);\n"
        "  UPDATE %s T SET %s = GR WHERE %s;\n"
        "  COMMIT;\n"
        "END;",
        pszColumn, pszTable, pszWhere,
        pszTable, pszColumn, pszWhere ) );

    if( ! poStmt->Execute() )
    {
        ObjFree_nt( poStmt );
        CPLError( CE_Failure, 
            CPLE_AppDefined, "Failure to do changeCellValue " );
        return false;
    }

    delete poStmt;

    return true;
}

//  ---------------------------------------------------------------------------
//                                                              SetExtentSRID()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::SetGeoReference()
{
    OWStatement* poStmt = poConnection->CreateStatement( CPLSPrintf(
        "DECLARE\n"
        "  GR sdo_georaster;\n"
        "BEGIN\n"
        "  SELECT %s INTO GR FROM %s T WHERE %s FOR UPDATE;\n"
        "  SDO_GEOR.georeference( gr, :1, 1,\n"
        "    SDO_NUMBER_ARRAY(:2, :3, :4), SDO_NUMBER_ARRAY(:5, :6, :7));\n"
        "  UPDATE %s T SET %s = GR WHERE %s;\n"
        "  COMMIT;\n"
        "END;", 
        pszColumn, pszTable, pszWhere,
        pszTable, pszColumn, pszWhere ) );

    poStmt->Bind( &nSRID );
    poStmt->Bind( &dfXCoefficient[0] );
    poStmt->Bind( &dfXCoefficient[1] );
    poStmt->Bind( &dfXCoefficient[2] );
    poStmt->Bind( &dfYCoefficient[0] );
    poStmt->Bind( &dfYCoefficient[1] );
    poStmt->Bind( &dfYCoefficient[2] );

    if( ! poStmt->Execute() )
    {
        ObjFree_nt( poStmt );
        CPLError( CE_Failure, 
            CPLE_AppDefined, "Failure to set Georeference" );
        return false;
    }

    delete poStmt;

    return true;
}

//  ---------------------------------------------------------------------------
//                                                                  GetWKText()
//  ---------------------------------------------------------------------------

char* GeoRasterWrapper::GetWKText( int nSRIDin, bool bCode )
{
    char szWKText[OWTEXT];
    char szWKTcls[OWTEXT];

    OWStatement* poStmt = poConnection->CreateStatement(
        "SELECT WKTEXT,\n"
        "SELECT REGEXP_REPLACE(WKTEXT, '\\(EPSG [A-Z]+ [0-9]+\\)', '')\n"
        "FROM   MDSYS.CS_SRS\n"
        "WHERE  SRID = :1" );

    poStmt->Bind( &nSRIDin );
    poStmt->Define( szWKText,  OWTEXT );
    poStmt->Define( szWKTcls, OWTEXT );

    if( poStmt->Execute() == false ||
        poStmt->Fetch()   == false )
    {
        delete poStmt;
        return NULL;
    }

    delete poStmt;

    if( bCode )
    {
        return CPLStrdup( szWKText );
    }
    else
    {
        return CPLStrdup( szWKTcls );
    }
}

//  ---------------------------------------------------------------------------
//                                                              GetRasterInfo()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::GetRasterInfo( char* pszXML )
{
    phMetadata      = CPLParseXMLString( pszXML );

    //  -------------------------------------------------------------------
    //  Get dimensions
    //  -------------------------------------------------------------------

    int nCount  = 0;

    CPLXMLNode* phDimSize   = NULL;
    const char* pszType     = NULL;

    nCount      = atoi( CPLGetXMLValue( phMetadata, 
                  "rasterInfo.totalDimensions", "0" ) );
    phDimSize   = CPLGetXMLNode( phMetadata, "rasterInfo.dimensionSize" );

    int i = 0;

    for( i = 0; i < nCount; i++ )
    {
        pszType = CPLGetXMLValue( phDimSize, "type", "0" );

        if( EQUAL( pszType, "ROW" ) )
        {
            nRasterRows = atoi( CPLGetXMLValue( phDimSize, "size", "0" ) );
        }

        if( EQUAL( pszType, "COLUMN" ) )
        {
            nRasterColumns = atoi( CPLGetXMLValue( phDimSize, "size", "0" ) );
        }

        if( EQUAL( pszType, "BAND" ) )
        {
            nRasterBands = atoi( CPLGetXMLValue( phDimSize, "size", "0" ) );
        }

        phDimSize = phDimSize->psNext;
    }

    if( nRasterBands == 0 )
    {
        nRasterBands = 1;
    }

    //  ------------------------------------------------------------------- 
    //  Get Interleaving mode
    //  ------------------------------------------------------------------- 

    strncpy( szInterleaving, CPLGetXMLValue( phMetadata, 
                            "rasterInfo.interleaving", "BSQ" ), 4 );

    //  -------------------------------------------------------------------
    //  Get blocking
    //  -------------------------------------------------------------------

    nRowBlockSize       = atoi( CPLGetXMLValue( phMetadata, 
                            "rasterInfo.blocking.rowBlockSize", "0" ) );

    nColumnBlockSize    = atoi( CPLGetXMLValue( phMetadata,  
                            "rasterInfo.blocking.columnBlockSize", "0" ) );

    nBandBlockSize      = atoi( CPLGetXMLValue( phMetadata,  
                            "rasterInfo.blocking.bandBlockSize", "-1" ) );

    nTotalColumnBlocks  = atoi( CPLGetXMLValue( phMetadata,  
                            "rasterInfo.blocking.totalColumnBlocks","0") );

    nTotalRowBlocks     = atoi( CPLGetXMLValue( phMetadata,  
                            "rasterInfo.blocking.totalRowBlocks", "0" ) );

    nTotalBandBlocks    = atoi( CPLGetXMLValue( phMetadata,  
                            "rasterInfo.blocking.totalBandBlocks", "1" ) );

    if( nBandBlockSize == -1 )
    {
        CPLDebug("GEOR","There is no BandBlockSize, "
                 "assume number of bands (%d)", nRasterBands );

       nBandBlockSize = nRasterBands;
    }

    //  -------------------------------------------------------------------
    //  Get data type
    //  -------------------------------------------------------------------

    pszCellDepth        = CPLStrdup( CPLGetXMLValue( phMetadata, 
                            "rasterInfo.cellDepth", "8BIT_U" ) );

    sscanf( pszCellDepth, "%d", &nCellSize );

    nCellSize           /= 8;

    eType               = OWGetDataType( pszCellDepth );

    nCellSizeGDAL       = GDALGetDataTypeSize( eType ) / 8;

    //  -------------------------------------------------------------------
    //  Get default RGB Bands
    //  -------------------------------------------------------------------

    iDefaultRedBand     = atoi( CPLGetXMLValue( phMetadata, 
                            "objectInfo.defaultRed", "-1" ) );

    iDefaultGreenBand   = atoi( CPLGetXMLValue( phMetadata,
                            "objectInfo.defaultGreen", "-1" ) );

    iDefaultBlueBand    = atoi( CPLGetXMLValue( phMetadata,
                            "objectInfo.defaultBlue", "-1" ) );

    //  ------------------------------------------------------------------- 
    //  Prepare to get Extents
    //  ------------------------------------------------------------------- 

    bIsReferenced       = EQUAL( "TRUE", CPLGetXMLValue( phMetadata, 
                            "spatialReferenceInfo.isReferenced", "FALSE" ) );

    nSRID               = atoi( CPLGetXMLValue( phMetadata, 
                            "spatialReferenceInfo.SRID", "0" ) );
}

//  ---------------------------------------------------------------------------
//                                                             GetImageExtent()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::GetImageExtent( double *padfTransform )
{
    OWStatement*    poStmt        = NULL;
    sdo_geometry*   poUpperLeft   = NULL;
    sdo_geometry*   poUpperRight  = NULL;
    sdo_geometry*   poLowerLeft   = NULL;
    sdo_geometry*   poLowerRight  = NULL;

    poStmt = poConnection->CreateStatement( CPLSPrintf( 
        "SELECT\n"
        "  SDO_GEOR.getModelCoordinate(%s, 0, SDO_NUMBER_ARRAY(%d, %d)),\n"
        "  SDO_GEOR.getModelCoordinate(%s, 0, SDO_NUMBER_ARRAY(%d, %d)),\n"
        "  SDO_GEOR.getModelCoordinate(%s, 0, SDO_NUMBER_ARRAY(%d, %d)),\n"
        "  SDO_GEOR.getModelCoordinate(%s, 0, SDO_NUMBER_ARRAY(%d, %d))\n"
        "FROM  %s T\n"
        "WHERE %s",
        pszColumn, 0,            0,
        pszColumn, 0,            nRasterColumns,
        pszColumn, nRasterRows,  0, 
        pszColumn, nRasterRows,  nRasterColumns,
        pszTable,
        pszWhere ) );

    poStmt->Define( &poUpperLeft );
    poStmt->Define( &poLowerLeft );
    poStmt->Define( &poUpperRight );
    poStmt->Define( &poLowerRight );

    if( poStmt->Execute() == false ||
        poStmt->Fetch()   == false )
    {
        ObjFree_nt( poStmt );
        return false;
    }

    double dfULx = poStmt->GetDouble( &poUpperLeft->sdo_point.x );
    double dfURx = poStmt->GetDouble( &poUpperRight->sdo_point.x );
    double dfLRx = poStmt->GetDouble( &poLowerRight->sdo_point.x );

    double dfULy = poStmt->GetDouble( &poUpperLeft->sdo_point.y );
    double dfLLy = poStmt->GetDouble( &poLowerLeft->sdo_point.y );
    double dfLRy = poStmt->GetDouble( &poLowerRight->sdo_point.y );

    ObjFree_nt( poStmt );

    // --------------------------------------------------------------------
    // Generate a Affine transformation matrix
    // --------------------------------------------------------------------

    double dfRotation = 0.0;

    if( ! CPLIsEqual( dfULy, dfLLy ) )
    {
        dfRotation = ( dfURx - dfULx ) / ( dfLLy - dfULy );
    }

    padfTransform[0] = dfULx;
    padfTransform[1] = ( dfLRx - dfULx ) / nRasterColumns;
    padfTransform[2] = dfRotation;

    padfTransform[3] = dfULy;
    padfTransform[4] = -dfRotation;
    padfTransform[5] = ( dfLRy - dfULy ) / nRasterRows;

    bool bUpLeft = EQUAL( "UPPERLEFT", CPLGetXMLValue( phMetadata, 
        "spatialReferenceInfo.modelCoordinateLocation", "UPPERLEFT" ) );

    if( ! bUpLeft )
    {
        padfTransform[0] -= padfTransform[1] / 2;
        padfTransform[3] -= padfTransform[5] / 2;
    }

    return true;
}
//  ---------------------------------------------------------------------------
//                                                              GetStatistics()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::GetStatistics( double dfMin,
                                      double dfMax,
                                      double dfMean,
                                      double dfStdDev,
                                      int nBand )
{
    int n = 1;

    CPLXMLNode *phSubLayer = CPLGetXMLNode( phMetadata, 
        "layerInfo.subLayer" );

    for( n = 1 ; phSubLayer ; phSubLayer = phSubLayer->psNext, n++ )
    {
        if( n == nBand && CPLGetXMLNode( phSubLayer, "statisticDataset" ) )
        {
            dfMin    = atoi( CPLGetXMLValue( phSubLayer, 
                                "statisticDataset.MIM",  "0.0" ) );

            dfMax    = atoi( CPLGetXMLValue( phSubLayer,  
                                "statisticDataset.MAX",  "0.0" ) );

            dfMean   = atoi( CPLGetXMLValue( phSubLayer,  
                                "statisticDataset.MEAN", "0.0" ) );

            dfStdDev = atoi( CPLGetXMLValue( phSubLayer,  
                                "statisticDataset.STD",  "0.0" ) );

            XMLFree_nt( phSubLayer );

            return true;
        }
    }
    return false;
}

//  ---------------------------------------------------------------------------
//                                                              HasColorTable()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::HasColorTable( int nBand )
{
    CPLXMLNode *psLayers;

    int n = 1;

    psLayers = CPLGetXMLNode( phMetadata, "layerInfo.subLayer" );

    for( ; psLayers; psLayers = psLayers->psNext, n++ )
    {
        if( n == nBand )
        {
            if( CPLGetXMLNode( psLayers, "colorMap.colors" ) )
            {
                return true;
            }
        }
    }

    return false;
}

//  ---------------------------------------------------------------------------
//                                                              GetColorTable()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::GetColorTable( int nBand, GDALColorTable* poCT )
{
    GDALColorEntry oEntry;

    CPLXMLNode *psLayers;
    CPLXMLNode *psColors;

    int n = 1;

    psLayers = CPLGetXMLNode( phMetadata, "layerInfo.subLayer" );

    for( ; psLayers; psLayers = psLayers->psNext, n++ )
    {
        if( n != nBand )
        {
            continue;
        }

        psColors    = CPLGetXMLNode( psLayers, "colorMap.colors.cell" );

        int iColor = 0;

        for(  ; psColors; psColors = psColors->psNext )
        {
            iColor    = (short) atoi( CPLGetXMLValue( psColors, "value", "0" ) );
            oEntry.c1 = (short) atoi( CPLGetXMLValue( psColors, "red",   "0" ) );
            oEntry.c2 = (short) atoi( CPLGetXMLValue( psColors, "green", "0" ) );
            oEntry.c3 = (short) atoi( CPLGetXMLValue( psColors, "blue",  "0" ) );
            oEntry.c4 = (short) atoi( CPLGetXMLValue( psColors, "alpha", "0" ) );
            poCT->SetColorEntry( iColor, &oEntry );
        }
        break;
    }
}

//  ---------------------------------------------------------------------------
//                                                              SetColorTable()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::SetColorTable( int nBand, GDALColorTable* poCT )
{
    GDALColorEntry oEntry;

    CPLXMLNode *psLayers;
    CPLXMLNode *psColors;

    int n = 1;

    psLayers = CPLGetXMLNode( phMetadata, "layerInfo.subLayer" );

    for( ; psLayers; psLayers = psLayers->psNext, n++ )
    {
        if( n != nBand )
        {
            continue;
        }

        psColors    = CPLGetXMLNode( psLayers, "colorMap.colors.cell" );

        int iColor = 0;

        for(  ; psColors; psColors = psColors->psNext )
        {
            iColor    = (short) atoi( CPLGetXMLValue( psColors, "value", "0" ) );
            oEntry.c1 = (short) atoi( CPLGetXMLValue( psColors, "red",   "0" ) );
            oEntry.c2 = (short) atoi( CPLGetXMLValue( psColors, "green", "0" ) );
            oEntry.c3 = (short) atoi( CPLGetXMLValue( psColors, "blue",  "0" ) );
            oEntry.c4 = (short) atoi( CPLGetXMLValue( psColors, "alpha", "0" ) );
            poCT->SetColorEntry( iColor, &oEntry );
        }
        break;
    }
}

//  ---------------------------------------------------------------------------
//                                                             InitializeBlob()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::InitializeBlob()
{
    CPLDebug("GEOR", "Initialize, SetGeoreference(), nSRID=%d", nSRID );

    if( nSRID > 0 )
    {
        if( ! SetGeoReference() )
        {
           return false;
        }
    }

    CPLDebug("GEOR", "Initialize, CreateDataRows()");

    if( ! CreateDataRows() )
    {
       return false;
    }

    bBlobInitialized = true;

    return true;
}

//  ---------------------------------------------------------------------------
//                                                               InitializeIO()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::InitializeIO()
{
    nBlockCount     = nTotalColumnBlocks * nTotalRowBlocks * nTotalBandBlocks;
    nBlockBytes     = nColumnBlockSize   * nRowBlockSize   * nBandBlockSize *
                      nCellSize;
    nBlockBytesGDAL = nColumnBlockSize   * nRowBlockSize   * nCellSizeGDAL;

    // --------------------------------------------------------------------
    // Allocate buffer for one RASTERBLOCK
    // --------------------------------------------------------------------

    pabyBlockBuf    = (GByte*) VSIMalloc( nBlockBytes );

    if ( pabyBlockBuf == NULL )
    {
        return false;
    }

    // --------------------------------------------------------------------
    // Allocate array of LOB Locators
    // --------------------------------------------------------------------

    pahLocator = (OCILobLocator**) VSIMalloc( sizeof(OCILobLocator*) 
               * nBlockCount );

    if ( pahLocator == NULL )
    {
        return false;
    }

    bIOInitialized = true;

    return true;
}

//  ---------------------------------------------------------------------------
//                                                               GetBandBlock()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::GetBandBlock( int nBand,
                                     int nXOffset,
                                     int nYOffset,
                                     void* pData )
{
    nBandBlock = nBandBlockSize == 1 ? 0 : nBand % nTotalBandBlocks;
    int nBlock = ( ( nYOffset * nTotalColumnBlocks ) + nXOffset );

    if( ! bIOInitialized )
    {
        InitializeIO();
    }

    if ( nCurrentBandBlock != nBandBlock )
    {
        nCurrentBandBlock = nBandBlock;
        nCurrentXOffset = -1;
        nCurrentYOffset = -1;

        CPLFree_nt( poStmtIO );

        poStmtIO = poConnection->CreateStatement( CPLSPrintf(
            "SELECT RASTERBLOCK\n"
            "FROM   %s\n"
            "WHERE  RASTERID = :1 AND\n"
            "       BANDBLOCKNUMBER = :2 AND\n"
            "       PYRAMIDLEVEL = :3\n"
            "ORDER BY\n"
            "       ROWBLOCKNUMBER ASC,\n"
            "       COLUMNBLOCKNUMBER ASC", 
            pszDataTable ) );

        poStmtIO->Bind( &nRasterId );
        poStmtIO->Bind( &nBandBlock );
        poStmtIO->Bind( &nPyraLevel );

        poStmtIO->Define( pahLocator, nBlockCount );

        poStmtIO->Execute();

        if( poStmtIO->Fetch( nBlockCount ) == false )
        {
            return false;
        }
    }

    if ( nCurrentXOffset != nXOffset ||
         nCurrentYOffset != nYOffset )
    {
        nCurrentXOffset = nXOffset;
        nCurrentYOffset = nYOffset;

        if( ! poStmtIO->ReadBlob( pahLocator[nBlock], 
                                  pabyBlockBuf, nBlockBytes ) )
        {
            return false;
        }
    }

    //  --------------------------------------------------------------------
    //  Unpacking bits 
    //  --------------------------------------------------------------------

    if( EQUAL( pszCellDepth, "4BIT" ) )
    {
        GByte *pabyData = (GByte *) pabyBlockBuf;
        int  ii = 0;
        for( ii = nBlockBytes; ii >= 0; ii -= 2 )
        {
            int k = ii>>1;
            pabyData[ii+1] = (pabyData[k]>>4) & 0xf;
            pabyData[ii]   = (pabyData[k]) & 0xf;
        }
    }

    if( EQUAL( pszCellDepth, "2BIT" ) )
    {
        GByte *pabyData = (GByte *) pabyBlockBuf;
        int  ii = 0;
        for( ii = nBlockBytes; ii >= 0; ii -= 4 )
        {
            int k = ii>>2;
            pabyData[ii+3] = (pabyData[k]>>6) & 0x3;
            pabyData[ii+2] = (pabyData[k]>>4) & 0x3;
            pabyData[ii+1] = (pabyData[k]>>2) & 0x3;
            pabyData[ii]   = (pabyData[k]) & 0x3;
        }
    }

    if( EQUAL( pszCellDepth, "1BIT" ) )
    {
        GByte *pabyData = (GByte *) pabyBlockBuf;
        int  ii = 0;
        for( ii = nBlockBytes; ii >= 0; ii-- )
        {
            if( (pabyData[ii>>3] & (1 << (ii & 0x7))) )
                pabyData[ii] = 1;
            else
                pabyData[ii] = 0;
        }
    }

    //  --------------------------------------------------------------------
    //  Uninterleave it if necessary
    //  --------------------------------------------------------------------

    if( EQUAL( szInterleaving, "BSQ" ) )
    {
        int nStart  = ( ( nBand - 1 ) % nRasterBands ) * nBlockBytesGDAL;
        memcpy( pData, &pabyBlockBuf[nStart], nBlockBytesGDAL );
    }
    else
    {
        int nStart  = ( ( nBand - 1 ) % nRasterBands ) * nCellSizeGDAL;
        int nIncr   = nBandBlockSize * nCellSizeGDAL;
        int nSize   = nCellSizeGDAL;

        if( EQUAL( szInterleaving, "BIL" ) )
        {
            nStart  *= nColumnBlockSize;
            nIncr   *= nColumnBlockSize;
            nSize   *= nColumnBlockSize;
        }

        GByte* pabyData = (GByte*) pData;
        int  ii = 0;
        int  jj = nStart;
        for( ii = 0; ii < nBlockBytesGDAL; 
             ii += ( nCellSizeGDAL * nSize ), 
             jj += nIncr )
        {
            memcpy( &pabyData[ii], &pabyBlockBuf[jj], nSize );
        }
    }

    return true;
}

//  ---------------------------------------------------------------------------
//                                                               SetBandBlock()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::SetBandBlock( int nBand, 
                                     int nXOffset, 
                                     int nYOffset, 
                                     void* pData )
{
    nBandBlock = nBandBlockSize == 1 ? 0 : nBand % nTotalBandBlocks;
    int nBlock = ( ( nYOffset * nTotalColumnBlocks ) + nXOffset );

    if( ! bIOInitialized )
    {
        InitializeIO();
        InitializeBlob();
    }

    if ( nCurrentBandBlock != nBandBlock )
    {
        nCurrentBandBlock = nBandBlock;
        nCurrentXOffset = -1;
        nCurrentYOffset = -1;

        CPLFree_nt( poStmtIO );

        poStmtIO = poConnection->CreateStatement( CPLSPrintf(
            "SELECT RASTERBLOCK\n"
            "FROM   %s\n"
            "WHERE  RASTERID = :1 AND\n"
            "       BANDBLOCKNUMBER = :2 AND\n"
            "       PYRAMIDLEVEL = :3\n"
            "ORDER BY\n"
            "       ROWBLOCKNUMBER ASC,\n"
            "       COLUMNBLOCKNUMBER ASC\n"
            "FOR UPDATE ", 
            pszDataTable ) );

        poStmtIO->Bind( &nRasterId );
        poStmtIO->Bind( &nBandBlock );
        poStmtIO->Bind( &nPyraLevel );

        poStmtIO->Define( pahLocator, nBlockCount );

        poStmtIO->Execute();

        if( poStmtIO->Fetch( nBlockCount ) == false )
        {
            return false;
        }
    }

    if ( nCurrentXOffset != nXOffset ||
         nCurrentYOffset != nYOffset )
    {
        nCurrentXOffset = nXOffset;
        nCurrentYOffset = nYOffset;

        if( ! poStmtIO->ReadBlob( pahLocator[nBlock],
                                  pabyBlockBuf, nBlockBytes ) )
        {
            return false;
        }
    }

    GByte *pabyOutBuf = (GByte *) pData;

    //  --------------------------------------------------------------------
    //  Packing bits 
    //  --------------------------------------------------------------------

    if( EQUAL( pszCellDepth, "1BIT" ) ||
        EQUAL( pszCellDepth, "2BIT" ) ||
        EQUAL( pszCellDepth, "4BIT" ) )
    {
        int nPixCount =  nColumnBlockSize * nRowBlockSize;
        pabyOutBuf = (GByte *) VSIMalloc(nColumnBlockSize * nRowBlockSize);

        if (pabyOutBuf == NULL)
        {
            return false;
        }

        if( EQUAL( pszCellDepth, "1BIT" ) )
        {
            for( int ii = 0; ii < nPixCount - 7; ii += 8 )
            {
                int k = ii>>3;
                pabyOutBuf[k] = 
                    (((GByte *) pData)[ii] & 0x1)
                    | ((((GByte *) pData)[ii+1]&0x1) << 1)
                    | ((((GByte *) pData)[ii+2]&0x1) << 2)
                    | ((((GByte *) pData)[ii+3]&0x1) << 3)
                    | ((((GByte *) pData)[ii+4]&0x1) << 4)
                    | ((((GByte *) pData)[ii+5]&0x1) << 5)
                    | ((((GByte *) pData)[ii+6]&0x1) << 6)
                    | ((((GByte *) pData)[ii+7]&0x1) << 7);
            }
        }
        else if( EQUAL( pszCellDepth, "2BIT" ) )
        {
            for( int ii = 0; ii < nPixCount - 3; ii += 4 )
            {
                int k = ii>>2;
                pabyOutBuf[k] = 
                    (((GByte *) pData)[ii] & 0x3)
                    | ((((GByte *) pData)[ii+1]&0x3) << 2)
                    | ((((GByte *) pData)[ii+2]&0x3) << 4)
                    | ((((GByte *) pData)[ii+3]&0x3) << 6);
            }
        }
        else if( EQUAL( pszCellDepth, "4BIT" ) )
        {
            for( int ii = 0; ii < nPixCount - 1; ii += 2 )
            {
                int k = ii>>1;
                pabyOutBuf[k] = 
                    (((GByte *) pData)[ii] & 0xf) 
                    | ((((GByte *) pData)[ii+1]&0xf) << 4);
            }
        }
    }

    //  --------------------------------------------------------------------
    //  Writing LOBs
    //  --------------------------------------------------------------------

    if( EQUAL( szInterleaving, "BSQ" ) )
    {
        int nStart  = ( ( nBand - 1 ) % nRasterBands ) * nBlockBytesGDAL;
        memcpy( &pabyBlockBuf[nStart], pabyOutBuf, nBlockBytesGDAL );
    }
    else
    {
        int nStart  = ( ( nBand - 1 ) % nRasterBands ) * nCellSizeGDAL;
        int nIncr   = nBandBlockSize * nCellSizeGDAL;
        int nSize   = nCellSizeGDAL;

        if( EQUAL( szInterleaving, "BIL" ) )
        {
            nStart  *= nColumnBlockSize;
            nIncr   *= nColumnBlockSize;
            nSize   *= nColumnBlockSize;
        }

        int  ii = 0;
        int  jj = nStart;
        for( ii = 0; ii < nBlockBytesGDAL; 
             ii += ( nCellSizeGDAL * nSize ), 
             jj += nIncr )
        {
            memcpy( &pabyBlockBuf[jj], &pabyOutBuf[ii], nSize );
        }
    }

    if( ! poStmtIO->WriteBlob( pahLocator[nBlock], 
                               pabyBlockBuf,
                               nBlockBytes ))
    {
        return false;
    }

    return true;
}

//  ---------------------------------------------------------------------------
//                                                                      Flush()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::Flush()
{
//TODO: update sdo_georaster.metadata
}
