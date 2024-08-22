/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRGeoJSONWriteLayer class (OGR GeoJSON Driver).
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2007, Mateusz Loskot
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

#include "ogr_geojson.h"
#include "ogrgeojsonwriter.h"

#include "cpl_vsi_virtual.h"

#include <algorithm>

/************************************************************************/
/*                         OGRGeoJSONWriteLayer()                       */
/************************************************************************/

OGRGeoJSONWriteLayer::OGRGeoJSONWriteLayer(const char *pszName,
                                           OGRwkbGeometryType eGType,
                                           CSLConstList papszOptions,
                                           bool bWriteFC_BBOXIn,
                                           OGRCoordinateTransformation *poCT,
                                           OGRGeoJSONDataSource *poDS)
    : poDS_(poDS), poFeatureDefn_(new OGRFeatureDefn(pszName)), nOutCounter_(0),
      bWriteBBOX(CPLTestBool(
          CSLFetchNameValueDef(papszOptions, "WRITE_BBOX", "FALSE"))),
      bBBOX3D(false), bWriteFC_BBOX(bWriteFC_BBOXIn),
      nSignificantFigures_(atoi(
          CSLFetchNameValueDef(papszOptions, "SIGNIFICANT_FIGURES", "-1"))),
      bRFC7946_(
          CPLTestBool(CSLFetchNameValueDef(papszOptions, "RFC7946", "FALSE"))),
      bWrapDateLine_(CPLTestBool(
          CSLFetchNameValueDef(papszOptions, "WRAPDATELINE", "YES"))),
      osForeignMembers_(
          CSLFetchNameValueDef(papszOptions, "FOREIGN_MEMBERS_FEATURE", "")),
      poCT_(poCT)
{
    if (!osForeignMembers_.empty())
    {
        // Already checked in OGRGeoJSONDataSource::ICreateLayer()
        CPLAssert(osForeignMembers_.front() == '{');
        CPLAssert(osForeignMembers_.back() == '}');
        osForeignMembers_ =
            osForeignMembers_.substr(1, osForeignMembers_.size() - 2);
    }
    poFeatureDefn_->Reference();
    poFeatureDefn_->SetGeomType(eGType);
    SetDescription(poFeatureDefn_->GetName());
    const char *pszCoordPrecision =
        CSLFetchNameValue(papszOptions, "COORDINATE_PRECISION");
    if (pszCoordPrecision)
    {
        oWriteOptions_.nXYCoordPrecision = atoi(pszCoordPrecision);
        oWriteOptions_.nZCoordPrecision = atoi(pszCoordPrecision);
    }
    else
    {
        oWriteOptions_.nXYCoordPrecision = atoi(CSLFetchNameValueDef(
            papszOptions, "XY_COORD_PRECISION", bRFC7946_ ? "7" : "-1"));
        oWriteOptions_.nZCoordPrecision = atoi(CSLFetchNameValueDef(
            papszOptions, "Z_COORD_PRECISION", bRFC7946_ ? "3" : "-1"));
    }
    oWriteOptions_.bWriteBBOX = bWriteBBOX;
    oWriteOptions_.nSignificantFigures = nSignificantFigures_;
    if (bRFC7946_)
    {
        oWriteOptions_.SetRFC7946Settings();
    }
    oWriteOptions_.SetIDOptions(papszOptions);
    oWriteOptions_.bAllowNonFiniteValues = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "WRITE_NON_FINITE_VALUES", "FALSE"));
    oWriteOptions_.bAutodetectJsonStrings = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "AUTODETECT_JSON_STRINGS", "TRUE"));
}

/************************************************************************/
/*                        ~OGRGeoJSONWriteLayer()                       */
/************************************************************************/

OGRGeoJSONWriteLayer::~OGRGeoJSONWriteLayer()
{
    FinishWriting();

    if (nullptr != poFeatureDefn_)
    {
        poFeatureDefn_->Release();
    }

    delete poCT_;
}

/************************************************************************/
/*                           FinishWriting()                            */
/************************************************************************/

void OGRGeoJSONWriteLayer::FinishWriting()
{
    if (m_nPositionBeforeFCClosed == 0)
    {
        VSILFILE *fp = poDS_->GetOutputFile();

        m_nPositionBeforeFCClosed = fp->Tell();

        VSIFPrintfL(fp, "\n]");

        if (bWriteFC_BBOX && sEnvelopeLayer.IsInit())
        {
            CPLString osBBOX = "[ ";
            char szFormat[32];
            if (oWriteOptions_.nXYCoordPrecision >= 0)
                snprintf(szFormat, sizeof(szFormat), "%%.%df",
                         oWriteOptions_.nXYCoordPrecision);
            else
                snprintf(szFormat, sizeof(szFormat), "%s", "%.15g");

            osBBOX += CPLSPrintf(szFormat, sEnvelopeLayer.MinX);
            osBBOX += ", ";
            osBBOX += CPLSPrintf(szFormat, sEnvelopeLayer.MinY);
            osBBOX += ", ";
            if (bBBOX3D)
            {
                osBBOX += CPLSPrintf(szFormat, sEnvelopeLayer.MinZ);
                osBBOX += ", ";
            }
            osBBOX += CPLSPrintf(szFormat, sEnvelopeLayer.MaxX);
            osBBOX += ", ";
            osBBOX += CPLSPrintf(szFormat, sEnvelopeLayer.MaxY);
            if (bBBOX3D)
            {
                osBBOX += ", ";
                osBBOX += CPLSPrintf(szFormat, sEnvelopeLayer.MaxZ);
            }
            osBBOX += " ]";

            if (poDS_->GetFpOutputIsSeekable() &&
                osBBOX.size() + 9 < OGRGeoJSONDataSource::SPACE_FOR_BBOX)
            {
                VSIFSeekL(fp, poDS_->GetBBOXInsertLocation(), SEEK_SET);
                VSIFPrintfL(fp, "\"bbox\": %s,", osBBOX.c_str());
                VSIFSeekL(fp, 0, SEEK_END);
            }
            else
            {
                VSIFPrintfL(fp, ",\n\"bbox\": %s", osBBOX.c_str());
            }
        }

        VSIFPrintfL(fp, "\n}\n");
        fp->Flush();
    }
}

/************************************************************************/
/*                           SyncToDisk()                               */
/************************************************************************/

OGRErr OGRGeoJSONWriteLayer::SyncToDisk()
{
    if (m_nPositionBeforeFCClosed == 0 && poDS_->GetFpOutputIsSeekable())
    {
        FinishWriting();
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGRGeoJSONWriteLayer::ICreateFeature(OGRFeature *poFeature)
{
    VSILFILE *fp = poDS_->GetOutputFile();

    OGRFeature *poFeatureToWrite;
    if (poCT_ != nullptr || bRFC7946_)
    {
        poFeatureToWrite = new OGRFeature(poFeatureDefn_);
        poFeatureToWrite->SetFrom(poFeature);
        poFeatureToWrite->SetFID(poFeature->GetFID());
        OGRGeometry *poGeometry = poFeatureToWrite->GetGeometryRef();
        if (poGeometry)
        {
            const char *const apszOptions[] = {
                bWrapDateLine_ ? "WRAPDATELINE=YES" : nullptr, nullptr};
            OGRGeometry *poNewGeom = OGRGeometryFactory::transformWithOptions(
                poGeometry, poCT_, const_cast<char **>(apszOptions),
                oTransformCache_);
            if (poNewGeom == nullptr)
            {
                delete poFeatureToWrite;
                return OGRERR_FAILURE;
            }

            OGREnvelope sEnvelope;
            poNewGeom->getEnvelope(&sEnvelope);
            if (sEnvelope.MinX < -180.0 || sEnvelope.MaxX > 180.0 ||
                sEnvelope.MinY < -90.0 || sEnvelope.MaxY > 90.0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Geometry extent outside of "
                         "[-180.0,180.0]x[-90.0,90.0] bounds");
                delete poFeatureToWrite;
                return OGRERR_FAILURE;
            }

            poFeatureToWrite->SetGeometryDirectly(poNewGeom);
        }
    }
    else
    {
        poFeatureToWrite = poFeature;
    }

    const auto IsValid = [](const OGRGeometry *poGeom)
    {
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        return poGeom->IsValid();
    };

    // Special processing to detect and repair invalid geometries due to
    // coordinate precision.
    // Normally drivers shouldn't do that as similar code is triggered by
    // setting the OGR_APPLY_GEOM_SET_PRECISION=YES configuration option by
    // the generic OGRLayer::CreateFeature() code path. But this code predates
    // its introduction and RFC99, and can be useful in RFC7946 mode due to
    // coordinate reprojection.
    OGRGeometry *poOrigGeom = poFeature->GetGeometryRef();
    if (OGRGeometryFactory::haveGEOS() &&
        oWriteOptions_.nXYCoordPrecision >= 0 && poOrigGeom &&
        wkbFlatten(poOrigGeom->getGeometryType()) != wkbPoint &&
        IsValid(poOrigGeom))
    {
        const double dfXYResolution =
            std::pow(10.0, double(-oWriteOptions_.nXYCoordPrecision));
        auto poNewGeom = std::unique_ptr<OGRGeometry>(
            poFeatureToWrite->GetGeometryRef()->clone());
        OGRGeomCoordinatePrecision sPrecision;
        sPrecision.dfXYResolution = dfXYResolution;
        poNewGeom->roundCoordinates(sPrecision);
        if (!IsValid(poNewGeom.get()))
        {
            std::unique_ptr<OGRGeometry> poValidGeom;
            if (poFeature == poFeatureToWrite)
            {
                CPLDebug("GeoJSON",
                         "Running SetPrecision() to correct an invalid "
                         "geometry due to reduced precision output");
                poValidGeom.reset(
                    poOrigGeom->SetPrecision(dfXYResolution, /* nFlags = */ 0));
            }
            else
            {
                CPLDebug("GeoJSON", "Running MakeValid() to correct an invalid "
                                    "geometry due to reduced precision output");
                poValidGeom.reset(poNewGeom->MakeValid());
                if (poValidGeom)
                {
                    auto poValidGeomRoundCoordinates =
                        std::unique_ptr<OGRGeometry>(poValidGeom->clone());
                    poValidGeomRoundCoordinates->roundCoordinates(sPrecision);
                    if (!IsValid(poValidGeomRoundCoordinates.get()))
                    {
                        CPLDebug("GeoJSON",
                                 "Running SetPrecision() to correct an invalid "
                                 "geometry due to reduced precision output");
                        auto poValidGeom2 = std::unique_ptr<OGRGeometry>(
                            poValidGeom->SetPrecision(dfXYResolution,
                                                      /* nFlags = */ 0));
                        if (poValidGeom2)
                            poValidGeom = std::move(poValidGeom2);
                    }
                }
            }
            if (poValidGeom)
            {
                if (poFeature == poFeatureToWrite)
                {
                    poFeatureToWrite = new OGRFeature(poFeatureDefn_);
                    poFeatureToWrite->SetFrom(poFeature);
                    poFeatureToWrite->SetFID(poFeature->GetFID());
                }
                poFeatureToWrite->SetGeometryDirectly(poValidGeom.release());
            }
        }
    }

    if (oWriteOptions_.bGenerateID && poFeatureToWrite->GetFID() == OGRNullFID)
    {
        poFeatureToWrite->SetFID(nOutCounter_);
    }
    json_object *poObj =
        OGRGeoJSONWriteFeature(poFeatureToWrite, oWriteOptions_);
    CPLAssert(nullptr != poObj);

    if (m_nPositionBeforeFCClosed)
    {
        // If we had called SyncToDisk() previously, undo its effects
        fp->Seek(m_nPositionBeforeFCClosed, SEEK_SET);
        m_nPositionBeforeFCClosed = 0;
    }

    if (nOutCounter_ > 0)
    {
        /* Separate "Feature" entries in "FeatureCollection" object. */
        VSIFPrintfL(fp, ",\n");
    }
    const char *pszJson = json_object_to_json_string_ext(
        poObj, JSON_C_TO_STRING_SPACED
#ifdef JSON_C_TO_STRING_NOSLASHESCAPE
                   | JSON_C_TO_STRING_NOSLASHESCAPE
#endif
    );

    OGRErr eErr = OGRERR_NONE;
    size_t nLen = strlen(pszJson);
    if (!osForeignMembers_.empty())
    {
        if (nLen > 2 && pszJson[nLen - 2] == ' ' && pszJson[nLen - 1] == '}')
        {
            nLen -= 2;
        }
        else
        {
            // should not happen
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unexpected JSON output for feature. Cannot write foreign "
                     "member");
            osForeignMembers_.clear();
        }
    }
    if (VSIFWriteL(pszJson, nLen, 1, fp) != 1)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot write feature");
        eErr = OGRERR_FAILURE;
    }
    else if (!osForeignMembers_.empty() &&
             (VSIFWriteL(", ", 2, 1, fp) != 1 ||
              VSIFWriteL(osForeignMembers_.c_str(), osForeignMembers_.size(), 1,
                         fp) != 1 ||
              VSIFWriteL("}", 1, 1, fp) != 1))
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot write feature");
        eErr = OGRERR_FAILURE;
    }

    json_object_put(poObj);

    ++nOutCounter_;

    OGRGeometry *poGeometry = poFeatureToWrite->GetGeometryRef();
    if (poGeometry != nullptr && !poGeometry->IsEmpty())
    {
        OGREnvelope3D sEnvelope = OGRGeoJSONGetBBox(poGeometry, oWriteOptions_);
        if (poGeometry->getCoordinateDimension() == 3)
            bBBOX3D = true;

        if (!sEnvelopeLayer.IsInit())
        {
            sEnvelopeLayer = sEnvelope;
        }
        else if (oWriteOptions_.bBBOXRFC7946)
        {
            const bool bEnvelopeCrossAM = (sEnvelope.MinX > sEnvelope.MaxX);
            const bool bEnvelopeLayerCrossAM =
                (sEnvelopeLayer.MinX > sEnvelopeLayer.MaxX);
            if (bEnvelopeCrossAM)
            {
                if (bEnvelopeLayerCrossAM)
                {
                    sEnvelopeLayer.MinX =
                        std::min(sEnvelopeLayer.MinX, sEnvelope.MinX);
                    sEnvelopeLayer.MaxX =
                        std::max(sEnvelopeLayer.MaxX, sEnvelope.MaxX);
                }
                else
                {
                    if (sEnvelopeLayer.MinX > 0)
                    {
                        sEnvelopeLayer.MinX =
                            std::min(sEnvelopeLayer.MinX, sEnvelope.MinX);
                        sEnvelopeLayer.MaxX = sEnvelope.MaxX;
                    }
                    else if (sEnvelopeLayer.MaxX < 0)
                    {
                        sEnvelopeLayer.MaxX =
                            std::max(sEnvelopeLayer.MaxX, sEnvelope.MaxX);
                        sEnvelopeLayer.MinX = sEnvelope.MinX;
                    }
                    else
                    {
                        sEnvelopeLayer.MinX = -180.0;
                        sEnvelopeLayer.MaxX = 180.0;
                    }
                }
            }
            else if (bEnvelopeLayerCrossAM)
            {
                if (sEnvelope.MinX > 0)
                {
                    sEnvelopeLayer.MinX =
                        std::min(sEnvelopeLayer.MinX, sEnvelope.MinX);
                }
                else if (sEnvelope.MaxX < 0)
                {
                    sEnvelopeLayer.MaxX =
                        std::max(sEnvelopeLayer.MaxX, sEnvelope.MaxX);
                }
                else
                {
                    sEnvelopeLayer.MinX = -180.0;
                    sEnvelopeLayer.MaxX = 180.0;
                }
            }
            else
            {
                sEnvelopeLayer.MinX =
                    std::min(sEnvelopeLayer.MinX, sEnvelope.MinX);
                sEnvelopeLayer.MaxX =
                    std::max(sEnvelopeLayer.MaxX, sEnvelope.MaxX);
            }

            sEnvelopeLayer.MinY = std::min(sEnvelopeLayer.MinY, sEnvelope.MinY);
            sEnvelopeLayer.MaxY = std::max(sEnvelopeLayer.MaxY, sEnvelope.MaxY);
        }
        else
        {
            sEnvelopeLayer.Merge(sEnvelope);
        }
    }

    if (poFeatureToWrite != poFeature)
        delete poFeatureToWrite;

    return eErr;
}

/************************************************************************/
/*                           CreateField()                              */
/************************************************************************/

OGRErr OGRGeoJSONWriteLayer::CreateField(const OGRFieldDefn *poField,
                                         int /* bApproxOK */)
{
    if (poFeatureDefn_->GetFieldIndexCaseSensitive(poField->GetNameRef()) >= 0)
    {
        CPLDebug("GeoJSON", "Field '%s' already present in schema",
                 poField->GetNameRef());

        // TODO - mloskot: Is this return code correct?
        return OGRERR_NONE;
    }

    poFeatureDefn_->AddFieldDefn(poField);

    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGeoJSONWriteLayer::TestCapability(const char *pszCap)
{
    if (EQUAL(pszCap, OLCCreateField))
        return TRUE;
    else if (EQUAL(pszCap, OLCSequentialWrite))
        return TRUE;
    else if (EQUAL(pszCap, OLCStringsAsUTF8))
        return TRUE;
    return FALSE;
}

/************************************************************************/
/*                            GetExtent()                               */
/************************************************************************/

OGRErr OGRGeoJSONWriteLayer::GetExtent(OGREnvelope *psExtent, int)
{
    if (sEnvelopeLayer.IsInit())
    {
        *psExtent = sEnvelopeLayer;
        return OGRERR_NONE;
    }
    return OGRERR_FAILURE;
}

/************************************************************************/
/*                             GetDataset()                             */
/************************************************************************/

GDALDataset *OGRGeoJSONWriteLayer::GetDataset()
{
    return poDS_;
}
