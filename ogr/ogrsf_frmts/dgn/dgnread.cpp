/******************************************************************************
 * $Id$
 *
 * Project:  Microstation DGN Access Library
 * Purpose:  DGN Access Library element reading code.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Avenza Systems Inc, http://www.avenza.com/
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
 * Revision 1.10  2001/03/07 13:56:44  warmerda
 * updated copyright to be held by Avenza Systems
 *
 * Revision 1.9  2001/03/07 13:52:15  warmerda
 * Don't include deleted elements in the total extents.
 * Capture attribute data.
 *
 * Revision 1.7  2001/02/02 22:20:29  warmerda
 * compute text height/width properly
 *
 * Revision 1.6  2001/01/17 16:07:33  warmerda
 * ensure that TCB and ColorTable side effects occur on indexing pass too
 *
 * Revision 1.5  2001/01/16 18:12:52  warmerda
 * Added arc support, DGNLookupColor
 *
 * Revision 1.4  2001/01/10 16:13:45  warmerda
 * added docs and extents api
 *
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

static DGNElemCore *DGNParseTCB( DGNInfo * );
static DGNElemCore *DGNParseColorTable( DGNInfo * );

/************************************************************************/
/*                           DGNGotoElement()                           */
/************************************************************************/

/**
 * Seek to indicated element.
 *
 * Changes what element will be read on the next call to DGNReadElement(). 
 * Note that this function requires and index, and one will be built if
 * not already available.
 *
 * @param hDGN the file to affect.
 * @param element_id the element to seek to.  These values are sequentially
 * ordered starting at zero for the first element.
 * 
 * @return returns TRUE on success or FALSE on failure. 
 */

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

/**
 * Read a DGN element.
 *
 * This function will return the next element in the file, starting with the
 * first.  It is affected by DGNGotoElement() calls. 
 *
 * The element is read into a structure which includes the DGNElemCore 
 * structure.  It is expected that applications will inspect the stype
 * field of the returned DGNElemCore and use it to cast the pointer to the
 * appropriate element structure type such as DGNElemMultiPoint. 
 *
 * @param hDGN the handle of the file to read from.
 *
 * @return pointer to element structure, or NULL on EOF or processing error.
 * The structure should be freed with DGNFreeElement() when no longer needed.
 */

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
            psElement = DGNParseColorTable( psDGN );
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
          DGNElemArc *psEllipse;

          psEllipse = (DGNElemArc *) CPLCalloc(sizeof(DGNElemArc),1);
          psElement = (DGNElemCore *) psEllipse;
          psElement->stype = DGNST_ARC;
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

          psEllipse->startang = 0.0;
          psEllipse->sweepang = 360.0;
      }
      break;

      case DGNT_ARC:
      {
          DGNElemArc *psEllipse;
          GInt32     nSweepVal;

          psEllipse = (DGNElemArc *) CPLCalloc(sizeof(DGNElemArc),1);
          psElement = (DGNElemCore *) psEllipse;
          psElement->stype = DGNST_ARC;
          DGNParseCore( psDGN, psElement );

          psEllipse->startang = DGN_INT32( psDGN->abyElem + 36 );
          psEllipse->startang = psEllipse->startang / 360000.0;
#ifdef notdef
          nSweepVal = DGN_INT32( psDGN->abyElem + 40 );
          if( nSweepVal & 0x80000000 ) 
              psEllipse->sweepang = - (nSweepVal & 0x7fffffff)/360000.0;
          else if( nSweepVal  == 0 )
              psEllipse->sweepang = 360.0;
          else
              psEllipse->sweepang = nSweepVal / 360000.0;
#else
          if( psDGN->abyElem[41] & 0x80 )
          {
              psDGN->abyElem[41] &= 0x7f;
              nSweepVal = -1 * DGN_INT32( psDGN->abyElem + 40 );
          }
          else
              nSweepVal = DGN_INT32( psDGN->abyElem + 40 );

          if( nSweepVal == 0 )
              psEllipse->sweepang = 360.0;
          else
              psEllipse->sweepang = nSweepVal / 360000.0;
#endif
          
          memcpy( &(psEllipse->primary_axis), psDGN->abyElem + 44, 8 );
          DGN2IEEEDouble( &(psEllipse->primary_axis) );
          psEllipse->primary_axis *= psDGN->scale;

          memcpy( &(psEllipse->secondary_axis), psDGN->abyElem + 52, 8 );
          DGN2IEEEDouble( &(psEllipse->secondary_axis) );
          psEllipse->secondary_axis *= psDGN->scale;
          
          psEllipse->rotation = DGN_INT32( psDGN->abyElem + 60 );
          psEllipse->rotation = psEllipse->rotation / 360000.0;
          
          memcpy( &(psEllipse->origin.x), psDGN->abyElem + 64, 8 );
          DGN2IEEEDouble( &(psEllipse->origin.x) );

          memcpy( &(psEllipse->origin.y), psDGN->abyElem + 72, 8 );
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
          psText->length_mult = (DGN_INT32( psDGN->abyElem + 38 ))
              * psDGN->scale * 6.0 / 1000.0;
          psText->height_mult = (DGN_INT32( psDGN->abyElem + 42 ))
              * psDGN->scale * 6.0 / 1000.0;

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
        psElement = DGNParseTCB( psDGN );
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
    psElement->deleted = psData[1] & 0x80;
    psElement->type = psData[1] & 0x7f;

    if( psDGN->nElemBytes >= 36 )
    {
        psElement->graphic_group = psData[28] + psData[29] * 256;
        psElement->properties = psData[32] + psData[33] * 256;
        psElement->style = psData[34] & 0x7;
        psElement->weight = (psData[34] & 0xf8) >> 3;
        psElement->color = psData[35];
    }

    if( psElement->properties & DGNPF_ATTRIBUTES )
    {
        int   nAttIndex;
        
        nAttIndex = psData[30] + psData[31] * 256;

        psElement->attr_bytes = psDGN->nElemBytes - nAttIndex*2 - 32;
        psElement->attr_data = (unsigned char *) 
            CPLMalloc(psElement->attr_bytes);
        memcpy( psElement->attr_data, psData + nAttIndex * 2 + 32,
                psElement->attr_bytes );
    }
    
    return TRUE;
}

/************************************************************************/
/*                         DGNParseColorTable()                         */
/************************************************************************/

static DGNElemCore *DGNParseColorTable( DGNInfo * psDGN )

{
    DGNElemCore *psElement;
    DGNElemColorTable  *psColorTable;
            
    psColorTable = (DGNElemColorTable *) 
        CPLCalloc(sizeof(DGNElemColorTable),1);
    psElement = (DGNElemCore *) psColorTable;
    psElement->stype = DGNST_COLORTABLE;

    DGNParseCore( psDGN, psElement );

    psColorTable->screen_flag = 
        psDGN->abyElem[36] + psDGN->abyElem[37] * 256;

    memcpy( psColorTable->color_info, psDGN->abyElem+41, 768 );	
    if( !psDGN->got_color_table )
    {
        memcpy( psDGN->color_table, psColorTable->color_info, 768 );
        psDGN->got_color_table = 1;
    }
    
    return psElement;
}

/************************************************************************/
/*                            DGNParseTCB()                             */
/************************************************************************/

static DGNElemCore *DGNParseTCB( DGNInfo * psDGN )

{
    DGNElemTCB *psTCB;
    DGNElemCore *psElement;

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

    return psElement;
}

/************************************************************************/
/*                           DGNFreeElement()                           */
/************************************************************************/

/**
 * Free an element structure.
 *
 * This function will deallocate all resources associated with any element
 * structure returned by DGNReadElement(). 
 *
 * @param hDGN handle to file from which the element was read.
 * @param psElement the element structure returned by DGNReadElement().
 */

void DGNFreeElement( DGNHandle hDGN, DGNElemCore *psElement )

{
    if( psElement->attr_data != NULL )
        VSIFree( psElement->attr_data );
    CPLFree( psElement );
}

/************************************************************************/
/*                             DGNRewind()                              */
/************************************************************************/

/**
 * Rewind element reading.
 *
 * Rewind the indicated DGN file, so the next element read with 
 * DGNReadElement() will be the first.  Does not require indexing like
 * the more general DGNReadElement() function.
 *
 * @param hDGN handle to file.
 */

void DGNRewind( DGNHandle hDGN )

{
    DGNInfo	*psDGN = (DGNInfo *) hDGN;

    VSIRewind( psDGN->fp );

    psDGN->next_element_id = 0;
}


/************************************************************************/
/*                           DGNDumpElement()                           */
/************************************************************************/

/**
 * Emit textual report of an element.
 *
 * This function exists primarily for debugging, and will produce a textual
 * report about any element type to the designated file. 
 *
 * @param hDGN the file from which the element originated.
 * @param psElement the element to report on.
 * @param fp the file (such as stdout) to report the element information to.
 */

void DGNDumpElement( DGNHandle hDGN, DGNElemCore *psElement, FILE *fp )

{
    fprintf( fp, "\n" );
    fprintf( fp, "Element:%-12s Level:%2d id:%-6d ",
             DGNTypeToName( psElement->type ),
             psElement->level, 
             psElement->element_id );

    if( psElement->complex )
        fprintf( fp, "(Complex) " );

    if( psElement->deleted )
        fprintf( fp, "(DELETED) " );

    fprintf( fp, "\n" );

#ifdef DGN_DEBUG
    fprintf( fp, "  offset=%d  size=%d bytes\n", 
             psElement->offset, psElement->size );
#endif    

    fprintf( fp, 
             "  graphic_group:%-3d color:%d weight:%d style:%d\n", 
             psElement->graphic_group,
             psElement->color,
             psElement->weight,
             psElement->style );

    if( psElement->properties != 0 )
    {
        int	nClass;

        fprintf( fp, "  properties=%d", psElement->properties );
        if( psElement->properties & DGNPF_HOLE )
            fprintf( fp, ",HOLE" );
        if( psElement->properties & DGNPF_SNAPPABLE )
            fprintf( fp, ",SNAPPABLE" );
        if( psElement->properties & DGNPF_PLANAR )
            fprintf( fp, ",PLANAR" );
        if( psElement->properties & DGNPF_ORIENTATION )
            fprintf( fp, ",ORIENTATION" );
        if( psElement->properties & DGNPF_ATTRIBUTES )
            fprintf( fp, ",ATTRIBUTES" );
        if( psElement->properties & DGNPF_MODIFIED )
            fprintf( fp, ",MODIFIED" );
        if( psElement->properties & DGNPF_NEW )
            fprintf( fp, ",NEW" );
        if( psElement->properties & DGNPF_LOCKED )
            fprintf( fp, ",LOCKED" );

        nClass = psElement->properties & DGNPF_CLASS;
        if( nClass == DGNC_PATTERN_COMPONENT )
            fprintf( fp, ",PATTERN_COMPONENT" );
        else if( nClass == DGNC_CONSTRUCTION_ELEMENT )
            fprintf( fp, ",CONSTRUCTION ELEMENT" );
        else if( nClass == DGNC_DIMENSION_ELEMENT )
            fprintf( fp, ",DIMENSION ELEMENT" );
        else if( nClass == DGNC_PRIMARY_RULE_ELEMENT )
            fprintf( fp, ",PRIMARY RULE ELEMENT" );
        else if( nClass == DGNC_LINEAR_PATTERNED_ELEMENT )
            fprintf( fp, ",LINEAR PATTERNED ELEMENT" );
        else if( nClass == DGNC_CONSTRUCTION_RULE_ELEMENT )
            fprintf( fp, ",CONSTRUCTION_RULE_ELEMENT" );
            
        fprintf( fp, "\n" );
    }

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

      case DGNST_ARC:
      {
          DGNElemArc	*psArc = (DGNElemArc *) psElement;

          fprintf( fp, 
                   "  origin=(%.5f,%.5f), rotation=%f\n"
                   "  axes=(%.5f,%.5f), start angle=%f, sweep=%f\n", 
                   psArc->origin.x, 
                   psArc->origin.y, 
                   psArc->rotation,
                   psArc->primary_axis,
                   psArc->secondary_axis,
                   psArc->startang,
                   psArc->sweepang );                   
      }
      break;

      case DGNST_TEXT:
      {
          DGNElemText	*psText = (DGNElemText *) psElement;

          fprintf( fp, 
                   "  origin=(%.5f,%.5f), rotation=%f\n"
                   "  font=%d, just=%d, length_mult=%g, height_mult=%g\n"
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

    if( psElement->attr_bytes > 0 )
    {
        int       i;

        fprintf( fp, "Attributes (%d bytes):\n", psElement->attr_bytes );
        for( i = 0; i < psElement->attr_bytes; i++ )
        {
            if( (i%32) == 0 && i != 0 )
                fprintf( fp, "\n" );
            fprintf( fp, "%02x", psElement->attr_data[i] );
        }
        fprintf( fp, "\n" );
    }
}

/************************************************************************/
/*                           DGNTypeToName()                            */
/************************************************************************/

/**
 * Convert type to name.
 *
 * Returns a human readable name for an element type such as DGNT_LINE.
 *
 * @param nType the DGNT_* type code to translate.
 *
 * @return a pointer to an internal string with the translation.  This string
 * should not be modified or freed.
 */

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

/**
 * Fetch element index.
 *
 * This function will return an array with brief information about every
 * element in a DGN file.  It requires one pass through the entire file to
 * generate (this is not repeated on subsequent calls). 
 *
 * The returned array of DGNElementInfo structures contain the level, type, 
 * stype, and other flags for each element in the file.  This can facilitate
 * application level code representing the number of elements of various types
 * effeciently. 
 *
 * Note that while building the index requires one pass through the whole file,
 * it does not generally request much processing for each element. 
 *
 * @param hDGN the file to get an index for.
 * @param pnElementCount the integer to put the total element count into. 
 *
 * @return a pointer to an internal array of DGNElementInfo structures (there 
 * will be *pnElementCount entries in the array), or NULL on failure.  The
 * returned array should not be modified or freed, and will last only as long
 * as the DGN file remains open. 
 */

const DGNElementInfo *DGNGetElementIndex( DGNHandle hDGN, int *pnElementCount )

{
    DGNInfo	*psDGN = (DGNInfo *) hDGN;

    DGNBuildIndex( psDGN );

    if( pnElementCount != NULL )
        *pnElementCount = psDGN->element_count;
    
    return psDGN->element_index;
}

/************************************************************************/
/*                           DGNGetExtents()                            */
/************************************************************************/

/**
 * Fetch overall file extents.
 *
 * The extents are collected for each element while building an index, so
 * if an index has not already been built, it will be built when 
 * DGNGetExtents() is called.  
 * 
 * The Z min/max values are generally meaningless (0 and 0xffffffff in uor
 * space). 
 * 
 * @param hDGN the file to get extents for.
 * @param padfExtents pointer to an array of six doubles into which are loaded
 * the values xmin, ymin, zmin, xmax, ymax, and zmax.
 *
 * @return TRUE on success or FALSE on failure.
 */

int DGNGetExtents( DGNHandle hDGN, double * padfExtents )

{
    DGNInfo	*psDGN = (DGNInfo *) hDGN;
    DGNPoint	sMin, sMax;

    DGNBuildIndex( psDGN );

    if( !psDGN->got_bounds )
        return FALSE;

    sMin.x = psDGN->min_x - 2147483648.0;
    sMin.y = psDGN->min_y - 2147483648.0;
    sMin.z = psDGN->min_z - 2147483648.0;
    
    DGNTransformPoint( psDGN, &sMin );

    padfExtents[0] = sMin.x;
    padfExtents[1] = sMin.y;
    padfExtents[2] = sMin.z;
    
    sMax.x = psDGN->max_x - 2147483648.0;
    sMax.y = psDGN->max_y - 2147483648.0;
    sMax.z = psDGN->max_z - 2147483648.0;

    DGNTransformPoint( psDGN, &sMax );

    padfExtents[3] = sMax.x;
    padfExtents[4] = sMax.y;
    padfExtents[5] = sMax.z;

    return TRUE;
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

        if( psDGN->abyElem[1] & 0x80 )
            psEI->flags |= DGNEIF_DELETED;

        if( nType == DGNT_LINE || nType == DGNT_LINE_STRING
            || nType == DGNT_SHAPE || nType == DGNT_CURVE
            || nType == DGNT_BSPLINE )
            psEI->stype = DGNST_MULTIPOINT;

        else if( nType == DGNT_GROUP_DATA && nLevel == DGN_GDL_COLOR_TABLE )
        {
            DGNElemCore	*psCT = DGNParseColorTable( psDGN );
            DGNFreeElement( (DGNHandle) psDGN, psCT );
            psEI->stype = DGNST_COLORTABLE;
        }
        else if( nType == DGNT_ELLIPSE || nType == DGNT_ARC )
            psEI->stype = DGNST_ARC;
        
        else if( nType == DGNT_TEXT )
            psEI->stype = DGNST_TEXT;

        else if( nType == DGNT_TCB )
        {
            DGNElemCore	*psTCB = DGNParseTCB( psDGN );
            DGNFreeElement( (DGNHandle) psDGN, psTCB );
            psEI->stype = DGNST_TCB;
            
        }
        else
            psEI->stype = DGNST_CORE;

        if( (psEI->stype == DGNST_MULTIPOINT 
             || psEI->stype == DGNST_ARC
             || psEI->stype == DGNST_TEXT)
            && !(psEI->flags & DGNEIF_DELETED) )
        {
            GUInt32	anRegion[6];

            anRegion[0] = DGN_INT32( psDGN->abyElem + 4 );
            anRegion[1] = DGN_INT32( psDGN->abyElem + 8 );
            anRegion[2] = DGN_INT32( psDGN->abyElem + 12 );
            anRegion[3] = DGN_INT32( psDGN->abyElem + 16 );
            anRegion[4] = DGN_INT32( psDGN->abyElem + 20 );
            anRegion[5] = DGN_INT32( psDGN->abyElem + 24 );
#ifdef notdef
            printf( "panRegion[%d]=%.1f,%.1f,%.1f,%.1f,%.1f,%.1f\n", 
                    psDGN->element_count,
                    anRegion[0] - 2147483648.0,
                    anRegion[1] - 2147483648.0,
                    anRegion[2] - 2147483648.0,
                    anRegion[3] - 2147483648.0,
                    anRegion[4] - 2147483648.0,
                    anRegion[5] - 2147483648.0 );
#endif            
            if( psDGN->got_bounds )
            {
                psDGN->min_x = MIN(psDGN->min_x, anRegion[0]);
                psDGN->min_y = MIN(psDGN->min_y, anRegion[1]);
                psDGN->min_z = MIN(psDGN->min_z, anRegion[2]);
                psDGN->max_x = MAX(psDGN->max_x, anRegion[3]);
                psDGN->max_y = MAX(psDGN->max_y, anRegion[4]);
                psDGN->max_z = MAX(psDGN->max_z, anRegion[5]);
            }
            else
            {
                memcpy( &(psDGN->min_x), anRegion, sizeof(GInt32) * 6 );
                psDGN->got_bounds = TRUE;
            }
        }

        psDGN->element_count++;

        nLastOffset = VSIFTell( psDGN->fp );
    }

    DGNRewind( psDGN );
}

/************************************************************************/
/*                           DGNLookupColor()                           */
/************************************************************************/

/**
 * Translate color index into RGB values.
 *
 * @param hDGN the file.
 * @param color_index the color index to lookup.
 * @param red location to put red component.
 * @param green location to put green component.
 * @param blue location to put blue component.
 *
 * @return TRUE on success or FALSE on failure.  May fail if color_index is
 * out of range, or if a color table has not been read yet. 
 */

int DGNLookupColor( DGNHandle hDGN, int color_index, 
                    int * red, int * green, int * blue )

{
    DGNInfo	*psDGN = (DGNInfo *) hDGN;

    if( color_index < 0 || color_index > 255 || !psDGN->got_color_table )
        return FALSE;

    *red = psDGN->color_table[color_index][0];
    *green = psDGN->color_table[color_index][1];
    *blue = psDGN->color_table[color_index][2];

    return TRUE;
}
