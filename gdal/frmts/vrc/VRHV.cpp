/******************************************************************************
 * $Id: VRHV.cpp,v 1.102 2021/06/26 20:28:41 werdna Exp werdna $
 *
 * Project:  GDAL
 * Purpose:  Viewranger GDAL Driver
 * Authors:  Andrew C Aitchison
 *
 ******************************************************************************
 * Copyright (c) 2015-9, Andrew C Aitchison
 ****************************************************************************/

/* -*- tab-width: 4 ; indent-tabs-mode: nil ; c-basic-offset 'tab-width -*- */

//#ifdef FRMT_vrc
//#ifdef FRMT_vrhv

#include "VRCutils.h"

CPL_CVSID("$Id: VRHV.cpp,v 1.102 2021/06/26 20:28:41 werdna Exp werdna $")

CPL_C_START
void   GDALRegister_VRHV(void);
CPL_C_END

#ifdef CODE_ANALYSIS

// Printing variables with CPLDebug can hide
// the fact that they are not otherwise used ...
#define CPLDebug(...)

#endif // CODE_ANALYSIS


static const unsigned int vrh_magic = 0xfac6804f; // 0x4f80c6fa; //
static const signed int nVRHNoData = -32768;

static const unsigned int vrv_magic = 0x2;

// ViewRanger Map Chooser (.vmc) file
// viewrangershop can read and write these files
// which describe the tiles selected from a VRV file 
static const unsigned int vmc_magic = 0x1;
static const unsigned int nVMCNoData = 0;
static const unsigned int nVMCYesData = 255;

static const unsigned int nVRNoData = 255;
static const unsigned int nVRVNoData = 255;

// typedef struct {double lat; double lon; } latlon;


/************************************************************************/
/* ==================================================================== */
/*                         VRHVDataset                                   */
/* ==================================================================== */
/************************************************************************/

class VRHRasterBand;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
class VRHVDataset : public GDALPamDataset
{
    friend class VRHRasterBand;
    
    VSILFILE            *fp;
    GDALColorTable      *poColorTable;
    GByte       abyHeader[0x5a0];
    
    unsigned int nMagic, nPixelMetres;
    int nVRHVersion;
    signed int nLeft, nRight, nTop, nBottom;
    unsigned int nScale;
    unsigned int *anColumnIndex;
    unsigned int *anTileIndex;
    short nCountry;
    OGRSpatialReference* poSRS = nullptr;

    char *pszLongTitle,
    // *pszName, *pszIdentifier,
    // *pszEdition, *pszRevision, *pszKeywords,
        *pszCopyright
        ;
    // *pszDepths, *pszHeights, *pszProjection,
    // *pszOrigFileName;
    // latlon TL, TR, BL, BR;

    std::string sDatum;
    
#pragma clang diagnostic pop

public:
    VRHVDataset();
    ~VRHVDataset() override;
    
    static GDALDataset *Open( GDALOpenInfo *poOpenInfo );
    static int Identify( GDALOpenInfo *poOpenInfo );
    
    // Gdal <3 uses proj.4, Gdal>=3 uses proj.6, see eg:
    // https://trac.osgeo.org/gdal/wiki/rfc73_proj6_wkt2_srsbarn
    // https://gdal.org/development/rfc/rfc73_proj6_wkt2_srsbarn.html
#if GDAL_VERSION_MAJOR >= 3
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }
    const char *_GetProjectionRef
#else
    const char *GetProjectionRef
#endif
    () override {
        return sDatum.c_str();
    }

#if 1 // GeoLoc
    CPLErr GetGeoTransform( double * padfTransform ) override;
#endif

    char       **GetFileList() override;

    static char *VRHGetString( VSILFILE *fp, unsigned long long byteaddr );
    // static void xy_to_latlon(VRHVDataset *poDS, int pixel_x, int pixel_y, latlon *latlon);
};


/* -------------------------------------------------------------------------
 * Returns a (null-terminated) string allocated from VSIMalloc.
 * The 32 bit length of the string is stored in file fp at byteaddr.
 * The string itself is stored immediately after its length;
 * it is *not* null-terminated in the file.
 * If index pointer is nul then an empty string is returned
 * (rather than a null pointer).
 */
char *VRHVDataset::VRHGetString( VSILFILE *fp, unsigned long long byteaddr )
{
    if (byteaddr==0) return( strdup (""));
    
    int seekres = VSIFSeekL( fp, byteaddr, SEEK_SET );
    if ( seekres ) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "cannot seek to VRH string" );
        return nullptr;
    }
    int string_length = VRReadInt(fp);
    if (string_length<=0) {
        if (string_length<0) {
            CPLDebug("ViewrangerHV", "odd length for string %012llx - length %d",
                     byteaddr, string_length);
        }
        return( strdup (""));
    }
    auto ustring_length = static_cast<size_t>(string_length);

    auto *pszNewString = static_cast<char*>(VSIMalloc(1+ustring_length));
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
      CPLDebug("ViewrangerHV", "requested x%08x bytes but only got x%8lx",
               string_length, bytesread);
      CPLError(CE_Failure, CPLE_AppDefined,
               "problem reading string\n");
      return nullptr;     
    }

    pszNewString[string_length] = 0;
    // CPLDebug("ViewrangerHV", "read string %s at %08x - length %d",
    //         pszNewString, byteaddr, string_length);
    return pszNewString;
}


/************************************************************************/
/* ==================================================================== */
/*                            VRHRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class VRHRasterBand : public GDALPamRasterBand
{
    friend class VRHVDataset;
    
    int          nRecordSize;
    
    GDALColorInterp  eBandInterp;
    signed short *pVRHVData;

    void read_VRH_Tile( VSILFILE *fp,
                                int tile_xx, int tile_yy,
                                void *pimage);
    void read_VRV_Tile( VSILFILE *fp,
                                int tile_xx, int tile_yy,
                                void *pimage);
    
    void read_VMC_Tile( VSILFILE *fp,
                                int tile_xx, int tile_yy,
                                void *pimage);
    
public:

    VRHRasterBand( VRHVDataset* poDSIn, int nBandIn, int iOverviewIn);
    ~VRHRasterBand() override;

    // virtual
    GDALColorInterp GetColorInterpretation() override;
    // virtual
    GDALColorTable  *GetColorTable() override;
    // virtual
    double GetNoDataValue( int* pbSuccess ) override;    
    // virtual
    CPLErr IReadBlock( int nBlockXOff,
                       int nBlockYOff,
                       void* pImage ) override;

};


/************************************************************************/
/*                           VRHRasterBand()                            */
/************************************************************************/

VRHRasterBand::VRHRasterBand(
                             VRHVDataset *poDSIn,
                             int nBandIn,
                             int iOverviewIn )
{
    this->poDS = poDSIn;
    (void)iOverviewIn;
    this->nBand = nBandIn;
    CPLDebug("ViewrangerHV", "VRHRasterBand(%p, %d, %d)",
             static_cast<void*>(poDS), nBand, iOverviewIn);
    
    nRasterXSize = poDSIn->nRasterXSize;
    nRasterYSize = poDSIn->nRasterYSize;

    // int tileXcount = poDS->tileXcount;
    // int tileYcount = poDS->tileYcount;

    CPLDebug("ViewrangerHV", "nRasterXSize %d nRasterYSize %d",
             nRasterXSize, nRasterYSize);

    CPLDebug("ViewrangerHV", "eBandInterp x%08x, (Blue=x%08x)",
             eBandInterp, GCI_BlueBand);

    pVRHVData = nullptr;

    if (poDSIn->nMagic == vrh_magic) {
        eDataType = GDT_Int16;
        eBandInterp = GCI_GrayIndex;

        /* Height data has an index of columns, so
         * we have one block per column.
         */
        nBlockXSize = 1;
        nBlockYSize = poDSIn->nRasterYSize;
        nRecordSize = /* nBlockXSize* */ nBlockYSize; // * sizeof(short) ?
    } else if (poDSIn->nMagic == vrv_magic || poDSIn->nMagic == vmc_magic) {
        eDataType = GDT_Byte;
        eBandInterp = GCI_GrayIndex;
        
        /* We currently make one block that stores the whole image.
         * We could make the blocks use fewer rows, but
         * since the data on file is west up we need to rotate it
         * to put into the block.
         * Since all known sample images are small, we don't
         * currently support multiple blocks.
         */
        
        nBlockXSize = nRasterXSize;
        nBlockYSize = nRasterYSize;
        
        /* Cannot overflow as nBlockXSize, nBlockYSize both <= 4096
         * in VRHVDataset::Open()
         */
        nRecordSize = nBlockXSize*nBlockYSize;
    }
    CPLDebug("ViewrangerHV", "eBandInterp x%08x, (Red==x%08x)",
             eBandInterp, GCI_RedBand);
} // VRHRasterBand()

/************************************************************************/
/*                          ~VRHRasterBand()                            */
/************************************************************************/

VRHRasterBand::~VRHRasterBand()
{
    if (pVRHVData) {
        VSIFree(pVRHVData);
        pVRHVData = nullptr;
    }
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr VRHRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )
{
    auto *poGDS = static_cast<VRHVDataset *>(poDS);

    CPLDebug("ViewrangerHV", "Block (%d,%d)", nBlockXOff, nBlockYOff);
    if (nBlockXOff < 0 || nBlockXOff*nBlockXSize >= poGDS->nRasterXSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Block (any,%d) does not exist %d * %d >?= %d",
                 nBlockXOff, nBlockXOff,
                 poGDS->nRasterXSize, nBlockXSize );
            return CE_Failure;
    }
    if (nBlockYOff < 0 || nBlockYOff*nBlockYSize >= poGDS->nRasterYSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Block (any,%d) does not exist %d * %d >?= %d",
                 nBlockYOff, nBlockYOff,
                 poGDS->nRasterYSize, nBlockYSize );
            return CE_Failure;
    }

#if 0
    if (pVRHVData == nullptr) {
        if (nRecordSize < 0)
            return CE_Failure;

        pVRHVData = static_cast<signed short*>(VSIMalloc(nRecordSize));
        if (pVRHVData == nullptr) {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Cannot allocate scanline buffer");
            nRecordSize = -1;
            return CE_Failure;
        }
    }
#endif

    if ( poGDS->nMagic == vrh_magic ) {
#if 0
        // dummy data for now
        // this draws a box
        for( int i = 0; i < nBlockXSize *nBlockYSize ; i++ ) {
            static_cast<signed short*>(pImage)[i] = nVRHNoData;
        }
        for( int i = 0; i < nBlockXSize ; i++ ) {
            static_cast<signed short*>(pImage)[i] = 0;
            static_cast<signed short*>(pImage)[nBlockXSize *nBlockYSize - i] = 0;
        }
        for( int i = 1; i < nBlockYSize-1 ; i++ ) {
            static_cast<signed short*>(pImage)[i*nBlockXSize] = 0;
            static_cast<signed short*>(pImage)[(i+1)*nBlockXSize-1] = 0;
        }
#endif

        if ( poGDS->anColumnIndex[nBlockXOff] ) {
            int seekres =
                VSIFSeekL( poGDS->fp, poGDS->anColumnIndex[nBlockXOff], SEEK_SET );
            if ( seekres ) {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "cannot seek to VRH column %d", nBlockXOff );
                return CE_Failure;
            }
            read_VRH_Tile(poGDS->fp, nBlockXOff, nBlockYOff, pImage);

        } else {
            // no data for this column;
            auto *panColumn = static_cast<short*>(pImage);
            for (int i=0; i < nBlockYSize; i++) {
                panColumn[i] = nVRHNoData;
            }
            return CE_None; // all OK
        }        
    } else  if ( poGDS->nMagic == vrv_magic ) {
        read_VRV_Tile(poGDS->fp, nBlockXOff, nBlockYOff, pImage);
#if 0
        char * charImage = (char *)pImage;
        CPLDebug("ViewrangerHV", "IReadBlock %p\t%02x %02x %02x %02x %02x %02x %02x %02x\t%02x %02x %02x %02x %02x %02x %02x %02x",
                 charImage,
                 charImage[0], charImage[1], charImage[2], charImage[3],
                 charImage[4], charImage[5], charImage[6], charImage[7],
                 charImage[8], charImage[9], charImage[10], charImage[11],
                 charImage[12], charImage[13], charImage[14], charImage[15]
                 );
#endif
        return CE_None; // all OK
        
    } else  if ( poGDS->nMagic == vmc_magic ) {
        read_VMC_Tile(poGDS->fp, nBlockXOff, nBlockYOff, pImage);
        CPLDebug("ViewrangerHV", "IReadBlock(%d, %d, %p)",
                 nBlockXOff, nBlockYOff, pImage);
        return CE_None; // all OK
        
    }
    return CE_None;
} // VRHRasterBand::IReadBlock

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double VRHRasterBand::GetNoDataValue( int* pbSuccess )
{
  if (pbSuccess) {
      *pbSuccess=TRUE;
  }

  switch (static_cast<VRHVDataset*>(poDS)->nMagic) {
  case vmc_magic: return nVMCNoData;
  case vrh_magic: return nVRHNoData;
  case vrv_magic: return nVRVNoData;
  default :
      if (pbSuccess) {
          *pbSuccess=FALSE;
      }
      return 0.0;
  }
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp VRHRasterBand::GetColorInterpretation()

{
    return GCI_GrayIndex;
}


/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *VRHRasterBand::GetColorTable()

{
#if 0
    VRHVDataset  *poGDS = (VRHVDataset *) poDS;

    if( nBand == 1 )
        return poGDS->poColorTable;
    else
#endif
        return nullptr;
}


/************************************************************************/
/* ==================================================================== */
/*                            VRHVDataset                                */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            VRHVDataset()                              */
/************************************************************************/

VRHVDataset::VRHVDataset() :
    fp(nullptr),
    poColorTable(nullptr),
    // These are only here to keep cppcheck happy
    // They may make the program think things are OK when they are not.
#ifdef CODE_ANALYSIS
    abyHeader{0},   // Inefficient since we never use the initial value.
    // iOverview(0),
    nMagic(0),
    nPixelMetres(0),
    nVRHVersion(-1),
#endif // CODE_ANALYSIS
    nLeft(INT_MAX),
    nRight(INT_MIN),
    nTop(INT_MIN),
    nBottom(INT_MAX),
#ifdef CODE_ANALYSIS
    nScale(0),
#endif // CODE_ANALYSIS
    anColumnIndex(nullptr),
    anTileIndex(nullptr),
#ifdef CODE_ANALYSIS
    nCountry(-1),
#endif // CODE_ANALYSIS
    poSRS(nullptr),

    pszLongTitle(nullptr),
    //pszName(nullptr),
    //pszIdentifier(nullptr),
    //pszEdition(nullptr),
    //pszRevision(nullptr),
    //pszKeywords(nullptr),
    pszCopyright(nullptr),
    //pszDepths(nullptr),
    //pszHeights(nullptr),
    //pszProjection(nullptr),
    //pszOrigFileName(nullptr),
    // These are only here to keep cppcheck happy
    // They may make the program think things are OK when they are not.

    sDatum(CPLStrdup(""))
{
}

/************************************************************************/
/*                           ~VRHVDataset()                             */
/************************************************************************/

VRHVDataset::~VRHVDataset()

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
    if (pszLongTitle != nullptr ) {
        VSIFree(pszLongTitle);
        pszLongTitle = nullptr;
    }
    if (pszCopyright != nullptr ) {
        VSIFree(pszCopyright);
        pszCopyright = nullptr;
    }
    if (poSRS) {
        poSRS->Release();
        poSRS = nullptr;
    }
#if 0
    if (sDatum != nullptr ) {
        VSIFree(sDatum);
        sDatum = nullptr;
    }

    // trying to free strings created in VRHVDataset::Open
    for (int ii=0; ii<nStringCount; ++ii) {
        if (paszStrings[ii]) {
            VSIFree(paszStrings[ii]);
            paszStrings[ii] = 0;
        }        
    }
    VSIFree(paszStrings);
    paszStrings = 0;
#endif
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/
#if 1 // GeoLoc
CPLErr VRHVDataset::GetGeoTransform( double * padfTransform )

{
    double dLeft = nLeft;
    double dRight = nRight;
    double dTop = nTop ;
    double dBottom = nBottom ;

    double tenMillion = 10.0 * 1000 * 1000;
     if (nCountry == 17) {
        // This may not be correct
        // USA, Discovery (Spain) and some Belgium (VRH height) maps have coordinate unit of
        //   1 degree/ten million
        CPLDebug("ViewrangerHV", "country/srs 17 USA?Belgium?Discovery(Spain) grid is unknown. Current guess is unlikely to be correct.");
        CPLDebug("ViewrangerHV", "raw position: TL: %d %d BR: %d %d",
                 nTop,nLeft,nBottom,nRight);
#if 1 // standard until 20 Sept 2020
        dLeft   /= tenMillion;
        dRight  /= tenMillion;
        dTop    /= tenMillion;
        dBottom /= tenMillion;
        CPLDebug("ViewrangerHV", "scaling by 10 million: TL: %g %g BR: %g %g",
                 dTop,dLeft,dBottom,dRight);
#else
        const double nineMillion = 9.0 * 1000 * 1000;
        dLeft   /= nineMillion;
        dRight  /= nineMillion;
        dTop    /= nineMillion;
        dBottom /= nineMillion;
        CPLDebug("ViewrangerHV", "scaling by 9 million: TL: %g %g BR: %g %g",
                 dTop,dLeft,dBottom,dRight);
#endif
    } else if (nCountry == 155) {
        // New South Wales srs is not quite GDA94/MGA55 EPSG:28355
        dLeft   = 1.0*nLeft;
        dRight  = 1.0*nRight;
        dTop    = 1.0*nTop + tenMillion;
        dBottom = 1.0*nBottom + tenMillion;
        CPLDebug("ViewrangerHV", "shifting by 10 million: TL: %g %g BR: %g %g",
                 dTop,dLeft,dBottom,dRight);
    }

    // Xgeo = padfTransform[0] + pixel*padfTransform[1] + line*padfTransform[2];
    // Ygeo = padfTransform[3] + pixel*padfTransform[4] + line*padfTransform[5];
    if (nMagic == vrh_magic || nMagic == vrv_magic || nMagic == vmc_magic) {
        padfTransform[0] = dLeft;
        padfTransform[1] = (1.0*dRight - dLeft) / (GetRasterXSize());
        padfTransform[2] = 0.0;
        padfTransform[3] = dTop;
        padfTransform[4] = 0.0;
        padfTransform[5] = (1.0*dBottom - dTop) / (GetRasterYSize());
    } else {
#if 0
        padfTransform[0] = dLeft;
        padfTransform[1] = (1.0*dRight - dLeft);
        padfTransform[2] = 0.0;
        padfTransform[3] = dTop;
        padfTransform[4] = 0.0;
        padfTransform[5] = (1.0*dBottom - dTop);
#endif
        CPLError( CE_Failure, CPLE_AppDefined,
                          "unknown magic %d", nMagic);
    }

    CPLDebug("ViewrangerHV", "padfTransform raster %d x %d", GetRasterXSize(), GetRasterYSize());
    CPLDebug("ViewrangerHV", "padfTransform %g %g %g", padfTransform[0], padfTransform[1], padfTransform[2]);
    CPLDebug("ViewrangerHV", "padfTransform %g %g %g", padfTransform[3], padfTransform[4], padfTransform[5]);
    return CE_None;
}
#endif

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int VRHVDataset::Identify( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*  This has to be a file on disk ending in .VRH, .VRV or .vmc          */
/*  case is probably not important, but this is what we get on linux    */
/* .VRH (but not all .VRV) files also have an obvious right magic number*/
/* -------------------------------------------------------------------- */

    // Need to read 
    // http://trac.osgeo.org/gdal/wiki/rfc11_fastidentify
    CPLDebug("ViewrangerHV", "VRHVDataset::Identify(%s) %d byte header available",
             poOpenInfo->pszFilename, poOpenInfo->nHeaderBytes
             );
 
    if(poOpenInfo->nHeaderBytes < 20 ) {
         return FALSE;
    }

    unsigned int magic = VRGetUInt(poOpenInfo->pabyHeader, 0);
    unsigned int version = VRGetUInt(poOpenInfo->pabyHeader, 4);

    // .VRH files can be very small and may not have a header
    if(magic != vrv_magic && magic != vmc_magic && magic != vrh_magic
       && poOpenInfo->nHeaderBytes < 0x60 ) {
        // http://lists.osgeo.org/pipermail/gdal-dev/2013-February/035530.html
        // suggests that file extension is a bad way to detect file format
        // but if the header is not present we don't have a choice

        if( EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"VRH") ||
            EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"vrh") )  {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "VRH identify given %d byte header - needs 0x60 (file %s)",
                      poOpenInfo->nHeaderBytes, poOpenInfo->pszFilename );
        }
        // Small file that doesn't have any magic and has wrong filename
        return FALSE;
    }

    if( magic == vrh_magic ) {
        CPLDebug("ViewrangerHV", "VRH file %s supported",
                 poOpenInfo->pszFilename);
        return TRUE;
    } else if( magic == vmc_magic ) {
        // This match could easily be accidental,
        // so we require the correct extension even though
        // http://lists.osgeo.org/pipermail/gdal-dev/2013-February/035530.html
        // suggests that file extension is a bad way to detect file format.
        if( ! EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"VMC") &&
            ! EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"vmc") ) {
            return FALSE;
        }

        if (version == 1 || version == 2) {
            CPLDebug("ViewrangerHV", ".vmc file %s support limited",
                     poOpenInfo->pszFilename);
            return TRUE;     
        } else {
            CPLDebug("ViewrangerHV", "unexpected vmc version %08x", version);
            return FALSE;
       }
    } else if( magic == vrv_magic ) {
        // should do more checks here; matching this magic could easily be accidental

        // http://lists.osgeo.org/pipermail/gdal-dev/2013-February/035530.html
        // suggests that file extension is a bad way to detect file format
        // but .VRV files can be very small so we may not have a choice.
        if( EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"VRV") ||
            EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"vrv") ) {
            CPLDebug("ViewrangerHV", "VRV file %s supported",
                     poOpenInfo->pszFilename);
            return TRUE;
        } else {
            CPLDebug("ViewrangerHV", "ignoring possible VRV file %s with unexpected extension",
                     poOpenInfo->pszFilename);
            return FALSE;
        }
    } else if( EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"VRH") ||
               EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"vrh") ) {
        // *some* .VRH files have no magic.
        // http://lists.osgeo.org/pipermail/gdal-dev/2013-February/035530.html
        // suggests that file extension is a bad way to detect file format
        // so use that plus some extra checks.
        // We need to be extra careful in case this is not in fact a VRH file

        CPLDebug("ViewrangerHV", "Doing extra checks for VRH file %s",
                 poOpenInfo->pszFilename);

        int nLeft = VRGetInt( poOpenInfo->pabyHeader, 0 );
        int nRight = VRGetInt( poOpenInfo->pabyHeader, 4 );
        int nBottom = VRGetInt( poOpenInfo->pabyHeader, 8 );
        int nTop = VRGetInt( poOpenInfo->pabyHeader, 12 );
        int nWidth = nRight - nLeft;
        int nHeight = nTop - nBottom;
        int nPixelMetres = VRGetInt( poOpenInfo->pabyHeader, 16 );
        const int hundmill = 100 * 1000 * 1000;

        CPLDebug("ViewrangerHV", "nLeft %d nRight %d nBottom %d nTop %d nWidth %d nHeight %d",
                 nLeft, nRight, nBottom, nTop, nWidth, nHeight);

        if (   nLeft   < -hundmill || nLeft   > hundmill
            || nRight  < -hundmill || nRight  > hundmill
            || nWidth  < -hundmill || nWidth  > hundmill 
            || nTop    < -hundmill || nTop    > hundmill 
            || nBottom < -hundmill || nBottom > hundmill 
            || nHeight < -hundmill || nHeight > hundmill
            || nPixelMetres <= 0   || nPixelMetres > 1000 * 1000 )
            {
                CPLDebug("ViewrangerHV", "%s failed extra checks for a .VRH file",
                 poOpenInfo->pszFilename);
                return FALSE;
            }
        CPLDebug("ViewrangerHV", "%s passes extra checks for a .VRH file",
                 poOpenInfo->pszFilename);
        return TRUE;
    }
    return FALSE;
} // VRHVDataset::Identify()

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *VRHVDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if (!Identify(poOpenInfo))
        return nullptr;

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
                  "The VRH driver does not support update access to existing"
                  " datasets.\n" );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    // unsigned int nVRHVersion;
    auto* poDS = new VRHVDataset();

#if GDAL_VERSION_MAJOR >= 2
    /* Borrow the file pointer from GDALOpenInfo* */
    poDS->fp = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;
#else
    poDS->fp = VSIFOpenL( poOpenInfo->pszFilename, "rb" );
    if (poDS->fp == nullptr)
    {
        delete poDS;
        return nullptr;
    }
#endif

/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */
    VSIFReadL( poDS->abyHeader, 1, 0x5a0, poDS->fp );

    poDS->nMagic = VRGetUInt(poDS->abyHeader, 0);
    poDS->nVRHVersion = VRGetInt(poDS->abyHeader, 4);
    
    if ( poDS->nMagic!=vrh_magic && poDS->nMagic!=vmc_magic) {
        if( EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"VRH") ||
            EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"vrh") )
            {
                // early .VRH files have no magic signature
                poDS->nMagic = vrh_magic;
                poDS->nVRHVersion = 0;
            }
    }

    switch (poDS->nMagic) {
    case vrh_magic:
        {
            // .VRH height file
            unsigned int vrh_header_offset=0;
            if (poDS->nVRHVersion < 2) {
                poDS->nCountry = 1;
            } else {
                vrh_header_offset = 10;
                poDS->nCountry = VRGetShort( poDS->abyHeader, 8 );
            }
            
            poDS->nLeft =
                VRGetInt( poDS->abyHeader, vrh_header_offset );
            poDS->nRight =
                VRGetInt( poDS->abyHeader, vrh_header_offset+4 );
            poDS->nBottom =
                VRGetInt( poDS->abyHeader, vrh_header_offset+8 );
            poDS->nTop =
                VRGetInt( poDS->abyHeader, vrh_header_offset+12 );

            poDS->nPixelMetres =
                VRGetUInt( poDS->abyHeader, vrh_header_offset+16 );

            if (poDS->nPixelMetres < 1) {
                CPLDebug("ViewrangerHV",
                         "Map with %d metre pixels is too large scale (detailed) for the current VRHV driver",
                         poDS->nPixelMetres);
            } else {
                // Should check that poDS->nRasterXSize and
                // poDS->nRasterXSize are integers ...
                auto dfPixelMetres = static_cast<double>(poDS->nPixelMetres);
                double dfRasterXSize =
                    static_cast<double>(poDS->nRight - poDS->nLeft) /
                    dfPixelMetres;
                poDS->nRasterXSize = static_cast<int>(dfRasterXSize);
                double dfRasterYSize =
                    static_cast<double>(poDS->nTop - poDS->nBottom) /
                    dfPixelMetres;
                poDS->nRasterYSize = static_cast<int>(dfRasterYSize);

                // cast to double to avoid overflow and loss of precision
                // eg  (10000*503316480)/327680000 = 15360
                //             but                 = 11 with 32bit ints.
                CPLDebug("ViewrangerHV", "Image %d x %d",
                         poDS->nRasterXSize, poDS->nRasterYSize
                         );
            }
            
            poDS->pszLongTitle = CPLStrdup("") ; // poOpenInfo->pszFilename might be good
            poDS->pszCopyright = CPLStrdup("") ;
            
            /*************************************************************/
            /*             Read index data from VRH file                 */
            /*************************************************************/
            int seekres =
                VSIFSeekL( poDS->fp, vrh_header_offset+20, SEEK_SET );
            if ( seekres ) {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "cannot seek to VRH column index" );
                return nullptr;
            }
            
            poDS->anColumnIndex = static_cast<unsigned int*>(
                VSIMalloc2(sizeof (unsigned int),
                           static_cast<size_t>(poDS->nRasterXSize)) );
            if (poDS->anColumnIndex == nullptr) {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                         "Cannot allocate %d bytes of memory for column index",
                         poDS->nRasterXSize);
                return nullptr;
            }
            for (int ii=0; ii<poDS->nRasterXSize; ii++) {
                poDS->anColumnIndex[ii] = VRReadUInt(poDS->fp);
            }
        }
        break; // case vrh_magic
    case vrv_magic:
        // .VRV file describes tiles available for purchase
        poDS->nPixelMetres = VRGetUInt(poDS->abyHeader, 4);
        poDS->nRasterXSize = VRGetInt(poDS->abyHeader, 8 );
        poDS->nRasterYSize = VRGetInt(poDS->abyHeader, 0xC );
        
        poDS->nLeft = VRGetInt( poDS->abyHeader, 0x10 );
        poDS->nBottom = VRGetInt( poDS->abyHeader, 0x14 );
        CPLDebug("ViewrangerHV", "VRV max value %d",
                 VRGetInt( poDS->abyHeader, 0x18)
                 );
        poDS->nCountry = VRGetShort( poDS->abyHeader, 0x1C );
        poDS->nScale = VRGetUInt( poDS->abyHeader, 0x20 );

        {
            // based on 10 pixels/millimetre (254 dpi)
            double scaleFactor = poDS->nPixelMetres; //100000.0/poDS->nScale;
            poDS->nTop = poDS->nBottom +
                static_cast<int>(scaleFactor*poDS->nRasterYSize);
            poDS->nRight = poDS->nLeft + 
                static_cast<int>(scaleFactor*poDS->nRasterXSize);
            CPLDebug("ViewrangerHV", "Top %d = %d + %lf * %d",
                     poDS->nTop, poDS->nBottom, scaleFactor, poDS->nRasterYSize);
            CPLDebug("ViewrangerHV", "Right %d = %d + %lf * %d",
                     poDS->nRight, poDS->nLeft, scaleFactor, poDS->nRasterXSize);
        }

        poDS->nCountry = VRGetShort(poDS->abyHeader, 6);
        {
            const char* szInCharset = CharsetFromCountry(poDS->nCountry);
            const char* szOutCharset = "UTF-8";
            if (poDS->pszLongTitle != nullptr ) {
                VSIFree(poDS->pszLongTitle);
            }
            char* pszLT = VRHGetString(poDS->fp, 0x24);  // poOpenInfo->pszFilename might be good
            poDS->pszLongTitle =
                CPLRecode(pszLT, szInCharset, szOutCharset);
            VSIFree(pszLT);
        }
        poDS->pszCopyright = CPLStrdup("ViewRanger") ;
        
        break; // case vrv_magic
    case vmc_magic:
        // .vmc viewranger map choice file
        // generated by viewrangershop to store tiles to be purchased.
        // poDS->nPixelMetres = VRGetInt(poDS->abyHeader, 8);
        poDS->nPixelMetres = static_cast<unsigned int>
            (VRGetInt(poDS->abyHeader, 8) / 10.0);
        // int l5 = VRGetInt(poDS->abyHeader, 12);
        poDS->nRasterXSize = VRGetInt(poDS->abyHeader, 16);
        poDS->nRasterYSize = VRGetInt(poDS->abyHeader, 20);
        poDS->nScale = VRGetUInt( poDS->abyHeader, 0x20 );
        {
            // I am curious about these values
            unsigned int l5 = VRGetUInt(poDS->abyHeader, 12);
            unsigned int dc1 = (static_cast<unsigned char*>(poDS->abyHeader))[24];
            unsigned int p = VRGetUInt(poDS->abyHeader, 25);

            CPLDebug("ViewrangerHV",
                     "VMC nPixelMetres %d nScale %d l5 x%08x dc1 x%02x p x%08x",
                     poDS->nPixelMetres, poDS->nScale, l5, dc1, p);
            // CPLDebug doesn't count as "using" a variable
            (void) l5;
            (void) dc1;
            (void) p;
        }
        if (poDS->nVRHVersion == 1) {
            poDS->nCountry = 1;  // UK
            poDS->nLeft=0;
            poDS->nBottom=0;
        } else if (poDS->nVRHVersion == 2) {
            poDS->nCountry = VRGetShort(poDS->abyHeader, 29 );
            poDS->nLeft = VRGetInt(poDS->abyHeader, 33 );
            poDS->nBottom = VRGetInt(poDS->abyHeader, 37 );
        } else {
            CPLDebug("ViewrangerHV", "Unexpected VMC file version %d",
                     poDS->nVRHVersion);
        }
        poDS->nTop = poDS->nBottom + poDS->nRasterYSize*
            static_cast<int>(poDS->nPixelMetres);
        poDS->nRight = poDS->nLeft + poDS->nRasterXSize*
            static_cast<int>(poDS->nPixelMetres);
        CPLDebug("ViewrangerHV", "VMC Top %d = %d + %d * %d",
                 poDS->nTop, poDS->nBottom, poDS->nPixelMetres, poDS->nRasterYSize);
        CPLDebug("ViewrangerHV", "VMC Right %d = %d + %d * %d",
                 poDS->nRight, poDS->nLeft, poDS->nPixelMetres, poDS->nRasterXSize);
        poDS->pszCopyright = CPLStrdup("Unknown. Probably ViewRanger") ;
        break; // case vmc_magic
    default:
        // CPLError( CE_Failure, CPLE_NotSupported, 
        CPLDebug("Viewranger VRH/VRV",
                 "File magic 0x%08x unknown to viewranger VRH/VRV driver\n",
                  poDS->nMagic );
        delete poDS;
        return nullptr;
    } // switch poDS->nMagic


    if  (poDS->nRasterXSize <= 0 || poDS->nRasterYSize <= 0 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "Invalid dimensions : %d x %d", 
                  poDS->nRasterXSize, poDS->nRasterYSize);
        delete poDS;
        return nullptr;
    }
#define MAX_X 1024 // 2711
#define MAX_Y 1024 // 3267
    if  (poDS->nRasterXSize >MAX_X || poDS->nRasterYSize > MAX_Y )
    {
        if ( poDS->nMagic != vrh_magic ) {
            CPLError( CE_Failure, CPLE_NotSupported, 
                      "Unsupported dimensions : %d x %d (max %d x %d)", 
                      poDS->nRasterXSize, poDS->nRasterYSize,
                      MAX_X, MAX_Y);
            delete poDS;
            return nullptr;
            /* We could handle this case by using more than one
             * block (perhaps one per row) but that makes the
             * rotation from the "west up" data on file harder
             * and is not necessary for any files yet found.
             */
        }
        CPLDebug("ViewrangerHV",
                 "Unsupported dimensions : %d x %d (max %d x %d)", 
                 poDS->nRasterXSize, poDS->nRasterYSize,
                 MAX_X, MAX_Y);
    }
 
    /********************************************************************/
    /*                     Set datum - do I mean CRS ?                  */
    /********************************************************************/
    {
        char* pszSRS=nullptr;
        if(!poDS->poSRS) {
            poDS->poSRS = CRSfromCountry(poDS->nCountry);
        }
        poDS->poSRS->exportToWkt( &pszSRS );
        if (pszSRS) {
            poDS->sDatum = CPLString(pszSRS);
            CPLFree(pszSRS);
        }
    }

    /********************************************************************/
    /*             Report some strings found in the file                */
    /********************************************************************/
    CPLDebug("ViewrangerHV", "Long Title: %s",poDS->pszLongTitle);
    CPLDebug("ViewrangerHV", "Copyright: %s",poDS->pszCopyright);
    CPLDebug("ViewrangerHV", "%d metre pixels",poDS->nPixelMetres);
    if ((poDS->nMagic != vrh_magic) && poDS->nScale > 0) {
        CPLDebug("ViewrangerHV", "Scale: 1: %d",poDS->nScale);
    }
    CPLDebug("ViewrangerHV", "Datum: %s",poDS->sDatum.c_str());


    /********************************************************************/
    /*                       Set coordinate model                       */
    /********************************************************************/

    // calculate corner coordinates
#if 0
#if defined VRC_PIXEL_IS_FILE
    xy_to_latlon(poDS, 0, 0, &poDS->TL);
    xy_to_latlon(poDS, poDS->nRasterXSize, 0, &poDS->TR);
    xy_to_latlon(poDS, 0, poDS->nRasterYSize, &poDS->BL);
    xy_to_latlon(poDS, poDS->nRasterXSize, poDS->nRasterYSize, &poDS->BR);
#else
#if defined VRC_PIXEL_IS_TILE
    xy_to_latlon(poDS, 0, 0, &poDS->TL);
    xy_to_latlon(poDS, poDS->nRasterXSize, 0, &poDS->TR);
    xy_to_latlon(poDS, 0, poDS->nRasterYSize, &poDS->BL);
    xy_to_latlon(poDS, poDS->nRasterXSize, poDS->nRasterYSize, &poDS->BR);
#else
    // VRC_PIXEL_IS_PIXEL
    xy_to_latlon(poDS, 0, 0, &poDS->TL);
    xy_to_latlon(poDS, poDS->nRasterXSize, 0, &poDS->TR);
    xy_to_latlon(poDS, 0, poDS->nRasterYSize, &poDS->BL);
    xy_to_latlon(poDS, poDS->nRasterXSize, poDS->nRasterYSize, &poDS->BR);
#endif
#endif
#endif


/* -------------------------------------------------------------------- */
/*      Create copyright information.                                   */
/* -------------------------------------------------------------------- */
    poDS->SetMetadataItem( "TIFFTAG_COPYRIGHT", poDS->pszCopyright, "" );


/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    auto *poBand = new VRHRasterBand( poDS, 1, 01);
    poDS->SetBand( 1, poBand);
    if (poDS->nMagic == vrh_magic) {
        poBand->SetNoDataValue( nVRHNoData );
    } else if (poDS->nMagic == vrv_magic) {
        poBand->SetNoDataValue( nVRVNoData );
    } else {
        poBand->SetNoDataValue( nVRNoData );
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    /* Let gdal do this for us.      */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
} // VRHVDataset::Open()

/************************************************************************/
/*                       GDALRegister_VRHV()                      */
/************************************************************************/

void GDALRegister_VRHV()

{
    if (! GDAL_CHECK_VERSION("ViewrangerVRHV"))
        return;

    if( GDALGetDriverByName( "ViewrangerVRH/VRV" ) == nullptr )
    {
        auto *poDriver = new GDALDriver();
        if (poDriver==nullptr) {
            CPLError( CE_Failure, CPLE_ObjectNull,
                      "Could not build a driver for ViewrangerHV"
                     );
            return;
        }

        poDriver->SetDescription( "ViewrangerVRH/VRV" );

        // required in gdal version 2, not supported in gdal 1.11
#if GDAL_VERSION_MAJOR >= 2
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
#endif

        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "ViewRanger Height (.VRH/.VHV)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#VRHV" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "VRH" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                                   "Byte Int16" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        // "NONRECIPROCAL" is the intent of the author
        // of the code for this driver.
        // Since they are not the authors, or owners,
        // of the ViewRanger file formats, further research may be needed.
        poDriver->SetMetadataItem( "LICENSE_POLICY", "NONRECIPROCAL" );

        poDriver->pfnOpen = VRHVDataset::Open;
        poDriver->pfnIdentify = VRHVDataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}



/* -------------------------------------------------------------------------
 */
void
VRHRasterBand::read_VRH_Tile(VSILFILE *fp,
                             int tile_xx, int tile_yy,
                             void *pimage)
{
    if (tile_xx < 0 || tile_xx >= static_cast<VRHVDataset *>(poDS)->nRasterXSize ) {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "read_VRH_Tile invalid row %d", tile_xx );
        return ;
    }
    if (tile_yy < 0 || tile_yy >= static_cast<VRHVDataset *>(poDS)->nRasterYSize ) {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "read_VRH_Tile invalid column %d", tile_yy );
        return ;
    }
    if (pimage == nullptr ) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "read_VRH_Tile passed no image" );
        return ;
    }

    auto* pnBottomPixel = static_cast<signed short*>(pimage);
    // pnBottomPixel += static_cast<VRHVDataset *>(poDS)->nRasterYSize * tile_xx;

    signed short *pnOutPixel =
        pnBottomPixel + static_cast<VRHVDataset *>(poDS)->nRasterYSize - 1;

    signed int max = -1 * 0x10000;
    while (pnOutPixel >= pnBottomPixel) {
        signed int length = 1;
        signed int value = VRReadShort(fp) & 0xffff;
        if (value >= 0xf000) value -= 0x10000;
        if (value >= 0x8000) {
            length = VRReadShort(fp);
            value = VRReadShort(fp);
            if (static_cast<unsigned short>(value) == 0x8878) {
                CPLDebug("ViewrangerHV",
                         "run, length %d, value %d",
                         length, value);
            }
        }
        if (value > max) {
            max = value;
        }
        while ( length>0 ) {
            length--;
            *pnOutPixel-- = static_cast<short>(value);
            if (pnOutPixel < pnBottomPixel) {
                break;
            }
        }
    }
} // end VRHRasterBand::read_VRH_Tile()


/* -------------------------------------------------------------------------
 */
void
VRHRasterBand::read_VMC_Tile(VSILFILE *fp,
                             int tile_xx, int tile_yy,
                             void *pimage)
{
    if (tile_xx != 0 ) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "read_VMC_Tile %d %d out of range",
                  tile_xx, tile_yy );
        return ;
    }
    //if (tile_yy < 0 || tile_yy >= static_cast<VRHVDataset *>(poDS)->nRasterYSize)
    if (tile_yy != 0)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "read_VMC_Tile %d %d out of range",
                  tile_xx, tile_yy );
        return ;
    }
    if (pimage == nullptr ) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "read_VMC_Tile passed no image" );
        return ;
    }

    int seekres=0;
    if (static_cast<VRHVDataset *>(poDS)->nVRHVersion==1) {
        CPLDebug("ViewrangerHV", "Seeking to byte 29 for version 1");
        seekres = VSIFSeekL( fp, 29, SEEK_SET );
    } else {
        CPLDebug("ViewrangerHV", "Seeking to byte 41 for version 2");
        seekres = VSIFSeekL( fp, 41, SEEK_SET );
    }
    if ( seekres!=0 ) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "cannot seek to VMC data" );
        return ;
    }

    auto* pnBottomPixel = static_cast<unsigned char*>(pimage);
    // pnBottomPixel += static_cast<VRHVDataset *>(poDS)->nRasterYSize * tile_xx;

    // char *pnOutPixel = pnBottomPixel + static_cast<VRHVDataset *>(poDS)->nRasterYSize - 1;

    auto nCurrentData=static_cast<unsigned int>(VRReadChar(fp));
    int nBitsLeft=8;
    int nBytesRead=1;
    int nMaxPix=0;
    for (int x=0; x<nBlockXSize; x++) {
        for (int y=nBlockYSize-1 ; y>=0; y--) {
            int nPix = x + y*nBlockXSize;
            if (nPix>nMaxPix) {
                nMaxPix=nPix;
            }
            pnBottomPixel[nPix] =
                    (nCurrentData & 1) ? nVMCYesData : nVMCNoData ;
#if 0
            if (x>nBlockXSize/2) {
                pnBottomPixel[nPix] |= 128;
            }
            if (y>nBlockYSize/3) {
                pnBottomPixel[nPix] |= 64;
            }
#endif
#if 1
            CPLDebug("ViewrangerHV", "read_VMC_Tile: %p %3d %3d [%06d] = x%02x x%02x %d/%d",
                     pimage, x, y, nPix,
                     pnBottomPixel[nPix],
                     0xff&nCurrentData, nBytesRead, nBitsLeft);
#endif
            nCurrentData >>= 1;  nBitsLeft--;
            if (nBitsLeft<=0) {
                nCurrentData=static_cast<unsigned int>(VRReadChar(fp));
                nBitsLeft=8;
                nBytesRead++;
            }
        }
    }
    CPLDebug("ViewrangerHV", "read_VMC_Tile(%p %d %d %p %p)",
             static_cast<void*>(fp), tile_xx, tile_yy, pimage, pnBottomPixel);
    CPLDebug("ViewrangerHV", "read_VMC_Tile: x%02x %d/%d - pnBottomPixel %p furthest pix %d",
             0xff&nCurrentData, nBytesRead, nBitsLeft, pnBottomPixel, nMaxPix);
} // VRHRasterBand::read_VMC_Tile()


/* -------------------------------------------------------------------------
 */
void
VRHRasterBand::read_VRV_Tile(VSILFILE *fp,
                             int tile_xx, int tile_yy,
                             void *pimage)
{
    if (tile_xx != 0 ) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "read_VRV_Tile %d %d out of range",
                  tile_xx, tile_yy );
        return ;
    }
    if (tile_yy < 0 || tile_yy >= static_cast<VRHVDataset *>(poDS)->nRasterYSize)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "read_VRV_Tile %d %d out of range",
                  tile_xx, tile_yy );
        return ;
    }
    if (pimage == nullptr ) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "read_VRV_Tile passed no image" );
        return ;
    }

    int seekres = VSIFSeekL( fp, 0x24, SEEK_SET );
    if ( seekres ) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "cannot seek to VRV data" );
        return ;
    }
    vsi_l_offset string_length = VRReadUInt(fp);
    seekres = VSIFSeekL( fp, 0x28+string_length, SEEK_SET );
    if ( seekres ) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "cannot seek to VRV data" );
        return ;
    }

    auto* pnBottomPixel = static_cast<char*>(pimage);
    // pnBottomPixel += static_cast<VRHVDataset *>(poDS)->nRasterYSize * tile_xx;

    // char *pnOutPixel = pnBottomPixel + static_cast<VRHVDataset *>(poDS)->nRasterYSize - 1;

    int pixelnum = 0;
    for (int x=0; x<nBlockXSize; x++) {
        for (int y=nBlockYSize-1 ; y>=0; y--) {
            int nPixel=VRReadChar(fp);
            if (nPixel == 0) nPixel=nVRVNoData;
            pnBottomPixel[y*nBlockXSize + x] = static_cast<char>(nPixel);

#if 0
            CPLDebug("ViewrangerHV", "read_VRV_Tile: %p %d %d %d [%d] = %d",
                     pimage, x, y, pixelnum, y*nBlockXSize + x,
                     (int)pnBottomPixel[y*nBlockXSize + x] );
#endif

            pixelnum++;
        }
    }
    CPLDebug("ViewrangerHV", "read_VRV_Tile(%p %d %d %p %p)\n\tread %d = %d * %d pixels",
             static_cast<void*>(fp), tile_xx, tile_yy, pimage, pnBottomPixel,
             pixelnum, nBlockXSize, nBlockYSize);
    (void)pixelnum; // cppcheck ignores CPLDebug args. Don't complain that pixelnum is never used.
#if 0
    // Could crash with small files (eg Switzerland1M.VRV is 2x2) ?
    CPLDebug("ViewrangerHV", "read_VRV_Tile %p\t%02x %02x %02x %02x %02x %02x %02x %02x\t%02x %02x %02x %02x %02x %02x %02x %02x",
             pnBottomPixel,
             pnBottomPixel[0], pnBottomPixel[1], pnBottomPixel[2], pnBottomPixel[3],
             pnBottomPixel[4], pnBottomPixel[5], pnBottomPixel[6], pnBottomPixel[7],
             pnBottomPixel[8], pnBottomPixel[9], pnBottomPixel[10], pnBottomPixel[11],
             pnBottomPixel[12], pnBottomPixel[13], pnBottomPixel[14], pnBottomPixel[15]
             );
#endif
} // VRHRasterBand::read_VMC_Tile()


/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **VRHVDataset::GetFileList()
{
    char **papszFileList = GDALPamDataset::GetFileList();

    CPLDebug("ViewrangerHV", "GetDescription %s", GetDescription() );

    // GDALReadWorldFile2 (gdal_misc.cpp) has code we need to copy
#if 0
    if (osWldFilename.size() != 0 &&
        CSLFindString(papszFileList, osWldFilename) == -1)
    {
        papszFileList = CSLAddString( papszFileList, osWldFilename );
    }
#endif
    return papszFileList;
}

//#endif // def FRMT_vrhv
//#endif // def FRMT_vrc
