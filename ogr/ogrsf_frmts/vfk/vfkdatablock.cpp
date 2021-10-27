/******************************************************************************
 *
 * Project:  VFK Reader - Data block definition
 * Purpose:  Implements VFKDataBlock class.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2009-2013, Martin Landa <landa.martin gmail.com>
 * Copyright (c) 2012-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include <ctime>

#include "vfkreader.h"
#include "vfkreaderp.h"

#include "cpl_conv.h"
#include "cpl_error.h"

CPL_CVSID("$Id$")

/*!
  \brief VFK Data Block constructor

  \param pszName data block name
*/
IVFKDataBlock::IVFKDataBlock( const char *pszName, const IVFKReader *poReader ) :
    m_papoFeature(nullptr),
    m_nPropertyCount(0),
    m_papoProperty(nullptr),
    m_pszName(CPLStrdup(pszName)),
    m_bGeometry(false),   // Geometry is not loaded by default.
    m_nGeometryType(wkbUnknown),
    m_bGeometryPerBlock(true),    // Load geometry per block/feature.
    m_nFeatureCount(-1),  // Load data on first request.
    m_iNextFeature(-1),
    m_poReader(const_cast<IVFKReader *>(poReader))
{
    m_nRecordCount[RecordValid] = 0L;  // Number of valid records.
    m_nRecordCount[RecordSkipped] = 0L;  // Number of skipped (invalid) records.
    m_nRecordCount[RecordDuplicated] = 0L;  // Number of duplicated records.
}

/*!
  \brief VFKDataBlock destructor
*/
IVFKDataBlock::~IVFKDataBlock()
{
    CPLFree(m_pszName);

    for( int i = 0; i < m_nPropertyCount; i++ )
    {
        if( m_papoProperty[i] )
            delete m_papoProperty[i];
    }
    CPLFree(m_papoProperty);

    for( int i = 0; i < m_nFeatureCount; i++ )
    {
        if( m_papoFeature[i] )
            delete m_papoFeature[i];
    }
    CPLFree(m_papoFeature);
}

/*!
  \brief Get property definition

  \param iIndex property index

  \return pointer to VFKPropertyDefn definition or NULL on failure
*/
VFKPropertyDefn *IVFKDataBlock::GetProperty(int iIndex) const
{
    if(iIndex < 0 || iIndex >= m_nPropertyCount)
        return nullptr;

    return m_papoProperty[iIndex];
}

/*!
  \brief Set properties

  \param poLine pointer to line
*/
void IVFKDataBlock::SetProperties(const char *poLine)
{
    /* skip data block name */
    const char *poChar = strchr(poLine, ';');
    if( poChar == nullptr )
        return;

    poChar++;

    /* read property name/type */
    const char *poProp  = poChar;
    char *pszName = nullptr;
    char *pszType = nullptr;
    int nLength = 0;
    while(*poChar != '\0') {
        if (*poChar == ' ') {
            pszName = (char *) CPLRealloc(pszName, nLength + 1);
            strncpy(pszName, poProp, nLength);
            pszName[nLength] = '\0';

            poProp = ++poChar;
            nLength = 0;
            if( *poProp == '\0' )
                break;
        }
        else if (*poChar == ';') {
            pszType = (char *) CPLRealloc(pszType, nLength + 1);
            strncpy(pszType, poProp, nLength);
            pszType[nLength] = '\0';

            /* add property */
            if (pszName && *pszName != '\0' &&
                *pszType != '\0')
                AddProperty(pszName, pszType);

            poProp = ++poChar;
            nLength = 0;
            if( *poProp == '\0' )
                break;
        }
        poChar++;
        nLength++;
    }

    pszType = (char *) CPLRealloc(pszType, nLength + 1);
    if( nLength > 0 )
        strncpy(pszType, poProp, nLength);
    pszType[nLength] = '\0';

    /* add property */
    if (pszName && *pszName != '\0' &&
        *pszType != '\0')
        AddProperty(pszName, pszType);

    CPLFree(pszName);
    CPLFree(pszType);
}

/*!
  \brief Add data block property

  \param pszName property name
  \param pszType property type

  \return number of properties
*/
int IVFKDataBlock::AddProperty(const char *pszName, const char *pszType)
{
    /* Force text attributes to avoid int64 overflow
       see https://github.com/OSGeo/gdal/issues/672 */
    if( EQUAL( m_pszName, "VLA" ) &&
        ( EQUAL( pszName, "PODIL_CITATEL" ) ||
          EQUAL( pszName, "PODIL_JMENOVATEL" ) ) )
        pszType = "T30";

    VFKPropertyDefn *poNewProperty = new VFKPropertyDefn(pszName, pszType,
                                                         m_poReader->IsLatin2());

    m_nPropertyCount++;

    m_papoProperty = (VFKPropertyDefn **)
        CPLRealloc(m_papoProperty, sizeof (VFKPropertyDefn *) * m_nPropertyCount);
    m_papoProperty[m_nPropertyCount-1] = poNewProperty;

    return m_nPropertyCount;
}

/*!
  \brief Get number of features for given data block

  \param bForce true to force reading VFK data blocks if needed

  \return number of features
*/
GIntBig IVFKDataBlock::GetFeatureCount( bool bForce )
{
    if( bForce && m_nFeatureCount == -1 )
    {
        m_poReader->ReadDataRecords(this); /* read VFK data records */
        if( m_bGeometryPerBlock && !m_bGeometry )
        {
            LoadGeometry(); /* get real number of features */
        }
    }

    return m_nFeatureCount;
}

/*!
  \brief Set number of features per data block

  \param nNewCount number of features
  \param bIncrement increment current value
*/
void IVFKDataBlock::SetFeatureCount(int nNewCount, bool bIncrement)
{
    if (bIncrement) {
        m_nFeatureCount += nNewCount;
    }
    else {
        m_nFeatureCount = nNewCount;
    }
}

/*!
  \brief Reset reading

  \param iIdx force index
*/
void IVFKDataBlock::ResetReading(int iIdx)
{
    if (iIdx > -1) {
        m_iNextFeature = iIdx;
    }
    else {
        m_iNextFeature = 0;
    }
}

/*!
  \brief Get next feature

  \return pointer to VFKFeature instance or NULL on error
*/
IVFKFeature *IVFKDataBlock::GetNextFeature()
{
    if (m_nFeatureCount < 0) {
        m_poReader->ReadDataRecords(this);
    }

    if (m_bGeometryPerBlock && !m_bGeometry) {
        LoadGeometry();
    }

    if (m_iNextFeature < 0)
        ResetReading();

    if (m_iNextFeature < 0 || m_iNextFeature >= m_nFeatureCount)
        return nullptr;

    return m_papoFeature[m_iNextFeature++];
}

/*!
  \brief Get previous feature

  \return pointer to VFKFeature instance or NULL on error
*/
IVFKFeature *IVFKDataBlock::GetPreviousFeature()
{
    if (m_nFeatureCount < 0) {
        m_poReader->ReadDataRecords(this);
    }

    if (m_bGeometryPerBlock && !m_bGeometry) {
       LoadGeometry();
    }

    if (m_iNextFeature < 0)
        ResetReading();

    if (m_iNextFeature < 0 || m_iNextFeature >= m_nFeatureCount)
        return nullptr;

    return m_papoFeature[m_iNextFeature--];
}

/*!
  \brief Get first feature

  \return pointer to VFKFeature instance or NULL on error
*/
IVFKFeature *IVFKDataBlock::GetFirstFeature()
{
    if (m_nFeatureCount < 0) {
        m_poReader->ReadDataRecords(this);
    }

    if (m_bGeometryPerBlock && !m_bGeometry) {
        LoadGeometry();
    }

    if (m_nFeatureCount < 1)
        return nullptr;

    return m_papoFeature[0];
}

/*!
  \brief Get last feature

  \return pointer to VFKFeature instance or NULL on error
*/
IVFKFeature *IVFKDataBlock::GetLastFeature()
{
    if (m_nFeatureCount < 0) {
        m_poReader->ReadDataRecords(this);
    }

    if (m_bGeometryPerBlock && !m_bGeometry) {
        LoadGeometry();
    }

    if (m_nFeatureCount < 1)
        return nullptr;

    return m_papoFeature[m_nFeatureCount-1];
}

/*!
  \brief Get property index by name

  \param pszName property name

  \return property index or -1 on error (property name not found)
*/
int IVFKDataBlock::GetPropertyIndex(const char *pszName) const
{
    for (int i = 0; i < m_nPropertyCount; i++)
        if (EQUAL(pszName,m_papoProperty[i]->GetName()))
            return i;

    return -1;
}

/*!
  \brief Set geometry type (point, linestring, polygon)

  \param bSuppressGeometry True for forcing wkbNone type

  \return geometry type
*/
OGRwkbGeometryType IVFKDataBlock::SetGeometryType(bool bSuppressGeometry)
{
    m_nGeometryType = wkbNone; /* pure attribute records */
    if ( bSuppressGeometry ) {
        m_bGeometry = true; /* pretend that geometry is already loaded */

        return m_nGeometryType;
    }

    if (EQUAL (m_pszName, "SOBR") ||
        EQUAL (m_pszName, "OBBP") ||
        EQUAL (m_pszName, "SPOL") ||
        EQUAL (m_pszName, "OB") ||
        EQUAL (m_pszName, "OP") ||
        EQUAL (m_pszName, "OBPEJ"))
        m_nGeometryType = wkbPoint;

    else if (EQUAL (m_pszName, "SBP") ||
             EQUAL (m_pszName, "SBPG") ||
             EQUAL (m_pszName, "HP") ||
             EQUAL (m_pszName, "DPM") ||
             EQUAL (m_pszName, "ZVB"))
        m_nGeometryType = wkbLineString;

    else if (EQUAL (m_pszName, "PAR") ||
             EQUAL (m_pszName, "BUD"))
        m_nGeometryType = wkbPolygon;

    return m_nGeometryType;
}

/*!
  \brief Get geometry type

  \return geometry type
*/
OGRwkbGeometryType IVFKDataBlock::GetGeometryType() const
{
    return m_nGeometryType;
}

/*!
  \brief Get feature by index

  \param iIndex feature index

  \return pointer to feature definition or NULL on failure
*/
IVFKFeature *IVFKDataBlock::GetFeatureByIndex(int iIndex) const
{
    if(iIndex < 0 || iIndex >= m_nFeatureCount)
        return nullptr;

    return m_papoFeature[iIndex];
}

/*!
  \brief Get feature by FID

  Modifies next feature id.

  \param nFID feature id

  \return pointer to feature definition or NULL on failure (not found)
*/
IVFKFeature *IVFKDataBlock::GetFeature(GIntBig nFID)
{
    if (m_nFeatureCount < 0) {
        m_poReader->ReadDataRecords(this);
    }

    if (nFID < 1 || nFID > m_nFeatureCount)
        return nullptr;

    if (m_bGeometryPerBlock && !m_bGeometry) {
        LoadGeometry();
    }

    return GetFeatureByIndex(int (nFID) - 1); /* zero-based index */
}

/*!
  \brief Load geometry

  Print warning when some invalid features are detected.

  \return number of invalid features or -1 on failure
*/
int IVFKDataBlock::LoadGeometry()
{
    if( m_bGeometry )
        return 0;

    m_bGeometry = true;
    int nInvalid = 0;

#ifdef DEBUG_TIMING
    const clock_t start       = clock();
#endif

    if (m_nFeatureCount < 0) {
        m_poReader->ReadDataRecords(this);
    }

    if (EQUAL (m_pszName, "SOBR") ||
        EQUAL (m_pszName, "SPOL") ||
        EQUAL (m_pszName, "OP") ||
        EQUAL (m_pszName, "OBPEJ") ||
        EQUAL (m_pszName, "OB") ||
        EQUAL (m_pszName, "OBBP")) {
        /* -> wkbPoint */
        nInvalid = LoadGeometryPoint();
    }
    else if (EQUAL (m_pszName, "SBP") ||
             EQUAL (m_pszName, "SBPG")) {
        /* -> wkbLineString */
        nInvalid = LoadGeometryLineStringSBP();
    }
    else if (EQUAL (m_pszName, "HP") ||
             EQUAL (m_pszName, "DPM") ||
             EQUAL (m_pszName, "ZVB")) {
        /* -> wkbLineString */
        nInvalid = LoadGeometryLineStringHP();
    }
    else if (EQUAL (m_pszName, "PAR") ||
             EQUAL (m_pszName, "BUD")) {
        /* -> wkbPolygon */
        nInvalid = LoadGeometryPolygon();
    }

#ifdef DEBUG_TIMING
    const clock_t end = clock();
#endif

    if (nInvalid > 0) {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "%s: %d features with invalid or empty geometry", m_pszName, nInvalid);
    }

#ifdef DEBUG_TIMING
    CPLDebug("OGR-VFK", "VFKDataBlock::LoadGeometry(): name=%s time=%ld sec",
             m_pszName, (long)((end - start) / CLOCKS_PER_SEC));
#endif

    return nInvalid;
}

void IVFKDataBlock::FillPointList(PointList* poList, const OGRLineString *poLine)
{
    poList->reserve(poLine->getNumPoints());

    /* OGRLineString -> PointList */
    for( int i = 0; i < poLine->getNumPoints(); i++ )
    {
        OGRPoint pt;
        poLine->getPoint(i, &pt);
        poList->emplace_back(std::move(pt));
    }
}

/*!
  \brief Add linestring to a ring (private)

  \param[in,out] papoRing list of rings
  \param poLine pointer to linestring to be added to a ring
  \param bNewRing  create new ring
  \param bBackward allow backward direction

  \return true on success or false on failure
*/
bool IVFKDataBlock::AppendLineToRing(PointListArray *papoRing, const OGRLineString *poLine,
                                     bool bNewRing, bool bBackward)
{
    /* create new ring */
    if (bNewRing) {
        PointList* poList = new PointList();
        FillPointList(poList, poLine);
        papoRing->emplace_back(poList);
        return true;
    }

    if( poLine->getNumPoints() < 2)
        return false;
    OGRPoint oFirstNew;
    OGRPoint oLastNew;
    poLine->StartPoint(&oFirstNew);
    poLine->EndPoint(&oLastNew);

    for( PointList *ring: *papoRing )
    {
        const OGRPoint& oFirst = ring->front();
        const OGRPoint& oLast  = ring->back();

        if (oFirstNew.getX() == oLast.getX() &&
            oFirstNew.getY() == oLast.getY()) {
            PointList oList;
            FillPointList(&oList, poLine);
            /* forward, skip first point */
            ring->insert(ring->end(), oList.begin()+1, oList.end());
            return true;
        }

        if (bBackward &&
            oFirstNew.getX() == oFirst.getX() &&
            oFirstNew.getY() == oFirst.getY()) {
            PointList oList;
            FillPointList(&oList, poLine);
            /* backward, skip last point */
            ring->insert(ring->begin(), oList.rbegin(), oList.rend()-1);
            return true;
        }

        if (oLastNew.getX() == oLast.getX() &&
            oLastNew.getY() == oLast.getY()) {
            PointList oList;
            FillPointList(&oList, poLine);
            /* backward, skip first point */
            ring->insert(ring->end(), oList.rbegin()+1, oList.rend());
            return true;
        }

        if (bBackward &&
            oLastNew.getX() == oFirst.getX() &&
            oLastNew.getY() == oFirst.getY()) {
            PointList oList;
            FillPointList(&oList, poLine);
            /* forward, skip last point */
            ring->insert(ring->begin(), oList.begin(), oList.end()-1);
            return true;
        }
    }

    return false;
}

/*!
  \brief Set next feature

  \param poFeature pointer to current feature

  \return index of current feature or -1 on failure
*/
int IVFKDataBlock::SetNextFeature(const IVFKFeature *poFeature)
{
    for (int i = 0; i < m_nFeatureCount; i++) {
        if (m_papoFeature[i] == poFeature) {
            m_iNextFeature = i + 1;
            return i;
        }
    }

    return -1;
}

/*!
  \brief Add feature

  \param poNewFeature pointer to VFKFeature instance
*/
void IVFKDataBlock::AddFeature(IVFKFeature *poNewFeature)
{
    m_nFeatureCount++;

    m_papoFeature = (IVFKFeature **)
        CPLRealloc(m_papoFeature, sizeof (IVFKFeature *) * m_nFeatureCount);
    m_papoFeature[m_nFeatureCount-1] = poNewFeature;
}

/*!
  \brief Get number of records

  \param iRec record type (valid, skipped, duplicated)

  \return number of records
*/
int IVFKDataBlock::GetRecordCount(RecordType iRec) const
{
    return (int) m_nRecordCount[iRec];
}

/*!
  \brief Increment number of records

  \param iRec record type (valid, skipped, duplicated)
*/
void IVFKDataBlock::SetIncRecordCount(RecordType iRec)
{
    m_nRecordCount[iRec]++;
}

/*!
  \brief Get first found feature based on its properties

  Note: modifies next feature.

  \param idx property index
  \param value property value
  \param poList list of features (NULL to loop all features)

  \return pointer to feature definition or NULL on failure (not found)
*/
VFKFeature *VFKDataBlock::GetFeature(int idx, GUIntBig value, VFKFeatureList *poList)
{
    if (poList) {
        for( VFKFeatureList::iterator i = poList->begin(), e = poList->end();
             i != e;
             ++i )
        {
            VFKFeature *poVfkFeature = *i;
            const GUIntBig iPropertyValue =
                strtoul(poVfkFeature->GetProperty(idx)->GetValueS(), nullptr, 0);
            if (iPropertyValue == value) {
                poList->erase(i); /* ??? */
                return poVfkFeature;
            }
        }
    }
    else
    {
        for( int i = 0; i < m_nFeatureCount; i++ )
        {
            VFKFeature *poVfkFeature = (VFKFeature *) GetFeatureByIndex(i);
            const GUIntBig iPropertyValue =
                strtoul(poVfkFeature->GetProperty(idx)->GetValueS(), nullptr, 0);
            if (iPropertyValue == value) {
                m_iNextFeature = i + 1;
                return poVfkFeature;
            }
        }
    }

    return nullptr;
}

/*!
  \brief Get features based on properties

  \param idx property index
  \param value property value

  \return list of features
*/
VFKFeatureList VFKDataBlock::GetFeatures(int idx, GUIntBig value)
{
    std::vector<VFKFeature *> poResult;

    for (int i = 0; i < m_nFeatureCount; i++) {
        VFKFeature *poVfkFeature = (VFKFeature *) GetFeatureByIndex(i);
        const GUIntBig iPropertyValue =
            strtoul(poVfkFeature->GetProperty(idx)->GetValueS(), nullptr, 0);
        if (iPropertyValue == value) {
            poResult.push_back(poVfkFeature);
        }
    }

    return poResult;
}

/*!
  \brief Get features based on properties

  \param idx1 property index
  \param idx2 property index
  \param value property value

  \return list of features
*/
VFKFeatureList VFKDataBlock::GetFeatures(int idx1, int idx2, GUIntBig value)
{
    std::vector<VFKFeature *> poResult;

    for( int i = 0; i < m_nFeatureCount; i++ )
    {
        VFKFeature *poVfkFeature = (VFKFeature *) GetFeatureByIndex(i);
        const GUIntBig iPropertyValue1 =
            strtoul(poVfkFeature->GetProperty(idx1)->GetValueS(), nullptr, 0);
        if (idx2 < 0) {
            if (iPropertyValue1 == value) {
                poResult.push_back(poVfkFeature);
            }
        }
        else
        {
            const GUIntBig iPropertyValue2 =
                strtoul(poVfkFeature->GetProperty(idx2)->GetValueS(), nullptr, 0);
            if (iPropertyValue1 == value || iPropertyValue2 == value) {
                poResult.push_back(poVfkFeature);
            }
        }
    }

    return poResult;
}

/*!
  \brief Get feature count based on property value

  \param pszName property name
  \param pszValue property value

  \return number of features or -1 on error
*/
GIntBig VFKDataBlock::GetFeatureCount(const char *pszName, const char *pszValue)
{
    const int propIdx = GetPropertyIndex(pszName);
    if (propIdx < 0)
        return -1;

    int nfeatures = 0;
    for (int i = 0; i < ((IVFKDataBlock *) this)->GetFeatureCount(); i++) {
        VFKFeature *poVFKFeature =
            (VFKFeature *) ((IVFKDataBlock *) this)->GetFeature(i);
        if (!poVFKFeature)
            return -1;
        if (EQUAL (poVFKFeature->GetProperty(propIdx)->GetValueS(), pszValue))
            nfeatures++;
    }

    return nfeatures;
}

/*!
  \brief Load geometry (point layers)

  \return number of invalid features
*/
int VFKDataBlock::LoadGeometryPoint()
{
    int nInvalid = 0;
    int i_idxY = GetPropertyIndex("SOURADNICE_Y");
    int i_idxX = GetPropertyIndex("SOURADNICE_X");
    if (i_idxY < 0 || i_idxX < 0) {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Corrupted data (%s).\n", m_pszName);
        return nInvalid;
    }

    for (int j = 0; j < ((IVFKDataBlock *) this)->GetFeatureCount(); j++) {
        VFKFeature *poFeature = (VFKFeature *) GetFeatureByIndex(j);
        double x = -1.0 * poFeature->GetProperty(i_idxY)->GetValueD();
        double y = -1.0 * poFeature->GetProperty(i_idxX)->GetValueD();
        OGRPoint pt(x, y);
        if (!poFeature->SetGeometry(&pt))
            nInvalid++;
    }

    return nInvalid;
}

/*!
  \brief Load geometry (linestring SBP/SBPG layer)

  \return number of invalid features
*/
int VFKDataBlock::LoadGeometryLineStringSBP()
{
    VFKDataBlock *poDataBlockPoints = (VFKDataBlock *) m_poReader->GetDataBlock("SOBR");
    if (nullptr == poDataBlockPoints) {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Data block %s not found.\n", m_pszName);
        return 0;
    }

    poDataBlockPoints->LoadGeometry();
    int idxId    = poDataBlockPoints->GetPropertyIndex("ID");
    int idxBp_Id = GetPropertyIndex("BP_ID");
    int idxPCB   = GetPropertyIndex("PORADOVE_CISLO_BODU");
    if (idxId < 0 || idxBp_Id < 0 || idxPCB < 0) {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Corrupted data (%s).\n", m_pszName);
        return 0;
    }

    OGRLineString oOGRLine;
    VFKFeature   *poLine = nullptr;
    int nInvalid = 0;

    for (int j = 0; j < ((IVFKDataBlock *) this)->GetFeatureCount(); j++) {
        VFKFeature *poFeature = (VFKFeature *) GetFeatureByIndex(j);
        CPLAssert(nullptr != poFeature);

        poFeature->SetGeometry(nullptr);
        GUIntBig id   = strtoul(poFeature->GetProperty(idxBp_Id)->GetValueS(), nullptr, 0);
        GUIntBig ipcb = strtoul(poFeature->GetProperty(idxPCB)->GetValueS(), nullptr, 0);
        if (ipcb == 1) {
            if (!oOGRLine.IsEmpty()) {
                oOGRLine.setCoordinateDimension(2); /* force 2D */
                if (poLine != nullptr && !poLine->SetGeometry(&oOGRLine))
                    nInvalid++;
                oOGRLine.empty(); /* restore line */
            }
            poLine = poFeature;
        }
        else {
            poFeature->SetGeometryType(wkbUnknown);
        }
        VFKFeature *poPoint = poDataBlockPoints->GetFeature(idxId, id);
        if (!poPoint)
            continue;
        OGRPoint *pt = poPoint->GetGeometry()->toPoint();
        oOGRLine.addPoint(pt);
    }
    /* add last line */
    oOGRLine.setCoordinateDimension(2); /* force 2D */
    if (poLine) {
        if (!poLine->SetGeometry(&oOGRLine))
            nInvalid++;
    }
    poDataBlockPoints->ResetReading();

    return nInvalid;
}

/*!
  \brief Load geometry (linestring HP/DPM/ZVB layer)

  \return number of invalid features
*/
int VFKDataBlock::LoadGeometryLineStringHP()
{
    int nInvalid = 0;

    VFKDataBlock  *poDataBlockLines
        = (VFKDataBlock *) m_poReader->GetDataBlock("SBP");
    if (nullptr == poDataBlockLines) {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Data block %s not found.\n", m_pszName);
        return nInvalid;
    }

    poDataBlockLines->LoadGeometry();
    const int idxId = GetPropertyIndex("ID");
    CPLString osColumn;
    osColumn.Printf("%s_ID", m_pszName);
    const int idxMy_Id = poDataBlockLines->GetPropertyIndex(osColumn);
    const int idxPCB = poDataBlockLines->GetPropertyIndex("PORADOVE_CISLO_BODU");
    if (idxId < 0 || idxMy_Id < 0 || idxPCB < 0) {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Corrupted data (%s).\n", m_pszName);
        return nInvalid;
    }

    // Reduce to first segment.
    VFKFeatureList poLineList = poDataBlockLines->GetFeatures(idxPCB, 1);
    for (int i = 0; i < ((IVFKDataBlock *) this)->GetFeatureCount(); i++) {
        VFKFeature *poFeature = (VFKFeature *) GetFeatureByIndex(i);
        CPLAssert(nullptr != poFeature);
        GUIntBig id = strtoul(poFeature->GetProperty(idxId)->GetValueS(), nullptr, 0);
        VFKFeature *poLine
            = poDataBlockLines->GetFeature(idxMy_Id, id, &poLineList);
        if (!poLine || !poLine->GetGeometry())
            continue;
        if (!poFeature->SetGeometry(poLine->GetGeometry()))
            nInvalid++;
    }
    poDataBlockLines->ResetReading();

    return nInvalid;
}

/*!
  \brief Load geometry (polygon BUD/PAR layers)

  \return number of invalid features
*/
int VFKDataBlock::LoadGeometryPolygon()
{
    VFKDataBlock *poDataBlockLines1 = nullptr;
    VFKDataBlock *poDataBlockLines2 = nullptr;

    bool bIsPar = false;
    if (EQUAL (m_pszName, "PAR")) {
        poDataBlockLines1 = (VFKDataBlock *) m_poReader->GetDataBlock("HP");
        poDataBlockLines2 = poDataBlockLines1;
        bIsPar = true;
    }
    else {
        poDataBlockLines1 = (VFKDataBlock *) m_poReader->GetDataBlock("OB");
        poDataBlockLines2 = (VFKDataBlock *) m_poReader->GetDataBlock("SBP");
        bIsPar = false;
    }

    int nInvalid = 0;
    if (nullptr == poDataBlockLines1 || nullptr == poDataBlockLines2) {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Data block %s not found.\n", m_pszName);
        return nInvalid;
    }

    poDataBlockLines1->LoadGeometry();
    poDataBlockLines2->LoadGeometry();
    int idxId = GetPropertyIndex("ID");
    if (idxId < 0) {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Corrupted data (%s).\n", m_pszName);
        return nInvalid;
    }

    int idxBud = 0;
    int idxOb = 0;
    int idxIdOb = 0;
    int idxPar1 = 0;
    int idxPar2 = 0;
    if( bIsPar )
    {
        idxPar1 = poDataBlockLines1->GetPropertyIndex("PAR_ID_1");
        idxPar2 = poDataBlockLines1->GetPropertyIndex("PAR_ID_2");
        if (idxPar1 < 0 || idxPar2 < 0) {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Corrupted data (%s).\n", m_pszName);
            return nInvalid;
        }
    }
    else { /* BUD */
        idxIdOb  = poDataBlockLines1->GetPropertyIndex("ID");
        idxBud = poDataBlockLines1->GetPropertyIndex("BUD_ID");
        idxOb  = poDataBlockLines2->GetPropertyIndex("OB_ID");
        if (idxIdOb < 0 || idxBud < 0 || idxOb < 0) {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Corrupted data (%s).\n", m_pszName);
            return nInvalid;
        }
    }

    VFKFeatureList poLineList;
    PointListArray poRingList; /* first is to be considered as exterior */
    OGRLinearRing ogrRing;
    OGRPolygon ogrPolygon;

    for( int i = 0; i < ((IVFKDataBlock *) this)->GetFeatureCount(); i++ )
    {
        VFKFeature *poFeature = (VFKFeature *) GetFeatureByIndex(i);
        CPLAssert(nullptr != poFeature);
        const GUIntBig id =
            strtoul(poFeature->GetProperty(idxId)->GetValueS(), nullptr, 0);
        if (bIsPar) {
            poLineList = poDataBlockLines1->GetFeatures(idxPar1, idxPar2, id);
        }
        else {
            VFKFeature *poLineOb, *poLineSbp;
            std::vector<VFKFeature *> poLineListOb;
            poLineListOb = poDataBlockLines1->GetFeatures(idxBud, id);
            for (std::vector<VFKFeature *>::const_iterator iOb = poLineListOb.begin(), eOb = poLineListOb.end();
                 iOb != eOb; ++iOb) {
                poLineOb = (*iOb);
                GUIntBig idOb = strtoul(poLineOb->GetProperty(idxIdOb)->GetValueS(), nullptr, 0);
                poLineSbp = poDataBlockLines2->GetFeature(idxOb, idOb);
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
        bool bFound = false;
        int nCount = 0;
        int nCountMax = static_cast<int>(poLineList.size()) * 2;
        while (!poLineList.empty() && nCount < nCountMax) {
            bool bNewRing = !bFound;
            bFound = false;
            for (VFKFeatureList::iterator iHp = poLineList.begin(), eHp = poLineList.end();
                 iHp != eHp; ++iHp) {
                auto pGeom = (*iHp)->GetGeometry();
                if (pGeom && AppendLineToRing(&poRingList, pGeom->toLineString(), bNewRing)) {
                    bFound = true;
                    poLineList.erase(iHp);
                    break;
                }
            }
            nCount++;
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
    for (PointListArray::iterator iRing = poRingList.begin(),
             eRing = poRingList.end(); iRing != eRing; ++iRing) {
        delete (*iRing);
        *iRing = nullptr;
    }
    poDataBlockLines1->ResetReading();
    poDataBlockLines2->ResetReading();

    return nInvalid;
}
