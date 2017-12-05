/******************************************************************************
 *
 * Project:  GTM Driver
 * Purpose:  Implementation of OGRGTMLayer class.
 * Author:   Leonardo de Paula Rosa Piga; http://lampiao.lsc.ic.unicamp.br/~piga
 *
 ******************************************************************************
 * Copyright (c) 2009, Leonardo de Paula Rosa Piga
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

#include "ogr_gtm.h"

CPL_CVSID("$Id$")

OGRGTMLayer::OGRGTMLayer() :
    poDS(NULL),
    poSRS(NULL),
    poCT(NULL),
    pszName(NULL),
    poFeatureDefn(NULL),
    nNextFID(0),
    nTotalFCount(0),
    bError(false)
{}

OGRGTMLayer::~OGRGTMLayer()
{
    if (poFeatureDefn != NULL)
    {
        poFeatureDefn->Release();
        poFeatureDefn = NULL;
    }

    if (poSRS != NULL)
    {
        poSRS->Release();
        poSRS = NULL;
    }

    if (poCT != NULL)
    {
        delete poCT;
        poCT = NULL;
    }

    CPLFree( pszName );
}

/************************************************************************/
/*                            GetLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn* OGRGTMLayer::GetLayerDefn()
{
    return poFeatureDefn;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGTMLayer::TestCapability( const char * pszCap )
{
    if (EQUAL(pszCap,OLCFastFeatureCount) &&
        m_poFilterGeom == NULL && m_poAttrQuery == NULL )
        return TRUE;

    if( EQUAL(pszCap,OLCCreateField) )
        return poDS != NULL && poDS->getOutputFP() != NULL;

    if( EQUAL(pszCap,OLCSequentialWrite) )
        return poDS != NULL && poDS->getOutputFP() != NULL;

    return FALSE;
}

/************************************************************************/
/*                CheckAndFixCoordinatesValidity()                      */
/************************************************************************/

OGRErr OGRGTMLayer::CheckAndFixCoordinatesValidity( double& pdfLatitude, double& pdfLongitude )
{
    if (pdfLatitude < -90 || pdfLatitude > 90)
    {
        static bool bFirstWarning = true;
        if (bFirstWarning)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Latitude %f is invalid. Valid range is [-90,90]. This warning will not be issued any more",
                     pdfLatitude);
            bFirstWarning = false;
        }
        return OGRERR_FAILURE;
    }

    if (pdfLongitude < -180 || pdfLongitude > 180)
    {
        static bool bFirstWarning = true;
        if (bFirstWarning)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Longitude %f has been modified to fit into range [-180,180]. This warning will not be issued any more",
                     pdfLongitude);
            bFirstWarning = false;
        }

        if (pdfLongitude > 180)
            pdfLongitude -= ((int) ((pdfLongitude+180)/360)*360);
        else if (pdfLongitude < -180)
            pdfLongitude += ((int) (180 - pdfLongitude)/360)*360;

        return OGRERR_NONE;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRGTMLayer::CreateField( OGRFieldDefn *poField,
                                 int /* bApproxOK */ )
{
    for( int iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        if (strcmp(poFeatureDefn->GetFieldDefn(iField)->GetNameRef(),
                   poField->GetNameRef() ) == 0)
        {
            return OGRERR_NONE;
        }
    }
    CPLError(CE_Failure, CPLE_NotSupported,
            "Field of name '%s' is not supported. ",
             poField->GetNameRef());
    return OGRERR_FAILURE;
}
