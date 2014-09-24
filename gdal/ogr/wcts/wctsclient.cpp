/******************************************************************************
 * $Id$
 *
 * Project:  Web Coordinate Transformation Service
 * Purpose:  cgi-bin client form processor.  Turns client form request into
 *           a WCTS request, and returns the result to the client in HTML.
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
 ****************************************************************************/

#include <assert.h>
#include <curl/curl.h>
#include "cpl_minixml.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                   WCTSClientEmitServiceException()                   */
/************************************************************************/

static void WCTSClientEmitServiceException( const char * pszMessage )

{
    printf("Content-type: text/html%c%c",10,10);

    printf("<html><title>WCTS Client Error</title><body>\n" );
    printf("<h1>WCTS Client Error</h1>\n" );
    printf("%s\n", pszMessage );
    printf("</html>\n" );

    exit( 1 );
}

/************************************************************************/
/*                        WCTSClientReturnXML()                         */
/*                                                                      */
/*      Return an XML document literally but properly typed.            */
/************************************************************************/

static void WCTSClientReturnXML( const char *pszXML )

{
    if( EQUALN(pszXML,"<?xml",5) )
        printf("Content-type: text/xml%c%c",10,10);
    else if( strstr(pszXML,"<HTML") != NULL
             || strstr(pszXML,"<html") != NULL )
        printf("Content-type: text/html%c%c",10,10);
    else
        printf("Content-type: text/plain%c%c",10,10);

    printf("%s", pszXML );
    
    exit( 0 );
}

/************************************************************************/
/*                         WCTSClientWriteFct()                         */
/*                                                                      */
/*      Append incoming text to our collection buffer, reallocating     */
/*      it larger as needed.                                            */
/************************************************************************/

static size_t 
WCTSClientWriteFct(void *buffer, size_t size, size_t nmemb, void *reqInfo)

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
/*                        WCTSClientHTTPFetch()                         */
/*                                                                      */
/*      Fetch a document from an url and return in a string.            */
/************************************************************************/

static char *WCTSClientHTTPFetch( const char *pszURL, const char *pszPostDoc )

{
    CURL *http_handle;
    char *pszData = NULL;
    char szCurlErrBuf[CURL_ERROR_SIZE+1];
    CURLcode error;

    CPLDebug( "WCTSCLIENT", "HTTP Fetch: %s", pszURL );

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
    curl_easy_setopt(http_handle, CURLOPT_WRITEFUNCTION, WCTSClientWriteFct );

    if( pszPostDoc != NULL )
    {
        curl_easy_setopt(http_handle, CURLOPT_POST, 1 );
        curl_easy_setopt(http_handle, CURLOPT_POSTFIELDS, pszPostDoc );
    }

    szCurlErrBuf[0] = '\0';

    curl_easy_setopt(http_handle, CURLOPT_ERRORBUFFER, szCurlErrBuf );

    error = curl_easy_perform( http_handle );

    curl_easy_cleanup( http_handle );

    if( strlen(szCurlErrBuf) > 0 )
        WCTSClientEmitServiceException( szCurlErrBuf );
    else if( pszData == NULL )
        WCTSClientEmitServiceException( "No response from WCTS server." );


    return pszData;
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char ** papszArgv )

{
    putenv( "CPL_LOG=/tmp/wctsclient.log" );
    putenv( "CPL_DEBUG=ON" );

/* ==================================================================== */
/*      Collect client request.                                         */
/* ==================================================================== */
    char **papszParmList = NULL;

/* -------------------------------------------------------------------- */
/*      Parse the query string.                                         */
/* -------------------------------------------------------------------- */
    if( getenv("REQUEST_METHOD") == NULL
        || !EQUAL(getenv("REQUEST_METHOD"),"POST") )
    {
        if( getenv("QUERY_STRING") == NULL )
            WCTSClientEmitServiceException( "QUERY_STRING not set." );
        
        papszParmList = CSLTokenizeString2( getenv("QUERY_STRING"), "&",
                                            CSLT_PRESERVEESCAPES );
    }

/* -------------------------------------------------------------------- */
/*      Or parse the urlencoded text in the POST body (stdin)           */
/* -------------------------------------------------------------------- */
    else
    {
        int nContentLength = 0;
        char *pszBody = NULL;
        
        if( getenv("CONTENT_LENGTH") != NULL )
        {
            nContentLength = atoi(getenv("CONTENT_LENGTH"));
            
            pszBody = (char *) CPLMalloc(nContentLength+1);
            
            if( (int) fread(pszBody, 1, nContentLength, stdin) < nContentLength )
                WCTSClientEmitServiceException( "POST body is short." );
            
            pszBody[nContentLength] = '\0';
        }
        
        else
        {
            int nBodyMax, nBodyLen=0;
            
            nBodyMax = 100;
            pszBody = (char *) CPLMalloc(nBodyMax);
            
            while( !feof(stdin) )
            {
                pszBody[nBodyLen++] = fgetc(stdin);
                if( nBodyLen == nBodyMax )
                {
                    nBodyMax = nBodyMax * 2;
                    pszBody = (char *) CPLRealloc(pszBody, nBodyMax);
                }
            }
            
            pszBody[nBodyLen] = '\0';
        }

        papszParmList = CSLTokenizeString2( pszBody, "&",
                                            CSLT_PRESERVEESCAPES );
    }
    
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
/*	Fetch and default arguments.					*/
/* -------------------------------------------------------------------- */
    const char *pszRequest = CSLFetchNameValue(papszParmList,"Request");
    const char *pszSourceCRS = CSLFetchNameValue(papszParmList,"SourceCRS");
    const char *pszTargetCRS = CSLFetchNameValue(papszParmList,"TargetCRS");
    const char *pszInputX = CSLFetchNameValue(papszParmList,"InputX");
    const char *pszInputY = CSLFetchNameValue(papszParmList,"InputY");
    const char *pszGMLURL = CSLFetchNameValue(papszParmList,"GMLURL");
    const char *pszGMLData = CSLFetchNameValue(papszParmList,"GMLDATA");
    const char *pszServer = CSLFetchNameValue(papszParmList,"WCTSServer");

    if( pszRequest == NULL )
        pszRequest = "Transform";

    if( pszServer == NULL )
        WCTSClientEmitServiceException( "WCTS Server not selected." );

    CPLDebug( "WCTSCLIENT", "Request=%s", pszRequest );
    CPLDebug( "WCTSCLIENT", "Server=%s", pszServer );

/* ==================================================================== */
/*      Handle a GetCapabilities request.                               */
/* ==================================================================== */
    if( EQUAL( pszRequest, "GetCapabilities" ) )
    {
        char *pszURL;
        char *pszCapXML;

        pszURL = CPLStrdup(
            CPLSPrintf( "%s?REQUEST=GetCapabilities&Service=WCTS", 
                        pszServer ));

        pszCapXML = WCTSClientHTTPFetch( pszURL, NULL );
        WCTSClientReturnXML( pszCapXML );
    }

/* ==================================================================== */
/*      Handle IsTransformable request.                                 */
/* ==================================================================== */
    if( EQUAL( pszRequest, "IsTransformable" ) )
    {
        char *pszURL;
        char *pszResultXML;

        pszURL = CPLStrdup(
            CPLSPrintf( "%s?REQUEST=IsTransformable&Service=WCTS"
                        "&SourceCRS=EPSG:%s&TargetCRS=EPSG:%s", 
                        pszServer, pszSourceCRS, pszTargetCRS ));

        pszResultXML = WCTSClientHTTPFetch( pszURL, NULL );
        WCTSClientReturnXML( pszResultXML );
    }

/* ==================================================================== */
/*      Handle DescribeTransformation request.                          */
/* ==================================================================== */
    if( EQUAL( pszRequest, "DescribeTransformation" ) )
    {
        char *pszURL;
        char *pszResultXML;

        pszURL = CPLStrdup(
            CPLSPrintf( "%s?REQUEST=DescribeTransformation&Service=WCTS"
                        "&SourceCRS=EPSG:%s&TargetCRS=EPSG:%s", 
                        pszServer, pszSourceCRS, pszTargetCRS ));

        pszResultXML = WCTSClientHTTPFetch( pszURL, NULL );
        WCTSClientReturnXML( pszResultXML );
    }

/* ==================================================================== */
/*      Handle Transform request for a single point provided in the     */
/*      form.                                                           */
/* ==================================================================== */
    if( EQUAL( pszRequest, "Transform" ) && pszGMLURL != NULL
        && strlen(pszGMLURL) != 0 )
    {
/* -------------------------------------------------------------------- */
/*      Prepare request.                                                */
/* -------------------------------------------------------------------- */
        char szReqDoc[10000];

        sprintf( szReqDoc, 
"<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>\n"
"<Transform xmlns=\"http://schemas.opengis.net/wcts\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:gml=\"http://www.opengis.net/gml\" version=\"0.1.0\">\n"
"  <SourceCRS>\n"
"    <crsID>\n"
"      <gml:code>%s</gml:code>\n"
"      <gml:codeSpace>EPSG</gml:codeSpace>\n"
"    </crsID>\n"
"  </SourceCRS>\n"
"  <TargetCRS>\n"
"    <crsID>\n"
"      <gml:code>%s</gml:code>\n"
"      <gml:codeSpace>EPSG</gml:codeSpace>\n"
"    </crsID>\n"
"  </TargetCRS>\n"
"  <Data>\n"
"    <FileURL>%s</FileURL>\n" 
"  </Data>\n"
"</Transform>\n",
                 pszSourceCRS, pszTargetCRS, pszGMLURL );

/* -------------------------------------------------------------------- */
/*      Invoke Service.                                                 */
/* -------------------------------------------------------------------- */
        char *pszResultXML;

        pszResultXML = WCTSClientHTTPFetch( pszServer, szReqDoc );

/* -------------------------------------------------------------------- */
/*      Display result.                                                 */
/* -------------------------------------------------------------------- */
        WCTSClientReturnXML( pszResultXML );
    }

/* ==================================================================== */
/*      Handle Transform request for a single point provided in the     */
/*      form.                                                           */
/* ==================================================================== */
    if( EQUAL( pszRequest, "Transform" ) && pszGMLData != NULL
        && strlen(pszGMLData) != 0 )
    {
        
/* -------------------------------------------------------------------- */
/*      Skip past any <?xml> element.                                   */
/* -------------------------------------------------------------------- */
        if( EQUALN(pszGMLData,"<?xml", 5) )
        {
            
            while( *pszGMLData != '\0' && !EQUALN(pszGMLData,"?>",2) )
                pszGMLData++;

            if( EQUALN(pszGMLData,"?>",2) )
                pszGMLData += 2;
        }
        
/* -------------------------------------------------------------------- */
/*      Prepare request.                                                */
/* -------------------------------------------------------------------- */
        char *pszReqDoc;

        pszReqDoc = (char *) CPLMalloc(strlen(pszGMLData) + 10000);

        sprintf( pszReqDoc, 
"<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>\n"
"<Transform xmlns=\"http://schemas.opengis.net/wcts\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:gml=\"http://www.opengis.net/gml\" version=\"0.1.0\">\n"
"  <SourceCRS>\n"
"    <crsID>\n"
"      <gml:code>%s</gml:code>\n"
"      <gml:codeSpace>EPSG</gml:codeSpace>\n"
"    </crsID>\n"
"  </SourceCRS>\n"
"  <TargetCRS>\n"
"    <crsID>\n"
"      <gml:code>%s</gml:code>\n"
"      <gml:codeSpace>EPSG</gml:codeSpace>\n"
"    </crsID>\n"
"  </TargetCRS>\n"
"  <Data>\n"
"%s\n" 
"  </Data>\n"
"</Transform>\n",
                 pszSourceCRS, pszTargetCRS, pszGMLData );

/* -------------------------------------------------------------------- */
/*      Invoke Service.                                                 */
/* -------------------------------------------------------------------- */
        char *pszResultXML;

        pszResultXML = WCTSClientHTTPFetch( pszServer, pszReqDoc );
        CPLFree( pszReqDoc );

/* -------------------------------------------------------------------- */
/*      Display result.                                                 */
/* -------------------------------------------------------------------- */
        WCTSClientReturnXML( pszResultXML );
    }

/* ==================================================================== */
/*      Handle Transform request for a single point provided in the     */
/*      form.                                                           */
/* ==================================================================== */
    if( EQUAL( pszRequest, "Transform" ) )
    {
        char *pszResultXML;

        if( pszInputX == NULL || pszInputY == NULL
            || strlen(pszInputX) == 0 || strlen(pszInputY) == 0 )
            WCTSClientEmitServiceException( "InputX or InputY missing or empty" );

/* -------------------------------------------------------------------- */
/*      Prepare an XML document representing the transformation to      */
/*      be executed.                                                    */
/* -------------------------------------------------------------------- */
        char szReqDoc[10000];

        sprintf( szReqDoc, 
"<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>\n"
"<Transform xmlns=\"http://schemas.opengis.net/wcts\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:gml=\"http://www.opengis.net/gml\" version=\"0.1.0\">\n"
"  <SourceCRS>\n"
"    <crsID>\n"
"      <gml:code>%s</gml:code>\n"
"      <gml:codeSpace>EPSG</gml:codeSpace>\n"
"    </crsID>\n"
"  </SourceCRS>\n"
"  <TargetCRS>\n"
"    <crsID>\n"
"      <gml:code>%s</gml:code>\n"
"      <gml:codeSpace>EPSG</gml:codeSpace>\n"
"    </crsID>\n"
"  </TargetCRS>\n"
"  <Data>\n"
"    <TrFeature fid=\"0\">\n"
"      <gml:geometryProperty>\n"
"        <gml:Point>\n"
"          <gml:coordinates>%s,%s</gml:coordinates>\n"
"        </gml:Point>\n"
"      </gml:geometryProperty>\n"
"    </TrFeature>\n"
"  </Data>\n"
"</Transform>\n",
                 pszSourceCRS, pszTargetCRS, pszInputX, pszInputY );

/* -------------------------------------------------------------------- */
/*      Invoke Service.                                                 */
/* -------------------------------------------------------------------- */
        pszResultXML = WCTSClientHTTPFetch( pszServer, szReqDoc );

/* -------------------------------------------------------------------- */
/*      Display result.                                                 */
/* -------------------------------------------------------------------- */
        char *pszCoord = strstr(pszResultXML,"<gml:coordinates>");

        if( pszCoord != NULL )
        {
            pszCoord += 17;
            char *pszEnd = pszCoord;
            while( *pszEnd != '\0' && *pszEnd != '<' )
                pszEnd++;

            *pszEnd = '\0';

            printf( "Content-type: text/html\n\n" );
            printf( "<html><body>\n" );
            printf( "Transformed coordinate: <b>%s</b>", pszCoord );
            printf( "<body></html>\n" );
            exit( 0 );
        }
        else
        {
            WCTSClientReturnXML( pszResultXML );
        }
    }

/* ==================================================================== */
/*      No request match.                                               */
/* ==================================================================== */
    WCTSClientEmitServiceException( 
        CPLSPrintf( "REQUEST=%s not supported.", pszRequest ) );
}

/*
REQUEST_URI="/cgi-bin/printenv?H=1"
SERVER_NAME="gdal.velocet.ca"
*/
