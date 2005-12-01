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
 * Revision 1.97  2005/12/01 04:59:46  fwarmerdam
 * added two point equidistant support
 *
 * Revision 1.96  2005/10/16 01:32:41  fwarmerdam
 * Apply epsilon testing to UTM zone false easting and northing.
 *
 * Revision 1.95  2005/09/21 03:00:42  fwarmerdam
 * fixed return for Release
 *
 * Revision 1.94  2005/09/21 00:51:27  fwarmerdam
 * added Release
 *
 * Revision 1.93  2005/04/06 00:02:05  fwarmerdam
 * various osr and oct functions now stdcall
 *
 * Revision 1.92  2005/03/03 04:55:42  fwarmerdam
 * make exportToWkt() const
 *
 * Revision 1.91  2005/02/11 14:21:28  fwarmerdam
 * added GEOS projection support
 *
 * Revision 1.90  2005/01/13 05:17:37  fwarmerdam
 * added SetLinearUnitsAndUpdateParameters
 *
 * Revision 1.89  2005/01/05 21:02:33  fwarmerdam
 * added Goode Homolosine
 *
 * Revision 1.88  2004/11/11 18:28:45  fwarmerdam
 * added Bonne projection support
 *
 * Revision 1.87  2004/09/23 16:20:13  fwarmerdam
 * added OSRCleanup
 *
 * Revision 1.86  2004/09/10 20:59:06  fwarmerdam
 * Added note on SetSOC() being deprecated.
 *
 * Revision 1.85  2004/05/06 19:26:04  dron
 * Added OSRSetProjection() function.
 *
 * Revision 1.84  2004/05/04 17:54:45  warmerda
 * internal longitude format is greenwich relative - no adjustments needed
 *
 * Revision 1.83  2004/03/04 18:04:45  warmerda
 * added importFromDict() support
 *
 * Revision 1.82  2004/02/05 17:07:59  dron
 * Support for HOM projection, specified by two points on centerline.
 *
 * Revision 1.81  2003/12/05 16:22:49  warmerda
 * optimized IsProjected
 *
 * Revision 1.80  2003/10/07 04:20:50  warmerda
 * added WMS AUTO: support
 *
 * Revision 1.79  2003/09/18 14:43:40  warmerda
 * Ensure that SetAuthority() clears old nodes.
 * Don't crash on NULL root in exportToPrettyWkt().
 *
 * Revision 1.78  2003/08/18 13:26:01  warmerda
 * added SetTMVariant() and related definitions
 *
 * Revision 1.77  2003/06/19 17:10:26  warmerda
 * a couple fixes in last commit
 *
 * Revision 1.76  2003/06/18 18:24:17  warmerda
 * added projection specific set methods to C API
 *
 * Revision 1.75  2003/05/30 18:34:41  warmerda
 * clear existing authority node in SetLinearUnits if one exists
 *
 * Revision 1.74  2003/05/28 19:16:43  warmerda
 * fixed up argument names and stuff for docs
 *
 * Revision 1.73  2003/04/01 14:34:45  warmerda
 * Clarify SetUTM() documentation.
 *
 * Revision 1.72  2003/03/28 17:42:05  warmerda
 * fixed reference/dereference problem
 *
 * Revision 1.71  2003/02/25 04:55:41  warmerda
 * Added SetGeogCSFrom() method. Modified SetWellKnownGeogCS() to use it.
 * Modfied SetGeogCS() to replace an existing GEOGCS node if there is one.
 *
 * Revision 1.70  2003/02/14 22:15:04  warmerda
 * expand tabs
 *
 * Revision 1.69  2003/02/08 00:37:15  warmerda
 * try to improve documentation
 *
 * Revision 1.68  2003/02/06 04:53:12  warmerda
 * added Fixup() method
 *
 * Revision 1.67  2003/01/31 02:27:08  warmerda
 * modified SetFromUserInput() to avoid large buffer on stack
 *
 * Revision 1.66  2003/01/08 18:14:28  warmerda
 * added FixupOrdering()
 */

#include "ogr_spatialref.h"
#include "ogr_p.h"
#include "cpl_csv.h"

CPL_CVSID("$Id$");

// The current opinion is that WKT longitudes like central meridian
// should be relative to greenwich, not the prime meridian in use. 
// Define the following if they should be relative to the prime meridian
// of then geogcs.
#undef WKT_LONGITUDE_RELATIVE_TO_PM

/************************************************************************/
/*                           OGRPrintDouble()                           */
/************************************************************************/

static void OGRPrintDouble( char * pszStrBuf, double dfValue )

{
    sprintf( pszStrBuf, "%.16g", dfValue );

    int nLen = strlen(pszStrBuf);

    // The following hack is intended to truncate some "precision" in cases
    // that appear to be roundoff error. 
    if( nLen > 15 
        && (strcmp(pszStrBuf+nLen-6,"999999") == 0
            || strcmp(pszStrBuf+nLen-6,"000001") == 0) )
    {
        sprintf( pszStrBuf, "%.15g", dfValue );
    }
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
    bNormInfoSet = FALSE;
    nRefCount = 1;
    poRoot = NULL;

    if( pszWKT != NULL )
        importFromWkt( (char **) &pszWKT );
}

/************************************************************************/
/*                       OSRNewSpatialReference()                       */
/************************************************************************/

OGRSpatialReferenceH CPL_STDCALL OSRNewSpatialReference( const char *pszWKT )

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
    bNormInfoSet = FALSE;
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

void CPL_STDCALL OSRDestroySpatialReference( OGRSpatialReferenceH hSRS )

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
    return ((OGRSpatialReference *) hSRS)->Dereference();
}

/************************************************************************/
/*                         GetReferenceCount()                          */
/************************************************************************/

/**
 * \fn int OGRSpatialReference::GetReferenceCount() const;
 *
 * Fetch current reference count.
 *
 * @return the current reference count.
 */

/************************************************************************/
/*                              Release()                               */
/************************************************************************/

/**
 * Decrements the reference count by one, and destroy if zero.
 *
 * The method does the same thing as the C function OSRRelease(). 
 */

void OGRSpatialReference::Release()

{
    if( this && Dereference() == 0 )
        delete this;
}

/************************************************************************/
/*                             OSRRelease()                             */
/************************************************************************/

void OSRRelease( OGRSpatialReferenceH hSRS )

{
    ((OGRSpatialReference *) hSRS)->Release();
}

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

const OGR_SRSNode *
OGRSpatialReference::GetAttrNode( const char * pszNodePath ) const

{
    OGR_SRSNode *poNode;

    poNode = ((OGRSpatialReference *) this)->GetAttrNode(pszNodePath);

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
                                               int iAttr ) const

{
    const OGR_SRSNode *poNode;

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

const char * CPL_STDCALL OSRGetAttrValue( OGRSpatialReferenceH hSRS,
                             const char * pszKey, int iChild )

{
    return ((OGRSpatialReference *) hSRS)->GetAttrValue( pszKey, iChild );
}

/************************************************************************/
/*                               Clone()                                */
/************************************************************************/

/**
 * Make a duplicate of this OGRSpatialReference.
 *
 * This method is the same as the C function OSRClone().
 *
 * @return a new SRS, which becomes the responsibility of the caller.
 */

OGRSpatialReference *OGRSpatialReference::Clone() const

{
    OGRSpatialReference *poNewRef;

    poNewRef = new OGRSpatialReference();

    if( poRoot != NULL )
        poNewRef->poRoot = poRoot->Clone();

    return poNewRef;
}

/************************************************************************/
/*                              OSRClone()                              */
/************************************************************************/

OGRSpatialReferenceH CPL_STDCALL OSRClone( OGRSpatialReferenceH hSRS )

{
    return (OGRSpatialReferenceH) ((OGRSpatialReference *) hSRS)->Clone();
}

/************************************************************************/
/*                         exportToPrettyWkt()                          */
/*                                                                      */
/*      Translate into a nicely formatted string for display to a       */
/*      person.                                                         */
/************************************************************************/

OGRErr OGRSpatialReference::exportToPrettyWkt( char ** ppszResult, 
                                               int bSimplify ) const

{
    if( poRoot == NULL )
    {
        *ppszResult = CPLStrdup("");
        return OGRERR_NONE;
    }

    if( bSimplify )
    {
        OGRSpatialReference *poSimpleClone = Clone();
        OGRErr eErr;

        poSimpleClone->GetRoot()->StripNodes( "AXIS" );
        poSimpleClone->GetRoot()->StripNodes( "AUTHORITY" );
        eErr = poSimpleClone->GetRoot()->exportToPrettyWkt( ppszResult, 1 );
        delete poSimpleClone;
        return eErr;
    }
    else
        return poRoot->exportToPrettyWkt( ppszResult, 1 );
}

/************************************************************************/
/*                        OSRExportToPrettyWkt()                        */
/************************************************************************/

OGRErr CPL_STDCALL OSRExportToPrettyWkt( OGRSpatialReferenceH hSRS, char ** ppszReturn,
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
 
OGRErr  OGRSpatialReference::exportToWkt( char ** ppszResult ) const

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

OGRErr CPL_STDCALL OSRExportToWkt( OGRSpatialReferenceH hSRS, char ** ppszReturn )

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

    bNormInfoSet = FALSE;

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

OGRErr CPL_STDCALL OSRSetAttrValue( OGRSpatialReferenceH hSRS, 
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
        // notdef: sprintf( szValue, "%.16g", dfValue );
        OGRPrintDouble( szValue, dfValue );

    return SetNode( pszNodePath, szValue );
}

/************************************************************************/
/*                          SetAngularUnits()                           */
/************************************************************************/

/**
 * Set the angular units for the geographic coordinate system.
 *
 * This method creates a UNITS subnode with the specified values as a
 * child of the GEOGCS node. 
 *
 * This method does the same as the C function OSRSetAngularUnits(). 
 *
 * @param pszUnitsName the units name to be used.  Some preferred units
 * names can be found in ogr_srs_api.h such as SRS_UA_DEGREE. 
 *
 * @param dfInRadians the value to multiple by an angle in the indicated
 * units to transform to radians.  Some standard conversion factors can
 * be found in ogr_srs_api.h. 
 *
 * @return OGRERR_NONE on success.
 */

OGRErr OGRSpatialReference::SetAngularUnits( const char * pszUnitsName,
                                             double dfInRadians )

{
    OGR_SRSNode *poCS;
    OGR_SRSNode *poUnits;
    char        szValue[128];

    bNormInfoSet = FALSE;

    poCS = GetAttrNode( "GEOGCS" );

    if( poCS == NULL )
        return OGRERR_FAILURE;

    OGRPrintDouble( szValue, dfInRadians );

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
/*                         OSRSetAngularUnits()                         */
/************************************************************************/

OGRErr OSRSetAngularUnits( OGRSpatialReferenceH hSRS, 
                           const char * pszUnits, double dfInRadians )

{
    return ((OGRSpatialReference *) hSRS)->SetAngularUnits( pszUnits, 
                                                            dfInRadians );
}

/************************************************************************/
/*                          GetAngularUnits()                           */
/************************************************************************/

/**
 * Fetch angular geographic coordinate system units.
 *
 * If no units are available, a value of "degree" and SRS_UA_DEGREE_CONV 
 * will be assumed.  This method only checks directly under the GEOGCS node
 * for units.
 *
 * This method does the same thing as the C function OSRGetAngularUnits().
 *
 * @param ppszName a pointer to be updated with the pointer to the 
 * units name.  The returned value remains internal to the OGRSpatialReference
 * and shouldn't be freed, or modified.  It may be invalidated on the next
 * OGRSpatialReference call. 
 *
 * @return the value to multiply by angular distances to transform them to 
 * radians.
 */

double OGRSpatialReference::GetAngularUnits( char ** ppszName ) const

{
    const OGR_SRSNode *poCS = GetAttrNode( "GEOGCS" );

    if( ppszName != NULL )
        *ppszName = "degree";
        
    if( poCS == NULL )
        return atof(SRS_UA_DEGREE_CONV);

    for( int iChild = 0; iChild < poCS->GetChildCount(); iChild++ )
    {
        const OGR_SRSNode     *poChild = poCS->GetChild(iChild);
        
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
/*                         OSRGetAngularUnits()                         */
/************************************************************************/

double OSRGetAngularUnits( OGRSpatialReferenceH hSRS, char ** ppszName )
    
{
    return ((OGRSpatialReference *) hSRS)->GetAngularUnits( ppszName );
}

/************************************************************************/
/*                 SetLinearUnitsAndUpdateParameters()                  */
/************************************************************************/

/**
 * Set the linear units for the projection.
 *
 * This method creates a UNITS subnode with the specified values as a
 * child of the PROJCS or LOCAL_CS node.   It works the same as the
 * SetLinearUnits() method, but it also updates all existing linear
 * projection parameter values from the old units to the new units. 
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

OGRErr OGRSpatialReference::SetLinearUnitsAndUpdateParameters(
    const char *pszName, double dfInMeters )

{
    double dfOldInMeters = GetLinearUnits();
    OGR_SRSNode *poPROJCS = GetAttrNode( "PROJCS" );

    if( dfInMeters == 0.0 )
        return OGRERR_FAILURE;

    if( dfInMeters == dfOldInMeters || poPROJCS == NULL )
        return SetLinearUnits( pszName, dfInMeters );

    for( int iChild = 0; iChild < poPROJCS->GetChildCount(); iChild++ )
    {
        const OGR_SRSNode     *poChild = poPROJCS->GetChild(iChild);
        
        if( EQUAL(poChild->GetValue(),"PARAMETER")
            && poChild->GetChildCount() > 1 )
        {
            char *pszParmName = CPLStrdup(poChild->GetChild(0)->GetValue());
            
            if( IsLinearParameter( pszParmName ) )
            {
                double dfOldValue = GetProjParm( pszParmName );

                SetProjParm( pszParmName, 
                             dfOldValue * dfOldInMeters / dfInMeters );
            }

            CPLFree( pszParmName );
        }
    }

    return SetLinearUnits( pszName, dfInMeters );
}

/************************************************************************/
/*                           SetLinearUnits()                           */
/************************************************************************/

/**
 * Set the linear units for the projection.
 *
 * This method creates a UNITS subnode with the specified values as a
 * child of the PROJCS or LOCAL_CS node. 
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

    bNormInfoSet = FALSE;

    poCS = GetAttrNode( "PROJCS" );
    if( poCS == NULL )
        poCS = GetAttrNode( "LOCAL_CS" );

    if( poCS == NULL )
        return OGRERR_FAILURE;

    if( dfInMeters == (int) dfInMeters )
        sprintf( szValue, "%d", (int) dfInMeters );
    else
        //notdef: sprintf( szValue, "%.16g", dfInMeters );
        OGRPrintDouble( szValue, dfInMeters );

    if( poCS->FindChild( "UNIT" ) >= 0 )
    {
        poUnits = poCS->GetChild( poCS->FindChild( "UNIT" ) );
        poUnits->GetChild(0)->SetValue( pszUnitsName );
        poUnits->GetChild(1)->SetValue( szValue );
        if( poUnits->FindChild( "AUTHORITY" ) != -1 )
            poUnits->DestroyChild( poUnits->FindChild( "AUTHORITY" ) );
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

double OGRSpatialReference::GetLinearUnits( char ** ppszName ) const

{
    const OGR_SRSNode *poCS = GetAttrNode( "PROJCS" );

    if( poCS == NULL )
        poCS = GetAttrNode( "LOCAL_CS" );

    if( ppszName != NULL )
        *ppszName = "unknown";
        
    if( poCS == NULL )
        return 1.0;

    for( int iChild = 0; iChild < poCS->GetChildCount(); iChild++ )
    {
        const OGR_SRSNode     *poChild = poCS->GetChild(iChild);
        
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
/*                          GetPrimeMeridian()                          */
/************************************************************************/

/**
 * Fetch prime meridian info.
 *
 * Returns the offset of the prime meridian from greenwich in degrees,
 * and the prime meridian name (if requested).   If no PRIMEM value exists
 * in the coordinate system definition a value of "Greenwich" and an
 * offset of 0.0 is assumed.
 *
 * If the prime meridian name is returned, the pointer is to an internal
 * copy of the name. It should not be freed, altered or depended on after
 * the next OGR call.
 *
 * This method is the same as the C function OSRGetPrimeMeridian().
 *
 * @param ppszName return location for prime meridian name.  If NULL, name
 * is not returned.
 *
 * @return the offset to the GEOGCS prime meridian from greenwich in decimal
 * degrees.
 */

double OGRSpatialReference::GetPrimeMeridian( char **ppszName ) const 

{
    const OGR_SRSNode *poPRIMEM = GetAttrNode( "PRIMEM" );

    if( poPRIMEM != NULL && poPRIMEM->GetChildCount() >= 2 
        && atof(poPRIMEM->GetChild(1)->GetValue()) != 0.0 )
    {
        if( ppszName != NULL )
            *ppszName = (char *) poPRIMEM->GetChild(0)->GetValue();
        return atof(poPRIMEM->GetChild(1)->GetValue());
    }
    
    if( ppszName != NULL )
        *ppszName = SRS_PM_GREENWICH;

    return 0.0;
}

/************************************************************************/
/*                        OSRGetPrimeMeridian()                         */
/************************************************************************/

double OSRGetPrimeMeridian( OGRSpatialReferenceH hSRS, char **ppszName )

{
    return ((OGRSpatialReference *) hSRS)->GetPrimeMeridian( ppszName );
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
 * 1/f = 1.0 / (1.0 - semiminor/semimajor).
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
    bNormInfoSet = FALSE;

/* -------------------------------------------------------------------- */
/*      Do we already have a GEOGCS?  If so, blow it away so it can     */
/*      be properly replaced.                                           */
/* -------------------------------------------------------------------- */
    if( GetAttrNode( "GEOGCS" ) != NULL )
    {
        OGR_SRSNode *poPROJCS;

        if( EQUAL(GetRoot()->GetValue(),"GEOGCS") )
            Clear();
        else if( (poPROJCS = GetAttrNode( "PROJCS" )) != NULL
                 && poPROJCS->FindChild( "GEOGCS" ) != -1 )
            poPROJCS->DestroyChild( poPROJCS->FindChild( "GEOGCS" ) );
        else
            return OGRERR_FAILURE;
    }

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

    OGRPrintDouble( szValue, dfSemiMajor );
    poSpheroid->AddChild( new OGR_SRSNode(szValue) );

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
    OGRSpatialReference   oSRS2;
    OGRErr eErr;

/* -------------------------------------------------------------------- */
/*      Check for EPSG authority numbers.                               */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszName, "EPSG:",5) )
    {
        eErr = oSRS2.importFromEPSG( atoi(pszName+5) );
        if( eErr != OGRERR_NONE )
            return eErr;

        if( !oSRS2.IsGeographic() )
            return OGRERR_FAILURE;

        return CopyGeogCSFrom( &oSRS2 );
    }

/* -------------------------------------------------------------------- */
/*      Check for simple names.                                         */
/* -------------------------------------------------------------------- */
    char         *pszWKT = NULL;

    if( EQUAL(pszName, "WGS84") )
        pszWKT = "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9108\"]],AXIS[\"Lat\",NORTH],AXIS[\"Long\",EAST],AUTHORITY[\"EPSG\",\"4326\"]]";

    else if( EQUAL(pszName, "WGS72") )
        pszWKT = "GEOGCS[\"WGS 72\",DATUM[\"WGS_1972\",SPHEROID[\"WGS 72\",6378135,298.26,AUTHORITY[\"EPSG\",\"7043\"]],TOWGS84[0,0,4.5,0,0,0.554,0.2263],AUTHORITY[\"EPSG\",\"6322\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9108\"]],AXIS[\"Lat\",NORTH],AXIS[\"Long\",EAST],AUTHORITY[\"EPSG\",\"4322\"]]";

    else if( EQUAL(pszName, "NAD27") )
        pszWKT = "GEOGCS[\"NAD27\",DATUM[\"North_American_Datum_1927\",SPHEROID[\"Clarke 1866\",6378206.4,294.978698213898,AUTHORITY[\"EPSG\",\"7008\"]],TOWGS84[-3,142,183,0,0,0,0],AUTHORITY[\"EPSG\",\"6267\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9108\"]],AXIS[\"Lat\",NORTH],AXIS[\"Long\",EAST],AUTHORITY[\"EPSG\",\"4267\"]]";
        
    else if( EQUAL(pszName, "NAD83") )
        pszWKT = "GEOGCS[\"NAD83\",DATUM[\"North_American_Datum_1983\",SPHEROID[\"GRS 1980\",6378137,298.257222101,AUTHORITY[\"EPSG\",\"7019\"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",\"6269\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9108\"]],AXIS[\"Lat\",NORTH],AXIS[\"Long\",EAST],AUTHORITY[\"EPSG\",\"4269\"]]";

    else
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Import the WKT                                                  */
/* -------------------------------------------------------------------- */
    eErr = oSRS2.importFromWkt( &pszWKT );
    if( eErr != OGRERR_NONE )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Copy over.                                                      */
/* -------------------------------------------------------------------- */
    return CopyGeogCSFrom( &oSRS2 );
}

/************************************************************************/
/*                       OSRSetWellKnownGeogCS()                        */
/************************************************************************/

OGRErr OSRSetWellKnownGeogCS( OGRSpatialReferenceH hSRS, const char *pszName )

{
    return ((OGRSpatialReference *) hSRS)->SetWellKnownGeogCS( pszName );
}

/************************************************************************/
/*                           CopyGeogCSFrom()                           */
/************************************************************************/

/**
 * Copy GEOGCS from another OGRSpatialReference.
 *
 * The GEOGCS information is copied into this OGRSpatialReference from another.
 * If this object has a PROJCS root already, the GEOGCS is installed within
 * it, otherwise it is installed as the root.
 * 
 * @param poSrcSRS the spatial reference to copy the GEOGCS information from.
 * 
 * @return OGRERR_NONE on success or an error code.
 */


OGRErr OGRSpatialReference::CopyGeogCSFrom( 
    const OGRSpatialReference * poSrcSRS )

{
    const OGR_SRSNode  *poGeogCS = NULL;

    bNormInfoSet = FALSE;

/* -------------------------------------------------------------------- */
/*      Do we already have a GEOGCS?  If so, blow it away so it can     */
/*      be properly replaced.                                           */
/* -------------------------------------------------------------------- */
    if( GetAttrNode( "GEOGCS" ) != NULL )
    {
        OGR_SRSNode *poPROJCS;

        if( EQUAL(GetRoot()->GetValue(),"GEOGCS") )
            Clear();
        else if( (poPROJCS = GetAttrNode( "PROJCS" )) != NULL
                 && poPROJCS->FindChild( "GEOGCS" ) != -1 )
            poPROJCS->DestroyChild( poPROJCS->FindChild( "GEOGCS" ) );
        else
            return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Find the GEOGCS node on the source.                             */
/* -------------------------------------------------------------------- */
    poGeogCS = poSrcSRS->GetAttrNode( "GEOGCS" );
    if( poGeogCS == NULL )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Attach below the PROJCS if there is one, or make this the root. */
/* -------------------------------------------------------------------- */
    if( GetRoot() != NULL && EQUAL(GetRoot()->GetValue(),"PROJCS") )
        poRoot->InsertChild( poGeogCS->Clone(), 1 );
    else
        SetRoot( poGeogCS->Clone() );

    return OGRERR_NONE;
}

/************************************************************************/
/*                         OSRCopyGeogCSFrom()                          */
/************************************************************************/

OGRErr OSRCopyGeogCSFrom( OGRSpatialReferenceH hSRS, 
                          OGRSpatialReferenceH hSrcSRS )

{
    return ((OGRSpatialReference *) hSRS)->CopyGeogCSFrom( 
        (const OGRSpatialReference *) hSrcSRS );
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
 * <li> "AUTO:proj_id,unit_id,lon0,lat0" - WMS auto projections.
 * <li> PROJ.4 definitions - passed on to importFromProj4().
 * <li> filename - file read for WKT, XML or PROJ.4 definition.
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
    int     bESRI = FALSE;
    OGRErr  err;

    if( EQUALN(pszDefinition,"ESRI::",6) )
    {
        bESRI = TRUE;
        pszDefinition += 6;
    }

/* -------------------------------------------------------------------- */
/*      Is it a recognised syntax?                                      */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszDefinition,"PROJCS",6)
        || EQUALN(pszDefinition,"GEOGCS",6)
        || EQUALN(pszDefinition,"LOCAL_CS",8) )
    {
        err = importFromWkt( (char **) &pszDefinition );
        if( err == OGRERR_NONE && bESRI )
            err = morphFromESRI();

        return err;
    }

    if( EQUALN(pszDefinition,"EPSG:",5) )
        return importFromEPSG( atoi(pszDefinition+5) );

    if( EQUALN(pszDefinition,"AUTO:",5) )
        return importFromWMSAUTO( pszDefinition );

    if( EQUALN(pszDefinition,"DICT:",5) 
        && strstr(pszDefinition,",") )
    {
        char *pszFile = CPLStrdup(pszDefinition+5);
        char *pszCode = strstr(pszFile,",") + 1;
        
        pszCode[-1] = '\0';

        err = importFromDict( pszFile, pszCode );
        CPLFree( pszFile );

        return err;
    }

    if( EQUAL(pszDefinition,"NAD27") 
        || EQUAL(pszDefinition,"NAD83") 
        || EQUAL(pszDefinition,"WGS84") 
        || EQUAL(pszDefinition,"WGS72") )
    {
        Clear();
        return SetWellKnownGeogCS( pszDefinition );
    }

    if( strstr(pszDefinition,"+proj") != NULL 
             || strstr(pszDefinition,"+init") != NULL )
        return importFromProj4( pszDefinition );

/* -------------------------------------------------------------------- */
/*      Try to open it as a file.                                       */
/* -------------------------------------------------------------------- */
    FILE        *fp;
    int         nBufMax = 100000;
    char        *pszBufPtr, *pszBuffer;
    int         nBytes;

    fp = VSIFOpen( pszDefinition, "rt" );
    if( fp == NULL )
        return OGRERR_CORRUPT_DATA;

    pszBuffer = (char *) CPLMalloc(nBufMax);
    nBytes = VSIFRead( pszBuffer, 1, nBufMax-1, fp );
    VSIFClose( fp );

    if( nBytes == nBufMax-1 )
    {
        CPLDebug( "OGR", 
                  "OGRSpatialReference::SetFromUserInput(%s), opened file\n"
                  "but it is to large for our generous buffer.  Is it really\n"
                  "just a WKT definition?" );
        CPLFree( pszBuffer );
        return OGRERR_FAILURE;
    }

    pszBuffer[nBytes] = '\0';

    pszBufPtr = pszBuffer;
    while( pszBufPtr[0] == ' ' || pszBufPtr[0] == '\n' )
        pszBufPtr++;

    if( pszBufPtr[0] == '<' )
        err = importFromXML( pszBufPtr );
    else if( strstr(pszBuffer,"+proj") != NULL 
             || strstr(pszBuffer,"+init") != NULL )
        err = importFromProj4( pszBufPtr );
    else
    {
        err = importFromWkt( &pszBufPtr );
        if( err == OGRERR_NONE && bESRI )
            err = morphFromESRI();
    }

    CPLFree( pszBuffer );

    return err;
}

/************************************************************************/
/*                        OSRSetFromUserInput()                         */
/************************************************************************/

OGRErr CPL_STDCALL OSRSetFromUserInput( OGRSpatialReferenceH hSRS, 
                                        const char *pszDef )

{
    return ((OGRSpatialReference *) hSRS)->SetFromUserInput( pszDef );
}

/************************************************************************/
/*                         importFromWMSAUTO()                          */
/************************************************************************/

OGRErr OGRSpatialReference::importFromWMSAUTO( const char * pszDefinition )

{
    char **papszTokens;
    int nProjId, nUnitsId;
    double dfRefLong, dfRefLat;
    
/* -------------------------------------------------------------------- */
/*      Tokenize                                                        */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszDefinition,"AUTO:",5) )
        pszDefinition += 5;

    papszTokens = CSLTokenizeStringComplex( pszDefinition, ",", FALSE, TRUE );

    if( CSLCount(papszTokens) != 4 )
    {
        CSLDestroy( papszTokens );
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "AUTO projection has wrong number of arguments, expected\n"
                  "AUTO:proj_id,units_id,ref_long,ref_lat" );
        return OGRERR_FAILURE;
    }

    nProjId = atoi(papszTokens[0]);
    nUnitsId = atoi(papszTokens[1]);
    dfRefLong = atof(papszTokens[2]);
    dfRefLat = atof(papszTokens[3]);

    CSLDestroy( papszTokens );

/* -------------------------------------------------------------------- */
/*      Build coordsys.                                                 */
/* -------------------------------------------------------------------- */
    Clear();

    switch( nProjId )
    {
      case 42001: // Auto UTM
        SetUTM( (int) floor( (dfRefLong + 180.0) / 6.0 ) + 1, 
                dfRefLat >= 0.0 );
        break;

      case 42002: // Auto TM (strangely very UTM-like).
        SetTM( 0, dfRefLong, 0.9996, 
               500000.0, (dfRefLat >= 0.0) ? 0.0 : 10000000.0 );
        break;

      case 42003: // Auto Orthographic.
        SetOrthographic( dfRefLat, dfRefLong, 0.0, 0.0 );
        break;

      case 42004: // Auto Equirectangular
        SetEquirectangular( dfRefLat, dfRefLong, 0.0, 0.0 );
        break;

      case 42005:
        SetMollweide( dfRefLong, 0.0, 0.0 );
        break;

      default:
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unsupported projection id in importFromWMSAUTO(): %d", 
                  nProjId );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Set units.                                                      */
/* -------------------------------------------------------------------- */

    switch( nUnitsId )
    {
      case 9001:
        SetLinearUnits( SRS_UL_METER, 1.0 );
        break;

      case 9002:
        SetLinearUnits( "Foot", 0.3048 );
        break;

      case 9003:
        SetLinearUnits( "US survey foot", 0.304800609601 );
        break;

      default:
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unsupported units code (%d).", 
                  nUnitsId );
        return OGRERR_FAILURE;
        break;
    }
    
    SetAuthority( "PROJCS|UNIT", "EPSG", nUnitsId );

/* -------------------------------------------------------------------- */
/*      Set WGS84.                                                      */
/* -------------------------------------------------------------------- */
    SetWellKnownGeogCS( "WGS84" );

    return OGRERR_NONE;
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

double OGRSpatialReference::GetSemiMajor( OGRErr * pnErr ) const

{
    const OGR_SRSNode *poSpheroid = GetAttrNode( "SPHEROID" );
    
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

double OGRSpatialReference::GetInvFlattening( OGRErr * pnErr ) const

{
    const OGR_SRSNode *poSpheroid = GetAttrNode( "SPHEROID" );
    
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

double OGRSpatialReference::GetSemiMinor( OGRErr * pnErr ) const

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

/**
 * Set a projection name.
 *
 * This method is the same as the C function OSRSetProjection().
 *
 * @param pszProjection the projection name, which should be selected from
 * the macros in ogr_srs_api.h, such as SRS_PT_TRANSVERSE_MERCATOR. 
 *
 * @return OGRERR_NONE on success.
 */

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
/*                            OSRSetProjection()                        */
/************************************************************************/

OGRErr OSRSetProjection( OGRSpatialReferenceH hSRS,
                         const char * pszProjection )

{
    return ((OGRSpatialReference *) hSRS)->SetProjection( pszProjection );
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

    if( poPROJCS == NULL )
        return OGRERR_FAILURE;

    OGRPrintDouble( szValue, dfValue );

/* -------------------------------------------------------------------- */
/*      Try to find existing parameter with this name.                  */
/* -------------------------------------------------------------------- */
    for( int iChild = 0; iChild < poPROJCS->GetChildCount(); iChild++ )
    {
        poParm = poPROJCS->GetChild( iChild );

        if( EQUAL(poParm->GetValue(),"PARAMETER")
            && poParm->GetChildCount() == 2 
            && EQUAL(poParm->GetChild(0)->GetValue(),pszParmName) )
        {
            poParm->GetChild(1)->SetValue( szValue );
            return OGRERR_NONE;
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Otherwise create a new parameter and append.                    */
/* -------------------------------------------------------------------- */
    poParm = new OGR_SRSNode( "PARAMETER" );
    poParm->AddChild( new OGR_SRSNode( pszParmName ) );
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
                                         OGRErr *pnErr ) const

{
    const OGR_SRSNode *poPROJCS = GetAttrNode( "PROJCS" );
    const OGR_SRSNode *poParameter = NULL;

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
/*                          GetNormProjParm()                           */
/************************************************************************/

/**
 * Fetch a normalized projection parameter value.                      
 *
 * This method is the same as GetProjParm() except that the value of
 * the parameter is "normalized" into degrees or meters depending on 
 * whether it is linear or angular.
 *
 * This method is the same as the C function OSRGetNormProjParm().
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

double OGRSpatialReference::GetNormProjParm( const char * pszName,
                                             double dfDefaultValue,
                                             OGRErr *pnErr ) const

{
    double dfRawResult;
    OGRErr nError;

    if( pnErr == NULL )
        pnErr = &nError;

    GetNormInfo();

    dfRawResult = GetProjParm( pszName, dfDefaultValue, pnErr );

    // If we got the default just return it unadjusted.
    if( *pnErr != OGRERR_NONE )
        return dfRawResult;

    if( dfToDegrees != 1.0 && IsAngularParameter(pszName) )
        dfRawResult *= dfToDegrees;

    if( dfToMeter != 1.0 && IsLinearParameter( pszName ) )
        return dfRawResult * dfToMeter;
#ifdef WKT_LONGITUDE_RELATIVE_TO_PM
    else if( dfFromGreenwich != 0.0 && IsLongitudeParameter( pszName ) )
        return dfRawResult + dfFromGreenwich;
#endif
    else
        return dfRawResult;
}

/************************************************************************/
/*                         OSRGetNormProjParm()                         */
/************************************************************************/

double OSRGetNormProjParm( OGRSpatialReferenceH hSRS, const char *pszName,
                           double dfDefaultValue, OGRErr *pnErr )

{
    return ((OGRSpatialReference *) hSRS)->
        GetNormProjParm(pszName, dfDefaultValue, pnErr);
}

/************************************************************************/
/*                          SetNormProjParm()                           */
/************************************************************************/

/**
 * Set a projection parameter with a normalized value.
 *
 * This method is the same as SetProjParm() except that the value of
 * the parameter passed in is assumed to be in "normalized" form (decimal
 * degrees for angular values, meters for linear values.  The values are 
 * converted in a form suitable for the GEOGCS and linear units in effect.
 *
 * This method is the same as the C function OSRSetNormProjParm().
 *
 * @param pszName the parameter name, which should be selected from
 * the macros in ogr_srs_api.h, such as SRS_PP_CENTRAL_MERIDIAN. 
 *
 * @param dfValue value to assign. 
 * 
 * @return OGRERR_NONE on success.
 */

OGRErr OGRSpatialReference::SetNormProjParm( const char * pszName,
                                             double dfValue )

{
    GetNormInfo();

    if( (dfToDegrees != 1.0 || dfFromGreenwich != 0.0) 
        && IsAngularParameter(pszName) )
    {
#ifdef WKT_LONGITUDE_RELATIVE_TO_PM
        if( dfFromGreenwich != 0.0 && IsLongitudeParameter( pszName ) )
            dfValue -= dfFromGreenwich;
#endif

        dfValue /= dfToDegrees;
    }
    else if( dfToMeter != 1.0 && IsLinearParameter( pszName ) )
        dfValue /= dfToMeter;

    return SetProjParm( pszName, dfValue );
}

/************************************************************************/
/*                         OSRSetNormProjParm()                         */
/************************************************************************/

OGRErr OSRSetNormProjParm( OGRSpatialReferenceH hSRS, 
                           const char * pszParmName, double dfValue )

{
    return ((OGRSpatialReference *) hSRS)->
        SetNormProjParm( pszParmName, dfValue );
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
    SetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetNormProjParm( SRS_PP_SCALE_FACTOR, dfScale );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                              OSRSetTM()                              */
/************************************************************************/

OGRErr OSRSetTM( OGRSpatialReferenceH hSRS, 
                 double dfCenterLat, double dfCenterLong,
                 double dfScale,
                 double dfFalseEasting,
                 double dfFalseNorthing )

{
    return ((OGRSpatialReference *) hSRS)->SetTM( 
        dfCenterLat, dfCenterLong, 
        dfScale, 
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                            SetTMVariant()                            */
/************************************************************************/

OGRErr OGRSpatialReference::SetTMVariant( 
    const char *pszVariantName,
    double dfCenterLat, double dfCenterLong,
    double dfScale,
    double dfFalseEasting,
    double dfFalseNorthing )

{
    SetProjection( pszVariantName );
    SetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetNormProjParm( SRS_PP_SCALE_FACTOR, dfScale );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                          OSRSetTMVariant()                           */
/************************************************************************/

OGRErr OSRSetTMVariant( OGRSpatialReferenceH hSRS, 
                        const char *pszVariantName,
                        double dfCenterLat, double dfCenterLong,
                        double dfScale,
                        double dfFalseEasting,
                        double dfFalseNorthing )

{
    return ((OGRSpatialReference *) hSRS)->SetTMVariant( 
        pszVariantName,
        dfCenterLat, dfCenterLong, 
        dfScale, 
        dfFalseEasting, dfFalseNorthing );
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
    SetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetNormProjParm( SRS_PP_SCALE_FACTOR, dfScale );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                              SetTPED()                               */
/************************************************************************/

OGRErr OGRSpatialReference::SetTPED( double dfLat1, double dfLong1, 
                                     double dfLat2, double dfLong2,
                                     double dfFalseEasting,
                                     double dfFalseNorthing )

{
    SetProjection( SRS_PT_TWO_POINT_EQUIDISTANT );
    SetNormProjParm( SRS_PP_LATITUDE_OF_1ST_POINT, dfLat1 );
    SetNormProjParm( SRS_PP_LONGITUDE_OF_1ST_POINT, dfLong1 );
    SetNormProjParm( SRS_PP_LATITUDE_OF_2ND_POINT, dfLat2 );
    SetNormProjParm( SRS_PP_LONGITUDE_OF_2ND_POINT, dfLong2 );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                             OSRSetTMSO()                             */
/************************************************************************/

OGRErr OSRSetTMSO( OGRSpatialReferenceH hSRS, 
                 double dfCenterLat, double dfCenterLong,
                 double dfScale,
                 double dfFalseEasting,
                 double dfFalseNorthing )

{
    return ((OGRSpatialReference *) hSRS)->SetTMSO( 
        dfCenterLat, dfCenterLong, 
        dfScale, 
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                               SetTMG()                               */
/************************************************************************/

OGRErr 
OGRSpatialReference::SetTMG( double dfCenterLat, double dfCenterLong,
                             double dfFalseEasting, double dfFalseNorthing )
    
{
    SetProjection( SRS_PT_TUNISIA_MINING_GRID );
    SetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                             OSRSetTMG()                              */
/************************************************************************/

OGRErr OSRSetTMG( OGRSpatialReferenceH hSRS, 
                 double dfCenterLat, double dfCenterLong,
                 double dfFalseEasting,
                 double dfFalseNorthing )

{
    return ((OGRSpatialReference *) hSRS)->SetTMG( 
        dfCenterLat, dfCenterLong, 
        dfFalseEasting, dfFalseNorthing );
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
    SetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, dfStdP1 );
    SetNormProjParm( SRS_PP_STANDARD_PARALLEL_2, dfStdP2 );
    SetNormProjParm( SRS_PP_LATITUDE_OF_CENTER, dfCenterLat );
    SetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, dfCenterLong );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                             OSRSetACEA()                             */
/************************************************************************/

OGRErr OSRSetACEA( OGRSpatialReferenceH hSRS, 
                   double dfStdP1, double dfStdP2,
                   double dfCenterLat, double dfCenterLong,
                   double dfFalseEasting,
                   double dfFalseNorthing )

{
    return ((OGRSpatialReference *) hSRS)->SetACEA( 
        dfStdP1, dfStdP2, 
        dfCenterLat, dfCenterLong, 
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                               SetAE()                                */
/************************************************************************/

OGRErr OGRSpatialReference::SetAE( double dfCenterLat, double dfCenterLong,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    SetProjection( SRS_PT_AZIMUTHAL_EQUIDISTANT );
    SetNormProjParm( SRS_PP_LATITUDE_OF_CENTER, dfCenterLat );
    SetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, dfCenterLong );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                              OSRSetAE()                              */
/************************************************************************/

OGRErr OSRSetAE( OGRSpatialReferenceH hSRS, 
                 double dfCenterLat, double dfCenterLong,
                 double dfFalseEasting,
                 double dfFalseNorthing )

{
    return ((OGRSpatialReference *) hSRS)->SetAE( 
        dfCenterLat, dfCenterLong, 
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                              SetBonne()                              */
/************************************************************************/

OGRErr OGRSpatialReference::SetBonne( 
    double dfStdP1, double dfCentralMeridian,
    double dfFalseEasting, double dfFalseNorthing )

{
    SetProjection( SRS_PT_BONNE );
    SetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, dfStdP1 );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCentralMeridian );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            OSRSetBonne()                             */
/************************************************************************/

OGRErr OSRSetBonne( OGRSpatialReferenceH hSRS, 
                    double dfStandardParallel, double dfCentralMeridian,
                    double dfFalseEasting,
                    double dfFalseNorthing )
    
{
    return ((OGRSpatialReference *) hSRS)->SetAE( 
        dfStandardParallel, dfCentralMeridian,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                               SetCEA()                               */
/************************************************************************/

OGRErr OGRSpatialReference::SetCEA( double dfStdP1, double dfCentralMeridian,
                                    double dfFalseEasting,
                                    double dfFalseNorthing )

{
    SetProjection( SRS_PT_CYLINDRICAL_EQUAL_AREA );
    SetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, dfStdP1 );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCentralMeridian );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                             OSRSetCEA()                              */
/************************************************************************/

OGRErr OSRSetCEA( OGRSpatialReferenceH hSRS, 
                  double dfStdP1, double dfCentralMeridian,
                  double dfFalseEasting, double dfFalseNorthing )

{
    return ((OGRSpatialReference *) hSRS)->SetCEA( 
        dfStdP1, dfCentralMeridian,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                               SetCS()                                */
/************************************************************************/

OGRErr OGRSpatialReference::SetCS( double dfCenterLat, double dfCenterLong,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    SetProjection( SRS_PT_CASSINI_SOLDNER );
    SetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                              OSRSetCS()                              */
/************************************************************************/

OGRErr OSRSetCS( OGRSpatialReferenceH hSRS, 
                 double dfCenterLat, double dfCenterLong,
                 double dfFalseEasting,
                 double dfFalseNorthing )

{
    return ((OGRSpatialReference *) hSRS)->SetCS( 
        dfCenterLat, dfCenterLong, 
        dfFalseEasting, dfFalseNorthing );
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
    SetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, dfStdP1 );
    SetNormProjParm( SRS_PP_STANDARD_PARALLEL_2, dfStdP2 );
    SetNormProjParm( SRS_PP_LATITUDE_OF_CENTER, dfCenterLat );
    SetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, dfCenterLong );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                              OSRSetEC()                              */
/************************************************************************/

OGRErr OSRSetEC( OGRSpatialReferenceH hSRS, 
                 double dfStdP1, double dfStdP2,
                 double dfCenterLat, double dfCenterLong,
                 double dfFalseEasting,
                 double dfFalseNorthing )

{
    return ((OGRSpatialReference *) hSRS)->SetEC( 
        dfStdP1, dfStdP2, 
        dfCenterLat, dfCenterLong, 
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                            SetEckertIV()                             */
/************************************************************************/

OGRErr OGRSpatialReference::SetEckertIV( double dfCentralMeridian,
                                         double dfFalseEasting,
                                         double dfFalseNorthing )

{
    SetProjection( SRS_PT_ECKERT_IV );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCentralMeridian );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           OSRSetEckertIV()                           */
/************************************************************************/

OGRErr OSRSetEckertIV( OGRSpatialReferenceH hSRS, 
                       double dfCentralMeridian,
                       double dfFalseEasting,
                       double dfFalseNorthing )

{
    return ((OGRSpatialReference *) hSRS)->SetEckertIV( 
        dfCentralMeridian,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                            SetEckertVI()                             */
/************************************************************************/

OGRErr OGRSpatialReference::SetEckertVI( double dfCentralMeridian,
                                         double dfFalseEasting,
                                         double dfFalseNorthing )

{
    SetProjection( SRS_PT_ECKERT_VI );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCentralMeridian );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           OSRSetEckertVI()                           */
/************************************************************************/

OGRErr OSRSetEckertVI( OGRSpatialReferenceH hSRS, 
                       double dfCentralMeridian,
                       double dfFalseEasting,
                       double dfFalseNorthing )

{
    return ((OGRSpatialReference *) hSRS)->SetEckertVI( 
        dfCentralMeridian,
        dfFalseEasting, dfFalseNorthing );
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
    SetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                       OSRSetEquirectangular()                        */
/************************************************************************/

OGRErr OSRSetEquirectangular( OGRSpatialReferenceH hSRS, 
                              double dfCenterLat, double dfCenterLong,
                              double dfFalseEasting,
                              double dfFalseNorthing )
    
{
    return ((OGRSpatialReference *) hSRS)->SetEquirectangular( 
        dfCenterLat, dfCenterLong, 
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                               SetGS()                                */
/************************************************************************/

OGRErr OGRSpatialReference::SetGS( double dfCentralMeridian,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    SetProjection( SRS_PT_GALL_STEREOGRAPHIC );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCentralMeridian );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                              OSRSetGS()                              */
/************************************************************************/

OGRErr OSRSetGS( OGRSpatialReferenceH hSRS, 
                 double dfCentralMeridian,
                 double dfFalseEasting,
                 double dfFalseNorthing )

{
    return ((OGRSpatialReference *) hSRS)->SetGS( 
        dfCentralMeridian,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                               SetGH()                                */
/************************************************************************/

OGRErr OGRSpatialReference::SetGH( double dfCentralMeridian,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    SetProjection( SRS_PT_GOODE_HOMOLOSINE );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCentralMeridian );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                              OSRSetGH()                              */
/************************************************************************/

OGRErr OSRSetGH( OGRSpatialReferenceH hSRS, 
                 double dfCentralMeridian,
                 double dfFalseEasting,
                 double dfFalseNorthing )

{
    return ((OGRSpatialReference *) hSRS)->SetGH( 
        dfCentralMeridian,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                              SetGEOS()                               */
/************************************************************************/

OGRErr OGRSpatialReference::SetGEOS( double dfCentralMeridian,
                                     double dfSatelliteHeight,
                                     double dfFalseEasting,
                                     double dfFalseNorthing )

{
    SetProjection( SRS_PT_GEOSTATIONARY_SATELLITE );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCentralMeridian );
    SetNormProjParm( SRS_PP_SATELLITE_HEIGHT, dfSatelliteHeight );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                              OSRSetGEOS()                             */
/************************************************************************/

OGRErr OSRSetGEOS( OGRSpatialReferenceH hSRS, 
                   double dfCentralMeridian,
                   double dfSatelliteHeight,
                   double dfFalseEasting,
                   double dfFalseNorthing )

{
    return ((OGRSpatialReference *) hSRS)->SetGEOS( 
        dfCentralMeridian, dfSatelliteHeight,
        dfFalseEasting, dfFalseNorthing );
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
    SetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           OSRSetGnomonic()                           */
/************************************************************************/

OGRErr OSRSetGnomonic( OGRSpatialReferenceH hSRS, 
                       double dfCenterLat, double dfCenterLong,
                       double dfFalseEasting,
                       double dfFalseNorthing )
    
{
    return ((OGRSpatialReference *) hSRS)->SetGnomonic( 
        dfCenterLat, dfCenterLong, 
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                               SetHOM()                               */
/************************************************************************/

/**
 * Set a Hotine Oblique Mercator projection using azimuth angle.
 *
 * This method does the same thing as the C function OSRSetHOM().
 *
 * @param dfCenterLat Latitude of the projection origin.
 * @param dfCenterLong Longitude of the projection origin.
 * @param dfAzimuth Azimuth, measured clockwise from North, of the projection
 * centerline.
 * @param dfRectToSkew ?.
 * @param dfScale Scale factor applies to the projection origin.
 * @param dfFalseEasting False easting.
 * @param dfFalseNorthing False northing.
 *
 * @return OGRERR_NONE on success.
 */ 

OGRErr OGRSpatialReference::SetHOM( double dfCenterLat, double dfCenterLong,
                                    double dfAzimuth, double dfRectToSkew,
                                    double dfScale,
                                    double dfFalseEasting,
                                    double dfFalseNorthing )

{
    SetProjection( SRS_PT_HOTINE_OBLIQUE_MERCATOR );
    SetNormProjParm( SRS_PP_LATITUDE_OF_CENTER, dfCenterLat );
    SetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, dfCenterLong );
    SetNormProjParm( SRS_PP_AZIMUTH, dfAzimuth );
    SetNormProjParm( SRS_PP_RECTIFIED_GRID_ANGLE, dfRectToSkew );
    SetNormProjParm( SRS_PP_SCALE_FACTOR, dfScale );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                             OSRSetHOM()                              */
/************************************************************************/

OGRErr OSRSetHOM( OGRSpatialReferenceH hSRS, 
                  double dfCenterLat, double dfCenterLong,
                  double dfAzimuth, double dfRectToSkew, 
                  double dfScale,
                  double dfFalseEasting,
                  double dfFalseNorthing )
    
{
    return ((OGRSpatialReference *) hSRS)->SetHOM( 
        dfCenterLat, dfCenterLong, 
        dfAzimuth, dfRectToSkew, 
        dfScale,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                             SetHOM2PNO()                             */
/************************************************************************/

/**
 * Set a Hotine Oblique Mercator projection using two points on projection
 * centerline.
 *
 * This method does the same thing as the C function OSRSetHOM2PNO().
 *
 * @param dfCenterLat Latitude of the projection origin.
 * @param dfLat1 Latitude of the first point on center line.
 * @param dfLong1 Longitude of the first point on center line.
 * @param dfLat2 Latitude of the second point on center line.
 * @param dfLong2 Longitude of the second point on center line.
 * @param dfScale Scale factor applies to the projection origin.
 * @param dfFalseEasting False easting.
 * @param dfFalseNorthing False northing.
 *
 * @return OGRERR_NONE on success.
 */ 

OGRErr OGRSpatialReference::SetHOM2PNO( double dfCenterLat,
                                        double dfLat1, double dfLong1,
                                        double dfLat2, double dfLong2,
                                        double dfScale,
                                        double dfFalseEasting,
                                        double dfFalseNorthing )

{
    SetProjection( SRS_PT_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN );
    SetNormProjParm( SRS_PP_LATITUDE_OF_CENTER, dfCenterLat );
    SetNormProjParm( SRS_PP_LATITUDE_OF_POINT_1, dfLat1 );
    SetNormProjParm( SRS_PP_LONGITUDE_OF_POINT_1, dfLong1 );
    SetNormProjParm( SRS_PP_LATITUDE_OF_POINT_2, dfLat2 );
    SetNormProjParm( SRS_PP_LONGITUDE_OF_POINT_2, dfLong2 );
    SetNormProjParm( SRS_PP_SCALE_FACTOR, dfScale );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           OSRSetHOM2PNO()                            */
/************************************************************************/

OGRErr OSRSetHOM2PNO( OGRSpatialReferenceH hSRS, 
                      double dfCenterLat,
                      double dfLat1, double dfLong1,
                      double dfLat2, double dfLong2,
                      double dfScale,
                      double dfFalseEasting, double dfFalseNorthing )
    
{
    return ((OGRSpatialReference *) hSRS)->SetHOM2PNO( 
        dfCenterLat,
        dfLat1, dfLong1,
        dfLat2, dfLong2,
        dfScale,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                             SetKrovak()                              */
/************************************************************************/

OGRErr OGRSpatialReference::SetKrovak( double dfCenterLat, double dfCenterLong,
                                       double dfAzimuth, 
                                       double dfPseudoStdParallel1,
                                       double dfScale,
                                       double dfFalseEasting,
                                       double dfFalseNorthing )

{
    SetProjection( SRS_PT_KROVAK );
    SetNormProjParm( SRS_PP_LATITUDE_OF_CENTER, dfCenterLat );
    SetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, dfCenterLong );
    SetNormProjParm( SRS_PP_AZIMUTH, dfAzimuth );
    SetNormProjParm( SRS_PP_PSEUDO_STD_PARALLEL_1, dfPseudoStdParallel1 );
    SetNormProjParm( SRS_PP_SCALE_FACTOR, dfScale );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            OSRSetKrovak()                            */
/************************************************************************/

OGRErr OSRSetKrovak( OGRSpatialReferenceH hSRS, 
                     double dfCenterLat, double dfCenterLong,
                     double dfAzimuth, double dfPseudoStdParallel1,
                     double dfScale,
                     double dfFalseEasting,
                     double dfFalseNorthing )
    
{
    return ((OGRSpatialReference *) hSRS)->SetKrovak( 
        dfCenterLat, dfCenterLong, 
        dfAzimuth, dfPseudoStdParallel1,
        dfScale,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                              SetLAEA()                               */
/************************************************************************/

OGRErr OGRSpatialReference::SetLAEA( double dfCenterLat, double dfCenterLong,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    SetProjection( SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA );
    SetNormProjParm( SRS_PP_LATITUDE_OF_CENTER, dfCenterLat );
    SetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, dfCenterLong );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                             OSRSetLAEA()                             */
/************************************************************************/

OGRErr OSRSetLAEA( OGRSpatialReferenceH hSRS, 
                   double dfCenterLat, double dfCenterLong,
                   double dfFalseEasting, double dfFalseNorthing )
    
{
    return ((OGRSpatialReference *) hSRS)->SetLAEA( 
        dfCenterLat, dfCenterLong, 
        dfFalseEasting, dfFalseNorthing );
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
    SetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, dfStdP1 );
    SetNormProjParm( SRS_PP_STANDARD_PARALLEL_2, dfStdP2 );
    SetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                             OSRSetLCC()                              */
/************************************************************************/

OGRErr OSRSetLCC( OGRSpatialReferenceH hSRS, 
                  double dfStdP1, double dfStdP2, 
                  double dfCenterLat, double dfCenterLong,
                  double dfFalseEasting, double dfFalseNorthing )
    
{
    return ((OGRSpatialReference *) hSRS)->SetLCC( 
        dfStdP1, dfStdP2, 
        dfCenterLat, dfCenterLong, 
        dfFalseEasting, dfFalseNorthing );
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
    SetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetNormProjParm( SRS_PP_SCALE_FACTOR, dfScale );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            OSRSetLCC1SP()                            */
/************************************************************************/

OGRErr OSRSetLCC1SP( OGRSpatialReferenceH hSRS, 
                     double dfCenterLat, double dfCenterLong,
                     double dfScale,
                     double dfFalseEasting, double dfFalseNorthing )
    
{
    return ((OGRSpatialReference *) hSRS)->SetLCC1SP( 
        dfCenterLat, dfCenterLong, 
        dfScale,
        dfFalseEasting, dfFalseNorthing );
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
    SetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, dfStdP1 );
    SetNormProjParm( SRS_PP_STANDARD_PARALLEL_2, dfStdP2 );
    SetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                             OSRSetLCCB()                             */
/************************************************************************/

OGRErr OSRSetLCCB( OGRSpatialReferenceH hSRS, 
                   double dfStdP1, double dfStdP2, 
                   double dfCenterLat, double dfCenterLong,
                   double dfFalseEasting, double dfFalseNorthing )
    
{
    return ((OGRSpatialReference *) hSRS)->SetLCCB( 
        dfStdP1, dfStdP2, 
        dfCenterLat, dfCenterLong, 
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                               SetMC()                                */
/************************************************************************/

OGRErr OGRSpatialReference::SetMC( double dfCenterLat, double dfCenterLong,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    SetProjection( SRS_PT_MILLER_CYLINDRICAL );
    SetNormProjParm( SRS_PP_LATITUDE_OF_CENTER, dfCenterLat );
    SetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, dfCenterLong );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                              OSRSetMC()                              */
/************************************************************************/

OGRErr OSRSetMC( OGRSpatialReferenceH hSRS, 
                 double dfCenterLat, double dfCenterLong,
                 double dfFalseEasting, double dfFalseNorthing )
    
{
    return ((OGRSpatialReference *) hSRS)->SetMC( 
        dfCenterLat, dfCenterLong, 
        dfFalseEasting, dfFalseNorthing );
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
    SetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetNormProjParm( SRS_PP_SCALE_FACTOR, dfScale );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           OSRSetMercator()                           */
/************************************************************************/

OGRErr OSRSetMercator( OGRSpatialReferenceH hSRS, 
                       double dfCenterLat, double dfCenterLong,
                       double dfScale,
                       double dfFalseEasting, double dfFalseNorthing )
    
{
    return ((OGRSpatialReference *) hSRS)->SetMercator( 
        dfCenterLat, dfCenterLong, 
        dfScale,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                            SetMollweide()                            */
/************************************************************************/

OGRErr OGRSpatialReference::SetMollweide( double dfCentralMeridian,
                                          double dfFalseEasting,
                                          double dfFalseNorthing )

{
    SetProjection( SRS_PT_MOLLWEIDE );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCentralMeridian );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                          OSRSetMollweide()                           */
/************************************************************************/

OGRErr OSRSetMollweide( OGRSpatialReferenceH hSRS, 
                        double dfCentralMeridian,
                        double dfFalseEasting, double dfFalseNorthing )
    
{
    return ((OGRSpatialReference *) hSRS)->SetMollweide( 
        dfCentralMeridian,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                              SetNZMG()                               */
/************************************************************************/

OGRErr OGRSpatialReference::SetNZMG( double dfCenterLat, double dfCenterLong,
                                     double dfFalseEasting,
                                     double dfFalseNorthing )

{
    SetProjection( SRS_PT_NEW_ZEALAND_MAP_GRID );
    SetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                             OSRSetNZMG()                             */
/************************************************************************/

OGRErr OSRSetNZMG( OGRSpatialReferenceH hSRS, 
                   double dfCenterLat, double dfCenterLong,
                   double dfFalseEasting, double dfFalseNorthing )
    
{
    return ((OGRSpatialReference *) hSRS)->SetNZMG( 
        dfCenterLat, dfCenterLong, 
        dfFalseEasting, dfFalseNorthing );
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
    SetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfOriginLat );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCMeridian );
    SetNormProjParm( SRS_PP_SCALE_FACTOR, dfScale );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                              OSRSetOS()                              */
/************************************************************************/

OGRErr OSRSetOS( OGRSpatialReferenceH hSRS, 
                 double dfOriginLat, double dfCMeridian,
                 double dfScale,
                 double dfFalseEasting, double dfFalseNorthing )
    
{
    return ((OGRSpatialReference *) hSRS)->SetOS( 
        dfOriginLat, dfCMeridian,
        dfScale,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                          SetOrthographic()                           */
/************************************************************************/

OGRErr OGRSpatialReference::SetOrthographic(
                                double dfCenterLat, double dfCenterLong,
                                double dfFalseEasting, double dfFalseNorthing )

{
    SetProjection( SRS_PT_ORTHOGRAPHIC );
    SetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                         OSRSetOrthographic()                         */
/************************************************************************/

OGRErr OSRSetOrthographic( OGRSpatialReferenceH hSRS, 
                           double dfCenterLat, double dfCenterLong,
                           double dfFalseEasting, double dfFalseNorthing )
    
{
    return ((OGRSpatialReference *) hSRS)->SetOrthographic( 
        dfCenterLat, dfCenterLong, 
        dfFalseEasting, dfFalseNorthing );
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
    SetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                          OSRSetPolyconic()                           */
/************************************************************************/

OGRErr OSRSetPolyconic( OGRSpatialReferenceH hSRS, 
                        double dfCenterLat, double dfCenterLong,
                        double dfFalseEasting, double dfFalseNorthing )
    
{
    return ((OGRSpatialReference *) hSRS)->SetPolyconic( 
        dfCenterLat, dfCenterLong, 
        dfFalseEasting, dfFalseNorthing );
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
    SetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetNormProjParm( SRS_PP_SCALE_FACTOR, dfScale );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                              OSRSetPS()                              */
/************************************************************************/

OGRErr OSRSetPS( OGRSpatialReferenceH hSRS, 
                 double dfCenterLat, double dfCenterLong,
                 double dfScale,
                 double dfFalseEasting, double dfFalseNorthing )
    
{
    return ((OGRSpatialReference *) hSRS)->SetPS( 
        dfCenterLat, dfCenterLong, 
        dfScale,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                            SetRobinson()                             */
/************************************************************************/

OGRErr OGRSpatialReference::SetRobinson( double dfCenterLong,
                                         double dfFalseEasting,
                                         double dfFalseNorthing )

{
    SetProjection( SRS_PT_ROBINSON );
    SetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, dfCenterLong );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           OSRSetRobinson()                           */
/************************************************************************/

OGRErr OSRSetRobinson( OGRSpatialReferenceH hSRS, 
                        double dfCenterLong,
                        double dfFalseEasting, double dfFalseNorthing )
    
{
    return ((OGRSpatialReference *) hSRS)->SetRobinson( 
        dfCenterLong, 
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                           SetSinusoidal()                            */
/************************************************************************/

OGRErr OGRSpatialReference::SetSinusoidal( double dfCenterLong,
                                           double dfFalseEasting,
                                           double dfFalseNorthing )

{
    SetProjection( SRS_PT_SINUSOIDAL );
    SetNormProjParm( SRS_PP_LONGITUDE_OF_CENTER, dfCenterLong );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                          OSRSetSinusoidal()                          */
/************************************************************************/

OGRErr OSRSetSinusoidal( OGRSpatialReferenceH hSRS, 
                         double dfCenterLong,
                         double dfFalseEasting, double dfFalseNorthing )
    
{
    return ((OGRSpatialReference *) hSRS)->SetSinusoidal( 
        dfCenterLong, 
        dfFalseEasting, dfFalseNorthing );
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
    SetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfOriginLat );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCMeridian );
    SetNormProjParm( SRS_PP_SCALE_FACTOR, dfScale );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                        OSRSetStereographic()                         */
/************************************************************************/

OGRErr OSRSetStereographic( OGRSpatialReferenceH hSRS, 
                            double dfOriginLat, double dfCMeridian,
                            double dfScale,
                            double dfFalseEasting, double dfFalseNorthing )
    
{
    return ((OGRSpatialReference *) hSRS)->SetStereographic( 
        dfOriginLat, dfCMeridian,
        dfScale,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                               SetSOC()                               */
/*                                                                      */
/*      NOTE: This definition isn't really used in practice any more    */
/*      and should be considered deprecated.  It seems that swiss       */
/*      oblique mercator is now define as Hotine_Oblique_Mercator       */
/*      with an azimuth of 90 and a rectified_grid_angle of 90.  See    */
/*      EPSG:2056 and Bug 423.                                          */
/************************************************************************/

OGRErr OGRSpatialReference::SetSOC( double dfLatitudeOfOrigin, 
                                    double dfCentralMeridian,
                                    double dfFalseEasting,
                                    double dfFalseNorthing )

{
    SetProjection( SRS_PT_SWISS_OBLIQUE_CYLINDRICAL );
    SetNormProjParm( SRS_PP_LATITUDE_OF_CENTER, dfLatitudeOfOrigin );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCentralMeridian );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                             OSRSetSOC()                              */
/************************************************************************/

OGRErr OSRSetSOC( OGRSpatialReferenceH hSRS, 
                  double dfLatitudeOfOrigin, double dfCentralMeridian,
                  double dfFalseEasting, double dfFalseNorthing )
    
{
    return ((OGRSpatialReference *) hSRS)->SetSOC( 
        dfLatitudeOfOrigin, dfCentralMeridian,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                               SetVDG()                               */
/************************************************************************/

OGRErr OGRSpatialReference::SetVDG( double dfCMeridian,
                                    double dfFalseEasting,
                                    double dfFalseNorthing )

{
    SetProjection( SRS_PT_VANDERGRINTEN );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCMeridian );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                             OSRSetVDG()                              */
/************************************************************************/

OGRErr OSRSetVDG( OGRSpatialReferenceH hSRS, 
                  double dfCentralMeridian,
                  double dfFalseEasting, double dfFalseNorthing )
    
{
    return ((OGRSpatialReference *) hSRS)->SetVDG( 
        dfCentralMeridian,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                               SetUTM()                               */
/************************************************************************/

/**
 * Set UTM projection definition.
 *
 * This will generate a projection definition with the full set of 
 * transverse mercator projection parameters for the given UTM zone.
 * If no PROJCS[] description is set yet, one will be set to look
 * like "UTM Zone %d, {Northern, Southern} Hemisphere". 
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
    SetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0 );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, nZone * 6 - 183 );
    SetNormProjParm( SRS_PP_SCALE_FACTOR, 0.9996 );
    SetNormProjParm( SRS_PP_FALSE_EASTING, 500000.0 );

    if( bNorth )
        SetNormProjParm( SRS_PP_FALSE_NORTHING, 0 );
    else
        SetNormProjParm( SRS_PP_FALSE_NORTHING, 10000000 );

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

int OGRSpatialReference::GetUTMZone( int * pbNorth ) const

{
    const char  *pszProjection = GetAttrValue( "PROJECTION" );

    if( pszProjection == NULL
        || !EQUAL(pszProjection,SRS_PT_TRANSVERSE_MERCATOR) )
        return 0;

    if( GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) != 0.0 )
        return 0;

    if( GetProjParm( SRS_PP_SCALE_FACTOR, 1.0 ) != 0.9996 )
        return 0;
          
    if( fabs(GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 )-500000.0) > 0.001 )
        return 0;

    double      dfFalseNorthing = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0);

    if( dfFalseNorthing != 0.0 
        && fabs(dfFalseNorthing-10000000.0) > 0.001 )
        return 0;

    if( pbNorth != NULL )
        *pbNorth = (dfFalseNorthing == 0);

    double      dfCentralMeridian = GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 
                                                     0.0);
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
/* -------------------------------------------------------------------- */
/*      Find the node below which the authority should be put.          */
/* -------------------------------------------------------------------- */
    OGR_SRSNode  *poNode = GetAttrNode( pszTargetKey );

    if( poNode == NULL )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      If there is an existing AUTHORITY child blow it away before     */
/*      trying to set a new one.                                        */
/* -------------------------------------------------------------------- */
    int iOldChild = poNode->FindChild( "AUTHORITY" );
    if( iOldChild != -1 )
        poNode->DestroyChild( iOldChild );

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
/*                          GetAuthorityCode()                          */
/************************************************************************/

/**
 * Get the authority code for a node.
 *
 * This method is used to query an AUTHORITY[] node from within the 
 * WKT tree, and fetch the code value.  
 *
 * While in theory values may be non-numeric, for the EPSG authority all
 * code values should be integral.
 *
 * This method is the same as the C function OSRGetAuthorityCode().
 *
 * @param pszTargetKey the partial or complete path to the node to 
 * set an authority on.  ie. "PROJCS", "GEOGCS" or "GEOGCS|UNIT".
 *
 * @return value code from authority node, or NULL on failure.  The value
 * returned is internal and should not be freed or modified.
 */

const char *
OGRSpatialReference::GetAuthorityCode( const char *pszTargetKey ) const

{
/* -------------------------------------------------------------------- */
/*      Find the node below which the authority should be put.          */
/* -------------------------------------------------------------------- */
    OGR_SRSNode  *poNode = 
        ((OGRSpatialReference *) this)->GetAttrNode( pszTargetKey );

    if( poNode == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Fetch AUTHORITY child if there is one.                          */
/* -------------------------------------------------------------------- */
    if( poNode->FindChild("AUTHORITY") == -1 )
        return NULL;
        
    poNode = poNode->GetChild(poNode->FindChild("AUTHORITY"));

/* -------------------------------------------------------------------- */
/*      Create a new authority node.                                    */
/* -------------------------------------------------------------------- */
    if( poNode->GetChildCount() < 2 )
        return NULL;

    return poNode->GetChild(1)->GetValue();
}

/************************************************************************/
/*                          OSRGetAuthorityCode()                       */
/************************************************************************/

const char *OSRGetAuthorityCode( OGRSpatialReferenceH hSRS, 
                                 const char *pszTargetKey )

{
    return ((OGRSpatialReference *) hSRS)->GetAuthorityCode( pszTargetKey );
}

/************************************************************************/
/*                          GetAuthorityName()                          */
/************************************************************************/

/**
 * Get the authority name for a node.
 *
 * This method is used to query an AUTHORITY[] node from within the 
 * WKT tree, and fetch the authority name value.  
 *
 * The most common authority is "EPSG".
 *
 * This method is the same as the C function OSRGetAuthorityName().
 *
 * @param pszTargetKey the partial or complete path to the node to 
 * set an authority on.  ie. "PROJCS", "GEOGCS" or "GEOGCS|UNIT".
 *
 * @return value code from authority node, or NULL on failure. The value
 * returned is internal and should not be freed or modified.
 */

const char *
OGRSpatialReference::GetAuthorityName( const char *pszTargetKey ) const

{
/* -------------------------------------------------------------------- */
/*      Find the node below which the authority should be put.          */
/* -------------------------------------------------------------------- */
    OGR_SRSNode  *poNode = 
        ((OGRSpatialReference *) this)->GetAttrNode( pszTargetKey );

    if( poNode == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Fetch AUTHORITY child if there is one.                          */
/* -------------------------------------------------------------------- */
    if( poNode->FindChild("AUTHORITY") == -1 )
        return NULL;
        
    poNode = poNode->GetChild(poNode->FindChild("AUTHORITY"));

/* -------------------------------------------------------------------- */
/*      Create a new authority node.                                    */
/* -------------------------------------------------------------------- */
    if( poNode->GetChildCount() < 2 )
        return NULL;

    return poNode->GetChild(0)->GetValue();
}

/************************************************************************/
/*                        OSRGetAuthorityName()                         */
/************************************************************************/

const char *OSRGetAuthorityName( OGRSpatialReferenceH hSRS, 
                                 const char *pszTargetKey )

{
    return ((OGRSpatialReference *) hSRS)->GetAuthorityName( pszTargetKey );
}

/************************************************************************/
/*                            StripCTParms()                            */
/************************************************************************/

/** 
 * Strip OGC CT Parameters.
 *
 * This method will remove all components of the coordinate system
 * that are specific to the OGC CT Specification.  That is it will attempt
 * to strip it down to being compatible with the Simple Features 1.0 
 * specification.
 *
 * This method is the same as the C function OSRStripCTParms().
 *
 * @param poCurrent node to operate on.  NULL to operate on whole tree.
 *
 * @return OGRERR_NONE on success or an error code.
 */

OGRErr OGRSpatialReference::StripCTParms( OGR_SRSNode * poCurrent )

{
    if( poCurrent == NULL )
        poCurrent = GetRoot();

    if( poCurrent == NULL )
        return OGRERR_NONE;

    if( poCurrent == GetRoot() && EQUAL(poCurrent->GetValue(),"LOCAL_CS") )
    {
        delete poCurrent;
        poRoot = NULL;

        return OGRERR_NONE;
    }
    
    if( poCurrent == NULL )
        return OGRERR_NONE;

    poCurrent->StripNodes( "AUTHORITY" );
    poCurrent->StripNodes( "TOWGS84" );
    poCurrent->StripNodes( "AXIS" );

    return OGRERR_NONE;
}

/************************************************************************/
/*                          OSRStripCTParms()                           */
/************************************************************************/

OGRErr OSRStripCTParms( OGRSpatialReferenceH hSRS )

{
    return ((OGRSpatialReference *) hSRS)->StripCTParms( NULL );
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

int OGRSpatialReference::IsProjected() const

{
    if( poRoot == NULL )
        return FALSE;

    // If we eventually support composite coordinate systems this will
    // need to improve. 

    return EQUAL(poRoot->GetValue(),"PROJCS");
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

int OGRSpatialReference::IsGeographic() const

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

int OGRSpatialReference::IsLocal() const

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

OGRSpatialReference *OGRSpatialReference::CloneGeogCS() const

{
    const OGR_SRSNode *poGeogCS;
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

OGRSpatialReferenceH CPL_STDCALL OSRCloneGeogCS( OGRSpatialReferenceH hSource )

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

int OGRSpatialReference::IsSameGeogCS( const OGRSpatialReference *poOther ) const

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

int OGRSpatialReference::IsSame( const OGRSpatialReference * poOtherSRS ) const

{
    if( GetRoot() == NULL && poOtherSRS->GetRoot() == NULL )
        return TRUE;
    else if( GetRoot() == NULL || poOtherSRS->GetRoot() == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Compare geographic coordinate system.                           */
/* -------------------------------------------------------------------- */
    if( !IsSameGeogCS( poOtherSRS ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Do the have the same root types?  Ie. is one PROJCS and one     */
/*      GEOGCS or perhaps LOCALCS?                                      */
/* -------------------------------------------------------------------- */
    if( !EQUAL(GetRoot()->GetValue(),poOtherSRS->GetRoot()->GetValue()) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Compare projected coordinate system.                            */
/* -------------------------------------------------------------------- */
    if( IsProjected() )
    {
        const char *pszValue1, *pszValue2;
        const OGR_SRSNode *poPROJCS = GetAttrNode( "PROJCS" );

        pszValue1 = this->GetAttrValue( "PROJECTION" );
        pszValue2 = poOtherSRS->GetAttrValue( "PROJECTION" );
        if( pszValue1 == NULL || pszValue2 == NULL
            || !EQUAL(pszValue1,pszValue2) )
            return FALSE;

        for( int iChild = 0; iChild < poPROJCS->GetChildCount(); iChild++ )
        {
            const OGR_SRSNode    *poNode;

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
            double      dfRatio;

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

OGRErr OGRSpatialReference::GetTOWGS84( double * padfCoeff, 
                                        int nCoeffCount ) const

{
    const OGR_SRSNode   *poNode = GetAttrNode( "TOWGS84" );

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

/************************************************************************/
/*                         IsAngularParameter()                         */
/*                                                                      */
/*      Is the passed projection parameter an angular one?              */
/************************************************************************/

int OGRSpatialReference::IsAngularParameter( const char *pszParameterName )

{
    if( EQUALN(pszParameterName,"long",4)
        || EQUALN(pszParameterName,"lati",4)
        || EQUAL(pszParameterName,SRS_PP_CENTRAL_MERIDIAN)
        || EQUALN(pszParameterName,"standard_parallel",17)
        || EQUAL(pszParameterName,SRS_PP_AZIMUTH)
        || EQUAL(pszParameterName,SRS_PP_RECTIFIED_GRID_ANGLE) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                        IsLongitudeParameter()                        */
/*                                                                      */
/*      Is the passed projection parameter an angular longitude         */
/*      (relative to a prime meridian)?                                 */
/************************************************************************/

int OGRSpatialReference::IsLongitudeParameter( const char *pszParameterName )

{
    if( EQUALN(pszParameterName,"long",4)
        || EQUAL(pszParameterName,SRS_PP_CENTRAL_MERIDIAN) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                         IsLinearParameter()                          */
/*                                                                      */
/*      Is the passed projection parameter an linear one measured in    */
/*      meters or some similar linear measure.                          */
/************************************************************************/

int OGRSpatialReference::IsLinearParameter( const char *pszParameterName )

{
    if( EQUALN(pszParameterName,"false_",6) 
        || EQUAL(pszParameterName,SRS_PP_SATELLITE_HEIGHT) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                            GetNormInfo()                             */
/*                                                                      */
/*      Set the internal information for normalizing linear, and        */
/*      angular values.                                                 */
/************************************************************************/

void OGRSpatialReference::GetNormInfo(void) const

{
    if( bNormInfoSet )
        return;

/* -------------------------------------------------------------------- */
/*      Initialize values.                                              */
/* -------------------------------------------------------------------- */
    OGRSpatialReference *poThis = (OGRSpatialReference *) this;

    poThis->bNormInfoSet = TRUE;

    poThis->dfFromGreenwich = GetPrimeMeridian(NULL);
    poThis->dfToMeter = GetLinearUnits(NULL);
    poThis->dfToDegrees = GetAngularUnits(NULL) / atof(SRS_UA_DEGREE_CONV);
    if( fabs(poThis->dfToDegrees-1.0) < 0.000000001 )
        poThis->dfToDegrees = 1.0;
}

/************************************************************************/
/*                           FixupOrdering()                            */
/************************************************************************/

/**
 * Correct parameter ordering to match CT Specification.
 *
 * Some mechanisms to create WKT using OGRSpatialReference, and some
 * imported WKT fail to maintain the order of parameters required according
 * to the BNF definitions in the OpenGIS SF-SQL and CT Specifications.  This
 * method attempts to massage things back into the required order.
 *
 * This method is the same as the C function OSRFixupOrdering().
 *
 * @return OGRERR_NONE on success or an error code if something goes 
 * wrong.  
 */

OGRErr OGRSpatialReference::FixupOrdering()

{
    if( GetRoot() != NULL )
        return GetRoot()->FixupOrdering();
    else
        return OGRERR_NONE;
}

/************************************************************************/
/*                          OSRFixupOrdering()                          */
/************************************************************************/

OGRErr OSRFixupOrdering( OGRSpatialReferenceH hSRS )

{
    return ((OGRSpatialReference *) hSRS)->FixupOrdering();
}

/************************************************************************/
/*                               Fixup()                                */
/************************************************************************/

/**
 * Fixup as needed.
 *
 * Some mechanisms to create WKT using OGRSpatialReference, and some
 * imported WKT, are not valid according to the OGC CT specification.  This
 * method attempts to fill in any missing defaults that are required, and
 * fixup ordering problems (using OSRFixupOrdering()) so that the resulting
 * WKT is valid. 
 *
 * This method should be expected to evolve over time to as problems are
 * discovered.  The following are amoung the fixup actions this method will
 * take:
 *
 * - Fixup the ordering of nodes to match the BNF WKT ordering, using
 * the FixupOrdering() method. 
 *
 * - Add missing linear or angular units nodes.  
 *
 * This method is the same as the C function OSRFixup().
 *
 * @return OGRERR_NONE on success or an error code if something goes 
 * wrong.  
 */

OGRErr OGRSpatialReference::Fixup()

{
/* -------------------------------------------------------------------- */
/*      Ensure linear units defaulted to METER if missing for PROJCS    */
/*      or LOCAL_CS.                                                    */
/* -------------------------------------------------------------------- */
    const OGR_SRSNode *poCS = GetAttrNode( "PROJCS" );

    if( poCS == NULL )
        poCS = GetAttrNode( "LOCAL_CS" );

    if( poCS != NULL && poCS->FindChild( "UNIT" ) == -1 )
        SetLinearUnits( SRS_UL_METER, 1.0 );

/* -------------------------------------------------------------------- */
/*      Ensure angular units defaulted to degrees on the GEOGCS.        */
/* -------------------------------------------------------------------- */
    poCS = GetAttrNode( "GEOGCS" );
    if( poCS != NULL && poCS->FindChild( "UNIT" ) == -1 )
        SetAngularUnits( SRS_UA_DEGREE, atof(SRS_UA_DEGREE_CONV) );

    return FixupOrdering();
}

/************************************************************************/
/*                              OSRFixup()                              */
/************************************************************************/

OGRErr OSRFixup( OGRSpatialReferenceH hSRS )

{
    return ((OGRSpatialReference *) hSRS)->Fixup();
}


/************************************************************************/
/*                             OSRCleanup()                             */
/************************************************************************/

/**
 * Cleanup cached SRS related memory.
 *
 * This function will attempt to cleanup any cache spatial reference
 * related information, such as cached tables of coordinate systems. 
 */

CPL_C_START 
void CleanupESRIDatumMappingTable();
CPL_C_END


void OSRCleanup( void )

{
    CleanupESRIDatumMappingTable();
    CSVDeaccess( NULL );
}
