/******************************************************************************
 *
 * Project:  ILWIS Driver
 * Purpose:  GDALDataset driver for ILWIS translator for read/write support.
 * Author:   Lichun Wang, lichun@itc.nl
 *
 ******************************************************************************
 * Copyright (c) 2004, ITC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef ILWISDATASET_H_INCLUDED
#define ILWISDATASET_H_INCLUDED

#include "gdal_pam.h"
#include "cpl_csv.h"
#include "ogr_spatialref.h"

#include <cstdio>
#include <cstdlib>

#include <map>
#include <string>

#define shUNDEF -32767
#define iUNDEF -2147483647
#define flUNDEF ((float)-1e38)
#define rUNDEF ((double)-1e308)

namespace GDAL
{

enum ilwisStoreType
{
    stByte,
    stInt,
    stLong,
    stFloat,
    stReal
};

class ValueRange
{
  public:
    ValueRange(double min, double max);  // step = 1
    ValueRange(double min, double max, double step);
    explicit ValueRange(const std::string &str);
    std::string ToString() const;

    ilwisStoreType get_NeededStoreType() const
    {
        return st;
    }

    double get_rLo() const
    {
        return _rLo;
    }

    double get_rHi() const
    {
        return _rHi;
    }

    double get_rStep() const
    {
        return _rStep;
    }

    double get_rRaw0() const
    {
        return _r0;
    }

    int get_iDec() const
    {
        return _iDec;
    }

    double rValue(int raw) const;
    int iRaw(double value) const;

  private:
    void init(double rRaw0);
    void init();
    double _rLo, _rHi;
    double _rStep;
    int _iDec;
    double _r0;
    int iRawUndef;
    short _iWidth;
    ilwisStoreType st;
};

/************************************************************************/
/*                     ILWISInfo                                        */
/************************************************************************/

struct ILWISInfo
{
    ILWISInfo() : bUseValueRange(false), vr(0, 0), stStoreType(stByte)
    {
    }

    bool bUseValueRange;
    ValueRange vr;
    ilwisStoreType stStoreType;
    std::string stDomain;
};

/************************************************************************/
/*                           ILWISRasterBand                            */
/************************************************************************/

class ILWISDataset;

class ILWISRasterBand final : public GDALPamRasterBand
{
    friend class ILWISDataset;

  public:
    VSILFILE *fpRaw;
    ILWISInfo psInfo;
    int nSizePerPixel;

    ILWISRasterBand(ILWISDataset *, int, const std::string &sBandNameIn);
    virtual ~ILWISRasterBand();
    CPLErr GetILWISInfo(const std::string &pszFileName);
    void ILWISOpen(const std::string &pszFilename);

    virtual CPLErr IReadBlock(int, int, void *) override;
    virtual CPLErr IWriteBlock(int, int, void *) override;
    virtual double GetNoDataValue(int *pbSuccess) override;

  private:
    void FillWithNoData(void *pImage);
    void SetValue(void *pImage, int i, double rV);
    double GetValue(void *pImage, int i);
    void ReadValueDomainProperties(const std::string &pszFileName);
};

/************************************************************************/
/*                         ILWISDataset                                 */
/************************************************************************/
class ILWISDataset final : public GDALPamDataset
{
    friend class ILWISRasterBand;
    CPLString osFileName;
    std::string pszIlwFileName;
    OGRSpatialReference m_oSRS{};
    GDALGeoTransform m_gt{};
    int bGeoDirty;
    int bNewDataset;          /* product of Create() */
    std::string pszFileType;  // indicating the input dataset: Map/MapList
    CPLErr ReadProjection(const std::string &csyFileName);
    CPLErr WriteProjection();
    void WriteGeoReference();
    void CollectTransformCoef(std::string &pszRefFile);

  public:
    ILWISDataset();
    virtual ~ILWISDataset();

    static GDALDataset *Open(GDALOpenInfo *);

    static GDALDataset *CreateCopy(const char *pszFilename,
                                   GDALDataset *poSrcDS, int bStrict,
                                   char **papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData);

    static GDALDataset *Create(const char *pszFilename, int nXSize, int nYSize,
                               int nBands, GDALDataType eType,
                               char **papszParamList);

    virtual CPLErr GetGeoTransform(GDALGeoTransform &gt) const override;
    virtual CPLErr SetGeoTransform(const GDALGeoTransform &gt) override;

    const OGRSpatialReference *GetSpatialRef() const override;
    CPLErr SetSpatialRef(const OGRSpatialReference *poSRS) override;

    virtual CPLErr FlushCache(bool bAtClosing) override;
};

// IniFile.h: interface for the IniFile class.
//
//////////////////////////////////////////////////////////////////////

class CompareAsNum
{
  public:
    bool operator()(const std::string &, const std::string &) const;
};

typedef std::map<std::string, std::string> SectionEntries;
typedef std::map<std::string, SectionEntries *> Sections;

class IniFile
{
  public:
    explicit IniFile(const std::string &filename);
    virtual ~IniFile();

    void SetKeyValue(const std::string &section, const std::string &key,
                     const std::string &value);
    std::string GetKeyValue(const std::string &section, const std::string &key);

    void RemoveKeyValue(const std::string &section, const std::string &key);
    void RemoveSection(const std::string &section);

  private:
    std::string filename;
    Sections sections;
    bool bChanged;

    void Load();
    void Store();
};

std::string ReadElement(const std::string &section, const std::string &entry,
                        const std::string &filename);
bool WriteElement(const std::string &sSection, const std::string &sEntry,
                  const std::string &fn, const std::string &sValue);
bool WriteElement(const std::string &sSection, const std::string &sEntry,
                  const std::string &fn, int nValue);
bool WriteElement(const std::string &sSection, const std::string &sEntry,
                  const std::string &fn, double dValue);

}  // namespace GDAL

#endif  // ILWISDATASET_H_INCLUDED
