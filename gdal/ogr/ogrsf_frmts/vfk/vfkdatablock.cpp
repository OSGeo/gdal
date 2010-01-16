/******************************************************************************
 * $Id$
 *
 * Project:  VFK Reader - Data block definition
 * Purpose:  Implements VFKDataBlock class.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2009-2010, Martin Landa <landa.martin gmail.com>
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

/*!
  \brief VFK Data Block constructor

  \param pszName data block name
*/
VFKDataBlock::VFKDataBlock(const char *pszName, const IVFKReader *poReader)
{
    m_pszName        = CPLStrdup(pszName);
    
    m_nPropertyCount = 0;
    m_papoProperty   = NULL;

    m_nFeatureCount  = 0;
    m_papoFeature    = NULL;

    m_iNextFeature   = -1;

    m_nGeometryType     = wkbUnknown;
    m_bGeometry         = FALSE;   /* geometry is not loaded by default */
    m_bGeometryPerBlock = TRUE;    /* load geometry per block/feature */

    m_nFID           = 0;

    m_poReader       = (IVFKReader *) poReader;
}

/*!
  \brief VFKDataBlock destructor
*/
VFKDataBlock::~VFKDataBlock()
{
    CPLFree(m_pszName);

    for (int i = 0; i < m_nPropertyCount; i++) {
	if (m_papoProperty[i])
	    delete m_papoProperty[i];
    }
    CPLFree(m_papoProperty);

    for (int i = 0; i < m_nFeatureCount; i++) {
	if (m_papoFeature[i])
	    delete m_papoFeature[i];
    }
    CPLFree(m_papoFeature);
}

/*!
  \brief Get property definition

  \param iIndex property index

  \return pointer to VFKPropertyDefn definition
  \return NULL on failure
*/
VFKPropertyDefn *VFKDataBlock::GetProperty(int iIndex) const
{
    if(iIndex < 0 || iIndex >= m_nPropertyCount)
        return NULL;

    return m_papoProperty[iIndex];
}

/*!
  \brief Set properties

  \param poLine pointer to line

  \return number of added properties
*/
int VFKDataBlock::SetProperties(char *poLine)
{
    char *poChar;
    char *pszName, *poProp, *pszType;
    int   nLength;
    
    pszName = pszType = NULL;
    
    /* skip data block name */
    for (poChar = poLine; *poChar != '0' && *poChar != ';'; poChar++)
	;
    if (*poChar == '\0')
        return 0;

    poChar++;

    /* read property name/type */
    poProp  = poChar;
    nLength = 0;
    while(*poChar != '\0' && !(*poChar == '\r' && *(poChar+1) == '\n')) {
	if (*poChar == ' ') {
	    pszName = (char *) CPLRealloc(pszName, nLength + 1);
	    strncpy(pszName, poProp, nLength);
	    pszName[nLength] = '\0';
	    
	    poProp = ++poChar;
	    nLength = 0;
	}
	else if (*poChar == ';') {
	    pszType = (char *) CPLRealloc(pszType, nLength + 1);
	    strncpy(pszType, poProp, nLength);
	    pszType[nLength] = '\0';
	    
	    /* add property */
            if (pszName && *pszName != '\0' &&
                pszType && *pszType != '\0')
	        AddProperty(pszName, pszType);
	    
	    poProp = ++poChar;
	    nLength = 0;
	}
	poChar++;
	nLength++;
    }

    pszType = (char *) CPLRealloc(pszType, nLength + 1);
    strncpy(pszType, poProp, nLength);
    pszType[nLength] = '\0';
    
    /* add property */
    if (pszName && *pszName != '\0' &&
        pszType && *pszType != '\0')
        AddProperty(pszName, pszType);
    
    CPLFree(pszName);
    CPLFree(pszType);

    return 0;
}

/*!
  \brief Add data block property

  \param pszName property name
  \param pszType property type

  \return number of properties
*/
int VFKDataBlock::AddProperty(const char *pszName, const char *pszType)
{
    VFKPropertyDefn *poNewProperty = new VFKPropertyDefn(pszName, pszType);

    m_nPropertyCount++;
    
    m_papoProperty = (VFKPropertyDefn **)
	CPLRealloc(m_papoProperty, sizeof (VFKPropertyDefn *) * m_nPropertyCount);
    m_papoProperty[m_nPropertyCount-1] = poNewProperty;

    return m_nPropertyCount;
}

/*!
  \brief Set number of features per data block

  \param nNewCount number of features
  \param bIncrement increment current value
*/
void VFKDataBlock::SetFeatureCount(int nNewCount, int bIncrement)
{
    if (bIncrement) {
	m_nFeatureCount += nNewCount;
    }
    else {
	m_nFeatureCount = nNewCount;
    }
}

/*!
  \brief Add feature

  \param pszLine pointer to line containing feature definition
*/
int VFKDataBlock::AddFeature(const char *poLine)
{
    int iIndex, nLength;
    const char *poChar, *poProp;
    char* pszProp;
    bool inString;
    
    VFKFeature *poNewFeature;

    poNewFeature = new VFKFeature(this);
    pszProp = NULL;
    
    /* set feature properties */
    for (poChar = poLine; *poChar != '\0' && *poChar != ';'; poChar++)
	/* skip data block name */
	;
    if (poChar == '\0')
        return m_nFeatureCount;

    poChar++;
    
    poProp = poChar;
    iIndex = 0;
    nLength = 0;
    inString = FALSE;
    while(*poChar != '\0' && !(*poChar == '\r' && *(poChar+1) == '\n')) {
	if (*poChar == '"' && 
	    (*(poChar-1) == ';' || *(poChar+1) == ';' || *(poChar+1) == '\r')) {
	    poChar++; /* skip '"' */
	    inString = inString ? FALSE : TRUE;
	    if (inString) {
		poProp = poChar;
		if (*poChar == '"') { 
		    poChar++;
		    inString = FALSE;
		}
	    }
	    if (*poChar == '\r' && *(poChar+1) == '\n')
		break;
	}
	if (*poChar == ';' && !inString) {
	    pszProp = (char *) CPLRealloc(pszProp, nLength + 1);
	    if (nLength > 0)
		strncpy(pszProp, poProp, nLength);
	    pszProp[nLength] = '\0';
	    poNewFeature->SetProperty(iIndex, pszProp);

	    iIndex++;
	    poProp = ++poChar;
	    nLength = 0;
	}
	else {
	    poChar++;
	    nLength++;
	}
    }
    /* append last property */
    pszProp = (char *) CPLRealloc(pszProp, nLength + 1);
    if (nLength > 0)
	strncpy(pszProp, poProp, nLength);
    pszProp[nLength] = '\0';
    poNewFeature->SetProperty(iIndex, pszProp);
    
    /* set fid */
    if (EQUAL(m_pszName, "SBP")) {
	int id;
	const VFKProperty *poVfkProperty;
	
	poVfkProperty = poNewFeature->GetProperty("PORADOVE_CISLO_BODU");
	id = poVfkProperty->GetValueI();
	if (id == 1)
	    poNewFeature->SetFID(0); /* set next feature */
	else
	    poNewFeature->SetFID(-1); /* set same feature */
    }
    else {
	poNewFeature->SetFID(0); /* set next feature */
    }
    SetMaxFID(poNewFeature->GetFID()); /* update max value */

    /* add feature */
    m_nFeatureCount++;
    
    m_papoFeature = (VFKFeature **)
	CPLRealloc(m_papoFeature, sizeof (VFKFeature *) * m_nFeatureCount);
    m_papoFeature[m_nFeatureCount-1] = poNewFeature;
    
    CPLFree(pszProp);
    
    return m_nFeatureCount;
}

/*!
  \brief Reset reading

  \param iIdx force index
*/
void VFKDataBlock::ResetReading(int iIdx)
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

  \return pointer to VFKFeature instance
  \return NULL on error
*/
VFKFeature *VFKDataBlock::GetNextFeature()
{
    if (m_bGeometryPerBlock && !m_bGeometry) {
	LoadGeometry();
    }
    
    if (m_iNextFeature < 0)
	ResetReading();

    if (m_iNextFeature < 0 || m_iNextFeature >= m_nFeatureCount)
        return NULL;
    
    return m_papoFeature[m_iNextFeature++];
}

/*!
  \brief Get previous feature

  \return pointer to VFKFeature instance
  \return NULL on error
*/
VFKFeature *VFKDataBlock::GetPreviousFeature()
{
    if (m_bGeometryPerBlock && !m_bGeometry) {
       LoadGeometry();
    }

    if (m_iNextFeature < 0)
	ResetReading();
    
    if (m_iNextFeature < 0 || m_iNextFeature >= m_nFeatureCount)
        return NULL;
    
    return m_papoFeature[m_iNextFeature--];
}

/*!
  \brief Get first feature

  \return pointer to VFKFeature instance
  \return NULL on error
*/
VFKFeature *VFKDataBlock::GetFirstFeature()
{
    if (m_bGeometryPerBlock && !m_bGeometry) {
	LoadGeometry();
    }
    
    if (m_nFeatureCount < 1)
        return NULL;
    
    return m_papoFeature[0];
}

/*!
  \brief Get last feature

  \return pointer to VFKFeature instance
  \return NULL on error
*/
VFKFeature *VFKDataBlock::GetLastFeature()
{
    if (m_bGeometryPerBlock && !m_bGeometry) {
	LoadGeometry();
    }
    
    if (m_nFeatureCount < 1)
        return NULL;
    
    return m_papoFeature[m_nFeatureCount-1];
}

/*!
  \brief Get property index by name

  \param pszName property name

  \return property index
  \return -1 on error (property name not found)
*/
int VFKDataBlock::GetPropertyIndex(const char *pszName) const
{
    for (int i = 0; i < m_nPropertyCount; i++)
        if (EQUAL(pszName,m_papoProperty[i]->GetName()))
            return i;
    
    return -1;
}

/*!
  \brief Set geometry type (point, linestring, polygon)

  \return geometry type
*/
OGRwkbGeometryType VFKDataBlock::SetGeometryType()
{
    m_nGeometryType = wkbNone; /* pure attribute records */
    
    if (EQUAL (m_pszName, "SOBR") ||
	EQUAL (m_pszName, "OBBP") ||
	EQUAL (m_pszName, "SPOL") ||
	EQUAL (m_pszName, "OB") ||
	EQUAL (m_pszName, "OP") ||
	EQUAL (m_pszName, "OBPEJ"))
	m_nGeometryType = wkbPoint;
    
    else if (EQUAL (m_pszName, "SBP") ||
	     EQUAL (m_pszName, "HP") ||
	     EQUAL (m_pszName, "DPM"))
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
OGRwkbGeometryType VFKDataBlock::GetGeometryType() const
{
    return m_nGeometryType;
}

/*!
  \brief Get vfkfeature

  \param iIndex feature index

  \return pointer to VFKFeature definition
  \return NULL on failure
*/
VFKFeature *VFKDataBlock::GetFeatureByIndex(int iIndex) const
{
    if(iIndex < 0 || iIndex >= m_nFeatureCount)
        return NULL;

    return m_papoFeature[iIndex];
}

/*!
  \brief Get first vfkfeature based on properties
  
  Note: modifies next feature.
  
  \param idx property index
  \param value property value
  \param poList list of features (NULL to loop all features)

  \return pointer to VFKFeature definition
  \return NULL on failure (not found)
*/
VFKFeature *VFKDataBlock::GetFeature(int idx, int value, VFKFeatureList *poList)
{
    int         iPropertyValue;
    VFKFeature *poVfkFeature;

    if (poList) {
	for (VFKFeatureList::iterator i = poList->begin(), e = poList->end();
	     i != e; ++i) {
	    poVfkFeature = *i;
	    iPropertyValue = poVfkFeature->GetProperty(idx)->GetValueI();
	    if (iPropertyValue == value) {
		poList->erase(i); /* ??? */
		return poVfkFeature;
	    }
	}
    }
    else {
	for (int i = 0; i < m_nFeatureCount; i++) {
	    poVfkFeature = GetFeatureByIndex(i);
	    iPropertyValue = poVfkFeature->GetProperty(idx)->GetValueI();
	    if (iPropertyValue == value) {
		m_iNextFeature = i + 1;
		return poVfkFeature;
	    }
	}
    }
    
    return NULL;
}

/*!
  \brief Get vfkfeatures based on properties
  
  \param idx property index
  \param value property value
  
  \return pointer to VFKFeature definition
  \return NULL on failure (not found)
*/
std::vector<VFKFeature *> VFKDataBlock::GetFeatures(int idx, int value)
{
    int         iPropertyValue;
    VFKFeature *poVfkFeature;
    std::vector<VFKFeature *> poResult;

    for (int i = 0; i < m_nFeatureCount; i++) {
	poVfkFeature = GetFeatureByIndex(i);
	iPropertyValue = poVfkFeature->GetProperty(idx)->GetValueI();
	if (iPropertyValue == value) {
	    poResult.push_back(poVfkFeature);
	}
    }
    
    return poResult;
}

/*!
  \brief Get vfkfeatures based on properties
  
  \param idx1, idx2 property index
  \param value property value
  
  \return pointer to VFKFeature definition
  \return NULL on failure (not found)
*/
std::vector<VFKFeature *> VFKDataBlock::GetFeatures(int idx1, int idx2, int value)
{
    int         iPropertyValue1, iPropertyValue2;
    VFKFeature *poVfkFeature;
    std::vector<VFKFeature *> poResult;

    for (int i = 0; i < m_nFeatureCount; i++) {
	poVfkFeature = GetFeatureByIndex(i);
	iPropertyValue1 = poVfkFeature->GetProperty(idx1)->GetValueI();
	if (idx2 < 0) {
	    if (iPropertyValue1 == value) {
		poResult.push_back(poVfkFeature);
	    }
	}
	else {
	    iPropertyValue2 = poVfkFeature->GetProperty(idx2)->GetValueI();
	    if (iPropertyValue1 == value || iPropertyValue2 == value) {
		poResult.push_back(poVfkFeature);
	    }
	}
    }
    
    return poResult;
}

/*!
  \brief Get vfkfeature by FID

  Modifies next feature id.
  
  \param nFID feature id

  \return pointer to VFKFeature definition
  \return NULL on failure (not found)
*/
VFKFeature *VFKDataBlock::GetFeature(long nFID)
{
    if (nFID < 1 || nFID > m_nFeatureCount)
	return NULL;

    if (m_bGeometryPerBlock && !m_bGeometry) {
	LoadGeometry();
    }
    
    if (m_nGeometryType == wkbPoint || m_nGeometryType == wkbPolygon) {
	m_iNextFeature = (int) nFID;
	return GetFeatureByIndex(int (nFID) - 1); /* zero-based index */
    }
    else if (m_nGeometryType == wkbLineString) {
	/* line string */
	for (int i = 0; i < m_nFeatureCount; i++) {
	    if (m_papoFeature[i]->GetFID() == nFID) {
		m_iNextFeature = i + 1;
		return m_papoFeature[i];
	    }
	}
    }
    
    return NULL;
}

/*!
  \brief Get feature count based on property value

  \param pszName property name
  \param pszValue property value

  \return number of features
  \return -1 on error
*/
int VFKDataBlock::GetFeatureCount(const char *pszName, const char *pszValue)
{
    int nfeatures, propIdx;
    VFKFeature *poVFKFeature;

    propIdx = GetPropertyIndex(pszName);
    if (propIdx < 0)
	return -1;
    
    nfeatures = 0;
    for (int i = 0; i < GetFeatureCount(); i++) {
	poVFKFeature = GetFeature(i);
	if (!poVFKFeature)
	    return -1;
	if (EQUAL (poVFKFeature->GetProperty(propIdx)->GetValueS(), pszValue))
	    nfeatures++;
    }

    return nfeatures;
}

/*!
  \brief Set maximum fid in datablock

  \param nFID fid number

  \return max fid
*/
long VFKDataBlock::SetMaxFID(long nFID)
{
    if (m_nFID < nFID)
	m_nFID = nFID;

    return m_nFID;
}

/*!
  \brief Load geometry

  \return number of processed features
  \return -1 on failure
*/
long VFKDataBlock::LoadGeometry()
{
    long        nfeatures;
    clock_t     start, end;
    VFKFeature *poFeature;
    
    if (m_bGeometry)
	return 0;
    
    nfeatures   = 0;
    m_bGeometry = TRUE;
    start       = clock();

    if (EQUAL (m_pszName, "SOBR") ||
	EQUAL (m_pszName, "OBBP") ||
	EQUAL (m_pszName, "SPOL") ||
	EQUAL (m_pszName, "OB") ||
	EQUAL (m_pszName, "OP") ||
	EQUAL (m_pszName, "OBPEJ")) {
	/* -> wkbPoint */
	double x, y;
	int i_idxX, i_idxY;
	
	i_idxY = GetPropertyIndex("SOURADNICE_Y");
	i_idxX = GetPropertyIndex("SOURADNICE_X");
	if (i_idxY < 0 || i_idxX < 0)
	    return -1;
	for (int j = 0; j < GetFeatureCount(); j++) {
	    poFeature = GetFeatureByIndex(j);
	    x = -1.0 * poFeature->GetProperty(i_idxY)->GetValueD();
	    y = -1.0 * poFeature->GetProperty(i_idxX)->GetValueD();
	    OGRPoint pt(x, y);
	    poFeature->SetGeometry(&pt);
	    nfeatures++;
	}
    }
    else if (EQUAL (m_pszName, "SBP")) {
	/* -> wkbLineString */
	int id, idxId, idxBp_Id, idxPCB, ipcb;

	VFKDataBlock *poDataBlockPoints;
	VFKFeature   *poPoint, *poLine = NULL;

	OGRLineString oOGRLine;
	
	poDataBlockPoints = m_poReader->GetDataBlock("SOBR");
	if (!poDataBlockPoints)
	    return -1;
	
	poDataBlockPoints->LoadGeometry();
	idxId    = poDataBlockPoints->GetPropertyIndex("ID");
	idxBp_Id = GetPropertyIndex("BP_ID");
	idxPCB   = GetPropertyIndex("PORADOVE_CISLO_BODU");
	if (idxId < 0 || idxBp_Id < 0 || idxPCB < 0)
	    return -1;
	
	for (int j = 0; j < GetFeatureCount(); j++) {
	    poFeature = GetFeatureByIndex(j);
	    poFeature->SetGeometry(NULL);
	    id   = poFeature->GetProperty(idxBp_Id)->GetValueI();
	    ipcb = poFeature->GetProperty(idxPCB)->GetValueI();
	    if (ipcb == 1) {
		if (!oOGRLine.IsEmpty()) {
		    oOGRLine.setCoordinateDimension(2); /* force 2D */
		    poLine->SetGeometry(&oOGRLine);
		    oOGRLine.empty(); /* restore line */
		}
		poLine = poFeature;
	    }
	    poPoint = poDataBlockPoints->GetFeature(idxId, id);
	    if (!poPoint)
		continue;
	    OGRPoint *pt = (OGRPoint *) poPoint->GetGeometry();
	    oOGRLine.addPoint(pt);
	    nfeatures++;
	}
	/* add last line */
	oOGRLine.setCoordinateDimension(2); /* force 2D */
        if (poLine)
  	    poLine->SetGeometry(&oOGRLine);
	
	poDataBlockPoints->ResetReading();
    }
    else if (EQUAL (m_pszName, "HP") ||
	     EQUAL (m_pszName, "DPM")) {
	/* -> wkbLineString */
	int           id, idxId, idxMy_Id, idxPCB;
	VFKDataBlock *poDataBlockLines;
	VFKFeature   *poLine;
	std::vector<VFKFeature *> poLineList;
	
	poDataBlockLines = m_poReader->GetDataBlock("SBP");
	if (!poDataBlockLines)
	    return nfeatures;
	
	poDataBlockLines->LoadGeometry();
	idxId    = GetPropertyIndex("ID");
	if (EQUAL (m_pszName, "HP"))
	    idxMy_Id = poDataBlockLines->GetPropertyIndex("HP_ID");
	else
	    idxMy_Id = poDataBlockLines->GetPropertyIndex("DPM_ID");
	idxPCB   = poDataBlockLines->GetPropertyIndex("PORADOVE_CISLO_BODU");
	if (idxId < 0 || idxMy_Id < 0 || idxPCB < 0)
	    return nfeatures;
	
	poLineList = poDataBlockLines->GetFeatures(idxPCB, 1); /* reduce to first segment */
	for (int i = 0; i < GetFeatureCount(); i++) {
	    poFeature = GetFeatureByIndex(i);
	    id = poFeature->GetProperty(idxId)->GetValueI();
	    poLine = poDataBlockLines->GetFeature(idxMy_Id, id, &poLineList);
	    if (!poLine || !poLine->GetGeometry())
		continue;
	    poFeature->SetGeometry(poLine->GetGeometry());
	    nfeatures++;
	}
	poDataBlockLines->ResetReading();
    }
    else if (EQUAL (m_pszName, "PAR") ||
	     EQUAL (m_pszName, "BUD")) {
	/* -> wkbPolygon */
	bool bIsPar, bNewRing, bFound;
	
	int id, idOb;
	int idxId, idxPar1, idxPar2, idxBud, idxOb, idxIdOb;

	VFKDataBlock *poDataBlockLines1, *poDataBlockLines2;
	
	std::vector<VFKFeature    *> poLineList;
	PointListArray               poRingList; /* first is to be considered as exterior */

	OGRLinearRing ogrRing;
	OGRPolygon    ogrPolygon;

	if (EQUAL (m_pszName, "PAR")) {
	    poDataBlockLines1 = m_poReader->GetDataBlock("HP");
	    poDataBlockLines2 = poDataBlockLines1;
	    bIsPar = TRUE;
	}
	else {
	    poDataBlockLines1 = m_poReader->GetDataBlock("OB");
	    poDataBlockLines2 = m_poReader->GetDataBlock("SBP");
	    bIsPar = FALSE;
	}
	if (!poDataBlockLines1 || !poDataBlockLines2)
	    return nfeatures;
	
	poDataBlockLines1->LoadGeometry();
	poDataBlockLines2->LoadGeometry();
	idxId = GetPropertyIndex("ID");
	if (idxId < 0)
	    return nfeatures;
	if (bIsPar) {
	    idxPar1 = poDataBlockLines1->GetPropertyIndex("PAR_ID_1");
	    idxPar2 = poDataBlockLines1->GetPropertyIndex("PAR_ID_2");
	    if (idxPar1 < 0 || idxPar2 < 0)
		return nfeatures;
	}
	else { /* BUD */
	    idxIdOb  = poDataBlockLines1->GetPropertyIndex("ID");
	    idxBud = poDataBlockLines1->GetPropertyIndex("BUD_ID");
	    idxOb  = poDataBlockLines2->GetPropertyIndex("OB_ID");
	    if (idxIdOb < 0 || idxBud < 0 || idxOb < 0)
		return nfeatures;
	}
	
	for (int i = 0; i < GetFeatureCount(); i++) {
	    poFeature = GetFeatureByIndex(i);
	    id = poFeature->GetProperty(idxId)->GetValueI();
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
		    idOb = poLineOb->GetProperty(idxIdOb)->GetValueI();
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
	    bFound = FALSE;
	    while (poLineList.size() > 0) {
		bNewRing = !bFound ? TRUE : FALSE;
		bFound = FALSE;
		for (VFKFeatureList::iterator iHp = poLineList.begin(), eHp = poLineList.end();
		     iHp != eHp; ++iHp) {
		    const OGRLineString *pLine = (OGRLineString *) (*iHp)->GetGeometry();
		    if (pLine && AppendLineToRing(&poRingList, pLine, bNewRing)) {
			bFound = TRUE;
			poLineList.erase(iHp);
			break;
		    }
		}
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
	    poFeature->SetGeometry(&ogrPolygon);
	    nfeatures++;
	}

	/* free ring list */
	for (PointListArray::iterator iRing = poRingList.begin(), eRing = poRingList.end();
	     iRing != eRing; ++iRing) {
	    delete (*iRing);
	    *iRing = NULL;
	}
	poDataBlockLines1->ResetReading();
	poDataBlockLines2->ResetReading();
    }
    
    end = clock();
    
    CPLDebug("OGR_VFK", "VFKDataBlock::LoadGeometry(): name=%s nfeatures=%ld sec=%ld",
	     m_pszName, nfeatures, (end - start) / CLOCKS_PER_SEC);
    
    return nfeatures;
}

/*!
  \brief Add linestring to a ring (private)

  \param[in,out] papoRing list of rings
  \param poLine pointer to linestring to be added to a ring 

  \return TRUE on success
  \return FALSE on failure
*/
bool VFKDataBlock::AppendLineToRing(PointListArray *papoRing, const OGRLineString *poLine, bool bNewRing)
{
    OGRPoint *poFirst, *poLast;
    OGRPoint *poFirstNew, *poLastNew;

    OGRPoint  pt;
    PointList poList;

    /* OGRLineString -> PointList */
    for (int i = 0; i < poLine->getNumPoints(); i++) {
	poLine->getPoint(i, &pt);
	poList.push_back(pt);
    }

    /* create new ring */
    if (bNewRing) {
	papoRing->push_back(new PointList(poList));
	return TRUE;
    }
    
    poFirstNew = &(poList.front());
    poLastNew  = &(poList.back());
    for (PointListArray::const_iterator i = papoRing->begin(), e = papoRing->end();
	 i != e; ++i) {
	PointList *ring = (*i);
	poFirst = &(ring->front());
	poLast  = &(ring->back());
	if (!poFirst || !poLast || poLine->getNumPoints() < 2)
	    return FALSE;

	if (poFirstNew->Equals(poLast)) {
	    /* forward, skip first point */
	    ring->insert(ring->end(), poList.begin()+1, poList.end());
	    return TRUE;
	}
	
	if (poFirstNew->Equals(poFirst)) {
	    /* backward, skip last point */
	    ring->insert(ring->begin(), poList.rbegin(), poList.rend()-1);
	    return TRUE;
	}
	
	if (poLastNew->Equals(poLast)) {
	    /* backward, skip first point */
	    ring->insert(ring->end(), poList.rbegin()+1, poList.rend());
	    return TRUE;
	}
	
	if (poLastNew->Equals(poFirst)) {
	    /* forward, skip last point */
	    ring->insert(ring->begin(), poList.begin(), poList.end()-1);
	    return TRUE;
	}
    }

    return FALSE;
}

/*!
  \brief Set next feature

  \param poFeature pointer to current feature

  \return index of current feature
  \return -1 on failure
*/
int VFKDataBlock::SetNextFeature(const VFKFeature *poFeature)
{
    for (int i = 0; i < m_nFeatureCount; i++) {
	if (m_papoFeature[i] == poFeature) {
	    m_iNextFeature = i + 1;
	    return i;
	}
    }

    return NULL;
}
