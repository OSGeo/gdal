/******************************************************************************
 * $Id$
 *
 * Project:  VFK Reader - Feature definition
 * Purpose:  Implements IVFKFeature/VFKFeature class.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2009-2010, 2012, Martin Landa <landa.martin gmail.com>
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

/*!
  \brief IVFKFeature constructor

  \param poDataBlock pointer to VFKDataBlock instance
*/
IVFKFeature::IVFKFeature(IVFKDataBlock *poDataBlock)
{
    CPLAssert(NULL != poDataBlock);
    m_poDataBlock   = poDataBlock;
    
    m_nFID           = -1;
    m_nGeometryType = poDataBlock->GetGeometryType();
    m_bGeometry     = FALSE;
    m_paGeom        = NULL;
}

/*!
  \brief IVFKFeature destructor
*/
IVFKFeature::~IVFKFeature()
{
    if (m_paGeom)
	delete m_paGeom;
    
    m_poDataBlock = NULL;
}

/*!
  \brief Set feature geometry type
*/
void IVFKFeature::SetGeometryType(OGRwkbGeometryType nGeomType)
{
    m_nGeometryType = nGeomType;
}

/*!
  \brief Set feature id

  FID: 0 for next, -1 for same
  
  \param FID feature id
*/
void IVFKFeature::SetFID(long nFID)
{
    if (m_nFID > 0)
    {
        m_nFID = nFID;
    }
    
    if (m_nFID < 1)
    {
        long nMaxFID = m_poDataBlock->GetMaxFID();
        if (nFID == 0) /* next */
        {
            m_nFID = nMaxFID + 1;
        }
        else /* same */
        {
            m_nFID = nMaxFID;
        }
    }
}

/*!
  \brief Set feature geometry

  \param poGeom pointer to OGRGeometry
*/
void IVFKFeature::SetGeometry(OGRGeometry *poGeom)
{
    m_bGeometry = TRUE;
    if (!poGeom)
        return;

    delete m_paGeom;
    m_paGeom = (OGRGeometry *) poGeom->clone(); /* make copy */
    
    if (m_nGeometryType == wkbNone && m_paGeom->IsEmpty())
    {
        CPLError(CE_Warning, CPLE_AppDefined, 
                 "Empty geometry FID %ld.\n", m_nFID);
    }

    if (m_nGeometryType == wkbLineString && ((OGRLineString *) m_paGeom)->getNumPoints() < 2)
    {
        CPLError(CE_Warning, CPLE_AppDefined, 
                 "Invalid LineString FID %ld (%d points).\n",
                 m_nFID,
                 ((OGRLineString *) m_paGeom)->getNumPoints());
    }
}

/*!
  \brief Get feature geometry

  \return pointer to OGRGeometry
  \return NULL on error
*/
OGRGeometry *IVFKFeature::GetGeometry()
{
    if (m_nGeometryType != wkbNone && !m_bGeometry)
        LoadGeometry();

    return m_paGeom;
}


/*!
  \brief Load geometry

  \return TRUE on success
  \return FALSE on failure
*/
bool IVFKFeature::LoadGeometry()
{
    const char *pszName;
    CPLString osSQL;
    
    if (m_bGeometry)
	return TRUE;

    pszName  = m_poDataBlock->GetName();
    
    if (EQUAL (pszName, "SOBR") ||
	EQUAL (pszName, "OBBP") ||
	EQUAL (pszName, "SPOL") ||
	EQUAL (pszName, "OB") ||
	EQUAL (pszName, "OP") ||
	EQUAL (pszName, "OBPEJ")) {
	/* -> wkbPoint */
	
	return LoadGeometryPoint();
    }
    else if (EQUAL (pszName, "SBP")) {
	/* -> wkbLineString */
	return LoadGeometryLineStringSBP();
    }
    else if (EQUAL (pszName, "HP") ||
	     EQUAL (pszName, "DPM")) {
	/* -> wkbLineString */
	return LoadGeometryLineStringHP();
    }
    else if (EQUAL (pszName, "PAR") ||
	     EQUAL (pszName, "BUD")) {
	/* -> wkbPolygon */
	return LoadGeometryPolygon();
    }

    return FALSE;
}

/*!
  \brief VFKFeature constructor

  \param poDataBlock pointer to VFKDataBlock instance
*/
VFKFeature::VFKFeature(IVFKDataBlock *poDataBlock) : IVFKFeature(poDataBlock)
{
    m_propertyList.assign(poDataBlock->GetPropertyCount(), VFKProperty());
    CPLAssert(size_t (poDataBlock->GetPropertyCount()) == m_propertyList.size());
}

/*!
  \brief Set feature properties

  \param pszLine pointer to line containing feature definition
*/
void VFKFeature::SetProperties(const char *poLine)
{
    int iIndex, nLength;
    const char *poChar, *poProp;
    char* pszProp;
    bool inString;
    
    pszProp = NULL;
    
    /* set feature properties */
    for (poChar = poLine; *poChar != '\0' && *poChar != ';'; poChar++)
	/* skip data block name */
	;
    if (poChar == '\0')
	return;

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
	    SetProperty(iIndex, pszProp);
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
    SetProperty(iIndex, pszProp);
    
    /* set fid */
    if (EQUAL(m_poDataBlock->GetName(), "SBP")) {
	GUIntBig id;
	const VFKProperty *poVfkProperty;
	
	poVfkProperty = GetProperty("PORADOVE_CISLO_BODU");
	id = strtoul(poVfkProperty->GetValueS(), NULL, 0);
	if (id == 1)
	    SetFID(0); /* set next feature */
	else
	    SetFID(-1); /* set same feature */
    }
    else {
	SetFID(0); /* set next feature */
    }
    m_poDataBlock->SetMaxFID(GetFID()); /* update max value */

    CPLFree(pszProp);
}

/*!
  \brief Set feature property

  \param iIndex property index
  \param pszValue property value
*/
void VFKFeature::SetProperty(int iIndex, const char *pszValue)
{
    if (iIndex < 0 || iIndex >= m_poDataBlock->GetPropertyCount() || size_t(iIndex) >= m_propertyList.size()) {
        CPLAssert(FALSE);
        return;
    }

    if (strlen(pszValue) < 1)
	m_propertyList[iIndex] = VFKProperty();
    else {
	OGRFieldType fType;
	fType = m_poDataBlock->GetProperty(iIndex)->GetType();
	switch (fType) {
	case OFTInteger:
	    m_propertyList[iIndex] = VFKProperty(atoi(pszValue));
	    break;
	case OFTReal:
	    m_propertyList[iIndex] = VFKProperty(CPLAtof(pszValue));
	    break;
	default:
	    m_propertyList[iIndex] = VFKProperty(pszValue);
	    break;
	}
    }
}

/*!
  \brief Get property value by index

  \param iIndex property index

  \return property value
  \return NULL on error
*/
const VFKProperty *VFKFeature::GetProperty(int iIndex) const
{
    if (iIndex < 0 || iIndex >= m_poDataBlock->GetPropertyCount() || size_t(iIndex) >= m_propertyList.size())
        return NULL;
    
    const VFKProperty* poProperty = &m_propertyList[iIndex];
    return poProperty;
}

/*!
  \brief Get property value by name

  \param pszName property name

  \return property value
  \return NULL on error
*/
const VFKProperty *VFKFeature::GetProperty(const char *pszName) const
{
    return GetProperty(m_poDataBlock->GetPropertyIndex(pszName));
}

/*!
  \brief Load geometry (point layers)

  \todo Really needed?
  
  \return TRUE on success
  \return FALSE on failure
*/
bool VFKFeature::LoadGeometryPoint()
{
    double x, y;
    int i_idxX, i_idxY;
    
    i_idxY = m_poDataBlock->GetPropertyIndex("SOURADNICE_Y");
    i_idxX = m_poDataBlock->GetPropertyIndex("SOURADNICE_X");
    if (i_idxY < 0 || i_idxX < 0)
	return FALSE;
    
    x = -1.0 * GetProperty(i_idxY)->GetValueD();
    y = -1.0 * GetProperty(i_idxX)->GetValueD();
    OGRPoint pt(x, y);
    SetGeometry(&pt);
    
    return TRUE;
}

/*!
  \brief Load geometry (linestring SBP layer)

  \todo Really needed?

  \return TRUE on success
  \return FALSE on failure
*/
bool VFKFeature::LoadGeometryLineStringSBP()
{
    int id, idxId, idxBp_Id, idxPCB, ipcb;
    
    VFKDataBlock *poDataBlockPoints;
    VFKFeature   *poPoint, *poLine;
    
    OGRLineString OGRLine;
    
    poDataBlockPoints = (VFKDataBlock *) m_poDataBlock->GetReader()->GetDataBlock("SOBR");
    if (!poDataBlockPoints)
	return FALSE;
    
    idxId    = poDataBlockPoints->GetPropertyIndex("ID");
    idxBp_Id = m_poDataBlock->GetPropertyIndex("BP_ID");
    idxPCB   = m_poDataBlock->GetPropertyIndex("PORADOVE_CISLO_BODU");
    if (idxId < 0 || idxBp_Id < 0 || idxPCB < 0)
	return false;
    
    poLine = this;
    while (TRUE)
    {
	id   = poLine->GetProperty(idxBp_Id)->GetValueI();
	ipcb = poLine->GetProperty(idxPCB)->GetValueI();
	if (OGRLine.getNumPoints() > 0 && ipcb == 1)
	{
	    m_poDataBlock->GetPreviousFeature(); /* push back */
	    break;
	}
        
	poPoint = poDataBlockPoints->GetFeature(idxId, id);
	if (!poPoint)
	{
	    continue;
	}
	OGRPoint *pt = (OGRPoint *) poPoint->GetGeometry();
	OGRLine.addPoint(pt);
        
	poLine = (VFKFeature *) m_poDataBlock->GetNextFeature();
	if (!poLine)
	    break;
    };
    
    OGRLine.setCoordinateDimension(2); /* force 2D */
    SetGeometry(&OGRLine);
    
    /* reset reading */
    poDataBlockPoints->ResetReading();
    
    return TRUE;
}

/*!
  \brief Load geometry (linestring HP/DPM layer)

  \todo Really needed?

  \return TRUE on success
  \return FALSE on failure
*/
bool VFKFeature::LoadGeometryLineStringHP()
{
    int           id, idxId, idxHp_Id;
    VFKDataBlock *poDataBlockLines;
    VFKFeature   *poLine;
    
    poDataBlockLines = (VFKDataBlock *) m_poDataBlock->GetReader()->GetDataBlock("SBP");
    if (!poDataBlockLines)
	return FALSE;
    
    idxId    = m_poDataBlock->GetPropertyIndex("ID");
    idxHp_Id = poDataBlockLines->GetPropertyIndex("HP_ID");
    if (idxId < 0 || idxHp_Id < 0)
	return FALSE;
    
    id = GetProperty(idxId)->GetValueI();
    poLine = poDataBlockLines->GetFeature(idxHp_Id, id);
    if (!poLine || !poLine->GetGeometry())
	return FALSE;
    
    SetGeometry(poLine->GetGeometry());
    poDataBlockLines->ResetReading();
    
    return TRUE;
}

/*!
  \brief Load geometry (polygon BUD/PAR layers)

  \todo Implement (really needed?)

  \return TRUE on success
  \return FALSE on failure
*/
bool VFKFeature::LoadGeometryPolygon()
{
    return FALSE;
}
OGRErr VFKFeature::LoadProperties(OGRFeature *poFeature)
{
    for (int iField = 0; iField < m_poDataBlock->GetPropertyCount(); iField++) {
	if (GetProperty(iField)->IsNull())
	    continue;
	OGRFieldType fType = poFeature->GetDefnRef()->GetFieldDefn(iField)->GetType();
	if (fType == OFTInteger) 
	    poFeature->SetField(iField,
				GetProperty(iField)->GetValueI());
	else if (fType == OFTReal)
	    poFeature->SetField(iField,
				GetProperty(iField)->GetValueD());
	else
	    poFeature->SetField(iField,
				GetProperty(iField)->GetValueS());
    }

    return OGRERR_NONE;
}
