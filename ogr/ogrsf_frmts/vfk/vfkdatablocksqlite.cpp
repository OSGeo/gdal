/******************************************************************************
 *
 * Project:  VFK Reader - Data block definition (SQLite)
 * Purpose:  Implements VFKDataBlockSQLite
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2012-2014, Martin Landa <landa.martin gmail.com>
 * Copyright (c) 2012-2014, Even Rouault <even dot rouault at spatialys.com>
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
#include <limits>
#include <map>
#include <utility>

#include "vfkreader.h"
#include "vfkreaderp.h"

#include "cpl_conv.h"
#include "cpl_error.h"

CPL_CVSID("$Id$")

/*!
  \brief VFKDataBlockSQLite constructor
*/
VFKDataBlockSQLite::VFKDataBlockSQLite(const char *pszName, const IVFKReader *poReader) :
   IVFKDataBlock(pszName, poReader),
   m_hStmt(nullptr)
{
}

/*!
  \brief Load geometry (point layers)

  \return number of invalid features
*/
int VFKDataBlockSQLite::LoadGeometryPoint()
{
    if (LoadGeometryFromDB()) /* try to load geometry from DB */
        return 0;

    const bool bSkipInvalid =
        EQUAL(m_pszName, "OB") || EQUAL(m_pszName, "OP") ||
        EQUAL(m_pszName, "OBBP");

    CPLString osSQL;
    osSQL.Printf("SELECT SOURADNICE_Y,SOURADNICE_X,%s,rowid FROM %s",
                 FID_COLUMN, m_pszName);

    VFKReaderSQLite *poReader = (VFKReaderSQLite*) m_poReader;
    sqlite3_stmt *hStmt = poReader->PrepareStatement(osSQL.c_str());

    if (poReader->IsSpatial())
        poReader->ExecuteSQL("BEGIN");

    int nGeometries = 0;
    int nInvalid = 0;
    while(poReader->ExecuteSQL(hStmt) == OGRERR_NONE) {
        /* read values */
        const double x = -1.0 * sqlite3_column_double(hStmt, 0); /* S-JTSK coordinate system expected */
        const double y = -1.0 * sqlite3_column_double(hStmt, 1);
        const GIntBig iFID = sqlite3_column_int64(hStmt, 2);
        const int rowId = sqlite3_column_int(hStmt, 3);

        VFKFeatureSQLite *poFeature = dynamic_cast<VFKFeatureSQLite *>(
            GetFeatureByIndex(rowId - 1));
        if( poFeature == nullptr || poFeature->GetFID() != iFID )
        {
            continue;
        }

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
  \brief Set geometry for linestrings

  \param poLine VFK feature
  \param oOGRLine line geometry
  \param[in,out] bValid true when feature's geometry is valid
  \param ftype geometry VFK type
  \param[in,out] rowIdFeat list of row ids which forms linestring
  \param[in,out] nGeometries number of features with valid geometry
*/
bool VFKDataBlockSQLite::SetGeometryLineString(VFKFeatureSQLite *poLine, OGRLineString *oOGRLine,
                                               bool& bValid, const char *ftype,
                                               std::vector<int>& rowIdFeat, int& nGeometries)
{
    VFKReaderSQLite *poReader = (VFKReaderSQLite*) m_poReader;

    oOGRLine->setCoordinateDimension(2); /* force 2D */

    /* check also VFK validity */
    if( bValid )
    {
        /* Feature types

           - '3'    - line       (2 points)
           - '4'    - linestring (at least 2 points)
           - '11'   - curve      (at least 2 points)
           - '15'   - circle     (3 points)
           - '15 r' - circle     (center point & radius)
           - '16'   - arc        (3 points)
        */

        const int npoints = oOGRLine->getNumPoints();
        if (EQUAL(ftype, "3") && npoints > 2) {
            /* be less pedantic, just inform user about data
             * inconsistency

               bValid = false;
            */
            CPLDebug("OGR-VFK",
                     "Line (fid=" CPL_FRMT_GIB ") defined by more than two vertices",
                     poLine->GetFID());
        }
        else if (EQUAL(ftype, "11") && npoints < 2) {
            bValid = false;
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Curve (fid=" CPL_FRMT_GIB ") defined by less than two vertices",
                     poLine->GetFID());
        }
        else if (EQUAL(ftype, "15") && npoints != 3) {
            bValid = false;
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Circle (fid=" CPL_FRMT_GIB ") defined by invalid number of vertices (%d)",
                     poLine->GetFID(), oOGRLine->getNumPoints());
        }
        else if (strlen(ftype) > 2 && STARTS_WITH_CI(ftype, "15") && npoints != 1) {
            bValid = false;
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Circle (fid=" CPL_FRMT_GIB ") defined by invalid number of vertices (%d)",
                     poLine->GetFID(), oOGRLine->getNumPoints());
        }
        else if (EQUAL(ftype, "16") && npoints != 3) {
            bValid = false;
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Arc (fid=" CPL_FRMT_GIB ") defined by invalid number of vertices (%d)",
                     poLine->GetFID(), oOGRLine->getNumPoints());
        }
    }

    /* set geometry (NULL for invalid features) */
    if( bValid )
    {
        if (!poLine->SetGeometry(oOGRLine, ftype)) {
            bValid = false;
        }
    }
    else
    {
        poLine->SetGeometry(nullptr);
    }

    /* update fid column */
    UpdateFID(poLine->GetFID(), rowIdFeat);

    /* store also geometry in DB */
    CPLAssert( !rowIdFeat.empty() );
    if( bValid && poReader->IsSpatial() &&
        SaveGeometryToDB(bValid ? poLine->GetGeometry() : nullptr,
                         rowIdFeat[0]) != OGRERR_FAILURE )
    {
        nGeometries++;
    }

    rowIdFeat.clear();
    oOGRLine->empty(); /* restore line */

    return bValid;
}

/*!
  \brief Load geometry (linestring SBP layer)

  \return number of invalid features
*/
int VFKDataBlockSQLite::LoadGeometryLineStringSBP()
{
    int nInvalid = 0;

    VFKDataBlockSQLite *poDataBlockPoints =
        (VFKDataBlockSQLite *) m_poReader->GetDataBlock("SOBR");
    if (nullptr == poDataBlockPoints) {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Data block %s not found.\n", m_pszName);
        return nInvalid;
    }

    int nGeometries = 0;
    VFKReaderSQLite *poReader = (VFKReaderSQLite*) m_poReader;

    poDataBlockPoints->LoadGeometry();

    if (LoadGeometryFromDB()) /* try to load geometry from DB */
        return 0;

    CPLString osSQL;
    osSQL.Printf("UPDATE %s SET %s = -1", m_pszName, FID_COLUMN);
    poReader->ExecuteSQL(osSQL.c_str());
    bool bValid = true;
    int iIdx = 0;

    VFKFeatureSQLite *poLine = nullptr;

    for( int i = 0; i < 2; i++ )
    {
        /* first collect linestrings related to HP, OB, DPM and ZVB
           then collect rest of linestrings */
        if( i == 0 )
            osSQL.Printf("SELECT BP_ID,PORADOVE_CISLO_BODU,PARAMETRY_SPOJENI,_rowid_ FROM '%s' WHERE "
                         "HP_ID IS NOT NULL OR OB_ID IS NOT NULL OR DPM_ID IS NOT NULL OR ZVB_ID IS NOT NULL "
                         "ORDER BY HP_ID,OB_ID,DPM_ID,ZVB_ID,PORADOVE_CISLO_BODU", m_pszName);
        else
            osSQL.Printf("SELECT BP_ID,PORADOVE_CISLO_BODU,PARAMETRY_SPOJENI,_rowid_ FROM '%s' WHERE "
                         "OB_ID IS NULL AND HP_ID IS NULL AND DPM_ID IS NULL AND ZVB_ID IS NULL "
                         "ORDER BY ID,PORADOVE_CISLO_BODU", m_pszName);

        sqlite3_stmt *hStmt = poReader->PrepareStatement(osSQL.c_str());

        if (poReader->IsSpatial())
            poReader->ExecuteSQL("BEGIN");

        std::vector<int> rowIdFeat;
        CPLString osFType;
        OGRLineString oOGRLine;

        while(poReader->ExecuteSQL(hStmt) == OGRERR_NONE) {
            // read values
            const GUIntBig id = sqlite3_column_int64(hStmt, 0);
            const GUIntBig ipcb  = sqlite3_column_int64(hStmt, 1);
            const char* pszFType = reinterpret_cast<const char*>(
                sqlite3_column_text(hStmt, 2));
            int rowId = sqlite3_column_int(hStmt, 3);

            if (ipcb == 1) {
                VFKFeatureSQLite *poFeature =
                    (VFKFeatureSQLite *) GetFeatureByIndex(iIdx);
                if( poFeature == nullptr )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot retrieve feature %d", iIdx);
                    sqlite3_finalize(hStmt);
                    break;
                }
                poFeature->SetRowId(rowId);

                /* set geometry & reset */
                if( poLine &&
                    !SetGeometryLineString(
                        poLine, &oOGRLine,
                        bValid, osFType.c_str(), rowIdFeat, nGeometries) )
                {
                    nInvalid++;
                }

                bValid = true;
                poLine = poFeature;
                osFType = pszFType ? pszFType : "";
                iIdx++;
            }

            VFKFeatureSQLite *poPoint =
                (VFKFeatureSQLite *) poDataBlockPoints->GetFeature("ID", id);
            if( poPoint )
            {
                OGRGeometry *pt = poPoint->GetGeometry();
                if (pt) {
                    oOGRLine.addPoint(pt->toPoint());
                }
                else
                {
                    CPLDebug("OGR-VFK",
                             "Geometry (point ID = " CPL_FRMT_GUIB ") not valid", id);
                    bValid = false;
                }
            }
            else
            {
                CPLDebug("OGR-VFK",
                         "Point ID = " CPL_FRMT_GUIB " not found (rowid = %d)",
                         id, rowId);
                bValid = false;
            }

            /* add vertex to the linestring */
            rowIdFeat.push_back(rowId);
        }

        /* add last line */
        if( poLine &&
            !SetGeometryLineString(
                poLine, &oOGRLine,
                bValid, osFType.c_str(), rowIdFeat, nGeometries) )
        {
            nInvalid++;
        }
        poLine = nullptr;

        if (poReader->IsSpatial())
            poReader->ExecuteSQL("COMMIT");
    }

    /* update number of geometries in VFK_DB_TABLE table */
    UpdateVfkBlocks(nGeometries);

    return nInvalid;
}

/*!
  \brief Load geometry (linestring HP/DPM/ZVB layer)

  \return number of invalid features
*/
int VFKDataBlockSQLite::LoadGeometryLineStringHP()
{
    int nInvalid = 0;
    VFKReaderSQLite *poReader = (VFKReaderSQLite*) m_poReader;

    VFKDataBlockSQLite *poDataBlockLines =
        (VFKDataBlockSQLite *) m_poReader->GetDataBlock("SBP");
    if (nullptr == poDataBlockLines) {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Data block %s not found.", m_pszName);
        return nInvalid;
    }

    poDataBlockLines->LoadGeometry();

    if (LoadGeometryFromDB()) /* try to load geometry from DB */
        return 0;

    CPLString osColumn;
    osColumn.Printf("%s_ID", m_pszName);
    const char *vrColumn[2] = {
        osColumn.c_str(),
        "PORADOVE_CISLO_BODU"
    };

    GUIntBig vrValue[2] = { 0, 1 }; // Reduce to first segment.

    CPLString osSQL;
    osSQL.Printf("SELECT ID,%s,rowid FROM %s", FID_COLUMN, m_pszName);
    /* TODO: handle points in DPM */
    if (EQUAL(m_pszName, "DPM"))
        osSQL += " WHERE SOURADNICE_X IS NULL";
    sqlite3_stmt *hStmt = poReader->PrepareStatement(osSQL.c_str());

    if (poReader->IsSpatial())
        poReader->ExecuteSQL("BEGIN");

    int nGeometries = 0;

    while( poReader->ExecuteSQL(hStmt) == OGRERR_NONE )
    {
        /* read values */
        vrValue[0] = sqlite3_column_int64(hStmt, 0);
        const GIntBig iFID = sqlite3_column_int64(hStmt, 1);
        const int rowId = sqlite3_column_int(hStmt, 2);

        VFKFeatureSQLite *poFeature =
            (VFKFeatureSQLite *) GetFeatureByIndex(rowId - 1);
        if( poFeature == nullptr || poFeature->GetFID() != iFID )
        {
            continue;
        }

        VFKFeatureSQLite *poLine =
            poDataBlockLines->GetFeature(vrColumn, vrValue, 2, TRUE);

        OGRGeometry *poOgrGeometry = nullptr;
        if( !poLine )
        {
            poOgrGeometry = nullptr;
        }
        else
        {
            poOgrGeometry = poLine->GetGeometry();
        }
        if (!poOgrGeometry || !poFeature->SetGeometry(poOgrGeometry)) {
            CPLDebug("OGR-VFK", "VFKDataBlockSQLite::LoadGeometryLineStringHP(): name=%s fid=" CPL_FRMT_GIB " "
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
    VFKReaderSQLite *poReader = (VFKReaderSQLite*) m_poReader;

    VFKDataBlockSQLite *poDataBlockLines1 = nullptr;
    VFKDataBlockSQLite *poDataBlockLines2 = nullptr;
    bool bIsPar = false;
    if (EQUAL (m_pszName, "PAR")) {
        poDataBlockLines1 = (VFKDataBlockSQLite *) m_poReader->GetDataBlock("HP");
        poDataBlockLines2 = poDataBlockLines1;
        bIsPar = true;
    }
    else {
        poDataBlockLines1 = (VFKDataBlockSQLite *) m_poReader->GetDataBlock("OB");
        poDataBlockLines2 = (VFKDataBlockSQLite *) m_poReader->GetDataBlock("SBP");
        bIsPar = false;
    }
    if( nullptr == poDataBlockLines1 )
    {
        CPLError(CE_Warning, CPLE_FileIO,
                 "Data block %s not found. Unable to build geometry for %s.",
                 bIsPar ? "HP" : "OB", m_pszName);
        return -1;
    }
    if( nullptr == poDataBlockLines2 )
    {
        CPLError(CE_Warning, CPLE_FileIO,
                 "Data block %s not found. Unable to build geometry for %s.",
                 "SBP", m_pszName);
        return -1;
    }

    poDataBlockLines1->LoadGeometry();
    poDataBlockLines2->LoadGeometry();

    if( LoadGeometryFromDB() )  // Try to load geometry from DB.
        return 0;

    const char *vrColumn[2] = { nullptr, nullptr };
    GUIntBig vrValue[2] = { 0, 0 };
    if (bIsPar) {
        vrColumn[0] = "PAR_ID_1";
        vrColumn[1] = "PAR_ID_2";
    }
    else {
        vrColumn[0] = "OB_ID";
        vrColumn[1] = "PORADOVE_CISLO_BODU";
        vrValue[1]  = 1;
    }

    CPLString osSQL;
    osSQL.Printf("SELECT ID,%s,rowid FROM %s", FID_COLUMN, m_pszName);
    sqlite3_stmt *hStmt = poReader->PrepareStatement(osSQL.c_str());

    if (poReader->IsSpatial())
        poReader->ExecuteSQL("BEGIN");

    VFKFeatureSQLiteList poLineList;
    /* first is to be considered as exterior */
    PointListArray poRingList;
    std::vector<OGRLinearRing *> poLinearRingList;
    OGRPolygon ogrPolygon;
    int nInvalidNoLines = 0;
    int nInvalidNoRings = 0;
    int nGeometries = 0;

    while(poReader->ExecuteSQL(hStmt) == OGRERR_NONE) {
        /* read values */
        const GUIntBig id = sqlite3_column_int64(hStmt, 0);
        const long iFID = static_cast<long>(sqlite3_column_int64(hStmt, 1));
        const int rowId = sqlite3_column_int(hStmt, 2);

        VFKFeatureSQLite *poFeature =
            (VFKFeatureSQLite *) GetFeatureByIndex(rowId - 1);
        if( poFeature == nullptr || poFeature->GetFID() != iFID )
        {
            continue;
        }

        if( bIsPar )
        {
            vrValue[0] = vrValue[1] = id;
            poLineList = poDataBlockLines1->GetFeatures(vrColumn, vrValue, 2);
        }
        else
        {
            // std::vector<VFKFeatureSQLite *> poLineListOb;

            osSQL.Printf("SELECT ID FROM %s WHERE BUD_ID = " CPL_FRMT_GUIB,
                         poDataBlockLines1->GetName(), id);
            if (poReader->IsSpatial()) {
                CPLString osColumn;

                osColumn.Printf(" AND %s IS NULL", GEOM_COLUMN);
                osSQL += osColumn;
            }
            sqlite3_stmt *hStmtOb = poReader->PrepareStatement(osSQL.c_str());

            while(poReader->ExecuteSQL(hStmtOb) == OGRERR_NONE) {
                const GUIntBig idOb = sqlite3_column_int64(hStmtOb, 0);
                vrValue[0] = idOb;
                VFKFeatureSQLite *poLineSbp =
                    poDataBlockLines2->GetFeature(vrColumn, vrValue, 2);
                if (poLineSbp)
                    poLineList.push_back(poLineSbp);
            }
        }
        size_t nLines = poLineList.size();
        if (nLines < 1) {
            CPLDebug("OGR-VFK",
                     "%s: unable to collect rings for polygon fid = %ld (no lines)",
                     m_pszName, iFID);
            nInvalidNoLines++;
            continue;
        }

        /* clear */
        ogrPolygon.empty();

        /* free ring list */
        for (PointListArray::iterator iRing = poRingList.begin(), eRing = poRingList.end();
            iRing != eRing; ++iRing) {
            delete (*iRing);
            *iRing = nullptr;
        }
        poRingList.clear();

        /* collect rings from lines */
#if 1
        // Fast version using a map to identify quickly a ring from its end point.
        std::map<std::pair<double,double>, PointList*> oMapEndRing;
        while( !poLineList.empty() )
        {
            auto pGeom = poLineList.front()->GetGeometry();
            if( pGeom )
            {
                auto poLine = pGeom->toLineString();
                if( poLine == nullptr || poLine->getNumPoints() < 2 )
                    continue;
                poLineList.erase(poLineList.begin());
                PointList* poList = new PointList();
                FillPointList(poList, poLine);
                poRingList.emplace_back(poList);
                OGRPoint oFirst, oLast;
                poLine->StartPoint(&oFirst);
                poLine->EndPoint(&oLast);
                oMapEndRing[std::pair<double,double>(
                    oLast.getX(), oLast.getY())] = poList;

                bool bWorkDone = true;
                while( bWorkDone && (*poList).front() != (*poList).back() )
                {
                    bWorkDone = false;
                    for( auto oIter = poLineList.begin(); oIter != poLineList.end(); ++oIter )
                    {
                        const auto& oCandidate = *oIter;
                        auto poCandidateGeom = oCandidate->GetGeometry();
                        if( poCandidateGeom == nullptr )
                            continue;
                        poLine = poCandidateGeom->toLineString();
                        if( poLine == nullptr || poLine->getNumPoints() < 2 )
                            continue;
                        poLine->StartPoint(&oFirst);
                        poLine->EndPoint(&oLast);
                        // MER = MapEndRing
                        auto oIterMER = oMapEndRing.find(std::pair<double,double>(
                            oFirst.getX(), oFirst.getY()));
                        if( oIterMER != oMapEndRing.end() )
                        {
                            auto ring = oIterMER->second;
                            PointList oList;
                            FillPointList(&oList, poLine);
                            /* forward, skip first point */
                            ring->insert(ring->end(), oList.begin()+1, oList.end());
                            poLineList.erase(oIter);
                            oMapEndRing.erase(oIterMER);
                            oMapEndRing[std::pair<double,double>(
                                oLast.getX(), oLast.getY())] = poList;
                            bWorkDone = true;
                            break;
                        }
                        oIterMER = oMapEndRing.find(std::pair<double,double>(
                            oLast.getX(), oLast.getY()));
                        if( oIterMER != oMapEndRing.end() )
                        {
                            auto ring = oIterMER->second;
                            PointList oList;
                            FillPointList(&oList, poLine);
                            /* backward, skip first point */
                            ring->insert(ring->end(), oList.rbegin()+1, oList.rend());
                            poLineList.erase(oIter);
                            oMapEndRing.erase(oIterMER);
                            oMapEndRing[std::pair<double,double>(
                                oFirst.getX(), oFirst.getY())] = ring;
                            bWorkDone = true;
                            break;
                        }
                    }
                }
            }
        }
#else
        bool bFound = false;
        int nCount = 0;
        const int nCountMax = static_cast<int>(nLines) * 2;
        while( !poLineList.empty() && nCount < nCountMax )
        {
            bool bNewRing = !bFound;
            bFound = false;
            int i = 1;
            for (VFKFeatureSQLiteList::iterator iHp = poLineList.begin(), eHp = poLineList.end();
                 iHp != eHp; ++iHp, ++i) {
                auto pGeom = (*iHp)->GetGeometry();
                if (pGeom && AppendLineToRing(&poRingList, pGeom->toLineString(), bNewRing)) {
                    bFound = true;
                    poLineList.erase(iHp);
                    break;
                }
            }
            nCount++;
        }
#endif
        CPLDebug("OGR-VFK", "%s: fid = %ld nlines = %d -> nrings = %d", m_pszName,
                 iFID, (int)nLines, (int)poRingList.size());

        if (!poLineList.empty()) {
            CPLDebug("OGR-VFK",
                     "%s: unable to collect rings for polygon fid = %ld",
                     m_pszName, iFID);
            nInvalidNoRings++;
            continue;
        }

        /* build rings */
        poLinearRingList.clear();
        OGRLinearRing *poOgrRing = nullptr;
        int i = 1;
        for( PointListArray::const_iterator iRing = poRingList.begin(),
                 eRing = poRingList.end();
             iRing != eRing;
             ++iRing)
        {
            PointList *poList = *iRing;

            poLinearRingList.push_back(new OGRLinearRing());
            poOgrRing = poLinearRingList.back();
            CPLAssert(nullptr != poOgrRing);

            for( PointList::iterator iPoint = poList->begin(),
                     ePoint = poList->end();
                 iPoint != ePoint;
                 ++iPoint)
            {
                OGRPoint *poPoint = &(*iPoint);
                poOgrRing->addPoint(poPoint);
            }
            i++;
        }

        /* find exterior ring */
        if( poLinearRingList.size() > 1 )
        {
            std::vector<OGRLinearRing *>::iterator exteriorRing;

            exteriorRing = poLinearRingList.begin();
            double dMaxArea = -1.0;
            for( std::vector<OGRLinearRing *>::iterator iRing =
                     poLinearRingList.begin(),
                     eRing = poLinearRingList.end();
                 iRing != eRing;
                 ++iRing )
            {
                poOgrRing = *iRing;
                if (!IsRingClosed(poOgrRing))
                    continue; /* skip unclosed rings */

                const double dArea = poOgrRing->get_Area();
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
        int nBridges = 0;
        for( std::vector<OGRLinearRing *>::iterator iRing =
                 poLinearRingList.begin(),
                 eRing = poLinearRingList.end();
             iRing != eRing;
             ++iRing )
        {
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
            *iRing = nullptr;
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
        *iRing = nullptr;
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
IVFKFeature *VFKDataBlockSQLite::GetFeature(GIntBig nFID)
{
    if (m_nFeatureCount < 0) {
        m_poReader->ReadDataRecords(this);
    }

    if (nFID < 1 || nFID > m_nFeatureCount)
        return nullptr;

    if( m_bGeometryPerBlock && !m_bGeometry )
    {
        LoadGeometry();
    }

    VFKReaderSQLite *poReader = (VFKReaderSQLite*) m_poReader;

    CPLString osSQL;
    osSQL.Printf("SELECT rowid FROM %s WHERE %s = " CPL_FRMT_GIB,
                 m_pszName, FID_COLUMN, nFID);
    if ( EQUAL(m_pszName, "SBP") || EQUAL(m_pszName, "SBPG") ) {
        osSQL += " AND PORADOVE_CISLO_BODU = 1";
    }
    sqlite3_stmt *hStmt = poReader->PrepareStatement(osSQL.c_str());

    int rowId = -1;
    if (poReader->ExecuteSQL(hStmt) == OGRERR_NONE) {
        rowId = sqlite3_column_int(hStmt, 0);
    }
    sqlite3_finalize(hStmt);

    return GetFeatureByIndex(rowId - 1);
}

/*!
  \brief Get first found feature based on its property

  \param column property name
  \param value property value
  \param bGeom True to check also geometry != NULL

  \return pointer to feature definition or NULL on failure (not found)
*/
VFKFeatureSQLite *VFKDataBlockSQLite::GetFeature(const char *column, GUIntBig value,
                                                 bool bGeom)
{
    VFKReaderSQLite *poReader = (VFKReaderSQLite*) m_poReader;

    CPLString osSQL;
    osSQL.Printf("SELECT %s from %s WHERE %s = " CPL_FRMT_GUIB,
                 FID_COLUMN, m_pszName, column, value);
    if( bGeom )
    {
        CPLString osColumn;

        osColumn.Printf(" AND %s IS NOT NULL", GEOM_COLUMN);
        osSQL += osColumn;
    }

    sqlite3_stmt *hStmt = poReader->PrepareStatement(osSQL.c_str());
    if (poReader->ExecuteSQL(hStmt) != OGRERR_NONE)
        return nullptr;

    const int idx = sqlite3_column_int(hStmt, 0) - 1;
    sqlite3_finalize(hStmt);

    if (idx < 0 || idx >= m_nFeatureCount) // ? assert
        return nullptr;

    return (VFKFeatureSQLite *) GetFeatureByIndex(idx);
}

/*!
  \brief Get first found feature based on its properties (AND)

  \param column array of property names
  \param value array of property values
  \param num number of array items
  \param bGeom True to check also geometry != NULL

  \return pointer to feature definition or NULL on failure (not found)
*/
VFKFeatureSQLite *VFKDataBlockSQLite::GetFeature(const char **column, GUIntBig *value, int num,
                                                 bool bGeom)
{
    VFKReaderSQLite *poReader = (VFKReaderSQLite*) m_poReader;

    CPLString osSQL;
    osSQL.Printf("SELECT %s FROM %s WHERE ", FID_COLUMN, m_pszName);

    CPLString osItem;
    for( int i = 0; i < num; i++ )
    {
        if (i > 0)
            osItem.Printf(" AND %s = " CPL_FRMT_GUIB, column[i], value[i]);
        else
            osItem.Printf("%s = " CPL_FRMT_GUIB, column[i], value[i]);
        osSQL += osItem;
    }
    if( bGeom )
    {
        osItem.Printf(" AND %s IS NOT NULL", GEOM_COLUMN);
        osSQL += osItem;
    }

    sqlite3_stmt *hStmt = poReader->PrepareStatement(osSQL.c_str());
    if (poReader->ExecuteSQL(hStmt) != OGRERR_NONE)
        return nullptr;

    int idx = sqlite3_column_int(hStmt, 0) - 1; /* rowid starts at 1 */
    sqlite3_finalize(hStmt);

    if (idx < 0 || idx >= m_nFeatureCount) // ? assert
        return nullptr;

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
    VFKReaderSQLite *poReader = (VFKReaderSQLite*) m_poReader;

    CPLString osItem;
    CPLString osSQL;
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

    VFKFeatureSQLiteList fList;

    sqlite3_stmt *hStmt = poReader->PrepareStatement(osSQL.c_str());
    while (poReader->ExecuteSQL(hStmt) == OGRERR_NONE) {
        const int iRowId = sqlite3_column_int(hStmt, 0);
        VFKFeatureSQLite* poFeature = dynamic_cast<VFKFeatureSQLite*>(
            GetFeatureByIndex(iRowId - 1));
        if( poFeature == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot retrieve feature %d", iRowId);
            sqlite3_finalize(hStmt);
            return VFKFeatureSQLiteList();
        }
        fList.push_back(poFeature);
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
    int        rc;
    CPLString  osSQL;

    sqlite3_stmt *hStmt = nullptr;

    VFKReaderSQLite *poReader = (VFKReaderSQLite*) m_poReader;

    /* check if geometry column exists (see SUPPRESS_GEOMETRY open
       option) */
    if ( AddGeometryColumn() != OGRERR_NONE )
        return OGRERR_FAILURE;

    if (poGeom) {
        const size_t nWKBLen = poGeom->WkbSize();
        if( nWKBLen > static_cast<size_t>(std::numeric_limits<int>::max()) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Too large geometry");
            return OGRERR_FAILURE;
        }
        GByte *pabyWKB = (GByte *) VSI_MALLOC_VERBOSE(nWKBLen);
        if( pabyWKB )
        {
            poGeom->exportToWkb(wkbNDR, pabyWKB);

            osSQL.Printf("UPDATE %s SET %s = ? WHERE rowid = %d",
                         m_pszName, GEOM_COLUMN, iRowId);
            hStmt = poReader->PrepareStatement(osSQL.c_str());

            rc = sqlite3_bind_blob(hStmt, 1, pabyWKB, static_cast<int>(nWKBLen), CPLFree);
            if (rc != SQLITE_OK) {
                sqlite3_finalize(hStmt);
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Storing geometry in DB failed");
                return OGRERR_FAILURE;
            }
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

  \return true if geometry successfully loaded otherwise false
*/
bool VFKDataBlockSQLite::LoadGeometryFromDB()
{
    VFKReaderSQLite *poReader = (VFKReaderSQLite*) m_poReader;

    if (!poReader->IsSpatial())   /* check if DB is spatial */
        return false;

    CPLString osSQL;
    osSQL.Printf("SELECT num_geometries FROM %s WHERE table_name = '%s'",
                 VFK_DB_TABLE, m_pszName);
    sqlite3_stmt *hStmt = poReader->PrepareStatement(osSQL.c_str());
    if (poReader->ExecuteSQL(hStmt) != OGRERR_NONE)
        return false;
    const int nGeometries = sqlite3_column_int(hStmt, 0);
    sqlite3_finalize(hStmt);

    if( nGeometries < 1 )
        return false;

    const bool bSkipInvalid =
        EQUAL(m_pszName, "OB") ||
        EQUAL(m_pszName, "OP") ||
        EQUAL(m_pszName, "OBBP");

    /* load geometry from DB */
    osSQL.Printf("SELECT %s,rowid,%s FROM %s ",
                 GEOM_COLUMN, FID_COLUMN, m_pszName);
    if ( EQUAL(m_pszName, "SBP") || EQUAL(m_pszName, "SBPG") )
        osSQL += "WHERE PORADOVE_CISLO_BODU = 1 ";
    osSQL += "ORDER BY ";
    osSQL += FID_COLUMN;
    hStmt = poReader->PrepareStatement(osSQL.c_str());

    int rowId = 0;
    int nInvalid = 0;
    int nGeometriesCount = 0;

    while(poReader->ExecuteSQL(hStmt) == OGRERR_NONE) {
        rowId++; // =sqlite3_column_int(hStmt, 1);
        const GIntBig iFID = sqlite3_column_int64(hStmt, 2);
        VFKFeatureSQLite *poFeature = dynamic_cast<VFKFeatureSQLite *>(
            GetFeatureByIndex(rowId - 1));
        if( poFeature == nullptr || poFeature->GetFID() != iFID )
        {
            continue;
        }

        // read geometry from DB
        const int nBytes = sqlite3_column_bytes(hStmt, 0);
        OGRGeometry *poGeometry = nullptr;
        if (nBytes > 0 &&
            OGRGeometryFactory::createFromWkb(sqlite3_column_blob(hStmt, 0),
                                              nullptr, &poGeometry, nBytes) == OGRERR_NONE) {
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
                 "%s: %d features with invalid or empty geometry",
                 m_pszName, nInvalid);
    }

    return true;
}

/*!
  \brief Update VFK_DB_TABLE table

  \param nGeometries number of geometries to update
*/
void VFKDataBlockSQLite::UpdateVfkBlocks(int nGeometries) {
    CPLString osSQL;

    VFKReaderSQLite *poReader = (VFKReaderSQLite*) m_poReader;

    /* update number of features in VFK_DB_TABLE table */
    const int nFeatCount = (int)GetFeatureCount();
    if (nFeatCount > 0) {
        osSQL.Printf("UPDATE %s SET num_features = %d WHERE table_name = '%s'",
                     VFK_DB_TABLE, nFeatCount, m_pszName);
        poReader->ExecuteSQL(osSQL.c_str());
    }

    /* update number of geometries in VFK_DB_TABLE table */
    if (nGeometries > 0) {
        CPLDebug("OGR-VFK",
                 "VFKDataBlockSQLite::UpdateVfkBlocks(): name=%s -> "
                 "%d geometries saved to internal DB", m_pszName, nGeometries);

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
void VFKDataBlockSQLite::UpdateFID(GIntBig iFID, std::vector<int> rowId)
{
    CPLString osSQL, osValue;
    VFKReaderSQLite *poReader = (VFKReaderSQLite*) m_poReader;

    /* update number of geometries in VFK_DB_TABLE table */
    osSQL.Printf("UPDATE %s SET %s = " CPL_FRMT_GIB " WHERE rowid IN (",
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

  \return true if closed otherwise false
*/
bool VFKDataBlockSQLite::IsRingClosed( const OGRLinearRing *poRing )
{
    const int nPoints = poRing->getNumPoints();
    if (nPoints < 3)
        return false;

    if (poRing->getX(0) == poRing->getX(nPoints-1) &&
        poRing->getY(0) == poRing->getY(nPoints-1))
        return true;

    return false;
}

/*!
  \brief Get primary key

  \return property name or NULL
*/
const char *VFKDataBlockSQLite::GetKey() const
{
    if( GetPropertyCount() > 1 )
    {
        const VFKPropertyDefn *poPropDefn = GetProperty(0);
        const char *pszKey = poPropDefn->GetName();
        if( EQUAL(pszKey, "ID") )
            return pszKey;
    }

    return nullptr;
}

/*!
  \brief Get geometry SQL type (for geometry_columns table)

  \return geometry_type as integer
*/
int VFKDataBlockSQLite::GetGeometrySQLType() const
{
    if (m_nGeometryType == wkbPolygon)
        return 3;
    else if (m_nGeometryType == wkbLineString)
        return 2;
    else if (m_nGeometryType == wkbPoint)
        return 1;

    return 0; /* unknown geometry type */
}

/*!
  \brief Add geometry column into table if not exists

  \return OGRERR_NONE on success otherwise OGRERR_FAILURE
 */
OGRErr VFKDataBlockSQLite::AddGeometryColumn() const
{
    CPLString osSQL;

    VFKReaderSQLite *poReader = (VFKReaderSQLite*) m_poReader;

    osSQL.Printf("SELECT %s FROM %s LIMIT 0",
                 GEOM_COLUMN, m_pszName);
    if ( poReader->ExecuteSQL(osSQL.c_str(), CE_None) == OGRERR_FAILURE ) {
        /* query failed, we assume that geometry column not exists */
        osSQL.Printf("ALTER TABLE %s ADD COLUMN %s blob",
                     m_pszName, GEOM_COLUMN);
        return poReader->ExecuteSQL(osSQL.c_str());
    }

    return OGRERR_NONE;
}

/*!
  \brief Load feature properties

  Used for sequential access, see OGRVFKLayer:GetNextFeature().

  \return OGRERR_NONE on success otherwise OGRERR_FAILURE
*/
OGRErr VFKDataBlockSQLite::LoadProperties()
{
    CPLString osSQL;

    if ( m_hStmt )
        sqlite3_finalize(m_hStmt);

    osSQL.Printf("SELECT * FROM %s", // TODO: where
                m_pszName);
    if ( EQUAL(m_pszName, "SBP") || EQUAL(m_pszName, "SBPG") )
        osSQL += " WHERE PORADOVE_CISLO_BODU = 1";

    m_hStmt = ((VFKReaderSQLite*) m_poReader)->PrepareStatement(osSQL.c_str());

    if ( m_hStmt == nullptr )
        return OGRERR_FAILURE;

    return OGRERR_NONE;
}

/*
  \brief Clean feature properties for a next run

  \return OGRERR_NONE on success otherwise OGRERR_FAILURE
*/
OGRErr VFKDataBlockSQLite::CleanProperties()
{
    if ( m_hStmt ) {
        if ( sqlite3_finalize(m_hStmt) != SQLITE_OK ) {
            m_hStmt = nullptr;
            return OGRERR_FAILURE;
        }
        m_hStmt = nullptr;
    }

    return OGRERR_NONE;
}
