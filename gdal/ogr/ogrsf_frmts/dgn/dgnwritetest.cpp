/******************************************************************************
 * $Id$
 *
 * Project:  Microstation DGN Access Library
 * Purpose:  Test program for use of write api.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
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

#include "dgnlib.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int /* argc */, char ** /* argv */ )

{
/* -------------------------------------------------------------------- */
/*      Create new DGN file.                                            */
/* -------------------------------------------------------------------- */
    DGNHandle hNewDGN = DGNCreate( "out.dgn", "seed.dgn",
                                   DGNCF_USE_SEED_UNITS
                                   | DGNCF_USE_SEED_ORIGIN,
                                   0.0, 0.0, 0.0, 0, 0, "", "" );

    if( hNewDGN == NULL )
    {
        printf( "DGNCreate failed.\n" );/*ok*/
        exit( 10 );
    }

/* -------------------------------------------------------------------- */
/*      Write one line segment to it.                                   */
/* -------------------------------------------------------------------- */
    DGNPoint asPoints[10] = {};

    asPoints[0].x = 0;
    asPoints[0].y = 0;
    asPoints[0].z = 100;
    asPoints[1].x = 10000;
    asPoints[1].y = 4000;
    asPoints[1].z = 110;

    DGNElemCore *psLine =
        DGNCreateMultiPointElem( hNewDGN, DGNT_LINE, 2, asPoints );
    DGNUpdateElemCore( hNewDGN, psLine, 15, 0, 3, 1, 0 );
    DGNWriteElement( hNewDGN, psLine );
    DGNFreeElement( hNewDGN, psLine );

/* -------------------------------------------------------------------- */
/*      Write a line string.                                            */
/* -------------------------------------------------------------------- */
    asPoints[0].x = 0;
    asPoints[0].y = 1000;
    asPoints[1].x = 6000;
    asPoints[1].y = 5000;
    asPoints[2].x = 12000;
    asPoints[2].y = 6000;

    psLine = DGNCreateMultiPointElem( hNewDGN, DGNT_LINE_STRING, 3, asPoints );
    DGNUpdateElemCore( hNewDGN, psLine, 15, 0, 3, 1, 0 );
    DGNWriteElement( hNewDGN, psLine );
    DGNFreeElement( hNewDGN, psLine );

/* -------------------------------------------------------------------- */
/*      Write an Arc.                                                   */
/* -------------------------------------------------------------------- */
    psLine = DGNCreateArcElem( hNewDGN, DGNT_ARC,
                               2000.0, 3000.0, 500.0, 2000.0, 1000.0,
                               0.0, 270.0, 0.0, NULL );

    DGNUpdateElemCore( hNewDGN, psLine, 15, 0, 3, 1, 0 );
    DGNWriteElement( hNewDGN, psLine );
    DGNFreeElement( hNewDGN, psLine );

/* -------------------------------------------------------------------- */
/*      Write an Ellipse with fill info.                                */
/* -------------------------------------------------------------------- */
    psLine = DGNCreateArcElem( hNewDGN, DGNT_ELLIPSE,
                               200.0, 30.0, 5.0, 10.0, 10.0,
                               0.0, 360.0, 0.0, NULL );

    DGNUpdateElemCore( hNewDGN, psLine, 15, 0, 3, 1, 0 );
    DGNWriteElement( hNewDGN, psLine );
    DGNFreeElement( hNewDGN, psLine );

/* -------------------------------------------------------------------- */
/*      Write some text.                                                */
/* -------------------------------------------------------------------- */
    psLine = DGNCreateTextElem( hNewDGN, "This is a test string",
                                0, DGNJ_CENTER_TOP, 200.0, 200.0, 0.0, NULL,
                                2000.0, 3000.0, 0.0 );

    DGNAddMSLink( hNewDGN, psLine, DGNLT_XBASE, 7, 101 );
    DGNAddMSLink( hNewDGN, psLine, DGNLT_DMRS, 7, 101 );
    DGNUpdateElemCore( hNewDGN, psLine, 15, 0, 3, 1, 0 );
    DGNWriteElement( hNewDGN, psLine );
    DGNFreeElement( hNewDGN, psLine );

    psLine = DGNCreateTextElem( hNewDGN, "------- 30 degrees",
                                0, DGNJ_CENTER_TOP, 200.0, 200.0, 30.0, NULL,
                                2000.0, 3000.0, 0.0 );

    DGNUpdateElemCore( hNewDGN, psLine, 15, 0, 3, 1, 0 );
    DGNWriteElement( hNewDGN, psLine );
    DGNFreeElement( hNewDGN, psLine );

    psLine = DGNCreateTextElem( hNewDGN, "------- 90 degrees",
                                0, DGNJ_CENTER_TOP, 200.0, 200.0, 90.0, NULL,
                                2000.0, 3000.0, 0.0 );

    DGNUpdateElemCore( hNewDGN, psLine, 15, 0, 3, 1, 0 );
    DGNWriteElement( hNewDGN, psLine );
    DGNFreeElement( hNewDGN, psLine );

/* -------------------------------------------------------------------- */
/*      Write a complex shape consisting of two line strings.           */
/* -------------------------------------------------------------------- */
    asPoints[0].x = 8000;
    asPoints[0].y = 8000;
    asPoints[1].x = 6000;
    asPoints[1].y = 8000;
    asPoints[2].x = 6000;
    asPoints[2].y = 6000;

    DGNElemCore *psMembers[2] = {
        DGNCreateMultiPointElem( hNewDGN, DGNT_LINE_STRING, 3, asPoints ),
        DGNCreateMultiPointElem( hNewDGN, DGNT_LINE_STRING, 3, asPoints )
    };
    DGNUpdateElemCore( hNewDGN, psMembers[0], 9, 0, 3, 1, 0 );

    asPoints[0].x = 6000;
    asPoints[0].y = 6000;
    asPoints[1].x = 8000;
    asPoints[1].y = 6000;
    asPoints[2].x = 8000;
    asPoints[2].y = 8000;

    DGNUpdateElemCore( hNewDGN, psMembers[1], 9, 0, 3, 1, 0 );

    psLine = DGNCreateComplexHeaderFromGroup( hNewDGN,
                                              DGNT_COMPLEX_SHAPE_HEADER,
                                              2, psMembers );

    DGNUpdateElemCore( hNewDGN, psLine, 9, 0, 3, 1, 0 );
    DGNAddShapeFillInfo( hNewDGN, psLine, 7 );

    DGNWriteElement( hNewDGN, psLine );
    DGNWriteElement( hNewDGN, psMembers[0] );
    DGNWriteElement( hNewDGN, psMembers[1] );

    DGNFreeElement( hNewDGN, psLine );
    DGNFreeElement( hNewDGN, psMembers[0] );
    DGNFreeElement( hNewDGN, psMembers[1] );

/* -------------------------------------------------------------------- */
/*      Write a cell with two lines.                                    */
/* -------------------------------------------------------------------- */
    asPoints[0].x = 7000;
    asPoints[0].y = 7000;
    asPoints[1].x = 5000;
    asPoints[1].y = 7000;
    asPoints[2].x = 5000;
    asPoints[2].y = 5000;

    psMembers[0] = DGNCreateMultiPointElem( hNewDGN, DGNT_LINE_STRING, 3,
                                            asPoints );
    DGNUpdateElemCore( hNewDGN, psMembers[0], 10, 0, 3, 1, 0 );

    asPoints[0].x = 5000;
    asPoints[0].y = 5000;
    asPoints[1].x = 8000;
    asPoints[1].y = 5000;
    asPoints[2].x = 7000;
    asPoints[2].y = 7000;

    psMembers[1] = DGNCreateMultiPointElem( hNewDGN, DGNT_LINE_STRING, 3,
                                            asPoints );
    DGNUpdateElemCore( hNewDGN, psMembers[1], 9, 0, 3, 1, 0 );

    asPoints[0].x = 5000;
    asPoints[0].y = 5000;

    psLine = DGNCreateCellHeaderFromGroup( hNewDGN, "BE70", 1, NULL,
                                           2, psMembers, asPoints + 0,
                                           1.0, 1.0, 0.0 );

    DGNWriteElement( hNewDGN, psLine );
    DGNWriteElement( hNewDGN, psMembers[0] );
    DGNWriteElement( hNewDGN, psMembers[1] );

    DGNFreeElement( hNewDGN, psLine );
    DGNFreeElement( hNewDGN, psMembers[0] );
    DGNFreeElement( hNewDGN, psMembers[1] );

/* -------------------------------------------------------------------- */
/*      Close it.                                                       */
/* -------------------------------------------------------------------- */
    DGNClose( hNewDGN );

    return 0;
}
