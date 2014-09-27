/******************************************************************************
 * $Id$
 *
 * Project:  Microstation DGN Access Library
 * Purpose:  Application visible helper functions for parsing DGN information.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Avenza Systems Inc, http://www.avenza.com/
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

CPL_CVSID("$Id$");

static unsigned char abyDefaultPCT[256][3] = 
{
  {255,255,255},
  {0,0,255},
  {0,255,0},
  {255,0,0},
  {255,255,0},
  {255,0,255},
  {255,127,0},
  {0,255,255},
  {64,64,64},
  {192,192,192},
  {254,0,96},
  {160,224,0},
  {0,254,160},
  {128,0,160},
  {176,176,176},
  {0,240,240},
  {240,240,240},
  {0,0,240},
  {0,240,0},
  {240,0,0},
  {240,240,0},
  {240,0,240},
  {240,122,0},
  {0,240,240},
  {240,240,240},
  {0,0,240},
  {0,240,0},
  {240,0,0},
  {240,240,0},
  {240,0,240},
  {240,122,0},
  {0,225,225},
  {225,225,225},
  {0,0,225},
  {0,225,0},
  {225,0,0},
  {225,225,0},
  {225,0,225},
  {225,117,0},
  {0,225,225},
  {225,225,225},
  {0,0,225},
  {0,225,0},
  {225,0,0},
  {225,225,0},
  {225,0,225},
  {225,117,0},
  {0,210,210},
  {210,210,210},
  {0,0,210},
  {0,210,0},
  {210,0,0},
  {210,210,0},
  {210,0,210},
  {210,112,0},
  {0,210,210},
  {210,210,210},
  {0,0,210},
  {0,210,0},
  {210,0,0},
  {210,210,0},
  {210,0,210},
  {210,112,0},
  {0,195,195},
  {195,195,195},
  {0,0,195},
  {0,195,0},
  {195,0,0},
  {195,195,0},
  {195,0,195},
  {195,107,0},
  {0,195,195},
  {195,195,195},
  {0,0,195},
  {0,195,0},
  {195,0,0},
  {195,195,0},
  {195,0,195},
  {195,107,0},
  {0,180,180},
  {180,180,180},
  {0,0,180},
  {0,180,0},
  {180,0,0},
  {180,180,0},
  {180,0,180},
  {180,102,0},
  {0,180,180},
  {180,180,180},
  {0,0,180},
  {0,180,0},
  {180,0,0},
  {180,180,0},
  {180,0,180},
  {180,102,0},
  {0,165,165},
  {165,165,165},
  {0,0,165},
  {0,165,0},
  {165,0,0},
  {165,165,0},
  {165,0,165},
  {165,97,0},
  {0,165,165},
  {165,165,165},
  {0,0,165},
  {0,165,0},
  {165,0,0},
  {165,165,0},
  {165,0,165},
  {165,97,0},
  {0,150,150},
  {150,150,150},
  {0,0,150},
  {0,150,0},
  {150,0,0},
  {150,150,0},
  {150,0,150},
  {150,92,0},
  {0,150,150},
  {150,150,150},
  {0,0,150},
  {0,150,0},
  {150,0,0},
  {150,150,0},
  {150,0,150},
  {150,92,0},
  {0,135,135},
  {135,135,135},
  {0,0,135},
  {0,135,0},
  {135,0,0},
  {135,135,0},
  {135,0,135},
  {135,87,0},
  {0,135,135},
  {135,135,135},
  {0,0,135},
  {0,135,0},
  {135,0,0},
  {135,135,0},
  {135,0,135},
  {135,87,0},
  {0,120,120},
  {120,120,120},
  {0,0,120},
  {0,120,0},
  {120,0,0},
  {120,120,0},
  {120,0,120},
  {120,82,0},
  {0,120,120},
  {120,120,120},
  {0,0,120},
  {0,120,0},
  {120,0,0},
  {120,120,0},
  {120,0,120},
  {120,82,0},
  {0,105,105},
  {105,105,105},
  {0,0,105},
  {0,105,0},
  {105,0,0},
  {105,105,0},
  {105,0,105},
  {105,77,0},
  {0,105,105},
  {105,105,105},
  {0,0,105},
  {0,105,0},
  {105,0,0},
  {105,105,0},
  {105,0,105},
  {105,77,0},
  {0,90,90},
  {90,90,90},
  {0,0,90},
  {0,90,0},
  {90,0,0},
  {90,90,0},
  {90,0,90},
  {90,72,0},
  {0,90,90},
  {90,90,90},
  {0,0,90},
  {0,90,0},
  {90,0,0},
  {90,90,0},
  {90,0,90},
  {90,72,0},
  {0,75,75},
  {75,75,75},
  {0,0,75},
  {0,75,0},
  {75,0,0},
  {75,75,0},
  {75,0,75},
  {75,67,0},
  {0,75,75},
  {75,75,75},
  {0,0,75},
  {0,75,0},
  {75,0,0},
  {75,75,0},
  {75,0,75},
  {75,67,0},
  {0,60,60},
  {60,60,60},
  {0,0,60},
  {0,60,0},
  {60,0,0},
  {60,60,0},
  {60,0,60},
  {60,62,0},
  {0,60,60},
  {60,60,60},
  {0,0,60},
  {0,60,0},
  {60,0,0},
  {60,60,0},
  {60,0,60},
  {60,62,0},
  {0,45,45},
  {45,45,45},
  {0,0,45},
  {0,45,0},
  {45,0,0},
  {45,45,0},
  {45,0,45},
  {45,57,0},
  {0,45,45},
  {45,45,45},
  {0,0,45},
  {0,45,0},
  {45,0,0},
  {45,45,0},
  {45,0,45},
  {45,57,0},
  {0,30,30},
  {30,30,30},
  {0,0,30},
  {0,30,0},
  {30,0,0},
  {30,30,0},
  {30,0,30},
  {30,52,0},
  {0,30,30},
  {30,30,30},
  {0,0,30},
  {0,30,0},
  {30,0,0},
  {30,30,0},
  {30,0,30},
  {192,192,192},
  {28,0,100}
};


/************************************************************************/
/*                           DGNLookupColor()                           */
/************************************************************************/

/**
 * Translate color index into RGB values.
 *
 * If no color table has yet been encountered in the file a hard-coded
 * "default" color table will be used.  This seems to be what Microstation
 * uses as a color table when there isn't one in a DGN file but I am not
 * absolutely convinced it is appropriate.
 *
 * @param hDGN the file.
 * @param color_index the color index to lookup.
 * @param red location to put red component.
 * @param green location to put green component.
 * @param blue location to put blue component.
 *
 * @return TRUE on success or FALSE on failure.  May fail if color_index is
 * out of range.
 */

int DGNLookupColor( DGNHandle hDGN, int color_index, 
                    int * red, int * green, int * blue )

{
    DGNInfo     *psDGN = (DGNInfo *) hDGN;

    if( color_index < 0 || color_index > 255  )
        return FALSE;

    if( !psDGN->got_color_table )
    {
        *red = abyDefaultPCT[color_index][0];
        *green = abyDefaultPCT[color_index][1];
        *blue = abyDefaultPCT[color_index][2];
    }
    else
    {
        *red = psDGN->color_table[color_index][0];
        *green = psDGN->color_table[color_index][1];
        *blue = psDGN->color_table[color_index][2];
    }

    return TRUE;
}

/************************************************************************/
/*                        DGNGetShapeFillInfo()                         */
/************************************************************************/

/**
 * Fetch fill color for a shape.
 *
 * This method will check for a 0x0041 user attribute linkaged with fill
 * color information for the element.  If found the function returns TRUE,
 * and places the fill color in *pnColor, otherwise FALSE is returned and
 * *pnColor is not updated.
 *
 * @param hDGN the file.
 * @param psElem the element.
 * @param pnColor the location to return the fill color. 
 *
 * @return TRUE on success or FALSE on failure.
 */

int DGNGetShapeFillInfo( DGNHandle hDGN, DGNElemCore *psElem, int *pnColor )

{
    int iLink;
    
    for( iLink = 0; TRUE; iLink++ )
    {
        int nLinkType, nLinkSize;
        unsigned char *pabyData;

        pabyData = DGNGetLinkage( hDGN, psElem, iLink, &nLinkType, 
                                  NULL, NULL, &nLinkSize );
        if( pabyData == NULL )
            return FALSE;

        if( nLinkType == DGNLT_SHAPE_FILL && nLinkSize >= 7 )
        {
            *pnColor = pabyData[8];
            return TRUE;
        }
    }
}

/************************************************************************/
/*                        DGNGetAssocID()                               */
/************************************************************************/

/**
 * Fetch association id for an element.
 *
 * This method will check if an element has an association id, and if so
 * returns it, otherwise returning -1.  Association ids are kept as a
 * user attribute linkage where present.
 *
 * @param hDGN the file.
 * @param psElem the element.
 *
 * @return The id or -1 on failure.
 */

int DGNGetAssocID( DGNHandle hDGN, DGNElemCore *psElem )

{
    int iLink;
    
    for( iLink = 0; TRUE; iLink++ )
    {
        int nLinkType, nLinkSize;
        unsigned char *pabyData;

        pabyData = DGNGetLinkage( hDGN, psElem, iLink, &nLinkType, 
                                  NULL, NULL, &nLinkSize );
        if( pabyData == NULL )
            return -1;

        if( nLinkType == DGNLT_ASSOC_ID && nLinkSize >= 8 )
        {
            return pabyData[4] 
                + pabyData[5] * 256 
                + pabyData[6]*256*256
                + pabyData[7] * 256*256*256;
        }
    }
}

/************************************************************************/
/*                          DGNRad50ToAscii()                           */
/*                                                                      */
/*      Convert one 16-bits Radix-50 to ASCII (3 chars).                */
/************************************************************************/

void DGNRad50ToAscii(unsigned short sRad50, char *str )
{
    unsigned short sValue;
    char           ch = '\0';
    unsigned short saQuots[3] = {1600,40,1};
    int i;

    for ( i=0; i<3; i++)
    {
        sValue = sRad50;
        sValue /= saQuots[i];
        /* Map 0..39 to ASCII */
        if (sValue==0)                     
            ch = ' ';          /* space */
        else if (sValue >= 1 && sValue <= 26) 
            ch = (char) (sValue-1+'A');/* printable alpha A..Z */
        else if (sValue == 27)             
            ch = '$';          /* dollar */
        else if (sValue == 28)             
            ch = '.';          /* period */
        else if (sValue == 29)             
            ch = ' ';          /* unused char, emit a space instead */
        else if (sValue >= 30 && sValue <= 39)
            ch = (char) (sValue-30+'0');   /* digit 0..9 */
        *str = ch;
        str++;

        sRad50-=(sValue*saQuots[i]);
    }

    /* Do zero-terminate */
    *str = '\0';
}

/************************************************************************/
/*                          DGNAsciiToRad50()                           */
/************************************************************************/

void DGNAsciiToRad50( const char *str, unsigned short *pRad50 )

{
    unsigned short rad50 = 0;
    int  i;

    for( i = 0; i < 3; i++ )
    {
        unsigned short value;

        if( i >= (int) strlen(str) )
        {
            rad50 = rad50 * 40;
            continue;
        }

        if( str[i] == '$' )
            value = 27;
        else if( str[i] == '.' )
            value = 28;
        else if( str[i] == ' ' )
            value = 29;
        else if( str[i] >= '0' && str[i] <= '9' )
            value = str[i] - '0' + 30;
        else if( str[i] >= 'a' && str[i] <= 'z' )
            value = str[i] - 'a' + 1;
        else if( str[i] >= 'A' && str[i] <= 'Z' )
            value = str[i] - 'A' + 1;
        else
            value = 0;

        rad50 = rad50 * 40 + value;
    }

    *pRad50 = rad50;
}


/************************************************************************/
/*                        DGNGetLineStyleName()                         */
/*                                                                      */
/*      Read the line style name from symbol table.                     */
/*      The got name is stored in psLine.                               */
/************************************************************************/

int DGNGetLineStyleName(CPL_UNUSED DGNInfo *psDGN,
                        DGNElemMultiPoint *psLine,
                        char szLineStyle[65] )
{
    if (psLine->core.attr_bytes > 0 &&
        psLine->core.attr_data[1] == 0x10 &&
        psLine->core.attr_data[2] == 0xf9 &&
        psLine->core.attr_data[3] == 0x79)
    {
#ifdef notdef
        for (int i=0;i<SYMBOL_TABLE_SIZE;i++)
        {
            if (*((unsigned char*)psDGN->buffer + 0x21e5 + i) == psLine->core.attr_data[4] &&
                *((unsigned char*)psDGN->buffer + 0x21e6 + i) == psLine->core.attr_data[5] &&
                *((unsigned char*)psDGN->buffer + 0x21e7 + i) == psLine->core.attr_data[6] &&
                *((unsigned char*)psDGN->buffer + 0x21e8 + i) == psLine->core.attr_data[7])
            {
                memcpy( szLineStyle, 
                        (unsigned char*)psDGN->buffer + 0x21e9 + i, 64 );
                szLineStyle[64] = '\0';
                return TRUE;
            }
        }
#endif
        return FALSE;
    }
    else
    {
        szLineStyle[0] = '\0';
        return FALSE;
    }
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
    DGNInfo *psInfo = (DGNInfo *) hDGN;

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

    fprintf( fp, "  offset=%d  size=%d bytes\n", 
             psElement->offset, psElement->size );

    fprintf( fp, 
             "  graphic_group:%-3d color:%d weight:%d style:%d\n", 
             psElement->graphic_group,
             psElement->color,
             psElement->weight,
             psElement->style );

    if( psElement->properties != 0 )
    {
        int     nClass;

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
          DGNElemMultiPoint     *psLine = (DGNElemMultiPoint *) psElement;
          int                   i;
          
          for( i=0; i < psLine->num_vertices; i++ )
              fprintf( fp, "  (%.6f,%.6f,%.6f)\n", 
                       psLine->vertices[i].x, 
                       psLine->vertices[i].y, 
                       psLine->vertices[i].z );
      }
      break;

      case DGNST_CELL_HEADER:
      {
          DGNElemCellHeader     *psCell = (DGNElemCellHeader*) psElement;

          fprintf( fp, "  totlength=%d, name=%s, class=%x, levels=%02x%02x%02x%02x\n",
                   psCell->totlength, psCell->name, psCell->cclass,
                   psCell->levels[0], psCell->levels[1], psCell->levels[2],
                   psCell->levels[3] );
          fprintf( fp, "  rnglow=(%.5f,%.5f,%.5f)\n"
                       "  rnghigh=(%.5f,%.5f,%.5f)\n",
                   psCell->rnglow.x, psCell->rnglow.y, psCell->rnglow.z,
                   psCell->rnghigh.x, psCell->rnghigh.y, psCell->rnghigh.z );
          fprintf( fp, "  origin=(%.5f,%.5f,%.5f)\n",
                   psCell->origin.x, psCell->origin.y, psCell->origin.z);
          
          if( psInfo->dimension == 2 )
              fprintf( fp, "  xscale=%g, yscale=%g, rotation=%g\n",
                       psCell->xscale, psCell->yscale, psCell->rotation );
          else
              fprintf( fp, "  trans=%g,%g,%g,%g,%g,%g,%g,%g,%g\n",
                       psCell->trans[0],
                       psCell->trans[1],
                       psCell->trans[2],
                       psCell->trans[3],
                       psCell->trans[4],
                       psCell->trans[5],
                       psCell->trans[6],
                       psCell->trans[7],
                       psCell->trans[8] );
      }
      break;

      case DGNST_CELL_LIBRARY:
      {
          DGNElemCellLibrary    *psCell = (DGNElemCellLibrary*) psElement;

          fprintf( fp, 
                   "  name=%s, class=%x, levels=%02x%02x%02x%02x, numwords=%d\n", 
                   psCell->name, psCell->cclass, 
                   psCell->levels[0], psCell->levels[1], psCell->levels[2], 
                   psCell->levels[3], psCell->numwords );
          fprintf( fp, "  dispsymb=%d, description=%s\n", 
                   psCell->dispsymb, psCell->description );
      }
      break;

      case DGNST_SHARED_CELL_DEFN:
      {
          DGNElemSharedCellDefn *psShared = (DGNElemSharedCellDefn *) psElement;

          fprintf( fp, "  totlength=%d\n", psShared->totlength);
      }
      break;

      case DGNST_ARC:
      {
          DGNElemArc    *psArc = (DGNElemArc *) psElement;

          if( psInfo->dimension == 2 )
              fprintf( fp, "  origin=(%.5f,%.5f), rotation=%f\n",
                       psArc->origin.x, 
                       psArc->origin.y, 
                       psArc->rotation );
          else
              fprintf( fp, "  origin=(%.5f,%.5f,%.5f), quat=%d,%d,%d,%d\n",
                       psArc->origin.x, 
                       psArc->origin.y, 
                       psArc->origin.z, 
                       psArc->quat[0], 
                       psArc->quat[1], 
                       psArc->quat[2], 
                       psArc->quat[3] );
          fprintf( fp, "  axes=(%.5f,%.5f), start angle=%f, sweep=%f\n", 
                   psArc->primary_axis,
                   psArc->secondary_axis,
                   psArc->startang,
                   psArc->sweepang );                   
      }
      break;

      case DGNST_TEXT:
      {
          DGNElemText   *psText = (DGNElemText *) psElement;

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

      case DGNST_TEXT_NODE:
      {
          DGNElemTextNode *psNode = (DGNElemTextNode *) psElement;

          fprintf( fp, 
                   "  totlength=%d, num_texts=%d\n",
                   psNode->totlength,
                   psNode->numelems );
          fprintf( fp, 
                   "  origin=(%.5f,%.5f), rotation=%f\n"
                   "  font=%d, just=%d, length_mult=%g, height_mult=%g\n",
                   psNode->origin.x, 
                   psNode->origin.y, 
                   psNode->rotation,
                   psNode->font_id,
                   psNode->justification,
                   psNode->length_mult,
                   psNode->height_mult );
          fprintf( fp, 
		   "  max_length=%d, used=%d,",
		   psNode->max_length, 
		   psNode->max_used );
          fprintf( fp, 
		   "  node_number=%d\n", 
		   psNode->node_number );
      }
      break;

      case DGNST_COMPLEX_HEADER:
      {
          DGNElemComplexHeader  *psHdr = (DGNElemComplexHeader *) psElement;

          fprintf( fp, 
                   "  totlength=%d, numelems=%d\n",
                   psHdr->totlength,
                   psHdr->numelems );
          if (psElement->type  == DGNT_3DSOLID_HEADER ||
              psElement->type  == DGNT_3DSURFACE_HEADER) {
            fprintf( fp, 
                     "  surftype=%d, boundelms=%d\n", 
                     psHdr->surftype, psHdr->boundelms );
          }
      }
      break;

      case DGNST_COLORTABLE:
      {
          DGNElemColorTable *psCT = (DGNElemColorTable *) psElement;
          int                   i;

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
          int iView;

          fprintf( fp, "  dimension = %d\n", psTCB->dimension );
          fprintf( fp, "  uor_per_subunit = %ld, subunits = `%s'\n",
                   psTCB->uor_per_subunit, psTCB->sub_units );
          fprintf( fp, "  subunits_per_master = %ld, master units = `%s'\n",
                   psTCB->subunits_per_master, psTCB->master_units );
          fprintf( fp, "  origin = (%.5f,%.5f,%.5f)\n", 
                   psTCB->origin_x,
                   psTCB->origin_y,
                   psTCB->origin_z );

          for( iView = 0; iView < 8; iView++ )
          {
              DGNViewInfo *psView = psTCB->views + iView;
              
              fprintf(fp, 
                      "  View%d: flags=%04X, levels=%02X%02X%02X%02X%02X%02X%02X%02X\n",
                      iView,
                      psView->flags, 
                      psView->levels[0],
                      psView->levels[1],
                      psView->levels[2],
                      psView->levels[3],
                      psView->levels[4],
                      psView->levels[5],
                      psView->levels[6],
                      psView->levels[7] );
              fprintf(fp, 
                      "        origin=(%g,%g,%g)\n        delta=(%g,%g,%g)\n", 
                      psView->origin.x, psView->origin.y, psView->origin.z,
                      psView->delta.x, psView->delta.y, psView->delta.z );
              fprintf(fp, 
                      "       trans=(%g,%g,%g,%g,%g,%g,%g,%g,%g)\n",
                      psView->transmatrx[0],
                      psView->transmatrx[1],
                      psView->transmatrx[2],
                      psView->transmatrx[3],
                      psView->transmatrx[4],
                      psView->transmatrx[5],
                      psView->transmatrx[6],
                      psView->transmatrx[7],
                      psView->transmatrx[8] );
          }
      }
      break;

      case DGNST_TAG_SET:
      {
          DGNElemTagSet *psTagSet = (DGNElemTagSet*) psElement;
          int            iTag;

          fprintf( fp, "  tagSetName=%s, tagSet=%d, tagCount=%d, flags=%d\n", 
                   psTagSet->tagSetName, psTagSet->tagSet, 
                   psTagSet->tagCount, psTagSet->flags );
          for( iTag = 0; iTag < psTagSet->tagCount; iTag++ )
          {
              DGNTagDef *psTagDef = psTagSet->tagList + iTag;

              fprintf( fp, "    %d: name=%s, type=%d, prompt=%s", 
                       psTagDef->id, psTagDef->name, psTagDef->type, 
                       psTagDef->prompt );
              if( psTagDef->type == 1 )
                  fprintf( fp, ", default=%s\n", 
                           psTagDef->defaultValue.string );
              else if( psTagDef->type == 3 || psTagDef->type == 5 )
                  fprintf( fp, ", default=%d\n", 
                           psTagDef->defaultValue.integer );
              else if( psTagDef->type == 4 )
                  fprintf( fp, ", default=%g\n", 
                           psTagDef->defaultValue.real );
              else
                  fprintf( fp, ", default=<unknown>\n" );
          }
      }
      break;

      case DGNST_TAG_VALUE:
      {
          DGNElemTagValue *psTag = (DGNElemTagValue*) psElement;

          fprintf( fp, "  tagType=%d, tagSet=%d, tagIndex=%d, tagLength=%d\n", 
                   psTag->tagType, psTag->tagSet, psTag->tagIndex, 
                   psTag->tagLength );
          if( psTag->tagType == 1 )
              fprintf( fp, "  value=%s\n", psTag->tagValue.string );
          else if( psTag->tagType == 3 )
              fprintf( fp, "  value=%d\n", psTag->tagValue.integer );
          else if( psTag->tagType == 4 )
              fprintf( fp, "  value=%g\n", psTag->tagValue.real );
      }
      break;

      case DGNST_CONE:
      {
          DGNElemCone *psCone = (DGNElemCone *) psElement;

          fprintf( fp, 
                   "  center_1=(%g,%g,%g) radius=%g\n"
                   "  center_2=(%g,%g,%g) radius=%g\n"
                   "  quat=%d,%d,%d,%d unknown=%d\n", 
                   psCone->center_1.x, psCone->center_1.y, psCone->center_1.z,
                   psCone->radius_1, 
                   psCone->center_2.x, psCone->center_2.y, psCone->center_2.z,
                   psCone->radius_2, 
                   psCone->quat[0], psCone->quat[1],
                   psCone->quat[2], psCone->quat[3],
                   psCone->unknown );
      }
      break;

      case DGNST_BSPLINE_SURFACE_HEADER:
      {
          DGNElemBSplineSurfaceHeader *psSpline =
            (DGNElemBSplineSurfaceHeader *) psElement;

          fprintf( fp, "  desc_words=%ld, curve type=%d\n",
                   psSpline->desc_words, psSpline->curve_type);

          fprintf( fp, "  U: properties=%02x",
                   psSpline->u_properties);
          if (psSpline->u_properties != 0) {
            if (psSpline->u_properties & DGNBSC_CURVE_DISPLAY) {
              fprintf(fp, ",CURVE_DISPLAY");
            }
            if (psSpline->u_properties & DGNBSC_POLY_DISPLAY) {
              fprintf(fp, ",POLY_DISPLAY");
            }
            if (psSpline->u_properties & DGNBSC_RATIONAL) {
              fprintf(fp, ",RATIONAL");
            }
            if (psSpline->u_properties & DGNBSC_CLOSED) {
              fprintf(fp, ",CLOSED");
            }
          }
          fprintf(fp, "\n");
          fprintf( fp, "     order=%d\n  %d poles, %d knots, %d rule lines\n",
                   psSpline->u_order, psSpline->num_poles_u, 
                   psSpline->num_knots_u, psSpline->rule_lines_u);

          fprintf( fp, "  V: properties=%02x",
                   psSpline->v_properties);
          if (psSpline->v_properties != 0) {
            if (psSpline->v_properties & DGNBSS_ARC_SPACING) {
              fprintf(fp, ",ARC_SPACING");
            }
            if (psSpline->v_properties & DGNBSS_CLOSED) {
              fprintf(fp, ",CLOSED");
            }
          }
          fprintf(fp, "\n");
          fprintf( fp, "     order=%d\n  %d poles, %d knots, %d rule lines\n",
                   psSpline->v_order, psSpline->num_poles_v, 
                   psSpline->num_knots_v, psSpline->rule_lines_v);
      }
      break;

      case DGNST_BSPLINE_CURVE_HEADER:
      {
          DGNElemBSplineCurveHeader *psSpline =
            (DGNElemBSplineCurveHeader *) psElement;

          fprintf( fp, 
                   "  desc_words=%ld, curve type=%d\n"
                   "  properties=%02x",
                   psSpline->desc_words, psSpline->curve_type,
                   psSpline->properties);
          if (psSpline->properties != 0) {
            if (psSpline->properties & DGNBSC_CURVE_DISPLAY) {
              fprintf(fp, ",CURVE_DISPLAY");
            }
            if (psSpline->properties & DGNBSC_POLY_DISPLAY) {
              fprintf(fp, ",POLY_DISPLAY");
            }
            if (psSpline->properties & DGNBSC_RATIONAL) {
              fprintf(fp, ",RATIONAL");
            }
            if (psSpline->properties & DGNBSC_CLOSED) {
              fprintf(fp, ",CLOSED");
            }
          }
          fprintf(fp, "\n");
          fprintf( fp, "  order=%d\n  %d poles, %d knots\n",
                   psSpline->order, psSpline->num_poles, psSpline->num_knots);
      }
      break;

      case DGNST_BSPLINE_SURFACE_BOUNDARY:
      {
          DGNElemBSplineSurfaceBoundary *psBounds =
            (DGNElemBSplineSurfaceBoundary *) psElement;

          fprintf( fp, "  boundary number=%d, # vertices=%d\n",
                   psBounds->number, psBounds->numverts);
          for (int i=0;i<psBounds->numverts;i++) {
            fprintf( fp, "  (%.6f,%.6f)\n",
                     psBounds->vertices[i].x,
                     psBounds->vertices[i].y);
          }
      }
      break;

      case DGNST_KNOT_WEIGHT:
      {
          DGNElemKnotWeight *psArray = (DGNElemKnotWeight *) psElement;
          int numelems = (psArray->core.size-36)/4;
          for (int i=0;i<numelems;i++) {
            fprintf(fp, "  %.6f\n", psArray->array[i]);
          }
      }
      break;

      default:
        break;
    }

    if( psElement->attr_bytes > 0 )
    {
        int iLink;

        fprintf( fp, "Attributes (%d bytes):\n", psElement->attr_bytes );
        
        for( iLink = 0; TRUE; iLink++ )

        {
            int nLinkType, nEntityNum=0, nMSLink=0, nLinkSize, i;
            unsigned char *pabyData;

            pabyData = DGNGetLinkage( hDGN, psElement, iLink, &nLinkType, 
                                      &nEntityNum, &nMSLink, &nLinkSize );
            if( pabyData == NULL )
                break;

            fprintf( fp, "Type=0x%04x", nLinkType );
            if( nMSLink != 0 || nEntityNum != 0 )
                fprintf( fp, ", EntityNum=%d, MSLink=%d", 
                         nEntityNum, nMSLink );

            int nBytes = psElement->attr_data + psElement->attr_bytes - pabyData;
            if( nBytes < nLinkSize )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "Corrupt linkage, element id:%d, link:%d",
                        psElement->element_id, iLink);
                fprintf(fp, " (Corrupt, declared size: %d, assuming size: %d)", 
                    nLinkSize, nBytes);
                nLinkSize = nBytes;
            }
            fprintf( fp, "\n  0x" );

            for( i = 0; i < nLinkSize; i++ )
                fprintf( fp, "%02x", pabyData[i] );
            fprintf( fp, "\n" );
            
        }
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
    static char szNumericResult[16];

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

      case DGNT_POINT_STRING:
        return "Point String";

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

      case DGNT_BSPLINE_POLE:
        return "B-Spline Pole";

      case DGNT_BSPLINE_SURFACE_HEADER:
        return "B-Spline Surface Header";

      case DGNT_BSPLINE_SURFACE_BOUNDARY:
        return "B-Spline Surface Boundary";

      case DGNT_BSPLINE_KNOT:
        return "B-Spline Knot";

      case DGNT_BSPLINE_CURVE_HEADER:
        return "B-Spline Curve Header";

      case DGNT_BSPLINE_WEIGHT_FACTOR:
        return "B-Spline Weight Factor";

      case DGNT_APPLICATION_ELEM:
        return "Application Element";

      case DGNT_SHARED_CELL_DEFN:
        return "Shared Cell Definition";
        
      case DGNT_SHARED_CELL_ELEM:
        return "Shared Cell Element";
        
      case DGNT_TAG_VALUE:
        return "Tag Value";
        
      case DGNT_CONE:
        return "Cone";

      case DGNT_3DSURFACE_HEADER:
        return "3D Surface Header";

      case DGNT_3DSOLID_HEADER:
        return "3D Solid Header";

      default:
        sprintf( szNumericResult, "%d", nType );
        return szNumericResult;
    }
}

/************************************************************************/
/*                         DGNGetAttrLinkSize()                         */
/************************************************************************/

/**
 * Get attribute linkage size. 
 *
 * Returns the size, in bytes, of the attribute linkage starting at byte
 * offset nOffset.  On failure a value of 0 is returned.
 *
 * @param hDGN the file from which the element originated.
 * @param psElement the element to report on.
 * @param nOffset byte offset within attribute data of linkage to check.
 *
 * @return size of linkage in bytes, or zero. 
 */

int DGNGetAttrLinkSize( CPL_UNUSED DGNHandle hDGN,
                        DGNElemCore *psElement,
                        int nOffset )
{
    if( psElement->attr_bytes < nOffset + 4 )
        return 0;

    /* DMRS Linkage */
    if( (psElement->attr_data[nOffset+0] == 0 
         && psElement->attr_data[nOffset+1] == 0)
        || (psElement->attr_data[nOffset+0] == 0 
            && psElement->attr_data[nOffset+1] == 0x80) )
        return 8;

    /* If low order bit of second byte is set, first byte is length */
    if( psElement->attr_data[nOffset+1] & 0x10 )
        return psElement->attr_data[nOffset+0] * 2 + 2;

    /* unknown */
    return 0;
}

/************************************************************************/
/*                           DGNGetLinkage()                            */
/************************************************************************/

/**
 * Returns requested linkage raw data. 
 *
 * A pointer to the raw data for the requested attribute linkage is returned
 * as well as (potentially) various information about the linkage including
 * the linkage type, database entity number and MSLink value, and the length
 * of the raw linkage data in bytes.
 *
 * If the requested linkage (iIndex) does not exist a value of zero is 
 * returned.
 *
 * The entity number is (loosely speaking) the index of the table within
 * the current database to which the MSLINK value will refer.  The entity
 * number should be used to lookup the table name in the MSCATALOG table. 
 * The MSLINK value is the key value for the record in the target table. 
 *
 * @param hDGN the file from which the element originated.
 * @param psElement the element to report on.
 * @param iIndex the zero based index of the linkage to fetch.
 * @param pnLinkageType variable to return linkage type.  This may be one of
 * the predefined DGNLT_ values or a different value. This pointer may be NULL.
 * @param pnEntityNum variable to return the entity number in or NULL if not
 * required.  
 * @param pnMSLink variable to return the MSLINK value in, or NULL if not 
 * required.
 * @param pnLength variable to returned the linkage size in bytes or NULL.
 * 
 * @return pointer to raw internal linkage data.  This data should not be
 * altered or freed.  NULL returned on failure. 
 */

unsigned char *DGNGetLinkage( DGNHandle hDGN, DGNElemCore *psElement, 
                              int iIndex, int *pnLinkageType,
                              int *pnEntityNum, int *pnMSLink, int *pnLength )
    
{
    int nAttrOffset;
    int iLinkage, nLinkSize;

    for( iLinkage=0, nAttrOffset=0;
         (nLinkSize = DGNGetAttrLinkSize( hDGN, psElement, nAttrOffset)) != 0;
         iLinkage++, nAttrOffset += nLinkSize )
    {
        if( iLinkage == iIndex )
        {
            int  nLinkageType=0, nEntityNum=0, nMSLink = 0;
            CPLAssert( nLinkSize > 4 );

            if( psElement->attr_data[nAttrOffset+0] == 0x00
                && (psElement->attr_data[nAttrOffset+1] == 0x00
                    || psElement->attr_data[nAttrOffset+1] == 0x80) )
            {
                nLinkageType = DGNLT_DMRS;
                nEntityNum = psElement->attr_data[nAttrOffset+2] 
                    + psElement->attr_data[nAttrOffset+3] * 256;
                nMSLink = psElement->attr_data[nAttrOffset+4] 
                    + psElement->attr_data[nAttrOffset+5] * 256
                    + psElement->attr_data[nAttrOffset+6] * 65536;
            }
            else
                nLinkageType = psElement->attr_data[nAttrOffset+2] 
                    + psElement->attr_data[nAttrOffset+3] * 256;

            // Possibly an external database linkage?
            if( nLinkSize == 16 && nLinkageType != DGNLT_SHAPE_FILL )
            {
                nEntityNum = psElement->attr_data[nAttrOffset+6] 
                    + psElement->attr_data[nAttrOffset+7] * 256;
                nMSLink = psElement->attr_data[nAttrOffset+8] 
                    + psElement->attr_data[nAttrOffset+9] * 256
                    + psElement->attr_data[nAttrOffset+10] * 65536
                    + psElement->attr_data[nAttrOffset+11] * 65536 * 256;
                
            }

            if( pnLinkageType != NULL )
                *pnLinkageType = nLinkageType;
            if( pnEntityNum != NULL )
                *pnEntityNum = nEntityNum;
            if( pnMSLink != NULL )
                *pnMSLink = nMSLink;
            if( pnLength != NULL )
                *pnLength = nLinkSize;

            return psElement->attr_data + nAttrOffset;
        }
    }
             
    return NULL;
}

/************************************************************************/
/*                         DGNRotationToQuat()                          */
/*                                                                      */
/*      Compute a quaternion for a given Z rotation.                    */
/************************************************************************/

void DGNRotationToQuaternion( double dfRotation, int *panQuaternion )

{
    double dfRadianRot = (dfRotation / 180.0)  * PI;

    panQuaternion[0] = (int) (cos(-dfRadianRot/2.0) * 2147483647);
    panQuaternion[1] = 0;
    panQuaternion[2] = 0;
    panQuaternion[3] = (int) (sin(-dfRadianRot/2.0) * 2147483647);
}

/************************************************************************/
/*                         DGNQuaternionToMatrix()                      */
/*                                                                      */
/*      Compute a rotation matrix for a given quaternion                */
/* FIXME: Write documentation on how to use this matrix                 */
/* (i.e. things like row/column major, OpenGL style or not)             */
/* kintel 20030819                                                      */
/************************************************************************/

void DGNQuaternionToMatrix( int *quat, float *mat )
{
  double q[4];

  q[0] = 1.0 * quat[1] / (1<<31);
  q[1] = 1.0 * quat[2] / (1<<31);
  q[2] = 1.0 * quat[3] / (1<<31);
  q[3] = 1.0 * quat[0] / (1<<31);

  mat[0*3+0] = (float) (q[0]*q[0] - q[1]*q[1] - q[2]*q[2] + q[3]*q[3]);
  mat[0*3+1] = (float) (2 * (q[2]*q[3] + q[0]*q[1]));
  mat[0*3+2] = (float) (2 * (q[0]*q[2] - q[1]*q[3]));
  mat[1*3+0] = (float) (2 * (q[0]*q[1] - q[2]*q[3]));
  mat[1*3+1] = (float) (-q[0]*q[0] + q[1]*q[1] - q[2]*q[2] + q[3]*q[3]);
  mat[1*3+2] = (float) (2 * (q[0]*q[3] + q[1]*q[2]));
  mat[2*3+0] = (float) (2 * (q[0]*q[2] + q[1]*q[3])); 
  mat[2*3+1] = (float) (2 * (q[1]*q[2] - q[0]*q[3]));
  mat[2*3+2] = (float) (-q[0]*q[0] - q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
}

/************************************************************************/
/*                  DGNTransformPointWithQuaternion()                   */
/************************************************************************/

void DGNTransformPointWithQuaternionVertex( CPL_UNUSED int *quat,
                                            CPL_UNUSED DGNPoint *v1,
                                            CPL_UNUSED DGNPoint *v2 )
{
/* ==================================================================== */
/*      Original code provided by kintel 20030819, but assumed to be    */
/*      incomplete.                                                     */
/* ==================================================================== */

#ifdef notdef
    See below for sketched implementation. kintel 20030819.
                               float x,y,z,w;
    // FIXME: Convert quat to x,y,z,w
    v2.x = w*w*v1.x + 2*y*w*v1.z - 2*z*w*v1.y + x*x*v1.x + 2*y*x*v1.y + 2*z*x*v1.z - z*z*v1.x - y*y*v1.x; 
    v2.y = 2*x*y*v1.x + y*y*v1.y + 2*z*y*v1.z + 2*w*z*v1.x - z*z*v1.y + w*w*v1.y - 2*x*w*v1.z - x*x*v1.y; 
    v2.z = 2*x*z*v1.x + 2*y*z*v1.y + z*z*v1.z - 2*w*y*v1.x - y*y*v1.z + 2*w*x*v1.y - x*x*v1.z + w*w*v1.z;
#endif

/* ==================================================================== */
/*      Impelementation provided by Peggy Jung - 2004/03/05.            */
/*      peggy.jung at moskito-gis dot de.  I haven't tested it.         */
/* ==================================================================== */

/*  Version: 0.1                                 Datum: 26.01.2004
 
IN:
x,y,z               // DGNPoint &v1
quat[]              // 
 
OUT:
newX, newY, newZ    // DGNPoint &v2

A u t o r  :  Peggy Jung
*/
/*
    double ROT[12];  //rotation matrix for a given quaternion
    double xx, xy, xz, xw, yy, yz, yw, zz, zw;
    double a, b, c, d, n, x, y, z;

    x = v1->x;
    y = v1->y;
    z = v1->z;
 
    n = sqrt((double)PDP2PC_long(quat[0])*(double)PDP2PC_long(quat[0])+(double)PDP2PC_long(quat[1])*(double)PDP2PC_long(quat[1])+
             (double)PDP2PC_long(quat[2])*(double)PDP2PC_long(quat[2])+(double)PDP2PC_long(quat[3])*(double)PDP2PC_long(quat[3]));
 
    a = (double)PDP2PC_long(quat[0])/n; //w
    b = (double)PDP2PC_long(quat[1])/n; //x
    c = (double)PDP2PC_long(quat[2])/n; //y
    d = (double)PDP2PC_long(quat[3])/n; //z
 
    xx      = b*b;
    xy      = b*c;
    xz      = b*d;
    xw      = b*a;
 
    yy      = c*c;
    yz      = c*d;
    yw      = c*a;
 
    zz      = d*d;
    zw      = d+a;
 
    ROT[0] = 1 - 2 * yy - 2 * zz ;
    ROT[1] =     2 * xy - 2 * zw ;
    ROT[2] =     2 * xz + 2 * yw ;
 
    ROT[4] =     2 * xy + 2 * zw ;
    ROT[5] = 1 - 2 * xx - 2 * zz ;
    ROT[6] =     2 * yz - 2 * xw ;
 
    ROT[8] =     2 * xz - 2 * yw ;
    ROT[9] =     2 * yz + 2 * xw ;
    ROT[10] = 1 - 2 * xx - 2 * yy ;
 
    v2->x = ROT[0]*x + ROT[1]*y + ROT[2]*z;
    v2->y = ROT[4]*x + ROT[5]*y + ROT[6]*z;
    v2->z = ROT[8]*x + ROT[9]*y + ROT[10]*z;
*/
}
