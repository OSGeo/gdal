/******************************************************************************
 * $Id$
 *
 * Project:  JPEG-2000
 * Purpose:  Partial implementation of the ISO/IEC 15444-1 standard
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2002, Andrey Kiselev <dron@ak4719.spb.edu>
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

#include "gdal_pam.h"
#include "cpl_string.h"
#include "gdaljp2metadata.h"

#include <jasper/jasper.h>
#include "jpeg2000_vsil_io.h"

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_JPEG2000(void);
CPL_C_END

// XXX: Part of code below extracted from the JasPer internal headers and
// must be in sync with JasPer version (this one works with JasPer 1.900.1)
#define JP2_FTYP_MAXCOMPATCODES 32
#define JP2_BOX_IHDR    0x69686472      /* Image Header */
#define JP2_BOX_BPCC    0x62706363      /* Bits Per Component */
#define	JP2_BOX_PCLR	0x70636c72	/* Palette */
#define JP2_BOX_UUID    0x75756964      /* UUID */
extern "C" {
typedef struct {
        uint_fast32_t magic;
} jp2_jp_t;
typedef struct {
        uint_fast32_t majver;
        uint_fast32_t minver;
        uint_fast32_t numcompatcodes;
        uint_fast32_t compatcodes[JP2_FTYP_MAXCOMPATCODES];
} jp2_ftyp_t;
typedef struct {
        uint_fast32_t width;
        uint_fast32_t height;
        uint_fast16_t numcmpts;
        uint_fast8_t bpc;
        uint_fast8_t comptype;
        uint_fast8_t csunk;
        uint_fast8_t ipr;
} jp2_ihdr_t;
typedef struct {
        uint_fast16_t numcmpts;
        uint_fast8_t *bpcs;
} jp2_bpcc_t;
typedef struct {
        uint_fast8_t method;
        uint_fast8_t pri;
        uint_fast8_t approx;
        uint_fast32_t csid;
        uint_fast8_t *iccp;
        int iccplen;
} jp2_colr_t;
typedef struct {
        uint_fast16_t numlutents;
        uint_fast8_t numchans;
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
        jp2_cdefchan_t *ents;
} jp2_cdef_t;
typedef struct {
        uint_fast16_t cmptno;
        uint_fast8_t map;
        uint_fast8_t pcol;
} jp2_cmapent_t;

typedef struct {
        uint_fast16_t numchans;
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
                jp2_jp_t jp;
                jp2_ftyp_t ftyp;
                jp2_ihdr_t ihdr;
                jp2_bpcc_t bpcc;
                jp2_colr_t colr;
                jp2_pclr_t pclr;
                jp2_cdef_t cdef;
                jp2_cmap_t cmap;
#ifdef HAVE_JASPER_UUID
                jp2_uuid_t uuid;
#endif
        } data;

} jp2_box_t;

typedef struct jp2_boxops_s {
        void (*init)(jp2_box_t *box);
        void (*destroy)(jp2_box_t *box);
        int (*getdata)(jp2_box_t *box, jas_stream_t *in);
        int (*putdata)(jp2_box_t *box, jas_stream_t *out);
        void (*dumpdata)(jp2_box_t *box, FILE *out);
} jp2_boxops_t;

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

class JPEG2000Dataset : public GDALPamDataset
{
    friend class JPEG2000RasterBand;

    jas_stream_t *psStream;
    jas_image_t *psImage;
    int         iFormat;

    char        *pszProjection;
    int         bGeoTransformValid;
    double      adfGeoTransform[6];
    int         nGCPCount;
    GDAL_GCP    *pasGCPList;

    int         bAlreadyDecoded;
    int         DecodeImage();

  public:
                JPEG2000Dataset();
                ~JPEG2000Dataset();
    
    static int           Identify( GDALOpenInfo * );
    static GDALDataset  *Open( GDALOpenInfo * );

    CPLErr              GetGeoTransform( double* );
    virtual const char  *GetProjectionRef(void);
    virtual int         GetGCPCount();
    virtual const char  *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();
};

/************************************************************************/
/* ==================================================================== */
/*                            JPEG2000RasterBand                        */
/* ==================================================================== */
/************************************************************************/

class JPEG2000RasterBand : public GDALPamRasterBand
{
    friend class JPEG2000Dataset;
    
    // NOTE: poDS may be altered for NITF/JPEG2000 files!
    JPEG2000Dataset     *poGDS;

    jas_matrix_t        *psMatrix;
    
    int                  iDepth;
    int                  bSignedness;

  public:

                JPEG2000RasterBand( JPEG2000Dataset *, int, int, int );
                ~JPEG2000RasterBand();
                
    virtual CPLErr IReadBlock( int, int, void * );
    virtual GDALColorInterp GetColorInterpretation();
};


/************************************************************************/
/*                           JPEG2000RasterBand()                       */
/************************************************************************/

JPEG2000RasterBand::JPEG2000RasterBand( JPEG2000Dataset *poDS, int nBand,
                int iDepth, int bSignedness )

{
    this->poDS = poDS;
    poGDS = poDS;
    this->nBand = nBand;
    this->iDepth = iDepth;
    this->bSignedness = bSignedness;

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
    nBlockXSize = MIN(256, poDS->nRasterXSize);
    nBlockYSize = MIN(256, poDS->nRasterYSize);
    psMatrix = jas_matrix_create(nBlockYSize, nBlockXSize);
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
    int nWidthToRead = MIN(nBlockXSize, poGDS->nRasterXSize - nBlockXOff * nBlockXSize);
    int nHeightToRead = MIN(nBlockYSize, poGDS->nRasterYSize - nBlockYOff * nBlockYSize);

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

JPEG2000Dataset::JPEG2000Dataset()
{
    psStream = NULL;
    psImage = NULL;
    nBands = 0;
    pszProjection = CPLStrdup("");
    nGCPCount = 0;
    pasGCPList = NULL;
    bGeoTransformValid = FALSE;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
    bAlreadyDecoded = FALSE;
    
    poDriver = (GDALDriver *)GDALGetDriverByName("JPEG2000");
}

/************************************************************************/
/*                            ~JPEG2000Dataset()                        */
/************************************************************************/

JPEG2000Dataset::~JPEG2000Dataset()

{
    FlushCache();

    if ( psStream )
        jas_stream_close( psStream );
    if ( psImage )
        jas_image_destroy( psImage );

    if ( pszProjection )
        CPLFree( pszProjection );
    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }
}

/************************************************************************/
/*                             DecodeImage()                            */
/************************************************************************/
int JPEG2000Dataset::DecodeImage()
{
    if (bAlreadyDecoded)
        return psImage != NULL;
        
    bAlreadyDecoded = TRUE;    
    if ( !( psImage = jas_image_decode(psStream, iFormat, 0) ) )
    {
        CPLDebug( "JPEG2000", "Unable to decode image. Format: %s, %d",
                  jas_image_fmttostr( iFormat ), iFormat );
        return FALSE;
    }
    
    /* Case of a JP2 image : check that the properties given by */
    /* the JP2 boxes match the ones of the code stream */
    if (nBands != 0)
    {
        if (nBands != jas_image_numcmpts( psImage ))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "The number of components indicated in the IHDR box (%d) mismatch "
                     "the value specified in the code stream (%d)",
                     nBands, jas_image_numcmpts( psImage ));
            jas_image_destroy( psImage );
            psImage = NULL;
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
            psImage = NULL;
            return FALSE;
        }
        
        int iBand;
        for ( iBand = 0; iBand < nBands; iBand++ )
        {
            JPEG2000RasterBand* poBand = (JPEG2000RasterBand*) GetRasterBand(iBand+1);
            if (poBand->iDepth != jas_image_cmptprec( psImage, iBand ) ||
                poBand->bSignedness != jas_image_cmptsgnd( psImage, iBand ))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "The bit depth of band %d indicated in the IHDR box (%d) mismatch "
                         "the value specified in the code stream (%d)",
                         iBand + 1, poBand->iDepth, jas_image_cmptprec( psImage, iBand ));
                jas_image_destroy( psImage );
                psImage = NULL;
                return FALSE;
            }
        }
    }
    
    /* Ask for YCbCr -> RGB translation */
    if ( jas_clrspc_fam( jas_image_clrspc( psImage ) ) == 
              JAS_CLRSPC_FAM_YCBCR )
    {
        jas_image_t *psRGBImage;
        jas_cmprof_t *psRGBProf;
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


/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *JPEG2000Dataset::GetProjectionRef()

{
    return( pszProjection );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr JPEG2000Dataset::GetGeoTransform( double * padfTransform )
{
    if( bGeoTransformValid )
    {
        memcpy( padfTransform, adfGeoTransform, sizeof(adfGeoTransform[0]) * 6 );
        return CE_None;
    }
    else
        return CE_Failure;
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int JPEG2000Dataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *JPEG2000Dataset::GetGCPProjection()

{
    if( nGCPCount > 0 )
        return pszProjection;
    else
        return "";
}

/************************************************************************/
/*                               GetGCP()                               */
/************************************************************************/

const GDAL_GCP *JPEG2000Dataset::GetGCPs()

{
    return pasGCPList;
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
    static const unsigned char jpc_header[] = {0xff,0x4f};
    static const unsigned char jp2_box_jp[] = {0x6a,0x50,0x20,0x20}; /* 'jP  ' */
        
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
    char        *pszFormatName = NULL;
    jas_stream_t *sS;
    
    if (!Identify(poOpenInfo))
        return NULL;

    JPEG2000Init();
    if( !(sS = JPEG2000_VSIL_fopen( poOpenInfo->pszFilename, "rb" )) )
    {
        return NULL;
    }

    iFormat = jas_image_getfmt( sS );
    if ( !(pszFormatName = jas_image_fmttostr( iFormat )) )
    {
        jas_stream_close( sS );
        return NULL;
    }
    if ( strlen( pszFormatName ) < 3 ||
        (!EQUALN( pszFormatName, "jp2", 3 ) &&
         !EQUALN( pszFormatName, "jpc", 3 ) &&
         !EQUALN( pszFormatName, "pgx", 3 )) )
    {
        CPLDebug( "JPEG2000", "JasPer reports file is format type `%s'.", 
                  pszFormatName );
        jas_stream_close( sS );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        jas_stream_close(sS);
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The JPEG2000 driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    JPEG2000Dataset     *poDS;
    int                 *paiDepth = NULL, *pabSignedness = NULL;
    int                 iBand;

    poDS = new JPEG2000Dataset();

    poDS->psStream = sS;
    poDS->iFormat = iFormat;

    if ( EQUALN( pszFormatName, "jp2", 3 ) )
    {
        // XXX: Hack to read JP2 boxes from input file. JasPer hasn't public
        // API call for such things, so we will use internal JasPer functions.
        jp2_box_t *box;
        box = 0;
        while ( ( box = jp2_box_get(poDS->psStream) ) )
        {
            switch (box->type)
            {
                case JP2_BOX_IHDR:
                poDS->nBands = box->data.ihdr.numcmpts;
                poDS->nRasterXSize = box->data.ihdr.width;
                poDS->nRasterYSize = box->data.ihdr.height;
                CPLDebug( "JPEG2000",
                          "IHDR box found. Dump: "
                          "width=%d, height=%d, numcmpts=%d, bpp=%d",
                          (int)box->data.ihdr.width, (int)box->data.ihdr.height,
                          (int)box->data.ihdr.numcmpts, (box->data.ihdr.bpc & 0x7F) + 1 );
                /* ISO/IEC 15444-1:2004 I.5.3.1 specifies that 255 means that all */
                /* components have not the same bit depth and/or sign and that a */
                /* BPCC box must then follow to specify them for each component */
                if ( box->data.ihdr.bpc != 255 )
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
            box = 0;
        }
        if( !paiDepth || !pabSignedness )
        {
            delete poDS;
            CPLDebug( "JPEG2000", "Unable to read JP2 header boxes.\n" );
            return NULL;
        }
        if ( jas_stream_rewind( poDS->psStream ) < 0 )
        {
            delete poDS;
            CPLDebug( "JPEG2000", "Unable to rewind input stream.\n" );
            return NULL;
        }
    }
    else
    {
        if ( !poDS->DecodeImage() )
        {
            delete poDS;
            return NULL;
        }

        poDS->nBands = jas_image_numcmpts( poDS->psImage );
        poDS->nRasterXSize = jas_image_cmptwidth( poDS->psImage, 0 );
        poDS->nRasterYSize = jas_image_cmptheight( poDS->psImage, 0 );
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
        return NULL;
    }

/* -------------------------------------------------------------------- */

/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( iBand = 1; iBand <= poDS->nBands; iBand++ )
    {
        poDS->SetBand( iBand, new JPEG2000RasterBand( poDS, iBand,
            paiDepth[iBand - 1], pabSignedness[iBand - 1] ) );
        
    }
    
    if ( paiDepth )
        CPLFree( paiDepth );
    if ( pabSignedness )
        CPLFree( pabSignedness );

/* -------------------------------------------------------------------- */
/*      Check for georeferencing information.                           */
/* -------------------------------------------------------------------- */
    GDALJP2Metadata oJP2Geo;
    
    if( oJP2Geo.ReadAndParse( poOpenInfo->pszFilename ) )
    {
        if ( poDS->pszProjection )
            CPLFree( poDS->pszProjection );
        poDS->pszProjection = CPLStrdup(oJP2Geo.pszProjection);
        poDS->bGeoTransformValid = oJP2Geo.bHaveGeoTransform;
        memcpy( poDS->adfGeoTransform, oJP2Geo.adfGeoTransform, 
                sizeof(double) * 6 );
        poDS->nGCPCount = oJP2Geo.nGCPCount;
        poDS->pasGCPList =
            GDALDuplicateGCPs( oJP2Geo.nGCPCount, oJP2Geo.pasGCPList );
    }

    if (oJP2Geo.pszXMPMetadata)
    {
        char *apszMDList[2];
        apszMDList[0] = (char *) oJP2Geo.pszXMPMetadata;
        apszMDList[1] = NULL;
        poDS->SetMetadata(apszMDList, "xml:XMP");
    }

/* -------------------------------------------------------------------- */
/*      Check for world file.                                           */
/* -------------------------------------------------------------------- */
    if( !poDS->bGeoTransformValid )
    {
        poDS->bGeoTransformValid |=
            GDALReadWorldFile2( poOpenInfo->pszFilename, NULL,
                                poDS->adfGeoTransform,
                                poOpenInfo->papszSiblingFiles, NULL )
            || GDALReadWorldFile2( poOpenInfo->pszFilename, ".wld",
                                   poDS->adfGeoTransform,
                                   poOpenInfo->papszSiblingFiles, NULL );
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );
    
    return( poDS );
}

/************************************************************************/
/*                      JPEG2000CreateCopy()                            */
/************************************************************************/

static GDALDataset *
JPEG2000CreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                    int bStrict, char ** papszOptions, 
                    GDALProgressFunc pfnProgress, void * pProgressData )

{
    int  nBands = poSrcDS->GetRasterCount();
    int  nXSize = poSrcDS->GetRasterXSize();
    int  nYSize = poSrcDS->GetRasterYSize();

    if( nBands == 0 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Unable to export files with zero bands." );
        return NULL;
    }

    if (poSrcDS->GetRasterBand(1)->GetColorTable() != NULL)
    {
        CPLError( (bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported, 
                  "JPEG2000 driver ignores color table. "
                  "The source raster band will be considered as grey level.\n"
                  "Consider using color table expansion (-expand option in gdal_translate)\n");
        if (bStrict)
            return NULL;
    }
    
    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */
    int                 iBand;
    jas_stream_t        *psStream;
    jas_image_t         *psImage;

    JPEG2000Init();
    const char* pszAccess = EQUALN(pszFilename, "/vsisubfile/", 12) ? "r+b" : "w+b";
    if( !(psStream = JPEG2000_VSIL_fopen( pszFilename, pszAccess) ) )
    {
        CPLError( CE_Failure, CPLE_FileIO, "Unable to create file %s.\n", 
                  pszFilename );
        return NULL;
    }
    
    if ( !(psImage = jas_image_create0()) )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, "Unable to create image %s.\n", 
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Loop over image, copying image data.                            */
/* -------------------------------------------------------------------- */
    GDALRasterBand      *poBand;
    GUInt32             *paiScanline;
    int                 iLine, iPixel;
    CPLErr              eErr = CE_None;
    jas_matrix_t        *psMatrix;
    jas_image_cmptparm_t *sComps; // Array of pointers to image components

    sComps = (jas_image_cmptparm_t*)
        CPLMalloc( nBands * sizeof(jas_image_cmptparm_t) );
  
    if ( !(psMatrix = jas_matrix_create( 1, nXSize )) )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, 
                  "Unable to create matrix with size %dx%d.\n", 1, nYSize );
        CPLFree( sComps );
        jas_image_destroy( psImage );
        return NULL;
    }
    paiScanline = (GUInt32 *) CPLMalloc( nXSize *
                            GDALGetDataTypeSize(GDT_UInt32) / 8 );
    
    for ( iBand = 0; iBand < nBands; iBand++ )
    {
        poBand = poSrcDS->GetRasterBand( iBand + 1);
        
        sComps[iBand].tlx = sComps[iBand].tly = 0;
        sComps[iBand].hstep = sComps[iBand].vstep = 1;
        sComps[iBand].width = nXSize;
        sComps[iBand].height = nYSize;
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
                              sizeof(GUInt32), sizeof(GUInt32) * nXSize );
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
                return NULL;
            }
            
            if( eErr == CE_None &&
            !pfnProgress( ((iLine + 1) + iBand * nYSize) /
                          ((double) nYSize * nBands),
                         NULL, pProgressData) )
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
    int             i, j;
    const int       OPTSMAX = 4096;
    const char      *pszFormatName;
    char            pszOptionBuf[OPTSMAX + 1];

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
        NULL
    };
    
    pszFormatName = CSLFetchNameValue( papszOptions, "FORMAT" );
    if ( !pszFormatName ||
         (!EQUALN( pszFormatName, "jp2", 3 ) &&
          !EQUALN( pszFormatName, "jpc", 3 ) ) )
        pszFormatName = "jp2";
    
    pszOptionBuf[0] = '\0';
    if ( papszOptions )
    {
        CPLDebug( "JPEG2000", "User supplied parameters:" );
        for ( i = 0; papszOptions[i] != NULL; i++ )
        {
            CPLDebug( "JPEG2000", "%s\n", papszOptions[i] );
            for ( j = 0; apszComprOptions[j] != NULL; j++ )
                if( EQUALN( apszComprOptions[j], papszOptions[i],
                            strlen(apszComprOptions[j]) ) )
                {
                    int m, n;

                    n = strlen( pszOptionBuf );
                    m = n + strlen( papszOptions[i] ) + 1;
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
        /* but if we explictely set the cmprof, it does not work better */
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
    if ( EQUALN( pszFormatName, "jp2", 3 ) )
    {
#ifdef HAVE_JASPER_UUID
        double  adfGeoTransform[6];
        if( ((poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None
                 && (adfGeoTransform[0] != 0.0 
                     || adfGeoTransform[1] != 1.0 
                     || adfGeoTransform[2] != 0.0 
                     || adfGeoTransform[3] != 0.0 
                     || adfGeoTransform[4] != 0.0 
                     || ABS(adfGeoTransform[5]) != 1.0))
                || poSrcDS->GetGCPCount() > 0) )
        {
            GDALJP2Metadata oJP2Geo;

            if( poSrcDS->GetGCPCount() > 0 )
            {
                oJP2Geo.SetProjection( poSrcDS->GetGCPProjection() );
                oJP2Geo.SetGCPs( poSrcDS->GetGCPCount(), poSrcDS->GetGCPs() );
            }
            else
            {
                oJP2Geo.SetProjection( poSrcDS->GetProjectionRef() );
                oJP2Geo.SetGeoTransform( adfGeoTransform );
            }

            GDALJP2Box *poBox = oJP2Geo.CreateJP2GeoTIFF();
            jp2_box_t  *box = jp2_box_create( JP2_BOX_UUID );
            memcpy( box->data.uuid.uuid, poBox->GetUUID(), 16 );
            box->data.uuid.datalen = poBox->GetDataLength() - 16;
            box->data.uuid.data =
                (uint_fast8_t *)jas_malloc( poBox->GetDataLength() - 16 );
            memcpy( box->data.uuid.data, poBox->GetWritableData() + 16,
                    poBox->GetDataLength() - 16 );
            delete poBox;
            poBox = NULL;

            if ( jp2_encode_uuid( psImage, psStream, pszOptionBuf, box) < 0 )
            {
                CPLError( CE_Failure, CPLE_FileIO,
                          "Unable to encode image %s.", pszFilename );
                jp2_box_destroy( box );
                jas_matrix_destroy( psMatrix );
                CPLFree( paiScanline );
                CPLFree( sComps );
                jas_image_destroy( psImage );
                return NULL;
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
                return NULL;
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
            return NULL;
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
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Do we need a world file?                                        */
/* -------------------------------------------------------------------- */
    if( CSLFetchBoolean( papszOptions, "WORLDFILE", FALSE ) )
    {
        double      adfGeoTransform[6];
        
        poSrcDS->GetGeoTransform( adfGeoTransform );
        GDALWriteWorldFile( pszFilename, "wld", adfGeoTransform );
    }

/* -------------------------------------------------------------------- */
/*      Re-open dataset, and copy any auxilary pam information.         */
/* -------------------------------------------------------------------- */
    GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);
    GDALPamDataset *poDS = (GDALPamDataset*) JPEG2000Dataset::Open(&oOpenInfo);

    if( poDS )
        poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );

    return poDS;
}

/************************************************************************/
/*                        GDALRegister_JPEG2000()                       */
/************************************************************************/

void GDALRegister_JPEG2000()

{
    GDALDriver  *poDriver;
    
    if (! GDAL_CHECK_VERSION("JPEG2000 driver"))
        return;

    if( GDALGetDriverByName( "JPEG2000" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "JPEG2000" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "JPEG-2000 part 1 (ISO/IEC 15444-1)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_jpeg2000.html" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte Int16 UInt16 Int32 UInt32" );
        poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE, "image/jp2" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "jp2" );
        
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnIdentify = JPEG2000Dataset::Identify;
        poDriver->pfnOpen = JPEG2000Dataset::Open;
        poDriver->pfnCreateCopy = JPEG2000CreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

