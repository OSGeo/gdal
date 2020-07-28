/******************************************************************************
 * $Id$
 *
 * Project:  Anatrack Ranges Edge File Translator
 * Purpose:  Implements OGREdgLayer class
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

#include "cpl_port.h"
#include "ogr_edg.h"

#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_error.h"
#include "cpl_vsi.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_geometry.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"
#include "ogrsf_frmts.h"

CPL_CVSID("$Id$")



/************************************************************************/
/*                            OGREdgLayer()                             */
/************************************************************************/

OGREdgLayer::OGREdgLayer(const char * pszFilenameP,
                            OGRSpatialReference *poSRSInP, 
                            bool bWriterInP) :
    psFilename(pszFilenameP),
    poSRSIn(nullptr),
    fp(nullptr),
    poEdgAppendix(new EdgAppendix()),
    poCT(nullptr),
    poFeatureDefn(new OGRFeatureDefn(CPLGetBasename(pszFilenameP))),
    bWriter(bWriterInP),
    bCTSet(false)
{
    nNextFID = 0;
    SetupFeatureDefinition();

    if (nullptr != poSRSInP) {
        poSRSIn = poSRSInP->Clone();
    }

    if (!bWriter)  
    {
        InitialiseReading();
    }
}

/************************************************************************/
/*                           ~OGREdgLayer()                           */
/************************************************************************/

OGREdgLayer::~OGREdgLayer()
{

    if (bWriter)
    {
        WriteEdgFile();
    }

    if (nullptr != poEdgAppendix)
        delete poEdgAppendix;

    if (nullptr != poFeatureDefn)
        poFeatureDefn->Release();
    
    if (nullptr != poCT)
        delete poCT;

    if (nullptr != poSRSIn)
        poSRSIn->Release();

    if (nullptr != fp)
    {
        VSIFCloseL(fp);
    }

}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGREdgLayer::ResetReading()
{
    VSIFSeekL(fp, 0, SEEK_SET);
    nNextFID = 0;
}

void OGREdgLayer::SetupFeatureDefinition()
{
    SetDescription(poFeatureDefn->GetName());
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbMultiPolygon);
    {
        OGRFieldDefn oFieldTemplate("ID", OFTString);
        poFeatureDefn->AddFieldDefn(&oFieldTemplate);
    }
    {
        OGRFieldDefn oFieldTemplate("Age", OFTString);
        poFeatureDefn->AddFieldDefn(&oFieldTemplate);
    }
    {
        OGRFieldDefn oFieldTemplate("Sex", OFTString);
        poFeatureDefn->AddFieldDefn(&oFieldTemplate);
    }
    {
        OGRFieldDefn oFieldTemplate("Month", OFTInteger);
        poFeatureDefn->AddFieldDefn(&oFieldTemplate);
    }
    {
        OGRFieldDefn oFieldTemplate("Year", OFTInteger);
        poFeatureDefn->AddFieldDefn(&oFieldTemplate);
    }
    {
        OGRFieldDefn oFieldTemplate("Core", OFTString);
        poFeatureDefn->AddFieldDefn(&oFieldTemplate);
    }

}

/************************************************************************/
/*                         InitialiseReading()                          */
/************************************************************************/


void OGREdgLayer::InitialiseReading()
{
    fp = VSIFOpenL(psFilename, "r");
    if (fp == nullptr)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
            "Unable to open the EDG file.");
        return;
    }

    if (!poEdgAppendix->ReadAppendix(fp))
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
            "Failed to load EDG file appendix.");
        return;
    }

    // Set the SRS from the appendix
    OGRSpatialReference* oSRS = poEdgAppendix->GetSpatialReference(); 
    if (nullptr != oSRS)
    {
        poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(oSRS);
        oSRS->Release();
    }
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGREdgLayer::GetNextFeature()
{

    CPLString fsId = "";
    int fiAge = 0;
    int fiSex = 0;
    int fiMonth = -9;
    int fiYear = -9;

    OGRMultiPolygon* poMultiPoly = nullptr;
    OGRPolygon* poPoly = nullptr;
    OGRLinearRing* poRing = nullptr;

    bool bLabelLineBefore = false;
    int iRangeNum = 0;

    while (true)
    {
        const char *pszLine = CPLReadLineL(fp);
        if (pszLine == NULL) // Are we at end of file (out of features)?
            return NULL;

        CPLString pszLineTabsToSpaces(pszLine);
        for (size_t i = 0; i < pszLineTabsToSpaces.size(); i++)
        {
            if (pszLineTabsToSpaces[i] == '\t')
            {
                pszLineTabsToSpaces[i] = ' ';
            }
        }

        char** papszTokens = CSLTokenizeString(pszLineTabsToSpaces);
        int nTokens = CSLCount(papszTokens);

        if (nTokens < 2)
        {
            // empty line or just one character
            // ignore them
        }
        else if (EQUAL(papszTokens[0], "~"))
        {
            //the appendix line and the end of the data
            break;
        }
        else if (!bLabelLineBefore && nTokens == 7)
        {
            iRangeNum++;
            fsId = papszTokens[0];
            fiAge = atoi(papszTokens[1]);
            fiSex = atoi(papszTokens[2]);
            fiMonth = atoi(papszTokens[3]);
            fiYear = atoi(papszTokens[4]);

            bLabelLineBefore = true;
        }
        else if (EQUAL(papszTokens[0], "-1") && nTokens == 2)
        {
            OGRFeature *poFeature = new OGRFeature(poFeatureDefn);

            // the end of the range - defines the feature
            if (poPoly)  
            {
                std::vector<std::string> vsAgeLabels = poEdgAppendix->GetAgeLabels();
                std::vector<std::string> vsSexLabels = poEdgAppendix->GetSexLabels();

                //get the core
                char** papszCoreTokens = CSLTokenizeString2(papszTokens[1], ".", CSLT_HONOURSTRINGS);
                int iCore = atoi(papszCoreTokens[0]);

                // set field values
                poFeature->SetField(0, fsId);
                poFeature->SetField(1, vsAgeLabels[fiAge].c_str());
                poFeature->SetField(2, vsSexLabels[fiSex].c_str());
                poFeature->SetField(3, fiMonth);
                poFeature->SetField(4, fiYear);
                poFeature->SetField(5, iCore);

                poFeature->SetFID(nNextFID++);
                if (poMultiPoly)
                {
                    poFeature->SetGeometryDirectly(poMultiPoly);
                }
                else
                {
                    poFeature->SetGeometryDirectly(poPoly);
                }
            }

            // feature is complete, check and return
            if (
                (m_poFilterGeom == NULL || FilterGeometry(poFeature->GetGeometryRef())) &&
                (m_poAttrQuery == NULL || m_poAttrQuery->Evaluate(poFeature))
                    )
            {
                return poFeature;
            }

            delete poFeature; //remove this one and loop again
            poFeature = nullptr;

            bLabelLineBefore = false;
        }
        else
        {
            // an edge shape - create a vector of coordinates

            poRing = new OGRLinearRing();

            int totalCorners = atoi(papszTokens[0]);
            bool isHole = (totalCorners < 0); // holes - attach it to the current edge polygon
            for (int n = 1; n < nTokens - 2; n++) // ignore the last pair
            {
                double fdE = atof(papszTokens[n]);
                n++;
                double fdN = atof(papszTokens[n]);

                poRing->addPoint(fdE, fdN);
            }
            poRing->closeRings();

            if (poPoly == nullptr) //the first poly
            {
                poPoly = new OGRPolygon();
                poPoly->addRingDirectly(poRing);
            }
            else
            {
                if (isHole)
                {
                    poPoly->addRingDirectly(poRing);
                }
                else // not a hole therefore a new poly and create a new poly and add it to the current multipoly
                {

                    if (poMultiPoly == nullptr) // as there is more than one polygon, create a multipolygon and add the first one
                    {
                        poMultiPoly = new OGRMultiPolygon();
                        poMultiPoly->addGeometryDirectly(poPoly);
                    }

                    poPoly = new OGRPolygon(); // then create a new polygon and add that one too
                    poMultiPoly->addGeometryDirectly(poPoly);
                    poPoly->addRingDirectly(poRing);
                }
            }
        }
    }

    return nullptr;

}

/************************************************************************/
/*                           GetUTMZone()                               */
/* Zone is retrieved from the niddle point of the first geometry        */
/************************************************************************/

void OGREdgLayer::GetUTMZone(double pdLat, double pdLon, char *pcLatZone, int *piLonZone, int *pbNorth)
{
    *piLonZone = (int)floor(pdLon / 6 + 31);

    *pcLatZone = 'X';
    if (pdLat<-72)
        *pcLatZone = 'C';
    else if (pdLat<-64)
        *pcLatZone = 'D';
    else if (pdLat<-56)
        *pcLatZone = 'E';
    else if (pdLat<-48)
        *pcLatZone = 'F';
    else if (pdLat<-40)
        *pcLatZone = 'G';
    else if (pdLat<-32)
        *pcLatZone = 'H';
    else if (pdLat<-24)
        *pcLatZone = 'J';
    else if (pdLat<-16)
        *pcLatZone = 'K';
    else if (pdLat<-8)
        *pcLatZone = 'L';
    else if (pdLat<0)
        *pcLatZone = 'M';
    else if (pdLat<8)
        *pcLatZone = 'N';
    else if (pdLat<16)
        *pcLatZone = 'P';
    else if (pdLat<24)
        *pcLatZone = 'Q';
    else if (pdLat<32)
        *pcLatZone = 'R';
    else if (pdLat<40)
        *pcLatZone = 'S';
    else if (pdLat<48)
        *pcLatZone = 'T';
    else if (pdLat<56)
        *pcLatZone = 'U';
    else if (pdLat<64)
        *pcLatZone = 'V';
    else if (pdLat<72)
        *pcLatZone = 'W';

    *pbNorth = FALSE;
    if(pdLat > 0)
        *pbNorth = TRUE;
}

/************************************************************************/
/*                      CreateCoordinateTransform()                     */
/************************************************************************/

void OGREdgLayer::CreateCoordinateTransform(OGREnvelope sSourceGeomBounds)
{

    /* Four scenarios
        1) poSRSIn is null -> SRS out is null/dont set projection in EdgAppendix/No CT
        2) poSRSIn has UTMZone & WGS84 -> set SRSout the same/ set projection in EdgAppendix/No CT
        3) poSRSIn has UTMZone & !WGS84 -> set SRSout with WGS84 and same UTM/ set projection in EdgAppendix/Yes CT
        4) poSRSIn but no UTM ZONE -> set SRSout with WGS84 set flag to get when coords available / set projection in EdgAppendix/Yes CT
    */

    if(poSRSIn != nullptr) 
    {
        char cLatZone;
        int iLonZone;
        int iNorth;
        
        // create coordinate transform
        OGRSpatialReference* oSRSOut = new OGRSpatialReference(nullptr);
        oSRSOut->SetWellKnownGeogCS("WGS84");
        oSRSOut->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        iLonZone = poSRSIn->GetUTMZone(&iNorth);
        if (iLonZone > 0)  // projected source SR
        {
            cLatZone = 'L'; //any zone in the south will do for Ranges
            if (iNorth == TRUE)
            {
                cLatZone = 'P';
            }
        }
        else { // lat-lon source SR
            
            // get UTM letter and number from the geometry
            double dLat = sSourceGeomBounds.MaxX - sSourceGeomBounds.MinX;
            double dLon = sSourceGeomBounds.MaxY - sSourceGeomBounds.MinY;
            GetUTMZone(dLat, dLon, &cLatZone, &iLonZone, &iNorth);
        }

        oSRSOut->SetUTM(iLonZone, iNorth);
        if (!oSRSOut->IsSame(poSRSIn))
        {
            poCT = OGRCreateCoordinateTransformation(poSRSIn, oSRSOut);
        }

        // set Edge appendix zone letter and number
        poEdgAppendix->setUTMZone(cLatZone, iLonZone);

        oSRSOut->Release();
    }

    bCTSet = true;
}

/************************************************************************/
/*                            WriteEdgFile()                            */
/************************************************************************/

void OGREdgLayer::WriteEdgFile()
{

    if (geometryMap.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Layer is empty. Nothing to write.");
        return;
    }
        
    VSILFILE* fpOutput = VSIFOpenL(psFilename, "w");
    if (fpOutput == nullptr)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
            "open(%s) failed: %s",
            psFilename.c_str(), VSIStrerror(errno));
        return;
    }

    std::vector<std::string> vsIds = poEdgAppendix->GetIds(); // a set of the individual headers
    std::vector<std::string> vsMetaDataStrings = poEdgAppendix->GetMetaDataStrings(); // a set of the individual headers
    std::vector<std::string> vsCores = poEdgAppendix->GetCores(); // a list of the cores in the correct order
    std::vector<int> viOrderedCores = poEdgAppendix->GetOrderedCores(); // a "map" of the cores in the correct order


    for (int j = 0; j < static_cast<int>(vsIds.size()); j++)  // for each id
    {
        // write a header
        VSIFPrintfL(fpOutput, vsMetaDataStrings[j].c_str());

        for (auto const& iCore : viOrderedCores) // for each core in order
        {
            // write the geometry
            std::pair <int, int> pIdCore(j, iCore);
            std::vector<std::string> vsGeometryPolygons = geometryMap.find(pIdCore)->second;

            for (auto const& sPolygon : vsGeometryPolygons)  // for each id
            {
                VSIFPrintfL(fpOutput, sPolygon.c_str());
            }

            std::string sCoreFooter = "-1 " + vsCores[iCore] + ".0\n";
            VSIFPrintfL(fpOutput, sCoreFooter.c_str()); // write a core footer
        }
        VSIFPrintfL(fpOutput, "\n"); // new line at the end of each range
    }

    std::string sAppendixLeader = "-1\n\n";
    VSIFPrintfL(fpOutput, sAppendixLeader.c_str());

    VSIFPrintfL(fpOutput, poEdgAppendix->GetAppendixString().c_str());     // write the appendix

    VSIFCloseL(fpOutput);

}

/************************************************************************/
/*                        CollectGeometryLine()                         */
/************************************************************************/

void OGREdgLayer::CollectGeometryLine(const OGRLineString *poLine, bool bIsHole, std::vector<std::string> &pvsGeometryPolygons)
{
    std::vector<float> vfGeometryLine;

    int iTotalLineCoords = poLine->getNumPoints();
    if (iTotalLineCoords < 3) {
        //not a polygon - ignore
        return;
    }

    int iTotalLineCoordsForWriting = iTotalLineCoords;
    if (bIsHole)
    {
        iTotalLineCoordsForWriting *= -1;
    }

    for (int iPoint = 0; iPoint < iTotalLineCoords; iPoint++)
    {
        vfGeometryLine.push_back(static_cast<float>(poLine->getX(iPoint)));
        vfGeometryLine.push_back(static_cast<float>(poLine->getY(iPoint)));
    }

    vfGeometryLine.push_back(-9.0F);
    vfGeometryLine.push_back(-9.0F);

    std::stringstream ssCoords;
    for (size_t i = 0; i < vfGeometryLine.size(); ++i)
    {
        ssCoords << " ";
        ssCoords << std::fixed << vfGeometryLine[i];
    }

    pvsGeometryPolygons.push_back(std::to_string(iTotalLineCoordsForWriting) + ssCoords.str() + "\n");

}

/************************************************************************/
/*                          CollectGeometry()                           */
/************************************************************************/

bool OGREdgLayer::CollectGeometry(OGRGeometry *poGeometry, std::vector<std::string> &pvsGeometryPolygons)
{
    if (poGeometry->getGeometryType() == wkbLineString)
    {
        CollectGeometryLine(poGeometry->toLineString(), false, pvsGeometryPolygons);
    }
    else if (poGeometry->getGeometryType() == wkbPolygon)
    {
        OGRPolygon* poPolygon = poGeometry->toPolygon();
        if (poPolygon->getExteriorRing() != nullptr)
        {
            CollectGeometryLine(poPolygon->getExteriorRing(), false, pvsGeometryPolygons);
        }

        for (int iRing = 0; iRing < poPolygon->getNumInteriorRings(); iRing++)
        {
            OGRLinearRing *poRing = poPolygon->getInteriorRing(iRing);

            CollectGeometryLine(poRing, true, pvsGeometryPolygons);
        }
    }
    else if (wkbFlatten(poGeometry->getGeometryType()) == wkbMultiPolygon
        || wkbFlatten(poGeometry->getGeometryType()) == wkbGeometryCollection)
    {
        OGRGeometryCollection* poGC = poGeometry->toGeometryCollection();

        for (auto&& poMember : poGC)
        {
            if (!CollectGeometry(poMember, pvsGeometryPolygons))
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }

    return true;
}

/************************************************************************/
/*                    GetRangeParameterFromField()                      */
/************************************************************************/

int OGREdgLayer::GetRangeParameterFromField(CPLString pszValue, std::vector<std::string> lookupVector, std::function<void(std::string)> updateFunction)
{
    int iValue = 0;

    std::vector<std::string>::iterator it = find(lookupVector.begin(), lookupVector.end(), pszValue);
    if (it != lookupVector.end())
    {
        iValue = static_cast<int>(std::distance(lookupVector.begin(), it));
    }
    else
    {
        iValue = static_cast<int>(lookupVector.size());
        updateFunction(pszValue);
    }

    return iValue;
}

/************************************************************************/
/*                           ICreateFeature()                           */
/************************************************************************/

OGRErr OGREdgLayer::ICreateFeature( OGRFeature *poFeature )
{

    OGRwkbGeometryType eGeomType = wkbNone;
    if (poFeature->GetGeometryRef() != nullptr)
        eGeomType = wkbFlatten(poFeature->GetGeometryRef()->getGeometryType());

    if (wkbPolygon == eGeomType
        || wkbMultiPolygon == eGeomType
        || wkbLineString == eGeomType
        || wkbMultiLineString == eGeomType)
    {
        // find any relevant field values: id/name, age, sex, month, year, core
        // note that these are indexes not values
        int iId = -1;
        int iAge = 0;
        int iSex = 0;
        int iMonth = -9;
        int iYear = -9;
        int iCore = -1;

        CPLString pszId(poFeature->GetFieldAsString(0));
        if (strlen(pszId) > 0)
        {
            for (size_t j = 0; j < pszId.size(); j++)
            {
                if (pszId[j] == ' ')
                {
                    pszId[j] = '_';
                }
            }

            iId = GetRangeParameterFromField(pszId, poEdgAppendix->GetIds(), std::bind(&EdgAppendix::AddId, poEdgAppendix, std::placeholders::_1));
        }

        CPLString pszAge(poFeature->GetFieldAsString(1));
        if (EQUAL(pszAge, ""))
            pszAge = "?";
        iAge = GetRangeParameterFromField(pszAge, poEdgAppendix->GetAgeLabels(), std::bind(&EdgAppendix::AddAgeLabel, poEdgAppendix, std::placeholders::_1));

        CPLString pszSex(poFeature->GetFieldAsString(2));
        if (EQUAL(pszSex, ""))
            pszSex = "?";
        iSex = GetRangeParameterFromField(pszSex, poEdgAppendix->GetSexLabels(), std::bind(&EdgAppendix::AddSexLabel, poEdgAppendix, std::placeholders::_1));

        int iSourceMonth = poFeature->GetFieldAsInteger(3);
        if (iSourceMonth > 0 && iSourceMonth < 13)
            iMonth = (int)iSourceMonth;

        int iSourceYear = poFeature->GetFieldAsInteger(4);
        if (iSourceYear > 1900 && iSourceYear < 2100)
            iYear = (int)iSourceYear;
        
        int iCoreValue = poFeature->GetFieldAsInteger(5); // Ranges does not yet handle decimal cores
        if (iCoreValue > 0)
        {
            CPLString pszCore = CPLSPrintf("%d",iCoreValue);
            iCore = GetRangeParameterFromField(pszCore, poEdgAppendix->GetCores(), std::bind(&EdgAppendix::AddCore, poEdgAppendix, std::placeholders::_1));
        }

        // if not core or ID then break out
        if (iId < 0 || iCore < 0) {
            CPLError(CE_Failure, CPLE_AppDefined,
                "Feature does not contain both ID or Core fields. Cannot convert to Anatrack EDG format");
            return OGRERR_NOT_ENOUGH_DATA;
        }

        std::string sMetaDataString = poEdgAppendix->GetIds()[iId] + "\t" 
                + std::to_string(iAge) + "\t"
                + std::to_string(iSex) + "\t"
                + std::to_string(iMonth) + "\t"
                + std::to_string(iYear) + "\t-9.000000\t-9.000000\n";
        poEdgAppendix->UpdateMetaDataString(sMetaDataString, iId);

        OGREnvelope sGeomBounds;
        OGRGeometry* poWGS84Geom = nullptr;

        if (!bCTSet)
        {
            OGREnvelope sSourceGeomBounds;
            poFeature->GetGeometryRef()->getEnvelope(&sSourceGeomBounds);
            CreateCoordinateTransform(sSourceGeomBounds);
        }

        // do the coordinate transformation 
        if (nullptr != poCT)
        {
            poWGS84Geom = poFeature->GetGeometryRef()->clone();
            poWGS84Geom->transform(poCT);
        }
        else
        {
            poWGS84Geom = poFeature->GetGeometryRef();
        }

        // create a vector of feature polygons
        // and add it to the map
        std::vector<std::string> vsGeometryPolygons;
        if (!CollectGeometry(poWGS84Geom, vsGeometryPolygons))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                "Failed to write geometry to EDG file");
        }

        std::pair <int, int> pIdCore(iId, iCore);
        geometryMap.insert({ pIdCore, vsGeometryPolygons });

        poWGS84Geom->getEnvelope(&sGeomBounds);
        poEdgAppendix->GrowExtents(&sGeomBounds);

        if (nullptr != poCT)
        {
            delete poWGS84Geom;
        }

    }
    
    return OGRERR_NONE;
}


