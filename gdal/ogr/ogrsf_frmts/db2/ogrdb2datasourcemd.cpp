/****************************************************************************
 *
 * Project:  DB2 Spatial driver
 * Purpose:  Implements OGRDB2DataSource class
 *           Metadata functions
 * Author:   David Adler, dadler at adtechgeospatial dot com
 *
 ****************************************************************************
 * Copyright (c) 2010, Tamas Szekeres
 * Copyright (c) 2015, David Adler
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

#include "ogr_db2.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                            FlushMetadata()                           */
/************************************************************************/

CPLErr OGRDB2DataSource::FlushMetadata()
{
    CPLDebug("OGRDB2DataSource::FlushMetadata","Entering");
// LATER - where is m_bMetadataDirty set?
    if( !m_bMetadataDirty || m_poParentDS != NULL ||
            !CPLTestBool(CPLGetConfigOption("CREATE_METADATA_TABLES", "YES")) )
        return CE_None;
    if( !HasMetadataTables() && !CreateMetadataTables() )
        return CE_Failure;
    CPLDebug("OGRDB2DataSource::FlushMetadata","Write Metadata");
    m_bMetadataDirty = FALSE;

    if( !m_osRasterTable.empty() )
    {
        const char* pszIdentifier = GetMetadataItem("IDENTIFIER");
        const char* pszDescription = GetMetadataItem("DESCRIPTION");
        if( !m_bIdentifierAsCO && pszIdentifier != NULL &&
                pszIdentifier != m_osIdentifier )
        {
            m_osIdentifier = pszIdentifier;
            OGRDB2Statement oStatement( GetSession() );
            oStatement.Appendf( "UPDATE gpkg.contents SET identifier = '%s' "
                                "WHERE table_name = '%s'",
                                pszIdentifier, m_osRasterTable.c_str());

            if( !oStatement.DB2Execute("OGR_DB2DataSource::FlushMetadata") )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Set identifier failed in gpkg.contents"
                         "for table %s: %s",
                         m_osRasterTable.c_str(),
                         GetSession()->GetLastError());
                return CE_Failure;
            }
        }
        if( !m_bDescriptionAsCO && pszDescription != NULL &&
                pszDescription != m_osDescription )
        {
            m_osDescription = pszDescription;
            OGRDB2Statement oStatement( GetSession() );
            oStatement.Appendf( "UPDATE gpkg.contents SET description = '%s' "
                                "WHERE table_name = '%s'",
                                pszDescription, m_osRasterTable.c_str());

            if( !oStatement.DB2Execute("OGR_DB2DataSource::FlushMetadata") )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Set description failed in gpkg.contents"
                         "for table %s: %s",
                         m_osRasterTable.c_str(),
                         GetSession()->GetLastError());
                return CE_Failure;
            }
        }
    }

    char** papszMDDup = NULL;
    for( char** papszIter = GetMetadata(); papszIter && *papszIter; ++papszIter )
    {
        if( STARTS_WITH_CI(*papszIter, "IDENTIFIER=") )
            continue;
        if( STARTS_WITH_CI(*papszIter, "DESCRIPTION=") )
            continue;
        if( STARTS_WITH_CI(*papszIter, "ZOOM_LEVEL=") )
            continue;
        if( STARTS_WITH_CI(*papszIter, "GPKG_METADATA_ITEM_") )
            continue;
        papszMDDup = CSLInsertString(papszMDDup, -1, *papszIter);
    }

    CPLXMLNode* psXMLNode;
    {
        GDALMultiDomainMetadata oLocalMDMD;
        char** papszDomainList = oMDMD.GetDomainList();
        char** papszIter = papszDomainList;
        oLocalMDMD.SetMetadata(papszMDDup);
        while( papszIter && *papszIter )
        {
            if( !EQUAL(*papszIter, "") &&
                    !EQUAL(*papszIter, "IMAGE_STRUCTURE") &&
                    !EQUAL(*papszIter, "GEOPACKAGE") )
                oLocalMDMD.SetMetadata(oMDMD.GetMetadata(*papszIter), *papszIter);
            papszIter ++;
        }
        psXMLNode = oLocalMDMD.Serialize();
    }

    CSLDestroy(papszMDDup);
    papszMDDup = NULL;

    WriteMetadata(psXMLNode, m_osRasterTable.c_str() );

    if( !m_osRasterTable.empty() )
    {
        char** papszGeopackageMD = GetMetadata("GEOPACKAGE");

        papszMDDup = NULL;
        for( char** papszIter = papszGeopackageMD; papszIter && *papszIter; ++papszIter )
        {
            papszMDDup = CSLInsertString(papszMDDup, -1, *papszIter);
        }

        GDALMultiDomainMetadata oLocalMDMD;
        oLocalMDMD.SetMetadata(papszMDDup);
        CSLDestroy(papszMDDup);
        papszMDDup = NULL;
        psXMLNode = oLocalMDMD.Serialize();

        WriteMetadata(psXMLNode, NULL);
    }

    for(int i=0; i<m_nLayers; i++)
    {
        const char* pszIdentifier = m_papoLayers[i]->GetMetadataItem("IDENTIFIER");
        const char* pszDescription = m_papoLayers[i]->GetMetadataItem("DESCRIPTION");
        if( pszIdentifier != NULL )
        {
            OGRDB2Statement oStatement( GetSession() );
            oStatement.Appendf( "UPDATE gpkg.contents SET identifier = '%s' "
                                "WHERE table_name = '%s'",
                                pszIdentifier, m_papoLayers[i]->GetName());

            if( !oStatement.DB2Execute("OGR_DB2DataSource::FlushMetadata") )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Set identifier failed in gpkg.contents"
                         "for table %s: %s",
                         m_osRasterTable.c_str(),
                         GetSession()->GetLastError());
                CPLDebug("OGRDB2DataSource::FlushMetadata",
                         "Set identifier failed in gpkg.contents"
                         "for table %s: %s",
                         m_osRasterTable.c_str(),
                         GetSession()->GetLastError());
                return CE_Failure;
            }
        }
        if( pszDescription != NULL )
        {
            OGRDB2Statement oStatement( GetSession() );
            oStatement.Appendf( "UPDATE gpkg.contents SET description = '%s' "
                                "WHERE table_name = '%s'",
                                pszDescription, m_papoLayers[i]->GetName());

            if( !oStatement.DB2Execute("OGR_DB2DataSource::ICreateLayer") )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Set description failed in gpkg.contents"
                         "for table %s: %s",
                         m_osRasterTable.c_str(),
                         GetSession()->GetLastError());
                CPLDebug("OGRDB2DataSource::FlushMetadata",
                         "Set description failed in gpkg.contents"
                         "for table %s: %s",
                         m_osRasterTable.c_str(),
                         GetSession()->GetLastError());
                return CE_Failure;
            }
        }

        papszMDDup = NULL;
        for( char** papszIter = m_papoLayers[i]->GetMetadata(); papszIter && *papszIter; ++papszIter )
        {
            if( STARTS_WITH_CI(*papszIter, "IDENTIFIER=") )
                continue;
            if( STARTS_WITH_CI(*papszIter, "DESCRIPTION=") )
                continue;
            if( STARTS_WITH_CI(*papszIter, "OLMD_FID64=") )
                continue;
            papszMDDup = CSLInsertString(papszMDDup, -1, *papszIter);
        }

        {
            GDALMultiDomainMetadata oLocalMDMD;
            char** papszDomainList = m_papoLayers[i]->GetMetadataDomainList();
            char** papszIter = papszDomainList;
            oLocalMDMD.SetMetadata(papszMDDup);
            while( papszIter && *papszIter )
            {
                if( !EQUAL(*papszIter, "") )
                    oLocalMDMD.SetMetadata(m_papoLayers[i]->GetMetadata(*papszIter), *papszIter);
                papszIter ++;
            }
            CSLDestroy(papszDomainList);
            psXMLNode = oLocalMDMD.Serialize();
        }

        CSLDestroy(papszMDDup);
        papszMDDup = NULL;

        WriteMetadata(psXMLNode, m_papoLayers[i]->GetName() );
    }

    return CE_None;
}

/************************************************************************/
/*                            WriteMetadata()                           */
/************************************************************************/

void OGRDB2DataSource::WriteMetadata(CPLXMLNode* psXMLNode, /* will be destroyed by the method */
                                     const char* pszTableName)
{
    int bIsEmpty = (psXMLNode == NULL);
    char *pszXML = NULL;
    if( !bIsEmpty )
    {
        CPLXMLNode* psMasterXMLNode = CPLCreateXMLNode( NULL, CXT_Element,
                                      "GDALMultiDomainMetadata" );
        psMasterXMLNode->psChild = psXMLNode;
        pszXML = CPLSerializeXMLTree(psMasterXMLNode);
        CPLDestroyXMLNode(psMasterXMLNode);
    }
    // cppcheck-suppress uselessAssignmentPtrArg
    psXMLNode = NULL;
    CPLDebug("OGRDB2DataSource::WriteMetadata",
             "pszTableName: %s; bIsEmpty: %d", pszTableName, bIsEmpty);
    OGRDB2Statement oStatement( GetSession() );
    oStatement.Append(
        "SELECT md.id FROM gpkg.metadata md "
        "JOIN gpkg.metadata_reference mdr "
        "ON (md.id = mdr.md_file_id ) "
        "WHERE md.md_scope = 'dataset' "
        "AND md.md_standard_uri='http://gdal.org' "
        "AND md.mime_type='text/xml' ");
    if( pszTableName && pszTableName[0] != '\0' )
    {
        oStatement.Appendf(
            "AND mdr.reference_scope = 'table' "
            "AND mdr.table_name = '%s'",
            pszTableName);
    }
    else
    {
        oStatement.Append(
            "AND mdr.reference_scope = 'geopackage'");
    }

    if( !oStatement.DB2Execute("OGR_DB2DataSource::WriteMetadata") )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failed getting md.id; error: %s",
                 GetSession()->GetLastError());
    }

    int mdId = -1;
    if (oStatement.Fetch()) {
        CPLDebug("OGRDB2DataSource::WriteMetadata",
                 "col(0): %s", oStatement.GetColData(0));
        mdId = atoi(oStatement.GetColData(0));
    }
    CPLDebug("OGRDB2DataSource::WriteMetadata",
             "mdId: %d", mdId);
    oStatement.Clear();
    if( bIsEmpty )
    {
        if( mdId >= 0 )
        {
            oStatement.Appendf("DELETE FROM gpkg.metadata_reference "
                               "WHERE md_file_id = %d", mdId);
            if( !oStatement.DB2Execute("OGR_DB2DataSource::WriteMetadata") )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed getting md.id; error: %s",
                         GetSession()->GetLastError());
            }
            oStatement.Clear();
            oStatement.Appendf("DELETE FROM gpkg.metadata "
                               "WHERE id = %d", mdId);
            if( !oStatement.DB2Execute("OGR_DB2DataSource::WriteMetadata") )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed deleting md.id; error: %s",
                         GetSession()->GetLastError());
            }
            oStatement.Clear();
        }
    } else
    {
        if ( mdId >= 0 )
        {
            oStatement.Appendf( "UPDATE gpkg.metadata "
                                "SET metadata = '%s' WHERE id = %d",
                                pszXML, mdId);
        }
        else
        {
            oStatement.Appendf(
                "INSERT INTO gpkg.metadata (md_scope, "
                "md_standard_uri, mime_type, metadata) VALUES "
                "('dataset','http://gdal.org','text/xml','%s')",
                pszXML);
        }
        if( !oStatement.DB2Execute("OGR_DB2DataSource::WriteMetadata") )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed updating metadata; error: %s",
                     GetSession()->GetLastError());
        }
        int nNewId = -1;
        if (mdId < 0 ) {
            OGRDB2Statement oStatement2( GetSession() );
            oStatement2.Append( "select IDENTITY_VAL_LOCAL() AS IDENTITY "
                                "FROM SYSIBM.SYSDUMMY1");
            if( oStatement2.DB2Execute("OGR_DB2DataSource::WriteMetadata")
                    && oStatement2.Fetch() )
            {
                if ( oStatement2.GetColData( 0 ) )
                    nNewId = atoi(oStatement2.GetColData( 0 ) );
            }
        }

        CPLDebug("OGRDB2DataSource::WriteMetadata",
                 "nNewId: %d", nNewId);
        oStatement.Clear();

        CPLFree(pszXML);

        if( mdId < 0 )
        {
            if( pszTableName != NULL && pszTableName[0] != '\0' )
            {
                oStatement.Appendf("INSERT INTO gpkg.metadata_reference "
                                   "(reference_scope, table_name, md_file_id) "
                                   "VALUES ('table', '%s', %d)",
                                   pszTableName, nNewId);
            }
            else
            {
                oStatement.Appendf("INSERT INTO gpkg.metadata_reference "
                                   "(reference_scope, md_file_id) "
                                   "VALUES ('geopackage', %d)",
                                   nNewId);
            }
        }
        else
        {
            oStatement.Appendf("UPDATE gpkg.metadata_reference "
                               "SET timestamp = CURRENT TIMESTAMP "
                               "WHERE md_file_id = %d",
                               mdId);
        }

        if( !oStatement.DB2Execute("OGR_DB2DataSource::WriteMetadata") )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed updating metadata; error: %s",
                     GetSession()->GetLastError());
        }
    }
    CPLDebug("OGRDB2DataSource::WriteMetadata",
             "exiting");
    return;
}

/************************************************************************/
/*                        CreateMetadataTables()                        */
/************************************************************************/

int OGRDB2DataSource::CreateMetadataTables()
{
    CPLDebug("OGRDB2DataSource::CreateMetadataTables","Enter");

//    int bCreateTriggers = CPLTestBool(CPLGetConfigOption("CREATE_TRIGGERS", "YES"));
    OGRDB2Statement oStatement( GetSession() );
    m_oSession.BeginTransaction();
    /* Requirement 13: A GeoPackage file SHALL include a gpkg_contents table */
    /* http://opengis.github.io/geopackage/#_contents */

    oStatement.Appendf("CREATE TABLE gpkg.contents ( "
                       "table_name VARCHAR(128) NOT NULL PRIMARY KEY, "
                       "data_type VARCHAR(128) NOT NULL, "

                       "identifier VARCHAR(128) NOT NULL UNIQUE, "
                       "description VARCHAR(128) DEFAULT '', "
                       "last_change TIMESTAMP NOT NULL DEFAULT , "
                       "min_x DOUBLE, "
                       "min_y DOUBLE, "
                       "max_x DOUBLE, "
                       "max_y DOUBLE, "
                       "srs_id INTEGER "
//              "CONSTRAINT fk_gc_r_srs_id FOREIGN KEY (srs_id) REFERENCES "
//              "db2gse.gse_spatial_reference_systems(srs_id)" // Fails???
                       ")");

    if( !oStatement.DB2Execute("OGR_DB2DataSource::CreateMetadataTables") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Error creating gpkg.contents: %s",
                  GetSession()->GetLastError() );
        CPLDebug("OGRDB2DataSource::CreateMetadataTables", "Error creating gpkg.contents");
        m_oSession.RollbackTransaction();
        return FALSE;
    }

    /* From C.5. gpkg_tile_matrix_set Table 28. gpkg_tile_matrix_set Table Creation SQL  */
    oStatement.Clear();
    oStatement.Appendf("CREATE TABLE gpkg.tile_matrix_set ( "
                       "table_name VARCHAR(128) NOT NULL PRIMARY KEY, "
                       "srs_id INTEGER NOT NULL, "
                       "min_x DOUBLE, "
                       "min_y DOUBLE, "
                       "max_x DOUBLE, "
                       "max_y DOUBLE, "
                       "CONSTRAINT fk_gtms_table_name FOREIGN KEY (table_name) "
                       "REFERENCES gpkg.contents(table_name) "
                       "ON DELETE CASCADE"
//              "CONSTRAINT fk_gtms_srs_id FOREIGN KEY (srs_id) REFERENCES "
//              "db2gse.gse_spatial_reference_systems(srs_id)" // Fails???
                       ")");

    if( !oStatement.DB2Execute("OGR_DB2DataSource::CreateMetadataTables") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Error creating gpkg.tile_matrix_set: %s",
                  GetSession()->GetLastError() );
        CPLDebug("OGRDB2DataSource::CreateMetadataTables",
                 "Error creating gpkg.tile_matrix_set");
        m_oSession.RollbackTransaction();
        return FALSE;
    }

    /* From C.6. gpkg_tile_matrix Table 29. */
    /* gpkg_tile_matrix Table Creation SQL */
    oStatement.Clear();
    oStatement.Appendf("CREATE TABLE gpkg.tile_matrix ( "
                       "table_name VARCHAR(128) NOT NULL, "
                       "zoom_level INTEGER NOT NULL, "
                       "matrix_width INTEGER NOT NULL, "
                       "matrix_height INTEGER NOT NULL, "
                       "tile_width INTEGER NOT NULL, "
                       "tile_height INTEGER NOT NULL, "
                       "pixel_x_size DOUBLE NOT NULL, "
                       "pixel_y_size DOUBLE NOT NULL, "
                       "CONSTRAINT pk_ttm PRIMARY KEY (table_name, zoom_level), "
                       "CONSTRAINT fk_tmm_table_name FOREIGN KEY (table_name) "
                       "REFERENCES gpkg.contents(table_name) "
                       "ON DELETE CASCADE"
                       ")");

    if( !oStatement.DB2Execute("OGR_DB2DataSource::CreateMetadataTables") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Error creating gpkg.tile_matrix: %s",
                  GetSession()->GetLastError() );
        m_oSession.RollbackTransaction();
        return FALSE;
    }

    /* From C.10. gpkg_metadata Table 35. */
    /* gpkg_metadata Table Definition SQL  */
    oStatement.Clear();
    oStatement.Append("CREATE TABLE gpkg.metadata ( "
                      "id INTEGER PRIMARY KEY NOT NULL GENERATED BY DEFAULT AS IDENTITY, "
                      "md_scope VARCHAR(128) NOT NULL DEFAULT 'dataset', "
                      "md_standard_uri VARCHAR(128) NOT NULL, "
                      "mime_type VARCHAR(128) NOT NULL DEFAULT 'text/xml', "
                      "metadata VARCHAR(32000) NOT NULL "
                      ")");

    if( !oStatement.DB2Execute("OGR_DB2DataSource::CreateMetadataTables") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Error creating gpkg.metadata: %s",
                  GetSession()->GetLastError() );
        CPLDebug("OGRDB2DataSource::CreateMetadataTables",
                 "Error creating gpkg.metadata");
        m_oSession.RollbackTransaction();
        return FALSE;
    }

#ifdef LATER
    /* From D.2. metadata Table 40. metadata Trigger Definition SQL  */
    const char* pszMetadataTriggers =
        "CREATE TRIGGER 'gpkg_metadata_md_scope_insert' "
        "BEFORE INSERT ON 'gpkg_metadata' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'insert on table gpkg_metadata violates "
        "constraint: md_scope must be one of undefined | fieldSession | "
        "collectionSession | series | dataset | featureType | feature | "
        "attributeType | attribute | tile | model | catalogue | schema | "
        "taxonomy software | service | collectionHardware | "
        "nonGeographicDataset | dimensionGroup') "
        "WHERE NOT(NEW.md_scope IN "
        "('undefined','fieldSession','collectionSession','series','dataset', "
        "'featureType','feature','attributeType','attribute','tile','model', "
        "'catalogue','schema','taxonomy','software','service', "
        "'collectionHardware','nonGeographicDataset','dimensionGroup')); "
        "END; "
        "CREATE TRIGGER 'gpkg_metadata_md_scope_update' "
        "BEFORE UPDATE OF 'md_scope' ON 'gpkg_metadata' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'update on table gpkg_metadata violates "
        "constraint: md_scope must be one of undefined | fieldSession | "
        "collectionSession | series | dataset | featureType | feature | "
        "attributeType | attribute | tile | model | catalogue | schema | "
        "taxonomy software | service | collectionHardware | "
        "nonGeographicDataset | dimensionGroup') "
        "WHERE NOT(NEW.md_scope IN "
        "('undefined','fieldSession','collectionSession','series','dataset', "
        "'featureType','feature','attributeType','attribute','tile','model', "
        "'catalogue','schema','taxonomy','software','service', "
        "'collectionHardware','nonGeographicDataset','dimensionGroup')); "
        "END";
    if ( bCreateTriggers && OGRERR_NONE != SQLCommand(hDB, pszMetadataTriggers) )
        return FALSE;
#endif

    /* From C.11. gpkg_metadata_reference Table 36. gpkg_metadata_reference Table Definition SQL */
    oStatement.Clear();
    oStatement.Appendf("CREATE TABLE gpkg.metadata_reference ( "
                       "reference_scope VARCHAR(128) NOT NULL, "
                       "table_name VARCHAR(128), "
                       "column_name VARCHAR(128), "
                       "row_id_value INTEGER, "
                       "timestamp TIMESTAMP NOT NULL DEFAULT, "
                       "md_file_id INTEGER NOT NULL, "
                       "md_parent_id INTEGER, "
                       "CONSTRAINT crmr_mfi_fk FOREIGN KEY (md_file_id) "
                       "REFERENCES gpkg.metadata(id), "
                       "CONSTRAINT crmr_mpi_fk FOREIGN KEY (md_parent_id) "
                       "REFERENCES gpkg.metadata(id) "
                       ")");

    if( !oStatement.DB2Execute("OGR_DB2DataSource::CreateMetadataTables") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Error creating gpkg.metadata_reference: %s",
                  GetSession()->GetLastError() );
        m_oSession.RollbackTransaction();
        return FALSE;
    }

#ifdef LATER
    /* From D.3. metadata_reference Table 41. gpkg_metadata_reference Trigger Definition SQL   */
    const char* pszMetadataReferenceTriggers =
        "CREATE TRIGGER 'gpkg_metadata_reference_reference_scope_insert' "
        "BEFORE INSERT ON 'gpkg_metadata_reference' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'insert on table gpkg_metadata_reference "
        "violates constraint: reference_scope must be one of \"geopackage\", "
        "table\", \"column\", \"row\", \"row/col\"') "
        "WHERE NOT NEW.reference_scope IN "
        "('geopackage','table','column','row','row/col'); "
        "END; "
        "CREATE TRIGGER 'gpkg_metadata_reference_reference_scope_update' "
        "BEFORE UPDATE OF 'reference_scope' ON 'gpkg_metadata_reference' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'update on table gpkg_metadata_reference "
        "violates constraint: referrence_scope must be one of \"geopackage\", "
        "\"table\", \"column\", \"row\", \"row/col\"') "
        "WHERE NOT NEW.reference_scope IN "
        "('geopackage','table','column','row','row/col'); "
        "END; "
        "CREATE TRIGGER 'gpkg_metadata_reference_column_name_insert' "
        "BEFORE INSERT ON 'gpkg_metadata_reference' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'insert on table gpkg_metadata_reference "
        "violates constraint: column name must be NULL when reference_scope "
        "is \"geopackage\", \"table\" or \"row\"') "
        "WHERE (NEW.reference_scope IN ('geopackage','table','row') "
        "AND NEW.column_name IS NOT NULL); "
        "SELECT RAISE(ABORT, 'insert on table gpkg_metadata_reference "
        "violates constraint: column name must be defined for the specified "
        "table when reference_scope is \"column\" or \"row/col\"') "
        "WHERE (NEW.reference_scope IN ('column','row/col') "
        "AND NOT NEW.table_name IN ( "
        "SELECT name FROM SQLITE_MASTER WHERE type = 'table' "
        "AND name = NEW.table_name "
        "AND sql LIKE ('%' || NEW.column_name || '%'))); "
        "END; "
        "CREATE TRIGGER 'gpkg_metadata_reference_column_name_update' "
        "BEFORE UPDATE OF column_name ON 'gpkg_metadata_reference' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'update on table gpkg_metadata_reference "
        "violates constraint: column name must be NULL when reference_scope "
        "is \"geopackage\", \"table\" or \"row\"') "
        "WHERE (NEW.reference_scope IN ('geopackage','table','row') "
        "AND NEW.column_nameIS NOT NULL); "
        "SELECT RAISE(ABORT, 'update on table gpkg_metadata_reference "
        "violates constraint: column name must be defined for the specified "
        "table when reference_scope is \"column\" or \"row/col\"') "
        "WHERE (NEW.reference_scope IN ('column','row/col') "
        "AND NOT NEW.table_name IN ( "
        "SELECT name FROM SQLITE_MASTER WHERE type = 'table' "
        "AND name = NEW.table_name "
        "AND sql LIKE ('%' || NEW.column_name || '%'))); "
        "END; "
        "CREATE TRIGGER 'gpkg_metadata_reference_row_id_value_insert' "
        "BEFORE INSERT ON 'gpkg_metadata_reference' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'insert on table gpkg_metadata_reference "
        "violates constraint: row_id_value must be NULL when reference_scope "
        "is \"geopackage\", \"table\" or \"column\"') "
        "WHERE NEW.reference_scope IN ('geopackage','table','column') "
        "AND NEW.row_id_value IS NOT NULL; "
        "SELECT RAISE(ABORT, 'insert on table gpkg_metadata_reference "
        "violates constraint: row_id_value must exist in specified table when "
        "reference_scope is \"row\" or \"row/col\"') "
        "WHERE NEW.reference_scope IN ('row','row/col') "
        "AND NOT EXISTS (SELECT rowid "
        "FROM (SELECT NEW.table_name AS table_name) WHERE rowid = "
        "NEW.row_id_value); "
        "END; "
        "CREATE TRIGGER 'gpkg_metadata_reference_row_id_value_update' "
        "BEFORE UPDATE OF 'row_id_value' ON 'gpkg_metadata_reference' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'update on table gpkg_metadata_reference "
        "violates constraint: row_id_value must be NULL when reference_scope "
        "is \"geopackage\", \"table\" or \"column\"') "
        "WHERE NEW.reference_scope IN ('geopackage','table','column') "
        "AND NEW.row_id_value IS NOT NULL; "
        "SELECT RAISE(ABORT, 'update on table gpkg_metadata_reference "
        "violates constraint: row_id_value must exist in specified table when "
        "reference_scope is \"row\" or \"row/col\"') "
        "WHERE NEW.reference_scope IN ('row','row/col') "
        "AND NOT EXISTS (SELECT rowid "
        "FROM (SELECT NEW.table_name AS table_name) WHERE rowid = "
        "NEW.row_id_value); "
        "END; "
        "CREATE TRIGGER 'gpkg_metadata_reference_timestamp_insert' "
        "BEFORE INSERT ON 'gpkg_metadata_reference' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'insert on table gpkg_metadata_reference "
        "violates constraint: timestamp must be a valid time in ISO 8601 "
        "\"yyyy-mm-ddThh:mm:ss.cccZ\" form') "
        "WHERE NOT (NEW.timestamp GLOB "
        "'[1-2][0-9][0-9][0-9]-[0-1][0-9]-[0-3][0-9]T[0-2][0-9]:[0-5][0-9]:[0-5][0-9].[0-9][0-9][0-9]Z' "
        "AND strftime('%s',NEW.timestamp) NOT NULL); "
        "END; "
        "CREATE TRIGGER 'gpkg_metadata_reference_timestamp_update' "
        "BEFORE UPDATE OF 'timestamp' ON 'gpkg_metadata_reference' "
        "FOR EACH ROW BEGIN "
        "SELECT RAISE(ABORT, 'update on table gpkg_metadata_reference "
        "violates constraint: timestamp must be a valid time in ISO 8601 "
        "\"yyyy-mm-ddThh:mm:ss.cccZ\" form') "
        "WHERE NOT (NEW.timestamp GLOB "
        "'[1-2][0-9][0-9][0-9]-[0-1][0-9]-[0-3][0-9]T[0-2][0-9]:[0-5][0-9]:[0-5][0-9].[0-9][0-9][0-9]Z' "
        "AND strftime('%s',NEW.timestamp) NOT NULL); "
        "END";
    if ( bCreateTriggers && OGRERR_NONE != SQLCommand(hDB, pszMetadataReferenceTriggers) )
        return FALSE;

    return TRUE;
#endif
    m_oSession.CommitTransaction();
    return TRUE;
}

/************************************************************************/
/*                           HasMetadataTables()                        */
/************************************************************************/

int OGRDB2DataSource::HasMetadataTables()
{
    if (m_bHasMetadataTables) return TRUE;

    OGRDB2Statement oStatement( GetSession() );
    oStatement.Append("SELECT COUNT(md.id) FROM gpkg.metadata md");

// We assume that if the statement fails, the table doesn't exist
    if( !oStatement.DB2Execute("OGR_DB2DataSource::HasMetadataTables") )
    {
        CPLDebug("OGRDB2DataSource::HasMetadataTables","Tables not found");
        if (!CreateMetadataTables()) {
            return FALSE;
        }
    }
    m_bHasMetadataTables = TRUE;

    return TRUE;
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **OGRDB2DataSource::GetMetadataDomainList()
{
    CPLDebug("OGRDB2DataSource::GetMetadataDomainList","Entering");
    GetMetadata();
    if( !m_osRasterTable.empty() )
        GetMetadata("GEOPACKAGE");
    return BuildMetadataDomainList(GDALPamDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "SUBDATASETS", NULL);
}

/************************************************************************/
/*                        CheckMetadataDomain()                         */
/************************************************************************/

const char* OGRDB2DataSource::CheckMetadataDomain( const char* pszDomain )
{
    DB2_DEBUG_ENTER("OGRDB2DataSource::CheckMetadataDomain");
    if( pszDomain != NULL && EQUAL(pszDomain, "GEOPACKAGE") &&
            m_osRasterTable.empty() )
    {
        CPLError(CE_Warning, CPLE_IllegalArg,
                 "Using GEOPACKAGE for a non-raster geopackage is not supported. "
                 "Using default domain instead");
        return NULL;
    }
    return pszDomain;
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **OGRDB2DataSource::GetMetadata( const char *pszDomain )

{
    DB2_DEBUG_ENTER("OGRDB2DataSource::GetMetadata");
    if( pszDomain != NULL && EQUAL(pszDomain,"SUBDATASETS") )
        return m_papszSubDatasets;
    CPLDebug("OGRDB2DataSource::GetMetadata","m_bHasReadMetadataFromStorage1: %d", m_bHasReadMetadataFromStorage);
    if( m_bHasReadMetadataFromStorage )
        return GDALPamDataset::GetMetadata( pszDomain );

    m_bHasReadMetadataFromStorage = TRUE;
    CPLDebug("OGRDB2DataSource::GetMetadata","m_bHasReadMetadataFromStorage2: %d", m_bHasReadMetadataFromStorage);

    if ( !HasMetadataTables() )
        return GDALPamDataset::GetMetadata( pszDomain );

    OGRDB2Statement oStatement( GetSession() );
    if( !m_osRasterTable.empty() )
    {

        oStatement.Appendf(
            "SELECT md.metadata, md.md_standard_uri, md.mime_type, "
            "mdr.reference_scope FROM gpkg.metadata md "
            "JOIN gpkg.metadata_reference mdr ON (md.id = mdr.md_file_id ) "
            "WHERE mdr.reference_scope = 'geopackage' OR "
            "(mdr.reference_scope = 'table' AND mdr.table_name = '%s') "
            " ORDER BY md.id",
            m_osRasterTable.c_str());
    }
    else
    {
        oStatement.Append(
            "SELECT md.metadata, md.md_standard_uri, md.mime_type, "
            "mdr.reference_scope FROM gpkg.metadata md "
            "JOIN gpkg.metadata_reference mdr ON (md.id = mdr.md_file_id ) "
            "WHERE mdr.reference_scope = 'geopackage' ORDER BY md.id");
    }

    if( !oStatement.DB2Execute("OGR_DB2DataSource::GetMetadata") )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failed getting metadata; error: %s",
                 GetSession()->GetLastError());
        return GDALPamDataset::GetMetadata( pszDomain );
    }

    char** papszMetadata = CSLDuplicate(GDALPamDataset::GetMetadata());

    /* GDAL metadata */
    while(oStatement.Fetch())
    {
        const char* pszMetadata = oStatement.GetColData( 0);
        const char* pszMDStandardURI = oStatement.GetColData( 1);
        const char* pszMimeType = oStatement.GetColData( 2);
        const char* pszReferenceScope = oStatement.GetColData( 3);
        int bIsGPKGScope = EQUAL(pszReferenceScope, "geopackage");
        if( pszMetadata == NULL )
            continue;
        if( pszMDStandardURI != NULL && EQUAL(pszMDStandardURI, "http://gdal.org") &&
                pszMimeType != NULL && EQUAL(pszMimeType, "text/xml") )
        {
            CPLXMLNode* psXMLNode = CPLParseXMLString(pszMetadata);
            if( psXMLNode )
            {
                GDALMultiDomainMetadata oLocalMDMD;
                oLocalMDMD.XMLInit(psXMLNode, FALSE);
                if( !m_osRasterTable.empty() && bIsGPKGScope )
                {
                    oMDMD.SetMetadata( oLocalMDMD.GetMetadata(), "GEOPACKAGE" );
                }
                else
                {
                    papszMetadata = CSLMerge(papszMetadata, oLocalMDMD.GetMetadata());
                    char** papszDomainList = oLocalMDMD.GetDomainList();
                    char** papszIter = papszDomainList;
                    while( papszIter && *papszIter )
                    {
                        if( !EQUAL(*papszIter, "") && !EQUAL(*papszIter, "IMAGE_STRUCTURE") )
                            oMDMD.SetMetadata(oLocalMDMD.GetMetadata(*papszIter), *papszIter);
                        papszIter ++;
                    }
                }
                CPLDestroyXMLNode(psXMLNode);
            }
        }
    }

    GDALPamDataset::SetMetadata(papszMetadata);
    CSLDestroy(papszMetadata);
    papszMetadata = NULL;
    CPLDebug("OGRDB2DataSource::GetMetadata","Exiting");
#ifdef LATER
// where is the result set for this section created????
    /* Add non-GDAL metadata now */
    int nNonGDALMDILocal = 1;
    int nNonGDALMDIGeopackage = 1;
    for(int i=0; i<oResult.nRowCount; i++)
    {
        const char *pszMetadata = oStatement.GetColData( 0, i);
        const char* pszMDStandardURI = oStatement.GetColData( 1, i);
        const char* pszMimeType = oStatement.GetColData( 2, i);
        const char* pszReferenceScope = oStatement.GetColData( 3, i);
        int bIsGPKGScope = EQUAL(pszReferenceScope, "geopackage");
        if( pszMetadata == NULL )
            continue;
        if( pszMDStandardURI != NULL && EQUAL(pszMDStandardURI, "http://gdal.org") &&
                pszMimeType != NULL && EQUAL(pszMimeType, "text/xml") )
            continue;

        if( !m_osRasterTable.empty() && bIsGPKGScope )
        {
            oMDMD.SetMetadataItem( CPLSPrintf("GPKG_METADATA_ITEM_%d", nNonGDALMDIGeopackage),
                                   pszMetadata,
                                   "GEOPACKAGE" );
            nNonGDALMDIGeopackage ++;
        }
        /*else if( strcmp( pszMDStandardURI, "http://www.isotc211.org/2005/gmd" ) == 0 &&
            strcmp( pszMimeType, "text/xml" ) == 0 )
        {
            char* apszMD[2];
            apszMD[0] = (char*)pszMetadata;
            apszMD[1] = NULL;
            oMDMD.SetMetadata(apszMD, "xml:MD_Metadata");
        }*/
        else
        {
            oMDMD.SetMetadataItem( CPLSPrintf("GPKG_METADATA_ITEM_%d", nNonGDALMDILocal),
                                   pszMetadata );
            nNonGDALMDILocal ++;
        }
    }
#endif

    return GDALPamDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *OGRDB2DataSource::GetMetadataItem( const char * pszName,
        const char * pszDomain )
{
    pszDomain = CheckMetadataDomain(pszDomain);
    CPLDebug("OGRDB2DataSource::GetMetadataItem","'%s'; '%s'; '%s'",pszName,pszDomain,CSLFetchNameValue( GetMetadata(pszDomain), pszName ));
    return CSLFetchNameValue( GetMetadata(pszDomain), pszName );
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr OGRDB2DataSource::SetMetadata( char ** papszMetadata, const char * pszDomain )
{
    pszDomain = CheckMetadataDomain(pszDomain);
    m_bMetadataDirty = TRUE;
    GetMetadata(); /* force loading from storage if needed */
    return GDALPamDataset::SetMetadata(papszMetadata, pszDomain);
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr OGRDB2DataSource::SetMetadataItem( const char * pszName,
        const char * pszValue,
        const char * pszDomain )
{
    pszDomain = CheckMetadataDomain(pszDomain);
    m_bMetadataDirty = TRUE;
    GetMetadata(); /* force loading from storage if needed */
    CPLDebug("OGRDB2DataSource::SetMetadataItem","'%s'; '%s'; '%s'",pszName,pszDomain,pszValue);
    return GDALPamDataset::SetMetadataItem(pszName, pszValue, pszDomain);
}
