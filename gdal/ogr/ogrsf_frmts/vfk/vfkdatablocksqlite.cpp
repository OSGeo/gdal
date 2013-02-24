/******************************************************************************
 * $Id$
 *
 * Project:  VFK Reader - Data block definition (SQLite)
 * Purpose:  Implements VFKDataBlockSQLite
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
  \brief Load geometry (point layers)

  \return number of invalid features
*/
int VFKDataBlockSQLite::LoadGeometryPoint()
{
    int   nInvalid;
    double x, y;

    CPLString     osSQL;
    sqlite3_stmt *hStmt;
    
    VFKFeatureSQLite *poFeature;
    VFKReaderSQLite  *poReader;
        
    nInvalid  = 0;
    poReader  = (VFKReaderSQLite*) m_poReader;

    osSQL.Printf("SELECT SOURADNICE_Y,SOURADNICE_X FROM '%s'", m_pszName);
    hStmt = poReader->PrepareStatement(osSQL.c_str());
    
    ResetReading();
    while(poReader->ExecuteSQL(hStmt) == OGRERR_NONE) {
        poFeature = (VFKFeatureSQLite *) GetNextFeature();
        CPLAssert(NULL != poFeature);
        
        x = -1.0 * sqlite3_column_double(hStmt, 0);
        y = -1.0 * sqlite3_column_double(hStmt, 1);
        OGRPoint pt(x, y);
        if (!poFeature->SetGeometry(&pt))
            nInvalid++;
    }
    ResetReading();

    return nInvalid;
}

/*!
  \brief Load geometry (linestring SBP layer)

  \return number of invalid features
*/
int VFKDataBlockSQLite::LoadGeometryLineStringSBP()
{
    int      nInvalid;
    int      rowId;
    GUIntBig id, ipcb;

    CPLString     osSQL;
    sqlite3_stmt *hStmt;
    
    VFKReaderSQLite    *poReader;
    VFKDataBlockSQLite *poDataBlockPoints;
    VFKFeatureSQLite   *poFeature, *poPoint, *poLine;
    
    OGRLineString oOGRLine;
    
    nInvalid  = 0;
    poReader  = (VFKReaderSQLite*) m_poReader;
    poLine    = NULL;
    
    poDataBlockPoints = (VFKDataBlockSQLite *) m_poReader->GetDataBlock("SOBR");
    if (NULL == poDataBlockPoints) {
        CPLError(CE_Failure, CPLE_NotSupported, 
                 "Data block %s not found.\n", m_pszName);
        return nInvalid;
    }
    
    poDataBlockPoints->LoadGeometry();
    
    for (int i = 0; i < 2; i++) {
        if (i == 0)
            osSQL.Printf("SELECT BP_ID,PORADOVE_CISLO_BODU,_rowid_,ID FROM '%s' WHERE "
                         "HP_ID IS NOT NULL OR OB_ID IS NOT NULL OR DPM_ID IS NOT NULL "
                         "ORDER BY HP_ID,OB_ID,DPM_ID,PORADOVE_CISLO_BODU", m_pszName);
        else
            osSQL.Printf("SELECT BP_ID,PORADOVE_CISLO_BODU,_rowid_,ID FROM '%s' WHERE "
                         "OB_ID IS NULL AND HP_ID IS NULL AND DPM_ID IS NULL "
                         "ORDER BY ID,PORADOVE_CISLO_BODU", m_pszName);
        
        hStmt = poReader->PrepareStatement(osSQL.c_str());
        
        while(poReader->ExecuteSQL(hStmt) == OGRERR_NONE) {
            id    = sqlite3_column_double(hStmt, 0);
            ipcb  = sqlite3_column_double(hStmt, 1);
            rowId = sqlite3_column_int(hStmt, 2) - 1;
            poFeature = (VFKFeatureSQLite *) GetFeatureByIndex(rowId);
            if (!poFeature)
                continue;
            poFeature->SetGeometry(NULL);
            
            if (ipcb == 1) {
                if (!oOGRLine.IsEmpty()) {
                    oOGRLine.setCoordinateDimension(2); /* force 2D */
                    if (poLine && !poLine->SetGeometry(&oOGRLine))
                        nInvalid++;
                    oOGRLine.empty(); /* restore line */
                }
                poLine = poFeature;
            }
            else {
                poFeature->SetGeometryType(wkbUnknown);
            }
            
            poPoint = (VFKFeatureSQLite *) poDataBlockPoints->GetFeature("ID", id);
            if (!poPoint)
                continue;
            OGRPoint *pt = (OGRPoint *) poPoint->GetGeometry();
            if (!pt)
                continue;
            oOGRLine.addPoint(pt);
        }
        /* add last line */
        oOGRLine.setCoordinateDimension(2); /* force 2D */
        if (poLine && !poLine->SetGeometry(&oOGRLine))
            nInvalid++;
    }
    
    return nInvalid;
}

/*!
  \brief Load geometry (linestring HP/DPM layer)

  \return number of invalid features
*/
int VFKDataBlockSQLite::LoadGeometryLineStringHP()
{
    int          nInvalid;
    int          rowId;
    
    CPLString    osColumn, osSQL;
    const char  *vrColumn[2];
    GUIntBig     vrValue[2];
    
    sqlite3_stmt *hStmt;
    
    VFKReaderSQLite    *poReader;
    VFKDataBlockSQLite *poDataBlockLines;
    VFKFeatureSQLite   *poFeature, *poLine;
    
    nInvalid  = 0;
    poReader  = (VFKReaderSQLite*) m_poReader;
    
    poDataBlockLines = (VFKDataBlockSQLite *) m_poReader->GetDataBlock("SBP");
    if (NULL == poDataBlockLines) {
        CPLError(CE_Failure, CPLE_NotSupported, 
                 "Data block %s not found.\n", m_pszName);
        return nInvalid;
    }
    
    poDataBlockLines->LoadGeometry();
    osColumn.Printf("%s_ID", m_pszName);
    vrColumn[0] = osColumn.c_str();
    vrColumn[1] = "PORADOVE_CISLO_BODU";
    vrValue[1]  = 1; /* reduce to first segment */
    
    osSQL.Printf("SELECT ID,_rowid_ FROM '%s'", m_pszName);
    hStmt = poReader->PrepareStatement(osSQL.c_str());
    
    while(poReader->ExecuteSQL(hStmt) == OGRERR_NONE) {
        vrValue[0] = sqlite3_column_double(hStmt, 0);
        rowId      = sqlite3_column_int(hStmt, 1) - 1;
        poFeature  = (VFKFeatureSQLite *) GetFeatureByIndex(rowId);
            
        poLine = poDataBlockLines->GetFeature(vrColumn, vrValue, 2);
        if (!poLine || !poLine->GetGeometry())
            continue;
        if (!poFeature->SetGeometry(poLine->GetGeometry()))
            nInvalid++;
    }
    
    return nInvalid;
}

/*!
  \brief Load geometry (polygon BUD/PAR layers)

  \return number of invalid features
*/
int VFKDataBlockSQLite::LoadGeometryPolygon()
{
    int  nInvalid;
    int  rowId, nCount, nCountMax;
    bool bIsPar, bNewRing, bFound;
        
    CPLString    osSQL;
    const char  *vrColumn[2];
    GUIntBig     vrValue[2];
    GUIntBig     id, idOb;
    
    sqlite3_stmt *hStmt;
    
    VFKReaderSQLite    *poReader;
    VFKDataBlockSQLite *poDataBlockLines1, *poDataBlockLines2;
    VFKFeatureSQLite   *poFeature;

    VFKFeatureSQLiteList  poLineList;
    /* first is to be considered as exterior */
    PointListArray        poRingList;
    
    OGRLinearRing ogrRing;
    OGRPolygon    ogrPolygon;
    
    nInvalid  = 0;
    poReader  = (VFKReaderSQLite*) m_poReader;
    
    if (EQUAL (m_pszName, "PAR")) {
        poDataBlockLines1 = (VFKDataBlockSQLite *) m_poReader->GetDataBlock("HP");
        poDataBlockLines2 = poDataBlockLines1;
        bIsPar = TRUE;
    }
    else {
        poDataBlockLines1 = (VFKDataBlockSQLite *) m_poReader->GetDataBlock("OB");
        poDataBlockLines2 = (VFKDataBlockSQLite *) m_poReader->GetDataBlock("SBP");
        bIsPar = FALSE;
    }
    if (NULL == poDataBlockLines1 || NULL == poDataBlockLines2) {
        CPLError(CE_Failure, CPLE_NotSupported, 
                 "Data block %s not found.\n", m_pszName);
        return nInvalid;
    }
    
    poDataBlockLines1->LoadGeometry();
    poDataBlockLines2->LoadGeometry();
    
    if (bIsPar) {
        vrColumn[0] = "PAR_ID_1";
        vrColumn[1] = "PAR_ID_2";
    }
    else {
        vrColumn[0] = "OB_ID";
        vrColumn[1] = "PORADOVE_CISLO_BODU";
        vrValue[1]  = 1;
    }

    osSQL.Printf("SELECT ID,_rowid_ FROM '%s'", m_pszName);
    hStmt = poReader->PrepareStatement(osSQL.c_str());
    
    while(poReader->ExecuteSQL(hStmt) == OGRERR_NONE) {
        id        = sqlite3_column_double(hStmt, 0);
        rowId     = sqlite3_column_int(hStmt, 1) - 1;
        poFeature = (VFKFeatureSQLite *) GetFeatureByIndex(rowId);
        if (bIsPar) {
            vrValue[0] = vrValue[1] = id;
            poLineList = poDataBlockLines1->GetFeatures(vrColumn, vrValue, 2);
        }
        else {
            VFKFeatureSQLite *poLineSbp;
            std::vector<VFKFeatureSQLite *> poLineListOb;
            sqlite3_stmt *hStmtOb;
            
            osSQL.Printf("SELECT ID FROM '%s' WHERE BUD_ID = " CPL_FRMT_GUIB,
                         poDataBlockLines1->GetName(), id);
            hStmtOb = poReader->PrepareStatement(osSQL.c_str());
            
            while(poReader->ExecuteSQL(hStmtOb) == OGRERR_NONE) {
                idOb = sqlite3_column_double(hStmtOb, 0); 
                vrValue[0] = idOb;
                poLineSbp = poDataBlockLines2->GetFeature(vrColumn, vrValue, 2);
                if (poLineSbp)
                    poLineList.push_back(poLineSbp);
            }
        }
        if (poLineList.size() < 1)
            continue;
        
        /* clear */
        ogrPolygon.empty();
        poRingList.clear();
        
        /* collect rings (points) */
        bFound = FALSE;
        nCount = 0;
        nCountMax = poLineList.size() * 2;
        while (poLineList.size() > 0 && nCount < nCountMax) {
            bNewRing = !bFound ? TRUE : FALSE;
            bFound = FALSE;
            for (VFKFeatureSQLiteList::iterator iHp = poLineList.begin(), eHp = poLineList.end();
                 iHp != eHp; ++iHp) {
                const OGRLineString *pLine = (OGRLineString *) (*iHp)->GetGeometry();
                if (pLine && AppendLineToRing(&poRingList, pLine, bNewRing)) {
                    bFound = TRUE;
                    poLineList.erase(iHp);
                    break;
                }
            }
            nCount++;
        }
        
        if (poLineList.size() > 0) {
            CPLError(CE_Warning, CPLE_AppDefined, 
                     "Unable to collect rings for feature " CPL_FRMT_GUIB " (%s).\n",
                     id, m_pszName);
            continue;
        }
        
        /* create rings */
        for (PointListArray::const_iterator iRing = poRingList.begin(), eRing = poRingList.end();
             iRing != eRing; ++iRing) {
            PointList *poList = *iRing;
            ogrRing.empty();
            for (PointList::iterator iPoint = poList->begin(), ePoint = poList->end();
                 iPoint != ePoint; ++iPoint) {
                ogrRing.addPoint(&(*iPoint));
            }
            ogrPolygon.addRing(&ogrRing);
        }
        
        /* set polygon */
        ogrPolygon.setCoordinateDimension(2); /* force 2D */
        if (!poFeature->SetGeometry(&ogrPolygon))
            nInvalid++;
    }
    
    /* free ring list */
    for (PointListArray::iterator iRing = poRingList.begin(), eRing = poRingList.end();
         iRing != eRing; ++iRing) {
        delete (*iRing);
        *iRing = NULL;
    }
    
    return nInvalid;
}

/*!
  \brief Get first found feature based on it's property
  
  \param column property name
  \param value property value
  
  \return pointer to feature definition or NULL on failure (not found)
*/
VFKFeatureSQLite *VFKDataBlockSQLite::GetFeature(const char *column, GUIntBig value)
{
    int idx;
    CPLString osSQL;
    VFKReaderSQLite  *poReader;

    sqlite3_stmt *hStmt;
    
    poReader = (VFKReaderSQLite*) m_poReader;
    
    osSQL.Printf("SELECT _rowid_ from '%s' WHERE %s = " CPL_FRMT_GUIB, m_pszName, column, value);
    hStmt = poReader->PrepareStatement(osSQL.c_str());
    if (poReader->ExecuteSQL(hStmt) != OGRERR_NONE)
        return NULL;
    
    idx = sqlite3_column_int(hStmt, 0) - 1;
    if (idx < 0 || idx >= m_nFeatureCount) // ? assert
        return NULL;
    
    sqlite3_finalize(hStmt);

    return (VFKFeatureSQLite *) GetFeatureByIndex(idx);
}

/*!
  \brief Get first found feature based on it's properties (AND)
  
  \param column array of property names
  \param value array of property values
  \param num number of array items
  
  \return pointer to feature definition or NULL on failure (not found)
*/
VFKFeatureSQLite *VFKDataBlockSQLite::GetFeature(const char **column, GUIntBig *value, int num)
{
    int idx;
    CPLString osSQL, osItem;
    VFKReaderSQLite  *poReader;

    sqlite3_stmt *hStmt;
    
    poReader = (VFKReaderSQLite*) m_poReader;
    
    osSQL.Printf("SELECT _rowid_ from '%s' WHERE ", m_pszName);
    for (int i = 0; i < num; i++) {
        if (i > 0)
            osItem.Printf(" AND %s = " CPL_FRMT_GUIB, column[i], value[i]);
        else
            osItem.Printf("%s = " CPL_FRMT_GUIB, column[i], value[i]);
        osSQL += osItem;
    }
    
    hStmt = poReader->PrepareStatement(osSQL.c_str());
    if (poReader->ExecuteSQL(hStmt) != OGRERR_NONE)
        return NULL;
    
    idx = sqlite3_column_int(hStmt, 0) - 1;
    
    if (idx < 0 || idx >= m_nFeatureCount) // ? assert
        return NULL;

    sqlite3_finalize(hStmt);
    
    return (VFKFeatureSQLite *) GetFeatureByIndex(idx);
}

/*!
  \brief Get features based on properties
  
  \param column array of property names
  \param value array of property values
  \param num number of array items
  
  \return list of features
*/
VFKFeatureSQLiteList VFKDataBlockSQLite::GetFeatures(const char **column, GUIntBig *value, int num)
{
    int idx;
    CPLString osSQL, osItem;

    VFKReaderSQLite     *poReader;
    VFKFeatureSQLiteList fList;
    
    sqlite3_stmt *hStmt;
    
    poReader = (VFKReaderSQLite*) m_poReader;
    
    osSQL.Printf("SELECT _rowid_ from '%s' WHERE ", m_pszName);
    for (int i = 0; i < num; i++) {
        if (i > 0)
            osItem.Printf(" OR %s = " CPL_FRMT_GUIB, column[i], value[i]);
        else
            osItem.Printf("%s = " CPL_FRMT_GUIB, column[i], value[i]);
        osSQL += osItem;
    }
    
    hStmt = poReader->PrepareStatement(osSQL.c_str());
    while (poReader->ExecuteSQL(hStmt) == OGRERR_NONE) {
        idx = sqlite3_column_int(hStmt, 0) - 1;
        if (idx < 0 || idx >= m_nFeatureCount)
            continue; // assert?
        fList.push_back((VFKFeatureSQLite *)GetFeatureByIndex(idx));
    }
    
    return fList;
}

#endif // HAVE_SQLITE
