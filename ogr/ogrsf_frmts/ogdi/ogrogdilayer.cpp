/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGROGDILayer class.
 * Author:   Daniel Morissette, danmo@videotron.ca
 *           (Based on some code contributed by Frank Warmerdam :)
 *
 ******************************************************************************
 * Copyright (c) 2000, Daniel Morissette
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2000/08/24 04:16:19  danmo
 * Initial revision
 *
 */

#include "ogrogdi.h"
#include "cpl_conv.h"
#include "cpl_string.h"

/************************************************************************/
/*                           OGROGDILayer()                            */
/************************************************************************/

OGROGDILayer::OGROGDILayer( int nNewClientID, const char * pszName,
                            ecs_Family eFamily, ecs_Region *sGlobalBounds )
{
    m_nClientID = nNewClientID;
    m_eFamily = eFamily;

    m_pszOGDILayerName = CPLStrdup(pszName);

    m_poFilterGeom = NULL;
    m_sFilterBounds = *sGlobalBounds;

    m_iNextShapeId = 0;
    m_nTotalShapeCount = -1;
    
    // Select layer and feature family.
    ResetReading();

    BuildFeatureDefn();
}

/************************************************************************/
/*                           ~OGROGDILayer()                           */
/************************************************************************/

OGROGDILayer::~OGROGDILayer()

{
    if (m_poFeatureDefn)
        delete m_poFeatureDefn;

    CPLFree(m_pszOGDILayerName);

    if( m_poFilterGeom != NULL )
        delete m_poFilterGeom;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGROGDILayer::SetSpatialFilter( OGRGeometry * poGeomIn )

{
    if( m_poFilterGeom != NULL )
    {
        delete m_poFilterGeom;
        m_poFilterGeom = NULL;
    }

    if( poGeomIn != NULL )
    {
        OGREnvelope     oEnv;
        ecs_Result     *psResult;

        m_poFilterGeom = poGeomIn->clone();
        m_poFilterGeom->getEnvelope(&oEnv);

        m_sFilterBounds.north = oEnv.MaxY;
        m_sFilterBounds.south = oEnv.MinY;
        m_sFilterBounds.east  = oEnv.MinX;
        m_sFilterBounds.west  = oEnv.MaxX;

        psResult = cln_SelectRegion( m_nClientID, &m_sFilterBounds);
        if( ECSERROR(psResult) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s", psResult->message );
            return;
        }
    }

    m_iNextShapeId = 0;
    m_nTotalShapeCount = -1;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGROGDILayer::ResetReading()

{
    ecs_Result *psResult;
    ecs_LayerSelection sSelectionLayer;

    sSelectionLayer.Select = m_pszOGDILayerName;
    sSelectionLayer.F = m_eFamily;

    psResult = cln_SelectLayer(m_nClientID, &sSelectionLayer);
    if( ECSERROR( psResult ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Access to layer '%s' Failed: \n",
                  m_pszOGDILayerName, psResult->message );
        return;
    }

    m_iNextShapeId = 0;

}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGROGDILayer::GetNextFeature()

{
    OGRFeature	*poFeature=NULL;
    ecs_Result  *psResult;
    int         i;

    if (m_nTotalShapeCount != -1 && m_iNextShapeId >= m_nTotalShapeCount )
        return NULL;
        
/* -------------------------------------------------------------------- */
/*      Retrieve object from OGDI server and create new feature         */
/* -------------------------------------------------------------------- */
    psResult = cln_GetNextObject(m_nClientID);
    if (! ECSSUCCESS(psResult))
    {
        // We probably reached EOF... keep track of shape count.
        m_nTotalShapeCount = m_iNextShapeId;
        return NULL;
    }
   
    poFeature = new OGRFeature(m_poFeatureDefn);

    poFeature->SetFID( m_iNextShapeId++ );

/* -------------------------------------------------------------------- */
/*      Process geometry                                                */
/* -------------------------------------------------------------------- */
    if (m_eFamily == Point)
    {
        ecs_Point       *psPoint = &(ECSGEOM(psResult).point);
        OGRPoint        *poOGRPoint = new OGRPoint(psPoint->c.x, psPoint->c.y);

        poFeature->SetGeometryDirectly(poOGRPoint);
    }
    else if (m_eFamily == Line)
    {
        ecs_Line        *psLine = &(ECSGEOM(psResult).line);
        OGRLineString   *poOGRLine = new OGRLineString();

        poOGRLine->setNumPoints( psLine->c.c_len );

        for( i=0; i < (int) psLine->c.c_len; i++ ) 
        {
            poOGRLine->setPoint(i, psLine->c.c_val[i].x, psLine->c.c_val[i].y);
        }

        poFeature->SetGeometryDirectly(poOGRLine);
    }
    else if (m_eFamily == Area)
    {
        ecs_Area        *psArea = &(ECSGEOM(psResult).area);
        OGRPolygon      *poOGRPolygon = new OGRPolygon();

        for(int iRing=0; iRing < (int) psArea->ring.ring_len; iRing++)
        {
            ecs_FeatureRing     *psRing = &(psArea->ring.ring_val[iRing]);
            OGRLinearRing       *poOGRRing = new OGRLinearRing();

            poOGRRing->setNumPoints( psRing->c.c_len );

            for( i=0; i < (int) psRing->c.c_len; i++ ) 
            {
                poOGRRing->setPoint(i, psRing->c.c_val[i].x, 
                                    psRing->c.c_val[i].y);
            }
            poOGRPolygon->addRingDirectly(poOGRRing);
        }

        // __TODO__
        // When OGR supports polygon centroids then we should carry them here

        poFeature->SetGeometryDirectly(poOGRPolygon);
    }
    else if (m_eFamily == Text)
    {
        // __TODO__
        // For now text is treated as a point and string is lost
        //
        ecs_Text       *psText = &(ECSGEOM(psResult).text);
        OGRPoint        *poOGRPoint = new OGRPoint(psText->c.x, psText->c.y);

        poFeature->SetGeometryDirectly(poOGRPoint);
    }
    else
    {
        CPLAssert(FALSE);
    }

/* -------------------------------------------------------------------- */
/*      Set attributes                                                  */
/* -------------------------------------------------------------------- */
    char *pszAttrList = ECSOBJECTATTR(psResult);

    for( int iField = 0; iField < m_poFeatureDefn->GetFieldCount(); iField++ )
    {
        char        *pszFieldStart;
        int         nNameLen;
        char        chSavedChar;

        /* parse out the next attribute value */
        if( !ecs_FindElement( pszAttrList, &pszFieldStart, &pszAttrList,
                              &nNameLen, NULL ) )
        {
            nNameLen = 0;
            pszFieldStart = pszAttrList;
        }

        /* Skip any trailing white space (for string constants). */

        if( nNameLen > 0 && pszFieldStart[nNameLen-1] == ' ' )
            nNameLen--;

        /* zero terminate the single field value, but save the          */
        /* character we overwrote, so we can restore it when done.      */

        chSavedChar = pszFieldStart[nNameLen];
        pszFieldStart[nNameLen] = '\0';

        /* OGR takes care of all field type conversions for us! */

        poFeature->SetField(iField, pszFieldStart);

        pszFieldStart[nNameLen] = chSavedChar;
    }


    return poFeature;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGROGDILayer::GetFeature( long nFeatureId )

{
    ecs_Result  *psResult;

    if (m_nTotalShapeCount != -1 && nFeatureId > m_nTotalShapeCount)
        return NULL;

    if (m_iNextShapeId > nFeatureId )
        ResetReading();

    while(m_iNextShapeId != nFeatureId)
    {
        psResult = cln_GetNextObject(m_nClientID);
        if (ECSSUCCESS(psResult))
            m_iNextShapeId++;
        else
        {
            // We probably reached EOF... keep track of shape count.
            m_nTotalShapeCount = m_iNextShapeId;
            return NULL;
        }
    }

    // OK, we're ready to read the requested feature...
    return GetNextFeature();
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      If a spatial filter is in effect, we turn control over to       */
/*      the generic counter.  Otherwise we return the total count.      */
/*      Eventually we should consider implementing a more efficient     */
/*      way of counting features matching a spatial query.              */
/************************************************************************/

int OGROGDILayer::GetFeatureCount( int bForce )

{
    if( m_nTotalShapeCount == -1)
    {
        m_nTotalShapeCount = OGRLayer::GetFeatureCount( bForce );
    }

    return m_nTotalShapeCount;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGROGDILayer::TestCapability( const char * pszCap )

{
/* -------------------------------------------------------------------- */
/*      Hummm... what are the proper capabilities...                    */
/*      Does OGDI have any idea of capabilities???                      */
/*      For now just return FALSE for everything.                       */
/* -------------------------------------------------------------------- */
#ifdef __TODO__
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return poFilterGeom == NULL;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return FALSE;

    else 
        return FALSE;
#endif

    return FALSE;
}



/************************************************************************/
/*                          BuildFeatureDefn()                          */
/*                                                                      */
/*      (private) Initializes the schema in m_poFeatureDefn             */
/************************************************************************/

void OGROGDILayer::BuildFeatureDefn()
{
    ecs_Result  *psResult;
    ecs_ObjAttributeFormat *oaf;
    int         i, numFields;
    const char  *pszGeomName;
    OGRwkbGeometryType eLayerGeomType;

/* -------------------------------------------------------------------- */
/*      Feature Defn name will be "<OGDILyrName>_<FeatureFamily>"       */
/* -------------------------------------------------------------------- */
    
    switch(m_eFamily)
    {
      case Point:
        pszGeomName = "point";
        eLayerGeomType = wkbPoint;
        break;
      case Line:
        pszGeomName = "line";
        eLayerGeomType = wkbLineString;
        break;
      case Area:
        pszGeomName = "area";
        eLayerGeomType = wkbPolygon;
        break;
      case Text:
        pszGeomName = "text";
        eLayerGeomType = wkbPoint;
        break;
      default:
        pszGeomName = "unknown";
        eLayerGeomType = wkbUnknown;
        break;
    }

    m_poFeatureDefn = new OGRFeatureDefn(CPLSPrintf("%s_%s", 
                                                    m_pszOGDILayerName, 
                                                    pszGeomName ));
    m_poFeatureDefn->SetGeomType(eLayerGeomType);

/* -------------------------------------------------------------------- */
/*      Fetch schema from OGDI server and map to OGR types              */
/* -------------------------------------------------------------------- */
    psResult = cln_GetAttributesFormat( m_nClientID );
    if( ECSERROR( psResult ) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "ECSERROR: %s\n", psResult->message);
        return;
    }

    oaf = &(ECSRESULT(psResult).oaf);
    numFields = oaf->oa.oa_len;
    for( i = 0; i < numFields; i++ )
    {
        OGRFieldDefn	oField("", OFTInteger);

        oField.SetName( oaf->oa.oa_val[i].name );
        oField.SetPrecision( 0 );

        switch( oaf->oa.oa_val[i].type )
        {
          case Decimal:
          case Smallint:
          case Integer:
            oField.SetType( OFTInteger );
            if( oaf->oa.oa_val[i].lenght > 0 )
                oField.SetWidth( oaf->oa.oa_val[i].lenght );
            else
                oField.SetWidth( 11 );
            break;

          case Numeric:
          case Real:
          case Float:
          case Double:
            oField.SetType( OFTReal );
            if( oaf->oa.oa_val[i].lenght > 0 )
            {
                oField.SetWidth( oaf->oa.oa_val[i].lenght );
                oField.SetPrecision( oaf->oa.oa_val[i].precision );
            }
            else
            {
                oField.SetWidth( 18 );
                oField.SetPrecision( 7 );
            }
            break;

          case Char:
          case Varchar:
          case Longvarchar:
          default:
            oField.SetType( OFTString );
            if( oaf->oa.oa_val[i].lenght > 0 )
                oField.SetWidth( oaf->oa.oa_val[i].lenght );
            else
                oField.SetWidth( 64 );
            break;

        }

        m_poFeatureDefn->AddFieldDefn( &oField );
    }
    
}




