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
    dfXCoefficient[0]   = 1;
    dfXCoefficient[1]   = 0;
    dfXCoefficient[2]   = 0;
    dfYCoefficient[0]   = 0;
    dfYCoefficient[1]   = 1;
    dfYCoefficient[2]   = 0;
    pszCellDepth        = NULL;
    pahLocator          = NULL;
    pabyBlockBuf        = NULL;
    nPyraLevel          = 0;
    bIsReferenced       = false;
    poStmtRead          = NULL;
    poStmtWrite         = NULL;
    nCurrentBlock       = -1;
    nCurrentBandBlock   = -1;
    szInterleaving[0]   = 'B';
    szInterleaving[1]   = 'S';
    szInterleaving[2]   = 'Q';
    szInterleaving[3]   = '\0';
    bIOInitialized      = false;
    bBlobInitialized    = false;
    bFlushMetadata      = false;
    nSRID               = -1;
}

//  ---------------------------------------------------------------------------
//                                                           GeoRasterDataset()
//  ---------------------------------------------------------------------------

GeoRasterWrapper::~GeoRasterWrapper()
{
    FlushMetadata();

    CPLFree_nt( pszTable );
    CPLFree_nt( pszColumn );
    CPLFree_nt( pszDataTable );
    CPLFree_nt( pszWhere );
    XMLFree_nt( phMetadata );
    CPLFree_nt( pszCellDepth );
    OWStatement::Free( pahLocator, nBlockCount );
    CPLFree_nt( pabyBlockBuf );
    CPLFree_nt( poStmtRead );
    CPLFree_nt( poStmtWrite );
}

//  ---------------------------------------------------------------------------
//                                                                       Open()
//  ---------------------------------------------------------------------------

GeoRasterWrapper* GeoRasterWrapper::Open( const char* pszStringID )
{
    //  -------------------------------------------------------------------
    //  Parse arguments
    //  -------------------------------------------------------------------

    char **papszParam = CSLTokenizeString2(
                            strstr( pszStringID, ":" ) + 1, ID_SEPARATORS,
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
        CSLDestroy( papszParam );
        delete poGRW;
        return NULL;
    }

    //  -------------------------------------------------------------------
    //  Assign parameters from Identification string
    //  -------------------------------------------------------------------

    switch( nArgc ) 
    {
    case 6:
        poGRW->pszTable       = CPLStrdup( papszParam[3] );
        poGRW->pszColumn      = CPLStrdup( papszParam[4] );
        poGRW->pszWhere       = CPLStrdup( papszParam[5] );
        break;
    case 5 :
        if( OWIsNumeric( papszParam[4] ) )
        {
            poGRW->pszDataTable   = CPLStrdup( papszParam[3] );
            poGRW->nRasterId      = atoi( papszParam[4]);
            break;
        }
        else
        {
            poGRW->pszTable   = CPLStrdup( papszParam[3] );
            poGRW->pszColumn  = CPLStrdup( papszParam[4] );
            return poGRW;
        }
    case 4 :
        poGRW->pszTable       = CPLStrdup( papszParam[3] );
        return poGRW;
    default :
        return poGRW;
    }

    CSLDestroy( papszParam );

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
    //  Fetch Metadata, RDT, RID
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
        CPLDebug("GEORASTER","GeoRaster not unique (\"%s\")",poGRW->pszWhere); 
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
        CPLDebug( "GEORASTER", "Error reading Metadata!" );
    }

    //  -------------------------------------------------------------------
    //  Get basic information from text metadata
    //  -------------------------------------------------------------------

    poGRW->phMetadata = CPLParseXMLString( pszXML );

    poGRW->GetRasterInfo();

    //  -------------------------------------------------------------------
    //  Clean up and leave
    //  -------------------------------------------------------------------

    OCIDescriptorFree( phLocator, OCI_DTYPE_LOB );
    CPLFree( pszXML );

    delete poStmt;

    return poGRW;
}

//  ---------------------------------------------------------------------------
//                                                                 Initialize()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::Create( char* pszDescription, char* pszInsert )
{
    //  -------------------------------------------------------------------
    //  New table description
    //  -------------------------------------------------------------------

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
    //  Insert parameters
    //  -------------------------------------------------------------------

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
        strcpy( szRDT, CPLSPrintf( "'%s'", pszDataTable ) );
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

    if( poConnection->GetVersion() < 11 )
    {
        if( nRasterBands == 1 )
        {
            strcpy( szInsert, OWReplaceToken( szValues, '*', CPLSPrintf(
                "SDO_GEOR.createBlank(20001, \n"
                "  SDO_NUMBER_ARRAY(0, 0), \n"
                "  SDO_NUMBER_ARRAY(%d, %d), 0, %s, %s)",
                nRasterRows, nRasterColumns, szRDT, szRID ) ) );
        }
        else
        {
            strcpy( szInsert, OWReplaceToken( szValues, '*', CPLSPrintf(
                "SDO_GEOR.createBlank(21001, \n"
                "  SDO_NUMBER_ARRAY(0, 0, 0), \n"
                "  SDO_NUMBER_ARRAY(%d, %d, %d), 0, %s, %s)", 
                nRasterRows, nRasterColumns, nRasterBands, szRDT, szRID ) ) );
        }
    }
    else
    {
        strcpy( szInsert, OWReplaceToken( szValues, '*', CPLSPrintf(
            "SDO_GEOR.init(%s, %s)", szRDT, szRID ) ) );
    }

    //  -----------------------------------------------------------
    //  Storage parameters
    //  -----------------------------------------------------------

    nColumnBlockSize = nColumnBlockSize == 0 ? 255 : nColumnBlockSize;
    nRowBlockSize    = nRowBlockSize    == 0 ? 255 : nRowBlockSize;
    nBandBlockSize   = nBandBlockSize   == 0 ? 
        ( nRasterBands == 3 ? 1 : nRasterBands ) : nBandBlockSize;

    char szFormat[OWTEXT];

    if( poConnection->GetVersion() < 11 )
    {
        if( nRasterBands == 1 )
        {
            strcpy( szFormat, CPLSPrintf( 
                "blockSize=(%d, %d) "
                "cellDepth=%s "
                "interleaving=%s "
                "pyramid=FALSE",
                nColumnBlockSize, nRowBlockSize,
                pszCellDepth, 
                szInterleaving ) );
        }
        else
        {
            strcpy( szFormat, CPLSPrintf(
                "blockSize=(%d, %d, %d) "
                "cellDepth=%s "
                "interleaving=%s "
                "pyramid=FALSE",
                nColumnBlockSize, nRowBlockSize, nBandBlockSize,
                pszCellDepth, 
                szInterleaving ) );
        }
    }
    else
    {
        if( nRasterBands == 1 )
        {
            strcpy( szFormat, CPLSPrintf(
                "20001, '"
                "dimSize=(%d,%d) "
                "blockSize=(%d,%d) "
                "cellDepth=%s "
                "interleaving=%s "
                "'",
                nRasterRows, nRasterColumns, 
                nColumnBlockSize, nRowBlockSize,
                pszCellDepth, 
                szInterleaving ) );
        }
        else
        {
            strcpy( szFormat, CPLSPrintf(
                "21001, '"
                "dimSize=(%d,%d,%d) "
                "blockSize=(%d,%d,%d) "
                "cellDepth=%s "
                "interleaving=%s "
                "'",
                nRasterRows, nRasterColumns, nRasterBands, 
                nColumnBlockSize, nRowBlockSize, nBandBlockSize,
                pszCellDepth, 
                szInterleaving ) );
        }
    }

    nTotalColumnBlocks  = (int) ( nRasterColumns / nColumnBlockSize );
    nTotalRowBlocks     = (int) ( nRasterRows    / nRowBlockSize );
    nTotalBandBlocks    = (int) ( nRasterBands   / nBandBlockSize );

    nTotalColumnBlocks += ( nRasterColumns % nColumnBlockSize ) == 0 ? 0 : 1;
    nTotalRowBlocks    += ( nRasterRows    % nRowBlockSize )    == 0 ? 0 : 1;
    nTotalBandBlocks   += ( nRasterBands   % nBandBlockSize )   == 0 ? 0 : 1;

    //  -------------------------------------------------------------------
    //  Create Georaster Table if needed
    //  -------------------------------------------------------------------

    OWStatement* poStmt;

    char szUser[OWNAME];

    strcpy( szUser, poConnection->GetUser() );

    poStmt = poConnection->CreateStatement( CPLSPrintf(
        "DECLARE\n"
        "  TAB VARCHAR2(68)  := UPPER(:1);\n"
        "  COL VARCHAR2(68)  := UPPER(:2);\n"
        "  USR VARCHAR2(68)  := UPPER(:3);\n"
        "  CNT NUMBER        := 0;\n"
        "BEGIN\n"
        "  EXECUTE IMMEDIATE 'SELECT COUNT(*) FROM ALL_TABLES\n"
        "    WHERE TABLE_NAME = :1 AND OWNER = :2 ' INTO CNT USING TAB, USR;\n"
        "  IF CNT = 0 THEN\n"
        "    EXECUTE IMMEDIATE 'CREATE TABLE '||TAB||' %s';\n"
        "    SDO_GEOR_UTL.createDMLTrigger( TAB,  COL );\n"
        "  END IF;\n"
        "END;", szDescription ) );

    poStmt->Bind( pszTable );
    poStmt->Bind( pszColumn );
    poStmt->Bind( szUser );

    if( ! poStmt->Execute() )
    {
        delete ( poStmt );
        CPLError( CE_Failure, CPLE_AppDefined, "Create Table Error!" );
        return false;
    }

    delete poStmt;

    //  -----------------------------------------------------------
    //  Create RTD if needed and insert GeoRaster
    //  -----------------------------------------------------------

    char szBindRDT[OWNAME] = "";
    int  nBindRID = 0;

    if( ! ( poConnection->GetVersion() < 11 ) )
    {
        poStmt = poConnection->CreateStatement( CPLSPrintf(
            "DECLARE\n"
            "  TAB  VARCHAR2(68)    := UPPER(:1);\n"
            "  COL  VARCHAR2(68)    := UPPER(:2);\n"
            "  USR  VARCHAR2(68)    := UPPER(:3);\n"
            "  CNT  NUMBER          := 0;\n"
            "  GR1  SDO_GEORASTER   := NULL;\n"
            "BEGIN\n"
            "  INSERT INTO %s %s\n"
            "    RETURNING %s INTO GR1;\n"
            "  SELECT GR1.RASTERDATATABLE INTO :rdt FROM DUAL;\n"
            "  SELECT GR1.RASTERID        INTO :rid FROM DUAL;\n"
            "\n"
            "  EXECUTE IMMEDIATE 'SELECT COUNT(*) FROM ALL_TABLES WHERE \n"
            "    TABLE_NAME = :1 AND OWNER = :2' INTO CNT USING :rdt, USR;\n"
            "  IF CNT = 0 THEN\n"
            "    EXECUTE IMMEDIATE 'CREATE TABLE '||:rdt||' OF MDSYS.SDO_RASTER\n"
            "      (PRIMARY KEY (RASTERID, PYRAMIDLEVEL, BANDBLOCKNUMBER,\n"
            "      ROWBLOCKNUMBER, COLUMNBLOCKNUMBER))\n"
            "      LOB(RASTERBLOCK) STORE AS (NOCACHE NOLOGGING)';\n"
            "  END IF;\n"
            "\n"
            "  SDO_GEOR.createTemplate(GR1, %s, null, 'TRUE');\n"
            "  UPDATE %s T SET %s = GR1     WHERE"
            "    T.%s.RasterDataTable = :rdt AND"
            "    T.%s.RasterId = :rid;\n"
            "END;\n",
            pszTable, szInsert, pszColumn, szFormat,
            pszTable, pszColumn, pszColumn, pszColumn  ) );

        poStmt->Bind( pszTable );
        poStmt->Bind( pszColumn );
        poStmt->Bind( szUser );
        poStmt->BindName( ":rdt", szBindRDT );
        poStmt->BindName( ":rid", &nBindRID );

        if( ! poStmt->Execute() )
        {
            delete poStmt;
            CPLError( CE_Failure, CPLE_AppDefined, 
                "Failure to initialize GeoRaster" );
            return false;
        }
        CPLFree_nt( pszDataTable );
        pszDataTable = CPLStrdup( szBindRDT );
        nRasterId    = nBindRID;

        delete poStmt;

        return true;
    }

    //  -----------------------------------------------------------
    //  Procedure for Server version older than 11
    //  -----------------------------------------------------------

    poStmt = poConnection->CreateStatement( CPLSPrintf(
        "DECLARE\n"
        "  TAB  VARCHAR2(68)    := UPPER(:1);\n"
        "  COL  VARCHAR2(68)    := UPPER(:2);\n"
        "  USR  VARCHAR2(68)    := UPPER(:3);\n"
        "  W    NUMBER          := :4;\n"
        "  H    NUMBER          := :5;\n"
        "  BB   NUMBER          := :6;\n"
        "  RB   NUMBER          := :7;\n"
        "  CB   NUMBER          := :8;\n"
        "  CNT  NUMBER          := 0;\n"
        "  X    NUMBER          := 0;\n"
        "  Y    NUMBER          := 0;\n"
        "  GR1  SDO_GEORASTER   := NULL;\n"
        "  GR2  SDO_GEORASTER   := NULL;\n"
        "  STM  VARCHAR2(1024)  := '';\n"
        "BEGIN\n"
        "  INSERT INTO %s %s\n"
        "    RETURNING %s INTO GR1;\n"
        "  SELECT GR1.RASTERDATATABLE INTO :rdt FROM DUAL;\n"
        "  SELECT GR1.RASTERID        INTO :rid FROM DUAL;\n"
        "\n"
        "  SELECT %s INTO GR2 FROM %s T WHERE"
        "    T.%s.RasterDataTable = :rdt AND"
        "    T.%s.RasterId = :rid FOR UPDATE;\n"
        "  SELECT %s INTO GR1 FROM %s T WHERE"
        "    T.%s.RasterDataTable = :rdt AND"
        "    T.%s.RasterId = :rid;\n"
        "  SDO_GEOR.changeFormatCopy(GR1, '%s', GR2);\n"
        "  UPDATE %s T SET %s = GR2     WHERE"
        "    T.%s.RasterDataTable = :rdt AND"
        "    T.%s.RasterId = :rid;\n"
        "\n"
        "  EXECUTE IMMEDIATE 'SELECT COUNT(*) FROM ALL_TABLES WHERE \n"
        "    TABLE_NAME = :1 AND OWNER = :2' INTO CNT USING :rdt, USR;\n"
        "  IF CNT = 0 THEN\n"
        "    EXECUTE IMMEDIATE 'CREATE TABLE '||:rdt||' OF MDSYS.SDO_RASTER\n"
        "      (PRIMARY KEY (RASTERID, PYRAMIDLEVEL, BANDBLOCKNUMBER,\n"
        "      ROWBLOCKNUMBER, COLUMNBLOCKNUMBER))\n"
        "      LOB(RASTERBLOCK) STORE AS (NOCACHE NOLOGGING)';\n"
        "  END IF;\n"
        "\n"
        "  STM := 'INSERT INTO '||:rdt||' VALUES (:1,0,:2-1,:3-1,:4-1,\n"
        "    SDO_GEOMETRY(2003, NULL, NULL, SDO_ELEM_INFO_ARRAY(1, 1003, 3),\n"
        "    SDO_ORDINATE_ARRAY(:5,:6,:7-1,:8-1)), EMPTY_BLOB() )';\n\n"
        "  FOR b IN 1..BB LOOP\n"
        "    Y := 0;\n"
        "    FOR r IN 1..RB LOOP\n"
        "      X := 0;\n"
        "      FOR c IN 1..CB LOOP\n"
        "        EXECUTE IMMEDIATE STM USING :rid, b, r, c, Y, X, (Y+H), (X+W);\n"
        "        X := X + W;\n"
        "      END LOOP;\n"
        "      Y := Y + H;\n"
        "    END LOOP;\n"
        "  END LOOP;\n"
        "END;",
        pszTable,
        szInsert,
        pszColumn,
        pszColumn, pszTable, pszColumn, pszColumn,
        pszColumn, pszTable, pszColumn, pszColumn, szFormat,
        pszTable, pszColumn, pszColumn, pszColumn  ) );

    poStmt->Bind( pszTable );
    poStmt->Bind( pszColumn );
    poStmt->Bind( szUser );
    poStmt->Bind( &nColumnBlockSize );
    poStmt->Bind( &nRowBlockSize );
    poStmt->Bind( &nTotalBandBlocks );
    poStmt->Bind( &nTotalRowBlocks );
    poStmt->Bind( &nTotalColumnBlocks );
    poStmt->BindName( ":rdt", szBindRDT );
    poStmt->BindName( ":rid", &nBindRID );

    if( ! poStmt->Execute() )
    {
        delete poStmt;
        CPLError( CE_Failure, CPLE_AppDefined, 
            "Failure to initialize GeoRaster" );
        return false;
    }

    CPLFree_nt( pszDataTable );

    pszDataTable = CPLStrdup( szBindRDT );
    nRasterId    = nBindRID;

    delete poStmt;

    return true;
}

//  ---------------------------------------------------------------------------
//                                                            SetGeoReference()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::SetGeoReference( int nSRIDIn )
{
    if( nSRIDIn == 0 )
    {
        return false;
    }

    nSRID = nSRIDIn;

    bFlushMetadata = true;

    return true;
}

//  ---------------------------------------------------------------------------
//                                                                  GetWKText()
//  ---------------------------------------------------------------------------

char* GeoRasterWrapper::GetWKText( int nSRIDin )
{
    char szWKText[OWTEXT];

    OWStatement* poStmt = poConnection->CreateStatement(
        "SELECT WKTEXT\n"
        "FROM   MDSYS.CS_SRS\n"
        "WHERE  SRID = :1 AND WKTEXT IS NOT NULL" );

    poStmt->Bind( &nSRIDin );
    poStmt->Define( szWKText,  OWTEXT );

    if( poStmt->Execute() == false ||
        poStmt->Fetch()   == false )
    {
        delete poStmt;
        return NULL;
    }

    delete poStmt;

    return CPLStrdup( szWKText );
}

//  ---------------------------------------------------------------------------
//                                                              GetRasterInfo()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::GetRasterInfo( void )
{
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
        CPLDebug("GEORASTER","There is no BandBlockSize, "
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

    GDALDataType eType  = OWGetDataType( pszCellDepth );

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
        delete poStmt;
        return false;
    }

    double dfULx = poStmt->GetDouble( &poUpperLeft->sdo_point.x );
    double dfURx = poStmt->GetDouble( &poUpperRight->sdo_point.x );
    double dfLRx = poStmt->GetDouble( &poLowerRight->sdo_point.x );

    double dfULy = poStmt->GetDouble( &poUpperLeft->sdo_point.y );
    double dfLLy = poStmt->GetDouble( &poLowerLeft->sdo_point.y );
    double dfLRy = poStmt->GetDouble( &poLowerRight->sdo_point.y );

    delete poStmt;

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

bool GeoRasterWrapper::GetStatistics( int nBand,
                                      double dfMin,
                                      double dfMax,
                                      double dfMean,
                                      double dfStdDev )
{
    int n = 1;

    CPLXMLNode *phSubLayer = CPLGetXMLNode( phMetadata, "layerInfo.subLayer" );

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
//                                                              SetStatistics()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::SetStatistics( double dfMin,
                                      double dfMax,
                                      double dfMean,
                                      double dfStdDev,
                                      int nBand )
{
    InitializeLayersNode();

    bFlushMetadata = true;

    int n = 1;

    CPLXMLNode* phSubLayer = CPLGetXMLNode( phMetadata, "layerInfo.subLayer" );

    for( n = 1 ; phSubLayer ; phSubLayer = phSubLayer->psNext, n++ )
    {
        if( n != nBand )
        {
            continue;
        }

        CPLXMLNode* psSDaset = CPLGetXMLNode( phSubLayer, "statisticDataset" );

        if( psSDaset != NULL )
        {
            CPLRemoveXMLChild( phSubLayer, psSDaset );
            CPLDestroyXMLNode( psSDaset );
        }

        psSDaset = CPLCreateXMLNode(phSubLayer,CXT_Element,"statisticDataset");

        CPLCreateXMLElementAndValue(psSDaset,"MIM", CPLSPrintf("%f",dfMin));
        CPLCreateXMLElementAndValue(psSDaset,"MAX", CPLSPrintf("%f",dfMax));
        CPLCreateXMLElementAndValue(psSDaset,"MEAN",CPLSPrintf("%f",dfMean));
        CPLCreateXMLElementAndValue(psSDaset,"STD", CPLSPrintf("%f",dfStdDev));

        return true;
    }
    return false;
}

//  ---------------------------------------------------------------------------
//                                                              HasColorTable()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::HasColorMap( int nBand )
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
//                                                        InitializeLayersNode()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::InitializeLayersNode()
{
    CPLXMLNode *pslInfo  = CPLGetXMLNode( phMetadata, "layerInfo" );
    
    int n = 1;

    for( n = 0 ; n < nRasterBands; n++ )
    {
        CPLXMLNode *psSLayer = CPLGetXMLNode( pslInfo,    "subLayer" );

        if( psSLayer == NULL )
        {
            psSLayer = CPLCreateXMLNode( pslInfo, CXT_Element, "subLayer" );

            CPLCreateXMLElementAndValue( psSLayer, "layerNumber", 
                CPLSPrintf( "%d", n + 1 ) );

            CPLCreateXMLElementAndValue( psSLayer, "layerDimensionOrdinate",
                CPLSPrintf( "%d", n ) );

            CPLCreateXMLElementAndValue( psSLayer, "layerID",
                CPLSPrintf( "subLayer%d", n + 1 ) );
        }
    }
}

//  ---------------------------------------------------------------------------
//                                                              GetColorTable()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::GetColorMap( int nBand, GDALColorTable* poCT )
{
    GDALColorEntry oEntry;

    CPLXMLNode* psLayers;

    int n = 1;

    psLayers = CPLGetXMLNode( phMetadata, "layerInfo.subLayer" );

    for( ; psLayers; psLayers = psLayers->psNext, n++ )
    {
        if( n != nBand )
        {
            continue;
        }

        CPLXMLNode* psColors = CPLGetXMLNode( psLayers, "colorMap.colors.cell" );

        int iColor = 0;

        for(  ; psColors; psColors = psColors->psNext )
        {
            iColor    = (short) atoi( CPLGetXMLValue( psColors, "value","0"));
            oEntry.c1 = (short) atoi( CPLGetXMLValue( psColors, "red",  "0"));
            oEntry.c2 = (short) atoi( CPLGetXMLValue( psColors, "green","0"));
            oEntry.c3 = (short) atoi( CPLGetXMLValue( psColors, "blue", "0"));
            oEntry.c4 = (short) atoi( CPLGetXMLValue( psColors, "alpha","0"));
            poCT->SetColorEntry( iColor, &oEntry );
        }
        break;
    }
}

//  ---------------------------------------------------------------------------
//                                                              SetColorTable()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::SetColorMap( int nBand, GDALColorTable* poCT )
{
    InitializeLayersNode();

    bFlushMetadata = true;

    GDALColorEntry oEntry;

    int n = 1;

    CPLXMLNode* phSubLayer = CPLGetXMLNode( phMetadata, "layerInfo.subLayer" );

    for( n = 1 ; phSubLayer ; phSubLayer = phSubLayer->psNext, n++ )
    {
        if( n != nBand )
        {
            continue;
        }

        CPLXMLNode* psCMap = CPLGetXMLNode( phSubLayer, "colorMap" );

        if( psCMap != NULL )
        {
            CPLRemoveXMLChild( phSubLayer, psCMap );
            CPLDestroyXMLNode( psCMap );
        }

        psCMap = CPLCreateXMLNode( phSubLayer, CXT_Element, "colorMap" );

        CPLXMLNode* psColor = CPLCreateXMLNode( psCMap, CXT_Element, "colors" );

        // ------------------------------------------------
        // Clean existing colors entry (RGB color table)
        // ------------------------------------------------

        if( psColor != NULL )
        {
            CPLRemoveXMLChild( psCMap, psColor );
            CPLDestroyXMLNode( psColor );
        }

        psColor = CPLCreateXMLNode( psCMap, CXT_Element, "colors" );

        int iColor = 0;

        for( iColor = 0; iColor < poCT->GetColorEntryCount(); iColor++ )
        {
            poCT->GetColorEntryAsRGB( iColor, &oEntry );

            CPLXMLNode* psCell = CPLCreateXMLNode( psColor, CXT_Element, "cell" );

            CPLSetXMLValue( psCell, "#value", CPLSPrintf("%d", iColor) );
            CPLSetXMLValue( psCell, "#blue",  CPLSPrintf("%d", oEntry.c3) );
            CPLSetXMLValue( psCell, "#red",   CPLSPrintf("%d", oEntry.c1) );
            CPLSetXMLValue( psCell, "#green", CPLSPrintf("%d", oEntry.c2) );
            CPLSetXMLValue( psCell, "#alpha", CPLSPrintf("%d", oEntry.c4) );
        }

        break;
    }
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
        nCurrentBlock     = -1;

        CPLFree_nt( poStmtRead );

        poStmtRead = poConnection->CreateStatement( CPLSPrintf(
            "SELECT RASTERBLOCK\n"
            "FROM   %s\n"
            "WHERE  RASTERID = :1 AND\n"
            "       BANDBLOCKNUMBER = :2 AND\n"
            "       PYRAMIDLEVEL = :3\n"
            "ORDER BY\n"
            "       ROWBLOCKNUMBER ASC,\n"
            "       COLUMNBLOCKNUMBER ASC", 
            pszDataTable ) );

        poStmtRead->Bind( &nRasterId );
        poStmtRead->Bind( &nBandBlock );
        poStmtRead->Bind( &nPyraLevel );

        poStmtRead->Define( pahLocator, nBlockCount );

        poStmtRead->Execute();

        if( poStmtRead->Fetch( nBlockCount ) == false )
        {
            return false;
        }
    }

    if( nCurrentBlock != nBlock )
    {
        nCurrentBlock = nBlock;

        if( ! poStmtRead->ReadBlob( pahLocator[nBlock], 
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
    }

    if ( nCurrentBandBlock != nBandBlock ) //TODO: Optimize for multi-band
    {
        nCurrentBandBlock = nBandBlock;

        CPLFree_nt( poStmtWrite );

        poStmtWrite = poConnection->CreateStatement( CPLSPrintf(
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

        poStmtWrite->Bind( &nRasterId );
        poStmtWrite->Bind( &nBandBlock );
        poStmtWrite->Bind( &nPyraLevel );

        poStmtWrite->Define( pahLocator, nBlockCount );

        poStmtWrite->Execute();

        if( poStmtWrite->Fetch( nBlockCount ) == false )
        {
            return false;
        }
    }

    if( nBandBlockSize > 1 && nCurrentBlock != nBlock )
    {
        nCurrentBlock = nBlock;
        poStmtWrite->ReadBlob( pahLocator[nBlock], pabyBlockBuf, nBlockBytes );
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

    if( ! poStmtWrite->WriteBlob( pahLocator[nBlock], 
                                  pabyBlockBuf,
                                  nBlockBytes ))
    {
        return false;
    }

    return true;
}

//  ---------------------------------------------------------------------------
//                                                                  GetNoData()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::GetNoData( double* pdfNoDataValue )
{
    if( EQUAL( CPLGetXMLValue( phMetadata,
        "rasterInfo.NODATA", "NONE" ), "NONE" ) )
    {
        return false;
    }

    *pdfNoDataValue = atof( CPLGetXMLValue( phMetadata,
        "rasterInfo.NODATA", "0.0" ) );

    return true;
}

//  ---------------------------------------------------------------------------
//                                                             SetNoDataValue()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::SetNoData( double dfNoDataValue )
{
    CPLXMLNode* psRInfo = CPLGetXMLNode( phMetadata, "rasterInfo" );
    CPLXMLNode* psNData = CPLGetXMLNode( psRInfo, "NODATA");

    if( psNData )
    {
        CPLRemoveXMLChild( psRInfo, psNData );
        CPLDestroyXMLNode( psNData );
    }

    psNData = CPLCreateXMLElementAndValue( psRInfo, "NODATA",
        CPLSPrintf( "%f", dfNoDataValue ) );

    if( psNData )
    {
        bFlushMetadata = true;

        return true;
    }

    return false;
}

//  ---------------------------------------------------------------------------
//                                                                     SetVAT()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::SetVAT( int nBand, const char* pszName )
{
    InitializeLayersNode();

    bFlushMetadata = true;

    int n = 1;

    CPLXMLNode* psLayers = CPLGetXMLNode( phMetadata, "layerInfo.subLayer" );

    for( ; psLayers; psLayers = psLayers->psNext, n++ )
    {
        if( n != nBand )
        {
            continue;
        }

        CPLXMLNode* psVAT = CPLGetXMLNode( psLayers, "vatTableName" );

        if( psVAT != NULL )
        {
            CPLRemoveXMLChild( psLayers, psVAT );
            CPLDestroyXMLNode( psVAT );
        }

        CPLCreateXMLElementAndValue(psLayers, "vatTableName", pszName );

        return true;
    }

    return false;
}

//  ---------------------------------------------------------------------------
//                                                                     GetVAT()
//  ---------------------------------------------------------------------------

char* GeoRasterWrapper::GetVAT( int nBand )
{
    CPLXMLNode* psLayers = CPLGetXMLNode( phMetadata, "layerInfo.subLayer" );

    if( psLayers == NULL )
    {
        return NULL;
    }

    char* pszTablename = NULL;

    int n = 1;

    for( ; psLayers; psLayers = psLayers->psNext, n++ )
    {
        if( n != nBand )
        {
            continue;
        }

        CPLXMLNode* psVAT = CPLGetXMLNode( psLayers, "vatTableName" );

        if( psVAT != NULL )
        {
            pszTablename = CPLStrdup(
                CPLGetXMLValue( psLayers, "vatTableName", "" ) );
        }

        break;
    }

    return pszTablename;
}

//  ---------------------------------------------------------------------------
//                                                              FlushMetadata()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::FlushMetadata()
{
    if( ! bFlushMetadata )
    {
        return true;
    }

    bFlushMetadata = false;

    CPLDebug("**************************", "FLushMetadata" );

    //  --------------------------------------------------------------------
    //  Change the isBlank setting left by SDO_GEOR.createBlank() to 'false'
    //  --------------------------------------------------------------------

    CPLXMLNode* psOInfo = CPLGetXMLNode( phMetadata, "objectInfo" );
    CPLXMLNode* psNode  = NULL;

    CPLSetXMLValue( psOInfo,  "isBlank", "false" );

    psNode  = CPLGetXMLNode( psOInfo, "blankCellValue" );

    if( psNode != NULL )
    {
        CPLRemoveXMLChild( psOInfo, psNode );
        CPLDestroyXMLNode( psNode );
    }

    const char* pszRed   = "1";
    const char* pszGreen = "1";
    const char* pszBlue  = "1";

    if( ( nRasterBands == 3 ) && ( EQUAL( pszCellDepth, "8BIT_U") ) &&
        ( ! HasColorMap( 1 ) ) && 
        ( ! HasColorMap( 2 ) ) && 
        ( ! HasColorMap( 3 ) ) )
    {
        pszRed   = "1";
        pszGreen = "2";
        pszBlue  = "3";
    }

    psNode = CPLGetXMLNode( psOInfo, "defaultRed" );
    if( psNode )
    {
        CPLRemoveXMLChild( psOInfo, psNode );
        CPLDestroyXMLNode( psNode );
    }
    CPLCreateXMLElementAndValue( psOInfo, "defaultRed",   pszRed );

    psNode = CPLGetXMLNode( psOInfo, "defaultGreen" );
    if( psNode )
    {
        CPLRemoveXMLChild( psOInfo, psNode );
        CPLDestroyXMLNode( psNode );
    }
    CPLCreateXMLElementAndValue( psOInfo, "defaultGreen",   pszGreen );

    psNode = CPLGetXMLNode( psOInfo, "defaultBlue" );
    if( psNode )
    {
        CPLRemoveXMLChild( psOInfo, psNode );
        CPLDestroyXMLNode( psNode );
    }
    CPLCreateXMLElementAndValue( psOInfo, "defaultBlue",   pszBlue );

    //  --------------------------------------------------------------------
    //  Update the Metadata directly from the XML text
    //  --------------------------------------------------------------------

    char* pszXML = CPLSerializeXMLTree( phMetadata );

    OWStatement* poStmt = poConnection->CreateStatement( CPLSPrintf(
        "DECLARE\n"
        "  GR   sdo_georaster;\n"
        "BEGIN\n"
        "  SELECT %s INTO GR FROM %s T WHERE %s FOR UPDATE;\n"
        "  GR.metadata := XMLTYPE(:1);\n"
        "  IF NOT :2 = 0 THEN\n"
        "  SDO_GEOR.georeference( GR, :2, 1,\n"
        "    SDO_NUMBER_ARRAY(:3, :4, :5), SDO_NUMBER_ARRAY(:6, :7, :8));\n"
        "  END IF;\n"
        "  UPDATE %s T SET %s = GR WHERE %s;\n"
        "  COMMIT;\n"
        "END;", 
        pszColumn, pszTable, pszWhere,
        pszTable, pszColumn, pszWhere ) );

    poStmt->Bind( pszXML, strlen( pszXML ) + 1);
    poStmt->Bind( &nSRID );
    poStmt->Bind( &dfXCoefficient[0] );
    poStmt->Bind( &dfXCoefficient[1] );
    poStmt->Bind( &dfXCoefficient[2] );
    poStmt->Bind( &dfYCoefficient[0] );
    poStmt->Bind( &dfYCoefficient[1] );
    poStmt->Bind( &dfYCoefficient[2] );

    if( ! poStmt->Execute() )
    {
        CPLDebug( "XML-Error:", "%s", pszXML );
        CPLFree_nt( pszXML );
        ObjFree_nt( poStmt );
        return false;
    }

    CPLFree_nt( pszXML );

    delete poStmt;

    return true;
}
