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

#include <string>


#include "georaster_priv.h"
#include "cpl_error.h"
#include "cpl_string.h"

//  ---------------------------------------------------------------------------
//                                                           GeoRasterWrapper()
//  ---------------------------------------------------------------------------

GeoRasterWrapper::GeoRasterWrapper()
{
    nRasterId           = -1;
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
    sCompressionType    = "NONE";
    nCompressQuality    = 75;
    pahLocator          = NULL;
    pabyBlockBuf        = NULL;
    pabyCompressBuf     = NULL;
    bIsReferenced       = false;
    poBlockStmt         = NULL;
    nCurrentBlock       = -1;
    nCurrentLevel       = -1;
    pahLevels           = NULL;
    nLevelOffset        = 0L;
    sInterleaving       = "BSQ";
    bUpdate             = false;
    bInitializeIO       = false;
    bFlushMetadata      = false;
    nSRID               = UNKNOWN_CRS;
    nPyramidMaxLevel    = 0;
    nBlockCount         = 0;
    nGDALBlockBytes     = 0;
    sDInfo.global_state = 0;
    sCInfo.global_state = 0;
    bHasBitmapMask      = false;
    nBlockBytes         = 0;
    bFlushBlock         = false;
    bUniqueFound        = false;
}

//  ---------------------------------------------------------------------------
//                                                           GeoRasterDataset()
//  ---------------------------------------------------------------------------

GeoRasterWrapper::~GeoRasterWrapper()
{
    FlushMetadata();

    if( pahLocator && nBlockCount )
    {
        OWStatement::Free( pahLocator, nBlockCount );
    }

    CPLFree( pahLocator );
    CPLFree( pabyBlockBuf );
    CPLFree( pabyCompressBuf );
    CPLFree( pahLevels );
    
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
        return NULL;
    }

    poGRW->bUpdate = bUpdate;
    
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
            poGRW->sOwner  = papszSchema[0];
            poGRW->sSchema = CPLSPrintf( "%s.", poGRW->sOwner.c_str() );

            papszParam = CSLRemoveStrings( papszParam, 3, 1, NULL );

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
    else
    {
        poGRW->sSchema = "";
        poGRW->sOwner  = poGRW->poConnection->GetUser();
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
            poGRW->nRasterId    = atoi( papszParam[4]);
            break;
        }
        else
        {
            poGRW->sTable   = papszParam[3];
            poGRW->sColumn  = papszParam[4];
            return poGRW;
        }
    case 4 :
        poGRW->sTable   = papszParam[3];
        return poGRW;
    default :
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
    int nRasterId = -1;
    int nSizeX = 0;
    int nSizeY = 0;
    int nSRID  = 0;
    OCILobLocator* phLocator = NULL;
    double dfULx = 0.0;
    double dfURx = 0.0;
    double dfLRx = 0.0;
    double dfULy = 0.0;
    double dfLLy = 0.0;
    double dfLRy = 0.0;
    char szWKText[2 * OWTEXT];
    char szAuthority[OWTEXT];

    if( ! poGRW->sOwner.empty() )
      strcpy( szOwner, poGRW->sOwner.c_str() );
    else
      szOwner[0] = '\0';

    if( ! poGRW->sTable.empty() )
      strcpy( szTable, poGRW->sTable.c_str() );
    else
      szTable[0] = '\0';

    if( ! poGRW->sColumn.empty() )
      strcpy( szColumn, poGRW->sColumn.c_str() );
    else
      szColumn[0] = '\0';

    if( ! poGRW->sDataTable.empty() )
      strcpy( szDataTable, poGRW->sDataTable.c_str() );
    else
      szDataTable[0] = '\0';

    nRasterId = poGRW->nRasterId;

    if( ! poGRW->sWhere.empty() )
      strcpy( szWhere, poGRW->sWhere.c_str() );
    else
      szWhere[0] = '\0';

    OWStatement* poStmt = poGRW->poConnection->CreateStatement(
      "DECLARE\n"
      "  SCM VARCHAR2(64) := 'xmlns=\"http://xmlns.oracle.com/spatial/georaster\"';\n"
      "  GUL SDO_GEOMETRY := null;\n"
      "  GUR SDO_GEOMETRY := null;\n"
      "  GLL SDO_GEOMETRY := null;\n"
      "  GLR SDO_GEOMETRY := null;\n"
      "BEGIN\n"
      "\n"
      "    IF :datatable IS NOT NULL AND :rasterid  > 0 THEN\n"
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
      "  SELECT\n"
      "    extractValue(XMLType(:metadata),"
      "'/georasterMetadata/rasterInfo/dimensionSize[@type=\"ROW\"]/size', "
      "SCM),\n"
      "    extractValue(XMLType(:metadata),"
      "'/georasterMetadata/rasterInfo/dimensionSize[@type=\"COLUMN\"]/size', "
      "SCM),\n"
      "    extractValue(XMLType(:metadata),"
      "'/georasterMetadata/spatialReferenceInfo/SRID', "
      "SCM)\n"
      "    INTO :sizey, :sizex, :srid FROM DUAL;\n"
      "\n"
      "  EXECUTE IMMEDIATE\n"
      "    'SELECT\n"
      "      SDO_GEOR.getModelCoordinate('||:column||', 0, "
      "SDO_NUMBER_ARRAY(0, 0)),\n"
      "      SDO_GEOR.getModelCoordinate('||:column||', 0, "
      "SDO_NUMBER_ARRAY(0, '||:sizex||')),\n"
      "      SDO_GEOR.getModelCoordinate('||:column||', 0, "
      "SDO_NUMBER_ARRAY('||:sizey||', 0)),\n"
      "      SDO_GEOR.getModelCoordinate('||:column||', 0, "
      "SDO_NUMBER_ARRAY('||:sizey||', '||:sizex||'))\n"
      "     FROM  '||:owner||'.'||:table||' T\n"
      "     WHERE T.'||:column||'.RASTERDATATABLE = UPPER(:1)\n"
      "       AND T.'||:column||'.RASTERID = :2'\n"
      "    INTO  GUL, GLL, GUR, GLR\n"
      "    USING :datatable, :rasterid;\n"
      "\n"
      "  :ULx := GUL.sdo_point.x;\n"
      "  :URx := GUR.sdo_point.x;\n"
      "  :LRx := GLR.sdo_point.x;\n"
      "  :ULy := GUL.sdo_point.y;\n"
      "  :LLy := GLL.sdo_point.y;\n"
      "  :LRy := GLR.sdo_point.y;\n"
      "\n"
      "  BEGIN\n"
      "    EXECUTE IMMEDIATE\n"
      "      'SELECT WKTEXT, AUTH_NAME\n"
      "       FROM   MDSYS.CS_SRS\n"
      "       WHERE  SRID = :1 AND WKTEXT IS NOT NULL'\n"
      "      INTO   :wktext, :authority\n"
      "      USING  :srid;\n"
      "  EXCEPTION\n"
      "    WHEN no_data_found THEN\n"
      "      :wktext := '';\n"
      "      :authority := '';\n"
      "  END;\n"
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
    poStmt->BindName( ":sizex", &nSizeX );
    poStmt->BindName( ":sizey", &nSizeY );
    poStmt->BindName( ":srid", &nSRID );
    poStmt->BindName( ":ULx", &dfULx );
    poStmt->BindName( ":URx", &dfURx );
    poStmt->BindName( ":LRx", &dfLRx );
    poStmt->BindName( ":ULy", &dfULy );
    poStmt->BindName( ":LLy", &dfLLy );
    poStmt->BindName( ":LRy", &dfLRy );
    poStmt->BindName( ":wktext", szWKText, sizeof(szWKText) );
    poStmt->BindName( ":authority", szAuthority );

    if( ! poStmt->Execute() )
    {
        delete poStmt;
        delete poGRW;
        return NULL;
    }

    if( nCounter < 1 )
    {
        delete poStmt;
        delete poGRW;
        return NULL;
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

    poGRW->sWKText      = szWKText;
    poGRW->sAuthority   = szAuthority;
    poGRW->sDataTable   = szDataTable;
    poGRW->nRasterId    = nRasterId;
    poGRW->sWhere       = CPLSPrintf(
        "T.%s.RASTERDATATABLE = UPPER('%s') AND T.%s.RASTERID = %d",
        poGRW->sColumn.c_str(),
        poGRW->sDataTable.c_str(),
        poGRW->sColumn.c_str(),
        poGRW->nRasterId );
    
    //  -------------------------------------------------------------------
    //  Read Metadata XML in text
    //  -------------------------------------------------------------------

    char* pszXML = poStmt->ReadCLob( phLocator );

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
        poGRW->sDataTable = "";
        poGRW->nRasterId  = 0;
    }

    // --------------------------------------------------------------------
    // Load Coefficients matrix
    // --------------------------------------------------------------------

    double dfRotation = 0.0;

    if( ! CPLIsEqual( dfULy, dfLLy ) )
    {
        dfRotation = ( dfURx - dfULx ) / ( dfLLy - dfULy );
    }

    poGRW->dfXCoefficient[0] = ( dfLRx - dfULx ) / nSizeX;
    poGRW->dfXCoefficient[1] = dfRotation;
    poGRW->dfXCoefficient[2] = dfULx;
    poGRW->dfYCoefficient[0] = -dfRotation;
    poGRW->dfYCoefficient[1] = ( dfLRy - dfULy ) / nSizeY;
    poGRW->dfYCoefficient[2] = dfULy;

    //  -------------------------------------------------------------------
    //  Clean up
    //  -------------------------------------------------------------------

    OCIDescriptorFree( phLocator, OCI_DTYPE_LOB );
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
                               bool bUpdate )
{
    CPLString sValues;
    CPLString sFormat;

    if( sTable.empty() ||
        sColumn.empty() )
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
                "(%s MDSYS.SDO_GEORASTER)", sColumn.c_str() ) );
        }

        //  ---------------------------------------------------------------
        //  Insert parameters
        //  ---------------------------------------------------------------

        if( pszInsert )
        {
            if( strstr( pszInsert, "VALUES" ) == NULL &&
                strstr( pszInsert, "values" ) == NULL )
            {
                sValues = CPLSPrintf( "VALUES %s", pszInsert );
            }
            else
            {
                sValues = pszInsert;
            }
        }
        else
        {
            sValues = "VALUES (SDO_GEOR.INIT(NULL,NULL))";
        }
    }

    //  -------------------------------------------------------------------
    //  Parse RDT/RID from the current szValues
    //  -------------------------------------------------------------------

    char szRDT[OWNAME];
    char szRID[OWCODE];

    if( ! sDataTable.empty() )
    {
        strcpy( szRDT, CPLSPrintf( "'%s'", sDataTable.c_str() ) );
    }
    else
    {
        strcpy( szRDT, OWParseSDO_GEOR_INIT( sValues.c_str(), 1 ) );
    }

    if ( nRasterId > 0 )
    {
        strcpy( szRID, CPLSPrintf( "%d", nRasterId ) );
    }
    else
    {
        strcpy( szRID, OWParseSDO_GEOR_INIT( sValues.c_str(), 2 ) );
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
            OWReplaceString( sValues.c_str(), "SDO_GEOR.INIT", ")", "GR1" ) );
    }

    //  -----------------------------------------------------------
    //  Storage parameters
    //  -----------------------------------------------------------

    nColumnBlockSize = nColumnBlockSize == 0 ? DEFAULT_BLOCK_COLUMNS : nColumnBlockSize;
    nRowBlockSize    = nRowBlockSize    == 0 ? DEFAULT_BLOCK_ROWS    : nRowBlockSize;
    nBandBlockSize   = nBandBlockSize   == 0 ? 1 : nBandBlockSize;

    if( poConnection->GetVersion() < 11 )
    {
        if( nRasterBands == 1 )
        {
            sFormat =  CPLSPrintf(
                "blockSize=(%d, %d) "
                "cellDepth=%s "
                "interleaving=%s "
                "pyramid=FALSE "
                "compression=NONE ",
                nRowBlockSize, nColumnBlockSize,
                sCellDepth.c_str(),
                sInterleaving.c_str() );
        }
        else
        {
            sFormat = CPLSPrintf(
                "blockSize=(%d, %d, %d) "
                "cellDepth=%s "
                "interleaving=%s "
                "pyramid=FALSE "
                "compression=NONE ",
                nRowBlockSize, nColumnBlockSize, nBandBlockSize,
                sCellDepth.c_str(),
                sInterleaving.c_str() );
        }
    }
    else
    {
        if( nRasterBands == 1 )
        {
            sFormat = CPLSPrintf(
                "20001, '"
                "dimSize=(%d,%d) "
                "blockSize=(%d,%d) "
                "cellDepth=%s "
                "interleaving=%s "
                "compression=%s '",
                nRasterRows, nRasterColumns,
                nRowBlockSize, nColumnBlockSize,
                sCellDepth.c_str(),
                sInterleaving.c_str(),
                sCompressionType.c_str() );
        }
        else
        {
            sFormat = CPLSPrintf(
                "21001, '"
                "dimSize=(%d,%d,%d) "
                "blockSize=(%d,%d,%d) "
                "cellDepth=%s "
                "interleaving=%s "
                "compression=%s '",
                nRasterRows, nRasterColumns, nRasterBands,
                nRowBlockSize, nColumnBlockSize, nBandBlockSize,
                sCellDepth.c_str(),
                sInterleaving.c_str(),
                sCompressionType.c_str() );
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
            sSchema.c_str(),
            sTable.c_str(),
            sColumn.c_str(),
            sWhere.c_str(),
            sColumn.c_str() ) );
    }
    else
    {
        strcpy( szCommand, CPLSPrintf(
            "INSERT INTO %s%s %s RETURNING %s INTO GR1;",
            sSchema.c_str(),
            sTable.c_str(),
            szInsert,
            sColumn.c_str() ) );
    }

    //  -----------------------------------------------------------
    //  Create RTD if needed and insert/update GeoRaster
    //  -----------------------------------------------------------

    char szBindRDT[OWNAME];
    int  nBindRID = 0;

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
                szCreateBlank,
                szCommand,
                sSchema.c_str(),
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

        OCILobLocator* phLocator = NULL;

        poStmt->BindName( ":metadata", &phLocator );
        poStmt->BindName( ":rdt", szBindRDT );
        poStmt->BindName( ":rid", &nBindRID );

        if( ! poStmt->Execute() )
        {
            delete poStmt;
            CPLError( CE_Failure, CPLE_AppDefined,
                "Failure to initialize GeoRaster" );
            return false;
        }

        sDataTable = szBindRDT;
        nRasterId  = nBindRID;

        OCIDescriptorFree( phLocator, OCI_DTYPE_LOB );

        
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
        "  OWN  VARCHAR2(68)    := UPPER('%s');\n"
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
        "\n"
        "  SDO_GEOR.changeFormatCopy(GR1, '%s', GR2);\n"
        "\n"
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
        "\n"
        "  SELECT %s INTO GR1 FROM %s%s T WHERE"
        " T.%s.RasterDataTable = :rdt AND"
        " T.%s.RasterId = :rid FOR UPDATE;\n"
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
            szCreateBlank,
            szCommand,
            sColumn.c_str(), sSchema.c_str(), sTable.c_str(),
            sColumn.c_str(), sColumn.c_str(),
            sColumn.c_str(), sSchema.c_str(), sTable.c_str(),
            sColumn.c_str(), sColumn.c_str(), 
            sFormat.c_str(), sSchema.c_str(), sTable.c_str(),
            sColumn.c_str(), sColumn.c_str(), sColumn.c_str(),
            sSchema.c_str(), sSchema.c_str(), sSchema.c_str(),
            sColumn.c_str(), sSchema.c_str(), sTable.c_str(),
            sColumn.c_str(), sColumn.c_str(),
            UNKNOWN_CRS, MCL_DEFAULT,
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
        CPLError( CE_Failure, CPLE_AppDefined,
            "Failure to initialize GeoRaster" );
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
    bIsReferenced       = false;
    nCurrentBlock       = -1;
    nCurrentLevel       = -1;
    pahLevels           = NULL;
    nLevelOffset        = 0L;
    sInterleaving       = "BSQ";
    bUpdate             = false;
    bInitializeIO       = false;
    bFlushMetadata      = false;
    nSRID               = -1;
    nPyramidMaxLevel    = 0;
    nBlockCount         = 0;
    sDInfo.global_state = 0;
    sCInfo.global_state = 0;
    bHasBitmapMask      = false;
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

    sInterleaving = CPLGetXMLValue( phMetadata,
        "rasterInfo.interleaving", "BSQ" );

    //  -------------------------------------------------------------------
    //  Get blocking
    //  -------------------------------------------------------------------

    nRowBlockSize       = atoi( CPLGetXMLValue( phMetadata,
                            "rasterInfo.blocking.rowBlockSize", "0" ) );

    nColumnBlockSize    = atoi( CPLGetXMLValue( phMetadata,
                            "rasterInfo.blocking.columnBlockSize", "0" ) );

    nBandBlockSize      = atoi( CPLGetXMLValue( phMetadata,
                            "rasterInfo.blocking.bandBlockSize", "1" ) );

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

    if( EQUALN( sCompressionType.c_str(), "JPEG", 4 ) )
    {
        nCompressQuality = atoi( CPLGetXMLValue( phMetadata,
                            "rasterInfo.compression.quality", "75" ) );

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

    nBlockCount = nTotalColumnBlocks * nTotalRowBlocks * nTotalBandBlocks;
    nBlockBytes = ( nColumnBlockSize * nRowBlockSize * nBandBlockSize *
                  nCellSizeBits ) / 8;
    nGDALBlockBytes = nColumnBlockSize * nRowBlockSize * nGDALCellBytes;

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
        int nTCB = nTotalColumnBlocks;
        int nTRB = nTotalRowBlocks;

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
            nCBS = nXSize;
            nRBS = nYSize;
        }

        // ----------------------------------------------------------------
        // Recalculate blocks quantity
        // ----------------------------------------------------------------

        nTCB = (int) MAX( 1, (int) ceil( (double) nTCB / dfScale ) );
        nTRB = (int) MAX( 1, (int) ceil( (double) nTRB / dfScale ) );

        // --------------------------------------------------------------------
        // Save level datails
        // --------------------------------------------------------------------

        pahLevels[iLevel].nColumnBlockSize   = nCBS;
        pahLevels[iLevel].nRowBlockSize      = nRBS;
        pahLevels[iLevel].nTotalColumnBlocks = nTCB;
        pahLevels[iLevel].nTotalRowBlocks    = nTRB;
        pahLevels[iLevel].nBlockCount        = nTCB * nTRB * nTotalBandBlocks;
        pahLevels[iLevel].nBlockBytes        = ( nCBS * nRBS * nBandBlockSize *
                                               nCellSizeBits ) / 8;
        pahLevels[iLevel].nGDALBlockBytes    = nCBS * nRBS * nGDALCellBytes;
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

    int nMaxBufferSize = MAX( nBlockBytes, nGDALBlockBytes );

    pabyBlockBuf = (GByte*) VSIMalloc( sizeof(GByte) * nMaxBufferSize );

    if ( pabyBlockBuf == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, "InitializeIO - Block Buffer" );
        return false;
    }

    // --------------------------------------------------------------------
    // Allocate buffer for one compressed raster block
    // --------------------------------------------------------------------

    if( bUpdate && ! EQUAL( sCompressionType.c_str(), "NONE") )
    {
        pabyCompressBuf = (GByte*) VSIMalloc( sizeof(GByte) * nMaxBufferSize );

        if ( pabyCompressBuf == NULL )
        {
            CPLError( CE_Failure, CPLE_OutOfMemory,
                    "InitializeIO - Compress Buffer" );
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

    if( ! poBlockStmt->Execute( nBlockCount ) )
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
        InitializeIO();
    }

    if( nCurrentLevel != nLevel )
    {
        InitializeLevel( nLevel );
    }
    
    long nBlock = GetBlockNumber( nBand, nXOffset, nYOffset );

    if( nCurrentBlock != nBlock )
    {
        nCurrentBlock = nBlock;

        unsigned long nBytesRead = 0;

        nBytesRead = poBlockStmt->ReadBlob( pahLocator[nBlock],
                                            pabyBlockBuf,
                                            nBlockBytes );

        CPLDebug( "GEOR", "Read Block = %ld; Size = %ld", nBlock, nBytesRead );

        if( nBytesRead == 0 )
        {
            memset( pData, 0, nGDALBlockBytes );

            return true;
        }

        if( nBytesRead < nBlockBytes && EQUAL( sCompressionType.c_str(), "NONE") )
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

        if( EQUALN( sCompressionType.c_str(), "JPEG", 4 ) )
        {
            UncompressJpeg( nBytesRead );
        }
        else if ( EQUAL( sCompressionType.c_str(), "DEFLATE" ) )
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

    CPLDebug( "GEOR", "Level, Block, Band, YOffset, XOffset = "
                      "%4d, %4ld, %4d, %4d, %4d",
                      nLevel, nBlock, nBand, nYOffset, nXOffset );

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
        InitializeIO();
    }

    if( nCurrentLevel != nLevel )
    {
        InitializeLevel( nLevel );
    }

    long nBlock = GetBlockNumber( nBand, nXOffset, nYOffset );

    //  --------------------------------------------------------------------
    //  Flush previous block
    //  --------------------------------------------------------------------

    if( bFlushBlock && nCurrentBlock != nBlock )
    {
        if( ! FlushBlock() )
        {
            return false;
        }
    }

    CPLDebug( "GEOR", "Level, Block, Band, YOffset, XOffset = "
                      "%4d, %4ld, %4d, %4d, %4d",
                      nLevel, nBlock, nBand, nYOffset, nXOffset );

    //  --------------------------------------------------------------------
    //  Read interleaved block
    //  --------------------------------------------------------------------

    if( nBandBlockSize > 1 && nCurrentBlock != nBlock && bFlushBlock )
    {
        CPLDebug( "GEOR", "Reloading block %ld", nBlock );

        unsigned long nBytesRead = 0;

        nBytesRead = poBlockStmt->ReadBlob( pahLocator[nBlock],
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

            if( EQUALN( sCompressionType.c_str(), "JPEG", 4 ) )
            {
                UncompressJpeg( nBytesRead );
            }
            else if ( EQUAL( sCompressionType.c_str(), "DEFLATE" ) )
            {
                UncompressDeflate( nBytesRead );
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

    nCurrentBlock = nBlock;

    bFlushBlock = true;

    return true;
}

//  ---------------------------------------------------------------------------
//                                                                 FlushBlock()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::FlushBlock( void )
{
    unsigned long nFlushBlockSize = nBlockBytes;

    GByte* pabyFlushBuffer = (GByte *) pabyBlockBuf;

    //  --------------------------------------------------------------------
    //  Compress ( from pabyBlockBuf to pabyBlockBuf2 )
    //  --------------------------------------------------------------------

    if( ! EQUAL( sCompressionType.c_str(), "NONE" ) )
    {
        if( EQUALN( sCompressionType.c_str(), "JPEG", 4 ) )
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
    //  Pack bits ( inside pabyOutBuf )
    //  --------------------------------------------------------------------

    if( nCellSizeBits < 8 || nCurrentLevel == DEFAULT_BMP_MASK )
    {
        PackNBits( pabyFlushBuffer );
    }

    //  --------------------------------------------------------------------
    //  Write BLOB
    //  --------------------------------------------------------------------

    CPLDebug( "GEOR", "FlushBlock = %ld; Size = %ld", nCurrentBlock, nFlushBlockSize );

    bFlushBlock = false;

    if( ! poBlockStmt->WriteBlob( pahLocator[nCurrentBlock],
                                  pabyFlushBuffer,
                                  nFlushBlockSize ) )
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

    if( bFlushBlock )
    {
        FlushBlock();
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
    //  Set compression
    //  --------------------------------------------------------------------

    psNode = CPLGetXMLNode( phMetadata, "rasterInfo.compression" );

    if( psNode )
    {
        CPLSetXMLValue( psNode, "type", sCompressionType.c_str() );

        if( EQUALN( sCompressionType.c_str(), "JPEG", 4 ) )
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

    int nModelCoordinateLocation = 0;

#if defined(OW_DEFAULT_CENTER)
    nModelCoordinateLocation = 1;
#endif
    
    CPLString osStatement;

    osStatement.Printf(
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
        sColumn.c_str(),
        sSchema.c_str(),
        sTable.c_str(),
        sWhere.c_str(),
        CPLSerializeXMLTree( phMetadata ),
        UNKNOWN_CRS,
        UNKNOWN_CRS,
        sSchema.c_str(),
        sTable.c_str(),
        sColumn.c_str(),
        sWhere.c_str() );

    OWStatement* poStmt = poConnection->CreateStatement( osStatement );

    poStmt->Bind( &nSRID );
    poStmt->Bind( &nModelCoordinateLocation );
    poStmt->Bind( &dfXCoefficient[0] );
    poStmt->Bind( &dfXCoefficient[1] );
    poStmt->Bind( &dfXCoefficient[2] );
    poStmt->Bind( &dfYCoefficient[0] );
    poStmt->Bind( &dfYCoefficient[1] );
    poStmt->Bind( &dfYCoefficient[2] );

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
                                        bool bInternal )
{
    nPyramidMaxLevel = nLevels;

    if( bInternal )
    {
        OWStatement* poStmt = poConnection->CreateStatement( CPLSPrintf(
            "DECLARE\n"
            "  gr sdo_georaster;\n"
            "BEGIN\n"
            "  SELECT %s INTO gr FROM %s t WHERE %s FOR UPDATE;\n"
            "  sdo_geor.generatePyramid(gr, 'rlevel=%d resampling=%s');\n"
            "  UPDATE %s t SET %s = gr WHERE %s;\n"
            "  COMMIT;\n"
            "END;\n",
                sColumn.c_str(),
                sTable.c_str(),
                sWhere.c_str(),
                nLevels,
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

    OWStatement* poStmt = NULL;

    poStmt = poConnection->CreateStatement(
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
        CPLError( CE_Failure, CPLE_AppDefined,
            "Failure to initialize Level %d", nLevels );
        return false;
    }

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

bool GeoRasterWrapper::DeletePyramid()
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
    
    poStmt->Execute();

    delete poStmt;
    return false;
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

    FILE *fpImage = VSIFOpenL( pszMemFile, "wb" );

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
    unsigned long nSize = VSIFReadL( pabyCompressBuf, 1, nBlockBytes, fpImage );
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

    int nRet = compress( pabyCompressBuf, &nLen, pabyBuf, nBlockBytes );

    CPLFree( pabyBuf );

    if( nRet != Z_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "ZLib return code (%d)", nRet );
        return 0;
    }

    return nLen;
}
