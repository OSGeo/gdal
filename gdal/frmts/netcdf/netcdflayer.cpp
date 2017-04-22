/******************************************************************************
 *
 * Project:  netCDF read/write Driver
 * Purpose:  GDAL bindings over netCDF library.
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault <even.rouault at spatialys.com>
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

#include "netcdfdataset.h"
#include "cpl_time.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            netCDFLayer()                             */
/************************************************************************/

netCDFLayer::netCDFLayer(netCDFDataset *poDS,
                         int nLayerCDFId,
                         const char *pszName,
                         OGRwkbGeometryType eGeomType,
                         OGRSpatialReference *poSRS) :
        m_poDS(poDS),
        m_nLayerCDFId(nLayerCDFId),
        m_poFeatureDefn(new OGRFeatureDefn(pszName)),
        m_osRecordDimName("record"),
        m_nRecordDimID(-1),
        m_nDefaultWidth(10),
        m_bAutoGrowStrings(true),
        m_nDefaultMaxWidthDimId(-1),
        m_nXVarID(-1),
        m_nYVarID(-1),
        m_nZVarID(-1),
        m_nXVarNCDFType(NC_NAT),
        m_nYVarNCDFType(NC_NAT),
        m_nZVarNCDFType(NC_NAT),
        m_osWKTVarName("ogc_wkt"),
        m_nWKTMaxWidth(10000),
        m_nWKTMaxWidthDimId(-1),
        m_nWKTVarID(-1),
        m_nWKTNCDFType(NC_NAT),
        m_nCurFeatureId(1),
        m_bWriteGDALTags(true),
        m_bUseStringInNC4(true),
        m_bNCDumpCompat(true),
        m_nProfileDimID(-1),
        m_nProfileVarID(-1),
        m_bProfileVarUnlimited(false),
        m_nParentIndexVarID(-1),
        m_poLayerConfig(NULL)
{
    m_uXVarNoData.nVal64 = 0;
    m_uYVarNoData.nVal64 = 0;
    m_uZVarNoData.nVal64 = 0;
    m_poFeatureDefn->SetGeomType(eGeomType);
    if( eGeomType != wkbNone )
        m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
    m_poFeatureDefn->Reference();
    SetDescription(pszName);
}

/************************************************************************/
/*                           ~netCDFLayer()                             */
/************************************************************************/

netCDFLayer::~netCDFLayer() { m_poFeatureDefn->Release(); }

/************************************************************************/
/*                   netCDFWriteAttributesFromConf()                    */
/************************************************************************/

static void netCDFWriteAttributesFromConf(
    int cdfid, int varid,
    const std::vector<netCDFWriterConfigAttribute> &aoAttributes)
{
    for(size_t i = 0; i < aoAttributes.size(); i++)
    {
        const netCDFWriterConfigAttribute &oAtt = aoAttributes[i];
        int status = NC_NOERR;
        if( oAtt.m_osValue.empty() )
        {
            int attid = -1;
            status = nc_inq_attid(cdfid, varid, oAtt.m_osName, &attid);
            if( status == NC_NOERR )
                status = nc_del_att(cdfid, varid, oAtt.m_osName);
            else
                status = NC_NOERR;
        }
        else if( EQUAL(oAtt.m_osType, "string") )
        {
            status = nc_put_att_text(cdfid, varid, oAtt.m_osName,
                                     oAtt.m_osValue.size(), oAtt.m_osValue);
        }
        else if( EQUAL(oAtt.m_osType, "integer") )
        {
            int nVal = atoi(oAtt.m_osValue);
            status =
                nc_put_att_int(cdfid, varid, oAtt.m_osName, NC_INT, 1, &nVal);
        }
        else if( EQUAL(oAtt.m_osType, "double") )
        {
            double dfVal = CPLAtof(oAtt.m_osValue);
            status = nc_put_att_double(cdfid, varid, oAtt.m_osName,
                                       NC_DOUBLE, 1, &dfVal);
        }
        NCDF_ERR(status);
    }
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

bool netCDFLayer::Create(char **papszOptions,
                         const netCDFWriterConfigLayer *poLayerConfig)
{
    m_osRecordDimName = CSLFetchNameValueDef(papszOptions, "RECORD_DIM_NAME",
                                             m_osRecordDimName.c_str());
    m_bAutoGrowStrings =
        CPL_TO_BOOL(CSLFetchBoolean(papszOptions, "AUTOGROW_STRINGS", TRUE));
    m_nDefaultWidth = atoi(
        CSLFetchNameValueDef(papszOptions, "STRING_DEFAULT_WIDTH",
                             CPLSPrintf("%d", m_bAutoGrowStrings ? 10 : 80)));
    m_bWriteGDALTags = CPL_TO_BOOL(
        CSLFetchBoolean(m_poDS->papszCreationOptions, "WRITE_GDAL_TAGS", TRUE));
    m_bUseStringInNC4 =
        CPL_TO_BOOL(CSLFetchBoolean(papszOptions, "USE_STRING_IN_NC4", TRUE));
    m_bNCDumpCompat =
        CPL_TO_BOOL(CSLFetchBoolean(papszOptions, "NCDUMP_COMPAT", TRUE));

    std::vector<std::pair<CPLString, int> > aoAutoVariables;

    const char *pszFeatureType =
        CSLFetchNameValue(papszOptions, "FEATURE_TYPE");
    if( pszFeatureType != NULL )
    {
        if( EQUAL(pszFeatureType, "POINT") )
        {
            if( wkbFlatten(m_poFeatureDefn->GetGeomType()) != wkbPoint )
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                         "FEATURE_TYPE=POINT only supported for Point layer "
                         "geometry type.");
            }
        }
        else if( EQUAL(pszFeatureType, "PROFILE") )
        {
            if( wkbFlatten(m_poFeatureDefn->GetGeomType()) != wkbPoint )
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                         "FEATURE_TYPE=PROFILE only supported for Point layer "
                         "geometry type.");
            }
            else
            {
                // Cf http://cfconventions.org/cf-conventions/v1.6.0/cf-conventions.html#_indexed_ragged_array_representation_of_profiles

                m_osProfileDimName = CSLFetchNameValueDef(
                    papszOptions, "PROFILE_DIM_NAME", "profile");
                m_osProfileVariables =
                    CSLFetchNameValueDef(papszOptions, "PROFILE_VARIABLES", "");

                const char *pszProfileInitSize = CSLFetchNameValueDef(
                    papszOptions, "PROFILE_DIM_INIT_SIZE",
                    (m_poDS->eFormat == NCDF_FORMAT_NC4) ? "UNLIMITED" : "100");
                m_bProfileVarUnlimited = EQUAL(pszProfileInitSize, "UNLIMITED");
                size_t nProfileInitSize =
                    m_bProfileVarUnlimited
                        ? NC_UNLIMITED
                        : static_cast<size_t>(atoi(pszProfileInitSize));
                int status = nc_def_dim(m_nLayerCDFId, m_osProfileDimName,
                                        nProfileInitSize, &m_nProfileDimID);
                NCDF_ERR(status);
                if( status != NC_NOERR )
                    return false;

                status = nc_def_var(m_nLayerCDFId, m_osProfileDimName, NC_INT,
                                    1, &m_nProfileDimID, &m_nProfileVarID);
                NCDF_ERR(status);
                if( status != NC_NOERR )
                    return false;

                aoAutoVariables.push_back(std::pair<CPLString, int>(
                    m_osProfileDimName, m_nProfileVarID));

                status =
                    nc_put_att_text(m_nLayerCDFId, m_nProfileVarID, "cf_role",
                                    strlen("profile_id"), "profile_id");
                NCDF_ERR(status);
            }
        }
        else if( !EQUAL(pszFeatureType, "AUTO") )
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "FEATURE_TYPE=%s not supported.", pszFeatureType);
        }
    }

    int status;
    if( m_bWriteGDALTags )
    {
        status = nc_put_att_text(m_nLayerCDFId, NC_GLOBAL, "ogr_layer_name",
                                 strlen(m_poFeatureDefn->GetName()),
                                 m_poFeatureDefn->GetName());
        NCDF_ERR(status);
    }

    status = nc_def_dim(m_nLayerCDFId, m_osRecordDimName,
                        NC_UNLIMITED, &m_nRecordDimID);
    NCDF_ERR(status);
    if( status != NC_NOERR )
        return false;

    if( !m_osProfileDimName.empty() )
    {
        status = nc_def_var(m_nLayerCDFId, "parentIndex", NC_INT,
                            1, &m_nRecordDimID, &m_nParentIndexVarID);
        NCDF_ERR(status);
        if( status != NC_NOERR )
            return false;

        aoAutoVariables.push_back(
            std::pair<CPLString, int>("parentIndex", m_nParentIndexVarID));

        status =
            nc_put_att_text(m_nLayerCDFId, m_nParentIndexVarID, CF_LNG_NAME,
                            strlen("index of profile"), "index of profile");
        NCDF_ERR(status);

        status = nc_put_att_text(
            m_nLayerCDFId, m_nParentIndexVarID, "instance_dimension",
            m_osProfileDimName.size(), m_osProfileDimName.c_str());
        NCDF_ERR(status);
    }

    OGRSpatialReference *poSRS = NULL;
    if( m_poFeatureDefn->GetGeomFieldCount() )
        poSRS = m_poFeatureDefn->GetGeomFieldDefn(0)->GetSpatialRef();

    if( wkbFlatten(m_poFeatureDefn->GetGeomType()) == wkbPoint )
    {
        const int nPointDim =
            !m_osProfileDimName.empty() ? m_nProfileDimID : m_nRecordDimID;
        const bool bIsGeographic = (poSRS == NULL || poSRS->IsGeographic());

        const char *pszXVarName =
            bIsGeographic ? CF_LONGITUDE_VAR_NAME : CF_PROJ_X_VAR_NAME;
        status = nc_def_var(m_nLayerCDFId, pszXVarName, NC_DOUBLE, 1,
                            &nPointDim, &m_nXVarID);
        NCDF_ERR(status);
        if( status != NC_NOERR )
        {
            return false;
        }

        const char *pszYVarName =
            bIsGeographic ? CF_LATITUDE_VAR_NAME : CF_PROJ_Y_VAR_NAME;
        status = nc_def_var(m_nLayerCDFId, pszYVarName, NC_DOUBLE, 1,
                            &nPointDim, &m_nYVarID);
        NCDF_ERR(status);
        if( status != NC_NOERR )
        {
            return false;
        }

        aoAutoVariables.push_back(
            std::pair<CPLString, int>(pszXVarName, m_nXVarID));
        aoAutoVariables.push_back(
            std::pair<CPLString, int>(pszYVarName, m_nYVarID));

        m_nXVarNCDFType = NC_DOUBLE;
        m_nYVarNCDFType = NC_DOUBLE;
        m_uXVarNoData.dfVal = NC_FILL_DOUBLE;
        m_uYVarNoData.dfVal = NC_FILL_DOUBLE;

        m_osCoordinatesValue = pszXVarName;
        m_osCoordinatesValue += " ";
        m_osCoordinatesValue += pszYVarName;

        if( poSRS == NULL || poSRS->IsGeographic() )
        {
            NCDFWriteLonLatVarsAttributes(m_nLayerCDFId, m_nXVarID, m_nYVarID);
        }
        else if (poSRS != NULL && poSRS->IsProjected() )
        {
            NCDFWriteXYVarsAttributes(m_nLayerCDFId, m_nXVarID, m_nYVarID,
                                      poSRS);
        }

        if( m_poFeatureDefn->GetGeomType() == wkbPoint25D )
        {
            const char *pszZVarName = "z";
            status = nc_def_var(m_nLayerCDFId, pszZVarName, NC_DOUBLE, 1,
                                &m_nRecordDimID, &m_nZVarID);
            NCDF_ERR(status);
            if( status != NC_NOERR )
            {
                return false;
            }

            aoAutoVariables.push_back(
                std::pair<CPLString, int>(pszZVarName, m_nZVarID));

            m_nZVarNCDFType = NC_DOUBLE;
            m_uZVarNoData.dfVal = NC_FILL_DOUBLE;

            status = nc_put_att_text(m_nLayerCDFId, m_nZVarID, CF_LNG_NAME,
                                     strlen("z coordinate"), "z coordinate");
            NCDF_ERR(status);

            status = nc_put_att_text(m_nLayerCDFId, m_nZVarID, CF_STD_NAME,
                                     strlen("height"), "height");
            NCDF_ERR(status);

            status = nc_put_att_text(m_nLayerCDFId, m_nZVarID, CF_AXIS,
                                     strlen("Z"), "Z");
            NCDF_ERR(status);

            status = nc_put_att_text(m_nLayerCDFId, m_nZVarID, CF_UNITS,
                                     strlen("m"), "m");
            NCDF_ERR(status);

            m_osCoordinatesValue += " ";
            m_osCoordinatesValue += pszZVarName;
        }

        const char *pszFeatureTypeVal =
            !m_osProfileDimName.empty() ? "profile" : "point";
        status = nc_put_att_text(m_nLayerCDFId, NC_GLOBAL, "featureType",
                                 strlen(pszFeatureTypeVal), pszFeatureTypeVal);
        NCDF_ERR(status);
    }
    else if( m_poFeatureDefn->GetGeomType() != wkbNone )
    {
#ifdef NETCDF_HAS_NC4
        if( m_poDS->eFormat == NCDF_FORMAT_NC4 && m_bUseStringInNC4 )
        {
            m_nWKTNCDFType = NC_STRING;
            status = nc_def_var(m_nLayerCDFId, m_osWKTVarName.c_str(),
                                NC_STRING, 1, &m_nRecordDimID, &m_nWKTVarID);
        }
        else
#endif
        {
            m_nWKTNCDFType = NC_CHAR;
            m_nWKTMaxWidth = atoi(CSLFetchNameValueDef(
                papszOptions, "WKT_DEFAULT_WIDTH",
                CPLSPrintf("%d", m_bAutoGrowStrings ? 1000 : 10000)));
            status =
                nc_def_dim(m_nLayerCDFId,
                           CPLSPrintf("%s_max_width", m_osWKTVarName.c_str()),
                           m_nWKTMaxWidth, &m_nWKTMaxWidthDimId);
            NCDF_ERR(status);
            if( status != NC_NOERR )
            {
                return false;
            }

            int anDims[2] = { m_nRecordDimID, m_nWKTMaxWidthDimId };
            status = nc_def_var(m_nLayerCDFId, m_osWKTVarName.c_str(), NC_CHAR,
                                2, anDims, &m_nWKTVarID);
        }
        NCDF_ERR(status);
        if( status != NC_NOERR )
        {
            return false;
        }

        aoAutoVariables.push_back(
            std::pair<CPLString, int>(m_osWKTVarName, m_nWKTVarID));

        status = nc_put_att_text(m_nLayerCDFId, m_nWKTVarID, CF_LNG_NAME,
                                 strlen("Geometry as ISO WKT"),
                                 "Geometry as ISO WKT");
        NCDF_ERR(status);

        // nc_put_att_text(m_nLayerCDFId, m_nWKTVarID, CF_UNITS,
        //                 strlen("none"), "none");

        if( m_bWriteGDALTags )
        {
            status =
                nc_put_att_text(m_nLayerCDFId, NC_GLOBAL, "ogr_geometry_field",
                                m_osWKTVarName.size(), m_osWKTVarName.c_str());
            NCDF_ERR(status);

            CPLString osGeometryType =
                OGRToOGCGeomType(m_poFeatureDefn->GetGeomType());
            if( wkbHasZ(m_poFeatureDefn->GetGeomType()) )
                osGeometryType += " Z";
            status =
                nc_put_att_text(m_nLayerCDFId, NC_GLOBAL, "ogr_layer_type",
                                osGeometryType.size(), osGeometryType.c_str());
            NCDF_ERR(status);
        }
    }

    if( poSRS != NULL )
    {
        char *pszCFProjection = NULL;
        int nSRSVarId = NCDFWriteSRSVariable(
            m_nLayerCDFId, poSRS, &pszCFProjection, m_bWriteGDALTags);
        if( nSRSVarId < 0 )
            return false;
        if( pszCFProjection != NULL )
        {
            aoAutoVariables.push_back(
                std::pair<CPLString, int>(pszCFProjection, nSRSVarId));

            m_osGridMapping = pszCFProjection;
            CPLFree(pszCFProjection);
        }

        if( m_nWKTVarID >= 0 && !m_osGridMapping.empty() )
        {
            status = nc_put_att_text(m_nLayerCDFId, m_nWKTVarID, CF_GRD_MAPPING,
                                     m_osGridMapping.size(),
                                     m_osGridMapping.c_str());
            NCDF_ERR(status);
        }
    }

    if( m_poDS->oWriterConfig.m_bIsValid )
    {
        m_poLayerConfig = poLayerConfig;

        netCDFWriteAttributesFromConf(m_nLayerCDFId, NC_GLOBAL,
                                      m_poDS->oWriterConfig.m_aoAttributes);
        if( poLayerConfig != NULL )
        {
            netCDFWriteAttributesFromConf(m_nLayerCDFId, NC_GLOBAL,
                                          poLayerConfig->m_aoAttributes);
        }

        for( size_t i = 0; i < aoAutoVariables.size(); i++ )
        {
            const netCDFWriterConfigField *poConfig = NULL;
            CPLString osLookup = "__" + aoAutoVariables[i].first;
            std::map<CPLString, netCDFWriterConfigField>::const_iterator oIter;
            if( m_poLayerConfig != NULL &&
                (oIter = m_poLayerConfig->m_oFields.find(osLookup)) !=
                    m_poLayerConfig->m_oFields.end() )
            {
                poConfig = &(oIter->second);
            }
            else if( (oIter = m_poDS->oWriterConfig.m_oFields.find(osLookup)) !=
                     m_poDS->oWriterConfig.m_oFields.end() )
            {
                poConfig = &(oIter->second);
            }

            if( poConfig != NULL )
            {
                netCDFWriteAttributesFromConf(m_nLayerCDFId,
                                              aoAutoVariables[i].second,
                                              poConfig->m_aoAttributes);
            }
        }
    }

    return true;
}

/************************************************************************/
/*                          SetRecordDimID()                            */
/************************************************************************/

void netCDFLayer::SetRecordDimID(int nRecordDimID)
{
    m_nRecordDimID = nRecordDimID;
    char szTemp[NC_MAX_NAME + 1];
    szTemp[0] = 0;
    int status = nc_inq_dimname(m_nLayerCDFId, m_nRecordDimID, szTemp);
    NCDF_ERR(status);
    m_osRecordDimName = szTemp;
}

/************************************************************************/
/*                            GetFillValue()                            */
/************************************************************************/

CPLErr netCDFLayer::GetFillValue( int nVarId, char **ppszValue )
{
    if( NCDFGetAttr(m_nLayerCDFId, nVarId, _FillValue, ppszValue) == CE_None )
        return CE_None;
    return NCDFGetAttr(m_nLayerCDFId, nVarId, "missing_value", ppszValue);
}

CPLErr netCDFLayer::GetFillValue( int nVarId, double *pdfValue )
{
    if( NCDFGetAttr(m_nLayerCDFId, nVarId, _FillValue, pdfValue) == CE_None )
        return CE_None;
    return NCDFGetAttr(m_nLayerCDFId, nVarId, "missing_value", pdfValue);
}

/************************************************************************/
/*                         GetNoDataValueForFloat()                     */
/************************************************************************/

void netCDFLayer::GetNoDataValueForFloat( int nVarId, NCDFNoDataUnion *puNoData )
{
    double dfValue;
    if( GetFillValue(nVarId, &dfValue) == CE_None )
        puNoData->fVal = static_cast<float>(dfValue);
    else
        puNoData->fVal = NC_FILL_FLOAT;
}

/************************************************************************/
/*                        GetNoDataValueForDouble()                     */
/************************************************************************/

void netCDFLayer::GetNoDataValueForDouble( int nVarId, NCDFNoDataUnion *puNoData )
{
    double dfValue;
    if( GetFillValue(nVarId, &dfValue) == CE_None )
        puNoData->dfVal = dfValue;
    else
        puNoData->dfVal = NC_FILL_DOUBLE;
}

/************************************************************************/
/*                            GetNoDataValue()                          */
/************************************************************************/

void netCDFLayer::GetNoDataValue( int nVarId, nc_type nVarType,
                                  NCDFNoDataUnion *puNoData )
{
    if( nVarType == NC_DOUBLE )
        GetNoDataValueForDouble(nVarId, puNoData);
    else if( nVarType == NC_FLOAT )
        GetNoDataValueForFloat(nVarId, puNoData);
}

/************************************************************************/
/*                             SetXYZVars()                             */
/************************************************************************/

void netCDFLayer::SetXYZVars(int nXVarId, int nYVarId, int nZVarId)
{
    m_nXVarID = nXVarId;
    m_nYVarID = nYVarId;
    m_nZVarID = nZVarId;

    nc_inq_vartype(m_nLayerCDFId, m_nXVarID, &m_nXVarNCDFType);
    nc_inq_vartype(m_nLayerCDFId, m_nYVarID, &m_nYVarNCDFType);
    if( (m_nXVarNCDFType != NC_FLOAT && m_nXVarNCDFType != NC_DOUBLE) ||
        (m_nYVarNCDFType != NC_FLOAT && m_nYVarNCDFType != NC_DOUBLE) )
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "X or Y variable of type X=%d,Y=%d not handled",
                 m_nXVarNCDFType, m_nYVarNCDFType);
        m_nXVarID = -1;
        m_nYVarID = -1;
    }
    if( m_nZVarID >= 0 )
    {
        nc_inq_vartype(m_nLayerCDFId, m_nZVarID, &m_nZVarNCDFType);
        if( m_nZVarNCDFType != NC_FLOAT && m_nZVarNCDFType != NC_DOUBLE )
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Z variable of type %d not handled", m_nZVarNCDFType);
            m_nZVarID = -1;
        }
    }

    if( m_nXVarID >= 0 && m_nYVarID >= 0 )
    {
        char szVarName[NC_MAX_NAME + 1];
        szVarName[0] = '\0';
        CPL_IGNORE_RET_VAL(nc_inq_varname(m_nLayerCDFId, m_nXVarID, szVarName));
        m_osCoordinatesValue = szVarName;

        szVarName[0] = '\0';
        CPL_IGNORE_RET_VAL(nc_inq_varname(m_nLayerCDFId, m_nYVarID, szVarName));
        m_osCoordinatesValue += " ";
        m_osCoordinatesValue += szVarName;

        if( m_nZVarID >= 0 )
        {
            szVarName[0] = '\0';
            CPL_IGNORE_RET_VAL(
                nc_inq_varname(m_nLayerCDFId, m_nZVarID, szVarName));
            m_osCoordinatesValue += " ";
            m_osCoordinatesValue += szVarName;
        }
    }

    if( m_nXVarID >= 0 )
        GetNoDataValue(m_nXVarID, m_nXVarNCDFType, &m_uXVarNoData);
    if( m_nYVarID >= 0 )
        GetNoDataValue(m_nYVarID, m_nYVarNCDFType, &m_uYVarNoData);
    if( m_nZVarID >= 0 )
        GetNoDataValue(m_nZVarID, m_nZVarNCDFType, &m_uZVarNoData);
}

/************************************************************************/
/*                       SetWKTGeometryField()                          */
/************************************************************************/

void netCDFLayer::SetWKTGeometryField(const char *pszWKTVarName)
{
    m_nWKTVarID = -1;
    nc_inq_varid(m_nLayerCDFId, pszWKTVarName, &m_nWKTVarID);
    if( m_nWKTVarID < 0 )
        return;
    int nd;
    nc_inq_varndims(m_nLayerCDFId, m_nWKTVarID, &nd);
    nc_inq_vartype(m_nLayerCDFId, m_nWKTVarID, &m_nWKTNCDFType);
#ifdef NETCDF_HAS_NC4
    if( nd == 1 && m_nWKTNCDFType == NC_STRING )
    {
        int nDimID;
        if( nc_inq_vardimid(m_nLayerCDFId, m_nWKTVarID, &nDimID ) != NC_NOERR ||
            nDimID != m_nRecordDimID )
        {
            m_nWKTVarID = -1;
            return;
        }
    }
    else
#endif
    if (nd == 2 && m_nWKTNCDFType == NC_CHAR )
    {
        int anDimIds [2] = { -1, -1 };
        size_t nLen = 0;
        if( nc_inq_vardimid(m_nLayerCDFId, m_nWKTVarID, anDimIds) != NC_NOERR ||
            anDimIds[0] != m_nRecordDimID ||
            nc_inq_dimlen(m_nLayerCDFId, anDimIds[1], &nLen) != NC_NOERR )
        {
            m_nWKTVarID = -1;
            return;
        }
        m_nWKTMaxWidth = static_cast<int>(nLen);
        m_nWKTMaxWidthDimId = anDimIds[1];
    }
    else
    {
        m_nWKTVarID = -1;
        return;
    }

    m_osWKTVarName = pszWKTVarName;
}

/************************************************************************/
/*                          SetGridMapping()                            */
/************************************************************************/

void netCDFLayer::SetGridMapping(const char *pszGridMapping)
{
    m_osGridMapping = pszGridMapping;
}

/************************************************************************/
/*                            SetProfile()                              */
/************************************************************************/

void netCDFLayer::SetProfile(int nProfileDimID, int nParentIndexVarID)
{
    m_nProfileDimID = nProfileDimID;
    m_nParentIndexVarID = nParentIndexVarID;
    if( m_nProfileDimID >= 0 )
    {
        char szTemp[NC_MAX_NAME + 1];
        szTemp[0] = 0;
        int status = nc_inq_dimname(m_nLayerCDFId, m_nProfileDimID, szTemp);
        NCDF_ERR(status);
        m_osProfileDimName = szTemp;

        nc_inq_varid(m_nLayerCDFId, m_osProfileDimName, &m_nProfileVarID);
        m_bProfileVarUnlimited = NCDFIsUnlimitedDim(
            m_poDS->eFormat == NCDF_FORMAT_NC4, m_nLayerCDFId, m_nProfileVarID);
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void netCDFLayer::ResetReading() { m_nCurFeatureId = 1; }

/************************************************************************/
/*                           Get1DVarAsDouble()                         */
/************************************************************************/

double netCDFLayer::Get1DVarAsDouble( int nVarId, nc_type nVarType,
                                      size_t nIndex, NCDFNoDataUnion noDataVal,
                                      bool *pbIsNoData )
{
    double dfVal = 0;
    if( nVarType == NC_DOUBLE )
    {
        nc_get_var1_double(m_nLayerCDFId, nVarId, &nIndex, &dfVal);
        if( pbIsNoData )
            *pbIsNoData = dfVal == noDataVal.dfVal;
    }
    else if( nVarType == NC_FLOAT )
    {
        float fVal = 0.f;
        nc_get_var1_float(m_nLayerCDFId, nVarId, &nIndex, &fVal);
        if( pbIsNoData )
            *pbIsNoData = fVal == noDataVal.fVal;
        dfVal = fVal;
    }
    else if( pbIsNoData )
        *pbIsNoData = true;
    return dfVal;
}

/************************************************************************/
/*                        GetNextRawFeature()                           */
/************************************************************************/

OGRFeature *netCDFLayer::GetNextRawFeature()
{
    m_poDS->SetDefineMode(false);

    // In update mode, nc_get_varXXX() doesn't return error if we are
    // beyond the end of dimension
    size_t nDimLen = 0;
    nc_inq_dimlen(m_nLayerCDFId, m_nRecordDimID, &nDimLen);
    if( m_nCurFeatureId > static_cast<GIntBig>(nDimLen) )
        return NULL;

    OGRFeature *poFeature = new OGRFeature(m_poFeatureDefn);

    if( m_nParentIndexVarID >= 0 )
    {
        int nProfileIdx = 0;
        size_t nIdx = static_cast<size_t>(m_nCurFeatureId - 1);
        int status = nc_get_var1_int(m_nLayerCDFId, m_nParentIndexVarID,
                                     &nIdx, &nProfileIdx);
        if( status == NC_NOERR && nProfileIdx >= 0 )
        {
            nIdx = static_cast<size_t>(nProfileIdx);
            FillFeatureFromVar(poFeature, m_nProfileDimID, nIdx);
        }
    }

    if( !FillFeatureFromVar(poFeature, m_nRecordDimID,
                            static_cast<size_t>(m_nCurFeatureId - 1)) )
    {
        m_nCurFeatureId++;
        delete poFeature;
        return NULL;
    }

    poFeature->SetFID(m_nCurFeatureId);
    m_nCurFeatureId++;

    return poFeature;
}

/************************************************************************/
/*                           FillFeatureFromVar()                       */
/************************************************************************/

bool netCDFLayer::FillFeatureFromVar(OGRFeature *poFeature, int nMainDimId,
                                     size_t nIndex)
{
    size_t anIndex[2];
    anIndex[0] = nIndex;
    anIndex[1] = 0;

    for( int i = 0; i < m_poFeatureDefn->GetFieldCount(); i++ )
    {
        if( m_aoFieldDesc[i].nMainDimId != nMainDimId )
            continue;

        switch( m_aoFieldDesc[i].nType )
        {
        case NC_CHAR:
        {
            if( m_aoFieldDesc[i].nDimCount == 1 )
            {
                char szVal[2] = { 0, 0 };
                int status = nc_get_var1_text(
                    m_nLayerCDFId, m_aoFieldDesc[i].nVarId, anIndex, szVal);
                if( status != NC_NOERR )
                {
                    NCDF_ERR(status);
                    continue;
                }
                poFeature->SetField(i, szVal);
            }
            else
            {
                size_t anCount[2];
                anCount[0] = 1;
                anCount[1] = 0;
                nc_inq_dimlen(m_nLayerCDFId, m_aoFieldDesc[i].nSecDimId,
                              &(anCount[1]));
                char *pszVal = (char *)CPLCalloc(1, anCount[1] + 1);
                int status =
                    nc_get_vara_text(m_nLayerCDFId, m_aoFieldDesc[i].nVarId,
                                     anIndex, anCount, pszVal);
                if( status != NC_NOERR )
                {
                    NCDF_ERR(status);
                    CPLFree(pszVal);
                    continue;
                }
                poFeature->SetField(i, pszVal);
                CPLFree(pszVal);
            }
            break;
        }

#ifdef NETCDF_HAS_NC4
        case NC_STRING:
        {
            char *pszVal = NULL;
            int status = nc_get_var1_string(
                m_nLayerCDFId, m_aoFieldDesc[i].nVarId, anIndex, &pszVal);
            if( status != NC_NOERR )
            {
                NCDF_ERR(status);
                continue;
            }
            if( pszVal != NULL )
            {
                poFeature->SetField(i, pszVal);
                nc_free_string(1, &pszVal);
            }
            break;
        }
#endif

        case NC_BYTE:
        {
            signed char chVal = 0;
            int status = nc_get_var1_schar(
                m_nLayerCDFId, m_aoFieldDesc[i].nVarId, anIndex, &chVal);
            if( status != NC_NOERR )
            {
                NCDF_ERR(status);
                continue;
            }
            if( chVal == m_aoFieldDesc[i].uNoData.chVal )
                continue;
            poFeature->SetField(i, static_cast<int>(chVal));
            break;
        }

#ifdef NETCDF_HAS_NC4
        case NC_UBYTE:
        {
            unsigned char uchVal = 0;
            int status = nc_get_var1_uchar(
                m_nLayerCDFId, m_aoFieldDesc[i].nVarId, anIndex, &uchVal);
            if( status != NC_NOERR )
            {
                NCDF_ERR(status);
                continue;
            }
            if( uchVal == m_aoFieldDesc[i].uNoData.uchVal )
                continue;
            poFeature->SetField(i, static_cast<int>(uchVal));
            break;
        }
#endif

        case NC_SHORT:
        {
            short sVal = 0;
            int status = nc_get_var1_short(
                m_nLayerCDFId, m_aoFieldDesc[i].nVarId, anIndex, &sVal);

            if( status != NC_NOERR )
            {
                NCDF_ERR(status);
                continue;
            }
            if( sVal == m_aoFieldDesc[i].uNoData.sVal )
                continue;
            poFeature->SetField(i, static_cast<int>(sVal));
            break;
        }

#ifdef NETCDF_HAS_NC4
        case NC_USHORT:
        {
            unsigned short usVal = 0;
            int status = nc_get_var1_ushort(
                m_nLayerCDFId, m_aoFieldDesc[i].nVarId, anIndex, &usVal);
            if( status != NC_NOERR )
            {
                NCDF_ERR(status);
                continue;
            }
            if( usVal == m_aoFieldDesc[i].uNoData.usVal )
                continue;
            poFeature->SetField(i, static_cast<int>(usVal));
            break;
        }
#endif

        case NC_INT:
        {
            int nVal = 0;
            int status = nc_get_var1_int(m_nLayerCDFId, m_aoFieldDesc[i].nVarId,
                                         anIndex, &nVal);
            if( status != NC_NOERR )
            {
                NCDF_ERR(status);
                continue;
            }
            if( nVal == m_aoFieldDesc[i].uNoData.nVal )
                continue;
            if( m_poFeatureDefn->GetFieldDefn(i)->GetType() == OFTDate ||
                m_poFeatureDefn->GetFieldDefn(i)->GetType() == OFTDateTime )
            {
                struct tm brokendowntime;
                GIntBig nVal64 = static_cast<GIntBig>(nVal);
                if( m_aoFieldDesc[i].bIsDays )
                    nVal64 *= 86400;
                CPLUnixTimeToYMDHMS(nVal64, &brokendowntime);
                poFeature->SetField(
                    i, brokendowntime.tm_year + 1900, brokendowntime.tm_mon + 1,
                    brokendowntime.tm_mday, brokendowntime.tm_hour,
                    brokendowntime.tm_min,
                    static_cast<float>(brokendowntime.tm_sec), 0);
            }
            else
            {
                poFeature->SetField(i, nVal);
            }
            break;
        }

#ifdef NETCDF_HAS_NC4
        case NC_UINT:
        {
            unsigned int unVal = 0;
            // nc_get_var1_uint() doesn't work on old netCDF version when
            // the returned value is > INT_MAX
            // https://bugtracking.unidata.ucar.edu/browse/NCF-226
            // nc_get_vara_uint() has not this bug
            size_t nCount = 1;
            int status =
                nc_get_vara_uint(m_nLayerCDFId, m_aoFieldDesc[i].nVarId,
                                 anIndex, &nCount, &unVal);
            if( status != NC_NOERR )
            {
                NCDF_ERR(status);
                continue;
            }
            if( unVal == m_aoFieldDesc[i].uNoData.unVal )
                continue;
            poFeature->SetField(i, static_cast<GIntBig>(unVal));
            break;
        }
#endif

#ifdef NETCDF_HAS_NC4
        case NC_INT64:
        {
            GIntBig nVal = 0;
            int status = nc_get_var1_longlong(
                m_nLayerCDFId, m_aoFieldDesc[i].nVarId, anIndex, &nVal);
            if( status != NC_NOERR )
            {
                NCDF_ERR(status);
                continue;
            }
            if( nVal == m_aoFieldDesc[i].uNoData.nVal64 )
                continue;
            poFeature->SetField(i, nVal);
            break;
        }

        case NC_UINT64:
        {
            GUIntBig nVal = 0;
            int status = nc_get_var1_ulonglong(
                m_nLayerCDFId, m_aoFieldDesc[i].nVarId, anIndex, &nVal);
            if( status != NC_NOERR )
            {
                NCDF_ERR(status);
                continue;
            }
            if( nVal == m_aoFieldDesc[i].uNoData.unVal64 )
                continue;
            poFeature->SetField(i, static_cast<double>(nVal));
            break;
        }
#endif

        case NC_FLOAT:
        {
            float fVal = 0.f;
            int status = nc_get_var1_float(
                m_nLayerCDFId, m_aoFieldDesc[i].nVarId, anIndex, &fVal);
            if( status != NC_NOERR )
            {
                NCDF_ERR(status);
                continue;
            }
            if( fVal == m_aoFieldDesc[i].uNoData.fVal )
                continue;
            poFeature->SetField(i, static_cast<double>(fVal));
            break;
        }

        case NC_DOUBLE:
        {
            double dfVal = 0.0;
            int status = nc_get_var1_double(
                m_nLayerCDFId, m_aoFieldDesc[i].nVarId, anIndex, &dfVal);
            if( status != NC_NOERR )
            {
                NCDF_ERR(status);
                continue;
            }
            if( dfVal == m_aoFieldDesc[i].uNoData.dfVal )
                continue;
            if( m_poFeatureDefn->GetFieldDefn(i)->GetType() == OFTDate ||
                m_poFeatureDefn->GetFieldDefn(i)->GetType() == OFTDateTime )
            {
                if( m_aoFieldDesc[i].bIsDays )
                    dfVal *= 86400.0;
                struct tm brokendowntime;
                GIntBig nVal = static_cast<GIntBig>(floor(dfVal));
                CPLUnixTimeToYMDHMS(nVal, &brokendowntime);
                poFeature->SetField(
                    i, brokendowntime.tm_year + 1900, brokendowntime.tm_mon + 1,
                    brokendowntime.tm_mday, brokendowntime.tm_hour,
                    brokendowntime.tm_min,
                    static_cast<float>(brokendowntime.tm_sec + (dfVal - nVal)),
                    0);
            }
            else
            {
                poFeature->SetField(i, dfVal);
            }
            break;
        }

        default:
            break;
        }
    }

    if( m_nXVarID >= 0 && m_nYVarID >= 0 &&
        (m_osProfileDimName.empty() || nMainDimId == m_nProfileDimID) )
    {
        bool bXIsNoData = false;
        const double dfX = Get1DVarAsDouble(
            m_nXVarID, m_nXVarNCDFType, anIndex[0], m_uXVarNoData, &bXIsNoData);
        bool bYIsNoData = false;
        const double dfY = Get1DVarAsDouble(
            m_nYVarID, m_nYVarNCDFType, anIndex[0], m_uYVarNoData, &bYIsNoData);

        if( !bXIsNoData && !bYIsNoData )
        {
            OGRPoint *poPoint = NULL;
            if( m_nZVarID >= 0 && m_osProfileDimName.empty() )
            {
                bool bZIsNoData = false;
                const double dfZ =
                    Get1DVarAsDouble(m_nZVarID, m_nZVarNCDFType, anIndex[0],
                                     m_uZVarNoData, &bZIsNoData);
                if( bZIsNoData )
                    poPoint = new OGRPoint(dfX, dfY);
                else
                    poPoint = new OGRPoint(dfX, dfY, dfZ);
            }
            else
                poPoint = new OGRPoint(dfX, dfY);
            poPoint->assignSpatialReference(GetSpatialRef());
            poFeature->SetGeometryDirectly(poPoint);
        }
    }
    else if( m_nXVarID >= 0 && m_nYVarID >= 0 && m_nZVarID >= 0 &&
             !m_osProfileDimName.empty() && nMainDimId == m_nRecordDimID )
    {
        OGRGeometry *poGeom = poFeature->GetGeometryRef();
        if( poGeom != NULL && wkbFlatten(poGeom->getGeometryType()) == wkbPoint )
        {
            bool bZIsNoData = false;
            const double dfZ =
                Get1DVarAsDouble(m_nZVarID, m_nZVarNCDFType, anIndex[0],
                                 m_uZVarNoData, &bZIsNoData);
            if( !bZIsNoData )
                ((OGRPoint *)poGeom)->setZ(dfZ);
        }
    }
    else if( m_nWKTVarID >= 0 )
    {
        char *pszWKT = NULL;
        if( m_nWKTNCDFType == NC_CHAR )
        {
            size_t anCount[2];
            anCount[0] = 1;
            anCount[1] = m_nWKTMaxWidth;
            pszWKT = (char *)CPLCalloc(1, anCount[1] + 1);
            int status = nc_get_vara_text(m_nLayerCDFId, m_nWKTVarID,
                                          anIndex, anCount, pszWKT);
            if( status == NC_EINVALCOORDS || status == NC_EEDGE )
            {
                CPLFree(pszWKT);
                return false;
            }
            if( status != NC_NOERR )
            {
                NCDF_ERR(status);
                CPLFree(pszWKT);
                pszWKT = NULL;
            }
        }
#ifdef NETCDF_HAS_NC4
        else if( m_nWKTNCDFType == NC_STRING )
        {
            char *pszVal = NULL;
            int status = nc_get_var1_string(m_nLayerCDFId, m_nWKTVarID,
                                            anIndex, &pszVal);
            if( status == NC_EINVALCOORDS || status == NC_EEDGE )
            {
                return false;
            }
            if( status != NC_NOERR )
            {
                NCDF_ERR(status);
            }
            else if( pszVal != NULL )
            {
                pszWKT = CPLStrdup(pszVal);
                nc_free_string(1, &pszVal);
            }
        }
#endif
        if( pszWKT != NULL )
        {
            char *pszWKTTmp = pszWKT;
            OGRGeometry *poGeom = NULL;
            CPL_IGNORE_RET_VAL(
                OGRGeometryFactory::createFromWkt(&pszWKTTmp, NULL, &poGeom));
            if( poGeom != NULL )
            {
                poGeom->assignSpatialReference(GetSpatialRef());
                poFeature->SetGeometryDirectly(poGeom);
            }
            CPLFree(pszWKT);
        }
    }

    return true;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *netCDFLayer::GetNextFeature()
{
    while( true )
    {
        OGRFeature *poFeature = GetNextRawFeature();
        if( poFeature == NULL )
            return NULL;

        if( (m_poFilterGeom == NULL
            || FilterGeometry(poFeature->GetGeomFieldRef(m_iGeomFieldFilter) ))
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate(poFeature)) )
            return poFeature;

        delete poFeature;
    }
}

/************************************************************************/
/*                            GetLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn *netCDFLayer::GetLayerDefn() { return m_poFeatureDefn; }

/************************************************************************/
/*                           ICreateFeature()                           */
/************************************************************************/

OGRErr netCDFLayer::ICreateFeature(OGRFeature *poFeature)
{
    m_poDS->SetDefineMode(false);

    size_t nFeatureIdx = 0;
    nc_inq_dimlen(m_nLayerCDFId, m_nRecordDimID, &nFeatureIdx);

    if( m_nProfileDimID >= 0 )
    {
        size_t nProfileCount = 0;
        nc_inq_dimlen(m_nLayerCDFId, m_nProfileDimID, &nProfileCount);

        OGRFeature *poProfileToLookup = poFeature->Clone();
        poProfileToLookup->SetFID(-1);
        for( int i = 0; i < m_poFeatureDefn->GetFieldCount(); i++ )
        {
            if( !(poProfileToLookup->IsFieldSetAndNotNull(i)) ||
                m_aoFieldDesc[i].nMainDimId != m_nProfileDimID )
            {
                poProfileToLookup->UnsetField(i);
                continue;
            }
        }
        OGRGeometry *poGeom = poProfileToLookup->GetGeometryRef();
        if( poGeom != NULL && wkbFlatten(poGeom->getGeometryType()) == wkbPoint )
        {
            ((OGRPoint *)poGeom)->setZ(0);
        }

        size_t nProfileIdx = 0;
        bool bFoundProfile = false;
        for( ; nProfileIdx < nProfileCount; nProfileIdx++ )
        {
            int nId = NC_FILL_INT;
            int status = nc_get_var1_int(m_nLayerCDFId, m_nProfileVarID,
                                         &nProfileIdx, &nId);
            NCDF_ERR(status);
            if( nId == NC_FILL_INT )
                break;

            OGRFeature *poIterFeature = new OGRFeature(m_poFeatureDefn);
            if( FillFeatureFromVar(poIterFeature, m_nProfileDimID, nProfileIdx) )
            {
                poGeom = poIterFeature->GetGeometryRef();
                if( poGeom != NULL &&
                    wkbFlatten(poGeom->getGeometryType()) == wkbPoint )
                {
                    ((OGRPoint *)poGeom)->setZ(0);
                }
                if( poIterFeature->Equal(poProfileToLookup) )
                {
                    bFoundProfile = true;
                    delete poIterFeature;
                    break;
                }
            }
            delete poIterFeature;
        }

        if( !bFoundProfile )
        {
            if( !m_bProfileVarUnlimited && nProfileIdx == nProfileCount )
            {
                size_t nNewSize = 1 + nProfileCount + nProfileCount / 3;
                m_poDS->GrowDim(m_nLayerCDFId, m_nProfileDimID, nNewSize);
            }

            if( !FillVarFromFeature(poProfileToLookup, m_nProfileDimID,
                                    nProfileIdx) )
            {
                delete poProfileToLookup;
                return OGRERR_FAILURE;
            }
        }

        int nProfileIdIdx = m_poFeatureDefn->GetFieldIndex(m_osProfileDimName);
        if( nProfileIdIdx < 0 ||
            m_poFeatureDefn->GetFieldDefn(nProfileIdIdx)->GetType() !=
                OFTInteger )
        {
            int nVal = static_cast<int>(nProfileIdx);
            int status = nc_put_var1_int(m_nLayerCDFId, m_nProfileVarID,
                                         &nProfileIdx, &nVal);
            NCDF_ERR(status);
        }

        int nVal = static_cast<int>(nProfileIdx);
        int status = nc_put_var1_int(m_nLayerCDFId, m_nParentIndexVarID,
                                     &nFeatureIdx, &nVal);
        NCDF_ERR(status);

        delete poProfileToLookup;
    }

    if( !FillVarFromFeature(poFeature, m_nRecordDimID, nFeatureIdx) )
        return OGRERR_FAILURE;

    poFeature->SetFID(nFeatureIdx + 1);

    return OGRERR_NONE;
}

/************************************************************************/
/*                           FillVarFromFeature()                       */
/************************************************************************/

bool netCDFLayer::FillVarFromFeature(OGRFeature *poFeature, int nMainDimId,
                                     size_t nIndex)
{
    size_t anIndex[2];
    anIndex[0] = nIndex;
    anIndex[1] = 0;

    for( int i = 0; i < m_poFeatureDefn->GetFieldCount(); i++ )
    {
        if( m_aoFieldDesc[i].nMainDimId != nMainDimId )
            continue;

        if( !(poFeature->IsFieldSetAndNotNull(i)) )
        {
#ifdef NETCDF_HAS_NC4
            if( m_bNCDumpCompat && m_aoFieldDesc[i].nType == NC_STRING )
            {
                const char *pszVal = "";
                int status = nc_put_var1_string(
                    m_nLayerCDFId, m_aoFieldDesc[i].nVarId, anIndex, &pszVal);
                NCDF_ERR(status);
            }
#endif
            continue;
        }

        int status = NC_NOERR;
        switch( m_aoFieldDesc[i].nType )
        {
        case NC_CHAR:
        {
            const char *pszVal = poFeature->GetFieldAsString(i);
            if( m_aoFieldDesc[i].nDimCount == 1 )
            {
                if( strlen(pszVal) > 1 &&
                    !m_aoFieldDesc[i].bHasWarnedAboutTruncation )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Content of field %s exceeded the 1 character "
                             "limit and will be truncated",
                             m_poFeatureDefn->GetFieldDefn(i)->GetNameRef());
                    m_aoFieldDesc[i].bHasWarnedAboutTruncation = true;
                }
                status = nc_put_var1_text(
                    m_nLayerCDFId, m_aoFieldDesc[i].nVarId, anIndex, pszVal);
            }
            else
            {
                size_t anCount[2];
                anCount[0] = 1;
                anCount[1] = strlen(pszVal);
                size_t nWidth = 0;
                nc_inq_dimlen(m_nLayerCDFId, m_aoFieldDesc[i].nSecDimId,
                              &nWidth);
                if( anCount[1] > nWidth )
                {
                    if( m_bAutoGrowStrings &&
                        m_poFeatureDefn->GetFieldDefn(i)->GetWidth() == 0 )
                    {
                        size_t nNewSize = anCount[1] + anCount[1] / 3;

                        CPLDebug("GDAL_netCDF", "Growing %s from %u to %u",
                                 m_poFeatureDefn->GetFieldDefn(i)->GetNameRef(),
                                 static_cast<unsigned>(nWidth),
                                 static_cast<unsigned>(nNewSize));
                        m_poDS->GrowDim(m_nLayerCDFId,
                                        m_aoFieldDesc[i].nSecDimId, nNewSize);

                        pszVal = poFeature->GetFieldAsString(i);
                    }
                    else
                    {
                        anCount[1] = nWidth;
                        if( !m_aoFieldDesc[i].bHasWarnedAboutTruncation )
                        {
                            CPLError(
                                CE_Warning, CPLE_AppDefined,
                                "Content of field %s exceeded the %u character "
                                "limit and will be truncated",
                                m_poFeatureDefn->GetFieldDefn(i)->GetNameRef(),
                                static_cast<unsigned int>(nWidth));
                            m_aoFieldDesc[i].bHasWarnedAboutTruncation = true;
                        }
                    }
                }
                status =
                    nc_put_vara_text(m_nLayerCDFId, m_aoFieldDesc[i].nVarId,
                                     anIndex, anCount, pszVal);
            }
            break;
        }

#ifdef NETCDF_HAS_NC4
        case NC_STRING:
        {
            const char *pszVal = poFeature->GetFieldAsString(i);
            status = nc_put_var1_string(m_nLayerCDFId, m_aoFieldDesc[i].nVarId,
                                        anIndex, &pszVal);
            break;
        }
#endif

        case NC_BYTE:
        {
            int nVal = poFeature->GetFieldAsInteger(i);
            signed char chVal = static_cast<signed char>(nVal);
            status = nc_put_var1_schar(m_nLayerCDFId, m_aoFieldDesc[i].nVarId,
                                       anIndex, &chVal);
            break;
        }

#ifdef NETCDF_HAS_NC4
        case NC_UBYTE:
        {
            int nVal = poFeature->GetFieldAsInteger(i);
            unsigned char uchVal = static_cast<unsigned char>(nVal);
            status = nc_put_var1_uchar(m_nLayerCDFId, m_aoFieldDesc[i].nVarId,
                                       anIndex, &uchVal);
            break;
        }
#endif

        case NC_SHORT:
        {
            int nVal = poFeature->GetFieldAsInteger(i);
            short sVal = static_cast<short>(nVal);
            status = nc_put_var1_short(m_nLayerCDFId, m_aoFieldDesc[i].nVarId,
                                       anIndex, &sVal);
            break;
        }

#ifdef NETCDF_HAS_NC4
        case NC_USHORT:
        {
            int nVal = poFeature->GetFieldAsInteger(i);
            unsigned short usVal = static_cast<unsigned short>(nVal);
            status = nc_put_var1_ushort(m_nLayerCDFId, m_aoFieldDesc[i].nVarId,
                                        anIndex, &usVal);
            break;
        }
#endif

        case NC_INT:
        {
            int nVal;
            if( m_poFeatureDefn->GetFieldDefn(i)->GetType() == OFTDate )
            {
                int nYear;
                int nMonth;
                int nDay;
                int nHour;
                int nMinute;
                float fSecond;
                int nTZ;
                poFeature->GetFieldAsDateTime(i, &nYear, &nMonth, &nDay,
                                              &nHour, &nMinute, &fSecond, &nTZ);
                struct tm brokendowntime;
                brokendowntime.tm_year = nYear - 1900;
                brokendowntime.tm_mon = nMonth - 1;
                brokendowntime.tm_mday = nDay;
                brokendowntime.tm_hour = 0;
                brokendowntime.tm_min = 0;
                brokendowntime.tm_sec = 0;
                GIntBig nVal64 = CPLYMDHMSToUnixTime(&brokendowntime);
                if( m_aoFieldDesc[i].bIsDays )
                    nVal64 /= 86400;
                nVal = static_cast<int>(nVal64);
            }
            else
            {
                nVal = poFeature->GetFieldAsInteger(i);
            }
            status = nc_put_var1_int(m_nLayerCDFId, m_aoFieldDesc[i].nVarId,
                                     anIndex, &nVal);
            break;
        }

#ifdef NETCDF_HAS_NC4
        case NC_UINT:
        {
            GIntBig nVal = poFeature->GetFieldAsInteger64(i);
            unsigned int unVal = static_cast<unsigned int>(nVal);
            status = nc_put_var1_uint(m_nLayerCDFId, m_aoFieldDesc[i].nVarId,
                                      anIndex, &unVal);
            break;
        }
#endif

#ifdef NETCDF_HAS_NC4
        case NC_INT64:
        {
            GIntBig nVal = poFeature->GetFieldAsInteger64(i);
            status = nc_put_var1_longlong(
                m_nLayerCDFId, m_aoFieldDesc[i].nVarId, anIndex, &nVal);
            break;
        }

        case NC_UINT64:
        {
            double dfVal = poFeature->GetFieldAsDouble(i);
            GUIntBig nVal = static_cast<GUIntBig>(dfVal);
            status = nc_put_var1_ulonglong(
                m_nLayerCDFId, m_aoFieldDesc[i].nVarId, anIndex, &nVal);
            break;
        }
#endif

        case NC_FLOAT:
        {
            double dfVal = poFeature->GetFieldAsDouble(i);
            float fVal = static_cast<float>(dfVal);
            status = nc_put_var1_float(m_nLayerCDFId, m_aoFieldDesc[i].nVarId,
                                       anIndex, &fVal);
            break;
        }

        case NC_DOUBLE:
        {
            double dfVal;
            if( m_poFeatureDefn->GetFieldDefn(i)->GetType() == OFTDate ||
                m_poFeatureDefn->GetFieldDefn(i)->GetType() == OFTDateTime )
            {
                int nYear;
                int nMonth;
                int nDay;
                int nHour;
                int nMinute;
                float fSecond;
                int nTZ;
                poFeature->GetFieldAsDateTime(i, &nYear, &nMonth, &nDay,
                                              &nHour, &nMinute, &fSecond, &nTZ);
                struct tm brokendowntime;
                brokendowntime.tm_year = nYear - 1900;
                brokendowntime.tm_mon = nMonth - 1;
                brokendowntime.tm_mday = nDay;
                brokendowntime.tm_hour = nHour;
                brokendowntime.tm_min = nMinute;
                brokendowntime.tm_sec = static_cast<int>(fSecond);
                GIntBig nVal = CPLYMDHMSToUnixTime(&brokendowntime);
                dfVal = static_cast<double>(nVal) + fmod(fSecond, 1.0f);
                if( m_aoFieldDesc[i].bIsDays )
                    dfVal /= 86400.0;
            }
            else
            {
                dfVal = poFeature->GetFieldAsDouble(i);
            }
            status = nc_put_var1_double(m_nLayerCDFId, m_aoFieldDesc[i].nVarId,
                                        anIndex, &dfVal);
            break;
        }

        default:
            break;
        }

        NCDF_ERR(status);
        if( status != NC_NOERR )
        {
            return false;
        }
    }

    OGRGeometry *poGeom = poFeature->GetGeometryRef();
    if( wkbFlatten(m_poFeatureDefn->GetGeomType()) == wkbPoint &&
        poGeom != NULL && wkbFlatten(poGeom->getGeometryType()) == wkbPoint )
    {
        if( m_osProfileDimName.empty() || nMainDimId == m_nProfileDimID )
        {
            double dfX = static_cast<OGRPoint *>(poGeom)->getX();
            double dfY = static_cast<OGRPoint *>(poGeom)->getY();

            int status;

            if( m_nXVarNCDFType == NC_DOUBLE )
                status =
                    nc_put_var1_double(m_nLayerCDFId, m_nXVarID, anIndex, &dfX);
            else
            {
                float fX = static_cast<float>(dfX);
                status =
                    nc_put_var1_float(m_nLayerCDFId, m_nXVarID, anIndex, &fX);
            }
            NCDF_ERR(status);
            if( status != NC_NOERR )
            {
                return false;
            }

            if( m_nYVarNCDFType == NC_DOUBLE )
                status =
                    nc_put_var1_double(m_nLayerCDFId, m_nYVarID, anIndex, &dfY);
            else
            {
                float fY = static_cast<float>(dfY);
                status =
                    nc_put_var1_float(m_nLayerCDFId, m_nYVarID, anIndex, &fY);
            }
            NCDF_ERR(status);
            if( status != NC_NOERR )
            {
                return false;
            }
        }

        if( m_poFeatureDefn->GetGeomType() == wkbPoint25D &&
            (m_osProfileDimName.empty() || nMainDimId == m_nRecordDimID) )
        {
            int status;
            double dfZ = static_cast<OGRPoint *>(poGeom)->getZ();
            if( m_nZVarNCDFType == NC_DOUBLE )
                status =
                    nc_put_var1_double(m_nLayerCDFId, m_nZVarID, anIndex, &dfZ);
            else
            {
                float fZ = static_cast<float>(dfZ);
                status =
                    nc_put_var1_float(m_nLayerCDFId, m_nZVarID, anIndex, &fZ);
            }
            NCDF_ERR(status);
            if( status != NC_NOERR )
            {
                return false;
            }
        }
    }
    else if( m_poFeatureDefn->GetGeomType() != wkbNone && m_nWKTVarID >= 0 &&
             poGeom != NULL )
    {
        char *pszWKT = NULL;
        poGeom->exportToWkt(&pszWKT, wkbVariantIso);
        int status;
#ifdef NETCDF_HAS_NC4
        if( m_nWKTNCDFType == NC_STRING )
        {
            const char *pszWKTConst = pszWKT;
            status = nc_put_var1_string(m_nLayerCDFId, m_nWKTVarID,
                                        anIndex, &pszWKTConst);
        }
        else
#endif
        {
            size_t anCount[2];
            anCount[0] = 1;
            anCount[1] = strlen(pszWKT);
            if( anCount[1] > static_cast<unsigned int>(m_nWKTMaxWidth) )
            {
                if( m_bAutoGrowStrings )
                {
                    size_t nNewSize = anCount[1] + anCount[1] / 3;

                    CPLDebug("GDAL_netCDF", "Growing %s from %u to %u",
                             m_osWKTVarName.c_str(),
                             static_cast<unsigned>(m_nWKTMaxWidth),
                             static_cast<unsigned>(nNewSize));
                    m_poDS->GrowDim(m_nLayerCDFId, m_nWKTMaxWidthDimId,
                                    nNewSize);

                    m_nWKTMaxWidth = static_cast<int>(nNewSize);

                    status = nc_put_vara_text(m_nLayerCDFId, m_nWKTVarID,
                                              anIndex, anCount, pszWKT);
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot write geometry as WKT. Would require %d "
                             "characters but field width is %d",
                             static_cast<int>(anCount[1]), m_nWKTMaxWidth);
                    status = NC_NOERR;
                }
            }
            else
            {
                status = nc_put_vara_text(m_nLayerCDFId, m_nWKTVarID,
                                          anIndex, anCount, pszWKT);
            }
        }
        CPLFree(pszWKT);
        NCDF_ERR(status);
        if( status != NC_NOERR )
        {
            return false;
        }
    }
#ifdef NETCDF_HAS_NC4
    else if( m_poFeatureDefn->GetGeomType() != wkbNone && m_nWKTVarID >= 0 &&
             poGeom == NULL && m_nWKTNCDFType == NC_STRING && m_bNCDumpCompat )
    {
        const char *pszWKTConst = "";
        int status = nc_put_var1_string(m_nLayerCDFId, m_nWKTVarID,
                                        anIndex, &pszWKTConst);
        NCDF_ERR(status);
    }
#endif

    return true;
}

/************************************************************************/
/*                              AddField()                              */
/************************************************************************/

bool netCDFLayer::AddField(int nVarID)
{
    if( nVarID == m_nWKTVarID )
        return false;

    char szName[NC_MAX_NAME + 1];
    szName[0] = '\0';
    CPL_IGNORE_RET_VAL(nc_inq_varname(m_nLayerCDFId, nVarID, szName));

    nc_type vartype = NC_NAT;
    nc_inq_vartype(m_nLayerCDFId, nVarID, &vartype);

    OGRFieldType eType = OFTString;
    OGRFieldSubType eSubType = OFSTNone;
    int nWidth = 0;

    NCDFNoDataUnion nodata;
    memset(&nodata, 0, sizeof(nodata));
    int nDimCount = 1;
    nc_inq_varndims(m_nLayerCDFId, nVarID, &nDimCount);
    int anDimIds[2] = { -1, -1 };
    if( nDimCount <= 2 )
        nc_inq_vardimid(m_nLayerCDFId, nVarID, anDimIds);

    switch( vartype )
    {
    case NC_BYTE:
    {
        eType = OFTInteger;
        char *pszValue = NULL;
        if( GetFillValue(nVarID, &pszValue) == CE_None )
            nodata.chVal = static_cast<signed char>(atoi(pszValue));
        else
            nodata.chVal = NC_FILL_BYTE;
        CPLFree(pszValue);
        break;
    }

#ifdef NETCDF_HAS_NC4
    case NC_UBYTE:
    {
        eType = OFTInteger;
        char *pszValue = NULL;
        if( GetFillValue(nVarID, &pszValue) == CE_None )
            nodata.uchVal = static_cast<unsigned char>(atoi(pszValue));
        else
            nodata.uchVal = NC_FILL_UBYTE;
        CPLFree(pszValue);
        break;
    }
#endif

    case NC_CHAR:
    {
        eType = OFTString;
        if( nDimCount == 1 )
        {
            nWidth = 1;
        }
        else if( nDimCount == 2 )
        {
            size_t nDimLen = 0;
            nc_inq_dimlen(m_nLayerCDFId, anDimIds[1], &nDimLen);
            nWidth = static_cast<int>(nDimLen);
        }
        break;
    }

#ifdef NETCDF_HAS_NC4
    case NC_STRING:
    {
        eType = OFTString;
        break;
    }
#endif

    case NC_SHORT:
    {
        eType = OFTInteger;
        eSubType = OFSTInt16;
        char *pszValue = NULL;
        if( GetFillValue(nVarID, &pszValue) == CE_None )
            nodata.sVal = static_cast<short>(atoi(pszValue));
        else
            nodata.sVal = NC_FILL_SHORT;
        CPLFree(pszValue);
        break;
    }

#ifdef NETCDF_HAS_NC4
    case NC_USHORT:
    {
        eType = OFTInteger;
        char *pszValue = NULL;
        if( GetFillValue(nVarID, &pszValue) == CE_None )
            nodata.usVal = static_cast<unsigned short>(atoi(pszValue));
        else
            nodata.usVal = NC_FILL_USHORT;
        CPLFree(pszValue);
        break;
    }
#endif

    case NC_INT:
    {
        eType = OFTInteger;
        char *pszValue = NULL;
        if( GetFillValue(nVarID, &pszValue) == CE_None )
            nodata.nVal = atoi(pszValue);
        else
            nodata.nVal = NC_FILL_INT;
        CPLFree(pszValue);
        break;
    }

#ifdef NETCDF_HAS_NC4
    case NC_UINT:
    {
        eType = OFTInteger64;
        char *pszValue = NULL;
        if( GetFillValue(nVarID, &pszValue) == CE_None )
            nodata.unVal = static_cast<unsigned int>(CPLAtoGIntBig(pszValue));
        else
            nodata.unVal = NC_FILL_UINT;
        CPLFree(pszValue);
        break;
    }
#endif

#ifdef NETCDF_HAS_NC4
    case NC_INT64:
    {
        eType = OFTInteger64;
        char *pszValue = NULL;
        if( GetFillValue(nVarID, &pszValue) == CE_None )
            nodata.nVal64 = CPLAtoGIntBig(pszValue);
        else
            nodata.nVal64 = NC_FILL_INT64;
        CPLFree(pszValue);
        break;
    }

    case NC_UINT64:
    {
        eType = OFTReal;
        char *pszValue = NULL;
        if( GetFillValue(nVarID, &pszValue) == CE_None )
        {
            nodata.unVal64 = 0;
            for( int i = 0; pszValue[i] != '\0'; i++ )
            {
                nodata.unVal64 = nodata.unVal64 * 10 + (pszValue[i] - '0');
            }
        }
        else
            nodata.unVal64 = NC_FILL_UINT64;
        CPLFree(pszValue);
        break;
    }
#endif

    case NC_FLOAT:
    {
        eType = OFTReal;
        eSubType = OFSTFloat32;
        double dfValue;
        if( GetFillValue(nVarID, &dfValue) == CE_None )
            nodata.fVal = static_cast<float>(dfValue);
        else
            nodata.fVal = NC_FILL_FLOAT;
        break;
    }

    case NC_DOUBLE:
    {
        eType = OFTReal;
        double dfValue;
        if( GetFillValue(nVarID, &dfValue) == CE_None )
            nodata.dfVal = dfValue;
        else
            nodata.dfVal = NC_FILL_DOUBLE;
        break;
    }

    default:
    {
        CPLDebug("GDAL_netCDF", "Variable %s has type %d, which is unhandled",
                 szName, vartype);
        return false;
    }
    }

    bool bIsDays = false;

    char *pszValue = NULL;
    if( NCDFGetAttr(m_nLayerCDFId, nVarID, "ogr_field_type", &pszValue) ==
        CE_None )
    {
        if( (eType == OFTInteger || eType == OFTReal) && EQUAL(pszValue, "Date") )
        {
            eType = OFTDate;
            // cppcheck-suppress knownConditionTrueFalse
            bIsDays = (eType == OFTInteger);
        }
        else if( (eType == OFTInteger || eType == OFTReal) &&
                 EQUAL(pszValue, "DateTime") )
            eType = OFTDateTime;
        else if( eType == OFTReal && EQUAL(pszValue, "Integer64") )
            eType = OFTInteger64;
        else if( eType == OFTInteger && EQUAL(pszValue, "Integer(Boolean)") )
            eSubType = OFSTBoolean;
    }
    CPLFree(pszValue);
    pszValue = NULL;

    if( NCDFGetAttr(m_nLayerCDFId, nVarID, "units", &pszValue) == CE_None )
    {
        if( (eType == OFTInteger || eType == OFTReal || eType == OFTDate) &&
            (EQUAL(pszValue, "seconds since 1970-1-1 0:0:0") ||
             EQUAL(pszValue, "seconds since 1970-01-01 00:00:00")) )
        {
            if( eType != OFTDate )
                eType = OFTDateTime;
            bIsDays = false;
        }
        else if( (eType == OFTInteger || eType == OFTReal || eType == OFTDate) &&
                 (EQUAL(pszValue, "days since 1970-1-1") ||
                  EQUAL(pszValue, "days since 1970-01-01")) )
        {
            eType = OFTDate;
            bIsDays = true;
        }
    }
    CPLFree(pszValue);
    pszValue = NULL;

    if( NCDFGetAttr(m_nLayerCDFId, nVarID, "ogr_field_name", &pszValue) ==
        CE_None )
    {
        snprintf(szName, sizeof(szName), "%s", pszValue);
    }
    CPLFree(pszValue);
    pszValue = NULL;

    if( NCDFGetAttr(m_nLayerCDFId, nVarID, "ogr_field_width", &pszValue) ==
        CE_None )
    {
        nWidth = atoi(pszValue);
    }
    CPLFree(pszValue);
    pszValue = NULL;

    int nPrecision = 0;
    if( NCDFGetAttr(m_nLayerCDFId, nVarID, "ogr_field_precision", &pszValue) ==
        CE_None )
    {
        nPrecision = atoi(pszValue);
    }
    CPLFree(pszValue);
    /* pszValue = NULL; */

    OGRFieldDefn oFieldDefn(szName, eType);
    oFieldDefn.SetSubType(eSubType);
    oFieldDefn.SetWidth(nWidth);
    oFieldDefn.SetPrecision(nPrecision);

    FieldDesc fieldDesc;
    fieldDesc.uNoData = nodata;
    fieldDesc.nType = vartype;
    fieldDesc.nVarId = nVarID;
    fieldDesc.nDimCount = nDimCount;
    fieldDesc.nMainDimId = anDimIds[0];
    fieldDesc.nSecDimId = anDimIds[1];
    fieldDesc.bHasWarnedAboutTruncation = false;
    fieldDesc.bIsDays = bIsDays;
    m_aoFieldDesc.push_back(fieldDesc);

    m_poFeatureDefn->AddFieldDefn(&oFieldDefn);

    return true;
}

/************************************************************************/
/*                             CreateField()                            */
/************************************************************************/

OGRErr netCDFLayer::CreateField(OGRFieldDefn *poFieldDefn, int /* bApproxOK */)
{
    int nSecDimId = -1;
    int nVarID = -1;
    int status;

    const netCDFWriterConfigField *poConfig = NULL;
    if( m_poDS->oWriterConfig.m_bIsValid )
    {
        std::map<CPLString, netCDFWriterConfigField>::const_iterator oIter;
        if( m_poLayerConfig != NULL &&
            (oIter = m_poLayerConfig->m_oFields.find(
                 poFieldDefn->GetNameRef())) != m_poLayerConfig->m_oFields.end() )
        {
            poConfig = &(oIter->second);
        }
        else if( (oIter = m_poDS->oWriterConfig.m_oFields.find(
                      poFieldDefn->GetNameRef())) !=
                 m_poDS->oWriterConfig.m_oFields.end() )
        {
            poConfig = &(oIter->second);
        }
    }

    if( !m_osProfileDimName.empty() &&
        EQUAL(poFieldDefn->GetNameRef(), m_osProfileDimName) &&
        poFieldDefn->GetType() == OFTInteger )
    {
        FieldDesc fieldDesc;
        fieldDesc.uNoData.nVal = NC_FILL_INT;
        fieldDesc.nType = NC_INT;
        fieldDesc.nVarId = m_nProfileVarID;
        fieldDesc.nDimCount = 1;
        fieldDesc.nMainDimId = m_nProfileDimID;
        fieldDesc.nSecDimId = -1;
        fieldDesc.bHasWarnedAboutTruncation = false;
        fieldDesc.bIsDays = false;
        m_aoFieldDesc.push_back(fieldDesc);
        m_poFeatureDefn->AddFieldDefn(poFieldDefn);
        return OGRERR_NONE;
    }

    m_poDS->SetDefineMode(true);

    // Try to use the field name as variable name, but detects conflict first
    CPLString osVarName(poConfig != NULL
                           ? poConfig->m_osNetCDFName
                           : CPLString(poFieldDefn->GetNameRef()));

    status = nc_inq_varid(m_nLayerCDFId, osVarName, &nVarID);
    if( status == NC_NOERR )
    {
        for( int i = 1; i <= 100; i++ )
        {
            osVarName = CPLSPrintf("%s%d", poFieldDefn->GetNameRef(), i);
            status = nc_inq_varid(m_nLayerCDFId, osVarName, &nVarID);
            if( status != NC_NOERR )
                break;
        }
        CPLDebug("netCDF", "Field %s is written in variable %s",
                 poFieldDefn->GetNameRef(), osVarName.c_str());
    }

    const char *pszVarName = osVarName.c_str();

    NCDFNoDataUnion nodata;
    memset(&nodata, 0, sizeof(nodata));

    const OGRFieldType eType = poFieldDefn->GetType();
    const OGRFieldSubType eSubType = poFieldDefn->GetSubType();
    nc_type nType = NC_NAT;
    int nDimCount = 1;

    // Find which is the dimension that this variable should be indexed against
    int nMainDimId = m_nRecordDimID;
    if( !m_osProfileVariables.empty() )
    {
        char **papszTokens =
            CSLTokenizeString2(m_osProfileVariables, ",", CSLT_HONOURSTRINGS);
        if( CSLFindString(papszTokens, poFieldDefn->GetNameRef()) >= 0 )
            nMainDimId = m_nProfileDimID;
        CSLDestroy(papszTokens);
    }
    if( poConfig != NULL && !poConfig->m_osMainDim.empty() )
    {
        int ndims = 0;
        status = nc_inq_ndims(m_nLayerCDFId, &ndims);
        NCDF_ERR(status);
        bool bFound = false;
        for( int idim = 0; idim < ndims; idim++ )
        {
            char szDimName[NC_MAX_NAME + 1];
            szDimName[0] = 0;
            status = nc_inq_dimname(m_poDS->cdfid, idim, szDimName);
            NCDF_ERR(status);
            if( strcmp(poConfig->m_osMainDim, szDimName) == 0 )
            {
                nMainDimId = idim;
                bFound = true;
                break;
            }
        }
        if( !bFound )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Dimension '%s' does not exist",
                     poConfig->m_osMainDim.c_str());
        }
    }

    switch( eType )
    {
    case OFTString:
    case OFTStringList:
    case OFTIntegerList:
    case OFTRealList:
    {
        if( poFieldDefn->GetWidth() == 1 )
        {
            nType = NC_CHAR;
            status = nc_def_var(m_nLayerCDFId, pszVarName, nType, 1,
                                &nMainDimId, &nVarID);
        }
#ifdef NETCDF_HAS_NC4
        else if( m_poDS->eFormat == NCDF_FORMAT_NC4 && m_bUseStringInNC4 )
        {
            nType = NC_STRING;
            status = nc_def_var(m_nLayerCDFId, pszVarName, nType, 1,
                                &nMainDimId, &nVarID);
        }
#endif
        else
        {
            if( poFieldDefn->GetWidth() == 0 && !m_bAutoGrowStrings )
            {
                if( m_nDefaultMaxWidthDimId < 0 )
                {
                    status =
                        nc_def_dim(m_nLayerCDFId, "string_default_max_width",
                                   m_nDefaultWidth, &m_nDefaultMaxWidthDimId);
                    NCDF_ERR(status);
                    if( status != NC_NOERR )
                    {
                        return OGRERR_FAILURE;
                    }
                }
                nSecDimId = m_nDefaultMaxWidthDimId;
            }
            else
            {
                size_t nDim = poFieldDefn->GetWidth() == 0
                                  ? m_nDefaultWidth
                                  : poFieldDefn->GetWidth();
                status = nc_def_dim(m_nLayerCDFId,
                                    CPLSPrintf("%s_max_width", pszVarName),
                                    nDim, &nSecDimId);
                NCDF_ERR(status);
                if( status != NC_NOERR )
                {
                    return OGRERR_FAILURE;
                }
            }

            nDimCount = 2;
            int anDims[2] = {nMainDimId, nSecDimId};
            nType = NC_CHAR;
            status = nc_def_var(m_nLayerCDFId, pszVarName, nType, 2, anDims,
                                &nVarID);
        }
        NCDF_ERR(status);
        if( status != NC_NOERR )
        {
            return OGRERR_FAILURE;
        }

        break;
    }

    case OFTInteger:
    {
        nType = eSubType == OFSTBoolean
                    ? NC_BYTE
                    : (eSubType == OFSTInt16) ? NC_SHORT : NC_INT;

        if( nType == NC_BYTE )
            nodata.chVal = NC_FILL_BYTE;
        else if( nType == NC_SHORT )
            nodata.sVal = NC_FILL_SHORT;
        else if( nType == NC_INT )
            nodata.nVal = NC_FILL_INT;

        status = nc_def_var(m_nLayerCDFId, pszVarName, nType, 1, &nMainDimId,
                            &nVarID);
        NCDF_ERR(status);
        if( status != NC_NOERR )
        {
            return OGRERR_FAILURE;
        }

        if( eSubType == OFSTBoolean )
        {
            signed char anRange[2] = { 0, 1 };
            nc_put_att_schar(m_nLayerCDFId, nVarID, "valid_range", NC_BYTE, 2,
                             anRange);
        }

        break;
    }

    case OFTInteger64:
    {
        nType = NC_DOUBLE;
        nodata.dfVal = NC_FILL_DOUBLE;
#ifdef NETCDF_HAS_NC4
        if( m_poDS->eFormat == NCDF_FORMAT_NC4 )
        {
            nType = NC_INT64;
            nodata.nVal64 = NC_FILL_INT64;
        }
#endif
        status = nc_def_var(m_nLayerCDFId, pszVarName, nType, 1, &nMainDimId,
                            &nVarID);
        NCDF_ERR(status);
        if( status != NC_NOERR )
        {
            return OGRERR_FAILURE;
        }
        break;
    }

    case OFTReal:
    {
        nType = (eSubType == OFSTFloat32) ? NC_FLOAT : NC_DOUBLE;
        if( eSubType == OFSTFloat32 )
            nodata.fVal = NC_FILL_FLOAT;
        else
            nodata.dfVal = NC_FILL_DOUBLE;
        status = nc_def_var(m_nLayerCDFId, pszVarName, nType, 1, &nMainDimId,
                            &nVarID);
        NCDF_ERR(status);
        if( status != NC_NOERR )
        {
            return OGRERR_FAILURE;
        }

        break;
    }

    case OFTDate:
    {
        nType = NC_INT;
        status = nc_def_var(m_nLayerCDFId, pszVarName, nType, 1, &nMainDimId,
                            &nVarID);
        NCDF_ERR(status);
        if( status != NC_NOERR )
        {
            return OGRERR_FAILURE;
        }
        nodata.nVal = NC_FILL_INT;

        status = nc_put_att_text(m_nLayerCDFId, nVarID, CF_UNITS,
                                 strlen("days since 1970-1-1"),
                                 "days since 1970-1-1");
        NCDF_ERR(status);

        break;
    }

    case OFTDateTime:
    {
        nType = NC_DOUBLE;
        status = nc_def_var(m_nLayerCDFId, pszVarName, nType, 1, &nMainDimId,
                            &nVarID);
        NCDF_ERR(status);
        if( status != NC_NOERR )
        {
            return OGRERR_FAILURE;
        }
        nodata.dfVal = NC_FILL_DOUBLE;

        status = nc_put_att_text(m_nLayerCDFId, nVarID, CF_UNITS,
                                 strlen("seconds since 1970-1-1 0:0:0"),
                                 "seconds since 1970-1-1 0:0:0");
        NCDF_ERR(status);

        break;
    }

    default:
        return OGRERR_FAILURE;
    }

    FieldDesc fieldDesc;
    fieldDesc.uNoData = nodata;
    fieldDesc.nType = nType;
    fieldDesc.nVarId = nVarID;
    fieldDesc.nDimCount = nDimCount;
    fieldDesc.nMainDimId = nMainDimId;
    fieldDesc.nSecDimId = nSecDimId;
    fieldDesc.bHasWarnedAboutTruncation = false;
    fieldDesc.bIsDays = (eType == OFTDate);
    m_aoFieldDesc.push_back(fieldDesc);

    const char *pszLongName = CPLSPrintf("Field %s", poFieldDefn->GetNameRef());
    status = nc_put_att_text(m_nLayerCDFId, nVarID, CF_LNG_NAME,
                             strlen(pszLongName), pszLongName);
    NCDF_ERR(status);

    if( m_bWriteGDALTags )
    {
        status = nc_put_att_text(m_nLayerCDFId, nVarID, "ogr_field_name",
                                 strlen(poFieldDefn->GetNameRef()),
                                 poFieldDefn->GetNameRef());
        NCDF_ERR(status);

        const char *pszType = OGRFieldDefn::GetFieldTypeName(eType);
        if( eSubType != OFSTNone )
        {
            pszType = CPLSPrintf("%s(%s)", pszType,
                                 OGRFieldDefn::GetFieldSubTypeName(eSubType));
        }
        status = nc_put_att_text(m_nLayerCDFId, nVarID, "ogr_field_type",
                                 strlen(pszType), pszType);
        NCDF_ERR(status);

        const int nWidth = poFieldDefn->GetWidth();
        if(nWidth || nType == NC_CHAR)
        {
            status = nc_put_att_int(m_nLayerCDFId, nVarID, "ogr_field_width",
                                    NC_INT, 1, &nWidth);
            NCDF_ERR(status);

            const int nPrecision = poFieldDefn->GetPrecision();
            if( nPrecision )
            {
                status =
                    nc_put_att_int(m_nLayerCDFId, nVarID, "ogr_field_precision",
                                   NC_INT, 1, &nPrecision);
                NCDF_ERR(status);
            }
        }
    }

    // nc_put_att_text(m_nLayerCDFId, nVarID, CF_UNITS,
    //                 strlen("none"), "none");

    if( !m_osGridMapping.empty() && nMainDimId == m_nRecordDimID )
    {
        status =
            nc_put_att_text(m_nLayerCDFId, nVarID, CF_GRD_MAPPING,
                            m_osGridMapping.size(), m_osGridMapping.c_str());
        NCDF_ERR(status);
    }

    if( !m_osCoordinatesValue.empty() && nMainDimId == m_nRecordDimID )
    {
        status = nc_put_att_text(m_nLayerCDFId, nVarID, CF_COORDINATES,
                                 m_osCoordinatesValue.size(),
                                 m_osCoordinatesValue.c_str());
        NCDF_ERR(status);
    }

    if( poConfig != NULL )
    {
        netCDFWriteAttributesFromConf(m_nLayerCDFId, nVarID,
                                      poConfig->m_aoAttributes);
    }

    m_poFeatureDefn->AddFieldDefn(poFieldDefn);
    return OGRERR_NONE;
}

/************************************************************************/
/*                         GetFeatureCount()                            */
/************************************************************************/

GIntBig netCDFLayer::GetFeatureCount(int bForce)
{
    if( m_poFilterGeom == NULL && m_poAttrQuery == NULL )
    {
        size_t nDimLen;
        nc_inq_dimlen(m_nLayerCDFId, m_nRecordDimID, &nDimLen);
        return static_cast<GIntBig>(nDimLen);
    }
    return OGRLayer::GetFeatureCount(bForce);
}

/************************************************************************/
/*                          TestCapability()                            */
/************************************************************************/

int netCDFLayer::TestCapability(const char *pszCap)
{
    if( EQUAL(pszCap, OLCSequentialWrite) )
        return m_poDS->GetAccess() == GA_Update;
    if( EQUAL(pszCap, OLCCreateField) )
        return m_poDS->GetAccess() == GA_Update;
    if( EQUAL(pszCap, OLCFastFeatureCount) )
        return m_poFilterGeom == NULL && m_poAttrQuery == NULL;
    return FALSE;
}
