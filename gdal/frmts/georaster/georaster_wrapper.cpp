/******************************************************************************
 *
 * Name:     georaster_wrapper.cpp
 * Project:  Oracle Spatial GeoRaster Driver
 * Purpose:  Implement GeoRasterWrapper methods
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

#include <string.h>

#include "georaster_priv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_minixml.h"

CPL_CVSID("$Id$")

//  ---------------------------------------------------------------------------
//                                                           GeoRasterWrapper()
//  ---------------------------------------------------------------------------

GeoRasterWrapper::GeoRasterWrapper() :
    sPyramidResampling  ( "NN" ),
    sCompressionType    ( "NONE" ),
    sInterleaving       ( "BSQ" )
{
    nRasterId           = -1;
    phMetadata          = nullptr;
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
    nCompressQuality    = 75;
    bGenPyramid         = false;
    nPyramidLevels      = 0;
    pahLocator          = nullptr;
    pabyBlockBuf        = nullptr;
    pabyCompressBuf     = nullptr;
    bIsReferenced       = false;
    poBlockStmt         = nullptr;
    nCacheBlockId       = -1;
    nCurrentLevel       = -1;
    pahLevels           = nullptr;
    nLevelOffset        = 0L;
    bUpdate             = false;
    bInitializeIO       = false;
    bFlushMetadata      = false;
    nSRID               = DEFAULT_CRS;;
    nExtentSRID         = 0;
    bGenSpatialExtent    = false;
    bCreateObjectTable  = false;
    nPyramidMaxLevel    = 0;
    nBlockCount         = 0L;
    nGDALBlockBytes     = 0L;
    sDInfo.global_state = 0;
    sCInfo.global_state = 0;
    bHasBitmapMask      = false;
    nBlockBytes         = 0L;
    bFlushBlock         = false;
    nFlushBlockSize     = 0L;
    bUniqueFound        = false;
    psNoDataList        = nullptr;
    bWriteOnly          = false;
    bBlocking           = true;
    bAutoBlocking       = false;
    eModelCoordLocation = MCL_DEFAULT;
    phRPC               = nullptr;
    poConnection        = nullptr;
    iDefaultRedBand     = 0;
    iDefaultGreenBand   = 0;
    iDefaultBlueBand    = 0;
    anULTCoordinate[0]  = 0;
    anULTCoordinate[1]  = 0;
    anULTCoordinate[2]  = 0;
    pasGCPList          = nullptr;
    nGCPCount           = 0;
    bFlushGCP           = false;
    memset(&sCInfo, 0, sizeof(sCInfo));
    memset(&sDInfo, 0, sizeof(sDInfo));
    memset(&sJErr, 0, sizeof(sJErr));
}

//  ---------------------------------------------------------------------------
//                                                           GeoRasterDataset()
//  ---------------------------------------------------------------------------

GeoRasterWrapper::~GeoRasterWrapper()
{
    FlushMetadata();

    if( pahLocator && nBlockCount )
    {
        OWStatement::Free( pahLocator, static_cast<int>(nBlockCount) );
    }

    CPLFree( pahLocator );
    CPLFree( pabyBlockBuf );
    CPLFree( pabyCompressBuf );
    CPLFree( pahLevels );

    if( bFlushGCP )
    {
        FlushGCP();
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
        pasGCPList = nullptr;
        nGCPCount = 0;
    }

    if( CPLListCount( psNoDataList ) )
    {
        CPLList* psList = nullptr;

        for( psList = psNoDataList; psList ; psList = psList->psNext )
        {
            CPLFree( psList->pData );
        }

        CPLListDestroy( psNoDataList );
    }

    if( poBlockStmt )
    {
        delete poBlockStmt;
    }

    CPLDestroyXMLNode( phMetadata );

    if( sDInfo.global_state )
    {
        jpeg_destroy_decompress( &sDInfo );
    }

    if( sCInfo.global_state )
    {
        jpeg_destroy_compress( &sCInfo );
    }

    if( poConnection )
    {
        delete poConnection;
    }

    if( phRPC )
    {
        CPLFree( phRPC );
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
            papszParam = CSLRemoveStrings( papszParam, 2, 1, nullptr );
        }
        CSLDestroy( papszFirst2 );
    }

    return papszParam;
}

//  ---------------------------------------------------------------------------
//                                                                       Open()
//  ---------------------------------------------------------------------------

GeoRasterWrapper* GeoRasterWrapper::Open( const char* pszStringId, bool bUpdate )
{
    char** papszParam = ParseIdentificator( pszStringId );

    //  ---------------------------------------------------------------
    //  Validate identificator
    //  ---------------------------------------------------------------

    int nArgc = CSLCount( papszParam );

    for( ; nArgc < 3; nArgc++ )
    {
        papszParam = CSLAddString( papszParam, "" );
    }

    //  ---------------------------------------------------------------
    //  Create a GeoRasterWrapper object
    //  ---------------------------------------------------------------

    GeoRasterWrapper* poGRW = new GeoRasterWrapper();

    if( ! poGRW )
    {
        CSLDestroy(papszParam);
        return nullptr;
    }

    poGRW->bUpdate = bUpdate;

    //  ---------------------------------------------------------------
    //  Get a connection with Oracle server
    //  ---------------------------------------------------------------

    if( strlen( papszParam[0] ) == 0 &&
        strlen( papszParam[1] ) == 0 &&
        strlen( papszParam[2] ) == 0 )
    {
        /* In an external procedure environment, before opening any
         * dataset, the caller must pass the with_context as an
         * string metadata item OCI_CONTEXT_PTR to the driver. */

        OCIExtProcContext* with_context = nullptr;

        const char* pszContext = GDALGetMetadataItem(
                                           GDALGetDriverByName("GEORASTER"),
                                          "OCI_CONTEXT_PTR", nullptr );

        if( pszContext )
        {
            sscanf( pszContext, "%p", &with_context );

            poGRW->poConnection = new OWConnection( with_context );
        }
    }
    else
    {
        poGRW->poConnection = new OWConnection( papszParam[0],
                                                papszParam[1],
                                                papszParam[2] );
    }

    if( ! poGRW->poConnection ||
        ! poGRW->poConnection->Succeeded() )
    {
        CSLDestroy( papszParam );
        delete poGRW;
        return nullptr;
    }

    //  -------------------------------------------------------------------
    //  Extract schema name
    //  -------------------------------------------------------------------

    if( poGRW->poConnection->IsExtProc() )
    {
        poGRW->sOwner  = poGRW->poConnection->GetExtProcUser();
        poGRW->sSchema = poGRW->poConnection->GetExtProcSchema();
    }
    else
    {
        char** papszSchema = CSLTokenizeString2( papszParam[3], ".",
                                CSLT_HONOURSTRINGS | CSLT_ALLOWEMPTYTOKENS );

        if( CSLCount( papszSchema ) == 2 )
        {
            poGRW->sOwner  = papszSchema[0];
            poGRW->sSchema = CPLSPrintf( "%s.", poGRW->sOwner.c_str() );

            papszParam = CSLRemoveStrings( papszParam, 3, 1, nullptr );

            if( ! EQUAL( papszSchema[1], "" ) )
            {
                papszParam = CSLInsertString( papszParam, 3, papszSchema[1] );
            }

            nArgc = CSLCount( papszParam );
        }
        else
        {
            poGRW->sSchema = "";
            poGRW->sOwner  = poGRW->poConnection->GetUser();
        }

        CSLDestroy( papszSchema );
    }

    //  -------------------------------------------------------------------
    //  Assign parameters from Identification string
    //  -------------------------------------------------------------------

    switch( nArgc )
    {
    case 6 :
        poGRW->sTable   = papszParam[3];
        poGRW->sColumn  = papszParam[4];
        poGRW->sWhere   = papszParam[5];
        break;
    case 5 :
        if( OWIsNumeric( papszParam[4] ) )
        {
            poGRW->sDataTable   = papszParam[3];
            poGRW->nRasterId    = (long long) CPLAtoGIntBig( papszParam[4]);
            break;
        }
        else
        {
            poGRW->sTable   = papszParam[3];
            poGRW->sColumn  = papszParam[4];
            CSLDestroy(papszParam);
            return poGRW;
        }
    case 4 :
        poGRW->sTable   = papszParam[3];
        CSLDestroy(papszParam);
        return poGRW;
    default :
        CSLDestroy(papszParam);
        return poGRW;
    }

    CSLDestroy( papszParam );

    //  -------------------------------------------------------------------
    //  Query all the basic information at once to reduce round trips
    //  -------------------------------------------------------------------

    char szOwner[OWCODE];
    char szTable[OWCODE];
    char szColumn[OWTEXT];
    char szDataTable[OWCODE];
    char szWhere[OWTEXT];
    long long nRasterId = -1;
    OCILobLocator* phLocator = nullptr;

    szOwner[0]     = '\0';
    szTable[0]     = '\0';
    szColumn[0]    = '\0';
    szDataTable[0] = '\0';
    szWhere[0]     = '\0';

    if( ! poGRW->sOwner.empty() )
    {
      snprintf( szOwner, sizeof(szOwner), "%s", poGRW->sOwner.c_str() );
    }

    if( ! poGRW->sTable.empty() )
    {
      snprintf( szTable, sizeof(szTable), "%s", poGRW->sTable.c_str() );
    }

    if( ! poGRW->sColumn.empty() )
    {
      snprintf( szColumn, sizeof(szColumn), "%s", poGRW->sColumn.c_str() );
    }

    if( ! poGRW->sDataTable.empty() )
    {
      snprintf( szDataTable, sizeof(szDataTable), "%s", poGRW->sDataTable.c_str() );
    }

    nRasterId = poGRW->nRasterId;

    if( ! poGRW->sWhere.empty() )
    {
      snprintf( szWhere, sizeof(szWhere), "%s", poGRW->sWhere.c_str() );
    }

    OWStatement* poStmt = poGRW->poConnection->CreateStatement(
      "BEGIN\n"
      "\n"
      "    IF :datatable IS NOT NULL AND :rasterid IS NOT NULL THEN\n"
      "\n"
      "      EXECUTE IMMEDIATE\n"
      "        'SELECT OWNER, TABLE_NAME, COLUMN_NAME\n"
      "         FROM   ALL_SDO_GEOR_SYSDATA\n"
      "         WHERE  RDT_TABLE_NAME = UPPER(:1)\n"
      "           AND  RASTER_ID = :2'\n"
      "        INTO  :owner, :table, :column\n"
      "        USING :datatable, :rasterid;\n"
      "\n"
      "      EXECUTE IMMEDIATE\n"
      "        'SELECT T.'||:column||'.METADATA.getClobVal()\n"
      "         FROM   '||:owner||'.'||:table||' T\n"
      "         WHERE  T.'||:column||'.RASTERDATATABLE = UPPER(:1)\n"
      "           AND  T.'||:column||'.RASTERID = :2'\n"
      "        INTO  :metadata\n"
      "        USING :datatable, :rasterid;\n"
      "      :counter := 1;\n"
      "\n"
      "    ELSE\n"
      "\n"
      "      EXECUTE IMMEDIATE\n"
      "        'SELECT T.'||:column||'.RASTERDATATABLE,\n"
      "                T.'||:column||'.RASTERID,\n"
      "                T.'||:column||'.METADATA.getClobVal()\n"
      "         FROM  '||:owner||'.'||:table||' T\n"
      "         WHERE '||:where\n"
      "        INTO  :datatable, :rasterid, :metadata;\n"
      "      :counter := 1;\n"
      "\n"
      "    END IF;\n"
      "\n"
      "  EXCEPTION\n"
      "    WHEN no_data_found THEN :counter := 0;\n"
      "    WHEN too_many_rows THEN :counter := 2;\n"
      "END;" );

    int nCounter = 0;

    poStmt->BindName( ":datatable", szDataTable );
    poStmt->BindName( ":rasterid", &nRasterId );
    poStmt->BindName( ":owner", szOwner );
    poStmt->BindName( ":table", szTable );
    poStmt->BindName( ":column", szColumn );
    poStmt->BindName( ":where", szWhere );
    poStmt->BindName( ":counter", &nCounter );
    poStmt->BindName( ":metadata", &phLocator );

    CPLErrorReset();

    if( ! poStmt->Execute() )
    {
        delete poStmt;
        delete poGRW;
        return nullptr;
    }

    if( nCounter < 1 )
    {
        delete poStmt;
        delete poGRW;
        return nullptr;
    }

    poGRW->sSchema  = CPLSPrintf( "%s.", szOwner );
    poGRW->sOwner   = szOwner;
    poGRW->sTable   = szTable;
    poGRW->sColumn  = szColumn;

    if( nCounter == 1 )
    {
        poGRW->bUniqueFound = true;
    }
    else
    {
        poGRW->bUniqueFound = false;

        delete poStmt;
        return poGRW;
    }

    poGRW->sDataTable   = szDataTable;
    poGRW->nRasterId    = nRasterId;
    poGRW->sWhere       = CPLSPrintf(
        "T.%s.RASTERDATATABLE = UPPER('%s') AND T.%s.RASTERID = %lld",
        poGRW->sColumn.c_str(),
        poGRW->sDataTable.c_str(),
        poGRW->sColumn.c_str(),
        poGRW->nRasterId );

    //  -------------------------------------------------------------------
    //  Read Metadata XML in text
    //  -------------------------------------------------------------------

    CPLPushErrorHandler( CPLQuietErrorHandler );

    char* pszXML = poStmt->ReadCLob( phLocator );

    CPLPopErrorHandler();

    if( pszXML )
    {
        //  -----------------------------------------------------------
        //  Get basic information from xml metadata
        //  -----------------------------------------------------------

        poGRW->phMetadata = CPLParseXMLString( pszXML );
        poGRW->GetRasterInfo();
        poGRW->GetSpatialReference();
    }
    else
    {
        poGRW->sDataTable = "";
        poGRW->nRasterId  = 0;
    }

    //  -------------------------------------------------------------------
    //  Clean up
    //  -------------------------------------------------------------------

    poStmt->FreeLob(phLocator);
    CPLFree( pszXML );
    delete poStmt;

    //  -------------------------------------------------------------------
    //  Return a GeoRasterWrapper object
    //  -------------------------------------------------------------------

    return poGRW;
}

//  ---------------------------------------------------------------------------
//                                                                     Create()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::Create( char* pszDescription,
                               char* pszInsert,
                               bool bUpdateIn )
{
    CPLString sValues;
    CPLString sFormat;

    if( sTable.empty() ||
        sColumn.empty() )
    {
        return false;
    }

    //  -------------------------------------------------------------------
    //  Parse RDT/RID from the current szValues
    //  -------------------------------------------------------------------

    char szRDT[OWNAME];
    char szRID[OWCODE];

    if( ! sDataTable.empty() )
    {
        snprintf( szRDT, sizeof(szRDT), "'%s'", sDataTable.c_str() );
    }
    else
    {
        snprintf( szRDT, sizeof(szRDT), "%s",
                  OWParseSDO_GEOR_INIT( sValues.c_str(), 1 ).c_str() );
    }

    if ( nRasterId > 0 )
    {
        snprintf( szRID, sizeof(szRID), "%lld", nRasterId );
    }
    else
    {
        snprintf( szRID, sizeof(szRID), "%s",
                  OWParseSDO_GEOR_INIT( sValues.c_str(), 2 ).c_str() );

        if ( EQUAL( szRID, "" ) )
        {
            strcpy( szRID, "NULL" );
        }
    }

    //  -------------------------------------------------------------------
    //  Description parameters
    //  -------------------------------------------------------------------

    char szDescription[OWTEXT];

    if( bUpdateIn == false )
    {

        if ( pszDescription  )
        {
            snprintf( szDescription, sizeof(szDescription), "%s", pszDescription );
        }
        else
        {
             snprintf( szDescription, sizeof(szDescription),
                "(%s MDSYS.SDO_GEORASTER)", sColumn.c_str() );
        }

        //  ---------------------------------------------------------------
        //  Insert parameters
        //  ---------------------------------------------------------------

        if( pszInsert )
        {
            sValues = pszInsert;

            if( pszInsert[0] == '(' && sValues.ifind( "VALUES" ) == std::string::npos )
            {
                sValues = CPLSPrintf( "VALUES %s", pszInsert );
            }
        }
        else
        {
            sValues = CPLSPrintf( "VALUES (SDO_GEOR.INIT(%s,%s))", szRDT, szRID );
        }
    }

    //  -----------------------------------------------------------
    //  Storage parameters
    //  -----------------------------------------------------------

    nColumnBlockSize = nColumnBlockSize == 0 ? DEFAULT_BLOCK_COLUMNS : nColumnBlockSize;
    nRowBlockSize    = nRowBlockSize    == 0 ? DEFAULT_BLOCK_ROWS    : nRowBlockSize;
    nBandBlockSize   = nBandBlockSize   == 0 ? 1 : nBandBlockSize;

    //  -----------------------------------------------------------
    //  Blocking storage parameters
    //  -----------------------------------------------------------

    CPLString sBlocking;

    if( bBlocking == true )
    {
        if( bAutoBlocking == true )
        {
            int nBlockXSize = nColumnBlockSize;
            int nBlockYSize = nRowBlockSize;
            int nBlockBSize = nBandBlockSize;

            OWStatement* poStmt = poConnection->CreateStatement(
                "DECLARE\n"
                "  dimensionSize    sdo_number_array;\n"
                "  blockSize        sdo_number_array;\n"
                "BEGIN\n"
                "  dimensionSize := sdo_number_array(:1, :2, :3);\n"
                "  blockSize     := sdo_number_array(:4, :5, :6);\n"
                "  sdo_geor_utl.calcOptimizedBlockSize(dimensionSize,blockSize);\n"
                "  :4 := blockSize(1);\n"
                "  :5 := blockSize(2);\n"
                "  :6 := blockSize(3);\n"
                "END;" );

            poStmt->Bind( &nRasterColumns );
            poStmt->Bind( &nRasterRows );
            poStmt->Bind( &nRasterBands );
            poStmt->Bind( &nBlockXSize );
            poStmt->Bind( &nBlockYSize );
            poStmt->Bind( &nBlockBSize );

            if( poStmt->Execute() )
            {
                nColumnBlockSize = nBlockXSize;
                nRowBlockSize = nBlockYSize;
                nBandBlockSize = nBlockBSize;
            }

            delete poStmt;
        }

        if( nRasterBands == 1 )
        {
            sBlocking = CPLSPrintf(
                "blockSize=(%d, %d)",
                nRowBlockSize,
                nColumnBlockSize );
        }
        else
        {
            sBlocking = CPLSPrintf(
                "blockSize=(%d, %d, %d)",
                nRowBlockSize,
                nColumnBlockSize,
                nBandBlockSize );
        }
    }
    else
    {
        sBlocking = "blocking=FALSE";

        nColumnBlockSize = nRasterColumns;
        nRowBlockSize = nRasterRows;
        nBandBlockSize = nRasterBands;
    }

    //  -----------------------------------------------------------
    //  Complete format parameters
    //  -----------------------------------------------------------

    if( poConnection->GetVersion() > 10 )
    {
        if( nRasterBands == 1 )
        {
            sFormat = CPLSPrintf(
                "20001, '"
                "dimSize=(%d,%d) ",
                nRasterRows, nRasterColumns );
        }
        else
        {
            sFormat = CPLSPrintf(
                "21001, '"
                "dimSize=(%d,%d,%d) ",
                nRasterRows, nRasterColumns, nRasterBands );
        }

        if( STARTS_WITH_CI(sCompressionType.c_str(), "JPEG") )
        {
            sFormat.append( CPLSPrintf(
                    "%s "
                    "cellDepth=%s "
                    "interleaving=%s "
                    "compression=%s "
                    "quality=%d'",
                    sBlocking.c_str(),
                    sCellDepth.c_str(),
                    sInterleaving.c_str(),
                    sCompressionType.c_str(),
                    nCompressQuality) );
        }
        else
        {
            sFormat.append( CPLSPrintf(
                    "%s "
                    "cellDepth=%s "
                    "interleaving=%s "
                    "compression=%s'",
                    sBlocking.c_str(),
                    sCellDepth.c_str(),
                    sInterleaving.c_str(),
                    sCompressionType.c_str() ) );
        }
    }
    else
    {
        //  -------------------------------------------------------
        //  For versions 10g or older
        //  -------------------------------------------------------

        sFormat = CPLSPrintf(
            "%s "
            "cellDepth=%s "
            "interleaving=%s "
            "pyramid=FALSE "
            "compression=NONE",
            sBlocking.c_str(),
            sCellDepth.c_str(),
            sInterleaving.c_str() );
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

    if( ! bUpdateIn )
    {
        poStmt = poConnection->CreateStatement( CPLSPrintf(
            "DECLARE\n"
            "  TAB VARCHAR2(68)  := UPPER('%s');\n"
            "  COL VARCHAR2(68)  := UPPER('%s');\n"
            "  OWN VARCHAR2(68)  := UPPER('%s');\n"
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
                sTable.c_str(),
                sColumn.c_str(),
                sOwner.c_str(),
                sSchema.c_str(),
                sTable.c_str(),
                szDescription ) );

        if( ! poStmt->Execute() )
        {
            delete ( poStmt );
            return false;
        }

        delete poStmt;
    }

    //  -----------------------------------------------------------
    //  Prepare UPDATE or INSERT command
    //  -----------------------------------------------------------

    CPLString sCommand;

    if( bUpdateIn )
    {
        sCommand = CPLSPrintf(
            "SELECT %s INTO GR1 FROM %s%s T WHERE %s FOR UPDATE;",
            sColumn.c_str(),
            sSchema.c_str(),
            sTable.c_str(),
            sWhere.c_str() );
    }
    else
    {
        sCommand = CPLSPrintf(
            "INSERT INTO %s%s %s RETURNING %s INTO GR1;",
            sSchema.c_str(),
            sTable.c_str(),
            sValues.c_str(),
            sColumn.c_str() );
    }

    //  -----------------------------------------------------------
    //  Create RTD if needed and insert/update GeoRaster
    //  -----------------------------------------------------------

    char szBindRDT[OWNAME];
    long long nBindRID = 0;
    szBindRDT[0] = '\0';

    CPLString sObjectTable;
    CPLString sSecureFile;

    // For version > 11 create RDT as relational table by default,
    // if it is not specified by create-option OBJECTTABLE=TRUE

    if( poConnection->GetVersion() <= 11 || bCreateObjectTable )
    {
        sObjectTable = "OF MDSYS.SDO_RASTER\n      (";
    }
    else
    {
        sObjectTable = CPLSPrintf("(\n"
                       "      RASTERID           NUMBER,\n"
                       "      PYRAMIDLEVEL       NUMBER,\n"
                       "      BANDBLOCKNUMBER    NUMBER,\n"
                       "      ROWBLOCKNUMBER     NUMBER,\n"
                       "      COLUMNBLOCKNUMBER  NUMBER,\n"
                       "      BLOCKMBR           SDO_GEOMETRY,\n"
                       "      RASTERBLOCK        BLOB,\n"
                       "      CONSTRAINT '||:rdt||'_RDT_PK ");
    }

    // For version >= 11 create RDT rasterBlock as securefile

    if( poConnection->GetVersion() >= 11 )
    {
        sSecureFile = "SECUREFILE(CACHE)";
    }
    else
    {
        sSecureFile = "(NOCACHE NOLOGGING)";
    }

    if( poConnection->GetVersion() > 10 )
    {
        poStmt = poConnection->CreateStatement( CPLSPrintf(
            "DECLARE\n"
            "  TAB  VARCHAR2(68)    := UPPER('%s');\n"
            "  COL  VARCHAR2(68)    := UPPER('%s');\n"
            "  OWN  VARCHAR2(68)    := UPPER('%s');\n"
            "  CNT  NUMBER          := 0;\n"
            "  GR1  SDO_GEORASTER   := NULL;\n"
            "BEGIN\n"
            "\n"
            "  %s\n"
            "\n"
            "  GR1.spatialExtent := NULL;\n"
            "\n"
            "  SELECT GR1.RASTERDATATABLE INTO :rdt FROM DUAL;\n"
            "  SELECT GR1.RASTERID        INTO :rid FROM DUAL;\n"
            "\n"
            "  EXECUTE IMMEDIATE 'SELECT COUNT(*) FROM ALL_OBJECT_TABLES\n"
            "    WHERE TABLE_NAME = :1 AND OWNER = UPPER(:2)'\n"
            "      INTO CNT USING :rdt, OWN;\n"
            "\n"
            "  IF CNT = 0 THEN\n"
            "    EXECUTE IMMEDIATE 'SELECT COUNT(*) FROM ALL_TABLES\n"
            "      WHERE TABLE_NAME = :1 AND OWNER = UPPER(:2)'\n"
            "        INTO CNT USING :rdt, OWN;\n"
            "  END IF;\n"
            "\n"
            "  IF CNT = 0 THEN\n"
            "    EXECUTE IMMEDIATE 'CREATE TABLE %s'||:rdt||' %s"
            "PRIMARY KEY (RASTERID, PYRAMIDLEVEL,\n"
            "      BANDBLOCKNUMBER, ROWBLOCKNUMBER, COLUMNBLOCKNUMBER))\n"
            "      LOB(RASTERBLOCK) STORE AS %s';\n"
            "  END IF;\n"
            "\n"
            "  SDO_GEOR.createTemplate(GR1, %s, null, 'TRUE');\n"
            "\n"
            "  UPDATE %s%s T SET %s = GR1 WHERE\n"
            "    T.%s.RasterDataTable = :rdt AND\n"
            "    T.%s.RasterId = :rid;\n"
            "\n"
            "  EXECUTE IMMEDIATE\n"
            "    'SELECT T.%s.METADATA.getClobVal()\n"
            "     FROM   %s%s T\n"
            "     WHERE  T.%s.RASTERDATATABLE = UPPER(:1)\n"
            "       AND  T.%s.RASTERID = :2'\n"
            "      INTO  :metadata\n"
            "     USING  :rdt, :rid;\n"
            "\n"
            "END;\n",
                sTable.c_str(),
                sColumn.c_str(),
                sOwner.c_str(),
                sCommand.c_str(),
                sSchema.c_str(),
                sObjectTable.c_str(),
                sSecureFile.c_str(),
                sFormat.c_str(),
                sSchema.c_str(),
                sTable.c_str(),
                sColumn.c_str(),
                sColumn.c_str(),
                sColumn.c_str(),
                sColumn.c_str(),
                sSchema.c_str(),
                sTable.c_str(),
                sColumn.c_str(),
                sColumn.c_str() ) );

        OCILobLocator* phLocator = nullptr;

        poStmt->BindName( ":metadata", &phLocator );
        poStmt->BindName( ":rdt", szBindRDT );
        poStmt->BindName( ":rid", &nBindRID );

        CPLErrorReset();

        if( ! poStmt->Execute() )
        {
            delete poStmt;
            return false;
        }

        sDataTable = szBindRDT;
        nRasterId  = nBindRID;

        poStmt->FreeLob(phLocator);

        delete poStmt;

        return true;
    }

    //  -----------------------------------------------------------
    //  Procedure for Server version older than 11
    //  -----------------------------------------------------------

    char szCreateBlank[OWTEXT];

    if( nRasterBands == 1 )
    {
        snprintf( szCreateBlank, sizeof(szCreateBlank),
            "SDO_GEOR.createBlank(20001, "
            "SDO_NUMBER_ARRAY(0, 0), "
            "SDO_NUMBER_ARRAY(%d, %d), 0, :rdt, :rid)",
            nRasterRows, nRasterColumns );
    }
    else
    {
        snprintf( szCreateBlank, sizeof(szCreateBlank),
            "SDO_GEOR.createBlank(21001, "
            "SDO_NUMBER_ARRAY(0, 0, 0), "
            "SDO_NUMBER_ARRAY(%d, %d, %d), 0, :rdt, :rid)",
            nRasterRows, nRasterColumns, nRasterBands );
    }

    poStmt = poConnection->CreateStatement( CPLSPrintf(
        "DECLARE\n"
        "  W    NUMBER          := :1;\n"
        "  H    NUMBER          := :2;\n"
        "  BB   NUMBER          := :3;\n"
        "  RB   NUMBER          := :4;\n"
        "  CB   NUMBER          := :5;\n"
        "  OWN  VARCHAR2(68)    := UPPER('%s');\n"
        "  X    NUMBER          := 0;\n"
        "  Y    NUMBER          := 0;\n"
        "  CNT  NUMBER          := 0;\n"
        "  GR1  SDO_GEORASTER   := NULL;\n"
        "  GR2  SDO_GEORASTER   := NULL;\n"
        "  STM  VARCHAR2(1024)  := '';\n"
        "BEGIN\n"
        "\n"
        "  %s\n"
        "\n"
        "  SELECT GR1.RASTERDATATABLE INTO :rdt FROM DUAL;\n"
        "  SELECT GR1.RASTERID        INTO :rid FROM DUAL;\n"
        "\n"
        "  SELECT %s INTO GR2 FROM %s%s T WHERE"
        " T.%s.RasterDataTable = :rdt AND"
        " T.%s.RasterId = :rid FOR UPDATE;\n"
        "\n"
        "  GR1 := %s;\n"
        "\n"
        "  SDO_GEOR.changeFormatCopy(GR1, '%s', GR2);\n"
        "\n"
        "  UPDATE %s%s T SET %s = GR2 WHERE"
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
        "\n"
        "  SDO_GEOR.georeference(GR1, %d, %d,"
        " SDO_NUMBER_ARRAY(1.0, 0.0, 0.0),"
        " SDO_NUMBER_ARRAY(0.0, 1.0, 0.0));\n"
        "\n"
        "  UPDATE %s%s T SET %s = GR1 WHERE"
        " T.%s.RasterDataTable = :rdt AND"
        " T.%s.RasterId = :rid;\n"
        "\n"
        "END;",
            sOwner.c_str(),
            sCommand.c_str(),
            sColumn.c_str(), sSchema.c_str(), sTable.c_str(),
            sColumn.c_str(), sColumn.c_str(),
            szCreateBlank,
            sFormat.c_str(),
            sSchema.c_str(), sTable.c_str(),
            sColumn.c_str(), sColumn.c_str(), sColumn.c_str(),
            sSchema.c_str(), sSchema.c_str(), sSchema.c_str(),
            DEFAULT_CRS, MCL_DEFAULT,
            sSchema.c_str(), sTable.c_str(),
            sColumn.c_str(), sColumn.c_str(), sColumn.c_str() ) );

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
        return false;
    }

    sDataTable = szBindRDT;
    nRasterId  = nBindRID;

    delete poStmt;

    return true;
}

//  ---------------------------------------------------------------------------
//                                                         PrepareToOverwrite()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::PrepareToOverwrite( void )
{
    nTotalColumnBlocks  = 0;
    nTotalRowBlocks     = 0;
    nTotalBandBlocks    = 0;
    if( sscanf( sCellDepth.c_str(), "%dBIT", &nCellSizeBits ) )
    {
        nGDALCellBytes   = GDALGetDataTypeSize(
                          OWGetDataType( sCellDepth.c_str() ) ) / 8;
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
    sCompressionType    = "NONE";
    nCompressQuality    = 75;
    bGenPyramid         = false;
    nPyramidLevels      = 0;
    sPyramidResampling  = "NN";
    bIsReferenced       = false;
    nCacheBlockId       = -1;
    nCurrentLevel       = -1;
    pahLevels           = nullptr;
    nLevelOffset        = 0L;
    sInterleaving       = "BSQ";
    bUpdate             = false;
    bInitializeIO       = false;
    bFlushMetadata      = false;
    nSRID               = DEFAULT_CRS;;
    nExtentSRID         = 0;
    bGenSpatialExtent    = false;
    bCreateObjectTable  = false;
    nPyramidMaxLevel    = 0;
    nBlockCount         = 0L;
    sDInfo.global_state = 0;
    sCInfo.global_state = 0;
    bHasBitmapMask      = false;
    bWriteOnly          = false;
    bBlocking           = true;
    bAutoBlocking       = false;
    eModelCoordLocation = MCL_DEFAULT;
    bFlushBlock         = false;
    nFlushBlockSize     = 0L;
    phRPC               = nullptr;
}

//  ---------------------------------------------------------------------------
//                                                                     Delete()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::Delete( void )
{
    if( ! bUniqueFound )
    {
        return false;
    }

    OWStatement* poStmt = poConnection->CreateStatement( CPLSPrintf(
      "UPDATE %s%s T SET %s = NULL WHERE %s\n",
            sSchema.c_str(),
            sTable.c_str(),
            sColumn.c_str(),
            sWhere.c_str() ) );

    bool bReturn = poStmt->Execute();

    delete poStmt;

    return bReturn;
}

//  ---------------------------------------------------------------------------
//                                                            SetGeoReference()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::SetGeoReference( long long nSRIDIn )
{
    nSRID = nSRIDIn;

    bIsReferenced = true;

    bFlushMetadata = true;
}

//  ---------------------------------------------------------------------------
//                                                              GetRasterInfo()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::GetRasterInfo( void )
{
    //  -------------------------------------------------------------------
    //  Get dimensions
    //  -------------------------------------------------------------------

    int nCount      = atoi( CPLGetXMLValue( phMetadata,
                  "rasterInfo.totalDimensions", "0" ) );
    CPLXMLNode* phDimSize   = CPLGetXMLNode( phMetadata, "rasterInfo.dimensionSize" );

    int i = 0;

    for( i = 0; i < nCount; i++ )
    {
        const char* pszType = CPLGetXMLValue( phDimSize, "type", "0" );

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
    //  Load NoData Values
    //  -------------------------------------------------------------------

    LoadNoDataValues();

    //  -------------------------------------------------------------------
    //  Get ULTCoordinate values
    //  -------------------------------------------------------------------

    anULTCoordinate[0] = atoi(CPLGetXMLValue(
            phMetadata, "rasterInfo.ULTCoordinate.column", "0"));

    anULTCoordinate[1] = atoi(CPLGetXMLValue(
            phMetadata, "rasterInfo.ULTCoordinate.row", "0"));

    anULTCoordinate[2] = atoi(CPLGetXMLValue(
            phMetadata, "rasterInfo.ULTCoordinate.band", "0"));

    //  -------------------------------------------------------------------
    //  Get Interleaving mode
    //  -------------------------------------------------------------------

    sInterleaving = CPLGetXMLValue( phMetadata,
        "rasterInfo.interleaving", "BSQ" );

    //  -------------------------------------------------------------------
    //  Get blocking
    //  -------------------------------------------------------------------

    nRowBlockSize       = atoi( CPLGetXMLValue( phMetadata,
                            "rasterInfo.blocking.rowBlockSize",
                            CPLSPrintf( "%d", nRasterRows ) ) );

    nColumnBlockSize    = atoi( CPLGetXMLValue( phMetadata,
                            "rasterInfo.blocking.columnBlockSize",
                            CPLSPrintf( "%d", nRasterColumns ) ) );

    nBandBlockSize      = atoi( CPLGetXMLValue( phMetadata,
                            "rasterInfo.blocking.bandBlockSize",
                            CPLSPrintf( "%d", nRasterBands ) ) );

    nTotalColumnBlocks  = atoi( CPLGetXMLValue( phMetadata,
                            "rasterInfo.blocking.totalColumnBlocks","1") );

    nTotalRowBlocks     = atoi( CPLGetXMLValue( phMetadata,
                            "rasterInfo.blocking.totalRowBlocks", "1" ) );

    nTotalBandBlocks    = atoi( CPLGetXMLValue( phMetadata,
                            "rasterInfo.blocking.totalBandBlocks", "1" ) );

    if( nBandBlockSize == -1 )
    {
       nBandBlockSize = nRasterBands;
    }

    //  -------------------------------------------------------------------
    //  Get data type
    //  -------------------------------------------------------------------

    sCellDepth = CPLGetXMLValue( phMetadata, "rasterInfo.cellDepth", "8BIT_U" );

    if( sscanf( sCellDepth.c_str(), "%dBIT", &nCellSizeBits ) )
    {
        nGDALCellBytes   = GDALGetDataTypeSize(
            OWGetDataType( sCellDepth.c_str() ) ) / 8;
    }
    else
    {
        nGDALCellBytes   = 1;
    }

    sCompressionType  = CPLGetXMLValue( phMetadata,
        "rasterInfo.compression.type", "NONE" );

    if( STARTS_WITH_CI(sCompressionType.c_str(), "JPEG") )
    {
        nCompressQuality = atoi( CPLGetXMLValue( phMetadata,
                            "rasterInfo.compression.quality", "75" ) );
    }

    if( STARTS_WITH_CI(sCompressionType.c_str(), "JPEG") )
    {
        sInterleaving = "BIP";
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

    snprintf( szPyramidType, sizeof(szPyramidType), "%s",
              CPLGetXMLValue( phMetadata,
                            "rasterInfo.pyramid.type", "None" ) );

    if( EQUAL( szPyramidType, "DECREASE" ) )
    {
        nPyramidMaxLevel = atoi( CPLGetXMLValue( phMetadata,
                            "rasterInfo.pyramid.maxLevel", "0" ) );
    }

    //  -------------------------------------------------------------------
    //  Check for RPCs
    //  -------------------------------------------------------------------

    CPLXMLNode* phModelType = CPLGetXMLNode( phMetadata,
        "spatialReferenceInfo.modelType");

    for( int n = 1 ; phModelType ; phModelType = phModelType->psNext, n++ )
    {
        const char* pszModelType = CPLGetXMLValue( phModelType, ".", "None" );

        if( EQUAL( pszModelType, "StoredFunction" ) )
        {
            GetGCP();
        }

        if( EQUAL( pszModelType, "FunctionalFitting" ) )
        {
            GetRPC();
        }
    }

    //  -------------------------------------------------------------------
    //  Prepare to get Spatial reference system
    //  -------------------------------------------------------------------

    bIsReferenced       = EQUAL( "TRUE", CPLGetXMLValue( phMetadata,
                            "spatialReferenceInfo.isReferenced", "FALSE" ) );

    nSRID               = (long long) CPLAtoGIntBig( CPLGetXMLValue( phMetadata,
                            "spatialReferenceInfo.SRID", "0" ) );

    if( bIsReferenced == false || nSRID == 0 || nSRID == UNKNOWN_CRS )
    {
        return;
    }

}

//  ---------------------------------------------------------------------------
//                                                                QueryWKText()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::QueryWKText()
{
    char* pszWKText = (char*) VSI_MALLOC2_VERBOSE( sizeof(char), 3 * OWTEXT);
    char* pszAuthority = (char*) VSI_MALLOC2_VERBOSE( sizeof(char), OWTEXT);

    OWStatement* poStmt = poConnection->CreateStatement(
        "select wktext, auth_name from mdsys.cs_srs "
        "where srid = :1 and wktext is not null" );

    poStmt->Bind( &nSRID );
    poStmt->Define( pszWKText, 3 * OWTEXT );
    poStmt->Define( pszAuthority, OWTEXT );

    if( poStmt->Execute() )
    {
        sWKText = pszWKText;
        sAuthority = pszAuthority;
    }

    CPLFree( pszWKText );
    CPLFree( pszAuthority );

    delete poStmt;
}

//  ---------------------------------------------------------------------------
//                                                              GetStatistics()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::GetStatistics( int nBand,
                                      char* pszMin,
                                      char* pszMax,
                                      char* pszMean,
                                      char* pszMedian,
                                      char* pszMode,
                                      char* pszStdDev,
                                      char* pszSampling )
{
    int n = 1;

    CPLXMLNode *phSubLayer = CPLGetXMLNode( phMetadata, "layerInfo.subLayer" );

    for( n = 1 ; phSubLayer ; phSubLayer = phSubLayer->psNext, n++ )
    {
        if( n == nBand && CPLGetXMLNode( phSubLayer, "statisticDataset" ) )
        {
            strncpy( pszSampling, CPLGetXMLValue( phSubLayer,
                "statisticDataset.samplingFactor",  "0.0" ), MAX_DOUBLE_STR_REP );
            strncpy( pszMin, CPLGetXMLValue( phSubLayer,
                "statisticDataset.MIN",  "0.0" ), MAX_DOUBLE_STR_REP );
            strncpy( pszMax, CPLGetXMLValue( phSubLayer,
                "statisticDataset.MAX",  "0.0" ), MAX_DOUBLE_STR_REP );
            strncpy( pszMean, CPLGetXMLValue( phSubLayer,
                "statisticDataset.MEAN", "0.0" ), MAX_DOUBLE_STR_REP );
            strncpy( pszMedian, CPLGetXMLValue( phSubLayer,
                "statisticDataset.MEDIAN", "0.0" ), MAX_DOUBLE_STR_REP );
            strncpy( pszMode, CPLGetXMLValue( phSubLayer,
                "statisticDataset.MODEVALUE", "0.0" ), MAX_DOUBLE_STR_REP );
            strncpy( pszStdDev, CPLGetXMLValue( phSubLayer,
                "statisticDataset.STD",  "0.0" ), MAX_DOUBLE_STR_REP );
            return true;
        }
    }
    return false;
}

//  ---------------------------------------------------------------------------
//                                                              SetStatistics()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::SetStatistics( int nBand,
                                      const char* pszMin,
                                      const char* pszMax,
                                      const char* pszMean,
                                      const char* pszMedian,
                                      const char* pszMode,
                                      const char* pszStdDev,
                                      const char* pszSampling )
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

        if( psSDaset != nullptr )
        {
            CPLRemoveXMLChild( phSubLayer, psSDaset );
            CPLDestroyXMLNode( psSDaset );
        }

        psSDaset = CPLCreateXMLNode(phSubLayer,CXT_Element,"statisticDataset");

        CPLCreateXMLElementAndValue(psSDaset,"samplingFactor", pszSampling );
        CPLCreateXMLElementAndValue(psSDaset,"MIN", pszMin );
        CPLCreateXMLElementAndValue(psSDaset,"MAX", pszMax );
        CPLCreateXMLElementAndValue(psSDaset,"MEAN", pszMean );
        CPLCreateXMLElementAndValue(psSDaset,"MEDIAN", pszMedian );
        CPLCreateXMLElementAndValue(psSDaset,"MODEVALUE", pszMode );
        CPLCreateXMLElementAndValue(psSDaset,"STD", pszStdDev );

        return true;
    }
    return false;
}

//  ---------------------------------------------------------------------------
//                                                              HasColorTable()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::HasColorMap( int nBand )
{
    int n = 1;

    CPLXMLNode *psLayers = CPLGetXMLNode( phMetadata, "layerInfo.subLayer" );

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

        if( psSLayer == nullptr )
        {
            psSLayer = CPLCreateXMLNode( pslInfo, CXT_Element, "subLayer" );

            CPLCreateXMLElementAndValue( psSLayer, "layerNumber",
                CPLSPrintf( "%d", n + 1 ) );

            CPLCreateXMLElementAndValue( psSLayer, "layerDimensionOrdinate",
                CPLSPrintf( "%d", n ) );

            CPLCreateXMLElementAndValue( psSLayer, "layerID", "" );
        }
    }
}

//  ---------------------------------------------------------------------------
//                                                              GetColorTable()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::GetColorMap( int nBand, GDALColorTable* poCT )
{
    GDALColorEntry oEntry;

    int n = 1;

    CPLXMLNode* psLayers = CPLGetXMLNode( phMetadata, "layerInfo.subLayer" );

    for( ; psLayers; psLayers = psLayers->psNext, n++ )
    {
        if( n != nBand )
        {
            continue;
        }

        CPLXMLNode* psColors = CPLGetXMLNode( psLayers, "colorMap.colors.cell" );

        for(  ; psColors; psColors = psColors->psNext )
        {
            const int iColor    = (short) atoi( CPLGetXMLValue( psColors, "value","0"));
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

        if( psCMap != nullptr )
        {
            CPLRemoveXMLChild( phSubLayer, psCMap );
            CPLDestroyXMLNode( psCMap );
        }

        psCMap = CPLCreateXMLNode( phSubLayer, CXT_Element, "colorMap" );

        CPLXMLNode* psColor = CPLCreateXMLNode( psCMap, CXT_Element, "colors" );

        // ------------------------------------------------
        // Clean existing colors entry (RGB color table)
        // ------------------------------------------------

        if( psColor != nullptr )
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
    }
}

//  ---------------------------------------------------------------------------
//                                                               InitializeIO()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::InitializeIO( void )
{
    bInitializeIO = true;

    // --------------------------------------------------------------------
    // Initialize Pyramid level details
    // --------------------------------------------------------------------

    pahLevels = (hLevelDetails*) CPLCalloc( sizeof(hLevelDetails),
                                            nPyramidMaxLevel + 1 );

    // --------------------------------------------------------------------
    // Calculate number and size of the blocks in level zero
    // --------------------------------------------------------------------

    nBlockCount = (unsigned long) ( nTotalColumnBlocks * nTotalRowBlocks * nTotalBandBlocks );
    nBlockBytes = (unsigned long) ( nColumnBlockSize * nRowBlockSize * nBandBlockSize *
                                    nCellSizeBits / 8L );
    nGDALBlockBytes = (unsigned long) ( nColumnBlockSize * nRowBlockSize * nGDALCellBytes );

    pahLevels[0].nColumnBlockSize   = nColumnBlockSize;
    pahLevels[0].nRowBlockSize      = nRowBlockSize;
    pahLevels[0].nTotalColumnBlocks = nTotalColumnBlocks;
    pahLevels[0].nTotalRowBlocks    = nTotalRowBlocks;
    pahLevels[0].nBlockCount        = nBlockCount;
    pahLevels[0].nBlockBytes        = nBlockBytes;
    pahLevels[0].nGDALBlockBytes    = nGDALBlockBytes;
    pahLevels[0].nOffset            = 0L;

    // --------------------------------------------------------------------
    // Calculate number and size of the blocks in Pyramid levels
    // --------------------------------------------------------------------

    int iLevel = 1;

    for( iLevel = 1; iLevel <= nPyramidMaxLevel; iLevel++ )
    {
        int nRBS = nRowBlockSize;
        int nCBS = nColumnBlockSize;
        int nTCB;
        int nTRB;

        // --------------------------------------------------------------------
        // Calculate the actual size of a lower resolution block
        // --------------------------------------------------------------------

        double dfScale = pow( (double) 2.0, (double) iLevel );

        int nXSize  = (int) floor( (double) nRasterColumns / dfScale );
        int nYSize  = (int) floor( (double) nRasterRows / dfScale );
        int nXBlock = (int) floor( (double) nCBS / 2.0 );
        int nYBlock = (int) floor( (double) nRBS / 2.0 );

        if( nXSize <= nXBlock && nYSize <= nYBlock )
        {
            // ------------------------------------------------------------
            // Calculate the size of the single small blocks
            // ------------------------------------------------------------

            nCBS = nXSize;
            nRBS = nYSize;
            nTCB = 1;
            nTRB = 1;
        }
        else
        {
            // ------------------------------------------------------------
            // Recalculate blocks quantity
            // ------------------------------------------------------------

            nTCB = (int) ceil( (double) nXSize / nCBS );
            nTRB = (int) ceil( (double) nYSize / nRBS );
        }

        // --------------------------------------------------------------------
        // Save level datails
        // --------------------------------------------------------------------

        pahLevels[iLevel].nColumnBlockSize   = nCBS;
        pahLevels[iLevel].nRowBlockSize      = nRBS;
        pahLevels[iLevel].nTotalColumnBlocks = nTCB;
        pahLevels[iLevel].nTotalRowBlocks    = nTRB;
        pahLevels[iLevel].nBlockCount        = (unsigned long ) ( nTCB * nTRB * nTotalBandBlocks );
        pahLevels[iLevel].nBlockBytes        = (unsigned long ) ( nCBS * nRBS * nBandBlockSize *
                                                                  nCellSizeBits / 8L );
        pahLevels[iLevel].nGDALBlockBytes    = (unsigned long ) ( nCBS * nRBS * nGDALCellBytes );
        pahLevels[iLevel].nOffset            = 0L;
    }

    // --------------------------------------------------------------------
    // Calculate total row count and level's offsets
    // --------------------------------------------------------------------

    nBlockCount = 0L;

    for( iLevel = 0; iLevel <= nPyramidMaxLevel; iLevel++ )
    {
        pahLevels[iLevel].nOffset = nBlockCount;
        nBlockCount += pahLevels[iLevel].nBlockCount;
    }

    // --------------------------------------------------------------------
    // Allocate buffer for one raster block
    // --------------------------------------------------------------------

    long nMaxBufferSize = MAX( nBlockBytes, nGDALBlockBytes );

    pabyBlockBuf = (GByte*) VSI_MALLOC_VERBOSE( nMaxBufferSize );

    if ( pabyBlockBuf == nullptr )
    {
        return false;
    }

    // --------------------------------------------------------------------
    // Allocate buffer for one compressed raster block
    // --------------------------------------------------------------------

    if( bUpdate && ! EQUAL( sCompressionType.c_str(), "NONE") )
    {
        pabyCompressBuf = (GByte*) VSI_MALLOC_VERBOSE( nMaxBufferSize );

        if ( pabyCompressBuf == nullptr )
        {
            return false;
        }
    }

    // --------------------------------------------------------------------
    // Allocate array of LOB Locators
    // --------------------------------------------------------------------

    pahLocator = (OCILobLocator**) VSI_MALLOC_VERBOSE( sizeof(void*) * nBlockCount );

    if ( pahLocator == nullptr )
    {
        return false;
    }

    //  --------------------------------------------------------------------
    //  Issue a statement to load the locators
    //  --------------------------------------------------------------------

    const char* pszUpdate = bUpdate ? "\nFOR UPDATE" : "";

    delete poBlockStmt;
    poBlockStmt = poConnection->CreateStatement( CPLSPrintf(
        "SELECT RASTERBLOCK\n"
        "FROM   %s%s\n"
        "WHERE  RASTERID = :1\n"
        "ORDER BY\n"
        "       PYRAMIDLEVEL ASC,\n"
        "       BANDBLOCKNUMBER ASC,\n"
        "       ROWBLOCKNUMBER ASC,\n"
        "       COLUMNBLOCKNUMBER ASC%s",
        sSchema.c_str(),
        sDataTable.c_str(),
        pszUpdate ) );

    poBlockStmt->Bind( &nRasterId );
    poBlockStmt->Define( pahLocator, nBlockCount );

    if( ! poBlockStmt->Execute( static_cast<int>(nBlockCount) ) )
    {
        return false;
    }

    return true;
}

//  ---------------------------------------------------------------------------
//                                                            InitializeLevel()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::InitializeLevel( int nLevel )
{
    nCurrentLevel       = nLevel;
    nColumnBlockSize    = pahLevels[nLevel].nColumnBlockSize;
    nRowBlockSize       = pahLevels[nLevel].nRowBlockSize;
    nTotalColumnBlocks  = pahLevels[nLevel].nTotalColumnBlocks;
    nTotalRowBlocks     = pahLevels[nLevel].nTotalRowBlocks;
    nBlockBytes         = pahLevels[nLevel].nBlockBytes;
    nGDALBlockBytes     = pahLevels[nLevel].nGDALBlockBytes;
    nLevelOffset        = pahLevels[nLevel].nOffset;
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
    if( ! bInitializeIO )
    {
        if( InitializeIO() == false )
        {
            return false;
        }
    }

    if( nCurrentLevel != nLevel && nLevel != DEFAULT_BMP_MASK )
    {
        InitializeLevel( nLevel );
    }

    long nBlock = GetBlockNumber( nBand, nXOffset, nYOffset );

    CPLDebug( "Read  ",
              "Block = %4ld Size = %7ld Band = %d Level = %d X = %d Y = %d",
              nBlock, nBlockBytes, nBand, nLevel, nXOffset, nYOffset );

    if( nCacheBlockId != nBlock )
    {
        if ( bFlushBlock )
        {
            if( ! FlushBlock( nCacheBlockId ) )
            {
                return false;
            }
        }

        nCacheBlockId = nBlock;

        unsigned long nBytesRead = 0;

        nBytesRead = poBlockStmt->ReadBlob( pahLocator[nBlock],
                                            pabyBlockBuf,
                                            nBlockBytes );

        CPLDebug( "Load  ", "Block = %4ld Size = %7ld", nBlock, nBlockBytes );

        if( nBytesRead == 0 )
        {
            memset( pData, 0, nGDALBlockBytes );
            return true;
        }

        if( nBytesRead < nBlockBytes &&
            EQUAL( sCompressionType.c_str(), "NONE") )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                "BLOB size (%ld) is smaller than expected (%ld) !",
                nBytesRead,  nBlockBytes );
            memset( pData, 0, nGDALBlockBytes );
            return true;
        }

        if( nBytesRead > nBlockBytes )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                "BLOB size (%ld) is bigger than expected (%ld) !",
                nBytesRead,  nBlockBytes );
            memset( pData, 0, nGDALBlockBytes );
            return true;
        }

        //  ----------------------------------------------------------------
        //  GeoRaster BLOB is always Big endian
        //  ----------------------------------------------------------------

#ifndef CPL_MSB
        if( nCellSizeBits > 8 &&
            EQUAL( sCompressionType.c_str(), "DEFLATE") == false )
        {
            int nWordSize  = nCellSizeBits / 8;
            int nWordCount = nColumnBlockSize * nRowBlockSize * nBandBlockSize;
            GDALSwapWords( pabyBlockBuf, nWordSize, nWordCount, nWordSize );
        }
#endif

        //  ----------------------------------------------------------------
        //  Uncompress
        //  ----------------------------------------------------------------

        if( STARTS_WITH_CI(sCompressionType.c_str(), "JPEG") )
        {
            UncompressJpeg( nBytesRead );
        }
        else if ( EQUAL( sCompressionType.c_str(), "DEFLATE" ) )
        {
            UncompressDeflate( nBytesRead );
#ifndef CPL_MSB
            if( nCellSizeBits > 8 )
            {
                int nWordSize  = nCellSizeBits / 8;
                int nWordCount = nColumnBlockSize * nRowBlockSize * nBandBlockSize;
                GDALSwapWords( pabyBlockBuf, nWordSize, nWordCount, nWordSize );
            }
#endif
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
    //  Uninterleaving, extract band from block buffer
    //  --------------------------------------------------------------------

    int nStart = ( nBand - 1 ) % nBandBlockSize;

    if( EQUAL( sInterleaving.c_str(), "BSQ" ) || nBandBlockSize == 1 )
    {
        nStart *= nGDALBlockBytes;

        memcpy( pData, &pabyBlockBuf[nStart], nGDALBlockBytes );
    }
    else
    {
        int nIncr   = nBandBlockSize * nGDALCellBytes;
        int nSize   = nGDALCellBytes;

        if( EQUAL( sInterleaving.c_str(), "BIL" ) )
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

    if( ! bInitializeIO )
    {
        if( InitializeIO() == false )
        {
            return false;
        }
    }

    if( nCurrentLevel != nLevel )
    {
        InitializeLevel( nLevel );
    }

    long nBlock = GetBlockNumber( nBand, nXOffset, nYOffset );

    CPLDebug( "Write ",
              "Block = %4ld Size = %7ld Band = %d Level = %d X = %d Y = %d",
              nBlock, nBlockBytes, nBand, nLevel, nXOffset, nYOffset );

    //  --------------------------------------------------------------------
    //  Flush previous block
    //  --------------------------------------------------------------------

    if( nCacheBlockId != nBlock && bFlushBlock )
    {
        if( ! FlushBlock( nCacheBlockId ) )
        {
            return false;
        }
    }

    //  --------------------------------------------------------------------
    //  Re-load interleaved block
    //  --------------------------------------------------------------------

    if( nBandBlockSize > 1 && bWriteOnly == false && nCacheBlockId != nBlock )
    {
        nCacheBlockId = nBlock;

        unsigned long nBytesRead = 0;

        nBytesRead = poBlockStmt->ReadBlob( pahLocator[nBlock],
                                            pabyBlockBuf,
                                            static_cast<unsigned long>(nBlockBytes) );

        CPLDebug( "Reload", "Block = %4ld Size = %7ld", nBlock, nBlockBytes );

        if( nBytesRead == 0 )
        {
            memset( pabyBlockBuf, 0, nBlockBytes );
        }
        else
        {
            //  ------------------------------------------------------------
            //  Uncompress
            //  ------------------------------------------------------------

            if( STARTS_WITH_CI(sCompressionType.c_str(), "JPEG") )
            {
                UncompressJpeg( nBytesRead );
            }
            else if ( EQUAL( sCompressionType.c_str(), "DEFLATE" ) )
            {
                UncompressDeflate( nBytesRead );
            }

            //  ------------------------------------------------------------
            //  Unpack NBits
            //  ------------------------------------------------------------

            if( nCellSizeBits < 8 || nLevel == DEFAULT_BMP_MASK )
            {
                UnpackNBits( pabyBlockBuf );
            }
        }
    }

    GByte *pabyInBuf = (GByte *) pData;

    //  --------------------------------------------------------------------
    //  Interleave
    //  --------------------------------------------------------------------

    int nStart = ( nBand - 1 ) % nBandBlockSize;

    if( EQUAL( sInterleaving.c_str(), "BSQ" ) || nBandBlockSize == 1 )
    {
        nStart *= nGDALBlockBytes;

        memcpy( &pabyBlockBuf[nStart], pabyInBuf, nGDALBlockBytes );
    }
    else
    {
        int nIncr   = nBandBlockSize * nGDALCellBytes;
        int nSize   = nGDALCellBytes;

        if( EQUAL( sInterleaving.c_str(), "BIL" ) )
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
    //  Flag the flush block
    //  --------------------------------------------------------------------

    nCacheBlockId   = nBlock;
    bFlushBlock     = true;
    nFlushBlockSize = nBlockBytes;

    return true;
}

//  ---------------------------------------------------------------------------
//                                                                 FlushBlock()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::FlushBlock( long nCacheBlock )
{
    GByte* pabyFlushBuffer = (GByte *) pabyBlockBuf;

    //  --------------------------------------------------------------------
    //  Pack bits ( inside pabyOutBuf )
    //  --------------------------------------------------------------------

    if( nCellSizeBits < 8 || nCurrentLevel == DEFAULT_BMP_MASK )
    {
        PackNBits( pabyFlushBuffer );
    }

    //  --------------------------------------------------------------------
    //  Compress ( from pabyBlockBuf to pabyBlockBuf2 )
    //  --------------------------------------------------------------------

    if( ! EQUAL( sCompressionType.c_str(), "NONE" ) )
    {
        if( STARTS_WITH_CI(sCompressionType.c_str(), "JPEG") )
        {
            nFlushBlockSize = CompressJpeg();
        }
        else if ( EQUAL( sCompressionType.c_str(), "DEFLATE" ) )
        {
            nFlushBlockSize = CompressDeflate();
        }

        pabyFlushBuffer = pabyCompressBuf;
    }

    //  --------------------------------------------------------------------
    //  Write BLOB
    //  --------------------------------------------------------------------

    CPLDebug( "Flush ", "Block = %4ld Size = %7ld", nCacheBlock,
              nFlushBlockSize );

    if( ! poBlockStmt->WriteBlob( pahLocator[nCacheBlock],
                                  pabyFlushBuffer,
                                  nFlushBlockSize ) )
    {
        return false;
    }

    bFlushBlock     = false;
    bFlushMetadata  = true;
    nFlushBlockSize = nBlockBytes;

    return true;
}

//  ---------------------------------------------------------------------------
//                                                           LoadNoDataValues()
//  ---------------------------------------------------------------------------
static
CPLList* AddToNoDataList( CPLXMLNode* phNode, int nNumber, CPLList* poList )
{
    CPLXMLNode* psChild = phNode->psChild;

    const char* pszMin = nullptr;
    const char* pszMax = nullptr;

    for( ; psChild ; psChild = psChild->psNext )
    {
        if( EQUAL( psChild->pszValue, "value" ) )
        {
            pszMin = CPLGetXMLValue( psChild, nullptr, "NONE" );
            pszMax = pszMin;
        }
        else if ( EQUAL( psChild->pszValue, "range" ) )
        {
            pszMin = CPLGetXMLValue( psChild, "min", "NONE" );
            pszMax = CPLGetXMLValue( psChild, "max", "NONE" );
        }
        else
        {
            continue;
        }

        hNoDataItem* poItem = (hNoDataItem*) CPLMalloc( sizeof( hNoDataItem ) );

        poItem->nBand = nNumber;
        poItem->dfLower = CPLAtof( pszMin );
        poItem->dfUpper = CPLAtof( pszMax );

        poList = CPLListAppend( poList, poItem );
    }

    return poList;
}

void GeoRasterWrapper::LoadNoDataValues( void )
{
    if( psNoDataList )
        return;

    CPLXMLNode* phLayerInfo = CPLGetXMLNode( phMetadata, "layerInfo" );

    if( phLayerInfo == nullptr )
    {
        return;
    }

    //  -------------------------------------------------------------------
    //  Load NoData from list of values and list of value ranges
    //  -------------------------------------------------------------------

    CPLXMLNode* phObjNoData = CPLGetXMLNode( phLayerInfo, "objectLayer.NODATA" );

    for( ; phObjNoData ; phObjNoData = phObjNoData->psNext )
    {
        psNoDataList = AddToNoDataList( phObjNoData, 0, psNoDataList );
    }

    CPLXMLNode* phSubLayer = CPLGetXMLNode( phLayerInfo, "subLayer" );

    for( ; phSubLayer ; phSubLayer = phSubLayer->psNext )
    {
        int nNumber = atoi( CPLGetXMLValue( phSubLayer, "layerNumber", "-1") );

        CPLXMLNode* phSubNoData = CPLGetXMLNode( phSubLayer, "NODATA" );

        if( phSubNoData )
        {
            psNoDataList = AddToNoDataList( phSubNoData, nNumber, psNoDataList );
        }
    }
}

//  ---------------------------------------------------------------------------
//                                                        GetSpatialReference()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::GetSpatialReference()
{
    int i;

    CPLXMLNode* phSRSInfo = CPLGetXMLNode( phMetadata, "spatialReferenceInfo" );

    if( phSRSInfo == nullptr )
    {
        return;
    }

    const char* pszMCL = CPLGetXMLValue( phSRSInfo, "modelCoordinateLocation",
                                                    "CENTER" );

    if( EQUAL( pszMCL, "CENTER" ) )
    {
      eModelCoordLocation = MCL_CENTER;
    }
    else
    {
      eModelCoordLocation = MCL_UPPERLEFT;
    }

    const char* pszModelType = CPLGetXMLValue( phSRSInfo, "modelType", "None" );

    if( EQUAL( pszModelType, "FunctionalFitting" ) == false )
    {
        return;
    }

    CPLXMLNode* phPolyModel = CPLGetXMLNode( phSRSInfo, "polynomialModel" );

    if ( phPolyModel == nullptr )
    {
        return;
    }

    CPLXMLNode* phPolynomial = CPLGetXMLNode( phPolyModel, "pPolynomial" );

    if ( phPolynomial == nullptr )
    {
        return;
    }

    int nNumCoeff = atoi( CPLGetXMLValue( phPolynomial, "nCoefficients", "0" ));

    if ( nNumCoeff != 3 )
    {
        return;
    }

    const char* pszPolyCoeff = CPLGetXMLValue(phPolynomial,
                                              "polynomialCoefficients", "None");

    if ( EQUAL( pszPolyCoeff, "None" ) )
    {
        return;
    }

    char** papszCeoff = CSLTokenizeString2( pszPolyCoeff, " ",
                                           CSLT_STRIPLEADSPACES );

    if( CSLCount( papszCeoff ) < 3 )
    {
        CSLDestroy(papszCeoff);
        return;
    }

    double adfPCoef[3];

    for( i = 0; i < 3; i++ )
    {
        adfPCoef[i] = CPLAtof( papszCeoff[i] );
    }
    CSLDestroy(papszCeoff);

    phPolynomial = CPLGetXMLNode( phPolyModel, "rPolynomial" );

    if ( phPolynomial == nullptr )
    {
        return;
    }

    pszPolyCoeff = CPLGetXMLValue( phPolynomial, "polynomialCoefficients", "None" );

    if ( EQUAL( pszPolyCoeff, "None" ) )
    {
        return;
    }

    papszCeoff = CSLTokenizeString2( pszPolyCoeff, " ", CSLT_STRIPLEADSPACES );

    if( CSLCount( papszCeoff ) < 3 )
    {
        CSLDestroy(papszCeoff);
        return;
    }

    double adfRCoef[3];

    for( i = 0; i < 3; i++ )
    {
        adfRCoef[i] = CPLAtof( papszCeoff[i] );
    }
    CSLDestroy(papszCeoff);

    //  -------------------------------------------------------------------
    //  Inverse the transformation matrix
    //  -------------------------------------------------------------------

    double adfVal[6] = {1.0, 0.0, 0.0, 1.0, 0.0, 0.0};

    double dfDet = adfRCoef[1] * adfPCoef[2] - adfRCoef[2] * adfPCoef[1];

    if( CPLIsEqual( dfDet, 0.0 ) )
    {
        dfDet = 0.0000000001; // to avoid divide by zero
        CPLError( CE_Warning, CPLE_AppDefined, "Determinant is ZERO!!!");
    }

    adfVal[0] =   adfPCoef[2] / dfDet;
    adfVal[1] =  -adfRCoef[2] / dfDet;
    adfVal[2] = -(adfRCoef[0] * adfPCoef[2] - adfPCoef[0] * adfRCoef[2]) / dfDet;
    adfVal[3] =  -adfPCoef[1] / dfDet;
    adfVal[4] =   adfRCoef[1] / dfDet;
    adfVal[5] =  (adfRCoef[0] * adfPCoef[1] - adfPCoef[0] * adfRCoef[1]) / dfDet;

    //  -------------------------------------------------------------------
    //  Adjust Model Coordinate Location
    //  -------------------------------------------------------------------

    if ( eModelCoordLocation == MCL_CENTER )
    {
       adfVal[2] -= ( adfVal[0] / 2.0 );
       adfVal[5] -= ( adfVal[4] / 2.0 );
    }

    dfXCoefficient[0] = adfVal[0];
    dfXCoefficient[1] = adfVal[1];
    dfXCoefficient[2] = adfVal[2];
    dfYCoefficient[0] = adfVal[3];
    dfYCoefficient[1] = adfVal[4];
    dfYCoefficient[2] = adfVal[5];

    //  -------------------------------------------------------------------
    //  Apply ULTCoordinate
    //  -------------------------------------------------------------------

    dfXCoefficient[2] += ( anULTCoordinate[0] * dfXCoefficient[0] );

    dfYCoefficient[2] += ( anULTCoordinate[1] * dfYCoefficient[1] );
}

//  ---------------------------------------------------------------------------
//                                                                     GetGCP()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::GetGCP()
{
    CPLXMLNode* psGCP = CPLGetXMLNode( phMetadata,
                "spatialReferenceInfo.gcpGeoreferenceModel.gcp" );

    CPLXMLNode* psFirst = psGCP;

    if( nGCPCount > 0 && pasGCPList )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }

    pasGCPList = nullptr;
    nGCPCount = 0;

    for( int i = 1 ; psGCP ; psGCP = psGCP->psNext, i++ )
    {
        if( ! EQUAL( CPLGetXMLValue( psGCP, "type",  "" ), "ControlPoint" ) )
        {
            continue;
        }
        nGCPCount++;
    }

    pasGCPList = static_cast<GDAL_GCP *>(
            CPLCalloc(sizeof(GDAL_GCP), nGCPCount) );

    psGCP = psFirst;

    for( int i = 0 ; psGCP ; psGCP = psGCP->psNext, i++ )
    {
        if( ! EQUAL( CPLGetXMLValue( psGCP, "type",  "" ), "ControlPoint" ) )
        {
            continue;
        }
        pasGCPList[i].pszId    = CPLStrdup( CPLGetXMLValue( psGCP, "ID", "" ) );
        pasGCPList[i].pszInfo  = CPLStrdup( "" );
        pasGCPList[i].dfGCPLine  = CPLAtof( CPLGetXMLValue( psGCP, "row", "0.0" ) );
        pasGCPList[i].dfGCPPixel = CPLAtof( CPLGetXMLValue( psGCP, "column", "0.0" ) );
        pasGCPList[i].dfGCPX     = CPLAtof( CPLGetXMLValue( psGCP, "X", "0.0" ) );
        pasGCPList[i].dfGCPY     = CPLAtof( CPLGetXMLValue( psGCP, "Y", "0.0" ) );
        pasGCPList[i].dfGCPZ     = CPLAtof( CPLGetXMLValue( psGCP, "Z", "0.0" ));
    }
}

//  ---------------------------------------------------------------------------
//                                                                     SetGCP()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::SetGCP( int nGCPCountIn, const GDAL_GCP *pasGCPListIn)
{
    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }

    nGCPCount  = nGCPCountIn;
    pasGCPList = GDALDuplicateGCPs(nGCPCount, pasGCPListIn);
    bFlushGCP  = true;
}

void GeoRasterWrapper::FlushGCP()
{
    const char* pszStatement = CPLSPrintf(
        "DECLARE\n"
        "  gr   sdo_georaster;\n"
        "BEGIN\n"
        "  SELECT %s INTO gr FROM %s%s t WHERE %s FOR UPDATE;\n",
            sColumn.c_str(),
            sSchema.c_str(),
            sTable.c_str(),
            sWhere.c_str());

    int nDimns = 2;

    for( int iGCP = 0; iGCP < nGCPCount; ++iGCP )
    {
        if( pasGCPList[iGCP].dfGCPZ != 0.0 )
        {
            nDimns = 3;
            break;
        }
    }

    if ( nDimns == 3 )
    {
        pszStatement = CPLSPrintf(
            "%s"
            "  sdo_geor.setControlPoint(gr,\n"
            "    SDO_GEOR_GCP(TO_CHAR(:1), '', 1,\n"
            "      2, sdo_number_array(:2, :3),\n"
            "      3, sdo_number_array(:4, :5, :6),\n"
            "      NULL, NULL));\n",
            pszStatement);
    }
    else
    {
        pszStatement = CPLSPrintf(
            "%s"
            "  sdo_geor.setControlPoint(gr,\n"
            "    SDO_GEOR_GCP(TO_CHAR(:1), '', 1,\n"
            "      2, sdo_number_array(:2, :3),\n"
            "      2, sdo_number_array(:4, :5),\n"
            "      NULL, NULL));\n",
            pszStatement);
    }

    pszStatement = CPLSPrintf(
            "%s"
            "  UPDATE %s%s t SET %s = gr WHERE %s;\n"
            "END;\n",
            pszStatement,
            sSchema.c_str(),
            sTable.c_str(),
            sColumn.c_str(),
            sWhere.c_str());

    OWStatement* poStmt = poConnection->CreateStatement( pszStatement );

    long   lBindN = 0L;
    double dBindL = 0.0;
    double dBindP = 0.0;
    double dBindX = 0.0;
    double dBindY = 0.0;
    double dBindZ = 0.0;

    poStmt->Bind( &lBindN );
    poStmt->Bind( &dBindL );
    poStmt->Bind( &dBindP );
    poStmt->Bind( &dBindX );
    poStmt->Bind( &dBindY );

    if ( nDimns == 3 )
    {
        poStmt->Bind( &dBindZ );
    }

    for( int iGCP = 0; iGCP < nGCPCount; ++iGCP )
    {

        // Assign bound variables

        lBindN = iGCP + 1;
        dBindL = pasGCPList[iGCP].dfGCPLine;
        dBindP = pasGCPList[iGCP].dfGCPPixel;
        dBindX = pasGCPList[iGCP].dfGCPX;
        dBindY = pasGCPList[iGCP].dfGCPY;
        dBindZ = nDimns == 3 ? pasGCPList[iGCP].dfGCPZ : 0.0;

        // To avoid cppcheck false positive complaints about unreadVariable
        CPL_IGNORE_RET_VAL(lBindN);
        CPL_IGNORE_RET_VAL(dBindL);
        CPL_IGNORE_RET_VAL(dBindP);
        CPL_IGNORE_RET_VAL(dBindX);
        CPL_IGNORE_RET_VAL(dBindY);
        CPL_IGNORE_RET_VAL(dBindZ);

        // Consume bound values

        if( ! poStmt->Execute() )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Error loading GCP.");
        }
    }

    delete poStmt;
}

//  ---------------------------------------------------------------------------
//                                                                     GetRPC()
//  ---------------------------------------------------------------------------

/* This is the order for storing 20 coefficients in GeoRaster Metadata */

constexpr int anOrder[] = {
    1, 2, 8, 12, 3, 5, 15, 9, 13, 16, 4, 6, 18, 7, 11, 19, 10, 14, 17, 20
};

void GeoRasterWrapper::GetRPC()
{
    int i;

    CPLXMLNode* phSRSInfo = CPLGetXMLNode( phMetadata,
                                           "spatialReferenceInfo" );

    if( phSRSInfo == nullptr )
    {
        return;
    }

    CPLXMLNode* phPolyModel = CPLGetXMLNode( phSRSInfo, "polynomialModel" );

    if ( phPolyModel == nullptr )
    {
        return;
    }

    // pPolynomial refers to LINE_NUM

    CPLXMLNode* phPolynomial = CPLGetXMLNode( phPolyModel, "pPolynomial" );

    if ( phPolynomial == nullptr )
    {
        return;
    }

    int nNumCoeff = atoi( CPLGetXMLValue( phPolynomial, "nCoefficients", "0" ) );

    if ( nNumCoeff != 20 )
    {
        return;
    }

    const char* pszPolyCoeff = CPLGetXMLValue( phPolynomial, "polynomialCoefficients", "None" );

    if ( EQUAL( pszPolyCoeff, "None" ) )
    {
        return;
    }

    char** papszCeoff = CSLTokenizeString2( pszPolyCoeff, " ", CSLT_STRIPLEADSPACES );

    if( CSLCount( papszCeoff ) != 20 )
    {
        CSLDestroy(papszCeoff);
        return;
    }

    phRPC = (GDALRPCInfoV2*) VSIMalloc( sizeof(GDALRPCInfoV2) );

    phRPC->dfERR_BIAS = -1.0;
    phRPC->dfERR_RAND = -1.0;
    phRPC->dfLINE_OFF     = CPLAtof( CPLGetXMLValue( phPolyModel, "rowOff", "0" ) );
    phRPC->dfSAMP_OFF     = CPLAtof( CPLGetXMLValue( phPolyModel, "columnOff", "0" ) );
    phRPC->dfLONG_OFF     = CPLAtof( CPLGetXMLValue( phPolyModel, "xOff", "0" ) );
    phRPC->dfLAT_OFF      = CPLAtof( CPLGetXMLValue( phPolyModel, "yOff", "0" ) );
    phRPC->dfHEIGHT_OFF   = CPLAtof( CPLGetXMLValue( phPolyModel, "zOff", "0" ) );

    phRPC->dfLINE_SCALE   = CPLAtof( CPLGetXMLValue( phPolyModel, "rowScale", "0" ) );
    phRPC->dfSAMP_SCALE   = CPLAtof( CPLGetXMLValue( phPolyModel, "columnScale", "0" ) );
    phRPC->dfLONG_SCALE   = CPLAtof( CPLGetXMLValue( phPolyModel, "xScale", "0" ) );
    phRPC->dfLAT_SCALE    = CPLAtof( CPLGetXMLValue( phPolyModel, "yScale", "0" ) );
    phRPC->dfHEIGHT_SCALE = CPLAtof( CPLGetXMLValue( phPolyModel, "zScale", "0" ) );

    for( i = 0; i < 20; i++ )
    {
        phRPC->adfLINE_NUM_COEFF[anOrder[i] - 1] = CPLAtof( papszCeoff[i] );
    }
    CSLDestroy(papszCeoff);

    // qPolynomial refers to LINE_DEN

    phPolynomial = CPLGetXMLNode( phPolyModel, "qPolynomial" );

    if ( phPolynomial == nullptr )
    {
        CPLFree( phRPC );
        phRPC = nullptr;
        return;
    }

    pszPolyCoeff = CPLGetXMLValue( phPolynomial, "polynomialCoefficients", "None" );

    if ( EQUAL( pszPolyCoeff, "None" ) )
    {
        CPLFree( phRPC );
        phRPC = nullptr;
        return;
    }

    papszCeoff = CSLTokenizeString2( pszPolyCoeff, " ", CSLT_STRIPLEADSPACES );

    if( CSLCount( papszCeoff ) != 20 )
    {
        CPLFree( phRPC );
        phRPC = nullptr;
        CSLDestroy(papszCeoff);
        return;
    }

    for( i = 0; i < 20; i++ )
    {
        phRPC->adfLINE_DEN_COEFF[anOrder[i] - 1] = CPLAtof( papszCeoff[i] );
    }
    CSLDestroy(papszCeoff);

    // rPolynomial refers to SAMP_NUM

    phPolynomial = CPLGetXMLNode( phPolyModel, "rPolynomial" );

    if ( phPolynomial == nullptr )
    {
        CPLFree( phRPC );
        phRPC = nullptr;
        return;
    }

    pszPolyCoeff = CPLGetXMLValue( phPolynomial, "polynomialCoefficients", "None" );

    if ( EQUAL( pszPolyCoeff, "None" ) )
    {
        CPLFree( phRPC );
        phRPC = nullptr;
        return;
    }

    papszCeoff = CSLTokenizeString2( pszPolyCoeff, " ", CSLT_STRIPLEADSPACES );

    if( CSLCount( papszCeoff ) != 20 )
    {
        CPLFree( phRPC );
        phRPC = nullptr;
        CSLDestroy(papszCeoff);
        return;
    }

    for( i = 0; i < 20; i++ )
    {
        phRPC->adfSAMP_NUM_COEFF[anOrder[i] - 1] = CPLAtof( papszCeoff[i] );
    }
    CSLDestroy(papszCeoff);

    // sPolynomial refers to SAMP_DEN

    phPolynomial = CPLGetXMLNode( phPolyModel, "sPolynomial" );

    if ( phPolynomial == nullptr )
    {
        CPLFree( phRPC );
        phRPC = nullptr;
        return;
    }

    pszPolyCoeff = CPLGetXMLValue( phPolynomial, "polynomialCoefficients", "None" );

    if ( EQUAL( pszPolyCoeff, "None" ) )
    {
        CPLFree( phRPC );
        phRPC = nullptr;
        return;
    }

    papszCeoff = CSLTokenizeString2( pszPolyCoeff, " ", CSLT_STRIPLEADSPACES );

    if( CSLCount( papszCeoff ) != 20 )
    {
        CPLFree( phRPC );
        phRPC = nullptr;
        CSLDestroy(papszCeoff);
        return;
    }

    for( i = 0; i < 20; i++ )
    {
        phRPC->adfSAMP_DEN_COEFF[anOrder[i] - 1] = CPLAtof( papszCeoff[i] );
    }
    CSLDestroy(papszCeoff);
}

//  ---------------------------------------------------------------------------
//                                                                     SetRPC()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::SetRPC()
{
    //  -------------------------------------------------------------------
    //  Remove "layerInfo" tree
    //  -------------------------------------------------------------------

    CPLXMLNode* phLayerInfo = CPLGetXMLNode( phMetadata, "layerInfo" );
    CPLXMLNode* phClone = nullptr;

    if( phLayerInfo )
    {
        phClone = CPLCloneXMLTree( phLayerInfo );
        CPLRemoveXMLChild( phMetadata, phLayerInfo );
    }

    //  -------------------------------------------------------------------
    //  Start loading the RPC to "spatialReferenceInfo" tree
    //  -------------------------------------------------------------------

    int i = 0;
    CPLString osField, osMultiField;
    CPLXMLNode* phPolynomial = nullptr;

    CPLXMLNode* phSRSInfo = CPLGetXMLNode( phMetadata,
                                           "spatialReferenceInfo" );

    if( ! phSRSInfo )
    {
        phSRSInfo = CPLCreateXMLNode( phMetadata, CXT_Element,
                                      "spatialReferenceInfo" );
    }
    else
    {
        CPLXMLNode* phNode = CPLGetXMLNode( phSRSInfo, "isReferenced" );
        if( phNode )
        {
            CPLRemoveXMLChild( phSRSInfo, phNode );
        }

        phNode = CPLGetXMLNode( phSRSInfo, "SRID" );
        if( phNode )
        {
            CPLRemoveXMLChild( phSRSInfo, phNode );
        }

        phNode = CPLGetXMLNode( phSRSInfo, "modelCoordinateLocation" );
        if( phNode )
        {
            CPLRemoveXMLChild( phSRSInfo, phNode );
        }

        phNode = CPLGetXMLNode( phSRSInfo, "modelType" );
        if( phNode )
        {
            CPLRemoveXMLChild( phSRSInfo, phNode );
        }

        phNode = CPLGetXMLNode( phSRSInfo, "polynomialModel" );
        if( phNode )
        {
            CPLRemoveXMLChild( phSRSInfo, phNode );
        }
    }

    CPLCreateXMLElementAndValue( phSRSInfo, "isReferenced", "true" );
    CPLCreateXMLElementAndValue( phSRSInfo, "SRID", "4327" );
    CPLCreateXMLElementAndValue( phSRSInfo, "modelCoordinateLocation",
                                            "CENTER" );
    CPLCreateXMLElementAndValue( phSRSInfo, "modelType", "FunctionalFitting" );
    CPLSetXMLValue( phSRSInfo, "polynomialModel.#rowOff",
                                    CPLSPrintf( "%.15g", phRPC->dfLINE_OFF ) );
    CPLSetXMLValue( phSRSInfo, "polynomialModel.#columnOff",
                                    CPLSPrintf( "%.15g", phRPC->dfSAMP_OFF ) );
    CPLSetXMLValue( phSRSInfo, "polynomialModel.#xOff",
                                    CPLSPrintf( "%.15g", phRPC->dfLONG_OFF ) );
    CPLSetXMLValue( phSRSInfo, "polynomialModel.#yOff",
                                    CPLSPrintf( "%.15g", phRPC->dfLAT_OFF ) );
    CPLSetXMLValue( phSRSInfo, "polynomialModel.#zOff",
                                    CPLSPrintf( "%.15g", phRPC->dfHEIGHT_OFF ) );
    CPLSetXMLValue( phSRSInfo, "polynomialModel.#rowScale",
                                    CPLSPrintf( "%.15g", phRPC->dfLINE_SCALE ) );
    CPLSetXMLValue( phSRSInfo, "polynomialModel.#columnScale",
                                    CPLSPrintf( "%.15g", phRPC->dfSAMP_SCALE ) );
    CPLSetXMLValue( phSRSInfo, "polynomialModel.#xScale",
                                    CPLSPrintf( "%.15g", phRPC->dfLONG_SCALE ) );
    CPLSetXMLValue( phSRSInfo, "polynomialModel.#yScale",
                                    CPLSPrintf( "%.15g", phRPC->dfLAT_SCALE ) );
    CPLSetXMLValue( phSRSInfo, "polynomialModel.#zScale",
                                    CPLSPrintf( "%.15g", phRPC->dfHEIGHT_SCALE ) );
    CPLXMLNode*     phPloyModel = CPLGetXMLNode( phSRSInfo, "polynomialModel" );

    // pPolynomial refers to LINE_NUM

    CPLSetXMLValue( phPloyModel, "pPolynomial.#pType",         "1" );
    CPLSetXMLValue( phPloyModel, "pPolynomial.#nVars",         "3" );
    CPLSetXMLValue( phPloyModel, "pPolynomial.#order",         "3" );
    CPLSetXMLValue( phPloyModel, "pPolynomial.#nCoefficients", "20" );
    for( i = 0; i < 20; i++ )
    {
        osField.Printf( "%.15g", phRPC->adfLINE_NUM_COEFF[anOrder[i] - 1] );
        if( i > 0 )
            osMultiField += " ";
        else
            osMultiField = "";
        osMultiField += osField;
    }
    phPolynomial = CPLGetXMLNode( phPloyModel, "pPolynomial" );
    CPLCreateXMLElementAndValue( phPolynomial, "polynomialCoefficients",
                                 osMultiField );

    // qPolynomial refers to LINE_DEN

    CPLSetXMLValue( phPloyModel, "qPolynomial.#pType",         "1" );
    CPLSetXMLValue( phPloyModel, "qPolynomial.#nVars",         "3" );
    CPLSetXMLValue( phPloyModel, "qPolynomial.#order",         "3" );
    CPLSetXMLValue( phPloyModel, "qPolynomial.#nCoefficients", "20" );
    for( i = 0; i < 20; i++ )
    {
        osField.Printf( "%.15g", phRPC->adfLINE_DEN_COEFF[anOrder[i] - 1] );
        if( i > 0 )
            osMultiField += " ";
        else
            osMultiField = "";
        osMultiField += osField;
    }
    phPolynomial = CPLGetXMLNode( phPloyModel, "qPolynomial" );
    CPLCreateXMLElementAndValue( phPolynomial, "polynomialCoefficients",
                                 osMultiField );

    // rPolynomial refers to SAMP_NUM

    CPLSetXMLValue( phPloyModel, "rPolynomial.#pType",         "1" );
    CPLSetXMLValue( phPloyModel, "rPolynomial.#nVars",         "3" );
    CPLSetXMLValue( phPloyModel, "rPolynomial.#order",         "3" );
    CPLSetXMLValue( phPloyModel, "rPolynomial.#nCoefficients", "20" );
    for( i = 0; i < 20; i++ )
    {
        osField.Printf( "%.15g", phRPC->adfSAMP_NUM_COEFF[anOrder[i] - 1] );
        if( i > 0 )
            osMultiField += " ";
        else
            osMultiField = "";
        osMultiField += osField;
    }
    phPolynomial = CPLGetXMLNode( phPloyModel, "rPolynomial" );
    CPLCreateXMLElementAndValue( phPolynomial, "polynomialCoefficients",
                                 osMultiField );

    // sPolynomial refers to SAMP_DEN

    CPLSetXMLValue( phPloyModel, "sPolynomial.#pType",         "1" );
    CPLSetXMLValue( phPloyModel, "sPolynomial.#nVars",         "3" );
    CPLSetXMLValue( phPloyModel, "sPolynomial.#order",         "3" );
    CPLSetXMLValue( phPloyModel, "sPolynomial.#nCoefficients", "20" );
    for( i = 0; i < 20; i++ )
    {
        osField.Printf( "%.15g", phRPC->adfSAMP_DEN_COEFF[anOrder[i] - 1] );
        if( i > 0 )
            osMultiField += " ";
        else
            osMultiField = "";
        osMultiField += osField;
    }
    phPolynomial = CPLGetXMLNode( phPloyModel, "sPolynomial" );
    CPLCreateXMLElementAndValue( phPolynomial, "polynomialCoefficients",
                                 osMultiField );

    //  -------------------------------------------------------------------
    //  Add "layerInfo" tree back
    //  -------------------------------------------------------------------

    if( phClone )
    {
        CPLAddXMLChild( phMetadata, phClone );
    }
}

//  ---------------------------------------------------------------------------
//                                                                 SetMaxLeve()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::SetMaxLevel( int nLevels )
{
    CPLXMLNode* psNode = CPLGetXMLNode( phMetadata, "rasterInfo.pyramid" );

    if( psNode )
    {
        CPLSetXMLValue( psNode, "type", "DECREASE" );
        CPLSetXMLValue( psNode, "maxLevel", CPLSPrintf( "%d", nLevels ) );
    }
}

//  ---------------------------------------------------------------------------
//                                                                  GetNoData()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::GetNoData( int nLayer, double* pdfNoDataValue )
{
    if( psNoDataList == nullptr || CPLListCount( psNoDataList ) == 0 )
    {
        return false;
    }

    //  -------------------------------------------------------------------
    //  Get only single value NoData, no list of values or value ranges
    //  -------------------------------------------------------------------

    int nCount = 0;
    double dfValue = 0.0;

    CPLList* psList = nullptr;

    //  -------------------------------------------------------------------
    //  Process Object Layer values
    //  -------------------------------------------------------------------

    for( psList = psNoDataList; psList ; psList = psList->psNext )
    {
        hNoDataItem* phItem = (hNoDataItem*) psList->pData;

        if( phItem->nBand == 0 )
        {
            if( phItem->dfLower == phItem->dfUpper )
            {
                dfValue = phItem->dfLower;
                nCount++;
            }
            else
            {
                return false; // value range
            }
        }
    }

    //  -------------------------------------------------------------------
    //  Values from the Object Layer override values from the layers
    //  -------------------------------------------------------------------

    if( nCount == 1 )
    {
        *pdfNoDataValue = dfValue;
        return true;
    }

    //  -------------------------------------------------------------------
    //  Process SubLayer values
    //  -------------------------------------------------------------------

    for( psList = psNoDataList; psList ; psList = psList->psNext )
    {
        hNoDataItem* phItem = (hNoDataItem*) psList->pData;

        if( phItem->nBand == nLayer )
        {
            if( phItem->dfLower == phItem->dfUpper )
            {
                dfValue = phItem->dfLower;
                nCount++;
            }
            else
            {
                return false; // value range
            }
        }
    }

    if( nCount == 1 )
    {
        *pdfNoDataValue = dfValue;
        return true;
    }

    return false;
}

//  ---------------------------------------------------------------------------
//                                                             SetNoDataValue()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::SetNoData( int nLayer, const char* pszValue )
{
    // ------------------------------------------------------------
    //  Set one NoData per dataset on "rasterInfo" section (10g)
    // ------------------------------------------------------------

    if( poConnection->GetVersion() < 11 )
    {
        CPLXMLNode* psRInfo = CPLGetXMLNode( phMetadata, "rasterInfo" );
        CPLXMLNode* psNData = CPLSearchXMLNode( psRInfo, "NODATA" );

        if( psNData == nullptr )
        {
            psNData = CPLCreateXMLNode( nullptr, CXT_Element, "NODATA" );

            CPLXMLNode* psCDepth = CPLGetXMLNode( psRInfo, "cellDepth" );

            CPLXMLNode* psPointer = psCDepth->psNext;
            psCDepth->psNext = psNData;
            psNData->psNext = psPointer;
        }

        CPLSetXMLValue( psRInfo, "NODATA", pszValue );
        bFlushMetadata = true;
        return true;
    }

    // ------------------------------------------------------------
    //  Add NoData for all bands (layer=0) or for a specific band
    // ------------------------------------------------------------

    char szRDT[OWCODE];
    char szNoData[OWTEXT];

    snprintf( szRDT, sizeof(szRDT), "%s", sDataTable.c_str() );
    snprintf( szNoData, sizeof(szNoData), "%s", pszValue );

    long long  nRID = nRasterId;

    // ------------------------------------------------------------
    //  Write the in memory XML metadata to avoid losing other changes.
    // ------------------------------------------------------------

    char* pszMetadata = CPLSerializeXMLTree( phMetadata );

    if( pszMetadata == nullptr )
    {
        return false;
    }

    OCILobLocator* phLocatorR = nullptr;
    OCILobLocator* phLocatorW = nullptr;

    OWStatement* poStmt = poConnection->CreateStatement( CPLSPrintf(
        "DECLARE\n"
        "  GR1 sdo_georaster;\n"
        "BEGIN\n"
        "  SELECT %s INTO GR1 FROM %s%s T WHERE %s FOR UPDATE;\n"
        "\n"
        "  GR1.metadata := sys.xmltype.createxml(:1);\n"
        "\n"
        "  SDO_GEOR.addNODATA( GR1, :2, :3 );\n"
        "\n"
        "  UPDATE %s%s T SET %s = GR1 WHERE %s;\n"
        "\n"
        "  EXECUTE IMMEDIATE\n"
        "    'SELECT T.%s.METADATA.getClobVal() FROM %s%s T \n"
        "     WHERE  T.%s.RASTERDATATABLE = UPPER(:1)\n"
        "       AND  T.%s.RASTERID = :2'\n"
        "    INTO :metadata USING :rdt, :rid;\n"
        "END;",
            sColumn.c_str(), sSchema.c_str(), sTable.c_str(), sWhere.c_str(),
            sSchema.c_str(), sTable.c_str(), sColumn.c_str(), sWhere.c_str(),
            sColumn.c_str(), sSchema.c_str(), sTable.c_str(),
            sColumn.c_str(),
            sColumn.c_str() ) );

    poStmt->WriteCLob( &phLocatorW, pszMetadata );

    poStmt->Bind( &phLocatorW );
    poStmt->Bind( &nLayer );
    poStmt->Bind( szNoData );
    poStmt->BindName( ":metadata", &phLocatorR );
    poStmt->BindName( ":rdt", szRDT );
    poStmt->BindName( ":rid", &nRID );

    CPLFree( pszMetadata );

    if( ! poStmt->Execute() )
    {
        poStmt->FreeLob(phLocatorR);
        poStmt->FreeLob(phLocatorW);
        delete poStmt;
        return false;
    }

    poStmt->FreeLob(phLocatorW);

    // ------------------------------------------------------------
    //  Read the XML metadata from db to memory with nodata updates
    // ------------------------------------------------------------

    char* pszXML = poStmt->ReadCLob( phLocatorR );

    if( pszXML )
    {
        CPLDestroyXMLNode( phMetadata );
        phMetadata = CPLParseXMLString( pszXML );
        CPLFree( pszXML );
    }

    poStmt->FreeLob(phLocatorR);

    bFlushMetadata = true;
    delete poStmt;
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

        if( psVAT != nullptr )
        {
            CPLRemoveXMLChild( psLayers, psVAT );
            CPLDestroyXMLNode( psVAT );
        }

        CPLCreateXMLElementAndValue(psLayers, "vatTableName", pszName );

        // ------------------------------------------------------------
        // To be updated at Flush Metadata in SDO_GEOR.setVAT()
        // ------------------------------------------------------------

        sValueAttributeTab = pszName;

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

    if( psLayers == nullptr )
    {
        return nullptr;
    }

    char* pszTablename = nullptr;

    int n = 1;

    for( ; psLayers; psLayers = psLayers->psNext, n++ )
    {
        if( n != nBand )
        {
            continue;
        }

        CPLXMLNode* psVAT = CPLGetXMLNode( psLayers, "vatTableName" );

        if( psVAT != nullptr )
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
    if( bFlushBlock )
    {
        FlushBlock( nCacheBlockId );
    }

    if( ! bFlushMetadata )
    {
        return true;
    }

    bFlushMetadata = false;

    //  --------------------------------------------------------------------
    //  Change the isBlank setting left by SDO_GEOR.createBlank() to 'false'
    //  --------------------------------------------------------------------

    CPLXMLNode* psOInfo = CPLGetXMLNode( phMetadata, "objectInfo" );

    CPLSetXMLValue( psOInfo,  "isBlank", "false" );

    CPLXMLNode* psNode  = CPLGetXMLNode( psOInfo, "blankCellValue" );

    if( psNode != nullptr )
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
    //  Set compression
    //  --------------------------------------------------------------------

    psNode = CPLGetXMLNode( phMetadata, "rasterInfo.compression" );

    if( psNode )
    {
        CPLSetXMLValue( psNode, "type", sCompressionType.c_str() );

        if( STARTS_WITH_CI(sCompressionType.c_str(), "JPEG") )
        {
            CPLSetXMLValue( psNode, "quality",
                CPLSPrintf( "%d", nCompressQuality ) );
        }
    }

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

    double dfXCoef[3];
    double dfYCoef[3];
    int nMLC;

    dfXCoef[0] = dfXCoefficient[0];
    dfXCoef[1] = dfXCoefficient[1];
    dfXCoef[2] = dfXCoefficient[2];

    dfYCoef[0] = dfYCoefficient[0];
    dfYCoef[1] = dfYCoefficient[1];
    dfYCoef[2] = dfYCoefficient[2];

    if ( eModelCoordLocation == MCL_CENTER )
    {
        dfXCoef[2] += ( dfXCoefficient[0] / 2.0 );
        dfYCoef[2] += ( dfYCoefficient[1] / 2.0 );
        nMLC = MCL_CENTER;
    }
    else
    {
        nMLC = MCL_UPPERLEFT;
    }

    if( phRPC )
    {
        SetRPC();
        nSRID = NO_CRS;
    }

    //  --------------------------------------------------------------------
    //  Serialize XML metadata to plain text
    //  --------------------------------------------------------------------

    char* pszMetadata = CPLSerializeXMLTree( phMetadata );

    if( pszMetadata == nullptr )
    {
        return false;
    }

    //  --------------------------------------------------------------------
    //  Search for existing Spatial Index SRID
    //  --------------------------------------------------------------------

    OWStatement* poStmt = (OWStatement*) nullptr;

    if ( nSRID != UNKNOWN_CRS && bGenSpatialExtent )
    {
        long long nIdxSRID = -1;

        poStmt = poConnection->CreateStatement( CPLSPrintf(
            "DECLARE\n"
            "  IDX_SRID NUMBER;\n"
            "BEGIN\n"
            "  BEGIN\n"
            "    EXECUTE IMMEDIATE \n"
            "         'SELECT SRID FROM ALL_SDO_GEOM_METADATA ' || \n"
            "         'WHERE OWNER = ''%s'' AND ' || \n"
            "         'TABLE_NAME  = ''%s'' AND ' || \n"
            "         'COLUMN_NAME = ''%s'' || ''.SPATIALEXTENT'' '\n"
            "      INTO IDX_SRID;\n"
            "  EXCEPTION\n"
            "    WHEN NO_DATA_FOUND THEN\n"
            "      IDX_SRID := -1;\n"
            "  END;\n"
            "  :idx_srid := IDX_SRID;\n"
            "END;",
                sOwner.c_str(),
                sTable.c_str(),
                sColumn.c_str()));

        poStmt->BindName( ":idx_srid", &nIdxSRID );

        if( ! poStmt->Execute() )
        {
            nIdxSRID = -1;
        }

        if ( nIdxSRID != -1 )
        {
            if ( nExtentSRID == 0 )
            {
                nExtentSRID = nIdxSRID;
            }
            else
            {
                if ( nExtentSRID != nIdxSRID )
                {
                    CPLError( CE_Warning, CPLE_AppDefined,
                      "Cannot generate spatialExtent, "
                      "Spatial Index SRID is (%lld)",
                      nIdxSRID );

                    nExtentSRID = 0;
                }
            }
        }

        delete poStmt;

        CPLDebug("GEOR","nIdxSRID    = %lld",nIdxSRID);
    }
    else
    {
        nExtentSRID = 0;
    }

    if ( nSRID == 0 || nSRID == UNKNOWN_CRS )
    {
        nExtentSRID = 0;
    }

    CPLDebug("GEOR","nExtentSRID = %lld",nExtentSRID);
    CPLDebug("GEOR","nSRID       = %lld",nSRID);

    //  --------------------------------------------------------------------
    //  Update GeoRaster Metadata
    //  --------------------------------------------------------------------

    int nException = 0;

    OCILobLocator* phLocator = nullptr;

    poStmt = poConnection->CreateStatement( CPLSPrintf(
        "DECLARE\n"
        "  GR1      sdo_georaster;\n"
        "  GM1      sdo_geometry;\n"
        "  SRID     number  := :1;\n"
        "  EXT_SRID number  := :2;\n"
        "  VAT      varchar2(128);\n"
        "BEGIN\n"
        "\n"
        "  SELECT %s INTO GR1 FROM %s%s T WHERE %s FOR UPDATE;\n"
        "\n"
        "  GR1.metadata := sys.xmltype.createxml(:3);\n"
        "\n"
        "  IF SRID != 0 THEN\n"
        "    SDO_GEOR.georeference( GR1, SRID, :4, \n"
        "      SDO_NUMBER_ARRAY(:5, :6, :7), SDO_NUMBER_ARRAY(:8, :9, :10));\n"
        "  END IF;\n"
        "\n"
        "  IF EXT_SRID = 0 THEN\n"
        "    GM1 := NULL;\n"
        "  ELSE\n"
        "    GM1 := SDO_GEOR.generateSpatialExtent( GR1 );\n"
        "    IF EXT_SRID != SRID THEN\n"
        "      GM1 := SDO_CS.transform( GM1, EXT_SRID );\n"
        "    END IF;\n"
        "  END IF;\n"
        "\n"
        "  GR1.spatialExtent := GM1;\n"
        "\n"
        "  VAT := '%s';\n"
        "  IF VAT != '' THEN\n"
        "    SDO_GEOR.setVAT(GR1, 1, VAT);\n"
        "  END IF;\n"
        "\n"
        "  BEGIN\n"
        "    UPDATE %s%s T SET %s = GR1\n"
        "    WHERE %s;\n"
        "  EXCEPTION\n"
        "    WHEN OTHERS THEN\n"
        "      :except := SQLCODE;\n"
        "  END;\n"
        "\n"
        "  COMMIT;\n"
        "END;",
            sColumn.c_str(),
            sSchema.c_str(),
            sTable.c_str(),
            sWhere.c_str(),
            sValueAttributeTab.c_str(),
            sSchema.c_str(),
            sTable.c_str(),
            sColumn.c_str(),
            sWhere.c_str() ) );

    poStmt->WriteCLob( &phLocator, pszMetadata );

    poStmt->Bind( &nSRID );
    poStmt->Bind( &nExtentSRID );
    poStmt->Bind( &phLocator );
    poStmt->Bind( &nMLC );
    poStmt->Bind( &dfXCoef[0] );
    poStmt->Bind( &dfXCoef[1] );
    poStmt->Bind( &dfXCoef[2] );
    poStmt->Bind( &dfYCoef[0] );
    poStmt->Bind( &dfYCoef[1] );
    poStmt->Bind( &dfYCoef[2] );
    poStmt->BindName( ":except", &nException );

    CPLFree( pszMetadata );

    if( ! poStmt->Execute() )
    {
        poStmt->FreeLob(phLocator);
        delete poStmt;
        return false;
    }

    poStmt->FreeLob(phLocator);

    delete poStmt;

    if( nException )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
            "Fail to update GeoRaster Metadata (ORA-%d) ", nException );
    }

    if (bGenPyramid)
    {
        if (GeneratePyramid( nPyramidLevels, sPyramidResampling.c_str(), true ))
        {
            CPLDebug("GEOR", "Generated pyramid successfully.");
        }
        else
        {
            CPLError( CE_Warning, CPLE_AppDefined, "Error generating pyramid!");
        }
    }

    return true;
}

//  ---------------------------------------------------------------------------
//                                                            GeneratePyramid()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::GeneratePyramid( int nLevels,
                                        const char* pszResampling,
                                        bool bInternal )
{
    nPyramidMaxLevel = nLevels;

    if( bInternal )
    {
        CPLString sLevels = "";

        if (nLevels > 0)
        {
            sLevels = CPLSPrintf("rlevel=%d", nLevels);
        }

        OWStatement* poStmt = poConnection->CreateStatement( CPLSPrintf(
            "DECLARE\n"
            "  gr sdo_georaster;\n"
            "BEGIN\n"
            "  SELECT %s INTO gr FROM %s t WHERE %s FOR UPDATE;\n"
            "  sdo_geor.generatePyramid(gr, '%s resampling=%s');\n"
            "  UPDATE %s t SET %s = gr WHERE %s;\n"
            "  COMMIT;\n"
            "END;\n",
                sColumn.c_str(),
                sTable.c_str(),
                sWhere.c_str(),
                sLevels.c_str(),
                pszResampling,
                sTable.c_str(),
                sColumn.c_str(),
                sWhere.c_str() ) );

        if( poStmt->Execute() )
        {
            delete poStmt;
            return true;
        }

        delete poStmt;
        return false;
    }

    //  -----------------------------------------------------------
    //  Create rows for pyramid levels
    //  -----------------------------------------------------------

    OWStatement* poStmt = poConnection->CreateStatement(
        "DECLARE\n"
        "  SCL  NUMBER         := 0;\n"
        "  RC   NUMBER         := 0;\n"
        "  RR   NUMBER         := 0;\n"
        "  CBS2 NUMBER         := 0;\n"
        "  RBS2 NUMBER         := 0;\n"
        "  TBB  NUMBER         := 0;\n"
        "  TRB  NUMBER         := 0;\n"
        "  TCB  NUMBER         := 0;\n"
        "  X    NUMBER         := 0;\n"
        "  Y    NUMBER         := 0;\n"
        "  W    NUMBER         := 0;\n"
        "  H    NUMBER         := 0;\n"
        "  STM  VARCHAR2(1024) := '';\n"
        "BEGIN\n"
        "  EXECUTE IMMEDIATE 'DELETE FROM '||:rdt||' \n"
        "    WHERE RASTERID = '||:rid||' AND PYRAMIDLEVEL > 0';\n"
        "  STM := 'INSERT INTO '||:rdt||' VALUES (:1, :2, :3-1, :4-1, :5-1 ,\n"
        "    SDO_GEOMETRY(2003, NULL, NULL, SDO_ELEM_INFO_ARRAY(1, 1003, 3),\n"
        "    SDO_ORDINATE_ARRAY(:6, :7, :8-1, :9-1)), EMPTY_BLOB() )';\n"
        "  TBB  := :TotalBandBlocks;\n"
        "  RBS2 := floor(:RowBlockSize / 2);\n"
        "  CBS2 := floor(:ColumnBlockSize / 2);\n"
        "  FOR l IN 1..:level LOOP\n"
        "    SCL := 2 ** l;\n"
        "    RR  := floor(:RasterRows / SCL);\n"
        "    RC  := floor(:RasterColumns / SCL);\n"
        "    IF (RC <= CBS2) OR (RR <= RBS2) THEN\n"
        "      H   := RR;\n"
        "      W   := RC;\n"
        "    ELSE\n"
        "      H   := :RowBlockSize;\n"
        "      W   := :ColumnBlockSize;\n"
        "    END IF;\n"
        "    TRB := greatest(1, ceil( ceil(:RasterColumns / :ColumnBlockSize) / SCL));\n"
        "    TCB := greatest(1, ceil( ceil(:RasterRows / :RowBlockSize) / SCL));\n"
        "    FOR b IN 1..TBB LOOP\n"
        "      Y := 0;\n"
        "      FOR r IN 1..TCB LOOP\n"
        "        X := 0;\n"
        "        FOR c IN 1..TRB LOOP\n"
        "          EXECUTE IMMEDIATE STM USING :rid, l, b, r, c, Y, X, (Y+H), (X+W);\n"
        "          X := X + W;\n"
        "        END LOOP;\n"
        "        Y := Y + H;\n"
        "      END LOOP;\n"
        "    END LOOP;\n"
        "  END LOOP;\n"
        "  COMMIT;\n"
        "END;" );

    const char* pszDataTable = sDataTable.c_str();

    poStmt->BindName( ":rdt",             (char*) pszDataTable );
    poStmt->BindName( ":rid",             &nRasterId );
    poStmt->BindName( ":level",           &nLevels );
    poStmt->BindName( ":TotalBandBlocks", &nTotalBandBlocks );
    poStmt->BindName( ":RowBlockSize",    &nRowBlockSize );
    poStmt->BindName( ":ColumnBlockSize", &nColumnBlockSize );
    poStmt->BindName( ":RasterRows",      &nRasterRows );
    poStmt->BindName( ":RasterColumns",   &nRasterColumns );

    if( ! poStmt->Execute() )
    {
        delete poStmt;
        return false;
    }
    delete poStmt;

    CPLXMLNode* psNode = CPLGetXMLNode( phMetadata, "rasterInfo.pyramid" );

    if( psNode )
    {
        CPLSetXMLValue( psNode, "type", "DECREASE" );
        CPLSetXMLValue( psNode, "resampling", pszResampling );
        CPLSetXMLValue( psNode, "maxLevel", CPLSPrintf( "%d", nLevels ) );
    }

    bFlushMetadata = true;

    return true;
}

//  ---------------------------------------------------------------------------
//                                                            GeneratePyramid()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::DeletePyramid()
{
    OWStatement* poStmt = poConnection->CreateStatement( CPLSPrintf(
        "DECLARE\n"
        "  gr sdo_georaster;\n"
        "BEGIN\n"
        "  SELECT %s INTO gr FROM %s t WHERE %s FOR UPDATE;\n"
        "  sdo_geor.deletePyramid(gr);\n"
        "  UPDATE %s t SET %s = gr WHERE %s;\n"
        "  COMMIT;\n"
        "END;\n",
            sColumn.c_str(),
            sTable.c_str(),
            sWhere.c_str(),
            sTable.c_str(),
            sColumn.c_str(),
            sWhere.c_str() ) );

    CPL_IGNORE_RET_VAL(poStmt->Execute());

    delete poStmt;
}

//  ---------------------------------------------------------------------------
//                                                           CreateBitmapMask()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::InitializeMask( int nLevel,
                                       int nBlockColumns,
                                       int nBlockRows,
                                       int nColumnBlocks,
                                       int nRowBlocks,
                                       int nBandBlocks )
{
    //  -----------------------------------------------------------
    //  Create rows for the bitmap mask
    //  -----------------------------------------------------------

    OWStatement* poStmt = poConnection->CreateStatement(
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

    char pszDataTable[OWNAME];

    poStmt->Bind( &nBlockColumns );
    poStmt->Bind( &nBlockRows );
    poStmt->Bind( &nBandBlocks );
    poStmt->Bind( &nRowBlocks );
    poStmt->Bind( &nColumnBlocks );
    poStmt->BindName( ":rdt", pszDataTable );
    poStmt->BindName( ":rid", &nRasterId );
    poStmt->BindName( ":lev", &nLevel );

    if( ! poStmt->Execute() )
    {
        delete poStmt;
        return false;
    }

    delete poStmt;
    return true;
}

//  ---------------------------------------------------------------------------
//                                                                UnpackNBits()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::UnpackNBits( GByte* pabyData )
{
    int nPixCount = nColumnBlockSize * nRowBlockSize * nBandBlockSize;

    if( EQUAL( sCellDepth.c_str(), "4BIT" ) )
    {
        for( int ii = nPixCount - 2; ii >= 0; ii -= 2 )
        {
            int k = ii >> 1;
            pabyData[ii+1] = (pabyData[k]     ) & 0xf;
            pabyData[ii]   = (pabyData[k] >> 4) & 0xf;
        }
    }
    else if( EQUAL( sCellDepth.c_str(), "2BIT" ) )
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

void GeoRasterWrapper::PackNBits( GByte* pabyData ) const
{
    int nPixCount = nBandBlockSize * nRowBlockSize * nColumnBlockSize;

    GByte* pabyBuffer = (GByte*) VSI_MALLOC_VERBOSE( nPixCount );

    if( pabyBuffer == nullptr )
    {
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

constexpr int AC_BITS[16] =
{
    0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 119
};

constexpr int AC_HUFFVAL[256] =
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

constexpr int DC_BITS[16] =
{
    0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0
};

constexpr int DC_HUFFVAL[256] =
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
static
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

    VSILFILE *fpImage = VSIFOpenL( pszMemFile, "wb" );
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

        if( EQUAL( sCompressionType.c_str(), "JPEG-B") )
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

    VSILFILE *fpImage = VSIFOpenL( pszMemFile, "wb" );

    bool write_all_tables = TRUE;

    if( EQUAL( sCompressionType.c_str(), "JPEG-B") )
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

        if( EQUAL( sCompressionType.c_str(), "JPEG-B") )
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
    size_t nSize = VSIFReadL( pabyCompressBuf, 1, nBlockBytes, fpImage );
    VSIFCloseL( fpImage );

    VSIUnlink( pszMemFile );

    return (unsigned long) nSize;
}

//  ---------------------------------------------------------------------------
//                                                          UncompressDeflate()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::UncompressDeflate( unsigned long nBufferSize )
{
    GByte* pabyBuf = (GByte*) VSI_MALLOC_VERBOSE( nBufferSize );

    if( pabyBuf == nullptr )
    {
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

    GByte* pabyBuf = (GByte*) VSI_MALLOC_VERBOSE( nBlockBytes );

    if( pabyBuf == nullptr )
    {
        return 0;
    }

    memcpy( pabyBuf, pabyBlockBuf, nBlockBytes );

    // Call ZLib compress

    int nRet = compress( pabyCompressBuf, &nLen, pabyBuf, nBlockBytes );

    CPLFree( pabyBuf );

    if( nRet != Z_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "ZLib return code (%d)", nRet );
        return 0;
    }

    return nLen;
}
