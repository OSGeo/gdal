/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRSpatialReference class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 * Revision 1.36  2001/09/21 16:29:21  warmerda
 * fixed typos in docs
 *
 * Revision 1.35  2001/09/21 16:21:02  warmerda
 * added Clear(), and SetFromUserInput() methods
 *
 * Revision 1.34  2001/09/11 19:04:59  warmerda
 * fixed logic in GetLinearUnits() to work on nodes with authority defn
 *
 * Revision 1.33  2001/08/13 11:23:58  warmerda
 * improved IsSame() test
 *
 * Revision 1.32  2001/07/19 18:25:07  warmerda
 * expanded tabs
 *
 * Revision 1.31  2001/07/18 05:03:05  warmerda
 * added CPL_CVSID
 *
 * Revision 1.30  2001/07/16 03:34:55  warmerda
 * various fixes, and improvements suggested by Ben Driscoe on gdal list
 *
 * Revision 1.29  2001/07/13 12:33:10  warmerda
 * Fixed crash on OGRSpatialReference if PROJECTION missing.
 *
 * Revision 1.28  2001/04/04 16:09:57  warmerda
 * clarify units and definition of TOWGS84
 *
 * Revision 1.27  2001/01/22 13:59:55  warmerda
 * added SetSOC
 *
 * Revision 1.26  2001/01/19 22:14:49  warmerda
 * fixed SetNode to replace existing value properly if it exists
 *
 * Revision 1.25  2001/01/19 21:10:47  warmerda
 * replaced tabs
 *
 * Revision 1.24  2000/11/17 17:26:02  warmerda
 * set a name in SetUTM()
 *
 * Revision 1.23  2000/11/09 06:21:32  warmerda
 * added limited ESRI prj support
 *
 * Revision 1.22  2000/10/20 04:20:17  warmerda
 * overwrite existing linear units node if one exists
 *
 * Revision 1.21  2000/10/16 21:26:07  warmerda
 * added some level of LOCAL_CS support
 *
 * Revision 1.20  2000/10/13 20:59:49  warmerda
 * ensure we can set the PROJCS name on an existing PROJCS
 *
 * Revision 1.19  2000/07/09 20:49:54  warmerda
 * added exportToPrettyWkt()
 *
 * Revision 1.18  2000/06/09 13:26:03  warmerda
 * avoid using an inverse flattening of zero
 *
 * Revision 1.17  2000/05/30 22:45:45  warmerda
 * added OSRCloneGeogCS()
 *
 * Revision 1.16  2000/03/24 14:49:56  warmerda
 * added WGS84 related methods
 *
 * Revision 1.15  2000/03/22 01:09:43  warmerda
 * added SetProjCS and SetWellKnownTextCS
 *
 * Revision 1.14  2000/03/20 22:40:59  warmerda
 * Added C API and some documentation.
 *
 * Revision 1.13  2000/03/20 14:58:39  warmerda
 * added CloneGeogCS, IsProjected, isGeogCS, and GeogCSMatch
 *
 * Revision 1.12  2000/03/16 19:04:55  warmerda
 * added SetTMG(), SetAuthority() and StripCTParms()
 *
 * Revision 1.11  2000/02/25 13:23:25  warmerda
 * removed include of ogr_geometry.h
 *
 * Revision 1.10  2000/01/11 22:12:39  warmerda
 * Ensure GEOGCS node is always at position 1 under PROJCS
 *
 * Revision 1.9  2000/01/06 19:46:10  warmerda
 * added special logic for setting, and recognising UTM
 *
 * Revision 1.8  1999/11/18 19:02:19  warmerda
 * expanded tabs
 *
 * Revision 1.7  1999/10/05 17:53:11  warmerda
 * GetLinearUnits() should look for UNIT not UNITS.
 *
 * Revision 1.6  1999/09/29 16:37:05  warmerda
 * added several new projection set methods
 *
 * Revision 1.5  1999/09/17 16:15:15  warmerda
 * Added angular units to SetGeogCS().  Use OGRPrintDouble(). Added SetLCC1SP().
 *
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
#include "ogr_p.h"

CPL_CVSID("$Id$");

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


static void OGRPrintDouble( char * pszStrBuf, double dfValue )

{
    sprintf( pszStrBuf, "%.15g", dfValue );
}

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
 *
 * The C function OSRNewSpatialReference() does the same thing as this
 * constructor. 
 *
 * @param pszWKT well known text definition to which the object should
 * be initialized, or NULL (the default). 
 */

OGRSpatialReference::OGRSpatialReference( const char * pszWKT )

{
    nRefCount = 1;
    poRoot = NULL;

    if( pszWKT != NULL )
        importFromWkt( (char **) &pszWKT );
}

/************************************************************************/
/*                       OSRNewSpatialReference()                       */
/************************************************************************/

OGRSpatialReferenceH OSRNewSpatialReference( const char *pszWKT )

{
    OGRSpatialReference * poSRS;

    poSRS = new OGRSpatialReference();

    if( pszWKT != NULL && strlen(pszWKT) > 0 )
    {
        if( poSRS->importFromWkt( (char **) (&pszWKT) ) != OGRERR_NONE )
        {
            delete poSRS;
            poSRS = NULL;
        }
    }

    return poSRS;
}

/************************************************************************/
/*                        OGRSpatialReference()                         */
/*                                                                      */
/*      Simple copy constructor.  See also Clone().                     */
/************************************************************************/

OGRSpatialReference::OGRSpatialReference(const OGRSpatialReference &oOther)

{
    nRefCount = 1;
    poRoot = NULL;

    if( oOther.poRoot != NULL )
        poRoot = oOther.poRoot->Clone();
}

/************************************************************************/
/*                        ~OGRSpatialReference()                        */
/************************************************************************/

/**
 * OGRSpatialReference destructor. 
 *
 * The C function OSRDestroySpatialReference() does the same thing as this
 * method. 
 */

OGRSpatialReference::~OGRSpatialReference()

{
    if( poRoot != NULL )
        delete poRoot;
}

/************************************************************************/
/*                     OSRDestroySpatialReference()                     */
/************************************************************************/

void OSRDestroySpatialReference( OGRSpatialReferenceH hSRS )

{
    delete ((OGRSpatialReference *) hSRS);
}

/************************************************************************/
/*                               Clear()                                */
/************************************************************************/

/**
 * Wipe current definition.
 *
 * Returns OGRSpatialReference to a state with no definition, as it 
 * exists when first created.  It does not affect reference counts.
 */

void OGRSpatialReference::Clear()

{
    if( poRoot )
        delete poRoot;

    poRoot = NULL;
}

/************************************************************************/
/*                             operator=()                              */
/************************************************************************/

OGRSpatialReference &
OGRSpatialReference::operator=(const OGRSpatialReference &oSource)

{
    if( poRoot != NULL )
    {
        delete poRoot;
        poRoot = NULL;
    }
    
    if( oSource.poRoot != NULL )
        poRoot = oSource.poRoot->Clone();

    return *this;
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
 * The method does the same thing as the C function OSRReference(). 
 *
 * @return the updated reference count.
 */

int OGRSpatialReference::Reference()

{
    return ++nRefCount;
}

/************************************************************************/
/*                            OSRReference()                            */
/************************************************************************/

int OSRReference( OGRSpatialReferenceH hSRS )

{
    return ((OGRSpatialReference *) hSRS)->Reference();
}

/************************************************************************/
/*                            Dereference()                             */
/************************************************************************/

/**
 * Decrements the reference count by one.
 *
 * The method does the same thing as the C function OSRDereference(). 
 *
 * @return the updated reference count.
 */

int OGRSpatialReference::Dereference()

{
    return --nRefCount;
}

/************************************************************************/
/*                           OSRDereference()                           */
/************************************************************************/

int OSRDereference( OGRSpatialReferenceH hSRS )

{
    return ((OGRSpatialReference *) hSRS)->Reference();
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
 * @param pszNodePath the name of the node to search for.  May contain multiple
 * components such as "GEOGCS|UNITS".
 *
 * @return a pointer to the node found, or NULL if none.
 */

OGR_SRSNode *OGRSpatialReference::GetAttrNode( const char * pszNodePath )

{
    char        **papszPathTokens;
    OGR_SRSNode *poNode;

    papszPathTokens = CSLTokenizeStringComplex(pszNodePath, "|", TRUE, FALSE);

    if( CSLCount( papszPathTokens ) < 1 )
        return NULL;

    poNode = GetRoot();
    for( int i = 0; poNode != NULL && papszPathTokens[i] != NULL; i++ )
    {
        poNode = poNode->GetNode( papszPathTokens[i] );
    }

    CSLDestroy( papszPathTokens );

    return poNode;
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
 * This method does the same thing as the C function OSRGetAttrValue().
 *
 * @param pszNodeName the tree node to look for (case insensitive).
 * @param iAttr the child of the node to fetch (zero based).
 *
 * @return the requested value, or NULL if it fails for any reason. 
 */

const char *OGRSpatialReference::GetAttrValue( const char * pszNodeName,
                                               int iAttr )

{
    OGR_SRSNode *poNode;

    poNode = GetAttrNode( pszNodeName );
    if( poNode == NULL )
        return NULL;

    if( iAttr < 0 || iAttr >= poNode->GetChildCount() )
        return NULL;

    return poNode->GetChild(iAttr)->GetValue();
}

/************************************************************************/
/*                          OSRGetAttrValue()                           */
/************************************************************************/

const char *OSRGetAttrValue( OGRSpatialReferenceH hSRS,
                             const char * pszKey, int iChild )

{
    return ((OGRSpatialReference *) hSRS)->GetAttrValue( pszKey, iChild );
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
        OGR_SRSNode     *poNode;
        int             i;

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
    OGR_SRSNode *poGEOGCS = poRoot->GetNode( "GEOGCS" );

    if( poGEOGCS != NULL )
    {
        OGR_SRSNode     *poNode;
        int             i;

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
    OGR_SRSNode *poDATUM = poRoot->GetNode( "DATUM" );

    if( poDATUM != NULL )
    {
        OGR_SRSNode     *poSPHEROID;

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
    OGRSpatialReference *poNewRef;

    poNewRef = new OGRSpatialReference();

    if( poRoot != NULL )
        poNewRef->poRoot = poRoot->Clone();

    return poNewRef;
}

/************************************************************************/
/*                         exportToPrettyWkt()                          */
/*                                                                      */
/*      Translate into a nicely formatted string for display to a       */
/*      person.                                                         */
/************************************************************************/

OGRErr OGRSpatialReference::exportToPrettyWkt( char ** ppszResult, 
                                               int bSimplify )

{
    return poRoot->exportToPrettyWkt( ppszResult, bSimplify, 1 );
}

/************************************************************************/
/*                        OSRExportToPrettyWkt()                        */
/************************************************************************/

OGRErr OSRExportToPrettyWkt( OGRSpatialReferenceH hSRS, char ** ppszReturn,
                             int bSimplify)

{
    *ppszReturn = NULL;

    return ((OGRSpatialReference *) hSRS)->exportToPrettyWkt( ppszReturn,
                                                              bSimplify );
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
 * This method is the same as the C function OSRExportToWkt().
 *
 * @param ppszResult the resulting string is returned in this pointer.
 *
 * @return currently OGRERR_NONE is always returned, but the future it
 * is possible error conditions will develop. 
 */
 
OGRErr  OGRSpatialReference::exportToWkt( char ** ppszResult )

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
/*                           OSRExportToWkt()                           */
/************************************************************************/

OGRErr OSRExportToWkt( OGRSpatialReferenceH hSRS, char ** ppszReturn )

{
    *ppszReturn = NULL;

    return ((OGRSpatialReference *) hSRS)->exportToWkt( ppszReturn );
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
 * This method is the same as the C function OSRImportFromWkt().
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
/*                          OSRImportFromWkt()                          */
/************************************************************************/

OGRErr OSRImportFromWkt( OGRSpatialReferenceH hSRS, char **ppszInput )

{
    return ((OGRSpatialReference *) hSRS)->importFromWkt( ppszInput );
}

/************************************************************************/
/*                              SetNode()                               */
/************************************************************************/

/**
 * Set attribute value in spatial reference.
 *
 * Missing intermediate nodes in the path will be created if not already
 * in existance.  If the attribute has no children one will be created and
 * assigned the value otherwise the zeroth child will be assigned the value.
 *
 * This method does the same as the C function OSRSetAttrValue(). 
 *
 * @param pszNodePath full path to attribute to be set.  For instance
 * "PROJCS|GEOGCS|UNITS".
 * 
 * @param pszNewNodeValue value to be assigned to node, such as "meter". 
 * This may be NULL if you just want to force creation of the intermediate
 * path.
 *
 * @return OGRERR_NONE on success. 
 */

OGRErr OGRSpatialReference::SetNode( const char * pszNodePath,
                                     const char * pszNewNodeValue )

{
    char        **papszPathTokens;
    int         i;
    OGR_SRSNode *poNode;

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
        int     j;
        
        for( j = 0; j < poNode->GetChildCount(); j++ )
        {
            if( EQUAL(poNode->GetChild( j )->GetValue(),papszPathTokens[i]) )
            {
                poNode = poNode->GetChild(j);
                j = -1;
                break;
            }
        }

        if( j != -1 )
        {
            OGR_SRSNode *poNewNode = new OGR_SRSNode( papszPathTokens[i] );
            poNode->AddChild( poNewNode );
            poNode = poNewNode;
        }
    }

    CSLDestroy( papszPathTokens );

    if( pszNewNodeValue != NULL )
    {
        if( poNode->GetChildCount() > 0 )
            poNode->GetChild(0)->SetValue( pszNewNodeValue );
        else
            poNode->AddChild( new OGR_SRSNode( pszNewNodeValue ) );
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                          OSRSetAttrValue()                           */
/************************************************************************/

OGRErr OSRSetAttrValue( OGRSpatialReferenceH hSRS, 
                        const char * pszPath, const char * pszValue )

{
    return ((OGRSpatialReference *) hSRS)->SetNode( pszPath, pszValue );
}

/************************************************************************/
/*                              SetNode()                               */
/************************************************************************/

OGRErr OGRSpatialReference::SetNode( const char *pszNodePath,
                                     double dfValue )

{
    char        szValue[64];

    if( ABS(dfValue - (int) dfValue) == 0.0 )
        sprintf( szValue, "%d", (int) dfValue );
    else
        // notdef: sprintf( szValue, "%.12f", dfValue );
        OGRPrintDouble( szValue, dfValue );

    return SetNode( pszNodePath, szValue );
}

/************************************************************************/
/*                           SetLinearUnits()                           */
/************************************************************************/

/**
 * Set the linear units for the projection.
 *
 * This method creates a UNITS subnode with the specified values as a
 * child of the PROJCS or LOCAL_CS node.  It does not currently check for an 
 * existing node and override it, but it should!
 *
 * This method does the same as the C function OSRSetLinearUnits(). 
 *
 * @param pszUnitsName the units name to be used.  Some preferred units
 * names can be found in ogr_srs_api.h such as SRS_UL_METER, SRS_UL_FOOT 
 * and SRS_UL_US_FOOT. 
 *
 * @param dfInMeters the value to multiple by a length in the indicated
 * units to transform to meters.  Some standard conversion factors can
 * be found in ogr_srs_api.h. 
 *
 * @return OGRERR_NONE on success.
 */

OGRErr OGRSpatialReference::SetLinearUnits( const char * pszUnitsName,
                                            double dfInMeters )

{
    OGR_SRSNode *poCS;
    OGR_SRSNode *poUnits;
    char        szValue[128];

    poCS = GetAttrNode( "PROJCS" );
    if( poCS == NULL )
        poCS = GetAttrNode( "LOCAL_CS" );

    if( poCS == NULL )
        return OGRERR_FAILURE;

    if( dfInMeters == (int) dfInMeters )
        sprintf( szValue, "%d", (int) dfInMeters );
    else
        //notdef: sprintf( szValue, "%.12f", dfInMeters );
        OGRPrintDouble( szValue, dfInMeters );

    if( poCS->FindChild( "UNIT" ) >= 0 )
    {
        poUnits = poCS->GetChild( poCS->FindChild( "UNIT" ) );
        poUnits->GetChild(0)->SetValue( pszUnitsName );
        poUnits->GetChild(1)->SetValue( szValue );
    }
    else
    {
        poUnits = new OGR_SRSNode( "UNIT" );
        poUnits->AddChild( new OGR_SRSNode( pszUnitsName ) );
        poUnits->AddChild( new OGR_SRSNode( szValue ) );
        
        poCS->AddChild( poUnits );
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                         OSRSetLinearUnits()                          */
/************************************************************************/

OGRErr OSRSetLinearUnits( OGRSpatialReferenceH hSRS, 
                          const char * pszUnits, double dfInMeters )

{
    return ((OGRSpatialReference *) hSRS)->SetLinearUnits( pszUnits, 
                                                           dfInMeters );
}

/************************************************************************/
/*                           GetLinearUnits()                           */
/************************************************************************/

/**
 * Fetch linear projection units. 
 *
 * If no units are available, a value of "Meters" and 1.0 will be assumed.
 * This method only checks directly under the PROJCS or LOCAL_CS node for 
 * units.
 *
 * This method does the same thing as the C function OSRGetLinearUnits()/
 *
 * @param ppszName a pointer to be updated with the pointer to the 
 * units name.  The returned value remains internal to the OGRSpatialReference
 * and shouldn't be freed, or modified.  It may be invalidated on the next
 * OGRSpatialReference call. 
 *
 * @return the value to multiply by linear distances to transform them to 
 * meters.
 */

double OGRSpatialReference::GetLinearUnits( char ** ppszName )

{
    OGR_SRSNode *poCS = GetAttrNode( "PROJCS" );

    if( poCS == NULL )
        poCS = GetAttrNode( "LOCAL_CS" );

    if( ppszName != NULL )
        *ppszName = "unknown";
        
    if( poCS == NULL )
        return 1.0;

    for( int iChild = 0; iChild < poCS->GetChildCount(); iChild++ )
    {
        OGR_SRSNode     *poChild = poCS->GetChild(iChild);
        
        if( EQUAL(poChild->GetValue(),"UNIT")
            && poChild->GetChildCount() >= 2 )
        {
            if( ppszName != NULL )
                *ppszName = (char *) poChild->GetChild(0)->GetValue();
            
            return atof( poChild->GetChild(1)->GetValue() );
        }
    }

    return 1.0;
}

/************************************************************************/
/*                         OSRGetLinearUnits()                          */
/************************************************************************/

double OSRGetLinearUnits( OGRSpatialReferenceH hSRS, char ** ppszName )
    
{
    return ((OGRSpatialReference *) hSRS)->GetLinearUnits( ppszName );
}

/************************************************************************/
/*                             SetGeogCS()                              */
/************************************************************************/

/**
 * Set geographic coordinate system. 
 *
 * This method is used to set the datum, ellipsoid, prime meridian and
 * angular units for a geographic coordinate system.  It can be used on it's
 * own to establish a geographic spatial reference, or applied to a 
 * projected coordinate system to establish the underlying geographic 
 * coordinate system. 
 *
 * This method does the same as the C function OSRSetGeogCS(). 
 *
 * @param pszGeogName user visible name for the geographic coordinate system
 * (not to serve as a key).
 * 
 * @param pszDatumName key name for this datum.  The OpenGIS specification 
 * lists some known values, and otherwise EPSG datum names with a standard
 * transformation are considered legal keys. 
 * 
 * @param pszSpheroidName user visible spheroid name (not to serve as a key)
 *
 * @param dfSemiMajor the semi major axis of the spheroid.
 * 
 * @param dfInvFlattening the inverse flattening for the spheroid.
 * This can be computed from the semi minor axis as 
 * 1/f = 1 / (semimajor/semiminor - 1.0).
 *
 * @param pszPMName the name of the prime merdidian (not to serve as a key)
 * If this is NULL a default value of "Greenwich" will be used. 
 * 
 * @param dfPMOffset the longitude of greenwich relative to this prime
 * meridian.
 *
 * @param pszAngularUnits the angular units name (see ogr_srs_api.h for some
 * standard names).  If NULL a value of "degrees" will be assumed. 
 * 
 * @param dfConvertToRadians value to multiply angular units by to transform
 * them to radians.  A value of SRS_UL_DEGREE_CONV will be used if
 * pszAngularUnits is NULL.
 *
 * @return OGRERR_NONE on success.
 */

OGRErr
OGRSpatialReference::SetGeogCS( const char * pszGeogName,
                                const char * pszDatumName,
                                const char * pszSpheroidName,
                                double dfSemiMajor, double dfInvFlattening,
                                const char * pszPMName, double dfPMOffset,
                                const char * pszAngularUnits,
                                double dfConvertToRadians )

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

    if( pszAngularUnits == NULL )
    {
        pszAngularUnits = SRS_UA_DEGREE;
        dfConvertToRadians = atof(SRS_UA_DEGREE_CONV);
    }

/* -------------------------------------------------------------------- */
/*      Build the GEOGCS object.                                        */
/* -------------------------------------------------------------------- */
    char                szValue[128];
    OGR_SRSNode         *poGeogCS, *poSpheroid, *poDatum, *poPM, *poUnits;

    poGeogCS = new OGR_SRSNode( "GEOGCS" );
    poGeogCS->AddChild( new OGR_SRSNode( pszGeogName ) );
    
/* -------------------------------------------------------------------- */
/*      Setup the spheroid.                                             */
/* -------------------------------------------------------------------- */
    poSpheroid = new OGR_SRSNode( "SPHEROID" );
    poSpheroid->AddChild( new OGR_SRSNode( pszSpheroidName ) );

    //notdef: sprintf( szValue, "%.3f", dfSemiMajor );
    OGRPrintDouble( szValue, dfSemiMajor );
    poSpheroid->AddChild( new OGR_SRSNode(szValue) );

    //notdef: sprintf( szValue, "%.14f", dfInvFlattening );
    OGRPrintDouble( szValue, dfInvFlattening );
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
        //notdef: sprintf( szValue, "%.16f", dfPMOffset );
        OGRPrintDouble( szValue, dfPMOffset );
    
    poPM = new OGR_SRSNode( "PRIMEM" );
    poPM->AddChild( new OGR_SRSNode( pszPMName ) );
    poPM->AddChild( new OGR_SRSNode( szValue ) );

/* -------------------------------------------------------------------- */
/*      Setup the rotational units.                                     */
/* -------------------------------------------------------------------- */
    OGRPrintDouble( szValue, dfConvertToRadians );
    
    poUnits = new OGR_SRSNode( "UNIT" );
    poUnits->AddChild( new OGR_SRSNode(pszAngularUnits) );
    poUnits->AddChild( new OGR_SRSNode(szValue) );
    
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
        poRoot->InsertChild( poGeogCS, 1 );
    else
        SetRoot( poGeogCS );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            OSRSetGeogCS()                            */
/************************************************************************/

OGRErr OSRSetGeogCS( OGRSpatialReferenceH hSRS,
                     const char * pszGeogName,
                     const char * pszDatumName,
                     const char * pszSpheroidName,
                     double dfSemiMajor, double dfInvFlattening,
                     const char * pszPMName, double dfPMOffset,
                     const char * pszAngularUnits,
                     double dfConvertToRadians )

{
    return ((OGRSpatialReference *) hSRS)->SetGeogCS( 
        pszGeogName, pszDatumName, 
        pszSpheroidName, dfSemiMajor, dfInvFlattening, 
        pszPMName, dfPMOffset, pszAngularUnits, dfConvertToRadians );
                                                      
}

/************************************************************************/
/*                         SetWellKnownGeogCS()                         */
/************************************************************************/

/**
 * Set a GeogCS based on well known name.
 *
 * This may be called on an empty OGRSpatialReference to make a geographic
 * coordinate system, or on something with an existing PROJCS node to 
 * set the underlying geographic coordinate system of a projected coordinate
 * system. 
 *
 * The following well known text values are currently supported:
 * <ul>
 * <li> "WGS84": same as "EPSG:4326" but has no dependence on EPSG data files.
 * <li> "WGS72": same as "EPSG:4322" but has no dependence on EPSG data files.
 * <li> "NAD27": same as "EPSG:4267" but has no dependence on EPSG data files.
 * <li> "NAD83": same as "EPSG:4269" but has no dependence on EPSG data files.
 * <li> "EPSG:n": same as doing an ImportFromEPSG(n).
 * </ul>
 * 
 * @param pszName name of well known geographic coordinate system.
 * @return OGRERR_NONE on success, or OGRERR_FAILURE if the name isn't
 * recognised, the target object is already initialized, or an EPSG value
 * can't be successfully looked up.
 */ 

OGRErr OGRSpatialReference::SetWellKnownGeogCS( const char * pszName )

{
    OGR_SRSNode  *poGeogCS = NULL;
    OGRErr       eErr = OGRERR_FAILURE;
    char         *pszWKT = NULL;

/* -------------------------------------------------------------------- */
/*      Do we already have a GEOGCS?                                    */
/* -------------------------------------------------------------------- */
    if( GetAttrNode( "GEOGCS" ) != NULL )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Check for EPSG authority numbers.                               */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszName, "EPSG:",5) )
    {
        OGRSpatialReference   oSRS2;

        eErr = oSRS2.importFromEPSG( atoi(pszName+5) );
        if( eErr != OGRERR_NONE )
            return eErr;

        if( !oSRS2.IsGeographic() )
            return OGRERR_FAILURE;

        poGeogCS = oSRS2.GetRoot()->Clone();
    }

/* -------------------------------------------------------------------- */
/*      Check for simple names.                                         */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszName, "WGS84") )
        pszWKT = "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",7030]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",6326]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",8901]],UNIT[\"DMSH\",0.0174532925199433,AUTHORITY[\"EPSG\",9108]],AXIS[\"Lat\",NORTH],AXIS[\"Long\",EAST],AUTHORITY[\"EPSG\",4326]]";

    else if( EQUAL(pszName, "WGS72") )
        pszWKT = "GEOGCS[\"WGS 72\",DATUM[\"WGS_1972\",SPHEROID[\"WGS 72\",6378135,298.26,AUTHORITY[\"EPSG\",7043]],TOWGS84[0,0,4.5,0,0,0.554,0.2263],AUTHORITY[\"EPSG\",6322]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",8901]],UNIT[\"DMSH\",0.0174532925199433,AUTHORITY[\"EPSG\",9108]],AXIS[\"Lat\",NORTH],AXIS[\"Long\",EAST],AUTHORITY[\"EPSG\",4322]]";

    else if( EQUAL(pszName, "NAD27") )
        pszWKT = "GEOGCS[\"NAD27\",DATUM[\"North_American_Datum_1927\",SPHEROID[\"Clarke 1866\",6378206.4,294.978698213898,AUTHORITY[\"EPSG\",7008]],TOWGS84[-3,142,183,0,0,0,0],AUTHORITY[\"EPSG\",6267]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",8901]],UNIT[\"DMSH\",0.0174532925199433,AUTHORITY[\"EPSG\",9108]],AXIS[\"Lat\",NORTH],AXIS[\"Long\",EAST],AUTHORITY[\"EPSG\",4267]]";
        
    else if( EQUAL(pszName, "NAD83") )
        pszWKT = "GEOGCS[\"NAD83\",DATUM[\"North_American_Datum_1983\",SPHEROID[\"GRS 1980\",6378137,298.257222101,AUTHORITY[\"EPSG\",7019]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",6269]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",8901]],UNIT[\"DMSH\",0.0174532925199433,AUTHORITY[\"EPSG\",9108]],AXIS[\"Lat\",NORTH],AXIS[\"Long\",EAST],AUTHORITY[\"EPSG\",4269]]";

    if( pszWKT != NULL )
    {
        poGeogCS = new OGR_SRSNode;
        poGeogCS->importFromWkt(&pszWKT);
    }

    if( poGeogCS == NULL )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Attach below the PROJCS if there is one, or make this the root. */
/* -------------------------------------------------------------------- */
    if( GetRoot() != NULL && EQUAL(GetRoot()->GetValue(),"PROJCS") )
        poRoot->InsertChild( poGeogCS, 1 );
    else
        SetRoot( poGeogCS );

    return OGRERR_NONE;
}

/************************************************************************/
/*                       OSRSetWellKnownGeogCS()                        */
/************************************************************************/

OGRErr OSRSetWellKnownGeogCS( OGRSpatialReferenceH hSRS, const char *pszName )

{
    return ((OGRSpatialReference *) hSRS)->SetWellKnownGeogCS( pszName );
}

/************************************************************************/
/*                          SetFromUserInput()                          */
/************************************************************************/

/**
 * Set spatial reference from various text formats.
 *
 * This method will examine the provided input, and try to deduce the
 * format, and then use it to initialize the spatial reference system.  It
 * may take the following forms:
 *
 * <ol>
 * <li> Well Known Text definition - passed on to importFromWkt().
 * <li> "EPSG:n" - number passed on to importFromEPSG(). 
 * <li> filename - file read for WKT definition, passed on to importFromWkt().
 * <li> well known name accepted by SetWellKnownGeogCS(), such as NAD27, NAD83,
 * WGS84 or WGS72. 
 * </ol>
 *
 * It is expected that this method will be extended in the future to support
 * XML and perhaps a simplified "minilanguage" for indicating common UTM and
 * State Plane definitions. 
 *
 * This method is intended to be flexible, but by it's nature it is 
 * imprecise as it must guess information about the format intended.  When
 * possible applications should call the specific method appropriate if the
 * input is known to be in a particular format. 
 *
 * This method does the same thing as the OSRSetFromUserInput() function.
 * 
 * @param pszDefinition text definition to try to deduce SRS from.
 *
 * @return OGRERR_NONE on success, or an error code if the name isn't
 * recognised, the definition is corrupt, or an EPSG value can't be 
 * successfully looked up.
 */ 

OGRErr OGRSpatialReference::SetFromUserInput( const char * pszDefinition )

{
/* -------------------------------------------------------------------- */
/*      Is it a recognised syntax?                                      */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszDefinition,"PROJCS",6)
        || EQUALN(pszDefinition,"GEOGCS",6)
        || EQUALN(pszDefinition,"LOCAL_CS",8) )
    {
        return importFromWkt( (char **) &pszDefinition );
    }

    if( EQUALN(pszDefinition,"EPSG:",5) )
    {
        return importFromEPSG( atoi(pszDefinition+5) );
    }

    if( EQUAL(pszDefinition,"NAD27") 
        || EQUAL(pszDefinition,"NAD83") 
        || EQUAL(pszDefinition,"WGS84") 
        || EQUAL(pszDefinition,"WGS72") )
    {
        Clear();
        return SetWellKnownGeogCS( pszDefinition );
    }

/* -------------------------------------------------------------------- */
/*      Try to open it as a file.                                       */
/* -------------------------------------------------------------------- */
    FILE	*fp;
    char	szBuffer[40000], *pszBufPtr;
    int		nBytes;

    fp = VSIFOpen( pszDefinition, "rt" );
    if( fp == NULL )
        return OGRERR_NONE;

    nBytes = VSIFRead( szBuffer, 1, sizeof(szBuffer), fp );
    VSIFClose( fp );

    if( nBytes == sizeof(szBuffer) )
    {
        CPLDebug( "OGR", 
                  "OGRSpatialReference::SetFromUserInput(%s), opened file\n"
                  "but it is to large for our generous buffer.  Is it really\n"
                  "just a WKT definition?" );
        return OGRERR_FAILURE;
    }

    pszBufPtr = szBuffer;

    return importFromWkt( &pszBufPtr );
}

/************************************************************************/
/*                        OSRSetFromUserInput()                         */
/************************************************************************/

OGRErr OSRSetFromUserInput( OGRSpatialReferenceH hSRS, const char *pszDef )

{
    return ((OGRSpatialReference *) hSRS)->SetFromUserInput( pszDef );
}

/************************************************************************/
/*                            GetSemiMajor()                            */
/************************************************************************/

/**
 * Get spheroid semi major axis.
 *
 * This method does the same thing as the C function OSRGetSemiMajor().
 *
 * @param pnErr if non-NULL set to OGRERR_FAILURE if semi major axis
 * can be found. 
 *
 * @return semi-major axis, or SRS_WGS84_SEMIMAJOR if it can't be found.
 */

double OGRSpatialReference::GetSemiMajor( OGRErr * pnErr )

{
    OGR_SRSNode *poSpheroid = GetAttrNode( "SPHEROID" );
    
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
/*                          OSRGetSemiMajor()                           */
/************************************************************************/

double OSRGetSemiMajor( OGRSpatialReferenceH hSRS, OGRErr *pnErr )

{
    return ((OGRSpatialReference *) hSRS)->GetSemiMajor( pnErr );
}

/************************************************************************/
/*                          GetInvFlattening()                          */
/************************************************************************/

/**
 * Get spheroid inverse flattening.
 *
 * This method does the same thing as the C function OSRGetInvFlattening().
 *
 * @param pnErr if non-NULL set to OGRERR_FAILURE if no inverse flattening 
 * can be found. 
 *
 * @return inverse flattening, or SRS_WGS84_INVFLATTENING if it can't be found.
 */

double OGRSpatialReference::GetInvFlattening( OGRErr * pnErr )

{
    OGR_SRSNode *poSpheroid = GetAttrNode( "SPHEROID" );
    
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
/*                        OSRGetInvFlattening()                         */
/************************************************************************/

double OSRGetInvFlattening( OGRSpatialReferenceH hSRS, OGRErr *pnErr )

{
    return ((OGRSpatialReference *) hSRS)->GetInvFlattening( pnErr );
}

/************************************************************************/
/*                            GetSemiMinor()                            */
/************************************************************************/

/**
 * Get spheroid semi minor axis.
 *
 * This method does the same thing as the C function OSRGetSemiMinor().
 *
 * @param pnErr if non-NULL set to OGRERR_FAILURE if semi minor axis
 * can be found. 
 *
 * @return semi-minor axis, or WGS84 semi minor if it can't be found.
 */

double OGRSpatialReference::GetSemiMinor( OGRErr * pnErr )

{
    double      dfInvFlattening, dfSemiMajor;

    dfSemiMajor = GetSemiMajor( pnErr );
    dfInvFlattening = GetInvFlattening( pnErr );

    if( ABS(dfInvFlattening) < 0.000000000001 )
        return dfSemiMajor;
    else
        return dfSemiMajor * (1.0 - 1.0/dfInvFlattening);
}

/************************************************************************/
/*                          OSRGetSemiMinor()                           */
/************************************************************************/

double OSRGetSemiMinor( OGRSpatialReferenceH hSRS, OGRErr *pnErr )

{
    return ((OGRSpatialReference *) hSRS)->GetSemiMinor( pnErr );
}

/************************************************************************/
/*                             SetLocalCS()                             */
/************************************************************************/

/**
 * Set the user visible LOCAL_CS name.
 *
 * This method is the same as the C function OSRSetLocalCS(). 
 *
 * This method is will ensure a LOCAL_CS node is created as the root, 
 * and set the provided name on it.  It must be used before SetLinearUnits().
 *
 * @param pszName the user visible name to assign.  Not used as a key.
 * 
 * @return OGRERR_NONE on success.
 */

OGRErr OGRSpatialReference::SetLocalCS( const char * pszName )

{
    OGR_SRSNode *poCS = GetAttrNode( "LOCAL_CS" );

    if( poCS == NULL && GetRoot() != NULL )
    {
        CPLDebug( "OGR", 
                  "OGRSpatialReference::SetLocalCS(%s) failed.\n"
               "It appears an incompatible root node (%s) already exists.\n",
                  GetRoot()->GetValue() );
        return OGRERR_FAILURE;
    }
    else
    {
        SetNode( "LOCAL_CS", pszName );
        return OGRERR_NONE;
    }
}

/************************************************************************/
/*                           OSRSetLocalCS()                            */
/************************************************************************/

OGRErr OSRSetLocalCS( OGRSpatialReferenceH hSRS, const char * pszName )

{
    return ((OGRSpatialReference *) hSRS)->SetLocalCS( pszName );
}

/************************************************************************/
/*                             SetProjCS()                              */
/************************************************************************/

/**
 * Set the user visible PROJCS name.
 *
 * This method is the same as the C function OSRSetProjCS(). 
 *
 * This method is will ensure a PROJCS node is created as the root, 
 * and set the provided name on it.  If used on a GEOGCS coordinate system, 
 * the GEOGCS node will be demoted to be a child of the new PROJCS root.
 *
 * @param pszName the user visible name to assign.  Not used as a key.
 * 
 * @return OGRERR_NONE on success.
 */

OGRErr OGRSpatialReference::SetProjCS( const char * pszName )

{
    OGR_SRSNode *poGeogCS = NULL;
    OGR_SRSNode *poProjCS = GetAttrNode( "PROJCS" );

    if( poRoot != NULL && EQUAL(poRoot->GetValue(),"GEOGCS") )
    {
        poGeogCS = poRoot;
        poRoot = NULL;
    }

    if( poProjCS == NULL && GetRoot() != NULL )
    {
        CPLDebug( "OGR", 
                  "OGRSpatialReference::SetProjCS(%s) failed.\n"
               "It appears an incompatible root node (%s) already exists.\n",
                  GetRoot()->GetValue() );
        return OGRERR_FAILURE;
    }

    SetNode( "PROJCS", pszName );

    if( poGeogCS != NULL )
        poRoot->InsertChild( poGeogCS, 1 );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            OSRSetProjCS()                            */
/************************************************************************/

OGRErr OSRSetProjCS( OGRSpatialReferenceH hSRS, const char * pszName )

{
    return ((OGRSpatialReference *) hSRS)->SetProjCS( pszName );
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

OGRErr OGRSpatialReference::SetProjection( const char * pszProjection )

{
    OGR_SRSNode *poGeogCS = NULL;
    OGRErr eErr;

    if( poRoot != NULL && EQUAL(poRoot->GetValue(),"GEOGCS") )
    {
        poGeogCS = poRoot;
        poRoot = NULL;
    }

    if( !GetAttrNode( "PROJCS" ) )
    {
        SetNode( "PROJCS", "unnamed" );
    }

    eErr = SetNode( "PROJCS|PROJECTION", pszProjection );
    if( eErr != OGRERR_NONE )
        return eErr;

    if( poGeogCS != NULL )
        poRoot->InsertChild( poGeogCS, 1 );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            SetProjParm()                             */
/************************************************************************/

/**
 * Set a projection parameter value.
 *
 * Adds a new PARAMETER under the PROJCS with the indicated name and value.
 *
 * This method is the same as the C function OSRSetProjParm().
 *
 * Please check http://www.remotesensing.org/geotiff/proj_list pages for
 * legal parameter names for specific projections.
 *
 * 
 * @param pszParmName the parameter name, which should be selected from
 * the macros in ogr_srs_api.h, such as SRS_PP_CENTRAL_MERIDIAN. 
 *
 * @param dfValue value to assign. 
 * 
 * @return OGRERR_NONE on success.
 */

OGRErr OGRSpatialReference::SetProjParm( const char * pszParmName,
                                         double dfValue )

{
    OGR_SRSNode *poPROJCS = GetAttrNode( "PROJCS" );
    OGR_SRSNode *poParm;
    char        szValue[64];

    if( poPROJCS == NULL || GetAttrNode( pszParmName ) != NULL )
        return OGRERR_FAILURE;

    poParm = new OGR_SRSNode( "PARAMETER" );
    poParm->AddChild( new OGR_SRSNode( pszParmName ) );

#ifdef notdef    
    if( ABS(dfValue - (int) dfValue) == 0.0 )
        sprintf( szValue, "%d", (int) dfValue );
    else
        sprintf( szValue, "%.12f", dfValue );
#endif        
    OGRPrintDouble( szValue, dfValue );

    poParm->AddChild( new OGR_SRSNode( szValue ) );

    poPROJCS->AddChild( poParm );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           OSRSetProjParm()                           */
/************************************************************************/

OGRErr OSRSetProjParm( OGRSpatialReferenceH hSRS, 
                       const char * pszParmName, double dfValue )

{
    return ((OGRSpatialReference *) hSRS)->SetProjParm( pszParmName, dfValue );
}

/************************************************************************/
/*                            GetProjParm()                             */
/************************************************************************/

/**
 * Fetch a projection parameter value.
 *
 * NOTE: This code should be modified to translate non degree angles into
 * degrees based on the GEOGCS unit.  This has not yet been done.
 *
 * This method is the same as the C function OSRGetProjParm().
 *
 * @param pszName the name of the parameter to fetch, from the set of 
 * SRS_PP codes in ogr_srs_api.h.
 *
 * @param dfDefaultValue the value to return if this parameter doesn't exist.
 *
 * @param pnErr place to put error code on failure.  Ignored if NULL.
 *
 * @return value of parameter.
 */

double OGRSpatialReference::GetProjParm( const char * pszName,
                                         double dfDefaultValue,
                                         OGRErr *pnErr )

{
    OGR_SRSNode *poPROJCS = GetAttrNode( "PROJCS" );
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
    double      dfValue;
    OGRErr      nSubErr;
    
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
/*                           OSRGetProjParm()                           */
/************************************************************************/

double OSRGetProjParm( OGRSpatialReferenceH hSRS, const char *pszName,
                       double dfDefaultValue, OGRErr *pnErr )

{
    return ((OGRSpatialReference *) hSRS)->
        GetProjParm(pszName, dfDefaultValue, pnErr);
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
/*                               SetTMG()                               */
/************************************************************************/

OGRErr 
OGRSpatialReference::SetTMG( double dfCenterLat, double dfCenterLong,
                             double dfFalseEasting, double dfFalseNorthing )
    
{
    SetProjection( SRS_PT_TUNISIA_MINING_GRID );
    SetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
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
/*                               SetCEA()                               */
/************************************************************************/

OGRErr OGRSpatialReference::SetCEA( double dfStdP1, double dfCentralMeridian,
                                    double dfFalseEasting,
                                    double dfFalseNorthing )

{
    SetProjection( SRS_PT_CYLINDRICAL_EQUAL_AREA );
    SetProjParm( SRS_PP_STANDARD_PARALLEL_1, dfStdP1 );
    SetProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCentralMeridian );
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
/*                            SetEckertIV()                             */
/************************************************************************/

OGRErr OGRSpatialReference::SetEckertIV( double dfCentralMeridian,
                                         double dfFalseEasting,
                                         double dfFalseNorthing )

{
    SetProjection( SRS_PT_ECKERT_IV );
    SetProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCentralMeridian );
    SetProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            SetEckertVI()                             */
/************************************************************************/

OGRErr OGRSpatialReference::SetEckertVI( double dfCentralMeridian,
                                         double dfFalseEasting,
                                         double dfFalseNorthing )

{
    SetProjection( SRS_PT_ECKERT_VI );
    SetProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCentralMeridian );
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
/*                               SetGS()                                */
/************************************************************************/

OGRErr OGRSpatialReference::SetGS( double dfCentralMeridian,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    SetProjection( SRS_PT_GALL_STEREOGRAPHIC );
    SetProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCentralMeridian );
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
/*                             SetLCC1SP()                              */
/************************************************************************/

OGRErr OGRSpatialReference::SetLCC1SP( double dfCenterLat, double dfCenterLong,
                                       double dfScale,
                                       double dfFalseEasting,
                                       double dfFalseNorthing )

{
    SetProjection( SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP );
    SetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetProjParm( SRS_PP_SCALE_FACTOR, dfScale );
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
/*                            SetMollweide()                            */
/************************************************************************/

OGRErr OGRSpatialReference::SetMollweide( double dfCentralMeridian,
                                          double dfFalseEasting,
                                          double dfFalseNorthing )

{
    SetProjection( SRS_PT_MOLLWEIDE );
    SetProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCentralMeridian );
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
    // note: it seems that by some definitions this should include a
    //       scale_factor parameter.
    
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
/*                               SetSOC()                               */
/************************************************************************/

OGRErr OGRSpatialReference::SetSOC( double dfLatitudeOfOrigin, 
                                    double dfCentralMeridian,
                                    double dfFalseEasting,
                                    double dfFalseNorthing )

{
    SetProjection( SRS_PT_SWISS_OBLIQUE_CYLINDRICAL );
    SetProjParm( SRS_PP_LATITUDE_OF_CENTER, dfLatitudeOfOrigin );
    SetProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCentralMeridian );
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

/************************************************************************/
/*                               SetUTM()                               */
/************************************************************************/

/**
 * Set UTM projection definition.
 *
 * This will generate a projection definition with the full set of 
 * transverse mercator projection parameters for the given UTM zone.
 *
 * This method is the same as the C function OSRSetUTM().
 *
 * @param nZone UTM zone.
 *
 * @param bNorth TRUE for northern hemisphere, or FALSE for southern 
 * hemisphere. 
 * 
 * @return OGRERR_NONE on success. 
 */

OGRErr OGRSpatialReference::SetUTM( int nZone, int bNorth )

{
    SetProjection( SRS_PT_TRANSVERSE_MERCATOR );
    SetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0 );
    SetProjParm( SRS_PP_CENTRAL_MERIDIAN, nZone * 6 - 183 );
    SetProjParm( SRS_PP_SCALE_FACTOR, 0.9996 );
    SetProjParm( SRS_PP_FALSE_EASTING, 500000.0 );

    if( bNorth )
        SetProjParm( SRS_PP_FALSE_NORTHING, 0 );
    else
        SetProjParm( SRS_PP_FALSE_NORTHING, 10000000 );

    if( EQUAL(GetAttrValue("PROJCS"),"unnamed") )
    {
        char    szUTMName[128];

        if( bNorth )
            sprintf( szUTMName, "UTM Zone %d, Northern Hemisphere", nZone );
        else
            sprintf( szUTMName, "UTM Zone %d, Southern Hemisphere", nZone );

        SetNode( "PROJCS", szUTMName );
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                             OSRSetUTM()                              */
/************************************************************************/

OGRErr OSRSetUTM( OGRSpatialReferenceH hSRS, int nZone, int bNorth )

{
    return ((OGRSpatialReference *) hSRS)->SetUTM( nZone, bNorth );
}

/************************************************************************/
/*                             GetUTMZone()                             */
/*                                                                      */
/*      Returns zero if it isn't UTM.                                   */
/************************************************************************/

/**
 * Get utm zone information.
 *
 * This is the same as the C function OSRGetUTMZone().
 *
 * @param pbNorth pointer to in to set to TRUE if northern hemisphere, or
 * FALSE if southern. 
 * 
 * @return UTM zone number or zero if this isn't a UTM definition. 
 */

int OGRSpatialReference::GetUTMZone( int * pbNorth )

{
    const char  *pszProjection = GetAttrValue( "PROJECTION" );

    if( pszProjection == NULL
        || !EQUAL(pszProjection,SRS_PT_TRANSVERSE_MERCATOR) )
        return 0;

    if( GetProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) != 0.0 )
        return 0;

    if( GetProjParm( SRS_PP_SCALE_FACTOR, 1.0 ) != 0.9996 )
        return 0;
          
    if( GetProjParm( SRS_PP_FALSE_EASTING, 0.0 ) != 500000 )
        return 0;

    double      dfFalseNorthing = GetProjParm( SRS_PP_FALSE_NORTHING, 0.0 );

    if( dfFalseNorthing != 0.0 && dfFalseNorthing != 10000000 )
        return 0;

    if( pbNorth != NULL )
        *pbNorth = (dfFalseNorthing == 0);

    double      dfCentralMeridian = GetProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0);
    double      dfZone = (dfCentralMeridian+183) / 6.0 + 0.000000001;

    if( ABS(dfZone - (int) dfZone) > 0.00001
        || dfCentralMeridian < -177.00001
        || dfCentralMeridian > 177.000001 )
        return 0;
    else
        return (int) dfZone;
}

/************************************************************************/
/*                           OSRGetUTMZone()                            */
/************************************************************************/

int OSRGetUTMZone( OGRSpatialReferenceH hSRS, int *pbNorth )

{
    return ((OGRSpatialReference *) hSRS)->GetUTMZone( pbNorth );
}

/************************************************************************/
/*                            SetAuthority()                            */
/************************************************************************/

/**
 * Set the authority for a node.
 *
 * This method is the same as the C function OSRSetAuthority().
 *
 * @param pszTargetKey the partial or complete path to the node to 
 * set an authority on.  ie. "PROJCS", "GEOGCS" or "GEOGCS|UNIT".
 *
 * @param pszAuthority authority name, such as "EPSG".
 *
 * @param nCode code for value with this authority.
 *
 * @return OGRERR_NONE on success.
 */

OGRErr OGRSpatialReference::SetAuthority( const char *pszTargetKey,
                                          const char * pszAuthority, 
                                          int nCode )

{
    OGR_SRSNode  *poNode;
    char         **papszNodePath;
    int          iPath;

/* -------------------------------------------------------------------- */
/*      Find the node below which the authority should be put.          */
/* -------------------------------------------------------------------- */
    papszNodePath = CSLTokenizeStringComplex( pszTargetKey,"|",FALSE,FALSE);

    for( poNode = GetRoot(), iPath = 0; 
         poNode != NULL && papszNodePath[iPath] != NULL; 
         iPath++ )
    {
        poNode = poNode->GetNode( papszNodePath[iPath] );
    }

    CSLDestroy( papszNodePath );

    if( poNode == NULL )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      For now we assume there is no authority child.  Eventually      */
/*      we will have to handle this properly.                           */
/* -------------------------------------------------------------------- */
    /* CPLAssert( poNode->GetNode( "AUTHORITY" ) == NULL ); */

/* -------------------------------------------------------------------- */
/*      Create a new authority node.                                    */
/* -------------------------------------------------------------------- */
    char   szCode[32];
    OGR_SRSNode *poAuthNode;

    sprintf( szCode, "%d", nCode );

    poAuthNode = new OGR_SRSNode( "AUTHORITY" );
    poAuthNode->AddChild( new OGR_SRSNode( pszAuthority ) );
    poAuthNode->AddChild( new OGR_SRSNode( szCode ) );
    
    poNode->AddChild( poAuthNode );

    return OGRERR_NONE;
}

/************************************************************************/
/*                          OSRSetAuthority()                           */
/************************************************************************/

OGRErr OSRSetAuthority( OGRSpatialReferenceH hSRS, 
                        const char *pszTargetKey,
                        const char * pszAuthority, 
                        int nCode )

{
    return ((OGRSpatialReference *) hSRS)->SetAuthority( pszTargetKey, 
                                                         pszAuthority,
                                                         nCode );
}

/************************************************************************/
/*                            StripCTParms()                            */
/************************************************************************/

OGRErr OGRSpatialReference::StripCTParms( OGR_SRSNode * poCurrent )

{
    if( poCurrent == NULL )
        poCurrent = GetRoot();
    
    if( poCurrent == NULL )
        return OGRERR_NONE;

    poCurrent->DestroyChild( poCurrent->FindChild( "AUTHORITY" ) );
    poCurrent->DestroyChild( poCurrent->FindChild( "AXIS" ) );
    poCurrent->DestroyChild( poCurrent->FindChild( "TOWGS84" ) );
    
    for( int iChild = 0; iChild < poCurrent->GetChildCount(); iChild++ )
        StripCTParms( poCurrent->GetChild( iChild ) );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            IsProjected()                             */
/************************************************************************/

/**
 * Check if projected coordinate system.
 *
 * This method is the same as the C function OSRIsProjected().
 *
 * @return TRUE if this contains a PROJCS node indicating a it is a 
 * projected coordinate system. 
 */

int OGRSpatialReference::IsProjected() 

{
    return GetAttrNode("PROJCS") != NULL;
}

/************************************************************************/
/*                           OSRIsProjected()                           */
/************************************************************************/

int OSRIsProjected( OGRSpatialReferenceH hSRS )

{
    return ((OGRSpatialReference *) hSRS)->IsProjected();
}

/************************************************************************/
/*                            IsGeographic()                            */
/************************************************************************/

/**
 * Check if geographic coordinate system.
 *
 * This method is the same as the C function OSRIsGeographic().
 *
 * @return TRUE if this spatial reference is geographic ... that is the 
 * root is a GEOGCS node. 
 */

int OGRSpatialReference::IsGeographic() 

{
    if( GetRoot() == NULL )
        return FALSE;

    return EQUAL(GetRoot()->GetValue(),"GEOGCS");
}

/************************************************************************/
/*                          OSRIsGeographic()                           */
/************************************************************************/

int OSRIsGeographic( OGRSpatialReferenceH hSRS )

{
    return ((OGRSpatialReference *) hSRS)->IsGeographic();
}

/************************************************************************/
/*                              IsLocal()                               */
/************************************************************************/

/**
 * Check if local coordinate system.
 *
 * This method is the same as the C function OSRIsLocal().
 *
 * @return TRUE if this spatial reference is local ... that is the 
 * root is a LOCAL_CS node. 
 */

int OGRSpatialReference::IsLocal() 

{
    if( GetRoot() == NULL )
        return FALSE;

    return EQUAL(GetRoot()->GetValue(),"LOCAL_CS");
}

/************************************************************************/
/*                          OSRIsLocal()                                */
/************************************************************************/

int OSRIsLocal( OGRSpatialReferenceH hSRS )

{
    return ((OGRSpatialReference *) hSRS)->IsLocal();
}

/************************************************************************/
/*                            CloneGeogCS()                             */
/************************************************************************/

OGRSpatialReference *OGRSpatialReference::CloneGeogCS()

{
    OGR_SRSNode *poGeogCS;
    OGRSpatialReference * poNewSRS;

    poGeogCS = GetAttrNode( "GEOGCS" );
    if( poGeogCS == NULL )
        return NULL;

    poNewSRS = new OGRSpatialReference();
    poNewSRS->SetRoot( poGeogCS->Clone() );

    return poNewSRS;
}

/************************************************************************/
/*                           OSRCloneGeogCS()                           */
/************************************************************************/

OGRSpatialReferenceH OSRCloneGeogCS( OGRSpatialReferenceH hSource )

{
    return (OGRSpatialReferenceH) 
        ((OGRSpatialReference *) hSource)->CloneGeogCS();
}

/************************************************************************/
/*                            IsSameGeogCS()                            */
/************************************************************************/

/**
 * Do the GeogCS'es match?
 *
 * This method is the same as the C function OSRIsSameGeogCS().
 *
 * @param poOther the SRS being compared against. 
 *
 * @return TRUE if they are the same or FALSE otherwise. 
 */

int OGRSpatialReference::IsSameGeogCS( OGRSpatialReference *poOther )

{
    const char *pszThisValue, *pszOtherValue;

/* -------------------------------------------------------------------- */
/*      Does the datum name match?  Note that we assume                 */
/*      compatibility if either is missing a datum.                     */
/* -------------------------------------------------------------------- */
    pszThisValue = this->GetAttrValue( "DATUM" );
    pszOtherValue = poOther->GetAttrValue( "DATUM" );

    if( pszThisValue != NULL && pszOtherValue != NULL 
        && !EQUAL(pszThisValue,pszOtherValue) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Do the prime meridians match?  If missing assume a value of zero.*/
/* -------------------------------------------------------------------- */
    pszThisValue = this->GetAttrValue( "PRIMEM", 1 );
    if( pszThisValue == NULL )
        pszThisValue = "0.0";

    pszOtherValue = poOther->GetAttrValue( "PRIMEM", 1 );
    if( pszOtherValue == NULL )
        pszOtherValue = "0.0";

    if( atof(pszOtherValue) != atof(pszThisValue) )
        return FALSE;
    
/* -------------------------------------------------------------------- */
/*      Do the units match?                                             */
/* -------------------------------------------------------------------- */
    pszThisValue = this->GetAttrValue( "GEOGCS|UNITS", 1 );
    if( pszThisValue == NULL )
        pszThisValue = SRS_UA_DEGREE_CONV;

    pszOtherValue = poOther->GetAttrValue( "GEOGCS|UNITS", 1 );
    if( pszOtherValue == NULL )
        pszOtherValue = SRS_UA_DEGREE_CONV;

    if( ABS(atof(pszOtherValue) - atof(pszThisValue)) > 0.00000001 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Does the spheroid match.  Check semi major, and inverse         */
/*      flattening.                                                     */
/* -------------------------------------------------------------------- */
    pszThisValue = this->GetAttrValue( "SPHEROID", 1 );
    pszOtherValue = poOther->GetAttrValue( "SPHEROID", 1 );
    if( pszThisValue != NULL && pszOtherValue != NULL 
        && ABS(atof(pszThisValue) - atof(pszOtherValue)) > 0.01 )
        return FALSE;

    pszThisValue = this->GetAttrValue( "SPHEROID", 2 );
    pszOtherValue = poOther->GetAttrValue( "SPHEROID", 2 );
    if( pszThisValue != NULL && pszOtherValue != NULL 
        && ABS(atof(pszThisValue) - atof(pszOtherValue)) > 0.0001 )
        return FALSE;
    
    return TRUE;
}

/************************************************************************/
/*                          OSRIsSameGeogCS()                           */
/************************************************************************/

int OSRIsSameGeogCS( OGRSpatialReferenceH hSRS1, OGRSpatialReferenceH hSRS2 )

{
    return ((OGRSpatialReference *) hSRS1)->IsSameGeogCS( 
        (OGRSpatialReference *) hSRS2 );
}

/************************************************************************/
/*                               IsSame()                               */
/************************************************************************/

/**
 * These two spatial references describe the same system.
 *
 * @param poOtherSRS the SRS being compared to.
 *
 * @return TRUE if equivelent or FALSE otherwise. 
 */

int OGRSpatialReference::IsSame( OGRSpatialReference * poOtherSRS )

{
    if( GetRoot() == NULL && poOtherSRS->GetRoot() == NULL )
        return TRUE;

/* -------------------------------------------------------------------- */
/*      Compare geographic coordinate system.                           */
/* -------------------------------------------------------------------- */
    if( !IsSameGeogCS( poOtherSRS ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Do the have the same root types?  Ie. is one PROJCS and one     */
/*      GEOGCS or perhaps LOCALCS?                                      */
/* -------------------------------------------------------------------- */
    if( EQUAL(GetRoot()->GetValue(),poOtherSRS->GetRoot()->GetValue()) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Compare projected coordinate system.                            */
/* -------------------------------------------------------------------- */
    if( IsProjected() )
    {
        const char *pszValue1, *pszValue2;
        OGR_SRSNode *poPROJCS = GetAttrNode( "PROJCS" );

        pszValue1 = this->GetAttrValue( "PROJECTION" );
        pszValue2 = poOtherSRS->GetAttrValue( "PROJECTION" );
        if( pszValue1 == NULL || pszValue2 == NULL
            || !EQUAL(pszValue1,pszValue2) )
            return FALSE;

        for( int iChild = 0; iChild < poPROJCS->GetChildCount(); iChild++ )
        {
            OGR_SRSNode    *poNode;

            poNode = poPROJCS->GetChild( iChild );
            if( !EQUAL(poNode->GetValue(),"PARAMETER") 
                || poNode->GetChildCount() != 2 )
                continue;

            /* this this eventually test within some epsilon? */
            if( this->GetProjParm( poNode->GetChild(0)->GetValue() )
                != poOtherSRS->GetProjParm( poNode->GetChild(0)->GetValue() ) )
                return FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      If they are LOCALCS/PROJCS, do they have the same units?        */
/* -------------------------------------------------------------------- */
    if( EQUAL(GetRoot()->GetValue(),"LOCALCS") || IsProjected() )
    {
        if( GetLinearUnits() != 0.0 )
        {
            double	dfRatio;

            dfRatio = poOtherSRS->GetLinearUnits() / GetLinearUnits();
            if( dfRatio < 0.9999999999 || dfRatio > 1.000000001 )
                return FALSE;
        }
    }

    return TRUE;
}

/************************************************************************/
/*                             OSRIsSame()                              */
/************************************************************************/

int OSRIsSame( OGRSpatialReferenceH hSRS1, OGRSpatialReferenceH hSRS2 )

{
    return ((OGRSpatialReference *) hSRS1)->IsSame( 
        (OGRSpatialReference *) hSRS2 );
}

/************************************************************************/
/*                             SetTOWGS84()                             */
/************************************************************************/

/**
 * Set the Bursa-Wolf conversion to WGS84. 
 * 
 * This will create the TOWGS84 node as a child of the DATUM.  It will fail
 * if there is no existing DATUM node.  Unlike most OGRSpatialReference
 * methods it will insert itself in the appropriate order, and will replace
 * an existing TOWGS84 node if there is one. 
 *
 * The parameters have the same meaning as EPSG transformation 9606
 * (Position Vector 7-param. transformation). 
 * 
 * This method is the same as the C function OSRSetTOWGS84().
 * 
 * @param dfDX X child in meters.
 * @param dfDY Y child in meters.
 * @param dfDZ Z child in meters.
 * @param dfEX X rotation in arc seconds (optional, defaults to zero).
 * @param dfEY Y rotation in arc seconds (optional, defaults to zero).
 * @param dfEZ Z rotation in arc seconds (optional, defaults to zero).
 * @param dfPPM scaling factor (parts per million).
 * 
 * @return OGRERR_NONE on success. 
 */ 

OGRErr OGRSpatialReference::SetTOWGS84( double dfDX, double dfDY, double dfDZ,
                                        double dfEX, double dfEY, double dfEZ, 
                                        double dfPPM )

{
    OGR_SRSNode     *poDatum, *poTOWGS84;
    int             iPosition;
    char            szValue[64];

    poDatum = GetAttrNode( "DATUM" );
    if( poDatum == NULL )
        return OGRERR_FAILURE;
    
    if( poDatum->FindChild( "TOWGS84" ) != -1 )
        poDatum->DestroyChild( poDatum->FindChild( "TOWGS84" ) );

    iPosition = poDatum->GetChildCount();
    if( poDatum->FindChild("AUTHORITY") != -1 )
    {
        iPosition = poDatum->FindChild("AUTHORITY");
    }

    poTOWGS84 = new OGR_SRSNode("TOWGS84");

    OGRPrintDouble( szValue, dfDX );
    poTOWGS84->AddChild( new OGR_SRSNode( szValue ) );

    OGRPrintDouble( szValue, dfDY );
    poTOWGS84->AddChild( new OGR_SRSNode( szValue ) );

    OGRPrintDouble( szValue, dfDZ );
    poTOWGS84->AddChild( new OGR_SRSNode( szValue ) );

    OGRPrintDouble( szValue, dfEX );
    poTOWGS84->AddChild( new OGR_SRSNode( szValue ) );

    OGRPrintDouble( szValue, dfEY );
    poTOWGS84->AddChild( new OGR_SRSNode( szValue ) );

    OGRPrintDouble( szValue, dfEZ );
    poTOWGS84->AddChild( new OGR_SRSNode( szValue ) );

    OGRPrintDouble( szValue, dfPPM );
    poTOWGS84->AddChild( new OGR_SRSNode( szValue ) );

    poDatum->InsertChild( poTOWGS84, iPosition );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           OSRSetTOWGS84()                            */
/************************************************************************/

OGRErr OSRSetTOWGS84( OGRSpatialReferenceH hSRS, 
                      double dfDX, double dfDY, double dfDZ, 
                      double dfEX, double dfEY, double dfEZ, 
                      double dfPPM )

{
    return ((OGRSpatialReference *) hSRS)->SetTOWGS84( dfDX, dfDY, dfDZ, 
                                                       dfEX, dfEY, dfEZ, 
                                                       dfPPM );
}

/************************************************************************/
/*                             GetTOWGS84()                             */
/************************************************************************/

/**
 * Fetch TOWGS84 parameters, if available. 
 * 
 * @param padfCoeff array into which up to 7 coefficients are placed.
 * @param nCoeffCount size of padfCoeff - defaults to 7.
 * 
 * @return OGRERR_NONE on success, or OGRERR_FAILURE if there is no
 * TOWGS84 node available. 
 */

OGRErr OGRSpatialReference::GetTOWGS84( double * padfCoeff, int nCoeffCount )

{
    OGR_SRSNode   *poNode = GetAttrNode( "TOWGS84" );

    memset( padfCoeff, 0, sizeof(double) * nCoeffCount );

    if( poNode == NULL )
        return OGRERR_FAILURE;

    for( int i = 0; i < nCoeffCount && i < poNode->GetChildCount(); i++ )
    {
        padfCoeff[i] = atof(poNode->GetChild(i)->GetValue());
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           OSRGetTOWGS84()                            */
/************************************************************************/

OGRErr OSRGetTOWGS84( OGRSpatialReferenceH hSRS, 
                      double * padfCoeff, int nCoeffCount )

{
    return ((OGRSpatialReference *) hSRS)->GetTOWGS84( padfCoeff, nCoeffCount);
}
