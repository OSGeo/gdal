/******************************************************************************
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
 ****************************************************************************/

#include "dgnlibp.h"

#include <algorithm>

CPL_CVSID("$Id$")

static DGNElemCore *DGNParseTCB( DGNInfo * );
static DGNElemCore *DGNParseColorTable( DGNInfo * );
static DGNElemCore *DGNParseTagSet( DGNInfo * );


/************************************************************************/
/*                             DGN_INT16()                              */
/************************************************************************/

static short int DGN_INT16(const GByte *p)
{
    return static_cast<short>(p[0] | (p[1] << 8));
}

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
    DGNInfo     *psDGN = (DGNInfo *) hDGN;

    DGNBuildIndex( psDGN );

    if( element_id < 0 || element_id >= psDGN->element_count )
        return FALSE;

    if( VSIFSeekL( psDGN->fp, psDGN->element_index[element_id].offset,
                  SEEK_SET ) != 0 )
        return FALSE;

    psDGN->next_element_id = element_id;
    psDGN->in_complex_group = false;

    return TRUE;
}

/************************************************************************/
/*                         DGNLoadRawElement()                          */
/************************************************************************/

int DGNLoadRawElement( DGNInfo *psDGN, int *pnType, int *pnLevel )

{
/* -------------------------------------------------------------------- */
/*      Read the first four bytes to get the level, type, and word      */
/*      count.                                                          */
/* -------------------------------------------------------------------- */
    if( VSIFReadL( psDGN->abyElem, 1, 4, psDGN->fp ) != 4 )
        return FALSE;

    /* Is this an 0xFFFF endof file marker? */
    if( psDGN->abyElem[0] == 0xff && psDGN->abyElem[1] == 0xff )
        return FALSE;

    int nWords = psDGN->abyElem[2] + psDGN->abyElem[3]*256;
    int nType = psDGN->abyElem[1] & 0x7f;
    int nLevel = psDGN->abyElem[0] & 0x3f;

/* -------------------------------------------------------------------- */
/*      Read the rest of the element data into the working buffer.      */
/* -------------------------------------------------------------------- */
    if( nWords * 2 + 4 > (int) sizeof(psDGN->abyElem) )
        return FALSE;

    /* coverity[tainted_data] */
    if( (int) VSIFReadL( psDGN->abyElem + 4, 2, nWords, psDGN->fp ) != nWords )
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
/*                          DGNGetRawExtents()                          */
/*                                                                      */
/*      Returns false if the element type does not have recognizable    */
/*      element extents, other true and the extents will be updated.    */
/*                                                                      */
/*      It is assumed the raw element data has been loaded into the     */
/*      working area by DGNLoadRawElement().                            */
/************************************************************************/

static bool
DGNGetRawExtents( DGNInfo *psDGN, int nType, unsigned char *pabyRawData,
                  GUInt32 *pnXMin, GUInt32 *pnYMin, GUInt32 *pnZMin,
                  GUInt32 *pnXMax, GUInt32 *pnYMax, GUInt32 *pnZMax )

{
    if( pabyRawData == NULL )
        pabyRawData = psDGN->abyElem + 0;

    switch( nType )
    {
      case DGNT_LINE:
      case DGNT_LINE_STRING:
      case DGNT_SHAPE:
      case DGNT_CURVE:
      case DGNT_BSPLINE_POLE:
      case DGNT_BSPLINE_SURFACE_HEADER:
      case DGNT_BSPLINE_CURVE_HEADER:
      case DGNT_ELLIPSE:
      case DGNT_ARC:
      case DGNT_TEXT:
      case DGNT_TEXT_NODE:
      case DGNT_COMPLEX_CHAIN_HEADER:
      case DGNT_COMPLEX_SHAPE_HEADER:
      case DGNT_CONE:
      case DGNT_3DSURFACE_HEADER:
      case DGNT_3DSOLID_HEADER:
        *pnXMin = DGN_INT32( pabyRawData + 4 );
        *pnYMin = DGN_INT32( pabyRawData + 8 );
        if( pnZMin != NULL )
            *pnZMin = DGN_INT32( pabyRawData + 12 );

        *pnXMax = DGN_INT32( pabyRawData + 16 );
        *pnYMax = DGN_INT32( pabyRawData + 20 );
        if( pnZMax != NULL )
            *pnZMax = DGN_INT32( pabyRawData + 24 );
        return true;

      default:
        return false;
    }
}

/************************************************************************/
/*                        DGNGetElementExtents()                        */
/************************************************************************/

/**
 * Fetch extents of an element.
 *
 * This function will return the extents of the passed element if possible.
 * The extents are extracted from the element header if it contains them,
 * and transformed into master georeferenced format.  Some element types
 * do not have extents at all and will fail.
 *
 * This call will also fail if the extents raw data for the element is not
 * available.  This will occur if it was not the most recently read element,
 * and if the raw_data field is not loaded.
 *
 * @param hDGN the handle of the file to read from.
 *
 * @param psElement the element to extract extents from.
 *
 * @param psMin structure loaded with X, Y and Z minimum values for the
 * extent.
 *
 * @param psMax structure loaded with X, Y and Z maximum values for the
 * extent.
 *
 * @return TRUE on success of FALSE if extracting extents fails.
 */

int DGNGetElementExtents( DGNHandle hDGN, DGNElemCore *psElement,
                          DGNPoint *psMin, DGNPoint *psMax )

{
    DGNInfo *psDGN = (DGNInfo *) hDGN;
    bool bResult = false;

    GUInt32 anMin[3] = { 0, 0, 0 };
    GUInt32 anMax[3] = { 0, 0, 0 };

/* -------------------------------------------------------------------- */
/*      Get the extents if we have raw data in the element, or          */
/*      loaded in the file buffer.                                      */
/* -------------------------------------------------------------------- */
    if( psElement->raw_data != NULL )
        bResult = DGNGetRawExtents( psDGN, psElement->type,
                                    psElement->raw_data,
                                    anMin + 0, anMin + 1, anMin + 2,
                                    anMax + 0, anMax + 1, anMax + 2 );
    else if( psElement->element_id == psDGN->next_element_id - 1 )
        bResult = DGNGetRawExtents( psDGN, psElement->type,
                                    psDGN->abyElem + 0,
                                    anMin + 0, anMin + 1, anMin + 2,
                                    anMax + 0, anMax + 1, anMax + 2 );
    else
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "DGNGetElementExtents() fails because the requested element "
                 "does not have raw data available." );
        return FALSE;
    }

    if( !bResult )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Transform to user coordinate system and return.  The offset     */
/*      is to convert from "binary offset" form to twos complement.     */
/* -------------------------------------------------------------------- */
    psMin->x = anMin[0] - 2147483648.0;
    psMin->y = anMin[1] - 2147483648.0;
    psMin->z = anMin[2] - 2147483648.0;

    psMax->x = anMax[0] - 2147483648.0;
    psMax->y = anMax[1] - 2147483648.0;
    psMax->z = anMax[2] - 2147483648.0;

    DGNTransformPoint( psDGN, psMin );
    DGNTransformPoint( psDGN, psMax );

    return TRUE;
}

/************************************************************************/
/*                         DGNProcessElement()                          */
/*                                                                      */
/*      Assumes the raw element data has already been loaded, and       */
/*      tries to convert it into an element structure.                  */
/************************************************************************/

static DGNElemCore *DGNProcessElement( DGNInfo *psDGN, int nType, int nLevel )

{
    DGNElemCore *psElement = NULL;

/* -------------------------------------------------------------------- */
/*      Handle based on element type.                                   */
/* -------------------------------------------------------------------- */
    switch( nType )
    {
      case DGNT_CELL_HEADER:
      {
          DGNElemCellHeader *psCell = static_cast<DGNElemCellHeader *>(
              CPLCalloc(sizeof(DGNElemCellHeader), 1));
          psElement = (DGNElemCore *) psCell;
          psElement->stype = DGNST_CELL_HEADER;
          DGNParseCore( psDGN, psElement );

          psCell->totlength = psDGN->abyElem[36] + psDGN->abyElem[37] * 256;

          DGNRad50ToAscii( psDGN->abyElem[38] + psDGN->abyElem[39] * 256,
                           psCell->name + 0 );
          DGNRad50ToAscii( psDGN->abyElem[40] + psDGN->abyElem[41] * 256,
                           psCell->name + 3 );

          psCell->cclass = psDGN->abyElem[42] + psDGN->abyElem[43] * 256;
          psCell->levels[0] = psDGN->abyElem[44] + psDGN->abyElem[45] * 256;
          psCell->levels[1] = psDGN->abyElem[46] + psDGN->abyElem[47] * 256;
          psCell->levels[2] = psDGN->abyElem[48] + psDGN->abyElem[49] * 256;
          psCell->levels[3] = psDGN->abyElem[50] + psDGN->abyElem[51] * 256;

          if( psDGN->dimension == 2 )
          {
              psCell->rnglow.x = DGN_INT32( psDGN->abyElem + 52 );
              psCell->rnglow.y = DGN_INT32( psDGN->abyElem + 56 );
              psCell->rnghigh.x = DGN_INT32( psDGN->abyElem + 60 );
              psCell->rnghigh.y = DGN_INT32( psDGN->abyElem + 64 );

              psCell->trans[0] =
                1.0 * DGN_INT32( psDGN->abyElem + 68 ) / (1<<31);
              psCell->trans[1] =
                1.0 * DGN_INT32( psDGN->abyElem + 72 ) / (1<<31);
              psCell->trans[2] =
                1.0 * DGN_INT32( psDGN->abyElem + 76 ) / (1<<31);
              psCell->trans[3] =
                1.0 * DGN_INT32( psDGN->abyElem + 80 ) / (1<<31);

              psCell->origin.x = DGN_INT32( psDGN->abyElem + 84 );
              psCell->origin.y = DGN_INT32( psDGN->abyElem + 88 );

              {
              const double a = DGN_INT32( psDGN->abyElem + 68 );
              const double b = DGN_INT32( psDGN->abyElem + 72 );
              const double c = DGN_INT32( psDGN->abyElem + 76 );
              const double d = DGN_INT32( psDGN->abyElem + 80 );
              const double a2 = a * a;
              const double c2 = c * c;

              psCell->xscale = sqrt(a2 + c2) / 214748;
              psCell->yscale = sqrt(b*b + d*d) / 214748;
              if( (a2 + c2) <= 0.0 )
                  psCell->rotation = 0.0;
              else
                  psCell->rotation = acos(a / sqrt(a2 + c2));

              if (b <= 0)
                  psCell->rotation = psCell->rotation * 180 / M_PI;
              else
                  psCell->rotation = 360 - psCell->rotation * 180 / M_PI;
              }
          }
          else
          {
              psCell->rnglow.x = DGN_INT32( psDGN->abyElem + 52 );
              psCell->rnglow.y = DGN_INT32( psDGN->abyElem + 56 );
              psCell->rnglow.z = DGN_INT32( psDGN->abyElem + 60 );
              psCell->rnghigh.x = DGN_INT32( psDGN->abyElem + 64 );
              psCell->rnghigh.y = DGN_INT32( psDGN->abyElem + 68 );
              psCell->rnghigh.z = DGN_INT32( psDGN->abyElem + 72 );

              psCell->trans[0] =
                1.0 * DGN_INT32( psDGN->abyElem + 76 ) / (1<<31);
              psCell->trans[1] =
                1.0 * DGN_INT32( psDGN->abyElem + 80 ) / (1<<31);
              psCell->trans[2] =
                1.0 * DGN_INT32( psDGN->abyElem + 84 ) / (1<<31);
              psCell->trans[3] =
                1.0 * DGN_INT32( psDGN->abyElem + 88 ) / (1<<31);
              psCell->trans[4] =
                1.0 * DGN_INT32( psDGN->abyElem + 92 ) / (1<<31);
              psCell->trans[5] =
                1.0 * DGN_INT32( psDGN->abyElem + 96 ) / (1<<31);
              psCell->trans[6] =
                1.0 * DGN_INT32( psDGN->abyElem + 100 ) / (1<<31);
              psCell->trans[7] =
                1.0 * DGN_INT32( psDGN->abyElem + 104 ) / (1<<31);
              psCell->trans[8] =
                1.0 * DGN_INT32( psDGN->abyElem + 108 ) / (1<<31);

              psCell->origin.x = DGN_INT32( psDGN->abyElem + 112 );
              psCell->origin.y = DGN_INT32( psDGN->abyElem + 116 );
              psCell->origin.z = DGN_INT32( psDGN->abyElem + 120 );
          }

          DGNTransformPoint( psDGN, &(psCell->rnglow) );
          DGNTransformPoint( psDGN, &(psCell->rnghigh) );
          DGNTransformPoint( psDGN, &(psCell->origin) );
      }
      break;

      case DGNT_CELL_LIBRARY:
      {
          DGNElemCellLibrary *psCell = static_cast<DGNElemCellLibrary *>(
              CPLCalloc(sizeof(DGNElemCellLibrary), 1));
          psElement = (DGNElemCore *) psCell;
          psElement->stype = DGNST_CELL_LIBRARY;
          DGNParseCore( psDGN, psElement );

          DGNRad50ToAscii( psDGN->abyElem[32] + psDGN->abyElem[33] * 256,
                           psCell->name + 0 );
          DGNRad50ToAscii( psDGN->abyElem[34] + psDGN->abyElem[35] * 256,
                           psCell->name + 3 );

          psElement->properties = psDGN->abyElem[38]
              + psDGN->abyElem[39] * 256;

          psCell->dispsymb = psDGN->abyElem[40] + psDGN->abyElem[41] * 256;

          psCell->cclass = psDGN->abyElem[42] + psDGN->abyElem[43] * 256;
          psCell->levels[0] = psDGN->abyElem[44] + psDGN->abyElem[45] * 256;
          psCell->levels[1] = psDGN->abyElem[46] + psDGN->abyElem[47] * 256;
          psCell->levels[2] = psDGN->abyElem[48] + psDGN->abyElem[49] * 256;
          psCell->levels[3] = psDGN->abyElem[50] + psDGN->abyElem[51] * 256;

          psCell->numwords = psDGN->abyElem[36] + psDGN->abyElem[37] * 256;

          memset( psCell->description, 0, sizeof(psCell->description) );

          for( int iWord = 0; iWord < 9; iWord++ )
          {
              int iOffset = 52 + iWord * 2;

              DGNRad50ToAscii( psDGN->abyElem[iOffset]
                               + psDGN->abyElem[iOffset+1] * 256,
                               psCell->description + iWord * 3 );
          }
      }
      break;

      case DGNT_LINE:
      {
          DGNElemMultiPoint *psLine = static_cast<DGNElemMultiPoint *>(
              CPLCalloc(sizeof(DGNElemMultiPoint) + sizeof(DGNPoint), 1));
          psElement = (DGNElemCore *) psLine;
          psElement->stype = DGNST_MULTIPOINT;
          DGNParseCore( psDGN, psElement );

          int deltaLength = 0, deltaStart = 0;
          if (psLine->core.properties & DGNPF_ATTRIBUTES)
          {
            for (int iAttr = 0; iAttr<psLine->core.attr_bytes - 3; iAttr++)
            {
                if (psLine->core.attr_data[iAttr] == 0xA9 &&
                    psLine->core.attr_data[iAttr + 1] == 0x51)
                {
                    deltaLength = (psLine->core.attr_data[iAttr + 2] +
                                   psLine->core.attr_data[iAttr + 3] * 256) * 2;
                    deltaStart = iAttr + 6;
                    break;
                }
            }
          }

          psLine->num_vertices = 2;
          if( psDGN->dimension == 2 )
          {
              psLine->vertices[0].x = DGN_INT32( psDGN->abyElem + 36 );
              psLine->vertices[0].y = DGN_INT32( psDGN->abyElem + 40 );
              psLine->vertices[1].x = DGN_INT32( psDGN->abyElem + 44 );
              psLine->vertices[1].y = DGN_INT32( psDGN->abyElem + 48 );
          }
          else
          {
              psLine->vertices[0].x = DGN_INT32( psDGN->abyElem + 36 );
              psLine->vertices[0].y = DGN_INT32( psDGN->abyElem + 40 );
              psLine->vertices[0].z = DGN_INT32( psDGN->abyElem + 44 );
              psLine->vertices[1].x = DGN_INT32( psDGN->abyElem + 48 );
              psLine->vertices[1].y = DGN_INT32( psDGN->abyElem + 52 );
              psLine->vertices[1].z = DGN_INT32( psDGN->abyElem + 56 );
          }

          if (deltaStart && deltaLength &&
              deltaStart + 1 * 4 + 2 + 2 <= psLine->core.attr_bytes)
          {
              for (int i=0; i<2; i++)
              {
                 int dx = DGN_INT16(psLine->core.attr_data + deltaStart + i * 4);
                 int dy = DGN_INT16(psLine->core.attr_data + deltaStart + i * 4 + 2);
                 psLine->vertices[i].x += dx / 32767.0;
                 psLine->vertices[i].y += dy / 32767.0;
              }
          }

          DGNTransformPoint( psDGN, psLine->vertices + 0 );
          DGNTransformPoint( psDGN, psLine->vertices + 1 );
      }
      break;

      case DGNT_LINE_STRING:
      case DGNT_SHAPE:
      case DGNT_CURVE:
      case DGNT_BSPLINE_POLE:
      {
          int pntsize = psDGN->dimension * 4;

          int count = psDGN->abyElem[36] + psDGN->abyElem[37]*256;
          if( count < 2 )
          {
              CPLError(CE_Failure, CPLE_AssertionFailed, "count < 2");
              return NULL;
          }
          DGNElemMultiPoint *psLine = static_cast<DGNElemMultiPoint *>(
              CPLCalloc(sizeof(DGNElemMultiPoint)+(count-1)*sizeof(DGNPoint),
                        1));
          psElement = (DGNElemCore *) psLine;
          psElement->stype = DGNST_MULTIPOINT;
          DGNParseCore( psDGN, psElement );

          if( psDGN->nElemBytes < 38 + count * pntsize )
          {
              int new_count = (psDGN->nElemBytes - 38) / pntsize;
              if( new_count < 0 )
              {
                  CPLError(CE_Failure, CPLE_AssertionFailed, "new_count < 2");
                  DGNFreeElement(psDGN, psElement);
                  return NULL;
              }
              CPLError( CE_Warning, CPLE_AppDefined,
                        "Trimming multipoint vertices to %d from %d because\n"
                        "element is short.\n",
                        new_count,
                        count );
              count = new_count;
          }
          int deltaLength=0,deltaStart=0;
          if (psLine->core.properties & DGNPF_ATTRIBUTES)
          {
              for (int iAttr=0; iAttr<psLine->core.attr_bytes-3; iAttr++)
              {
                    if (psLine->core.attr_data[iAttr] == 0xA9 &&
                        psLine->core.attr_data[iAttr+1] == 0x51)
                    {
                        deltaLength = (psLine->core.attr_data[iAttr + 2] +
                            psLine->core.attr_data[iAttr + 3] * 256) * 2;
                        deltaStart = iAttr + 6;
                        break;
                    }
              }
          }
          for( int i = 0; i < count &&
                          (( psDGN->dimension == 3 ) ? 46 : 42) +
                                i*pntsize + 4 <= psDGN->nElemBytes; i++ )
          {
              psLine->vertices[i].x =
                  DGN_INT32( psDGN->abyElem + 38 + i*pntsize );
              psLine->vertices[i].y =
                  DGN_INT32( psDGN->abyElem + 42 + i*pntsize );
              if( psDGN->dimension == 3 )
                  psLine->vertices[i].z =
                      DGN_INT32( psDGN->abyElem + 46 + i*pntsize );
              if (deltaStart && deltaLength &&
                  deltaStart + i * 4 + 2 + 2 <= psLine->core.attr_bytes)
              {
                int dx = DGN_INT16(psLine->core.attr_data + deltaStart + i * 4);
                int dy = DGN_INT16(psLine->core.attr_data + deltaStart + i * 4 + 2);
                psLine->vertices[i].x += dx / 32767.0;
                psLine->vertices[i].y += dy / 32767.0;
              }
              DGNTransformPoint( psDGN, psLine->vertices + i );
              psLine->num_vertices = i+1;
          }
      }
      break;

      case DGNT_TEXT_NODE:
      {
          DGNElemTextNode *psNode = static_cast<DGNElemTextNode *>(
              CPLCalloc(sizeof(DGNElemTextNode), 1));
          psElement = (DGNElemCore *) psNode;
          psElement->stype = DGNST_TEXT_NODE;
          DGNParseCore( psDGN, psElement );

          psNode->totlength = psDGN->abyElem[36] + psDGN->abyElem[37] * 256;
          psNode->numelems  = psDGN->abyElem[38] + psDGN->abyElem[39] * 256;

          psNode->node_number   = psDGN->abyElem[40] + psDGN->abyElem[41] * 256;
          psNode->max_length    = psDGN->abyElem[42];
          psNode->max_used      = psDGN->abyElem[43];
          psNode->font_id       = psDGN->abyElem[44];
          psNode->justification = psDGN->abyElem[45];
          psNode->length_mult = (DGN_INT32( psDGN->abyElem + 50 ))
              * psDGN->scale * 6.0 / 1000.0;
          psNode->height_mult = (DGN_INT32( psDGN->abyElem + 54 ))
              * psDGN->scale * 6.0 / 1000.0;

          if( psDGN->dimension == 2 )
          {
              psNode->rotation = DGN_INT32( psDGN->abyElem + 58 ) / 360000.0;

              psNode->origin.x = DGN_INT32( psDGN->abyElem + 62 );
              psNode->origin.y = DGN_INT32( psDGN->abyElem + 66 );
          }
          else
          {
              /* leave quaternion for later */

              psNode->origin.x = DGN_INT32( psDGN->abyElem + 74 );
              psNode->origin.y = DGN_INT32( psDGN->abyElem + 78 );
              psNode->origin.z = DGN_INT32( psDGN->abyElem + 82 );
          }
          DGNTransformPoint( psDGN, &(psNode->origin) );
      }
      break;

      case DGNT_GROUP_DATA:
        if( nLevel == DGN_GDL_COLOR_TABLE )
        {
            psElement = DGNParseColorTable( psDGN );
        }
        else
        {
            psElement = static_cast<DGNElemCore *>(
                CPLCalloc(sizeof(DGNElemCore), 1));
            psElement->stype = DGNST_CORE;
            DGNParseCore( psDGN, psElement );
        }
        break;

      case DGNT_ELLIPSE:
      {
          DGNElemArc *psEllipse =
              static_cast<DGNElemArc *>(CPLCalloc(sizeof(DGNElemArc), 1));
          psElement = (DGNElemCore *) psEllipse;
          psElement->stype = DGNST_ARC;
          DGNParseCore( psDGN, psElement );

          memcpy( &(psEllipse->primary_axis), psDGN->abyElem + 36, 8 );
          DGN2IEEEDouble( &(psEllipse->primary_axis) );
          psEllipse->primary_axis *= psDGN->scale;

          memcpy( &(psEllipse->secondary_axis), psDGN->abyElem + 44, 8 );
          DGN2IEEEDouble( &(psEllipse->secondary_axis) );
          psEllipse->secondary_axis *= psDGN->scale;

          if( psDGN->dimension == 2 )
          {
              psEllipse->rotation = DGN_INT32( psDGN->abyElem + 52 );
              psEllipse->rotation = psEllipse->rotation / 360000.0;

              memcpy( &(psEllipse->origin.x), psDGN->abyElem + 56, 8 );
              DGN2IEEEDouble( &(psEllipse->origin.x) );

              memcpy( &(psEllipse->origin.y), psDGN->abyElem + 64, 8 );
              DGN2IEEEDouble( &(psEllipse->origin.y) );
          }
          else
          {
              /* leave quaternion for later */

              memcpy( &(psEllipse->origin.x), psDGN->abyElem + 68, 8 );
              DGN2IEEEDouble( &(psEllipse->origin.x) );

              memcpy( &(psEllipse->origin.y), psDGN->abyElem + 76, 8 );
              DGN2IEEEDouble( &(psEllipse->origin.y) );

              memcpy( &(psEllipse->origin.z), psDGN->abyElem + 84, 8 );
              DGN2IEEEDouble( &(psEllipse->origin.z) );

              psEllipse->quat[0] = DGN_INT32( psDGN->abyElem + 52 );
              psEllipse->quat[1] = DGN_INT32( psDGN->abyElem + 56 );
              psEllipse->quat[2] = DGN_INT32( psDGN->abyElem + 60 );
              psEllipse->quat[3] = DGN_INT32( psDGN->abyElem + 64 );
          }

          DGNTransformPoint( psDGN, &(psEllipse->origin) );

          psEllipse->startang = 0.0;
          psEllipse->sweepang = 360.0;
      }
      break;

      case DGNT_ARC:
      {
          GInt32 nSweepVal = 0;

          DGNElemArc *psEllipse =
              static_cast<DGNElemArc *>(CPLCalloc(sizeof(DGNElemArc), 1));
          psElement = (DGNElemCore *) psEllipse;
          psElement->stype = DGNST_ARC;
          DGNParseCore( psDGN, psElement );

          psEllipse->startang = DGN_INT32( psDGN->abyElem + 36 );
          psEllipse->startang = psEllipse->startang / 360000.0;
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

          memcpy( &(psEllipse->primary_axis), psDGN->abyElem + 44, 8 );
          DGN2IEEEDouble( &(psEllipse->primary_axis) );
          psEllipse->primary_axis *= psDGN->scale;

          memcpy( &(psEllipse->secondary_axis), psDGN->abyElem + 52, 8 );
          DGN2IEEEDouble( &(psEllipse->secondary_axis) );
          psEllipse->secondary_axis *= psDGN->scale;

          if( psDGN->dimension == 2 )
          {
              psEllipse->rotation = DGN_INT32( psDGN->abyElem + 60 );
              psEllipse->rotation = psEllipse->rotation / 360000.0;

              memcpy( &(psEllipse->origin.x), psDGN->abyElem + 64, 8 );
              DGN2IEEEDouble( &(psEllipse->origin.x) );

              memcpy( &(psEllipse->origin.y), psDGN->abyElem + 72, 8 );
              DGN2IEEEDouble( &(psEllipse->origin.y) );
          }
          else
          {
              /* for now we don't try to handle quaternion */
              psEllipse->rotation = 0;

              memcpy( &(psEllipse->origin.x), psDGN->abyElem + 76, 8 );
              DGN2IEEEDouble( &(psEllipse->origin.x) );

              memcpy( &(psEllipse->origin.y), psDGN->abyElem + 84, 8 );
              DGN2IEEEDouble( &(psEllipse->origin.y) );

              memcpy( &(psEllipse->origin.z), psDGN->abyElem + 92, 8 );
              DGN2IEEEDouble( &(psEllipse->origin.z) );

              psEllipse->quat[0] = DGN_INT32( psDGN->abyElem + 60 );
              psEllipse->quat[1] = DGN_INT32( psDGN->abyElem + 64 );
              psEllipse->quat[2] = DGN_INT32( psDGN->abyElem + 68 );
              psEllipse->quat[3] = DGN_INT32( psDGN->abyElem + 72 );
          }

          DGNTransformPoint( psDGN, &(psEllipse->origin) );
      }
      break;

      case DGNT_TEXT:
      {
          int num_chars = 0;
          int text_off = 0;

          if( psDGN->dimension == 2 )
              num_chars = psDGN->abyElem[58];
          else
              num_chars = psDGN->abyElem[74];

          DGNElemText *psText = static_cast<DGNElemText *>(
              CPLCalloc(sizeof(DGNElemText)+num_chars, 1));
          psElement = (DGNElemCore *) psText;
          psElement->stype = DGNST_TEXT;
          DGNParseCore( psDGN, psElement );

          psText->font_id = psDGN->abyElem[36];
          psText->justification = psDGN->abyElem[37];
          psText->length_mult = (DGN_INT32( psDGN->abyElem + 38 ))
              * psDGN->scale * 6.0 / 1000.0;
          psText->height_mult = (DGN_INT32( psDGN->abyElem + 42 ))
              * psDGN->scale * 6.0 / 1000.0;

          if( psDGN->dimension == 2 )
          {
              psText->rotation = DGN_INT32( psDGN->abyElem + 46 );
              psText->rotation = psText->rotation / 360000.0;

              psText->origin.x = DGN_INT32( psDGN->abyElem + 50 );
              psText->origin.y = DGN_INT32( psDGN->abyElem + 54 );
              text_off = 60;
          }
          else
          {
              /* leave quaternion for later */

              psText->origin.x = DGN_INT32( psDGN->abyElem + 62 );
              psText->origin.y = DGN_INT32( psDGN->abyElem + 66 );
              psText->origin.z = DGN_INT32( psDGN->abyElem + 70 );
              text_off = 76;
          }

          DGNTransformPoint( psDGN, &(psText->origin) );

          /* experimental multibyte support from Ason Kang (hiska@netian.com)*/
          if (*(psDGN->abyElem + text_off) == 0xFF
              && *(psDGN->abyElem + text_off + 1) == 0xFD)
          {
              int n=0;
              for( int i = 0; i < num_chars/2 - 1; i++ )
              {
                  unsigned short w = 0;
                  memcpy(&w, psDGN->abyElem + text_off + 2 + i*2, 2);
                  CPL_LSBPTR16(&w);
                  if (w<256) { // if alpa-numeric code area : Normal character
                      *(psText->string + n) = (char) (w & 0xFF);
                      n++; // skip 1 byte;
                  }
                  else { // if extend code area : 2 byte Korean character
                      *(psText->string + n)     = (char) (w >> 8);   // hi
                      *(psText->string + n + 1) = (char) (w & 0xFF); // lo
                      n+=2; // 2 byte
                  }
              }
              psText->string[n] = '\0'; // terminate C string
          }
          else
          {
              memcpy( psText->string, psDGN->abyElem + text_off, num_chars );
              psText->string[num_chars] = '\0';
          }
      }
      break;

      case DGNT_TCB:
        psElement = DGNParseTCB( psDGN );
        break;

      case DGNT_COMPLEX_CHAIN_HEADER:
      case DGNT_COMPLEX_SHAPE_HEADER:
      {
          DGNElemComplexHeader *psHdr = static_cast<DGNElemComplexHeader *>(
              CPLCalloc(sizeof(DGNElemComplexHeader), 1));
          psElement = (DGNElemCore *) psHdr;
          psElement->stype = DGNST_COMPLEX_HEADER;
          DGNParseCore( psDGN, psElement );

          psHdr->totlength = psDGN->abyElem[36] + psDGN->abyElem[37] * 256;
          psHdr->numelems = psDGN->abyElem[38] + psDGN->abyElem[39] * 256;
      }
      break;

      case DGNT_TAG_VALUE:
      {
          DGNElemTagValue *psTag = static_cast<DGNElemTagValue *>(
              CPLCalloc(sizeof(DGNElemTagValue), 1));
          psElement = (DGNElemCore *) psTag;
          psElement->stype = DGNST_TAG_VALUE;
          DGNParseCore( psDGN, psElement );

          psTag->tagType = psDGN->abyElem[74] + psDGN->abyElem[75] * 256;
          memcpy( &(psTag->tagSet), psDGN->abyElem + 68, 4 );
          CPL_LSBPTR32( &(psTag->tagSet) );
          psTag->tagIndex = psDGN->abyElem[72] + psDGN->abyElem[73] * 256;
          psTag->tagLength = psDGN->abyElem[150] + psDGN->abyElem[151] * 256;

          if( psTag->tagType == 1 )
          {
              psTag->tagValue.string =
                  CPLStrdup( (char *) psDGN->abyElem + 154 );
          }
          else if( psTag->tagType == 3 )
          {
              memcpy( &(psTag->tagValue.integer),
                      psDGN->abyElem + 154, 4 );
              CPL_LSBPTR32( &(psTag->tagValue.integer) );
          }
          else if( psTag->tagType == 4 )
          {
              memcpy( &(psTag->tagValue.real),
                      psDGN->abyElem + 154, 8 );
              DGN2IEEEDouble( &(psTag->tagValue.real) );
          }
      }
      break;

      case DGNT_APPLICATION_ELEM:
        if( nLevel == 24 )
        {
            psElement = DGNParseTagSet( psDGN );
            if( psElement == NULL )
                return NULL;
        }
        else
        {
            psElement = static_cast<DGNElemCore *>(
                CPLCalloc(sizeof(DGNElemCore), 1));
            psElement->stype = DGNST_CORE;
            DGNParseCore( psDGN, psElement );
        }
        break;

      case DGNT_CONE:
        {
          if( psDGN->dimension != 3 )
          {
              CPLError(CE_Failure, CPLE_AssertionFailed, "psDGN->dimension != 3");
              return NULL;
          }

          DGNElemCone *psCone =
              static_cast<DGNElemCone *>(CPLCalloc(sizeof(DGNElemCone), 1));
          psElement = (DGNElemCore *) psCone;
          psElement->stype = DGNST_CONE;
          DGNParseCore( psDGN, psElement );

          psCone->unknown = psDGN->abyElem[36] + psDGN->abyElem[37] * 256;
          psCone->quat[0] = DGN_INT32( psDGN->abyElem + 38 );
          psCone->quat[1] = DGN_INT32( psDGN->abyElem + 42 );
          psCone->quat[2] = DGN_INT32( psDGN->abyElem + 46 );
          psCone->quat[3] = DGN_INT32( psDGN->abyElem + 50 );

          memcpy( &(psCone->center_1.x), psDGN->abyElem + 54, 8 );
          DGN2IEEEDouble( &(psCone->center_1.x) );
          memcpy( &(psCone->center_1.y), psDGN->abyElem + 62, 8 );
          DGN2IEEEDouble( &(psCone->center_1.y) );
          memcpy( &(psCone->center_1.z), psDGN->abyElem + 70, 8 );
          DGN2IEEEDouble( &(psCone->center_1.z) );
          memcpy( &(psCone->radius_1), psDGN->abyElem + 78, 8 );
          DGN2IEEEDouble( &(psCone->radius_1) );

          memcpy( &(psCone->center_2.x), psDGN->abyElem + 86, 8 );
          DGN2IEEEDouble( &(psCone->center_2.x) );
          memcpy( &(psCone->center_2.y), psDGN->abyElem + 94, 8 );
          DGN2IEEEDouble( &(psCone->center_2.y) );
          memcpy( &(psCone->center_2.z), psDGN->abyElem + 102, 8 );
          DGN2IEEEDouble( &(psCone->center_2.z) );
          memcpy( &(psCone->radius_2), psDGN->abyElem + 110, 8 );
          DGN2IEEEDouble( &(psCone->radius_2) );

          psCone->radius_1 *= psDGN->scale;
          psCone->radius_2 *= psDGN->scale;
          DGNTransformPoint( psDGN, &psCone->center_1 );
          DGNTransformPoint( psDGN, &psCone->center_2 );
        }
        break;

      case DGNT_3DSURFACE_HEADER:
      case DGNT_3DSOLID_HEADER:
        {
          DGNElemComplexHeader *psShape = static_cast<DGNElemComplexHeader *>(
              CPLCalloc(sizeof(DGNElemComplexHeader), 1));
          psElement = (DGNElemCore *) psShape;
          psElement->stype = DGNST_COMPLEX_HEADER;
          DGNParseCore( psDGN, psElement );

          // Read complex header
          psShape->totlength = psDGN->abyElem[36] + psDGN->abyElem[37] * 256;
          psShape->numelems = psDGN->abyElem[38] + psDGN->abyElem[39] * 256;
          psShape->surftype = psDGN->abyElem[40];
          psShape->boundelms = psDGN->abyElem[41] + 1;
        }
        break;
      case DGNT_BSPLINE_SURFACE_HEADER:
        {
          DGNElemBSplineSurfaceHeader *psSpline =
              static_cast<DGNElemBSplineSurfaceHeader *>(
                  CPLCalloc(sizeof(DGNElemBSplineSurfaceHeader), 1));
          psElement = (DGNElemCore *) psSpline;
          psElement->stype = DGNST_BSPLINE_SURFACE_HEADER;
          DGNParseCore( psDGN, psElement );

          // Read B-Spline surface header
          psSpline->desc_words = static_cast<long>(DGN_INT32(psDGN->abyElem + 36));
          psSpline->curve_type = psDGN->abyElem[41];

          // U
          psSpline->u_order = (psDGN->abyElem[40] & 0x0f) + 2;
          psSpline->u_properties = psDGN->abyElem[40] & 0xf0;
          psSpline->num_poles_u = psDGN->abyElem[42] + psDGN->abyElem[43]*256;
          psSpline->num_knots_u = psDGN->abyElem[44] + psDGN->abyElem[45]*256;
          psSpline->rule_lines_u = psDGN->abyElem[46] + psDGN->abyElem[47]*256;

          // V
          psSpline->v_order = (psDGN->abyElem[48] & 0x0f) + 2;
          psSpline->v_properties = psDGN->abyElem[48] & 0xf0;
          psSpline->num_poles_v = psDGN->abyElem[50] + psDGN->abyElem[51]*256;
          psSpline->num_knots_v = psDGN->abyElem[52] + psDGN->abyElem[53]*256;
          psSpline->rule_lines_v = psDGN->abyElem[54] + psDGN->abyElem[55]*256;

          psSpline->num_bounds = psDGN->abyElem[56] + psDGN->abyElem[57]*556;
        }
      break;
      case DGNT_BSPLINE_CURVE_HEADER:
        {
          DGNElemBSplineCurveHeader *psSpline =
              static_cast<DGNElemBSplineCurveHeader *>(
                  CPLCalloc(sizeof(DGNElemBSplineCurveHeader), 1));
          psElement = (DGNElemCore *) psSpline;
          psElement->stype = DGNST_BSPLINE_CURVE_HEADER;
          DGNParseCore( psDGN, psElement );

          // Read B-Spline curve header
          psSpline->desc_words = static_cast<long>(DGN_INT32(psDGN->abyElem + 36));

          // flags
          psSpline->order = (psDGN->abyElem[40] & 0x0f) + 2;
          psSpline->properties = psDGN->abyElem[40] & 0xf0;
          psSpline->curve_type = psDGN->abyElem[41];

          psSpline->num_poles = psDGN->abyElem[42] + psDGN->abyElem[43]*256;
          psSpline->num_knots = psDGN->abyElem[44] + psDGN->abyElem[45]*256;
        }
      break;
      case DGNT_BSPLINE_SURFACE_BOUNDARY:
        {
          short numverts = psDGN->abyElem[38] + psDGN->abyElem[39]*256;
          if( numverts <= 0 )
          {
              CPLError(CE_Failure, CPLE_AssertionFailed, "numverts <= 0");
              return NULL;
          }

          DGNElemBSplineSurfaceBoundary *psBounds =
              static_cast<DGNElemBSplineSurfaceBoundary *>(
                  CPLCalloc(sizeof(DGNElemBSplineSurfaceBoundary) +
                            (numverts-1)*sizeof(DGNPoint), 1));
          psElement = (DGNElemCore *) psBounds;
          psElement->stype = DGNST_BSPLINE_SURFACE_BOUNDARY;
          DGNParseCore( psDGN, psElement );

          int deltaLength=0,deltaStart=0;
          if (psBounds->core.properties & DGNPF_ATTRIBUTES)
          {
              for (int iAttr=0; iAttr<psBounds->core.attr_bytes-3; iAttr++)
              {
                    if (psBounds->core.attr_data[iAttr] == 0xA9 &&
                        psBounds->core.attr_data[iAttr+1] == 0x51)
                    {
                        deltaLength = (psBounds->core.attr_data[iAttr + 2] +
                            psBounds->core.attr_data[iAttr + 3] * 256) * 2;
                        deltaStart = iAttr + 6;
                        break;
                    }
              }
          }
          // Read B-Spline surface boundary
          psBounds->number = psDGN->abyElem[36] + psDGN->abyElem[37]*256;

          for (int i=0;i<numverts &&
                       44 + i * 8 + 4 <= psDGN->nElemBytes;i++) {
            psBounds->vertices[i].x = DGN_INT32( psDGN->abyElem + 40 + i*8 );
            psBounds->vertices[i].y = DGN_INT32( psDGN->abyElem + 44 + i*8 );
            psBounds->vertices[i].z = 0;
            if (deltaStart && deltaLength &&
                deltaStart + i * 4 + 2 + 2 <= psBounds->core.attr_bytes)
            {
                int dx = DGN_INT16(psBounds->core.attr_data + deltaStart + i * 4);
                int dy = DGN_INT16(psBounds->core.attr_data + deltaStart + i * 4 + 2);
                psBounds->vertices[i].x += dx / 32767.0;
                psBounds->vertices[i].y += dy / 32767.0;
            }
            psBounds->numverts = static_cast<short>(i+1);
          }
        }
      break;
      case DGNT_BSPLINE_KNOT:
      case DGNT_BSPLINE_WEIGHT_FACTOR:
        {
          // FIXME: Is it OK to assume that the # of elements corresponds
          // directly to the element size? kintel 20051215.
          int attr_bytes = psDGN->nElemBytes -
            (psDGN->abyElem[30] + psDGN->abyElem[31]*256)*2 - 32;
          int numelems = (psDGN->nElemBytes - 36 - attr_bytes)/4;

          DGNElemKnotWeight *psArray = static_cast<DGNElemKnotWeight *>(
              CPLCalloc(sizeof(DGNElemKnotWeight) + (numelems-1)*sizeof(float),
                        1));

          psElement = (DGNElemCore *) psArray;
          psElement->stype = DGNST_KNOT_WEIGHT;
          DGNParseCore( psDGN, psElement );

          // Read array
          for (int i=0;i<numelems;i++) {
            psArray->array[i] =
              static_cast<float>(1.0 * DGN_INT32(psDGN->abyElem + 36 + i*4) / ((1UL << 31) - 1));
          }
        }
      break;
      case DGNT_SHARED_CELL_DEFN:
      {
          DGNElemSharedCellDefn *psShared =
              static_cast<DGNElemSharedCellDefn *>(
                  CPLCalloc(sizeof(DGNElemSharedCellDefn), 1));
          psElement = (DGNElemCore *) psShared;
          psElement->stype = DGNST_SHARED_CELL_DEFN;
          DGNParseCore( psDGN, psElement );

          psShared->totlength = psDGN->abyElem[36] + psDGN->abyElem[37] * 256;
      }
      break;
      default:
      {
          psElement = static_cast<DGNElemCore *>(
              CPLCalloc(sizeof(DGNElemCore), 1));
          psElement->stype = DGNST_CORE;
          DGNParseCore( psDGN, psElement );
      }
      break;
    }

/* -------------------------------------------------------------------- */
/*      If the element structure type is "core" or if we are running    */
/*      in "capture all" mode, record the complete binary image of      */
/*      the element.                                                    */
/* -------------------------------------------------------------------- */
    if( psElement->stype == DGNST_CORE
        || (psDGN->options & DGNO_CAPTURE_RAW_DATA) )
    {
        psElement->raw_bytes = psDGN->nElemBytes;
        psElement->raw_data = static_cast<unsigned char *>(
            CPLMalloc(psElement->raw_bytes));

        memcpy( psElement->raw_data, psDGN->abyElem, psElement->raw_bytes );
    }

/* -------------------------------------------------------------------- */
/*      Collect some additional generic information.                    */
/* -------------------------------------------------------------------- */
    psElement->element_id = psDGN->next_element_id - 1;

    psElement->offset =
        static_cast<int>(VSIFTellL( psDGN->fp )) - psDGN->nElemBytes;
    psElement->size = psDGN->nElemBytes;

    return psElement;
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
    DGNInfo     *psDGN = (DGNInfo *) hDGN;
    int nType = 0;
    int nLevel = 0;
    bool bInsideFilter = false;

/* -------------------------------------------------------------------- */
/*      Load the element data into the current buffer.  If a spatial    */
/*      filter is in effect, loop until we get something within our     */
/*      spatial constraints.                                            */
/* -------------------------------------------------------------------- */
    do {
        bInsideFilter = true;

        if( !DGNLoadRawElement( psDGN, &nType, &nLevel ) )
            return NULL;

        if( psDGN->has_spatial_filter )
        {
            if( !psDGN->sf_converted_to_uor )
                DGNSpatialFilterToUOR( psDGN );

            GUInt32 nXMin = 0;
            GUInt32 nXMax = 0;
            GUInt32 nYMin = 0;
            GUInt32 nYMax = 0;
            if( !DGNGetRawExtents( psDGN, nType, NULL,
                                   &nXMin, &nYMin, NULL,
                                   &nXMax, &nYMax, NULL ) )
            {
                /* If we don't have spatial characteristics for the element
                   we will pass it through. */
                bInsideFilter = true;
            }
            else if( nXMin > psDGN->sf_max_x
                     || nYMin > psDGN->sf_max_y
                     || nXMax < psDGN->sf_min_x
                     || nYMax < psDGN->sf_min_y )
            {
                bInsideFilter = false;
            }

            /*
            ** We want to select complex elements based on the extents of
            ** the header, not the individual elements.
            */
            if( nType == DGNT_COMPLEX_CHAIN_HEADER
                || nType == DGNT_COMPLEX_SHAPE_HEADER )
            {
                psDGN->in_complex_group = true;
                psDGN->select_complex_group = bInsideFilter;
            }
            else if( psDGN->abyElem[0] & 0x80 /* complex flag set */ )
            {
                if( psDGN->in_complex_group )
                    bInsideFilter = psDGN->select_complex_group;
            }
            else
            {
                psDGN->in_complex_group = false;
            }
        }
    } while( !bInsideFilter );

/* -------------------------------------------------------------------- */
/*      Convert into an element structure.                              */
/* -------------------------------------------------------------------- */
    DGNElemCore *psElement = DGNProcessElement( psDGN, nType, nLevel );

    return psElement;
}

/************************************************************************/
/*                       DGNElemTypeHasDispHdr()                        */
/************************************************************************/

/**
 * Does element type have display header.
 *
 * @param nElemType element type (0-63) to test.
 *
 * @return TRUE if elements of passed in type have a display header after the
 * core element header, or FALSE otherwise.
 */

int DGNElemTypeHasDispHdr( int nElemType )

{
    switch( nElemType )
    {
      case 0:
      case DGNT_TCB:
      case DGNT_CELL_LIBRARY:
      case DGNT_LEVEL_SYMBOLOGY:
      case 32:
      case 44:
      case 48:
      case 49:
      case 50:
      case 51:
      case 57:
      case 60:
      case 61:
      case 62:
      case 63:
        return FALSE;

      default:
        return TRUE;
    }
}

/************************************************************************/
/*                            DGNParseCore()                            */
/************************************************************************/

int DGNParseCore( DGNInfo *psDGN, DGNElemCore *psElement )

{
    GByte       *psData = psDGN->abyElem+0;

    psElement->level = psData[0] & 0x3f;
    psElement->complex = psData[0] & 0x80;
    psElement->deleted = psData[1] & 0x80;
    psElement->type = psData[1] & 0x7f;

    if( psDGN->nElemBytes >= 36 && DGNElemTypeHasDispHdr( psElement->type ) )
    {
        psElement->graphic_group = psData[28] + psData[29] * 256;
        psElement->properties = psData[32] + psData[33] * 256;
        psElement->style = psData[34] & 0x7;
        psElement->weight = (psData[34] & 0xf8) >> 3;
        psElement->color = psData[35];
    }
    else
    {
        psElement->graphic_group = 0;
        psElement->properties = 0;
        psElement->style = 0;
        psElement->weight = 0;
        psElement->color = 0;
    }

    if( psElement->properties & DGNPF_ATTRIBUTES )
    {
        const int nAttIndex = psData[30] + psData[31] * 256;

        psElement->attr_bytes = psDGN->nElemBytes - nAttIndex*2 - 32;
        if( psElement->attr_bytes > 0 )
        {
            psElement->attr_data = static_cast<unsigned char *>(
                CPLMalloc(psElement->attr_bytes));
            memcpy( psElement->attr_data, psData + nAttIndex * 2 + 32,
                    psElement->attr_bytes );
        }
        else
        {
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "Computed %d bytes for attribute info on element,\n"
                "perhaps this element type doesn't really have a disphdr?",
                psElement->attr_bytes );
            psElement->attr_bytes = 0;
        }
    }

    return TRUE;
}

/************************************************************************/
/*                         DGNParseColorTable()                         */
/************************************************************************/

static DGNElemCore *DGNParseColorTable( DGNInfo * psDGN )

{
    DGNElemColorTable *psColorTable = static_cast<DGNElemColorTable *>(
        CPLCalloc(sizeof(DGNElemColorTable), 1));
    DGNElemCore *psElement = (DGNElemCore *) psColorTable;
    psElement->stype = DGNST_COLORTABLE;

    DGNParseCore( psDGN, psElement );

    psColorTable->screen_flag =
        psDGN->abyElem[36] + psDGN->abyElem[37] * 256;

    memcpy( psColorTable->color_info[255], psDGN->abyElem+38, 3 );
    memcpy( psColorTable->color_info, psDGN->abyElem+41, 765 );

    // We used to only install a color table as the default color
    // table if it was the first in the file.  But apparently we should
    // really be using the last one.  This doesn't necessarily accomplish
    // that either if the elements are being read out of order but it will
    // usually do better at least.
    memcpy( psDGN->color_table, psColorTable->color_info, 768 );
    psDGN->got_color_table = 1;

    return psElement;
}

/************************************************************************/
/*                           DGNParseTagSet()                           */
/************************************************************************/

static DGNElemCore *DGNParseTagSet( DGNInfo * psDGN )

{
    DGNElemTagSet *psTagSet =
        static_cast<DGNElemTagSet *>(CPLCalloc(sizeof(DGNElemTagSet), 1));
    DGNElemCore *psElement = (DGNElemCore *) psTagSet;
    psElement->stype = DGNST_TAG_SET;

    DGNParseCore( psDGN, psElement );

/* -------------------------------------------------------------------- */
/*      Parse the overall information.                                  */
/* -------------------------------------------------------------------- */
    psTagSet->tagCount =
        psDGN->abyElem[44] + psDGN->abyElem[45] * 256;
    psTagSet->flags =
        psDGN->abyElem[46] + psDGN->abyElem[47] * 256;
    psTagSet->tagSetName = CPLStrdup( (const char *) (psDGN->abyElem + 48) );

/* -------------------------------------------------------------------- */
/*      Get the tag set number out of the attributes, if available.     */
/* -------------------------------------------------------------------- */
    psTagSet->tagSet = -1;

    if( psElement->attr_bytes >= 8
        && psElement->attr_data[0] == 0x03
        && psElement->attr_data[1] == 0x10
        && psElement->attr_data[2] == 0x2f
        && psElement->attr_data[3] == 0x7d )
        psTagSet->tagSet = psElement->attr_data[4]
            + psElement->attr_data[5] * 256;

/* -------------------------------------------------------------------- */
/*      Parse each of the tag definitions.                              */
/* -------------------------------------------------------------------- */
    psTagSet->tagList = static_cast<DGNTagDef *>(
        CPLCalloc(sizeof(DGNTagDef), psTagSet->tagCount));

    size_t nDataOffset = 48 + strlen(psTagSet->tagSetName) + 1 + 1;

    for( int iTag = 0; iTag < psTagSet->tagCount; iTag++ )
    {
        DGNTagDef *tagDef = psTagSet->tagList + iTag;

        if( nDataOffset >= static_cast<size_t>(psDGN->nElemBytes) )
        {
            CPLError(CE_Failure, CPLE_AssertionFailed,
                     "nDataOffset >= static_cast<size_t>(psDGN->nElemBytes)");
            DGNFreeElement(psDGN, psElement);
            return NULL;
        }

        /* collect tag name. */
        tagDef->name = CPLStrdup( (char *) psDGN->abyElem + nDataOffset );
        nDataOffset += strlen(tagDef->name)+1;

        /* Get tag id */
        tagDef->id = psDGN->abyElem[nDataOffset]
            + psDGN->abyElem[nDataOffset+1] * 256;
        nDataOffset += 2;

        /* Get User Prompt */
        tagDef->prompt = CPLStrdup( (char *) psDGN->abyElem + nDataOffset );
        nDataOffset += strlen(tagDef->prompt)+1;

        /* Get type */
        tagDef->type = psDGN->abyElem[nDataOffset]
            + psDGN->abyElem[nDataOffset+1] * 256;
        nDataOffset += 2;

        /* skip five zeros */
        nDataOffset += 5;

        /* Get the default */
        if( tagDef->type == 1 )
        {
            tagDef->defaultValue.string =
                CPLStrdup( (char *) psDGN->abyElem + nDataOffset );
            nDataOffset += strlen(tagDef->defaultValue.string)+1;
        }
        else if( tagDef->type == 3 || tagDef->type == 5 )
        {
            memcpy( &(tagDef->defaultValue.integer),
                    psDGN->abyElem + nDataOffset, 4 );
            CPL_LSBPTR32( &(tagDef->defaultValue.integer) );
            nDataOffset += 4;
        }
        else if( tagDef->type == 4 )
        {
            memcpy( &(tagDef->defaultValue.real),
                    psDGN->abyElem + nDataOffset, 8 );
            DGN2IEEEDouble( &(tagDef->defaultValue.real) );
            nDataOffset += 8;
        }
        else
            nDataOffset += 4;
    }
    return psElement;
}

/************************************************************************/
/*                            DGNParseTCB()                             */
/************************************************************************/

static DGNElemCore *DGNParseTCB( DGNInfo * psDGN )

{
    DGNElemTCB *psTCB = static_cast<DGNElemTCB *>(
        CPLCalloc(sizeof(DGNElemTCB), 1));
    DGNElemCore *psElement = (DGNElemCore *) psTCB;
    psElement->stype = DGNST_TCB;
    DGNParseCore( psDGN, psElement );

    if( psDGN->abyElem[1214] & 0x40 )
        psTCB->dimension = 3;
    else
        psTCB->dimension = 2;

    psTCB->subunits_per_master = static_cast<long>(DGN_INT32( psDGN->abyElem + 1112 ));

    psTCB->master_units[0] = (char) psDGN->abyElem[1120];
    psTCB->master_units[1] = (char) psDGN->abyElem[1121];
    psTCB->master_units[2] = '\0';

    psTCB->uor_per_subunit = static_cast<long>(DGN_INT32( psDGN->abyElem + 1116 ));

    psTCB->sub_units[0] = (char) psDGN->abyElem[1122];
    psTCB->sub_units[1] = (char) psDGN->abyElem[1123];
    psTCB->sub_units[2] = '\0';

    /* Get global origin */
    memcpy( &(psTCB->origin_x), psDGN->abyElem+1240, 8 );
    memcpy( &(psTCB->origin_y), psDGN->abyElem+1248, 8 );
    memcpy( &(psTCB->origin_z), psDGN->abyElem+1256, 8 );

    /* Transform to IEEE */
    DGN2IEEEDouble( &(psTCB->origin_x) );
    DGN2IEEEDouble( &(psTCB->origin_y) );
    DGN2IEEEDouble( &(psTCB->origin_z) );

    /* Convert from UORs to master units. */
    if( psTCB->uor_per_subunit != 0
        && psTCB->subunits_per_master != 0 )
    {
        psTCB->origin_x = psTCB->origin_x /
            (psTCB->uor_per_subunit * psTCB->subunits_per_master);
        psTCB->origin_y = psTCB->origin_y /
            (psTCB->uor_per_subunit * psTCB->subunits_per_master);
        psTCB->origin_z = psTCB->origin_z /
            (psTCB->uor_per_subunit * psTCB->subunits_per_master);
    }

    if( !psDGN->got_tcb )
    {
        psDGN->got_tcb = true;
        psDGN->dimension = psTCB->dimension;
        psDGN->origin_x = psTCB->origin_x;
        psDGN->origin_y = psTCB->origin_y;
        psDGN->origin_z = psTCB->origin_z;

        if( psTCB->uor_per_subunit != 0
            && psTCB->subunits_per_master != 0 )
            psDGN->scale = 1.0
                / (psTCB->uor_per_subunit * psTCB->subunits_per_master);
    }

    /* Collect views */
    for( int iView = 0; iView < 8; iView++ )
    {
        unsigned char *pabyRawView = psDGN->abyElem + 46 + iView*118;
        DGNViewInfo *psView = psTCB->views + iView;

        psView->flags = pabyRawView[0] + pabyRawView[1] * 256;
        memcpy( psView->levels, pabyRawView + 2, 8 );

        psView->origin.x = DGN_INT32( pabyRawView + 10 );
        psView->origin.y = DGN_INT32( pabyRawView + 14 );
        psView->origin.z = DGN_INT32( pabyRawView + 18 );

        DGNTransformPoint( psDGN, &(psView->origin) );

        psView->delta.x = DGN_INT32( pabyRawView + 22 );
        psView->delta.y = DGN_INT32( pabyRawView + 26 );
        psView->delta.z = DGN_INT32( pabyRawView + 30 );

        psView->delta.x *= psDGN->scale;
        psView->delta.y *= psDGN->scale;
        psView->delta.z *= psDGN->scale;

        memcpy( psView->transmatrx, pabyRawView + 34, sizeof(double) * 9 );
        for( int i = 0; i < 9; i++ )
            DGN2IEEEDouble( psView->transmatrx + i );

        memcpy( &(psView->conversion), pabyRawView + 106, sizeof(double) );
        DGN2IEEEDouble( &(psView->conversion) );

        psView->activez = static_cast<unsigned long>(DGN_INT32( pabyRawView + 114 ));
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

void DGNFreeElement( CPL_UNUSED DGNHandle hDGN, DGNElemCore *psElement )
{
    if( psElement->attr_data != NULL )
        VSIFree( psElement->attr_data );

    if( psElement->raw_data != NULL )
        VSIFree( psElement->raw_data );

    if( psElement->stype == DGNST_TAG_SET )
    {
        DGNElemTagSet *psTagSet = (DGNElemTagSet *) psElement;
        CPLFree( psTagSet->tagSetName );

        for( int iTag = 0; iTag < psTagSet->tagCount; iTag++ )
        {
            CPLFree( psTagSet->tagList[iTag].name );
            CPLFree( psTagSet->tagList[iTag].prompt );

            if( psTagSet->tagList[iTag].type == 1 )
                CPLFree( psTagSet->tagList[iTag].defaultValue.string );
        }
        CPLFree( psTagSet->tagList );
    }
    else if( psElement->stype == DGNST_TAG_VALUE )
    {
        if( ((DGNElemTagValue *) psElement)->tagType == 1 )
            CPLFree( ((DGNElemTagValue *) psElement)->tagValue.string );
    }

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
    DGNInfo *psDGN = (DGNInfo *) hDGN;

    VSIRewindL( psDGN->fp );

    psDGN->next_element_id = 0;
    psDGN->in_complex_group = false;
}

/************************************************************************/
/*                         DGNTransformPoint()                          */
/************************************************************************/

void DGNTransformPoint( DGNInfo *psDGN, DGNPoint *psPoint )

{
    psPoint->x = psPoint->x * psDGN->scale - psDGN->origin_x;
    psPoint->y = psPoint->y * psDGN->scale - psDGN->origin_y;
    psPoint->z = psPoint->z * psDGN->scale - psDGN->origin_z;
}

/************************************************************************/
/*                      DGNInverseTransformPoint()                      */
/************************************************************************/

void DGNInverseTransformPoint( DGNInfo *psDGN, DGNPoint *psPoint )

{
    psPoint->x = (psPoint->x + psDGN->origin_x) / psDGN->scale;
    psPoint->y = (psPoint->y + psDGN->origin_y) / psDGN->scale;
    psPoint->z = (psPoint->z + psDGN->origin_z) / psDGN->scale;

    psPoint->x = std::max(-2147483647.0, std::min(2147483647.0, psPoint->x));
    psPoint->y = std::max(-2147483647.0, std::min(2147483647.0, psPoint->y));
    psPoint->z = std::max(-2147483647.0, std::min(2147483647.0, psPoint->z));
}

/************************************************************************/
/*                   DGNInverseTransformPointToInt()                    */
/************************************************************************/

void DGNInverseTransformPointToInt( DGNInfo *psDGN, DGNPoint *psPoint,
                                    unsigned char *pabyTarget )

{
    double adfCT[3] = {
        (psPoint->x + psDGN->origin_x) / psDGN->scale,
        (psPoint->y + psDGN->origin_y) / psDGN->scale,
        (psPoint->z + psDGN->origin_z) / psDGN->scale
    };

    const int nIter = std::min(3, psDGN->dimension);
    for( int i = 0; i < nIter; i++ )
    {
        GInt32 nCTI = static_cast<GInt32>(
            std::max(-2147483647.0, std::min(2147483647.0, adfCT[i])));
        unsigned char *pabyCTI = (unsigned char *) &nCTI;

#ifdef WORDS_BIGENDIAN
        pabyTarget[i*4+0] = pabyCTI[1];
        pabyTarget[i*4+1] = pabyCTI[0];
        pabyTarget[i*4+2] = pabyCTI[3];
        pabyTarget[i*4+3] = pabyCTI[2];
#else
        pabyTarget[i*4+3] = pabyCTI[1];
        pabyTarget[i*4+2] = pabyCTI[0];
        pabyTarget[i*4+1] = pabyCTI[3];
        pabyTarget[i*4+0] = pabyCTI[2];
#endif
    }
}

/************************************************************************/
/*                             DGNLoadTCB()                             */
/************************************************************************/

/**
 * Load TCB if not already loaded.
 *
 * This function will load the TCB element if it is not already loaded.
 * It is used primarily to ensure the TCB is loaded before doing any operations
 * that require TCB values (like creating new elements).
 *
 * @return FALSE on failure or TRUE on success.
 */

int DGNLoadTCB( DGNHandle hDGN )

{
    DGNInfo     *psDGN = (DGNInfo *) hDGN;

    if( psDGN->got_tcb )
        return TRUE;

    while( !psDGN->got_tcb )
    {
        DGNElemCore *psElem = DGNReadElement( hDGN );
        if( psElem == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "DGNLoadTCB() - unable to find TCB in file." );
            return FALSE;
        }
        DGNFreeElement( hDGN, psElem );
    }

    return TRUE;
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
 * efficiently.
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
    DGNInfo     *psDGN = (DGNInfo *) hDGN;

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
    DGNInfo *psDGN = (DGNInfo *) hDGN;

    DGNBuildIndex( psDGN );

    if( !psDGN->got_bounds )
        return FALSE;

    DGNPoint sMin = {
        psDGN->min_x - 2147483648.0,
        psDGN->min_y - 2147483648.0,
        psDGN->min_z - 2147483648.0
    };

    DGNTransformPoint( psDGN, &sMin );

    padfExtents[0] = sMin.x;
    padfExtents[1] = sMin.y;
    padfExtents[2] = sMin.z;

    DGNPoint sMax = {
        psDGN->max_x - 2147483648.0,
        psDGN->max_y - 2147483648.0,
        psDGN->max_z - 2147483648.0
    };

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
    if( psDGN->index_built )
        return;

    int nType = 0;
    int nLevel = 0;
    GUInt32 anRegion[6] = {};

    psDGN->index_built = true;

    DGNRewind( psDGN );

    int nMaxElements = 0;

    vsi_l_offset nLastOffset = VSIFTellL( psDGN->fp );
    while( DGNLoadRawElement( psDGN, &nType, &nLevel ) )
    {
        if( psDGN->element_count == nMaxElements )
        {
            nMaxElements = (int) (nMaxElements * 1.5) + 500;

            psDGN->element_index = (DGNElementInfo *)
                CPLRealloc( psDGN->element_index,
                            nMaxElements * sizeof(DGNElementInfo) );
        }

        DGNElementInfo *psEI = psDGN->element_index + psDGN->element_count;
        psEI->level = (unsigned char) nLevel;
        psEI->type = (unsigned char) nType;
        psEI->flags = 0;
        psEI->offset = nLastOffset;

        if( psDGN->abyElem[0] & 0x80 )
            psEI->flags |= DGNEIF_COMPLEX;

        if( psDGN->abyElem[1] & 0x80 )
            psEI->flags |= DGNEIF_DELETED;

        if( nType == DGNT_LINE || nType == DGNT_LINE_STRING
            || nType == DGNT_SHAPE || nType == DGNT_CURVE
            || nType == DGNT_BSPLINE_POLE )
            psEI->stype = DGNST_MULTIPOINT;

        else if( nType == DGNT_GROUP_DATA && nLevel == DGN_GDL_COLOR_TABLE )
        {
            DGNElemCore *psCT = DGNParseColorTable( psDGN );
            DGNFreeElement( (DGNHandle) psDGN, psCT );
            psEI->stype = DGNST_COLORTABLE;
        }
        else if( nType == DGNT_ELLIPSE || nType == DGNT_ARC )
            psEI->stype = DGNST_ARC;

        else if( nType == DGNT_COMPLEX_SHAPE_HEADER
                 || nType == DGNT_COMPLEX_CHAIN_HEADER
                 || nType == DGNT_3DSURFACE_HEADER
                 || nType == DGNT_3DSOLID_HEADER)
            psEI->stype = DGNST_COMPLEX_HEADER;

        else if( nType == DGNT_TEXT )
            psEI->stype = DGNST_TEXT;

        else if( nType == DGNT_TAG_VALUE )
            psEI->stype = DGNST_TAG_VALUE;

        else if( nType == DGNT_APPLICATION_ELEM )
        {
            if( nLevel == 24 )
                psEI->stype = DGNST_TAG_SET;
            else
                psEI->stype = DGNST_CORE;
        }
        else if( nType == DGNT_TCB )
        {
            DGNElemCore *psTCB = DGNParseTCB( psDGN );
            DGNFreeElement( (DGNHandle) psDGN, psTCB );
            psEI->stype = DGNST_TCB;
        }
        else if( nType == DGNT_CONE )
            psEI->stype = DGNST_CONE;
        else
            psEI->stype = DGNST_CORE;

        if( !(psEI->flags & DGNEIF_DELETED)
            && !(psEI->flags & DGNEIF_COMPLEX)
            && DGNGetRawExtents( psDGN, nType, NULL,
                                 anRegion+0, anRegion+1, anRegion+2,
                                 anRegion+3, anRegion+4, anRegion+5 ) )
        {
#ifdef notdef
            printf( "panRegion[%d]=%.1f,%.1f,%.1f,%.1f,%.1f,%.1f\n",/*ok*/
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
                psDGN->min_x = std::min(psDGN->min_x, anRegion[0]);
                psDGN->min_y = std::min(psDGN->min_y, anRegion[1]);
                psDGN->min_z = std::min(psDGN->min_z, anRegion[2]);
                psDGN->max_x = std::max(psDGN->max_x, anRegion[3]);
                psDGN->max_y = std::max(psDGN->max_y, anRegion[4]);
                psDGN->max_z = std::max(psDGN->max_z, anRegion[5]);
            }
            else
            {
                psDGN->min_x = anRegion[0];
                psDGN->min_y = anRegion[1];
                psDGN->min_z = anRegion[2];
                psDGN->max_x = anRegion[3];
                psDGN->max_y = anRegion[4];
                psDGN->max_z = anRegion[5];
                psDGN->got_bounds = true;
            }
        }

        psDGN->element_count++;

        nLastOffset = VSIFTellL( psDGN->fp );
    }

    DGNRewind( psDGN );

    psDGN->max_element_count = nMaxElements;
}
