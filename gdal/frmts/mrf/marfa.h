/*
* Copyright (c) 2002-2012, California Institute of Technology.
* All rights reserved.  Based on Government Sponsored Research under contracts NAS7-1407 and/or NAS7-03001.
*
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
*   1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
*   2. Redistributions in binary form must reproduce the above copyright notice,
*      this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
*   3. Neither the name of the California Institute of Technology (Caltech), its operating division the Jet Propulsion Laboratory (JPL),
*      the National Aeronautics and Space Administration (NASA), nor the names of its contributors may be used to
*      endorse or promote products derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL THE CALIFORNIA INSTITUTE OF TECHNOLOGY BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* Copyright 2014-2015 Esri
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

/******************************************************************************
 * $Id$
 *
 * Project:  Meta Raster Format
 * Purpose:  MRF structures
 * Author:   Lucian Plesea
 *
 ******************************************************************************
 *
 *
 *
 ****************************************************************************/

#ifndef GDAL_FRMTS_MRF_MARFA_H_INCLUDED
#define GDAL_FRMTS_MRF_MARFA_H_INCLUDED

#include <gdal_pam.h>
#include <ogr_srs_api.h>
#include <ogr_spatialref.h>

// For printing values
#include <ostream>
#include <iostream>
#include <sstream>

#ifdef GDAL_COMPILATION
#define NAMESPACE_MRF_START namespace GDAL_MRF {
#define NAMESPACE_MRF_END   }
#define USING_NAMESPACE_MRF using namespace GDAL_MRF;
#else
#define NAMESPACE_MRF_START
#define NAMESPACE_MRF_END
#define USING_NAMESPACE_MRF
#endif

NAMESPACE_MRF_START

// ZLIB Bit flag fields
// 0:3 - level, 4 - GZip, 5 RAW zlib, 6:9 strategy
#define ZFLAG_LMASK 0xF
// GZ and RAW are mutually exclusive, GZ has higher priority
// If neither is set, use zlib stream format
#define ZFLAG_GZ 0x10
#define ZFLAG_RAW 0x20

// Mask for zlib strategy, valid values are 0 to 4 shifted six bits, see zlib for meaning
// Can use one of:
// Z_DEFAULT : whatever zlib decides
// Z_FILTERED : optimized for delta encoding
// Z_HUFFMAN_ONLY : Only huffman encoding (adaptive)
// Z_RLE : Only next character matches
// Z_FIXED : Static huffman, faster decoder
//
#define ZFLAG_SMASK 0x1c0

// Force LERC to be included, normally off, detected in the makefile
// #define LERC

// These are a pain to maintain in sync.  They should be replaced with 
// C++11 uniform initializers.  The externs reside in util.cpp
enum ILCompression {
    IL_PNG = 0, IL_PPNG, IL_JPEG, IL_JPNG, IL_NONE, IL_ZLIB, IL_TIF, 
#if defined(LERC)
    IL_LERC,
#endif
    IL_ERR_COMP
};

// Sequential is not supported by GDAL
enum ILOrder { IL_Interleaved = 0, IL_Separate, IL_Sequential, IL_ERR_ORD };
extern char const * const * ILComp_Name;
extern char const * const * ILComp_Ext;
extern char const * const * ILOrder_Name;

class GDALMRFDataset;
class GDALMRFRasterBand;

typedef struct {
    char   *buffer;
    size_t size;
} buf_mgr;

// A tile index record, 16 bytes, big endian
typedef struct {
    GIntBig offset;
    GIntBig size;
} ILIdx;

// Size of an image, also used as a tile or pixel location
struct ILSize {
    GInt32 x, y, z, c;
    GIntBig l; // Dual use, sometimes it holds the number of pages
    ILSize(const int x_ = -1, const int y_ = -1, const int z_ = -1,
        const int c_ = -1, const int l_ = -1)
    {
        x = x_; y = y_; z = z_; c = c_; l = l_;
    }

    bool operator==(const ILSize& other)
    {
        return ((x == other.x) && (y == other.y) && (z == other.z) &&
            (c == other.c) && (l == other.l));
    }

    bool operator!=(const ILSize& other) { return !(*this == other); }
};

std::ostream& operator<<(std::ostream &out, const ILSize& sz);
std::ostream& operator<<(std::ostream &out, const ILIdx& t);

bool is_Endianess_Dependent(GDALDataType dt, ILCompression comp);

// Debugging support
// #define PPMW
#ifdef PPMW
void ppmWrite(const char *fname, const char *data, const ILSize &sz);
#endif

/**
 * Collects information pertaining to a single raster
 * This structure is being shallow copied, no pointers allowed
 *
 */

typedef struct ILImage {
    ILImage();
    GIntBig dataoffset;
    GIntBig idxoffset;
    GInt32 quality;
    GInt32 pageSizeBytes;
    ILSize size;
    ILSize pagesize;
    ILSize pagecount;
    ILCompression comp;
    ILOrder order;
    int nbo;
    int hasNoData;
    double NoDataValue;
    CPLString datfname;
    CPLString idxfname;
    GDALDataType dt;
    GDALColorInterp ci;
} ILImage;

// Delarations of utility functions

/**
 *
 *\brief  Converters between endianness
 *  Call netXX() to guarantee big endian
 *
 */
static inline unsigned short int swab16(const unsigned short int val)
{
    return (val << 8) | (val >> 8);
}

static inline unsigned int swab32(unsigned int val)
{
    return (unsigned int)(swab16((unsigned short int) val)) << 16
        | swab16((unsigned short int) (val >> 16));
}

static inline unsigned long long int swab64(const unsigned long long int val)
{
    return (unsigned long long int) (swab32((unsigned int)val)) << 32
        | swab32((unsigned int)(val >> 32));
}

// NET_ORDER is true if machine is BE, false otherwise
// Call netxx() if network (big) order is needed

#ifdef CPL_MSB
#define NET_ORDER true
// These could be macros, but for the side effects related to type
static inline unsigned short net16(const unsigned short x)
{
    return (x);
}
static inline unsigned int net32(const unsigned int x)
{
    return (x);
}
static inline unsigned long long net64(const unsigned long long x)
{
    return (x);
}
#else
#define NET_ORDER false
#define net16(x) swab16(x)
#define net32(x) swab32(x)
#define net64(x) swab64(x)
#endif

const char *CompName(ILCompression comp);
const char *OrderName(ILOrder val);
ILCompression CompToken(const char *, ILCompression def = IL_ERR_COMP);
ILOrder OrderToken(const char *, ILOrder def = IL_ERR_ORD);
CPLString getFname(CPLXMLNode *, const char *, const CPLString &, const char *);
CPLString getFname(const CPLString &, const char *);
double getXMLNum(CPLXMLNode *, const char *, double);
GIntBig IdxOffset(const ILSize &, const ILImage &);
double logb(double val, double base);
int IsPower(double value, double base);
CPLXMLNode *SearchXMLSiblings(CPLXMLNode *psRoot, const char *pszElement);
CPLString PrintDouble(double d, const char *frmt = "%12.8f");
void XMLSetAttributeVal(CPLXMLNode *parent, const char* pszName,
    const double val, const char *frmt = "%12.8f");
CPLXMLNode *XMLSetAttributeVal(CPLXMLNode *parent,
    const char*pszName, const ILSize &sz, const char *frmt = NULL);
void XMLSetAttributeVal(CPLXMLNode *parent,
    const char*pszName, std::vector<double> const &values);
//
// Extension to CSL, set an entry only if it doesn't already exist
//
char **CSLAddIfMissing(char **papszList,
    const char *pszName, const char *pszValue);

GDALColorEntry GetXMLColorEntry(CPLXMLNode *p);
GIntBig IdxSize(const ILImage &full, const int scale = 0);
// Similar to uncompress() from zlib, accepts the ZFLAG_RAW
// Return true if it worked
int ZUnPack(const buf_mgr &src, buf_mgr &dst, int flags);
// Similar to compress2() but with flags to control zlib features
// Returns true if it worked
int ZPack(const buf_mgr &src, buf_mgr &dst, int flags);
// checks that the file exists and is at least sz, if access is update it extends it
int CheckFileSize(const char *fname, GIntBig sz, GDALAccess eAccess);

// Number of pages of size psz needed to hold n elements
static inline int pcount(const int n, const int sz) {
    return 1 + (n - 1) / sz;
}

// Returns a pagecount per dimension, .l will have the total number
static inline const ILSize pcount(const ILSize &size, const ILSize &psz) {
    ILSize pcnt;
    pcnt.x = pcount(size.x, psz.x);
    pcnt.y = pcount(size.y, psz.y);
    pcnt.z = pcount(size.z, psz.z);
    pcnt.c = pcount(size.c, psz.c);
    pcnt.l = static_cast<GIntBig>(pcnt.x) * pcnt.y * pcnt.z * pcnt.c;
    return pcnt;
}

// Wrapper around the VISFile, remembers how the file was opened
typedef struct {
    VSILFILE *FP;
    GDALRWFlag acc;
} VF;

// Offset of index, pos is in pages
GIntBig IdxOffset(const ILSize &pos, const ILImage &img);

enum { SAMPLING_ERR, SAMPLING_Avg, SAMPLING_Near };

GDALMRFRasterBand *newMRFRasterBand(GDALMRFDataset *, const ILImage &, int, int level = 0);

class GDALMRFDataset : public GDALPamDataset {


    friend class GDALMRFRasterBand;
    friend GDALMRFRasterBand *newMRFRasterBand(GDALMRFDataset *, const ILImage &, int, int level);

public:
    GDALMRFDataset();
    virtual ~GDALMRFDataset();

    static GDALDataset *Open(GDALOpenInfo *);
    static int Identify(GDALOpenInfo *);
    void Crystalize();

    static GDALDataset *CreateCopy(const char *pszFilename, GDALDataset *poSrcDS,
        int bStrict, char **papszOptions, GDALProgressFunc pfnProgress,
        void *pProgressData);

    static GDALDataset *Create(const char * pszName,
        int nXSize, int nYSize, int nBands,
        GDALDataType eType, char ** papszOptions);

    // Stub for delete, GDAL should only overwrite the XML
    static CPLErr Delete(const char *) {
        return CE_None;
    }

    virtual const char *GetProjectionRef() { return projection; }
    virtual CPLErr SetProjection(const char *proj) {
        projection = proj;
        return CE_None;
    }

    virtual CPLString const &GetPhotometricInterpretation() { return photometric; }
    virtual CPLErr SetPhotometricInterpretation(const char *photo) {
        photometric = photo;
        return CE_None;
    }

    virtual CPLErr GetGeoTransform(double *gt);
    virtual CPLErr SetGeoTransform(double *gt);

#ifdef unused
    virtual CPLErr AdviseRead(int nXOff, int nYOff, int nXSize, int nYSize,
        int nBufXSize, int nBufYSize,
        GDALDataType eDT,
        int nBandCount, int *panBandList,
        char **papszOptions);
#endif

    virtual char **GetFileList();

    void SetColorTable(GDALColorTable *pct) { poColorTable = pct; };
    const GDALColorTable *GetColorTable() { return poColorTable; };
    void SetNoDataValue(const char*);
    void SetMinValue(const char*);
    void SetMaxValue(const char*);
    CPLErr SetVersion(int version);

    const CPLString GetFname() { return fname; };
    // Patches a region of all the next overview, argument counts are in blocks
    virtual CPLErr PatchOverview(int BlockX, int BlockY, int Width, int Height,
        int srcLevel = 0, int recursive = false, int sampling_mode = SAMPLING_Avg);

    // Creates an XML tree from the current MRF.  If written to a file it becomes an MRF
    CPLXMLNode *BuildConfig();

    void SetPBufferSize(unsigned int sz) {
        pbsize = sz;
    }
    unsigned int GetPBufferSize() {
        return pbsize;
    }

protected:
    CPLErr LevelInit(const int l);

    // Reads the XML metadata and returns the XML
    CPLXMLNode *ReadConfig() const;

    // Apply create options to the current dataset
    void ProcessCreateOptions(char **papszOptions);

    // Writes the XML tree as MRF.  It does not check the content
    int WriteConfig(CPLXMLNode *);

    // Initializes the dataset from an MRF metadata XML
    CPLErr Initialize(CPLXMLNode *);

    // Do nothing, this is not possible in an MRF
    CPLErr CleanOverviews(void) { return CE_None; }

    // Add uniform scale overlays, returns the new size of the index file
    GIntBig AddOverviews(int scale);

    // Late allocation buffer
    bool SetPBuffer(unsigned int sz);
    void *GetPBuffer() {
        if (!pbuffer && pbsize)
            SetPBuffer(pbsize);
        return pbuffer;
    }

#if GDAL_VERSION_MAJOR >= 2
    virtual CPLErr IRasterIO(GDALRWFlag, int, int, int, int,
        void *, int, int, GDALDataType,
        int, int *, GSpacing, GSpacing, GSpacing, GDALRasterIOExtraArg*);
#else
    virtual CPLErr IRasterIO(GDALRWFlag, int, int, int, int,
        void *, int, int, GDALDataType,
        int, int *, int, int, int);
#endif

    virtual CPLErr IBuildOverviews(const char*, int, int*, int, int*,
        GDALProgressFunc, void*);


    // Write a tile, the infooffset is the relative position in the index file
    virtual CPLErr WriteTile(void *buff, GUIntBig infooffset, GUIntBig size = 0);

    // For versioned MRFs, add a version
    CPLErr AddVersion();

    // Read the index record itself
    CPLErr ReadTileIdx(ILIdx &tinfo, const ILSize &pos, const ILImage &img, const GIntBig bias = 0);

    VSILFILE *IdxFP();
    VSILFILE *DataFP();
    GDALRWFlag IdxMode() {
        if (!ifp.FP) IdxFP();
        return ifp.acc;
    };
    GDALRWFlag DataMode() {
        if (!dfp.FP) DataFP();
        return dfp.acc;
    };
    GDALDataset *GetSrcDS();

    /*
     *  There are two images defined to allow for morphing on use, in the future
     *  For example storing a multispectral image and opening it as RGB
     *
     *  Support for this feature is not yet implemented.
     *
     */

    // What the image is on disk
    ILImage full;
    // How we use it currently
    ILImage current;
    // The third dimension slice in use
    int zslice;

    // MRF file name
    CPLString fname;

    // The source to be cached in this MRF
    CPLString source;
    GIntBig idxSize; // The size of each version index, or the size of the cloned index

    int clonedSource; // Is it a cloned source
    int bypass_cache; // Do we alter disk cache
    int mp_safe;      // Not thread safe, only multiple writers
    int hasVersions;  // Does it support versions
    int verCount;     // The last version
    int bCrystalized; // Unset only during the create process

    // Freeform sticky dataset options, as a list of key-value pairs
    CPLStringList optlist;

    // If caching data, the parent dataset
    GDALDataset *poSrcDS;

    // Level picked, or -1 for native
    int level;

    // Child dataset, if picking a specific level
    GDALMRFDataset *cds;
    // A small int actually due to GDAL limitations
    double scale;

    // A place to keep an uncompressed block, to keep from allocating it all the time
    void *pbuffer;
    unsigned int pbsize;
    ILSize tile; // ID of tile present in buffer
    GIntBig bdirty;    // Holds bits, to be used in pixel interleaved (up to 64 bands)

    // GeoTransform support
    double GeoTransform[6];
    int bGeoTransformValid;

    // Projection string, WKT
    CPLString projection;

    // Photometric interpretation
    CPLString photometric;

    GDALColorTable *poColorTable;
    int Quality;

    VF dfp;  // Data file handle
    VF ifp;  // Index file handle

    // statistical values
    std::vector<double> vNoData, vMin, vMax;
};

class GDALMRFRasterBand : public GDALPamRasterBand {
    friend class GDALMRFDataset;
public:
    GDALMRFRasterBand(GDALMRFDataset *, const ILImage &, int, int);
    virtual ~GDALMRFRasterBand();
    virtual CPLErr IReadBlock(int xblk, int yblk, void *buffer);
    virtual CPLErr IWriteBlock(int xblk, int yblk, void *buffer);

    virtual GDALColorTable *GetColorTable() { return poDS->poColorTable; }

    CPLErr SetColorInterpretation(GDALColorInterp ci) { img.ci = ci; return CE_None; }
    virtual GDALColorInterp GetColorInterpretation() { return img.ci; }

    // Get works within MRF or with PAM
    virtual double  GetNoDataValue(int *);
    virtual CPLErr  SetNoDataValue(double);

    // These get set with SetStatistics.  Let PAM handle it
    virtual double  GetMinimum(int *);
    virtual double  GetMaximum(int *);


    // MRF specific, fetch is from a remote source
    CPLErr FetchBlock(int xblk, int yblk, void *buffer = NULL);
    // Fetch a block from a cloned MRF
    CPLErr FetchClonedBlock(int xblk, int yblk, void *buffer = NULL);

    // Block not stored on disk
    CPLErr FillBlock(void *buffer);

    // de-interlace a buffer in pixel blocks
    CPLErr RB(int xblk, int yblk, buf_mgr src, void *buffer);

    const char *GetOptionValue(const char *opt, const char *def) const;
    void SetAccess(GDALAccess eA) { eAccess = eA; }
    void SetDeflate(int v) { deflatep = (v != 0); }

protected:
    // Pointer to the GDALMRFDataset
    GDALMRFDataset *poDS;
    // 0 based
    GInt32 m_band;
    int deflatep;
    int deflate_flags;
    // Level count of this band
    GInt32 m_l;
    // The info about the current image, to enable R-sets
    ILImage img;
    std::vector<GDALMRFRasterBand *> overviews;
    int overview;

    VSILFILE *IdxFP() { return poDS->IdxFP(); }
    GDALRWFlag IdxMode() { return poDS->IdxMode(); }
    VSILFILE *DataFP() { return poDS->DataFP(); }
    GDALRWFlag DataMode() { return poDS->DataMode(); }

    // How many bytes are in a band block (not a page, a single band block)
    // Easiest is to calculate it from the pageSizeBytes
    GUInt32 blockSizeBytes() {
        return poDS->current.pageSizeBytes / poDS->current.pagesize.c;
    }

    const CPLStringList & GetOptlist() const { return poDS->optlist; }

    // Compression and decompression functions.  To be overwritten by specific implementations
    virtual CPLErr Compress(buf_mgr &dst, buf_mgr &src) = 0;
    virtual CPLErr Decompress(buf_mgr &dst, buf_mgr &src) = 0;

    // Read the index record itself, can be overwritten
    //    virtual CPLErr ReadTileIdx(const ILSize &, ILIdx &, GIntBig bias = 0);

    GIntBig bandbit(int b) { return ((GIntBig)1) << b; }
    GIntBig bandbit() { return bandbit(m_band); }
    GIntBig AllBandMask() { return bandbit(poDS->nBands) - 1; }

    // Overview Support
    // Inherited from GDALRasterBand
    // These are called only in the base level RasterBand
    virtual int GetOverviewCount();
    virtual GDALRasterBand *GetOverview(int n);
    void AddOverview(GDALMRFRasterBand *b) { overviews.push_back(b); }
};

/**
 * Each type of compression needs to define at least two methods, a compress and a
 * decompress, which take as arguments a dest and a source buffer, plus an image structure
 * that holds the information about the compression type.
 * Filtering is needed, probably in the form of pack and unpack functions
 *
 */

class PNG_Codec {
public:
    PNG_Codec(const ILImage &image) : img(image), 
        PNGColors(NULL), PNGAlpha(NULL), PalSize(0), TransSize(0), deflate_flags(0) {};

    virtual ~PNG_Codec() {
        CPLFree(PNGColors);
        CPLFree(PNGAlpha);
    }

    CPLErr CompressPNG(buf_mgr &dst, buf_mgr &src);
    CPLErr DecompressPNG(buf_mgr &dst, buf_mgr &src);

    const ILImage img;

    void *PNGColors;
    void *PNGAlpha;
    int PalSize, TransSize, deflate_flags;

private:
    PNG_Codec& operator= (const PNG_Codec& src); // not implemented. but suppress MSVC warning about 'assignment operator could not be generated'

};

class PNG_Band : public GDALMRFRasterBand {
    friend class GDALMRFDataset;
public:
    PNG_Band(GDALMRFDataset *pDS, const ILImage &image, int b, int level);

protected:
    virtual CPLErr Decompress(buf_mgr &dst, buf_mgr &src);
    virtual CPLErr Compress(buf_mgr &dst, buf_mgr &src);

    PNG_Codec codec;
};

/* 
 * The JPEG Codec can be used outside of the JPEG_Band
*/

class JPEG_Codec {
public:
    JPEG_Codec(const ILImage &image) : img(image), sameres(FALSE), rgb(FALSE), optimize(false) {};

    CPLErr CompressJPEG(buf_mgr &dst, buf_mgr &src);
    CPLErr DecompressJPEG(buf_mgr &dst, buf_mgr &src);

#if defined(JPEG12_SUPPORTED) // Internal only
#define LIBJPEG_12_H "../jpeg/libjpeg12/jpeglib.h"
    CPLErr CompressJPEG12(buf_mgr &dst, buf_mgr &src);
    CPLErr DecompressJPEG12(buf_mgr &dst, buf_mgr &src);
#endif

    const ILImage img;

    // JPEG specific flags
    bool sameres;
    bool rgb;
    bool optimize;

private:
    JPEG_Codec& operator= (const JPEG_Codec& src); // not implemented. but suppress MSVC warning about 'assignment operator could not be generated'
};

class JPEG_Band : public GDALMRFRasterBand {
    friend class GDALMRFDataset;
public:
    JPEG_Band(GDALMRFDataset *pDS, const ILImage &image, int b, int level);
    virtual ~JPEG_Band() {};

protected:
    virtual CPLErr Decompress(buf_mgr &dst, buf_mgr &src);
    virtual CPLErr Compress(buf_mgr &dst, buf_mgr &src);

    JPEG_Codec codec;
};

// A 2 or 4 band, with JPEG and/or PNG page encoding, optimized for size
class JPNG_Band : public GDALMRFRasterBand {
    friend class GDALMRFDataset;
public:
    JPNG_Band(GDALMRFDataset *pDS, const ILImage &image, int b, int level);
    virtual ~JPNG_Band();
protected:
    virtual CPLErr Decompress(buf_mgr &dst, buf_mgr &src);
    virtual CPLErr Compress(buf_mgr &dst, buf_mgr &src);

    CPLErr CompressJPNG(buf_mgr &dst, buf_mgr &src);
    CPLErr DecompressJPNG(buf_mgr &dst, buf_mgr &src);
    bool rgb, sameres, optimize;
};

class Raw_Band : public GDALMRFRasterBand {
    friend class GDALMRFDataset;
public:
    Raw_Band(GDALMRFDataset *pDS, const ILImage &image, int b, int level) :
        GDALMRFRasterBand(pDS, image, b, int(level)) {};
    virtual ~Raw_Band() {};
protected:
    virtual CPLErr Decompress(buf_mgr &dst, buf_mgr &src);
    virtual CPLErr Compress(buf_mgr &dst, buf_mgr &src);
};

class TIF_Band : public GDALMRFRasterBand {
    friend class GDALMRFDataset;
public:
    TIF_Band(GDALMRFDataset *pDS, const ILImage &image, int b, int level);
    virtual ~TIF_Band();
protected:
    virtual CPLErr Decompress(buf_mgr &dst, buf_mgr &src);
    virtual CPLErr Compress(buf_mgr &dst, buf_mgr &src);

    // Create options for TIF pages
    char **papszOptions;
};

#if defined(LERC)
class LERC_Band : public GDALMRFRasterBand {
    friend class GDALMRFDataset;
public:
    LERC_Band(GDALMRFDataset *pDS, const ILImage &image, int b, int level);
    virtual ~LERC_Band();
protected:
    virtual CPLErr Decompress(buf_mgr &dst, buf_mgr &src);
    virtual CPLErr Compress(buf_mgr &dst, buf_mgr &src);
    double precision;
    int version;
};
#endif

/*\brief band for level mrf
 *
 * Stand alone definition of a derived band, used in access to a specific level in an MRF
 *
 */
class GDALMRFLRasterBand : public GDALPamRasterBand {
public:
    GDALMRFLRasterBand(GDALMRFRasterBand *b) {
        pBand = b;
        eDataType = b->GetRasterDataType();
        b->GetBlockSize(&nBlockXSize, &nBlockYSize);
        eAccess = b->GetAccess();
        nRasterXSize = b->GetXSize();
        nRasterYSize = b->GetYSize();
    }
    virtual CPLErr IReadBlock(int xblk, int yblk, void *buffer) {
        return pBand->IReadBlock(xblk, yblk, buffer);
    }
    virtual CPLErr IWriteBlock(int xblk, int yblk, void *buffer) {
        return pBand->IWriteBlock(xblk, yblk, buffer);
    }
    virtual GDALColorTable *GetColorTable() {
        return pBand->GetColorTable();
    }
    virtual GDALColorInterp GetColorInterpretation() {
        return pBand->GetColorInterpretation();
    }
    virtual double  GetNoDataValue(int * pbSuccess) {
        return pBand->GetNoDataValue(pbSuccess);
    }
    virtual double  GetMinimum(int *b) {
        return pBand->GetMinimum(b);
    }
    virtual double  GetMaximum(int *b) {
        return pBand->GetMaximum(b);
    }

protected:
    virtual int GetOverviewCount() { return 0; }
    virtual GDALRasterBand *GetOverview(int ) { return NULL; }

    GDALMRFRasterBand *pBand;
};

NAMESPACE_MRF_END

#endif // GDAL_FRMTS_MRF_MARFA_H_INCLUDED

