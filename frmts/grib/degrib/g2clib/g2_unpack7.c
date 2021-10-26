#include <float.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "grib2.h"

#ifdef USE_PNG
  g2int pngunpack(unsigned char *,g2int,g2int *,g2int, g2float *);
#endif  /* USE_PNG */

static float DoubleToFloatClamp(double val) {
   if (val >= FLT_MAX) return FLT_MAX;
   if (val <= -FLT_MAX) return -FLT_MAX;
   return (float)val;
}

g2int g2_unpack7(unsigned char *cgrib,g2int cgrib_length,g2int *iofst,g2int igdsnum,g2int *igdstmpl,
               g2int idrsnum,g2int *idrstmpl,g2int ndpts,g2float **fld)
//$$$  SUBPROGRAM DOCUMENTATION BLOCK
//                .      .    .                                       .
// SUBPROGRAM:    g2_unpack7
//   PRGMMR: Gilbert         ORG: W/NP11    DATE: 2002-10-31
//
// ABSTRACT: This subroutine unpacks Section 7 (Data Section)
//           as defined in GRIB Edition 2.
//
// PROGRAM HISTORY LOG:
// 2002-10-31  Gilbert
// 2002-12-20  Gilbert - Added GDT info to arguments
//                       and added 5.51 processing.
// 2003-08-29  Gilbert  - Added support for new templates using
//                        PNG and JPEG2000 algorithms/templates.
// 2004-11-29  Gilbert  - JPEG2000 now allowed to use WMO Template no. 5.40
//                        PNG now allowed to use WMO Template no. 5.41
// 2004-12-16  Taylor   - Added check on comunpack return code.
// 2008-12-23  Wesley   - Initialize Number of data points unpacked
//
// USAGE:    int g2_unpack7(unsigned char *cgrib,g2int *iofst,g2int igdsnum,
//                          g2int *igdstmpl, g2int idrsnum,
//                          g2int *idrstmpl, g2int ndpts,g2float **fld)
//   INPUT ARGUMENTS:
//     cgrib    - char array containing Section 7 of the GRIB2 message
//     iofst    - Bit offset of the beginning of Section 7 in cgrib.
//     igdsnum  - Grid Definition Template Number ( see Code Table 3.0)
//                ( Only used for DRS Template 5.51 )
//     igdstmpl - Pointer to an integer array containing the data values for
//                the specified Grid Definition
//                Template ( N=igdsnum ).  Each element of this integer
//                array contains an entry (in the order specified) of Grid
//                Definition Template 3.N
//                ( Only used for DRS Template 5.51 )
//     idrsnum  - Data Representation Template Number ( see Code Table 5.0)
//     idrstmpl - Pointer to an integer array containing the data values for
//                the specified Data Representation
//                Template ( N=idrsnum ).  Each element of this integer
//                array contains an entry (in the order specified) of Data
//                Representation Template 5.N
//     ndpts    - Number of data points unpacked and returned.
//
//   OUTPUT ARGUMENTS:
//     iofst    - Bit offset at the end of Section 7, returned.
//     fld      - Pointer to a float array containing the unpacked data field.
//
//   RETURN VALUES:
//     ierr     - Error return code.
//                0 = no error
//                2 = Not section 7
//                4 = Unrecognized Data Representation Template
//                5 = need one of GDT 3.50 through 3.53 to decode DRT 5.51
//                6 = memory allocation error
//                7 = corrupt section 7.
//
// REMARKS: None
//
// ATTRIBUTES:
//   LANGUAGE: C
//   MACHINE:
//
//$$$//
{
      g2int ierr,isecnum;
      g2int ipos,lensec;
      g2float *lfld;

      ierr=0;
      *fld=0;     //NULL

      gbit(cgrib,&lensec,*iofst,32);        // Get Length of Section
      *iofst=*iofst+32;
      gbit(cgrib,&isecnum,*iofst,8);         // Get Section Number
      *iofst=*iofst+8;

      if ( isecnum != 7 ) {
         ierr=2;
         //fprintf(stderr,"g2_unpack7: Not Section 7 data.\n");
         return(ierr);
      }

      ipos=(*iofst/8);
      if( ipos >= cgrib_length ) {
          return 7;
      }
      if (idrsnum == 40 || idrsnum == 40000) /* added by GDAL */
      {
          *fld= lfld = 0;
      }
      else
      {
        lfld=(g2float *)calloc(ndpts,sizeof(g2float));
        if (lfld == 0) {
            ierr=6;
            return(ierr);
        }
        else {
            *fld=lfld;
        }
      }

      if (idrsnum == 0)
        simunpack(cgrib+ipos,cgrib_length-ipos,idrstmpl,ndpts,lfld);
      else if (idrsnum == 2 || idrsnum == 3) {
        if (comunpack(cgrib+ipos,cgrib_length-ipos,lensec,idrsnum,idrstmpl,ndpts,lfld) != 0) {
          return 7;
        }
      }
      else if( idrsnum == 4 ) {
        // Grid point data - IEEE Floating Point Data
        static const int one = 1;
        int is_lsb = *((char*)&one) == 1;
        if (idrstmpl[0] == 1) {
          // IEEE754 single precision
          if( cgrib_length-ipos < 4 * ndpts )
             return 7;
          memcpy(lfld, cgrib+ipos, 4 * ndpts );
          if( is_lsb )
          {
              int i;
              unsigned char* ch_fld = (unsigned char*) lfld;
              for(i=0;i<ndpts;i++)
              {
                  unsigned char temp = ch_fld[i*4];
                  ch_fld[i*4] = ch_fld[i*4+3];
                  ch_fld[i*4+3] = temp;
                  temp = ch_fld[i*4+1];
                  ch_fld[i*4+1] = ch_fld[i*4+2];
                  ch_fld[i*4+2] = temp;
              }
          }
        }
        else if( idrstmpl[0] == 2) {
          // IEEE754 double precision
          // FIXME? due to the interface: we downgrade it to float
          int i;
          unsigned char* src = cgrib+ipos;
          if( cgrib_length-ipos < 8 * ndpts )
             return 7;
          if( is_lsb )
          {
              for(i=0;i<ndpts;i++)
              {
                  unsigned char temp[8];
                  double d;
                  {
                    int j;
                    for(j = 0; j < 8; j++ )
                      temp[j] = src[i * 8 + 7 - j];
                  }
                  memcpy(&d, temp, 8);
                  lfld[i] = DoubleToFloatClamp(d);
              }
          }
          else
          {
              for(i=0;i<ndpts;i++)
              {
                  double d;
                  memcpy(&d, src + i * 8, 8);
                  lfld[i] = DoubleToFloatClamp(d);
              }
          }
        }
        else
        {
            fprintf(stderr,"g2_unpack7: Invalid precision=%d for Data Section 5.4.\n", idrstmpl[0]);
        }
      }
      else if (idrsnum == 50) {            // Spectral Simple
        if( ndpts > 0 )
        {
            simunpack(cgrib+ipos,cgrib_length-ipos,idrstmpl,ndpts-1,lfld+1);
            rdieee(idrstmpl+4,lfld+0,1);
        }
      }
      else if (idrsnum == 51)              //  Spectral complex
        if ( igdsnum>=50 && igdsnum <=53 )
          specunpack(cgrib+ipos,idrstmpl,ndpts,igdstmpl[0],igdstmpl[2],igdstmpl[2],lfld);
        else {
          fprintf(stderr,"g2_unpack7: Cannot use GDT 3.%d to unpack Data Section 5.51.\n",(int)igdsnum);
          ierr=5;
          if ( lfld != 0 ) free(lfld);
          *fld=0;     //NULL
          return(ierr);
        }
      else if (idrsnum == 40 || idrsnum == 40000) {
        if( jpcunpack(cgrib+ipos,lensec-5,idrstmpl,ndpts,fld) != 0 )
        {
            ierr=7;
            if ( *fld != 0 ) free(*fld);
            *fld=0;     //NULL
            return(ierr);
        }
      }
#ifdef USE_PNG
      else if (idrsnum == 41 || idrsnum == 40010) {
        pngunpack(cgrib+ipos,lensec-5,idrstmpl,ndpts,lfld);
        }
#endif  /* USE_PNG */
      else {
        fprintf(stderr,"g2_unpack7: Data Representation Template 5.%d not yet implemented.\n",(int)idrsnum);
        ierr=4;
        if ( lfld != 0 ) free(lfld);
        *fld=0;     //NULL
        return(ierr);
      }

      *iofst=*iofst+(8*lensec);

      return(ierr);    // End of Section 7 processing

}
