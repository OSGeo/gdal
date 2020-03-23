/******************************************************************************
 *
 * Project:  netCDF read/write Driver
 * Purpose:  Sentinel 3 SRAL/MWR Level 2 products
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
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

// Example product:
// https://scihub.copernicus.eu/dhus/odata/v1/Products('65b615b0-0db9-4ced-8020-eb17818f0c26')/$value
// Specification:
// https://sentinel.esa.int/documents/247904/2753172/Sentinel-3-Product-Data-Format-Specification-Level-2-Land

#include "netcdfdataset.h"

#include <limits>

#ifdef NETCDF_HAS_NC4

/************************************************************************/
/*                      Sentinel3_SRAL_MWR_Layer                        */
/************************************************************************/

class Sentinel3_SRAL_MWR_Layer final: public OGRLayer
{
        OGRFeatureDefn* m_poFDefn = nullptr;
        int m_cdfid;
        size_t m_nCurIdx = 0;
        size_t m_nFeatureCount = 0;
        CPLStringList m_aosMetadata{};
        struct VariableInfo
        {
            int varid;
            nc_type nctype;
            double scale;
            double offset;
            double nodata;
        };
        std::vector<VariableInfo> m_asVarInfo{};
        int m_iLongitude = -1;
        int m_iLatitude = -1;
        double m_dfLongScale = 1.0;
        double m_dfLongOffset = 0.0;
        double m_dfLatScale = 1.0;
        double m_dfLatOffset = 0.0;

        OGRFeature* TranslateFeature(size_t nIndex);
        OGRFeature* GetNextRawFeature();

    public:
        Sentinel3_SRAL_MWR_Layer(const std::string& name, int cdfid, int dimid);
        ~Sentinel3_SRAL_MWR_Layer();

        OGRFeatureDefn* GetLayerDefn() override { return m_poFDefn; }
        void ResetReading() override;
        OGRFeature* GetNextFeature() override;
        OGRFeature* GetFeature(GIntBig nFID) override;
        GIntBig GetFeatureCount(int bForce) override;
        int TestCapability(const char* pszCap) override;
        char** GetMetadata(const char* pszDomain) override;
        const char* GetMetadataItem(const char* pszKey, const char* pszDomain) override;
};

/************************************************************************/
/*                      Sentinel3_SRAL_MWR_Layer()                      */
/************************************************************************/

Sentinel3_SRAL_MWR_Layer::Sentinel3_SRAL_MWR_Layer(
    const std::string& name, int cdfid, int dimid):
    m_cdfid(cdfid)
{
    m_poFDefn = new OGRFeatureDefn(name.c_str());
    m_poFDefn->SetGeomType(wkbPoint);
    m_poFDefn->Reference();
    SetDescription(name.c_str());

    nc_inq_dimlen(cdfid, dimid, &m_nFeatureCount);

    int nVars = 0;
    NCDF_ERR(nc_inq(cdfid, nullptr, &nVars, nullptr, nullptr));
    for( int iVar = 0; iVar < nVars; iVar++ )
    {
        int nVarDims = 0;
        NCDF_ERR(nc_inq_varndims(cdfid, iVar, &nVarDims));
        if( nVarDims != 1 )
            continue;

        int vardimid = -1;
        NCDF_ERR(nc_inq_vardimid(cdfid, iVar, &vardimid));
        if( vardimid != dimid )
            continue;

        char szVarName[NC_MAX_NAME + 1] = {};
        NCDF_ERR(nc_inq_varname(cdfid, iVar, szVarName));

        nc_type vartype = NC_NAT;
        nc_inq_vartype(cdfid, iVar, &vartype);

        int nbAttr = 0;
        NCDF_ERR(nc_inq_varnatts(cdfid, iVar, &nbAttr));
        std::string scaleFactor;
        std::string offset;
        std::string fillValue;
        CPLStringList aosMetadata;
        for( int iAttr = 0; iAttr < nbAttr; iAttr++ )
        {
            char szAttrName[NC_MAX_NAME + 1];
            szAttrName[0] = 0;
            NCDF_ERR(nc_inq_attname(cdfid, iVar, iAttr, szAttrName));
            char *pszMetaTemp = nullptr;
            if( NCDFGetAttr(cdfid, iVar, szAttrName, &pszMetaTemp) == CE_None &&
                pszMetaTemp )
            {
                if( EQUAL(szAttrName, "scale_factor") )
                {
                    scaleFactor = pszMetaTemp;
                }
                else if( EQUAL(szAttrName, "add_offset") )
                {
                    offset = pszMetaTemp;
                }
                else if( EQUAL(szAttrName, "_FillValue") )
                {
                    fillValue = pszMetaTemp;
                }
                else if( !EQUAL(szAttrName, "coordinates") )
                {
                    aosMetadata.SetNameValue(szAttrName, pszMetaTemp);
                }
            }
            CPLFree(pszMetaTemp);
        }

        const char* pszStandardName = aosMetadata.FetchNameValue("standard_name");
        if( pszStandardName )
        {
            if( EQUAL(pszStandardName, "longitude") && vartype == NC_INT )
            {
                m_iLongitude = iVar;
                if( !scaleFactor.empty() )
                    m_dfLongScale = CPLAtof(scaleFactor.c_str());
                if( !offset.empty() )
                    m_dfLongOffset = CPLAtof(offset.c_str());
                continue;
            }
            if( EQUAL(pszStandardName, "latitude") && vartype == NC_INT )
            {
                m_iLatitude = iVar;
                if( !scaleFactor.empty() )
                    m_dfLatScale = CPLAtof(scaleFactor.c_str());
                if( !offset.empty() )
                    m_dfLatOffset = CPLAtof(offset.c_str());
                continue;
            }
        }

        for( int i = 0; i < aosMetadata.size(); i++ )
            m_aosMetadata.AddString((std::string(szVarName) + '_' + aosMetadata[i]).c_str());

        OGRFieldType eType = OFTReal;
        if( !scaleFactor.empty() )
        {
            // do nothing
        }
        else if( !offset.empty() )
        {
            // do nothing
        }
        else if( vartype == NC_BYTE || vartype == NC_SHORT ||
                 vartype == NC_INT || vartype == NC_USHORT ||
                 vartype == NC_UINT )
        {
            eType = OFTInteger;
        }
        OGRFieldDefn oField(szVarName, eType);
        m_poFDefn->AddFieldDefn(&oField);
        VariableInfo varInfo;
        varInfo.varid = iVar;
        varInfo.nctype = vartype;
        varInfo.scale = scaleFactor.empty() ? 1.0 : CPLAtof(scaleFactor.c_str());
        varInfo.offset = offset.empty() ? 0.0 : CPLAtof(offset.c_str());
        varInfo.nodata = fillValue.empty() ?
            std::numeric_limits<double>::quiet_NaN() : CPLAtof(fillValue.c_str());
        m_asVarInfo.emplace_back(varInfo);
    }
}

/************************************************************************/
/*                     ~Sentinel3_SRAL_MWR_Layer()                      */
/************************************************************************/

Sentinel3_SRAL_MWR_Layer::~Sentinel3_SRAL_MWR_Layer()
{
    m_poFDefn->Release();
}

/************************************************************************/
/*                           GetMetadata()                              */
/************************************************************************/

char** Sentinel3_SRAL_MWR_Layer::GetMetadata(const char* pszDomain)
{
    if( pszDomain == nullptr || EQUAL(pszDomain, "") )
        return m_aosMetadata.List();
    return OGRLayer::GetMetadata(pszDomain);
}

/************************************************************************/
/*                           GetMetadataItem()                          */
/************************************************************************/

const char* Sentinel3_SRAL_MWR_Layer::GetMetadataItem(const char* pszKey,
                                                      const char* pszDomain)
{
    if( pszDomain == nullptr || EQUAL(pszDomain, "") )
        return m_aosMetadata.FetchNameValue(pszKey);
    return OGRLayer::GetMetadataItem(pszKey, pszDomain);
}

/************************************************************************/
/*                           ResetReading()                             */
/************************************************************************/

void Sentinel3_SRAL_MWR_Layer::ResetReading()
{
    m_nCurIdx = 0;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig Sentinel3_SRAL_MWR_Layer::GetFeatureCount(int bForce)
{
    if( m_poFilterGeom == nullptr && m_poAttrQuery == nullptr )
        return m_nFeatureCount;
    return OGRLayer::GetFeatureCount(bForce);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int Sentinel3_SRAL_MWR_Layer::TestCapability(const char* pszCap)
{
    if( EQUAL(pszCap, OLCFastFeatureCount) )
        return m_poFilterGeom == nullptr && m_poAttrQuery == nullptr;
    if( EQUAL(pszCap, OLCRandomRead) )
        return true;
    return false;
}

/************************************************************************/
/*                        TranslateFeature()                            */
/************************************************************************/

OGRFeature* Sentinel3_SRAL_MWR_Layer::TranslateFeature(size_t nIndex)
{
    OGRFeature* poFeat = new OGRFeature(m_poFDefn);
    poFeat->SetFID(nIndex + 1);
    if( m_iLongitude >= 0 && m_iLatitude >= 0 )
    {
        int nLong = 0;
        int status = nc_get_var1_int(m_cdfid, m_iLongitude, &nIndex, &nLong);
        if( status == NC_NOERR )
        {
            int nLat = 0;
            status = nc_get_var1_int(m_cdfid, m_iLatitude, &nIndex, &nLat);
            if( status == NC_NOERR )
            {
                const double dfLong = nLong * m_dfLongScale + m_dfLongOffset;
                const double dfLat = nLat * m_dfLatScale + m_dfLatOffset;
                auto poGeom = new OGRPoint(dfLong, dfLat);
                auto poGeomField = m_poFDefn->GetGeomFieldDefn(0);
                poGeom->assignSpatialReference(poGeomField->GetSpatialRef());
                poFeat->SetGeometryDirectly(poGeom);
            }
        }
    }

    for( int i = 0; i < static_cast<int>(m_asVarInfo.size()); i++ )
    {
        if( m_asVarInfo[i].nctype == NC_BYTE )
        {
            signed char nVal = 0;
            int status = nc_get_var1_schar(m_cdfid, m_asVarInfo[i].varid,
                                           &nIndex, &nVal);
            if( status == NC_NOERR && nVal != m_asVarInfo[i].nodata )
            {
                poFeat->SetField(i,
                    nVal * m_asVarInfo[i].scale + m_asVarInfo[i].offset);
            }
        }
        else if( m_asVarInfo[i].nctype == NC_SHORT )
        {
            short nVal = 0;
            int status = nc_get_var1_short(m_cdfid, m_asVarInfo[i].varid,
                                           &nIndex, &nVal);
            if( status == NC_NOERR && nVal != m_asVarInfo[i].nodata )
            {
                poFeat->SetField(i,
                    nVal * m_asVarInfo[i].scale + m_asVarInfo[i].offset);
            }
        }
        else if( m_asVarInfo[i].nctype == NC_USHORT )
        {
            unsigned short nVal = 0;
            int status = nc_get_var1_ushort(m_cdfid, m_asVarInfo[i].varid,
                                            &nIndex, &nVal);
            if( status == NC_NOERR && nVal != m_asVarInfo[i].nodata )
            {
                poFeat->SetField(i,
                    nVal * m_asVarInfo[i].scale + m_asVarInfo[i].offset);
            }
        }
        else if( m_asVarInfo[i].nctype == NC_INT )
        {
            int nVal = 0;
            int status = nc_get_var1_int(m_cdfid, m_asVarInfo[i].varid,
                                         &nIndex, &nVal);
            if( status == NC_NOERR && nVal != m_asVarInfo[i].nodata )
            {
                poFeat->SetField(i,
                    nVal * m_asVarInfo[i].scale + m_asVarInfo[i].offset);
            }
        }
        else if( m_asVarInfo[i].nctype == NC_UINT )
        {
            unsigned int nVal = 0;
            int status = nc_get_var1_uint(m_cdfid, m_asVarInfo[i].varid,
                                          &nIndex, &nVal);
            if( status == NC_NOERR && nVal != m_asVarInfo[i].nodata )
            {
                poFeat->SetField(i,
                    nVal * m_asVarInfo[i].scale + m_asVarInfo[i].offset);
            }
        }
        else if( m_asVarInfo[i].nctype == NC_DOUBLE)
        {
            double dfVal = 0.0;
            int status = nc_get_var1_double(m_cdfid, m_asVarInfo[i].varid,
                                            &nIndex, &dfVal);
            if( status == NC_NOERR && dfVal != m_asVarInfo[i].nodata )
            {
                poFeat->SetField(i,
                    dfVal * m_asVarInfo[i].scale + m_asVarInfo[i].offset);
            }
        }
        else
        {
            CPLDebug("netCDF", "Unhandled data type %d for %s",
                     m_asVarInfo[i].nctype,
                     m_poFDefn->GetFieldDefn(i)->GetNameRef());
        }
    }

    return poFeat;
}

/************************************************************************/
/*                        GetNextRawFeature()                           */
/************************************************************************/

OGRFeature* Sentinel3_SRAL_MWR_Layer::GetNextRawFeature()
{
    if( m_nCurIdx == m_nFeatureCount )
        return nullptr;
    OGRFeature* poFeat = TranslateFeature(m_nCurIdx);
    m_nCurIdx++;
    return poFeat;
}

/************************************************************************/
/*                           GetFeature()                               */
/************************************************************************/

OGRFeature* Sentinel3_SRAL_MWR_Layer::GetFeature(GIntBig nFID)
{
    if( nFID <= 0 || static_cast<size_t>(nFID) > m_nFeatureCount )
        return nullptr;
    return TranslateFeature(static_cast<size_t>(nFID - 1));
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *Sentinel3_SRAL_MWR_Layer::GetNextFeature()
{
    while( true )
    {
        OGRFeature *poFeature = GetNextRawFeature();
        if( poFeature == nullptr )
            return nullptr;

        if( (m_poFilterGeom == nullptr
            || FilterGeometry(poFeature->GetGeomFieldRef(m_iGeomFieldFilter) ))
            && (m_poAttrQuery == nullptr
                || m_poAttrQuery->Evaluate(poFeature)) )
            return poFeature;

        delete poFeature;
    }
}

/************************************************************************/
/*                       ProcessSentinel3_SRAL_MWR()                    */
/************************************************************************/

void netCDFDataset::ProcessSentinel3_SRAL_MWR()
{
    int nDimCount = -1;
    int status = nc_inq_ndims(cdfid, &nDimCount);
    NCDF_ERR(status);
    if( status != NC_NOERR || nDimCount == 0 || nDimCount > 1000 )
        return;
    std::vector<int> dimIds(nDimCount);
    int nDimCount2 = -1;
    status = nc_inq_dimids(cdfid, &nDimCount2, &dimIds[0], FALSE);
    NCDF_ERR(status);
    if( status != NC_NOERR )
        return;
    CPLAssert(nDimCount == nDimCount2);

    OGRSpatialReference* poSRS = nullptr;
    const char* pszSemiMajor = CSLFetchNameValue(
        papszMetadata, "NC_GLOBAL#semi_major_ellipsoid_axis");
    const char* pszFlattening = CSLFetchNameValue(
        papszMetadata, "NC_GLOBAL#ellipsoid_flattening");
    if( pszSemiMajor && EQUAL(pszSemiMajor, "6378137") &&
        pszFlattening && fabs(CPLAtof(pszFlattening) - 0.00335281066474748) < 1e-16 )
    {
        int iAttr = CSLFindName(papszMetadata, "NC_GLOBAL#semi_major_ellipsoid_axis");
        if( iAttr >= 0 )
            papszMetadata = CSLRemoveStrings(papszMetadata, iAttr, 1, nullptr);
        iAttr = CSLFindName(papszMetadata, "NC_GLOBAL#ellipsoid_flattening");
        if( iAttr >= 0 )
            papszMetadata = CSLRemoveStrings(papszMetadata, iAttr, 1, nullptr);
        poSRS = new OGRSpatialReference();
        poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        poSRS->importFromEPSG(4326);
    }

    for(int i = 0; i < nDimCount; i++ )
    {
        char szDimName[NC_MAX_NAME + 1] = {};
        status = nc_inq_dimname(cdfid, dimIds[i], szDimName);
        NCDF_ERR(status);
        if( status != NC_NOERR )
            break;
        std::string name(CPLGetBasename(GetDescription()));
        name += '_';
        name += szDimName;
        std::shared_ptr<OGRLayer> poLayer(
            new Sentinel3_SRAL_MWR_Layer(name.c_str(), cdfid, dimIds[i]));
        auto poGeomField = poLayer->GetLayerDefn()->GetGeomFieldDefn(0);
        if( poGeomField )
            poGeomField->SetSpatialRef(poSRS);
        papoLayers.emplace_back(poLayer);
    }

    if( poSRS )
        poSRS->Release();
}

#endif // NETCDF_HAS_NC4