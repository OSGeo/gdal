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
 * $Log$
 * Revision 1.7  2003/03/12 20:51:39  warmerda
 * integrated special handling for bounding box
 *
 * Revision 1.6  2003/03/11 21:48:38  warmerda
 * Fixed test/xml to be text/xml.
 *
 * Revision 1.5  2003/03/11 21:32:09  warmerda
 * Added preliminary KVP support
 *
 * Revision 1.4  2003/03/11 17:28:49  warmerda
 * Changed where we look for capabilities.
 *
 * Revision 1.3  2003/03/11 15:40:44  warmerda
 * initial minimally working implementation
 *
 * Revision 1.2  2003/03/05 22:11:10  warmerda
 * implement istransformable
 *
 * Revision 1.1  2003/03/05 21:07:31  warmerda
 * New
 *
 */

#include <assert.h>
#include "cpl_minixml.h"
#include "ogr_api.h"
#include "ogrsf_frmts.h"
#include "cpl_conv.h"
#include "cpl_string.h"

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
            CPLSPrintf( "%s value corrupt, use 'authority:code'.",
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
        WCTSEmitServiceException( "Unrecognised REQUEST value." );

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
            CPLSPrintf( "Attempt to GetCapabilities for unsupported '%s'\n"
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
        WCTSEmitServiceException( "WCTS server misconfigured, unable to find capabilities document." );
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
    int nEPSGCode;

/* -------------------------------------------------------------------- */
/*      Get the EPSG code, and verify that it is in the EPSG            */
/*      codeSpace.                                                      */
/* -------------------------------------------------------------------- */
    if( !EQUAL(CPLGetXMLValue( psXMLCRS, "crsID.gml:codeSpace", "" ), "EPSG"))
    {
        WCTSEmitServiceException( "Failed to decode CoordinateReferenceSystem with missing,\n"
                                  "or non-EPSG crsID.gml:codeSpace" );
    }	

    nEPSGCode = atoi(CPLGetXMLValue( psXMLCRS, "crsID.gml:code", "0" ));

    if( nEPSGCode == 0 )
    {
        WCTSEmitServiceException( "Failed to decode CoordinateReferenceSystem with missing,\n"
                                  "or zero crsID.gml:code" );
    }								

/* -------------------------------------------------------------------- */
/*      Translate into an OGRSpatialReference from EPSG code.           */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS;

    CPLErrorReset();
    if( oSRS.importFromEPSG( nEPSGCode ) != OGRERR_NONE )
    {
        if( strlen(CPLGetLastErrorMsg()) > 0 )
            WCTSEmitServiceException( CPLGetLastErrorMsg() );
        else
            WCTSEmitServiceException( 
                CPLSPrintf( "OGRSpatialReference::importFromEPSG(%d) failed.  Is this a defined EPSG code?", 
                            nEPSGCode ) );
    }

/* -------------------------------------------------------------------- */
/*      Return SRS.                                                     */
/* -------------------------------------------------------------------- */
    return oSRS.Clone();
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
                "Usage: ogrwcts [-log logfilename] [-data directory]\n"
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

    WCTSEmitServiceException( "No recognisable supported request found." );
    exit( 1 );
}

