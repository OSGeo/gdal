/******************************************************************************
 *
 * Project:  CPL
 * Purpose:  Convert between VAX and IEEE floating point formats
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Avenza Systems Inc, http://www.avenza.com/
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

#include "cpl_port.h"
#include "cpl_vax.h"

CPL_CVSID("$Id$")

namespace {
typedef struct dbl {
    GUInt32 hi;
    GUInt32 lo;
} double64_t;
}

/************************************************************************/
/*                          CPLVaxToIEEEDouble()                        */
/************************************************************************/

void    CPLVaxToIEEEDouble(void * dbl)

{
    double64_t  dt;
    GUInt32     sign;
    int     exponent;
    GUInt32     rndbits;

/* -------------------------------------------------------------------- */
/*      Arrange the VAX double so that it may be accessed by a          */
/*      double64_t structure, (two GUInt32s).                           */
/* -------------------------------------------------------------------- */
    {
    const unsigned char *src =  static_cast<const unsigned char *>(dbl);
    unsigned char dest[8];
#ifdef CPL_LSB
    dest[2] = src[0];
    dest[3] = src[1];
    dest[0] = src[2];
    dest[1] = src[3];
    dest[6] = src[4];
    dest[7] = src[5];
    dest[4] = src[6];
    dest[5] = src[7];
#else
    dest[1] = src[0];
    dest[0] = src[1];
    dest[3] = src[2];
    dest[2] = src[3];
    dest[5] = src[4];
    dest[4] = src[5];
    dest[7] = src[6];
    dest[6] = src[7];
#endif
    memcpy(&dt, dest, 8);
    }

/* -------------------------------------------------------------------- */
/*      Save the sign of the double                                     */
/* -------------------------------------------------------------------- */
    sign         = dt.hi & 0x80000000;

/* -------------------------------------------------------------------- */
/*      Adjust the exponent so that we may work with it                 */
/* -------------------------------------------------------------------- */
    exponent = (dt.hi >> 23) & 0x000000ff;

    if (exponent)
        exponent = exponent -129 + 1023;

/* -------------------------------------------------------------------- */
/*      Save the bits that we are discarding so we can round properly   */
/* -------------------------------------------------------------------- */
    rndbits = dt.lo & 0x00000007;

    dt.lo = dt.lo >> 3;
    dt.lo = (dt.lo & 0x1fffffff) | (dt.hi << 29);

    if (rndbits)
        dt.lo = dt.lo | 0x00000001;

/* -------------------------------------------------------------------- */
/*      Shift the hi-order int over 3 and insert the exponent and sign  */
/* -------------------------------------------------------------------- */
    dt.hi = dt.hi >> 3;
    dt.hi = dt.hi & 0x000fffff;
    dt.hi = dt.hi | (static_cast<GUInt32>(exponent) << 20) | sign;

#ifdef CPL_LSB
/* -------------------------------------------------------------------- */
/*      Change the number to a byte swapped format                      */
/* -------------------------------------------------------------------- */
    const unsigned char* src = reinterpret_cast<const unsigned char *>(&dt);
    unsigned char* dest = static_cast<unsigned char *>(dbl);

    memcpy(dest + 0, src + 4, 4);
    memcpy(dest + 4, src + 0, 4);
#else
    memcpy( dbl, &dt, 8 );
#endif
}

/************************************************************************/
/*                         CPLIEEEToVaxDouble()                         */
/************************************************************************/

void    CPLIEEEToVaxDouble(void * dbl)

{
    double64_t dt;

#ifdef CPL_LSB
    {
    const GByte* src  = static_cast<const GByte *>(dbl);
    GByte dest[8];

    dest[0] = src[4];
    dest[1] = src[5];
    dest[2] = src[6];
    dest[3] = src[7];
    dest[4] = src[0];
    dest[5] = src[1];
    dest[6] = src[2];
    dest[7] = src[3];
    memcpy( &dt, dest, 8 );
    }
#else
    memcpy( &dt, dbl, 8 );
#endif

    GInt32 sign = dt.hi & 0x80000000;
    GInt32 exponent = dt.hi >> 20;
    exponent = exponent & 0x000007ff;

/* -------------------------------------------------------------------- */
/*      An exponent of zero means a zero value.                         */
/* -------------------------------------------------------------------- */
    if (exponent)
        exponent = exponent -1023+129;

/* -------------------------------------------------------------------- */
/*      In the case of overflow, return the largest number we can       */
/* -------------------------------------------------------------------- */
    if (exponent > 255)
    {
        GByte dest[8];

        if (sign)
            dest[1] = 0xff;
        else
            dest[1] = 0x7f;

        dest[0] = 0xff;
        dest[2] = 0xff;
        dest[3] = 0xff;
        dest[4] = 0xff;
        dest[5] = 0xff;
        dest[6] = 0xff;
        dest[7] = 0xff;
        memcpy( dbl, dest, 8 );

        return;
    }

/* -------------------------------------------------------------------- */
/*      In the case of of underflow return zero                         */
/* -------------------------------------------------------------------- */
    else if ((exponent < 0 ) ||
             (exponent == 0 && sign == 0))
    {
        memset( dbl, 0, 8 );

        return;
    }
    else
    {
/* -------------------------------------------------------------------- */
/*          Shift the fraction 3 bits left and set the exponent and sign*/
/* -------------------------------------------------------------------- */
        dt.hi = dt.hi << 3;
        dt.hi = dt.hi | (dt.lo >> 29);
        dt.hi = dt.hi & 0x007fffff;
        dt.hi = dt.hi | (exponent << 23) | sign;

        dt.lo = dt.lo << 3;
    }

/* -------------------------------------------------------------------- */
/*      Convert the double back to VAX format                           */
/* -------------------------------------------------------------------- */
    const GByte* src = reinterpret_cast<GByte *>(&dt);

#ifdef CPL_LSB
    GByte* dest = static_cast<GByte *>(dbl);
    memcpy(dest + 2, src + 0, 2);
    memcpy(dest + 0, src + 2, 2);
    memcpy(dest + 6, src + 4, 2);
    memcpy(dest + 4, src + 6, 2);
#else
    GByte dest[8];
    dest[1] = src[0];
    dest[0] = src[1];
    dest[3] = src[2];
    dest[2] = src[3];
    dest[5] = src[4];
    dest[4] = src[5];
    dest[7] = src[6];
    dest[6] = src[7];
    memcpy( dbl, dest, 8 );
#endif
}

//////////////////////////////////////////////////////////////////////////
/// Below code is adapted from Public Domain VICAR project
/// https://github.com/nasa/VICAR/blob/master/vos/rtl/source/conv_vax_ieee_r.c
//////////////////////////////////////////////////////////////////////////

static void real_byte_swap(const unsigned char from[4], unsigned char to[4])
{
   to[0] = from[1];
   to[1] = from[0];
   to[2] = from[3];
   to[3] = from[2];
}

/* Shift x[1]..x[3] right one bit by bytes, don't bother with x[0] */
#define SHIFT_RIGHT(x)                                      \
   { x[3] = ((x[3]>>1) & 0x7F) | ((x[2]<<7) & 0x80);        \
     x[2] = ((x[2]>>1) & 0x7F) | ((x[1]<<7) & 0x80);        \
     x[1] = (x[1]>>1) & 0x7F;                               \
   }

/* Shift x[1]..x[3] left one bit by bytes, don't bother with x[0] */
#define SHIFT_LEFT(x)                                       \
   { x[1] = ((x[1]<<1) & 0xFE) | ((x[2]>>7) & 0x01);        \
     x[2] = ((x[2]<<1) & 0xFE) | ((x[3]>>7) & 0x01);        \
     x[3] = (x[3]<<1) & 0xFE;                               \
   }

/************************************************************************/
/* Convert between IEEE and Vax single-precision floating point.        */
/* Both formats are represented as:                                     */
/* (-1)^s * f * 2^(e-bias)                                              */
/* where s is the sign bit, f is the mantissa (see below), e is the     */
/* exponent, and bias is the exponent bias (see below).                 */
/* There is an assumed leading 1 on the mantissa (except for IEEE       */
/* denormalized numbers), but the placement of the binary point varies. */
/*                                                                      */
/* IEEE format:    seeeeeee efffffff 8*f 8*f                            */
/*        where e is exponent with bias of 127 and f is of the          */
/*        form 1.fffff...                                               */
/* Special cases:                                                       */
/*    e=255, f!=0:        NaN (Not a Number)                            */
/*    e=255, f=0:        Infinity (+/- depending on s)                  */
/*    e=0, f!=0:        Denormalized numbers, of the form               */
/*                (-1)^s * (0.ffff) * 2^(-126)                          */
/*    e=0, f=0:        Zero (can be +/-)                                */
/*                                                                      */
/* VAX format:    seeeeeee efffffff 8*f 8*f                             */
/*        where e is exponent with bias of 128 and f is of the          */
/*        form .1fffff...                                               */
/* Byte swapping: Note that the above format is the logical format,     */
/*        which can be represented as bytes SE1 E2F1 F2 F3.             */
/*        The actual order in memory is E2F1 SE1 F3 F2 (which is        */
/*        two half-word swaps, NOT a full-word swap).                   */
/* Special cases:                                                       */
/*    e=0, s=0:        Zero (no +/-)                                    */
/*    e=0, s=1:        Invalid, causes Reserved Operand error           */
/*                                                                      */
/* The same code works on all byte-order machines because only byte     */
/* operations are performed.  It could perhaps be done more efficiently */
/* on a longword basis, but then the code would be byte-order dependent.*/
/* MAKE SURE any mods will work on either byte order!!!                 */
/************************************************************************/

/************************************************************************/
/* This routine will convert VAX F floating point values to IEEE        */
/* single precision floating point.                                     */
/************************************************************************/

static void vax_ieee_r(const unsigned char *from, unsigned char *ieee)
{
   unsigned char vaxf[4];
   unsigned char exp;

   real_byte_swap(from, vaxf);    /* Put bytes in rational order */
   memcpy(ieee, vaxf, 4);    /* Since most bits are the same */

   exp = ((vaxf[0]<<1)&0xFE) | ((vaxf[1]>>7)&0x01);

   if (exp == 0) {        /* Zero or invalid pattern */
      if (vaxf[0]&0x80) {    /* Sign bit set, which is illegal for VAX */
         ieee[0] = 0x7F;        /* IEEE NaN */
         ieee[1] = 0xFF;
         ieee[2] = 0xFF;
         ieee[3] = 0xFF;
      }
      else {            /* Zero */
         ieee[0] = ieee[1] = ieee[2] = ieee[3] = 0;
      }
   }

   else if (exp >= 3) {        /* Normal case */
      exp -= 2;
      ieee[0] = (vaxf[0]&0x80) | ((exp>>1)&0x7F);   /* remake sign + exponent */
   }            /* Low bit of exp can't change, so don't bother w/it */

   else if (exp == 2) {        /* Denormalize the number */
      SHIFT_RIGHT(ieee);    /* Which means shift right 1, */
      ieee[1] = (ieee[1] & 0x3F) | 0x40;   /* Add suppressed most signif bit, */
      ieee[0] = vaxf[0] & 0x80;    /* and set exponent to 0 (preserving sign) */
   }

   else {            /* Exp==1, denormalize again */
      SHIFT_RIGHT(ieee);    /* Like above but shift by 2 */
      SHIFT_RIGHT(ieee);
      ieee[1] = (ieee[1] & 0x1F) | 0x20;
      ieee[0] = vaxf[0] & 0x80;
   }

#ifdef CPL_LSB
   CPL_SWAP32PTR(ieee);
#endif
}


/************************************************************************/
/* This routine will convert IEEE single precision floating point       */
/* values to VAX F floating point.                                      */
/************************************************************************/

static void ieee_vax_r(unsigned char *ieee, unsigned char *to)
{
   unsigned char vaxf[4];
   unsigned char exp;

#ifdef CPL_LSB
   CPL_SWAP32PTR(ieee);
#endif

   memcpy(vaxf, ieee, 4);    /* Since most bits are the same */

   exp = ((ieee[0]<<1)&0xFE) | ((ieee[1]>>7)&0x01);

   /* Exponent 255 means NaN or Infinity, exponent 254 is too large for */
   /* VAX notation.  In either case, set to sign * highest possible number */

   if (exp == 255 || exp == 254) {        /* Infinity or NaN or too big */
      vaxf[0] = 0x7F | (ieee[0]&0x80);
      vaxf[1] = 0xFF;
      vaxf[2] = 0xFF;
      vaxf[3] = 0xFF;
   }

   else if (exp != 0) {        /* Normal case */
      exp += 2;
      vaxf[0] = (ieee[0]&0x80) | ((exp>>1)&0x7F);   /* remake sign + exponent */
   }            /* Low bit of exp can't change, so don't bother w/it */

   else {            /* exp == 0, zero or denormalized number */
      if (ieee[1] == 0 &&
      ieee[2] == 0 &&
      ieee[3] == 0) {        /* +/- 0 */
         vaxf[0] = vaxf[1] = vaxf[2] = vaxf[3] = 0;
      }
      else {            /* denormalized number */
         if (ieee[1] & 0x40) {    /* hi bit set (0.1ffff) */
            SHIFT_LEFT(vaxf);    /* Renormalize */
            vaxf[1] = vaxf[1] & 0x7F;    /* Set vax exponent to 2 */
            vaxf[0] = (ieee[0]&0x80) | 0x01;    /* sign, exponent==2 */
         }
         else if (ieee[1] & 0x20) {    /* next bit set (0.01ffff) */
            SHIFT_LEFT(vaxf);    /* Renormalize */
            SHIFT_LEFT(vaxf);
            vaxf[1] = vaxf[1] | 0x80;    /* Set vax exponent to 1 */
            vaxf[0] = ieee[0]&0x80;        /* sign, exponent==1 */
         }
         else {            /* Number too small for VAX */
            vaxf[0] = vaxf[1] = vaxf[2] = vaxf[3] = 0;    /* so set to 0 */
         }
      }
   }

   real_byte_swap(vaxf, to);    /* Put bytes in weird VAX order */
}


void CPLVaxToIEEEFloat( void * f )
{
    unsigned char res[4];
    vax_ieee_r( static_cast<const unsigned char*>(f), res );
    memcpy(f, res, 4);
}

void CPLIEEEToVaxFloat( void * f )
{
    unsigned char res[4];
    ieee_vax_r( static_cast<unsigned char*>(f), res );
    memcpy(f, res, 4);
}
