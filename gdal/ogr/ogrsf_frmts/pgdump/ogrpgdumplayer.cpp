/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGDumpLayer class
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault
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

#include "ogr_pgdump.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

#define USE_COPY_UNSET -1

/* Flags for creating WKB format for PostGIS */
#define WKBZOFFSET 0x80000000
#define WKBMOFFSET 0x40000000
#define WKBSRIDFLAG 0x20000000
#define WKBBBOXFLAG 0x10000000

static CPLString OGRPGDumpEscapeStringList(
                                       char** papszItems, int bForInsertOrUpdate);

/************************************************************************/
/*                        OGRPGDumpLayer()                              */
/************************************************************************/

OGRPGDumpLayer::OGRPGDumpLayer(OGRPGDumpDataSource* poDS,
                               const char* pszSchemaName,
                               const char* pszTableName,
                               const char* pszGeomColumn,
                               const char *pszFIDColumn,
                               int         nCoordDimension,
                               int         nSRSId,
                               int         bWriteAsHexIn,
                               int         bCreateTable)
{
    this->poDS = poDS;
    poFeatureDefn = new OGRFeatureDefn( pszTableName );
    poFeatureDefn->Reference();
    nFeatures = 0;
    pszSqlTableName = CPLStrdup(CPLString().Printf("%s.%s",
                               OGRPGDumpEscapeColumnName(pszSchemaName).c_str(),
                               OGRPGDumpEscapeColumnName(pszTableName).c_str() ));
    this->pszGeomColumn = (pszGeomColumn) ? CPLStrdup(pszGeomColumn) : NULL;
    this->pszFIDColumn = CPLStrdup(pszFIDColumn);
    this->nCoordDimension = nCoordDimension;
    this->nSRSId = nSRSId;
    this->bCreateTable = bCreateTable;
    bLaunderColumnNames = TRUE;
    bPreservePrecision = TRUE;
    bUseCopy = USE_COPY_UNSET;
    bWriteAsHex = bWriteAsHexIn;
    bCopyActive = FALSE;
    papszHSTOREColumns = NULL;
}

/************************************************************************/
/*                          ~OGRPGDumpLayer()                           */
/************************************************************************/

OGRPGDumpLayer::~OGRPGDumpLayer()
{
    EndCopy();

    poFeatureDefn->Release();
    CPLFree(pszSqlTableName);
    CPLFree(pszGeomColumn);
    CPLFree(pszFIDColumn);
    CSLDestroy(papszHSTOREColumns);
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRPGDumpLayer::GetNextFeature()
{
    CPLError(CE_Failure, CPLE_NotSupported, "PGDump driver is write only");
    return NULL;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

int OGRPGDumpLayer::TestCapability( const char * pszCap )
{
    if( EQUAL(pszCap,OLCSequentialWrite) ||
        EQUAL(pszCap,OLCCreateField) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                           GeometryToHex()                            */
/************************************************************************/

char *OGRPGDumpLayer::GeometryToHex( OGRGeometry * poGeometry, int nSRSId )
{
    GByte       *pabyWKB;
    char        *pszTextBuf;
    char        *pszTextBufCurrent;
    char        *pszHex;

    int nWkbSize = poGeometry->WkbSize();
    pabyWKB = (GByte *) CPLMalloc(nWkbSize);

    if( poGeometry->exportToWkb( wkbNDR, pabyWKB ) != OGRERR_NONE )
    {
        CPLFree( pabyWKB );
        return CPLStrdup("");
    }

    /* When converting to hex, each byte takes 2 hex characters.  In addition
       we add in 8 characters to represent the SRID integer in hex, and
       one for a null terminator */

    int pszSize = nWkbSize*2 + 8 + 1;
    pszTextBuf = (char *) CPLMalloc(pszSize);
    pszTextBufCurrent = pszTextBuf;

    /* Convert the 1st byte, which is the endianess flag, to hex. */
    pszHex = CPLBinaryToHex( 1, pabyWKB );
    strcpy(pszTextBufCurrent, pszHex );
    CPLFree ( pszHex );
    pszTextBufCurrent += 2;

    /* Next, get the geom type which is bytes 2 through 5 */
    GUInt32 geomType;
    memcpy( &geomType, pabyWKB+1, 4 );

    /* Now add the SRID flag if an SRID is provided */
    if (nSRSId > 0)
    {
        /* Change the flag to wkbNDR (little) endianess */
        GUInt32 nGSrsFlag = CPL_LSBWORD32( WKBSRIDFLAG );
        /* Apply the flag */
        geomType = geomType | nGSrsFlag;
    }

    /* Now write the geom type which is 4 bytes */
    pszHex = CPLBinaryToHex( 4, (GByte*) &geomType );
    strcpy(pszTextBufCurrent, pszHex );
    CPLFree ( pszHex );
    pszTextBufCurrent += 8;

    /* Now include SRID if provided */
    if (nSRSId > 0)
    {
        /* Force the srsid to wkbNDR (little) endianess */
        GUInt32 nGSRSId = CPL_LSBWORD32( nSRSId );
        pszHex = CPLBinaryToHex( sizeof(nGSRSId),(GByte*) &nGSRSId );
        strcpy(pszTextBufCurrent, pszHex );
        CPLFree ( pszHex );
        pszTextBufCurrent += 8;
    }

    /* Copy the rest of the data over - subtract
       5 since we already copied 5 bytes above */
    pszHex = CPLBinaryToHex( nWkbSize - 5, pabyWKB + 5 );
    strcpy(pszTextBufCurrent, pszHex );
    CPLFree ( pszHex );

    CPLFree( pabyWKB );

    return pszTextBuf;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRErr OGRPGDumpLayer::CreateFeature( OGRFeature *poFeature )
{
    if( NULL == poFeature )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "NULL pointer to OGRFeature passed to CreateFeature()." );
        return OGRERR_FAILURE;
    }

    nFeatures ++;

    // We avoid testing the config option too often. 
    if( bUseCopy == USE_COPY_UNSET )
        bUseCopy = CSLTestBoolean( CPLGetConfigOption( "PG_USE_COPY", "NO") );

    if( !bUseCopy )
    {
        return CreateFeatureViaInsert( poFeature );
    }
    else
    {
        if ( !bCopyActive )
            StartCopy();

        return CreateFeatureViaCopy( poFeature );
    }
}

/************************************************************************/
/*                       CreateFeatureViaInsert()                       */
/************************************************************************/

OGRErr OGRPGDumpLayer::CreateFeatureViaInsert( OGRFeature *poFeature )

{
    CPLString           osCommand;
    int                 i = 0;
    int                 bNeedComma = FALSE;
    OGRErr              eErr = OGRERR_FAILURE;
    int bEmptyInsert = FALSE;
    
    if( NULL == poFeature )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "NULL pointer to OGRFeature passed to CreateFeatureViaInsert()." );
        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Form the INSERT command.                                        */
/* -------------------------------------------------------------------- */
    osCommand.Printf( "INSERT INTO %s (", pszSqlTableName );

    OGRGeometry *poGeom = poFeature->GetGeometryRef();
    if( poGeom != NULL && pszGeomColumn != NULL )
    {
        osCommand = osCommand + OGRPGDumpEscapeColumnName(pszGeomColumn) + " ";
        bNeedComma = TRUE;
    }

    if( poFeature->GetFID() != OGRNullFID && pszFIDColumn != NULL )
    {
        if( bNeedComma )
            osCommand += ", ";
        
        osCommand = osCommand + OGRPGDumpEscapeColumnName(pszFIDColumn) + " ";
        bNeedComma = TRUE;
    }

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if( !poFeature->IsFieldSet( i ) )
            continue;

        if( !bNeedComma )
            bNeedComma = TRUE;
        else
            osCommand += ", ";

        osCommand = osCommand 
            + OGRPGDumpEscapeColumnName(poFeatureDefn->GetFieldDefn(i)->GetNameRef());
    }

    if (!bNeedComma)
        bEmptyInsert = TRUE;

    osCommand += ") VALUES (";

    /* Set the geometry */
    bNeedComma = FALSE;
    if( poGeom != NULL && pszGeomColumn != NULL )
    {
        char    *pszWKT = NULL;

        poGeom->closeRings();
        poGeom->setCoordinateDimension( nCoordDimension );

        if( bWriteAsHex )
        {
            char* pszHex = GeometryToHex( poGeom, nSRSId );
            osCommand += "'";
            if (pszHex)
                osCommand += pszHex;
            osCommand += "'";
            CPLFree(pszHex);
        }
        else
        {
            poGeom->exportToWkt( &pszWKT );

            if( pszWKT != NULL )
            {
                osCommand +=
                    CPLString().Printf(
                        "GeomFromEWKT('SRID=%d;%s'::TEXT) ", nSRSId, pszWKT );
                OGRFree( pszWKT );
            }
            else
                osCommand += "''";
        }

        bNeedComma = TRUE;
    }

    /* Set the FID */
    if( poFeature->GetFID() != OGRNullFID && pszFIDColumn != NULL )
    {
        if( bNeedComma )
            osCommand += ", ";
        osCommand += CPLString().Printf( "%ld ", poFeature->GetFID() );
        bNeedComma = TRUE;
    }


    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if( !poFeature->IsFieldSet( i ) )
            continue;

        if( bNeedComma )
            osCommand += ", ";
        else
            bNeedComma = TRUE;

        AppendFieldValue(osCommand, poFeature, i);
    }

    osCommand += ")";

    if (bEmptyInsert)
        osCommand.Printf( "INSERT INTO %s DEFAULT VALUES", pszSqlTableName );

/* -------------------------------------------------------------------- */
/*      Execute the insert.                                             */
/* -------------------------------------------------------------------- */
    poDS->Log(osCommand);

    return OGRERR_NONE;
}


/************************************************************************/
/*                        CreateFeatureViaCopy()                        */
/************************************************************************/

OGRErr OGRPGDumpLayer::CreateFeatureViaCopy( OGRFeature *poFeature )
{
    CPLString            osCommand;

    /* First process geometry */
    OGRGeometry *poGeometry = (OGRGeometry *) poFeature->GetGeometryRef();
    
    if (pszGeomColumn)
    {
        char *pszGeom = NULL;
        if ( NULL != poGeometry /* && (bHasWkb || bHasPostGISGeometry || bHasPostGISGeography) */)
        {
            poGeometry->closeRings();
            poGeometry->setCoordinateDimension( nCoordDimension );
            
            //CheckGeomTypeCompatibility(poGeometry);
    
            /*if (bHasWkb)
                pszGeom = GeometryToBYTEA( poGeometry );
            else*/
                pszGeom = GeometryToHex( poGeometry, nSRSId );
        }
    
        if ( pszGeom )
        {
            osCommand += pszGeom,
            CPLFree( pszGeom );
        }
        else
        {
            osCommand = "\\N";
        }
    }

    /* Next process the field id column */
    if( /*bHasFid &&*/ poFeatureDefn->GetFieldIndex( pszFIDColumn ) != -1 )
    {
        if (osCommand.size() > 0)
            osCommand += "\t";
            
        /* Set the FID */
        if( poFeature->GetFID() != OGRNullFID )
        {
            osCommand += CPLString().Printf("%ld ", poFeature->GetFID());
        }
        else
        {
            osCommand += "\\N" ;
        }
    }


    /* Now process the remaining fields */

    int nFieldCount = poFeatureDefn->GetFieldCount();
    for( int i = 0; i < nFieldCount;  i++ )
    {
        const char *pszStrValue = poFeature->GetFieldAsString(i);
        char *pszNeedToFree = NULL;

        if (i > 0 || osCommand.size() > 0)
            osCommand += "\t";
            
        if( !poFeature->IsFieldSet( i ) )
        {
            osCommand += "\\N" ;

            continue;
        }

        int nOGRFieldType = poFeatureDefn->GetFieldDefn(i)->GetType();

        // We need special formatting for integer list values.
        if( nOGRFieldType == OFTIntegerList )
        {
            int nCount, nOff = 0, j;
            const int *panItems = poFeature->GetFieldAsIntegerList(i,&nCount);

            pszNeedToFree = (char *) CPLMalloc(nCount * 13 + 10);
            strcpy( pszNeedToFree, "{" );
            for( j = 0; j < nCount; j++ )
            {
                if( j != 0 )
                    strcat( pszNeedToFree+nOff, "," );

                nOff += strlen(pszNeedToFree+nOff);
                sprintf( pszNeedToFree+nOff, "%d", panItems[j] );
            }
            strcat( pszNeedToFree+nOff, "}" );
            pszStrValue = pszNeedToFree;
        }

        // We need special formatting for real list values.
        else if( nOGRFieldType == OFTRealList )
        {
            int nCount, nOff = 0, j;
            const double *padfItems =poFeature->GetFieldAsDoubleList(i,&nCount);

            pszNeedToFree = (char *) CPLMalloc(nCount * 40 + 10);
            strcpy( pszNeedToFree, "{" );
            for( j = 0; j < nCount; j++ )
            {
                if( j != 0 )
                    strcat( pszNeedToFree+nOff, "," );

                nOff += strlen(pszNeedToFree+nOff);
                //Check for special values. They need to be quoted.
                if( CPLIsNan(padfItems[j]) )
                    sprintf( pszNeedToFree+nOff, "NaN" );
                else if( CPLIsInf(padfItems[j]) )
                    sprintf( pszNeedToFree+nOff, (padfItems[j] > 0) ? "Infinity" : "-Infinity" );
                else
                    sprintf( pszNeedToFree+nOff, "%.16g", padfItems[j] );

            }
            strcat( pszNeedToFree+nOff, "}" );
            pszStrValue = pszNeedToFree;
        }


        // We need special formatting for string list values.
        else if( nOGRFieldType == OFTStringList )
        {
            CPLString osStr;
            char **papszItems = poFeature->GetFieldAsStringList(i);

            pszStrValue = pszNeedToFree = CPLStrdup(OGRPGDumpEscapeStringList(papszItems, FALSE));
        }

        // Binary formatting
        else if( nOGRFieldType == OFTBinary )
        {
            int nLen = 0;
            GByte* pabyData = poFeature->GetFieldAsBinary( i, &nLen );
            char* pszBytea = GByteArrayToBYTEA( pabyData, nLen);

            pszStrValue = pszNeedToFree = pszBytea;
        }

        else if( nOGRFieldType == OFTReal )
        {
            char* pszComma = strchr((char*)pszStrValue, ',');
            if (pszComma)
                *pszComma = '.';
            //Check for special values. They need to be quoted.
            double dfVal = poFeature->GetFieldAsDouble(i);
            if( CPLIsNan(dfVal) )
                pszStrValue = "NaN";
            else if( CPLIsInf(dfVal) )
                pszStrValue = (dfVal > 0) ? "Infinity" : "-Infinity";
        }

        if( nOGRFieldType != OFTIntegerList &&
            nOGRFieldType != OFTRealList &&
            nOGRFieldType != OFTInteger &&
            nOGRFieldType != OFTReal &&
            nOGRFieldType != OFTBinary )
        {
            int         iChar;

            for( iChar = 0; pszStrValue[iChar] != '\0'; iChar++ )
            {
                if( poFeatureDefn->GetFieldDefn(i)->GetWidth() > 0
                    && iChar == poFeatureDefn->GetFieldDefn(i)->GetWidth() )
                {
                    CPLDebug( "PG",
                              "Truncated %s field value, it was too long.",
                              poFeatureDefn->GetFieldDefn(i)->GetNameRef() );
                    break;
                }

                /* Escape embedded \, \t, \n, \r since they will cause COPY
                   to misinterpret a line of text and thus abort */
                if( pszStrValue[iChar] == '\\' || 
                    pszStrValue[iChar] == '\t' || 
                    pszStrValue[iChar] == '\r' || 
                    pszStrValue[iChar] == '\n'   )
                {
                    osCommand += '\\';
                }

                osCommand += pszStrValue[iChar];
            }
        }
        else
        {
            osCommand += pszStrValue;
        }

        if( pszNeedToFree )
            CPLFree( pszNeedToFree );
    }

    /* Add end of line marker */
    //osCommand += "\n";


    /* ------------------------------------------------------------ */
    /*      Execute the copy.                                       */
    /* ------------------------------------------------------------ */

    OGRErr result = OGRERR_NONE;

    poDS->Log(osCommand, FALSE);

    return result;
}

/************************************************************************/
/*                             StartCopy()                              */
/************************************************************************/

OGRErr OGRPGDumpLayer::StartCopy()

{
    /* Tell the datasource we are now planning to copy data */
    poDS->StartCopy( this ); 

    CPLString osFields = BuildCopyFields();

    int size = strlen(osFields) +  strlen(pszSqlTableName) + 100;
    char *pszCommand = (char *) CPLMalloc(size);

    sprintf( pszCommand,
             "COPY %s (%s) FROM STDIN",
             pszSqlTableName, osFields.c_str() );

    poDS->Log(pszCommand);
    bCopyActive = TRUE;

    CPLFree( pszCommand );

    return OGRERR_NONE;
}

/************************************************************************/
/*                              EndCopy()                               */
/************************************************************************/

OGRErr OGRPGDumpLayer::EndCopy()

{
    if( !bCopyActive )
        return OGRERR_NONE;

    bCopyActive = FALSE;

    poDS->Log("\\.", FALSE);
    poDS->Log("END");

    bUseCopy = USE_COPY_UNSET;

    return OGRERR_NONE;
}

/************************************************************************/
/*                          BuildCopyFields()                           */
/************************************************************************/

CPLString OGRPGDumpLayer::BuildCopyFields()
{
    int     i = 0;
    CPLString osFieldList;

    if( /*bHasFid &&*/ poFeatureDefn->GetFieldIndex( pszFIDColumn ) != -1 )
    {
        osFieldList += OGRPGDumpEscapeColumnName(pszFIDColumn);
    }

    if( pszGeomColumn )
    {
        if( strlen(osFieldList) > 0 )
            osFieldList += ", ";

        osFieldList += OGRPGDumpEscapeColumnName(pszGeomColumn);
    }

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        const char *pszName = poFeatureDefn->GetFieldDefn(i)->GetNameRef();

        if( strlen(osFieldList) > 0 )
            osFieldList += ", ";

        osFieldList += OGRPGDumpEscapeColumnName(pszName);
    }

    return osFieldList;
}

/************************************************************************/
/*                       OGRPGDumpEscapeColumnName( )                   */
/************************************************************************/

CPLString OGRPGDumpEscapeColumnName(const char* pszColumnName)
{
    CPLString osStr;

    osStr += "\"";

    char ch;
    for(int i=0; (ch = pszColumnName[i]) != '\0'; i++)
    {
        if (ch == '"')
            osStr.append(1, ch);
        osStr.append(1, ch);
    }

    osStr += "\"";

    return osStr;
}

/************************************************************************/
/*                             EscapeString( )                          */
/************************************************************************/

CPLString OGRPGDumpEscapeString(
                                   const char* pszStrValue, int nMaxLength,
                                   const char* pszFieldName)
{
    CPLString osCommand;

    /* We need to quote and escape string fields. */
    osCommand += "'";

    int nSrcLen = strlen(pszStrValue);
    if (nMaxLength > 0 && nSrcLen > nMaxLength)
    {
        CPLDebug( "PG",
                  "Truncated %s field value, it was too long.",
                  pszFieldName );
        nSrcLen = nMaxLength;
        
        while( nSrcLen > 0 && ((unsigned char *) pszStrValue)[nSrcLen-1] > 127 )
        {
            CPLDebug( "PG", "Backup to start of multi-byte character." );
            nSrcLen--;
        }
    }

    char* pszDestStr = (char*)CPLMalloc(2 * nSrcLen + 1);

    /* -------------------------------------------------------------------- */
    /*  PQescapeStringConn was introduced in PostgreSQL security releases   */
    /*  8.1.4, 8.0.8, 7.4.13, 7.3.15                                        */
    /*  PG_HAS_PQESCAPESTRINGCONN is added by a test in 'configure'         */
    /*  so it is not set by default when building OGR for Win32             */
    /* -------------------------------------------------------------------- */
#if defined(PG_HAS_PQESCAPESTRINGCONN)
    int nError;
    PQescapeStringConn (hPGConn, pszDestStr, pszStrValue, nSrcLen, &nError);
    if (nError == 0)
        osCommand += pszDestStr;
    else
        CPLError(CE_Warning, CPLE_AppDefined, 
                 "PQescapeString(): %s\n"
                 "  input: '%s'\n"
                 "    got: '%s'\n",
                 PQerrorMessage( hPGConn ),
                 pszStrValue, pszDestStr );
#else
    //PQescapeString(pszDestStr, pszStrValue, nSrcLen);
    
    int i, j;
    for(i=0,j=0; i < nSrcLen; i++)
    {
        if (pszStrValue[i] == '\'')
        {
            pszDestStr[j++] = '\'';
            pszDestStr[j++] = '\'';
        }
        else if (pszStrValue[i] == '\\')
        {
            pszDestStr[j++] = '\\';
            pszDestStr[j++] = '\\';
        }
        else
            pszDestStr[j++] = pszStrValue[i];
    }
    pszDestStr[j] = 0;

    osCommand += pszDestStr;
#endif
    CPLFree(pszDestStr);

    osCommand += "'";

    return osCommand;
}


/************************************************************************/
/*                    OGRPGDumpEscapeStringList( )                      */
/************************************************************************/

static CPLString OGRPGDumpEscapeStringList(
                                       char** papszItems, int bForInsertOrUpdate)
{
    int bFirstItem = TRUE;
    CPLString osStr;
    if (bForInsertOrUpdate)
        osStr += "ARRAY[";
    else
        osStr += "{";
    while(*papszItems)
    {
        if (!bFirstItem)
        {
            osStr += ',';
        }

        char* pszStr = *papszItems;
        if (*pszStr != '\0')
        {
            if (bForInsertOrUpdate)
                osStr += OGRPGDumpEscapeString(pszStr, -1, "");
            else
            {
                osStr += '"';

                while(*pszStr)
                {
                    if (*pszStr == '"' )
                        osStr += "\\";
                    osStr += *pszStr;
                    pszStr++;
                }

                osStr += '"';
            }
        }
        else
            osStr += "NULL";

        bFirstItem = FALSE;

        papszItems++;
    }
    if (bForInsertOrUpdate)
        osStr += "]";
    else
        osStr += "}";
    return osStr;
}

/************************************************************************/
/*                          AppendFieldValue()                          */
/*                                                                      */
/* Used by CreateFeatureViaInsert() and SetFeature() to format a        */
/* non-empty field value                                                */
/************************************************************************/

void OGRPGDumpLayer::AppendFieldValue(CPLString& osCommand,
                                       OGRFeature* poFeature, int i)
{
    int nOGRFieldType = poFeatureDefn->GetFieldDefn(i)->GetType();

    // We need special formatting for integer list values.
    if(  nOGRFieldType == OFTIntegerList )
    {
        int nCount, nOff = 0, j;
        const int *panItems = poFeature->GetFieldAsIntegerList(i,&nCount);
        char *pszNeedToFree = NULL;

        pszNeedToFree = (char *) CPLMalloc(nCount * 13 + 10);
        strcpy( pszNeedToFree, "'{" );
        for( j = 0; j < nCount; j++ )
        {
            if( j != 0 )
                strcat( pszNeedToFree+nOff, "," );

            nOff += strlen(pszNeedToFree+nOff);
            sprintf( pszNeedToFree+nOff, "%d", panItems[j] );
        }
        strcat( pszNeedToFree+nOff, "}'" );

        osCommand += pszNeedToFree;
        CPLFree(pszNeedToFree);

        return;
    }

    // We need special formatting for real list values.
    else if( nOGRFieldType == OFTRealList )
    {
        int nCount, nOff = 0, j;
        const double *padfItems =poFeature->GetFieldAsDoubleList(i,&nCount);
        char *pszNeedToFree = NULL;

        pszNeedToFree = (char *) CPLMalloc(nCount * 40 + 10);
        strcpy( pszNeedToFree, "'{" );
        for( j = 0; j < nCount; j++ )
        {
            if( j != 0 )
                strcat( pszNeedToFree+nOff, "," );

            nOff += strlen(pszNeedToFree+nOff);
            //Check for special values. They need to be quoted.
            if( CPLIsNan(padfItems[j]) )
                sprintf( pszNeedToFree+nOff, "NaN" );
            else if( CPLIsInf(padfItems[j]) )
                sprintf( pszNeedToFree+nOff, (padfItems[j] > 0) ? "Infinity" : "-Infinity" );
            else
                sprintf( pszNeedToFree+nOff, "%.16g", padfItems[j] );

        }
        strcat( pszNeedToFree+nOff, "}'" );

        osCommand += pszNeedToFree;
        CPLFree(pszNeedToFree);

        return;
    }

    // We need special formatting for string list values.
    else if( nOGRFieldType == OFTStringList )
    {
        char **papszItems = poFeature->GetFieldAsStringList(i);

        osCommand += OGRPGDumpEscapeStringList(papszItems, TRUE);

        return;
    }

    // Binary formatting
    else if( nOGRFieldType == OFTBinary )
    {
        osCommand += "'";

        int nLen = 0;
        GByte* pabyData = poFeature->GetFieldAsBinary( i, &nLen );
        char* pszBytea = GByteArrayToBYTEA( pabyData, nLen);

        osCommand += pszBytea;

        CPLFree(pszBytea);
        osCommand += "'";

        return;
    }

    // Flag indicating NULL or not-a-date date value
    // e.g. 0000-00-00 - there is no year 0
    OGRBoolean bIsDateNull = FALSE;

    const char *pszStrValue = poFeature->GetFieldAsString(i);

    // Check if date is NULL: 0000-00-00
    if( nOGRFieldType == OFTDate )
    {
        if( EQUALN( pszStrValue, "0000", 4 ) )
        {
            pszStrValue = "NULL";
            bIsDateNull = TRUE;
        }
    }
    else if ( nOGRFieldType == OFTReal )
    {
        char* pszComma = strchr((char*)pszStrValue, ',');
        if (pszComma)
            *pszComma = '.';
        //Check for special values. They need to be quoted.
        double dfVal = poFeature->GetFieldAsDouble(i);
        if( CPLIsNan(dfVal) )
            pszStrValue = "'NaN'";
        else if( CPLIsInf(dfVal) )
            pszStrValue = (dfVal > 0) ? "'Infinity'" : "'-Infinity'";
    }

    if( nOGRFieldType != OFTInteger && nOGRFieldType != OFTReal
        && !bIsDateNull )
    {
        osCommand += OGRPGDumpEscapeString( pszStrValue,
                                        poFeatureDefn->GetFieldDefn(i)->GetWidth(),
                                        poFeatureDefn->GetFieldDefn(i)->GetNameRef() );
    }
    else
    {
        osCommand += pszStrValue;
    }
}


/************************************************************************/
/*                        GByteArrayToBYTEA()                           */
/************************************************************************/

char* OGRPGDumpLayer::GByteArrayToBYTEA( const GByte* pabyData, int nLen)
{
    char* pszTextBuf;

    pszTextBuf = (char *) CPLMalloc(nLen*5+1);

    int  iSrc, iDst=0;

    for( iSrc = 0; iSrc < nLen; iSrc++ )
    {
        if( pabyData[iSrc] < 40 || pabyData[iSrc] > 126
            || pabyData[iSrc] == '\\' )
        {
            sprintf( pszTextBuf+iDst, "\\\\%03o", pabyData[iSrc] );
            iDst += 5;
        }
        else
            pszTextBuf[iDst++] = pabyData[iSrc];
    }
    pszTextBuf[iDst] = '\0';

    return pszTextBuf;
}

/************************************************************************/
/*                        OGRPGTableLayerGetType()                      */
/************************************************************************/

static CPLString OGRPGTableLayerGetType(OGRFieldDefn& oField,
                                        int bPreservePrecision,
                                        int bApproxOK)
{
    char                szFieldType[256];

/* -------------------------------------------------------------------- */
/*      Work out the PostgreSQL type.                                   */
/* -------------------------------------------------------------------- */
    if( oField.GetType() == OFTInteger )
    {
        if( oField.GetWidth() > 0 && bPreservePrecision )
            sprintf( szFieldType, "NUMERIC(%d,0)", oField.GetWidth() );
        else
            strcpy( szFieldType, "INTEGER" );
    }
    else if( oField.GetType() == OFTReal )
    {
        if( oField.GetWidth() > 0 && oField.GetPrecision() > 0
            && bPreservePrecision )
            sprintf( szFieldType, "NUMERIC(%d,%d)",
                     oField.GetWidth(), oField.GetPrecision() );
        else
            strcpy( szFieldType, "FLOAT8" );
    }
    else if( oField.GetType() == OFTString )
    {
        if (oField.GetWidth() > 0 &&  bPreservePrecision )
            sprintf( szFieldType, "VARCHAR(%d)",  oField.GetWidth() );
        else
            strcpy( szFieldType, "VARCHAR");
    }
    else if( oField.GetType() == OFTIntegerList )
    {
        strcpy( szFieldType, "INTEGER[]" );
    }
    else if( oField.GetType() == OFTRealList )
    {
        strcpy( szFieldType, "FLOAT8[]" );
    }
    else if( oField.GetType() == OFTStringList )
    {
        strcpy( szFieldType, "varchar[]" );
    }
    else if( oField.GetType() == OFTDate )
    {
        strcpy( szFieldType, "date" );
    }
    else if( oField.GetType() == OFTTime )
    {
        strcpy( szFieldType, "time" );
    }
    else if( oField.GetType() == OFTDateTime )
    {
        strcpy( szFieldType, "timestamp with time zone" );
    }
    else if( oField.GetType() == OFTBinary )
    {
        strcpy( szFieldType, "bytea" );
    }
    else if( bApproxOK )
    {
        CPLError( CE_Warning, CPLE_NotSupported,
                  "Can't create field %s with type %s on PostgreSQL layers.  Creating as VARCHAR.",
                  oField.GetNameRef(),
                  OGRFieldDefn::GetFieldTypeName(oField.GetType()) );
        strcpy( szFieldType, "VARCHAR" );
    }
    else
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't create field %s with type %s on PostgreSQL layers.",
                  oField.GetNameRef(),
                  OGRFieldDefn::GetFieldTypeName(oField.GetType()) );
        strcpy( szFieldType, "");
    }

    return szFieldType;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRErr OGRPGDumpLayer::CreateField( OGRFieldDefn *poFieldIn,
                                     int bApproxOK )
{
    if (nFeatures != 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot create field after first feature has been written");
        return OGRERR_FAILURE;
    }
    
    CPLString           osCommand;
    CPLString           osFieldType;
    OGRFieldDefn        oField( poFieldIn );

/* -------------------------------------------------------------------- */
/*      Do we want to "launder" the column names into Postgres          */
/*      friendly format?                                                */
/* -------------------------------------------------------------------- */
    if( bLaunderColumnNames )
    {
        char    *pszSafeName = poDS->LaunderName( oField.GetNameRef() );

        oField.SetName( pszSafeName );
        CPLFree( pszSafeName );

        if( EQUAL(oField.GetNameRef(),"oid") )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Renaming field 'oid' to 'oid_' to avoid conflict with internal oid field." );
            oField.SetName( "oid_" );
        }
    }

    osFieldType = OGRPGTableLayerGetType(oField, bPreservePrecision, bApproxOK);
    if (osFieldType.size() == 0)
        return OGRERR_FAILURE;

    if( CSLFindString(papszHSTOREColumns, oField.GetNameRef()) != -1 )
        osFieldType = "hstore";

/* -------------------------------------------------------------------- */
/*      Create the new field.                                           */
/* -------------------------------------------------------------------- */
    osCommand.Printf( "ALTER TABLE %s ADD COLUMN %s %s",
                      pszSqlTableName, OGRPGDumpEscapeColumnName(oField.GetNameRef()).c_str(),
                      osFieldType.c_str() );
    if (bCreateTable)
        poDS->Log(osCommand);

    poFeatureDefn->AddFieldDefn( &oField );
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                         SetHSTOREColumns()                           */
/************************************************************************/

void OGRPGDumpLayer::SetHSTOREColumns( const char* pszHSTOREColumns )
{
    if( pszHSTOREColumns == NULL )
        return;

    papszHSTOREColumns = CSLTokenizeString2(pszHSTOREColumns, ",", 0);
}
