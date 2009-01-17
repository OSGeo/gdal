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
#include <zlib.h>

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
    nCellSizeBits       = 0;
    nCellSizeGDAL       = 0;
    dfXCoefficient[0]   = 1.0;
    dfXCoefficient[1]   = 0.0;
    dfXCoefficient[2]   = 0.0;
    dfYCoefficient[0]   = 0.0;
    dfYCoefficient[1]   = 1.0;
    dfYCoefficient[2]   = 0.0;
    pszCellDepth        = NULL;
    pszCompressionType  = NULL;
    pahLocator          = NULL;
    pabyBlockBuf        = NULL;
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
    bBlobInitialized    = false;
    bFlushMetadata      = false;
    bFlushBuffer        = false;
    nSRID               = -1;
    bRDTRIDOnly         = false;
    bPackingOrCompress  = 0;
    nPyramidMaxLevel    = 0;
    nBlockCount         = 0;
}

//  ---------------------------------------------------------------------------
//                                                           GeoRasterDataset()
//  ---------------------------------------------------------------------------

GeoRasterWrapper::~GeoRasterWrapper()
{
    FlushMetadata();

    CPLFree( pszTable );
    CPLFree( pszColumn );
    CPLFree( pszDataTable );
    CPLFree( pszWhere );
    CPLFree( pszCellDepth );
    CPLFree( pszCompressionType );
    CPLFree( pabyBlockBuf );
    delete poStmtRead;
    delete poStmtWrite;
    CPLDestroyXMLNode( phMetadata );
    OWStatement::Free( pahLocator, nBlockCount );
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
        //  Borrow the first Table/Column found as a reference
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
        delete poStmt;
        return poGRW;
    }

    //  -------------------------------------------------------------------
    //  Read Metadata XML in text form
    //  -------------------------------------------------------------------

    char* pszXML = NULL;

    pszXML = poStmt->ReadClob( phLocator );

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
                "compression=%s ",
                nColumnBlockSize, nRowBlockSize,
                pszCellDepth,
                szInterleaving,
                pszCompressionType ) );
        }
        else
        {
            strcpy( szFormat, CPLSPrintf(
                "blockSize=(%d, %d, %d) "
                "cellDepth=%s "
                "interleaving=%s "
                "pyramid=FALSE "
                "compression=%s ",
                nColumnBlockSize, nRowBlockSize, nBandBlockSize,
                pszCellDepth,
                szInterleaving,
                pszCompressionType ) );
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
            "  CNT NUMBER        := 0;\n"
            "BEGIN\n"
            "  EXECUTE IMMEDIATE 'SELECT COUNT(*) FROM USER_TABLES\n"
            "    WHERE TABLE_NAME = :1 ' INTO CNT USING TAB;\n"
            "\n"
            "  IF CNT = 0 THEN\n"
            "    EXECUTE IMMEDIATE 'CREATE TABLE '||TAB||' %s';\n"
            "    SDO_GEOR_UTL.createDMLTrigger( TAB,  COL );\n"
            "  END IF;\n"
            "END;", szDescription ) );

        poStmt->Bind( pszTable );
        poStmt->Bind( pszColumn );

        if( ! poStmt->Execute() )
        {
            delete ( poStmt );
            CPLError( CE_Failure, CPLE_AppDefined, "Create Table Error!" );
            return false;
        }

        delete poStmt;
    }

    //  -----------------------------------------------------------
    //  Perare UPDATE or INSERT comand
    //  -----------------------------------------------------------

    char szCommand[OWTEXT];

    if( bUpdate )
    {
        strcpy( szCommand, CPLSPrintf( 
            "UPDATE %s T SET %s = GR1 WHERE %s RETURNING %s INTO GR1;",
            pszTable, pszColumn, pszWhere, pszColumn ) );
    }
    else
    {
        strcpy( szCommand, CPLSPrintf(
            "INSERT INTO %s %s RETURNING %s INTO GR1;",
            pszTable, szInsert, pszColumn ) );
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
            "  EXECUTE IMMEDIATE 'SELECT COUNT(*) FROM USER_OBJECT_TABLES\n"
            "    WHERE TABLE_NAME = :1' INTO CNT USING :rdt;\n"
            "\n"
            "  IF CNT = 0 THEN\n"
            "    EXECUTE IMMEDIATE 'CREATE TABLE '||:rdt||' OF MDSYS.SDO_RASTER\n"
            "      (PRIMARY KEY (RASTERID, PYRAMIDLEVEL, BANDBLOCKNUMBER,\n"
            "      ROWBLOCKNUMBER, COLUMNBLOCKNUMBER))\n"
            "      LOB(RASTERBLOCK) STORE AS (NOCACHE NOLOGGING)';\n"
            "  END IF;\n"
            "\n"
            "  SDO_GEOR.createTemplate(GR1, %s, null, 'TRUE');\n"
            "\n"
            "  UPDATE %s T SET %s = GR1 WHERE"
            " T.%s.RasterDataTable = :rdt AND"
            " T.%s.RasterId = :rid;\n"
            "END;\n",
            szCreateBlank,
            szCommand,
            szFormat,
            pszTable, pszColumn, pszColumn, pszColumn  ) );

        poStmt->Bind( pszTable );
        poStmt->Bind( pszColumn );
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
        "  SELECT %s INTO GR2 FROM %s T WHERE"
        " T.%s.RasterDataTable = :rdt AND"
        " T.%s.RasterId = :rid FOR UPDATE;\n"
        "  SELECT %s INTO GR1 FROM %s T WHERE"
        " T.%s.RasterDataTable = :rdt AND"
        " T.%s.RasterId = :rid;\n"
        "  SDO_GEOR.changeFormatCopy(GR1, '%s', GR2);\n"
        "  UPDATE %s T SET %s = GR2     WHERE"
        " T.%s.RasterDataTable = :rdt AND"
        " T.%s.RasterId = :rid;\n"
        "\n"
        "  EXECUTE IMMEDIATE 'SELECT COUNT(*) FROM USER_OBJECT_TABLES\n"
        "    WHERE TABLE_NAME = :1' INTO CNT USING :rdt;\n"
        "\n"
        "  IF CNT = 0 THEN\n"
        "    EXECUTE IMMEDIATE 'CREATE TABLE '||:rdt||' OF MDSYS.SDO_RASTER\n"
        "      (PRIMARY KEY (RASTERID, PYRAMIDLEVEL, BANDBLOCKNUMBER,\n"
        "      ROWBLOCKNUMBER, COLUMNBLOCKNUMBER))\n"
        "      LOB(RASTERBLOCK) STORE AS (NOCACHE NOLOGGING)';\n"
        "  ELSE\n"
        "    EXECUTE IMMEDIATE 'DELETE FROM '||:rdt||' WHERE RASTERID ='||:rid||' ';\n" 
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
        szCreateBlank,
        szCommand,
        pszColumn, pszTable, pszColumn, pszColumn,
        pszColumn, pszTable, pszColumn, pszColumn, szFormat,
        pszTable, pszColumn, pszColumn, pszColumn  ) );

    poStmt->Bind( &nColumnBlockSize );
    poStmt->Bind( &nRowBlockSize );
    poStmt->Bind( &nTotalBandBlocks );
    poStmt->Bind( &nTotalRowBlocks );
    poStmt->Bind( &nTotalColumnBlocks );
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
//                                                                     Delete()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::Delete( void )
{
    OWStatement* poStmt = poConnection->CreateStatement( CPLSPrintf(
      "UPDATE %s SET %s = NULL WHERE %s\n", pszTable, pszColumn, pszWhere ) );

    bool bReturn = poStmt->Execute();

    delete poStmt;

    return bReturn;
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

    sscanf( pszCellDepth, "%dBIT", &nCellSizeBits );

    GDALDataType eType  = OWGetDataType( pszCellDepth );

    nCellSizeGDAL       = GDALGetDataTypeSize( eType ) / 8;

    //  -------------------------------------------------------------------
    //  Get compression type
    //  -------------------------------------------------------------------

    if( EQUAL( pszCellDepth, "1BIT" ) ||
        EQUAL( pszCellDepth, "2BIT" ) ||
        EQUAL( pszCellDepth, "4BIT" ) )
    {
        bPackingOrCompress = true;
    }

    pszCompressionType  = CPLStrdup( CPLGetXMLValue( phMetadata,
                            "rasterInfo.compression.type", "NONE" ) );

    if( EQUAL( pszCompressionType, "JPEG-B" ) )
    {
        bPackingOrCompress = true;
    }
    else if ( EQUAL( pszCompressionType, "JPEG-F" ) )
    {
        bPackingOrCompress = true;
    }
    else if ( EQUAL( pszCompressionType, "DEFLATE" ) )
    {
        bPackingOrCompress = true;
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

bool GeoRasterWrapper::InitializeIO( int nLevel, bool bUpdate )
{
    // --------------------------------------------------------------------
    // Calculate the actual size of a lower resolution block
    // --------------------------------------------------------------------

    if( nLevel )
    {
        nTotalColumnBlocks  = atoi( CPLGetXMLValue( phMetadata,
                                "rasterInfo.blocking.totalColumnBlocks","0") );
        nTotalRowBlocks     = atoi( CPLGetXMLValue( phMetadata,
                                "rasterInfo.blocking.totalRowBlocks", "0" ) );

        double dfScale      = pow( (double) 2.0, (double) nLevel );

        int nPyramidRows         = (int) ceil( (double) ( nRasterRows / dfScale ) );
        int nPyramidColumns      = (int) ceil( (double) ( nRasterColumns / dfScale ) );
        int nHalfBlockRows       = (int) ceil( (double) ( nRowBlockSize / 2 ) );
        int nHalfBlockColumns    = (int) ceil( (double) ( nColumnBlockSize / 2 ) );

        // There is problably a easier way to do that math ...

        if( nPyramidRows    <= nHalfBlockRows ||
            nPyramidColumns <= nHalfBlockColumns )
        {
            nColumnBlockSize    = nPyramidColumns;
            nRowBlockSize       = nPyramidRows;
        }

        nTotalColumnBlocks  = (int) ceil( (double) ( nTotalColumnBlocks / dfScale ) );
        nTotalRowBlocks     = (int) ceil( (double) ( nTotalRowBlocks / dfScale ) );
    }

    // --------------------------------------------------------------------
    // Calculate number and size of the BLOB blocks
    // --------------------------------------------------------------------

    nBlockCount     = nTotalColumnBlocks * nTotalRowBlocks * nTotalBandBlocks;

    nBlockBytes     = nColumnBlockSize   * nRowBlockSize   * nBandBlockSize *
                      nCellSizeBits / 8;

    nBlockBytesGDAL = nColumnBlockSize   * nRowBlockSize   * nCellSizeGDAL;

    // --------------------------------------------------------------------
    // Allocate buffer for one raster block
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

    //  --------------------------------------------------------------------
    //  Issue a statement to load the locators
    //  --------------------------------------------------------------------

    const char* pszUpdate = "\0";
    OWStatement* poStmt = NULL;

    if( bUpdate )
    {
        pszUpdate = "\nFOR UPDATE";
    }

    poStmt = poConnection->CreateStatement( CPLSPrintf(
        "SELECT RASTERBLOCK\n"
        "FROM   %s\n"
        "WHERE  RASTERID = :1 AND\n"
        "       PYRAMIDLEVEL = :3\n"
        "ORDER BY\n"
        "       BANDBLOCKNUMBER ASC,\n"
        "       ROWBLOCKNUMBER ASC,\n"
        "       COLUMNBLOCKNUMBER ASC%s",
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
        CPLDebug("GEOR","Pyramid level (%d)", nLevel);
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
                return false;
        }
    }

    if( bPackingOrCompress )
    {
        //  ---------------------------------------------------------------
        //  Unpack NBits
        //  ---------------------------------------------------------------

        if( EQUAL( pszCellDepth, "1BIT" ) ||
            EQUAL( pszCellDepth, "2BIT" ) ||
            EQUAL( pszCellDepth, "4BIT" ) )
        {
            UnpackNBits( pabyBlockBuf );
        }

        //  ----------------------------------------------------------------
        //  Uncompress
        //  ----------------------------------------------------------------

        if( EQUAL( pszCompressionType, "JPEG-B" ) )
        {
            UncompressJpeg( nBytesRead );
        }
        else if ( EQUAL( pszCompressionType, "JPEG-F" ) )
        {
            UncompressJpeg( nBytesRead );
        }
        else if ( EQUAL( pszCompressionType, "DEFLATE" ) )
        {
            UncompressDeflate( nBlockBytes, nBytesRead );
        }
    }

    //  --------------------------------------------------------------------
    //  Uninterleave it if necessary
    //  --------------------------------------------------------------------

    int nStart = ( nBand - 1 ) % nBandBlockSize;

    if( EQUAL( szInterleaving, "BSQ" ) )
    {
        nStart *= nBlockBytesGDAL;

        memcpy( pData, &pabyBlockBuf[nStart], nBlockBytesGDAL );
    }
    else
    {
        int nIncr   = nBandBlockSize * nCellSizeGDAL;
        int nSize   = nCellSizeGDAL;

        if( EQUAL( szInterleaving, "BIL" ) )
        {
            nStart  *= nColumnBlockSize;
            nIncr   *= nColumnBlockSize;
            nSize   *= nColumnBlockSize;
        }

        GByte* pabyData = (GByte*) pData;
        unsigned long ii = 0;
        unsigned long jj = nStart;
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
//                                                               SetDataBlock()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::SetDataBlock( int nBand,
                                     int nLevel,
                                     int nXOffset,
                                     int nYOffset,
                                     void* pData )
{
    if( bIOInitialized == false || nCurrentLevel != nLevel )
    {
        InitializeIO( nLevel, true );

        nCurrentLevel = nLevel;
    }

    int nBlock = CALCULATEBLOCK( nBand, nXOffset, nYOffset, nBandBlockSize,
                                 nTotalColumnBlocks, nTotalRowBlocks );

    GByte *pabyOutBuf = (GByte *) pData;

    //  --------------------------------------------------------------------
    //  Pack NBits
    //  --------------------------------------------------------------------

    if( bPackingOrCompress )
    {
        if( EQUAL( pszCellDepth, "1BIT" ) ||
            EQUAL( pszCellDepth, "2BIT" ) ||
            EQUAL( pszCellDepth, "4BIT" ) )
        {
            PackNBits( pabyBlockBuf, pData );
        }
    }

    //  --------------------------------------------------------------------
    //  Interleave it if necessary
    //  --------------------------------------------------------------------

    int nStart = ( nBand - 1 ) % nBandBlockSize;

    if( EQUAL( szInterleaving, "BSQ" ) )
    {
        nStart *= nBlockBytesGDAL;

        memcpy( &pabyBlockBuf[nStart], pabyOutBuf, nBlockBytesGDAL );
    }
    else
    {
        int nIncr   = nBandBlockSize * nCellSizeGDAL;
        int nSize   = nCellSizeGDAL;

        if( EQUAL( szInterleaving, "BIL" ) )
        {
            nStart  *= nColumnBlockSize;
            nIncr   *= nColumnBlockSize;
            nSize   *= nColumnBlockSize;
        }

        unsigned long ii = 0;
        unsigned long jj = nStart;
        for( ii = 0; ii < nBlockBytesGDAL;
             ii += ( nCellSizeGDAL * nSize ),
             jj += nIncr )
        {
            memcpy( &pabyBlockBuf[jj], &pabyOutBuf[ii], nSize );
        }
    }

    //  --------------------------------------------------------------------
    //  Compress
    //  --------------------------------------------------------------------

    if( bPackingOrCompress )
    {
        if( EQUAL( pszCompressionType, "JPEG-B" ) )
        {
            CompressJpeg();
        }
        else if ( EQUAL( pszCompressionType, "JPEG-F" ) )
        {
            CompressJpeg();
        }
        else if ( EQUAL( pszCompressionType, "DEFLATE" ) )
        {
            CompressDeflate( pData );
        }
    }

    if( ! poStmtWrite->WriteBlob( pahLocator[nBlock],
                                  pabyBlockBuf,
                                  nBlockBytes ) )
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
    //  Update the Metadata directly from the XML text
    //  --------------------------------------------------------------------

    int nModelCoordinateLocation = 0;

    if( ! OW_DEFAULT_CENTER )
    {
        nModelCoordinateLocation = 1;
    }

    char* pszXML = CPLSerializeXMLTree( phMetadata );

    OWStatement* poStmt = poConnection->CreateStatement( CPLSPrintf(
        "DECLARE\n"
        "  GR1  sdo_georaster;\n"
        "  SRID number;\n"
        "BEGIN\n"
        "\n"
        "  SELECT %s INTO GR1 FROM %s T WHERE %s FOR UPDATE;\n"
        "\n"
        "  GR1.metadata := XMLTYPE(:1);\n"
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
        "  UPDATE %s T SET %s = GR1 WHERE %s;\n"
        "\n"
        "  COMMIT;\n"
        "END;", 
        pszColumn, pszTable, pszWhere,
        UNKNOWN_CRS,
        UNKNOWN_CRS,
        pszTable, pszColumn, pszWhere ) );

    poStmt->Bind( pszXML, strlen( pszXML ) + 1);
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
        CPLFree( pszXML );
        delete poStmt;
        return false;
    }

    CPLFree( pszXML );

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
//                                                            UnpackNBits()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::UnpackNBits( GByte* pabyData )
{
    int nPixCount =  nColumnBlockSize * nRowBlockSize;

    if( EQUAL( pszCellDepth, "4BIT" ) )
    {
        int  ii = 0;
        for( ii = nPixCount; ii >= 0; ii -= 2 )
        {
            int k = ii>>1;
            pabyData[ii+1] = (pabyData[k]>>4) & 0xf;
            pabyData[ii]   = (pabyData[k]) & 0xf;
        }
    }

    if( EQUAL( pszCellDepth, "2BIT" ) )
    {
        int  ii = 0;
        for( ii = nPixCount; ii >= 0; ii -= 4 )
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
        int  ii = 0;
        for( ii = nPixCount; ii >= 0; ii-- )
        {
            if( ( pabyData[ii>>3] & ( 1 << (ii & 0x7) ) ) )
                pabyData[ii] = 1;
            else
                pabyData[ii] = 0;
        }
    }
}

//  ---------------------------------------------------------------------------
//                                                                  PackNBits()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::PackNBits( GByte* pabyOutBuf, void* pData )
{
    if( EQUAL( pszCellDepth, "1BIT" ) ||
        EQUAL( pszCellDepth, "2BIT" ) ||
        EQUAL( pszCellDepth, "4BIT" ) )
    {
        int nPixCount =  nColumnBlockSize * nRowBlockSize;
        pabyOutBuf = (GByte *) VSIMalloc( nColumnBlockSize * nRowBlockSize );

        if (pabyOutBuf == NULL)
        {
            return;
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
}

//  ---------------------------------------------------------------------------
//                                                             UncompressJpeg()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::UncompressJpeg( unsigned long nInSize )
{
    const char* pszFname = CPLSPrintf( "/vsimem/jpeg_%p.virtual", pabyBlockBuf );

    // Write jpeg file in memory

    FILE *fp = VSIFOpenL( pszFname, "w+" );
    VSIFWriteL( pabyBlockBuf, 1, nInSize, fp );
    VSIFCloseL( fp );

    // Read all jpeg at once

    GDALDatasetH hDS = GDALOpen( pszFname, GA_ReadOnly );

    GDALDatasetRasterIO( hDS, GF_Read, 0, 0,
        nColumnBlockSize, nRowBlockSize, pabyBlockBuf,
        nColumnBlockSize, nRowBlockSize, GDT_Byte, nBandBlockSize, NULL, 0, 0, 0 );

    GDALClose( hDS );

    VSIUnlink( pszFname );
}

//  ---------------------------------------------------------------------------
//                                                               CompressJpeg()
//  ---------------------------------------------------------------------------

void GeoRasterWrapper::CompressJpeg()
{
    // Create a GDAL MEM driver identification string

    const char* pszFname1 = CPLSPrintf(
        "MEM:::DATAPOINTER=%p,PIXELS=%d,LINES=%d,"
        "BANDS=%d,DATATYPE=%s,PIXELOFFSET=%d,LINEOFFSET=%d,BANDOFFSET=%d",
        pabyBlockBuf, nColumnBlockSize, nRowBlockSize, nBandBlockSize, "Byte", 1,
        nRowBlockSize, 1 );

    // Open buffer as an in-memory dataset

    GDALDatasetH hDS1 = GDALOpen( pszFname1, GA_ReadOnly );

    // CreateCopy it to a JPEG file in memory

    const char* pszFname2 = CPLSPrintf( "/vsimem/jpeg_%p.virtual", pabyBlockBuf );

    GDALDriverH hDR1 = GDALGetDriverByName( "JPEG" );

    GDALDatasetH hDS2 = GDALCreateCopy( hDR1, pszFname2, hDS1,
        false, NULL, NULL, NULL );

    GDALClose( hDS1 );

    GDALClose( hDS2 );

    // Copy the memory file to the main buffer

    FILE *fp = VSIFOpenL( pszFname2, "r+" );
    VSIFReadL( pabyBlockBuf, 1, nBlockBytes, fp );
    VSIFCloseL( fp );

    VSIUnlink( pszFname2 );
}


//  ---------------------------------------------------------------------------
//                                                          UncompressDeflate()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::UncompressDeflate( unsigned long nDestLen,
                                          unsigned long nSourceLen )
{
    GByte* pabyBuf = (GByte*) CPLMalloc( nSourceLen );

    memcpy( pabyBuf, pabyBlockBuf, nSourceLen );

    // Call ZLib uncompress

    int nRet = uncompress( pabyBlockBuf, &nDestLen, pabyBuf, nSourceLen );

    CPLFree( pabyBuf );

    if( nRet != Z_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "ZLib return code (%d)", nRet );
        return false;
    }

    return true;
}

//  ---------------------------------------------------------------------------
//                                                            CompressDeflate()
//  ---------------------------------------------------------------------------

bool GeoRasterWrapper::CompressDeflate( void* pData )
{
    unsigned long nLen = ((unsigned long)(nBlockBytes * 1.1)) + 12;

    GByte* pabyBuf = (GByte*) CPLMalloc( nLen );

    memcpy( pabyBuf, pabyBlockBuf, nBlockBytes );

    // Call ZLib compress

    int nRet = compress( pabyBlockBuf, &nLen, (GByte*) pData, nBlockBytes );

    CPLFree( pabyBuf );

    if( nRet != Z_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "ZLib return code (%d)", nRet );
        return false;
    }

    return true;
}
