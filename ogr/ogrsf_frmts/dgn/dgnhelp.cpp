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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.9  2002/05/31 03:40:22  warmerda
 * added improved support for parsing attribute linkages
 *
 * Revision 1.8  2002/05/30 19:24:38  warmerda
 * add partial support for tag type 5
 *
 * Revision 1.7  2002/04/22 20:44:41  warmerda
 * added (partial) cell library support
 *
 * Revision 1.6  2002/04/16 17:56:07  warmerda
 * Initialize ch.
 *
 * Revision 1.5  2002/03/14 21:38:05  warmerda
 * removed DGN_DEBUG macro
 *
 * Revision 1.4  2002/03/12 17:07:26  warmerda
 * added tagset and tag value element support
 *
 * Revision 1.3  2002/02/22 22:18:10  warmerda
 * ensure that multi-part attributes supported for fill info
 *
 * Revision 1.2  2002/01/15 06:42:30  warmerda
 * remove stub statement
 *
 * Revision 1.1  2002/01/15 06:38:02  warmerda
 * New
 *
 */

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
    DGNInfo	*psDGN = (DGNInfo *) hDGN;

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
/*                          DGNRad50ToAscii()                           */
/*                                                                      */
/*      Convert one 16-bits Radix-50 to ASCII (3 chars).                */
/************************************************************************/

void DGNRad50ToAscii(unsigned short rad50, char *str )
{
    unsigned char cTimes;
    unsigned short value;
    unsigned short temp;
    char ch = '\0';

    while (rad50 > 0)
    {
        value = rad50;
        cTimes = 0;
        while (value >= 40)
        {
            value /= 40;
            cTimes ++;
        }

        /* Map 0..39 to ASCII */
        if (value==0)                      
            ch = ' ';          /* space */
        else if (value >= 1 && value <= 26)  
            ch = value-1+'A';/* printable alpha A..Z */
        else if (value == 27)              
            ch = '$';          /* dollar */
        else if (value == 28)              
            ch = '.';          /* period */
        else if (value == 29)              
            ch = ' ';          /* unused char, emit a space instead */
        else if (value >= 30 && value <= 39) 
            ch = value-30+'0';   /* digit 0..9 */

        *str = ch;
        str++;

        temp = 1;
        while (cTimes-- > 0)
            temp *= 40;

        rad50-=value*temp;
    }
    /* Do zero-terminate */
    *str = '\0';
}

/************************************************************************/
/*                        DGNGetLineStyleName()                         */
/*                                                                      */
/*      Read the line style name from symbol table.                     */
/*      The got name is stored in psLine.                               */
/************************************************************************/

int DGNGetLineStyleName(DGNInfo *psDGN, DGNElemMultiPoint *psLine,
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

      case DGNST_CELL_HEADER:
      {
          DGNElemCellHeader	*psCell = (DGNElemCellHeader*) psElement;

          fprintf( fp, "  totlength=%d, name=%s, class=%x, levels=%02x%02x%02x%02x\n", 
                   psCell->totlength, psCell->name, psCell->cclass, 
                   psCell->levels[0], psCell->levels[1], psCell->levels[2], 
                   psCell->levels[3] );
          fprintf( fp, "  rnglow=(%.5f,%.5f), rnghigh=(%.5f,%.5f)\n",
                   psCell->rnglow.x, psCell->rnglow.y, 
                   psCell->rnghigh.x, psCell->rnghigh.y ); 
          fprintf( fp, "  origin=(%.5f,%.5f)\n", 
                   psCell->origin.x, psCell->origin.y );
          fprintf( fp, "  xscale=%g, yscale=%g, rotation=%g\n", 
                   psCell->xscale, psCell->yscale, psCell->rotation );
      }
      break;

      case DGNST_CELL_LIBRARY:
      {
          DGNElemCellLibrary	*psCell = (DGNElemCellLibrary*) psElement;

          fprintf( fp, 
                "  name=%s, class=%x, levels=%02x%02x%02x%02x, numwords=%d\n", 
                   psCell->name, psCell->cclass, 
                   psCell->levels[0], psCell->levels[1], psCell->levels[2], 
                   psCell->levels[3], psCell->numwords );
          fprintf( fp, "  dispsymb=%d, description=%s\n", 
                   psCell->dispsymb, psCell->description );
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

      case DGNST_COMPLEX_HEADER:
      {
          DGNElemComplexHeader	*psHdr = (DGNElemComplexHeader *) psElement;

          fprintf( fp, 
                   "  totlength=%d, numelems=%d\n",
                   psHdr->totlength,
                   psHdr->numelems );
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

      case DGNST_TAG_SET:
      {
          DGNElemTagSet	*psTagSet = (DGNElemTagSet*) psElement;
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

      case DGNT_SHARED_CELL_DEFN:
        return "Shared Cell Definition";
        
      case DGNT_SHARED_CELL_ELEM:
        return "Shared Cell Element";
        
      case DGNT_TAG_VALUE:
        return "Tag Value";
        
      default:
        sprintf( szNumericResult, "%d", nType );
        return szNumericResult;
    }
}

/************************************************************************/
/*                         DGNGetAttrLinkSize()                         */
/************************************************************************/

int DGNGetAttrLinkSize( DGNHandle hDGN, DGNElemCore *psElement, int nOffset )

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
                && (psElement->attr_data[nAttrOffset+0] == 0x00
                    || psElement->attr_data[nAttrOffset+0] == 0x80) )
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
