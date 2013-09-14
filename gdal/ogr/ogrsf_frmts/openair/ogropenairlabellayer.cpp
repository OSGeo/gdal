/******************************************************************************
 * $Id$
 *
 * Project:  OpenAir Translator
 * Purpose:  Implements OGROpenAirLabelLayer class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault <even dot rouault at mines dash paris dot org>
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

#include "ogr_openair.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_p.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                      OGROpenAirLabelLayer()                          */
/************************************************************************/

OGROpenAirLabelLayer::OGROpenAirLabelLayer( VSILFILE* fp )

{
    fpOpenAir = fp;
    nNextFID = 0;

    poSRS = new OGRSpatialReference(SRS_WKT_WGS84);

    poFeatureDefn = new OGRFeatureDefn( "labels"  );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbPoint );
    poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);

    OGRFieldDefn    oField1( "CLASS", OFTString);
    poFeatureDefn->AddFieldDefn( &oField1 );
    OGRFieldDefn    oField2( "NAME", OFTString);
    poFeatureDefn->AddFieldDefn( &oField2 );
    OGRFieldDefn    oField3( "FLOOR", OFTString);
    poFeatureDefn->AddFieldDefn( &oField3 );
    OGRFieldDefn    oField4( "CEILING", OFTString);
    poFeatureDefn->AddFieldDefn( &oField4 );
}

/************************************************************************/
/*                      ~OGROpenAirLabelLayer()                         */
/************************************************************************/

OGROpenAirLabelLayer::~OGROpenAirLabelLayer()

{
    if( poSRS != NULL )
        poSRS->Release();

    poFeatureDefn->Release();

    VSIFCloseL( fpOpenAir );
}


/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGROpenAirLabelLayer::ResetReading()

{
    nNextFID = 0;
    VSIFSeekL( fpOpenAir, 0, SEEK_SET );
}


/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGROpenAirLabelLayer::GetNextFeature()
{
    OGRFeature  *poFeature;

    while(TRUE)
    {
        poFeature = GetNextRawFeature();
        if (poFeature == NULL)
            return NULL;

        if((m_poFilterGeom == NULL
            || FilterGeometry( poFeature->GetGeometryRef() ) )
        && (m_poAttrQuery == NULL
            || m_poAttrQuery->Evaluate( poFeature )) )
        {
            return poFeature;
        }
        else
            delete poFeature;
    }
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGROpenAirLabelLayer::GetNextRawFeature()
{
    const char* pszLine;
    double dfLat = 0, dfLon = 0;
    int bHasCoord = FALSE;

    while(TRUE)
    {
        pszLine = CPLReadLine2L(fpOpenAir, 1024, NULL);
        if (pszLine == NULL)
            return NULL;

        if (pszLine[0] == '*' || pszLine[0] == '\0')
            continue;

        if (EQUALN(pszLine, "AC ", 3))
        {
            if (osCLASS.size() != 0)
            {
                osNAME = "";
                osCEILING = "";
                osFLOOR = "";
            }
            osCLASS = pszLine + 3;
        }
        else if (EQUALN(pszLine, "AN ", 3))
            osNAME = pszLine + 3;
        else if (EQUALN(pszLine, "AH ", 3))
            osCEILING = pszLine + 3;
        else if (EQUALN(pszLine, "AL ", 3))
            osFLOOR = pszLine + 3;
        else if (EQUALN(pszLine, "AT ", 3))
        {
            bHasCoord = OGROpenAirGetLatLon(pszLine + 3, dfLat, dfLon);
            break;
        }
    }

    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetField(0, osCLASS.c_str());
    poFeature->SetField(1, osNAME.c_str());
    poFeature->SetField(2, osFLOOR.c_str());
    poFeature->SetField(3, osCEILING.c_str());

    CPLString osStyle;
    osStyle.Printf("LABEL(t:\"%s\")", osNAME.c_str());
    poFeature->SetStyleString(osStyle.c_str());

    if (bHasCoord)
    {
        OGRPoint* poPoint = new OGRPoint(dfLon, dfLat);
        poPoint->assignSpatialReference(poSRS);
        poFeature->SetGeometryDirectly(poPoint);
    }

    poFeature->SetFID(nNextFID++);

    return poFeature;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGROpenAirLabelLayer::TestCapability( const char * pszCap )

{
    return FALSE;
}

