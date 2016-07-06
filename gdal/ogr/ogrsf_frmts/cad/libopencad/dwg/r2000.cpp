/*******************************************************************************
 *  Project: libopencad
 *  Purpose: OpenSource CAD formats support library
 *  Author: Alexandr Borzykh, mush3d at gmail.com
 *  Author: Dmitry Baryshnikov, bishop.dev@gmail.com
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2016 Alexandr Borzykh
 *  Copyright (c) 2016 NextGIS, <info@nextgis.com>
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *******************************************************************************/

#include "r2000.h"
#include "io.h"
#include "cadgeometry.h"
#include "cadobjects.h"
#include "opencad_api.h"

#include <iostream>
#include <cstring>
#include <cassert>
#include <memory>

#ifdef __APPLE__
#include <MacTypes.h>
#endif

#define UNKNOWN1 CADHeader::MAX_HEADER_CONSTANT + 1
#define UNKNOWN2 CADHeader::MAX_HEADER_CONSTANT + 2
#define UNKNOWN3 CADHeader::MAX_HEADER_CONSTANT + 3
#define UNKNOWN4 CADHeader::MAX_HEADER_CONSTANT + 4
#define UNKNOWN5 CADHeader::MAX_HEADER_CONSTANT + 5
#define UNKNOWN6 CADHeader::MAX_HEADER_CONSTANT + 6
#define UNKNOWN7 CADHeader::MAX_HEADER_CONSTANT + 7
#define UNKNOWN8 CADHeader::MAX_HEADER_CONSTANT + 8
#define UNKNOWN9 CADHeader::MAX_HEADER_CONSTANT + 9
#define UNKNOWN10 CADHeader::MAX_HEADER_CONSTANT + 10
#define UNKNOWN11 CADHeader::MAX_HEADER_CONSTANT + 11
#define UNKNOWN12 CADHeader::MAX_HEADER_CONSTANT + 12
#define UNKNOWN13 CADHeader::MAX_HEADER_CONSTANT + 13
#define UNKNOWN14 CADHeader::MAX_HEADER_CONSTANT + 14
#define UNKNOWN15 CADHeader::MAX_HEADER_CONSTANT + 15

int DWGFileR2000::readHeader (OpenOptions eOptions)
{
    char buffer[255];
    char * pabyBuf;
    size_t dHeaderVarsSectionLength = 0;

    fileIO->Seek (sectionLocatorRecords[0].dSeeker, CADFileIO::SeekOrigin::BEG);
    fileIO->Read (buffer, DWGSentinelLength);
    if ( memcmp (buffer, DWGHeaderVariablesStart, DWGSentinelLength) )
    {
        DebugMsg("File is corrupted (wrong pointer to HEADER_VARS section,"
                        "or HEADERVARS starting sentinel corrupted.)");

        return CADErrorCodes::HEADER_SECTION_READ_FAILED;
    }

    fileIO->Read (&dHeaderVarsSectionLength, 4);
    DebugMsg("Header variables section length: %ld\n", dHeaderVarsSectionLength);

    size_t nBitOffsetFromStart = 0;
    pabyBuf = new char[dHeaderVarsSectionLength + 4];
    fileIO->Read ( pabyBuf, dHeaderVarsSectionLength + 2 );

    if(eOptions == OpenOptions::READ_ALL)
    {
        header.addValue(UNKNOWN1, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));
        header.addValue(UNKNOWN2, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));
        header.addValue(UNKNOWN3, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));
        header.addValue(UNKNOWN4, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));
        header.addValue(UNKNOWN5, ReadTV (pabyBuf, nBitOffsetFromStart));
        header.addValue(UNKNOWN6, ReadTV (pabyBuf, nBitOffsetFromStart));
        header.addValue(UNKNOWN7, ReadTV (pabyBuf, nBitOffsetFromStart));
        header.addValue(UNKNOWN8, ReadTV (pabyBuf, nBitOffsetFromStart));
        header.addValue(UNKNOWN9, ReadBITLONG (pabyBuf, nBitOffsetFromStart));
        header.addValue(UNKNOWN10, ReadBITLONG (pabyBuf, nBitOffsetFromStart));
    }
    else
    {
        skipBITDOUBLE(pabyBuf, nBitOffsetFromStart);
        skipBITDOUBLE(pabyBuf, nBitOffsetFromStart);
        skipBITDOUBLE(pabyBuf, nBitOffsetFromStart);
        skipBITDOUBLE(pabyBuf, nBitOffsetFromStart);
        skipTV(pabyBuf, nBitOffsetFromStart);
        skipTV(pabyBuf, nBitOffsetFromStart);
        skipTV(pabyBuf, nBitOffsetFromStart);
        skipTV(pabyBuf, nBitOffsetFromStart);
        skipBITLONG(pabyBuf, nBitOffsetFromStart);
        skipBITLONG(pabyBuf, nBitOffsetFromStart);
    }

    CADHandle stCurrentViewportTable = ReadHANDLE (pabyBuf, nBitOffsetFromStart);
    tables.addTable (CADTables::CurrentViewportTable,
                        stCurrentViewportTable);

    if(eOptions == OpenOptions::READ_ALL)
    {
        header.addValue(CADHeader::DIMASO, ReadBIT (pabyBuf, nBitOffsetFromStart));     // 1
        header.addValue(CADHeader::DIMSHO, ReadBIT (pabyBuf, nBitOffsetFromStart));     // 2
        header.addValue(CADHeader::PLINEGEN, ReadBIT (pabyBuf, nBitOffsetFromStart));   // 3
        header.addValue(CADHeader::ORTHOMODE, ReadBIT (pabyBuf, nBitOffsetFromStart));  // 4
        header.addValue(CADHeader::REGENMODE, ReadBIT (pabyBuf, nBitOffsetFromStart));  // 5
        header.addValue(CADHeader::FILLMODE, ReadBIT (pabyBuf, nBitOffsetFromStart));   // 6
        header.addValue(CADHeader::QTEXTMODE, ReadBIT (pabyBuf, nBitOffsetFromStart));  // 7
        header.addValue(CADHeader::PSLTSCALE, ReadBIT (pabyBuf, nBitOffsetFromStart));  // 8
        header.addValue(CADHeader::LIMCHECK, ReadBIT (pabyBuf, nBitOffsetFromStart));   // 9
        header.addValue(CADHeader::USRTIMER, ReadBIT (pabyBuf, nBitOffsetFromStart));   // 10
        header.addValue(CADHeader::SKPOLY, ReadBIT (pabyBuf, nBitOffsetFromStart));     // 11
        header.addValue(CADHeader::ANGDIR, ReadBIT (pabyBuf, nBitOffsetFromStart));     // 12
        header.addValue(CADHeader::SPLFRAME, ReadBIT (pabyBuf, nBitOffsetFromStart));   // 13
        header.addValue(CADHeader::MIRRTEXT, ReadBIT (pabyBuf, nBitOffsetFromStart));   // 14
        header.addValue(CADHeader::WORDLVIEW, ReadBIT (pabyBuf, nBitOffsetFromStart));  // 15
        header.addValue(CADHeader::TILEMODE, ReadBIT (pabyBuf, nBitOffsetFromStart));   // 16
        header.addValue(CADHeader::PLIMCHECK, ReadBIT (pabyBuf, nBitOffsetFromStart));  // 17
        header.addValue(CADHeader::VISRETAIN, ReadBIT (pabyBuf, nBitOffsetFromStart));  // 18
        header.addValue(CADHeader::DISPSILH, ReadBIT (pabyBuf, nBitOffsetFromStart));   // 19
        header.addValue(CADHeader::PELLIPSE, ReadBIT (pabyBuf, nBitOffsetFromStart));   // 20
    }
    else
    {
        nBitOffsetFromStart += 20;
    }

    if(eOptions == OpenOptions::READ_ALL)
    {
        header.addValue(CADHeader::PROXYGRAPHICS, ReadBITSHORT (pabyBuf, nBitOffsetFromStart)); // 1
        header.addValue(CADHeader::TREEDEPTH, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));     // 2
        header.addValue(CADHeader::LUNITS, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));        // 3
        header.addValue(CADHeader::LUPREC, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));        // 4
        header.addValue(CADHeader::AUNITS, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));        // 5
        header.addValue(CADHeader::AUPREC, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));        // 6
    }
    else
    {
        for(char i = 0; i < 6; ++i)
            skipBITSHORT (pabyBuf, nBitOffsetFromStart);
    }

    header.addValue(CADHeader::ATTMODE, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::PDMODE, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));

    if(eOptions == OpenOptions::READ_ALL)
    {
        header.addValue(CADHeader::USERI1, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));    // 1
        header.addValue(CADHeader::USERI2, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));    // 2
        header.addValue(CADHeader::USERI3, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));    // 3
        header.addValue(CADHeader::USERI4, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));    // 4
        header.addValue(CADHeader::USERI5, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));    // 5
        header.addValue(CADHeader::SPLINESEGS, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));// 6
        header.addValue(CADHeader::SURFU, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));     // 7
        header.addValue(CADHeader::SURFV, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));     // 8
        header.addValue(CADHeader::SURFTYPE, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));  // 9
        header.addValue(CADHeader::SURFTAB1, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));  // 10
        header.addValue(CADHeader::SURFTAB2, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));  // 11
        header.addValue(CADHeader::SPLINETYPE, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));// 12
        header.addValue(CADHeader::SHADEDGE, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));  // 13
        header.addValue(CADHeader::SHADEDIF, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));  // 14
        header.addValue(CADHeader::UNITMODE, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));  // 15
        header.addValue(CADHeader::MAXACTVP, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));  // 16
        header.addValue(CADHeader::ISOLINES, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));  // 17
        header.addValue(CADHeader::CMLJUST, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));   // 18
        header.addValue(CADHeader::TEXTQLTY, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));  // 19
    }
    else
    {
        for(char i = 0; i < 19; ++i)
            skipBITSHORT (pabyBuf, nBitOffsetFromStart);
    }

    header.addValue(CADHeader::LTSCALE, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::TEXTSIZE, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::TRACEWID, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::SKETCHINC, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::FILLETRAD, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::THICKNESS, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::ANGBASE, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::PDSIZE, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::PLINEWID, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));

    if(eOptions == OpenOptions::READ_ALL)
    {
        header.addValue(CADHeader::USERR1, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));   // 1
        header.addValue(CADHeader::USERR2, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));   // 2
        header.addValue(CADHeader::USERR3, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));   // 3
        header.addValue(CADHeader::USERR4, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));   // 4
        header.addValue(CADHeader::USERR5, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));   // 5
        header.addValue(CADHeader::CHAMFERA, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart)); // 6
        header.addValue(CADHeader::CHAMFERB, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart)); // 7
        header.addValue(CADHeader::CHAMFERC, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart)); // 8
        header.addValue(CADHeader::CHAMFERD, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart)); // 9
        header.addValue(CADHeader::FACETRES, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart)); // 10
        header.addValue(CADHeader::CMLSCALE, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart)); // 11
        header.addValue(CADHeader::CELTSCALE, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));// 12

        header.addValue(CADHeader::MENU, ReadTV (pabyBuf, nBitOffsetFromStart));
    }
    else
    {
        for(char i = 0; i < 12; ++i)
            skipBITDOUBLE (pabyBuf, nBitOffsetFromStart);
        skipTV (pabyBuf, nBitOffsetFromStart);
    }

    long juliandate, millisec;
    juliandate = ReadBITLONG (pabyBuf, nBitOffsetFromStart);
    millisec = ReadBITLONG (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::TDCREATE, juliandate, millisec);
    juliandate = ReadBITLONG (pabyBuf, nBitOffsetFromStart);
    millisec = ReadBITLONG (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::TDUPDATE, juliandate, millisec);
    juliandate = ReadBITLONG (pabyBuf, nBitOffsetFromStart);
    millisec = ReadBITLONG (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::TDINDWG, juliandate, millisec);
    juliandate = ReadBITLONG (pabyBuf, nBitOffsetFromStart);
    millisec = ReadBITLONG (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::TDUSRTIMER, juliandate, millisec);

    header.addValue(CADHeader::CECOLOR, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));

    header.addValue(CADHeader::HANDSEED, ReadHANDLE8BLENGTH (pabyBuf, nBitOffsetFromStart)); // CHECK THIS CASE.

    header.addValue(CADHeader::CLAYER, ReadHANDLE (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::TEXTSTYLE, ReadHANDLE (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::CELTYPE, ReadHANDLE (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::DIMSTYLE, ReadHANDLE (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::CMLSTYLE, ReadHANDLE (pabyBuf, nBitOffsetFromStart));

    header.addValue(CADHeader::PSVPSCALE, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));
    double dX, dY, dZ;
    dX = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dY = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dZ = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::PINSBASE, dX, dY, dZ);

    dX = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dY = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dZ = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::PEXTMIN, dX, dY, dZ);
    dX = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dY = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dZ = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::PEXTMAX, dX, dY, dZ);
    dX = ReadRAWDOUBLE (pabyBuf, nBitOffsetFromStart);
    dY = ReadRAWDOUBLE (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::PLIMMIN, dX, dY);
    dX = ReadRAWDOUBLE (pabyBuf, nBitOffsetFromStart);
    dY = ReadRAWDOUBLE (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::PLIMMAX, dX, dY);

    header.addValue(CADHeader::PELEVATION, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));

    dX = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dY = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dZ = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::PUCSORG, dX, dY, dZ);
    dX = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dY = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dZ = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::PUCSXDIR, dX, dY, dZ);
    dX = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dY = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dZ = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::PUCSYDIR, dX, dY, dZ);

    header.addValue(CADHeader::PUCSNAME, ReadHANDLE (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::PUCSORTHOREF, ReadHANDLE (pabyBuf, nBitOffsetFromStart));

    header.addValue(CADHeader::PUCSORTHOVIEW, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::PUCSBASE, ReadHANDLE (pabyBuf, nBitOffsetFromStart));

    dX = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dY = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dZ = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::PUCSORGTOP, dX, dY, dZ);
    dX = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dY = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dZ = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::PUCSORGBOTTOM, dX, dY, dZ);
    dX = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dY = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dZ = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::PUCSORGLEFT, dX, dY, dZ);
    dX = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dY = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dZ = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::PUCSORGRIGHT, dX, dY, dZ);
    dX = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dY = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dZ = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::PUCSORGFRONT, dX, dY, dZ);
    dX = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dY = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dZ = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::PUCSORGBACK, dX, dY, dZ);

    dX = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dY = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dZ = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::INSBASE, dX, dY, dZ);
    dX = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dY = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dZ = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::EXTMIN, dX, dY, dZ);
    dX = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dY = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dZ = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::EXTMAX, dX, dY, dZ);
    dX = ReadRAWDOUBLE (pabyBuf, nBitOffsetFromStart);
    dY = ReadRAWDOUBLE (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::LIMMIN, dX, dY);
    dX = ReadRAWDOUBLE (pabyBuf, nBitOffsetFromStart);
    dY = ReadRAWDOUBLE (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::LIMMAX, dX, dY);

    header.addValue(CADHeader::ELEVATION, ReadBITDOUBLE (pabyBuf,
                                                         nBitOffsetFromStart));
    dX = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dY = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dZ = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::UCSORG, dX, dY, dZ);
    dX = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dY = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dZ = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::UCSXDIR, dX, dY, dZ);
    dX = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dY = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dZ = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::UCSYDIR, dX, dY, dZ);

    header.addValue(CADHeader::UCSNAME, ReadHANDLE (pabyBuf,
                                                    nBitOffsetFromStart));
    header.addValue(CADHeader::UCSORTHOREF, ReadHANDLE (pabyBuf,
                                                        nBitOffsetFromStart));

    header.addValue(CADHeader::UCSORTHOVIEW, ReadBITSHORT (pabyBuf,
                                                           nBitOffsetFromStart));

    header.addValue(CADHeader::UCSBASE, ReadHANDLE (pabyBuf,
                                                    nBitOffsetFromStart));

    dX = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dY = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dZ = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::UCSORGTOP, dX, dY, dZ);
    dX = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dY = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dZ = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::UCSORGBOTTOM, dX, dY, dZ);
    dX = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dY = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dZ = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::UCSORGLEFT, dX, dY, dZ);
    dX = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dY = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dZ = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::UCSORGRIGHT, dX, dY, dZ);
    dX = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dY = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dZ = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::UCSORGFRONT, dX, dY, dZ);
    dX = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dY = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    dZ = ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::UCSORGBACK, dX, dY, dZ);

    if(eOptions == OpenOptions::READ_ALL){
    header.addValue(CADHeader::DIMPOST, ReadTV (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::DIMAPOST, ReadTV (pabyBuf, nBitOffsetFromStart));

    header.addValue(CADHeader::DIMSCALE, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart)); // 1
    header.addValue(CADHeader::DIMASZ, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));   // 2
    header.addValue(CADHeader::DIMEXO, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));   // 3
    header.addValue(CADHeader::DIMDLI, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));   // 4
    header.addValue(CADHeader::DIMEXE, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));   // 5
    header.addValue(CADHeader::DIMRND, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));   // 6
    header.addValue(CADHeader::DIMDLE, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));   // 7
    header.addValue(CADHeader::DIMTP, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));    // 8
    header.addValue(CADHeader::DIMTM, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));    // 9

    header.addValue(CADHeader::DIMTOL, ReadBIT (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::DIMLIM, ReadBIT (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::DIMTIH, ReadBIT (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::DIMTOH, ReadBIT (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::DIMSE1, ReadBIT (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::DIMSE2, ReadBIT (pabyBuf, nBitOffsetFromStart));

    header.addValue(CADHeader::DIMTAD, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::DIMZIN, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::DIMAZIN, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));

    header.addValue(CADHeader::DIMTXT, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));   // 1
    header.addValue(CADHeader::DIMCEN, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));   // 2
    header.addValue(CADHeader::DIMTSZ, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));   // 3
    header.addValue(CADHeader::DIMALTF, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));  // 4
    header.addValue(CADHeader::DIMLFAC, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));  // 5
    header.addValue(CADHeader::DIMTVP, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));   // 6
    header.addValue(CADHeader::DIMTFAC, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));  // 7
    header.addValue(CADHeader::DIMGAP, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));   // 8
    header.addValue(CADHeader::DIMALTRND, ReadBITDOUBLE (pabyBuf, nBitOffsetFromStart));// 9

    header.addValue(CADHeader::DIMALT, ReadBIT (pabyBuf, nBitOffsetFromStart));

    header.addValue(CADHeader::DIMALTD, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));

    header.addValue(CADHeader::DIMTOFL, ReadBIT (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::DIMSAH, ReadBIT (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::DIMTIX, ReadBIT (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::DIMSOXD, ReadBIT (pabyBuf, nBitOffsetFromStart));

    header.addValue(CADHeader::DIMCLRD, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));   // 1
    header.addValue(CADHeader::DIMCLRE, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));   // 2
    header.addValue(CADHeader::DIMCLRT, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));   // 3
    header.addValue(CADHeader::DIMADEC, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));   // 4
    header.addValue(CADHeader::DIMDEC, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));    // 5
    header.addValue(CADHeader::DIMTDEC, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));   // 6
    header.addValue(CADHeader::DIMALTU, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));   // 7
    header.addValue(CADHeader::DIMALTTD, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));  // 8
    header.addValue(CADHeader::DIMAUNIT, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));  // 9
    header.addValue(CADHeader::DIMFRAC, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));   // 10
    header.addValue(CADHeader::DIMLUNIT, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));  // 11
    header.addValue(CADHeader::DIMDSEP, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));   // 12
    header.addValue(CADHeader::DIMTMOVE, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));  // 13
    header.addValue(CADHeader::DIMJUST, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));   // 14

    header.addValue(CADHeader::DIMSD1, ReadBIT (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::DIMSD2, ReadBIT (pabyBuf, nBitOffsetFromStart));

    header.addValue(CADHeader::DIMTOLJ, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::DIMTZIN, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::DIMALTZ, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::DIMALTTZ, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));

    header.addValue(CADHeader::DIMUPT, ReadBIT (pabyBuf, nBitOffsetFromStart));

    header.addValue(CADHeader::DIMATFIT, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));

    header.addValue(CADHeader::DIMTXSTY, ReadHANDLE (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::DIMLDRBLK, ReadHANDLE (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::DIMBLK, ReadHANDLE (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::DIMBLK1, ReadHANDLE (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::DIMBLK2, ReadHANDLE (pabyBuf, nBitOffsetFromStart));

    header.addValue(CADHeader::DIMLWD, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::DIMLWE, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));
    }
    else
    {
        skipTV(pabyBuf, nBitOffsetFromStart);
        skipTV(pabyBuf, nBitOffsetFromStart);

        for(char i = 0; i < 9; ++i)
            skipBITDOUBLE (pabyBuf, nBitOffsetFromStart);

        nBitOffsetFromStart += 6;

        for(char i = 0; i < 3; ++i)
            skipBITSHORT (pabyBuf, nBitOffsetFromStart);

        for(char i = 0; i < 9; ++i)
            skipBITDOUBLE (pabyBuf, nBitOffsetFromStart);

        nBitOffsetFromStart++;

        skipBITSHORT (pabyBuf, nBitOffsetFromStart);

        nBitOffsetFromStart += 4;

        for(char i = 0; i < 14; ++i)
            skipBITSHORT (pabyBuf, nBitOffsetFromStart);

        nBitOffsetFromStart += 2;

        for(char i = 0; i < 4; ++i)
            skipBITSHORT (pabyBuf, nBitOffsetFromStart);

        nBitOffsetFromStart++;
        skipBITSHORT (pabyBuf, nBitOffsetFromStart);

        for(char i = 0; i < 5; ++i)
            skipHANDLE (pabyBuf, nBitOffsetFromStart);

        skipBITSHORT (pabyBuf, nBitOffsetFromStart);
        skipBITSHORT (pabyBuf, nBitOffsetFromStart);
    }

    CADHandle stBlocksTable = ReadHANDLE (pabyBuf, nBitOffsetFromStart);
    tables.addTable (CADTables::BlocksTable, stBlocksTable);

    CADHandle stLayersTable = ReadHANDLE (pabyBuf, nBitOffsetFromStart);
    tables.addTable (CADTables::LayersTable, stLayersTable);

    CADHandle stStyleTable = ReadHANDLE (pabyBuf, nBitOffsetFromStart);
    tables.addTable (CADTables::StyleTable, stStyleTable);

    CADHandle stLineTypesTable = ReadHANDLE (pabyBuf, nBitOffsetFromStart);
    tables.addTable (CADTables::LineTypesTable, stLineTypesTable);

    CADHandle stViewTable = ReadHANDLE (pabyBuf, nBitOffsetFromStart);
    tables.addTable (CADTables::ViewTable, stViewTable);

    CADHandle stUCSTable = ReadHANDLE (pabyBuf, nBitOffsetFromStart);
    tables.addTable (CADTables::UCSTable, stUCSTable);

    CADHandle stViewportTable = ReadHANDLE (pabyBuf, nBitOffsetFromStart);
    tables.addTable (CADTables::ViewportTable, stViewportTable);

    CADHandle stAPPIDTable = ReadHANDLE (pabyBuf, nBitOffsetFromStart);
    tables.addTable (CADTables::APPIDTable, stAPPIDTable);

    if(eOptions == OpenOptions::READ_ALL)
    {
        header.addValue(CADHeader::DIMSTYLE, ReadHANDLE (pabyBuf,
                                                        nBitOffsetFromStart));
    }
    else
    {
        skipHANDLE (pabyBuf, nBitOffsetFromStart);
    }

    CADHandle stEntityTable = ReadHANDLE (pabyBuf, nBitOffsetFromStart);
    tables.addTable (CADTables::EntityTable, stEntityTable);

    CADHandle stACADGroupDict = ReadHANDLE (pabyBuf, nBitOffsetFromStart);
    tables.addTable (CADTables::ACADGroupDict, stACADGroupDict);

    CADHandle stACADMLineStyleDict = ReadHANDLE (pabyBuf, nBitOffsetFromStart);
    tables.addTable (CADTables::ACADMLineStyleDict, stACADMLineStyleDict);

    CADHandle stNamedObjectsDict = ReadHANDLE (pabyBuf, nBitOffsetFromStart);
    tables.addTable (CADTables::NamedObjectsDict, stNamedObjectsDict);

    if(eOptions == OpenOptions::READ_ALL)
    {
        header.addValue(CADHeader::TSTACKALIGN, ReadBITSHORT (pabyBuf,
                                                        nBitOffsetFromStart));
        header.addValue(CADHeader::TSTACKSIZE, ReadBITSHORT (pabyBuf,
                                                        nBitOffsetFromStart));
    }
    else{
       skipBITSHORT (pabyBuf, nBitOffsetFromStart);
       skipBITSHORT (pabyBuf, nBitOffsetFromStart);
    }
    header.addValue(CADHeader::HYPERLINKBASE, ReadTV (pabyBuf,
                                                         nBitOffsetFromStart));
    header.addValue(CADHeader::STYLESHEET, ReadTV (pabyBuf,
                                                      nBitOffsetFromStart));

    CADHandle stLayoutsDict = ReadHANDLE (pabyBuf, nBitOffsetFromStart);
    tables.addTable (CADTables::LayoutsDict, stLayoutsDict);

    CADHandle stPlotSettingsDict = ReadHANDLE (pabyBuf, nBitOffsetFromStart);
    tables.addTable (CADTables::PlotSettingsDict, stPlotSettingsDict);

    CADHandle stPlotStylesDict = ReadHANDLE (pabyBuf, nBitOffsetFromStart);
    tables.addTable (CADTables::PlotStylesDict, stPlotStylesDict);

    if(eOptions == OpenOptions::READ_ALL)
    {
        int Flags = ReadBITLONG (pabyBuf, nBitOffsetFromStart);
        header.addValue(CADHeader::CELWEIGHT, Flags & 0x001F);
        header.addValue(CADHeader::ENDCAPS, static_cast<bool>(Flags & 0x0060));
        header.addValue(CADHeader::JOINSTYLE, static_cast<bool>(Flags & 0x0180));
        header.addValue(CADHeader::LWDISPLAY, static_cast<bool>(!(Flags & 0x0200)));
        header.addValue(CADHeader::XEDIT, static_cast<bool>(!(Flags & 0x0400)));
        header.addValue(CADHeader::EXTNAMES, static_cast<bool>(Flags & 0x0800));
        header.addValue(CADHeader::PSTYLEMODE, static_cast<bool>(Flags & 0x2000));
        header.addValue(CADHeader::OLESTARTUP, static_cast<bool>(Flags & 0x4000));
    }
    else
    {
        skipBITLONG (pabyBuf, nBitOffsetFromStart);
    }

    header.addValue(CADHeader::INSUNITS, ReadBITSHORT (pabyBuf,
                                                       nBitOffsetFromStart));
    short nCEPSNTYPE = ReadBITSHORT (pabyBuf, nBitOffsetFromStart);
    header.addValue(CADHeader::CEPSNTYPE, nCEPSNTYPE);

    if ( nCEPSNTYPE == 3 )
        header.addValue(CADHeader::CEPSNID, ReadHANDLE (pabyBuf, nBitOffsetFromStart));

    header.addValue(CADHeader::FINGERPRINTGUID, ReadTV (pabyBuf, nBitOffsetFromStart));
    header.addValue(CADHeader::VERSIONGUID, ReadTV (pabyBuf, nBitOffsetFromStart));

    CADHandle stBlockRecordPaperSpace = ReadHANDLE (pabyBuf, nBitOffsetFromStart);
    tables.addTable (CADTables::BlockRecordPaperSpace,
                        stBlockRecordPaperSpace);
    // TODO: is this part of the header?
    CADHandle stBlockRecordModelSpace = ReadHANDLE (pabyBuf, nBitOffsetFromStart);
    tables.addTable (CADTables::BlockRecordModelSpace, stBlockRecordModelSpace);

    if(eOptions == OpenOptions::READ_ALL)
    {
        // Is this part of the header?

        /*CADHandle LTYPE_BYLAYER = */ReadHANDLE (pabyBuf, nBitOffsetFromStart);
        /*CADHandle LTYPE_BYBLOCK = */ReadHANDLE (pabyBuf, nBitOffsetFromStart);
        /*CADHandle LTYPE_CONTINUOUS = */ReadHANDLE (pabyBuf, nBitOffsetFromStart);

        header.addValue(UNKNOWN11, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));
        header.addValue(UNKNOWN12, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));
        header.addValue(UNKNOWN13, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));
        header.addValue(UNKNOWN14, ReadBITSHORT (pabyBuf, nBitOffsetFromStart));
    }
    else
    {
        skipHANDLE (pabyBuf, nBitOffsetFromStart);
        skipHANDLE (pabyBuf, nBitOffsetFromStart);
        skipHANDLE (pabyBuf, nBitOffsetFromStart);
        skipBITSHORT (pabyBuf, nBitOffsetFromStart);
        skipBITSHORT (pabyBuf, nBitOffsetFromStart);
        skipBITSHORT (pabyBuf, nBitOffsetFromStart);
        skipBITSHORT (pabyBuf, nBitOffsetFromStart);
    }

    /*short nCRC =*/ ReadRAWSHORT (pabyBuf, nBitOffsetFromStart);
    unsigned short initial = 0xC0C1;
    /*short calculated_crc = */ CalculateCRC8 (initial, pabyBuf,
                                    static_cast<int>(dHeaderVarsSectionLength)); // TODO: CRC is calculated wrong every time.


    int returnCode = CADErrorCodes::SUCCESS;
    fileIO->Read (pabyBuf, DWGSentinelLength);
    if ( memcmp (pabyBuf, DWGHeaderVariablesEnd, DWGSentinelLength) )
    {
        DebugMsg("File is corrupted (HEADERVARS section ending sentinel "
                 "doesnt match.)");

        returnCode = CADErrorCodes::HEADER_SECTION_READ_FAILED;
    }

    delete[] pabyBuf;

    return returnCode;
}

int DWGFileR2000::readClasses (enum OpenOptions eOptions)
{
    if(eOptions == OpenOptions::READ_ALL || eOptions == OpenOptions::READ_FAST ){
        char    *pabySectionContent;
        char    buffer[255];
        size_t dSectionSize = 0;
        size_t nBitOffsetFromStart = 0;

        fileIO->Seek (sectionLocatorRecords[1].dSeeker, CADFileIO::SeekOrigin::BEG);

        fileIO->Read (buffer, DWGSentinelLength);
        if ( memcmp (buffer, DWGDSClassesStart, DWGSentinelLength) )
        {
            cerr << "File is corrupted (wrong pointer to CLASSES section,"
                    "or CLASSES starting sentinel corrupted.)\n";

            return CADErrorCodes::CLASSES_SECTION_READ_FAILED;
        }

        fileIO->Read (&dSectionSize, 4);
        DebugMsg ("Classes section length: %d\n", dSectionSize);

        pabySectionContent = new char[dSectionSize + 4];
        fileIO->Read (pabySectionContent, dSectionSize);

        while ( ( nBitOffsetFromStart / 8 ) + 1 < dSectionSize )
        {
            CADClass stClass;
            stClass.dClassNum = ReadBITSHORT (pabySectionContent,
                                              nBitOffsetFromStart);
            stClass.dProxyCapFlag = ReadBITSHORT (pabySectionContent,
                                                nBitOffsetFromStart);
            stClass.sApplicationName = ReadTV (pabySectionContent,
                                               nBitOffsetFromStart);
            stClass.sCppClassName = ReadTV (pabySectionContent,
                                            nBitOffsetFromStart);
            stClass.sDXFRecordName = ReadTV (pabySectionContent,
                                             nBitOffsetFromStart);
            stClass.bWasZombie = ReadBIT (pabySectionContent, nBitOffsetFromStart);
            stClass.bIsEntity  = ReadBITSHORT (pabySectionContent,
                                        nBitOffsetFromStart) == 0x1F2 ? true : false;

            classes.addClass (stClass);
        }

        delete [] pabySectionContent;

        fileIO->Read (buffer, 2); // CLASSES CRC!. TODO: add CRC computing & checking feature.

        fileIO->Read (buffer, DWGSentinelLength);
        if ( memcmp (buffer, DWGDSClassesEnd, DWGSentinelLength) )
        {
            cerr << "File is corrupted (CLASSES section ending sentinel "
                         "doesnt match.)\n";
            return CADErrorCodes::CLASSES_SECTION_READ_FAILED;
        }
    }
    return CADErrorCodes::SUCCESS;
}

int DWGFileR2000::createFileMap ()
{
    // Seems like ODA specification is completely awful. CRC is included in section size.
    // section size
    char     *pabySectionContent;
    unsigned short dSectionSize;
    size_t nRecordsInSection;
    size_t nSection = 0;
    size_t nBitOffsetFromStart;

    typedef pair<long, long> ObjHandleOffset;
    ObjHandleOffset previousObjHandleOffset;
    ObjHandleOffset tmpOffset;

    objectsMap.clear ();

    // seek to the begining of the objects map
    fileIO->Seek (sectionLocatorRecords[2].dSeeker, CADFileIO::SeekOrigin::BEG);

    while ( true )
    {
        dSectionSize = 0;

        // read section size
        fileIO->Read (&dSectionSize, 2);
        SwapEndianness (dSectionSize, sizeof (dSectionSize));

        DebugMsg ("Object map section #%d size: %d\n", ++nSection,
                  dSectionSize);

        if ( dSectionSize == 2 )
            break; // last section is empty.

        pabySectionContent = new char[dSectionSize + 4];
        nBitOffsetFromStart = 0;
        nRecordsInSection = 0;

        // read section data
        fileIO->Read (pabySectionContent, dSectionSize);

        while ( ( nBitOffsetFromStart / 8 ) < ( dSectionSize - 2 ) )
        {
            tmpOffset.first  = ReadUMCHAR (pabySectionContent, nBitOffsetFromStart);
            tmpOffset.second = ReadMCHAR (pabySectionContent, nBitOffsetFromStart);

            if(0 == nRecordsInSection) {
                previousObjHandleOffset = tmpOffset;
            }
            else {
                previousObjHandleOffset.first += tmpOffset.first;
                previousObjHandleOffset.second += tmpOffset.second;
            }
#ifdef _DEBUG
            assert(objectsMap.find (previousObjHandleOffset.first) ==
                   objectsMap.end ());
#endif //_DEBUG
            objectsMap.insert (previousObjHandleOffset);
            ++nRecordsInSection;
        }

        /* Unused
        dSectionCRC = */ReadRAWSHORT (pabySectionContent, nBitOffsetFromStart);/*
        SwapEndianness (dSectionCRC, sizeof (dSectionCRC));
        */

        delete[] pabySectionContent;
    }

    return CADErrorCodes::SUCCESS;
}

//TODO: fast extracting handles/CED works for entities,
//can we implement same capability for non-entities?
CADObject * DWGFileR2000::getObject (long index, bool bHandlesOnly)
{
    CADObject * readed_object = nullptr;

    char pabyObjectSize[8];
    size_t nBitOffsetFromStart = 0;
    fileIO->Seek (objectsMap[index], CADFileIO::SeekOrigin::BEG);
    fileIO->Read (pabyObjectSize, 8);
    unsigned int dObjectSize = ReadMSHORT (pabyObjectSize, nBitOffsetFromStart);

    // And read whole data chunk into memory for future parsing.
    // + nBitOffsetFromStart/8 + 2 is because dObjectSize doesn't cover CRC and itself.
    size_t nSectionSize = dObjectSize + nBitOffsetFromStart/8 + 2;
    unique_ptr<char[]> sectionContentPtr(new char[nSectionSize + 4]);
    char* pabySectionContent = sectionContentPtr.get ();
    fileIO->Seek (objectsMap[index], CADFileIO::SeekOrigin::BEG);
    fileIO->Read (pabySectionContent, nSectionSize);

    nBitOffsetFromStart = 0;
    dObjectSize = ReadMSHORT (pabySectionContent, nBitOffsetFromStart);
    short dObjectType = ReadBITSHORT (pabySectionContent, nBitOffsetFromStart);

    if(dObjectType >= 500){
        CADClass cadClass = classes.getClassByNum (dObjectType);
        if(cadClass.sCppClassName == "AcDbRasterImage"){
            dObjectType = CADObject::IMAGE;
        }
        else if(cadClass.sCppClassName == "AcDbRasterImageDef"){
            dObjectType = CADObject::IMAGEDEF;
        }
        else if(cadClass.sCppClassName == "AcDbRasterImageDefReactor"){
            dObjectType = CADObject::IMAGEDEFREACTOR;
        }
    }

    // Entities handling
    if ( isCommonEntityType(dObjectType) )
    {
        struct CADCommonED stCommonEntityData; // common for all entities

        stCommonEntityData.nObjectSizeInBits = ReadRAWLONG (pabySectionContent,
                                                            nBitOffsetFromStart);
        stCommonEntityData.hObjectHandle = ReadHANDLE (pabySectionContent,
                                                       nBitOffsetFromStart);

        short dEEDSize;
        CADEed dwgEed;
        while ( (dEEDSize = ReadBITSHORT (pabySectionContent,
                                          nBitOffsetFromStart)) != 0 )
        {
            dwgEed.dLength = dEEDSize;
            dwgEed.hApplication = ReadHANDLE (pabySectionContent,
                                              nBitOffsetFromStart);

            for ( short i = 0; i < dEEDSize; ++i )
            {
                dwgEed.acData.push_back(ReadCHAR (pabySectionContent,
                                                  nBitOffsetFromStart));
            }

            stCommonEntityData.aEED.push_back (dwgEed);
        }

        stCommonEntityData.bGraphicsPresented = ReadBIT (pabySectionContent,
                                                         nBitOffsetFromStart);
        if(stCommonEntityData.bGraphicsPresented){
            size_t nGraphicsDataSize = static_cast<size_t>(ReadRAWLONG (
                                                    pabySectionContent,
                                                    nBitOffsetFromStart));
            // skip read graphics data
            nBitOffsetFromStart += nGraphicsDataSize * 8;
        }
        stCommonEntityData.bbEntMode = Read2B (pabySectionContent,
                                               nBitOffsetFromStart);
        stCommonEntityData.nNumReactors = ReadBITLONG (pabySectionContent,
                                                       nBitOffsetFromStart);
        stCommonEntityData.bNoLinks = ReadBIT (pabySectionContent,
                                               nBitOffsetFromStart);
        stCommonEntityData.nCMColor = ReadBITSHORT (pabySectionContent,
                                                    nBitOffsetFromStart);
        stCommonEntityData.dfLTypeScale = ReadBITDOUBLE (pabySectionContent,
                                                         nBitOffsetFromStart);
        stCommonEntityData.bbLTypeFlags = Read2B (pabySectionContent,
                                                  nBitOffsetFromStart);
        stCommonEntityData.bbPlotStyleFlags = Read2B (pabySectionContent,
                                                      nBitOffsetFromStart);
        stCommonEntityData.nInvisibility = ReadBITSHORT (pabySectionContent,
                                                         nBitOffsetFromStart);
        stCommonEntityData.nLineWeight = ReadCHAR (pabySectionContent,
                                                   nBitOffsetFromStart);

        // Skip entitity-specific data, we dont need it if bHandlesOnly == true
        if( bHandlesOnly == true )
        {
            return getEntity(dObjectType, dObjectSize, stCommonEntityData,
                             pabySectionContent, nBitOffsetFromStart);
        }

        switch ( dObjectType )
        {
        case CADObject::BLOCK:
            return getBlock(dObjectSize, stCommonEntityData,
                            pabySectionContent, nBitOffsetFromStart);

        case CADObject::ELLIPSE:
            return getEllipse(dObjectSize, stCommonEntityData,
                              pabySectionContent, nBitOffsetFromStart);

        case CADObject::MLINE:
            return getMLine(dObjectSize, stCommonEntityData,
                            pabySectionContent, nBitOffsetFromStart);

        case CADObject::SOLID:
            return getSolid(dObjectSize, stCommonEntityData,
                            pabySectionContent, nBitOffsetFromStart);

        case CADObject::POINT:
            return getPoint(dObjectSize, stCommonEntityData,
                            pabySectionContent, nBitOffsetFromStart);

        case CADObject::POLYLINE3D:
            return getPolyLine3D(dObjectSize, stCommonEntityData,
                            pabySectionContent, nBitOffsetFromStart);

        case CADObject::RAY:
            return getRay(dObjectSize, stCommonEntityData,
                            pabySectionContent, nBitOffsetFromStart);

        case CADObject::XLINE:
            return getXLine(dObjectSize, stCommonEntityData,
                            pabySectionContent, nBitOffsetFromStart);

        case CADObject::LINE:
            return getLine(dObjectSize, stCommonEntityData,
                           pabySectionContent, nBitOffsetFromStart);

        case CADObject::TEXT:
            return getText(dObjectSize, stCommonEntityData,
                           pabySectionContent, nBitOffsetFromStart);

        case CADObject::VERTEX3D:
            return getVertex3D(dObjectSize, stCommonEntityData,
                           pabySectionContent, nBitOffsetFromStart);

        case CADObject::CIRCLE:
            return getCircle(dObjectSize, stCommonEntityData,
                             pabySectionContent, nBitOffsetFromStart);

        case CADObject::ENDBLK:
            return getEndBlock(dObjectSize, stCommonEntityData,
                               pabySectionContent, nBitOffsetFromStart);

        case CADObject::POLYLINE2D:
            return getPolyline2D(dObjectSize, stCommonEntityData,
                                 pabySectionContent, nBitOffsetFromStart);

        case CADObject::ATTRIB:
            return getAttributes(dObjectSize, stCommonEntityData,
                                 pabySectionContent, nBitOffsetFromStart);

        case CADObject::ATTDEF:
            return getAttributesDefn(dObjectSize, stCommonEntityData,
                                 pabySectionContent, nBitOffsetFromStart);

        case CADObject::LWPOLYLINE:
            return getLWPolyLine(dObjectSize, stCommonEntityData,
                                 pabySectionContent, nBitOffsetFromStart);

        case CADObject::ARC:
            return getArc(dObjectSize, stCommonEntityData,
                          pabySectionContent, nBitOffsetFromStart);

        case CADObject::SPLINE:
            return getSpline(dObjectSize, stCommonEntityData,
                             pabySectionContent, nBitOffsetFromStart);

        case CADObject::POLYLINE_PFACE:
            return getPolylinePFace (dObjectSize, stCommonEntityData,
                                     pabySectionContent, nBitOffsetFromStart);

        case CADObject::IMAGE:
            return getImage (dObjectSize, stCommonEntityData,
                             pabySectionContent, nBitOffsetFromStart);

        case CADObject::FACE3D:
            return get3DFace (dObjectSize, stCommonEntityData,
                              pabySectionContent, nBitOffsetFromStart);

        case CADObject::VERTEX_MESH:
            return getVertexMesh (dObjectSize, stCommonEntityData,
                                  pabySectionContent, nBitOffsetFromStart);

        case CADObject::VERTEX_PFACE:
            return getVertexPFace (dObjectSize, stCommonEntityData,
                                   pabySectionContent, nBitOffsetFromStart);

        case CADObject::MTEXT:
            return getMText (dObjectSize, stCommonEntityData,
                                   pabySectionContent, nBitOffsetFromStart);

        case CADObject::DIMENSION_RADIUS:
        case CADObject::DIMENSION_DIAMETER:
        case CADObject::DIMENSION_ALIGNED:
        case CADObject::DIMENSION_ANG_3PT:
        case CADObject::DIMENSION_ANG_2LN:
        case CADObject::DIMENSION_ORDINATE:
        case CADObject::DIMENSION_LINEAR:
            return getDimension (dObjectType, dObjectSize, stCommonEntityData,
                                   pabySectionContent, nBitOffsetFromStart);

        case CADObject::INSERT:
            return getInsert(dObjectType, dObjectSize, stCommonEntityData,
                             pabySectionContent, nBitOffsetFromStart);

        default:
            return getEntity(dObjectType, dObjectSize, stCommonEntityData,
                             pabySectionContent, nBitOffsetFromStart);
        }
    }
    else
    {
        switch ( dObjectType )
        {
        case CADObject::DICTIONARY:
            return getDictionary(dObjectSize, pabySectionContent,
                                 nBitOffsetFromStart);

        case CADObject::LAYER:
            return getLayerObject(dObjectSize, pabySectionContent,
                            nBitOffsetFromStart);

        case CADObject::LAYER_CONTROL_OBJ:
            return getLayerControl(dObjectSize, pabySectionContent,
                                   nBitOffsetFromStart);

        case CADObject::BLOCK_CONTROL_OBJ:
            return getBlockControl(dObjectSize, pabySectionContent,
                                   nBitOffsetFromStart);

        case CADObject::BLOCK_HEADER:
            return getBlockHeader(dObjectSize, pabySectionContent,
                                  nBitOffsetFromStart);

        case CADObject::LTYPE_CONTROL_OBJ:
            return getLineTypeControl(dObjectSize, pabySectionContent,
                                 nBitOffsetFromStart);

        case CADObject::LTYPE1:
            return getLineType1(dObjectSize, pabySectionContent,
                             nBitOffsetFromStart);

        case CADObject::IMAGEDEF:
            return getImageDef (dObjectSize, pabySectionContent,
                                nBitOffsetFromStart);

        case CADObject::IMAGEDEFREACTOR:
            return getImageDefReactor (dObjectSize, pabySectionContent,
                                       nBitOffsetFromStart);

        case CADObject::XRECORD:
            return getXRecord (dObjectSize, pabySectionContent,
                                nBitOffsetFromStart);
            }
        }

    return readed_object;
}

CADGeometry *DWGFileR2000::getGeometry(long index)
{
    unique_ptr<CADEntityObject> readedObject( ( CADEntityObject* ) getObject(index) );

    if(nullptr == readedObject)
        return nullptr;

    // Casting object's EED to a vector of strings
    vector< string > asEED;
    for( auto citer = readedObject->stCed.aEED.cbegin();
         citer != readedObject->stCed.aEED.cend(); ++citer )
    {
        string sEED = "";
        // Detect the type of EED entity
        switch(citer->acData[0])
        {
            case 0: // string
            {
                unsigned char nStrSize = citer->acData[1];
                // +2 = skip CodePage, no idea how to use it anyway
                for( size_t i = 0; i < nStrSize; ++i )
                {
                    sEED += citer->acData[i + 4];
                }
                break;
            }
            case 1: // invalid
            {
                DebugMsg("Error: EED obj type is 1, error in R2000::getGeometry()");
                break;
            }
            case 2: // { or }
            {
                sEED += citer->acData[1] == 0 ? '{' : '}';
                break;
            }
            case 3: // layer table ref
            {
                // FIXME: get CADHandle and return getAsLong() result.
                sEED += "Layer table ref (handle):";
                for( size_t i = 0; i < 8; ++i )
                {
                    sEED += citer->acData[i+1];
                }
                break;
            }
            case 4: // binary chunk
            {
                unsigned char nChunkSize = citer->acData[1];
                sEED += "Binary chunk (chars):";
                for( size_t i = 0; i < nChunkSize; ++i )
                {
                    sEED += citer->acData[i + 2];
                }
                break;
            }
            case 5: // entity handle ref
            {
                // FIXME: get CADHandle and return getAsLong() result.
                sEED += "Entity handle ref (handle):";
                for( size_t i = 0; i < 8; ++i )
                {
                    sEED += citer->acData[i+1];
                }
                break;
            }
            case 10:
            case 11:
            case 12:
            case 13:
            {
                sEED += "Point: {";
                double dfX = 0, dfY = 0, dfZ = 0;
                memcpy( &dfX, citer->acData.data() + 1, 8);
                memcpy( &dfY, citer->acData.data() + 9, 8);
                memcpy( &dfZ, citer->acData.data() + 17, 8);
                sEED += to_string( dfX );
                sEED += ';';
                sEED += to_string( dfY );
                sEED += ';';
                sEED += to_string( dfZ );
                sEED += '}';
                break;
            }
            case 40:
            case 41:
            case 42:
            {
                sEED += "Double:";
                double dfVal = 0;
                memcpy( &dfVal, citer->acData.data() + 1, 8);
                sEED += to_string( dfVal );
                break;
            }
            case 70:
            {
                sEED += "Short:";
                short dVal = 0;
                memcpy( &dVal, citer->acData.data() + 1, 2 );
                sEED += to_string( dVal );
                break;
            }
            case 71:
            {
                sEED += "Long Int:";
                long dVal = 0;
                memcpy( &dVal, citer->acData.data() + 1, 4 );
                sEED += to_string( dVal );
                break;
            }
            default:
            {
                DebugMsg("Error in parsing geometry EED: undefined typecode: %d",
                         (int)citer->acData[0]);
            }
        }
        asEED.emplace_back( sEED );
    }

    switch ( readedObject->getType() )
    {
    case CADObject::ARC:
    {
        CADArc * arc = new CADArc();
        CADArcObject * cadArc = static_cast<CADArcObject*>(
                    readedObject.get());

        arc->setColor (cadArc->stCed.nCMColor);
        arc->setPosition (cadArc->vertPosition);
        arc->setExtrusion (cadArc->vectExtrusion);
        arc->setRadius (cadArc->dfRadius);
        arc->setThickness(cadArc->dfThickness);
        arc->setStartingAngle (cadArc->dfStartAngle);
        arc->setEndingAngle (cadArc->dfEndAngle);
        arc->setEED( asEED );

        return arc;
    }

    case CADObject::POINT:
    {
        CADPoint3D * point = new CADPoint3D();
        CADPointObject * cadPoint = static_cast<CADPointObject*>(
                    readedObject.get());

        point->setColor (cadPoint->stCed.nCMColor);
        point->setPosition (cadPoint->vertPosition);
        point->setExtrusion (cadPoint->vectExtrusion);
        point->setXAxisAng (cadPoint->dfXAxisAng);
        point->setThickness(cadPoint->dfThickness);
        point->setEED( asEED );

        return point;
    }

    case CADObject::POLYLINE3D:
    {
        CADPolyline3D * polyline = new CADPolyline3D();
        CADPolyline3DObject * cadPolyline3D = static_cast<CADPolyline3DObject*>(
                    readedObject.get ());

        polyline->setColor (cadPolyline3D->stCed.nCMColor);
        polyline->setEED( asEED );
        // TODO: code can be much simplified if CADHandle will be used.
        // to do so, == and ++ operators should be implemented.
        unique_ptr<CADVertex3DObject> vertex;
        long currentVertexH = cadPolyline3D->hVertexes[0].getAsLong ();
        while ( currentVertexH != 0 )
        {
            vertex.reset (static_cast<CADVertex3DObject*>(
                              getObject (currentVertexH)));

            if ( vertex == nullptr )
                break;

            currentVertexH = vertex->stCed.hObjectHandle.getAsLong ();
            polyline->addVertex ( vertex->vertPosition );
            if ( vertex->stCed.bNoLinks == true )
            {
                ++currentVertexH;
            }
            else
            {
                currentVertexH = vertex->stChed.hNextEntity.getAsLong (
                            vertex->stCed.hObjectHandle );
            }

            // Last vertex is reached. read it and break reading.
            if ( currentVertexH == cadPolyline3D->hVertexes[1].getAsLong () )
            {
                vertex.reset (static_cast<CADVertex3DObject*>(
                                  getObject (currentVertexH)));
                polyline->addVertex ( vertex->vertPosition );
                break;
            }
        }
        return polyline;
    }

    case CADObject::LWPOLYLINE:
    {
        CADLWPolyline * lwPolyline = new CADLWPolyline();
        CADLWPolylineObject * cadlwPolyline = static_cast<CADLWPolylineObject*>(
                    readedObject.get ());

        lwPolyline->setColor (cadlwPolyline->stCed.nCMColor);
        lwPolyline->setConstWidth (cadlwPolyline->dfConstWidth);
        lwPolyline->setElevation (cadlwPolyline->dfElevation);
        for(const CADVector& vertex : cadlwPolyline->avertVertexes)
            lwPolyline->addVertex (vertex);
        lwPolyline->setVectExtrusion (cadlwPolyline->vectExtrusion);
        lwPolyline->setWidths (cadlwPolyline->astWidths);
        lwPolyline->setEED( asEED );

        return lwPolyline;
    }

    case CADObject::CIRCLE:
    {
        CADCircle * circle = new CADCircle();
        CADCircleObject * cadCircle = static_cast<CADCircleObject*>(
                    readedObject.get());

        circle->setColor (cadCircle->stCed.nCMColor);
        circle->setPosition (cadCircle->vertPosition);
        circle->setExtrusion (cadCircle->vectExtrusion);
        circle->setRadius (cadCircle->dfRadius);
        circle->setThickness(cadCircle->dfThickness);
        circle->setEED( asEED );

        return circle;
    }

    case CADObject::ATTRIB:
    {
        CADAttrib * attrib = new CADAttrib();
        CADAttribObject * cadAttrib = static_cast<CADAttribObject*>(
                readedObject.get() );

        attrib->setPosition (cadAttrib->vertInsetionPoint);
        attrib->setColor (cadAttrib->stCed.nCMColor);
        attrib->setExtrusion (cadAttrib->vectExtrusion);
        attrib->setRotationAngle (cadAttrib->dfRotationAng);
        attrib->setAlignmentPoint (cadAttrib->vertAlignmentPoint);
        attrib->setElevation (cadAttrib->dfElevation);
        attrib->setHeight (cadAttrib->dfHeight);
        attrib->setObliqueAngle (cadAttrib->dfObliqueAng);
        attrib->setPositionLocked (cadAttrib->bLockPosition);
        attrib->setTag (cadAttrib->sTag);
        attrib->setTextValue (cadAttrib->sTextValue);
        attrib->setThickness (cadAttrib->dfThickness);
        attrib->setEED( asEED );

        return attrib;
    }

    case CADObject::ATTDEF:
    {
        CADAttdef * attdef = new CADAttdef();
        CADAttdefObject * cadAttrib = static_cast<CADAttdefObject*>(
                readedObject.get() );

        attdef->setPosition (cadAttrib->vertInsetionPoint);
        attdef->setColor (cadAttrib->stCed.nCMColor);
        attdef->setExtrusion (cadAttrib->vectExtrusion);
        attdef->setRotationAngle (cadAttrib->dfRotationAng);
        attdef->setAlignmentPoint (cadAttrib->vertAlignmentPoint);
        attdef->setElevation (cadAttrib->dfElevation);
        attdef->setHeight (cadAttrib->dfHeight);
        attdef->setObliqueAngle (cadAttrib->dfObliqueAng);
        attdef->setPositionLocked (cadAttrib->bLockPosition);
        attdef->setTag (cadAttrib->sTag);
        attdef->setTextValue (cadAttrib->sTextValue);
        attdef->setThickness (cadAttrib->dfThickness);
        attdef->setEED( asEED );

        return attdef;
    }

    case CADObject::ELLIPSE:
    {
        CADEllipse * ellipse = new CADEllipse();
        CADEllipseObject * cadEllipse = static_cast<CADEllipseObject*>(
                    readedObject.get());

        ellipse->setColor (cadEllipse->stCed.nCMColor);
        ellipse->setPosition (cadEllipse->vertPosition);
        ellipse->setSMAxis (cadEllipse->vectSMAxis);
        ellipse->setAxisRatio (cadEllipse->dfAxisRatio);
        ellipse->setEndingAngle (cadEllipse->dfEndAngle);
        ellipse->setStartingAngle (cadEllipse->dfBegAngle);
        ellipse->setEED( asEED );

        return ellipse;
    }

    case CADObject::LINE:
    {
        CADLineObject * cadLine = static_cast<CADLineObject *>(
                    readedObject.get());

        CADPoint3D ptBeg(cadLine->vertStart, cadLine->dfThickness);
        CADPoint3D ptEnd(cadLine->vertEnd, cadLine->dfThickness);

        CADLine * line = new CADLine(ptBeg, ptEnd);
        line->setColor (cadLine->stCed.nCMColor);
        line->setEED( asEED );

        return line;
    }

    case CADObject::RAY:
    {
        CADRay * ray = new CADRay();
        CADRayObject * cadRay = static_cast<CADRayObject *>(
                    readedObject.get());

        ray->setColor (cadRay->stCed.nCMColor);
        ray->setVectVector (cadRay->vectVector);
        ray->setPosition (cadRay->vertPosition);
        ray->setEED( asEED );

        return ray;
    }

    case CADObject::SPLINE:
    {
        CADSpline * spline = new CADSpline();
        CADSplineObject * cadSpline = static_cast<CADSplineObject *>(
                    readedObject.get());


        spline->setColor (cadSpline->stCed.nCMColor);
        spline->setScenario (cadSpline->dScenario);
        spline->setDegree( cadSpline->dDegree );
        spline->setEED( asEED );
        if ( spline->getScenario() == 2 )
        {
            spline->setFitTollerance (cadSpline->dfFitTol);
        }
        else if ( spline->getScenario() == 1 )
        {
            spline->setRational (cadSpline->bRational);
            spline->setClosed (cadSpline->bClosed);
            spline->setWeight (cadSpline->bWeight);
        }
        for(double weight : cadSpline->adfCtrlPointsWeight)
            spline->addControlPointsWeight (weight);

        for(const CADVector &pt : cadSpline->averFitPoints)
            spline->addFitPoint(pt);

        for(const CADVector &pt : cadSpline->avertCtrlPoints)
            spline->addControlPoint(pt);

        return spline;
    }

    case CADObject::TEXT:
    {
        CADText * text = new CADText();
        CADTextObject * cadText = static_cast<CADTextObject *>(
                    readedObject.get());

        text->setColor (cadText->stCed.nCMColor);
        text->setPosition (cadText->vertInsetionPoint);
        text->setTextValue (cadText->sTextValue);
        text->setRotationAngle (cadText->dfRotationAng);
        text->setObliqueAngle (cadText->dfObliqueAng);
        text->setThickness(cadText->dfThickness);
        text->setHeight (cadText->dfElevation);
        text->setEED( asEED );

        return text;
    }

    case CADObject::SOLID:
    {
        CADSolid * solid = new CADSolid();
        CADSolidObject * cadSolid = static_cast<CADSolidObject *>(
                    readedObject.get());

        solid->setColor (cadSolid->stCed.nCMColor);
        solid->setElevation (cadSolid->dfElevation);
        solid->setThickness(cadSolid->dfThickness);
        for(const CADVector& corner : cadSolid->avertCorners)
            solid->addAverCorner (corner) ;
        solid->setExtrusion (cadSolid->vectExtrusion);
        solid->setEED( asEED );

        return solid;
    }

    case CADObject::IMAGE:
    {
        CADImage * image = new CADImage();
        CADImageObject * cadImage = static_cast<CADImageObject *>(
                    readedObject.get());

        unique_ptr<CADImageDefObject> cadImageDef (
                    static_cast<CADImageDefObject *>(
                        getObject ( cadImage->hImageDef.getAsLong () ) ) );


        image->setColor (cadImage->stCed.nCMColor);
        image->setClippingBoundaryType (cadImage->dClipBoundaryType);
        image->setFilePath (cadImageDef->sFilePath);
        image->setVertInsertionPoint(cadImage->vertInsertion);
        CADVector imageSize(cadImage->dfSizeX,
                            cadImage->dfSizeY);
        image->setImageSize(imageSize);
        CADVector imageSizeInPx(cadImageDef->dfXImageSizeInPx,
                                cadImageDef->dfYImageSizeInPx);
        image->setImageSizeInPx(imageSizeInPx);
        CADVector pixelSizeInACADUnits (cadImageDef->dfXPixelSize,
                                        cadImageDef->dfYPixelSize);
        image->setPixelSizeInACADUnits(pixelSizeInACADUnits);
        image->setResolutionUnits(cadImageDef->dResUnits);
        image->setOptions(cadImage->dDisplayProps & 0x08,
                          cadImage->bClipping,
                          cadImage->dBrightness,
                          cadImage->dContrast);
        for(const CADVector &clipPt :  cadImage->avertClippingPolygonVertexes)
        {
            image->addClippingPoint(clipPt);
        }
        image->setEED( asEED );

        return image;
    }

    case CADObject::MLINE:
    {
        CADMLine * mline = new CADMLine();
        CADMLineObject * cadmLine = static_cast<CADMLineObject *>(
                    readedObject.get());

        mline->setColor (cadmLine->stCed.nCMColor);
        mline->setScale (cadmLine->dfScale);
        mline->setOpened (cadmLine->dOpenClosed == 1 ? true : false);
        for (  const CADMLineVertex &vertex : cadmLine->avertVertexes )
            mline->addVertex (vertex.vertPosition);
        mline->setEED( asEED );

        return mline;
    }

    case CADObject::MTEXT:
    {
        CADMText * mtext = new CADMText();
        CADMTextObject * cadmText = static_cast<CADMTextObject *>(
                    readedObject.get());

        mtext->setColor (cadmText->stCed.nCMColor);

        mtext->setTextValue (cadmText->sTextValue);
        mtext->setXAxisAng (cadmText->vectXAxisDir.getX ()); //TODO: is this needed?

        mtext->setPosition (cadmText->vertInsertionPoint);
        mtext->setExtrusion (cadmText->vectExtrusion);

        mtext->setHeight (cadmText->dfTextHeight);
        mtext->setRectWidth(cadmText->dfRectWidth);
        mtext->setExtents(cadmText->dfExtents);
        mtext->setExtentsWidth(cadmText->dfExtentsWidth);
        mtext->setEED( asEED );

        return mtext;
    }
    case CADObject::POLYLINE_PFACE:
    {
        CADPolylinePFace * polyline = new CADPolylinePFace();
        CADPolylinePFaceObject * cadpolyPface = static_cast<CADPolylinePFaceObject *>(
                    readedObject.get());

        // TODO: code can be much simplified if CADHandle will be used.
        // to do so, == and ++ operators should be implemented.
        polyline->setColor (cadpolyPface->stCed.nCMColor);
        polyline->setEED( asEED );
        unique_ptr<CADVertexPFaceObject> vertex;
        auto dCurrentEntHandle = cadpolyPface->hVertexes[0].getAsLong ();
        auto dLastEntHandle    = cadpolyPface->hVertexes[1].getAsLong ();
        while ( true )
        {
            vertex.reset (static_cast<CADVertexPFaceObject*>(
                              getObject (dCurrentEntHandle)));
            /* TODO: this check is excessive, but if something goes wrong way -
             * some part of geometries will be parsed. */
            if ( vertex == nullptr )
                    continue;

            polyline->addVertex (vertex->vertPosition);

            /* FIXME: somehow one more vertex which isnot presented is read.
             * so, checking the number of added vertexes */
            /*TODO: is this needed - check on real data
            if ( polyline->hVertexes.size() == cadpolyPface->nNumVertexes )
            {
                delete( vertex );
                break;
            }*/

            if ( vertex->stCed.bNoLinks )
                ++dCurrentEntHandle;
            else
                dCurrentEntHandle = vertex->stChed.hNextEntity.getAsLong (
                            vertex->stCed.hObjectHandle);

            if ( dCurrentEntHandle == dLastEntHandle )
            {
                vertex.reset (static_cast<CADVertexPFaceObject*>(
                                  getObject (dCurrentEntHandle)));
                polyline->addVertex (vertex->vertPosition);
                break;
            }
        }

        return polyline;
    }
    case CADObject::XLINE:
    {
        CADXLine * xline = new CADXLine();
        CADXLineObject * cadxLine = static_cast<CADXLineObject *>(
                    readedObject.get());

        xline->setColor (cadxLine->stCed.nCMColor);
        xline->setVectVector (cadxLine->vectVector);
        xline->setPosition (cadxLine->vertPosition);
        xline->setEED( asEED );

        return xline;
    }
    case CADObject::FACE3D:
    {
        CADFace3D * face = new CADFace3D();
        CAD3DFaceObject * cad3DFace = static_cast<CAD3DFaceObject *>(
                    readedObject.get());

        face->setColor (cad3DFace->stCed.nCMColor);
        for(const CADVector& corner : cad3DFace->avertCorners)
            face->addCorner (corner);
        face->setInvisFlags (cad3DFace->dInvisFlags);
        face->setEED( asEED );

        return face;
    }
    case CADObject::POLYLINE_MESH:
    case CADObject::VERTEX_MESH:
    case CADObject::VERTEX_PFACE_FACE:
    default:
        cerr << "Asked geometry has unsupported type." << endl;
    }

    return nullptr;
}

CADBlockObject *DWGFileR2000::getBlock(long dObjectSize,
                                       struct CADCommonED stCommonEntityData,
                                       const char * pabyInput,
                                       size_t& nBitOffsetFromStart)
{
    CADBlockObject * pBlock = new CADBlockObject();

    pBlock->setSize(dObjectSize);
    pBlock->stCed = stCommonEntityData;

    pBlock->sBlockName = ReadTV (pabyInput, nBitOffsetFromStart);

    fillCommonEntityHandleData(pBlock, pabyInput, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
    pBlock->setCRC(ReadRAWSHORT (pabyInput, nBitOffsetFromStart));

#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
            __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif // _DEBUG

    return pBlock;
}

CADEllipseObject *DWGFileR2000::getEllipse(long dObjectSize,
                                         CADCommonED stCommonEntityData,
                                         const char *pabyInput,
                                         size_t &nBitOffsetFromStart)
{
    CADEllipseObject * ellipse = new CADEllipseObject();

    ellipse->setSize(dObjectSize);
    ellipse->stCed = stCommonEntityData;

    CADVector vertPosition = ReadVector(pabyInput, nBitOffsetFromStart);

    ellipse->vertPosition = vertPosition;

    CADVector vectSMAxis = ReadVector(pabyInput, nBitOffsetFromStart);

    ellipse->vectSMAxis = vectSMAxis;

    CADVector vectExtrusion = ReadVector(pabyInput, nBitOffsetFromStart);

    ellipse->vectExtrusion = vectExtrusion;

    ellipse->dfAxisRatio = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
    ellipse->dfBegAngle = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
    ellipse->dfEndAngle = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);

    fillCommonEntityHandleData(ellipse, pabyInput, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
    ellipse->setCRC(ReadRAWSHORT (pabyInput, nBitOffsetFromStart));

#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
            __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif // _DEBUG

    return ellipse;
}

CADSolidObject *DWGFileR2000::getSolid(long dObjectSize,
                                         CADCommonED stCommonEntityData,
                                         const char *pabyInput,
                                         size_t &nBitOffsetFromStart)
{
    CADSolidObject * solid = new CADSolidObject();

    solid->setSize( dObjectSize );
    solid->stCed = stCommonEntityData;

    solid->dfThickness = ReadBIT (pabyInput, nBitOffsetFromStart) ?
                         0.0f : ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);

    solid->dfElevation = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);

    CADVector oCorner;
    for ( size_t i = 0; i < 4; ++i )
    {
        oCorner.setX(ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart));
        oCorner.setY(ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart));
        solid->avertCorners.push_back ( oCorner );
    }

    if ( ReadBIT (pabyInput, nBitOffsetFromStart) )
    {
        solid->vectExtrusion = CADVector(0.0f, 0.0f, 1.0f);
    }
    else
    {
        CADVector vectExtrusion = ReadVector(pabyInput, nBitOffsetFromStart);
        solid->vectExtrusion = vectExtrusion;
    }


    fillCommonEntityHandleData(solid, pabyInput, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
    solid->setCRC( ReadRAWSHORT (pabyInput, nBitOffsetFromStart) );

#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
         __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif //_DEBUG

    return solid;
}

CADPointObject *DWGFileR2000::getPoint(long dObjectSize,
                                       CADCommonED stCommonEntityData,
                                       const char *pabyInput,
                                       size_t &nBitOffsetFromStart)
{

    CADPointObject * point = new CADPointObject();

    point->setSize( dObjectSize );
    point->stCed = stCommonEntityData;

    CADVector vertPosition = ReadVector(pabyInput, nBitOffsetFromStart);

    point->vertPosition = vertPosition;

    point->dfThickness = ReadBIT (pabyInput, nBitOffsetFromStart) ?
                          0.0f : ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);

    if ( ReadBIT (pabyInput, nBitOffsetFromStart) )
    {
        point->vectExtrusion = CADVector(0.0f, 0.0f, 1.0f);
    }
    else
    {
        CADVector vectExtrusion = ReadVector(pabyInput, nBitOffsetFromStart);
        point->vectExtrusion = vectExtrusion;
    }

    point->dfXAxisAng = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);

    fillCommonEntityHandleData(point, pabyInput, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
    point->setCRC( ReadRAWSHORT (pabyInput, nBitOffsetFromStart));

#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif // _DEBUG

    return point;
}

CADPolyline3DObject *DWGFileR2000::getPolyLine3D(long dObjectSize,
                                           CADCommonED stCommonEntityData,
                                           const char *pabyInput,
                                           size_t &nBitOffsetFromStart)
{
    CADPolyline3DObject * polyline = new CADPolyline3DObject();

    polyline->setSize(dObjectSize);
    polyline->stCed = stCommonEntityData;

    polyline->SplinedFlags = ReadCHAR (pabyInput, nBitOffsetFromStart);
    polyline->ClosedFlags = ReadCHAR (pabyInput, nBitOffsetFromStart);

    fillCommonEntityHandleData(polyline, pabyInput, nBitOffsetFromStart);

    polyline->hVertexes.push_back ( ReadHANDLE (pabyInput, nBitOffsetFromStart) ); // 1st vertex
    polyline->hVertexes.push_back ( ReadHANDLE (pabyInput, nBitOffsetFromStart) ); // last vertex

    polyline->hSeqend = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 ); // padding bits to next byte boundary
    polyline->setCRC( ReadRAWSHORT (pabyInput, nBitOffsetFromStart) );

#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif // _DEBUG

    return polyline;
}

CADRayObject *DWGFileR2000::getRay(long dObjectSize,
                                   CADCommonED stCommonEntityData,
                                   const char *pabyInput,
                                   size_t &nBitOffsetFromStart)
{
    CADRayObject * ray = new CADRayObject();

    ray->setSize(dObjectSize);
    ray->stCed = stCommonEntityData;

    CADVector vertPosition = ReadVector(pabyInput, nBitOffsetFromStart);

    ray->vertPosition = vertPosition;

    CADVector vectVector = ReadVector(pabyInput, nBitOffsetFromStart);
    ray->vectVector = vectVector;

    fillCommonEntityHandleData(ray, pabyInput, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 ); // padding bits to next byte boundary
    ray->setCRC(ReadRAWSHORT (pabyInput, nBitOffsetFromStart));

#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif //_DEBUG

    return ray;
}

CADXLineObject *DWGFileR2000::getXLine(long dObjectSize,
                                       CADCommonED stCommonEntityData,
                                       const char *pabyInput,
                                       size_t &nBitOffsetFromStart)
{
    CADXLineObject * xline = new CADXLineObject();

    xline->setSize( dObjectSize );
    xline->stCed = stCommonEntityData;

    CADVector vertPosition = ReadVector(pabyInput, nBitOffsetFromStart);

    xline->vertPosition = vertPosition;

    CADVector vectVector = ReadVector(pabyInput, nBitOffsetFromStart);
    xline->vectVector = vectVector;

    fillCommonEntityHandleData(xline, pabyInput, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 ); // padding bits to next byte boundary
    xline->setCRC( ReadRAWSHORT (pabyInput, nBitOffsetFromStart) );

#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif // _DEBUG

    return xline;
}

CADLineObject *DWGFileR2000::getLine(long dObjectSize,
                                     CADCommonED stCommonEntityData,
                                     const char *pabyInput,
                                     size_t &nBitOffsetFromStart)
{
    CADLineObject * line = new CADLineObject();

    line->setSize(dObjectSize);
    line->stCed = stCommonEntityData;

    bool bZsAreZeros = ReadBIT (pabyInput, nBitOffsetFromStart);

    CADVector vertStart, vertEnd;
    vertStart.setX (ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart));
    vertEnd.setX (ReadBITDOUBLEWD (pabyInput, nBitOffsetFromStart,
                                   vertStart.getX()));
    vertStart.setY (ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart));
    vertEnd.setY (ReadBITDOUBLEWD (pabyInput, nBitOffsetFromStart,
                                   vertStart.getY()));

    if ( !bZsAreZeros )
    {
        vertStart.setZ(ReadBITDOUBLE (pabyInput, nBitOffsetFromStart));
        vertEnd.setZ(ReadBITDOUBLEWD (pabyInput, nBitOffsetFromStart,
                                      vertStart.getZ()));
    }

    line->vertStart = vertStart;
    line->vertEnd   = vertEnd;

    line->dfThickness = ReadBIT (pabyInput, nBitOffsetFromStart) ?
                        0.0f : ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);

    if ( ReadBIT (pabyInput, nBitOffsetFromStart) )
    {
        line->vectExtrusion = CADVector(0.0f, 0.0f, 1.0f);
    }
    else
    {
        CADVector vectExtrusion = ReadVector(pabyInput, nBitOffsetFromStart);
        line->vectExtrusion = vectExtrusion;
    }

    fillCommonEntityHandleData(line, pabyInput, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 ); // padding bits to next byte boundary
    line->setCRC(ReadRAWSHORT (pabyInput, nBitOffsetFromStart));

#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif // _DEBUG
    return line;
}

CADTextObject *DWGFileR2000::getText(long dObjectSize,
                                     CADCommonED stCommonEntityData,
                                     const char *pabyInput,
                                     size_t &nBitOffsetFromStart)
{
    CADTextObject * text = new CADTextObject();

    text->setSize (dObjectSize);
    text->stCed = stCommonEntityData;

    text->DataFlags = ReadCHAR (pabyInput, nBitOffsetFromStart);

    if ( !( text->DataFlags & 0x01 ) )
        text->dfElevation = ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);

    CADVector vertInsetionPoint = ReadRAWVector (pabyInput,
                                                 nBitOffsetFromStart);

    text->vertInsetionPoint = vertInsetionPoint;

    if ( !( text->DataFlags & 0x02 ) )
    {
        double x, y;
        x = ReadBITDOUBLEWD (pabyInput,
                             nBitOffsetFromStart, vertInsetionPoint.getX());
        y = ReadBITDOUBLEWD (pabyInput,
                             nBitOffsetFromStart, vertInsetionPoint.getY());
        CADVector vertAlignmentPoint(x, y);
        text->vertAlignmentPoint = vertAlignmentPoint;
    }

    if ( ReadBIT (pabyInput, nBitOffsetFromStart) )
    {
        text->vectExtrusion = CADVector(0.0f, 0.0f, 1.0f);
    }
    else
    {
        CADVector vectExtrusion = ReadVector(pabyInput, nBitOffsetFromStart);
        text->vectExtrusion = vectExtrusion;
    }

    text->dfThickness = ReadBIT (pabyInput, nBitOffsetFromStart) ?
                          0.0f : ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);

    if ( !( text->DataFlags & 0x04 ) )
        text->dfObliqueAng = ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);
    if ( !( text->DataFlags & 0x08 ) )
        text->dfRotationAng = ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);

    text->dfHeight = ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);

    if ( !( text->DataFlags & 0x10 ) )
        text->dfWidthFactor = ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);

    text->sTextValue = ReadTV (pabyInput, nBitOffsetFromStart);

    if ( !( text->DataFlags & 0x20 ) )
        text->dGeneration = ReadBITSHORT (pabyInput, nBitOffsetFromStart);
    if ( !( text->DataFlags & 0x40 ) )
        text->dHorizAlign = ReadBITSHORT (pabyInput, nBitOffsetFromStart);
    if ( !( text->DataFlags & 0x80 ) )
        text->dVertAlign = ReadBITSHORT (pabyInput, nBitOffsetFromStart);

    fillCommonEntityHandleData(text, pabyInput, nBitOffsetFromStart);

    text->hStyle = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 ); // padding bits to next byte boundary
    text->setCRC (ReadRAWSHORT (pabyInput, nBitOffsetFromStart));

#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif // _DEBUG

    return text;
}

CADVertex3DObject *DWGFileR2000::getVertex3D(long dObjectSize,
                                             CADCommonED stCommonEntityData,
                                             const char *pabyInput,
                                             size_t &nBitOffsetFromStart)
{
    CADVertex3DObject * vertex = new CADVertex3DObject();

    vertex->setSize(dObjectSize);
    vertex->stCed = stCommonEntityData;

    /*unsigned char Flags = */ReadCHAR (pabyInput, nBitOffsetFromStart);

    CADVector vertPosition = ReadVector(pabyInput, nBitOffsetFromStart);;
    vertex->vertPosition = vertPosition;

    fillCommonEntityHandleData(vertex, pabyInput, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 ); // padding bits to next byte boundary
    vertex->setCRC(ReadRAWSHORT (pabyInput, nBitOffsetFromStart));

#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif // _DEBUG
    return vertex;
}

CADCircleObject *DWGFileR2000::getCircle(long dObjectSize,
                                         CADCommonED stCommonEntityData,
                                         const char *pabyInput,
                                         size_t &nBitOffsetFromStart)
{
    CADCircleObject * circle = new CADCircleObject();

    circle->setSize(dObjectSize);
    circle->stCed = stCommonEntityData;

    CADVector vertPosition = ReadVector(pabyInput, nBitOffsetFromStart);
    circle->vertPosition = vertPosition;
    circle->dfRadius    = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
    circle->dfThickness = ReadBIT (pabyInput, nBitOffsetFromStart) ?
                          0.0f : ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);

    if ( ReadBIT (pabyInput, nBitOffsetFromStart) )
    {
        circle->vectExtrusion = CADVector(0.0f, 0.0f, 1.0f);
    }
    else
    {
        CADVector vectExtrusion = ReadVector(pabyInput, nBitOffsetFromStart);
        circle->vectExtrusion = vectExtrusion;
    }

    fillCommonEntityHandleData(circle, pabyInput, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 ); // padding bits to next byte boundary
    circle->setCRC(ReadRAWSHORT (pabyInput, nBitOffsetFromStart));

#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif // _DEBUG
    return circle;
}

CADEndblkObject *DWGFileR2000::getEndBlock(long dObjectSize,
                                           CADCommonED stCommonEntityData,
                                           const char *pabyInput,
                                           size_t &nBitOffsetFromStart)
{
    CADEndblkObject * endblk = new CADEndblkObject();

    endblk->setSize(dObjectSize);
    endblk->stCed = stCommonEntityData;

    fillCommonEntityHandleData(endblk, pabyInput, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
    endblk->setCRC(ReadRAWSHORT (pabyInput, nBitOffsetFromStart));

#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif //_DEBUG
    return endblk;
}

CADPolyline2DObject *DWGFileR2000::getPolyline2D(long dObjectSize,
                                                 CADCommonED stCommonEntityData,
                                                 const char *pabyInput,
                                                 size_t &nBitOffsetFromStart)
{
    CADPolyline2DObject * polyline = new CADPolyline2DObject();

    polyline->setSize (dObjectSize);
    polyline->stCed = stCommonEntityData;

    polyline->dFlags = ReadBITSHORT (pabyInput, nBitOffsetFromStart);
    polyline->dCurveNSmoothSurfType = ReadBITSHORT (pabyInput, nBitOffsetFromStart);

    polyline->dfStartWidth = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
    polyline->dfEndWidth = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);

    polyline->dfThickness = ReadBIT (pabyInput, nBitOffsetFromStart) ?
                            0.0f : ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);

    polyline->dfElevation = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);

    if ( ReadBIT (pabyInput, nBitOffsetFromStart) )
    {
        polyline->vectExtrusion = CADVector(0.0f, 0.0f, 1.0f);
    }
    else
    {
        CADVector vectExtrusion = ReadVector(pabyInput, nBitOffsetFromStart);
        polyline->vectExtrusion = vectExtrusion;
    }

    fillCommonEntityHandleData(polyline, pabyInput, nBitOffsetFromStart);

    polyline->hVertexes.push_back ( ReadHANDLE (pabyInput, nBitOffsetFromStart) ); // 1st vertex
    polyline->hVertexes.push_back ( ReadHANDLE (pabyInput, nBitOffsetFromStart) ); // last vertex

    polyline->hSeqend = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 ); // padding bits to next byte boundary
    polyline->setCRC(ReadRAWSHORT (pabyInput, nBitOffsetFromStart));

#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif // _DEBUG
    return polyline;
}

CADAttribObject *DWGFileR2000::getAttributes(long dObjectSize,
                                            CADCommonED stCommonEntityData,
                                            const char *pabyInput,
                                            size_t &nBitOffsetFromStart)
{
    CADAttribObject * attrib = new CADAttribObject();

    attrib->stCed = stCommonEntityData;
    attrib->DataFlags = ReadCHAR (pabyInput, nBitOffsetFromStart);

    if ( !(attrib->DataFlags & 0x01) )
        attrib->dfElevation = ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);

    double x, y;

    CADVector vertInsetionPoint = ReadRAWVector(pabyInput, nBitOffsetFromStart);
    attrib->vertInsetionPoint = vertInsetionPoint;

    if ( !(attrib->DataFlags & 0x02) )
    {
        x = ReadBITDOUBLEWD (pabyInput, nBitOffsetFromStart,
                             vertInsetionPoint.getX());
        y = ReadBITDOUBLEWD (pabyInput, nBitOffsetFromStart,
                             vertInsetionPoint.getY());
        CADVector vertAlignmentPoint(x, y);
        attrib->vertAlignmentPoint = vertAlignmentPoint;
    }

    if ( ReadBIT (pabyInput, nBitOffsetFromStart) )
    {
        attrib->vectExtrusion = CADVector(0.0f, 0.0f, 1.0f);
    }
    else
    {
        CADVector vectExtrusion = ReadVector(pabyInput, nBitOffsetFromStart);
        attrib->vectExtrusion = vectExtrusion;
    }

    attrib->dfThickness = ReadBIT (pabyInput, nBitOffsetFromStart) ?
                        0.0f : ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);

    if ( !(attrib->DataFlags & 0x04) )
        attrib->dfObliqueAng = ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);
    if ( !(attrib->DataFlags & 0x08) )
        attrib->dfRotationAng = ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);
    attrib->dfHeight = ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);
    if ( !(attrib->DataFlags & 0x10) )
        attrib->dfWidthFactor = ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);
    attrib->sTextValue = ReadTV (pabyInput, nBitOffsetFromStart);
    if ( !(attrib->DataFlags & 0x20) )
        attrib->dGeneration = ReadBITSHORT (pabyInput, nBitOffsetFromStart);
    if ( !(attrib->DataFlags & 0x40) )
        attrib->dHorizAlign = ReadBITSHORT (pabyInput, nBitOffsetFromStart);
    if ( !(attrib->DataFlags & 0x80) )
        attrib->dVertAlign = ReadBITSHORT (pabyInput, nBitOffsetFromStart);

    attrib->sTag = ReadTV (pabyInput, nBitOffsetFromStart);
    attrib->nFieldLength = ReadBITSHORT (pabyInput, nBitOffsetFromStart);
    attrib->nFlags = ReadCHAR (pabyInput, nBitOffsetFromStart);

    fillCommonEntityHandleData(attrib, pabyInput, nBitOffsetFromStart);

    attrib->hStyle = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
    attrib->setCRC(ReadRAWSHORT (pabyInput, nBitOffsetFromStart));

#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
            __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif //_DEBUG
    return attrib;
}

CADAttdefObject *DWGFileR2000::getAttributesDefn(long dObjectSize,
                                                 CADCommonED stCommonEntityData,
                                                 const char *pabyInput,
                                                 size_t &nBitOffsetFromStart)
{
    CADAttdefObject * attdef = new CADAttdefObject();
    attdef->stCed = stCommonEntityData;
    attdef->DataFlags = ReadCHAR (pabyInput, nBitOffsetFromStart);

    if ( !(attdef->DataFlags & 0x01) )
        attdef->dfElevation = ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);

    CADVector vertInsetionPoint = ReadRAWVector(pabyInput, nBitOffsetFromStart);
    attdef->vertInsetionPoint = vertInsetionPoint;

    if ( !(attdef->DataFlags & 0x02) )
    {
        double x = ReadBITDOUBLEWD (pabyInput, nBitOffsetFromStart,
                                    vertInsetionPoint.getX());
        double y = ReadBITDOUBLEWD (pabyInput, nBitOffsetFromStart,
                                    vertInsetionPoint.getY());
        CADVector vertAlignmentPoint(x, y);
        attdef->vertAlignmentPoint = vertAlignmentPoint;
    }

    if ( ReadBIT (pabyInput, nBitOffsetFromStart) )
    {
        attdef->vectExtrusion = CADVector(0.0f, 0.0f, 1.0f);
    }
    else
    {
        CADVector vectExtrusion = ReadVector(pabyInput, nBitOffsetFromStart);
        attdef->vectExtrusion = vectExtrusion;
    }

    attdef->dfThickness = ReadBIT (pabyInput, nBitOffsetFromStart) ?
                          0.0f : ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);

    if ( !(attdef->DataFlags & 0x04) )
        attdef->dfObliqueAng = ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);
    if ( !(attdef->DataFlags & 0x08) )
        attdef->dfRotationAng = ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);
    attdef->dfHeight = ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);
    if ( !(attdef->DataFlags & 0x10) )
        attdef->dfWidthFactor = ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);
    attdef->sTextValue = ReadTV (pabyInput, nBitOffsetFromStart);
    if ( !(attdef->DataFlags & 0x20) )
        attdef->dGeneration = ReadBITSHORT (pabyInput, nBitOffsetFromStart);
    if ( !(attdef->DataFlags & 0x40) )
        attdef->dHorizAlign = ReadBITSHORT (pabyInput, nBitOffsetFromStart);
    if ( !(attdef->DataFlags & 0x80) )
        attdef->dVertAlign = ReadBITSHORT (pabyInput, nBitOffsetFromStart);

    attdef->sTag = ReadTV (pabyInput, nBitOffsetFromStart);
    attdef->nFieldLength = ReadBITSHORT (pabyInput, nBitOffsetFromStart);
    attdef->nFlags = ReadCHAR (pabyInput, nBitOffsetFromStart);

    attdef->sPrompt = ReadTV (pabyInput, nBitOffsetFromStart);

    fillCommonEntityHandleData(attdef, pabyInput, nBitOffsetFromStart);

    attdef->hStyle = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
    attdef->setCRC( ReadRAWSHORT (pabyInput, nBitOffsetFromStart) );

#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif //_DEBUG
    return  attdef;
            }

CADLWPolylineObject *DWGFileR2000::getLWPolyLine(long dObjectSize,
                                                 CADCommonED stCommonEntityData,
                                                 const char *pabyInput,
                                                 size_t &nBitOffsetFromStart)
{
    CADLWPolylineObject * polyline   = new CADLWPolylineObject ();
    polyline->setSize(dObjectSize);
    polyline->stCed = stCommonEntityData;

    double x, y;
    int vertixesCount  = 0, nBulges = 0, nNumWidths = 0;
    short dataFlag = ReadBITSHORT (pabyInput, nBitOffsetFromStart);
    if ( dataFlag & 4 )
        polyline->dfConstWidth = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
    if ( dataFlag & 8 )
        polyline->dfElevation  = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
    if ( dataFlag & 2 )
        polyline->dfThickness  = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
    if ( dataFlag & 1 )
    {
        CADVector vectExtrusion = ReadVector(pabyInput, nBitOffsetFromStart);
        polyline->vectExtrusion = vectExtrusion;
    }

    vertixesCount = ReadBITLONG (pabyInput, nBitOffsetFromStart);

    if ( dataFlag & 16 )
    {
        nBulges = ReadBITLONG (pabyInput, nBitOffsetFromStart);
    }

    // TODO: tell ODA that R2000 contains nNumWidths flag
    if ( dataFlag & 32 )
    {
        nNumWidths = ReadBITLONG (pabyInput, nBitOffsetFromStart);
    }

    // First of all, read first vertex.
    CADVector vertex = ReadRAWVector(pabyInput, nBitOffsetFromStart);
    polyline->avertVertexes.push_back (vertex);

    // All the others are not raw doubles; bitdoubles with default instead,
    // where default is previous point coords.
    size_t prev;
    for ( int i = 1; i < vertixesCount; ++i )
    {
        prev = size_t(i - 1);
        x = ReadBITDOUBLEWD (pabyInput, nBitOffsetFromStart,
                                           polyline->avertVertexes[prev].getX());
        y = ReadBITDOUBLEWD (pabyInput, nBitOffsetFromStart,
                                           polyline->avertVertexes[prev].getY());
        CADVector vertex(x, y);
        polyline->avertVertexes.push_back (vertex);
    }

    for ( int i = 0; i < nBulges; ++i )
    {
        double dfBulgeValue = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
        polyline->adfBulges.push_back (dfBulgeValue);
    }

    for ( int i = 0; i < nNumWidths; ++i )
    {
        double dfStartWidth = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
        double dfEndWidth = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
        polyline->astWidths.push_back ( make_pair ( dfStartWidth, dfEndWidth ) );
    }

    fillCommonEntityHandleData(polyline, pabyInput, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
    polyline->setCRC (ReadRAWSHORT (pabyInput, nBitOffsetFromStart));

#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, ( nBitOffsetFromStart / 8 - dObjectSize - 4 ));
#endif //_DEBUG
    return polyline;
}

CADArcObject *DWGFileR2000::getArc(long dObjectSize,
                                   CADCommonED stCommonEntityData,
                                   const char *pabyInput,
                                   size_t &nBitOffsetFromStart)
{
    CADArcObject * arc = new CADArcObject();

    arc->stCed = stCommonEntityData;

    CADVector vertPosition = ReadVector(pabyInput, nBitOffsetFromStart);
    arc->vertPosition    = vertPosition;
    arc->dfRadius        = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
    arc->dfThickness     = ReadBIT (pabyInput, nBitOffsetFromStart) ?
                           0.0f : ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);

    if ( ReadBIT (pabyInput, nBitOffsetFromStart) )
    {
        arc->vectExtrusion = CADVector(0.0f, 0.0f, 1.0f);
    }
    else
    {
        CADVector vectExtrusion = ReadVector(pabyInput, nBitOffsetFromStart);
        arc->vectExtrusion = vectExtrusion;
    }

    arc->dfStartAngle = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
    arc->dfEndAngle   = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);

    fillCommonEntityHandleData(arc, pabyInput, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
    arc->setCRC (ReadRAWSHORT (pabyInput, nBitOffsetFromStart));
#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif // _DEBUG
    return arc;
}

CADSplineObject *DWGFileR2000::getSpline(long dObjectSize,
                                         CADCommonED stCommonEntityData,
                                         const char *pabyInput,
                                         size_t &nBitOffsetFromStart)
{
    CADSplineObject * spline = new CADSplineObject();
    spline->setSize(dObjectSize);
    spline->stCed = stCommonEntityData;
    spline->dScenario = ReadBITLONG (pabyInput, nBitOffsetFromStart);
    spline->dDegree  = ReadBITLONG (pabyInput, nBitOffsetFromStart);

    if ( spline->dScenario == 2 )
    {
        spline->dfFitTol = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
        CADVector vectBegTangDir = ReadVector(pabyInput, nBitOffsetFromStart);
        spline->vectBegTangDir = vectBegTangDir;
        CADVector vectEndTangDir = ReadVector(pabyInput, nBitOffsetFromStart);
        spline->vectEndTangDir = vectEndTangDir;
        spline->nNumFitPts = ReadBITLONG (pabyInput, nBitOffsetFromStart);
    }
    else if ( spline->dScenario == 1 )
    {
        spline->bRational = ReadBIT (pabyInput, nBitOffsetFromStart);
        spline->bClosed = ReadBIT (pabyInput, nBitOffsetFromStart);
        spline->bPeriodic = ReadBIT (pabyInput, nBitOffsetFromStart);
        spline->dfKnotTol = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
        spline->dfCtrlTol = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
        spline->nNumKnots = ReadBITLONG (pabyInput, nBitOffsetFromStart);
        spline->nNumCtrlPts = ReadBITLONG (pabyInput, nBitOffsetFromStart);
        spline->bWeight = ReadBIT (pabyInput, nBitOffsetFromStart);
    }
#ifdef _DEBUG
    else
    {
        DebugMsg ("Spline scenario != {1,2} readed: error.");
    }
#endif
    for ( long i = 0; i < spline->nNumKnots; ++i )
        spline->adfKnots.push_back ( ReadBITDOUBLE (pabyInput, nBitOffsetFromStart) );
    for ( long i = 0; i < spline->nNumCtrlPts; ++i )
    {
        CADVector vertex = ReadVector(pabyInput, nBitOffsetFromStart);
        spline->avertCtrlPoints.push_back ( vertex );
        if ( spline->bWeight )
            spline->adfCtrlPointsWeight.push_back (
                        ReadBITDOUBLE (pabyInput, nBitOffsetFromStart) );
    }
    for ( long i = 0; i < spline->nNumFitPts; ++i )
    {
        CADVector vertex = ReadVector(pabyInput, nBitOffsetFromStart);
        spline->averFitPoints.push_back ( vertex );
    }

    fillCommonEntityHandleData(spline, pabyInput, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
    spline->setCRC (ReadRAWSHORT (pabyInput, nBitOffsetFromStart));

#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif //_DEBUG
    return spline;
}

CADEntityObject *DWGFileR2000::getEntity(int dObjectType,
                                         long dObjectSize,
                                         CADCommonED stCommonEntityData,
                                         const char *pabyInput,
                                         size_t &nBitOffsetFromStart)
{
    CADEntityObject * entity = new CADEntityObject();

    entity->setType (static_cast<CADObject::ObjectType>(dObjectType));
    entity->setSize (dObjectSize);
    entity->stCed = stCommonEntityData;

    nBitOffsetFromStart = static_cast<size_t>(
                entity->stCed.nObjectSizeInBits + 16);

    fillCommonEntityHandleData(entity, pabyInput, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
    entity->setCRC (ReadRAWSHORT (pabyInput, nBitOffsetFromStart));
#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif //_DEBUG
    return entity;
}

CADInsertObject *DWGFileR2000::getInsert(int dObjectType, long dObjectSize,
                                         CADCommonED stCommonEntityData,
                                         const char *pabyInput,
                                         size_t &nBitOffsetFromStart)
{
    CADInsertObject * insert = new CADInsertObject();

    insert->setType (static_cast<CADObject::ObjectType>(dObjectType));
    insert->setSize (dObjectSize);
    insert->stCed = stCommonEntityData;

    insert->vertInsertionPoint = ReadVector(pabyInput, nBitOffsetFromStart);
    unsigned char dataFlags = Read2B(pabyInput, nBitOffsetFromStart);
    double val41 = 1.0;
    double val42 = 1.0;
    double val43 = 1.0;
    if(dataFlags == 0){
        val41 = ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);
        val42 = ReadBITDOUBLEWD (pabyInput, nBitOffsetFromStart, val41);
        val43 = ReadBITDOUBLEWD (pabyInput, nBitOffsetFromStart, val41);
    }
    else if(dataFlags == 1){
        val41 = 1.0;
        val42 = ReadBITDOUBLEWD (pabyInput, nBitOffsetFromStart, val41);
        val43 = ReadBITDOUBLEWD (pabyInput, nBitOffsetFromStart, val41);
    }
    else if(dataFlags == 2){
        val41 = ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);
        val42 = val41;
        val43 = val41;
    }
    insert->vertScales = CADVector(val41, val42, val43);
    insert->dfRotation = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
    insert->vectExtrusion = ReadVector (pabyInput, nBitOffsetFromStart);
    insert->bHasAttribs = ReadBIT (pabyInput, nBitOffsetFromStart);

    fillCommonEntityHandleData(insert, pabyInput, nBitOffsetFromStart);

    insert->hBlockHeader = ReadHANDLE (pabyInput, nBitOffsetFromStart);
    if(insert->bHasAttribs){
        insert->hAtrribs.push_back (ReadHANDLE (pabyInput, nBitOffsetFromStart));
        insert->hAtrribs.push_back (ReadHANDLE (pabyInput, nBitOffsetFromStart));
        insert->hSeqend = ReadHANDLE (pabyInput, nBitOffsetFromStart);
    }

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
    insert->setCRC (ReadRAWSHORT (pabyInput, nBitOffsetFromStart));
#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif //_DEBUG

    return insert;
}

CADDictionaryObject *DWGFileR2000::getDictionary(long dObjectSize,
                                                 const char *pabyInput,
                                                 size_t &nBitOffsetFromStart)
{
    /*
     * FIXME: ODA has a lot of mistypes in spec. for this objects,
     * it doesnt work for now (error begins in handles stream).
     * Nonetheless, dictionary->sItemNames is 100% array,
     * not a single obj as pointer by their docs.
     */
    CADDictionaryObject * dictionary = new CADDictionaryObject();

    dictionary->setSize (dObjectSize);
    dictionary->nObjectSizeInBits = ReadRAWLONG (pabyInput, nBitOffsetFromStart);
    dictionary->hObjectHandle = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    short dEEDSize = 0;
    CADEed dwgEed;
    while ( (dEEDSize = ReadBITSHORT (pabyInput, nBitOffsetFromStart)) != 0 )
    {
        dwgEed.dLength = dEEDSize;
        dwgEed.hApplication = ReadHANDLE (pabyInput, nBitOffsetFromStart);

        for ( short i = 0; i < dEEDSize; ++i )
        {
            dwgEed.acData.push_back(ReadCHAR (pabyInput, nBitOffsetFromStart));
        }

        dictionary->aEED.push_back (dwgEed);
    }

    dictionary->nNumReactors = ReadBITSHORT (pabyInput, nBitOffsetFromStart);
    dictionary->nNumItems = ReadBITLONG (pabyInput, nBitOffsetFromStart);
    dictionary->dCloningFlag = ReadBITSHORT (pabyInput, nBitOffsetFromStart);
    dictionary->dHardOwnerFlag = ReadCHAR (pabyInput, nBitOffsetFromStart);

    for ( long i = 0; i < dictionary->nNumItems; ++i )
        dictionary->sItemNames.push_back ( ReadTV (pabyInput,
                                                   nBitOffsetFromStart) );

    dictionary->hParentHandle = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    for ( long i = 0; i < dictionary->nNumReactors; ++i )
        dictionary->hReactors.push_back (ReadHANDLE (pabyInput,
                                                     nBitOffsetFromStart) );
    dictionary->hXDictionary = ReadHANDLE (pabyInput, nBitOffsetFromStart);
    for ( long i = 0; i < dictionary->nNumItems; ++i )
        dictionary->hItemHandles.push_back ( ReadHANDLE (pabyInput,
                                                         nBitOffsetFromStart) );

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
    dictionary->setCRC (ReadRAWSHORT (pabyInput, nBitOffsetFromStart));

#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif // _DEBUG
    return dictionary;
}

CADLayerObject *DWGFileR2000::getLayerObject(long dObjectSize,
                                       const char *pabyInput,
                                       size_t &nBitOffsetFromStart)
{
    CADLayerObject * layer = new CADLayerObject();

    layer->setSize (dObjectSize);
    layer->nObjectSizeInBits = ReadRAWLONG (pabyInput, nBitOffsetFromStart);
    layer->hObjectHandle = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    short dEEDSize = 0;
    CADEed dwgEed;
    while ( (dEEDSize = ReadBITSHORT (pabyInput, nBitOffsetFromStart)) != 0 )
    {
        dwgEed.dLength = dEEDSize;
        dwgEed.hApplication = ReadHANDLE (pabyInput, nBitOffsetFromStart);

        for ( short i = 0; i < dEEDSize; ++i )
        {
            dwgEed.acData.push_back(ReadCHAR (pabyInput, nBitOffsetFromStart));
        }

        layer->aEED.push_back (dwgEed);
    }

    layer->nNumReactors = ReadBITLONG (pabyInput, nBitOffsetFromStart);
    layer->sLayerName = ReadTV (pabyInput, nBitOffsetFromStart);
    layer->b64Flag = ReadBIT (pabyInput, nBitOffsetFromStart);
    layer->dXRefIndex = ReadBITSHORT (pabyInput, nBitOffsetFromStart);
    layer->bXDep = ReadBIT (pabyInput, nBitOffsetFromStart);

    short dFlags = ReadBITSHORT (pabyInput, nBitOffsetFromStart);
                layer->bFrozen = dFlags & 0x01;
                layer->bOn     = dFlags & 0x02;
                layer->bFrozenInNewVPORT = dFlags & 0x04;
                layer->bLocked = dFlags & 0x08;
                layer->bPlottingFlag = dFlags & 0x10;
                layer->dLineWeight = dFlags & 0x03E0;
    layer->dCMColor = ReadBITSHORT (pabyInput, nBitOffsetFromStart);
    layer->hLayerControl = ReadHANDLE (pabyInput, nBitOffsetFromStart);
    for ( long i = 0; i < layer->nNumReactors; ++i )
        layer->hReactors.push_back (ReadHANDLE (pabyInput, nBitOffsetFromStart) );
    layer->hXDictionary = ReadHANDLE (pabyInput, nBitOffsetFromStart);
    layer->hExternalRefBlockHandle = ReadHANDLE (pabyInput, nBitOffsetFromStart);
    layer->hPlotStyle = ReadHANDLE (pabyInput, nBitOffsetFromStart);
    layer->hLType = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    /*
     * FIXME: ODA says that this handle should be null hard pointer. It is not.
     * Also, after reading it dObjectSize is != actual readed structure's size.
     * Not used anyway, so no point to read it for now.
     * It also means that CRC cannot be computed correctly.
     */
// layer->hUnknownHandle = ReadHANDLE (pabySectionContent, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
    layer->setCRC (ReadRAWSHORT (pabyInput, nBitOffsetFromStart));

#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif //_DEBUG
    return layer;
}

CADLayerControlObject *DWGFileR2000::getLayerControl(long dObjectSize,
                                                     const char *pabyInput,
                                                     size_t &nBitOffsetFromStart)
{
    CADLayerControlObject * layerControl = new CADLayerControlObject();

    layerControl->setSize (dObjectSize);
    layerControl->nObjectSizeInBits = ReadRAWLONG (pabyInput, nBitOffsetFromStart);
    layerControl->hObjectHandle = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    short dEEDSize = 0;
    CADEed dwgEed;
    while ( (dEEDSize = ReadBITSHORT (pabyInput, nBitOffsetFromStart)) != 0 )
    {

        dwgEed.dLength = dEEDSize;
        dwgEed.hApplication = ReadHANDLE (pabyInput, nBitOffsetFromStart);

        for ( short i = 0; i < dEEDSize; ++i )
        {
            dwgEed.acData.push_back(ReadCHAR (pabyInput, nBitOffsetFromStart));
        }

        layerControl->aEED.push_back (dwgEed);
    }

    layerControl->nNumReactors = ReadBITLONG (pabyInput, nBitOffsetFromStart);
    layerControl->nNumEntries = ReadBITLONG (pabyInput, nBitOffsetFromStart);
    layerControl->hNull = ReadHANDLE (pabyInput, nBitOffsetFromStart);
    layerControl->hXDictionary = ReadHANDLE (pabyInput, nBitOffsetFromStart);
    for ( long i = 0; i < layerControl->nNumEntries; ++i )
        layerControl->hLayers.push_back( ReadHANDLE (pabyInput, nBitOffsetFromStart) );

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
    layerControl->setCRC (ReadRAWSHORT (pabyInput, nBitOffsetFromStart));
#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif
    return layerControl;
}

CADBlockControlObject *DWGFileR2000::getBlockControl(long dObjectSize,
                                                     const char *pabyInput,
                                                     size_t &nBitOffsetFromStart)
{
    CADBlockControlObject * blockControl = new CADBlockControlObject();

    blockControl->setSize (dObjectSize);
    blockControl->nObjectSizeInBits = ReadRAWLONG (pabyInput, nBitOffsetFromStart);
    blockControl->hObjectHandle = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    short dEEDSize = 0;
    CADEed dwgEed;
    while ( (dEEDSize = ReadBITSHORT (pabyInput, nBitOffsetFromStart)) != 0 )
    {
        dwgEed.dLength = dEEDSize;
        dwgEed.hApplication = ReadHANDLE (pabyInput, nBitOffsetFromStart);

        for ( short i = 0; i < dEEDSize; ++i )
        {
            dwgEed.acData.push_back(ReadCHAR (pabyInput, nBitOffsetFromStart));
        }

        blockControl->aEED.push_back (dwgEed);
    }

    blockControl->nNumReactors = ReadBITLONG (pabyInput, nBitOffsetFromStart);
    blockControl->nNumEntries = ReadBITLONG (pabyInput, nBitOffsetFromStart);

    blockControl->hNull = ReadHANDLE (pabyInput, nBitOffsetFromStart);
    blockControl->hXDictionary = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    for ( long i = 0; i < blockControl->nNumEntries + 2; ++i )
    {
        blockControl->hBlocks.push_back ( ReadHANDLE (pabyInput, nBitOffsetFromStart) );
    }

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
    blockControl->setCRC (ReadRAWSHORT (pabyInput, nBitOffsetFromStart));
#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif //_DEBUG
    return blockControl;
}

CADBlockHeaderObject *DWGFileR2000::getBlockHeader(long dObjectSize,
                                                   const char *pabyInput,
                                                   size_t &nBitOffsetFromStart)
{
    CADBlockHeaderObject * blockHeader = new CADBlockHeaderObject();

    blockHeader->setSize (dObjectSize);
    blockHeader->nObjectSizeInBits = ReadRAWLONG (pabyInput, nBitOffsetFromStart);
    blockHeader->hObjectHandle = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    short dEEDSize;
    CADEed dwgEed;
    while ( (dEEDSize = ReadBITSHORT (pabyInput, nBitOffsetFromStart)) != 0 )
    {
        dwgEed.dLength = dEEDSize;
        dwgEed.hApplication = ReadHANDLE (pabyInput, nBitOffsetFromStart);

        for ( short i = 0; i < dEEDSize; ++i )
        {
            dwgEed.acData.push_back(ReadCHAR (pabyInput, nBitOffsetFromStart));
        }

        blockHeader->aEED.push_back (dwgEed);
    }

    blockHeader->nNumReactors = ReadBITLONG (pabyInput, nBitOffsetFromStart);
    blockHeader->sEntryName = ReadTV (pabyInput, nBitOffsetFromStart);
    blockHeader->b64Flag = ReadBIT (pabyInput, nBitOffsetFromStart);
    blockHeader->dXRefIndex = ReadBITSHORT (pabyInput, nBitOffsetFromStart);
    blockHeader->bXDep = ReadBIT (pabyInput, nBitOffsetFromStart);
    blockHeader->bAnonymous = ReadBIT (pabyInput, nBitOffsetFromStart);
    blockHeader->bHasAtts = ReadBIT (pabyInput, nBitOffsetFromStart);
    blockHeader->bBlkisXRef = ReadBIT (pabyInput, nBitOffsetFromStart);
    blockHeader->bXRefOverlaid = ReadBIT (pabyInput, nBitOffsetFromStart);
    blockHeader->bLoadedBit = ReadBIT (pabyInput, nBitOffsetFromStart);

    CADVector vertBasePoint = ReadVector(pabyInput, nBitOffsetFromStart);
    blockHeader->vertBasePoint = vertBasePoint;
    blockHeader->sXRefPName = ReadTV (pabyInput, nBitOffsetFromStart);
    unsigned char Tmp;
    do
    {
        Tmp = ReadCHAR (pabyInput, nBitOffsetFromStart );
                    blockHeader->adInsertCount.push_back(Tmp);
    } while ( Tmp != 0 );

    blockHeader->sBlockDescription = ReadTV (pabyInput, nBitOffsetFromStart);
    blockHeader->nSizeOfPreviewData = ReadBITLONG (pabyInput, nBitOffsetFromStart);
    for ( long i = 0; i < blockHeader->nSizeOfPreviewData; ++i )
        blockHeader->abyBinaryPreviewData.push_back ( ReadCHAR (pabyInput,
                                                        nBitOffsetFromStart) );

    blockHeader->hBlockControl = ReadHANDLE (pabyInput, nBitOffsetFromStart);
    for ( long i = 0; i < blockHeader->nNumReactors; ++i )
        blockHeader->hReactors.push_back ( ReadHANDLE (pabyInput, nBitOffsetFromStart) );
    blockHeader->hXDictionary = ReadHANDLE (pabyInput, nBitOffsetFromStart);
    blockHeader->hNull = ReadHANDLE (pabyInput, nBitOffsetFromStart);
    blockHeader->hBlockEntity = ReadHANDLE (pabyInput, nBitOffsetFromStart);
    if ( !blockHeader->bBlkisXRef && !blockHeader->bXRefOverlaid )
    {
        blockHeader->hEntities.push_back ( ReadHANDLE(pabyInput, nBitOffsetFromStart) ); // first
        blockHeader->hEntities.push_back ( ReadHANDLE(pabyInput, nBitOffsetFromStart) ); // last
    }

    blockHeader->hEndBlk = ReadHANDLE (pabyInput, nBitOffsetFromStart);
    for ( size_t i = 0; i < blockHeader->adInsertCount.size() - 1; ++i )
        blockHeader->hInsertHandles.push_back ( ReadHANDLE (pabyInput, nBitOffsetFromStart) );
    blockHeader->hLayout = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
    blockHeader->setCRC (ReadRAWSHORT (pabyInput, nBitOffsetFromStart));
#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif //_DEBUG
    return blockHeader;
}

CADLineTypeControlObject *DWGFileR2000::getLineTypeControl(long dObjectSize,
                                                           const char *pabyInput,
                                                           size_t &nBitOffsetFromStart)
{
    CADLineTypeControlObject * ltypeControl = new CADLineTypeControlObject();
    ltypeControl->setSize (dObjectSize);
    ltypeControl->nObjectSizeInBits = ReadRAWLONG (pabyInput, nBitOffsetFromStart);
    ltypeControl->hObjectHandle = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    short dEEDSize = 0;
    CADEed dwgEed;
    while ( (dEEDSize = ReadBITSHORT (pabyInput, nBitOffsetFromStart)) != 0 )
    {
        dwgEed.dLength = dEEDSize;
        dwgEed.hApplication = ReadHANDLE (pabyInput, nBitOffsetFromStart);

        for ( short i = 0; i < dEEDSize; ++i )
        {
            dwgEed.acData.push_back(ReadCHAR (pabyInput, nBitOffsetFromStart));
        }

        ltypeControl->aEED.push_back (dwgEed);
    }

    ltypeControl->nNumReactors = ReadBITLONG (pabyInput, nBitOffsetFromStart);
    ltypeControl->nNumEntries = ReadBITLONG (pabyInput, nBitOffsetFromStart);

    ltypeControl->hNull = ReadHANDLE (pabyInput, nBitOffsetFromStart);
    ltypeControl->hXDictionary = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    // hLTypes ends with BYLAYER and BYBLOCK
    for ( long i = 0; i < ltypeControl->nNumEntries + 2; ++i )
        ltypeControl->hLTypes.push_back(
                    ReadHANDLE (pabyInput, nBitOffsetFromStart) );

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
    ltypeControl->setCRC(ReadRAWSHORT (pabyInput, nBitOffsetFromStart));

#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n"
                , __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif //_DEBUG
    return ltypeControl;
}

CADLineTypeObject *DWGFileR2000::getLineType1(long dObjectSize,
                                              const char *pabyInput,
                                              size_t &nBitOffsetFromStart)
{
    CADLineTypeObject * ltype = new CADLineTypeObject();

    ltype->setSize (dObjectSize);
    ltype->nObjectSizeInBits = ReadRAWLONG (pabyInput, nBitOffsetFromStart);
    ltype->hObjectHandle = ReadHANDLE (pabyInput, nBitOffsetFromStart);
    short dEEDSize = 0;
    CADEed dwgEed;
    while ( (dEEDSize = ReadBITSHORT (pabyInput, nBitOffsetFromStart)) != 0 )
    {
        dwgEed.dLength = dEEDSize;
        dwgEed.hApplication = ReadHANDLE (pabyInput, nBitOffsetFromStart);

        for ( short i = 0; i < dEEDSize; ++i )
        {
            dwgEed.acData.push_back(ReadCHAR (pabyInput, nBitOffsetFromStart));
        }

        ltype->aEED.push_back (dwgEed);
    }

    ltype->nNumReactors = ReadBITLONG (pabyInput, nBitOffsetFromStart);
    ltype->sEntryName = ReadTV (pabyInput, nBitOffsetFromStart);
    ltype->b64Flag = ReadBIT (pabyInput, nBitOffsetFromStart);
    ltype->dXRefIndex = ReadBITSHORT (pabyInput, nBitOffsetFromStart);
    ltype->bXDep = ReadBIT (pabyInput, nBitOffsetFromStart);
    ltype->sDescription = ReadTV (pabyInput, nBitOffsetFromStart);
    ltype->dfPatternLen = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
    ltype->dAlignment = ReadCHAR (pabyInput, nBitOffsetFromStart);
    ltype->nNumDashes = ReadCHAR (pabyInput, nBitOffsetFromStart);

    CADDash dash;
    for ( size_t i = 0; i < ltype->nNumDashes; ++i )
    {
        dash.dfLength = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
        dash.dComplexShapecode = ReadBITSHORT (pabyInput, nBitOffsetFromStart);
        dash.dfXOffset = ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);
        dash.dfYOffset = ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);
        dash.dfScale = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
        dash.dfRotation = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
        dash.dShapeflag = ReadBITSHORT (pabyInput, nBitOffsetFromStart); // TODO: what to do with it?

        ltype->astDashes.push_back ( dash );
    }

    for ( short i = 0; i < 256; ++i )
        ltype->abyTextArea.push_back ( ReadCHAR (pabyInput, nBitOffsetFromStart ) );

    ltype->hLTControl = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    for ( long i = 0; i < ltype->nNumReactors; ++i )
        ltype->hReactors.push_back ( ReadHANDLE (pabyInput, nBitOffsetFromStart) );

    ltype->hXDictionary = ReadHANDLE (pabyInput, nBitOffsetFromStart);
    ltype->hXRefBlock = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    // TODO: shapefile for dash/shape (1 each). Does it mean that we have nNumDashes * 2 handles, or what?

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
    ltype->setCRC (ReadRAWSHORT (pabyInput, nBitOffsetFromStart));
#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("[NOT IMPORTANT, CAUSE NOT IMPLEMENTATION NOT COMPLETED] "
                          "Assertion failed at %d in %s\nSize difference: %d\n"
                , __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif //_DEBUG
    return ltype;
}

CADMLineObject *DWGFileR2000::getMLine(long dObjectSize,
                                       CADCommonED stCommonEntityData,
                                       const char *pabyInput,
                                       size_t &nBitOffsetFromStart)
{
    CADMLineObject * mline = new CADMLineObject();

    mline->setSize (dObjectSize);
    mline->stCed = stCommonEntityData;

    mline->dfScale = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
    mline->dJust = ReadCHAR (pabyInput, nBitOffsetFromStart);

    CADVector vertBasePoint = ReadVector(pabyInput, nBitOffsetFromStart);
    mline->vertBasePoint = vertBasePoint;

    CADVector vectExtrusion = ReadVector(pabyInput, nBitOffsetFromStart);
    mline->vectExtrusion = vectExtrusion;
    mline->dOpenClosed = ReadBITSHORT (pabyInput, nBitOffsetFromStart);
    mline->nLinesInStyle = ReadCHAR (pabyInput, nBitOffsetFromStart);
    mline->nNumVertexes = ReadBITSHORT (pabyInput, nBitOffsetFromStart);

    CADMLineVertex stVertex;
    CADLineStyle stLStyle;
    for ( long i = 0; i < mline->nNumVertexes; ++i )
    {
        CADVector vertPosition = ReadVector(pabyInput, nBitOffsetFromStart);
        stVertex.vertPosition = vertPosition;

        CADVector vectDirection = ReadVector(pabyInput, nBitOffsetFromStart);
        stVertex.vectDirection = vectDirection;

        CADVector vectMIterDirection = ReadVector(pabyInput, nBitOffsetFromStart);
        stVertex.vectMIterDirection = vectMIterDirection;
        for ( size_t j = 0; j < mline->nLinesInStyle; ++j )
        {
            stLStyle.nNumSegParms = ReadBITSHORT (pabyInput, nBitOffsetFromStart);
            for ( short k = 0; k < stLStyle.nNumSegParms; ++k )
                stLStyle.adfSegparms.push_back ( ReadBITDOUBLE (pabyInput,
                                                                nBitOffsetFromStart) );
            stLStyle.nAreaFillParms = ReadBITSHORT (pabyInput, nBitOffsetFromStart);
            for ( short k = 0; k < stLStyle.nAreaFillParms; ++k )
                stLStyle.adfAreaFillParameters.push_back (
                            ReadBITDOUBLE (pabyInput, nBitOffsetFromStart) );

            stVertex.astLStyles.push_back (stLStyle);
        }
        mline->avertVertexes.push_back (stVertex);
    }

    if ( mline->stCed.bbEntMode == 0 )
        mline->stChed.hOwner = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    for ( long i = 0; i < mline->stCed.nNumReactors; ++i )
        mline->stChed.hReactors.push_back (
                    ReadHANDLE (pabyInput, nBitOffsetFromStart));

    mline->stChed.hXDictionary = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    if ( !mline->stCed.bNoLinks )
    {
        mline->stChed.hPrevEntity = ReadHANDLE (pabyInput, nBitOffsetFromStart);
        mline->stChed.hNextEntity = ReadHANDLE (pabyInput, nBitOffsetFromStart);
    }

    mline->stChed.hLayer = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    if ( mline->stCed.bbLTypeFlags == 0x03 )
        mline->stChed.hLType = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    if ( mline->stCed.bbPlotStyleFlags == 0x03 )
        mline->stChed.hPlotStyle = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
    mline->setCRC (ReadRAWSHORT (pabyInput, nBitOffsetFromStart));

#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif //_DEBUG
    return mline;
}

CADPolylinePFaceObject *DWGFileR2000::getPolylinePFace(long dObjectSize,
                                                       CADCommonED stCommonEntityData,
                                                       const char *pabyInput,
                                                       size_t &nBitOffsetFromStart)
{
    CADPolylinePFaceObject * polyline = new CADPolylinePFaceObject();

    polyline->setSize (dObjectSize);
    polyline->stCed = stCommonEntityData;

    polyline->nNumVertexes = ReadBITSHORT (pabyInput, nBitOffsetFromStart);
    polyline->nNumFaces = ReadBITSHORT (pabyInput, nBitOffsetFromStart);

    fillCommonEntityHandleData(polyline, pabyInput, nBitOffsetFromStart);

    polyline->hVertexes.push_back ( ReadHANDLE (pabyInput, nBitOffsetFromStart) ); // 1st vertex
    polyline->hVertexes.push_back ( ReadHANDLE (pabyInput, nBitOffsetFromStart) ); // last vertex

    polyline->hSeqend = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 ); // padding bits to next byte boundary
    polyline->setCRC (ReadRAWSHORT (pabyInput, nBitOffsetFromStart));

#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif //_DEBUG
    return polyline;
}

CADImageObject *DWGFileR2000::getImage(long dObjectSize,
                                       CADCommonED stCommonEntityData,
                                       const char *pabyInput,
                                       size_t &nBitOffsetFromStart)
{
    CADImageObject * image = new CADImageObject();

    image->setSize (dObjectSize);
    image->stCed = stCommonEntityData;

    image->dClassVersion = ReadBITLONG (pabyInput, nBitOffsetFromStart);

    CADVector vertInsertion = ReadVector(pabyInput, nBitOffsetFromStart);
    image->vertInsertion = vertInsertion;

    CADVector vectUDirection = ReadVector(pabyInput, nBitOffsetFromStart);
    image->vectUDirection = vectUDirection;

    CADVector vectVDirection = ReadVector(pabyInput, nBitOffsetFromStart);
    image->vectVDirection = vectVDirection;

    image->dfSizeX = ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);
    image->dfSizeY = ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);
    image->dDisplayProps = ReadBITSHORT (pabyInput, nBitOffsetFromStart);

    image->bClipping = ReadBIT (pabyInput, nBitOffsetFromStart);
    image->dBrightness = ReadCHAR (pabyInput, nBitOffsetFromStart);
    image->dContrast = ReadCHAR (pabyInput, nBitOffsetFromStart);
    image->dFade = ReadCHAR (pabyInput, nBitOffsetFromStart);
    image->dClipBoundaryType = ReadBITSHORT (pabyInput, nBitOffsetFromStart);

    if ( image->dClipBoundaryType == 1 )
    {
        CADVector vertPoint1 = ReadRAWVector(pabyInput, nBitOffsetFromStart);
        image->avertClippingPolygonVertexes.push_back(vertPoint1);

        CADVector vertPoint2 = ReadRAWVector(pabyInput, nBitOffsetFromStart);
        image->avertClippingPolygonVertexes.push_back(vertPoint2);
    }
    else
    {
        image->nNumberVertexesInClipPolygon =
                ReadBITLONG (pabyInput, nBitOffsetFromStart);

        for ( long i = 0; i < image->nNumberVertexesInClipPolygon; ++i )
        {
            CADVector vertPoint = ReadRAWVector(pabyInput, nBitOffsetFromStart);
            image->avertClippingPolygonVertexes.push_back(vertPoint);
        }
    }

    fillCommonEntityHandleData(image, pabyInput, nBitOffsetFromStart);

    image->hImageDef = ReadHANDLE (pabyInput, nBitOffsetFromStart);
    image->hImageDefReactor = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 ); // padding bits to next byte boundary
    image->setCRC (ReadRAWSHORT (pabyInput, nBitOffsetFromStart));

#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif //_DEBUG

    return image;
}

CAD3DFaceObject *DWGFileR2000::get3DFace(long dObjectSize,
                                       CADCommonED stCommonEntityData,
                                       const char *pabyInput,
                                       size_t &nBitOffsetFromStart)
{
    CAD3DFaceObject * face = new CAD3DFaceObject();

    face->setSize (dObjectSize);
    face->stCed = stCommonEntityData;

    face->bHasNoFlagInd = ReadBIT (pabyInput, nBitOffsetFromStart);
    face->bZZero = ReadBIT (pabyInput, nBitOffsetFromStart);

    double x, y, z;

    CADVector vertex = ReadRAWVector(pabyInput, nBitOffsetFromStart);
    if ( !face->bZZero ){
        z = ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);
        vertex.setZ (z);
    }
    face->avertCorners.push_back (vertex);
    for ( size_t i = 1; i < 4; ++i )
    {
        x = ReadBITDOUBLEWD (pabyInput, nBitOffsetFromStart,
                             face->avertCorners[i-1].getX());
        y = ReadBITDOUBLEWD (pabyInput, nBitOffsetFromStart,
                             face->avertCorners[i-1].getY());
        z = ReadBITDOUBLEWD (pabyInput, nBitOffsetFromStart,
                             face->avertCorners[i-1].getZ());

        CADVector corner(x, y, z);
        face->avertCorners.push_back (corner);
    }

    if ( !face->bHasNoFlagInd )
        face->dInvisFlags = ReadBITSHORT (pabyInput, nBitOffsetFromStart);

    fillCommonEntityHandleData(face, pabyInput, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 ); // padding bits to next byte boundary
    face->setCRC (ReadRAWSHORT (pabyInput, nBitOffsetFromStart));

#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif //_DEBUG
    return face;
}

CADVertexMeshObject *DWGFileR2000::getVertexMesh(long dObjectSize,
                                       CADCommonED stCommonEntityData,
                                       const char *pabyInput,
                                       size_t &nBitOffsetFromStart)
{
    CADVertexMeshObject * vertex = new CADVertexMeshObject();

    vertex->setSize (dObjectSize);
    vertex->stCed = stCommonEntityData;

    /*unsigned char Flags = */ReadCHAR (pabyInput, nBitOffsetFromStart);
    CADVector vertPosition = ReadVector(pabyInput, nBitOffsetFromStart);
    vertex->vertPosition = vertPosition;

    fillCommonEntityHandleData(vertex, pabyInput, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 ); // padding bits to next byte boundary
    vertex->setCRC (ReadRAWSHORT (pabyInput, nBitOffsetFromStart));

#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif
    return vertex;
}

CADVertexPFaceObject *DWGFileR2000::getVertexPFace(long dObjectSize,
                                       CADCommonED stCommonEntityData,
                                       const char *pabyInput,
                                       size_t &nBitOffsetFromStart)
{
    CADVertexPFaceObject * vertex = new CADVertexPFaceObject();

    vertex->setSize (dObjectSize);
    vertex->stCed = stCommonEntityData;

    /*unsigned char Flags = */ReadCHAR (pabyInput, nBitOffsetFromStart);
    CADVector vertPosition = ReadVector(pabyInput, nBitOffsetFromStart);
    vertex->vertPosition = vertPosition;

    fillCommonEntityHandleData(vertex, pabyInput, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 ); // padding bits to next byte boundary
    vertex->setCRC (ReadRAWSHORT (pabyInput, nBitOffsetFromStart));
#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif
    return vertex;
}

CADMTextObject *DWGFileR2000::getMText(long dObjectSize,
                                       CADCommonED stCommonEntityData,
                                       const char *pabyInput,
                                       size_t &nBitOffsetFromStart)
{
    CADMTextObject * text = new CADMTextObject();

    text->setSize (dObjectSize);
    text->stCed = stCommonEntityData;

    CADVector vertInsertionPoint = ReadVector(pabyInput, nBitOffsetFromStart);
    text->vertInsertionPoint = vertInsertionPoint;

    CADVector vectExtrusion = ReadVector(pabyInput, nBitOffsetFromStart);
    text->vectExtrusion = vectExtrusion;

    CADVector vectXAxisDir = ReadVector(pabyInput, nBitOffsetFromStart);
    text->vectXAxisDir = vectXAxisDir;

    text->dfRectWidth = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
    text->dfTextHeight = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
    text->dAttachment = ReadBITSHORT (pabyInput, nBitOffsetFromStart);
    text->dDrawingDir = ReadBITSHORT (pabyInput, nBitOffsetFromStart);
    text->dfExtents = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
    text->dfExtentsWidth = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
    text->sTextValue = ReadTV (pabyInput, nBitOffsetFromStart);
    text->dLineSpacingStyle = ReadBITSHORT (pabyInput, nBitOffsetFromStart);
    text->dLineSpacingFactor = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
    text->bUnknownBit = ReadBIT (pabyInput, nBitOffsetFromStart);

    fillCommonEntityHandleData(text, pabyInput, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
    text->setCRC (ReadRAWSHORT (pabyInput, nBitOffsetFromStart));
#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif
    return text;
}

CADDimensionObject *DWGFileR2000::getDimension(short dObjectType,long dObjectSize,
                                               CADCommonED stCommonEntityData,
                                               const char *pabyInput,
                                               size_t &nBitOffsetFromStart)
{
    CADCommonDimensionData stCDD;

    CADVector vectExtrusion = ReadVector(pabyInput, nBitOffsetFromStart);
    stCDD.vectExtrusion = vectExtrusion;

    CADVector vertTextMidPt = ReadRAWVector(pabyInput, nBitOffsetFromStart);
    stCDD.vertTextMidPt = vertTextMidPt;

    stCDD.dfElevation = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
    stCDD.dFlags = ReadCHAR (pabyInput, nBitOffsetFromStart);

    stCDD.sUserText = ReadTV (pabyInput, nBitOffsetFromStart);
    stCDD.dfTextRotation = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
    stCDD.dfHorizDir = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);

    stCDD.dfInsXScale = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
    stCDD.dfInsYScale = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
    stCDD.dfInsZScale = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
    stCDD.dfInsRotation = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);

    stCDD.dAttachmentPoint = ReadBITSHORT (pabyInput, nBitOffsetFromStart);
    stCDD.dLineSpacingStyle = ReadBITSHORT (pabyInput, nBitOffsetFromStart);
    stCDD.dfLineSpacingFactor = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
    stCDD.dfActualMeasurement = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);

    CADVector vert12Pt = ReadRAWVector(pabyInput, nBitOffsetFromStart);
    stCDD.vert12Pt = vert12Pt;

    switch(dObjectType)
    {
        case CADObject::DIMENSION_ORDINATE:
        {
            CADDimensionOrdinateObject * dimension =
                    new CADDimensionOrdinateObject();

            dimension->setSize (dObjectSize);
            dimension->stCed = stCommonEntityData;
            dimension->cdd = stCDD;

            CADVector vert10pt = ReadVector(pabyInput, nBitOffsetFromStart);
            dimension->vert10pt = vert10pt;

            CADVector vert13pt = ReadVector(pabyInput, nBitOffsetFromStart);
            dimension->vert13pt = vert13pt;

            CADVector vert14pt = ReadVector(pabyInput, nBitOffsetFromStart);
            dimension->vert14pt = vert14pt;

            dimension->Flags2 = ReadCHAR (pabyInput, nBitOffsetFromStart);

            fillCommonEntityHandleData(dimension, pabyInput, nBitOffsetFromStart);

            dimension->hDimstyle = ReadHANDLE (pabyInput, nBitOffsetFromStart);
            dimension->hAnonymousBlock = ReadHANDLE (pabyInput, nBitOffsetFromStart);

            nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
            dimension->setCRC (ReadRAWSHORT (pabyInput, nBitOffsetFromStart));
#ifdef _DEBUG
            if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
                DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                          __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif
            return  dimension;
        }

        case CADObject::DIMENSION_LINEAR:
        {
            CADDimensionLinearObject * dimension = new CADDimensionLinearObject();

            dimension->setSize (dObjectSize);
            dimension->stCed = stCommonEntityData;
            dimension->cdd = stCDD;

            CADVector vert13pt = ReadVector(pabyInput, nBitOffsetFromStart);
            dimension->vert13pt = vert13pt;

            CADVector vert14pt = ReadVector(pabyInput, nBitOffsetFromStart);
            dimension->vert14pt = vert14pt;

            CADVector vert10pt = ReadVector(pabyInput, nBitOffsetFromStart);
            dimension->vert10pt = vert10pt;

            dimension->dfExtLnRot = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);
            dimension->dfDimRot = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);

            fillCommonEntityHandleData(dimension, pabyInput, nBitOffsetFromStart);

            dimension->hDimstyle = ReadHANDLE (pabyInput, nBitOffsetFromStart);
            dimension->hAnonymousBlock = ReadHANDLE (pabyInput, nBitOffsetFromStart);

            nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
            dimension->setCRC (ReadRAWSHORT (pabyInput, nBitOffsetFromStart));
#ifdef _DEBUG
            if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
                DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                          __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif
            return  dimension;
        }

        case CADObject::DIMENSION_ALIGNED:
        {
            CADDimensionAlignedObject * dimension =
                    new CADDimensionAlignedObject();

            dimension->setSize (dObjectSize);
            dimension->stCed = stCommonEntityData;
            dimension->cdd = stCDD;

            CADVector vert13pt = ReadVector(pabyInput, nBitOffsetFromStart);
            dimension->vert13pt = vert13pt;

            CADVector vert14pt = ReadVector(pabyInput, nBitOffsetFromStart);
            dimension->vert14pt = vert14pt;

            CADVector vert10pt = ReadVector(pabyInput, nBitOffsetFromStart);
            dimension->vert10pt = vert10pt;

            dimension->dfExtLnRot =
                    ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);

            fillCommonEntityHandleData(dimension, pabyInput, nBitOffsetFromStart);

            dimension->hDimstyle = ReadHANDLE (pabyInput, nBitOffsetFromStart);
            dimension->hAnonymousBlock = ReadHANDLE (pabyInput, nBitOffsetFromStart);

            nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
            dimension->setCRC (ReadRAWSHORT (pabyInput, nBitOffsetFromStart));
#ifdef _DEBUG
            if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
                DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                          __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif
            return  dimension;
        }

        case CADObject::DIMENSION_ANG_3PT:
        {
            CADDimensionAngular3PtObject * dimension =
                    new CADDimensionAngular3PtObject();

            dimension->setSize (dObjectSize);
            dimension->stCed = stCommonEntityData;
            dimension->cdd = stCDD;

            CADVector vert10pt = ReadVector(pabyInput, nBitOffsetFromStart);
            dimension->vert10pt = vert10pt;

            CADVector vert13pt = ReadVector(pabyInput, nBitOffsetFromStart);
            dimension->vert13pt = vert13pt;

            CADVector vert14pt = ReadVector(pabyInput, nBitOffsetFromStart);
            dimension->vert14pt = vert14pt;

            CADVector vert15pt = ReadVector(pabyInput, nBitOffsetFromStart);
            dimension->vert15pt = vert15pt;

            fillCommonEntityHandleData(dimension, pabyInput, nBitOffsetFromStart);

            dimension->hDimstyle = ReadHANDLE (pabyInput, nBitOffsetFromStart);
            dimension->hAnonymousBlock = ReadHANDLE (pabyInput, nBitOffsetFromStart);

            nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
            dimension->setCRC (ReadRAWSHORT (pabyInput, nBitOffsetFromStart));
#ifdef _DEBUG
            if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
                DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                          __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif
            return  dimension;
        }

        case CADObject::DIMENSION_ANG_2LN:
        {
            CADDimensionAngular2LnObject * dimension =
                    new CADDimensionAngular2LnObject();

            dimension->setSize (dObjectSize);
            dimension->stCed = stCommonEntityData;
            dimension->cdd = stCDD;

            CADVector vert16pt = ReadVector(pabyInput, nBitOffsetFromStart);
            dimension->vert16pt = vert16pt;

            CADVector vert13pt = ReadVector(pabyInput, nBitOffsetFromStart);
            dimension->vert13pt = vert13pt;

            CADVector vert14pt = ReadVector(pabyInput, nBitOffsetFromStart);
            dimension->vert14pt = vert14pt;

            CADVector vert15pt = ReadVector(pabyInput, nBitOffsetFromStart);
            dimension->vert15pt = vert15pt;

            CADVector vert10pt = ReadVector(pabyInput, nBitOffsetFromStart);
            dimension->vert10pt = vert10pt;

            fillCommonEntityHandleData(dimension, pabyInput, nBitOffsetFromStart);

            dimension->hDimstyle = ReadHANDLE (pabyInput, nBitOffsetFromStart);
            dimension->hAnonymousBlock = ReadHANDLE (pabyInput, nBitOffsetFromStart);

            nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
            dimension->setCRC (ReadRAWSHORT (pabyInput, nBitOffsetFromStart));
#ifdef _DEBUG
            if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
                DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                          __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif
            return  dimension;
        }

        case CADObject::DIMENSION_RADIUS:
        {
            CADDimensionRadiusObject * dimension =
                    new CADDimensionRadiusObject();

            dimension->setSize (dObjectSize);
            dimension->stCed = stCommonEntityData;
            dimension->cdd = stCDD;

            CADVector vert10pt = ReadVector(pabyInput, nBitOffsetFromStart);
            dimension->vert10pt = vert10pt;

            CADVector vert15pt = ReadVector(pabyInput, nBitOffsetFromStart);
            dimension->vert15pt = vert15pt;

            dimension->dfLeaderLen = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);

            fillCommonEntityHandleData(dimension, pabyInput, nBitOffsetFromStart);

            dimension->hDimstyle = ReadHANDLE (pabyInput, nBitOffsetFromStart);
            dimension->hAnonymousBlock = ReadHANDLE (pabyInput, nBitOffsetFromStart);

            nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
            dimension->setCRC (ReadRAWSHORT (pabyInput, nBitOffsetFromStart));
#ifdef _DEBUG
            if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
                DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                          __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif
            return  dimension;
        }

        case CADObject::DIMENSION_DIAMETER:
        {
            CADDimensionDiameterObject * dimension =
                    new CADDimensionDiameterObject();

            dimension->setSize (dObjectSize);
            dimension->stCed = stCommonEntityData;
            dimension->cdd = stCDD;

            CADVector vert15pt = ReadVector(pabyInput, nBitOffsetFromStart);
            dimension->vert15pt = vert15pt;

            CADVector vert10pt = ReadVector(pabyInput, nBitOffsetFromStart);
            dimension->vert10pt = vert10pt;

            dimension->dfLeaderLen = ReadBITDOUBLE (pabyInput, nBitOffsetFromStart);

            fillCommonEntityHandleData(dimension, pabyInput, nBitOffsetFromStart);

            dimension->hDimstyle = ReadHANDLE (pabyInput, nBitOffsetFromStart);
            dimension->hAnonymousBlock = ReadHANDLE (pabyInput, nBitOffsetFromStart);

            nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
            dimension->setCRC (ReadRAWSHORT (pabyInput, nBitOffsetFromStart));
#ifdef _DEBUG
            if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
                DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                          __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif
            return  dimension;
    }
    }
    return nullptr;
}

CADImageDefObject *DWGFileR2000::getImageDef(long dObjectSize,
                                             const char *pabyInput,
                                             size_t &nBitOffsetFromStart)
{
    CADImageDefObject * imagedef = new CADImageDefObject();

    imagedef->setSize (dObjectSize);
    imagedef->nObjectSizeInBits = ReadRAWLONG (pabyInput, nBitOffsetFromStart);
    imagedef->hObjectHandle = ReadHANDLE8BLENGTH (pabyInput, nBitOffsetFromStart);

    short dEEDSize = 0;
    CADEed dwgEed;
    while ( (dEEDSize = ReadBITSHORT (pabyInput, nBitOffsetFromStart)) != 0 )
    {
        dwgEed.dLength = dEEDSize;
        dwgEed.hApplication = ReadHANDLE (pabyInput, nBitOffsetFromStart);

        for ( short i = 0; i < dEEDSize; ++i )
        {
            dwgEed.acData.push_back(ReadCHAR (pabyInput, nBitOffsetFromStart));
        }

        imagedef->aEED.push_back (dwgEed);
    }

    imagedef->nNumReactors = ReadBITLONG (pabyInput, nBitOffsetFromStart);
    imagedef->dClassVersion = ReadBITLONG (pabyInput, nBitOffsetFromStart);

    imagedef->dfXImageSizeInPx = ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);
    imagedef->dfYImageSizeInPx = ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);

    imagedef->sFilePath = ReadTV (pabyInput, nBitOffsetFromStart);
    imagedef->bIsLoaded = ReadBIT (pabyInput, nBitOffsetFromStart);

    imagedef->dResUnits = ReadCHAR (pabyInput, nBitOffsetFromStart);

    imagedef->dfXPixelSize = ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);
    imagedef->dfYPixelSize = ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);

    imagedef->hParentHandle = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    for ( long i = 0; i < imagedef->nNumReactors; ++i )
        imagedef->hReactors.push_back (ReadHANDLE (pabyInput, nBitOffsetFromStart) );

    imagedef->hXDictionary = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
    imagedef->setCRC (ReadRAWSHORT (pabyInput, nBitOffsetFromStart));
#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif

    return imagedef;
}

CADImageDefReactorObject *DWGFileR2000::getImageDefReactor(long dObjectSize,
                                             const char *pabyInput,
                                             size_t &nBitOffsetFromStart)

{
    CADImageDefReactorObject * imagedefreactor = new CADImageDefReactorObject();

    imagedefreactor->setSize (dObjectSize);
    imagedefreactor->nObjectSizeInBits = ReadRAWLONG (pabyInput, nBitOffsetFromStart);
    imagedefreactor->hObjectHandle = ReadHANDLE8BLENGTH (pabyInput, nBitOffsetFromStart);

    short dEEDSize = 0;
    CADEed dwgEed;
    while ( (dEEDSize = ReadBITSHORT (pabyInput, nBitOffsetFromStart)) != 0 )
    {
        dwgEed.dLength = dEEDSize;
        dwgEed.hApplication = ReadHANDLE (pabyInput, nBitOffsetFromStart);

        for ( short i = 0; i < dEEDSize; ++i )
        {
            dwgEed.acData.push_back(ReadCHAR (pabyInput, nBitOffsetFromStart));
        }

        imagedefreactor->aEED.push_back (dwgEed);
    }

    imagedefreactor->nNumReactors = ReadBITLONG (pabyInput, nBitOffsetFromStart);
    imagedefreactor->dClassVersion = ReadBITLONG (pabyInput, nBitOffsetFromStart);

    imagedefreactor->hParentHandle = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    for ( long i = 0; i < imagedefreactor->nNumReactors; ++i )
        imagedefreactor->hReactors.push_back (ReadHANDLE (pabyInput, nBitOffsetFromStart) );

    imagedefreactor->hXDictionary = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
    imagedefreactor->setCRC (ReadRAWSHORT (pabyInput, nBitOffsetFromStart));
#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif

    return imagedefreactor;
}

CADXRecordObject *DWGFileR2000::getXRecord(long dObjectSize,
                                           const char *pabyInput,
                                           size_t &nBitOffsetFromStart)

{
    CADXRecordObject * xrecord = new CADXRecordObject();

    xrecord->setSize (dObjectSize);
    xrecord->nObjectSizeInBits = ReadRAWLONG( pabyInput, nBitOffsetFromStart );
    xrecord->hObjectHandle = ReadHANDLE8BLENGTH( pabyInput, nBitOffsetFromStart );

    short dEEDSize = 0;
    CADEed dwgEed;
    while ( (dEEDSize = ReadBITSHORT( pabyInput, nBitOffsetFromStart ) ) != 0 )
    {
        dwgEed.dLength = dEEDSize;
        dwgEed.hApplication = ReadHANDLE (pabyInput, nBitOffsetFromStart);

        for ( short i = 0; i < dEEDSize; ++i )
        {
            dwgEed.acData.push_back( ReadCHAR (pabyInput, nBitOffsetFromStart) );
        }

        xrecord->aEED.push_back (dwgEed);
    }

    xrecord->nNumReactors = ReadBITLONG( pabyInput, nBitOffsetFromStart );
    xrecord->nNumDataBytes = ReadBITLONG (pabyInput, nBitOffsetFromStart);

    for( size_t i = 0; i < xrecord->nNumDataBytes; ++i )
    {
        xrecord->abyDataBytes.push_back( ReadCHAR(pabyInput, nBitOffsetFromStart) );
    }

    xrecord->dCloningFlag = ReadBITSHORT( pabyInput, nBitOffsetFromStart );

    short dIndicatorNumber = ReadRAWSHORT( pabyInput, nBitOffsetFromStart );
    if( dIndicatorNumber == 1 )
    {
        unsigned char nStringSize = ReadCHAR ( pabyInput, nBitOffsetFromStart );
        char dCodePage   = ReadCHAR ( pabyInput, nBitOffsetFromStart );
        for ( unsigned char i = 0; i < nStringSize; ++i )
        {
            ReadCHAR ( pabyInput, nBitOffsetFromStart );
        }
    }
    else if ( dIndicatorNumber == 70 )
    {
        ReadRAWSHORT (pabyInput, nBitOffsetFromStart);
    }
    else if ( dIndicatorNumber == 10 )
    {
        ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);
        ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);
        ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);
    }
    else if ( dIndicatorNumber == 40 )
    {
        ReadRAWDOUBLE (pabyInput, nBitOffsetFromStart);
    }

    xrecord->hParentHandle = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    for ( long i = 0; i < xrecord->nNumReactors; ++i )
        xrecord->hReactors.push_back (ReadHANDLE (pabyInput, nBitOffsetFromStart) );

    xrecord->hXDictionary = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    while( nBitOffsetFromStart / 8 < (dObjectSize + 4) )
    {
        xrecord->hObjIdHandles.push_back( ReadHANDLE(pabyInput, nBitOffsetFromStart) );
    }

    nBitOffsetFromStart += 8 - ( nBitOffsetFromStart % 8 );
    xrecord->setCRC (ReadRAWSHORT (pabyInput, nBitOffsetFromStart));
#ifdef _DEBUG
    if ( (nBitOffsetFromStart/8) != (dObjectSize + 4) )
        DebugMsg ("Assertion failed at %d in %s\nSize difference: %d\n",
                  __LINE__, __FILE__, (nBitOffsetFromStart/8 - dObjectSize - 4));
#endif

    return xrecord;
}

void DWGFileR2000::fillCommonEntityHandleData(CADEntityObject* pEnt,
                                              const char *pabyInput,
                                              size_t &nBitOffsetFromStart)
{
    if (pEnt->stCed.bbEntMode == 0 )
        pEnt->stChed.hOwner = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    for ( long i = 0; i < pEnt->stCed.nNumReactors; ++i )
        pEnt->stChed.hReactors.push_back (ReadHANDLE (pabyInput,
                                                        nBitOffsetFromStart));

    pEnt->stChed.hXDictionary = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    if ( !pEnt->stCed.bNoLinks )
    {
        pEnt->stChed.hPrevEntity = ReadHANDLE (pabyInput, nBitOffsetFromStart);
        pEnt->stChed.hNextEntity = ReadHANDLE (pabyInput, nBitOffsetFromStart);
    }

    pEnt->stChed.hLayer = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    if ( pEnt->stCed.bbLTypeFlags == 0x03 )
        pEnt->stChed.hLType = ReadHANDLE (pabyInput, nBitOffsetFromStart);

    if ( pEnt->stCed.bbPlotStyleFlags == 0x03 )
        pEnt->stChed.hPlotStyle = ReadHANDLE (pabyInput, nBitOffsetFromStart);
}

DWGFileR2000::DWGFileR2000(CADFileIO* poFileIO) : CADFile(poFileIO)
{
    header.addValue(CADHeader::OPENCADVER, CADVersions::DWG_R2000);
}

DWGFileR2000::~DWGFileR2000()
{
}

int DWGFileR2000::readSectionLocator()
{
    char    abyBuf[255];
    int     dImageSeeker, SLRecordsCount;
    short   dCodePage;

    fileIO->Rewind();
    memset(abyBuf, 0, DWG_VERSION_STR_SIZE + 1);
    fileIO->Read(abyBuf, DWG_VERSION_STR_SIZE);
    header.addValue(CADHeader::ACADVER, abyBuf);
    memset(abyBuf, 0, 8);
    fileIO->Read(abyBuf, 7);
    header.addValue(CADHeader::ACADMAINTVER, abyBuf);
    // TODO: code can be much simplified if CADHandle will be used.
    fileIO->Read (&dImageSeeker, 4);
    // to do so, == and ++ operators should be implemented.
    DebugMsg("Image seeker readed: %d\n", dImageSeeker);
    imageSeeker = dImageSeeker;

    fileIO->Seek (2, CADFileIO::SeekOrigin::CUR); // 19
    fileIO->Read (&dCodePage, 2);
    header.addValue(CADHeader::DWGCODEPAGE, dCodePage);

    DebugMsg("DWG Code page: %d\n", dCodePage);

    fileIO->Read (&SLRecordsCount, 4); // 21
                // Last vertex is reached. read it and break reading.
    DebugMsg("Section locator records count: %d\n", SLRecordsCount);

    for ( size_t i = 0; i < static_cast<size_t>(SLRecordsCount); ++i )
    {
        SectionLocatorRecord readedRecord;
        fileIO->Read (&readedRecord.byRecordNumber, 1);
        fileIO->Read (&readedRecord.dSeeker, 4);
        fileIO->Read (&readedRecord.dSize, 4);

        sectionLocatorRecords.push_back (readedRecord);
        DebugMsg("  Record #%d : %d %d\n",
                 sectionLocatorRecords[i].byRecordNumber,
                 sectionLocatorRecords[i].dSeeker,
                 sectionLocatorRecords[i].dSize);
    }

    return CADErrorCodes::SUCCESS;
}

// TODO: code is really bad. Just for test purposes only, will fix later.
string DWGFileR2000::getESRISpatialRef()
{
    unique_ptr< CADDictionaryObject > spoNamedDictObj( ( CADDictionaryObject* )
                                                               getObject (tables.getTableHandle (CADTables::NamedObjectsDict).getAsLong () ) );

    for( size_t i = 0; i < spoNamedDictObj->sItemNames.size(); ++i )
    {
        if ( !strcmp ("ESRI_PRJ", spoNamedDictObj->sItemNames[i].c_str()) )
        {
            unique_ptr<CADXRecordObject> spoXRecordObj (
                    ( CADXRecordObject * ) getObject (spoNamedDictObj->hItemHandles[i].getAsLong ()));

            if( spoXRecordObj.get() == nullptr ) return string("");

            size_t esri_prj_begins = 10000;
            for( size_t j = 0; j < spoXRecordObj->abyDataBytes.size(); ++j )
            {
                if( spoXRecordObj->abyDataBytes[j] == 'G' )
                {
                    if( spoXRecordObj->abyDataBytes[j+1] == 'E' )
                    {
                        esri_prj_begins = j;
                        break;
                    }
                }
            }

            if( esri_prj_begins > spoXRecordObj->abyDataBytes.size() )
            {
                return string("");
            }

            string esri_prj;
            for( size_t j = esri_prj_begins; j < spoXRecordObj->abyDataBytes.size(); ++j )
            {
                esri_prj.push_back( spoXRecordObj->abyDataBytes[j] );
            }

            return esri_prj;
        }
    }

    return string("");
}
