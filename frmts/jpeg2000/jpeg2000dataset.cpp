/******************************************************************************
 *
 * Project:  JPEG-2000
 * Purpose:  Partial implementation of the ISO/IEC 15444-1 standard
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2002, Andrey Kiselev <dron@ak4719.spb.edu>
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef __STDC_LIMIT_MACROS
// Needed on RHEL 6 for SIZE_MAX availability, needed by Jasper
 #define __STDC_LIMIT_MACROS 1
#endif

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "gdaljp2abstractdataset.h"
#include "gdaljp2metadata.h"

#include <jasper/jasper.h>
#include "jpeg2000_vsil_io.h"

#include <cmath>

#include <algorithm>

CPL_CVSID("$Id$")

// XXX: Part of code below extracted from the JasPer internal headers and
// must be in sync with JasPer version (this one works with JasPer 1.900.1)
#define JP2_FTYP_MAXCOMPATCODES 32
#define JP2_BOX_IHDR    0x69686472      /* Image Header */
#define JP2_BOX_BPCC    0x62706363      /* Bits Per Component */
#define JP2_BOX_PCLR    0x70636c72      /* Palette */
#define JP2_BOX_UUID    0x75756964      /* UUID */
extern "C" {
#ifdef NOT_USED
typedef struct {
        uint_fast32_t magic;
} jp2_jp_t;
typedef struct {
        uint_fast32_t majver;
        uint_fast32_t minver;
        uint_fast32_t numcompatcodes;
        uint_fast32_t compatcodes[JP2_FTYP_MAXCOMPATCODES];
} jp2_ftyp_t;
#endif
typedef struct {
        uint_fast32_t width;
        uint_fast32_t height;
        uint_fast16_t numcmpts;
        uint_fast8_t bpc;
        // cppcheck-suppress unusedStructMember
        uint_fast8_t comptype;
        // cppcheck-suppress unusedStructMember
        uint_fast8_t csunk;
        // cppcheck-suppress unusedStructMember
        uint_fast8_t ipr;
} jp2_ihdr_t;
typedef struct {
        uint_fast16_t numcmpts;
        uint_fast8_t *bpcs;
} jp2_bpcc_t;
#ifdef NOT_USED
typedef struct {
        uint_fast8_t method;
        uint_fast8_t pri;
        uint_fast8_t approx;
        uint_fast32_t csid;
        uint_fast8_t *iccp;
        int iccplen;
} jp2_colr_t;
#endif
typedef struct {
        uint_fast16_t numlutents;
        uint_fast8_t numchans;
        // cppcheck-suppress unusedStructMember
        int_fast32_t *lutdata;
        uint_fast8_t *bpc;
} jp2_pclr_t;
typedef struct {
        uint_fast16_t channo;
        uint_fast16_t type;
        uint_fast16_t assoc;
} jp2_cdefchan_t;
typedef struct {
        uint_fast16_t numchans;
        // cppcheck-suppress unusedStructMember
        jp2_cdefchan_t *ents;
} jp2_cdef_t;
typedef struct {
        uint_fast16_t cmptno;
        uint_fast8_t map;
        uint_fast8_t pcol;
} jp2_cmapent_t;

typedef struct {
        uint_fast16_t numchans;
        // cppcheck-suppress unusedStructMember
        jp2_cmapent_t *ents;
} jp2_cmap_t;

#ifdef HAVE_JASPER_UUID
typedef struct {
        uint_fast32_t datalen;
        uint_fast8_t uuid[16];
        uint_fast8_t *data;
} jp2_uuid_t;
#endif

struct jp2_boxops_s;
typedef struct {

        struct jp2_boxops_s *ops;
        struct jp2_boxinfo_s *info;

        uint_fast32_t type;

        /* The length of the box including the (variable-length) header. */
        uint_fast32_t len;

        /* The length of the box data. */
        uint_fast32_t datalen;

        union {
#ifdef NOT_USED
                jp2_jp_t jp;
                jp2_ftyp_t ftyp;
#endif
                jp2_ihdr_t ihdr;
                jp2_bpcc_t bpcc;
#ifdef NOT_USED
                jp2_colr_t colr;
#endif
                jp2_pclr_t pclr;
                jp2_cdef_t cdef;
                jp2_cmap_t cmap;
#ifdef HAVE_JASPER_UUID
                jp2_uuid_t uuid;
#endif
        } data;

} jp2_box_t;

#ifdef NOT_USED
typedef struct jp2_boxops_s {
        void (*init)(jp2_box_t *box);
        void (*destroy)(jp2_box_t *box);
        int (*getdata)(jp2_box_t *box, jas_stream_t *in);
        int (*putdata)(jp2_box_t *box, jas_stream_t *out);
        void (*dumpdata)(jp2_box_t *box, FILE *out);
} jp2_boxops_t;
#endif

extern jp2_box_t *jp2_box_create(int type);
extern void jp2_box_destroy(jp2_box_t *box);
extern jp2_box_t *jp2_box_get(jas_stream_t *in);
extern int jp2_box_put(jp2_box_t *box, jas_stream_t *out);
#ifdef HAVE_JASPER_UUID
int jp2_encode_uuid(jas_image_t *image, jas_stream_t *out,
                    char *optstr, jp2_box_t *uuid);
#endif
}
// XXX: End of JasPer header.

/************************************************************************/
/* ==================================================================== */
/*                              JPEG2000Dataset                         */
/* ==================================================================== */
/************************************************************************/

class JPEG2000Dataset final: public GDALJP2AbstractDataset
{
    friend class JPEG2000RasterBand;

    jas_stream_t *psStream;
    jas_image_t *psImage;
    int         iFormat;
    int         bPromoteTo8Bit;

    int         bAlreadyDecoded;
    int         DecodeImage();

  public:
                JPEG2000Dataset();
                ~JPEG2000Dataset();

    static int           Identify( GDALOpenInfo * );
    static GDALDataset  *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            JPEG2000RasterBand                        */
/* ==================================================================== */
/************************************************************************/

class JPEG2000RasterBand final: public GDALPamRasterBand
{
    friend class JPEG2000Dataset;

    // NOTE: poDS may be altered for NITF/JPEG2000 files!
    JPEG2000Dataset     *poGDS;

    jas_matrix_t        *psMatrix;

    int                  iDepth;
    int                  bSignedness;

  public:

                JPEG2000RasterBand( JPEG2000Dataset *, int, int, int );
    virtual ~JPEG2000RasterBand();

    virtual CPLErr IReadBlock( int, int, void * ) override;
    virtual GDALColorInterp GetColorInterpretation() override;
};

/************************************************************************/
/*                           JPEG2000RasterBand()                       */
/************************************************************************/

JPEG2000RasterBand::JPEG2000RasterBand( JPEG2000Dataset *poDSIn, int nBandIn,
                                        int iDepthIn, int bSignednessIn ) :
    poGDS(poDSIn),
    psMatrix(nullptr),
    iDepth(iDepthIn),
    bSignedness(bSignednessIn)
{
    poDS = poDSIn;
    nBand = nBandIn;

    // XXX: JasPer can't handle data with depth > 32 bits
    // Maximum possible depth for JPEG2000 is 38!
    switch ( bSignedness )
    {
        case 1:                         // Signed component
        if (iDepth <= 8)
            this->eDataType = GDT_Byte; // FIXME: should be signed,
                                        // but we haven't signed byte
                                        // data type in GDAL
        else if (iDepth <= 16)
            this->eDataType = GDT_Int16;
        else if (iDepth <= 32)
            this->eDataType = GDT_Int32;
        break;
        case 0:                         // Unsigned component
        default:
        if (iDepth <= 8)
            this->eDataType = GDT_Byte;
        else if (iDepth <= 16)
            this->eDataType = GDT_UInt16;
        else if (iDepth <= 32)
            this->eDataType = GDT_UInt32;
        break;
    }
    // FIXME: Figure out optimal block size!
    // Should the block size be fixed or determined dynamically?
    nBlockXSize = std::min(256, poDSIn->nRasterXSize);
    nBlockYSize = std::min(256, poDSIn->nRasterYSize);
    psMatrix = jas_matrix_create(nBlockYSize, nBlockXSize);

    if( iDepth % 8 != 0 && !poDSIn->bPromoteTo8Bit )
    {
        SetMetadataItem( "NBITS",
                         CPLString().Printf("%d",iDepth),
                         "IMAGE_STRUCTURE" );
    }
    SetMetadataItem( "COMPRESSION", "JP2000", "IMAGE_STRUCTURE" );
}

/************************************************************************/
/*                         ~JPEG2000RasterBand()                        */
/************************************************************************/

JPEG2000RasterBand::~JPEG2000RasterBand()
{
    if ( psMatrix )
        jas_matrix_destroy( psMatrix );
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr JPEG2000RasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                      void * pImage )
{
    int             i, j;

    // Decode image from the stream, if not yet
    if ( !poGDS->DecodeImage() )
    {
        return CE_Failure;
    }

    // Now we can calculate the pixel offset of the top left by multiplying
    // block offset with the block size.

    /* In case the dimensions of the image are not multiple of the block dimensions */
    /* take care of not requesting more pixels than available for the blocks at the */
    /* right or bottom of the image */
    const int nWidthToRead =
        std::min(nBlockXSize, poGDS->nRasterXSize - nBlockXOff * nBlockXSize);
    const int nHeightToRead =
        std::min(nBlockYSize, poGDS->nRasterYSize - nBlockYOff * nBlockYSize);

    jas_image_readcmpt( poGDS->psImage, nBand - 1,
                        nBlockXOff * nBlockXSize, nBlockYOff * nBlockYSize,
                        nWidthToRead, nHeightToRead, psMatrix );

    int nWordSize = GDALGetDataTypeSize(eDataType) / 8;
    int nLineSize = nBlockXSize * nWordSize;
    GByte* ptr = (GByte*)pImage;

    /* Pad incomplete blocks at the right or bottom of the image */
    if (nWidthToRead != nBlockXSize || nHeightToRead != nBlockYSize)
        memset(pImage, 0, nLineSize * nBlockYSize);

    for( i = 0; i < nHeightToRead; i++, ptr += nLineSize )
    {
        for( j = 0; j < nWidthToRead; j++ )
        {
            // XXX: We need casting because matrix element always
            // has 32 bit depth in JasPer
            // FIXME: what about float values?
            switch( eDataType )
            {
                case GDT_Int16:
                {
                    ((GInt16*)ptr)[j] = (GInt16)jas_matrix_get(psMatrix, i, j);
                }
                break;
                case GDT_Int32:
                {
                    ((GInt32*)ptr)[j] = (GInt32)jas_matrix_get(psMatrix, i, j);
                }
                break;
                case GDT_UInt16:
                {
                    ((GUInt16*)ptr)[j] = (GUInt16)jas_matrix_get(psMatrix, i, j);
                }
                break;
                case GDT_UInt32:
                {
                    ((GUInt32*)ptr)[j] = (GUInt32)jas_matrix_get(psMatrix, i, j);
                }
                break;
                case GDT_Byte:
                default:
                {
                    ((GByte*)ptr)[j] = (GByte)jas_matrix_get(psMatrix, i, j);
                }
                break;
            }
        }
    }

    if( poGDS->bPromoteTo8Bit && nBand == 4 )
    {
        ptr = (GByte*)pImage;
        for( i = 0; i < nHeightToRead; i++, ptr += nLineSize )
        {
            for( j = 0; j < nWidthToRead; j++ )
            {
                ((GByte*)ptr)[j] *= 255;
            }
        }
    }

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp JPEG2000RasterBand::GetColorInterpretation()
{
    // Decode image from the stream, if not yet
    if ( !poGDS->DecodeImage() )
    {
        return GCI_Undefined;
    }

    if ( jas_clrspc_fam( jas_image_clrspc( poGDS->psImage ) ) ==
         JAS_CLRSPC_FAM_GRAY )
        return GCI_GrayIndex;
    else if ( jas_clrspc_fam( jas_image_clrspc( poGDS->psImage ) ) ==
              JAS_CLRSPC_FAM_RGB )
    {
        switch ( jas_image_cmpttype( poGDS->psImage, nBand - 1 ) )
        {
            case JAS_IMAGE_CT_RGB_R:
                return GCI_RedBand;
            case JAS_IMAGE_CT_RGB_G:
                return GCI_GreenBand;
            case JAS_IMAGE_CT_RGB_B:
                return GCI_BlueBand;
            case JAS_IMAGE_CT_OPACITY:
                return GCI_AlphaBand;
            default:
                return GCI_Undefined;
        }
    }
    else
        return GCI_Undefined;
}

/************************************************************************/
/* ==================================================================== */
/*                              JPEG2000Dataset                         */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           JPEG2000Dataset()                          */
/************************************************************************/

JPEG2000Dataset::JPEG2000Dataset() :
    psStream(nullptr),
    psImage(nullptr),
    iFormat(0),
    bPromoteTo8Bit(FALSE),
    bAlreadyDecoded(FALSE)
{
    nBands = 0;

    poDriver = (GDALDriver *)GDALGetDriverByName("JPEG2000");
}

/************************************************************************/
/*                            ~JPEG2000Dataset()                        */
/************************************************************************/

JPEG2000Dataset::~JPEG2000Dataset()

{
    FlushCache(true);

    if ( psStream )
        jas_stream_close( psStream );
    if ( psImage )
        jas_image_destroy( psImage );
}

/************************************************************************/
/*                             DecodeImage()                            */
/************************************************************************/
int JPEG2000Dataset::DecodeImage()
{
    if (bAlreadyDecoded)
        return psImage != nullptr;

    bAlreadyDecoded = TRUE;
    if ( !( psImage = jas_image_decode(psStream, iFormat, nullptr) ) )
    {
        CPLDebug( "JPEG2000", "Unable to decode image. Format: %s, %d",
                  jas_image_fmttostr( iFormat ), iFormat );
        return FALSE;
    }

    /* Case of a JP2 image : check that the properties given by */
    /* the JP2 boxes match the ones of the code stream */
    if (nBands != 0)
    {
        if (nBands != static_cast<int>(jas_image_numcmpts( psImage )))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "The number of components indicated in the IHDR box (%d) mismatch "
                     "the value specified in the code stream (%d)",
                     nBands, jas_image_numcmpts( psImage ));
            jas_image_destroy( psImage );
            psImage = nullptr;
            return FALSE;
        }

        if (nRasterXSize != jas_image_cmptwidth( psImage, 0 ) ||
            nRasterYSize != jas_image_cmptheight( psImage, 0 ) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "The dimensions indicated in the IHDR box (%d x %d) mismatch "
                     "the value specified in the code stream (%d x %d)",
                     nRasterXSize, nRasterYSize,
                     (int)jas_image_cmptwidth( psImage, 0 ),
                     (int)jas_image_cmptheight( psImage, 0 ));
            jas_image_destroy( psImage );
            psImage = nullptr;
            return FALSE;
        }

        int iBand;
        for ( iBand = 0; iBand < nBands; iBand++ )
        {
            JPEG2000RasterBand* poBand = (JPEG2000RasterBand*) GetRasterBand(iBand+1);
            if (poBand->iDepth != static_cast<int>(jas_image_cmptprec( psImage, iBand )) ||
                poBand->bSignedness != jas_image_cmptsgnd( psImage, iBand ))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "The bit depth of band %d indicated in the IHDR box (%d) mismatch "
                         "the value specified in the code stream (%d)",
                         iBand + 1, poBand->iDepth, jas_image_cmptprec( psImage, iBand ));
                jas_image_destroy( psImage );
                psImage = nullptr;
                return FALSE;
            }
        }
    }

    /* Ask for YCbCr -> RGB translation */
    if ( jas_clrspc_fam( jas_image_clrspc( psImage ) ) ==
              JAS_CLRSPC_FAM_YCBCR )
    {
        jas_image_t *psRGBImage = nullptr;
        jas_cmprof_t *psRGBProf = nullptr;
        CPLDebug( "JPEG2000", "forcing conversion to sRGB");
        if (!(psRGBProf = jas_cmprof_createfromclrspc(JAS_CLRSPC_SRGB))) {
            CPLDebug( "JPEG2000", "cannot create sRGB profile");
            return TRUE;
        }
        if (!(psRGBImage = jas_image_chclrspc(psImage, psRGBProf, JAS_CMXFORM_INTENT_PER))) {
            CPLDebug( "JPEG2000", "cannot convert to sRGB");
            jas_cmprof_destroy(psRGBProf);
            return TRUE;
        }
        jas_image_destroy(psImage);
        jas_cmprof_destroy(psRGBProf);
        psImage = psRGBImage;
    }

    return TRUE;
}

static void JPEG2000Init()
{
    static int bHasInit = FALSE;
    if (!bHasInit)
    {
        bHasInit = TRUE;
        jas_init();
    }
}

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

int JPEG2000Dataset::Identify( GDALOpenInfo * poOpenInfo )

{
    constexpr unsigned char jpc_header[] = {0xff,0x4f,0xff,0x51}; // SOC + RSIZ markers
    constexpr unsigned char jp2_box_jp[] = {0x6a,0x50,0x20,0x20}; /* 'jP  ' */

    if( poOpenInfo->nHeaderBytes >= 16
        && (memcmp( poOpenInfo->pabyHeader, jpc_header,
                    sizeof(jpc_header) ) == 0
            || memcmp( poOpenInfo->pabyHeader + 4, jp2_box_jp,
                    sizeof(jp2_box_jp) ) == 0
            /* PGX file*/
            || (memcmp( poOpenInfo->pabyHeader, "PG", 2) == 0 &&
                (poOpenInfo->pabyHeader[2] == ' ' || poOpenInfo->pabyHeader[2] == '\t') &&
                (memcmp( poOpenInfo->pabyHeader + 3, "ML", 2) == 0 ||
                 memcmp( poOpenInfo->pabyHeader + 3, "LM", 2) == 0))) )
        return TRUE;

    else
        return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *JPEG2000Dataset::Open( GDALOpenInfo * poOpenInfo )

{
    int         iFormat;
    const char *pszFormatName = nullptr;

    if (!Identify(poOpenInfo))
        return nullptr;

    JPEG2000Init();
    jas_stream_t *sS= JPEG2000_VSIL_fopen( poOpenInfo->pszFilename, "rb" );
    if( !sS )
    {
        return nullptr;
    }

    iFormat = jas_image_getfmt( sS );
    if ( !(pszFormatName = jas_image_fmttostr( iFormat )) )
    {
        jas_stream_close( sS );
        return nullptr;
    }
    if ( strlen( pszFormatName ) < 3 ||
        (!STARTS_WITH_CI(pszFormatName, "jp2") &&
         !STARTS_WITH_CI(pszFormatName, "jpc") &&
         !STARTS_WITH_CI(pszFormatName, "pgx")) )
    {
        CPLDebug( "JPEG2000", "JasPer reports file is format type `%s'.",
                  pszFormatName );
        jas_stream_close( sS );
        return nullptr;
    }

    if( !GDALIsDriverDeprecatedForGDAL35StillEnabled("JPEG2000", "You should consider using another driver, in particular the JP2OpenJPEG driver that is a better free and open source alternative. ") )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        jas_stream_close(sS);
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The JPEG2000 driver does not support update access to existing"
                  " datasets.\n" );
        return nullptr;
    }
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    int *paiDepth = nullptr;
    int *pabSignedness = nullptr;
    int iBand;

    JPEG2000Dataset *poDS = new JPEG2000Dataset();

    poDS->psStream = sS;
    poDS->iFormat = iFormat;

    if ( STARTS_WITH_CI(pszFormatName, "jp2") )
    {
        // XXX: Hack to read JP2 boxes from input file. JasPer hasn't public
        // API call for such things, so we will use internal JasPer functions.
        jp2_box_t *box;
        while ( ( box = jp2_box_get(poDS->psStream) ) != nullptr )
        {
            switch (box->type)
            {
                case JP2_BOX_IHDR:
                poDS->nBands = static_cast<int>(box->data.ihdr.numcmpts);
                poDS->nRasterXSize = static_cast<int>(box->data.ihdr.width);
                poDS->nRasterYSize = static_cast<int>(box->data.ihdr.height);
                CPLDebug( "JPEG2000",
                          "IHDR box found. Dump: "
                          "width=%d, height=%d, numcmpts=%d, bpp=%d",
                          (int)box->data.ihdr.width, (int)box->data.ihdr.height,
                          (int)box->data.ihdr.numcmpts, (box->data.ihdr.bpc & 0x7F) + 1 );
                /* ISO/IEC 15444-1:2004 I.5.3.1 specifies that 255 means that all */
                /* components have not the same bit depth and/or sign and that a */
                /* BPCC box must then follow to specify them for each component */
                if ( box->data.ihdr.bpc != 255 && paiDepth == nullptr && pabSignedness == nullptr )
                {
                    paiDepth = (int *)CPLMalloc(poDS->nBands * sizeof(int));
                    pabSignedness = (int *)CPLMalloc(poDS->nBands * sizeof(int));
                    for ( iBand = 0; iBand < poDS->nBands; iBand++ )
                    {
                        paiDepth[iBand] = (box->data.ihdr.bpc & 0x7F) + 1;
                        pabSignedness[iBand] = box->data.ihdr.bpc >> 7;
                        CPLDebug( "JPEG2000",
                                  "Component %d: bpp=%d, signedness=%d",
                                  iBand, paiDepth[iBand], pabSignedness[iBand] );
                    }
                }
                break;

                case JP2_BOX_BPCC:
                CPLDebug( "JPEG2000", "BPCC box found. Dump:" );
                if ( !paiDepth && !pabSignedness )
                {
                    paiDepth = (int *)
                        CPLMalloc( box->data.bpcc.numcmpts * sizeof(int) );
                    pabSignedness = (int *)
                        CPLMalloc( box->data.bpcc.numcmpts * sizeof(int) );
                    for( iBand = 0; iBand < (int)box->data.bpcc.numcmpts; iBand++ )
                    {
                        paiDepth[iBand] = (box->data.bpcc.bpcs[iBand] & 0x7F) + 1;
                        pabSignedness[iBand] = box->data.bpcc.bpcs[iBand] >> 7;
                        CPLDebug( "JPEG2000",
                                  "Component %d: bpp=%d, signedness=%d",
                                  iBand, paiDepth[iBand], pabSignedness[iBand] );
                    }
                }
                break;

                case JP2_BOX_PCLR:
                CPLDebug( "JPEG2000",
                          "PCLR box found. Dump: number of LUT entries=%d, "
                          "number of resulting channels=%d",
                          (int)box->data.pclr.numlutents, box->data.pclr.numchans );
                poDS->nBands = box->data.pclr.numchans;
                if ( paiDepth )
                    CPLFree( paiDepth );
                if ( pabSignedness )
                    CPLFree( pabSignedness );
                paiDepth = (int *)
                        CPLMalloc( box->data.pclr.numchans * sizeof(int) );
                pabSignedness = (int *)
                        CPLMalloc( box->data.pclr.numchans * sizeof(int) );
                for( iBand = 0; iBand < (int)box->data.pclr.numchans; iBand++ )
                {
                    paiDepth[iBand] = (box->data.pclr.bpc[iBand] & 0x7F) + 1;
                    pabSignedness[iBand] = box->data.pclr.bpc[iBand] >> 7;
                    CPLDebug( "JPEG2000",
                              "Component %d: bpp=%d, signedness=%d",
                              iBand, paiDepth[iBand], pabSignedness[iBand] );
                }
                break;
            }
            jp2_box_destroy( box );
            box = nullptr;
        }
        if( !paiDepth || !pabSignedness )
        {
            delete poDS;
            CPLDebug( "JPEG2000", "Unable to read JP2 header boxes.\n" );
            CPLFree( paiDepth );
            CPLFree( pabSignedness );
            return nullptr;
        }
        if ( jas_stream_rewind( poDS->psStream ) < 0 )
        {
            delete poDS;
            CPLDebug( "JPEG2000", "Unable to rewind input stream.\n" );
            CPLFree( paiDepth );
            CPLFree( pabSignedness );
            return nullptr;
        }
    }
    else
    {
        if ( !poDS->DecodeImage() )
        {
            delete poDS;
            return nullptr;
        }

        poDS->nBands = jas_image_numcmpts( poDS->psImage );
        poDS->nRasterXSize = static_cast<int>(jas_image_cmptwidth( poDS->psImage, 0 ));
        poDS->nRasterYSize = static_cast<int>(jas_image_cmptheight( poDS->psImage, 0 ));
        paiDepth = (int *)CPLMalloc( poDS->nBands * sizeof(int) );
        pabSignedness = (int *)CPLMalloc( poDS->nBands * sizeof(int) );
        for ( iBand = 0; iBand < poDS->nBands; iBand++ )
        {
            paiDepth[iBand] = jas_image_cmptprec( poDS->psImage, iBand );
            pabSignedness[iBand] = jas_image_cmptsgnd( poDS->psImage, iBand );
        }
    }

    if ( !GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize) ||
         !GDALCheckBandCount(poDS->nBands, 0) )
    {
        CPLFree( paiDepth );
        CPLFree( pabSignedness );
        delete poDS;
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Should we promote alpha channel to 8 bits ?                     */
/* -------------------------------------------------------------------- */
    poDS->bPromoteTo8Bit =
        poDS->nBands == 4 &&
        paiDepth[0] == 8 &&
        paiDepth[1] == 8 &&
        paiDepth[2] == 8 &&
        paiDepth[3] == 1 &&
        CPLFetchBool(poOpenInfo->papszOpenOptions,
                     "1BIT_ALPHA_PROMOTION", true);
    if( poDS->bPromoteTo8Bit )
        CPLDebug( "JPEG2000",  "Fourth (alpha) band is promoted from 1 bit to 8 bit");

/* -------------------------------------------------------------------- */

/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */

    for( iBand = 1; iBand <= poDS->nBands; iBand++ )
    {
        poDS->SetBand( iBand, new JPEG2000RasterBand( poDS, iBand,
            paiDepth[iBand - 1], pabSignedness[iBand - 1] ) );
    }

    CPLFree( paiDepth );
    CPLFree( pabSignedness );

    poDS->LoadJP2Metadata(poOpenInfo);

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

/* -------------------------------------------------------------------- */
/*      Vector layers                                                   */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nOpenFlags & GDAL_OF_VECTOR )
    {
        poDS->LoadVectorLayers(
            CPLFetchBool(poOpenInfo->papszOpenOptions,
                         "OPEN_REMOTE_GML", false));

        // If file opened in vector-only mode and there's no vector,
        // return
        if( (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) == 0 &&
            poDS->GetLayerCount() == 0 )
        {
            delete poDS;
            return nullptr;
        }
    }

    return poDS;
}

/************************************************************************/
/*                      JPEG2000CreateCopy()                            */
/************************************************************************/

static GDALDataset *
JPEG2000CreateCopy( const char * pszFilename, GDALDataset *poSrcDS,
                    int bStrict, char ** papszOptions,
                    GDALProgressFunc pfnProgress, void * pProgressData )

{
    if( !GDALIsDriverDeprecatedForGDAL35StillEnabled("JPEG2000", "You should consider using another driver, in particular the JP2OpenJPEG driver that is a better free and open source alternative. ") )
        return nullptr;

    int  nBands = poSrcDS->GetRasterCount();
    int  nXSize = poSrcDS->GetRasterXSize();
    int  nYSize = poSrcDS->GetRasterYSize();

    if( nBands == 0 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Unable to export files with zero bands." );
        return nullptr;
    }

    if (poSrcDS->GetRasterBand(1)->GetColorTable() != nullptr)
    {
        CPLError( (bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported,
                  "JPEG2000 driver ignores color table. "
                  "The source raster band will be considered as grey level.\n"
                  "Consider using color table expansion (-expand option in gdal_translate)\n");
        if (bStrict)
            return nullptr;
    }

    // TODO(schwehr): Localize these vars.
    int iBand;
    GDALRasterBand  *poBand = nullptr;
    for ( iBand = 0; iBand < nBands; iBand++ )
    {
        poBand = poSrcDS->GetRasterBand( iBand + 1);

        switch ( poBand->GetRasterDataType() )
        {
            case GDT_Byte:
            case GDT_Int16:
            case GDT_UInt16:
                break;

            default:
                if( !CPLTestBool(CPLGetConfigOption("JPEG2000_FORCE_CREATION", "NO")) )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "A band of the source dataset is of type %s, which might cause crashes in libjasper. "
                             "Set JPEG2000_FORCE_CREATION configuration option to YES to attempt the creation of the file.",
                             GDALGetDataTypeName(poBand->GetRasterDataType()));
                    return nullptr;
                }
                break;
        }
    }

    if( !pfnProgress( 0.0, nullptr, pProgressData ) )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */
    JPEG2000Init();
    const char* pszAccess = STARTS_WITH_CI(pszFilename, "/vsisubfile/") ? "r+b" : "w+b";
    jas_stream_t *psStream = JPEG2000_VSIL_fopen( pszFilename, pszAccess);
    if( !psStream )
    {
        CPLError( CE_Failure, CPLE_FileIO, "Unable to create file %s.\n",
                  pszFilename );
        return nullptr;
    }

    jas_image_t *psImage = jas_image_create0();
    if ( !psImage )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, "Unable to create image %s.\n",
                  pszFilename );
        jas_stream_close( psStream );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Loop over image, copying image data.                            */
/* -------------------------------------------------------------------- */
    GUInt32             *paiScanline;
    int                 iLine, iPixel;
    CPLErr              eErr = CE_None;
    jas_image_cmptparm_t *sComps; // Array of pointers to image components

    sComps = (jas_image_cmptparm_t*)
        CPLMalloc( nBands * sizeof(jas_image_cmptparm_t) );

    jas_matrix_t *psMatrix  = jas_matrix_create( 1, nXSize );
    if ( !psMatrix )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                  "Unable to create matrix with size %dx%d.\n", 1, nYSize );
        CPLFree( sComps );
        jas_image_destroy( psImage );
        jas_stream_close( psStream );
        return nullptr;
    }
    paiScanline = (GUInt32 *) CPLMalloc( nXSize *
                            GDALGetDataTypeSize(GDT_UInt32) / 8 );

    for ( iBand = 0; iBand < nBands; iBand++ )
    {
        poBand = poSrcDS->GetRasterBand( iBand + 1);

        sComps[iBand].tlx = 0;
        sComps[iBand].tly = 0;
        sComps[iBand].hstep = 1;
        sComps[iBand].vstep = 1;
        sComps[iBand].width = nXSize;
        sComps[iBand].height = nYSize;
        const char* pszNBITS = CSLFetchNameValue(papszOptions, "NBITS");
        if( pszNBITS && atoi(pszNBITS) > 0 )
            sComps[iBand].prec = atoi(pszNBITS);
        else
            sComps[iBand].prec = GDALGetDataTypeSize( poBand->GetRasterDataType() );
        switch ( poBand->GetRasterDataType() )
        {
            case GDT_Int16:
            case GDT_Int32:
            case GDT_Float32:
            case GDT_Float64:
            sComps[iBand].sgnd = 1;
            break;
            case GDT_Byte:
            case GDT_UInt16:
            case GDT_UInt32:
            default:
            sComps[iBand].sgnd = 0;
            break;
        }
        jas_image_addcmpt(psImage, iBand, sComps);

        for( iLine = 0; eErr == CE_None && iLine < nYSize; iLine++ )
        {
            eErr = poBand->RasterIO( GF_Read, 0, iLine, nXSize, 1,
                              paiScanline, nXSize, 1, GDT_UInt32,
                              sizeof(GUInt32), sizeof(GUInt32) * nXSize, nullptr );
            for ( iPixel = 0; iPixel < nXSize; iPixel++ )
                jas_matrix_setv( psMatrix, iPixel, paiScanline[iPixel] );

            if( (jas_image_writecmpt(psImage, iBand, 0, iLine,
                              nXSize, 1, psMatrix)) < 0 )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                    "Unable to write scanline %d of the component %d.\n",
                    iLine, iBand );
                jas_matrix_destroy( psMatrix );
                CPLFree( paiScanline );
                CPLFree( sComps );
                jas_image_destroy( psImage );
                jas_stream_close( psStream );
                return nullptr;
            }

            if( eErr == CE_None &&
            !pfnProgress( ((iLine + 1) + iBand * nYSize) /
                          ((double) nYSize * nBands),
                         nullptr, pProgressData) )
            {
                eErr = CE_Failure;
                CPLError( CE_Failure, CPLE_UserInterrupt,
                      "User terminated CreateCopy()" );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*       Read compression parameters and encode the image.              */
/* -------------------------------------------------------------------- */
    const char  *apszComprOptions[]=
    {
        "imgareatlx",
        "imgareatly",
        "tilegrdtlx",
        "tilegrdtly",
        "tilewidth",
        "tileheight",
        "prcwidth",
        "prcheight",
        "cblkwidth",
        "cblkheight",
        "mode",
        "rate",
        "ilyrrates",
        "prg",
        "numrlvls",
        "sop",
        "eph",
        "lazy",
        "termall",
        "segsym",
        "vcausal",
        "pterm",
        "resetprob",
        "numgbits",
        nullptr
    };

    const char *pszFormatName = CSLFetchNameValue( papszOptions, "FORMAT" );
    if( pszFormatName == nullptr && EQUAL(CPLGetExtension(pszFilename), "J2K") )
        pszFormatName = "jpc";
    else if ( !pszFormatName ||
         (!STARTS_WITH_CI(pszFormatName, "jp2") &&
          !STARTS_WITH_CI(pszFormatName, "jpc") ) )
        pszFormatName = "jp2";

    // TODO(schwehr): Move pszOptionBuf off the stack.
    const int OPTSMAX = 4096;
    char pszOptionBuf[OPTSMAX + 1] = {};

    if ( papszOptions )
    {
        CPLDebug( "JPEG2000", "User supplied parameters:" );
        for ( int i = 0; papszOptions[i] != nullptr; i++ )
        {
            CPLDebug( "JPEG2000", "%s\n", papszOptions[i] );
            for ( int j = 0; apszComprOptions[j] != nullptr; j++ )
                if( EQUALN( apszComprOptions[j], papszOptions[i],
                            strlen(apszComprOptions[j]) ) )
                {
                    const int n = static_cast<int>(strlen( pszOptionBuf ));
                    const int m =
                        n + static_cast<int>(strlen( papszOptions[i] )) + 1;
                    if ( m > OPTSMAX )
                        break;
                    if ( n > 0 )
                    {
                        strcat( pszOptionBuf, "\n" );
                    }
                    strcat( pszOptionBuf, papszOptions[i] );
                }
        }
    }
    CPLDebug( "JPEG2000", "Parameters, delivered to the JasPer library:" );
    CPLDebug( "JPEG2000", "%s", pszOptionBuf );

    if ( nBands == 1 )                      // Grayscale
    {
        jas_image_setclrspc( psImage, JAS_CLRSPC_SGRAY );
        jas_image_setcmpttype( psImage, 0, JAS_IMAGE_CT_GRAY_Y );
    }
    else if ( nBands == 3 || nBands == 4 )  // Assume as RGB(A)
    {
        jas_image_setclrspc( psImage, JAS_CLRSPC_SRGB );
        for ( iBand = 0; iBand < nBands; iBand++ )
        {
            poBand = poSrcDS->GetRasterBand( iBand + 1);
            switch ( poBand->GetColorInterpretation() )
            {
                case GCI_RedBand:
                jas_image_setcmpttype( psImage, iBand, JAS_IMAGE_CT_RGB_R );
                break;
                case GCI_GreenBand:
                jas_image_setcmpttype( psImage, iBand, JAS_IMAGE_CT_RGB_G );
                break;
                case GCI_BlueBand:
                jas_image_setcmpttype( psImage, iBand, JAS_IMAGE_CT_RGB_B );
                break;
                case GCI_AlphaBand:
                jas_image_setcmpttype( psImage, iBand, JAS_IMAGE_CT_OPACITY );
                break;
                default:
                jas_image_setcmpttype( psImage, iBand, JAS_IMAGE_CT_UNKNOWN );
                break;
            }
        }
    }
    else                                    // Unknown
    {
        /* JAS_CLRSPC_UNKNOWN causes crashes in Jasper jp2_enc.c at line 231 */
        /* iccprof = jas_iccprof_createfromcmprof(jas_image_cmprof(image)); */
        /* but if we explicitly set the cmprof, it does not work better */
        /* since it would abort at line 281 later ... */
        /* So the best option is to switch to gray colorspace */
        /* And we need to switch at the band level too, otherwise Kakadu or */
        /* JP2MrSID don't like it */
        //jas_image_setclrspc( psImage, JAS_CLRSPC_UNKNOWN );
        jas_image_setclrspc( psImage, JAS_CLRSPC_SGRAY );
        for ( iBand = 0; iBand < nBands; iBand++ )
            //jas_image_setcmpttype( psImage, iBand, JAS_IMAGE_CT_UNKNOWN );
            jas_image_setcmpttype( psImage, iBand, JAS_IMAGE_CT_GRAY_Y );
    }

/* -------------------------------------------------------------------- */
/*      Set the GeoTIFF box if georeferencing is available, and this    */
/*      is a JP2 file.                                                  */
/* -------------------------------------------------------------------- */
    if ( STARTS_WITH_CI(pszFormatName, "jp2") )
    {
#ifdef HAVE_JASPER_UUID
        double  adfGeoTransform[6];
        if( CPLFetchBool( papszOptions, "GeoJP2", true ) &&
            ((poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None
                 && (adfGeoTransform[0] != 0.0
                     || adfGeoTransform[1] != 1.0
                     || adfGeoTransform[2] != 0.0
                     || adfGeoTransform[3] != 0.0
                     || adfGeoTransform[4] != 0.0
                     || std::abs(adfGeoTransform[5]) != 1.0))
                || poSrcDS->GetGCPCount() > 0
                || poSrcDS->GetMetadata("RPC") != nullptr ) )
        {
            GDALJP2Metadata oJP2Geo;

            if( poSrcDS->GetGCPCount() > 0 )
            {
                oJP2Geo.SetSpatialRef( poSrcDS->GetGCPSpatialRef() );
                oJP2Geo.SetGCPs( poSrcDS->GetGCPCount(), poSrcDS->GetGCPs() );
            }
            else
            {
                oJP2Geo.SetSpatialRef( poSrcDS->GetSpatialRef() );
                oJP2Geo.SetGeoTransform( adfGeoTransform );
            }

            oJP2Geo.SetRPCMD(  poSrcDS->GetMetadata("RPC") );

            const char* pszAreaOrPoint = poSrcDS->GetMetadataItem(GDALMD_AREA_OR_POINT);
            oJP2Geo.bPixelIsPoint = pszAreaOrPoint != nullptr && EQUAL(pszAreaOrPoint, GDALMD_AOP_POINT);

            GDALJP2Box *poBox = oJP2Geo.CreateJP2GeoTIFF();
            jp2_box_t  *box = jp2_box_create( JP2_BOX_UUID );
            memcpy( box->data.uuid.uuid, poBox->GetUUID(), 16 );
            box->data.uuid.datalen = poBox->GetDataLength() - 16;
            box->data.uuid.data =
                (uint_fast8_t *)jas_malloc( poBox->GetDataLength() - 16 );
            memcpy( box->data.uuid.data, poBox->GetWritableData() + 16,
                    poBox->GetDataLength() - 16 );
            delete poBox;
            poBox = nullptr;

            if ( jp2_encode_uuid( psImage, psStream, pszOptionBuf, box) < 0 )
            {
                CPLError( CE_Failure, CPLE_FileIO,
                          "Unable to encode image %s.", pszFilename );
                jp2_box_destroy( box );
                jas_matrix_destroy( psMatrix );
                CPLFree( paiScanline );
                CPLFree( sComps );
                jas_image_destroy( psImage );
                jas_stream_close( psStream );
                return nullptr;
            }
            jp2_box_destroy( box );
        }
        else
        {
#endif
            if ( jp2_encode( psImage, psStream, pszOptionBuf) < 0 )
            {
                CPLError( CE_Failure, CPLE_FileIO,
                          "Unable to encode image %s.", pszFilename );
                jas_matrix_destroy( psMatrix );
                CPLFree( paiScanline );
                CPLFree( sComps );
                jas_image_destroy( psImage );
                jas_stream_close( psStream );
                return nullptr;
            }
#ifdef HAVE_JASPER_UUID
        }
#endif
    }
    else    // Write JPC code stream
    {
        if ( jpc_encode(psImage, psStream, pszOptionBuf) < 0 )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Unable to encode image %s.\n", pszFilename );
            jas_matrix_destroy( psMatrix );
            CPLFree( paiScanline );
            CPLFree( sComps );
            jas_image_destroy( psImage );
            jas_stream_close( psStream );
            return nullptr;
        }
    }

    jas_stream_flush( psStream );

    jas_matrix_destroy( psMatrix );
    CPLFree( paiScanline );
    CPLFree( sComps );
    jas_image_destroy( psImage );
    if ( jas_stream_close( psStream ) )
    {
        CPLError( CE_Failure, CPLE_FileIO, "Unable to close file %s.\n",
                  pszFilename );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Add GMLJP2 box at end of file.                                  */
/* -------------------------------------------------------------------- */
    if ( STARTS_WITH_CI(pszFormatName, "jp2") )
    {
        double  adfGeoTransform[6];
        if( CPLFetchBool( papszOptions, "GMLJP2", true ) &&
            poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None &&
            poSrcDS->GetSpatialRef() != nullptr )
        {
            VSILFILE* fp = VSIFOpenL(pszFilename, "rb+");
            if( fp )
            {
                // Look for jp2c box and patch its LBox to be the real box size
                // instead of zero
                int bOK = FALSE;
                GUInt32   nLBox;
                GUInt32   nTBox;

                while( true )
                {
                    if( VSIFReadL(&nLBox, 4, 1, fp) != 1 ||
                        VSIFReadL(&nTBox, 4, 1, fp) != 1 )
                        break;
                    nLBox = CPL_MSBWORD32( nLBox );
                    if( memcmp(&nTBox, "jp2c", 4) == 0 )
                    {
                        if( nLBox >= 8 )
                        {
                            bOK = TRUE;
                            break;
                        }
                        if( nLBox == 0 )
                        {
                            vsi_l_offset nPos = VSIFTellL(fp);
                            VSIFSeekL(fp, 0, SEEK_END);
                            vsi_l_offset nEnd = VSIFTellL(fp);
                            VSIFSeekL(fp, nPos - 8, SEEK_SET);
                            nLBox = (GUInt32)(8 + nEnd - nPos);
                            if( nLBox == (vsi_l_offset)8 + nEnd - nPos )
                            {
                                nLBox = CPL_MSBWORD32( nLBox );
                                VSIFWriteL(&nLBox, 1, 4, fp);
                                bOK = TRUE;
                            }
                        }
                        break;
                    }
                    if( nLBox < 8 )
                        break;
                    VSIFSeekL(fp, nLBox - 8, SEEK_CUR);
                }

                // Can write GMLJP2 box
                if( bOK )
                {
                    GDALJP2Metadata oJP2MD;
                    oJP2MD.SetSpatialRef( poSrcDS->GetSpatialRef() );
                    oJP2MD.SetGeoTransform( adfGeoTransform );
                    GDALJP2Box *poBox;
                    const char* pszGMLJP2V2Def = CSLFetchNameValue( papszOptions, "GMLJP2V2_DEF" );
                    if( pszGMLJP2V2Def != nullptr )
                        poBox = oJP2MD.CreateGMLJP2V2(nXSize,nYSize,pszGMLJP2V2Def,poSrcDS);
                    else
                        poBox = oJP2MD.CreateGMLJP2(nXSize,nYSize);

                    nLBox = (int) poBox->GetDataLength() + 8;
                    CPL_MSBPTR32( &nLBox );
                    memcpy(&nTBox, poBox->GetType(), 4);

                    VSIFSeekL(fp, 0, SEEK_END);
                    VSIFWriteL( &nLBox, 4, 1, fp );
                    VSIFWriteL( &nTBox, 4, 1, fp );
                    VSIFWriteL(poBox->GetWritableData(), 1, (int) poBox->GetDataLength(), fp);
                    VSIFCloseL(fp);

                    delete poBox;
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we need a world file?                                        */
/* -------------------------------------------------------------------- */
    if( CPLFetchBool( papszOptions, "WORLDFILE", false ) )
    {
        double      adfGeoTransform[6];

        poSrcDS->GetGeoTransform( adfGeoTransform );
        GDALWriteWorldFile( pszFilename, "wld", adfGeoTransform );
    }

/* -------------------------------------------------------------------- */
/*      Re-open dataset, and copy any auxiliary pam information.         */
/* -------------------------------------------------------------------- */
    GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);
    GDALPamDataset *poDS = (GDALPamDataset*) JPEG2000Dataset::Open(&oOpenInfo);

    if( poDS )
    {
        poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT & (~GCIF_METADATA) );

        /* Only write relevant metadata to PAM, and if needed */
        char** papszSrcMD = CSLDuplicate(poSrcDS->GetMetadata());
        papszSrcMD = CSLSetNameValue(papszSrcMD, GDALMD_AREA_OR_POINT, nullptr);
        papszSrcMD = CSLSetNameValue(papszSrcMD, "Corder", nullptr);
        for(char** papszSrcMDIter = papszSrcMD;
                papszSrcMDIter && *papszSrcMDIter; )
        {
            /* Remove entries like KEY= (without value) */
            if( (*papszSrcMDIter)[0] &&
                (*papszSrcMDIter)[strlen((*papszSrcMDIter))-1] == '=' )
            {
                CPLFree(*papszSrcMDIter);
                memmove(papszSrcMDIter, papszSrcMDIter + 1,
                        sizeof(char*) * (CSLCount(papszSrcMDIter + 1) + 1));
            }
            else
                ++papszSrcMDIter;
        }
        char** papszMD = CSLDuplicate(poDS->GetMetadata());
        papszMD = CSLSetNameValue(papszMD, GDALMD_AREA_OR_POINT, nullptr);
        if( papszSrcMD && papszSrcMD[0] != nullptr &&
            CSLCount(papszSrcMD) != CSLCount(papszMD) )
        {
            poDS->SetMetadata(papszSrcMD);
        }
        CSLDestroy(papszSrcMD);
        CSLDestroy(papszMD);
    }

    return poDS;
}

/************************************************************************/
/*                        GDALRegister_JPEG2000()                       */
/************************************************************************/

void GDALRegister_JPEG2000()

{
    if( !GDAL_CHECK_VERSION( "JPEG2000 driver" ) )
        return;

    if( GDALGetDriverByName( "JPEG2000" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "JPEG2000" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "JPEG-2000 part 1 (ISO/IEC 15444-1), "
                               "based on Jasper library" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/jpeg2000.html" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte Int16 UInt16 Int32 UInt32" );
    poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE, "image/jp2" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "jp2" );

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"   <Option name='1BIT_ALPHA_PROMOTION' type='boolean' description='Whether a 1-bit alpha channel should be promoted to 8-bit' default='YES'/>"
"   <Option name='OPEN_REMOTE_GML' type='boolean' description='Whether to load remote vector layers referenced by a link in a GMLJP2 v2 box' default='NO'/>"
"   <Option name='GEOREF_SOURCES' type='string' description='Comma separated list made with values INTERNAL/GMLJP2/GEOJP2/WORLDFILE/PAM/NONE that describe the priority order for georeferencing' default='PAM,GEOJP2,GMLJP2,WORLDFILE'/>"
"</OpenOptionList>" );

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='FORMAT' type='string-select' default='according to file extension. If unknown, default to J2K'>"
"       <Value>JP2</Value>"
"       <Value>JPC</Value>"
"   </Option>"
"   <Option name='GeoJP2' type='boolean' description='Whether to emit a GeoJP2 box' default='YES'/>"
"   <Option name='GMLJP2' type='boolean' description='Whether to emit a GMLJP2 v1 box' default='YES'/>"
"   <Option name='GMLJP2V2_DEF' type='string' description='Definition file to describe how a GMLJP2 v2 box should be generated. If set to YES, a minimal instance will be created'/>"
"   <Option name='WORLDFILE' type='boolean' description='Whether to write a worldfile .wld' default='NO'/>"
"   <Option name='NBITS' type='int' description='Bits (precision) for sub-byte files (1-7), sub-uint16 (9-15)'/>"
"   <Option name='imgareatlx' type='string' />"
"   <Option name='imgareatly' type='string' />"
"   <Option name='tilegrdtlx' type='string' />"
"   <Option name='tilegrdtly' type='string' />"
"   <Option name='tilewidth' type='string' />"
"   <Option name='tileheight' type='string' />"
"   <Option name='prcwidth' type='string' />"
"   <Option name='prcheight' type='string' />"
"   <Option name='cblkwidth' type='string' />"
"   <Option name='cblkheight' type='string' />"
"   <Option name='mode' type='string' />"
"   <Option name='rate' type='string' />"
"   <Option name='ilyrrates' type='string' />"
"   <Option name='prg' type='string' />"
"   <Option name='numrlvls' type='string' />"
"   <Option name='sop' type='string' />"
"   <Option name='eph' type='string' />"
"   <Option name='lazy' type='string' />"
"   <Option name='termall' type='string' />"
"   <Option name='segsym' type='string' />"
"   <Option name='vcausal' type='string' />"
"   <Option name='pterm' type='string' />"
"   <Option name='resetprob' type='string' />"
"   <Option name='numgbits' type='string' />"
"</CreationOptionList>"  );

    poDriver->pfnIdentify = JPEG2000Dataset::Identify;
    poDriver->pfnOpen = JPEG2000Dataset::Open;
    poDriver->pfnCreateCopy = JPEG2000CreateCopy;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
