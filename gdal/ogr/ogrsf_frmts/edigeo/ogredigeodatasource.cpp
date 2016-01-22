/******************************************************************************
 * $Id$
 *
 * Project:  EDIGEO Translator
 * Purpose:  Implements OGREDIGEODataSource class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
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

#include "ogr_edigeo.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

#ifndef M_PI
# define M_PI  3.1415926535897932384626433832795
#endif

/************************************************************************/
/*                        OGREDIGEODataSource()                         */
/************************************************************************/

OGREDIGEODataSource::OGREDIGEODataSource()

{
    papoLayers = NULL;
    nLayers = 0;

    pszName = NULL;
    poSRS = NULL;

    bExtentValid = FALSE;
    dfMinX = dfMinY = dfMaxX = dfMaxY = 0;

    fpTHF = NULL;
    bHasReadEDIGEO = FALSE;

    bIncludeFontFamily = CSLTestBoolean(CPLGetConfigOption(
                                 "OGR_EDIGEO_INCLUDE_FONT_FAMILY", "YES"));

    iATR = iDI3 = iDI4 = iHEI = iFON = -1;
    iATR_VAL = iANGLE = iSIZE = iOBJ_LNK = iOBJ_LNK_LAYER = -1;
    dfSizeFactor = CPLAtof(CPLGetConfigOption("OGR_EDIGEO_FONT_SIZE_FACTOR", "2"));
    if (dfSizeFactor <= 0 || dfSizeFactor >= 100)
        dfSizeFactor = 2;

    bRecodeToUTF8 = CSLTestBoolean(CPLGetConfigOption(
                                        "OGR_EDIGEO_RECODE_TO_UTF8", "YES"));
    bHasUTF8ContentOnly = TRUE;
}

/************************************************************************/
/*                      ~OGREDIGEODataSource()                          */
/************************************************************************/

OGREDIGEODataSource::~OGREDIGEODataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    CPLFree( pszName );

    if (fpTHF)
        VSIFCloseL(fpTHF);

    if (poSRS)
        poSRS->Release();
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGREDIGEODataSource::TestCapability( CPL_UNUSED const char * pszCap )
{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGREDIGEODataSource::GetLayer( int iLayer )

{
    ReadEDIGEO();
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                         GetLayerCount()                              */
/************************************************************************/

int OGREDIGEODataSource::GetLayerCount()
{
    ReadEDIGEO();
    return nLayers;
}

/************************************************************************/
/*                              ReadTHF()                               */
/************************************************************************/

int OGREDIGEODataSource::ReadTHF(VSILFILE* fp)
{
    const char* pszLine;
    while((pszLine = CPLReadLine2L(fp, 81, NULL)) != NULL)
    {
        if (strlen(pszLine) < 8 || pszLine[7] != ':')
            continue;

        /* Cf Z 52000 tableau 56 for field list*/

        if (strncmp(pszLine, "LONSA", 5) == 0)
        {
            if (osLON.size() != 0)
            {
                CPLDebug("EDIGEO", "We only handle one lot per THF file");
                break;
            }
            osLON = pszLine + 8;
        }
        else if (strncmp(pszLine, "GNNSA", 5) == 0)
            osGNN = pszLine + 8;
        else if (strncmp(pszLine, "GONSA", 5) == 0)
            osGON = pszLine + 8;
        else if (strncmp(pszLine, "QANSA", 5) == 0)
            osQAN = pszLine + 8;
        else if (strncmp(pszLine, "DINSA", 5) == 0)
            osDIN = pszLine + 8;
        else if (strncmp(pszLine, "SCNSA", 5) == 0)
            osSCN = pszLine + 8;
        else if (strncmp(pszLine, "GDNSA", 5) == 0)
            aosGDN.push_back(pszLine + 8);
    }
    if (osLON.size() == 0)
    {
        CPLDebug("EDIGEO", "LON field missing");
        return 0;
    }
    if (osGON.size() == 0)
    {
        CPLDebug("EDIGEO", "GON field missing");
        return 0;
    }
    if (osDIN.size() == 0)
    {
        CPLDebug("EDIGEO", "DIN field missing");
        return 0;
    }
    if (osSCN.size() == 0)
    {
        CPLDebug("EDIGEO", "SCN field missing");
        return FALSE;
    }

    CPLDebug("EDIGEO", "LON = %s", osLON.c_str());
    CPLDebug("EDIGEO", "GNN = %s", osGNN.c_str());
    CPLDebug("EDIGEO", "GON = %s", osGON.c_str());
    CPLDebug("EDIGEO", "QAN = %s", osQAN.c_str());
    CPLDebug("EDIGEO", "DIN = %s", osDIN.c_str());
    CPLDebug("EDIGEO", "SCN = %s", osSCN.c_str());
    for(int i=0;i<(int)aosGDN.size();i++)
        CPLDebug("EDIGEO", "GDN[%d] = %s", i, aosGDN[i].c_str());
    
    return TRUE;
}


/************************************************************************/
/*                             OpenFile()                               */
/************************************************************************/

VSILFILE* OGREDIGEODataSource::OpenFile(const char *pszType,
                                        const CPLString& osExt)
{
    CPLString osTmp = osLON + pszType;
    CPLString osFilename = CPLFormCIFilename(CPLGetPath(pszName),
                                             osTmp.c_str(), osExt.c_str());
    VSILFILE* fp = VSIFOpenL(osFilename, "rb");
    if (fp == NULL)
    {
        CPLString osExtLower = osExt;
        for(int i=0;i<(int)osExt.size();i++)
            osExtLower[i] = (char)tolower(osExt[i]);
        CPLString osFilename2 = CPLFormCIFilename(CPLGetPath(pszName),
                                             osTmp.c_str(), osExtLower.c_str());
        fp = VSIFOpenL(osFilename2, "rb");
        if (fp == NULL)
        {
            CPLDebug("EDIGEO", "Cannot open %s", osFilename.c_str());
        }
    }
    return fp;
}

/************************************************************************/
/*                              ReadGEO()                               */
/************************************************************************/

int OGREDIGEODataSource::ReadGEO()
{
    VSILFILE* fp = OpenFile(osGON, "GEO");
    if (fp == NULL)
        return FALSE;

    const char* pszLine;
    while((pszLine = CPLReadLine2L(fp, 81, NULL)) != NULL)
    {
        if (strlen(pszLine) < 8 || pszLine[7] != ':')
            continue;

        if (strncmp(pszLine, "RELSA", 5) == 0)
        {
            osREL = pszLine + 8;
            CPLDebug("EDIGEO", "REL = %s", osREL.c_str());
            break;
        }
    }

    VSIFCloseL(fp);

    if (osREL.size() == 0)
    {
        CPLDebug("EDIGEO", "REL field missing");
        return FALSE;
    }

    /* All the SRS names mentionned in B.8.2.3 and B.8.3.1 are in the IGN file */
    poSRS = new OGRSpatialReference();
    CPLString osProj4Str = "+init=IGNF:" + osREL;
    if (poSRS->SetFromUserInput(osProj4Str.c_str()) != OGRERR_NONE)
    {
        /* Hard code a few common cases */
        if (osREL == "LAMB1")
            poSRS->importFromProj4("+proj=lcc +lat_1=49.5 +lat_0=49.5 +lon_0=0 +k_0=0.99987734 +x_0=600000 +y_0=200000 +a=6378249.2 +b=6356514.999978254 +nadgrids=ntf_r93.gsb,null +pm=paris +units=m +no_defs");
        else if (osREL == "LAMB2")
            poSRS->importFromProj4("+proj=lcc +lat_1=46.8 +lat_0=46.8 +lon_0=0 +k_0=0.99987742 +x_0=600000 +y_0=200000 +a=6378249.2 +b=6356514.999978254 +nadgrids=ntf_r93.gsb,null +pm=paris +units=m +no_defs");
        else if (osREL == "LAMB3")
            poSRS->importFromProj4("+proj=lcc +lat_1=44.1 +lat_0=44.1 +lon_0=0 +k_0=0.9998775 +x_0=600000 +y_0=200000 +a=6378249.2 +b=6356514.999978254 +nadgrids=ntf_r93.gsb,null +pm=paris +units=m +no_defs");
        else if (osREL == "LAMB4")
            poSRS->importFromProj4("+proj=lcc +lat_1=42.165 +lat_0=42.165 +lon_0=0 +k_0=0.99994471 +x_0=234.358 +y_0=185861.369 +a=6378249.2 +b=6356514.999978254 +nadgrids=ntf_r93.gsb,null +pm=paris +units=m +no_defs");
        else if (osREL == "LAMB93")
            poSRS->importFromProj4("+proj=lcc +lat_1=44 +lat_2=49 +lat_0=46.5 +lon_0=3 +x_0=700000 +y_0=6600000 +ellps=GRS81 +towgs84=0,0,0,0,0,0,0 +units=m +no_defs");
        else
        {
            CPLDebug("EDIGEO", "Cannot resolve %s SRS. Check that the IGNF file is in the directory of PROJ.4 ressource files", osREL.c_str());
            delete poSRS;
            poSRS = NULL;
        }
    }
    
    return TRUE;
}

/************************************************************************/
/*                              ReadGEN()                               */
/************************************************************************/

int OGREDIGEODataSource::ReadGEN()
{
    VSILFILE* fp = OpenFile(osGNN, "GEN");
    if (fp == NULL)
        return FALSE;

    const char* pszLine;
    CPLString osCM1, osCM2;
    while((pszLine = CPLReadLine2L(fp, 81, NULL)) != NULL)
    {
        if (strlen(pszLine) < 8 || pszLine[7] != ':')
            continue;

        if (strncmp(pszLine, "CM1CC", 5) == 0)
        {
            osCM1 = pszLine + 8;
        }
        else if (strncmp(pszLine, "CM2CC", 5) == 0)
        {
            osCM2 = pszLine + 8;
        }
    }

    VSIFCloseL(fp);

    if (osCM1.size() == 0 || osCM2.size() == 0)
        return FALSE;

    char** papszTokens1 = CSLTokenizeString2(osCM1.c_str(), ";", 0);
    char** papszTokens2 = CSLTokenizeString2(osCM2.c_str(), ";", 0);
    if (CSLCount(papszTokens1) == 2 && CSLCount(papszTokens2) == 2)
    {
        bExtentValid = TRUE;
        dfMinX = CPLAtof(papszTokens1[0]);
        dfMinY = CPLAtof(papszTokens1[1]);
        dfMaxX = CPLAtof(papszTokens2[0]);
        dfMaxY = CPLAtof(papszTokens2[1]);
    }
    CSLDestroy(papszTokens1);
    CSLDestroy(papszTokens2);

    return bExtentValid;
}

/************************************************************************/
/*                              ReadDIC()                               */
/************************************************************************/

int OGREDIGEODataSource::ReadDIC()
{
    VSILFILE* fp = OpenFile(osDIN, "DIC");
    if (fp == NULL)
        return FALSE;

    const char* pszLine;
    CPLString osRTY, osRID, osLAB, osTYP;
    while(TRUE)
    {
        pszLine = CPLReadLine2L(fp, 81, NULL);
        if (pszLine != NULL)
        {
            if (strlen(pszLine) < 8 || pszLine[7] != ':')
                continue;
        }

        if (pszLine == NULL || strncmp(pszLine, "RTYSA", 5) == 0)
        {
            if (osRTY == "DID")
            {
                //CPLDebug("EDIGEO", "Object %s = %s",
                //         osRID.c_str(), osLAB.c_str());
                mapObjects[osRID] = osLAB;
            }
            else if (osRTY == "DIA")
            {
                //CPLDebug("EDIGEO", "Attribute %s = %s, %s",
                //         osRID.c_str(), osLAB.c_str(), osTYP.c_str());
                OGREDIGEOAttributeDef sAttributeDef;
                sAttributeDef.osLAB = osLAB;
                sAttributeDef.osTYP = osTYP;
                mapAttributes[osRID] = sAttributeDef;
            }
            if (pszLine == NULL)
                break;
            osRTY = pszLine + 8;
            osRID = "";
            osLAB = "";
            osTYP = "";
        }
        if (strncmp(pszLine, "RIDSA", 5) == 0)
            osRID = pszLine + 8;
        else if (strncmp(pszLine, "LABSA", 5) == 0)
            osLAB = pszLine + 8;
        else if (strncmp(pszLine, "TYPSA", 5) == 0)
            osTYP = pszLine + 8;
    }

    VSIFCloseL(fp);


    return TRUE;
}

/************************************************************************/
/*                              ReadSCD()                               */
/************************************************************************/

int OGREDIGEODataSource::ReadSCD()
{
    VSILFILE* fp = OpenFile(osSCN, "SCD");
    if (fp == NULL)
        return FALSE;

    const char* pszLine;
    CPLString osRTY, osRID, osNameRID, osKND;
    strListType aosAttrRID;
    int nWidth = 0;
    while(TRUE)
    {
        pszLine = CPLReadLine2L(fp, 81, NULL);
        if (pszLine != NULL)
        {
            if (strlen(pszLine) < 8 || pszLine[7] != ':')
                continue;
        }

        if (pszLine == NULL || strncmp(pszLine, "RTYSA", 5) == 0)
        {
            if (osRTY == "OBJ")
            {
                if (mapObjects.find(osNameRID) == mapObjects.end())
                {
                    CPLDebug("EDIGEO", "Cannot find object %s",
                             osNameRID.c_str());
                }
                else
                {
                    OGREDIGEOObjectDescriptor objDesc;
                    objDesc.osRID = osRID;
                    objDesc.osNameRID = osNameRID;
                    objDesc.osKND = osKND;
                    objDesc.aosAttrRID = aosAttrRID;
                    /*CPLDebug("EDIGEO", "Object %s = %s, %s, %d attributes",
                            osRID.c_str(), osNameRID.c_str(), osKND.c_str(),
                            (int)aosAttrRID.size());*/

                    aoObjList.push_back(objDesc);
                }
            }
            else if (osRTY == "ATT")
            {
                if (mapAttributes.find(osNameRID) == mapAttributes.end())
                {
                    CPLDebug("EDIGEO", "Cannot find attribute %s",
                             osNameRID.c_str());
                }
                else
                {
                    OGREDIGEOAttributeDescriptor attDesc;
                    attDesc.osRID = osRID;
                    attDesc.osNameRID = osNameRID;
                    attDesc.nWidth = nWidth;
                    /*CPLDebug("EDIGEO", "Attribute %s = %s, %d",
                            osRID.c_str(), osNameRID.c_str(), nWidth);*/

                    mapAttributesSCD[osRID] = attDesc;
                }
            }
            if (pszLine == NULL)
                break;
            osRTY = pszLine + 8;
            osRID = "";
            osNameRID = "";
            osKND = "";
            aosAttrRID.resize(0);
            nWidth = 0;
        }
        if (strncmp(pszLine, "RIDSA", 5) == 0)
            osRID = pszLine + 8;
        else if (strncmp(pszLine, "DIPCP", 5) == 0)
        {
            const char* pszDIP = pszLine + 8;
            char** papszTokens = CSLTokenizeString2(pszDIP, ";", 0);
            if (CSLCount(papszTokens) == 4)
            {
                osNameRID = papszTokens[3];
            }
            CSLDestroy(papszTokens);
        }
        else if (strncmp(pszLine, "KNDSA", 5) == 0)
            osKND = pszLine + 8;
        else if (strncmp(pszLine, "AAPCP", 5) == 0)
        {
            const char* pszAAP = pszLine + 8;
            char** papszTokens = CSLTokenizeString2(pszAAP, ";", 0);
            if (CSLCount(papszTokens) == 4)
            {
                const char* pszAttRID = papszTokens[3];
                aosAttrRID.push_back(pszAttRID);
            }
            CSLDestroy(papszTokens);
        }
        else if (strncmp(pszLine, "CANSN", 5) == 0)
            nWidth = atoi(pszLine + 8);
    }

    VSIFCloseL(fp);


    return TRUE;
}

/************************************************************************/
/*                              ReadQAL()                               */
/************************************************************************/

int OGREDIGEODataSource::ReadQAL()
{
    VSILFILE* fp = OpenFile(osQAN, "QAL");
    if (fp == NULL)
        return FALSE;

    const char* pszLine;
    CPLString osRTY, osRID;
    int nODA = 0, nUDA = 0;
    while(TRUE)
    {
        pszLine = CPLReadLine2L(fp, 81, NULL);
        if (pszLine != NULL)
        {
            if (strlen(pszLine) < 8 || pszLine[7] != ':')
                continue;
        }

        if (pszLine == NULL || strncmp(pszLine, "RTYSA", 5) == 0)
        {
            if (osRTY == "QUP")
            {
                mapQAL[osRID] = intintType(nODA, nUDA);
            }
            if (pszLine == NULL)
                break;
            osRTY = pszLine + 8;
            osRID = "";
            nODA = 0;
            nUDA = 0;
        }
        else if (strncmp(pszLine, "RIDSA", 5) == 0)
            osRID = pszLine + 8;
        else if (strncmp(pszLine, "ODASD", 5) == 0)
            nODA = atoi(pszLine + 8);
        else if (strncmp(pszLine, "UDASD", 5) == 0)
            nUDA = atoi(pszLine + 8);
    }

    VSIFCloseL(fp);

    return TRUE;
}

/************************************************************************/
/*                       CreateLayerFromObjectDesc()                    */
/************************************************************************/

int OGREDIGEODataSource::CreateLayerFromObjectDesc(const OGREDIGEOObjectDescriptor& objDesc)
{
    OGRwkbGeometryType eType = wkbUnknown;
    if (objDesc.osKND == "ARE")
        eType = wkbPolygon;
    else if (objDesc.osKND == "LIN")
        eType = wkbLineString;
    else if (objDesc.osKND == "PCT")
        eType = wkbPoint;
    else
    {
        CPLDebug("EDIGEO", "Unknown KND : %s", objDesc.osKND.c_str());
        return FALSE;
    }

    const char* pszLayerName = objDesc.osRID.c_str();
        //mapObjects.find(objDesc.osNameRID)->second.c_str();
    OGREDIGEOLayer* poLayer = new OGREDIGEOLayer(this, pszLayerName,
                                                    eType, poSRS);

    poLayer->AddFieldDefn("OBJECT_RID", OFTString, "");

    for(int j=0;j<(int)objDesc.aosAttrRID.size();j++)
    {
        std::map<CPLString,OGREDIGEOAttributeDescriptor>::iterator it =
            mapAttributesSCD.find(objDesc.aosAttrRID[j]);
        if (it != mapAttributesSCD.end())
        {
            const OGREDIGEOAttributeDescriptor& attrDesc = it->second;
            const OGREDIGEOAttributeDef& attrDef =
                                    mapAttributes[attrDesc.osNameRID];
            OGRFieldType eType = OFTString;
            if (attrDef.osTYP == "R" || attrDef.osTYP == "E")
                eType = OFTReal;
            else if (attrDef.osTYP == "I" || attrDef.osTYP == "N")
                eType = OFTInteger;

            poLayer->AddFieldDefn(attrDef.osLAB, eType, objDesc.aosAttrRID[j]);
        }
    }


    if (strcmp(poLayer->GetName(), "ID_S_OBJ_Z_1_2_2") == 0)
    {
        OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();

        iATR = poFDefn->GetFieldIndex("ATR");
        iDI3 = poFDefn->GetFieldIndex("DI3");
        iDI4 = poFDefn->GetFieldIndex("DI4");
        iHEI = poFDefn->GetFieldIndex("HEI");
        iFON = poFDefn->GetFieldIndex("FON");

        poLayer->AddFieldDefn("OGR_OBJ_LNK", OFTString, "");
        iOBJ_LNK = poFDefn->GetFieldIndex("OGR_OBJ_LNK");

        poLayer->AddFieldDefn("OGR_OBJ_LNK_LAYER", OFTString, "");
        iOBJ_LNK_LAYER = poFDefn->GetFieldIndex("OGR_OBJ_LNK_LAYER");

        poLayer->AddFieldDefn("OGR_ATR_VAL", OFTString, "");
        iATR_VAL = poFDefn->GetFieldIndex("OGR_ATR_VAL");

        poLayer->AddFieldDefn("OGR_ANGLE", OFTReal, "");
        iANGLE = poFDefn->GetFieldIndex("OGR_ANGLE");

        poLayer->AddFieldDefn("OGR_FONT_SIZE", OFTReal, "");
        iSIZE = poFDefn->GetFieldIndex("OGR_FONT_SIZE");
    }
    else if (mapQAL.size() != 0)
    {
        poLayer->AddFieldDefn("CREAT_DATE", OFTInteger, "");
        poLayer->AddFieldDefn("UPDATE_DATE", OFTInteger, "");
    }

    mapLayer[objDesc.osRID] = poLayer;

    papoLayers = (OGRLayer**)
        CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
    papoLayers[nLayers] = poLayer;
    nLayers ++;

    return TRUE;
}

/************************************************************************/
/*                              ReadVEC()                               */
/************************************************************************/

int OGREDIGEODataSource::ReadVEC(const char* pszVECName)
{
    VSILFILE* fp = OpenFile(pszVECName, "VEC");
    if (fp == NULL)
        return FALSE;

    const char* pszLine;
    CPLString osRTY, osRID;
    xyPairListType aXY;
    CPLString osLnkStartType, osLnkStartName, osLnkEndType, osLnkEndName;
    strListType osLnkEndNameList;
    CPLString osAttId;
    std::vector< strstrType > aosAttIdVal;
    CPLString osSCP;
    CPLString osQUP_RID;
    int bIso8859_1 = FALSE;

    while(TRUE)
    {
        pszLine = CPLReadLine2L(fp, 81, NULL);
skip_read_next_line:
        if (pszLine != NULL)
        {
            if (strlen(pszLine) < 8 || pszLine[7] != ':')
                continue;
        }

        if (pszLine == NULL || strncmp(pszLine, "RTYSA", 5) == 0)
        {
            if (osRTY == "PAR")
            {
                if (aXY.size() < 2)
                    CPLDebug("EDIGEO", "Error: ARC %s has not enough points",
                                    osRID.c_str());
                else
                    mapPAR[osRID] = aXY;
            }
            else if (osRTY == "LNK")
            {
                if (osLnkStartType == "PAR" && osLnkEndType == "PFE")
                {
                    /*CPLDebug("EDIGEO", "PFE[%s] -> PAR[%s]",
                             osLnkEndName.c_str(), osLnkStartName.c_str());*/
                    if (mapPFE_PAR.find(osLnkEndName) == mapPFE_PAR.end())
                        mapPFE_PAR[osLnkEndName].push_back(osLnkStartName);
                    else
                    {
                        int bAlreadyExists = FALSE;
                        strListType& osPARList = mapPFE_PAR[osLnkEndName];
                        for(int j=0;j<(int)osPARList.size();j++)
                        {
                            if (osPARList[j] == osLnkStartName)
                                bAlreadyExists = TRUE;
                        }
                        if (!bAlreadyExists)
                            osPARList.push_back(osLnkStartName);
                    }
                }
                else if (osLnkStartType == "FEA" && osLnkEndType == "PFE")
                {
                    /*CPLDebug("EDIGEO", "FEA[%s] -> PFE[%s]",
                             osLnkStartName.c_str(), osLnkEndName.c_str());*/
                    listFEA_PFE.push_back(strstrType
                                               (osLnkStartName, osLnkEndName));
                }
                else if (osLnkStartType == "FEA" && osLnkEndType == "PAR")
                {
                    /*CPLDebug("EDIGEO", "FEA[%s] -> PAR[%s]",
                             osLnkStartName.c_str(), osLnkEndName.c_str());*/
                    listFEA_PAR.push_back(std::pair<CPLString, strListType >
                                                (osLnkStartName, osLnkEndNameList));
                }
                else if (osLnkStartType == "FEA" && osLnkEndType == "PNO")
                {
                    /*CPLDebug("EDIGEO", "FEA[%s] -> PNO[%s]",
                             osLnkStartName.c_str(), osLnkEndName.c_str());*/
                    listFEA_PNO.push_back(strstrType
                                                (osLnkStartName, osLnkEndName));
                }
                else if (osLnkStartType == "FEA" && osLnkEndType == "FEA")
                {
                    /*CPLDebug("EDIGEO", "FEA[%s] -> FEA[%s]",
                             osLnkStartName.c_str(), osLnkEndName.c_str());*/
                    if (osSCP == "IS_S_REL_IWW")
                        mapFEA_FEA[osLnkStartName] = osLnkEndName;
                }
                else if (osLnkStartType == "PAR" && osLnkEndType == "PNO")
                {
                }
                else
                {
                    CPLDebug("EDIGEO", "Unhandled LNK(%s) %s=%s --> %s=%s",
                             osRID.c_str(),
                             osLnkStartType.c_str(), osLnkStartName.c_str(),
                             osLnkEndType.c_str(), osLnkEndName.c_str());
                }
            }
            else if (osRTY == "FEA")
            {
                OGREDIGEOFEADesc feaDesc;
                feaDesc.aosAttIdVal = aosAttIdVal;
                feaDesc.osSCP = osSCP;
                feaDesc.osQUP_RID = osQUP_RID;
                mapFEA[osRID] = feaDesc;
            }
            else if (osRTY == "PNO")
            {
                if (aXY.size() == 1)
                {
                    /*CPLDebug("EDIGEO", "PNO[%s] = %f, %f",
                             osRID.c_str(), aXY[0].first, aXY[0].second);*/
                    mapPNO[osRID] = aXY[0];
                }
            }
            if (pszLine == NULL)
                break;
            osRTY = pszLine + 8;
            osRID = "";
            aXY.resize(0);
            osLnkStartType = "";
            osLnkStartName = "";
            osLnkEndType = "";
            osLnkEndName = "";
            osAttId = "";
            aosAttIdVal.resize(0);
            osLnkEndNameList.resize(0);
            osSCP = "";
            osQUP_RID = "";
            bIso8859_1 = FALSE;
        }
        else if (strncmp(pszLine, "RIDSA", 5) == 0)
            osRID = pszLine + 8;
        else if (strncmp(pszLine, "CORCC", 5) == 0)
        {
            const char* pszY = strchr(pszLine+8, ';');
            if (pszY)
            {
                double dfX = CPLAtof(pszLine + 8);
                double dfY = CPLAtof(pszY + 1);
                aXY.push_back(xyPairType (dfX, dfY));
            }
        }
        else if (strncmp(pszLine, "FTPCP", 5) == 0)
        {
            char** papszTokens = CSLTokenizeString2(pszLine + 8, ";", 0);
            if (CSLCount(papszTokens) == 4)
            {
                if (osLnkStartType.size() == 0)
                {
                    osLnkStartType = papszTokens[2];
                    osLnkStartName = papszTokens[3];
                }
                else
                {
                    osLnkEndType = papszTokens[2];
                    osLnkEndName = papszTokens[3];
                    osLnkEndNameList.push_back(osLnkEndName);
                }
            }
            CSLDestroy(papszTokens);
        }
        else if (strncmp(pszLine, "SCPCP", 5) == 0)
        {
            char** papszTokens = CSLTokenizeString2(pszLine + 8, ";", 0);
            if (CSLCount(papszTokens) == 4)
            {
                if (osRTY == "LNK")
                {
                    if (strcmp(papszTokens[2], "ASS") == 0)
                        osSCP = papszTokens[3];
                }
                else if (strcmp(papszTokens[2], "OBJ") == 0)
                    osSCP = papszTokens[3];
            }
            CSLDestroy(papszTokens);
        }
        else if (strncmp(pszLine, "ATPCP", 5) == 0)
        {
            char** papszTokens = CSLTokenizeString2(pszLine + 8, ";", 0);
            if (CSLCount(papszTokens) == 4)
            {
                if (strcmp(papszTokens[2], "ATT") == 0)
                    osAttId = papszTokens[3];
            }
            CSLDestroy(papszTokens);
        }
        else if (strcmp(pszLine, "TEXT 06:8859-1") == 0)
        {
            bIso8859_1 = TRUE;
        }
        else if (strncmp(pszLine, "ATVS", 4) == 0)
        {
            CPLString osAttVal = pszLine + 8;
            int bSkipReadNextLine = FALSE;
            while(TRUE)
            {
                pszLine = CPLReadLine2L(fp, 81, NULL);
                if (pszLine != NULL &&
                    strlen(pszLine) >= 8 &&
                    pszLine[7] == ':' &&
                    strncmp(pszLine, "NEXT ", 5) == 0)
                {
                    osAttVal += pszLine + 8;
                }
                else
                {
                    bSkipReadNextLine = TRUE;
                    break;
                }
            }
            if (bIso8859_1 && bRecodeToUTF8)
            {
                char* pszNewVal = CPLRecode(osAttVal.c_str(),
                                            CPL_ENC_ISO8859_1, CPL_ENC_UTF8);
                osAttVal = pszNewVal;
                CPLFree(pszNewVal);
            }
            else if (bHasUTF8ContentOnly)
            {
                bHasUTF8ContentOnly = CPLIsUTF8(osAttVal.c_str(), -1);
            }
            if (osAttId.size() != 0)
                aosAttIdVal.push_back( strstrType (osAttId, osAttVal) );
            osAttId = "";
            bIso8859_1 = FALSE;
            if (bSkipReadNextLine)
                goto skip_read_next_line;
        }
        else if (strncmp(pszLine, "ATVCP", 5) == 0)
        {
            char** papszTokens = CSLTokenizeString2(pszLine + 8, ";", 0);
            if (CSLCount(papszTokens) == 4)
            {
                if (strcmp(papszTokens[2], "ATT") == 0)
                {
                    CPLString osAttVal = papszTokens[3];
                    if (osAttId.size() != 0)
                        aosAttIdVal.push_back( strstrType (osAttId, osAttVal) );
                    osAttId = "";
                }
            }
            CSLDestroy(papszTokens);
        }
        else if (strncmp(pszLine, "QAPCP", 5) == 0)
        {
            char** papszTokens = CSLTokenizeString2(pszLine + 8, ";", 0);
            if (CSLCount(papszTokens) == 4)
            {
                if (strcmp(papszTokens[2], "QUP") == 0)
                {
                    osQUP_RID = papszTokens[3];
                }
            }
            CSLDestroy(papszTokens);
        }
    }

    VSIFCloseL(fp);

    return TRUE;
}

/************************************************************************/
/*                        CreateFeature()                               */
/************************************************************************/

OGRFeature* OGREDIGEODataSource::CreateFeature(const CPLString& osFEA)
{
    const std::map< CPLString, OGREDIGEOFEADesc >::iterator itFEA =
                                                        mapFEA.find(osFEA);
    if (itFEA == mapFEA.end())
    {
        CPLDebug("EDIGEO", "ERROR: Cannot find FEA %s", osFEA.c_str());
        return NULL;
    }

    const OGREDIGEOFEADesc& fea = itFEA->second;
    const std::map<CPLString,OGREDIGEOLayer*>::iterator itLyr =
                                                    mapLayer.find(fea.osSCP);
    if (itLyr != mapLayer.end())
    {
        OGREDIGEOLayer* poLayer = itLyr->second;

        OGRFeature* poFeature = new OGRFeature(poLayer->GetLayerDefn());
        poFeature->SetField(0, itFEA->first.c_str());
        for(int i=0;i<(int)fea.aosAttIdVal.size();i++)
        {
            const CPLString& id = fea.aosAttIdVal[i].first;
            const CPLString& val = fea.aosAttIdVal[i].second;
            int iIndex = poLayer->GetAttributeIndex(id);
            if (iIndex != -1)
                poFeature->SetField(iIndex, val.c_str());
            else
                CPLDebug("EDIGEO",
                         "ERROR: Cannot find attribute %s", id.c_str());
        }

        if (strcmp(poLayer->GetName(), "ID_S_OBJ_Z_1_2_2") != 0 &&
            mapQAL.size() != 0 && fea.osQUP_RID.size() != 0)
        {
            const std::map<CPLString, intintType>::iterator itQAL =
                                                        mapQAL.find(fea.osQUP_RID);
            if (itQAL != mapQAL.end())
            {
                const intintType& creationUpdateDate = itQAL->second;
                if (creationUpdateDate.first != 0)
                    poFeature->SetField("CREAT_DATE", creationUpdateDate.first);
                if (creationUpdateDate.second != 0)
                    poFeature->SetField("UPDATE_DATE", creationUpdateDate.second);
            }
        }

        poLayer->AddFeature(poFeature);

        return poFeature;
    }
    else
    {
        CPLDebug("EDIGEO", "ERROR: Cannot find layer %s", fea.osSCP.c_str());
        return NULL;
    }
}

/************************************************************************/
/*                             SetStyle()                               */
/************************************************************************/

int OGREDIGEODataSource::SetStyle(const CPLString& osFEA,
                                  OGRFeature* poFeature)
{
    /* EDIGEO PCI specific */
    /* See EDIGeO_PCI.pdf, chapter 3 "Principes généraux de */
    /* positionnement de la toponymie. */
    const char* pszATR = NULL;
    if (strcmp(poFeature->GetDefnRef()->GetName(), "ID_S_OBJ_Z_1_2_2") == 0 &&
        iATR != -1 && (pszATR = poFeature->GetFieldAsString(iATR)) != NULL)
    {
        const CPLString osATR = pszATR;
        std::map< CPLString, CPLString>::iterator itFEA_FEA =
                                                mapFEA_FEA.find(osFEA);
        if (itFEA_FEA != mapFEA_FEA.end())
        {
            const CPLString& osOBJ_LNK = itFEA_FEA->second;
            std::map< CPLString, OGREDIGEOFEADesc >::iterator itFEA_LNK =
                                                        mapFEA.find(osOBJ_LNK);
            if (itFEA_LNK != mapFEA.end())
            {
                const OGREDIGEOFEADesc& fea_lnk = itFEA_LNK->second;
                for(int j=0;j<(int)fea_lnk.aosAttIdVal.size();j++)
                {
                    if (fea_lnk.aosAttIdVal[j].first == osATR)
                    {
                        double dfAngle = 0;
                        if (iDI3 != -1 && iDI4 != -1)
                        {
                            double dfBaseVectorX =
                                poFeature->GetFieldAsDouble(iDI3);
                            double dfBaseVectorY =
                                poFeature->GetFieldAsDouble(iDI4);
                            dfAngle = atan2(dfBaseVectorY, dfBaseVectorX)
                                                                / M_PI * 180;
                            if (dfAngle < 0)
                                dfAngle += 360;
                        }
                        double dfSize = 1;
                        if (iHEI != -1)
                            dfSize = poFeature->GetFieldAsDouble(iHEI);
                        if (dfSize <= 0 || dfSize >= 100)
                            dfSize = 1;
                        const char* pszFontFamily = NULL;
                        if (iFON != -1)
                            pszFontFamily = poFeature->GetFieldAsString(iFON);

                        CPLString osStyle("LABEL(t:\"");
                        osStyle += fea_lnk.aosAttIdVal[j].second;
                        osStyle += "\"";
                        if (dfAngle != 0)
                        {
                            osStyle += ",a:";
                            osStyle += CPLString().Printf("%.1f", dfAngle);
                        }
                        if (pszFontFamily != NULL && bIncludeFontFamily)
                        {
                            osStyle += ",f:\"";
                            osStyle += pszFontFamily;
                            osStyle += "\"";
                        }
                        osStyle += ",s:";
                        osStyle += CPLString().Printf("%.1f", dfSize);
                        osStyle += ",c:#000000)";
                        poFeature->SetStyleString(osStyle);

                        poFeature->SetField(iATR_VAL,
                                            fea_lnk.aosAttIdVal[j].second);
                        poFeature->SetField(iANGLE, dfAngle);
                        poFeature->SetField(iSIZE, dfSize * dfSizeFactor);
                        poFeature->SetField(iOBJ_LNK, osOBJ_LNK);
                        poFeature->SetField(iOBJ_LNK_LAYER, fea_lnk.osSCP);

                        setLayersWithLabels.insert(fea_lnk.osSCP);

                        break;
                    }
                }
            }
        }
    }

    return TRUE;
}

/************************************************************************/
/*                           BuildPoints()                              */
/************************************************************************/

int OGREDIGEODataSource::BuildPoints()
{
    for(int i=0;i<(int)listFEA_PNO.size();i++)
    {
        const CPLString& osFEA = listFEA_PNO[i].first;
        const CPLString& osPNO = listFEA_PNO[i].second;
        const std::map< CPLString, xyPairType >::iterator itPNO =
                                                        mapPNO.find(osPNO);
        if (itPNO == mapPNO.end())
        {
            CPLDebug("EDIGEO", "Cannot find PNO %s", osPNO.c_str());
        }
        else
        {
            OGRFeature* poFeature = CreateFeature(osFEA);
            if (poFeature)
            {
                const xyPairType& pno = itPNO->second;
                OGRPoint* poPoint = new OGRPoint(pno.first, pno.second);
                if (poSRS)
                    poPoint->assignSpatialReference(poSRS);
                poFeature->SetGeometryDirectly(poPoint);

                SetStyle(osFEA, poFeature);
            }
        }
    }

    return TRUE;
}

/************************************************************************/
/*                        BuildLineStrings()                            */
/************************************************************************/

int OGREDIGEODataSource::BuildLineStrings()
{
    int i, iter;

    for(iter=0;iter<(int)listFEA_PAR.size();iter++)
    {
        const CPLString& osFEA = listFEA_PAR[iter].first;
        const strListType & aosPAR = listFEA_PAR[iter].second;
        OGRFeature* poFeature = CreateFeature(osFEA);
        if (poFeature)
        {
            OGRMultiLineString* poMulti = NULL;
            for(int k=0;k<(int)aosPAR.size();k++)
            {
                const std::map< CPLString, xyPairListType >::iterator itPAR =
                                                    mapPAR.find(aosPAR[k]);
                if (itPAR != mapPAR.end())
                {
                    const xyPairListType& arc = itPAR->second;

                    OGRLineString* poLS = new OGRLineString();
                    poLS->setNumPoints((int)arc.size());
                    for(i=0;i<(int)arc.size();i++)
                    {
                        poLS->setPoint(i, arc[i].first, arc[i].second);
                    }

                    if (poFeature->GetGeometryRef() != NULL)
                    {
                        if (poMulti == NULL)
                        {
                            OGRLineString* poPrevLS =
                                (OGRLineString*) poFeature->StealGeometry();
                            poMulti = new OGRMultiLineString();
                            poMulti->addGeometryDirectly(poPrevLS);
                            poFeature->SetGeometryDirectly(poMulti);
                        }
                        poMulti->addGeometryDirectly(poLS);
                    }
                    else
                        poFeature->SetGeometryDirectly(poLS);
                }
                else
                    CPLDebug("EDIGEO",
                             "ERROR: Cannot find ARC %s", aosPAR[k].c_str());
            }
            if (poFeature->GetGeometryRef())
                poFeature->GetGeometryRef()->assignSpatialReference(poSRS);
        }
    }

    return TRUE;
}

/************************************************************************/
/*                           BuildPolygon()                             */
/************************************************************************/

int OGREDIGEODataSource::BuildPolygon(const CPLString& osFEA,
                                      const CPLString& osPFE)
{
    int i;

    const std::map< CPLString, strListType >::iterator itPFE_PAR =
                                                    mapPFE_PAR.find(osPFE);
    if (itPFE_PAR == mapPFE_PAR.end())
    {
        CPLDebug("EDIGEO", "ERROR: Cannot find PFE %s", osPFE.c_str());
        return FALSE;
    }

    const strListType & aosPARList = itPFE_PAR->second;

/* -------------------------------------------------------------------- */
/*      Resolve arc ids to arc coordinate lists.                        */
/* -------------------------------------------------------------------- */
    std::vector< const xyPairListType *> aoPARPtrList;
    for(i=0;i<(int)aosPARList.size();i++)
    {
        const std::map< CPLString, xyPairListType >::iterator itPAR =
                                            mapPAR.find(aosPARList[i]);
        if (itPAR != mapPAR.end())
            aoPARPtrList.push_back(&(itPAR->second));
        else
            CPLDebug("EDIGEO",
                     "ERROR: Cannot find ARC %s", aosPARList[i].c_str());
    }

    if (aoPARPtrList.size() == 0)
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Now try to chain all arcs together.                             */
/* -------------------------------------------------------------------- */
    std::vector<xyPairListType> aoXYList;

    int j;
    for(j=0;j<(int)aoPARPtrList.size();j++)
    {
        if (aoPARPtrList[j] == NULL)
            continue;
        const xyPairListType& sFirstRing = *(aoPARPtrList[j]);
        const xyPairType* psNext = &(sFirstRing[sFirstRing.size()-1]);

        xyPairListType aoXY;
        for(i=0;i<(int)sFirstRing.size();i++)
            aoXY.push_back(sFirstRing[i]);
        aoPARPtrList[j] = NULL;

        int nIter = 1;
        while(aoXY[aoXY.size()-1] != aoXY[0] && nIter < (int)aoPARPtrList.size())
        {
            int bFound = FALSE;
            int bReverseSecond = FALSE;
            for(i=0;i<(int)aoPARPtrList.size();i++)
            {
                if (aoPARPtrList[i] != NULL)
                {
                    const xyPairListType& sSecondRing = *(aoPARPtrList[i]);
                    if (*psNext == sSecondRing[0])
                    {
                        bFound = TRUE;
                        bReverseSecond = FALSE;
                        break;
                    }
                    else if (*psNext == sSecondRing[sSecondRing.size()-1])
                    {
                        bFound = TRUE;
                        bReverseSecond = TRUE;
                        break;
                    }
                }
            }

            if (!bFound)
            {
                CPLDebug("EDIGEO", "Cannot find ring for FEA %s / PFE %s",
                        osFEA.c_str(), osPFE.c_str());
                break;
            }
            else
            {
                const xyPairListType& secondRing = *(aoPARPtrList[i]);
                aoPARPtrList[i] = NULL;
                if (!bReverseSecond)
                {
                    for(i=1;i<(int)secondRing.size();i++)
                        aoXY.push_back(secondRing[i]);
                    psNext = &secondRing[secondRing.size()-1];
                }
                else
                {
                    for(i=1;i<(int)secondRing.size();i++)
                        aoXY.push_back(secondRing[secondRing.size()-1-i]);
                    psNext = &secondRing[0];
                }
            }

            nIter ++;
        }

        aoXYList.push_back(aoXY);
    }

/* -------------------------------------------------------------------- */
/*      Create feature.                                                 */
/* -------------------------------------------------------------------- */
    OGRFeature* poFeature = CreateFeature(osFEA);
    if (poFeature)
    {
        std::vector<OGRGeometry*> aosPolygons;
        for(j=0;j<(int)aoXYList.size();j++)
        {
            const xyPairListType& aoXY = aoXYList[j];
            OGRLinearRing* poLS = new OGRLinearRing();
            poLS->setNumPoints((int)aoXY.size());
            for(i=0;i<(int)aoXY.size();i++)
                poLS->setPoint(i, aoXY[i].first, aoXY[i].second);
            poLS->closeRings();
            OGRPolygon* poPolygon = new OGRPolygon();
            poPolygon->addRingDirectly(poLS);
            aosPolygons.push_back(poPolygon);
        }

        int bIsValidGeometry;
        OGRGeometry* poGeom = OGRGeometryFactory::organizePolygons(
            &aosPolygons[0], (int)aosPolygons.size(),
            &bIsValidGeometry, NULL);
        if (poGeom)
        {
            if (poSRS)
                poGeom->assignSpatialReference(poSRS);
            poFeature->SetGeometryDirectly(poGeom);
        }
    }

    return TRUE;
}

/************************************************************************/
/*                          BuildPolygons()                             */
/************************************************************************/

int OGREDIGEODataSource::BuildPolygons()
{
    int iter;
    for(iter=0;iter<(int)listFEA_PFE.size();iter++)
    {
        const CPLString& osFEA = listFEA_PFE[iter].first;
        const CPLString& osPFE = listFEA_PFE[iter].second;
        BuildPolygon(osFEA, osPFE);
    }

    return TRUE;
}

/************************************************************************/
/*                  OGREDIGEOSortForQGIS()                              */
/************************************************************************/

static int OGREDIGEOSortForQGIS(const void* a, const void* b)
{
    OGREDIGEOLayer* poLayerA = *((OGREDIGEOLayer**) a);
    OGREDIGEOLayer* poLayerB = *((OGREDIGEOLayer**) b);
    int nTypeA, nTypeB;
    switch (poLayerA->GetLayerDefn()->GetGeomType())
    {
        case wkbPoint: nTypeA = 1; break;
        case wkbLineString: nTypeA = 2; break;
        case wkbPolygon: nTypeA = 3; break;
        default: nTypeA = 4; break;
    }
    switch (poLayerB->GetLayerDefn()->GetGeomType())
    {
        case wkbPoint: nTypeB = 1; break;
        case wkbLineString: nTypeB = 2; break;
        case wkbPolygon: nTypeB = 3; break;
        default: nTypeB = 4; break;
    }
    if (nTypeA == nTypeB)
    {
        int nCmp = strcmp(poLayerA->GetName(), poLayerB->GetName());
        if (nCmp == 0)
            return 0;

        static const char* apszPolyOrder[] =
            { "COMMUNE_id", "LIEUDIT_id", "SECTION_id", "SUBDSECT_id",
              "SUBDFISC_id", "PARCELLE_id", "BATIMENT_id" };
        for(int i=0;i<(int)(sizeof(apszPolyOrder)/sizeof(char*));i++)
        {
            if (strcmp(poLayerA->GetName(), apszPolyOrder[i]) == 0)
                return -1;
            if (strcmp(poLayerB->GetName(), apszPolyOrder[i]) == 0)
                return 1;
        }
        return nCmp;
    }
    else
        return nTypeB - nTypeA;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGREDIGEODataSource::Open( const char * pszFilename, int bUpdateIn)

{
    if (bUpdateIn)
    {
        return FALSE;
    }

    pszName = CPLStrdup( pszFilename );

/* -------------------------------------------------------------------- */
/*      Does this appear to be a .THF file?                             */
/* -------------------------------------------------------------------- */
    if( !EQUAL(CPLGetExtension(pszFilename), "thf") )
        return FALSE;

    fpTHF = VSIFOpenL(pszFilename, "rb");
    if (fpTHF == NULL)
        return FALSE;

    const char* pszLine;
    int i = 0;
    int bIsEDIGEO = FALSE;
    while(i < 100 && (pszLine = CPLReadLine2L(fpTHF, 81, NULL)) != NULL)
    {
        if (strcmp(pszLine, "RTYSA03:GTS") == 0)
        {
            bIsEDIGEO = TRUE;
            break;
        }
        i++;
    }

    if (!bIsEDIGEO)
    {
        VSIFCloseL(fpTHF);
        fpTHF = NULL;
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                           ReadEDIGEO()                               */
/************************************************************************/

void OGREDIGEODataSource::ReadEDIGEO()
{
    if (bHasReadEDIGEO)
        return;

    bHasReadEDIGEO = TRUE;

/* -------------------------------------------------------------------- */
/*      Read .THF file                                                  */
/* -------------------------------------------------------------------- */
    VSIFSeekL(fpTHF, 0, SEEK_SET);
    if (!ReadTHF(fpTHF))
    {
        VSIFCloseL(fpTHF);
        fpTHF = NULL;
        return;
    }
    VSIFCloseL(fpTHF);
    fpTHF = NULL;

/* -------------------------------------------------------------------- */
/*      Read .GEO file                                                  */
/* -------------------------------------------------------------------- */
    if (!ReadGEO())
        return;

/* -------------------------------------------------------------------- */
/*      Read .GEN file                                                  */
/* -------------------------------------------------------------------- */
    if (osGNN.size() != 0)
        ReadGEN();

/* -------------------------------------------------------------------- */
/*      Read .DIC file                                                  */
/* -------------------------------------------------------------------- */
    if (!ReadDIC())
        return;

/* -------------------------------------------------------------------- */
/*      Read .SCD file                                                  */
/* -------------------------------------------------------------------- */
    if (!ReadSCD())
        return;

/* -------------------------------------------------------------------- */
/*      Read .QAL file                                                  */
/* -------------------------------------------------------------------- */
    if (osQAN.size() != 0)
        ReadQAL();

/* -------------------------------------------------------------------- */
/*      Create layers from SCD definitions                              */
/* -------------------------------------------------------------------- */
    int i;
    for(i=0;i<(int)aoObjList.size();i++)
    {
        CreateLayerFromObjectDesc(aoObjList[i]);
    }

/* -------------------------------------------------------------------- */
/*      Read .VEC files and create features                             */
/* -------------------------------------------------------------------- */
    for(i=0;i<(int)aosGDN.size();i++)
    {
        ReadVEC(aosGDN[i]);

        BuildPoints();
        BuildLineStrings();
        BuildPolygons();

        mapPNO.clear();
        mapPAR.clear();
        mapFEA.clear();
        mapPFE_PAR.clear();
        listFEA_PFE.clear();
        listFEA_PAR.clear();
        listFEA_PNO.clear();
        mapFEA_FEA.clear();
    }

    mapObjects.clear();
    mapAttributes.clear();
    mapAttributesSCD.clear();
    mapQAL.clear();

/* -------------------------------------------------------------------- */
/*      Delete empty layers                                             */
/* -------------------------------------------------------------------- */
    for(i=0;i<nLayers;/*nothing*/)
    {
        if (papoLayers[i]->GetFeatureCount(TRUE) == 0)
        {
            delete papoLayers[i];
            if (i < nLayers - 1)
                memmove(papoLayers + i, papoLayers + i + 1,
                        (nLayers - i - 1) * sizeof(OGREDIGEOLayer*));
            nLayers --;
        }
        else
            i++;
    }

/* -------------------------------------------------------------------- */
/*      When added from QGIS, the layers must be ordered from           */
/*      bottom (Polygon) to top (Point) to get nice visual effect       */
/* -------------------------------------------------------------------- */
    if (CSLTestBoolean(CPLGetConfigOption("OGR_EDIGEO_SORT_FOR_QGIS", "YES")))
        qsort(papoLayers, nLayers, sizeof(OGREDIGEOLayer*), OGREDIGEOSortForQGIS);

/* -------------------------------------------------------------------- */
/*      Create a label layer for each feature layer                     */
/* -------------------------------------------------------------------- */
    if (CSLTestBoolean(CPLGetConfigOption("OGR_EDIGEO_CREATE_LABEL_LAYERS", "YES")))
        CreateLabelLayers();

    return;
}

/************************************************************************/
/*                         CreateLabelLayers()                          */
/************************************************************************/

void OGREDIGEODataSource::CreateLabelLayers()
{
    OGRLayer* poLayer = GetLayerByName("ID_S_OBJ_Z_1_2_2");
    if (poLayer == NULL)
        return;

    std::map<CPLString, OGREDIGEOLayer*> mapLayerNameToLayer;

    OGRFeature* poFeature;
    OGRFeatureDefn* poFeatureDefn = poLayer->GetLayerDefn();
    while((poFeature = poLayer->GetNextFeature()) != NULL)
    {
        const char* pszBelongingLayerName =
            poFeature->GetFieldAsString(iOBJ_LNK_LAYER);
        if (pszBelongingLayerName)
        {
            CPLString osBelongingLayerName = pszBelongingLayerName;
            std::map<CPLString, OGREDIGEOLayer*>::iterator it =
                        mapLayerNameToLayer.find(osBelongingLayerName);
            OGREDIGEOLayer* poLabelLayer;

            if (it == mapLayerNameToLayer.end())
            {
                /* Create label layer if it does not already exist */
                CPLString osLayerLabelName = osBelongingLayerName + "_LABEL";
                poLabelLayer = new OGREDIGEOLayer(this, osLayerLabelName.c_str(),
                                             wkbPoint, poSRS);
                int i;
                OGRFeatureDefn* poLabelFeatureDefn = poLabelLayer->GetLayerDefn();
                for(i=0;i<poFeatureDefn->GetFieldCount();i++)
                    poLabelFeatureDefn->AddFieldDefn(poFeatureDefn->GetFieldDefn(i));
                mapLayerNameToLayer[osBelongingLayerName] = poLabelLayer;

                papoLayers = (OGRLayer**)
                    CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
                papoLayers[nLayers] = poLabelLayer;
                nLayers ++;
            }
            else
                poLabelLayer = mapLayerNameToLayer[osBelongingLayerName];

            OGRFeature* poNewFeature = new OGRFeature(poLabelLayer->GetLayerDefn());
            poNewFeature->SetFrom(poFeature);
            poLabelLayer->AddFeature(poNewFeature);
        }
        delete poFeature;
    }

    poLayer->ResetReading();
}

