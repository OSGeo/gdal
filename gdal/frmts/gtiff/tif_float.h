/******************************************************************************
 * $Id$
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Floating point conversion functions. Convert 16- and 24-bit
 *           floating point numbers into the 32-bit IEEE 754 compliant ones.
 * Author:   Andrey Kiselev, dron@remotesensing.org
 *
 ******************************************************************************
 * Copyright (c) 2005, Andrey Kiselev <dron@remotesensing.org>
 * Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
 *
 * This code is based on the code from OpenEXR project with the following
 * copyright:
 *
 * Copyright (c) 2002, Industrial Light & Magic, a division of Lucas
 * Digital Ltd. LLC
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * *       Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * *       Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 * *       Neither the name of Industrial Light & Magic nor the names of
 * its contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#ifndef TIF_FLOAT_H_INCLUDED
#define TIF_FLOAT_H_INCLUDED

#include "cpl_port.h"

CPL_C_START
GUInt32 HalfToFloat( GUInt16 iHalf );
GUInt32 TripleToFloat( GUInt32 iTriple );
CPL_C_END

#ifdef __cplusplus
GUInt16 FloatToHalf( GUInt32 iFloat32, bool& bHasWarned );
#endif

#endif // TIF_FLOAT_H_INCLUDED
