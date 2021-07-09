#ifndef __STDC_LIMIT_MACROS
// Needed on RHEL 6 for SIZE_MAX availability, needed by Jasper
#define __STDC_LIMIT_MACROS 1
#endif

#include "cpl_port.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gdal_pam.h"
#include "cpl_conv.h"

CPL_C_START
#include "grib2.h"
CPL_C_END

int dec_jpeg2000(const void *injpc,g2int bufsize,g2int **outfld,g2int outpixels)
/*$$$  SUBPROGRAM DOCUMENTATION BLOCK
*                .      .    .                                       .
* SUBPROGRAM:    dec_jpeg2000      Decodes JPEG2000 code stream
*   PRGMMR: Gilbert          ORG: W/NP11     DATE: 2002-12-02
*
* ABSTRACT: This Function decodes a JPEG2000 code stream specified in the
*   JPEG2000 Part-1 standard (i.e., ISO/IEC 15444-1) using a GDAL JPEG2000
*   capable driver.
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

* ATTRIBUTES:
*   LANGUAGE: C
*   MACHINE:  IBM SP
*
*$$$*/

{
    // create "memory file" from buffer
    CPLString osFileName;
    osFileName.Printf( "/vsimem/work_grib_%p.jpc", injpc );

    VSIFCloseL( VSIFileFromMemBuffer(
                    osFileName, (unsigned char*)injpc, bufsize,
                    FALSE ) ); // TRUE to let vsi delete the buffer when done

    // Open memory buffer for reading
    GDALDataset* poJ2KDataset = (GDALDataset *)
        GDALOpen( osFileName, GA_ReadOnly );

    if( poJ2KDataset == nullptr )
    {
        fprintf(stderr, "dec_jpeg2000: Unable to open JPEG2000 image within GRIB file.\n"
                  "Is the JPEG2000 driver available?" );
        VSIUnlink( osFileName );
        return -3;
    }

    if( poJ2KDataset->GetRasterCount() != 1 )
    {
       fprintf(stderr, "dec_jpeg2000: Found color image.  Grayscale expected.\n");
       GDALClose( poJ2KDataset );
       VSIUnlink( osFileName );
       return (-5);
    }

    // Fulfill administration: initialize parameters required for RasterIO
    const int nXSize = poJ2KDataset->GetRasterXSize();
    const int nYSize = poJ2KDataset->GetRasterYSize();
    // Do not test strict equality, since there are cases where the image
    // is actually smaller than the requested number of pixels
    if( nYSize == 0 || nXSize > outpixels / nYSize )
    {
        fprintf(stderr, "dec_jpeg2000: Image contains %ld pixels > %d.\n",
                (long)nXSize * nYSize, outpixels);
       GDALClose( poJ2KDataset );
       VSIUnlink( osFileName );
       return (-5);
    }
    // But on the other side if the image is much smaller than it is suspicious
    if( nXSize < outpixels / nYSize / 100 )
    {
        fprintf(stderr, "dec_jpeg2000: Image contains %ld pixels << %d.\n",
                (long)nXSize * nYSize, outpixels);
       GDALClose( poJ2KDataset );
       VSIUnlink( osFileName );
       return (-5);
    }
    *outfld=(g2int *)calloc(outpixels,sizeof(g2int));
    if ( *outfld == nullptr ) {
        fprintf(stderr, "Could not allocate space in jpcunpack.\n"
                "Data field NOT unpacked.\n");
        GDALClose( poJ2KDataset );
        VSIUnlink( osFileName );
        return(-5);
    }
    int nXOff = 0;
    int nYOff = 0;
    int nBufXSize = nXSize;
    int nBufYSize = nYSize;
    GDALDataType eBufType = GDT_Int32; // map to type of "outfld" buffer: g2int*
    int nBandCount = 1;
    int* panBandMap = nullptr;
    int nPixelSpace = 0;
    int nLineSpace = 0;
    int nBandSpace = 0;

    //    Decompress the JPEG2000 into the output integer array.
    const CPLErr eErr = poJ2KDataset->RasterIO( GF_Read, nXOff, nYOff, nXSize, nYSize,
                            *outfld, nBufXSize, nBufYSize, eBufType,
                            nBandCount, panBandMap,
                            nPixelSpace, nLineSpace, nBandSpace, nullptr );

    // close source file, and "unlink" it.
    GDALClose( poJ2KDataset );
    VSIUnlink( osFileName );

    return (eErr == CE_None) ? 0 : -3;
}
