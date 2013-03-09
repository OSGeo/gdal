/******************************************************************************
 * $Id$
 *
 * Project:  VFK Reader (SQLite)
 * Purpose:  Implements VFKReaderSQLite class.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2012-2013, Martin Landa <landa.martin gmail.com>
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

#define SUPPORT_GEOMETRY

#ifdef SUPPORT_GEOMETRY
#  include "ogr_geometry.h"
#endif

/*!
  \brief VFKReaderSQLite constructor
*/
VFKReaderSQLite::VFKReaderSQLite(const char *pszFilename) : VFKReader(pszFilename)
{
    const char *pszDbNameConf;
    CPLString   pszDbName;
    CPLString   osCommand;
    VSIStatBufL sStatBuf;
    bool        bNewDb;
    
    /* open tmp SQLite DB (re-use DB file if already exists) */
    pszDbNameConf = CPLGetConfigOption("OGR_VFK_DB_NAME", NULL);
    if (pszDbNameConf) {
	pszDbName = pszDbNameConf;
    }
    else {
	pszDbName.Printf("%s.db", m_pszFilename);
    }
    
    if (CSLTestBoolean(CPLGetConfigOption("OGR_VFK_DB_SPATIAL", "YES")))
	m_bSpatial = TRUE;    /* build geometry from DB */
    else
	m_bSpatial = FALSE;   /* store also geometry in DB */
    
    bNewDb = TRUE;
    if (VSIStatL(pszDbName, &sStatBuf ) == 0) {
	if (CSLTestBoolean(CPLGetConfigOption("OGR_VFK_DB_OVERWRITE", "NO"))) {
	    bNewDb = TRUE;     /* overwrite existing DB */
	    VSIUnlink(pszDbName);
	}
	else {
	    bNewDb = FALSE;    /* re-use exising DB */
	}
    }
    else {
      	CPLError(CE_Warning, CPLE_AppDefined, 
                 "SQLite DB not found. Reading VFK data may take some time...");
    }
    CPLDebug("OGR-VFK", "New DB: %s Spatial: %s",
	     bNewDb ? "yes" : "no", m_bSpatial ? "yes" : "no");

    if (SQLITE_OK != sqlite3_open(pszDbName, &m_poDB)) {
        CPLError(CE_Failure, CPLE_AppDefined, 
                 "Creating SQLite DB failed");
    }
    else {
        char* pszErrMsg = NULL;
        sqlite3_exec(m_poDB, "PRAGMA synchronous = OFF", NULL, NULL, &pszErrMsg);
        sqlite3_free(pszErrMsg);
    }
    
    if (bNewDb) {
        /* new DB, create support metadata tables */
        osCommand = "CREATE TABLE 'vfk_blocks' "
	  "(file_name text, table_name text, num_records integer, "
	    "num_geometries integer, table_defn text)";
        ExecuteSQL(osCommand.c_str());
    }
}

/*!
  \brief VFKReaderSQLite destructor
*/
VFKReaderSQLite::~VFKReaderSQLite()
{
    CPLString pszDbName(m_pszFilename);

    pszDbName += ".db";
    
    /* close tmp SQLite DB */
    if (SQLITE_OK != sqlite3_close(m_poDB)) {
        CPLError(CE_Failure, CPLE_AppDefined, 
                 "Closing SQLite DB failed\n  %s",
                 sqlite3_errmsg(m_poDB));
    }
}

/*!
  \brief Load data block definitions (&B)

  Call VFKReader::OpenFile() before this function.

  \return number of data blocks or -1 on error
*/
int VFKReaderSQLite::ReadDataBlocks()
{
    int  nDataBlocks = -1;
    CPLString osSQL;
    const char *pszName, *pszDefn;
    IVFKDataBlock *poNewDataBlock;
    
    sqlite3_stmt *hStmt;
    
    osSQL.Printf("SELECT table_name, table_defn FROM 'vfk_blocks'");
    hStmt = PrepareStatement(osSQL.c_str());
    while(ExecuteSQL(hStmt) == OGRERR_NONE) {
        pszName = (const char*) sqlite3_column_text(hStmt, 0);
        pszDefn = (const char*) sqlite3_column_text(hStmt, 1);
        poNewDataBlock = (IVFKDataBlock *) CreateDataBlock(pszName);
        poNewDataBlock->SetGeometryType();
        poNewDataBlock->SetProperties(pszDefn);
        VFKReader::AddDataBlock(poNewDataBlock, NULL);
    }
    
    if (m_nDataBlockCount == 0) {
        sqlite3_exec(m_poDB, "BEGIN", 0, 0, 0);  
        /* CREATE TABLE ... */
        nDataBlocks = VFKReader::ReadDataBlocks();
        sqlite3_exec(m_poDB, "COMMIT", 0, 0, 0);
    }
    
    return nDataBlocks;
}

/*!
  \brief Load data records (&D)

  Call VFKReader::OpenFile() before this function.
  
  \return number of data records or -1 on error
*/
int VFKReaderSQLite::ReadDataRecords(IVFKDataBlock *poDataBlock)
{
    int         nDataRecords;
    const char *pszName;
    CPLString   osSQL;

    sqlite3_stmt *hStmt;
    
    nDataRecords = 0;
    
    /* table name */
    pszName = poDataBlock->GetName();
    
    /* check for existing records (re-use already inserted data) */
    osSQL.Printf("SELECT num_records FROM vfk_blocks WHERE "
                 "table_name = '%s'",
                 pszName);
    hStmt = PrepareStatement(osSQL.c_str());
    nDataRecords = -1;
    if (ExecuteSQL(hStmt) == OGRERR_NONE) {
        nDataRecords = sqlite3_column_int(hStmt, 0);
    }
    sqlite3_finalize(hStmt);

    poDataBlock->SetFeatureCount(0); /* avoid recursive calls */
    
    if (nDataRecords > -1) {
        /* read from  DB */
        long iFID;
        VFKFeatureSQLite *poNewFeature = NULL;

        if (EQUAL(pszName, "SBP")) {
            return 0; /* see LoadGeometry() */
        }
            
        osSQL.Printf("SELECT %s FROM %s", FID_COLUMN, pszName);
        hStmt = PrepareStatement(osSQL.c_str());
        nDataRecords = 1;
        while (ExecuteSQL(hStmt) == OGRERR_NONE) {
            iFID = sqlite3_column_int(hStmt, 0);
            poNewFeature = new VFKFeatureSQLite(poDataBlock, nDataRecords++, iFID);
            poDataBlock->AddFeature(poNewFeature);
        }
    }
    else {
        /* read from VFK file and insert records into DB */
        bool bUnique;
        const char *pszKey;
        
        bUnique = !CSLTestBoolean(CPLGetConfigOption("OGR_VFK_DB_IGNORE_DUPLICATES", "NO"));
        
        /* begin transaction */
        ExecuteSQL("BEGIN");
        
        /* create indeces */
        osSQL.Printf("%s_%s", pszName, FID_COLUMN);
        CreateIndex(osSQL.c_str(), pszName, FID_COLUMN, !EQUAL(pszName, "SBP"));

        pszKey = ((VFKDataBlockSQLite *) poDataBlock)->GetKey();
        if (pszKey) {
            osSQL.Printf("%s_%s", pszName, pszKey);
            CreateIndex(osSQL.c_str(), pszName, pszKey, bUnique);
        }
        
        if (EQUAL(pszName, "SBP")) {
            /* create extra indices for SBP */
            CreateIndex("SBP_OB",        pszName, "OB_ID", FALSE);
            CreateIndex("SBP_HP",        pszName, "HP_ID", FALSE);
            CreateIndex("SBP_DPM",       pszName, "DPM_ID", FALSE);
            CreateIndex("SBP_OB_HP_DPM", pszName, "OB_ID,HP_ID,DPM_ID", bUnique);
            CreateIndex("SBP_OB_POR",    pszName, "OB_ID,PORADOVE_CISLO_BODU", FALSE);
            CreateIndex("SBP_HP_POR",    pszName, "HP_ID,PORADOVE_CISLO_BODU", FALSE);
            CreateIndex("SBP_DPM_POR",   pszName, "DPM_ID,PORADOVE_CISLO_BODU", FALSE);
        }
        else if (EQUAL(pszName, "HP")) {
            /* create extra indices for HP */
            CreateIndex("HP_PAR1",        pszName, "PAR_ID_1", FALSE);
            CreateIndex("HP_PAR2",        pszName, "PAR_ID_2", FALSE);
        }
        else if (EQUAL(pszName, "OB")) {
            /* create extra indices for OP */
            CreateIndex("OB_BUD",        pszName, "BUD_ID", FALSE);
        }
        
        /* INSERT ... */
        nDataRecords = VFKReader::ReadDataRecords(poDataBlock);
        
        /* update 'vfk_blocks' table */
        osSQL.Printf("UPDATE vfk_blocks SET num_records = %d WHERE "
		     "table_name = '%s'",
                     nDataRecords, pszName);
	ExecuteSQL(osSQL);
        
        /* commit transaction */
        ExecuteSQL("COMMIT");
    }
    
    return nDataRecords;
}

/*!
  \brief Create index

  If creating unique index fails, then non-unique index is created instead.

  \param name index name
  \param table table name
  \param column column(s) name
  \param unique TRUE to create unique index
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
    return new VFKDataBlockSQLite(pszBlockName, (IVFKReader *) this);
}

/*!
  \brief Create DB table from VFKDataBlock (SQLITE only)

  \param poDataBlock pointer to VFKDataBlock instance
*/
void VFKReaderSQLite::AddDataBlock(IVFKDataBlock *poDataBlock, const char *pszDefn)
{
    CPLString osCommand, osColumn;
    
    VFKPropertyDefn *poPropertyDefn;
    
    sqlite3_stmt *hStmt;

    /* register table in 'vfk_blocks' */
    osCommand.Printf("SELECT COUNT(*) FROM vfk_blocks WHERE "
                     "table_name = '%s'",
                     poDataBlock->GetName());
    hStmt = PrepareStatement(osCommand.c_str());
    if (ExecuteSQL(hStmt) == OGRERR_NONE &&
        sqlite3_column_int(hStmt, 0) == 0) {
        
        osCommand.Printf("CREATE TABLE '%s' (", poDataBlock->GetName());
        for (int i = 0; i < poDataBlock->GetPropertyCount(); i++) {
            poPropertyDefn = poDataBlock->GetProperty(i);
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
        
        osCommand.Printf("INSERT INTO 'vfk_blocks' (file_name, table_name, "
                         "num_records, num_geometries, table_defn) VALUES "
			 "('%s', '%s', -1, 0, '%s')",
			 m_pszFilename, poDataBlock->GetName(), pszDefn);
	
        ExecuteSQL(osCommand.c_str());

        sqlite3_finalize(hStmt);
    }
        
    return VFKReader::AddDataBlock(poDataBlock, NULL);
}

/*!
  \brief Prepare SQL statement
  
  \param pszSQLCommand SQL statement to be prepared

  \return pointer to sqlite3_stmt instance or NULL on error
*/
sqlite3_stmt *VFKReaderSQLite::PrepareStatement(const char *pszSQLCommand)
{
    int rc;
    sqlite3_stmt *hStmt = NULL;
    
    /* CPLDebug("OGR-VFK", "PrepareStatement(): %s", pszSQLCommand); */

    rc = sqlite3_prepare(m_poDB, pszSQLCommand, strlen(pszSQLCommand),
                         &hStmt, NULL);
    
    if (rc != SQLITE_OK) {
        CPLError(CE_Failure, CPLE_AppDefined, 
                 "In PrepareStatement(): sqlite3_prepare(%s):\n  %s",
                 pszSQLCommand, sqlite3_errmsg(m_poDB));
        
        if(hStmt != NULL) {
            sqlite3_finalize(hStmt);
        }
        
        return NULL;
    }

    return hStmt;
}

/*!
  \brief Execute prepared SQL statement

  \param hStmt pointer to sqlite3_stmt 

  \return OGRERR_NONE on success
*/
OGRErr VFKReaderSQLite::ExecuteSQL(sqlite3_stmt *hStmt)
{
    int rc;
    
    // assert

    rc = sqlite3_step(hStmt);
    if (rc != SQLITE_ROW) {
        if (rc == SQLITE_DONE) {
            sqlite3_finalize(hStmt);
            return OGRERR_NOT_ENOUGH_DATA;
        }
        
        CPLError(CE_Failure, CPLE_AppDefined, 
                 "In ExecuteSQL(): sqlite3_step:\n  %s", 
                 sqlite3_errmsg(m_poDB));
        if (hStmt)
            sqlite3_finalize(hStmt);
        return OGRERR_FAILURE;
    }
    
    return OGRERR_NONE;

}

/*!
  \brief Execute SQL statement (SQLITE only)

  \return OGRERR_NONE on success or OGRERR_FAILURE on failure
*/
OGRErr VFKReaderSQLite::ExecuteSQL(const char *pszSQLCommand, bool bQuiet)
{
    char *pszErrMsg = NULL;
    
    /*
    CPLDebug("OGR-VFK", 
	     "ExecuteSQL(): %s", pszSQLCommand);
    */

    if (SQLITE_OK != sqlite3_exec(m_poDB, pszSQLCommand, NULL, NULL, &pszErrMsg)) {
        if (!bQuiet)
            CPLError(CE_Failure, CPLE_AppDefined, 
                     "In ExecuteSQL(%s): %s",
                     pszSQLCommand, pszErrMsg);
        return  OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/*!
  \brief Add feature

  \param poDataBlock pointer to VFKDataBlock instance
  \param poFeature pointer to VFKFeature instance
*/
OGRErr VFKReaderSQLite::AddFeature(IVFKDataBlock *poDataBlock, VFKFeature *poFeature)
{
    CPLString     osCommand;
    CPLString     osValue;
    
    const char   *pszBlockName;

    OGRFieldType  ftype;

    const VFKProperty *poProperty;

    pszBlockName = poDataBlock->GetName();
    osCommand.Printf("INSERT INTO '%s' VALUES(", pszBlockName);
    
    for (int i = 0; i < poDataBlock->GetPropertyCount(); i++) {
        ftype = poDataBlock->GetProperty(i)->GetType();
        poProperty = poFeature->GetProperty(i);
        if (i > 0)
            osCommand += ",";
        if (poProperty->IsNull())
            osValue.Printf("NULL");
        else {
            switch (ftype) {
            case OFTInteger:
                osValue.Printf("%d", poProperty->GetValueI());
                break;
            case OFTReal:
                osValue.Printf("%f", poProperty->GetValueD());
                break;
            case OFTString:
                if (poDataBlock->GetProperty(i)->IsIntBig())
		    osValue.Printf("%s", poProperty->GetValueS());
                else
                    osValue.Printf("'%s'", poProperty->GetValueS());
                break;
            default:
                osValue.Printf("'%s'", poProperty->GetValueS());
                break;
            }
        }
        osCommand += osValue;
    }
    osValue.Printf(",%lu", poFeature->GetFID());
    if (poDataBlock->GetGeometryType() != wkbNone) {
	osValue += ",NULL";
    }
    osValue += ")";
    osCommand += osValue;

    if (ExecuteSQL(osCommand.c_str(), TRUE) != OGRERR_NONE)
        return OGRERR_FAILURE;
    
    if (!EQUAL(pszBlockName, "SBP")) {
        VFKFeatureSQLite *poNewFeature;
        
        poNewFeature = new VFKFeatureSQLite(poDataBlock, poDataBlock->GetFeatureCount() + 1,
                                            poFeature->GetFID());
        poDataBlock->AddFeature(poNewFeature);
    }
    
    return OGRERR_NONE;
}
