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
 * All Rights Reserved
 *
 * This software may not be copied or reproduced, in all or in part, 
 * without the prior written consent of Safe Software Inc.
 *
 * The entire risk as to the results and performance of the software,
 * supporting text and other information contained in this file
 * (collectively called the "Software") is with the user.  Although
 * Safe Software Incorporated has used considerable efforts in preparing 
 * the Software, Safe Software Incorporated does not warrant the
 * accuracy or completeness of the Software. In no event will Safe Software 
 * Incorporated be liable for damages, including loss of profits or 
 * consequential damages, arising out of the use of the Software.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.5  2005/09/21 01:00:22  fwarmerdam
 * fixup OGRFeatureDefn and OGRSpatialReference refcount handling
 *
 * Revision 1.4  2005/02/22 12:57:19  fwarmerdam
 * use OGRLayer base spatial filter support
 *
 * Revision 1.3  2005/02/02 20:54:27  fwarmerdam
 * track m_nFeaturesRead
 *
 * Revision 1.2  2002/10/29 03:28:34  warmerda
 * fixed 2.5D flag value
 *
 * Revision 1.1  2002/05/24 06:23:57  warmerda
 * New
 *
 * Revision 1.13  2002/05/24 06:17:01  warmerda
 * clean up dependencies on strimp.h, and fme2ogrspatialref func
 *
 * Revision 1.12  2002/05/06 14:06:37  warmerda
 * override coordsys from cached features if needed
 *
 * Revision 1.11  2002/04/25 21:29:03  warmerda
 * removed noisy debug statement
 *
 * Revision 1.10  2002/04/10 20:10:46  warmerda
 * filled out geometry type mappings
 *
 * Revision 1.9  2002/04/08 14:26:33  warmerda
 * add support for harvesting the geometry type from schema
 *
 * Revision 1.8  2001/11/26 18:37:34  warmerda
 * filter out fme_geomattr attributes
 *
 * Revision 1.7  2001/11/21 15:45:25  warmerda
 * allow SRS to be passed in
 *
 * Revision 1.6  2001/11/19 22:11:51  warmerda
 * avoid leaking string, or spatialref
 *
 * Revision 1.5  2001/09/07 15:54:14  warmerda
 * now just a subclass of DB and Cached specific implementations
 *
 * Revision 1.4  2001/07/27 17:27:06  warmerda
 * added CVSID
 *
 * Revision 1.3  2001/07/27 17:24:45  warmerda
 * First phase rewrite for MapGuide
 *
 * Revision 1.2  1999/11/23 15:39:51  warmerda
 * tab expantion
 *
 * Revision 1.1  1999/11/23 15:22:58  warmerda
 * New
 *
 * Revision 1.4  1999/11/22 22:10:49  warmerda
 * added GetSpatialRef(), and fme_real64 support
 *
 * Revision 1.3  1999/11/10 14:04:44  warmerda
 * updated to new fmeobjects kit
 *
 * Revision 1.2  1999/09/09 21:05:34  warmerda
 * further fleshed out
 *
 * Revision 1.1  1999/09/09 20:40:56  warmerda
 * New
 */

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
        eGeomType = (OGRwkbGeometryType) (((int)eGeomType) | wkb25DBit);

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
