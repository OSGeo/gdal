/******************************************************************************
 * $Id$
 *
 * Project:  Microstation DGN Access Library
 * Purpose:  DGN Access Library element reading code.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam (warmerdam@pobox.com)
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
 * Revision 1.3  2000/12/28 21:28:59  warmerda
 * added element index support
 *
 * Revision 1.2  2000/12/14 17:10:57  warmerda
 * implemented TCB, Ellipse, TEXT
 *
 * Revision 1.1  2000/11/28 19:03:47  warmerda
 * New
 *
 */

#include "dgnlibp.h"

/************************************************************************/
/*                           DGNGotoElement()                           */
/************************************************************************/

int DGNGotoElement( DGNHandle hDGN, int element_id )

{
    DGNInfo	*psDGN = (DGNInfo *) hDGN;

    DGNBuildIndex( psDGN );

    if( element_id < 0 || element_id >= psDGN->element_count )
        return FALSE;

    if( VSIFSeek( psDGN->fp, psDGN->element_index[element_id].offset, 
                  SEEK_SET ) != 0 )
        return FALSE;

    psDGN->next_element_id = element_id;

    return TRUE;
}

/************************************************************************/
/*                         DGNLoadRawElement()                          */
/************************************************************************/

static int DGNLoadRawElement( DGNInfo *psDGN, int *pnType, int *pnLevel )

{
/* -------------------------------------------------------------------- */
/*      Read the first four bytes to get the level, type, and word      */
/*      count.                                                          */
/* -------------------------------------------------------------------- */
    int		nType, nWords, nLevel;

    if( VSIFRead( psDGN->abyElem, 1, 4, psDGN->fp ) != 4 )
        return FALSE;

    /* Is this an 0xFFFF endof file marker? */
    if( psDGN->abyElem[0] == 0xff && psDGN->abyElem[1] == 0xff )
        return FALSE;

    nWords = psDGN->abyElem[2] + psDGN->abyElem[3]*256;
    nType = psDGN->abyElem[1] & 0x7f;
    nLevel = psDGN->abyElem[0] & 0x3f;

/* -------------------------------------------------------------------- */
/*      Read the rest of the element data into the working buffer.      */
/* -------------------------------------------------------------------- */
    if( (int) VSIFRead( psDGN->abyElem + 4, 2, nWords, psDGN->fp ) != nWords )
        return FALSE;

    psDGN->nElemBytes = nWords * 2 + 4;

    psDGN->next_element_id++;

/* -------------------------------------------------------------------- */
/*      Return requested info.                                          */
/* -------------------------------------------------------------------- */
    if( pnType != NULL )
        *pnType = nType;
    
    if( pnLevel != NULL )
        *pnLevel = nLevel;
    
    return TRUE;
}

/************************************************************************/
/*                           DGNReadElement()                           */
/************************************************************************/

DGNElemCore *DGNReadElement( DGNHandle hDGN )

{
    DGNInfo	*psDGN = (DGNInfo *) hDGN;
    DGNElemCore *psElement = NULL;
    int		nType, nLevel;

#ifdef DGN_DEBUG
    GUInt32     nOffset;

    nOffset = VSIFTell( psDGN->fp );
#endif

/* -------------------------------------------------------------------- */
/*      Load the element data into the current buffer.                  */
/* -------------------------------------------------------------------- */
    if( !DGNLoadRawElement( psDGN, &nType, &nLevel ) )
        return NULL;

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
          psElement->stype = DGNST_MULTIPOINT;
          DGNParseCore( psDGN, psElement );

          psLine->num_vertices = 2;
          psLine->vertices[0].x = DGN_INT32( psDGN->abyElem + 36 );
          psLine->vertices[0].y = DGN_INT32( psDGN->abyElem + 40 );
          DGNTransformPoint( psDGN, psLine->vertices + 0 );
          psLine->vertices[1].x = DGN_INT32( psDGN->abyElem + 44 );
          psLine->vertices[1].y = DGN_INT32( psDGN->abyElem + 48 );
          DGNTransformPoint( psDGN, psLine->vertices + 1 );
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
          psElement->stype = DGNST_MULTIPOINT;
          DGNParseCore( psDGN, psElement );

          if( psDGN->nElemBytes < 38 + count * 8 )
          {
              CPLError( CE_Warning, CPLE_AppDefined, 
                        "Trimming multipoint vertices to %d from %d because\n"
                        "element is short.\n", 
                        (psDGN->nElemBytes - 38) / 8,
                        count );
              count = (psDGN->nElemBytes - 38) / 8;
          }
          psLine->num_vertices = count;
          for( i = 0; i < psLine->num_vertices; i++ )
          {
              psLine->vertices[i].x = DGN_INT32( psDGN->abyElem + 38 + i*8 );
              psLine->vertices[i].y = DGN_INT32( psDGN->abyElem + 42 + i*8 );
              DGNTransformPoint( psDGN, psLine->vertices + i );
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
            psElement->stype = DGNST_COLORTABLE;

            DGNParseCore( psDGN, psElement );

            psColorTable->screen_flag = 
                psDGN->abyElem[36] + psDGN->abyElem[37] * 256;

            memcpy( psColorTable->color_info, psDGN->abyElem+38, 768 );	
        }
        else
        {
            psElement = (DGNElemCore *) CPLCalloc(sizeof(DGNElemCore),1);
            psElement->stype = DGNST_CORE;
            DGNParseCore( psDGN, psElement );
        }
        break;

      case DGNT_ELLIPSE:
      {
          DGNElemEllipse *psEllipse;

          psEllipse = (DGNElemEllipse *) CPLCalloc(sizeof(DGNElemEllipse),1);
          psElement = (DGNElemCore *) psEllipse;
          psElement->stype = DGNST_ELLIPSE;
          DGNParseCore( psDGN, psElement );

          memcpy( &(psEllipse->primary_axis), psDGN->abyElem + 36, 8 );
          DGN2IEEEDouble( &(psEllipse->primary_axis) );
          psEllipse->primary_axis *= psDGN->scale;

          memcpy( &(psEllipse->secondary_axis), psDGN->abyElem + 44, 8 );
          DGN2IEEEDouble( &(psEllipse->secondary_axis) );
          psEllipse->secondary_axis *= psDGN->scale;
          
          psEllipse->rotation = DGN_INT32( psDGN->abyElem + 52 );
          psEllipse->rotation = psEllipse->rotation / 360000.0;
          
          memcpy( &(psEllipse->origin.x), psDGN->abyElem + 56, 8 );
          DGN2IEEEDouble( &(psEllipse->origin.x) );

          memcpy( &(psEllipse->origin.y), psDGN->abyElem + 64, 8 );
          DGN2IEEEDouble( &(psEllipse->origin.y) );

          DGNTransformPoint( psDGN, &(psEllipse->origin) );
      }
      break;

      case DGNT_TEXT:
      {
          DGNElemText *psText;
          int	      num_chars;

          num_chars = psDGN->abyElem[58];

          psText = (DGNElemText *) CPLCalloc(sizeof(DGNElemText)+num_chars,1);
          psElement = (DGNElemCore *) psText;
          psElement->stype = DGNST_TEXT;
          DGNParseCore( psDGN, psElement );

          psText->font_id = psDGN->abyElem[36];
          psText->justification = psDGN->abyElem[37];
          psText->length_mult = DGN_INT32( psDGN->abyElem + 38 );
          psText->height_mult = DGN_INT32( psDGN->abyElem + 42 );

          psText->rotation = DGN_INT32( psDGN->abyElem + 46 );
          psText->rotation = psText->rotation / 360000.0;

          psText->origin.x = DGN_INT32( psDGN->abyElem + 50 );
          psText->origin.y = DGN_INT32( psDGN->abyElem + 54 );
          DGNTransformPoint( psDGN, &(psText->origin) );

          memcpy( psText->string, psDGN->abyElem + 60, num_chars );
          psText->string[num_chars] = '\0';
      }
      break;

      case DGNT_TCB:
      {
          DGNElemTCB *psTCB;

          psTCB = (DGNElemTCB *) CPLCalloc(sizeof(DGNElemTCB),1);
          psElement = (DGNElemCore *) psTCB;
          psElement->stype = DGNST_TCB;
          DGNParseCore( psDGN, psElement );

          if( psDGN->abyElem[1214] & 0x40 )
              psTCB->dimension = 3;
          else
              psTCB->dimension = 2;
          
          psTCB->subunits_per_master = DGN_INT32( psDGN->abyElem + 1112 );

          psTCB->master_units[0] = (char) psDGN->abyElem[1120];
          psTCB->master_units[1] = (char) psDGN->abyElem[1121];
          psTCB->master_units[2] = '\0';

          psTCB->uor_per_subunit = DGN_INT32( psDGN->abyElem + 1116 );

          psTCB->sub_units[0] = (char) psDGN->abyElem[1122];
          psTCB->sub_units[1] = (char) psDGN->abyElem[1123];
          psTCB->sub_units[2] = '\0';

          /* NOTDEF: Add origin extraction later */
          if( !psDGN->got_tcb )
          {
              psDGN->got_tcb = TRUE;
              psDGN->dimension = psTCB->dimension;
              psDGN->origin_x = psTCB->origin_x;
              psDGN->origin_y = psTCB->origin_y;
              psDGN->origin_z = psTCB->origin_z;

              if( psTCB->uor_per_subunit != 0
                  && psTCB->subunits_per_master != 0 )
                  psDGN->scale = 1.0 
                      / (psTCB->uor_per_subunit * psTCB->subunits_per_master);
          }
          
      }
      break;

      default:
      {
          psElement = (DGNElemCore *) CPLCalloc(sizeof(DGNElemCore),1);
          psElement->stype = DGNST_CORE;
          DGNParseCore( psDGN, psElement );
      }
      break;
    }

/* -------------------------------------------------------------------- */
/*      Collect some additional generic information.                    */
/* -------------------------------------------------------------------- */
    psElement->element_id = psDGN->next_element_id - 1;

#ifdef DGN_DEBUG
    psElement->offset = nOffset;
    psElement->size = psDGN->nElemBytes;
#endif

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

    psDGN->next_element_id = 0;
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

#ifdef DGN_DEBUG
    fprintf( fp, "  offset=%d  size=%d bytes\n", 
             psElement->offset, psElement->size );
#endif    

    fprintf( fp, 
             "  graphic_group:%-3d properties:%04x color:%d weight:%d style:%d\n", 
             psElement->graphic_group,
             psElement->properties,
             psElement->color,
             psElement->weight,
             psElement->style );

    switch( psElement->stype )
    {
      case DGNST_MULTIPOINT:
      {
          DGNElemMultiPoint	*psLine = (DGNElemMultiPoint *) psElement;
          int			i;
          
          for( i=0; i < psLine->num_vertices; i++ )
              fprintf( fp, "  (%.6f,%.6f,%.6f)\n", 
                       psLine->vertices[i].x, 
                       psLine->vertices[i].y, 
                       psLine->vertices[i].z );
      }
      break;

      case DGNST_ELLIPSE:
      {
          DGNElemEllipse	*psEllipse = (DGNElemEllipse *) psElement;

          fprintf( fp, 
                   "  origin=(%.5f,%.5f), rotation=%f\n"
                   "  axes=(%.5f,%.5f)\n", 
                   psEllipse->origin.x, 
                   psEllipse->origin.y, 
                   psEllipse->rotation,
                   psEllipse->primary_axis,
                   psEllipse->secondary_axis );
      }
      break;

      case DGNST_TEXT:
      {
          DGNElemText	*psText = (DGNElemText *) psElement;

          fprintf( fp, 
                   "  origin=(%.5f,%.5f), rotation=%f\n"
                   "  font=%d, just=%d, length_mult=%ld, height_mult=%ld\n"
                   "  string = \"%s\"\n",
                   psText->origin.x, 
                   psText->origin.y, 
                   psText->rotation,
                   psText->font_id,
                   psText->justification,
                   psText->length_mult,
                   psText->height_mult,
                   psText->string );
      }
      break;

      case DGNST_COLORTABLE:
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

      case DGNST_TCB:
      {
          DGNElemTCB *psTCB = (DGNElemTCB *) psElement;

          fprintf( fp, "  dimension = %d\n", psTCB->dimension );
          fprintf( fp, "  uor_per_subunit = %ld, subunits = `%s'\n",
                   psTCB->uor_per_subunit, psTCB->sub_units );
          fprintf( fp, "  subunits_per_master = %ld, master units = `%s'\n",
                   psTCB->subunits_per_master, psTCB->master_units );
          fprintf( fp, "  origin = (%.5f,%.5f,%.5f)\n", 
                   psTCB->origin_x,
                   psTCB->origin_y,
                   psTCB->origin_z );
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

      case DGNT_APPLICATION_ELEM:
        return "Application Element";
        
      default:
        sprintf( szNumericResult, "%d", nType );
        return szNumericResult;
    }
}

/************************************************************************/
/*                         DGNTransformPoint()                          */
/************************************************************************/

void DGNTransformPoint( DGNInfo *psDGN, DGNPoint *psPoint )

{
    psPoint->x = psPoint->x * psDGN->scale + psDGN->origin_x;
    psPoint->y = psPoint->y * psDGN->scale + psDGN->origin_y;
    psPoint->z = psPoint->z * psDGN->scale + psDGN->origin_z;
}

/************************************************************************/
/*                         DGNGetElementIndex()                         */
/************************************************************************/

const DGNElementInfo *DGNGetElementIndex( DGNHandle hDGN, int *pnElementCount )

{
    DGNInfo	*psDGN = (DGNInfo *) hDGN;

    DGNBuildIndex( psDGN );

    if( pnElementCount != NULL )
        *pnElementCount = psDGN->element_count;
    
    return psDGN->element_index;
}

/************************************************************************/
/*                           DGNBuildIndex()                            */
/************************************************************************/

void DGNBuildIndex( DGNInfo *psDGN )

{
    int	nMaxElements, nType, nLevel;
    long nLastOffset;

    if( psDGN->index_built ) 
        return;

    psDGN->index_built = TRUE;
    
    DGNRewind( psDGN );

    nMaxElements = 0;

    nLastOffset = VSIFTell( psDGN->fp );
    while( DGNLoadRawElement( psDGN, &nType, &nLevel ) )
    {
        DGNElementInfo	*psEI;

        if( psDGN->element_count == nMaxElements )
        {
            nMaxElements = (int) (nMaxElements * 1.5) + 500;
            
            psDGN->element_index = (DGNElementInfo *) 
                CPLRealloc( psDGN->element_index, 
                            nMaxElements * sizeof(DGNElementInfo) );
        }

        psEI = psDGN->element_index + psDGN->element_count;
        psEI->level = nLevel;
        psEI->type = nType;
        psEI->flags = 0;
        psEI->offset = nLastOffset;

        if( nType == DGNT_LINE || nType == DGNT_LINE_STRING
            || nType == DGNT_SHAPE || nType == DGNT_CURVE
            || nType == DGNT_BSPLINE )
            psEI->stype = DGNST_MULTIPOINT;

        else if( nType == DGNT_GROUP_DATA && nLevel == DGN_GDL_COLOR_TABLE )
            psEI->stype = DGNST_COLORTABLE;
        
        else if( nType == DGNT_ELLIPSE )
            psEI->stype = DGNST_ELLIPSE;
        
        else if( nType == DGNT_TEXT )
            psEI->stype = DGNST_TEXT;

        else if( nType == DGNT_TCB )
            psEI->stype = DGNST_TCB;

        else
            psEI->stype = DGNST_CORE;

        psDGN->element_count++;

        nLastOffset = VSIFTell( psDGN->fp );
    }

    DGNRewind( psDGN );
}
