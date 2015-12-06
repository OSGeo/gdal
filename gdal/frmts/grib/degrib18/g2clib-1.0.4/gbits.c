#include "grib2.h"

void gbit(unsigned char *in,g2int *iout,g2int iskip,g2int nbyte)
{
      gbits(in,iout,iskip,nbyte,(g2int)0,(g2int)1);
}

void sbit(unsigned char *out,const g2int *in,g2int iskip,g2int nbyte)
{
      sbits(out,in,iskip,nbyte,(g2int)0,(g2int)1);
}


void gbits(unsigned char *in,g2int *iout,g2int iskip,g2int nbyte,g2int nskip,
           g2int n)
/*          Get bits - unpack bits:  Extract arbitrary size values from a
/          packed bit string, right justifying each value in the unpacked
/          iout array.
/           *in    = pointer to character array input
/           *iout  = pointer to unpacked array output
/            iskip = initial number of bits to skip
/            nbyte = number of bits to take
/            nskip = additional number of bits to skip on each iteration
/            n     = number of iterations
/ v1.1
*/
{
      g2int i,tbit,bitcnt,ibit,itmp;
      g2int nbit,l_index;
      static const g2int ones[]={1,3,7,15,31,63,127,255};

//     nbit is the start position of the field in bits
      nbit = iskip;
      for (i=0;i<n;i++) {
         bitcnt = nbyte;
         l_index=nbit/8;
         ibit=nbit%8;
         nbit = nbit + nbyte + nskip;

//        first byte
         tbit= ( bitcnt < (8-ibit) ) ? bitcnt : 8-ibit;  // find min
         itmp = (int)*(in+l_index) & ones[7-ibit];
         if (tbit != 8-ibit) itmp >>= (8-ibit-tbit);
         l_index++;
         bitcnt = bitcnt - tbit;

//        now transfer whole bytes
         while (bitcnt >= 8) {
             itmp = itmp<<8 | (int)*(in+l_index);
             bitcnt = bitcnt - 8;
             l_index++;
         }

//        get data from last byte
         if (bitcnt > 0) {
             itmp = ( itmp << bitcnt ) | ( ((int)*(in+l_index) >> (8-bitcnt)) & ones[bitcnt-1] );
         }

         *(iout+i) = itmp;
      }
}


void sbits(unsigned char *out,const g2int *in,g2int iskip,g2int nbyte,g2int nskip,
           g2int n)
/*C          Store bits - pack bits:  Put arbitrary size values into a
/          packed bit string, taking the low order bits from each value
/          in the unpacked array.
/           *iout  = pointer to packed array output
/           *in    = pointer to unpacked array input
/            iskip = initial number of bits to skip
/            nbyte = number of bits to pack
/            nskip = additional number of bits to skip on each iteration
/            n     = number of iterations
/ v1.1
*/
{
      g2int i,bitcnt,tbit,ibit,itmp,imask,itmp2,itmp3;
      g2int nbit,l_index;
      static const g2int ones[]={1,3,7,15,31,63,127,255};

//     number bits from zero to ...
//     nbit is the last bit of the field to be filled

      nbit = iskip + nbyte - 1;
      for (i=0;i<n;i++) {
         itmp = *(in+i);
         bitcnt = nbyte;
         l_index=nbit/8;
         ibit=nbit%8;
         nbit = nbit + nbyte + nskip;

//        make byte aligned 
         if (ibit != 7) {
             tbit= ( bitcnt < (ibit+1) ) ? bitcnt : ibit+1;  // find min
             imask = ones[tbit-1] << (7-ibit);
             itmp2 = (itmp << (7-ibit)) & imask;
             itmp3 = (int)*(out+l_index) & (255-imask);
             out[l_index] = (unsigned char)(itmp2 | itmp3);
             bitcnt = bitcnt - tbit;
             itmp = itmp >> tbit;
             l_index--;
         }

//        now byte aligned

//        do by bytes
         while (bitcnt >= 8) {
             out[l_index] = (unsigned char)(itmp & 255);
             itmp = itmp >> 8;
             bitcnt = bitcnt - 8;
             l_index--;
         }

//        do last byte

         if (bitcnt > 0) {
             itmp2 = itmp & ones[bitcnt-1];
             itmp3 = (int)*(out+l_index) & (255-ones[bitcnt-1]);
             out[l_index] = (unsigned char)(itmp2 | itmp3);
         }
      }

}
