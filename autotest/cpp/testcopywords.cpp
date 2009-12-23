/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Test GDALCopyWords().
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2009, Even Rouault
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
 
 #include <gdal.h>

#define ASSERT(intype, inval, outtype, expected_outval, outval ) \
    if (fabs((double)outval - (double)expected_outval) > .1) { \
        printf("Test failed at line %d (intype=%s,inval=%f,outtype=%s,got %f expected %f) !!!\n", \
        __LINE__, GDALGetDataTypeName(intype), (double)inval, GDALGetDataTypeName(outtype), (double)outval, (double)expected_outval); bErr = TRUE; break; }
                              
#define TEST(intype, inval, invali, outtype, outval, outvali) do { \
    memset(pIn, 0xff, 64); \
    memset(pOut, 0xff, 64); \
    if (intype == GDT_Byte) \
    {\
        *(GByte*)(pIn) = (GByte)inval; \
        *(GByte*)(pIn + 32) = (GByte)inval; \
    } \
    else if (intype == GDT_Int16)\
    {\
        *(GInt16*)(pIn) = (GInt16)inval; \
        *(GInt16*)(pIn + 32) = (GInt16)inval; \
    } \
    else if (intype == GDT_UInt16)\
    {\
        *(GUInt16*)(pIn) = (GUInt16)inval; \
        *(GUInt16*)(pIn + 32) = (GUInt16)inval; \
    } \
    else if (intype == GDT_Int32)\
    {\
        *(GInt32*)(pIn) = (GInt32)inval; \
        *(GInt32*)(pIn + 32) = (GInt32)inval; \
    } \
    else if (intype == GDT_UInt32)\
    {\
        *(GUInt32*)(pIn) = (GUInt32)inval; \
        *(GUInt32*)(pIn + 32) = (GUInt32)inval; \
    } \
    else if (intype == GDT_Float32)\
    {\
        *(float*)(pIn) = inval; \
        *(float*)(pIn + 32) = inval; \
    } \
    else if (intype == GDT_Float64)\
    {\
        *(double*)(pIn) = inval; \
        *(double*)(pIn + 32) = inval; \
    } \
    else if (intype == GDT_CInt16)\
    {\
        ((GInt16*)(pIn))[0] = (GInt16)inval; \
        ((GInt16*)(pIn))[1] = (GInt16)invali; \
        ((GInt16*)(pIn + 32))[0] = (GInt16)inval; \
        ((GInt16*)(pIn + 32))[1] = (GInt16)invali; \
    } \
    else if (intype == GDT_CInt32)\
    {\
        ((GInt32*)(pIn))[0] = (GInt32)inval; \
        ((GInt32*)(pIn))[1] = (GInt32)invali; \
        ((GInt32*)(pIn + 32))[0] = (GInt32)inval; \
        ((GInt32*)(pIn + 32))[1] = (GInt32)invali; \
    } \
    else if (intype == GDT_CFloat32)\
    {\
        ((float*)(pIn))[0] = inval; \
        ((float*)(pIn))[1] = invali; \
        ((float*)(pIn + 32))[0] = inval; \
        ((float*)(pIn + 32))[1] = invali; \
    } \
    else if (intype == GDT_CFloat64)\
    {\
        ((double*)(pIn))[0] = inval; \
        ((double*)(pIn))[1] = invali; \
        ((double*)(pIn + 32))[0] = inval; \
        ((double*)(pIn + 32))[1] = invali; \
    } \
    GDALCopyWords(pIn, intype, 32, pOut, outtype, 32, 2); \
    if (outtype == GDT_Byte) \
    {\
        ASSERT(intype, inval, outtype, outval, *(GByte*)(pOut)); \
        ASSERT(intype, inval, outtype, outval, *(GByte*)(pOut + 32)); \
    } \
    else if (outtype == GDT_Int16)\
    {\
        ASSERT(intype, inval, outtype, outval, *(GInt16*)(pOut)); \
        ASSERT(intype, inval, outtype, outval, *(GInt16*)(pOut + 32)); \
    } \
    else if (outtype == GDT_UInt16)\
    {\
        ASSERT(intype, inval, outtype, outval, *(GUInt16*)(pOut)); \
        ASSERT(intype, inval, outtype, outval, *(GUInt16*)(pOut + 32)); \
    } \
    else if (outtype == GDT_Int32)\
    {\
        ASSERT(intype, inval, outtype, outval, *(GInt32*)(pOut)); \
        ASSERT(intype, inval, outtype, outval, *(GInt32*)(pOut + 32)); \
    } \
    else if (outtype == GDT_UInt32)\
    {\
        ASSERT(intype, inval, outtype, outval, *(GUInt32*)(pOut)); \
        ASSERT(intype, inval, outtype, outval, *(GUInt32*)(pOut + 32)); \
    } \
    else if (outtype == GDT_Float32)\
    {\
        ASSERT(intype, inval, outtype, outval, *(float*)(pOut)); \
        ASSERT(intype, inval, outtype, outval, *(float*)(pOut + 32)); \
    } \
    else if (outtype == GDT_Float64)\
    {\
        ASSERT(intype, inval, outtype, outval, *(double*)(pOut)); \
        ASSERT(intype, inval, outtype, outval, *(double*)(pOut + 32)); \
    } \
    else if (outtype == GDT_CInt16)\
    {\
        ASSERT(intype, inval, outtype, outval, ((GInt16*)(pOut))[0]); \
        ASSERT(intype, inval, outtype, outvali, ((GInt16*)(pOut))[1]); \
        ASSERT(intype, inval, outtype, outval, ((GInt16*)(pOut + 32))[0]); \
        ASSERT(intype, inval, outtype, outvali, ((GInt16*)(pOut + 32))[1]); \
    } \
    else if (outtype == GDT_CInt32)\
    {\
        ASSERT(intype, inval, outtype, outval, ((GInt32*)(pOut))[0]); \
        ASSERT(intype, inval, outtype, outvali, ((GInt32*)(pOut))[1]); \
        ASSERT(intype, inval, outtype, outval, ((GInt32*)(pOut + 32))[0]); \
        ASSERT(intype, inval, outtype, outvali, ((GInt32*)(pOut + 32))[1]); \
    } \
    else if (outtype == GDT_CFloat32)\
    {\
        ASSERT(intype, inval, outtype, outval, ((float*)(pOut))[0]); \
        ASSERT(intype, inval, outtype, outvali, ((float*)(pOut))[1]); \
        ASSERT(intype, inval, outtype, outval, ((float*)(pOut + 32))[0]); \
        ASSERT(intype, inval, outtype, outvali, ((float*)(pOut + 32))[1]); \
    } \
    else if (outtype == GDT_CFloat64)\
    {\
        ASSERT(intype, inval, outtype, outval, ((double*)(pOut))[0]); \
        ASSERT(intype, inval, outtype, outvali, ((double*)(pOut))[1]); \
        ASSERT(intype, inval, outtype, outval, ((double*)(pOut + 32))[0]); \
        ASSERT(intype, inval, outtype, outvali, ((double*)(pOut + 32))[1]); \
    } } while(0)

#define FROM_R(intype, inval, outtype, outval) TEST(intype, inval, 0, outtype, outval, 0)
#define FROM_C(intype, inval, invali, outtype, outval, outvali) TEST(intype, inval, invali, outtype, outval, outvali)

#define IS_UNSIGNED(x) (x == GDT_Byte || x == GDT_UInt16 || x == GDT_UInt32)
#define IS_FLOAT(x) (x == GDT_Float32 || x == GDT_Float64 || x == GDT_CFloat32 || x == GDT_CFloat64)

char pIn[64];
char pOut[64];
int bErr = FALSE;
int i;
GDALDataType outtype;

void check_GDT_Byte()
{
    /* GDT_Byte */
    for(outtype=GDT_Byte; outtype<=GDT_CFloat64;outtype = (GDALDataType)(outtype + 1))
    {
        FROM_R(GDT_Byte, 0, outtype, 0);
        FROM_R(GDT_Byte, 127, outtype, 127);
        FROM_R(GDT_Byte, 255, outtype, 255);
    }
}

void check_GDT_Int16()
{
    /* GDT_Int16 */
    FROM_R(GDT_Int16, -32000, GDT_Byte, 0); /* clamp */
    FROM_R(GDT_Int16, -32000, GDT_Int16, -32000);
    FROM_R(GDT_Int16, -32000, GDT_UInt16, 0); /* clamp */
    FROM_R(GDT_Int16, -32000, GDT_Int32, -32000);
    FROM_R(GDT_Int16, -32000, GDT_UInt32, 0); /* clamp */
    FROM_R(GDT_Int16, -32000, GDT_Float32, -32000);
    FROM_R(GDT_Int16, -32000, GDT_Float64, -32000);
    FROM_R(GDT_Int16, -32000, GDT_CInt16, -32000);
    FROM_R(GDT_Int16, -32000, GDT_CInt32, -32000);
    FROM_R(GDT_Int16, -32000, GDT_CFloat32, -32000);
    FROM_R(GDT_Int16, -32000, GDT_CFloat64, -32000);
    for(outtype=GDT_Byte; outtype<=GDT_CFloat64;outtype = (GDALDataType)(outtype + 1))
    {
        FROM_R(GDT_Int16, 127, outtype, 127);
    }
    
    FROM_R(GDT_Int16, 32000, GDT_Byte, 255); /* clamp */
    FROM_R(GDT_Int16, 32000, GDT_Int16, 32000);
    FROM_R(GDT_Int16, 32000, GDT_UInt16, 32000);
    FROM_R(GDT_Int16, 32000, GDT_Int32, 32000);
    FROM_R(GDT_Int16, 32000, GDT_UInt32, 32000);
    FROM_R(GDT_Int16, 32000, GDT_Float32, 32000);
    FROM_R(GDT_Int16, 32000, GDT_Float64, 32000);
    FROM_R(GDT_Int16, 32000, GDT_CInt16, 32000);
    FROM_R(GDT_Int16, 32000, GDT_CInt32, 32000);
    FROM_R(GDT_Int16, 32000, GDT_CFloat32, 32000);
    FROM_R(GDT_Int16, 32000, GDT_CFloat64, 32000);
}

void check_GDT_UInt16()
{
    /* GDT_UInt16 */
    for(outtype=GDT_Byte; outtype<=GDT_CFloat64;outtype = (GDALDataType)(outtype + 1))
    {
        FROM_R(GDT_UInt16, 0, outtype, 0);
        FROM_R(GDT_UInt16, 127, outtype, 127);
    }
    
    FROM_R(GDT_UInt16, 65000, GDT_Byte, 255); /* clamp */
    FROM_R(GDT_UInt16, 65000, GDT_Int16, 32767); /* clamp */
    FROM_R(GDT_UInt16, 65000, GDT_UInt16, 65000);
    FROM_R(GDT_UInt16, 65000, GDT_Int32, 65000);
    FROM_R(GDT_UInt16, 65000, GDT_UInt32, 65000);
    FROM_R(GDT_UInt16, 65000, GDT_Float32, 65000);
    FROM_R(GDT_UInt16, 65000, GDT_Float64, 65000);
    FROM_R(GDT_UInt16, 65000, GDT_CInt16, 32767); /* clamp */
    FROM_R(GDT_UInt16, 65000, GDT_CInt32, 65000);
    FROM_R(GDT_UInt16, 65000, GDT_CFloat32, 65000);
    FROM_R(GDT_UInt16, 65000, GDT_CFloat64, 65000);
}

void check_GDT_Int32()
{
    /* GDT_Int32 */
    FROM_R(GDT_Int32, -33000, GDT_Byte, 0); /* clamp */
    FROM_R(GDT_Int32, -33000, GDT_Int16, -32768); /* clamp */
    FROM_R(GDT_Int32, -33000, GDT_UInt16, 0); /* clamp */
    FROM_R(GDT_Int32, -33000, GDT_Int32, -33000); /* clamp */
    FROM_R(GDT_Int32, -33000, GDT_UInt32, 0); /* clamp */
    FROM_R(GDT_Int32, -33000, GDT_Float32, -33000);
    FROM_R(GDT_Int32, -33000, GDT_Float64, -33000);
    FROM_R(GDT_Int32, -33000, GDT_CInt16, -32768); /* clamp */
    FROM_R(GDT_Int32, -33000, GDT_CInt32, -33000);
    FROM_R(GDT_Int32, -33000, GDT_CFloat32, -33000);
    FROM_R(GDT_Int32, -33000, GDT_CFloat64, -33000);
    for(outtype=GDT_Byte; outtype<=GDT_CFloat64;outtype = (GDALDataType)(outtype + 1))
    {
        FROM_R(GDT_Int32, 127, outtype, 127);
    }
    
    FROM_R(GDT_Int32, 67000, GDT_Byte, 255); /* clamp */
    FROM_R(GDT_Int32, 67000, GDT_Int16, 32767);  /* clamp */
    FROM_R(GDT_Int32, 67000, GDT_UInt16, 65535);  /* clamp */
    FROM_R(GDT_Int32, 67000, GDT_Int32, 67000);
    FROM_R(GDT_Int32, 67000, GDT_UInt32, 67000);
    FROM_R(GDT_Int32, 67000, GDT_Float32, 67000);
    FROM_R(GDT_Int32, 67000, GDT_Float64, 67000);
    FROM_R(GDT_Int32, 67000, GDT_CInt16, 32767);  /* clamp */
    FROM_R(GDT_Int32, 67000, GDT_CInt32, 67000);
    FROM_R(GDT_Int32, 67000, GDT_CFloat32, 67000);
    FROM_R(GDT_Int32, 67000, GDT_CFloat64, 67000);
}

void check_GDT_UInt32()
{
    /* GDT_UInt32 */
    for(outtype=GDT_Byte; outtype<=GDT_CFloat64;outtype = (GDALDataType)(outtype + 1))
    {
        FROM_R(GDT_UInt32, 0, outtype, 0);
        FROM_R(GDT_UInt32, 127, outtype, 127);
    }
    
    FROM_R(GDT_UInt32, 3000000000U, GDT_Byte, 255); /* clamp */
    FROM_R(GDT_UInt32, 3000000000U, GDT_Int16, 32767);  /* clamp */
    FROM_R(GDT_UInt32, 3000000000U, GDT_UInt16, 65535);  /* clamp */
    FROM_R(GDT_UInt32, 3000000000U, GDT_Int32, 2147483647);  /* clamp */
    FROM_R(GDT_UInt32, 3000000000U, GDT_UInt32, 3000000000U);
    FROM_R(GDT_UInt32, 3000000000U, GDT_Float32, 3000000000U);
    FROM_R(GDT_UInt32, 3000000000U, GDT_Float64, 3000000000U);
    FROM_R(GDT_UInt32, 3000000000U, GDT_CInt16, 32767);  /* clamp */
    FROM_R(GDT_UInt32, 3000000000U, GDT_CInt32, 2147483647);  /* clamp */
    FROM_R(GDT_UInt32, 3000000000U, GDT_CFloat32, 3000000000U);
    FROM_R(GDT_UInt32, 3000000000U, GDT_CFloat64, 3000000000U);
}

void check_GDT_Float32and64()
{
    /* GDT_Float32 and GDT_Float64 */
    for(i=0;i<2;i++)
    {
        GDALDataType intype = (i == 0) ? GDT_Float32 : GDT_Float64;
        for(outtype=GDT_Byte; outtype<=GDT_CFloat64;outtype = (GDALDataType)(outtype + 1))
        {
            if (IS_FLOAT(outtype))
                FROM_R(intype, 127.1, outtype, 127.1);
            else
            {
                FROM_R(intype, 127.1, outtype, 127);
                FROM_R(intype, 127.9, outtype, 128);
            }
        }
        FROM_R(intype, -1, GDT_Byte, 0);
        FROM_R(intype, 256, GDT_Byte, 255);
        FROM_R(intype, -33000, GDT_Int16, -32768);
        FROM_R(intype, 33000, GDT_Int16, 32767);
        FROM_R(intype, -1, GDT_UInt16, 0);
        FROM_R(intype, 66000, GDT_UInt16, 65535);
        FROM_R(intype, -3000000000.0, GDT_Int32, INT_MIN);
        FROM_R(intype, 3000000000.0, GDT_Int32, 2147483647);
        FROM_R(intype, -1, GDT_UInt32, 0);
        FROM_R(intype, 5000000000.0, GDT_UInt32, 4294967295UL);
        FROM_R(intype, 5000000000.0, GDT_Float32, 5000000000.0);
        FROM_R(intype, -5000000000.0, GDT_Float32, -5000000000.0);
        FROM_R(intype, 5000000000.0, GDT_Float64, 5000000000.0);
        FROM_R(intype, -5000000000.0, GDT_Float64, -5000000000.0);
        FROM_R(intype, -33000, GDT_CInt16, -32768);
        FROM_R(intype, 33000, GDT_CInt16, 32767);
        FROM_R(intype, -3000000000.0, GDT_CInt32, INT_MIN);
        FROM_R(intype, 3000000000.0, GDT_CInt32, 2147483647);
        FROM_R(intype, 5000000000.0, GDT_CFloat32, 5000000000.0);
        FROM_R(intype, -5000000000.0, GDT_CFloat32, -5000000000.0);
        FROM_R(intype, 5000000000.0, GDT_CFloat64, 5000000000.0);
        FROM_R(intype, -5000000000.0, GDT_CFloat64, -5000000000.0);
    }
}

void check_GDT_CInt16()
{
    /* GDT_CInt16 */
    FROM_C(GDT_CInt16, -32000, -32500, GDT_Byte, 0, 0); /* clamp */
    FROM_C(GDT_CInt16, -32000, -32500, GDT_Int16, -32000, 0);
    FROM_C(GDT_CInt16, -32000, -32500, GDT_UInt16, 0, 0); /* clamp */
    FROM_C(GDT_CInt16, -32000, -32500, GDT_Int32, -32000, 0);
    FROM_C(GDT_CInt16, -32000, -32500, GDT_UInt32, 0,0); /* clamp */
    FROM_C(GDT_CInt16, -32000, -32500, GDT_Float32, -32000, 0);
    FROM_C(GDT_CInt16, -32000, -32500, GDT_Float64, -32000, 0);
    FROM_C(GDT_CInt16, -32000, -32500, GDT_CInt16, -32000, -32500);
    FROM_C(GDT_CInt16, -32000, -32500, GDT_CInt32, -32000, -32500);
    FROM_C(GDT_CInt16, -32000, -32500, GDT_CFloat32, -32000, -32500);
    FROM_C(GDT_CInt16, -32000, -32500, GDT_CFloat64, -32000, -32500);
    for(outtype=GDT_Byte; outtype<=GDT_CFloat64;outtype = (GDALDataType)(outtype + 1))
    {
        FROM_C(GDT_CInt16, 127, 128, outtype, 127, 128);
    }
    
    FROM_C(GDT_CInt16, 32000, 32500, GDT_Byte, 255, 0); /* clamp */
    FROM_C(GDT_CInt16, 32000, 32500, GDT_Int16, 32000, 0);
    FROM_C(GDT_CInt16, 32000, 32500, GDT_UInt16, 32000, 0);
    FROM_C(GDT_CInt16, 32000, 32500, GDT_Int32, 32000, 0);
    FROM_C(GDT_CInt16, 32000, 32500, GDT_UInt32, 32000, 0);
    FROM_C(GDT_CInt16, 32000, 32500, GDT_Float32, 32000, 0);
    FROM_C(GDT_CInt16, 32000, 32500, GDT_Float64, 32000, 0);
    FROM_C(GDT_CInt16, 32000, 32500, GDT_CInt16, 32000, 32500);
    FROM_C(GDT_CInt16, 32000, 32500, GDT_CInt32, 32000, 32500);
    FROM_C(GDT_CInt16, 32000, 32500, GDT_CFloat32, 32000, 32500);
    FROM_C(GDT_CInt16, 32000, 32500, GDT_CFloat64, 32000, 32500);
}

void check_GDT_CInt32()
{
    /* GDT_CInt32 */
    FROM_C(GDT_CInt32, -33000, -33500, GDT_Byte, 0, 0); /* clamp */
    FROM_C(GDT_CInt32, -33000, -33500, GDT_Int16, -32768, 0); /* clamp */
    FROM_C(GDT_CInt32, -33000, -33500, GDT_UInt16, 0, 0); /* clamp */
    FROM_C(GDT_CInt32, -33000, -33500, GDT_Int32, -33000, 0);
    FROM_C(GDT_CInt32, -33000, -33500, GDT_UInt32, 0,0); /* clamp */
    FROM_C(GDT_CInt32, -33000, -33500, GDT_Float32, -33000, 0);
    FROM_C(GDT_CInt32, -33000, -33500, GDT_Float64, -33000, 0);
    FROM_C(GDT_CInt32, -33000, -33500, GDT_CInt16, -32768, -32768); /* clamp */
    FROM_C(GDT_CInt32, -33000, -33500, GDT_CInt32, -33000, -33500);
    FROM_C(GDT_CInt32, -33000, -33500, GDT_CFloat32, -33000, -33500);
    FROM_C(GDT_CInt32, -33000, -33500, GDT_CFloat64, -33000, -33500);
    for(outtype=GDT_Byte; outtype<=GDT_CFloat64;outtype = (GDALDataType)(outtype + 1))
    {
        FROM_C(GDT_CInt32, 127, 128, outtype, 127, 128);
    }
    
    FROM_C(GDT_CInt32, 67000, 67500, GDT_Byte, 255, 0); /* clamp */
    FROM_C(GDT_CInt32, 67000, 67500, GDT_Int16, 32767, 0); /* clamp */
    FROM_C(GDT_CInt32, 67000, 67500, GDT_UInt16, 65535, 0); /* clamp */
    FROM_C(GDT_CInt32, 67000, 67500, GDT_Int32, 67000, 0);
    FROM_C(GDT_CInt32, 67000, 67500, GDT_UInt32, 67000, 0);
    FROM_C(GDT_CInt32, 67000, 67500, GDT_Float32, 67000, 0);
    FROM_C(GDT_CInt32, 67000, 67500, GDT_Float64, 67000, 0);
    FROM_C(GDT_CInt32, 67000, 67500, GDT_CInt16, 32767, 32767); /* clamp */
    FROM_C(GDT_CInt32, 67000, 67500, GDT_CInt32, 67000, 67500);
    FROM_C(GDT_CInt32, 67000, 67500, GDT_CFloat32, 67000, 67500);
    FROM_C(GDT_CInt32, 67000, 67500, GDT_CFloat64, 67000, 67500);
}

void check_GDT_CFloat32and64()
{
    /* GDT_CFloat32 and GDT_CFloat64 */
    for(i=0;i<2;i++)
    {
        GDALDataType intype = (i == 0) ? GDT_CFloat32 : GDT_CFloat64;
        for(outtype=GDT_Byte; outtype<=GDT_CFloat64;outtype = (GDALDataType)(outtype + 1))
        {
            if (IS_FLOAT(outtype))
                FROM_C(intype, 127.1, 127.9, outtype, 127.1, 127.9);
            else
            {
                FROM_C(intype, 127.1, 150.9, outtype, 127, 151);
                FROM_C(intype, 127.9, 150.1, outtype, 128, 150);
            }
        }
        FROM_C(intype, -1, 256, GDT_Byte, 0, 0);
        FROM_C(intype, 256, -1, GDT_Byte, 255, 0);
        FROM_C(intype, -33000, 33000, GDT_Int16, -32768, 0);
        FROM_C(intype, 33000, -33000, GDT_Int16, 32767, 0);
        FROM_C(intype, -1, 66000, GDT_UInt16, 0, 0);
        FROM_C(intype, 66000, -1, GDT_UInt16, 65535, 0);
        FROM_C(intype, -3000000000.0, -3000000000.0, GDT_Int32, INT_MIN, 0);
        FROM_C(intype, 3000000000.0, 3000000000.0, GDT_Int32, 2147483647, 0);
        FROM_C(intype, -1, 5000000000.0, GDT_UInt32, 0, 0);
        FROM_C(intype, 5000000000.0, -1, GDT_UInt32, 4294967295UL, 0);
        FROM_C(intype, 5000000000.0, -1, GDT_Float32, 5000000000.0, 0);
        FROM_C(intype, 5000000000.0, -1, GDT_Float64, 5000000000.0, 0);
        FROM_C(intype, -5000000000.0, -1, GDT_Float32, -5000000000.0, 0);
        FROM_C(intype, -5000000000.0, -1, GDT_Float64, -5000000000.0, 0);
        FROM_C(intype, -33000, 33000, GDT_CInt16, -32768, 32767);
        FROM_C(intype, 33000, -33000, GDT_CInt16, 32767, -32768);
        FROM_C(intype, -3000000000.0, -3000000000.0, GDT_CInt32, INT_MIN, INT_MIN);
        FROM_C(intype, 3000000000.0, 3000000000.0, GDT_CInt32, 2147483647, 2147483647);
        FROM_C(intype, 5000000000.0, -5000000000.0, GDT_CFloat32, 5000000000.0, -5000000000.0);
        FROM_C(intype, 5000000000.0, -5000000000.0, GDT_CFloat64, 5000000000.0, -5000000000.0);
    }
}

int main(int argc, char* argv[])
{
    check_GDT_Byte();
    check_GDT_Int16();
    check_GDT_UInt16();
    check_GDT_Int32();
    check_GDT_UInt32();
    check_GDT_Float32and64();
    check_GDT_CInt16();
    check_GDT_CInt32();
    check_GDT_CFloat32and64();
    
    if (bErr == FALSE)
        printf("success !\n");
    else
        printf("fail !\n");
    
    return (bErr == FALSE) ? 0 : -1;
}
