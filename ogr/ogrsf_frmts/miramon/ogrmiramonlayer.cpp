/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMiraMonLayer class.
 * Author:   Abel Pau, a.pau@creaf.uab.cat
 *
 ******************************************************************************
 * Copyright (c) 2023,  MiraMon
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


#include "ogrmiramon.h"
#include "cpl_conv.h"
#include "ogr_p.h"

#include <algorithm>

#include "mm_gdal_functions.h"  // For MM_strnzcpy()
#include "mmrdlayr.h"
#include "mm_wrlayr.h"  // For MMInitLayer()

/************************************************************************/
/*                            OGRMiraMonLayer()                         */
/************************************************************************/

OGRMiraMonLayer::OGRMiraMonLayer(const char *pszFilename, VSILFILE *fp,
                         const OGRSpatialReference *poSRS, int bUpdateIn,
                         char **papszOpenOptions)
    : poFeatureDefn(nullptr), iNextFID(0), bUpdate(CPL_TO_BOOL(bUpdateIn)),
      // Assume header complete in readonly mode.
      m_fp(fp ? fp : VSIFOpenL(pszFilename, (bUpdateIn ? "r+" : "r"))),
      papszKeyedValues(nullptr), bValidFile(false), hMMFeature(),
      hMiraMonLayer(), pMMHeader(), hLayerDB()
{

    if (m_fp == nullptr)
        return;

    /* -------------------------------------------------------------------- */
    /*      Create the feature definition                                   */
    /* -------------------------------------------------------------------- */
    poFeatureDefn = new OGRFeatureDefn(CPLGetBasename(pszFilename));
    SetDescription(poFeatureDefn->GetName());
    poFeatureDefn->Reference();

    if (bUpdate)
    {
        /* -------------------------------------------------------------------- */
        /*      Preparing to write the layer                                    */
        /* -------------------------------------------------------------------- */
        if (!STARTS_WITH(pszFilename, "/vsistdout"))
        {
            int nMMVersion;

            // reading the minimal 
            MMReadHeader(m_fp, &pMMHeader);
            MMInitFeature(&hMMFeature);

            nMMVersion = MMGetVectorVersion(&pMMHeader);
            if (nMMVersion == MM_UNKNOWN_VERSION)
                bValidFile = false;
            if (pMMHeader.aFileType[0] == 'P' &&
                pMMHeader.aFileType[1] == 'N' &&
                pMMHeader.aFileType[2] == 'T')
            {
                if (pMMHeader.Flag & MM_LAYER_3D_INFO)
                {
                    poFeatureDefn->SetGeomType(wkbPoint25D);
                    MMInitLayer(&hMiraMonLayer, pszFilename,
                        nMMVersion, NULL, MM_WRITTING_MODE);
                    hMiraMonLayer.eLT = MM_LayerType_Point3d;
                }

                else
                {
                    poFeatureDefn->SetGeomType(wkbPoint);
                    MMInitLayer(&hMiraMonLayer, pszFilename,
                        nMMVersion, NULL, MM_WRITTING_MODE);
                    hMiraMonLayer.eLT = MM_LayerType_Point;
                }
                MMInitLayerByType(&hMiraMonLayer);
                hMiraMonLayer.bIsBeenInit = 1;
                hMiraMonLayer.bIsPoint = 1;
            }
            else if (pMMHeader.aFileType[0] == 'A' &&
                pMMHeader.aFileType[1] == 'R' &&
                pMMHeader.aFileType[2] == 'C')
            {
                if (pMMHeader.Flag & MM_LAYER_3D_INFO)
                {
                    poFeatureDefn->SetGeomType(wkbLineString25D);
                    MMInitLayer(&hMiraMonLayer, pszFilename,
                        nMMVersion, NULL, MM_WRITTING_MODE);
                    hMiraMonLayer.eLT = MM_LayerType_Arc3d;
                }
                else
                {
                    poFeatureDefn->SetGeomType(wkbLineString);
                    MMInitLayer(&hMiraMonLayer, pszFilename,
                        nMMVersion, NULL, MM_WRITTING_MODE);
                    hMiraMonLayer.eLT = MM_LayerType_Arc;
                }
                MMInitLayerByType(&hMiraMonLayer);
                hMiraMonLayer.bIsBeenInit = 1;
                hMiraMonLayer.bIsArc = 1;
            }
            else if (pMMHeader.aFileType[0] == 'P' &&
                pMMHeader.aFileType[1] == 'O' &&
                pMMHeader.aFileType[2] == 'L')
            {
                // 3D
                if (pMMHeader.Flag & MM_LAYER_3D_INFO)
                {
                    if (pMMHeader.Flag & MM_LAYER_MULTIPOLYGON)
                        poFeatureDefn->SetGeomType(wkbMultiPolygon25D);
                    else
                        poFeatureDefn->SetGeomType(wkbPolygon25D);
                    MMInitLayer(&hMiraMonLayer, pszFilename,
                        nMMVersion, NULL, MM_WRITTING_MODE);
                    hMiraMonLayer.eLT = MM_LayerType_Pol3d;
                }
                else
                {
                    if (pMMHeader.Flag & MM_LAYER_MULTIPOLYGON)
                        poFeatureDefn->SetGeomType(wkbMultiPolygon);
                    else
                        poFeatureDefn->SetGeomType(wkbPolygon);
                    MMInitLayer(&hMiraMonLayer, pszFilename,
                        nMMVersion, NULL, MM_WRITTING_MODE);
                    hMiraMonLayer.eLT = MM_LayerType_Pol;
                }
                MMInitLayerByType(&hMiraMonLayer);
                hMiraMonLayer.bIsBeenInit = 1;
                hMiraMonLayer.bIsPolygon = 1;
            }
            else
            {
                // Unknown type
                MMInitLayer(&hMiraMonLayer, pszFilename,
                    nMMVersion, NULL, MM_WRITTING_MODE);
                hMiraMonLayer.bIsBeenInit = 0;
                hMiraMonLayer.bNameNeedsCorrection = 1;
            }
        }
    }
    else
    {
        /* ---------------------------------------------------------------- */
        /*      Read the header.                                            */
        /* -----------------------------------------------------------------*/
        if (!STARTS_WITH(pszFilename, "/vsistdout"))
        {
            CPLString osFieldNames;
            CPLString osFieldTypes;
            CPLString osGeometryType;
            CPLString osRegion;
            CPLString osWKT;
            CPLString osProj4;
            CPLString osEPSG;
            int nMMVersion;

            if (MMInitLayerToRead(&hMiraMonLayer, m_fp, pszFilename))
            {
                bValidFile = false;
                return;
            }

            nMMVersion = MMGetVectorVersion(&hMiraMonLayer.TopHeader);
            if (nMMVersion == MM_UNKNOWN_VERSION)
                bValidFile = false;
            if (hMiraMonLayer.bIsPoint)
            {
                if (hMiraMonLayer.TopHeader.bIs3d)
                    poFeatureDefn->SetGeomType(wkbPoint25D);
                else
                    poFeatureDefn->SetGeomType(wkbPoint);
            }
            else if (hMiraMonLayer.bIsArc && !hMiraMonLayer.bIsPolygon)
            {
                if (hMiraMonLayer.TopHeader.bIs3d)
                    poFeatureDefn->SetGeomType(wkbLineString25D);
                else
                    poFeatureDefn->SetGeomType(wkbLineString);
            }
            else if (hMiraMonLayer.bIsPolygon)
            {
                // 3D
                if (hMiraMonLayer.TopHeader.bIs3d)
                {
                    if (hMiraMonLayer.TopHeader.bIsMultipolygon)
                        poFeatureDefn->SetGeomType(wkbMultiPolygon25D);
                    else
                        poFeatureDefn->SetGeomType(wkbPolygon25D);
                }
                else
                {
                    if (hMiraMonLayer.TopHeader.bIsMultipolygon)
                        poFeatureDefn->SetGeomType(wkbMultiPolygon);
                    else
                        poFeatureDefn->SetGeomType(wkbPolygon);
                }
            }
            else
                bValidFile = false;

            if (hMiraMonLayer.TopHeader.bIs3d)
            {
                const char* szHeight = CSLFetchNameValue(papszOpenOptions, "Height");
                if (szHeight)
                {
                    if (!stricmp(szHeight, "Highest"))
                        hMiraMonLayer.nSelectCoordz = MM_SELECT_HIGHEST_COORDZ;
                    else if (!stricmp(szHeight, "Lowest"))
                        hMiraMonLayer.nSelectCoordz = MM_SELECT_LOWEST_COORDZ;
                    else
                        hMiraMonLayer.nSelectCoordz = MM_SELECT_FIRST_COORDZ;
                }
                else
                    hMiraMonLayer.nSelectCoordz = MM_SELECT_FIRST_COORDZ;
            }

            if (hMiraMonLayer.nSRS_EPSG != 0)
            {
                m_poSRS = new OGRSpatialReference();
                m_poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                if (m_poSRS->importFromEPSG(hMiraMonLayer.nSRS_EPSG) != OGRERR_NONE)
                {
                    delete m_poSRS;
                    m_poSRS = nullptr;
                }
            }

            if (hMiraMonLayer.pMMBDXP)
            {
                if(!hMiraMonLayer.pMMBDXP->pfBaseDades)
                {
		            if ( (hMiraMonLayer.pMMBDXP->pfBaseDades =
                            fopen_function(hMiraMonLayer.pMMBDXP->szNomFitxer,"r"))==NULL )
                    {
                        CPLDebug("MiraMon", "File '%s' cannot be opened.",
                            hMiraMonLayer.pMMBDXP->szNomFitxer);
                        bValidFile=false;
                    }

                    // First time we open the extended DBF we create an index to fastly find
                    // all non geometrical features.
                    hMiraMonLayer.pMultRecordIndex=MMCreateExtendedDBFIndex(
                        hMiraMonLayer.pMMBDXP->pfBaseDades,
                        hMiraMonLayer.pMMBDXP->nfitxes,
                        hMiraMonLayer.pMMBDXP->nfitxes,
                        hMiraMonLayer.pMMBDXP->OffsetPrimeraFitxa,
                        hMiraMonLayer.pMMBDXP->BytesPerFitxa,
                        hMiraMonLayer.pMMBDXP->Camp[hMiraMonLayer.pMMBDXP->CampIdGrafic].BytesAcumulats,
                        hMiraMonLayer.pMMBDXP->Camp[hMiraMonLayer.pMMBDXP->CampIdGrafic].BytesPerCamp,
                        &hMiraMonLayer.isListField);
                }

                for (MM_EXT_DBF_N_FIELDS nIField = 0; nIField < hMiraMonLayer.pMMBDXP->ncamps; nIField++)
                {
                    OGRFieldDefn oField("", OFTString);
                    oField.SetName(hMiraMonLayer.pMMBDXP->Camp[nIField].NomCamp);

                    if (hMiraMonLayer.pMMBDXP->Camp[nIField].TipusDeCamp == 'C')
                        oField.SetType(hMiraMonLayer.isListField?OFTStringList:OFTString);
                    else if (hMiraMonLayer.pMMBDXP->Camp[nIField].TipusDeCamp == 'N')
                    {
                        if (hMiraMonLayer.pMMBDXP->Camp[nIField].DecimalsSiEsFloat)
                            oField.SetType(hMiraMonLayer.isListField?OFTRealList:OFTReal);
                        else
                            oField.SetType(hMiraMonLayer.isListField?OFTIntegerList:OFTInteger);
                    }
                    else if (hMiraMonLayer.pMMBDXP->Camp[nIField].TipusDeCamp == 'D')
                        oField.SetType(OFTDateTime);

                    oField.SetWidth(hMiraMonLayer.pMMBDXP->Camp[nIField].BytesPerCamp);
                    oField.SetPrecision(hMiraMonLayer.pMMBDXP->Camp[nIField].DecimalsSiEsFloat);

                    poFeatureDefn->AddFieldDefn(&oField);
                }
            }
        }
        else
        {
            if (poSRS)
            {
                m_poSRS = poSRS->Clone();
                m_poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            }
        }

        poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(m_poSRS);
    }

    bValidFile = true;
}

/************************************************************************/
/*                           ~OGRMiraMonLayer()                           */
/************************************************************************/

OGRMiraMonLayer::~OGRMiraMonLayer()

{
    if (m_nFeaturesRead > 0 && poFeatureDefn != nullptr)
    {
        CPLDebug("MiraMon", "%d features read on layer '%s'.",
                 static_cast<int>(m_nFeaturesRead), poFeatureDefn->GetName());
    }

    /* -------------------------------------------------------------------- */
    /*      Write out the region bounds if we know where they go, and we    */
    /*      are in update mode.                                             */
    /* -------------------------------------------------------------------- */
    MMCloseLayer(&hMiraMonLayer);
	MMFreeLayer(&hMiraMonLayer);

    /* -------------------------------------------------------------------- */
    /*      Clean up.                                                       */
    /* -------------------------------------------------------------------- */
    CSLDestroy(papszKeyedValues);

    if (poFeatureDefn)
        poFeatureDefn->Release();

    if (m_poSRS)
        m_poSRS->Release();

    if (m_fp != nullptr)
        VSIFCloseL(m_fp);
}


/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRMiraMonLayer::ResetReading()

{
    if (iNextFID == 0)
        return;

    iNextFID = 0;
    VSIFSeekL(m_fp, 0, SEEK_SET);
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

void OGRMiraMonLayer::GoToFieldOfMultipleRecord(MM_INTERNAL_FID iFID,
                    MM_EXT_DBF_N_RECORDS nIRecord, MM_EXT_DBF_N_FIELDS nIField)

{
    fseek_function(hMiraMonLayer.pMMBDXP->pfBaseDades,
        hMiraMonLayer.pMultRecordIndex[iFID].offset +
        (MM_FILE_OFFSET)nIRecord * hMiraMonLayer.pMMBDXP->BytesPerFitxa +
        hMiraMonLayer.pMMBDXP->Camp[nIField].BytesAcumulats,
        SEEK_SET);
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRMiraMonLayer::GetNextRawFeature()

{
    OGRGeometry *poGeom = nullptr;
    OGRPoint *poPoint = nullptr;
    OGRLineString *poLS = nullptr;
    MM_INTERNAL_FID nIElem;
    MM_EXT_DBF_N_RECORDS nIRecord = 0;
    
    /* -------------------------------------------------------------------- */
    /*      Read iNextFID feature directly from the file.                   */
    /* -------------------------------------------------------------------- */
    if (hMiraMonLayer.bIsPolygon)
    {
        // First polygon is not returned because it's the universal polygon
        if (iNextFID+1 >= hMiraMonLayer.TopHeader.nElemCount)
            return nullptr;
        nIElem = (MM_INTERNAL_FID)iNextFID+1;
    }
    else
    {
        if(iNextFID>=hMiraMonLayer.TopHeader.nElemCount)
            return nullptr;
        nIElem = (MM_INTERNAL_FID)iNextFID;
    }
    
    switch(hMiraMonLayer.eLT)
    {
        case MM_LayerType_Point:
        case MM_LayerType_Point3d:
            // Read point
            poGeom = new OGRPoint();
            poPoint = poGeom->toPoint();

            // Get X,Y (z). MiraMon has no multipoints
            if (MMGetFeatureFromVector(&hMiraMonLayer, nIElem))
                return nullptr;

            poPoint->setX(hMiraMonLayer.ReadedFeature.pCoord[0].dfX);
            poPoint->setY(hMiraMonLayer.ReadedFeature.pCoord[0].dfY);
            if (hMiraMonLayer.TopHeader.bIs3d)
                poPoint->setZ(hMiraMonLayer.ReadedFeature.pZCoord[0]);
            break;
           
        case MM_LayerType_Arc:
        case MM_LayerType_Arc3d:
            poGeom = new OGRLineString();
            poLS = poGeom->toLineString();

            // Get X,Y (Z) n times MiraMon has no multilines
            if (MMGetFeatureFromVector(&hMiraMonLayer, nIElem))
                return nullptr;

            for (MM_N_VERTICES_TYPE nIVrt = 0; nIVrt < hMiraMonLayer.ReadedFeature.pNCoordRing[0]; nIVrt++)
            {
                if (hMiraMonLayer.TopHeader.bIs3d)
                    poLS->addPoint(hMiraMonLayer.ReadedFeature.pCoord[nIVrt].dfX,
                        hMiraMonLayer.ReadedFeature.pCoord[nIVrt].dfY,
                        hMiraMonLayer.ReadedFeature.pZCoord[nIVrt]);
                else
                    poLS->addPoint(hMiraMonLayer.ReadedFeature.pCoord[nIVrt].dfX,
                        hMiraMonLayer.ReadedFeature.pCoord[nIVrt].dfY);
            }
            break;
        
        case MM_LayerType_Pol:
        case MM_LayerType_Pol3d:
            // Read polygon
            OGRPolygon poPoly;
            MM_POLYGON_RINGS_COUNT nIRing;
            MM_N_VERTICES_TYPE nIVrtAcum;

            if (hMiraMonLayer.TopHeader.bIsMultipolygon)
            {
                OGRMultiPolygon *poMP = nullptr;

                poGeom = new OGRMultiPolygon();
                poMP = poGeom->toMultiPolygon();

                // Get X,Y (Z) n times MiraMon has no multilines
                if (MMGetFeatureFromVector(&hMiraMonLayer, nIElem))
                    return nullptr;

                nIVrtAcum = 0;
                if (!hMiraMonLayer.ReadedFeature.pbArcInfo[0])
                {
                    CPLError(CE_Failure, CPLE_NoWriteAccess,
                        "\nWrong polygon format.");
                    return nullptr;
                }
                int IAmExternal;

                for (nIRing = 0; nIRing < hMiraMonLayer.ReadedFeature.nNRings; nIRing++)
                {
                    OGRLinearRing poRing;

                    IAmExternal = hMiraMonLayer.ReadedFeature.pbArcInfo[nIRing];

                    for (MM_N_VERTICES_TYPE nIVrt = 0; nIVrt < hMiraMonLayer.ReadedFeature.pNCoordRing[nIRing]; nIVrt++)
                    {
                        if (hMiraMonLayer.TopHeader.bIs3d)
                        {
                            poRing.addPoint(hMiraMonLayer.ReadedFeature.pCoord[nIVrtAcum].dfX,
                                hMiraMonLayer.ReadedFeature.pCoord[nIVrtAcum].dfY,
                                hMiraMonLayer.ReadedFeature.pZCoord[nIVrtAcum]);
                        }
                        else
                        {
                            poRing.addPoint(hMiraMonLayer.ReadedFeature.pCoord[nIVrtAcum].dfX,
                                hMiraMonLayer.ReadedFeature.pCoord[nIVrtAcum].dfY);
                        }

                        nIVrtAcum++;
                    }

                    // If I'm going to start a new polygon...
                    if ((IAmExternal && nIRing + 1 < hMiraMonLayer.ReadedFeature.nNRings &&
                        hMiraMonLayer.ReadedFeature.pbArcInfo[nIRing + 1]) ||
                        nIRing + 1 >= hMiraMonLayer.ReadedFeature.nNRings)
                    {
                        poPoly.addRing(&poRing);
                        poMP->addGeometry(&poPoly);
                        poPoly.empty();
                    }
                    else
                        poPoly.addRing(&poRing);
                }
            }
            else
            {
                OGRPolygon *poP = nullptr;

                poGeom = new OGRPolygon();
                poP = poGeom->toPolygon();


                // Get X,Y (Z) n times MiraMon has no multilines
                if (MMGetFeatureFromVector(&hMiraMonLayer, nIElem))
                    return nullptr;

                nIVrtAcum = 0;
                if (!hMiraMonLayer.ReadedFeature.pbArcInfo[0])
                {
                    CPLError(CE_Failure, CPLE_NoWriteAccess,
                        "\nWrong polygon format.");
                    return nullptr;
                }
                int IAmExternal;

                for (nIRing = 0; nIRing < hMiraMonLayer.ReadedFeature.nNRings; nIRing++)
                {
                    OGRLinearRing poRing;

                    IAmExternal = hMiraMonLayer.ReadedFeature.pbArcInfo[nIRing];

                    for (MM_N_VERTICES_TYPE nIVrt = 0; nIVrt < hMiraMonLayer.ReadedFeature.pNCoordRing[nIRing]; nIVrt++)
                    {
                        if (hMiraMonLayer.TopHeader.bIs3d)
                        {
                            poRing.addPoint(hMiraMonLayer.ReadedFeature.pCoord[nIVrtAcum].dfX,
                                hMiraMonLayer.ReadedFeature.pCoord[nIVrtAcum].dfY,
                                hMiraMonLayer.ReadedFeature.pZCoord[nIVrtAcum]);
                        }
                        else
                        {
                            poRing.addPoint(hMiraMonLayer.ReadedFeature.pCoord[nIVrtAcum].dfX,
                                hMiraMonLayer.ReadedFeature.pCoord[nIVrtAcum].dfY);
                        }

                        nIVrtAcum++;
                    }
                    poP->addRing(&poRing);
                }
            }
            
            break;
    }

    if (poGeom == nullptr)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Create feature.                                                 */
    /* -------------------------------------------------------------------- */
    OGRFeature *poFeature = new OGRFeature(poFeatureDefn);
    poGeom->assignSpatialReference(m_poSRS);
    poFeature->SetGeometryDirectly(poGeom);
    

    /* -------------------------------------------------------------------- */
    /*      Process field values.                                           */
    /* -------------------------------------------------------------------- */
    // ·$· Hem d'agafar els valors dels camps i posar-los a poFeature
    // Accedim a la fitxa o fitxes amb el camp ID_GRAFIC a nIElem
    // Per eficiencia podem mirar si el seguent que tenim a disposicio
    // te ID_GRAFIC adient i si el té, l'usem sense buscar.
    // hMiraMonLayer.pLayerDB->pFields[hMiraMonLayer.pMMBDXP->CampIdGrafic]
    // Row of the extended DBF
    if (hMiraMonLayer.pMMBDXP)
    {
        MM_EXT_DBF_N_FIELDS nIField;

        for (nIField = 0; nIField < hMiraMonLayer.pMMBDXP->ncamps; nIField ++)
        {
            MM_ResizeStringToOperateIfNeeded(&hMiraMonLayer, hMiraMonLayer.pMMBDXP->Camp[nIField].BytesPerCamp);
            
                
            if(poFeature->GetDefnRef()->GetFieldDefn(nIField)->GetType()==OFTStringList)
            {
                char** papszValues =  // ·$· it can be optimized
                    static_cast<char**>(CPLCalloc(hMiraMonLayer.pMultRecordIndex[iNextFID].n +
                        (MM_EXT_DBF_N_RECORDS)1, sizeof(char*)));
                    
                for (nIRecord = 0; nIRecord < hMiraMonLayer.pMultRecordIndex[iNextFID].n; nIRecord++)
                {
                    GoToFieldOfMultipleRecord((MM_INTERNAL_FID)iNextFID, nIRecord, nIField);
                    memset(hMiraMonLayer.szStringToOperate, 0, hMiraMonLayer.pMMBDXP->Camp[nIField].BytesPerCamp);
                    fread_function(hMiraMonLayer.szStringToOperate,
                        hMiraMonLayer.pMMBDXP->Camp[nIField].BytesPerCamp,
                        1, hMiraMonLayer.pMMBDXP->pfBaseDades);
                    hMiraMonLayer.szStringToOperate[hMiraMonLayer.pMMBDXP->Camp[nIField].BytesPerCamp] = '\0';
                    MM_TreuBlancsDeFinalDeCadena(hMiraMonLayer.szStringToOperate);

                    papszValues[nIRecord] = CPLStrdup(hMiraMonLayer.szStringToOperate);
                }
                poFeature->SetField(nIField, papszValues);
                CSLDestroy(papszValues);
            }
            else if (poFeature->GetDefnRef()->GetFieldDefn(nIField)->GetType() == OFTString)
            {
                GoToFieldOfMultipleRecord((MM_INTERNAL_FID)iNextFID, 0, nIField);
                memset(hMiraMonLayer.szStringToOperate, 0, hMiraMonLayer.pMMBDXP->Camp[nIField].BytesPerCamp);
                fread_function(hMiraMonLayer.szStringToOperate,
                    hMiraMonLayer.pMMBDXP->Camp[nIField].BytesPerCamp,
                    1, hMiraMonLayer.pMMBDXP->pfBaseDades);
                hMiraMonLayer.szStringToOperate[hMiraMonLayer.pMMBDXP->Camp[nIField].BytesPerCamp] = '\0';

                MM_TreuBlancsDeFinalDeCadena(hMiraMonLayer.szStringToOperate);
                poFeature->SetField(nIField, hMiraMonLayer.szStringToOperate);
            }
            else if (poFeature->GetDefnRef()->GetFieldDefn(nIField)->GetType() == OFTIntegerList ||
                poFeature->GetDefnRef()->GetFieldDefn(nIField)->GetType() == OFTInteger64List ||
                poFeature->GetDefnRef()->GetFieldDefn(nIField)->GetType() == OFTRealList)
            {
                double* padfValues =
                    static_cast<double*>(CPLCalloc(hMiraMonLayer.pMultRecordIndex[iNextFID].n, sizeof(double)));
                for (nIRecord = 0; nIRecord < hMiraMonLayer.pMultRecordIndex[iNextFID].n; nIRecord++)
                {
                    GoToFieldOfMultipleRecord((MM_INTERNAL_FID)iNextFID, nIRecord, nIField);
                    memset(hMiraMonLayer.szStringToOperate, 0, hMiraMonLayer.pMMBDXP->Camp[nIField].BytesPerCamp);
                    fread_function(hMiraMonLayer.szStringToOperate,
                        hMiraMonLayer.pMMBDXP->Camp[nIField].BytesPerCamp,
                        1, hMiraMonLayer.pMMBDXP->pfBaseDades);
                    hMiraMonLayer.szStringToOperate[hMiraMonLayer.pMMBDXP->Camp[nIField].BytesPerCamp] = '\0';

                    if(hMiraMonLayer.pMMBDXP->Camp[nIField].DecimalsSiEsFloat)
                        padfValues[nIRecord] = atof(hMiraMonLayer.szStringToOperate);
                    else
                        padfValues[nIRecord] = atof(hMiraMonLayer.szStringToOperate);
                }

                if(hMiraMonLayer.pMMBDXP->Camp[nIField].DecimalsSiEsFloat>0)
                    poFeature->GetDefnRef()->GetFieldDefn(nIField)->SetType(OFTRealList);
                else
                    poFeature->GetDefnRef()->GetFieldDefn(nIField)->SetType(OFTIntegerList);
                poFeature->SetField(nIField,hMiraMonLayer.pMultRecordIndex[iNextFID].n, padfValues);
                CPLFree(padfValues);
            }
            else if (poFeature->GetDefnRef()->GetFieldDefn(nIField)->GetType() == OFTInteger ||
                poFeature->GetDefnRef()->GetFieldDefn(nIField)->GetType() == OFTInteger64 ||
                poFeature->GetDefnRef()->GetFieldDefn(nIField)->GetType() == OFTReal)
            {
                GoToFieldOfMultipleRecord((MM_INTERNAL_FID)iNextFID, 0, nIField);
                memset(hMiraMonLayer.szStringToOperate, 0, hMiraMonLayer.pMMBDXP->Camp[nIField].BytesPerCamp);
                fread_function(hMiraMonLayer.szStringToOperate,
                    hMiraMonLayer.pMMBDXP->Camp[nIField].BytesPerCamp,
                    1, hMiraMonLayer.pMMBDXP->pfBaseDades);
                hMiraMonLayer.szStringToOperate[hMiraMonLayer.pMMBDXP->Camp[nIField].BytesPerCamp] = '\0';
                MM_TreuBlancsDeFinalDeCadena(hMiraMonLayer.szStringToOperate);
                poFeature->SetField(nIField, atof(hMiraMonLayer.szStringToOperate));
            }
            else if (poFeature->GetDefnRef()->GetFieldDefn(nIField)->GetType() == OFTDate ||
                poFeature->GetDefnRef()->GetFieldDefn(nIField)->GetType() == OFTDateTime)
            {
                GoToFieldOfMultipleRecord((MM_INTERNAL_FID)iNextFID, 0, nIField);
                memset(hMiraMonLayer.szStringToOperate, 0, hMiraMonLayer.pMMBDXP->Camp[nIField].BytesPerCamp);
                fread_function(hMiraMonLayer.szStringToOperate,
                    hMiraMonLayer.pMMBDXP->Camp[nIField].BytesPerCamp,
                    1, hMiraMonLayer.pMMBDXP->pfBaseDades);
                hMiraMonLayer.szStringToOperate[hMiraMonLayer.pMMBDXP->Camp[nIField].BytesPerCamp] = '\0';

                MM_TreuBlancsDeFinalDeCadena(hMiraMonLayer.szStringToOperate);
                // ·$· To refine
                poFeature->SetField(nIField, hMiraMonLayer.szStringToOperate);   
            }
        }
    }

    poFeature->SetFID(iNextFID++);
    m_nFeaturesRead++;

    return poFeature;
}

/************************************************************************/
/*                         GetFeatureCount()                            */
/************************************************************************/
GIntBig OGRMiraMonLayer::GetFeatureCount(int bForce)
{
    if(hMiraMonLayer.bIsPolygon)
        return (GIntBig)hMiraMonLayer.TopHeader.nElemCount-1;
    else
        return (GIntBig)hMiraMonLayer.TopHeader.nElemCount;
}

/************************************************************************/
/*                           ICreateFeature()                           */
/************************************************************************/

OGRErr OGRMiraMonLayer::ICreateFeature(OGRFeature *poFeature)

{
    OGRErr eErr = OGRERR_NONE;

    if (!bUpdate)
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "\nCannot create features on read-only dataset.");
        return OGRERR_FAILURE;
    }

    /* -------------------------------------------------------------------- */
    /*      Write out the feature                                           */
    /* -------------------------------------------------------------------- */
    OGRGeometry *poGeom = poFeature->GetGeometryRef();

    if (poGeom == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "\nFeatures without geometry not supported by MiraMon writer.");
        return OGRERR_FAILURE;
    }

    if (poFeatureDefn->GetGeomType() == wkbUnknown)
        poFeatureDefn->SetGeomType(wkbFlatten(poGeom->getGeometryType()));

    if (hMiraMonLayer.eLT==MM_LayerType_Unknown)
    {
        switch (wkbFlatten(poFeatureDefn->GetGeomType()))
        {
            case wkbPoint:
		    case wkbMultiPoint:
                hMiraMonLayer.eLT=MM_LayerType_Point;
                break;
            case wkbPoint25D:
			    hMiraMonLayer.eLT=MM_LayerType_Point3d;
			    break;
		    case wkbLineString:
		    case wkbMultiLineString:
			    hMiraMonLayer.eLT=MM_LayerType_Arc;
			    break;
            case wkbLineString25D:
			    hMiraMonLayer.eLT=MM_LayerType_Arc3d;
			    break;
		    case wkbPolygon:
		    case wkbMultiPolygon:
                hMiraMonLayer.eLT=MM_LayerType_Pol;
			    break;
            case wkbPolygon25D:
            case wkbMultiPolygon25D:
                hMiraMonLayer.eLT=MM_LayerType_Pol3d;
			    break;
            case wkbGeometryCollection:
		    case wkbUnknown:
		    default:
                hMiraMonLayer.eLT=MM_LayerType_Unknown;
			    break;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Write Geometry                                                  */
    /* -------------------------------------------------------------------- */
    // Reset the object where readed coordinates are going to be stored
    MMResetFeature(&hMMFeature);

    // Reads objects with coordinates and transform them to MiraMon
    eErr = LoadGeometry(OGRGeometry::ToHandle(poGeom), true, poFeature);

    // Writes coordinates to the disk
    if(eErr == OGRERR_NONE)
        return WriteGeometry(true, poFeature);

    return eErr;
}

/************************************************************************/
/*                          DumpVertices()                              */
/************************************************************************/

OGRErr OGRMiraMonLayer::DumpVertices(OGRGeometryH hGeom,
                        bool bExternalRing, int eLT)
{
    if (MMResize_MM_N_VERTICES_TYPE_Pointer(&hMMFeature.pNCoordRing, &hMMFeature.nMaxpNCoordRing,
        hMMFeature.nNRings + 1, MM_MEAN_NUMBER_OF_RINGS, 0))
    {
        CPLError(CE_Failure, CPLE_FileIO, "\nMiraMon write failure: %s",
            VSIStrerror(errno));
        return OGRERR_FAILURE;
    }

    if (MMResizeIntPointer(&hMMFeature.pbArcInfo, &hMMFeature.nMaxpbArcInfo,
        hMMFeature.nNRings + 1, MM_MEAN_NUMBER_OF_RINGS, 0))
    {
        CPLError(CE_Failure, CPLE_FileIO, "\nMiraMon write failure: %s",
            VSIStrerror(errno));
        return OGRERR_FAILURE;
    }
    if (bExternalRing)
        hMMFeature.pbArcInfo[hMMFeature.nIRing] = 1;
    else
        hMMFeature.pbArcInfo[hMMFeature.nIRing] = 0;

    hMMFeature.pNCoordRing[hMMFeature.nIRing] = OGR_G_GetPointCount(hGeom);

    if (MMResizeMM_POINT2DPointer(&hMMFeature.pCoord, &hMMFeature.nMaxpCoord,
        hMMFeature.nICoord + hMMFeature.pNCoordRing[hMMFeature.nIRing],
        MM_MEAN_NUMBER_OF_NCOORDS, 0))
    {
        CPLError(CE_Failure, CPLE_FileIO, "\nMiraMon write failure: %s",
            VSIStrerror(errno));
        return OGRERR_FAILURE;
    }
    if (hMiraMonLayer.TopHeader.bIs3d)
    {
        if (MMResizeDoublePointer(&hMMFeature.pZCoord, &hMMFeature.nMaxpZCoord,
            hMMFeature.nICoord + hMMFeature.pNCoordRing[hMMFeature.nIRing],
            MM_MEAN_NUMBER_OF_NCOORDS, 0))
        {
            CPLError(CE_Failure, CPLE_FileIO, "\nMiraMon write failure: %s",
                VSIStrerror(errno));
            return OGRERR_FAILURE;
        }
    }

    for (int iPoint = 0; iPoint < hMMFeature.pNCoordRing[hMMFeature.nIRing]; iPoint++)
    {
        hMMFeature.pCoord[hMMFeature.nICoord].dfX = OGR_G_GetX(hGeom, iPoint);
        hMMFeature.pCoord[hMMFeature.nICoord].dfY = OGR_G_GetY(hGeom, iPoint);
        if (hMiraMonLayer.TopHeader.bIs3d && OGR_G_GetCoordinateDimension(hGeom) != 3)
        {
            CPLError(CE_Failure, CPLE_FileIO, "\nMiraMon write failure: is 3d or not?");
            return OGRERR_FAILURE;
        }
        if (hMiraMonLayer.TopHeader.bIs3d)
        {
            if (OGR_G_GetCoordinateDimension(hGeom) == 2)
                hMMFeature.pZCoord[hMMFeature.nICoord] = 0;  // Possible rare case
            else
                hMMFeature.pZCoord[hMMFeature.nICoord] = OGR_G_GetZ(hGeom, iPoint);
        }
        hMMFeature.nICoord++;
    }
    hMMFeature.nIRing++;
    hMMFeature.nNRings++;
    return OGRERR_NONE;
 }

/************************************************************************/
/*                           LoadGeometry()                             */
/*                                                                      */
/*      Loads on a MiraMon object Feature all readed coordinates        */
/*                                                                      */
/************************************************************************/
OGRErr OGRMiraMonLayer::LoadGeometry(OGRGeometryH hGeom,
                                        bool bExternalRing,
                                        OGRFeature *poFeature)

{
    OGRErr eErr = OGRERR_NONE;

    /* -------------------------------------------------------------------- */
    /*      This is a geometry with sub-geometries.                         */
    /* -------------------------------------------------------------------- */
    int nGeom=OGR_G_GetGeometryCount(hGeom);
    
    /*
        wkbPolygon[25D] --> MiraMon polygon 
        wkbMultiPoint[25D] --> N MiraMon points
        wkbMultiLineString[25D]--> N MiraMon lines
        wkbMultiPolygon[25D] --> MiraMon polygon 
        wkbGeometryCollection[25D] --> MiraMon doesn't accept mixed geometries.
    */
    int eLT=wkbFlatten(OGR_G_GetGeometryType(hGeom));

    // If the layer has unknown type let's guess it from the feature.
    if(eLT == MM_LayerType_Unknown)
        eLT=poFeatureDefn->GetGeomType();

    if (eLT == wkbMultiLineString || eLT == wkbMultiPoint)
    {
        for (int iGeom = 0; iGeom < nGeom && eErr == OGRERR_NONE; iGeom++)
        {
            OGRGeometryH poNewGeometry=OGR_G_GetGeometryRef(hGeom, iGeom);
            MMResetFeature(&hMMFeature);
            // Reads all coordinates
            eErr = LoadGeometry(poNewGeometry, true, poFeature);

            // Writes them to the disk
            if(eErr == OGRERR_NONE)
                return WriteGeometry(true, poFeature);
        }
        return eErr;
                    
    }
    else if (eLT == wkbMultiPolygon)
    {
        MMResetFeature(&hMMFeature);
        for (int iGeom = 0; iGeom < nGeom && eErr == OGRERR_NONE; iGeom++)
        {
            OGRGeometryH poNewGeometry=OGR_G_GetGeometryRef(hGeom, iGeom);
                
            // Reads all coordinates
            eErr = LoadGeometry(poNewGeometry, true, poFeature);
            if(eErr != OGRERR_NONE)
                return eErr;
        }
    }
    else if (eLT == wkbPolygon)
    {
        for (int iGeom = 0;
            iGeom < nGeom && eErr == OGRERR_NONE;
            iGeom++)
        {
            OGRGeometryH poNewGeometry=OGR_G_GetGeometryRef(hGeom, iGeom);

            if (iGeom == 0)
                bExternalRing = true;
            else
                bExternalRing = false;

            eErr = DumpVertices(poNewGeometry, bExternalRing, eLT);
            if(eErr != OGRERR_NONE)
                return eErr;
        }
    }
    else if(eLT == wkbPoint || eLT == wkbLineString)
    {
        // Reads all coordinates
        MMResetFeature(&hMMFeature);
        eErr = DumpVertices(hGeom, true, eLT);
        if(eErr != OGRERR_NONE)
            return eErr;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           WriteGeometry()                            */
/*                                                                      */
/*      Write a geometry to the file.  If bExternalRing is true it      */
/*      means the ring is being processed is external.                  */
/*                                                                      */
/************************************************************************/

OGRErr OGRMiraMonLayer::WriteGeometry(bool bExternalRing,
                                        OGRFeature *poFeature)

{
    // Field translation from GDAL to MiraMon
    if(!hMiraMonLayer.pLayerDB)
        TranslateFieldsToMM();

    // All coordinates can be written to the disk
    int result = TranslateFieldsValuesToMM(poFeature);
    if(result!=OGRERR_NONE)
        return result;

    result = AddMMFeature(&hMiraMonLayer, &hMMFeature);
        
    if(result==MM_FATAL_ERROR_WRITING_FEATURES)
    {
        CPLError(CE_Failure, CPLE_FileIO, "\nMiraMon write failure: %s",
                        VSIStrerror(errno));
        return OGRERR_FAILURE;
    }
    if(result==MM_STOP_WRITING_FEATURES)
    {
        CPLError(CE_Failure, CPLE_FileIO, "\nMiraMon format limitations.");
        CPLError(CE_Failure, CPLE_FileIO, "\nTry V2.0 option.");
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                       TranslateFieldsToMM()                          */
/*                                                                      */
/*      Translase ogr Fields to a structure that MiraMon can understand */
/*                                                                      */
/*      Returns OGRERR_NONE/OGRERR_NOT_ENOUGH_MEMORY.                   */
/************************************************************************/

OGRErr OGRMiraMonLayer::TranslateFieldsToMM()

{
    if (poFeatureDefn->GetFieldCount() == 0)
        return OGRERR_NONE;

    // If the structure i fill we do anything
    if(hMiraMonLayer.pLayerDB)
        return OGRERR_NONE;

    hMiraMonLayer.pLayerDB=
        static_cast<struct MiraMonDataBase *>(CPLCalloc(
            sizeof(*hMiraMonLayer.pLayerDB), 1));
    if(!hMiraMonLayer.pLayerDB)
        return OGRERR_NOT_ENOUGH_MEMORY;

    hMiraMonLayer.pLayerDB->pFields=
        static_cast<struct MiraMonDataBaseField *>(CPLCalloc(
            poFeatureDefn->GetFieldCount(),
            sizeof(*(hMiraMonLayer.pLayerDB->pFields))));
    if(!hMiraMonLayer.pLayerDB->pFields)
    {
	    CPLFree(hMiraMonLayer.pLayerDB);
        return OGRERR_NOT_ENOUGH_MEMORY;
    }

    hMiraMonLayer.pLayerDB->nNFields=0;
    if (hMiraMonLayer.pLayerDB->pFields)
    {
        memset(hMiraMonLayer.pLayerDB->pFields, 0,
            poFeatureDefn->GetFieldCount()*sizeof(*hMiraMonLayer.pLayerDB->pFields));
        for (MM_EXT_DBF_N_FIELDS iField = 0; iField < (MM_EXT_DBF_N_FIELDS)poFeatureDefn->GetFieldCount(); iField++)
        {
            if(!(hMiraMonLayer.pLayerDB->pFields+iField))
                continue;
            switch (poFeatureDefn->GetFieldDefn(iField)->GetType())
            {
                case OFTInteger:
                case OFTIntegerList:
                    hMiraMonLayer.pLayerDB->pFields[iField].eFieldType = MM_Numeric;
                    hMiraMonLayer.pLayerDB->pFields[iField].nNumberOfDecimals = 0;
                    break;

                case OFTInteger64:
                case OFTInteger64List:
                    hMiraMonLayer.pLayerDB->pFields[iField].bIs64BitInteger = TRUE;
                    hMiraMonLayer.pLayerDB->pFields[iField].eFieldType = MM_Numeric;
                    hMiraMonLayer.pLayerDB->pFields[iField].nNumberOfDecimals = 0;
                    break;

                case OFTReal:
                case OFTRealList:
                    hMiraMonLayer.pLayerDB->pFields[iField].eFieldType = MM_Numeric;
                    hMiraMonLayer.pLayerDB->pFields[iField].nNumberOfDecimals =
                        poFeatureDefn->GetFieldDefn(iField)->GetPrecision();
                    break;

                case OFTBinary:
                    hMiraMonLayer.pLayerDB->pFields[iField].eFieldType = MM_Logic;
                    break;
                case OFTDate:
                case OFTTime:
                case OFTDateTime:
                    hMiraMonLayer.pLayerDB->pFields[iField].eFieldType = MM_Data;
                    break;

                case OFTString:
                case OFTStringList:
                default:
                    hMiraMonLayer.pLayerDB->pFields[iField].eFieldType = MM_Character;
                    break;
            }
            if (poFeatureDefn->GetFieldDefn(iField)->GetPrecision() == 0)
            {
                hMiraMonLayer.pLayerDB->pFields[iField].nFieldSize =
                    poFeatureDefn->GetFieldDefn(iField)->GetWidth();
                if(hMiraMonLayer.pLayerDB->pFields[iField].nFieldSize == 0)
                    hMiraMonLayer.pLayerDB->pFields[iField].nFieldSize=1;
            }
            else
            {
                // One more space for the "."
                hMiraMonLayer.pLayerDB->pFields[iField].nFieldSize =
                    (unsigned int)(poFeatureDefn->GetFieldDefn(iField)->GetWidth() + 1);
            }

            if (poFeatureDefn->GetFieldDefn(iField)->GetNameRef())
            {
                // Interlis 1 encoding is ISO 8859-1 (Latin1) -> Recode from UTF-8
                char* pszString =
                    CPLRecode(poFeatureDefn->GetFieldDefn(iField)->GetNameRef(),
                        CPL_ENC_UTF8, CPL_ENC_ISO8859_1);
                for (size_t i = 0; pszString[i] != '\0'; i++)
                {
                    if (pszString[i] == ' ')
                        pszString[i] = '_';
                }
                MM_strnzcpy(hMiraMonLayer.pLayerDB->pFields[iField].pszFieldName,
                    pszString, MM_MAX_LON_FIELD_NAME_DBF);
            }
            
            if (poFeatureDefn->GetFieldDefn(iField)->GetAlternativeNameRef())
            {
                char* pszString =
                    CPLRecode(poFeatureDefn->GetFieldDefn(iField)->GetAlternativeNameRef(),
                        CPL_ENC_UTF8, CPL_ENC_ISO8859_1);
                for (size_t i = 0; pszString[i] != '\0'; i++)
                {
                    if (pszString[i] == ' ')
                        pszString[i] = '_';
                }
                MM_strnzcpy(hMiraMonLayer.pLayerDB->pFields[iField].pszFieldDescription,
                    pszString, MM_MAX_BYTES_FIELD_DESC);
                CPLFree(pszString);
            }
            hMiraMonLayer.pLayerDB->nNFields++;
        }
    }
        
    return OGRERR_NONE;
}

/************************************************************************/
/*                       TranslateFieldsValuesToMM()                    */
/*                                                                      */
/*      Translase ogr Fields to a structure that MiraMon can understand */
/*                                                                      */
/*      Returns OGRERR_NONE/OGRRERR_FAILURE/OGRERR_NOT_ENOUGH_MEMORY    */
/************************************************************************/

OGRErr OGRMiraMonLayer::TranslateFieldsValuesToMM(OGRFeature *poFeature)

{
    if (poFeatureDefn->GetFieldCount() == 0)
    {
        // MiraMon have private DataBase records
        hMMFeature.nNumRecords = 1;
        return OGRERR_NONE;
    }

    CPLString osFieldData;
    MM_EXT_DBF_N_RECORDS nIRecord;
    int nNumFields = poFeatureDefn->GetFieldCount();
    MM_EXT_DBF_N_RECORDS nNumRecords;
    hMMFeature.nNumRecords = 0;

    for (int iField = 0; iField < nNumFields; iField++)
    {
        OGRFieldType eFType =
            poFeatureDefn->GetFieldDefn(iField)->GetType();
        const char* pszRawValue = poFeature->GetFieldAsString(iField);

        if (eFType == OFTStringList)
        {
            char **panValues =
                poFeature->GetFieldAsStringList(iField);
            nNumRecords = CSLCount(panValues);
            if(nNumRecords ==0 )
                nNumRecords++;
            hMMFeature.nNumRecords = max_function(hMMFeature.nNumRecords, nNumRecords);
            if(MMResizeMiraMonRecord(&hMMFeature.pRecords, &hMMFeature.nMaxRecords,
                    hMMFeature.nNumRecords, MM_INC_NUMBER_OF_RECORDS, hMMFeature.nNumRecords))
                return OGRERR_NOT_ENOUGH_MEMORY;

            for (nIRecord = 0; nIRecord < hMMFeature.nNumRecords; nIRecord++)
            {
                hMMFeature.pRecords[nIRecord].nNumField=poFeatureDefn->GetFieldCount();

                if (MMResizeMiraMonFieldValue(&(hMMFeature.pRecords[nIRecord].pField),
                    &hMMFeature.pRecords[nIRecord].nMaxField,
                    hMMFeature.pRecords[nIRecord].nNumField,
                    MM_INC_NUMBER_OF_FIELDS, hMMFeature.pRecords[nIRecord].nNumField))
                        return OGRERR_NOT_ENOUGH_MEMORY;

                // MiraMon encoding is ISO 8859-1 (Latin1) -> Recode from UTF-8
                char *pszString =
                    CPLRecode(panValues[nIRecord], CPL_ENC_UTF8, CPL_ENC_ISO8859_1);
                /*for (size_t i = 0; pszString[i] != '\0'; i++)
                {
                    if (pszString[i] == ' ')
                        pszString[i] = '_';
                }*/
                if(MM_SecureCopyStringFieldValue(&hMMFeature.pRecords[nIRecord].pField[iField].pDinValue,
                        pszString, &hMMFeature.pRecords[nIRecord].pField[iField].nNumDinValue))
                {
                    CPLFree(pszString);
                	return OGRERR_NOT_ENOUGH_MEMORY;
                }
                CPLFree(pszString);
                hMMFeature.pRecords[nIRecord].pField[iField].bIsValid = 1;
            }
        }
        else if (eFType == OFTIntegerList)
        {
            int nCount = 0;
            const int *panValues =
                poFeature->GetFieldAsIntegerList(iField, &nCount);
            
            nNumRecords = nCount;
            if(nNumRecords ==0 )
                nNumRecords++;
            hMMFeature.nNumRecords = max_function(hMMFeature.nNumRecords, nNumRecords);
            if(MMResizeMiraMonRecord(&hMMFeature.pRecords, &hMMFeature.nMaxRecords,
                    hMMFeature.nNumRecords, MM_INC_NUMBER_OF_RECORDS, hMMFeature.nNumRecords))
                return OGRERR_NOT_ENOUGH_MEMORY;

            for (nIRecord = 0; nIRecord < hMMFeature.nNumRecords; nIRecord++)
            {
                hMMFeature.pRecords[nIRecord].nNumField=nNumFields;

                if (MMResizeMiraMonFieldValue(&(hMMFeature.pRecords[nIRecord].pField),
                    &hMMFeature.pRecords[nIRecord].nMaxField,
                    hMMFeature.pRecords[nIRecord].nNumField,
                    MM_INC_NUMBER_OF_FIELDS, hMMFeature.pRecords[nIRecord].nNumField))
                        return OGRERR_NOT_ENOUGH_MEMORY;

                hMMFeature.pRecords[nIRecord].pField[iField].dValue =
                    panValues[nIRecord];

                if(MM_SecureCopyStringFieldValue(
                        &hMMFeature.pRecords[nIRecord].pField[iField].pDinValue,
                        MMGetNFieldValue(pszRawValue, nIRecord),
                        &hMMFeature.pRecords[nIRecord].pField[iField].nNumDinValue))
                    return OGRERR_NOT_ENOUGH_MEMORY;
                                    
                hMMFeature.pRecords[nIRecord].pField[iField].bIsValid = 1;
            }
        }
        else if (eFType == OFTInteger64List)
        {
            int nCount = 0;
            const GIntBig *panValues =
                poFeature->GetFieldAsInteger64List(iField, &nCount);
            nNumRecords = nCount;
            if(nNumRecords ==0 )
                nNumRecords++;
            hMMFeature.nNumRecords = max_function(hMMFeature.nNumRecords, nNumRecords);
            if(MMResizeMiraMonRecord(&hMMFeature.pRecords, &hMMFeature.nMaxRecords,
                    hMMFeature.nNumRecords, MM_INC_NUMBER_OF_RECORDS, hMMFeature.nNumRecords))
                return OGRERR_NOT_ENOUGH_MEMORY;

            for (nIRecord = 0; nIRecord < hMMFeature.nNumRecords; nIRecord++)
            {
                hMMFeature.pRecords[nIRecord].nNumField = nNumFields;

                if (MMResizeMiraMonFieldValue(&(hMMFeature.pRecords[nIRecord].pField),
                    &hMMFeature.pRecords[nIRecord].nMaxField,
                    hMMFeature.pRecords[nIRecord].nNumField,
                    MM_INC_NUMBER_OF_FIELDS, hMMFeature.pRecords[nIRecord].nNumField))
                        return OGRERR_NOT_ENOUGH_MEMORY;

                hMMFeature.pRecords[nIRecord].pField[iField].iValue = panValues[nIRecord];
                if(MM_SecureCopyStringFieldValue(
                        &hMMFeature.pRecords[nIRecord].pField[iField].pDinValue,
                        MMGetNFieldValue(pszRawValue, nIRecord),
                        &hMMFeature.pRecords[nIRecord].pField[iField].nNumDinValue))
                    return OGRERR_NOT_ENOUGH_MEMORY;
                hMMFeature.pRecords[nIRecord].pField[iField].bIsValid = 1;
            }
        }
        else if (eFType == OFTRealList)
        {
            int nCount = 0;
            const double *panValues =
                poFeature->GetFieldAsDoubleList(iField, &nCount);
            nNumRecords = nCount;
            if(nNumRecords ==0 )
                nNumRecords++;
            hMMFeature.nNumRecords = max_function(hMMFeature.nNumRecords, nNumRecords);
            if(MMResizeMiraMonRecord(&hMMFeature.pRecords, &hMMFeature.nMaxRecords,
                    hMMFeature.nNumRecords, MM_INC_NUMBER_OF_RECORDS, hMMFeature.nNumRecords))
                return OGRERR_NOT_ENOUGH_MEMORY;

            for (nIRecord = 0; nIRecord < hMMFeature.nNumRecords; nIRecord++)
            {
                hMMFeature.pRecords[nIRecord].nNumField = iField;

                if (MMResizeMiraMonFieldValue(&(hMMFeature.pRecords[nIRecord].pField),
                    &hMMFeature.pRecords[nIRecord].nMaxField,
                    hMMFeature.pRecords[nIRecord].nNumField,
                    MM_INC_NUMBER_OF_FIELDS, hMMFeature.pRecords[nIRecord].nNumField))
                        return OGRERR_NOT_ENOUGH_MEMORY;

                hMMFeature.pRecords[nIRecord].pField[iField].dValue = panValues[nIRecord];
                if(MM_SecureCopyStringFieldValue(
                        &hMMFeature.pRecords[nIRecord].pField[iField].pDinValue,
                        MMGetNFieldValue(pszRawValue, nIRecord),
                        &hMMFeature.pRecords[nIRecord].pField[iField].nNumDinValue))
                    return OGRERR_NOT_ENOUGH_MEMORY;
                hMMFeature.pRecords[nIRecord].pField[iField].bIsValid = 1;
            }
        }
        else if (eFType == OFTString)
        {
            hMMFeature.nNumRecords = max_function(hMMFeature.nNumRecords, 1);
            hMMFeature.pRecords[0].nNumField = nNumFields;
            if (MMResizeMiraMonFieldValue(&(hMMFeature.pRecords[0].pField),
                    &hMMFeature.pRecords[0].nMaxField,
                    hMMFeature.pRecords[0].nNumField,
                    MM_INC_NUMBER_OF_FIELDS, hMMFeature.pRecords[0].nNumField))
                        return OGRERR_NOT_ENOUGH_MEMORY;

            // MiraMon encoding is ISO 8859-1 (Latin1) -> Recode from UTF-8
            char *pszString =
                CPLRecode(pszRawValue, CPL_ENC_UTF8, CPL_ENC_ISO8859_1);
            /*for (size_t i = 0; pszString[i] != '\0'; i++)
            {
                if (pszString[i] == ' ')
                    pszString[i] = '_';
            }*/
            if (MM_SecureCopyStringFieldValue(&hMMFeature.pRecords[0].pField[iField].pDinValue,
                pszString, &hMMFeature.pRecords[0].pField[iField].nNumDinValue))
            {
                CPLFree(pszString);
                return OGRERR_NOT_ENOUGH_MEMORY;
            }
            hMMFeature.pRecords[0].pField[iField].bIsValid = 1;
        }
        else if (eFType == OFTDate)
        {
            hMMFeature.nNumRecords = max_function(hMMFeature.nNumRecords, 1);
            hMMFeature.pRecords[0].nNumField = nNumFields;
             if (MMResizeMiraMonFieldValue(&(hMMFeature.pRecords[0].pField),
                    &hMMFeature.pRecords[0].nMaxField,
                    hMMFeature.pRecords[0].nNumField,
                    MM_INC_NUMBER_OF_FIELDS, hMMFeature.pRecords[0].nNumField))
                        return OGRERR_NOT_ENOUGH_MEMORY;


            const OGRField *poField = poFeature->GetRawFieldRef(iField);
            char szDate[9];
            sprintf(szDate, "%04d%02d%02d", poField->Date.Year,
                        poField->Date.Month, poField->Date.Day);
            if(MM_SecureCopyStringFieldValue(&hMMFeature.pRecords[0].pField[iField].pDinValue,
                    szDate, &hMMFeature.pRecords[0].pField[iField].nNumDinValue))
                return OGRERR_NOT_ENOUGH_MEMORY;
            hMMFeature.pRecords[0].pField[iField].bIsValid = 1;
        }
        else if (eFType == OFTInteger)
        {
            hMMFeature.nNumRecords = max_function(hMMFeature.nNumRecords, 1);
            hMMFeature.pRecords[0].nNumField = nNumFields;
             if (MMResizeMiraMonFieldValue(&(hMMFeature.pRecords[0].pField),
                    &hMMFeature.pRecords[0].nMaxField,
                    hMMFeature.pRecords[0].nNumField,
                    MM_INC_NUMBER_OF_FIELDS, hMMFeature.pRecords[0].nNumField))
                        return OGRERR_NOT_ENOUGH_MEMORY;

            hMMFeature.pRecords[0].pField[iField].dValue =
                poFeature->GetFieldAsInteger(iField);
            if(MM_SecureCopyStringFieldValue(&hMMFeature.pRecords[0].pField[iField].pDinValue,
                    pszRawValue,
                    &hMMFeature.pRecords[0].pField[iField].nNumDinValue))
                return OGRERR_NOT_ENOUGH_MEMORY;
            hMMFeature.pRecords[0].pField[iField].bIsValid = 1;
        }
        else if (eFType == OFTInteger64)
        {
            hMMFeature.nNumRecords = max_function(hMMFeature.nNumRecords, 1);
            hMMFeature.pRecords[0].nNumField = nNumFields;
             if (MMResizeMiraMonFieldValue(&(hMMFeature.pRecords[0].pField),
                    &hMMFeature.pRecords[0].nMaxField,
                    hMMFeature.pRecords[0].nNumField,
                    MM_INC_NUMBER_OF_FIELDS, hMMFeature.pRecords[0].nNumField))
                        return OGRERR_NOT_ENOUGH_MEMORY;

            hMMFeature.pRecords[0].pField[iField].iValue =
                poFeature->GetFieldAsInteger64(iField);
            if(MM_SecureCopyStringFieldValue(&hMMFeature.pRecords[0].pField[iField].pDinValue,
                    poFeature->GetFieldAsString(iField),
                    &hMMFeature.pRecords[0].pField[iField].nNumDinValue))
                return OGRERR_NOT_ENOUGH_MEMORY;
            hMMFeature.pRecords[0].pField[iField].bIsValid = 1;
        }
        else if (eFType == OFTReal)
        {
            hMMFeature.nNumRecords = max_function(hMMFeature.nNumRecords, 1);
            hMMFeature.pRecords[0].nNumField = nNumFields;
             if (MMResizeMiraMonFieldValue(&(hMMFeature.pRecords[0].pField),
                    &hMMFeature.pRecords[0].nMaxField,
                    hMMFeature.pRecords[0].nNumField,
                    MM_INC_NUMBER_OF_FIELDS, hMMFeature.pRecords[0].nNumField))
                        return OGRERR_NOT_ENOUGH_MEMORY;

            hMMFeature.pRecords[0].pField[iField].dValue =
                poFeature->GetFieldAsDouble(iField);
            if(MM_SecureCopyStringFieldValue(&hMMFeature.pRecords[0].pField[iField].pDinValue,
                    poFeature->GetFieldAsString(iField),
                    &hMMFeature.pRecords[0].pField[iField].nNumDinValue))
                return OGRERR_NOT_ENOUGH_MEMORY;
            hMMFeature.pRecords[0].pField[iField].bIsValid = 1;
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                             GetExtent()                              */
/*                                                                      */
/*      Fetch extent of the data currently stored in the dataset.       */
/*      The bForce flag has no effect on SHO files since that value     */
/*      is always in the header.                                        */
/*                                                                      */
/*      Returns OGRERR_NONE/OGRRERR_FAILURE.                            */
/************************************************************************/

OGRErr OGRMiraMonLayer::GetExtent(OGREnvelope *psExtent, int bForce)

{
    psExtent->MinX = hMiraMonLayer.TopHeader.hBB.dfMinX;
    psExtent->MaxX = hMiraMonLayer.TopHeader.hBB.dfMaxX;
    psExtent->MinY = hMiraMonLayer.TopHeader.hBB.dfMinY;
    psExtent->MaxY = hMiraMonLayer.TopHeader.hBB.dfMaxY;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRMiraMonLayer::TestCapability(const char *pszCap)

{
    if (EQUAL(pszCap, OLCRandomRead))
        return FALSE;

    if (EQUAL(pszCap, OLCSequentialWrite))
        return TRUE;

    if (EQUAL(pszCap, OLCFastGetExtent))
        return TRUE;

    if (EQUAL(pszCap, OLCCreateField))
        return TRUE;

    if (EQUAL(pszCap, OLCFastFeatureCount))
        return TRUE;
    
    if (EQUAL(pszCap, OLCZGeometries))
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRMiraMonLayer::CreateField(const OGRFieldDefn *poField, int bApproxOK)

{
    if (!bUpdate)
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "\nCannot create fields on read-only dataset.");
        return OGRERR_FAILURE;
    }

    switch (poField->GetType())
    {
        case OFTInteger:
        case OFTReal:
        case OFTString:
        case OFTDateTime:
            poFeatureDefn->AddFieldDefn(poField);
            return OGRERR_NONE;
        default:
            if (!bApproxOK)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "\nField %s is of unsupported type %s.",
                         poField->GetNameRef(),
                         poField->GetFieldTypeName(poField->GetType()));
                return OGRERR_FAILURE;
            }
            else if (poField->GetType() == OFTDate ||
                     poField->GetType() == OFTTime)
            {
                OGRFieldDefn oModDef(poField);
                oModDef.SetType(OFTDateTime);
                poFeatureDefn->AddFieldDefn(poField);
                return OGRERR_NONE;
            }
            else
            {
                OGRFieldDefn oModDef(poField);
                oModDef.SetType(OFTString);
                poFeatureDefn->AddFieldDefn(poField);
                return OGRERR_NONE;
            }
    }
}

