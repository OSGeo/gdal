/******************************************************************************
 * $Id$
 *
 * Project:  VFK Reader - Feature definition (SQLite)
 * Purpose:  Implements VFKFeatureSQLite class.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2012, Martin Landa <landa.martin gmail.com>
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

#ifdef HAVE_SQLITE


/*!
  \brief VFKFeatureSQLite constructor
*/
VFKFeatureSQLite::VFKFeatureSQLite(const VFKFeature *poVFKFeature) : IVFKFeature(poVFKFeature->m_poDataBlock)
{
    m_nFID   = poVFKFeature->m_nFID;
    
    m_hStmt  = NULL;
    
    m_nIndex = m_poDataBlock->GetFeatureCount();
}

/*!
  \brief Load geometry (point layers)

  \todo Implement (really needed?)
  
  \return TRUE on success
  \return FALSE on failure
*/
bool VFKFeatureSQLite::LoadGeometryPoint()
{
    return FALSE;
}

/*!
  \brief Load geometry (linestring SBP layer)

  \todo Implement (really needed?)

  \return TRUE on success
  \return FALSE on failure
*/
bool VFKFeatureSQLite::LoadGeometryLineStringSBP()
{
    return FALSE;
}

/*!
  \brief Load geometry (linestring HP/DPM layer)

  \todo Implement (really needed?)

  \return TRUE on success
  \return FALSE on failure
*/
bool VFKFeatureSQLite::LoadGeometryLineStringHP()
{
    return FALSE;
}

/*!
  \brief Load geometry (polygon BUD/PAR layers)

  \todo Implement (really needed?)

  \return TRUE on success
  \return FALSE on failure
*/
bool VFKFeatureSQLite::LoadGeometryPolygon()
{
    return FALSE;
}

/*!
  \brief Load feature properties from DB

  \param poFeature pointer to OGR feature

  \return OGRERR_NONE on success
  \return OGRERR_FAILURE on failure
*/
OGRErr VFKFeatureSQLite::LoadProperties(OGRFeature *poFeature)
{
    int rc;
    CPLString   osSQL;
    const char *pszSQL;

    sqlite3  *poDB;

    VFKReaderSQLite *poReader = (VFKReaderSQLite *) m_poDataBlock->GetReader();
    poDB = poReader->GetDB();

    osSQL.Printf("SELECT * FROM '%s' WHERE _rowid_ = %d",
		 m_poDataBlock->GetName(), m_nIndex);
    pszSQL = osSQL.c_str();
    
    rc = sqlite3_prepare(poDB, pszSQL, strlen(pszSQL),
			 &m_hStmt, NULL);
    
    if (rc != SQLITE_OK) {
        CPLError(CE_Failure, CPLE_AppDefined, 
		 "In LoadProperties(): sqlite3_prepare(%s):\n  %s",
		 pszSQL, sqlite3_errmsg(poDB));
	
        if(m_hStmt != NULL) {
            sqlite3_finalize(m_hStmt);
	    m_hStmt = NULL;
        }
	return OGRERR_FAILURE;
    }
    
    rc = sqlite3_step(m_hStmt);
    if (rc != SQLITE_ROW) {
	CPLError(CE_Failure, CPLE_AppDefined, 
		 "In ExecuteSQL(): sqlite3_step(%s):\n  %s", 
		 pszSQL, sqlite3_errmsg(poDB));
	
	if (m_hStmt) {
	    sqlite3_finalize(m_hStmt);
	    m_hStmt = NULL;
	}
	
	return OGRERR_FAILURE;
    }
    
    for (int iField = 0; iField < m_poDataBlock->GetPropertyCount(); iField++) {
	OGRFieldType fType = poFeature->GetDefnRef()->GetFieldDefn(iField)->GetType();
	if (fType == OFTInteger)
	    poFeature->SetField(iField,
				sqlite3_column_int(m_hStmt, iField));
	else if (fType == OFTReal)
	    poFeature->SetField(iField,
				sqlite3_column_double(m_hStmt, iField));
	else
	    poFeature->SetField(iField,
				(const char *) sqlite3_column_text(m_hStmt, iField));
    }

    sqlite3_finalize(m_hStmt);
    m_hStmt = NULL;

    return OGRERR_NONE;
}

#endif // HAVE_SQLITE
