/******************************************************************************
 * $Id$
 *
 * Project:  Microstation DGN Access Library
 * Purpose:  DGN Access Library element reading code.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam (warmerda@home.com)
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2000/11/28 19:03:47  warmerda
 * New
 *
 */

#include "dgnlibp.h"

/************************************************************************/
/*                           DGNReadElement()                           */
/************************************************************************/

DGNElemCore *DGNReadElement( DGNHandle hDGN )

{
    DGNInfo	*psDGN = (DGNInfo *) hDGN;
    DGNElemCore *psElement = NULL;

/* -------------------------------------------------------------------- */
/*      Read the first four bytes to get the level, type, and word      */
/*      count.                                                          */
/* -------------------------------------------------------------------- */
    int		nType, nWords, nLevel;

    if( VSIFRead( psDGN->abyElem, 1, 4, psDGN->fp ) != 4 )
        return NULL;

    nWords = psDGN->abyElem[2] + psDGN->abyElem[3]*256;
    nType = psDGN->abyElem[1] & 0x7f;
    nLevel = psDGN->abyElem[0] & 0x3f;

/* -------------------------------------------------------------------- */
/*      Read the rest of the element data into the working buffer.      */
/* -------------------------------------------------------------------- */
    if( (int) VSIFRead( psDGN->abyElem + 4, 2, nWords, psDGN->fp ) != nWords )
    {
        return NULL;
    }

    psDGN->nElemBytes = nWords * 2 + 4;
    
/* -------------------------------------------------------------------- */
/*      Handle based on element type.                                   */
/* -------------------------------------------------------------------- */
    switch( nType )
    {
      case DGNT_LINE:
      {
          DGNElemMultiPoint *psLine;

          psLine = (DGNElemMultiPoint *) 
              CPLCalloc(sizeof(DGNElemMultiPoint),1);
          psElement = (DGNElemCore *) psLine;
          DGNParseCore( psDGN, psElement );

          psLine->num_vertices = 2;
          psLine->vertices[0].x = DGN_INT32( psDGN->abyElem + 36 );
          psLine->vertices[0].y = DGN_INT32( psDGN->abyElem + 40 );
          psLine->vertices[1].x = DGN_INT32( psDGN->abyElem + 44 );
          psLine->vertices[1].y = DGN_INT32( psDGN->abyElem + 48 );
      }
      break;

      case DGNT_LINE_STRING:
      case DGNT_SHAPE:
      case DGNT_CURVE:
      case DGNT_BSPLINE:
      {
          DGNElemMultiPoint *psLine;
          int                i, count;

          count = psDGN->abyElem[36] + psDGN->abyElem[37]*256;
          psLine = (DGNElemMultiPoint *) 
             CPLCalloc(sizeof(DGNElemMultiPoint)+(count-2)*sizeof(DGNPoint),1);
          psElement = (DGNElemCore *) psLine;
          DGNParseCore( psDGN, psElement );

          psLine->num_vertices = count;
          for( i = 0; i < psLine->num_vertices; i++ )
          {
              psLine->vertices[i].x = DGN_INT32( psDGN->abyElem + 38 + i*8 );
              psLine->vertices[i].y = DGN_INT32( psDGN->abyElem + 42 + i*8 );
          }
      }
      break;

      case DGNT_GROUP_DATA:
        if( nLevel == DGN_GDL_COLOR_TABLE )
        {
            DGNElemColorTable  *psColorTable;
            
            psColorTable = (DGNElemColorTable *) 
                CPLCalloc(sizeof(DGNElemColorTable),1);
            psElement = (DGNElemCore *) psColorTable;

            DGNParseCore( psDGN, psElement );

            psColorTable->screen_flag = 
                psDGN->abyElem[36] + psDGN->abyElem[37] * 256;

            memcpy( psColorTable->color_info, psDGN->abyElem+38, 768 );	
        }
        else
        {
            psElement = (DGNElemCore *) CPLCalloc(sizeof(DGNElemCore),1);
            DGNParseCore( psDGN, psElement );
        }
        break;

      default:
      {
          psElement = (DGNElemCore *) CPLCalloc(sizeof(DGNElemCore),1);
          DGNParseCore( psDGN, psElement );
      }
      break;
    }

    return psElement;
}

/************************************************************************/
/*                            DGNParseCore()                            */
/************************************************************************/

int DGNParseCore( DGNInfo *psDGN, DGNElemCore *psElement )

{
    GByte	*psData = psDGN->abyElem+0;

    psElement->level = psData[0] & 0x3f;
    psElement->complex = psData[0] & 0x80;
    psElement->type = psData[1] & 0x7f;

    if( psDGN->nElemBytes >= 36 )
    {
        psElement->graphic_group = psData[28] + psData[29] * 256;
        psElement->properties = psData[32] + psData[33] * 256;
        psElement->style = psData[34] & 0x7;
        psElement->weight = (psData[34] & 0xf8) >> 3;
        psElement->color = psData[35];
    }
    
    return TRUE;
}

/************************************************************************/
/*                           DGNFreeElement()                           */
/************************************************************************/

void DGNFreeElement( DGNHandle hDGN, DGNElemCore *psElement )

{
    CPLFree( psElement );
}

/************************************************************************/
/*                             DGNRewind()                              */
/************************************************************************/

void DGNRewind( DGNHandle hDGN )

{
    DGNInfo	*psDGN = (DGNInfo *) hDGN;

    VSIRewind( psDGN->fp );

    psDGN->nElementOffset = 0;
}


/************************************************************************/
/*                           DGNDumpElement()                           */
/************************************************************************/

void DGNDumpElement( DGNHandle hDGN, DGNElemCore *psElement, FILE *fp )

{
    fprintf( fp, "\n" );
    fprintf( fp, "Element:%-12s Level:%2d ",
             DGNTypeToName( psElement->type ),
             psElement->level );
    if( psElement->complex )
        fprintf( fp, "(Complex) " );

    fprintf( fp, "\n" );

    fprintf( fp, 
             "  graphic_group:%-3d properties:%04x color:%d weight:%d style:%d\n", 
             psElement->graphic_group,
             psElement->properties,
             psElement->color,
             psElement->weight,
             psElement->style );

    switch( psElement->type )
    {
      case DGNT_LINE:
      case DGNT_LINE_STRING:
      case DGNT_SHAPE:
      case DGNT_CURVE:
      case DGNT_BSPLINE:
      {
          DGNElemMultiPoint	*psLine = (DGNElemMultiPoint *) psElement;
          int			i;
          
          for( i=0; i < psLine->num_vertices; i++ )
              fprintf( fp, "  (%g,%g,%g)\n", 
                       psLine->vertices[i].x, 
                       psLine->vertices[i].y, 
                       psLine->vertices[i].z );
      }
      break;

      case DGNT_GROUP_DATA:
        if( psElement->level == DGN_GDL_COLOR_TABLE )
        {
            DGNElemColorTable *psCT = (DGNElemColorTable *) psElement;
            int			i;

            fprintf( fp, "  screen_flag: %d\n", psCT->screen_flag );
            for( i = 0; i < 256; i++ )
            {
                fprintf( fp, "  %3d: (%3d,%3d,%3d)\n",
                         i, 
                         psCT->color_info[i][0], 
                         psCT->color_info[i][1], 
                         psCT->color_info[i][2] );
            }
        }
        break;

      default:
        break;
    }
}

/************************************************************************/
/*                           DGNTypeToName()                            */
/************************************************************************/

const char *DGNTypeToName( int nType )

{
    static char	szNumericResult[16];

    switch( nType )
    {
      case DGNT_CELL_LIBRARY:
        return "Cell Library";
        
      case DGNT_CELL_HEADER:
        return "Cell Header";
        
      case DGNT_LINE:
        return "Line";
        
      case DGNT_LINE_STRING:
        return "Line String";

      case DGNT_GROUP_DATA:
        return "Group Data";

      case DGNT_SHAPE:
        return "Shape";
        
      case DGNT_TEXT_NODE:
        return "Text Node";

      case DGNT_DIGITIZER_SETUP:
        return "Digitizer Setup";
        
      case DGNT_TCB:
        return "TCB";
        
      case DGNT_LEVEL_SYMBOLOGY:
        return "Level Symbology";
        
      case DGNT_CURVE:
        return "Curve";
        
      case DGNT_COMPLEX_CHAIN_HEADER:
        return "Complex Chain Header";
        
      case DGNT_COMPLEX_SHAPE_HEADER:
        return "Complex Shape Header";
        
      case DGNT_ELLIPSE:
        return "Ellipse";
        
      case DGNT_ARC:
        return "Arc";
        
      case DGNT_TEXT:
        return "Text";

      case DGNT_BSPLINE:
        return "B-Spline";
        
      default:
        sprintf( szNumericResult, "%d", nType );
        return szNumericResult;
    }
}
