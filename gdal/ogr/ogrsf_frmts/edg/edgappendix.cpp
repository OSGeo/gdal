/******************************************************************************
*
* Project:  Anatrack Ranges Edge File Translator
* Purpose:  Implements EdgAppendix class
* Author:   Nick Casey, nick@anatrack.com
*
******************************************************************************
* Copyright (c) 2020, Nick Casey <nick at anatrack.com>
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


#include "ogr_edg.h"

/************************************************************************/
/*                             EdgAppendix()                            */
/************************************************************************/

EdgAppendix::EdgAppendix():
    bAppendixLoaded(false),
    iLongitudeZone(-1),
    cLatitudeZone(0)
{
    // initialise options age and sex with unknown
    vsAgeLabels.push_back("?");
    vsSexLabels.push_back("?");

    // do not initialise core - we need to know it
}

/************************************************************************/
/*                            ~EdgAppendix()                            */
/************************************************************************/

EdgAppendix::~EdgAppendix()
{
}

/************************************************************************/
/*                            ReadAppendix()                            */
/************************************************************************/

bool EdgAppendix::ReadAppendix(VSILFILE* fp)
{
    if (fp == nullptr) {
        return false;
    }

    VSIFSeekL(fp, 0, SEEK_END); 

    // find the beginning of the appendix 
    char fcBuf;
    do {
        if (VSIFTellL(fp) < 2) { // didnt find the appendix marker
            return false;
        }
        VSIFSeekL(fp, -2, SEEK_CUR);
        if( VSIFReadL(&fcBuf, 1, 1, fp) != 1) {
            return false;
        }
    } while (fcBuf != '~');

    const char *pszLine = CPLReadLineL(fp);
    if (pszLine == NULL) // Didnt find an appendix line
        return false;

    char** papszTokens = CSLTokenizeString(pszLine);
    int nTokens = CSLCount(papszTokens);

    int iNumAgeLabels = atoi(papszTokens[1]);
    int iNumSexLabels = atoi(papszTokens[2]);

    for (int i = 0; i < iNumAgeLabels; i++)
    {
        vsAgeLabels.push_back( papszTokens[20 + i] );
    }

    for (int j = 0; j < iNumSexLabels; j++)
    {
        vsSexLabels.push_back( papszTokens[20 + iNumAgeLabels + j] );
    }

    for(int k=20 + iNumAgeLabels + iNumSexLabels; k < nTokens; k++) {  // not very important where we start this search 

        std::string sToken(papszTokens[k]);

        std::string sPre = "utm:";
        if (sToken.compare(0, sPre.size(), sPre) == 0) {

            sToken.erase(0, sPre.size()); //remove the  "utm:"

            std::string delimiter = "/";
            size_t pos = 0;

            pos = sToken.find(delimiter);
            sReferenceEllipsoid = sToken.substr(0, pos);
            
            sToken.erase(0, pos + delimiter.length());
            pos = sToken.find(delimiter);
            try {
                iLongitudeZone = stoi(sToken.substr(0, pos));
            }
            catch (const std::invalid_argument& ) {}

            sToken.erase(0, pos + delimiter.length());
            if (sToken.length() > 0)
            {
                cLatitudeZone = sToken.at(0);
            }

        }

    }

    return true;

}

/************************************************************************/
/*               GetHemisphereFromUTMLatitudeZone()                     */
/************************************************************************/

int EdgAppendix::GetHemisphereFromUTMLatitudeZone()
{
    int bNorth = FALSE;
    if (cLatitudeZone != 0)
    {
        if (cLatitudeZone >= 'N' && cLatitudeZone <= 'X')
        {
            bNorth = TRUE;
        }
    }

    return bNorth;
}

/************************************************************************/
/*                        GetSpatialReference()                         */
/************************************************************************/

OGRSpatialReference* EdgAppendix::GetSpatialReference()
{
    OGRSpatialReference* poSRS = new OGRSpatialReference(SRS_WKT_WGS84_LAT_LONG);
    poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    if (iLongitudeZone > -1)    // set coord system
    {
        poSRS->SetWellKnownGeogCS(sReferenceEllipsoid.c_str());
        poSRS->SetUTM(iLongitudeZone, GetHemisphereFromUTMLatitudeZone());
    }

    return poSRS;
}

/************************************************************************/
/*                           GrowExtents()                              */
/************************************************************************/

void EdgAppendix::GrowExtents(OGREnvelope *psGeomBounds)
{
    oEnvelope.Merge(*psGeomBounds);
}

/************************************************************************/
/*                           GetOrderedCores()                          */
/************************************************************************/

const std::vector<int> EdgAppendix::GetOrderedCores()
{
    std::vector<int> viOrderedCores;
    std::vector<int> viCores;

    std::transform(vsCores.begin(), vsCores.end(), std::back_inserter(viCores),
        [](const std::string& str) { return std::stoi(str); });

    std::sort(viCores.begin(), viCores.end(), std::greater<int>()); //sort the integered cores

    for (auto&& s : viCores) {
        std::vector<std::string>::iterator it = std::find(vsCores.begin(), vsCores.end(), std::to_string(s));
        if (it != vsCores.end())
        {
            viOrderedCores.push_back(static_cast<int>(std::distance(vsCores.begin(), it)));
        }
    }

    return viOrderedCores;
}

/************************************************************************/
/*                         UpdateMetaDataString()                       */
/************************************************************************/

void EdgAppendix::UpdateMetaDataString(std::string sMetaDataString, int piPosition)
{ 
    if (piPosition < static_cast<int>(vsMetaDataStrings.size()))
    {
        vsMetaDataStrings[piPosition] = sMetaDataString; 
    }
    else
    {
        vsMetaDataStrings.push_back(sMetaDataString);
    }
}

/************************************************************************/
/*                           GetAppendixString()                        */
/************************************************************************/

std::string EdgAppendix::GetAppendixString()
{
    std::string sAgeLabels;
    if (vsAgeLabels.size() > 1) {
        for (size_t i = 1; i < vsAgeLabels.size();i++) sAgeLabels += vsAgeLabels[i] + " ";
    }

    std::string sSexLabels;
    if (vsSexLabels.size() > 1) {
        for (size_t j = 1; j < vsSexLabels.size(); j++) sSexLabels += vsSexLabels[j] + " ";
    }
    
    std::string sCores;
    for (const auto &coreIndex : EdgAppendix::GetOrderedCores()) sCores += vsCores[coreIndex] + "/";

    std::string sUTMString = "";
    if (iLongitudeZone > 0)
        sUTMString = "utm:WGS84/" + std::to_string(iLongitudeZone) + "/" + cLatitudeZone;

    return "~ 10 "
        + std::to_string(vsAgeLabels.size() - 1) + " "
        + std::to_string(vsSexLabels.size() - 1) + " "
        + "0 "
        + std::to_string(oEnvelope.MinX) + " "
        + std::to_string(oEnvelope.MaxX) + " "
        + std::to_string(oEnvelope.MinY) + " "
        + std::to_string(oEnvelope.MaxY) + " "
        + std::to_string(vsIds.size()) + " "
        + "1 0 100 0 0 0 1 0 77 "
        + std::to_string(vsCores.size()) + " "
        + "10 "
        + sAgeLabels
        + sSexLabels
        + sCores + " "
        + "fqv_spreads "
        + sUTMString + " "
        + "10";
}
