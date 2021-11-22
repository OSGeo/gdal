/******************************************************************************
 *
 * Project:  VFK Reader - Feature definition (SQLite)
 * Purpose:  Implements VFKFeatureSQLite class.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2012-2018, Martin Landa <landa.martin gmail.com>
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

#include "vfkreader.h"
#include "vfkreaderp.h"

#include "cpl_conv.h"
#include "cpl_error.h"

CPL_CVSID("$Id$")

/*!
  \brief VFKFeatureSQLite constructor (from DB)

  Read VFK feature from DB

  \param poDataBlock pointer to related IVFKDataBlock
*/
VFKFeatureSQLite::VFKFeatureSQLite( IVFKDataBlock *poDataBlock ) :
    IVFKFeature(poDataBlock),
    // Starts at 1.
    m_iRowId(static_cast<int>(poDataBlock->GetFeatureCount() + 1)),
    m_hStmt(nullptr)
{
    // Set FID from DB.
    SetFIDFromDB();  // -> m_nFID
}

/*!
  \brief VFKFeatureSQLite constructor

  \param poDataBlock pointer to related IVFKDataBlock
  \param iRowId feature DB rowid (starts at 1)
  \param nFID feature id
*/
VFKFeatureSQLite::VFKFeatureSQLite( IVFKDataBlock *poDataBlock, int iRowId,
                                    GIntBig nFID) :
    IVFKFeature(poDataBlock),
    m_iRowId(iRowId),
    m_hStmt(nullptr)
{
    m_nFID = nFID;
}

/*!
  \brief Read FID from DB
*/
OGRErr VFKFeatureSQLite::SetFIDFromDB()
{
    CPLString   osSQL;

    osSQL.Printf("SELECT %s FROM %s WHERE rowid = %d",
                 FID_COLUMN, m_poDataBlock->GetName(), m_iRowId);
    if (ExecuteSQL(osSQL.c_str()) != OGRERR_NONE)
        return OGRERR_FAILURE;

    m_nFID = sqlite3_column_int(m_hStmt, 0);

    FinalizeSQL();

    return OGRERR_NONE;
}

/*!
  \brief Set DB row id

  \param iRowId row id to be set
*/
void VFKFeatureSQLite::SetRowId(int iRowId)
{
    m_iRowId = iRowId;
}

/*!
  \brief Finalize SQL statement
*/
void VFKFeatureSQLite::FinalizeSQL()
{
    sqlite3_finalize(m_hStmt);
    m_hStmt = nullptr;
}

/*!
  \brief Execute SQL (select) statement

  \param pszSQLCommand SQL command string

  \return OGRERR_NONE on success or OGRERR_FAILURE on error
*/
OGRErr VFKFeatureSQLite::ExecuteSQL(const char *pszSQLCommand)
{
    VFKReaderSQLite *poReader = (VFKReaderSQLite *) m_poDataBlock->GetReader();
    sqlite3  *poDB = poReader->m_poDB;

    int rc = sqlite3_prepare_v2(poDB, pszSQLCommand, -1,
                         &m_hStmt, nullptr);
    if (rc != SQLITE_OK) {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "In ExecuteSQL(): sqlite3_prepare_v2(%s):\n  %s",
                 pszSQLCommand, sqlite3_errmsg(poDB));

        if(m_hStmt != nullptr) {
            FinalizeSQL();
        }
        return OGRERR_FAILURE;
    }
    rc = sqlite3_step(m_hStmt);
    if (rc != SQLITE_ROW) {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "In ExecuteSQL(): sqlite3_step(%s):\n  %s",
                 pszSQLCommand, sqlite3_errmsg(poDB));

        if (m_hStmt) {
            FinalizeSQL();
        }

        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/*!
  \brief VFKFeatureSQLite constructor (derived from VFKFeature)

  Read VFK feature from VFK file and insert it into DB
*/
VFKFeatureSQLite::VFKFeatureSQLite( const VFKFeature *poVFKFeature ) :
    IVFKFeature(poVFKFeature->m_poDataBlock),
    // Starts at 1.
    m_iRowId(static_cast<int>(
        poVFKFeature->m_poDataBlock->GetFeatureCount() + 1)),
    m_hStmt(nullptr)
{
    m_nFID = poVFKFeature->m_nFID;
}

/*!
  \brief Load geometry (point layers)

  \todo Implement (really needed?)

  \return true on success or false on failure
*/
bool VFKFeatureSQLite::LoadGeometryPoint()
{
    return false;
}

/*!
  \brief Load geometry (linestring SBP layer)

  \todo Implement (really needed?)

  \return true on success or false on failure
*/
bool VFKFeatureSQLite::LoadGeometryLineStringSBP()
{
    return false;
}

/*!
  \brief Load geometry (linestring HP/DPM layer)

  \todo Implement (really needed?)

  \return true on success or false on failure
*/
bool VFKFeatureSQLite::LoadGeometryLineStringHP()
{
    return false;
}

/*!
  \brief Load geometry (polygon BUD/PAR layers)

  \todo Implement (really needed?)

  \return true on success or false on failure
*/
bool VFKFeatureSQLite::LoadGeometryPolygon()
{
    return false;
}

/*!
  \brief Load feature properties from DB

  \param poFeature pointer to OGR feature

  \return OGRERR_NONE on success or OGRERR_FAILURE on failure
*/
OGRErr VFKFeatureSQLite::LoadProperties(OGRFeature *poFeature)
{
    sqlite3_stmt *hStmt = ((VFKDataBlockSQLite *) m_poDataBlock)->m_hStmt;
    if ( hStmt == nullptr ) {
        /* random access */
        CPLString   osSQL;

        osSQL.Printf("SELECT * FROM %s WHERE rowid = %d",
                    m_poDataBlock->GetName(), m_iRowId);
        if (ExecuteSQL(osSQL.c_str()) != OGRERR_NONE)
            return OGRERR_FAILURE;

        hStmt = m_hStmt;
    }
    else {
        /* sequential access */
        VFKReaderSQLite *poReader = (VFKReaderSQLite *) m_poDataBlock->GetReader();
        if ( poReader->ExecuteSQL(hStmt) != OGRERR_NONE )
        {
            ((VFKDataBlockSQLite *) m_poDataBlock)->m_hStmt = nullptr;
            return OGRERR_FAILURE;
        }
    }

    int nPropertyCount = m_poDataBlock->GetPropertyCount();
    for( int iField = 0; iField < nPropertyCount; iField++ ) {
        if (sqlite3_column_type(hStmt, iField) == SQLITE_NULL) /* skip null values */
            continue;
        OGRFieldType fType = poFeature->GetDefnRef()->GetFieldDefn(iField)->GetType();
        switch (fType) {
        case OFTInteger:
            poFeature->SetField(iField,
                                sqlite3_column_int(hStmt, iField));
            break;
        case OFTInteger64:
            poFeature->SetField(iField,
                                sqlite3_column_int64(hStmt, iField));
            break;
        case OFTReal:
            poFeature->SetField(iField,
                                sqlite3_column_double(hStmt, iField));
            break;
        default:
            poFeature->SetField(iField,
                                (const char *) sqlite3_column_text(hStmt, iField));
            break;
        }
    }

    if ( m_poDataBlock->GetReader()->HasFileField() ) {
        /* open option FILE_FIELD=YES specified, append extra
         * attribute */
        poFeature->SetField( nPropertyCount,
                             CPLGetFilename(m_poDataBlock->GetReader()->GetFilename()) );
    }

    FinalizeSQL();

    return OGRERR_NONE;
}
