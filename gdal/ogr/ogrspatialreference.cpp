/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRSpatialReference class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
 ****************************************************************************/

#include "cpl_port.h"
#include "ogr_spatialref.h"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "cpl_atomic_ops.h"
#include "cpl_conv.h"
#include "cpl_csv.h"
#include "cpl_error.h"
#include "cpl_http.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "ogr_core.h"
#include "ogr_p.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$")

// The current opinion is that WKT longitudes like central meridian
// should be relative to Greenwich, not the prime meridian in use.
// Define the following if they should be relative to the prime meridian
// of then geogcs.
#undef WKT_LONGITUDE_RELATIVE_TO_PM

/************************************************************************/
/*                           OGRsnPrintDouble()                         */
/************************************************************************/

void OGRsnPrintDouble( char * pszStrBuf, size_t size, double dfValue );

void OGRsnPrintDouble( char * pszStrBuf, size_t size, double dfValue )

{
    CPLsnprintf( pszStrBuf, size, "%.16g", dfValue );

    const size_t nLen = strlen(pszStrBuf);

    // The following hack is intended to truncate some "precision" in cases
    // that appear to be roundoff error.
    if( nLen > 15
        && (strcmp(pszStrBuf+nLen-6, "999999") == 0
            || strcmp(pszStrBuf+nLen-6, "000001") == 0) )
    {
        CPLsnprintf( pszStrBuf, size, "%.15g", dfValue );
    }

    // Force to user periods regardless of locale.
    if( strchr( pszStrBuf, ',' ) != nullptr )
    {
        char * const pszDelim = strchr( pszStrBuf, ',' );
        *pszDelim = '.';
    }
}

/************************************************************************/
/*                        OGRSpatialReference()                         */
/************************************************************************/

/**
 * \brief Constructor.
 *
 * This constructor takes an optional string argument which if passed
 * should be a WKT representation of an SRS.  Passing this is equivalent
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

OGRSpatialReference::OGRSpatialReference( const char * pszWKT ) :
    dfFromGreenwich(0.0),
    dfToMeter(0.0),
    dfToDegrees(0.0),
    poRoot(nullptr),
    nRefCount(1),
    bNormInfoSet(FALSE)
{
    if( pszWKT != nullptr )
        importFromWkt( (char **) &pszWKT );
}

/************************************************************************/
/*                       OSRNewSpatialReference()                       */
/************************************************************************/

/**
 * \brief Constructor.
 *
 * This function is the same as OGRSpatialReference::OGRSpatialReference()
 */
OGRSpatialReferenceH CPL_STDCALL OSRNewSpatialReference( const char *pszWKT )

{
    OGRSpatialReference * poSRS = new OGRSpatialReference();

    if( pszWKT != nullptr && strlen(pszWKT) > 0 )
    {
        if( poSRS->importFromWkt( (char **) (&pszWKT) ) != OGRERR_NONE )
        {
            delete poSRS;
            poSRS = nullptr;
        }
    }

    return reinterpret_cast<OGRSpatialReferenceH>( poSRS );
}

/************************************************************************/
/*                        OGRSpatialReference()                         */
/************************************************************************/

/** Simple copy constructor. See also Clone().
 * @param oOther other spatial reference
 */
OGRSpatialReference::OGRSpatialReference(const OGRSpatialReference &oOther) :
    dfFromGreenwich(0.0),
    dfToMeter(0.0),
    dfToDegrees(0.0),
    poRoot(nullptr),
    nRefCount(1),
    bNormInfoSet(FALSE)
{
    if( oOther.poRoot != nullptr )
        poRoot = oOther.poRoot->Clone();
}

/************************************************************************/
/*                        ~OGRSpatialReference()                        */
/************************************************************************/

/**
 * \brief OGRSpatialReference destructor.
 *
 * The C function OSRDestroySpatialReference() does the same thing as this
 * method. Preferred C++ method : OGRSpatialReference::DestroySpatialReference()
  *
 * @deprecated
 */

OGRSpatialReference::~OGRSpatialReference()

{
    delete poRoot;
}

/************************************************************************/
/*                      DestroySpatialReference()                       */
/************************************************************************/

/**
 * \brief OGRSpatialReference destructor.
 *
 * This static method will destroy a OGRSpatialReference.  It is
 * equivalent to calling delete on the object, but it ensures that the
 * deallocation is properly executed within the OGR libraries heap on
 * platforms where this can matter (win32).
 *
 * This function is the same as OSRDestroySpatialReference()
 *
 * @param poSRS the object to delete
 *
 * @since GDAL 1.7.0
 */

void OGRSpatialReference::DestroySpatialReference(OGRSpatialReference* poSRS)
{
    delete poSRS;
}

/************************************************************************/
/*                     OSRDestroySpatialReference()                     */
/************************************************************************/

/**
 * \brief OGRSpatialReference destructor.
 *
 * This function is the same as OGRSpatialReference::~OGRSpatialReference()
 * and OGRSpatialReference::DestroySpatialReference()
 *
 * @param hSRS the object to delete
 */
void CPL_STDCALL OSRDestroySpatialReference( OGRSpatialReferenceH hSRS )

{
    delete reinterpret_cast<OGRSpatialReference *>(hSRS);
}

/************************************************************************/
/*                               Clear()                                */
/************************************************************************/

/**
 * \brief Wipe current definition.
 *
 * Returns OGRSpatialReference to a state with no definition, as it
 * exists when first created.  It does not affect reference counts.
 */

void OGRSpatialReference::Clear()

{
    delete poRoot;
    poRoot = nullptr;

    bNormInfoSet = FALSE;
    dfFromGreenwich = 1.0;
    dfToMeter = 1.0;
    dfToDegrees = 1.0;
}

/************************************************************************/
/*                             operator=()                              */
/************************************************************************/

/** Assignment operator.
 * @param oSource SRS to assing to *this
 * @return *this
 */
OGRSpatialReference &
OGRSpatialReference::operator=(const OGRSpatialReference &oSource)

{
    if( &oSource != this )
    {
        Clear();
#ifdef CPPCHECK
        // Otherwise cppcheck would protest that nRefCount isn't modified
        nRefCount = (nRefCount + 1) - 1;
#endif

        if( oSource.poRoot != nullptr )
            poRoot = oSource.poRoot->Clone();
    }

    return *this;
}

/************************************************************************/
/*                             Reference()                              */
/************************************************************************/

/**
 * \brief Increments the reference count by one.
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
    return CPLAtomicInc(&nRefCount);
}

/************************************************************************/
/*                            OSRReference()                            */
/************************************************************************/

/**
 * \brief Increments the reference count by one.
 *
 * This function is the same as OGRSpatialReference::Reference()
 */
int OSRReference( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRReference", 0 );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->Reference();
}

/************************************************************************/
/*                            Dereference()                             */
/************************************************************************/

/**
 * \brief Decrements the reference count by one.
 *
 * The method does the same thing as the C function OSRDereference().
 *
 * @return the updated reference count.
 */

int OGRSpatialReference::Dereference()

{
    if( nRefCount <= 0 )
        CPLDebug( "OSR",
                  "Dereference() called on an object with refcount %d,"
                  "likely already destroyed!",
                  nRefCount );
    return CPLAtomicDec(&nRefCount);
}

/************************************************************************/
/*                           OSRDereference()                           */
/************************************************************************/

/**
 * \brief Decrements the reference count by one.
 *
 * This function is the same as OGRSpatialReference::Dereference()
 */
int OSRDereference( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRDereference", 0 );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->Dereference();
}

/************************************************************************/
/*                         GetReferenceCount()                          */
/************************************************************************/

/**
 * \fn int OGRSpatialReference::GetReferenceCount() const;
 *
 * \brief Fetch current reference count.
 *
 * @return the current reference count.
 */

/************************************************************************/
/*                              Release()                               */
/************************************************************************/

/**
 * \brief Decrements the reference count by one, and destroy if zero.
 *
 * The method does the same thing as the C function OSRRelease().
 */

void OGRSpatialReference::Release()

{
    if( Dereference() <= 0 )
        delete this;
}

/************************************************************************/
/*                             OSRRelease()                             */
/************************************************************************/

/**
 * \brief Decrements the reference count by one, and destroy if zero.
 *
 * This function is the same as OGRSpatialReference::Release()
 */
void OSRRelease( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER0( hSRS, "OSRRelease" );

    reinterpret_cast<OGRSpatialReference *>(hSRS)->Release();
}

/************************************************************************/
/*                              SetRoot()                               */
/************************************************************************/

/**
 * \brief Set the root SRS node.
 *
 * If the object has an existing tree of OGR_SRSNodes, they are destroyed
 * as part of assigning the new root.  Ownership of the passed OGR_SRSNode is
 * is assumed by the OGRSpatialReference.
 *
 * @param poNewRoot object to assign as root.
 */

void OGRSpatialReference::SetRoot( OGR_SRSNode * poNewRoot )

{
    delete poRoot;
    poRoot = poNewRoot;
}

/************************************************************************/
/*                            GetAttrNode()                             */
/************************************************************************/

/**
 * \brief Find named node in tree.
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
 * components such as "GEOGCS|UNIT".
 *
 * @return a pointer to the node found, or NULL if none.
 */

OGR_SRSNode *OGRSpatialReference::GetAttrNode( const char * pszNodePath )

{
    if( strchr(pszNodePath, '|') == nullptr )
    {
        // Fast path
        OGR_SRSNode *poNode = GetRoot();
        if( poNode )
            poNode = poNode->GetNode( pszNodePath );
        return poNode;
    }

    char **papszPathTokens =
        CSLTokenizeStringComplex(pszNodePath, "|", TRUE, FALSE);

    if( CSLCount( papszPathTokens ) < 1 )
    {
        CSLDestroy(papszPathTokens);
        return nullptr;
    }

    OGR_SRSNode *poNode = GetRoot();
    for( int i = 0; poNode != nullptr && papszPathTokens[i] != nullptr; i++ )
    {
        poNode = poNode->GetNode( papszPathTokens[i] );
    }

    CSLDestroy( papszPathTokens );

    return poNode;
}

/**
 * \brief Find named node in tree.
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
 * components such as "GEOGCS|UNIT".
 *
 * @return a pointer to the node found, or NULL if none.
 */

const OGR_SRSNode *
OGRSpatialReference::GetAttrNode( const char * pszNodePath ) const

{
    OGR_SRSNode *poNode =
        const_cast<OGRSpatialReference *>(this)->GetAttrNode(pszNodePath);

    return poNode;
}

/************************************************************************/
/*                            GetAttrValue()                            */
/************************************************************************/

/**
 * \brief Fetch indicated attribute of named node.
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
    const OGR_SRSNode *poNode = GetAttrNode( pszNodeName );
    if( poNode == nullptr )
        return nullptr;

    if( iAttr < 0 || iAttr >= poNode->GetChildCount() )
        return nullptr;

    return poNode->GetChild(iAttr)->GetValue();
}

/************************************************************************/
/*                          OSRGetAttrValue()                           */
/************************************************************************/

/**
 * \brief Fetch indicated attribute of named node.
 *
 * This function is the same as OGRSpatialReference::GetAttrValue()
 */
const char * CPL_STDCALL OSRGetAttrValue( OGRSpatialReferenceH hSRS,
                             const char * pszKey, int iChild )

{
    VALIDATE_POINTER1( hSRS, "OSRGetAttrValue", nullptr );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        GetAttrValue( pszKey, iChild );
}

/************************************************************************/
/*                               Clone()                                */
/************************************************************************/

/**
 * \brief Make a duplicate of this OGRSpatialReference.
 *
 * This method is the same as the C function OSRClone().
 *
 * @return a new SRS, which becomes the responsibility of the caller.
 */

OGRSpatialReference *OGRSpatialReference::Clone() const

{
    OGRSpatialReference *poNewRef = new OGRSpatialReference();

    if( poRoot != nullptr )
        poNewRef->poRoot = poRoot->Clone();

    return poNewRef;
}

/************************************************************************/
/*                              OSRClone()                              */
/************************************************************************/

/**
 * \brief Make a duplicate of this OGRSpatialReference.
 *
 * This function is the same as OGRSpatialReference::Clone()
 */
OGRSpatialReferenceH CPL_STDCALL OSRClone( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRClone", nullptr );

    return reinterpret_cast<OGRSpatialReferenceH>(
        reinterpret_cast<OGRSpatialReference *>(hSRS)->Clone() );
}

/************************************************************************/
/*                            dumpReadable()                            */
/************************************************************************/

/** Dump pretty wkt to stdout, mostly for debugging.
 */
void OGRSpatialReference::dumpReadable()

{
    char *pszPrettyWkt = nullptr;

    exportToPrettyWkt( &pszPrettyWkt, FALSE );
    printf( "%s\n", pszPrettyWkt );/*ok*/
    CPLFree( pszPrettyWkt );
}

/************************************************************************/
/*                         exportToPrettyWkt()                          */
/************************************************************************/

/**
 * Convert this SRS into a nicely formatted WKT string for display to a person.
 *
 * Note that the returned WKT string should be freed with
 * CPLFree() when no longer needed.  It is the responsibility of the caller.
 *
 * This method is the same as the C function OSRExportToPrettyWkt().
 *
 * @param ppszResult the resulting string is returned in this pointer.
 * @param bSimplify TRUE if the AXIS, AUTHORITY and EXTENSION nodes should be
 *   stripped off.
 *
 * @return currently OGRERR_NONE is always returned, but the future it
 * is possible error conditions will develop.
 */

OGRErr OGRSpatialReference::exportToPrettyWkt( char ** ppszResult,
                                               int bSimplify ) const

{
    if( poRoot == nullptr )
    {
        *ppszResult = CPLStrdup("");
        return OGRERR_NONE;
    }

    if( bSimplify )
    {
        OGRSpatialReference *poSimpleClone = Clone();

        poSimpleClone->GetRoot()->StripNodes( "AXIS" );
        poSimpleClone->GetRoot()->StripNodes( "AUTHORITY" );
        poSimpleClone->GetRoot()->StripNodes( "EXTENSION" );
        const OGRErr eErr =
            poSimpleClone->GetRoot()->exportToPrettyWkt( ppszResult, 1 );
        delete poSimpleClone;
        return eErr;
    }

    return poRoot->exportToPrettyWkt( ppszResult, 1 );
}

/************************************************************************/
/*                        OSRExportToPrettyWkt()                        */
/************************************************************************/

/**
 * \brief Convert this SRS into a nicely formatted WKT string for display to a
 * person.
 *
 * This function is the same as OGRSpatialReference::exportToPrettyWkt().
 */

OGRErr CPL_STDCALL OSRExportToPrettyWkt( OGRSpatialReferenceH hSRS, char ** ppszReturn,
                             int bSimplify)

{
    VALIDATE_POINTER1( hSRS, "OSRExportToPrettyWkt", OGRERR_FAILURE );

    *ppszReturn = nullptr;

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        exportToPrettyWkt( ppszReturn, bSimplify );
}

/************************************************************************/
/*                            exportToWkt()                             */
/************************************************************************/

/**
 * \brief Convert this SRS into WKT format.
 *
 * Note that the returned WKT string should be freed with
 * CPLFree() when no longer needed.  It is the responsibility of the caller.
 *
 * This method is the same as the C function OSRExportToWkt().
 *
 * @param ppszResult the resulting string is returned in this pointer.
 *
 * @return currently OGRERR_NONE is always returned, but the future it
 * is possible error conditions will develop.
 */

OGRErr OGRSpatialReference::exportToWkt( char ** ppszResult ) const

{
    if( poRoot == nullptr )
    {
        *ppszResult = CPLStrdup("");
        return OGRERR_NONE;
    }

    return poRoot->exportToWkt(ppszResult);
}

/************************************************************************/
/*                           OSRExportToWkt()                           */
/************************************************************************/

/**
 * \brief Convert this SRS into WKT format.
 *
 * This function is the same as OGRSpatialReference::exportToWkt().
 */

OGRErr CPL_STDCALL OSRExportToWkt( OGRSpatialReferenceH hSRS,
                                   char ** ppszReturn )

{
    VALIDATE_POINTER1( hSRS, "OSRExportToWkt", OGRERR_FAILURE );

    *ppszReturn = nullptr;

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->exportToWkt( ppszReturn );
}

/************************************************************************/
/*                           importFromWkt()                            */
/************************************************************************/

/**
 * \brief Import from WKT string.
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
    if( !ppszInput || !*ppszInput )
        return OGRERR_FAILURE;

    Clear();

    poRoot = new OGR_SRSNode();

    const OGRErr eErr = poRoot->importFromWkt( ppszInput );
    if( eErr != OGRERR_NONE )
        return eErr;

/* -------------------------------------------------------------------- */
/*      The following seems to try and detect and unconsumed            */
/*      VERTCS[] coordinate system definition (ESRI style) and to       */
/*      import and attach it to the existing root.  Likely we will      */
/*      need to extend this somewhat to bring it into an acceptable     */
/*      OGRSpatialReference organization at some point.                 */
/* -------------------------------------------------------------------- */
    if( strlen(*ppszInput) > 0 && strstr(*ppszInput, "VERTCS") )
    {
        if( ((*ppszInput)[0]) == ',' )
            (*ppszInput)++;
        OGR_SRSNode *poNewChild = new OGR_SRSNode();
        poRoot->AddChild( poNewChild );
        return poNewChild->importFromWkt( ppszInput );
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                          OSRImportFromWkt()                          */
/************************************************************************/

/**
 * \brief Import from WKT string.
 *
 * This function is the same as OGRSpatialReference::importFromWkt().
 */

OGRErr OSRImportFromWkt( OGRSpatialReferenceH hSRS, char **ppszInput )

{
    VALIDATE_POINTER1( hSRS, "OSRImportFromWkt", OGRERR_FAILURE );

    return
        reinterpret_cast<OGRSpatialReference *>(hSRS)->
            importFromWkt( ppszInput );
}

/************************************************************************/
/*                              SetNode()                               */
/************************************************************************/

/**
 * \brief Set attribute value in spatial reference.
 *
 * Missing intermediate nodes in the path will be created if not already
 * in existence.  If the attribute has no children one will be created and
 * assigned the value otherwise the zeroth child will be assigned the value.
 *
 * This method does the same as the C function OSRSetAttrValue().
 *
 * @param pszNodePath full path to attribute to be set.  For instance
 * "PROJCS|GEOGCS|UNIT".
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
    char **papszPathTokens =
        CSLTokenizeStringComplex(pszNodePath, "|", TRUE, FALSE);

    if( CSLCount( papszPathTokens ) < 1 )
    {
        CSLDestroy(papszPathTokens);
        return OGRERR_FAILURE;
    }

    if( GetRoot() == nullptr || !EQUAL(papszPathTokens[0], GetRoot()->GetValue()) )
    {
        SetRoot( new OGR_SRSNode( papszPathTokens[0] ) );
    }

    OGR_SRSNode *poNode = GetRoot();
    for( int i = 1; papszPathTokens[i] != nullptr; i++ )
    {
        int j = 0;  // Used after for.

        for( ; j < poNode->GetChildCount(); j++ )
        {
            if( EQUAL(poNode->GetChild( j )->GetValue(), papszPathTokens[i]) )
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

    if( pszNewNodeValue != nullptr )
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

/**
 * \brief Set attribute value in spatial reference.
 *
 * This function is the same as OGRSpatialReference::SetNode()
 */
OGRErr CPL_STDCALL OSRSetAttrValue( OGRSpatialReferenceH hSRS,
                        const char * pszPath, const char * pszValue )

{
    VALIDATE_POINTER1( hSRS, "OSRSetAttrValue", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        SetNode( pszPath, pszValue );
}

/************************************************************************/
/*                              SetNode()                               */
/************************************************************************/

/**
 * \brief Set attribute value in spatial reference.
 *
 * Missing intermediate nodes in the path will be created if not already
 * in existence.  If the attribute has no children one will be created and
 * assigned the value otherwise the zeroth child will be assigned the value.
 *
 * This method does the same as the C function OSRSetAttrValue().
 *
 * @param pszNodePath full path to attribute to be set.  For instance
 * "PROJCS|GEOGCS|UNIT".
 *
 * @param dfValue value to be assigned to node.
 *
 * @return OGRERR_NONE on success.
 */

OGRErr OGRSpatialReference::SetNode( const char *pszNodePath,
                                     double dfValue )

{
    char szValue[64] = { '\0' };

    if( std::abs(dfValue - static_cast<int>(dfValue)) == 0.0 )
        snprintf( szValue, sizeof(szValue), "%d", static_cast<int>(dfValue) );
    else
        OGRsnPrintDouble( szValue, sizeof(szValue), dfValue );

    return SetNode( pszNodePath, szValue );
}

/************************************************************************/
/*                          SetAngularUnits()                           */
/************************************************************************/

/**
 * \brief Set the angular units for the geographic coordinate system.
 *
 * This method creates a UNIT subnode with the specified values as a
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
    bNormInfoSet = FALSE;

    OGR_SRSNode *poCS = GetAttrNode( "GEOGCS" );

    if( poCS == nullptr )
        return OGRERR_FAILURE;

    char szValue[128] = { '\0' };
    OGRsnPrintDouble( szValue, sizeof(szValue), dfInRadians );

    OGR_SRSNode *poUnits = nullptr;
    if( poCS->FindChild( "UNIT" ) >= 0 )
    {
        poUnits = poCS->GetChild( poCS->FindChild( "UNIT" ) );
        if( poUnits->GetChildCount() < 2 )
            return OGRERR_FAILURE;
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

/**
 * \brief Set the angular units for the geographic coordinate system.
 *
 * This function is the same as OGRSpatialReference::SetAngularUnits()
 */
OGRErr OSRSetAngularUnits( OGRSpatialReferenceH hSRS,
                           const char * pszUnits, double dfInRadians )

{
    VALIDATE_POINTER1( hSRS, "OSRSetAngularUnits", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        SetAngularUnits( pszUnits, dfInRadians );
}

/************************************************************************/
/*                          GetAngularUnits()                           */
/************************************************************************/

/**
 * \brief Fetch angular geographic coordinate system units.
 *
 * If no units are available, a value of "degree" and SRS_UA_DEGREE_CONV
 * will be assumed.  This method only checks directly under the GEOGCS node
 * for units.
 *
 * This method does the same thing as the C function OSRGetAngularUnits().
 *
 * @param ppszName a pointer to be updated with the pointer to the units name.
 * The returned value remains internal to the OGRSpatialReference and should
 * not be freed, or modified.  It may be invalidated on the next
 * OGRSpatialReference call.
 *
 * @return the value to multiply by angular distances to transform them to
 * radians.
 */

double OGRSpatialReference::GetAngularUnits( char ** ppszName ) const

{
    const OGR_SRSNode *poCS = GetAttrNode( "GEOGCS" );

    if( ppszName != nullptr )
        *ppszName = (char* ) "degree";

    if( poCS == nullptr )
        return CPLAtof(SRS_UA_DEGREE_CONV);

    for( int iChild = 0; iChild < poCS->GetChildCount(); iChild++ )
    {
        const OGR_SRSNode *poChild = poCS->GetChild(iChild);

        if( EQUAL(poChild->GetValue(), "UNIT")
            && poChild->GetChildCount() >= 2 )
        {
            if( ppszName != nullptr )
              *ppszName =
                  const_cast<char *>(poChild->GetChild(0)->GetValue());

            return CPLAtof( poChild->GetChild(1)->GetValue() );
        }
    }

    return 1.0;
}

/************************************************************************/
/*                         OSRGetAngularUnits()                         */
/************************************************************************/

/**
 * \brief Fetch angular geographic coordinate system units.
 *
 * This function is the same as OGRSpatialReference::GetAngularUnits()
 */
double OSRGetAngularUnits( OGRSpatialReferenceH hSRS, char ** ppszName )

{
    VALIDATE_POINTER1( hSRS, "OSRGetAngularUnits", 0 );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        GetAngularUnits( ppszName );
}

/************************************************************************/
/*                 SetLinearUnitsAndUpdateParameters()                  */
/************************************************************************/

/**
 * \brief Set the linear units for the projection.
 *
 * This method creates a UNIT subnode with the specified values as a
 * child of the PROJCS or LOCAL_CS node.   It works the same as the
 * SetLinearUnits() method, but it also updates all existing linear
 * projection parameter values from the old units to the new units.
 *
 * @param pszName the units name to be used.  Some preferred units
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
    if( dfInMeters == 0.0 )
        return OGRERR_FAILURE;

    const double dfOldInMeters = GetLinearUnits();
    OGR_SRSNode *poPROJCS = GetAttrNode( "PROJCS" );

    if( dfInMeters == dfOldInMeters || poPROJCS == nullptr )
        return SetLinearUnits( pszName, dfInMeters );

    for( int iChild = 0; iChild < poPROJCS->GetChildCount(); iChild++ )
    {
        const OGR_SRSNode *poChild = poPROJCS->GetChild(iChild);

        if( EQUAL(poChild->GetValue(), "PARAMETER")
            && poChild->GetChildCount() > 1 )
        {
            char *const pszParmName =
                CPLStrdup(poChild->GetChild(0)->GetValue());

            if( IsLinearParameter( pszParmName ) )
            {
                const double dfOldValue = GetProjParm( pszParmName );

                SetProjParm( pszParmName,
                             dfOldValue * dfOldInMeters / dfInMeters );
            }

            CPLFree( pszParmName );
        }
    }

    return SetLinearUnits( pszName, dfInMeters );
}

/************************************************************************/
/*                OSRSetLinearUnitsAndUpdateParameters()                */
/************************************************************************/

/**
 * \brief Set the linear units for the projection.
 *
 * This function is the same as
 *   OGRSpatialReference::SetLinearUnitsAndUpdateParameters()
 */
OGRErr OSRSetLinearUnitsAndUpdateParameters( OGRSpatialReferenceH hSRS,
                                             const char * pszUnits,
                                             double dfInMeters )

{
    VALIDATE_POINTER1( hSRS, "OSRSetLinearUnitsAndUpdateParameters",
                       OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        SetLinearUnitsAndUpdateParameters( pszUnits, dfInMeters );
}

/************************************************************************/
/*                           SetLinearUnits()                           */
/************************************************************************/

/**
 * \brief Set the linear units for the projection.
 *
 * This method creates a UNIT subnode with the specified values as a
 * child of the PROJCS, GEOCCS or LOCAL_CS node.
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
    return SetTargetLinearUnits( nullptr, pszUnitsName, dfInMeters );
}

/************************************************************************/
/*                         OSRSetLinearUnits()                          */
/************************************************************************/

/**
 * \brief Set the linear units for the projection.
 *
 * This function is the same as OGRSpatialReference::SetLinearUnits()
 */
OGRErr OSRSetLinearUnits( OGRSpatialReferenceH hSRS,
                          const char * pszUnits, double dfInMeters )

{
    VALIDATE_POINTER1( hSRS, "OSRSetLinearUnits", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        SetLinearUnits( pszUnits, dfInMeters );
}

/************************************************************************/
/*                        SetTargetLinearUnits()                        */
/************************************************************************/

/**
 * \brief Set the linear units for the projection.
 *
 * This method creates a UNIT subnode with the specified values as a
 * child of the target node.
 *
 * This method does the same as the C function OSRSetTargetLinearUnits().
 *
 * @param pszTargetKey the keyword to set the linear units for.
 * i.e. "PROJCS" or "VERT_CS"
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
 *
 * @since OGR 1.9.0
 */

OGRErr OGRSpatialReference::SetTargetLinearUnits( const char *pszTargetKey,
                                                  const char * pszUnitsName,
                                                  double dfInMeters )

{
    if( dfInMeters <= 0.0 )
        return OGRERR_FAILURE;

    bNormInfoSet = FALSE;

    OGR_SRSNode *poCS = nullptr;
    if( pszTargetKey == nullptr )
    {
        poCS = GetAttrNode( "PROJCS" );

        if( poCS == nullptr )
            poCS = GetAttrNode( "LOCAL_CS" );
        if( poCS == nullptr )
            poCS = GetAttrNode( "GEOCCS" );
        if( poCS == nullptr && IsVertical() )
            poCS = GetAttrNode( "VERT_CS" );
    }
    else
        poCS = GetAttrNode( pszTargetKey );

    if( poCS == nullptr )
        return OGRERR_FAILURE;

    char szValue[128] = { '\0' };
    if( dfInMeters < std::numeric_limits<int>::max() &&
        dfInMeters > std::numeric_limits<int>::min() &&
        dfInMeters == static_cast<int>(dfInMeters) )
        snprintf( szValue, sizeof(szValue),
                  "%d", static_cast<int>(dfInMeters) );
    else
        OGRsnPrintDouble( szValue, sizeof(szValue), dfInMeters );

    OGR_SRSNode *poUnits = nullptr;
    if( poCS->FindChild( "UNIT" ) >= 0 )
    {
        poUnits = poCS->GetChild( poCS->FindChild( "UNIT" ) );
        if( poUnits->GetChildCount() < 2 )
            return OGRERR_FAILURE;
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

/**
 * \brief Set the linear units for the target node.
 *
 * This function is the same as OGRSpatialReference::SetTargetLinearUnits()
 *
 * @since OGR 1.9.0
 */
OGRErr OSRSetTargetLinearUnits( OGRSpatialReferenceH hSRS,
                                const char *pszTargetKey,
                                const char * pszUnits, double dfInMeters )

{
    VALIDATE_POINTER1( hSRS, "OSRSetTargetLinearUnits", OGRERR_FAILURE );

    return ((OGRSpatialReference *) hSRS)->
        SetTargetLinearUnits( pszTargetKey, pszUnits, dfInMeters );
}

/************************************************************************/
/*                           GetLinearUnits()                           */
/************************************************************************/

/**
 * \brief Fetch linear projection units.
 *
 * If no units are available, a value of "Meters" and 1.0 will be assumed.
 * This method only checks directly under the PROJCS, GEOCCS or LOCAL_CS node
 * for units.
 *
 * This method does the same thing as the C function OSRGetLinearUnits()
 *
 * @param ppszName a pointer to be updated with the pointer to the units name.
 * The returned value remains internal to the OGRSpatialReference and should
 * not be freed, or modified.  It may be invalidated on the next
 * OGRSpatialReference call.
 *
 * @return the value to multiply by linear distances to transform them to
 * meters.
 */

double OGRSpatialReference::GetLinearUnits( char ** ppszName ) const

{
    return GetTargetLinearUnits( nullptr, ppszName );
}

/************************************************************************/
/*                         OSRGetLinearUnits()                          */
/************************************************************************/

/**
 * \brief Fetch linear projection units.
 *
 * This function is the same as OGRSpatialReference::GetLinearUnits()
 */
double OSRGetLinearUnits( OGRSpatialReferenceH hSRS, char ** ppszName )

{
    VALIDATE_POINTER1( hSRS, "OSRGetLinearUnits", 0 );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->GetLinearUnits( ppszName );
}

/************************************************************************/
/*                        GetTargetLinearUnits()                        */
/************************************************************************/

/**
 * \brief Fetch linear units for target.
 *
 * If no units are available, a value of "Meters" and 1.0 will be assumed.
 *
 * This method does the same thing as the C function OSRGetTargetLinearUnits()
 *
 * @param pszTargetKey the key to look on. i.e. "PROJCS" or "VERT_CS". Might be
 * NULL, in which case PROJCS will be implied (and if not found, LOCAL_CS,
 * GEOCCS and VERT_CS are looked up)
 * @param ppszName a pointer to be updated with the pointer to the units name.
 * The returned value remains internal to the OGRSpatialReference and should not
 * be freed, or modified.  It may be invalidated on the next
 * OGRSpatialReference call. ppszName can be set to NULL.
 *
 * @return the value to multiply by linear distances to transform them to
 * meters.
 *
 * @since OGR 1.9.0
 */

double OGRSpatialReference::GetTargetLinearUnits( const char *pszTargetKey,
                                                  char ** ppszName ) const

{
    const OGR_SRSNode *poCS = nullptr;

    if( pszTargetKey == nullptr )
    {
        poCS = GetAttrNode( "PROJCS" );

        if( poCS == nullptr )
            poCS = GetAttrNode( "LOCAL_CS" );
        if( poCS == nullptr )
            poCS = GetAttrNode( "GEOCCS" );
        if( poCS == nullptr && IsVertical() )
            poCS = GetAttrNode( "VERT_CS" );
    }
    else
        poCS = GetAttrNode( pszTargetKey );

    if( ppszName != nullptr )
        *ppszName = (char*) "unknown";

    if( poCS == nullptr )
        return 1.0;

    for( int iChild = 0; iChild < poCS->GetChildCount(); iChild++ )
    {
        const OGR_SRSNode *poChild = poCS->GetChild(iChild);

        if( EQUAL(poChild->GetValue(), "UNIT")
            && poChild->GetChildCount() >= 2 )
        {
            if( ppszName != nullptr )
              *ppszName = const_cast<char *>(poChild->GetChild(0)->GetValue());

            return CPLAtof( poChild->GetChild(1)->GetValue() );
        }
    }

    return 1.0;
}

/************************************************************************/
/*                      OSRGetTargetLinearUnits()                       */
/************************************************************************/

/**
 * \brief Fetch linear projection units.
 *
 * This function is the same as OGRSpatialReference::GetTargetLinearUnits()
 *
 * @since OGR 1.9.0
 */
double OSRGetTargetLinearUnits( OGRSpatialReferenceH hSRS,
                                const char *pszTargetKey,
                                char ** ppszName )

{
    VALIDATE_POINTER1( hSRS, "OSRGetTargetLinearUnits", 0 );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        GetTargetLinearUnits( pszTargetKey, ppszName );
}

/************************************************************************/
/*                          GetPrimeMeridian()                          */
/************************************************************************/

/**
 * \brief Fetch prime meridian info.
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

    if( poPRIMEM != nullptr && poPRIMEM->GetChildCount() >= 2
        && CPLAtof(poPRIMEM->GetChild(1)->GetValue()) != 0.0 )
    {
        if( ppszName != nullptr )
            *ppszName = const_cast<char *>(poPRIMEM->GetChild(0)->GetValue());
        return CPLAtof(poPRIMEM->GetChild(1)->GetValue());
    }

    if( ppszName != nullptr )
        *ppszName = (char*) SRS_PM_GREENWICH;

    return 0.0;
}

/************************************************************************/
/*                        OSRGetPrimeMeridian()                         */
/************************************************************************/

/**
 * \brief Fetch prime meridian info.
 *
 * This function is the same as OGRSpatialReference::GetPrimeMeridian()
 */
double OSRGetPrimeMeridian( OGRSpatialReferenceH hSRS, char **ppszName )

{
    VALIDATE_POINTER1( hSRS, "OSRGetPrimeMeridian", 0 );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        GetPrimeMeridian( ppszName );
}

/************************************************************************/
/*                             SetGeogCS()                              */
/************************************************************************/

/**
 * \brief Set geographic coordinate system.
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
 * @param pszPMName the name of the prime meridian (not to serve as a key)
 * If this is NULL a default value of "Greenwich" will be used.
 *
 * @param dfPMOffset the longitude of Greenwich relative to this prime
 * meridian.
 *
 * @param pszAngularUnits the angular units name (see ogr_srs_api.h for some
 * standard names).  If NULL a value of "degrees" will be assumed.
 *
 * @param dfConvertToRadians value to multiply angular units by to transform
 * them to radians.  A value of SRS_UA_DEGREE_CONV will be used if
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
/*      For a geocentric coordinate system we want to set the datum     */
/*      and ellipsoid based on the GEOGCS.  Create the GEOGCS in a      */
/*      temporary srs and use the copy method which has special         */
/*      handling for GEOCCS.                                            */
/* -------------------------------------------------------------------- */
    if( IsGeocentric() )
    {
        OGRSpatialReference oGCS;

        oGCS.SetGeogCS( pszGeogName, pszDatumName, pszSpheroidName,
                        dfSemiMajor, dfInvFlattening,
                        pszPMName, dfPMOffset,
                        pszAngularUnits, dfConvertToRadians );
        return CopyGeogCSFrom( &oGCS );
    }

/* -------------------------------------------------------------------- */
/*      Do we already have a GEOGCS?  If so, blow it away so it can     */
/*      be properly replaced.                                           */
/* -------------------------------------------------------------------- */
    if( GetAttrNode( "GEOGCS" ) != nullptr )
    {
        OGR_SRSNode *poCS = nullptr;

        if( EQUAL(GetRoot()->GetValue(), "GEOGCS") )
            Clear();
        else if( (poCS = GetAttrNode( "PROJCS" )) != nullptr
                 && poCS->FindChild( "GEOGCS" ) != -1 )
            poCS->DestroyChild( poCS->FindChild( "GEOGCS" ) );
        else
            return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Set defaults for various parameters.                            */
/* -------------------------------------------------------------------- */
    if( pszGeogName == nullptr )
        pszGeogName = "unnamed";

    if( pszPMName == nullptr )
        pszPMName = SRS_PM_GREENWICH;

    if( pszDatumName == nullptr )
        pszDatumName = "unknown";

    if( pszSpheroidName == nullptr )
        pszSpheroidName = "unnamed";

    if( pszAngularUnits == nullptr )
    {
        pszAngularUnits = SRS_UA_DEGREE;
        dfConvertToRadians = CPLAtof(SRS_UA_DEGREE_CONV);
    }

/* -------------------------------------------------------------------- */
/*      Build the GEOGCS object.                                        */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poGeogCS = new OGR_SRSNode( "GEOGCS" );
    poGeogCS->AddChild( new OGR_SRSNode( pszGeogName ) );

/* -------------------------------------------------------------------- */
/*      Setup the spheroid.                                             */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poSpheroid = new OGR_SRSNode( "SPHEROID" );
    poSpheroid->AddChild( new OGR_SRSNode( pszSpheroidName ) );

    char szValue[128] = {};
    OGRsnPrintDouble( szValue, sizeof(szValue), dfSemiMajor );
    poSpheroid->AddChild( new OGR_SRSNode(szValue) );

    OGRsnPrintDouble( szValue, sizeof(szValue), dfInvFlattening );
    poSpheroid->AddChild( new OGR_SRSNode(szValue) );

/* -------------------------------------------------------------------- */
/*      Setup the Datum.                                                */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poDatum = new OGR_SRSNode( "DATUM" );
    poDatum->AddChild( new OGR_SRSNode(pszDatumName) );
    poDatum->AddChild( poSpheroid );

/* -------------------------------------------------------------------- */
/*      Setup the prime meridian.                                       */
/* -------------------------------------------------------------------- */
    if( dfPMOffset == 0.0 )
        strcpy( szValue, "0" );
    else
      OGRsnPrintDouble( szValue, sizeof(szValue), dfPMOffset );

    OGR_SRSNode *poPM = new OGR_SRSNode( "PRIMEM" );
    poPM->AddChild( new OGR_SRSNode( pszPMName ) );
    poPM->AddChild( new OGR_SRSNode( szValue ) );

/* -------------------------------------------------------------------- */
/*      Setup the rotational units.                                     */
/* -------------------------------------------------------------------- */
    OGRsnPrintDouble( szValue, sizeof(szValue), dfConvertToRadians );

    OGR_SRSNode *poUnits = new OGR_SRSNode( "UNIT" );
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
    if( GetRoot() != nullptr && EQUAL(GetRoot()->GetValue(), "PROJCS") )
        poRoot->InsertChild( poGeogCS, 1 );
    else
        SetRoot( poGeogCS );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            OSRSetGeogCS()                            */
/************************************************************************/

/**
 * \brief Set geographic coordinate system.
 *
 * This function is the same as OGRSpatialReference::SetGeogCS()
 */
OGRErr OSRSetGeogCS( OGRSpatialReferenceH hSRS,
                     const char * pszGeogName,
                     const char * pszDatumName,
                     const char * pszSpheroidName,
                     double dfSemiMajor, double dfInvFlattening,
                     const char * pszPMName, double dfPMOffset,
                     const char * pszAngularUnits,
                     double dfConvertToRadians )

{
    VALIDATE_POINTER1( hSRS, "OSRSetGeogCS", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetGeogCS(
        pszGeogName, pszDatumName,
        pszSpheroidName, dfSemiMajor, dfInvFlattening,
        pszPMName, dfPMOffset, pszAngularUnits, dfConvertToRadians );
}

/************************************************************************/
/*                         SetWellKnownGeogCS()                         */
/************************************************************************/

/**
 * \brief Set a GeogCS based on well known name.
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
/* -------------------------------------------------------------------- */
/*      Check for EPSG authority numbers.                               */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszName, "EPSG:") )
    {
        OGRSpatialReference oSRS2;
        const OGRErr eErr = oSRS2.importFromEPSG( atoi(pszName+5) );
        if( eErr != OGRERR_NONE )
            return eErr;

        if( !oSRS2.IsGeographic() )
            return OGRERR_FAILURE;

        return CopyGeogCSFrom( &oSRS2 );
    }

/* -------------------------------------------------------------------- */
/*      Check for EPSGA authority numbers.                               */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszName, "EPSGA:") )
    {
        OGRSpatialReference oSRS2;
        const OGRErr eErr = oSRS2.importFromEPSGA( atoi(pszName+6) );
        if( eErr != OGRERR_NONE )
            return eErr;

        if( !oSRS2.IsGeographic() )
            return OGRERR_FAILURE;

        return CopyGeogCSFrom( &oSRS2 );
    }

/* -------------------------------------------------------------------- */
/*      Check for simple names.                                         */
/* -------------------------------------------------------------------- */
    char *pszWKT = nullptr;

    if( EQUAL(pszName, "WGS84") || EQUAL(pszName, "CRS84") ||
        EQUAL(pszName, "CRS:84") )
        pszWKT = (char* ) SRS_WKT_WGS84;

    else if( EQUAL(pszName, "WGS72") )
        pszWKT = const_cast<char *>(
            "GEOGCS[\"WGS 72\",DATUM[\"WGS_1972\","
            "SPHEROID[\"WGS 72\",6378135,298.26,AUTHORITY[\"EPSG\",\"7043\"]],"
            "TOWGS84[0,0,4.5,0,0,0.554,0.2263],AUTHORITY[\"EPSG\",\"6322\"]],"
            "PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],"
            "UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9122\"]],"
            "AUTHORITY[\"EPSG\",\"4322\"]]" );

    else if( EQUAL(pszName, "NAD27") || EQUAL(pszName, "CRS27") ||
             EQUAL(pszName, "CRS:27") )
        pszWKT = const_cast<char *>(
            "GEOGCS[\"NAD27\",DATUM[\"North_American_Datum_1927\","
            "SPHEROID[\"Clarke 1866\",6378206.4,294.9786982138982,"
            "AUTHORITY[\"EPSG\",\"7008\"]],AUTHORITY[\"EPSG\",\"6267\"]],"
            "PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],"
            "UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9122\"]],"
            "AUTHORITY[\"EPSG\",\"4267\"]]" );

    else if( EQUAL(pszName, "NAD83") || EQUAL(pszName, "CRS83") ||
             EQUAL(pszName, "CRS:83") )
        pszWKT = const_cast<char *>(
            "GEOGCS[\"NAD83\",DATUM[\"North_American_Datum_1983\","
            "SPHEROID[\"GRS 1980\",6378137,298.257222101,"
            "AUTHORITY[\"EPSG\",\"7019\"]],TOWGS84[0,0,0,0,0,0,0],"
            "AUTHORITY[\"EPSG\",\"6269\"]],PRIMEM[\"Greenwich\",0,"
            "AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,"
            "AUTHORITY[\"EPSG\",\"9122\"]],AUTHORITY[\"EPSG\",\"4269\"]]" );

    else
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Import the WKT                                                  */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS2;
    const OGRErr eErr = oSRS2.importFromWkt( &pszWKT );
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

/**
 * \brief Set a GeogCS based on well known name.
 *
 * This function is the same as OGRSpatialReference::SetWellKnownGeogCS()
 */
OGRErr OSRSetWellKnownGeogCS( OGRSpatialReferenceH hSRS, const char *pszName )

{
    VALIDATE_POINTER1( hSRS, "OSRSetWellKnownGeogCS", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        SetWellKnownGeogCS( pszName );
}

/************************************************************************/
/*                           CopyGeogCSFrom()                           */
/************************************************************************/

/**
 * \brief Copy GEOGCS from another OGRSpatialReference.
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
    bNormInfoSet = FALSE;

/* -------------------------------------------------------------------- */
/*      Handle geocentric coordinate systems specially.  We just        */
/*      want to copy the DATUM and PRIMEM nodes.                        */
/* -------------------------------------------------------------------- */
    if( IsGeocentric() )
    {
        if( GetRoot()->FindChild( "DATUM" ) != -1 )
            GetRoot()->DestroyChild( GetRoot()->FindChild( "DATUM" ) );
        if( GetRoot()->FindChild( "PRIMEM" ) != -1 )
            GetRoot()->DestroyChild( GetRoot()->FindChild( "PRIMEM" ) );

        const OGR_SRSNode *poDatum = poSrcSRS->GetAttrNode( "DATUM" );
        const OGR_SRSNode *poPrimeM = poSrcSRS->GetAttrNode( "PRIMEM" );

        if( poDatum == nullptr || poPrimeM == nullptr )
            return OGRERR_FAILURE;

        poRoot->InsertChild( poDatum->Clone(), 1 );
        poRoot->InsertChild( poPrimeM->Clone(), 2 );

        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Do we already have a GEOGCS?  If so, blow it away so it can     */
/*      be properly replaced.                                           */
/* -------------------------------------------------------------------- */
    if( GetAttrNode( "GEOGCS" ) != nullptr )
    {
        OGR_SRSNode *poPROJCS = nullptr;

        if( EQUAL(GetRoot()->GetValue(), "GEOGCS") )
            Clear();
        else if( (poPROJCS = GetAttrNode( "PROJCS" )) != nullptr
                 && poPROJCS->FindChild( "GEOGCS" ) != -1 )
            poPROJCS->DestroyChild( poPROJCS->FindChild( "GEOGCS" ) );
        else
            return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Find the GEOGCS node on the source.                             */
/* -------------------------------------------------------------------- */
    const OGR_SRSNode *poGeogCS = poSrcSRS->GetAttrNode( "GEOGCS" );
    if( poGeogCS == nullptr )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Attach below the PROJCS if there is one, or make this the root. */
/* -------------------------------------------------------------------- */
    if( GetRoot() != nullptr && EQUAL(GetRoot()->GetValue(), "PROJCS") )
        poRoot->InsertChild( poGeogCS->Clone(), 1 );
    else
        SetRoot( poGeogCS->Clone() );

    return OGRERR_NONE;
}

/************************************************************************/
/*                         OSRCopyGeogCSFrom()                          */
/************************************************************************/

/**
 * \brief Copy GEOGCS from another OGRSpatialReference.
 *
 * This function is the same as OGRSpatialReference::CopyGeogCSFrom()
 */
OGRErr OSRCopyGeogCSFrom( OGRSpatialReferenceH hSRS,
                          const OGRSpatialReferenceH hSrcSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRCopyGeogCSFrom", OGRERR_FAILURE );
    VALIDATE_POINTER1( hSrcSRS, "OSRCopyGeogCSFrom", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->CopyGeogCSFrom(
        reinterpret_cast<const OGRSpatialReference *>(hSrcSRS) );
}

/************************************************************************/
/*                          SetFromUserInput()                          */
/************************************************************************/

/**
 * \brief Set spatial reference from various text formats.
 *
 * This method will examine the provided input, and try to deduce the
 * format, and then use it to initialize the spatial reference system.  It
 * may take the following forms:
 *
 * <ol>
 * <li> Well Known Text definition - passed on to importFromWkt().
 * <li> "EPSG:n" - number passed on to importFromEPSG().
 * <li> "EPSGA:n" - number passed on to importFromEPSGA().
 * <li> "AUTO:proj_id,unit_id,lon0,lat0" - WMS auto projections.
 * <li> "urn:ogc:def:crs:EPSG::n" - ogc urns
 * <li> PROJ.4 definitions - passed on to importFromProj4().
 * <li> filename - file read for WKT, XML or PROJ.4 definition.
 * <li> well known name accepted by SetWellKnownGeogCS(), such as NAD27, NAD83,
 * WGS84 or WGS72.
 * <li> WKT (directly or in a file) in ESRI format should be prefixed with
 * ESRI:: to trigger an automatic morphFromESRI().
 * <li> "IGNF:xxx" - "+init=IGNF:xxx" passed on to importFromProj4().
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
    bool bESRI = false;

    if( STARTS_WITH_CI(pszDefinition, "ESRI::") )
    {
        bESRI = true;
        pszDefinition += 6;
    }

/* -------------------------------------------------------------------- */
/*      Is it a recognised syntax?                                      */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszDefinition, "PROJCS")
        || STARTS_WITH_CI(pszDefinition, "GEOGCS")
        || STARTS_WITH_CI(pszDefinition, "COMPD_CS")
        || STARTS_WITH_CI(pszDefinition, "GEOCCS")
        || STARTS_WITH_CI(pszDefinition, "VERT_CS")
        || STARTS_WITH_CI(pszDefinition, "LOCAL_CS") )
    {
        OGRErr err = importFromWkt( (char **) &pszDefinition );
        if( err == OGRERR_NONE && bESRI )
            err = morphFromESRI();

        return err;
    }

    if( STARTS_WITH_CI(pszDefinition, "EPSG:")
        || STARTS_WITH_CI(pszDefinition, "EPSGA:") )
    {
        OGRErr eStatus = OGRERR_NONE;

        if( STARTS_WITH_CI(pszDefinition, "EPSG:") )
            eStatus = importFromEPSG( atoi(pszDefinition+5) );

        else // if( STARTS_WITH_CI(pszDefinition, "EPSGA:") )
            eStatus = importFromEPSGA( atoi(pszDefinition+6) );

        // Do we want to turn this into a compound definition
        // with a vertical datum?
        if( eStatus == OGRERR_NONE && strchr( pszDefinition, '+' ) != nullptr )
        {
            OGRSpatialReference oVertSRS;

            eStatus = oVertSRS.importFromEPSG(
                atoi(strchr(pszDefinition, '+') + 1) );
            if( eStatus == OGRERR_NONE )
            {
                OGR_SRSNode *poHorizSRS = GetRoot()->Clone();

                Clear();

                CPLString osName = poHorizSRS->GetChild(0)->GetValue();
                osName += " + ";
                osName += oVertSRS.GetRoot()->GetChild(0)->GetValue();

                SetNode( "COMPD_CS", osName );
                GetRoot()->AddChild( poHorizSRS );
                GetRoot()->AddChild( oVertSRS.GetRoot()->Clone() );
            }
        }

        return eStatus;
    }

    if( STARTS_WITH_CI(pszDefinition, "urn:ogc:def:crs:")
        || STARTS_WITH_CI(pszDefinition, "urn:ogc:def:crs,crs:")
        || STARTS_WITH_CI(pszDefinition, "urn:x-ogc:def:crs:")
        || STARTS_WITH_CI(pszDefinition, "urn:opengis:crs:")
        || STARTS_WITH_CI(pszDefinition, "urn:opengis:def:crs:"))
        return importFromURN( pszDefinition );

    if( STARTS_WITH_CI(pszDefinition, "http://opengis.net/def/crs")
        || STARTS_WITH_CI(pszDefinition, "http://www.opengis.net/def/crs")
        || STARTS_WITH_CI(pszDefinition, "www.opengis.net/def/crs"))
        return importFromCRSURL( pszDefinition );

    if( STARTS_WITH_CI(pszDefinition, "AUTO:") )
        return importFromWMSAUTO( pszDefinition );

    // WMS/WCS OGC codes like OGC:CRS84.
    if( STARTS_WITH_CI(pszDefinition, "OGC:") )
        return SetWellKnownGeogCS( pszDefinition+4 );

    if( STARTS_WITH_CI(pszDefinition, "CRS:") )
        return SetWellKnownGeogCS( pszDefinition );

    if( STARTS_WITH_CI(pszDefinition, "DICT:")
        && strstr(pszDefinition, ",") )
    {
        char *pszFile = CPLStrdup(pszDefinition+5);
        char *pszCode = strstr(pszFile, ",") + 1;

        pszCode[-1] = '\0';

        OGRErr err = importFromDict( pszFile, pszCode );
        CPLFree( pszFile );

        if( err == OGRERR_NONE && bESRI )
            err = morphFromESRI();

        return err;
    }

    if( EQUAL(pszDefinition, "NAD27")
        || EQUAL(pszDefinition,"NAD83")
        || EQUAL(pszDefinition,"WGS84")
        || EQUAL(pszDefinition,"WGS72") )
    {
        Clear();
        return SetWellKnownGeogCS( pszDefinition );
    }

    if( strstr(pszDefinition, "+proj") != nullptr
             || strstr(pszDefinition, "+init") != nullptr )
        return importFromProj4( pszDefinition );

    if( STARTS_WITH_CI(pszDefinition, "IGNF:") )
    {
        char* pszProj4Str =
            static_cast<char *>(CPLMalloc(6 + strlen(pszDefinition) + 1));
        strcpy(pszProj4Str, "+init=");
        strcat(pszProj4Str, pszDefinition);
        const OGRErr err = importFromProj4( pszProj4Str );
        CPLFree(pszProj4Str);

        return err;
    }

    if( STARTS_WITH_CI(pszDefinition, "http://") )
    {
        return importFromUrl (pszDefinition);
    }

    if( EQUAL(pszDefinition, "osgb:BNG") )
    {
        return importFromEPSG(27700);
    }

/* -------------------------------------------------------------------- */
/*      Try to open it as a file.                                       */
/* -------------------------------------------------------------------- */
    CPLConfigOptionSetter oSetter("CPL_ALLOW_VSISTDIN", "NO", true);
    VSILFILE * const fp = VSIFOpenL( pszDefinition, "rt" );
    if( fp == nullptr )
        return OGRERR_CORRUPT_DATA;

    const size_t nBufMax = 100000;
    char * const pszBuffer = static_cast<char *>( CPLMalloc(nBufMax) );
    const size_t nBytes = VSIFReadL( pszBuffer, 1, nBufMax-1, fp );
    VSIFCloseL( fp );

    if( nBytes == nBufMax-1 )
    {
        CPLDebug( "OGR",
                  "OGRSpatialReference::SetFromUserInput(%s), opened file "
                  "but it is to large for our generous buffer.  Is it really "
                  "just a WKT definition?", pszDefinition );
        CPLFree( pszBuffer );
        return OGRERR_FAILURE;
    }

    pszBuffer[nBytes] = '\0';

    char *pszBufPtr = pszBuffer;
    while( pszBufPtr[0] == ' ' || pszBufPtr[0] == '\n' )
        pszBufPtr++;

    OGRErr err = OGRERR_NONE;
    if( pszBufPtr[0] == '<' )
        err = importFromXML( pszBufPtr );
    else if( (strstr(pszBuffer, "+proj") != nullptr
              || strstr(pszBuffer, "+init") != nullptr)
             && (strstr(pszBuffer, "EXTENSION") == nullptr
                 && strstr(pszBuffer, "extension") == nullptr) )
        err = importFromProj4( pszBufPtr );
    else
    {
        if( STARTS_WITH_CI(pszBufPtr, "ESRI::") )
        {
            bESRI = true;
            pszBufPtr += 6;
        }

        // coverity[tainted_data]
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

/**
 * \brief Set spatial reference from various text formats.
 *
 * This function is the same as OGRSpatialReference::SetFromUserInput()
 */
OGRErr CPL_STDCALL OSRSetFromUserInput( OGRSpatialReferenceH hSRS,
                                        const char *pszDef )

{
    VALIDATE_POINTER1( hSRS, "OSRSetFromUserInput", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetFromUserInput( pszDef );
}

/************************************************************************/
/*                          ImportFromUrl()                             */
/************************************************************************/

/**
 * \brief Set spatial reference from a URL.
 *
 * This method will download the spatial reference at a given URL and
 * feed it into SetFromUserInput for you.
 *
 * This method does the same thing as the OSRImportFromUrl() function.
 *
 * @param pszUrl text definition to try to deduce SRS from.
 *
 * @return OGRERR_NONE on success, or an error code with the curl
 * error message if it is unable to dowload data.
 */

OGRErr OGRSpatialReference::importFromUrl( const char * pszUrl )

{
    if( !STARTS_WITH_CI(pszUrl, "http://") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "The given string is not recognized as a URL"
                  "starting with 'http://' -- %s", pszUrl );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Fetch the result.                                               */
/* -------------------------------------------------------------------- */
    CPLErrorReset();

    const char* pszHeaders = "HEADERS=Accept: application/x-ogcwkt";
    const char* pszTimeout = "TIMEOUT=10";
    char *apszOptions[] = {
        const_cast<char *>(pszHeaders),
        const_cast<char *>(pszTimeout),
        nullptr
    };

    CPLHTTPResult *psResult = CPLHTTPFetch( pszUrl, apszOptions );

/* -------------------------------------------------------------------- */
/*      Try to handle errors.                                           */
/* -------------------------------------------------------------------- */

    if( psResult == nullptr )
        return OGRERR_FAILURE;
    if( psResult->nDataLen == 0
        || CPLGetLastErrorNo() != 0 || psResult->pabyData == nullptr )
    {
        if( CPLGetLastErrorNo() == 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "No data was returned from the given URL" );
        }
        CPLHTTPDestroyResult( psResult );
        return OGRERR_FAILURE;
    }

    if( psResult->nStatus != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Curl reports error: %d: %s",
                  psResult->nStatus, psResult->pszErrBuf );
        CPLHTTPDestroyResult( psResult );
        return OGRERR_FAILURE;
    }

    if( STARTS_WITH_CI((const char*) psResult->pabyData, "http://") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "The data that was downloaded also starts with 'http://' "
                  "and cannot be passed into SetFromUserInput.  Is this "
                  "really a spatial reference definition? ");
        CPLHTTPDestroyResult( psResult );
        return OGRERR_FAILURE;
    }
    if( OGRERR_NONE != SetFromUserInput( (const char *) psResult->pabyData )) {
        CPLHTTPDestroyResult( psResult );
        return OGRERR_FAILURE;
    }

    CPLHTTPDestroyResult( psResult );
    return OGRERR_NONE;
}

/************************************************************************/
/*                        OSRimportFromUrl()                            */
/************************************************************************/

/**
 * \brief Set spatial reference from a URL.
 *
 * This function is the same as OGRSpatialReference::importFromUrl()
 */
OGRErr OSRImportFromUrl( OGRSpatialReferenceH hSRS, const char *pszUrl )

{
    VALIDATE_POINTER1( hSRS, "OSRImportFromUrl", OGRERR_FAILURE );

    return
        reinterpret_cast<OGRSpatialReference *>(hSRS)->importFromUrl( pszUrl );
}

/************************************************************************/
/*                         importFromURNPart()                          */
/************************************************************************/
OGRErr OGRSpatialReference::importFromURNPart(const char* pszAuthority,
                                              const char* pszCode,
                                              const char* pszURN)
{
/* -------------------------------------------------------------------- */
/*      Is this an EPSG code? Note that we import it with EPSG          */
/*      preferred axis ordering for geographic coordinate systems.      */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszAuthority, "EPSG") )
        return importFromEPSGA( atoi(pszCode) );

/* -------------------------------------------------------------------- */
/*      Is this an IAU code?  Lets try for the IAU2000 dictionary.      */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszAuthority, "IAU") )
        return importFromDict( "IAU2000.wkt", pszCode );

/* -------------------------------------------------------------------- */
/*      Is this an OGC code?                                            */
/* -------------------------------------------------------------------- */
    if( !STARTS_WITH_CI(pszAuthority, "OGC") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "URN %s has unrecognized authority.",
                  pszURN );
        return OGRERR_FAILURE;
    }

    if( STARTS_WITH_CI(pszCode, "CRS84") )
        return SetWellKnownGeogCS( pszCode );
    else if( STARTS_WITH_CI(pszCode, "CRS83") )
        return SetWellKnownGeogCS( pszCode );
    else if( STARTS_WITH_CI(pszCode, "CRS27") )
        return SetWellKnownGeogCS( pszCode );
    else if( STARTS_WITH_CI(pszCode, "84") )  // urn:ogc:def:crs:OGC:2:84
        return SetWellKnownGeogCS( "CRS84" );

/* -------------------------------------------------------------------- */
/*      Handle auto codes.  We need to convert from format              */
/*      AUTO42001:99:8888 to format AUTO:42001,99,8888.                 */
/* -------------------------------------------------------------------- */
    else if( STARTS_WITH_CI(pszCode, "AUTO") )
    {
        char szWMSAuto[100] = { '\0' };

        if( strlen(pszCode) > sizeof(szWMSAuto)-2 )
            return OGRERR_FAILURE;

        snprintf( szWMSAuto, sizeof(szWMSAuto), "AUTO:%s", pszCode + 4 );
        for( int i = 5; szWMSAuto[i] != '\0'; i++ )
        {
            if( szWMSAuto[i] == ':' )
                szWMSAuto[i] = ',';
        }

        return importFromWMSAUTO( szWMSAuto );
    }

/* -------------------------------------------------------------------- */
/*      Not a recognise OGC item.                                       */
/* -------------------------------------------------------------------- */
    CPLError( CE_Failure, CPLE_AppDefined,
              "URN %s value not supported.",
              pszURN );

    return OGRERR_FAILURE;
}

/************************************************************************/
/*                           importFromURN()                            */
/*                                                                      */
/*      See OGC recommendation paper 06-023r1 or later for details.     */
/************************************************************************/

/**
 * \brief Initialize from OGC URN.
 *
 * Initializes this spatial reference from a coordinate system defined
 * by an OGC URN prefixed with "urn:ogc:def:crs:" per recommendation
 * paper 06-023r1.  Currently EPSG and OGC authority values are supported,
 * including OGC auto codes, but not including CRS1 or CRS88 (NAVD88).
 *
 * This method is also support through SetFromUserInput() which can
 * normally be used for URNs.
 *
 * @param pszURN the urn string.
 *
 * @return OGRERR_NONE on success or an error code.
 */

OGRErr OGRSpatialReference::importFromURN( const char *pszURN )

{
    const char *pszCur = nullptr;

    if( STARTS_WITH_CI(pszURN, "urn:ogc:def:crs:") )
        pszCur = pszURN + 16;
    else if( STARTS_WITH_CI(pszURN, "urn:ogc:def:crs,crs:") )
        pszCur = pszURN + 20;
    else if( STARTS_WITH_CI(pszURN, "urn:x-ogc:def:crs:") )
        pszCur = pszURN + 18;
    else if( STARTS_WITH_CI(pszURN, "urn:opengis:crs:") )
        pszCur = pszURN + 16;
    else if( STARTS_WITH_CI(pszURN, "urn:opengis:def:crs:") )
        pszCur = pszURN + 20;
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "URN %s not a supported format.", pszURN );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Clear any existing definition.                                  */
/* -------------------------------------------------------------------- */
    Clear();

/* -------------------------------------------------------------------- */
/*      Find code (ignoring version) out of string like:                */
/*                                                                      */
/*      authority:[version]:code                                        */
/* -------------------------------------------------------------------- */
    const char *pszAuthority = pszCur;

    // skip authority
    while( *pszCur != ':' && *pszCur )
        pszCur++;
    if( *pszCur == ':' )
        pszCur++;

    // skip version
    const char* pszBeforeVersion = pszCur;
    while( *pszCur != ':' && *pszCur )
        pszCur++;
    if( *pszCur == ':' )
        pszCur++;
    else
        // We come here in the case, the content to parse is authority:code
        // (instead of authority::code) which is probably illegal according to
        // http://www.opengeospatial.org/ogcUrnPolicy but such content is found
        // for example in what is returned by GeoServer.
        pszCur = pszBeforeVersion;

    const char *pszCode = pszCur;

    const char* pszComma = strchr(pszCur, ',');
    if( pszComma == nullptr )
        return importFromURNPart(pszAuthority, pszCode, pszURN);

    // There's a second part with the vertical SRS.
    pszCur = pszComma + 1;
    if( !STARTS_WITH(pszCur, "crs:") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "URN %s not a supported format.", pszURN );
        return OGRERR_FAILURE;
    }

    pszCur += 4;

    char* pszFirstCode = CPLStrdup(pszCode);
    pszFirstCode[pszComma - pszCode] = '\0';
    OGRErr eStatus = importFromURNPart(pszAuthority, pszFirstCode, pszURN);
    CPLFree(pszFirstCode);

    // Do we want to turn this into a compound definition
    // with a vertical datum?
    if( eStatus != OGRERR_NONE )
        return eStatus;

    /* -------------------------------------------------------------------- */
    /*      Find code (ignoring version) out of string like:                */
    /*                                                                      */
    /*      authority:[version]:code                                        */
    /* -------------------------------------------------------------------- */
    pszAuthority = pszCur;

    // skip authority
    while( *pszCur != ':' && *pszCur )
        pszCur++;
    if( *pszCur == ':' )
        pszCur++;

    // skip version
    pszBeforeVersion = pszCur;
    while( *pszCur != ':' && *pszCur )
        pszCur++;
    if( *pszCur == ':' )
        pszCur++;
    else
        pszCur = pszBeforeVersion;

    pszCode = pszCur;

    OGRSpatialReference oVertSRS;
    eStatus = oVertSRS.importFromURNPart(pszAuthority, pszCode, pszURN);
    if( eStatus == OGRERR_NONE )
    {
        OGR_SRSNode *poHorizSRS = GetRoot()->Clone();

        Clear();

        CPLString osName = poHorizSRS->GetChild(0)->GetValue();
        osName += " + ";
        osName += oVertSRS.GetRoot()->GetChild(0)->GetValue();

        SetNode( "COMPD_CS", osName );
        GetRoot()->AddChild( poHorizSRS );
        GetRoot()->AddChild( oVertSRS.GetRoot()->Clone() );
    }

    return eStatus;
}

/************************************************************************/
/*                           importFromCRSURL()                         */
/*                                                                      */
/*      See OGC Best Practice document 11-135 for details.              */
/************************************************************************/

/**
 * \brief Initialize from OGC URL.
 *
 * Initializes this spatial reference from a coordinate system defined
 * by an OGC URL prefixed with "http://opengis.net/def/crs" per best practice
 * paper 11-135.  Currently EPSG and OGC authority values are supported,
 * including OGC auto codes, but not including CRS1 or CRS88 (NAVD88).
 *
 * This method is also supported through SetFromUserInput() which can
 * normally be used for URLs.
 *
 * @param pszURL the URL string.
 *
 * @return OGRERR_NONE on success or an error code.
 */

OGRErr OGRSpatialReference::importFromCRSURL( const char *pszURL )

{
    const char *pszCur = nullptr;

    if( STARTS_WITH_CI(pszURL, "http://opengis.net/def/crs") )
        pszCur = pszURL + 26;
    else if( STARTS_WITH_CI(pszURL, "http://www.opengis.net/def/crs") )
        pszCur = pszURL + 30;
    else if( STARTS_WITH_CI(pszURL, "www.opengis.net/def/crs") )
        pszCur = pszURL + 23;
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "URL %s not a supported format.", pszURL );
        return OGRERR_FAILURE;
    }

    if( *pszCur == '\0' )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "URL %s malformed.", pszURL);
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Clear any existing definition.                                  */
/* -------------------------------------------------------------------- */
    Clear();

    if( STARTS_WITH_CI(pszCur, "-compound?1=") )
    {
/* -------------------------------------------------------------------- */
/*      It's a compound CRS, of the form:                               */
/*                                                                      */
/*      http://opengis.net/def/crs-compound?1=URL1&2=URL2&3=URL3&..     */
/* -------------------------------------------------------------------- */
        pszCur += 12;

        // Extract each component CRS URL.
        int iComponentUrl = 2;

        CPLString osName = "";
        Clear();

        while( iComponentUrl != -1 )
        {
            char searchStr[15] = {};
            snprintf(searchStr, sizeof(searchStr), "&%d=", iComponentUrl);

            const char* pszUrlEnd = strstr(pszCur, searchStr);

            // Figure out the next component URL.
            char* pszComponentUrl = nullptr;

            if( pszUrlEnd )
            {
                size_t nLen = pszUrlEnd - pszCur;
                pszComponentUrl = static_cast<char *>(CPLMalloc(nLen + 1));
                strncpy(pszComponentUrl, pszCur, nLen);
                pszComponentUrl[nLen] = '\0';

                ++iComponentUrl;
                pszCur += nLen + strlen(searchStr);
            }
            else
            {
                if( iComponentUrl == 2 )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "Compound CRS URLs must have at least two component CRSs." );
                    return OGRERR_FAILURE;
                }
                else
                {
                    pszComponentUrl = CPLStrdup(pszCur);
                    // no more components
                    iComponentUrl = -1;
                }
            }

            OGRSpatialReference oComponentSRS;
            OGRErr eStatus = oComponentSRS.importFromCRSURL( pszComponentUrl );

            CPLFree(pszComponentUrl);
            pszComponentUrl = nullptr;

            if( eStatus == OGRERR_NONE )
            {
                if( osName.length() != 0 )
                {
                  osName += " + ";
                }
                osName += oComponentSRS.GetRoot()->GetValue();
                SetNode( "COMPD_CS", osName );
                GetRoot()->AddChild( oComponentSRS.GetRoot()->Clone() );
            }
            else
                return eStatus;
        }

        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      It's a normal CRS URL, of the form:                             */
/*                                                                      */
/*      http://opengis.net/def/crs/AUTHORITY/VERSION/CODE               */
/* -------------------------------------------------------------------- */
    ++pszCur;
    const char *pszAuthority = pszCur;

    // skip authority
    while( *pszCur != '/' && *pszCur )
        pszCur++;
    if( *pszCur == '/' )
        pszCur++;

    // skip version
    while( *pszCur != '/' && *pszCur )
        pszCur++;
    if( *pszCur == '/' )
        pszCur++;

    const char *pszCode = pszCur;

    return importFromURNPart( pszAuthority, pszCode, pszURL );
}

/************************************************************************/
/*                         importFromWMSAUTO()                          */
/************************************************************************/

/**
 * \brief Initialize from WMSAUTO string.
 *
 * Note that the WMS 1.3 specification does not include the
 * units code, while apparently earlier specs do.  We try to
 * guess around this.
 *
 * @param pszDefinition the WMSAUTO string
 *
 * @return OGRERR_NONE on success or an error code.
 */
OGRErr OGRSpatialReference::importFromWMSAUTO( const char * pszDefinition )

{
    int nProjId, nUnitsId;
    double dfRefLong, dfRefLat = 0.0;

/* -------------------------------------------------------------------- */
/*      Tokenize                                                        */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszDefinition, "AUTO:") )
        pszDefinition += 5;

    char **papszTokens =
        CSLTokenizeStringComplex( pszDefinition, ",", FALSE, TRUE );

    if( CSLCount(papszTokens) == 4 )
    {
        nProjId = atoi(papszTokens[0]);
        nUnitsId = atoi(papszTokens[1]);
        dfRefLong = CPLAtof(papszTokens[2]);
        dfRefLat = CPLAtof(papszTokens[3]);
    }
    else if( CSLCount(papszTokens) == 3 && atoi(papszTokens[0]) == 42005 )
    {
        nProjId = atoi(papszTokens[0]);
        nUnitsId = atoi(papszTokens[1]);
        dfRefLong = CPLAtof(papszTokens[2]);
        dfRefLat = 0.0;
    }
    else if( CSLCount(papszTokens) == 3 )
    {
        nProjId = atoi(papszTokens[0]);
        nUnitsId = 9001;
        dfRefLong = CPLAtof(papszTokens[1]);
        dfRefLat = CPLAtof(papszTokens[2]);
    }
    else if( CSLCount(papszTokens) == 2 && atoi(papszTokens[0]) == 42005 )
    {
        nProjId = atoi(papszTokens[0]);
        nUnitsId = 9001;
        dfRefLong = CPLAtof(papszTokens[1]);
    }
    else
    {
        CSLDestroy( papszTokens );
        CPLError( CE_Failure, CPLE_AppDefined,
                  "AUTO projection has wrong number of arguments, expected\n"
                  "AUTO:proj_id,units_id,ref_long,ref_lat or"
                  "AUTO:proj_id,ref_long,ref_lat" );
        return OGRERR_FAILURE;
    }

    CSLDestroy( papszTokens );
    papszTokens = nullptr;

/* -------------------------------------------------------------------- */
/*      Build coordsys.                                                 */
/* -------------------------------------------------------------------- */
    Clear();

    switch( nProjId )
    {
      case 42001: // Auto UTM
        SetUTM( static_cast<int>(floor( (dfRefLong + 180.0) / 6.0 )) + 1,
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
        SetLinearUnits( "US survey foot", CPLAtof(SRS_UL_US_FOOT_CONV) );
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
 * \brief Get spheroid semi major axis.
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

    if( pnErr != nullptr )
        *pnErr = OGRERR_NONE;

    if( poSpheroid != nullptr && poSpheroid->GetChildCount() >= 3 )
    {
        return CPLAtof( poSpheroid->GetChild(1)->GetValue() );
    }

    if( pnErr != nullptr )
        *pnErr = OGRERR_FAILURE;

    return SRS_WGS84_SEMIMAJOR;
}

/************************************************************************/
/*                          OSRGetSemiMajor()                           */
/************************************************************************/

/**
 * \brief Get spheroid semi major axis.
 *
 * This function is the same as OGRSpatialReference::GetSemiMajor()
 */
double OSRGetSemiMajor( OGRSpatialReferenceH hSRS, OGRErr *pnErr )

{
    VALIDATE_POINTER1( hSRS, "OSRGetSemiMajor", 0 );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->GetSemiMajor( pnErr );
}

/************************************************************************/
/*                          GetInvFlattening()                          */
/************************************************************************/

/**
 * \brief Get spheroid inverse flattening.
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

    if( pnErr != nullptr )
        *pnErr = OGRERR_NONE;

    if( poSpheroid != nullptr && poSpheroid->GetChildCount() >= 3 )
    {
        return CPLAtof( poSpheroid->GetChild(2)->GetValue() );
    }

    if( pnErr != nullptr )
        *pnErr = OGRERR_FAILURE;

    return SRS_WGS84_INVFLATTENING;
}

/************************************************************************/
/*                        OSRGetInvFlattening()                         */
/************************************************************************/

/**
 * \brief Get spheroid inverse flattening.
 *
 * This function is the same as OGRSpatialReference::GetInvFlattening()
 */
double OSRGetInvFlattening( OGRSpatialReferenceH hSRS, OGRErr *pnErr )

{
    VALIDATE_POINTER1( hSRS, "OSRGetInvFlattening", 0 );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->GetInvFlattening( pnErr );
}

/************************************************************************/
/*                           GetEccentricity()                          */
/************************************************************************/

/**
 * \brief Get spheroid eccentricity
 *
 * @return eccentricity (or -1 in case of error)
 * @since GDAL 2.3
 */

double OGRSpatialReference::GetEccentricity() const

{
    OGRErr eErr = OGRERR_NONE;
    const double dfInvFlattening = GetInvFlattening(&eErr);
    if( eErr != OGRERR_NONE )
    {
        return -1.0;
    }
    if( dfInvFlattening == 0.0 )
        return 0.0;
    if( dfInvFlattening < 0.5 )
        return -1.0;
    return sqrt(2.0 / dfInvFlattening -
                    1.0 / (dfInvFlattening * dfInvFlattening));
}

/************************************************************************/
/*                      GetSquaredEccentricity()                        */
/************************************************************************/

/**
 * \brief Get spheroid squared eccentricity
 *
 * @return squared eccentricity (or -1 in case of error)
 * @since GDAL 2.3
 */

double OGRSpatialReference::GetSquaredEccentricity() const

{
    OGRErr eErr = OGRERR_NONE;
    const double dfInvFlattening = GetInvFlattening(&eErr);
    if( eErr != OGRERR_NONE )
    {
        return -1.0;
    }
    if( dfInvFlattening == 0.0 )
        return 0.0;
    if( dfInvFlattening < 0.5 )
        return -1.0;
    return 2.0 / dfInvFlattening -
                    1.0 / (dfInvFlattening * dfInvFlattening);
}

/************************************************************************/
/*                            GetSemiMinor()                            */
/************************************************************************/

/**
 * \brief Get spheroid semi minor axis.
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
    const double dfSemiMajor = GetSemiMajor( pnErr );
    const double dfInvFlattening = GetInvFlattening( pnErr );

    return OSRCalcSemiMinorFromInvFlattening(dfSemiMajor, dfInvFlattening);
}

/************************************************************************/
/*                          OSRGetSemiMinor()                           */
/************************************************************************/

/**
 * \brief Get spheroid semi minor axis.
 *
 * This function is the same as OGRSpatialReference::GetSemiMinor()
 */
double OSRGetSemiMinor( OGRSpatialReferenceH hSRS, OGRErr *pnErr )

{
    VALIDATE_POINTER1( hSRS, "OSRGetSemiMinor", 0 );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->GetSemiMinor( pnErr );
}

/************************************************************************/
/*                             SetLocalCS()                             */
/************************************************************************/

/**
 * \brief Set the user visible LOCAL_CS name.
 *
 * This method is the same as the C function OSRSetLocalCS().
 *
 * This method will ensure a LOCAL_CS node is created as the root,
 * and set the provided name on it.  It must be used before SetLinearUnits().
 *
 * @param pszName the user visible name to assign.  Not used as a key.
 *
 * @return OGRERR_NONE on success.
 */

OGRErr OGRSpatialReference::SetLocalCS( const char * pszName )

{
    const OGR_SRSNode * const poCS = GetAttrNode( "LOCAL_CS" );

    if( poCS == nullptr && GetRoot() != nullptr )
    {
        CPLDebug( "OGR",
                  "OGRSpatialReference::SetLocalCS(%s) failed.  "
                  "It appears an incompatible root node (%s) already exists.",
                  pszName, GetRoot()->GetValue() );
        return OGRERR_FAILURE;
    }

    SetNode( "LOCAL_CS", pszName );
    return OGRERR_NONE;
}

/************************************************************************/
/*                           OSRSetLocalCS()                            */
/************************************************************************/

/**
 * \brief Set the user visible LOCAL_CS name.
 *
 * This function is the same as OGRSpatialReference::SetLocalCS()
 */
OGRErr OSRSetLocalCS( OGRSpatialReferenceH hSRS, const char * pszName )

{
    VALIDATE_POINTER1( hSRS, "OSRSetLocalCS", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetLocalCS( pszName );
}

/************************************************************************/
/*                             SetGeocCS()                              */
/************************************************************************/

/**
 * \brief Set the user visible GEOCCS name.
 *
 * This method is the same as the C function OSRSetGeocCS().

 * This method will ensure a GEOCCS node is created as the root,
 * and set the provided name on it.  If used on a GEOGCS coordinate system,
 * the DATUM and PRIMEM nodes from the GEOGCS will be transferred over to
 * the GEOGCS.
 *
 * @param pszName the user visible name to assign.  Not used as a key.
 *
 * @return OGRERR_NONE on success.
 *
 * @since OGR 1.9.0
 */

OGRErr OGRSpatialReference::SetGeocCS( const char * pszName )

{
    OGR_SRSNode *poGeogCS = nullptr;
    OGR_SRSNode *poGeocCS = GetAttrNode( "GEOCCS" );

    if( poRoot != nullptr && EQUAL(poRoot->GetValue(),"GEOGCS") )
    {
        poGeogCS = poRoot;
        poRoot = nullptr;
    }

    if( poGeocCS == nullptr && GetRoot() != nullptr )
    {
        CPLDebug( "OGR",
                  "OGRSpatialReference::SetGeocCS(%s) failed.  "
                  "It appears an incompatible root node (%s) already exists.",
                  pszName, GetRoot()->GetValue() );
        return OGRERR_FAILURE;
    }

    SetNode( "GEOCCS", pszName );

    if( poGeogCS != nullptr )
    {
        OGR_SRSNode *poDatum = poGeogCS->GetNode( "DATUM" );
        OGR_SRSNode *poPRIMEM = poGeogCS->GetNode( "PRIMEM" );
        if( poRoot != nullptr && poDatum != nullptr && poPRIMEM != nullptr )
        {
            poRoot->InsertChild( poDatum->Clone(), 1 );
            poRoot->InsertChild( poPRIMEM->Clone(), 2 );
        }
        delete poGeogCS;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                            OSRSetGeocCS()                            */
/************************************************************************/

/**
 * \brief Set the user visible PROJCS name.
 *
 * This function is the same as OGRSpatialReference::SetGeocCS()
 *
 * @since OGR 1.9.0
 */
OGRErr OSRSetGeocCS( OGRSpatialReferenceH hSRS, const char * pszName )

{
    VALIDATE_POINTER1( hSRS, "OSRSetGeocCS", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetGeocCS( pszName );
}

/************************************************************************/
/*                             SetVertCS()                              */
/************************************************************************/

/**
 * \brief Set the user visible VERT_CS name.
 *
 * This method is the same as the C function OSRSetVertCS().

 * This method will ensure a VERT_CS node is created if needed.  If the
 * existing coordinate system is GEOGCS or PROJCS rooted, then it will be
 * turned into a COMPD_CS.
 *
 * @param pszVertCSName the user visible name of the vertical coordinate
 * system. Not used as a key.
 *
 * @param pszVertDatumName the user visible name of the vertical datum.  It
 * is helpful if this matches the EPSG name.
 *
 * @param nVertDatumType the OGC vertical datum type, usually 2005.
 *
 * @return OGRERR_NONE on success.
 *
 * @since OGR 1.9.0
 */

OGRErr OGRSpatialReference::SetVertCS( const char * pszVertCSName,
                                       const char * pszVertDatumName,
                                       int nVertDatumType )

{
/* -------------------------------------------------------------------- */
/*      Handle the case where we want to make a compound coordinate     */
/*      system.                                                         */
/* -------------------------------------------------------------------- */
    if( IsProjected() || IsGeographic() )
    {
        OGR_SRSNode *poNewRoot = new OGR_SRSNode( "COMPD_CS" );
        poNewRoot->AddChild( poRoot );
        poRoot = poNewRoot;
    }

    else if( GetAttrNode( "VERT_CS" ) == nullptr )
        Clear();

/* -------------------------------------------------------------------- */
/*      If we already have a VERT_CS, wipe and recreate the root        */
/*      otherwise create the VERT_CS now.                               */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poVertCS = GetAttrNode( "VERT_CS" );

    if( poVertCS != nullptr )
    {
        poVertCS->ClearChildren();
    }
    else
    {
        poVertCS = new OGR_SRSNode( "VERT_CS" );
        if( poRoot != nullptr && EQUAL(poRoot->GetValue(), "COMPD_CS") )
        {
            poRoot->AddChild( poVertCS );
        }
        else
            SetRoot( poVertCS );
    }

/* -------------------------------------------------------------------- */
/*      Set the name, datumname, and type.                              */
/* -------------------------------------------------------------------- */

    poVertCS->AddChild( new OGR_SRSNode( pszVertCSName ) );

    OGR_SRSNode *poVertDatum = new OGR_SRSNode( "VERT_DATUM" );
    poVertCS->AddChild( poVertDatum );

    poVertDatum->AddChild( new OGR_SRSNode( pszVertDatumName ) );

    CPLString osVertDatumType;
    osVertDatumType.Printf( "%d", nVertDatumType );
    poVertDatum->AddChild( new OGR_SRSNode( osVertDatumType ) );

    // Add default axis node.
    OGR_SRSNode *poAxis = new OGR_SRSNode( "AXIS" );

    poAxis->AddChild( new OGR_SRSNode( "Up" ) );
    poAxis->AddChild( new OGR_SRSNode( "UP" ) );

    poVertCS->AddChild( poAxis );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            OSRSetVertCS()                            */
/************************************************************************/

/**
 * \brief Setup the vertical coordinate system.
 *
 * This function is the same as OGRSpatialReference::SetVertCS()
 *
 * @since OGR 1.9.0
 */
OGRErr OSRSetVertCS( OGRSpatialReferenceH hSRS,
                     const char * pszVertCSName,
                     const char * pszVertDatumName,
                     int nVertDatumType )

{
    VALIDATE_POINTER1( hSRS, "OSRSetVertCS", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        SetVertCS( pszVertCSName, pszVertDatumName, nVertDatumType );
}

/************************************************************************/
/*                           SetCompoundCS()                            */
/************************************************************************/

/**
 * \brief Setup a compound coordinate system.
 *
 * This method is the same as the C function OSRSetCompoundCS().

 * This method is replace the current SRS with a COMPD_CS coordinate system
 * consisting of the passed in horizontal and vertical coordinate systems.
 *
 * @param pszName the name of the compound coordinate system.
 *
 * @param poHorizSRS the horizontal SRS (PROJCS or GEOGCS).
 *
 * @param poVertSRS the vertical SRS (VERT_CS).
 *
 * @return OGRERR_NONE on success.
 */

OGRErr
OGRSpatialReference::SetCompoundCS( const char *pszName,
                                    const OGRSpatialReference *poHorizSRS,
                                    const OGRSpatialReference *poVertSRS )

{
/* -------------------------------------------------------------------- */
/*      Verify these are legal horizontal and vertical coordinate       */
/*      systems.                                                        */
/* -------------------------------------------------------------------- */
    if( !poVertSRS->IsVertical() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "SetCompoundCS() fails, vertical component is not VERT_CS." );
        return OGRERR_FAILURE;
    }
    if( !poHorizSRS->IsProjected()
        && !poHorizSRS->IsGeographic() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "SetCompoundCS() fails, horizontal component is not PROJCS or GEOGCS." );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Replace with compound srs.                                      */
/* -------------------------------------------------------------------- */
    Clear();

    poRoot = new OGR_SRSNode( "COMPD_CS" );
    poRoot->AddChild( new OGR_SRSNode( pszName ) );
    poRoot->AddChild( poHorizSRS->GetRoot()->Clone() );
    poRoot->AddChild( poVertSRS->GetRoot()->Clone() );

    return OGRERR_NONE;
}

/************************************************************************/
/*                          OSRSetCompoundCS()                          */
/************************************************************************/

/**
 * \brief Setup a compound coordinate system.
 *
 * This function is the same as OGRSpatialReference::SetCompoundCS()
 */
OGRErr OSRSetCompoundCS( OGRSpatialReferenceH hSRS,
                         const char *pszName,
                         OGRSpatialReferenceH hHorizSRS,
                         OGRSpatialReferenceH hVertSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRSetCompoundCS", OGRERR_FAILURE );
    VALIDATE_POINTER1( hHorizSRS, "OSRSetCompoundCS", OGRERR_FAILURE );
    VALIDATE_POINTER1( hVertSRS, "OSRSetCompoundCS", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        SetCompoundCS( pszName,
                       reinterpret_cast<OGRSpatialReference *>(hHorizSRS),
                       reinterpret_cast<OGRSpatialReference *>(hVertSRS) );
}

/************************************************************************/
/*                             SetProjCS()                              */
/************************************************************************/

/**
 * \brief Set the user visible PROJCS name.
 *
 * This method is the same as the C function OSRSetProjCS().
 *
 * This method will ensure a PROJCS node is created as the root,
 * and set the provided name on it.  If used on a GEOGCS coordinate system,
 * the GEOGCS node will be demoted to be a child of the new PROJCS root.
 *
 * @param pszName the user visible name to assign.  Not used as a key.
 *
 * @return OGRERR_NONE on success.
 */

OGRErr OGRSpatialReference::SetProjCS( const char * pszName )

{
    OGR_SRSNode *poGeogCS = nullptr;
    OGR_SRSNode *poProjCS = GetAttrNode( "PROJCS" );

    if( poRoot != nullptr && EQUAL(poRoot->GetValue(), "GEOGCS") )
    {
        poGeogCS = poRoot;
        poRoot = nullptr;
    }

    if( poProjCS == nullptr && GetRoot() != nullptr )
    {
        CPLDebug( "OGR",
                  "OGRSpatialReference::SetProjCS(%s) failed.  "
                  "It appears an incompatible root node (%s) already exists.",
                  pszName, GetRoot()->GetValue() );
        return OGRERR_FAILURE;
    }

    SetNode( "PROJCS", pszName );

    if( poRoot != nullptr && poGeogCS != nullptr )
        poRoot->InsertChild( poGeogCS, 1 );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            OSRSetProjCS()                            */
/************************************************************************/

/**
 * \brief Set the user visible PROJCS name.
 *
 * This function is the same as OGRSpatialReference::SetProjCS()
 */
OGRErr OSRSetProjCS( OGRSpatialReferenceH hSRS, const char * pszName )

{
    VALIDATE_POINTER1( hSRS, "OSRSetProjCS", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetProjCS( pszName );
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

/**
 * \brief Set a projection name.
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
    OGR_SRSNode *poGeogCS = nullptr;

    if( poRoot != nullptr && EQUAL(poRoot->GetValue(), "GEOGCS") )
    {
        poGeogCS = poRoot;
        poRoot = nullptr;
    }

    if( !GetAttrNode( "PROJCS" ) )
    {
        SetNode( "PROJCS", "unnamed" );
    }

    const OGRErr eErr = SetNode( "PROJCS|PROJECTION", pszProjection );
    if( eErr != OGRERR_NONE )
        return eErr;

    if( poGeogCS != nullptr )
        poRoot->InsertChild( poGeogCS, 1 );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            OSRSetProjection()                        */
/************************************************************************/

/**
 * \brief Set a projection name.
 *
 * This function is the same as OGRSpatialReference::SetProjection()
 */
OGRErr OSRSetProjection( OGRSpatialReferenceH hSRS,
                         const char * pszProjection )

{
    VALIDATE_POINTER1( hSRS, "OSRSetProjection", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        SetProjection( pszProjection );
}

/************************************************************************/
/*                            SetProjParm()                             */
/************************************************************************/

/**
 * \brief Set a projection parameter value.
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

    if( poPROJCS == nullptr )
        return OGRERR_FAILURE;

    char szValue[64] = { '\0' };
    OGRsnPrintDouble( szValue, sizeof(szValue), dfValue );

/* -------------------------------------------------------------------- */
/*      Try to find existing parameter with this name.                  */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poParm = nullptr;
    for( int iChild = 0; iChild < poPROJCS->GetChildCount(); iChild++ )
    {
        poParm = poPROJCS->GetChild( iChild );

        if( EQUAL(poParm->GetValue(), "PARAMETER")
            && poParm->GetChildCount() == 2
            && EQUAL(poParm->GetChild(0)->GetValue(), pszParmName) )
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

/**
 * \brief Set a projection parameter value.
 *
 * This function is the same as OGRSpatialReference::SetProjParm()
 */
OGRErr OSRSetProjParm( OGRSpatialReferenceH hSRS,
                       const char * pszParmName, double dfValue )

{
    VALIDATE_POINTER1( hSRS, "OSRSetProjParm", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        SetProjParm( pszParmName, dfValue );
}

/************************************************************************/
/*                            FindProjParm()                            */
/************************************************************************/

/**
  * \brief Return the child index of the named projection parameter on
  * its parent PROJCS node.
  *
  * @param pszParameter projection parameter to look for
  * @param poPROJCS projection CS node to look in. If NULL is passed,
  *        the PROJCS node of the SpatialReference object will be searched.
  *
  * @return the child index of the named projection parameter. -1 on failure
  */
int OGRSpatialReference::FindProjParm( const char *pszParameter,
                                       const OGR_SRSNode *poPROJCS ) const

{
    if( poPROJCS == nullptr )
        poPROJCS = GetAttrNode( "PROJCS" );

    if( poPROJCS == nullptr )
        return -1;

/* -------------------------------------------------------------------- */
/*      Search for requested parameter.                                 */
/* -------------------------------------------------------------------- */
    for( int iChild = 0; iChild < poPROJCS->GetChildCount(); iChild++ )
    {
        const OGR_SRSNode *poParameter = poPROJCS->GetChild(iChild);

        if( EQUAL(poParameter->GetValue(), "PARAMETER")
            && poParameter->GetChildCount() == 2
            && EQUAL(poPROJCS->GetChild(iChild)->GetChild(0)->GetValue(),
                     pszParameter) )
        {
            return iChild;
        }
    }

/* -------------------------------------------------------------------- */
/*      Try similar names, for selected parameters.                     */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszParameter, SRS_PP_LATITUDE_OF_ORIGIN) )
    {
        return FindProjParm( SRS_PP_LATITUDE_OF_CENTER, poPROJCS );
    }

    if( EQUAL(pszParameter, SRS_PP_CENTRAL_MERIDIAN) )
    {
        int iChild = FindProjParm(SRS_PP_LONGITUDE_OF_CENTER, poPROJCS );
        if( iChild == -1 )
            iChild = FindProjParm(SRS_PP_LONGITUDE_OF_ORIGIN, poPROJCS );
        return iChild;
    }

    return -1;
}

/************************************************************************/
/*                            GetProjParm()                             */
/************************************************************************/

/**
 * \brief Fetch a projection parameter value.
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
    if( pnErr != nullptr )
        *pnErr = OGRERR_NONE;

/* -------------------------------------------------------------------- */
/*      Find the desired parameter.                                     */
/* -------------------------------------------------------------------- */
    const OGR_SRSNode *poPROJCS = GetAttrNode( "PROJCS" );
    if( poPROJCS == nullptr )
    {
        if( pnErr != nullptr )
            *pnErr = OGRERR_FAILURE;
        return dfDefaultValue;
    }

    const int iChild = FindProjParm( pszName, poPROJCS );
    if( iChild == -1 )
    {
        if( pnErr != nullptr )
            *pnErr = OGRERR_FAILURE;
        return dfDefaultValue;
    }

    const OGR_SRSNode *poParameter = poPROJCS->GetChild(iChild);
    return CPLAtof(poParameter->GetChild(1)->GetValue());
}

/************************************************************************/
/*                           OSRGetProjParm()                           */
/************************************************************************/

/**
 * \brief Fetch a projection parameter value.
 *
 * This function is the same as OGRSpatialReference::GetProjParm()
 */
double OSRGetProjParm( OGRSpatialReferenceH hSRS, const char *pszName,
                       double dfDefaultValue, OGRErr *pnErr )

{
    VALIDATE_POINTER1( hSRS, "OSRGetProjParm", 0 );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        GetProjParm(pszName, dfDefaultValue, pnErr);
}

/************************************************************************/
/*                          GetNormProjParm()                           */
/************************************************************************/

/**
 * \brief Fetch a normalized projection parameter value.
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
    GetNormInfo();

    OGRErr nError = OGRERR_NONE;
    double dfRawResult = GetProjParm( pszName, dfDefaultValue, &nError );
    if( pnErr != nullptr )
        *pnErr = nError;

    // If we got the default just return it unadjusted.
    if( nError != OGRERR_NONE )
        return dfRawResult;

    if( dfToDegrees != 1.0 && IsAngularParameter(pszName) )
        dfRawResult *= dfToDegrees;

    if( dfToMeter != 1.0 && IsLinearParameter( pszName ) )
        return dfRawResult * dfToMeter;
#ifdef WKT_LONGITUDE_RELATIVE_TO_PM
    else if( dfFromGreenwich != 0.0 && IsLongitudeParameter( pszName ) )
        return dfRawResult + dfFromGreenwich;
#endif

    return dfRawResult;
}

/************************************************************************/
/*                         OSRGetNormProjParm()                         */
/************************************************************************/

/**
 * \brief This function is the same as OGRSpatialReference::
 *
 * This function is the same as OGRSpatialReference::GetNormProjParm()
 */
double OSRGetNormProjParm( OGRSpatialReferenceH hSRS, const char *pszName,
                           double dfDefaultValue, OGRErr *pnErr )

{
    VALIDATE_POINTER1( hSRS, "OSRGetNormProjParm", 0 );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        GetNormProjParm(pszName, dfDefaultValue, pnErr);
}

/************************************************************************/
/*                          SetNormProjParm()                           */
/************************************************************************/

/**
 * \brief Set a projection parameter with a normalized value.
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

/**
 * \brief Set a projection parameter with a normalized value.
 *
 * This function is the same as OGRSpatialReference::SetNormProjParm()
 */
OGRErr OSRSetNormProjParm( OGRSpatialReferenceH hSRS,
                           const char * pszParmName, double dfValue )

{
    VALIDATE_POINTER1( hSRS, "OSRSetNormProjParm", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
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
    VALIDATE_POINTER1( hSRS, "OSRSetTM", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetTM(
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
    VALIDATE_POINTER1( hSRS, "OSRSetTMVariant", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetTMVariant(
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
/*                             OSRSetTPED()                             */
/************************************************************************/

OGRErr OSRSetTPED( OGRSpatialReferenceH hSRS,
                   double dfLat1, double dfLong1,
                   double dfLat2, double dfLong2,
                   double dfFalseEasting, double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetTPED", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetTPED(
        dfLat1, dfLong1, dfLat2, dfLong2,
        dfFalseEasting, dfFalseNorthing );
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
    VALIDATE_POINTER1( hSRS, "OSRSetTMSO", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetTMSO(
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
    VALIDATE_POINTER1( hSRS, "OSRSetTMG", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetTMG(
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
    VALIDATE_POINTER1( hSRS, "OSRSetACEA", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetACEA(
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
    VALIDATE_POINTER1( hSRS, "OSRSetACEA", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetAE(
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
                    double dfStdP1, double dfCentralMeridian,
                    double dfFalseEasting, double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetBonne", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetBonne(
        dfStdP1, dfCentralMeridian,
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
    VALIDATE_POINTER1( hSRS, "OSRSetCEA", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetCEA(
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
    VALIDATE_POINTER1( hSRS, "OSRSetCS", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetCS(
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
    VALIDATE_POINTER1( hSRS, "OSRSetEC", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetEC(
        dfStdP1, dfStdP2,
        dfCenterLat, dfCenterLong,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                             SetEckert()                              */
/************************************************************************/

OGRErr OGRSpatialReference::SetEckert( int nVariation,  // 1-6.
                                       double dfCentralMeridian,
                                       double dfFalseEasting,
                                       double dfFalseNorthing )

{
    if( nVariation == 1 )
        SetProjection( SRS_PT_ECKERT_I );
    else if( nVariation == 2 )
        SetProjection( SRS_PT_ECKERT_II );
    else if( nVariation == 3 )
        SetProjection( SRS_PT_ECKERT_III );
    else if( nVariation == 4 )
        SetProjection( SRS_PT_ECKERT_IV );
    else if( nVariation == 5 )
        SetProjection( SRS_PT_ECKERT_V );
    else if( nVariation == 6 )
        SetProjection( SRS_PT_ECKERT_VI );
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unsupported Eckert variation (%d).",
                  nVariation );
        return OGRERR_UNSUPPORTED_SRS;
    }

    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCentralMeridian );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            OSRSetEckert()                            */
/************************************************************************/

OGRErr OSRSetEckert( OGRSpatialReferenceH hSRS,
                     int nVariation,
                     double dfCentralMeridian,
                     double dfFalseEasting,
                     double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetEckert", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetEckert(
        nVariation, dfCentralMeridian,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                            SetEckertIV()                             */
/*                                                                      */
/*      Deprecated                                                      */
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
    VALIDATE_POINTER1( hSRS, "OSRSetEckertIV", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetEckertIV(
        dfCentralMeridian,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                            SetEckertVI()                             */
/*                                                                      */
/*      Deprecated                                                      */
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
    VALIDATE_POINTER1( hSRS, "OSRSetEckertVI", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetEckertVI(
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
    VALIDATE_POINTER1( hSRS, "OSRSetEquirectangular", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetEquirectangular(
        dfCenterLat, dfCenterLong,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                         SetEquirectangular2()                        */
/* Generalized form                                                     */
/************************************************************************/

OGRErr OGRSpatialReference::SetEquirectangular2(
                                   double dfCenterLat, double dfCenterLong,
                                   double dfStdParallel1,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    SetProjection( SRS_PT_EQUIRECTANGULAR );
    SetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, dfStdParallel1 );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                       OSRSetEquirectangular2()                       */
/************************************************************************/

OGRErr OSRSetEquirectangular2( OGRSpatialReferenceH hSRS,
                               double dfCenterLat, double dfCenterLong,
                               double dfStdParallel1,
                               double dfFalseEasting,
                               double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetEquirectangular2", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetEquirectangular2(
        dfCenterLat, dfCenterLong,
        dfStdParallel1,
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
    VALIDATE_POINTER1( hSRS, "OSRSetGS", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetGS(
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
    VALIDATE_POINTER1( hSRS, "OSRSetGH", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetGH(
        dfCentralMeridian,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                              SetIGH()                                */
/************************************************************************/

OGRErr OGRSpatialReference::SetIGH()

{
    SetProjection( SRS_PT_IGH );

    return OGRERR_NONE;
}

/************************************************************************/
/*                              OSRSetIGH()                             */
/************************************************************************/

OGRErr OSRSetIGH( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRSetIGH", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetIGH();
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
    VALIDATE_POINTER1( hSRS, "OSRSetGEOS", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetGEOS(
        dfCentralMeridian, dfSatelliteHeight,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                       SetGaussSchreiberTMercator()                   */
/************************************************************************/

OGRErr OGRSpatialReference::SetGaussSchreiberTMercator(
                                   double dfCenterLat, double dfCenterLong,
                                   double dfScale,
                                   double dfFalseEasting,
                                   double dfFalseNorthing )

{
    SetProjection( SRS_PT_GAUSSSCHREIBERTMERCATOR );
    SetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetNormProjParm( SRS_PP_SCALE_FACTOR, dfScale );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                     OSRSetGaussSchreiberTMercator()                  */
/************************************************************************/

OGRErr OSRSetGaussSchreiberTMercator( OGRSpatialReferenceH hSRS,
                                      double dfCenterLat, double dfCenterLong,
                                      double dfScale,
                                      double dfFalseEasting,
                                      double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetGaussSchreiberTMercator", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetGaussSchreiberTMercator(
        dfCenterLat, dfCenterLong, dfScale,
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
    VALIDATE_POINTER1( hSRS, "OSRSetGnomonic", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetGnomonic(
        dfCenterLat, dfCenterLong,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                              SetHOMAC()                              */
/************************************************************************/

/**
 * \brief Set an Hotine Oblique Mercator Azimuth Center projection using
 * azimuth angle.
 *
 * This projection corresponds to EPSG projection method 9815, also
 * sometimes known as hotine oblique mercator (variant B).
 *
 * This method does the same thing as the C function OSRSetHOMAC().
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

OGRErr OGRSpatialReference::SetHOMAC( double dfCenterLat, double dfCenterLong,
                                      double dfAzimuth, double dfRectToSkew,
                                      double dfScale,
                                      double dfFalseEasting,
                                      double dfFalseNorthing )

{
    SetProjection( SRS_PT_HOTINE_OBLIQUE_MERCATOR_AZIMUTH_CENTER );
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
/*                            OSRSetHOMAC()                             */
/************************************************************************/

/**
 * \brief Set an Oblique Mercator projection using azimuth angle.
 *
 * This is the same as the C++ method OGRSpatialReference::SetHOMAC()
 */
OGRErr OSRSetHOMAC( OGRSpatialReferenceH hSRS,
                    double dfCenterLat, double dfCenterLong,
                    double dfAzimuth, double dfRectToSkew,
                    double dfScale,
                    double dfFalseEasting,
                    double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetHOMAC", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetHOMAC(
        dfCenterLat, dfCenterLong,
        dfAzimuth, dfRectToSkew,
        dfScale,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                               SetHOM()                               */
/************************************************************************/

/**
 * \brief Set a Hotine Oblique Mercator projection using azimuth angle.
 *
 * This projection corresponds to EPSG projection method 9812, also
 * sometimes known as hotine oblique mercator (variant A)..
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
/**
 * \brief Set a Hotine Oblique Mercator projection using azimuth angle.
 *
 * This is the same as the C++ method OGRSpatialReference::SetHOM()
 */
OGRErr OSRSetHOM( OGRSpatialReferenceH hSRS,
                  double dfCenterLat, double dfCenterLong,
                  double dfAzimuth, double dfRectToSkew,
                  double dfScale,
                  double dfFalseEasting,
                  double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetHOM", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetHOM(
        dfCenterLat, dfCenterLong,
        dfAzimuth, dfRectToSkew,
        dfScale,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                             SetHOM2PNO()                             */
/************************************************************************/

/**
 * \brief Set a Hotine Oblique Mercator projection using two points on projection
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
/**
 * \brief Set a Hotine Oblique Mercator projection using two points on
 *  projection centerline.
 *
 * This is the same as the C++ method OGRSpatialReference::SetHOM2PNO()
 */
OGRErr OSRSetHOM2PNO( OGRSpatialReferenceH hSRS,
                      double dfCenterLat,
                      double dfLat1, double dfLong1,
                      double dfLat2, double dfLong2,
                      double dfScale,
                      double dfFalseEasting, double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetHOM2PNO", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetHOM2PNO(
        dfCenterLat,
        dfLat1, dfLong1,
        dfLat2, dfLong2,
        dfScale,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                            SetIWMPolyconic()                         */
/************************************************************************/

OGRErr OGRSpatialReference::SetIWMPolyconic(
                                double dfLat1, double dfLat2,
                                double dfCenterLong,
                                double dfFalseEasting, double dfFalseNorthing )

{
    SetProjection( SRS_PT_IMW_POLYCONIC );
    SetNormProjParm( SRS_PP_LATITUDE_OF_1ST_POINT, dfLat1 );
    SetNormProjParm( SRS_PP_LATITUDE_OF_2ND_POINT, dfLat2 );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                          OSRSetIWMPolyconic()                        */
/************************************************************************/

OGRErr OSRSetIWMPolyconic( OGRSpatialReferenceH hSRS,
                           double dfLat1, double dfLat2,
                           double dfCenterLong,
                           double dfFalseEasting, double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetIWMPolyconic", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetIWMPolyconic(
        dfLat1, dfLat2, dfCenterLong,
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
    VALIDATE_POINTER1( hSRS, "OSRSetKrovak", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetKrovak(
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
    VALIDATE_POINTER1( hSRS, "OSRSetLAEA", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetLAEA(
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
    VALIDATE_POINTER1( hSRS, "OSRSetLCC", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetLCC(
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
    VALIDATE_POINTER1( hSRS, "OSRSetLCC1SP", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetLCC1SP(
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
    VALIDATE_POINTER1( hSRS, "OSRSetLCCB", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetLCCB(
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
    VALIDATE_POINTER1( hSRS, "OSRSetMC", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetMC(
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

    if( dfCenterLat != 0.0 )
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
    VALIDATE_POINTER1( hSRS, "OSRSetMercator", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetMercator(
        dfCenterLat, dfCenterLong,
        dfScale,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                           SetMercator2SP()                           */
/************************************************************************/

OGRErr OGRSpatialReference::SetMercator2SP(
    double dfStdP1,
    double dfCenterLat, double dfCenterLong,
    double dfFalseEasting,
    double dfFalseNorthing )

{
    SetProjection( SRS_PT_MERCATOR_2SP );

    SetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, dfStdP1 );
    if( dfCenterLat != 0.0 )
        SetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );

    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );
    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                         OSRSetMercator2SP()                          */
/************************************************************************/

OGRErr OSRSetMercator2SP( OGRSpatialReferenceH hSRS,
                          double dfStdP1,
                          double dfCenterLat, double dfCenterLong,
                          double dfFalseEasting, double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetMercator2SP", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetMercator2SP(
        dfStdP1,
        dfCenterLat, dfCenterLong,
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
    VALIDATE_POINTER1( hSRS, "OSRSetMollweide", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetMollweide(
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
    VALIDATE_POINTER1( hSRS, "OSRSetNZMG", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetNZMG(
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
    VALIDATE_POINTER1( hSRS, "OSRSetOS", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetOS(
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
    VALIDATE_POINTER1( hSRS, "OSRSetOrthographic", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetOrthographic(
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
    VALIDATE_POINTER1( hSRS, "OSRSetPolyconic", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetPolyconic(
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
    VALIDATE_POINTER1( hSRS, "OSRSetPS", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetPS(
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
    VALIDATE_POINTER1( hSRS, "OSRSetRobinson", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetRobinson(
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
    VALIDATE_POINTER1( hSRS, "OSRSetSinusoidal", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetSinusoidal(
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
    VALIDATE_POINTER1( hSRS, "OSRSetStereographic", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetStereographic(
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
    VALIDATE_POINTER1( hSRS, "OSRSetSOC", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetSOC(
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
    VALIDATE_POINTER1( hSRS, "OSRSetVDG", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetVDG(
        dfCentralMeridian,
        dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                               SetUTM()                               */
/************************************************************************/

/**
 * \brief Set UTM projection definition.
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
    if( nZone < 0 || nZone > 60 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid zone: %d", nZone);
        return OGRERR_FAILURE;
    }
    SetProjection( SRS_PT_TRANSVERSE_MERCATOR );
    SetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0 );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, nZone * 6 - 183 );
    SetNormProjParm( SRS_PP_SCALE_FACTOR, 0.9996 );
    SetNormProjParm( SRS_PP_FALSE_EASTING, 500000.0 );

    if( bNorth )
        SetNormProjParm( SRS_PP_FALSE_NORTHING, 0 );
    else
        SetNormProjParm( SRS_PP_FALSE_NORTHING, 10000000 );

    if( EQUAL(GetAttrValue("PROJCS"), "unnamed") )
    {
        char szUTMName[128] = {};

        if( bNorth )
            snprintf(szUTMName, sizeof(szUTMName),
                     "UTM Zone %d, Northern Hemisphere", nZone);
        else
            snprintf(szUTMName, sizeof(szUTMName),
                     "UTM Zone %d, Southern Hemisphere", nZone);

        SetNode( "PROJCS", szUTMName );
    }

    SetLinearUnits( SRS_UL_METER, 1.0 );

    return OGRERR_NONE;
}

/************************************************************************/
/*                             OSRSetUTM()                              */
/************************************************************************/

/**
 * \brief Set UTM projection definition.
 *
 * This is the same as the C++ method OGRSpatialReference::SetUTM()
 */
OGRErr OSRSetUTM( OGRSpatialReferenceH hSRS, int nZone, int bNorth )

{
    VALIDATE_POINTER1( hSRS, "OSRSetUTM", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetUTM( nZone, bNorth );
}

/************************************************************************/
/*                             GetUTMZone()                             */
/*                                                                      */
/*      Returns zero if it isn't UTM.                                   */
/************************************************************************/

/**
 * \brief Get utm zone information.
 *
 * This is the same as the C function OSRGetUTMZone().
 *
 * In SWIG bindings (Python, Java, etc) the GetUTMZone() method returns a
 * zone which is negative in the southern hemisphere instead of having the
 * pbNorth flag used in the C and C++ interface.
 *
 * @param pbNorth pointer to in to set to TRUE if northern hemisphere, or
 * FALSE if southern.
 *
 * @return UTM zone number or zero if this isn't a UTM definition.
 */

int OGRSpatialReference::GetUTMZone( int * pbNorth ) const

{
    const char *pszProjection = GetAttrValue( "PROJECTION" );

    if( pszProjection == nullptr
        || !EQUAL(pszProjection, SRS_PT_TRANSVERSE_MERCATOR) )
        return 0;

    if( GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 ) != 0.0 )
        return 0;

    if( GetProjParm( SRS_PP_SCALE_FACTOR, 1.0 ) != 0.9996 )
        return 0;

    if( fabs(GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 )-500000.0) > 0.001 )
        return 0;

    const double dfFalseNorthing = GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0);

    if( dfFalseNorthing != 0.0
        && fabs(dfFalseNorthing-10000000.0) > 0.001 )
        return 0;

    if( pbNorth != nullptr )
        *pbNorth = (dfFalseNorthing == 0);

    const double dfCentralMeridian =
        GetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, 0.0);
    const double dfZone = (dfCentralMeridian + 186.0) / 6.0;

    if( dfCentralMeridian < -177.00001 ||
        dfCentralMeridian > 177.000001 ||
        CPLIsNan(dfZone) ||
        std::abs(dfZone - static_cast<int>(dfZone) - 0.5 ) > 0.00001 )
      return 0;

    return static_cast<int>(dfZone);
}

/************************************************************************/
/*                           OSRGetUTMZone()                            */
/************************************************************************/

/**
 * \brief Get utm zone information.
 *
 * This is the same as the C++ method OGRSpatialReference::GetUTMZone()
 */
int OSRGetUTMZone( OGRSpatialReferenceH hSRS, int *pbNorth )

{
    VALIDATE_POINTER1( hSRS, "OSRGetUTMZone", 0 );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->GetUTMZone( pbNorth );
}

/************************************************************************/
/*                             SetWagner()                              */
/************************************************************************/

OGRErr OGRSpatialReference::SetWagner( int nVariation,  // 1--7.
                                       double dfCenterLat,
                                       double dfFalseEasting,
                                       double dfFalseNorthing )

{
    if( nVariation == 1 )
        SetProjection( SRS_PT_WAGNER_I );
    else if( nVariation == 2 )
        SetProjection( SRS_PT_WAGNER_II );
    else if( nVariation == 3 )
    {
        SetProjection( SRS_PT_WAGNER_III );
        SetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    }
    else if( nVariation == 4 )
        SetProjection( SRS_PT_WAGNER_IV );
    else if( nVariation == 5 )
        SetProjection( SRS_PT_WAGNER_V );
    else if( nVariation == 6 )
        SetProjection( SRS_PT_WAGNER_VI );
    else if( nVariation == 7 )
        SetProjection( SRS_PT_WAGNER_VII );
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unsupported Wagner variation (%d).", nVariation );
        return OGRERR_UNSUPPORTED_SRS;
    }

    SetNormProjParm( SRS_PP_FALSE_EASTING, dfFalseEasting );
    SetNormProjParm( SRS_PP_FALSE_NORTHING, dfFalseNorthing );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            OSRSetWagner()                            */
/************************************************************************/

OGRErr OSRSetWagner( OGRSpatialReferenceH hSRS,
                     int nVariation, double dfCenterLat,
                     double dfFalseEasting,
                     double dfFalseNorthing )

{
    VALIDATE_POINTER1( hSRS, "OSRSetWagner", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetWagner(
        nVariation, dfCenterLat, dfFalseEasting, dfFalseNorthing );
}

/************************************************************************/
/*                            SetQSC()                     */
/************************************************************************/

OGRErr OGRSpatialReference::SetQSC( double dfCenterLat, double dfCenterLong )

{
    SetProjection( SRS_PT_QSC );
    SetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, dfCenterLat );
    SetNormProjParm( SRS_PP_CENTRAL_MERIDIAN, dfCenterLong );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           OSRSetQSC()                   */
/************************************************************************/

OGRErr OSRSetQSC( OGRSpatialReferenceH hSRS,
                       double dfCenterLat, double dfCenterLong )

{
    VALIDATE_POINTER1( hSRS, "OSRSetQSC", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetQSC(
        dfCenterLat, dfCenterLong );
}

/************************************************************************/
/*                            SetSCH()                     */
/************************************************************************/

OGRErr OGRSpatialReference::SetSCH( double dfPegLat, double dfPegLong,
                                    double dfPegHeading, double dfPegHgt)

{
    SetProjection( SRS_PT_SCH );
    SetNormProjParm( SRS_PP_PEG_POINT_LATITUDE, dfPegLat );
    SetNormProjParm( SRS_PP_PEG_POINT_LONGITUDE, dfPegLong );
    SetNormProjParm( SRS_PP_PEG_POINT_HEADING, dfPegHeading );
    SetNormProjParm( SRS_PP_PEG_POINT_HEIGHT, dfPegHgt);

    return OGRERR_NONE;
}

/************************************************************************/
/*                           OSRSetSCH()                   */
/************************************************************************/

OGRErr OSRSetSCH( OGRSpatialReferenceH hSRS,
                       double dfPegLat, double dfPegLong,
                       double dfPegHeading, double dfPegHgt)

{
    VALIDATE_POINTER1( hSRS, "OSRSetSCH", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetSCH(
        dfPegLat, dfPegLong, dfPegHeading, dfPegHgt );
}

/************************************************************************/
/*                            SetAuthority()                            */
/************************************************************************/

/**
 * \brief Set the authority for a node.
 *
 * This method is the same as the C function OSRSetAuthority().
 *
 * @param pszTargetKey the partial or complete path to the node to
 * set an authority on.  i.e. "PROJCS", "GEOGCS" or "GEOGCS|UNIT".
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
    OGR_SRSNode *poNode = GetAttrNode( pszTargetKey );

    if( poNode == nullptr )
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
    char szCode[32] = {};

    snprintf( szCode, sizeof(szCode), "%d", nCode );

    OGR_SRSNode *poAuthNode = new OGR_SRSNode( "AUTHORITY" );
    poAuthNode->AddChild( new OGR_SRSNode( pszAuthority ) );
    poAuthNode->AddChild( new OGR_SRSNode( szCode ) );

    poNode->AddChild( poAuthNode );

    return OGRERR_NONE;
}

/************************************************************************/
/*                          OSRSetAuthority()                           */
/************************************************************************/

/**
 * \brief Set the authority for a node.
 *
 * This function is the same as OGRSpatialReference::SetAuthority().
 */
OGRErr OSRSetAuthority( OGRSpatialReferenceH hSRS,
                        const char *pszTargetKey,
                        const char * pszAuthority,
                        int nCode )

{
    VALIDATE_POINTER1( hSRS, "OSRSetAuthority", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetAuthority( pszTargetKey,
                                                         pszAuthority,
                                                         nCode );
}

/************************************************************************/
/*                          GetAuthorityCode()                          */
/************************************************************************/

/**
 * \brief Get the authority code for a node.
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
 * get an authority from.  i.e. "PROJCS", "GEOGCS", "GEOGCS|UNIT" or NULL to
 * search for an authority node on the root element.
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
    const OGR_SRSNode *poNode = pszTargetKey == nullptr
        ? poRoot
        : GetAttrNode( pszTargetKey );

    if( poNode == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Fetch AUTHORITY child if there is one.                          */
/* -------------------------------------------------------------------- */
    if( poNode->FindChild("AUTHORITY") == -1 )
        return nullptr;

    poNode = poNode->GetChild(poNode->FindChild("AUTHORITY"));

/* -------------------------------------------------------------------- */
/*      Create a new authority node.                                    */
/* -------------------------------------------------------------------- */
    if( poNode->GetChildCount() < 2 )
        return nullptr;

    return poNode->GetChild(1)->GetValue();
}

/************************************************************************/
/*                          OSRGetAuthorityCode()                       */
/************************************************************************/

/**
 * \brief Get the authority code for a node.
 *
 * This function is the same as OGRSpatialReference::GetAuthorityCode().
 */
const char *OSRGetAuthorityCode( OGRSpatialReferenceH hSRS,
                                 const char *pszTargetKey )

{
    VALIDATE_POINTER1( hSRS, "OSRGetAuthorityCode", nullptr );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        GetAuthorityCode( pszTargetKey );
}

/************************************************************************/
/*                          GetAuthorityName()                          */
/************************************************************************/

/**
 * \brief Get the authority name for a node.
 *
 * This method is used to query an AUTHORITY[] node from within the
 * WKT tree, and fetch the authority name value.
 *
 * The most common authority is "EPSG".
 *
 * This method is the same as the C function OSRGetAuthorityName().
 *
 * @param pszTargetKey the partial or complete path to the node to
 * get an authority from.  i.e. "PROJCS", "GEOGCS", "GEOGCS|UNIT" or NULL to
 * search for an authority node on the root element.
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
    const OGR_SRSNode *poNode = nullptr;

    if( pszTargetKey == nullptr )
        poNode = poRoot;
    else
        poNode = const_cast<OGRSpatialReference *>(this)->
            GetAttrNode( pszTargetKey );

    if( poNode == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Fetch AUTHORITY child if there is one.                          */
/* -------------------------------------------------------------------- */
    if( poNode->FindChild("AUTHORITY") == -1 )
        return nullptr;

    poNode = poNode->GetChild(poNode->FindChild("AUTHORITY"));

/* -------------------------------------------------------------------- */
/*      Create a new authority node.                                    */
/* -------------------------------------------------------------------- */
    if( poNode->GetChildCount() < 2 )
        return nullptr;

    return poNode->GetChild(0)->GetValue();
}

/************************************************************************/
/*                        OSRGetAuthorityName()                         */
/************************************************************************/

/**
 * \brief Get the authority name for a node.
 *
 * This function is the same as OGRSpatialReference::GetAuthorityName().
 */
const char *OSRGetAuthorityName( OGRSpatialReferenceH hSRS,
                                 const char *pszTargetKey )

{
    VALIDATE_POINTER1( hSRS, "OSRGetAuthorityName", nullptr );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        GetAuthorityName( pszTargetKey );
}

/************************************************************************/
/*                           StripVertical()                            */
/************************************************************************/

/**
 * \brief Convert a compound cs into a horizontal CS.
 *
 * If this SRS is of type COMPD_CS[] then the vertical CS and the root COMPD_CS
 * nodes are stripped resulting and only the horizontal coordinate system
 * portion remains (normally PROJCS, GEOGCS or LOCAL_CS).
 *
 * If this is not a compound coordinate system then nothing is changed.
 *
 * @since OGR 1.8.0
 */

OGRErr OGRSpatialReference::StripVertical()

{
    if( GetRoot() == nullptr
        || !EQUAL(GetRoot()->GetValue(), "COMPD_CS") )
        return OGRERR_NONE;

    OGR_SRSNode *poHorizontalCS = GetRoot()->GetChild( 1 );
    if( poHorizontalCS != nullptr )
        poHorizontalCS = poHorizontalCS->Clone();
    SetRoot( poHorizontalCS );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            StripCTParms()                            */
/************************************************************************/

/**
 * \brief Strip OGC CT Parameters.
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
    if( poCurrent == nullptr )
    {
        StripVertical();
        poCurrent = GetRoot();
    }

    if( poCurrent == nullptr )
        return OGRERR_NONE;

    if( poCurrent == GetRoot() && EQUAL(poCurrent->GetValue(), "LOCAL_CS") )
    {
        delete poCurrent;
        poRoot = nullptr;

        return OGRERR_NONE;
    }

    poCurrent->StripNodes( "AUTHORITY" );
    poCurrent->StripNodes( "TOWGS84" );
    poCurrent->StripNodes( "AXIS" );
    poCurrent->StripNodes( "EXTENSION" );

    return OGRERR_NONE;
}

/************************************************************************/
/*                          OSRStripCTParms()                           */
/************************************************************************/

/**
 * \brief Strip OGC CT Parameters.
 *
 * This function is the same as OGRSpatialReference::StripCTParms().
 */
OGRErr OSRStripCTParms( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRStripCTParms", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->StripCTParms( nullptr );
}

/************************************************************************/
/*                             IsCompound()                             */
/************************************************************************/

/**
 * \brief Check if coordinate system is compound.
 *
 * This method is the same as the C function OSRIsCompound().
 *
 * @return TRUE if this is rooted with a COMPD_CS node.
 */

int OGRSpatialReference::IsCompound() const

{
    if( poRoot == nullptr )
        return FALSE;

    return EQUAL(poRoot->GetValue(), "COMPD_CS");
}

/************************************************************************/
/*                           OSRIsCompound()                            */
/************************************************************************/

/**
 * \brief Check if the coordinate system is compound.
 *
 * This function is the same as OGRSpatialReference::IsCompound().
 */
int OSRIsCompound( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRIsCompound", 0 );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->IsCompound();
}

/************************************************************************/
/*                            IsProjected()                             */
/************************************************************************/

/**
 * \brief Check if projected coordinate system.
 *
 * This method is the same as the C function OSRIsProjected().
 *
 * @return TRUE if this contains a PROJCS node indicating a it is a
 * projected coordinate system.
 */

int OGRSpatialReference::IsProjected() const

{
    if( poRoot == nullptr )
        return FALSE;


    if( EQUAL(poRoot->GetValue(), "PROJCS") )
        return TRUE;
    else if( EQUAL(poRoot->GetValue(), "COMPD_CS") )
        return GetAttrNode( "PROJCS" ) != nullptr;
    else
        return FALSE;
}

/************************************************************************/
/*                           OSRIsProjected()                           */
/************************************************************************/
/**
 * \brief Check if projected coordinate system.
 *
 * This function is the same as OGRSpatialReference::IsProjected().
 */
int OSRIsProjected( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRIsProjected", 0 );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->IsProjected();
}

/************************************************************************/
/*                            IsGeocentric()                            */
/************************************************************************/

/**
 * \brief Check if geocentric coordinate system.
 *
 * This method is the same as the C function OSRIsGeocentric().
 *
 * @return TRUE if this contains a GEOCCS node indicating a it is a
 * geocentric coordinate system.
 *
 * @since OGR 1.9.0
 */

int OGRSpatialReference::IsGeocentric() const

{
    if( poRoot == nullptr )
        return FALSE;

    if( EQUAL(poRoot->GetValue(), "GEOCCS") )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                           OSRIsGeocentric()                          */
/************************************************************************/
/**
 * \brief Check if geocentric coordinate system.
 *
 * This function is the same as OGRSpatialReference::IsGeocentric().
 *
 * @since OGR 1.9.0
 */
int OSRIsGeocentric( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRIsGeocentric", 0 );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->IsGeocentric();
}

/************************************************************************/
/*                            IsGeographic()                            */
/************************************************************************/

/**
 * \brief Check if geographic coordinate system.
 *
 * This method is the same as the C function OSRIsGeographic().
 *
 * @return TRUE if this spatial reference is geographic ... that is the
 * root is a GEOGCS node.
 */

int OGRSpatialReference::IsGeographic() const

{
    if( GetRoot() == nullptr )
        return FALSE;

    if( EQUAL(poRoot->GetValue(), "GEOGCS") )
        return TRUE;

    if( EQUAL(poRoot->GetValue(), "COMPD_CS") )
        return GetAttrNode( "GEOGCS" ) != nullptr
            && GetAttrNode( "PROJCS" ) == nullptr;

    return FALSE;
}

/************************************************************************/
/*                          OSRIsGeographic()                           */
/************************************************************************/
/**
 * \brief Check if geographic coordinate system.
 *
 * This function is the same as OGRSpatialReference::IsGeographic().
 */
int OSRIsGeographic( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRIsGeographic", 0 );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->IsGeographic();
}

/************************************************************************/
/*                              IsLocal()                               */
/************************************************************************/

/**
 * \brief Check if local coordinate system.
 *
 * This method is the same as the C function OSRIsLocal().
 *
 * @return TRUE if this spatial reference is local ... that is the
 * root is a LOCAL_CS node.
 */

int OGRSpatialReference::IsLocal() const

{
    if( GetRoot() == nullptr )
        return FALSE;

    return EQUAL(GetRoot()->GetValue(), "LOCAL_CS");
}

/************************************************************************/
/*                          OSRIsLocal()                                */
/************************************************************************/
/**
 * \brief Check if local coordinate system.
 *
 * This function is the same as OGRSpatialReference::IsLocal().
 */
int OSRIsLocal( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRIsLocal", 0 );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->IsLocal();
}

/************************************************************************/
/*                            IsVertical()                              */
/************************************************************************/

/**
 * \brief Check if vertical coordinate system.
 *
 * This method is the same as the C function OSRIsVertical().
 *
 * @return TRUE if this contains a VERT_CS node indicating a it is a
 * vertical coordinate system.
 *
 * @since OGR 1.8.0
 */

int OGRSpatialReference::IsVertical() const

{
    if( poRoot == nullptr )
        return FALSE;

    if( EQUAL(poRoot->GetValue(), "VERT_CS") )
        return TRUE;

    if( EQUAL(poRoot->GetValue(), "COMPD_CS") )
        return GetAttrNode( "VERT_CS" ) != nullptr;

    return FALSE;
}

/************************************************************************/
/*                           OSRIsVertical()                            */
/************************************************************************/
/**
 * \brief Check if vertical coordinate system.
 *
 * This function is the same as OGRSpatialReference::IsVertical().
 *
 * @since OGR 1.8.0
 */
int OSRIsVertical( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRIsVertical", 0 );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->IsVertical();
}

/************************************************************************/
/*                            CloneGeogCS()                             */
/************************************************************************/

/**
 * \brief Make a duplicate of the GEOGCS node of this OGRSpatialReference object.
 *
 * @return a new SRS, which becomes the responsibility of the caller.
 */
OGRSpatialReference *OGRSpatialReference::CloneGeogCS() const

{
    OGRSpatialReference * poNewSRS = nullptr;

/* -------------------------------------------------------------------- */
/*      We have to reconstruct the GEOGCS node for geocentric           */
/*      coordinate systems.                                             */
/* -------------------------------------------------------------------- */
    if( IsGeocentric() )
    {
        const OGR_SRSNode *poDatum = GetAttrNode( "DATUM" );
        const OGR_SRSNode *poPRIMEM = GetAttrNode( "PRIMEM" );
        OGR_SRSNode *poGeogCS = nullptr;

        if( poDatum == nullptr || poPRIMEM == nullptr )
            return nullptr;

        poGeogCS = new OGR_SRSNode( "GEOGCS" );
        poGeogCS->AddChild( new OGR_SRSNode( "unnamed" ) );
        poGeogCS->AddChild( poDatum->Clone() );
        poGeogCS->AddChild( poPRIMEM->Clone() );

        poNewSRS = new OGRSpatialReference();
        poNewSRS->SetRoot( poGeogCS );

        poNewSRS->SetAngularUnits( "degree", CPLAtof(SRS_UA_DEGREE_CONV) );

        return poNewSRS;
    }

/* -------------------------------------------------------------------- */
/*      For all others we just search the tree, and duplicate.          */
/* -------------------------------------------------------------------- */
    const OGR_SRSNode *poGeogCS = GetAttrNode( "GEOGCS" );
    if( poGeogCS == nullptr )
        return nullptr;

    poNewSRS = new OGRSpatialReference();
    poNewSRS->SetRoot( poGeogCS->Clone() );

    return poNewSRS;
}

/************************************************************************/
/*                           OSRCloneGeogCS()                           */
/************************************************************************/
/**
 * \brief Make a duplicate of the GEOGCS node of this OGRSpatialReference object.
 *
 * This function is the same as OGRSpatialReference::CloneGeogCS().
 */
OGRSpatialReferenceH CPL_STDCALL OSRCloneGeogCS( OGRSpatialReferenceH hSource )

{
    VALIDATE_POINTER1( hSource, "OSRCloneGeogCS", nullptr );

    return reinterpret_cast<OGRSpatialReferenceH>(
        reinterpret_cast<OGRSpatialReference *>(hSource)->CloneGeogCS() );
}

/************************************************************************/
/*                      IsRelativeErrorSmaller()                        */
/************************************************************************/

static bool IsRelativeErrorSmaller(double dfA, double dfB, double dfRelError)
{
    return fabs(dfA - dfB) <= dfRelError * fabs(dfA);
}

/************************************************************************/
/*                            IsSameGeogCS()                            */
/************************************************************************/

/**
 * \brief Do the GeogCS'es match?
 *
 * This method is the same as the C function OSRIsSameGeogCS().
 *
 * @param poOther the SRS being compared against.
 *
 * @return TRUE if they are the same or FALSE otherwise.
 */

int OGRSpatialReference::IsSameGeogCS( const OGRSpatialReference *poOther ) const

{
    return IsSameGeogCS( poOther, nullptr );
}

/**
 * \brief Do the GeogCS'es match?
 *
 * This method is the same as the C function OSRIsSameGeogCS().
 *
 * @param poOther the SRS being compared against.
 * @param papszOptions options. DATUM=STRICT/IGNORE. TOWGS84=STRICT/ONLY_IF_IN_BOTH/IGNORE
 *
 * @return TRUE if they are the same or FALSE otherwise.
 */

int OGRSpatialReference::IsSameGeogCS( const OGRSpatialReference *poOther,
                                       const char* const * papszOptions ) const

{

    const char *pszThisValue, *pszOtherValue;

/* -------------------------------------------------------------------- */
/*      Does the datum name match?  Note that we assume                 */
/*      compatibility if either is missing a datum.                     */
/* -------------------------------------------------------------------- */
    pszThisValue = this->GetAttrValue( "DATUM" );
    pszOtherValue = poOther->GetAttrValue( "DATUM" );

    const char* pszDatumRule =
        CSLFetchNameValueDef( papszOptions, "DATUM", "STRICT");
    if( pszThisValue != nullptr && pszOtherValue != nullptr
        && !EQUAL(pszThisValue, pszOtherValue)
        && EQUAL(pszDatumRule, "STRICT") )
    {
#if DEBUG_VERBOSE
        CPLDebug("OSR", "DATUM names do not match");
#endif
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Do the datum TOWGS84 values match if present?                   */
/* -------------------------------------------------------------------- */
    const char* pszTOWGS84Rule =
        CSLFetchNameValueDef( papszOptions, "TOWGS84", "STRICT" );
    double adfTOWGS84[7] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double adfOtherTOWGS84[7] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    bool bThisHasTOWGS84 = this->GetTOWGS84( adfTOWGS84, 7 ) == OGRERR_NONE;
    bool bOtherHasTOWGS84 = poOther->GetTOWGS84( adfOtherTOWGS84, 7 ) == OGRERR_NONE;
    if( EQUAL(pszTOWGS84Rule, "STRICT" ) ||
        (bThisHasTOWGS84 && bOtherHasTOWGS84 &&
         EQUAL(pszTOWGS84Rule, "ONLY_IF_IN_BOTH")) )
    {
        for( int i = 0; i < 7; i++ )
        {
            if( fabs(adfTOWGS84[i] - adfOtherTOWGS84[i]) > 0.00001 )
            {
#ifdef DEBUG_VERBOSE
                CPLDebug("OSR", "TOWGS84 do not match");
#endif
                return FALSE;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*    Do the prime meridians match?  If missing assume a value of zero. */
/* -------------------------------------------------------------------- */
    pszThisValue = this->GetAttrValue( "PRIMEM", 1 );
    if( pszThisValue == nullptr )
        pszThisValue = "0.0";

    pszOtherValue = poOther->GetAttrValue( "PRIMEM", 1 );
    if( pszOtherValue == nullptr )
        pszOtherValue = "0.0";

    if( !IsRelativeErrorSmaller(CPLAtof(pszOtherValue),
                                CPLAtof(pszThisValue), 1e-8) )
    {
#ifdef DEBUG_VERBOSE
        CPLDebug("OSR", "PRIMEM do not match");
#endif
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Do the units match?                                             */
/* -------------------------------------------------------------------- */
    pszThisValue = this->GetAttrValue( "GEOGCS|UNIT", 1 );
    if( pszThisValue == nullptr )
        pszThisValue = SRS_UA_DEGREE_CONV;

    pszOtherValue = poOther->GetAttrValue( "GEOGCS|UNIT", 1 );
    if( pszOtherValue == nullptr )
        pszOtherValue = SRS_UA_DEGREE_CONV;

    if( !IsRelativeErrorSmaller(CPLAtof(pszOtherValue),
                                CPLAtof(pszThisValue), 1e-8) )
    {
#ifdef DEBUG_VERBOSE
        CPLDebug("OSR", "GEOGCS|UNIT do not match");
#endif
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Does the spheroid match.  Check semi major, and inverse         */
/*      flattening.                                                     */
/* -------------------------------------------------------------------- */
    pszThisValue = this->GetAttrValue( "SPHEROID", 1 );
    pszOtherValue = poOther->GetAttrValue( "SPHEROID", 1 );
    if( pszThisValue != nullptr && pszOtherValue != nullptr
        && std::abs(CPLAtof(pszThisValue) - CPLAtof(pszOtherValue)) > 0.01 )
    {
#ifdef DEBUG_VERBOSE
        CPLDebug("OSR", "SPHEROID major do not match");
#endif
        return FALSE;
    }

    pszThisValue = this->GetAttrValue( "SPHEROID", 2 );
    pszOtherValue = poOther->GetAttrValue( "SPHEROID", 2 );
    if( pszThisValue != nullptr && pszOtherValue != nullptr
        && std::abs(CPLAtof(pszThisValue) - CPLAtof(pszOtherValue)) > 0.0001 )
    {
#ifdef DEBUG_VERBOSE
        CPLDebug("OSR", "SPHEROID inverse flattening do not match");
#endif
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                          OSRIsSameGeogCS()                           */
/************************************************************************/

/**
 * \brief Do the GeogCS'es match?
 *
 * This function is the same as OGRSpatialReference::IsSameGeogCS().
 */
int OSRIsSameGeogCS( OGRSpatialReferenceH hSRS1, OGRSpatialReferenceH hSRS2 )

{
    VALIDATE_POINTER1( hSRS1, "OSRIsSameGeogCS", 0 );
    VALIDATE_POINTER1( hSRS2, "OSRIsSameGeogCS", 0 );

    return reinterpret_cast<OGRSpatialReference *>(hSRS1)->IsSameGeogCS(
        reinterpret_cast<OGRSpatialReference *>(hSRS2) );
}

/************************************************************************/
/*                            IsSameVertCS()                            */
/************************************************************************/

/**
 * \brief Do the VertCS'es match?
 *
 * This method is the same as the C function OSRIsSameVertCS().
 *
 * @param poOther the SRS being compared against.
 *
 * @return TRUE if they are the same or FALSE otherwise.
 */

int OGRSpatialReference::IsSameVertCS( const OGRSpatialReference *poOther ) const

{
/* -------------------------------------------------------------------- */
/*      Does the datum name match?                                      */
/* -------------------------------------------------------------------- */
    const char *pszThisValue = this->GetAttrValue( "VERT_DATUM" );
    const char *pszOtherValue = poOther->GetAttrValue( "VERT_DATUM" );

    if( pszThisValue == nullptr || pszOtherValue == nullptr
        || !EQUAL(pszThisValue, pszOtherValue) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Do the units match?                                             */
/* -------------------------------------------------------------------- */
    pszThisValue = this->GetAttrValue( "VERT_CS|UNIT", 1 );
    if( pszThisValue == nullptr )
        pszThisValue = "1.0";

    pszOtherValue = poOther->GetAttrValue( "VERT_CS|UNIT", 1 );
    if( pszOtherValue == nullptr )
        pszOtherValue = "1.0";

    if( std::abs(CPLAtof(pszOtherValue) - CPLAtof(pszThisValue)) > 0.00000001 )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                          OSRIsSameVertCS()                           */
/************************************************************************/

/**
 * \brief Do the VertCS'es match?
 *
 * This function is the same as OGRSpatialReference::IsSameVertCS().
 */
int OSRIsSameVertCS( OGRSpatialReferenceH hSRS1, OGRSpatialReferenceH hSRS2 )

{
    VALIDATE_POINTER1( hSRS1, "OSRIsSameVertCS", 0 );
    VALIDATE_POINTER1( hSRS2, "OSRIsSameVertCS", 0 );

    return reinterpret_cast<OGRSpatialReference *>(hSRS1)->IsSameVertCS(
        reinterpret_cast<OGRSpatialReference *>(hSRS2) );
}

/************************************************************************/
/*                        IsDefaultParameter()                          */
/************************************************************************/

static bool IsDefaultParameter(const char* pszParamName, double dfVal)
{
    if( STARTS_WITH_CI(pszParamName, "scale") )
        return dfVal == 1.0;
    else
        return dfVal == 0.0;
}

/************************************************************************/
/*                               IsSame()                               */
/************************************************************************/

/**
 * \brief Do these two spatial references describe the same system ?
 *
 * @param poOtherSRS the SRS being compared to.
 *
 * @return TRUE if equivalent or FALSE otherwise.
 */

int OGRSpatialReference::IsSame( const OGRSpatialReference * poOtherSRS ) const

{
    return IsSame(poOtherSRS, nullptr);
}


/**
 * \brief Do these two spatial references describe the same system ?
 *
 * @param poOtherSRS the SRS being compared to.
 * @param papszOptions options. DATUM=STRICT/IGNORE. TOWGS84=STRICT/ONLY_IF_IN_BOTH/IGNORE
 *
 * @return TRUE if equivalent or FALSE otherwise.
 */

int OGRSpatialReference::IsSame( const OGRSpatialReference * poOtherSRS,
                                 const char* const * papszOptions ) const

{
    if( GetRoot() == nullptr && poOtherSRS->GetRoot() == nullptr )
        return TRUE;

    if( GetRoot() == nullptr || poOtherSRS->GetRoot() == nullptr )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Do the have the same root types?  i.e. is one PROJCS and one    */
/*      GEOGCS or perhaps LOCALCS?                                      */
/* -------------------------------------------------------------------- */
    if( !EQUAL(GetRoot()->GetValue(), poOtherSRS->GetRoot()->GetValue()) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Compare proj.4 extensions.                                      */
/* -------------------------------------------------------------------- */
    const char* pszThisProj4Ext = GetExtension(nullptr, "PROJ4", nullptr);
    const char* pszOtherProj4Ext = poOtherSRS->GetExtension(nullptr, "PROJ4", nullptr);
    if( (pszThisProj4Ext == nullptr && pszOtherProj4Ext != nullptr) ||
        (pszThisProj4Ext != nullptr && pszOtherProj4Ext == nullptr) ||
        (pszThisProj4Ext != nullptr && pszOtherProj4Ext != nullptr &&
         !EQUAL(CPLString(pszThisProj4Ext).Trim().replaceAll("  "," "),
                CPLString(pszOtherProj4Ext).Trim().replaceAll("  "," "))) )
    {
#ifdef DEBUG_VERBOSE
        CPLDebug("OSR", "Different EXTENSION");
#endif
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Compare geographic coordinate system.                           */
/* -------------------------------------------------------------------- */
    if( !IsSameGeogCS( poOtherSRS, papszOptions ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Compare projected coordinate system.                            */
/* -------------------------------------------------------------------- */
    const OGR_SRSNode *poPROJCS = GetAttrNode( "PROJCS" );
    if( poPROJCS != nullptr )
    {
        const char *pszValue1 = this->GetAttrValue( "PROJECTION" );
        const char *pszValue2 = poOtherSRS->GetAttrValue( "PROJECTION" );

        if( pszValue1 == nullptr || pszValue2 == nullptr )
        {
#ifdef DEBUG_VERBOSE
            CPLDebug("OSR", "Different PROJECTION");
#endif
            return FALSE;
        }

        if( !EQUAL(pszValue1, pszValue2) )
        {
            OGRSpatialReference* poThisInOtherProj =
                convertToOtherProjection(pszValue2);
            if( poThisInOtherProj )
            {
                int bRet = poThisInOtherProj->IsSame(poOtherSRS);
                delete poThisInOtherProj;
                return bRet;
            }
            else
            {
                OGRSpatialReference* poOtherSRSInThisProj =
                    poOtherSRS->convertToOtherProjection(pszValue1);
                if( poOtherSRSInThisProj )
                {
                    int bRet = IsSame(poOtherSRSInThisProj);
                    delete poOtherSRSInThisProj;
                    return bRet;
                }
            }

#ifdef DEBUG_VERBOSE
            CPLDebug("OSR", "Different PROJECTION");
#endif
            return FALSE;
        }

        bool bIgnoreStdParallel12 = false;
        bool bIgnoreRectifiedGridAngle = false;
        if( EQUAL(pszValue1, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) )
        {
            double dfThis1 = GetProjParm(SRS_PP_STANDARD_PARALLEL_1);
            double dfOther1 = poOtherSRS->GetProjParm(SRS_PP_STANDARD_PARALLEL_1);
            double dfThis2 = GetProjParm(SRS_PP_STANDARD_PARALLEL_2);
            double dfOther2 = poOtherSRS->GetProjParm(SRS_PP_STANDARD_PARALLEL_2);
            if( !((IsRelativeErrorSmaller(dfThis1, dfOther1, 1e-8) &&
                   IsRelativeErrorSmaller(dfThis2, dfOther2, 1e-8)) ||
                  (IsRelativeErrorSmaller(dfThis1, dfOther2, 1e-8) &&
                   IsRelativeErrorSmaller(dfThis2, dfOther1, 1e-8))) )
            {
#ifdef DEBUG_VERBOSE
                CPLDebug("OSR", "Relative error for StdParallel1/2 too big");
#endif
                return FALSE;
            }
            bIgnoreStdParallel12 = true;
        }
        else if( EQUAL(pszValue1, SRS_PT_HOTINE_OBLIQUE_MERCATOR) )
        {
            double dfAz = fmod(GetProjParm(SRS_PP_AZIMUTH) + 360.0, 360.0);
            double dfRectToSkew = fmod(GetProjParm(SRS_PP_RECTIFIED_GRID_ANGLE, dfAz) + 360.0, 360.0);
            double dfAz2 = fmod(poOtherSRS->GetProjParm(SRS_PP_AZIMUTH) + 360.0, 360.0 );
            double dfRectToSkew2 = fmod(poOtherSRS->GetProjParm(SRS_PP_RECTIFIED_GRID_ANGLE, dfAz2) + 360.0, 360.0);
            if( !IsRelativeErrorSmaller(dfRectToSkew, dfRectToSkew2, 1e-8) )
            {
#ifdef DEBUG_VERBOSE
                CPLDebug("OSR", "Relative error for rectified_grid_angle too big");
#endif
                return FALSE;
            }
            bIgnoreRectifiedGridAngle = true;
        }

        int nCountNonDefaultParameters = 0;
        for( int iChild = 0; iChild < poPROJCS->GetChildCount(); iChild++ )
        {
            const OGR_SRSNode *poNode = poPROJCS->GetChild( iChild );
            if( !EQUAL(poNode->GetValue(), "PARAMETER")
                || poNode->GetChildCount() != 2 )
                continue;

            const char* pszParamName = poNode->GetChild(0)->GetValue();
            if( bIgnoreStdParallel12 &&
                (EQUAL( pszParamName, SRS_PP_STANDARD_PARALLEL_1 ) ||
                 EQUAL( pszParamName, SRS_PP_STANDARD_PARALLEL_2 )) )
            {
                continue;
            }

            if( bIgnoreRectifiedGridAngle &&
                EQUAL( pszParamName, SRS_PP_RECTIFIED_GRID_ANGLE ) )
            {
                continue;
            }

            double dfVal = GetProjParm( pszParamName );
            if( !IsDefaultParameter(pszParamName, dfVal) )
            {
                nCountNonDefaultParameters ++;
            }

            double dfOtherVal = poOtherSRS->GetProjParm( pszParamName,
                STARTS_WITH_CI(pszParamName, "Scale") ? 1.0 : 0.0 );

            if( EQUAL(pszParamName, SRS_PP_AZIMUTH) )
            {
                dfVal = fmod(dfVal + 360.0, 360.0);
                dfOtherVal = fmod(dfOtherVal + 360.0, 360.0);
            }

            if( !IsRelativeErrorSmaller(dfVal, dfOtherVal, 1e-8) )
            {
#ifdef DEBUG_VERBOSE
                CPLDebug("OSR", "Relative error for %s too big",
                         pszParamName);
#endif
                return FALSE;
            }
        }

        const OGR_SRSNode *poOtherPROJCS = poOtherSRS->GetAttrNode( "PROJCS" );
        if( poOtherPROJCS == nullptr )
        {
            return FALSE;
        }
        int nCounterOtherNonDefaultParameters = 0;
        for( int iChild = 0; iChild < poOtherPROJCS->GetChildCount(); iChild++ )
        {
            const OGR_SRSNode *poNode = poOtherPROJCS->GetChild( iChild );
            if( !EQUAL(poNode->GetValue(), "PARAMETER")
                || poNode->GetChildCount() != 2 )
                continue;

            const char* pszParamName = poNode->GetChild(0)->GetValue();
            if( bIgnoreStdParallel12 &&
                (EQUAL( pszParamName, SRS_PP_STANDARD_PARALLEL_1 ) ||
                 EQUAL( pszParamName, SRS_PP_STANDARD_PARALLEL_2 )) )
            {
                continue;
            }

            if( bIgnoreRectifiedGridAngle &&
                EQUAL( pszParamName, SRS_PP_RECTIFIED_GRID_ANGLE ) )
            {
                continue;
            }

            double dfVal = poOtherSRS->GetProjParm( pszParamName );
            if( !IsDefaultParameter(pszParamName, dfVal) )
            {
                nCounterOtherNonDefaultParameters ++;
            }
        }

        if( nCountNonDefaultParameters != nCounterOtherNonDefaultParameters )
        {
#ifdef DEBUG_VERBOSE
            CPLDebug("OSR", "Different number of non default PARAMETER");
#endif
            return FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      If they are LOCALCS/PROJCS, do they have the same units?        */
/* -------------------------------------------------------------------- */
    if( IsLocal() || IsProjected() )
    {
        if( GetLinearUnits() != 0.0 )
        {
            // EPSG uses 0.201166195164 for Clarke's link, ESRI 0.2011661949 --> 1.3e-9 relative error
            if( !IsRelativeErrorSmaller(poOtherSRS->GetLinearUnits(),
                                        GetLinearUnits(), 1e-8 ) )
            {
#ifdef DEBUG_VERBOSE
                CPLDebug("OSR", "Relative error for linear unit too big");
#endif
                return FALSE;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Compare vertical coordinate system.                             */
/* -------------------------------------------------------------------- */
    if( IsVertical() && !IsSameVertCS( poOtherSRS ) )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                             OSRIsSame()                              */
/************************************************************************/

/**
 * \brief Do these two spatial references describe the same system ?
 *
 * This function is the same as OGRSpatialReference::IsSame().
 */
int OSRIsSame( OGRSpatialReferenceH hSRS1, OGRSpatialReferenceH hSRS2 )

{
    VALIDATE_POINTER1( hSRS1, "OSRIsSame", 0 );
    VALIDATE_POINTER1( hSRS2, "OSRIsSame", 0 );

    return reinterpret_cast<OGRSpatialReference *>(hSRS1)->IsSame(
        reinterpret_cast<OGRSpatialReference *>(hSRS2) );
}

/************************************************************************/
/*                               tsfn()                                 */
/************************************************************************/

static double tsfn(double phi, double ec)
{
    const double sinphi = sin(phi);
    const double sinphi_ec = sinphi * ec;
    return tan(0.5 * (M_PI/2 - phi)) /
               pow( (1.0 - sinphi_ec) / (1.0 + sinphi_ec), 0.5 * ec);
}

/************************************************************************/
/*                               msfn()                                 */
/************************************************************************/

static double msfn(double phi, double ec)
{
    const double sinphi = sin(phi);
    const double cosphi = cos(phi);
    const double sinphi_ec = sinphi * ec;
    return cosphi / sqrt(1.0 - sinphi_ec * sinphi_ec);
}

/************************************************************************/
/*                         lcc_1sp_to_2sp_f()                           */
/************************************************************************/

// Function whose zeroes are the sin of the standard parallels of LCC_2SP
static double lcc_1sp_to_2sp_f(double sinphi, double K, double ec, double n)
{
    const double x = sinphi;
    const double ecx = ec * x;
    return ( 1 - x* x ) / (1 - ecx * ecx) -
        K * K * pow((1.0 - x ) / (1.0 + x) *
                    pow( (1.0 + ecx)/(1.0 - ecx), ec), n);
}

/************************************************************************/
/*                    find_zero_lcc_1sp_to_2sp_f()                      */
/************************************************************************/

// Find the sin of the standard parallels of LCC_2SP
static double find_zero_lcc_1sp_to_2sp_f(double sinphi0, bool bNorth,
                                         double K, double ec)
{
    double a, b;
    double f_a;
    if( bNorth )
    {
        // Look for zero above phi0
        a = sinphi0;
        b = 1.0; // sin(North pole)
        f_a = 1.0; // some positive value, but we only care about the sign
    }
    else
    {
        // Look for zero below phi0
        a = -1.0; // sin(South pole)
        b = sinphi0;
        f_a = -1.0; // minus infinity in fact, but we only care about the sign
    }
    // We use dichotomy search. lcc_1sp_to_2sp_f() is positive at sinphi_init,
    // has a zero in ]-1,sinphi0[ and ]sinphi0,1[ ranges
    for( int N=0; N<100; N++ )
    {
        double c = (a + b) / 2;
        double f_c = lcc_1sp_to_2sp_f(c, K, ec, sinphi0);
        if ( f_c == 0.0 || (b-a) < 1e-18 )
        {
            return c;
        }
        if( (f_c > 0 && f_a > 0) || (f_c < 0 && f_a < 0) )
        {
            a = c;
            f_a = f_c;
        }
        else
        {
            b = c;
        }
    }
    return (a + b) / 2;
}

static double DegToRad(double x) { return x / 180.0 * M_PI; }
static double RadToDeg(double x) { return x / M_PI * 180.0; }

/************************************************************************/
/*                    convertToOtherProjection()                        */
/************************************************************************/

/**
 * \brief Convert to another equivalent projection
 * 
 * Currently implemented:
 * <ul>
 * <li>SRS_PT_MERCATOR_1SP to SRS_PT_MERCATOR_2SP</li>
 * <li>SRS_PT_MERCATOR_2SP to SRS_PT_MERCATOR_1SP</li>
 * <li>SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP to SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP</li>
 * <li>SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP to SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP</li>
 * </ul>
 *
 * @param pszTargetProjection target projection.
 * @param papszOptions lists of options. None supported currently.
 * @return a new SRS, or NULL in case of error.
 *
 * @since GDAL 2.3
 */
OGRSpatialReference* OGRSpatialReference::convertToOtherProjection(
                            const char* pszTargetProjection,
                            CPL_UNUSED const char* const* papszOptions ) const
{
    const char *pszProjection = GetAttrValue( "PROJECTION" );
    if( pszProjection == nullptr || pszTargetProjection == nullptr )
        return nullptr;

    if( EQUAL(pszProjection, pszTargetProjection) )
        return Clone();

    if( EQUAL(pszProjection, SRS_PT_MERCATOR_1SP) &&
        EQUAL(pszTargetProjection, SRS_PT_MERCATOR_2SP) &&
        GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0) == 0.0 )
    {
        const double k0 = GetNormProjParm(SRS_PP_SCALE_FACTOR, 1.0);
        if( !(k0 > 0 && k0 <= 1.0+ 1e-10) )
            return nullptr;
        const double e2 = GetSquaredEccentricity();
        if( e2 < 0 )
            return nullptr;
        const double dfStdP1Lat = ( k0 >= 1.0 ) ? 0.0 :
            RadToDeg(acos( sqrt( (1.0 - e2) / ((1.0 / (k0 * k0)) - e2)) ));
        OGRSpatialReference* poMerc2SP = new OGRSpatialReference();
        poMerc2SP->CopyGeogCSFrom(this);
        poMerc2SP->SetMercator2SP(
                              dfStdP1Lat,
                              GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0),
                              GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0),
                              GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0),
                              GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0) );
        return poMerc2SP;
    }

    if( EQUAL(pszProjection, SRS_PT_MERCATOR_2SP) &&
        EQUAL(pszTargetProjection, SRS_PT_MERCATOR_1SP) &&
        GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0) == 0.0 )
    {
        const double dfStdP1Lat =
            GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1, 0.0);
        const double phi1 = DegToRad(dfStdP1Lat);
        if( !(fabs(phi1) < M_PI / 2) )
            return nullptr;
        const double ec = GetEccentricity();
        if( ec < 0 )
            return nullptr;
        const double k0 = msfn(phi1, ec);
        OGRSpatialReference* poMerc1SP = new OGRSpatialReference();
        poMerc1SP->CopyGeogCSFrom(this);
        poMerc1SP->SetMercator(
                              GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0),
                              GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0),
                              k0,
                              GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0),
                              GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0) );
        return poMerc1SP;
    }

    if( EQUAL(pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP) &&
        EQUAL(pszTargetProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) )
    {
        // Notations m0, t0, n, m1, t1, F are those of the EPSG guidance
        // "1.3.1.1 Lambert Conic Conformal (2SP)" and
        // "1.3.1.2 Lambert Conic Conformal (1SP)" and
        // or Snyder pages 106-109
        const double dfLatitudeOfOrigin =
            GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0);
        const double phi0 = DegToRad(dfLatitudeOfOrigin);
        const double k0 = GetNormProjParm(SRS_PP_SCALE_FACTOR, 1.0);
        if( !(fabs(phi0) < M_PI / 2) )
            return nullptr;
        if( !(k0 > 0 && k0 <= 1.0+ 1e-10) )
            return nullptr;
        const double ec = GetEccentricity();
        if( ec < 0 )
            return nullptr;
        const double m0 = msfn(phi0, ec);
        const double t0 = tsfn(phi0, ec);
        const double n = sin(phi0);
        if( fabs(n) < 1e-10 )
            return nullptr;
        OGRSpatialReference* poLCC2SP = new OGRSpatialReference();
        poLCC2SP->CopyGeogCSFrom(this);
        if( fabs(k0 - 1.0) <= 1e-10 )
        {
            poLCC2SP->SetLCC( dfLatitudeOfOrigin,
                              dfLatitudeOfOrigin,
                              dfLatitudeOfOrigin,
                              GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0),
                              GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0),
                              GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0) );
        }
        else
        {
            const double K = k0 * m0 / pow(t0, n);
            const double phi1 =
                asin(find_zero_lcc_1sp_to_2sp_f(n, true, K, ec));
            const double phi2 =
                asin(find_zero_lcc_1sp_to_2sp_f(n, false, K, ec));
            double phi1Deg = RadToDeg(phi1);
            double phi2Deg = RadToDeg(phi2);

            // Try to round to hundreth of degree if very close to it
            if( fabs(phi1Deg * 1000 - floor(phi1Deg * 1000 + 0.5)) < 1e-8 )
                phi1Deg = floor(phi1Deg * 1000 + 0.5) / 1000;
            if( fabs(phi2Deg * 1000 - floor(phi2Deg * 1000 + 0.5)) < 1e-8 )
                phi2Deg = floor(phi2Deg * 1000 + 0.5) / 1000;

            // The following improvement is too turn the LCC1SP equivalent of
            // EPSG:2154 to the real LCC2SP
            // If the computed latitude of origin is close to .0 or .5 degrees
            // then check if rounding it to it will get a false northing
            // close to an integer
            const double FN = GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0);
            if( fabs(dfLatitudeOfOrigin * 2 -
                     floor(dfLatitudeOfOrigin * 2 + 0.5)) < 0.2 )
            {
                const double dfRoundedLatOfOrig =
                    floor(dfLatitudeOfOrigin * 2 + 0.5) / 2;
                const double m1 = msfn(phi1, ec);
                const double t1 = tsfn(phi1, ec);
                const double F = m1 / (n * pow(t1, n));
                const double a = GetSemiMajor();
                const double tRoundedLatOfOrig =
                    tsfn(DegToRad(dfRoundedLatOfOrig), ec);
                const double FN_correction =
                    a * F * (pow(tRoundedLatOfOrig, n) - pow(t0, n));
                const double FN_corrected = FN - FN_correction;
                const double FN_corrected_rounded = floor(FN_corrected + 0.5);
                if( fabs(FN_corrected - FN_corrected_rounded) < 1e-8 )
                {
                    poLCC2SP->SetLCC(
                              phi1Deg,
                              phi2Deg,
                              dfRoundedLatOfOrig,
                              GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0),
                              GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0),
                              FN_corrected_rounded );
                    return poLCC2SP;
                }
            }

            poLCC2SP->SetLCC( phi1Deg,
                              phi2Deg,
                              dfLatitudeOfOrigin,
                              GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0),
                              GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0),
                              FN );
        }
        return poLCC2SP;
    }

    if( EQUAL(pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) &&
        EQUAL(pszTargetProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP) )
    {
        // Notations m0, t0, m1, t1, m2, t2 n, F are those of the EPSG guidance
        // "1.3.1.1 Lambert Conic Conformal (2SP)" and
        // "1.3.1.2 Lambert Conic Conformal (1SP)" and
        // or Snyder pages 106-109
        const double phiF =
            DegToRad(GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0));
        const double phi1 =
            DegToRad(GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1, 0.0));
        const double phi2 =
            DegToRad(GetNormProjParm(SRS_PP_STANDARD_PARALLEL_2, 0.0));
        if( !(fabs(phiF) < M_PI / 2) )
            return nullptr;
        if( !(fabs(phi1) < M_PI / 2) )
            return nullptr;
        if( !(fabs(phi2) < M_PI / 2) )
            return nullptr;
        const double ec = GetEccentricity();
        if( ec < 0 )
            return nullptr;
        const double m1 = msfn(phi1, ec);
        const double m2 = msfn(phi2, ec);
        const double t1 = tsfn(phi1, ec);
        const double t2 = tsfn(phi2, ec);
        const double n_denom = log(t1) - log(t2);
        const double n = (fabs(n_denom) < 1e-10) ? sin(phi1) :
                                (log(m1) - log(m2)) / n_denom;
        if( fabs(n) < 1e-10 )
            return nullptr;
        const double F = m1 / (n * pow(t1, n));
        const double phi0 = asin(n);
        const double m0 = msfn(phi0, ec);
        const double t0 = tsfn(phi0, ec);
        const double F0 = m0 / (n * pow(t0, n));
        const double k0 = F / F0;
        const double a = GetSemiMajor();
        const double tF = tsfn(phiF, ec);
        const double FN_correction = a * F * (pow(tF, n) - pow(t0, n));

        OGRSpatialReference* poLCC1SP = new OGRSpatialReference();
        poLCC1SP->CopyGeogCSFrom(this);
        double phi0Deg = RadToDeg(phi0);
        // Try to round to thousandth of degree if very close to it
        if( fabs(phi0Deg * 1000 - floor(phi0Deg * 1000 + 0.5)) < 1e-8 )
            phi0Deg = floor(phi0Deg * 1000 + 0.5) / 1000;
        poLCC1SP->SetLCC1SP(
                phi0Deg,
                GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0),
                k0,
                GetNormProjParm(SRS_PP_FALSE_EASTING, 0.0),
                GetNormProjParm(SRS_PP_FALSE_NORTHING, 0.0) +
                    (fabs(FN_correction) > 1e-8 ? FN_correction : 0) );
        return poLCC1SP;
    }

    return nullptr;
}


/************************************************************************/
/*                    OSRConvertToOtherProjection()                     */
/************************************************************************/

/**
 * \brief Convert to another equivalent projection
 * 
 * Currently implemented:
 * <ul>
 * <li>SRS_PT_MERCATOR_1SP to SRS_PT_MERCATOR_2SP</li>
 * <li>SRS_PT_MERCATOR_2SP to SRS_PT_MERCATOR_1SP</li>
 * <li>SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP to SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP</li>
 * <li>SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP to SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP</li>
 * </ul>
 *
 * @param hSRS source SRS 
 * @param pszTargetProjection target projection.
 * @param papszOptions lists of options. None supported currently.
 * @return a new SRS, or NULL in case of error.
 *
 * @since GDAL 2.3
 */
OGRSpatialReferenceH OSRConvertToOtherProjection(
                                    OGRSpatialReferenceH hSRS,
                                    const char* pszTargetProjection,
                                    const char* const* papszOptions )
{
    VALIDATE_POINTER1( hSRS, "OSRConvertToOtherProjection", nullptr );
    return reinterpret_cast<OGRSpatialReferenceH>(
        reinterpret_cast<OGRSpatialReference*>(hSRS)->
            convertToOtherProjection(pszTargetProjection, papszOptions));
}

/************************************************************************/
/*                           OSRFindMatches()                           */
/************************************************************************/

/**
 * \brief Try to identify a match between the passed SRS and a related SRS
 * in a catalog (currently EPSG only)
 *
 * Matching may be partial, or may fail.
 * Returned entries will be sorted by decreasing match confidence (first
 * entry has the highest match confidence).
 *
 * The exact way matching is done may change in future versions.
 *
 * The current algorithm is:
 * - try first AutoIdentifyEPSG(). If it succeeds, return the corresponding SRS
 * - otherwise iterate over all SRS from the EPSG catalog (as found in GDAL
 *   pcs.csv and gcs.csv files+esri_extra.wkt), and find those that match the
 *   input SRS using the IsSame() function (ignoring TOWGS84 clauses)
 * - if there is a single match using IsSame() or one of the matches has the
 *   same SRS name, return it with 100% confidence
 * - if a SRS has the same SRS name, but does not pass the IsSame() criteria,
 *   return it with 50% confidence.
 * - otherwise return all candidate SRS that pass the IsSame() criteria with a
 *   90% confidence.
 *
 * A pre-built SRS cache in ~/.gdal/X.Y/srs_cache will be used if existing,
 * otherwise it will be built at the first run of this function.
 *
 * This function is the same as OGRSpatialReference::FindMatches().
 *
 * @param hSRS SRS to match
 * @param papszOptions NULL terminated list of options or NULL
 * @param pnEntries Output parameter. Number of values in the returned array.
 * @param ppanMatchConfidence Output parameter (or NULL). *ppanMatchConfidence
 * will be allocated to an array of *pnEntries whose values between 0 and 100
 * indicate the confidence in the match. 100 is the highest confidence level.
 * The array must be freed with CPLFree().
 * 
 * @return an array of SRS that match the passed SRS, or NULL. Must be freed with
 * OSRFreeSRSArray()
 *
 * @since GDAL 2.3
 */
OGRSpatialReferenceH* OSRFindMatches( OGRSpatialReferenceH hSRS,
                                      char** papszOptions,
                                      int* pnEntries,
                                      int** ppanMatchConfidence )
{
    if( pnEntries )
        *pnEntries = 0;
    if( ppanMatchConfidence )
        *ppanMatchConfidence = nullptr;
    VALIDATE_POINTER1( hSRS, "OSRFindMatches", nullptr );

    OGRSpatialReference* poSRS = reinterpret_cast<OGRSpatialReference*>(hSRS);
    return poSRS->FindMatches(papszOptions, pnEntries,
                              ppanMatchConfidence);
}

/************************************************************************/
/*                           OSRFreeSRSArray()                          */
/************************************************************************/

/**
 * \brief Free return of OSRIdentifyMatches()
 *
 * @param pahSRS array of SRS (must be NULL terminated)
 * @since GDAL 2.3
 */
void OSRFreeSRSArray(OGRSpatialReferenceH* pahSRS)
{
    if( pahSRS != nullptr )
    {
        for( int i = 0; pahSRS[i] != nullptr; ++i )
        {
            OSRRelease(pahSRS[i]);
        }
        CPLFree(pahSRS);
    }
}

/************************************************************************/
/*                             SetTOWGS84()                             */
/************************************************************************/

/**
 * \brief Set the Bursa-Wolf conversion to WGS84.
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
    OGR_SRSNode *poDatum = GetAttrNode( "DATUM" );
    if( poDatum == nullptr )
        return OGRERR_FAILURE;

    if( poDatum->FindChild( "TOWGS84" ) != -1 )
        poDatum->DestroyChild( poDatum->FindChild( "TOWGS84" ) );

    int iPosition = poDatum->GetChildCount();
    if( poDatum->FindChild("AUTHORITY") != -1 )
    {
        iPosition = poDatum->FindChild("AUTHORITY");
    }

    OGR_SRSNode *poTOWGS84 = new OGR_SRSNode("TOWGS84");
    char szValue[64] = { '\0' };

    OGRsnPrintDouble( szValue, sizeof(szValue), dfDX );
    poTOWGS84->AddChild( new OGR_SRSNode( szValue ) );

    OGRsnPrintDouble( szValue, sizeof(szValue), dfDY );
    poTOWGS84->AddChild( new OGR_SRSNode( szValue ) );

    OGRsnPrintDouble( szValue, sizeof(szValue), dfDZ );
    poTOWGS84->AddChild( new OGR_SRSNode( szValue ) );

    OGRsnPrintDouble( szValue, sizeof(szValue), dfEX );
    poTOWGS84->AddChild( new OGR_SRSNode( szValue ) );

    OGRsnPrintDouble( szValue, sizeof(szValue), dfEY );
    poTOWGS84->AddChild( new OGR_SRSNode( szValue ) );

    OGRsnPrintDouble( szValue, sizeof(szValue), dfEZ );
    poTOWGS84->AddChild( new OGR_SRSNode( szValue ) );

    OGRsnPrintDouble( szValue, sizeof(szValue), dfPPM );
    poTOWGS84->AddChild( new OGR_SRSNode( szValue ) );

    poDatum->InsertChild( poTOWGS84, iPosition );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           OSRSetTOWGS84()                            */
/************************************************************************/

/**
 * \brief Set the Bursa-Wolf conversion to WGS84.
 *
 * This function is the same as OGRSpatialReference::SetTOWGS84().
 */
OGRErr OSRSetTOWGS84( OGRSpatialReferenceH hSRS,
                      double dfDX, double dfDY, double dfDZ,
                      double dfEX, double dfEY, double dfEZ,
                      double dfPPM )

{
    VALIDATE_POINTER1( hSRS, "OSRSetTOWGS84", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        SetTOWGS84( dfDX, dfDY, dfDZ, dfEX, dfEY, dfEZ, dfPPM );
}

/************************************************************************/
/*                             GetTOWGS84()                             */
/************************************************************************/

/**
 * \brief Fetch TOWGS84 parameters, if available.
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
    const OGR_SRSNode *poNode = GetAttrNode( "TOWGS84" );

    memset( padfCoeff, 0, sizeof(double) * nCoeffCount );

    if( poNode == nullptr )
        return OGRERR_FAILURE;

    for( int i = 0; i < nCoeffCount && i < poNode->GetChildCount(); i++ )
    {
        padfCoeff[i] = CPLAtof(poNode->GetChild(i)->GetValue());
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           OSRGetTOWGS84()                            */
/************************************************************************/

/**
 * \brief Fetch TOWGS84 parameters, if available.
 *
 * This function is the same as OGRSpatialReference::GetTOWGS84().
 */
OGRErr OSRGetTOWGS84( OGRSpatialReferenceH hSRS,
                      double * padfCoeff, int nCoeffCount )

{
    VALIDATE_POINTER1( hSRS, "OSRGetTOWGS84", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        GetTOWGS84( padfCoeff, nCoeffCount);
}

/************************************************************************/
/*                         IsAngularParameter()                         */
/************************************************************************/

/** Is the passed projection parameter an angular one?
 *
 * @return TRUE or FALSE
 */

int OGRSpatialReference::IsAngularParameter( const char *pszParameterName )

{
    if( STARTS_WITH_CI(pszParameterName, "long")
        || STARTS_WITH_CI(pszParameterName, "lati")
        || EQUAL(pszParameterName, SRS_PP_CENTRAL_MERIDIAN)
        || STARTS_WITH_CI(pszParameterName, "standard_parallel")
        || EQUAL(pszParameterName, SRS_PP_AZIMUTH)
        || EQUAL(pszParameterName, SRS_PP_RECTIFIED_GRID_ANGLE) )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                        IsLongitudeParameter()                        */
/************************************************************************/

/** Is the passed projection parameter an angular longitude
 * (relative to a prime meridian)?
 *
 * @return TRUE or FALSE
 */

int OGRSpatialReference::IsLongitudeParameter( const char *pszParameterName )

{
    if( STARTS_WITH_CI(pszParameterName, "long")
        || EQUAL(pszParameterName, SRS_PP_CENTRAL_MERIDIAN) )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                         IsLinearParameter()                          */
/************************************************************************/

/** Is the passed projection parameter an linear one measured in meters or
 * some similar linear measure.
 *
 * @return TRUE or FALSE
 */
int OGRSpatialReference::IsLinearParameter( const char *pszParameterName )

{
    if( STARTS_WITH_CI(pszParameterName, "false_")
        || EQUAL(pszParameterName, SRS_PP_SATELLITE_HEIGHT) )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                            GetNormInfo()                             */
/************************************************************************/

/**
 * \brief Set the internal information for normalizing linear, and angular values.
 */
void OGRSpatialReference::GetNormInfo(void) const

{
    if( bNormInfoSet )
        return;

/* -------------------------------------------------------------------- */
/*      Initialize values.                                              */
/* -------------------------------------------------------------------- */
    OGRSpatialReference *poThis = (OGRSpatialReference *) this;

    poThis->bNormInfoSet = TRUE;

    poThis->dfFromGreenwich = GetPrimeMeridian(nullptr);
    poThis->dfToMeter = GetLinearUnits(nullptr);
    poThis->dfToDegrees = GetAngularUnits(nullptr) / CPLAtof(SRS_UA_DEGREE_CONV);
    if( fabs(poThis->dfToDegrees-1.0) < 0.000000001 )
        poThis->dfToDegrees = 1.0;
}

/************************************************************************/
/*                           FixupOrdering()                            */
/************************************************************************/

/**
 * \brief Correct parameter ordering to match CT Specification.
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
    if( GetRoot() != nullptr )
        return GetRoot()->FixupOrdering();

    return OGRERR_NONE;
}

/************************************************************************/
/*                          OSRFixupOrdering()                          */
/************************************************************************/

/**
 * \brief Correct parameter ordering to match CT Specification.
 *
 * This function is the same as OGRSpatialReference::FixupOrdering().
 */
OGRErr OSRFixupOrdering( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRFixupOrdering", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->FixupOrdering();
}

/************************************************************************/
/*                               Fixup()                                */
/************************************************************************/

/**
 * \brief Fixup as needed.
 *
 * Some mechanisms to create WKT using OGRSpatialReference, and some
 * imported WKT, are not valid according to the OGC CT specification.  This
 * method attempts to fill in any missing defaults that are required, and
 * fixup ordering problems (using OSRFixupOrdering()) so that the resulting
 * WKT is valid.
 *
 * This method should be expected to evolve over time to as problems are
 * discovered.  The following are among the fixup actions this method will
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
/*      Ensure linear units defaulted to METER if missing for PROJCS,   */
/*      GEOCCS or LOCAL_CS.                                             */
/* -------------------------------------------------------------------- */
    const OGR_SRSNode *poCS = GetAttrNode( "PROJCS" );

    if( poCS == nullptr )
        poCS = GetAttrNode( "LOCAL_CS" );

    if( poCS == nullptr )
        poCS = GetAttrNode( "GEOCCS" );

    if( poCS != nullptr && poCS->FindChild( "UNIT" ) == -1 )
        SetLinearUnits( SRS_UL_METER, 1.0 );

/* -------------------------------------------------------------------- */
/*      Ensure angular units defaulted to degrees on the GEOGCS.        */
/* -------------------------------------------------------------------- */
    poCS = GetAttrNode( "GEOGCS" );
    if( poCS != nullptr && poCS->FindChild( "UNIT" ) == -1 )
        SetAngularUnits( SRS_UA_DEGREE, CPLAtof(SRS_UA_DEGREE_CONV) );

    return FixupOrdering();
}

/************************************************************************/
/*                              OSRFixup()                              */
/************************************************************************/

/**
 * \brief Fixup as needed.
 *
 * This function is the same as OGRSpatialReference::Fixup().
 */
OGRErr OSRFixup( OGRSpatialReferenceH hSRS )

{
    VALIDATE_POINTER1( hSRS, "OSRFixup", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->Fixup();
}

/************************************************************************/
/*                            GetExtension()                            */
/************************************************************************/

/**
 * \brief Fetch extension value.
 *
 * Fetch the value of the named EXTENSION item for the identified
 * target node.
 *
 * @param pszTargetKey the name or path to the parent node of the EXTENSION.
 * @param pszName the name of the extension being fetched.
 * @param pszDefault the value to return if the extension is not found.
 *
 * @return node value if successful or pszDefault on failure.
 */

const char *OGRSpatialReference::GetExtension( const char *pszTargetKey,
                                               const char *pszName,
                                               const char *pszDefault ) const

{
/* -------------------------------------------------------------------- */
/*      Find the target node.                                           */
/* -------------------------------------------------------------------- */
    const OGR_SRSNode *poNode = pszTargetKey == nullptr
        ? poRoot
        : const_cast<OGRSpatialReference *>(this)->GetAttrNode( pszTargetKey );

    if( poNode == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Fetch matching EXTENSION if there is one.                       */
/* -------------------------------------------------------------------- */
    for( int i = poNode->GetChildCount()-1; i >= 0; i-- )
    {
        const OGR_SRSNode *poChild = poNode->GetChild(i);

        if( EQUAL(poChild->GetValue(), "EXTENSION")
            && poChild->GetChildCount() >= 2 )
        {
            if( EQUAL(poChild->GetChild(0)->GetValue(), pszName) )
                return poChild->GetChild(1)->GetValue();
        }
    }

    return pszDefault;
}

/************************************************************************/
/*                            SetExtension()                            */
/************************************************************************/
/**
 * \brief Set extension value.
 *
 * Set the value of the named EXTENSION item for the identified
 * target node.
 *
 * @param pszTargetKey the name or path to the parent node of the EXTENSION.
 * @param pszName the name of the extension being fetched.
 * @param pszValue the value to set
 *
 * @return OGRERR_NONE on success
 */

OGRErr OGRSpatialReference::SetExtension( const char *pszTargetKey,
                                          const char *pszName,
                                          const char *pszValue )

{
/* -------------------------------------------------------------------- */
/*      Find the target node.                                           */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poNode = nullptr;

    if( pszTargetKey == nullptr )
        poNode = poRoot;
    else
        poNode =
            const_cast<OGRSpatialReference *>(this)->GetAttrNode(pszTargetKey);

    if( poNode == nullptr )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Fetch matching EXTENSION if there is one.                       */
/* -------------------------------------------------------------------- */
    for( int i = poNode->GetChildCount()-1; i >= 0; i-- )
    {
        OGR_SRSNode *poChild = poNode->GetChild(i);

        if( EQUAL(poChild->GetValue(), "EXTENSION")
            && poChild->GetChildCount() >= 2 )
        {
            if( EQUAL(poChild->GetChild(0)->GetValue(), pszName) )
            {
                poChild->GetChild(1)->SetValue( pszValue );
                return OGRERR_NONE;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Create a new EXTENSION node.                                    */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poAuthNode = new OGR_SRSNode( "EXTENSION" );
    poAuthNode->AddChild( new OGR_SRSNode( pszName ) );
    poAuthNode->AddChild( new OGR_SRSNode( pszValue ) );

    poNode->AddChild( poAuthNode );

    return OGRERR_NONE;
}

/************************************************************************/
/*                             OSRCleanup()                             */
/************************************************************************/

CPL_C_START
void CleanupESRIDatumMappingTable();
CPL_C_END
static void CleanupSRSWGS84Mutex();
void CleanupFindMatchesCacheAndMutex();

/**
 * \brief Cleanup cached SRS related memory.
 *
 * This function will attempt to cleanup any cache spatial reference
 * related information, such as cached tables of coordinate systems.
 */
void OSRCleanup( void )

{
    CleanupESRIDatumMappingTable();
    CSVDeaccess( nullptr );
    OCTCleanupProjMutex();
    CleanupSRSWGS84Mutex();
    CleanupFindMatchesCacheAndMutex();
}

/************************************************************************/
/*                              GetAxis()                               */
/************************************************************************/

/**
 * \brief Fetch the orientation of one axis.
 *
 * Fetches the request axis (iAxis - zero based) from the
 * indicated portion of the coordinate system (pszTargetKey) which
 * should be either "GEOGCS" or "PROJCS".
 *
 * No CPLError is issued on routine failures (such as not finding the AXIS).
 *
 * This method is equivalent to the C function OSRGetAxis().
 *
 * @param pszTargetKey the coordinate system part to query ("PROJCS" or "GEOGCS").
 * @param iAxis the axis to query (0 for first, 1 for second).
 * @param peOrientation location into which to place the fetch orientation, may be NULL.
 *
 * @return the name of the axis or NULL on failure.
 */

const char *
OGRSpatialReference::GetAxis( const char *pszTargetKey, int iAxis,
                              OGRAxisOrientation *peOrientation ) const

{
    if( peOrientation != nullptr )
        *peOrientation = OAO_Other;

/* -------------------------------------------------------------------- */
/*      Find the target node.                                           */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poNode = nullptr;

    if( pszTargetKey == nullptr )
        poNode = poRoot;
    else
        poNode =
            const_cast<OGRSpatialReference *>(this)->GetAttrNode(pszTargetKey);

    if( poNode == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Find desired child AXIS.                                        */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poAxis = nullptr;
    const int nChildCount = poNode->GetChildCount();

    for( int iChild = 0; iChild < nChildCount; iChild++ )
    {
        OGR_SRSNode *poChild = poNode->GetChild( iChild );

        if( !EQUAL(poChild->GetValue(), "AXIS") )
            continue;

        if( iAxis == 0 )
        {
            poAxis = poChild;
            break;
        }
        iAxis--;
    }

    if( poAxis == nullptr )
        return nullptr;

    if( poAxis->GetChildCount() < 2 )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Extract name and orientation if possible.                       */
/* -------------------------------------------------------------------- */
    if( peOrientation != nullptr )
    {
        const char *pszOrientation = poAxis->GetChild(1)->GetValue();

        if( EQUAL(pszOrientation, "NORTH") )
            *peOrientation = OAO_North;
        else if( EQUAL(pszOrientation, "EAST") )
            *peOrientation = OAO_East;
        else if( EQUAL(pszOrientation, "SOUTH") )
            *peOrientation = OAO_South;
        else if( EQUAL(pszOrientation, "WEST") )
            *peOrientation = OAO_West;
        else if( EQUAL(pszOrientation, "UP") )
            *peOrientation = OAO_Up;
        else if( EQUAL(pszOrientation, "DOWN") )
            *peOrientation = OAO_Down;
        else if( EQUAL(pszOrientation, "OTHER") )
            *peOrientation = OAO_Other;
        else
        {
            CPLDebug( "OSR", "Unrecognized orientation value '%s'.",
                      pszOrientation );
        }
    }

    return poAxis->GetChild(0)->GetValue();
}

/************************************************************************/
/*                             OSRGetAxis()                             */
/************************************************************************/

/**
 * \brief Fetch the orientation of one axis.
 *
 * This method is the equivalent of the C++ method OGRSpatialReference::GetAxis
 */
const char *OSRGetAxis( OGRSpatialReferenceH hSRS,
                        const char *pszTargetKey, int iAxis,
                        OGRAxisOrientation *peOrientation )

{
    VALIDATE_POINTER1( hSRS, "OSRGetAxis", nullptr );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->GetAxis( pszTargetKey, iAxis,
                                                              peOrientation );
}

/************************************************************************/
/*                         OSRAxisEnumToName()                          */
/************************************************************************/

/**
 * \brief Return the string representation for the OGRAxisOrientation enumeration.
 *
 * For example "NORTH" for OAO_North.
 *
 * @return an internal string
 */
const char *OSRAxisEnumToName( OGRAxisOrientation eOrientation )

{
    if( eOrientation == OAO_North )
        return "NORTH";
    if( eOrientation == OAO_East )
        return "EAST";
    if( eOrientation == OAO_South )
        return "SOUTH";
    if( eOrientation == OAO_West )
        return "WEST";
    if( eOrientation == OAO_Up )
        return "UP";
    if( eOrientation == OAO_Down )
        return "DOWN";
    if( eOrientation == OAO_Other )
        return "OTHER";

    return "UNKNOWN";
}

/************************************************************************/
/*                              SetAxes()                               */
/************************************************************************/

/**
 * \brief Set the axes for a coordinate system.
 *
 * Set the names, and orientations of the axes for either a projected
 * (PROJCS) or geographic (GEOGCS) coordinate system.
 *
 * This method is equivalent to the C function OSRSetAxes().
 *
 * @param pszTargetKey either "PROJCS" or "GEOGCS", must already exist in SRS.
 * @param pszXAxisName name of first axis, normally "Long" or "Easting".
 * @param eXAxisOrientation normally OAO_East.
 * @param pszYAxisName name of second axis, normally "Lat" or "Northing".
 * @param eYAxisOrientation normally OAO_North.
 *
 * @return OGRERR_NONE on success or an error code.
 */

OGRErr
OGRSpatialReference::SetAxes( const char *pszTargetKey,
                              const char *pszXAxisName,
                              OGRAxisOrientation eXAxisOrientation,
                              const char *pszYAxisName,
                              OGRAxisOrientation eYAxisOrientation )

{
/* -------------------------------------------------------------------- */
/*      Find the target node.                                           */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poNode = nullptr;

    if( pszTargetKey == nullptr )
        poNode = poRoot;
    else
        poNode = reinterpret_cast<OGRSpatialReference *>(this)->
            GetAttrNode( pszTargetKey );

    if( poNode == nullptr )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Strip any existing AXIS children.                               */
/* -------------------------------------------------------------------- */
    while( poNode->FindChild( "AXIS" ) >= 0 )
        poNode->DestroyChild( poNode->FindChild( "AXIS" ) );

/* -------------------------------------------------------------------- */
/*      Insert desired axes                                             */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poAxis = new OGR_SRSNode( "AXIS" );

    poAxis->AddChild( new OGR_SRSNode( pszXAxisName ) );
    poAxis->AddChild( new OGR_SRSNode( OSRAxisEnumToName(eXAxisOrientation) ));

    poNode->AddChild( poAxis );

    poAxis = new OGR_SRSNode( "AXIS" );

    poAxis->AddChild( new OGR_SRSNode( pszYAxisName ) );
    poAxis->AddChild( new OGR_SRSNode( OSRAxisEnumToName(eYAxisOrientation) ));

    poNode->AddChild( poAxis );

    return OGRERR_NONE;
}

/************************************************************************/
/*                             OSRSetAxes()                             */
/************************************************************************/
/**
 * \brief Set the axes for a coordinate system.
 *
 * This method is the equivalent of the C++ method OGRSpatialReference::SetAxes
 */
OGRErr OSRSetAxes( OGRSpatialReferenceH hSRS,
                   const char *pszTargetKey,
                   const char *pszXAxisName,
                   OGRAxisOrientation eXAxisOrientation,
                   const char *pszYAxisName,
                   OGRAxisOrientation eYAxisOrientation )
{
    VALIDATE_POINTER1( hSRS, "OSRSetAxes", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->SetAxes( pszTargetKey,
                                                              pszXAxisName,
                                                              eXAxisOrientation,
                                                              pszYAxisName,
                                                    eYAxisOrientation );
}

#ifdef HAVE_MITAB
char CPL_DLL *MITABSpatialRef2CoordSys( OGRSpatialReference * );
OGRSpatialReference CPL_DLL * MITABCoordSys2SpatialRef( const char * );
#endif

/************************************************************************/
/*                       OSRExportToMICoordSys()                        */
/************************************************************************/
/**
 * \brief Export coordinate system in Mapinfo style CoordSys format.
 *
 * This method is the equivalent of the C++ method OGRSpatialReference::exportToMICoordSys
 */
OGRErr OSRExportToMICoordSys( OGRSpatialReferenceH hSRS, char ** ppszReturn )

{
    VALIDATE_POINTER1( hSRS, "OSRExportToMICoordSys", OGRERR_FAILURE );

    *ppszReturn = nullptr;

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->exportToMICoordSys( ppszReturn );
}

/************************************************************************/
/*                         exportToMICoordSys()                         */
/************************************************************************/

/**
 * \brief Export coordinate system in Mapinfo style CoordSys format.
 *
 * Note that the returned WKT string should be freed with
 * CPLFree() when no longer needed.  It is the responsibility of the caller.
 *
 * This method is the same as the C function OSRExportToMICoordSys().
 *
 * @param ppszResult pointer to which dynamically allocated Mapinfo CoordSys
 * definition will be assigned.
 *
 * @return OGRERR_NONE on success, OGRERR_FAILURE on failure,
 * OGRERR_UNSUPPORTED_OPERATION if MITAB library was not linked in.
 */

OGRErr OGRSpatialReference::exportToMICoordSys( char **ppszResult ) const

{
#ifdef HAVE_MITAB
    *ppszResult = MITABSpatialRef2CoordSys( (OGRSpatialReference *) this );
    if( *ppszResult != nullptr && strlen(*ppszResult) > 0 )
        return OGRERR_NONE;

    return OGRERR_FAILURE;
#else
    CPLError( CE_Failure, CPLE_NotSupported,
              "MITAB not available, CoordSys support disabled." );

    return OGRERR_UNSUPPORTED_OPERATION;
#endif
}

/************************************************************************/
/*                       OSRImportFromMICoordSys()                      */
/************************************************************************/
/**
 * \brief Import Mapinfo style CoordSys definition.
 *
 * This method is the equivalent of the C++ method OGRSpatialReference::importFromMICoordSys
 */

OGRErr OSRImportFromMICoordSys( OGRSpatialReferenceH hSRS,
                                const char *pszCoordSys )

{
    VALIDATE_POINTER1( hSRS, "OSRImportFromMICoordSys", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->importFromMICoordSys( pszCoordSys );
}

/************************************************************************/
/*                        importFromMICoordSys()                        */
/************************************************************************/

/**
 * \brief Import Mapinfo style CoordSys definition.
 *
 * The OGRSpatialReference is initialized from the passed Mapinfo style CoordSys definition string.
 *
 * This method is the equivalent of the C function OSRImportFromMICoordSys().
 *
 * @param pszCoordSys Mapinfo style CoordSys definition string.
 *
 * @return OGRERR_NONE on success, OGRERR_FAILURE on failure,
 * OGRERR_UNSUPPORTED_OPERATION if MITAB library was not linked in.
 */

OGRErr OGRSpatialReference::importFromMICoordSys( const char *pszCoordSys )

{
#ifdef HAVE_MITAB
    OGRSpatialReference *poResult = MITABCoordSys2SpatialRef( pszCoordSys );

    if( poResult == nullptr )
        return OGRERR_FAILURE;

    *this = *poResult;
    delete poResult;

    return OGRERR_NONE;
#else
    CPLError( CE_Failure, CPLE_NotSupported,
              "MITAB not available, CoordSys support disabled." );

    return OGRERR_UNSUPPORTED_OPERATION;
#endif
}

/************************************************************************/
/*                        OSRCalcInvFlattening()                        */
/************************************************************************/

/**
 * \brief Compute inverse flattening from semi-major and semi-minor axis
 *
 * @param dfSemiMajor Semi-major axis length.
 * @param dfSemiMinor Semi-minor axis length.
 *
 * @return inverse flattening, or 0 if both axis are equal.
 * @since GDAL 2.0
 */

double OSRCalcInvFlattening( double dfSemiMajor, double dfSemiMinor )
{
    if( fabs(dfSemiMajor-dfSemiMinor) < 1e-1 )
        return 0;
    if( dfSemiMajor <= 0 || dfSemiMinor <= 0 || dfSemiMinor > dfSemiMajor )
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "OSRCalcInvFlattening(): Wrong input values");
        return 0;
    }

    return dfSemiMajor / (dfSemiMajor - dfSemiMinor);
}

/************************************************************************/
/*                        OSRCalcInvFlattening()                        */
/************************************************************************/

/**
 * \brief Compute semi-minor axis from semi-major axis and inverse flattening.
 *
 * @param dfSemiMajor Semi-major axis length.
 * @param dfInvFlattening Inverse flattening or 0 for sphere.
 *
 * @return semi-minor axis
 * @since GDAL 2.0
 */

double OSRCalcSemiMinorFromInvFlattening( double dfSemiMajor, double dfInvFlattening )
{
    if( fabs(dfInvFlattening) < 0.000000000001 )
        return dfSemiMajor;
    if( dfSemiMajor <= 0.0 || dfInvFlattening <= 1.0 )
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "OSRCalcSemiMinorFromInvFlattening(): Wrong input values");
        return dfSemiMajor;
    }

    return dfSemiMajor * (1.0 - 1.0/dfInvFlattening);
}

/************************************************************************/
/*                        GetWGS84SRS()                                 */
/************************************************************************/

static OGRSpatialReference* poSRSWGS84 = nullptr;
static CPLMutex* hMutex = nullptr;

/**
 * \brief Returns an instance of a SRS object with WGS84 WKT.
 *
 * The reference counter of the returned object is not increased by this operation.
 *
 * @return instance.
 * @since GDAL 2.0
 */

OGRSpatialReference* OGRSpatialReference::GetWGS84SRS()
{
    CPLMutexHolderD(&hMutex);
    if( poSRSWGS84 == nullptr )
        poSRSWGS84 = new OGRSpatialReference(SRS_WKT_WGS84);
    return poSRSWGS84;
}

/************************************************************************/
/*                        CleanupSRSWGS84Mutex()                       */
/************************************************************************/

static void CleanupSRSWGS84Mutex()
{
    if( hMutex != nullptr )
    {
        poSRSWGS84->Release();
        poSRSWGS84 = nullptr;
        CPLDestroyMutex(hMutex);
        hMutex = nullptr;
    }
}
