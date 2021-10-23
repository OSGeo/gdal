/******************************************************************************
 *
 * Project:  VFK Reader (SQLite)
 * Purpose:  Implements VFKReaderSQLite class.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2012-2018, Martin Landa <landa.martin gmail.com>
 * Copyright (c) 2012-2018, Even Rouault <even dot rouault at spatialys.com>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ****************************************************************************/

#include "cpl_vsi.h"

#include "vfkreader.h"
#include "vfkreaderp.h"

#include "cpl_conv.h"
#include "cpl_error.h"

#include <cstring>

#include "ogr_geometry.h"

CPL_CVSID("$Id$")

/*!
  \brief VFKReaderSQLite constructor
*/
VFKReaderSQLite::VFKReaderSQLite( const GDALOpenInfo* poOpenInfo ) :
    VFKReader( poOpenInfo ),
    m_pszDBname(nullptr),
    m_poDB(nullptr),
    // True - build geometry from DB
    // False - store also geometry in DB
    m_bSpatial(CPLTestBool(CPLGetConfigOption("OGR_VFK_DB_SPATIAL", "YES"))),
    m_bNewDb(false),
    m_bDbSource(false)
{
    size_t nLen = 0;
    VSIStatBufL sStatBufDb;

    m_bDbSource = poOpenInfo->nHeaderBytes >= 16 &&
        STARTS_WITH((const char*)poOpenInfo->pabyHeader, "SQLite format 3");

    const char *pszDbNameConf = CPLGetConfigOption("OGR_VFK_DB_NAME", nullptr);
    CPLString osDbName;

    if( !m_bDbSource )
    {
        m_bNewDb = true;

        /* open tmp SQLite DB (re-use DB file if already exists) */
        if (pszDbNameConf) {
            osDbName = pszDbNameConf;
        }
        else
        {
            osDbName = CPLResetExtension(m_pszFilename, "db");
        }
        nLen = osDbName.length();
        if( nLen > 2048 )
        {
            nLen = 2048;
            osDbName.resize(nLen);
        }
    }
    else
    {
        // m_bNewDb = false;
        nLen = strlen(m_pszFilename);
        osDbName = m_pszFilename;
    }

    m_pszDBname = new char [nLen+1];
    std::strncpy(m_pszDBname, osDbName.c_str(), nLen);
    m_pszDBname[nLen] = 0;

    CPLDebug("OGR-VFK", "Using internal DB: %s",
             m_pszDBname);

    if( !m_bDbSource && VSIStatL(osDbName, &sStatBufDb) == 0 )
    {
        /* Internal DB exists */
        if (CPLTestBool(CPLGetConfigOption("OGR_VFK_DB_OVERWRITE", "NO"))) {
            m_bNewDb = true;     // Overwrite existing DB.
            CPLDebug("OGR-VFK", "Internal DB (%s) already exists and will be overwritten",
                     m_pszDBname);
            VSIUnlink(osDbName);
        }
        else
        {
            if (pszDbNameConf == nullptr &&
                m_poFStat->st_mtime > sStatBufDb.st_mtime) {
                CPLDebug("OGR-VFK",
                         "Found %s but ignoring because it appears\n"
                         "be older than the associated VFK file.",
                         osDbName.c_str());
                m_bNewDb = true;
                VSIUnlink(osDbName);
            }
            else
            {
                m_bNewDb = false;    /* re-use existing DB */
            }
        }
    }

    CPLDebug("OGR-VFK", "New DB: %s Spatial: %s",
             m_bNewDb ? "yes" : "no", m_bSpatial ? "yes" : "no");

    if (SQLITE_OK != sqlite3_open(osDbName, &m_poDB)) {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Creating SQLite DB failed: %s",
                 sqlite3_errmsg(m_poDB));
    }

    CPLString osCommand;
    if( m_bDbSource )
    {
        /* check if it is really VFK DB datasource */
        char* pszErrMsg = nullptr;
        char** papszResult = nullptr;
        int nRowCount = 0;
        int nColCount = 0;

        osCommand.Printf("SELECT * FROM sqlite_master WHERE type='table' AND name='%s'",
                         VFK_DB_TABLE);
        sqlite3_get_table(m_poDB,
                          osCommand.c_str(),
                          &papszResult,
                          &nRowCount, &nColCount, &pszErrMsg);
        sqlite3_free_table(papszResult);
        sqlite3_free(pszErrMsg);

        if (nRowCount != 1) {
            /* DB is not valid VFK datasource */
            sqlite3_close(m_poDB);
            m_poDB = nullptr;
            return;
        }
    }

    if( !m_bNewDb )
    {
        /* check if DB is up-to-date datasource */
        char* pszErrMsg = nullptr;
        char** papszResult = nullptr;
        int nRowCount = 0;
        int nColCount = 0;

        osCommand.Printf("SELECT * FROM %s LIMIT 1", VFK_DB_TABLE);
        sqlite3_get_table(m_poDB,
                          osCommand.c_str(),
                          &papszResult,
                          &nRowCount, &nColCount, &pszErrMsg);
        sqlite3_free_table(papszResult);
        sqlite3_free(pszErrMsg);

        if (nColCount != 7) {
            /* it seems that DB is outdated, let's create new DB from
             * scratch */
            if( m_bDbSource )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid VFK DB datasource");
            }

            if (SQLITE_OK != sqlite3_close(m_poDB)) {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Closing SQLite DB failed: %s",
                         sqlite3_errmsg(m_poDB));
            }
            VSIUnlink(osDbName);
            if (SQLITE_OK != sqlite3_open(osDbName, &m_poDB)) {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Creating SQLite DB failed: %s",
                         sqlite3_errmsg(m_poDB));
            }
            CPLDebug("OGR-VFK", "Internal DB (%s) is invalid - will be re-created",
                     m_pszDBname);

            m_bNewDb = true;
        }
    }

    char* pszErrMsg = nullptr;
    CPL_IGNORE_RET_VAL(sqlite3_exec(m_poDB, "PRAGMA synchronous = OFF",
                                    nullptr, nullptr, &pszErrMsg));
    sqlite3_free(pszErrMsg);

    if( m_bNewDb )
    {
        OGRSpatialReference *poSRS;

        /* new DB, create support metadata tables */
        osCommand.Printf(
            "CREATE TABLE %s (file_name text, file_size integer, "
            "table_name text, num_records integer, "
            "num_features integer, num_geometries integer, table_defn text)",
            VFK_DB_TABLE);
        ExecuteSQL(osCommand.c_str());

        /* header table */
        osCommand.Printf(
            "CREATE TABLE %s (key text, value text)", VFK_DB_HEADER_TABLE);
        ExecuteSQL(osCommand.c_str());

        /* geometry_columns */
        osCommand.Printf(
            "CREATE TABLE %s (f_table_name text, f_geometry_column text, "
            "geometry_type integer, coord_dimension integer, "
            "srid integer, geometry_format text)", VFK_DB_GEOMETRY_TABLE);
        ExecuteSQL(osCommand.c_str());

        /* spatial_ref_sys */
        osCommand.Printf(
            "CREATE TABLE %s (srid interer, auth_name text, auth_srid text, "
            "srtext text)", VFK_DB_SPATIAL_REF_TABLE);
        ExecuteSQL(osCommand.c_str());

        /* insert S-JTSK into spatial_ref_sys table */
        poSRS = new OGRSpatialReference();
        poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if (poSRS->importFromEPSG(5514) != OGRERR_FAILURE)
        {
            char *pszWKT = nullptr;
            poSRS->exportToWkt(&pszWKT);
            osCommand.Printf("INSERT INTO %s (srid, auth_name, auth_srid, "
                             "srtext) VALUES (5514, 'EPSG', 5514, '%s')",
                             VFK_DB_SPATIAL_REF_TABLE, pszWKT);
            ExecuteSQL(osCommand.c_str());
            CPLFree(pszWKT);
        }
        delete poSRS;
    }
}

/*!
  \brief VFKReaderSQLite destructor
*/
VFKReaderSQLite::~VFKReaderSQLite()
{
    /* clean loaded properties */
    for (int i = 0; i < m_nDataBlockCount; i++)
        m_papoDataBlock[i]->CleanProperties();

    /* close backend SQLite DB */
    if( SQLITE_OK != sqlite3_close(m_poDB) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Closing SQLite DB failed: %s",
                 sqlite3_errmsg(m_poDB));
    }
    CPLDebug("OGR-VFK", "Internal DB (%s) closed", m_pszDBname);

    /* delete backend SQLite DB if requested */
    if( CPLTestBool(CPLGetConfigOption("OGR_VFK_DB_DELETE", "NO")) )
    {
        CPLDebug("OGR-VFK", "Internal DB (%s) deleted", m_pszDBname);
        VSIUnlink(m_pszDBname);
    }
    delete[] m_pszDBname;
}

/*!
  \brief Load data block definitions (&B)

  Call VFKReader::OpenFile() before this function.

  \return number of data blocks or -1 on error
*/
int VFKReaderSQLite::ReadDataBlocks(bool bSuppressGeometry)
{
    CPLString osSQL;
    osSQL.Printf("SELECT table_name, table_defn FROM %s", VFK_DB_TABLE);
    sqlite3_stmt *hStmt = PrepareStatement(osSQL.c_str());
    while(ExecuteSQL(hStmt) == OGRERR_NONE) {
        const char *pszName = (const char*) sqlite3_column_text(hStmt, 0);
        const char *pszDefn = (const char*) sqlite3_column_text(hStmt, 1);
        if( pszName && pszDefn )
        {
            IVFKDataBlock *poNewDataBlock =
                (IVFKDataBlock *) CreateDataBlock(pszName);
            poNewDataBlock->SetGeometryType(bSuppressGeometry);
            if ( poNewDataBlock->GetGeometryType() != wkbNone ) {
                /* may happen:
                 * first open with "-oo SUPPRESS_GEOMETRY=YES --config OGR_VFK_DB_READ_ALL_BLOCKS NO"
                 * than attempt to fetch feature from geometry-included layer: SOBR -fid 1
                 */
                ((VFKDataBlockSQLite *) poNewDataBlock)->AddGeometryColumn();
            }
            poNewDataBlock->SetProperties(pszDefn);
            VFKReader::AddDataBlock(poNewDataBlock, nullptr);
        }
    }

    CPL_IGNORE_RET_VAL(sqlite3_exec(m_poDB, "BEGIN", nullptr, nullptr, nullptr));
    /* Read data from VFK file */
    const int nDataBlocks = VFKReader::ReadDataBlocks(bSuppressGeometry);
    CPL_IGNORE_RET_VAL(sqlite3_exec(m_poDB, "COMMIT", nullptr, nullptr, nullptr));

    return nDataBlocks;
}

/*!
  \brief Load data records (&D)

  Call VFKReader::OpenFile() before this function.

  \param poDataBlock limit to selected data block or NULL for all

  \return number of data records or -1 on error
*/
int VFKReaderSQLite::ReadDataRecords(IVFKDataBlock *poDataBlock)
{
    CPLString   osSQL;
    IVFKDataBlock *poDataBlockCurrent = nullptr;
    sqlite3_stmt *hStmt = nullptr;
    const char *pszName = nullptr;
    int nDataRecords = 0;
    bool bReadVfk = !m_bDbSource;
    bool bReadDb = false;

    if (poDataBlock) { /* read records only for selected data block */
        /* table name */
        pszName = poDataBlock->GetName();

        /* check for existing records (re-use already inserted data) */
        osSQL.Printf("SELECT num_records FROM %s WHERE "
                     "table_name = '%s'",
                     VFK_DB_TABLE, pszName);
        hStmt = PrepareStatement(osSQL.c_str());
        if (ExecuteSQL(hStmt) == OGRERR_NONE) {
            nDataRecords = sqlite3_column_int(hStmt, 0);
            if (nDataRecords > 0)
                bReadDb = true; /* -> read from DB */
            else
                nDataRecords = 0;
        }
        sqlite3_finalize(hStmt);
    }
    else
    {                     /* read all data blocks */
        /* check for existing records (re-use already inserted data) */
        osSQL.Printf("SELECT COUNT(*) FROM %s WHERE num_records > 0", VFK_DB_TABLE);
        hStmt = PrepareStatement(osSQL.c_str());
        if (ExecuteSQL(hStmt) == OGRERR_NONE &&
            sqlite3_column_int(hStmt, 0) != 0)
            bReadDb = true;     /* -> read from DB */
        sqlite3_finalize(hStmt);

        /* check if file is already registered in DB (requires file_size column) */
        osSQL.Printf("SELECT COUNT(*) FROM %s WHERE file_name = '%s' AND "
                     "file_size = " CPL_FRMT_GUIB " AND num_records > 0",
                     VFK_DB_TABLE, CPLGetFilename(m_pszFilename),
                     (GUIntBig) m_poFStat->st_size);
        hStmt = PrepareStatement(osSQL.c_str());
        if (ExecuteSQL(hStmt) == OGRERR_NONE &&
            sqlite3_column_int(hStmt, 0) > 0) {
            /* -> file already registered (filename & size is the same) */
            CPLDebug("OGR-VFK", "VFK file %s already loaded in DB", m_pszFilename);
            bReadVfk = false;
        }
        sqlite3_finalize(hStmt);
    }

    if( bReadDb )
    {  /* read records from DB */
        /* read from  DB */
        VFKFeatureSQLite *poNewFeature = nullptr;

        poDataBlockCurrent = nullptr;
        for( int iDataBlock = 0;
             iDataBlock < GetDataBlockCount();
             iDataBlock++ )
        {
            poDataBlockCurrent = GetDataBlock(iDataBlock);

            if (poDataBlock && poDataBlock != poDataBlockCurrent)
                continue;

            poDataBlockCurrent->SetFeatureCount(0); /* avoid recursive call */

            pszName = poDataBlockCurrent->GetName();
            CPLAssert(nullptr != pszName);

            osSQL.Printf("SELECT %s,_rowid_ FROM %s ",
                         FID_COLUMN, pszName);
            if (EQUAL(pszName, "SBP") || EQUAL(pszName, "SBPG"))
                osSQL += "WHERE PORADOVE_CISLO_BODU = 1 ";
            osSQL += "ORDER BY ";
            osSQL += FID_COLUMN;
            hStmt = PrepareStatement(osSQL.c_str());
            nDataRecords = 0;
            while (ExecuteSQL(hStmt) == OGRERR_NONE) {
                const long iFID = sqlite3_column_int(hStmt, 0);
                int iRowId = sqlite3_column_int(hStmt, 1);
                poNewFeature = new VFKFeatureSQLite(poDataBlockCurrent, iRowId, iFID);
                poDataBlockCurrent->AddFeature(poNewFeature);
                nDataRecords++;
            }

            /* check DB consistency */
            osSQL.Printf("SELECT num_features FROM %s WHERE table_name = '%s'",
                         VFK_DB_TABLE, pszName);
            hStmt = PrepareStatement(osSQL.c_str());
            if (ExecuteSQL(hStmt) == OGRERR_NONE) {
                const int nFeatDB = sqlite3_column_int(hStmt, 0);
                if (nFeatDB > 0 && nFeatDB != poDataBlockCurrent->GetFeatureCount())
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "%s: Invalid number of features " CPL_FRMT_GIB " (should be %d)",
                             pszName, poDataBlockCurrent->GetFeatureCount(), nFeatDB);
            }
            sqlite3_finalize(hStmt);
        }
    }

    if( bReadVfk )
    {  /* read from VFK file and insert records into DB */
        /* begin transaction */
        ExecuteSQL("BEGIN");

        /* Store VFK header to DB */
        StoreInfo2DB();

        /* Insert VFK data records into DB */
        nDataRecords += VFKReader::ReadDataRecords(poDataBlock);

        /* update VFK_DB_TABLE table */
        poDataBlockCurrent = nullptr;
        for( int iDataBlock = 0;
             iDataBlock < GetDataBlockCount();
             iDataBlock++ )
        {
            poDataBlockCurrent = GetDataBlock(iDataBlock);

            if (poDataBlock && poDataBlock != poDataBlockCurrent)
                continue;

            /* update number of records in metadata table */
            osSQL.Printf("UPDATE %s SET num_records = %d WHERE "
                         "table_name = '%s'",
                         VFK_DB_TABLE, poDataBlockCurrent->GetRecordCount(),
                         poDataBlockCurrent->GetName());

            ExecuteSQL(osSQL);
        }

        /* create indices if not exist */
        CreateIndices();

        /* commit transaction */
        ExecuteSQL("COMMIT");
    }

    return nDataRecords;
}

/*!
  \brief Store header info to VFK_DB_HEADER
*/
void VFKReaderSQLite::StoreInfo2DB()
{
    for( std::map<CPLString, CPLString>::iterator i = poInfo.begin();
         i != poInfo.end(); ++i )
    {
        const char *value = i->second.c_str();

        const char q = (value[0] == '"') ? ' ' : '"';

        CPLString osSQL;
        osSQL.Printf("INSERT INTO %s VALUES(\"%s\", %c%s%c)",
                     VFK_DB_HEADER_TABLE, i->first.c_str(),
                     q, value, q);
        ExecuteSQL(osSQL);
    }
}

/*!
  \brief Create indices for newly added db tables (datablocks)
*/
void VFKReaderSQLite::CreateIndices()
{
    const char *pszBlockName;
    CPLString osIndexName, osSQL;
    VFKDataBlockSQLite *poDataBlock;

    sqlite3_stmt *hStmt;

    for( int iLayer = 0; iLayer < GetDataBlockCount(); iLayer++ ) {
        poDataBlock = (VFKDataBlockSQLite *) GetDataBlock(iLayer);
        pszBlockName = poDataBlock->GetName();

        /* ogr_fid */
        osIndexName.Printf("%s_%s", pszBlockName, FID_COLUMN);

        /* check if index on ogr_fid column exists */
        osSQL.Printf("SELECT COUNT(*) FROM sqlite_master WHERE type = 'index' AND name = '%s'",
                     osIndexName.c_str());
        hStmt = PrepareStatement(osSQL.c_str());

        if( ExecuteSQL(hStmt) == OGRERR_NONE &&
            sqlite3_column_int(hStmt, 0) > 0 ) {
            /* index on ogr_fid column exists, skip creating indices
               for current datablock */
            sqlite3_finalize(hStmt);
            continue;
        }
        sqlite3_finalize(hStmt);

        /* create index on ogr_fid */
        CreateIndex(osIndexName.c_str(), pszBlockName, FID_COLUMN,
                    !(EQUAL(pszBlockName, "SBP") || EQUAL(pszBlockName, "SBPG")));

        if ( poDataBlock->GetGeometryType() == wkbNone ) {
            /* skip geometry-related indices */
            continue;
        }

        if( EQUAL (pszBlockName, "SOBR") ||
            EQUAL (pszBlockName, "OBBP") ||
            EQUAL (pszBlockName, "SPOL") ||
            EQUAL (pszBlockName, "OB") ||
            EQUAL (pszBlockName, "OP") ||
            EQUAL (pszBlockName, "OBPEJ") ||
            EQUAL (pszBlockName, "SBP") ||
            EQUAL (pszBlockName, "SBPG") ||
            EQUAL (pszBlockName, "HP") ||
            EQUAL (pszBlockName, "DPM") ||
            EQUAL (pszBlockName, "ZVB") ||
            EQUAL (pszBlockName, "PAR") ||
            EQUAL (pszBlockName, "BUD") ) {
            const char *pszKey = ((VFKDataBlockSQLite *) poDataBlock)->GetKey();
            if( pszKey ) {
                /* ID */
                osIndexName.Printf("%s_%s", pszBlockName, pszKey);
                CreateIndex(osIndexName.c_str(), pszBlockName, pszKey, !m_bAmendment);
            }
        }

        /* create other indices used for building geometry */
        if( EQUAL(pszBlockName, "SBP") ) {
            /* SBP */
            CreateIndex("SBP_OB",        pszBlockName, "OB_ID", false);
            CreateIndex("SBP_HP",        pszBlockName, "HP_ID", false);
            CreateIndex("SBP_DPM",       pszBlockName, "DPM_ID", false);
            CreateIndex("SBP_OB_HP_DPM", pszBlockName, "OB_ID,HP_ID,DPM_ID", true);
            CreateIndex("SBP_OB_POR",    pszBlockName, "OB_ID,PORADOVE_CISLO_BODU", false);
            CreateIndex("SBP_HP_POR",    pszBlockName, "HP_ID,PORADOVE_CISLO_BODU", false);
            CreateIndex("SBP_DPM_POR",   pszBlockName, "DPM_ID,PORADOVE_CISLO_BODU", false);
        }
        else if( EQUAL(pszBlockName, "HP") ) {
            /* HP */
            CreateIndex("HP_PAR1",        pszBlockName, "PAR_ID_1", false);
            CreateIndex("HP_PAR2",        pszBlockName, "PAR_ID_2", false);
        }
        else if( EQUAL(pszBlockName, "OB") ) {
            /* OP */
            CreateIndex("OB_BUD",        pszBlockName, "BUD_ID", false);
        }
    }
}

/*!
  \brief Create index

  If creating unique index fails, then non-unique index is created instead.

  \param name index name
  \param table table name
  \param column column(s) name
  \param unique true to create unique index
*/
void VFKReaderSQLite::CreateIndex(const char *name, const char *table, const char *column,
                                  bool unique)
{
    CPLString   osSQL;

    if (unique) {
        osSQL.Printf("CREATE UNIQUE INDEX %s ON %s (%s)",
                     name, table, column);
        if (ExecuteSQL(osSQL.c_str()) == OGRERR_NONE) {
            return;
        }
    }

    osSQL.Printf("CREATE INDEX %s ON %s (%s)",
                 name, table, column);
    ExecuteSQL(osSQL.c_str());
}

/*!
  \brief Create new data block

  \param pszBlockName name of the block to be created

  \return pointer to VFKDataBlockSQLite instance
*/
IVFKDataBlock *VFKReaderSQLite::CreateDataBlock(const char *pszBlockName)
{
    /* create new data block, i.e. table in DB */
    return new VFKDataBlockSQLite(pszBlockName, (IVFKReader *) this);
}

/*!
  \brief Create DB table from VFKDataBlock (SQLITE only)

  \param poDataBlock pointer to VFKDataBlock instance
*/
void VFKReaderSQLite::AddDataBlock(IVFKDataBlock *poDataBlock, const char *pszDefn)
{
    CPLString osColumn;

    const char *pszBlockName = poDataBlock->GetName();

    /* register table in VFK_DB_TABLE */
    CPLString osCommand;
    osCommand.Printf("SELECT COUNT(*) FROM %s WHERE "
                     "table_name = '%s'",
                     VFK_DB_TABLE, pszBlockName);
    sqlite3_stmt *hStmt = PrepareStatement(osCommand.c_str());

    if (ExecuteSQL(hStmt) == OGRERR_NONE )
    {
        if( sqlite3_column_int(hStmt, 0) == 0) {

            osCommand.Printf("CREATE TABLE IF NOT EXISTS '%s' (", pszBlockName);
            for (int i = 0; i < poDataBlock->GetPropertyCount(); i++) {
                VFKPropertyDefn *poPropertyDefn = poDataBlock->GetProperty(i);
                if (i > 0)
                    osCommand += ",";
                osColumn.Printf("%s %s", poPropertyDefn->GetName(),
                                poPropertyDefn->GetTypeSQL().c_str());
                osCommand += osColumn;
            }
            osColumn.Printf(",%s integer", FID_COLUMN);
            osCommand += osColumn;
            if (poDataBlock->GetGeometryType() != wkbNone) {
                osColumn.Printf(",%s blob", GEOM_COLUMN);
                osCommand += osColumn;
            }
            osCommand += ")";
            ExecuteSQL(osCommand.c_str()); /* CREATE TABLE */

            /* update VFK_DB_TABLE meta-table */
            osCommand.Printf("INSERT INTO %s (file_name, file_size, table_name, "
                             "num_records, num_features, num_geometries, table_defn) VALUES "
                             "('%s', " CPL_FRMT_GUIB ", '%s', -1, 0, 0, '%s')",
                             VFK_DB_TABLE, CPLGetFilename(m_pszFilename),
                             (GUIntBig) m_poFStat->st_size,
                             pszBlockName, pszDefn);
            ExecuteSQL(osCommand.c_str());

            int geom_type = ((VFKDataBlockSQLite *) poDataBlock)->GetGeometrySQLType();
            /* update VFK_DB_GEOMETRY_TABLE */
            osCommand.Printf("INSERT INTO %s (f_table_name, f_geometry_column, geometry_type, "
                             "coord_dimension, srid, geometry_format) VALUES "
                             "('%s', '%s', %d, 2, 5514, 'WKB')",
                             VFK_DB_GEOMETRY_TABLE, pszBlockName, GEOM_COLUMN, geom_type);
            ExecuteSQL(osCommand.c_str());
        }
        sqlite3_finalize(hStmt);
    }

    return VFKReader::AddDataBlock(poDataBlock, nullptr);
}

/*!
  \brief Prepare SQL statement

  \param pszSQLCommand SQL statement to be prepared

  \return pointer to sqlite3_stmt instance or NULL on error
*/
sqlite3_stmt *VFKReaderSQLite::PrepareStatement(const char *pszSQLCommand)
{
    CPLDebug("OGR-VFK", "VFKReaderSQLite::PrepareStatement(): %s", pszSQLCommand);

    sqlite3_stmt *hStmt = nullptr;
    const int rc = sqlite3_prepare_v2(m_poDB, pszSQLCommand, -1,
                                   &hStmt, nullptr);

    // TODO(schwehr): if( rc == SQLITE_OK ) return NULL;
    if (rc != SQLITE_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "In PrepareStatement(): sqlite3_prepare_v2(%s):\n  %s",
                 pszSQLCommand, sqlite3_errmsg(m_poDB));

        if(hStmt != nullptr) {
            sqlite3_finalize(hStmt);
        }

        return nullptr;
    }

    return hStmt;
}

/*!
  \brief Execute prepared SQL statement

  \param hStmt pointer to sqlite3_stmt

  \return OGRERR_NONE on success
*/
OGRErr VFKReaderSQLite::ExecuteSQL(sqlite3_stmt *& hStmt)
{
    const int rc = sqlite3_step(hStmt);
    if (rc != SQLITE_ROW) {
        if (rc == SQLITE_DONE) {
            sqlite3_finalize(hStmt);
            hStmt = nullptr;
            return OGRERR_NOT_ENOUGH_DATA;
        }

        CPLError(CE_Failure, CPLE_AppDefined,
                 "In ExecuteSQL(): sqlite3_step:\n  %s",
                 sqlite3_errmsg(m_poDB));
        if (hStmt)
        {
            sqlite3_finalize(hStmt);
            hStmt = nullptr;
        }
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/*!
  \brief Execute SQL statement (SQLITE only)

  \param pszSQLCommand SQL command to execute
  \param eErrLevel if equal to CE_None, no error message will be emitted on failure.

  \return OGRERR_NONE on success or OGRERR_FAILURE on failure
*/
OGRErr VFKReaderSQLite::ExecuteSQL( const char *pszSQLCommand, CPLErr eErrLevel )
{
    char *pszErrMsg = nullptr;

    if( SQLITE_OK != sqlite3_exec(m_poDB, pszSQLCommand,
                                  nullptr, nullptr, &pszErrMsg) )
    {
        if ( eErrLevel != CE_None ) {
            CPLError(eErrLevel, CPLE_AppDefined,
                     "In ExecuteSQL(%s): %s",
                     pszSQLCommand, pszErrMsg ? pszErrMsg : "(null)");
        }
        sqlite3_free(pszErrMsg);

        return  OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/*!
  \brief Add feature

  \param poDataBlock pointer to VFKDataBlock instance
  \param poFeature pointer to VFKFeature instance
*/
OGRErr VFKReaderSQLite::AddFeature( IVFKDataBlock *poDataBlock,
                                    VFKFeature *poFeature )
{
    CPLString osValue;

    const VFKProperty *poProperty = nullptr;

    const char *pszBlockName = poDataBlock->GetName();
    CPLString osCommand;
    osCommand.Printf("INSERT INTO '%s' VALUES(", pszBlockName);

    for( int i = 0; i < poDataBlock->GetPropertyCount(); i++ )
    {
        const OGRFieldType ftype = poDataBlock->GetProperty(i)->GetType();
        poProperty = poFeature->GetProperty(i);
        if (i > 0)
            osCommand += ",";

        if( poProperty->IsNull() )
        {
            osValue.Printf("NULL");
        }
        else
        {
            switch (ftype) {
            case OFTInteger:
                osValue.Printf("%d", poProperty->GetValueI());
                break;
            case OFTInteger64:
                osValue.Printf(CPL_FRMT_GIB, poProperty->GetValueI64());
                break;
            case OFTReal:
                osValue.Printf("%f", poProperty->GetValueD());
                break;
            case OFTString:
                osValue.Printf("'%s'", poProperty->GetValueS(true));
                break;
            default:
                osValue.Printf("'%s'", poProperty->GetValueS(true));
                break;
            }
        }
        osCommand += osValue;
    }
    osValue.Printf("," CPL_FRMT_GIB, poFeature->GetFID());
    if (poDataBlock->GetGeometryType() != wkbNone) {
        osValue += ",NULL";
    }
    osCommand += osValue;
    osCommand += ")";

    if( ExecuteSQL(osCommand.c_str(), CE_Warning) != OGRERR_NONE )
        return OGRERR_FAILURE;

    if ( EQUAL(pszBlockName, "SBP") || EQUAL(pszBlockName, "SBPG") ) {
        poProperty = poFeature->GetProperty("PORADOVE_CISLO_BODU");
        if( poProperty == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot find property PORADOVE_CISLO_BODU");
            return OGRERR_FAILURE;
        }
        if (poProperty->GetValueI64() != 1)
            return OGRERR_NONE;
    }

    VFKFeatureSQLite *poNewFeature =
        new VFKFeatureSQLite(poDataBlock,
                             poDataBlock->GetRecordCount(RecordValid) + 1,
                             poFeature->GetFID());
    poDataBlock->AddFeature(poNewFeature);

    return OGRERR_NONE;
}
