#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "grib2.h"

#include "libaec.h"

// Cf https://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_doc/grib2_temp5-42.shtml
// and https://github.com/erget/wgrib2/commit/07b0f6fcb9669e0e3285318f50d516731b6956b2

g2int aecunpack(unsigned char *cpack,g2int len,g2int *idrstmpl,g2int ndpts,
                g2float *fld)
{

      g2int  iret = 0;
      g2float  refV;

      rdieee(idrstmpl+0,&refV,1);
      g2float bscale = (g2float)int_power(2.0,idrstmpl[1]);
      g2float dscale = (g2float)int_power(10.0,-idrstmpl[2]);
      g2float bdscale = bscale * dscale;
      g2float refD = refV * dscale;

      g2int nbits = idrstmpl[3];
//
//  if nbits equals 0, we have a constant field where the reference value
//  is the data value at each gridpoint
//
      if (nbits != 0) {
         int nbytes_per_sample = (nbits + 7) / 8;
         if( ndpts != 0 && nbytes_per_sample > INT_MAX / ndpts )
         {
             return 1;
         }
         g2int* ifld=(g2int *)calloc(ndpts,sizeof(g2int));
         // Was checked just before
         // coverity[integer_overflow,overflow_sink]
         unsigned char* ctemp=(unsigned char *)calloc((size_t)(ndpts) * nbytes_per_sample,1);
         if ( ifld == NULL || ctemp == NULL) {
            fprintf(stderr, "Could not allocate space in aecunpack.\n"
                    "Data field NOT unpacked.\n");
            free(ifld);
            free(ctemp);
            return(1);
         }

         struct aec_stream strm = {0};
         strm.flags = idrstmpl[5]; // CCSDS compression options mask
         strm.bits_per_sample = nbits;
         strm.block_size = idrstmpl[6];
         strm.rsi = idrstmpl[7]; // Restart interval

         strm.next_in = cpack;
         strm.avail_in = len;
         strm.next_out = ctemp;
         strm.avail_out = (size_t)(ndpts) * nbytes_per_sample;

         // Note: libaec doesn't seem to be very robust to invalid inputs...
         int status = aec_buffer_decode(&strm);
         if (status != AEC_OK)
         {
             fprintf(stderr, "aec_buffer_decode() failed with return code %d", status);
             iret = 1;
         }
         else
         {
             gbits(ctemp,ndpts * nbytes_per_sample,ifld,0,nbytes_per_sample*8,0,ndpts);
             g2int j;
             for (j=0;j<ndpts;j++) {
                fld[j] = refD + bdscale*(g2float)(ifld[j]);
             }
         }
         free(ctemp);
         free(ifld);
      }
      else {
         g2int j;
         for (j=0;j<ndpts;j++) fld[j]=refD;
      }

      return(iret);
}
