/******************************************************************************
 * $Id$
 *
 * Project:  Microstation DGN Access Library
 * Purpose:  DGN Access functions related to writing DGN elements.
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.2  2002/10/20 01:50:20  warmerda
 * added new write prototypes
 *
 * Revision 1.1  2002/03/14 21:40:43  warmerda
 * New
 *
 */

#include "dgnlibp.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          DGNResizeElement()                          */
/************************************************************************/

/**
 * Resize an existing element.
 *
 * If the new size is the same as the old nothing happens. 
 *
 * Otherwise, the old element in the file is marked as deleted, and the
 * DGNElemCore.offset and element_id are set to -1 indicating that the
 * element should be written to the end of file when next written by
 * DGNWriteElement().  The internal raw data buffer is updated to the new
 * size.
 * 
 * Only elements with "raw_data" loaded may be moved.
 *
 * In normal use the DGNResizeElement() call would be called on a previously
 * loaded element, and afterwards the raw_data would be updated before calling
 * DGNWriteElement().  If DGNWriteElement() isn't called after 
 * DGNResizeElement() then the element will be lost having been marked as
 * deleted in it's old position but never written at the new location. 
 *
 * @param hDGN the DGN file on which the element lives.
 * @param psElement the element to alter.  
 * @param nNewSize the desired new size of the element in bytes.  Must be
 * a multiple of 2. 
 *
 * @return TRUE on success, or FALSE on error.
 */

int DGNResizeElement( DGNHandle hDGN, DGNElemCore *psElement, int nNewSize )

{
    DGNInfo	*psDGN = (DGNInfo *) hDGN;

/* -------------------------------------------------------------------- */
/*      Check various conditions.                                       */
/* -------------------------------------------------------------------- */
    if( psElement->raw_bytes == 0 
        || psElement->raw_bytes != psElement->size )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Raw bytes not loaded, or not matching element size." );
        return FALSE;
    }

    if( nNewSize % 2 == 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "DGNResizeElement(%d): "
                  "can't change to odd (not divisible by two) size.", 
                  nNewSize );
        return FALSE;
    }

    if( nNewSize == psElement->raw_bytes )
        return TRUE;

/* -------------------------------------------------------------------- */
/*      Mark the existing element as deleted if the element has to      */
/*      move to the end of the file.                                    */
/* -------------------------------------------------------------------- */

    if( psElement->offset != -1 )
    {
        int nOldFLoc = VSIFTell( psDGN->fp );
        unsigned char abyLeader[2];
        
        if( VSIFSeek( psDGN->fp, psElement->offset, SEEK_SET ) != 0
            || VSIFRead( abyLeader, sizeof(abyLeader), 1, psDGN->fp ) != 1 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Failed seek or read when trying to mark existing\n"
                      "element as deleted in DGNResizeElement()\n" );
            return FALSE;
        }

        abyLeader[1] |= 0x80;
        
        if( VSIFSeek( psDGN->fp, psElement->offset, SEEK_SET ) != 0
            || VSIFWrite( abyLeader, sizeof(abyLeader), 1, psDGN->fp ) != 1 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Failed seek or write when trying to mark existing\n"
                      "element as deleted in DGNResizeElement()\n" );
            return FALSE;
        }

        VSIFSeek( psDGN->fp, SEEK_SET, nOldFLoc );

        if( psElement->element_id != -1 && psDGN->index_built )
            psDGN->element_index[psElement->element_id].flags 
                |= DGNEIF_DELETED;
    }

    psElement->offset = -1; /* move to end of file. */
    psElement->element_id = -1;

/* -------------------------------------------------------------------- */
/*      Set the new size information, and realloc the raw data buffer.  */
/* -------------------------------------------------------------------- */
    psElement->size = nNewSize;
    psElement->raw_data = (unsigned char *) 
        CPLRealloc( psElement->raw_data, nNewSize );
    psElement->raw_bytes = nNewSize;

/* -------------------------------------------------------------------- */
/*      Update the size information within the raw buffer.              */
/* -------------------------------------------------------------------- */
    int nWords = (nNewSize / 2) - 2;

    psElement->raw_data[2] = nWords % 256;
    psElement->raw_data[3] = nWords / 256;

    return TRUE;
}

/************************************************************************/
/*                          DGNWriteElement()                           */
/************************************************************************/

/** 
 * Write element to file. 
 *
 * Only elements with "raw_data" loaded may be written.
 *
 * The passed element is written to the indicated file.  If the 
 * DGNElemCore.offset field is -1 then the element is written at the end of
 * the file (and offset/element are reset properly) otherwise the element 
 * is written back to the location indicated by DGNElemCore.offset.  
 *
 * If the element is added at the end of the file, and if an element index
 * has already been built, it will be updated to reference the new element.
 *
 * This function takes care of ensuring that the end-of-file marker is 
 * maintained after the last element.
 *
 * @param hDGN the file to write the element to.
 * @param psElement the element to write. 
 *
 * @return TRUE on success or FALSE in case of failure.
 */

int DGNWriteElement( DGNHandle hDGN, DGNElemCore *psElement )

{
    DGNInfo	*psDGN = (DGNInfo *) hDGN;

/* ==================================================================== */
/*      If this element hasn't been positioned yet, place it at the     */
/*      end of the file.                                                */
/* ==================================================================== */
    if( psElement->offset == -1 )
    {
        int nJunk;

        // We must have an index, in order to properly assign the 
        // element id of the newly written element.  Ensure it is built.
        if( !psDGN->index_built )
            DGNBuildIndex( psDGN );

        // Read the current "last" element.
        if( !DGNGotoElement( hDGN, psDGN->element_count-1 ) )
            return FALSE;

        if( !DGNLoadRawElement( psDGN, &nJunk, &nJunk ) )
            return FALSE;

        // Establish the position of the new element.
        psElement->offset = VSIFTell( psDGN->fp );
        psElement->element_id = psDGN->element_count;

        // Grow element buffer if needed.
        if( psDGN->element_count == psDGN->max_element_count )
        {
            psDGN->max_element_count += 500;
            
            psDGN->element_index = (DGNElementInfo *) 
                CPLRealloc( psDGN->element_index, 
                            psDGN->max_element_count * sizeof(DGNElementInfo));
        }

        // Set up the element info
        DGNElementInfo *psInfo;
        
        psInfo = psDGN->element_index + psDGN->element_count;
        psInfo->level = psElement->level;
        psInfo->type = psElement->type;
        psInfo->stype = psElement->stype;
        psInfo->offset = psElement->offset;
        if( psElement->complex )
            psInfo->flags = DGNEIF_COMPLEX;
        else
            psInfo->flags = 0;
    
        psDGN->element_count++;
    }

/* -------------------------------------------------------------------- */
/*      Write out the element.                                          */
/* -------------------------------------------------------------------- */
    if( VSIFSeek( psDGN->fp, psElement->offset, SEEK_SET ) != 0 
        || VSIFWrite( psElement->raw_data, psElement->raw_bytes, 
                      1, psDGN->fp) != 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Error seeking or writing new element of %d bytes at %d.",
                  psElement->offset, 
                  psElement->raw_bytes );
        return FALSE;
    }

    psDGN->next_element_id = psElement->element_id + 1;

/* -------------------------------------------------------------------- */
/*      Write out the end of file 0xffff marker (if we were             */
/*      extending the file), but push the file pointer back before      */
/*      this EOF when done.                                             */
/* -------------------------------------------------------------------- */
    if( psDGN->next_element_id == psDGN->element_count )
    {
        unsigned char abyEOF[2];

        abyEOF[0] = 0xff;
        abyEOF[1] = 0xff;

        VSIFWrite( abyEOF, 2, 1, psDGN->fp );
        VSIFSeek( psDGN->fp, -2, SEEK_CUR );
    }
        
    return TRUE;
}
    
/************************************************************************/
/*                             DGNCreate()                              */
/************************************************************************/

/**
 * Create new DGN file.
 *
 * This function will create a new DGN file based on the provided seed
 * file, and return a handle on which elements may be read and written.
 *
 * The following creation flags may be passed:
 * <ul>
 * <li> DGNCF_USE_SEED_UNITS: The master and subunit resolutions and names
 * from the seed file will be used in the new file.  The nMasterUnitPerSubUnit,
 * nUORPerSubUnit, pszMasterUnits, and pszSubUnits arguments will be ignored.
 * <li> DGNCF_USE_SEED_ORIGIN: The origin from the seed file will be used
 * and the X, Y and Z origin passed into the call will be ignored. 
 * <li> DGNCF_COPY_SEED_FILE_COLOR_TABLE: Should the first color table occuring
 * in the seed file also be copied? 
 * <li> DGNCF_COPY_WHOLE_SEED_FILE: By default only the first three elements
 * (TCB, Digitizer Setup and Level Symbology) are copied from the seed file. 
 * If this flag is provided the entire seed file is copied verbatim (with the
 * TCB origin and units possibly updated).
 * </ul>
 * 
 * @param pszNewFilename the filename to create.  If it already exists
 * it will be overwritten.
 * @param pszSeedFile the seed file to copy header from.
 * @param nCreationFlags An ORing of DGNCF_* flags that are to take effect.
 * @param dfOriginX the X origin for the file.  
 * @param dfOriginY the Y origin for the file. 
 * @param dfOriginZ the Z origin for the file. 
 * @param nMasterUnitPerSubUnit the number of subunits in one master unit.
 * @param nUORPerSubUnit the number of UOR (units of resolution) per subunit.
 * @param pszMasterUnits the name of the master units (2 characters). 
 * @param pszSubUnits the name of the subunits (2 characters). 
 */

DGNHandle
      DGNCreate( const char *pszNewFilename, const char *pszSeedFile, 
                 int nCreationFlags, 
                 double dfOriginX, double dfOriginY, double dfOriginZ,
                 int nMasterUnitPerSubUnit, int nUORPerSubUnit, 
                 const char *pszMasterUnits, const char *pszSubUnits )

{
    return NULL;
}

/************************************************************************/
/*                          DGNCloneElement()                           */
/************************************************************************/

/**
 * Clone a retargetted element.
 *
 * Creates a copy of an element in a suitable form to write to a
 * different file than that it was read from. 
 *
 * NOTE: At this time the clone operation will fail if the source
 * and destination file have a different origin or master/sub units. 
 *
 * @param hDGNSrc the source file (from which psSrcElement was read).
 * @param hDGNDst the destination file (to which the returned element may be
 * written). 
 * @param psSrcElement the element to be cloned (from hDGNSrc). 
 *
 * @return NULL on failure, or an appropriately modified copy of 
 * the source element suitable to write to hDGNDst. 
 */

DGNElemCore *DGNCloneElement( DGNHandle hDGNSrc, DGNHandle hDGNDst, 
                              DGNElemCore *psSrcElement )

{
    return NULL;
}


/************************************************************************/
/*                         DGNUpdateElemCore()                          */
/************************************************************************/

/**
 * Change element core values.
 * 
 * The indicated values in the element are updated in the structure, as well
 * as in the raw data.  The updated element is not written to disk.  That
 * must be done with DGNWriteElement().   The element must have raw_data
 * loaded.
 * 
 * @param hDGN the file on which the element belongs. 
 * @param psElement the element to modify.
 * @param nLevel the new level value.
 * @param nGraphicGroup the new graphic group value.
 * @param nColor the new color index.
 * @param nWeight the new element weight.
 * @param nStyle the new style value for the element.
 *
 * @return Returns TRUE on success or FALSE on failure.
 */

int DGNUpdateElemCore( DGNHandle hDGN, DGNElemCore *psElement, 
                       int nLevel, int nGraphicGroup, int nColor, 
                       int nWeight, int nStyle )

{
    return FALSE;
}

/************************************************************************/
/*                     DGNUpdateElemCoreExtended()                      */
/************************************************************************/

/**
 * Update internal raw data representation.
 *
 * The raw_data representation of the passed element is updated to reflect
 * the various core fields.  The DGNElemCore level, type, complex, deleted,
 * graphic_group, properties, color, weight and style values are all 
 * applied to the raw_data representation.  Spatial bounds, element type
 * specific information and attributes are not updated in the raw data. 
 *
 * @param hDGN the file to which the element belongs. 
 * @param psElement the element to be updated. 
 *
 * @return TRUE on success, or FALSE on failure.
 */


int DGNUpdateElemCoreExtended( DGNHandle hDGN, DGNElemCore *psElement )

{
    return TRUE;
}

/************************************************************************/
/*                      DGNCreateMultiPointElem()                       */
/************************************************************************/

/**
 * Create new multi-point element. 
 *
 * The newly created element will still need to be written to file using
 * DGNWriteElement(). Also the level and other core values will be defaulted.
 * Use DGNUpdateElemCore() on the element before writing to set these values.
 *
 * NOTE: There are restrictions on the nPointCount for some elements. For
 * instance, DGNT_LINE can only have 2 points. Maximum element size 
 * precludes very large numbers of points. 
 *
 * @param hDGN the file on which the element will eventually be written.
 * @param nType the type of the element to be created.  It must be one of
 * DGNT_LINE, DGNT_LINE_STRING, DGNT_SHAPE, DGNT_CURVE or DGNT_BSPLINE. 
 * @param nPointCount the number of points in the pasVertices list.
 * @param pasVertices the list of points to be written. 
 *
 * @return the new element (a DGNElemMultiPoint structure) or NULL on failure.
 */

DGNElemCore *DGNCreateMultiPointElem( DGNHandle hDGN, int nType, 
                                      int nPointCount, DGNPoint *pasVertices )

{
    return NULL;
}

/************************************************************************/
/*                         DGNCreateArcElem2D()                         */
/************************************************************************/

/**
 * Create Arc or Ellipse element.
 *
 * Create a new 2D arc or ellipse element.  The start angle, and sweep angle
 * are ignored for DGNT_ELLIPSE but used for DGNT_ARC.
 *
 * The newly created element will still need to be written to file using
 * DGNWriteElement(). Also the level and other core values will be defaulted.
 * Use DGNUpdateElemCore() on the element before writing to set these values.
 *
 * @param hDGN the DGN file on which the element will eventually be written.
 * @param nType either DGNT_ELLIPSE or DGNT_ARC to select element type. 
 * @param dfOriginX the origin (center of rotation) of the arc (X).
 * @param dfOriginY the origin (center of rotation) of the arc (Y).
 * @param dfPrimaryAxis the length of the primary axis.
 * @param dfSecondaryAxis the length of the secondary axis. 
 * @param dfRotation Counterclockwise rotation in degrees. 
 * @param dfStartAngle start angle, degrees counterclockwise of primary axis.
 * @param dfSweepAngle sweep angle, degrees
 * 
 * @return the new element (DGNElemArc) or NULL on failure.
 */

DGNElemCore *
DGNCreateArcElem2D( DGNHandle hDGN, int nType, 
                    double dfOriginX, double dfOriginY,
                    double dfPrimaryAxis, double dfSecondaryAxis, 
                    double dfRotation, 
                    double dfStartAngle, double dfSweepAngle )

{
    return NULL;
}
                                 
/************************************************************************/
/*                         DGNCreateTextElem()                          */
/************************************************************************/

/**
 * Create text element.
 *
 * The newly created element will still need to be written to file using
 * DGNWriteElement(). Also the level and other core values will be defaulted.
 * Use DGNUpdateElemCore() on the element before writing to set these values.
 *
 * @param hDGN the file on which the element will eventually be written.
 * @param pszText the string of text. 
 * @param nFontId microstation font id for the text.  1 may be used as default.
 * @param nJustification text justification.  One of DGNJ_LEFT_TOP, 
 * DGNJ_LEFT_CENTER, DGNJ_LEFT_BOTTOM, DGNJ_CENTER_TOP, DGNJ_CENTER_CENTER, 
 * DGNJ_CENTER_BOTTOM, DGNJ_RIGHT_TOP, DGNJ_RIGHT_CENTER, DGNJ_RIGHT_BOTTOM.
 * @param dfLengthMult character width in master units.
 * @param dfHeightMult character height in master units.
 * @param dfRotation Counterclockwise text rotation in degrees.
 * @param dfOriginX Text origin (X).
 * @param dfOriginY Text origin (Y).
 * @param dfOriginZ Text origin (Z).
 * 
 * @return the new element (DGNElemText) or NULL on failure.
 */

DGNElemCore *
DGNCreateTextElem( DGNHandle hDGN, const char *pszText, 
                   int nFontId, int nJustification, 
                   double dfLengthMult, double dfHeightMult, 
                   double dfRotation, 
                   double dfOriginX, double dfOriginY, double dfOriginZ )

{
    return NULL;
}

/************************************************************************/
/*                      DGNCreateColorTableElem()                       */
/************************************************************************/

/**
 * Create color table element.
 *
 * Creates a color table element with the indicated color table. 
 *
 * Note that color table elements are actally of type DGNT_GROUP_DATA(5)
 * and always on level 1.  Do not alter the level with DGNUpdateElemCore()
 * or the element will essentially be corrupt. 
 *
 * The newly created element will still need to be written to file using
 * DGNWriteElement(). Also the level and other core values will be defaulted.
 * Use DGNUpdateElemCore() on the element before writing to set these values.
 *
 * @param hDGN the file to which the element will eventually be written.
 * @param nScreenFlag the screen to which the color table applies
 * (0 = left, 1 = right). 
 * @param abyColorInfo[8][3] array of 256 color entries. The first is
 * the background color. 
 *
 * @return the new element (DGNElemColorTable) or NULL on failure. 
 */


DGNElemCore *
DGNCreateColorTableElem( DGNHandle hDGN, int nScreenFlag, 
                         GByte abyColorInfo[256][3] )

{
    return NULL;
}

/************************************************************************/
/*                     DGNCreateComplexHeaderElem()                     */
/************************************************************************/

DGNElemCore *
DGNCreateComplexHeaderElem( DGNHandle hDGN, int nType, 
                            int nTotLength, int nNumElems )

/**
 * Create complex chain/shape header.
 *
 * The newly created element will still need to be written to file using
 * DGNWriteElement(). Also the level and other core values will be defaulted.
 * Use DGNUpdateElemCore() on the element before writing to set these values.
 *
 * The nTotLength is the sum of the size of all elements in the complex 
 * group plus 5.  The DGNCreateComplexHeaderFromGroup() can be used to build
 * a complex element from the members more conveniently.  
 *
 * @param hDGN the file on which the element will be written.
 * @param nType either DGNT_COMPLEX_CHAIN_HEADER or DGNT_COMPLEX_SHAPE_HEADER
 * depending on whether the list is open or closed (last point equal to last).
 * @param nTotLength the value of the totlength field in the element.
 * @param nNumElems the number of elements in the complex group not including
 * the header element. 
 *
 * @return the new element (DGNElemComplexHeader) or NULL on failure. 
 */

{
    return NULL;
}

/************************************************************************/
/*                  DGNCreateComplexHeaderFromGroup()                   */
/************************************************************************/

/**
 * Create complex chain/shape header.
 *
 * This function is similar to DGNCreateComplexHeaderElem(), but it takes
 * care of computing the total size of the set of elements being written.
 * It also takes care of some other convenience issues, like marking all the
 * member elements as complex, and setting the level based on the level of
 * the member elements. 
 * 
 * @param hDGN the file on which the element will be written.
 * @param nType either DGNT_COMPLEX_CHAIN_HEADER or DGNT_COMPLEX_SHAPE_HEADER
 * depending on whether the list is open or closed (last point equal to last).
 * @param nNumElems the number of elements in the complex group not including
 * the header element. 
 * @param papsElems array of pointers to nNumElems elements in the complex
 * group.  Some updates may be made to these elements. 
 *
 * @return the new element (DGNElemComplexHeader) or NULL on failure. 
 */

DGNElemCore *
DGNCreateComplexHeaderFromGroup( DGNHandle hDGN, int nType, 
                                 int nNumElems, DGNElemCore **papsElems )

{
    return NULL;
}

/************************************************************************/
/*                            DGNAddMSLink()                            */
/************************************************************************/

/**
 * Add a database link to element.
 *
 * The target element must already have raw_data loaded, and it will be 
 * resized (see DGNResizeElement()) as needed for the new attribute data. 
 * Note that the element is not written to disk immediate.  Use 
 * DGNWriteElement() for that. 
 *
 * @param hDGN the file to which the element corresponds.
 * @param psElement the element being updated.
 * @param nLinkageType link type (DGNLT_*).  Usually one of DGNLT_DMRS, 
 * DGNLT_INFORMIX, DGNLT_ODBC, DGNLT_ORACLE, DGNLT_RIS, DGNLT_SYBASE, 
 * or DGNLT_XBASE. 
 * @param nEntityNum indicator of the table referenced on target database.
 * @param nMSLink indicator of the record referenced on target table.
 *
 * @return -1 on failure, or the link index. 
 */ 

int DGNAddMSLink( DGNHandle hDGN, DGNElemCore *psElement, 
                  int nLinkageType, int nEntityNum, int nMSLink )

{
    return -1;
}

/************************************************************************/
/*                         DGNAddRawAttrLink()                          */
/************************************************************************/

/**
 * Add a raw attribute linkage to element.
 *
 * Given a raw data buffer, append it to this element as an attribute linkage
 * without trying to interprete the linkage data.   
 *
 * The target element must already have raw_data loaded, and it will be 
 * resized (see DGNResizeElement()) as needed for the new attribute data. 
 * Note that the element is not written to disk immediate.  Use 
 * DGNWriteElement() for that. 
 *
 * This function will take care of updating the "totlength" field of 
 * complex chain or shape headers to account for the extra attribute space
 * consumed in the header element.
 *
 * @param hDGN the file to which the element corresponds.
 * @param psElement the element being updated.
 * @param nLinkSize the size of the linkage in bytes. 
 * @param pabyRawLinkData the raw linkage data (nLinkSize bytes worth). 
 *
 * @return -1 on failure, or the link index. 
 */ 

int DGNAddRawAttrLink( DGNHandle hDGN, DGNElemCore *psElement, 
                       int nLinkSize, unsigned char *pabyRawLinkData )

{
    return -1;
}

/************************************************************************/
/*                        DGNAddShapeFileInfo()                         */
/************************************************************************/

/**
 * Add a shape fill attribute linkage.
 *
 * The target element must already have raw_data loaded, and it will be 
 * resized (see DGNResizeElement()) as needed for the new attribute data. 
 * Note that the element is not written to disk immediate.  Use 
 * DGNWriteElement() for that. 
 *
 * @param hDGN the file to which the element corresponds.
 * @param psElement the element being updated.
 * @param nColor fill color (color index from palette).
 *
 * @return -1 on failure, or the link index. 
 */ 

int DGNAddFShapeFillInfo( DGNHandle hDGN, DGNElemCore *psElement, 
                          int nColor )

{
    return -1;
}
