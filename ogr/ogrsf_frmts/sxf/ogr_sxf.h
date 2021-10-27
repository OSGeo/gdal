/******************************************************************************
 * $Id$
 *
 * Project:  SXF Translator
 * Purpose:  Include file defining classes for OGR SXF driver, datasource and layers.
 * Author:   Ben Ahmed Daho Ali, bidandou(at)yahoo(dot)fr
 *           Dmitry Baryshnikov, polimax@mail.ru
 *           Alexandr Lisovenko, alexander.lisovenko@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2011, Ben Ahmed Daho Ali
 * Copyright (c) 2013, NextGIS
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

#ifndef OGR_SXF_H_INCLUDED
#define OGR_SXF_H_INCLUDED

#include <set>
#include <vector>
#include <map>

#include "ogrsf_frmts.h"
#include "org_sxf_defs.h"

#define CHECK_BIT(var,pos) (((var) & (1<<(pos))) != 0)
#define TO_DEGREES 57.2957795130823208766
#define TO_RADIANS 0.017453292519943295769

/************************************************************************/
/*                         OGRSXFLayer                                */
/************************************************************************/
class OGRSXFLayer final: public OGRLayer
{
protected:
    OGRFeatureDefn*    poFeatureDefn;
    VSILFILE*          fpSXF;
    GByte              nLayerID;
    std::map<unsigned, CPLString> mnClassificators;
    std::map<long, vsi_l_offset> mnRecordDesc;
    std::map<long, vsi_l_offset>::const_iterator oNextIt;
    SXFMapDescription  stSXFMapDescription;
    std::set<GUInt16> snAttributeCodes;
    int m_nSXFFormatVer;
    CPLString sFIDColumn_;
    CPLMutex            **m_hIOMutex;
    double              m_dfCoeff;
    virtual OGRFeature *       GetNextRawFeature(long nFID);

    GUInt32 TranslateXYH(const SXFRecordDescription& certifInfo,
                         const char *psBuff, GUInt32 nBufLen,
                         double *dfX, double *dfY, double *dfH = nullptr);

    OGRFeature *TranslatePoint(const SXFRecordDescription& certifInfo, const char * psRecordBuf, GUInt32 nBufLen);
    OGRFeature *TranslateText(const SXFRecordDescription& certifInfo, const char * psBuff, GUInt32 nBufLen);
    OGRFeature *TranslatePolygon(const SXFRecordDescription& certifInfo, const char * psBuff, GUInt32 nBufLen);
    OGRFeature *TranslateLine(const SXFRecordDescription& certifInfo, const char * psBuff, GUInt32 nBufLen);
    OGRFeature *TranslateVetorAngle(const SXFRecordDescription& certifInfo, const char * psBuff, GUInt32 nBufLen);
public:
    OGRSXFLayer(VSILFILE* fp, CPLMutex** hIOMutex, GByte nID, const char* pszLayerName, int nVer, const SXFMapDescription&  sxfMapDesc);
    virtual ~OGRSXFLayer();

    virtual void                ResetReading() override;
    virtual OGRFeature         *GetNextFeature() override;
    virtual OGRErr              SetNextByIndex(GIntBig nIndex) override;
    virtual OGRFeature         *GetFeature(GIntBig nFID) override;
    virtual OGRFeatureDefn     *GetLayerDefn() override { return poFeatureDefn;}

    virtual int                 TestCapability( const char * ) override;

    virtual GIntBig     GetFeatureCount(int bForce = TRUE) override;
    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }
    virtual OGRSpatialReference *GetSpatialRef() override;
    virtual const char* GetFIDColumn() override;

    virtual GByte GetId() const { return nLayerID; }
    virtual void AddClassifyCode(unsigned nClassCode, const char *szName = nullptr);
    virtual bool AddRecord( long nFID, unsigned nClassCode,
                            vsi_l_offset nOffset, bool bHasSemantic,
                            size_t nSemanticsSize );
private:
    static int CanRecode(const char* pszEncoding);
};

/************************************************************************/
/*                        OGRSXFDataSource                       */
/************************************************************************/

class OGRSXFDataSource final: public OGRDataSource
{
    SXFPassport oSXFPassport;

    CPLString               pszName;

    OGRLayer**          papoLayers;
    size_t              nLayers;

    VSILFILE* fpSXF;
    CPLMutex  *hIOMutex;
    void FillLayers();
    void CreateLayers();
    void CreateLayers(VSILFILE* fpRSC, const char* const* papszOpenOpts);
    static OGRErr ReadSXFInformationFlags(VSILFILE* fpSXF, SXFPassport& passport);
    OGRErr ReadSXFDescription(VSILFILE* fpSXF, SXFPassport& passport);
    static void SetVertCS(const long iVCS, SXFPassport& passport,
                          const char* const* papszOpenOpts);
    static OGRErr ReadSXFMapDescription(VSILFILE* fpSXF, SXFPassport& passport,
                                        const char* const* papszOpenOpts);
    OGRSXFLayer*       GetLayerById(GByte);
public:
                        OGRSXFDataSource();
                        virtual ~OGRSXFDataSource();

    int                 Open(const char * pszFilename, bool bUpdate,
                             const char* const* papszOpenOpts = nullptr );

    virtual const char*     GetName() override { return pszName; }

    virtual int             GetLayerCount() override { return static_cast<int>(nLayers); }
    virtual OGRLayer*       GetLayer( int ) override;

    virtual int             TestCapability( const char * ) override;
    void                    CloseFile();
};

/************************************************************************/
/*                         OGRSXFDriver                          */
/************************************************************************/

class OGRSXFDriver final: public GDALDriver
{
  public:
                ~OGRSXFDriver();

    static GDALDataset* Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
    static CPLErr       DeleteDataSource(const char* pszName);
};

#endif
