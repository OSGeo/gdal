/******************************************************************************
 * $Id$
 *
 * Project:  FMEObjects Translator
 * Purpose:  Implementation of the OGRFMELayer base class.  The class
 *           implements behaviour shared between database and spatial cached
 *           layer types.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, 2001 Safe Software Inc.
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "fme2ogr.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            OGRFMELayer()                             */
/************************************************************************/

OGRFMELayer::OGRFMELayer( OGRFMEDataSource *poDSIn )

{
    poDS = poDSIn;

    poFeatureDefn = NULL;
    poSpatialRef = NULL;
    pszAttributeFilter = NULL;

    poFMEFeature = NULL;
}

/************************************************************************/
/*                            ~OGRFMELayer()                            */
/************************************************************************/

OGRFMELayer::~OGRFMELayer()

{
    if( m_nFeaturesRead > 0 && poFeatureDefn != NULL )
    {
        CPLDebug( "FME", "%d features read on layer '%s'.",
                  (int) m_nFeaturesRead, 
                  poFeatureDefn->GetName() );
    }

    CPLFree( pszAttributeFilter );

    if( poFMEFeature != NULL )
        poDS->GetFMESession()->destroyFeature( poFMEFeature );

    if( poFeatureDefn != NULL )
    {
        poFeatureDefn->Release();
    }

    if( poSpatialRef != NULL )
        poSpatialRef->Release();
}

/************************************************************************/
/*                             Initialize()                             */
/*                                                                      */
/*      Build an OGRFeatureDefn for this layer from the passed          */
/*      schema IFMEFeature.                                             */
/************************************************************************/

int OGRFMELayer::Initialize( IFMEFeature * poSchemaFeature,
                             OGRSpatialReference *poSRS )

{
    IFMEString  *poFMEString = NULL;
    
    poFMEString = poDS->GetFMESession()->createString();
    poFMEFeature = poDS->GetFMESession()->createFeature();

    if( poSRS != NULL )
        poSpatialRef = poSRS->Clone();

/* -------------------------------------------------------------------- */
/*      Create the definition with the definition name being the        */
/*      same as the FME feature type.                                   */
/* -------------------------------------------------------------------- */
    poSchemaFeature->getFeatureType( *poFMEString );

    poFeatureDefn = new OGRFeatureDefn( poFMEString->data() );
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();

    poDS->GetFMESession()->destroyString( poFMEString );

/* -------------------------------------------------------------------- */
/*      Get the list of attribute names.                                */
/* -------------------------------------------------------------------- */
    IFMEStringArray     *poAttrNames;

    poAttrNames = poDS->GetFMESession()->createStringArray();
    poSchemaFeature->getAllAttributeNames( *poAttrNames );

/* ==================================================================== */
/*      Loop over attributes, adding them to our feature defn.          */
/* ==================================================================== */
    OGRwkbGeometryType eGeomType = wkbNone;
    IFMEString  *poAttrValue;
    poAttrValue = poDS->GetFMESession()->createString();
    
    for( int iAttr = 0; iAttr < (int)poAttrNames->entries(); iAttr++ )
    {
        const char       *pszAttrName = (*poAttrNames)(iAttr);

/* -------------------------------------------------------------------- */
/*      Get the attribute value.                                        */
/* -------------------------------------------------------------------- */
        if( !poSchemaFeature->getAttribute(pszAttrName,*poAttrValue) )
            continue;

/* -------------------------------------------------------------------- */
/*      Handle geometry attributes.  Use them to try and establish      */
/*      the geometry type of this layer.  If we get conflicting         */
/*      geometries just fall back to the generic geometry type.         */
/* -------------------------------------------------------------------- */
        if( EQUALN(pszAttrName,"fme_geometry",12) )
        {
            OGRwkbGeometryType eAttrGeomType = wkbNone;

            if( EQUAL(poAttrValue->data(),"fme_point") )
                eAttrGeomType = wkbPoint;
            else if( EQUAL(poAttrValue->data(),"fme_text") )
                eAttrGeomType = wkbPoint;
            else if( EQUAL(poAttrValue->data(),"fme_area") )
                eAttrGeomType = wkbPolygon;
            else if( EQUAL(poAttrValue->data(),"fme_polygon") )
                eAttrGeomType = wkbPolygon;
            else if( EQUAL(poAttrValue->data(),"fme_rectangle") )
                eAttrGeomType = wkbPolygon;
            else if( EQUAL(poAttrValue->data(),"fme_rounded_rectangle") )
                eAttrGeomType = wkbPolygon;
            else if( EQUAL(poAttrValue->data(),"fme_line") )
                eAttrGeomType = wkbLineString;
            else if( EQUAL(poAttrValue->data(),"fme_arc") )
                eAttrGeomType = wkbLineString;
            else if( EQUAL(poAttrValue->data(),"fme_aggregate") )
                eAttrGeomType = wkbGeometryCollection;
            else if( EQUAL(poAttrValue->data(),"fme_no_geom") )
                eAttrGeomType = wkbNone;
            else
            {
                CPLDebug( "FME_OLEDB", 
                          "geometry field %s has unknown value %s, ignored.",
                          pszAttrName, 
                          poAttrValue->data() );
                continue;
            }

            if( eGeomType == wkbNone )
                eGeomType = eAttrGeomType;
            else if( eGeomType != eAttrGeomType )
                eGeomType = wkbUnknown;
        }

/* -------------------------------------------------------------------- */
/*      Skip '*' attributes which appear to be the raw attribute        */
/*      names from the source reader.  The versions that don't start    */
/*      with * appear to be massaged suitably for use, with fme         */
/*      standard data types.                                            */
/* -------------------------------------------------------------------- */
        if( EQUALN(pszAttrName,"fme_geometry",12) 
            || pszAttrName[0] == '*'
            || EQUALN(pszAttrName,"fme_geomattr",12) )
        {
            continue;
        }

/* -------------------------------------------------------------------- */
/*      Parse the type into tokens for easier use.                      */
/* -------------------------------------------------------------------- */
        char      **papszTokens;

        papszTokens = CSLTokenizeStringComplex( poAttrValue->data(),
                                                "(,", FALSE, FALSE );

/* -------------------------------------------------------------------- */
/*      Establish new fields.                                           */
/* -------------------------------------------------------------------- */
        OGRFieldType      eType;
        int               nWidth, nPrecision = 0;

        if( CSLCount(papszTokens) == 2 && EQUAL(papszTokens[0],"fme_char") )
        {
            eType = OFTString;
            nWidth = atoi(papszTokens[1]);
        }
        else if( CSLCount(papszTokens) == 3 
                 && EQUAL(papszTokens[0],"fme_decimal") )
        {
            nWidth = atoi(papszTokens[1]);
            nPrecision = atoi(papszTokens[2]);
            if( nPrecision == 0 )
                eType = OFTInteger;
            else
                eType = OFTReal;
        }
        else if( CSLCount(papszTokens) == 1
                 && EQUAL(papszTokens[0],"fme_int16") )
        {
            nWidth = 6;
            nPrecision = 0;
            eType = OFTInteger;
        }
        else if( CSLCount(papszTokens) == 1
                 && EQUAL(papszTokens[0],"fme_int32") )
        {
            nWidth = 0;
            nPrecision = 0;
            eType = OFTInteger;
        }
        else if( CSLCount(papszTokens) == 1
                 && (EQUAL(papszTokens[0],"fme_real32") 
                     || EQUAL(papszTokens[0],"fme_real64")) )
        {
            nWidth = 0;
            nPrecision = 0;
            eType = OFTReal;
        }
        else if( CSLCount(papszTokens) == 1
                 && EQUAL(papszTokens[0],"fme_boolean") )
        {
            nWidth = 1;
            nPrecision = 0;
            eType = OFTInteger;
        }
        else
        {
            printf( "Not able to translate field type: %s\n", 
                    poAttrValue->data() );
            CSLDestroy( papszTokens );
            continue;
        }

/* -------------------------------------------------------------------- */
/*      Add the field to the feature definition.                        */
/* -------------------------------------------------------------------- */
        OGRFieldDefn  oFieldDefn( pszAttrName, eType );
        
        oFieldDefn.SetWidth( nWidth );
        oFieldDefn.SetPrecision( nPrecision );

        poFeatureDefn->AddFieldDefn( &oFieldDefn );

        CSLDestroy( papszTokens );
    }

/* -------------------------------------------------------------------- */
/*      Assign the geometry type ... try to apply 3D-ness as well.      */
/* -------------------------------------------------------------------- */
    if( poSchemaFeature->getDimension() == FME_THREE_D )
        eGeomType = wkbSetZ(eGeomType);

    poFeatureDefn->SetGeomType( eGeomType );

/* -------------------------------------------------------------------- */
/*      Translate the spatial reference system.                         */
/* -------------------------------------------------------------------- */
    if( poSchemaFeature->getCoordSys() != NULL 
        && strlen(poSchemaFeature->getCoordSys()) > 0
        && poSpatialRef == NULL )
    {
        CPLDebug( "FME_OLEDB", "Layer %s has COORDSYS=%s on schema feature.",
                  poFeatureDefn->GetName(), 
                  poSchemaFeature->getCoordSys() );
        poSpatialRef = poDS->FME2OGRSpatialRef(poSchemaFeature->getCoordSys());
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    poDS->GetFMESession()->destroyString( poAttrValue );
    poDS->GetFMESession()->destroyStringArray( poAttrNames );

    return TRUE;
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRFMELayer::SetAttributeFilter( const char *pszNewFilter )

{
    OGRErr      eErr;

    CPLFree( pszAttributeFilter );
    pszAttributeFilter = NULL;

/* -------------------------------------------------------------------- */
/*      Allow clearing of attribute query.                              */
/* -------------------------------------------------------------------- */
    if( pszNewFilter == NULL || strlen(pszNewFilter) == 0 )
    {
        if( m_poAttrQuery != NULL )
        {
            delete m_poAttrQuery;
            m_poAttrQuery = NULL;
        }
        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Compile new query.  Note that we currently will only accept     */
/*      queries we recognise as valid.  It would be better to pass      */
/*      directly through to Oracle or other databases where we will     */
/*      use the setConstraints method and let them decide on the        */
/*      validity of the query.  However, it will be difficult to        */
/*      return the error at this point if we defer setting the          */
/*      constraint.                                                     */
/* -------------------------------------------------------------------- */
    if( !m_poAttrQuery )
        m_poAttrQuery = new OGRFeatureQuery();

    eErr = m_poAttrQuery->Compile( GetLayerDefn(), pszNewFilter );
    if( eErr != OGRERR_NONE )
    {
        delete m_poAttrQuery;
        m_poAttrQuery = NULL;
    }
    else
    {
        pszAttributeFilter = CPLStrdup( pszNewFilter );
    }

    ResetReading();
    return eErr;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRFMELayer::GetSpatialRef()

{
    return poSpatialRef;
}
