/*****************************************************************************
* Copyright 2016 Pitney Bowes Inc.
*
* Licensed under the MIT License (the “License”); you may not use this file
* except in the compliance with the License.
* You may obtain a copy of the License at https://opensource.org/licenses/MIT

* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an “AS IS” WITHOUT
* WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*****************************************************************************/


#include "cpl_port.h"
#include "OGREFAL.h"
#include "ogrgeopackageutility.h"

#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif
#include <algorithm>
#include <string>
#include <vector>

#include "cpl_conv.h"
#include "cpl_csv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_geometry.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"
#include "ogrsf_frmts.h"
#include "from_mitab.h"

CPL_CVSID("$Id: OGREFALLayer.cpp 37371 2017-02-13 11:41:59Z rouault $");

#include <ctime>
#include <chrono>

extern void OGREFALReleaseSession(EFALHANDLE hSession);
extern EFALHANDLE OGREFALGetSession(GUIntBig);
/************************************************************************/
/*                            OGREFALLayer()                             */
/*                                                                      */
/*      Note that the OGREFALLayer assumes ownership of the passed       */
/*      file pointer.                                                   */
/************************************************************************/
OGREFALLayer::OGREFALLayer(EFALHANDLE argSession, EFALHANDLE argTable, EfalOpenMode eEfalOpenMode) :
    hSession(argSession),
    hTable(argTable),
    hSequentialCursor(0),
    poFeatureDefn(nullptr),
    pszTableCSys(nullptr),
    bHasFieldNames(false),
    efalOpenMode(eEfalOpenMode),
    bNew(FALSE),
    bNeedEndAccess(FALSE),
    bCreateNativeX(FALSE),
    nBlockSize(16384),
    charset(Ellis::MICHARSET::CHARSET_WLATIN1),
    bHasBounds(FALSE),
    xmin(0),
    ymin(0),
    xmax(0),
    ymax(0),
    bInWriteMode(FALSE),
    pszFilename(nullptr),
    nLastFID(-1),
    bHasMap(false),
    pSpatialReference(nullptr)
{
    /*-------------------------------------------------------------
    * Do initial setup of feature definition.
    *------------------------------------------------------------*/
    const wchar_t * pwszFeatureClassName = efallib->GetTableName(hSession, hTable);
    char * pszFeatureClassName = CPLRecodeFromWChar(pwszFeatureClassName, CPL_ENC_UCS2, CPL_ENC_UTF8);

    const wchar_t * pwszTablePath = efallib->GetTablePath(hSession, hTable);
    pszFilename = CPLRecodeFromWChar(pwszTablePath, CPL_ENC_UCS2, CPL_ENC_UTF8);

    poFeatureDefn = new OGRFeatureDefn(pszFeatureClassName);
    SetDescription(poFeatureDefn->GetName());
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbNone);
    CPLFree(pszFeatureClassName);

    charset = efallib->GetTableCharset(hSession, hTable);

    /*
     * Create the field definitions
    */
    OGRFieldDefn *poFieldDefn = nullptr;
    for (MI_UINT32 i = 0; i < efallib->GetColumnCount(hSession, hTable); i++)
    {
        poFieldDefn = nullptr;
        const wchar_t * pwszAlias = efallib->GetColumnName(hSession, hTable, i);
        char * pszAlias = CPLRecodeFromWChar(pwszAlias, CPL_ENC_UCS2, CPL_ENC_UTF8);

        Ellis::ALLTYPE_TYPE atType = efallib->GetColumnType(hSession, hTable, i);
        switch (atType)
        {
        case Ellis::ALLTYPE_TYPE::OT_CHAR:
            poFieldDefn = new OGRFieldDefn(pszAlias, OFTString);
            poFieldDefn->SetWidth(efallib->GetColumnWidth(hSession, hTable, i));
            break;
        case Ellis::ALLTYPE_TYPE::OT_DECIMAL:
            poFieldDefn = new OGRFieldDefn(pszAlias, OFTReal);
            poFieldDefn->SetWidth(efallib->GetColumnWidth(hSession, hTable, i));
            poFieldDefn->SetPrecision(efallib->GetColumnDecimals(hSession, hTable, i));
            break;
        case Ellis::ALLTYPE_TYPE::OT_FLOAT:
            poFieldDefn = new OGRFieldDefn(pszAlias, OFTReal);
            break;
        case Ellis::ALLTYPE_TYPE::OT_SMALLINT:
            poFieldDefn = new OGRFieldDefn(pszAlias, OFTInteger);
            // if (efallib->GetColumnWidth(hSession, hTable, i) > 0)
                // poFieldDefn->SetWidth(efallib->GetColumnWidth(hSession, hTable, i));
            break;
        case Ellis::ALLTYPE_TYPE::OT_INTEGER:
            poFieldDefn = new OGRFieldDefn(pszAlias, OFTInteger);
            // if (efallib->GetColumnWidth(hSession, hTable, i) > 0)
                // poFieldDefn->SetWidth(efallib->GetColumnWidth(hSession, hTable, i));
            break;
        case Ellis::ALLTYPE_TYPE::OT_INTEGER64:
            poFieldDefn = new OGRFieldDefn(pszAlias, OFTInteger64);
            break;
        case Ellis::ALLTYPE_TYPE::OT_LOGICAL:
            poFieldDefn = new OGRFieldDefn(pszAlias, OFTString);
            poFieldDefn->SetWidth(1);
            break;
        case Ellis::ALLTYPE_TYPE::OT_DATE:
            poFieldDefn = new OGRFieldDefn(pszAlias, OFTDate);
            break;
        case Ellis::ALLTYPE_TYPE::OT_TIME:
            poFieldDefn = new OGRFieldDefn(pszAlias, OFTTime);
            break;
        case Ellis::ALLTYPE_TYPE::OT_DATETIME:
            poFieldDefn = new OGRFieldDefn(pszAlias, OFTDateTime);
            break;
        case Ellis::ALLTYPE_TYPE::OT_TIMESPAN:
            poFieldDefn = new OGRFieldDefn(pszAlias, OFTReal);
            break;
        case Ellis::ALLTYPE_TYPE::OT_STYLE:
            /*
             * OGRFeature does not treat style as a column but it does allow style values to
             * be supplied as a string using the SetStyleString/GetStyleString methods.
             * Note that OGR defines it's own style string syntax so we'll have to do some
             * translating between the MapBasic styles and an OGR style. See
             * http://www.gdal.org/ogr_feature_style.html for details.
             */
            break;
        case Ellis::ALLTYPE_TYPE::OT_OBJECT:
        {
            /*
             * NOTE: OGRFeatureDefn ctor automatically adds 1 geometry field definition with
             * type of unknown and no SRS. So we don't create a new one, we just update that one.
             */
            int numPoints = efallib->GetPointObjectCount(hSession, hTable, i);
            int numRegions = efallib->GetAreaObjectCount(hSession, hTable, i);
            int numLines = efallib->GetLineObjectCount(hSession, hTable, i);
            int numTexts = efallib->GetMiscObjectCount(hSession, hTable, i);
            if (numPoints > 0 && numLines == 0 && numRegions == 0 && numTexts == 0)
                poFeatureDefn->SetGeomType(wkbPoint);
            else {
                /* we leave it unknown indicating a mixture */
                poFeatureDefn->SetGeomType(wkbUnknown);
            }

            poFeatureDefn->GetGeomFieldDefn(0)->SetName("OBJ");
            const wchar_t* szwCoordSys = efallib->GetColumnCSys(hSession, hTable, i);
            pszTableCSys = CPLRecodeFromWChar(szwCoordSys, CPL_ENC_UCS2, CPL_ENC_UTF8);
            pSpatialReference = EFALCSys2OGRSpatialRef(szwCoordSys);
            double dMinx, dMiny, dMaxx, dMaxy;
            if (ExtractBoundsFromCSysString(pszTableCSys, dMinx, dMiny, dMaxx, dMaxy)) {
                SetBounds(dMinx, dMiny, dMaxx, dMaxy);
            }
            poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(pSpatialReference);
            bHasMap = true;
            break;
        }
        default:
            break;
        }

        CPLFree(pszAlias);
        if (poFieldDefn)
        {
            poFeatureDefn->AddFieldDefn(poFieldDefn);
            delete poFieldDefn;
            poFieldDefn = nullptr;
        }
    }
    switch (efalOpenMode)
    {
    case EfalOpenMode::EFAL_LOCK_READ:
        this->bNeedEndAccess = efallib->BeginReadAccess(hSession, hTable);
        break;
    case EfalOpenMode::EFAL_LOCK_WRITE:
        this->bNeedEndAccess = efallib->BeginWriteAccess(hSession, hTable);
        break;
    case EfalOpenMode::EFAL_READ_ONLY:
    case EfalOpenMode::EFAL_READ_WRITE:
        break;
    }
}

OGREFALLayer::OGREFALLayer(EFALHANDLE argSession, const char *pszLayerNameIn,
    const char *pszFilenameIn, bool bNativeX, int BlockSize, Ellis::MICHARSET eCharset) :
    hSession(argSession),
    hTable(0),
    hSequentialCursor(0),
    poFeatureDefn(nullptr),
    pszTableCSys(nullptr),
    bHasFieldNames(false),
    efalOpenMode(EfalOpenMode::EFAL_LOCK_WRITE),
    bNew(true),
    bNeedEndAccess(FALSE),
    bCreateNativeX(bNativeX),
    nBlockSize(BlockSize),
    charset(eCharset),
    bHasBounds(FALSE),
    xmin(0),
    ymin(0),
    xmax(0),
    ymax(0),
    bInWriteMode(true),
    pszFilename(CPLStrdup(pszFilenameIn)),
    nLastFID(-1),
    bHasMap(false),
    pSpatialReference(nullptr)
{
    poFeatureDefn = new OGRFeatureDefn(pszLayerNameIn);
    SetDescription(poFeatureDefn->GetName());
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbNone);
}

/************************************************************************/
/*                             SetSpatialRef()                          */
/************************************************************************/
void OGREFALLayer::SetSpatialRef(OGRSpatialReference *poSpatialRef)
{
    if (poFeatureDefn->GetGeomFieldCount() == 0)
    {
        poFeatureDefn->SetGeomType(wkbUnknown);
    }
    bHasMap = true;
    if (poSpatialRef)
    {
        poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSpatialRef);
        pSpatialReference = poSpatialRef;
        pSpatialReference->Reference();
    }
    else
    {
        pSpatialReference = EFALCSys2OGRSpatialRef(L"epsg:4326");
    }
}

/************************************************************************/
/*                            ~OGREFALLayer()                            */
/************************************************************************/

OGREFALLayer::~OGREFALLayer()
{
    // we need to create the table as its not yet created.
    CreateNewTable();

    if ((hSession > 0) && (hTable > 0))
    {
        if (pszFilename != nullptr) {
            wchar_t* pswzFileName = CPLRecodeToWChar(pszFilename, CPL_ENC_UTF8, CPL_ENC_UCS2);
            if (efallib->GetTableHandleFromTablePath(hSession, pswzFileName) != (EFALHANDLE)nullptr)
            {
                CloseSequentialCursor();
                if (bNeedEndAccess)
                    efallib->EndAccess(hSession, hTable);
                efallib->CloseTable(hSession, hTable);
            }
            CPLFree(pswzFileName);
        }
    }

    poFeatureDefn->Release();
    if (pSpatialReference) {
        pSpatialReference->Release();
    }

    if (pszFilename) {
        CPLFree(pszFilename);
    }
    if (pszTableCSys) {
        CPLFree(pszTableCSys);
    }
    hSequentialCursor = 0;
    hTable = 0;
    OGREFALReleaseSession(hSession);
    hSession = 0;
}

/************************************************************************/
/*                           SetBounds()                                */
/************************************************************************/
void OGREFALLayer::SetBounds(double arg_xmin, double arg_ymin, double arg_xmax, double arg_ymax)
{
    this->xmin = arg_xmin;
    this->ymin = arg_ymin;
    this->xmax = arg_xmax;
    this->ymax = arg_ymax;
    bHasBounds = true;
}
/************************************************************************/
/*                           GetExtent()                                */
/************************************************************************/
OGRErr OGREFALLayer::GetExtent(OGREnvelope *psExtent, int /*bForce*/)
{
    // we need to create the table as its not yet created.
    OGRErr status = CreateNewTable();
    if (status != OGRERR_NONE) return status;

    for (MI_UINT32 i = 0; i < efallib->GetColumnCount(hSession, hTable); i++)
    {
        Ellis::ALLTYPE_TYPE atType = efallib->GetColumnType(hSession, hTable, i);
        if (atType == Ellis::ALLTYPE_TYPE::OT_OBJECT)
        {
            Ellis::DRECT bounds = efallib->GetEntireBounds(hSession, hTable, i);
            OGREnvelope env;
            env.MinX = bounds.x1;
            env.MinY = bounds.y1;
            env.MaxX = bounds.x2;
            env.MaxY = bounds.y2;
            *psExtent = env;
            return OGRERR_NONE;
        }
    }
    return OGRERR_NON_EXISTING_FEATURE;
}

/************************************************************************/
/*                       CloseSequentialCursor()                        */
/************************************************************************/
void OGREFALLayer::CloseSequentialCursor()
{
    if (hSequentialCursor != 0)
    {
        efallib->DisposeCursor(hSession, hSequentialCursor);
        hSequentialCursor = 0;
    }
}

/************************************************************************/
/*                            BuildQuery()                              */
/************************************************************************/

void OGREFALLayer::BuildQuery(wchar_t * szQuery, size_t sz, bool count) const
{
    CPLString query = "SELECT ";
    if (count)
        query += "COUNT(*)";
    else
        query += "*";
    query += " FROM \"";
    const wchar_t * szwTableName = efallib->GetTableName(hSession, hTable);
    char * szTableName = CPLRecodeFromWChar(szwTableName, CPL_ENC_UCS2, CPL_ENC_UTF8);
    query += szTableName;
    CPLFree(szTableName);
    query += "\"";

    CPLString where = "";
    if (m_poFilterGeom != nullptr)
    {
        OGREnvelope envelope;
        m_poFilterGeom->getEnvelope(&envelope);
        where += " WHERE MI_EnvelopesIntersect(OBJ, MI_Box(";
        char szEnvelope[400];
        szEnvelope[0] = '\0';
        CPLsnprintf(szEnvelope, sizeof(szEnvelope),
            "%.18g, %.18g, %.18g, %.18g",
            envelope.MinX, envelope.MinY,
            envelope.MaxX, envelope.MaxY);
        where += szEnvelope;
        where += ",'";
        where += pszTableCSys;
        where += "'))";
    }
    if (m_pszAttrQueryString != nullptr)
    {
        if (m_poFilterGeom != nullptr)
            where += " AND ";
        else
            where += " WHERE ";
        where += m_pszAttrQueryString;
    }
    CPLString combined = query;
    combined += where;
    wchar_t * pszQuery = CPLRecodeToWChar(combined, CPL_ENC_UTF8, CPL_ENC_UCS2);
    if (wcslen(pszQuery) + 1 > sz)
    {
        CPLFree(pszQuery);
        pszQuery = CPLRecodeToWChar(query, CPL_ENC_UTF8, CPL_ENC_UCS2);
    }
    wcscpy_s(szQuery, sz, pszQuery);
    CPLFree(pszQuery);
}

/************************************************************************/
/*                        GetFeatureCount()                             */
/************************************************************************/

GIntBig OGREFALLayer::GetFeatureCount(int /*bForce*/)
{
    // we need to create the table as its not yet created.
    OGRErr status = CreateNewTable();
    if (status != OGRERR_NONE) return 0;

    wchar_t szQuery[256] = { 0 };
    BuildQuery(szQuery, sizeof(szQuery) / sizeof(wchar_t), true);
    EFALHANDLE hCountCursor = efallib->Select(hSession, szQuery);
    GIntBig count = 0;
    if (hCountCursor != 0)
    {
        if (efallib->FetchNext(hSession, hCountCursor))
        {
            double d = efallib->GetCursorValueDouble(hSession, hCountCursor, 0);
            count = (GIntBig)d;
        }
        efallib->DisposeCursor(hSession, hCountCursor);
    }
    return count;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGREFALLayer::ResetReading()
{
    // we need to create the table as its not yet created.
    OGRErr status = CreateNewTable();
    if (status != OGRERR_NONE) { hSequentialCursor = 0; return; }

    CloseSequentialCursor();
    wchar_t szQuery[256];
    BuildQuery(szQuery, sizeof(szQuery) / sizeof(wchar_t), false);
    hSequentialCursor = efallib->Select(hSession, szQuery);
}

/************************************************************************/
/*                      EFALCSys2OGRSpatialRef()                        */
/************************************************************************/
OGRSpatialReference* OGREFALLayer::EFALCSys2OGRSpatialRef(const wchar_t* szwCoordSys)
{
    char * szCoordSys = CPLRecodeFromWChar(szwCoordSys, CPL_ENC_UCS2, CPL_ENC_UTF8);
    OGRSpatialReference* poSpatialRef = nullptr;
    if (strnicmp(szCoordSys, "mapinfo:coordsys ", 17) == 0)
    {
        szwCoordSys = efallib->CoordSys2MBString(hSession, szwCoordSys);
        CPLFree(szCoordSys);
        szCoordSys = CPLRecodeFromWChar(szwCoordSys, CPL_ENC_UCS2, CPL_ENC_UTF8);
        poSpatialRef = new OGRSpatialReference();
        CPLPushErrorHandler(CPLQuietErrorHandler);
        /*OGRErr eErr = */
        poSpatialRef->importFromMICoordSys(szCoordSys);
        CPLPopErrorHandler();
        CPLErrorReset();
    }
    else if (strnicmp(szCoordSys, "epsg:", 5) == 0)
    {
        char * szCode = szCoordSys + 5;
        int nEPSGCode = atoi(szCode);
        poSpatialRef = new OGRSpatialReference();
        CPLPushErrorHandler(CPLQuietErrorHandler);
        poSpatialRef->importFromEPSG(nEPSGCode);
        CPLPopErrorHandler();
        CPLErrorReset();
    }
    CPLFree(szCoordSys);
    return poSpatialRef;
}

bool OGREFALLayer::ExtractBoundsFromCSysString(const char * pszCoordSys, double &dXMin, double &dYMin, double &dXMax, double &dYMax)
{
    if (pszCoordSys == nullptr)
        return false;

    char **papszFields = CSLTokenizeStringComplex(pszCoordSys, " ,()", TRUE, FALSE);

    int iBounds = CSLFindString(papszFields, "Bounds");

    if (iBounds >= 0 && iBounds + 4 < CSLCount(papszFields))
    {
        dXMin = CPLAtof(papszFields[++iBounds]);
        dYMin = CPLAtof(papszFields[++iBounds]);
        dXMax = CPLAtof(papszFields[++iBounds]);
        dYMax = CPLAtof(papszFields[++iBounds]);
        CSLDestroy(papszFields);
        return true;
    }

    CSLDestroy(papszFields);
    return false;
}

/************************************************************************/
/*                      OGRSpatialRef2EFALCSys()                        */
/************************************************************************/
const wchar_t* OGREFALLayer::OGRSpatialRef2EFALCSys(const OGRSpatialReference* poSpatialRef)
{
    if (poSpatialRef)
    {
        char * szResult = nullptr;
        OGRErr err = poSpatialRef->exportToMICoordSys(&szResult);
        if (err == OGRERR_NONE)
        {
            wchar_t * pszwMapInfoCoordSys = CPLRecodeToWChar(szResult, CPL_ENC_UTF8, CPL_ENC_UCS2);
            CPLFree(szResult);
            const wchar_t * pwConvertedCoordSys = efallib->MB2CoordSysString(hSession, pszwMapInfoCoordSys);
            CPLFree(pszwMapInfoCoordSys);
            return pwConvertedCoordSys;
        }
    }
    return nullptr;
}

/************************************************************************/
/*                      EFALGeometry2OGRGeometry()                      */
/************************************************************************/
OGRGeometry* OGREFALLayer::EFALGeometry2OGRGeometry(GByte* bytes, size_t sz)
{
    OGRGeometry *poGeom = GPkgGeometryToOGR(bytes, sz, GetSpatialRef());
    return poGeom;
}
/************************************************************************/
/*                      OGRGeometry2EFALGeometry()                      */
/************************************************************************/
void OGREFALLayer::OGRGeometry2EFALGeometry(OGRGeometry* ogrGeometry, GByte** pbytes, size_t* psz)
{
    // EFAL does not use the iSrsId so we pass a Zero. Coordinate values are assumed to be in the csys of the table already.
    (*pbytes) = GPkgGeometryFromOGR(ogrGeometry, /*iSrsId*/0, psz);
}


/************************************************************************/
/*                    CursorIndex2FeatureIndex()                        */
/************************************************************************/
int OGREFALLayer::CursorIndex2FeatureIndex(EFALHANDLE hCursor, OGRFeatureDefn* pFeatureDefn, MI_UINT32 idxCursor) const
{
    const wchar_t* szwColumnName = efallib->GetCursorColumnName(hSession, hCursor, idxCursor);
    char * szColumnName = CPLRecodeFromWChar(szwColumnName, CPL_ENC_UCS2, CPL_ENC_UTF8);
    for (int i = 0, n = pFeatureDefn->GetFieldCount(); i < n; i++)
    {
        if (strcmp(pFeatureDefn->GetFieldDefn(i)->GetNameRef(), szColumnName) == 0)
        {
            CPLFree(szColumnName);
            return i;
        }
    }
    CPLFree(szColumnName);
    return -1;
}
/************************************************************************/
/*                          Cursor2Feature()                            */
/************************************************************************/
/* Create a Feature from the current cursor location.                   */

OGRFeature* OGREFALLayer::Cursor2Feature(EFALHANDLE hCursor, OGRFeatureDefn* pFeatureDefn)
{
    OGRFeature * pFeature = new OGRFeature(pFeatureDefn);
    const wchar_t * pszwMIKey = efallib->GetCursorCurrentKey(hSession, hCursor);
    if (pszwMIKey)
    {
        char * pszMIKey = CPLRecodeFromWChar(pszwMIKey, CPL_ENC_UCS2, CPL_ENC_UTF8);
        GIntBig iFID = CPLAtoGIntBig(pszMIKey);
        CPLFree(pszMIKey);
        pFeature->SetFID(iFID);
    }

    for (MI_UINT32 i = 0, n = efallib->GetCursorColumnCount(hSession, hCursor); i < n; i++)
    {
        if (efallib->GetCursorColumnType(hSession, hCursor, i) == Ellis::ALLTYPE_TYPE::OT_STYLE)
        {
            const wchar_t* szwMBStyle = efallib->GetCursorValueStyle(hSession, hCursor, i);
            if (szwMBStyle != nullptr) {
                CPLString szOGRStyle = MapBasicStyle2OGRStyle(szwMBStyle);
                pFeature->SetStyleString(szOGRStyle);
            }
        }
        else if (efallib->GetCursorColumnType(hSession, hCursor, i) == Ellis::ALLTYPE_TYPE::OT_OBJECT)
        {
            unsigned long sz = efallib->PrepareCursorValueGeometry(hSession, hCursor, i);
            char* bytes = new char[sz];
            efallib->GetData(hSession, bytes, sz);
            pFeature->SetGeometryDirectly(EFALGeometry2OGRGeometry((GByte*)bytes, sz));
            delete[] bytes;
        }
        else
        {
            int idxFeature = CursorIndex2FeatureIndex(hCursor, pFeatureDefn, i);
            if ((idxFeature < 0) &&
                (efallib->GetCursorColumnType(hSession, hCursor, i) != Ellis::ALLTYPE_TYPE::OT_STYLE) &&
                (efallib->GetCursorColumnType(hSession, hCursor, i) != Ellis::ALLTYPE_TYPE::OT_OBJECT)) continue;

            if (efallib->GetCursorIsNull(hSession, hCursor, i))
            {
                pFeature->SetFieldNull((int)idxFeature);
            }
            else
            {
                switch (efallib->GetCursorColumnType(hSession, hCursor, i))
                {
                case Ellis::ALLTYPE_TYPE::OT_CHAR:
                {
                    char * szValue = nullptr;
                    szValue = CPLRecodeFromWChar(efallib->GetCursorValueString(hSession, hCursor, i), CPL_ENC_UCS2, CPL_ENC_UTF8);
                    pFeature->SetField(idxFeature, szValue);
                    CPLFree(szValue);
                    szValue = nullptr;
                }
                break;
                case Ellis::ALLTYPE_TYPE::OT_DECIMAL:
                case Ellis::ALLTYPE_TYPE::OT_FLOAT:
                    pFeature->SetField(idxFeature, efallib->GetCursorValueDouble(hSession, hCursor, i));
                    break;
                case Ellis::ALLTYPE_TYPE::OT_SMALLINT:
                    pFeature->SetField(idxFeature, efallib->GetCursorValueInt16(hSession, hCursor, i));
                    break;
                case Ellis::ALLTYPE_TYPE::OT_INTEGER:
                    pFeature->SetField(idxFeature, (int)efallib->GetCursorValueInt32(hSession, hCursor, i));
                    break;
                case Ellis::ALLTYPE_TYPE::OT_INTEGER64:
                    pFeature->SetField(idxFeature, (GIntBig)efallib->GetCursorValueInt64(hSession, hCursor, i));
                    break;
                case Ellis::ALLTYPE_TYPE::OT_LOGICAL:
                    pFeature->SetField(idxFeature, (efallib->GetCursorValueBoolean(hSession, hCursor, i) ? "T" : "F"));
                    break;
                case Ellis::ALLTYPE_TYPE::OT_DATE:
                {
                    EFALDATE efalDate = efallib->GetCursorValueDate(hSession, hCursor, i);
                    pFeature->SetField(idxFeature, efalDate.year, efalDate.month, efalDate.day, 0, 0, 0, 0);
                }
                break;
                case Ellis::ALLTYPE_TYPE::OT_TIME:
                {
                    EFALTIME efalTime = efallib->GetCursorValueTime(hSession, hCursor, i);
                    pFeature->SetField(idxFeature, 0, 0, 0, efalTime.hour, efalTime.minute, efalTime.second + efalTime.millisecond / 1000.0f, 0);
                }
                break;
                case Ellis::ALLTYPE_TYPE::OT_DATETIME:
                {
                    EFALDATETIME efalDateTime = efallib->GetCursorValueDateTime(hSession, hCursor, i);
                    // TODO: tz??? The last argument is 0 which is tz...
                    pFeature->SetField(idxFeature, efalDateTime.year, efalDateTime.month, efalDateTime.day, efalDateTime.hour, efalDateTime.minute, efalDateTime.second + efalDateTime.millisecond / 1000.0f, 0);
                }
                break;
                case Ellis::ALLTYPE_TYPE::OT_TIMESPAN:
                    pFeature->SetField(idxFeature, efallib->GetCursorValueDouble(hSession, hCursor, i));
                    break;
                default:
                    break;
                }
            }
        }
    }
    return pFeature;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature* OGREFALLayer::GetFeature(GIntBig nFID)
{
    // we need to create the table as its not yet created.
    OGRErr status = CreateNewTable();
    if (status != OGRERR_NONE) return nullptr;

    wchar_t szQuery[256] = { 0 };
    swprintf(szQuery, sizeof(szQuery) / sizeof(wchar_t), L"SELECT * FROM \"%ls\" WHERE MI_KEY = '" CPL_FRMT_GIB "'", efallib->GetTableName(hSession, hTable), nFID);
    EFALHANDLE hCursor = efallib->Select(hSession, szQuery);
    if (hCursor)
    {
        OGRFeature* pFeature = nullptr;
        if (efallib->FetchNext(hSession, hCursor))
        {
            /* From the docs: The returned feature becomes the responsibility of the caller to delete with OGRFeature::DestroyFeature(). */
            pFeature = Cursor2Feature(hCursor, poFeatureDefn);
        }
        efallib->DisposeCursor(hSession, hCursor);
        return pFeature;
    }
    return nullptr;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGREFALLayer::GetNextFeature()
{
    if (hSequentialCursor == 0)
        ResetReading();

    if (hSequentialCursor != 0 && efallib->FetchNext(hSession, hSequentialCursor))
    {
        /* From the docs: The returned feature becomes the responsibility of the caller to delete with OGRFeature::DestroyFeature(). */
        OGRFeature* pFeature = Cursor2Feature(hSequentialCursor, poFeatureDefn);
        return pFeature;
    }
    return nullptr;
}
/************************************************************************/
/*                           ISetFeature()                              */
/************************************************************************/
/* Rewrite an existing feature.

This method is implemented by drivers and not called directly.User code should use SetFeature() instead.

This method will write a feature to the layer, based on the feature id within the OGRFeature.
*/
OGRErr OGREFALLayer::ISetFeature(OGRFeature *poFeature)
{
    // we need to create the table as its not yet created.
    OGRErr err = CreateNewTable();
    if (err != OGRERR_NONE) return err;

    CloseSequentialCursor();

    CPLString command = "UPDATE \"";
    const wchar_t * pwszFeatureClassName = efallib->GetTableName(hSession, hTable);
    char * pszFeatureClassName = CPLRecodeFromWChar(pwszFeatureClassName, CPL_ENC_UCS2, CPL_ENC_UTF8);
    command += pszFeatureClassName;
    CPLFree(pszFeatureClassName);
    command += "\" SET ";
    bool first = true;
    for (int i = 0, n = poFeature->GetFieldCount(); i < n; i++)
    {
        if (!poFeature->IsFieldSet(i))
            continue;

        const char * fieldName = poFeature->GetFieldDefnRef(i)->GetNameRef();
        if (poFeature->IsFieldNull(i))
        {
            if (!first)
            {
                command += ",";
            }
            first = false;
            command += fieldName;
            command += "= nullptr";
        }
        else
        {
            wchar_t warname[32];
            swprintf(warname, sizeof(warname) / sizeof(wchar_t), L"@%d", i);
            char * varname = CPLRecodeFromWChar(warname, CPL_ENC_UCS2, CPL_ENC_UTF8);
            switch (poFeature->GetFieldDefnRef(i)->GetType())
            {
            case OGRFieldType::OFTString:
            {
                efallib->CreateVariable(hSession, warname);
                const char * value = poFeature->GetFieldAsString(i);
                wchar_t * walue = CPLRecodeToWChar(value, CPL_ENC_UTF8, CPL_ENC_UCS2);
                efallib->SetVariableValueString(hSession, warname, walue);
                CPLFree(walue);
                if (!first)
                {
                    command += ",";
                }
                first = false;
                command += fieldName;
                command += "=";
                command += varname;
            }
            break;
            case OGRFieldType::OFTInteger:
            {
                efallib->CreateVariable(hSession, warname);
                int value = poFeature->GetFieldAsInteger(i);
                efallib->SetVariableValueInt32(hSession, warname, value);
                if (!first)
                {
                    command += ",";
                }
                first = false;
                command += fieldName;
                command += "=";
                command += varname;
            }
            break;
            case OGRFieldType::OFTInteger64:
            {
                efallib->CreateVariable(hSession, warname);
                GIntBig value = poFeature->GetFieldAsInteger64(i);
                efallib->SetVariableValueInt64(hSession, warname, value);
                if (!first)
                {
                    command += ",";
                }
                first = false;
                command += fieldName;
                command += "=";
                command += varname;
            }
            break;
            case OGRFieldType::OFTReal:
            {
                efallib->CreateVariable(hSession, warname);
                double value = poFeature->GetFieldAsDouble(i);
                efallib->SetVariableValueDouble(hSession, warname, value);
                if (!first)
                {
                    command += ",";
                }
                first = false;
                command += fieldName;
                command += "=";
                command += varname;
            }
            break;
            case OGRFieldType::OFTDate:
            {
                efallib->CreateVariable(hSession, warname);
                int y = 0, m = 0, d = 0, h = 0, min = 0, tz = 0;
                float s = 0;
                poFeature->GetFieldAsDateTime(i, &y, &m, &d, &h, &min, &s, &tz);
                EFALDATE value;
                value.year = y; value.month = m; value.day = d;
                efallib->SetVariableValueDate(hSession, warname, value);
                if (!first)
                {
                    command += ",";
                }
                first = false;
                command += fieldName;
                command += "=";
                command += varname;
            }
            break;
            case OGRFieldType::OFTDateTime:
            {
                efallib->CreateVariable(hSession, warname);
                int y = 0, m = 0, d = 0, h = 0, min = 0, tz = 0;
                float s = 0;
                poFeature->GetFieldAsDateTime(i, &y, &m, &d, &h, &min, &s, &tz);
                EFALDATETIME value;
                value.year = y; value.month = m; value.day = d;
                value.hour = h; value.minute = min; value.second = (int)floor(s);
                value.millisecond = (int)floor((s - value.second) * 1000.0);
                // TODO: tz???
                efallib->SetVariableValueDateTime(hSession, warname, value);
                if (!first)
                {
                    command += ",";
                }
                first = false;
                command += fieldName;
                command += "=";
                command += varname;
            }
            break;
            case OGRFieldType::OFTTime:
            {
                efallib->CreateVariable(hSession, warname);
                int y = 0, m = 0, d = 0, h = 0, min = 0, tz = 0;
                float s = 0;
                poFeature->GetFieldAsDateTime(i, &y, &m, &d, &h, &min, &s, &tz);
                EFALTIME value;
                value.hour = h; value.minute = min; value.second = (int)floor(s);
                value.millisecond = (int)floor((s - value.second) * 1000.0);
                efallib->SetVariableValueTime(hSession, warname, value);
                if (!first)
                {
                    command += ",";
                }
                first = false;
                command += fieldName;
                command += "=";
                command += varname;
            }
            break;

            case OGRFieldType::OFTBinary: // unsupported by TAB formats (supported by others not exposed by EFAL)
            case OGRFieldType::OFTInteger64List: // unsupported
            case OGRFieldType::OFTIntegerList: // unsupported
            case OGRFieldType::OFTRealList: // unsupported
            case OGRFieldType::OFTStringList: // unsupported
            case OGRFieldType::OFTWideString: // deprecated
            case OGRFieldType::OFTWideStringList: // deprecated
                err = OGRERR_FAILURE;
                break;
            }
            CPLFree(varname);
        }
    }
    // API Seems unclear of the meaning when the Geometry and/or Style is nullptr on the feature
    // whether this means the record in the table should be updated to nullptr or if they should 
    // not be included in the update at all. It appears from other implementations that they
    // are not included in the update at all so this driver will follow that pattern.
    OGRGeometry * ogrGeometry = poFeature->GetGeometryRef(); // may be nullptr
    if (ogrGeometry != nullptr)
    {
        GByte* bytes = nullptr;
        size_t sz = 0;
        OGRGeometry2EFALGeometry(ogrGeometry, &bytes, &sz);
        if (bytes == nullptr)
        {
            err = OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
        }
        else
        {
            const wchar_t * warname = L"@geom";
            const char * varname = "@geom";
            efallib->CreateVariable(hSession, warname);
            efallib->SetVariableValueBinary(hSession, warname, (MI_UINT32)sz, (const char *)bytes);
            if (!first)
            {
                command += ",";
            }
            first = false;
            command += "OBJ";
            command += "=";
            command += varname;
        }
    }
    const char * ogrStyleString = poFeature->GetStyleString(); // may be nullptr
    if (ogrStyleString != nullptr)
    {
        const wchar_t * warname = L"@style";

        char* mbStyleString = OGRStyle2MapBasicStyle(ogrStyleString);
        if (mbStyleString == nullptr)
        {
            // Just because we failed to parse an OGR style into a MapBasic style, we won't fail the operation, just allow it to default to the Ellis default values.
            efallib->CreateVariable(hSession, warname);
            efallib->SetVariableIsNull(hSession, warname);
        }
        else
        {
            const char * varname = "@style";
            wchar_t * szwMBStyleString = CPLRecodeToWChar(mbStyleString, CPL_ENC_UTF8, CPL_ENC_UCS2);
            efallib->CreateVariable(hSession, warname);
            efallib->SetVariableValueStyle(hSession, warname, szwMBStyleString);
            CPLFree(szwMBStyleString);
            if (!first)
            {
                command += ",";
            }
            command += "MI_Style";
            command += "=";
            command += varname;
        }
    }
    command += " WHERE MI_Key = '";
    char szFID[64];
    szFID[0] = '\0';
    //ltoa((long)poFeature->GetFID(), szFID, 10);
    CPLsnprintf(szFID, sizeof(szFID), "%ld", (long)poFeature->GetFID());
    command += szFID;
    command += "'";
    if (err == OGRERR_NONE)
    {
        wchar_t * szwCommand = CPLRecodeToWChar(command, CPL_ENC_UTF8, CPL_ENC_UCS2);
        long nrecs = efallib->Update(hSession, szwCommand);
        CPLFree(szwCommand);
        err = (nrecs == 1) ? OGRERR_NONE : OGRERR_NON_EXISTING_FEATURE;
    }
    // Now drop all of the variables
    for (MI_UINT32 n = efallib->GetVariableCount(hSession); n > 0; n--)
    {
        efallib->DropVariable(hSession, efallib->GetVariableName(hSession, n - 1));
    }
    return err;
}
/************************************************************************/
/*                           CreateNewTable()                           */
/************************************************************************/
/* Create the new TAB file. The FIrstFeature, if supplied, may be used  */
/* to refine some data types. For example, if a property definition was */
/* specified as String(1), OGR may mean this is to be a LOGICAL field.  */
/* Since we don't know, we'll create it as a CHAR(1) unless the first   */
/* feature has a 'Y' or 'N' value.                                      */
/************************************************************************/
OGRErr OGREFALLayer::CreateNewTable()
{
    OGRErr status = OGRERR_NONE;
    // if table is new but not yet created.
    if (bNew && hTable == 0)
    {
        wchar_t * tableName = CPLRecodeToWChar(poFeatureDefn->GetName(), CPL_ENC_UTF8, CPL_ENC_UCS2);
        wchar_t * tablePath = CPLRecodeToWChar(pszFilename, CPL_ENC_UTF8, CPL_ENC_UCS2);
        EFALHANDLE hMetadata = 0;
        if (bCreateNativeX)
        {
            hMetadata = efallib->CreateNativeXTableMetadata(hSession, tableName, tablePath, charset);

            wchar_t szBlockSize[32];
            swprintf(szBlockSize, sizeof(szBlockSize) / sizeof(wchar_t), L"%d", nBlockSize);
            efallib->SetMetadata(hSession, hMetadata, L"\blockSizeMapFile", szBlockSize);
        }
        else
        {
            hMetadata = efallib->CreateNativeTableMetadata(hSession, tableName, tablePath, charset);
        }
        int n = poFeatureDefn->GetFieldCount();

        if (n > 0) {
            for (int i = 0; (i < n) && (status == OGRERR_NONE); i++)
            {
                OGRFieldDefn* pFieldDefn = poFeatureDefn->GetFieldDefn(i);
                wchar_t * columnName = CPLRecodeToWChar(pFieldDefn->GetNameRef(), CPL_ENC_UTF8, CPL_ENC_UCS2); // TODO: Is UCS2 the right thing throughout here or is UTF16 better???
                OGRFieldType ogrType = pFieldDefn->GetType();
                MI_UINT32 columnWidth = 0;
                MI_UINT32 columnDecimals = 0;
                Ellis::ALLTYPE_TYPE columnType = Ellis::ALLTYPE_TYPE::OT_NONE;

                switch (ogrType)
                {
                case OGRFieldType::OFTString:
                    columnType = Ellis::ALLTYPE_TYPE::OT_CHAR;
                    columnWidth = pFieldDefn->GetWidth();
                    break;
                case OGRFieldType::OFTInteger:
                    columnType = Ellis::ALLTYPE_TYPE::OT_INTEGER;
                    break;
                case OGRFieldType::OFTInteger64:
                    if (bCreateNativeX)
                    {
                        columnType = Ellis::ALLTYPE_TYPE::OT_INTEGER64;
                    }
                    else
                    {
                        columnType = Ellis::ALLTYPE_TYPE::OT_INTEGER;
                    }
                    break;
                case OGRFieldType::OFTReal:
                    if (pFieldDefn->GetWidth() > 0)
                    {
                        columnWidth = (MI_UINT32)pFieldDefn->GetWidth();
                        columnDecimals = (MI_UINT32)pFieldDefn->GetPrecision();
                        columnType = Ellis::ALLTYPE_TYPE::OT_DECIMAL;
                    }
                    else
                    {
                        columnType = Ellis::ALLTYPE_TYPE::OT_FLOAT;
                    }
                    break;
                case OGRFieldType::OFTDate:
                    columnType = Ellis::ALLTYPE_TYPE::OT_DATE;
                    break;
                case OGRFieldType::OFTDateTime:
                    columnType = Ellis::ALLTYPE_TYPE::OT_DATETIME;
                    break;
                case OGRFieldType::OFTTime:
                    columnType = Ellis::ALLTYPE_TYPE::OT_TIME;
                    break;

                case OGRFieldType::OFTBinary: // unsupported
                case OGRFieldType::OFTWideString: // deprecated
                case OGRFieldType::OFTInteger64List: // unsupported
                case OGRFieldType::OFTIntegerList: // unsupported
                case OGRFieldType::OFTRealList: // unsupported
                case OGRFieldType::OFTStringList: // unsupported
                case OGRFieldType::OFTWideStringList: // deprecated
                    status = OGRERR_FAILURE;
                    CPLError(CE_Failure, CPLE_NotSupported,
                        "Unsupported column type.");
                    break;
                }
                if (status == OGRERR_NONE)
                {
                    efallib->AddColumn(hSession, hMetadata, columnName, columnType, false, columnWidth, columnDecimals, nullptr);
                }
                CPLFree(columnName);
            }
        }
        else
        {
            // Add a single FID column.
            efallib->AddColumn(hSession, hMetadata, L"FID", Ellis::ALLTYPE_TYPE::OT_INTEGER, true, 0, 0, nullptr);
        }
        // Add Geometry and Style columns
        if (poFeatureDefn->GetGeomFieldCount() > 0)
        {
            const wchar_t * efalCSys = OGRSpatialRef2EFALCSys(GetSpatialRef());
            efallib->AddColumn(hSession, hMetadata, L"OBJ", Ellis::ALLTYPE_TYPE::OT_OBJECT, false, 0, 0, efalCSys);
            efallib->AddColumn(hSession, hMetadata, L"MI_STYLE", Ellis::ALLTYPE_TYPE::OT_STYLE, false, 0, 0, nullptr);
            bHasMap = true;
        }

        hTable = efallib->CreateTable(hSession, hMetadata);
        efallib->DestroyTableMetadata(hSession, hMetadata);
        if (hTable == 0)
        {
            // TODO: efallib->HaveErrors and error handling throughout is missing/poor
            CPLError(CE_Failure, CPLE_NotSupported,
                "Creation of new TAB file failed.");
            status = OGRERR_FAILURE;
        }
        if (status == OGRERR_NONE)
        {
            bNew = false;
            bNeedEndAccess = efallib->BeginWriteAccess(hSession, hTable);
        }
        CPLFree(tableName);
        CPLFree(tablePath);
    }
    return status;
}
/************************************************************************/
/*                           ICreateFeature()                           */
/************************************************************************/
/* Create and write a new feature within a layer.

This method is implemented by drivers and not called directly. User code should use CreateFeature() instead.

The passed feature is written to the layer as a new feature, rather than overwriting an existing one.
If the feature has a feature id other than OGRNullFID, then the native implementation may use that as
the feature id of the new feature, but not necessarily. Upon successful return the passed feature
will have been updated with the new feature id.
 */
OGRErr OGREFALLayer::ICreateFeature(OGRFeature *poFeature)
{
    OGRErr err = CreateNewTable();
    if (err != OGRERR_NONE) return err;

    CloseSequentialCursor();

    CPLString command = "INSERT INTO \"";
    const wchar_t * pwszFeatureClassName = efallib->GetTableName(hSession, hTable);
    char * pszFeatureClassName = CPLRecodeFromWChar(pwszFeatureClassName, CPL_ENC_UCS2, CPL_ENC_UTF8);
    command += pszFeatureClassName;
    command += "\" (";
    CPLString values = "";
    bool first = true;
    for (int i = 0, n = poFeature->GetFieldCount(); i < n; i++)
    {
        if (!poFeature->IsFieldSet(i))
            continue;
        const char * fieldName = poFeature->GetFieldDefnRef(i)->GetNameRef();
        if (poFeature->IsFieldNull(i))
        {
            if (!first) { command += ","; values += ","; } first = false;
            command += fieldName;
            values += "NULL";
        }
        else
        {
            wchar_t warname[32];
            swprintf(warname, sizeof(warname) / sizeof(wchar_t), L"@%d", i);
            char * varname = CPLRecodeFromWChar(warname, CPL_ENC_UCS2, CPL_ENC_UTF8);
            switch (poFeature->GetFieldDefnRef(i)->GetType())
            {
            case OGRFieldType::OFTString:
            {
                efallib->CreateVariable(hSession, warname);
                const char * value = poFeature->GetFieldAsString(i);
                wchar_t * walue = CPLRecodeToWChar(value, CPL_ENC_UTF8, CPL_ENC_UCS2);
                efallib->SetVariableValueString(hSession, warname, walue);
                CPLFree(walue);
                if (!first) { command += ","; values += ","; } first = false;
                command += fieldName;
                values += varname;
            }
            break;
            case OGRFieldType::OFTInteger:
            {
                efallib->CreateVariable(hSession, warname);
                int value = poFeature->GetFieldAsInteger(i);
                efallib->SetVariableValueInt32(hSession, warname, value);
                if (!first) { command += ","; values += ","; } first = false;
                command += fieldName;
                values += varname;
            }
            break;
            case OGRFieldType::OFTInteger64:
            {
                efallib->CreateVariable(hSession, warname);
                GIntBig value = poFeature->GetFieldAsInteger64(i);
                efallib->SetVariableValueInt64(hSession, warname, value);
                if (!first) { command += ","; values += ","; } first = false;
                command += fieldName;
                values += varname;
            }
            break;
            case OGRFieldType::OFTReal:
            {
                efallib->CreateVariable(hSession, warname);
                double value = poFeature->GetFieldAsDouble(i);
                efallib->SetVariableValueDouble(hSession, warname, value);
                if (!first) { command += ","; values += ","; } first = false;
                command += fieldName;
                values += varname;
            }
            break;
            case OGRFieldType::OFTDate:
            {
                efallib->CreateVariable(hSession, warname);
                int y = 0, m = 0, d = 0, h = 0, min = 0, tz = 0;
                float s = 0;
                poFeature->GetFieldAsDateTime(i, &y, &m, &d, &h, &min, &s, &tz);
                EFALDATE value;
                value.year = y; value.month = m; value.day = d;
                efallib->SetVariableValueDate(hSession, warname, value);
                if (!first) { command += ","; values += ","; } first = false;
                command += fieldName;
                values += varname;
            }
            break;
            case OGRFieldType::OFTDateTime:
            {
                efallib->CreateVariable(hSession, warname);
                int y = 0, m = 0, d = 0, h = 0, min = 0, tz = 0;
                float s = 0;
                poFeature->GetFieldAsDateTime(i, &y, &m, &d, &h, &min, &s, &tz);
                EFALDATETIME value;
                value.year = y; value.month = m; value.day = d;
                value.hour = h; value.minute = min; value.second = (int)floor(s);
                value.millisecond = (int)floor((s - value.second) * 1000.0);
                // TODO: tz???
                efallib->SetVariableValueDateTime(hSession, warname, value);
                if (!first) { command += ","; values += ","; } first = false;
                command += fieldName;
                values += varname;
            }
            break;
            case OGRFieldType::OFTTime:
            {
                efallib->CreateVariable(hSession, warname);
                int y = 0, m = 0, d = 0, h = 0, min = 0, tz = 0;
                float s = 0;
                poFeature->GetFieldAsDateTime(i, &y, &m, &d, &h, &min, &s, &tz);
                EFALTIME value;
                value.hour = h; value.minute = min; value.second = (int)floor(s);
                value.millisecond = (int)floor((s - value.second) * 1000.0);
                efallib->SetVariableValueTime(hSession, warname, value);
                if (!first) { command += ","; values += ","; } first = false;
                command += fieldName;
                values += varname;
            }
            break;

            case OGRFieldType::OFTBinary: // unsupported by TAB formats (supported by others not exposed by EFAL)
            case OGRFieldType::OFTInteger64List: // unsupported
            case OGRFieldType::OFTIntegerList: // unsupported
            case OGRFieldType::OFTRealList: // unsupported
            case OGRFieldType::OFTStringList: // unsupported
            case OGRFieldType::OFTWideString: // deprecated
            case OGRFieldType::OFTWideStringList: // deprecated
                err = OGRERR_FAILURE;
                break;
            }
            CPLFree(varname);
        }
    }
    // API Seems unclear of the meaning when the Geometry and/or Style is nullptr on the feature
    // whether this means the record in the table should be updated to nullptr or if they should 
    // not be included in the update at all. It appears from other implementations that they
    // are not included in the update at all so this driver will follow that pattern.
    OGRGeometry * ogrGeometry = poFeature->GetGeometryRef(); // may be nullptr
    if (ogrGeometry != nullptr)
    {
        GByte* bytes = nullptr;
        size_t sz = 0;
        OGRGeometry2EFALGeometry(ogrGeometry, &bytes, &sz);
        if (bytes == nullptr)
        {
            err = OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
        }
        else
        {
            const wchar_t * warname = L"@geom";
            const char * varname = "@geom";
            const wchar_t * pszwCSys = OGRSpatialRef2EFALCSys(GetSpatialRef());
            if (pszwCSys == nullptr) {
                for (MI_UINT32 i = 0; i < efallib->GetColumnCount(hSession, hTable); i++)
                {
                    Ellis::ALLTYPE_TYPE atType = efallib->GetColumnType(hSession, hTable, i);
                    if (atType == Ellis::ALLTYPE_TYPE::OT_OBJECT) {
                        pszwCSys = efallib->GetColumnCSys(hSession, hTable, i);
                    }
                }
            }
            efallib->CreateVariable(hSession, warname);
            efallib->SetVariableValueGeometry(hSession, warname, (MI_UINT32)sz, (const char *)bytes, pszwCSys);
            if (!first) { command += ","; values += ","; } first = false;
            command += "OBJ";
            values += varname;
        }
    }
    const char * ogrStyleString = poFeature->GetStyleString(); // may be nullptr
    if (ogrStyleString != nullptr)
    {
        const wchar_t * warname = L"@style";
        char* mbStyleString = OGRStyle2MapBasicStyle(ogrStyleString);
        if (mbStyleString == nullptr || strlen(mbStyleString) == 0)
        {
            // Just because we failed to parse an OGR style into a MapBasic style, we won't fail the operation, just allow it to default to the Ellis default values.
            efallib->CreateVariable(hSession, warname);
            efallib->SetVariableIsNull(hSession, warname);
        }
        else
        {
            const char * varname = "@style";
            wchar_t * szwMBStyleString = CPLRecodeToWChar(mbStyleString, CPL_ENC_UTF8, CPL_ENC_UCS2);
            efallib->CreateVariable(hSession, warname);
            efallib->SetVariableValueStyle(hSession, warname, szwMBStyleString);
            CPLFree(szwMBStyleString);
            if (!first) { command += ","; values += ","; }
            command += "MI_Style";
            values += varname;
        }
    }
    command += ") VALUES (";
    command += values;
    command += ")";

    //printf("%s\n", command.c_str());

    if (err == OGRERR_NONE)
    {
        wchar_t * szwCommand = CPLRecodeToWChar(command, CPL_ENC_UTF8, CPL_ENC_UCS2);
        long nrecs = efallib->Insert(hSession, szwCommand);
        CPLFree(szwCommand);

        if (nrecs == 1)
        {
            // Now get the ROWID of the new feature - for Native TAB this will be the MAX rowid (mi_key)
            nLastFID = (GIntBig)efallib->GetColumnCount(hSession, hTable);
            if (nLastFID <= 0)
            {
                // This table type does not support it, let's query for it explicitly
                CPLString select = "SELECT MAX(StringToNumber(MI_KEY,'999999999')) FROM \"";
                select += pszFeatureClassName;
                select += "\"";
                wchar_t * szwSelect = CPLRecodeToWChar(select, CPL_ENC_UTF8, CPL_ENC_UCS2);
                EFALHANDLE hMaxCursor = efallib->Select(hSession, szwSelect);
                CPLFree(szwSelect);
                efallib->FetchNext(hSession, hMaxCursor);
                nLastFID = (GIntBig)efallib->GetCursorValueDouble(hSession, hMaxCursor, 0);
                efallib->DisposeCursor(hSession, hMaxCursor);
            }
            poFeature->SetFID(nLastFID);
        }
        else
        {
            err = OGRERR_NON_EXISTING_FEATURE;
        }
    }
    CPLFree(pszFeatureClassName);
    // Now drop all of the variables
    for (MI_UINT32 n = efallib->GetVariableCount(hSession); n > 0; n--)
    {
        efallib->DropVariable(hSession, efallib->GetVariableName(hSession, n - 1));
    }
    return err;
}

/************************************************************************/
/*                           DeleteFeature()                            */
/************************************************************************/
/* Delete feature from layer.

The feature with the indicated feature id is deleted from the layer if supported by the driver.
Most drivers do not support feature deletion, and will return OGRERR_UNSUPPORTED_OPERATION.
The TestCapability() layer method may be called with OLCDeleteFeature to check if the driver
supports feature deletion.
*/
OGRErr OGREFALLayer::DeleteFeature(GIntBig nFID)
{
    // we need to create the table as its not yet created.
    OGRErr err = CreateNewTable();
    if (err != OGRERR_NONE) return err;

    CloseSequentialCursor();

    CPLString command = "DELETE FROM \"";
    const wchar_t * pwszFeatureClassName = efallib->GetTableName(hSession, hTable);
    char * pszFeatureClassName = CPLRecodeFromWChar(pwszFeatureClassName, CPL_ENC_UCS2, CPL_ENC_UTF8);
    command += pszFeatureClassName;
    CPLFree(pszFeatureClassName);
    command += "\" WHERE MI_Key = '";
    char szFID[64];
    szFID[0] = '\0';
    //ltoa((long)nFID, szFID, 10);
    CPLsnprintf(szFID, sizeof(szFID), "%ld", (long)nFID);
    command += szFID;
    command += "'";
    if (err == OGRERR_NONE)
    {
        wchar_t * szwCommand = CPLRecodeToWChar(command, CPL_ENC_UTF8, CPL_ENC_UCS2);
        long nrecs = efallib->Delete(hSession, szwCommand);
        CPLFree(szwCommand);
        err = (nrecs == 1) ? OGRERR_NONE : OGRERR_NON_EXISTING_FEATURE;
    }
    return err;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGREFALLayer::TestCapability(const char * pszCap)
{
    /*
    OLCRandomRead / "RandomRead": TRUE if the GetFeature() method is implemented in an optimized way for this layer, as opposed to the default
    implementation using ResetReading() and GetNextFeature() to find the requested feature id.
    */
    if (EQUAL(pszCap, OLCRandomRead))
        return TRUE;
    /*
    OLCSequentialWrite / "SequentialWrite": TRUE if the CreateFeature() method works for this layer. Note this means that this particular layer is
    writable. The same OGRLayer class may returned FALSE for other layer instances that are effectively read-only.
    */
    else if (EQUAL(pszCap, OLCSequentialWrite))
        return TRUE;
    /*
    OLCRandomWrite / "RandomWrite": TRUE if the SetFeature() method is operational on this layer. Note this means that this particular layer is
    writable. The same OGRLayer class may returned FALSE for other layer instances that are effectively read-only.
    */
    else if (EQUAL(pszCap, OLCRandomWrite))
        return TRUE;
    /*
    OLCFastSpatialFilter / "FastSpatialFilter": TRUE if this layer implements spatial filtering efficiently. Layers that effectively read all
    features, and test them with the OGRFeature intersection methods should return FALSE. This can be used as a clue by the application whether
    it should build and maintain its own spatial index for features in this layer.
    */
    else if (EQUAL(pszCap, OLCFastSpatialFilter))
        return TRUE;
    /*
    OLCFastFeatureCount / "FastFeatureCount": TRUE if this layer can return a feature count (via GetFeatureCount()) efficiently. i.e. without
    counting the features. In some cases this will return TRUE until a spatial filter is installed after which it will return FALSE.
    */
    else if (EQUAL(pszCap, OLCFastFeatureCount))
        return TRUE;
    /*
    OLCFastGetExtent / "FastGetExtent": TRUE if this layer can return its data extent (via GetExtent()) efficiently, i.e. without scanning all
    the features. In some cases this will return TRUE until a spatial filter is installed after which it will return FALSE.
    */
    else if (EQUAL(pszCap, OLCFastGetExtent))
        return TRUE;
    /*
    OLCFastSetNextByIndex / "FastSetNextByIndex": TRUE if this layer can perform the SetNextByIndex() call efficiently, otherwise FALSE.
    */
    else if (EQUAL(pszCap, OLCFastSetNextByIndex))
        return FALSE;
    /*
    OLCCreateField / "CreateField": TRUE if this layer can create new fields on the current layer using CreateField(), otherwise FALSE.
    */
    else if (EQUAL(pszCap, OLCCreateField))
        return bNew;
    /*
    OLCCreateGeomField / "CreateGeomField": (GDAL >= 1.11) TRUE if this layer can create new geometry fields on the current layer using
    CreateGeomField(), otherwise FALSE.
    */
    else if (EQUAL(pszCap, OLCCreateGeomField))
        return bNew;
    /*
    OLCDeleteField / "DeleteField": TRUE if this layer can delete existing fields on the current layer using DeleteField(), otherwise FALSE.
    */
    else if (EQUAL(pszCap, OLCDeleteField))
        return FALSE;
    /*
    OLCReorderFields / "ReorderFields": TRUE if this layer can reorder existing fields on the current layer using ReorderField() or
    ReorderFields(), otherwise FALSE.
    */
    else if (EQUAL(pszCap, OLCReorderFields))
        return FALSE;
    /*
    OLCAlterFieldDefn / "AlterFieldDefn": TRUE if this layer can alter the definition of an existing field on the current layer using
    AlterFieldDefn(), otherwise FALSE.
    */
    else if (EQUAL(pszCap, OLCAlterFieldDefn))
        return FALSE;
    /*
    OLCDeleteFeature / "DeleteFeature": TRUE if the DeleteFeature() method is supported on this layer, otherwise FALSE.
    */
    else if (EQUAL(pszCap, OLCDeleteFeature))
        return TRUE;
    /*
    OLCStringsAsUTF8 / "StringsAsUTF8": TRUE if values of OFTString fields are assured to be in UTF-8 format. If FALSE the encoding of fields is
    uncertain, though it might still be UTF-8.
    */
    else if (EQUAL(pszCap, OLCStringsAsUTF8))
        return TRUE;
    /*
    OLCTransactions / "Transactions": TRUE if the StartTransaction(), CommitTransaction() and RollbackTransaction() methods work in a
    meaningful way, otherwise FALSE.
    */
    else if (EQUAL(pszCap, OLCTransactions))
        return FALSE;
    /*
    OLCIgnoreFields / "IgnoreFields": TRUE if fields, geometry and style will be omitted when fetching features as set by SetIgnoredFields() method.
    */
    else if (EQUAL(pszCap, OLCIgnoreFields))
        return FALSE;
    /*
    OLCCurveGeometries / "CurveGeometries": TRUE if this layer supports writing curve geometries or may return such geometries. (GDAL 2.0).
    */
    else if (EQUAL(pszCap, OLCCurveGeometries))
        return FALSE;

    else
        return FALSE;
}

/************************************************************************/
/*                            GetTABType()                              */
/*                                                                      */
/*      Create a native field based on a generic OGR definition.        */
/************************************************************************/
// Adapted from MITAB
int OGREFALLayer::GetTABType(OGRFieldDefn *poField,
    Ellis::ALLTYPE_TYPE* peTABType,
    int *pnWidth,
    int *pnPrecision)
{
    Ellis::ALLTYPE_TYPE eTABType;
    int                 nWidth = poField->GetWidth();
    int                 nPrecision = poField->GetPrecision();

    if (poField->GetType() == OFTInteger)
    {
        eTABType = Ellis::ALLTYPE_TYPE::OT_INTEGER;
        // if (nWidth == 0)
            // nWidth = 12;
    }
    else if (poField->GetType() == OFTInteger64)
    {
        if (bCreateNativeX)
        {
            eTABType = Ellis::ALLTYPE_TYPE::OT_INTEGER64;
        }
        else
        {
            eTABType = Ellis::ALLTYPE_TYPE::OT_INTEGER;
        }
        // if (nWidth == 0)
            // nWidth = 12;
    }
    else if (poField->GetType() == OFTReal)
    {
        if (nWidth == 0 && poField->GetPrecision() == 0)
        {
            eTABType = Ellis::ALLTYPE_TYPE::OT_FLOAT;
            nWidth = 32;
        }
        else
        {
            eTABType = Ellis::ALLTYPE_TYPE::OT_DECIMAL;
            // Enforce Mapinfo limits, otherwise MapInfo will crash (#6392)
            if (nWidth > 20 || nWidth - nPrecision < 2 || nPrecision > 16)
            {
                if (nWidth > 20)
                    nWidth = 20;
                if (nWidth - nPrecision < 2)
                    nPrecision = nWidth - 2;
                if (nPrecision > 16)
                    nPrecision = 16;
                CPLDebug("EFAL",
                    "Adjusting initial width,precision of %s from %d,%d to %d,%d",
                    poField->GetNameRef(),
                    poField->GetWidth(), poField->GetPrecision(),
                    nWidth, nPrecision);
            }
        }
    }
    else if (poField->GetType() == OFTDate)
    {
        eTABType = Ellis::ALLTYPE_TYPE::OT_DATE;
        if (nWidth == 0)
            nWidth = 10;
    }
    else if (poField->GetType() == OFTTime)
    {
        eTABType = Ellis::ALLTYPE_TYPE::OT_TIME;
        if (nWidth == 0)
            nWidth = 9;
    }
    else if (poField->GetType() == OFTDateTime)
    {
        eTABType = Ellis::ALLTYPE_TYPE::OT_DATETIME;
        if (nWidth == 0)
            nWidth = 19;
    }
    else if (poField->GetType() == OFTString)
    {
        eTABType = Ellis::ALLTYPE_TYPE::OT_CHAR;
        if (nWidth == 0)
            nWidth = 254;
        else
            nWidth = std::min(254, nWidth);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "OGREFALLayer::CreateField() called with unsupported field"
            " type %d.\n"
            "Note that Mapinfo files don't support list field types.\n",
            poField->GetType());

        return -1;
    }

    *peTABType = eTABType;
    *pnWidth = nWidth;
    *pnPrecision = nPrecision;

    return 0;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/
OGRErr OGREFALLayer::CreateField(OGRFieldDefn *poNewField, int bApproxOK)
{
    if (!bNew)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
            "CreateField() cannot be used at this time.");
        return -1;
    }

    Ellis::ALLTYPE_TYPE eTABType;
    int nWidth = 0;
    int nPrecision = 0;

    if (GetTABType(poNewField, &eTABType, &nWidth, &nPrecision) < 0)
        return OGRERR_FAILURE;

    const char * pszName = poNewField->GetNameRef();

    /*-----------------------------------------------------------------
    * Validate field width... must be <= 254
    *----------------------------------------------------------------*/
    if (nWidth > 254)
    {
        CPLError(CE_Warning, CPLE_IllegalArg,
            "Invalid size (%d) for field '%s'.  "
            "Size must be 254 or less.", nWidth, pszName);
        nWidth = 254;
    }

    /*-----------------------------------------------------------------
    * Map fields with width=0 (variable length in OGR) to a valid default
    *----------------------------------------------------------------*/
    if (eTABType == Ellis::ALLTYPE_TYPE::OT_DECIMAL && nWidth == 0)
        nWidth = 20;
    else if (nWidth == 0)
        nWidth = 254; /* char fields */

                          /*-----------------------------------------------------------------
                          * Make sure field name is valid... check for special chars, etc.
                          * (pszCleanName will have to be freed.)
                          *----------------------------------------------------------------*/
    char *pszCleanName = EFAL_GDAL_DRIVER::TABCleanFieldName(pszName);

    if (!bApproxOK &&
        (poFeatureDefn->GetFieldIndex(pszCleanName) >= 0 ||
            !EQUAL(pszName, pszCleanName)))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
            "Failed to add field named '%s'",
            pszName);
    }

    char szNewFieldName[31 + 1];  // 31 is the max characters for a field name.
    strncpy(szNewFieldName, pszCleanName, sizeof(szNewFieldName) - 1);
    szNewFieldName[sizeof(szNewFieldName) - 1] = '\0';

    int nRenameNum = 1;

    while (poFeatureDefn->GetFieldIndex(szNewFieldName) >= 0 && nRenameNum < 10)
        CPLsnprintf(szNewFieldName, sizeof(szNewFieldName), "%.29s_%.1d", pszCleanName, nRenameNum++);

    while (poFeatureDefn->GetFieldIndex(szNewFieldName) >= 0 && nRenameNum < 100)
        CPLsnprintf(szNewFieldName, sizeof(szNewFieldName), "%.29s%.2d", pszCleanName, nRenameNum++);

    if (poFeatureDefn->GetFieldIndex(szNewFieldName) >= 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
            "Too many field names like '%s' when truncated to 31 letters "
            "for MapInfo format.", pszCleanName);
    }

    if (!EQUAL(pszCleanName, szNewFieldName))
    {
        CPLError(CE_Warning, CPLE_NotSupported,
            "Normalized/laundered field name: '%s' to '%s'",
            pszCleanName,
            szNewFieldName);
    }

    /*-----------------------------------------------------------------
    * Map MapInfo native types to OGR types
    *----------------------------------------------------------------*/
    OGRFieldDefn *poFieldDefn = nullptr;

    switch (eTABType)
    {
    case Ellis::ALLTYPE_TYPE::OT_CHAR:
        /*-------------------------------------------------
        * CHAR type
        *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(szNewFieldName, OFTString);
        poFieldDefn->SetWidth(nWidth);
        break;
    case Ellis::ALLTYPE_TYPE::OT_INTEGER:
        /*-------------------------------------------------
        * INTEGER type
        *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(szNewFieldName, OFTInteger);
        // if (nWidth <= 10)
            // poFieldDefn->SetWidth(nWidth);
        break;
    case Ellis::ALLTYPE_TYPE::OT_INTEGER64:
        /*-------------------------------------------------
        * INTEGER64 type
        *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(szNewFieldName, OFTInteger64);
        break;
    case Ellis::ALLTYPE_TYPE::OT_SMALLINT:
        /*-------------------------------------------------
        * SMALLINT type
        *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(szNewFieldName, OFTInteger);
        // if (nWidth <= 5)
            // poFieldDefn->SetWidth(nWidth);
        break;
    case Ellis::ALLTYPE_TYPE::OT_DECIMAL:
        /*-------------------------------------------------
        * DECIMAL type
        *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(szNewFieldName, OFTReal);
        poFieldDefn->SetWidth(nWidth);
        poFieldDefn->SetPrecision(nPrecision);
        break;
    case Ellis::ALLTYPE_TYPE::OT_FLOAT:
        /*-------------------------------------------------
        * FLOAT type
        *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(szNewFieldName, OFTReal);
        break;
    case Ellis::ALLTYPE_TYPE::OT_DATE:
        /*-------------------------------------------------
        * DATE type (V450, returned as a string: "DD/MM/YYYY")
        *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(szNewFieldName, OFTDate);
        // poFieldDefn->SetWidth(10);
        //TODO? m_nVersion = std::max(m_nVersion, 450);
        break;
    case Ellis::ALLTYPE_TYPE::OT_TIME:
        /*-------------------------------------------------
        * TIME type (V900, returned as a string: "HH:MM:SS")
        *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(szNewFieldName, OFTTime);
        // poFieldDefn->SetWidth(8);
        //TODO? m_nVersion = std::max(m_nVersion, 900);
        break;
    case Ellis::ALLTYPE_TYPE::OT_DATETIME:
        /*-------------------------------------------------
        * DATETIME type (V900, returned as a string: "DD/MM/YYYY HH:MM:SS")
        *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(szNewFieldName, OFTDateTime);
        // poFieldDefn->SetWidth(19);
        //TODO? m_nVersion = std::max(m_nVersion, 900);
        break;
    case Ellis::ALLTYPE_TYPE::OT_LOGICAL:
        /*-------------------------------------------------
        * LOGICAL type (value "T" or "F")
        *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(szNewFieldName, OFTString);
        poFieldDefn->SetWidth(1);
        break;
    default:
        CPLError(CE_Failure, CPLE_NotSupported,
            "Unsupported type for field %s", szNewFieldName);
        CPLFree(pszCleanName);
        return -1;
    }

    // TODO: Is there a way for the poNewField to say it should be indexed???

    /*-----------------------------------------------------
    * Add the FieldDefn to the FeatureDefn
    *----------------------------------------------------*/
    poFeatureDefn->AddFieldDefn(poFieldDefn);
    delete poFieldDefn;

    CPLFree(pszCleanName);
    return OGRERR_NONE;
}
