/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRSpatialReference class.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
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
 * Revision 1.4  1999/07/29 17:29:45  warmerda
 * added various help methods for projections
 *
 * Revision 1.3  1999/07/14 05:23:38  warmerda
 * Added projection set methods, and #defined tokens
 *
 * Revision 1.2  1999/07/05 17:19:26  warmerda
 * initial implementation
 *
 * Revision 1.1  1999/06/25 20:20:54  warmerda
 * New
 *
 */

#include "ogr_spatialref.h"
#include "ogr_geometry.h"
#include "ogr_p.h"

/* why would fipszone and zone be paramers when they relate to a composite
   projection which renders done into a non-zoned projection? */

static char *papszParameters[] =
{
    SRS_PP_CENTRAL_MERIDIAN,
    SRS_PP_SCALE_FACTOR,
    SRS_PP_STANDARD_PARALLEL_1,
    SRS_PP_STANDARD_PARALLEL_2,
    SRS_PP_LONGITUDE_OF_CENTER,
    SRS_PP_LATITUDE_OF_CENTER,
    SRS_PP_LONGITUDE_OF_ORIGIN,
    SRS_PP_LATITUDE_OF_ORIGIN,
    SRS_PP_FALSE_EASTING,
    SRS_PP_FALSE_NORTHING,
    SRS_PP_AZIMUTH,
    SRS_PP_LONGITUDE_OF_POINT_1,
    SRS_PP_LATITUDE_OF_POINT_1,
    SRS_PP_LONGITUDE_OF_POINT_2,
    SRS_PP_LATITUDE_OF_POINT_2,
    SRS_PP_LONGITUDE_OF_POINT_3,
    SRS_PP_LATITUDE_OF_POINT_3,
    SRS_PP_LANDSAT_NUMBER,
    SRS_PP_PATH_NUMBER,
    SRS_PP_PERSPECTIVE_POINT_HEIGHT,
    SRS_PP_FIPSZONE,
    SRS_PP_ZONE,
    NULL
};

// the following projection lists are incomplete.  they will likely
// change after the CT RPF response.  Examples show alternate forms with
// underscores instead of spaces.  Should we use the EPSG names were available?
// Plate-Caree has an accent in the spec!

static char *papszProjectionSupported[] =
{
    SRS_PT_CASSINI_SOLDNER,
    SRS_PT_EQUIDISTANT_CONIC,
    SRS_PT_EQUIRECTANGULAR,
    SRS_PT_MERCATOR_1SP,
    SRS_PT_MERCATOR_2SP,
    SRS_PT_ROBINSON,
    SRS_PT_ALBERS_CONIC_EQUAL_AREA,
    SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP,
    SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP,
    SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM,
    SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA,
    SRS_PT_TRANSVERSE_MERCATOR,
    SRS_PT_OBLIQUE_STEREOGRAPHIC,
    SRS_PT_POLAR_STEREOGRAPHIC,
    SRS_PT_HOTINE_OBLIQUE_MERCATOR,
    SRS_PT_LABORDE_OBLIQUE_MERCATOR,
    SRS_PT_SWISS_OBLIQUE_CYLINDRICAL,
    SRS_PT_AZIMUTHAL_EQUIDISTANT,
    SRS_PT_MILLER_CYLINDRICAL,
    SRS_PT_SINUSOIDAL,
    SRS_PT_STEREOGRAPHIC,
    SRS_PT_GNOMONIC,
    SRS_PT_ORTHOGRAPHIC,
    SRS_PT_POLYCONIC,
    SRS_PT_VANDERGRINTEN,
    NULL
};

static char *papszProjectionUnsupported[] =
{
    SRS_PT_NEW_ZEALAND_MAP_GRID,
    SRS_PT_TRANSVERSE_MERCATOR_SOUTH_ORIENTED,
    SRS_PT_TUNISIA_MINING_GRID,
    NULL
};



/************************************************************************/
/*                        OGRSpatialReference()                         */
/************************************************************************/

/**
 * Constructor.
 *
 * This constructor takes an optional string argument which if passed
 * should be a WKT representation of an SRS.  Passing this is equivelent
 * to not passing it, and then calling importFromWkt() with the WKT string.
 *
 * Note that newly created objects are given a reference count of one. 
 */

OGRSpatialReference::OGRSpatialReference( const char * pszWKT )

{
    nRefCount = 1;
    poRoot = NULL;

    if( pszWKT != NULL )
        importFromWkt( (char **) &pszWKT );
}

/************************************************************************/
/*                        ~OGRSpatialReference()                        */
/************************************************************************/

OGRSpatialReference::~OGRSpatialReference()

{
    if( poRoot != NULL )
        delete poRoot;
}

/************************************************************************/
/*                             Reference()                              */
/************************************************************************/

/**
 * Increments the reference count by one.
 *
 * The reference count is used keep track of the number of OGRGeometry objects
 * referencing this SRS.
 *
 * @return the updated reference count.
 */

int OGRSpatialReference::Reference()

{
    return ++nRefCount;
}

/************************************************************************/
/*                            Dereference()                             */
/************************************************************************/

/**
 * Decrements the reference count by one.
 *
 * @return the updated reference count.
 */

int OGRSpatialReference::Dereference()

{
    return --nRefCount;
}

/************************************************************************/
/*                         GetReferenceCount()                          */
/************************************************************************/

/**
 * \fn int OGRSpatialReference::GetReferenceCount();
 *
 * Fetch current reference count.
 *
 * @return the current reference count.
 */

/************************************************************************/
/*                              SetRoot()                               */
/************************************************************************/

/**
 * Set the root SRS node.
 *
 * If the object has an existing tree of OGR_SRSNodes, they are destroyed
 * as part of assigning the new root.  Ownership of the passed OGR_SRSNode is
 * is assumed by the OGRSpatialReference.
 *
 * @param poNewRoot object to assign as root.
 */

void OGRSpatialReference::SetRoot( OGR_SRSNode * poNewRoot )

{
    if( poRoot != NULL )
        delete poRoot;

    poRoot = poNewRoot;
}

/************************************************************************/
/*                            GetAttrNode()                             */
/************************************************************************/

/**
 * Find named node in tree.
 *
 * This method does a pre-order traversal of the node tree searching for
 * a node with this exact value (case insensitive), and returns it.  Leaf
 * nodes are not considered, under the assumption that they are just
 * attribute value nodes.
 *
 * If a node appears more than once in the tree (such as UNIT for instance),
 * the first encountered will be returned.  Use GetNode() on a subtree to be
 * more specific. 
 *
 * @param pszNodeName the name of the node to search for.
 *
 * @return a pointer to the node found, or NULL if none.
 */

OGR_SRSNode *OGRSpatialReference::GetAttrNode( const char * pszNodeName )

{
    if( poRoot == NULL )
        return NULL;

    return poRoot->GetNode( pszNodeName );
}

/************************************************************************/
/*                            GetAttrValue()                            */
/************************************************************************/

/**
 * Fetch indicated attribute of named node.
 *
 * This method uses GetAttrNode() to find the named node, and then extracts
 * the value of the indicated child.  Thus a call to GetAttrValue("UNIT",1)
 * would return the second child of the UNIT node, which is normally the
 * length of the linear unit in meters.
 *
 * @param pszNodeName the tree node to look for (case insensitive).
 * @param iAttr the child of the node to fetch (zero based).
 *
 * @return the requested value, or NULL if it fails for any reason. 
 */

const char *OGRSpatialReference::GetAttrValue( const char * pszNodeName,
                                               int iAttr )

{
    OGR_SRSNode	*poNode;

    poNode = GetAttrNode( pszNodeName );
    if( poNode == NULL )
        return NULL;

    if( iAttr < 0 || iAttr >= poNode->GetChildCount() )
        return NULL;

    return poNode->GetChild(iAttr)->GetValue();
}

/************************************************************************/
/*                              Validate()                              */
/************************************************************************/

/**
 * Validate SRS tokens.
 *
 * This method attempts to verify that the spatial reference system is
 * well formed, and consists of known tokens.  The validation is not
 * comprehensive. 
 *
 * @return OGRERR_NONE if all is fine, OGRERR_CORRUPT_DATA if the SRS is
 * not well formed, and OGRERR_UNSUPPORTED_SRS if the SRS is well formed,
 * but contains non-standard PROJECTION[] values.
 */

OGRErr OGRSpatialReference::Validate()

{
/* -------------------------------------------------------------------- */
/*      Validate root node.                                             */
/* -------------------------------------------------------------------- */
    if( poRoot == NULL )
    {
        CPLDebug( "OGRSpatialReference::Validate",
                  "No root pointer.\n" );
        return OGRERR_CORRUPT_DATA;
    }
    
    if( !EQUAL(poRoot->GetValue(),"GEOGCS")
        && !EQUAL(poRoot->GetValue(),"PROJCS")
        && !EQUAL(poRoot->GetValue(),"GEOCCS") )
    {
        CPLDebug( "OGRSpatialReference::Validate",
                  "Unrecognised root node `%s'\n",
                  poRoot->GetValue() );
        return OGRERR_CORRUPT_DATA;
    }
    
/* -------------------------------------------------------------------- */
/*      For a PROJCS, validate subparameters (other than GEOGCS).       */
/* -------------------------------------------------------------------- */
    if( EQUAL(poRoot->GetValue(),"PROJCS") )
    {
        OGR_SRSNode	*poNode;
        int		i;

        for( i = 1; i < poRoot->GetChildCount(); i++ )
        {
            poNode = poRoot->GetChild(i);

            if( EQUAL(poNode->GetValue(),"GEOGCS") )
            {
                /* validated elsewhere */
            }
            else if( EQUAL(poNode->GetValue(),"UNIT") )
            {
                if( poNode->GetChildCount() != 2 )
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                           "UNIT has wrong number of children (%d), not 2.\n",
                              poNode->GetChildCount() );
                    return OGRERR_CORRUPT_DATA;
                }
                else if( atof(poNode->GetChild(1)->GetValue()) == 0.0 )
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                              "UNIT does not appear to have meaningful"
                              "coefficient (%s).\n",
                              poNode->GetChild(1)->GetValue() );
                    return OGRERR_CORRUPT_DATA;
                }
            }
            else if( EQUAL(poNode->GetValue(),"PARAMETER") )
            {
                if( poNode->GetChildCount() != 2 )
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                              "PARAMETER has wrong number of children (%d),"
                              "not 2 as expected.\n",
                              poNode->GetChildCount() );
                    
                    return OGRERR_CORRUPT_DATA;
                }
                else if( CSLFindString( papszParameters,
                                        poNode->GetChild(0)->GetValue()) == -1)
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                              "Unrecognised PARAMETER `%s'.\n",
                              poNode->GetChild(0)->GetValue() );
                    
                    return OGRERR_UNSUPPORTED_SRS;
                }
            }
            else if( EQUAL(poNode->GetValue(),"PROJECTION") )
            {
                if( poNode->GetChildCount() != 1 )
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                              "PROJECTION has wrong number of children (%d),"
                              "not 1 as expected.\n",
                              poNode->GetChildCount() );
                    
                    return OGRERR_CORRUPT_DATA;
                }
                else if( CSLFindString( papszProjectionSupported,
                                        poNode->GetChild(0)->GetValue()) == -1
                      && CSLFindString( papszProjectionUnsupported,
                                        poNode->GetChild(0)->GetValue()) == -1)
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                              "Unrecognised PROJECTION `%s'.\n",
                              poNode->GetChild(0)->GetValue() );
                    
                    return OGRERR_UNSUPPORTED_SRS;
                }
                else if( CSLFindString( papszProjectionSupported,
                                        poNode->GetChild(0)->GetValue()) == -1)
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                              "Unsupported, but recognised PROJECTION `%s'.\n",
                              poNode->GetChild(0)->GetValue() );
                    
                    return OGRERR_UNSUPPORTED_SRS;
                }
            }
            else
            {
                CPLDebug( "OGRSpatialReference::Validate",
                          "Unexpected child for PROJCS `%s'.\n",
                          poNode->GetValue() );
                
                return OGRERR_CORRUPT_DATA;
            }
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Validate GEOGCS if found.                                       */
/* -------------------------------------------------------------------- */
    OGR_SRSNode	*poGEOGCS = poRoot->GetNode( "GEOGCS" );

    if( poGEOGCS != NULL )
    {
        OGR_SRSNode	*poNode;
        int		i;

        for( i = 1; i < poRoot->GetChildCount(); i++ )
        {
            poNode = poRoot->GetChild(i);

            if( EQUAL(poNode->GetValue(),"DATUM") )
            {
                /* validated elsewhere */
            }
            else if( EQUAL(poNode->GetValue(),"PRIMEM") )
            {
                if( poNode->GetChildCount() != 2 )
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                              "PRIMEM has wrong number of children (%d),"
                              "not 2 as expected.\n",
                              poNode->GetChildCount() );
                    
                    return OGRERR_CORRUPT_DATA;
                }
            }
            else if( EQUAL(poNode->GetValue(),"UNIT") )
            {
                if( poNode->GetChildCount() != 2 )
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                           "UNIT has wrong number of children (%d), not 2.\n",
                              poNode->GetChildCount() );
                    return OGRERR_CORRUPT_DATA;
                }
                else if( atof(poNode->GetChild(1)->GetValue()) == 0.0 )
                {
                    CPLDebug( "OGRSpatialReference::Validate",
                              "UNIT does not appear to have meaningful"
                              "coefficient (%s).\n",
                              poNode->GetChild(1)->GetValue() );
                    return OGRERR_CORRUPT_DATA;
                }
            }
            else
            {
                CPLDebug( "OGRSpatialReference::Validate",
                          "Unexpected child for GEOGCS `%s'.\n",
                          poNode->GetValue() );
                
                return OGRERR_CORRUPT_DATA;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Validate DATUM/SPHEROID.                                        */
/* -------------------------------------------------------------------- */
    OGR_SRSNode	*poDATUM = poRoot->GetNode( "DATUM" );

    if( poDATUM != NULL )
    {
        OGR_SRSNode	*poSPHEROID;

        if( poDATUM->GetChildCount() != 2 )
        {
            CPLDebug( "OGRSpatialReference::Validate",
                      "DATUM has wrong number of children (%d),"
                      "not 2 as expected.\n",
                      poDATUM->GetChildCount() );
            
            return OGRERR_CORRUPT_DATA;
        }
        else if( !EQUAL(poDATUM->GetChild(1)->GetValue(),"SPHEROID") )
        {
            CPLDebug( "OGRSpatialReference::Validate",
                      "DATUM missing SPHEROID.\n" );
            return OGRERR_CORRUPT_DATA;
        }

        poSPHEROID = poDATUM->GetChild(1);

        if( poSPHEROID->GetChildCount() != 3 )
        {
            CPLDebug( "OGRSpatialReference::Validate",
                      "SPHEROID has wrong number of children (%d),"
                      "not 3 as expected.\n",
                      poSPHEROID->GetChildCount() );
            
            return OGRERR_CORRUPT_DATA;
        }
        else if( atof(poSPHEROID->GetChild(1)->GetValue()) == 0.0 )
        {
            CPLDebug( "OGRSpatialReference::Validate",
                      "SPHEROID semi-major axis is zero (%s)!\n",
                      poSPHEROID->GetChild(1)->GetValue() );
            return OGRERR_CORRUPT_DATA;
        }
    }        

/* -------------------------------------------------------------------- */
/*      Final check.                                                    */
/* -------------------------------------------------------------------- */
    if( EQUAL(poRoot->GetValue(),"GEOCCS") )
        return OGRERR_UNSUPPORTED_SRS;

    return OGRERR_NONE;
}

/************************************************************************/
/*                               Clone()                                */
/************************************************************************/

/**
 * Make a duplicate of this OGRSpatialReference.
 *
 * @return a new SRS, which becomes the responsibility of the caller.
 */

OGRSpatialReference *OGRSpatialReference::Clone()

{
    OGRSpatialReference	*poNewRef;

    poNewRef = new OGRSpatialReference();

    poNewRef->poRoot = poRoot->Clone();

    return poNewRef;
}

/************************************************************************/
/*                            exportToWkt()                             */
/************************************************************************/

/**
 * Convert this SRS into WKT format.
 *
 * Note that the returned WKT string should be freed with OGRFree() or
 * CPLFree() when no longer needed.  It is the responsibility of the caller.
 *
 * @param ppszResult the resulting string is returned in this pointer.
 *
 * @return currently OGRERR_NONE is always returned, but the future it
 * is possible error conditions will develop. 
 */
 
OGRErr	OGRSpatialReference::exportToWkt( char ** ppszResult )

{
    if( poRoot == NULL )
    {
        *ppszResult = CPLStrdup("");
        return OGRERR_NONE;
    }
    else
    {
        return poRoot->exportToWkt(ppszResult);
    }
}

/************************************************************************/
/*                           importFromWkt()                            */
/************************************************************************/

/**
 * Import from WKT string.
 *
 * This method will wipe the existing SRS definition, and
 * reassign it based on the contents of the passed WKT string.  Only as
 * much of the input string as needed to construct this SRS is consumed from
 * the input string, and the input string pointer
 * is then updated to point to the remaining (unused) input.
 *
 * @param ppszInput Pointer to pointer to input.  The pointer is updated to
 * point to remaining unused input text.
 *
 * @return OGRERR_NONE if import succeeds, or OGRERR_CORRUPT_DATA if it
 * fails for any reason.
 */

OGRErr OGRSpatialReference::importFromWkt( char ** ppszInput )

{
    if( poRoot != NULL )
        delete poRoot;

    poRoot = new OGR_SRSNode();

    return poRoot->importFromWkt( ppszInput );
}

/************************************************************************/
/*                              SetNode()                               */
/************************************************************************/

OGRErr OGRSpatialReference::SetNode( const char * pszNodePath,
                                     const char * pszNewNodeValue )

{
    char	**papszPathTokens;
    int		i;
    OGR_SRSNode	*poNode;

    papszPathTokens = CSLTokenizeStringComplex(pszNodePath, "|", TRUE, FALSE);

    if( CSLCount( papszPathTokens ) < 1 )
        return OGRERR_FAILURE;

    if( GetRoot() == NULL || !EQUAL(papszPathTokens[0],GetRoot()->GetValue()) )
    {
        SetRoot( new OGR_SRSNode( papszPathTokens[0] ) );
    }

    poNode = GetRoot();
    for( i = 1; papszPathTokens[i] != NULL; i++ )
    {
        int	j;
        
        for( j = 0; j < poNode->GetChildCount(); j++ )
        {
            if( EQUAL(poNode->GetChild( j )->GetValue(),papszPathTokens[i]) )
            {
                poNode = poNode->GetChild(j);
                break;
            }
        }

        if( j == poNode->GetChildCount() )
        {
            OGR_SRSNode	*poNewNode = new OGR_SRSNode( papszPathTokens[i] );
            poNode->AddChild( poNewNode );
            poNode = poNewNode;
        }
    }

    CSLDestroy( papszPathTokens );

    if( poNode->GetChildCount() > 0 )
        poNode->GetChild(0)->SetValue( pszNewNodeValue );
    else
        poNode->AddChild( new OGR_SRSNode( pszNewNodeValue ) );

    return OGRERR_NONE;
}

/************************************************************************/
/*                              SetNode()                               */
/************************************************************************/

OGRErr OGRSpatialReference::SetNode( const char *pszNodePath,
                                     double dfValue )

{
    char	szValue[64];

    if( ABS(dfValue - (int) dfValue) == 0.0 )
        sprintf( szValue, "%d", (int) dfValue );
    else
        sprintf( szValue, "%.12f", dfValue );

    return SetNode( pszNodePath, szValue );
}

/************************************************************************/
/*                           SetLinearUnits()                           */
/************************************************************************/

OGRErr OGRSpatialReference::SetLinearUnits( const char * pszUnitsName,
                                            double dfInMeters )

{
    OGR_SRSNode	*poPROJCS = GetAttrNode( "PROJCS" );
    OGR_SRSNode *poUnits;
    char	szValue[128];

    if( poPROJCS == NULL )
        return OGRERR_FAILURE;

    if( dfInMeters == (int) dfInMeters )
        sprintf( szValue, "%d", (int) dfInMeters );
    else
        sprintf( szValue, "%.12f", dfInMeters );

    poUnits = new OGR_SRSNode( "UNIT" );
    poUnits->AddChild( new OGR_SRSNode( pszUnitsName ) );
    poUnits->AddChild( new OGR_SRSNode( szValue ) );

    poPROJCS->AddChild( poUnits );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           GetLinearUnits()                           */
/************************************************************************/

double OGRSpatialReference::GetLinearUnits( char ** ppszName )

{
    OGR_SRSNode	*poPROJCS = GetAttrNode( "PROJCS" );

    if( ppszName != NULL )
        *ppszName = "unknown";
        
    if( poPROJCS == NULL )
        return 1.0;

    for( int iChild = 0; iChild < poPROJCS->GetChildCount(); iChild++ )
    {
        OGR_SRSNode	*poChild = poPROJCS->GetChild(iChild);
        
        if( EQUAL(poChild->GetValue(),"UNITS")
            && poChild->GetChildCount() == 2 )
        {
            if( ppszName != NULL )
                *ppszName = (char *) poChild->GetChild(0)->GetValue();
            
            return atof( poChild->GetChild(1)->GetValue() );
        }
    }

    return 1.0;
}

/************************************************************************/
/*                             SetGeogCS()                              */
/************************************************************************/

OGRErr
OGRSpatialReference::SetGeogCS( const char * pszGeogName,
                                const char * pszDatumName,
                                const char * pszSpheroidName,
                                double dfSemiMajor, double dfInvFlattening,
                                const char * pszPMName, double dfPMOffset )

{

/* -------------------------------------------------------------------- */
/*      Set defaults for various parameters.                            */
/* -------------------------------------------------------------------- */
    if( pszGeogName == NULL )
        pszGeogName = "unnamed";

    if( pszPMName == NULL )
        pszPMName = SRS_PM_GREENWICH;

    if( pszDatumName == NULL )
        pszDatumName = "unknown";

    if( pszSpheroidName == NULL )
        pszSpheroidName = "unnamed";

/* -------------------------------------------------------------------- */
/*      Build the GEOGCS object.                                        */
/* -------------------------------------------------------------------- */
    char	        szValue[128];
    OGR_SRSNode		*poGeogCS, *poSpheroid, *poDatum, *poPM, *poUnits;

    poGeogCS = new OGR_SRSNode( "GEOGCS" );
    poGeogCS->AddChild( new OGR_SRSNode( pszGeogName ) );
    
/* -------------------------------------------------------------------- */
/*      Setup the spheroid.                                             */
/* -------------------------------------------------------------------- */
    poSpheroid = new OGR_SRSNode( "SPHEROID" );
    poSpheroid->AddChild( new OGR_SRSNode( pszSpheroidName ) );

    sprintf( szValue, "%.3f", dfSemiMajor );
    poSpheroid->AddChild( new OGR_SRSNode(szValue) );

    sprintf( szValue, "%.14f", dfInvFlattening );
    poSpheroid->AddChild( new OGR_SRSNode(szValue) );

/* -------------------------------------------------------------------- */
/*      Setup the Datum.                                                */
/* -------------------------------------------------------------------- */
    poDatum = new OGR_SRSNode( "DATUM" );
    poDatum->AddChild( new OGR_SRSNode(pszDatumName) );
    poDatum->AddChild( poSpheroid );

/* -------------------------------------------------------------------- */
/*      Setup the prime meridian.                                       */
/* -------------------------------------------------------------------- */
    if( dfPMOffset == 0.0 )
        strcpy( szValue, "0" );
    else
        sprintf( szValue, "%.16f", dfPMOffset );
    
    poPM = new OGR_SRSNode( "PRIMEM" );
    poPM->AddChild( new OGR_SRSNode( pszPMName ) );
    poPM->AddChild( new OGR_SRSNode( szValue ) );

/* -------------------------------------------------------------------- */
/*      Setup the rotational units.                                     */
/* -------------------------------------------------------------------- */
    poUnits = new OGR_SRSNode( "UNIT" );
    poUnits->AddChild( new OGR_SRSNode(SRS_UA_DEGREE) );
    poUnits->AddChild( new OGR_SRSNode(SRS_UA_DEGREE_CONV) );
    
/* -------------------------------------------------------------------- */
/*      Complete the GeogCS                                             */
/* -------------------------------------------------------------------- */
    poGeogCS->AddChild( poDatum );
    poGeogCS->AddChild( poPM );
    poGeogCS->AddChild( poUnits );

/* -------------------------------------------------------------------- */
/*      Attach below the PROJCS if there is one, or make this the root. */
/* -------------------------------------------------------------------- */
    if( GetRoot() != NULL && EQUAL(GetRoot()->GetValue(),"PROJCS") )
        poRoot->AddChild( poGeogCS );
    else
        SetRoot( poGeogCS );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            GetSemiMajor()                            */
/************************************************************************/

double OGRSpatialReference::GetSemiMajor( OGRErr * pnErr )

{
    OGR_SRSNode	*poSpheroid = GetAttrNode( "SPHEROID" );
    
    if( pnErr != NULL )
        *pnErr = OGRERR_NONE;

    if( poSpheroid != NULL && poSpheroid->GetChildCount() >= 3 )
    {
        return atof( poSpheroid->GetChild(1)->GetValue() );
    }
    else
    {
        if( pnErr != NULL )
            *pnErr = OGRERR_FAILURE;

        return SRS_WGS84_SEMIMAJOR;
    }
}

/************************************************************************/
/*                          GetInvFlattening()                          */
/************************************************************************/

double OGRSpatialReference::GetInvFlattening( OGRErr * pnErr )

{
    OGR_SRSNode	*poSpheroid = GetAttrNode( "SPHEROID" );
    
    if( pnErr != NULL )
        *pnErr = OGRERR_NONE;

    if( poSpheroid != NULL && poSpheroid->GetChildCount() >= 3 )
    {
        return atof( poSpheroid->GetChild(2)->GetValue() );
    }
    else
    {
        if( pnErr != NULL )
            *pnErr = OGRERR_FAILURE;

        return SRS_WGS84_INVFLATTENING;
    }
}

/************************************************************************/
/*                            GetSemiMinor()                            */
/************************************************************************/

double OGRSpatialReference::GetSemiMinor( OGRErr * pnErr )

{
    double	dfInvFlattening, dfSemiMajor;

    dfSemiMajor = GetSemiMajor( pnErr );
    dfInvFlattening = GetInvFlattening( pnErr );

    return dfSemiMajor * (1.0 - 1.0/dfInvFlattening);
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

OGRErr OGRSpatialReference::SetProjection( const char * pszProjection )

{
    if( !GetAttrNode( "PROJCS" ) )
    {
        SetNode( "PROJCS", "unnamed" );
    }
    
    return SetNode( "PROJCS|PROJECTION", pszProjection );
}

/************************************************************************/
/*                            SetProjParm()                             */
/************************************************************************/

OGRErr OGRSpatialReference::SetProjParm( const char * pszParmName,
                                         double dfValue )

{
    OGR_SRSNode	*poPROJCS = GetAttrNode( "PROJCS" );
    OGR_SRSNode *poParm;
    char	szValue[64];

    if( poPROJCS == NULL || GetAttrNode( pszParmName ) != NULL )
        return OGRERR_FAILURE;

    poParm = new OGR_SRSNode( "PARAMETER" );
    poParm->AddChild( new OGR_SRSNode( pszParmName ) );

    if( ABS(dfValue - (int) dfValue) == 0.0 )
        sprintf( szValue, "%d", (int) dfValue );
    else
        sprintf( szValue, "%.12f", dfValue );

    poParm->AddChild( new OGR_SRSNode( szValue ) );

    poPROJCS->AddChild( poParm );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            GetProjParm()                             */
/*                                                                      */
/*      This should be modified to translate non degree angles into     */
/*      degrees based on the GEOGCS unit.  Note that Cadcorp            */
/*      examples include use of "DDD.MMSSsss".                          */
/************************************************************************/

double OGRSpatialReference::GetProjParm( const char * pszName,
                                         double dfDefaultValue,
                                         OGRErr *pnErr )

{
    OGR_SRSNode	*poPROJCS = GetAttrNode( "PROJCS" );
    OGR_SRSNode *poParameter = NULL;

    if( pnErr != NULL )
        *pnErr = OGRERR_NONE;
    
/* -------------------------------------------------------------------- */
/*      Search for requested parameter.                                 */
/* -------------------------------------------------------------------- */
    if( poPROJCS != NULL )
    {
        for( int iChild = 0; iChild < poPROJCS->GetChildCount(); iChild++ )
        {
            poParameter = poPROJCS->GetChild(iChild);
            
            if( EQUAL(poParameter->GetValue(),"PARAMETER")
                && poParameter->GetChildCount() == 2 
                && EQUAL(poPROJCS->GetChild(iChild)->GetChild(0)->GetValue(),
                         pszName) )
            {
                return atof(poParameter->GetChild(1)->GetValue());
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Try similar names, for selected parameters.                     */
/* -------------------------------------------------------------------- */
    double	dfValue;
    OGRErr	nSubErr;
    
    if( EQUAL(pszName,SRS_PP_LATITUDE_OF_ORIGIN) )
    {
        dfValue = GetProjParm(SRS_PP_LATITUDE_OF_CENTER,0.0,&nSubErr);
        
        if( nSubErr == OGRERR_NONE )
            return dfValue;
    }
    else if( EQUAL(pszName,SRS_PP_CENTRAL_MERIDIAN) )
    {
        dfValue = GetProjParm(SRS_PP_LONGITUDE_OF_CENTER,0.0,&nSubErr);
        if( nSubErr != OGRERR_NONE )
            dfValue = GetProjParm(SRS_PP_LONGITUDE_OF_ORIGIN,0.0,&nSubErr);

        if( nSubErr == OGRERR_NONE )
            return dfValue;
    }
    
/* -------------------------------------------------------------------- */
/*      Return default value on failure.                                */
/* -------------------------------------------------------------------- */
    if( pnErr != NULL )
        *pnErr = OGRERR_FAILURE;

    return dfDefaultValue;
}

/************************************************************************/
/*                               SetTM()                                */
/************************************************************************/

OGRErr OGRSpatialReference::SetTM( double dfCenterLat, double dfCenterLong,
                                   double dfScale,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    SetProjection( SRS_PT_TRANSVERSE_MERCATOR );
    SetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetProjParm( SRS_PP_SCALE_FACTOR, dfScale );
    SetProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                              SetTMSO()                               */
/************************************************************************/

OGRErr OGRSpatialReference::SetTMSO( double dfCenterLat, double dfCenterLong,
                                     double dfScale,
                                     double dfFalseEasting,
                                     double dfFalseNorthing )

{
    SetProjection( SRS_PT_TRANSVERSE_MERCATOR_SOUTH_ORIENTED );
    SetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetProjParm( SRS_PP_SCALE_FACTOR, dfScale );
    SetProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                              SetACEA()                               */
/************************************************************************/

OGRErr OGRSpatialReference::SetACEA( double dfStdP1, double dfStdP2,
                                     double dfCenterLat, double dfCenterLong,
                                     double dfFalseEasting,
                                     double dfFalseNorthing )

{
    SetProjection( SRS_PT_ALBERS_CONIC_EQUAL_AREA );
    SetProjParm( SRS_PP_STANDARD_PARALLEL_1, dfStdP1 );
    SetProjParm( SRS_PP_STANDARD_PARALLEL_2, dfStdP2 );
    SetProjParm( SRS_PP_LATITUDE_OF_CENTER, dfCenterLat );
    SetProjParm( SRS_PP_LONGITUDE_OF_CENTER, dfCenterLong );
    SetProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                               SetAE()                                */
/************************************************************************/

OGRErr OGRSpatialReference::SetAE( double dfCenterLat, double dfCenterLong,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    SetProjection( SRS_PT_AZIMUTHAL_EQUIDISTANT );
    SetProjParm( SRS_PP_LATITUDE_OF_CENTER, dfCenterLat );
    SetProjParm( SRS_PP_LONGITUDE_OF_CENTER, dfCenterLong );
    SetProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                               SetCS()                                */
/************************************************************************/

OGRErr OGRSpatialReference::SetCS( double dfCenterLat, double dfCenterLong,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    SetProjection( SRS_PT_CASSINI_SOLDNER );
    SetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                               SetEC()                                */
/************************************************************************/

OGRErr OGRSpatialReference::SetEC( double dfStdP1, double dfStdP2,
                                   double dfCenterLat, double dfCenterLong,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    SetProjection( SRS_PT_EQUIDISTANT_CONIC );
    SetProjParm( SRS_PP_STANDARD_PARALLEL_1, dfStdP1 );
    SetProjParm( SRS_PP_STANDARD_PARALLEL_2, dfStdP2 );
    SetProjParm( SRS_PP_LATITUDE_OF_CENTER, dfCenterLat );
    SetProjParm( SRS_PP_LONGITUDE_OF_CENTER, dfCenterLong );
    SetProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                         SetEquirectangular()                         */
/************************************************************************/

OGRErr OGRSpatialReference::SetEquirectangular(
    				   double dfCenterLat, double dfCenterLong,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    SetProjection( SRS_PT_EQUIRECTANGULAR );
    SetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            SetGnomonic()                             */
/************************************************************************/

OGRErr OGRSpatialReference::SetGnomonic(
    				   double dfCenterLat, double dfCenterLong,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    SetProjection( SRS_PT_GNOMONIC );
    SetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                               SetHOM()                               */
/************************************************************************/

OGRErr OGRSpatialReference::SetHOM( double dfCenterLat, double dfCenterLong,
                                    double dfAzimuth, double dfRectToSkew,
                                    double dfScale,
                                    double dfFalseEasting,
                                    double dfFalseNorthing )

{
    SetProjection( SRS_PT_HOTINE_OBLIQUE_MERCATOR );
    SetProjParm( SRS_PP_LATITUDE_OF_CENTER, dfCenterLat );
    SetProjParm( SRS_PP_LONGITUDE_OF_CENTER, dfCenterLong );
    SetProjParm( SRS_PP_AZIMUTH, dfAzimuth );
    SetProjParm( SRS_PP_RECTIFIED_GRID_ANGLE, dfRectToSkew );
    SetProjParm( SRS_PP_SCALE_FACTOR, dfScale );
    SetProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                              SetLAEA()                               */
/************************************************************************/

OGRErr OGRSpatialReference::SetLAEA( double dfCenterLat, double dfCenterLong,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    SetProjection( SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA );
    SetProjParm( SRS_PP_LATITUDE_OF_CENTER, dfCenterLat );
    SetProjParm( SRS_PP_LONGITUDE_OF_CENTER, dfCenterLong );
    SetProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                               SetLCC()                               */
/************************************************************************/

OGRErr OGRSpatialReference::SetLCC( double dfStdP1, double dfStdP2,
                                    double dfCenterLat, double dfCenterLong,
                                    double dfFalseEasting,
                                    double dfFalseNorthing )

{
    SetProjection( SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP );
    SetProjParm( SRS_PP_STANDARD_PARALLEL_1, dfStdP1 );
    SetProjParm( SRS_PP_STANDARD_PARALLEL_2, dfStdP2 );
    SetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                              SetLCCB()                               */
/************************************************************************/

OGRErr OGRSpatialReference::SetLCCB( double dfStdP1, double dfStdP2,
                                     double dfCenterLat, double dfCenterLong,
                                     double dfFalseEasting,
                                     double dfFalseNorthing )

{
    SetProjection( SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM );
    SetProjParm( SRS_PP_STANDARD_PARALLEL_1, dfStdP1 );
    SetProjParm( SRS_PP_STANDARD_PARALLEL_2, dfStdP2 );
    SetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                               SetMC()                                */
/************************************************************************/

OGRErr OGRSpatialReference::SetMC( double dfCenterLat, double dfCenterLong,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    SetProjection( SRS_PT_MILLER_CYLINDRICAL );
    SetProjParm( SRS_PP_LATITUDE_OF_CENTER, dfCenterLat );
    SetProjParm( SRS_PP_LONGITUDE_OF_CENTER, dfCenterLong );
    SetProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            SetMercator()                             */
/************************************************************************/

OGRErr OGRSpatialReference::SetMercator( double dfCenterLat, double dfCenterLong,
                                         double dfScale,
                                         double dfFalseEasting,
                                         double dfFalseNorthing )

{
    SetProjection( SRS_PT_MERCATOR_1SP );
    SetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetProjParm( SRS_PP_SCALE_FACTOR, dfScale );
    SetProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                              SetNZMG()                               */
/************************************************************************/

OGRErr OGRSpatialReference::SetNZMG( double dfCenterLat, double dfCenterLong,
                                     double dfFalseEasting,
                                     double dfFalseNorthing )

{
    SetProjection( SRS_PT_NEW_ZEALAND_MAP_GRID );
    SetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                               SetOS()                                */
/************************************************************************/

OGRErr OGRSpatialReference::SetOS( double dfOriginLat, double dfCMeridian,
                                   double dfScale,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    SetProjection( SRS_PT_OBLIQUE_STEREOGRAPHIC );
    SetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfOriginLat );
    SetProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCMeridian );
    SetProjParm( SRS_PP_SCALE_FACTOR, dfScale );
    SetProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                          SetOrthographic()                           */
/************************************************************************/

OGRErr OGRSpatialReference::SetOrthographic(
    				double dfCenterLat, double dfCenterLong,
                                double dfFalseEasting, double dfFalseNorthing )

{
    SetProjection( SRS_PT_ORTHOGRAPHIC );
    SetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            SetPolyconic()                            */
/************************************************************************/

OGRErr OGRSpatialReference::SetPolyconic(
    				double dfCenterLat, double dfCenterLong,
                                double dfFalseEasting, double dfFalseNorthing )

{
    SetProjection( SRS_PT_POLYCONIC );
    SetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                               SetPS()                                */
/************************************************************************/

OGRErr OGRSpatialReference::SetPS(
    				double dfCenterLat, double dfCenterLong,
                                double dfScale,
                                double dfFalseEasting, double dfFalseNorthing )

{
    SetProjection( SRS_PT_POLAR_STEREOGRAPHIC );
    SetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetProjParm( SRS_PP_SCALE_FACTOR, dfScale );
    SetProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            SetRobinson()                             */
/************************************************************************/

OGRErr OGRSpatialReference::SetRobinson( double dfCenterLong,
                                         double dfFalseEasting,
                                         double dfFalseNorthing )

{
    SetProjection( SRS_PT_ROBINSON );
    SetProjParm( SRS_PP_LONGITUDE_OF_CENTER, dfCenterLong );
    SetProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           SetSinusoidal()                            */
/************************************************************************/

OGRErr OGRSpatialReference::SetSinusoidal( double dfCenterLong,
                                           double dfFalseEasting,
                                           double dfFalseNorthing )

{
    SetProjection( SRS_PT_SINUSOIDAL );
    SetProjParm( SRS_PP_LONGITUDE_OF_CENTER, dfCenterLong );
    SetProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                          SetStereographic()                          */
/************************************************************************/

OGRErr OGRSpatialReference::SetStereographic(
			    double dfOriginLat, double dfCMeridian,
			    double dfScale,
                            double dfFalseEasting,
                            double dfFalseNorthing )

{
    SetProjection( SRS_PT_STEREOGRAPHIC );
    SetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfOriginLat );
    SetProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCMeridian );
    SetProjParm( SRS_PP_SCALE_FACTOR, dfScale );
    SetProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                               SetVDG()                               */
/************************************************************************/

OGRErr OGRSpatialReference::SetVDG( double dfCMeridian,
                                    double dfFalseEasting,
                                    double dfFalseNorthing )

{
    SetProjection( SRS_PT_VANDERGRINTEN );
    SetProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCMeridian );
    SetProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}


