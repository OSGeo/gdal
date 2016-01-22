/******************************************************************************
 * $Id$
 *
 * Project:  Web Coordinate Transformation Service
 * Purpose:  cgi-bin mainline for WCTS Implementation
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdma <warmerdam@pobox.com>
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
 * Independent Security Audit 2003/04/17 Andrey Kiselev:
 *   Completed audit of this module and the same required items:
 * 
 *   - CSLTokenizeString*() and other functions from cpl_string.cpp;
 *   - XMP parsing and serializing functions from cpl_minixml.cpp;
 *   - GML Translation modules (gml2ogrgeometr.cpp, ogr2gmlgeometry.cpp);
 *
 * Security Audit 2003/03/29 warmerda:
 *   Completed security audit.  I believe that *this* module may be safely used
 *   to handle arbitrary input.  It also requires the following to be safe:
 *
 *    1) libcurl (not checked), all URLs other than http, https and ftp have
 *       been disabled to avoid issues with less known protocols.
 *    2) CPLTokenize() support for parsing QUERY_STRING.  (checked)
 *    3) OGR GML Geometry reading and writing services. (checked)
 *    4) OGR GML CRS reading and writing services.  (not checked)
 *    5) cpl_minixml.cpp parsing and serializing services (checked)
 *    6) cpl_string escaping logic, and stringlist handling (checked)
 * 
 *   For optimal overall security this server should be run user defined CRS
 *   support as this code is in flux, so any audit will be rapidly outdated.
 *   This may be accomplished by compiling with DISABLE_USER_DEFINED_CRS
 *   defined in the GNUmakefile (the default).
 *
 */

#include <assert.h>
#include "cpl_minixml.h"
#include "ogr_api.h"
#include "ogrsf_frmts.h"
#include "cpl_conv.h"
#include "cpl_string.h"

#ifdef HAVE_CURL
#include <curl/curl.h>
#endif

CPL_CVSID("$Id$");

/************************************************************************/
/*                      WCTSEmitServiceException()                      */
/************************************************************************/

static void WCTSEmitServiceException( const char *pszMessage )

{
    /* printf( "Content-type: text/xml%c%c", 10, 10 ); */
    printf("Content-type: application/vnd.ogc.se_xml%c%c",10,10);

    printf( "<?xml version='1.0' encoding=\"%s\" standalone=\"no\" ?>\n",
            "ISO-8859-1" );

    printf("<!DOCTYPE ServiceExceptionReport SYSTEM \"http://www.digitalearth.gov/wmt/xml/exception_1_1_0.dtd\">\n");

    printf("<ServiceExceptionReport version=\"1.1.0\">\n");        
    printf("<ServiceException>\n");
    printf("%s\n", pszMessage ); /* this should likely be XML escaped */
    printf("</ServiceException>\n");
    printf("</ServiceExceptionReport>\n");

    exit( 1 );
}

/************************************************************************/
/*                            WCTSWriteFct()                            */
/*                                                                      */
/*      Append incoming text to our collection buffer, reallocating     */
/*      it larger as needed.                                            */
/************************************************************************/

size_t WCTSWriteFct(void *buffer, size_t size, size_t nmemb, void *reqInfo)

{
    char **ppszWorkBuffer = (char **) reqInfo;
    int  nNewSize, nOldSize;

    if( *ppszWorkBuffer == NULL )
        nOldSize = 0;
    else
        nOldSize = strlen(*ppszWorkBuffer);

    nNewSize = nOldSize + nmemb * size + 1;

    *ppszWorkBuffer = (char *) CPLRealloc(*ppszWorkBuffer, nNewSize);
    strncpy( (*ppszWorkBuffer) + nOldSize, (char *) buffer, 
             nmemb * size );
    (*ppszWorkBuffer)[nNewSize-1] = '\0';

    return nmemb;
}

/************************************************************************/
/*                           WCTSHTTPFetch()                            */
/*                                                                      */
/*      Fetch a document from an url and return in a string.            */
/************************************************************************/

char *WCTSHTTPFetch( const char *pszURL )

{
#ifndef HAVE_CURL
    WCTSEmitServiceException( "Server not compiled with libcurl support, remote requests not supported." );
    return NULL;
#else
    CURL *http_handle;
    char *pszData = NULL;
    char szCurlErrBuf[CURL_ERROR_SIZE+1];
    CURLcode error;

    http_handle = curl_easy_init();

    curl_easy_setopt(http_handle, CURLOPT_URL, pszURL );

    /* Enable following redirections.  Requires libcurl 7.10.1 at least */
    curl_easy_setopt(http_handle, CURLOPT_FOLLOWLOCATION, 1 );
    curl_easy_setopt(http_handle, CURLOPT_MAXREDIRS, 10 );
    
    /* Set timeout.*/
    curl_easy_setopt(http_handle, CURLOPT_TIMEOUT, 15 );

    /* NOSIGNAL should be set to true for timeout to work in multithread
     * environments on Unix, requires libcurl 7.10 or more recent.
     * (this force avoiding the use of sgnal handlers)
     */
#ifdef CURLOPT_NOSIGNAL
    curl_easy_setopt(http_handle, CURLOPT_NOSIGNAL, 1 );
#endif

    curl_easy_setopt(http_handle, CURLOPT_WRITEDATA, &pszData );
    curl_easy_setopt(http_handle, CURLOPT_WRITEFUNCTION, WCTSWriteFct );

    szCurlErrBuf[0] = '\0';

    curl_easy_setopt(http_handle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

    error = curl_easy_perform( http_handle );

    curl_easy_cleanup( http_handle );

    if( strlen(szCurlErrBuf) > 0 )
        WCTSEmitServiceException( szCurlErrBuf );
    else if( pszData == NULL )
        WCTSEmitServiceException( "No response from WCTS server." );

    return pszData;
#endif /* def HAVE_CURL */
}

/************************************************************************/
/*                          WCTSAuthId2crsId()                          */
/*                                                                      */
/*      Convert a KVP format CRS keyword into XML format.  Returns      */
/*      the crsID node and done.                                        */
/************************************************************************/

CPLXMLNode *WCTSAuthId2crsId( char **papszParms, const char *pszName )

{
    const char *pszAuthId = CSLFetchNameValue( papszParms, pszName );
    CPLXMLNode *psCRSId;
    char **papszTokens;

    if( pszAuthId == NULL )
        WCTSEmitServiceException( 
            CPLSPrintf( "%s keyword missing", pszName ) );
    
    papszTokens = CSLTokenizeString2( pszAuthId, ":", 0 );
    if( CSLCount(papszTokens) != 2 )
        WCTSEmitServiceException( 
            CPLSPrintf( "%.500s value corrupt, use 'authority:code'.",
                        pszName ));
    
    psCRSId = CPLCreateXMLNode( NULL, CXT_Element, "crsID" );
    
    CPLCreateXMLElementAndValue( psCRSId, "gml:codeSpace", papszTokens[0]);
    CPLCreateXMLElementAndValue( psCRSId, "gml:code", papszTokens[1] );
    
    CSLDestroy( papszTokens );

    return psCRSId;
}


/************************************************************************/
/*                       WCTSCollectKVPRequest()                        */
/*                                                                      */
/*      Build an XML tree representation of a request received in       */
/*      KVP format via QUERY_STRING.                                    */
/************************************************************************/

CPLXMLNode *WCTSCollectKVPRequest()

{
    char **papszParmList;

/* -------------------------------------------------------------------- */
/*      Parse the query string.                                         */
/* -------------------------------------------------------------------- */
    if( getenv("QUERY_STRING") == NULL )
        WCTSEmitServiceException( "QUERY_STRING not set." );

    papszParmList = CSLTokenizeString2( getenv("QUERY_STRING"), "&",
                                        CSLT_PRESERVEESCAPES );
    
/* -------------------------------------------------------------------- */
/*      Un-url-encode the items.                                        */
/* -------------------------------------------------------------------- */
    int i;

    for( i = 0; papszParmList != NULL && papszParmList[i] != NULL; i++ )
    {
        char *pszNewValue = CPLUnescapeString( papszParmList[i], 
                                               NULL, CPLES_URL );
        
        CPLFree( papszParmList[i] );
        papszParmList[i] = pszNewValue;
    }

/* -------------------------------------------------------------------- */
/*      Check for REQUEST                                               */
/* -------------------------------------------------------------------- */
    const char *pszVersion = CSLFetchNameValue(papszParmList,"VERSION");
    const char *pszRequest = CSLFetchNameValue(papszParmList,"REQUEST");

    if( pszRequest == NULL )
        WCTSEmitServiceException( "REQUEST not provided in KVP URL." );

/* -------------------------------------------------------------------- */
/*      Handle GetCapabilities                                          */
/* -------------------------------------------------------------------- */
    else if( EQUAL(pszRequest,"GetCapabilities") )
    {
        CPLXMLNode *psRequest = CPLCreateXMLNode( NULL, CXT_Element, 
                                                  "GetCapabilities" );

        if( pszVersion != NULL )
        {
            CPLCreateXMLNode( 
                CPLCreateXMLNode( psRequest, CXT_Attribute, "version" ),
                CXT_Text, pszVersion );
        }

        if( CSLFetchNameValue(papszParmList,"SERVICE") != NULL )
        {
            CPLCreateXMLNode( 
                CPLCreateXMLNode( psRequest, CXT_Attribute, "service" ),
                CXT_Text, CSLFetchNameValue(papszParmList,"SERVICE") );
        }

        return psRequest;
    }

/* ==================================================================== */
/*      Handle IsTransformable                                          */
/* ==================================================================== */
    else if( EQUAL(pszRequest,"IsTransformable") )
    {
        CPLXMLNode *psRequest = CPLCreateXMLNode( NULL, CXT_Element, 
                                                  "IsTransformable" );

/* -------------------------------------------------------------------- */
/*      Translate the source crs.                                       */
/* -------------------------------------------------------------------- */
        CPLAddXMLChild( 
            CPLCreateXMLNode( psRequest, CXT_Element, "SourceCRS" ),
            WCTSAuthId2crsId( papszParmList, "SOURCECRS" ) );

/* -------------------------------------------------------------------- */
/*      Translate the destination crs.                                  */
/* -------------------------------------------------------------------- */
        CPLAddXMLChild( 
            CPLCreateXMLNode( psRequest, CXT_Element, "TargetCRS" ),
            WCTSAuthId2crsId( papszParmList, "TARGETCRS" ) );

/* -------------------------------------------------------------------- */
/*      Handle version.                                                 */
/* -------------------------------------------------------------------- */
        if( pszVersion != NULL )
        {
            CPLCreateXMLNode( 
                CPLCreateXMLNode( psRequest, CXT_Attribute, "version" ),
                CXT_Text, pszVersion );
        }

/* -------------------------------------------------------------------- */
/*      geometric primitive.                                            */
/* -------------------------------------------------------------------- */
        if( CSLFetchNameValue(papszParmList,"GEOMETRICPRIMITIVE") != NULL )
        {
            CPLCreateXMLElementAndValue( 
                psRequest, "GeometricPrimitive", 
                CSLFetchNameValue(papszParmList,"GEOMETRICPRIMITIVE") );
        }

        /* Add COVERAGETYPE and COVERAGEINTERPOLATIONMETHOD layer? */

        return psRequest;
    }

/* -------------------------------------------------------------------- */
/*      Unrecognised.                                                   */
/* -------------------------------------------------------------------- */
    else
        WCTSEmitServiceException( 
            CPLSPrintf( "Unrecognised REQUEST value (%.500s).", pszRequest) );

    return NULL;
}

/************************************************************************/
/*                         WCTSCollectRequest()                         */
/*                                                                      */
/*      This function will return an XML document in CPLXMLNode tree    */
/*      format corresponding to the current request.  If an error       */
/*      occurs the function does not return.  GET KVP style requests    */
/*      are internally converted into XML format.                       */
/************************************************************************/

CPLXMLNode *WCTSCollectRequest()

{
    if( getenv("REQUEST_METHOD") == NULL )
        WCTSEmitServiceException( "REQUEST_METHOD not set." );

    if( EQUAL(getenv("REQUEST_METHOD"),"GET") )
        return WCTSCollectKVPRequest();

/* -------------------------------------------------------------------- */
/*      Read the body of the POST message into a buffer.                */
/* -------------------------------------------------------------------- */
    int nContentLength = 0;
    char *pszXML = NULL;

    if( getenv("CONTENT_LENGTH") != NULL )
    {
        nContentLength = atoi(getenv("CONTENT_LENGTH"));

        pszXML = (char *) CPLMalloc(nContentLength+1);
        
        if( (int) fread(pszXML, 1, nContentLength, stdin) < nContentLength )
            WCTSEmitServiceException( "POST body is short." );

        pszXML[nContentLength] = '\0';
    }

    else
    {
        int nXMLMax, nXMLLen=0;

        nXMLMax = 100;
        pszXML = (char *) CPLMalloc(nXMLMax);
        
        while( !feof(stdin) )
        {
            pszXML[nXMLLen++] = fgetc(stdin);
            if( nXMLLen == nXMLMax )
            {
                nXMLMax = nXMLMax * 2;
                pszXML = (char *) CPLRealloc(pszXML, nXMLMax);
            }
        }

        pszXML[nXMLLen] = '\0';
    }

/* -------------------------------------------------------------------- */
/*      Convert into an XML document.                                   */
/* -------------------------------------------------------------------- */
    CPLErrorReset();

    CPLXMLNode *psTree = CPLParseXMLString( pszXML );
    CPLFree( pszXML );

    if( CPLGetLastErrorType() == CE_Failure )
        WCTSEmitServiceException( CPLGetLastErrorMsg() );

    return psTree;
}

/************************************************************************/
/*                        WCTSGetCapabilities()                         */
/*                                                                      */
/*      For now we just return a fixed capabilities document from       */
/*      the file system.  No real need to dynamically generate          */
/*      this except possibly to insert the coordinate system list       */
/*      based on scanning pcs.csv and gcs.csv.                          */
/************************************************************************/

void WCTSGetCapabilities( CPLXMLNode *psOperation )

{
/* -------------------------------------------------------------------- */
/*      Verify the service.                                             */
/* -------------------------------------------------------------------- */
    if( !EQUAL(CPLGetXMLValue(psOperation,"service","WCTS"),"WCTS") )
    {
        WCTSEmitServiceException( 
            CPLSPrintf( "Attempt to GetCapabilities for unsupported '%.500s'\n"
                        "service.  Only WCTS supported.",
                        CPLGetXMLValue(psOperation,"service","WCTS") ) );
    }

/* -------------------------------------------------------------------- */
/*      Search for our capabilities document.                           */
/* -------------------------------------------------------------------- */
    const char *pszCapFilename;
    FILE *fp;

    pszCapFilename = CPLFindFile( "gdal", "wcts_capabilities.xml.0.1.0" );

    if( pszCapFilename == NULL 
        || (fp = VSIFOpen( pszCapFilename, "rt")) == NULL )
    {
        WCTSEmitServiceException( "WCTS server misconfigured, "
                                  "unable to find capabilities document." );
    }

/* -------------------------------------------------------------------- */
/*      Emit the document.                                              */
/* -------------------------------------------------------------------- */
    int nLen;
    char *pszDoc;

    VSIFSeek( fp, 0, SEEK_END );
    nLen = VSIFTell( fp );
    VSIFSeek( fp, 0, SEEK_SET );
    
    pszDoc = (char *) CPLMalloc(nLen);
    VSIFRead( pszDoc, 1, nLen, fp );
    VSIFClose( fp );

    printf( "Content-type: text/xml%c%c", 10, 10 );

    VSIFWrite( pszDoc, 1, nLen, stdout );
    fflush( stdout );

    CPLFree( pszDoc );

    exit( 0 );
}

/************************************************************************/
/*                WCTSImportCoordinateReferenceSystem()                 */
/*                                                                      */
/*      This is a place holder. Eventually this will use                */
/*      OGRSpatialReference.importFromXML() when that has been          */
/*      updated to the GML 3.0 CRS formats.                             */
/************************************************************************/

OGRSpatialReference *
WCTSImportCoordinateReferenceSystem( CPLXMLNode *psXMLCRS )

{
    CPLStripXMLNamespace( psXMLCRS->psChild, NULL, TRUE );

/* ==================================================================== */
/*      Try to find a direct crsID as per old specification.            */
/* ==================================================================== */
    const char *pszCode = CPLGetXMLValue( psXMLCRS, "crsID.code", NULL );
    const char *pszCodeSpace = CPLGetXMLValue( psXMLCRS, "crsID.codeSpace", 
                                               NULL );

    if( pszCode != NULL && pszCodeSpace != NULL )
    {
/* -------------------------------------------------------------------- */
/*      Get the EPSG code, and verify that it is in the EPSG            */
/*      codeSpace.                                                      */
/* -------------------------------------------------------------------- */
        OGRSpatialReference oSRS;

        if( EQUAL(pszCodeSpace,"EPSG") )
        {
            int nEPSGCode = atoi(pszCode);
            
            if( nEPSGCode == 0 )
            {
                WCTSEmitServiceException( "Failed to decode CoordinateReferenceSystem with missing,\n"
                                          "or zero crsID.code" );
            }								

            CPLErrorReset();
            if( oSRS.importFromEPSG( nEPSGCode ) != OGRERR_NONE )
            {
                if( strlen(CPLGetLastErrorMsg()) > 0 )
                    WCTSEmitServiceException( CPLGetLastErrorMsg() );
                else
                    WCTSEmitServiceException( 
                        CPLSPrintf( "OGRSpatialReference::importFromEPSG(%d) "
                                    "failed.  Is this a defined EPSG code?", 
                                    nEPSGCode ) );
            }
        }

/* -------------------------------------------------------------------- */
/*      Handle AUTO case.                                               */
/* -------------------------------------------------------------------- */
        else if( EQUAL(pszCodeSpace,"AUTO") )
        {
            if( oSRS.importFromWMSAUTO( pszCode ) != OGRERR_NONE )
            {
                if( strlen(CPLGetLastErrorMsg()) > 0 )
                    WCTSEmitServiceException( CPLGetLastErrorMsg() );
                else
                    WCTSEmitServiceException( 
                        CPLSPrintf( "OGRSpatialReference::importFromWMSAUTO(%s) "
                                    "failed.  Is this a defined EPSG code?", 
                                    pszCode  ) );
            }
        }

/* -------------------------------------------------------------------- */
/*      Otherwise blow a gasket.                                        */
/* -------------------------------------------------------------------- */
        else
        {
            WCTSEmitServiceException( "Failed to decode CoordinateReferenceSystem with missing,\n"
                                      "or non-EPSG crsID.codeSpace" );
        }	
        
/* -------------------------------------------------------------------- */
/*      Translate into an OGRSpatialReference from EPSG code.           */
/* -------------------------------------------------------------------- */

        return oSRS.Clone();
    }

/* ==================================================================== */
/*      Try to import a projectedCRS or geographicCRS.                  */
/* ==================================================================== */
    if( CPLGetXMLNode( psXMLCRS, "ProjectedCRS" ) != NULL 
        || CPLGetXMLNode( psXMLCRS, "GeographicCRS" ) != NULL )
    {
#ifdef DISABLE_USER_DEFINED_CRS
        WCTSEmitServiceException( 
            "User defined ProjectedCRS and GeographicCRS support\n"
            "disabled for security reasons." );
#else
        char *pszSerializedForm;
        OGRSpatialReference oSRS;

        pszSerializedForm = CPLSerializeXMLTree( psXMLCRS->psChild );
        if( oSRS.importFromXML( pszSerializedForm ) != OGRERR_NONE )
        {
            CPLFree( pszSerializedForm );
            if( strlen(CPLGetLastErrorMsg()) > 0 )
                WCTSEmitServiceException( CPLGetLastErrorMsg() );
            else
                WCTSEmitServiceException( "Failed to import CRS" );
        }

        CPLFree( pszSerializedForm );
        return oSRS.Clone();
#endif
    }
    
/* -------------------------------------------------------------------- */
/*      We don't seem to recognise a CRS here.                          */
/* -------------------------------------------------------------------- */
    WCTSEmitServiceException( "Unable to identify CRS in one of SourceCRS or TargetCRS elements" );

    return NULL;
}

/************************************************************************/
/*                        WCTSIsTransformable()                         */
/************************************************************************/

void WCTSIsTransformable( CPLXMLNode *psOperation )

{
    OGRSpatialReference *poSrcCRS, *poDstCRS;
    CPLXMLNode *psSrcXMLCRS, *psDstXMLCRS;

/* -------------------------------------------------------------------- */
/*      Translate the source CRS.                                       */
/* -------------------------------------------------------------------- */
    psSrcXMLCRS = CPLGetXMLNode( psOperation, "SourceCRS" );

    if( psSrcXMLCRS == NULL )
        WCTSEmitServiceException( "Unable to identify SourceCRS.CoordinateReferenceSystem" );

    poSrcCRS = WCTSImportCoordinateReferenceSystem( psSrcXMLCRS );

/* -------------------------------------------------------------------- */
/*      Translate the destination CRS.                                  */
/* -------------------------------------------------------------------- */
    psDstXMLCRS = CPLGetXMLNode( psOperation, "TargetCRS" );

    if( psDstXMLCRS == NULL )
        WCTSEmitServiceException( "Unable to identify DestinationCRS.CoordinateReferenceSystem" );

    poDstCRS = WCTSImportCoordinateReferenceSystem( psDstXMLCRS );

/* -------------------------------------------------------------------- */
/*      Create a transformation object between the coordinate           */
/*      systems as an added step of verification that they are          */
/*      supported.                                                      */
/* -------------------------------------------------------------------- */
    OGRCoordinateTransformation *poCT;
    const char *pszResult;

    poCT = OGRCreateCoordinateTransformation( poSrcCRS, poDstCRS );
    if( poCT == NULL )
        pszResult = "false";
    else
    {
        delete poCT;
        pszResult = "true";
    }

    delete poSrcCRS;
    delete poDstCRS;

/* -------------------------------------------------------------------- */
/*      Return the answer.                                              */
/* -------------------------------------------------------------------- */
    printf( "Content-type: text/xml%c%c", 10, 10 );

    printf( "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" );
    printf( "<TransformableResponse xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"http://www.deegree.org/xml/schemas/wcts/transformableResponse.xsd\" transformable=\"%s\"/>\n", 
            pszResult );

    exit( 0 );
}

/************************************************************************/
/*                       WCTSIsGeometryElement()                        */
/************************************************************************/

int WCTSIsGeometryElement( CPLXMLNode *psNode )

{
    if( psNode->eType != CXT_Element )
        return FALSE;
    
    const char *pszElement = psNode->pszValue;
    
    if( EQUALN(pszElement,"gml:",4) )
        pszElement += 4;

    return EQUAL(pszElement,"Polygon") 
        || EQUAL(pszElement,"MultiPolygon") 
        || EQUAL(pszElement,"MultiPoint") 
        || EQUAL(pszElement,"MultiLineString") 
        || EQUAL(pszElement,"GeometryCollection") 
        || EQUAL(pszElement,"Point") 
        || EQUAL(pszElement,"Box")
        || EQUAL(pszElement,"LineString");
}
    
/************************************************************************/
/*                      WCTSRecurseAndTransform()                       */
/*                                                                      */
/*      Recurse down a XML document tree that contains some GML         */
/*      geometries.  Identify them, convert them into OGRGeometries,    */
/*      transform these, convert back to GML, insert in place of old    */
/*      geometry fragments, and continue on.                            */
/************************************************************************/

void WCTSRecurseAndTransform( CPLXMLNode *psTree, 
                              OGRCoordinateTransformation *poCT )

{
    if( psTree == NULL )
        return;

/* -------------------------------------------------------------------- */
/*      If this isn't a geometry mode just recurse.                     */
/* -------------------------------------------------------------------- */
    if( !WCTSIsGeometryElement( psTree ) )
    {
        WCTSRecurseAndTransform( psTree->psChild, poCT );
        WCTSRecurseAndTransform( psTree->psNext, poCT );
        return;
    }
    
/* -------------------------------------------------------------------- */
/*      Convert this node, and it's children (but not it's sibling)     */
/*      into serialized XML form for feeding to the GML geometry        */
/*      parser.                                                         */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psNext = psTree->psNext;
    OGRGeometry *poGeometry;

    psTree->psNext = NULL;
    poGeometry = (OGRGeometry *) OGR_G_CreateFromGMLTree( psTree );
    psTree->psNext = psNext;

    if( poGeometry == NULL )
    {
        /* should we raise an exception?  For now, no.*/
        WCTSRecurseAndTransform( psTree->psNext, poCT );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Attempt to transform the geometry (inplace).                    */
/* -------------------------------------------------------------------- */
    if( poGeometry->transform( poCT ) != OGRERR_NONE )
        WCTSEmitServiceException( "Unable to transform some geometries." );

/* -------------------------------------------------------------------- */
/*      Convert back to XML Tree format.                                */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psAltered, sTempCopy;

    if( strstr(psTree->pszValue,"Box") == NULL )
        psAltered = OGR_G_ExportToGMLTree( (OGRGeometryH) poGeometry );
    else
        psAltered = OGR_G_ExportEnvelopeToGMLTree( (OGRGeometryH) poGeometry );

    OGRGeometryFactory::destroyGeometry( poGeometry );
    
/* -------------------------------------------------------------------- */
/*      do fancy swap to copy contents of altered tree in over the      */
/*      node being changed.  We do this in such a funky way because     */
/*      we can't change the nodes that point to psTree to point to      */
/*      psAltered.                                                      */
/* -------------------------------------------------------------------- */
    CPLAssert( psAltered->psNext == NULL );

    memcpy( &sTempCopy, psTree, sizeof(CPLXMLNode));
    memcpy( psTree, psAltered, sizeof(CPLXMLNode));
    memcpy( psAltered, &sTempCopy, sizeof(CPLXMLNode));

    psTree->psNext = psAltered->psNext;
    psAltered->psNext = NULL;

    CPLDestroyXMLNode( psAltered );
    
/* -------------------------------------------------------------------- */
/*      Continue on to sibling nodes, but do no further travelling      */
/*      to this nodes children.                                         */
/* -------------------------------------------------------------------- */
    WCTSRecurseAndTransform( psTree->psNext, poCT );
}

/************************************************************************/
/*                            WCTSGetData()                             */
/*                                                                      */
/*      Fetch the data component as a parsed XML tree.  In some         */
/*      cases the data contents are local, in other cases they have     */
/*      to be fetched from a remote tree.                               */
/*                                                                      */
/*      The argument passed in is the <Data> element.  If it has a      */
/*      FileURL child that child is replaced by the actual instance.    */
/************************************************************************/

void WCTSGetData( CPLXMLNode * psData )

{
    CPLAssert( psData != NULL && psData->eType == CXT_Element
               && EQUAL(psData->pszValue,"Data") );

/* ==================================================================== */
/*      Handle a FileURL.                                               */
/* ==================================================================== */
    if( psData->psChild != NULL 
        && EQUAL(psData->psChild->pszValue,"FileURL")
        && psData->psChild->eType == CXT_Element 
        && psData->psChild->psChild != NULL
        && psData->psChild->psChild->eType == CXT_Text )
    {
        CPLXMLNode *psNewDataTree;
        char *pszData;

        if( !EQUALN(psData->psChild->psChild->pszValue,"http:",5) 
            && !EQUALN(psData->psChild->psChild->pszValue,"https:",6) 
            && !EQUALN(psData->psChild->psChild->pszValue,"ftp:",4) )
            
        {
            WCTSEmitServiceException( 
                "Use of FileURL with protocol other than http, https or ftp\n"
                "not supported for security reasons." );
        }

        pszData = WCTSHTTPFetch( psData->psChild->psChild->pszValue );

        psNewDataTree = CPLParseXMLString( pszData );
        if( psNewDataTree == NULL )
        {
            if( strlen(CPLGetLastErrorMsg()) > 0 )
                WCTSEmitServiceException( CPLGetLastErrorMsg() );
            else
                WCTSEmitServiceException( "Failing parsing GML fetched from FileURL." );
        }

        CPLFree( pszData );

        /* discard special prefix line if present */
        if( psNewDataTree->eType == CXT_Literal 
            || (psNewDataTree->eType == CXT_Element
                && EQUALN(psNewDataTree->pszValue,"?",1) ) )
        {
            CPLXMLNode *psNext = psNewDataTree->psNext;
            psNewDataTree->psNext = NULL;
            CPLDestroyXMLNode( psNewDataTree );
            psNewDataTree = psNext;
        }
        
        /* substitute this tree in place of the FileURL */
        CPLDestroyXMLNode( psData->psChild );
        psData->psChild = psNewDataTree;

        return;
    }

/* ==================================================================== */
/*      Otherwise, no change required.                                  */
/* ==================================================================== */
    return;
}

/************************************************************************/
/*                           WCTSTransform()                            */
/************************************************************************/

void WCTSTransform( CPLXMLNode *psOperation )

{
    OGRSpatialReference *poSrcCRS, *poDstCRS;
    CPLXMLNode *psSrcXMLCRS, *psDstXMLCRS;

/* -------------------------------------------------------------------- */
/*      Translate the source CRS.                                       */
/* -------------------------------------------------------------------- */
    psSrcXMLCRS = CPLGetXMLNode( psOperation, "SourceCRS" );

    if( psSrcXMLCRS == NULL )
        WCTSEmitServiceException( "Unable to identify SourceCRS.CoordinateReferenceSystem" );

    poSrcCRS = WCTSImportCoordinateReferenceSystem( psSrcXMLCRS );

/* -------------------------------------------------------------------- */
/*      Translate the destination CRS.                                  */
/* -------------------------------------------------------------------- */
    psDstXMLCRS = CPLGetXMLNode( psOperation, "TargetCRS" );

    if( psDstXMLCRS == NULL )
        WCTSEmitServiceException( "Unable to identify DestinationCRS.CoordinateReferenceSystem" );

    poDstCRS = WCTSImportCoordinateReferenceSystem( psDstXMLCRS );

/* -------------------------------------------------------------------- */
/*      Create the coordinate transformation object.                    */
/* -------------------------------------------------------------------- */
    OGRCoordinateTransformation *poCT;

    poCT = OGRCreateCoordinateTransformation( poSrcCRS, poDstCRS );
    if( poCT == NULL )
        WCTSEmitServiceException( "Unable to transform between source and destination CRSs." );

    delete poSrcCRS;
    delete poDstCRS;

/* -------------------------------------------------------------------- */
/*      We will recurse over the GML data tree looking for segments     */
/*      that are recognizably geometries to be transformed in place.    */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psData = CPLGetXMLNode(psOperation,"Data");

    if( psData == NULL )
        WCTSEmitServiceException( "Unable to find GML Data contents." );

    WCTSGetData( psData );
    WCTSRecurseAndTransform( psData, poCT );

/* -------------------------------------------------------------------- */
/*      Now translate the data back into a serialized form suitable     */
/*      for including in the reply.                                     */
/* -------------------------------------------------------------------- */
    char *pszDataText = CPLSerializeXMLTree( psData );

/* -------------------------------------------------------------------- */
/*      Return result.                                                  */
/* -------------------------------------------------------------------- */
    printf( "Content-type: text/xml%c%c", 10, 10 );

    printf( "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" );
    printf( "<TransformResponse xmlns:gml=\"http://www.opengis.net/gml\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" >\n" );
    fwrite( pszDataText, 1, strlen(pszDataText), stdout );
    printf( "</TransformResponse>\n" );

    exit( 0 );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char ** papszArgv )

{
    RegisterOGRGML();

/* -------------------------------------------------------------------- */
/*      Process any configuration switches.                             */
/* -------------------------------------------------------------------- */
    int iArg;

    for( iArg = 1; iArg < nArgc; iArg++ )
    {
        if( EQUAL(papszArgv[iArg],"-log") && iArg < nArgc-1 )
        {
            char *pszLogEnv = (char *) CPLMalloc(strlen(papszArgv[iArg+1])+20);

            sprintf( pszLogEnv, "CPL_LOG=%s", papszArgv[iArg+1] );
            putenv( pszLogEnv );

            iArg++;
        }
        else if( EQUAL(papszArgv[iArg],"-debug") )
        {
            putenv( "CPL_DEBUG=ON" );
            putenv( "PROJ_DEBUG=ON" );
        }
        else if( EQUAL(papszArgv[iArg],"-data")  && iArg < nArgc-1 )
        {
            CPLPushFinderLocation( papszArgv[++iArg] );
        }
        else if( EQUAL(papszArgv[iArg],"-put") )
        {
            putenv( "REQUEST_METHOD=PUT" );
        }
        else if( EQUAL(papszArgv[iArg],"-get")  && iArg < nArgc-1 )
        {
            char *pszLogEnv = (char *) CPLMalloc(strlen(papszArgv[iArg+1])+20);

            sprintf( pszLogEnv, "QUERY_STRING=%s", papszArgv[iArg+1] );
            putenv( pszLogEnv );
            putenv( "REQUEST_METHOD=GET" );

            iArg++;
        }
        else
        {
            WCTSEmitServiceException( 
               "Server misconfigured, unknown commandline options received.\n"
               "\n"
               "Usage: ogrwcts [-log logfilename] [-debug] [-data directory]\n"
               "               [-get url] [-put]\n" );
        }
    }

/* -------------------------------------------------------------------- */
/*      Collect the request as a parsed XML document.                   */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psRequest;

    psRequest = WCTSCollectRequest();

/* -------------------------------------------------------------------- */
/*      Scan for known operation nodes.                                 */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psOperation;

    for( psOperation = psRequest; 
         psOperation != NULL; 
         psOperation = psOperation->psNext )
    {
        if( psOperation->eType == CXT_Element
            && EQUAL(psOperation->pszValue,"GetCapabilities") )
        {
            WCTSGetCapabilities( psOperation );
            assert( FALSE );
        }
        else if( psOperation->eType == CXT_Element
            && EQUAL(psOperation->pszValue,"IsTransformable") )
        {
            WCTSIsTransformable( psOperation );
            assert( FALSE );
        }
        else if( psOperation->eType == CXT_Element
            && EQUAL(psOperation->pszValue,"Transform") )
        {
            WCTSTransform( psOperation );
            assert( FALSE );
        }
        else if( psOperation->eType == CXT_Element
            && EQUAL(psOperation->pszValue,"DescribeTransformation") )
        {
            WCTSEmitServiceException( "This server does not support the DescribeTransformation operation." );
        }
    }

    CPLDestroyXMLNode( psRequest );

    WCTSEmitServiceException( "No recognisable supported request found." );
    exit( 1 );
}

