/******************************************************************************
 * $Id: ogr_sxf.h  $
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

#ifndef _OGR_SXF_H_INCLUDED
#define _OGR_SXF_H_INCLUDED

#include <set>
#include <vector>
#include <map>

#include "ogrsf_frmts.h"
#include "org_sxf_defs.h"

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))
#define TO_DEGREES 57.2957795130823208766
#define TO_RADIANS 0.017453292519943295769

/************************************************************************/
/*                         OGRSXFLayer                                */
/************************************************************************/
class OGRSXFLayer : public OGRLayer
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
    void            **m_hIOMutex;
    double              m_dfCoeff;
    virtual OGRFeature *       GetNextRawFeature(long nFID);

    GUInt32 TranslateXYH(const SXFRecordDescription& certifInfo,
                         const char *psBuff, GUInt32 nBufLen,
                         double *dfX, double *dfY, double *dfH = NULL);


    OGRFeature *TranslatePoint(const SXFRecordDescription& certifInfo, const char * psRecordBuf, GUInt32 nBufLen);
    OGRFeature *TranslateText(const SXFRecordDescription& certifInfo, const char * psBuff, GUInt32 nBufLen);
    OGRFeature *TranslatePolygon(const SXFRecordDescription& certifInfo, const char * psBuff, GUInt32 nBufLen);
    OGRFeature *TranslateLine(const SXFRecordDescription& certifInfo, const char * psBuff, GUInt32 nBufLen);
    OGRFeature *TranslateVetorAngle(const SXFRecordDescription& certifInfo, const char * psBuff, GUInt32 nBufLen);
public:
    OGRSXFLayer(VSILFILE* fp, void** hIOMutex, GByte nID, const char* pszLayerName, int nVer, const SXFMapDescription&  sxfMapDesc);
    ~OGRSXFLayer();

	virtual void                ResetReading();
    virtual OGRFeature         *GetNextFeature();
    virtual OGRErr              SetNextByIndex(long nIndex);
    virtual OGRFeature         *GetFeature(long nFID);
    virtual OGRFeatureDefn     *GetLayerDefn() { return poFeatureDefn;}

    virtual int                 TestCapability( const char * );

    virtual int         GetFeatureCount(int bForce = TRUE);
    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE);
    virtual OGRSpatialReference *GetSpatialRef();
    virtual const char* GetFIDColumn();

    virtual GByte GetId() const { return nLayerID; };
    virtual void AddClassifyCode(unsigned nClassCode, const char *szName = NULL);
    virtual int AddRecord(long nFID, unsigned nClassCode, vsi_l_offset nOffset, bool bHasSemantic, int nSemanticsSize);
};


/************************************************************************/
/*                        OGRSXFDataSource                       */
/************************************************************************/

class OGRSXFDataSource : public OGRDataSource
{
    SXFPassport oSXFPassport;

    CPLString               pszName;

    OGRLayer**          papoLayers;
    size_t              nLayers;

    VSILFILE* fpSXF;    
    void  *hIOMutex;
    void FillLayers(void);
    void CreateLayers();
    void CreateLayers(VSILFILE* fpRSC);
    OGRErr ReadSXFInformationFlags(VSILFILE* fpSXF, SXFPassport& passport);
    OGRErr ReadSXFDescription(VSILFILE* fpSXF, SXFPassport& passport);
    void SetVertCS(const long iVCS, SXFPassport& passport);
    OGRErr ReadSXFMapDescription(VSILFILE* fpSXF, SXFPassport& passport);
    OGRSXFLayer*       GetLayerById(GByte);
public:
                        OGRSXFDataSource();
                        ~OGRSXFDataSource();

    int                 Open( const char * pszFilename,
                              int bUpdate );

    virtual const char*     GetName() { return pszName; }

    virtual int             GetLayerCount() { return nLayers; }
    virtual OGRLayer*       GetLayer( int );

    virtual int             TestCapability( const char * );
    void                    CloseFile(); 
};

/************************************************************************/
/*                         OGRSXFDriver                          */
/************************************************************************/

class OGRSXFDriver : public OGRSFDriver
{
  public:
                ~OGRSXFDriver();

    const char*     GetName();
    OGRDataSource*  Open( const char *, int );
    OGRErr          DeleteDataSource(const char* pszName);
    int             TestCapability(const char *);
};

#endif 
