/******************************************************************************
 *
 * Project:  CouchDB Translator
 * Purpose:  Implements OGRCouchDBRowsLayer class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_couchdb.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                         OGRCouchDBRowsLayer()                        */
/************************************************************************/

OGRCouchDBRowsLayer::OGRCouchDBRowsLayer(OGRCouchDBDataSource* poDSIn) :
    OGRCouchDBLayer(poDSIn),
    bAllInOne(false)
{
    poFeatureDefn = new OGRFeatureDefn( "rows" );
    poFeatureDefn->Reference();

    OGRFieldDefn oFieldId("_id", OFTString);
    poFeatureDefn->AddFieldDefn(&oFieldId);

    OGRFieldDefn oFieldRev("_rev", OFTString);
    poFeatureDefn->AddFieldDefn(&oFieldRev);

    SetDescription( poFeatureDefn->GetName() );
}

/************************************************************************/
/*                        ~OGRCouchDBRowsLayer()                        */
/************************************************************************/

OGRCouchDBRowsLayer::~OGRCouchDBRowsLayer() {}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRCouchDBRowsLayer::ResetReading()

{
    OGRCouchDBLayer::ResetReading();

    if( !bAllInOne )
    {
        json_object_put(poFeatures);
        poFeatures = NULL;
        aoFeatures.resize(0);
    }
}

/************************************************************************/
/*                           FetchNextRows()                            */
/************************************************************************/

bool OGRCouchDBRowsLayer::FetchNextRows()
{
    if( bAllInOne )
        return false;

    json_object_put(poFeatures);
    poFeatures = NULL;
    aoFeatures.resize(0);

    bool bHasEsperluet = strstr(poDS->GetURL(), "?") != NULL;

    CPLString osURI;
    if (strstr(poDS->GetURL(), "limit=") == NULL &&
        strstr(poDS->GetURL(), "skip=") == NULL)
    {
        if (!bHasEsperluet)
        {
            bHasEsperluet = true;
            osURI += "?";
        }

        osURI += CPLSPrintf("&limit=%d&skip=%d",
                            GetFeaturesToFetch(), nOffset);
    }
    if (strstr(poDS->GetURL(), "reduce=") == NULL)
    {
        if( !bHasEsperluet )
        {
            // bHasEsperluet = true;
            osURI += "?";
        }

        osURI += "&reduce=false";
    }
    json_object* poAnswerObj = poDS->GET(osURI);
    return FetchNextRowsAnalyseDocs(poAnswerObj);
}

/************************************************************************/
/*                         BuildFeatureDefn()                           */
/************************************************************************/

bool OGRCouchDBRowsLayer::BuildFeatureDefn()
{
    bool bRet = FetchNextRows();
    if (!bRet)
        return false;

    bRet = BuildFeatureDefnFromRows(poFeatures);
    if (!bRet)
        return false;

    if( bEOF )
        bAllInOne = true;

    return true;
}
