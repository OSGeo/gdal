/******************************************************************************
 * $Id$
 *
 * Project:  GDAL 
 * Purpose:  JP2 Box Reader (and GMLJP2 Interpreter)
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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
 *****************************************************************************
 *
 * $Log$
 * Revision 1.4  2006/04/07 05:35:25  fwarmerdam
 * Added ReadAndParse() method, which includes worldfile reading.
 * Actually set HaveGeoTransform flag properly.
 *
 * Revision 1.3  2005/07/05 22:09:00  fwarmerdam
 * add preliminary support for MSIG boxes
 *
 * Revision 1.2  2005/05/05 20:17:15  fwarmerdam
 * support dictionary lookups
 *
 * Revision 1.1  2005/05/03 21:10:59  fwarmerdam
 * New
 *
 */

#ifndef _JP2READER_H_INCLUDED 
#define _JP2READER_H_INCLUDED 

#include "cpl_conv.h"
#include "cpl_vsi.h"
#include "gdal.h"

/************************************************************************/
/*                              GDALJP2Box                              */
/************************************************************************/

class CPL_DLL GDALJP2Box
{

    FILE        *fpVSIL;

    char        szBoxType[5];

    GIntBig     nBoxOffset;
    GIntBig     nBoxLength;

    GIntBig     nDataOffset;

    GByte       abyUUID[16];

public:
                GDALJP2Box( FILE * );
                ~GDALJP2Box();

    int         SetOffset( GIntBig nNewOffset );
    int         ReadBox();

    int         ReadFirst();
    int         ReadNext();

    int         ReadFirstChild( GDALJP2Box *poSuperBox );
    int         ReadNextChild( GDALJP2Box *poSuperBox );

    GIntBig     GetDataLength();
    const char *GetType() { return szBoxType; }
    
    GByte      *ReadBoxData();

    int         IsSuperBox();

    int         DumpReadable( FILE * );

    FILE        *GetFILE() { return fpVSIL; }

    const GByte *GetUUID() { return abyUUID; }
};

/************************************************************************/
/*                           GDALJP2Metadata                            */
/************************************************************************/

class CPL_DLL GDALJP2Metadata

{
  private:
    void    CollectGMLData( GDALJP2Box * );
    int     GMLSRSLookup( const char *pszURN );

  public:
    int     bHaveGeoTransform;
    double  adfGeoTransform[6];

    char    *pszProjection;

    int         nGCPCount;
    GDAL_GCP    *pasGCPList;

    char   **papszGMLMetadata;
    
    int    nGeoTIFFSize;
    GByte  *pabyGeoTIFFData;

    int    nMSIGSize;
    GByte  *pabyMSIGData;

            GDALJP2Metadata();
           ~GDALJP2Metadata();

    int     ReadBoxes( FILE * fpVSIL );

    int     ParseJP2GeoTIFF();
    int     ParseMSIG();
    int     ParseGMLCoverageDesc();

    int     ReadAndParse( const char *pszFilename );
};

#endif /* ndef _JP2READER_H_INCLUDED */


