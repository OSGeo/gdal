/******************************************************************************
 * $Id$
 *
 * Project:  Common Portability Library
 * Purpose:  Google OAuth2 Authentication Services
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2013, Frank Warmerdam 
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

#include "cpl_http.h"

CPL_CVSID("$Id$");

/* ==================================================================== */
/*      Values related to OAuth2 authorization to use fusion            */
/*      tables.  Many of these values are related to the                */
/*      gdalautotest@gmail.com account for GDAL managed by Even         */
/*      Rouault and Frank Warmerdam.  Some information about OAuth2     */
/*      as managed by that account can be found at the following url    */
/*      when logged in as gdalautotest@gmail.com:                       */
/*  https://code.google.com/apis/console/#project:265656308688:access   */
/* ==================================================================== */
#define GDAL_CLIENT_ID "265656308688.apps.googleusercontent.com"
#define GDAL_CLIENT_SECRET "0IbTUDOYzaL6vnIdWTuQnvLz"
#define GDAL_API_KEY "AIzaSyA_2h1_wXMOLHNSVeo-jf1ACME-M1XMgP0"

#define GOOGLE_AUTH_URL "https://accounts.google.com/o/oauth2/token"
#define FUSION_TABLE_SCOPE "https%3A%2F%2Fwww.googleapis.com%2Fauth%2Ffusiontables"

#define AUTH_TOKEN_REQUEST "https://accounts.google.com/o/oauth2/auth?scope=" FUSION_TABLE_SCOPE "&state=%2Fprofile&redirect_uri=urn:ietf:wg:oauth:2.0:oob&response_type=code&client_id=" GDAL_CLIENT_ID

/************************************************************************/
/*                          ParseSimpleJson()                           */
/*                                                                      */
/*      Return a string list of name/value pairs extracted from a       */
/*      JSON doc.  The Google OAuth2 web service returns simple JSON    */
/*      responses.  The parsing as done currently is very fragile       */
/*      and depends on JSON documents being in a very very simple       */
/*      form.                                                           */
/************************************************************************/

static CPLStringList ParseSimpleJson(const char *pszJson)

{
/* -------------------------------------------------------------------- */
/*      We are expecting simple documents like the following with no    */
/*      heirarchy or complex structure.                                 */
/* -------------------------------------------------------------------- */
/*    
    {
        "access_token":"1/fFBGRNJru1FQd44AzqT3Zg",
        "expires_in":3920,
        "token_type":"Bearer"
    }
*/

    CPLStringList oWords(
        CSLTokenizeString2(pszJson, " \n\t,:{}", CSLT_HONOURSTRINGS ));
    CPLStringList oNameValue;

    for( int i=0; i < oWords.size(); i += 2 )
    {
        oNameValue.SetNameValue( oWords[i], oWords[i+1] );
    }
    
    return oNameValue;
}

/************************************************************************/
/*                      GOA2GetAuthorizationURL()                       */
/************************************************************************/

/**
 * Return authorization url for a given scope. 
 *
 * Returns the URL that a user should visit, and use for authentication
 * in order to get an "auth token" indicating their willingness to use a 
 * service. 
 *
 * Note that when the user visits this url they will be asked to login
 * (using a google/gmail/etc) account, and to authorize use of the 
 * requested scope for the application "GDAL/OGR".  Once they have done 
 * so, they will be presented with a lengthy string they should "enter
 * into their application".  This is the "auth token" to be passed to
 * GOA2GetRefreshToken().  The "auth token" can only be used once. 
 * 
 * This function should never fail. 
 *
 * @param pszScope the service being requested, not yet URL encoded, such as 
 * "https://www.googleapis.com/auth/fusiontables". 
 * 
 * @return the URL to visit - should be freed with CPLFree().
 */

char *GOA2GetAuthorizationURL(const char *pszScope)

{
    // Eventually we should allow application to provide their own
    // client id, and client secret via configuration options or even 
    // possibly compile time macros. 
    return CPLStrdup(AUTH_TOKEN_REQUEST);
}

/************************************************************************/
/*                        GOA2GetRefreshToken()                         */
/************************************************************************/

/**
 * Turn Auth Token into a Refresh Token. 
 * 
 * A one time "auth token" provided by the user is turned into a 
 * reusable "refresh token" using a google oauth2 web service.
 *
 * A CPLError will be reported if the translation fails for some reason.
 * Common reasons include the auth token already having been used before,
 * it not being appropriate for the passed scope and configured client api 
 * or http connection problems.  NULL is returned on error.
 *
 * @param pszAuthToken the authorization token from the user. 
 * @param pszScope the scope for which it is valid. 
 * 
 * @return refresh token, to be freed with CPLFree(), null on failure.
 */

char CPL_DLL *GOA2GetRefreshToken( const char *pszAuthToken,
                                   const char *pszScope )

{
/* -------------------------------------------------------------------- */
/*      Prepare request.                                                */
/* -------------------------------------------------------------------- */
    CPLString osItem;
    CPLStringList oOptions;

    oOptions.AddString(
        "HEADERS=Content-Type: application/x-www-form-urlencoded" );

    osItem.Printf(
        "POSTFIELDS="
        "code=%s"
        "&client_id=%s"
        "&client_secret=%s"
        "&redirect_uri=urn:ietf:wg:oauth:2.0:oob"
        "&grant_type=authorization_code", 
        pszAuthToken,
        GDAL_CLIENT_ID, 
        GDAL_CLIENT_SECRET);
    oOptions.AddString(osItem);

/* -------------------------------------------------------------------- */
/*      Submit request by HTTP.                                         */
/* -------------------------------------------------------------------- */
    CPLHTTPResult * psResult = CPLHTTPFetch( GOOGLE_AUTH_URL, oOptions);

    if (psResult == NULL)
        return FALSE;

/* -------------------------------------------------------------------- */
/*      One common mistake is to try and reuse the auth token.          */
/*      After the first use it will return invalid_grant.               */
/* -------------------------------------------------------------------- */
    if( psResult->pabyData != NULL
        && strstr((const char *) psResult->pabyData,"invalid_grant") != NULL) 
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to use a OAuth2 authorization code multiple times.\n"
                  "Request a fresh authorization token at\n%s.",
                  AUTH_TOKEN_REQUEST );
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    if (psResult->pabyData == NULL ||
        psResult->pszErrBuf != NULL)
    {
        if( psResult->pszErrBuf != NULL )
            CPLDebug( "GOA2", "%s", psResult->pszErrBuf );
        if( psResult->pabyData != NULL )
            CPLDebug( "GOA2", "%s", psResult->pabyData );

        CPLError( CE_Failure, CPLE_AppDefined,
                  "Fetching OAuth2 access code from auth code failed.");
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    CPLDebug( "GOA2", "Access Token Response:\n%s", 
              (const char *) psResult->pabyData );

/* -------------------------------------------------------------------- */
/*      This response is in JSON and will look something like:          */
/* -------------------------------------------------------------------- */
/*
{
  "access_token" : "ya29.AHES6ZToqkIJkat5rIqMixR1b8PlWBACNO8OYbqqV-YF1Q13E2Kzjw",
  "token_type" : "Bearer",
  "expires_in" : 3600,
  "refresh_token" : "1/eF88pciwq9Tp_rHEhuiIv9AS44Ufe4GOymGawTVPGYo"
}
*/
    CPLStringList oResponse = ParseSimpleJson(
        (const char *) psResult->pabyData );
    CPLHTTPDestroyResult(psResult);

    CPLString osAccessToken = oResponse.FetchNameValueDef( "access_token", "" );
    CPLString osRefreshToken = oResponse.FetchNameValueDef( "refresh_token", "" );
    CPLDebug("GOA2", "Access Token : '%s'", osAccessToken.c_str());
    CPLDebug("GOA2", "Refresh Token : '%s'", osRefreshToken.c_str());

    if( osRefreshToken.size() == 0) 
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to identify a refresh token in the OAuth2 response.");
        return NULL;
    }
    else 
    {
        // Currently we discard the access token and just return the refresh token
        return CPLStrdup(osRefreshToken);
    }
}

/************************************************************************/
/*                         GOA2GetAccessToken()                         */
/************************************************************************/

/**
 * Fetch access token using refresh token.
 * 
 * The permanent refresh token is used to fetch a temporary (usually one
 * hour) access token using Google OAuth2 web services. 
 *
 * A CPLError will be reported if the request fails for some reason.
 * Common reasons include the refresh token having been revoked by the
 * user or http connection problems.
 *
 * @param pszRefreshToken the refresh token from GOA2GetRefreshToken().
 * @param pszScope the scope for which it is valid. 
 * 
 * @return access token, to be freed with CPLFree(), null on failure.
 */

char *GOA2GetAccessToken( const char *pszRefreshToken, 
                          const char *pszScope )
{
/* -------------------------------------------------------------------- */
/*      Prepare request.                                                */
/* -------------------------------------------------------------------- */
    CPLString osItem;
    CPLStringList oOptions;

    oOptions.AddString(
        "HEADERS=Content-Type: application/x-www-form-urlencoded" );

    osItem.Printf(
        "POSTFIELDS="
        "refresh_token=%s"
        "&client_id=%s"
        "&client_secret=%s"
        "&grant_type=refresh_token", 
        pszRefreshToken,
        GDAL_CLIENT_ID, 
        GDAL_CLIENT_SECRET);
    oOptions.AddString(osItem);

/* -------------------------------------------------------------------- */
/*      Submit request by HTTP.                                         */
/* -------------------------------------------------------------------- */
    CPLHTTPResult * psResult = CPLHTTPFetch( GOOGLE_AUTH_URL, oOptions);

    if (psResult == NULL)
        return FALSE;

    if (psResult->pabyData == NULL ||
        psResult->pszErrBuf != NULL)
    {
        if( psResult->pszErrBuf != NULL )
            CPLDebug( "GFT", "%s", psResult->pszErrBuf );
        if( psResult->pabyData != NULL )
            CPLDebug( "GFT", "%s", psResult->pabyData );

        CPLError( CE_Failure, CPLE_AppDefined,
                  "Fetching OAuth2 access code from auth code failed.");
        CPLHTTPDestroyResult(psResult);
        return FALSE;
    }

    CPLDebug( "GOA2", "Refresh Token Response:\n%s", 
              (const char *) psResult->pabyData );

/* -------------------------------------------------------------------- */
/*      This response is in JSON and will look something like:          */
/* -------------------------------------------------------------------- */
/*
{
"access_token":"1/fFBGRNJru1FQd44AzqT3Zg",
"expires_in":3920,
"token_type":"Bearer"
}
*/
    CPLStringList oResponse = ParseSimpleJson(
        (const char *) psResult->pabyData );
    CPLHTTPDestroyResult(psResult);

    CPLString osAccessToken = oResponse.FetchNameValueDef( "access_token", "" );

    CPLDebug("GOA2", "Access Token : '%s'", osAccessToken.c_str());

    if (osAccessToken.size() == 0) 
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to identify an access token in the OAuth2 response.");
        return NULL;
    }
    else 
        return CPLStrdup(osAccessToken);
}



