/******************************************************************************
 * $Id: ogr_sxflayer.cpp  $
 *
 * Project:  SXF Translator
 * Purpose:  Definition of classes for OGR SXF Layers.
 * Author:   Ben Ahmed Daho Ali, bidandou(at)yahoo(dot)fr
 *           Dmitry Baryshnikov, polimax@mail.ru
 *           Alexandr Lisovenko, alexander.lisovenko@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2011, Ben Ahmed Daho Ali
 * Copyright (c) 2013, NextGIS
 * Copyright (c) 2014, Even Rouault <even dot rouault at mines-paris dot org>
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
#define  _USE_MATH_DEFINES

#include "ogr_sxf.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_p.h"
#include "ogr_srs_api.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id: ogrsxflayer.cpp $");

/************************************************************************/
/*                        OGRSXFLayer()                                 */
/************************************************************************/

OGRSXFLayer::OGRSXFLayer(VSILFILE* fp, void** hIOMutex, GByte nID, const char* pszLayerName, int nVer, const SXFMapDescription&  sxfMapDesc) : OGRLayer()
{
    sFIDColumn_ = "ogc_fid";
    fpSXF = fp;
    nLayerID = nID;
    stSXFMapDescription = sxfMapDesc;
    stSXFMapDescription.pSpatRef->Reference();
    m_nSXFFormatVer = nVer;
    oNextIt = mnRecordDesc.begin();
    m_hIOMutex = hIOMutex;
    m_dfCoeff = stSXFMapDescription.dfScale / stSXFMapDescription.nResolution;
    poFeatureDefn = new OGRFeatureDefn(pszLayerName);
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    
    poFeatureDefn->SetGeomType(wkbUnknown);
    if (poFeatureDefn->GetGeomFieldCount() != 0)
        poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(stSXFMapDescription.pSpatRef);

    //OGRGeomFieldDefn oGeomFieldDefn("Shape", wkbGeometryCollection);
    //oGeomFieldDefn.SetSpatialRef(stSXFMapDescription.pSpatRef);
    //poFeatureDefn->AddGeomFieldDefn(&oGeomFieldDefn);

    OGRFieldDefn oFIDField = OGRFieldDefn(sFIDColumn_, OFTInteger);
    poFeatureDefn->AddFieldDefn(&oFIDField);

    OGRFieldDefn oClCodeField = OGRFieldDefn( "CLCODE", OFTInteger );
    oClCodeField.SetWidth(10);
    poFeatureDefn->AddFieldDefn( &oClCodeField );

    OGRFieldDefn oClNameField = OGRFieldDefn( "CLNAME", OFTString );
    oClNameField.SetWidth(32);
    poFeatureDefn->AddFieldDefn( &oClNameField );

    OGRFieldDefn oNumField = OGRFieldDefn( "OBJECTNUMB", OFTInteger );
    oNumField.SetWidth(10);
    poFeatureDefn->AddFieldDefn( &oNumField );

    OGRFieldDefn oAngField = OGRFieldDefn("ANGLE", OFTReal);
    poFeatureDefn->AddFieldDefn(&oAngField);

    OGRFieldDefn  oTextField( "TEXT", OFTString );
    oTextField.SetWidth(255);
    poFeatureDefn->AddFieldDefn( &oTextField );
}

/************************************************************************/
/*                         ~OGRSXFLayer()                               */
/************************************************************************/

OGRSXFLayer::~OGRSXFLayer()
{
    stSXFMapDescription.pSpatRef->Release();
    poFeatureDefn->Release();
}

/************************************************************************/
/*                AddClassifyCode(unsigned nClassCode)                  */
/* Add layer supported classify codes. Only records with this code can  */
/* be in layer                                                          */
/************************************************************************/

void OGRSXFLayer::AddClassifyCode(unsigned nClassCode, const char *szName)
{
    if (szName != NULL)
    {
        mnClassificators[nClassCode] = CPLString(szName);
    }
    else
    {
        CPLString szIdName;
        szIdName.Printf("%d", nClassCode);
        mnClassificators[nClassCode] = szIdName;
    }
}

/************************************************************************/
/*                         AddRecord()                               */
/************************************************************************/

int OGRSXFLayer::AddRecord(long nFID, unsigned nClassCode, vsi_l_offset nOffset, bool bHasSemantic, size_t nSemanticsSize)
{
    if (mnClassificators.empty() || mnClassificators.find(nClassCode) != mnClassificators.end())
    {
        mnRecordDesc[nFID] = nOffset;
        //add addtionals semantics (attribute fields)
        if (bHasSemantic)
        {
            size_t offset = 0;

            while (offset < nSemanticsSize)
            {
                SXFRecordAttributeInfo stAttrInfo;
                bool bAddField = false;
                size_t nCurrOff = 0;
                int nReadObj = VSIFReadL(&stAttrInfo, 4, 1, fpSXF);
                if (nReadObj == 1)
                {
                    CPLString oFieldName;
                    if (snAttributeCodes.find(stAttrInfo.nCode) == snAttributeCodes.end())
                    {
                        bAddField = true;
                        snAttributeCodes.insert(stAttrInfo.nCode);
                        oFieldName.Printf("SC_%d", stAttrInfo.nCode);
                    }

                    SXFRecordAttributeType eType = (SXFRecordAttributeType)stAttrInfo.nType;

                    offset += 4;


                    switch (eType) //TODO: set field type form RSC as here sometimes we have the codes and string values can be get from RSC by this code
                    {
                    case SXF_RAT_ASCIIZ_DOS:
                    {
                        if (bAddField)
                        {
                            OGRFieldDefn  oField(oFieldName, OFTString);
                            oField.SetWidth(255);
                            poFeatureDefn->AddFieldDefn(&oField);
                        }
                        offset += stAttrInfo.nScale + 1;
                        nCurrOff = stAttrInfo.nScale + 1;
                        break;
                    }
                    case SXF_RAT_ONEBYTE:
                    {
                        if (bAddField)
                        {
                            OGRFieldDefn  oField(oFieldName, OFTReal);
                            poFeatureDefn->AddFieldDefn(&oField);
                        }
                        offset += 1;
                        nCurrOff = 1;
                        break;
                    }
                    case SXF_RAT_TWOBYTE:
                    {
                        if (bAddField)
                        {
                            OGRFieldDefn  oField(oFieldName, OFTReal);
                            poFeatureDefn->AddFieldDefn(&oField);
                        }
                        offset += 2;
                        nCurrOff = 2;
                        break;
                    }
                    case SXF_RAT_FOURBYTE:
                    {
                        if (bAddField)
                        {
                            OGRFieldDefn  oField(oFieldName, OFTReal);
                            poFeatureDefn->AddFieldDefn(&oField);
                        }
                        offset += 4;
                        nCurrOff = 4;
                        break;
                    }
                    case SXF_RAT_EIGHTBYTE:
                    {
                        if (bAddField)
                        {
                            OGRFieldDefn  oField(oFieldName, OFTReal);
                            poFeatureDefn->AddFieldDefn(&oField);
                        }
                        offset += 8;
                        nCurrOff = 8;
                        break;
                    }
                    case SXF_RAT_ANSI_WIN:
                    {
                        if (bAddField)
                        {
                            OGRFieldDefn  oField(oFieldName, OFTString);
                            oField.SetWidth(255);
                            poFeatureDefn->AddFieldDefn(&oField);
                        }
                        unsigned nLen = unsigned(stAttrInfo.nScale) + 1;
                        offset += nLen;
                        nCurrOff = nLen;
                        break;
                    }
                    case SXF_RAT_UNICODE:
                    {
                        if (bAddField)
                        {
                            OGRFieldDefn  oField(oFieldName, OFTString);
                            oField.SetWidth(255);
                            poFeatureDefn->AddFieldDefn(&oField);
                        }
                        unsigned nLen = unsigned(stAttrInfo.nScale) + 1;
                        offset += nLen;
                        nCurrOff = nLen;
                        break;
                    }
                    case SXF_RAT_BIGTEXT:
                    {
                        if (bAddField)
                        {
                            OGRFieldDefn  oField(oFieldName, OFTString);
                            oField.SetWidth(1024);
                            poFeatureDefn->AddFieldDefn(&oField);
                        }
                        GUInt32 scale2;
                        VSIFReadL(&scale2, sizeof(GUInt32), 1, fpSXF);
                        CPL_LSBUINT32PTR(&scale2);

                        offset += scale2;
                        nCurrOff = scale2;
                        break;
                    }
                    default:
                        break;
                    }
                }
                if( nCurrOff == 0 )
                    break;
                VSIFSeekL(fpSXF, nCurrOff, SEEK_CUR);
            }
        }
        return TRUE;
    }

    return FALSE;
}

/************************************************************************/
/*                           SetNextByIndex()                           */
/************************************************************************/

OGRErr OGRSXFLayer::SetNextByIndex(long nIndex)
{
    if (nIndex < 0 || nIndex > (long)mnRecordDesc.size())
        return OGRERR_FAILURE;

    oNextIt = mnRecordDesc.begin();
    std::advance(oNextIt, nIndex);

    return OGRERR_NONE;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRSXFLayer::GetFeature(long nFID)
{
    std::map<long, vsi_l_offset>::const_iterator IT = mnRecordDesc.find(nFID);
    if (IT != mnRecordDesc.end())
    {
        VSIFSeekL(fpSXF, IT->second, SEEK_SET);
        OGRFeature *poFeature = GetNextRawFeature(IT->first);
        if (poFeature != NULL && poFeature->GetGeometryRef() != NULL && GetSpatialRef() != NULL)
        {
            poFeature->GetGeometryRef()->assignSpatialReference(GetSpatialRef());
        }
        return poFeature;
    }

    return NULL;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRSXFLayer::GetSpatialRef()
{
    return stSXFMapDescription.pSpatRef;
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRSXFLayer::GetExtent(OGREnvelope *psExtent, int bForce)
{
    if (bForce)
    {
        return OGRLayer::GetExtent(psExtent, bForce);
    }
    else
    {
        psExtent->MinX = stSXFMapDescription.Env.MinX;
        psExtent->MaxX = stSXFMapDescription.Env.MaxX;
        psExtent->MinY = stSXFMapDescription.Env.MinY;
        psExtent->MaxY = stSXFMapDescription.Env.MaxY;

        return OGRERR_NONE;
    }
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRSXFLayer::GetFeatureCount(int bForce)
{
    if (m_poFilterGeom == NULL && m_poAttrQuery == NULL)
        return static_cast<int>(mnRecordDesc.size());
    else
        return OGRLayer::GetFeatureCount(bForce);
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRSXFLayer::ResetReading()

{
    oNextIt = mnRecordDesc.begin();
}


/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRSXFLayer::GetNextFeature()
{
    CPLMutexHolderD(m_hIOMutex);
    while (oNextIt != mnRecordDesc.end())
    {
        VSIFSeekL(fpSXF, oNextIt->second, SEEK_SET);
        OGRFeature  *poFeature = GetNextRawFeature(oNextIt->first);	

        ++oNextIt;

        if (poFeature == NULL)
            continue;

        if ((m_poFilterGeom == NULL
            || FilterGeometry(poFeature->GetGeometryRef()))
            && (m_poAttrQuery == NULL
            || m_poAttrQuery->Evaluate(poFeature)))
        {
            if (poFeature->GetGeometryRef() != NULL && GetSpatialRef() != NULL)
            {
                poFeature->GetGeometryRef()->assignSpatialReference(GetSpatialRef());
            }
	    
            return poFeature;
        }

        delete poFeature;
    }
    return NULL;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSXFLayer::TestCapability( const char * pszCap )

{
    if (EQUAL(pszCap, OLCStringsAsUTF8))
        return TRUE;
    else if (EQUAL(pszCap, OLCRandomRead))
        return TRUE;
    else if (EQUAL(pszCap, OLCFastFeatureCount))
        return TRUE;
    else if (EQUAL(pszCap, OLCFastGetExtent))
        return TRUE;
    else if (EQUAL(pszCap, OLCFastSetNextByIndex))
        return TRUE;
        
    return FALSE;
}

/************************************************************************/
/*                                TranslateXYH()                        */
/************************************************************************/
/****
 * TODO : Take into account informations given in the passport 
 * like unit of mesurement, type and dimensions (integer, float, double) of coordinate,
 * the vector format ....
 */

GUInt32 OGRSXFLayer::TranslateXYH(const SXFRecordDescription& certifInfo,
                                  const char *psBuff, GUInt32 nBufLen,
                          double *dfX, double *dfY, double *dfH)
{
    //Xp, Yp(м) = Xo, Yo(м) + (Xd, Yd / R * S), (1)

	int offset = 0;
    switch (certifInfo.eValType)
    {
    case SXF_VT_SHORT:
    {
        if( nBufLen < 4 )
            return 0;
        GInt16 x, y;
        memcpy(&y, psBuff, 2);
        CPL_LSBINT16PTR(&y);
        memcpy(&x, psBuff + 2, 2);
        CPL_LSBINT16PTR(&x);

        if (stSXFMapDescription.bIsRealCoordinates)
        {
            *dfX = (double)x;
            *dfY = (double)y;
        }
        else
        {
            if (m_nSXFFormatVer == 3)
            {
                *dfX = stSXFMapDescription.dfXOr + (double)x * m_dfCoeff;
                *dfY = stSXFMapDescription.dfYOr + (double)y * m_dfCoeff;
            }
            else if (m_nSXFFormatVer == 4)
            {
                //TODO: check on real data
                *dfX = stSXFMapDescription.dfXOr + (double)x * m_dfCoeff;
                *dfY = stSXFMapDescription.dfYOr + (double)y * m_dfCoeff;
            }
        }

        offset += 4;

        if (dfH != NULL)
        {
            if( nBufLen < 4 + 4 )
                return 0;
            float h;
            memcpy(&h, psBuff + 4, 4); // H always in float
            CPL_LSBPTR32(&h);
            *dfH = (double)h;

            offset += 4;
        }
    }
        break;
    case SXF_VT_FLOAT:
    {
        if( nBufLen < 8 )
            return 0;
        float x, y;
        memcpy(&y, psBuff, 4);
        CPL_LSBPTR32(&y);
        memcpy(&x, psBuff + 4, 4);
        CPL_LSBPTR32(&x);

        if (stSXFMapDescription.bIsRealCoordinates)
        {
            *dfX = (double)x;
            *dfY = (double)y;
        }
        else
        {
            *dfX = stSXFMapDescription.dfXOr + (double)x * m_dfCoeff;
            *dfY = stSXFMapDescription.dfYOr + (double)y * m_dfCoeff;
        }

        offset += 8;

        if (dfH != NULL)
        {
            if( nBufLen < 8 + 4 )
                return 0;
            float h;
            memcpy(&h, psBuff + 8, 4); // H always in float
            CPL_LSBPTR32(&h);
            *dfH = (double)h;

            offset += 4;
        }
    }
        break;
    case SXF_VT_INT:
    {
        if( nBufLen < 8 )
            return 0;
        GInt32 x, y;
        memcpy(&y, psBuff, 4);
        CPL_LSBPTR32(&y);
        memcpy(&x, psBuff + 4, 4);
        CPL_LSBPTR32(&x);

        if (stSXFMapDescription.bIsRealCoordinates)
        {
            *dfX = (double)x;
            *dfY = (double)y;
        }
        else
        {
            //TODO: check on real data
            if (m_nSXFFormatVer == 3)
            {
                *dfX = stSXFMapDescription.dfXOr + (double)x * m_dfCoeff;
                *dfY = stSXFMapDescription.dfYOr + (double)y * m_dfCoeff;
            }
            else if (m_nSXFFormatVer == 4)
            {
                *dfX = stSXFMapDescription.dfXOr + (double)x * m_dfCoeff;
                *dfY = stSXFMapDescription.dfYOr + (double)y * m_dfCoeff;
            }
        }
        offset += 8;

        if (dfH != NULL)
        {
            if( nBufLen < 8 + 4 )
                return 0;
            float h;
            memcpy(&h, psBuff + 8, 4); // H always in float
            CPL_LSBPTR32(&h);
            *dfH = (double)h;

            offset += 4;
        }
    }
        break;
    case SXF_VT_DOUBLE:
    {
        if( nBufLen < 16 )
            return 0;
        double x, y;
        memcpy(&y, psBuff, 8);
        CPL_LSBPTR64(&y);
        memcpy(&x, psBuff + 8, 8);
        CPL_LSBPTR64(&x);

        if (stSXFMapDescription.bIsRealCoordinates)
        {
            *dfX = x;
            *dfY = y;
        }
        else
        {
            *dfX = stSXFMapDescription.dfXOr + x * m_dfCoeff;
            *dfY = stSXFMapDescription.dfYOr + y * m_dfCoeff;
        }

        offset += 16;

        if (dfH != NULL)
        {
            if( nBufLen < 16 + 8 )
                return 0;
            double h;
            memcpy(&h, psBuff + 16, 8); // H in double
            CPL_LSBPTR64(&h);
            *dfH = (double)h;

            offset += 8;
        }
    }
        break;
    };

    return offset;
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRSXFLayer::GetNextRawFeature(long nFID)
{
    SXFRecordHeader stRecordHeader;
    int nObjectRead;

    nObjectRead = VSIFReadL(&stRecordHeader, sizeof(SXFRecordHeader), 1, fpSXF);

    if (nObjectRead != 1 || stRecordHeader.nID != IDSXFOBJ)
    {
        CPLError(CE_Failure, CPLE_FileIO, "SXF. Read record failed.");
        return NULL;
    }

    SXFGeometryType eGeomType = SXF_GT_Unknown;
    GByte code = 0;

    if (m_nSXFFormatVer == 3)
    {
        if (CHECK_BIT(stRecordHeader.nRef[2], 3))
        {
            if (CHECK_BIT(stRecordHeader.nRef[2], 4))
            {
                code = 0x22;
                stRecordHeader.nSubObjectCount = 0;
            }
            else
            {
                code = 0x21; 
                stRecordHeader.nSubObjectCount = 0;
            }
        }
        else
        {
            code = stRecordHeader.nRef[0] & 3;//get first 2 bits
        }
    }
    else if (m_nSXFFormatVer == 4)
    {
        if (CHECK_BIT(stRecordHeader.nRef[2], 5))
        {
            stRecordHeader.nSubObjectCount = 0;
        }

        //check if vector
        code = stRecordHeader.nRef[0] & 15;//get first 4 bits
        if (code == 0x04) // xxxx0100
        {
            code = 0x21;
            stRecordHeader.nSubObjectCount = 0;
            //if (CHECK_BIT(stRecordHeader.nRef[2], 5))
            //{
            //    code = 0x22;
            //    stRecordHeader.nSubObjectCount = 0;
            //}
            //else
            //{
            //    code = 0x21;
            //    stRecordHeader.nSubObjectCount = 0;
            //}
            //if (CHECK_BIT(stRecordHeader.nRef[2], 4))
            //{
            //}
            //else
            //{
            //}
        }
    }

    if (code == 0x00) // xxxx0000
        eGeomType = SXF_GT_Line;
    else if (code == 0x01) // xxxx0001
        eGeomType = SXF_GT_Polygon;
    else if (code == 0x02) // xxxx0010
        eGeomType = SXF_GT_Point;
    else if (code == 0x03) // xxxx0011
        eGeomType = SXF_GT_Text;
    //beginning 4.0
    else if (code == 0x04) // xxxx0100
    {
        CPLError(CE_Warning, CPLE_NotSupported,
            "SXF. Not support type.");
        eGeomType = SXF_GT_Vector;
    }
    else if (code == 0x05) // xxxx0101
        eGeomType = SXF_GT_TextTemplate;
    else if (code == 0x21)
        eGeomType = SXF_GT_VectorAngle;
    else if (code == 0x22)
        eGeomType = SXF_GT_VectorScaled;

    bool bHasAttributes = CHECK_BIT(stRecordHeader.nRef[1], 1);
    bool bHasRefVector = CHECK_BIT(stRecordHeader.nRef[1], 3);
    if (bHasRefVector == true)
        CPLError(CE_Failure, CPLE_NotSupported,
        "SXF. Parsing the vector of the tying not support.");

    SXFRecordDescription stCertInfo;
    if (stRecordHeader.nPointCountSmall == 65535)
    {
        stCertInfo.nPointCount = stRecordHeader.nPointCount;
    }
    else
    {
        stCertInfo.nPointCount = stRecordHeader.nPointCountSmall;
    }
    stCertInfo.nSubObjectCount = stRecordHeader.nSubObjectCount;

    bool bFloatType = 0, bBigType = 0;
    bool b3D(true);
    if (m_nSXFFormatVer == 3)
    {
        b3D = CHECK_BIT(stRecordHeader.nRef[2], 1);
        bFloatType = CHECK_BIT(stRecordHeader.nRef[2], 2);
        bBigType = CHECK_BIT(stRecordHeader.nRef[1], 2);
        stCertInfo.bHasTextSign = CHECK_BIT(stRecordHeader.nRef[2], 5);
    }
    else if (m_nSXFFormatVer == 4)
    {
        b3D = CHECK_BIT(stRecordHeader.nRef[2], 1);
        bFloatType = CHECK_BIT(stRecordHeader.nRef[2], 2);
        bBigType = CHECK_BIT(stRecordHeader.nRef[1], 2);
        stCertInfo.bHasTextSign = CHECK_BIT(stRecordHeader.nRef[2], 3);
    }
    // Else trouble.

    if (b3D) //xххххх1х
        stCertInfo.bDim = 1;
    else
        stCertInfo.bDim = 0;

    if (bFloatType)
    {
        if (bBigType)
        {
            stCertInfo.eValType = SXF_VT_DOUBLE;
        }
        else
        {
            stCertInfo.eValType = SXF_VT_FLOAT;
        }
    }
    else
    {
        if (bBigType)
        {
            stCertInfo.eValType = SXF_VT_INT;
        }
        else
        {
            stCertInfo.eValType = SXF_VT_SHORT;
        }
    }


    stCertInfo.bFormat = CHECK_BIT(stRecordHeader.nRef[2], 0);
    stCertInfo.eGeomType = eGeomType;

    OGRFeature *poFeature = NULL;
    if( stRecordHeader.nGeometryLength > 100 * 1024 * 1024 )
        return NULL;
    char * recordCertifBuf = (char *)VSIMalloc(stRecordHeader.nGeometryLength);
    if( recordCertifBuf == NULL )
        return NULL;
    nObjectRead = VSIFReadL(recordCertifBuf, stRecordHeader.nGeometryLength, 1, fpSXF);
    if (nObjectRead != 1)
    {
        CPLError(CE_Failure, CPLE_FileIO,
            "SXF. Read geometry failed.");
        CPLFree(recordCertifBuf);
        return NULL;
    }

    if (eGeomType == SXF_GT_Point)
        poFeature = TranslatePoint(stCertInfo, recordCertifBuf,
                                   stRecordHeader.nGeometryLength);
    else if (eGeomType == SXF_GT_Line || eGeomType == SXF_GT_VectorScaled)
        poFeature = TranslateLine(stCertInfo, recordCertifBuf,
                                   stRecordHeader.nGeometryLength);
    else if (eGeomType == SXF_GT_Polygon)
        poFeature = TranslatePolygon(stCertInfo, recordCertifBuf,
                                   stRecordHeader.nGeometryLength);
    else if (eGeomType == SXF_GT_Text)
        poFeature = TranslateText(stCertInfo, recordCertifBuf,
                                   stRecordHeader.nGeometryLength);
    else if (eGeomType == SXF_GT_VectorAngle)
    {
        poFeature = TranslateVetorAngle(stCertInfo, recordCertifBuf,
            stRecordHeader.nGeometryLength);
    }
    else if (eGeomType == SXF_GT_Vector ) 
    {
      CPLError( CE_Warning, CPLE_NotSupported,
      "SXF. Geometry type Vector do not support." );
      return NULL;
    }
    else if (eGeomType == SXF_GT_TextTemplate ) // TODO realise this
    {
      CPLError( CE_Warning, CPLE_NotSupported,
      "SXF. Geometry type Text Template do not support." );
      return NULL;
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
            "SXF. Unsupported geometry type.");
        CPLFree(recordCertifBuf);
        return NULL;
    }
    
    if( poFeature == NULL )
    {
        CPLFree(recordCertifBuf);
        return NULL;
    }

    poFeature->SetField(sFIDColumn_, (int)nFID);

    poFeature->SetField("CLCODE", (int)stRecordHeader.nClassifyCode);

    CPLString szName = mnClassificators[stRecordHeader.nClassifyCode];

    if (szName.empty())
    {
        szName.Printf("%d", stRecordHeader.nClassifyCode);
    }
    poFeature->SetField("CLNAME", szName);

    poFeature->SetField("OBJECTNUMB", stRecordHeader.nSubObjectCount);

    if (bHasAttributes)
    {
        if( stRecordHeader.nFullLength < 32 ||
            stRecordHeader.nGeometryLength > stRecordHeader.nFullLength - 32 )
        {
            CPLFree(recordCertifBuf);
            delete poFeature;
            return NULL;
        }
        size_t  nSemanticsSize = stRecordHeader.nFullLength - 32 - stRecordHeader.nGeometryLength;
        if( nSemanticsSize > 1024 * 1024 )
        {
            CPLFree(recordCertifBuf);
            delete poFeature;
            return NULL;
        }
        char * psSemanticsdBuf = (char *)VSIMalloc(nSemanticsSize);
        if( psSemanticsdBuf == NULL )
        {
            CPLFree(recordCertifBuf);
            delete poFeature;
            return NULL;
        }
        char * psSemanticsdBufOrig = psSemanticsdBuf;
        nObjectRead = VSIFReadL(psSemanticsdBuf, nSemanticsSize, 1, fpSXF);
        if (nObjectRead == 1)
        {
            size_t offset = 0;
            double nVal = 0;

            while (offset + sizeof(SXFRecordAttributeInfo) < nSemanticsSize)
            {
                char *psSemanticsdBufBeg = psSemanticsdBuf + offset;
                SXFRecordAttributeInfo stAttInfo = *(SXFRecordAttributeInfo*)psSemanticsdBufBeg;
                offset += 4;

                CPLString oFieldName;
                oFieldName.Printf("SC_%d", stAttInfo.nCode);

                CPLString oFieldValue;

                SXFRecordAttributeType eType = (SXFRecordAttributeType)stAttInfo.nType;

                switch (eType)
                {
                case SXF_RAT_ASCIIZ_DOS:
                {
                    unsigned nLen = unsigned(stAttInfo.nScale) + 1;
                    if( nLen > nSemanticsSize || nSemanticsSize - nLen < offset )
                    {
                        nSemanticsSize = 0;
                        break;
                    }
                    char * value = (char*)CPLMalloc(nLen);
                    memcpy(value, psSemanticsdBuf + offset, nLen);
                    value[nLen-1] = 0;
                    char* pszRecoded = CPLRecode(value, "CP866", CPL_ENC_UTF8);
                    poFeature->SetField(oFieldName, pszRecoded);
                    CPLFree(pszRecoded);
                    CPLFree(value);

                    offset += stAttInfo.nScale + 1;
                    break;
                }
                case SXF_RAT_ONEBYTE:
                {
                    GByte nTmpVal;
                    if( offset + sizeof(GByte) > nSemanticsSize )
                    {
                        nSemanticsSize = 0;
                        break;
                    }
                    memcpy(&nTmpVal, psSemanticsdBuf + offset, sizeof(GByte));
                    nVal = double(nTmpVal) * pow(10.0, (double)stAttInfo.nScale);

                    poFeature->SetField(oFieldName, nVal);
                    offset += 1;
                    break;
                }
                case SXF_RAT_TWOBYTE:
                {
                    GInt16 nTmpVal;
                    if( offset + sizeof(GInt16) > nSemanticsSize )
                    {
                        nSemanticsSize = 0;
                        break;
                    }
                    memcpy(&nTmpVal, psSemanticsdBuf + offset, sizeof(GInt16));
                    nVal = double(CPL_LSBWORD16(nTmpVal)) * pow(10.0, (double)stAttInfo.nScale);
   
                    poFeature->SetField(oFieldName, nVal);
                    offset += 2;
                    break;
                }
                case SXF_RAT_FOURBYTE:
                {
                    GInt32 nTmpVal;
                    if( offset + sizeof(GInt32) > nSemanticsSize )
                    {
                        nSemanticsSize = 0;
                        break;
                    }
                    memcpy(&nTmpVal, psSemanticsdBuf + offset, sizeof(GInt32));
                    nVal = double(CPL_LSBWORD32(nTmpVal)) * pow(10.0, (double)stAttInfo.nScale);

                    poFeature->SetField(oFieldName, nVal);
                    offset += 4;
                    break;
                }
                case SXF_RAT_EIGHTBYTE:
                {
                    double dfTmpVal;
                    if( offset + sizeof(double) > nSemanticsSize )
                    {
                        nSemanticsSize = 0;
                        break;
                    }
                    memcpy(&dfTmpVal, psSemanticsdBuf + offset, sizeof(double));
                    CPL_LSBPTR64(&dfTmpVal);
                    double d = dfTmpVal * pow(10.0, (double)stAttInfo.nScale);

                    poFeature->SetField(oFieldName, d);

                    offset += 8;
                    break;
                }
                case SXF_RAT_ANSI_WIN:
                {
                    unsigned nLen = unsigned(stAttInfo.nScale) + 1;
                    if( nLen > nSemanticsSize || nSemanticsSize - nLen < offset )
                    {
                        nSemanticsSize = 0;
                        break;
                    }
                    char * value = (char*)CPLMalloc(nLen);
                    memcpy(value, psSemanticsdBuf + offset, nLen);
                    value[nLen-1] = 0;
                    char* pszRecoded = CPLRecode(value, "CP1251", CPL_ENC_UTF8);
                    poFeature->SetField(oFieldName, pszRecoded);
                    CPLFree(pszRecoded);
                    CPLFree(value);

                    offset += nLen;
                    break;
                }
                case SXF_RAT_UNICODE:
                {
                    unsigned nLen = (unsigned(stAttInfo.nScale) + 1) * 2;
                    if(nLen < 2 || nLen > nSemanticsSize || nSemanticsSize - nLen < offset )
                    {
                        nSemanticsSize = 0;
                        break;
                    }
                    char* value = (char*)CPLMalloc(nLen);
                    memcpy(value, psSemanticsdBuf + offset, nLen);
                    value[nLen-1] = 0;
                    value[nLen-2] = 0;
                    char* dst = (char*)CPLMalloc(nLen);
                    int nCount = 0;
                    for(int i = 0; (unsigned)i < nLen; i += 2)
                    {
                         unsigned char ucs = value[i];

                         if (ucs < 0x80U)
                         {
                             dst[nCount++] = ucs;
                         }
                         else
                         {
                             dst[nCount++] = 0xc0 | (ucs >> 6);
                             dst[nCount++] = 0x80 | (ucs & 0x3F);
                         }
                    }

                    poFeature->SetField(oFieldName, dst);
                    CPLFree(dst);
                    CPLFree(value);

                    offset += nLen;
                    break;
                }
                case SXF_RAT_BIGTEXT:
                {
                    GUInt32 scale2;
                    if( offset + sizeof(GUInt32) > nSemanticsSize )
                    {
                        nSemanticsSize = 0;
                        break;
                    }
                    memcpy(&scale2, psSemanticsdBuf + offset, sizeof(GUInt32));
                    CPL_LSBUINT32PTR(&scale2);
                    /* FIXME add ?: offset += sizeof(GUInt32); */
                    if( scale2 > nSemanticsSize - 1 || nSemanticsSize - (scale2 + 1) < offset )
                    {
                        nSemanticsSize = 0;
                        break;
                    }

                    char * value = (char*)CPLMalloc(scale2 + 1);
                    memcpy(value, psSemanticsdBuf + offset, scale2 + 1);
                    value[scale2] = 0;
                    char* pszRecoded = CPLRecode(value, CPL_ENC_UTF16, CPL_ENC_UTF8);
                    poFeature->SetField(oFieldName, pszRecoded);
                    CPLFree(pszRecoded);
                    CPLFree(value);

                    offset += scale2;
                    break;
                }
                default:
                    CPLFree(recordCertifBuf);
                    CPLFree(psSemanticsdBufOrig);
                    delete poFeature;
                    return NULL;
                }
            }
            CPLFree(psSemanticsdBufOrig);
        }
    }    
   
    poFeature->SetFID(nFID);

    CPLFree(recordCertifBuf);

    return poFeature;
}

/************************************************************************/
/*                         TranslatePoint   ()                          */
/************************************************************************/

OGRFeature *OGRSXFLayer::TranslatePoint(const SXFRecordDescription& certifInfo,
                                        const char * psRecordBuf, GUInt32 nBufLen)
{
    double dfX = 1.0;
    double dfY = 1.0;
    GUInt32 nOffset = 0;

    GUInt32 nDelta = TranslateXYH( certifInfo, psRecordBuf , nBufLen, &dfX, &dfY );
    if( nDelta == 0 )
        return NULL;
    nOffset += nDelta;

	//OGRFeatureDefn *fd = poFeatureDefn->Clone();
	//fd->SetGeomType( wkbMultiPoint );
 //   OGRFeature *poFeature = new OGRFeature(fd);
    OGRFeature *poFeature = new OGRFeature(poFeatureDefn);
    OGRMultiPoint* poMPt = new OGRMultiPoint();

    if (certifInfo.bDim == 1) // TODO realise this
	{
		CPLError( CE_Failure, CPLE_NotSupported, 
				"SXF. 3D metrics do not support." );
	}


    poMPt->addGeometryDirectly( new OGRPoint( dfX, dfY ) );

/*---------------------- Reading SubObjects --------------------------------*/

    for(int count=0 ; count <  certifInfo.nSubObjectCount ; count++)
    {
        if( nOffset + 4 > nBufLen )
            break;

        GUInt16 nSubObj;
        memcpy(&nSubObj, psRecordBuf + nOffset, 2);
        CPL_LSBUINT16PTR(&nSubObj);

        GUInt16 nCoords;
        memcpy(&nCoords, psRecordBuf + nOffset + 2, 2);
        CPL_LSBUINT16PTR(&nCoords);

        nOffset +=4;

        for (int i=0; i < nCoords ; i++)
        {
            const char * psCoords = psRecordBuf + nOffset ;

            nDelta = TranslateXYH( certifInfo, psCoords , nBufLen - nOffset, &dfX, &dfY ) ;
            if( nDelta == 0 )
                break;
            nOffset +=  nDelta;

            poMPt->addGeometryDirectly( new OGRPoint( dfX, dfY ) );
        } 
    }

/*****
 * TODO : 
 *          - Translate graphics 
 *          - Translate 3D vector
 */

    poFeature->SetGeometryDirectly( poMPt );

    return poFeature;
}

/************************************************************************/
/*                         TranslateLine    ()                          */
/************************************************************************/

OGRFeature *OGRSXFLayer::TranslateLine(const SXFRecordDescription& certifInfo,
                                       const char * psRecordBuf, GUInt32 nBufLen)
{
    double dfX = 1.0;
    double dfY = 1.0;
    GUInt32 nOffset = 0;
    GUInt32 count;
    
	//OGRFeatureDefn *fd = poFeatureDefn->Clone();
	//fd->SetGeomType( wkbMultiLineString );
 //   OGRFeature *poFeature = new OGRFeature(fd);

    OGRFeature *poFeature = new OGRFeature(poFeatureDefn);
    OGRMultiLineString *poMLS = new  OGRMultiLineString ();

/*---------------------- Reading Primary Line --------------------------------*/

    OGRLineString* poLS = new OGRLineString();

    if (certifInfo.bDim == 1) // TODO realise this
	{
		 CPLError( CE_Failure, CPLE_NotSupported, 
                  "SXF. 3D metrics do not support." );
	}

    for(count=0 ; count <  certifInfo.nPointCount ; count++)
    {
        const char * psCoords = psRecordBuf + nOffset ;
        GUInt32 nDelta = TranslateXYH( certifInfo, psCoords, nBufLen - nOffset, &dfX, &dfY );
        if( nDelta == 0 )
            break;
        nOffset += nDelta;

        poLS->addPoint( dfX  , dfY );
    }

    poMLS->addGeometry( poLS );

/*---------------------- Reading Sub Lines --------------------------------*/

    for(int count=0 ; count <  certifInfo.nSubObjectCount ; count++)
    {
        poLS->empty();

        if( nOffset + 4 > nBufLen )
            break;

        GUInt16 nSubObj;
        memcpy(&nSubObj, psRecordBuf + nOffset, 2);
        CPL_LSBUINT16PTR(&nSubObj);

        GUInt16 nCoords;
        memcpy(&nCoords, psRecordBuf + nOffset + 2, 2);
        CPL_LSBUINT16PTR(&nCoords);

        nOffset +=4;

        for (int i=0; i < nCoords ; i++)
        {
            const char * psCoords = psRecordBuf + nOffset ;

            GUInt32 nDelta = TranslateXYH( certifInfo, psCoords, nBufLen - nOffset, &dfX, &dfY );
            if( nDelta == 0 )
                break;
            nOffset += nDelta;

            poLS->addPoint( dfX  , dfY );
        }

        poMLS->addGeometry( poLS );
    }    // for

    delete poLS;
    poFeature->SetGeometryDirectly( poMLS );

/*****
 * TODO : 
 *          - Translate graphics 
 *          - Translate 3D vector
 */

    return poFeature;
}

/************************************************************************/
/*                       TranslateVetorAngle()                          */
/************************************************************************/

OGRFeature *OGRSXFLayer::TranslateVetorAngle(const SXFRecordDescription& certifInfo,
    const char * psRecordBuf, GUInt32 nBufLen)
{
    if (certifInfo.nPointCount != 2)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
            "SXF. The vector object should have 2 points, but not.");
        return NULL;
    }

    double dfX = 1.0;
    double dfY = 1.0;
    GUInt32 nOffset = 0;
    GUInt32 count;
    
    OGRFeature *poFeature = new OGRFeature(poFeatureDefn);
    OGRPoint *poPT = new OGRPoint();

    /*---------------------- Reading Primary Line --------------------------------*/

    OGRLineString* poLS = new OGRLineString();

    if (certifInfo.bDim == 1) // TODO realise this
    {
        CPLError(CE_Failure, CPLE_NotSupported,
            "SXF. 3D metrics do not support.");
    }

    for (count = 0; count < certifInfo.nPointCount; count++)
    {
        const char * psCoords = psRecordBuf + nOffset;
        GUInt32 nDelta = TranslateXYH(certifInfo, psCoords, nBufLen - nOffset, &dfX, &dfY);
        if (nDelta == 0)
            break;
        nOffset += nDelta;

        poLS->addPoint(dfX, dfY);
    }

    poLS->StartPoint(poPT);

    OGRPoint *poAngPT = new OGRPoint();
    poLS->EndPoint(poAngPT);

    double xDiff = poPT->getX() - poAngPT->getX();
    double yDiff = poPT->getY() - poAngPT->getY();
    double dfAngle = atan2(xDiff, yDiff) * TO_DEGREES - 90;
    if (dfAngle < 0)
        dfAngle += 360;

    poFeature->SetGeometryDirectly(poPT);
    poFeature->SetField("ANGLE", dfAngle);

    delete poAngPT;
    delete poLS;

    return poFeature;
}


/************************************************************************/
/*                         TranslatePolygon ()                          */
/************************************************************************/

OGRFeature *OGRSXFLayer::TranslatePolygon(const SXFRecordDescription& certifInfo,
                                          const char * psRecordBuf, GUInt32 nBufLen)
{
    double dfX = 1.0;
    double dfY = 1.0;
    GUInt32 nOffset = 0;
    GUInt32 count;

	//OGRFeatureDefn *fd = poFeatureDefn->Clone();
	//fd->SetGeomType( wkbMultiPolygon );
 //   OGRFeature *poFeature = new OGRFeature(fd);

    OGRFeature *poFeature = new OGRFeature(poFeatureDefn);
    OGRPolygon *poPoly = new OGRPolygon();
    OGRLineString* poLS = new OGRLineString();

/*---------------------- Reading Primary Polygon --------------------------------*/

    if (certifInfo.bDim == 1) // TODO realise this
	{
			CPLError( CE_Failure, CPLE_NotSupported, 
					"SXF. 3D metrics do not support." );
	}

    for(count=0 ; count <  certifInfo.nPointCount ; count++)
    {
        const char * psBuf = psRecordBuf + nOffset ;

        GUInt32 nDelta = TranslateXYH( certifInfo, psBuf, nBufLen - nOffset, &dfX, &dfY );
        if( nDelta == 0 )
            break;
        nOffset += nDelta;
        poLS->addPoint( dfX  , dfY );

    }    // for

    OGRLinearRing *poLR = new OGRLinearRing();
    poLR->addSubLineString( poLS, 0 );

    poPoly->addRingDirectly( poLR );

/*---------------------- Reading Sub Lines --------------------------------*/

    for(int count=0 ; count <  certifInfo.nSubObjectCount ; count++)
    {
        poLS->empty();

        if( nOffset + 4 > nBufLen )
            break;

        GUInt16 nSubObj;
        memcpy(&nSubObj, psRecordBuf + nOffset, 2);
        CPL_LSBUINT16PTR(&nSubObj);

        GUInt16 nCoords;
        memcpy(&nCoords, psRecordBuf + nOffset + 2, 2);
        CPL_LSBUINT16PTR(&nCoords);

        nOffset +=4;

        int i;
        for (i=0; i < nCoords ; i++)
        {
            const char * psCoords = psRecordBuf + nOffset ;

            GUInt32 nDelta = TranslateXYH( certifInfo, psCoords, nBufLen - nOffset, &dfX, &dfY );
            if( nDelta == 0 )
                break;
            nOffset += nDelta;

            poLS->addPoint( dfX  , dfY );
        }

        OGRLinearRing *poLR = new OGRLinearRing();
        poLR->addSubLineString( poLS, 0 );

        poPoly->addRingDirectly( poLR );
    }    // for

    poFeature->SetGeometryDirectly( poPoly );   //poLS);
    delete poLS;

/*****
 * TODO : 
 *          - Translate graphics 
 *          - Translate 3D vector
 */
    return poFeature;
}

/************************************************************************/
/*                         TranslateText    ()                          */
/************************************************************************/
OGRFeature *OGRSXFLayer::TranslateText(const SXFRecordDescription& certifInfo,
                                       const char * psRecordBuf, GUInt32 nBufLen)
{
    double dfX = 1.0;
    double dfY = 1.0;
    GUInt32 nOffset = 0;
    GUInt32 count;
	//OGRFeatureDefn *fd = poFeatureDefn->Clone();
	//fd->SetGeomType( wkbLineString );
 //   OGRFeature *poFeature = new OGRFeature(fd);

    OGRFeature *poFeature = new OGRFeature(poFeatureDefn);
    OGRLineString* poLS = new OGRLineString();

    if (certifInfo.bDim == 1) // TODO realise this
	{
			CPLError( CE_Failure, CPLE_NotSupported, 
					"SXF. 3D metrics do not support." );
	}

    for(count=0 ; count <  certifInfo.nPointCount ; count++)
    {
        const char * psBuf = psRecordBuf + nOffset;
        GUInt32 nDelta = TranslateXYH( certifInfo, psBuf, nBufLen - nOffset, &dfX, &dfY );
        if( nDelta == 0 )
            break;
        nOffset += nDelta;
        poLS->addPoint( dfX  , dfY );
    }

    poFeature->SetGeometryDirectly( poLS );

/*------------------     READING TEXT VALUE   ---------------------------------------*/

    if ( certifInfo.nSubObjectCount == 0 && certifInfo.bHasTextSign == true)
    {
        if( nOffset + 1 > nBufLen )
            return poFeature;
        const char * pszTxt = psRecordBuf + nOffset;
        GByte nTextL = (GByte) *pszTxt;
        if( nOffset + 1 + nTextL > nBufLen )
            return poFeature;

        char * pszTextBuf = (char *)CPLMalloc( nTextL+1 );

        strncpy(pszTextBuf, (pszTxt+1),    nTextL);
        pszTextBuf[nTextL] = '\0';

        //TODO: Check encoding from sxf
        poFeature->SetField("TEXT", pszTextBuf);
 
        CPLFree( pszTextBuf );
    }


/*****
 * TODO : 
 *          - Translate graphics 
 *          - Translate 3D vector
 */

    return poFeature;
}

const char* OGRSXFLayer::GetFIDColumn()
{
    return sFIDColumn_.c_str();
}
