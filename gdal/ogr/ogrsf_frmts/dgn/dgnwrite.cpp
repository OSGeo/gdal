/******************************************************************************
 * $Id$
 *
 * Project:  Microstation DGN Access Library
 * Purpose:  DGN Access functions related to writing DGN elements.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

static void DGNPointToInt( DGNInfo *psDGN, DGNPoint *psPoint, 
                           unsigned char *pabyTarget );

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
    DGNInfo     *psDGN = (DGNInfo *) hDGN;

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

    psElement->raw_data[2] = (unsigned char) (nWords % 256);
    psElement->raw_data[3] = (unsigned char) (nWords / 256);

    return TRUE;
}

/************************************************************************/
/*                          DGNWriteElement()                           */
/************************************************************************/

/** 
 * Write element to file. 
 *
 * Only elements with "raw_data" loaded may be written.  This should
 * include elements created with the various DGNCreate*() functions, and
 * those read from the file with the DGNO_CAPTURE_RAW_DATA flag turned on
 * with DGNSetOptions(). 
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
    DGNInfo     *psDGN = (DGNInfo *) hDGN;

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
        psInfo->level = (unsigned char) psElement->level;
        psInfo->type = (unsigned char) psElement->type;
        psInfo->stype = (unsigned char) psElement->stype;
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
 * @param nSubUnitsPerMasterUnit the number of subunits in one master unit.
 * @param nUORPerSubUnit the number of UOR (units of resolution) per subunit.
 * @param pszMasterUnits the name of the master units (2 characters). 
 * @param pszSubUnits the name of the subunits (2 characters). 
 */

DGNHandle
      DGNCreate( const char *pszNewFilename, const char *pszSeedFile, 
                 int nCreationFlags, 
                 double dfOriginX, double dfOriginY, double dfOriginZ,
                 int nSubUnitsPerMasterUnit, int nUORPerSubUnit, 
                 const char *pszMasterUnits, const char *pszSubUnits )

{
    DGNInfo *psSeed, *psDGN;
    FILE    *fpNew;
    DGNElemCore *psSrcTCB;

/* -------------------------------------------------------------------- */
/*      Open seed file, and read TCB element.                           */
/* -------------------------------------------------------------------- */
    psSeed = (DGNInfo *) DGNOpen( pszSeedFile, FALSE );
    if( psSeed == NULL )
        return NULL;

    DGNSetOptions( psSeed, DGNO_CAPTURE_RAW_DATA );
    
    psSrcTCB = DGNReadElement( psSeed );

    CPLAssert( psSrcTCB->raw_bytes >= 1536 );
    
/* -------------------------------------------------------------------- */
/*      Open output file.                                               */
/* -------------------------------------------------------------------- */
    fpNew = VSIFOpen( pszNewFilename, "wb" );
    if( fpNew == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to open output file: %s", pszNewFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Modify TCB appropriately for the output file.                   */
/* -------------------------------------------------------------------- */
    GByte *pabyRawTCB = (GByte *) CPLMalloc(psSrcTCB->raw_bytes);
    
    memcpy( pabyRawTCB, psSrcTCB->raw_data, psSrcTCB->raw_bytes );

    if( !(nCreationFlags & DGNCF_USE_SEED_UNITS) )
    {
        memcpy( pabyRawTCB+1120, pszMasterUnits, 2 );
        memcpy( pabyRawTCB+1122, pszSubUnits, 2 );

        DGN_WRITE_INT32( nUORPerSubUnit, pabyRawTCB+1116 );
        DGN_WRITE_INT32( nSubUnitsPerMasterUnit,pabyRawTCB+1112);
    }
    else
    {
        nUORPerSubUnit = DGN_INT32( pabyRawTCB+1116 );
        nSubUnitsPerMasterUnit = DGN_INT32( pabyRawTCB+1112 );
    }

    if( !(nCreationFlags & DGNCF_USE_SEED_ORIGIN) )
    {
        dfOriginX *= (nUORPerSubUnit * nSubUnitsPerMasterUnit);
        dfOriginY *= (nUORPerSubUnit * nSubUnitsPerMasterUnit);
        dfOriginZ *= (nUORPerSubUnit * nSubUnitsPerMasterUnit);

        memcpy( pabyRawTCB+1240, &dfOriginX, 8 );
        memcpy( pabyRawTCB+1248, &dfOriginY, 8 );
        memcpy( pabyRawTCB+1256, &dfOriginZ, 8 );

        IEEE2DGNDouble( pabyRawTCB+1240 );
        IEEE2DGNDouble( pabyRawTCB+1248 );
        IEEE2DGNDouble( pabyRawTCB+1256 );
    }

/* -------------------------------------------------------------------- */
/*      Write TCB and EOF to new file.                                  */
/* -------------------------------------------------------------------- */
    unsigned char abyEOF[2];
    
    VSIFWrite( pabyRawTCB, psSrcTCB->raw_bytes, 1, fpNew );
    CPLFree( pabyRawTCB );

    abyEOF[0] = 0xff;
    abyEOF[1] = 0xff;
    
    VSIFWrite( abyEOF, 2, 1, fpNew );

    DGNFreeElement( psSeed, psSrcTCB );

/* -------------------------------------------------------------------- */
/*      Close and re-open using DGN API.                                */
/* -------------------------------------------------------------------- */
    VSIFClose( fpNew );

    psDGN = (DGNInfo *) DGNOpen( pszNewFilename, TRUE );

/* -------------------------------------------------------------------- */
/*      Now copy over elements according to options in effect.          */
/* -------------------------------------------------------------------- */
    DGNElemCore *psSrcElement, *psDstElement;

    while( (psSrcElement = DGNReadElement( psSeed )) != NULL )
    {
        if( (nCreationFlags & DGNCF_COPY_WHOLE_SEED_FILE)
            || (psSrcElement->stype == DGNST_COLORTABLE 
                && nCreationFlags & DGNCF_COPY_SEED_FILE_COLOR_TABLE)
            || psSrcElement->element_id <= 2 )
        {
            psDstElement = DGNCloneElement( psSeed, psDGN, psSrcElement );
            DGNWriteElement( psDGN, psDstElement );
            DGNFreeElement( psDGN, psDstElement );
        }

        DGNFreeElement( psSeed, psSrcElement );
    }

    DGNClose( psSeed );
    
    return psDGN;
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

DGNElemCore *DGNCloneElement( CPL_UNUSED DGNHandle hDGNSrc,
                              DGNHandle hDGNDst,
                              DGNElemCore *psSrcElement )

{
    DGNElemCore *psClone = NULL;

    DGNLoadTCB( hDGNDst );

/* -------------------------------------------------------------------- */
/*      Per structure specific copying.  The core is fixed up later.    */
/* -------------------------------------------------------------------- */
    if( psSrcElement->stype == DGNST_CORE )
    {
        psClone = (DGNElemCore *) CPLMalloc(sizeof(DGNElemCore));
        memcpy( psClone, psSrcElement, sizeof(DGNElemCore) );
    }
    else if( psSrcElement->stype == DGNST_MULTIPOINT )
    {
        DGNElemMultiPoint *psMP, *psSrcMP;
        int               nSize;

        psSrcMP = (DGNElemMultiPoint *) psSrcElement;

        nSize = sizeof(DGNElemMultiPoint) 
            + sizeof(DGNPoint) * (psSrcMP->num_vertices-2);

        psMP = (DGNElemMultiPoint *) CPLMalloc( nSize );
        memcpy( psMP, psSrcElement, nSize );

        psClone = (DGNElemCore *) psMP;
    }
    else if( psSrcElement->stype == DGNST_ARC )
    {
        DGNElemArc *psArc;

        psArc = (DGNElemArc *) CPLMalloc(sizeof(DGNElemArc));
        memcpy( psArc, psSrcElement, sizeof(DGNElemArc) );

        psClone = (DGNElemCore *) psArc;
    }
    else if( psSrcElement->stype == DGNST_TEXT )
    {
        DGNElemText       *psText, *psSrcText;
        int               nSize;

        psSrcText = (DGNElemText *) psSrcElement;
        nSize = sizeof(DGNElemText) + strlen(psSrcText->string);

        psText = (DGNElemText *) CPLMalloc( nSize );
        memcpy( psText, psSrcElement, nSize );

        psClone = (DGNElemCore *) psText;
    }
    else if( psSrcElement->stype == DGNST_TEXT_NODE )
    {
        DGNElemTextNode *psNode;

        psNode = (DGNElemTextNode *) 
            CPLMalloc(sizeof(DGNElemTextNode));
        memcpy( psNode, psSrcElement, sizeof(DGNElemTextNode) );

        psClone = (DGNElemCore *) psNode;
    }
    else if( psSrcElement->stype == DGNST_COMPLEX_HEADER )
    {
        DGNElemComplexHeader *psCH;

        psCH = (DGNElemComplexHeader *) 
            CPLMalloc(sizeof(DGNElemComplexHeader));
        memcpy( psCH, psSrcElement, sizeof(DGNElemComplexHeader) );

        psClone = (DGNElemCore *) psCH;
    }
    else if( psSrcElement->stype == DGNST_COLORTABLE )
    {
        DGNElemColorTable *psCT;

        psCT = (DGNElemColorTable *) CPLMalloc(sizeof(DGNElemColorTable));
        memcpy( psCT, psSrcElement, sizeof(DGNElemColorTable) );

        psClone = (DGNElemCore *) psCT;
    }
    else if( psSrcElement->stype == DGNST_TCB )
    {
        DGNElemTCB *psTCB;

        psTCB = (DGNElemTCB *) CPLMalloc(sizeof(DGNElemTCB));
        memcpy( psTCB, psSrcElement, sizeof(DGNElemTCB) );

        psClone = (DGNElemCore *) psTCB;
    }
    else if( psSrcElement->stype == DGNST_CELL_HEADER )
    {
        DGNElemCellHeader *psCH;

        psCH = (DGNElemCellHeader *) CPLMalloc(sizeof(DGNElemCellHeader));
        memcpy( psCH, psSrcElement, sizeof(DGNElemCellHeader) );

        psClone = (DGNElemCore *) psCH;
    }
    else if( psSrcElement->stype == DGNST_CELL_LIBRARY )
    {
        DGNElemCellLibrary *psCL;
        
        psCL = (DGNElemCellLibrary *) CPLMalloc(sizeof(DGNElemCellLibrary));
        memcpy( psCL, psSrcElement, sizeof(DGNElemCellLibrary) );

        psClone = (DGNElemCore *) psCL;
    }
    else if( psSrcElement->stype == DGNST_TAG_VALUE )
    {
        DGNElemTagValue *psTV;
        
        psTV = (DGNElemTagValue *) CPLMalloc(sizeof(DGNElemTagValue));
        memcpy( psTV, psSrcElement, sizeof(DGNElemTagValue) );

        if( psTV->tagType == 1 )
            psTV->tagValue.string = CPLStrdup( psTV->tagValue.string );

        psClone = (DGNElemCore *) psTV;
    }
    else if( psSrcElement->stype == DGNST_TAG_SET )
    {
        DGNElemTagSet *psTS;
        int iTag;
        DGNTagDef *pasTagList;
        
        psTS = (DGNElemTagSet *) CPLMalloc(sizeof(DGNElemTagSet));
        memcpy( psTS, psSrcElement, sizeof(DGNElemTagSet) );

        psTS->tagSetName = CPLStrdup( psTS->tagSetName );
        
        pasTagList = (DGNTagDef *) 
            CPLMalloc( sizeof(DGNTagDef) * psTS->tagCount );
        memcpy( pasTagList, psTS->tagList, 
                sizeof(DGNTagDef) * psTS->tagCount );

        for( iTag = 0; iTag < psTS->tagCount; iTag++ )
        {
            pasTagList[iTag].name = CPLStrdup( pasTagList[iTag].name );
            pasTagList[iTag].prompt = CPLStrdup( pasTagList[iTag].prompt );
            if( pasTagList[iTag].type == 1 )
                pasTagList[iTag].defaultValue.string = 
                    CPLStrdup( pasTagList[iTag].defaultValue.string);
        }

        psTS->tagList = pasTagList;
        psClone = (DGNElemCore *) psTS;
    }
    else if( psSrcElement->stype == DGNST_CONE )
    {
        DGNElemCone *psCone;

        psCone = (DGNElemCone *) CPLMalloc(sizeof(DGNElemCone));
        memcpy( psCone, psSrcElement, sizeof(DGNElemCone) );

        psClone = (DGNElemCore *) psCone;
    }
    else if( psSrcElement->stype == DGNST_BSPLINE_SURFACE_HEADER )
    {
        DGNElemBSplineSurfaceHeader *psSurface;

        psSurface = (DGNElemBSplineSurfaceHeader *) 
          CPLMalloc(sizeof(DGNElemBSplineSurfaceHeader));
        memcpy( psSurface, psSrcElement, sizeof(DGNElemBSplineSurfaceHeader) );

        psClone = (DGNElemCore *) psSurface;
    }
    else if( psSrcElement->stype == DGNST_BSPLINE_CURVE_HEADER )
    {
        DGNElemBSplineCurveHeader *psCurve;

        psCurve = (DGNElemBSplineCurveHeader *) 
          CPLMalloc(sizeof(DGNElemBSplineCurveHeader));
        memcpy( psCurve, psSrcElement, sizeof(DGNElemBSplineCurveHeader) );

        psClone = (DGNElemCore *) psCurve;
    }
    else if( psSrcElement->stype == DGNST_BSPLINE_SURFACE_BOUNDARY )
    {
        DGNElemBSplineSurfaceBoundary *psBSB, *psSrcBSB;
        int               nSize;

        psSrcBSB = (DGNElemBSplineSurfaceBoundary *) psSrcElement;

        nSize = sizeof(DGNElemBSplineSurfaceBoundary) 
            + sizeof(DGNPoint) * (psSrcBSB->numverts-1);

        psBSB = (DGNElemBSplineSurfaceBoundary *) CPLMalloc( nSize );
        memcpy( psBSB, psSrcElement, nSize );

        psClone = (DGNElemCore *) psBSB;
    }
    else if( psSrcElement->stype == DGNST_KNOT_WEIGHT )
    {
        DGNElemKnotWeight *psArray /* , *psSrcArray*/;
        int               nSize, numelems;

        // FIXME: Is it OK to assume that the # of elements corresponds
        // directly to the element size? kintel 20051218.
        numelems = (psSrcElement->size - 36 - psSrcElement->attr_bytes)/4;

        /* psSrcArray = (DGNElemKnotWeight *) psSrcElement; */

        nSize = sizeof(DGNElemKnotWeight) + sizeof(long) * (numelems-1);

        psArray = (DGNElemKnotWeight *) CPLMalloc( nSize );
        memcpy( psArray, psSrcElement, nSize );

        psClone = (DGNElemCore *) psArray;
    }
    else if( psSrcElement->stype == DGNST_SHARED_CELL_DEFN )
    {
        DGNElemSharedCellDefn *psCH;

        psCH = (DGNElemSharedCellDefn *)CPLMalloc(sizeof(DGNElemSharedCellDefn));
        memcpy( psCH, psSrcElement, sizeof(DGNElemSharedCellDefn) );

        psClone = (DGNElemCore *) psCH;
    }
    else
    {
        CPLAssert( FALSE );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Copy core raw data, and attributes.                             */
/* -------------------------------------------------------------------- */
    if( psClone->raw_bytes != 0 )
    {
        psClone->raw_data = (unsigned char *) CPLMalloc(psClone->raw_bytes);
        memcpy( psClone->raw_data, psSrcElement->raw_data, 
                psClone->raw_bytes );
    }

    if( psClone->attr_bytes != 0 )
    {
        psClone->attr_data = (unsigned char *) CPLMalloc(psClone->attr_bytes);
        memcpy( psClone->attr_data, psSrcElement->attr_data, 
                psClone->attr_bytes );
    }

/* -------------------------------------------------------------------- */
/*      Clear location and id information.                              */
/* -------------------------------------------------------------------- */
    psClone->offset = -1;
    psClone->element_id = -1;

    return psClone;
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
    psElement->level = nLevel;
    psElement->graphic_group = nGraphicGroup;
    psElement->color = nColor;
    psElement->weight = nWeight;
    psElement->style = nStyle;

    return DGNUpdateElemCoreExtended( hDGN, psElement );
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


int DGNUpdateElemCoreExtended( CPL_UNUSED DGNHandle hDGN,
                               DGNElemCore *psElement )
{
    GByte *rd = psElement->raw_data;
    int   nWords = (psElement->raw_bytes / 2) - 2;

    if( psElement->raw_data == NULL
        || psElement->raw_bytes < 36 )
    {
        CPLAssert( FALSE );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Setup first four bytes.                                         */
/* -------------------------------------------------------------------- */
    rd[0] = (GByte) psElement->level;
    if( psElement->complex )
        rd[0] |= 0x80;

    rd[1] = (GByte) psElement->type;
    if( psElement->deleted )
        rd[1] |= 0x80;

    rd[2] = (GByte) (nWords % 256);
    rd[3] = (GByte) (nWords / 256);

/* -------------------------------------------------------------------- */
/*      If the attribute offset hasn't been set, set it now under       */
/*      the assumption it should point to the end of the element.       */
/* -------------------------------------------------------------------- */
    if( psElement->raw_data[30] == 0 && psElement->raw_data[31] == 0 )
    {
        int     nAttIndex = (psElement->raw_bytes - 32) / 2;
        
        psElement->raw_data[30] = (GByte) (nAttIndex % 256);
        psElement->raw_data[31] = (GByte) (nAttIndex / 256);
    }
/* -------------------------------------------------------------------- */
/*      Handle the graphic properties.                                  */
/* -------------------------------------------------------------------- */
    if( psElement->raw_bytes > 36 && DGNElemTypeHasDispHdr( psElement->type ) )
    {
        rd[28] = (GByte) (psElement->graphic_group % 256);
        rd[29] = (GByte) (psElement->graphic_group / 256);
        rd[32] = (GByte) (psElement->properties % 256);
        rd[33] = (GByte) (psElement->properties / 256);
        rd[34] = (GByte) (psElement->style | (psElement->weight << 3));
        rd[35] = (GByte) psElement->color;
    }
    
    return TRUE;
}

/************************************************************************/
/*                         DGNInitializeElemCore()                      */
/************************************************************************/

static void DGNInitializeElemCore( CPL_UNUSED DGNHandle hDGN,
                                   DGNElemCore *psElement )
{
    memset( psElement, 0, sizeof(DGNElemCore) );

    psElement->offset = -1;
    psElement->element_id = -1;
}

/************************************************************************/
/*                           DGNWriteBounds()                           */
/*                                                                      */
/*      Write bounds to element raw data.                               */
/************************************************************************/

static void DGNWriteBounds( DGNInfo *psInfo, DGNElemCore *psElement,
                            DGNPoint *psMin, DGNPoint *psMax )

{
    CPLAssert( psElement->raw_bytes >= 28 );

    DGNInverseTransformPointToInt( psInfo, psMin, psElement->raw_data + 4 );
    DGNInverseTransformPointToInt( psInfo, psMax, psElement->raw_data + 16 );

    /* convert from twos complement to "binary offset" format. */

    psElement->raw_data[5] ^= 0x80;
    psElement->raw_data[9] ^= 0x80;
    psElement->raw_data[13] ^= 0x80;
    psElement->raw_data[17] ^= 0x80;
    psElement->raw_data[21] ^= 0x80;
    psElement->raw_data[25] ^= 0x80;
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
 * DGNT_LINE, DGNT_LINE_STRING, DGNT_SHAPE, DGNT_CURVE or DGNT_BSPLINE_POLE. 
 * @param nPointCount the number of points in the pasVertices list.
 * @param pasVertices the list of points to be written. 
 *
 * @return the new element (a DGNElemMultiPoint structure) or NULL on failure.
 */

DGNElemCore *DGNCreateMultiPointElem( DGNHandle hDGN, int nType, 
                                      int nPointCount, DGNPoint *pasVertices )

{
    DGNElemMultiPoint *psMP;
    DGNElemCore *psCore;
    DGNInfo *psDGN = (DGNInfo *) hDGN;
    int i;
    DGNPoint sMin, sMax;

    CPLAssert( nType == DGNT_LINE 
               || nType == DGNT_LINE_STRING
               || nType == DGNT_SHAPE
               || nType == DGNT_CURVE
               || nType == DGNT_BSPLINE_POLE );

    DGNLoadTCB( hDGN );

/* -------------------------------------------------------------------- */
/*      Is this too many vertices to write to a single element?         */
/* -------------------------------------------------------------------- */
    if( nPointCount > 101 )
    {
        CPLError( CE_Failure, CPLE_ElementTooBig, 
                  "Attempt to create %s element with %d points failed.\n"
                  "Element would be too large.",
                  DGNTypeToName( nType ), nPointCount );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Allocate element.                                               */
/* -------------------------------------------------------------------- */
    psMP = (DGNElemMultiPoint *) 
        CPLCalloc( sizeof(DGNElemMultiPoint)
                   + sizeof(DGNPoint) * (nPointCount-2), 1 );
    psCore = &(psMP->core);

    DGNInitializeElemCore( hDGN, psCore );
    psCore->stype = DGNST_MULTIPOINT;
    psCore->type = nType;

/* -------------------------------------------------------------------- */
/*      Set multipoint specific information in the structure.           */
/* -------------------------------------------------------------------- */
    psMP->num_vertices = nPointCount;
    memcpy( psMP->vertices + 0, pasVertices, sizeof(DGNPoint) * nPointCount );

/* -------------------------------------------------------------------- */
/*      Setup Raw data for the multipoint section.                      */
/* -------------------------------------------------------------------- */
    if( nType == DGNT_LINE )
    {
        CPLAssert( nPointCount == 2 );

        psCore->raw_bytes = 36 + psDGN->dimension* 4 * nPointCount;

        psCore->raw_data = (unsigned char*) CPLCalloc(psCore->raw_bytes,1);

        DGNInverseTransformPointToInt( psDGN, pasVertices + 0, 
                                       psCore->raw_data + 36 );
        DGNInverseTransformPointToInt( psDGN, pasVertices + 1, 
                                       psCore->raw_data + 36
                                       + psDGN->dimension * 4 );
    }
    else
    {
        CPLAssert( nPointCount >= 2 );

        psCore->raw_bytes = 38 + psDGN->dimension * 4 * nPointCount;
        psCore->raw_data = (unsigned char*) CPLCalloc(psCore->raw_bytes,1);

        psCore->raw_data[36] = (unsigned char) (nPointCount % 256);
        psCore->raw_data[37] = (unsigned char) (nPointCount/256);

        for( i = 0; i < nPointCount; i++ )
            DGNInverseTransformPointToInt( psDGN, pasVertices + i, 
                                           psCore->raw_data + 38 
                                           + psDGN->dimension * i * 4 );
    }
    
/* -------------------------------------------------------------------- */
/*      Set the core raw data, including the bounds.                    */
/* -------------------------------------------------------------------- */
    DGNUpdateElemCoreExtended( hDGN, psCore );

    sMin = sMax = pasVertices[0];
    for( i = 1; i < nPointCount; i++ )
    {
        sMin.x = MIN(pasVertices[i].x,sMin.x);
        sMin.y = MIN(pasVertices[i].y,sMin.y);
        sMin.z = MIN(pasVertices[i].z,sMin.z);
        sMax.x = MAX(pasVertices[i].x,sMax.x);
        sMax.y = MAX(pasVertices[i].y,sMax.y);
        sMax.z = MAX(pasVertices[i].z,sMax.z);
    }

    DGNWriteBounds( psDGN, psCore, &sMin, &sMax );
    
    return psCore;
}

/************************************************************************/
/*                         DGNCreateArcElem2D()                         */
/************************************************************************/

DGNElemCore *
DGNCreateArcElem2D( DGNHandle hDGN, int nType, 
                    double dfOriginX, double dfOriginY,
                    double dfPrimaryAxis, double dfSecondaryAxis, 
                    double dfRotation, 
                    double dfStartAngle, double dfSweepAngle )

{
    return DGNCreateArcElem( hDGN, nType, dfOriginX, dfOriginY, 0.0, 
                             dfPrimaryAxis, dfSecondaryAxis, 
                             dfStartAngle, dfSweepAngle, 
                             dfRotation, NULL );
}
                                 
/************************************************************************/
/*                          DGNCreateArcElem()                          */
/************************************************************************/

/**
 * Create Arc or Ellipse element.
 *
 * Create a new 2D or 3D arc or ellipse element.  The start angle, and sweep 
 * angle are ignored for DGNT_ELLIPSE but used for DGNT_ARC.
 *
 * The newly created element will still need to be written to file using
 * DGNWriteElement(). Also the level and other core values will be defaulted.
 * Use DGNUpdateElemCore() on the element before writing to set these values.
 *
 * @param hDGN the DGN file on which the element will eventually be written.
 * @param nType either DGNT_ELLIPSE or DGNT_ARC to select element type. 
 * @param dfOriginX the origin (center of rotation) of the arc (X).
 * @param dfOriginY the origin (center of rotation) of the arc (Y).
 * @param dfOriginZ the origin (center of rotation) of the arc (Y).
 * @param dfPrimaryAxis the length of the primary axis.
 * @param dfSecondaryAxis the length of the secondary axis. 
 * @param dfStartAngle start angle, degrees counterclockwise of primary axis.
 * @param dfSweepAngle sweep angle, degrees
 * @param dfRotation Counterclockwise rotation in degrees. 
 * @param panQuaternion 3D orientation quaternion (NULL to use rotation).
 * 
 * @return the new element (DGNElemArc) or NULL on failure.
 */

DGNElemCore *
DGNCreateArcElem( DGNHandle hDGN, int nType, 
                  double dfOriginX, double dfOriginY, double dfOriginZ,
                  double dfPrimaryAxis, double dfSecondaryAxis, 
                  double dfStartAngle, double dfSweepAngle,
                  double dfRotation, int *panQuaternion )

{
    DGNElemArc *psArc;
    DGNElemCore *psCore;
    DGNInfo *psDGN = (DGNInfo *) hDGN;
    DGNPoint sMin, sMax, sOrigin;
    GInt32 nAngle;

    CPLAssert( nType == DGNT_ARC || nType == DGNT_ELLIPSE );

    DGNLoadTCB( hDGN );

/* -------------------------------------------------------------------- */
/*      Allocate element.                                               */
/* -------------------------------------------------------------------- */
    psArc = (DGNElemArc *) CPLCalloc( sizeof(DGNElemArc), 1 );
    psCore = &(psArc->core);

    DGNInitializeElemCore( hDGN, psCore );
    psCore->stype = DGNST_ARC;
    psCore->type = nType;

/* -------------------------------------------------------------------- */
/*      Set arc specific information in the structure.                  */
/* -------------------------------------------------------------------- */
    sOrigin.x = dfOriginX;
    sOrigin.y = dfOriginY;
    sOrigin.z = dfOriginZ;

    psArc->origin = sOrigin;
    psArc->primary_axis = dfPrimaryAxis;
    psArc->secondary_axis = dfSecondaryAxis;
    memset( psArc->quat, 0, sizeof(int) * 4 );
    psArc->startang = dfStartAngle;
    psArc->sweepang = dfSweepAngle;

    psArc->rotation = dfRotation;
    if( panQuaternion == NULL )
    {
        DGNRotationToQuaternion( dfRotation, psArc->quat );
    }
    else
    {
        memcpy( psArc->quat, panQuaternion, sizeof(int)*4 );
    }

/* -------------------------------------------------------------------- */
/*      Setup Raw data for the arc section.                             */
/* -------------------------------------------------------------------- */
    if( nType == DGNT_ARC )
    {
        double dfScaledAxis;

        if( psDGN->dimension == 3 )
            psCore->raw_bytes = 100;
        else
            psCore->raw_bytes = 80;
        psCore->raw_data = (unsigned char*) CPLCalloc(psCore->raw_bytes,1);

        /* start angle */
        nAngle = (int) (dfStartAngle * 360000.0);
        DGN_WRITE_INT32( nAngle, psCore->raw_data + 36 );

        /* sweep angle */
        if( dfSweepAngle < 0.0 )
        {
            nAngle = (int) (ABS(dfSweepAngle) * 360000.0);
            nAngle |= 0x80000000;
        }
        else if( dfSweepAngle > 364.9999 )
        {
            nAngle = 0;
        }
        else
        {
            nAngle = (int) (dfSweepAngle * 360000.0);
        }
        DGN_WRITE_INT32( nAngle, psCore->raw_data + 40 );

        /* axes */
        dfScaledAxis = dfPrimaryAxis / psDGN->scale;
        memcpy( psCore->raw_data + 44, &dfScaledAxis, 8 );
        IEEE2DGNDouble( psCore->raw_data + 44 );

        dfScaledAxis = dfSecondaryAxis / psDGN->scale;
        memcpy( psCore->raw_data + 52, &dfScaledAxis, 8 );
        IEEE2DGNDouble( psCore->raw_data + 52 );

        if( psDGN->dimension == 3 )
        {
            /* quaternion */
            DGN_WRITE_INT32( psArc->quat[0], psCore->raw_data + 60 );
            DGN_WRITE_INT32( psArc->quat[1], psCore->raw_data + 64 );
            DGN_WRITE_INT32( psArc->quat[2], psCore->raw_data + 68 );
            DGN_WRITE_INT32( psArc->quat[3], psCore->raw_data + 72 );
            
            /* origin */
            DGNInverseTransformPoint( psDGN, &sOrigin );
            memcpy( psCore->raw_data + 76, &(sOrigin.x), 8 );
            memcpy( psCore->raw_data + 84, &(sOrigin.y), 8 );
            memcpy( psCore->raw_data + 92, &(sOrigin.z), 8 );
            IEEE2DGNDouble( psCore->raw_data + 76 );
            IEEE2DGNDouble( psCore->raw_data + 84 );
            IEEE2DGNDouble( psCore->raw_data + 92 );
        }
        else
        {
            /* rotation */
            nAngle = (int) (dfRotation * 360000.0);
            DGN_WRITE_INT32( nAngle, psCore->raw_data + 60 );
            
            /* origin */
            DGNInverseTransformPoint( psDGN, &sOrigin );
            memcpy( psCore->raw_data + 64, &(sOrigin.x), 8 );
            memcpy( psCore->raw_data + 72, &(sOrigin.y), 8 );
            IEEE2DGNDouble( psCore->raw_data + 64 );
            IEEE2DGNDouble( psCore->raw_data + 72 );
        }
    }

/* -------------------------------------------------------------------- */
/*      Setup Raw data for the ellipse section.                         */
/* -------------------------------------------------------------------- */
    else
    {
        double dfScaledAxis;

        if( psDGN->dimension == 3 )
            psCore->raw_bytes = 92;
        else
            psCore->raw_bytes = 72;
        psCore->raw_data = (unsigned char*) CPLCalloc(psCore->raw_bytes,1);

        /* axes */
        dfScaledAxis = dfPrimaryAxis / psDGN->scale;
        memcpy( psCore->raw_data + 36, &dfScaledAxis, 8 );
        IEEE2DGNDouble( psCore->raw_data + 36 );

        dfScaledAxis = dfSecondaryAxis / psDGN->scale;
        memcpy( psCore->raw_data + 44, &dfScaledAxis, 8 );
        IEEE2DGNDouble( psCore->raw_data + 44 );

        if( psDGN->dimension == 3 )
        {
            /* quaternion */
            DGN_WRITE_INT32( psArc->quat[0], psCore->raw_data + 52 );
            DGN_WRITE_INT32( psArc->quat[1], psCore->raw_data + 56 );
            DGN_WRITE_INT32( psArc->quat[2], psCore->raw_data + 60 );
            DGN_WRITE_INT32( psArc->quat[3], psCore->raw_data + 64 );
            
            /* origin */
            DGNInverseTransformPoint( psDGN, &sOrigin );
            memcpy( psCore->raw_data + 68, &(sOrigin.x), 8 );
            memcpy( psCore->raw_data + 76, &(sOrigin.y), 8 );
            memcpy( psCore->raw_data + 84, &(sOrigin.z), 8 );
            IEEE2DGNDouble( psCore->raw_data + 68 );
            IEEE2DGNDouble( psCore->raw_data + 76 );
            IEEE2DGNDouble( psCore->raw_data + 84 );
        }
        else
        {
            /* rotation */
            nAngle = (int) (dfRotation * 360000.0);
            DGN_WRITE_INT32( nAngle, psCore->raw_data + 52 );
            
            /* origin */
            DGNInverseTransformPoint( psDGN, &sOrigin );
            memcpy( psCore->raw_data + 56, &(sOrigin.x), 8 );
            memcpy( psCore->raw_data + 64, &(sOrigin.y), 8 );
            IEEE2DGNDouble( psCore->raw_data + 56 );
            IEEE2DGNDouble( psCore->raw_data + 64 );
        }
        
        psArc->startang = 0.0;
        psArc->sweepang = 360.0;
    }
    
/* -------------------------------------------------------------------- */
/*      Set the core raw data, including the bounds.                    */
/* -------------------------------------------------------------------- */
    DGNUpdateElemCoreExtended( hDGN, psCore );

    sMin.x = dfOriginX - MAX(dfPrimaryAxis,dfSecondaryAxis);
    sMin.y = dfOriginY - MAX(dfPrimaryAxis,dfSecondaryAxis);
    sMin.z = dfOriginZ - MAX(dfPrimaryAxis,dfSecondaryAxis);
    sMax.x = dfOriginX + MAX(dfPrimaryAxis,dfSecondaryAxis);
    sMax.y = dfOriginY + MAX(dfPrimaryAxis,dfSecondaryAxis);
    sMax.z = dfOriginZ + MAX(dfPrimaryAxis,dfSecondaryAxis);

    DGNWriteBounds( psDGN, psCore, &sMin, &sMax );
    
    return psCore;
}
                                 
/************************************************************************/
/*                          DGNCreateConeElem()                         */
/************************************************************************/

/**
 * Create Cone element.
 *
 * Create a new 3D cone element.
 *
 * The newly created element will still need to be written to file using
 * DGNWriteElement(). Also the level and other core values will be defaulted.
 * Use DGNUpdateElemCore() on the element before writing to set these values.
 *
 * @param hDGN the DGN file on which the element will eventually be written.
 * @param dfCenter_1X the center of the first bounding circle (X).
 * @param dfCenter_1Y the center of the first bounding circle (Y).
 * @param dfCenter_1Z the center of the first bounding circle (Z).
 * @param dfRadius_1 the radius of the first bounding circle.
 * @param dfCenter_2X the center of the second bounding circle (X).
 * @param dfCenter_2Y the center of the second bounding circle (Y).
 * @param dfCenter_2Z the center of the second bounding circle (Z).
 * @param dfRadius_2 the radius of the second bounding circle.
 * @param panQuaternion 3D orientation quaternion (NULL for default orientation - circles parallel to the X-Y plane).
 * 
 * @return the new element (DGNElemCone) or NULL on failure.
 */

DGNElemCore *
DGNCreateConeElem( DGNHandle hDGN,
                   double dfCenter_1X, double dfCenter_1Y,
                   double dfCenter_1Z, double dfRadius_1,
                   double dfCenter_2X, double dfCenter_2Y,
                   double dfCenter_2Z, double dfRadius_2,
                   int *panQuaternion )
{
    DGNElemCone *psCone;
    DGNElemCore *psCore;
    DGNInfo *psDGN = (DGNInfo *) hDGN;
    DGNPoint sMin, sMax, sCenter_1, sCenter_2;
    double dfScaledRadius;

    DGNLoadTCB( hDGN );

/* -------------------------------------------------------------------- */
/*      Allocate element.                                               */
/* -------------------------------------------------------------------- */
    psCone = (DGNElemCone *) CPLCalloc( sizeof(DGNElemCone), 1 );
    psCore = &(psCone->core);

    DGNInitializeElemCore( hDGN, psCore );
    psCore->stype = DGNST_CONE;
    psCore->type = DGNT_CONE;

/* -------------------------------------------------------------------- */
/*      Set cone specific information in the structure.                 */
/* -------------------------------------------------------------------- */
    sCenter_1.x = dfCenter_1X;
    sCenter_1.y = dfCenter_1Y;
    sCenter_1.z = dfCenter_1Z;
    sCenter_2.x = dfCenter_2X;
    sCenter_2.y = dfCenter_2Y;
    sCenter_2.z = dfCenter_2Z;
    psCone->center_1 = sCenter_1;
    psCone->center_2 = sCenter_2;
    psCone->radius_1 = dfRadius_1;
    psCone->radius_2 = dfRadius_2;

    memset( psCone->quat, 0, sizeof(int) * 4 );
    if( panQuaternion != NULL )
    {
        memcpy( psCone->quat, panQuaternion, sizeof(int)*4 );
    }
    else {
      psCone->quat[0] = 1 << 31;
      psCone->quat[1] = 0;
      psCone->quat[2] = 0;
      psCone->quat[3] = 0;
    }

/* -------------------------------------------------------------------- */
/*      Setup Raw data for the cone.                                    */
/* -------------------------------------------------------------------- */
    psCore->raw_bytes = 118;
    psCore->raw_data = (unsigned char*) CPLCalloc(psCore->raw_bytes,1);

    /* unknown data */
    psCore->raw_data[36] = 0;
    psCore->raw_data[37] = 0;
    
    /* quaternion */
    DGN_WRITE_INT32( psCone->quat[0], psCore->raw_data + 38 );
    DGN_WRITE_INT32( psCone->quat[1], psCore->raw_data + 42 );
    DGN_WRITE_INT32( psCone->quat[2], psCore->raw_data + 46 );
    DGN_WRITE_INT32( psCone->quat[3], psCore->raw_data + 50 );

    /* center_1 */
    DGNInverseTransformPoint( psDGN, &sCenter_1 );
    memcpy( psCore->raw_data + 54, &sCenter_1.x, 8 );
    memcpy( psCore->raw_data + 62, &sCenter_1.y, 8 );
    memcpy( psCore->raw_data + 70, &sCenter_1.z, 8 );
    IEEE2DGNDouble( psCore->raw_data + 54 );
    IEEE2DGNDouble( psCore->raw_data + 62 );
    IEEE2DGNDouble( psCore->raw_data + 70 );

    /* radius_1 */
    dfScaledRadius = psCone->radius_1 / psDGN->scale;
    memcpy( psCore->raw_data + 78, &dfScaledRadius, 8 );
    IEEE2DGNDouble( psCore->raw_data + 78 );

    /* center_2 */
    DGNInverseTransformPoint( psDGN, &sCenter_2 );
    memcpy( psCore->raw_data + 86, &sCenter_2.x, 8 );
    memcpy( psCore->raw_data + 94, &sCenter_2.y, 8 );
    memcpy( psCore->raw_data + 102, &sCenter_2.z, 8 );
    IEEE2DGNDouble( psCore->raw_data + 86 );
    IEEE2DGNDouble( psCore->raw_data + 94 );
    IEEE2DGNDouble( psCore->raw_data + 102 );

    /* radius_2 */
    dfScaledRadius = psCone->radius_2 / psDGN->scale;
    memcpy( psCore->raw_data + 110, &dfScaledRadius, 8 );
    IEEE2DGNDouble( psCore->raw_data + 110 );
    
/* -------------------------------------------------------------------- */
/*      Set the core raw data, including the bounds.                    */
/* -------------------------------------------------------------------- */
    DGNUpdateElemCoreExtended( hDGN, psCore );

    //FIXME: Calculate bounds. Do we need to take the quaternion into account?
    // kintel 20030819

    // Old implementation attempt:
    // What if center_1.z > center_2.z ?
//     double largestRadius = 
//       psCone->radius_1>psCone->radius_2?psCone->radius_1:psCone->radius_2;
//     sMin.x = psCone->center_1.x-largestRadius;
//     sMin.y = psCone->center_1.y-largestRadius;
//     sMin.z = psCone->center_1.z;
//     sMax.x = psCone->center_2.x+largestRadius;
//     sMax.y = psCone->center_2.y+largestRadius;
//     sMax.z = psCone->center_2.z;

    DGNWriteBounds( psDGN, psCore, &sMin, &sMax );
    
    return psCore;
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
 * @param panQuaternion 3D orientation quaternion (NULL to use rotation).
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
                   double dfRotation, int *panQuaternion,
                   double dfOriginX, double dfOriginY, double dfOriginZ )

{
    DGNElemText *psText;
    DGNElemCore *psCore;
    DGNInfo *psDGN = (DGNInfo *) hDGN;
    DGNPoint sMin, sMax, sLowLeft, sLowRight, sUpLeft, sUpRight;
    GInt32 nIntValue, nBase;
    double length, height, diagonal;

    DGNLoadTCB( hDGN );

/* -------------------------------------------------------------------- */
/*      Allocate element.                                               */
/* -------------------------------------------------------------------- */
    psText = (DGNElemText *) 
        CPLCalloc( sizeof(DGNElemText)+strlen(pszText), 1 );
    psCore = &(psText->core);

    DGNInitializeElemCore( hDGN, psCore );
    psCore->stype = DGNST_TEXT;
    psCore->type = DGNT_TEXT;

/* -------------------------------------------------------------------- */
/*      Set arc specific information in the structure.                  */
/* -------------------------------------------------------------------- */
    psText->font_id = nFontId;
    psText->justification = nJustification;
    psText->length_mult = dfLengthMult;
    psText->height_mult = dfHeightMult;
    psText->rotation = dfRotation;
    psText->origin.x = dfOriginX;
    psText->origin.y = dfOriginY;
    psText->origin.z = dfOriginZ;
    strcpy( psText->string, pszText );

/* -------------------------------------------------------------------- */
/*      Setup Raw data for the text specific portion.                   */
/* -------------------------------------------------------------------- */
    if( psDGN->dimension == 2 )
        psCore->raw_bytes = 60 + strlen(pszText);
    else
        psCore->raw_bytes = 76 + strlen(pszText);

    psCore->raw_bytes += (psCore->raw_bytes % 2);
    psCore->raw_data = (unsigned char*) CPLCalloc(psCore->raw_bytes,1);

    psCore->raw_data[36] = (unsigned char) nFontId;
    psCore->raw_data[37] = (unsigned char) nJustification;

    nIntValue = (int) (dfLengthMult * 1000.0 / (psDGN->scale * 6.0) + 0.5);
    DGN_WRITE_INT32( nIntValue, psCore->raw_data + 38 );
    
    nIntValue = (int) (dfHeightMult * 1000.0 / (psDGN->scale * 6.0) + 0.5);
    DGN_WRITE_INT32( nIntValue, psCore->raw_data + 42 );

    if( psDGN->dimension == 2 )
    {
        nIntValue = (int) (dfRotation * 360000.0);
        DGN_WRITE_INT32( nIntValue, psCore->raw_data + 46 );

        DGNInverseTransformPointToInt( psDGN, &(psText->origin), 
                                       psCore->raw_data + 50 );

        nBase = 58;
    }
    else
    {
        int anQuaternion[4];

        if( panQuaternion == NULL )
            DGNRotationToQuaternion( dfRotation, anQuaternion );
        else
            memcpy( anQuaternion, panQuaternion, sizeof(int) * 4 );

        DGN_WRITE_INT32( anQuaternion[0], psCore->raw_data + 46 );
        DGN_WRITE_INT32( anQuaternion[1], psCore->raw_data + 50 );
        DGN_WRITE_INT32( anQuaternion[2], psCore->raw_data + 54 );
        DGN_WRITE_INT32( anQuaternion[3], psCore->raw_data + 58 );

        DGNInverseTransformPointToInt( psDGN, &(psText->origin), 
                                       psCore->raw_data + 62 );
        nBase = 74;
    }

    psCore->raw_data[nBase] = (unsigned char) strlen(pszText);
    psCore->raw_data[nBase+1] = 0; /* edflds? */
    memcpy( psCore->raw_data + nBase+2, pszText, strlen(pszText) );
    
/* -------------------------------------------------------------------- */
/*      Set the core raw data, including the bounds.                    */
/*                                                                      */
/*      Code contributed by Mart Kelder.                                */
/* -------------------------------------------------------------------- */
    DGNUpdateElemCoreExtended( hDGN, psCore );

    //calculate bounds if rotation is 0
    sMin.x = dfOriginX;
    sMin.y = dfOriginY;
    sMin.z = 0.0;
    sMax.x = dfOriginX + dfLengthMult * strlen(pszText);
    sMax.y = dfOriginY + dfHeightMult;
    sMax.z = 0.0;

    //calculate rotated bounding box coordinates
    length = sMax.x-sMin.x;
    height = sMax.y-sMin.y;
    diagonal=sqrt(length*length+height*height);
    sLowLeft.x=sMin.x;
    sLowLeft.y=sMin.y;
    sLowRight.x=sMin.x+cos(psText->rotation*PI/180.0)*length;
    sLowRight.y=sMin.y+sin(psText->rotation*PI/180.0)*length;
    sUpRight.x=sMin.x+cos((psText->rotation*PI/180.0)+atan(height/length))*diagonal;
    sUpRight.y=sMin.y+sin((psText->rotation*PI/180.0)+atan(height/length))*diagonal;
    sUpLeft.x=sMin.x+cos((psText->rotation+90.0)*PI/180.0)*height;
    sUpLeft.y=sMin.y+sin((psText->rotation+90.0)*PI/180.0)*height;

    //calculate new values for bounding box
    sMin.x=MIN(sLowLeft.x,MIN(sLowRight.x,MIN(sUpLeft.x,sUpRight.x)));
    sMin.y=MIN(sLowLeft.y,MIN(sLowRight.y,MIN(sUpLeft.y,sUpRight.y)));
    sMax.x=MAX(sLowLeft.x,MAX(sLowRight.x,MAX(sUpLeft.x,sUpRight.x)));
    sMax.y=MAX(sLowLeft.y,MAX(sLowRight.y,MAX(sUpLeft.y,sUpRight.y)));
    sMin.x = dfOriginX - dfLengthMult * strlen(pszText);
    sMin.y = dfOriginY - dfHeightMult;
    sMin.z = 0.0;
    sMax.x = dfOriginX + dfLengthMult * strlen(pszText);
    sMax.y = dfOriginY + dfHeightMult;
    sMax.z = 0.0;

    DGNWriteBounds( psDGN, psCore, &sMin, &sMax );
    
    return psCore;
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
 * @param abyColorInfo array of 256 color entries. The first is
 * the background color. 
 *
 * @return the new element (DGNElemColorTable) or NULL on failure. 
 */


DGNElemCore *
DGNCreateColorTableElem( DGNHandle hDGN, int nScreenFlag, 
                         GByte abyColorInfo[256][3] )

{
    DGNElemColorTable *psCT;
    DGNElemCore *psCore;

/* -------------------------------------------------------------------- */
/*      Allocate element.                                               */
/* -------------------------------------------------------------------- */
    psCT = (DGNElemColorTable *) CPLCalloc( sizeof(DGNElemColorTable), 1 );
    psCore = &(psCT->core);

    DGNInitializeElemCore( hDGN, psCore );
    psCore->stype = DGNST_COLORTABLE;
    psCore->type = DGNT_GROUP_DATA;
    psCore->level = DGN_GDL_COLOR_TABLE;

/* -------------------------------------------------------------------- */
/*      Set colortable specific information in the structure.           */
/* -------------------------------------------------------------------- */
    psCT->screen_flag = nScreenFlag;
    memcpy( psCT->color_info, abyColorInfo, 768 );

/* -------------------------------------------------------------------- */
/*      Setup Raw data for the color table specific portion.            */
/* -------------------------------------------------------------------- */
    psCore->raw_bytes = 806; /* FIXME: this is invalid : 806 < 41 + 783 (see below lines) */
    psCore->raw_data = (unsigned char*) CPLCalloc(psCore->raw_bytes,1);

    psCore->raw_data[36] = (unsigned char) (nScreenFlag % 256);
    psCore->raw_data[37] = (unsigned char) (nScreenFlag / 256);

    memcpy( psCore->raw_data + 38, abyColorInfo[255], 3 );
    memcpy( psCore->raw_data + 41, abyColorInfo, 783 );
    
/* -------------------------------------------------------------------- */
/*      Set the core raw data.                                          */
/* -------------------------------------------------------------------- */
    DGNUpdateElemCoreExtended( hDGN, psCore );
    
    return psCore;
}

/************************************************************************/
/*                     DGNCreateComplexHeaderElem()                     */
/************************************************************************/

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
 * @param nType DGNT_COMPLEX_CHAIN_HEADER or DGNT_COMPLEX_SHAPE_HEADER.
 * depending on whether the list is open or closed (last point equal to last)
 * or if the object represents a surface or a solid.
 * @param nTotLength the value of the totlength field in the element.
 * @param nNumElems the number of elements in the complex group not including
 * the header element. 
 *
 * @return the new element (DGNElemComplexHeader) or NULL on failure. 
 */
DGNElemCore *
DGNCreateComplexHeaderElem( DGNHandle hDGN, int nType, 
                            int nTotLength, int nNumElems )
{
    DGNElemComplexHeader *psCH;
    DGNElemCore *psCore;
    unsigned char abyRawZeroLinkage[8] = {0,0,0,0,0,0,0,0};

    CPLAssert( nType == DGNT_COMPLEX_CHAIN_HEADER 
               || nType == DGNT_COMPLEX_SHAPE_HEADER );

    DGNLoadTCB( hDGN );

/* -------------------------------------------------------------------- */
/*      Allocate element.                                               */
/* -------------------------------------------------------------------- */
    psCH = (DGNElemComplexHeader *) 
        CPLCalloc( sizeof(DGNElemComplexHeader), 1 );
    psCore = &(psCH->core);

    DGNInitializeElemCore( hDGN, psCore );
    psCore->complex = TRUE;
    psCore->stype = DGNST_COMPLEX_HEADER;
    psCore->type = nType;

/* -------------------------------------------------------------------- */
/*      Set complex header specific information in the structure.       */
/* -------------------------------------------------------------------- */
    psCH->totlength = nTotLength - 4;
    psCH->numelems = nNumElems;
    psCH->surftype = 0;
    psCH->boundelms = 0;

/* -------------------------------------------------------------------- */
/*      Setup Raw data for the complex specific portion.                */
/* -------------------------------------------------------------------- */
    psCore->raw_bytes = 40;
    psCore->raw_data = (unsigned char*) CPLCalloc(psCore->raw_bytes,1);

    psCore->raw_data[36] = (unsigned char) ((nTotLength-4) % 256);
    psCore->raw_data[37] = (unsigned char) ((nTotLength-4) / 256);
    psCore->raw_data[38] = (unsigned char) (nNumElems % 256);
    psCore->raw_data[39] = (unsigned char) (nNumElems / 256);

/* -------------------------------------------------------------------- */
/*      Set the core raw data.                                          */
/* -------------------------------------------------------------------- */
    DGNUpdateElemCoreExtended( hDGN, psCore );

/* -------------------------------------------------------------------- */
/*      Elements have to be at least 48 bytes long, so we have to       */
/*      add a dummy bit of attribute data to fill out the length.       */
/* -------------------------------------------------------------------- */
    DGNAddRawAttrLink( hDGN, psCore, 8, abyRawZeroLinkage );
    
    return psCore;
}

/************************************************************************/
/*                  DGNCreateComplexHeaderFromGroup()                   */
/************************************************************************/

/**
 * Create complex chain/shape header.
 *
 * This function is similar to DGNCreateComplexHeaderElem(), but it takes
 * care of computing the total size of the set of elements being written, 
 * and collecting the bounding extents.  It also takes care of some other
 * convenience issues, like marking all the member elements as complex, and 
 * setting the level based on the level of the member elements. 
 * 
 * @param hDGN the file on which the element will be written.
 * @param nType DGNT_COMPLEX_CHAIN_HEADER or DGNT_COMPLEX_SHAPE_HEADER.
 * depending on whether the list is open or closed (last point equal to last)
 * or if the object represents a surface or a solid.
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
    int         nTotalLength = 5;
    int         i, nLevel;
    DGNElemCore *psCH;
    DGNPoint    sMin = {0.0,0.0,0.0}, sMax = {0.0,0.0,0.0};

    DGNLoadTCB( hDGN );

    if( nNumElems < 1 || papsElems == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Need at least one element to form a complex group." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Collect the total size, and bounds.                             */
/* -------------------------------------------------------------------- */
    nLevel = papsElems[0]->level;

    for( i = 0; i < nNumElems; i++ )
    {
        DGNPoint sThisMin, sThisMax;

        nTotalLength += papsElems[i]->raw_bytes / 2;

        papsElems[i]->complex = TRUE;
        papsElems[i]->raw_data[0] |= 0x80;

        if( papsElems[i]->level != nLevel )
        {
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "Not all level values matching in a complex set group!");
        }

        DGNGetElementExtents( hDGN, papsElems[i], &sThisMin, &sThisMax );
        if( i == 0 )
        {
            sMin = sThisMin;
            sMax = sThisMax;
        }
        else
        {
            sMin.x = MIN(sMin.x,sThisMin.x);
            sMin.y = MIN(sMin.y,sThisMin.y);
            sMin.z = MIN(sMin.z,sThisMin.z);
            sMax.x = MAX(sMax.x,sThisMax.x);
            sMax.y = MAX(sMax.y,sThisMax.y);
            sMax.z = MAX(sMax.z,sThisMax.z);
        }
    }

/* -------------------------------------------------------------------- */
/*      Create the corresponding complex header.                        */
/* -------------------------------------------------------------------- */
    psCH = DGNCreateComplexHeaderElem( hDGN, nType, nTotalLength, nNumElems );
    DGNUpdateElemCore( hDGN, psCH, papsElems[0]->level, psCH->graphic_group,
                       psCH->color, psCH->weight, psCH->style );

    DGNWriteBounds( (DGNInfo *) hDGN, psCH, &sMin, &sMax );
    
    return psCH;
}

/************************************************************************/
/*                     DGNCreateSolidHeaderElem()                       */
/************************************************************************/

/**
 * Create 3D solid/surface.
 *
 * The newly created element will still need to be written to file using
 * DGNWriteElement(). Also the level and other core values will be defaulted.
 * Use DGNUpdateElemCore() on the element before writing to set these values.
 *
 * The nTotLength is the sum of the size of all elements in the solid
 * group plus 6.  The DGNCreateSolidHeaderFromGroup() can be used to build
 * a solid element from the members more conveniently.  
 *
 * @param hDGN the file on which the element will be written.
 * @param nType DGNT_3DSURFACE_HEADER or DGNT_3DSOLID_HEADER.
 * @param nSurfType the surface/solid type, one of DGNSUT_* or DGNSOT_*. 
 * @param nBoundElems the number of elements in each boundary. 
 * @param nTotLength the value of the totlength field in the element.
 * @param nNumElems the number of elements in the solid not including
 * the header element. 
 *
 * @return the new element (DGNElemComplexHeader) or NULL on failure. 
 */
DGNElemCore *
DGNCreateSolidHeaderElem( DGNHandle hDGN, int nType, int nSurfType,
                          int nBoundElems, int nTotLength, int nNumElems )
{
    DGNElemComplexHeader *psCH;
    DGNElemCore *psCore;
    unsigned char abyRawZeroLinkage[8] = {0,0,0,0,0,0,0,0};

    CPLAssert( nType == DGNT_3DSURFACE_HEADER 
               || nType == DGNT_3DSOLID_HEADER );

    DGNLoadTCB( hDGN );

/* -------------------------------------------------------------------- */
/*      Allocate element.                                               */
/* -------------------------------------------------------------------- */
    psCH = (DGNElemComplexHeader *) 
        CPLCalloc( sizeof(DGNElemComplexHeader), 1 );
    psCore = &(psCH->core);

    DGNInitializeElemCore( hDGN, psCore );
    psCore->complex = TRUE;
    psCore->stype = DGNST_COMPLEX_HEADER;
    psCore->type = nType;

/* -------------------------------------------------------------------- */
/*      Set solid header specific information in the structure.         */
/* -------------------------------------------------------------------- */
    psCH->totlength = nTotLength - 4;
    psCH->numelems = nNumElems;
    psCH->surftype = nSurfType;
    psCH->boundelms = nBoundElems;

/* -------------------------------------------------------------------- */
/*      Setup Raw data for the solid specific portion.                  */
/* -------------------------------------------------------------------- */
    psCore->raw_bytes = 42;

    psCore->raw_data = (unsigned char*) CPLCalloc(psCore->raw_bytes,1);

    psCore->raw_data[36] = (unsigned char) ((nTotLength-4) % 256);
    psCore->raw_data[37] = (unsigned char) ((nTotLength-4) / 256);
    psCore->raw_data[38] = (unsigned char) (nNumElems % 256);
    psCore->raw_data[39] = (unsigned char) (nNumElems / 256);
    psCore->raw_data[40] = (unsigned char) psCH->surftype;
    psCore->raw_data[41] = (unsigned char) psCH->boundelms - 1;

/* -------------------------------------------------------------------- */
/*      Set the core raw data.                                          */
/* -------------------------------------------------------------------- */
    DGNUpdateElemCoreExtended( hDGN, psCore );

/* -------------------------------------------------------------------- */
/*      Elements have to be at least 48 bytes long, so we have to       */
/*      add a dummy bit of attribute data to fill out the length.       */
/* -------------------------------------------------------------------- */
    DGNAddRawAttrLink( hDGN, psCore, 8, abyRawZeroLinkage );
    
    return psCore;
}

/************************************************************************/
/*                  DGNCreateSolidHeaderFromGroup()                     */
/************************************************************************/

/**
 * Create 3D solid/surface header.
 *
 * This function is similar to DGNCreateSolidHeaderElem(), but it takes
 * care of computing the total size of the set of elements being written, 
 * and collecting the bounding extents.  It also takes care of some other
 * convenience issues, like marking all the member elements as complex, and 
 * setting the level based on the level of the member elements. 
 * 
 * @param hDGN the file on which the element will be written.
 * @param nType DGNT_3DSURFACE_HEADER or DGNT_3DSOLID_HEADER.
 * @param nSurfType the surface/solid type, one of DGNSUT_* or DGNSOT_*. 
 * @param nBoundElems the number of boundary elements. 
 * @param nNumElems the number of elements in the solid not including
 * the header element. 
 * @param papsElems array of pointers to nNumElems elements in the solid.
 * Some updates may be made to these elements. 
 *
 * @return the new element (DGNElemComplexHeader) or NULL on failure. 
 */

DGNElemCore *
DGNCreateSolidHeaderFromGroup( DGNHandle hDGN, int nType, int nSurfType,
                               int nBoundElems, int nNumElems, 
                               DGNElemCore **papsElems )

{
    int         nTotalLength = 6;
    int         i, nLevel;
    DGNElemCore *psCH;
    DGNPoint    sMin = {0.0,0.0,0.0}, sMax = {0.0,0.0,0.0};

    DGNLoadTCB( hDGN );

    if( nNumElems < 1 || papsElems == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Need at least one element to form a solid." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Collect the total size, and bounds.                             */
/* -------------------------------------------------------------------- */
    nLevel = papsElems[0]->level;

    for( i = 0; i < nNumElems; i++ )
    {
        DGNPoint sThisMin, sThisMax;

        nTotalLength += papsElems[i]->raw_bytes / 2;

        papsElems[i]->complex = TRUE;
        papsElems[i]->raw_data[0] |= 0x80;

        if( papsElems[i]->level != nLevel )
        {
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "Not all level values matching in a complex set group!");
        }

        DGNGetElementExtents( hDGN, papsElems[i], &sThisMin, &sThisMax );
        if( i == 0 )
        {
            sMin = sThisMin;
            sMax = sThisMax;
        }
        else
        {
            sMin.x = MIN(sMin.x,sThisMin.x);
            sMin.y = MIN(sMin.y,sThisMin.y);
            sMin.z = MIN(sMin.z,sThisMin.z);
            sMax.x = MAX(sMax.x,sThisMax.x);
            sMax.y = MAX(sMax.y,sThisMax.y);
            sMax.z = MAX(sMax.z,sThisMax.z);
        }
    }

/* -------------------------------------------------------------------- */
/*      Create the corresponding solid header.                          */
/* -------------------------------------------------------------------- */
    psCH = DGNCreateSolidHeaderElem( hDGN, nType, nSurfType, nBoundElems, 
                                     nTotalLength, nNumElems );
    DGNUpdateElemCore( hDGN, psCH, papsElems[0]->level, psCH->graphic_group,
                       psCH->color, psCH->weight, psCH->style );

    DGNWriteBounds( (DGNInfo *) hDGN, psCH, &sMin, &sMax );
    
    return psCH;
}

/************************************************************************/
/*                      DGNCreateCellHeaderElem()                       */
/************************************************************************/

DGNElemCore CPL_DLL  *
DGNCreateCellHeaderElem( DGNHandle hDGN, int nTotLength, const char *pszName, 
                         short nClass, short *panLevels, 
                         DGNPoint *psRangeLow, DGNPoint *psRangeHigh, 
                         DGNPoint *psOrigin, double dfXScale, double dfYScale,
                         double dfRotation )

/**
 * Create cell header.
 *
 * The newly created element will still need to be written to file using
 * DGNWriteElement(). Also the level and other core values will be defaulted.
 * Use DGNUpdateElemCore() on the element before writing to set these values.
 *
 * Generally speaking the function DGNCreateCellHeaderFromGroup() should
 * be used instead of this function.
 *
 * @param hDGN the file handle on which the element is to be written.
 * @param nTotLength total length of cell in words not including the 38 bytes
 * of the cell header that occur before the totlength indicator. 
 * @param nClass the class value for the cell. 
 * @param panLevels an array of shorts holding the bit mask of levels in
 * effect for this cell.  This array should contain 4 shorts (64 bits). 
 * @param psRangeLow the cell diagonal origin in original cell file 
 * coordinates.
 * @param psRangeHigh the cell diagonal top left corner in original cell file 
 * coordinates.
 * @param psOrigin the origin of the cell in output file coordinates. 
 * @param dfXScale the amount of scaling applied in the X dimension in 
 * mapping from cell file coordinates to output file coordinates.
 * @param dfYScale the amount of scaling applied in the Y dimension in 
 * mapping from cell file coordinates to output file coordinates.
 * @param dfRotation the amount of rotation (degrees counterclockwise) in 
 * mapping from cell coordinates to output file coordinates. 
 *
 * @return the new element (DGNElemCellHeader) or NULL on failure. 
 */

{
    DGNElemCellHeader *psCH;
    DGNElemCore *psCore;
    DGNInfo *psInfo = (DGNInfo *) hDGN;

    DGNLoadTCB( hDGN );

/* -------------------------------------------------------------------- */
/*      Allocate element.                                               */
/* -------------------------------------------------------------------- */
    psCH = (DGNElemCellHeader *) CPLCalloc( sizeof(DGNElemCellHeader), 1 );
    psCore = &(psCH->core);

    DGNInitializeElemCore( hDGN, psCore );
    psCore->stype = DGNST_CELL_HEADER;
    psCore->type = DGNT_CELL_HEADER;

/* -------------------------------------------------------------------- */
/*      Set complex header specific information in the structure.       */
/* -------------------------------------------------------------------- */
    psCH->totlength = nTotLength;

/* -------------------------------------------------------------------- */
/*      Setup Raw data for the cell header specific portion.            */
/* -------------------------------------------------------------------- */
    if( psInfo->dimension == 2 )
        psCore->raw_bytes = 92;
    else
        psCore->raw_bytes = 124;
    psCore->raw_data = (unsigned char*) CPLCalloc(psCore->raw_bytes,1);

    psCore->raw_data[36] = (unsigned char) (nTotLength % 256);
    psCore->raw_data[37] = (unsigned char) (nTotLength / 256);
    
    DGNAsciiToRad50( pszName, (unsigned short *) (psCore->raw_data + 38) );
    if( strlen(pszName) > 3 )
        DGNAsciiToRad50( pszName+3, (unsigned short *) (psCore->raw_data+40) );

    psCore->raw_data[42] = (unsigned char) (nClass % 256);
    psCore->raw_data[43] = (unsigned char) (nClass / 256);

    memcpy( psCore->raw_data + 44, panLevels, 8 );

    if( psInfo->dimension == 2 )
    {
        DGNPointToInt( psInfo, psRangeLow, psCore->raw_data + 52 );
        DGNPointToInt( psInfo, psRangeHigh, psCore->raw_data+ 60 );
        
        DGNInverseTransformPointToInt( psInfo, psOrigin, 
                                       psCore->raw_data + 84 );
    }
    else
    {
        DGNPointToInt( psInfo, psRangeLow, psCore->raw_data + 52 );
        DGNPointToInt( psInfo, psRangeHigh, psCore->raw_data+ 64 );
        
        DGNInverseTransformPointToInt( psInfo, psOrigin, 
                                       psCore->raw_data + 112 );
    }

/* -------------------------------------------------------------------- */
/*      Produce a transformation matrix that approximates the           */
/*      requested scaling and rotation.                                 */
/* -------------------------------------------------------------------- */
    if( psInfo->dimension == 2 )
    {
        long anTrans[4];
        double cos_a = cos(-dfRotation * PI / 180.0);
        double sin_a = sin(-dfRotation * PI / 180.0);
        
        anTrans[0] = (long) (cos_a * dfXScale * 214748);
        anTrans[1] = (long) (sin_a * dfYScale * 214748);
        anTrans[2] = (long)(-sin_a * dfXScale * 214748);
        anTrans[3] = (long) (cos_a * dfYScale * 214748);
        
        DGN_WRITE_INT32( anTrans[0], psCore->raw_data + 68 );
        DGN_WRITE_INT32( anTrans[1], psCore->raw_data + 72 );
        DGN_WRITE_INT32( anTrans[2], psCore->raw_data + 76 );
        DGN_WRITE_INT32( anTrans[3], psCore->raw_data + 80 );
    }
    else
    {
    }

/* -------------------------------------------------------------------- */
/*      Set the core raw data.                                          */
/* -------------------------------------------------------------------- */
    DGNUpdateElemCoreExtended( hDGN, psCore );
    
    return psCore;
}

/************************************************************************/
/*                           DGNPointToInt()                            */
/*                                                                      */
/*      Convert a point directly to integer coordinates and write to    */
/*      the indicate memory location.  Intended to be used for the      */
/*      range section of the CELL HEADER.                               */
/************************************************************************/

static void DGNPointToInt( DGNInfo *psDGN, DGNPoint *psPoint, 
                           unsigned char *pabyTarget )

{
    double     adfCT[3];
    int        i;

    adfCT[0] = psPoint->x;
    adfCT[1] = psPoint->y;
    adfCT[2] = psPoint->z;

    for( i = 0; i < psDGN->dimension; i++ )
    {
        GInt32 nCTI;
        unsigned char *pabyCTI = (unsigned char *) &nCTI;

        nCTI = (GInt32) MAX(-2147483647,MIN(2147483647,adfCT[i]));
        
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
/*                    DGNCreateCellHeaderFromGroup()                    */
/************************************************************************/

/**
 * Create cell header from a group of elements.
 *
 * The newly created element will still need to be written to file using
 * DGNWriteElement(). Also the level and other core values will be defaulted.
 * Use DGNUpdateElemCore() on the element before writing to set these values.
 *
 * This function will compute the total length, bounding box, and diagonal
 * range values from the set of provided elements.  Note that the proper
 * diagonal range values will only be written if 1.0 is used for the x and y
 * scale values, and 0.0 for the rotation.  Use of other values will result
 * in incorrect scaling handles being presented to the user in Microstation
 * when they select the element.  
 *
 * @param hDGN the file handle on which the element is to be written.
 * @param nClass the class value for the cell. 
 * @param panLevels an array of shorts holding the bit mask of levels in
 * effect for this cell.  This array should contain 4 shorts (64 bits). 
 * This array would normally be passed in as NULL, and the function will
 * build a mask from the passed list of elements. 
 * @param psOrigin the origin of the cell in output file coordinates. 
 * @param dfXScale the amount of scaling applied in the X dimension in 
 * mapping from cell file coordinates to output file coordinates.
 * @param dfYScale the amount of scaling applied in the Y dimension in 
 * mapping from cell file coordinates to output file coordinates.
 * @param dfRotation the amount of rotation (degrees counterclockwise) in 
 * mapping from cell coordinates to output file coordinates. 
 *
 * @return the new element (DGNElemCellHeader) or NULL on failure. 
 */

DGNElemCore *
DGNCreateCellHeaderFromGroup( DGNHandle hDGN, const char *pszName, 
                              short nClass, short *panLevels, 
                              int nNumElems, DGNElemCore **papsElems,
                              DGNPoint *psOrigin, 
                              double dfXScale, double dfYScale,
                              double dfRotation )

{
    int         nTotalLength;
    int         i /* , nLevel */;
    DGNElemCore *psCH;
    DGNPoint    sMin={0.0,0.0,0.0}, sMax={0.0,0.0,0.0};
    unsigned char abyLevelsOccuring[8] = {0,0,0,0,0,0,0,0};
    DGNInfo *psInfo = (DGNInfo *) hDGN;

    DGNLoadTCB( hDGN );

    if( nNumElems < 1 || papsElems == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Need at least one element to form a cell." );
        return NULL;
    }

    if( psInfo->dimension == 2 )
        nTotalLength = 27;
    else
        nTotalLength = 43;

/* -------------------------------------------------------------------- */
/*      Collect the total size, and bounds.                             */
/* -------------------------------------------------------------------- */
    /* nLevel = papsElems[0]->level; */

    for( i = 0; i < nNumElems; i++ )
    {
        DGNPoint sThisMin, sThisMax;
        int  nLevel;

        nTotalLength += papsElems[i]->raw_bytes / 2;

        /* mark as complex */
        papsElems[i]->complex = TRUE;
        papsElems[i]->raw_data[0] |= 0x80;

        /* establish level */
        nLevel = papsElems[i]->level;
        nLevel = MAX(1,MIN(nLevel,64));
        abyLevelsOccuring[(nLevel-1) >> 3] |= (0x1 << ((nLevel-1)&0x7));
        
        DGNGetElementExtents( hDGN, papsElems[i], &sThisMin, &sThisMax );
        if( i == 0 )
        {
            sMin = sThisMin;
            sMax = sThisMax;
        }
        else
        {
            sMin.x = MIN(sMin.x,sThisMin.x);
            sMin.y = MIN(sMin.y,sThisMin.y);
            sMin.z = MIN(sMin.z,sThisMin.z);
            sMax.x = MAX(sMax.x,sThisMax.x);
            sMax.y = MAX(sMax.y,sThisMax.y);
            sMax.z = MAX(sMax.z,sThisMax.z);
        }
    }

/* -------------------------------------------------------------------- */
/*      It seems that the range needs to be adjusted according to       */
/*      the rotation and scaling.                                       */
/*                                                                      */
/*      NOTE: Omitting code ... this is already done in                 */
/*      DGNInverseTransformPoint() called from DGNWriteBounds().        */
/* -------------------------------------------------------------------- */
#ifdef notdef
    sMin.x -= psOrigin->x;
    sMin.y -= psOrigin->y;
    sMin.z -= psOrigin->z;
    sMax.x -= psOrigin->x;
    sMax.y -= psOrigin->y;
    sMax.z -= psOrigin->z;

    sMin.x /= ((DGNInfo *) hDGN)->scale;
    sMin.y /= ((DGNInfo *) hDGN)->scale;
    sMin.z /= ((DGNInfo *) hDGN)->scale;
    sMax.x /= ((DGNInfo *) hDGN)->scale;
    sMax.y /= ((DGNInfo *) hDGN)->scale;
    sMax.z /= ((DGNInfo *) hDGN)->scale;
#endif

/* -------------------------------------------------------------------- */
/*      Create the corresponding cell header.                           */
/* -------------------------------------------------------------------- */
    if( panLevels == NULL )
        panLevels = (short *) abyLevelsOccuring + 0;

    psCH = DGNCreateCellHeaderElem( hDGN, nTotalLength, pszName, 
                                    nClass, panLevels, 
                                    &sMin, &sMax, psOrigin, 
                                    dfXScale, dfYScale, dfRotation );
    DGNWriteBounds( (DGNInfo *) hDGN, psCH, &sMin, &sMax );
    
    return psCH;
    
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
    unsigned char abyLinkage[32];
    int           nLinkageSize;

    if( nLinkageType == DGNLT_DMRS )
    {
        nLinkageSize = 8;
        abyLinkage[0] = 0x00;
        abyLinkage[1] = 0x00;
        abyLinkage[2] = (GByte) (nEntityNum % 256);
        abyLinkage[3] = (GByte) (nEntityNum / 256);
        abyLinkage[4] = (GByte) (nMSLink % 256);
        abyLinkage[5] = (GByte) ((nMSLink / 256) % 256);
        abyLinkage[6] = (GByte) (nMSLink / 65536);
        abyLinkage[7] = 0x01;
    }
    else
    {
        nLinkageSize = 16;
        abyLinkage[0] = 0x07;
        abyLinkage[1] = 0x10;
        abyLinkage[2] = (GByte) (nLinkageType % 256);
        abyLinkage[3] = (GByte) (nLinkageType / 256);
        abyLinkage[4] = (GByte) (0x81);
        abyLinkage[5] = (GByte) (0x0F);
        abyLinkage[6] = (GByte) (nEntityNum % 256);
        abyLinkage[7] = (GByte) (nEntityNum / 256);
        abyLinkage[8] = (GByte) (nMSLink % 256);
        abyLinkage[9] = (GByte) ((nMSLink / 256) % 256);
        abyLinkage[10] = (GByte) ((nMSLink / 65536) % 256);
        abyLinkage[11] = (GByte) (nMSLink / 16777216);
        abyLinkage[12] = 0x00;
        abyLinkage[13] = 0x00;
        abyLinkage[14] = 0x00;
        abyLinkage[15] = 0x00;
    }

    return DGNAddRawAttrLink( hDGN, psElement, nLinkageSize, abyLinkage );
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
    int   iLinkage;

    if( nLinkSize % 2 == 1 )
        nLinkSize++;

    if( psElement->size + nLinkSize > 768 )
    {
        CPLError( CE_Failure, CPLE_ElementTooBig, 
                  "Attempt to add %d byte linkage to element exceeds maximum"
                  " element size.", 
                  nLinkSize );
        return -1;
    }
    
/* -------------------------------------------------------------------- */
/*      Ensure the attribute linkage bit is set.                        */
/* -------------------------------------------------------------------- */
    psElement->properties |= DGNPF_ATTRIBUTES;

/* -------------------------------------------------------------------- */
/*      Append the attribute linkage to the linkage area.               */
/* -------------------------------------------------------------------- */
    psElement->attr_bytes += nLinkSize;
    psElement->attr_data = (unsigned char *) 
        CPLRealloc( psElement->attr_data, psElement->attr_bytes );
    
    memcpy( psElement->attr_data + (psElement->attr_bytes-nLinkSize), 
            pabyRawLinkData, nLinkSize );

/* -------------------------------------------------------------------- */
/*      Grow the raw data, if we have rawdata.                          */
/* -------------------------------------------------------------------- */
    psElement->raw_bytes += nLinkSize;
    psElement->raw_data = (unsigned char *)
        CPLRealloc( psElement->raw_data, psElement->raw_bytes );

    memcpy( psElement->raw_data + (psElement->raw_bytes-nLinkSize),
            pabyRawLinkData, nLinkSize );

/* -------------------------------------------------------------------- */
/*      If the element is a shape or chain complex header, then we      */
/*      need to increase the total complex group size appropriately.    */
/* -------------------------------------------------------------------- */
    if( psElement->stype == DGNST_COMPLEX_HEADER ||
        psElement->stype == DGNST_TEXT_NODE )  // compatible structures
    {
        DGNElemComplexHeader *psCT = (DGNElemComplexHeader *) psElement;

        psCT->totlength += (nLinkSize / 2);

        psElement->raw_data[36] = (unsigned char) (psCT->totlength % 256);
        psElement->raw_data[37] = (unsigned char) (psCT->totlength / 256);
    }

/* -------------------------------------------------------------------- */
/*      Ensure everything is updated properly, including element        */
/*      length and properties.                                          */
/* -------------------------------------------------------------------- */
    DGNUpdateElemCoreExtended( hDGN, psElement );

/* -------------------------------------------------------------------- */
/*      Figure out what the linkage index is.                           */
/* -------------------------------------------------------------------- */
    for( iLinkage = 0; ; iLinkage++ )
    {
        if( DGNGetLinkage( hDGN, psElement, iLinkage, NULL, NULL, NULL, NULL )
            == NULL )
            break;
    }

    return iLinkage-1;
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

int DGNAddShapeFillInfo( DGNHandle hDGN, DGNElemCore *psElement, 
                          int nColor )

{
    unsigned char abyFillInfo[16] = 
    { 0x07, 0x10, 0x41, 0x00, 0x02, 0x08, 0x01, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    abyFillInfo[8] = (unsigned char) nColor;

    return DGNAddRawAttrLink( hDGN, psElement, 16, abyFillInfo );
}
