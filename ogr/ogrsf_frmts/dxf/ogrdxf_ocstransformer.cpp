/******************************************************************************
 *
 * Project:  DXF Translator
 * Purpose:  Implements the OCS to WCS transformer used in DXF files.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2018, Alan Thomas <alant@outlook.com.au>
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

#include "ogr_dxf.h"

/************************************************************************/
/*                         Mathematical helpers                         */
/************************************************************************/

static double Det2x2( double a, double b, double c, double d )
{
    return a*d - b*c;
}

static void CrossProduct(const double *a, const double *b, double *vResult) {
    vResult[0] = a[1] * b[2] - a[2] * b[1];
    vResult[1] = a[2] * b[0] - a[0] * b[2];
    vResult[2] = a[0] * b[1] - a[1] * b[0];
}

static void Scale2Unit(double* adfV) {
    double dfLen=sqrt(adfV[0]*adfV[0] + adfV[1]*adfV[1] + adfV[2]*adfV[2]);
    if (dfLen != 0)
    {
        adfV[0] /= dfLen;
        adfV[1] /= dfLen;
        adfV[2] /= dfLen;
    }
}

/************************************************************************/
/*                        OGRDXFOCSTransformer()                        */
/************************************************************************/

OGRDXFOCSTransformer::OGRDXFOCSTransformer( double adfNIn[3],
    bool bInverse /* = false */ ) :
    aadfInverse()
{
    static const double dSmall = 1.0 / 64.0;
    static const double adfWZ[3] = { 0.0, 0.0, 1.0 };
    static const double adfWY[3] = { 0.0, 1.0, 0.0 };

    dfDeterminant = 0.0;
    Scale2Unit( adfNIn );
    memcpy( adfN, adfNIn, sizeof(double)*3 );

    if ((std::abs(adfN[0]) < dSmall) && (std::abs(adfN[1]) < dSmall))
        CrossProduct(adfWY, adfN, adfAX);
    else
        CrossProduct(adfWZ, adfN, adfAX);

    Scale2Unit( adfAX );
    CrossProduct(adfN, adfAX, adfAY);
    Scale2Unit( adfAY );

    if( bInverse == true ) {
        const double a[4] = { 0.0, adfAX[0], adfAY[0], adfN[0] };
        const double b[4] = { 0.0, adfAX[1], adfAY[1], adfN[1] };
        const double c[4] = { 0.0, adfAX[2], adfAY[2], adfN[2] };

        dfDeterminant = a[1]*b[2]*c[3] - a[1]*b[3]*c[2]
            + a[2]*b[3]*c[1] - a[2]*b[1]*c[3]
            + a[3]*b[1]*c[2] - a[3]*b[2]*c[1];

        if( dfDeterminant != 0.0 ) {
            const double k = 1.0 / dfDeterminant;
            const double a11 = adfAX[0];
            const double a12 = adfAY[0];
            const double a13 = adfN[0];
            const double a21 = adfAX[1];
            const double a22 = adfAY[1];
            const double a23 = adfN[1];
            const double a31 = adfAX[2];
            const double a32 = adfAY[2];
            const double a33 = adfN[2];

            aadfInverse[1][1] = k * Det2x2( a22,a23,a32,a33 );
            aadfInverse[1][2] = k * Det2x2( a13,a12,a33,a32 );
            aadfInverse[1][3] = k * Det2x2( a12,a13,a22,a23 );

            aadfInverse[2][1] = k * Det2x2( a23,a21,a33,a31 );
            aadfInverse[2][2] = k * Det2x2( a11,a13,a31,a33 );
            aadfInverse[2][3] = k * Det2x2( a13,a11,a23,a21 );

            aadfInverse[3][1] = k * Det2x2( a21,a22,a31,a32 );
            aadfInverse[3][2] = k * Det2x2( a12,a11,a32,a31 );
            aadfInverse[3][3] = k * Det2x2( a11,a12,a21,a22 );
        }
    }
}

/************************************************************************/
/*                            Transform()                               */
/************************************************************************/

int OGRDXFOCSTransformer::Transform( int nCount,
    double *adfX, double *adfY, double *adfZ, double * /* adfT */,
    int *pabSuccess /* = nullptr */ ) 
{
    for( int i = 0; i < nCount; i++ )
    {
        const double x = adfX[i];
        const double y = adfY[i];
        const double z = adfZ[i];

        adfX[i] = x * adfAX[0] + y * adfAY[0] + z * adfN[0];
        adfY[i] = x * adfAX[1] + y * adfAY[1] + z * adfN[1];
        adfZ[i] = x * adfAX[2] + y * adfAY[2] + z * adfN[2];

        if( pabSuccess )
            pabSuccess[i] = TRUE;
    }
    return TRUE;
}

/************************************************************************/
/*                          InverseTransform()                          */
/************************************************************************/

int OGRDXFOCSTransformer::InverseTransform( int nCount,
    double *adfX, double *adfY, double *adfZ )
{
    if( dfDeterminant == 0.0 )
        return FALSE;

    for( int i = 0; i < nCount; i++ )
    {
        const double x = adfX[i];
        const double y = adfY[i];
        const double z = adfZ[i];

        adfX[i] = x * aadfInverse[1][1] + y * aadfInverse[1][2]
            + z * aadfInverse[1][3];
        adfY[i] = x * aadfInverse[2][1] + y * aadfInverse[2][2]
            + z * aadfInverse[2][3];
        adfZ[i] = x * aadfInverse[3][1] + y * aadfInverse[3][2]
            + z * aadfInverse[3][3];
    }
    return TRUE;
}

/************************************************************************/
/*                             ComposeOnto()                            */
/*                                                                      */
/*    Applies this transformer to the given affine transformer.         */
/************************************************************************/

void OGRDXFOCSTransformer::ComposeOnto( OGRDXFAffineTransform& oCT ) const
{
    double adfNew[12];

    adfNew[0] = adfAX[0] * oCT.adfData[0] +
        adfAY[0] * oCT.adfData[1] +
        adfN[0] * oCT.adfData[2];
    adfNew[1] = adfAX[1] * oCT.adfData[0] +
        adfAY[1] * oCT.adfData[1] +
        adfN[1] * oCT.adfData[2];
    adfNew[2] = adfAX[2] * oCT.adfData[0] +
        adfAY[2] * oCT.adfData[1] +
        adfN[2] * oCT.adfData[2];

    adfNew[3] = adfAX[0] * oCT.adfData[3] +
        adfAY[0] * oCT.adfData[4] +
        adfN[0] * oCT.adfData[5];
    adfNew[4] = adfAX[1] * oCT.adfData[3] +
        adfAY[1] * oCT.adfData[4] +
        adfN[1] * oCT.adfData[5];
    adfNew[5] = adfAX[2] * oCT.adfData[3] +
        adfAY[2] * oCT.adfData[4] +
        adfN[2] * oCT.adfData[5];

    adfNew[6] = adfAX[0] * oCT.adfData[6] +
        adfAY[0] * oCT.adfData[7] +
        adfN[0] * oCT.adfData[8];
    adfNew[7] = adfAX[1] * oCT.adfData[6] +
        adfAY[1] * oCT.adfData[7] +
        adfN[1] * oCT.adfData[8];
    adfNew[8] = adfAX[2] * oCT.adfData[6] +
        adfAY[2] * oCT.adfData[7] +
        adfN[2] * oCT.adfData[8];

    adfNew[9] = adfAX[0] * oCT.adfData[9] +
        adfAY[0] * oCT.adfData[10] +
        adfN[0] * oCT.adfData[11];
    adfNew[10] = adfAX[1] * oCT.adfData[9] +
        adfAY[1] * oCT.adfData[10] +
        adfN[1] * oCT.adfData[11];
    adfNew[11] = adfAX[2] * oCT.adfData[9] +
        adfAY[2] * oCT.adfData[10] +
        adfN[2] * oCT.adfData[11];

    memcpy( oCT.adfData, adfNew, sizeof(adfNew) );
}
