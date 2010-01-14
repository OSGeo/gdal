/******************************************************************************
 * $Id: ogr_xplane_fix_reader.cpp
 *
 * Project:  X-Plane fix.dat file reader
 * Purpose:  Implements OGRXPlaneFixReader class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2008, Even Rouault
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

#include "ogr_xplane_fix_reader.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                   OGRXPlaneCreateFixFileReader                       */
/************************************************************************/

OGRXPlaneReader* OGRXPlaneCreateFixFileReader( OGRXPlaneDataSource* poDataSource )
{
    OGRXPlaneReader* poReader = new OGRXPlaneFixReader(poDataSource);
    return poReader;
}


/************************************************************************/
/*                         OGRXPlaneFixReader()                         */
/************************************************************************/
OGRXPlaneFixReader::OGRXPlaneFixReader()
{
    poFIXLayer = NULL;
}

/************************************************************************/
/*                          OGRXPlaneFixReader()                        */
/************************************************************************/

OGRXPlaneFixReader::OGRXPlaneFixReader( OGRXPlaneDataSource* poDataSource )
{
    poFIXLayer = new OGRXPlaneFIXLayer();

    poDataSource->RegisterLayer(poFIXLayer);
}

/************************************************************************/
/*                        CloneForLayer()                               */
/************************************************************************/

OGRXPlaneReader* OGRXPlaneFixReader::CloneForLayer(OGRXPlaneLayer* poLayer)
{
    OGRXPlaneFixReader* poReader = new OGRXPlaneFixReader();

    poReader->poInterestLayer = poLayer;

    SET_IF_INTEREST_LAYER(poFIXLayer);

    if (pszFilename)
    {
        poReader->pszFilename = CPLStrdup(pszFilename);
        poReader->fp = VSIFOpen( pszFilename, "rt" );
    }

    return poReader;
}

/************************************************************************/
/*                         IsRecognizedVersion()                        */
/************************************************************************/

int OGRXPlaneFixReader::IsRecognizedVersion( const char* pszVersionString)
{
    return EQUALN(pszVersionString, "600 Version", 11);
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

void OGRXPlaneFixReader::Read()
{
    const char* pszLine;
    while((pszLine = CPLReadLine(fp)) != NULL)
    {
        papszTokens = CSLTokenizeString(pszLine);
        nTokens = CSLCount(papszTokens);

        nLineNumber ++;

        if (nTokens == 1 && strcmp(papszTokens[0], "99") == 0)
        {
            CSLDestroy(papszTokens);
            papszTokens = NULL;
            bEOF = TRUE;
            return;
        }
        else if (nTokens == 0 || assertMinCol(3) == FALSE)
        {
            CSLDestroy(papszTokens);
            papszTokens = NULL;
            continue;
        }

        ParseRecord();

        CSLDestroy(papszTokens);
        papszTokens = NULL;

        if (poInterestLayer && poInterestLayer->IsEmpty() == FALSE)
            return;
    }

    papszTokens = NULL;
    bEOF = TRUE;
}

/************************************************************************/
/*                            ParseRecord()                             */
/************************************************************************/

void    OGRXPlaneFixReader::ParseRecord()
{
    double dfLat, dfLon;
    CPLString osName;

    RET_IF_FAIL(readLatLon(&dfLat, &dfLon, 0));
    osName = readStringUntilEnd(2);

    if (poFIXLayer)
        poFIXLayer->AddFeature(osName, dfLat, dfLon);
}


/************************************************************************/
/*                           OGRXPlaneFIXLayer()                        */
/************************************************************************/

OGRXPlaneFIXLayer::OGRXPlaneFIXLayer() : OGRXPlaneLayer("FIX")
{
    poFeatureDefn->SetGeomType( wkbPoint );

    OGRFieldDefn oFieldName("fix_name", OFTString );
    oFieldName.SetPrecision(5);
    poFeatureDefn->AddFieldDefn( &oFieldName );
}

/************************************************************************/
/*                           AddFeature()                               */
/************************************************************************/

OGRFeature*
     OGRXPlaneFIXLayer::AddFeature(const char* pszFixName,
                                   double dfLat,
                                   double dfLon)
{
    int nCount = 0;
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetGeometryDirectly( new OGRPoint( dfLon, dfLat ) );
    poFeature->SetField( nCount++, pszFixName );

    RegisterFeature(poFeature);

    return poFeature;
}
