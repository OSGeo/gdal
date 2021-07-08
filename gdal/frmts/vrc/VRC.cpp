/******************************************************************************
 * $Id: VRC.cpp,v 1.281 2021/07/08 11:52:49 werdna Exp $
 *
 * Project:  GDAL
 * Purpose:  Viewranger GDAL Driver
 * Authors:  Andrew C Aitchison
 *
 ******************************************************************************
 * Copyright (c) 2015-21, Andrew C Aitchison
 ****************************************************************************
 * Portions taken from gdal-2.2.3/frmts/png/pngdataset.cpp and other GDAL code
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at mines-paris dot org>
 ****************************************************************************/

/* -*- tab-width: 4 ; indent-tabs-mode: nil ; c-basic-offset 'tab-width -*- */

// #ifdef FRMT_vrc

#include "VRC.h"
#include "png_crc.h"  // for crc pngcrc_for_VRC, used in:
                    //   PNGCRCcheck

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winfinite-recursion"
//#pragma clang diagnostic ignored "-Wunneeded-internal-declaration"
CPL_CVSID("$Id: VRC.cpp,v 1.281 2021/07/08 11:52:49 werdna Exp $")
#pragma clang diagnostic push

CPL_C_START
void CPL_DLL GDALRegister_VRC(void);
CPL_C_END

void VRC_file_strerror_r(int nFileErr, char* const buf, size_t buflen)
{
    if (buf==nullptr || buflen<1) {
        return;
    }
#if 0
#define STRERR_DEBUG(...) CPLDebug(...)
#else
#define STRERR_DEBUG(...)
#endif

#if defined(WIN32)
    STRERR_DEBUG("Viewranger",
                 "Windows file error %d",
                 nFileErr);
    snprintf(buf, buflen, "Windows file error %d",
                 nFileErr);
#else
#if (_POSIX_C_SOURCE >= 200112L) && ! _GNU_SOURCE
    /* strerror_r is XSI-compliant */
    if (int err=strerror_r(nFileErr, buf, buflen)) {
        int errnum=errno;
        // error getting error for nFileErr
        CPLError( CE_Failure, CPLE_AppDefined,
                  "nested errors - %d=x%x  %d=x%x %d=x%x ?\n\tbuf %p buflen %ld",
                  nFileErr,nFileErr, err,err, errnum,errnum, buf, buflen );
    } else {
        STRERR_DEBUG("Viewranger",
                 "XSI strerror_r(%d,...) returns null and sets buf to %s",
                 nFileErr, buf);
    }
#else
    /* GNU-specific strerror_r */
    char *ret=strerror_r(nFileErr, buf, buflen);
    if (ret) {
        STRERR_DEBUG("Viewranger",
                 "GNU strerror_r(%d,...) returns %s and sets buf to %s",
                  nFileErr, ret, buf);
        if (buf && buf!=ret) {
            strncpy(buf,ret,buflen);
            buf[buflen]=0;
        }
    } else {
        STRERR_DEBUG("Viewranger",
                 "GNU strerror_r(%d,...) returns null and sets buf to %s",
                 nFileErr, buf);
    }
#endif
#endif // !defined(WIN32)
#undef STRERR_DEBUG
} // VRC_file_strerror_r()


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
typedef struct __attribute__((aligned(32))) {
    png_bytep pData;
    signed long length;
    signed long current;
} VRCpng_data;
#pragma clang diagnostic pop

static unsigned int PNGGetUInt( void* base, unsigned int byteOffset )
{
    unsigned char * buf = static_cast<unsigned char*>(base)+byteOffset;
    unsigned int vv = buf[3];
    vv |= static_cast<unsigned int>(buf[2]) << 8;
    vv |= static_cast<unsigned int>(buf[1]) << 16;
    vv |= static_cast<unsigned int>(buf[0]) << 24;

    return(vv);
}
static signed int PNGGetInt( void* base, unsigned int byteOffset )
{
    return(static_cast<signed int>(PNGGetUInt(base, byteOffset)));
}


static
unsigned int PNGReadUInt(VSILFILE *fp)
{
    unsigned char buf[4]={};
    // int ret =
    VSIFReadL(buf, 1, 4, fp);
    return PNGGetUInt(buf, 0);
}
#if 0 // Not used
static
signed int PNGReadInt(VSILFILE *fp)
{
    return(static_cast<signed int>(PNGReadInt(fp)));
}
#endif

static
void PNGAPI
VRC_png_read_data_fn (png_structp png_read_ptr,
                      png_bytep data,
                      png_size_t length)
{
    if (png_read_ptr == nullptr) {
        CPLDebug("Viewranger PNG",
                 "VRC_png_read_data_fn given null io ptr");
        //png_warning(png_read_ptr,
        //            "VRC_png_read_data_fn given null io ptr\n");
        return;
    }
    if (data == nullptr) {
        CPLDebug("Viewranger PNG",
                 "VRC_png_read_data_fn cannot write to null ptr");
        //png_warning(png_read_ptr,
        //            "VRC_png_read_data_fn cannot write to null ptr\n");
        return;
    }
    if (length <1) {
        CPLDebug("Viewranger PNG",
                 "VRC_png_read_data_fn() requested length %ld < 1",
                 static_cast<long>(length)
                 );
        return;
    }

    auto *pVRCpng_data =
        static_cast<VRCpng_data*>(png_get_io_ptr(png_read_ptr));
#if 0
    CPLDebug("Viewranger PNG",
             "VRC_png_read_data_fn(%p %p %ld) %p %s %p",
             png_read_ptr, data, static_cast<long>(length),
             pVRCpng_data,
             pVRCpng_data == png_read_ptr->io_ptr ? "==" : "!=",
             png_read_ptr->io_ptr
             );
    CPLDebug("Viewranger PNG",
            "pVRCpng_data %p cur=%ld len=%ld",
             pVRCpng_data->pData,
             pVRCpng_data->current,
             pVRCpng_data->length
             );
#endif

    // Sanity checks on our data pointer
    if (pVRCpng_data->pData==nullptr) {
        CPLDebug("Viewranger PNG",
                 "VRC_png_read_data_fn(%p %p %ld) pVRCpng_data is null",
                 png_read_ptr, data, length
                 );
        return;
    }
    if ( pVRCpng_data->current < 0) {
        CPLDebug("Viewranger PNG",
                 "VRC_png_read_data_fn() current %ld < 0",
                 pVRCpng_data->current
                 );
        return;
    }
    long nSpare =  pVRCpng_data->length - pVRCpng_data->current;
    if ( nSpare<0) {
        CPLDebug("Viewranger PNG",
                 "VRC_png_read_data_fn() current %ld > length %ld - diff %ld",
                 pVRCpng_data->current, pVRCpng_data->length,
                 -nSpare
                 );
        return;
    }
    // Sanity check the function args
    if ( static_cast<long>(length) > nSpare ) {
        CPLDebug("Viewranger PNG",
                 "VRC_png_read_data_fn(%p %p %ld) %ld+%ld reads beyond %ld",
                 png_read_ptr, data, length,
                 pVRCpng_data->current, length, pVRCpng_data->length
                 );
        // Copy the data we have ...
        if (nSpare>0) { // pVRCpng_data->current < pVRCpng_data->length) {
            memcpy(data,
                   pVRCpng_data->pData + pVRCpng_data->current,
                   static_cast<size_t>(nSpare));
        } else {
            CPLDebug("Viewranger PNG",
                     "VRC_png_read_data_fn(%p %p %ld) buffer already over full: current %ld >= space %ld",
                     png_read_ptr, data, length,
                     pVRCpng_data->current, pVRCpng_data->length
                     );
        }
        pVRCpng_data->current = pVRCpng_data->length;
        return;
    }

    memcpy(data,
           pVRCpng_data->pData + pVRCpng_data->current,
           length);
    pVRCpng_data->current += static_cast<long>(length);

}

static
int PNGCRCcheck(VRCpng_data *oVRCpng_data, unsigned long nGiven)
{
    if (oVRCpng_data->current < 8) {
        CPLDebug("Viewranger PNG", "PNGCRCcheck: current %lu < 8",
                 oVRCpng_data->current);
        return -1;
    }
    unsigned char *pBuf = &oVRCpng_data->pData[oVRCpng_data->current-4];
    auto nLen = static_cast<unsigned>
        (PNGGetInt(&oVRCpng_data->pData[oVRCpng_data->current-8], 0));

    if (nLen > static_cast<unsigned long int> (oVRCpng_data->length)
        || nLen > static_cast<unsigned long int> (1LLU<<31U)
        ) {
        // from PNG spec nLen <= 2^31
        CPLDebug("Viewranger PNG", "PNGCRCcheck: nLen %u > buffer length %lu",
                 nLen, oVRCpng_data->length);
        return -1;
    } else {
        CPLDebug("Viewranger PNG", "PNGCRCcheck((%p, %lu) %u, x%08lx)",
                 pBuf, oVRCpng_data->current, nLen, nGiven);
    }

    unsigned long nFileCRC = 0xffffffff & static_cast<unsigned long>
        ( PNGGetInt(oVRCpng_data->pData,
                    (static_cast<unsigned int>(oVRCpng_data->current)
                     + nLen)) );
    if (nGiven == nFileCRC) {
        CPLDebug("Viewranger PNG",
                 "PNGCRCcheck(x%08lx) given CRC matches CRC from file",
                 nFileCRC);
    } else {
        CPLDebug("Viewranger PNG",
                 "PNGCRCcheck(x%08lx) CRC given does not match x%08lx from file",
                 nGiven, nFileCRC);
        return -1;
    }

    unsigned long nComputed = pngcrc_for_VRC(pBuf, nLen+4);
    nComputed &= 0xffffffff;
    int ret = (nGiven==nComputed);
    if (ret==0) {
        CPLDebug("Viewranger PNG",
                 "PNG file: CRC given x%08lx, calculated x%08lx",
                 nGiven, nComputed);
    }

    return ret;
}


/* -------------------------------------------------------------------------
 * Returns a (null-terminated) string allocated from VSIMalloc.
 * The 32 bit length of the string is stored in file fp at byteaddr.
 * The string itself is stored immediately after its length;
 * it is *not* null-terminated in the file.
 * If index pointer is nul then an empty string is returned
 * (rather than a null pointer).
 */
char *VRCDataset::VRCGetString( VSILFILE *fp, unsigned int byteaddr )
{
    if (byteaddr==0) return( strdup (""));

    int seekres = VSIFSeekL( fp, byteaddr, SEEK_SET );
    if ( seekres ) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "cannot seek to VRC string" );
        return nullptr;
    }
    int string_length = VRReadInt(fp);
    if (string_length<=0) {
        if (string_length<0) {
            CPLDebug("Viewranger", "odd length for string %08x - length %d",
                     byteaddr, string_length);
        }
        return( strdup (""));
    }
    size_t ustring_length = static_cast<unsigned>(string_length);

    char *pszNewString = static_cast<char*>(VSIMalloc(1+ustring_length));
    if (pszNewString == nullptr) {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Cannot allocate %d bytes for string",
                 1+string_length);
        return nullptr;
    }


    size_t bytesread =
      VSIFReadL( pszNewString, 1, ustring_length, fp);

    if (bytesread < ustring_length) {
      VSIFree(pszNewString);
      CPLDebug("Viewranger", "requested x%08x bytes but only got x%8lx",
               string_length, bytesread);
      CPLError(CE_Failure, CPLE_AppDefined,
               "problem reading string\n");
      return nullptr;
    }

    pszNewString[string_length] = 0;
    // CPLDebug("Viewranger", "read string %s at %08x - length %d",
    //         pszNewString, byteaddr, string_length);
    return pszNewString;
} // VRCDataset::VRCGetString( VSILFILE *fp, unsigned int byteaddr )



/************************************************************************/
/*                           VRCRasterBand()                            */
/************************************************************************/

VRCRasterBand::VRCRasterBand(
                             VRCDataset *poDSIn,
                             int nBandIn,
                             int nThisOverviewIn,
                             int nOverviewCountIn,
                             VRCRasterBand**papoOverviewBandsIn
                             )
    :
    // nRecordSize(0),
    eBandInterp(GCI_Undefined),
    nThisOverview(nThisOverviewIn),
    nOverviewCount(nOverviewCountIn),
    papoOverviewBands(papoOverviewBandsIn)
{
    VRCDataset *poVRCDS = poDSIn;
    poDS = static_cast<GDALDataset *>(poVRCDS);
    nBand = nBandIn;
    CPLDebug("Viewranger", "%s %p->VRCRasterBand(%p, %d, %d, %d, %p)",
             poVRCDS->sLongTitle.c_str(), this,
             poVRCDS, nBand, nThisOverview, nOverviewCount, papoOverviewBands);

    if (nOverviewCount >=32) {
        // This is unnecessarily big;
        // the scale factor will not fit in an int, and
        // a 1cm / pixel map of the world will have a one pixel overview.
        // nOverviewCount=32;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%d overviews is not practical",
                 nOverviewCount);
        nOverviewCount=0;
        return;  // Can I return failure ?
    }
    if (nOverviewCount>=0 && nThisOverview >= nOverviewCount) {
        if (nOverviewCount>0) {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "failed: cannot set overview %d of %d\n",
                     nThisOverview, nOverviewCount);
        }
        return; // CE_Failure; // nullptr; ?
    }

    auto nOverviewScale =
        static_cast<signed int>(1U << static_cast<unsigned int>(nThisOverview+1));
    nRasterXSize = poVRCDS->nRasterXSize / nOverviewScale; // >> nOverviewShift;
    nRasterYSize = poVRCDS->nRasterYSize / nOverviewScale; // >> nOverviewShift;

    // int tileXcount = poVRCDS->tileXcount;
    // int tileYcount = poVRCDS->tileYcount;

    CPLDebug("Viewranger", "nRasterXSize %d nRasterYSize %d",
             nRasterXSize, nRasterYSize);


    // Image Structure Metadata:  INTERLEAVE=PIXEL would be good
    SetMetadataItem( "INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE" );
    
    if (poVRCDS->nMagic == vrc_magic_metres
        // || poVRDSIn->nMagic == vrc_magic_thirtysix // temp: merging metre and thirtysix tile code
        ) {
        
        eDataType = GDT_Byte; // GDT_UInt32;
        // GCI_Undefined; // GCI_GrayIndex; // GCI_PaletteIndex;
        // GCI_RedBand; // GCI_GreenBand;  // GCI_BlueBand; //GCI_AlphaBand;
        switch (nBand) {
        case 1:
            eBandInterp = GCI_RedBand;
            CPLDebug("Viewranger", "vrcmetres_pixel_is_pixel Red band");
            break;
        case 2:
            eBandInterp = GCI_GreenBand;
            CPLDebug("Viewranger", "vrcmetres_pixel_is_pixel Green band");
            break;
        case 3:
            eBandInterp = GCI_BlueBand;
            CPLDebug("Viewranger", "vrcmetres_pixel_is_pixel Blue band");
            break;
        case 4:
          eBandInterp = GCI_AlphaBand;
          CPLDebug("Viewranger", "vrcmetres_pixel_is_pixel Alpha band");
          break;
        default:
            CPLDebug("Viewranger", "vrcmetres_pixel_is_pixel band %d unexpected !",
                     nBand);
        }

        // These will change with nThisOverview

        CPLDebug("Viewranger",
                 "vrcmetres_pixel_is_pixel nThisOverview=%d",
                 nThisOverview);
        if (nThisOverview<-1) {
            CPLDebug("Viewranger",
                     "\toverview %d invalid",
                     nThisOverview);
            nThisOverview=-1; //  main view
        } else if (nThisOverview>7) {
            CPLDebug("Viewranger",
                     "\toverview %d unexpected",
                     nThisOverview);
        }

#if defined ROUND_UP_OVERVIEWSIZE
        {
            unsigned int rounding =
                (1U<<static_cast<unsigned int>(1+nThisOverview)) -1;
            nBlockXSize =
                (static_cast<signed int>(rounding + poVRCDS->tileSizeMax))
                / nOverviewScale;
            CPLDebug("Viewranger",
                     "overview %d tileSizeMax %d nBlockXSize %d rounding %d",
                     nThisOverview, poVRCDS->tileSizeMax, nBlockXSize, rounding
                     );
        }
#else
        nBlockXSize =
            static_cast<signed int>(poVRCDS->tileSizeMax) / nOverviewScale;
             // was static_cast<int>(poVRCDS->tileSizeMax >> (1+nThisOverview));
#endif // ROUND_UP_OVERVIEWSIZE
        nBlockYSize = nBlockXSize;
        if (nBlockXSize<1) {
             CPLDebug("Viewranger",
                      "overview %d block %d x %d too small",
                      nThisOverview, nBlockXSize, nBlockYSize
                 );
             nBlockYSize=nBlockXSize=1;
        }
        CPLDebug("Viewranger",
                 "overview %d block %d x %d",
                 nThisOverview, nBlockXSize, nBlockYSize
                 );

#if 0
        nRecordSize = static_cast<GUIntBig>(nBlockXSize)
            * static_cast<GUIntBig>(nBlockYSize)
            * sizeof(GByte); // sizeof(GUInt32);
#endif
    } else if (poVRCDS->nMagic == vrc_magic_thirtysix) {
#if defined VRC36_PIXEL_IS_FILE
        CPLDebug("Viewranger", "vrcthirtysix_pixel_is_file");
        // eDataType = GDT_Byte;
        // eBandInterp = GCI_GrayIndex; // GCI_PaletteIndex;
        // at this stage data values are probably a pointers
        // we want to view them to spot duplicates etc.
        eDataType = GDT_UInt32;
        eBandInterp = GCI_Undefined; // GCI_GrayIndex; // GCI_PaletteIndex;

        /* We cannot yet interpret the map data.
         * For now we pretend it is a single pixel.
         */

        nBlockXSize = 1;
        nBlockYSize = 1;

        // nRecordSize = sizeof(GUInt32);
#else
#if defined VRC36_PIXEL_IS_TILE
        CPLDebug("Viewranger", "vrcthirtysix_pixel_is_tile");
        // at this stage data is probably a pointer
        eDataType = GDT_UInt32;
        eBandInterp = GCI_Undefined; // GCI_GrayIndex; // GCI_PaletteIndex;

        nBlockXSize = poVRCDS->tileXcount;
        nBlockYSize = poVRCDS->tileYcount;
        // nRecordSize = static_cast<GUIntBig>(nBlockXSize) * static_cast<GUIntBig>(nBlockYSize) * sizeof(GUInt32);
#else
        CPLDebug("Viewranger", "vrcthirtysix_pixel_is_pixel");

        // VRC36_PIXEL_IS_PIXEL
        // this will be the default
        CPLDebug("Viewranger", "vrcthirtysix_pixel_is_pixel not yet tested");
        eDataType = GDT_Byte; // GDT_UInt32;
        eBandInterp =  GCI_PaletteIndex; // GCI_GrayIndex; // GCI_Undefined; // GCI_GrayIndex; //

#if defined ROUND_UP_OVERVIEWSIZE
        {
            int rounding = (1<<(1+nThisOverview)) -1;
            nBlockXSize = (rounding + poVRCDS->tileSizeMax) >> (1+nThisOverview);
            CPLDebug("Viewranger",
                     "overview %d tileSizeMax %d nBlockXSize %d rounding %d",
                     nThisOverview, poVRCDS->tileSizeMax, nBlockXSize, rounding
                     );
        }
#else
        nBlockXSize = static_cast<int>(poVRCDS->tileSizeMax >> (1+nThisOverview));
#endif // ROUND_UP_OVERVIEWSIZE
        nBlockYSize = nBlockXSize;
        if (nBlockXSize<1) {
             CPLDebug("Viewranger",
                      "overview %d block %d x %d too small",
                      nThisOverview, nBlockXSize, nBlockYSize
                 );
             nBlockYSize=nBlockXSize=1;
        }
        //nBlockXSize = 1;
        //nBlockYSize = 1;
#if 0 // nRecordSize
        nRecordSize = static_cast<GUIntBig>(nBlockXSize)
            * static_cast<GUIntBig>(nBlockYSize)
            * sizeof(GByte);
#endif // nRecordSize
#endif // not defined VRC36_PIXEL_IS_TILE - so VRC36_PIXEL_IS_PIXEL
#endif // not defined VRC36_PIXEL_IS_FILE
    } // else if (poVRCDS->nMagic == vrc_magic_thirtysix) {
    
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-enum"
    switch (eBandInterp) {
    case GCI_GrayIndex:
        CPLDebug("Viewranger", "eBandInterp Greyscale (x%08x)",
             eBandInterp);
        break;
    case GCI_PaletteIndex:
        CPLDebug("Viewranger", "eBandInterp Paletted (x%08x)",
             eBandInterp);
        break;
    case GCI_RedBand:
        CPLDebug("Viewranger", "eBandInterp Red (x%08x)",
             eBandInterp);
        break;
    case GCI_GreenBand:
        CPLDebug("Viewranger", "eBandInterp Green (x%08x)",
             eBandInterp);
        break;
    case GCI_BlueBand:
        CPLDebug("Viewranger", "eBandInterp Blue (x%08x)",
             eBandInterp);
        break;
    case GCI_AlphaBand:
        CPLDebug("Viewranger", "eBandInterp Alpha (x%08x)",
             eBandInterp);
        break;
    default:
        CPLDebug("Viewranger", "eBandInterp x%08x, (Red==x%08x)",
                 eBandInterp, GCI_RedBand);
        break;
    }
#pragma clang diagnostic pop
    SetColorInterpretation(eBandInterp);

/* -------------------------------------------------------------------- */
/*      If this is the base layer, create the overview layers.          */
/* -------------------------------------------------------------------- */

#if defined VRC36_PIXEL_IS_FILE || defined VRC36_PIXEL_IS_TILE
    if (poVRCDS->nMagic == vrc_magic_thirtysix) {
        nOverviewCount=0;
        return ;
    }
#endif // VRC36_PIXEL_IS_ not _PIXEL

    if( nOverviewCount>=0 && nThisOverview == -1 ) {
        if (papoOverviewBands != nullptr) {
            CPLDebug("Viewranger OVRV",
                     "%s nThisOverview==-1 but %d papoOverviewBands already set at %p",
                     poVRCDS->sLongTitle.c_str(),
                     nOverviewCount+1, papoOverviewBands);
        } else {
            if (nOverviewCount!=6) {
                // Seen Fri 24 Jul 2020
                CPLDebug("Viewranger OVRV",
                         "nThisOverview==-1 expected 6 overviews but given %d",
                         nOverviewCount);
                // nOverviewCount=6; // Hack. FIXME
            }
            if (nOverviewCount>=32) {
                // This is unnecessarily big;
                // the scale factor will not fit in an int, and
                // a 1cm / pixel map of the world will have a one pixel overview.
                // nOverviewCount=32;
                CPLDebug("Viewranger OVRV",
                         "%s Reducing nOverviewCount from %d to 6",
                         poVRCDS->sLongTitle.c_str(),
                         nOverviewCount);
                nOverviewCount=6;
            }
            if (nOverviewCount>=0) {
                papoOverviewBands = static_cast<VRCRasterBand **>
                    (CPLCalloc(sizeof(void*), 1+static_cast<size_t>(nOverviewCount)));
                if (papoOverviewBands==nullptr) {
                    // Seen Fri 24 Jul 2020
                    CPLError(CE_Failure, CPLE_OutOfMemory,
                             "Cannot allocate memory for %d overview bands",
                             nOverviewCount+1
                             );
                    return;
                }
            }
            CPLDebug("Viewranger OVRV",
                     "%s this = %p VRCRasterBand(%p, %d, %d, %d, %p)",
                     poVRCDS->sLongTitle.c_str(),
                     this, poVRCDS, nBandIn, nThisOverview,
                     nOverviewCount, papoOverviewBands);
            for (int i=0; i<nOverviewCount; i++) {
                if (papoOverviewBands[i]) {
                    CPLError( CE_Warning, CPLE_AppDefined,
                              "\toverview %p[%d] already set to %p",
                              papoOverviewBands, i, papoOverviewBands[i]
                              );
                } else {
                    papoOverviewBands[i]= new // 8 Feb 2021 leaks memory
                        VRCRasterBand(poVRCDS, nBand, i,
                                      // the overview has no overviews, so
                                      0, nullptr
                                      // not: nOverviewCount, papoOverviewBands
                                      );
                }
            } // for (int i=0; i<nOverviewCount; i++) {
        }
    } else { // !(nOverviewCount>=0 && nThisOverview == -1)
#if 1 // NOISY
        // if (getenv("VRC_NOISY"))
        {
            if (papoOverviewBands == nullptr) {
                CPLDebug("Viewranger OVRV",
                         "nOverviewCount==%d nThisOverview==%d and papoOverviewBands is null - OK",
                         nOverviewCount, nThisOverview);
            } else {
                CPLDebug("Viewranger OVRV",
                         "nThisOverview==%d but papoOverviewBands is already set to %p",
                         nThisOverview, papoOverviewBands);
            }
        }
#endif // NOISY

        if (nThisOverview<-1 || nThisOverview>nOverviewCount) { // Off-by-one somewhere ?
            CPLDebug("ViewrangerOverview",
                     "%s %p nThisOverview==%d out of range [-1,%d]",
                     poVRCDS->sLongTitle.c_str(),
                     this,
                     nThisOverview, nOverviewCount);
        }
    }
    
    CPLDebug("Viewranger",
             "%s %p->VRCRasterBand(%p, %d, %d, %d, %p) finished",
             poVRCDS->sLongTitle.c_str(),
             this, poVRCDS, nBand, nThisOverview,
             nOverviewCount, papoOverviewBands
             );

} // VRCRasterBand::VRCRasterBand()

/************************************************************************/
/*                          ~VRCRasterBand()                            */
/************************************************************************/

VRCRasterBand::~VRCRasterBand()
{
    // FlushCache();

    CPLDebug("Viewranger",
             "deleting %p->VRCRasterBand(%p, %d, %d, %d, %p)",
             this, poDS, nBand, nThisOverview,
             nOverviewCount, papoOverviewBands);

    if( nThisOverview<0 && papoOverviewBands) {
        CPLDebug("Viewranger",
                 "deleting papoOverviewBands %p",
                 papoOverviewBands);
        VRCRasterBand** papo=papoOverviewBands;
        papoOverviewBands=nullptr;
        if( nOverviewCount>0 ) {
            int nC=nOverviewCount;
            nOverviewCount=0;
            for( int i = 0; i < nC; i++ ) {
                if (papo[i]) {
                    papo[i]->nOverviewCount=0;
                    CPLDebug("Viewranger",
                             "deleting papoOverviewBands[%d]=%p",
                             i, papo[i]);
                    delete papo[i];
                    papo[i]=nullptr;
                }
            }
        }
        CPLFree( papo );
    }
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr VRCRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    auto *poGDS = static_cast<VRCDataset *>(poDS);
    
    CPLDebug("Viewranger", "IReadBlock(%d,%d,%p) %d",
             nBlockXOff, nBlockYOff, pImage, nThisOverview);
    CPLDebug("Viewranger", "Block (%d,%d) %d x %d band %d (%d x %d) overview %d",
             nBlockXOff, nBlockYOff,
             nBlockXSize,nBlockYSize,
             nBand,
             nRasterXSize, nRasterXSize,
             nThisOverview);
    if (nBlockXOff < 0 || nBlockXOff*nBlockXSize >= nRasterXSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Block (any,%d) overview %d does not exist %d* %d >?= %d",
                 nBlockXOff, nThisOverview,
                 nBlockXOff, nRasterXSize, nBlockXSize );
            return CE_Failure;
    }
    if (nBlockYOff < 0 || nBlockYOff*nBlockYSize >= nRasterYSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Block (any,%d)) overview %d does not exist %d* %d >?= %d",
                 nBlockYOff, nThisOverview, nBlockYOff,
                 poGDS->nRasterYSize, nBlockYSize );
            return CE_Failure;
    }

    if ( poGDS->nMagic == vrc_magic_metres) {
        read_VRC_Tile_Metres(poGDS->fp, nBlockXOff, nBlockYOff, pImage);
        // return CE_None; // I cannot yet confirm no errors

    } else  if (poGDS->nMagic == vrc_magic_thirtysix) {
        // Seeing whether we can do thirtysix files at the (PNG/raw?) block level
#if 0
        read_VRC_Tile_Metres(poGDS->fp, nBlockXOff, nBlockYOff, pImage);
#else
        read_VRC_Tile_ThirtySix(poGDS->fp, nBlockXOff, nBlockYOff, pImage);
#endif
        // return CE_None; // I cannot yet confirm no errors
    }

    CPLErr eErr = CE_None;
#if 0
    // Copied from pngdataset.cpp PNGRasterBand::IReadBlock() lines 311-318
    // Forcibly load the other bands associated with this scanline.
    // Is this recursive ?
    // Does GetRasterBand or GetLockedBlockRef call IReadBlock ?
    //
    // Does this need to understand overviews ?

    for(int iBand = 1; iBand < poGDS->GetRasterCount(); iBand++)
    {
        CPLDebug("Viewranger",
                 "Forcing band %d overview %d to be loaded.",
                 iBand+1, nThisOverview);
        GDALRasterBand *poBand = poGDS->GetRasterBand(iBand+1);
        // if( nThisOverview != -1 ) {
        //     poBand = poBand->GetOverview( nThisOverview );
        // }
        if (poBand != nullptr ) {
            GDALRasterBlock *poBlock =
                poBand->GetLockedBlockRef(nBlockXOff,nBlockYOff);
            if( poBlock != nullptr ) {
                poBlock->DropLock();
            }
        } else {
            CPLDebug("Viewranger",
                 "GetRasterBand(band %d) is null: overview %d.",
                 iBand+1, nThisOverview);
            eErr = CE_Failure;
        }
    }
    CPLDebug("Viewranger",
             "Forced %d bands to be loaded.",
             poGDS->GetRasterCount());
#endif
    return eErr;
} // VRCRasterBand::IReadBlock()

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double VRCRasterBand::GetNoDataValue( int *pbSuccess )
{
  if (pbSuccess) {
      *pbSuccess=TRUE;
  }
  return nVRCNoData;
}

/************************************************************************/
/*                           IGetDataCoverageStatus()                           */
/************************************************************************/

#if GDAL_VERSION_NUM >= 2020000
    // See https://trac.osgeo.org/gdal/wiki/rfc63_sparse_datasets_improvements
    // and https://github.com/rouault/gdal2/blob/sparse_datasets/gdal/frmts/gtiff/geotiff.cpp
/* Most of this function
 * Copyright (c) 1998, 2002, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2015, Even Rouault <even dot rouault at spatialys dot com>
 */
int VRCRasterBand::IGetDataCoverageStatus( int nXOff, int nYOff,
                                           int nXSize, int nYSize,
                                           int nMaskFlagStop,
                                           double* pdfDataPct)
{
    int nStatus = 0;
    auto *poGDS = static_cast<VRCDataset *>(poDS);
    if ( poGDS->anTileIndex == nullptr ) {
        nStatus = GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED | GDAL_DATA_COVERAGE_STATUS_DATA;
        CPLDebug("Viewranger", "IGetDataCoverageStatus(%d, %d, %d, %d, %d, %p) not yet available - Tile Index not yet read",
                 nXOff, nYOff, nXSize, nYSize, nMaskFlagStop, pdfDataPct);
        if( pdfDataPct ) {
            *pdfDataPct = -1.0;
        }
        return nStatus;
    }

    CPLDebug("Viewranger", "IGetDataCoverageStatus(%d, %d, %d, %d, %d, %p) top skip %d right skip %d",
             nXOff, nYOff, nXSize, nYSize, nMaskFlagStop, pdfDataPct,
             poGDS->nTopSkipPix, poGDS->nRightSkipPix);

    const int iXBlockStart = nXOff / nBlockXSize;
    const int iXBlockEnd = (nXOff + nXSize - 1) / nBlockXSize;
    const int iYBlockStart = nYOff / nBlockYSize;
    const int iYBlockEnd = (nYOff + nYSize - 1) / nBlockYSize;

    GIntBig nPixelsData = 0;
    int nTopEdge = MAX( nYOff, poGDS->nTopSkipPix);
    int nRightEdge = //nXOff + nXSize; //
        MIN( nXOff + nXSize, poGDS->nRasterXSize - poGDS->nRightSkipPix);
    for( int iY = iYBlockStart; iY <= iYBlockEnd; ++iY ) {
        for( int iX = iXBlockStart; iX <= iXBlockEnd; ++iX ) {
            const int nBlockIdBand0 =
                iX + iY * nBlocksPerRow;
            int nBlockId = nBlockIdBand0;
            bool bHasData = false;
            if( poGDS->anTileIndex[nBlockId] == 0 ) {
                nStatus |= GDAL_DATA_COVERAGE_STATUS_EMPTY;
            } else {
                bHasData = true;
            }
            if( bHasData ) {
                // We could be more accurate by looking at the png sub-tiles.
                // We should also discount any strip we added for short (or narrow?) tiles.
                nPixelsData += (MIN( (iX + 1) * nBlockXSize, nRightEdge ) - MAX( iX * nBlockXSize, nXOff )) *
                               (MIN( (iY + 1) * nBlockYSize, nYOff+nYSize) - MAX( iY * nBlockYSize, nTopEdge ));
                nStatus |= GDAL_DATA_COVERAGE_STATUS_DATA;
            }
            if( nMaskFlagStop != 0 && (nMaskFlagStop & nStatus) != 0 ) {
                if( pdfDataPct ) {
                    *pdfDataPct = -1.0;
                }
                return nStatus;
            }
        }
    }

    double dfDataPct =
        100.0 * static_cast<double>(nPixelsData) /
        (static_cast<double>(nXSize) * static_cast<double>(nYSize));
    if (pdfDataPct) {
        *pdfDataPct = dfDataPct;
    }

    CPLDebug("Viewranger",
             "IGetDataCoverageStatus(%d, %d, %d, %d, %d, %p) returns %d with %lf%% coverage",
             nXOff, nYOff, nXSize, nYSize, nMaskFlagStop,
             pdfDataPct, nStatus, dfDataPct);

    return nStatus;
} // VRCRasterBand::IGetDataCoverageStatus()
#endif

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp VRCRasterBand::GetColorInterpretation()

{
    auto *poGDS = static_cast<VRCDataset*>(poDS);
    if (poGDS->nMagic == vrc_magic_metres) {
        CPLDebug("Viewranger",
                 "VRCRasterBand::GetColorInterpretation vrcmetres GetColorInterpretation %08x %d",
                 poGDS->nMagic, this->eBandInterp);
        return this->eBandInterp;
    } else if (poGDS->nMagic == vrc_magic_thirtysix) {
        CPLDebug("Viewranger",
                 "VRCRasterBand::GetColorInterpretation vrcthirtysix GetColorInterpretation %08x %d",
                 poGDS->nMagic, this->eBandInterp);
        return this->eBandInterp;
    } else {
        CPLDebug("Viewranger",
                 "VRCRasterBand::GetColorInterpretation unexpected magic %08x - GetColorInterpretation %d -but returning GrayIndex",
                 poGDS->nMagic, this->eBandInterp);
        return GCI_GrayIndex;
    }
}


/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *VRCRasterBand::GetColorTable()

{
#if 0
    VRCDataset  *poGDS = static_cast<VRCDataset *>(poDS);

    if( nBand == 1 )
        return poGDS->poColorTable;
    else
#endif
        return nullptr;
}


/************************************************************************/
/* ==================================================================== */
/*                            VRCDataset                                */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            VRCDataset()                              */
/************************************************************************/

VRCDataset::VRCDataset() :
    fp(nullptr),
    poColorTable ( nullptr),
    anColumnIndex ( nullptr),
    anTileIndex ( nullptr),

    // These are only here to keep cppcheck happy
    // They may make the program think things are OK when they are not.
#ifdef CODE_ANALYSIS
    nMagic(0),
    dfPixelMetres(0.0),
    nMapID(-1),

    nLeft(INT_MAX),
    nRight(INT_MIN),
    nTop(INT_MIN),
    nBottom(INT_MAX),
    nTopSkipPix(0),
    nRightSkipPix(0),
    nScale(0),
    nMaxOverviewCount(7),
    nCountry(-1),
#endif // CODE_ANALYSIS

    poSRS(nullptr)

    // ,sLongTitle("")
    // ,sCopyright("")

#ifdef CODE_ANALYSIS
    , tileSizeMax(0),
    tileSizeMin(INT_MAX),
    tileXcount(0),
    tileYcount(0)
#endif // CODE_ANALYSIS
{

    // Real "initialization" is done later, in VRCDataset::Open()
    // but some values need to be set before
    // IGetDataCoverageStatus() is called.
    nTopSkipPix=0;
    nRightSkipPix=0;
    
    CPLDebug("Viewranger",
             "creating VRCDataset %p",
             this);

} // VRCDataset::VRCDataset()


/************************************************************************/
/*                           ~VRCDataset()                             */
/************************************************************************/

VRCDataset::~VRCDataset()
{
    FlushCache();
    if( fp != nullptr )
        VSIFCloseL( fp );

    if( poColorTable != nullptr )
        delete ( poColorTable );

    if (anColumnIndex != nullptr ) {
        VSIFree(anColumnIndex);
        anColumnIndex = nullptr;
    }
    if (anTileIndex != nullptr ) {
        VSIFree(anTileIndex);
        anTileIndex = nullptr;
    }
#if 0
    if (pszLongTitle != nullptr ) {
        VSIFree(pszLongTitle);
        pszLongTitle = nullptr;
    }
    if (pszCopyright != nullptr ) {
        VSIFree(pszCopyright);
        pszCopyright = nullptr;
    }
#elif 0 // These don't compile
    if (sLongTitle) {
        VSIFree(sLongTitle);
        sLongTitle = nullptr;
    }
    if (sCopyright) {
        VSIFree(sCopyright);
        sCopyright = nullptr;
    }
#endif
#if 0
    // trying to free strings created in VRCDataset::Open
    if (paszStrings) {
        for (int ii=0; ii<nStringCount; ++ii) {
            if (paszStrings[ii]) {
                VSIFree(paszStrings[ii]);
                paszStrings[ii] = nullptr;
            }
        }
        VSIFree(paszStrings);
        paszStrings = nullptr;
    }
#endif

    if (poSRS) {
        poSRS->Release();
        poSRS = nullptr;
    }
} // VRCDataset::~VRCDataset()

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/
CPLErr VRCDataset::GetGeoTransform( double * padfTransform )
{
    const double tenMillion = 10.0 * 1000 * 1000;

    double dLeft = nLeft;
    double dRight = nRight;
    double dTop = nTop ;
    double dBottom = nBottom ;

    if (nCountry == 17) {
        // This may not be correct
        // USA, Discovery (Spain) and some Belgium (VRH height) maps have coordinate unit of
        //   1 degree/ten million
        CPLDebug("Viewranger", "country/srs 17 USA?Belgium?Discovery(Spain) grid is unknown. Current guess is unlikely to be correct.");
#if 0 // standard until 20 Sept 2020
        dLeft   /= tenMillion;
        dRight  /= tenMillion;
        dTop    /= tenMillion;
        dBottom /= tenMillion;
#else
        const double nineMillion = 9.0 * 1000 * 1000;
        dLeft   /= nineMillion;
        dRight  /= nineMillion;
        dTop    /= nineMillion;
        dBottom /= nineMillion;
#endif // standard until 20 Sept 2020
        CPLDebug("Viewranger", "scaling by 10 million: TL: %g %g BR: %g %g",
                 dTop,dLeft,dBottom,dRight);
    } else if (nCountry == 155) {
        // New South Wales, Australia uses GDA94/MGA55 EPSG:28355
        // but without the 10million metre false_northing
        dLeft   = 1.0*nLeft;
        dRight  = 1.0*nRight;
        dTop    = 1.0*nTop + tenMillion;
        dBottom = 1.0*nBottom + tenMillion;

        CPLDebug("Viewranger", "shifting by 10 million: TL: %g %g BR: %g %g",
                 dTop,dLeft,dBottom,dRight);
    }

    // Xgeo = padfTransform[0] + pixel*padfTransform[1] + line*padfTransform[2];
    // Ygeo = padfTransform[3] + pixel*padfTransform[4] + line*padfTransform[5];
    if (nMagic == vrc_magic_metres) {
        padfTransform[0] = dLeft;
        padfTransform[1] = (1.0*dRight - dLeft) / (GetRasterXSize() /* -1.0 */);
        padfTransform[2] = 0.0;
        padfTransform[3] = dTop;
        padfTransform[4] = 0.0;
        padfTransform[5] = (1.0*dBottom - dTop) / (GetRasterYSize() /* -1.0 */);
    } else if (nMagic == vrc_magic_thirtysix) {
        padfTransform[0] = dLeft;
        padfTransform[1] = (1.0*dRight - dLeft);
        padfTransform[2] = 0.0;
        padfTransform[3] = dTop;
        padfTransform[4] = 0.0;
        padfTransform[5] = (1.0*dBottom - dTop);
#if !defined VRC36_PIXEL_IS_FILE
        padfTransform[1] /= (GetRasterXSize());
        padfTransform[5] /= (GetRasterYSize());
#endif
    } else {
        CPLDebug("Viewranger", "nMagic x%08x unknown", nMagic);
        padfTransform[0] = dLeft;
        padfTransform[1] = (1.0*dRight - dLeft) / (GetRasterXSize() /* -1.0 */);
        padfTransform[2] = 0.0;
        padfTransform[3] = dTop;
        padfTransform[4] = 0.0;
        padfTransform[5] = (1.0*dBottom - dTop) / (GetRasterYSize() /* -1.0 */);
    }

    CPLDebug("Viewranger", "padfTransform raster %d x %d", GetRasterXSize(), GetRasterYSize());
    CPLDebug("Viewranger", "padfTransform %g %g %g", padfTransform[0], padfTransform[1], padfTransform[2]);
    CPLDebug("Viewranger", "padfTransform %g %g %g", padfTransform[3], padfTransform[4], padfTransform[5]);
    return CE_None;
} // GetGeoTransform()

/************************************************************************/
/*                           CleanOverviews()                           */
/************************************************************************/

#if 0
CPLErr VRCRasterBand::CleanOverviews()
{
    CPLDebug("Viewranger",
             "%p->VRCRasterBand::CleanOverviews() band=%d, overview %d of %d, %p\n",
             poDS, nBand,
             nThisOverview, nOverviewCount,
             papoOverviewBands
             );
    
    if( nOverviewCount == 0 )
        return CE_None;

    // Clear our reference to overviews as bands.
    for( int iOverview = 0; iOverview < nOverviewCount; iOverview++ ) {
        delete papoOverviewBands[iOverview];
    }

    CPLFree(papoOverviewBands);
    papoOverviewBands = nullptr;
    nOverviewCount = 0;

#if 0
    // Search for any RRDNamesList and destroy it.
    HFABand *poBand = hHFA->papoBand[nBand - 1];
    HFAEntry *poEntry = poBand->poNode->GetNamedChild("RRDNamesList");
    if( poEntry != nullptr )
    {
        poEntry->RemoveAndDestroy();
    }

    // Destroy and subsample layers under our band.
    for( HFAEntry *poChild = poBand->poNode->GetChild();
         poChild != nullptr; )
    {
        HFAEntry *poNext = poChild->GetNext();

        if( EQUAL(poChild->GetType(), "Eimg_Layer_SubSample") )
            poChild->RemoveAndDestroy();

        poChild = poNext;
    }
#endif
    return CE_None;
} // VRCRasterBand::CleanOverviews()
#endif


/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int VRCDataset::Identify( GDALOpenInfo * poOpenInfo )

{
#if 1
    const char* fileName = CPLGetFilename(poOpenInfo->pszFilename);
    if( fileName==nullptr ) {
        return GDAL_IDENTIFY_FALSE;
    }
    if( !EQUAL(fileName + strlen(fileName) - strlen(".VRC"), ".VRC")) {
        return GDAL_IDENTIFY_FALSE;
    }
#endif

    if ( poOpenInfo->nHeaderBytes < 12 ) {
         return GDAL_IDENTIFY_UNKNOWN;
    }

    unsigned int nMagic = VRGetUInt(poOpenInfo->pabyHeader, 0);
    // unsigned int version = VRGetUInt(poOpenInfo->pabyHeader, 4);

    bool b64k1=(VRGetUInt(poOpenInfo->pabyHeader, 8) == 0x10001);

    if( nMagic == vrc_magic_metres ) {
        CPLDebug("Viewranger", "VRCmetres file %s supported",
                 poOpenInfo->pszFilename);
        if (!b64k1) {
            CPLDebug("Viewranger",
                     "VRC file %s - limited support for unusual third long (not x10001)",
                     poOpenInfo->pszFilename);
        }
        return GDAL_IDENTIFY_TRUE;
    } else if( nMagic == vrc_magic_thirtysix ) {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "%s: image data for .VRC magic 0x3663ce01 files not yet understood",
                 poOpenInfo->pszFilename);
        if (!b64k1) {
            CPLDebug("Viewranger",
                     "VRC file %s - limited support for unusual third long (not x10001)",
                     poOpenInfo->pszFilename);
        }
        return GDAL_IDENTIFY_TRUE;
    }
    return GDAL_IDENTIFY_FALSE;
} // VRCDataset::Identify()


/************************************************************************/
/*                              VRCGetTileIndex()                       */
/************************************************************************/


unsigned int* VRCDataset::VRCGetTileIndex( unsigned int nTileIndexStart )
{
    // We were reading from abyHeader;
    // the next bit may be too big for that,
    // so we need to start reading directly from the file.

    //int nTileStart = -1;
    if ( VSIFSeekL( fp, static_cast<size_t>(nTileIndexStart), SEEK_SET ) ) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "cannot seek to VRC tile index" );
        return nullptr;
    }

#if 0 // only true for MapId 8. Maybe not even then.
    int nSeven=VRReadInt(fp);
    if (nSeven == 7) {
        // CPLDebug does not support m$ in format strings
        CPLDebug("Viewranger",
                 "VRCGetTileIndex(): %d=x%08x points to seven as expected",
                 startHere, startHere);
    } else {
        CPLDebug("Viewranger",
                 "VRCGetTileIndex(): %d=x%08x arg points to %08x - not seven",
                 startHere, startHere, nSeven);
    }
#endif

    auto *anNewTileIndex = static_cast<unsigned int *>
        (VSIMalloc3(sizeof (unsigned int),
                    static_cast<size_t>(tileXcount),
                    static_cast<size_t>(tileYcount)) );
    if (anNewTileIndex == nullptr) {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Cannot allocate memory for tile index");
        return nullptr;
    }

    // Read Tile Index into memory
    // rotating it as we read it,
    // since viewranger files start by going up the left column
    // whilst gdal expects to go left to right across the top row.
    for (int i=0; i<tileXcount; i++) {
        int q = tileXcount*(tileYcount-1) +i;
        for (int j=0; j<tileYcount; j++) {
            unsigned int nValue = VRReadUInt(fp);
            // Ignore the index if it points
            // outside the limits of the file
            if (/* nValue <= 0 || */ nValue >= oStatBufL.st_size) {
                CPLDebug("Viewranger",
                         "anNewTileIndex[%d] (%d %d) addr x%08x not in file",
                         q, i, j, nValue);
                nValue = 0; // nVRCNoData ? ;
            }
            CPLDebug("Viewranger",
                     "setting anNewTileIndex[%d] (%d %d) to %d=x%08x",
                     q, i, j, nValue, nValue);
            anNewTileIndex[q] = nValue;
            q -= tileXcount;
        }
    }

    // Separate loop, since the previous loop has sequential reads
    // and this loop has random reads.
    for (int q=0; q<tileXcount*tileYcount; q++) {
        unsigned int nIndex=anNewTileIndex[q];
        if (nIndex<16) {
            CPLDebug("Viewranger",
                     "anNewTileIndex[%d]=x%08x=%d - points into file header",
                     q, nIndex, nIndex);
            anNewTileIndex[q] = 0;
            continue;
        }

        // This looks promising on DE_50 tiles
        // see how good it is in general
        if ( (nIndex %100) == 0 && nIndex < 10000) {
            CPLDebug("Viewranger",
                     "anNewTileIndex[%d]=x%08x=%d - ignore small multiples of 100",
                     q, nIndex, nIndex);
            anNewTileIndex[q] = 0;
            continue;
        }
        int nValue = VRReadInt(fp, nIndex);
        if (/*nIndex>0 && */nValue != 7) {
            CPLDebug("Viewranger",
                     "anNewTileIndex[%d]=%08x points to %d=x%08x - expected seven.",
                     q, nIndex, nValue, nValue);
        }
    }
    return anNewTileIndex;
} // VRCDataset::VRCGetTileIndex()

// MapId==8 files may have more than one tile.
// If so there is not tile index (that I can find),
// so we have to wander through the tile overview indices to build it.
//
// This may be a bit hacky.
//
unsigned int* VRCDataset::VRCBuildTileIndex( unsigned int nTileIndexStart )
{
    if (nMapID!=8) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "VRCBuildTileIndex called for a map with mapID %d",
                  nMapID);
    }
    if ( VSIFSeekL( fp, static_cast<size_t>(nTileIndexStart), SEEK_SET ) ) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "cannot seek to VRC tile index start 0x%xu",
                  nTileIndexStart);
        return nullptr;
    }
    if (tileXcount<=0 || tileYcount<=0) {
         CPLError( CE_Failure, CPLE_AppDefined,
                  "VRCBuildTileIndex(x%x) called for empty (%d x %d) image",
                   nTileIndexStart, tileXcount, tileYcount);
        return nullptr;
    }

    auto *anNewTileIndex = static_cast<unsigned int *>
        (VSIMalloc3(sizeof (unsigned int),
                    static_cast<size_t>(tileXcount),
                    static_cast<size_t>(tileYcount)) );
    if (anNewTileIndex == nullptr) {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Cannot allocate memory for tile index");
        return nullptr;
    }
    int nTileFound=0;
    for (nTileFound=0; nTileFound < tileXcount * tileYcount; nTileFound++) {
        anNewTileIndex[nTileFound]=0;
    }
    nTileFound=0;
    unsigned int nLastTileFound = anNewTileIndex[nTileFound++] = nTileIndexStart;
    while (nTileFound < tileXcount * tileYcount) {
        // VR tiles start at the bottom left and count up then right;
        // GDAL tiles start at the top left and count across then down.
        int nVRow = nTileFound % tileYcount;
        // int nGRow = tileYcount-1 - nVRow;
        // int nVCol = (nTileFound-nVRow) / tileYcount;
        // int nGCol = nVCol;
        // int nGdalTile = nGCol + nGRow * tileXcount;
        // int nGdalTile = (nTileFound-nTileFound % tileYcount) / tileYcount
        //    + (tileYcount-1 - nTileFound % tileYcount) * tileXcount;
        int nGdalTile = (nTileFound - nVRow) / tileYcount + nVRow * tileXcount;

        // Ignore the index if it points
        // outside the limits of the file
        if (/* nLastTileFound <= 0 || */ nLastTileFound >= oStatBufL.st_size) {
            if (nLastTileFound == oStatBufL.st_size) {
                CPLDebug("Viewranger",
                         "Searching for anTileIndex[%d]: nLastTileFound x%08x is end of file",
                         nTileFound, nLastTileFound);
            } else {
                CPLDebug("Viewranger",
                         "Searching for anTileIndex[%d]: nLastTileFound x%08x beyond end of file",
                         nTileFound, nLastTileFound);
            }
            break;
        }
        
        int nOverviewCount = VRReadInt(fp, nLastTileFound);
        if (nOverviewCount!=7) {
            CPLDebug("Viewranger",
                     "VRCBuildTileIndex(0x%08x) tile %d 0x%08x: expected OverviewIndex with 7 entries - got %d",
                     nTileIndexStart,
                     nTileFound, nLastTileFound,
                     nOverviewCount );
            //VSIFree(anNewTileIndex);
            //return nullptr;
            break;
        }
        unsigned int anOverviewIndex[7]={};
        size_t res=VSIFReadL(anOverviewIndex, 4, 7, fp);
        if (res!=7) {
            CPLDebug("Viewranger",
                     "VRCBuildTileIndex(%d) tile %d 0x%08x: expected OverviewIndex with %d entries - read %zd",
                     nTileIndexStart,
                     nTileFound, nLastTileFound,
                     7 //nOverviewCount,
                     , res );
            //VSIFree(anNewTileIndex);
            //return nullptr;
            break;
        }
        int nLastOI = nOverviewCount;
        while (nLastOI>0) {
            nLastOI--;
            if (anOverviewIndex[nLastOI]) {
                unsigned int x = VRReadUInt(fp, anOverviewIndex[nLastOI]);
                unsigned int y = VRReadUInt(fp);
                anNewTileIndex[nGdalTile] =
                    VRReadUInt(fp,
                              anOverviewIndex[nLastOI] +
                              (
                               2+2 // tile count and size
                               +x*y // ignore x by y matrix
                                    // and read the "pointer to end of last tile"
                               )*sizeof(int) );
                nLastTileFound=anNewTileIndex[nGdalTile];
                nTileFound++;
                break;
            }
        }
        if (nLastOI<=0) break;
    } // while (nTileFound < tileXcount * tileYcount)
    for (int y=0; y<tileYcount; y++) {
        for (int x=0; x<tileXcount; x++) {
            CPLDebug("Viewranger",
                     "anNewTileIndex[%d,%d] = %x",
                     x,y, anNewTileIndex[x+y*tileXcount] );
        }
    }
    return anNewTileIndex;
} //VRCDataset::VRCBuildTileIndex()

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *VRCDataset::Open( GDALOpenInfo * poOpenInfo )

{
    CPLDebug("Viewranger",
             "VRCDataset::Open( %p )",
             poOpenInfo);
    
    if (!Identify(poOpenInfo))
        return nullptr;

#if defined(__GNUC__)
    //    CPLDebug("Viewranger", "__GNUC__ defined as %d", __GNUC__);
    CPLDebug("Viewranger", "__GNUC__ defined");
#else
    CPLDebug("Viewranger", "__GNUC__ not defined");
#endif
#if defined(_GNU_SOURCE)
    CPLDebug("Viewranger", "_GNU_SOURCE defined as %d", _GNU_SOURCE);
#else
    CPLDebug("Viewranger", "_GNU_SOURCE not defined");
#endif
#if defined(GCC_VERSION)
    CPLDebug("Viewranger", "GCC_VERSION defined as %d", GCC_VERSION);
#else
    CPLDebug("Viewranger", "GCC_VERSION not defined");
#endif
#if defined(__clang__)
    CPLDebug("Viewranger", "__clang__ defined as %d", __clang__);
#else
    CPLDebug("Viewranger", "__clang__ not defined");
#endif
#if defined(__cplusplus)
    CPLDebug("Viewranger", "__cplusplus defined as %ld", __cplusplus);
#else
    CPLDebug("Viewranger", "__cplusplus not defined");
#endif
#if defined(_POSIX_C_SOURCE)
    CPLDebug("Viewranger", "_POSIX_C_SOURCE defined as %ld", _POSIX_C_SOURCE);
#else
    CPLDebug("Viewranger", "_POSIX_C_SOURCE not defined");
#endif
#if defined(WIN32)
    CPLDebug("Viewranger", "WIN32 defined as %s", "WIN32");
#else
    CPLDebug("Viewranger", "WIN32 not defined");
#endif
#if defined(override)
    CPLDebug("Viewranger", "override defined as %ld", override+0l);
#else
    CPLDebug("Viewranger", "override not defined");
#endif
#if defined(nullptr)
    CPLDebug("Viewranger", "nullptr defined as %d", nullptr);
#else
    CPLDebug("Viewranger", "nullptr not defined");
#endif
    
#if GDAL_VERSION_MAJOR >= 2
    /* Check that the file pointer from GDALOpenInfo* is available */
    if( poOpenInfo->fpL == nullptr )
    {
        return nullptr;
    }
#endif

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The VRC driver does not support update access to existing"
                  " datasets.\n" );
        return nullptr;
    }


/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */

    auto*poDS = new VRCDataset();

#if GDAL_VERSION_MAJOR >= 2
    /* Borrow the file pointer from GDALOpenInfo* */
    poDS->fp = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;
#else
    poDS->fp = VSIFOpenL( poOpenInfo->pszFilename, "rb" );
    if (poDS->fp == nullptr)
    {
        GDALClose(poDS);
        poDS=nullptr;
        return nullptr;
    }
#endif

/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */
    VSIFReadL( poDS->abyHeader, 1, 0x5a0, poDS->fp );

    poDS->nMagic = VRGetUInt(poOpenInfo->pabyHeader, 0);

    if (poDS->nMagic != vrc_magic_metres && poDS->nMagic != vrc_magic_thirtysix) {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "File magic 0x%08x unknown to viewranger VRC driver\n",
                  poDS->nMagic );
        GDALClose(poDS);
        poDS=nullptr;
        return nullptr;
    }

    {
        // Verify and/or report some unknown?unused values early in the header
        auto nVRCdownloadID =
            static_cast<unsigned int>(VRGetShort(poOpenInfo->pabyHeader, 4));
        unsigned int sixtyfourKplus1 = VRGetUInt( poDS->abyHeader, 8 );
        int byte12 = poDS->abyHeader[12];
        int byte13 = poDS->abyHeader[13];

        if (nVRCdownloadID!=4) {
            CPLDebug("Viewranger", "VRC file %s unexpected download ID %d",
                     poOpenInfo->pszFilename, nVRCdownloadID);
        }
        if (sixtyfourKplus1!=0x00010001) {
            CPLDebug("Viewranger", "VRC file %s expected 0x00010001 but got 0x%08x",
                     poOpenInfo->pszFilename, sixtyfourKplus1);
        }
        // No idea what byte12 and byte13 represent;
        // report them in case I can spot a pattern.
        if (byte12!=15) {
            // NSWRoadMap250k.VRC has 0xAA
            CPLDebug("Viewranger", "VRC file %s byte 0x0000000c is 0x%02x - expected 0x0f",
                 poOpenInfo->pszFilename, byte12);
        }
        if (byte13!=9) {
            CPLDebug("Viewranger", "VRC file %s byte 0x0000000d is 0x%02x - expected 0x09",
                     poOpenInfo->pszFilename, byte13);
        }
    }  // Verify and/or report some unknown?unused values early in the header

    poDS->nCountry = VRGetShort(poDS->abyHeader, 6);
    const char* szInCharset = CharsetFromCountry(poDS->nCountry);
    const char* szOutCharset = "UTF-8";

    CPLDebug("ViewRanger",
             "Country %d has charset %s",
             poDS->nCountry, szInCharset
             );

    poDS->nMapID = VRGetInt( poDS->abyHeader, 14 );
    if (   poDS->nMapID !=  -10 // Demo_{Hemsedal,Oslo}.VRC IrelandTrial50K.VRC
        && poDS->nMapID !=    0 // overviews and some demos
        && poDS->nMapID !=    8 // pay-by-tile
        && poDS->nMapID !=   16 // GreatBritain-250k-{FarNorth,North,South}.VRC
        && poDS->nMapID !=   22 // Finland1M.VRC
        && poDS->nMapID !=  293 // SouthTyrol50k/SouthTyro50k.VRC
        && poDS->nMapID !=  294 // TrentinoGarda50k.VRC
        && poDS->nMapID !=  588 // Danmark50k-*.VRC
        && poDS->nMapID != 3038 // 4LAND200AlpSouth
         ) {
#if 1
        CPLDebug("Viewranger", "VRC file %s unexpected Map ID %d",
                 poOpenInfo->pszFilename, poDS->nMapID);
#else
        CPLError(CE_Failure, CPLE_NotSupported,
                 "VRC file %s unexpected Map ID %d",
                 poOpenInfo->pszFilename, poDS->nMapID);
        return nullptr;
#endif
    }

    {
#define VRCpszMapIDlen 11
        char pszMapID[VRCpszMapIDlen]="";
        int ret=CPLsnprintf(pszMapID, VRCpszMapIDlen, "0x%08x", poDS->nMapID);
        pszMapID[VRCpszMapIDlen-1]='\000';
        if(ret==VRCpszMapIDlen-1) {
            poDS->SetMetadataItem("VRC ViewRanger MapID", pszMapID, "");
        } else {
            CPLDebug("Viewranger",
                     "Could not set MapID Metadata - CPLsnprintf( , VRCpszMapIDlen, 0x%08x) returned %d", poDS->nMapID, ret);
        }
#undef VRCpszMapIDlen
    }

    unsigned int nStringCount = VRGetUInt( poDS->abyHeader, 18 );
    unsigned int nNextString = 22;
    if (nStringCount == 0 && poDS->nMapID == 8) {
        // seems to be needed for pay-by-tile files
        CPLDebug("Viewranger", "Pay-by-tile; skipping null int before string count.");
        nStringCount = VRGetUInt( poDS->abyHeader, 22 );
        nNextString += 4;
    }
    CPLDebug("Viewranger", "VRC Map ID %d with %d strings",
             poDS->nMapID, nStringCount);
    if (poDS->nMagic == vrc_magic_metres) {
        CPLDebug("Viewranger", "vrc_magic_metres driver represents all pixels");
    } else {
#if defined VRC36_PIXEL_IS_FILE
        CPLDebug("Viewranger", "vrc_magic_thirtysix driver represents a whole file in each pixel");
#else
#if defined VRC36_PIXEL_IS_TILE
        CPLDebug("Viewranger", "vrc_magic_thirtysix driver represents a tile in each pixel");
#else
        // VRC36_PIXEL_IS_PIXEL is the default
        CPLDebug("Viewranger", "vrc_magic_thirtysix driver represents all pixels");
#endif
#endif
    }
    char ** paszStrings =
        static_cast<char **>(VSIMalloc2(sizeof (char *), nStringCount));
    if (paszStrings == nullptr) {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Cannot allocate memory for array strings");
        return nullptr;
    }
    for (unsigned int ii=0; ii<nStringCount; ++ii) {
        paszStrings[ii] = VRCGetString(poDS->fp, nNextString);
        nNextString += 4 + VRGetUInt( poDS->abyHeader, nNextString );
        CPLDebug("Viewranger", "string %u %s", ii,
                 paszStrings[ii] );

        if (*paszStrings[ii]) {
            // Save the string as a MetadataItem.
            const int VRCpszTAGlen=18;
            char pszTag[VRCpszTAGlen+1]="";
            int ret=CPLsnprintf(pszTag, VRCpszTAGlen, "String%u", ii);
            pszTag[VRCpszTAGlen] = '\000';
            if(VRCpszTAGlen>=ret && ret>0) {
                poDS->SetMetadataItem
                    ( pszTag,
                      CPLRecode(paszStrings[ii],
                                szInCharset, szOutCharset)
                      );
            } else {
                CPLDebug("Viewranger",
                         "Could not set String%d Metadata - CPLsnprintf(..., VRCpszTAGlen %s) returned %d",
                         ii, paszStrings[ii], ret);
            }
        }
    }
    if (nStringCount > 0) {
        poDS->sLongTitle = CPLRecode(paszStrings[0],
                                     szInCharset, szOutCharset);
        poDS->SetMetadataItem("TIFFTAG_IMAGEDESCRIPTION",
                              poDS->sLongTitle.c_str(), "" );
    }
    if (nStringCount > 1) {
        // poDS->sCopyright = CPLString(paszStrings[1]);
        poDS->sCopyright = CPLRecode(paszStrings[1],
                                     szInCharset, szOutCharset);
        poDS->SetMetadataItem("TIFFTAG_COPYRIGHT",
                              poDS->sCopyright.c_str(), "" );
    }
    if (nStringCount > 5 && *paszStrings[5]) {
        poDS->SetMetadataItem("VRC ViewRanger Device ID",
                              CPLString(paszStrings[5]).c_str(), "" );
    }

    poDS->nLeft   = VRGetInt( poDS->abyHeader, nNextString );
    poDS->nTop    = VRGetInt( poDS->abyHeader, nNextString +4 );
    poDS->nRight  = VRGetInt( poDS->abyHeader, nNextString +8 );
    poDS->nBottom = VRGetInt( poDS->abyHeader, nNextString +12 );
    poDS->nScale  = VRGetUInt( poDS->abyHeader, nNextString +16 );
    if (poDS->nScale==0) {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot locate a VRC map with zero scale");
        return nullptr;        
    }

    // based on 10 pixels/millimetre (254 dpi)
    poDS->dfPixelMetres = poDS->nScale / 10000.0;
    if (static_cast<unsigned long>(lround(10000.0*poDS->dfPixelMetres)) != poDS->nScale )
    {
        CPLDebug("Viewranger", "VRC %f metre pixels is not exactly 1:%d",
                 poDS->dfPixelMetres, poDS->nScale);
    } else {
        CPLDebug("Viewranger", "VRC %f metre pixels",
                 poDS->dfPixelMetres);
    }
    if (poDS->dfPixelMetres <0.5) {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Map with %g metre pixels is too large scale (detailed) for the current VRC driver",
                 poDS->dfPixelMetres);
        return nullptr;        
    }
    
    {
        double dfRasterXSize =
            ((10000.0)*(poDS->nRight - poDS->nLeft)) / poDS->nScale;
        poDS->nRasterXSize = static_cast<int>(dfRasterXSize);
        double dfRasterYSize =
            ((10000.0)*(poDS->nTop - poDS->nBottom)) / poDS->nScale;
        poDS->nRasterYSize = static_cast<int>(dfRasterYSize);

        // cast to double to avoid overflow and loss of precision
        // eg  (10000*503316480)/327680000 = 15360
        //             but                 = 11 with 32bit ints.

        CPLDebug("Viewranger", "%d=%f x %d=%f pixels",
                 poDS->nRasterXSize, dfRasterXSize,
                 poDS->nRasterYSize, dfRasterYSize);

        if  (poDS->nRasterXSize <= 0 || poDS->nRasterYSize <= 0 ) {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "Invalid dimensions : %d x %d",
                      poDS->nRasterXSize, poDS->nRasterYSize);
            GDALClose(poDS);
            poDS=nullptr;
            return nullptr;
        }
    }

    {
        poDS->tileSizeMax = VRGetUInt( poDS->abyHeader, nNextString +20 );
        poDS->tileSizeMin = VRGetUInt( poDS->abyHeader, nNextString +24 );
        if (poDS->tileSizeMax==0) {
            CPLError( CE_Failure, CPLE_NotSupported,
                     "tileSizeMax is zero and invalid"
                     );
            GDALClose(poDS);
            poDS=nullptr;
            return nullptr;
        }
        if (poDS->tileSizeMin==0) {
            poDS->tileSizeMin=poDS->tileSizeMax;
            CPLDebug("Viewranger",
                     "tileSizeMin is zero. Using tileSizeMax %d",
                     poDS->tileSizeMax
                     );
        }
        int bits = 63 - __builtin_clzll
            (static_cast<unsigned long long>(
                           poDS->tileSizeMax/poDS->tileSizeMin) );
        if (poDS->tileSizeMax == poDS->tileSizeMin << bits) {
            CPLDebug("Viewranger",
                     "%d / %d == %f == 2^%d",
                     poDS->tileSizeMax, poDS->tileSizeMin,
                     1.0*poDS->tileSizeMax/poDS->tileSizeMin, bits);
        } else {
            CPLDebug("Viewranger",
                     "%d / %d == %f != 2^%d",
                     poDS->tileSizeMax, poDS->tileSizeMin,
                     1.0*poDS->tileSizeMax/poDS->tileSizeMin, bits);
        }
        poDS->nMaxOverviewCount = 1+static_cast<unsigned int>(bits);
    
        // seven is not really used yet
        unsigned int seven = VRGetUInt( poDS->abyHeader, nNextString +28 );

        // I don't really know what chksum is but am curious about the value
        unsigned int chksum = VRGetUInt( poDS->abyHeader, nNextString +32 );
        // Record it in the metadata (TIFF tags or similar) in case it is important.
        const unsigned int VRCpszSumLen=11;
        char pszChkSum[VRCpszSumLen]="";
        int ret=CPLsnprintf(pszChkSum, VRCpszSumLen, "0x%08x", chksum);
        pszChkSum[VRCpszSumLen-1]='\000';
        if(ret==VRCpszSumLen-1) {
            poDS->SetMetadataItem("VRCchecksum", pszChkSum, "");
        } else {
            CPLDebug("Viewranger",
                     "Could not set VRCchecksum to 0x%08x", chksum);
        }

        poDS->tileXcount   = VRGetInt( poDS->abyHeader, nNextString +36 );
        poDS->tileYcount   = VRGetInt( poDS->abyHeader, nNextString +40 );

        CPLDebug("Viewranger", "tileSizeMax %d\ttileSizeMin %d",
                 poDS->tileSizeMax, poDS->tileSizeMin);
        if (seven != 7) {
            CPLDebug("Viewranger", "expected seven; got %d", seven);
        }
        CPLDebug("Viewranger", "chksum 0x%08x", chksum);
        CPLDebug("Viewranger", "tile count %d x %d",
                 poDS->tileXcount, poDS->tileYcount);

        // Sets        VSIStatBufL oStatBufL;
        // Find out how big the file is.
        // Used in VRCGetTileIndex to recognize noData values
        // and several other places.
        if (VSIStatL(poOpenInfo->pszFilename, &poDS->oStatBufL) ) {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "cannot stat file %s\n", poOpenInfo->pszFilename );
            return nullptr;
        }

        unsigned int nTileIndexAddr = nNextString + 44;

        if ( poDS->anTileIndex != nullptr ) {
            CPLDebug("Viewranger",
                     "poDS->anTileIndex unexpectedly set, to %p",
                     poDS->anTileIndex);
            // The address of the index isn't very useful for debugging,
            // so try to print some of the index. This may crash.
#undef VRC_DANGEROUS_TILE_INDEX
#if defined VRC_DANGEROUS_TILE_INDEX
            for (int ii=0; ii<poDS->tileXcount; ii++) {
                CPLDebug("Viewranger",
                         "\tpoDS->anTileIndex[%d] = %d=x%08x",
                         ii, poDS->anTileIndex[ii], poDS->anTileIndex[ii]);
            }
#endif // VRC_DANGEROUS_TILE_INDEX
            return nullptr; // ?
        }

        if (poDS->nMapID != 8) {
            // Read the index of tile addresses

            poDS->anTileIndex = poDS->VRCGetTileIndex( nTileIndexAddr );
            if ( poDS->anTileIndex == nullptr ) {
                CPLDebug("Viewranger",
                         "VRCGetTileIndex(%d=0x%08x) failed",
                         nTileIndexAddr, nTileIndexAddr);
            }

        } else { // So poDS->nMapID == 8
            if ( VSIFSeekL( poDS->fp, nTileIndexAddr, SEEK_SET ) ) {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "cannot seek to nTileIndexAddr %d=x%08x",
                          nTileIndexAddr, nTileIndexAddr);
                return nullptr;
            }
            CPLDebug("Viewranger",
                     "Pay-by-tile: skipping %dx%d values after tile count:",
                     poDS->tileXcount, poDS->tileYcount);
            for (int jj=0; jj<poDS->tileYcount; jj++) {
                for (int ii=0; ii<poDS->tileXcount; ii++) {
                    int nValue=VRReadInt(poDS->fp);
                    CPLDebug("Viewranger",
                             "\t(%d,%d) = %d=x%08x",
                             ii, jj, nValue, nValue);
                    (void) nValue; // CPLDebug doesn't count as "using" a variable
                }
            }
        }

        // Verify 07 00 00 00 01 00 01 00 01 00 01
        unsigned int nSecondSevenPtr =
            nTileIndexAddr + 4 *
            static_cast<unsigned int>(poDS->tileXcount) * static_cast<unsigned int>(poDS->tileYcount);

        if ( VSIFSeekL( poDS->fp, nSecondSevenPtr, SEEK_SET ) ) {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "cannot seek to nSecondSevenPtr %d=x%08x",
                      nSecondSevenPtr, nSecondSevenPtr);
            return nullptr;
        }

        int nSecondSeven=VRReadInt(poDS->fp);
        int nSignature1=VRReadInt(poDS->fp);
        int nSignature2=VRReadInt(poDS->fp);
        if ( nSecondSeven==7 &&
             nSignature1==0x00010001 &&
             (nSignature2&0x00ffffff)==0x010001) {
            CPLDebug("Viewranger",
                     "x%08x found expected signature 07 00 00 00 01 00 01 00 01 00 01",
                     nSecondSevenPtr
                     );
        } else {
            CPLDebug("Viewranger",
                     "x%08x got signature x%08x x%08x x%08x - expected 07 00 00 00 01 00 01 00 01 00 01",
                     nSecondSevenPtr,
                     nSecondSeven, nSignature1, nSignature2
                     );
        }

        unsigned int nCornerPtr=nSecondSevenPtr+11;  // skip over 07 00 00 00 01 00 01 00 01 00 01
        if ( // VSIFSeekL( poDS->fp, -1, SEEK_CUR ) // offset is unsigned :-(
            VSIFSeekL( poDS->fp, nCornerPtr, SEEK_SET )
             ) {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "cannot seek to VRC tile corners" );
                return nullptr;
        }

        // Tile corners here
        signed int anCorners[4]={};
        anCorners[0]=VRReadInt(poDS->fp);
        anCorners[1]=VRReadInt(poDS->fp);
        anCorners[2]=VRReadInt(poDS->fp);
        anCorners[3]=VRReadInt(poDS->fp);
        CPLDebug("Viewranger",
                 "x%08x LTRB (outer) %d %d %d %d",
                 nCornerPtr,
                 poDS->nLeft, poDS->nTop,
                 poDS->nRight, poDS->nBottom
                 );
        CPLDebug("Viewranger",
                 "x%08x LTRB (inner) %d %d %d %d",
                 nCornerPtr,
                 anCorners[0], anCorners[3],
                 anCorners[2], anCorners[1]
                 );

        if (poDS->nTop != anCorners[3]) {
            CPLDebug("Viewranger",
                     "mismatch original Top %d %d",
                     poDS->nTop, anCorners[3]
                     );
        }

        //   We have some short (underheight) tiles.
        // GDAL expects these at the top of the bottom tile,
        // but VRC puts these at the bottom of the top tile.
        //   We need to add a blank strip at the top of the
        // file up to compensate.
        double dfHeightPix = (poDS->nTop - poDS->nBottom) / poDS->dfPixelMetres;
        int nFullHeightPix = 0;
        if (poDS->tileSizeMax>0) {
            nFullHeightPix = static_cast<signed int>(poDS->tileSizeMax)
                * static_cast<signed int>(dfHeightPix/poDS->tileSizeMax);
        }
        if ( (poDS->nTop - poDS->nBottom) != (anCorners[3]- anCorners[1])
             || (poDS->nTop - poDS->nBottom) != static_cast<signed int>(poDS->nRasterYSize*poDS->dfPixelMetres) ) {
            // Equivalent to
            // if (dfHeightPix!=dfheight2 || dfHeightPix!=poDS->nRasterYSize) {
            // but without the division and floating-point equality test.
#ifndef CODE_ANALYSIS
            // Appease cppcheck.
            // It ignores CPLDebug then deduces that dfheight2 is not used.
            double dfheight2 =
                (anCorners[3]-anCorners[1]) / poDS->dfPixelMetres;
            CPLDebug("Viewranger",
                     "height either %d %g or %g pixels",
                     poDS->nRasterYSize, dfHeightPix, dfheight2
                     );
#endif
        }

        if (nFullHeightPix < dfHeightPix) {
            nFullHeightPix += poDS->tileSizeMax;
            int nNewTop = poDS->nBottom
                + static_cast<signed int>(nFullHeightPix*poDS->dfPixelMetres);
            poDS->nTopSkipPix = nFullHeightPix - static_cast<signed int>(dfHeightPix);
            CPLDebug("Viewranger",
                     "Adding %d pixels at top edge - from %d to %d - height was %d now %d",
                     poDS->nTopSkipPix,
                     poDS->nTop, nNewTop,
                     poDS->nRasterYSize, nFullHeightPix
                     );
            poDS->nTop = nNewTop;
            if (poDS->nTop != anCorners[3]) {
                CPLDebug("Viewranger",
                         "mismatch new Top %d %d",
                         poDS->nTop, anCorners[3]
                         );
            }
            poDS->nRasterYSize = nFullHeightPix;
        }

        if (poDS->nLeft != anCorners[0]) {
            CPLDebug("Viewranger",
                     "Unexpected mismatch Left %d %d",
                     poDS->nLeft, anCorners[0]
                     );
        }
        if (poDS->nBottom != anCorners[1]) {
            CPLDebug("Viewranger",
                     "Unexpected mismatch Bottom %d %d",
                     poDS->nBottom, anCorners[1]
                     );
        }
        if (poDS->nRight != anCorners[2]) {
            //   Unlike the top edge, GDAL and VRC agree that
            // narrow tiles are at the left edge of the right-most tile.
            //   We don't need to adjust anything for this case...
            CPLDebug("Viewranger",
                     "mismatch Right %d %d",
                     poDS->nRight, anCorners[2]
                     );
        }
        unsigned int nThirdSevenPtr=nCornerPtr+16; // Skip the corners


        int nThirdSeven=VRReadInt(poDS->fp);
        if (nThirdSeven == 7) {
            // CPLDebug does not support m$ in format strings
            CPLDebug("Viewranger",
                     "nThirdSevenPtr %d=x%08x points to seven as expected",
                     nThirdSevenPtr, nThirdSevenPtr);
        } else {
            CPLDebug("Viewranger",
                     "nThirdSevenPtr %d=x%08x points to %08x is not seven",
                     nThirdSevenPtr, nThirdSevenPtr, nThirdSeven);
        }

        if (poDS->nMapID == 8) {
            // Read the index of tile addresses
            if ( poDS->anTileIndex == nullptr ) {
#if 0   // Did this break DE...VRC ?
                poDS->anTileIndex = static_cast<unsigned int *>
                    (VSIMalloc3(sizeof (unsigned int),
                                static_cast<size_t>(poDS->tileXcount),
                                static_cast<size_t>(poDS->tileYcount) ));
                if (poDS->anTileIndex == nullptr) {
                    CPLError(CE_Failure, CPLE_OutOfMemory,
                             "Cannot allocate memory for block index");
                    return nullptr;
                }
                // poDS->anTileIndex[0] = nThirdSevenPtr+4;
                poDS->anTileIndex[0] = nThirdSevenPtr; // correct; we want the rest of the values too.
                {
                    // Dirty hack while testing
                    //   SE_50_W770010_E839990_S6610010_N6659990.VRC
                    // 21 Aug, 2020
                    // poDS->anTileIndex[1] = VRReadInt(poDS->fp);
                    //poDS->anTileIndex[2] = VRReadInt(poDS->fp);
                    
                    if ( // VSIFSeekL( poDS->fp, -4, SEEK_CUR ) // offset is unsigned :-(
                        VSIFSeekL( poDS->fp, nThirdSevenPtr, SEEK_SET )
                         ) {
                        CPLError( CE_Failure, CPLE_AppDefined,
                                  "cannot seek to VRC tile corners" );
                        return nullptr;
                    }
                    // Read Tile Index into memory
                    // rotating it as we read it,
                    // since viewranger files start by going up the left column
                    // whilst gdal expects to go left to right across the top row.
                    for (int i=0; i<poDS->tileXcount; i++) {
                        int q = poDS->tileXcount*(poDS->tileYcount-1) +i;
                        for (int j=0; j<poDS->tileYcount; j++) {
                            unsigned int nValue = VRReadUInt(poDS->fp);
                            // Ignore the index if it points
                            // outside the limits of the file
                            if (/* nValue <= 0 || */ nValue >= poDS->oStatBufL.st_size) {
                                CPLDebug("Viewranger",
                                         "anTileIndex[%d] (%d %d) addr x%08x not in file",
                                         q, i, j, nValue);
                                nValue = 0; // nVRCNoData ? ;
                            }
                            CPLDebug("Viewranger",
                                     "setting anTileIndex[%d] (%d %d) to %d=x%08x",
                                     q, i, j, nValue, nValue);
                            poDS->anTileIndex[q] = nValue;
                            q -= poDS->tileXcount;
                        }
                    }
                }
#else
                //poDS->anTileIndex = poDS->VRCGetTileIndex( nThirdSevenPtr+4 );
                poDS->anTileIndex = poDS->VRCBuildTileIndex( nThirdSevenPtr );
                //poDS->anTileIndex = poDS->VRCGetTileIndex( nThirdSevenPtr );
                if ( poDS->anTileIndex == nullptr ) {
                    CPLDebug("Viewranger", "VRCGetTileIndex(%d=0x%08x) failed",
                             nThirdSevenPtr, nThirdSevenPtr);
                    return nullptr;
                }
#endif
            } else {
                CPLDebug("Viewranger",
                         "poDS->anTileIndex unexpectedly set, to %p",
                         poDS->anTileIndex);
                // The address of the index isn't very useful for debugging,
                // so try to print some of the index. This may crash.
#if defined VRC_DANGEROUS_TILE_INDEX
                for (int ii=0; ii<poDS->tileXcount; ii++) {
                    CPLDebug("Viewranger",
                             "\tpoDS->anTileIndex[%d] = %d=x%08x",
                             ii, poDS->anTileIndex[ii], poDS->anTileIndex[ii]);
                }
#endif
            }
        }

        if (poDS->nMagic == vrc_magic_metres) {
            // nRasterXSize,nRasterYSize are fine
            // (perhaps except for short tiles ?)
            // but we need to get tileSizeMax/Min and/or tile[XY]count
            // into the band
        } else if (poDS->nMagic == vrc_magic_thirtysix) {
#if defined VRC36_PIXEL_IS_FILE
            CPLDebug("Viewranger", "each pixel represents a whole thirtysix-based file");
            // Fake an image with one pixel.
            poDS->nRasterXSize = 1;
            poDS->nRasterYSize = 1;
#else
#if defined VRC36_PIXEL_IS_TILE
            // Fake an image with one pixel for each tile.
            CPLDebug("Viewranger", "each pixel represents a thirtysix-based tile");
            poDS->nRasterXSize = poDS->tileXcount;
            poDS->nRasterYSize = poDS->tileYcount;
#else
            // VRC36_PIXEL_IS_PIXEL
            // this will be the default
            // nRasterXSize,nRasterYSize are fine
            // but we need to get tileSizeMax/Min and/or tile[XY]count
            // into the band
            CPLDebug("Viewranger", "each pixel represents a thirtysix-based pixel");
#endif
#endif
        } else {
            CPLDebug("Viewranger", "nMagic x%08x unknown", poDS->nMagic);
        }
    }
    if (paszStrings) {
        for (unsigned int ii=0; ii<nStringCount; ++ii) {
            if (paszStrings[ii]) {
                VSIFree(paszStrings[ii]);
                paszStrings[ii] = nullptr;
            }
        }
        VSIFree(paszStrings);
        paszStrings= nullptr ;
    }

    /********************************************************************/
    /*                              Set CRS                             */
    /********************************************************************/
    if(!poDS->poSRS) {
        poDS->poSRS = CRSfromCountry(poDS->nCountry);
    }

    
    /********************************************************************/
    /*             Report some strings found in the file                */
    /********************************************************************/
    CPLDebug("Viewranger", "Long Title: %s",poDS->sLongTitle.c_str());
    CPLDebug("Viewranger", "Copyright: %s",poDS->sCopyright.c_str());
    CPLDebug("Viewranger", "%g metre pixels",poDS->dfPixelMetres);
    if (poDS->nScale > 0) {
        CPLDebug("Viewranger", "Scale: 1: %d",poDS->nScale);
    } else {
        CPLDebug("Viewranger", "Scale not given");
    }


/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    // Until we support overviews, large files are very slow.
    // This environment variable allows users to skip them.
    int fSlowFile=FALSE;
   if (getenv("VRC_MAX_SIZE")!=nullptr) {
        long long nMaxSize = strtoll(getenv("VRC_MAX_SIZE"),nullptr,10);
        // Should support KMGTP... suffixes.
        if (nMaxSize < static_cast<VRCDataset *>(poDS)->oStatBufL.st_size) {
            CPLDebug("Viewranger",
                     "skipping file bigger than VRC_MAX_SIZE %lld",
                     nMaxSize);
            fSlowFile=TRUE;
        }
    }
    if (!fSlowFile) {
        int nMyBandCount=4;
        if (poDS->nMagic == vrc_magic_thirtysix) {
            nMyBandCount=1;
        }
        for (int i=1; i<=nMyBandCount; i++) {
            auto *poBand = new VRCRasterBand( poDS, i, -1, 6, nullptr);
            poDS->SetBand( i, poBand );

            if (i==4) {
                // Alpha band. Do we need to set a no data value ?
                poBand->SetNoDataValue( nVRCNoData );
            }
        }

        // More metadata.
        if( poDS->nBands > 1 ) {
            poDS->SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE" );
        }
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

    CPLDebug("Viewranger",
             "VRCDataset::Open( %p ) returns %p",
             poOpenInfo, poDS);
    return( poDS );
} // VRCDataset::Open()


void dumpPPM(unsigned int width,
             unsigned int height,
             unsigned char* const data,
             unsigned int rowlength,
             CPLString osBaseLabel,
             VRCinterleave eInterleave,
             unsigned int nMaxPPM
             )
{
    // Is static the best way to count the PPMs ?
    static unsigned int nPPMcount = 0;

    CPLDebug("Viewranger PPM",
             "dumpPPM(%d %d %p %d %s %s-interleaved) count %u",
             width, height, data, rowlength,
             osBaseLabel.c_str(),
             (eInterleave==pixel) ? "pixel" : "band",
             nPPMcount
             );
    if (osBaseLabel==nullptr) {
        CPLDebug("Viewranger PPM",
                 "dumpPPM: null osBaseLabel\n");
        return;
    }
    if (rowlength==0) {
        rowlength=width;
        CPLDebug("Viewranger PPM",
                 "dumpPPM(... %d %s) no rowlength, setting to width = %d",
                 0, osBaseLabel.c_str(), rowlength);
    }

    CPLString osPPMname =
        CPLString().Printf("%s.%05d.%s", osBaseLabel.c_str(), nPPMcount,
                           (eInterleave==pixel) ? "ppm" : "pgm"
                           );
    if (osPPMname==nullptr) {
        CPLDebug("Viewranger PPM",
                 "osPPMname truncated %s %d",
                 osBaseLabel.c_str(), nPPMcount);
    }
    char const*pszPPMname = osPPMname.c_str();

    if (nPPMcount>10 && nPPMcount>nMaxPPM) {
         CPLDebug("Viewranger PPM",
             "... too many PPM files; skipping  %s",
             pszPPMname
             );
         nPPMcount++;
         return;
    }

    CPLDebug("Viewranger PPM",
             "About to dump PPM file %s",
             pszPPMname
             );

    char errstr[256]="";
    VSILFILE *fpPPM = VSIFOpenL(pszPPMname, "w");
    if (fpPPM == nullptr) {
        int nFileErr=errno;
        VRC_file_strerror_r(nFileErr, errstr, 255);
        CPLDebug("Viewranger PPM",
                 "PPM data dump file %s failed; errno=%d %s",
                 pszPPMname, nFileErr, errstr
                 );
    } else {
        const size_t nHeaderBufSize = 40;
        char acHeaderBuf[nHeaderBufSize]="";
        size_t nHeaderSize=0;
        switch (eInterleave) {
        case pixel:
            nHeaderSize=static_cast<size_t>
                (CPLsnprintf(acHeaderBuf, nHeaderBufSize,
                          "P6\n%u %u\n255\n",
                          width, height) );
            break;
        case band:
            nHeaderSize=static_cast<size_t>
                (CPLsnprintf(acHeaderBuf, nHeaderBufSize,
                          "P5\n%u %u\n255\n",
                          width, height) );
            break;
#if 0
        default:
            break;
#endif
        }
        // CPLsnprintf may return negative values;
        // the cast to size_t converts these to large positive
        // values, so we only need one test.
        if (nHeaderSize>=nHeaderBufSize) {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "dumpPPM error generating header for %s\n",
                      pszPPMname);
            VSIFCloseL(fpPPM);
            return;
        }
        size_t nHeaderWriteResult=VSIFWriteL(acHeaderBuf, 1, nHeaderSize, fpPPM);
        if (nHeaderSize==nHeaderWriteResult) {
            const unsigned char* pRow = data;
            for (unsigned int r=0; r<height; r++) {
                if (eInterleave==pixel) {
                    if (width!=VSIFWriteL(pRow, 3, width, fpPPM)) {
                        int nWriteErr = errno;
                        VRC_file_strerror_r(nWriteErr, errstr, 255);
                        CPLError( CE_Failure, CPLE_AppDefined,
                                  "dumpPPM error writing %s row %d errno=%d %s\n",
                                  pszPPMname, r, nWriteErr, errstr);
                        break;
                    }
                    pRow += 3*rowlength;
                } else { // must be band interleaved
#if 0 // PPM
                    for (unsigned int c=0; c<width; c++) {
                        unsigned char acTmpBuf[4]="";
                        acTmpBuf[0] = pRow[c];
                        acTmpBuf[1] = pRow[c]; // +rowlength];
                        acTmpBuf[2] = pRow[c]; // +rowlength+rowlength];
                        // acTmpBuf[3] = 255; // opposite of nVRCNoData
                        //if (1!=VSIFWriteL(acTmpBuf, 4, 1, fpPPM)) {
                        if (1!=VSIFWriteL(acTmpBuf, 3, 1, fpPPM)) {
                            int nWriteErr = errno;
                            VRC_file_strerror_r(nWriteErr, errstr, 255);
                            CPLError( CE_Failure, CPLE_AppDefined,
                                      "dumpPPM error writing %s row %u col %u;errno=%d %s",
                                      pszPPMname, r, c, nWriteErr, errstr);
                            VSIFCloseL(fpPPM);
                            return; // nested break, goto or throw/catch would be better
                        }
                    }
                    pRow += rowlength; // pRow += 4*rowlength;
#else // PGM
                    size_t rowwriteresult=VSIFWriteL(pRow, 1, width, fpPPM);
#if 1 // Noisy
                    if (getenv("VRC_NOISY")) {
                        CPLDebug("Viewranger PGM",
                                 "dumpPPM: writing(%p, 1, %d, %p) returned %lu",
                                 pRow, width, fpPPM, rowwriteresult);
                    }
#endif // Noisy
                    if (width!=rowwriteresult) {
                        int nWriteErr = errno;
                        VRC_file_strerror_r(nWriteErr, errstr, 255);
                        CPLError( CE_Failure, CPLE_AppDefined,
                                  "dumpPPM error writing %s row %u: errno=%d %s",
                                  pszPPMname, r, nWriteErr, errstr);
                        break;
                    }
                    pRow += rowlength;
#endif // PPM or PGM
                } // pixel or band interleaved ?
            } // for row r
        } else { // nHeaderSize!=nHeaderWriteResult
            int nWriteErr=errno;
            VRC_file_strerror_r(nWriteErr, errstr, 255);
            // CPLError( CE_Failure, CPLE_AppDefined,
            CPLDebug("Viewranger PPM",
                     "dumpPPM error writing header for %s errno=%u %s",
                     pszPPMname, nWriteErr, errstr);
        }

        if (0!=VSIFCloseL(fpPPM)) {
            CPLDebug("Viewranger PPM",
                     "Failed to close PPM data dump file %s; errno=%d",
                     pszPPMname, errno
                     );
        } else {
            CPLDebug("Viewranger PPM",
                     "PPM data dumped to file %s",
                     pszPPMname
                     );
        }
    }

    nPPMcount++;

    // return;

} // dumpPPM

static
void dumpPNG(
        // unsigned int width,
        // unsigned int height,
        unsigned char* const data, // pre-prepared PNG data, *not* raw image.
        int nDataLen,
        CPLString osBaseLabel,
        unsigned int nMaxPNG
        )
{
    // Is static the best way to count the PNGs ?
    static unsigned int nPNGcount = 0;

    CPLDebug("Viewranger PNG",
             // "dumpPNG(%d %d %p %d %s) count %u)",
             "dumpPNG(%p %d %s) count %u)",
             // width, height,
             data, nDataLen,
             osBaseLabel.c_str(), nPNGcount
             );
    if (osBaseLabel==nullptr) {
        CPLDebug("Viewranger PNG",
                 "dumpPNG: null osBaseLabel\n");
        return;
    }

    CPLString osPPMname =
        CPLString().Printf("%s.%05d.png", osBaseLabel.c_str(), nPNGcount);
    if (osPPMname==nullptr) {
        CPLDebug("Viewranger PNG",
                 "osPPMname truncated %s %d",
                 osBaseLabel.c_str(), nPNGcount );
    }
    char const*pszPNGname = osPPMname.c_str();

    if (nPNGcount>10 && nPNGcount>nMaxPNG) {
         CPLDebug("Viewranger PNG",
             "... too many PNG files; skipping %s",
             pszPNGname
             );
         nPNGcount++;
         return;
    }

    CPLDebug("Viewranger PNG",
             "About to dump PNG file %s",
             pszPNGname
             );

    char pszErrStr[256]="";
    VSILFILE *fpPNG = VSIFOpenL(pszPNGname, "w");
    if (fpPNG == nullptr) {
        int nFileErr=errno;
        VRC_file_strerror_r(nFileErr, pszErrStr, 255);
        CPLDebug("Viewranger PNG",
                 "PNG data dump file %s failed; errno=%d %s",
                 pszPNGname, nFileErr, pszErrStr
                 );
    } else {
        size_t nWriteResult =
            VSIFWriteL( data, 1, static_cast<size_t>(nDataLen), fpPNG );
        if (static_cast<size_t>(nDataLen)!=nWriteResult) {
            int nFileErr=errno;
            VRC_file_strerror_r(nFileErr, pszErrStr, 255);
            CPLError( CE_Failure, CPLE_AppDefined,
                      "dumpPNG error writing %s result=%d errno=%lu\n\t%s",
                      pszPNGname, nFileErr, nWriteResult, pszErrStr);

            return;
        }
        if (0!=VSIFCloseL(fpPNG)) {
            int nFileErr=errno;
            VRC_file_strerror_r(nFileErr, pszErrStr, 255);
            CPLDebug("Viewranger PNG",
                     "Failed to close PNG data dump file %s; errno=%d %s",
                     pszPNGname, nFileErr, pszErrStr
                     );
        } else {
            CPLDebug("Viewranger PNG",
                     "PNG data dumped to file %s",
                     pszPNGname
                     );
        }
    }

    nPNGcount++;
} // dumpPNG()

png_byte *
VRCRasterBand::read_PNG(VSILFILE *fp,
                        // void *pImage,
                        unsigned int *pPNGwidth,
                        unsigned int *pPNGheight,
                        unsigned int nVRCHeader,
                        vsi_l_offset nPalette,
                        unsigned int nVRCDataLen,
                        // int nPNGXcount, int nPNGYcount,
                        int nGDtile_xx, int nGDtile_yy,
                        unsigned int nVRtile_xx, unsigned int nVRtile_yy
                        )
{
    unsigned int nVRCData = nVRCHeader+0x12;

    if (fp==nullptr) {
        CPLDebug("Viewranger PNG",
                 "read_PNG given null file pointer");
        return nullptr;
    }
#if 0
    if (pImage==nullptr) {
        CPLDebug("Viewranger PNG",
                 "read_PNG given null image pointer");
        return nullptr;
    }
#endif
    if (pPNGwidth==nullptr || pPNGheight==nullptr) {
        CPLDebug("Viewranger PNG",
                 "read_PNG needs space to return image size");
        return nullptr;
    }

    if (nVRCHeader == 0) {
        CPLDebug("Viewranger PNG",
                 "block (%d,%d) tile (%d,%d) nVRCHeader is nullptr",
                 nGDtile_xx, nGDtile_yy, nVRtile_xx, nVRtile_yy);
        return nullptr;
    }
    if (nVRCDataLen < 12) {
        CPLDebug("Viewranger PNG",
                 "block (%d,%d) tile (%d,%d) nVRCData is too small %u < 12",
                 nGDtile_xx, nGDtile_yy, nVRtile_xx, nVRtile_yy,
                 nVRCDataLen );
        return nullptr;
    }
    if (nVRCDataLen >= static_cast<VRCDataset *>(poDS)->oStatBufL.st_size) {
        CPLDebug("Viewranger PNG",
                 "block (%d,%d) tile (%d,%d) nVRCData is bigger %u than file %lu",
                 nGDtile_xx, nGDtile_yy, nVRtile_xx, nVRtile_yy,
                 nVRCDataLen, static_cast<VRCDataset *>(poDS)->oStatBufL.st_size);
        return nullptr;
    }

    png_voidp user_error_ptr=nullptr;
    // Initialize PNG structures
    png_structp png_ptr = png_create_read_struct
        (PNG_LIBPNG_VER_STRING, static_cast<png_voidp>(user_error_ptr),
         nullptr, nullptr // user_error_fn, user_warning_fn
         );
    if (!png_ptr) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "VRCRasterBand::read_PNG png_create_read_struct error %p\n",
                  user_error_ptr);
        return nullptr;
    }
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr,
                                static_cast<png_infopp>(nullptr), static_cast<png_infopp>(nullptr));
        CPLError( CE_Failure, CPLE_AppDefined,
                  "VRCRasterBand::read_PNG png_create_info_struct error %p\n",
                  user_error_ptr);
        return nullptr;
    }

    png_infop end_info = png_create_info_struct(png_ptr);
    if (!end_info) {
        png_destroy_read_struct(&png_ptr, &info_ptr,
                                static_cast<png_infopp>(nullptr));
        CPLError( CE_Failure, CPLE_AppDefined,
                  "VRCRasterBand::read_PNG end_info png_create_info_struct error %p\n",
                  user_error_ptr);
        return nullptr;
    }


   /*********************************************************************
    *
    * This is where we create the PNG file from the VRC data.
    *
    * I wish I could find a *simple* way to avoid reading it in to memory
    * only to pass it to the iterator VRC_png_read_data_fn.
    *
    *********************************************************************/

    const unsigned char PNG_sig[]    = {0x89,  'P',  'N',  'G', 0x0d, 0x0a, 0x1a, 0x0a};
    const unsigned char IHDR_head[]  = {0x00, 0x00, 0x00, 0x0d, 'I', 'H', 'D', 'R'};
    const unsigned char IEND_chunk[] = {0x00, 0x00, 0x00, 0x00, 'I', 'E', 'N', 'D',
                                       0xae, 0x42, 0x60, 0x82};

#if 0
    // Redundant ?
    // Find the length of the VRCData
    if ( VSIFSeekL( fp, nVRCData, SEEK_SET ) ) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "cannot seek to nVRCData %d=x%08x",
                  nVRCData, nVRCData);
        return nullptr;
    }
#endif

    // This is missing:
    // the IHDR data (+CRC),
    // the PLTE chunk, and
    // the IDAT data chunk(s).
    VRCpng_data oVRCpng_data = {nullptr, 0, 0};
    // C++11 does not let us cast unsigned-ness inside {}
    oVRCpng_data.length = static_cast<long>
        (sizeof(PNG_sig)
         +sizeof(IHDR_head)+13+4
         + 3*256 + 3*4   // enough for 256x3 entry palette plus length, "PLTE" and checksum.
         + nVRCDataLen     // IDAT chunks
         +sizeof(IEND_chunk));

    oVRCpng_data.pData = static_cast<png_byte*>
        (png_malloc(png_ptr,
                    static_cast<png_size_t>(oVRCpng_data.length)) );
    if (oVRCpng_data.pData == nullptr) {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Cannot allocate memory for copy of PNG file");
        return nullptr;
    }
    oVRCpng_data.current = 0;
    memcpy(static_cast<void*>(oVRCpng_data.pData),
           PNG_sig, sizeof(PNG_sig) );
    oVRCpng_data.current += sizeof(PNG_sig);

    // IHDR starts here
    memcpy(static_cast<void*>(&oVRCpng_data.pData[oVRCpng_data.current]),
           IHDR_head, sizeof(IHDR_head) );
    oVRCpng_data.current += sizeof(IHDR_head);

    // IHDR_data here

    char aVRCHeader[17]={};
    if ( VSIFSeekL( fp, nVRCHeader, SEEK_SET ) ) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "cannot seek to nVRCHeader %d=x%08x",
                  nVRCHeader, nVRCHeader);
        return nullptr;
    }
    if (int n=VRReadChar(fp)) {
        (void)n; // Appease static analysers
        CPLDebug("Viewranger PNG",
                 "%d=x%08x: First PNG header byte is x%02x - expected x00",
                 nVRCHeader, nVRCHeader, n);
    } else {
         CPLDebug("Viewranger PNG",
                  "%d=x%08x: First PNG header byte is x00 as expected",
                  nVRCHeader, nVRCHeader);
    }
    size_t count=VSIFReadL(aVRCHeader, 1, 17, fp);
    if (17>count) {
        CPLError( CE_Failure, CPLE_FileIO,
                  "only read %d of 17 bytes for PNG header\n",
                  static_cast<int>(count));
        return nullptr;
     }

    memcpy(static_cast<void*>(&oVRCpng_data.pData[oVRCpng_data.current]),
           aVRCHeader, 17 );
    unsigned int nPNGwidth=PNGGetUInt(aVRCHeader, 0);
    *pPNGwidth=nPNGwidth;
    unsigned int nPNGheight=PNGGetUInt(aVRCHeader, 4);
    *pPNGheight=nPNGheight;

    if (nPNGwidth==0 || nPNGheight==0) {
        CPLDebug("Viewranger PNG",
                 "empty PNG tile %d x %d (VRC tile %d,%d)",
                 nPNGwidth, nRasterXSize,
                 nVRtile_xx, nVRtile_yy);
        return nullptr;
    }

#if 0
    // We need to make *two* adjustments to the position
    // of the PNG tile within the GDAL block.
    // 1. If the last (top or right) VRC block is smaller than the others,
    //    then there may be fewer tiles in the block, eg:
    //       nPNGheight*(nPNGYcount+k) == nBlockYSize ?
    // 2. If nBlockXSizePNG / 2^n is not an integer
    //    then the total width/height of the PNG overview tiles
    //    will be slightly bigger than the GDAL block, eg:
    //       nPNGheight*nPNGYcount > nBlockYSize ?
    // I don't know whether both can occur in the same block.
    
    if (nPNGwidth*nPNGXcount!= nBlockXSize) { // nRasterXSize ?
        CPLDebug("Viewranger PNG",
                 "PNG width %d * PNG count %d != block width %d - G=%d V=%d",
                 nPNGwidth, nPNGXcount, nBlockXSize,
                 nGDtile_xx, nVRtile_xx);
    }
#endif


#if defined UseCountFull
    double dfPNGYcountFull = nBlockYSize/nPNGheight;
    int nPNGYcountFull = (int)(dfPNGYcountFull+.5);
    if (nPNGheight*nPNGYcount==nBlockYSize) { // nRasterYSize ? - nBlockYSize probably OK
        CPLDebug("Viewranger PNG",
                 "PNG height: %d * PNG count %d == block height %d - G=%d V=%d",
                 nPNGheight, nPNGYcount, nBlockYSize,
                 nGDtile_yy, nVRtile_yy);
    } else {
        bool bCase1 = false;
        bool bCase2 = false;
        if (nPNGYcount<nPNGYcountFull) {
            bCase1 = true;
            CPLDebug("Viewranger PNG",
                     "PNG height %d: %d pixel block G=%d V=%d has fewer PNGS (%d<%d) than other block rows",
                     nPNGheight, nBlockYSize,
                     nGDtile_yy, nVRtile_yy,
                     nPNGYcount, nPNGYcountFull
                     );
        }

        if (nPNGheight*nPNGYcountFull != nBlockYSize) {
            bCase2 = true;
            CPLDebug("Viewranger PNG",
                     "PNG height %d does not divide block height %d - counts %d %d - G=%d V=%d",
                     nPNGheight, nBlockYSize,
                     nPNGYcount, nPNGYcountFull,
                     nGDtile_yy, nVRtile_yy);
        }

        if (bCase1 && bCase2) {
            CPLDebug("Viewranger PNG", "PNG height: both cases");
        } else if (!bCase1 && !bCase2) {
            CPLDebug("Viewranger PNG",
                     "PNG height %d - PNG counts %d %d - block height %d - G=%d V=%d",
                     nPNGheight, nPNGYcountFull, nPNGYcount, nBlockYSize,
                     nGDtile_yy, nVRtile_yy);
            CPLDebug("Viewranger PNG", "PNG height: neither case");
        }
    }
#endif // defined UseCountFull

#if 0 // defined UsePNGsizeMin
    signed int nPNGwidthMin = std::min(nPNGwidth,nBlockXSize/nPNGXcount);
    signed int nPNGheightMin = std::min(nPNGheight,nBlockYSize/nPNGYcount);
    CPLDebug("Viewranger PNG",
             "nBlockXSize %d ?= nPNGwidthMin %d * nPNGXcount %d",
             nBlockXSize, nPNGwidthMin, nPNGXcount);
    CPLDebug("Viewranger PNG",
             "nBlockYSize %d ?= nPNGheightMin %d * nPNGYcount %d",
             nBlockYSize, nPNGheightMin, nPNGYcount);
#endif // defined UsePNGsizeMin

    // pbyPNGbuffer needs freeing in lots of places, before we return nullptr
    auto *pbyPNGbuffer = static_cast< png_byte*>
        (VSIMalloc3(3,
                    static_cast<size_t>(nPNGwidth), 
                    static_cast<size_t>(nPNGheight) ));
    if (pbyPNGbuffer == nullptr) {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Cannot allocate memory for PNG buffer");
        return nullptr;
    }
    // Do we need to zero the buffer ?
    for (unsigned int ii=0; ii<3*nPNGwidth*nPNGheight; ii++) {
        // apc_row_pointers[ii][jj] = 0;
        pbyPNGbuffer[ii] = nVRCNoData; // werdna July 14 2020
    }

    std::vector<unsigned char*> apc_row_pointers;
    apc_row_pointers.reserve(static_cast<size_t>(nPNGheight));
    for (unsigned int ii=0; ii<nPNGheight; ii++) {
        // clag-tidy readability-simplify-subscript-expr objects to
        apc_row_pointers.data()[ii] = 
        // but g++ -Werror,-Wsign-conversion objects to
        // apc_row_pointers[ii] = static_cast<unsigned char*>
            (&((static_cast<GByte*>(pbyPNGbuffer))[3*nPNGwidth*ii]));
    }
    png_set_rows(png_ptr, info_ptr, apc_row_pointers.data());

    auto nPNGdepth = static_cast<unsigned char>(aVRCHeader[8]);
    auto nPNGcolour = static_cast<unsigned char>(aVRCHeader[9]);
    auto nPNGcompress = static_cast<unsigned char>(aVRCHeader[10]);
    auto nPNGfilter = static_cast<unsigned char>(aVRCHeader[11]);
    auto nPNGinterlace = static_cast<unsigned char>(aVRCHeader[12]);
    unsigned int nPNGcrc = PNGGetUInt(aVRCHeader, 13);

    CPLDebug("Viewranger PNG",
             "PNG file: %d x %d depth %d colour %d, compress=%d, filter=%d, interlace=%d crc=x%08x",
             nPNGwidth, nPNGheight, nPNGdepth, nPNGcolour,
             nPNGcompress, nPNGfilter, nPNGinterlace, nPNGcrc
             );

    switch (nPNGdepth) {
    case 1:    case 2:    case 4:    case 8:
        break;
    // case 16:
    default:
        CPLDebug("Viewranger PNG",
                 "PNG file: Depth %d depth unsupported",
                 nPNGdepth);
        VSIFree(pbyPNGbuffer);
        return nullptr;
    }
    switch (nPNGcolour) {
    case 0: // Gray
        break;
    case 2: // RGB
        if (nPNGdepth==8) {
            break;
        }
        if (nPNGdepth==16) {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "16/48bit RGB unexpected");
            break;
        }
        CPL_FALLTHROUGH
    case 3: // Palette
        if (nPNGdepth<16 && nPNGcolour==3) {
            break;
        }
        CPLDebug("Viewranger PNG",
                 "PNG file: colour %d depth %d combination unsupported",
                 nPNGcolour, nPNGdepth);
        VSIFree(pbyPNGbuffer);
        return nullptr;
    // case 4: // Gray + Alpha
    // case 6: // RGBA
    default:
        CPLDebug("Viewranger PNG",
                 "PNG file: colour %d unsupported",
                 nPNGcolour);
        VSIFree(pbyPNGbuffer);
        return nullptr;
    }
    switch (nPNGcompress) {
    case 0:
        break;
    default:
        CPLDebug("Viewranger PNG",
                 "PNG file: compress %d unsupported",
                 nPNGcompress);
        VSIFree(pbyPNGbuffer);
        return nullptr;
    }
    switch (nPNGfilter) {
    case 0:
        break;
     default:
        CPLDebug("Viewranger PNG",
                 "PNG file: filter %d unsupported",
                 nPNGfilter);
        VSIFree(pbyPNGbuffer);
        return nullptr;
    }
    switch (nPNGinterlace) {
    case 0: // None
    case 1: // Adam7
        break;
    default:
        CPLDebug("Viewranger PNG",
                 "PNG file: interlace %d unsupported",
                 nPNGinterlace);
        VSIFree(pbyPNGbuffer);
        return nullptr;
    }

    int check = PNGCRCcheck(&oVRCpng_data, nPNGcrc);
    if (1!=check) {
        VSIFree(pbyPNGbuffer);
        return nullptr;
    }

    oVRCpng_data.current+= 13;
    oVRCpng_data.current+= 4;

    // PLTE chunk here (no "PLTE" type string in VRC data)
    if (nPalette!=0) {
        if ( VSIFSeekL( fp, nPalette, SEEK_SET ) ) {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "cannot seek to nPalette %llu=x%012llx",
                      nPalette, nPalette);
            VSIFree(pbyPNGbuffer);
            return nullptr;
        }

        unsigned int nVRCPlteLen = VRReadUInt(fp);
        if ( nVRCPlteLen > static_cast<VRCDataset *>(poDS)->oStatBufL.st_size) {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "implausible palette length %d=x%08x",
                      nVRCPlteLen, nVRCPlteLen);
            VSIFree(pbyPNGbuffer);
            return nullptr;
        }
        char *pVRCPalette = static_cast<char *>(VSIMalloc(nVRCPlteLen));
        if (pVRCPalette == nullptr) {
            CPLDebug("Viewranger PNG",
                     "could not allocate memory for pVRCPalette");
            VSIFree(pbyPNGbuffer);
            return nullptr;
        }

        size_t nBytesRead=VSIFReadL(pVRCPalette,1, nVRCPlteLen, fp);
        if (nVRCPlteLen!=nBytesRead) {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "at x%012llx cannot read %u=x%08x bytes of PNG palette data - read %012zx",
                      nPalette, nVRCPlteLen, nVRCPlteLen,nBytesRead );
            VSIFree(pbyPNGbuffer);
            return nullptr;
        }

        unsigned int nPNGPlteLen = PNGGetUInt(pVRCPalette,0);
        if ( nVRCPlteLen != nPNGPlteLen+8) {
            CPLDebug("Viewranger PNG",
                     "Palette lengths mismatch: VRC %d != PNG %u +8",
                     nVRCPlteLen, nPNGPlteLen
                     );
            VSIFree(pbyPNGbuffer);
            return nullptr;
        }
        if ( nPNGPlteLen > static_cast<VRCDataset *>(poDS)->oStatBufL.st_size) {
            CPLDebug("Viewranger PNG",
                     "PNGPalette length %u=x%08x bigger than file !",
                     nPNGPlteLen, nPNGPlteLen
                     );
            VSIFree(pbyPNGbuffer);
            return nullptr;
        }
        if (nPNGPlteLen %3) {
            CPLDebug("Viewranger PNG",
                     "palette size %d=x%08x not a multiple of 3",
                     nPNGPlteLen, nPNGPlteLen
                     );
            VSIFree(pbyPNGbuffer);
            return nullptr;
        } else {
            CPLDebug("Viewranger PNG",
                     "palette %d=x%08x bytes, %d entries",
                     nPNGPlteLen, nPNGPlteLen,
                     nPNGPlteLen/3
                     );
        }
        memcpy(static_cast<void*>(&oVRCpng_data.pData[oVRCpng_data.current]),
               static_cast<char*>(pVRCPalette),
               4 );
        oVRCpng_data.current+= 4;
        memcpy(static_cast<void*>(&oVRCpng_data.pData[oVRCpng_data.current]),
               "PLTE",
               4 );
        oVRCpng_data.current+= 4;

        memcpy(static_cast<void*>(&oVRCpng_data.pData[oVRCpng_data.current]),
               static_cast<char*>(pVRCPalette)+4,
               nPNGPlteLen+4 );
        oVRCpng_data.current+= nPNGPlteLen+4;
        VSIFree(pVRCPalette);
    } else { // if (nVRCpalette!=0)
        if (nPNGcolour == 3) {
            CPLDebug("Viewranger PNG",
                     "Colour type 3 PNG: needs a PLTE. Assuming Greyscale."
                     );
            // Next four bytes are 3*256 in PNGendian
            oVRCpng_data.pData[oVRCpng_data.current++] = 0;
            oVRCpng_data.pData[oVRCpng_data.current++] = 0;
            oVRCpng_data.pData[oVRCpng_data.current++] = 3;
            oVRCpng_data.pData[oVRCpng_data.current++] = 0;

            memcpy(static_cast<void*>(&oVRCpng_data.pData[oVRCpng_data.current]),
                   "PLTE",
                   4 );
            oVRCpng_data.current+= 4;

            for (int i=0; i<256; i++) {
                // for (unsigned char i=0; i<256; i++) gives
                //  i<256 is always true.
                // "& 255" is shorter than "static_cast<char>(i)"
                oVRCpng_data.pData[oVRCpng_data.current++] = i & 255;
                oVRCpng_data.pData[oVRCpng_data.current++] = i & 255;
                oVRCpng_data.pData[oVRCpng_data.current++] = i & 255;
            }

            // The checksum 0xe2b05d7d (not 0xa5d99fdd) of the greyscale palette.
            oVRCpng_data.pData[oVRCpng_data.current++] = 0xe2;
            oVRCpng_data.pData[oVRCpng_data.current++] = 0xb0;
            oVRCpng_data.pData[oVRCpng_data.current++] = 0x5d;
            oVRCpng_data.pData[oVRCpng_data.current++] = 0x7d;
        }
    }

    // IDAT chunk(s) here
    // werdna 26 June 2017. This is subject to verification and may well be wrong.

    // Jump to VRCData
    if ( VSIFSeekL( fp, nVRCData, SEEK_SET ) ) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "cannot seek to nVRCData %d=x%08x",
                  nVRCData, nVRCData);
        VSIFree(pbyPNGbuffer);
        return nullptr;
    }

    if (static_cast<size_t>(oVRCpng_data.length) <
        static_cast<size_t>(oVRCpng_data.current) +
        static_cast<size_t>(nVRCDataLen) + sizeof(IEND_chunk)
        ) {
        // Either we seg-faulted before we got here (if current > length)
        // or we will do.
        // Abort now.
        // FixME: We should either realloc, or use some other memory allocation system ...
        long long nNeeded=
            oVRCpng_data.current
            + static_cast<long long>(nVRCDataLen)
            + static_cast<long long>(sizeof(IEND_chunk));
        long long nMore= nNeeded - oVRCpng_data.length;
        CPLError( CE_Failure, CPLE_OutOfMemory,
                  "allocated %ld bytes for PNG but need %lld = %lld more",
                  oVRCpng_data.length,
                  nNeeded, nMore
                  );
        VSIFree(pbyPNGbuffer);
        // exit(0);
        return nullptr;
    }

    auto nBytesRead = static_cast<unsigned>
        (VSIFReadL(&oVRCpng_data.pData[oVRCpng_data.current],
                   1 , nVRCDataLen, fp));
    if (nVRCDataLen != nBytesRead) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "only read %u=x%08x bytes of PNG data out of %u=x%08x",
                  nBytesRead, nBytesRead, nVRCDataLen, nVRCDataLen);
        VSIFree(pbyPNGbuffer);
        return nullptr;
    }

    oVRCpng_data.current += nVRCDataLen;

    // IEND chunk is fixed and pre-canned.
    memcpy(static_cast<void*>(&oVRCpng_data.pData[oVRCpng_data.current]),
           IEND_chunk, sizeof(IEND_chunk) );
    oVRCpng_data.current+=sizeof(IEND_chunk);

    if (oVRCpng_data.length > oVRCpng_data.current) {
        int nPNGPlteLen = static_cast<int>
            (768 + oVRCpng_data.current - oVRCpng_data.length);
        if (nPNGPlteLen %3 != 0 || nPNGPlteLen<0 || nPNGPlteLen>768) {
            if (nPNGPlteLen!=780 || (nPNGcolour!=0 && nPNGcolour!=4)) {
                CPLDebug("Viewranger PNG",
                         "allocated %ld bytes for PNG but only copied %ld - short %ld bytes",
                         oVRCpng_data.length, oVRCpng_data.current,
                         oVRCpng_data.length - oVRCpng_data.current
                         );
            } else {
                // We allowed for a 768byte palette but the tile has none.
            }
        }
    }

    if (getenv("VRC_DUMP_PNG")) {
        auto nEnvPNGDump = static_cast<unsigned int>
            (strtol(getenv("VRC_DUMP_PNG"),nullptr,10));
        CPLString osBaseLabel
            = CPLString().
            Printf("/tmp/werdna/vrc2tif/%s.%01d.%03d.%03d.%03d.%03d.%02u.x%012x",
                   // CPLGetBasename(poOpenInfo->pszFilename) doesn't quite work
                   static_cast<VRCDataset *>(poDS)->sLongTitle.c_str(),
                   nThisOverview, nGDtile_xx, nGDtile_yy, nVRtile_xx, nVRtile_yy,
                   nBand, nVRCHeader);
        dumpPNG(
                // static_cast<unsigned int>(nPNGwidth), 
                // static_cast<unsigned int>(nPNGheight),
                oVRCpng_data.pData,
                static_cast<int>(oVRCpng_data.current),
                osBaseLabel,
                nEnvPNGDump
            );
    }

    // return to start of buffer
    // ... but skip the png magic - see below.
    oVRCpng_data.current = sizeof(PNG_sig);

    // The first eight, png-identifying, magic, bytes of the PNG file
    // are not present in the VRC data; we tell libpng to skip them.
    png_set_sig_bytes(png_ptr, static_cast<int>(oVRCpng_data.current));

    /*******************************************************************/

    // if (color_type == PNG_COLOR_TYPE_PALETTE)
    //     png_set_palette_to_rgb(png_ptr);
    //
    // if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
    //     png_set_expand_gray_1_2_4_to_8(png_ptr);
    //

    // png_set_read_fn( png_ptr, info_ptr, VRC_png_read_data_fn );
    png_set_read_fn( png_ptr, &oVRCpng_data, VRC_png_read_data_fn );

    CPLDebug("Viewranger PNG",
             "oVRCpng_data %p (%p %ld %ld)",
             &oVRCpng_data,
             oVRCpng_data.pData,
             oVRCpng_data.length, oVRCpng_data.current
             );

    // May wish to change this so that the result is RGBA (currently RGB)
    png_read_png( png_ptr, info_ptr,
#if PNG_LIBPNG_VER > 10504
                  PNG_TRANSFORM_SCALE_16 |
#else
                  PNG_TRANSFORM_STRIP_16 |
#endif
                  PNG_TRANSFORM_STRIP_ALPHA |
                  PNG_TRANSFORM_PACKING |
                  PNG_TRANSFORM_GRAY_TO_RGB |
                  PNG_TRANSFORM_EXPAND,
                  nullptr);

    CPLDebug("Viewranger PNG",
             "read oVRCpng_data %p (%p %ld %ld) to %p",
             &oVRCpng_data,
             oVRCpng_data.pData,
             oVRCpng_data.length, oVRCpng_data.current,
             pbyPNGbuffer
             );

    png_free(png_ptr, oVRCpng_data.pData);

    png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);


    oVRCpng_data.pData = nullptr;
    oVRCpng_data.current = oVRCpng_data.length = 0;

    return pbyPNGbuffer;

}  // VRCRasterBand::read_PNG

/************************************************************************/
/*                          GDALRegister_VRC()                          */
/************************************************************************/

extern "C" void CPL_DLL GDALRegister_VRC()

{
    if (! GDAL_CHECK_VERSION("ViewrangerVRC"))
        return;

    if( GDALGetDriverByName( "ViewrangerVRC" ) == nullptr )
    {
        auto*poDriver = new GDALDriver();
        if (poDriver==nullptr) {
            CPLError( CE_Failure, CPLE_ObjectNull,
                      "Could not build a driver for VRC"
                     );
            return;
        }

        poDriver->SetDescription( "ViewrangerVRC" );

        // required in gdal version 2, not supported in gdal 1.11
#if GDAL_VERSION_MAJOR >= 2
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
#endif

        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "ViewRanger (.VRC)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "frmt_various.html#VRC" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "VRC" );

        // poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Byte Int16" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        // See https://gdal.org/development/rfc/rfc34_license_policy.html
        poDriver->SetMetadataItem( "LICENSE_POLICY", "NONRECIPROCAL" );

        // Which of these is correct ?
        // poDriver->SetMetadataItem(GDALMD_AREA_OR_POINT, GDALMD_AOP_POINT);
        poDriver->SetMetadataItem(GDALMD_AREA_OR_POINT, GDALMD_AOP_AREA);
        // GDALMD_AOP_AREA is the GDAL default.

        // poDriver->SetMetadataItem( "INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE" );

        poDriver->pfnOpen = VRCDataset::Open;
        poDriver->pfnIdentify = VRCDataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}   // GDALRegister_VRC()

// -------------------------------------------------------------------------

/************************************************************************/
/*                         EstablishOverviews()                         */
/*                                                                      */
/*      Delayed population of overview information.                     */
/************************************************************************/
#if 0
void VRCRasterBand::EstablishOverviews()
{
    CPLDebug("VRC","EstablishOverviews()");
    
    if( nOverviewCount != -1 )
        return;

#if 0
    nOverviewCount = GetOverviewCount(hVRC, nBand);
#else
    nOverviewCount = 0;
#endif
    if( nOverviewCount > 0 )
    {
        papoOverviewBands = static_cast<VRCRasterBand **>
            (CPLMalloc(sizeof(void*) * static_cast<size_t>(nOverviewCount)) );

        for( int iOvIndex = 0; iOvIndex < nOverviewCount; iOvIndex++ )
        {
            papoOverviewBands[iOvIndex] =
                new VRCRasterBand(static_cast<VRCDataset *>(poDS), nBand, iOvIndex);
            if( papoOverviewBands[iOvIndex]->GetXSize() == 0 )
            {
                delete papoOverviewBands[iOvIndex];
                papoOverviewBands[iOvIndex] = nullptr;
            }
        }
    }
} // VRCRasterBand::EstablishOverviews
#endif // 0

int VRCRasterBand::GetOverviewCount()
{    
    auto*poVRCDS = static_cast<VRCDataset*>(poDS);
    if (poVRCDS == nullptr) {
        CPLDebug("VRC",
                 "%p->GetOverviewCount() - band has no dataset",
                 this);
        return 0;
    }

#if defined VRC36_PIXEL_IS_FILE || defined VRC36_PIXEL_IS_TILE
    if (poVRCDS->nMagic == vrc_magic_thirtysix) {
        return 0;
    }
#endif // VRC36_PIXEL_IS_ not _PIXEL

#if 0 // prune overview list
    int nRet = nOverviewCount;
    CPLDebug("Viewranger OVRV",
             "VRCRasterBand::GetOverviewCount(nOverviewCount=%d papoOverviewBands=%p)",
             nOverviewCount, papoOverviewBands);
    if (nRet>0 && papoOverviewBands) {
        while (nRet>0 && papoOverviewBands[nRet]==nullptr) {
            --nRet;
        }
    }
    if (nRet<=0) return 0;
    CPLDebug("Viewranger OVRV",
             "VRCRasterBand::GetOverviewCount(nOverviewCount=%d papoOverviewBands=%p) returns %d",
             nOverviewCount, papoOverviewBands, nRet-1);
    return nRet-1;
#endif // prune overview list

    auto* poFullBand =
        static_cast<VRCRasterBand*>(poVRCDS->GetRasterBand(nBand));
    if (nullptr==poFullBand) {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s %p->GetOverviewCount() band %d but dataset %p has no such band",
                 poVRCDS->sLongTitle.c_str(),
                 this, nBand, poVRCDS
                 );
        return 0;
    }
    if (this==poFullBand) {
        CPLDebug("Viewranger OVRV",
                 "%s band %p is a parent band with %d overviews at %p",
                 poVRCDS->sLongTitle.c_str(),
                 this, poFullBand->nOverviewCount,
                 poFullBand->papoOverviewBands);
        if (nOverviewCount != poFullBand->nOverviewCount) {
            // This cannot happen ?
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s %p==%p but overview count %d != %d",
                 poVRCDS->sLongTitle.c_str(),
                      this, poFullBand,
                      nOverviewCount, poFullBand->nOverviewCount);
        }
    } else {
        CPLDebug("Viewranger OVRV",
                 "%s band %p has %d overviews at %p; its parent %p has %d overviews at %p",
                 poVRCDS->sLongTitle.c_str(),
                 this, nOverviewCount, papoOverviewBands,
                 poFullBand, poFullBand->nOverviewCount, poFullBand->papoOverviewBands
                 );
    }
    
    if (poFullBand->papoOverviewBands) {
        return poFullBand->nOverviewCount;
    } else {    
        return 0;
    }
    
#if 0 // old
    if (this==poFullBand) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "cannot find overview tables for full band %p",
                  this);
        return 0;
    }

    int nRet = nOverviewCount;
    
    if (0==nRet) {
        nRet = poFullBand -> nOverviewCount;
        if ( papoOverviewBands != nullptr &&
             papoOverviewBands != poFullBand->papoOverviewBands ) {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "overview tables for band %p and full band %p differ",
                      this, poFullBand);
            return 0;
        }

        // Should we return 0 for any child overview band ?
        // return 0;
    }
    if (nRet>32) {
        // silly
        // something has gone wrong
        CPLError( CE_Failure, CPLE_AppDefined,
             "%s %p->VRCRasterBand::GetOverviewCount() nOverviewCount=%d is silly. papoOverviewBands=%p Returning 0",
                  poVRCDS->sLongTitle.c_str(),
                  this, nOverviewCount, papoOverviewBands);
        return 0;
    }

    // This should not be needed
    // but we get called with "this" equal to one of the overview bands
    // (this may be our fault)
    // and we don't want GDALRasterBand::RasterIO() and
    // GDALRasterBand::IRasterIO() calling each other.
    for (int i=0; i<nOverviewCount; i++) {
        if(this==poFullBand->papoOverviewBands[i]) {
            if (i==nThisOverview) {
                CPLDebug("VRC",
                         "Found band %p as overview %d",
                         this, i );
            } else {
                static int nErrCount=0;
                nErrCount++;
                nRet=0;
                CPLError( CE_Failure, CPLE_AppDefined,
                          "%p->VRCRasterBand::GetOverviewCount(nOverviewCount=%d papoOverviewBands=%p) returns band %d - itself ! (%d such errors)",
                          this, nOverviewCount, papoOverviewBands, i,
                          nErrCount);
                break;
            }
        }
    }
    CPLDebug("Viewranger OVRV",
             "%p->VRCRasterBand::GetOverviewCount(nOverviewCount=%d papoOverviewBands=%p) returns %d",
             this, nOverviewCount, poFullBand->papoOverviewBands, nRet);
    return nRet;
#endif // 0 // old
} // VRCRasterBand::GetOverviewCount

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *VRCRasterBand::GetOverview( int iOverviewIn )
{
    auto* poVRCDS = static_cast<VRCDataset*>(poDS);
    if (poVRCDS == nullptr) {
        CPLDebug("VRC",
                 "%p->GetOverview(%d) - band has no dataset",
                 this, iOverviewIn);
        return nullptr;
    }
#if defined VRC36_PIXEL_IS_FILE || defined VRC36_PIXEL_IS_TILE
    if (poVRCDS->nMagic == vrc_magic_thirtysix) {
        return nullptr;
    }
#endif // VRC36_PIXEL_IS_ not _PIXEL
    
    auto* poFullBand =
        static_cast<VRCRasterBand*>(poVRCDS->GetRasterBand(nBand));
    if (poFullBand == nullptr) {
        CPLDebug("VRC",
                 "%p->GetOverview(%d) - dataset %p has no band %d",
                 this, iOverviewIn,poVRCDS, nBand );
        return nullptr;
    }

    // Short circuit the sanity checks in this case.
    if (iOverviewIn==poFullBand->nThisOverview) {
        CPLDebug("VRC", "%p->GetOverview(%d) is itself",
                 poFullBand, iOverviewIn);
        return poFullBand;
    }
  
    if( nOverviewCount>32) {
        CPLDebug("Viewranger",
                 "nBand %d requested overview %d of %d: more than 32 is silly - something has gone wrong",
                 nBand, iOverviewIn, nOverviewCount);
        // This *should* cause it to be regenerated if required.
        nOverviewCount=-1;
        return nullptr;
    }
    if( nOverviewCount<-1) {
        CPLDebug("Viewranger",
                 "nBand %d has %d overviews, but overview %d requested - something has gone wrong",
                 nBand, nOverviewCount, iOverviewIn);
        nOverviewCount=-1; // This should cause it to be regenerated if required.
        return nullptr;
    }
    if( iOverviewIn < 0 || iOverviewIn >= poFullBand->nOverviewCount) {
        CPLDebug("Viewranger",
                 "nBand %d expected 0<= iOverviewIn %d < nOverviewCount %d",
                 nBand, iOverviewIn, poFullBand->nOverviewCount);
        return nullptr;
    }
    if( iOverviewIn>32) {
        // We should not get here
        CPLDebug("Viewranger",
                 "nBand %d overview %d requested: more than 32 is silly",
                 nBand, iOverviewIn);
        return nullptr;
    }
    if(poFullBand->papoOverviewBands==nullptr) {
        // Seen Fri 24 Jul 2020 and Thu 21 Jan 2021
        // CPLDebug("Viewranger",
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%p->GetOverview(%d) nBand %d - no overviews but count is %d :-(",
                  this, iOverviewIn, nBand, nOverviewCount);
        return nullptr;
    } else {
        VRCRasterBand* pThisOverview
            = poFullBand->papoOverviewBands[iOverviewIn];
        CPLDebug("Viewranger",
                 "GetOverview(%d) nBand %d - returns %d x %d overview %p (overview count is %d)",
                 iOverviewIn, nBand,
                 pThisOverview->nRasterXSize,
                 pThisOverview->nRasterYSize,
                 pThisOverview, nOverviewCount);
        if (this==pThisOverview) {
            static int nCount=0;
            nCount++;
            CPLDebug("VRC",
             "%p->VRCRasterBand::GetOverview(%d) returns itself - called %d times",
                 this, iOverviewIn, nCount);
#if 0
            if (nCount>1000) {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "%p->VRCRasterBand::GetOverview(%d) returns itself - aborting",
                          this,iOverviewIn);
                exit(0);
            }
#endif
        }
        return pThisOverview;
    }
} // VRCRasterBand::GetOverview

extern void
dumpTileHeaderData(
                   VSILFILE *fp,
                   unsigned int nTileIndex,
                   unsigned int nOverviewCount,
                   unsigned int anTileOverviewIndex[],
                   const int tile_xx, const int tile_yy )
{
    if (fp==nullptr || anTileOverviewIndex==nullptr) {return;}
    vsi_l_offset byteOffset = VSIFTellL(fp);
    if (nOverviewCount!=7) {
        CPLDebug("Viewranger",
                 "tile (%d %d) header at x%x: %d - not seven",
                 tile_xx, tile_yy, nTileIndex, nOverviewCount);
        // CPLDebug does not "use" values
        (void)tile_xx;
        (void)tile_yy;
    }
    if ( VSIFSeekL( fp, nTileIndex, SEEK_SET ) ) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "dumpTileHeaderData cannot seek to nTileIndex %u=x%08ux",
                  nTileIndex, nTileIndex);
    }
    for (unsigned int i=0; i<nOverviewCount; i++) {
        unsigned int a=anTileOverviewIndex[i];
        if (0==a) {
            CPLDebug("Viewranger",
                     "\tanTileOverviewIndex[%d] =x%08x",
                     i, a);
        } else {
            int nXcount = VRReadInt(fp, a);
            int nYcount = VRReadInt(fp, a+4);
            int nXsize = VRReadInt(fp, a+8);
            int nYsize = VRReadInt(fp, a+12);
            CPLDebug("Viewranger",
                     "\ttile(%d,%d) anTileOverviewIndex[%d]=x%08x %dx%d tiles each %dx%d pixels",
                     tile_xx, tile_yy,
                     i, a, nXcount, nYcount, nXsize, nYsize);
            // CPLDebug does not "use" values
            (void)nXcount;
            (void)nYcount;
            (void)nXsize;
            (void)nYsize;
        }
    }
    if ( VSIFSeekL( fp, byteOffset, SEEK_SET ) ) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "dumpTileHeaderData cannot return file pointer to VRC byteOffset %d=x%08x",
                  static_cast<int>(byteOffset),
                  static_cast<int>(byteOffset));
    }
} // dumpTileHeaderData()

/* -------------------------------------------------------------------------
 * -------------------------------------------------------------------------
 *                VRCRasterBand::read_VRC_Tile_Metres()
 * -------------------------------------------------------------------------
 * -------------------------------------------------------------------------
 */
void
VRCRasterBand::read_VRC_Tile_Metres(VSILFILE *fp,
                             int block_xx, int block_yy,
                             void *pImage)
{

    if (block_xx < 0 || block_xx >= static_cast<VRCDataset *>(poDS)->nRasterXSize ) {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "read_VRC_Tile_Metres invalid row %d", block_xx );
        return ;
    }
    if (block_yy < 0 || block_yy >= static_cast<VRCDataset *>(poDS)->nRasterYSize ) {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "read_VRC_Tile_Metres invalid column %d", block_yy );
        return ;
    }
    if (pImage == nullptr ) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "read_VRC_Tile_Metres passed no image" );
        return ;
    }
    if (static_cast<VRCDataset *>(poDS)->nMagic != vrc_magic_metres) {
        // Second "if" will be temporary
        // if we can read "thirtysix" file data at the subtile/block level.
        if (static_cast<VRCDataset *>(poDS)->nMagic != vrc_magic_thirtysix) {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "read_VRC_Tile_Metres called with wrong magic number x%08x",
                      static_cast<VRCDataset *>(poDS)->nMagic );
            return ;
        }
    }

    CPLDebug("Viewranger", "read_VRC_Tile_Metres(%p, %d, %d, %p) band %d overview %d",
             fp, block_xx, block_yy, pImage, nBand, nThisOverview
             );


    // int tilenum = static_cast<VRCDataset *>(poDS)->nRasterXSize * block_xx + block_yy;
    // int tilenum = nBlockYSize * block_xx + block_yy;
    // int tilenum = static_cast<VRCDataset *>(poDS)->tileYcount * block_xx + block_yy;
    int tilenum = static_cast<VRCDataset *>(poDS)->tileXcount * block_yy + block_xx;

    unsigned int nTileIndex = static_cast<VRCDataset *>(poDS)->anTileIndex[tilenum];
    // CPLDebug("Viewranger", "vrcmetres_pixel_is_pixel");
    CPLDebug("Viewranger", "\tblock %d x %d, (%d, %d) tilenum %d tileIndex x%08x",
             nBlockXSize,
             nBlockYSize,
             block_xx, block_yy,
             tilenum,
             nTileIndex
             );

    // Write nodata to the canvas before we start reading
    if (eDataType==GDT_Byte) {
        for (int j=0; j < nBlockYSize ; j++) {
            for (int i=0; i < nBlockXSize ; i++) {
                int pixelnum = j * nBlockXSize + i;
                if (nBand==4) {
                    static_cast<GByte *>(pImage)[pixelnum] = 255 ; // alpha: opaque
                } else {
                    static_cast<GByte *>(pImage)[pixelnum] =
#if 0
                        static_cast<GByte>((i+j) % 256);
#else
                        nVRCNoData;
#endif
                }
                // ((GByte *) pImage)[pixelnum] = nVRCNoData;
            }
        }
    } else {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "VRCRasterBand::read_VRC_Tile_Metres eDataType %d unexpected for null tile",
                  eDataType);
    }
    
    if (nTileIndex==0) {
        // No data for this tile
        CPLDebug("Viewranger",
                 "VRCRasterBand::read_VRC_Tile_Metres(.. %d %d ..) null tile",
                 block_xx, block_yy );

        return;
    }  // nTileIndex==0 No data for this tile

    if (nTileIndex >= static_cast<VRCDataset *>(poDS)->oStatBufL.st_size) {
        // No data for this tile
        CPLDebug("Viewranger",
                 "VRCRasterBand::read_VRC_Tile_Metres(.. %d %d ..) tileIndex %d beyond end of file",
                 block_xx, block_yy, nTileIndex);
        return;
    }  // nTileIndex >= oStatBufL.st_size

    if ( VSIFSeekL( fp, nTileIndex, SEEK_SET ) ) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "cannot seek to tile header x%08x", nTileIndex );
        return;
    }

    nOverviewCount = VRReadInt(fp);
    if (nOverviewCount != 7) {
        CPLDebug("Viewranger OVRV", "read_VRC_Tile_Metres: nOverviewCount is %d - expected seven - MapID %d",
                 nOverviewCount,
                 static_cast<VRCDataset *>(poDS)->nMapID
                 );
        return;
    }
    if (static_cast<unsigned int>(nOverviewCount) > static_cast<VRCDataset*>(poDS)->nMaxOverviewCount) {
        CPLDebug("Viewranger OVRV", "read_VRC_Tile_Metres: reducing nOverviewCount %d to %d (from tile sizes %d - %d)",
                 nOverviewCount,
                 static_cast<VRCDataset *>(poDS)->nMaxOverviewCount,
                 static_cast<VRCDataset *>(poDS)->tileSizeMax,
                 static_cast<VRCDataset *>(poDS)->tileSizeMin
                 );
        // nOverviewCount = static_cast<int>(static_cast<VRCDataset *>(poDS)->nMaxOverviewCount);
    }

    unsigned int anTileOverviewIndex[7]={};
    for (int ii=0; ii<std::min(7,nOverviewCount); ii++) {
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

    // here 22 Aug, 2020
    
    // This is just for the developer's understanding.
    if (0x20 + nTileIndex == anTileOverviewIndex[0]) {
        CPLDebug("Viewranger OVRV",
                 "anTileOverviewIndex[0] %d x%08x - 0x20 = %d x%08x as expected",
                 anTileOverviewIndex[0], anTileOverviewIndex[0],
                 nTileIndex, nTileIndex);
    } else {
        CPLDebug("Viewranger OVRV",
                 "anTileOverviewIndex[0] %d x%08x - nTileIndex %d x%08x = %d x%08x - expected 0x20",
                 anTileOverviewIndex[0], anTileOverviewIndex[0],
                 nTileIndex,           nTileIndex,
                 anTileOverviewIndex[0] - nTileIndex,
                 anTileOverviewIndex[0] - nTileIndex);
    }

    dumpTileHeaderData(fp, nTileIndex,
                       1+static_cast<unsigned int>(nOverviewCount),
                       anTileOverviewIndex, block_xx, block_yy );

    if (nThisOverview < -1 || nThisOverview >= nOverviewCount) {
        CPLDebug("Viewranger OVRV",
                 "read_VRC_Tile_Metres: overview %d=x%08x not in range [-1, %d)",
                 nThisOverview, nThisOverview, nOverviewCount);
        return;
    }

    if (anTileOverviewIndex[nThisOverview+1] >= static_cast<VRCDataset *>(poDS)->oStatBufL.st_size) {
        CPLDebug("Viewranger OVRV",
                 "\toverview level %d data at x%08x is beyond end of file",
                 nThisOverview, anTileOverviewIndex[nThisOverview+1] );
        return ;
    }
    CPLDebug("Viewranger OVRV",
             "\toverview level %d data at x%08x",
             nThisOverview, anTileOverviewIndex[nThisOverview+1] );

    bool bTileShrink = (anTileOverviewIndex[nThisOverview+1]==0);
    unsigned int nShrinkFactor=1;
    if (bTileShrink == false) {
        nShrinkFactor = 1;
        if ( VSIFSeekL( fp, anTileOverviewIndex[nThisOverview+1], SEEK_SET ) ) {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "cannot seek to overview level %d data at x%08x",
                      nThisOverview, anTileOverviewIndex[nThisOverview+1] );
            return;
        }

        unsigned int nTileMax = static_cast<VRCDataset *>(poDS)->tileSizeMax;
        unsigned int nTileMin = static_cast<VRCDataset *>(poDS)->tileSizeMin;
        int bits = 63 - __builtin_clzll
            (static_cast<unsigned long long>(nTileMax/nTileMin) );

        if (nTileMax == nTileMin << bits) {
            CPLDebug("Viewranger OVRV",
                     "%d / %d == %f == 2^%d",
                     nTileMax, nTileMin,
                     1.0*nTileMax/nTileMin, bits);
        } else {
            CPLDebug("Viewranger OVRV",
                     "%d / %d == %f != 2^%d",
                     nTileMax, nTileMin,
                     1.0*nTileMax/nTileMin, bits);
        }

        CPLDebug("Viewranger OVRV",
                 "\tblock %d x %d, max %d min %d overview %d",
                 nBlockXSize,
                 nBlockYSize,
                 nTileMax, nTileMin,
                 nThisOverview
                 );
        
    } else { // bTileShrink == true;
        // Data for this block is not available
        // so we need to rescale another overview.
        if(anTileOverviewIndex[nThisOverview]==0) {
            CPLDebug("Viewranger OVRV",
                     "Band %d block %d,%d overviews %d and %d empty - cannot shrink one to get other\n",
                     nBand, block_xx, block_yy,
                     nThisOverview-1, nThisOverview
                     );
            return;
        }

        nShrinkFactor = 2;

        CPLDebug("Viewranger OVRV",
                 "Band %d block %d,%d empty at overview %d\n",
                 nBand, block_xx, block_yy, nThisOverview
                 );
        CPLDebug("Viewranger OVRV",
                 "\t overview %d at x%08x\n",
                 nThisOverview-1, anTileOverviewIndex[nThisOverview]
                 );

        if ( VSIFSeekL( fp, anTileOverviewIndex[nThisOverview], SEEK_SET ) ) {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "cannot seek to overview level %d data at x%08x",
                      nThisOverview-1, anTileOverviewIndex[nThisOverview] );
            return;
        }

        CPLDebug("Viewranger OVRV",
                 "Band %d block %d,%d overview %d will be downsampled",
                 nBand, block_xx, block_yy, nThisOverview
                 );
    } // end bTileShrink == true

    // We have reached the start of the tile
    // ... but it is split into (essentially .png file) subtiles
    unsigned int nPNGXcount = VRReadUInt(fp);
    unsigned int nPNGYcount = VRReadUInt(fp);
    unsigned int pngXsize  = VRReadUInt(fp);
    unsigned int pngYsize  = VRReadUInt(fp);

    if (nPNGXcount==0 || nPNGYcount==0) {
        CPLDebug("Viewranger",
                 "tilenum %d contains no subtiles (%u x %u)",
                 tilenum, nPNGXcount, nPNGYcount );
        return;
    }
    if (pngXsize==0||pngYsize==0) {
        CPLDebug("Viewranger",
                 "empty (%u x %u) subtile in tilenum %d",
                 pngXsize, pngYsize, tilenum );
        return;
    }
    auto nFullBlockXSize =
        static_cast<unsigned>(nBlockXSize) * nShrinkFactor;
    if ( nPNGXcount > nFullBlockXSize
         || pngXsize > nFullBlockXSize
         || nPNGXcount * pngXsize > nFullBlockXSize
         ) {
        CPLDebug("Viewranger",
                 "nPNGXcount %d x pngXsize %d too big > nBlockXSize %d * nShrinkFactor %d",
                 nPNGXcount, pngXsize, nBlockXSize, nShrinkFactor);
        // return;
    }
    auto nFullBlockYSize =
        static_cast<unsigned>(nBlockYSize) * nShrinkFactor;
    if ( nPNGYcount > nFullBlockYSize
         || pngYsize > nFullBlockYSize
         || nPNGYcount * pngYsize > nFullBlockYSize
         ) {
        CPLDebug("Viewranger",
                 "nPNGYcount %d x pngYsize %d too big > nBlockYSize %d * nShrinkFactor %d",
                 nPNGYcount, pngYsize, nBlockYSize, nShrinkFactor);
        // return;
    }

    CPLDebug("Viewranger",
             "ovrvw %d nPNGXcount %d nPNGYcount %d pngXsize %d pngYsize %d nShrinkFactor %d",
             nThisOverview,
             nPNGXcount, nPNGYcount,
             pngXsize, pngYsize,
             nShrinkFactor);

    // Read in this tile's index to png sub-tiles.
    std::vector<unsigned int> anPngIndex;
    anPngIndex.reserve(static_cast<size_t>(nPNGXcount)*nPNGYcount +1 );
    for (unsigned long loop=0;
         loop <= static_cast<unsigned long>(nPNGXcount)*nPNGYcount;
         loop++) {
        // <= because there is an extra entry
        //    pointing just passed the last png sub-tile.
        anPngIndex[loop] = VRReadUInt(fp);
        if (anPngIndex[loop] > static_cast<VRCDataset *>(poDS)->oStatBufL.st_size) {
            CPLDebug("Viewranger",
                     "Band %d ovrvw %d block [%d,%d] png image %lu at x%x is beyond EOF - is file truncated ?",
                     nBand, nThisOverview, block_xx, block_yy, loop, anPngIndex[loop] );
            anPngIndex[loop] = 0;
        }
    }

    // unsigned int nPNGplteIndex = nTileIndex + 0x20 + 0x10 +8
    //    + 4*(nPNGXcount*nPNGYcount+1);
    vsi_l_offset nPNGplteIndex = VSIFTellL(fp);
    CPLDebug("Viewranger",
             "nPNGplteIndex %llu=x%08llx",
             nPNGplteIndex, nPNGplteIndex );

    unsigned int VRCplteSize = VRReadUInt(fp);
    unsigned int PNGplteSize = PNGReadUInt(fp);
    if (VRCplteSize-PNGplteSize ==8) {
        if (PNGplteSize %3) {
            CPLDebug("Viewranger",
                     "ignoring palette: size %d=x%08x not a multiple of 3",
                     PNGplteSize, PNGplteSize );
            nPNGplteIndex = 0;
        } else {
            CPLDebug("Viewranger",
                     "palette %d=x%08x bytes, %d entries at %012llx",
                     PNGplteSize, PNGplteSize,
                     PNGplteSize/3, nPNGplteIndex);
        }
    } else {
        CPLDebug("Viewranger",
                 "ignoring palette at %lld=x%08llx: size mismatch %d=x%08x - %d=x%08x is %d=x%08x not 8",
                 nPNGplteIndex, nPNGplteIndex,
                 VRCplteSize,VRCplteSize, PNGplteSize,PNGplteSize,
                 VRCplteSize-PNGplteSize, VRCplteSize-PNGplteSize
                 );
        nPNGplteIndex = 0;
    }

    int nLeftCol=0;
    unsigned int nPrevPNGwidth=0;
    unsigned int nXlimit = MIN(nPNGXcount, nFullBlockXSize);
    unsigned int nYlimit = MIN(nPNGYcount, nFullBlockYSize);
    for (unsigned int loopX=0; loopX < nXlimit; loopX++) {
        int nRightCol=0;
        unsigned int nPrevPNGheight=0;
        int nBottomRow=nBlockYSize;
        // int nTopRow;

        // for (unsigned int loopY=nYlimit; loopY >= 0; loopY--) {
        // unsigned>=0 always true, so a standard for loop wont work:
        for (unsigned int loopY=nYlimit; loopY >= 1; /* see comments */ ) {
            // decrement at *start* of loop
            //so that we can have the last pass with loopY==0
            --loopY;
            
            unsigned int loop = (nYlimit-1-loopY) + loopX*nPNGYcount;

            unsigned int nHeader =
                anPngIndex[static_cast<unsigned>(loop)];
            unsigned int nextPngIndex =
                anPngIndex[static_cast<unsigned>(loop+1)];
            signed int nDataLen =
                static_cast<signed>(nextPngIndex) -
                static_cast<signed>(nHeader + 0x12);
            if (nHeader==0) {
                CPLDebug("Viewranger",
                         "block (%d,%d) tile (%d,%d) empty",
                         block_xx, block_yy, loopX, loopY);
                continue;
            }
            if (nDataLen<1) { // There should be a better/higher limit.
                CPLDebug("Viewranger PNG",
                         "block (%d,%d) tile (%d,%d) PNG data overflows - length %d",
                         block_xx, block_yy, loopX, loopY, nDataLen);
                continue ;
            }
            //unsigned int nPalette = nPNGplteIndex;

            if ( static_cast<VRCDataset *>(poDS)->nMagic == vrc_magic_metres) {
                unsigned int nPNGwidth=0;
                unsigned int nPNGheight=0;
                CPLDebug("Viewranger",
                         "calling read_PNG(%p .. %d %llu %d tile (%d %d) loop (%d %d))",
                         fp,
                         nHeader, nPNGplteIndex, nDataLen,
                         block_xx, block_yy,
                         loopX, loopY
                         );
                png_byte* pbyPNGbuffer =
                    read_PNG (
                              fp,
                              &nPNGwidth, &nPNGheight,
                              nHeader, nPNGplteIndex,
                              static_cast<unsigned int>(nDataLen),
                              block_xx, block_yy,
                              loopX, loopY
                              );
                if (pbyPNGbuffer) {
                    CPLDebug("Viewranger",
                             "read_PNG() returned %p: %d x %d tile",
                             pbyPNGbuffer, nPNGwidth, nPNGheight);
                    if (getenv("VRC_DUMP_TILE")) {
                        auto nEnvTile = static_cast<unsigned int>
                            (strtol(getenv("VRC_DUMP_TILE"),nullptr,10));
                        // Dump pbyPNGbuffer as .ppm, one for each band, they should be full-colour and the same.
                        CPLString osBaseLabel = CPLString().Printf
                            ("/tmp/werdna/vrc2tif/%s.%01d.%03d.%03d.%03d.%03d.%02ua",
                             // CPLGetBasename(poOpenInfo->pszFilename) doesn't quite work
                             static_cast<VRCDataset *>(poDS)->sLongTitle.c_str(),
                             nThisOverview, block_xx, block_yy,
                             loopX, loopY,
                             nBand);
                        dumpPPM(
                                static_cast<unsigned int>(nPNGwidth),
                                static_cast<unsigned int>(nPNGheight),
                                pbyPNGbuffer,
                                static_cast<unsigned int>(nPNGwidth),
                                osBaseLabel,
                                pixel,
                                nEnvTile
                                );
                    }

                    if (nPrevPNGwidth==0) {
                        nPrevPNGwidth=nPNGwidth;
                    } else if (nPNGwidth!=nPrevPNGwidth) {
                        CPLDebug("Viewranger",
                                 "PNG width %d different from previous tile %d in same column",
                                 nPNGwidth, nPrevPNGwidth);
                    }
    
                    if (nPrevPNGheight==0) {
                        nPrevPNGheight=nPNGheight;
                    } else if (nPrevPNGheight!=nPNGheight) {
                        CPLDebug("Viewranger",
                                 "PNG height %d different from previous tile %d in same row",
                                 nPNGheight, nPrevPNGheight);
                    }
    
                    nRightCol=nLeftCol;
                    int nTopRow=nBottomRow;
                    nRightCol += nPNGwidth/nShrinkFactor;
                    nTopRow -= nPNGheight/nShrinkFactor;

                    if (nPNGheight>=nFullBlockYSize) {
                        // single tile block
                        if (nTopRow<0) {
                            CPLDebug("Viewranger",
                                     "Single PNG high band toprow %d set to 0",
                                     nTopRow);
                            nTopRow=0;
                        }
                    }
                    if (nTopRow<0) {
                        CPLDebug("Viewranger",
                                 "%d tall PNG tile: top row %d above top of %d tall block",
                                 nPNGheight, nTopRow, nBlockYSize 
                                 );
                    }
                    
                    // Blank the top of the top tile if necessary
                    if (loopY==nYlimit-1) {
#if 0
                        int rowStartPixel =
                            nTopRow * std::max(nPNGwidth, nBlockXSize)
                            + nLeftCol;
#endif
                        auto *pGImage = static_cast<GByte*>(pImage);
                        for (int ii=nBlockYSize; ii<nTopRow; ii++) {
                            for (int jj=nLeftCol; jj<nRightCol; jj++) {
                                pGImage[jj] = // (ii+jj)%255;
                                    nVRCNoData;
                            }
                            pGImage += nBlockXSize;
                        } 
                    }
                    
                    int nCopyResult=0;
                    if(!bTileShrink){ // anTileOverviewIndex[nThisOverview+1]) {
                        CPLDebug("Viewranger",
                                 "Band %d: Copy_Tile_ (%d %d) into_Block (%d %d) [%d %d)x[%d %d)",
                                 nBand,
                                 loopX, loopY,
                                 block_xx, block_yy,
                                 nLeftCol, nRightCol,
                                 nTopRow, nBottomRow
                                 );
                        nCopyResult = Copy_Tile_into_Block
                            (static_cast<GByte*>(pbyPNGbuffer),
                             static_cast<int>(nPNGwidth),
                             static_cast<int>(nPNGheight),
                             nLeftCol,nRightCol,
                             nTopRow, nBottomRow,
                             pImage
                             // , nBlockXSize, nBlockYSize
                             );
                    } else {
                        CPLDebug("Viewranger",
                                 "Band %d: Shrink_Tile_ (%d %d) into_Block (%d %d) [%d %d)x[%d %d)",
                                 nBand,
                                 loopX, loopY,
                                 block_xx, block_yy,
                                 nLeftCol, nRightCol,
                                 nTopRow, nBottomRow
                                 );

                        nCopyResult = Shrink_Tile_into_Block
                            (static_cast<GByte*>(pbyPNGbuffer),
                             static_cast<int>(nPNGwidth),
                             static_cast<int>(nPNGheight),
                             nLeftCol,nRightCol,
                             nTopRow, nBottomRow,
                             pImage
                             // , nBlockXSize, nBlockYSize
                             );
                        CPLDebug("Viewranger",
                                 "\tShrink_Tile (%d %d) _into_Block (%d %d) returned %d",
                                 loopX, loopY,
                                 block_xx, block_yy,
                                 nCopyResult
                                 );
                    }

                    nBottomRow = nTopRow;
                    VSIFree(pbyPNGbuffer);
                    pbyPNGbuffer=nullptr;
                    if (nCopyResult) {
                        CPLDebug("Viewranger",
                                 "failed to copy/shrink tile to block"
                                 );
                    }
                } else {
                    // read_PNG returned nullptr
                    CPLDebug("Viewranger", "empty %d x %d tile ... prev was %d x %d",
                             nPNGwidth, nPNGheight, nPrevPNGwidth, nPrevPNGheight
                             );
                } // if (pbyPNGbuffer)
                CPLDebug("Viewranger",
                         "... read PNG tile (%d %d) overview %d block (%d %d) completed",
                         loopX, loopY,
                         nThisOverview,
                         block_xx, block_yy
                         );
                
            } else if (static_cast<VRCDataset *>(poDS)->nMagic == vrc_magic_thirtysix) {
#if 0
                CPLString osBaseLabel = CPLString().
                    Printf("/tmp/werdna/vrc2tif/%s.%01d.%03d.%03d.%03d.%03d.%08lu.%02u",
                           // CPLGetBasename(poOpenInfo->pszFilename) doesn't quite work
                           static_cast<VRCDataset *>(poDS)->sLongTitle.c_str(),
                           nThisOverview, block_xx, block_yy, loopX, loopY,
                           static_cast<long unsigned>(nHeader), nBand);
#endif
                int ret=
                    verifySubTileFile(fp,
                                      nHeader,
                                      nextPngIndex,
                                      block_xx, block_yy, loopX, loopY);
                if (0!=ret) {
                    CPLDebug("Viewranger RAW",
                             "verify tile (%d,%d) subtile (%d,%d) returned %d\n",
                             block_xx, block_yy, loopX, loopY, ret);
                }
            } else {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "We should not be here with magic=x%08x",
                         static_cast<VRCDataset *>(poDS)->nMagic);
                return;
            } // if (magic ...)
        } // for (loopY
        nLeftCol=nRightCol;
    } // for (loopX

#if 0
    // Blank a strip on the right of any under-wide tiles.
    int nSkipRightCols = nBlockXSize - nPNGXcount * pngXsize;
    if (nSkipRightCols > 0) {
        CPLDebug("Viewranger",
                 "Band %d ovrvw %d block [%d,%d] png image %d pixels (nSkipRightCols) underwide: pngXsize %d",
                 nBand, nThisOverview, block_xx, block_yy, nSkipRightCols,
                 pngXsize);
        //   Unlike the top edge, GDAL and VRC agree that
        // narrow tiles are at the left edge of the right-most tile.
        //   We don't need to adjust any sizes for this case...
        // ... but we may need to blank a strip at the right of the tile.
        for (int nY=0; nY<nBlockYSize; nY++) {
            int nPix = nY * nBlockXSize +  nPNGXcount * pngXsize;
            for (int nX=0; nX<nSkipRightCols; nX++) {
                // July 8, 2020 - seeing whether this affects Slovenia 25% - no
                // ((unsigned char*)pImage)[nPix++] = 255; // 63*nBand; // nVRCNoData;
                ((unsigned char*)pImage)[nPix++] = nVRCNoData;
            }
        }
        CPLDebug("Viewranger",
                 "Band %d ovrvw %d block [%d,%d] png image %d pixels under-wide",
                 nBand, nThisOverview, block_xx, block_yy, nSkipRightCols);
    }
    // Should we set poDS->nRightSkipPix here ?
    CPLDebug("Viewranger",
             "VRCRasterBand::read_VRC_Tile_Metres: nRightSkipPix %d nSkipRightCols %d",
             poDS->nRightSkipPix, nSkipRightCols );
#endif
    
    if (getenv("VRC_DUMP_TILE")) {
        auto nPPMcount = static_cast<unsigned int>
            (strtol(getenv("VRC_DUMP_TILE"),nullptr,10));
        // Dump VRC tile as .pgm, one for each band, they will be monochrome.
        CPLString osBaseLabel
            = CPLString().Printf("/tmp/werdna/vrc2tif/%s.%01d.%03d.%03d.%02u",
                                 // CPLGetBasename(poOpenInfo->pszFilename) doesn't quite work
                                 static_cast<VRCDataset *>(poDS)->sLongTitle.c_str(),
                                 nThisOverview, block_xx, block_yy,
                                 nBand);

        dumpPPM(
                static_cast<unsigned int>(nBlockXSize),
                static_cast<unsigned int>(nBlockYSize),
                static_cast<unsigned char*>(pImage),
                static_cast<unsigned int>(nBlockXSize),
                osBaseLabel,
                band,
                nPPMcount
                );
    }

} // VRCRasterBand::read_VRC_Tile_Metres

int VRCRasterBand::Copy_Tile_into_Block
(
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
 )
{
    // Copy image data from buffer to band
    
    CPLDebug("Viewranger PNG",
             "Copy_Tile_into_Block(%p %d x %d -> [%d %d)x[%d %d) %p) band %d",
             pbyPNGbuffer,
             nPNGwidth,   nPNGheight,
             nLeftCol,    nRightCol,
             nTopRow,     nBottomRow,
             pImage,
             // nBlockXSize, nBlockYSize,
             nBand
             );

    int rowStartPixel =
        nTopRow * std::max(nPNGwidth, nBlockXSize)
        + nLeftCol;
    // Need to adjust if we have a short (underheight) tile.
    // werdna, 2020 July 09 done ? No.
    // What about underwide tiles/blocks ?
        

    // GByte *pGImage = &(((GByte *)(pImage))[rowStartPixel]);
    GByte *pGImage = (static_cast<GByte*>(pImage))+rowStartPixel;
    CPLDebug("Viewranger PNG",
             "VRC band %d ovrvw %d nTopRow %d rowStartPixel %d",
             nBand, nThisOverview,
             nTopRow, rowStartPixel
             );

    if (nPNGheight < nBlockYSize) {
        if (nTopRow+nPNGheight > nBlockYSize) {
            CPLDebug("Viewranger PNG",
                     "band %d overview %d nTopRow %d +nPNGheight %d > nRasterYSize %d",
                     nBand, nThisOverview,
                     nTopRow, nPNGheight, nRasterYSize);
            //VSIFree(pbyPNGbuffer);
            //continue;
        }
    }
                        
    CPLDebug("Viewranger PNG",
             "band %d overview %d copying to [%d %d) x [%d %d)",
             nBand, nThisOverview,
             nLeftCol, nRightCol, nTopRow, nBottomRow
             );
#if 0
    CPLDebug("Viewranger PNG",
             "band %d overview %d block %d %d prev %d %d, current %d %d",
             nBand, nThisOverview, block_xx, block_yy,
             nPrevPNGwidth, nPrevPNGheight, nPNGwidth, nPNGheight
             );
#endif
    
    int nCopyStopRow=std::min(nPNGheight,nBlockYSize-nTopRow);

    if (nBottomRow!=nCopyStopRow) {
        CPLDebug("Viewranger PNG",
                 "band %d overview %d nTopRow %d - nBottomRow %d != %d nCopyStopRow",
                 nBand, nThisOverview,
                 nTopRow, nBottomRow,
                 nCopyStopRow
             );
    }
    for (int ii=0; ii<nCopyStopRow; ii++) {
        long long nGImageOffset = pGImage-static_cast<GByte*>(pImage);
        
#if 1 // Noisy
        if (getenv("VRC_NOISY")) {
            CPLDebug("Viewranger PNG",
                     "band %d overview %d row %d: copying from %p = pbyPNGbuffer %p + %ld",
                     nBand, nThisOverview, ii, pbyPNGbuffer+nPNGwidth*ii,
                     pbyPNGbuffer, static_cast<long>(nPNGwidth)*ii
                     );
            CPLDebug("Viewranger PNG",
                     "band %d overview %d copying 1  to %p = pImage + %lld = pImage + %g*%d",
                     nBand, nThisOverview, pGImage, nGImageOffset,
                     static_cast<double>(nGImageOffset)/nRasterXSize, nRasterXSize
                         
                     );
            // Which of these is right (if any) ?
            CPLDebug("Viewranger PNG",
                     "band %d overview %d copying 2  to %p = pImage + %lld = pImage + %g*%d",
                     nBand, nThisOverview, pGImage, nGImageOffset,
                     static_cast<double>(nGImageOffset)/nBlockXSize, nBlockXSize
                     );
            }
#endif // Noisy
        if (nGImageOffset+nPNGwidth > nBlockXSize*nBlockYSize) {
            CPLDebug("Viewranger PNG",
                     "Bang: %lld+%d ?> %d = %d*%d",
                     nGImageOffset, nPNGwidth,
                     nBlockXSize*nBlockYSize,
                     nBlockXSize,nBlockYSize
                     );
        }
        // If nBlockXSize is not divisible by a sufficiently large power of two
        // then nPNGwidth*2^k may be slightly bigger than nBlockXSize
        int nCopyStopCol=std::min(nPNGwidth,nBlockXSize-nLeftCol);
        if (nRightCol!=nCopyStopCol) {
            CPLDebug("Viewranger PNG",
                     "stopping at col %d of %d",
                     nCopyStopCol, nRightCol
                     );
        }
#if 0 // Grey-scale
        for (int jj=0; jj<nCopyStopCol; jj++) {
            pGImage[jj] = (pbyPNGbuffer+nPNGwidth*ii)[jj];
        }
#else
        if (nBand==4) {
            for (int jj=0; jj<nCopyStopCol; jj++) {
                // pGImage[jj] = 255; // Opposite of nVRCNoData;
            }
        } else {
            for (int jj=0, jjj=nBand-1; jj<nCopyStopCol; jj++, jjj+=3) {
                unsigned char temp =
                    (pbyPNGbuffer+3*nPNGwidth*ii)[jjj];
#if 1 // Noisy
                if (getenv("VRC_NOISY")) {
                    CPLDebug("Viewranger PNG",
                             "pixel copy %d[%d] (%d) -> %lld[%d]",
                             3*nPNGwidth*ii, jjj,
                             temp,
                             nGImageOffset, jj);
                }
#endif // Noisy
                pGImage[jj] = temp;
            }
        }
#endif // ! Grey-scale

        pGImage += nBlockXSize;
    } // for ii < nCopyStopRow
    
    CPLDebug("Viewranger PNG",
             "copied PNG buffer %p %d x %d into pImage %p %d x %d",
             pbyPNGbuffer, nPNGwidth, nPNGheight,
             pImage, nRasterXSize, nRasterYSize
             );

#if 1 // defined VRCDUMPTILE
    if (getenv("VRC_DUMP_TILE")) {
        auto nPPMcount = static_cast<unsigned int>
            (strtol(getenv("VRC_DUMP_TILE"),nullptr,10));
        CPLString osBaseLabel = CPLString().Printf
            ("/tmp/werdna/vrc2tif/%s.%d.%01d.t%03d.l%03d.w%03d.h%03d",
             // CPLGetBasename(poOpenInfo->pszFilename) doesn't quite work
             static_cast<VRCDataset *>(poDS)->sLongTitle.c_str(),
             nBand, nThisOverview,
             nTopRow, nLeftCol,
             nPNGwidth, nPNGheight
             );
        
        dumpPPM(
                static_cast<unsigned>(nPNGwidth),
                static_cast<unsigned>(nPNGheight),
                static_cast<unsigned char*>(pImage)
                + nBlockXSize*nTopRow
                + nLeftCol
                ,
                static_cast<unsigned>(nBlockXSize), // nRasterXSize,
                osBaseLabel,
                band,
                nPPMcount
                );
    }
#endif // VRCDUMPTILE

    return 0;

} // VRCRasterBand::Copy_Tile_into_Block

int VRCRasterBand::Shrink_Tile_into_Block
(
 GByte* pbyPNGbuffer,
 int nPNGwidth,
 int nPNGheight,
 int nLeftCol,
 int nRightCol,
 int nTopRow,
 int nBottomRow,
 void* pImage
 // , int nBlockXSize,
 // int nBlockYSize
 )
{
    CPLDebug("Viewranger PNG",
             "Shrink_Tile_into_Block(%p %d x %d -> [%d %d)x[%d %d) %p [%d %d) )",
             pbyPNGbuffer,
             nPNGwidth,
             nPNGheight,
             nLeftCol,
             nRightCol,
             nTopRow,
             nBottomRow,
             pImage,
             nBlockXSize,
             nBlockYSize
             );
    
    if (nTopRow <0 || nTopRow>= nBlockYSize) {
            CPLDebug("Viewranger PNG",
                     "Shrink_Tile_into_Block: nTopRow %d not in [0,%d)",
                     nTopRow, nBlockYSize);
            //return -1;
    }
    if (nBottomRow < nTopRow || nBottomRow > nBlockYSize) {
            CPLDebug("Viewranger PNG",
             "Shrink_Tile_into_Block: nBottomRow %d not in [%d,%d)",
                     nBottomRow, nTopRow, nBlockYSize);
            //return -1;
    }

    if (nLeftCol <0 || nLeftCol>= nBlockXSize) {
            CPLDebug("Viewranger PNG",
                     "Shrink_Tile_into_Block: nLeftCol %d not in [0,%d)",
                     nLeftCol, nBlockXSize);
            //return -1;
    }
    if (nRightCol < nLeftCol || nRightCol > nBlockXSize ) {
            CPLDebug("Viewranger PNG",
             "Shrink_Tile_into_Block: nRightCol %d not in [%d,%d)",
                     nRightCol, nLeftCol, nBlockXSize);
            //return -1;
    }
    int nCopyStartCol=std::max(0,nLeftCol);
    int nCopyStartRow=std::max(0,nTopRow);
    // If nBlockXYSize is not divisible by a sufficiently large power
    // of two then nPNGwidthheight*2^k may be slightly bigger than nBlockXYSize
    int nCopyStopCol=
        std::min(nLeftCol+(nPNGwidth+1)/2,
                 std::min(nRightCol, nBlockYSize));
    int nCopyStopRow=std::min(nTopRow+(nPNGheight+1)/2, nBottomRow);

    // here 10 Feb 2021
        
    int nOutRowStartPixel =
        nCopyStartRow * nBlockXSize; // std::max((1+nPNGwidth)/2, nBlockXSize)
    // + nCopyStartCol;
    // Need to adjust if we have a short (underheight) tile.
    // werdna, 2020 July 09 done ? No.
    // What about underwide tiles/blocks ?
    CPLDebug("Viewranger PNG",
             "nOutRowStartPixel %d == %d * %d + %d",
             nOutRowStartPixel, nCopyStartRow, nBlockXSize, nCopyStartCol );
    CPLDebug("Viewranger PNG",
             "Shrink_Tile_into_Block: nOutRowStartPixel %d ii loops [%d/%d,%d/%d/%d)",
             nOutRowStartPixel,
             nTopRow,nCopyStartRow, nCopyStopRow,nBottomRow,nBlockYSize);
    CPLDebug("Viewranger PNG",
             "Shrink_Tile_into_Block: loopX-tile-adj missing jj loops [%d/%d,%d/%d/%d)",
             // need adjustment for loopX'th tile,
             nLeftCol,nCopyStartCol, nCopyStopCol,nRightCol,nBlockXSize);
        

    GByte *pGImage = static_cast<GByte *>(pImage)
        // need + adjust for loopX'th tile
        + nOutRowStartPixel;

#if 1
    {
        int i1=3*nPNGwidth*2*(nBottomRow-1-nCopyStartRow);
        // int i2=i1+3*nPNGwidth;
        int jjj=(nBand-1)+(nCopyStopCol-1-nCopyStartCol)*6;
        if (i1+jjj > 3*nPNGwidth*nPNGheight - 16) {
            CPLDebug("Viewranger PNG",
                     "Band %d: i1 %d = 3 * %d * 2 * %d",
                     nBand,
                     i1, nPNGwidth, nBottomRow-1-nCopyStartRow
                     );
            CPLDebug("Viewranger PNG",
                     "Band %d: jjj %d = %d + %d * 6",
                     nBand,
                     jjj, nBand-1, nCopyStopCol-1-nCopyStartCol
                     );
            CPLDebug("Viewranger PNG",
                     "Band %d: Shrink_Tile_into_Block: (i1+jjj %d+%d=%d) - 6*%d*%d = %d",
                     nBand,
                     i1, jjj, i1+jjj,
                     nPNGwidth,nPNGheight,
                     (i1+jjj)-(6*nPNGwidth*nPNGheight)
                     );
        }
    }
#endif

#if 0
    // ? 4 March, 2021 - found without ii - this is a guess
    // Do we need/want to blank the top of the block ?
    // No. one of the Sweden or Finland Overviews has bands which flash on and off with this code. Retire it.
    for (int ii=0; ii<nCopyStartRow; ii++) {
        for (int jj=nCopyStartCol; jj<nCopyStopCol; jj++) {
            pGImage[jj+nPNGwidth*ii] = nVRCNoData; // ? 4 March, 2021 - found without ii - this is a guess
        } // for jj
    } // for ii
#endif
    
    for (int ii=nCopyStartRow;
         ii< nCopyStopRow; // nBottomRow;
         ii++
         ) {
#if 1
        long long pixelOffset = pGImage-static_cast<GByte*>(pImage);
        long long nextOffset = pixelOffset - nBlockXSize*nBlockYSize;
        if (nextOffset+nCopyStopCol >=0) {
            CPLDebug("Viewranger PNG",
                     "Shrink_Tile_into_Block: %p pixelOffset %lld nextOffset %lld nCopyStopCol %d nxt+CSCol=%lld>=?0 (row %d<?%d)",
                     pGImage, pixelOffset,
                     nextOffset,nCopyStopCol,
                     nextOffset+nCopyStopCol,
                     ii, nBottomRow);
        }
#endif

#if 0 // Grey-scale
        for (int jj=nCopyStartCol; jj<nCopyStopCol; jj++) {
            pGImage[jj] = (pbyPNGbuffer+nPNGwidth*ii)[jj];
        }
#else
        if (nBand==4) {
            for (int jj=0; jj<nCopyStopCol; jj++) {
                // pGImage[jj] = 255; // Opposite of nVRCNoData;
            }
        } else {
            int i1=3*nPNGwidth*2*(ii-nCopyStartRow);
            int i2=i1+3*nPNGwidth;
            for (int jj=nCopyStartCol, jjj=nBand-1;
                 jj<nCopyStopCol;
                 jj++, jjj+=6) {
                unsigned short temp =
                    (pbyPNGbuffer+i1)[jjj];
                    temp += (pbyPNGbuffer+i2)[jjj];
                    temp += (pbyPNGbuffer+i1)[jjj+3];
                    temp += (pbyPNGbuffer+i2)[jjj+3];

                    pGImage[jj] = static_cast<GByte>(temp>>2);
            } // for jj,jjj
#endif // ! Grey-scale
        }
        pGImage += nBlockXSize;
    } // for ii < nCopyStopRow
    
    CPLDebug("Viewranger PNG",
             "shrunk PNG buffer %p %d x %d into pImage %p %d x %d within %d x %d",
             pbyPNGbuffer, nPNGwidth, nPNGheight,
             pImage, nBlockXSize, nBlockYSize, nRasterXSize, nRasterYSize
             );
         

    return 0;
} // VRCRasterBand::Shrink_Tile_into_Block

// #endif // def FRMT_vrc
