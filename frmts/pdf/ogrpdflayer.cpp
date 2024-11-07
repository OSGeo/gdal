/******************************************************************************
 *
 * Project:  PDF Translator
 * Purpose:  Implements OGRPDFDataSource class
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_pdf.h"

#ifdef HAVE_PDF_READ_SUPPORT

/************************************************************************/
/*                            OGRPDFLayer()                             */
/************************************************************************/

OGRPDFLayer::OGRPDFLayer(PDFDataset *poDSIn, const char *pszName,
                         OGRSpatialReference *poSRS,
                         OGRwkbGeometryType eGeomType)
    : OGRMemLayer(pszName, poSRS, eGeomType), poDS(poDSIn), bGeomTypeSet(FALSE),
      bGeomTypeMixed(FALSE)
{
}

/************************************************************************/
/*                              Fill()                                  */
/************************************************************************/

void OGRPDFLayer::Fill(GDALPDFArray *poArray)
{
    for (int i = 0; i < poArray->GetLength(); i++)
    {
        GDALPDFObject *poFeatureObj = poArray->Get(i);
        if (poFeatureObj == nullptr ||
            poFeatureObj->GetType() != PDFObjectType_Dictionary)
            continue;

        GDALPDFObject *poA = poFeatureObj->GetDictionary()->Get("A");
        if (!(poA != nullptr && poA->GetType() == PDFObjectType_Dictionary))
            continue;

        auto poO = poA->GetDictionary()->Get("O");
        if (!(poO && poO->GetType() == PDFObjectType_Name &&
              poO->GetName() == "UserProperties"))
            continue;

        // P is supposed to be required in A, but past GDAL versions could
        // generate features without attributes without a P array
        GDALPDFObject *poP = poA->GetDictionary()->Get("P");
        GDALPDFArray *poPArray = nullptr;
        if (poP != nullptr && poP->GetType() == PDFObjectType_Array)
            poPArray = poP->GetArray();
        else
            poP = nullptr;

        GDALPDFObject *poK = poFeatureObj->GetDictionary()->Get("K");
        int nK = -1;
        if (poK != nullptr && poK->GetType() == PDFObjectType_Int)
            nK = poK->GetInt();

        if (poP)
        {
            for (int j = 0; j < poPArray->GetLength(); j++)
            {
                GDALPDFObject *poKV = poPArray->Get(j);
                if (poKV && poKV->GetType() == PDFObjectType_Dictionary)
                {
                    GDALPDFObject *poN = poKV->GetDictionary()->Get("N");
                    GDALPDFObject *poV = poKV->GetDictionary()->Get("V");
                    if (poN != nullptr &&
                        poN->GetType() == PDFObjectType_String &&
                        poV != nullptr)
                    {
                        int nIdx = GetLayerDefn()->GetFieldIndex(
                            poN->GetString().c_str());
                        OGRFieldType eType = OFTString;
                        if (poV->GetType() == PDFObjectType_Int)
                            eType = OFTInteger;
                        else if (poV->GetType() == PDFObjectType_Real)
                            eType = OFTReal;
                        if (nIdx < 0)
                        {
                            OGRFieldDefn oField(poN->GetString().c_str(),
                                                eType);
                            CreateField(&oField);
                        }
                        else if (GetLayerDefn()
                                         ->GetFieldDefn(nIdx)
                                         ->GetType() != eType &&
                                 GetLayerDefn()
                                         ->GetFieldDefn(nIdx)
                                         ->GetType() != OFTString)
                        {
                            OGRFieldDefn oField(poN->GetString().c_str(),
                                                OFTString);
                            AlterFieldDefn(nIdx, &oField, ALTER_TYPE_FLAG);
                        }
                    }
                }
            }
        }

        OGRFeature *poFeature = new OGRFeature(GetLayerDefn());
        if (poPArray)
        {
            for (int j = 0; j < poPArray->GetLength(); j++)
            {
                GDALPDFObject *poKV = poPArray->Get(j);
                if (poKV && poKV->GetType() == PDFObjectType_Dictionary)
                {
                    GDALPDFObject *poN = poKV->GetDictionary()->Get("N");
                    GDALPDFObject *poV = poKV->GetDictionary()->Get("V");
                    if (poN != nullptr &&
                        poN->GetType() == PDFObjectType_String &&
                        poV != nullptr)
                    {
                        if (poV->GetType() == PDFObjectType_String)
                            poFeature->SetField(poN->GetString().c_str(),
                                                poV->GetString().c_str());
                        else if (poV->GetType() == PDFObjectType_Int)
                            poFeature->SetField(poN->GetString().c_str(),
                                                poV->GetInt());
                        else if (poV->GetType() == PDFObjectType_Real)
                            poFeature->SetField(poN->GetString().c_str(),
                                                poV->GetReal());
                    }
                }
            }
        }

        if (nK >= 0)
        {
            OGRGeometry *poGeom = poDS->GetGeometryFromMCID(nK);
            if (poGeom)
            {
                poGeom->assignSpatialReference(GetSpatialRef());
                poFeature->SetGeometry(poGeom);
            }
        }

        OGRGeometry *poGeom = poFeature->GetGeometryRef();
        if (!bGeomTypeMixed && poGeom != nullptr)
        {
            auto poLayerDefn = GetLayerDefn();
            if (!bGeomTypeSet)
            {
                bGeomTypeSet = TRUE;
                whileUnsealing(poLayerDefn)
                    ->SetGeomType(poGeom->getGeometryType());
            }
            else if (poLayerDefn->GetGeomType() != poGeom->getGeometryType())
            {
                bGeomTypeMixed = TRUE;
                whileUnsealing(poLayerDefn)->SetGeomType(wkbUnknown);
            }
        }
        ICreateFeature(poFeature);

        delete poFeature;
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPDFLayer::TestCapability(const char *pszCap)

{
    if (EQUAL(pszCap, OLCStringsAsUTF8))
        return TRUE;
    else
        return OGRMemLayer::TestCapability(pszCap);
}

/************************************************************************/
/*                             GetDataset()                             */
/************************************************************************/

GDALDataset *OGRPDFLayer::GetDataset()
{
    return poDS;
}

#endif /* HAVE_PDF_READ_SUPPORT */

/************************************************************************/
/*                        OGRPDFWritableLayer()                         */
/************************************************************************/

OGRPDFWritableLayer::OGRPDFWritableLayer(PDFWritableVectorDataset *poDSIn,
                                         const char *pszName,
                                         OGRSpatialReference *poSRS,
                                         OGRwkbGeometryType eGeomType)
    : OGRMemLayer(pszName, poSRS, eGeomType), poDS(poDSIn)
{
}

/************************************************************************/
/*                           ICreateFeature()                           */
/************************************************************************/

OGRErr OGRPDFWritableLayer::ICreateFeature(OGRFeature *poFeature)
{
    poDS->SetModified();
    return OGRMemLayer::ICreateFeature(poFeature);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPDFWritableLayer::TestCapability(const char *pszCap)

{
    if (EQUAL(pszCap, OLCStringsAsUTF8))
        return TRUE;
    else
        return OGRMemLayer::TestCapability(pszCap);
}

/************************************************************************/
/*                             GetDataset()                             */
/************************************************************************/

GDALDataset *OGRPDFWritableLayer::GetDataset()
{
    return poDS;
}
