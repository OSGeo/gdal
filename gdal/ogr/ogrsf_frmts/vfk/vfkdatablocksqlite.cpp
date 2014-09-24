/******************************************************************************
 * $Id$
 *
 * Project:  VFK Reader - Data block definition (SQLite)
 * Purpose:  Implements VFKDataBlockSQLite
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2012-2014, Martin Landa <landa.martin gmail.com>
 * Copyright (c) 2012-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include <algorithm>

#include "vfkreader.h"
#include "vfkreaderp.h"

#include "cpl_conv.h"
#include "cpl_error.h"

/*!
  \brief Load geometry (point layers)

  \return number of invalid features
*/
int VFKDataBlockSQLite::LoadGeometryPoint()
{
    int   nInvalid, rowId, nGeometries;
    bool  bSkipInvalid;
    long iFID;
    double x, y;

    CPLString     osSQL;
    sqlite3_stmt *hStmt;
    
    VFKFeatureSQLite *poFeature;
    VFKReaderSQLite  *poReader;
    
    nInvalid  = nGeometries = 0;
    poReader  = (VFKReaderSQLite*) m_poReader;

    if (LoadGeometryFromDB()) /* try to load geometry from DB */
	return 0;
    
    bSkipInvalid = EQUAL(m_pszName, "OB") || EQUAL(m_pszName, "OP") || EQUAL(m_pszName, "OBBP");
    osSQL.Printf("SELECT SOURADNICE_Y,SOURADNICE_X,%s,rowid FROM %s",
                 FID_COLUMN, m_pszName);
    hStmt = poReader->PrepareStatement(osSQL.c_str());

    if (poReader->IsSpatial())
	poReader->ExecuteSQL("BEGIN");

    while(poReader->ExecuteSQL(hStmt) == OGRERR_NONE) {
        /* read values */
        x = -1.0 * sqlite3_column_double(hStmt, 0); /* S-JTSK coordinate system expected */
        y = -1.0 * sqlite3_column_double(hStmt, 1);
	iFID = sqlite3_column_double(hStmt, 2);
	rowId = sqlite3_column_int(hStmt, 3);

        poFeature = (VFKFeatureSQLite *) GetFeatureByIndex(rowId - 1);
        CPLAssert(NULL != poFeature && poFeature->GetFID() == iFID);
        
        /* create geometry */
	OGRPoint pt(x, y);
        if (!poFeature->SetGeometry(&pt)) {
            nInvalid++;
            continue;
        }
        
	/* store also geometry in DB */
	if (poReader->IsSpatial() &&
	    SaveGeometryToDB(&pt, rowId) != OGRERR_FAILURE)
	    nGeometries++;
    }
    
    /* update number of geometries in VFK_DB_TABLE table */
    UpdateVfkBlocks(nGeometries);

    if (poReader->IsSpatial())
	poReader->ExecuteSQL("COMMIT");
    
    return bSkipInvalid ? 0 : nInvalid;
}

/*!
  \brief Load geometry (linestring SBP layer)

  \return number of invalid features
*/
int VFKDataBlockSQLite::LoadGeometryLineStringSBP()
{
    int      nInvalid, nGeometries, rowId;
    long int iFID;
    GUIntBig id, ipcb;
    bool     bValid;
    
    std::vector<int> rowIdFeat;
    CPLString     osSQL;
    sqlite3_stmt *hStmt;
    
    VFKReaderSQLite    *poReader;
    VFKDataBlockSQLite *poDataBlockPoints;
    VFKFeatureSQLite   *poFeature, *poPoint, *poLine;
    
    OGRLineString oOGRLine;
    
    nInvalid  = nGeometries = 0;
    poReader  = (VFKReaderSQLite*) m_poReader;
    poLine    = NULL;
    
    poDataBlockPoints = (VFKDataBlockSQLite *) m_poReader->GetDataBlock("SOBR");
    if (NULL == poDataBlockPoints) {
        CPLError(CE_Failure, CPLE_FileIO, 
                 "Data block %s not found.\n", m_pszName);
        return nInvalid;
    }
    
    poDataBlockPoints->LoadGeometry();
    
    if (LoadGeometryFromDB()) /* try to load geometry from DB */
	return 0;

    osSQL.Printf("UPDATE %s SET %s = -1", m_pszName, FID_COLUMN);
    poReader->ExecuteSQL(osSQL.c_str());
    bValid = TRUE;
    iFID = 1;
    for (int i = 0; i < 2; i++) {
	/* first collect linestrings related to HP, OB or DPM
	   then collect rest of linestrings */
        if (i == 0)
            osSQL.Printf("SELECT BP_ID,PORADOVE_CISLO_BODU,_rowid_ FROM '%s' WHERE "
                         "HP_ID IS NOT NULL OR OB_ID IS NOT NULL OR DPM_ID IS NOT NULL "
                         "ORDER BY HP_ID,OB_ID,DPM_ID,PORADOVE_CISLO_BODU", m_pszName);
        else
            osSQL.Printf("SELECT BP_ID,PORADOVE_CISLO_BODU,_rowid_ FROM '%s' WHERE "
                         "OB_ID IS NULL AND HP_ID IS NULL AND DPM_ID IS NULL "
                         "ORDER BY ID,PORADOVE_CISLO_BODU", m_pszName);
	
        hStmt = poReader->PrepareStatement(osSQL.c_str());
    
	if (poReader->IsSpatial())
	    poReader->ExecuteSQL("BEGIN");
	
        while(poReader->ExecuteSQL(hStmt) == OGRERR_NONE) {
            // read values
            id    = sqlite3_column_double(hStmt, 0);
            ipcb  = sqlite3_column_double(hStmt, 1);
            rowId = sqlite3_column_int(hStmt, 2);

            if (ipcb == 1) {
                /* add feature to the array */
                poFeature = new VFKFeatureSQLite(this, rowId, iFID);
                CPLAssert(NULL != poFeature && poFeature->GetFID() == iFID);
                AddFeature(poFeature);
                
                if (poLine) {
                    oOGRLine.setCoordinateDimension(2); /* force 2D */
		    if (bValid) {
			if (!poLine->SetGeometry(&oOGRLine)) {
                            bValid = FALSE;
			    nInvalid++;
                        }
		    }
		    else {
			poLine->SetGeometry(NULL);
			nInvalid++;
		    }
                    
		    /* update fid column */
		    UpdateFID(poLine->GetFID(), rowIdFeat);        

		    /* store also geometry in DB */
		    CPLAssert(0 != rowIdFeat.size());
		    if (bValid && poReader->IsSpatial() &&
			SaveGeometryToDB(bValid ? &oOGRLine : NULL,
					 rowIdFeat[0]) != OGRERR_FAILURE)
			nGeometries++;
		    
		    rowIdFeat.clear();
                    oOGRLine.empty(); /* restore line */
                }
		bValid = TRUE;
                poLine = poFeature;
                iFID++;
            }
            
            poPoint = (VFKFeatureSQLite *) poDataBlockPoints->GetFeature("ID", id);
	    if (poPoint) {
		OGRPoint *pt = (OGRPoint *) poPoint->GetGeometry();
		if (pt) {
		    oOGRLine.addPoint(pt);
		}
		else {
		    CPLDebug("OGR-VFK", 
			     "Geometry (point ID = " CPL_FRMT_GUIB ") not valid", id);
		    bValid = FALSE;
		}
	    }
	    else {
                CPLDebug("OGR-VFK", 
                         "Point ID = " CPL_FRMT_GUIB " not found (rowid = %d)",
                         id, rowId);
		bValid = FALSE;
            }
	    
	    /* add vertex to the linestring */
	    rowIdFeat.push_back(rowId);
        }

        /* add last line */
        if (poLine) {
	    oOGRLine.setCoordinateDimension(2); /* force 2D */
	    if (bValid) {
		if (!poLine->SetGeometry(&oOGRLine))
		    nInvalid++;
	    }
	    else {
		poLine->SetGeometry(NULL);
		nInvalid++;
	    }
            
	    /* update fid column */
	    UpdateFID(poLine->GetFID(), rowIdFeat);        
	    
	    /* store also geometry in DB */
	    CPLAssert(0 != rowIdFeat.size());
	    if (poReader->IsSpatial() &&
		SaveGeometryToDB(bValid ? &oOGRLine : NULL,
				 rowIdFeat[0]) != OGRERR_FAILURE && bValid)
		nGeometries++;
        }
	poLine = NULL;
	rowIdFeat.clear();
        oOGRLine.empty(); /* restore line */

	if (poReader->IsSpatial())
	    poReader->ExecuteSQL("COMMIT");
    }

    /* update number of geometries in VFK_DB_TABLE table */
    UpdateVfkBlocks(nGeometries);
    
    return nInvalid;
}

/*!
  \brief Load geometry (linestring HP/DPM layer)

  \return number of invalid features
*/
int VFKDataBlockSQLite::LoadGeometryLineStringHP()
{
    int          nInvalid, nGeometries;
    int          rowId;
    long         iFID;
    
    CPLString    osColumn, osSQL;
    const char  *vrColumn[2];
    GUIntBig     vrValue[2];
    
    sqlite3_stmt *hStmt;
    
    OGRGeometry        *poOgrGeometry;
    VFKReaderSQLite    *poReader;
    VFKDataBlockSQLite *poDataBlockLines;
    VFKFeatureSQLite   *poFeature, *poLine;
    
    nInvalid = nGeometries = 0;
    poReader  = (VFKReaderSQLite*) m_poReader;
    
    poDataBlockLines = (VFKDataBlockSQLite *) m_poReader->GetDataBlock("SBP");
    if (NULL == poDataBlockLines) {
        CPLError(CE_Failure, CPLE_FileIO, 
                 "Data block %s not found", m_pszName);
        return nInvalid;
    }
    
    poDataBlockLines->LoadGeometry();

    if (LoadGeometryFromDB()) /* try to load geometry from DB */
	return 0;
    
    osColumn.Printf("%s_ID", m_pszName);
    vrColumn[0] = osColumn.c_str();
    vrColumn[1] = "PORADOVE_CISLO_BODU";
    vrValue[1]  = 1; /* reduce to first segment */
    
    osSQL.Printf("SELECT ID,%s,rowid FROM %s", FID_COLUMN, m_pszName);
    /* TODO: handle points in DPM */
    if (EQUAL(m_pszName, "DPM"))
        osSQL += " WHERE SOURADNICE_X IS NULL";
    hStmt = poReader->PrepareStatement(osSQL.c_str());
    
    if (poReader->IsSpatial())
	poReader->ExecuteSQL("BEGIN");

    while(poReader->ExecuteSQL(hStmt) == OGRERR_NONE) {
        /* read values */
        vrValue[0] = sqlite3_column_double(hStmt, 0);
        iFID       = sqlite3_column_double(hStmt, 1);
        rowId      = sqlite3_column_int(hStmt, 2);

        poFeature = (VFKFeatureSQLite *) GetFeatureByIndex(rowId - 1);
        CPLAssert(NULL != poFeature && poFeature->GetFID() == iFID);

        poLine = poDataBlockLines->GetFeature(vrColumn, vrValue, 2, TRUE);
	if (!poLine) {
	    poOgrGeometry = NULL;
	}
	else {
	    poOgrGeometry = poLine->GetGeometry();
	}
	if (!poOgrGeometry || !poFeature->SetGeometry(poOgrGeometry)) {
            CPLDebug("OGR-VFK", "VFKDataBlockSQLite::LoadGeometryLineStringHP(): name=%s fid=%ld "
                     "id=" CPL_FRMT_GUIB " -> %s geometry", m_pszName, iFID, vrValue[0],
                     poOgrGeometry ? "invalid" : "empty");
	    nInvalid++;
            continue;
        }
	
	/* store also geometry in DB */
	if (poReader->IsSpatial() &&
	    SaveGeometryToDB(poOgrGeometry, rowId) != OGRERR_FAILURE &&
	    poOgrGeometry)
	    nGeometries++;
    }
    
    /* update number of geometries in VFK_DB_TABLE table */
    UpdateVfkBlocks(nGeometries);
    
    if (poReader->IsSpatial())
	poReader->ExecuteSQL("COMMIT");

    return nInvalid;
}

/*!
  \brief Load geometry (polygon BUD/PAR layers)

  \return number of invalid features
*/
int VFKDataBlockSQLite::LoadGeometryPolygon()
{
    int  nInvalidNoLines, nInvalidNoRings, nGeometries, nBridges;
    int  rowId, nCount, nCountMax;
    size_t nLines;
    long iFID;
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
    
    std::vector<OGRLinearRing *> poLinearRingList;
    OGRPolygon     ogrPolygon;
    OGRLinearRing *poOgrRing;

    nInvalidNoLines = nInvalidNoRings = nGeometries = 0;
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
        CPLError(CE_Failure, CPLE_FileIO, 
                 "Data block %s not found", m_pszName);
        return -1;
    }
    
    poDataBlockLines1->LoadGeometry();
    poDataBlockLines2->LoadGeometry();
    
    if (LoadGeometryFromDB()) /* try to load geometry from DB */
	return 0;
    
    if (bIsPar) {
        vrColumn[0] = "PAR_ID_1";
        vrColumn[1] = "PAR_ID_2";
    }
    else {
        vrColumn[0] = "OB_ID";
        vrColumn[1] = "PORADOVE_CISLO_BODU";
        vrValue[1]  = 1;
    }

    osSQL.Printf("SELECT ID,%s,rowid FROM %s", FID_COLUMN, m_pszName);
    hStmt = poReader->PrepareStatement(osSQL.c_str());
    
    if (poReader->IsSpatial())
	poReader->ExecuteSQL("BEGIN");

    while(poReader->ExecuteSQL(hStmt) == OGRERR_NONE) {
        nBridges = 0;
        
        /* read values */
        id        = sqlite3_column_double(hStmt, 0);
        iFID      = sqlite3_column_double(hStmt, 1);
        rowId     = sqlite3_column_int(hStmt, 2);
        
        poFeature = (VFKFeatureSQLite *) GetFeatureByIndex(rowId - 1);
        CPLAssert(NULL != poFeature && poFeature->GetFID() == iFID);

        if (bIsPar) {
            vrValue[0] = vrValue[1] = id;
            poLineList = poDataBlockLines1->GetFeatures(vrColumn, vrValue, 2);
        }
        else {
            VFKFeatureSQLite *poLineSbp;
            std::vector<VFKFeatureSQLite *> poLineListOb;
            sqlite3_stmt *hStmtOb;
            
            osSQL.Printf("SELECT ID FROM %s WHERE BUD_ID = " CPL_FRMT_GUIB,
                         poDataBlockLines1->GetName(), id);
            if (poReader->IsSpatial()) {
                CPLString osColumn;
                
                osColumn.Printf(" AND %s IS NULL", GEOM_COLUMN);
                osSQL += osColumn;
            }
            hStmtOb = poReader->PrepareStatement(osSQL.c_str());
            
            while(poReader->ExecuteSQL(hStmtOb) == OGRERR_NONE) {
                idOb = sqlite3_column_double(hStmtOb, 0); 
                vrValue[0] = idOb;
                poLineSbp = poDataBlockLines2->GetFeature(vrColumn, vrValue, 2);
                if (poLineSbp)
                    poLineList.push_back(poLineSbp);
            }
        }
        nLines = poLineList.size();
        if (nLines < 1) {
            CPLDebug("OGR-VFK", 
                     "%s: unable to collect rings for polygon fid = %ld (no lines)",
                     m_pszName, iFID);
            nInvalidNoLines++;
            continue;
        }
        
        /* clear */
        ogrPolygon.empty();
        poRingList.clear();
        
        /* collect rings from lines */
        bFound = FALSE;
        nCount = 0;
        nCountMax = nLines * 2;
	while (poLineList.size() > 0 && nCount < nCountMax) {
            bNewRing = !bFound ? TRUE : FALSE;
            bFound = FALSE;
            int i = 1;
            for (VFKFeatureSQLiteList::iterator iHp = poLineList.begin(), eHp = poLineList.end();
                 iHp != eHp; ++iHp, ++i) {
                const OGRLineString *pLine = (OGRLineString *) (*iHp)->GetGeometry();
                if (pLine && AppendLineToRing(&poRingList, pLine, bNewRing)) {
                    bFound = TRUE;
                    poLineList.erase(iHp);
                    break;
                }
            }
            nCount++;
        }
        CPLDebug("OGR-VFK", "%s: fid = %ld nlines = %d -> nrings = %d", m_pszName,
                 iFID, (int)nLines, (int)poRingList.size());

        if (poLineList.size() > 0) {
            CPLDebug("OGR-VFK", 
                     "%s: unable to collect rings for polygon fid = %ld",
                     m_pszName, iFID);
            nInvalidNoRings++;
            continue;
        }

        /* build rings */
	poLinearRingList.clear();
	int i = 1;
        for (PointListArray::const_iterator iRing = poRingList.begin(), eRing = poRingList.end();
             iRing != eRing; ++iRing) {
	    OGRPoint *poPoint;
            PointList *poList = *iRing;
	    
            poLinearRingList.push_back(new OGRLinearRing());
	    poOgrRing = poLinearRingList.back();
	    CPLAssert(NULL != poOgrRing);
	    
            for (PointList::iterator iPoint = poList->begin(), ePoint = poList->end();
                 iPoint != ePoint; ++iPoint) {
		poPoint = &(*iPoint);
                poOgrRing->addPoint(poPoint);
            }
	    i++;
	}

	/* find exterior ring */
	if (poLinearRingList.size() > 1) {
	    double dArea, dMaxArea;
	    std::vector<OGRLinearRing *>::iterator exteriorRing;
	    
	    exteriorRing = poLinearRingList.begin();
	    dMaxArea = -1.;
	    for (std::vector<OGRLinearRing *>::iterator iRing = poLinearRingList.begin(),
		     eRing = poLinearRingList.end(); iRing != eRing; ++iRing) {
		poOgrRing = *iRing;
		if (!IsRingClosed(poOgrRing))
		    continue; /* skip unclosed rings */
		
		dArea = poOgrRing->get_Area();
		if (dArea > dMaxArea) {
		    dMaxArea = dArea;
		    exteriorRing = iRing;
		}
	    }
	    if (exteriorRing != poLinearRingList.begin()) {
		std::swap(*poLinearRingList.begin(), *exteriorRing);
	    }
	}
	
	/* build polygon from rings */
        for (std::vector<OGRLinearRing *>::iterator iRing = poLinearRingList.begin(),
		 eRing = poLinearRingList.end(); iRing != eRing; ++iRing) {
	    poOgrRing = *iRing;
	    
	    /* check if ring is closed */
	    if (IsRingClosed(poOgrRing)) {
		ogrPolygon.addRing(poOgrRing);
            }
	    else {
                if (poOgrRing->getNumPoints() == 2) {
                    CPLDebug("OGR-VFK", "%s: Polygon (fid = %ld) bridge removed",
                             m_pszName, iFID);
                    nBridges++;
                }
                else {
                    CPLDebug("OGR-VFK",
                             "%s: Polygon (fid = %ld) unclosed ring skipped",
                             m_pszName, iFID);
                }
            }
	    delete poOgrRing;
            *iRing = NULL;
        }

        /* set polygon */
        ogrPolygon.setCoordinateDimension(2); /* force 2D */
        if (ogrPolygon.getNumInteriorRings() + nBridges != (int) poLinearRingList.size() - 1 ||
            !poFeature->SetGeometry(&ogrPolygon)) {
            nInvalidNoRings++;
            continue;
        }
	
        /* store also geometry in DB */
        if (poReader->IsSpatial() && 
	    SaveGeometryToDB(&ogrPolygon, rowId) != OGRERR_FAILURE)
            nGeometries++;
    }
    
    /* free ring list */
    for (PointListArray::iterator iRing = poRingList.begin(), eRing = poRingList.end();
         iRing != eRing; ++iRing) {
        delete (*iRing);
        *iRing = NULL;
    }
    
    CPLDebug("OGR-VFK", "%s: nolines = %d norings = %d",
             m_pszName, nInvalidNoLines, nInvalidNoRings);
    
    /* update number of geometries in VFK_DB_TABLE table */
    UpdateVfkBlocks(nGeometries);

    if (poReader->IsSpatial())
	poReader->ExecuteSQL("COMMIT");

    return nInvalidNoLines + nInvalidNoRings;
}

/*!
  \brief Get feature by FID

  Modifies next feature id.
  
  \param nFID feature id

  \return pointer to feature definition or NULL on failure (not found)
*/
IVFKFeature *VFKDataBlockSQLite::GetFeature(long nFID)
{
    int rowId;
    CPLString osSQL;
    VFKReaderSQLite  *poReader;
    
    sqlite3_stmt *hStmt;
         
    if (m_nFeatureCount < 0) {
        m_poReader->ReadDataRecords(this);
    }
    
    if (nFID < 1 || nFID > m_nFeatureCount)
        return NULL;

    if (m_bGeometryPerBlock && !m_bGeometry) {
        LoadGeometry();
    }
    
    poReader = (VFKReaderSQLite*) m_poReader;
    
    osSQL.Printf("SELECT rowid FROM %s WHERE %s = %ld",
                 m_pszName, FID_COLUMN, nFID);
    if (EQUAL(m_pszName, "SBP")) {
        osSQL += " AND PORADOVE_CISLO_BODU = 1";
    }
    hStmt = poReader->PrepareStatement(osSQL.c_str());

    rowId = -1;
    if (poReader->ExecuteSQL(hStmt) == OGRERR_NONE) {
        rowId = sqlite3_column_int(hStmt, 0);
    }
    sqlite3_finalize(hStmt);
    
    return GetFeatureByIndex(rowId - 1);
}

/*!
  \brief Get first found feature based on it's property
  
  \param column property name
  \param value property value
  \param bGeom True to check also geometry != NULL
    
  \return pointer to feature definition or NULL on failure (not found)
*/
VFKFeatureSQLite *VFKDataBlockSQLite::GetFeature(const char *column, GUIntBig value,
                                                 bool bGeom)
{
    int idx;
    CPLString osSQL;
    VFKReaderSQLite  *poReader;

    sqlite3_stmt *hStmt;
    
    poReader = (VFKReaderSQLite*) m_poReader;
    
    osSQL.Printf("SELECT %s from %s WHERE %s = " CPL_FRMT_GUIB,
                 FID_COLUMN, m_pszName, column, value);
    if (bGeom) {
        CPLString osColumn;
        
        osColumn.Printf(" AND %s IS NOT NULL", GEOM_COLUMN);
        osSQL += osColumn;
    }
    
    hStmt = poReader->PrepareStatement(osSQL.c_str());
    if (poReader->ExecuteSQL(hStmt) != OGRERR_NONE)
        return NULL;
    
    idx = sqlite3_column_int(hStmt, 0) - 1;
    sqlite3_finalize(hStmt);
    
    if (idx < 0 || idx >= m_nFeatureCount) // ? assert
        return NULL;

    return (VFKFeatureSQLite *) GetFeatureByIndex(idx);
}

/*!
  \brief Get first found feature based on it's properties (AND)
  
  \param column array of property names
  \param value array of property values
  \param num number of array items
  \param bGeom True to check also geometry != NULL

  \return pointer to feature definition or NULL on failure (not found)
*/
VFKFeatureSQLite *VFKDataBlockSQLite::GetFeature(const char **column, GUIntBig *value, int num,
                                                 bool bGeom)
{
    int idx;
    CPLString osSQL, osItem;
    VFKReaderSQLite  *poReader;

    sqlite3_stmt *hStmt;
    
    poReader = (VFKReaderSQLite*) m_poReader;
    
    osSQL.Printf("SELECT %s FROM %s WHERE ", FID_COLUMN, m_pszName);
    for (int i = 0; i < num; i++) {
        if (i > 0)
            osItem.Printf(" AND %s = " CPL_FRMT_GUIB, column[i], value[i]);
        else
            osItem.Printf("%s = " CPL_FRMT_GUIB, column[i], value[i]);
        osSQL += osItem;
    }
    if (bGeom) {
        osItem.Printf(" AND %s IS NOT NULL", GEOM_COLUMN);
        osSQL += osItem;
    }

    hStmt = poReader->PrepareStatement(osSQL.c_str());
    if (poReader->ExecuteSQL(hStmt) != OGRERR_NONE)
        return NULL;
    
    idx = sqlite3_column_int(hStmt, 0) - 1; /* rowid starts at 1 */
    sqlite3_finalize(hStmt);    

    if (idx < 0 || idx >= m_nFeatureCount) // ? assert
        return NULL;
    
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
    int iRowId;
    CPLString osSQL, osItem;

    VFKReaderSQLite     *poReader;
    VFKFeatureSQLiteList fList;
    
    sqlite3_stmt *hStmt;
    
    poReader = (VFKReaderSQLite*) m_poReader;
    
    osSQL.Printf("SELECT rowid from %s WHERE ", m_pszName);
    for (int i = 0; i < num; i++) {
        if (i > 0)
            osItem.Printf(" OR %s = " CPL_FRMT_GUIB, column[i], value[i]);
        else
            osItem.Printf("%s = " CPL_FRMT_GUIB, column[i], value[i]);
        osSQL += osItem;
    }
    osSQL += " ORDER BY ";
    osSQL += FID_COLUMN;
    
    hStmt = poReader->PrepareStatement(osSQL.c_str());
    while (poReader->ExecuteSQL(hStmt) == OGRERR_NONE) {
        iRowId = sqlite3_column_int(hStmt, 0);
        fList.push_back((VFKFeatureSQLite *)GetFeatureByIndex(iRowId - 1));
    }
    
    return fList;
}

/*!
  \brief Save geometry to DB (as WKB)

  \param poGeom pointer to OGRGeometry to be saved
  \param iRowId row id to update

  \return OGRERR_NONE on success otherwise OGRERR_FAILURE
*/
OGRErr VFKDataBlockSQLite::SaveGeometryToDB(const OGRGeometry *poGeom, int iRowId)
{
    int        rc, nWKBLen;
    GByte     *pabyWKB;
    CPLString  osSQL;

    sqlite3_stmt *hStmt;

    VFKReaderSQLite  *poReader;

    poReader  = (VFKReaderSQLite*) m_poReader;

    if (poGeom) {
	nWKBLen = poGeom->WkbSize();
	pabyWKB = (GByte *) CPLMalloc(nWKBLen + 1);
	poGeom->exportToWkb(wkbNDR, pabyWKB);
        
	osSQL.Printf("UPDATE %s SET %s = ? WHERE rowid = %d",
		     m_pszName, GEOM_COLUMN, iRowId);
	hStmt = poReader->PrepareStatement(osSQL.c_str());
	
	rc = sqlite3_bind_blob(hStmt, 1, pabyWKB, nWKBLen, CPLFree);
	if (rc != SQLITE_OK) {
	    sqlite3_finalize(hStmt);
	    CPLError(CE_Failure, CPLE_AppDefined, 
		     "Storing geometry in DB failed");
	    return OGRERR_FAILURE;
	}
    }
    else { /* invalid */
	osSQL.Printf("UPDATE %s SET %s = NULL WHERE rowid = %d",
		     m_pszName, GEOM_COLUMN, iRowId);
	hStmt = poReader->PrepareStatement(osSQL.c_str());
    }

    return poReader->ExecuteSQL(hStmt); /* calls sqlite3_finalize() */
}

/*!
  \brief Load geometry from DB

  \return TRUE geometry successfully loaded otherwise FALSE
*/
bool VFKDataBlockSQLite::LoadGeometryFromDB()
{
    int nInvalid, nGeometries, nGeometriesCount, nBytes, rowId;
    long iFID;
    bool bAddFeature, bSkipInvalid;
    
    CPLString osSQL;
    
    OGRGeometry      *poGeometry;
    
    VFKFeatureSQLite *poFeature;
    VFKReaderSQLite  *poReader;
    
    sqlite3_stmt *hStmt;
    
    poReader = (VFKReaderSQLite*) m_poReader;

    if (!poReader->IsSpatial())   /* check if DB is spatial */
	return FALSE;

    osSQL.Printf("SELECT num_geometries FROM %s WHERE table_name = '%s'",
		 VFK_DB_TABLE, m_pszName);
    hStmt = poReader->PrepareStatement(osSQL.c_str());
    if (poReader->ExecuteSQL(hStmt) != OGRERR_NONE)
        return FALSE;
    nGeometries = sqlite3_column_int(hStmt, 0);
    sqlite3_finalize(hStmt);
    
    if (nGeometries < 1)
	return FALSE;
    
    bAddFeature = EQUAL(m_pszName, "SBP");
    bSkipInvalid = EQUAL(m_pszName, "OB") || EQUAL(m_pszName, "OP") || EQUAL(m_pszName, "OBBP");
    
    /* load geometry from DB */
    nInvalid = nGeometriesCount = 0;
    osSQL.Printf("SELECT %s,rowid,%s FROM %s ",
		 GEOM_COLUMN, FID_COLUMN, m_pszName);
    if (EQUAL(m_pszName, "SBP"))
	osSQL += "WHERE PORADOVE_CISLO_BODU = 1 ";
    osSQL += "ORDER BY ";
    osSQL += FID_COLUMN;
    hStmt = poReader->PrepareStatement(osSQL.c_str());

    while(poReader->ExecuteSQL(hStmt) == OGRERR_NONE) {
        rowId = sqlite3_column_int(hStmt, 1);
        iFID = sqlite3_column_double(hStmt, 2);

        if (bAddFeature) {
            /* add feature to the array */
            poFeature = new VFKFeatureSQLite(this, rowId, iFID);
            AddFeature(poFeature);
        }
        else {
            poFeature = (VFKFeatureSQLite *) GetFeatureByIndex(rowId - 1);
        }
        CPLAssert(NULL != poFeature && poFeature->GetFID() == iFID);
        
        // read geometry from DB
	nBytes = sqlite3_column_bytes(hStmt, 0);
	if (nBytes > 0 &&
	    OGRGeometryFactory::createFromWkb((GByte*) sqlite3_column_blob(hStmt, 0),
					      NULL, &poGeometry, nBytes) == OGRERR_NONE) {
	    nGeometriesCount++;
	    if (!poFeature->SetGeometry(poGeometry)) {
		nInvalid++;
	    }
	    delete poGeometry;
	}
	else {
	    nInvalid++;
	}
    }

    CPLDebug("OGR-VFK", "%s: %d geometries loaded from DB",
	     m_pszName, nGeometriesCount);

    if (nGeometriesCount != nGeometries) {
	CPLError(CE_Warning, CPLE_AppDefined, 
                 "%s: %d geometries loaded (should be %d)",
		 m_pszName, nGeometriesCount, nGeometries);
    }

    if (nInvalid > 0 && !bSkipInvalid) {
	CPLError(CE_Warning, CPLE_AppDefined, 
                 "%s: %d features with invalid or empty geometry found",
		 m_pszName, nInvalid);
    }

    return TRUE;
}

/*!
  \brief Update VFK_DB_TABLE table

  \param nGeometries number of geometries to update
*/
void VFKDataBlockSQLite::UpdateVfkBlocks(int nGeometries) {
    CPLString osSQL;
    
    VFKReaderSQLite  *poReader;
    
    poReader = (VFKReaderSQLite*) m_poReader;

    if (nGeometries > 0) {
        CPLDebug("OGR-VFK", 
                 "VFKDataBlockSQLite::UpdateVfkBlocks(): name=%s -> "
                 "%d geometries saved to internal DB", m_pszName, nGeometries);
        
	/* update number of geometries in VFK_DB_TABLE table */
	osSQL.Printf("UPDATE %s SET num_geometries = %d WHERE table_name = '%s'",
		     VFK_DB_TABLE, nGeometries, m_pszName);
	poReader->ExecuteSQL(osSQL.c_str());
    }
}

/*!
  \brief Update feature id (see SBP)

  \param iFID feature id to set up
  \param rowId list of rows to update
*/
void VFKDataBlockSQLite::UpdateFID(long int iFID, std::vector<int> rowId)
{
    CPLString osSQL, osValue;
    VFKReaderSQLite  *poReader;
    
    poReader = (VFKReaderSQLite*) m_poReader;
    
    /* update number of geometries in VFK_DB_TABLE table */
    osSQL.Printf("UPDATE %s SET %s = %ld WHERE rowid IN (",
                 m_pszName, FID_COLUMN, iFID);
    for (size_t i = 0; i < rowId.size(); i++) {
	if (i > 0)
	    osValue.Printf(",%d", rowId[i]);
	else
	    osValue.Printf("%d", rowId[i]);
	osSQL += osValue;
    }
    osSQL += ")";
    
    poReader->ExecuteSQL(osSQL.c_str());
}

/*!
  \brief Check is ring is closed

  \param poRing pointer to OGRLinearRing to check

  \return TRUE if closed otherwise FALSE
*/
bool VFKDataBlockSQLite::IsRingClosed(const OGRLinearRing *poRing)
{
    int nPoints;

    nPoints = poRing->getNumPoints();
    if (nPoints < 3)
	return FALSE;

    if (poRing->getX(0) == poRing->getX(nPoints-1) &&
	poRing->getY(0) == poRing->getY(nPoints-1))
	return TRUE;

    return FALSE;
}

/*!
  \brief Get primary key

  \return property name or NULL
*/
const char *VFKDataBlockSQLite::GetKey() const
{
    const char *pszKey;
    const VFKPropertyDefn *poPropDefn;

    if (GetPropertyCount() > 1) {
        poPropDefn = GetProperty(0);
        pszKey = poPropDefn->GetName();
        if (EQUAL(pszKey, "ID"))
            return pszKey;
    }

    return NULL;
}
