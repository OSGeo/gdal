/******************************************************************************
 * $Id$
 *
 * Project:  VFK Reader - Feature definition
 * Purpose:  Implements IVFKFeature/VFKFeature class.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2009-2010, 2012-2015, Martin Landa <landa.martin gmail.com>
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines-paris dot org>
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
    
    m_nFID          = -1;
    m_nGeometryType = poDataBlock->GetGeometryType();
    m_bGeometry     = FALSE;
    m_bValid        = FALSE;
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
  
  \param nFID feature id
*/
void IVFKFeature::SetFID(long nFID)
{
    if (m_nFID > 0) {
        m_nFID = nFID;
    }
    else {
        m_nFID = m_poDataBlock->GetFeatureCount() + 1;
    }
}

/*!
  \brief Set feature geometry

  Also checks if given geometry is valid

  \param poGeom pointer to OGRGeometry
  \param ftype geometry VFK type

  \return TRUE on valid feature or otherwise FALSE
*/
bool IVFKFeature::SetGeometry(OGRGeometry *poGeom, const char *ftype)
{
    m_bGeometry = TRUE;

    delete m_paGeom;
    m_paGeom = NULL;
    m_bValid = TRUE;

    if (!poGeom) {
	return m_bValid;
    }

    /* check empty geometries */
    if (m_nGeometryType == wkbNone && poGeom->IsEmpty()) {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "%s: empty geometry fid = %ld",
                     m_poDataBlock->GetName(), m_nFID);
        m_bValid = FALSE;
    }
    
    /* check coordinates */
    if (m_nGeometryType == wkbPoint) {
        double x, y;
        x = ((OGRPoint *) poGeom)->getX();
        y = ((OGRPoint *) poGeom)->getY();
        if (x > -430000 || x < -910000 ||
            y > -930000 || y < -1230000) {
            CPLDebug("OGR-VFK", "%s: invalid point fid = %ld",
                     m_poDataBlock->GetName(), m_nFID);
            m_bValid = FALSE;
        }
    }

    /* check degenerated polygons */
    if (m_nGeometryType == wkbPolygon) {
        OGRLinearRing *poRing;
        poRing = ((OGRPolygon *) poGeom)->getExteriorRing();
        if (!poRing || poRing->getNumPoints() < 3) {
	    CPLDebug("OGR-VFK", "%s: invalid polygon fid = %ld",
		     m_poDataBlock->GetName(), m_nFID);
            m_bValid = FALSE;
	}
    }

    if (m_bValid) {
        if (ftype) {
            OGRPoint pt;
            OGRGeometry *poGeomCurved;
            OGRCircularString poGeomString;
                
            poGeomCurved = NULL;
            if (EQUAL(ftype, "15") || EQUAL(ftype, "16")) {         /* -> circle or arc */
                int npoints;
                
                npoints = ((OGRLineString *) poGeom)->getNumPoints();
                for (int i = 0; i < npoints; i++) {
                    ((OGRLineString *) poGeom)->getPoint(i, &pt);
                    poGeomString.addPoint(&pt);
                }
                if (EQUAL(ftype, "15")) {
                    /* compute center and radius of a circle */
                    double x[3], y[3];
                    double m1, n1, m2, n2, c1, c2, mx;
                    double c_x, c_y;
                    
                    for (int i = 0; i < npoints; i++) {
                        ((OGRLineString *) poGeom)->getPoint(i, &pt);
                        x[i] = pt.getX();
                        y[i] = pt.getY();
                    }
                                        
                    m1 = (x[0] + x[1]) / 2.0;
                    n1 = (y[0] + y[1]) / 2.0;

                    m2 = (x[0] + x[2]) / 2.0;
                    n2 = (y[0] + y[2]) / 2.0;

                    c1 = (x[1] - x[0]) * m1 + (y[1] - y[0]) * n1;
                    c2 = (x[2] - x[0]) * m2 + (y[2] - y[0]) * n2;

                    mx = (x[1] - x[0]) * (y[2] - y[0]) + (y[1] - y[0]) * (x[0] - x[2]);

                    c_x = (c1 * (y[2] - y[0]) + c2 * (y[0] - y[1])) / mx;
                    c_y = (c1 * (x[0] - x[2]) + c2 * (x[1] - x[0])) / mx;
   
                    /* compute a new intermediate point */
                    pt.setX(c_x - (x[1] - c_x));
                    pt.setY(c_y - (y[1] - c_y));
                    poGeomString.addPoint(&pt);

                    /* add last point */
                    ((OGRLineString *) poGeom)->getPoint(0, &pt);
                    poGeomString.addPoint(&pt);

                }
            }
            else if (strlen(ftype) > 2 && EQUALN(ftype, "15", 2)) { /* -> circle with radius */
                float r;
                char s[3]; /* 15 */

                r = 0;
                if (2 != sscanf(ftype, "%s %f", s, &r) || r < 0) {
                    CPLDebug("OGR-VFK", "%s: invalid circle (unknown or negative radius) "
                             "fid = %ld", m_poDataBlock->GetName(), m_nFID);
                    m_bValid = FALSE;
                }
                else {
                    double c_x, c_y;
                    
                    ((OGRLineString *) poGeom)->getPoint(0, &pt);
                    c_x = pt.getX();
                    c_y = pt.getY();

                    /* define first point on a circle */
                    pt.setX(c_x + r);
                    pt.setY(c_y);
                    poGeomString.addPoint(&pt);

                    /* define second point on a circle */
                    pt.setX(c_x);
                    pt.setY(c_y + r);
                    poGeomString.addPoint(&pt);

                    /* define third point on a circle */
                    pt.setX(c_x - r);
                    pt.setY(c_y);
                    poGeomString.addPoint(&pt);

                    /* define fourth point on a circle */
                    pt.setX(c_x);
                    pt.setY(c_y - r);
                    poGeomString.addPoint(&pt);

                    /* define last point (=first) on a circle */
                    pt.setX(c_x + r);
                    pt.setY(c_y);
                    poGeomString.addPoint(&pt);
                }

            }
            else if (EQUAL(ftype, "11")) {                          /* curve */
                int npoints;

                npoints = ((OGRLineString *) poGeom)->getNumPoints();
                if (npoints > 2) { /* circular otherwise line string */
                    for (int i = 0; i < npoints; i++) {
                        ((OGRLineString *) poGeom)->getPoint(i, &pt);
                        poGeomString.addPoint(&pt);
                    }
                }
            }

            if (!poGeomString.IsEmpty())
                poGeomCurved = poGeomString.CurveToLine();
            
            if (poGeomCurved) {
                int npoints;
                
                npoints = ((OGRLineString *) poGeomCurved)->getNumPoints();
                CPLDebug("OGR-VFK", "%s: curve (type=%s) to linestring (npoints=%d) fid = %ld",
                         m_poDataBlock->GetName(), ftype,
                         npoints, m_nFID);
                if (npoints > 1)
                    m_paGeom = (OGRGeometry *) poGeomCurved->clone();
                delete poGeomCurved;
            }
        }

        if (!m_paGeom) {
            /* check degenerated linestrings */
            if (m_nGeometryType == wkbLineString) {
                int npoints;
                
                npoints = ((OGRLineString *) poGeom)->getNumPoints();
                if (npoints < 2) {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "%s: invalid linestring (%d vertices) fid = %ld",
                             m_poDataBlock->GetName(), npoints, m_nFID);
                    m_bValid = FALSE;
                }
            }

            if (m_bValid)
                m_paGeom = (OGRGeometry *) poGeom->clone(); /* make copy */
        }
    }

    return m_bValid;
}

/*!
  \brief Get feature geometry

  \return pointer to OGRGeometry or NULL on error
*/
OGRGeometry *IVFKFeature::GetGeometry()
{
    if (m_nGeometryType != wkbNone && !m_bGeometry)
        LoadGeometry();

    return m_paGeom;
}


/*!
  \brief Load geometry

  \return TRUE on success or FALSE on failure
*/
bool IVFKFeature::LoadGeometry()
{
    const char *pszName;
    
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
VFKFeature::VFKFeature(IVFKDataBlock *poDataBlock, long iFID) : IVFKFeature(poDataBlock)
{
    m_nFID = iFID;
    m_propertyList.assign(poDataBlock->GetPropertyCount(), VFKProperty());
    CPLAssert(size_t (poDataBlock->GetPropertyCount()) == m_propertyList.size());
}

/*!
  \brief Set feature properties

  \param pszLine pointer to line containing feature definition

  \return TRUE on success or FALSE on failure
*/
bool VFKFeature::SetProperties(const char *pszLine)
{
    unsigned int iIndex, nLength;
    const char *poChar, *poProp;
    char* pszProp;
    bool inString;
    
    std::vector<CPLString> oPropList;
    
    pszProp = NULL;
    
    for (poChar = pszLine; *poChar != '\0' && *poChar != ';'; poChar++)
        /* skip data block name */
        ;
    if (*poChar == '\0')
        return FALSE; /* nothing to read */

    poChar++; /* skip ';' after data block name*/

    /* read properties into the list */
    poProp = poChar;
    iIndex = nLength = 0;
    inString = FALSE;
    while(*poChar != '\0') {
        if (*poChar == '"' && 
            (*(poChar-1) == ';' || *(poChar+1) == ';' || *(poChar+1) == '\0')) {
            poChar++; /* skip '"' */
            inString = inString ? FALSE : TRUE;
            if (inString) {
                poProp = poChar;
                if (*poChar == '"' && (*(poChar+1) == ';' || *(poChar+1) == '\0')) { 
                    poChar++;
                    inString = FALSE;
                }
            }
            if (*poChar == '\0')
                break;
        }
        if (*poChar == ';' && !inString) {
            pszProp = (char *) CPLRealloc(pszProp, nLength + 1);
            if (nLength > 0)
                strncpy(pszProp, poProp, nLength);
            pszProp[nLength] = '\0';
            oPropList.push_back(pszProp);
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
    if (inString) {
        nLength--; /* ignore '"' */
    }
    pszProp = (char *) CPLRealloc(pszProp, nLength + 1);
    if (nLength > 0)
        strncpy(pszProp, poProp, nLength);
    pszProp[nLength] = '\0';
    oPropList.push_back(pszProp);

    /* set properties from the list */
    if (oPropList.size() != (size_t) m_poDataBlock->GetPropertyCount()) {
        /* try to read also invalid records */
        CPLError(CE_Warning, CPLE_AppDefined, 
                 "%s: invalid number of properties %d should be %d",
                 m_poDataBlock->GetName(),
		 (int) oPropList.size(), m_poDataBlock->GetPropertyCount());
        return FALSE;
   }
    iIndex = 0;
    for (std::vector<CPLString>::iterator ip = oPropList.begin();
	 ip != oPropList.end(); ++ip) {
	SetProperty(iIndex++, (*ip).c_str());
    }
    
    /* set fid 
    if (EQUAL(m_poDataBlock->GetName(), "SBP")) {
        GUIntBig id;
        const VFKProperty *poVfkProperty;

        poVfkProperty = GetProperty("PORADOVE_CISLO_BODU");
        if (poVfkProperty)
        {
            id = strtoul(poVfkProperty->GetValueS(), NULL, 0);
            if (id == 1)
                SetFID(0); 
            else
                SetFID(-1); 
        }
    }
    else {
        SetFID(0); 
    }
    */
    CPLFree(pszProp);

    return TRUE;
}

/*!
  \brief Set feature property

  \param iIndex property index
  \param pszValue property value

  \return TRUE on success
  \return FALSE on failure
*/
bool VFKFeature::SetProperty(int iIndex, const char *pszValue)
{
    if (iIndex < 0 || iIndex >= m_poDataBlock->GetPropertyCount() ||
	size_t(iIndex) >= m_propertyList.size())
        return FALSE;
    
    if (strlen(pszValue) < 1)
        m_propertyList[iIndex] = VFKProperty();
    else {
        OGRFieldType fType;

        const char *pszEncoding;
        char       *pszValueEnc;
                
        fType = m_poDataBlock->GetProperty(iIndex)->GetType();
        switch (fType) {
        case OFTInteger:
            m_propertyList[iIndex] = VFKProperty(atoi(pszValue));
            break;
        case OFTReal:
            m_propertyList[iIndex] = VFKProperty(CPLAtof(pszValue));
            break;
        default:
            pszEncoding = m_poDataBlock->GetProperty(iIndex)->GetEncoding();
            if (pszEncoding) {
                pszValueEnc = CPLRecode(pszValue, pszEncoding,
                                        CPL_ENC_UTF8);
                m_propertyList[iIndex] = VFKProperty(pszValueEnc);
                CPLFree(pszValueEnc);
            }
            else {
                m_propertyList[iIndex] = VFKProperty(pszValue);
            }
            break;
        }
    }
    return TRUE;
}

/*!
  \brief Get property value by index

  \param iIndex property index

  \return property value
  \return NULL on error
*/
const VFKProperty *VFKFeature::GetProperty(int iIndex) const
{
    if (iIndex < 0 || iIndex >= m_poDataBlock->GetPropertyCount() ||
	size_t(iIndex) >= m_propertyList.size())
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

  \return TRUE on success or FALSE on failure
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

  \return TRUE on success or FALSE on failure
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

  \return TRUE on success or FALSE on failure
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
