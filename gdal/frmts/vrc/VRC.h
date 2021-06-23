/******************************************************************************
 * $Id: VRC.h,v 1.23 2021/06/21 13:03:42 werdna Exp $
 *
 * Author:  Andrew C Aitchison
 *
 ******************************************************************************
 * Copyright (c) 2019-21, Andrew C Aitchison
 *****************************************************************************/

#pragma once

#ifndef VRC_H_INCLUDED
#define VRC_H_INCLUDED

#ifdef FRMT_vrc
#define FRMT_viewranger
#endif

#pragma clang diagnostic push
// #pragma clang diagnostic ignored "-Wformat-pedantic"
#pragma clang diagnostic ignored "-Wdocumentation-unknown-command"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"
#pragma clang diagnostic ignored "-Winconsistent-missing-destructor-override"
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#pragma clang diagnostic ignored "-Wsuggest-destructor-override"
#pragma clang diagnostic ignored "-Wunknown-warning-option"
#pragma clang diagnostic ignored "-Wundef"
#pragma clang diagnostic ignored "-Wunused-template"
#pragma clang diagnostic ignored "-Wweak-vtables"
#include "gdal_pam.h"
#pragma clang diagnostic pop
#include "ogr_spatialref.h"
//#include "cpl_string.h"

// We have not fully deciphered the data format
// of VRC files with magic=0x01ce6336.
// Set *one* of these definitions (to 1)
// VRC36_PIXEL_IS_PIXEL is to be assumed if none are set.
// #define VRC36_PIXEL_IS_PIXEL 1
#define VRC36_PIXEL_IS_TILE 1
// #define VRC36_PIXEL_IS_FILE 1

#if GDAL_VERSION_NUM < 2010000
#define GDAL_IDENTIFY_UNKNOWN -1
#define GDAL_IDENTIFY_FALSE 0
#define GDAL_IDENTIFY_TRUE 1
#define CPLsnprintf snprintf
#else
// These are defined in gdal/gdal_priv.h
#endif // GDAL_VERSION_NUM < 2010000

#if 0 // __cplusplus <= 201103L
#define override
#define nullptr 0
#endif

#ifdef CODE_ANALYSIS

// Printing variables with CPLDebug can hide
// the fact that they are not otherwise used ...
// ... but this also confuse other checks, such as
// Found duplicate branches for 'if' and 'else'
// 
#define CPLDebug(...)

#endif // CODE_ANALYSIS



static const unsigned int vrc_magic_metres = 0x002e1f7e; // 0x7e1f2e00; //
static const unsigned int vrc_magic_thirtysix = 0x01ce6336; // 0x3663ce01; //

// static const unsigned int nVRCNoData = 255;
// static const unsigned int nVRCNoData = 0xffffffff;
static const unsigned int nVRCNoData = 0;

class VRCRasterBand;

int VRReadChar(VSILFILE *fp);
int VRReadInt(VSILFILE *fp);
void VRC_file_strerror_r(int nFileErr, char *buf, size_t buflen);

enum VRCinterleave {band, pixel};

void dumpPPM(unsigned int width,
             unsigned int height,
             unsigned char* const data,
             unsigned int rowlength,
             CPLString osBaseLabel,
             VRCinterleave eInterleave,
             unsigned int nMaxPPM
             );

OGRSpatialReference* CRSfromCountry(int nCountry);
extern const char* CharsetFromCountry(int nCountry);


/************************************************************************/
/* ==================================================================== */
/*                         VRCDataset                                   */
/* ==================================================================== */
/************************************************************************/

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
class VRCDataset : public GDALPamDataset
{
    friend class VRCRasterBand;
    
    VSILFILE            *fp;
    GDALColorTable      *poColorTable;
    GByte       abyHeader[0x5a0];
    
    unsigned int *anColumnIndex;
    unsigned int *anTileIndex;
    unsigned int nMagic;
    double dfPixelMetres;
    signed int nMapID;
    signed int nLeft, nRight, nTop, nBottom;
    signed int nTopSkipPix, nRightSkipPix;
    unsigned int nScale;
    unsigned int nMaxOverviewCount;
    short nCountry;
    OGRSpatialReference* poSRS = nullptr;

    std::string sLongTitle;
    std::string sCopyright;
    // std::string sDatum;

    unsigned int tileSizeMax, tileSizeMin;
    int tileXcount, tileYcount;

    unsigned int* VRCGetTileIndex( unsigned int nTileIndexStart );
    unsigned int* VRCBuildTileIndex( unsigned int nTileIndexStart );

    VSIStatBufL oStatBufL;

public:
    VRCDataset();
    ~VRCDataset() override;

#if 0    
    virtual CPLErr IBuildOverviews( const char *pszResampling,
                                    int nOverviewCount, int *panOverviewList,
                                    int nListBands, int *panBandList,
                                    GDALProgressFunc pfnProgress,
                                    void * pProgressData ) override;
#endif

    static GDALDataset *Open( GDALOpenInfo * );
    static int Identify( GDALOpenInfo * );

    // Gdal <3 uses proj.4, Gdal>=3 uses proj.6, see eg:
    // https://trac.osgeo.org/gdal/wiki/rfc73_proj6_wkt2_srsbarn
    // https://gdal.org/development/rfc/rfc73_proj6_wkt2_srsbarn.html
#if GDAL_VERSION_MAJOR >= 3
    const OGRSpatialReference* GetSpatialRef() const override {
        return poSRS;
    }
    // const char *_GetProjectionRef
#else
    const char* GetProjectionRef() override {
        char* pszSRS=nullptr;
        if(!poSRS) {
            poSRS = CRSfromCountry(nCountry);
        }
        if(poSRS) {
            poSRS->exportToWkt( &pszSRS );
            // delete poSRS; ???
        }
        CPLDebug("VRC",
                 "GetProjectionRef() returns %s",
                 pszSRS);
        return pszSRS;
    }
#endif

#if 1 // GeoLoc
    CPLErr GetGeoTransform( double * padfTransform ) override;
#endif

    static char *VRCGetString( VSILFILE *fp, unsigned int byteaddr );
}; // class VRCDataset

#pragma clang diagnostic pop


/************************************************************************/
/* ==================================================================== */
/*                            VRCRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class VRCRasterBand : public GDALPamRasterBand
{
    friend class VRCDataset;
    
    // GUIntBig    nRecordSize;
    
    GDALColorInterp  eBandInterp;
    int          nThisOverview;  // -1 for base ?
    unsigned int nResFactor;
    int          nOverviewCount;
    VRCRasterBand**  papoOverviewBands;

    void read_VRC_Tile_ThirtySix( VSILFILE *fp,
                                int block_xx, int block_yy,
                                void *pImage);
    void read_VRC_Tile_Metres( VSILFILE *fp,
                               int block_xx, int block_yy,
                               void *pImage);
    GByte* read_PNG(VSILFILE *fp,
                    // void *pImage,
                    unsigned int *pPNGwidth,
                    unsigned int *pPNGheight,
                    unsigned int nVRCHeader,
                    vsi_l_offset nPalette,
                    unsigned int nVRCDataLen,
                    // int nPNGXcount,  int nPNGYcount,
                    int nGDtile_xx, int nGDtile_yy,
                    unsigned int nVRtile_xx, unsigned int nVRtile_yy
                    );

    int Copy_Tile_into_Block(
                           GByte* pbyPNGbuffer,
                           int nPNGwidth,
                            int nPNGheight,
                           int nLeftCol,
                           int nRightCol,
                           int nTopRow,
                           int nBottomRow,
                           void* pImage
                           // , int nBlockXSize,
                           // , int nBlockYSize
                           );

    int Shrink_Tile_into_Block(
                           GByte* pbyPNGbuffer,
                           int nPNGwidth,
                           int nPNGheight,
                           int nLeftCol,
                           int nRightCol,
                           int nTopRow,
                           int nBottomRow,
                           void* pImage
                           // , int nBlockXSize,
                           // , int nBlockYSize
                           );

    int verifySubTileFile(
                       VSILFILE *fp,
                       unsigned long start,
                       unsigned long finish,
                       int nGDtile_xx,
                       int nGDtile_yy,
                       unsigned int nVRtile_xx,
                       unsigned int nVRtile_yy
                       );
    int verifySubTileMem(
                       GByte abyRawStartData[],
                       unsigned long start,
                       unsigned long finish,
                       int nGDtile_xx,
                       int nGDtile_yy,
                       unsigned int nVRtile_xx,
                       unsigned int nVRtile_yy
                       );

public:

    VRCRasterBand(VRCDataset *poDSIn,
                  int nBandIn,
                  int nThisOverviewIn,
                  int nOverviewCountIn,
                  VRCRasterBand**papoOverviewBandsIn);

#if 0
    VRCRasterBand(VRCDataset *pVRDS,
                  int nBandIn,
                  int nThisOverviewIn )
    {
         VRCRasterBand( pVRDS, nBandIn, nThisOverviewIn,
                        6, nullptr);
    }
#endif
    
    ~VRCRasterBand() override;
    
    virtual GDALColorInterp GetColorInterpretation() override;
    virtual GDALColorTable  *GetColorTable() override;

    // virtual double GetNoDataValue( int *pbSuccess = NULL );
    virtual double GetNoDataValue( int *) override;
    // virtual CPLErr SetNoDataValue( double );
    
    virtual int GetOverviewCount() override;
    virtual GDALRasterBand *GetOverview(int) override;
#if 0
    void EstablishOverviews();
    virtual CPLErr CleanOverviews();

    virtual CPLErr BuildOverviews( const char *pszResampling,
                                   int nReqOverviews, int *panOverviewList,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData ) override;
#endif
    virtual CPLErr IReadBlock( int, int, void * ) override;

#if GDAL_VERSION_NUM >= 2020000
    virtual int IGetDataCoverageStatus( int nXOff, int nYOff,
                                        int nXSize, int nYSize,
                                        int nMaskFlagStop,
                                        double* pdfDataPct) override;
#endif
}; // class VRCRasterBand 

extern void
dumpTileHeaderData(
                   VSILFILE *fp,
                   unsigned int nTileIndex,
                   unsigned int nOverviewCount,
                   unsigned int anTileOverviewIndex[],
                   const int tile_xx, const int tile_yy );

extern short VRGetShort( void* base, int byteOffset );
extern signed int VRGetInt( void* base, unsigned int byteOffset );
extern unsigned int VRGetUInt( void* base, unsigned int byteOffset );

extern int VRReadChar(VSILFILE *fp);
extern int VRReadShort(VSILFILE *fp);
extern int VRReadInt(VSILFILE *fp);
extern int VRReadInt(VSILFILE *fp, unsigned int byteOffset );
extern unsigned int VRReadUInt(VSILFILE *fp);
extern unsigned int VRReadUInt(VSILFILE *fp, unsigned int byteOffset );

#endif // ndef VRC_H_INCLUDED
