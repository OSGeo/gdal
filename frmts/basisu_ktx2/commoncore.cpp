/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Basis Universal / KTX2 driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
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

#include "commoncore.h"

/************************************************************************/
/*                  GDAL_KTX2_BASISU_GetCreationOptions()               */
/************************************************************************/

std::string GDAL_KTX2_BASISU_GetCreationOptions(bool bIsKTX2)
{
    std::string osRet =
        "<CreationOptionList>"
        "   <Option name='COMPRESSION' type='string-select' default='ETC1S'>"
        "       <Value>ETC1S</Value>"
        "       <Value>UASTC</Value>"
        "   </Option>";
    if (bIsKTX2)
        osRet += "   <Option name='UASTC_SUPER_COMPRESSION' "
                 "type='string-select' default='ZSTD'>"
                 "       <Value>NONE</Value>"
                 "       <Value>ZSTD</Value>"
                 "   </Option>";
    osRet +=
        "   <Option name='UASTC_LEVEL' type='int' min='0' max='4' default='2' "
        "description='The higher value, the higher the quality but the slower "
        "computing time. 4 is impractically slow'/>"
        "   <Option name='UASTC_RDO_LEVEL' type='float' min='0' default='1' "
        "description='Rate distortion optimization level. "
        "The lower value, the higher the quality, but the larger the file "
        "size. "
        "Usual range is [0.2,3]'/>"
        "   <Option name='ETC1S_LEVEL' type='int' min='0' max='6' default='1' "
        "description='The higher value, the higher the quality but the slower "
        "computing time.'/>"
        "   <Option name='ETC1S_QUALITY_LEVEL' type='int' min='1' max='255' "
        "default='128' "
        "description='The higher value, the higher the quality, "
        "but the larger the file size.'/>"
        "   <Option name='ETC1S_MAX_ENDPOINTS_CLUSTERS' type='int' min='1' "
        "max='16128' "
        "description='Maximum number of endpoint clusters. "
        "When set, ETC1S_MAX_SELECTOR_CLUSTERS must also be set. "
        "Mutually exclusive with ETC1S_QUALITY_LEVEL.'/>"
        "   <Option name='ETC1S_MAX_SELECTOR_CLUSTERS' type='int' min='1' "
        "max='16128' "
        "description='Maximum number of selector clusters. "
        "When set, ETC1S_MAX_ENDPOINTS_CLUSTERS must also be set. "
        "Mutually exclusive with ETC1S_QUALITY_LEVEL.'/>"
        "   <Option name='NUM_THREADS' type='int' description='Number of "
        "threads to use. "
        "By default, maximum number of virtual CPUs available'/>"
        "   <Option name='MIPMAP' type='boolean' "
        "description='Whether to enable MIPMAP generation.' default='NO'/>"
        "   <Option name='COLORSPACE' type='string-select' "
        "default='PERCEPTUAL_SRGB'>"
        "       <Value>PERCEPTUAL_SRGB</Value>"
        "       <Value>LINEAR</Value>"
        "   </Option>"
        "</CreationOptionList>";
    return osRet;
}
