/******************************************************************************
 *
 * Project:  PDS 4 Driver; Planetary Data System Format
 * Purpose:  Implementation of PDS4Dataset
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2019, Hobu Inc
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

#include "pds4dataset.h"
#include "ogr_vrt.h"

#include "ogr_p.h"

#include <algorithm>
#include <cassert>

/************************************************************************/
/* ==================================================================== */
/*                        PDS4TableBaseLayer                            */
/* ==================================================================== */
/************************************************************************/

PDS4TableBaseLayer::PDS4TableBaseLayer(PDS4Dataset* poDS,
                                       const char* pszName,
                                       const char* pszFilename) :
    m_poDS(poDS),
    m_poRawFeatureDefn(new OGRFeatureDefn(pszName)),
    m_poFeatureDefn(new OGRFeatureDefn(pszName)),
    m_osFilename(pszFilename)
{
    m_poRawFeatureDefn->SetGeomType(wkbNone);
    m_poRawFeatureDefn->Reference();
    m_poFeatureDefn->SetGeomType(wkbNone);
    m_poFeatureDefn->Reference();
    SetDescription(pszName);

    m_bKeepGeomColmuns = CPLFetchBool(
        m_poDS->GetOpenOptions(), "KEEP_GEOM_COLUMNS", false);
}

/************************************************************************/
/*                       ~PDS4TableBaseLayer()                          */
/************************************************************************/

PDS4TableBaseLayer::~PDS4TableBaseLayer()
{
    m_poFeatureDefn->Release();
    m_poRawFeatureDefn->Release();
    if( m_fp )
        VSIFCloseL(m_fp);
}

/************************************************************************/
/*                            RenameFileTo()                            */
/************************************************************************/

bool PDS4TableBaseLayer::RenameFileTo(const char* pszNewName)
{
    if( m_fp )
        VSIFCloseL(m_fp);
    m_fp = nullptr;
    CPLString osBackup(pszNewName);
    osBackup += ".bak";
    VSIRename(pszNewName, osBackup);
    bool bSuccess = VSIRename(m_osFilename, pszNewName) == 0;
    if( bSuccess )
    {
        m_fp = VSIFOpenL(pszNewName, "rb+");
        if( !m_fp )
        {
            VSIRename(osBackup, pszNewName);
            return false;
        }

        m_osFilename = pszNewName;
        VSIUnlink(osBackup);
        return true;
    }
    else
    {
        VSIRename(osBackup, pszNewName);
        return false;
    }
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char** PDS4TableBaseLayer::GetFileList() const
{
    return CSLAddString(nullptr, GetFileName());
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig PDS4TableBaseLayer::GetFeatureCount(int bForce)
{
    if( m_poAttrQuery != nullptr || m_poFilterGeom != nullptr )
    {
        return OGRLayer::GetFeatureCount(bForce);
    }
    return m_nFeatureCount;
}

/************************************************************************/
/*                           SetupGeomField()                           */
/************************************************************************/

void PDS4TableBaseLayer::SetupGeomField()
{
    const char* const *papszOpenOptions = m_poDS->GetOpenOptions();
    const char* pszWKT = CSLFetchNameValue(papszOpenOptions, "WKT");
    if( pszWKT == nullptr &&
        (m_iWKT = m_poRawFeatureDefn->GetFieldIndex("WKT")) >= 0 &&
        m_poRawFeatureDefn->GetFieldDefn(m_iWKT)->GetType() == OFTString )
    {
        pszWKT = "WKT";
    }
    else
    {
        m_iWKT = -1;
    }
    if( pszWKT && !EQUAL(pszWKT, "") )
    {
        m_iWKT = m_poRawFeatureDefn->GetFieldIndex(pszWKT);
        if( m_iWKT < 0 )
        {
            CPLError(CE_Warning, CPLE_AppDefined, "Unknown field %s", pszWKT);
        }
        else if( m_poRawFeatureDefn->GetFieldDefn(m_iWKT)->GetType() != OFTString )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "The %s field should be of type String", pszWKT);
        }
        else
        {
            m_poFeatureDefn->SetGeomType(wkbUnknown);
        }
    }
    else
    {
        const char* pszLat = CSLFetchNameValue(papszOpenOptions, "LAT");
        const char* pszLong = CSLFetchNameValue(papszOpenOptions, "LONG");
        if( pszLat == nullptr && pszLong == nullptr &&
            (m_iLatField = m_poRawFeatureDefn->GetFieldIndex("Latitude")) >= 0 &&
            (m_iLongField = m_poRawFeatureDefn->GetFieldIndex("Longitude")) >= 0 &&
            m_poRawFeatureDefn->GetFieldDefn(m_iLatField)->GetType() == OFTReal &&
            m_poRawFeatureDefn->GetFieldDefn(m_iLongField)->GetType() == OFTReal )
        {
            pszLat = "Latitude";
            pszLong = "Longitude";
        }
        else
        {
            m_iLatField = -1;
            m_iLongField = -1;
        }
        if( pszLat && pszLong && !EQUAL(pszLat, "") && !EQUAL(pszLong, "") )
        {
            m_iLatField = m_poRawFeatureDefn->GetFieldIndex(pszLat);
            m_iLongField = m_poRawFeatureDefn->GetFieldIndex(pszLong);
            if( m_iLatField < 0 )
            {
                CPLError(CE_Warning, CPLE_AppDefined, "Unknown field %s", pszLat);
            }
            else if( m_poRawFeatureDefn->GetFieldDefn(m_iLatField)->GetType() != OFTReal )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                        "The %s field should be of type Real", pszLat);
                m_iLatField = -1;
            }
            if( m_iLongField < 0 )
            {
                CPLError(CE_Warning, CPLE_AppDefined, "Unknown field %s", pszLong);
            }
            else if( m_poRawFeatureDefn->GetFieldDefn(m_iLongField)->GetType() != OFTReal )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                        "The %s field should be of type Real", pszLong);
                m_iLongField = -1;
            }
            if( m_iLatField < 0 || m_iLongField < 0 )
            {
                m_iLatField = -1;
                m_iLongField = -1;
            }
            else
            {
                const char* pszAlt = CSLFetchNameValue(papszOpenOptions, "ALT");
                if( pszAlt == nullptr &&
                    (m_iAltField = m_poRawFeatureDefn->GetFieldIndex("Altitude")) >= 0 &&
                    m_poRawFeatureDefn->GetFieldDefn(m_iAltField)->GetType() == OFTReal )
                {
                    pszAlt = "Altitude";
                }
                else
                {
                    m_iAltField = -1;
                }
                if( pszAlt && !EQUAL(pszAlt, "") )
                {
                    m_iAltField = m_poRawFeatureDefn->GetFieldIndex(pszAlt);
                    if( m_iAltField < 0 )
                    {
                        CPLError(CE_Warning, CPLE_AppDefined, "Unknown field %s", pszAlt);
                    }
                    else if( m_poRawFeatureDefn->GetFieldDefn(m_iAltField)->GetType() != OFTReal )
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                "The %s field should be of type Real", pszAlt);
                        m_iAltField = -1;
                    }
                }
                m_poFeatureDefn->SetGeomType(m_iAltField >= 0 ? wkbPoint25D : wkbPoint);
            }
        }
    }

    for( int i = 0; i < m_poRawFeatureDefn->GetFieldCount(); i++ )
    {
        if( !m_bKeepGeomColmuns &&
            (i == m_iWKT || i == m_iLatField || i == m_iLongField || i == m_iAltField) )
        {
            // do nothing;
        }
        else
        {
            m_poFeatureDefn->AddFieldDefn(m_poRawFeatureDefn->GetFieldDefn(i));
        }
    }
}

/************************************************************************/
/*                      AddGeometryFromFields()                         */
/************************************************************************/

OGRFeature* PDS4TableBaseLayer::AddGeometryFromFields(OGRFeature* poRawFeature)
{
    OGRFeature* poFeature = new OGRFeature(m_poFeatureDefn);
    poFeature->SetFID(poRawFeature->GetFID());
    for( int i = 0, j = 0; i < m_poRawFeatureDefn->GetFieldCount(); i++ )
    {
        if( !m_bKeepGeomColmuns &&
            (i == m_iWKT || i == m_iLatField || i == m_iLongField || i == m_iAltField) )
        {
            // do nothing;
        }
        else
        {
            poFeature->SetField(j, poRawFeature->GetRawFieldRef(i));
            j++;
        }
    }

    if( m_iWKT >= 0 )
    {
        const char* pszWKT = poRawFeature->GetFieldAsString(m_iWKT);
        if( pszWKT && pszWKT[0] != '\0' )
        {
            OGRGeometry* poGeom = nullptr;
            OGRGeometryFactory::createFromWkt(pszWKT, nullptr, &poGeom);
            if( poGeom )
            {
                poGeom->assignSpatialReference(GetSpatialRef());
                poFeature->SetGeometryDirectly(poGeom);
            }
        }
    }
    else if( m_iLatField >= 0 && m_iLongField >= 0 &&
             poRawFeature->IsFieldSetAndNotNull(m_iLatField) &&
             poRawFeature->IsFieldSetAndNotNull(m_iLongField) )
    {
        double dfLat = poRawFeature->GetFieldAsDouble(m_iLatField);
        double dfLong = poRawFeature->GetFieldAsDouble(m_iLongField);
        OGRPoint* poPoint;
        if( m_iAltField >= 0 && poRawFeature->IsFieldSetAndNotNull(m_iAltField) )
        {
            double dfAlt = poRawFeature->GetFieldAsDouble(m_iAltField);
            poPoint = new OGRPoint(dfLong, dfLat, dfAlt);
        }
        else
        {
            poPoint = new OGRPoint(dfLong, dfLat);
        }
        poPoint->assignSpatialReference(GetSpatialRef());
        poFeature->SetGeometryDirectly(poPoint);
    }
    return poFeature;
}

/************************************************************************/
/*                      AddFieldsFromGeometry()                         */
/************************************************************************/

OGRFeature* PDS4TableBaseLayer::AddFieldsFromGeometry(OGRFeature* poFeature)
{
    OGRFeature* poRawFeature = new OGRFeature(m_poRawFeatureDefn);
    for( int i = 0, j = 0; i < m_poRawFeatureDefn->GetFieldCount(); i++ )
    {
        if( !m_bKeepGeomColmuns &&
            (i == m_iWKT || i == m_iLatField || i == m_iLongField || i == m_iAltField) )
        {
            // do nothing;
        }
        else
        {
            poRawFeature->SetField(i, poFeature->GetRawFieldRef(j));
            j++;
        }
    }

    auto poGeom = poFeature->GetGeometryRef();
    if( poGeom )
    {
        if( m_iLongField >= 0 && m_iLatField >= 0 &&
            wkbFlatten(poGeom->getGeometryType()) == wkbPoint )
        {
            auto poPoint = poGeom->toPoint();
            poRawFeature->SetField(m_iLongField, poPoint->getX());
            poRawFeature->SetField(m_iLatField, poPoint->getY());
            if( m_iAltField >= 0 && poGeom->getGeometryType() == wkbPoint25D )
            {
                poRawFeature->SetField(m_iAltField, poPoint->getZ());
            }
        }
        else if( m_iWKT >= 0 )
        {
            char* pszWKT = nullptr;
            poGeom->exportToWkt(&pszWKT);
            if( pszWKT )
            {
                poRawFeature->SetField(m_iWKT, pszWKT);
            }
            CPLFree(pszWKT);
        }
    }
    return poRawFeature;
}

/************************************************************************/
/*                         MarkHeaderDirty()                            */
/************************************************************************/

void PDS4TableBaseLayer::MarkHeaderDirty()
{
    m_bDirtyHeader = true;
    m_poDS->MarkHeaderDirty();
}

/************************************************************************/
/*              RefreshFileAreaObservationalBeginningCommon()           */
/************************************************************************/

CPLXMLNode* PDS4TableBaseLayer::RefreshFileAreaObservationalBeginningCommon(
                                                CPLXMLNode* psFAO,
                                                const CPLString& osPrefix,
                                                const char* pszTableEltName,
                                                CPLString& osDescription)
{
    CPLXMLNode* psFile = CPLGetXMLNode(psFAO, (osPrefix + "File").c_str());
    CPLAssert(psFile);
    CPLXMLNode* psfile_size = CPLGetXMLNode(psFile, (osPrefix + "file_size").c_str());
    if( psfile_size )
    {
        CPLRemoveXMLChild(psFile, psfile_size);
        CPLDestroyXMLNode(psfile_size);
    }

    CPLXMLNode* psHeader = CPLGetXMLNode(psFAO, (osPrefix + "Header").c_str());
    if( psHeader )
    {
        CPLRemoveXMLChild(psFAO, psHeader);
        CPLDestroyXMLNode(psHeader);
    }

    CPLString osTableEltName(osPrefix + pszTableEltName);
    CPLXMLNode* psTable = CPLGetXMLNode(psFAO, osTableEltName);
    CPLString osName;
    CPLString osLocalIdentifier;
    if( psTable )
    {
        osName = CPLGetXMLValue(psTable, (osPrefix + "name").c_str(), "");
        osLocalIdentifier = CPLGetXMLValue(psTable, (osPrefix + "local_identifier").c_str(), "");
        osDescription = CPLGetXMLValue(psTable, (osPrefix + "description").c_str(), "");
        CPLRemoveXMLChild(psFAO, psTable);
        CPLDestroyXMLNode(psTable);
    }

    // Write Table_Delimited/Table_Character/Table_Binary
    psTable = CPLCreateXMLNode(psFAO, CXT_Element, osTableEltName);
    if( !osName.empty() )
        CPLCreateXMLElementAndValue(psTable,
                                    (osPrefix + "name").c_str(), osName);
    if( !osLocalIdentifier.empty() )
        CPLCreateXMLElementAndValue(psTable,
                                    (osPrefix + "local_identifier").c_str(),
                                    osLocalIdentifier);
    else
        CPLCreateXMLElementAndValue(psTable,
                                    (osPrefix + "local_identifier").c_str(),
                                    GetName());

    CPLXMLNode* psOffset = CPLCreateXMLElementAndValue(
        psTable, (osPrefix + "offset").c_str(),
        CPLSPrintf(CPL_FRMT_GUIB, m_nOffset));
    CPLAddXMLAttributeAndValue(psOffset, "unit", "byte");

    return psTable;
}

/************************************************************************/
/*                        ParseLineEndingOption()                       */
/************************************************************************/

void PDS4TableBaseLayer::ParseLineEndingOption(CSLConstList papszOptions)
{
    const char* pszLineEnding = CSLFetchNameValueDef(papszOptions, "LINE_ENDING", "CRLF");
    if( EQUAL(pszLineEnding, "CRLF") )
    {
        m_osLineEnding = "\r\n";
    }
    else if( EQUAL(pszLineEnding, "LF") )
    {
        m_osLineEnding = "\n";
    }
    else
    {
        m_osLineEnding = "\r\n";
        CPLError(CE_Warning, CPLE_AppDefined, "Unhandled value for LINE_ENDING");
    }
}

/************************************************************************/
/* ==================================================================== */
/*                        PDS4FixedWidthTable                           */
/* ==================================================================== */
/************************************************************************/

PDS4FixedWidthTable::PDS4FixedWidthTable(PDS4Dataset* poDS,
                                       const char* pszName,
                                       const char* pszFilename) :
    PDS4TableBaseLayer(poDS, pszName, pszFilename)
{
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void PDS4FixedWidthTable::ResetReading()
{
    m_nFID = 1;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature* PDS4FixedWidthTable::GetNextFeature()
{
    while(true)
    {
        auto poFeature = GetFeature(m_nFID);
        if( poFeature == nullptr )
        {
            return nullptr;
        }
        ++m_nFID;

        if((m_poFilterGeom == nullptr
            || FilterGeometry( poFeature->GetGeometryRef() ) )
        && (m_poAttrQuery == nullptr
            || m_poAttrQuery->Evaluate( poFeature )) )
        {
            return poFeature;
        }
        delete poFeature;
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int PDS4FixedWidthTable::TestCapability(const char* pszCap)
{
    if( EQUAL(pszCap, OLCRandomRead) ||
        EQUAL(pszCap, OLCStringsAsUTF8) )
    {
        return true;
    }
    if( EQUAL(pszCap, OLCFastFeatureCount) )
    {
        return m_poAttrQuery == nullptr && m_poFilterGeom == nullptr;
    }
    if( EQUAL(pszCap, OLCCreateField) )
    {
        return m_poDS->GetAccess() == GA_Update && m_nFeatureCount == 0;
    }
    if( EQUAL(pszCap, OLCSequentialWrite) ||
        EQUAL(pszCap, OLCRandomWrite) )
    {
        return m_poDS->GetAccess() == GA_Update;
    }
    return false;
}

/************************************************************************/
/*                            ISetFeature()                             */
/************************************************************************/

OGRErr PDS4FixedWidthTable::ISetFeature( OGRFeature *poFeature )
{
    if( poFeature->GetFID() <= 0 || poFeature->GetFID() > m_nFeatureCount )
    {
        return OGRERR_NON_EXISTING_FEATURE;
    }
    if( m_poDS->GetAccess() != GA_Update )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset opened in read-only mode");
        return OGRERR_FAILURE;
    }
    CPLAssert( static_cast<int>(m_osBuffer.size()) == m_nRecordSize );
    CPLAssert( m_nRecordSize > static_cast<int>(m_osLineEnding.size()) );

    VSIFSeekL(m_fp, m_nOffset + (poFeature->GetFID() - 1) * m_nRecordSize, SEEK_SET);
    memset(&m_osBuffer[0], ' ', m_nRecordSize);

    OGRFeature* poRawFeature = AddFieldsFromGeometry(poFeature);
    for( int i = 0; i < m_poRawFeatureDefn->GetFieldCount(); i++ )
    {
        if( !poRawFeature->IsFieldSetAndNotNull(i) )
        {
            continue;
        }
        CPLString osBuffer;
        const CPLString& osDT(m_aoFields[i].m_osDataType);
        const auto eType( m_poRawFeatureDefn->GetFieldDefn(i)->GetType() );
        if( osDT == "ASCII_Real" )
        {
            CPLString osFormat;
            osFormat.Printf("%%.%dg", m_aoFields[i].m_nLength - 2);
            osBuffer.Printf(osFormat.c_str(), poRawFeature->GetFieldAsDouble(i));
        }
        else if( osDT == "ASCII_Integer" ||
                 osDT == "ASCII_NonNegative_Integer" ||
                 eType == OFTString )
        {
            osBuffer = poRawFeature->GetFieldAsString(i);
        }
        else if( osDT == "ASCII_Boolean" )
        {
            osBuffer = poRawFeature->GetFieldAsInteger(i) == 1 ? "1" : "0";
        }
        else if( osDT == "IEEE754LSBDouble" )
        {
            double dfVal = poRawFeature->GetFieldAsDouble(i);
            CPL_LSBPTR64(&dfVal);
            osBuffer.resize(sizeof(dfVal));
            memcpy(&osBuffer[0], &dfVal, sizeof(dfVal));
        }
        else if( osDT == "IEEE754MSBDouble" )
        {
            double dfVal = poRawFeature->GetFieldAsDouble(i);
            CPL_MSBPTR64(&dfVal);
            osBuffer.resize(sizeof(dfVal));
            memcpy(&osBuffer[0], &dfVal, sizeof(dfVal));
        }
        else if( osDT == "IEEE754LSBSingle" )
        {
            float fVal = static_cast<float>(poRawFeature->GetFieldAsDouble(i));
            CPL_LSBPTR32(&fVal);
            osBuffer.resize(sizeof(fVal));
            memcpy(&osBuffer[0], &fVal, sizeof(fVal));
        }
        else if( osDT == "IEEE754MSBSingle" )
        {
            float fVal = static_cast<float>(poRawFeature->GetFieldAsDouble(i));
            CPL_MSBPTR32(&fVal);
            osBuffer.resize(sizeof(fVal));
            memcpy(&osBuffer[0], &fVal, sizeof(fVal));
        }
        else if( osDT == "SignedByte" )
        {
            signed char bVal = static_cast<signed char>(
                std::max(-128, std::min(127, poRawFeature->GetFieldAsInteger(i))));
            osBuffer.resize(sizeof(bVal));
            memcpy(&osBuffer[0], &bVal, sizeof(bVal));
        }
        else if( osDT == "UnsignedByte" )
        {
            GByte ubVal = static_cast<GByte>(
                std::max(0, std::min(255, poRawFeature->GetFieldAsInteger(i))));
            osBuffer.resize(sizeof(ubVal));
            memcpy(&osBuffer[0], &ubVal, sizeof(ubVal));
        }
        else if( osDT == "SignedLSB2" )
        {
            GInt16 sVal = static_cast<GInt16>(std::max(
                -32768, std::min(32767, poRawFeature->GetFieldAsInteger(i))));
            CPL_LSBPTR16(&sVal);
            osBuffer.resize(sizeof(sVal));
            memcpy(&osBuffer[0], &sVal, sizeof(sVal));
        }
        else if( osDT == "SignedMSB2" )
        {
            GInt16 sVal = static_cast<GInt16>(std::max(
                -32768, std::min(32767, poRawFeature->GetFieldAsInteger(i))));
            CPL_MSBPTR16(&sVal);
            osBuffer.resize(sizeof(sVal));
            memcpy(&osBuffer[0], &sVal, sizeof(sVal));
        }
        else if( osDT == "UnsignedLSB2" )
        {
            GUInt16 usVal = static_cast<GUInt16>(std::max(
                0, std::min(65535, poRawFeature->GetFieldAsInteger(i))));
            CPL_LSBPTR16(&usVal);
            osBuffer.resize(sizeof(usVal));
            memcpy(&osBuffer[0], &usVal, sizeof(usVal));
        }
        else if( osDT == "UnsignedMSB2" )
        {
            GUInt16 usVal = static_cast<GUInt16>(std::max(
                0, std::min(65535, poRawFeature->GetFieldAsInteger(i))));
            CPL_MSBPTR16(&usVal);
            osBuffer.resize(sizeof(usVal));
            memcpy(&osBuffer[0], &usVal, sizeof(usVal));
        }
        else if( osDT == "SignedLSB4" )
        {
            GInt32 nVal = poRawFeature->GetFieldAsInteger(i);
            CPL_LSBPTR32(&nVal);
            osBuffer.resize(sizeof(nVal));
            memcpy(&osBuffer[0], &nVal, sizeof(nVal));
        }
        else if( osDT == "SignedMSB4" )
        {
            GInt32 nVal = poRawFeature->GetFieldAsInteger(i);
            CPL_MSBPTR32(&nVal);
            osBuffer.resize(sizeof(nVal));
            memcpy(&osBuffer[0], &nVal, sizeof(nVal));
        }
        else if( osDT == "UnsignedLSB4" )
        {
            GUInt32 nVal = static_cast<GUInt32>(
                std::max(0, poRawFeature->GetFieldAsInteger(i)));
            CPL_LSBPTR32(&nVal);
            osBuffer.resize(sizeof(nVal));
            memcpy(&osBuffer[0], &nVal, sizeof(nVal));
        }
        else if( osDT == "UnsignedMSB4" )
        {
            GUInt32 nVal = static_cast<GUInt32>(
                std::max(0, poRawFeature->GetFieldAsInteger(i)));
            CPL_MSBPTR32(&nVal);
            osBuffer.resize(sizeof(nVal));
            memcpy(&osBuffer[0], &nVal, sizeof(nVal));
        }
        else if( osDT == "SignedLSB8" )
        {
            GInt64 nVal = poRawFeature->GetFieldAsInteger64(i);
            CPL_LSBPTR64(&nVal);
            osBuffer.resize(sizeof(nVal));
            memcpy(&osBuffer[0], &nVal, sizeof(nVal));
        }
        else if( osDT == "SignedMSB8" )
        {
            GInt64 nVal = poRawFeature->GetFieldAsInteger64(i);
            CPL_MSBPTR64(&nVal);
            osBuffer.resize(sizeof(nVal));
            memcpy(&osBuffer[0], &nVal, sizeof(nVal));
        }
        else if( osDT == "UnsignedLSB8" )
        {
            GUInt64 nVal = static_cast<GUInt64>(
                std::max(static_cast<GIntBig>(0),
                         poRawFeature->GetFieldAsInteger64(i)));
            CPL_LSBPTR64(&nVal);
            osBuffer.resize(sizeof(nVal));
            memcpy(&osBuffer[0], &nVal, sizeof(nVal));
        }
        else if( osDT == "UnsignedMSB8" )
        {
            GUInt64 nVal = static_cast<GUInt64>(
                std::max(static_cast<GIntBig>(0),
                         poRawFeature->GetFieldAsInteger64(i)));
            CPL_MSBPTR64(&nVal);
            osBuffer.resize(sizeof(nVal));
            memcpy(&osBuffer[0], &nVal, sizeof(nVal));
        }
        else if( osDT == "ASCII_Date_Time_YMD" ||
                 osDT == "ASCII_Date_Time_YMD_UTC" )
        {
            char* pszDateTime = OGRGetXMLDateTime(poRawFeature->GetRawFieldRef(i));
            osBuffer = pszDateTime;
            CPLFree(pszDateTime);
        }
        else if( osDT == "ASCII_Date_YMD" )
        {
            int nYear, nMonth, nDay;
            poRawFeature->GetFieldAsDateTime(i, &nYear, &nMonth, &nDay,
                                          nullptr, nullptr,
                                          static_cast<float*>(nullptr), nullptr);
            osBuffer.Printf("%04d-%02d-%02d", nYear, nMonth, nDay);
        }
        else if( osDT == "ASCII_Time" )
        {
            int nHour, nMin;
            float fSec;
            poRawFeature->GetFieldAsDateTime(i, nullptr, nullptr, nullptr,
                                          &nHour, &nMin, &fSec, nullptr);
            osBuffer.Printf("%02d:%02d:%05.3f", nHour, nMin, fSec);
        }

        if( !osBuffer.empty() &&
            osBuffer.size() <= static_cast<size_t>(m_aoFields[i].m_nLength) )
        {
            memcpy(&m_osBuffer[m_aoFields[i].m_nOffset +
                                m_aoFields[i].m_nLength - osBuffer.size()],
                    &osBuffer[0], osBuffer.size());
        }
        else if( !osBuffer.empty() )
        {
            if( eType == OFTString )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Value %s for field %s is too large. Truncating it",
                         osBuffer.c_str(),
                         m_poRawFeatureDefn->GetFieldDefn(i)->GetNameRef());
                memcpy(&m_osBuffer[m_aoFields[i].m_nOffset],
                       osBuffer.data(), m_aoFields[i].m_nLength);
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Value %s for field %s is too large. Omitting i",
                         osBuffer.c_str(),
                         m_poRawFeatureDefn->GetFieldDefn(i)->GetNameRef());
            }
        }
    }
    delete poRawFeature;

    if( !m_osLineEnding.empty() )
    {
        memcpy(&m_osBuffer[m_osBuffer.size() - m_osLineEnding.size()],
               m_osLineEnding.data(),
               m_osLineEnding.size());
    }

    if( VSIFWriteL(&m_osBuffer[0], m_nRecordSize, 1, m_fp) != 1 )
    {
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           ICreateFeature()                           */
/************************************************************************/

OGRErr PDS4FixedWidthTable::ICreateFeature( OGRFeature *poFeature )
{
    m_nFeatureCount ++;
    poFeature->SetFID(m_nFeatureCount);
    OGRErr eErr = ISetFeature(poFeature);
    if( eErr == OGRERR_NONE )
    {
        MarkHeaderDirty();
    }
    else
    {
        poFeature->SetFID(-1);
        m_nFeatureCount --;
    }
    return eErr;
}

/************************************************************************/
/*                              GetFeature()                            */
/************************************************************************/

OGRFeature* PDS4FixedWidthTable::GetFeature(GIntBig nFID)
{
    if( nFID <= 0 || nFID > m_nFeatureCount )
    {
        return nullptr;
    }
    VSIFSeekL(m_fp, m_nOffset + (nFID-1) * m_nRecordSize, SEEK_SET);
    if( VSIFReadL(&m_osBuffer[0], m_nRecordSize, 1, m_fp) != 1 )
    {
        return nullptr;
    }
    OGRFeature* poRawFeature = new OGRFeature(m_poRawFeatureDefn);
    poRawFeature->SetFID(nFID);
    for( int i = 0; i < poRawFeature->GetFieldCount(); i++ )
    {
        CPLString osVal(
            m_osBuffer.substr(m_aoFields[i].m_nOffset, m_aoFields[i].m_nLength));

        const CPLString& osDT(m_aoFields[i].m_osDataType);
        if( STARTS_WITH(osDT, "ASCII_") || STARTS_WITH(osDT, "UTF8_") )
        {
            osVal.Trim();
            if( osVal.empty() )
            {
                continue;
            }
        }

        if( osDT == "IEEE754LSBDouble" )
        {
            double dfVal;
            CPLAssert(osVal.size() == sizeof(dfVal));
            memcpy(&dfVal, osVal.data(), sizeof(dfVal));
            CPL_LSBPTR64(&dfVal);
            poRawFeature->SetField(i, dfVal);
        }
        else if( osDT == "IEEE754MSBDouble" )
        {
            double dfVal;
            CPLAssert(osVal.size() == sizeof(dfVal));
            memcpy(&dfVal, osVal.data(), sizeof(dfVal));
            CPL_MSBPTR64(&dfVal);
            poRawFeature->SetField(i, dfVal);
        }
        else if( osDT == "IEEE754LSBSingle" )
        {
            float fVal;
            CPLAssert(osVal.size() == sizeof(fVal));
            memcpy(&fVal, osVal.data(), sizeof(fVal));
            CPL_LSBPTR32(&fVal);
            poRawFeature->SetField(i, fVal);
        }
        else if( osDT == "IEEE754MSBSingle" )
        {
            float fVal;
            CPLAssert(osVal.size() == sizeof(fVal));
            memcpy(&fVal, osVal.data(), sizeof(fVal));
            CPL_MSBPTR32(&fVal);
            poRawFeature->SetField(i, fVal);
        }
        else if( osDT == "SignedByte" )
        {
            signed char bVal;
            CPLAssert(osVal.size() == sizeof(bVal));
            memcpy(&bVal, osVal.data(), sizeof(bVal));
            poRawFeature->SetField(i, bVal);
        }
        else if( osDT == "UnsignedByte" )
        {
            GByte bVal;
            CPLAssert(osVal.size() == sizeof(bVal));
            memcpy(&bVal, osVal.data(), sizeof(bVal));
            poRawFeature->SetField(i, bVal);
        }
        else if( osDT == "SignedLSB2" )
        {
            GInt16 sVal;
            CPLAssert(osVal.size() == sizeof(sVal));
            memcpy(&sVal, osVal.data(), sizeof(sVal));
            CPL_LSBPTR16(&sVal);
            poRawFeature->SetField(i, sVal);
        }
        else if( osDT == "SignedMSB2" )
        {
            GInt16 sVal;
            CPLAssert(osVal.size() == sizeof(sVal));
            memcpy(&sVal, osVal.data(), sizeof(sVal));
            CPL_MSBPTR16(&sVal);
            poRawFeature->SetField(i, sVal);
        }
        else if( osDT == "UnsignedLSB2" )
        {
            GUInt16 usVal;
            CPLAssert(osVal.size() == sizeof(usVal));
            memcpy(&usVal, osVal.data(), sizeof(usVal));
            CPL_LSBPTR16(&usVal);
            poRawFeature->SetField(i, usVal);
        }
        else if( osDT == "UnsignedMSB2" )
        {
            GUInt16 usVal;
            CPLAssert(osVal.size() == sizeof(usVal));
            memcpy(&usVal, osVal.data(), sizeof(usVal));
            CPL_MSBPTR16(&usVal);
            poRawFeature->SetField(i, usVal);
        }
        else if( osDT == "SignedLSB4" )
        {
            GInt32 nVal;
            CPLAssert(osVal.size() == sizeof(nVal));
            memcpy(&nVal, osVal.data(), sizeof(nVal));
            CPL_LSBPTR32(&nVal);
            poRawFeature->SetField(i, nVal);
        }
        else if( osDT == "SignedMSB4" )
        {
            GInt32 nVal;
            CPLAssert(osVal.size() == sizeof(nVal));
            memcpy(&nVal, osVal.data(), sizeof(nVal));
            CPL_MSBPTR32(&nVal);
            poRawFeature->SetField(i, nVal);
        }
        else if( osDT == "UnsignedLSB4" )
        {
            GUInt32 nVal;
            CPLAssert(osVal.size() == sizeof(nVal));
            memcpy(&nVal, osVal.data(), sizeof(nVal));
            CPL_LSBPTR32(&nVal);
            poRawFeature->SetField(i, static_cast<GIntBig>(nVal));
        }
        else if( osDT == "UnsignedMSB4" )
        {
            GUInt32 nVal;
            CPLAssert(osVal.size() == sizeof(nVal));
            memcpy(&nVal, osVal.data(), sizeof(nVal));
            CPL_MSBPTR32(&nVal);
            poRawFeature->SetField(i, static_cast<GIntBig>(nVal));
        }
        else if( osDT == "SignedLSB8" )
        {
            GInt64 nVal;
            CPLAssert(osVal.size() == sizeof(nVal));
            memcpy(&nVal, osVal.data(), sizeof(nVal));
            CPL_LSBPTR64(&nVal);
            poRawFeature->SetField(i, nVal);
        }
        else if( osDT == "SignedMSB8" )
        {
            GInt64 nVal;
            CPLAssert(osVal.size() == sizeof(nVal));
            memcpy(&nVal, osVal.data(), sizeof(nVal));
            CPL_MSBPTR64(&nVal);
            poRawFeature->SetField(i, nVal);
        }
        else if( osDT == "UnsignedLSB8" )
        {
            GUInt64 nVal;
            CPLAssert(osVal.size() == sizeof(nVal));
            memcpy(&nVal, osVal.data(), sizeof(nVal));
            CPL_LSBPTR64(&nVal);
            poRawFeature->SetField(i, static_cast<GIntBig>(nVal));
        }
        else if( osDT == "UnsignedMSB8" )
        {
            GUInt64 nVal;
            CPLAssert(osVal.size() == sizeof(nVal));
            memcpy(&nVal, osVal.data(), sizeof(nVal));
            CPL_MSBPTR64(&nVal);
            poRawFeature->SetField(i, static_cast<GIntBig>(nVal));
        }
        else if( osDT == "ASCII_Boolean" )
        {
            poRawFeature->SetField(i, EQUAL(osVal, "t") || EQUAL(osVal, "1") ? 1 : 0);
        }
        else
        {
            poRawFeature->SetField(i, osVal.c_str());
        }
    }
    OGRFeature* poFeature = AddGeometryFromFields(poRawFeature);
    delete poRawFeature;
    return poFeature;
}

/************************************************************************/
/*                     GetFieldTypeFromPDS4DataType()                   */
/************************************************************************/

static OGRFieldType GetFieldTypeFromPDS4DataType(const char* pszDataType,
                                                 int nDTSize,
                                                 OGRFieldSubType& eSubType,
                                                 bool& error)
{
    OGRFieldType eType = OFTString;
    eSubType = OFSTNone;
    error = false;
    if( EQUAL(pszDataType, "ASCII_Boolean") )
    {
        eSubType = OFSTBoolean;
        eType = OFTInteger;
    }
    else if( EQUAL(pszDataType, "ASCII_Date_Time_YMD") ||
             EQUAL(pszDataType, "ASCII_Date_Time_YMD_UTC") )
    {
        eType = OFTDateTime;
    }
    else if( EQUAL(pszDataType, "ASCII_Date_YMD") )
    {
        eType = OFTDate;
    }
    else if( EQUAL(pszDataType, "ASCII_Integer") ||
             EQUAL(pszDataType, "ASCII_NonNegative_Integer") )
    {
        eType = OFTInteger;
    }
    else if( EQUAL(pszDataType, "SignedByte") ||
             EQUAL(pszDataType, "UnsignedByte") )
    {
        if( nDTSize != 1 )
            error = true;
        eType = OFTInteger;
    }
    else if( EQUAL(pszDataType, "SignedLSB2") ||
             EQUAL(pszDataType, "SignedMSB2") )
    {
        if( nDTSize != 2 )
            error = true;
        eType = OFTInteger;
        eSubType = OFSTInt16;
    }
    else if( EQUAL(pszDataType, "UnsignedLSB2") ||
             EQUAL(pszDataType, "UnsignedMSB2") )
    {
        if( nDTSize != 2 )
            error = true;
        eType = OFTInteger;
    }
    else if( EQUAL(pszDataType, "SignedLSB4") ||
             EQUAL(pszDataType, "SignedMSB4") )
    {
        if( nDTSize != 4 )
            error = true;
        eType = OFTInteger;
    }
    else if( EQUAL(pszDataType, "UnsignedLSB4") ||
             EQUAL(pszDataType, "UnsignedMSB4") )
    {
        if( nDTSize != 4 )
            error = true;
        // Use larger data type as > 2 billion values don't hold on signed int32
        eType = OFTInteger64;
    }
    else if( EQUAL(pszDataType, "SignedLSB8") ||
             EQUAL(pszDataType, "SignedMSB8") )
    {
        if( nDTSize != 8 )
            error = true;
        eType = OFTInteger64;
    }
    else if( EQUAL(pszDataType, "UnsignedLSB8") ||
             EQUAL(pszDataType, "UnsignedMSB8") )
    {
        if( nDTSize != 8 )
            error = true;
        // Hope that we won't get value larger than > 2^63...
        eType = OFTInteger64;
    }
    else if( EQUAL(pszDataType, "ASCII_Real") )
    {
        eType = OFTReal;
    }
    else if( EQUAL(pszDataType, "IEEE754LSBDouble") ||
             EQUAL(pszDataType, "IEEE754MSBDouble") )
    {
        if( nDTSize != 8 )
            error = true;
        eType = OFTReal;
    }
    else if( EQUAL(pszDataType, "IEEE754LSBSingle") ||
             EQUAL(pszDataType, "IEEE754MSBSingle") )
    {
        if( nDTSize != 4 )
            error = true;
        eType = OFTReal;
        eSubType = OFSTFloat32;
    }
    else if( EQUAL(pszDataType, "ASCII_Time") )
    {
        eType = OFTTime;
    }
    return eType;
}

/************************************************************************/
/*                            ReadTableDef()                            */
/************************************************************************/

bool PDS4FixedWidthTable::ReadTableDef(const CPLXMLNode* psTable)
{
    CPLAssert( m_fp == nullptr );
    m_fp = VSIFOpenL(m_osFilename,
                     (m_poDS->GetAccess() == GA_ReadOnly ) ? "rb" : "r+b");
    if( !m_fp )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s",
                 m_osFilename.c_str());
        return false;
    }

    m_nOffset = static_cast<GUIntBig>(
        CPLAtoGIntBig(CPLGetXMLValue(psTable, "offset", "0")));

    m_nFeatureCount = CPLAtoGIntBig(
        CPLGetXMLValue(psTable, "records", "-1"));

    const char* pszRecordDelimiter = CPLGetXMLValue(psTable, "record_delimiter", "");
    if( EQUAL(pszRecordDelimiter, "Carriage-Return Line-Feed") )
        m_osLineEnding = "\r\n";
    else if( EQUAL(pszRecordDelimiter, "Line-Feed") )
        m_osLineEnding = "\n";
    else if( EQUAL(pszRecordDelimiter, "") )
    {
        if( GetSubType() == "Character" )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Missing record_delimiter");
            return false;
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid record_delimiter");
        return false;
    }

    const CPLXMLNode* psRecord =
        CPLGetXMLNode(psTable, ("Record_" + GetSubType()).c_str());
    if( !psRecord )
    {
        return false;
    }
    m_nRecordSize = atoi(CPLGetXMLValue(psRecord, "record_length", "0"));
    if( m_nRecordSize <= static_cast<int>(m_osLineEnding.size()) ||
        m_nRecordSize > 1000 * 1000 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid record_length");
        return false;
    }
    m_osBuffer.resize(m_nRecordSize);
    if( !ReadFields(psRecord, 0, "") )
    {
        return false;
    }

    SetupGeomField();

    return true;
}

/************************************************************************/
/*                             ReadFields()                             */
/************************************************************************/

bool PDS4FixedWidthTable::ReadFields(const CPLXMLNode* psParent,
                                     int nBaseOffset,
                                     const CPLString& osSuffixFieldName)
{
    for( const CPLXMLNode* psIter = psParent->psChild;
                           psIter; psIter = psIter->psNext )
    {
        if( psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, ("Field_" + GetSubType()).c_str()) == 0 )
        {
            const char* pszName = CPLGetXMLValue(psIter, "name", nullptr);
            if( !pszName )
            {
                return false;
            }
            const char* pszLoc = CPLGetXMLValue(psIter, "field_location", nullptr);
            if( !pszLoc )
            {
                return false;
            }
            const char* pszDataType = CPLGetXMLValue(psIter, "data_type", nullptr);
            if( !pszDataType )
            {
                return false;
            }
            const char* pszFieldLength = CPLGetXMLValue(psIter, "field_length", nullptr);
            if( !pszFieldLength )
            {
                return false;
            }
            Field f;
            f.m_nOffset = nBaseOffset + atoi(pszLoc) - 1; // Location is 1-based
            if( f.m_nOffset < 0 || f.m_nOffset >= m_nRecordSize )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid field_location");
                return false;
            }
            f.m_nLength = atoi(pszFieldLength);
            if( f.m_nLength <= 0 || f.m_nLength > m_nRecordSize - static_cast<int>(m_osLineEnding.size()) - f.m_nOffset )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid field_length");
                return false;
            }
            f.m_osDataType = pszDataType;
            f.m_osUnit = CPLGetXMLValue(psIter, "unit", "");
            f.m_osDescription = CPLGetXMLValue(psIter, "description", "");

            const char* pszFieldFormat = CPLGetXMLValue(psIter, "field_format", "");

            CPLXMLNode* psSpecialConstants = const_cast<CPLXMLNode*>(
                CPLGetXMLNode(psIter, "Special_Constants"));
            if( psSpecialConstants )
            {
                auto psNext = psSpecialConstants->psNext;
                psSpecialConstants->psNext = nullptr;
                char* pszXML = CPLSerializeXMLTree(psSpecialConstants);
                psSpecialConstants->psNext = psNext;
                if( pszXML )
                {
                    f.m_osSpecialConstantsXML = pszXML;
                    CPLFree(pszXML);
                }
            }

            m_aoFields.push_back(f);

            OGRFieldSubType eSubType = OFSTNone;
            bool error = false;
            auto eType = GetFieldTypeFromPDS4DataType(pszDataType, f.m_nLength, eSubType, error);
            if( error )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Inconsistent field_length w.r.t datatype");
                return false;
            }
            if( STARTS_WITH(f.m_osDataType, "ASCII_") &&
                eType == OFTInteger && f.m_nLength >= 10 )
            {
                eType = OFTInteger64;
            }
            OGRFieldDefn oFieldDefn((pszName + osSuffixFieldName).c_str(), eType);
            oFieldDefn.SetSubType(eSubType);
            if( eType != OFTReal &&
                (STARTS_WITH(f.m_osDataType, "ASCII_") ||
                 STARTS_WITH(f.m_osDataType, "UTF_8")) )
            {
                oFieldDefn.SetWidth(f.m_nLength);
            }
            else if( (eType == OFTInteger || eType == OFTInteger64) &&
                     pszFieldFormat &&
                     pszFieldFormat[0] == '%' &&
                     pszFieldFormat[strlen(pszFieldFormat)-1] == 'd' )
            {
                oFieldDefn.SetWidth(atoi(pszFieldFormat+1));
            }
            m_poRawFeatureDefn->AddFieldDefn(&oFieldDefn);
        }
        else if( psIter->eType == CXT_Element &&
                 strcmp(psIter->pszValue, ("Group_Field_" + GetSubType()).c_str()) == 0 )
        {
            const char* pszRepetitions = CPLGetXMLValue(psIter, "repetitions", nullptr);
            if( !pszRepetitions )
            {
                return false;
            }
            const char* pszGroupLocation = CPLGetXMLValue(psIter, "group_location", nullptr);
            if( !pszGroupLocation )
            {
                return false;
            }
            const char* pszGroupLength = CPLGetXMLValue(psIter, "group_length", nullptr);
            if( !pszGroupLength )
            {
                return false;
            }
            int nRepetitions = std::min(1000, atoi(pszRepetitions));
            if( nRepetitions <= 0 )
            {
                return false;
            }
            int nGroupOffset = atoi(pszGroupLocation) - 1; // Location is 1-based
            if( nGroupOffset < 0 || nGroupOffset >= m_nRecordSize )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid group_location");
                return false;
            }
            int nGroupLength = atoi(pszGroupLength);
            if( nGroupLength <= 0 ||
                nGroupLength > m_nRecordSize - static_cast<int>(m_osLineEnding.size()) - nGroupOffset ||
                (nGroupLength % nRepetitions) != 0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid group_length");
                return false;
            }
            int nGroupOneRepetitionLength = nGroupLength / nRepetitions;
            for( int i = 0; i < nRepetitions; i++ )
            {
                if( !ReadFields(psIter, nGroupOffset + i * nGroupOneRepetitionLength,
                                osSuffixFieldName + "_" + CPLSPrintf("%d", i+1)) )
                {
                    return false;
                }
            }
        }
    }
    return true;
}

/************************************************************************/
/*                      RefreshFileAreaObservational()                  */
/************************************************************************/

void PDS4FixedWidthTable::RefreshFileAreaObservational(CPLXMLNode* psFAO)
{
    CPLString osPrefix;
    if( STARTS_WITH(psFAO->pszValue, "pds:") )
        osPrefix = "pds:";

    CPLString osDescription;
    CPLXMLNode* psTable = RefreshFileAreaObservationalBeginningCommon(
        psFAO, osPrefix, ("Table_" + GetSubType()).c_str(), osDescription);

    CPLCreateXMLElementAndValue(psTable,
                                (osPrefix + "records").c_str(),
                                CPLSPrintf(CPL_FRMT_GIB, m_nFeatureCount));
    if( !osDescription.empty() )
        CPLCreateXMLElementAndValue(psTable,
                                    (osPrefix + "description").c_str(),
                                    osDescription);
    if( m_osLineEnding == "\r\n" )
    {
        CPLCreateXMLElementAndValue(psTable,
                                    (osPrefix + "record_delimiter").c_str(),
                                    "Carriage-Return Line-Feed");
    }
    else if( m_osLineEnding == "\n" )
    {
        CPLCreateXMLElementAndValue(psTable,
                                    (osPrefix + "record_delimiter").c_str(),
                                    "Line-Feed");
    }

    // Write Record_Character / Record_Binary
    CPLXMLNode* psRecord = CPLCreateXMLNode(
        psTable, CXT_Element, (osPrefix + "Record_" + GetSubType()).c_str());
    CPLCreateXMLElementAndValue(psRecord,
                        (osPrefix + "fields").c_str(),
                        CPLSPrintf("%d", static_cast<int>(m_aoFields.size())));
    CPLCreateXMLElementAndValue(psRecord,
                                (osPrefix + "groups").c_str(), "0");
    CPLXMLNode* psrecord_length = CPLCreateXMLElementAndValue(
        psRecord, (osPrefix + "record_length").c_str(),
        CPLSPrintf("%d", m_nRecordSize));
    CPLAddXMLAttributeAndValue(psrecord_length, "unit", "byte");

    CPLAssert(static_cast<int>(m_aoFields.size()) ==
                        m_poRawFeatureDefn->GetFieldCount());

    for(int i = 0; i < static_cast<int>(m_aoFields.size()); i++ )
    {
        auto& f= m_aoFields[i];
        auto poFieldDefn = m_poRawFeatureDefn->GetFieldDefn(i);

        CPLXMLNode* psField = CPLCreateXMLNode(
            psRecord, CXT_Element, (osPrefix + "Field_" + GetSubType()).c_str());

        CPLCreateXMLElementAndValue(psField,
                        (osPrefix + "name").c_str(),
                        poFieldDefn->GetNameRef());

        CPLCreateXMLElementAndValue(psField,
                                    (osPrefix + "field_number").c_str(),
                                    CPLSPrintf("%d", i+1));

        auto psfield_location = CPLCreateXMLElementAndValue(psField,
                            (osPrefix + "field_location").c_str(),
                            CPLSPrintf("%d", f.m_nOffset + 1));
        CPLAddXMLAttributeAndValue(psfield_location, "unit", "byte");

        CPLCreateXMLElementAndValue(psField,
                                    (osPrefix + "data_type").c_str(),
                                    f.m_osDataType.c_str());

        auto psfield_length = CPLCreateXMLElementAndValue(psField,
                                    (osPrefix + "field_length").c_str(),
                                    CPLSPrintf("%d", f.m_nLength));
        CPLAddXMLAttributeAndValue(psfield_length, "unit", "byte");

        const auto eType(poFieldDefn->GetType());
        const int nWidth = poFieldDefn->GetWidth();
        if( (eType == OFTInteger || eType == OFTInteger64) &&
            nWidth > 0 )
        {
            CPLCreateXMLElementAndValue(psField,
                                    (osPrefix + "field_format").c_str(),
                                    CPLSPrintf("%%%dd", nWidth));
        }

        if( !f.m_osUnit.empty() )
        {
            CPLCreateXMLElementAndValue(psField,
                                        (osPrefix + "unit").c_str(),
                                        m_aoFields[i].m_osUnit.c_str());
        }

        if( !f.m_osDescription.empty() )
        {
            CPLCreateXMLElementAndValue(psField,
                                        (osPrefix + "description").c_str(),
                                        m_aoFields[i].m_osDescription.c_str());
        }
        if( !f.m_osSpecialConstantsXML.empty() )
        {
            auto psSpecialConstants = CPLParseXMLString(f.m_osSpecialConstantsXML);
            if( psSpecialConstants )
            {
                CPLAddXMLChild(psField, psSpecialConstants);
            }
        }
    }
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr PDS4FixedWidthTable::CreateField( OGRFieldDefn *poFieldIn, int )

{
    if( m_poDS->GetAccess() != GA_Update )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset opened in read-only mode");
        return OGRERR_FAILURE;
    }
    if( m_nFeatureCount > 0 )
    {
        return OGRERR_FAILURE;
    }

    Field f;
    if( !m_aoFields.empty() )
    {
        f.m_nOffset = m_aoFields.back().m_nOffset + m_aoFields.back().m_nLength;
    }

    if( !CreateFieldInternal(poFieldIn->GetType(), poFieldIn->GetSubType(),
                             poFieldIn->GetWidth(), f) )
    {
        return OGRERR_FAILURE;
    }

    MarkHeaderDirty();
    m_aoFields.push_back(f);
    m_poRawFeatureDefn->AddFieldDefn(poFieldIn);
    m_poFeatureDefn->AddFieldDefn(poFieldIn);
    m_nRecordSize += f.m_nLength;
    m_osBuffer.resize(m_nRecordSize);

    return OGRERR_NONE;
}

/************************************************************************/
/*                          InitializeNewLayer()                        */
/************************************************************************/

bool PDS4FixedWidthTable::InitializeNewLayer(
                                OGRSpatialReference* poSRS,
                                bool bForceGeographic,
                                OGRwkbGeometryType eGType,
                                const char* const* papszOptions)
{
    CPLAssert( m_fp == nullptr );
    m_fp = VSIFOpenL(m_osFilename, "wb+");
    if( !m_fp )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot create %s", m_osFilename.c_str());
        return false;
    }
    m_aosLCO.Assign(CSLDuplicate(papszOptions));

    m_nRecordSize = 0;

    const char* pszGeomColumns = CSLFetchNameValueDef(papszOptions, "GEOM_COLUMNS", "AUTO");
    if( EQUAL(pszGeomColumns, "WKT") )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "GEOM_COLUMNS=WKT only supported for delimited/CSV tables");
    }

    if( (EQUAL(pszGeomColumns, "AUTO") && wkbFlatten(eGType) == wkbPoint &&
                (bForceGeographic || (poSRS && poSRS->IsGeographic()))) ||
        (EQUAL(pszGeomColumns, "LONG_LAT") && eGType != wkbNone) )
    {
        {
            OGRFieldDefn oFieldDefn(
                CSLFetchNameValueDef(papszOptions, "LAT", "Latitude"), OFTReal);
            m_poRawFeatureDefn->AddFieldDefn(&oFieldDefn);
            m_iLatField = m_poRawFeatureDefn->GetFieldCount() - 1;
            Field f;
            f.m_nOffset = m_aoFields.empty() ? 0 :
                m_aoFields.back().m_nOffset + m_aoFields.back().m_nLength;
            CreateFieldInternal(OFTReal, OFSTNone, 0, f);
            m_aoFields.push_back(f);
            m_nRecordSize += f.m_nLength;
        }
        {
            OGRFieldDefn oFieldDefn(
                CSLFetchNameValueDef(papszOptions, "LONG", "Longitude"), OFTReal);
            m_poRawFeatureDefn->AddFieldDefn(&oFieldDefn);
            m_iLongField = m_poRawFeatureDefn->GetFieldCount() - 1;
            Field f;
            f.m_nOffset = m_aoFields.empty() ? 0 :
                m_aoFields.back().m_nOffset + m_aoFields.back().m_nLength;
            CreateFieldInternal(OFTReal, OFSTNone, 0, f);
            m_aoFields.push_back(f);
            m_nRecordSize += f.m_nLength;
        }
        if( eGType == wkbPoint25D )
        {
            OGRFieldDefn oFieldDefn(
                CSLFetchNameValueDef(papszOptions, "ALT", "Altitude"), OFTReal);
            m_poRawFeatureDefn->AddFieldDefn(&oFieldDefn);
            m_iAltField = m_poRawFeatureDefn->GetFieldCount() - 1;
            Field f;
            f.m_nOffset = m_aoFields.empty() ? 0 :
                m_aoFields.back().m_nOffset + m_aoFields.back().m_nLength;
            CreateFieldInternal(OFTReal, OFSTNone, 0, f);
            m_aoFields.push_back(f);
            m_nRecordSize += f.m_nLength;
        }

        m_poRawFeatureDefn->SetGeomType(eGType);

        m_poFeatureDefn->SetGeomType(eGType);
        if( poSRS )
        {
            auto poSRSClone = poSRS->Clone();
            poSRSClone->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRSClone);
            poSRSClone->Release();
        }
    }

    if( GetSubType() == "Character" )
    {
        ParseLineEndingOption(papszOptions);
    }
    m_nRecordSize += static_cast<int>(m_osLineEnding.size());
    m_osBuffer.resize(m_nRecordSize);

    m_nFeatureCount = 0;
    MarkHeaderDirty();
    return true;
}

/************************************************************************/
/* ==================================================================== */
/*                         PDS4TableCharacter                           */
/* ==================================================================== */
/************************************************************************/

PDS4TableCharacter::PDS4TableCharacter(PDS4Dataset* poDS,
                                       const char* pszName,
                                       const char* pszFilename) :
    PDS4FixedWidthTable(poDS, pszName, pszFilename)
{
}

/************************************************************************/
/*                        CreateFieldInternal()                         */
/************************************************************************/

bool PDS4TableCharacter::CreateFieldInternal(OGRFieldType eType,
                                             OGRFieldSubType eSubType,
                                             int nWidth, Field& f)
{
    if( nWidth > 0 )
    {
        f.m_nLength = nWidth;
    }
    else
    {
        if( eType == OFTString )
        {
            f.m_nLength = 64;
        }
        else if( eType == OFTInteger )
        {
            f.m_nLength = eSubType == OFSTBoolean ? 1 : 11;
        }
        else if( eType == OFTInteger64 )
        {
            f.m_nLength = 21;
        }
        else if( eType == OFTReal )
        {
            f.m_nLength = 16;
        }
        else if( eType == OFTDateTime )
        {
            // YYYY-MM-DDTHH:MM:SS.sssZ
            f.m_nLength = 4 + 1 + 2 + 1 + 2 + 1 + 2 + 1 + 2 + 1 + 2 + 1 + 3 + 1;
        }
        else if( eType == OFTDate )
        {
            // YYYY-MM-DD
            f.m_nLength = 4 + 1 + 2 + 1 + 2;
        }
        else if( eType == OFTTime )
        {
            // HH:MM:SS.sss
            f.m_nLength = 2 + 1 + 2 + 1 + 2 + 1 + 3;
        }
    }
    if( eType == OFTString )
    {
        f.m_osDataType = "UTF8_String";
    }
    else if( eType == OFTInteger )
    {
        f.m_osDataType = eSubType == OFSTBoolean ?
                            "ASCII_Boolean" : "ASCII_Integer";
    }
    else if( eType == OFTInteger64 )
    {
        f.m_osDataType = "ASCII_Integer";
    }
    else if( eType == OFTReal )
    {
        f.m_osDataType = "ASCII_Real";
    }
    else if( eType == OFTDateTime )
    {
        f.m_osDataType = "ASCII_Date_Time_YMD";
    }
    else if( eType == OFTDate )
    {
        f.m_osDataType = "ASCII_Date_YMD";
    }
    else if( eType == OFTTime )
    {
        f.m_osDataType = "ASCII_Time";
    }
    else
    {
        return false;
    }
    return true;
}

/************************************************************************/
/* ==================================================================== */
/*                         PDS4TableBinary                              */
/* ==================================================================== */
/************************************************************************/

PDS4TableBinary::PDS4TableBinary(PDS4Dataset* poDS,
                                       const char* pszName,
                                       const char* pszFilename) :
    PDS4FixedWidthTable(poDS, pszName, pszFilename)
{
}

/************************************************************************/
/*                        CreateFieldInternal()                         */
/************************************************************************/

bool PDS4TableBinary::CreateFieldInternal(OGRFieldType eType,
                                          OGRFieldSubType eSubType,
                                          int nWidth, Field& f)
{
    CPLString osEndianness(CPLGetConfigOption("PDS4_ENDIANNESS", "LSB"));
    CPLString osSignedness(CPLGetConfigOption("PDS4_SIGNEDNESS", "Signed"));

    if( eType == OFTString )
    {
        f.m_osDataType = "UTF8_String";
        f.m_nLength = nWidth > 0 ? nWidth : 64;
    }
    else if( eType == OFTInteger )
    {
        f.m_osDataType = nWidth > 0 && nWidth <= 2 ? osSignedness + "Byte":
                         eSubType == OFSTBoolean ? CPLString("ASCII_Boolean") :
                         eSubType == OFSTInt16   ? osSignedness + osEndianness + "2" :
                                                   osSignedness + osEndianness + "4";
        f.m_nLength = nWidth > 0 && nWidth <= 2 ? 1 :
                      eSubType == OFSTBoolean ? 1 :
                      eSubType == OFSTInt16   ? 2 :
                                                4;
    }
    else if( eType == OFTInteger64 )
    {
        f.m_osDataType = osSignedness + osEndianness + "8";
        f.m_nLength = 8;
    }
    else if( eType == OFTReal )
    {
        f.m_osDataType = eSubType == OFSTFloat32 ?
                            "IEEE754" + osEndianness + "Single" :
                            "IEEE754" + osEndianness + "Double";
        f.m_nLength = eSubType == OFSTFloat32 ? 4 : 8;
    }
    else if( eType == OFTDateTime )
    {
        f.m_osDataType = "ASCII_Date_Time_YMD";
        // YYYY-MM-DDTHH:MM:SS.sssZ
        f.m_nLength = 4 + 1 + 2 + 1 + 2 + 1 + 2 + 1 + 2 + 1 + 2 + 1 + 3 + 1;
    }
    else if( eType == OFTDate )
    {
        f.m_osDataType = "ASCII_Date_YMD";
        // YYYY-MM-DD
        f.m_nLength = 4 + 1 + 2 + 1 + 2;
    }
    else if( eType == OFTTime )
    {
        f.m_osDataType = "ASCII_Time";
        // HH:MM:SS.sss
        f.m_nLength = 2 + 1 + 2 + 1 + 2 + 1 + 3;
    }
    else
    {
        return false;
    }
    return true;
}


/************************************************************************/
/* ==================================================================== */
/*                         PDS4DelimitedTable                           */
/* ==================================================================== */
/************************************************************************/

PDS4DelimitedTable::PDS4DelimitedTable(PDS4Dataset* poDS,
                                       const char* pszName,
                                       const char* pszFilename) :
    PDS4TableBaseLayer(poDS, pszName, pszFilename)
{
}

/************************************************************************/
/*                          ~PDS4DelimitedTable()                       */
/************************************************************************/

PDS4DelimitedTable::~PDS4DelimitedTable()
{
    if( m_bDirtyHeader )
        GenerateVRT();
}

/************************************************************************/
/*                             GenerateVRT()                            */
/************************************************************************/

void PDS4DelimitedTable::GenerateVRT()
{
    CPLString osVRTFilename = CPLResetExtension(m_osFilename, "vrt");
    if( m_bCreation )
    {
        // In creation mode, generate the VRT, unless explicitly disabled by
        // CREATE_VRT=NO
        if( !m_aosLCO.FetchBool("CREATE_VRT", true) )
            return;
    }
    else
    {
        // In a update situation, only generates the VRT if ones already exists
        VSIStatBufL sStat;
        if( VSIStatL(osVRTFilename, &sStat) != 0 )
            return;
    }

    CPLXMLNode* psRoot = CPLCreateXMLNode(nullptr, CXT_Element,
                                          "OGRVRTDataSource");
    CPLXMLNode* psLayer = CPLCreateXMLNode(psRoot, CXT_Element,
                                          "OGRVRTLayer");
    CPLAddXMLAttributeAndValue(psLayer, "name", GetName());

    CPLXMLNode* psSrcDataSource = CPLCreateXMLElementAndValue(psLayer,
                                "SrcDataSource", CPLGetFilename(m_osFilename));
    CPLAddXMLAttributeAndValue(psSrcDataSource, "relativeToVRT", "1");

    CPLCreateXMLElementAndValue(psLayer, "SrcLayer", GetName());

    CPLXMLNode* psLastChild = CPLCreateXMLElementAndValue(psLayer, "GeometryType",
        OGRVRTGetSerializedGeometryType(GetGeomType()).c_str());

    if( GetSpatialRef() )
    {
        char* pszWKT = nullptr;
        GetSpatialRef()->exportToWkt(&pszWKT);
        if( pszWKT )
        {
            CPLCreateXMLElementAndValue(psLayer, "LayerSRS", pszWKT);
            CPLFree(pszWKT);
        }
    }

    while( psLastChild->psNext )
        psLastChild = psLastChild->psNext;
    const int nFieldCount = m_poRawFeatureDefn->GetFieldCount();
    for( int i = 0; i < nFieldCount; i++ )
    {
        if( i != m_iWKT && i != m_iLongField && i != m_iLatField &&
            i != m_iAltField )
        {
            OGRFieldDefn* poFieldDefn = m_poRawFeatureDefn->GetFieldDefn(i);
            CPLXMLNode* psField = CPLCreateXMLNode(nullptr, CXT_Element, "Field");
            psLastChild->psNext = psField;
            psLastChild = psField;
            CPLAddXMLAttributeAndValue(psField, "name", poFieldDefn->GetNameRef());
            CPLAddXMLAttributeAndValue(psField, "type",
                                    OGR_GetFieldTypeName(poFieldDefn->GetType()));
            if( poFieldDefn->GetSubType() != OFSTNone )
            {
                CPLAddXMLAttributeAndValue(psField, "subtype",
                                    OGR_GetFieldSubTypeName(poFieldDefn->GetSubType()));
            }
            if( poFieldDefn->GetWidth() > 0 && poFieldDefn->GetType() != OFTReal )
            {
                CPLAddXMLAttributeAndValue(psField, "width",
                                    CPLSPrintf("%d", poFieldDefn->GetWidth()));
            }
            CPLAddXMLAttributeAndValue(psField, "src", poFieldDefn->GetNameRef());
        }
    }

    if( m_iWKT >= 0 )
    {
        CPLXMLNode* psField = CPLCreateXMLNode(nullptr,
                                               CXT_Element, "GeometryField");
        psLastChild->psNext = psField;
        psLastChild = psField;
        CPLAddXMLAttributeAndValue(psField, "encoding", "WKT");
        CPLAddXMLAttributeAndValue(psField, "field",
                            m_poRawFeatureDefn->GetFieldDefn(m_iWKT)->GetNameRef());
    }
    else if( m_iLongField >= 0 && m_iLatField >= 0 )
    {
        CPLXMLNode* psField = CPLCreateXMLNode(nullptr,
                                               CXT_Element, "GeometryField");
        psLastChild->psNext = psField;
        psLastChild = psField;
        CPLAddXMLAttributeAndValue(psField, "encoding", "PointFromColumns");
        CPLAddXMLAttributeAndValue(psField, "x",
                m_poRawFeatureDefn->GetFieldDefn(m_iLongField)->GetNameRef());
        CPLAddXMLAttributeAndValue(psField, "y",
                m_poRawFeatureDefn->GetFieldDefn(m_iLatField)->GetNameRef());
        if( m_iAltField >= 0 )
        {
            CPLAddXMLAttributeAndValue(psField, "z",
                    m_poRawFeatureDefn->GetFieldDefn(m_iAltField)->GetNameRef());
        }
    }

    CPL_IGNORE_RET_VAL(psLastChild);

    CPLSerializeXMLTreeToFile(psRoot, osVRTFilename);
    CPLDestroyXMLNode(psRoot);
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void PDS4DelimitedTable::ResetReading()
{
    m_nFID = 1;
    VSIFSeekL(m_fp, m_nOffset, SEEK_SET);
}

/************************************************************************/
/*                         GetNextFeatureRaw()                          */
/************************************************************************/

OGRFeature* PDS4DelimitedTable::GetNextFeatureRaw()
{
    const char* pszLine = CPLReadLine2L(m_fp, 10 * 1024 * 1024, nullptr);
    if( pszLine == nullptr )
    {
        return nullptr;
    }

    char szDelimiter[2] = { m_chFieldDelimiter, 0 };
    char **papszFields =
        CSLTokenizeString2( pszLine, szDelimiter,
                            CSLT_HONOURSTRINGS | CSLT_ALLOWEMPTYTOKENS );
    if( CSLCount(papszFields) != m_poRawFeatureDefn->GetFieldCount() )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Did not get expected number of fields at line " CPL_FRMT_GIB,
                 m_nFID);
    }

    OGRFeature* poRawFeature = new OGRFeature(m_poRawFeatureDefn);
    poRawFeature->SetFID(m_nFID);
    m_nFID++;
    for( int i = 0; i < m_poRawFeatureDefn->GetFieldCount() &&
                    papszFields && papszFields[i]; i++ )
    {
        if( !m_aoFields[i].m_osMissingConstant.empty() &&
            m_aoFields[i].m_osMissingConstant == papszFields[i] )
        {
            // do nothing
        }
        else if( m_aoFields[i].m_osDataType == "ASCII_Boolean" )
        {
            poRawFeature->SetField(i,
                EQUAL(papszFields[i], "t") || EQUAL(papszFields[i], "1") ? 1 : 0);
        }
        else
        {
            poRawFeature->SetField(i, papszFields[i]);
        }
    }

    CSLDestroy(papszFields);

    OGRFeature* poFeature = AddGeometryFromFields(poRawFeature);
    delete poRawFeature;
    return poFeature;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature* PDS4DelimitedTable::GetNextFeature()
{
    while(true)
    {
        auto poFeature = GetNextFeatureRaw();
        if( poFeature == nullptr )
        {
            return nullptr;
        }

        if((m_poFilterGeom == nullptr
            || FilterGeometry( poFeature->GetGeometryRef() ) )
        && (m_poAttrQuery == nullptr
            || m_poAttrQuery->Evaluate( poFeature )) )
        {
            return poFeature;
        }
        delete poFeature;
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int PDS4DelimitedTable::TestCapability(const char* pszCap)
{
    if( EQUAL(pszCap, OLCRandomRead) ||
        EQUAL(pszCap, OLCStringsAsUTF8) )
    {
        return true;
    }
    if( EQUAL(pszCap, OLCFastFeatureCount) )
    {
        return m_poAttrQuery == nullptr && m_poFilterGeom == nullptr;
    }
    if( EQUAL(pszCap, OLCCreateField) )
    {
        return m_poDS->GetAccess() == GA_Update && m_nFeatureCount == 0;
    }
    if( EQUAL(pszCap, OLCSequentialWrite) )
    {
        return m_poDS->GetAccess() == GA_Update;
    }
    return false;
}

/************************************************************************/
/*                            QuoteIfNeeded()                           */
/************************************************************************/

CPLString PDS4DelimitedTable::QuoteIfNeeded(const char* pszVal)
{
    if( strchr(pszVal, m_chFieldDelimiter) == nullptr )
    {
        return pszVal;
    }
    return '"' + CPLString(pszVal) + '"';
}

/************************************************************************/
/*                           ICreateFeature()                           */
/************************************************************************/

OGRErr PDS4DelimitedTable::ICreateFeature( OGRFeature *poFeature )
{
    if( m_bAddWKTColumnPending )
    {
        OGRFieldDefn oFieldDefn(
            CSLFetchNameValueDef(m_aosLCO.List(), "WKT", "WKT"), OFTString);
        m_poRawFeatureDefn->AddFieldDefn(&oFieldDefn);
        m_iWKT = m_poRawFeatureDefn->GetFieldCount() - 1;
        Field f;
        f.m_osDataType = "ASCII_String";
        m_aoFields.push_back(f);
        m_bAddWKTColumnPending = false;
    }

    if( m_nFeatureCount == 0 )
    {
        for( int i = 0; i < m_poRawFeatureDefn->GetFieldCount(); i++ )
        {
            if( i > 0 )
            {
                VSIFPrintfL(m_fp, "%c", m_chFieldDelimiter);
            }
            VSIFPrintfL(m_fp, "%s", QuoteIfNeeded(
                m_poRawFeatureDefn->GetFieldDefn(i)->GetNameRef()).c_str());
        }
        VSIFPrintfL(m_fp, "%s", m_osLineEnding.c_str());
        m_nOffset = VSIFTellL(m_fp);
    }

    OGRFeature* poRawFeature = AddFieldsFromGeometry(poFeature);
    for( int i = 0; i < m_poRawFeatureDefn->GetFieldCount(); i++ )
    {
        if( i > 0 )
        {
            VSIFPrintfL(m_fp, "%c", m_chFieldDelimiter);
        }
        if( !poRawFeature->IsFieldSetAndNotNull(i) )
        {
            if( !m_aoFields[i].m_osMissingConstant.empty() )
            {
                VSIFPrintfL(m_fp, "%s",
                    QuoteIfNeeded(m_aoFields[i].m_osMissingConstant).c_str());
            }
            continue;
        }
        VSIFPrintfL(m_fp, "%s",
            QuoteIfNeeded(poRawFeature->GetFieldAsString(i)).c_str());
    }
    VSIFPrintfL(m_fp, "%s", m_osLineEnding.c_str());
    delete poRawFeature;

    m_nFeatureCount ++;
    poFeature->SetFID(m_nFeatureCount);

    return OGRERR_NONE;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr PDS4DelimitedTable::CreateField( OGRFieldDefn *poFieldIn, int )

{
    if( m_poDS->GetAccess() != GA_Update )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset opened in read-only mode");
        return OGRERR_FAILURE;
    }
    if( m_nFeatureCount > 0 )
    {
        return OGRERR_FAILURE;
    }

    const auto eType = poFieldIn->GetType();
    Field f;
    if( eType == OFTString )
    {
        f.m_osDataType = "UTF8_String";
    }
    else if( eType == OFTInteger )
    {
        f.m_osDataType = poFieldIn->GetSubType() == OFSTBoolean ?
                            "ASCII_Boolean" : "ASCII_Integer";
    }
    else if( eType == OFTInteger64 )
    {
        f.m_osDataType = "ASCII_Integer";
    }
    else if( eType == OFTReal )
    {
        f.m_osDataType = "ASCII_Real";
    }
    else if( eType == OFTDateTime )
    {
        f.m_osDataType = "ASCII_Date_Time_YMD";
    }
    else if( eType == OFTDate )
    {
        f.m_osDataType = "ASCII_Date_YMD";
    }
    else if( eType == OFTTime )
    {
        f.m_osDataType = "ASCII_Time";
    }
    else
    {
        return OGRERR_FAILURE;
    }

    MarkHeaderDirty();
    m_aoFields.push_back(f);
    m_poRawFeatureDefn->AddFieldDefn(poFieldIn);
    m_poFeatureDefn->AddFieldDefn(poFieldIn);

    return OGRERR_NONE;
}

/************************************************************************/
/*                            ReadTableDef()                            */
/************************************************************************/

bool PDS4DelimitedTable::ReadTableDef(const CPLXMLNode* psTable)
{
    CPLAssert( m_fp == nullptr );
    m_fp = VSIFOpenL(m_osFilename,
                     (m_poDS->GetAccess() == GA_ReadOnly ) ? "rb" : "r+b");
    if( !m_fp )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s",
                 m_osFilename.c_str());
        return false;
    }

    m_nOffset = static_cast<GUIntBig>(
        CPLAtoGIntBig(CPLGetXMLValue(psTable, "offset", "0")));

    m_nFeatureCount = CPLAtoGIntBig(
        CPLGetXMLValue(psTable, "records", "-1"));


    const char* pszRecordDelimiter = CPLGetXMLValue(psTable, "record_delimiter", "");
    if( EQUAL(pszRecordDelimiter, "Carriage-Return Line-Feed") )
        m_osLineEnding = "\r\n";
    else if( EQUAL(pszRecordDelimiter, "Line-Feed") )
        m_osLineEnding = "\n";
    else if( EQUAL(pszRecordDelimiter, "") )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Missing record_delimiter");
        return false;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid record_delimiter");
        return false;
    }

    const char* pszFieldDelimiter = CPLGetXMLValue(
        psTable, "field_delimiter", nullptr);
    if( pszFieldDelimiter == nullptr )
    {
        return false;
    }
    if( EQUAL(pszFieldDelimiter, "Comma") )
    {
        m_chFieldDelimiter = ',';
    }
    else if( EQUAL(pszFieldDelimiter, "Horizontal Tab") )
    {
        m_chFieldDelimiter = '\t';
    }
    else if( EQUAL(pszFieldDelimiter, "Semicolon") )
    {
        m_chFieldDelimiter = ';';
    }
    else if( EQUAL(pszFieldDelimiter, "Vertical Bar") )
    {
        m_chFieldDelimiter = '|';
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "field_delimiter value not supported");
        return false;
    }

    const CPLXMLNode* psRecord =
        CPLGetXMLNode(psTable, "Record_Delimited");
    if( !psRecord )
    {
        return false;
    }
    if( !ReadFields(psRecord, "") )
    {
        return false;
    }

    SetupGeomField();
    ResetReading();

    return true;
}

/************************************************************************/
/*                             ReadFields()                             */
/************************************************************************/

bool PDS4DelimitedTable::ReadFields(const CPLXMLNode* psParent,
                                    const CPLString& osSuffixFieldName)
{
    for( const CPLXMLNode* psIter = psParent->psChild;
                           psIter; psIter = psIter->psNext )
    {
        if( psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "Field_Delimited") == 0 )
        {
            const char* pszName = CPLGetXMLValue(psIter, "name", nullptr);
            if( !pszName )
            {
                return false;
            }
            const char* pszDataType = CPLGetXMLValue(psIter, "data_type", nullptr);
            if( !pszDataType )
            {
                return false;
            }
            int nMaximumFieldLength =
                atoi(CPLGetXMLValue(psIter, "maximum_field_length", "0"));

            Field f;
            f.m_osDataType = pszDataType;
            f.m_osUnit = CPLGetXMLValue(psIter, "unit", "");
            f.m_osDescription = CPLGetXMLValue(psIter, "description", "");

            CPLXMLNode* psSpecialConstants = const_cast<CPLXMLNode*>(
                CPLGetXMLNode(psIter, "Special_Constants"));
            if( psSpecialConstants )
            {
                auto psNext = psSpecialConstants->psNext;
                psSpecialConstants->psNext = nullptr;
                char* pszXML = CPLSerializeXMLTree(psSpecialConstants);
                psSpecialConstants->psNext = psNext;
                if( pszXML )
                {
                    f.m_osSpecialConstantsXML = pszXML;
                    CPLFree(pszXML);
                }
            }
            f.m_osMissingConstant = CPLGetXMLValue(psIter,
                                    "Special_Constants.missing_constant", "");

            m_aoFields.push_back(f);

            OGRFieldSubType eSubType = OFSTNone;
            bool error = false;
            auto eType = GetFieldTypeFromPDS4DataType(pszDataType, 0, eSubType, error);
            if( error )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Binary fields not allowed");
                return false;
            }
            if( STARTS_WITH(f.m_osDataType, "ASCII_") &&
                eType == OFTInteger && eSubType == OFSTNone &&
                (nMaximumFieldLength == 0 || nMaximumFieldLength >= 10) )
            {
                eType = OFTInteger64;
            }
            OGRFieldDefn oFieldDefn((pszName + osSuffixFieldName).c_str(), eType);
            oFieldDefn.SetSubType(eSubType);
            if( eType != OFTReal &&
                (STARTS_WITH(f.m_osDataType, "ASCII_") ||
                 STARTS_WITH(f.m_osDataType, "UTF_8")) )
            {
                oFieldDefn.SetWidth(nMaximumFieldLength);
            }
            m_poRawFeatureDefn->AddFieldDefn(&oFieldDefn);
        }
        else if( psIter->eType == CXT_Element &&
                 strcmp(psIter->pszValue, "Group_Field_Delimited") == 0 )
        {
            const char* pszRepetitions = CPLGetXMLValue(psIter, "repetitions", nullptr);
            if( !pszRepetitions )
            {
                return false;
            }
            int nRepetitions = std::min(1000, atoi(pszRepetitions));
            if( nRepetitions <= 0 )
            {
                return false;
            }
            for( int i = 0; i < nRepetitions; i++ )
            {
                if( !ReadFields(psIter,
                                osSuffixFieldName + "_" + CPLSPrintf("%d", i+1)) )
                {
                    return false;
                }
            }
        }
    }
    return true;
}

/************************************************************************/
/*                      RefreshFileAreaObservational()                  */
/************************************************************************/

void PDS4DelimitedTable::RefreshFileAreaObservational(CPLXMLNode* psFAO)
{
    CPLString osPrefix;
    if( STARTS_WITH(psFAO->pszValue, "pds:") )
        osPrefix = "pds:";

    CPLString osDescription;
    CPLXMLNode* psTable = RefreshFileAreaObservationalBeginningCommon(
        psFAO, osPrefix, "Table_Delimited", osDescription);

    CPLCreateXMLElementAndValue(psTable,
                                (osPrefix + "parsing_standard_id").c_str(),
                                "PDS DSV 1");

    CPLCreateXMLElementAndValue(psTable,
                                (osPrefix + "records").c_str(),
                                CPLSPrintf(CPL_FRMT_GIB, m_nFeatureCount));
    if( !osDescription.empty() )
        CPLCreateXMLElementAndValue(psTable,
                                    (osPrefix + "description").c_str(),
                                    osDescription);

    if( m_osLineEnding == "\r\n" )
    {
        CPLCreateXMLElementAndValue(psTable,
                                    (osPrefix + "record_delimiter").c_str(),
                                    "Carriage-Return Line-Feed");
    }
    else if( m_osLineEnding == "\n" )
    {
        CPLCreateXMLElementAndValue(psTable,
                                    (osPrefix + "record_delimiter").c_str(),
                                    "Line-Feed");
    }

    CPLCreateXMLElementAndValue(psTable,
                                (osPrefix + "field_delimiter").c_str(),
                                m_chFieldDelimiter == '\t' ? "Horizontal Tab" :
                                m_chFieldDelimiter == ';' ?  "Semicolon" :
                                m_chFieldDelimiter == '|' ?  "Vertical Bar" :
                                                             "Comma");

    // Write Record_Delimited
    CPLXMLNode* psRecord = CPLCreateXMLNode(
        psTable, CXT_Element, (osPrefix + "Record_Delimited").c_str());

    CPLCreateXMLElementAndValue(psRecord,
                        (osPrefix + "fields").c_str(),
                        CPLSPrintf("%d", static_cast<int>(m_aoFields.size())));

    CPLXMLNode* psLastChild = CPLCreateXMLElementAndValue(psRecord,
                                (osPrefix + "groups").c_str(), "0");


    CPLAssert(static_cast<int>(m_aoFields.size()) ==
                        m_poRawFeatureDefn->GetFieldCount());

    const auto osPrefixedFieldDelimited(osPrefix + "Field_Delimited");
    const auto osPrefixedName(osPrefix + "name");
    const auto osPrefixedFieldNumber(osPrefix + "field_number");
    const auto osPrefixedFieldData(osPrefix + "data_type");
    const auto osPrefixMaxFieldLength(osPrefix + "maximum_field_length");
    const auto osPrefixedUnit(osPrefix + "unit");
    const auto osPrefixedDescription(osPrefix + "description");
    CPLAssert(psLastChild->psNext == nullptr);
    for(int i = 0; i < static_cast<int>(m_aoFields.size()); i++ )
    {
        const auto& f = m_aoFields[i];

        CPLXMLNode* psField = CPLCreateXMLNode(
            nullptr, CXT_Element, osPrefixedFieldDelimited.c_str());
        psLastChild->psNext = psField;
        psLastChild = psField;

        CPLCreateXMLElementAndValue(psField,
                        osPrefixedName.c_str(),
                        m_poRawFeatureDefn->GetFieldDefn(i)->GetNameRef());

        CPLCreateXMLElementAndValue(psField,
                                    osPrefixedFieldNumber.c_str(),
                                    CPLSPrintf("%d", i+1));

        CPLCreateXMLElementAndValue(psField,
                                    osPrefixedFieldData.c_str(),
                                    f.m_osDataType.c_str());

        int nWidth = m_poRawFeatureDefn->GetFieldDefn(i)->GetWidth();
        if( nWidth > 0 )
        {
            auto psfield_length = CPLCreateXMLElementAndValue(psField,
                                        osPrefixMaxFieldLength.c_str(),
                                        CPLSPrintf("%d", nWidth));
            CPLAddXMLAttributeAndValue(psfield_length, "unit", "byte");
        }

        if( !f.m_osUnit.empty() )
        {
            CPLCreateXMLElementAndValue(psField,
                                        osPrefixedUnit.c_str(),
                                        m_aoFields[i].m_osUnit.c_str());
        }

        if( !f.m_osDescription.empty() )
        {
            CPLCreateXMLElementAndValue(psField,
                                        osPrefixedDescription.c_str(),
                                        m_aoFields[i].m_osDescription.c_str());
        }

        if( !f.m_osSpecialConstantsXML.empty() )
        {
            auto psSpecialConstants = CPLParseXMLString(f.m_osSpecialConstantsXML);
            if( psSpecialConstants )
            {
                CPLAddXMLChild(psField, psSpecialConstants);
            }
        }
    }
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char** PDS4DelimitedTable::GetFileList() const
{
    auto papszFileList = PDS4TableBaseLayer::GetFileList();
    CPLString osVRTFilename = CPLResetExtension(m_osFilename, "vrt");
    VSIStatBufL sStat;
    if( VSIStatL(osVRTFilename, &sStat) == 0 )
    {
        papszFileList = CSLAddString(papszFileList, osVRTFilename);
    }
    return papszFileList;
}

/************************************************************************/
/*                          InitializeNewLayer()                        */
/************************************************************************/

bool PDS4DelimitedTable::InitializeNewLayer(
                                OGRSpatialReference* poSRS,
                                bool bForceGeographic,
                                OGRwkbGeometryType eGType,
                                const char* const* papszOptions)
{
    CPLAssert( m_fp == nullptr );
    m_fp = VSIFOpenL(m_osFilename, "wb+");
    if( !m_fp )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot create %s", m_osFilename.c_str());
        return false;
    }
    m_aosLCO.Assign(CSLDuplicate(papszOptions));
    m_bCreation = true;

    // For testing purposes
    m_chFieldDelimiter = CPLGetConfigOption("OGR_PDS4_FIELD_DELIMITER", ",")[0];

    const char* pszGeomColumns = CSLFetchNameValueDef(papszOptions, "GEOM_COLUMNS", "AUTO");
    if( (EQUAL(pszGeomColumns, "AUTO") && wkbFlatten(eGType) == wkbPoint &&
                (bForceGeographic || (poSRS && poSRS->IsGeographic()))) ||
        (EQUAL(pszGeomColumns, "LONG_LAT") && eGType != wkbNone) )
    {
        {
            OGRFieldDefn oFieldDefn(
                CSLFetchNameValueDef(papszOptions, "LAT", "Latitude"), OFTReal);
            m_poRawFeatureDefn->AddFieldDefn(&oFieldDefn);
            m_iLatField = m_poRawFeatureDefn->GetFieldCount() - 1;
            Field f;
            f.m_osDataType = "ASCII_Real";
            m_aoFields.push_back(f);
        }
        {
            OGRFieldDefn oFieldDefn(
                CSLFetchNameValueDef(papszOptions, "LONG", "Longitude"), OFTReal);
            m_poRawFeatureDefn->AddFieldDefn(&oFieldDefn);
            m_iLongField = m_poRawFeatureDefn->GetFieldCount() - 1;
            Field f;
            f.m_osDataType = "ASCII_Real";
            m_aoFields.push_back(f);
        }
        if( eGType == wkbPoint25D )
        {
            OGRFieldDefn oFieldDefn(
                CSLFetchNameValueDef(papszOptions, "ALT", "Altitude"), OFTReal);
            m_poRawFeatureDefn->AddFieldDefn(&oFieldDefn);
            m_iAltField = m_poRawFeatureDefn->GetFieldCount() - 1;
            Field f;
            f.m_osDataType = "ASCII_Real";
            m_aoFields.push_back(f);
        }
    }
    else if( eGType != wkbNone &&
             (EQUAL(pszGeomColumns, "AUTO") || EQUAL(pszGeomColumns, "WKT")) )
    {
        m_bAddWKTColumnPending = true;
    }

    if( eGType != wkbNone )
    {
        m_poRawFeatureDefn->SetGeomType(eGType);

        m_poFeatureDefn->SetGeomType(eGType);
        if( poSRS )
        {
            auto poSRSClone = poSRS->Clone();
            poSRSClone->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRSClone);
            poSRSClone->Release();
        }
    }

    ParseLineEndingOption(papszOptions);

    m_nFeatureCount = 0;
    MarkHeaderDirty();
    return true;
}

/************************************************************************/
/* ==================================================================== */
/*                           PDS4EditableSynchronizer                   */
/* ==================================================================== */
/************************************************************************/

template<class T>
OGRErr PDS4EditableSynchronizer<T>::EditableSyncToDisk(
                    OGRLayer* poEditableLayer, OGRLayer** ppoDecoratedLayer)
{
    auto poOriLayer = cpl::down_cast<T*>(*ppoDecoratedLayer);

    CPLString osTmpFilename(poOriLayer->m_osFilename + ".tmp");
    auto poNewLayer = poOriLayer->NewLayer(
        poOriLayer->m_poDS, poOriLayer->GetName(), osTmpFilename);
    CPLStringList aosLCO(poOriLayer->m_aosLCO);
    if( poOriLayer->m_iLatField >= 0 )
    {
        aosLCO.SetNameValue("LAT",
            poOriLayer->m_poRawFeatureDefn->GetFieldDefn(
                poOriLayer->m_iLatField)->GetNameRef());
    }
    if( poOriLayer->m_iLongField >= 0 )
    {
        aosLCO.SetNameValue("LONG",
            poOriLayer->m_poRawFeatureDefn->GetFieldDefn(
                poOriLayer->m_iLongField)->GetNameRef());
    }
    if( poOriLayer->m_iAltField >= 0 )
    {
        aosLCO.SetNameValue("ALT",
            poOriLayer->m_poRawFeatureDefn->GetFieldDefn(
                poOriLayer->m_iAltField)->GetNameRef());
    }
    if( !poNewLayer->InitializeNewLayer(poOriLayer->GetSpatialRef(),
                                        poOriLayer->m_iLatField >= 0,
                                        poOriLayer->GetGeomType(),
                                        aosLCO.List()) )
    {
        delete poNewLayer;
        VSIUnlink(osTmpFilename);
        return OGRERR_FAILURE;
    }

    const auto copyField = [](typename T::Field& oDst, const typename T::Field& oSrc)
    {
        oDst.m_osDescription = oSrc.m_osDescription;
        oDst.m_osUnit = oSrc.m_osUnit;
        oDst.m_osSpecialConstantsXML = oSrc.m_osSpecialConstantsXML;
    };

    if( poNewLayer->m_iLatField >= 0 )
    {
        copyField(poNewLayer->m_aoFields[poNewLayer->m_iLatField],
                  poOriLayer->m_aoFields[poOriLayer->m_iLatField]);
    }
    if( poNewLayer->m_iLongField >= 0 )
    {
        copyField(poNewLayer->m_aoFields[poNewLayer->m_iLongField],
                  poOriLayer->m_aoFields[poOriLayer->m_iLongField]);
    }
    if( poNewLayer->m_iAltField >= 0 )
    {
        copyField(poNewLayer->m_aoFields[poNewLayer->m_iAltField],
                  poOriLayer->m_aoFields[poOriLayer->m_iAltField]);
    }

    OGRFeatureDefn *poEditableFDefn = poEditableLayer->GetLayerDefn();
    for( int i = 0; i < poEditableFDefn->GetFieldCount(); i++ )
    {
        auto poFieldDefn = poEditableFDefn->GetFieldDefn(i);
        poNewLayer->CreateField(poFieldDefn, false);
        int idx = poOriLayer->m_poRawFeatureDefn->GetFieldIndex(
            poFieldDefn->GetNameRef());
        if( idx >= 0 )
        {
            copyField(poNewLayer->m_aoFields.back(),
                      poOriLayer->m_aoFields[idx]);
            OGRFieldDefn* poOriFieldDefn =
                poOriLayer->m_poRawFeatureDefn->GetFieldDefn(idx);
            if( poFieldDefn->GetType() == poOriFieldDefn->GetType() )
            {
                poNewLayer->m_aoFields.back().m_osDataType =
                    poOriLayer->m_aoFields[idx].m_osDataType;
            }
        }
    }

    poEditableLayer->ResetReading();

    // Disable all filters.
    const char* pszQueryStringConst = poEditableLayer->GetAttrQueryString();
    char* pszQueryStringBak = pszQueryStringConst ? CPLStrdup(pszQueryStringConst) : nullptr;
    poEditableLayer->SetAttributeFilter(nullptr);

    const int iFilterGeomIndexBak = poEditableLayer->GetGeomFieldFilter();
    OGRGeometry* poFilterGeomBak = poEditableLayer->GetSpatialFilter();
    if( poFilterGeomBak )
        poFilterGeomBak = poFilterGeomBak->clone();
    poEditableLayer->SetSpatialFilter(nullptr);

    auto aoMapSrcToTargetIdx = poNewLayer->GetLayerDefn()->
        ComputeMapForSetFrom(poEditableLayer->GetLayerDefn(), true);
    aoMapSrcToTargetIdx.push_back(-1); // add dummy entry to be sure that .data() is valid

    OGRErr eErr = OGRERR_NONE;
    for( auto&& poFeature: poEditableLayer )
    {
        OGRFeature *poNewFeature =
            new OGRFeature(poNewLayer->GetLayerDefn());
        poNewFeature->SetFrom(poFeature.get(), aoMapSrcToTargetIdx.data(), true);
        eErr = poNewLayer->CreateFeature(poNewFeature);
        delete poNewFeature;
        if( eErr != OGRERR_NONE )
        {
            break;
        }
    }

    // Restore filters.
    poEditableLayer->SetAttributeFilter(pszQueryStringBak);
    CPLFree(pszQueryStringBak);
    poEditableLayer->SetSpatialFilter(iFilterGeomIndexBak, poFilterGeomBak);
    delete poFilterGeomBak;

    if( eErr != OGRERR_NONE ||
        !poNewLayer->RenameFileTo(poOriLayer->GetFileName()) )
    {
        delete poNewLayer;
        VSIUnlink(osTmpFilename);
        return OGRERR_FAILURE;
    }

    delete poOriLayer;
    *ppoDecoratedLayer = poNewLayer;

    return OGRERR_NONE;
}

/************************************************************************/
/* ==================================================================== */
/*                         PDS4EditableLayer                            */
/* ==================================================================== */
/************************************************************************/

PDS4EditableLayer::PDS4EditableLayer(PDS4FixedWidthTable* poBaseLayer):
    OGREditableLayer(poBaseLayer, true,
                     new PDS4EditableSynchronizer<PDS4FixedWidthTable>(), true)
{
}

PDS4EditableLayer::PDS4EditableLayer(PDS4DelimitedTable* poBaseLayer):
    OGREditableLayer(poBaseLayer, true,
                     new PDS4EditableSynchronizer<PDS4DelimitedTable>(), true)
{
}

/************************************************************************/
/*                           GetBaseLayer()                             */
/************************************************************************/

PDS4TableBaseLayer* PDS4EditableLayer::GetBaseLayer() const
{
    return cpl::down_cast<PDS4TableBaseLayer*>(OGREditableLayer::GetBaseLayer());
}

/************************************************************************/
/*                            SetSpatialRef()                           */
/************************************************************************/

void PDS4EditableLayer::SetSpatialRef(OGRSpatialReference* poSRS)
{
    if( GetGeomType() != wkbNone )
    {
        GetLayerDefn()->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
        GetBaseLayer()->GetLayerDefn()->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
    }
}
