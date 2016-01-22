#include <cpl_port.h>

#include "grib2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------- */
/* ==================================================================== */
/*      We prefer to use JasPer directly if it is available.  If not    */
/*      we fallback on calling back to GDAL to try and process the      */
/*      jpeg2000 chunks.                                                */
/* ==================================================================== */
/* -------------------------------------------------------------------- */

#ifdef HAVE_JASPER
#include <jasper/jasper.h>
#define JAS_1_700_2
#else
#include <gdal_pam.h>
#include <cpl_conv.h>
#endif

CPL_C_START
// Cripes ... shouldn't this go in an include files!
int dec_jpeg2000(char *injpc,g2int bufsize,g2int *outfld);
CPL_C_END

int dec_jpeg2000(char *injpc,g2int bufsize,g2int *outfld)
/*$$$  SUBPROGRAM DOCUMENTATION BLOCK
*                .      .    .                                       .
* SUBPROGRAM:    dec_jpeg2000      Decodes JPEG2000 code stream
*   PRGMMR: Gilbert          ORG: W/NP11     DATE: 2002-12-02
*
* ABSTRACT: This Function decodes a JPEG2000 code stream specified in the
*   JPEG2000 Part-1 standard (i.e., ISO/IEC 15444-1) using JasPer 
*   Software version 1.500.4 (or 1.700.2) written by the University of British
*   Columbia and Image Power Inc, and others.
*   JasPer is available at http://www.ece.uvic.ca/~mdadams/jasper/.
*
* PROGRAM HISTORY LOG:
* 2002-12-02  Gilbert
*
* USAGE:     int dec_jpeg2000(char *injpc,g2int bufsize,g2int *outfld)
*
*   INPUT ARGUMENTS:
*      injpc - Input JPEG2000 code stream.
*    bufsize - Length (in bytes) of the input JPEG2000 code stream.
*
*   OUTPUT ARGUMENTS:
*     outfld - Output matrix of grayscale image values.
*
*   RETURN VALUES :
*          0 = Successful decode
*         -3 = Error decode jpeg2000 code stream.
*         -5 = decoded image had multiple color components.
*              Only grayscale is expected.
*
* REMARKS:
*
*      Requires JasPer Software version 1.500.4 or 1.700.2
*
* ATTRIBUTES:
*   LANGUAGE: C
*   MACHINE:  IBM SP
*
*$$$*/

{
#ifndef HAVE_JASPER
    // J2K_SUBFILE method
    
    // create "memory file" from buffer
    int fileNumber = 0;
    VSIStatBufL   sStatBuf;
    CPLString osFileName = "/vsimem/work.jpc";

    // ensure we don't overwrite an existing file accidentally
    while ( VSIStatL( osFileName, &sStatBuf ) == 0 ) {
        osFileName.Printf( "/vsimem/work%d.jpc", ++fileNumber );
    }

    VSIFCloseL( VSIFileFromMemBuffer( 
                    osFileName, (unsigned char*)injpc, bufsize, 
                    FALSE ) ); // TRUE to let vsi delete the buffer when done

    // Open memory buffer for reading 
    GDALDataset* poJ2KDataset = (GDALDataset *)
        GDALOpen( osFileName, GA_ReadOnly );
 
    if( poJ2KDataset == NULL )
    {
        printf("dec_jpeg2000: Unable to open JPEG2000 image within GRIB file.\n"
                  "Is the JPEG2000 driver available?" );
        return -3;
    }

    if( poJ2KDataset->GetRasterCount() != 1 )
    {
       printf("dec_jpeg2000: Found color image.  Grayscale expected.\n");
       return (-5);
    }

    // Fulfill administration: initialize parameters required for RasterIO
    int nXSize = poJ2KDataset->GetRasterXSize();
    int nYSize = poJ2KDataset->GetRasterYSize();
    int nXOff = 0;
    int nYOff = 0;
    int nBufXSize = nXSize;
    int nBufYSize = nYSize;
    GDALDataType eBufType = GDT_Int32; // map to type of "outfld" buffer: g2int*
    int nBandCount = 1;
    int* panBandMap = NULL;
    int nPixelSpace = 0;
    int nLineSpace = 0;
    int nBandSpace = 0;

    //    Decompress the JPEG2000 into the output integer array.
    poJ2KDataset->RasterIO( GF_Read, nXOff, nYOff, nXSize, nYSize,
                            outfld, nBufXSize, nBufYSize, eBufType,
                            nBandCount, panBandMap, 
                            nPixelSpace, nLineSpace, nBandSpace );

    // close source file, and "unlink" it.
    GDALClose( poJ2KDataset );
    VSIUnlink( osFileName );

    return 0;

#else 

    // JasPer method
    
    int ier;
    g2int i,j,k;
    jas_image_t *image=0;
    jas_stream_t *jpcstream;
    jas_image_cmpt_t *pcmpt;
    char *opts=0;
    jas_matrix_t *data;

//    jas_init();

    ier=0;
//   
//     Create jas_stream_t containing input JPEG200 codestream in memory.
//       

    jpcstream=jas_stream_memopen(injpc,bufsize);

//   
//     Decode JPEG200 codestream into jas_image_t structure.
//       

    image=jpc_decode(jpcstream,opts);
    if ( image == 0 ) {
       printf(" jpc_decode return = %d \n",ier);
       return -3;
    }
    
    pcmpt=image->cmpts_[0];

//   Expecting jpeg2000 image to be grayscale only.
//   No color components.
//
    if (image->numcmpts_ != 1 ) {
       printf("dec_jpeg2000: Found color image.  Grayscale expected.\n");
       return (-5);
    }

// 
//    Create a data matrix of grayscale image values decoded from
//    the jpeg2000 codestream.
//
    data=jas_matrix_create(jas_image_height(image), jas_image_width(image));
    jas_image_readcmpt(image,0,0,0,jas_image_width(image),
                       jas_image_height(image),data);
//
//    Copy data matrix to output integer array.
//
    k=0;
    for (i=0;i<pcmpt->height_;i++) 
      for (j=0;j<pcmpt->width_;j++) 
        outfld[k++]=data->rows_[i][j];
//
//     Clean up JasPer work structures.
//
    jas_matrix_destroy(data);
    ier=jas_stream_close(jpcstream);
    jas_image_destroy(image);

    return 0;
#endif
}
