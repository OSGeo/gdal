/******************************************************************************
 *
 * Project:  VICAR Driver; JPL/MIPL VICAR Format
 * Purpose:  Implementation of VICARDataset
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2014, Sebastian Walter <sebastian dot walter at fu-berlin dot de>
 * Copyright (c) 2019, Even Rouault, <even.rouault at spatialys.com>
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

#ifndef VICARDATASET_H
#define VICARDATASET_H

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "ogr_spatialref.h"
#include "ogrsf_frmts.h"
#include "rawdataset.h"
#include "vicarkeywordhandler.h"
#include <array>

/************************************************************************/
/* ==================================================================== */
/*                             VICARDataset                             */
/* ==================================================================== */
/************************************************************************/

class VICARDataset final: public RawDataset
{
    friend class VICARRawRasterBand;
    friend class VICARBASICRasterBand;

    VSILFILE    *fpImage = nullptr;

    VICARKeywordHandler  oKeywords;

    enum CompressMethod
    {
        COMPRESS_NONE,
        COMPRESS_BASIC,
        COMPRESS_BASIC2,
    };
    CompressMethod m_eCompress = COMPRESS_NONE;

    int          m_nRecordSize = 0;
    vsi_l_offset m_nImageOffsetWithoutNBB = 0;
    int          m_nLastRecordOffset = 0;
    std::vector<vsi_l_offset> m_anRecordOffsets{}; // for BASIC/BASIC2
    std::vector<GByte> m_abyCodedBuffer{};
    vsi_l_offset m_nLabelSize = 0;

    CPLJSONObject m_oJSonLabel;
    CPLStringList m_aosVICARMD;

    bool        m_bGotTransform = false;
    std::array<double, 6> m_adfGeoTransform = {{0.0,1.0,0,0.0,0.0,1.0}};

    OGRSpatialReference m_oSRS;

    std::unique_ptr<OGRLayer> m_poLayer;

    bool          m_bGeoRefFormatIsMIPL = true;

    CPLString   m_osLatitudeType; // creation only
    CPLString   m_osLongitudeDirection; // creation only
    CPLString   m_osTargetName; // creation only
    bool          m_bIsLabelWritten = true; // creation only
    bool          m_bUseSrcLabel = true; // creation only
    bool          m_bUseSrcMap = false; // creation only
    bool          m_bInitToNodata = false; // creation only
    CPLJSONObject m_oSrcJSonLabel; // creation only

    const char *GetKeyword( const char *pszPath,
                            const char *pszDefault = "");
    void         WriteLabel();
    void         PatchLabel();
    void         BuildLabel();
    void         InvalidateLabel();

    static VICARDataset *CreateInternal( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszOptions );

    void ReadProjectionFromMapGroup();
    void ReadProjectionFromGeoTIFFGroup();
    void BuildLabelPropertyMap(CPLJSONObject& oLabel);
    void BuildLabelPropertyGeoTIFF(CPLJSONObject& oLabel);

public:
    VICARDataset();
    virtual ~VICARDataset();

    CPLErr GetGeoTransform( double * padfTransform ) override;
    CPLErr SetGeoTransform( double * padfTransform ) override;

    const OGRSpatialReference* GetSpatialRef() const override;
    CPLErr SetSpatialRef(const OGRSpatialReference* poSRS) override;

    bool GetRawBinaryLayout(GDALDataset::RawBinaryLayout&) override;

    char **GetMetadataDomainList() override;
    char **GetMetadata( const char* pszDomain = "" ) override;
    CPLErr SetMetadata( char** papszMD, const char* pszDomain = "" ) override;

    int GetLayerCount() override { return m_poLayer ? 1 : 0; }
    OGRLayer* GetLayer(int i) override {
        return (m_poLayer && i == 0) ? m_poLayer.get() : nullptr; }

    static int          Identify( GDALOpenInfo * );
    static int          GetLabelOffset( GDALOpenInfo * );
    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszOptions );
    static GDALDataset* CreateCopy( const char *pszFilename,
                                       GDALDataset *poSrcDS,
                                       int bStrict,
                                       char ** papszOptions,
                                       GDALProgressFunc pfnProgress,
                                       void * pProgressData );

    static GDALDataType GetDataTypeFromFormat(const char* pszFormat);
    static bool         GetSpacings(const VICARKeywordHandler& keywords,
                                    GUInt64& nPixelOffset,
                                    GUInt64& nLineOffset,
                                    GUInt64& nBandOffset,
                                    GUInt64& nImageOffsetWithoutNBB,
                                    GUInt64& nNBB,
                                    GUInt64& nImageSize);

    static vsi_l_offset GetVICARLabelOffsetFromPDS3(const char* pszHdr,
                                                    VSILFILE* fp,
                                                    std::string& osVICARHeader);
};

#endif // VICARDATASET_H
