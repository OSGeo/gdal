/******************************************************************************
 * $Id$
 *
 * Project:  PCIDSK Database File
 * Purpose:  PCIDSK driver declarations.
 * Author:   Andrey Kiselev, dron@remotesensing.org
 *
 ******************************************************************************
 * Copyright (c) 2003, Andrey Kiselev <dron@remotesensing.org>
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"
#include "rawdataset.h"

typedef enum
{
    PDI_PIXEL,
    PDI_BAND,
    PDI_FILE
} PCIDSKInterleaving;

/************************************************************************/
/*                              PCIDSKDataset                           */
/************************************************************************/

class PCIDSKDataset : public RawDataset
{
    friend class PCIDSKRawRasterBand;
    friend class PCIDSKTiledRasterBand;

    const char          *pszFilename;
    VSILFILE            *fp;

    vsi_l_offset        nFileSize;

    char                *pszCreatTime;  // Date/time of the database creation

    vsi_l_offset        nGeoPtrOffset;  // Offset in bytes to the pointer
                                        // to GEO segment
    vsi_l_offset        nGeoOffset;     // Offset in bytes to the GEO segment
    vsi_l_offset        nGcpPtrOffset;  // Offset in bytes to the pointer
                                        // to GCP segment
    vsi_l_offset        nGcpOffset;     // Offset in bytes to the GCP segment

    int                 bGeoSegmentDirty;
    int		        bGeoTransformValid;

    int                 nBlockMapSeg;

    GDAL_GCP            *pasGCPList;
    long                 nGCPCount;

    double              adfGeoTransform[6];
    char                *pszProjection;
    char                *pszGCPProjection;

    GDALDataType  PCIDSKTypeToGDAL( const char *);
    void          WriteGeoSegment();

    void          CollectPCIDSKMetadata( int nSegment );

    // Segment map
    int           nSegCount;
    int          *panSegType;
    char        **papszSegName;
    vsi_l_offset *panSegOffset;
    vsi_l_offset *panSegSize;

    int         nBandFileCount;
    VSILFILE  **pafpBandFiles;

  public:
                PCIDSKDataset();
                ~PCIDSKDataset();

    static int           Identify( GDALOpenInfo * );
    static GDALDataset  *Open( GDALOpenInfo * );
    static GDALDataset  *Create( const char * pszFilename,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType, char **papszParmList );
    static GDALDataset *CreateCopy( const char * pszFilename, 
                                    GDALDataset *poSrcDS, 
                                    int bStrict, char ** papszOptions, 
                                    GDALProgressFunc pfnProgress, 
                                    void * pProgressData );

    virtual void        FlushCache( void );

    CPLErr              GetGeoTransform( double * padfTransform );
    virtual CPLErr      SetGeoTransform( double * );
    const char          *GetProjectionRef();
    virtual CPLErr      SetProjection( const char * );
    virtual int         GetGCPCount();
    virtual const       char *GetGCPProjection();
    virtual const       GDAL_GCP *GetGCPs();

    // pcidsk specific
    int                 SegRead( int nSegment,
                                 vsi_l_offset nOffset,
                                 int nSize,
                                 void *pBuffer );
};

/************************************************************************/
/*                         PCIDSKTiledRasterBand                        */
/************************************************************************/

class PCIDSKTiledRasterBand : public GDALPamRasterBand
{
    friend class PCIDSKDataset;

    PCIDSKDataset *poPDS;
    
    int          nImage;

    int           nBlocks;
    vsi_l_offset *panBlockOffset;// offset in physical file.

    int           nTileCount;
    vsi_l_offset *panTileOffset; // offset in "image" virtual file.
    int          *panTileSize;

    int         nOverviewCount;
    GDALRasterBand **papoOverviews;

    char        szCompression[9];
    
    void        AttachOverview( GDALRasterBand *poOvBand ) {

        nOverviewCount++;
        papoOverviews = (GDALRasterBand **)
            CPLRealloc(papoOverviews,sizeof(void*) * nOverviewCount);
        papoOverviews[nOverviewCount-1] = poOvBand;
    }

    int         BuildBlockMap();
    int         BuildTileMap();
    
  public:
                PCIDSKTiledRasterBand( PCIDSKDataset *, int, int );
                ~PCIDSKTiledRasterBand();

    virtual CPLErr IReadBlock( int, int, void * );

    int         SysRead( vsi_l_offset nOffset, int nSize, void * );

    virtual int GetOverviewCount() { return nOverviewCount; }
    virtual GDALRasterBand *GetOverview(int iOverview)
        { return papoOverviews[iOverview]; }
};

/************************************************************************/
/*                         PCIDSKRawRasterBand                          */
/************************************************************************/

class PCIDSKRawRasterBand : public RawRasterBand
{
    friend class PCIDSKDataset;

    int         nOverviewCount;
    GDALRasterBand **papoOverviews;
    
    void        AttachOverview( GDALRasterBand *poOvBand ) {
        nOverviewCount++;
        papoOverviews = (GDALRasterBand **)
            CPLRealloc(papoOverviews,sizeof(void*) * nOverviewCount);
        papoOverviews[nOverviewCount-1] = poOvBand;
    }
    
  public:
    PCIDSKRawRasterBand( GDALDataset *poDS, int nBand, VSILFILE * fpRaw,
                         vsi_l_offset nImgOffset, int nPixelOffset,
                         int nLineOffset,
                         GDALDataType eDataType, int bNativeOrder )
        : RawRasterBand( poDS, nBand, fpRaw, nImgOffset, nPixelOffset,
                         nLineOffset, eDataType, bNativeOrder, TRUE ) {
        nOverviewCount = 0;
        papoOverviews = NULL;
    }
    ~PCIDSKRawRasterBand() {
        FlushCache();
        for( int i = 0; i < nOverviewCount; i++ )
            delete papoOverviews[i];
        CPLFree( papoOverviews );
    }

    virtual int GetOverviewCount() { 
        if (nOverviewCount > 0)
            return nOverviewCount; 

        return RawRasterBand::GetOverviewCount();
    }
    virtual GDALRasterBand *GetOverview(int iOverview) { 
        if (iOverview < nOverviewCount)
            return papoOverviews[iOverview]; 

        return RawRasterBand::GetOverview(iOverview);
    }
};


