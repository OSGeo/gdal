/******************************************************************************
 * $Id: VRCthirtysix.cpp,v 1.20 2021/06/26 20:37:41 werdna Exp $
 *
 * Author:  Andrew C Aitchison
 *
 ******************************************************************************
 * Copyright (c) 2019, Andrew C Aitchison
 *****************************************************************************/

// #ifdef FRMT_vrc

#include "VRC.h"

#ifdef CODE_ANALYSIS

// Printing variables with CPLDebug can hide
// the fact that they are not otherwise used ...
#define CPLDebug(...)

#endif // CODE_ANALYSIS

// Like strncmp but null bytes don't terminate.
// Used in verifySubTileMem()
static
size_t bytesmatch(const unsigned char*data, const unsigned char*pattern, size_t nLen)
{
    size_t count=0;
    for (  ; count<nLen && data[count]==pattern[count]; count++) {
    }
    return count;
}


void VRCRasterBand::read_VRC_Tile_ThirtySix( VSILFILE *fp,
                                int block_xx, int block_yy,
                                void *pImage)
{
    auto *poGDS = dynamic_cast<VRCDataset *>(poDS);
    if (block_xx < 0 || block_xx >= nRasterXSize ) {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "read_VRC_Tile_ThirtySix invalid row %d", block_xx );
        return ;
    }
    if (block_yy < 0 || block_yy >= nRasterYSize ) {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "read_VRC_Tile_ThirtySix invalid column %d", block_yy );
        return ;
    }
    if (pImage == nullptr ) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "read_VRC_Tile_ThirtySix passed no image" );
        return ;
    }
    if (poGDS->nMagic != vrc_magic_thirtysix) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "read_VRC_Tile_ThirtySix called with wrong magic number x%08x",
                  poGDS->nMagic );
        return ;
    }
        
    CPLDebug("Viewranger", "read_VRC_Tile_ThirtySix(%p, %d, %d, %p)",
             static_cast<void*>(fp), block_xx, block_yy, pImage
             );

#if defined VRC36_PIXEL_IS_FILE
    // dummy pixel until we can read the real data
    if (block_xx != 0 || block_yy != 0 ) {
        CPLDebug("Viewranger",
                 "vrc36_pixel_is_file only supports one tile: %d %d requested",
                 block_xx, block_yy);
    }
    // static_cast<char*>(pImage)[0] = 1;
    // GDT_UInt32
    static_cast<GUInt32*>(pImage)[0] = 0xffbb7744;
    (void)fp; // We don't actually use the image data for VRC36_PIXEL_IS_FILE
#else

    int tilenum = poGDS->tileXcount * block_yy + block_xx;
#if defined VRC36_PIXEL_IS_TILE
    if (block_xx != 0 || block_yy != 0 ) {
        CPLDebug("Viewranger",
                 "vrc36_pixel_is_tile only supports one tile: %d %d requested",
                 block_xx, block_yy);
    }
    // The image is a single tile with one pixel for each VRC tile.
    // Since the tiles are divided into subtiles,
    // this is not particularly informative
    // - one pixel per subtile would be better.
    CPLDebug("Viewranger", "\traster %d x %d tilenum %d",
             poGDS->nRasterXSize, poGDS->nRasterYSize, tilenum );
    if (poGDS->nRasterXSize<=0 || poGDS->nRasterYSize<=0) {
        // No data to examine
        return;
    }
    auto nXsize = static_cast<unsigned int>(poGDS->nRasterXSize);
    auto nYsize = static_cast<unsigned int>(poGDS->nRasterYSize);
    // GDT_UInt32 ((char *) pImage)[0] = 1;
    // ((GUInt32 *) pImage)[tilenum] = 0xffbb7744; // temporary

    for (unsigned int i=0; i<nXsize * nYsize; i++) {
        // ((GUInt32 *) pImage)[i] = poGDS->anTileIndex[i];

        unsigned long nStart = poGDS->anTileIndex[i];
        unsigned long nFinish= poGDS->anTileIndex[i+1];
        long long nFileSize = poGDS->oStatBufL.st_size;
        CPLString osBaseLabel
            = CPLString().Printf("/tmp/werdna/vrc2tif/%s.%03d.%03d.%08lu.%02u",
                                 // CPLGetBasename(poOpenInfo->pszFilename) doesn't quite work
                                 poGDS->sLongTitle.c_str(),
                                 i / nYsize, i % nYsize , nStart, // nGDtile_xx, nGDtile_yy, // nVRtile_xx, nVRtile_yy,
                                 nBand);

        if (/* 0<=nStart && */ nStart<=nFinish
            && static_cast<long long>(nFinish) <= nFileSize) {
            int nVerifyResult=
                verifySubTileFile(fp,
                                  nStart, nFinish,
                                  block_xx, block_yy,
                                  //static_cast<unsigned int>
                                  (i / nYsize),
                                  //static_cast<unsigned int>
                                  (i % nYsize)
                                  );
            if (nVerifyResult<0) {
                static_cast<GUInt32*>(pImage)[tilenum] = nVRCNoData;
            } else {
                static_cast<GUInt32*>(pImage)[tilenum] = static_cast<GUInt32>(nVerifyResult);
                if (0 == (0x0100 & static_cast<GUInt32>(nVerifyResult))) {
                    CPLDebug("Viewranger",
                             "raw data for tile %u, %u did not verify\n",
                             i / nYsize, i % nYsize);
                }
            }
        } else {
            CPLDebug("Viewranger",
                     "skipping %s: expected 0 <= x%lx <= x%lx <= x%llx filesize",
                     osBaseLabel.c_str(),
                     nStart, nFinish, nFileSize
                     );
            static_cast<GUInt32*>(pImage)[i] = nVRCNoData;
        } // end range check 
    } // for  i<nXsize * nYsize
    
    // CPLDebug("Viewranger", "vrc36_pixel_is_tile set %d pixels", p);
#else
    // VRC36_PIXEL_IS_PIXEL
    // this will be the default
    CPLDebug("Viewranger", "vrc36_pixel_is_pixel only partially implemented");
    unsigned int nTileIndex = poGDS->anTileIndex[tilenum];
    // CPLDebug("Viewranger", "vrcmetres_pixel_is_pixel");
    CPLDebug("Viewranger", "\tblock %d x %d, (%d, %d) tilenum %d tileIndex x%08x",
             nBlockXSize,
             nBlockYSize,
             block_xx, block_yy,
             tilenum,
             nTileIndex
             );

    if (nTileIndex==0) {
        // No data for this tile
        CPLDebug("Viewranger",
                 "read_VRC_Tile_ThirtySix(.. %d %d ..) null tile",
                 block_xx, block_yy );

        if (eDataType==GDT_Byte) {
            for (int j=0; j < nBlockYSize ; j++) {
                int pixelnum = j * nBlockXSize;
                for (int i=0; i < nBlockXSize ; i++) {
                    static_cast<GUInt32*>(pImage)[pixelnum++] = nVRCNoData;
                }
            }
        } else {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "read_VRC_Tile_ThirtySix eDataType %d unexpected for null tile",
                      eDataType);
        }
        return;
    }  // nTileIndex==0 No data for this tile

    if ( VSIFSeekL( fp, nTileIndex, SEEK_SET ) ) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "cannot seek to tile header x%08x", nTileIndex );
        return;
    }

    if (poGDS->nMapID != 8) {
        nOverviewCount = VRReadInt(fp);
        if (nOverviewCount != 7) {
            CPLDebug("Viewranger OVRV", "read_VRC_Tile_Metres: nOverviewCount is %d - expected seven - MapID %d",
                nOverviewCount,
                poGDS->nMapID
            );
            return;
        }

        unsigned int anTileOverviewIndex[7]={};
        for (int ii=0; ii<nOverviewCount; ii++) {
            anTileOverviewIndex[ii] = VRReadUInt(fp);
        }
        CPLDebug("Viewranger OVRV",
                 "x%08x:  x%08x x%08x x%08x x%08x  x%08x x%08x x%08x x%08x",
                 nTileIndex, nOverviewCount,
                 anTileOverviewIndex[0],  anTileOverviewIndex[1],
                 anTileOverviewIndex[2],  anTileOverviewIndex[3],
                 anTileOverviewIndex[4],  anTileOverviewIndex[5],
                 anTileOverviewIndex[6]
                 );

        // VRC counts main image plus 6 overviews.
        // GDAL just counts the 6 overview images.
        // anTileOverviewIndex[0] points to the full image
        // ..[1-6] are the overviews:
        nOverviewCount--; // equals 6

        // If the smallest overviews do not exist, ignore them.
        // This saves this driver generating them from larger overviews,
        // they may need to be generated elsewhere ...
        while (nOverviewCount>0 && 0==anTileOverviewIndex[nOverviewCount]) {
            nOverviewCount--;
        }
        if (nOverviewCount<6) {
            CPLDebug("Viewranger OVRV",
                     "Overviews %d-6 not available",
                     1+nOverviewCount);
        }

        if (nOverviewCount<1 || anTileOverviewIndex[0] == 0) {
            CPLDebug("Viewranger",
                     "VRCRasterBand::read_VRC_Tile_Metres(.. %d %d ..) empty tile",
                     block_xx, block_yy );
            return; 
        }
    
        // This is just for the developer's understanding.
        if (0x20 + nTileIndex == anTileOverviewIndex[1]) {
            CPLDebug("Viewranger",
                     "anTileOverviewIndex[1] %d x%08x - 0x20 = %d x%08x as expected",
                     anTileOverviewIndex[1], anTileOverviewIndex[1],
                     nTileIndex, nTileIndex);
        } else {
            CPLDebug("Viewranger",
                     "anTileOverviewIndex[1] %d x%08x - anTileOverviewIndex[0] %d x%08x = %d x%08x - expected 0x20",
                     anTileOverviewIndex[1], anTileOverviewIndex[1],  
                     nTileIndex,           nTileIndex,
                     anTileOverviewIndex[1] - nTileIndex,
                     anTileOverviewIndex[1] - nTileIndex);
        }

        dumpTileHeaderData(fp, nTileIndex,
                           1+static_cast<unsigned int>(nOverviewCount),
                           anTileOverviewIndex,
                           block_xx, block_yy );

        if (nThisOverview < -1 || nThisOverview >= nOverviewCount) {
            CPLDebug("Viewranger",
                     "read_VRC_Tile_ThirtySix: overview %d=x%08x not in range [-1, %d]",
                     nThisOverview, nThisOverview, nOverviewCount);
            return;
        }

        if (anTileOverviewIndex[nThisOverview+1] >= poGDS->oStatBufL.st_size) {
            CPLDebug("Viewranger",
                     "\toverview level %d data beyond end of file at x%08x",
                     nThisOverview, anTileOverviewIndex[nThisOverview+1] );
            return ;
        }
        CPLDebug("Viewranger",
                 "\toverview level %d data at x%08x",
                 nThisOverview, anTileOverviewIndex[nThisOverview+1] );
    
        bool bTileShrink = (anTileOverviewIndex[nThisOverview+1]==0);
        // int nShrinkFactor=1;
        if (bTileShrink == false) {
            // nShrinkFactor = 1;
            if ( VSIFSeekL( fp, anTileOverviewIndex[nThisOverview+1], SEEK_SET ) ) {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "cannot seek to overview level %d data at x%08x",
                          nThisOverview, anTileOverviewIndex[nThisOverview+1] );
                return;
            }

            unsigned int nTileMax = poGDS->tileSizeMax;
            unsigned int nTileMin = poGDS->tileSizeMin;
            if (nTileMax==0) {
                CPLError( CE_Failure, CPLE_NotSupported,
                          "tileSizeMax is zero and invalid"
                          );
                return;
            }
            if (nTileMin==0) {
                poGDS->tileSizeMin=nTileMax;
                CPLDebug("Viewranger",
                         "nTileMin is zero. Using nTileMax %d",
                         nTileMax
                         );
            } else {
                int bits = 63 - __builtin_clzll
                    (static_cast<unsigned long long>(nTileMax/nTileMin) );
                if (nTileMax == nTileMin << bits) {
                    CPLDebug("Viewranger",
                             "%d / %d == %f == 2^%d",
                             nTileMax, nTileMin,
                             1.0*nTileMax/nTileMin, bits);
                } else {
                    CPLDebug("Viewranger",
                             "%d / %d == %f != 2^%d",
                             nTileMax, nTileMin,
                             1.0*nTileMax/nTileMin, bits);
                }
            }
        } else { // if(bTileShrink == false)
            // If data for this block is not available, we need to rescale another overview
            // perhaps with GDALRegenerateOverviews from gcore/overview.cpp
            
            CPLDebug("Viewranger",
                     "Band %d block %d,%d empty at overview %d\n",
                     nBand, block_xx, block_yy, nThisOverview
                     );
            GDALRasterBandH hOvrBandSrc =
                reinterpret_cast<GDALRasterBandH>(GetOverview( nThisOverview+1 ));
            GDALRasterBandH ahOvrBandTgts[1];
            ahOvrBandTgts[0] = reinterpret_cast<GDALRasterBandH>(GetOverview( nThisOverview+2 ));
            if (hOvrBandSrc==nullptr || ahOvrBandTgts[0]==nullptr) {
                CPLDebug("Viewranger",
                         "SrcBand %p, TargetBand %p\n",
                         hOvrBandSrc, ahOvrBandTgts[0]
                         );
                return;
            }
            CPLErr regErr =
                GDALRegenerateOverviews
                (hOvrBandSrc,
                 1,             // nOverviewCount,
                 &ahOvrBandTgts[0],   // GDALRasterBandH
                 "AVERAGE",     // const char * pszResampling,
                 nullptr,       // GDALProgressFunc
                 nullptr        // void * pProgressData 
                 );
            if (regErr!=CE_None) {
                CPLDebug("Viewranger",
                         "Band %d block %d,%d downsampling for overview %d failed: %d\n",
                         nBand, block_xx, block_yy, nThisOverview, regErr
                         );
            } else {
                CPLDebug("Viewranger",
                         "Band %d block %d,%d downsampling for overview %d succeeded\n",
                         nBand, block_xx, block_yy, nThisOverview
                         );
            }
            return;
        } // end else clause of if(bTileShrink == false) 
    } // nMapID != 8

    // We have reached the start of the tile
    // ... but it is split into subtiles (of a format yet to be determined)
    int nRawXcount = VRReadInt(fp);
    int nRawYcount = VRReadInt(fp);
    int nRawXsize  = VRReadInt(fp);
    int nRawYsize  = VRReadInt(fp);

    if (nRawXcount <=0) {
        CPLDebug("Viewranger",
                 "nRawXcount %d zero or negative in tilenum %d",
                 nRawXcount, tilenum );
        return;
    }
    if (nRawYcount <=0) {
        CPLDebug("Viewranger",
                 "nRawYcount %d zero or negative in tilenum %d",
                 nRawYcount, tilenum );
        return;
    }
    if (nRawXsize <=0) {
        CPLDebug("Viewranger",
                 "nRawXsize %d zero or negative in tilenum %d",
                 nRawXsize, tilenum );
        return;
    }
    if (nRawYsize <=0) {
        CPLDebug("Viewranger",
                 "nRawYsize %d zero or negative in tilenum %d",
                 nRawYsize, tilenum );
        return;
    }

    if ( nRawXcount > nBlockXSize
         || nRawXsize > nBlockXSize
         || nRawXcount * nRawXsize > nBlockXSize
         ) {
        CPLDebug("Viewranger",
                 "nRawXcount %d x nRawXsize %d too big > nBlockXSize %d\tx%08x x x%08x > x%08x",
                 nRawXcount, nRawXsize, nBlockXSize,
                 nRawXcount, nRawXsize, nBlockXSize );
        // return;
    }
    if ( nRawYcount > nBlockYSize
         || nRawYsize > nBlockYSize
         || nRawYcount * nRawYsize > nBlockYSize
         ) {
        CPLDebug("Viewranger",
                 "nRawYcount %d x nRawYsize %d too big > nBlockYSize %d\tx%08x x x%08x > x%08x",
                 nRawYcount, nRawYsize, nBlockYSize,
                 nRawYcount, nRawYsize, nBlockYSize );
        // return;
    }

    CPLDebug("Viewranger",
             "nRawXcount %d nRawYcount %d nRawXsize %d nRawYsize %d",
             nRawXcount, nRawYcount, nRawXsize, nRawYsize);

    // Allow for under-height tiles
    {
        int nSkipTopRows = nBlockYSize - nRawYcount * nRawYsize;
        if (nSkipTopRows > 0) {
            CPLDebug("Viewranger",
                     "underheight tile nRawYcount %d x nRawYsize %d < blocksize %d",
                     nRawYcount, nRawYsize, nBlockYSize );
            //   This is a short (underheight) tile.
            // GDAL expects these at the top of the bottom tile,
            // but VRC puts these at the bottom of the top tile.
            //   We need to add a blank strip at the top of the
            // tile to compensate.

            for (int nPix=0; nPix<nSkipTopRows*nBlockXSize; nPix++) {
                static_cast<char*>(pImage)[nPix] = nVRCNoData;
            }
        } else if (nSkipTopRows != 0) {
            // This should not happen
            CPLDebug("Viewranger",
                     "OVERheight tile nRawYcount %d x nRawYsize %d > blocksize %d)",
                     nRawYcount, nRawYsize, nBlockYSize );
        }
    }  // Finished allowing for under-height tiles

    CPLDebug("Viewranger",
             "nRawXcount %d nRawYcount %d nRawXsize %d nRawYsize %d",
             nRawXcount, nRawYcount, nRawXsize, nRawYsize);

    // Read in this tile's index to ?raw? sub-tiles.
    std::vector<unsigned int> anSubTileIndex;
    anSubTileIndex.reserve(static_cast<size_t>(nRawXcount*nRawYcount +1));
    for (size_t loop=0;
         loop <= static_cast<size_t>(nRawXcount*nRawYcount);
         loop++) {
        anSubTileIndex[loop] = VRReadUInt(fp);
        if (anSubTileIndex[loop] >= poGDS->oStatBufL.st_size)
            {
                CPLDebug("Viewranger",
                         "Band %d block [%d,%d] raw image %zd at x%x is beyond EOF - is file truncated ?",
                         nBand, block_xx, block_yy, loop, anSubTileIndex[loop] );
                anSubTileIndex[loop] = 0;
            }
    }
    

    for (int loopX=0; loopX < nRawXcount; loopX++) {
        for (int loopY=0; loopY < nRawYcount; loopY++) {
            auto loop = static_cast<size_t>
                (nRawYcount-1-loopY + loopX*nRawYcount);

            //((GUInt32 *) pImage)[i] = poGDS->anSubTileIndex[loop];

            unsigned long nStart = anSubTileIndex[loop];
            unsigned long nFinish= anSubTileIndex[loop+1];
            unsigned long nFileSize = static_cast<unsigned int>
                (poGDS->oStatBufL.st_size);
            CPLString osBaseLabel
                = CPLString().Printf("/tmp/werdna/vrc2tif/%s.%03d.%03d.%08lu.%02u",
                                     // CPLGetBasename(poOpenInfo->pszFilename) doesn't quite work
                                     poGDS->sLongTitle.c_str(),
                                     loopX, loopX, nStart,
                                     nBand);

            if (/* 0<=nStart && */ nStart<=nFinish && nFinish <= nFileSize) {
                size_t nRawSubtileSize = static_cast<size_t>
                    (nRawXsize*nRawYsize);
                if (nRawSubtileSize>nFinish-nStart) {
                    nRawSubtileSize = nFinish-nStart;
                }
                GByte *abySubTileData = static_cast<GByte *>(VSIMalloc(nRawSubtileSize));

                int seekres = VSIFSeekL( fp, nStart, SEEK_SET );
                if ( seekres ) {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "cannot seek to x%lx", nStart);
                    return;
                }
                size_t bytesread = VSIFReadL(abySubTileData, sizeof(GByte), nRawSubtileSize, fp);
                if (bytesread < nRawSubtileSize) {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "problem reading bytes [x%lx, x%lx)\n",
                             nStart,nFinish);
                    return;  
                }
            
                int nVerifyResult=verifySubTileMem(abySubTileData,
                                                   nStart, nFinish,
                                                   block_xx, block_yy,
                                                   loopX, loopY
                                                   );
                if (0==(nVerifyResult & ~0xff)) {
                    CPLDebug("Viewranger",
                             "raw data at x%08lx for tile (%d,%d) sub tile (%d,%d) did not verify\n",
                             nStart, block_xx, block_yy, loopX, loopY);
                }
                if ( VSIFSeekL( fp, nStart, SEEK_SET ) ) {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "cannot seek to start of tile (%d,%d) sub tile (%d,%d)",
                              block_xx, block_yy, loopX, loopY);
                    return;
                }

#if 0
                {
                    unsigned int nf0count=0;
                    unsigned int n47count=0;
                    for (unsigned int i=0; i < nRawSubtileSize ; i++) {
                        if (abySubTileData[i] == 0xf0) {
                            nf0count++;
                        }
                        if (abySubTileData[i] == 0x47) {
                            nf0count++;
                        }
                    }
                    CPLDebug("Viewranger",
                             "tile (%d,%d) sub tile (%d,%d) contains 0xf0 %d times",
                             block_xx, block_yy, loopX, loopY, nf0count);
                    CPLDebug("Viewranger",
                             "tile (%d,%d) sub tile (%d,%d) contains 0x47 %d times",
                             block_xx, block_yy, loopX, loopY, n47count);
                }
#endif

                // Allow for under-height tiles
                int nSkipTopRows = nBlockYSize - nRawYcount * nRawYsize;
                if (nSkipTopRows > 0) {
                    CPLDebug("Viewranger",
                             "underheight tile nRawYcount %d x nRawYsize %d < blocksize %d)",
                             nRawYcount, nRawYsize, nBlockYSize );
                    //   This is a short (underheight) tile.
                    // GDAL expects these at the top of the bottom tile,
                    // but VRC puts these at the bottom of the top tile.
                    //   We need to add a blank strip at the top of the
                    // tile to compensate.
                    
                    for (int nPix=0; nPix<nSkipTopRows*nBlockXSize; nPix++) {
                        // This probably blanks all the subtiles in the top row,
                        // not just subtile loopX
                        (static_cast<char*>(pImage))[nPix] = nVRCNoData;
                    }
                } else if (nSkipTopRows != 0) {
                    // This should not happen
                    CPLDebug("Viewranger",
                             "OVERheight tile nRawYcount %d x nRawYsize %d > blocksize %d)",
                             nRawYcount, nRawYsize, nBlockYSize );
                    nSkipTopRows = 0;
                }
                
                // Write the raw data into the subtile of the image,
                // padding with the result of verifySubTileMem/File.
                unsigned int nCount= static_cast<unsigned int>(nStart);
                for (int j=0; j < nRawYsize ; j++) {
                    int pixelnum = (j+loopY*nRawYsize+nSkipTopRows) * nBlockXSize
                        + loopX*nRawXsize;
                    for (int i=0; i < nRawXsize ; i++) {
                        if (pixelnum >= nBlockXSize*nBlockYSize) {
                            CPLDebug("Viewranger",
                                     "pixelnum %d > %x x %d - tile(%x,%d) loop(%x,%d) i=%d j=%d nCount=%d\n",
                                     pixelnum, nBlockXSize, nBlockYSize,
                                     block_xx, block_yy, loopX, loopY,
                                     i, j, nCount);
                            break;
                        }
                        if (nCount<nFinish) {
                            static_cast<GByte*>(pImage)[pixelnum] =
                                static_cast<GByte>(VRReadChar(fp));
                        } else {
                            static_cast<GByte*>(pImage)[pixelnum] =
                                static_cast<GByte>(nVerifyResult);
                            // static_cast<GByte*>(pImage)[pixelnum] =nf0count;
                        }
                        nCount++;
                        pixelnum++;
                    }
                }
                if(abySubTileData) {
                    VSIFree(abySubTileData);
                }            
            } else {
                CPLDebug("Viewranger",
                         "skipping %s: expected 0 <= x%lx <= x%lx <= x%lx filesize",
                         osBaseLabel.c_str(),
                         nStart, nFinish, nFileSize
                         );
            } // end range check 
        } // for loopY
    } // for loopX
#endif
#endif

    if (getenv("VRC_DUMP_TILE") && 1==nBand) {
        long nDumpCount = strtol(getenv("VRC_DUMP_TILE"),nullptr,10);
        // Dump first band of VRC tile as a (monochrome) .pgm.
        // The bands are currently all the same.
        CPLString osBaseLabel
            = CPLString().Printf("/tmp/werdna/vrc2tif/%s.%03d.%03d.%02u",
                                 // CPLGetBasename(poOpenInfo->pszFilename) doesn't quite work
                                 poGDS->sLongTitle.c_str(),
                                 block_xx, block_yy,
                                 nBand);
        
        dumpPPM(
                static_cast<unsigned int>(nBlockXSize),
                static_cast<unsigned int>(nBlockYSize),
                static_cast<unsigned char*>(pImage),
                static_cast<unsigned int>(nBlockXSize),
                osBaseLabel,
                band,
                static_cast<unsigned int>(nDumpCount)
                );
    }

} // VRCRasterBand::read_VRC_Tile_ThirtySix


int VRCRasterBand::verifySubTileFile(
        VSILFILE *fp,
        unsigned long start,
        unsigned long finish,
        int nGDtile_xx,
        int nGDtile_yy,
        unsigned int nVRtile_xx,
        unsigned int nVRtile_yy
)
{
    CPLString osBaseLabel;
    osBaseLabel.Printf("/tmp/werdna/vrc2tif/%s.%03d.%03d.%03d.%03d.%08lu.%02u",
                       // CPLGetBasename(poOpenInfo->pszFilename) doesn't quite work
                       static_cast<VRCDataset *>(poDS)->sLongTitle.c_str(),
                       nGDtile_xx, nGDtile_yy,
                       nVRtile_xx, nVRtile_yy,
                       start, nBand);

    if (start>finish) {
        CPLDebug("Viewranger", "Backwards sub-tile: %lu>%lu bytes at %s",
                 start, finish, osBaseLabel.c_str());
        return -1;
    }

    int seekres = VSIFSeekL( fp, start, SEEK_SET );
    if ( seekres ) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "cannot seek to x%lx", start);
        return -1;
    }

    auto nLen = static_cast<unsigned int>(finish-start);
    std::vector<GByte> abyRawSubtileData;
    abyRawSubtileData.reserve(nLen);
    size_t bytesread = VSIFReadL(abyRawSubtileData.data(), sizeof(GByte), nLen, fp);
    if (bytesread < nLen) {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "problem reading bytes [x%lx, x%lx)\n",
                 start,finish);
        return -1;  
    }

    return verifySubTileMem(abyRawSubtileData.data(),
                          start, finish,
                          nGDtile_xx, nGDtile_yy,
                          nVRtile_xx, nVRtile_yy );
} // VRCRasterBand::verifySubTileFile()

int VRCRasterBand::verifySubTileMem(
        GByte abyRawStartData[],
        unsigned long start,
        unsigned long finish,
        int nGDtile_xx,
        int nGDtile_yy,
        unsigned int nVRtile_xx,
        unsigned int nVRtile_yy
)
{
    CPLString osBaseLabel;
    osBaseLabel.Printf("/tmp/werdna/vrc2tif/%s.%03d.%03d.%03d.%03d.%08lu.%02u",
                       // CPLGetBasename(poOpenInfo->pszFilename) doesn't quite work
                       static_cast<VRCDataset *>(poDS)->sLongTitle.c_str(),
                       nGDtile_xx, nGDtile_yy,
                       nVRtile_xx, nVRtile_yy,
                       start, nBand);

    const unsigned char kacExpectedValues[144] = 
        {
         0x00, 0xbe, 0xe9, 0x42,        0x77, 0x64, 0x30, 0x21,
         0x3d, 0x5c, 0x2e, 0x34,        0x77, 0x46, 0x5a, 0x59,
         0x79, 0x24, 0x4b, 0x4b,        0x4e, 0x51, 0x38, 0x48,
         0x3d, 0x6d, 0x3c, 0x31,        0x36, 0x55, 0x27, 0x20,
                     
         0x66, 0x54, 0x47, 0x47,        0x69, 0x37, 0x5b, 0x55,
         0x5e, 0x5c, 0x17, 0x5d,        0x2e, 0x7f, 0x15, 0x39,
         0x2e, 0x4c, 0x0b, 0x1c,        0x51, 0x63, 0x79, 0x78,
         0x57, 0x09, 0x64, 0x5a,        0x5b, 0x6c, 0x02, 0x6f,
                     
         0x1c, 0x54, 0x13, 0x0d,        0x11, 0x72, 0xd4, 0xeb,
         0x71, 0x03, 0x5e, 0x58,        0x79, 0x24, 0x47,
         // Some USA sub-tiles only match up to here.
                                                          0x4b,
         // 80=x50 bytes
         0x4e, 0x52, 0x38, 0x48,        0x27, 0x4c, 0x2c, 0x33,
         0x22,
         // These 20 bytes ...
               0x72, 0x03, 0x18,        0x59, 0x68, 0x77, 0x77,
         0x56, 0x0b, 0x65, 0x6b,        0x6c, 0x69, 0x1a, 0x6a,
         0x1c, 0x4c, 0x1e, 0x0d,        0x10,
         // .. repeat ...
               0x72, 0x03, 0x18,        0x59, 0x68, 0x77, 0x77,
         0x56, 0x0b, 0x65, 0x6b,        0x6c, 0x69, 0x1a, 0x6a,
         0x1c, 0x4c, 0x1e, 0x0d,        0x10,
         // ... and 10 bytes again
               0x72, 0x03, 0x18,        0x59, 0x68, 0x77, 0x77,
         0x56, 0x0b, 0x65,
         //
                           0xbc,        0x84, 0x41, 0x23, 0x4a
        };
#if 0
    const unsigned char kacExpectedValues2[73] =
        { /*                                           */ 0x4B,
         0x4E, 0x52, 0x38, 0x48,        0x27, 0x4C, 0x2C, 0x33,
         0x22, 0x72, 0x59, 0x68,        0x77, 0x77, 0x56, 0x65,
         0x6B, 0x6C, 0x69,
        };
#endif

    if (start>finish) {
        CPLDebug("Viewranger", "Backwards sub-tile: %lu>%lu bytes at %s",
                 start, finish, osBaseLabel.c_str());
        return -1;
    }

    auto nLen = static_cast<unsigned int>(finish-start);
    unsigned int nHeadLen = 144;
    if (nLen < nHeadLen) {
        CPLDebug("Viewranger", "Short sub-tile: %u<144 bytes at x%lx %s",
                 nLen, start, osBaseLabel.c_str());       
        if (nLen>0) {
            nHeadLen = nLen;
        } else {
            nHeadLen=0;
        }
    }

    if (abyRawStartData==nullptr) {
        CPLDebug("Viewranger", "SubTile %s [%lu>%lu) has null ptr",
                 osBaseLabel.c_str(), start, finish
                 );
        return -1;
    }

    size_t nBytesMatched=bytesmatch(abyRawStartData, kacExpectedValues, nHeadLen);
    if (nBytesMatched==144) {
        CPLDebug("Viewranger", "Found all of expected 144-byte header at x%lx %s",
                 start, osBaseLabel.c_str());
    } else {
        CPLDebug("Viewranger", "Found %lu bytes of expected 144-byte header at x%lx %s",
                 nBytesMatched, start, osBaseLabel.c_str());
    }
    for (size_t i=nBytesMatched; i<nHeadLen; i++) {
        CPLDebug("Viewranger", "then [%lu] = x%02x",
                 i, static_cast<unsigned char>(abyRawStartData[i]));
    }

#if 0
    {
        // Script to write a "plain" portable graymap ...
        CPLDebug("Viewranger",
                 "echo \"P2 1 1 65535\n%lu\" > \"%s.pgm\"",
                 nBytesMatched, osBaseLabel.c_str() );
        // ... and a script to write the matching world map:
        // These values are a work in progress
        // and are not yet expected to be correct.
        double dx = static_cast<VRCDataset *>(poDS)->nLeft
            + nBlockXSize*nGDtile_xx + nVRtile_xx;
        double dy = static_cast<VRCDataset *>(poDS)->nBottom
            - nBlockYSize*nGDtile_yy + nVRtile_yy;
        CPLDebug("Viewranger",
                 "echo \"%g\n%g\n%g\n%g\n%g\n%g\n\" > \"%s.wld\"",
                 1.0*static_cast<VRCDataset *>(poDS)->tileXcount, 0.0,
                 0.0, -1.0*static_cast<VRCDataset *>(poDS)->tileYcount,
                 dx, dy,
                 osBaseLabel.c_str() );
    }
#endif

    return 0x0100 | static_cast<int>(nBytesMatched);
} // VRCRasterBand::verifySubTileMem()

// #endif // def FRMT_vrc
