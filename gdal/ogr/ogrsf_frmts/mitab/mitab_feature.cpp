/**********************************************************************
 *
 * Name:     mitab_feature.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation of the feature classes specific to MapInfo files.
 * Author:   Daniel Morissette, dmorissette@dmsolutions.ca
 *
 **********************************************************************
 * Copyright (c) 1999-2002, Daniel Morissette
 * Copyright (c) 2014, Even Rouault <even.rouault at spatialys.com>
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#include "cpl_port.h"
#include "mitab.h"
#include "mitab_geometry.h"
#include "mitab_utils.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <utility>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "mitab.h"
#include "mitab_geometry.h"
#include "mitab_priv.h"
#include "mitab_utils.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_featurestyle.h"
#include "ogr_geometry.h"

CPL_CVSID("$Id$")

/*=====================================================================
 *                      class TABFeature
 *====================================================================*/

/**********************************************************************
 *                   TABFeature::TABFeature()
 *
 * Constructor.
 **********************************************************************/
TABFeature::TABFeature(OGRFeatureDefn *poDefnIn) :
    OGRFeature(poDefnIn),
    m_nMapInfoType(TAB_GEOM_NONE),
    m_dXMin(0),
    m_dYMin(0),
    m_dXMax(0),
    m_dYMax(0),
    m_bDeletedFlag(FALSE),
    m_nXMin(0),
    m_nYMin(0),
    m_nXMax(0),
    m_nYMax(0),
    m_nComprOrgX(0),
    m_nComprOrgY(0)
{}

/**********************************************************************
 *                   TABFeature::~TABFeature()
 *
 * Destructor.
 **********************************************************************/
TABFeature::~TABFeature() {}

/**********************************************************************
 *                     TABFeature::CreateFromMapInfoType()
 *
 * Factory that creates a TABFeature of the right class for the specified
 * MapInfo Type
 *
 **********************************************************************/
TABFeature *TABFeature::CreateFromMapInfoType(int nMapInfoType,
                                              OGRFeatureDefn *poDefn)
{
    TABFeature *poFeature = nullptr;

    /*-----------------------------------------------------------------
     * Create new feature object of the right type
     *----------------------------------------------------------------*/
    switch(nMapInfoType)
    {
    case TAB_GEOM_NONE:
        poFeature = new TABFeature(poDefn);
        break;
    case TAB_GEOM_SYMBOL_C:
    case TAB_GEOM_SYMBOL:
        poFeature = new TABPoint(poDefn);
        break;
    case TAB_GEOM_FONTSYMBOL_C:
    case TAB_GEOM_FONTSYMBOL:
        poFeature = new TABFontPoint(poDefn);
        break;
    case TAB_GEOM_CUSTOMSYMBOL_C:
    case TAB_GEOM_CUSTOMSYMBOL:
        poFeature = new TABCustomPoint(poDefn);
        break;
    case TAB_GEOM_LINE_C:
    case TAB_GEOM_LINE:
    case TAB_GEOM_PLINE_C:
    case TAB_GEOM_PLINE:
    case TAB_GEOM_MULTIPLINE_C:
    case TAB_GEOM_MULTIPLINE:
    case TAB_GEOM_V450_MULTIPLINE_C:
    case TAB_GEOM_V450_MULTIPLINE:
    case TAB_GEOM_V800_MULTIPLINE_C:
    case TAB_GEOM_V800_MULTIPLINE:
        poFeature = new TABPolyline(poDefn);
        break;
    case TAB_GEOM_ARC_C:
    case TAB_GEOM_ARC:
        poFeature = new TABArc(poDefn);
        break;

    case TAB_GEOM_REGION_C:
    case TAB_GEOM_REGION:
    case TAB_GEOM_V450_REGION_C:
    case TAB_GEOM_V450_REGION:
    case TAB_GEOM_V800_REGION_C:
    case TAB_GEOM_V800_REGION:
        poFeature = new TABRegion(poDefn);
        break;
    case TAB_GEOM_RECT_C:
    case TAB_GEOM_RECT:
    case TAB_GEOM_ROUNDRECT_C:
    case TAB_GEOM_ROUNDRECT:
        poFeature = new TABRectangle(poDefn);
        break;
    case TAB_GEOM_ELLIPSE_C:
    case TAB_GEOM_ELLIPSE:
        poFeature = new TABEllipse(poDefn);
        break;
    case TAB_GEOM_TEXT_C:
    case TAB_GEOM_TEXT:
        poFeature = new TABText(poDefn);
        break;
    case TAB_GEOM_MULTIPOINT_C:
    case TAB_GEOM_MULTIPOINT:
    case TAB_GEOM_V800_MULTIPOINT_C:
    case TAB_GEOM_V800_MULTIPOINT:
        poFeature = new TABMultiPoint(poDefn);
        break;
    case TAB_GEOM_COLLECTION_C:
    case TAB_GEOM_COLLECTION:
    case TAB_GEOM_V800_COLLECTION_C:
    case TAB_GEOM_V800_COLLECTION:
        poFeature = new TABCollection(poDefn);
        break;
    default:
        /*-------------------------------------------------------------
         * Unsupported feature type... we still return a valid feature
         * with NONE geometry after producing a Warning.
         * Callers can trap that case by checking CPLGetLastErrorNo()
         * against TAB_WarningFeatureTypeNotSupported
         *------------------------------------------------------------*/
        // poFeature = new TABDebugFeature(poDefn);
        poFeature = new TABFeature(poDefn);

        CPLError(CE_Warning,
                 static_cast<CPLErrorNum>(TAB_WarningFeatureTypeNotSupported),
                 "Unsupported object type %d (0x%2.2x).  Feature will be "
                 "returned with NONE geometry.",
                 nMapInfoType, nMapInfoType);
    }

    return poFeature;
}

/**********************************************************************
 *                     TABFeature::CopyTABFeatureBase()
 *
 * Used by CloneTABFeature() to copy the basic (fields, geometry, etc.)
 * TABFeature members.
 *
 * The newly created feature is owned by the caller, and will have its own
 * reference to the OGRFeatureDefn.
 *
 * It is possible to create the clone with a different OGRFeatureDefn,
 * in this case, the fields won't be copied of course.
 *
 **********************************************************************/
void TABFeature::CopyTABFeatureBase(TABFeature *poDestFeature)
{
    /*-----------------------------------------------------------------
     * Copy fields only if OGRFeatureDefn is the same
     *----------------------------------------------------------------*/
    OGRFeatureDefn *poThisDefnRef = GetDefnRef();

    if (poThisDefnRef == poDestFeature->GetDefnRef())
    {
        for( int i = 0; i < poThisDefnRef->GetFieldCount(); i++ )
        {
            poDestFeature->SetField(i, GetRawFieldRef(i));
        }
    }

    /*-----------------------------------------------------------------
     * Copy the geometry
     *----------------------------------------------------------------*/
    poDestFeature->SetGeometry(GetGeometryRef());

    double dXMin = 0.0;
    double dYMin = 0.0;
    double dXMax = 0.0;
    double dYMax = 0.0;
    GetMBR(dXMin, dYMin, dXMax, dYMax);
    poDestFeature->SetMBR(dXMin, dYMin, dXMax, dYMax);

    GInt32 nXMin = 0;
    GInt32 nYMin = 0;
    GInt32 nXMax = 0;
    GInt32 nYMax = 0;
    GetIntMBR(nXMin, nYMin, nXMax, nYMax);
    poDestFeature->SetIntMBR(nXMin, nYMin, nXMax, nYMax);

    // m_nMapInfoType is not carried but it is not required anyways.
    // it will default to TAB_GEOM_NONE
}

/**********************************************************************
 *                     TABFeature::CloneTABFeature()
 *
 * Duplicate feature, including stuff specific to each TABFeature type.
 *
 * The newly created feature is owned by the caller, and will have its own
 * reference to the OGRFeatureDefn.
 *
 * It is possible to create the clone with a different OGRFeatureDefn,
 * in this case, the fields won't be copied of course.
 *
 * This method calls the generic TABFeature::CopyTABFeatureBase() and
 * then copies any members specific to its own type.
 **********************************************************************/
TABFeature *TABFeature::CloneTABFeature(OGRFeatureDefn *poNewDefn /*=NULL*/ )
{
    /*-----------------------------------------------------------------
     * Alloc new feature and copy the base stuff
     *----------------------------------------------------------------*/
    TABFeature *poNew = new TABFeature(poNewDefn ? poNewDefn : GetDefnRef());

    CopyTABFeatureBase(poNew);

    /*-----------------------------------------------------------------
     * And members specific to this class
     *----------------------------------------------------------------*/
    // Nothing to do for this class

    return poNew;
}

/**********************************************************************
 *                   TABFeature::SetMBR()
 *
 * Set the values for the MBR corners for this feature.
 **********************************************************************/
void TABFeature::SetMBR( double dXMin, double dYMin,
                         double dXMax, double dYMax )
{
    m_dXMin = std::min(dXMin, dXMax);
    m_dYMin = std::min(dYMin, dYMax);
    m_dXMax = std::max(dXMin, dXMax);
    m_dYMax = std::max(dYMin, dYMax);
}

/**********************************************************************
 *                   TABFeature::GetMBR()
 *
 * Return the values for the MBR corners for this feature.
 **********************************************************************/
void TABFeature::GetMBR( double &dXMin, double &dYMin,
                         double &dXMax, double &dYMax )
{
    dXMin = m_dXMin;
    dYMin = m_dYMin;
    dXMax = m_dXMax;
    dYMax = m_dYMax;
}

/**********************************************************************
 *                   TABFeature::SetIntMBR()
 *
 * Set the integer coordinates values of the MBR of this feature.
 **********************************************************************/
void TABFeature::SetIntMBR(GInt32 nXMin, GInt32 nYMin,
                           GInt32 nXMax, GInt32 nYMax)
{
    m_nXMin = nXMin;
    m_nYMin = nYMin;
    m_nXMax = nXMax;
    m_nYMax = nYMax;
}

/**********************************************************************
 *                   TABFeature::GetIntMBR()
 *
 * Return the integer coordinates values of the MBR of this feature.
 **********************************************************************/
void TABFeature::GetIntMBR(GInt32 &nXMin, GInt32 &nYMin,
                           GInt32 &nXMax, GInt32 &nYMax)
{
    nXMin = m_nXMin;
    nYMin = m_nYMin;
    nXMax = m_nXMax;
    nYMax = m_nYMax;
}

/**********************************************************************
 *                   TABFeature::ReadRecordFromDATFile()
 *
 * Fill the fields part of the feature from the contents of the
 * table record pointed to by poDATFile.
 *
 * It is assumed that poDATFile currently points to the beginning of
 * the table record and that this feature's OGRFeatureDefn has been
 * properly initialized for this table.
 **********************************************************************/
int TABFeature::ReadRecordFromDATFile(TABDATFile *poDATFile)
{
    CPLAssert(poDATFile);

    const int numFields = poDATFile->GetNumFields();

    for( int iField = 0; iField < numFields; iField++)
    {
        switch(poDATFile->GetFieldType(iField))
        {
        case TABFChar:
        {
            int         iWidth( poDATFile->GetFieldWidth(iField) );
            CPLString   osValue( poDATFile->ReadCharField( iWidth ) );

            if( !poDATFile->GetEncoding().empty() )
            {
                osValue.Recode( poDATFile->GetEncoding(), CPL_ENC_UTF8 );
            }
            SetField(iField, osValue);
            break;
        }
        case TABFDecimal:
        {
            const double dValue =
                poDATFile->ReadDecimalField(poDATFile->GetFieldWidth(iField));
            SetField(iField, dValue);
            break;
        }
        case TABFInteger:
        {
            const int nValue =
                poDATFile->ReadIntegerField(poDATFile->GetFieldWidth(iField));
            SetField(iField, nValue);
            break;
        }
        case TABFSmallInt:
        {
            const int nValue =
                poDATFile->ReadSmallIntField(poDATFile->GetFieldWidth(iField));
            SetField(iField, nValue);
            break;
        }
        case TABFFloat:
        {
            const double dValue =
                poDATFile->ReadFloatField(poDATFile->GetFieldWidth(iField));
            SetField(iField, dValue);
            break;
        }
        case TABFLogical:
        {
            const char *pszValue =
                poDATFile->ReadLogicalField(poDATFile->GetFieldWidth(iField));
            SetField(iField, pszValue);
            break;
        }
        case TABFDate:
        {
#ifdef MITAB_USE_OFTDATETIME
            int nYear = 0;
            int nMonth = 0;
            int nDay = 0;
            const int status = poDATFile->ReadDateField(
                poDATFile->GetFieldWidth(iField), &nYear, &nMonth, &nDay);
            if( status == 0)
            {
                SetField(iField, nYear, nMonth, nDay, 0, 0, 0, 0);
            }
#else
            const char *pszValue =
                poDATFile->ReadDateField(poDATFile->GetFieldWidth(iField));
            SetField(iField, pszValue);
#endif
            break;
        }
        case TABFTime:
        {
#ifdef MITAB_USE_OFTDATETIME
            int nHour = 0;
            int nMin = 0;
            int nMS = 0;
            int nSec = 0;
            const int status = poDATFile->ReadTimeField(
                poDATFile->GetFieldWidth(iField), &nHour, &nMin, &nSec, &nMS);
            if( status == 0 )
            {
                int nYear = 0;
                int nMonth = 0;
                int nDay = 0;
                SetField(iField, nYear, nMonth, nDay, nHour, nMin,
                         nSec + nMS / 1000.0f, 0);
            }
#else
            const char *pszValue =
                poDATFile->ReadTimeField(poDATFile->GetFieldWidth(iField));
            SetField(iField, pszValue);
#endif
            break;
        }
        case TABFDateTime:
        {
#ifdef MITAB_USE_OFTDATETIME
            int nYear = 0;
            int nMonth = 0;
            int nDay = 0;
            int nHour = 0;
            int nMin = 0;
            int nMS = 0;
            int nSec = 0;
            const int status = poDATFile->ReadDateTimeField(
                poDATFile->GetFieldWidth(iField), &nYear, &nMonth, &nDay,
                &nHour, &nMin, &nSec, &nMS);
            if( status == 0 )
            {
                SetField(iField, nYear, nMonth, nDay, nHour, nMin,
                         nSec + nMS / 1000.0f, 0);
            }
#else
            const char *pszValue =
                poDATFile->ReadDateTimeField(poDATFile->GetFieldWidth(iField));
            SetField(iField, pszValue);
#endif
            break;
        }
        default:
            // Other type???  Impossible!
            CPLError(CE_Failure, CPLE_AssertionFailed,
                     "Unsupported field type!");
        }
    }

    return 0;
}

/**********************************************************************
 *                   TABFeature::WriteRecordToDATFile()
 *
 * Write the attribute part of the feature to the .DAT file.
 *
 * It is assumed that poDATFile currently points to the beginning of
 * the table record and that this feature's OGRFeatureDefn has been
 * properly initialized for this table.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABFeature::WriteRecordToDATFile(TABDATFile *poDATFile,
                                     TABINDFile *poINDFile, int *panIndexNo)
{
#ifdef MITAB_USE_OFTDATETIME
    int nYear = 0;
    int nMon = 0;
    int nDay = 0;
    int nHour = 0;
    int nMin = 0;
    int nTZFlag = 0;
    float fSec = 0.0f;
#endif

    CPLAssert(poDATFile);

    const int numFields = poDATFile->GetNumFields();

    poDATFile->MarkRecordAsExisting();

    int nStatus = 0;
    for( int iField = 0;
         nStatus == 0 && iField < numFields;
         iField++ )
    {
        // Hack for "extra" introduced field.
        if( iField >= GetDefnRef()->GetFieldCount() )
        {
            CPLAssert(poDATFile->GetFieldType(iField) == TABFInteger &&
                      iField == 0);
            nStatus = poDATFile->WriteIntegerField(static_cast<int>(GetFID()), poINDFile, 0);
            continue;
        }
        CPLAssert(panIndexNo != nullptr);

        switch(poDATFile->GetFieldType(iField))
        {
        case TABFChar:
            {
                CPLString   osValue( GetFieldAsString(iField) );
                if( !poDATFile->GetEncoding().empty() )
                {
                    osValue.Recode( CPL_ENC_UTF8, poDATFile->GetEncoding() );
                }
                nStatus = poDATFile->WriteCharField(
                    osValue, poDATFile->GetFieldWidth(iField),
                    poINDFile, panIndexNo[iField]);
            }
            break;
        case TABFDecimal:
            nStatus = poDATFile->WriteDecimalField(
                GetFieldAsDouble(iField), poDATFile->GetFieldWidth(iField),
                poDATFile->GetFieldPrecision(iField), poINDFile,
                panIndexNo[iField]);
            break;
        case TABFInteger:
            nStatus = poDATFile->WriteIntegerField(
                GetFieldAsInteger(iField), poINDFile, panIndexNo[iField]);
            break;
        case TABFSmallInt:
            nStatus =
                poDATFile->WriteSmallIntField(static_cast<GInt16>(GetFieldAsInteger(iField)),
                                              poINDFile, panIndexNo[iField]);
            break;
        case TABFFloat:
            nStatus = poDATFile->WriteFloatField(GetFieldAsDouble(iField),
                                                 poINDFile, panIndexNo[iField]);
            break;
        case TABFLogical:
            nStatus = poDATFile->WriteLogicalField(
                GetFieldAsString(iField), poINDFile, panIndexNo[iField]);
            break;
        case TABFDate:
#ifdef MITAB_USE_OFTDATETIME
            if (IsFieldSetAndNotNull(iField))
            {
                GetFieldAsDateTime(iField, &nYear, &nMon, &nDay, &nHour, &nMin,
                                   &fSec, &nTZFlag);
            }
            else
            {
                nYear = 0;
                nMon = 0;
                nDay = 0;
            }

            nStatus = poDATFile->WriteDateField(nYear, nMon, nDay, poINDFile,
                                                panIndexNo[iField]);
#else
            nStatus = poDATFile->WriteDateField(GetFieldAsString(iField),
                                                poINDFile, panIndexNo[iField]);
#endif
            break;
        case TABFTime:
#ifdef MITAB_USE_OFTDATETIME
            if (IsFieldSetAndNotNull(iField))
            {
                GetFieldAsDateTime(iField, &nYear, &nMon, &nDay, &nHour, &nMin,
                                   &fSec, &nTZFlag);
            }
            else
            {
                // Put negative values, so that WriteTimeField() forges
                // a negative value, and ultimately write -1 in the binary field
                nHour = -1;
                nMin = -1;
                fSec = -1;
            }
            nStatus = poDATFile->WriteTimeField(nHour, nMin, static_cast<int>(fSec),
                                                OGR_GET_MS(fSec), poINDFile,
                                                panIndexNo[iField]);

#else
            nStatus = poDATFile->WriteTimeField(GetFieldAsString(iField),
                                                poINDFile, panIndexNo[iField]);
#endif
            break;
        case TABFDateTime:
#ifdef MITAB_USE_OFTDATETIME
            if (IsFieldSetAndNotNull(iField))
            {
                GetFieldAsDateTime(iField, &nYear, &nMon, &nDay, &nHour, &nMin,
                                   &fSec, &nTZFlag);
            }
            else
            {
                nYear = 0;
                nMon = 0;
                nDay = 0;
                nHour = 0;
                nMin = 0;
                fSec = 0;
            }

            nStatus = poDATFile->WriteDateTimeField(
                nYear, nMon, nDay, nHour, nMin, static_cast<int>(fSec), OGR_GET_MS(fSec),
                poINDFile, panIndexNo[iField]);
#else
            nStatus = poDATFile->WriteDateTimeField(
                GetFieldAsString(iField), poINDFile, panIndexNo[iField]);
#endif
            break;
        default:
            // Other type???  Impossible!
            CPLError(CE_Failure, CPLE_AssertionFailed,
                     "Unsupported field type!");
        }
    }

    if (nStatus != 0 )
        return nStatus;

    if (poDATFile->CommitRecordToFile() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABFeature::ReadGeometryFromMAPFile()
 *
 * In derived classes, this method should be reimplemented to
 * fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that before calling ReadGeometryFromMAPFile(), poMAPFile
 * currently points to the beginning of a map object.
 *
 * bCoordBlockDataOnly=TRUE is used when this method is called to copy only
 * the CoordBlock data during splitting of object blocks. In this case we
 * need to process only the information related to the CoordBlock. One
 * important thing to avoid is reading/writing pen/brush/symbol definitions
 * as that would screw up their ref counters.
 *
 * ppoCoordBlock is used by TABCollection and by index splitting code
 * to provide a CoordBlock to use instead of the one from the poMAPFile and
 * return the current pointer at the end of the call.
 *
 * The current implementation does nothing since instances of TABFeature
 * objects contain no geometry (i.e. TAB_GEOM_NONE).
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABFeature::ReadGeometryFromMAPFile(TABMAPFile * /*poMapFile*/,
                                        TABMAPObjHdr * /*poObjHdr*/,
                                        GBool /*bCoordBlockDataOnly=FALSE*/,
                                        TABMAPCoordBlock ** /*ppoCoordBlock=NULL*/)
{
    // Nothing to do. Instances of TABFeature objects contain no geometry.
    return 0;
}

/**********************************************************************
 *                   TABFeature::UpdateMBR()
 *
 * Fetch envelope of poGeom and update MBR.
 * Integer coord MBR is updated only if poMapFile is not NULL.
 *
 * Returns 0 on success, or -1 if there is no geometry in object
 **********************************************************************/
int TABFeature::UpdateMBR(TABMAPFile *poMapFile /*=NULL*/)
{
    OGRGeometry *poGeom = GetGeometryRef();

    if (poGeom)
    {
        OGREnvelope oEnv;
        poGeom->getEnvelope(&oEnv);

        m_dXMin = oEnv.MinX;
        m_dYMin = oEnv.MinY;
        m_dXMax = oEnv.MaxX;
        m_dYMax = oEnv.MaxY;

        if (poMapFile)
        {
            poMapFile->Coordsys2Int(oEnv.MinX, oEnv.MinY, m_nXMin, m_nYMin);
            poMapFile->Coordsys2Int(oEnv.MaxX, oEnv.MaxY, m_nXMax, m_nYMax);
            // Coordsy2Int can transform a min value to a max one and vice
            // versa.
            if( m_nXMin > m_nXMax )
            {
                std::swap(m_nXMin, m_nXMax);
            }
            if( m_nYMin > m_nYMax )
            {
                std::swap(m_nYMin, m_nYMax);
            }
        }

        return 0;
    }

    return -1;
}

/**********************************************************************
 *                   TABFeature::ValidateCoordType()
 *
 * Checks the feature envelope to establish if the feature should be
 * written using Compressed coordinates or not and adjust m_nMapInfoType
 * accordingly. Calling this method also sets (initializes) m_nXMin, m_nYMin,
 * m_nXMax, m_nYMax
 *
 * This function should be used only by the ValidateMapInfoType()
 * implementations.
 *
 * Returns TRUE if coord. should be compressed, FALSE otherwise
 **********************************************************************/
GBool TABFeature::ValidateCoordType(TABMAPFile *poMapFile)
{
    GBool bCompr = FALSE;

    /*-------------------------------------------------------------
     * Decide if coordinates should be compressed or not.
     *------------------------------------------------------------*/
    if (UpdateMBR(poMapFile) == 0)
    {
        /* Test for max range < 65535 here instead of < 65536 to avoid
         * compressed coordinate overflows in some boundary situations
         */
        if ((static_cast<GIntBig>(m_nXMax) - m_nXMin) < 65535 &&
            (static_cast<GIntBig>(m_nYMax) - m_nYMin) < 65535)
        {
            bCompr = TRUE;
        }
        m_nComprOrgX = static_cast<int>(
            (static_cast<GIntBig>(m_nXMin) + m_nXMax) / 2);
        m_nComprOrgY = static_cast<int>(
            (static_cast<GIntBig>(m_nYMin) + m_nYMax) / 2);
    }

    /*-------------------------------------------------------------
     * Adjust native type
     *------------------------------------------------------------*/
    if (bCompr && ((m_nMapInfoType % 3) == 2))
        m_nMapInfoType = static_cast<TABGeomType>(m_nMapInfoType - 1);  // compr = 1, 4, 7, ...
    else if (!bCompr && ((m_nMapInfoType % 3) == 1))
        m_nMapInfoType = static_cast<TABGeomType>(m_nMapInfoType + 1);  // non-compr = 2, 5, 8, ...

    return bCompr;
}

/**********************************************************************
 *                   TABFeature::ForceCoordTypeAndOrigin()
 *
 * This function is used by TABCollection::ValidateMapInfoType() to force
 * the coord type and compressed origin of all members of a collection
 * to be the same. (A replacement for ValidateCoordType() for this
 * specific case)
 **********************************************************************/
void TABFeature::ForceCoordTypeAndOrigin(TABGeomType nMapInfoType, GBool bCompr,
                                         GInt32 nComprOrgX, GInt32 nComprOrgY,
                                         GInt32 nXMin, GInt32 nYMin,
                                         GInt32 nXMax, GInt32 nYMax)
{
    /*-------------------------------------------------------------
     * Set Compressed Origin and adjust native type
     *------------------------------------------------------------*/
    m_nComprOrgX = nComprOrgX;
    m_nComprOrgY = nComprOrgY;

    m_nMapInfoType = nMapInfoType;

    if (bCompr && ((m_nMapInfoType % 3) == 2))
        m_nMapInfoType = static_cast<TABGeomType>(m_nMapInfoType - 1);  // compr = 1, 4, 7, ...
    else if (!bCompr && ((m_nMapInfoType % 3) == 1))
        m_nMapInfoType = static_cast<TABGeomType>(m_nMapInfoType + 1);  // non-compr = 2, 5, 8, ...

    m_nXMin = nXMin;
    m_nYMin = nYMin;
    m_nXMax = nXMax;
    m_nYMax = nYMax;
}

/**********************************************************************
 *                   TABFeature::WriteGeometryToMAPFile()
 *
 *
 * In derived classes, this method should be reimplemented to
 * write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that before calling WriteGeometryToMAPFile(), poMAPFile
 * currently points to a valid map object.
 *
 * bCoordBlockDataOnly=TRUE is used when this method is called to copy only
 * the CoordBlock data during splitting of object blocks. In this case we
 * need to process only the information related to the CoordBlock. One
 * important thing to avoid is reading/writing pen/brush/symbol definitions
 * as that would screw up their ref counters.
 *
 * ppoCoordBlock is used by TABCollection and by index splitting code
 * to provide a CoordBlock to use instead of the one from the poMAPFile and
 * return the current pointer at the end of the call.
 *
 * The current implementation does nothing since instances of TABFeature
 * objects contain no geometry (i.e. TAB_GEOM_NONE).
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABFeature::WriteGeometryToMAPFile(TABMAPFile * /* poMapFile*/,
                                       TABMAPObjHdr * /*poObjHdr*/,
                                       GBool /*bCoordBlockDataOnly=FALSE*/,
                                       TABMAPCoordBlock ** /*ppoCoordBlock=NULL*/)
{
    /*-----------------------------------------------------------------
     * Nothing to do... instances of TABFeature objects contain no geometry.
     *----------------------------------------------------------------*/

    return 0;
}

/**********************************************************************
 *                   TABFeature::DumpMID()
 *
 * Dump feature attributes in a format similar to .MID data records.
 **********************************************************************/
void TABFeature::DumpMID(FILE *fpOut /*=NULL*/)
{
    OGRFeatureDefn *l_poDefn = GetDefnRef();

    if (fpOut == nullptr)
        fpOut = stdout;

    for( int iField = 0; iField < GetFieldCount(); iField++ )
    {
        OGRFieldDefn *poFDefn = l_poDefn->GetFieldDefn(iField);

        fprintf(fpOut, "  %s (%s) = %s\n",
                poFDefn->GetNameRef(),
                OGRFieldDefn::GetFieldTypeName(poFDefn->GetType()),
                GetFieldAsString(iField));
    }

    fflush(fpOut);
}

/**********************************************************************
 *                   TABFeature::DumpMIF()
 *
 * Dump feature geometry in a format similar to .MIF files.
 **********************************************************************/
void TABFeature::DumpMIF(FILE *fpOut /*=NULL*/)
{
    if (fpOut == nullptr)
        fpOut = stdout;

    /*-----------------------------------------------------------------
     * Generate output... not much to do, feature contains no geometry.
     *----------------------------------------------------------------*/
    fprintf(fpOut, "NONE\n");

    fflush(fpOut);
}

/*=====================================================================
 *                      class TABPoint
 *====================================================================*/

/**********************************************************************
 *                   TABPoint::TABPoint()
 *
 * Constructor.
 **********************************************************************/
TABPoint::TABPoint(OGRFeatureDefn *poDefnIn) : TABFeature(poDefnIn) {}

/**********************************************************************
 *                   TABPoint::~TABPoint()
 *
 * Destructor.
 **********************************************************************/
TABPoint::~TABPoint() {}

/**********************************************************************
 *                     TABPoint::CloneTABFeature()
 *
 * Duplicate feature, including stuff specific to each TABFeature type.
 *
 * This method calls the generic TABFeature::CloneTABFeature() and
 * then copies any members specific to its own type.
 **********************************************************************/
TABFeature *TABPoint::CloneTABFeature(OGRFeatureDefn *poNewDefn /*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Alloc new feature and copy the base stuff
     *----------------------------------------------------------------*/
    TABPoint *poNew = new TABPoint(poNewDefn ? poNewDefn : GetDefnRef());

    CopyTABFeatureBase(poNew);

    /*-----------------------------------------------------------------
     * And members specific to this class
     *----------------------------------------------------------------*/
    // ITABFeatureSymbol
    *(poNew->GetSymbolDefRef()) = *GetSymbolDefRef();

    return poNew;
}

/**********************************************************************
 *                   TABPoint::ValidateMapInfoType()
 *
 * Check the feature's geometry part and return the corresponding
 * mapinfo object type code.  The m_nMapInfoType member will also
 * be updated for further calls to GetMapInfoType();
 *
 * Returns TAB_GEOM_NONE if the geometry is not compatible with what
 * is expected for this object class.
 **********************************************************************/
TABGeomType TABPoint::ValidateMapInfoType(TABMAPFile *poMapFile /*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     * __TODO__ For now we always write in uncompressed format (until we
     * find that this is not correct... note that at this point the
     * decision to use compressed/uncompressed will likely be based on
     * the distance between the point and the object block center in
     * integer coordinates being > 32767 or not... remains to be verified)
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
    {
        switch(GetFeatureClass())
        {
        case TABFCFontPoint:
            m_nMapInfoType = TAB_GEOM_FONTSYMBOL;
            break;
        case TABFCCustomPoint:
            m_nMapInfoType = TAB_GEOM_CUSTOMSYMBOL;
            break;
        case TABFCPoint:
        default:
            m_nMapInfoType = TAB_GEOM_SYMBOL;
            break;
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABPoint: Missing or Invalid Geometry!");
        m_nMapInfoType = TAB_GEOM_NONE;
    }

    UpdateMBR(poMapFile);

    return m_nMapInfoType;
}

/**********************************************************************
 *                   TABPoint::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABPoint::ReadGeometryFromMAPFile(TABMAPFile *poMapFile,
                                      TABMAPObjHdr *poObjHdr,
                                      GBool bCoordBlockDataOnly /*=FALSE*/,
                                      TABMAPCoordBlock ** /*ppoCoordBlock=NULL*/)
{
    /* Nothing to do for bCoordBlockDataOnly (used by index splitting) */
    if (bCoordBlockDataOnly)
        return 0;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poObjHdr->m_nType;

    if (m_nMapInfoType != TAB_GEOM_SYMBOL &&
        m_nMapInfoType != TAB_GEOM_SYMBOL_C )
    {
        CPLError(
            CE_Failure, CPLE_AssertionFailed,
            "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
            m_nMapInfoType, m_nMapInfoType);
        return -1;
    }

    /*-----------------------------------------------------------------
     * Read object information
     *----------------------------------------------------------------*/
    TABMAPObjPoint *poPointHdr = cpl::down_cast<TABMAPObjPoint *>(poObjHdr);

    m_nSymbolDefIndex = poPointHdr->m_nSymbolId;  // Symbol index

    poMapFile->ReadSymbolDef(m_nSymbolDefIndex, &m_sSymbolDef);

    /*-----------------------------------------------------------------
     * Create and fill geometry object
     *----------------------------------------------------------------*/
    double dX = 0.0;
    double dY = 0.0;

    poMapFile->Int2Coordsys(poPointHdr->m_nX, poPointHdr->m_nY, dX, dY);
    OGRGeometry *poGeometry = new OGRPoint(dX, dY);

    SetGeometryDirectly(poGeometry);

    SetMBR(dX, dY, dX, dY);
    SetIntMBR(poObjHdr->m_nMinX, poObjHdr->m_nMinY,
              poObjHdr->m_nMaxX, poObjHdr->m_nMaxY);

    return 0;
}

/**********************************************************************
 *                   TABPoint::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABPoint::WriteGeometryToMAPFile(TABMAPFile *poMapFile,
                                     TABMAPObjHdr *poObjHdr,
                                     GBool bCoordBlockDataOnly /*=FALSE*/,
                                     TABMAPCoordBlock ** /*ppoCoordBlock=NULL*/)
{
    /* Nothing to do for bCoordBlockDataOnly (used by index splitting) */
    if (bCoordBlockDataOnly)
        return 0;

    /*-----------------------------------------------------------------
     * We assume that ValidateMapInfoType() was called already and that
     * the type in poObjHdr->m_nType is valid.
     *----------------------------------------------------------------*/
    CPLAssert(m_nMapInfoType == poObjHdr->m_nType);

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    OGRPoint *poPoint = nullptr;
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
        poPoint = poGeom->toPoint();
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABPoint: Missing or Invalid Geometry!");
        return -1;
    }

    GInt32 nX = 0;
    GInt32 nY = 0;
    poMapFile->Coordsys2Int(poPoint->getX(), poPoint->getY(), nX, nY);

    /*-----------------------------------------------------------------
     * Copy object information
     *----------------------------------------------------------------*/
    TABMAPObjPoint *poPointHdr = cpl::down_cast<TABMAPObjPoint *>(poObjHdr);

    poPointHdr->m_nX = nX;
    poPointHdr->m_nY = nY;
    poPointHdr->SetMBR(nX, nY, nX, nY);

    m_nSymbolDefIndex = poMapFile->WriteSymbolDef(&m_sSymbolDef);
    poPointHdr->m_nSymbolId = static_cast<GByte>(m_nSymbolDefIndex);  // Symbol index

    if (CPLGetLastErrorType() == CE_Failure)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABPoint::GetX()
 *
 * Return this point's X coordinate.
 **********************************************************************/
double TABPoint::GetX()
{

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    OGRPoint *poPoint = nullptr;
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
        poPoint = poGeom->toPoint();
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABPoint: Missing or Invalid Geometry!");
        return 0.0;
    }

    return poPoint->getX();
}

/**********************************************************************
 *                   TABPoint::GetY()
 *
 * Return this point's Y coordinate.
 **********************************************************************/
double TABPoint::GetY()
{
    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    OGRPoint *poPoint = nullptr;
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
        poPoint = poGeom->toPoint();
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABPoint: Missing or Invalid Geometry!");
        return 0.0;
    }

    return poPoint->getY();
}

/**********************************************************************
 *                   TABPoint::GetStyleString() const
 *
 * Return style string for this feature.
 *
 * Style String is built only once during the first call to GetStyleString().
 **********************************************************************/
const char *TABPoint::GetStyleString() const
{
    if (m_pszStyleString == nullptr)
    {
        m_pszStyleString = CPLStrdup(GetSymbolStyleString());
    }

    return m_pszStyleString;
}

/**********************************************************************
 *                   TABPoint::DumpMIF()
 *
 * Dump feature geometry in a format similar to .MIF POINTs.
 **********************************************************************/
void TABPoint::DumpMIF(FILE *fpOut /*=NULL*/)
{
    if (fpOut == nullptr)
        fpOut = stdout;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    OGRPoint *poPoint = nullptr;
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
        poPoint = poGeom->toPoint();
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABPoint: Missing or Invalid Geometry!");
        return;
    }

    /*-----------------------------------------------------------------
     * Generate output
     *----------------------------------------------------------------*/
    fprintf(fpOut, "POINT %.15g %.15g\n", poPoint->getX(), poPoint->getY());

    DumpSymbolDef(fpOut);

    /*-----------------------------------------------------------------
     * Handle stuff specific to derived classes
     *----------------------------------------------------------------*/
    // cppcheck-suppress knownConditionTrueFalse
    if (GetFeatureClass() == TABFCFontPoint)
    {
        TABFontPoint *poFeature = cpl::down_cast<TABFontPoint *>(this);
        fprintf(fpOut, "  m_nFontStyle     = 0x%2.2x (%d)\n",
                poFeature->GetFontStyleTABValue(),
                poFeature->GetFontStyleTABValue());

        poFeature->DumpFontDef(fpOut);
    }
    // cppcheck-suppress knownConditionTrueFalse
    if (GetFeatureClass() == TABFCCustomPoint)
    {
        TABCustomPoint *poFeature = cpl::down_cast<TABCustomPoint *>(this);

        fprintf(fpOut, "  m_nUnknown_      = 0x%2.2x (%d)\n",
                poFeature->m_nUnknown_, poFeature->m_nUnknown_);
        fprintf(fpOut, "  m_nCustomStyle   = 0x%2.2x (%d)\n",
                poFeature->GetCustomSymbolStyle(),
                poFeature->GetCustomSymbolStyle());

        poFeature->DumpFontDef(fpOut);
    }

    fflush(fpOut);
}

/*=====================================================================
 *                      class TABFontPoint
 *====================================================================*/

/**********************************************************************
 *                   TABFontPoint::TABFontPoint()
 *
 * Constructor.
 **********************************************************************/
TABFontPoint::TABFontPoint(OGRFeatureDefn *poDefnIn) :
    TABPoint(poDefnIn),
    m_dAngle(0.0),
    m_nFontStyle(0)
{}

/**********************************************************************
 *                   TABFontPoint::~TABFontPoint()
 *
 * Destructor.
 **********************************************************************/
TABFontPoint::~TABFontPoint() {}

/**********************************************************************
 *                     TABFontPoint::CloneTABFeature()
 *
 * Duplicate feature, including stuff specific to each TABFeature type.
 *
 * This method calls the generic TABFeature::CloneTABFeature() and
 * then copies any members specific to its own type.
 **********************************************************************/
TABFeature *TABFontPoint::CloneTABFeature(OGRFeatureDefn *poNewDefn /*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Alloc new feature and copy the base stuff
     *----------------------------------------------------------------*/
    TABFontPoint *poNew =
        new TABFontPoint(poNewDefn ? poNewDefn : GetDefnRef());

    CopyTABFeatureBase(poNew);

    /*-----------------------------------------------------------------
     * And members specific to this class
     *----------------------------------------------------------------*/
    // ITABFeatureSymbol
    *(poNew->GetSymbolDefRef()) = *GetSymbolDefRef();

    // ITABFeatureFont
    *(poNew->GetFontDefRef()) = *GetFontDefRef();

    poNew->SetSymbolAngle(GetSymbolAngle());
    poNew->SetFontStyleTABValue(GetFontStyleTABValue());

    return poNew;
}

/**********************************************************************
 *                   TABFontPoint::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABFontPoint::ReadGeometryFromMAPFile(TABMAPFile *poMapFile,
                                          TABMAPObjHdr *poObjHdr,
                                          GBool bCoordBlockDataOnly /*=FALSE*/,
                                          TABMAPCoordBlock ** /*ppoCoordBlock=NULL*/)
{
    /* Nothing to do for bCoordBlockDataOnly (used by index splitting) */
    if (bCoordBlockDataOnly)
        return 0;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poObjHdr->m_nType;

    if (m_nMapInfoType != TAB_GEOM_FONTSYMBOL &&
        m_nMapInfoType != TAB_GEOM_FONTSYMBOL_C )
    {
        CPLError(
            CE_Failure, CPLE_AssertionFailed,
            "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
            m_nMapInfoType, m_nMapInfoType);
        return -1;
    }

    /*-----------------------------------------------------------------
     * Read object information
     * NOTE: This symbol type does not contain a reference to a
     * SymbolDef block in the file, but we still use the m_sSymbolDef
     * structure to store the information inside the class so that the
     * ITABFeatureSymbol methods work properly for the class user.
     *----------------------------------------------------------------*/
    TABMAPObjFontPoint *poPointHdr = cpl::down_cast<TABMAPObjFontPoint *>(poObjHdr);

    m_nSymbolDefIndex = -1;
    m_sSymbolDef.nRefCount = 0;

    m_sSymbolDef.nSymbolNo = poPointHdr->m_nSymbolId;    // shape
    m_sSymbolDef.nPointSize = poPointHdr->m_nPointSize;  // point size

    m_nFontStyle = poPointHdr->m_nFontStyle;  // font style

    m_sSymbolDef.rgbColor = poPointHdr->m_nR * 256 * 256 +
                            poPointHdr->m_nG * 256 +
                            poPointHdr->m_nB;

    /*-------------------------------------------------------------
     * Symbol Angle, in tenths of degree.
     * Contrary to arc start/end angles, no conversion based on
     * origin quadrant is required here.
     *------------------------------------------------------------*/
    m_dAngle = poPointHdr->m_nAngle / 10.0;

    m_nFontDefIndex = poPointHdr->m_nFontId;  // Font name index

    poMapFile->ReadFontDef(m_nFontDefIndex, &m_sFontDef);

    /*-----------------------------------------------------------------
     * Create and fill geometry object
     *----------------------------------------------------------------*/
    double dX = 0.0;
    double dY = 0.0;
    poMapFile->Int2Coordsys(poPointHdr->m_nX, poPointHdr->m_nY, dX, dY);
    OGRGeometry *poGeometry = new OGRPoint(dX, dY);

    SetGeometryDirectly(poGeometry);

    SetMBR(dX, dY, dX, dY);
    SetIntMBR(poObjHdr->m_nMinX, poObjHdr->m_nMinY,
              poObjHdr->m_nMaxX, poObjHdr->m_nMaxY);

    return 0;
}

/**********************************************************************
 *                   TABFontPoint::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABFontPoint::WriteGeometryToMAPFile(TABMAPFile *poMapFile,
                                         TABMAPObjHdr *poObjHdr,
                                         GBool bCoordBlockDataOnly /*=FALSE*/,
                                         TABMAPCoordBlock ** /*ppoCoordBlock=NULL*/)
{
    /* Nothing to do for bCoordBlockDataOnly (used by index splitting) */
    if (bCoordBlockDataOnly)
        return 0;

    /*-----------------------------------------------------------------
     * We assume that ValidateMapInfoType() was called already and that
     * the type in poObjHdr->m_nType is valid.
     *----------------------------------------------------------------*/
    CPLAssert(m_nMapInfoType == poObjHdr->m_nType);

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    OGRPoint *poPoint = nullptr;
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
        poPoint = poGeom->toPoint();
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABFontPoint: Missing or Invalid Geometry!");
        return -1;
    }

    GInt32 nX = 0;
    GInt32 nY = 0;
    poMapFile->Coordsys2Int(poPoint->getX(), poPoint->getY(), nX, nY);

    /*-----------------------------------------------------------------
     * Copy object information
     * NOTE: This symbol type does not contain a reference to a
     * SymbolDef block in the file, but we still use the m_sSymbolDef
     * structure to store the information inside the class so that the
     * ITABFeatureSymbol methods work properly for the class user.
     *----------------------------------------------------------------*/
    TABMAPObjFontPoint *poPointHdr = cpl::down_cast<TABMAPObjFontPoint *>(poObjHdr);

    poPointHdr->m_nX = nX;
    poPointHdr->m_nY = nY;
    poPointHdr->SetMBR(nX, nY, nX, nY);

    poPointHdr->m_nSymbolId = static_cast<GByte>(m_sSymbolDef.nSymbolNo);    // shape
    poPointHdr->m_nPointSize = static_cast<GByte>(m_sSymbolDef.nPointSize);  // point size
    poPointHdr->m_nFontStyle = m_nFontStyle;                    // font style

    poPointHdr->m_nR = static_cast<GByte>(COLOR_R(m_sSymbolDef.rgbColor));
    poPointHdr->m_nG = static_cast<GByte>(COLOR_G(m_sSymbolDef.rgbColor));
    poPointHdr->m_nB = static_cast<GByte>(COLOR_B(m_sSymbolDef.rgbColor));

    /*-------------------------------------------------------------
     * Symbol Angle, in tenths of degree.
     * Contrary to arc start/end angles, no conversion based on
     * origin quadrant is required here.
     *------------------------------------------------------------*/
    poPointHdr->m_nAngle = static_cast<GInt16>(ROUND_INT(m_dAngle * 10.0));

    // Write Font Def
    m_nFontDefIndex = poMapFile->WriteFontDef(&m_sFontDef);
    poPointHdr->m_nFontId = static_cast<GByte>(m_nFontDefIndex);  // Font name index

    if (CPLGetLastErrorType() == CE_Failure)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABFontPoint::QueryFontStyle()
 *
 * Return TRUE if the specified font style attribute is turned ON,
 * or FALSE otherwise.  See enum TABFontStyle for the list of styles
 * that can be queried on.
 **********************************************************************/
GBool TABFontPoint::QueryFontStyle(TABFontStyle eStyleToQuery)
{
    return (m_nFontStyle & static_cast<int>(eStyleToQuery)) ? TRUE : FALSE;
}

void TABFontPoint::ToggleFontStyle(TABFontStyle eStyleToToggle, GBool bStyleOn)
{
    if (bStyleOn)
        m_nFontStyle |= static_cast<int>(eStyleToToggle);
    else
        m_nFontStyle &= ~static_cast<int>(eStyleToToggle);
}

/**********************************************************************
 *                   TABFontPoint::GetFontStyleMIFValue()
 *
 * Return the Font Style value for this object using the style values
 * that are used in a MIF FONT() clause.  See MIF specs (appendix A).
 *
 * The reason why we have to differentiate between the TAB and the MIF font
 * style values is that in TAB, TABFSBox is included in the style value
 * as code 0x100, but in MIF it is not included, instead it is implied by
 * the presence of the BG color in the FONT() clause (the BG color is
 * present only when TABFSBox or TABFSHalo is set).
 * This also has the effect of shifting all the other style values > 0x100
 * by 1 byte.
 *
 * NOTE: Even if there is no BG color for font symbols, we inherit this
 * problem because Font Point styles use the same codes as Text Font styles.
 **********************************************************************/
int TABFontPoint::GetFontStyleMIFValue()
{
    // The conversion is simply to remove bit 0x100 from the value and shift
    // down all values past this bit.
    return (m_nFontStyle & 0xff) + (m_nFontStyle & (0xff00 - 0x0100)) / 2;
}

void TABFontPoint::SetFontStyleMIFValue(int nStyle)
{
    m_nFontStyle = static_cast<GByte>((nStyle & 0xff) + (nStyle & 0x7f00) * 2);
}

/**********************************************************************
 *                   TABFontPoint::SetSymbolAngle()
 *
 * Set the symbol angle value in degrees, making sure the value is
 * always in the range [0..360]
 **********************************************************************/
void TABFontPoint::SetSymbolAngle(double dAngle)
{
    dAngle = fmod(dAngle, 360.0);
    if(dAngle < 0.0)
        dAngle += 360.0;

    m_dAngle = dAngle;
}

/**********************************************************************
 *                   TABFontPoint::GetSymbolStyleString()
 *
 *  Return a Symbol() string. All representations info for the Symbol are here.
 **********************************************************************/
const char* TABFontPoint::GetSymbolStyleString(double dfAngle) const
{
    /* Get the SymbolStyleString, and add the outline Color
       (halo/border in MapInfo Symbol terminology) */
    const char *outlineColor = nullptr;
    if (m_nFontStyle & 16)
        outlineColor = ",o:#000000";
    else if (m_nFontStyle & 512)
        outlineColor = ",o:#ffffff";
    else
        outlineColor = "";

    int         nAngle = static_cast<int>(dfAngle);
    const char* pszStyle;

    pszStyle=CPLSPrintf("SYMBOL(a:%d,c:#%6.6x,s:%dpt,id:\"font-sym-%d,ogr-sym-9\"%s,f:\"%s\")",
                        nAngle,
                        m_sSymbolDef.rgbColor,
                        m_sSymbolDef.nPointSize,
                        m_sSymbolDef.nSymbolNo,
                        outlineColor,
                        GetFontNameRef());
    return pszStyle;
}

/**********************************************************************
 *                   TABFontPoint::GetStyleString() const
 *
 * Return style string for this feature.
 *
 * Style String is built only once during the first call to GetStyleString().
 **********************************************************************/
const char *TABFontPoint::GetStyleString() const
{
    if (m_pszStyleString == nullptr)
    {
        m_pszStyleString = CPLStrdup(GetSymbolStyleString(GetSymbolAngle()));
    }

    return m_pszStyleString;
}

/**********************************************************************
 *                   TABFontPoint::SetSymbolFromStyle()
 *
 *  Set all Symbol var from a OGRStyleSymbol.
 **********************************************************************/
void TABFontPoint::SetSymbolFromStyle(OGRStyleSymbol* poSymbolStyle)
{
    ITABFeatureSymbol::SetSymbolFromStyle(poSymbolStyle);

    GBool bIsNull = 0;

    // Try to set font glyph number
    const char* pszSymbolId = poSymbolStyle->Id(bIsNull);
    if((!bIsNull) && pszSymbolId && STARTS_WITH(pszSymbolId, "font-sym-"))
    {
        const int nSymbolId = atoi(pszSymbolId+9);
        SetSymbolNo(static_cast<GInt16>(nSymbolId));
    }

    const char* pszFontName = poSymbolStyle->FontName(bIsNull);
    if((!bIsNull) && pszFontName)
    {
        SetFontName(pszFontName);
    }
}

/*=====================================================================
 *                      class TABCustomPoint
 *====================================================================*/

/**********************************************************************
 *                   TABCustomPoint::TABCustomPoint()
 *
 * Constructor.
 **********************************************************************/
TABCustomPoint::TABCustomPoint(OGRFeatureDefn *poDefnIn) :
    TABPoint(poDefnIn),
    m_nCustomStyle(0),
    m_nUnknown_(0)
{}

/**********************************************************************
 *                   TABCustomPoint::~TABCustomPoint()
 *
 * Destructor.
 **********************************************************************/
TABCustomPoint::~TABCustomPoint() {}

/**********************************************************************
 *                     TABCustomPoint::CloneTABFeature()
 *
 * Duplicate feature, including stuff specific to each TABFeature type.
 *
 * This method calls the generic TABFeature::CloneTABFeature() and
 * then copies any members specific to its own type.
 **********************************************************************/
TABFeature *TABCustomPoint::CloneTABFeature(OGRFeatureDefn *poNewDefn /*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Alloc new feature and copy the base stuff
     *----------------------------------------------------------------*/
    TABCustomPoint *poNew =
        new TABCustomPoint(poNewDefn ? poNewDefn : GetDefnRef());

    CopyTABFeatureBase(poNew);

    /*-----------------------------------------------------------------
     * And members specific to this class
     *----------------------------------------------------------------*/
    // ITABFeatureSymbol
    *(poNew->GetSymbolDefRef()) = *GetSymbolDefRef();

    // ITABFeatureFont
    *(poNew->GetFontDefRef()) = *GetFontDefRef();

    poNew->SetCustomSymbolStyle(GetCustomSymbolStyle());

    return poNew;
}

/**********************************************************************
 *                   TABCustomPoint::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABCustomPoint::ReadGeometryFromMAPFile(TABMAPFile *poMapFile,
                                            TABMAPObjHdr *poObjHdr,
                                            GBool bCoordBlockDataOnly /*=FALSE*/,
                                            TABMAPCoordBlock ** /*ppoCoordBlock=NULL*/)
{
    /* Nothing to do for bCoordBlockDataOnly (used by index splitting) */
    if (bCoordBlockDataOnly)
        return 0;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poObjHdr->m_nType;

    if (m_nMapInfoType != TAB_GEOM_CUSTOMSYMBOL &&
        m_nMapInfoType != TAB_GEOM_CUSTOMSYMBOL_C)
    {
        CPLError(
            CE_Failure, CPLE_AssertionFailed,
            "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
            m_nMapInfoType, m_nMapInfoType);
        return -1;
    }

    /*-----------------------------------------------------------------
     * Read object information
     *----------------------------------------------------------------*/
    TABMAPObjCustomPoint *poPointHdr = cpl::down_cast<TABMAPObjCustomPoint *>(poObjHdr);

    m_nUnknown_ = poPointHdr->m_nUnknown_;        // ???
    m_nCustomStyle = poPointHdr->m_nCustomStyle;  // 0x01=Show BG,
                                                  // 0x02=Apply Color

    m_nSymbolDefIndex = poPointHdr->m_nSymbolId;  // Symbol index
    poMapFile->ReadSymbolDef(m_nSymbolDefIndex, &m_sSymbolDef);

    m_nFontDefIndex = poPointHdr->m_nFontId;  // Font index
    poMapFile->ReadFontDef(m_nFontDefIndex, &m_sFontDef);

    /*-----------------------------------------------------------------
     * Create and fill geometry object
     *----------------------------------------------------------------*/
    double dX = 0.0;
    double dY = 0.0;
    poMapFile->Int2Coordsys(poPointHdr->m_nX, poPointHdr->m_nY, dX, dY);
    OGRGeometry *poGeometry = new OGRPoint(dX, dY);

    SetGeometryDirectly(poGeometry);

    SetMBR(dX, dY, dX, dY);
    SetIntMBR(poObjHdr->m_nMinX, poObjHdr->m_nMinY,
              poObjHdr->m_nMaxX, poObjHdr->m_nMaxY);

    return 0;
}

/**********************************************************************
 *                   TABCustomPoint::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABCustomPoint::WriteGeometryToMAPFile(TABMAPFile *poMapFile,
                                           TABMAPObjHdr *poObjHdr,
                                           GBool bCoordBlockDataOnly /*=FALSE*/,
                                           TABMAPCoordBlock ** /*ppoCoordBlock=NULL*/)
{
    /* Nothing to do for bCoordBlockDataOnly (used by index splitting) */
    if (bCoordBlockDataOnly)
        return 0;

    /*-----------------------------------------------------------------
     * We assume that ValidateMapInfoType() was called already and that
     * the type in poObjHdr->m_nType is valid.
     *----------------------------------------------------------------*/
    CPLAssert(m_nMapInfoType == poObjHdr->m_nType);

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    OGRPoint *poPoint = nullptr;
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
        poPoint = poGeom->toPoint();
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABCustomPoint: Missing or Invalid Geometry!");
        return -1;
    }

    GInt32 nX = 0;
    GInt32 nY = 0;
    poMapFile->Coordsys2Int(poPoint->getX(), poPoint->getY(), nX, nY);

    /*-----------------------------------------------------------------
     * Copy object information
     *----------------------------------------------------------------*/
    TABMAPObjCustomPoint *poPointHdr = cpl::down_cast<TABMAPObjCustomPoint *>(poObjHdr);

    poPointHdr->m_nX = nX;
    poPointHdr->m_nY = nY;
    poPointHdr->SetMBR(nX, nY, nX, nY);
    poPointHdr->m_nUnknown_ = m_nUnknown_;
    poPointHdr->m_nCustomStyle = m_nCustomStyle;  // 0x01=Show BG,
                                                  // 0x02=Apply Color

    m_nSymbolDefIndex = poMapFile->WriteSymbolDef(&m_sSymbolDef);
    poPointHdr->m_nSymbolId = static_cast<GByte>(m_nSymbolDefIndex);  // Symbol index

    m_nFontDefIndex = poMapFile->WriteFontDef(&m_sFontDef);
    poPointHdr->m_nFontId = static_cast<GByte>(m_nFontDefIndex);  // Font index

    if (CPLGetLastErrorType() == CE_Failure)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABCustomPoint::GetSymbolStyleString()
 *
 *  Return a Symbol() string. All representations info for the Symbol are here.
 **********************************************************************/
const char* TABCustomPoint::GetSymbolStyleString(double dfAngle) const
{
    /* Get the SymbolStyleString, and add the color if m_nCustomStyle contains "apply color". */
    const char *color = nullptr;
    if (m_nCustomStyle & 0x02)
        color = CPLSPrintf(",c:#%6.6x", m_sSymbolDef.rgbColor);
    else
        color = "";

    int         nAngle = static_cast<int>(dfAngle);
    const char* pszStyle;
    const char* pszExt = CPLGetExtension(GetSymbolNameRef());
    char        szLowerExt[8] = "";
    const char* pszPtr = pszExt;
    int         i;

    for(i=0; i < 7 && *pszPtr != '\0' && *pszPtr != ' '; i++, pszPtr++)
    {
        szLowerExt[i] = static_cast<char>(tolower(*pszPtr));
    }
    szLowerExt[i] = '\0';

    pszStyle=CPLSPrintf("SYMBOL(a:%d%s,s:%dpt,id:\"mapinfo-custom-sym-%d-%s,%s-%s,ogr-sym-9\")",
                        nAngle,
                        color,
                        m_sSymbolDef.nPointSize,
                        m_nCustomStyle,
                        GetSymbolNameRef(),
                        szLowerExt,
                        GetSymbolNameRef());
    return pszStyle;
}

/**********************************************************************
 *                   TABCustomPoint::SetSymbolFromStyle()
 *
 *  Set all Symbol var from a OGRStyleSymbol.
 **********************************************************************/
void TABCustomPoint::SetSymbolFromStyle(OGRStyleSymbol* poSymbolStyle)
{
   ITABFeatureSymbol::SetSymbolFromStyle(poSymbolStyle);

    GBool bIsNull = 0;

    // Try to set font glyph number
    const char* pszSymbolId = poSymbolStyle->Id(bIsNull);
    if((!bIsNull) && pszSymbolId && STARTS_WITH(pszSymbolId, "mapinfo-custom-sym-"))
    {
        const int nSymbolStyle = atoi(pszSymbolId+19);
        SetCustomSymbolStyle(static_cast<GByte>(nSymbolStyle));

        const char* pszPtr = pszSymbolId+19;
        while (*pszPtr != '-')
        {
            pszPtr++;
        }
        pszPtr++;

        char szSymbolName[256] = "";
        int  i;
        for(i=0; i < 255 && *pszPtr != '\0' && *pszPtr != ',' && *pszPtr != '"'; i++, pszPtr++)
        {
            szSymbolName[i] = *pszPtr;
        }
        szSymbolName[i] = '\0';
        SetSymbolName(szSymbolName);
    }
}

/**********************************************************************
 *                   TABCustomPoint::GetStyleString() const
 *
 * Return style string for this feature.
 *
 * Style String is built only once during the first call to GetStyleString().
 **********************************************************************/
const char *TABCustomPoint::GetStyleString() const
{
    if (m_pszStyleString == nullptr)
    {
        m_pszStyleString = CPLStrdup(GetSymbolStyleString());
    }

    return m_pszStyleString;
}

/*=====================================================================
 *                      class TABPolyline
 *====================================================================*/

/**********************************************************************
 *                   TABPolyline::TABPolyline()
 *
 * Constructor.
 **********************************************************************/
TABPolyline::TABPolyline(OGRFeatureDefn *poDefnIn) :
    TABFeature(poDefnIn),
    m_bCenterIsSet(FALSE),
    m_dCenterX(0.0),
    m_dCenterY(0.0),
    m_bWriteTwoPointLineAsPolyline(FALSE),
    m_bSmooth(FALSE)
{}

/**********************************************************************
 *                   TABPolyline::~TABPolyline()
 *
 * Destructor.
 **********************************************************************/
TABPolyline::~TABPolyline() {}

/**********************************************************************
 *                     TABPolyline::CloneTABFeature()
 *
 * Duplicate feature, including stuff specific to each TABFeature type.
 *
 * This method calls the generic TABFeature::CloneTABFeature() and
 * then copies any members specific to its own type.
 **********************************************************************/
TABFeature *TABPolyline::CloneTABFeature(OGRFeatureDefn *poNewDefn /*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Alloc new feature and copy the base stuff
     *----------------------------------------------------------------*/
    TABPolyline *poNew = new TABPolyline(poNewDefn ? poNewDefn : GetDefnRef());

    CopyTABFeatureBase(poNew);

    /*-----------------------------------------------------------------
     * And members specific to this class
     *----------------------------------------------------------------*/
    // ITABFeaturePen
    *(poNew->GetPenDefRef()) = *GetPenDefRef();

    poNew->m_bSmooth = m_bSmooth;
    poNew->m_bCenterIsSet = m_bCenterIsSet;
    poNew->m_dCenterX = m_dCenterX;
    poNew->m_dCenterY = m_dCenterY;

    return poNew;
}

/**********************************************************************
 *                   TABPolyline::GetNumParts()
 *
 * Return the total number of parts in this object.
 *
 * Returns 0 if the geometry contained in the object is invalid or missing.
 **********************************************************************/
int TABPolyline::GetNumParts()
{
    int numParts = 0;

    OGRGeometry *poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString)
    {
        /*-------------------------------------------------------------
         * Simple polyline
         *------------------------------------------------------------*/
        numParts = 1;
    }
    else if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbMultiLineString)
    {
        /*-------------------------------------------------------------
         * Multiple polyline
         *------------------------------------------------------------*/
        OGRMultiLineString *poMultiLine = poGeom->toMultiLineString();
        numParts = poMultiLine->getNumGeometries();
    }

    return numParts;
}

/**********************************************************************
 *                   TABPolyline::GetPartRef()
 *
 * Returns a reference to the specified OGRLineString number, hiding the
 * complexity of dealing with OGRMultiLineString vs OGRLineString cases.
 *
 * Returns NULL if the geometry contained in the object is invalid or
 * missing or if the specified part index is invalid.
 **********************************************************************/
OGRLineString *TABPolyline::GetPartRef(int nPartIndex)
{
    OGRGeometry *poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString && nPartIndex==0)
    {
        /*-------------------------------------------------------------
         * Simple polyline
         *------------------------------------------------------------*/
        return poGeom->toLineString();
    }
    else if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbMultiLineString)
    {
        /*-------------------------------------------------------------
         * Multiple polyline
         *------------------------------------------------------------*/
        OGRMultiLineString *poMultiLine = poGeom->toMultiLineString();
        if (nPartIndex >= 0 &&
            nPartIndex < poMultiLine->getNumGeometries())
        {
            return poMultiLine->getGeometryRef(nPartIndex)->toLineString();
        }
        else
            return nullptr;
    }

    return nullptr;
}

/**********************************************************************
 *                   TABPolyline::ValidateMapInfoType()
 *
 * Check the feature's geometry part and return the corresponding
 * mapinfo object type code.  The m_nMapInfoType member will also
 * be updated for further calls to GetMapInfoType();
 *
 * Returns TAB_GEOM_NONE if the geometry is not compatible with what
 * is expected for this object class.
 **********************************************************************/
TABGeomType TABPolyline::ValidateMapInfoType(TABMAPFile *poMapFile /*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString)
    {
        /*-------------------------------------------------------------
         * Simple polyline
         *------------------------------------------------------------*/
        OGRLineString *poLine = poGeom->toLineString();
        if ( TAB_REGION_PLINE_REQUIRES_V800(1, poLine->getNumPoints()) )
        {
            m_nMapInfoType = TAB_GEOM_V800_MULTIPLINE;
        }
        else if ( poLine->getNumPoints() > TAB_REGION_PLINE_300_MAX_VERTICES)
        {
            m_nMapInfoType = TAB_GEOM_V450_MULTIPLINE;
        }
        else if ( poLine->getNumPoints() > 2 )
        {
            m_nMapInfoType = TAB_GEOM_PLINE;
        }
        else if ( (poLine->getNumPoints() == 2) &&
                  (m_bWriteTwoPointLineAsPolyline == TRUE) )
        {
           m_nMapInfoType = TAB_GEOM_PLINE;
        }
        else if ( (poLine->getNumPoints() == 2) &&
                  (m_bWriteTwoPointLineAsPolyline == FALSE) )
        {
            m_nMapInfoType = TAB_GEOM_LINE;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AssertionFailed,
                     "TABPolyline: Geometry must contain at least 2 points.");
            m_nMapInfoType = TAB_GEOM_NONE;
        }
    }
    else if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbMultiLineString)
    {
        /*-------------------------------------------------------------
         * Multiple polyline... validate all components
         *------------------------------------------------------------*/
        GInt32 numPointsTotal = 0;
        OGRMultiLineString *poMultiLine = poGeom->toMultiLineString();
        int numLines = poMultiLine->getNumGeometries();

        m_nMapInfoType = TAB_GEOM_MULTIPLINE;

        for( int iLine = 0; iLine < numLines; iLine++ )
        {
            poGeom = poMultiLine->getGeometryRef(iLine);
            if (poGeom == nullptr ||
                wkbFlatten(poGeom->getGeometryType()) != wkbLineString)
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "TABPolyline: Object contains an invalid Geometry!");
                m_nMapInfoType = TAB_GEOM_NONE;
                numPointsTotal = 0;
                break;
            }
            OGRLineString *poLine = poGeom->toLineString();
            numPointsTotal += poLine->getNumPoints();
        }

        if ( TAB_REGION_PLINE_REQUIRES_V800(numLines, numPointsTotal) )
            m_nMapInfoType = TAB_GEOM_V800_MULTIPLINE;
        else if (numPointsTotal > TAB_REGION_PLINE_300_MAX_VERTICES)
            m_nMapInfoType = TAB_GEOM_V450_MULTIPLINE;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABPolyline: Missing or Invalid Geometry!");
        m_nMapInfoType = TAB_GEOM_NONE;
    }

    /*-----------------------------------------------------------------
     * Decide if coordinates should be compressed or not.
     *
     * __TODO__ We never write type LINE (2 points line) as compressed
     * for the moment.  If we ever do it, then the decision to write
     * a 2 point line in compressed coordinates or not should take into
     * account the location of the object block MBR, so this would be
     * better handled directly by TABMAPObjLine::WriteObject() since the
     * object block center is not known until it is written to disk.
     *----------------------------------------------------------------*/
    if (m_nMapInfoType != TAB_GEOM_LINE)
    {
        ValidateCoordType(poMapFile);
    }
    else
    {
        UpdateMBR(poMapFile);
    }

    return m_nMapInfoType;
}

/**********************************************************************
 *                   TABPolyline::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABPolyline::ReadGeometryFromMAPFile(TABMAPFile *poMapFile,
                                         TABMAPObjHdr *poObjHdr,
                                         GBool bCoordBlockDataOnly /*=FALSE*/,
                                         TABMAPCoordBlock **ppoCoordBlock/*=NULL*/)
{
    GInt32 nX = 0;
    GInt32 nY = 0;
    double dX = 0.0;
    double dY = 0.0;
    double dXMin = 0.0;
    double dYMin = 0.0;
    double dXMax = 0.0;
    double dYMax = 0.0;
    OGRGeometry *poGeometry = nullptr;
    OGRLineString *poLine = nullptr;
    GBool bComprCoord = poObjHdr->IsCompressedType();
    TABMAPCoordBlock *poCoordBlock = nullptr;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poObjHdr->m_nType;

    if (m_nMapInfoType == TAB_GEOM_LINE ||
        m_nMapInfoType == TAB_GEOM_LINE_C)
    {
        /*=============================================================
         * LINE (2 vertices)
         *============================================================*/
        TABMAPObjLine *poLineHdr = cpl::down_cast<TABMAPObjLine *>(poObjHdr);

        m_bSmooth = FALSE;

        poLine = new OGRLineString();
        poGeometry = poLine;
        poLine->setNumPoints(2);

        poMapFile->Int2Coordsys(poLineHdr->m_nX1, poLineHdr->m_nY1,
                                dXMin, dYMin);
        poLine->setPoint(0, dXMin, dYMin);

        poMapFile->Int2Coordsys(poLineHdr->m_nX2, poLineHdr->m_nY2,
                                dXMax, dYMax);
        poLine->setPoint(1, dXMax, dYMax);

        if (!bCoordBlockDataOnly)
        {
            m_nPenDefIndex = poLineHdr->m_nPenId;  // Pen index
            poMapFile->ReadPenDef(m_nPenDefIndex, &m_sPenDef);
        }
    }
    else if (m_nMapInfoType == TAB_GEOM_PLINE ||
             m_nMapInfoType == TAB_GEOM_PLINE_C )
    {
        /*=============================================================
         * PLINE ( > 2 vertices)
         *============================================================*/

        /*-------------------------------------------------------------
         * Copy data from poObjHdr
         *------------------------------------------------------------*/
        TABMAPObjPLine *poPLineHdr = cpl::down_cast<TABMAPObjPLine *>(poObjHdr);

        GInt32 nCoordBlockPtr = poPLineHdr->m_nCoordBlockPtr;
        const GUInt32 nCoordDataSize = poPLineHdr->m_nCoordDataSize;
        if( nCoordDataSize > 1024 * 1024 &&
            nCoordDataSize > poMapFile->GetFileSize() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Too big nCoordDataSize = %u", nCoordDataSize);
            return -1;
        }
        // numLineSections = poPLineHdr->m_numLineSections; // Always 1
        m_bSmooth = poPLineHdr->m_bSmooth;

        // Centroid/label point
        poMapFile->Int2Coordsys(poPLineHdr->m_nLabelX, poPLineHdr->m_nLabelY,
                                dX, dY);
        SetCenter(dX, dY);

        // Compressed coordinate origin (useful only in compressed case!)
        m_nComprOrgX = poPLineHdr->m_nComprOrgX;
        m_nComprOrgY = poPLineHdr->m_nComprOrgY;

        // MBR
        poMapFile->Int2Coordsys(poPLineHdr->m_nMinX, poPLineHdr->m_nMinY,
                                dXMin, dYMin);
        poMapFile->Int2Coordsys(poPLineHdr->m_nMaxX, poPLineHdr->m_nMaxY,
                                dXMax, dYMax);

        if (!bCoordBlockDataOnly)
        {
            m_nPenDefIndex = poPLineHdr->m_nPenId;  // Pen index
            poMapFile->ReadPenDef(m_nPenDefIndex, &m_sPenDef);
        }

        /*-------------------------------------------------------------
         * Create Geometry and read coordinates
         *------------------------------------------------------------*/
        const int numPoints = nCoordDataSize / (bComprCoord ? 4 : 8);

        if (ppoCoordBlock != nullptr && *ppoCoordBlock != nullptr)
            poCoordBlock = *ppoCoordBlock;
        else
            poCoordBlock = poMapFile->GetCoordBlock(nCoordBlockPtr);
        if (poCoordBlock == nullptr)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Can't access coordinate block at offset %d",
                     nCoordBlockPtr);
            return -1;
        }

        poCoordBlock->SetComprCoordOrigin(m_nComprOrgX, m_nComprOrgY);

        poLine = new OGRLineString();
        poGeometry = poLine;
        poLine->setNumPoints(numPoints);

        int nStatus = 0;
        for( int i = 0; nStatus == 0 && i<numPoints; i++ )
        {
            nStatus = poCoordBlock->ReadIntCoord(bComprCoord, nX, nY);
            if (nStatus != 0)
                break;
            poMapFile->Int2Coordsys(nX, nY, dX, dY);
            poLine->setPoint(i, dX, dY);
        }

        if (nStatus != 0)
        {
            // Failed ... error message has already been produced
            delete poGeometry;
            return nStatus;
        }
    }
    else if (m_nMapInfoType == TAB_GEOM_MULTIPLINE ||
             m_nMapInfoType == TAB_GEOM_MULTIPLINE_C ||
             m_nMapInfoType == TAB_GEOM_V450_MULTIPLINE ||
             m_nMapInfoType == TAB_GEOM_V450_MULTIPLINE_C ||
             m_nMapInfoType == TAB_GEOM_V800_MULTIPLINE ||
             m_nMapInfoType == TAB_GEOM_V800_MULTIPLINE_C )
    {
        /*=============================================================
         * PLINE MULTIPLE
         *============================================================*/
        const int nVersion = TAB_GEOM_GET_VERSION(m_nMapInfoType);

        /*-------------------------------------------------------------
         * Copy data from poObjHdr
         *------------------------------------------------------------*/
        TABMAPObjPLine *poPLineHdr = cpl::down_cast<TABMAPObjPLine *>(poObjHdr);

        GInt32 nCoordBlockPtr = poPLineHdr->m_nCoordBlockPtr;
        /* GInt32 nCoordDataSize  = poPLineHdr->m_nCoordDataSize; */
        GInt32 numLineSections = poPLineHdr->m_numLineSections;
        m_bSmooth = poPLineHdr->m_bSmooth;

        // Centroid/label point
        poMapFile->Int2Coordsys(poPLineHdr->m_nLabelX, poPLineHdr->m_nLabelY,
                                dX, dY);
        SetCenter(dX, dY);

        // Compressed coordinate origin (useful only in compressed case!)
        m_nComprOrgX = poPLineHdr->m_nComprOrgX;
        m_nComprOrgY = poPLineHdr->m_nComprOrgY;

        // MBR
        poMapFile->Int2Coordsys(poPLineHdr->m_nMinX, poPLineHdr->m_nMinY,
                                dXMin, dYMin);
        poMapFile->Int2Coordsys(poPLineHdr->m_nMaxX, poPLineHdr->m_nMaxY,
                                dXMax, dYMax);

        if (!bCoordBlockDataOnly)
        {
            m_nPenDefIndex = poPLineHdr->m_nPenId;  // Pen index
            poMapFile->ReadPenDef(m_nPenDefIndex, &m_sPenDef);
        }

        const int nMinSizeOfSection = 24;
        if( numLineSections > INT_MAX / nMinSizeOfSection )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Too many numLineSections");
            return -1;
        }
        const GUInt32 nMinimumBytesForSections =
                                nMinSizeOfSection * numLineSections;
        if( nMinimumBytesForSections > 1024 * 1024 &&
            nMinimumBytesForSections > poMapFile->GetFileSize() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Too many numLineSections");
            return -1;
        }

        /*-------------------------------------------------------------
         * Read data from the coord. block
         *------------------------------------------------------------*/
        TABMAPCoordSecHdr *pasSecHdrs =
            static_cast<TABMAPCoordSecHdr *>(VSI_MALLOC2_VERBOSE(numLineSections,
                                                     sizeof(TABMAPCoordSecHdr)));
        if( pasSecHdrs == nullptr )
            return -1;

        if (ppoCoordBlock != nullptr && *ppoCoordBlock != nullptr)
            poCoordBlock = *ppoCoordBlock;
        else
            poCoordBlock = poMapFile->GetCoordBlock(nCoordBlockPtr);

        GInt32 numPointsTotal = 0;
        if (poCoordBlock == nullptr ||
            poCoordBlock->ReadCoordSecHdrs(bComprCoord, nVersion,
                                           numLineSections,
                                           pasSecHdrs, numPointsTotal) != 0)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed reading coordinate data at offset %d",
                     nCoordBlockPtr);
            CPLFree(pasSecHdrs);
            return -1;
        }

        const GUInt32 nMinimumBytesForPoints =
                        (bComprCoord ? 4 : 8) * numPointsTotal;
        if( nMinimumBytesForPoints > 1024 * 1024 &&
            nMinimumBytesForPoints > poMapFile->GetFileSize() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Too many numPointsTotal");
            CPLFree(pasSecHdrs);
            return -1;
        }

        poCoordBlock->SetComprCoordOrigin(m_nComprOrgX, m_nComprOrgY);

        GInt32 *panXY = static_cast<GInt32 *>(
            VSI_MALLOC2_VERBOSE(numPointsTotal, 2 * sizeof(GInt32)));
        if( panXY == nullptr )
        {
            CPLFree(pasSecHdrs);
            return -1;
        }

        if (poCoordBlock->ReadIntCoords(bComprCoord, numPointsTotal, panXY) != 0)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed reading coordinate data at offset %d",
                     nCoordBlockPtr);
            CPLFree(pasSecHdrs);
            CPLFree(panXY);
            return -1;
        }

        /*-------------------------------------------------------------
         * Create a Geometry collection with one line geometry for
         * each coordinates section
         * If object contains only one section, then return a simple LineString
         *------------------------------------------------------------*/
        OGRMultiLineString *poMultiLine = nullptr;
        if (numLineSections > 1)
        {
            poMultiLine = new OGRMultiLineString();
            poGeometry = poMultiLine;
        }

        for( int iSection = 0; iSection < numLineSections; iSection++ )
        {
            const int numSectionVertices = pasSecHdrs[iSection].numVertices;
            GInt32 *pnXYPtr = panXY + (pasSecHdrs[iSection].nVertexOffset * 2);

            poLine = new OGRLineString();
            poLine->setNumPoints(numSectionVertices);

            for( int i = 0; i < numSectionVertices; i++ )
            {
                poMapFile->Int2Coordsys(*pnXYPtr, *(pnXYPtr + 1), dX, dY);
                poLine->setPoint(i, dX, dY);
                pnXYPtr += 2;
            }

            if (poGeometry == nullptr)
                poGeometry = poLine;
            else if (poMultiLine->addGeometryDirectly(poLine) != OGRERR_NONE)
            {
                CPLAssert(false);  // Just in case lower-level lib is modified
            }
            poLine = nullptr;
        }

        CPLFree(pasSecHdrs);
        CPLFree(panXY);
    }
    else
    {
        CPLError(
            CE_Failure, CPLE_AssertionFailed,
            "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
            m_nMapInfoType, m_nMapInfoType);
        return -1;
    }

    SetGeometryDirectly(poGeometry);

    SetMBR(dXMin, dYMin, dXMax, dYMax);
    SetIntMBR(poObjHdr->m_nMinX, poObjHdr->m_nMinY,
              poObjHdr->m_nMaxX, poObjHdr->m_nMaxY);

    /* Return a ref to coord block so that caller can continue reading
     * after the end of this object (used by TABCollection and index splitting)
     */
    if (ppoCoordBlock)
        *ppoCoordBlock = poCoordBlock;

    return 0;
}

/**********************************************************************
 *                   TABPolyline::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABPolyline::WriteGeometryToMAPFile(TABMAPFile *poMapFile,
                                        TABMAPObjHdr *poObjHdr,
                                        GBool bCoordBlockDataOnly /*=FALSE*/,
                                        TABMAPCoordBlock **ppoCoordBlock/*=NULL*/)
{
    GInt32 nX = 0;
    GInt32 nY = 0;
    OGRLineString *poLine = nullptr;
    TABMAPCoordBlock *poCoordBlock = nullptr;

    /*-----------------------------------------------------------------
     * We assume that ValidateMapInfoType() was called already and that
     * the type in poObjHdr->m_nType is valid.
     *----------------------------------------------------------------*/
    CPLAssert(m_nMapInfoType == poObjHdr->m_nType);
    CPLErrorReset();

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();

    if ((m_nMapInfoType == TAB_GEOM_LINE ||
         m_nMapInfoType == TAB_GEOM_LINE_C ) &&
        poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString &&
        (poLine = poGeom->toLineString())->getNumPoints() == 2)
    {
        /*=============================================================
         * LINE (2 vertices)
         *============================================================*/
        TABMAPObjLine *poLineHdr = cpl::down_cast<TABMAPObjLine *>(poObjHdr);

        poMapFile->Coordsys2Int(poLine->getX(0), poLine->getY(0),
                                poLineHdr->m_nX1, poLineHdr->m_nY1);
        poMapFile->Coordsys2Int(poLine->getX(1), poLine->getY(1),
                                poLineHdr->m_nX2, poLineHdr->m_nY2);
        poLineHdr->SetMBR(poLineHdr->m_nX1, poLineHdr->m_nY1,
                          poLineHdr->m_nX2, poLineHdr->m_nY2 );

        if (!bCoordBlockDataOnly)
        {
            m_nPenDefIndex = poMapFile->WritePenDef(&m_sPenDef);
            poLineHdr->m_nPenId = static_cast<GByte>(m_nPenDefIndex);      // Pen index
        }
    }
    else if ((m_nMapInfoType == TAB_GEOM_PLINE ||
              m_nMapInfoType == TAB_GEOM_PLINE_C ) &&
             poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString )
    {
        /*=============================================================
         * PLINE ( > 2 vertices and less than 32767 vertices)
         *============================================================*/
        GBool bCompressed = poObjHdr->IsCompressedType();

        /*-------------------------------------------------------------
         * Process geometry first...
         *------------------------------------------------------------*/
        poLine = poGeom->toLineString();
        const int numPoints = poLine->getNumPoints();
        CPLAssert(numPoints <= TAB_REGION_PLINE_300_MAX_VERTICES);

        if (ppoCoordBlock != nullptr && *ppoCoordBlock != nullptr)
            poCoordBlock = *ppoCoordBlock;
        else
            poCoordBlock = poMapFile->GetCurCoordBlock();
        poCoordBlock->StartNewFeature();
        const GInt32 nCoordBlockPtr = poCoordBlock->GetCurAddress();
        poCoordBlock->SetComprCoordOrigin(m_nComprOrgX, m_nComprOrgY);

        int nStatus = 0;
        for( int i = 0; nStatus == 0 && i<numPoints; i++ )
        {
            poMapFile->Coordsys2Int(poLine->getX(i), poLine->getY(i), nX, nY);
            if ((nStatus = poCoordBlock->WriteIntCoord(nX, nY,
                                                       bCompressed)) != 0)
            {
                // Failed ... error message has already been produced
                return nStatus;
            }
        }

        const GUInt32 nCoordDataSize = poCoordBlock->GetFeatureDataSize();

        /*-------------------------------------------------------------
         * Copy info to poObjHdr
         *------------------------------------------------------------*/
        TABMAPObjPLine *poPLineHdr = cpl::down_cast<TABMAPObjPLine *>(poObjHdr);

        poPLineHdr->m_nCoordBlockPtr = nCoordBlockPtr;
        poPLineHdr->m_nCoordDataSize = nCoordDataSize;
        poPLineHdr->m_numLineSections = 1;

        poPLineHdr->m_bSmooth = m_bSmooth;

        // MBR
        poPLineHdr->SetMBR(m_nXMin, m_nYMin, m_nXMax, m_nYMax);

        // Polyline center/label point
        double dX = 0.0;
        double dY = 0.0;
        if (GetCenter(dX, dY) != -1)
        {
            poMapFile->Coordsys2Int(dX, dY, poPLineHdr->m_nLabelX,
                                    poPLineHdr->m_nLabelY);
        }
        else
        {
            poPLineHdr->m_nLabelX = m_nComprOrgX;
            poPLineHdr->m_nLabelY = m_nComprOrgY;
        }

        // Compressed coordinate origin (useful only in compressed case!)
        poPLineHdr->m_nComprOrgX = m_nComprOrgX;
        poPLineHdr->m_nComprOrgY = m_nComprOrgY;

        if (!bCoordBlockDataOnly)
        {
            m_nPenDefIndex = poMapFile->WritePenDef(&m_sPenDef);
            poPLineHdr->m_nPenId = static_cast<GByte>(m_nPenDefIndex);      // Pen index
        }
    }
    else if ((m_nMapInfoType == TAB_GEOM_MULTIPLINE ||
              m_nMapInfoType == TAB_GEOM_MULTIPLINE_C ||
              m_nMapInfoType == TAB_GEOM_V450_MULTIPLINE ||
              m_nMapInfoType == TAB_GEOM_V450_MULTIPLINE_C ||
              m_nMapInfoType == TAB_GEOM_V800_MULTIPLINE ||
              m_nMapInfoType == TAB_GEOM_V800_MULTIPLINE_C) &&
             poGeom && (wkbFlatten(poGeom->getGeometryType()) == wkbMultiLineString ||
                        wkbFlatten(poGeom->getGeometryType()) == wkbLineString) )
    {
        /*=============================================================
         * PLINE MULTIPLE (or single PLINE with more than 32767 vertices)
         *============================================================*/

        CPLAssert(m_nMapInfoType == TAB_GEOM_MULTIPLINE ||
                  m_nMapInfoType == TAB_GEOM_MULTIPLINE_C ||
                  m_nMapInfoType == TAB_GEOM_V450_MULTIPLINE ||
                  m_nMapInfoType == TAB_GEOM_V450_MULTIPLINE_C ||
                  m_nMapInfoType == TAB_GEOM_V800_MULTIPLINE ||
                  m_nMapInfoType == TAB_GEOM_V800_MULTIPLINE_C);

        int nStatus = 0;
        OGREnvelope sEnvelope;
        GBool bCompressed = poObjHdr->IsCompressedType();

        /*-------------------------------------------------------------
         * Process geometry first...
         *------------------------------------------------------------*/
        if (ppoCoordBlock != nullptr && *ppoCoordBlock != nullptr)
            poCoordBlock = *ppoCoordBlock;
        else
            poCoordBlock = poMapFile->GetCurCoordBlock();
        poCoordBlock->StartNewFeature();
        const GInt32 nCoordBlockPtr = poCoordBlock->GetCurAddress();
        poCoordBlock->SetComprCoordOrigin(m_nComprOrgX, m_nComprOrgY);

        OGRMultiLineString *poMultiLine = nullptr;
        GInt32 numLines = 1;
        if (wkbFlatten(poGeom->getGeometryType()) == wkbMultiLineString)
        {
            poMultiLine = poGeom->toMultiLineString();
            numLines = poMultiLine->getNumGeometries();
        }
        // else
        // {
        //     poMultiLine = NULL;
        //     numLines = 1;
        // }

        /*-------------------------------------------------------------
         * Build and write array of coord sections headers
         *------------------------------------------------------------*/
        TABMAPCoordSecHdr *pasSecHdrs = static_cast<TABMAPCoordSecHdr*>(
            VSI_CALLOC_VERBOSE(numLines, sizeof(TABMAPCoordSecHdr)));
        if( pasSecHdrs == nullptr )
        {
            return -1;
        }

        /*-------------------------------------------------------------
         * In calculation of nDataOffset, we have to take into account that
         * V450 header section uses int32 instead of int16 for numVertices
         * and we add another 2 bytes to align with a 4 bytes boundary.
         *------------------------------------------------------------*/
        int nVersion = TAB_GEOM_GET_VERSION(m_nMapInfoType);

        const int nTotalHdrSizeUncompressed =
            (nVersion >= 450 ? 28 : 24) * numLines;

        GInt32 numPointsTotal = 0;
        for( int iLine = 0; iLine < numLines; iLine++ )
        {
            if (poMultiLine)
                poGeom = poMultiLine->getGeometryRef(iLine);

            if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString)
            {
                poLine = poGeom->toLineString();
                const GInt32 numPoints = poLine->getNumPoints();
                poLine->getEnvelope(&sEnvelope);

                pasSecHdrs[iLine].numVertices = poLine->getNumPoints();
                pasSecHdrs[iLine].numHoles = 0; // It is a line!

                poMapFile->Coordsys2Int(sEnvelope.MinX, sEnvelope.MinY,
                                        pasSecHdrs[iLine].nXMin,
                                        pasSecHdrs[iLine].nYMin);
                poMapFile->Coordsys2Int(sEnvelope.MaxX, sEnvelope.MaxY,
                                        pasSecHdrs[iLine].nXMax,
                                        pasSecHdrs[iLine].nYMax);
                pasSecHdrs[iLine].nDataOffset = nTotalHdrSizeUncompressed +
                                                numPointsTotal*4*2;
                pasSecHdrs[iLine].nVertexOffset = numPointsTotal;

                numPointsTotal += numPoints;
            }
            else
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "TABPolyline: Object contains an invalid Geometry!");
                nStatus = -1;
            }
        }

        if (nStatus == 0)
            nStatus = poCoordBlock->WriteCoordSecHdrs(nVersion, numLines,
                                                      pasSecHdrs, bCompressed);

        CPLFree(pasSecHdrs);
        pasSecHdrs = nullptr;

        if (nStatus != 0)
            return nStatus;  // Error has already been reported.

        /*-------------------------------------------------------------
         * Then write the coordinates themselves...
         *------------------------------------------------------------*/
        for( int iLine = 0; nStatus == 0 && iLine < numLines; iLine++ )
        {
            if( poMultiLine )
                poGeom = poMultiLine->getGeometryRef(iLine);

            if( poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString )
            {
                poLine = poGeom->toLineString();
                GInt32 numPoints = poLine->getNumPoints();

                for( int i = 0; nStatus == 0 && i<numPoints; i++ )
                {
                    poMapFile->Coordsys2Int(poLine->getX(i), poLine->getY(i),
                                            nX, nY);
                    if ((nStatus=poCoordBlock->WriteIntCoord(nX, nY,
                                                           bCompressed)) != 0)
                    {
                        // Failed ... error message has already been produced
                        return nStatus;
                    }
                }
            }
            else
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "TABPolyline: Object contains an invalid Geometry!");
                return -1;
            }
        }

        const GUInt32 nCoordDataSize = poCoordBlock->GetFeatureDataSize();

        /*-------------------------------------------------------------
         * ... and finally copy info to poObjHdr
         *------------------------------------------------------------*/
        TABMAPObjPLine *poPLineHdr = cpl::down_cast<TABMAPObjPLine *>(poObjHdr);

        poPLineHdr->m_nCoordBlockPtr = nCoordBlockPtr;
        poPLineHdr->m_nCoordDataSize = nCoordDataSize;
        poPLineHdr->m_numLineSections = numLines;

        poPLineHdr->m_bSmooth = m_bSmooth;

        // MBR
        poPLineHdr->SetMBR(m_nXMin, m_nYMin, m_nXMax, m_nYMax);

        // Polyline center/label point
        double dX = 0.0;
        double dY = 0.0;
        if (GetCenter(dX, dY) != -1)
        {
            poMapFile->Coordsys2Int(dX, dY, poPLineHdr->m_nLabelX,
                                    poPLineHdr->m_nLabelY);
        }
        else
        {
            poPLineHdr->m_nLabelX = m_nComprOrgX;
            poPLineHdr->m_nLabelY = m_nComprOrgY;
        }

        // Compressed coordinate origin (useful only in compressed case!)
        poPLineHdr->m_nComprOrgX = m_nComprOrgX;
        poPLineHdr->m_nComprOrgY = m_nComprOrgY;

        if (!bCoordBlockDataOnly)
        {
            m_nPenDefIndex = poMapFile->WritePenDef(&m_sPenDef);
            poPLineHdr->m_nPenId = static_cast<GByte>(m_nPenDefIndex);      // Pen index
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABPolyline: Object contains an invalid Geometry!");
        return -1;
    }

    if (CPLGetLastErrorType() == CE_Failure )
        return -1;

    /* Return a ref to coord block so that caller can continue writing
     * after the end of this object (used by index splitting)
     */
    if (ppoCoordBlock)
        *ppoCoordBlock = poCoordBlock;

    return 0;
}

/**********************************************************************
 *                   TABPolyline::GetStyleString() const
 *
 * Return style string for this feature.
 *
 * Style String is built only once during the first call to GetStyleString().
 **********************************************************************/
const char *TABPolyline::GetStyleString() const
{
    if (m_pszStyleString == nullptr)
    {
        m_pszStyleString = CPLStrdup(GetPenStyleString());
    }

    return m_pszStyleString;
}

/**********************************************************************
 *                   TABPolyline::DumpMIF()
 *
 * Dump feature geometry in a format similar to .MIF PLINEs.
 **********************************************************************/
void TABPolyline::DumpMIF(FILE *fpOut /*=NULL*/)
{
    OGRMultiLineString *poMultiLine = nullptr;
    OGRLineString *poLine = nullptr;
    int i, numPoints;

    if (fpOut == nullptr)
        fpOut = stdout;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString)
    {
        /*-------------------------------------------------------------
         * Generate output for simple polyline
         *------------------------------------------------------------*/
        poLine = poGeom->toLineString();
        numPoints = poLine->getNumPoints();
        fprintf(fpOut, "PLINE %d\n", numPoints);
        for(i=0; i<numPoints; i++)
            fprintf(fpOut, "%.15g %.15g\n", poLine->getX(i), poLine->getY(i));
    }
    else if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbMultiLineString)
    {
        /*-------------------------------------------------------------
         * Generate output for multiple polyline
         *------------------------------------------------------------*/
        int iLine, numLines;
        poMultiLine = poGeom->toMultiLineString();
        numLines = poMultiLine->getNumGeometries();
        fprintf(fpOut, "PLINE MULTIPLE %d\n", numLines);
        for(iLine=0; iLine < numLines; iLine++)
        {
            poGeom = poMultiLine->getGeometryRef(iLine);
            if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString)
            {
                poLine = poGeom->toLineString();
                numPoints = poLine->getNumPoints();
                fprintf(fpOut, " %d\n", numPoints);
                for(i=0; i<numPoints; i++)
                    fprintf(fpOut, "%.15g %.15g\n",poLine->getX(i),poLine->getY(i));
            }
            else
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "TABPolyline: Object contains an invalid Geometry!");
                return;
            }
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABPolyline: Missing or Invalid Geometry!");
        return;
    }

    if (m_bCenterIsSet)
        fprintf(fpOut, "Center %.15g %.15g\n", m_dCenterX, m_dCenterY);

    // Finish with PEN/BRUSH/etc. clauses
    DumpPenDef();

    fflush(fpOut);
}

/**********************************************************************
 *                   TABPolyline::GetCenter()
 *
 * Returns the center point of the line.  Compute one if it was not
 * explicitly set:
 *
 * In MapInfo, for a simple or multiple polyline (pline), the center point
 * in the object definition is supposed to be either the center point of
 * the pline or the first section of a multiple pline (if an odd number of
 * points in the pline or first section), or the midway point between the
 * two central points (if an even number of points involved).
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABPolyline::GetCenter( double &dX, double &dY )
{
    if (!m_bCenterIsSet)
    {
        OGRLineString *poLine = nullptr;

        OGRGeometry *poGeom = GetGeometryRef();
        if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString)
        {
            poLine = poGeom->toLineString();
        }
        else if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbMultiLineString)
        {
            OGRMultiLineString *poMultiLine = poGeom->toMultiLineString();
            if (poMultiLine->getNumGeometries() > 0)
                poLine = poMultiLine->getGeometryRef(0)->toLineString();
        }

        if (poLine && poLine->getNumPoints() > 0)
        {
            int i = poLine->getNumPoints()/2;
            if (poLine->getNumPoints() % 2 == 0)
            {
                // Return the midway between the 2 center points
                m_dCenterX = (poLine->getX(i-1) + poLine->getX(i))/2.0;
                m_dCenterY = (poLine->getY(i-1) + poLine->getY(i))/2.0;
            }
            else
            {
                // Return the center point
                m_dCenterX = poLine->getX(i);
                m_dCenterY = poLine->getY(i);
            }
            m_bCenterIsSet = TRUE;
        }
    }

    if (!m_bCenterIsSet)
        return -1;

    dX = m_dCenterX;
    dY = m_dCenterY;
    return 0;
}

/**********************************************************************
 *                   TABPolyline::SetCenter()
 *
 * Set the X,Y coordinates to use as center point for the line.
 **********************************************************************/
void TABPolyline::SetCenter(double dX, double dY)
{
    m_dCenterX = dX;
    m_dCenterY = dY;
    m_bCenterIsSet = TRUE;
}

/**********************************************************************
 *                   TABPolyline::TwoPointLineAsPolyline()
 *
 * Returns the value of m_bWriteTwoPointLineAsPolyline
 **********************************************************************/
GBool TABPolyline::TwoPointLineAsPolyline()
{
    return m_bWriteTwoPointLineAsPolyline;
}

/**********************************************************************
*                   TABPolyline::TwoPointLineAsPolyline()
*
* Sets the value of m_bWriteTwoPointLineAsPolyline
**********************************************************************/
void TABPolyline::TwoPointLineAsPolyline(GBool bTwoPointLineAsPolyline)
{
    m_bWriteTwoPointLineAsPolyline = bTwoPointLineAsPolyline;
}

/*=====================================================================
 *                      class TABRegion
 *====================================================================*/

/**********************************************************************
 *                   TABRegion::TABRegion()
 *
 * Constructor.
 **********************************************************************/
TABRegion::TABRegion(OGRFeatureDefn *poDefnIn) :
    TABFeature(poDefnIn),
    m_bSmooth(FALSE),
    m_bCenterIsSet(FALSE),
    m_dCenterX(0.0),
    m_dCenterY(0.0)
{}

/**********************************************************************
 *                   TABRegion::~TABRegion()
 *
 * Destructor.
 **********************************************************************/
TABRegion::~TABRegion() {}

/**********************************************************************
 *                     TABRegion::CloneTABFeature()
 *
 * Duplicate feature, including stuff specific to each TABFeature type.
 *
 * This method calls the generic TABFeature::CopyTABFeatureBase() and
 * then copies any members specific to its own type.
 **********************************************************************/
TABFeature *TABRegion::CloneTABFeature(OGRFeatureDefn *poNewDefn/*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Alloc new feature and copy the base stuff
     *----------------------------------------------------------------*/
    TABRegion *poNew = new TABRegion(poNewDefn ? poNewDefn : GetDefnRef());

    CopyTABFeatureBase(poNew);

    /*-----------------------------------------------------------------
     * And members specific to this class
     *----------------------------------------------------------------*/
    // ITABFeaturePen
    *(poNew->GetPenDefRef()) = *GetPenDefRef();

    // ITABFeatureBrush
    *(poNew->GetBrushDefRef()) = *GetBrushDefRef();

    poNew->m_bSmooth = m_bSmooth;
    poNew->m_bCenterIsSet = m_bCenterIsSet;
    poNew->m_dCenterX = m_dCenterX;
    poNew->m_dCenterY = m_dCenterY;

    return poNew;
}

/**********************************************************************
 *                   TABRegion::ValidateMapInfoType()
 *
 * Check the feature's geometry part and return the corresponding
 * mapinfo object type code.  The m_nMapInfoType member will also
 * be updated for further calls to GetMapInfoType();
 *
 * Returns TAB_GEOM_NONE if the geometry is not compatible with what
 * is expected for this object class.
 **********************************************************************/
TABGeomType TABRegion::ValidateMapInfoType(TABMAPFile *poMapFile /*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    if (poGeom && (wkbFlatten(poGeom->getGeometryType()) == wkbPolygon ||
                   wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon))
    {
        GInt32 numPointsTotal = 0;
        GInt32 numRings = GetNumRings();
        for(int i=0; i<numRings; i++)
        {
            OGRLinearRing *poRing = GetRingRef(i);
            if (poRing)
                numPointsTotal += poRing->getNumPoints();
        }
        if ( TAB_REGION_PLINE_REQUIRES_V800(numRings, numPointsTotal) )
            m_nMapInfoType = TAB_GEOM_V800_REGION;
         else if (numPointsTotal > TAB_REGION_PLINE_300_MAX_VERTICES)
            m_nMapInfoType = TAB_GEOM_V450_REGION;
        else
            m_nMapInfoType = TAB_GEOM_REGION;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABRegion: Missing or Invalid Geometry!");
        m_nMapInfoType = TAB_GEOM_NONE;
    }

    /*-----------------------------------------------------------------
     * Decide if coordinates should be compressed or not.
     *----------------------------------------------------------------*/
    ValidateCoordType(poMapFile);

    return m_nMapInfoType;
}

/**********************************************************************
 *                   TABRegion::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABRegion::ReadGeometryFromMAPFile(TABMAPFile *poMapFile,
                                       TABMAPObjHdr *poObjHdr,
                                       GBool bCoordBlockDataOnly /*=FALSE*/,
                                       TABMAPCoordBlock **ppoCoordBlock/*=NULL*/)
{
    double dXMin = 0.0;
    double dYMin = 0.0;
    double dXMax = 0.0;
    double dYMax = 0.0;
    OGRGeometry *poGeometry = nullptr;
    TABMAPCoordBlock *poCoordBlock = nullptr;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poObjHdr->m_nType;

    if (m_nMapInfoType == TAB_GEOM_REGION ||
        m_nMapInfoType == TAB_GEOM_REGION_C ||
        m_nMapInfoType == TAB_GEOM_V450_REGION ||
        m_nMapInfoType == TAB_GEOM_V450_REGION_C ||
        m_nMapInfoType == TAB_GEOM_V800_REGION ||
        m_nMapInfoType == TAB_GEOM_V800_REGION_C )
    {
        /*=============================================================
         * REGION (Similar to PLINE MULTIPLE)
         *============================================================*/
        GInt32 /* nCoordDataSize, */ numPointsTotal;
        OGRMultiPolygon *poMultiPolygon = nullptr;
        OGRPolygon *poPolygon = nullptr;
        GBool bComprCoord = poObjHdr->IsCompressedType();
        int nVersion = TAB_GEOM_GET_VERSION(m_nMapInfoType);

        /*-------------------------------------------------------------
         * Copy data from poObjHdr
         *------------------------------------------------------------*/
        TABMAPObjPLine *poPLineHdr = cpl::down_cast<TABMAPObjPLine *>(poObjHdr);

        GInt32 nCoordBlockPtr = poPLineHdr->m_nCoordBlockPtr;
        /* nCoordDataSize  = poPLineHdr->m_nCoordDataSize; */
        GInt32 numLineSections = poPLineHdr->m_numLineSections;
        m_bSmooth       = poPLineHdr->m_bSmooth;

        // Centroid/label point
        double dX = 0.0;
        double dY = 0.0;
        poMapFile->Int2Coordsys(poPLineHdr->m_nLabelX, poPLineHdr->m_nLabelY,
                                dX, dY);
        SetCenter(dX, dY);

        // Compressed coordinate origin (useful only in compressed case!)
        m_nComprOrgX = poPLineHdr->m_nComprOrgX;
        m_nComprOrgY = poPLineHdr->m_nComprOrgY;

        // MBR
        poMapFile->Int2Coordsys(poPLineHdr->m_nMinX, poPLineHdr->m_nMinY,
                                dXMin, dYMin);
        poMapFile->Int2Coordsys(poPLineHdr->m_nMaxX, poPLineHdr->m_nMaxY,
                                dXMax, dYMax);

        if (!bCoordBlockDataOnly)
        {
            m_nPenDefIndex = poPLineHdr->m_nPenId;  // Pen index
            poMapFile->ReadPenDef(m_nPenDefIndex, &m_sPenDef);
            m_nBrushDefIndex = poPLineHdr->m_nBrushId;  // Brush index
            poMapFile->ReadBrushDef(m_nBrushDefIndex, &m_sBrushDef);
        }

        /*-------------------------------------------------------------
         * Read data from the coord. block
         *------------------------------------------------------------*/

        const int nMinSizeOfSection = 24;
        if( numLineSections > INT_MAX / nMinSizeOfSection )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Too many numLineSections");
            return -1;
        }
        const GUInt32 nMinimumBytesForSections =
                                nMinSizeOfSection * numLineSections;
        if( nMinimumBytesForSections > 1024 * 1024 &&
            nMinimumBytesForSections > poMapFile->GetFileSize() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Too many numLineSections");
            return -1;
        }

        TABMAPCoordSecHdr *pasSecHdrs = static_cast<TABMAPCoordSecHdr *>(
            VSI_MALLOC2_VERBOSE(numLineSections, sizeof(TABMAPCoordSecHdr)));
        if( pasSecHdrs == nullptr )
            return -1;

        if (ppoCoordBlock != nullptr && *ppoCoordBlock != nullptr)
            poCoordBlock = *ppoCoordBlock;
        else
            poCoordBlock = poMapFile->GetCoordBlock(nCoordBlockPtr);

        if (poCoordBlock)
            poCoordBlock->SetComprCoordOrigin(m_nComprOrgX, m_nComprOrgY);

        if (poCoordBlock == nullptr ||
            poCoordBlock->ReadCoordSecHdrs(bComprCoord, nVersion,
                                           numLineSections,
                                           pasSecHdrs, numPointsTotal) != 0)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed reading coordinate data at offset %d",
                     nCoordBlockPtr);
            CPLFree(pasSecHdrs);
            return -1;
        }

        const GUInt32 nMinimumBytesForPoints =
                        (bComprCoord ? 4 : 8) * numPointsTotal;
        if( nMinimumBytesForPoints > 1024 * 1024 &&
            nMinimumBytesForPoints > poMapFile->GetFileSize() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Too many numPointsTotal");
            CPLFree(pasSecHdrs);
            return -1;
        }

        GInt32 *panXY = static_cast<GInt32 *>(
            VSI_MALLOC2_VERBOSE(numPointsTotal, 2 * sizeof(GInt32)));
        if( panXY == nullptr )
        {
            CPLFree(pasSecHdrs);
            return -1;
        }

        if (poCoordBlock->ReadIntCoords(bComprCoord,numPointsTotal,panXY) != 0)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed reading coordinate data at offset %d",
                     nCoordBlockPtr);
            CPLFree(pasSecHdrs);
            CPLFree(panXY);
            return -1;
        }

        /*-------------------------------------------------------------
         * Decide if we should return an OGRPolygon or an OGRMultiPolygon
         * depending on the number of outer rings found in CoordSecHdr blocks.
         * The CoodSecHdr block for each outer ring in the region has a flag
         * indicating the number of inner rings that follow.
         * In older versions of the format, the count of inner rings was
         * always zero, so in this case we would always return MultiPolygons.
         *
         * Note: The current implementation assumes that there cannot be
         * holes inside holes (i.e. multiple levels of inner rings)... if
         * that case was encountered then we would return an OGRMultiPolygon
         * in which the topological relationship between the rings would
         * be lost.
         *------------------------------------------------------------*/
        int numOuterRings = 0;
        for( int iSection = 0; iSection < numLineSections; iSection++ )
        {
            // Count this as an outer ring.
            numOuterRings++;
            // Skip inner rings... so loop continues on an outer ring.
            iSection += pasSecHdrs[iSection].numHoles;
        }

        if (numOuterRings > 1)
        {
            poMultiPolygon = new OGRMultiPolygon;
            poGeometry = poMultiPolygon;
        }
        else
        {
            poGeometry = nullptr;  // Will be set later
        }

        /*-------------------------------------------------------------
         * OK, build the OGRGeometry object.
         *------------------------------------------------------------*/
        int numHolesToRead = 0;
        poPolygon = nullptr;
        for( int iSection = 0; iSection < numLineSections; iSection++ )
        {

            if (poPolygon == nullptr)
                poPolygon = new OGRPolygon();

            if (numHolesToRead < 1)
                numHolesToRead = pasSecHdrs[iSection].numHoles;
            else
                numHolesToRead--;

            int numSectionVertices = pasSecHdrs[iSection].numVertices;
            GInt32 *pnXYPtr = panXY + (pasSecHdrs[iSection].nVertexOffset * 2);

            OGRLinearRing *poRing = new OGRLinearRing();
            poRing->setNumPoints(numSectionVertices);

            for( int i = 0; i < numSectionVertices; i++ )
            {
                poMapFile->Int2Coordsys(*pnXYPtr, *(pnXYPtr+1), dX, dY);
                poRing->setPoint(i, dX, dY);
                pnXYPtr += 2;
            }

            poPolygon->addRingDirectly(poRing);
            poRing = nullptr;

            if (numHolesToRead < 1)
            {
                if (numOuterRings > 1)
                {
                    poMultiPolygon->addGeometryDirectly(poPolygon);
                }
                else
                {
                    poGeometry = poPolygon;
                    CPLAssert(iSection == numLineSections-1);
                }

                poPolygon = nullptr;  // We'll alloc a new polygon next loop.
            }
        }
        delete poPolygon; // should only trigger on corrupted files

        CPLFree(pasSecHdrs);
        CPLFree(panXY);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
           "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
                 m_nMapInfoType, m_nMapInfoType);
        return -1;
    }

    SetGeometryDirectly(poGeometry);

    SetMBR(dXMin, dYMin, dXMax, dYMax);
    SetIntMBR(poObjHdr->m_nMinX, poObjHdr->m_nMinY,
              poObjHdr->m_nMaxX, poObjHdr->m_nMaxY);

    /* Return a ref to coord block so that caller can continue reading
     * after the end of this object (used by TABCollection and index splitting)
     */
    if (ppoCoordBlock)
        *ppoCoordBlock = poCoordBlock;

    return 0;
}

/**********************************************************************
 *                   TABRegion::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABRegion::WriteGeometryToMAPFile(TABMAPFile *poMapFile,
                                      TABMAPObjHdr *poObjHdr,
                                      GBool bCoordBlockDataOnly /*=FALSE*/,
                                      TABMAPCoordBlock **ppoCoordBlock/*=NULL*/)
{
    /*-----------------------------------------------------------------
     * We assume that ValidateMapInfoType() was called already and that
     * the type in poObjHdr->m_nType is valid.
     *----------------------------------------------------------------*/
    CPLAssert(m_nMapInfoType == poObjHdr->m_nType);

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    TABMAPCoordBlock *poCoordBlock=nullptr;

    if ((m_nMapInfoType == TAB_GEOM_REGION ||
         m_nMapInfoType == TAB_GEOM_REGION_C ||
         m_nMapInfoType == TAB_GEOM_V450_REGION ||
         m_nMapInfoType == TAB_GEOM_V450_REGION_C ||
         m_nMapInfoType == TAB_GEOM_V800_REGION ||
         m_nMapInfoType == TAB_GEOM_V800_REGION_C) &&
        poGeom && (wkbFlatten(poGeom->getGeometryType()) == wkbPolygon ||
                   wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon))
    {
        /*=============================================================
         * REGIONs are similar to PLINE MULTIPLE
         *
         * We accept both OGRPolygons (with one or multiple rings) and
         * OGRMultiPolygons as input.
         *============================================================*/
        GBool   bCompressed = poObjHdr->IsCompressedType();

        /*-------------------------------------------------------------
         * Process geometry first...
         *------------------------------------------------------------*/
        if (ppoCoordBlock != nullptr && *ppoCoordBlock != nullptr)
            poCoordBlock = *ppoCoordBlock;
        else
            poCoordBlock = poMapFile->GetCurCoordBlock();
        poCoordBlock->StartNewFeature();
        GInt32 nCoordBlockPtr = poCoordBlock->GetCurAddress();
        poCoordBlock->SetComprCoordOrigin(m_nComprOrgX, m_nComprOrgY);

#ifdef TABDUMP
    printf("TABRegion::WriteGeometryToMAPFile(): ComprOrgX,Y= (%d,%d)\n",/*ok*/
           m_nComprOrgX, m_nComprOrgY);
#endif
        /*-------------------------------------------------------------
         * Fetch total number of rings and build array of coord
         * sections headers.
         *------------------------------------------------------------*/
        TABMAPCoordSecHdr *pasSecHdrs = nullptr;
        int numRingsTotal = ComputeNumRings(&pasSecHdrs, poMapFile);
        int nStatus = numRingsTotal == 0 ? -1 : 0;

        /*-------------------------------------------------------------
         * Write the Coord. Section Header
         *------------------------------------------------------------*/
        const int nVersion = TAB_GEOM_GET_VERSION(m_nMapInfoType);

        if( nStatus == 0 )
            nStatus = poCoordBlock->WriteCoordSecHdrs(nVersion, numRingsTotal,
                                                      pasSecHdrs, bCompressed);

        CPLFree(pasSecHdrs);
        pasSecHdrs = nullptr;

        if( nStatus != 0 )
            return nStatus;  // Error has already been reported.

        /*-------------------------------------------------------------
         * Go through all the rings in our OGRMultiPolygon or OGRPolygon
         * to write the coordinates themselves...
         *------------------------------------------------------------*/

        GInt32 nX, nY;
        for( int iRing=0; iRing < numRingsTotal; iRing++ )
        {
            OGRLinearRing *poRing = GetRingRef(iRing);
            if (poRing == nullptr)
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "TABRegion: Object Geometry contains NULL rings!");
                return -1;
            }

            int numPoints = poRing->getNumPoints();

            for( int i=0; nStatus == 0 && i<numPoints; i++ )
            {
                poMapFile->Coordsys2Int(poRing->getX(i), poRing->getY(i),
                                        nX, nY);
                if ((nStatus=poCoordBlock->WriteIntCoord(nX, nY,
                                                         bCompressed)) != 0)
                {
                    // Failed ... error message has already been produced
                    return nStatus;
                }
            }
        }/* for iRing*/

        GUInt32 nCoordDataSize = poCoordBlock->GetFeatureDataSize();

        /*-------------------------------------------------------------
         * ... and finally copy info to poObjHdr
         *------------------------------------------------------------*/
        TABMAPObjPLine *poPLineHdr = cpl::down_cast<TABMAPObjPLine *>(poObjHdr);

        poPLineHdr->m_nCoordBlockPtr = nCoordBlockPtr;
        poPLineHdr->m_nCoordDataSize = nCoordDataSize;
        poPLineHdr->m_numLineSections = numRingsTotal;

        poPLineHdr->m_bSmooth = m_bSmooth;

        // MBR
        poPLineHdr->SetMBR(m_nXMin, m_nYMin, m_nXMax, m_nYMax);

        // Region center/label point
        double dX = 0.0;
        double dY = 0.0;
        if (GetCenter(dX, dY) != -1)
        {
            poMapFile->Coordsys2Int(dX, dY, poPLineHdr->m_nLabelX,
                                    poPLineHdr->m_nLabelY);
        }
        else
        {
            poPLineHdr->m_nLabelX = m_nComprOrgX;
            poPLineHdr->m_nLabelY = m_nComprOrgY;
        }

        // Compressed coordinate origin (useful only in compressed case!)
        poPLineHdr->m_nComprOrgX = m_nComprOrgX;
        poPLineHdr->m_nComprOrgY = m_nComprOrgY;

        if (!bCoordBlockDataOnly)
        {
            m_nPenDefIndex = poMapFile->WritePenDef(&m_sPenDef);
            poPLineHdr->m_nPenId = static_cast<GByte>(m_nPenDefIndex);      // Pen index

            m_nBrushDefIndex = poMapFile->WriteBrushDef(&m_sBrushDef);
            poPLineHdr->m_nBrushId = static_cast<GByte>(m_nBrushDefIndex);  // Brush index
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABRegion: Object contains an invalid Geometry!");
        return -1;
    }

    if (CPLGetLastErrorType() == CE_Failure)
        return -1;

    /* Return a ref to coord block so that caller can continue writing
     * after the end of this object (used by index splitting)
     */
    if (ppoCoordBlock)
        *ppoCoordBlock = poCoordBlock;

    return 0;
}

/**********************************************************************
 *                   TABRegion::GetNumRings()
 *
 * Return the total number of rings in this object making it look like
 * all parts of the OGRMultiPolygon (or OGRPolygon) are a single collection
 * of rings... hides the complexity of handling OGRMultiPolygons vs
 * OGRPolygons, etc.
 *
 * Returns 0 if the geometry contained in the object is invalid or missing.
 **********************************************************************/
int TABRegion::GetNumRings()
{
    return ComputeNumRings(nullptr, nullptr);
}

int TABRegion::ComputeNumRings(TABMAPCoordSecHdr **ppasSecHdrs,
                               TABMAPFile *poMapFile)
{
    int numRingsTotal = 0;
    int iLastSect = 0;

    if (ppasSecHdrs)
        *ppasSecHdrs = nullptr;

    OGRGeometry *poGeom = GetGeometryRef();

    if (poGeom && (wkbFlatten(poGeom->getGeometryType()) == wkbPolygon ||
                   wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon))
    {
        /*-------------------------------------------------------------
         * Calculate total number of rings...
         *------------------------------------------------------------*/
        if (wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon)
        {
            for( auto&& poPolygon: *(poGeom->toMultiPolygon()) )
            {
                numRingsTotal += poPolygon->getNumInteriorRings()+1;

                if (ppasSecHdrs && poMapFile)
                {
                    if (AppendSecHdrs(poPolygon, *ppasSecHdrs,
                                      poMapFile, iLastSect) != 0)
                        return 0; // An error happened, return count=0
                }
            }  // for
        }
        else
        {
            OGRPolygon* poPolygon = poGeom->toPolygon();
            numRingsTotal = poPolygon->getNumInteriorRings()+1;

            if (ppasSecHdrs && poMapFile)
            {
                if (AppendSecHdrs(poPolygon, *ppasSecHdrs,
                                  poMapFile, iLastSect) != 0)
                    return 0;  // An error happened, return count=0
            }
        }
    }

    /*-----------------------------------------------------------------
     * If we're generating section header blocks, then init the
     * coordinate offset values.
     *
     * In calculation of nDataOffset, we have to take into account that
     * V450 header section uses int32 instead of int16 for numVertices
     * and we add another 2 bytes to align with a 4 bytes boundary.
     *------------------------------------------------------------*/
    const int nTotalHdrSizeUncompressed =
        (m_nMapInfoType == TAB_GEOM_V450_REGION ||
         m_nMapInfoType == TAB_GEOM_V450_REGION_C ||
         m_nMapInfoType == TAB_GEOM_V800_REGION ||
         m_nMapInfoType == TAB_GEOM_V800_REGION_C)
        ? 28 * numRingsTotal
        : 24 * numRingsTotal;

    if( ppasSecHdrs )
    {
        int numPointsTotal = 0;
        CPLAssert(iLastSect == numRingsTotal);
        for( int iRing=0; iRing < numRingsTotal; iRing++ )
        {
            (*ppasSecHdrs)[iRing].nDataOffset = nTotalHdrSizeUncompressed +
                                                   numPointsTotal*4*2;
            (*ppasSecHdrs)[iRing].nVertexOffset = numPointsTotal;

            numPointsTotal += (*ppasSecHdrs)[iRing].numVertices;
        }
    }

    return numRingsTotal;
}

/**********************************************************************
 *                   TABRegion::AppendSecHdrs()
 *
 * (Private method)
 *
 * Add a TABMAPCoordSecHdr for each ring in the specified polygon.
 **********************************************************************/
int TABRegion::AppendSecHdrs(OGRPolygon *poPolygon,
                             TABMAPCoordSecHdr * &pasSecHdrs,
                             TABMAPFile *poMapFile,
                             int &iLastRing)
{
    /*-------------------------------------------------------------
     * Add a pasSecHdrs[] entry for each ring in this polygon.
     * Note that the structs won't be fully initialized.
     *------------------------------------------------------------*/
    int numRingsInPolygon = poPolygon->getNumInteriorRings()+1;

    pasSecHdrs = static_cast<TABMAPCoordSecHdr*>(CPLRealloc(pasSecHdrs,
                                                (iLastRing+numRingsInPolygon)*
                                                sizeof(TABMAPCoordSecHdr)));

    for( int iRing=0; iRing < numRingsInPolygon; iRing++ )
    {
        OGRLinearRing *poRing = nullptr;
        OGREnvelope     sEnvelope;

        if (iRing == 0)
            poRing = poPolygon->getExteriorRing();
        else
            poRing = poPolygon->getInteriorRing(iRing-1);

        if (poRing == nullptr)
        {
            CPLError(CE_Failure, CPLE_AssertionFailed,
                     "Assertion Failed: Encountered NULL ring in OGRPolygon");
            return -1;
        }

        poRing->getEnvelope(&sEnvelope);

        pasSecHdrs[iLastRing].numVertices = poRing->getNumPoints();

        if (iRing == 0)
            pasSecHdrs[iLastRing].numHoles = numRingsInPolygon-1;
        else
            pasSecHdrs[iLastRing].numHoles = 0;

        poMapFile->Coordsys2Int(sEnvelope.MinX, sEnvelope.MinY,
                                pasSecHdrs[iLastRing].nXMin,
                                pasSecHdrs[iLastRing].nYMin);
        poMapFile->Coordsys2Int(sEnvelope.MaxX, sEnvelope.MaxY,
                                pasSecHdrs[iLastRing].nXMax,
                                pasSecHdrs[iLastRing].nYMax);

        iLastRing++;
    }/* for iRing*/

    return 0;
}

/**********************************************************************
 *                   TABRegion::GetRingRef()
 *
 * Returns a reference to the specified ring number making it look like
 * all parts of the OGRMultiPolygon (or OGRPolygon) are a single collection
 * of rings... hides the complexity of handling OGRMultiPolygons vs
 * OGRPolygons, etc.
 *
 * Returns NULL if the geometry contained in the object is invalid or
 * missing or if the specified ring index is invalid.
 **********************************************************************/
OGRLinearRing *TABRegion::GetRingRef(int nRequestedRingIndex)
{
    OGRLinearRing   *poRing = nullptr;

    OGRGeometry *poGeom = GetGeometryRef();

    if (poGeom && (wkbFlatten(poGeom->getGeometryType()) == wkbPolygon ||
                   wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon))
    {
        /*-------------------------------------------------------------
         * Establish number of polygons based on geometry type
         *------------------------------------------------------------*/
        OGRMultiPolygon *poMultiPolygon = nullptr;
        int             iCurRing = 0;
        int             numOGRPolygons = 0;

        if (wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon)
        {
            poMultiPolygon = poGeom->toMultiPolygon();
            numOGRPolygons = poMultiPolygon->getNumGeometries();
        }
        else
        {
            numOGRPolygons = 1;
        }

        /*-------------------------------------------------------------
         * Loop through polygons until we find the requested ring.
         *------------------------------------------------------------*/
        iCurRing = 0;
        for(int iPoly=0; poRing == nullptr && iPoly < numOGRPolygons; iPoly++)
        {
            OGRPolygon *poPolygon = nullptr;
            if (poMultiPolygon)
                poPolygon = poMultiPolygon->getGeometryRef(iPoly)->toPolygon();
            else
                poPolygon = poGeom->toPolygon();

            int numIntRings = poPolygon->getNumInteriorRings();

            if (iCurRing == nRequestedRingIndex)
            {
                poRing = poPolygon->getExteriorRing();
            }
            else if (nRequestedRingIndex > iCurRing &&
                     nRequestedRingIndex-(iCurRing+1) < numIntRings)
           {
                poRing = poPolygon->getInteriorRing(nRequestedRingIndex-
                                                                (iCurRing+1) );
            }
            iCurRing += numIntRings+1;
        }
    }

    return poRing;
}

/**********************************************************************
 *                   TABRegion::RingIsHole()
 *
 * Return false if the requested ring index is the first of a polygon
 **********************************************************************/
GBool TABRegion::IsInteriorRing(int nRequestedRingIndex)
{
    OGRGeometry *poGeom = GetGeometryRef();

    if (poGeom && (wkbFlatten(poGeom->getGeometryType()) == wkbPolygon ||
                   wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon))
    {
        /*-------------------------------------------------------------
         * Establish number of polygons based on geometry type
         *------------------------------------------------------------*/
        OGRMultiPolygon *poMultiPolygon = nullptr;
        int             iCurRing = 0;
        int             numOGRPolygons = 0;

        if (wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon)
        {
            poMultiPolygon = poGeom->toMultiPolygon();
            numOGRPolygons = poMultiPolygon->getNumGeometries();
        }
        else
        {
            numOGRPolygons = 1;
        }

        /*-------------------------------------------------------------
         * Loop through polygons until we find the requested ring.
         *------------------------------------------------------------*/
        iCurRing = 0;
        for(int iPoly=0; iPoly < numOGRPolygons; iPoly++)
        {
            OGRPolygon* poPolygon = nullptr;
            if (poMultiPolygon)
                poPolygon = poMultiPolygon->getGeometryRef(iPoly)->toPolygon();
            else
                poPolygon = poGeom->toPolygon();

            int numIntRings = poPolygon->getNumInteriorRings();

            if (iCurRing == nRequestedRingIndex)
            {
                return FALSE;
            }
            else if (nRequestedRingIndex > iCurRing &&
                     nRequestedRingIndex-(iCurRing+1) < numIntRings)
           {
                return TRUE;
            }
            iCurRing += numIntRings+1;
        }
    }

    return FALSE;
}

/**********************************************************************
 *                   TABRegion::GetStyleString() const
 *
 * Return style string for this feature.
 *
 * Style String is built only once during the first call to GetStyleString().
 **********************************************************************/
const char *TABRegion::GetStyleString() const
{
    if (m_pszStyleString == nullptr)
    {
        // Since GetPen/BrushStyleString() use CPLSPrintf(), we need
        // to use temporary buffers
        char *pszPen = CPLStrdup(GetPenStyleString());
        char *pszBrush = CPLStrdup(GetBrushStyleString());

        m_pszStyleString = CPLStrdup(CPLSPrintf("%s;%s", pszBrush, pszPen));

        CPLFree(pszPen);
        CPLFree(pszBrush);
    }

    return m_pszStyleString;
}

/**********************************************************************
 *                   TABRegion::DumpMIF()
 *
 * Dump feature geometry in a format similar to .MIF REGIONs.
 **********************************************************************/
void TABRegion::DumpMIF(FILE *fpOut /*=NULL*/)
{
    if (fpOut == nullptr)
        fpOut = stdout;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    if (poGeom && (wkbFlatten(poGeom->getGeometryType()) == wkbPolygon ||
                   wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon))
    {
        /*-------------------------------------------------------------
         * Generate output for region
         *
         * Note that we want to handle both OGRPolygons and OGRMultiPolygons
         * that's why we use the GetNumRings()/GetRingRef() interface.
         *------------------------------------------------------------*/
        int numRingsTotal = GetNumRings();

        fprintf(fpOut, "REGION %d\n", numRingsTotal);

        for( int iRing = 0; iRing < numRingsTotal; iRing++ )
        {
            OGRLinearRing *poRing = GetRingRef(iRing);

            if (poRing == nullptr)
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "TABRegion: Object Geometry contains NULL rings!");
                return;
            }

            const int numPoints = poRing->getNumPoints();
            fprintf(fpOut, " %d\n", numPoints);
            for( int i = 0;  i < numPoints; i++ )
                fprintf(fpOut, "%.15g %.15g\n",poRing->getX(i),poRing->getY(i));
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABRegion: Missing or Invalid Geometry!");
        return;
    }

    if (m_bCenterIsSet)
        fprintf(fpOut, "Center %.15g %.15g\n", m_dCenterX, m_dCenterY);

    // Finish with PEN/BRUSH/etc. clauses
    DumpPenDef();
    DumpBrushDef();

    fflush(fpOut);
}

/**********************************************************************
 *                   TABRegion::GetCenter()
 *
 * Returns the center/label point of the region.
 * Compute one using OGRPolygonLabelPoint() if it was not explicitly set
 * before.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABRegion::GetCenter( double &dX, double &dY )
{
    if (!m_bCenterIsSet)
    {
        /*-------------------------------------------------------------
         * Calculate label point.  If we have a multipolygon then we use
         * the first OGRPolygon in the feature to calculate the point.
         *------------------------------------------------------------*/
        OGRGeometry *poGeom = GetGeometryRef();
        if (poGeom == nullptr)
            return -1;

        OGRPolygon *poPolygon=nullptr;

        if (wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon)
        {
            OGRMultiPolygon *poMultiPolygon = poGeom->toMultiPolygon();
            if (poMultiPolygon->getNumGeometries() > 0)
                poPolygon = poMultiPolygon->getGeometryRef(0)->toPolygon();
        }
        else if (wkbFlatten(poGeom->getGeometryType()) == wkbPolygon)
        {
            poPolygon = poGeom->toPolygon();
        }

        OGRPoint oLabelPoint;
        if (poPolygon != nullptr &&
            OGRPolygonLabelPoint(poPolygon, &oLabelPoint) == OGRERR_NONE)
        {
            m_dCenterX = oLabelPoint.getX();
            m_dCenterY = oLabelPoint.getY();
        }
        else
        {
            OGREnvelope oEnv;
            poGeom->getEnvelope(&oEnv);
            m_dCenterX = (oEnv.MaxX + oEnv.MinX)/2.0;
            m_dCenterY = (oEnv.MaxY + oEnv.MinY)/2.0;
        }

        m_bCenterIsSet = TRUE;
    }

    if (!m_bCenterIsSet)
        return -1;

    dX = m_dCenterX;
    dY = m_dCenterY;
    return 0;
}

/**********************************************************************
 *                   TABRegion::SetCenter()
 *
 * Set the X,Y coordinates to use as center/label point for the region.
 **********************************************************************/
void TABRegion::SetCenter(double dX, double dY)
{
    m_dCenterX = dX;
    m_dCenterY = dY;
    m_bCenterIsSet = TRUE;
}

/*=====================================================================
 *                      class TABRectangle
 *====================================================================*/

/**********************************************************************
 *                   TABRectangle::TABRectangle()
 *
 * Constructor.
 **********************************************************************/
TABRectangle::TABRectangle( OGRFeatureDefn *poDefnIn ) :
    TABFeature(poDefnIn),
    m_bRoundCorners(FALSE),
    m_dRoundXRadius(0.0),
    m_dRoundYRadius(0.0)
{}

/**********************************************************************
 *                   TABRectangle::~TABRectangle()
 *
 * Destructor.
 **********************************************************************/
TABRectangle::~TABRectangle() {}

/**********************************************************************
 *                     TABRectangle::CloneTABFeature()
 *
 * Duplicate feature, including stuff specific to each TABFeature type.
 *
 * This method calls the generic TABFeature::CopyTABFeatureBase() and
 * then copies any members specific to its own type.
 **********************************************************************/
TABFeature *TABRectangle::CloneTABFeature(OGRFeatureDefn *poNewDefn/*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Alloc new feature and copy the base stuff
     *----------------------------------------------------------------*/
    TABRectangle *poNew = new TABRectangle(poNewDefn ? poNewDefn :
                                                       GetDefnRef());

    CopyTABFeatureBase(poNew);

    /*-----------------------------------------------------------------
     * And members specific to this class
     *----------------------------------------------------------------*/
    // ITABFeaturePen
    *(poNew->GetPenDefRef()) = *GetPenDefRef();

    // ITABFeatureBrush
    *(poNew->GetBrushDefRef()) = *GetBrushDefRef();

    poNew->m_bRoundCorners = m_bRoundCorners;
    poNew->m_dRoundXRadius = m_dRoundXRadius;
    poNew->m_dRoundYRadius = m_dRoundYRadius;

    return poNew;
}

/**********************************************************************
 *                   TABRectangle::ValidateMapInfoType()
 *
 * Check the feature's geometry part and return the corresponding
 * mapinfo object type code.  The m_nMapInfoType member will also
 * be updated for further calls to GetMapInfoType();
 *
 * Returns TAB_GEOM_NONE if the geometry is not compatible with what
 * is expected for this object class.
 **********************************************************************/
TABGeomType TABRectangle::ValidateMapInfoType(TABMAPFile *poMapFile /*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPolygon)
    {
        if (m_bRoundCorners && m_dRoundXRadius!=0.0 && m_dRoundYRadius!=0.0)
            m_nMapInfoType = TAB_GEOM_ROUNDRECT;
        else
            m_nMapInfoType = TAB_GEOM_RECT;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABRectangle: Missing or Invalid Geometry!");
        m_nMapInfoType = TAB_GEOM_NONE;
    }

    /*-----------------------------------------------------------------
     * Decide if coordinates should be compressed or not.
     *----------------------------------------------------------------*/
    // __TODO__ For now we always write uncompressed for this class...
    // ValidateCoordType(poMapFile);
    UpdateMBR(poMapFile);

    return m_nMapInfoType;
}

/**********************************************************************
 *                   TABRectangle::UpdateMBR()
 *
 * Update the feature MBR members using the geometry
 *
 * Returns 0 on success, or -1 if there is no geometry in object
 **********************************************************************/
int TABRectangle::UpdateMBR(TABMAPFile * poMapFile /*=NULL*/)
{
    OGREnvelope sEnvelope;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPolygon)
        poGeom->getEnvelope(&sEnvelope);
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABRectangle: Missing or Invalid Geometry!");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Note that we will simply use the rectangle's MBR and don't really
     * read the polygon geometry... this should be OK unless the
     * polygon geometry was not really a rectangle.
     *----------------------------------------------------------------*/
    m_dXMin = sEnvelope.MinX;
    m_dYMin = sEnvelope.MinY;
    m_dXMax = sEnvelope.MaxX;
    m_dYMax = sEnvelope.MaxY;

    if (poMapFile)
    {
        poMapFile->Coordsys2Int(m_dXMin, m_dYMin, m_nXMin, m_nYMin);
        poMapFile->Coordsys2Int(m_dXMax, m_dYMax, m_nXMax, m_nYMax);
    }

    return 0;
}

/**********************************************************************
 *                   TABRectangle::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABRectangle::ReadGeometryFromMAPFile(TABMAPFile *poMapFile,
                                          TABMAPObjHdr *poObjHdr,
                                          GBool bCoordBlockDataOnly /*=FALSE*/,
                                          TABMAPCoordBlock ** /*ppoCoordBlock=NULL*/)
{
    /* Nothing to do for bCoordBlockDataOnly (used by index splitting) */
    if (bCoordBlockDataOnly)
        return 0;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poObjHdr->m_nType;

    if (m_nMapInfoType != TAB_GEOM_RECT &&
        m_nMapInfoType != TAB_GEOM_RECT_C &&
        m_nMapInfoType != TAB_GEOM_ROUNDRECT &&
        m_nMapInfoType != TAB_GEOM_ROUNDRECT_C)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
           "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
                 m_nMapInfoType, m_nMapInfoType);
        return -1;
    }

    /*-----------------------------------------------------------------
     * Read object information
     *----------------------------------------------------------------*/
    TABMAPObjRectEllipse *poRectHdr = cpl::down_cast<TABMAPObjRectEllipse *>(poObjHdr);

    // Read the corners radius

    if (m_nMapInfoType == TAB_GEOM_ROUNDRECT ||
        m_nMapInfoType == TAB_GEOM_ROUNDRECT_C)
    {
        // Read the corner's diameters
        poMapFile->Int2CoordsysDist(poRectHdr->m_nCornerWidth,
                                    poRectHdr->m_nCornerHeight,
                                    m_dRoundXRadius, m_dRoundYRadius);

        // Divide by 2 since we store the corner's radius
        m_dRoundXRadius /= 2.0;
        m_dRoundYRadius /= 2.0;

        m_bRoundCorners = TRUE;
    }
    else
    {
        m_bRoundCorners = FALSE;
        m_dRoundXRadius = 0.0;
        m_dRoundYRadius = 0.0;
    }

    // A rectangle is defined by its MBR

    double dXMin = 0.0;
    double dYMin = 0.0;
    double dXMax = 0.0;
    double dYMax = 0.0;
    poMapFile->Int2Coordsys(poRectHdr->m_nMinX, poRectHdr->m_nMinY,
                            dXMin, dYMin);
    poMapFile->Int2Coordsys(poRectHdr->m_nMaxX, poRectHdr->m_nMaxY,
                            dXMax, dYMax);

    m_nPenDefIndex = poRectHdr->m_nPenId;       // Pen index
    poMapFile->ReadPenDef(m_nPenDefIndex, &m_sPenDef);

    m_nBrushDefIndex = poRectHdr->m_nBrushId;   // Brush index
    poMapFile->ReadBrushDef(m_nBrushDefIndex, &m_sBrushDef);

    /*-----------------------------------------------------------------
     * Call SetMBR() and GetMBR() now to make sure that min values are
     * really smaller than max values.
     *----------------------------------------------------------------*/
    SetMBR(dXMin, dYMin, dXMax, dYMax);
    GetMBR(dXMin, dYMin, dXMax, dYMax);

    /* Copy int MBR to feature class members */
    SetIntMBR(poObjHdr->m_nMinX, poObjHdr->m_nMinY,
              poObjHdr->m_nMaxX, poObjHdr->m_nMaxY);

    /*-----------------------------------------------------------------
     * Create and fill geometry object
     *----------------------------------------------------------------*/
    OGRPolygon *poPolygon = new OGRPolygon;
    OGRLinearRing *poRing = new OGRLinearRing();
    if (m_bRoundCorners && m_dRoundXRadius != 0.0 && m_dRoundYRadius != 0.0)
    {
        /*-------------------------------------------------------------
         * For rounded rectangles, we generate arcs with 45 line
         * segments for each corner.  We start with lower-left corner
         * and proceed counterclockwise
         * We also have to make sure that rounding radius is not too
         * large for the MBR in the generated polygon... however, we
         * always return the true X/Y radius (not adjusted) since this
         * is the way MapInfo seems to do it when a radius bigger than
         * the MBR is passed from TBA to MIF.
         *------------------------------------------------------------*/
        const double dXRadius =
            std::min(m_dRoundXRadius, (dXMax - dXMin) / 2.0);
        const double dYRadius =
            std::min(m_dRoundYRadius, (dYMax - dYMin) / 2.0);
        TABGenerateArc(poRing, 45,
                       dXMin + dXRadius, dYMin + dYRadius, dXRadius, dYRadius,
                       M_PI, 3.0*M_PI/2.0);
        TABGenerateArc(poRing, 45,
                       dXMax - dXRadius, dYMin + dYRadius, dXRadius, dYRadius,
                       3.0*M_PI/2.0, 2.0*M_PI);
        TABGenerateArc(poRing, 45,
                       dXMax - dXRadius, dYMax - dYRadius, dXRadius, dYRadius,
                       0.0, M_PI/2.0);
        TABGenerateArc(poRing, 45,
                       dXMin + dXRadius, dYMax - dYRadius, dXRadius, dYRadius,
                       M_PI/2.0, M_PI);

        TABCloseRing(poRing);
    }
    else
    {
        poRing->addPoint(dXMin, dYMin);
        poRing->addPoint(dXMax, dYMin);
        poRing->addPoint(dXMax, dYMax);
        poRing->addPoint(dXMin, dYMax);
        poRing->addPoint(dXMin, dYMin);
    }

    poPolygon->addRingDirectly(poRing);
    SetGeometryDirectly(poPolygon);

    return 0;
}

/**********************************************************************
 *                   TABRectangle::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABRectangle::WriteGeometryToMAPFile(TABMAPFile *poMapFile,
                                         TABMAPObjHdr *poObjHdr,
                                         GBool bCoordBlockDataOnly /*=FALSE*/,
                                         TABMAPCoordBlock ** /*ppoCoordBlock=NULL*/)
{
    /* Nothing to do for bCoordBlockDataOnly (used by index splitting) */
    if (bCoordBlockDataOnly)
        return 0;

    /*-----------------------------------------------------------------
     * We assume that ValidateMapInfoType() was called already and that
     * the type in poObjHdr->m_nType is valid.
     *----------------------------------------------------------------*/
    CPLAssert(m_nMapInfoType == poObjHdr->m_nType);

    /*-----------------------------------------------------------------
     * Fetch and validate geometry and update MBR
     * Note that we will simply use the geometry's MBR and don't really
     * read the polygon geometry... this should be OK unless the
     * polygon geometry was not really a rectangle.
     *----------------------------------------------------------------*/
    if (UpdateMBR(poMapFile) != 0)
        return -1;  /* Error already reported */

    /*-----------------------------------------------------------------
     * Copy object information
     *----------------------------------------------------------------*/
    TABMAPObjRectEllipse *poRectHdr = cpl::down_cast<TABMAPObjRectEllipse *>(poObjHdr);

    if (m_nMapInfoType == TAB_GEOM_ROUNDRECT ||
        m_nMapInfoType == TAB_GEOM_ROUNDRECT_C)
    {
        poMapFile->Coordsys2IntDist(m_dRoundXRadius*2.0, m_dRoundYRadius*2.0,
                                    poRectHdr->m_nCornerWidth,
                                    poRectHdr->m_nCornerHeight);
    }
    else
    {
        poRectHdr->m_nCornerWidth = 0;
        poRectHdr->m_nCornerHeight = 0;
    }

    // A rectangle is defined by its MBR (values were set in UpdateMBR())
    poRectHdr->m_nMinX = m_nXMin;
    poRectHdr->m_nMinY = m_nYMin;
    poRectHdr->m_nMaxX = m_nXMax;
    poRectHdr->m_nMaxY = m_nYMax;

    m_nPenDefIndex = poMapFile->WritePenDef(&m_sPenDef);
    poRectHdr->m_nPenId = static_cast<GByte>(m_nPenDefIndex);      // Pen index

    m_nBrushDefIndex = poMapFile->WriteBrushDef(&m_sBrushDef);
    poRectHdr->m_nBrushId = static_cast<GByte>(m_nBrushDefIndex);      // Brush index

    if (CPLGetLastErrorType() == CE_Failure)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABRectangle::GetStyleString() const
 *
 * Return style string for this feature.
 *
 * Style String is built only once during the first call to GetStyleString().
 **********************************************************************/
const char *TABRectangle::GetStyleString() const
{
    if (m_pszStyleString == nullptr)
    {
        // Since GetPen/BrushStyleString() use CPLSPrintf(), we need
        // to use temporary buffers
        char *pszPen = CPLStrdup(GetPenStyleString());
        char *pszBrush = CPLStrdup(GetBrushStyleString());

        m_pszStyleString = CPLStrdup(CPLSPrintf("%s;%s", pszBrush, pszPen));

        CPLFree(pszPen);
        CPLFree(pszBrush);
    }

    return m_pszStyleString;
}

/**********************************************************************
 *                   TABRectangle::DumpMIF()
 *
 * Dump feature geometry in a format similar to .MIF REGIONs.
 **********************************************************************/
void TABRectangle::DumpMIF(FILE *fpOut /*=NULL*/)
{
    if (fpOut == nullptr)
        fpOut = stdout;

    /*-----------------------------------------------------------------
     * Output RECT or ROUNDRECT parameters
     *----------------------------------------------------------------*/
    double dXMin = 0.0;
    double dYMin = 0.0;
    double dXMax = 0.0;
    double dYMax = 0.0;
    GetMBR(dXMin, dYMin, dXMax, dYMax);

    if (m_bRoundCorners)
        fprintf(fpOut, "(ROUNDRECT %.15g %.15g %.15g %.15g    %.15g %.15g)\n",
                dXMin, dYMin, dXMax, dYMax,
                m_dRoundXRadius, m_dRoundYRadius);
    else
       fprintf(fpOut, "(RECT %.15g %.15g %.15g %.15g)\n",
               dXMin, dYMin, dXMax, dYMax);

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPolygon)
    {
        /*-------------------------------------------------------------
         * Generate rectangle output as a region
         * We could also output as a RECT or ROUNDRECT in a real MIF generator
         *------------------------------------------------------------*/
        OGRPolygon *poPolygon = poGeom->toPolygon();
        int numIntRings = poPolygon->getNumInteriorRings();
        fprintf(fpOut, "REGION %d\n", numIntRings+1);
        // In this loop, iRing=-1 for the outer ring.
        for( int iRing = -1; iRing < numIntRings; iRing++ )
        {
            OGRLinearRing *poRing = nullptr;

            if (iRing == -1)
                poRing = poPolygon->getExteriorRing();
            else
                poRing = poPolygon->getInteriorRing(iRing);

            if (poRing == nullptr)
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "TABRectangle: Object Geometry contains NULL rings!");
                return;
            }

            const int numPoints = poRing->getNumPoints();
            fprintf(fpOut, " %d\n", numPoints);
            for( int i = 0; i < numPoints; i++ )
                fprintf(fpOut, "%.15g %.15g\n",poRing->getX(i),poRing->getY(i));
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABRectangle: Missing or Invalid Geometry!");
        return;
    }

    // Finish with PEN/BRUSH/etc. clauses
    DumpPenDef();
    DumpBrushDef();

    fflush(fpOut);
}

/*=====================================================================
 *                      class TABEllipse
 *====================================================================*/

/**********************************************************************
 *                   TABEllipse::TABEllipse()
 *
 * Constructor.
 **********************************************************************/
TABEllipse::TABEllipse( OGRFeatureDefn *poDefnIn ) :
    TABFeature(poDefnIn),
    m_dCenterX(0.0),
    m_dCenterY(0.0),
    m_dXRadius(0.0),
    m_dYRadius(0.0)
{}

/**********************************************************************
 *                   TABEllipse::~TABEllipse()
 *
 * Destructor.
 **********************************************************************/
TABEllipse::~TABEllipse() {}

/**********************************************************************
 *                     TABEllipse::CloneTABFeature()
 *
 * Duplicate feature, including stuff specific to each TABFeature type.
 *
 * This method calls the generic TABFeature::CopyTABFeatureBase() and
 * then copies any members specific to its own type.
 **********************************************************************/
TABFeature *TABEllipse::CloneTABFeature(OGRFeatureDefn *poNewDefn/*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Alloc new feature and copy the base stuff
     *----------------------------------------------------------------*/
    TABEllipse *poNew = new TABEllipse(poNewDefn ? poNewDefn :
                                                   GetDefnRef());

    CopyTABFeatureBase(poNew);

    /*-----------------------------------------------------------------
     * And members specific to this class
     *----------------------------------------------------------------*/
    // ITABFeaturePen
    *(poNew->GetPenDefRef()) = *GetPenDefRef();

    // ITABFeatureBrush
    *(poNew->GetBrushDefRef()) = *GetBrushDefRef();

    poNew->m_dCenterX = m_dCenterX;
    poNew->m_dCenterY = m_dCenterY;
    poNew->m_dXRadius = m_dXRadius;
    poNew->m_dYRadius = m_dYRadius;

    return poNew;
}

/**********************************************************************
 *                   TABEllipse::ValidateMapInfoType()
 *
 * Check the feature's geometry part and return the corresponding
 * mapinfo object type code.  The m_nMapInfoType member will also
 * be updated for further calls to GetMapInfoType();
 *
 * Returns TAB_GEOM_NONE if the geometry is not compatible with what
 * is expected for this object class.
 **********************************************************************/
TABGeomType TABEllipse::ValidateMapInfoType(TABMAPFile *poMapFile /*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    if ( (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPolygon ) ||
         (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint ) )
    {
        m_nMapInfoType = TAB_GEOM_ELLIPSE;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABEllipse: Missing or Invalid Geometry!");
        m_nMapInfoType = TAB_GEOM_NONE;
    }

    /*-----------------------------------------------------------------
     * Decide if coordinates should be compressed or not.
     *----------------------------------------------------------------*/
    // __TODO__ For now we always write uncompressed for this class...
    // ValidateCoordType(poMapFile);
    UpdateMBR(poMapFile);

    return m_nMapInfoType;
}

/**********************************************************************
 *                   TABEllipse::UpdateMBR()
 *
 * Update the feature MBR members using the geometry
 *
 * Returns 0 on success, or -1 if there is no geometry in object
 **********************************************************************/
int TABEllipse::UpdateMBR(TABMAPFile * poMapFile /*=NULL*/)
{
    OGREnvelope sEnvelope;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry... Polygon and point are accepted.
     * Note that we will simply use the ellipse's MBR and don't really
     * read the polygon geometry... this should be OK unless the
     * polygon geometry was not really an ellipse.
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    if ( (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPolygon ) ||
         (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint )  )
        poGeom->getEnvelope(&sEnvelope);
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABEllipse: Missing or Invalid Geometry!");
        return -1;
    }

    /*-----------------------------------------------------------------
     * We use the center of the MBR as the ellipse center, and the
     * X/Y radius to define the MBR size.  If X/Y radius are null then
     * we'll try to use the MBR to recompute them.
     *----------------------------------------------------------------*/
    const double dXCenter = (sEnvelope.MaxX + sEnvelope.MinX) / 2.0;
    const double dYCenter = (sEnvelope.MaxY + sEnvelope.MinY) / 2.0;
    if (m_dXRadius == 0.0 && m_dYRadius == 0.0)
    {
        m_dXRadius = std::abs(sEnvelope.MaxX - sEnvelope.MinX) / 2.0;
        m_dYRadius = std::abs(sEnvelope.MaxY - sEnvelope.MinY) / 2.0;
    }

    m_dXMin = dXCenter - m_dXRadius;
    m_dYMin = dYCenter - m_dYRadius;
    m_dXMax = dXCenter + m_dXRadius;
    m_dYMax = dYCenter + m_dYRadius;

    if (poMapFile)
    {
        poMapFile->Coordsys2Int(m_dXMin, m_dYMin, m_nXMin, m_nYMin);
        poMapFile->Coordsys2Int(m_dXMax, m_dYMax, m_nXMax, m_nYMax);
    }

    return 0;
}

/**********************************************************************
 *                   TABEllipse::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABEllipse::ReadGeometryFromMAPFile(TABMAPFile *poMapFile,
                                        TABMAPObjHdr *poObjHdr,
                                        GBool bCoordBlockDataOnly /*=FALSE*/,
                                        TABMAPCoordBlock ** /*ppoCoordBlock=NULL*/)
{
    /* Nothing to do for bCoordBlockDataOnly (used by index splitting) */
    if (bCoordBlockDataOnly)
        return 0;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poObjHdr->m_nType;

    if (m_nMapInfoType != TAB_GEOM_ELLIPSE &&
        m_nMapInfoType != TAB_GEOM_ELLIPSE_C )
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
           "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
                 m_nMapInfoType, m_nMapInfoType);
        return -1;
    }

    /*-----------------------------------------------------------------
     * Read object information
     *----------------------------------------------------------------*/
    TABMAPObjRectEllipse *poRectHdr = cpl::down_cast<TABMAPObjRectEllipse *>(poObjHdr);

    // An ellipse is defined by its MBR

    double dXMin = 0.0;
    double dYMin = 0.0;
    double dXMax = 0.0;
    double dYMax = 0.0;
    poMapFile->Int2Coordsys(poRectHdr->m_nMinX, poRectHdr->m_nMinY,
                            dXMin, dYMin);
    poMapFile->Int2Coordsys(poRectHdr->m_nMaxX, poRectHdr->m_nMaxY,
                            dXMax, dYMax);

    m_nPenDefIndex = poRectHdr->m_nPenId;       // Pen index
    poMapFile->ReadPenDef(m_nPenDefIndex, &m_sPenDef);

    m_nBrushDefIndex = poRectHdr->m_nBrushId;   // Brush index
    poMapFile->ReadBrushDef(m_nBrushDefIndex, &m_sBrushDef);

    /*-----------------------------------------------------------------
     * Save info about the ellipse def. inside class members
     *----------------------------------------------------------------*/
    m_dCenterX = (dXMin + dXMax) / 2.0;
    m_dCenterY = (dYMin + dYMax) / 2.0;
    m_dXRadius = std::abs( (dXMax - dXMin) / 2.0 );
    m_dYRadius = std::abs( (dYMax - dYMin) / 2.0 );

    SetMBR(dXMin, dYMin, dXMax, dYMax);

    SetIntMBR(poObjHdr->m_nMinX, poObjHdr->m_nMinY,
              poObjHdr->m_nMaxX, poObjHdr->m_nMaxY);

    /*-----------------------------------------------------------------
     * Create and fill geometry object
     *----------------------------------------------------------------*/
    OGRPolygon *poPolygon = new OGRPolygon;
    OGRLinearRing *poRing = new OGRLinearRing();

    /*-----------------------------------------------------------------
     * For the OGR geometry, we generate an ellipse with 2 degrees line
     * segments.
     *----------------------------------------------------------------*/
    TABGenerateArc(poRing, 180,
                   m_dCenterX, m_dCenterY,
                   m_dXRadius, m_dYRadius,
                   0.0, 2.0*M_PI);
    TABCloseRing(poRing);

    poPolygon->addRingDirectly(poRing);
    SetGeometryDirectly(poPolygon);

    return 0;
}

/**********************************************************************
 *                   TABEllipse::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABEllipse::WriteGeometryToMAPFile(TABMAPFile *poMapFile,
                                       TABMAPObjHdr *poObjHdr,
                                       GBool bCoordBlockDataOnly /*=FALSE*/,
                                       TABMAPCoordBlock ** /*ppoCoordBlock=NULL*/)
{
    /* Nothing to do for bCoordBlockDataOnly (used by index splitting) */
    if (bCoordBlockDataOnly)
        return 0;

    /*-----------------------------------------------------------------
     * We assume that ValidateMapInfoType() was called already and that
     * the type in poObjHdr->m_nType is valid.
     *----------------------------------------------------------------*/
    CPLAssert(m_nMapInfoType == poObjHdr->m_nType);

    /*-----------------------------------------------------------------
     * Fetch and validate geometry... Polygon and point are accepted.
     * Note that we will simply use the ellipse's MBR and don't really
     * read the polygon geometry... this should be OK unless the
     * polygon geometry was not really an ellipse.
     *
     * We use the center of the MBR as the ellipse center, and the
     * X/Y radius to define the MBR size.  If X/Y radius are null then
     * we'll try to use the MBR to recompute them.
     *----------------------------------------------------------------*/
    if (UpdateMBR(poMapFile) != 0)
        return -1;  /* Error already reported */

    /*-----------------------------------------------------------------
     * Copy object information
     *----------------------------------------------------------------*/
    TABMAPObjRectEllipse *poRectHdr = cpl::down_cast<TABMAPObjRectEllipse *>(poObjHdr);

    // Reset RoundRect Corner members... just in case (unused for ellipse)
    poRectHdr->m_nCornerWidth = 0;
    poRectHdr->m_nCornerHeight = 0;

    // An ellipse is defined by its MBR (values were set in UpdateMBR())
    poRectHdr->m_nMinX = m_nXMin;
    poRectHdr->m_nMinY = m_nYMin;
    poRectHdr->m_nMaxX = m_nXMax;
    poRectHdr->m_nMaxY = m_nYMax;

    m_nPenDefIndex = poMapFile->WritePenDef(&m_sPenDef);
    poRectHdr->m_nPenId = static_cast<GByte>(m_nPenDefIndex);      // Pen index

    m_nBrushDefIndex = poMapFile->WriteBrushDef(&m_sBrushDef);
    poRectHdr->m_nBrushId = static_cast<GByte>(m_nBrushDefIndex);      // Brush index

    if (CPLGetLastErrorType() == CE_Failure)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABEllipse::GetStyleString() const
 *
 * Return style string for this feature.
 *
 * Style String is built only once during the first call to GetStyleString().
 **********************************************************************/
const char *TABEllipse::GetStyleString() const
{
    if (m_pszStyleString == nullptr)
    {
        // Since GetPen/BrushStyleString() use CPLSPrintf(), we need
        // to use temporary buffers
        char *pszPen = CPLStrdup(GetPenStyleString());
        char *pszBrush = CPLStrdup(GetBrushStyleString());

        m_pszStyleString = CPLStrdup(CPLSPrintf("%s;%s", pszBrush, pszPen));

        CPLFree(pszPen);
        CPLFree(pszBrush);
    }

    return m_pszStyleString;
}

/**********************************************************************
 *                   TABEllipse::DumpMIF()
 *
 * Dump feature geometry in a format similar to .MIF REGIONs.
 **********************************************************************/
void TABEllipse::DumpMIF(FILE *fpOut /*=NULL*/)
{
    if (fpOut == nullptr)
        fpOut = stdout;

    /*-----------------------------------------------------------------
     * Output ELLIPSE parameters
     *----------------------------------------------------------------*/
    double dXMin = 0.0;
    double dYMin = 0.0;
    double dXMax = 0.0;
    double dYMax = 0.0;
    GetMBR(dXMin, dYMin, dXMax, dYMax);
    fprintf(fpOut, "(ELLIPSE %.15g %.15g %.15g %.15g)\n",
            dXMin, dYMin, dXMax, dYMax);

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPolygon)
    {
        /*-------------------------------------------------------------
         * Generate ellipse output as a region
         * We could also output as an ELLIPSE in a real MIF generator
         *------------------------------------------------------------*/
        OGRPolygon *poPolygon = poGeom->toPolygon();
        int numIntRings = poPolygon->getNumInteriorRings();
        fprintf(fpOut, "REGION %d\n", numIntRings+1);
        // In this loop, iRing=-1 for the outer ring.
        for( int iRing=-1; iRing < numIntRings; iRing++ )
        {
            OGRLinearRing *poRing = nullptr;

            if (iRing == -1)
                poRing = poPolygon->getExteriorRing();
            else
                poRing = poPolygon->getInteriorRing(iRing);

            if (poRing == nullptr)
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "TABEllipse: Object Geometry contains NULL rings!");
                return;
            }

            int numPoints = poRing->getNumPoints();
            fprintf(fpOut, " %d\n", numPoints);
            for( int i = 0; i < numPoints; i++ )
                fprintf(fpOut, "%.15g %.15g\n",poRing->getX(i),poRing->getY(i));
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABEllipse: Missing or Invalid Geometry!");
        return;
    }

    // Finish with PEN/BRUSH/etc. clauses
    DumpPenDef();
    DumpBrushDef();

    fflush(fpOut);
}

/*=====================================================================
 *                      class TABArc
 *====================================================================*/

/**********************************************************************
 *                   TABArc::TABArc()
 *
 * Constructor.
 **********************************************************************/
TABArc::TABArc( OGRFeatureDefn *poDefnIn ) :
    TABFeature(poDefnIn),
    m_dStartAngle(0.0),
    m_dEndAngle(0.0),
    m_dCenterX(0.0),
    m_dCenterY(0.0),
    m_dXRadius(0.0),
    m_dYRadius(0.0)
{}

/**********************************************************************
 *                   TABArc::~TABArc()
 *
 * Destructor.
 **********************************************************************/
TABArc::~TABArc() {}

/**********************************************************************
 *                     TABArc::CloneTABFeature()
 *
 * Duplicate feature, including stuff specific to each TABFeature type.
 *
 * This method calls the generic TABFeature::CopyTABFeatureBase() and
 * then copies any members specific to its own type.
 **********************************************************************/
TABFeature *TABArc::CloneTABFeature(OGRFeatureDefn *poNewDefn/*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Alloc new feature and copy the base stuff
     *----------------------------------------------------------------*/
    TABArc *poNew = new TABArc(poNewDefn ? poNewDefn : GetDefnRef());

    CopyTABFeatureBase(poNew);

    /*-----------------------------------------------------------------
     * And members specific to this class
     *----------------------------------------------------------------*/
    // ITABFeaturePen
    *(poNew->GetPenDefRef()) = *GetPenDefRef();

    poNew->SetStartAngle( GetStartAngle() );
    poNew->SetEndAngle( GetEndAngle() );

    poNew->m_dCenterX = m_dCenterX;
    poNew->m_dCenterY = m_dCenterY;
    poNew->m_dXRadius = m_dXRadius;
    poNew->m_dYRadius = m_dYRadius;

    return poNew;
}

/**********************************************************************
 *                   TABArc::ValidateMapInfoType()
 *
 * Check the feature's geometry part and return the corresponding
 * mapinfo object type code.  The m_nMapInfoType member will also
 * be updated for further calls to GetMapInfoType();
 *
 * Returns TAB_GEOM_NONE if the geometry is not compatible with what
 * is expected for this object class.
 **********************************************************************/
TABGeomType TABArc::ValidateMapInfoType(TABMAPFile *poMapFile /*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    if ( (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString ) ||
         (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint ) )
    {
        m_nMapInfoType = TAB_GEOM_ARC;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABArc: Missing or Invalid Geometry!");
        m_nMapInfoType = TAB_GEOM_NONE;
    }

    /*-----------------------------------------------------------------
     * Decide if coordinates should be compressed or not.
     *----------------------------------------------------------------*/
    // __TODO__ For now we always write uncompressed for this class...
    // ValidateCoordType(poMapFile);
    UpdateMBR(poMapFile);

    return m_nMapInfoType;
}

/**********************************************************************
 *                   TABArc::UpdateMBR()
 *
 * Update the feature MBR members using the geometry
 *
 * Returns 0 on success, or -1 if there is no geometry in object
 **********************************************************************/
int TABArc::UpdateMBR(TABMAPFile * poMapFile /*=NULL*/)
{
    OGREnvelope sEnvelope;

    OGRGeometry *poGeom = GetGeometryRef();
    if ( (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString ) )
    {
        /*-------------------------------------------------------------
         * POLYGON geometry:
         * Note that we will simply use the ellipse's MBR and don't really
         * read the polygon geometry... this should be OK unless the
         * polygon geometry was not really an ellipse.
         * In the case of a polygon geometry. the m_dCenterX/Y values MUST
         * have been set by the caller.
         *------------------------------------------------------------*/
        poGeom->getEnvelope(&sEnvelope);
    }
    else if ( (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint ) )
    {
        /*-------------------------------------------------------------
         * In the case of a POINT GEOMETRY, we will make sure the
         * feature's m_dCenterX/Y are in sync with the point's X,Y coords.
         *
         * In this case we have to reconstruct the arc inside a temporary
         * geometry object in order to find its real MBR.
         *------------------------------------------------------------*/
        OGRPoint *poPoint = poGeom->toPoint();
        m_dCenterX = poPoint->getX();
        m_dCenterY = poPoint->getY();

        OGRLineString oTmpLine;
        int numPts=0;
        if (m_dEndAngle < m_dStartAngle)
            numPts = static_cast<int>(std::abs( ((m_dEndAngle+360)-m_dStartAngle)/2 ) + 1);
        else
            numPts = static_cast<int>(std::abs( (m_dEndAngle-m_dStartAngle)/2 ) + 1);
        numPts = std::max(2, numPts);

        TABGenerateArc(&oTmpLine, numPts,
                       m_dCenterX, m_dCenterY,
                       m_dXRadius, m_dYRadius,
                       m_dStartAngle*M_PI/180.0, m_dEndAngle*M_PI/180.0);

        oTmpLine.getEnvelope(&sEnvelope);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABArc: Missing or Invalid Geometry!");
        return -1;
    }

    // Update the Arc's MBR
    m_dXMin = sEnvelope.MinX;
    m_dYMin = sEnvelope.MinY;
    m_dXMax = sEnvelope.MaxX;
    m_dYMax = sEnvelope.MaxY;

    if (poMapFile)
    {
        poMapFile->Coordsys2Int(m_dXMin, m_dYMin, m_nXMin, m_nYMin);
        poMapFile->Coordsys2Int(m_dXMax, m_dYMax, m_nXMax, m_nYMax);
    }

    return 0;
}

/**********************************************************************
 *                   TABArc::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABArc::ReadGeometryFromMAPFile(TABMAPFile *poMapFile,
                                    TABMAPObjHdr *poObjHdr,
                                    GBool bCoordBlockDataOnly /*=FALSE*/,
                                    TABMAPCoordBlock ** /*ppoCoordBlock=NULL*/)
{
    /* Nothing to do for bCoordBlockDataOnly (used by index splitting) */
    if (bCoordBlockDataOnly)
        return 0;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poObjHdr->m_nType;

    if (m_nMapInfoType != TAB_GEOM_ARC &&
        m_nMapInfoType != TAB_GEOM_ARC_C )
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
           "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
                 m_nMapInfoType, m_nMapInfoType);
        return -1;
    }

    /*-----------------------------------------------------------------
     * Read object information
     *----------------------------------------------------------------*/
    TABMAPObjArc *poArcHdr = cpl::down_cast<TABMAPObjArc *>(poObjHdr);

    /*-------------------------------------------------------------
     * Start/End angles
     * Since the angles are specified for integer coordinates, and
     * that these coordinates can have the X axis reversed, we have to
     * adjust the angle values for the change in the X axis
     * direction.
     *
     * This should be necessary only when X axis is flipped.
     * __TODO__ Why is order of start/end values reversed as well???
     *------------------------------------------------------------*/

    /*-------------------------------------------------------------
     * OK, Arc angles again!!!!!!!!!!!!
     * After some tests in 1999-11, it appeared that the angle values
     * ALWAYS had to be flipped (read order= end angle followed by
     * start angle), no matter which quadrant the file is in.
     * This does not make any sense, so I suspect that there is something
     * that we are missing here!
     *
     * 2000-01-14.... Again!!!  Based on some sample data files:
     *  File         Ver Quadr  ReflXAxis  Read_Order   Adjust_Angle
     * test_symb.tab 300    2        1      end,start    X=yes Y=no
     * alltypes.tab: 300    1        0      start,end    X=no  Y=no
     * arcs.tab:     300    2        0      end,start    X=yes Y=no
     *
     * Until we prove it wrong, the rule would be:
     *  -> Quadrant 1 and 3, angles order = start, end
     *  -> Quadrant 2 and 4, angles order = end, start
     * + Always adjust angles for x and y axis based on quadrant.
     *
     * This was confirmed using some more files in which the quadrant was
     * manually changed, but whether these are valid results is
     * disputable.
     *
     * The ReflectXAxis flag seems to have no effect here...
     *------------------------------------------------------------*/

    /*-------------------------------------------------------------
     * In version 100 .tab files (version 400 .map), it is possible
     * to have a quadrant value of 0 and it should be treated the
     * same way as quadrant 3
     *------------------------------------------------------------*/
    if ( poMapFile->GetHeaderBlock()->m_nCoordOriginQuadrant==1 ||
         poMapFile->GetHeaderBlock()->m_nCoordOriginQuadrant==3 ||
         poMapFile->GetHeaderBlock()->m_nCoordOriginQuadrant==0  )
    {
        // Quadrants 1 and 3 ... read order = start, end
        m_dStartAngle = poArcHdr->m_nStartAngle/10.0;
        m_dEndAngle = poArcHdr->m_nEndAngle/10.0;
    }
    else
    {
        // Quadrants 2 and 4 ... read order = end, start
        m_dStartAngle = poArcHdr->m_nEndAngle/10.0;
        m_dEndAngle = poArcHdr->m_nStartAngle/10.0;
    }

    if ( poMapFile->GetHeaderBlock()->m_nCoordOriginQuadrant==2 ||
         poMapFile->GetHeaderBlock()->m_nCoordOriginQuadrant==3 ||
         poMapFile->GetHeaderBlock()->m_nCoordOriginQuadrant==0 )
    {
        // X axis direction is flipped... adjust angle
        m_dStartAngle = (m_dStartAngle<=180.0) ? (180.0-m_dStartAngle):
            (540.0-m_dStartAngle);
        m_dEndAngle   = (m_dEndAngle<=180.0) ? (180.0-m_dEndAngle):
            (540.0-m_dEndAngle);
    }

    if( fabs(m_dEndAngle - m_dStartAngle) >= 721 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Wrong start and end angles: %f %f",
                 m_dStartAngle, m_dEndAngle);
        return -1;
    }

    if (poMapFile->GetHeaderBlock()->m_nCoordOriginQuadrant==3 ||
        poMapFile->GetHeaderBlock()->m_nCoordOriginQuadrant==4 ||
        poMapFile->GetHeaderBlock()->m_nCoordOriginQuadrant==0 )
    {
        // Y axis direction is flipped... this reverses angle direction
        // Unfortunately we never found any file that contains this case,
        // but this should be the behavior to expect!!!
        //
        // 2000-01-14: some files in which quadrant was set to 3 and 4
        // manually seemed to confirm that this is the right thing to do.
        m_dStartAngle = 360.0 - m_dStartAngle;
        m_dEndAngle = 360.0 - m_dEndAngle;
    }

    // An arc is defined by its defining ellipse's MBR:

    double dXMin = 0.0;
    double dYMin = 0.0;
    double dXMax = 0.0;
    double dYMax = 0.0;

    poMapFile->Int2Coordsys(poArcHdr->m_nArcEllipseMinX,
                            poArcHdr->m_nArcEllipseMinY , dXMin, dYMin);
    poMapFile->Int2Coordsys(poArcHdr->m_nArcEllipseMaxX,
                            poArcHdr->m_nArcEllipseMaxY , dXMax, dYMax);

    m_dCenterX = (dXMin + dXMax) / 2.0;
    m_dCenterY = (dYMin + dYMax) / 2.0;
    m_dXRadius = std::abs( (dXMax - dXMin) / 2.0 );
    m_dYRadius = std::abs( (dYMax - dYMin) / 2.0 );

    // Read the Arc's MBR and use that as this feature's MBR
    poMapFile->Int2Coordsys(poArcHdr->m_nMinX, poArcHdr->m_nMinY,
                            dXMin, dYMin);
    poMapFile->Int2Coordsys(poArcHdr->m_nMaxX, poArcHdr->m_nMaxY,
                            dXMax, dYMax);
    SetMBR(dXMin, dYMin, dXMax, dYMax);

    m_nPenDefIndex = poArcHdr->m_nPenId;        // Pen index
    poMapFile->ReadPenDef(m_nPenDefIndex, &m_sPenDef);

    /*-----------------------------------------------------------------
     * Create and fill geometry object
     * For the OGR geometry, we generate an arc with 2 degrees line
     * segments.
     *----------------------------------------------------------------*/
    OGRLineString *poLine = new OGRLineString;

    const int numPts =
        std::max(2,
            (m_dEndAngle < m_dStartAngle
             ? static_cast<int>(std::abs( ((m_dEndAngle+360.0)-m_dStartAngle)/2.0 ) + 1)
             : static_cast<int>(std::abs( (m_dEndAngle-m_dStartAngle)/2.0 ) + 1)));

    TABGenerateArc(poLine, numPts,
                   m_dCenterX, m_dCenterY,
                   m_dXRadius, m_dYRadius,
                   m_dStartAngle*M_PI/180.0, m_dEndAngle*M_PI/180.0);

    SetGeometryDirectly(poLine);

    return 0;
}

/**********************************************************************
 *                   TABArc::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABArc::WriteGeometryToMAPFile(TABMAPFile *poMapFile,
                                   TABMAPObjHdr *poObjHdr,
                                   GBool bCoordBlockDataOnly /*=FALSE*/,
                                   TABMAPCoordBlock ** /*ppoCoordBlock=NULL*/)
{
    /* Nothing to do for bCoordBlockDataOnly (used by index splitting) */
    if (bCoordBlockDataOnly)
        return 0;

    /*-----------------------------------------------------------------
     * We assume that ValidateMapInfoType() was called already and that
     * the type in poObjHdr->m_nType is valid.
     *----------------------------------------------------------------*/
    CPLAssert(m_nMapInfoType == poObjHdr->m_nType);

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     * In the case of ARCs, this is all done inside UpdateMBR()
     *----------------------------------------------------------------*/
    if (UpdateMBR(poMapFile) != 0)
        return -1;  /* Error already reported */

    /*-----------------------------------------------------------------
     * Copy object information
     *----------------------------------------------------------------*/
    TABMAPObjArc *poArcHdr = cpl::down_cast<TABMAPObjArc *>(poObjHdr);

    /*-------------------------------------------------------------
     * Start/End angles
     * Since we ALWAYS produce files in quadrant 1 then we can
     * ignore the special angle conversion required by flipped axis.
     *
     * See the notes about Arc angles in TABArc::ReadGeometryFromMAPFile()
     *------------------------------------------------------------*/
    CPLAssert(poMapFile->GetHeaderBlock()->m_nCoordOriginQuadrant == 1);

    poArcHdr->m_nStartAngle = ROUND_INT(m_dStartAngle*10.0);
    poArcHdr->m_nEndAngle = ROUND_INT(m_dEndAngle*10.0);

    // An arc is defined by its defining ellipse's MBR:
    poMapFile->Coordsys2Int(m_dCenterX-m_dXRadius, m_dCenterY-m_dYRadius,
                            poArcHdr->m_nArcEllipseMinX,
                            poArcHdr->m_nArcEllipseMinY);
    poMapFile->Coordsys2Int(m_dCenterX+m_dXRadius, m_dCenterY+m_dYRadius,
                            poArcHdr->m_nArcEllipseMaxX,
                            poArcHdr->m_nArcEllipseMaxY);

    // Pass the Arc's actual MBR (values were set in UpdateMBR())
    poArcHdr->m_nMinX = m_nXMin;
    poArcHdr->m_nMinY = m_nYMin;
    poArcHdr->m_nMaxX = m_nXMax;
    poArcHdr->m_nMaxY = m_nYMax;

    m_nPenDefIndex = poMapFile->WritePenDef(&m_sPenDef);
    poArcHdr->m_nPenId = static_cast<GByte>(m_nPenDefIndex);      // Pen index

    if (CPLGetLastErrorType() == CE_Failure)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABArc::SetStart/EndAngle()
 *
 * Set the start/end angle values in degrees, making sure the values are
 * always in the range [0..360]
 **********************************************************************/
void TABArc::SetStartAngle(double dAngle)
{
    dAngle = fmod(dAngle, 360.0);
    if(dAngle < 0.0)
        dAngle += 360.0;

    m_dStartAngle = dAngle;
}

void TABArc::SetEndAngle(double dAngle)
{
    dAngle = fmod(dAngle, 360.0);
    if(dAngle < 0.0)
        dAngle += 360.0;

    m_dEndAngle = dAngle;
}

/**********************************************************************
 *                   TABArc::GetStyleString() const
 *
 * Return style string for this feature.
 *
 * Style String is built only once during the first call to GetStyleString().
 **********************************************************************/
const char *TABArc::GetStyleString() const
{
    if (m_pszStyleString == nullptr)
    {
        m_pszStyleString = CPLStrdup(GetPenStyleString());
    }

    return m_pszStyleString;
}

/**********************************************************************
 *                   TABArc::DumpMIF()
 *
 * Dump feature geometry in a format similar to .MIF REGIONs.
 **********************************************************************/
void TABArc::DumpMIF(FILE *fpOut /*=NULL*/)
{
    if (fpOut == nullptr)
        fpOut = stdout;

    /*-----------------------------------------------------------------
     * Output ARC parameters
     *----------------------------------------------------------------*/
    fprintf(fpOut, "(ARC %.15g %.15g %.15g %.15g   %d %d)\n",
            m_dCenterX - m_dXRadius, m_dCenterY - m_dYRadius,
            m_dCenterX + m_dXRadius, m_dCenterY + m_dYRadius,
            static_cast<int>(m_dStartAngle), static_cast<int>(m_dEndAngle));

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString)
    {
        /*-------------------------------------------------------------
         * Generate arc output as a simple polyline
         * We could also output as an ELLIPSE in a real MIF generator
         *------------------------------------------------------------*/
        OGRLineString *poLine = poGeom->toLineString();
        const int numPoints = poLine->getNumPoints();
        fprintf(fpOut, "PLINE %d\n", numPoints);
        for( int i = 0; i < numPoints; i++ )
            fprintf(fpOut, "%.15g %.15g\n", poLine->getX(i), poLine->getY(i));
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABArc: Missing or Invalid Geometry!");
        return;
    }

    // Finish with PEN/BRUSH/etc. clauses
    DumpPenDef();

    fflush(fpOut);
}

/*=====================================================================
 *                      class TABText
 *====================================================================*/

/**********************************************************************
 *                   TABText::TABText()
 *
 * Constructor.
 **********************************************************************/
TABText::TABText( OGRFeatureDefn *poDefnIn ) :
    TABFeature(poDefnIn),
    m_pszString(nullptr),
    m_dAngle(0.0),
    m_dHeight(0.0),
    m_dWidth(0.0),
    m_dfLineEndX(0.0),
    m_dfLineEndY(0.0),
    m_bLineEndSet(FALSE),
    m_rgbForeground(0x000000),
    m_rgbBackground(0xffffff),
    m_rgbOutline(0xffffff),
    m_rgbShadow(0x808080),
    m_nTextAlignment(0),
    m_nFontStyle(0)
{}

/**********************************************************************
 *                   TABText::~TABText()
 *
 * Destructor.
 **********************************************************************/
TABText::~TABText()
{
    CPLFree(m_pszString);
}

/**********************************************************************
 *                     TABText::CloneTABFeature()
 *
 * Duplicate feature, including stuff specific to each TABFeature type.
 *
 * This method calls the generic TABFeature::CopyTABFeatureBase() and
 * then copies any members specific to its own type.
 **********************************************************************/
TABFeature *TABText::CloneTABFeature(OGRFeatureDefn *poNewDefn/*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Alloc new feature and copy the base stuff
     *----------------------------------------------------------------*/
    TABText *poNew = new TABText(poNewDefn ? poNewDefn : GetDefnRef());

    CopyTABFeatureBase(poNew);

    /*-----------------------------------------------------------------
     * And members specific to this class
     *----------------------------------------------------------------*/
    // ITABFeaturePen
    *(poNew->GetPenDefRef()) = *GetPenDefRef();

    // ITABFeatureFont
    *(poNew->GetFontDefRef()) = *GetFontDefRef();

    poNew->SetTextString( GetTextString() );
    poNew->SetTextAngle( GetTextAngle() );
    poNew->SetTextBoxHeight( GetTextBoxHeight() );
    poNew->SetTextBoxWidth( GetTextBoxWidth() );
    poNew->SetFontStyleTABValue( GetFontStyleTABValue() );
    poNew->SetFontBGColor( GetFontBGColor() );
    poNew->SetFontFGColor( GetFontFGColor() );
    poNew->SetFontOColor( GetFontOColor() );
    poNew->SetFontSColor( GetFontSColor() );

    poNew->SetTextJustification( GetTextJustification() );
    poNew->SetTextSpacing( GetTextSpacing() );
    // Note: Text arrow/line coordinates are not transported... but
    //       we ignore them most of the time anyways.
    poNew->SetTextLineType( TABTLNoLine );

    return poNew;
}

/**********************************************************************
 *                   TABText::ValidateMapInfoType()
 *
 * Check the feature's geometry part and return the corresponding
 * mapinfo object type code.  The m_nMapInfoType member will also
 * be updated for further calls to GetMapInfoType();
 *
 * Returns TAB_GEOM_NONE if the geometry is not compatible with what
 * is expected for this object class.
 **********************************************************************/
TABGeomType TABText::ValidateMapInfoType(TABMAPFile *poMapFile /*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
    {
        m_nMapInfoType = TAB_GEOM_TEXT;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABText: Missing or Invalid Geometry!");
        m_nMapInfoType = TAB_GEOM_NONE;
    }

    /*-----------------------------------------------------------------
     * Decide if coordinates should be compressed or not.
     *----------------------------------------------------------------*/
    // __TODO__ For now we always write uncompressed for this class...
    // ValidateCoordType(poMapFile);
    UpdateMBR(poMapFile);

    return m_nMapInfoType;
}

/**********************************************************************
 *                   TABText::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABText::ReadGeometryFromMAPFile(TABMAPFile *poMapFile,
                                     TABMAPObjHdr *poObjHdr,
                                     GBool bCoordBlockDataOnly /*=FALSE*/,
                                     TABMAPCoordBlock **ppoCoordBlock/*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poObjHdr->m_nType;

    if (m_nMapInfoType != TAB_GEOM_TEXT &&
        m_nMapInfoType != TAB_GEOM_TEXT_C )
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
           "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
                 m_nMapInfoType, m_nMapInfoType);
        return -1;
    }

    /*=============================================================
     * TEXT
     *============================================================*/

    /*-----------------------------------------------------------------
     * Read object information
     *----------------------------------------------------------------*/
    TABMAPObjText *poTextHdr = cpl::down_cast<TABMAPObjText *>(poObjHdr);

    const GInt32 nCoordBlockPtr =
        poTextHdr->m_nCoordBlockPtr;   // String position
    const int nStringLen = poTextHdr->m_nCoordDataSize;   // String length
    m_nTextAlignment = poTextHdr->m_nTextAlignment; // just./spacing/arrow

    /*-------------------------------------------------------------
     * Text Angle, in tenths of degree.
     * Contrary to arc start/end angles, no conversion based on
     * origin quadrant is required here.
     *------------------------------------------------------------*/
    m_dAngle       = poTextHdr->m_nAngle/10.0;

    m_nFontStyle   = poTextHdr->m_nFontStyle;          // Font style

    m_rgbForeground = (poTextHdr->m_nFGColorR*256*256 +
                       poTextHdr->m_nFGColorG*256 +
                       poTextHdr->m_nFGColorB);
    m_rgbBackground = (poTextHdr->m_nBGColorR*256*256 +
                       poTextHdr->m_nBGColorG*256 +
                       poTextHdr->m_nBGColorB);
    m_rgbOutline =  m_rgbBackground;
    // In MapInfo, the shadow color is always gray (128,128,128)
    m_rgbShadow     = 0x808080;

    // arrow endpoint
    poMapFile->Int2Coordsys(poTextHdr->m_nLineEndX, poTextHdr->m_nLineEndY,
                            m_dfLineEndX, m_dfLineEndY);
    m_bLineEndSet = TRUE;

    // Text Height
    double dJunk = 0.0;
    poMapFile->Int2CoordsysDist(0, poTextHdr->m_nHeight, dJunk, m_dHeight);

    if (!bCoordBlockDataOnly)
    {
        m_nFontDefIndex = poTextHdr->m_nFontId;      // Font name index
        poMapFile->ReadFontDef(m_nFontDefIndex, &m_sFontDef);
    }

    // MBR after rotation
    double dXMin = 0.0;
    double dYMin = 0.0;
    double dXMax = 0.0;
    double dYMax = 0.0;
    poMapFile->Int2Coordsys(poTextHdr->m_nMinX, poTextHdr->m_nMinY,
                            dXMin, dYMin);
    poMapFile->Int2Coordsys(poTextHdr->m_nMaxX, poTextHdr->m_nMaxY,
                            dXMax, dYMax);

    if (!bCoordBlockDataOnly)
    {
        m_nPenDefIndex = poTextHdr->m_nPenId;      // Pen index for line
        poMapFile->ReadPenDef(m_nPenDefIndex, &m_sPenDef);
    }

    /*-------------------------------------------------------------
     * Read text string from the coord. block
     * Note that the string may contain binary '\n' and '\\' chars
     * that we keep to an unescaped form internally. This is to
     * be like OGR drivers. See bug 1107 for details.
     *------------------------------------------------------------*/
    char *pszTmpString = static_cast<char*>(CPLMalloc((nStringLen+1)*sizeof(char)));

    if (nStringLen > 0)
    {
        TABMAPCoordBlock *poCoordBlock = nullptr;

        if (ppoCoordBlock != nullptr && *ppoCoordBlock != nullptr)
            poCoordBlock = *ppoCoordBlock;
        else
            poCoordBlock = poMapFile->GetCoordBlock(nCoordBlockPtr);
        if (poCoordBlock == nullptr ||
            poCoordBlock->ReadBytes(nStringLen,reinterpret_cast<GByte*>(pszTmpString)) != 0)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed reading text string at offset %d",
                     nCoordBlockPtr);
            CPLFree(pszTmpString);
            return -1;
        }

        /* Return a ref to coord block so that caller can continue reading
         * after the end of this object (used by index splitting)
         */
        if (ppoCoordBlock)
            *ppoCoordBlock = poCoordBlock;
    }

    pszTmpString[nStringLen] = '\0';

    if(!poMapFile->GetEncoding().empty())
    {
        char *pszUtf8String =
            CPLRecode(pszTmpString, poMapFile->GetEncoding(), CPL_ENC_UTF8);
        CPLFree(pszTmpString);
        pszTmpString = pszUtf8String;
    }

    CPLFree(m_pszString);
    m_pszString = pszTmpString; // This string was Escaped before 20050714

    /* Set/retrieve the MBR to make sure Mins are smaller than Maxs
     */
    SetMBR(dXMin, dYMin, dXMax, dYMax);
    GetMBR(dXMin, dYMin, dXMax, dYMax);

    /* Copy int MBR to feature class members */
    SetIntMBR(poObjHdr->m_nMinX, poObjHdr->m_nMinY,
              poObjHdr->m_nMaxX, poObjHdr->m_nMaxY);

    /*-----------------------------------------------------------------
     * Create an OGRPoint Geometry...
     * The point X,Y values will be the coords of the lower-left corner before
     * rotation is applied.  (Note that the rotation in MapInfo is done around
     * the upper-left corner)
     * We need to calculate the true lower left corner of the text based
     * on the MBR after rotation, the text height and the rotation angle.
     *----------------------------------------------------------------*/
    double dSin = sin(m_dAngle * M_PI/180.0);
    double dCos = cos(m_dAngle * M_PI/180.0);
    double dX = 0.0;
    double dY = 0.0;
    if (dSin > 0.0  && dCos > 0.0)
    {
        dX = dXMin + m_dHeight * dSin;
        dY = dYMin;
    }
    else if (dSin > 0.0  && dCos < 0.0)
    {
        dX = dXMax;
        dY = dYMin - m_dHeight * dCos;
    }
    else if (dSin < 0.0  && dCos < 0.0)
    {
        dX = dXMax + m_dHeight * dSin;
        dY = dYMax;
    }
    else  // dSin < 0 && dCos > 0
    {
        dX = dXMin;
        dY = dYMax - m_dHeight * dCos;
    }

    OGRGeometry *poGeometry = new OGRPoint(dX, dY);

    SetGeometryDirectly(poGeometry);

    /*-----------------------------------------------------------------
     * Compute Text Width: the width of the Text MBR before rotation
     * in ground units... unfortunately this value is not stored in the
     * file, so we have to compute it with the MBR after rotation and
     * the height of the MBR before rotation:
     * With  W = Width of MBR before rotation
     *       H = Height of MBR before rotation
     *       dX = Width of MBR after rotation
     *       dY = Height of MBR after rotation
     *       teta = rotation angle
     *
     *  For [-PI/4..teta..+PI/4] or [3*PI/4..teta..5*PI/4], we'll use:
     *   W = H * (dX - H * sin(teta)) / (H * cos(teta))
     *
     * and for other teta values, use:
     *   W = H * (dY - H * cos(teta)) / (H * sin(teta))
     *----------------------------------------------------------------*/
    dSin = std::abs(dSin);
    dCos = std::abs(dCos);
    if (m_dHeight == 0.0)
        m_dWidth = 0.0;
    else if ( dCos > dSin )
        m_dWidth = m_dHeight * ((dXMax-dXMin) - m_dHeight*dSin) /
                                                        (m_dHeight*dCos);
    else
        m_dWidth = m_dHeight * ((dYMax-dYMin) - m_dHeight*dCos) /
                                                        (m_dHeight*dSin);
    m_dWidth = std::abs(m_dWidth);

    return 0;
}

/**********************************************************************
 *                   TABText::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABText::WriteGeometryToMAPFile(TABMAPFile *poMapFile,
                                    TABMAPObjHdr *poObjHdr,
                                    GBool bCoordBlockDataOnly /*=FALSE*/,
                                    TABMAPCoordBlock **ppoCoordBlock/*=NULL*/)
{
    GInt32              nX, nY, nXMin, nYMin, nXMax, nYMax;

    /*-----------------------------------------------------------------
     * We assume that ValidateMapInfoType() was called already and that
     * the type in poObjHdr->m_nType is valid.
     *----------------------------------------------------------------*/
    CPLAssert(m_nMapInfoType == poObjHdr->m_nType);

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    OGRPoint *poPoint = nullptr;
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
        poPoint = poGeom->toPoint();
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABText: Missing or Invalid Geometry!");
        return -1;
    }

    poMapFile->Coordsys2Int(poPoint->getX(), poPoint->getY(), nX, nY);

    /*-----------------------------------------------------------------
     * Write string to a coord block first...
     * Note that the string may contain unescaped '\n' and '\\'
     * that we have to keep like that for the MAP file.
     * See MapTools bug 1107 for more details.
     *----------------------------------------------------------------*/
    TABMAPCoordBlock *poCoordBlock = nullptr;
    if (ppoCoordBlock != nullptr && *ppoCoordBlock != nullptr)
        poCoordBlock = *ppoCoordBlock;
    else
        poCoordBlock = poMapFile->GetCurCoordBlock();
    poCoordBlock->StartNewFeature();
    GInt32 nCoordBlockPtr = poCoordBlock->GetCurAddress();

    // This string was escaped before 20050714
    CPLString oTmpString(m_pszString);
    if(!poMapFile->GetEncoding().empty())
    {
        oTmpString.Recode(CPL_ENC_UTF8, poMapFile->GetEncoding());
    }

    int nStringLen = static_cast<int>(oTmpString.length());

    if (nStringLen > 0)
    {
        poCoordBlock->WriteBytes(nStringLen, reinterpret_cast<const GByte *>(oTmpString.c_str()));
    }
    else
    {
        nCoordBlockPtr = 0;
    }

    /*-----------------------------------------------------------------
     * Copy object information
     *----------------------------------------------------------------*/
    TABMAPObjText *poTextHdr = cpl::down_cast<TABMAPObjText *>(poObjHdr);

    poTextHdr->m_nCoordBlockPtr = nCoordBlockPtr;     // String position
    poTextHdr->m_nCoordDataSize = nStringLen;         // String length
    poTextHdr->m_nTextAlignment = m_nTextAlignment;   // just./spacing/arrow

    /*-----------------------------------------------------------------
     * Text Angle, (written in tenths of degrees)
     * Contrary to arc start/end angles, no conversion based on
     * origin quadrant is required here.
     *----------------------------------------------------------------*/
    poTextHdr->m_nAngle = ROUND_INT(m_dAngle*10.0);

    poTextHdr->m_nFontStyle = m_nFontStyle;          // Font style/effect

    poTextHdr->m_nFGColorR = static_cast<GByte>(COLOR_R(m_rgbForeground));
    poTextHdr->m_nFGColorG = static_cast<GByte>(COLOR_G(m_rgbForeground));
    poTextHdr->m_nFGColorB = static_cast<GByte>(COLOR_B(m_rgbForeground));

    poTextHdr->m_nBGColorR = static_cast<GByte>(COLOR_R(m_rgbBackground));
    poTextHdr->m_nBGColorG = static_cast<GByte>(COLOR_G(m_rgbBackground));
    poTextHdr->m_nBGColorB = static_cast<GByte>(COLOR_B(m_rgbBackground));

    /*-----------------------------------------------------------------
     * The OGRPoint's X,Y values were the coords of the lower-left corner
     * before rotation was applied.  (Note that the rotation in MapInfo is
     * done around the upper-left corner)
     * The Feature's MBR is the MBR of the text after rotation... that's
     * what MapInfo uses to define the text location.
     *----------------------------------------------------------------*/
    double dXMin = 0.0;
    double dYMin = 0.0;
    double dXMax = 0.0;
    double dYMax = 0.0;
    // Make sure Feature MBR is in sync with other params

    UpdateMBR();
    GetMBR(dXMin, dYMin, dXMax, dYMax);

    poMapFile->Coordsys2Int(dXMin, dYMin, nXMin, nYMin);
    poMapFile->Coordsys2Int(dXMax, dYMax, nXMax, nYMax);

    // Label line end point
    double dX = 0.0;
    double dY = 0.0;
    GetTextLineEndPoint(dX, dY); // Make sure a default line end point is set
    poMapFile->Coordsys2Int(m_dfLineEndX, m_dfLineEndY,
                           poTextHdr->m_nLineEndX, poTextHdr->m_nLineEndY);

    // Text Height
    poMapFile->Coordsys2IntDist(0.0, m_dHeight, nX, nY);
    poTextHdr->m_nHeight = nY;

    if (!bCoordBlockDataOnly)
    {
        // Font name
        m_nFontDefIndex = poMapFile->WriteFontDef(&m_sFontDef);
        poTextHdr->m_nFontId = static_cast<GByte>(m_nFontDefIndex);      // Font name index
    }

    // MBR after rotation
    poTextHdr->SetMBR(nXMin, nYMin, nXMax, nYMax);

    if (!bCoordBlockDataOnly)
    {
        m_nPenDefIndex = poMapFile->WritePenDef(&m_sPenDef);
        poTextHdr->m_nPenId = static_cast<GByte>(m_nPenDefIndex);      // Pen index for line/arrow
    }

    if (CPLGetLastErrorType() == CE_Failure)
        return -1;

    /* Return a ref to coord block so that caller can continue writing
     * after the end of this object (used by index splitting)
     */
    if (ppoCoordBlock)
        *ppoCoordBlock = poCoordBlock;

    return 0;
}

/**********************************************************************
 *                   TABText::GetTextString()
 *
 * Return ref to text string value.
 *
 * Returned string is a reference to the internal string buffer and should
 * not be modified or freed by the caller.
 **********************************************************************/
const char *TABText::GetTextString() const
{
    if (m_pszString == nullptr)
        return "";

    return m_pszString;
}

/**********************************************************************
 *                   TABText::SetTextString()
 *
 * Set new text string value.
 *
 * Note: The text string may contain "\n" chars or "\\" chars
 * and we expect to receive them in a 2 chars escaped form as
 * described in the MIF format specs.
 **********************************************************************/
void TABText::SetTextString(const char *pszNewStr)
{
    CPLFree(m_pszString);
    m_pszString = CPLStrdup(pszNewStr);
}

/**********************************************************************
 *                   TABText::GetTextAngle()
 *
 * Return text angle in degrees.
 **********************************************************************/
double TABText::GetTextAngle() const
{
    return m_dAngle;
}

void TABText::SetTextAngle(double dAngle)
{
    // Make sure angle is in the range [0..360]
    dAngle = fmod(dAngle, 360.0);
    if(dAngle < 0.0)
        dAngle += 360.0;
    m_dAngle = dAngle;
    UpdateMBR();
}

/**********************************************************************
 *                   TABText::GetTextBoxHeight()
 *
 * Return text height in Y axis coord. units of the text box before rotation.
 **********************************************************************/
double TABText::GetTextBoxHeight() const
{
    return m_dHeight;
}

void TABText::SetTextBoxHeight(double dHeight)
{
    m_dHeight = dHeight;
    UpdateMBR();
}

/**********************************************************************
 *                   TABText::GetTextBoxWidth()
 *
 * Return text width in X axis coord. units. of the text box before rotation.
 *
 * If value has not been set, then we force a default value that assumes
 * that one char's box width is 60% of its height... and we ignore
 * the multiline case.  This should not matter when the user PROPERLY sets
 * the value.
 **********************************************************************/
double TABText::GetTextBoxWidth() const
{
    if (m_dWidth == 0.0 && m_pszString)
    {
        m_dWidth = 0.6 * m_dHeight * strlen(m_pszString);
    }
    return m_dWidth;
}

void TABText::SetTextBoxWidth(double dWidth)
{
    m_dWidth = dWidth;
    UpdateMBR();
}

/**********************************************************************
 *                   TABText::GetTextLineEndPoint()
 *
 * Return X,Y coordinates of the text label line end point.
 * Default is the center of the text MBR.
 **********************************************************************/
void TABText::GetTextLineEndPoint(double &dX, double &dY)
{
    if (!m_bLineEndSet)
    {
        // Set default location at center of text MBR
        double dXMin = 0.0;
        double dYMin = 0.0;
        double dXMax = 0.0;
        double dYMax = 0.0;
        UpdateMBR();
        GetMBR(dXMin, dYMin, dXMax, dYMax);
        m_dfLineEndX = (dXMin + dXMax) /2.0;
        m_dfLineEndY = (dYMin + dYMax) /2.0;
        m_bLineEndSet = TRUE;
    }

    // Return values
    dX = m_dfLineEndX;
    dY = m_dfLineEndY;
}

void TABText::SetTextLineEndPoint(double dX, double dY)
{
    m_dfLineEndX = dX;
    m_dfLineEndY = dY;
    m_bLineEndSet = TRUE;
}

/**********************************************************************
 *                   TABText::UpdateMBR()
 *
 * Update the feature MBR using the text origin (OGRPoint geometry), the
 * rotation angle, and the Width/height before rotation.
 *
 * This function cannot perform properly unless all the above have been set.
 *
 * Returns 0 on success, or -1 if there is no geometry in object
 **********************************************************************/
int TABText::UpdateMBR(TABMAPFile * poMapFile /*=NULL*/)
{
    OGRGeometry *poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
    {
        OGRPoint *poPoint = poGeom->toPoint();

        const double dX0 = poPoint->getX();
        const double dY0 = poPoint->getY();

        const double dSin = sin(m_dAngle*M_PI/180.0);
        const double dCos = cos(m_dAngle*M_PI/180.0);

        GetTextBoxWidth();  // Force default width value if necessary.

        const double dX[4] = {
            dX0,
            dX0 + m_dWidth,
            dX0 + m_dWidth,
            dX0
        };
        const double dY[4] = {
            dY0,
            dY0,
            dY0 + m_dHeight,
            dY0 + m_dHeight
        };

        SetMBR(dX0, dY0, dX0, dY0);
        for( int i = 0; i < 4; i++ )
        {
            // Rotate one of the box corners
            const double dX1 = dX0 + (dX[i]-dX0)*dCos - (dY[i]-dY0)*dSin;
            const double dY1 = dY0 + (dX[i]-dX0)*dSin + (dY[i]-dY0)*dCos;

            // And update feature MBR with rotated coordinate
            if (dX1 < m_dXMin) m_dXMin = dX1;
            if (dX1 > m_dXMax) m_dXMax = dX1;
            if (dY1 < m_dYMin) m_dYMin = dY1;
            if (dY1 > m_dYMax) m_dYMax = dY1;
        }

        if (poMapFile)
        {
            poMapFile->Coordsys2Int(m_dXMin, m_dYMin, m_nXMin, m_nYMin);
            poMapFile->Coordsys2Int(m_dXMax, m_dYMax, m_nXMax, m_nYMax);
        }

        return 0;
    }

    return -1;
}

/**********************************************************************
 *                   TABText::GetFontBGColor()
 *
 * Return background color.
 **********************************************************************/
GInt32 TABText::GetFontBGColor() const
{
    return m_rgbBackground;
}

void TABText::SetFontBGColor(GInt32 rgbColor)
{
    m_rgbBackground = rgbColor;
}

/**********************************************************************
 *                   TABText::GetFontOColor()
 *
 * Return outline color.
 **********************************************************************/
GInt32 TABText::GetFontOColor() const
{
    return m_rgbOutline;
}

void TABText::SetFontOColor(GInt32 rgbColor)
{
    m_rgbOutline = rgbColor;
}

/**********************************************************************
 *                   TABText::GetFontSColor()
 *
 * Return shadow color.
 **********************************************************************/
GInt32 TABText::GetFontSColor() const
{
    return m_rgbShadow;
}

void TABText::SetFontSColor(GInt32 rgbColor)
{
    m_rgbShadow = rgbColor;
}

/**********************************************************************
 *                   TABText::GetFontFGColor()
 *
 * Return foreground color.
 **********************************************************************/
GInt32 TABText::GetFontFGColor() const
{
    return m_rgbForeground;
}

void TABText::SetFontFGColor(GInt32 rgbColor)
{
    m_rgbForeground = rgbColor;
}

/**********************************************************************
 *                   TABText::GetTextJustification()
 *
 * Return text justification.  Default is TABTJLeft
 **********************************************************************/
TABTextJust TABText::GetTextJustification() const
{
    TABTextJust eJust = TABTJLeft;

    if (m_nTextAlignment & 0x0200)
        eJust = TABTJCenter;
    else if (m_nTextAlignment & 0x0400)
        eJust = TABTJRight;

    return eJust;
}

void TABText::SetTextJustification(TABTextJust eJustification)
{
    // Flush current value... default is TABTJLeft
    m_nTextAlignment &= ~ 0x0600;
    // ... and set new one.
    if (eJustification == TABTJCenter)
        m_nTextAlignment |= 0x0200;
    else if (eJustification == TABTJRight)
        m_nTextAlignment |= 0x0400;
}

/**********************************************************************
 *                   TABText::GetTextSpacing()
 *
 * Return text vertical spacing factor.  Default is TABTSSingle
 **********************************************************************/
TABTextSpacing TABText::GetTextSpacing() const
{
    TABTextSpacing eSpacing = TABTSSingle;

    if (m_nTextAlignment & 0x0800)
        eSpacing = TABTS1_5;
    else if (m_nTextAlignment & 0x1000)
        eSpacing = TABTSDouble;

    return eSpacing;
}

void TABText::SetTextSpacing(TABTextSpacing eSpacing)
{
    // Flush current value... default is TABTSSingle
    m_nTextAlignment &= ~ 0x1800;
    // ... and set new one.
    if (eSpacing == TABTS1_5)
        m_nTextAlignment |= 0x0800;
    else if (eSpacing == TABTSDouble)
        m_nTextAlignment |= 0x1000;
}

/**********************************************************************
 *                   TABText::GetTextLineType()
 *
 * Return text line (arrow) type.  Default is TABTLNoLine
 **********************************************************************/
TABTextLineType TABText::GetTextLineType() const
{
    TABTextLineType eLine = TABTLNoLine;

    if (m_nTextAlignment & 0x2000)
        eLine = TABTLSimple;
    else if (m_nTextAlignment & 0x4000)
        eLine = TABTLArrow;

    return eLine;
}

void TABText::SetTextLineType(TABTextLineType eLineType)
{
    // Flush current value... default is TABTLNoLine
    m_nTextAlignment &= ~ 0x6000;
    // ... and set new one.
    if (eLineType == TABTLSimple)
        m_nTextAlignment |= 0x2000;
    else if (eLineType == TABTLArrow)
        m_nTextAlignment |= 0x4000;
}

/**********************************************************************
 *                   TABText::QueryFontStyle()
 *
 * Return TRUE if the specified font style attribute is turned ON,
 * or FALSE otherwise.  See enum TABFontStyle for the list of styles
 * that can be queried on.
 **********************************************************************/
GBool TABText::QueryFontStyle(TABFontStyle eStyleToQuery) const
{
    return (m_nFontStyle & static_cast<int>(eStyleToQuery)) ? TRUE: FALSE;
}

void TABText::ToggleFontStyle(TABFontStyle eStyleToToggle, GBool bStyleOn)
{
    if (bStyleOn)
        m_nFontStyle |=  static_cast<int>(eStyleToToggle);
    else
        m_nFontStyle &=  ~ static_cast<int>(eStyleToToggle);
}

/**********************************************************************
 *                   TABText::GetFontStyleMIFValue()
 *
 * Return the Font Style value for this object using the style values
 * that are used in a MIF FONT() clause.  See MIF specs (appendix A).
 *
 * The reason why we have to differentiate between the TAB and the MIF font
 * style values is that in TAB, TABFSBox is included in the style value
 * as code 0x100, but in MIF it is not included, instead it is implied by
 * the presence of the BG color in the FONT() clause (the BG color is
 * present only when TABFSBox or TABFSHalo is set).
 * This also has the effect of shifting all the other style values > 0x100
 * by 1 byte.
 **********************************************************************/
int TABText::GetFontStyleMIFValue() const
{
    // The conversion is simply to remove bit 0x100 from the value and shift
    // down all values past this bit.
    return (m_nFontStyle & 0xff) + (m_nFontStyle & (0xff00-0x0100))/2;
}

void TABText:: SetFontStyleMIFValue(int nStyle, GBool bBGColorSet)
{
    m_nFontStyle = static_cast<GInt16>((nStyle & 0xff) + (nStyle & 0x7f00)*2);
    // When BG color is set, then either BOX or HALO should be set.
    if (bBGColorSet && !QueryFontStyle(TABFSHalo))
        ToggleFontStyle(TABFSBox, TRUE);
}

int TABText::IsFontBGColorUsed() const
{
    // Font BG color is used only when BOX is set.
    return QueryFontStyle(TABFSBox);
}

int TABText::IsFontOColorUsed() const
{
    // Font outline color is used only when HALO is set.
    return QueryFontStyle(TABFSHalo);
}

int TABText::IsFontSColorUsed() const
{
    // Font shadow color is used only when Shadow is set.
    return QueryFontStyle(TABFSShadow);
}

int TABText::IsFontBold() const
{
    // Font bold is used only when Bold is set.
    return QueryFontStyle(TABFSBold);
}

int TABText::IsFontItalic() const
{
    // Font italic is used only when Italic is set.
    return QueryFontStyle(TABFSItalic);
}

int TABText::IsFontUnderline() const
{
    // Font underline is used only when Underline is set.
    return QueryFontStyle(TABFSUnderline);
}

/**********************************************************************
 *                   TABText::GetLabelStyleString()
 *
 * This is not the correct location, it should be in ITABFeatureFont,
 * but it is really more easy to put it here.  This fct return a complete
 * string for the representation with the string to display
 **********************************************************************/
const char *TABText::GetLabelStyleString() const
{
    const char *pszStyle = nullptr;
    int nStringLen = static_cast<int>(strlen(GetTextString()));
    // ALL Caps, Extpanded need to modify the string value
    char *pszTextString = static_cast<char*>(CPLMalloc((nStringLen+1)*sizeof(char)));
    /* char szPattern[20]; */
    int nJustification = 1;

    strcpy(pszTextString, GetTextString());
    /* szPattern[0] = '\0'; */

    switch(GetTextJustification())
    {
      case TABTJCenter:
        nJustification = 2;
        break;
      case TABTJRight:
        nJustification = 3;
        break;
      case TABTJLeft:
      default:
        nJustification = 1;
        break;
    }

    // Compute real font size, taking number of lines ("\\n", "\n") and line
    // spacing into account.
    int numLines = 1;
    for (int i=0; pszTextString[i];
         numLines += ((pszTextString[i]=='\n' ||
                       (pszTextString[i]=='\\' && pszTextString[i+1]=='n')) &&
                      pszTextString[i+1] != '\0' ),++i);

    double dHeight = GetTextBoxHeight()/numLines;

    // In all cases, take out 20% of font height to account for line spacing
    if (numLines > 1)
    {
        switch(GetTextSpacing())
        {
          case TABTS1_5:
            dHeight *= (0.80 * 0.69);
            break;
          case TABTSDouble:
            dHeight *= (0.66 * 0.69);
            break;
          default:
            dHeight *= 0.69;
        }
    }
    else
    {
        dHeight *= 0.69;
    }

    if (QueryFontStyle(TABFSAllCaps))
        for (int i=0; pszTextString[i];++i)
            if (isalpha(pszTextString[i]))
                pszTextString[i] = static_cast<char>(toupper(pszTextString[i]));

    /* Escape the double quote chars and expand the text */
    char *pszTmpTextString = nullptr;

    if  (QueryFontStyle(TABFSExpanded))
        pszTmpTextString = static_cast<char*>(CPLMalloc(((nStringLen*4)+1)*sizeof(char)));
    else
        pszTmpTextString = static_cast<char*>(CPLMalloc(((nStringLen*2)+1)*sizeof(char)));

    int j = 0;
    for( int i = 0; i < nStringLen; ++i,++j )
    {
        if (pszTextString[i] == '"')
        {
            pszTmpTextString[j] = '\\';
            pszTmpTextString[j+1] = pszTextString[i];
            ++j;
        }
        else
            pszTmpTextString[j] = pszTextString[i];

        if  (QueryFontStyle(TABFSExpanded))
        {
            pszTmpTextString[j+1] = ' ';
            ++j;
        }
    }

    pszTmpTextString[j] = '\0';
    CPLFree(pszTextString);
    pszTextString = static_cast<char*>(CPLMalloc((strlen(pszTmpTextString)+1)*sizeof(char)));
    strcpy(pszTextString, pszTmpTextString);
    CPLFree(pszTmpTextString);

    const char *pszBGColor = IsFontBGColorUsed() ? CPLSPrintf(",b:#%6.6x",
                                                              GetFontBGColor()) :"";
    const char *pszOColor =  IsFontOColorUsed() ? CPLSPrintf(",o:#%6.6x",
                                                             GetFontOColor()) :"";
    const char *pszSColor = IsFontSColorUsed() ? CPLSPrintf(",h:#%6.6x",
                                                              GetFontSColor()) :"";
    const char *pszBold = IsFontBold() ? ",bo:1" :"";
    const char *pszItalic = IsFontItalic() ? ",it:1" :"";
    const char *pszUnderline = IsFontUnderline() ? ",un:1" : "";

    pszStyle=CPLSPrintf("LABEL(t:\"%s\",a:%f,s:%fg,c:#%6.6x%s%s%s%s%s%s,p:%d,f:\"%s\")",
                        pszTextString,GetTextAngle(), dHeight,
                        GetFontFGColor(),pszBGColor,pszOColor,pszSColor,
                        pszBold,pszItalic,pszUnderline,nJustification,GetFontNameRef());

    CPLFree(pszTextString);
    return pszStyle;
}

/**********************************************************************
 *                   TABText::GetStyleString() const
 *
 * Return style string for this feature.
 *
 * Style String is built only once during the first call to GetStyleString().
 **********************************************************************/
const char *TABText::GetStyleString() const
{
    if (m_pszStyleString == nullptr)
    {
        m_pszStyleString = CPLStrdup(GetLabelStyleString());
    }

    return m_pszStyleString;
}

/**********************************************************************
 *                   TABText::DumpMIF()
 *
 * Dump feature geometry in a format similar to .MIF REGIONs.
 **********************************************************************/
void TABText::DumpMIF(FILE *fpOut /*=NULL*/)
{
    if (fpOut == nullptr)
        fpOut = stdout;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
    {
        /*-------------------------------------------------------------
         * Generate output for text object
         *------------------------------------------------------------*/
        OGRPoint *poPoint = poGeom->toPoint();

        fprintf(fpOut, "TEXT \"%s\" %.15g %.15g\n", m_pszString?m_pszString:"",
                poPoint->getX(), poPoint->getY());

        fprintf(fpOut, "  m_pszString = '%s'\n", m_pszString);
        fprintf(fpOut, "  m_dAngle    = %.15g\n",   m_dAngle);
        fprintf(fpOut, "  m_dHeight   = %.15g\n",   m_dHeight);
        fprintf(fpOut, "  m_rgbForeground  = 0x%6.6x (%d)\n",
                                             m_rgbForeground, m_rgbForeground);
        fprintf(fpOut, "  m_rgbBackground  = 0x%6.6x (%d)\n",
                                             m_rgbBackground, m_rgbBackground);
        fprintf(fpOut, "  m_nTextAlignment = 0x%4.4x\n",  m_nTextAlignment);
        fprintf(fpOut, "  m_nFontStyle     = 0x%4.4x\n",  m_nFontStyle);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABText: Missing or Invalid Geometry!");
        return;
    }

    // Finish with PEN/BRUSH/etc. clauses
    DumpPenDef();
    DumpFontDef();

    fflush(fpOut);
}

/*=====================================================================
 *                      class TABMultiPoint
 *====================================================================*/

/**********************************************************************
 *                   TABMultiPoint::TABMultiPoint()
 *
 * Constructor.
 **********************************************************************/
TABMultiPoint::TABMultiPoint( OGRFeatureDefn *poDefnIn ) :
    TABFeature(poDefnIn),
    m_bCenterIsSet(FALSE),
    m_dCenterX(0.0),
    m_dCenterY(0.0)
{}

/**********************************************************************
 *                   TABMultiPoint::~TABMultiPoint()
 *
 * Destructor.
 **********************************************************************/
TABMultiPoint::~TABMultiPoint() {}

/**********************************************************************
 *                     TABMultiPoint::CloneTABFeature()
 *
 * Duplicate feature, including stuff specific to each TABFeature type.
 *
 * This method calls the generic TABFeature::CloneTABFeature() and
 * then copies any members specific to its own type.
 **********************************************************************/
TABFeature *TABMultiPoint::CloneTABFeature(OGRFeatureDefn *poNewDefn /*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Alloc new feature and copy the base stuff
     *----------------------------------------------------------------*/
    TABMultiPoint *poNew = new TABMultiPoint(poNewDefn?poNewDefn:GetDefnRef());

    CopyTABFeatureBase(poNew);

    /*-----------------------------------------------------------------
     * And members specific to this class
     *----------------------------------------------------------------*/
    // ITABFeatureSymbol
    *(poNew->GetSymbolDefRef()) = *GetSymbolDefRef();

    poNew->m_bCenterIsSet = m_bCenterIsSet;
    poNew->m_dCenterX = m_dCenterX;
    poNew->m_dCenterY = m_dCenterY;

    return poNew;
}

/**********************************************************************
 *                   TABMultiPoint::ValidateMapInfoType()
 *
 * Check the feature's geometry part and return the corresponding
 * mapinfo object type code.  The m_nMapInfoType member will also
 * be updated for further calls to GetMapInfoType();
 *
 * Returns TAB_GEOM_NONE if the geometry is not compatible with what
 * is expected for this object class.
 **********************************************************************/
TABGeomType TABMultiPoint::ValidateMapInfoType(TABMAPFile *poMapFile /*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbMultiPoint)
    {
        OGRMultiPoint *poMPoint = poGeom->toMultiPoint();

        if (poMPoint->getNumGeometries() > TAB_MULTIPOINT_650_MAX_VERTICES)
            m_nMapInfoType = TAB_GEOM_V800_MULTIPOINT;
        else
            m_nMapInfoType = TAB_GEOM_MULTIPOINT;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABMultiPoint: Missing or Invalid Geometry!");
        m_nMapInfoType = TAB_GEOM_NONE;
    }

    /*-----------------------------------------------------------------
     * Decide if coordinates should be compressed or not.
     *----------------------------------------------------------------*/
    ValidateCoordType(poMapFile);

    return m_nMapInfoType;
}

/**********************************************************************
 *                   TABMultiPoint::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABMultiPoint::ReadGeometryFromMAPFile(TABMAPFile *poMapFile,
                                           TABMAPObjHdr *poObjHdr,
                                           GBool bCoordBlockDataOnly /*=FALSE*/,
                                           TABMAPCoordBlock **ppoCoordBlock/*=NULL*/)
{
    double dXMin = 0.0;
    double dYMin = 0.0;
    double dXMax = 0.0;
    double dYMax = 0.0;
    OGRGeometry         *poGeometry=nullptr;
    GBool               bComprCoord = poObjHdr->IsCompressedType();
    TABMAPCoordBlock    *poCoordBlock = nullptr;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poObjHdr->m_nType;

    /*-----------------------------------------------------------------
     * Read object information
     *----------------------------------------------------------------*/
    if (m_nMapInfoType == TAB_GEOM_MULTIPOINT ||
        m_nMapInfoType == TAB_GEOM_MULTIPOINT_C ||
        m_nMapInfoType == TAB_GEOM_V800_MULTIPOINT ||
        m_nMapInfoType == TAB_GEOM_V800_MULTIPOINT_C )
    {
        /*-------------------------------------------------------------
         * Copy data from poObjHdr
         *------------------------------------------------------------*/
        TABMAPObjMultiPoint *poMPointHdr = cpl::down_cast<TABMAPObjMultiPoint *>(poObjHdr);

        const GUInt32 nMinimumBytesForPoints =
                        (bComprCoord ? 4 : 8) * poMPointHdr->m_nNumPoints;
        if( nMinimumBytesForPoints > 1024 * 1024 &&
            nMinimumBytesForPoints > poMapFile->GetFileSize() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Too many points");
            return -1;
        }

        // MBR
        poMapFile->Int2Coordsys(poMPointHdr->m_nMinX, poMPointHdr->m_nMinY,
                                dXMin, dYMin);
        poMapFile->Int2Coordsys(poMPointHdr->m_nMaxX, poMPointHdr->m_nMaxY,
                                dXMax, dYMax);

        if (!bCoordBlockDataOnly)
        {
            m_nSymbolDefIndex = poMPointHdr->m_nSymbolId;   // Symbol index
            poMapFile->ReadSymbolDef(m_nSymbolDefIndex, &m_sSymbolDef);
        }

        double dX = 0.0;
        double dY = 0.0;
        // Centroid/label point
        poMapFile->Int2Coordsys(poMPointHdr->m_nLabelX, poMPointHdr->m_nLabelY,
                                dX, dY);
        SetCenter(dX, dY);

        // Compressed coordinate origin (useful only in compressed case!)
        m_nComprOrgX = poMPointHdr->m_nComprOrgX;
        m_nComprOrgY = poMPointHdr->m_nComprOrgY;

        /*-------------------------------------------------------------
         * Read Point Coordinates
         *------------------------------------------------------------*/
        OGRMultiPoint   *poMultiPoint = new OGRMultiPoint();
        poGeometry = poMultiPoint;

        if (ppoCoordBlock != nullptr && *ppoCoordBlock != nullptr)
            poCoordBlock = *ppoCoordBlock;
        else
            poCoordBlock = poMapFile->GetCoordBlock(poMPointHdr->m_nCoordBlockPtr);
        if( poCoordBlock == nullptr )
        {
            delete poGeometry;
            return -1;
        }
        poCoordBlock->SetComprCoordOrigin(m_nComprOrgX,
                                          m_nComprOrgY);

        for(int iPoint=0; iPoint<poMPointHdr->m_nNumPoints; iPoint++)
        {
            GInt32 nX = 0;
            GInt32 nY = 0;
            if (poCoordBlock->ReadIntCoord(bComprCoord, nX, nY) != 0)
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Failed reading coordinate data at offset %d",
                         poMPointHdr->m_nCoordBlockPtr);
                delete poGeometry;
                return -1;
            }

            poMapFile->Int2Coordsys(nX, nY, dX, dY);
            OGRPoint *poPoint = new OGRPoint(dX, dY);

            if (poMultiPoint->addGeometryDirectly(poPoint) != OGRERR_NONE)
            {
                CPLAssert(false); // Just in case lower-level lib is modified
            }
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
           "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
                 m_nMapInfoType, m_nMapInfoType);
        return -1;
    }

    SetGeometryDirectly(poGeometry);

    SetMBR(dXMin, dYMin, dXMax, dYMax);

    /* Copy int MBR to feature class members */
    SetIntMBR(poObjHdr->m_nMinX, poObjHdr->m_nMinY,
              poObjHdr->m_nMaxX, poObjHdr->m_nMaxY);

    /* Return a ref to coord block so that caller can continue reading
     * after the end of this object (used by TABCollection and index splitting)
     */
    if (ppoCoordBlock)
        *ppoCoordBlock = poCoordBlock;

    return 0;
}

/**********************************************************************
 *                   TABMultiPoint::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABMultiPoint::WriteGeometryToMAPFile(TABMAPFile *poMapFile,
                                          TABMAPObjHdr *poObjHdr,
                                          GBool bCoordBlockDataOnly /*=FALSE*/,
                                          TABMAPCoordBlock **ppoCoordBlock/*=NULL*/)
{
    GInt32              nX, nY;

    /*-----------------------------------------------------------------
     * We assume that ValidateMapInfoType() was called already and that
     * the type in poObjHdr->m_nType is valid.
     *----------------------------------------------------------------*/
    CPLAssert(m_nMapInfoType == poObjHdr->m_nType);

    TABMAPObjMultiPoint *poMPointHdr = cpl::down_cast<TABMAPObjMultiPoint *>(poObjHdr);

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    OGRMultiPoint *poMPoint = nullptr;
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbMultiPoint)
        poMPoint = poGeom->toMultiPoint();
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABMultiPoint: Missing or Invalid Geometry!");
        return -1;
    }

    poMPointHdr->m_nNumPoints = poMPoint->getNumGeometries();

    /*-----------------------------------------------------------------
     * Write data to coordinate block
     *----------------------------------------------------------------*/
    const GBool bCompressed = poObjHdr->IsCompressedType();

    TABMAPCoordBlock *poCoordBlock = nullptr;
    if (ppoCoordBlock != nullptr && *ppoCoordBlock != nullptr)
        poCoordBlock = *ppoCoordBlock;
    else
        poCoordBlock = poMapFile->GetCurCoordBlock();
    poCoordBlock->StartNewFeature();
    poMPointHdr->m_nCoordBlockPtr = poCoordBlock->GetCurAddress();
    poCoordBlock->SetComprCoordOrigin(m_nComprOrgX, m_nComprOrgY);

    for( int iPoint=0, nStatus=0;
         nStatus == 0 && iPoint < poMPointHdr->m_nNumPoints;
         iPoint++)
    {
        poGeom = poMPoint->getGeometryRef(iPoint);

        if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
        {
            OGRPoint *poPoint = poGeom->toPoint();

            poMapFile->Coordsys2Int(poPoint->getX(), poPoint->getY(), nX, nY);
            if (iPoint == 0)
            {
                // Default to the first point, we may use explicit value below
                poMPointHdr->m_nLabelX = nX;
                poMPointHdr->m_nLabelY = nY;
            }

            if ((nStatus = poCoordBlock->WriteIntCoord(nX, nY,
                                                       bCompressed)) != 0)
            {
                // Failed ... error message has already been produced
                return nStatus;
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AssertionFailed,
                     "TABMultiPoint: Invalid Geometry, expecting OGRPoint!");
            return -1;
        }
    }

    /*-----------------------------------------------------------------
     * Copy object information
     *----------------------------------------------------------------*/

    // Compressed coordinate origin (useful only in compressed case!)
    poMPointHdr->m_nComprOrgX = m_nComprOrgX;
    poMPointHdr->m_nComprOrgY = m_nComprOrgY;

    poMPointHdr->m_nCoordDataSize = poCoordBlock->GetFeatureDataSize();
    poMPointHdr->SetMBR(m_nXMin, m_nYMin, m_nXMax, m_nYMax);

    // Center/label point (default value already set above)
    double dX = 0.0;
    double dY = 0.0;
    if (GetCenter(dX, dY) != -1)
    {
        poMapFile->Coordsys2Int(dX, dY, poMPointHdr->m_nLabelX,
                                poMPointHdr->m_nLabelY);
    }

    if (!bCoordBlockDataOnly)
    {
        m_nSymbolDefIndex = poMapFile->WriteSymbolDef(&m_sSymbolDef);
        poMPointHdr->m_nSymbolId = static_cast<GByte>(m_nSymbolDefIndex);      // Symbol index
    }

    if (CPLGetLastErrorType() == CE_Failure)
        return -1;

    /* Return a ref to coord block so that caller can continue writing
     * after the end of this object (used by index splitting)
     */
    if (ppoCoordBlock)
        *ppoCoordBlock = poCoordBlock;

    return 0;
}

/**********************************************************************
 *                   TABMultiPoint::GetXY()
 *
 * Return this point's X,Y coordinates.
 **********************************************************************/
int TABMultiPoint::GetXY( int i, double &dX, double &dY )
{
    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbMultiPoint)
    {
        OGRMultiPoint *poMPoint = poGeom->toMultiPoint();

        if (i >= 0 && i < poMPoint->getNumGeometries() &&
            (poGeom = poMPoint->getGeometryRef(i)) != nullptr &&
            wkbFlatten(poGeom->getGeometryType()) == wkbPoint )
        {
            OGRPoint *poPoint = poGeom->toPoint();

            dX = poPoint->getX();
            dY = poPoint->getY();
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABMultiPoint: Missing or Invalid Geometry!");
        dX = 0.0;
        dY = 0.0;
        return -1;
    }

    return 0;
}

/**********************************************************************
 *                   TABMultiPoint::GetNumPoints()
 *
 * Return the number of points in this multipoint object
 **********************************************************************/
int TABMultiPoint::GetNumPoints()
{
    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbMultiPoint)
    {
        OGRMultiPoint *poMPoint = poGeom->toMultiPoint();

        return poMPoint->getNumGeometries();
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABMultiPoint: Missing or Invalid Geometry!");
        return 0;
    }
}

/**********************************************************************
 *                   TABMultiPoint::GetStyleString() const
 *
 * Return style string for this feature.
 *
 * Style String is built only once during the first call to GetStyleString().
 **********************************************************************/
const char *TABMultiPoint::GetStyleString() const
{
    if (m_pszStyleString == nullptr)
    {
        m_pszStyleString = CPLStrdup(GetSymbolStyleString());
    }

    return m_pszStyleString;
}

/**********************************************************************
 *                   TABMultiPoint::GetCenter()
 *
 * Returns the center point (or label point?) of the object.  Compute one
 * if it was not explicitly set:
 *
 * The default seems to be to use the first point in the collection as
 * the center.. so we'll use that.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMultiPoint::GetCenter( double &dX, double &dY )
{
    if (!m_bCenterIsSet && GetNumPoints() > 0)
    {
        // The default seems to be to use the first point in the collection
        // as the center... so we'll use that.
        if (GetXY(0, m_dCenterX, m_dCenterY) == 0)
            m_bCenterIsSet = TRUE;
    }

    if (!m_bCenterIsSet)
        return -1;

    dX = m_dCenterX;
    dY = m_dCenterY;
    return 0;
}

/**********************************************************************
 *                   TABMultiPoint::SetCenter()
 *
 * Set the X,Y coordinates to use as center point (or label point?)
 **********************************************************************/
void TABMultiPoint::SetCenter(double dX, double dY)
{
    m_dCenterX = dX;
    m_dCenterY = dY;
    m_bCenterIsSet = TRUE;
}

/**********************************************************************
 *                   TABMultiPoint::DumpMIF()
 *
 * Dump feature geometry in a format similar to .MIF POINTs.
 **********************************************************************/
void TABMultiPoint::DumpMIF(FILE *fpOut /*=NULL*/)
{
    if (fpOut == nullptr)
        fpOut = stdout;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    OGRMultiPoint *poMPoint = nullptr;
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbMultiPoint)
        poMPoint = poGeom->toMultiPoint();
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABMultiPoint: Missing or Invalid Geometry!");
        return;
    }

    /*-----------------------------------------------------------------
     * Generate output
     *----------------------------------------------------------------*/
    fprintf(fpOut, "MULTIPOINT %d\n", poMPoint->getNumGeometries());

    for (int iPoint=0; iPoint < poMPoint->getNumGeometries(); iPoint++)
    {
        poGeom = poMPoint->getGeometryRef(iPoint);

        if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
        {
            OGRPoint *poPoint = poGeom->toPoint();
            fprintf(fpOut, "  %.15g %.15g\n", poPoint->getX(), poPoint->getY() );
        }
        else
        {
            CPLError(CE_Failure, CPLE_AssertionFailed,
                     "TABMultiPoint: Invalid Geometry, expecting OGRPoint!");
            return;
        }
    }

    DumpSymbolDef(fpOut);

    if (m_bCenterIsSet)
        fprintf(fpOut, "Center %.15g %.15g\n", m_dCenterX, m_dCenterY);

    fflush(fpOut);
}

/*=====================================================================
 *                      class TABCollection
 *====================================================================*/

/**********************************************************************
 *                   TABCollection::TABCollection()
 *
 * Constructor.
 **********************************************************************/
TABCollection::TABCollection( OGRFeatureDefn *poDefnIn ) :
    TABFeature(poDefnIn),
    m_poRegion(nullptr),
    m_poPline(nullptr),
    m_poMpoint(nullptr)
{}

/**********************************************************************
 *                   TABCollection::~TABCollection()
 *
 * Destructor.
 **********************************************************************/
TABCollection::~TABCollection()
{
    EmptyCollection();
}

/**********************************************************************
 *                   TABCollection::EmptyCollection()
 *
 * Delete/free all collection components.
 **********************************************************************/
void TABCollection::EmptyCollection()
{

    if (m_poRegion)
    {
        delete m_poRegion;
        m_poRegion = nullptr;
    }

    if (m_poPline)
    {
        delete m_poPline;
        m_poPline = nullptr;
    }

    if (m_poMpoint)
    {
        delete m_poMpoint;
        m_poMpoint = nullptr;
    }

    // Empty OGR Geometry Collection as well
    SyncOGRGeometryCollection(TRUE, TRUE, TRUE);
}

/**********************************************************************
 *                     TABCollection::CloneTABFeature()
 *
 * Duplicate feature, including stuff specific to each TABFeature type.
 *
 * This method calls the generic TABFeature::CloneTABFeature() and
 * then copies any members specific to its own type.
 **********************************************************************/
TABFeature *TABCollection::CloneTABFeature(OGRFeatureDefn *poNewDefn /*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Alloc new feature and copy the base stuff
     *----------------------------------------------------------------*/
    TABCollection *poNew = new TABCollection(poNewDefn?poNewDefn:GetDefnRef());

    CopyTABFeatureBase(poNew);

    /*-----------------------------------------------------------------
     * And members specific to this class
     *----------------------------------------------------------------*/

    if (m_poRegion)
        poNew->SetRegionDirectly(cpl::down_cast<TABRegion*>(m_poRegion->CloneTABFeature()));

    if (m_poPline)
        poNew->SetPolylineDirectly(cpl::down_cast<TABPolyline*>(m_poPline->CloneTABFeature()));

    if (m_poMpoint)
        poNew->SetMultiPointDirectly(cpl::down_cast<TABMultiPoint*>(m_poMpoint->CloneTABFeature()));

    return poNew;
}

/**********************************************************************
 *                   TABCollection::ValidateMapInfoType()
 *
 * Check the feature's geometry part and return the corresponding
 * mapinfo object type code.  The m_nMapInfoType member will also
 * be updated for further calls to GetMapInfoType();
 *
 * Returns TAB_GEOM_NONE if the geometry is not compatible with what
 * is expected for this object class.
 **********************************************************************/
TABGeomType TABCollection::ValidateMapInfoType(TABMAPFile *poMapFile /*=NULL*/)
{
    int nRegionType=TAB_GEOM_NONE;
    int nPLineType=TAB_GEOM_NONE;
    int nMPointType=TAB_GEOM_NONE;
    int nVersion = 650;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbGeometryCollection)
    {
        m_nMapInfoType = TAB_GEOM_COLLECTION;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABCollection: Missing or Invalid Geometry!");
        m_nMapInfoType = TAB_GEOM_NONE;
    }

    /*-----------------------------------------------------------------
     * Decide if coordinates should be compressed or not.
     *----------------------------------------------------------------*/
    GBool bComprCoord = ValidateCoordType(poMapFile);

    /*-----------------------------------------------------------------
     * Since all members of the collection share the same compressed coord
     * origin, we should force the compressed origin in all components
     * to be the same.
     * This also implies that ValidateMapInfoType() should *NOT* be called
     * again until the collection components are written by WriteGeom...()
     *----------------------------------------------------------------*/

    // First pass to figure collection type...
    if (m_poRegion)
    {
        m_poRegion->ValidateCoordType(poMapFile);
        nRegionType = m_poRegion->ValidateMapInfoType(poMapFile);
        if (TAB_GEOM_GET_VERSION(nRegionType) > nVersion)
            nVersion = TAB_GEOM_GET_VERSION(nRegionType);
    }

    if (m_poPline)
    {
        m_poPline->ValidateCoordType(poMapFile);
        nPLineType = m_poPline->ValidateMapInfoType(poMapFile);
        if (TAB_GEOM_GET_VERSION(nPLineType) > nVersion)
            nVersion = TAB_GEOM_GET_VERSION(nPLineType);
    }

    if (m_poMpoint)
    {
        m_poMpoint->ValidateCoordType(poMapFile);
        nMPointType = m_poMpoint->ValidateMapInfoType(poMapFile);
        if (TAB_GEOM_GET_VERSION(nMPointType) > nVersion)
            nVersion = TAB_GEOM_GET_VERSION(nMPointType);
    }

    // Need to upgrade native type of collection?
    if (nVersion == 800)
    {
        m_nMapInfoType = TAB_GEOM_V800_COLLECTION;
    }

    // Make another pass updating native type and coordinates type and origin
    // of each component
    if (m_poRegion && nRegionType != TAB_GEOM_NONE)
    {
        GInt32 nXMin = 0;
        GInt32 nYMin = 0;
        GInt32 nXMax = 0;
        GInt32 nYMax = 0;
        m_poRegion->GetIntMBR(nXMin, nYMin, nXMax, nYMax);
        m_poRegion->ForceCoordTypeAndOrigin((nVersion == 800 ?
                                             TAB_GEOM_V800_REGION:
                                             TAB_GEOM_V450_REGION),
                                            bComprCoord,
                                            m_nComprOrgX, m_nComprOrgY,
                                            nXMin, nYMin, nXMax, nYMax);
    }

    if (m_poPline && nPLineType != TAB_GEOM_NONE)
    {
        GInt32 nXMin, nYMin, nXMax, nYMax;
        m_poPline->GetIntMBR(nXMin, nYMin, nXMax, nYMax);
        m_poPline->ForceCoordTypeAndOrigin((nVersion == 800 ?
                                            TAB_GEOM_V800_MULTIPLINE:
                                            TAB_GEOM_V450_MULTIPLINE),
                                           bComprCoord,
                                           m_nComprOrgX, m_nComprOrgY,
                                           nXMin, nYMin, nXMax, nYMax);
    }

    if (m_poMpoint && nMPointType != TAB_GEOM_NONE)
    {
        GInt32 nXMin, nYMin, nXMax, nYMax;
        m_poMpoint->GetIntMBR(nXMin, nYMin, nXMax, nYMax);
        m_poMpoint->ForceCoordTypeAndOrigin((nVersion == 800 ?
                                             TAB_GEOM_V800_MULTIPOINT:
                                             TAB_GEOM_MULTIPOINT),
                                            bComprCoord,
                                            m_nComprOrgX, m_nComprOrgY,
                                            nXMin, nYMin, nXMax, nYMax);
    }

    return m_nMapInfoType;
}

/**********************************************************************
 *                   TABCollection::ReadLabelAndMBR()
 *
 * Reads the label and MBR elements of the header of a collection component
 *
 * Returns 0 on success, -1 on failure.
 **********************************************************************/
int  TABCollection::ReadLabelAndMBR(TABMAPCoordBlock *poCoordBlock,
                                    GBool bComprCoord,
                                    GInt32 nComprOrgX, GInt32 nComprOrgY,
                                    GInt32 &pnMinX, GInt32 &pnMinY,
                                    GInt32 &pnMaxX, GInt32 &pnMaxY,
                                    GInt32 &pnLabelX, GInt32 &pnLabelY )
{
    //
    // The sections in the collection's coord blocks start with center/label
    // point + MBR that are normally found in the object data blocks
    // of regular region/pline/mulitpoint objects.
    //

    if (bComprCoord)
    {
        // Region center/label point, relative to compr. coord. origin
        // No it is not relative to the Object block center
        pnLabelX = poCoordBlock->ReadInt16();
        pnLabelY = poCoordBlock->ReadInt16();

        TABSaturatedAdd(pnLabelX, nComprOrgX);
        TABSaturatedAdd(pnLabelY, nComprOrgY);

        pnMinX = poCoordBlock->ReadInt16(); // Read MBR
        pnMinY = poCoordBlock->ReadInt16();
        pnMaxX = poCoordBlock->ReadInt16();
        pnMaxY = poCoordBlock->ReadInt16();
        TABSaturatedAdd(pnMinX, nComprOrgX);
        TABSaturatedAdd(pnMinY, nComprOrgY);
        TABSaturatedAdd(pnMaxX, nComprOrgX);
        TABSaturatedAdd(pnMaxY, nComprOrgY);
    }
    else
    {
        // Region center/label point, relative to compr. coord. origin
        // No it is not relative to the Object block center
        pnLabelX = poCoordBlock->ReadInt32();
        pnLabelY = poCoordBlock->ReadInt32();

        pnMinX = poCoordBlock->ReadInt32();    // Read MBR
        pnMinY = poCoordBlock->ReadInt32();
        pnMaxX = poCoordBlock->ReadInt32();
        pnMaxY = poCoordBlock->ReadInt32();
    }

    return 0;
}

/**********************************************************************
 *                   TABCollection::WriteLabelAndMBR()
 *
 * Writes the label and MBR elements of the header of a collection component
 *
 * Returns 0 on success, -1 on failure.
 **********************************************************************/
int  TABCollection::WriteLabelAndMBR(TABMAPCoordBlock *poCoordBlock,
                                     GBool bComprCoord,
                                     GInt32 nMinX, GInt32 nMinY,
                                     GInt32 nMaxX, GInt32 nMaxY,
                                     GInt32 nLabelX, GInt32 nLabelY )
{
    //
    // The sections in the collection's coord blocks start with center/label
    // point + MBR that are normally found in the object data blocks
    // of regular region/pline/mulitpoint objects.
    //

    int nStatus = 0;
    if ((nStatus = poCoordBlock->WriteIntCoord(nLabelX, nLabelY,
                                               bComprCoord)) != 0 ||
        (nStatus = poCoordBlock->WriteIntCoord(nMinX, nMinY,
                                               bComprCoord)) != 0 ||
        (nStatus = poCoordBlock->WriteIntCoord(nMaxX, nMaxY,
                                               bComprCoord)) != 0   )
    {
        // Failed ... error message has already been produced
        return nStatus;
    }

    return 0;
}

/**********************************************************************
 *                   TABCollection::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABCollection::ReadGeometryFromMAPFile(TABMAPFile *poMapFile,
                                           TABMAPObjHdr *poObjHdr,
                                           GBool bCoordBlockDataOnly /*=FALSE*/,
                                           TABMAPCoordBlock **ppoCoordBlock/*=NULL*/)
{
    const GBool bComprCoord = poObjHdr->IsCompressedType();

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poObjHdr->m_nType;

    if (m_nMapInfoType != TAB_GEOM_COLLECTION &&
        m_nMapInfoType != TAB_GEOM_COLLECTION_C &&
        m_nMapInfoType != TAB_GEOM_V800_COLLECTION &&
        m_nMapInfoType != TAB_GEOM_V800_COLLECTION_C )
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
           "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
                 m_nMapInfoType, m_nMapInfoType);
        return -1;
    }

    int nVersion = TAB_GEOM_GET_VERSION(m_nMapInfoType);

    // Make sure collection is empty
    EmptyCollection();

    /*-------------------------------------------------------------
     * Copy data from poObjHdr
     *------------------------------------------------------------*/
    TABMAPObjCollection *poCollHdr = cpl::down_cast<TABMAPObjCollection *>(poObjHdr);

    // MBR
    double dXMin = 0.0;
    double dYMin = 0.0;
    double dXMax = 0.0;
    double dYMax = 0.0;
    poMapFile->Int2Coordsys(poCollHdr->m_nMinX, poCollHdr->m_nMinY,
                            dXMin, dYMin);
    poMapFile->Int2Coordsys(poCollHdr->m_nMaxX, poCollHdr->m_nMaxY,
                            dXMax, dYMax);

    SetMBR(dXMin, dYMin, dXMax, dYMax);

    SetIntMBR(poObjHdr->m_nMinX, poObjHdr->m_nMinY,
              poObjHdr->m_nMaxX, poObjHdr->m_nMaxY);

    int nCurCoordBlockPtr = poCollHdr->m_nCoordBlockPtr;
    TABMAPCoordBlock* poCoordBlock = nullptr;
    if (ppoCoordBlock != nullptr && *ppoCoordBlock != nullptr)
        poCoordBlock = *ppoCoordBlock;
    else
        poCoordBlock = poMapFile->GetCoordBlock(nCurCoordBlockPtr);

    // Compressed coordinate origin (useful only in compressed case!)
    m_nComprOrgX = poCollHdr->m_nComprOrgX;
    m_nComprOrgY = poCollHdr->m_nComprOrgY;

    /*-----------------------------------------------------------------
     * Region Component
     *----------------------------------------------------------------*/
    if(poCoordBlock != nullptr && poCollHdr->m_nNumRegSections > 0)
    {
        //
        // Build fake coord section header to pass to TABRegion::ReadGeom...()
        //
        TABMAPObjPLine      oRegionHdr;

        oRegionHdr.m_nComprOrgX = poCollHdr->m_nComprOrgX;
        oRegionHdr.m_nComprOrgY = poCollHdr->m_nComprOrgY;

        //
        // The region section in the coord block starts with center/label
        // point + MBR that are normally found in the object data blocks
        // of regular region objects.
        //

        // In V800 the mini-header starts with a copy of num_parts
        if (nVersion >= 800)
        {
            // int numParts = poCoordBlock->ReadInt32();
            CPLAssert(poCoordBlock->ReadInt32() == poCollHdr->m_nNumRegSections);
        }

        ReadLabelAndMBR(poCoordBlock, bComprCoord,
                        oRegionHdr.m_nComprOrgX, oRegionHdr.m_nComprOrgY,
                        oRegionHdr.m_nMinX, oRegionHdr.m_nMinY,
                        oRegionHdr.m_nMaxX, oRegionHdr.m_nMaxY,
                        oRegionHdr.m_nLabelX, oRegionHdr.m_nLabelY);

        // Set CoordBlockPtr so that TABRegion continues reading here
        oRegionHdr.m_nCoordBlockPtr = poCoordBlock->GetCurAddress();

        if (bComprCoord)
            oRegionHdr.m_nType = TAB_GEOM_V450_REGION_C;
        else
            oRegionHdr.m_nType = TAB_GEOM_V450_REGION;
        if (nVersion == 800)
            oRegionHdr.m_nType = static_cast<TABGeomType>(oRegionHdr.m_nType + (TAB_GEOM_V800_REGION - TAB_GEOM_V450_REGION));

        oRegionHdr.m_numLineSections = poCollHdr->m_nNumRegSections;
        oRegionHdr.m_nPenId = poCollHdr->m_nRegionPenId;
        oRegionHdr.m_nBrushId = poCollHdr->m_nRegionBrushId;
        oRegionHdr.m_bSmooth = 0;       // TODO

        //
        // Use a TABRegion to read/store the Region coord data
        //
        m_poRegion = new TABRegion(GetDefnRef());
        if (m_poRegion->ReadGeometryFromMAPFile(poMapFile, &oRegionHdr,
                                                bCoordBlockDataOnly,
                                                &poCoordBlock) != 0)
            return -1;

        // Set new coord block ptr for next object
        /*if (poCoordBlock)
            nCurCoordBlockPtr = poCoordBlock->GetCurAddress();*/
    }

    /*-----------------------------------------------------------------
     * PLine Component
     *----------------------------------------------------------------*/
    if(poCoordBlock != nullptr && poCollHdr->m_nNumPLineSections > 0)
    {
        //
        // Build fake coord section header to pass to TABPolyline::ReadGeom..()
        //
        TABMAPObjPLine      oPLineHdr;

        oPLineHdr.m_nComprOrgX = poCollHdr->m_nComprOrgX;
        oPLineHdr.m_nComprOrgY = poCollHdr->m_nComprOrgY;

        //
        // The pline section in the coord block starts with center/label
        // point + MBR that are normally found in the object data blocks
        // of regular pline objects.
        //

        // In V800 the mini-header starts with a copy of num_parts
        if (nVersion >= 800)
        {
            // int numParts = poCoordBlock->ReadInt32();
            CPLAssert(poCoordBlock->ReadInt32() ==
                      poCollHdr->m_nNumPLineSections);
        }

        ReadLabelAndMBR(poCoordBlock, bComprCoord,
                        oPLineHdr.m_nComprOrgX, oPLineHdr.m_nComprOrgY,
                        oPLineHdr.m_nMinX, oPLineHdr.m_nMinY,
                        oPLineHdr.m_nMaxX, oPLineHdr.m_nMaxY,
                        oPLineHdr.m_nLabelX, oPLineHdr.m_nLabelY);

        // Set CoordBlockPtr so that TABRegion continues reading here
        oPLineHdr.m_nCoordBlockPtr = poCoordBlock->GetCurAddress();

        if (bComprCoord)
            oPLineHdr.m_nType = TAB_GEOM_V450_MULTIPLINE_C;
        else
            oPLineHdr.m_nType = TAB_GEOM_V450_MULTIPLINE;
        if (nVersion == 800)
            oPLineHdr.m_nType = static_cast<TABGeomType>(oPLineHdr.m_nType + (TAB_GEOM_V800_MULTIPLINE -
                                  TAB_GEOM_V450_MULTIPLINE));

        oPLineHdr.m_numLineSections = poCollHdr->m_nNumPLineSections;
        oPLineHdr.m_nPenId = poCollHdr->m_nPolylinePenId;
        oPLineHdr.m_bSmooth = 0;        // TODO

        //
        // Use a TABPolyline to read/store the Polyline coord data
        //
        m_poPline = new TABPolyline(GetDefnRef());
        if (m_poPline->ReadGeometryFromMAPFile(poMapFile, &oPLineHdr,
                                               bCoordBlockDataOnly,
                                               &poCoordBlock) != 0)
            return -1;

        // Set new coord block ptr for next object
        /*if (poCoordBlock)
            nCurCoordBlockPtr = poCoordBlock->GetCurAddress();*/
    }

    /*-----------------------------------------------------------------
     * MultiPoint Component
     *----------------------------------------------------------------*/
    if(poCoordBlock != nullptr && poCollHdr->m_nNumMultiPoints > 0)
    {
        //
        // Build fake coord section header to pass to TABMultiPoint::ReadGeom()
        //
        TABMAPObjMultiPoint oMPointHdr;

        oMPointHdr.m_nComprOrgX = poCollHdr->m_nComprOrgX;
        oMPointHdr.m_nComprOrgY = poCollHdr->m_nComprOrgY;

        //
        // The pline section in the coord block starts with center/label
        // point + MBR that are normally found in the object data blocks
        // of regular pline objects.
        //
        ReadLabelAndMBR(poCoordBlock, bComprCoord,
                        oMPointHdr.m_nComprOrgX, oMPointHdr.m_nComprOrgY,
                        oMPointHdr.m_nMinX, oMPointHdr.m_nMinY,
                        oMPointHdr.m_nMaxX, oMPointHdr.m_nMaxY,
                        oMPointHdr.m_nLabelX, oMPointHdr.m_nLabelY);

        // Set CoordBlockPtr so that TABRegion continues reading here
        oMPointHdr.m_nCoordBlockPtr = poCoordBlock->GetCurAddress();

        if (bComprCoord)
            oMPointHdr.m_nType = TAB_GEOM_MULTIPOINT_C;
        else
            oMPointHdr.m_nType = TAB_GEOM_MULTIPOINT;
        if (nVersion == 800)
            oMPointHdr.m_nType = static_cast<TABGeomType>(oMPointHdr.m_nType + (TAB_GEOM_V800_MULTIPOINT -
                                  TAB_GEOM_MULTIPOINT));

        oMPointHdr.m_nNumPoints = poCollHdr->m_nNumMultiPoints;
        oMPointHdr.m_nSymbolId = poCollHdr->m_nMultiPointSymbolId;

        //
        // Use a TABMultiPoint to read/store the coord data
        //
        m_poMpoint = new TABMultiPoint(GetDefnRef());
        if (m_poMpoint->ReadGeometryFromMAPFile(poMapFile, &oMPointHdr,
                                                bCoordBlockDataOnly,
                                                &poCoordBlock) != 0)
            return -1;

        // Set new coord block ptr for next object (not really useful here)
        /*if (poCoordBlock)
            nCurCoordBlockPtr = poCoordBlock->GetCurAddress();*/
    }

    /*-----------------------------------------------------------------
     * Set the main OGRFeature Geometry
     * (this is actually duplicating geometries from each member)
     *----------------------------------------------------------------*/
    if (SyncOGRGeometryCollection(TRUE, TRUE, TRUE) != 0)
        return -1;

    /* Return a ref to coord block so that caller can continue reading
     * after the end of this object (used by index splitting)
     */
    if (ppoCoordBlock)
        *ppoCoordBlock = poCoordBlock;

    return 0;
}

/**********************************************************************
 *                   TABCollection::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABCollection::WriteGeometryToMAPFile(TABMAPFile *poMapFile,
                                          TABMAPObjHdr *poObjHdr,
                                          GBool bCoordBlockDataOnly /*=FALSE*/,
                                          TABMAPCoordBlock **ppoCoordBlock/*=NULL*/)
{
    /*-----------------------------------------------------------------
     * Note that the current implementation does not allow setting the
     * Geometry via OGRFeature::SetGeometry(). The geometries must be set
     * via the SetRegion/Pline/MpointDirectly() methods which will take
     * care of keeping the OGRFeature's geometry in sync.
     *
     * TODO: If we ever want to support sync'ing changes from the OGRFeature's
     * geometry to the m_poRegion/Pline/Mpoint then a call should be added
     * here, or perhaps in ValidateMapInfoType(), or even better in
     * custom TABCollection::SetGeometry*()... but then this last option
     * won't work unless OGRFeature::SetGeometry*() are made virtual in OGR.
     *----------------------------------------------------------------*/

    /*-----------------------------------------------------------------
     * We assume that ValidateMapInfoType() was called already and that
     * the type in poObjHdr->m_nType is valid.
     *----------------------------------------------------------------*/
    CPLAssert(m_nMapInfoType == poObjHdr->m_nType);

    TABMAPObjCollection *poCollHdr = cpl::down_cast<TABMAPObjCollection *>(poObjHdr);

    /*-----------------------------------------------------------------
     * Write data to coordinate block for each component...
     *
     * Note that at this point, the caller (TABFile) has called
     * TABCollection::ValidateMapInfoType() which in turn has called
     * each component's respective ValidateMapInfoType() and
     * ForceCoordTypeAndCoordOrigin() so the objects are ready to have
     * their respective WriteGeometryToMapFile() called.
     *----------------------------------------------------------------*/
    const GBool   bCompressed = poObjHdr->IsCompressedType();
    // TODO: ??? Do we need to track overall collection coord data size???
    int     nTotalFeatureDataSize = 0;

    const int nVersion = TAB_GEOM_GET_VERSION(m_nMapInfoType);

    TABMAPCoordBlock *poCoordBlock = nullptr;
    if (ppoCoordBlock != nullptr && *ppoCoordBlock != nullptr)
        poCoordBlock = *ppoCoordBlock;
    else
        poCoordBlock = poMapFile->GetCurCoordBlock();
    poCoordBlock->StartNewFeature();
    poCollHdr->m_nCoordBlockPtr = poCoordBlock->GetCurAddress();
    poCoordBlock->SetComprCoordOrigin(m_nComprOrgX, m_nComprOrgY);

    /*-----------------------------------------------------------------
     * Region component
     *----------------------------------------------------------------*/
    if (m_poRegion && m_poRegion->GetMapInfoType() != TAB_GEOM_NONE)
    {
        CPLAssert(m_poRegion->GetMapInfoType() == TAB_GEOM_V450_REGION ||
                  m_poRegion->GetMapInfoType() == TAB_GEOM_V450_REGION_C ||
                  m_poRegion->GetMapInfoType() == TAB_GEOM_V800_REGION ||
                  m_poRegion->GetMapInfoType() == TAB_GEOM_V800_REGION_C );

        TABMAPObjPLine *poRegionHdr = cpl::down_cast<TABMAPObjPLine *>(
            TABMAPObjHdr::NewObj(m_poRegion->GetMapInfoType(), -1));

        // Update count of objects by type in header
        if (!bCoordBlockDataOnly)
            poMapFile->UpdateMapHeaderInfo(m_poRegion->GetMapInfoType());

        // Write a placeholder for centroid/label point and MBR mini-header
        // and we'll come back later to write the real values.
        //
        // Note that the call to WriteGeometryToMAPFile() below will call
        // StartNewFeature() as well, so we need to track the current
        // value before calling it

        poCoordBlock->StartNewFeature();
        int nMiniHeaderPtr = poCoordBlock->GetCurAddress();

        // In V800 the mini-header starts with a copy of num_parts
        if (nVersion >= 800)
        {
            poCoordBlock->WriteInt32(0);
        }
        WriteLabelAndMBR(poCoordBlock, bCompressed,
                         0, 0, 0, 0, 0, 0);
        nTotalFeatureDataSize += poCoordBlock->GetFeatureDataSize();

        if (m_poRegion->WriteGeometryToMAPFile(poMapFile, poRegionHdr,
                                               bCoordBlockDataOnly,
                                               &poCoordBlock) != 0)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed writing Region part in collection.");
            delete poRegionHdr;
            return -1;
        }

        nTotalFeatureDataSize += poRegionHdr->m_nCoordDataSize;

        // Come back to write the real values in the mini-header
        int nEndOfObjectPtr = poCoordBlock->GetCurAddress();
        poCoordBlock->StartNewFeature();

        if (poCoordBlock->GotoByteInFile(nMiniHeaderPtr, TRUE, TRUE) != 0)
        {
            delete poRegionHdr;
            return -1;
        }

        // In V800 the mini-header starts with a copy of num_parts
        if (nVersion >= 800)
        {
            poCoordBlock->WriteInt32(poRegionHdr->m_numLineSections);
        }
        WriteLabelAndMBR(poCoordBlock, bCompressed,
                         poRegionHdr->m_nMinX, poRegionHdr->m_nMinY,
                         poRegionHdr->m_nMaxX, poRegionHdr->m_nMaxY,
                         poRegionHdr->m_nLabelX, poRegionHdr->m_nLabelY);

        // And finally move the pointer back to the end of this component
        if (poCoordBlock->GotoByteInFile(nEndOfObjectPtr, TRUE, TRUE) != 0)
        {
            delete poRegionHdr;
            return -1;
        }

        // Copy other header members to the main collection header
        // TODO: Does m_nRegionDataSize need to include the centroid+mbr
        //       mini-header???
        poCollHdr->m_nRegionDataSize = poRegionHdr->m_nCoordDataSize;
        poCollHdr->m_nNumRegSections = poRegionHdr->m_numLineSections;

        if (!bCoordBlockDataOnly)
        {
            poCollHdr->m_nRegionPenId    = poRegionHdr->m_nPenId;
            poCollHdr->m_nRegionBrushId  = poRegionHdr->m_nBrushId;
            // TODO: Smooth flag         = poRegionHdr->m_bSmooth;
        }

        delete poRegionHdr;
    }
    else
    {
        // No Region component. Set corresponding header fields to 0

        poCollHdr->m_nRegionDataSize = 0;
        poCollHdr->m_nNumRegSections = 0;
        poCollHdr->m_nRegionPenId = 0;
        poCollHdr->m_nRegionBrushId = 0;
    }

    /*-----------------------------------------------------------------
     * PLine component
     *----------------------------------------------------------------*/
    if (m_poPline && m_poPline->GetMapInfoType() != TAB_GEOM_NONE)
    {
        CPLAssert(m_poPline->GetMapInfoType() == TAB_GEOM_V450_MULTIPLINE ||
                  m_poPline->GetMapInfoType() == TAB_GEOM_V450_MULTIPLINE_C ||
                  m_poPline->GetMapInfoType() == TAB_GEOM_V800_MULTIPLINE ||
                  m_poPline->GetMapInfoType() == TAB_GEOM_V800_MULTIPLINE_C );

        TABMAPObjPLine *poPlineHdr = cpl::down_cast<TABMAPObjPLine *>(
            TABMAPObjHdr::NewObj(m_poPline->GetMapInfoType(), -1));

        // Update count of objects by type in header
        if (!bCoordBlockDataOnly)
            poMapFile->UpdateMapHeaderInfo(m_poPline->GetMapInfoType());

        // Write a placeholder for centroid/label point and MBR mini-header
        // and we'll come back later to write the real values.
        //
        // Note that the call to WriteGeometryToMAPFile() below will call
        // StartNewFeature() as well, so we need to track the current
        // value before calling it

        poCoordBlock->StartNewFeature();
        int nMiniHeaderPtr = poCoordBlock->GetCurAddress();

        // In V800 the mini-header starts with a copy of num_parts
        if (nVersion >= 800)
        {
            poCoordBlock->WriteInt32(0);
        }
        WriteLabelAndMBR(poCoordBlock, bCompressed,
                         0, 0, 0, 0, 0, 0);
        nTotalFeatureDataSize += poCoordBlock->GetFeatureDataSize();

        if (m_poPline->WriteGeometryToMAPFile(poMapFile, poPlineHdr,
                                              bCoordBlockDataOnly,
                                              &poCoordBlock) != 0)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed writing Region part in collection.");
            delete poPlineHdr;
            return -1;
        }

        nTotalFeatureDataSize += poPlineHdr->m_nCoordDataSize;

        // Come back to write the real values in the mini-header
        int nEndOfObjectPtr = poCoordBlock->GetCurAddress();
        poCoordBlock->StartNewFeature();

        if (poCoordBlock->GotoByteInFile(nMiniHeaderPtr, TRUE, TRUE) != 0)
        {
            delete poPlineHdr;
            return -1;
        }

        // In V800 the mini-header starts with a copy of num_parts
        if (nVersion >= 800)
        {
            poCoordBlock->WriteInt32(poPlineHdr->m_numLineSections);
        }
        WriteLabelAndMBR(poCoordBlock, bCompressed,
                         poPlineHdr->m_nMinX, poPlineHdr->m_nMinY,
                         poPlineHdr->m_nMaxX, poPlineHdr->m_nMaxY,
                         poPlineHdr->m_nLabelX, poPlineHdr->m_nLabelY);

        // And finally move the pointer back to the end of this component
        if (poCoordBlock->GotoByteInFile(nEndOfObjectPtr, TRUE, TRUE) != 0)
        {
            delete poPlineHdr;
            return -1;
        }

        // Copy other header members to the main collection header
        // TODO: Does m_nRegionDataSize need to include the centroid+mbr
        //       mini-header???
        poCollHdr->m_nPolylineDataSize = poPlineHdr->m_nCoordDataSize;
        poCollHdr->m_nNumPLineSections = poPlineHdr->m_numLineSections;
        if (!bCoordBlockDataOnly)
        {
            poCollHdr->m_nPolylinePenId    = poPlineHdr->m_nPenId;
            // TODO: Smooth flag           = poPlineHdr->m_bSmooth;
        }

        delete poPlineHdr;
    }
    else
    {
        // No Polyline component. Set corresponding header fields to 0

        poCollHdr->m_nPolylineDataSize = 0;
        poCollHdr->m_nNumPLineSections = 0;
        poCollHdr->m_nPolylinePenId = 0;
    }

    /*-----------------------------------------------------------------
     * MultiPoint component
     *----------------------------------------------------------------*/
    if (m_poMpoint && m_poMpoint->GetMapInfoType() != TAB_GEOM_NONE)
    {
        CPLAssert(m_poMpoint->GetMapInfoType() == TAB_GEOM_MULTIPOINT ||
                  m_poMpoint->GetMapInfoType() == TAB_GEOM_MULTIPOINT_C ||
                  m_poMpoint->GetMapInfoType() == TAB_GEOM_V800_MULTIPOINT ||
                  m_poMpoint->GetMapInfoType() == TAB_GEOM_V800_MULTIPOINT_C );

        TABMAPObjMultiPoint *poMpointHdr = cpl::down_cast<TABMAPObjMultiPoint *>(
            TABMAPObjHdr::NewObj(m_poMpoint->GetMapInfoType(), -1));

        // Update count of objects by type in header
        if (!bCoordBlockDataOnly)
            poMapFile->UpdateMapHeaderInfo(m_poMpoint->GetMapInfoType());

        // Write a placeholder for centroid/label point and MBR mini-header
        // and we'll come back later to write the real values.
        //
        // Note that the call to WriteGeometryToMAPFile() below will call
        // StartNewFeature() as well, so we need to track the current
        // value before calling it

        poCoordBlock->StartNewFeature();
        int nMiniHeaderPtr = poCoordBlock->GetCurAddress();

        WriteLabelAndMBR(poCoordBlock, bCompressed,
                         0, 0, 0, 0, 0, 0);
        nTotalFeatureDataSize += poCoordBlock->GetFeatureDataSize();

        if (m_poMpoint->WriteGeometryToMAPFile(poMapFile, poMpointHdr,
                                               bCoordBlockDataOnly,
                                               &poCoordBlock) != 0)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed writing Region part in collection.");
            delete poMpointHdr;
            return -1;
        }

        nTotalFeatureDataSize += poMpointHdr->m_nCoordDataSize;

        // Come back to write the real values in the mini-header
        int nEndOfObjectPtr = poCoordBlock->GetCurAddress();
        poCoordBlock->StartNewFeature();

        if (poCoordBlock->GotoByteInFile(nMiniHeaderPtr, TRUE, TRUE) != 0)
        {
            delete poMpointHdr;
            return -1;
        }

        WriteLabelAndMBR(poCoordBlock, bCompressed,
                         poMpointHdr->m_nMinX, poMpointHdr->m_nMinY,
                         poMpointHdr->m_nMaxX, poMpointHdr->m_nMaxY,
                         poMpointHdr->m_nLabelX, poMpointHdr->m_nLabelY);

        // And finally move the pointer back to the end of this component
        if (poCoordBlock->GotoByteInFile(nEndOfObjectPtr, TRUE, TRUE) != 0)
        {
            delete poMpointHdr;
            return -1;
        }

        // Copy other header members to the main collection header
        // TODO: Does m_nRegionDataSize need to include the centroid+mbr
        //       mini-header???
        poCollHdr->m_nMPointDataSize     = poMpointHdr->m_nCoordDataSize;
        poCollHdr->m_nNumMultiPoints     = poMpointHdr->m_nNumPoints;
        if (!bCoordBlockDataOnly)
        {
            poCollHdr->m_nMultiPointSymbolId = poMpointHdr->m_nSymbolId;
        }

        delete poMpointHdr;
    }
    else
    {
        // No Multipoint component. Set corresponding header fields to 0

        poCollHdr->m_nMPointDataSize = 0;
        poCollHdr->m_nNumMultiPoints = 0;
        poCollHdr->m_nMultiPointSymbolId = 0;
    }

    /*-----------------------------------------------------------------
     * Copy object information
     *----------------------------------------------------------------*/

    // Compressed coordinate origin (useful only in compressed case!)
    poCollHdr->m_nComprOrgX = m_nComprOrgX;
    poCollHdr->m_nComprOrgY = m_nComprOrgY;

    poCollHdr->m_nCoordDataSize = nTotalFeatureDataSize;

    poCollHdr->SetMBR(m_nXMin, m_nYMin, m_nXMax, m_nYMax);

    if (CPLGetLastErrorType() == CE_Failure)
        return -1;

    /* Return a ref to coord block so that caller can continue writing
     * after the end of this object (used by index splitting)
     */
    if (ppoCoordBlock)
        *ppoCoordBlock = poCoordBlock;

    return 0;
}

/**********************************************************************
 *                   TABCollection::SyncOGRGeometryCollection()
 *
 * Copy the region/pline/multipoint's geometries to the OGRFeature's
 * geometry.
 **********************************************************************/
int    TABCollection::SyncOGRGeometryCollection(GBool bSyncRegion,
                                                GBool bSyncPline,
                                                GBool bSyncMpoint)
{
    OGRGeometry         *poThisGeom = GetGeometryRef();
    OGRGeometryCollection *poGeomColl = nullptr;

    // poGeometry is defined in the OGRFeature class
    if (poThisGeom == nullptr)
    {
        poGeomColl = new OGRGeometryCollection();
    }
    else if (wkbFlatten(poThisGeom->getGeometryType())==wkbGeometryCollection)
    {
         poGeomColl = poThisGeom->toGeometryCollection();
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABCollection: Invalid Geometry. Type must be OGRCollection.");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Start by removing geometries that need to be replaced
     * In theory there should be a single geometry of each type, but
     * just in case, we'll loop over the whole collection and delete all
     * instances of each type if there are some.
     *----------------------------------------------------------------*/
    int numGeometries = poGeomColl->getNumGeometries();
    for (int i=0; i<numGeometries; i++)
    {
        OGRGeometry *poGeom = poGeomColl->getGeometryRef(i);
        if (!poGeom)
            continue;

        if ( (bSyncRegion &&
              (wkbFlatten(poGeom->getGeometryType()) == wkbPolygon ||
               wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon) ) ||
             (bSyncPline &&
              (wkbFlatten(poGeom->getGeometryType()) == wkbLineString ||
               wkbFlatten(poGeom->getGeometryType()) == wkbMultiLineString)) ||
             (bSyncMpoint &&
              (wkbFlatten(poGeom->getGeometryType()) == wkbMultiPoint) ) )
        {
            // Remove this geometry
            poGeomColl->removeGeometry(i);

            // Unless this was the last geometry, we need to restart
            // scanning the collection since we modified it
            if (i != numGeometries-1)
            {
                i=0;
                numGeometries = poGeomColl->getNumGeometries();
            }
        }
    }

    /*-----------------------------------------------------------------
     * Copy TAB Feature geometries to OGRGeometryCollection
     *----------------------------------------------------------------*/
    if(bSyncRegion && m_poRegion && m_poRegion->GetGeometryRef() != nullptr)
        poGeomColl->addGeometry(m_poRegion->GetGeometryRef());

    if(bSyncPline && m_poPline && m_poPline->GetGeometryRef() != nullptr)
        poGeomColl->addGeometry(m_poPline->GetGeometryRef());

    if(bSyncMpoint && m_poMpoint && m_poMpoint->GetGeometryRef() != nullptr)
        poGeomColl->addGeometry(m_poMpoint->GetGeometryRef());

    if( poThisGeom == nullptr )
        SetGeometryDirectly(poGeomColl);

    return 0;
}

/**********************************************************************
 *                   TABCollection::SetRegionDirectly()
 *
 * Set the region component of the collection, deleting the current
 * region component if there is one. The object is then owned by the
 * TABCollection object. Passing NULL just deletes it.
 *
 * Note that an intentional side-effect is that calling this method
 * with the same poRegion pointer that is already owned by this object
 * will force resync'ing the OGR Geometry member.
 **********************************************************************/
int    TABCollection::SetRegionDirectly(TABRegion *poRegion)
{
    if (m_poRegion && m_poRegion != poRegion)
        delete m_poRegion;
    m_poRegion = poRegion;

    // Update OGRGeometryCollection component as well
    return SyncOGRGeometryCollection(TRUE, FALSE, FALSE);
}

/**********************************************************************
 *                   TABCollection::SetPolylineDirectly()
 *
 * Set the polyline component of the collection, deleting the current
 * polyline component if there is one. The object is then owned by the
 * TABCollection object. Passing NULL just deletes it.
 *
 * Note that an intentional side-effect is that calling this method
 * with the same poPline pointer that is already owned by this object
 * will force resync'ing the OGR Geometry member.
 **********************************************************************/
int    TABCollection::SetPolylineDirectly(TABPolyline *poPline)
{
    if (m_poPline && m_poPline != poPline)
        delete m_poPline;
    m_poPline = poPline;

    // Update OGRGeometryCollection component as well
    return SyncOGRGeometryCollection(FALSE, TRUE, FALSE);
}

/**********************************************************************
 *                   TABCollection::SetMultiPointDirectly()
 *
 * Set the multipoint component of the collection, deleting the current
 * multipoint component if there is one. The object is then owned by the
 * TABCollection object. Passing NULL just deletes it.
 *
 * Note that an intentional side-effect is that calling this method
 * with the same poMpoint pointer that is already owned by this object
 * will force resync'ing the OGR Geometry member.
 **********************************************************************/
int    TABCollection::SetMultiPointDirectly(TABMultiPoint *poMpoint)
{
    if (m_poMpoint && m_poMpoint != poMpoint)
        delete m_poMpoint;
    m_poMpoint = poMpoint;

    // Update OGRGeometryCollection component as well
    return SyncOGRGeometryCollection(FALSE, FALSE, TRUE);
}

/**********************************************************************
 *                   TABCollection::GetStyleString() const
 *
 * Return style string for this feature.
 *
 * Style String is built only once during the first call to GetStyleString().
 **********************************************************************/
const char *TABCollection::GetStyleString() const
{
    if (m_pszStyleString == nullptr)
    {
        m_pszStyleString = CPLStrdup(GetSymbolStyleString());
    }

    return m_pszStyleString;
}

/**********************************************************************
 *                   TABCollection::DumpMIF()
 *
 * Dump feature geometry
 **********************************************************************/
void TABCollection::DumpMIF(FILE *fpOut /*=NULL*/)
{
    if (fpOut == nullptr)
        fpOut = stdout;

    /*-----------------------------------------------------------------
     * Generate output
     *----------------------------------------------------------------*/
    int numParts = 0;
    if (m_poRegion)     numParts++;
    if (m_poPline)      numParts++;
    if (m_poMpoint)     numParts++;

    fprintf(fpOut, "COLLECTION %d\n", numParts);

    if (m_poRegion)
        m_poRegion->DumpMIF(fpOut);

    if (m_poPline)
        m_poPline->DumpMIF(fpOut);

    if (m_poMpoint)
        m_poMpoint->DumpMIF(fpOut);

    DumpSymbolDef(fpOut);

    fflush(fpOut);
}

/*=====================================================================
 *                      class TABDebugFeature
 *====================================================================*/

/**********************************************************************
 *                   TABDebugFeature::TABDebugFeature()
 *
 * Constructor.
 **********************************************************************/
TABDebugFeature::TABDebugFeature( OGRFeatureDefn *poDefnIn ) :
    TABFeature(poDefnIn),
    m_nSize(0),
    m_nCoordDataPtr(0),
    m_nCoordDataSize(0)
{
    memset( m_abyBuf, 0, sizeof(m_abyBuf) );
}

/**********************************************************************
 *                   TABDebugFeature::~TABDebugFeature()
 *
 * Destructor.
 **********************************************************************/
TABDebugFeature::~TABDebugFeature() {}

/**********************************************************************
 *                   TABDebugFeature::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABDebugFeature::ReadGeometryFromMAPFile(TABMAPFile *poMapFile,
                                             TABMAPObjHdr *poObjHdr,
                                             GBool /*bCoordBlockDataOnly=FALSE*/,
                                             TABMAPCoordBlock ** /*ppoCoordBlock=NULL*/)
{
    /*-----------------------------------------------------------------
     * Fetch geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poObjHdr->m_nType;

    TABMAPObjectBlock *poObjBlock = poMapFile->GetCurObjBlock();
    TABMAPHeaderBlock *poHeader = poMapFile->GetHeaderBlock();

    /*-----------------------------------------------------------------
     * If object type has coords in a type 3 block, then its position
     * follows
     *----------------------------------------------------------------*/
    if (poHeader->MapObjectUsesCoordBlock(m_nMapInfoType))
    {
        m_nCoordDataPtr = poObjBlock->ReadInt32();
        m_nCoordDataSize = poObjBlock->ReadInt32();
    }
    else
    {
        m_nCoordDataPtr = -1;
        m_nCoordDataSize = 0;
    }

    m_nSize = poHeader->GetMapObjectSize(m_nMapInfoType);
    if (m_nSize > 0)
    {
        poObjBlock->GotoByteRel(-5);    // Go back to beginning of header
        poObjBlock->ReadBytes(
           std::min(m_nSize,
                    static_cast<int>(sizeof(m_abyBuf))),
           m_abyBuf);
    }

    return 0;
}

/**********************************************************************
 *                   TABDebugFeature::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABDebugFeature::WriteGeometryToMAPFile(TABMAPFile * /*poMapFile*/,
                                            TABMAPObjHdr * /*poObjHdr*/,
                                            GBool /*bCoordBlockDataOnly=FALSE*/,
                                            TABMAPCoordBlock ** /*ppoCoordBlock=NULL*/)
{
    // Nothing to do here!

    CPLError(CE_Failure, CPLE_NotSupported,
             "TABDebugFeature::WriteGeometryToMAPFile() not implemented.\n");

    return -1;
}

/**********************************************************************
 *                   TABDebugFeature::DumpMIF()
 *
 * Dump feature contents... available only in DEBUG mode.
 **********************************************************************/
void TABDebugFeature::DumpMIF(FILE *fpOut /*=NULL*/)
{
    if (fpOut == nullptr)
        fpOut = stdout;

    fprintf(fpOut, "----- TABDebugFeature (type = 0x%2.2x) -----\n",
            GetMapInfoType());
    fprintf(fpOut, "  Object size: %d bytes\n", m_nSize);
    fprintf(fpOut, "  m_nCoordDataPtr  = %d\n", m_nCoordDataPtr);
    fprintf(fpOut, "  m_nCoordDataSize = %d\n", m_nCoordDataSize);
    fprintf(fpOut, "  ");

    for( int i = 0; i < m_nSize; i++ )
        fprintf(fpOut, " %2.2x", m_abyBuf[i]);

    fprintf(fpOut, "  \n");

    fflush(fpOut);
}

/*=====================================================================
 *                      class ITABFeaturePen
 *====================================================================*/

/**********************************************************************
 *                   ITABFeaturePen::ITABFeaturePen()
 **********************************************************************/

// MI default is PEN(1, 2, 0)
static const TABPenDef csDefaultPen = MITAB_PEN_DEFAULT;

ITABFeaturePen::ITABFeaturePen() :
    m_nPenDefIndex(-1),
    m_sPenDef(csDefaultPen)
{}

/**********************************************************************
 *                   ITABFeaturePen::GetPenWidthPixel()
 *                   ITABFeaturePen::SetPenWidthPixel()
 *                   ITABFeaturePen::GetPenWidthPoint()
 *                   ITABFeaturePen::SetPenWidthPoint()
 *
 * Pen width can be expressed in pixels (value from 1 to 7 pixels) or
 * in points (value from 0.1 to 203.7 points). The default pen width
 * in MapInfo is 1 pixel.  Pen width in points exist only in file version 450.
 *
 * The following methods hide the way the pen width is stored in the files.
 *
 * In order to establish if a given pen def had its width specified in
 * pixels or in points, one should first call GetPenWidthPoint(), and if
 * it returns 0 then the Pixel width should be used instead:
 *    if (GetPenWidthPoint() == 0)
 *       ... use pen width in points ...
 *    else
 *       ... use Pixel width from GetPenWidthPixel()
 *
 * Note that the reverse is not true: the default pixel width is always 1,
 * even when the pen width was actually set in points.
 **********************************************************************/

GByte ITABFeaturePen::GetPenWidthPixel() const
{
    return m_sPenDef.nPixelWidth;
}

void  ITABFeaturePen::SetPenWidthPixel(GByte val)
{
    const GByte nPixelWidthMin = 1;
    const GByte nPixelWidthMax = 7;
    m_sPenDef.nPixelWidth =
        std::min(std::max(val, nPixelWidthMin), nPixelWidthMax);
    m_sPenDef.nPointWidth = 0;
}

double ITABFeaturePen::GetPenWidthPoint() const
{
    // We store point width internally as tenths of points
    return m_sPenDef.nPointWidth/10.0;
}

void  ITABFeaturePen::SetPenWidthPoint(double val)
{
    m_sPenDef.nPointWidth =
         std::min(std::max(static_cast<int>(val*10), 1), 2037);
    m_sPenDef.nPixelWidth = 1;
}

/**********************************************************************
 *                   ITABFeaturePen::GetPenWidthMIF()
 *                   ITABFeaturePen::SetPenWidthMIF()
 *
 * The MIF representation for pen width is either a value from 1 to 7
 * for a pen width in pixels, or a value from 11 to 2047 for a pen
 * width in points = 10 + (point_width*10)
 **********************************************************************/
int     ITABFeaturePen::GetPenWidthMIF() const
{
    return ( m_sPenDef.nPointWidth > 0?
             (m_sPenDef.nPointWidth+10): m_sPenDef.nPixelWidth );
}

void ITABFeaturePen::SetPenWidthMIF( int val )
{
    if (val > 10)
    {
        m_sPenDef.nPointWidth = std::min((val-10), 2037);
        m_sPenDef.nPixelWidth = 0;
    }
    else
    {
        m_sPenDef.nPixelWidth = static_cast<GByte>(std::min(std::max(val, 1), 7));
        m_sPenDef.nPointWidth = 0;
    }
}

/**********************************************************************
 *                   ITABFeaturePen::GetPenStyleString()
 *
 *  Return a PEN() string. All representations info for the pen are here.
 **********************************************************************/
const char *ITABFeaturePen::GetPenStyleString() const
{
    const char *pszStyle = nullptr;
    int    nOGRStyle  = 0;
    char szPattern[20];

    szPattern[0] = '\0';

    // For now, I only add the 25 first styles
    switch (GetPenPattern())
    {
      case 1:
        nOGRStyle =1;
        break;
      case 2:
        nOGRStyle = 0;
        break;
      case 3:
        nOGRStyle = 3;
        strcpy(szPattern,"1 1");
        break;
      case 4:
        nOGRStyle = 3;
        strcpy(szPattern,"2 1");
        break;
      case 5:
        nOGRStyle = 3;
        strcpy(szPattern,"3 1");
        break;
      case 6:
        nOGRStyle = 3;
        strcpy(szPattern,"6 1");
        break;
      case 7:
        nOGRStyle = 4;
        strcpy(szPattern,"12 2");
        break;
      case 8:
        nOGRStyle = 4;
        strcpy(szPattern,"24 4");
        break;
      case 9:
        nOGRStyle = 3;
        strcpy(szPattern,"4 3");
        break;
      case 10:
        nOGRStyle = 5;
        strcpy(szPattern,"1 4");
        break;
      case 11:
        nOGRStyle = 3;
        strcpy(szPattern,"4 6");
        break;
      case 12:
        nOGRStyle = 3;
        strcpy(szPattern,"6 4");
        break;
      case 13:
        nOGRStyle = 4;
        strcpy(szPattern,"12 12");
        break;
      case 14:
        nOGRStyle = 6;
        strcpy(szPattern,"8 2 1 2");
        break;
      case 15:
        nOGRStyle = 6;
        strcpy(szPattern,"12 1 1 1");
        break;
      case 16:
        nOGRStyle = 6;
        strcpy(szPattern,"12 1 3 1");
        break;
      case 17:
        nOGRStyle = 6;
        strcpy(szPattern,"24 6 4 6");
        break;
      case 18:
        nOGRStyle = 7;
        strcpy(szPattern,"24 3 3 3 3 3");
        break;
      case 19:
        nOGRStyle = 7;
        strcpy(szPattern,"24 3 3 3 3 3 3 3");
        break;
      case 20:
        nOGRStyle = 7;
        strcpy(szPattern,"6 3 1 3 1 3");
        break;
      case 21:
        nOGRStyle = 7;
        strcpy(szPattern,"12 2 1 2 1 2");
        break;
      case 22:
        nOGRStyle = 7;
        strcpy(szPattern,"12 2 1 2 1 2 1 2");
        break;
      case 23:
        nOGRStyle = 6;
        strcpy(szPattern,"4 1 1 1");
        break;
      case 24:
        nOGRStyle = 7;
        strcpy(szPattern,"4 1 1 1 1");
        break;
      case 25:
        nOGRStyle = 6;
        strcpy(szPattern,"4 1 1 1 2 1 1 1");
        break;

        default:
        nOGRStyle = 0;
        break;
    }

    // note - MapInfo renders all lines using a round pen cap and round pen join
    // which are not the default values for OGR pen cap/join styles. So we need to explicitly
    // include the cap/j parameters in these strings
    if (strlen(szPattern) != 0)
    {
      if(m_sPenDef.nPointWidth > 0)
        pszStyle =CPLSPrintf("PEN(w:%dpt,c:#%6.6x,id:\"mapinfo-pen-%d,"
                             "ogr-pen-%d\",p:\"%spx\",cap:r,j:r)",
                             static_cast<int>(GetPenWidthPoint()),
                             m_sPenDef.rgbColor,GetPenPattern(),nOGRStyle,
                             szPattern);
      else
        pszStyle =CPLSPrintf("PEN(w:%dpx,c:#%6.6x,id:\"mapinfo-pen-%d,"
                             "ogr-pen-%d\",p:\"%spx\",cap:r,j:r)",
                             GetPenWidthPixel(),
                             m_sPenDef.rgbColor,GetPenPattern(),nOGRStyle,
                             szPattern);
    }
    else
    {
      if(m_sPenDef.nPointWidth > 0)
        pszStyle =CPLSPrintf("PEN(w:%dpt,c:#%6.6x,id:\""
                             "mapinfo-pen-%d,ogr-pen-%d\",cap:r,j:r)",
                             static_cast<int>(GetPenWidthPoint()),
                             m_sPenDef.rgbColor,GetPenPattern(),nOGRStyle);
      else
        pszStyle =CPLSPrintf("PEN(w:%dpx,c:#%6.6x,id:\""
                             "mapinfo-pen-%d,ogr-pen-%d\",cap:r,j:r)",
                             GetPenWidthPixel(),
                             m_sPenDef.rgbColor,GetPenPattern(),nOGRStyle);
    }

    return pszStyle;
}

/**********************************************************************
 *                   ITABFeaturePen::SetPenFromStyleString()
 *
 *  Init the Pen properties from a style string.
 **********************************************************************/
void  ITABFeaturePen::SetPenFromStyleString(const char *pszStyleString)
{
    GBool bIsNull = 0;

    // Use the Style Manager to retrieve all the information we need.
    OGRStyleMgr *poStyleMgr = new OGRStyleMgr(nullptr);
    OGRStyleTool *poStylePart = nullptr;

    // Init the StyleMgr with the StyleString.
    poStyleMgr->InitStyleString(pszStyleString);

    // Retrieve the Pen info.
    const int numParts = poStyleMgr->GetPartCount();
    for( int i = 0; i < numParts; i++ )
    {
        poStylePart = poStyleMgr->GetPart(i);
        if( poStylePart == nullptr )
            continue;

        if(poStylePart->GetType() == OGRSTCPen)
        {
            break;
        }
        else
        {
            delete poStylePart;
            poStylePart = nullptr;
        }
    }

    // If the no Pen found, do nothing.
    if(poStylePart == nullptr)
    {
        delete poStyleMgr;
        return;
    }

    OGRStylePen *poPenStyle = cpl::down_cast<OGRStylePen*>(poStylePart);

    // With Pen, we always want to output points or pixels (which are the same,
    // so just use points).
    //
    // It's very important to set the output unit of the feature.
    // The default value is meter. If we don't do it all numerical values
    // will be assumed to be converted from the input unit to meter when we
    // will get them via GetParam...() functions.
    // See OGRStyleTool::Parse() for more details.
    poPenStyle->SetUnit(OGRSTUPoints, 1);

    // Get the Pen Id or pattern
    const char *pszPenName = poPenStyle->Id(bIsNull);
    if (bIsNull) pszPenName = nullptr;

    // Set the width
    if(poPenStyle->Width(bIsNull) != 0.0)
    {
        const double nPenWidth = poPenStyle->Width(bIsNull);
        // Width < 10 is a pixel
        if(nPenWidth > 10)
            SetPenWidthPoint(nPenWidth);
        else
            SetPenWidthPixel(static_cast<GByte>(nPenWidth));
    }

    //Set the color
    const char *pszPenColor = poPenStyle->Color(bIsNull);
    if(pszPenColor != nullptr)
    {
        if(pszPenColor[0] == '#')
            pszPenColor++;
        // The Pen color is an Hexa string that need to be convert in a int
        const GInt32 nPenColor =
            static_cast<int>(strtol(pszPenColor, nullptr, 16));
        SetPenColor(nPenColor);
    }

    const char *pszPenPattern = nullptr;

    // Set the Id of the Pen, use Pattern if necessary.
    if(pszPenName &&
       (strstr(pszPenName, "mapinfo-pen-") || strstr(pszPenName, "ogr-pen-")) )
    {
        const char* pszPenId = strstr(pszPenName, "mapinfo-pen-");
        if( pszPenId != nullptr )
        {
            const int nPenId = atoi(pszPenId+12);
            SetPenPattern(static_cast<GByte>(nPenId));
        }
        else
        {
            pszPenId = strstr(pszPenName, "ogr-pen-");
            if( pszPenId != nullptr )
            {
                int nPenId = atoi(pszPenId+8);
                if(nPenId == 0)
                    nPenId = 2;
                SetPenPattern(static_cast<GByte>(nPenId));
            }
        }
    }
    else
    {
        // If no Pen Id, use the Pen Pattern to retrieve the Id.
        pszPenPattern = poPenStyle->Pattern(bIsNull);
        if (bIsNull)
            pszPenPattern = nullptr;
        else
        {
            if(strcmp(pszPenPattern, "1 1") == 0)
                SetPenPattern(3);
            else if(strcmp(pszPenPattern, "2 1") == 0)
                SetPenPattern(4);
            else if(strcmp(pszPenPattern, "3 1") == 0)
                SetPenPattern(5);
            else if(strcmp(pszPenPattern, "6 1") == 0)
                SetPenPattern(6);
            else if(strcmp(pszPenPattern, "12 2") == 0)
                SetPenPattern(7);
            else if(strcmp(pszPenPattern, "24 4") == 0)
                SetPenPattern(8);
            else if(strcmp(pszPenPattern, "4 3") == 0)
                SetPenPattern(9);
            else if(strcmp(pszPenPattern, "1 4") == 0)
                SetPenPattern(10);
            else if(strcmp(pszPenPattern, "4 6") == 0)
                SetPenPattern(11);
            else if(strcmp(pszPenPattern, "6 4") == 0)
                SetPenPattern(12);
            else if(strcmp(pszPenPattern, "12 12") == 0)
                SetPenPattern(13);
            else if(strcmp(pszPenPattern, "8 2 1 2") == 0)
                SetPenPattern(14);
            else if(strcmp(pszPenPattern, "12 1 1 1") == 0)
                SetPenPattern(15);
            else if(strcmp(pszPenPattern, "12 1 3 1") == 0)
                SetPenPattern(16);
            else if(strcmp(pszPenPattern, "24 6 4 6") == 0)
                SetPenPattern(17);
            else if(strcmp(pszPenPattern, "24 3 3 3 3 3") == 0)
                SetPenPattern(18);
            else if(strcmp(pszPenPattern, "24 3 3 3 3 3 3 3") == 0)
                SetPenPattern(19);
            else if(strcmp(pszPenPattern, "6 3 1 3 1 3") == 0)
                SetPenPattern(20);
            else if(strcmp(pszPenPattern, "12 2 1 2 1 2") == 0)
                SetPenPattern(21);
            else if(strcmp(pszPenPattern, "12 2 1 2 1 2 1 2") == 0)
                SetPenPattern(22);
            else if(strcmp(pszPenPattern, "4 1 1 1") == 0)
                SetPenPattern(23);
            else if(strcmp(pszPenPattern, "4 1 1 1 1") == 0)
                SetPenPattern(24);
            else if(strcmp(pszPenPattern, "4 1 1 1 2 1 1 1") == 0)
                SetPenPattern(25);
        }
    }

    delete poStyleMgr;
    delete poStylePart;

    return;
}

/**********************************************************************
 *                   ITABFeaturePen::DumpPenDef()
 *
 * Dump pen definition information.
 **********************************************************************/
void ITABFeaturePen::DumpPenDef(FILE *fpOut /*=NULL*/)
{
    if (fpOut == nullptr)
        fpOut = stdout;

    fprintf(fpOut, "  m_nPenDefIndex         = %d\n", m_nPenDefIndex);
    fprintf(fpOut, "  m_sPenDef.nRefCount    = %d\n", m_sPenDef.nRefCount);
    fprintf(fpOut, "  m_sPenDef.nPixelWidth  = %u\n", m_sPenDef.nPixelWidth);
    fprintf(fpOut, "  m_sPenDef.nLinePattern = %u\n", m_sPenDef.nLinePattern);
    fprintf(fpOut, "  m_sPenDef.nPointWidth  = %d\n", m_sPenDef.nPointWidth);
    fprintf(fpOut, "  m_sPenDef.rgbColor     = 0x%6.6x (%d)\n",
                                     m_sPenDef.rgbColor, m_sPenDef.rgbColor);

    fflush(fpOut);
}

/*=====================================================================
 *                      class ITABFeatureBrush
 *====================================================================*/

/**********************************************************************
 *                   ITABFeatureBrush::ITABFeatureBrush()
 **********************************************************************/

// MI default is BRUSH(2, 16777215, 16777215)
static const TABBrushDef csDefaultBrush = MITAB_BRUSH_DEFAULT;

ITABFeatureBrush::ITABFeatureBrush() :
    m_nBrushDefIndex(-1),
    m_sBrushDef(csDefaultBrush)
{}

/**********************************************************************
 *                   ITABFeatureBrush::GetBrushStyleString()
 *
 *  Return a Brush() string. All representations info for the Brush are here.
 **********************************************************************/
const char *ITABFeatureBrush::GetBrushStyleString() const
{
    const char *pszStyle = nullptr;
    int    nOGRStyle  = 0;
    /* char szPattern[20]; */
    //* szPattern[0] = '\0'; */

    if (m_sBrushDef.nFillPattern == 1)
      nOGRStyle = 1;
    else if (m_sBrushDef.nFillPattern == 3)
      nOGRStyle = 2;
    else if (m_sBrushDef.nFillPattern == 4)
      nOGRStyle = 3;
    else if (m_sBrushDef.nFillPattern == 5)
      nOGRStyle = 5;
    else if (m_sBrushDef.nFillPattern == 6)
      nOGRStyle = 4;
    else if (m_sBrushDef.nFillPattern == 7)
      nOGRStyle = 6;
    else if (m_sBrushDef.nFillPattern == 8)
      nOGRStyle = 7;

    if (GetBrushTransparent())
    {
        /* Omit BG Color for transparent brushes */
        pszStyle =CPLSPrintf("BRUSH(fc:#%6.6x,id:\"mapinfo-brush-%d,ogr-brush-%d\")",
                             m_sBrushDef.rgbFGColor,
                             m_sBrushDef.nFillPattern,nOGRStyle);
    }
    else
    {
        pszStyle =CPLSPrintf("BRUSH(fc:#%6.6x,bc:#%6.6x,id:\"mapinfo-brush-%d,ogr-brush-%d\")",
                             m_sBrushDef.rgbFGColor,
                             m_sBrushDef.rgbBGColor,
                             m_sBrushDef.nFillPattern,nOGRStyle);
    }

     return pszStyle;
}

/**********************************************************************
 *                   ITABFeatureBrush::SetBrushFromStyleString()
 *
 *  Set all Brush elements from a StyleString.
 *  Use StyleMgr to do so.
 **********************************************************************/
void  ITABFeatureBrush::SetBrushFromStyleString(const char *pszStyleString)
{
    GBool bIsNull = 0;

    // Use the Style Manager to retrieve all the information we need.
    OGRStyleMgr *poStyleMgr = new OGRStyleMgr(nullptr);
    OGRStyleTool *poStylePart = nullptr;

    // Init the StyleMgr with the StyleString.
    poStyleMgr->InitStyleString(pszStyleString);

    // Retrieve the Brush info.
    const int numParts = poStyleMgr->GetPartCount();
    for( int i = 0; i < numParts; i++ )
    {
        poStylePart = poStyleMgr->GetPart(i);
        if( poStylePart == nullptr )
            continue;

        if(poStylePart->GetType() == OGRSTCBrush)
        {
            break;
        }
        else
        {
            delete poStylePart;
            poStylePart = nullptr;
        }
    }

    // If the no Brush found, do nothing.
    if(poStylePart == nullptr)
    {
        delete poStyleMgr;
        return;
    }

    OGRStyleBrush *poBrushStyle = cpl::down_cast<OGRStyleBrush*>(poStylePart);

    // Set the Brush Id (FillPattern)
    const char *pszBrushId = poBrushStyle->Id(bIsNull);
    if(bIsNull) pszBrushId = nullptr;
    bool bHasBrushId = false;

    if(pszBrushId &&
       (strstr(pszBrushId, "mapinfo-brush-") ||
        strstr(pszBrushId, "ogr-brush-")) )
    {
        if(strstr(pszBrushId, "mapinfo-brush-"))
        {
            const int nBrushId = atoi(pszBrushId+14);
            SetBrushPattern(static_cast<GByte>(nBrushId));
            bHasBrushId = true;
        }
        else if(strstr(pszBrushId, "ogr-brush-"))
        {
            int nBrushId = atoi(pszBrushId+10);
            if(nBrushId > 1)
                nBrushId++;
            SetBrushPattern(static_cast<GByte>(nBrushId));
            bHasBrushId = true;
        }
    }

    // Set the BackColor, if not set, then it is transparent
    const char *pszBrushColor = poBrushStyle->BackColor(bIsNull);
    if(bIsNull) pszBrushColor = nullptr;

    if(pszBrushColor)
    {
        if(pszBrushColor[0] == '#')
            pszBrushColor++;
        if( strlen(pszBrushColor) == 8 &&
            pszBrushColor[6] == '0' && pszBrushColor[7] == '0' )
        {
            SetBrushTransparent(1);
        }
        else
        {
            CPLString osBrushColor(pszBrushColor);
            if( strlen(pszBrushColor) > 6 )
                osBrushColor.resize(6);
            const int nBrushColor =
                static_cast<int>(strtol(osBrushColor, nullptr, 16));
            SetBrushBGColor(static_cast<GInt32>(nBrushColor));
        }
    }
    else
    {
        SetBrushTransparent(1);
    }

    // Set the ForeColor
    pszBrushColor = poBrushStyle->ForeColor(bIsNull);
    if(bIsNull) pszBrushColor = nullptr;

    if(pszBrushColor)
    {
        if(pszBrushColor[0] == '#')
            pszBrushColor++;
        if( strlen(pszBrushColor) == 8 &&
            pszBrushColor[6] == '0' && pszBrushColor[7] == '0' )
        {
            if( !bHasBrushId )
                SetBrushPattern(static_cast<GByte>(1)); // No-fill
        }
        else
        {
            if( !bHasBrushId )
                SetBrushPattern(static_cast<GByte>(2)); // Solid-fill
        }

        CPLString osBrushColor(pszBrushColor);
        if( strlen(pszBrushColor) > 6 )
            osBrushColor.resize(6);
        const int nBrushColor =
            static_cast<int>(strtol(osBrushColor, nullptr, 16));
        SetBrushFGColor(static_cast<GInt32>(nBrushColor));
    }

    delete poStyleMgr;
    delete poStylePart;

    return;
}

/**********************************************************************
 *                   ITABFeatureBrush::DumpBrushDef()
 *
 * Dump Brush definition information.
 **********************************************************************/
void ITABFeatureBrush::DumpBrushDef(FILE *fpOut /*=NULL*/)
{
    if (fpOut == nullptr)
        fpOut = stdout;

    fprintf(fpOut, "  m_nBrushDefIndex         = %d\n", m_nBrushDefIndex);
    fprintf(fpOut, "  m_sBrushDef.nRefCount    = %d\n", m_sBrushDef.nRefCount);
    fprintf(fpOut, "  m_sBrushDef.nFillPattern = %d\n",
                                static_cast<int>(m_sBrushDef.nFillPattern));
    fprintf(fpOut, "  m_sBrushDef.bTransparentFill = %d\n",
                                static_cast<int>(m_sBrushDef.bTransparentFill));
    fprintf(fpOut, "  m_sBrushDef.rgbFGColor   = 0x%6.6x (%d)\n",
                               m_sBrushDef.rgbFGColor, m_sBrushDef.rgbFGColor);
    fprintf(fpOut, "  m_sBrushDef.rgbBGColor   = 0x%6.6x (%d)\n",
                               m_sBrushDef.rgbBGColor, m_sBrushDef.rgbBGColor);

    fflush(fpOut);
}

/*=====================================================================
 *                      class ITABFeatureFont
 *====================================================================*/

/**********************************************************************
 *                   ITABFeatureFont::ITABFeatureFont()
 **********************************************************************/

// MI default is Font("Arial", 0, 0, 0)
static const TABFontDef csDefaultFont = MITAB_FONT_DEFAULT;

ITABFeatureFont::ITABFeatureFont() :
    m_nFontDefIndex(-1),
    m_sFontDef(csDefaultFont)
{}

/**********************************************************************
 *                   ITABFeatureFont::SetFontName()
 **********************************************************************/
void ITABFeatureFont::SetFontName(const char *pszName)
{
    strncpy( m_sFontDef.szFontName, pszName, sizeof(m_sFontDef.szFontName)-1);
    m_sFontDef.szFontName[sizeof(m_sFontDef.szFontName)-1] = '\0';
}

/**********************************************************************
 *                   ITABFeatureFont::DumpFontDef()
 *
 * Dump Font definition information.
 **********************************************************************/
void ITABFeatureFont::DumpFontDef(FILE *fpOut /*=NULL*/)
{
    if (fpOut == nullptr)
        fpOut = stdout;

    fprintf(fpOut, "  m_nFontDefIndex       = %d\n", m_nFontDefIndex);
    fprintf(fpOut, "  m_sFontDef.nRefCount  = %d\n", m_sFontDef.nRefCount);
    fprintf(fpOut, "  m_sFontDef.szFontName = '%s'\n", m_sFontDef.szFontName);

    fflush(fpOut);
}

/*=====================================================================
 *                      class ITABFeatureSymbol
 *====================================================================*/

/**********************************************************************
 *                   ITABFeatureSymbol::ITABFeatureSymbol()
 **********************************************************************/

// MI default is Symbol(35, 0, 12)
static const TABSymbolDef csDefaultSymbol = MITAB_SYMBOL_DEFAULT;

ITABFeatureSymbol::ITABFeatureSymbol() :
    m_nSymbolDefIndex(-1),
    m_sSymbolDef(csDefaultSymbol)
{}

/**********************************************************************
 *                   ITABFeatureSymbol::GetSymbolStyleString()
 *
 *  Return a Symbol() string. All representations info for the Symbol are here.
 **********************************************************************/
const char *ITABFeatureSymbol::GetSymbolStyleString(double dfAngle) const
{
    const char *pszStyle = nullptr;
    int    nOGRStyle  = 1;
    /* char szPattern[20]; */
    int nAngle = 0;
    /* szPattern[0] = '\0'; */

    if (m_sSymbolDef.nSymbolNo == 31)
      nOGRStyle = 0;
    else if (m_sSymbolDef.nSymbolNo == 32)
      nOGRStyle = 6;
    else if (m_sSymbolDef.nSymbolNo == 33)
    {
        nAngle = 45;
        nOGRStyle = 6;
    }
    else if (m_sSymbolDef.nSymbolNo == 34)
      nOGRStyle = 4;
    else if (m_sSymbolDef.nSymbolNo == 35)
      nOGRStyle = 10;
    else if (m_sSymbolDef.nSymbolNo == 36)
      nOGRStyle = 8;
    else if (m_sSymbolDef.nSymbolNo == 37)
    {
        nAngle = 180;
        nOGRStyle = 8;
    }
    else if (m_sSymbolDef.nSymbolNo == 38)
      nOGRStyle = 5;
    else if (m_sSymbolDef.nSymbolNo == 39)
    {
        nAngle = 45;
        nOGRStyle = 5;
    }
    else if (m_sSymbolDef.nSymbolNo == 40)
      nOGRStyle = 3;
    else if (m_sSymbolDef.nSymbolNo == 41)
      nOGRStyle = 9;
    else if (m_sSymbolDef.nSymbolNo == 42)
      nOGRStyle = 7;
    else if (m_sSymbolDef.nSymbolNo == 43)
    {
        nAngle = 180;
        nOGRStyle = 7;
    }
    else if (m_sSymbolDef.nSymbolNo == 44)
      nOGRStyle = 6;
    else if (m_sSymbolDef.nSymbolNo == 45)
      nOGRStyle = 8;
    else if (m_sSymbolDef.nSymbolNo == 46)
      nOGRStyle = 4;
    else if (m_sSymbolDef.nSymbolNo == 49)
      nOGRStyle = 1;
    else if (m_sSymbolDef.nSymbolNo == 50)
      nOGRStyle = 2;

    nAngle += static_cast<int>(dfAngle);

    pszStyle=CPLSPrintf("SYMBOL(a:%d,c:#%6.6x,s:%dpt,id:\"mapinfo-sym-%d,ogr-sym-%d\")",
                        nAngle,
                        m_sSymbolDef.rgbColor,
                        m_sSymbolDef.nPointSize,
                        m_sSymbolDef.nSymbolNo,
                        nOGRStyle);

    return pszStyle;
}

/**********************************************************************
 *                   ITABFeatureSymbol::SetSymbolFromStyleString()
 *
 *  Set all Symbol var from a OGRStyleSymbol.
 **********************************************************************/
void ITABFeatureSymbol::SetSymbolFromStyle(OGRStyleSymbol* poSymbolStyle)
{
    GBool bIsNull = 0;

    // Set the Symbol Id (SymbolNo)
    const char *pszSymbolId = poSymbolStyle->Id(bIsNull);
    if(bIsNull) pszSymbolId = nullptr;

    if(pszSymbolId)
    {
        if(STARTS_WITH(pszSymbolId, "mapinfo-sym-"))
        {
            const int nSymbolId = atoi(pszSymbolId+12);
            SetSymbolNo(static_cast<GByte>(nSymbolId));
        }
        else if(STARTS_WITH(pszSymbolId, "ogr-sym-"))
        {
            const int nSymbolId = atoi(pszSymbolId+8);

            // The OGR symbol is not the MapInfo one
            // Here's some mapping
            switch (nSymbolId)
            {
              case 0:
                SetSymbolNo(31);
                break;
              case 1:
                SetSymbolNo(49);
                break;
              case 2:
                SetSymbolNo(50);
                break;
              case 3:
                SetSymbolNo(40);
                break;
              case 4:
                SetSymbolNo(34);
                break;
              case 5:
                SetSymbolNo(38);
                break;
              case 6:
                SetSymbolNo(32);
                break;
              case 7:
                SetSymbolNo(42);
                break;
              case 8:
                SetSymbolNo(36);
                break;
              case 9:
                SetSymbolNo(41);
                break;
              case 10:
                SetSymbolNo(35);
                break;
            }
        }
    }

    // Set SymbolSize
    const double dSymbolSize = poSymbolStyle->Size(bIsNull);
    if(dSymbolSize != 0.0)
    {
        SetSymbolSize(static_cast<GInt16>(dSymbolSize));
    }

    // Set Symbol Color
    const char *pszSymbolColor = poSymbolStyle->Color(bIsNull);
    if(pszSymbolColor)
    {
        if(pszSymbolColor[0] == '#')
            pszSymbolColor++;
        int nSymbolColor = static_cast<int>(strtol(pszSymbolColor, nullptr, 16));
        SetSymbolColor(static_cast<GInt32>(nSymbolColor));
    }
}

/**********************************************************************
 *                   ITABFeatureSymbol::SetSymbolFromStyleString()
 *
 *  Set all Symbol var from a StyleString. Use StyleMgr to do so.
 **********************************************************************/
void ITABFeatureSymbol::SetSymbolFromStyleString(const char *pszStyleString)
{
    // Use the Style Manager to retrieve all the information we need.
    OGRStyleMgr *poStyleMgr = new OGRStyleMgr(nullptr);
    OGRStyleTool *poStylePart = nullptr;

    // Init the StyleMgr with the StyleString.
    poStyleMgr->InitStyleString(pszStyleString);

    // Retrieve the Symbol info.
    const int numParts = poStyleMgr->GetPartCount();
    for( int i = 0; i < numParts; i++ )
    {
        poStylePart = poStyleMgr->GetPart(i);
        if( poStylePart == nullptr )
            continue;

        if(poStylePart->GetType() == OGRSTCSymbol)
        {
            break;
        }
        else
        {
            delete poStylePart;
            poStylePart = nullptr;
        }
    }

    // If the no Symbol found, do nothing.
    if(poStylePart == nullptr)
    {
        delete poStyleMgr;
        return;
    }

    OGRStyleSymbol *poSymbolStyle = cpl::down_cast<OGRStyleSymbol*>(poStylePart);

    // With Symbol, we always want to output points
    //
    // It's very important to set the output unit of the feature.
    // The default value is meter. If we don't do it all numerical values
    // will be assumed to be converted from the input unit to meter when we
    // will get them via GetParam...() functions.
    // See OGRStyleTool::Parse() for more details.
    poSymbolStyle->SetUnit(OGRSTUPoints, (72.0 * 39.37));

    SetSymbolFromStyle(poSymbolStyle);

    delete poStyleMgr;
    delete poStylePart;

    return;
}

/**********************************************************************
 *                   ITABFeatureSymbol::GetSymbolFeatureClass()
 *
 *  Return the feature class needed to represent the style string.
 **********************************************************************/
TABFeatureClass ITABFeatureSymbol::GetSymbolFeatureClass(const char *pszStyleString)
{
    // Use the Style Manager to retrieve all the information we need.
    OGRStyleMgr *poStyleMgr = new OGRStyleMgr(nullptr);
    OGRStyleTool *poStylePart = nullptr;

    // Init the StyleMgr with the StyleString.
    poStyleMgr->InitStyleString(pszStyleString);

    // Retrieve the Symbol info.
    const int numParts = poStyleMgr->GetPartCount();
    for( int i = 0; i < numParts; i++ )
    {
        poStylePart = poStyleMgr->GetPart(i);
        if( poStylePart == nullptr )
        {
            continue;
        }

        if(poStylePart->GetType() == OGRSTCSymbol)
        {
            break;
        }
        else
        {
            delete poStylePart;
            poStylePart = nullptr;
        }
    }

    TABFeatureClass result = TABFCPoint;

    // If the no Symbol found, do nothing.
    if(poStylePart == nullptr)
    {
        delete poStyleMgr;
        return result;
    }

    OGRStyleSymbol *poSymbolStyle = cpl::down_cast<OGRStyleSymbol*>(poStylePart);

    GBool bIsNull = 0;

    // Set the Symbol Id (SymbolNo)
    const char *pszSymbolId = poSymbolStyle->Id(bIsNull);
    if(bIsNull) pszSymbolId = nullptr;

    if(pszSymbolId)
    {
        if(STARTS_WITH(pszSymbolId, "font-sym-"))
        {
            result = TABFCFontPoint;
        }
        else if(STARTS_WITH(pszSymbolId, "mapinfo-custom-sym-"))
        {
            result = TABFCCustomPoint;
        }
    }

    delete poStyleMgr;
    delete poStylePart;

    return result;
}

/**********************************************************************
 *                   ITABFeatureSymbol::DumpSymbolDef()
 *
 * Dump Symbol definition information.
 **********************************************************************/
void ITABFeatureSymbol::DumpSymbolDef(FILE *fpOut /*=NULL*/)
{
    if (fpOut == nullptr)
        fpOut = stdout;

    fprintf(fpOut, "  m_nSymbolDefIndex       = %d\n", m_nSymbolDefIndex);
    fprintf(fpOut, "  m_sSymbolDef.nRefCount  = %d\n", m_sSymbolDef.nRefCount);
    fprintf(fpOut, "  m_sSymbolDef.nSymbolNo  = %d\n", m_sSymbolDef.nSymbolNo);
    fprintf(fpOut, "  m_sSymbolDef.nPointSize = %d\n",m_sSymbolDef.nPointSize);
    fprintf(fpOut, "  m_sSymbolDef._unknown_  = %d\n",
                                            static_cast<int>(m_sSymbolDef._nUnknownValue_));
    fprintf(fpOut, "  m_sSymbolDef.rgbColor   = 0x%6.6x (%d)\n",
                                m_sSymbolDef.rgbColor, m_sSymbolDef.rgbColor);

    fflush(fpOut);
}
