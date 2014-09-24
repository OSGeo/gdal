/******************************************************************************
 * $Id$
 *
 * Project:  Microstation DGN Access Library
 * Purpose:  Functions for translating DGN floats into IEEE floats.
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

#include "dgnlibp.h"

CPL_CVSID("$Id$");

typedef struct dbl {
    GUInt32 hi;
    GUInt32 lo;
} double64_t;

/************************************************************************/
/*                           DGN2IEEEDouble()                           */
/************************************************************************/

void    DGN2IEEEDouble(void * dbl)

{
    double64_t  dt;
    GUInt32     sign;
    GUInt32     exponent;
    GUInt32     rndbits;
    unsigned char       *src;
    unsigned char       *dest;

/* -------------------------------------------------------------------- */
/*      Arrange the VAX double so that it may be accessed by a          */
/*      double64_t structure, (two GUInt32s).                           */
/* -------------------------------------------------------------------- */
    src =  (unsigned char *) dbl;
    dest = (unsigned char *) &dt;
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

/* -------------------------------------------------------------------- */
/*      Save the sign of the double                                     */
/* -------------------------------------------------------------------- */
    sign         = dt.hi & 0x80000000;

/* -------------------------------------------------------------------- */
/*      Adjust the exponent so that we may work with it                 */      
/* -------------------------------------------------------------------- */
    exponent = dt.hi >> 23;
    exponent = exponent & 0x000000ff;

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
    dt.hi = dt.hi | (exponent << 20) | sign;



#ifdef CPL_LSB
/* -------------------------------------------------------------------- */
/*      Change the number to a byte swapped format                      */
/* -------------------------------------------------------------------- */
    src = (unsigned char *) &dt;
    dest = (unsigned char *) dbl;

    dest[0] = src[4];
    dest[1] = src[5];
    dest[2] = src[6];
    dest[3] = src[7];
    dest[4] = src[0];
    dest[5] = src[1];
    dest[6] = src[2];
    dest[7] = src[3];
#else
    memcpy( dbl, &dt, 8 );
#endif
}

/************************************************************************/
/*                           IEEE2DGNDouble()                           */
/************************************************************************/

void    IEEE2DGNDouble(void * dbl)

{
    double64_t  dt;
    GInt32      exponent;
    GInt32      sign;
    GByte       *src,*dest;
        
#ifdef CPL_LSB
    src  = (GByte *) dbl;
    dest = (GByte *) &dt;

    dest[0] = src[4];
    dest[1] = src[5];
    dest[2] = src[6];
    dest[3] = src[7];
    dest[4] = src[0];
    dest[5] = src[1];
    dest[6] = src[2];
    dest[7] = src[3];
#else
    memcpy( &dt, dbl, 8 );
#endif

    sign         = dt.hi & 0x80000000;
    exponent = dt.hi >> 20;
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
        dest = (GByte *) dbl;

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

        return;
    }   

/* -------------------------------------------------------------------- */
/*      In the case of of underflow return zero                         */
/* -------------------------------------------------------------------- */
    else if ((exponent < 0 ) ||
             (exponent == 0 && sign == 0))
    {
        dest = (GByte *) dbl;

        dest[0] = 0x00;
        dest[1] = 0x00;
        dest[2] = 0x00;
        dest[3] = 0x00;
        dest[4] = 0x00;
        dest[5] = 0x00;
        dest[6] = 0x00;
        dest[7] = 0x00;

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
    src = (GByte *) &dt;
    dest = (GByte *) dbl;

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
}
