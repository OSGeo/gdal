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
    "central_meridian",
    "scale_factor",
    "standard_parallel_1",
    "standard_parallel_2",
    "longitude_of_center",
    "latitude_of_center",
    "latitude_of_origin",
    "false_easting",
    "false_northing",
    "azimuth",
    "longitude_of_point_1",
    "latitude_of_point_1",
    "longitude_of_point_2",
    "latitude_of_point_2",
    "longitude_of_point_3",
    "latitude_of_point_3",
    "landsat_number",
    "path_number",
    "perspective_point_height",
    "fipszone",
    "zone",
    NULL
};

// the following projection lists are incomplete.  they will likely
// change after the CT RPF response.  Examples show alternate forms with
// underscores instead of spaces.  Should we use the EPSG names were available?
// Plate-Caree has an accent in the spec!

static char *papszProjectionSupported[] =
{
    "Cylindrical equal area",
    "Equirectangular",
    "Gauss-Kruger",
    "Mercator",
    "Miller cylindrical",
    "Oblique Mercator (Hotine)",
    "Transverse Mercator",
    "Lambert conformal conic",
    "Polyconic",
    "Orthographic",
    "Polar Stereographic",
    "Stereographic",
    "Robinson",
    NULL
};

static char *papszProjectionUnsupported[] =
{
    "Behrmann",
    "Cassini",
    "Gall's stereographic",
    "Times",
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
