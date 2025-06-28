/******************************************************************************
 *
 * Project:  SXF Translator
 * Purpose:  Include file defining classes for OGR SXF driver, datasource and
 *layers. Author:   Ben Ahmed Daho Ali, bidandou(at)yahoo(dot)fr Dmitry
 *Baryshnikov, polimax@mail.ru Alexandr Lisovenko, alexander.lisovenko@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2011, Ben Ahmed Daho Ali
 * Copyright (c) 2013, NextGIS
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_SXF_H_INCLUDED
#define OGR_SXF_H_INCLUDED

#include <set>
#include <vector>
#include <map>

#include "ogrsf_frmts.h"
#include "org_sxf_defs.h"

#define CHECK_BIT(var, pos) (((var) & (1 << (pos))) != 0)
constexpr double TO_DEGREES = 180.0 / M_PI;
constexpr double TO_RADIANS = M_PI / 180.0;

/************************************************************************/
/*                         OGRSXFLayer                                */
/************************************************************************/
class OGRSXFLayer final : public OGRLayer
{
  protected:
    OGRFeatureDefn *poFeatureDefn;
    VSILFILE *fpSXF;
    GByte nLayerID;
    std::map<unsigned, CPLString> mnClassificators;
    std::map<long, vsi_l_offset> mnRecordDesc;
    std::map<long, vsi_l_offset>::const_iterator oNextIt;
    SXFMapDescription stSXFMapDescription;
    std::set<GUInt16> snAttributeCodes;
    int m_nSXFFormatVer;
    CPLString sFIDColumn_;
    CPLMutex **m_hIOMutex;
    double m_dfCoeff;
    OGRFeature *GetNextRawFeature(long nFID);

    GUInt32 TranslateXYH(const SXFRecordDescription &certifInfo,
                         const char *psBuff, GUInt32 nBufLen, double *dfX,
                         double *dfY, double *dfH = nullptr);

    OGRFeature *TranslatePoint(const SXFRecordDescription &certifInfo,
                               const char *psRecordBuf, GUInt32 nBufLen);
    OGRFeature *TranslateText(const SXFRecordDescription &certifInfo,
                              const char *psBuff, GUInt32 nBufLen);
    OGRFeature *TranslatePolygon(const SXFRecordDescription &certifInfo,
                                 const char *psBuff, GUInt32 nBufLen);
    OGRFeature *TranslateLine(const SXFRecordDescription &certifInfo,
                              const char *psBuff, GUInt32 nBufLen);
    OGRFeature *TranslateVetorAngle(const SXFRecordDescription &certifInfo,
                                    const char *psBuff, GUInt32 nBufLen);

  public:
    OGRSXFLayer(VSILFILE *fp, CPLMutex **hIOMutex, GByte nID,
                const char *pszLayerName, int nVer,
                const SXFMapDescription &sxfMapDesc);
    virtual ~OGRSXFLayer();

    virtual void ResetReading() override;
    virtual OGRFeature *GetNextFeature() override;
    virtual OGRErr SetNextByIndex(GIntBig nIndex) override;
    virtual OGRFeature *GetFeature(GIntBig nFID) override;

    virtual OGRFeatureDefn *GetLayerDefn() override
    {
        return poFeatureDefn;
    }

    virtual int TestCapability(const char *) override;

    virtual GIntBig GetFeatureCount(int bForce = TRUE) override;
    virtual OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                              bool bForce) override;

    virtual OGRSpatialReference *GetSpatialRef() override;
    virtual const char *GetFIDColumn() override;

    GByte GetId() const
    {
        return nLayerID;
    }

    void AddClassifyCode(unsigned nClassCode, const char *szName = nullptr);
    bool AddRecord(long nFID, unsigned nClassCode, vsi_l_offset nOffset,
                   bool bHasSemantic, size_t nSemanticsSize);

  private:
    static int CanRecode(const char *pszEncoding);
};

/************************************************************************/
/*                           OGRSXFDataSource                           */
/************************************************************************/

class OGRSXFDataSource final : public GDALDataset
{
    SXFPassport oSXFPassport;

    std::vector<std::unique_ptr<OGRSXFLayer>> m_apoLayers{};

    VSILFILE *fpSXF = nullptr;
    CPLMutex *hIOMutex = nullptr;
    void FillLayers();
    void CreateLayers();
    void CreateLayers(VSILFILE *fpRSC, const char *const *papszOpenOpts);
    static OGRErr ReadSXFInformationFlags(VSILFILE *fpSXF,
                                          SXFPassport &passport);
    OGRErr ReadSXFDescription(VSILFILE *fpSXF, SXFPassport &passport);
    static void SetVertCS(const long iVCS, SXFPassport &passport,
                          const char *const *papszOpenOpts);
    static OGRErr ReadSXFMapDescription(VSILFILE *fpSXF, SXFPassport &passport,
                                        const char *const *papszOpenOpts);
    OGRSXFLayer *GetLayerById(GByte);

  public:
    OGRSXFDataSource();
    virtual ~OGRSXFDataSource();

    int Open(const char *pszFilename, bool bUpdate,
             const char *const *papszOpenOpts = nullptr);

    virtual int GetLayerCount() override
    {
        return static_cast<int>(m_apoLayers.size());
    }

    virtual OGRLayer *GetLayer(int) override;

    virtual int TestCapability(const char *) override;
    void CloseFile();
};

#endif
