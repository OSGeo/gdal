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
 *  Copyright (c) 2016-2018 NextGIS, <info@nextgis.com>
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
#include "cadgeometry.h"
#include "cadobjects.h"
#include "opencad_api.h"
#include "r2000.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <string>

#if ((defined(__sun__) || defined(__FreeBSD__)) && __GNUC__ == 4 && __GNUC_MINOR__ == 8) || defined(__ANDROID__)
// gcc 4.8 on Solaris 11.3 or FreeBSD 11 doesn't have std::string
#include <sstream>
namespace std
{
template <typename T> std::string to_string(T val)
{
    std::ostringstream os;
    os << val;
    return os.str();
}
}
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

using namespace std;

int DWGFileR2000::ReadHeader( OpenOptions eOptions )
{
    char bufferPre[255];
    unsigned dHeaderVarsSectionLength = 0;
    const size_t dSizeOfSectionSize = 4;

    pFileIO->Seek( sectionLocatorRecords[0].dSeeker, CADFileIO::SeekOrigin::BEG );
    size_t readSize = pFileIO->Read( bufferPre, DWGConstants::SentinelLength );
    if(readSize < DWGConstants::SentinelLength)
    {
        DebugMsg( "File is corrupted (size is less than sentinel length)" );

        return CADErrorCodes::HEADER_SECTION_READ_FAILED;
    }

#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    if( memcmp( bufferPre, DWGConstants::HeaderVariablesStart,
                           DWGConstants::SentinelLength ) )
    {
        DebugMsg( "File is corrupted (wrong pointer to HEADER_VARS section,"
                          "or HEADERVARS starting sentinel corrupted.)" );

        return CADErrorCodes::HEADER_SECTION_READ_FAILED;
    }
#endif

    readSize = pFileIO->Read( &dHeaderVarsSectionLength, dSizeOfSectionSize );
        DebugMsg( "Header variables section length: %d\n",
                  static_cast<int>(dHeaderVarsSectionLength) );
    if(readSize != dSizeOfSectionSize || dHeaderVarsSectionLength > 65536) //NOTE: maybe header section may be bigger
    {
        DebugMsg( "File is corrupted (HEADER_VARS section length too big)" );
        return CADErrorCodes::HEADER_SECTION_READ_FAILED;
    }

    CADBuffer buffer(dHeaderVarsSectionLength + dSizeOfSectionSize + 10);
    buffer.WriteRAW(&dHeaderVarsSectionLength, dSizeOfSectionSize);
    readSize = pFileIO->Read(buffer.GetRawBuffer(), dHeaderVarsSectionLength + 2 );
    if(readSize != dHeaderVarsSectionLength + 2)
    {
        DebugMsg( "Failed to read %d byte of file. Read only %d",
                  static_cast<int>(dHeaderVarsSectionLength + 2),
                  static_cast<int>(readSize) );
        return CADErrorCodes::HEADER_SECTION_READ_FAILED;
    }

    if( eOptions == OpenOptions::READ_ALL )
    {
        oHeader.addValue( UNKNOWN1, buffer.ReadBITDOUBLE() );
        oHeader.addValue( UNKNOWN2, buffer.ReadBITDOUBLE() );
        oHeader.addValue( UNKNOWN3, buffer.ReadBITDOUBLE() );
        oHeader.addValue( UNKNOWN4, buffer.ReadBITDOUBLE() );
        oHeader.addValue( UNKNOWN5, buffer.ReadTV() );
        oHeader.addValue( UNKNOWN6, buffer.ReadTV() );
        oHeader.addValue( UNKNOWN7, buffer.ReadTV() );
        oHeader.addValue( UNKNOWN8, buffer.ReadTV() );
        oHeader.addValue( UNKNOWN9, buffer.ReadBITLONG() );
        oHeader.addValue( UNKNOWN10, buffer.ReadBITLONG() );
    }
    else
    {
        buffer.SkipBITDOUBLE();
        buffer.SkipBITDOUBLE();
        buffer.SkipBITDOUBLE();
        buffer.SkipBITDOUBLE();
        buffer.SkipTV();
        buffer.SkipTV();
        buffer.SkipTV();
        buffer.SkipTV();
        buffer.SkipBITLONG();
        buffer.SkipBITLONG();
    }

    CADHandle stCurrentViewportTable = buffer.ReadHANDLE();
    oTables.AddTable( CADTables::CurrentViewportTable, stCurrentViewportTable );

    if( eOptions == OpenOptions::READ_ALL )
    {
        oHeader.addValue( CADHeader::DIMASO, buffer.ReadBIT() );     // 1
        oHeader.addValue( CADHeader::DIMSHO, buffer.ReadBIT() );     // 2
        oHeader.addValue( CADHeader::PLINEGEN, buffer.ReadBIT() );   // 3
        oHeader.addValue( CADHeader::ORTHOMODE, buffer.ReadBIT() );  // 4
        oHeader.addValue( CADHeader::REGENMODE, buffer.ReadBIT() );  // 5
        oHeader.addValue( CADHeader::FILLMODE, buffer.ReadBIT() );   // 6
        oHeader.addValue( CADHeader::QTEXTMODE, buffer.ReadBIT() );  // 7
        oHeader.addValue( CADHeader::PSLTSCALE, buffer.ReadBIT() );  // 8
        oHeader.addValue( CADHeader::LIMCHECK, buffer.ReadBIT() );   // 9
        oHeader.addValue( CADHeader::USRTIMER, buffer.ReadBIT() );   // 10
        oHeader.addValue( CADHeader::SKPOLY, buffer.ReadBIT() );     // 11
        oHeader.addValue( CADHeader::ANGDIR, buffer.ReadBIT() );     // 12
        oHeader.addValue( CADHeader::SPLFRAME, buffer.ReadBIT() );   // 13
        oHeader.addValue( CADHeader::MIRRTEXT, buffer.ReadBIT() );   // 14
        oHeader.addValue( CADHeader::WORDLVIEW, buffer.ReadBIT() );  // 15
        oHeader.addValue( CADHeader::TILEMODE, buffer.ReadBIT() );   // 16
        oHeader.addValue( CADHeader::PLIMCHECK, buffer.ReadBIT() );  // 17
        oHeader.addValue( CADHeader::VISRETAIN, buffer.ReadBIT() );  // 18
        oHeader.addValue( CADHeader::DISPSILH, buffer.ReadBIT() );   // 19
        oHeader.addValue( CADHeader::PELLIPSE, buffer.ReadBIT() );   // 20
    }
    else
    {
        buffer.Seek(20);
    }

    if( eOptions == OpenOptions::READ_ALL )
    {
        oHeader.addValue( CADHeader::PROXYGRAPHICS, buffer.ReadBITSHORT() ); // 1
        oHeader.addValue( CADHeader::TREEDEPTH, buffer.ReadBITSHORT() );     // 2
        oHeader.addValue( CADHeader::LUNITS, buffer.ReadBITSHORT() );        // 3
        oHeader.addValue( CADHeader::LUPREC, buffer.ReadBITSHORT() );        // 4
        oHeader.addValue( CADHeader::AUNITS, buffer.ReadBITSHORT() );        // 5
        oHeader.addValue( CADHeader::AUPREC, buffer.ReadBITSHORT() );        // 6
    } else
    {
        for( char i = 0; i < 6; ++i )
            buffer.SkipBITSHORT();
    }

    oHeader.addValue( CADHeader::ATTMODE, buffer.ReadBITSHORT() );
    oHeader.addValue( CADHeader::PDMODE, buffer.ReadBITSHORT() );

    if( eOptions == OpenOptions::READ_ALL )
    {
        oHeader.addValue( CADHeader::USERI1, buffer.ReadBITSHORT() );    // 1
        oHeader.addValue( CADHeader::USERI2, buffer.ReadBITSHORT() );    // 2
        oHeader.addValue( CADHeader::USERI3, buffer.ReadBITSHORT() );    // 3
        oHeader.addValue( CADHeader::USERI4, buffer.ReadBITSHORT() );    // 4
        oHeader.addValue( CADHeader::USERI5, buffer.ReadBITSHORT() );    // 5
        oHeader.addValue( CADHeader::SPLINESEGS, buffer.ReadBITSHORT() );// 6
        oHeader.addValue( CADHeader::SURFU, buffer.ReadBITSHORT() );     // 7
        oHeader.addValue( CADHeader::SURFV, buffer.ReadBITSHORT() );     // 8
        oHeader.addValue( CADHeader::SURFTYPE, buffer.ReadBITSHORT() );  // 9
        oHeader.addValue( CADHeader::SURFTAB1, buffer.ReadBITSHORT() );  // 10
        oHeader.addValue( CADHeader::SURFTAB2, buffer.ReadBITSHORT() );  // 11
        oHeader.addValue( CADHeader::SPLINETYPE, buffer.ReadBITSHORT() );// 12
        oHeader.addValue( CADHeader::SHADEDGE, buffer.ReadBITSHORT() );  // 13
        oHeader.addValue( CADHeader::SHADEDIF, buffer.ReadBITSHORT() );  // 14
        oHeader.addValue( CADHeader::UNITMODE, buffer.ReadBITSHORT() );  // 15
        oHeader.addValue( CADHeader::MAXACTVP, buffer.ReadBITSHORT() );  // 16
        oHeader.addValue( CADHeader::ISOLINES, buffer.ReadBITSHORT() );  // 17
        oHeader.addValue( CADHeader::CMLJUST, buffer.ReadBITSHORT() );   // 18
        oHeader.addValue( CADHeader::TEXTQLTY, buffer.ReadBITSHORT() );  // 19
    }
    else
    {
        for( char i = 0; i < 19; ++i )
            buffer.SkipBITSHORT();
    }

    oHeader.addValue( CADHeader::LTSCALE, buffer.ReadBITDOUBLE() );
    oHeader.addValue( CADHeader::TEXTSIZE, buffer.ReadBITDOUBLE() );
    oHeader.addValue( CADHeader::TRACEWID, buffer.ReadBITDOUBLE() );
    oHeader.addValue( CADHeader::SKETCHINC, buffer.ReadBITDOUBLE() );
    oHeader.addValue( CADHeader::FILLETRAD, buffer.ReadBITDOUBLE() );
    oHeader.addValue( CADHeader::THICKNESS, buffer.ReadBITDOUBLE() );
    oHeader.addValue( CADHeader::ANGBASE, buffer.ReadBITDOUBLE() );
    oHeader.addValue( CADHeader::PDSIZE, buffer.ReadBITDOUBLE() );
    oHeader.addValue( CADHeader::PLINEWID, buffer.ReadBITDOUBLE() );

    if( eOptions == OpenOptions::READ_ALL )
    {
        oHeader.addValue( CADHeader::USERR1, buffer.ReadBITDOUBLE() );   // 1
        oHeader.addValue( CADHeader::USERR2, buffer.ReadBITDOUBLE() );   // 2
        oHeader.addValue( CADHeader::USERR3, buffer.ReadBITDOUBLE() );   // 3
        oHeader.addValue( CADHeader::USERR4, buffer.ReadBITDOUBLE() );   // 4
        oHeader.addValue( CADHeader::USERR5, buffer.ReadBITDOUBLE() );   // 5
        oHeader.addValue( CADHeader::CHAMFERA, buffer.ReadBITDOUBLE() ); // 6
        oHeader.addValue( CADHeader::CHAMFERB, buffer.ReadBITDOUBLE() ); // 7
        oHeader.addValue( CADHeader::CHAMFERC, buffer.ReadBITDOUBLE() ); // 8
        oHeader.addValue( CADHeader::CHAMFERD, buffer.ReadBITDOUBLE() ); // 9
        oHeader.addValue( CADHeader::FACETRES, buffer.ReadBITDOUBLE() ); // 10
        oHeader.addValue( CADHeader::CMLSCALE, buffer.ReadBITDOUBLE() ); // 11
        oHeader.addValue( CADHeader::CELTSCALE, buffer.ReadBITDOUBLE() );// 12

        oHeader.addValue( CADHeader::MENU, buffer.ReadTV() );
    } else
    {
        for( char i = 0; i < 12; ++i )
            buffer.SkipBITDOUBLE();
        buffer.SkipTV();
    }

    long juliandate, millisec;
    juliandate = buffer.ReadBITLONG();
    millisec   = buffer.ReadBITLONG();
    oHeader.addValue( CADHeader::TDCREATE, juliandate, millisec );
    juliandate = buffer.ReadBITLONG();
    millisec   = buffer.ReadBITLONG();
    oHeader.addValue( CADHeader::TDUPDATE, juliandate, millisec );
    juliandate = buffer.ReadBITLONG();
    millisec   = buffer.ReadBITLONG();
    oHeader.addValue( CADHeader::TDINDWG, juliandate, millisec );
    juliandate = buffer.ReadBITLONG();
    millisec   = buffer.ReadBITLONG();
    oHeader.addValue( CADHeader::TDUSRTIMER, juliandate, millisec );

    oHeader.addValue( CADHeader::CECOLOR, buffer.ReadBITSHORT() );

    oHeader.addValue( CADHeader::HANDSEED, buffer.ReadHANDLE() );

    oHeader.addValue( CADHeader::CLAYER, buffer.ReadHANDLE() );
    oHeader.addValue( CADHeader::TEXTSTYLE, buffer.ReadHANDLE() );
    oHeader.addValue( CADHeader::CELTYPE, buffer.ReadHANDLE() );
    oHeader.addValue( CADHeader::DIMSTYLE, buffer.ReadHANDLE() );
    oHeader.addValue( CADHeader::CMLSTYLE, buffer.ReadHANDLE() );

    oHeader.addValue( CADHeader::PSVPSCALE, buffer.ReadBITDOUBLE() );
    double dX, dY, dZ;
    dX = buffer.ReadBITDOUBLE();
    dY = buffer.ReadBITDOUBLE();
    dZ = buffer.ReadBITDOUBLE();
    oHeader.addValue( CADHeader::PINSBASE, dX, dY, dZ );

    dX = buffer.ReadBITDOUBLE();
    dY = buffer.ReadBITDOUBLE();
    dZ = buffer.ReadBITDOUBLE();
    oHeader.addValue( CADHeader::PEXTMIN, dX, dY, dZ );
    dX = buffer.ReadBITDOUBLE();
    dY = buffer.ReadBITDOUBLE();
    dZ = buffer.ReadBITDOUBLE();
    oHeader.addValue( CADHeader::PEXTMAX, dX, dY, dZ );
    dX = buffer.ReadRAWDOUBLE();
    dY = buffer.ReadRAWDOUBLE();
    oHeader.addValue( CADHeader::PLIMMIN, dX, dY );
    dX = buffer.ReadRAWDOUBLE();
    dY = buffer.ReadRAWDOUBLE();
    oHeader.addValue( CADHeader::PLIMMAX, dX, dY );

    oHeader.addValue( CADHeader::PELEVATION, buffer.ReadBITDOUBLE() );

    dX = buffer.ReadBITDOUBLE();
    dY = buffer.ReadBITDOUBLE();
    dZ = buffer.ReadBITDOUBLE();
    oHeader.addValue( CADHeader::PUCSORG, dX, dY, dZ );
    dX = buffer.ReadBITDOUBLE();
    dY = buffer.ReadBITDOUBLE();
    dZ = buffer.ReadBITDOUBLE();
    oHeader.addValue( CADHeader::PUCSXDIR, dX, dY, dZ );
    dX = buffer.ReadBITDOUBLE();
    dY = buffer.ReadBITDOUBLE();
    dZ = buffer.ReadBITDOUBLE();
    oHeader.addValue( CADHeader::PUCSYDIR, dX, dY, dZ );

    oHeader.addValue( CADHeader::PUCSNAME, buffer.ReadHANDLE() );
    oHeader.addValue( CADHeader::PUCSORTHOREF, buffer.ReadHANDLE() );

    oHeader.addValue( CADHeader::PUCSORTHOVIEW, buffer.ReadBITSHORT() );
    oHeader.addValue( CADHeader::PUCSBASE, buffer.ReadHANDLE() );

    dX = buffer.ReadBITDOUBLE();
    dY = buffer.ReadBITDOUBLE();
    dZ = buffer.ReadBITDOUBLE();
    oHeader.addValue( CADHeader::PUCSORGTOP, dX, dY, dZ );
    dX = buffer.ReadBITDOUBLE();
    dY = buffer.ReadBITDOUBLE();
    dZ = buffer.ReadBITDOUBLE();
    oHeader.addValue( CADHeader::PUCSORGBOTTOM, dX, dY, dZ );
    dX = buffer.ReadBITDOUBLE();
    dY = buffer.ReadBITDOUBLE();
    dZ = buffer.ReadBITDOUBLE();
    oHeader.addValue( CADHeader::PUCSORGLEFT, dX, dY, dZ );
    dX = buffer.ReadBITDOUBLE();
    dY = buffer.ReadBITDOUBLE();
    dZ = buffer.ReadBITDOUBLE();
    oHeader.addValue( CADHeader::PUCSORGRIGHT, dX, dY, dZ );
    dX = buffer.ReadBITDOUBLE();
    dY = buffer.ReadBITDOUBLE();
    dZ = buffer.ReadBITDOUBLE();
    oHeader.addValue( CADHeader::PUCSORGFRONT, dX, dY, dZ );
    dX = buffer.ReadBITDOUBLE();
    dY = buffer.ReadBITDOUBLE();
    dZ = buffer.ReadBITDOUBLE();
    oHeader.addValue( CADHeader::PUCSORGBACK, dX, dY, dZ );

    dX = buffer.ReadBITDOUBLE();
    dY = buffer.ReadBITDOUBLE();
    dZ = buffer.ReadBITDOUBLE();
    oHeader.addValue( CADHeader::INSBASE, dX, dY, dZ );
    dX = buffer.ReadBITDOUBLE();
    dY = buffer.ReadBITDOUBLE();
    dZ = buffer.ReadBITDOUBLE();
    oHeader.addValue( CADHeader::EXTMIN, dX, dY, dZ );
    dX = buffer.ReadBITDOUBLE();
    dY = buffer.ReadBITDOUBLE();
    dZ = buffer.ReadBITDOUBLE();
    oHeader.addValue( CADHeader::EXTMAX, dX, dY, dZ );
    dX = buffer.ReadRAWDOUBLE();
    dY = buffer.ReadRAWDOUBLE();
    oHeader.addValue( CADHeader::LIMMIN, dX, dY );
    dX = buffer.ReadRAWDOUBLE();
    dY = buffer.ReadRAWDOUBLE();
    oHeader.addValue( CADHeader::LIMMAX, dX, dY );

    oHeader.addValue( CADHeader::ELEVATION, buffer.ReadBITDOUBLE() );
    dX = buffer.ReadBITDOUBLE();
    dY = buffer.ReadBITDOUBLE();
    dZ = buffer.ReadBITDOUBLE();
    oHeader.addValue( CADHeader::UCSORG, dX, dY, dZ );
    dX = buffer.ReadBITDOUBLE();
    dY = buffer.ReadBITDOUBLE();
    dZ = buffer.ReadBITDOUBLE();
    oHeader.addValue( CADHeader::UCSXDIR, dX, dY, dZ );
    dX = buffer.ReadBITDOUBLE();
    dY = buffer.ReadBITDOUBLE();
    dZ = buffer.ReadBITDOUBLE();
    oHeader.addValue( CADHeader::UCSYDIR, dX, dY, dZ );

    oHeader.addValue( CADHeader::UCSNAME, buffer.ReadHANDLE() );
    oHeader.addValue( CADHeader::UCSORTHOREF, buffer.ReadHANDLE() );

    oHeader.addValue( CADHeader::UCSORTHOVIEW, buffer.ReadBITSHORT() );

    oHeader.addValue( CADHeader::UCSBASE, buffer.ReadHANDLE() );

    dX = buffer.ReadBITDOUBLE();
    dY = buffer.ReadBITDOUBLE();
    dZ = buffer.ReadBITDOUBLE();
    oHeader.addValue( CADHeader::UCSORGTOP, dX, dY, dZ );
    dX = buffer.ReadBITDOUBLE();
    dY = buffer.ReadBITDOUBLE();
    dZ = buffer.ReadBITDOUBLE();
    oHeader.addValue( CADHeader::UCSORGBOTTOM, dX, dY, dZ );
    dX = buffer.ReadBITDOUBLE();
    dY = buffer.ReadBITDOUBLE();
    dZ = buffer.ReadBITDOUBLE();
    oHeader.addValue( CADHeader::UCSORGLEFT, dX, dY, dZ );
    dX = buffer.ReadBITDOUBLE();
    dY = buffer.ReadBITDOUBLE();
    dZ = buffer.ReadBITDOUBLE();
    oHeader.addValue( CADHeader::UCSORGRIGHT, dX, dY, dZ );
    dX = buffer.ReadBITDOUBLE();
    dY = buffer.ReadBITDOUBLE();
    dZ = buffer.ReadBITDOUBLE();
    oHeader.addValue( CADHeader::UCSORGFRONT, dX, dY, dZ );
    dX = buffer.ReadBITDOUBLE();
    dY = buffer.ReadBITDOUBLE();
    dZ = buffer.ReadBITDOUBLE();
    oHeader.addValue( CADHeader::UCSORGBACK, dX, dY, dZ );

    if( eOptions == OpenOptions::READ_ALL )
    {
        oHeader.addValue( CADHeader::DIMPOST, buffer.ReadTV() );
        oHeader.addValue( CADHeader::DIMAPOST, buffer.ReadTV() );

        oHeader.addValue( CADHeader::DIMSCALE, buffer.ReadBITDOUBLE() ); // 1
        oHeader.addValue( CADHeader::DIMASZ, buffer.ReadBITDOUBLE() );   // 2
        oHeader.addValue( CADHeader::DIMEXO, buffer.ReadBITDOUBLE() );   // 3
        oHeader.addValue( CADHeader::DIMDLI, buffer.ReadBITDOUBLE() );   // 4
        oHeader.addValue( CADHeader::DIMEXE, buffer.ReadBITDOUBLE() );   // 5
        oHeader.addValue( CADHeader::DIMRND, buffer.ReadBITDOUBLE() );   // 6
        oHeader.addValue( CADHeader::DIMDLE, buffer.ReadBITDOUBLE() );   // 7
        oHeader.addValue( CADHeader::DIMTP, buffer.ReadBITDOUBLE() );    // 8
        oHeader.addValue( CADHeader::DIMTM, buffer.ReadBITDOUBLE() );    // 9

        oHeader.addValue( CADHeader::DIMTOL, buffer.ReadBIT() );
        oHeader.addValue( CADHeader::DIMLIM, buffer.ReadBIT() );
        oHeader.addValue( CADHeader::DIMTIH, buffer.ReadBIT() );
        oHeader.addValue( CADHeader::DIMTOH, buffer.ReadBIT() );
        oHeader.addValue( CADHeader::DIMSE1, buffer.ReadBIT() );
        oHeader.addValue( CADHeader::DIMSE2, buffer.ReadBIT() );

        oHeader.addValue( CADHeader::DIMTAD, buffer.ReadBITSHORT() );
        oHeader.addValue( CADHeader::DIMZIN, buffer.ReadBITSHORT() );
        oHeader.addValue( CADHeader::DIMAZIN, buffer.ReadBITSHORT() );

        oHeader.addValue( CADHeader::DIMTXT, buffer.ReadBITDOUBLE() );   // 1
        oHeader.addValue( CADHeader::DIMCEN, buffer.ReadBITDOUBLE() );   // 2
        oHeader.addValue( CADHeader::DIMTSZ, buffer.ReadBITDOUBLE() );   // 3
        oHeader.addValue( CADHeader::DIMALTF, buffer.ReadBITDOUBLE() );  // 4
        oHeader.addValue( CADHeader::DIMLFAC, buffer.ReadBITDOUBLE() );  // 5
        oHeader.addValue( CADHeader::DIMTVP, buffer.ReadBITDOUBLE() );   // 6
        oHeader.addValue( CADHeader::DIMTFAC, buffer.ReadBITDOUBLE() );  // 7
        oHeader.addValue( CADHeader::DIMGAP, buffer.ReadBITDOUBLE() );   // 8
        oHeader.addValue( CADHeader::DIMALTRND, buffer.ReadBITDOUBLE() );// 9

        oHeader.addValue( CADHeader::DIMALT, buffer.ReadBIT() );

        oHeader.addValue( CADHeader::DIMALTD, buffer.ReadBITSHORT() );

        oHeader.addValue( CADHeader::DIMTOFL, buffer.ReadBIT() );
        oHeader.addValue( CADHeader::DIMSAH, buffer.ReadBIT() );
        oHeader.addValue( CADHeader::DIMTIX, buffer.ReadBIT() );
        oHeader.addValue( CADHeader::DIMSOXD, buffer.ReadBIT() );

        oHeader.addValue( CADHeader::DIMCLRD, buffer.ReadBITSHORT() );   // 1
        oHeader.addValue( CADHeader::DIMCLRE, buffer.ReadBITSHORT() );   // 2
        oHeader.addValue( CADHeader::DIMCLRT, buffer.ReadBITSHORT() );   // 3
        oHeader.addValue( CADHeader::DIMADEC, buffer.ReadBITSHORT() );   // 4
        oHeader.addValue( CADHeader::DIMDEC, buffer.ReadBITSHORT() );    // 5
        oHeader.addValue( CADHeader::DIMTDEC, buffer.ReadBITSHORT() );   // 6
        oHeader.addValue( CADHeader::DIMALTU, buffer.ReadBITSHORT() );   // 7
        oHeader.addValue( CADHeader::DIMALTTD, buffer.ReadBITSHORT() );  // 8
        oHeader.addValue( CADHeader::DIMAUNIT, buffer.ReadBITSHORT() );  // 9
        oHeader.addValue( CADHeader::DIMFRAC, buffer.ReadBITSHORT() );   // 10
        oHeader.addValue( CADHeader::DIMLUNIT, buffer.ReadBITSHORT() );  // 11
        oHeader.addValue( CADHeader::DIMDSEP, buffer.ReadBITSHORT() );   // 12
        oHeader.addValue( CADHeader::DIMTMOVE, buffer.ReadBITSHORT() );  // 13
        oHeader.addValue( CADHeader::DIMJUST, buffer.ReadBITSHORT() );   // 14

        oHeader.addValue( CADHeader::DIMSD1, buffer.ReadBIT() );
        oHeader.addValue( CADHeader::DIMSD2, buffer.ReadBIT() );

        oHeader.addValue( CADHeader::DIMTOLJ, buffer.ReadBITSHORT() );
        oHeader.addValue( CADHeader::DIMTZIN, buffer.ReadBITSHORT() );
        oHeader.addValue( CADHeader::DIMALTZ, buffer.ReadBITSHORT() );
        oHeader.addValue( CADHeader::DIMALTTZ, buffer.ReadBITSHORT() );

        oHeader.addValue( CADHeader::DIMUPT, buffer.ReadBIT() );

        oHeader.addValue( CADHeader::DIMATFIT, buffer.ReadBITSHORT() );

        oHeader.addValue( CADHeader::DIMTXSTY, buffer.ReadHANDLE() );
        oHeader.addValue( CADHeader::DIMLDRBLK, buffer.ReadHANDLE() );
        oHeader.addValue( CADHeader::DIMBLK, buffer.ReadHANDLE() );
        oHeader.addValue( CADHeader::DIMBLK1, buffer.ReadHANDLE() );
        oHeader.addValue( CADHeader::DIMBLK2, buffer.ReadHANDLE() );

        oHeader.addValue( CADHeader::DIMLWD, buffer.ReadBITSHORT() );
        oHeader.addValue( CADHeader::DIMLWE, buffer.ReadBITSHORT() );
    } else
    {
        buffer.SkipTV();
        buffer.SkipTV();

        for( char i = 0; i < 9; ++i )
            buffer.SkipBITDOUBLE();

        buffer.Seek(6);

        for( char i = 0; i < 3; ++i )
            buffer.SkipBITSHORT();

        for( char i = 0; i < 9; ++i )
            buffer.SkipBITDOUBLE();

        buffer.Seek(1);

        buffer.SkipBITSHORT();

        buffer.Seek(4);

        for( char i = 0; i < 14; ++i )
            buffer.SkipBITSHORT();

        buffer.Seek(2);

        for( char i = 0; i < 4; ++i )
            buffer.SkipBITSHORT();

        buffer.Seek(1);
        buffer.SkipBITSHORT();

        for( char i = 0; i < 5; ++i )
            buffer.SkipHANDLE();

        buffer.SkipBITSHORT();
        buffer.SkipBITSHORT();
    }

    CADHandle stBlocksTable = buffer.ReadHANDLE();
    oTables.AddTable( CADTables::BlocksTable, stBlocksTable );

    CADHandle stLayersTable = buffer.ReadHANDLE();
    oTables.AddTable( CADTables::LayersTable, stLayersTable );

    CADHandle stStyleTable = buffer.ReadHANDLE();
    oTables.AddTable( CADTables::StyleTable, stStyleTable );

    CADHandle stLineTypesTable = buffer.ReadHANDLE();
    oTables.AddTable( CADTables::LineTypesTable, stLineTypesTable );

    CADHandle stViewTable = buffer.ReadHANDLE();
    oTables.AddTable( CADTables::ViewTable, stViewTable );

    CADHandle stUCSTable = buffer.ReadHANDLE();
    oTables.AddTable( CADTables::UCSTable, stUCSTable );

    CADHandle stViewportTable = buffer.ReadHANDLE();
    oTables.AddTable( CADTables::ViewportTable, stViewportTable );

    CADHandle stAPPIDTable = buffer.ReadHANDLE();
    oTables.AddTable( CADTables::APPIDTable, stAPPIDTable );

    if( eOptions == OpenOptions::READ_ALL )
    {
        oHeader.addValue( CADHeader::DIMSTYLE, buffer.ReadHANDLE() );
    }
    else
    {
        buffer.SkipHANDLE();
    }

    CADHandle stEntityTable = buffer.ReadHANDLE();
    oTables.AddTable( CADTables::EntityTable, stEntityTable );

    CADHandle stACADGroupDict = buffer.ReadHANDLE();
    oTables.AddTable( CADTables::ACADGroupDict, stACADGroupDict );

    CADHandle stACADMLineStyleDict = buffer.ReadHANDLE();
    oTables.AddTable( CADTables::ACADMLineStyleDict, stACADMLineStyleDict );

    CADHandle stNamedObjectsDict = buffer.ReadHANDLE();
    oTables.AddTable( CADTables::NamedObjectsDict, stNamedObjectsDict );

    if( eOptions == OpenOptions::READ_ALL )
    {
        oHeader.addValue( CADHeader::TSTACKALIGN, buffer.ReadBITSHORT() );
        oHeader.addValue( CADHeader::TSTACKSIZE,  buffer.ReadBITSHORT() );
    } else
    {
        buffer.SkipBITSHORT();
        buffer.SkipBITSHORT();
    }

    oHeader.addValue( CADHeader::HYPERLINKBASE, buffer.ReadTV() );
    oHeader.addValue( CADHeader::STYLESHEET, buffer.ReadTV() );

    CADHandle stLayoutsDict = buffer.ReadHANDLE();
    oTables.AddTable( CADTables::LayoutsDict, stLayoutsDict );

    CADHandle stPlotSettingsDict = buffer.ReadHANDLE();
    oTables.AddTable( CADTables::PlotSettingsDict, stPlotSettingsDict );

    CADHandle stPlotStylesDict = buffer.ReadHANDLE();
    oTables.AddTable( CADTables::PlotStylesDict, stPlotStylesDict );

    if( eOptions == OpenOptions::READ_ALL )
    {
        int Flags = buffer.ReadBITLONG();
        oHeader.addValue( CADHeader::CELWEIGHT, Flags & 0x001F );
        oHeader.addValue( CADHeader::ENDCAPS, ( Flags & 0x0060 ) != 0 );
        oHeader.addValue( CADHeader::JOINSTYLE, (Flags & 0x0180) != 0);
        oHeader.addValue( CADHeader::LWDISPLAY, ( Flags & 0x0200 ) == 0);
        oHeader.addValue( CADHeader::XEDIT, ( Flags & 0x0400 ) == 0);
        oHeader.addValue( CADHeader::EXTNAMES, ( Flags & 0x0800 ) != 0 );
        oHeader.addValue( CADHeader::PSTYLEMODE, ( Flags & 0x2000 ) != 0 );
        oHeader.addValue( CADHeader::OLESTARTUP, ( Flags & 0x4000 ) != 0);
    }
    else
    {
        buffer.SkipBITLONG();
    }

    oHeader.addValue( CADHeader::INSUNITS, buffer.ReadBITSHORT() );
    short nCEPSNTYPE = buffer.ReadBITSHORT();
    oHeader.addValue( CADHeader::CEPSNTYPE, nCEPSNTYPE );

    if( nCEPSNTYPE == 3 )
        oHeader.addValue( CADHeader::CEPSNID, buffer.ReadHANDLE() );

    oHeader.addValue( CADHeader::FINGERPRINTGUID, buffer.ReadTV() );
    oHeader.addValue( CADHeader::VERSIONGUID, buffer.ReadTV() );

    CADHandle stBlockRecordPaperSpace = buffer.ReadHANDLE();
    oTables.AddTable( CADTables::BlockRecordPaperSpace, stBlockRecordPaperSpace );
    // TODO: is this part of the header?
    CADHandle stBlockRecordModelSpace = buffer.ReadHANDLE();
    oTables.AddTable( CADTables::BlockRecordModelSpace, stBlockRecordModelSpace );

    if( eOptions == OpenOptions::READ_ALL )
    {
        // Is this part of the header?

        /*CADHandle LTYPE_BYLAYER = */buffer.ReadHANDLE();
        /*CADHandle LTYPE_BYBLOCK = */buffer.ReadHANDLE();
        /*CADHandle LTYPE_CONTINUOUS = */buffer.ReadHANDLE();

        oHeader.addValue( UNKNOWN11, buffer.ReadBITSHORT() );
        oHeader.addValue( UNKNOWN12, buffer.ReadBITSHORT() );
        oHeader.addValue( UNKNOWN13, buffer.ReadBITSHORT() );
        oHeader.addValue( UNKNOWN14, buffer.ReadBITSHORT() );
    } else
    {
        buffer.SkipHANDLE();
        buffer.SkipHANDLE();
        buffer.SkipHANDLE();
        buffer.SkipBITSHORT();
        buffer.SkipBITSHORT();
        buffer.SkipBITSHORT();
        buffer.SkipBITSHORT();
    }

    int returnCode = CADErrorCodes::SUCCESS;
    unsigned short dSectionCRC = validateEntityCRC( buffer,
        static_cast<unsigned int>(dHeaderVarsSectionLength + dSizeOfSectionSize), "HEADERVARS" );
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    (void)dSectionCRC;
#else
    if(dSectionCRC == 0)
    {
        std::cerr << "File is corrupted (HEADERVARS section CRC doesn't match.)\n";
        return CADErrorCodes::HEADER_SECTION_READ_FAILED;
    }
#endif
    pFileIO->Read( bufferPre, DWGConstants::SentinelLength );
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    if( memcmp( bufferPre, DWGConstants::HeaderVariablesEnd,
                         DWGConstants::SentinelLength ) )
    {
        std::cerr << "File is corrupted (HEADERVARS section ending sentinel "
                          "doesn't match.)\n";
        returnCode = CADErrorCodes::HEADER_SECTION_READ_FAILED;
    }
#endif
    return returnCode;
}

int DWGFileR2000::ReadClasses( enum OpenOptions eOptions )
{
    if( eOptions == OpenOptions::READ_ALL || eOptions == OpenOptions::READ_FAST )
    {
        char   bufferPre[255];
        size_t dSectionSize = 0;
        const size_t dSizeOfSectionSize = 4;

        pFileIO->Seek( sectionLocatorRecords[1].dSeeker, CADFileIO::SeekOrigin::BEG );

        pFileIO->Read( bufferPre, DWGConstants::SentinelLength );
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
        if( memcmp( bufferPre, DWGConstants::DSClassesStart,
                               DWGConstants::SentinelLength ) )
        {
            std::cerr << "File is corrupted (wrong pointer to CLASSES section,"
                    "or CLASSES starting sentinel corrupted.)\n";

            return CADErrorCodes::CLASSES_SECTION_READ_FAILED;
        }
#endif

        pFileIO->Read( &dSectionSize, dSizeOfSectionSize );
        DebugMsg("Classes section length: %d\n",
                  static_cast<int>(dSectionSize) );
        if(dSectionSize > 65535) {
            DebugMsg("File is corrupted (CLASSES section is too large: %d\n",
                     static_cast<int>(dSectionSize));

            return CADErrorCodes::CLASSES_SECTION_READ_FAILED;
        }

        CADBuffer buffer(dSectionSize + dSizeOfSectionSize + 10);
        buffer.WriteRAW(&dSectionSize, dSizeOfSectionSize);
        size_t readSize = pFileIO->Read( buffer.GetRawBuffer(), dSectionSize + 2 );
        if(readSize != dSectionSize + 2)
        {
            DebugMsg( "Failed to read %d byte of file. Read only %d",
                      static_cast<int>(dSectionSize + 2),
                      static_cast<int>(readSize) );
            return CADErrorCodes::CLASSES_SECTION_READ_FAILED;
        }

        size_t dSectionBitSize = (dSectionSize + dSizeOfSectionSize) * 8;
        while( buffer.PositionBit() < dSectionBitSize - 8)
        {
            CADClass stClass;
            stClass.dClassNum        = buffer.ReadBITSHORT();
            stClass.dProxyCapFlag    = buffer.ReadBITSHORT();
            stClass.sApplicationName = buffer.ReadTV();
            stClass.sCppClassName    = buffer.ReadTV();
            stClass.sDXFRecordName   = buffer.ReadTV();
            stClass.bWasZombie       = buffer.ReadBIT();
            stClass.bIsEntity        = buffer.ReadBITSHORT() == 0x1F2;

            oClasses.addClass( stClass );
        }

        buffer.Seek(dSectionBitSize, CADBuffer::BEG);
        unsigned short dSectionCRC = validateEntityCRC( buffer,
                    static_cast<unsigned int>(dSectionSize + dSizeOfSectionSize),
                                                        "CLASSES" );
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
        (void)dSectionCRC;
#else
        if(dSectionCRC == 0)
        {
            std::cerr << "File is corrupted (CLASSES section CRC doesn't match.)\n";
            return CADErrorCodes::CLASSES_SECTION_READ_FAILED;
        }
#endif

        pFileIO->Read( bufferPre, DWGConstants::SentinelLength );
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
        if( memcmp( bufferPre, DWGConstants::DSClassesEnd,
                               DWGConstants::SentinelLength ) )
        {
            std::cerr << "File is corrupted (CLASSES section ending sentinel "
                    "doesn't match.)\n";
            return CADErrorCodes::CLASSES_SECTION_READ_FAILED;
        }
#endif
    }
    return CADErrorCodes::SUCCESS;
}

int DWGFileR2000::CreateFileMap()
{
    size_t nSection = 0;
    const size_t dSizeOfSectionSize = 2;

    typedef pair<long, long> ObjHandleOffset;
    ObjHandleOffset          previousObjHandleOffset;
    ObjHandleOffset          tmpOffset;

    mapObjects.clear();

    // Seek to the beginning of the objects map
    pFileIO->Seek( sectionLocatorRecords[2].dSeeker, CADFileIO::SeekOrigin::BEG );

    while( true )
    {
        unsigned short dSectionSize = 0;

        // Read section size

        pFileIO->Read( &dSectionSize, dSizeOfSectionSize );
        unsigned short dSectionSizeOriginal = dSectionSize;
        SwapEndianness( dSectionSize, sizeof( dSectionSize ) );

        DebugMsg( "Object map section #%d size: %d\n",
                  static_cast<int>(++nSection), dSectionSize );

        if( dSectionSize <= dSizeOfSectionSize )
            break; // Last section is empty.

        CADBuffer buffer(dSectionSize + dSizeOfSectionSize + 10);
        buffer.WriteRAW(&dSectionSizeOriginal, dSizeOfSectionSize);
        size_t nRecordsInSection   = 0;

        // Read section datsa
        size_t readSize = pFileIO->Read( buffer.GetRawBuffer(), dSectionSize );
        if(readSize != dSectionSize)
        {
            DebugMsg( "Failed to read %d byte of file. Read only %d",
                      static_cast<int>(dSectionSize),
                      static_cast<int>(readSize) );
            return CADErrorCodes::OBJECTS_SECTION_READ_FAILED;
        }
        unsigned int dSectionBitSize = dSectionSize * 8;

        while( buffer.PositionBit() < dSectionBitSize )
        {
            tmpOffset.first  = buffer.ReadUMCHAR(); // 8 + 8*8
            tmpOffset.second = buffer.ReadMCHAR(); // 8 + 8*8

            if( 0 == nRecordsInSection )
            {
                previousObjHandleOffset = tmpOffset;
            }
            else
            {
                if( (tmpOffset.first >= 0 &&
                     std::numeric_limits<long>::max() - tmpOffset.first > previousObjHandleOffset.first) ||
                    (tmpOffset.first < 0 &&
                     std::numeric_limits<long>::min() - tmpOffset.first <= previousObjHandleOffset.first) )
                {
                    previousObjHandleOffset.first += tmpOffset.first;
                }
                if( (tmpOffset.second >= 0 &&
                     std::numeric_limits<long>::max() - tmpOffset.second > previousObjHandleOffset.second) ||
                    (tmpOffset.second < 0 &&
                     std::numeric_limits<long>::min() - tmpOffset.second <= previousObjHandleOffset.second) )
                {
                    previousObjHandleOffset.second += tmpOffset.second;
                }
            }
#ifdef _DEBUG
            assert( mapObjects.find( previousObjHandleOffset.first ) ==
                                                                mapObjects.end() );
#endif //_DEBUG
            mapObjects.insert( previousObjHandleOffset );
            ++nRecordsInSection;
        }

        unsigned short dSectionCRC = validateEntityCRC( buffer,
                    static_cast<unsigned int>(dSectionSize), "OBJECTMAP", true );
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
        (void)dSectionCRC;
#else
        if(dSectionCRC == 0)
        {
            std::cerr << "File is corrupted (OBJECTMAP section CRC doesn't match.)\n";
            return CADErrorCodes::OBJECTS_SECTION_READ_FAILED;
        }
#endif
    }

    return CADErrorCodes::SUCCESS;
}

CADObject * DWGFileR2000::GetObject( long dHandle, bool bHandlesOnly )
{
    CADBuffer buffer(8);

    pFileIO->Seek( mapObjects[dHandle], CADFileIO::SeekOrigin::BEG );
    pFileIO->Read( buffer.GetRawBuffer(), 8 );
    unsigned int dObjectSize = buffer.ReadMSHORT();

    // FIXME: Limit object size to 64kB
    if( dObjectSize > 65536 )
        return nullptr;

    // And read whole data chunk into memory for future parsing.
    // + nBitOffsetFromStart/8 + 2 is because dObjectSize doesn't cover CRC and itself.
    dObjectSize += static_cast<unsigned int>(buffer.PositionBit() / 8 + 2);

    CADBuffer objectBuffer(dObjectSize + 64);

    pFileIO->Seek( mapObjects[dHandle], CADFileIO::SeekOrigin::BEG );
    size_t readSize = pFileIO->Read( objectBuffer.GetRawBuffer(),
                                     static_cast<size_t>(dObjectSize) );
    if(readSize != static_cast<size_t>(dObjectSize))
    {
        DebugMsg( "Failed to read %d byte of file. Read only %d",
                  static_cast<int>(dObjectSize),
                  static_cast<int>(readSize) );
        return nullptr;
    }

    /* Unused dObjectSize = */ objectBuffer.ReadMSHORT();
    short dObjectType = objectBuffer.ReadBITSHORT();
    if( dObjectType >= 500 )
    {
        CADClass cadClass = oClasses.getClassByNum( dObjectType );
        // FIXME: replace strcmp() with C++ analog
        if( !strcmp( cadClass.sCppClassName.c_str(), "AcDbRasterImage" ) )
        {
            dObjectType = CADObject::IMAGE;
        }
        else if( !strcmp( cadClass.sCppClassName.c_str(), "AcDbRasterImageDef" ) )
        {
            dObjectType = CADObject::IMAGEDEF;
        }
        else if( !strcmp( cadClass.sCppClassName.c_str(), "AcDbRasterImageDefReactor" ) )
        {
            dObjectType = CADObject::IMAGEDEFREACTOR;
        }
        else if( !strcmp( cadClass.sCppClassName.c_str(), "AcDbWipeout" ) )
        {
            dObjectType = CADObject::WIPEOUT;
        }
    }

    // Entities handling
    if( isCommonEntityType( dObjectType ) )
    {
        struct CADCommonED stCommonEntityData; // Common for all entities

        stCommonEntityData.nObjectSizeInBits = objectBuffer.ReadRAWLONG();
        stCommonEntityData.hObjectHandle     = objectBuffer.ReadHANDLE();

        short  dEEDSize;
        CADEed dwgEed;
        while( ( dEEDSize = objectBuffer.ReadBITSHORT() ) != 0 )
        {
            dwgEed.dLength      = dEEDSize;
            dwgEed.hApplication = objectBuffer.ReadHANDLE();

            if(dEEDSize < 0)
            {
                return nullptr;
            }

            for( short i = 0; i < dEEDSize; ++i )
            {
                dwgEed.acData.push_back( objectBuffer.ReadCHAR() );
            }

            stCommonEntityData.aEED.push_back( dwgEed );
        }

        stCommonEntityData.bGraphicsPresented = objectBuffer.ReadBIT();
        if( stCommonEntityData.bGraphicsPresented )
        {
            const auto rawLong = objectBuffer.ReadRAWLONG();
            if( rawLong < 0 )
                return nullptr;
            size_t nGraphicsDataSize = static_cast<size_t>(rawLong);
            if( nGraphicsDataSize > std::numeric_limits<size_t>::max() / 8 )
                return nullptr;
            // Skip read graphics data
            buffer.Seek(nGraphicsDataSize * 8);
        }
        stCommonEntityData.bbEntMode        = objectBuffer.Read2B();
        stCommonEntityData.nNumReactors     = objectBuffer.ReadBITLONG();
        if(stCommonEntityData.nNumReactors < 0 ||
           stCommonEntityData.nNumReactors > 5000)
        {
            return nullptr;
        }
        stCommonEntityData.bNoLinks         = objectBuffer.ReadBIT();
        stCommonEntityData.nCMColor         = objectBuffer.ReadBITSHORT();
        stCommonEntityData.dfLTypeScale     = objectBuffer.ReadBITDOUBLE();
        stCommonEntityData.bbLTypeFlags     = objectBuffer.Read2B();
        stCommonEntityData.bbPlotStyleFlags = objectBuffer.Read2B();
        stCommonEntityData.nInvisibility    = objectBuffer.ReadBITSHORT();
        stCommonEntityData.nLineWeight      = objectBuffer.ReadCHAR();

        // Skip entitity-specific data, we don't need it if bHandlesOnly == true
        if( bHandlesOnly == true )
        {
            return getEntity( dObjectType, dObjectSize, stCommonEntityData, objectBuffer);
        }

        switch( dObjectType )
        {
            case CADObject::BLOCK:
                return getBlock( dObjectSize, stCommonEntityData, objectBuffer);

            case CADObject::ELLIPSE:
                return getEllipse( dObjectSize, stCommonEntityData, objectBuffer);

            case CADObject::MLINE:
                return getMLine( dObjectSize, stCommonEntityData, objectBuffer);

            case CADObject::SOLID:
                return getSolid( dObjectSize, stCommonEntityData, objectBuffer);

            case CADObject::POINT:
                return getPoint( dObjectSize, stCommonEntityData, objectBuffer);

            case CADObject::POLYLINE3D:
                return getPolyLine3D( dObjectSize, stCommonEntityData, objectBuffer);

            case CADObject::RAY:
                return getRay( dObjectSize, stCommonEntityData, objectBuffer);

            case CADObject::XLINE:
                return getXLine( dObjectSize, stCommonEntityData, objectBuffer);

            case CADObject::LINE:
                return getLine( dObjectSize, stCommonEntityData, objectBuffer);

            case CADObject::TEXT:
                return getText( dObjectSize, stCommonEntityData, objectBuffer);

            case CADObject::VERTEX3D:
                return getVertex3D( dObjectSize, stCommonEntityData, objectBuffer);

            case CADObject::CIRCLE:
                return getCircle( dObjectSize, stCommonEntityData, objectBuffer);

            case CADObject::ENDBLK:
                return getEndBlock( dObjectSize, stCommonEntityData, objectBuffer);

            case CADObject::POLYLINE2D:
                return getPolyline2D( dObjectSize, stCommonEntityData, objectBuffer);

            case CADObject::ATTRIB:
                return getAttributes( dObjectSize, stCommonEntityData, objectBuffer);

            case CADObject::ATTDEF:
                return getAttributesDefn( dObjectSize, stCommonEntityData, objectBuffer);

            case CADObject::LWPOLYLINE:
                return getLWPolyLine( dObjectSize, stCommonEntityData, objectBuffer);

            case CADObject::ARC:
                return getArc( dObjectSize, stCommonEntityData, objectBuffer);

            case CADObject::SPLINE:
                return getSpline( dObjectSize, stCommonEntityData, objectBuffer);

            case CADObject::POLYLINE_PFACE:
                return getPolylinePFace( dObjectSize, stCommonEntityData, objectBuffer);

            case CADObject::IMAGE:
                return getImage( dObjectSize, stCommonEntityData, objectBuffer);

            case CADObject::FACE3D:
                return get3DFace( dObjectSize, stCommonEntityData, objectBuffer);

            case CADObject::VERTEX_MESH:
                return getVertexMesh( dObjectSize, stCommonEntityData, objectBuffer);

            case CADObject::VERTEX_PFACE:
                return getVertexPFace( dObjectSize, stCommonEntityData,
                                       objectBuffer);

            case CADObject::MTEXT:
                return getMText( dObjectSize, stCommonEntityData,
                                 objectBuffer);

            case CADObject::DIMENSION_RADIUS:
            case CADObject::DIMENSION_DIAMETER:
            case CADObject::DIMENSION_ALIGNED:
            case CADObject::DIMENSION_ANG_3PT:
            case CADObject::DIMENSION_ANG_2LN:
            case CADObject::DIMENSION_ORDINATE:
            case CADObject::DIMENSION_LINEAR:
                return getDimension( dObjectType, dObjectSize, stCommonEntityData,
                                     objectBuffer);

            case CADObject::INSERT:
                return getInsert( dObjectType, dObjectSize, stCommonEntityData,
                                  objectBuffer);

            default:
                return getEntity( dObjectType, dObjectSize, stCommonEntityData,
                                  objectBuffer);
        }
    }
    else
    {
        switch( dObjectType )
        {
            case CADObject::DICTIONARY:
                return getDictionary( dObjectSize, objectBuffer);

            case CADObject::LAYER:
                return getLayerObject( dObjectSize, objectBuffer);

            case CADObject::LAYER_CONTROL_OBJ:
                return getLayerControl( dObjectSize, objectBuffer);

            case CADObject::BLOCK_CONTROL_OBJ:
                return getBlockControl( dObjectSize, objectBuffer);

            case CADObject::BLOCK_HEADER:
                return getBlockHeader( dObjectSize, objectBuffer);

            case CADObject::LTYPE_CONTROL_OBJ:
                return getLineTypeControl( dObjectSize, objectBuffer);

            case CADObject::LTYPE1:
                return getLineType1( dObjectSize, objectBuffer);

            case CADObject::IMAGEDEF:
                return getImageDef( dObjectSize, objectBuffer);

            case CADObject::IMAGEDEFREACTOR:
                return getImageDefReactor( dObjectSize, objectBuffer);

            case CADObject::XRECORD:
                return getXRecord( dObjectSize, objectBuffer);
        }
    }

    return nullptr;
}

CADGeometry * DWGFileR2000::GetGeometry( size_t iLayerIndex, long dHandle, long dBlockRefHandle )
{
    CADGeometry * poGeometry = nullptr;
    unique_ptr<CADObject> pCADEntityObject( GetObject( dHandle ) );
    CADEntityObject* readObject =
                dynamic_cast<CADEntityObject *>( pCADEntityObject.get() );

    if( !readObject )
    {
        return nullptr;
    }

    switch( readObject->getType() )
    {
        case CADObject::ARC:
        {
            CADArc * arc = new CADArc();
            CADArcObject * cadArc = static_cast<CADArcObject *>(
                    readObject);

            arc->setPosition( cadArc->vertPosition );
            arc->setExtrusion( cadArc->vectExtrusion );
            arc->setRadius( cadArc->dfRadius );
            arc->setThickness( cadArc->dfThickness );
            arc->setStartingAngle( cadArc->dfStartAngle );
            arc->setEndingAngle( cadArc->dfEndAngle );

            poGeometry = arc;
            break;
        }

        case CADObject::POINT:
        {
            CADPoint3D * point = new CADPoint3D();
            CADPointObject * cadPoint = static_cast<CADPointObject *>(
                    readObject);

            point->setPosition( cadPoint->vertPosition );
            point->setExtrusion( cadPoint->vectExtrusion );
            point->setXAxisAng( cadPoint->dfXAxisAng );
            point->setThickness( cadPoint->dfThickness );

            poGeometry = point;
            break;
        }

        case CADObject::POLYLINE3D:
        {
            CADPolyline3D * polyline = new CADPolyline3D();
            CADPolyline3DObject * cadPolyline3D = static_cast<CADPolyline3DObject *>(
                    readObject);

            // TODO: code can be much simplified if CADHandle will be used.
            // to do so, == and ++ operators should be implemented.
            unique_ptr<CADVertex3DObject> vertex;
            long currentVertexH = cadPolyline3D->hVertices[0].getAsLong();
            while( currentVertexH != 0 )
            {
                CADObject *poCADVertexObject = GetObject( currentVertexH );
                vertex.reset( dynamic_cast<CADVertex3DObject *>( poCADVertexObject ) );

                if( !vertex )
                {
                    delete poCADVertexObject;
                    break;
                }

                currentVertexH = vertex->stCed.hObjectHandle.getAsLong();
                polyline->addVertex( vertex->vertPosition );
                if( vertex->stCed.bNoLinks == true )
                {
                    ++currentVertexH;
                }
                else
                {
                    currentVertexH = vertex->stChed.hNextEntity.getAsLong(
                                vertex->stCed.hObjectHandle );
                }

                // Last vertex is reached. Read it and break reading.
                if( currentVertexH == cadPolyline3D->hVertices[1].getAsLong() )
                {
                    CADObject *poCADVertex3DObject = GetObject( currentVertexH );
                    vertex.reset( dynamic_cast<CADVertex3DObject *>(
                                          poCADVertex3DObject) );
                    if( vertex)
                    {
                        polyline->addVertex( vertex->vertPosition );
                    }
                    else
                    {
                        delete poCADVertex3DObject;
                    }
                    break;
                }
            }

            poGeometry = polyline;
            break;
        }

        case CADObject::LWPOLYLINE:
        {
            CADLWPolyline * lwPolyline = new CADLWPolyline();
            CADLWPolylineObject * cadlwPolyline = static_cast<CADLWPolylineObject *>(
                    readObject);

            lwPolyline->setBulges( cadlwPolyline->adfBulges );
            lwPolyline->setClosed( cadlwPolyline->bClosed );
            lwPolyline->setConstWidth( cadlwPolyline->dfConstWidth );
            lwPolyline->setElevation( cadlwPolyline->dfElevation );
            for( const CADVector& vertex : cadlwPolyline->avertVertices )
                lwPolyline->addVertex( vertex );
            lwPolyline->setVectExtrusion( cadlwPolyline->vectExtrusion );
            lwPolyline->setWidths( cadlwPolyline->astWidths );

            poGeometry = lwPolyline;
            break;
        }

        case CADObject::CIRCLE:
        {
            CADCircle * circle = new CADCircle();
            CADCircleObject * cadCircle = static_cast<CADCircleObject *>(
                    readObject);

            circle->setPosition( cadCircle->vertPosition );
            circle->setExtrusion( cadCircle->vectExtrusion );
            circle->setRadius( cadCircle->dfRadius );
            circle->setThickness( cadCircle->dfThickness );

            poGeometry = circle;
            break;
        }

        case CADObject::ATTRIB:
        {
            CADAttrib * attrib = new CADAttrib();
            CADAttribObject * cadAttrib = static_cast<CADAttribObject *>(
                    readObject );

            attrib->setPosition( cadAttrib->vertInsetionPoint );
            attrib->setExtrusion( cadAttrib->vectExtrusion );
            attrib->setRotationAngle( cadAttrib->dfRotationAng );
            attrib->setAlignmentPoint( cadAttrib->vertAlignmentPoint );
            attrib->setElevation( cadAttrib->dfElevation );
            attrib->setHeight( cadAttrib->dfHeight );
            attrib->setObliqueAngle( cadAttrib->dfObliqueAng );
            attrib->setPositionLocked( cadAttrib->bLockPosition );
            attrib->setTag( cadAttrib->sTag );
            attrib->setTextValue( cadAttrib->sTextValue );
            attrib->setThickness( cadAttrib->dfThickness );

            poGeometry = attrib;
            break;
        }

        case CADObject::ATTDEF:
        {
            CADAttdef * attdef = new CADAttdef();
            CADAttdefObject * cadAttrib = static_cast<CADAttdefObject*>(
                    readObject );

            attdef->setPosition( cadAttrib->vertInsetionPoint );
            attdef->setExtrusion( cadAttrib->vectExtrusion );
            attdef->setRotationAngle( cadAttrib->dfRotationAng );
            attdef->setAlignmentPoint( cadAttrib->vertAlignmentPoint );
            attdef->setElevation( cadAttrib->dfElevation );
            attdef->setHeight( cadAttrib->dfHeight );
            attdef->setObliqueAngle( cadAttrib->dfObliqueAng );
            attdef->setPositionLocked( cadAttrib->bLockPosition );
            attdef->setTag( cadAttrib->sTag );
            attdef->setTextValue( cadAttrib->sTextValue );
            attdef->setThickness( cadAttrib->dfThickness );
            attdef->setPrompt( cadAttrib->sPrompt );

            poGeometry = attdef;
            break;
        }

        case CADObject::ELLIPSE:
        {
            CADEllipse * ellipse = new CADEllipse();
            CADEllipseObject * cadEllipse = static_cast<CADEllipseObject *>(
                    readObject);

            ellipse->setPosition( cadEllipse->vertPosition );
            ellipse->setSMAxis( cadEllipse->vectSMAxis );
            ellipse->setAxisRatio( cadEllipse->dfAxisRatio );
            ellipse->setEndingAngle( cadEllipse->dfEndAngle );
            ellipse->setStartingAngle( cadEllipse->dfBegAngle );

            poGeometry = ellipse;
            break;
        }

        case CADObject::LINE:
        {
            CADLineObject * cadLine = static_cast<CADLineObject *>(
                    readObject);

            CADPoint3D ptBeg( cadLine->vertStart, cadLine->dfThickness );
            CADPoint3D ptEnd( cadLine->vertEnd, cadLine->dfThickness );

            CADLine * line = new CADLine( ptBeg, ptEnd );

            poGeometry = line;
            break;
        }

        case CADObject::RAY:
        {
            CADRay * ray = new CADRay();
            CADRayObject * cadRay = static_cast<CADRayObject *>(
                    readObject);

            ray->setVectVector( cadRay->vectVector );
            ray->setPosition( cadRay->vertPosition );

            poGeometry = ray;
            break;
        }

        case CADObject::SPLINE:
        {
            CADSpline * spline = new CADSpline();
            CADSplineObject * cadSpline = static_cast<CADSplineObject *>(
                    readObject);

            spline->setScenario( cadSpline->dScenario );
            spline->setDegree( cadSpline->dDegree );
            if( spline->getScenario() == 2 )
            {
                spline->setFitTolerance( cadSpline->dfFitTol );
            }
            else if( spline->getScenario() == 1 )
            {
                spline->setRational( cadSpline->bRational );
                spline->setClosed( cadSpline->bClosed );
                spline->setWeight( cadSpline->bWeight );
            }
            for( double weight : cadSpline->adfCtrlPointsWeight )
                spline->addControlPointsWeight( weight );

            for( const CADVector& pt : cadSpline->averFitPoints )
                spline->addFitPoint( pt );

            for( const CADVector& pt : cadSpline->avertCtrlPoints )
                spline->addControlPoint( pt );

            poGeometry = spline;
            break;
        }

        case CADObject::TEXT:
        {
            CADText * text = new CADText();
            CADTextObject * cadText = static_cast<CADTextObject *>(
                    readObject);

            text->setPosition( cadText->vertInsetionPoint );
            text->setTextValue( cadText->sTextValue );
            text->setRotationAngle( cadText->dfRotationAng );
            text->setObliqueAngle( cadText->dfObliqueAng );
            text->setThickness( cadText->dfThickness );
            text->setHeight( cadText->dfElevation );

            poGeometry = text;
            break;
        }

        case CADObject::SOLID:
        {
            CADSolid * solid = new CADSolid();
            CADSolidObject * cadSolid = static_cast<CADSolidObject *>(
                    readObject);

            solid->setElevation( cadSolid->dfElevation );
            solid->setThickness( cadSolid->dfThickness );
            for( const CADVector& corner : cadSolid->avertCorners )
                solid->addCorner( corner );
            solid->setExtrusion( cadSolid->vectExtrusion );

            poGeometry = solid;
            break;
        }

        case CADObject::IMAGE:
        {
            CADImageObject * cadImage = static_cast<CADImageObject *>(
                    readObject);

            CADObject *pCADImageDefObject = GetObject( cadImage->hImageDef.getAsLong() );
            unique_ptr<CADImageDefObject> cadImageDef(
                dynamic_cast<CADImageDefObject *>( pCADImageDefObject ) );

            if(cadImageDef)
            {
                CADImage * image = new CADImage();
                image->setClippingBoundaryType( cadImage->dClipBoundaryType );
                image->setFilePath( cadImageDef->sFilePath );
                image->setVertInsertionPoint( cadImage->vertInsertion );
                CADVector imageSize( cadImage->dfSizeX, cadImage->dfSizeY );
                image->setImageSize( imageSize );
                CADVector imageSizeInPx( cadImageDef->dfXImageSizeInPx, cadImageDef->dfYImageSizeInPx );
                image->setImageSizeInPx( imageSizeInPx );
                CADVector pixelSizeInACADUnits( cadImageDef->dfXPixelSize, cadImageDef->dfYPixelSize );
                image->setPixelSizeInACADUnits( pixelSizeInACADUnits );
                image->setResolutionUnits(
                    static_cast<CADImage::ResolutionUnit>( cadImageDef->dResUnits ) );
                bool bTransparency = (cadImage->dDisplayProps & 0x08) != 0;
                image->setOptions( bTransparency,
                                   cadImage->bClipping,
                                   cadImage->dBrightness,
                                   cadImage->dContrast );
                for( const CADVector& clipPt : cadImage->avertClippingPolygonVertices )
                {
                    image->addClippingPoint( clipPt );
                }

                poGeometry = image;
            }
            else
            {
                delete pCADImageDefObject;
            }
            break;
        }

        case CADObject::MLINE:
        {
            CADMLine * mline = new CADMLine();
            CADMLineObject * cadmLine = static_cast<CADMLineObject *>(
                    readObject);

            mline->setScale( cadmLine->dfScale );
            mline->setOpened( cadmLine->dOpenClosed == 1 ? true : false );
            for( const CADMLineVertex& vertex : cadmLine->avertVertices )
                mline->addVertex( vertex.vertPosition );

            poGeometry = mline;
            break;
        }

        case CADObject::MTEXT:
        {
            CADMText * mtext = new CADMText();
            CADMTextObject * cadmText = static_cast<CADMTextObject *>(
                    readObject);

            mtext->setTextValue( cadmText->sTextValue );
            mtext->setXAxisAng( cadmText->vectXAxisDir.getX() ); //TODO: is this needed?

            mtext->setPosition( cadmText->vertInsertionPoint );
            mtext->setExtrusion( cadmText->vectExtrusion );

            mtext->setHeight( cadmText->dfTextHeight );
            mtext->setRectWidth( cadmText->dfRectWidth );
            mtext->setExtents( cadmText->dfExtents );
            mtext->setExtentsWidth( cadmText->dfExtentsWidth );

            poGeometry = mtext;
            break;
        }

        case CADObject::POLYLINE_PFACE:
        {
            CADPolylinePFace * polyline = new CADPolylinePFace();
            CADPolylinePFaceObject * cadpolyPface = static_cast<CADPolylinePFaceObject *>(
                    readObject);

            // TODO: code can be much simplified if CADHandle will be used.
            // to do so, == and ++ operators should be implemented.
            unique_ptr<CADVertexPFaceObject> vertex;
            auto dCurrentEntHandle = cadpolyPface->hVertices[0].getAsLong();
            auto dLastEntHandle = cadpolyPface->hVertices[1].getAsLong();
            while( true )
            {
                CADObject *pCADVertexPFaceObject = GetObject( dCurrentEntHandle );
                vertex.reset( dynamic_cast<CADVertexPFaceObject *>(
                                      pCADVertexPFaceObject ) );
                /* TODO: this check is excessive, but if something goes wrong way -
             * some part of geometries will be parsed. */
                if( !vertex )
                {
                    delete pCADVertexPFaceObject;
                    break;
                }

                polyline->addVertex( vertex->vertPosition );

                /* FIXME: somehow one more vertex which isnot presented is read.
             * so, checking the number of added vertices */
                /*TODO: is this needed - check on real data
            if ( polyline->hVertices.size() == cadpolyPface->nNumVertices )
            {
                delete( vertex );
                break;
            }*/

                if( vertex->stCed.bNoLinks )
                    ++dCurrentEntHandle;
                else
                    dCurrentEntHandle = vertex->stChed.hNextEntity.getAsLong( vertex->stCed.hObjectHandle );

                if( dCurrentEntHandle == dLastEntHandle )
                {
                    CADObject *pCADVertexPFaceObjectV = GetObject( dCurrentEntHandle );
                    vertex.reset( dynamic_cast<CADVertexPFaceObject *>(
                                          pCADVertexPFaceObjectV) );
                    if(vertex)
                    {
                        polyline->addVertex( vertex->vertPosition );
                    }
                    else
                    {
                        delete pCADVertexPFaceObjectV;
                    }
                    break;
                }
            }

            poGeometry = polyline;
            break;
        }

        case CADObject::XLINE:
        {
            CADXLine * xline = new CADXLine();
            CADXLineObject * cadxLine = static_cast<CADXLineObject *>(
                    readObject);

            xline->setVectVector( cadxLine->vectVector );
            xline->setPosition( cadxLine->vertPosition );

            poGeometry = xline;
            break;
        }

        case CADObject::FACE3D:
        {
            CADFace3D * face = new CADFace3D();
            CAD3DFaceObject * cad3DFace = static_cast<CAD3DFaceObject *>(
                    readObject);

            for( const CADVector& corner : cad3DFace->avertCorners )
                face->addCorner( corner );
            face->setInvisFlags( cad3DFace->dInvisFlags );

            poGeometry = face;
            break;
        }

        case CADObject::POLYLINE_MESH:
        case CADObject::VERTEX_MESH:
        case CADObject::VERTEX_PFACE_FACE:
        default:
            std::cerr << "Asked geometry has unsupported type.\n";
            poGeometry = new CADUnknown();
            break;
    }

    if( poGeometry == nullptr )
        return nullptr;

    // Applying color
    if( readObject->stCed.nCMColor == 256 ) // BYLAYER CASE
    {
        CADLayer& oCurrentLayer = this->GetLayer( iLayerIndex );
        poGeometry->setColor( getCADACIColor( oCurrentLayer.getColor() ) );
    }
    else if( readObject->stCed.nCMColor <= 255 &&
             readObject->stCed.nCMColor >= 0 ) // Excessive check until BYBLOCK case will not be implemented
    {
        poGeometry->setColor( getCADACIColor( readObject->stCed.nCMColor ) );
    }

    // Applying EED
    // Casting object's EED to a vector of strings
    vector<string> asEED;
    for( auto citer = readObject->stCed.aEED.cbegin();
         citer != readObject->stCed.aEED.cend(); ++citer )
    {
        string sEED = "";
        // Detect the type of EED entity
        switch( citer->acData[0] )
        {
            case 0: // String
            {
                if( citer->acData.size() > 1 )
                {
                    unsigned char nStrSize = citer->acData[1];
                    // +2 = skip CodePage, no idea how to use it anyway

                    if(nStrSize > 0)
                    {
                        for( size_t i = 0; i < nStrSize &&
                            i + 4 < citer->acData.size(); ++i )
                        {
                            sEED += citer->acData[i + 4];
                        }
                    }
                }
                break;
            }
            case 1: // Invalid
            {
                DebugMsg( "Error: EED obj type is 1, error in R2000::getGeometry()" );
                break;
            }
            case 2: // { or }
            {
                if( citer->acData.size() > 1 )
                {
                    sEED += citer->acData[1] == 0 ? '{' : '}';
                }
                break;
            }
            case 3: // Layer table ref
            {
                // FIXME: get CADHandle and return getAsLong() result.
                sEED += "Layer table ref (handle):";
                for( size_t i = 0; i < 8 && i + 1 < citer->acData.size(); ++i )
                {
                    sEED += citer->acData[i + 1];
                }
                break;
            }
            case 4: // Binary chunk
            {
                if( citer->acData.size() > 1 )
                {
                    unsigned char nChunkSize = citer->acData[1];
                    sEED += "Binary chunk (chars):";
                    if(nChunkSize > 0)
                    {
                        for( size_t i = 0; i < nChunkSize &&
                            i + 2 < citer->acData.size(); ++i )
                        {
                            sEED += citer->acData[i + 2];
                        }
                    }
                    else
                    {
                        sEED += "?";
                    }
                }
                break;
            }
            case 5: // Entity handle ref
            {
                // FIXME: Get CADHandle and return getAsLong() result.
                sEED += "Entity handle ref (handle):";
                for( size_t i = 0; i < 8 && i + 1 < citer->acData.size(); ++i )
                {
                    sEED += citer->acData[i + 1];
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
                if(citer->acData.size() > 24)
                {
                    memcpy( & dfX, citer->acData.data() + 1, 8 );
                    memcpy( & dfY, citer->acData.data() + 9, 8 );
                    memcpy( & dfZ, citer->acData.data() + 17, 8 );
                }
                sEED += std::to_string( dfX );
                sEED += ';';
                sEED += std::to_string( dfY );
                sEED += ';';
                sEED += std::to_string( dfZ );
                sEED += '}';
                break;
            }
            case 40:
            case 41:
            case 42:
            {
                sEED += "Double:";
                double dfVal = 0;
                if(citer->acData.size() > 8)
                    memcpy( & dfVal, citer->acData.data() + 1, 8 );
                sEED += std::to_string( dfVal );
                break;
            }
            case 70:
            {
                sEED += "Short:";
                int16_t dVal = 0;
                if(citer->acData.size() > 2)
                    memcpy( & dVal, citer->acData.data() + 1, 2 );
                sEED += std::to_string( dVal );
                break;
            }
            case 71:
            {
                sEED += "Long Int:";
                int32_t dVal = 0;
                if(citer->acData.size() > 4)
                    memcpy( & dVal, citer->acData.data() + 1, 4 );
                sEED += std::to_string( dVal );
                break;
            }
            default:
            {
                DebugMsg( "Error in parsing geometry EED: undefined typecode: %d",
                          static_cast<int>(citer->acData[0]) );
            }
        }
        asEED.emplace_back( sEED );
    }

    // Getting block reference attributes.
    if( dBlockRefHandle != 0 )
    {
        vector<CADAttrib>           blockRefAttributes;
        CADObject *pCADInsertObject = GetObject( dBlockRefHandle );
        unique_ptr<CADInsertObject> spoBlockRef(
                    dynamic_cast<CADInsertObject *>( pCADInsertObject ) );

        if( spoBlockRef )
        {
            if( !spoBlockRef->hAttribs.empty() )
            {
                long dCurrentEntHandle = spoBlockRef->hAttribs[0].getAsLong();
                long dLastEntHandle    = spoBlockRef->hAttribs[0].getAsLong();

                while( spoBlockRef->bHasAttribs )
                {
                    CADObject *pCADAttDefObj = GetObject( dCurrentEntHandle, true );

                    CADEntityObject * attDefObj =
                            dynamic_cast<CADEntityObject *>( pCADAttDefObj );

                    if( dCurrentEntHandle == dLastEntHandle )
                    {
                        if( attDefObj == nullptr )
                        {
                            delete pCADAttDefObj;
                            break;
                        }

                        CADAttrib * attrib = static_cast<CADAttrib *>(
                                GetGeometry( iLayerIndex, dCurrentEntHandle ) );

                        if( attrib )
                        {
                            blockRefAttributes.push_back( CADAttrib( * attrib ) );
                            delete attrib;
                        }
                        delete attDefObj;
                        break;
                    }

                    if( attDefObj != nullptr )
                    {
                        if( attDefObj->stCed.bNoLinks )
                            ++dCurrentEntHandle;
                        else
                            dCurrentEntHandle = attDefObj->stChed.hNextEntity.getAsLong( attDefObj->stCed.hObjectHandle );

                        CADAttrib * attrib = static_cast<CADAttrib *>(
                                GetGeometry( iLayerIndex, dCurrentEntHandle ) );

                        if( attrib )
                        {
                            blockRefAttributes.push_back( CADAttrib( * attrib ) );
                            delete attrib;
                        }
                        delete attDefObj;
                    }
                    else
                    {
                        delete pCADAttDefObj;
                    }
                }
                poGeometry->setBlockAttributes( blockRefAttributes );
            }
        }
        else
        {
            delete pCADInsertObject;
        }
    }

    poGeometry->setEED( asEED );
    return poGeometry;
}

CADBlockObject * DWGFileR2000::getBlock(unsigned int dObjectSize,
                                        const CADCommonED& stCommonEntityData,
                                        CADBuffer &buffer)
{
    CADBlockObject * pBlock = new CADBlockObject();

    pBlock->setSize( dObjectSize );
    pBlock->stCed = stCommonEntityData;

    pBlock->sBlockName = buffer.ReadTV();

    fillCommonEntityHandleData( pBlock, buffer);

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    pBlock->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "BLOCK" ) );

    return pBlock;
}

CADEllipseObject * DWGFileR2000::getEllipse(unsigned int dObjectSize,
                                            const CADCommonED& stCommonEntityData,
                                            CADBuffer& buffer)
{
    CADEllipseObject * ellipse = new CADEllipseObject();

    ellipse->setSize( dObjectSize );
    ellipse->stCed = stCommonEntityData;

    CADVector vertPosition = buffer.ReadVector();

    ellipse->vertPosition = vertPosition;

    CADVector vectSMAxis = buffer.ReadVector();

    ellipse->vectSMAxis = vectSMAxis;

    CADVector vectExtrusion = buffer.ReadVector();

    ellipse->vectExtrusion = vectExtrusion;

    ellipse->dfAxisRatio = buffer.ReadBITDOUBLE();
    ellipse->dfBegAngle  = buffer.ReadBITDOUBLE();
    ellipse->dfEndAngle  = buffer.ReadBITDOUBLE();

    fillCommonEntityHandleData(ellipse, buffer);

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    ellipse->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "ELLIPSE" ) );

    return ellipse;
}

CADSolidObject * DWGFileR2000::getSolid(unsigned int dObjectSize,
                                        const CADCommonED& stCommonEntityData,
                                        CADBuffer &buffer)
{
    CADSolidObject * solid = new CADSolidObject();

    solid->setSize( dObjectSize );
    solid->stCed = stCommonEntityData;

    solid->dfThickness = buffer.ReadBIT() ? 0.0f : buffer.ReadBITDOUBLE();

    solid->dfElevation = buffer.ReadBITDOUBLE();

    CADVector   oCorner;
    for( size_t i = 0; i < 4; ++i )
    {
        oCorner.setX( buffer.ReadRAWDOUBLE() );
        oCorner.setY( buffer.ReadRAWDOUBLE() );
        solid->avertCorners.push_back( oCorner );
    }

    if( buffer.ReadBIT() )
    {
        solid->vectExtrusion = CADVector( 0.0f, 0.0f, 1.0f );
    }
    else
    {
        CADVector vectExtrusion = buffer.ReadVector();
        solid->vectExtrusion = vectExtrusion;
    }


    fillCommonEntityHandleData( solid, buffer);

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    solid->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "SOLID" ) );

    return solid;
}

CADPointObject * DWGFileR2000::getPoint(unsigned int dObjectSize,
                                        const CADCommonED& stCommonEntityData,
                                        CADBuffer& buffer)
{
    CADPointObject * point = new CADPointObject();

    point->setSize( dObjectSize );
    point->stCed = stCommonEntityData;

    CADVector vertPosition = buffer.ReadVector();

    point->vertPosition = vertPosition;

    point->dfThickness = buffer.ReadBIT() ? 0.0f : buffer.ReadBITDOUBLE();

    if( buffer.ReadBIT() )
    {
        point->vectExtrusion = CADVector( 0.0f, 0.0f, 1.0f );
    }
    else
    {
        CADVector vectExtrusion = buffer.ReadVector();
        point->vectExtrusion = vectExtrusion;
    }

    point->dfXAxisAng = buffer.ReadBITDOUBLE();

    fillCommonEntityHandleData( point, buffer);

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    point->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "POINT" ) );

    return point;
}

CADPolyline3DObject * DWGFileR2000::getPolyLine3D(unsigned int dObjectSize,
                                                  const CADCommonED& stCommonEntityData,
                                                  CADBuffer &buffer)
{
    CADPolyline3DObject * polyline = new CADPolyline3DObject();

    polyline->setSize( dObjectSize );
    polyline->stCed = stCommonEntityData;

    polyline->SplinedFlags = buffer.ReadCHAR();
    polyline->ClosedFlags  = buffer.ReadCHAR();

    fillCommonEntityHandleData( polyline, buffer );

    polyline->hVertices.push_back( buffer.ReadHANDLE() ); // 1st vertex
    polyline->hVertices.push_back( buffer.ReadHANDLE() ); // last vertex

    polyline->hSeqend = buffer.ReadHANDLE();

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    polyline->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "POLYLINE" ) );

    return polyline;
}

CADRayObject * DWGFileR2000::getRay(unsigned int dObjectSize,
                                    const CADCommonED& stCommonEntityData,
                                    CADBuffer &buffer)
{
    CADRayObject * ray = new CADRayObject();

    ray->setSize( dObjectSize );
    ray->stCed = stCommonEntityData;

    CADVector vertPosition = buffer.ReadVector();

    ray->vertPosition = vertPosition;

    CADVector vectVector = buffer.ReadVector();
    ray->vectVector = vectVector;

    fillCommonEntityHandleData( ray, buffer);

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    ray->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "RAY" ) );

    return ray;
}

CADXLineObject * DWGFileR2000::getXLine(unsigned int dObjectSize,
                                        const CADCommonED& stCommonEntityData,
                                        CADBuffer &buffer)
{
    CADXLineObject * xline = new CADXLineObject();

    xline->setSize( dObjectSize );
    xline->stCed = stCommonEntityData;

    CADVector vertPosition = buffer.ReadVector();

    xline->vertPosition = vertPosition;

    CADVector vectVector = buffer.ReadVector();
    xline->vectVector = vectVector;

    fillCommonEntityHandleData( xline, buffer);

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    xline->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "XLINE" ) );

    return xline;
}

CADLineObject * DWGFileR2000::getLine(unsigned int dObjectSize,
                                      const CADCommonED& stCommonEntityData,
                                      CADBuffer &buffer)
{
    CADLineObject * line = new CADLineObject();

    line->setSize( dObjectSize );
    line->stCed = stCommonEntityData;

    bool bZsAreZeros = buffer.ReadBIT();

    CADVector vertStart, vertEnd;
    vertStart.setX( buffer.ReadRAWDOUBLE() );
    vertEnd.setX( buffer.ReadBITDOUBLEWD(vertStart.getX() ) );
    vertStart.setY( buffer.ReadRAWDOUBLE() );
    vertEnd.setY( buffer.ReadBITDOUBLEWD(vertStart.getY() ) );

    if( !bZsAreZeros )
    {
        vertStart.setZ( buffer.ReadBITDOUBLE() );
        vertEnd.setZ( buffer.ReadBITDOUBLEWD(vertStart.getZ() ) );
    }

    line->vertStart = vertStart;
    line->vertEnd   = vertEnd;

    line->dfThickness = buffer.ReadBIT() ? 0.0f : buffer.ReadBITDOUBLE();

    if( buffer.ReadBIT() )
    {
        line->vectExtrusion = CADVector( 0.0f, 0.0f, 1.0f );
    } else
    {
        CADVector vectExtrusion = buffer.ReadVector();
        line->vectExtrusion = vectExtrusion;
    }

    fillCommonEntityHandleData( line, buffer);

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    line->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "LINE" ) );
    return line;
}

CADTextObject * DWGFileR2000::getText(unsigned int dObjectSize,
                                      const CADCommonED& stCommonEntityData,
                                      CADBuffer &buffer)
{
    CADTextObject * text = new CADTextObject();

    text->setSize( dObjectSize );
    text->stCed = stCommonEntityData;

    text->DataFlags = buffer.ReadCHAR();

    if( !( text->DataFlags & 0x01 ) )
    {
        text->dfElevation = buffer.ReadRAWDOUBLE();
    }

    CADVector vertInsetionPoint = buffer.ReadRAWVector();

    text->vertInsetionPoint = vertInsetionPoint;

    if( !( text->DataFlags & 0x02 ) )
    {
        double x, y;
        x = buffer.ReadBITDOUBLEWD(vertInsetionPoint.getX() );
        y = buffer.ReadBITDOUBLEWD(vertInsetionPoint.getY() );
        CADVector vertAlignmentPoint( x, y );
        text->vertAlignmentPoint = vertAlignmentPoint;
    }

    if( buffer.ReadBIT() )
    {
        text->vectExtrusion = CADVector( 0.0f, 0.0f, 1.0f );
    }
    else
    {
        CADVector vectExtrusion = buffer.ReadVector();
        text->vectExtrusion = vectExtrusion;
    }

    text->dfThickness = buffer.ReadBIT() ? 0.0f : buffer.ReadBITDOUBLE();

    if( !( text->DataFlags & 0x04 ) )
        text->dfObliqueAng  = buffer.ReadRAWDOUBLE();
    if( !( text->DataFlags & 0x08 ) )
        text->dfRotationAng = buffer.ReadRAWDOUBLE();

    text->dfHeight = buffer.ReadRAWDOUBLE();

    if( !( text->DataFlags & 0x10 ) )
        text->dfWidthFactor = buffer.ReadRAWDOUBLE();

    text->sTextValue = buffer.ReadTV();

    if( !( text->DataFlags & 0x20 ) )
        text->dGeneration = buffer.ReadBITSHORT();
    if( !( text->DataFlags & 0x40 ) )
        text->dHorizAlign = buffer.ReadBITSHORT();
    if( !( text->DataFlags & 0x80 ) )
        text->dVertAlign  = buffer.ReadBITSHORT();

    fillCommonEntityHandleData( text, buffer);

    text->hStyle = buffer.ReadHANDLE();

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    text->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "TEXT" ) );

    return text;
}

CADVertex3DObject * DWGFileR2000::getVertex3D(unsigned int dObjectSize,
                                              const CADCommonED& stCommonEntityData,
                                              CADBuffer &buffer)
{
    CADVertex3DObject * vertex = new CADVertex3DObject();

    vertex->setSize( dObjectSize );
    vertex->stCed = stCommonEntityData;

    /*unsigned char Flags = */buffer.ReadCHAR();

    CADVector vertPosition = buffer.ReadVector();
    vertex->vertPosition = vertPosition;

    fillCommonEntityHandleData( vertex, buffer);

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    vertex->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "VERTEX" ) );
    return vertex;
}

CADCircleObject * DWGFileR2000::getCircle(unsigned int dObjectSize,
                                          const CADCommonED& stCommonEntityData,
                                          CADBuffer &buffer)
{
    CADCircleObject * circle = new CADCircleObject();

    circle->setSize( dObjectSize );
    circle->stCed = stCommonEntityData;

    CADVector vertPosition = buffer.ReadVector();
    circle->vertPosition = vertPosition;
    circle->dfRadius     = buffer.ReadBITDOUBLE();
    circle->dfThickness  = buffer.ReadBIT() ? 0.0f : buffer.ReadBITDOUBLE();

    if( buffer.ReadBIT() )
    {
        circle->vectExtrusion = CADVector( 0.0f, 0.0f, 1.0f );
    }
    else
    {
        CADVector vectExtrusion = buffer.ReadVector();
        circle->vectExtrusion = vectExtrusion;
    }

    fillCommonEntityHandleData( circle, buffer);

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    circle->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "CIRCLE" ) );
    return circle;
}

CADEndblkObject * DWGFileR2000::getEndBlock(unsigned int dObjectSize,
                                            const CADCommonED& stCommonEntityData,
                                            CADBuffer &buffer)
{
    CADEndblkObject * endblk = new CADEndblkObject();

    endblk->setSize( dObjectSize );
    endblk->stCed = stCommonEntityData;

    fillCommonEntityHandleData( endblk, buffer);

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    endblk->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "ENDBLK" ) );
    return endblk;
}

CADPolyline2DObject * DWGFileR2000::getPolyline2D(unsigned int dObjectSize,
                                                  const CADCommonED& stCommonEntityData,
                                                  CADBuffer &buffer)
{
    CADPolyline2DObject * polyline = new CADPolyline2DObject();

    polyline->setSize( dObjectSize );
    polyline->stCed = stCommonEntityData;

    polyline->dFlags                = buffer.ReadBITSHORT();
    polyline->dCurveNSmoothSurfType = buffer.ReadBITSHORT();

    polyline->dfStartWidth = buffer.ReadBITDOUBLE();
    polyline->dfEndWidth   = buffer.ReadBITDOUBLE();

    polyline->dfThickness = buffer.ReadBIT() ? 0.0f : buffer.ReadBITDOUBLE();

    polyline->dfElevation = buffer.ReadBITDOUBLE();

    if( buffer.ReadBIT() )
    {
        polyline->vectExtrusion = CADVector( 0.0f, 0.0f, 1.0f );
    }
    else
    {
        CADVector vectExtrusion = buffer.ReadVector();
        polyline->vectExtrusion = vectExtrusion;
    }

    fillCommonEntityHandleData( polyline, buffer);

    polyline->hVertices.push_back( buffer.ReadHANDLE() ); // 1st vertex
    polyline->hVertices.push_back( buffer.ReadHANDLE() ); // last vertex

    polyline->hSeqend = buffer.ReadHANDLE();

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    polyline->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "POLYLINE" ) );
    return polyline;
}

CADAttribObject * DWGFileR2000::getAttributes(unsigned int dObjectSize,
                                              const CADCommonED& stCommonEntityData,
                                              CADBuffer &buffer)
{
    CADAttribObject * attrib = new CADAttribObject();

    attrib->setSize( dObjectSize );
    attrib->stCed     = stCommonEntityData;
    attrib->DataFlags = buffer.ReadCHAR();

    if( !( attrib->DataFlags & 0x01 ) )
        attrib->dfElevation = buffer.ReadRAWDOUBLE();

    double x, y;

    CADVector vertInsetionPoint = buffer.ReadRAWVector();
    attrib->vertInsetionPoint = vertInsetionPoint;

    if( !( attrib->DataFlags & 0x02 ) )
    {
        x = buffer.ReadBITDOUBLEWD( vertInsetionPoint.getX() );
        y = buffer.ReadBITDOUBLEWD( vertInsetionPoint.getY() );
        CADVector vertAlignmentPoint( x, y );
        attrib->vertAlignmentPoint = vertAlignmentPoint;
    }

    if( buffer.ReadBIT() )
    {
        attrib->vectExtrusion = CADVector( 0.0f, 0.0f, 1.0f );
    }
    else
    {
        CADVector vectExtrusion = buffer.ReadVector();
        attrib->vectExtrusion = vectExtrusion;
    }

    attrib->dfThickness = buffer.ReadBIT() ? 0.0f : buffer.ReadBITDOUBLE();

    if( !( attrib->DataFlags & 0x04 ) )
        attrib->dfObliqueAng  = buffer.ReadRAWDOUBLE();
    if( !( attrib->DataFlags & 0x08 ) )
        attrib->dfRotationAng = buffer.ReadRAWDOUBLE();
    attrib->dfHeight          = buffer.ReadRAWDOUBLE();
    if( !( attrib->DataFlags & 0x10 ) )
        attrib->dfWidthFactor = buffer.ReadRAWDOUBLE();
    attrib->sTextValue        = buffer.ReadTV();
    if( !( attrib->DataFlags & 0x20 ) )
        attrib->dGeneration   = buffer.ReadBITSHORT();
    if( !( attrib->DataFlags & 0x40 ) )
        attrib->dHorizAlign   = buffer.ReadBITSHORT();
    if( !( attrib->DataFlags & 0x80 ) )
        attrib->dVertAlign    = buffer.ReadBITSHORT();

    attrib->sTag         = buffer.ReadTV();
    attrib->nFieldLength = buffer.ReadBITSHORT();
    attrib->nFlags       = buffer.ReadCHAR();

    fillCommonEntityHandleData( attrib, buffer);

    attrib->hStyle = buffer.ReadHANDLE();

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    attrib->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "ATTRIB" ) );
    return attrib;
}

CADAttdefObject * DWGFileR2000::getAttributesDefn(unsigned int dObjectSize,
                                                  const CADCommonED& stCommonEntityData,
                                                  CADBuffer &buffer)
{
    CADAttdefObject * attdef = new CADAttdefObject();

    attdef->setSize( dObjectSize );
    attdef->stCed     = stCommonEntityData;
    attdef->DataFlags = buffer.ReadCHAR();

    if( ( attdef->DataFlags & 0x01 ) == 0 )
        attdef->dfElevation = buffer.ReadRAWDOUBLE();

    CADVector vertInsetionPoint = buffer.ReadRAWVector();
    attdef->vertInsetionPoint = vertInsetionPoint;

    if( ( attdef->DataFlags & 0x02 ) == 0 )
    {
        double x = buffer.ReadBITDOUBLEWD( vertInsetionPoint.getX() );
        double y = buffer.ReadBITDOUBLEWD( vertInsetionPoint.getY() );
        CADVector vertAlignmentPoint( x, y );
        attdef->vertAlignmentPoint = vertAlignmentPoint;
    }

    if( buffer.ReadBIT() )
    {
        attdef->vectExtrusion = CADVector( 0.0f, 0.0f, 1.0f );
    }
    else
    {
        CADVector vectExtrusion = buffer.ReadVector();
        attdef->vectExtrusion = vectExtrusion;
    }

    attdef->dfThickness = buffer.ReadBIT() ? 0.0f :
                          buffer.ReadBITDOUBLE();

    if( ( attdef->DataFlags & 0x04 ) == 0 )
        attdef->dfObliqueAng  = buffer.ReadRAWDOUBLE();
    if( ( attdef->DataFlags & 0x08 ) == 0 )
        attdef->dfRotationAng = buffer.ReadRAWDOUBLE();
    attdef->dfHeight          = buffer.ReadRAWDOUBLE();
    if( ( attdef->DataFlags & 0x10 ) == 0 )
        attdef->dfWidthFactor = buffer.ReadRAWDOUBLE();
    attdef->sTextValue        = buffer.ReadTV();
    if( ( attdef->DataFlags & 0x20 ) == 0 )
        attdef->dGeneration   = buffer.ReadBITSHORT();
    if( ( attdef->DataFlags & 0x40 ) == 0 )
        attdef->dHorizAlign   = buffer.ReadBITSHORT();
    if( ( attdef->DataFlags & 0x80 ) == 0 )
        attdef->dVertAlign    = buffer.ReadBITSHORT();

    attdef->sTag         = buffer.ReadTV();
    attdef->nFieldLength = buffer.ReadBITSHORT();
    attdef->nFlags       = buffer.ReadCHAR();

    attdef->sPrompt = buffer.ReadTV();

    fillCommonEntityHandleData( attdef, buffer);

    attdef->hStyle = buffer.ReadHANDLE();

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    attdef->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "ATTRDEF" ) );
    return attdef;
}

CADLWPolylineObject * DWGFileR2000::getLWPolyLine(unsigned int dObjectSize,
                                                  const CADCommonED& stCommonEntityData,
                                                  CADBuffer &buffer)
{
    CADLWPolylineObject * polyline = new CADLWPolylineObject();
    polyline->setSize( dObjectSize );
    polyline->stCed = stCommonEntityData;

    int    verticesCount = 0, nBulges = 0, nNumWidths = 0;
    short  dataFlag      = buffer.ReadBITSHORT();
    if( dataFlag & 4 )
        polyline->dfConstWidth = buffer.ReadBITDOUBLE();
    if( dataFlag & 8 )
        polyline->dfElevation  = buffer.ReadBITDOUBLE();
    if( dataFlag & 2 )
        polyline->dfThickness  = buffer.ReadBITDOUBLE();
    if( dataFlag & 1 )
    {
        CADVector vectExtrusion = buffer.ReadVector();
        polyline->vectExtrusion = vectExtrusion;
    }

    verticesCount = buffer.ReadBITLONG();
    if(verticesCount < 1)
    {
        delete polyline;
        return nullptr;
    }
    if( verticesCount < 100000 )
    {
        // For some reason reserving huge amounts cause later segfaults
        // whereas an exception would have been expected
        polyline->avertVertices.reserve( static_cast<size_t>(verticesCount) );
    }

    if( dataFlag & 16 )
    {
        nBulges = buffer.ReadBITLONG();
        if(nBulges < 0)
        {
            delete polyline;
            return nullptr;
        }
        if( nBulges < 100000 )
        {
            polyline->adfBulges.reserve( static_cast<size_t>(nBulges) );
        }
    }

    // TODO: tell ODA that R2000 contains nNumWidths flag
    if( dataFlag & 32 )
    {
        nNumWidths = buffer.ReadBITLONG();
        if(nNumWidths < 0)
        {
            delete polyline;
            return nullptr;
        }
        if( nNumWidths < 100000 )
        {
            polyline->astWidths.reserve( static_cast<size_t>(nNumWidths) );
        }
    }

    if( dataFlag & 512 )
    {
        polyline->bClosed = true;
    }
    else
    {
        polyline->bClosed = false;
    }

    // First of all, read first vertex.
    CADVector vertex = buffer.ReadRAWVector();
    polyline->avertVertices.push_back( vertex );

    // All the others are not raw doubles; bitdoubles with default instead,
    // where default is previous point coords.
    size_t prev;
    for( int i = 1; i < verticesCount; ++i )
    {
        prev = size_t( i - 1 );
        double x = buffer.ReadBITDOUBLEWD( polyline->avertVertices[prev].getX() );
        double y = buffer.ReadBITDOUBLEWD( polyline->avertVertices[prev].getY() );
        if( buffer.IsEOB() )
        {
            delete polyline;
            return nullptr;
        }
        vertex.setX( x );
        vertex.setY( y );
        polyline->avertVertices.push_back( vertex );
    }

    for( int i = 0; i < nBulges; ++i )
    {
        double dfBulgeValue = buffer.ReadBITDOUBLE();
        polyline->adfBulges.push_back( dfBulgeValue );
        if( buffer.IsEOB() )
        {
            delete polyline;
            return nullptr;
        }
    }

    for( int i = 0; i < nNumWidths; ++i )
    {
        double dfStartWidth = buffer.ReadBITDOUBLE();
        double dfEndWidth   = buffer.ReadBITDOUBLE();
        if( buffer.IsEOB() )
        {
            delete polyline;
            return nullptr;
        }
        polyline->astWidths.push_back( make_pair( dfStartWidth, dfEndWidth ) );
    }

    fillCommonEntityHandleData( polyline, buffer);

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    polyline->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "WPOLYLINE" ) );
    return polyline;
}

CADArcObject * DWGFileR2000::getArc(unsigned int dObjectSize,
                                    const CADCommonED& stCommonEntityData,
                                    CADBuffer &buffer)
{
    CADArcObject * arc = new CADArcObject();

    arc->setSize( dObjectSize );
    arc->stCed = stCommonEntityData;

    CADVector vertPosition = buffer.ReadVector();
    arc->vertPosition = vertPosition;
    arc->dfRadius     = buffer.ReadBITDOUBLE();
    arc->dfThickness  = buffer.ReadBIT() ? 0.0f : buffer.ReadBITDOUBLE();

    if( buffer.ReadBIT() )
    {
        arc->vectExtrusion = CADVector( 0.0f, 0.0f, 1.0f );
    }
    else
    {
        CADVector vectExtrusion = buffer.ReadVector();
        arc->vectExtrusion = vectExtrusion;
    }

    arc->dfStartAngle = buffer.ReadBITDOUBLE();
    arc->dfEndAngle   = buffer.ReadBITDOUBLE();

    fillCommonEntityHandleData( arc, buffer);

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    arc->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "ARC" ) );
    return arc;
}

CADSplineObject * DWGFileR2000::getSpline(unsigned int dObjectSize,
                                          const CADCommonED& stCommonEntityData,
                                          CADBuffer &buffer)
{
    CADSplineObject * spline = new CADSplineObject();
    spline->setSize( dObjectSize );
    spline->stCed     = stCommonEntityData;
    spline->dScenario = buffer.ReadBITLONG();
    spline->dDegree   = buffer.ReadBITLONG();

    if( spline->dScenario == 2 )
    {
        spline->dfFitTol = buffer.ReadBITDOUBLE();
        CADVector vectBegTangDir = buffer.ReadVector();
        spline->vectBegTangDir = vectBegTangDir;
        CADVector vectEndTangDir = buffer.ReadVector();
        spline->vectEndTangDir = vectEndTangDir;

        spline->nNumFitPts = buffer.ReadBITLONG();
        if(spline->nNumFitPts < 0 || spline->nNumFitPts > 10 * 1024 * 1024)
        {
            delete spline;
            return nullptr;
        }
        spline->averFitPoints.reserve( static_cast<size_t>(spline->nNumFitPts) );
    }
    else if( spline->dScenario == 1 )
    {
        spline->bRational = buffer.ReadBIT();
        spline->bClosed   = buffer.ReadBIT();
        spline->bPeriodic = buffer.ReadBIT();
        spline->dfKnotTol = buffer.ReadBITDOUBLE();
        spline->dfCtrlTol = buffer.ReadBITDOUBLE();

        spline->nNumKnots = buffer.ReadBITLONG();
        if(spline->nNumKnots < 0 || spline->nNumKnots > 10 * 1024 * 1024)
        {
            delete spline;
            return nullptr;
        }
        spline->adfKnots.reserve( static_cast<size_t>(spline->nNumKnots) );

        spline->nNumCtrlPts = buffer.ReadBITLONG();
        if(spline->nNumCtrlPts < 0 || spline->nNumCtrlPts > 10 * 1024 * 1024)
        {
            delete spline;
            return nullptr;
        }
        spline->avertCtrlPoints.reserve( static_cast<size_t>(spline->nNumCtrlPts) );
        if( spline->bWeight )
            spline->adfCtrlPointsWeight.reserve( static_cast<size_t>(spline->nNumCtrlPts) );
        spline->bWeight = buffer.ReadBIT();
    }
#ifdef _DEBUG
    else
    {
        DebugMsg( "Spline scenario != {1,2} read: error." );
    }
#endif
    for( long i = 0; i < spline->nNumKnots; ++i )
    {
        spline->adfKnots.push_back( buffer.ReadBITDOUBLE() );
        if( buffer.IsEOB() )
        {
            delete spline;
            return nullptr;
        }
    }

    for( long i = 0; i < spline->nNumCtrlPts; ++i )
    {
        CADVector vertex = buffer.ReadVector();
        spline->avertCtrlPoints.push_back( vertex );
        if( spline->bWeight )
            spline->adfCtrlPointsWeight.push_back( buffer.ReadBITDOUBLE() );
        if( buffer.IsEOB() )
        {
            delete spline;
            return nullptr;
        }
    }

    for( long i = 0; i < spline->nNumFitPts; ++i )
    {
        CADVector vertex = buffer.ReadVector();
        if( buffer.IsEOB() )
        {
            delete spline;
            return nullptr;
        }
        spline->averFitPoints.push_back( vertex );
    }

    fillCommonEntityHandleData( spline, buffer);

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    spline->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "SPLINE" ) );
    return spline;
}

CADEntityObject * DWGFileR2000::getEntity(int dObjectType,
                                          unsigned int dObjectSize,
                                          const CADCommonED& stCommonEntityData,
                                          CADBuffer &buffer)
{
    CADEntityObject * entity = new CADEntityObject(
                    static_cast<CADObject::ObjectType>(dObjectType) );

    entity->setSize( dObjectSize );
    entity->stCed = stCommonEntityData;

    buffer.Seek(static_cast<size_t>(
                    entity->stCed.nObjectSizeInBits + 16), CADBuffer::BEG);

    fillCommonEntityHandleData( entity, buffer );

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    entity->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "ENTITY" ) );
    return entity;
}

CADInsertObject * DWGFileR2000::getInsert(int dObjectType,
                                          unsigned int dObjectSize,
                                          const CADCommonED& stCommonEntityData,
                                          CADBuffer &buffer)
{
    CADInsertObject * insert = new CADInsertObject(
                            static_cast<CADObject::ObjectType>(dObjectType) );
    insert->setSize( dObjectSize );
    insert->stCed = stCommonEntityData;

    insert->vertInsertionPoint = buffer.ReadVector();
    unsigned char dataFlags = buffer.Read2B();
    double        val41     = 1.0;
    double        val42     = 1.0;
    double        val43     = 1.0;
    if( dataFlags == 0 )
    {
        val41 = buffer.ReadRAWDOUBLE();
        val42 = buffer.ReadBITDOUBLEWD( val41 );
        val43 = buffer.ReadBITDOUBLEWD( val41 );
    }
    else if( dataFlags == 1 )
    {
        val41 = 1.0;
        val42 = buffer.ReadBITDOUBLEWD( val41 );
        val43 = buffer.ReadBITDOUBLEWD( val41 );
    }
    else if( dataFlags == 2 )
    {
        val41 = buffer.ReadRAWDOUBLE();
        val42 = val41;
        val43 = val41;
    }
    insert->vertScales    = CADVector( val41, val42, val43 );
    insert->dfRotation    = buffer.ReadBITDOUBLE();
    insert->vectExtrusion = buffer.ReadVector();
    insert->bHasAttribs   = buffer.ReadBIT();

    fillCommonEntityHandleData( insert, buffer);

    insert->hBlockHeader = buffer.ReadHANDLE();
    if( insert->bHasAttribs )
    {
        insert->hAttribs.push_back( buffer.ReadHANDLE() );
        insert->hAttribs.push_back( buffer.ReadHANDLE() );
        insert->hSeqend = buffer.ReadHANDLE();
    }

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    insert->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "INSERT" ) );

    return insert;
}

CADDictionaryObject * DWGFileR2000::getDictionary(unsigned int dObjectSize,
                                                  CADBuffer &buffer)
{
    /*
     * FIXME: ODA has a lot of mistypes in spec. for this objects,
     * it doesn't work for now (error begins in handles stream).
     * Nonetheless, dictionary->sItemNames is 100% array,
     * not a single obj as pointer by their docs.
     */
    CADDictionaryObject * dictionary = new CADDictionaryObject();

    if(!readBasicData(dictionary, dObjectSize, buffer))
    {
        delete dictionary;
        return nullptr;
    }

    dictionary->nNumItems      = buffer.ReadBITLONG();
    if(dictionary->nNumItems < 0)
    {
        delete dictionary;
        return nullptr;
    }
    dictionary->dCloningFlag   = buffer.ReadBITSHORT();
    dictionary->dHardOwnerFlag = buffer.ReadCHAR();

    for( long i = 0; i < dictionary->nNumItems; ++i )
    {
        dictionary->sItemNames.push_back( buffer.ReadTV() );
        if( buffer.IsEOB() )
        {
            delete dictionary;
            return nullptr;
        }
    }

    dictionary->hParentHandle = buffer.ReadHANDLE();

    for( long i = 0; i < dictionary->nNumReactors; ++i )
    {
        dictionary->hReactors.push_back( buffer.ReadHANDLE() );
        if( buffer.IsEOB() )
        {
            delete dictionary;
            return nullptr;
        }
    }
    dictionary->hXDictionary = buffer.ReadHANDLE();
    for( long i = 0; i < dictionary->nNumItems; ++i )
    {
        dictionary->hItemHandles.push_back( buffer.ReadHANDLE() );
        if( buffer.IsEOB() )
        {
            delete dictionary;
            return nullptr;
        }
    }

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    dictionary->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "DICT" ) );

    return dictionary;
}

CADLayerObject * DWGFileR2000::getLayerObject(unsigned int dObjectSize,
                                              CADBuffer &buffer)
{
    CADLayerObject * layer = new CADLayerObject();

    if(!readBasicData(layer, dObjectSize, buffer))
    {
        delete layer;
        return nullptr;
    }

    layer->sLayerName   = buffer.ReadTV();
    layer->b64Flag      = buffer.ReadBIT() != 0;
    layer->dXRefIndex   = buffer.ReadBITSHORT();
    layer->bXDep        = buffer.ReadBIT() != 0;

    short dFlags = buffer.ReadBITSHORT();
    layer->bFrozen           = (dFlags & 0x01) != 0;
    layer->bOn               = (dFlags & 0x02) != 0;
    layer->bFrozenInNewVPORT = (dFlags & 0x04) != 0;
    layer->bLocked           = (dFlags & 0x08) != 0;
    layer->bPlottingFlag     = (dFlags & 0x10) != 0;
    layer->dLineWeight       = dFlags & 0x03E0;
    layer->dCMColor          = buffer.ReadBITSHORT();
    layer->hLayerControl     = buffer.ReadHANDLE();
    for( long i = 0; i < layer->nNumReactors; ++i )
    {
        layer->hReactors.push_back( buffer.ReadHANDLE() );
        if( buffer.IsEOB() )
        {
            delete layer;
            return nullptr;
        }
    }
    layer->hXDictionary            = buffer.ReadHANDLE();
    layer->hExternalRefBlockHandle = buffer.ReadHANDLE();
    layer->hPlotStyle              = buffer.ReadHANDLE();
    layer->hLType                  = buffer.ReadHANDLE();

    /*
     * FIXME: ODA says that this handle should be null hard pointer. It is not.
     * Also, after reading it dObjectSize is != actual read structure's size.
     * Not used anyway, so no point to read it for now.
     * It also means that CRC cannot be computed correctly.
     */
// layer->hUnknownHandle = ReadHANDLE (pabySectionContent, nBitOffsetFromStart);

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    layer->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "LAYER" ) );
    return layer;
}

CADLayerControlObject * DWGFileR2000::getLayerControl(unsigned int dObjectSize,
                                                      CADBuffer &buffer)
{
    CADLayerControlObject * layerControl = new CADLayerControlObject();

    if(!readBasicData(layerControl, dObjectSize, buffer))
    {
        delete layerControl;
        return nullptr;
    }

    layerControl->nNumEntries  = buffer.ReadBITLONG();
    if(layerControl->nNumEntries < 0)
    {
        delete layerControl;
        return nullptr;
    }
    layerControl->hNull        = buffer.ReadHANDLE();
    layerControl->hXDictionary = buffer.ReadHANDLE();
    for( long i = 0; i < layerControl->nNumEntries; ++i )
    {
        layerControl->hLayers.push_back( buffer.ReadHANDLE() );
        if( buffer.IsEOB() )
        {
            delete layerControl;
            return nullptr;
        }
    }

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    layerControl->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "LAYERCONTROL" ) );
    return layerControl;
}

CADBlockControlObject * DWGFileR2000::getBlockControl(unsigned int dObjectSize,
                                                      CADBuffer &buffer)
{
    CADBlockControlObject * blockControl = new CADBlockControlObject();

    if(!readBasicData(blockControl, dObjectSize, buffer))
    {
        delete blockControl;
        return nullptr;
    }

    blockControl->nNumEntries  = buffer.ReadBITLONG();
    if(blockControl->nNumEntries < 0)
    {
        delete blockControl;
        return nullptr;
    }

    blockControl->hNull        = buffer.ReadHANDLE();
    blockControl->hXDictionary = buffer.ReadHANDLE();

    for( long i = 0; i < blockControl->nNumEntries + 2; ++i )
    {
        blockControl->hBlocks.push_back( buffer.ReadHANDLE() );
        if( buffer.IsEOB() )
        {
            delete blockControl;
            return nullptr;
        }
    }

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    blockControl->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "BLOCKCONTROL" ) );
    return blockControl;
}

CADBlockHeaderObject * DWGFileR2000::getBlockHeader(unsigned int dObjectSize,
                                                    CADBuffer &buffer)
{
    CADBlockHeaderObject * blockHeader = new CADBlockHeaderObject();

    if(!readBasicData(blockHeader, dObjectSize, buffer))
    {
        delete blockHeader;
        return nullptr;
    }

    blockHeader->sEntryName    = buffer.ReadTV();
    blockHeader->b64Flag       = buffer.ReadBIT();
    blockHeader->dXRefIndex    = buffer.ReadBITSHORT();
    blockHeader->bXDep         = buffer.ReadBIT();
    blockHeader->bAnonymous    = buffer.ReadBIT();
    blockHeader->bHasAtts      = buffer.ReadBIT();
    blockHeader->bBlkisXRef    = buffer.ReadBIT();
    blockHeader->bXRefOverlaid = buffer.ReadBIT();
    blockHeader->bLoadedBit    = buffer.ReadBIT();

    CADVector vertBasePoint = buffer.ReadVector();
    blockHeader->vertBasePoint = vertBasePoint;
    blockHeader->sXRefPName    = buffer.ReadTV();
    unsigned char Tmp;
    do
    {
        Tmp = buffer.ReadCHAR();
        blockHeader->adInsertCount.push_back( Tmp );
    } while( Tmp != 0 );

    blockHeader->sBlockDescription  = buffer.ReadTV();
    blockHeader->nSizeOfPreviewData = buffer.ReadBITLONG();
    if(blockHeader->nSizeOfPreviewData < 0)
    {
        delete blockHeader;
        return nullptr;
    }
    for( long i = 0; i < blockHeader->nSizeOfPreviewData; ++i )
    {
        blockHeader->abyBinaryPreviewData.push_back( buffer.ReadCHAR() );
        if( buffer.IsEOB() )
        {
            delete blockHeader;
            return nullptr;
        }
    }

    blockHeader->hBlockControl = buffer.ReadHANDLE();
    for( long i = 0; i < blockHeader->nNumReactors; ++i )
    {
        blockHeader->hReactors.push_back( buffer.ReadHANDLE() );
        if( buffer.IsEOB() )
        {
            delete blockHeader;
            return nullptr;
        }
    }
    blockHeader->hXDictionary = buffer.ReadHANDLE();
    blockHeader->hNull        = buffer.ReadHANDLE();
    blockHeader->hBlockEntity = buffer.ReadHANDLE();
    if( !blockHeader->bBlkisXRef && !blockHeader->bXRefOverlaid )
    {
        blockHeader->hEntities.push_back( buffer.ReadHANDLE() ); // first
        blockHeader->hEntities.push_back( buffer.ReadHANDLE() ); // last
    }

    blockHeader->hEndBlk = buffer.ReadHANDLE();
    for( size_t i = 0; i < blockHeader->adInsertCount.size() - 1; ++i )
        blockHeader->hInsertHandles.push_back( buffer.ReadHANDLE() );
    blockHeader->hLayout = buffer.ReadHANDLE();

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    blockHeader->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "BLOCKHEADER" ) );
    return blockHeader;
}

CADLineTypeControlObject * DWGFileR2000::getLineTypeControl(unsigned int dObjectSize,
                                                            CADBuffer &buffer)
{
    CADLineTypeControlObject * ltypeControl = new CADLineTypeControlObject();

    if(!readBasicData(ltypeControl, dObjectSize, buffer))
    {
        delete ltypeControl;
        return nullptr;
    }

    ltypeControl->nNumEntries  = buffer.ReadBITLONG();
    if(ltypeControl->nNumEntries < 0)
    {
        delete ltypeControl;
        return nullptr;
    }

    ltypeControl->hNull        = buffer.ReadHANDLE();
    ltypeControl->hXDictionary = buffer.ReadHANDLE();

    // hLTypes ends with BYLAYER and BYBLOCK
    for( long i = 0; i < ltypeControl->nNumEntries + 2; ++i )
    {
        ltypeControl->hLTypes.push_back( buffer.ReadHANDLE() );
        if( buffer.IsEOB() )
        {
            delete ltypeControl;
            return nullptr;
        }
    }

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    ltypeControl->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "LINETYPECTRL" ) );
    return ltypeControl;
}

CADLineTypeObject * DWGFileR2000::getLineType1(unsigned int dObjectSize, CADBuffer &buffer)
{
    CADLineTypeObject * ltype = new CADLineTypeObject();

    if(!readBasicData(ltype, dObjectSize, buffer))
    {
        delete ltype;
        return nullptr;
    }

    ltype->sEntryName   = buffer.ReadTV();
    ltype->b64Flag      = buffer.ReadBIT();
    ltype->dXRefIndex   = buffer.ReadBITSHORT();
    ltype->bXDep        = buffer.ReadBIT();
    ltype->sDescription = buffer.ReadTV();
    ltype->dfPatternLen = buffer.ReadBITDOUBLE();
    ltype->dAlignment   = buffer.ReadCHAR();
    ltype->nNumDashes   = buffer.ReadCHAR();

    CADDash     dash;
    for( size_t i = 0; i < ltype->nNumDashes; ++i )
    {
        dash.dfLength          = buffer.ReadBITDOUBLE();
        dash.dComplexShapecode = buffer.ReadBITSHORT();
        dash.dfXOffset         = buffer.ReadRAWDOUBLE();
        dash.dfYOffset         = buffer.ReadRAWDOUBLE();
        dash.dfScale           = buffer.ReadBITDOUBLE();
        dash.dfRotation        = buffer.ReadBITDOUBLE();
        dash.dShapeflag        = buffer.ReadBITSHORT(); // TODO: what to do with it?

        ltype->astDashes.push_back( dash );
    }

    for( short i = 0; i < 256; ++i )
        ltype->abyTextArea.push_back( buffer.ReadCHAR() );

    ltype->hLTControl = buffer.ReadHANDLE();

    for( long i = 0; i < ltype->nNumReactors; ++i )
    {
        ltype->hReactors.push_back( buffer.ReadHANDLE() );
        if( buffer.IsEOB() )
        {
            delete ltype;
            return nullptr;
        }
    }

    ltype->hXDictionary = buffer.ReadHANDLE();
    ltype->hXRefBlock   = buffer.ReadHANDLE();

    // TODO: shapefile for dash/shape (1 each). Does it mean that we have nNumDashes * 2 handles, or what?

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    ltype->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "LINETYPE" ) );
    return ltype;
}

CADMLineObject * DWGFileR2000::getMLine(unsigned int dObjectSize,
                                        const CADCommonED& stCommonEntityData,
                                        CADBuffer &buffer)
{
    CADMLineObject * mline = new CADMLineObject();

    mline->setSize( dObjectSize );
    mline->stCed = stCommonEntityData;

    mline->dfScale = buffer.ReadBITDOUBLE();
    mline->dJust   = buffer.ReadCHAR();

    CADVector vertBasePoint = buffer.ReadVector();
    mline->vertBasePoint = vertBasePoint;

    CADVector vectExtrusion = buffer.ReadVector();
    mline->vectExtrusion = vectExtrusion;
    mline->dOpenClosed   = buffer.ReadBITSHORT();
    mline->nLinesInStyle = buffer.ReadCHAR();
    mline->nNumVertices  = buffer.ReadBITSHORT();
    if(mline->nNumVertices < 0)
    {
        delete mline;
        return nullptr;
    }

    for( short i = 0; i < mline->nNumVertices; ++i )
    {
        CADMLineVertex stVertex;

        CADVector vertPosition = buffer.ReadVector();
        stVertex.vertPosition = vertPosition;

        CADVector vectDirection = buffer.ReadVector();
        stVertex.vectDirection = vectDirection;

        CADVector vectMIterDirection = buffer.ReadVector();
        stVertex.vectMIterDirection = vectMIterDirection;
        if( buffer.IsEOB() )
        {
            delete mline;
            return nullptr;
        }
        for( unsigned char j = 0; j < mline->nLinesInStyle; ++j )
        {
            CADLineStyle   stLStyle;
            stLStyle.nNumSegParams = buffer.ReadBITSHORT();
            if( stLStyle.nNumSegParams > 0 ) // Or return null here?
            {
                for( short k = 0; k < stLStyle.nNumSegParams; ++k )
                    stLStyle.adfSegparms.push_back( buffer.ReadBITDOUBLE() );
            }
            stLStyle.nAreaFillParams = buffer.ReadBITSHORT();
            if( stLStyle.nAreaFillParams > 0 )
            {
                for( short k = 0; k < stLStyle.nAreaFillParams; ++k )
                    stLStyle.adfAreaFillParameters.push_back( buffer.ReadBITDOUBLE() );
            }

            stVertex.astLStyles.push_back( stLStyle );
            if( buffer.IsEOB() )
            {
                delete mline;
                return nullptr;
            }
        }
        mline->avertVertices.push_back( stVertex );
    }

    if( mline->stCed.bbEntMode == 0 )
        mline->stChed.hOwner = buffer.ReadHANDLE();

    for( long i = 0; i < mline->stCed.nNumReactors; ++i )
        mline->stChed.hReactors.push_back( buffer.ReadHANDLE() );

    mline->stChed.hXDictionary = buffer.ReadHANDLE();

    if( !mline->stCed.bNoLinks )
    {
        mline->stChed.hPrevEntity = buffer.ReadHANDLE();
        mline->stChed.hNextEntity = buffer.ReadHANDLE();
    }

    mline->stChed.hLayer = buffer.ReadHANDLE();

    if( mline->stCed.bbLTypeFlags == 0x03 )
        mline->stChed.hLType = buffer.ReadHANDLE();

    if( mline->stCed.bbPlotStyleFlags == 0x03 )
        mline->stChed.hPlotStyle = buffer.ReadHANDLE();

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    mline->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "MLINE" ) );
    return mline;
}

CADPolylinePFaceObject * DWGFileR2000::getPolylinePFace(unsigned int dObjectSize,
                                                        const CADCommonED& stCommonEntityData,
                                                        CADBuffer &buffer)
{
    CADPolylinePFaceObject * polyline = new CADPolylinePFaceObject();

    polyline->setSize( dObjectSize );
    polyline->stCed = stCommonEntityData;

    polyline->nNumVertices = buffer.ReadBITSHORT();
    polyline->nNumFaces    = buffer.ReadBITSHORT();

    fillCommonEntityHandleData( polyline, buffer);

    polyline->hVertices.push_back( buffer.ReadHANDLE() ); // 1st vertex
    polyline->hVertices.push_back( buffer.ReadHANDLE() ); // last vertex

    polyline->hSeqend = buffer.ReadHANDLE();

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    polyline->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "POLYLINEPFACE" ) );
    return polyline;
}

CADImageObject * DWGFileR2000::getImage(unsigned int dObjectSize,
                                        const CADCommonED& stCommonEntityData,
                                        CADBuffer &buffer)
{
    CADImageObject * image = new CADImageObject();

    image->setSize( dObjectSize );
    image->stCed = stCommonEntityData;

    image->dClassVersion = buffer.ReadBITLONG();

    CADVector vertInsertion = buffer.ReadVector();
    image->vertInsertion = vertInsertion;

    CADVector vectUDirection = buffer.ReadVector();
    image->vectUDirection = vectUDirection;

    CADVector vectVDirection = buffer.ReadVector();
    image->vectVDirection = vectVDirection;

    image->dfSizeX       = buffer.ReadRAWDOUBLE();
    image->dfSizeY       = buffer.ReadRAWDOUBLE();
    image->dDisplayProps = buffer.ReadBITSHORT();

    image->bClipping         = buffer.ReadBIT();
    image->dBrightness       = buffer.ReadCHAR();
    image->dContrast         = buffer.ReadCHAR();
    image->dFade             = buffer.ReadCHAR();
    image->dClipBoundaryType = buffer.ReadBITSHORT();

    if( image->dClipBoundaryType == 1 )
    {
        CADVector vertPoint1 = buffer.ReadRAWVector();
        image->avertClippingPolygonVertices.push_back( vertPoint1 );

        CADVector vertPoint2 = buffer.ReadRAWVector();
        image->avertClippingPolygonVertices.push_back( vertPoint2 );
    }
    else
    {
        image->nNumberVerticesInClipPolygon = buffer.ReadBITLONG();
        if(image->nNumberVerticesInClipPolygon < 0)
        {
            delete image;
            return nullptr;
        }

        for( long i = 0; i < image->nNumberVerticesInClipPolygon; ++i )
        {
            CADVector vertPoint = buffer.ReadRAWVector();
            if( buffer.IsEOB() )
            {
                delete image;
                return nullptr;
            }
            image->avertClippingPolygonVertices.push_back( vertPoint );
        }
    }

    fillCommonEntityHandleData( image, buffer);

    image->hImageDef        = buffer.ReadHANDLE();
    image->hImageDefReactor = buffer.ReadHANDLE();

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    image->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "IMAGE" ) );

    return image;
}

CAD3DFaceObject * DWGFileR2000::get3DFace(unsigned int dObjectSize,
                                          const CADCommonED& stCommonEntityData,
                                          CADBuffer &buffer)
{
    CAD3DFaceObject * face = new CAD3DFaceObject();

    face->setSize( dObjectSize );
    face->stCed = stCommonEntityData;

    face->bHasNoFlagInd = buffer.ReadBIT();
    face->bZZero        = buffer.ReadBIT();

    double x, y, z;

    CADVector vertex = buffer.ReadRAWVector();
    if( !face->bZZero )
    {
        z = buffer.ReadRAWDOUBLE();
        vertex.setZ( z );
    }
    face->avertCorners.push_back( vertex );
    for( size_t i = 1; i < 4; ++i )
    {
        x = buffer.ReadBITDOUBLEWD( face->avertCorners[i - 1].getX() );
        y = buffer.ReadBITDOUBLEWD( face->avertCorners[i - 1].getY() );
        z = buffer.ReadBITDOUBLEWD( face->avertCorners[i - 1].getZ() );

        CADVector corner( x, y, z );
        face->avertCorners.push_back( corner );
    }

    if( !face->bHasNoFlagInd )
        face->dInvisFlags = buffer.ReadBITSHORT();

    fillCommonEntityHandleData( face, buffer);

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    face->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "3DFACE" ) );
    return face;
}

CADVertexMeshObject * DWGFileR2000::getVertexMesh(unsigned int dObjectSize,
                                                  const CADCommonED& stCommonEntityData,
                                                  CADBuffer &buffer)
{
    CADVertexMeshObject * vertex = new CADVertexMeshObject();

    vertex->setSize( dObjectSize );
    vertex->stCed = stCommonEntityData;

    /*unsigned char Flags = */buffer.ReadCHAR();
    CADVector vertPosition = buffer.ReadVector();
    vertex->vertPosition = vertPosition;

    fillCommonEntityHandleData( vertex, buffer);

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    vertex->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "VERTEXMESH" ) );
    return vertex;
}

CADVertexPFaceObject * DWGFileR2000::getVertexPFace(unsigned int dObjectSize,
                                                    const CADCommonED& stCommonEntityData,
                                                    CADBuffer &buffer)
{
    CADVertexPFaceObject * vertex = new CADVertexPFaceObject();

    vertex->setSize( dObjectSize );
    vertex->stCed = stCommonEntityData;

    /*unsigned char Flags = */buffer.ReadCHAR();
    CADVector vertPosition = buffer.ReadVector();
    vertex->vertPosition = vertPosition;

    fillCommonEntityHandleData( vertex, buffer);

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    vertex->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "VERTEXPFACE" ) );
    return vertex;
}

CADMTextObject * DWGFileR2000::getMText(unsigned int dObjectSize,
                                        const CADCommonED& stCommonEntityData,
                                        CADBuffer &buffer)
{
    CADMTextObject * text = new CADMTextObject();

    text->setSize( dObjectSize );
    text->stCed = stCommonEntityData;

    CADVector vertInsertionPoint = buffer.ReadVector();
    text->vertInsertionPoint = vertInsertionPoint;

    CADVector vectExtrusion = buffer.ReadVector();
    text->vectExtrusion = vectExtrusion;

    CADVector vectXAxisDir = buffer.ReadVector();
    text->vectXAxisDir = vectXAxisDir;

    text->dfRectWidth        = buffer.ReadBITDOUBLE();
    text->dfTextHeight       = buffer.ReadBITDOUBLE();
    text->dAttachment        = buffer.ReadBITSHORT();
    text->dDrawingDir        = buffer.ReadBITSHORT();
    text->dfExtents          = buffer.ReadBITDOUBLE();
    text->dfExtentsWidth     = buffer.ReadBITDOUBLE();
    text->sTextValue         = buffer.ReadTV();
    text->dLineSpacingStyle  = buffer.ReadBITSHORT();
    text->dLineSpacingFactor = buffer.ReadBITDOUBLE();
    text->bUnknownBit        = buffer.ReadBIT();

    fillCommonEntityHandleData( text, buffer);

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    text->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "MTEXT" ) );
    return text;
}

CADDimensionObject * DWGFileR2000::getDimension(short dObjectType,
                                                unsigned int dObjectSize,
                                                const CADCommonED& stCommonEntityData,
                                                CADBuffer &buffer)
{
    CADCommonDimensionData stCDD;

    CADVector vectExtrusion = buffer.ReadVector();
    stCDD.vectExtrusion = vectExtrusion;

    CADVector vertTextMidPt = buffer.ReadRAWVector();
    stCDD.vertTextMidPt = vertTextMidPt;

    stCDD.dfElevation = buffer.ReadBITDOUBLE();
    stCDD.dFlags      = buffer.ReadCHAR();

    stCDD.sUserText      = buffer.ReadTV();
    stCDD.dfTextRotation = buffer.ReadBITDOUBLE();
    stCDD.dfHorizDir     = buffer.ReadBITDOUBLE();

    stCDD.dfInsXScale   = buffer.ReadBITDOUBLE();
    stCDD.dfInsYScale   = buffer.ReadBITDOUBLE();
    stCDD.dfInsZScale   = buffer.ReadBITDOUBLE();
    stCDD.dfInsRotation = buffer.ReadBITDOUBLE();

    stCDD.dAttachmentPoint    = buffer.ReadBITSHORT();
    stCDD.dLineSpacingStyle   = buffer.ReadBITSHORT();
    stCDD.dfLineSpacingFactor = buffer.ReadBITDOUBLE();
    stCDD.dfActualMeasurement = buffer.ReadBITDOUBLE();

    CADVector vert12Pt = buffer.ReadRAWVector();
    stCDD.vert12Pt = vert12Pt;

    switch( dObjectType )
    {
        case CADObject::DIMENSION_ORDINATE:
        {
            CADDimensionOrdinateObject * dimension = new CADDimensionOrdinateObject();

            dimension->setSize( dObjectSize );
            dimension->stCed = stCommonEntityData;
            dimension->cdd   = stCDD;

            CADVector vert10pt = buffer.ReadVector();
            dimension->vert10pt = vert10pt;

            CADVector vert13pt = buffer.ReadVector();
            dimension->vert13pt = vert13pt;

            CADVector vert14pt = buffer.ReadVector();
            dimension->vert14pt = vert14pt;

            dimension->Flags2 = buffer.ReadCHAR();

            fillCommonEntityHandleData( dimension, buffer);

            dimension->hDimstyle       = buffer.ReadHANDLE();
            dimension->hAnonymousBlock = buffer.ReadHANDLE();

            buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
            dimension->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "DIM" ) );
            return dimension;
        }

        case CADObject::DIMENSION_LINEAR:
        {
            CADDimensionLinearObject * dimension = new CADDimensionLinearObject();

            dimension->setSize( dObjectSize );
            dimension->stCed = stCommonEntityData;
            dimension->cdd   = stCDD;

            CADVector vert13pt = buffer.ReadVector();
            dimension->vert13pt = vert13pt;

            CADVector vert14pt = buffer.ReadVector();
            dimension->vert14pt = vert14pt;

            CADVector vert10pt = buffer.ReadVector();
            dimension->vert10pt = vert10pt;

            dimension->dfExtLnRot = buffer.ReadBITDOUBLE();
            dimension->dfDimRot   = buffer.ReadBITDOUBLE();

            fillCommonEntityHandleData( dimension, buffer);

            dimension->hDimstyle       = buffer.ReadHANDLE();
            dimension->hAnonymousBlock = buffer.ReadHANDLE();

            buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
            dimension->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "DIM" ) );
            return dimension;
        }

        case CADObject::DIMENSION_ALIGNED:
        {
            CADDimensionAlignedObject * dimension = new CADDimensionAlignedObject();

            dimension->setSize( dObjectSize );
            dimension->stCed = stCommonEntityData;
            dimension->cdd   = stCDD;

            CADVector vert13pt = buffer.ReadVector();
            dimension->vert13pt = vert13pt;

            CADVector vert14pt = buffer.ReadVector();
            dimension->vert14pt = vert14pt;

            CADVector vert10pt = buffer.ReadVector();
            dimension->vert10pt = vert10pt;

            dimension->dfExtLnRot = buffer.ReadBITDOUBLE();

            fillCommonEntityHandleData( dimension, buffer);

            dimension->hDimstyle       = buffer.ReadHANDLE();
            dimension->hAnonymousBlock = buffer.ReadHANDLE();

            buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
            dimension->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "DIM" ) );
            return dimension;
        }

        case CADObject::DIMENSION_ANG_3PT:
        {
            CADDimensionAngular3PtObject * dimension = new CADDimensionAngular3PtObject();

            dimension->setSize( dObjectSize );
            dimension->stCed = stCommonEntityData;
            dimension->cdd   = stCDD;

            CADVector vert10pt = buffer.ReadVector();
            dimension->vert10pt = vert10pt;

            CADVector vert13pt = buffer.ReadVector();
            dimension->vert13pt = vert13pt;

            CADVector vert14pt = buffer.ReadVector();
            dimension->vert14pt = vert14pt;

            CADVector vert15pt = buffer.ReadVector();
            dimension->vert15pt = vert15pt;

            fillCommonEntityHandleData( dimension, buffer );

            dimension->hDimstyle       = buffer.ReadHANDLE();
            dimension->hAnonymousBlock = buffer.ReadHANDLE();

            buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
            dimension->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "DIM" ) );
            return dimension;
        }

        case CADObject::DIMENSION_ANG_2LN:
        {
            CADDimensionAngular2LnObject * dimension = new CADDimensionAngular2LnObject();

            dimension->setSize( dObjectSize );
            dimension->stCed = stCommonEntityData;
            dimension->cdd   = stCDD;

            CADVector vert16pt = buffer.ReadVector();
            dimension->vert16pt = vert16pt;

            CADVector vert13pt = buffer.ReadVector();
            dimension->vert13pt = vert13pt;

            CADVector vert14pt = buffer.ReadVector();
            dimension->vert14pt = vert14pt;

            CADVector vert15pt = buffer.ReadVector();
            dimension->vert15pt = vert15pt;

            CADVector vert10pt = buffer.ReadVector();
            dimension->vert10pt = vert10pt;

            fillCommonEntityHandleData( dimension, buffer);

            dimension->hDimstyle       = buffer.ReadHANDLE();
            dimension->hAnonymousBlock = buffer.ReadHANDLE();

            buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
            dimension->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "DIM" ) );
            return dimension;
        }

        case CADObject::DIMENSION_RADIUS:
        {
            CADDimensionRadiusObject * dimension = new CADDimensionRadiusObject();

            dimension->setSize( dObjectSize );
            dimension->stCed = stCommonEntityData;
            dimension->cdd   = stCDD;

            CADVector vert10pt = buffer.ReadVector();
            dimension->vert10pt = vert10pt;

            CADVector vert15pt = buffer.ReadVector();
            dimension->vert15pt = vert15pt;

            dimension->dfLeaderLen = buffer.ReadBITDOUBLE();

            fillCommonEntityHandleData( dimension, buffer);

            dimension->hDimstyle       = buffer.ReadHANDLE();
            dimension->hAnonymousBlock = buffer.ReadHANDLE();

            buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
            dimension->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "DIM" ) );
            return dimension;
        }

        case CADObject::DIMENSION_DIAMETER:
        {
            CADDimensionDiameterObject * dimension = new CADDimensionDiameterObject();

            dimension->setSize( dObjectSize );
            dimension->stCed = stCommonEntityData;
            dimension->cdd   = stCDD;

            CADVector vert15pt = buffer.ReadVector();
            dimension->vert15pt = vert15pt;

            CADVector vert10pt = buffer.ReadVector();
            dimension->vert10pt = vert10pt;

            dimension->dfLeaderLen = buffer.ReadBITDOUBLE();

            fillCommonEntityHandleData( dimension, buffer);

            dimension->hDimstyle       = buffer.ReadHANDLE();
            dimension->hAnonymousBlock = buffer.ReadHANDLE();

            buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
            dimension->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "DIM" ) );
            return dimension;
        }
    }
    return nullptr;
}

CADImageDefObject * DWGFileR2000::getImageDef(unsigned int dObjectSize,
                                              CADBuffer &buffer)
{
    CADImageDefObject * imagedef = new CADImageDefObject();

    if(!readBasicData(imagedef, dObjectSize, buffer))
    {
        delete imagedef;
        return nullptr;
    }

    imagedef->dClassVersion = buffer.ReadBITLONG();

    imagedef->dfXImageSizeInPx = buffer.ReadRAWDOUBLE();
    imagedef->dfYImageSizeInPx = buffer.ReadRAWDOUBLE();

    imagedef->sFilePath = buffer.ReadTV();
    imagedef->bIsLoaded = buffer.ReadBIT();

    imagedef->dResUnits = buffer.ReadCHAR();

    imagedef->dfXPixelSize = buffer.ReadRAWDOUBLE();
    imagedef->dfYPixelSize = buffer.ReadRAWDOUBLE();

    imagedef->hParentHandle = buffer.ReadHANDLE();

    for( long i = 0; i < imagedef->nNumReactors; ++i )
    {
        imagedef->hReactors.push_back( buffer.ReadHANDLE() );
        if( buffer.IsEOB() )
        {
            delete imagedef;
            return nullptr;
        }
    }

    imagedef->hXDictionary = buffer.ReadHANDLE();

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    imagedef->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "IMAGEDEF" ) );

    return imagedef;
}

CADImageDefReactorObject * DWGFileR2000::getImageDefReactor(unsigned int dObjectSize, CADBuffer &buffer)
{
    CADImageDefReactorObject * imagedefreactor = new CADImageDefReactorObject();

    if(!readBasicData(imagedefreactor, dObjectSize, buffer))
    {
        delete imagedefreactor;
        return nullptr;
    }

    imagedefreactor->dClassVersion = buffer.ReadBITLONG();

    imagedefreactor->hParentHandle =buffer.ReadHANDLE();

    for( long i = 0; i < imagedefreactor->nNumReactors; ++i )
    {
        imagedefreactor->hReactors.push_back( buffer.ReadHANDLE() );
        if( buffer.IsEOB() )
        {
            delete imagedefreactor;
            return nullptr;
        }
    }

    imagedefreactor->hXDictionary = buffer.ReadHANDLE();

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    imagedefreactor->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "IMAGEDEFREFACTOR" ) );

    return imagedefreactor;
}

CADXRecordObject * DWGFileR2000::getXRecord(unsigned int dObjectSize, CADBuffer &buffer)
{
    CADXRecordObject * xrecord = new CADXRecordObject();

    if(!readBasicData(xrecord, dObjectSize, buffer))
    {
        delete xrecord;
        return nullptr;
    }

    xrecord->nNumDataBytes = buffer.ReadBITLONG();
    if(xrecord->nNumDataBytes < 0)
    {
        delete xrecord;
        return nullptr;
    }
    for( long i = 0; i < xrecord->nNumDataBytes; ++i )
    {
        xrecord->abyDataBytes.push_back( buffer.ReadCHAR() );
        if( buffer.IsEOB() )
        {
            delete xrecord;
            return nullptr;
        }
    }

    xrecord->dCloningFlag = buffer.ReadBITSHORT();

    short dIndicatorNumber = buffer.ReadRAWSHORT();
    if( dIndicatorNumber == 1 )
    {
        unsigned char nStringSize = buffer.ReadCHAR();
        /* char dCodePage   =  */ buffer.ReadCHAR();
        for( unsigned char i = 0; i < nStringSize; ++i )
        {
            buffer.ReadCHAR();
        }
    }
    else if( dIndicatorNumber == 70 )
    {
        buffer.ReadRAWSHORT();
    }
    else if( dIndicatorNumber == 10 )
    {
        buffer.ReadRAWDOUBLE();
        buffer.ReadRAWDOUBLE();
        buffer.ReadRAWDOUBLE();
    }
    else if( dIndicatorNumber == 40 )
    {
        buffer.ReadRAWDOUBLE();
    }

    xrecord->hParentHandle = buffer.ReadHANDLE();

    for( long i = 0; i < xrecord->nNumReactors; ++i )
    {
        xrecord->hReactors.push_back( buffer.ReadHANDLE() );
        if( buffer.IsEOB() )
        {
            delete xrecord;
            return nullptr;
        }
    }

    xrecord->hXDictionary = buffer.ReadHANDLE();

    size_t dObjectSizeBit = (dObjectSize + 4) * 8;
    while( buffer.PositionBit() < dObjectSizeBit )
    {
        xrecord->hObjIdHandles.push_back( buffer.ReadHANDLE() );
    }

    buffer.Seek((dObjectSize - 2) * 8, CADBuffer::BEG);
    xrecord->setCRC( validateEntityCRC( buffer, dObjectSize - 2, "XRECORD" ) );

    return xrecord;
}

void DWGFileR2000::fillCommonEntityHandleData(CADEntityObject * pEnt,
                                              CADBuffer& buffer)
{
    if( pEnt->stCed.bbEntMode == 0 )
        pEnt->stChed.hOwner = buffer.ReadHANDLE();

    // TODO: Need some reasonable nNumReactors limits.
    if(pEnt->stCed.nNumReactors < 0 || pEnt->stCed.nNumReactors > 5000)
    {
        // Something wrong occurred
        return;
    }
    for( long i = 0; i < pEnt->stCed.nNumReactors; ++i )
        pEnt->stChed.hReactors.push_back( buffer.ReadHANDLE() );

    pEnt->stChed.hXDictionary = buffer.ReadHANDLE();

    if( !pEnt->stCed.bNoLinks )
    {
        pEnt->stChed.hPrevEntity = buffer.ReadHANDLE();
        pEnt->stChed.hNextEntity = buffer.ReadHANDLE();
    }

    pEnt->stChed.hLayer = buffer.ReadHANDLE();

    if( pEnt->stCed.bbLTypeFlags == 0x03 )
        pEnt->stChed.hLType = buffer.ReadHANDLE();

    if( pEnt->stCed.bbPlotStyleFlags == 0x03 )
        pEnt->stChed.hPlotStyle = buffer.ReadHANDLE();
}

DWGFileR2000::DWGFileR2000( CADFileIO * poFileIO ) :
    CADFile( poFileIO ),
    imageSeeker(0)
{
    oHeader.addValue( CADHeader::OPENCADVER, CADVersions::DWG_R2000 );
}

int DWGFileR2000::ReadSectionLocators()
{
    char  abyBuf[255] = { 0 };
    int   dImageSeeker = 0, SLRecordsCount = 0;
    short dCodePage = 0;

    pFileIO->Rewind();
    memset( abyBuf, 0, DWG_VERSION_STR_SIZE + 1 );
    pFileIO->Read( abyBuf, DWG_VERSION_STR_SIZE );
    oHeader.addValue( CADHeader::ACADVER, abyBuf );
    memset( abyBuf, 0, 8 );
    pFileIO->Read( abyBuf, 7 );
    oHeader.addValue( CADHeader::ACADMAINTVER, abyBuf );
    // TODO: code can be much simplified if CADHandle will be used.
    pFileIO->Read( & dImageSeeker, 4 );
    // to do so, == and ++ operators should be implemented.
    DebugMsg( "Image seeker read: %d\n", dImageSeeker );
    imageSeeker = dImageSeeker;

    pFileIO->Seek( 2, CADFileIO::SeekOrigin::CUR ); // 19
    pFileIO->Read( & dCodePage, 2 );
    oHeader.addValue( CADHeader::DWGCODEPAGE, dCodePage );

    DebugMsg( "DWG Code page: %d\n", dCodePage );

    pFileIO->Read( & SLRecordsCount, 4 ); // 21
    // Last vertex is reached. read it and break reading.
    DebugMsg( "Section locator records count: %d\n", SLRecordsCount );

    for( size_t i = 0; i < static_cast<size_t>(SLRecordsCount); ++i )
    {
        SectionLocatorRecord readRecord;
        if( pFileIO->Read( & readRecord.byRecordNumber, 1 ) != 1 ||
            pFileIO->Read( & readRecord.dSeeker, 4 ) != 4 ||
            pFileIO->Read( & readRecord.dSize, 4 ) != 4 )
        {
            return CADErrorCodes::HEADER_SECTION_READ_FAILED;
        }

        sectionLocatorRecords.push_back( readRecord );
        DebugMsg( "  Record #%d : %d %d\n", sectionLocatorRecords[i].byRecordNumber, sectionLocatorRecords[i].dSeeker,
                  sectionLocatorRecords[i].dSize );
    }
    if( sectionLocatorRecords.size() < 3 )
        return CADErrorCodes::HEADER_SECTION_READ_FAILED;

    return CADErrorCodes::SUCCESS;
}

CADDictionary DWGFileR2000::GetNOD()
{
    CADDictionary stNOD;
    unique_ptr<CADObject> pCADDictionaryObject( GetObject( oTables.GetTableHandle(
                                  CADTables::NamedObjectsDict ).getAsLong() ) );

    CADDictionaryObject* spoNamedDictObj =
            dynamic_cast<CADDictionaryObject*>( pCADDictionaryObject.get() );
    if( !spoNamedDictObj )
    {
        return stNOD;
    }

    for( size_t i = 0; i < spoNamedDictObj->sItemNames.size(); ++i )
    {
        unique_ptr<CADObject> spoDictRecord (
                    GetObject( spoNamedDictObj->hItemHandles[i].getAsLong() ) );

        if( spoDictRecord == nullptr )
            continue; // Skip unread objects

        if( spoDictRecord->getType() == CADObject::DICTIONARY )
        {
            // TODO: add implementation of DICTIONARY reading
        }
        else if( spoDictRecord->getType() == CADObject::XRECORD )
        {
            CADXRecord * cadxRecord = new CADXRecord();
            CADXRecordObject * cadxRecordObject =
                static_cast<CADXRecordObject*>(spoDictRecord.get());

            string xRecordData( cadxRecordObject->abyDataBytes.begin(),
                                cadxRecordObject->abyDataBytes.end() );
            cadxRecord->setRecordData( xRecordData );

            shared_ptr<CADDictionaryRecord> cadxRecordPtr(static_cast<CADDictionaryRecord*>(cadxRecord));

            stNOD.addRecord( make_pair( spoNamedDictObj->sItemNames[i], cadxRecordPtr ) );
        }
    }

    return stNOD;
}

unsigned short DWGFileR2000::validateEntityCRC(CADBuffer& buffer,
                                               unsigned int dObjectSize,
                                               const char * entityName,
                                               bool bSwapEndianness )
{
    unsigned short CRC = static_cast<unsigned short>(buffer.ReadRAWSHORT());
    if(bSwapEndianness)
    {
        SwapEndianness(CRC, sizeof (CRC));
    }

    buffer.Seek(0, CADBuffer::BEG);
    const unsigned short initial = 0xC0C1;
    const unsigned short calculated =
            CalculateCRC8(initial, static_cast<const char*>(buffer.GetRawBuffer()),
                          static_cast<int>(dObjectSize) );
    if( CRC != calculated )
    {
        DebugMsg( "Invalid CRC for %s object\nCRC read:0x%X calculated:0x%X\n",
                                                  entityName, CRC, calculated );
        return 0; // If CRC equal 0 - this is error
    }
    return CRC;
}

bool DWGFileR2000::readBasicData(CADBaseControlObject *pBaseControlObject,
                           unsigned int dObjectSize,
                           CADBuffer &buffer)
{
    pBaseControlObject->setSize( dObjectSize );
    pBaseControlObject->nObjectSizeInBits = buffer.ReadRAWLONG();
    pBaseControlObject->hObjectHandle = buffer.ReadHANDLE();
    short  dEEDSize = 0;
    CADEed dwgEed;
    while( ( dEEDSize = buffer.ReadBITSHORT() ) != 0 )
    {
        dwgEed.dLength = dEEDSize;
        dwgEed.hApplication = buffer.ReadHANDLE();

        if(dEEDSize > 0)
        {
            for( short i = 0; i < dEEDSize; ++i )
            {
                dwgEed.acData.push_back( buffer.ReadCHAR() );
            }
        }

        pBaseControlObject->aEED.push_back( dwgEed );
    }

    pBaseControlObject->nNumReactors = buffer.ReadBITLONG();
    // TODO: Need reasonable nNumReactors limits.
    if(pBaseControlObject->nNumReactors < 0 ||
       pBaseControlObject->nNumReactors > 5000)
    {
        return false;
    }
    return true;
}
