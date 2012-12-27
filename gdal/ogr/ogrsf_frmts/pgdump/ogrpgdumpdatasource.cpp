/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGDumpDataSource class.
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

#include <string.h>
#include "ogr_pgdump.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                      OGRPGDumpDataSource()                           */
/************************************************************************/

OGRPGDumpDataSource::OGRPGDumpDataSource(const char* pszName,
                                         char** papszOptions)

{
    nLayers = 0;
    papoLayers = NULL;
    this->pszName = CPLStrdup(pszName);
    bTriedOpen = FALSE;
    fp = NULL;
    bInTransaction = FALSE;
    poLayerInCopyMode = NULL;

    const char *pszCRLFFormat = CSLFetchNameValue( papszOptions, "LINEFORMAT");

    int bUseCRLF;
    if( pszCRLFFormat == NULL )
    {
#ifdef WIN32
        bUseCRLF = TRUE;
#else
        bUseCRLF = FALSE;
#endif
    }
    else if( EQUAL(pszCRLFFormat,"CRLF") )
        bUseCRLF = TRUE;
    else if( EQUAL(pszCRLFFormat,"LF") )
        bUseCRLF = FALSE;
    else
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "LINEFORMAT=%s not understood, use one of CRLF or LF.",
                  pszCRLFFormat );
#ifdef WIN32
        bUseCRLF = TRUE;
#else
        bUseCRLF = FALSE;
#endif
    }
    pszEOL = (bUseCRLF) ? "\r\n" : "\n";
}

/************************************************************************/
/*                          ~OGRPGDumpDataSource()                          */
/************************************************************************/

OGRPGDumpDataSource::~OGRPGDumpDataSource()

{
    int i;

    if (fp)
    {
        Commit();
        VSIFCloseL(fp);
        fp = NULL;
    }

    for(i=0;i<nLayers;i++)
        delete papoLayers[i];
    CPLFree(papoLayers);
    CPLFree(pszName);
}

/************************************************************************/
/*                         StartTransaction()                           */
/************************************************************************/

void OGRPGDumpDataSource::StartTransaction()
{
    if (bInTransaction)
        return;
    bInTransaction = TRUE;
    Log("BEGIN");
}

/************************************************************************/
/*                              Commit()                                */
/************************************************************************/

void OGRPGDumpDataSource::Commit()
{
    EndCopy();

    if (!bInTransaction)
        return;
    bInTransaction = FALSE;
    Log("COMMIT");
}

/************************************************************************/
/*                            LaunderName()                             */
/************************************************************************/

char *OGRPGDumpDataSource::LaunderName( const char *pszSrcName )

{
    char    *pszSafeName = CPLStrdup( pszSrcName );

    for( int i = 0; pszSafeName[i] != '\0'; i++ )
    {
        pszSafeName[i] = (char) tolower( pszSafeName[i] );
        if( pszSafeName[i] == '\'' || pszSafeName[i] == '-' || pszSafeName[i] == '#' )
            pszSafeName[i] = '_';
    }

    if( strcmp(pszSrcName,pszSafeName) != 0 )
        CPLDebug("PG","LaunderName('%s') -> '%s'", 
                 pszSrcName, pszSafeName);

    return pszSafeName;
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRPGDumpDataSource::CreateLayer( const char * pszLayerName,
                                  OGRSpatialReference *poSRS,
                                  OGRwkbGeometryType eType,
                                  char ** papszOptions )

{
    CPLString            osCommand;
    const char          *pszGeomType = NULL;
    char                *pszTableName = NULL;
    char                *pszSchemaName = NULL;
    int                  nDimension = 3;
    int                  bHavePostGIS = TRUE;

    const char* pszFIDColumnName = CSLFetchNameValue(papszOptions, "FID");
    CPLString osFIDColumnName;
    if (pszFIDColumnName == NULL)
        osFIDColumnName = "OGC_FID";
    else
    {
        if( CSLFetchBoolean(papszOptions,"LAUNDER", TRUE) )
        {
            char* pszLaunderedFid = LaunderName(pszFIDColumnName);
            osFIDColumnName += OGRPGDumpEscapeColumnName(pszLaunderedFid);
            CPLFree(pszLaunderedFid);
        }
        else
            osFIDColumnName += OGRPGDumpEscapeColumnName(pszFIDColumnName);
    }
    pszFIDColumnName = osFIDColumnName.c_str();

    if (strncmp(pszLayerName, "pg", 2) == 0)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "The layer name should not begin by 'pg' as it is a reserved prefix");
    }
    
    //bHavePostGIS = CSLFetchBoolean(papszOptions,"POSTGIS", TRUE);

    int bCreateTable = CSLFetchBoolean(papszOptions,"CREATE_TABLE", TRUE);
    int bCreateSchema = CSLFetchBoolean(papszOptions,"CREATE_SCHEMA", TRUE);
    const char* pszDropTable = CSLFetchNameValueDef(papszOptions,"DROP_TABLE", "YES");

    if( wkbFlatten(eType) == eType )
        nDimension = 2;

    if( CSLFetchNameValue( papszOptions, "DIM") != NULL )
        nDimension = atoi(CSLFetchNameValue( papszOptions, "DIM"));

    /* Should we turn layers with None geometry type as Unknown/GEOMETRY */
    /* so they are still recorded in geometry_columns table ? (#4012) */
    int bNoneAsUnknown = CSLTestBoolean(CSLFetchNameValueDef(
                                    papszOptions, "NONE_AS_UNKNOWN", "NO"));
    if (bNoneAsUnknown && eType == wkbNone)
        eType = wkbUnknown;
    else if (eType == wkbNone)
        bHavePostGIS = FALSE;

    int bExtractSchemaFromLayerName = CSLTestBoolean(CSLFetchNameValueDef(
                                    papszOptions, "EXTRACT_SCHEMA_FROM_LAYER_NAME", "YES"));

    /* Postgres Schema handling:
       Extract schema name from input layer name or passed with -lco SCHEMA.
       Set layer name to "schema.table" or to "table" if schema == current_schema()
       Usage without schema name is backwards compatible
    */
    const char* pszDotPos = strstr(pszLayerName,".");
    if ( pszDotPos != NULL && bExtractSchemaFromLayerName )
    {
      int length = pszDotPos - pszLayerName;
      pszSchemaName = (char*)CPLMalloc(length+1);
      strncpy(pszSchemaName, pszLayerName, length);
      pszSchemaName[length] = '\0';

      if( CSLFetchBoolean(papszOptions,"LAUNDER", TRUE) )
          pszTableName = LaunderName( pszDotPos + 1 ); //skip "."
      else
          pszTableName = CPLStrdup( pszDotPos + 1 ); //skip "."
    }
    else
    {
      pszSchemaName = NULL;
      if( CSLFetchBoolean(papszOptions,"LAUNDER", TRUE) )
          pszTableName = LaunderName( pszLayerName ); //skip "."
      else
          pszTableName = CPLStrdup( pszLayerName ); //skip "."
    }

    Commit();

/* -------------------------------------------------------------------- */
/*      Set the default schema for the layers.                          */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue( papszOptions, "SCHEMA" ) != NULL )
    {
        CPLFree(pszSchemaName);
        pszSchemaName = CPLStrdup(CSLFetchNameValue( papszOptions, "SCHEMA" ));
        if (bCreateSchema)
        {
            osCommand.Printf("CREATE SCHEMA \"%s\"", pszSchemaName);
            Log(osCommand);
        }
    }

    if ( pszSchemaName == NULL)
    {
        pszSchemaName = CPLStrdup("public");
    }

/* -------------------------------------------------------------------- */
/*      Do we already have this layer?                                  */
/* -------------------------------------------------------------------- */
    int iLayer;

    for( iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( EQUAL(pszLayerName,papoLayers[iLayer]->GetLayerDefn()->GetName()) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Layer %s already exists, CreateLayer failed.\n", 
                      pszLayerName );
            CPLFree( pszTableName );
            CPLFree( pszSchemaName );
            return NULL;
        }
    }


    if (bCreateTable && (EQUAL(pszDropTable, "YES") ||
                         EQUAL(pszDropTable, "ON") ||
                         EQUAL(pszDropTable, "TRUE") ||
                         EQUAL(pszDropTable, "IF_EXISTS")))
    {
        if (EQUAL(pszDropTable, "IF_EXISTS"))
            osCommand.Printf("DROP TABLE IF EXISTS \"%s\".\"%s\" CASCADE", pszSchemaName, pszTableName );
        else
            osCommand.Printf("DROP TABLE \"%s\".\"%s\" CASCADE", pszSchemaName, pszTableName );
        Log(osCommand);
    }
    
/* -------------------------------------------------------------------- */
/*      Handle the GEOM_TYPE option.                                    */
/* -------------------------------------------------------------------- */
    pszGeomType = CSLFetchNameValue( papszOptions, "GEOM_TYPE" );
    if( pszGeomType == NULL )
    {
        pszGeomType = "geometry";
    }

    if( !EQUAL(pszGeomType,"geometry") && !EQUAL(pszGeomType, "geography"))
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "GEOM_TYPE in PostGIS enabled databases must be 'geometry' or 'geography'.\n"
                    "Creation of layer %s with GEOM_TYPE %s has failed.",
                    pszLayerName, pszGeomType );
        CPLFree( pszTableName );
        CPLFree( pszSchemaName );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try to get the SRS Id of this spatial reference system,         */
/*      adding tot the srs table if needed.                             */
/* -------------------------------------------------------------------- */
    int nSRSId = -1;
    const char* pszPostgisVersion = CSLFetchNameValue( papszOptions, "POSTGIS_VERSION" );
    if( pszPostgisVersion != NULL && atoi(pszPostgisVersion) >= 2 )
        nSRSId = 0;

    if( CSLFetchNameValue( papszOptions, "SRID") != NULL )
        nSRSId = atoi(CSLFetchNameValue( papszOptions, "SRID"));
    else
    {
        if (poSRS)
        {
            const char* pszAuthorityName = poSRS->GetAuthorityName(NULL);
            if( pszAuthorityName != NULL && EQUAL( pszAuthorityName, "EPSG" ) )
            {
                /* Assume the EPSG Id is the SRS ID. Might be a wrong guess ! */
                nSRSId = atoi( poSRS->GetAuthorityCode(NULL) );
            }
            else
            {
                const char* pszGeogCSName = poSRS->GetAttrValue("GEOGCS");
                if (pszGeogCSName != NULL && EQUAL(pszGeogCSName, "GCS_WGS_1984"))
                    nSRSId = 4326;
            }
        }
    }

    CPLString osEscapedTableNameSingleQuote = OGRPGDumpEscapeString(pszTableName, -1, "");
    const char* pszEscapedTableNameSingleQuote = osEscapedTableNameSingleQuote.c_str();

    const char *pszGeometryType = OGRToOGCGeomType(eType);

    const char *pszGFldName = NULL;
    if( bHavePostGIS && !EQUAL(pszGeomType, "geography"))
    {
        if( CSLFetchNameValue( papszOptions, "GEOMETRY_NAME") != NULL )
            pszGFldName = CSLFetchNameValue( papszOptions, "GEOMETRY_NAME");
        else
            pszGFldName = "wkb_geometry";

        if( pszPostgisVersion == NULL || atoi(pszPostgisVersion) < 2 )
        {
            /* Sometimes there is an old cruft entry in the geometry_columns
            * table if things were not properly cleaned up before.  We make
            * an effort to clean out such cruft.
            * Note: PostGIS 2.0 defines geometry_columns as a view (no clean up is needed)
            */
            osCommand.Printf(
                    "DELETE FROM geometry_columns WHERE f_table_name = %s AND f_table_schema = '%s'",
                    pszEscapedTableNameSingleQuote, pszSchemaName );
            if (bCreateTable)
                Log(osCommand);
        }
    }


    StartTransaction();

/* -------------------------------------------------------------------- */
/*      Create a basic table with the FID.  Also include the            */
/*      geometry if this is not a PostGIS enabled table.                */
/* -------------------------------------------------------------------- */
    
    CPLString osCreateTable;
    int bTemporary = CSLFetchNameValue( papszOptions, "TEMPORARY" ) != NULL &&
                     CSLTestBoolean(CSLFetchNameValue( papszOptions, "TEMPORARY" ));
    if (bTemporary)
    {
        CPLFree(pszSchemaName);
        pszSchemaName = CPLStrdup("pg_temp_1");
        osCreateTable.Printf("CREATE TEMPORARY TABLE \"%s\"", pszTableName);
    }
    else
        osCreateTable.Printf("CREATE TABLE \"%s\".\"%s\"", pszSchemaName, pszTableName);

    if( !bHavePostGIS )
    {
        if (eType == wkbNone)
            osCommand.Printf(
                    "%s ( "
                    "   %s SERIAL, "
                    "   CONSTRAINT \"%s_pk\" PRIMARY KEY (%s) )",
                    osCreateTable.c_str(), pszFIDColumnName, pszTableName, pszFIDColumnName );
        else
            osCommand.Printf(
                    "%s ( "
                    "   %s SERIAL, "
                    "   WKB_GEOMETRY %s, "
                    "   CONSTRAINT \"%s_pk\" PRIMARY KEY (%s) )",
                    osCreateTable.c_str(), pszFIDColumnName, pszGeomType, pszTableName, pszFIDColumnName );
    }
    else if ( EQUAL(pszGeomType, "geography") )
    {
        if( CSLFetchNameValue( papszOptions, "GEOMETRY_NAME") != NULL )
            pszGFldName = CSLFetchNameValue( papszOptions, "GEOMETRY_NAME");
        else
            pszGFldName = "the_geog";

        if (nSRSId)
            osCommand.Printf(
                     "%s ( %s SERIAL, \"%s\" geography(%s%s,%d), CONSTRAINT \"%s_pk\" PRIMARY KEY (%s) )",
                     osCreateTable.c_str(), pszFIDColumnName, pszGFldName, pszGeometryType, nDimension == 2 ? "" : "Z", nSRSId, pszTableName, pszFIDColumnName );
        else
            osCommand.Printf(
                     "%s ( %s SERIAL, \"%s\" geography(%s%s), CONSTRAINT \"%s_pk\" PRIMARY KEY (%s) )",
                     osCreateTable.c_str(), pszFIDColumnName, pszGFldName, pszGeometryType, nDimension == 2 ? "" : "Z", pszTableName, pszFIDColumnName );
    }
    else
    {
        osCommand.Printf(
                 "%s ( %s SERIAL, CONSTRAINT \"%s_pk\" PRIMARY KEY (%s) )",
                 osCreateTable.c_str(), pszFIDColumnName, pszTableName, pszFIDColumnName );
    }

    if (bCreateTable)
        Log(osCommand);

/* -------------------------------------------------------------------- */
/*      Eventually we should be adding this table to a table of         */
/*      "geometric layers", capturing the WKT projection, and           */
/*      perhaps some other housekeeping.                                */
/* -------------------------------------------------------------------- */
    if( bCreateTable && bHavePostGIS && !EQUAL(pszGeomType, "geography"))
    {
        osCommand.Printf(
                "SELECT AddGeometryColumn('%s',%s,'%s',%d,'%s',%d)",
                pszSchemaName, pszEscapedTableNameSingleQuote, pszGFldName,
                nSRSId, pszGeometryType, nDimension );
        Log(osCommand);
    }

    if( bCreateTable && bHavePostGIS )
    {
/* -------------------------------------------------------------------- */
/*      Create the spatial index.                                       */
/*                                                                      */
/*      We're doing this before we add geometry and record to the table */
/*      so this may not be exactly the best way to do it.               */
/* -------------------------------------------------------------------- */
        const char *pszSI = CSLFetchNameValue( papszOptions, "SPATIAL_INDEX" );
        if( pszSI == NULL || CSLTestBoolean(pszSI) )
        {
            osCommand.Printf("CREATE INDEX \"%s_geom_idx\" "
                            "ON \"%s\".\"%s\" "
                            "USING GIST (\"%s\")",
                    pszTableName, pszSchemaName, pszTableName, pszGFldName);

            Log(osCommand);
        }
    }

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRPGDumpLayer     *poLayer;

    int bWriteAsHex = !CSLFetchBoolean(papszOptions,"WRITE_EWKT_GEOM",FALSE);

    poLayer = new OGRPGDumpLayer( this, pszSchemaName, pszTableName, pszGFldName,
                                  pszFIDColumnName, nDimension, nSRSId, bWriteAsHex, bCreateTable );
    poLayer->SetLaunderFlag( CSLFetchBoolean(papszOptions,"LAUNDER",TRUE) );
    poLayer->SetPrecisionFlag( CSLFetchBoolean(papszOptions,"PRECISION",TRUE));

    const char* pszOverrideColumnTypes = CSLFetchNameValue( papszOptions, "COLUMN_TYPES" );
    poLayer->SetOverrideColumnTypes(pszOverrideColumnTypes);

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRPGDumpLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRPGDumpLayer *) * (nLayers+1) );

    papoLayers[nLayers++] = poLayer;

    CPLFree( pszTableName );
    CPLFree( pszSchemaName );

    return poLayer;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPGDumpDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRPGDumpDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                                  Log()                               */
/************************************************************************/

void  OGRPGDumpDataSource::Log(const char* pszStr, int bAddSemiColumn)
{
    if (fp == NULL)
    {
        if (bTriedOpen)
            return;
        bTriedOpen = TRUE;
        fp = VSIFOpenL(pszName, "wb");
        if (fp == NULL)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot create %s", pszName);
            return;
        }
    }

    if (bAddSemiColumn)
        VSIFPrintfL(fp, "%s;%s", pszStr, pszEOL);
    else
        VSIFPrintfL(fp, "%s%s", pszStr, pszEOL);
}

/************************************************************************/
/*                             StartCopy()                              */
/************************************************************************/
void OGRPGDumpDataSource::StartCopy( OGRPGDumpLayer *poPGLayer )
{
    EndCopy();
    poLayerInCopyMode = poPGLayer;
}

/************************************************************************/
/*                              EndCopy()                               */
/************************************************************************/
OGRErr OGRPGDumpDataSource::EndCopy( )
{
    if( poLayerInCopyMode != NULL )
    {
        OGRErr result = poLayerInCopyMode->EndCopy();
        poLayerInCopyMode = NULL;

        return result;
    }
    else
        return OGRERR_NONE;
}
