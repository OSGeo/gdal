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
    pszSchema           = NULL;
    pszOwner            = NULL;
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
    nCellSizeBits       = 0;
    nGDALCellBytes      = 0;
    dfXCoefficient[0]   = 1.0;
    dfXCoefficient[1]   = 0.0;
    dfXCoefficient[2]   = 0.0;
    dfYCoefficient[0]   = 0.0;
    dfYCoefficient[1]   = 1.0;
    dfYCoefficient[2]   = 0.0;
    pszCellDepth        = NULL;
    pszCompressionType  = CPLStrdup( "NONE" );
    nCompressQuality    = 75;
    pahLocator          = NULL;
    pabyBlockBuf        = NULL;
    pabyBlockBuf2       = NULL;
    bIsReferenced       = false;
    poStmtRead          = NULL;
    poStmtWrite         = NULL;
    nCurrentBlock       = -1;
    nCurrentLevel       = -1;
    szInterleaving[0]   = 'B';
    szInterleaving[1]   = 'S';
    szInterleaving[2]   = 'Q';
    szInterleaving[3]   = '\0';
    bIOInitialized      = false;
    bFlushMetadata      = false;
    nSRID               = -1;
    bRDTRIDOnly         = false;
    nPyramidMaxLevel    = 0;
    nBlockCount         = 0;
    bOrderlyAccess      = false;
    nGDALBlockBytes     = 0;
    sDInfo.global_state = 0;
    sCInfo.global_state = 0;
    bHasBitmapMask      = false;
    eModelCoordLocation = MCL_DEFAULT;
    eForceCoordLocation = MCL_DEFAULT;
}

//  ---------------------------------------------------------------------------
//                                                           GeoRasterDataset()
//  ---------------------------------------------------------------------------

GeoRasterWrapper::~GeoRasterWrapper()
{
    FlushMetadata();

    CPLFree( pszTable );
    CPLFree( pszSchema );
    CPLFree( pszOwner );
    CPLFree( pszColumn );
    CPLFree( pszDataTable );
    CPLFree( pszWhere );
    CPLFree( pszCellDepth );
    CPLFree( pszCompressionType );
    CPLFree( pabyBlockBuf );
    CPLFree( pabyBlockBuf2 );
    delete poStmtRead;
    delete poStmtWrite;
    CPLDestroyXMLNode( phMetadata );
    OWStatement::Free( pahLocator, nBlockCount );
    CPLFree( pahLocator );

    if( sDInfo.global_state )
    {
        jpeg_destroy_decompress( &sDInfo );
    }

    if( sCInfo.global_state )
    {
        jpeg_destroy_compress( &sCInfo );
    }
}

//  ---------------------------------------------------------------------------
//                                                         ParseIdentificator()
//  ---------------------------------------------------------------------------
//
//  StringID:
//      {georaster,geor}:<name>{/,,}<password>{/,@}<db>,<tab>,<col>,<where>
//      {georaster,geor}:<name>{/,,}<password>{/,@}<db>,<rdt>,<rid>
//
//  ---------------------------------------------------------------------------

char** GeoRasterWrapper::ParseIdentificator( const char* pszStringID )
{

    char* pszStartPos = (char*) strstr( pszStringID, ":" ) + 1;

    char** papszParam = CSLTokenizeString2( pszStartPos, ",@",
                            CSLT_HONOURSTRINGS | CSLT_ALLOWEMPTYTOKENS |
                            CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES );

    //  -------------------------------------------------------------------
    //  The "/" should not be catch on the previous parser
    //  -------------------------------------------------------------------

    if( CSLCount( papszParam ) > 0 )
    {
        char** papszFirst2 = CSLTokenizeString2( papszParam[0], "/",
                             CSLT_HONOURSTRINGS | CSLT_ALLOWEMPTYTOKENS );
        if( CSLCount( papszFirst2 ) == 2 )
        {
            papszParam = CSLInsertStrings( papszParam, 0, papszFirst2 );
            papszParam = CSLRemoveStrings( papszParam, 2, 1, NULL );
        }
        CSLDestroy( papszFirst2 );
    }

    // --------------------------------------------------------------------
    // Assume a default database
    // --------------------------------------------------------------------

    if( CSLCount( papszParam ) == 2 )
    {
        papszParam = CSLAddString( papszParam, "" );
    }

    return papszParam;
}

//  ---------------------------------------------------------------------------
//                                                                       Open()
//  ---------------------------------------------------------------------------

GeoRasterWrapper* GeoRasterWrapper::Open( const char* pszStringId )
{
    char** papszParam = ParseIdentificator( pszStringId );

    int nArgc = CSLCount( papszParam );

    //  ---------------------------------------------------------------
    //  Create a GeoRasterWrapper object
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

    poGRW->poConnection = poDriver->GetConnection( papszParam[0],
                                                   papszParam[1],
                                                   papszParam[2] );

    if( ! poGRW->poConnection->Succeeded() )
    {
        CSLDestroy( papszParam );
        delete poGRW;
        return NULL;
    }

    //  -------------------------------------------------------------------
    //  Extract schema name
    //  -------------------------------------------------------------------

    if( nArgc > 3 )
    {
        char** papszSchema = CSLTokenizeString2( papszParam[3], ".",
                                CSLT_HONOURSTRINGS | CSLT_ALLOWEMPTYTOKENS );

        if( CSLCount( papszSchema ) == 2 )
        {
            poGRW->pszOwner  = CPLStrdup( papszSchema[0] );
            poGRW->pszSchema = CPLStrdup( CPLSPrintf( "%s.", poGRW->pszOwner ) );

            papszParam = CSLRemoveStrings( papszParam, 3, 1, NULL );

            if( ! EQUAL( papszSchema[1], "" ) )
            {
                papszParam = CSLInsertString( papszParam, 3, papszSchema[1] );
            }

            nArgc = CSLCount( papszParam );
        }
        else
        {
            poGRW->pszSchema = CPLStrdup( "" );
            poGRW->pszOwner  = CPLStrdup( poGRW->poConnection->GetUser() );
        }
        
        CSLDestroy( papszSchema );
    }
    else
    {
        poGRW->pszSchema = CPLStrdup( "" );
        poGRW->pszOwner  = CPLStrdup( poGRW->poConnection->GetUser() );
    }

    //  -------------------------------------------------------------------
    //  Assign parameters from Identification string
    //  -------------------------------------------------------------------

    switch( nArgc )
    {
    case 6 :
        poGRW->pszTable       = CPLStrdup( papszParam[3] );
        poGRW->pszColumn      = CPLStrdup( papszParam[4] );
        poGRW->pszWhere       = CPLStrdup( papszParam[5] );
        break;
    case 5 :
        if( OWIsNumeric( papszParam[4] ) )
        {
            poGRW->pszDataTable = CPLStrdup( papszParam[3] );
            poGRW->nRasterId    = atoi( papszParam[4]);
            poGRW->bRDTRIDOnly  = true;
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

    if( poGRW->bRDTRIDOnly )
    {
        OWStatement* poStmt = NULL;

        char  szOwner[OWNAME];
        char  szTable[OWNAME];
        char  szColumn[OWNAME];

        poStmt = poGRW->poConnection->CreateStatement(
            "SELECT OWNER, TABLE_NAME, COLUMN_NAME\n"
            "FROM   ALL_SDO_GEOR_SYSDATA\n"
            "WHERE  RDT_TABLE_NAME = UPPER(:1) AND RASTER_ID = :2 " );

        poStmt->Bind( poGRW->pszDataTable );
        poStmt->Bind( &poGRW->nRasterId );

        poStmt->Define( szOwner );
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
        //  Borrow the first Table/Column found as a reference
        //  ---------------------------------------------------------------

        CPLFree( poGRW->pszSchema );
        CPLFree( poGRW->pszOwner );
        CPLFree( poGRW->pszTable );
        CPLFree( poGRW->pszColumn );

        poGRW->pszSchema  = CPLStrdup( CPLSPrintf( "%s.", szOwner ) );
        poGRW->pszOwner   = CPLStrdup( szOwner );
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
        "FROM   %s%s T\n"
        "WHERE  %s",
        poGRW->pszColumn, poGRW->pszColumn, poGRW->pszColumn,
        poGRW->pszSchema, poGRW->pszTable,
        poGRW->pszWhere ) );

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
        delete poStmt;
        return poGRW;
    }

    //  -------------------------------------------------------------------
    //  Read Metadata XML in text form
    //  -------------------------------------------------------------------

    char* pszXML = NULL;

    pszXML = poStmt->ReadCLob( phLocator );

    if( pszXML )
    {
        //  -----------------------------------------------------------
        //  Get basic information from xml metadata
        //  -----------------------------------------------------------

        poGRW->phMetadata = CPLParseXMLString( pszXML );
        poGRW->GetRasterInfo();
    }
    else
    {
        CPLFree( poGRW->pszDataTable );
        poGRW->pszDataTable = NULL;
        poGRW->nRasterId    = 0;
    }

    //  -------------------------------------------------------------------
    //  Clean up and return a GeoRasterWrapper object
    //  -------------------------------------------------------------------

    OCIDescriptorFree( phLocator, OCI_DTYPE_LOB );
    CPLFree( pszXML );

    delete poStmt;

    return poGRW;
}

//  ---------------------------------------------------------------------------
//                                                                     Create()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::Create( char* pszDescription,
                               char* pszInsert,
                               bool bUpdate )
{
    char szValues[OWNAME];
    char szFormat[OWTEXT];

    if( pszTable  == NULL ||
        pszColumn == NULL )
    {
        return false;
    }

    char szDescription[OWTEXT];
    char szCreateBlank[OWTEXT];
    char szInsert[OWTEXT];

    if( bUpdate == false )
    {
        //  ---------------------------------------------------------------
        //  Description parameters
        //  ---------------------------------------------------------------

        if ( pszDescription  )
        {
            strcpy( szDescription, pszDescription );
        }
        else
        {
            strcpy( szDescription, CPLSPrintf(
                "(%s MDSYS.SDO_GEORASTER)", pszColumn ) );
        }

        //  ---------------------------------------------------------------
        //  Insert parameters
        //  ---------------------------------------------------------------

        if( pszInsert )
        {
            if( strstr( pszInsert, "VALUES" ) == NULL &&
                strstr( pszInsert, "values" ) == NULL )
            {
                strcpy( szValues, CPLSPrintf( "VALUES %s", pszInsert ) );
            }
            else
            {
                strcpy( szValues, pszInsert );
            }
        }
        else
        {
            strcpy( szValues, CPLStrdup(
                "VALUES (SDO_GEOR.INIT(NULL,NULL))" ) );
        }
    }

    //  -------------------------------------------------------------------
    //  Parse RDT/RID from the current szValues
    //  -------------------------------------------------------------------

    char szRDT[OWNAME];
    char szRID[OWCODE];

    if( pszDataTable )
    {
        strcpy( szRDT, CPLSPrintf( "'%s'", pszDataTable ) );
    }
    else
    {
        strcpy( szRDT, OWParseSDO_GEOR_INIT( szValues, 1 ) );
    }

    if ( nRasterId > 0 )
    {
        strcpy( szRID, CPLSPrintf( "%d", nRasterId ) );
    }
    else
    {
        strcpy( szRID, OWParseSDO_GEOR_INIT( szValues, 2 ) );
    }

    //  -------------------------------------------------------------------
    //  Prepare initialization parameters
    //  -------------------------------------------------------------------

    if( nRasterBands == 1 )
    {
        strcpy( szCreateBlank, CPLSPrintf( "SDO_GEOR.createBlank(20001, "
            "SDO_NUMBER_ARRAY(0, 0), "
            "SDO_NUMBER_ARRAY(%d, %d), 0, %s, %s)",
            nRasterRows, nRasterColumns, szRDT, szRID) );
    }
    else
    {
        strcpy( szCreateBlank, CPLSPrintf( "SDO_GEOR.createBlank(21001, "
            "SDO_NUMBER_ARRAY(0, 0, 0), "
            "SDO_NUMBER_ARRAY(%d, %d, %d), 0, %s, %s)",
            nRasterRows, nRasterColumns, nRasterBands, szRDT, szRID ) );
    }

    if( ! bUpdate )
    {
        strcpy( szInsert,
            OWReplaceString( szValues, "SDO_GEOR.INIT", ")", "GR1" ) );
    }

    //  -----------------------------------------------------------
    //  Storage parameters
    //  -----------------------------------------------------------

    nColumnBlockSize = nColumnBlockSize == 0 ? 256 : nColumnBlockSize;
    nRowBlockSize    = nRowBlockSize    == 0 ? 256 : nRowBlockSize;
    nBandBlockSize   = nBandBlockSize   == 0 ? 1   : nBandBlockSize;

    if( poConnection->GetVersion() < 11 )
    {
        if( nRasterBands == 1 )
        {
            strcpy( szFormat, CPLSPrintf(
                "blockSize=(%d, %d) "
                "cellDepth=%s "
                "interleaving=%s "
                "pyramid=FALSE "
                "compression=NONE ",
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
                "pyramid=FALSE "
                "compression=NONE ",
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
                "compression=%s "
                "'",
                nRasterRows, nRasterColumns,
                nColumnBlockSize, nRowBlockSize,
                pszCellDepth,
                szInterleaving,
                pszCompressionType ) );
        }
        else
        {
            strcpy( szFormat, CPLSPrintf(
                "21001, '"
                "dimSize=(%d,%d,%d) "
                "blockSize=(%d,%d,%d) "
                "cellDepth=%s "
                "interleaving=%s "
                "compression=%s "
                "'",
                nRasterRows, nRasterColumns, nRasterBands,
                nColumnBlockSize, nRowBlockSize, nBandBlockSize,
                pszCellDepth,
                szInterleaving,
                pszCompressionType ) );
        }
    }

    nTotalColumnBlocks = (int)
        ( ( nRasterColumns + nColumnBlockSize - 1 ) / nColumnBlockSize );

    nTotalRowBlocks = (int)
        ( ( nRasterRows + nRowBlockSize - 1 ) / nRowBlockSize );

    nTotalBandBlocks = (int)
        ( ( nRasterBands + nBandBlockSize - 1) / nBandBlockSize );

    //  -------------------------------------------------------------------
    //  Create Georaster Table if needed
    //  -------------------------------------------------------------------

    OWStatement* poStmt;

    if( ! bUpdate )
    {
        poStmt = poConnection->CreateStatement( CPLSPrintf(
            "DECLARE\n"
            "  TAB VARCHAR2(68)  := UPPER(:1);\n"
            "  COL VARCHAR2(68)  := UPPER(:2);\n"
            "  OWN VARCHAR2(68)  := UPPER(:3);\n"
            "  CNT NUMBER        := 0;\n"
            "BEGIN\n"
            "  EXECUTE IMMEDIATE 'SELECT COUNT(*) FROM ALL_TABLES\n"
            "    WHERE TABLE_NAME = :1 AND OWNER = UPPER(:2)'\n"
            "      INTO CNT USING TAB, OWN;\n"
            "\n"
            "  IF CNT = 0 THEN\n"
            "    EXECUTE IMMEDIATE 'CREATE TABLE %s%s %s';\n"
            "    SDO_GEOR_UTL.createDMLTrigger( TAB,  COL );\n"
            "  END IF;\n"
            "END;", 
            pszSchema, pszTable,
            szDescription ) );

        poStmt->Bind( pszTable );
        poStmt->Bind( pszColumn );
        poStmt->Bind( pszOwner );

        if( ! poStmt->Execute() )
        {
            delete ( poStmt );
            CPLError( CE_Failure, CPLE_AppDefined, "Create Table Error!" );
            return false;
        }

        delete poStmt;
    }

    //  -----------------------------------------------------------
    //  Prepare UPDATE or INSERT comand
    //  -----------------------------------------------------------

    char szCommand[OWTEXT];

    if( bUpdate )
    {
        strcpy( szCommand, CPLSPrintf(
            "UPDATE %s%s T SET %s = GR1 WHERE %s RETURNING %s INTO GR1;",
            pszSchema, pszTable, pszColumn, pszWhere, pszColumn ) );
    }
    else
    {
        strcpy( szCommand, CPLSPrintf(
            "INSERT INTO %s%s %s RETURNING %s INTO GR1;",
            pszSchema, pszTable, szInsert, pszColumn ) );
    }

    //  -----------------------------------------------------------
    //  Create RTD if needed and insert/update GeoRaster
    //  -----------------------------------------------------------

    char szBindRDT[OWNAME] = "";
    int  nBindRID = 0;

    if( poConnection->GetVersion() > 10 )
    {
        poStmt = poConnection->CreateStatement( CPLSPrintf(
            "DECLARE\n"
            "  TAB  VARCHAR2(68)    := UPPER(:1);\n"
            "  COL  VARCHAR2(68)    := UPPER(:2);\n"
            "  OWN  VARCHAR2(68)    := UPPER(:3);\n"
            "  CNT  NUMBER          := 0;\n"
            "  GR1  SDO_GEORASTER   := NULL;\n"
            "BEGIN\n"
            "\n"
            "  GR1 := %s;\n"
            "\n"
            "  GR1.spatialExtent := NULL;\n"
            "\n"
            "  %s\n"
            "\n"
            "  SELECT GR1.RASTERDATATABLE INTO :rdt FROM DUAL;\n"
            "  SELECT GR1.RASTERID        INTO :rid FROM DUAL;\n"
            "\n"
            "  EXECUTE IMMEDIATE 'SELECT COUNT(*) FROM ALL_OBJECT_TABLES\n"
            "    WHERE TABLE_NAME = :1 AND OWNER = UPPER(:2)'\n"
            "      INTO CNT USING :rdt, OWN;\n"
            "\n"
            "  IF CNT = 0 THEN\n"
            "    EXECUTE IMMEDIATE 'CREATE TABLE %s'||:rdt||' OF MDSYS.SDO_RASTER\n"
            "      (PRIMARY KEY (RASTERID, PYRAMIDLEVEL, BANDBLOCKNUMBER,\n"
            "      ROWBLOCKNUMBER, COLUMNBLOCKNUMBER))\n"
            "      LOB(RASTERBLOCK) STORE AS (NOCACHE NOLOGGING)';\n"
            "  END IF;\n"
            "\n"
            "  SDO_GEOR.createTemplate(GR1, %s, null, 'TRUE');\n"
            "\n"
            "  UPDATE %s%s T SET %s = GR1 WHERE"
            " T.%s.RasterDataTable = :rdt AND"
            " T.%s.RasterId = :rid;\n"
            "END;\n",
            szCreateBlank,
            szCommand,
            pszSchema,
            szFormat,
            pszSchema, pszTable, pszColumn, pszColumn, pszColumn  ) );

        poStmt->Bind( pszTable );
        poStmt->Bind( pszColumn );
        poStmt->Bind( pszOwner );
        poStmt->BindName( (char*) ":rdt", szBindRDT );
        poStmt->BindName( (char*) ":rid", &nBindRID );

        if( ! poStmt->Execute() )
        {
            delete poStmt;
            CPLError( CE_Failure, CPLE_AppDefined,
                "Failure to initialize GeoRaster" );
            return false;
        }

        CPLFree( pszDataTable );

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
        "  W    NUMBER          := :1;\n"
        "  H    NUMBER          := :2;\n"
        "  BB   NUMBER          := :3;\n"
        "  RB   NUMBER          := :4;\n"
        "  CB   NUMBER          := :5;\n"
        "  OWN  VARCHAR2(68)    := UPPER(:6);\n"
        "  X    NUMBER          := 0;\n"
        "  Y    NUMBER          := 0;\n"
        "  CNT  NUMBER          := 0;\n"
        "  GR1  SDO_GEORASTER   := NULL;\n"
        "  GR2  SDO_GEORASTER   := NULL;\n"
        "  STM  VARCHAR2(1024)  := '';\n"
        "BEGIN\n"
        "\n"
        "  GR1 := %s;\n"
        "\n"
        "  GR1.spatialExtent := NULL;\n"
        "\n"
        "  %s\n"
        "\n"
        "  SELECT GR1.RASTERDATATABLE INTO :rdt FROM DUAL;\n"
        "  SELECT GR1.RASTERID        INTO :rid FROM DUAL;\n"
        "\n"
        "  SELECT %s INTO GR2 FROM %s%s T WHERE"
        " T.%s.RasterDataTable = :rdt AND"
        " T.%s.RasterId = :rid FOR UPDATE;\n"
        "  SELECT %s INTO GR1 FROM %s%s T WHERE"
        " T.%s.RasterDataTable = :rdt AND"
        " T.%s.RasterId = :rid;\n"
        "  SDO_GEOR.changeFormatCopy(GR1, '%s', GR2);\n"
        "  UPDATE %s%s T SET %s = GR2     WHERE"
        " T.%s.RasterDataTable = :rdt AND"
        " T.%s.RasterId = :rid;\n"
        "\n"
        "  EXECUTE IMMEDIATE 'SELECT COUNT(*) FROM ALL_OBJECT_TABLES\n"
        "    WHERE TABLE_NAME = :1 AND OWNER = UPPER(:2)'\n"
        "      INTO CNT USING :rdt, OWN;\n"
        "\n"
        "  IF CNT = 0 THEN\n"
        "    EXECUTE IMMEDIATE 'CREATE TABLE %s'||:rdt||' OF MDSYS.SDO_RASTER\n"
        "      (PRIMARY KEY (RASTERID, PYRAMIDLEVEL, BANDBLOCKNUMBER,\n"
        "      ROWBLOCKNUMBER, COLUMNBLOCKNUMBER))\n"
        "      LOB(RASTERBLOCK) STORE AS (NOCACHE NOLOGGING)';\n"
        "  ELSE\n"
        "    EXECUTE IMMEDIATE 'DELETE FROM %s'||:rdt||' WHERE RASTERID ='||:rid||' ';\n"
        "  END IF;\n"
        "\n"
        "  STM := 'INSERT INTO %s'||:rdt||' VALUES (:1,0,:2-1,:3-1,:4-1,\n"
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
        szCreateBlank,
        szCommand,
        pszColumn, pszSchema, pszTable, pszColumn, pszColumn,
        pszColumn, pszSchema, pszTable, pszColumn, pszColumn, szFormat,
        pszSchema, pszTable, pszColumn, pszColumn, pszColumn,
        pszSchema, pszSchema, pszSchema ) );

    poStmt->Bind( &nColumnBlockSize );
    poStmt->Bind( &nRowBlockSize );
    poStmt->Bind( &nTotalBandBlocks );
    poStmt->Bind( &nTotalRowBlocks );
    poStmt->Bind( &nTotalColumnBlocks );
    poStmt->Bind( pszOwner );
    poStmt->BindName( (char*) ":rdt", szBindRDT );
    poStmt->BindName( (char*) ":rid", &nBindRID );

    if( ! poStmt->Execute() )
    {
        delete poStmt;
        CPLError( CE_Failure, CPLE_AppDefined,
            "Failure to initialize GeoRaster" );
        return false;
    }

    CPLFree( pszDataTable );

    pszDataTable = CPLStrdup( szBindRDT );
    nRasterId    = nBindRID;

    delete poStmt;

    return true;
}

//  ---------------------------------------------------------------------------
//                                                          PrepareToOverwrite()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::PrepareToOverwrite( void )
{
    nTotalColumnBlocks  = 0;
    nTotalRowBlocks     = 0;
    nTotalBandBlocks    = 0;
    if( sscanf( pszCellDepth, "%dBIT", &nCellSizeBits ) )
    {
        nGDALCellBytes   = GDALGetDataTypeSize(
                          OWGetDataType( pszCellDepth ) ) / 8;
    }
    else
    {
        nGDALCellBytes   = 1;
    }
    dfXCoefficient[0]   = 1.0;
    dfXCoefficient[1]   = 0.0;
    dfXCoefficient[2]   = 0.0;
    dfYCoefficient[0]   = 0.0;
    dfYCoefficient[1]   = 1.0;
    dfYCoefficient[2]   = 0.0;
    pszCompressionType  = CPLStrdup( "NONE" );
    nCompressQuality    = 75;
    bIsReferenced       = false;
    nCurrentBlock       = -1;
    nCurrentLevel       = -1;
    szInterleaving[0]   = 'B';
    szInterleaving[1]   = 'S';
    szInterleaving[2]   = 'Q';
    szInterleaving[3]   = '\0';
    bIOInitialized      = false;
    bFlushMetadata      = false;
    nSRID               = -1;
    nPyramidMaxLevel    = 0;
    nBlockCount         = 0;
    bOrderlyAccess      = true;
    sDInfo.global_state = 0;
    sCInfo.global_state = 0;
    bHasBitmapMask      = false;
    eModelCoordLocation = MCL_DEFAULT;
}

//  ---------------------------------------------------------------------------
//                                                                     Delete()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::Delete( void )
{
    OWStatement* poStmt = poConnection->CreateStatement( CPLSPrintf(
      "UPDATE %s T SET %s = NULL WHERE %s\n", pszTable, pszColumn, pszWhere ) );

    bool bReturn = poStmt->Execute();

    delete poStmt;

    return bReturn;
}

//  ---------------------------------------------------------------------------
//                                                            SetGeoReference()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::SetGeoReference( int nSRIDIn )
{
    if( nSRIDIn == 0 )
    {
        nSRIDIn = UNKNOWN_CRS;
    }

    nSRID = nSRIDIn;

    bIsReferenced = true;

    bFlushMetadata = true;
}

//  ---------------------------------------------------------------------------
//                                                             SetCompression()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::SetCompression( const char* pszType, int nQuality )
{
    pszCompressionType = CPLStrdup( pszType );
    nCompressQuality = nQuality;

    CPLXMLNode* psRInfo = CPLGetXMLNode( phMetadata, "rasterInfo" );
    CPLXMLNode* psNode = CPLGetXMLNode( psRInfo, "compression" );

    if( psNode )
    {
        CPLRemoveXMLChild( psRInfo, psNode );
        CPLDestroyXMLNode( psNode );
    }

    psNode = CPLCreateXMLNode( psRInfo, CXT_Element, "compression" );

    CPLCreateXMLElementAndValue( psNode, "type", pszCompressionType );

    if( EQUALN( pszCompressionType, "JPEG", 4 ) )
    {
        CPLCreateXMLElementAndValue( psNode, "quality",
            CPLSPrintf( "%d", nCompressQuality ) );
    }

    bFlushMetadata = true;
}

//  ---------------------------------------------------------------------------
//                                                                  GetWKText()
//  ---------------------------------------------------------------------------

char* GeoRasterWrapper::GetWKText( int nSRIDin )
{
    char szWKText[OWTEXT];
    char szAuthority[OWTEXT];

    OWStatement* poStmt = poConnection->CreateStatement(
        "SELECT WKTEXT, AUTH_NAME\n"
        "FROM   MDSYS.CS_SRS\n"
        "WHERE  SRID = :1 AND WKTEXT IS NOT NULL" );

    poStmt->Bind( &nSRIDin );
    poStmt->Define( szWKText,  OWTEXT );
    poStmt->Define( szAuthority,  OWTEXT );

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
       nBandBlockSize = nRasterBands;
    }

    //  -------------------------------------------------------------------
    //  Get data type
    //  -------------------------------------------------------------------

    pszCellDepth        = CPLStrdup( CPLGetXMLValue( phMetadata,
                            "rasterInfo.cellDepth", "8BIT_U" ) );

    if( sscanf( pszCellDepth, "%dBIT", &nCellSizeBits ) )
    {
        nGDALCellBytes   = GDALGetDataTypeSize(
                          OWGetDataType( pszCellDepth ) ) / 8;
    }
    else
    {
        nGDALCellBytes   = 1;
    }

    pszCompressionType  = CPLStrdup( CPLGetXMLValue( phMetadata,
                            "rasterInfo.compression.type", "NONE" ) );

    if( EQUALN( pszCompressionType, "JPEG", 4 ) )
    {
        nCompressQuality = atoi( CPLGetXMLValue( phMetadata,
                            "rasterInfo.compression.quality", "75" ) );

        strcpy( szInterleaving, "BIP" );
    }

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
    //  Get Pyramid details
    //  -------------------------------------------------------------------

    char szPyramidType[OWCODE];

    strcpy( szPyramidType, CPLGetXMLValue( phMetadata,
                            "rasterInfo.pyramid.type", "None" ) );

    if( EQUAL( szPyramidType, "DECREASE" ) )
    {
        nPyramidMaxLevel = atoi( CPLGetXMLValue( phMetadata,
                            "rasterInfo.pyramid.maxLevel", "0" ) );
    }

    //  -------------------------------------------------------------------
    //  Prepare to get Extents
    //  -------------------------------------------------------------------

    char szModelCoord[OWCODE];

    strcpy( szModelCoord, CPLGetXMLValue( phMetadata,
        "spatialReferenceInfo.modelCoordinateLocation", "UPPERLEFT" ) );

    if( EQUAL( szModelCoord, "CENTER") )
    {
        eModelCoordLocation    = MCL_CENTER;
    }
    else
    {
        eModelCoordLocation    = MCL_UPPERLEFT;
    }

    bIsReferenced       = EQUAL( "TRUE", CPLGetXMLValue( phMetadata,
                            "spatialReferenceInfo.isReferenced", "FALSE" ) );

    nSRID               = atoi( CPLGetXMLValue( phMetadata,
                            "spatialReferenceInfo.SRID", "0" ) );

    if( nSRID == 0 ||
        nSRID == UNKNOWN_CRS )
    {
        bIsReferenced   = false;
    }
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

    if( eModelCoordLocation == MCL_UPPERLEFT )
    {
        poStmt = poConnection->CreateStatement( CPLSPrintf(
            "SELECT\n"
            "  SDO_GEOR.getModelCoordinate(%s, 0, SDO_NUMBER_ARRAY(%d, %d)),\n"
            "  SDO_GEOR.getModelCoordinate(%s, 0, SDO_NUMBER_ARRAY(%d, %d)),\n"
            "  SDO_GEOR.getModelCoordinate(%s, 0, SDO_NUMBER_ARRAY(%d, %d)),\n"
            "  SDO_GEOR.getModelCoordinate(%s, 0, SDO_NUMBER_ARRAY(%d, %d))\n"
            "FROM  %s%s T\n"
            "WHERE %s",
                pszColumn, 0, 0,
                pszColumn, 0, nRasterColumns,
                pszColumn, nRasterRows, 0,
                pszColumn, nRasterRows, nRasterColumns,
                pszSchema, pszTable,
                pszWhere ) );
    }
    else
    {
        poStmt = poConnection->CreateStatement( CPLSPrintf(
            "SELECT\n"
            "  SDO_GEOR.getModelCoordinate(%s, 0, SDO_NUMBER_ARRAY(%.1f, %.1f)),\n"
            "  SDO_GEOR.getModelCoordinate(%s, 0, SDO_NUMBER_ARRAY(%.1f, %.1f)),\n"
            "  SDO_GEOR.getModelCoordinate(%s, 0, SDO_NUMBER_ARRAY(%.1f, %.1f)),\n"
            "  SDO_GEOR.getModelCoordinate(%s, 0, SDO_NUMBER_ARRAY(%.1f, %.1f))\n"
            "FROM  %s%s T\n"
            "WHERE %s",
                pszColumn, -0.5, -0.5,
                pszColumn, -0.5, (nRasterColumns - 0.5),
                pszColumn, (nRasterRows - 0.5), -0.5,
                pszColumn, (nRasterRows - 0.5), (nRasterColumns - 0.5),
                pszSchema, pszTable,
                pszWhere ) );
    }

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
    padfTransform[1] = ( dfLRx - dfULx ) / (double) nRasterColumns;
    padfTransform[2] = dfRotation;

    padfTransform[3] = dfULy;
    padfTransform[4] = -dfRotation;
    padfTransform[5] = ( dfLRy - dfULy ) / (double) nRasterRows;

    dfXCoefficient[0] = padfTransform[1];
    dfXCoefficient[1] = padfTransform[2];
    dfXCoefficient[2] = padfTransform[0];
    dfYCoefficient[0] = padfTransform[4];
    dfYCoefficient[1] = padfTransform[5];
    dfYCoefficient[2] = padfTransform[3];

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
            CPLDestroyXMLNode( phSubLayer );
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

        int nCount = 0;

        switch( nCellSizeBits )
        {
            case 1 :
                nCount = 2;
                break;
            case 2 :
                nCount = 4;
                break;
            case 4:
                nCount = 16;
                break;
            default:
                nCount = poCT->GetColorEntryCount();
        }


        for( iColor = 0; iColor < nCount; iColor++ )
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

bool GeoRasterWrapper::InitializeIO( int nLevel, bool bUpdate )
{
    // --------------------------------------------------------------------
    // Free previous LOB locator list
    // --------------------------------------------------------------------

    if( pahLocator && nBlockCount )
    {
        OWStatement::Free( pahLocator, nBlockCount );
    }
    CPLFree( pahLocator );
    CPLFree( pabyBlockBuf );
    CPLFree( pabyBlockBuf2 );
    delete poStmtRead;
    delete poStmtWrite;

    // --------------------------------------------------------------------
    // Restore the level 0 dimensions from metadata info
    // --------------------------------------------------------------------

    nRowBlockSize       = atoi( CPLGetXMLValue( phMetadata,
                            "rasterInfo.blocking.rowBlockSize", "0" ) );

    nColumnBlockSize    = atoi( CPLGetXMLValue( phMetadata,
                            "rasterInfo.blocking.columnBlockSize", "0" ) );

    nTotalColumnBlocks  = atoi( CPLGetXMLValue( phMetadata,
                            "rasterInfo.blocking.totalColumnBlocks","0") );

    nTotalRowBlocks     = atoi( CPLGetXMLValue( phMetadata,
                            "rasterInfo.blocking.totalRowBlocks", "0" ) );

    // --------------------------------------------------------------------
    // Calculate the actual size of a lower resolution block
    // --------------------------------------------------------------------

    if( nLevel )
    {

        // ----------------------------------------------------------------
        // Recalculate block size
        // ----------------------------------------------------------------

        double dfScale = pow( (double) 2.0, (double) nLevel );

        int nXSize  = (int) floor( (double) nRasterColumns   / dfScale );
        int nYSize  = (int) floor( (double) nRasterRows      / dfScale );
        int nXBlock = (int) floor( (double) nColumnBlockSize / 2.0 );
        int nYBlock = (int) floor( (double) nRowBlockSize    / 2.0 );

        if( nXSize <= nXBlock && nYSize <= nYBlock )
        {
            nColumnBlockSize  = nXSize;
            nRowBlockSize     = nYSize;
        }

        // ----------------------------------------------------------------
        // Recalculate blocks quantity
        // ----------------------------------------------------------------

        nTotalColumnBlocks  = (int) ceil( (double) nTotalColumnBlocks / dfScale );
        nTotalRowBlocks     = (int) ceil( (double) nTotalRowBlocks / dfScale );

        nTotalColumnBlocks  = (int) MAX( 1, nTotalColumnBlocks );
        nTotalRowBlocks     = (int) MAX( 1, nTotalRowBlocks );

    }

    // --------------------------------------------------------------------
    // Calculate number and size of the BLOB blocks
    // --------------------------------------------------------------------

    nBlockCount     = nTotalColumnBlocks * nTotalRowBlocks * nTotalBandBlocks;

    nBlockBytes     = nColumnBlockSize   * nRowBlockSize   * nBandBlockSize *
                      nCellSizeBits / 8;

    nGDALBlockBytes = nColumnBlockSize   * nRowBlockSize   * nGDALCellBytes;

    // --------------------------------------------------------------------
    // Allocate buffer for one raster block
    // --------------------------------------------------------------------

    int nMaxBufferSize = MAX( nBlockBytes, nGDALBlockBytes );

    pabyBlockBuf = (GByte*) VSIMalloc( sizeof(GByte) * nMaxBufferSize );

    if ( pabyBlockBuf == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                "InitializeIO - Block Buffer" );
        return false;
    }

    if( bUpdate && ! EQUAL( pszCompressionType, "None") )
    {
        pabyBlockBuf2 = (GByte*) VSIMalloc( sizeof(GByte) * nMaxBufferSize );

        if ( pabyBlockBuf2 == NULL )
        {
            CPLError( CE_Failure, CPLE_OutOfMemory,
                    "InitializeIO - Block Buffer 2" );
            return false;
        }
    }

    // --------------------------------------------------------------------
    // Allocate array of LOB Locators
    // --------------------------------------------------------------------

    pahLocator = (OCILobLocator**) VSIMalloc( sizeof(void*) * nBlockCount );

    if ( pahLocator == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                "InitializeIO - LobLocator Array" );
        return false;
    }

    //  --------------------------------------------------------------------
    //  Issue a statement to load the locators
    //  --------------------------------------------------------------------

    const char* pszUpdate = "";

    if( bUpdate )
    {
        pszUpdate = CPLStrdup( "\nFOR UPDATE" );
    }

    OWStatement* poStmt = poConnection->CreateStatement( CPLSPrintf(
        "SELECT RASTERBLOCK\n"
        "FROM   %s%s\n"
        "WHERE  RASTERID = :1 AND\n"
        "       PYRAMIDLEVEL = :3\n"
        "ORDER BY\n"
        "       BANDBLOCKNUMBER ASC,\n"
        "       ROWBLOCKNUMBER ASC,\n"
        "       COLUMNBLOCKNUMBER ASC%s",
        pszSchema,
        pszDataTable,
        pszUpdate ) );

    poStmt->Bind( &nRasterId );
    poStmt->Bind( &nLevel );
    poStmt->Define( pahLocator, nBlockCount );
    poStmt->Execute();

    if( poStmt->Fetch( nBlockCount ) == false )
    {
        return false;
    }

    //  --------------------------------------------------------------------
    //  Assign the statement pointer to the apropriated operation
    //  --------------------------------------------------------------------

    if( bUpdate )
    {
        poStmtWrite = poStmt;
    }
    else
    {
        poStmtRead = poStmt;
    }

    bIOInitialized = true;

    return true;
}

//  ---------------------------------------------------------------------------
//                                                               GetDataBlock()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::GetDataBlock( int nBand,
                                     int nLevel,
                                     int nXOffset,
                                     int nYOffset,
                                     void* pData )
{
    if( bIOInitialized == false || nCurrentLevel != nLevel )
    {
        InitializeIO( nLevel );

        nCurrentLevel = nLevel;
        nCurrentBlock = -1;
    }

    int nBlock = CALCULATEBLOCK( nBand, nXOffset, nYOffset, nBandBlockSize,
                                 nTotalColumnBlocks, nTotalRowBlocks );

    unsigned long nBytesRead = 0;

    if( nCurrentBlock != nBlock )
    {
        nCurrentBlock = nBlock;

        nBytesRead = poStmtRead->ReadBlob( pahLocator[nBlock],
                                           pabyBlockBuf,
                                           nBlockBytes );
        if( nBytesRead == 0 )
        {
            memset( pData, 0, nGDALBlockBytes );

            return true;
        }

        if( nBytesRead < nBlockBytes && EQUAL( pszCompressionType, "NONE") )
        {
            CPLDebug("GEOR", "BLOB size (%ld) is smaller than expected (%ld) !",
                nBytesRead,  nBlockBytes );

            memset( pData, 0, nGDALBlockBytes );

            return true;
        }

        if( nBytesRead > nBlockBytes )
        {
            CPLDebug("GEOR", "BLOB size (%ld) is bigger than expected (%ld) !",
                nBytesRead,  nBlockBytes );

            memset( pData, 0, nGDALBlockBytes );

            return true;
        }

#ifndef CPL_MSB
        if( nCellSizeBits > 8 )
        {
            int nWordSize  = nCellSizeBits / 8;
            int nWordCount = nColumnBlockSize * nRowBlockSize * nBandBlockSize;
            GDALSwapWords( pabyBlockBuf, nWordSize, nWordCount, nWordSize );
        }
#endif

        //  ----------------------------------------------------------------
        //  Uncompress
        //  ----------------------------------------------------------------

        if( EQUALN( pszCompressionType, "JPEG", 4 ) )
        {
            UncompressJpeg( nBytesRead );
        }
        else if ( EQUAL( pszCompressionType, "DEFLATE" ) )
        {
            UncompressDeflate( nBytesRead );
        }

        //  ----------------------------------------------------------------
        //  Unpack NBits
        //  ----------------------------------------------------------------

        if( nCellSizeBits < 8 || nLevel == DEFAULT_BMP_MASK )
        {
            UnpackNBits( pabyBlockBuf );
        }
    }

    //  --------------------------------------------------------------------
    //  Uninterleaving
    //  --------------------------------------------------------------------

    int nStart = ( nBand - 1 ) % nBandBlockSize;

    if( EQUAL( szInterleaving, "BSQ" ) || nBandBlockSize == 1 )
    {
        nStart *= nGDALBlockBytes;

        memcpy( pData, &pabyBlockBuf[nStart], nGDALBlockBytes );
    }
    else
    {
        int nIncr   = nBandBlockSize * nGDALCellBytes;
        int nSize   = nGDALCellBytes;

        if( EQUAL( szInterleaving, "BIL" ) )
        {
            nStart  *= nColumnBlockSize;
            nIncr   *= nColumnBlockSize;
            nSize   *= nColumnBlockSize;
        }

        GByte* pabyData = (GByte*) pData;

        unsigned long ii = 0;
        unsigned long jj = nStart * nGDALCellBytes;

        for( ii = 0; ii < nGDALBlockBytes; ii += nSize, jj += nIncr )
        {
            memcpy( &pabyData[ii], &pabyBlockBuf[jj], nSize );
        }
    }
/*
    CPLDebug( "GEOR",
      "nBlock, nBand, nLevel, nXOffset, nYOffset, nColumnBlockSize, nRowBlockSize = "
      "%d, %d, %d, %d, %d, %d, %d",
       nBlock, nBand, nLevel, nXOffset, nYOffset, nColumnBlockSize, nRowBlockSize );
*/
    return true;
}

//  ---------------------------------------------------------------------------
//                                                               SetDataBlock()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::SetDataBlock( int nBand,
                                     int nLevel,
                                     int nXOffset,
                                     int nYOffset,
                                     void* pData )
{
#ifndef CPL_MSB
    if( nCellSizeBits > 8 )
    {
        int nWordSize  = nCellSizeBits / 8;
        int nWordCount = nColumnBlockSize * nRowBlockSize;
        GDALSwapWords( pData, nWordSize, nWordCount, nWordSize );
    }
#endif

    if( bIOInitialized == false || nCurrentLevel != nLevel )
    {
        InitializeIO( nLevel, true );

        nCurrentLevel = nLevel;
        nCurrentBlock = -1;
    }

    int nBlock = CALCULATEBLOCK( nBand, nXOffset, nYOffset, nBandBlockSize,
                                 nTotalColumnBlocks, nTotalRowBlocks );

    //  --------------------------------------------------------------------
    //  Read interleaved block
    //  --------------------------------------------------------------------

    unsigned long nBytesRead = 0;

    if( bOrderlyAccess == false &&
        nBandBlockSize > 1 &&
        nCurrentBlock != nBlock )
    {
        CPLDebug( "GEOR", "Reloading block %d", nBlock );

        nBytesRead = poStmtWrite->ReadBlob( pahLocator[nBlock],
                                            pabyBlockBuf,
                                            nBlockBytes );
        if( nBytesRead == 0 )
        {
            memset( pabyBlockBuf, 0, nBlockBytes );
        }
        else
        {
            //  ------------------------------------------------------------
            //  Unpack NBits
            //  ------------------------------------------------------------

            if( nCellSizeBits < 8 || nLevel == DEFAULT_BMP_MASK )
            {
                UnpackNBits( pabyBlockBuf );
            }

            //  ------------------------------------------------------------
            //  Uncompress
            //  ------------------------------------------------------------

            if( EQUALN( pszCompressionType, "JPEG", 4 ) )
            {
                UncompressJpeg( nBytesRead );
            }
            else if ( EQUAL( pszCompressionType, "DEFLATE" ) )
            {
                UncompressDeflate( nBytesRead );
            }
        }
    }

    GByte *pabyInBuf = (GByte *) pData;

    nCurrentBlock = nBlock;

    //  --------------------------------------------------------------------
    //  Interleave
    //  --------------------------------------------------------------------

    int nStart = ( nBand - 1 ) % nBandBlockSize;

    if( EQUAL( szInterleaving, "BSQ" ) || nBandBlockSize == 1 )
    {
        nStart *= nGDALBlockBytes;

        memcpy( &pabyBlockBuf[nStart], pabyInBuf, nGDALBlockBytes );
    }
    else
    {
        int nIncr   = nBandBlockSize * nGDALCellBytes;
        int nSize   = nGDALCellBytes;

        if( EQUAL( szInterleaving, "BIL" ) )
        {
            nStart  *= nColumnBlockSize;
            nIncr   *= nColumnBlockSize;
            nSize   *= nColumnBlockSize;
        }

        unsigned long ii = 0;
        unsigned long jj = nStart * nGDALCellBytes;

        for( ii = 0; ii < nGDALBlockBytes; ii += nSize, jj += nIncr )
        {
            memcpy( &pabyBlockBuf[jj], &pabyInBuf[ii], nSize );
        }
    }

    //  --------------------------------------------------------------------
    //  Compress ( from pabyBlockBuf to pabyBlockBuf2 )
    //  --------------------------------------------------------------------

    GByte* pabyOutBuf = (GByte *) pabyBlockBuf;

    unsigned long nWriteBytes = nBlockBytes;

    if( ! EQUAL( pszCompressionType, "None" ) )
    {
        if( EQUALN( pszCompressionType, "JPEG", 4 ) )
        {
            nWriteBytes = CompressJpeg();
        }
        else if ( EQUAL( pszCompressionType, "DEFLATE" ) )
        {
            nWriteBytes = CompressDeflate();
        }

        pabyOutBuf = pabyBlockBuf2;
    }

    //  --------------------------------------------------------------------
    //  Pack bits ( inside pabyOutBuf )
    //  --------------------------------------------------------------------

    if( nCellSizeBits < 8 || nLevel == DEFAULT_BMP_MASK )
    {
        PackNBits( pabyOutBuf );
    }

    //  --------------------------------------------------------------------
    //  Write BLOB
    //  --------------------------------------------------------------------
/*
    CPLDebug( "GEOR",
      "nBlock, nBand, nLevel, nXOffset, nYOffset, nColumnBlockSize, nRowBlockSize = "
      "%d, %d, %d, %d, %d, %d, %d",
       nBlock, nBand, nLevel, nXOffset, nYOffset, nColumnBlockSize, nRowBlockSize );
*/
    if( ! poStmtWrite->WriteBlob( pahLocator[nBlock],
                                  pabyOutBuf,
                                  nWriteBytes ) )
    {
        return false;
    }

    bFlushMetadata = true;

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
    CPLXMLNode* psNData = CPLSearchXMLNode( psRInfo, "NODATA" );

    if( psNData == NULL )
    {
        psNData = CPLCreateXMLNode( NULL, CXT_Element, "NODATA" );

        // Plug NOTADA just after cellDepth node

        CPLXMLNode* psCDepth = CPLGetXMLNode( psRInfo, "cellDepth" );
        CPLXMLNode* psPointer = psCDepth->psNext;
        psCDepth->psNext = psNData;
        psNData->psNext = psPointer;
    }

    const char* pszFormat = EQUAL( &pszCellDepth[6], "REAL") ? "%f" : "%.0f";
    CPLSetXMLValue( psRInfo, "NODATA", CPLSPrintf( pszFormat, dfNoDataValue ) );

    bFlushMetadata = true;

    return true;
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

    if( ( nRasterBands > 2 ) &&
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
    //  Update BitmapMask info
    //  --------------------------------------------------------------------

    if( bHasBitmapMask )
    {
        CPLXMLNode* psLayers = CPLGetXMLNode( phMetadata, "layerInfo" );

        if( psLayers )
        {
            CPLCreateXMLElementAndValue( psLayers, "bitmapMask", "true" );
        }
    }

    //  --------------------------------------------------------------------
    //  Update the Metadata directly from the XML text
    //  --------------------------------------------------------------------

    double dfXCoef[3], dfYCoef[3];

    int eModel = eForceCoordLocation;

    dfXCoef[0] = dfXCoefficient[0];
    dfXCoef[1] = dfXCoefficient[1];
    dfXCoef[2] = dfXCoefficient[2];

    dfYCoef[0] = dfYCoefficient[0];
    dfYCoef[1] = dfYCoefficient[1];
    dfYCoef[2] = dfYCoefficient[2];

    if( eModel == MCL_CENTER )
    {
        dfXCoef[2] += ( dfXCoef[0] / 2.0 );
        dfYCoef[2] += ( dfYCoef[1] / 2.0 );
    }

    CPLString osStatemtn = "";

    osStatemtn.Printf(
        "DECLARE\n"
        "  GR1  sdo_georaster;\n"
        "  SRID number;\n"
        "BEGIN\n"
        "\n"
        "  SELECT %s INTO GR1 FROM %s%s T WHERE %s FOR UPDATE;\n"
        "\n"
        "  GR1.metadata := XMLTYPE('%s');\n"
        "\n"
        "  SRID := :2;\n"
        "  IF SRID = 0 THEN\n"
        "    SRID := %d;\n"
        "  END IF;\n"
        "\n"
        "  SDO_GEOR.georeference( GR1, SRID, :3,"
           " SDO_NUMBER_ARRAY(:4, :5, :6), SDO_NUMBER_ARRAY(:7, :8, :9));\n"
        "\n"
        "  IF SRID = %d THEN\n"
        "    GR1.spatialExtent := NULL;\n"
        "  ELSE\n"
        "    GR1.spatialExtent := SDO_GEOR.generateSpatialExtent( GR1 );\n"
        "  END IF;\n"
        "\n"
        "  UPDATE %s%s T SET %s = GR1 WHERE %s;\n"
        "\n"
        "  COMMIT;\n"
        "END;",
        pszColumn, pszSchema, pszTable, pszWhere,
        CPLSerializeXMLTree( phMetadata ),
        UNKNOWN_CRS,
        UNKNOWN_CRS,
        pszSchema, pszTable, pszColumn, pszWhere );

    OWStatement* poStmt = poConnection->CreateStatement( osStatemtn.c_str() );

    poStmt->Bind( &nSRID );
    poStmt->Bind( &eModel );
    poStmt->Bind( &dfXCoef[0] );
    poStmt->Bind( &dfXCoef[1] );
    poStmt->Bind( &dfXCoef[2] );
    poStmt->Bind( &dfYCoef[0] );
    poStmt->Bind( &dfYCoef[1] );
    poStmt->Bind( &dfYCoef[2] );

    if( ! poStmt->Execute() )
    {
        delete poStmt;
        return false;
    }

    delete poStmt;

    return true;
}

//  ---------------------------------------------------------------------------
//                                                            GeneratePyramid()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::GeneratePyramid( int nLevels,
                                        const char* pszResampling,
                                        bool bNodata )
{
    (void) bNodata;

    OWStatement* poStmt = poConnection->CreateStatement( CPLSPrintf(
        "DECLARE\n"
        "  gr sdo_georaster;\n"
        "BEGIN\n"
        "  SELECT %s INTO gr\n"
        "    FROM %s t WHERE %s FOR UPDATE;\n"
        "  sdo_geor.generatePyramid(gr, 'rlevel=%d resampling=%s');\n"
        "  UPDATE %s t SET %s = gr WHERE %s;\n"
        "END;\n",
        pszColumn,
        pszTable,
        pszWhere,
        nLevels,
        pszResampling,
        pszTable,
        pszColumn,
        pszWhere ) );

    bool bReturn = poStmt->Execute();

    delete poStmt;

    return bReturn;
}

//  ---------------------------------------------------------------------------
//                                                           CreateBitmapMask()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::InitializePyramidLevel( int nLevel,
                                               int nBlockColumns,
                                               int nBlockRows,
                                               int nColumnBlocks,
                                               int nRowBlocks,
                                               int nBandBlocks )
{
    //  -----------------------------------------------------------
    //  Create rows for the bitmap mask
    //  -----------------------------------------------------------

    OWStatement* poStmt = NULL;

    poStmt = poConnection->CreateStatement(
        "DECLARE\n"
        "  W    NUMBER          := :1;\n"
        "  H    NUMBER          := :2;\n"
        "  BB   NUMBER          := :3;\n"
        "  RB   NUMBER          := :4;\n"
        "  CB   NUMBER          := :5;\n"
        "  X    NUMBER          := 0;\n"
        "  Y    NUMBER          := 0;\n"
        "  STM  VARCHAR2(1024)  := '';\n"
        "BEGIN\n"
        "\n"
        "  EXECUTE IMMEDIATE 'DELETE FROM '||:rdt||' \n"
        "    WHERE RASTERID = '||:rid||' AND PYRAMIDLEVEL = '||:lev||' ';\n"
        "\n"
        "  STM := 'INSERT INTO '||:rdt||' VALUES (:1, :lev, :2-1, :3-1, :4-1 ,\n"
        "    SDO_GEOMETRY(2003, NULL, NULL, SDO_ELEM_INFO_ARRAY(1, 1003, 3),\n"
        "    SDO_ORDINATE_ARRAY(:5, :6, :7-1, :8-1)), EMPTY_BLOB() )';\n"
        "\n"
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
        "END;" );

    poStmt->Bind( &nBlockColumns );
    poStmt->Bind( &nBlockRows );
    poStmt->Bind( &nBandBlocks );
    poStmt->Bind( &nRowBlocks );
    poStmt->Bind( &nColumnBlocks );
    poStmt->BindName( (char*) ":rdt", pszDataTable );
    poStmt->BindName( (char*) ":rid", &nRasterId );
    poStmt->BindName( (char*) ":lev", &nLevel );

    if( ! poStmt->Execute() )
    {
        delete poStmt;
        CPLError( CE_Failure, CPLE_AppDefined,
            "Failure to initialize Level %d", nLevel );
        return false;
    }

    return true;
}

//  ---------------------------------------------------------------------------
//                                                                UnpackNBits()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::UnpackNBits( GByte* pabyData )
{
    int nPixCount = nColumnBlockSize * nRowBlockSize * nBandBlockSize;

    if( EQUAL( pszCellDepth, "4BIT" ) )
    {
        for( int ii = nPixCount - 2; ii >= 0; ii -= 2 )
        {
            int k = ii >> 1;
            pabyData[ii+1] = (pabyData[k]     ) & 0xf;
            pabyData[ii]   = (pabyData[k] >> 4) & 0xf;
        }
    }
    else if( EQUAL( pszCellDepth, "2BIT" ) )
    {
        for( int ii = nPixCount - 4; ii >= 0; ii -= 4 )
        {
            int k = ii >> 2;
            pabyData[ii+3] = (pabyData[k]     ) & 0x3;
            pabyData[ii+2] = (pabyData[k] >> 2) & 0x3;
            pabyData[ii+1] = (pabyData[k] >> 4) & 0x3;
            pabyData[ii]   = (pabyData[k] >> 6) & 0x3;
        }
    }
    else
    {
        for( int ii = nPixCount - 1; ii >= 0; ii-- )
        {
            if( ( pabyData[ii>>3] & ( 128 >> (ii & 0x7) ) ) )
                pabyData[ii] = 1;
            else
                pabyData[ii] = 0;
        }
    }
}

//  ---------------------------------------------------------------------------
//                                                                  PackNBits()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::PackNBits( GByte* pabyData )
{
    int nPixCount = nBandBlockSize * nRowBlockSize * nColumnBlockSize;

    GByte* pabyBuffer = (GByte*) VSIMalloc( nPixCount * sizeof(GByte*) );

    if( pabyBuffer == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, "PackNBits" );
        return;
    }

    if( nCellSizeBits == 4 )
    {
        for( int ii = 0; ii < nPixCount - 1; ii += 2 )
        {
            int k = ii >> 1;
            pabyBuffer[k] =
                  ((((GByte *) pabyData)[ii+1] & 0xf)     )
                | ((((GByte *) pabyData)[ii]   & 0xf) << 4);
        }
    }
    else if( nCellSizeBits == 2 )
    {
        for( int ii = 0; ii < nPixCount - 3; ii += 4 )
        {
            int k = ii >> 2;
            pabyBuffer[k] =
                  ((((GByte *) pabyData)[ii+3] & 0x3)     )
                | ((((GByte *) pabyData)[ii+2] & 0x3) << 2)
                | ((((GByte *) pabyData)[ii+1] & 0x3) << 4)
                | ((((GByte *) pabyData)[ii]   & 0x3) << 6);
        }
    }
    else
    {
        for( int ii = 0; ii < nPixCount - 7; ii += 8 )
        {
            int k = ii >> 3;
            pabyBuffer[k] =
                  ((((GByte *) pabyData)[ii+7] & 0x1)     )
                | ((((GByte *) pabyData)[ii+6] & 0x1) << 1)
                | ((((GByte *) pabyData)[ii+5] & 0x1) << 2)
                | ((((GByte *) pabyData)[ii+4] & 0x1) << 3)
                | ((((GByte *) pabyData)[ii+3] & 0x1) << 4)
                | ((((GByte *) pabyData)[ii+2] & 0x1) << 5)
                | ((((GByte *) pabyData)[ii+1] & 0x1) << 6)
                | ((((GByte *) pabyData)[ii]   & 0x1) << 7);
        }
    }

    memcpy( pabyData, pabyBuffer, nPixCount );

    CPLFree( pabyBuffer );
}

//  ---------------------------------------------------------------------------
//                                                             UncompressJpeg()
//  ---------------------------------------------------------------------------

const static int K2Chrominance[64] =
{
    17, 18, 24, 47, 99, 99, 99, 99,
    18, 21, 26, 66, 99, 99, 99, 99,
    24, 26, 56, 99, 99, 99, 99, 99,
    47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99
};

static const int AC_BITS[16] =
{
    0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 119
};

static const int AC_HUFFVAL[256] =
{
      0,   1,   2,   3,  17,   4,   5,  33,  49,   6,  18,
     65,  81,   7,  97, 113,  19,  34,  50, 129,   8,  20,
     66, 145, 161, 177, 193,   9,  35,  51,  82, 240,  21,
     98, 114, 209,  10,  22,  36,  52, 225,  37, 241,  23,
     24,  25,  26,  38,  39,  40,  41,  42,  53,  54,  55,
     56,  57,  58,  67,  68,  69,  70,  71,  72,  73,  74,
     83,  84,  85,  86,  87,  88,  89,  90,  99, 100, 101,
    102, 103, 104, 105, 106, 115, 116, 117, 118, 119, 120,
    121, 122, 130, 131, 132, 133, 134, 135, 136, 137, 138,
    146, 147, 148, 149, 150, 151, 152, 153, 154, 162, 163,
    164, 165, 166, 167, 168, 169, 170, 178, 179, 180, 181,
    182, 183, 184, 185, 186, 194, 195, 196, 197, 198, 199,
    200, 201, 202, 210, 211, 212, 213, 214, 215, 216, 217,
    218, 226, 227, 228, 229, 230, 231, 232, 233, 234, 242,
    243, 244, 245, 246, 247, 248, 249, 250
};

static const int DC_BITS[16] =
{
    0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0
};

static const int DC_HUFFVAL[256] =
{
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
};

/***
 *
 * Load the tables based on the Java's JAI default values.
 *
 * JPEGQTable.K2Chrominance.getScaledInstance()
 * JPEGHuffmanTable.StdACChrominance
 * JPEGHuffmanTable.StdDCChrominance
 *
 ***/

void JPEG_LoadTables( JQUANT_TBL* hquant_tbl_ptr,
                      JHUFF_TBL* huff_ac_ptr,
                      JHUFF_TBL* huff_dc_ptr,
                      unsigned int nQuality )
{
    int i = 0;
    float fscale_factor;

    //  --------------------------------------------------------------------
    //  Scale Quantization table based on quality
    //  --------------------------------------------------------------------

    fscale_factor = (float) jpeg_quality_scaling( nQuality ) / (float) 100.0;

    for ( i = 0; i < 64; i++ )
    {
        UINT16 temp = (UINT16) floor( K2Chrominance[i] * fscale_factor + 0.5 );
        if ( temp <= 0 )
            temp = 1;
        if ( temp > 255 )
            temp = 255;
        hquant_tbl_ptr->quantval[i] = (UINT16) temp;
    }

    //  --------------------------------------------------------------------
    //  Load AC huffman table
    //  --------------------------------------------------------------------

    for ( i = 1; i <= 16; i++ )
    {
        /* counts[i] is number of Huffman codes of length i bits, i=1..16 */
        huff_ac_ptr->bits[i] = (UINT8) AC_BITS[i-1];
    }

    for ( i = 0; i < 256; i++ )
    {
        /* symbols[] is the list of Huffman symbols, in code-length order */
        huff_ac_ptr->huffval[i] = (UINT8) AC_HUFFVAL[i];
    }

    //  --------------------------------------------------------------------
    //  Load DC huffman table
    //  --------------------------------------------------------------------

    for ( i = 1; i <= 16; i++ )
    {
        /* counts[i] is number of Huffman codes of length i bits, i=1..16 */
        huff_dc_ptr->bits[i] = (UINT8) DC_BITS[i-1];
    }

    for ( i = 0; i < 256; i++ )
    {
        /* symbols[] is the list of Huffman symbols, in code-length order */
        huff_dc_ptr->huffval[i] = (UINT8) DC_HUFFVAL[i];
    }
}

void GeoRasterWrapper::UncompressJpeg( unsigned long nInSize )
{
    //  --------------------------------------------------------------------
    //  Load JPEG in a virtual file
    //  --------------------------------------------------------------------

    const char* pszMemFile = CPLSPrintf( "/vsimem/geor_%p.jpg", pabyBlockBuf );

    FILE *fpImage = VSIFOpenL( pszMemFile, "wb" );
    VSIFWriteL( pabyBlockBuf, nInSize, 1, fpImage );
    VSIFCloseL( fpImage );

    fpImage = VSIFOpenL( pszMemFile, "rb" );

    //  --------------------------------------------------------------------
    //  Initialize decompressor
    //  --------------------------------------------------------------------

    if( ! sDInfo.global_state )
    {
        sDInfo.err = jpeg_std_error( &sJErr );
        jpeg_create_decompress( &sDInfo );

        // -----------------------------------------------------------------
        // Load table for abbreviated JPEG-B
        // -----------------------------------------------------------------

        int nComponentsToLoad = -1; /* doesn't load any table */

        if( EQUAL( pszCompressionType, "JPEG-B") )
        {
            nComponentsToLoad = nBandBlockSize;
        }

        for( int n = 0; n < nComponentsToLoad; n++ )
        {
            sDInfo.quant_tbl_ptrs[n] =
                jpeg_alloc_quant_table( (j_common_ptr) &sDInfo );
            sDInfo.ac_huff_tbl_ptrs[n] =
                jpeg_alloc_huff_table( (j_common_ptr) &sDInfo );
            sDInfo.dc_huff_tbl_ptrs[n] =
                jpeg_alloc_huff_table( (j_common_ptr) &sDInfo );
            
            JPEG_LoadTables( sDInfo.quant_tbl_ptrs[n],
                             sDInfo.ac_huff_tbl_ptrs[n],
                             sDInfo.dc_huff_tbl_ptrs[n],
                             nCompressQuality );
        }

    }

    jpeg_vsiio_src( &sDInfo, fpImage );
    jpeg_read_header( &sDInfo, TRUE );

    sDInfo.out_color_space = ( nBandBlockSize == 1 ? JCS_GRAYSCALE : JCS_RGB );

    jpeg_start_decompress( &sDInfo );

    GByte* pabyScanline = pabyBlockBuf;

    for( int iLine = 0; iLine < nRowBlockSize; iLine++ )
    {
        JSAMPLE* ppSamples = (JSAMPLE*) pabyScanline;
        jpeg_read_scanlines( &sDInfo, &ppSamples, 1 );
        pabyScanline += ( nColumnBlockSize * nBandBlockSize );
    }

    jpeg_finish_decompress( &sDInfo );

    VSIFCloseL( fpImage );

    VSIUnlink( pszMemFile );
}

//  ---------------------------------------------------------------------------
//                                                               CompressJpeg()
//  ---------------------------------------------------------------------------

unsigned long GeoRasterWrapper::CompressJpeg( void )
{
    //  --------------------------------------------------------------------
    //  Load JPEG in a virtual file
    //  --------------------------------------------------------------------

    const char* pszMemFile = CPLSPrintf( "/vsimem/geor_%p.jpg", pabyBlockBuf );

    FILE *fpImage = VSIFOpenL( pszMemFile, "wb" );

    bool write_all_tables = TRUE;

    if( EQUAL( pszCompressionType, "JPEG-B") )
    {
        write_all_tables = FALSE;
    }

    //  --------------------------------------------------------------------
    //  Initialize compressor
    //  --------------------------------------------------------------------

    if( ! sCInfo.global_state )
    {
        sCInfo.err = jpeg_std_error( &sJErr );
        jpeg_create_compress( &sCInfo );

        jpeg_vsiio_dest( &sCInfo, fpImage );
        
        sCInfo.image_width = nColumnBlockSize;
        sCInfo.image_height = nRowBlockSize;
        sCInfo.input_components = nBandBlockSize;
        sCInfo.in_color_space = (nBandBlockSize == 1 ? JCS_GRAYSCALE : JCS_RGB);
        jpeg_set_defaults( &sCInfo );
        sCInfo.JFIF_major_version = 1;
        sCInfo.JFIF_minor_version = 2;
        jpeg_set_quality( &sCInfo, nCompressQuality, TRUE );

        // -----------------------------------------------------------------
        // Load table for abbreviated JPEG-B
        // -----------------------------------------------------------------

        int nComponentsToLoad = -1; /* doesn't load any table */

        if( EQUAL( pszCompressionType, "JPEG-B") )
        {
            nComponentsToLoad = nBandBlockSize;
        }

        for( int n = 0; n < nComponentsToLoad; n++ )
        {
            sCInfo.quant_tbl_ptrs[n] =
                jpeg_alloc_quant_table( (j_common_ptr) &sCInfo );
            sCInfo.ac_huff_tbl_ptrs[n] =
                jpeg_alloc_huff_table( (j_common_ptr) &sCInfo );
            sCInfo.dc_huff_tbl_ptrs[n] =
                jpeg_alloc_huff_table( (j_common_ptr) &sCInfo );

            JPEG_LoadTables( sCInfo.quant_tbl_ptrs[n],
                             sCInfo.ac_huff_tbl_ptrs[n],
                             sCInfo.dc_huff_tbl_ptrs[n],
                             nCompressQuality );
        }
    }
    else
    {
        jpeg_vsiio_dest( &sCInfo, fpImage );
    }
    
    jpeg_suppress_tables( &sCInfo, ! write_all_tables );
    jpeg_start_compress( &sCInfo, write_all_tables );
    
    GByte* pabyScanline = pabyBlockBuf;

    for( int iLine = 0; iLine < nRowBlockSize; iLine++ )
    {
        JSAMPLE* ppSamples = (JSAMPLE*) pabyScanline;
        jpeg_write_scanlines( &sCInfo, &ppSamples, 1 );
        pabyScanline += ( nColumnBlockSize * nBandBlockSize );
    }

    jpeg_finish_compress( &sCInfo );

    VSIFCloseL( fpImage );

    fpImage = VSIFOpenL( pszMemFile, "rb" );
    unsigned long nSize = VSIFReadL( pabyBlockBuf2, 1, nBlockBytes, fpImage );
    VSIFCloseL( fpImage );

    VSIUnlink( pszMemFile );

    return nSize;
}

//  ---------------------------------------------------------------------------
//                                                          UncompressDeflate()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::UncompressDeflate( unsigned long nBufferSize )
{
    GByte* pabyBuf = (GByte*) VSIMalloc( nBufferSize );

    if( pabyBuf == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, "UncompressDeflate" );
        return false;
    }

    memcpy( pabyBuf, pabyBlockBuf, nBufferSize );

    // Call ZLib uncompress

    unsigned long nDestLen = nBlockBytes;

    int nRet = uncompress( pabyBlockBuf, &nDestLen, pabyBuf, nBufferSize );

    CPLFree( pabyBuf );

    if( nRet != Z_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "ZLib return code (%d)", nRet );
        return false;
    }

    if( nDestLen != nBlockBytes )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "ZLib decompressed buffer size (%ld) expected (%ld)", nDestLen, nBlockBytes );
        return false;
    }

    return true;
}

//  ---------------------------------------------------------------------------
//                                                            CompressDeflate()
//  ---------------------------------------------------------------------------

unsigned long GeoRasterWrapper::CompressDeflate( void )
{
    unsigned long nLen = ((unsigned long)(nBlockBytes * 1.1)) + 12;

    GByte* pabyBuf = (GByte*) VSIMalloc( nBlockBytes );

    if( pabyBuf == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, "CompressDeflate" );
        return 0;
    }

    memcpy( pabyBuf, pabyBlockBuf, nBlockBytes );

    // Call ZLib compress

    int nRet = compress( pabyBlockBuf2, &nLen, pabyBuf, nBlockBytes );

    CPLFree( pabyBuf );

    if( nRet != Z_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "ZLib return code (%d)", nRet );
        return 0;
    }

    return nLen;
}
