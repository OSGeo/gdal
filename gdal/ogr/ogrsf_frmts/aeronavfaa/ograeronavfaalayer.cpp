/******************************************************************************
 * $Id$
 *
 * Project:  AeronavFAA Translator
 * Purpose:  Implements OGRAeronavFAALayer class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_aeronavfaa.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_p.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                        OGRAeronavFAALayer()                          */
/************************************************************************/

OGRAeronavFAALayer::OGRAeronavFAALayer( VSILFILE* fp, const char* pszLayerName )

{
    fpAeronavFAA = fp;
    nNextFID = 0;
    bEOF = FALSE;

    psRecordDesc = NULL;

    poSRS = new OGRSpatialReference(SRS_WKT_WGS84);

    poFeatureDefn = new OGRFeatureDefn( pszLayerName );
    poFeatureDefn->Reference();
    poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
    SetDescription( poFeatureDefn->GetName() );
}

/************************************************************************/
/*                         ~OGRAeronavFAALayer()                        */
/************************************************************************/

OGRAeronavFAALayer::~OGRAeronavFAALayer()

{
    if( poSRS != NULL )
        poSRS->Release();

    poFeatureDefn->Release();

    VSIFCloseL( fpAeronavFAA );
}


/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRAeronavFAALayer::ResetReading()

{
    nNextFID = 0;
    bEOF = FALSE;
    VSIFSeekL( fpAeronavFAA, 0, SEEK_SET );
}


/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRAeronavFAALayer::GetNextFeature()
{
    OGRFeature  *poFeature;

    while(TRUE)
    {
        if (bEOF)
            return NULL;

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
/*                           TestCapability()                           */
/************************************************************************/

int OGRAeronavFAALayer::TestCapability( CPL_UNUSED const char * pszCap )
{
    return FALSE;
}



static const RecordFieldDesc DOFFields [] =
{
    { "ORS_CODE",  1, 2, OFTString },
    { "NUMBER"  ,  4, 9, OFTInteger, },
    { "VERIF_STATUS", 11, 11, OFTString },
    { "COUNTRY", 13, 14, OFTString  },
    { "STATE", 16, 17, OFTString },
    { "CITY", 19, 34, OFTString },
    { "TYPE", 63, 74, OFTString },
    { "QUANTITY", 76, 76, OFTInteger },
    { "AGL_HT", 78, 82, OFTInteger },
    { "AMSL_HT", 84, 88, OFTInteger },
    { "LIGHTING", 90, 90, OFTString },
    { "HOR_ACC", 92, 92, OFTString },
    { "VER_ACC", 94, 94, OFTString },
    { "MARK_INDIC", 96, 96, OFTString },
    { "FAA_STUDY_NUMBER", 98, 111, OFTString },
    { "ACTION", 113, 113, OFTString },
    { "DATE", 115, 121, OFTString }
};

static const RecordDesc DOF = { sizeof(DOFFields)/sizeof(DOFFields[0]), DOFFields, 36, 49 };


/************************************************************************/
/*                       OGRAeronavFAADOFLayer()                        */
/************************************************************************/

OGRAeronavFAADOFLayer::OGRAeronavFAADOFLayer( VSILFILE* fp, const char* pszLayerName ) :
                                            OGRAeronavFAALayer(fp, pszLayerName)
{
    poFeatureDefn->SetGeomType( wkbPoint );

    psRecordDesc = &DOF;

    int i;
    for(i=0;i<psRecordDesc->nFields;i++)
    {
        OGRFieldDefn oField( psRecordDesc->pasFields[i].pszFieldName, psRecordDesc->pasFields[i].eType );
        oField.SetWidth(psRecordDesc->pasFields[i].nLastCol - psRecordDesc->pasFields[i].nStartCol + 1);
        poFeatureDefn->AddFieldDefn( &oField );
    }
}

/************************************************************************/
/*                              GetLatLon()                             */
/************************************************************************/

static int GetLatLon(const char* pszLat,
                     char        chLatHemisphere,
                     const char* pszLon,
                     char        chLonHemisphere,
                     int         nSecLen,
                     double& dfLat, double& dfLon)
{
    char szDeg[4], szMin[3], szSec[10];
    szDeg[0] = pszLat[0];
    szDeg[1] = pszLat[1];
    szDeg[2] = 0;
    szMin[0] = pszLat[3];
    szMin[1] = pszLat[4];
    szMin[2] = 0;
    memcpy(szSec, pszLat + 6, MAX((int)sizeof(szSec) - 1, nSecLen));
    szSec[MAX((int)sizeof(szSec) - 1, nSecLen)] = 0;

    dfLat = atoi(szDeg) + atoi(szMin) / 60. + CPLAtof(szSec) / 3600.;
    if (chLatHemisphere == 'S')
        dfLat = -dfLat;

    szDeg[0] = pszLon[0];
    szDeg[1] = pszLon[1];
    szDeg[2] = pszLon[2];
    szDeg[3] = 0;
    szMin[0] = pszLon[4];
    szMin[1] = pszLon[5];
    szMin[2] = 0;
    memcpy(szSec, pszLon + 7, MAX((int)sizeof(szSec) - 1, nSecLen));
    szSec[MAX((int)sizeof(szSec) - 1, nSecLen)] = 0;

    dfLon = atoi(szDeg) + atoi(szMin) / 60. + CPLAtof(szSec) / 3600.;
    if (chLonHemisphere == ' ' || chLonHemisphere == 'W')
        dfLon = -dfLon;

    return TRUE;
}


/************************************************************************/
/*                              GetLatLon()                             */
/************************************************************************/

int OGRAeronavFAADOFLayer::GetLatLon(const char* pszLat, const char* pszLon, double& dfLat, double& dfLon)
{
    return ::GetLatLon(pszLat, pszLat[11], pszLon, pszLon[12], 5, dfLat, dfLon);
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRAeronavFAADOFLayer::GetNextRawFeature()
{
    const char* pszLine;
    char szBuffer[130];

    while(TRUE)
    {
        pszLine = CPLReadLine2L(fpAeronavFAA, 130, NULL);
        if (pszLine == NULL)
        {
            bEOF = TRUE;
            return NULL;
        }
        if (strlen(pszLine) != 128)
            continue;
        if ( ! (pszLine[psRecordDesc->nLatStartCol-1] >= '0' &&
                pszLine[psRecordDesc->nLatStartCol-1] <= '9') )
            continue;

        OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
        poFeature->SetFID(nNextFID ++);

        int i;
        for(i=0;i<psRecordDesc->nFields;i++)
        {
            int nWidth = psRecordDesc->pasFields[i].nLastCol - psRecordDesc->pasFields[i].nStartCol + 1;
            strncpy(szBuffer, pszLine + psRecordDesc->pasFields[i].nStartCol - 1, nWidth);
            szBuffer[nWidth] = 0;
            while(nWidth > 0 && szBuffer[nWidth - 1] == ' ')
            {
                szBuffer[nWidth - 1] = 0;
                nWidth --;
            }
            if (nWidth != 0)
                poFeature->SetField(i, szBuffer);
        }

        double dfLat, dfLon;
        GetLatLon(pszLine + psRecordDesc->nLatStartCol - 1,
                  pszLine + psRecordDesc->nLonStartCol - 1,
                  dfLat,
                  dfLon);

        OGRGeometry* poGeom = new OGRPoint(dfLon, dfLat);
        poGeom->assignSpatialReference(poSRS);
        poFeature->SetGeometryDirectly( poGeom );
        return poFeature;
    }
}



static const RecordFieldDesc NAVAIDFields [] =
{
    { "ID", 2, 6, OFTString },
    { "NAVAID_TYPE",  8, 9, OFTString },
    { "STATUS"  ,  11, 11, OFTString },
    { "NAME"  ,  44, 68, OFTString },
    { "CAN_ARTCC"  ,  69, 69, OFTString },
    { "SERVICE"  ,  76, 76, OFTString },
    { "FREQUENCY"  ,  78, 84, OFTString },
    { "CHANNEL"  ,  86, 89, OFTString },
    { "ELEVATION"  ,  92, 96, OFTString },
    { "MAG_VAR"  ,  98, 100, OFTString },
    { "ARTCC"  ,  102, 104, OFTString },
    { "STATE"  ,  106, 107, OFTString },
};

static const RecordDesc NAVAID = { sizeof(NAVAIDFields)/sizeof(NAVAIDFields[0]), NAVAIDFields, 17, 30 };


/************************************************************************/
/*                    OGRAeronavFAANAVAIDLayer()                        */
/************************************************************************/

OGRAeronavFAANAVAIDLayer::OGRAeronavFAANAVAIDLayer( VSILFILE* fp, const char* pszLayerName ) :
                                            OGRAeronavFAALayer(fp, pszLayerName)
{
    poFeatureDefn->SetGeomType( wkbPoint );

    psRecordDesc = &NAVAID;

    int i;
    for(i=0;i<psRecordDesc->nFields;i++)
    {
        OGRFieldDefn oField( psRecordDesc->pasFields[i].pszFieldName, psRecordDesc->pasFields[i].eType );
        oField.SetWidth(psRecordDesc->pasFields[i].nLastCol - psRecordDesc->pasFields[i].nStartCol + 1);
        poFeatureDefn->AddFieldDefn( &oField );
    }
}


/************************************************************************/
/*                              GetLatLon()                             */
/************************************************************************/

int OGRAeronavFAANAVAIDLayer::GetLatLon(const char* pszLat, const char* pszLon, double& dfLat, double& dfLon)
{
    return ::GetLatLon(pszLat + 2, pszLat[0], pszLon + 2, pszLon[0], 4, dfLat, dfLon);
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRAeronavFAANAVAIDLayer::GetNextRawFeature()
{
    const char* pszLine;
    char szBuffer[134];

    while(TRUE)
    {
        pszLine = CPLReadLine2L(fpAeronavFAA, 134, NULL);
        if (pszLine == NULL)
        {
            bEOF = TRUE;
            return NULL;
        }
        if (strlen(pszLine) != 132)
            continue;
        if ( !(pszLine[psRecordDesc->nLatStartCol-1] == 'N' ||
               pszLine[psRecordDesc->nLatStartCol-1] == 'S') )
            continue;
        if ( !(pszLine[psRecordDesc->nLonStartCol-1] == 'E' ||
               pszLine[psRecordDesc->nLonStartCol-1] == 'W') )
            continue;

        OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
        poFeature->SetFID(nNextFID ++);

        int i;
        for(i=0;i<psRecordDesc->nFields;i++)
        {
            int nWidth = psRecordDesc->pasFields[i].nLastCol - psRecordDesc->pasFields[i].nStartCol + 1;
            strncpy(szBuffer, pszLine + psRecordDesc->pasFields[i].nStartCol - 1, nWidth);
            szBuffer[nWidth] = 0;
            while(nWidth > 0 && szBuffer[nWidth - 1] == ' ')
            {
                szBuffer[nWidth - 1] = 0;
                nWidth --;
            }
            if (nWidth != 0)
                poFeature->SetField(i, szBuffer);
        }

        double dfLat, dfLon;
        GetLatLon(pszLine + psRecordDesc->nLatStartCol - 1,
                  pszLine + psRecordDesc->nLonStartCol - 1,
                  dfLat,
                  dfLon);

        OGRGeometry* poGeom = new OGRPoint(dfLon, dfLat);
        poGeom->assignSpatialReference(poSRS);
        poFeature->SetGeometryDirectly( poGeom );
        return poFeature;
    }
}



/************************************************************************/
/*                    OGRAeronavFAARouteLayer()                        */
/************************************************************************/

OGRAeronavFAARouteLayer::OGRAeronavFAARouteLayer( VSILFILE* fp, const char* pszLayerName, int bIsDPOrSTARS ) :
                        OGRAeronavFAALayer(fp, pszLayerName)
{
    this->bIsDPOrSTARS = bIsDPOrSTARS;

    poFeatureDefn->SetGeomType( wkbLineString );

    if (bIsDPOrSTARS)
    {
        {
            OGRFieldDefn oField( "APT_NAME", OFTString );
            poFeatureDefn->AddFieldDefn( &oField );
        }

        {
            OGRFieldDefn oField( "STATE", OFTString );
            poFeatureDefn->AddFieldDefn( &oField );
        }
    }

    {
        OGRFieldDefn oField( "NAME", OFTString );
        poFeatureDefn->AddFieldDefn( &oField );
    }
    
}


/************************************************************************/
/*                              GetLatLon()                             */
/************************************************************************/

int OGRAeronavFAARouteLayer::GetLatLon(const char* pszLat, const char* pszLon, double& dfLat, double& dfLon)
{
    return ::GetLatLon(pszLat, pszLat[10], pszLon, pszLon[11], 4, dfLat, dfLon);
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRAeronavFAARouteLayer::GetNextRawFeature()
{
    const char* pszLine;
    OGRFeature* poFeature = NULL;
    OGRLineString* poLS = NULL;

    while(TRUE)
    {
        if (osLastReadLine.size() != 0)
            pszLine = osLastReadLine.c_str();
        else
            pszLine = CPLReadLine2L(fpAeronavFAA, 87, NULL);
        osLastReadLine = "";

        if (pszLine == NULL)
        {
            bEOF = TRUE;
            return poFeature;
        }
        if (strlen(pszLine) != 85)
            continue;

        if (bIsDPOrSTARS && strncmp(pszLine, "===", 3) == 0 && pszLine[3] != '=')
        {
            osAPTName = pszLine + 3;
            const char* pszComma = strchr(pszLine + 3, ',');
            if (pszComma)
            {
                osAPTName.resize(pszComma - (pszLine + 3));
                osStateName = pszComma + 2;
                const char* pszEqual = strchr(pszComma + 2, '=');
                if (pszEqual)
                    osStateName.resize(pszEqual - (pszComma + 2));
            }
            else
            {
                const char* pszEqual = strchr(pszLine + 3, '=');
                if (pszEqual)
                    osAPTName.resize(pszEqual - (pszLine + 3));
                osStateName = "";
            }
        }

        if (strncmp(pszLine + 2, "FACILITY OR", strlen("FACILITY OR")) == 0)
            continue;
        if (strncmp(pszLine + 2, "INTERSECTION", strlen("INTERSECTION")) == 0)
            continue;

        if (strcmp(pszLine, "================================DELETIONS LIST=================================198326") == 0)
        {
            bEOF = TRUE;
            return poFeature;
        }

        if (poFeature == NULL)
        {
            if (pszLine[2] == ' ' || pszLine[2] == '-' )
            {
                continue;
            }

            if (strncmp(pszLine + 29, "                    ", 20) == 0 ||
                strchr(pszLine, '(') != NULL)
            {
                CPLString osName = pszLine + 2;
                osName.resize(60);
                while(osName.size() > 0 && osName[osName.size()-1] == ' ')
                {
                    osName.resize(osName.size()-1);
                }

                if (strcmp(osName.c_str(), "(DELETIONS LIST)") == 0)
                {
                    bEOF = TRUE;
                    return NULL;
                }

                poFeature = new OGRFeature(poFeatureDefn);
                poFeature->SetFID(nNextFID ++);
                if (bIsDPOrSTARS)
                {
                    poFeature->SetField(0, osAPTName);
                    poFeature->SetField(1, osStateName);
                    poFeature->SetField(2, osName);
                }
                else
                    poFeature->SetField(0, osName);
                poLS = new OGRLineString();
                poFeature->SetGeometryDirectly(poLS);
            }
            continue;
        }

        if (strncmp(pszLine, "                                                                                    0", 85) == 0)
        {
            if (poLS->getNumPoints() == 0)
                continue;
            else
                return poFeature;
        }

        if (pszLine[29 - 1] == ' ' && pszLine[42 - 1] == ' ')
            continue;
        if (strstr(pszLine, "RWY") || strchr(pszLine, '('))
        {
            osLastReadLine = pszLine;
            return poFeature;
        }

        double dfLat, dfLon;
        GetLatLon(pszLine + 29 - 1,
                  pszLine + 42 - 1,
                  dfLat,
                  dfLon);
        poLS->addPoint(dfLon, dfLat);
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRAeronavFAARouteLayer::ResetReading()

{
    OGRAeronavFAALayer::ResetReading();
    osLastReadLine = "";
    osAPTName = "";
    osStateName = "";
}


static const RecordFieldDesc IAPFields [] =
{
    { "LOC_ID",  4, 8, OFTString },
    { "MAG_VAR"  ,  52, 54, OFTInteger, },
    { "ELEVATION", 62, 67, OFTInteger },
    { "TYPE", 71, 77, OFTString  },

};

static const RecordDesc IAP = { sizeof(IAPFields)/sizeof(IAPFields[0]), IAPFields, -1, -1 };


/************************************************************************/
/*                     OGRAeronavFAAIAPLayer()                          */
/************************************************************************/

OGRAeronavFAAIAPLayer::OGRAeronavFAAIAPLayer( VSILFILE* fp, const char* pszLayerName ) :
                        OGRAeronavFAALayer(fp, pszLayerName)
{
    poFeatureDefn->SetGeomType( wkbPoint );

    {
        OGRFieldDefn oField( "CITY", OFTString );
        poFeatureDefn->AddFieldDefn( &oField );
    }
    {
        OGRFieldDefn oField( "STATE", OFTString );
        poFeatureDefn->AddFieldDefn( &oField );
    }
    {
        OGRFieldDefn oField( "APT_NAME", OFTString );
        poFeatureDefn->AddFieldDefn( &oField );
    }
    {
        OGRFieldDefn oField( "APT_CODE", OFTString );
        poFeatureDefn->AddFieldDefn( &oField );
    }


    psRecordDesc = &IAP;

    int i;
    for(i=0;i<psRecordDesc->nFields;i++)
    {
        OGRFieldDefn oField( psRecordDesc->pasFields[i].pszFieldName, psRecordDesc->pasFields[i].eType );
        oField.SetWidth(psRecordDesc->pasFields[i].nLastCol - psRecordDesc->pasFields[i].nStartCol + 1);
        poFeatureDefn->AddFieldDefn( &oField );
    }

}


/************************************************************************/
/*                              GetLatLon()                             */
/************************************************************************/

int OGRAeronavFAAIAPLayer::GetLatLon(const char* pszLat, const char* pszLon, double& dfLat, double& dfLon)
{
    return ::GetLatLon(pszLat, pszLat[11], pszLon, pszLon[12], 4, dfLat, dfLon);
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRAeronavFAAIAPLayer::GetNextRawFeature()
{
    const char* pszLine;
    char szBuffer[87];
    int nCountUnderscoreLines = 0;

    while(TRUE)
    {
        pszLine = CPLReadLine2L(fpAeronavFAA, 87, NULL);
        if (pszLine == NULL)
        {
            bEOF = TRUE;
            return NULL;
        }
        if (strlen(pszLine) != 85)
            continue;

        if (strncmp(pszLine, "DELETIONS", strlen("DELETIONS")) == 0)
        {
            bEOF = TRUE;
            return NULL;
        }

        if (nNextFID == 0 && nCountUnderscoreLines < 2)
        {
            if (strcmp(pszLine, "_____________________________________________________________________________  285285") == 0)
                nCountUnderscoreLines ++;
            continue;
        }

        if (pszLine[1] != ' ')
            continue;
        if (strncmp(pszLine, "                                                                               ", 79) == 0)
            continue;
        if (strstr(pszLine, "NAVIGATIONAL AIDS") != NULL)
            continue;
        if (strstr(pszLine, "TERMINAL INSTRUMENT FIXES") != NULL)
            continue;

        const char* pszComma = strchr(pszLine, ',');
        if (pszComma)
        {
            const char* pszBegin = pszLine;
            while( *pszBegin == ' ')
                pszBegin ++;
            osCityName = pszBegin;
            osCityName.resize(pszComma - pszBegin);
            osStateName = pszComma + 2;
            osStateName.resize(78 - (pszComma + 2 - pszLine));
            while(osStateName.size() > 0 && osStateName[osStateName.size()-1] == ' ')
            {
                osStateName.resize(osStateName.size()-1);
            }
            osAPTName = "";
            osAPTId = "";
            continue;
        }

        const char* pszLeftParenthesis = strstr(pszLine, " (");
        if (pszLeftParenthesis)
        {
            const char* pszRightParenthesis = strchr(pszLeftParenthesis, ')');
            if (pszRightParenthesis)
            {
                const char* pszBegin = pszLine;
                while( *pszBegin == ' ')
                    pszBegin ++;
                osAPTName = pszBegin;
                osAPTName.resize(pszLeftParenthesis - pszBegin);
                osAPTId = pszLeftParenthesis + 2;
                osAPTId.resize(pszRightParenthesis - (pszLeftParenthesis + 2));
            }
            continue;
        }

        OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
        poFeature->SetFID(nNextFID ++);

        poFeature->SetField(0, osCityName);
        poFeature->SetField(1, osStateName);
        poFeature->SetField(2, osAPTName);
        poFeature->SetField(3, osAPTId);

        int i;
        for(i=0;i<psRecordDesc->nFields;i++)
        {
            int nWidth = psRecordDesc->pasFields[i].nLastCol - psRecordDesc->pasFields[i].nStartCol + 1;
            strncpy(szBuffer, pszLine + psRecordDesc->pasFields[i].nStartCol - 1, nWidth);
            szBuffer[nWidth] = 0;
            while(nWidth > 0 && szBuffer[nWidth - 1] == ' ')
            {
                szBuffer[nWidth - 1] = 0;
                nWidth --;
            }
            if (nWidth != 0)
                poFeature->SetField(i + 4, szBuffer);
        }

        double dfLat, dfLon;
        GetLatLon(pszLine + 16 - 1,
                  (pszLine[34 - 1] != ' ') ? pszLine + 34 - 1 : pszLine + 35 - 1,
                  dfLat,
                  dfLon);

        OGRGeometry* poGeom = new OGRPoint(dfLon, dfLat);
        poGeom->assignSpatialReference(poSRS);
        poFeature->SetGeometryDirectly( poGeom );
        return poFeature;
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRAeronavFAAIAPLayer::ResetReading()

{
    OGRAeronavFAALayer::ResetReading();
    osCityName = "";
    osStateName = "";
    osAPTName = "";
    osAPTId = "";
}
