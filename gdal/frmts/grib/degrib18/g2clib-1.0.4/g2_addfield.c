#include <stdio.h>
#include <stdlib.h>
#include "grib2.h"

g2int getdim(unsigned char *,g2int *,g2int *,g2int *);
g2int getpoly(unsigned char *,g2int *,g2int *,g2int *);
void simpack(g2float *, g2int, g2int *, unsigned char *, g2int *);
void cmplxpack(g2float *, g2int, g2int, g2int *, unsigned char *, g2int *);
void specpack(g2float *,g2int,g2int,g2int,g2int,g2int *,unsigned char *,
              g2int *);
void jpcpack(g2float *,g2int,g2int,g2int *,unsigned char *,g2int *);
#ifdef USE_PNG
  void pngpack(g2float *,g2int,g2int,g2int *,unsigned char *,g2int *);
#endif  /* USE_PNG */



g2int g2_addfield(unsigned char *cgrib,g2int ipdsnum,g2int *ipdstmpl,
                g2float *coordlist,g2int numcoord,g2int idrsnum,g2int *idrstmpl,
                g2float *fld,g2int ngrdpts,g2int ibmap,g2int *bmap)
//$$$  SUBPROGRAM DOCUMENTATION BLOCK
//                .      .    .                                       .
// SUBPROGRAM:    g2_addfield 
//   PRGMMR: Gilbert         ORG: W/NP11    DATE: 2002-11-05
//
// ABSTRACT: This routine packs up Sections 4 through 7 for a given field
//   and adds them to a GRIB2 message.  They are Product Definition Section,
//   Data Representation Section, Bit-Map Section and Data Section, 
//   respectively.
//   This routine is used with routines "g2_create", "g2_addlocal", 
//   "g2_addgrid", and "g2_gribend" to create a complete GRIB2 message.  
//   g2_create must be called first to initialize a new GRIB2 message.
//   Also, routine g2_addgrid must be called after g2_create and
//   before this routine to add the appropriate grid description to
//   the GRIB2 message.   Also, a call to g2_gribend is required to complete 
//   GRIB2 message after all fields have been added.
//
// PROGRAM HISTORY LOG:
// 2002-11-05  Gilbert
// 2002-12-23  Gilbert  -  Added complex spherical harmonic packing
// 2003-08-27  Gilbert  - Added support for new templates using
//                        PNG and JPEG2000 algorithms/templates.
// 2004-11-29  Gilbert  - JPEG2000 now allowed to use WMO Template no. 5.40
//                        PNG now allowed to use WMO Template no. 5.41
//                      - Added check to determine if packing algorithm failed.
// 2005-05-10  Gilbert -  Imposed minimum size on cpack, used to hold encoded
//                        bit string.
//
// USAGE:    int g2_addfield(unsigned char *cgrib,g2int ipdsnum,g2int *ipdstmpl,
//              g2float *coordlist,g2int numcoord,g2int idrsnum,g2int *idrstmpl,
//              g2float *fld,g2int ngrdpts,g2int ibmap,g2int *bmap)
//   INPUT ARGUMENT LIST:
//     cgrib    - Char array that contains the GRIB2 message to which sections
//                4 through 7 should be added.
//     ipdsnum  - Product Definition Template Number ( see Code Table 4.0)
//     ipdstmpl - Contains the data values for the specified Product Definition
//                Template ( N=ipdsnum ).  Each element of this integer 
//                array contains an entry (in the order specified) of Product
//                Defintion Template 4.N
//     coordlist- Array containg floating point values intended to document
//                the vertical discretisation associated to model data
//                on hybrid coordinate vertical levels.
//     numcoord - number of values in array coordlist.
//     idrsnum  - Data Representation Template Number ( see Code Table 5.0 )
//     idrstmpl - Contains the data values for the specified Data Representation
//                Template ( N=idrsnum ).  Each element of this integer 
//                array contains an entry (in the order specified) of Data
//                Representation Template 5.N
//                Note that some values in this template (eg. reference
//                values, number of bits, etc...) may be changed by the
//                data packing algorithms.
//                Use this to specify scaling factors and order of
//                spatial differencing, if desired.
//     fld[]    - Array of data points to pack.
//     ngrdpts  - Number of data points in grid.
//                i.e.  size of fld and bmap.
//     ibmap    - Bitmap indicator ( see Code Table 6.0 )
//                0 = bitmap applies and is included in Section 6.
//                1-253 = Predefined bitmap applies
//                254 = Previously defined bitmap applies to this field
//                255 = Bit map does not apply to this product.
//     bmap[]   - Integer array containing bitmap to be added. ( if ibmap=0 )
//
//   OUTPUT ARGUMENT LIST:      
//     cgrib    - Character array to contain the updated GRIB2 message.
//                Must be allocated large enough to store the entire
//                GRIB2 message.
//
//   RETURN VALUES:
//     ierr     - Return code.
//              > 0 = Current size of updated GRIB2 message
//               -1 = GRIB message was not initialized.  Need to call
//                    routine g2_create first.
//               -2 = GRIB message already complete.  Cannot add new section.
//               -3 = Sum of Section byte counts doesn't add to total byte count
//               -4 = Previous Section was not 3 or 7.
//               -5 = Could not find requested Product Definition Template.
//               -6 = Section 3 (GDS) not previously defined in message
//               -7 = Tried to use unsupported Data Representationi Template
//               -8 = Specified use of a previously defined bitmap, but one
//                    does not exist in the GRIB message.
//               -9 = GDT of one of 5.50 through 5.53 required to pack field
//                    using DRT 5.51.
//              -10 = Error packing data field.
//
// REMARKS: Note that the Sections 4 through 7 can only follow
//          Section 3 or Section 7 in a GRIB2 message.
//
// ATTRIBUTES:
//   LANGUAGE: C
//   MACHINE:  
//
//$$$
{
      g2int ierr;
      static unsigned char G=0x47;       // 'G'
      static unsigned char R=0x52;       // 'R'
      static unsigned char I=0x49;       // 'I'
      static unsigned char B=0x42;       // 'B'
      static unsigned char s7=0x37;   // '7'

      unsigned char *cpack;
      static g2int  zero=0,one=1,four=4,five=5,six=6,seven=7;
      const g2int  minsize=50000;
      g2int   iofst,ibeg,lencurr,len,nsize;
      g2int   ilen,isecnum,i,nbits,temp,left;
      g2int   ibmprev,j,lcpack,ioctet,newlen,ndpts;
      g2int   lensec4,lensec5,lensec6,lensec7;
      g2int   issec3,isprevbmap,lpos3=0,JJ,KK,MM;
      g2int   *coordieee;
      g2int   width,height,iscan,itemp;
      g2float *pfld;
      xxtemplate  *mappds,*mapdrs;
      unsigned int allones=4294967295u;
 
      ierr=0;
//
//  Check to see if beginning of GRIB message exists
//
      if ( cgrib[0]!=G || cgrib[1]!=R || cgrib[2]!=I || cgrib[3]!=B ) {
        printf("g2_addfield: GRIB not found in given message.\n");
        printf("g2_addfield: Call to routine g2_create required to initialize GRIB messge.\n");
        ierr=-1;
        return(ierr);
      }
//
//  Get current length of GRIB message
//  
      gbit(cgrib,&lencurr,96,32);
//
//  Check to see if GRIB message is already complete
//  
      if ( cgrib[lencurr-4]==s7 && cgrib[lencurr-3]==s7 &&
           cgrib[lencurr-2]==s7 && cgrib[lencurr-1]==s7 ) {
        printf("g2_addfield: GRIB message already complete.  Cannot add new section.\n");
        ierr=-2;
        return(ierr);
      }
//
//  Loop through all current sections of the GRIB message to
//  find the last section number.
//
      issec3=0;
      isprevbmap=0;
      len=16;    // length of Section 0
      for (;;) { 
      //    Get number and length of next section
        iofst=len*8;
        gbit(cgrib,&ilen,iofst,32);
        iofst=iofst+32;
        gbit(cgrib,&isecnum,iofst,8);
        iofst=iofst+8;
      //  Check if previous Section 3 exists
        if (isecnum == 3) {
            issec3=1;
            lpos3=len;
        }
      //  Check if a previous defined bitmap exists
        if (isecnum == 6) {
          gbit(cgrib,&ibmprev,iofst,8);
          iofst=iofst+8;
          if ((ibmprev >= 0) && (ibmprev <= 253)) isprevbmap=1;
        }
        len=len+ilen;
      //    Exit loop if last section reached
        if ( len == lencurr ) break;
      //    If byte count for each section doesn't match current
      //    total length, then there is a problem.
        if ( len > lencurr ) {
          printf("g2_addfield: Section byte counts don''t add to total.\n");
          printf("g2_addfield: Sum of section byte counts = %d\n",len);
          printf("g2_addfield: Total byte count in Section 0 = %d\n",lencurr);
          ierr=-3;
          return(ierr);
        }
      }
//
//  Sections 4 through 7 can only be added after section 3 or 7.
//
      if ( (isecnum != 3) && (isecnum != 7) ) {
        printf("g2_addfield: Sections 4-7 can only be added after Section 3 or 7.\n");
        printf("g2_addfield: Section ',isecnum,' was the last found in given GRIB message.\n");
        ierr=-4;
        return(ierr);
//
//  Sections 4 through 7 can only be added if section 3 was previously defined.
//
      }
      else if ( ! issec3) {
        printf("g2_addfield: Sections 4-7 can only be added if Section 3 was previously included.\n");
        printf("g2_addfield: Section 3 was not found in given GRIB message.\n");
        printf("g2_addfield: Call to routine addgrid required to specify Grid definition.\n");
        ierr=-6;
        return(ierr);
      }
//
//  Add Section 4  - Product Definition Section
//
      ibeg=lencurr*8;        //   Calculate offset for beginning of section 4
      iofst=ibeg+32;         //   leave space for length of section
      sbit(cgrib,&four,iofst,8);     // Store section number ( 4 )
      iofst=iofst+8;
      sbit(cgrib,&numcoord,iofst,16);   // Store num of coordinate values
      iofst=iofst+16;
      sbit(cgrib,&ipdsnum,iofst,16);    // Store Prod Def Template num.
      iofst=iofst+16;
      //
      //   Get Product Definition Template
      //
      mappds=getpdstemplate(ipdsnum);
      if (mappds == 0) {          // undefined template
        ierr=-5;
        return(ierr);
      }
      //
      //   Extend the Product Definition Template, if necessary.
      //   The number of values in a specific template may vary
      //   depending on data specified in the "static" part of the
      //   template.
      //
      if ( mappds->needext ) {
        free(mappds);
        mappds=extpdstemplate(ipdsnum,ipdstmpl);
      }
      //
      //   Pack up each input value in array ipdstmpl into the
      //   the appropriate number of octets, which are specified in
      //   corresponding entries in array mappds.
      //
      for (i=0;i<mappds->maplen;i++) {
        nbits=abs(mappds->map[i])*8;
        if ( (mappds->map[i] >= 0) || (ipdstmpl[i] >= 0) )
          sbit(cgrib,ipdstmpl+i,iofst,nbits);
        else {
          sbit(cgrib,&one,iofst,1);
          temp=abs(ipdstmpl[i]);
          sbit(cgrib,&temp,iofst+1,nbits-1);
        }
        iofst=iofst+nbits;
      }
      //  Pack template extension, if appropriate
      j=mappds->maplen;
      if ( mappds->needext && (mappds->extlen > 0) ) {
         for (i=0;i<mappds->extlen;i++) {
           nbits=abs(mappds->ext[i])*8;
           if ( (mappds->ext[i] >= 0) || (ipdstmpl[j] >= 0) )
             sbit(cgrib,ipdstmpl+j,iofst,nbits);
           else {
             sbit(cgrib,&one,iofst,1);
             temp=abs(ipdstmpl[j]);
             sbit(cgrib,&temp,iofst+1,nbits-1);
           }
           iofst=iofst+nbits;
           j++;
         }
      }
      free(mappds);
      //
      //   Add Optional list of vertical coordinate values
      //   after the Product Definition Template, if necessary.
      //
      if ( numcoord != 0 ) {
        coordieee=(g2int *)calloc(numcoord,sizeof(g2int));
        mkieee(coordlist,coordieee,numcoord);
        sbits(cgrib,coordieee,iofst,32,0,numcoord);
        iofst=iofst+(32*numcoord);
        free(coordieee);
      }
      //
      //   Calculate length of section 4 and store it in octets
      //   1-4 of section 4.
      //
      lensec4=(iofst-ibeg)/8;
      sbit(cgrib,&lensec4,ibeg,32);
//
//  Pack Data using appropriate algorithm
//
      //
      //   Get Data Representation Template
      //
      mapdrs=getdrstemplate(idrsnum);
      if (mapdrs == 0) {
        ierr=-5;
        return(ierr);
      }
      //
      //  contract data field, removing data at invalid grid points,
      //  if bit-map is provided with field.
      //
      if ( ibmap == 0 || ibmap==254 ) {
         pfld=(g2float *)malloc(ngrdpts*sizeof(g2float));
         ndpts=0;
         for (j=0;j<ngrdpts;j++) {
             if ( bmap[j]==1 ) pfld[ndpts++]=fld[j];
         }
      }
      else {
         ndpts=ngrdpts;
         pfld=fld;
      }
      nsize=ndpts*4;
      if ( nsize < minsize ) nsize=minsize;
      cpack=malloc(nsize);
      if (idrsnum == 0)           //  Simple Packing
        simpack(pfld,ndpts,idrstmpl,cpack,&lcpack);
      else if (idrsnum==2 || idrsnum==3)           //  Complex Packing
        cmplxpack(pfld,ndpts,idrsnum,idrstmpl,cpack,&lcpack);
      else if (idrsnum == 50) {         //  Sperical Harmonic Simple Packing 
        simpack(pfld+1,ndpts-1,idrstmpl,cpack,&lcpack);
        mkieee(pfld+0,idrstmpl+4,1);  // ensure RE(0,0) value is IEEE format
      }
      else if (idrsnum == 51) {         //  Sperical Harmonic Complex Packing 
        getpoly(cgrib+lpos3,&JJ,&KK,&MM);
        if ( JJ!=0 && KK!=0 && MM!=0 )
           specpack(pfld,ndpts,JJ,KK,MM,idrstmpl,cpack,&lcpack);
        else {
           printf("g2_addfield: Cannot pack DRT 5.51.\n");
           return (-9);
        }
      }
      else if (idrsnum == 40 || idrsnum == 40000) {    /*  JPEG2000 encoding  */
        if (ibmap == 255) {
           getdim(cgrib+lpos3,&width,&height,&iscan);
           if ( width==0 || height==0 ) {
              width=ndpts;
              height=1;
           }
           else if ( (unsigned int)width==allones || (unsigned int)height==allones ) {
              width=ndpts;
              height=1;
           }
           else if ( (iscan&32) == 32) {   /* Scanning mode: bit 3  */
              itemp=width;
              width=height;
              height=itemp;
           }
        }
        else {
           width=ndpts;
           height=1;
        }
        lcpack=nsize;
        jpcpack(pfld,width,height,idrstmpl,cpack,&lcpack);
      }
#ifdef USE_PNG
      else if (idrsnum == 41 || idrsnum == 40010) {      /*  PNG encoding   */
        if (ibmap == 255) {
           getdim(cgrib+lpos3,&width,&height,&iscan);
           if ( width==0 || height==0 ) {
              width=ndpts;
              height=1;
           }
           else if ( width==allones || height==allones ) {
              width=ndpts;
              height=1;
           }
           else if ( (iscan&32) == 32) {   /* Scanning mode: bit 3  */
              itemp=width;
              width=height;
              height=itemp;
           }
        }
        else {
           width=ndpts;
           height=1;
        }
        pngpack(pfld,width,height,idrstmpl,cpack,&lcpack);
      }
#endif  /* USE_PNG */
      else {
        printf("g2_addfield: Data Representation Template 5.%d not yet implemented.\n",idrsnum);
        ierr=-7;
        return(ierr);
      }
      if ( ibmap == 0 || ibmap==254 ) {      // free temp space
         if (fld != pfld) free(pfld);
      }
      if ( lcpack < 0 ) {
        if( cpack != 0 ) free(cpack);
        ierr=-10;
        return(ierr);
      }

//
//  Add Section 5  - Data Representation Section
//
      ibeg=iofst;            //   Calculate offset for beginning of section 5
      iofst=ibeg+32;         //   leave space for length of section
      sbit(cgrib,&five,iofst,8);     // Store section number ( 5 )
      iofst=iofst+8;
      sbit(cgrib,&ndpts,iofst,32);    // Store num of actual data points
      iofst=iofst+32;
      sbit(cgrib,&idrsnum,iofst,16);    // Store Data Repr. Template num.
      iofst=iofst+16;
      //
      //   Pack up each input value in array idrstmpl into the
      //   the appropriate number of octets, which are specified in
      //   corresponding entries in array mapdrs.
      //
      for (i=0;i<mapdrs->maplen;i++) {
        nbits=abs(mapdrs->map[i])*8;
        if ( (mapdrs->map[i] >= 0) || (idrstmpl[i] >= 0) )
          sbit(cgrib,idrstmpl+i,iofst,nbits);
        else {
          sbit(cgrib,&one,iofst,1);
          temp=abs(idrstmpl[i]);
          sbit(cgrib,&temp,iofst+1,nbits-1);
        }
        iofst=iofst+nbits;
      }
      free(mapdrs);
      //
      //   Calculate length of section 5 and store it in octets
      //   1-4 of section 5.
      //
      lensec5=(iofst-ibeg)/8;
      sbit(cgrib,&lensec5,ibeg,32);

//
//  Add Section 6  - Bit-Map Section
//
      ibeg=iofst;            //   Calculate offset for beginning of section 6
      iofst=ibeg+32;         //   leave space for length of section
      sbit(cgrib,&six,iofst,8);     // Store section number ( 6 )
      iofst=iofst+8;
      sbit(cgrib,&ibmap,iofst,8);    // Store Bit Map indicator
      iofst=iofst+8;
      //
      //  Store bitmap, if supplied
      //
      if (ibmap == 0) {
        sbits(cgrib,bmap,iofst,1,0,ngrdpts);    // Store BitMap
        iofst=iofst+ngrdpts;
      }
      //
      //  If specifying a previously defined bit-map, make sure
      //  one already exists in the current GRIB message.
      //
      if ((ibmap==254) && ( ! isprevbmap)) {
        printf("g2_addfield: Requested previously defined bitmap,");
        printf(" but one does not exist in the current GRIB message.\n");
        ierr=-8;
        return(ierr);
      }
      //
      //   Calculate length of section 6 and store it in octets
      //   1-4 of section 6.  Pad to end of octect, if necessary.
      //
      left=8-(iofst%8);
      if (left != 8) {
        sbit(cgrib,&zero,iofst,left);     // Pad with zeros to fill Octet
        iofst=iofst+left;
      }
      lensec6=(iofst-ibeg)/8;
      sbit(cgrib,&lensec6,ibeg,32);

//
//  Add Section 7  - Data Section
//
      ibeg=iofst;            //   Calculate offset for beginning of section 7
      iofst=ibeg+32;        //   leave space for length of section
      sbit(cgrib,&seven,iofst,8);    // Store section number ( 7 )
      iofst=iofst+8;
      //      Store Packed Binary Data values, if non-constant field
      if (lcpack != 0) {
        ioctet=iofst/8;
        //cgrib(ioctet+1:ioctet+lcpack)=cpack(1:lcpack)
        for (j=0;j<lcpack;j++) cgrib[ioctet+j]=cpack[j];
        iofst=iofst+(8*lcpack);
      }
      //
      //   Calculate length of section 7 and store it in octets
      //   1-4 of section 7.  
      //
      lensec7=(iofst-ibeg)/8;
      sbit(cgrib,&lensec7,ibeg,32);

      if( cpack != 0 ) free(cpack);
//
//  Update current byte total of message in Section 0
//
      newlen=lencurr+lensec4+lensec5+lensec6+lensec7;
      sbit(cgrib,&newlen,96,32);

      return(newlen);

}
