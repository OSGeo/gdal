/*****************************************************************************
*   "Gif-Lib" - Yet another gif library.				     *
*									     *
* Written by:  Gershon Elber				Ver 0.1, Jun. 1989   *
* Extensively hacked by: Eric S. Raymond		Ver 1.?, Sep 1992    *
******************************************************************************
* GIF construction tools						      *
******************************************************************************
* History:								     *
* 15 Sep 92 - Version 1.0 by Eric Raymond.				     *
*****************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "gif_lib.h"

#define MAX(x, y)	(((x) > (y)) ? (x) : (y))

/******************************************************************************
* Miscellaneous utility functions					      *
******************************************************************************/

int BitSize(int n)
/* return smallest bitfield size n will fit in */
{
    register int i;

    for (i = 1; i <= 8; i++)
	if ((1 << i) >= n)
	    break;
    return(i);
}


/******************************************************************************
* Color map object functions						      *
******************************************************************************/

ColorMapObject *MakeMapObject(int ColorCount, GifColorType *ColorMap)
/*
 * Allocate a color map of given size; initialize with contents of
 * ColorMap if that pointer is non-NULL.
 */
{
    ColorMapObject *Object;

    if (ColorCount != (1 << BitSize(ColorCount)))
	return((ColorMapObject *)NULL);

    Object = (ColorMapObject *)malloc(sizeof(ColorMapObject));
    if (Object == (ColorMapObject *)NULL)
	return((ColorMapObject *)NULL);

    Object->Colors = (GifColorType *)calloc(ColorCount, sizeof(GifColorType));
    if (Object->Colors == (GifColorType *)NULL)
	return((ColorMapObject *)NULL);

    Object->ColorCount = ColorCount;
    Object->BitsPerPixel = BitSize(ColorCount);

    if (ColorMap)
	memcpy((char *)Object->Colors,
	       (char *)ColorMap, ColorCount * sizeof(GifColorType));

    return(Object);
}

void FreeMapObject(ColorMapObject *Object)
/*
 * Free a color map object
 */
{
    free(Object->Colors);
    free(Object);
}

#ifdef DEBUG
void DumpColorMap(ColorMapObject *Object, FILE *fp)
{
    if (Object)
    {
	int i, j, Len = Object->ColorCount;

	for (i = 0; i < Len; i+=4) {
	    for (j = 0; j < 4 && j < Len; j++) {
		fprintf(fp,
			"%3d: %02x %02x %02x   ", i + j,
		       Object->Colors[i + j].Red,
		       Object->Colors[i + j].Green,
		       Object->Colors[i + j].Blue);
	    }
	    fprintf(fp, "\n");
	}
    }
}
#endif /* DEBUG */

ColorMapObject *UnionColorMap(
			 ColorMapObject *ColorIn1,
			 ColorMapObject *ColorIn2,
			 GifPixelType ColorTransIn2[])
/*
 * Compute the union of two given color maps and return it.  If result can't 
 * fit into 256 colors, NULL is returned, the allocated union otherwise.
 * ColorIn1 is copied as is to ColorUnion, while colors from ColorIn2 are
 * copied iff they didn't exist before.  ColorTransIn2 maps the old
 * ColorIn2 into ColorUnion color map table.
 */
{
    int i, j, CrntSlot, RoundUpTo, NewBitSize;
    ColorMapObject *ColorUnion;

    /*
     * Allocate table which will hold the result for sure.
     */
    ColorUnion
	= MakeMapObject(MAX(ColorIn1->ColorCount,ColorIn2->ColorCount)*2,NULL);

    if (ColorUnion == NULL)
	return(NULL);

    /* Copy ColorIn1 to ColorUnionSize; */
    for (i = 0; i < ColorIn1->ColorCount; i++)
	ColorUnion->Colors[i] = ColorIn1->Colors[i];
    CrntSlot = ColorIn1->ColorCount;

    /*
     * Potentially obnoxious hack:
     *
     * Back CrntSlot down past all contiguous {0, 0, 0} slots at the end
     * of table 1.  This is very useful if your display is limited to
     * 16 colors.
     */
    while (ColorIn1->Colors[CrntSlot-1].Red == 0
	   && ColorIn1->Colors[CrntSlot-1].Green == 0
	   && ColorIn1->Colors[CrntSlot-1].Red == 0)
	CrntSlot--;

    /* Copy ColorIn2 to ColorUnionSize (use old colors if they exist): */
    for (i = 0; i < ColorIn2->ColorCount && CrntSlot<=256; i++)
    {
	/* Let's see if this color already exists: */
	for (j = 0; j < ColorIn1->ColorCount; j++)
	    if (memcmp(&ColorIn1->Colors[j], &ColorIn2->Colors[i], sizeof(GifColorType)) == 0)
		break;

	if (j < ColorIn1->ColorCount)
	    ColorTransIn2[i] = j;	/* color exists in Color1 */
	else
	{
	    /* Color is new - copy it to a new slot: */
	    ColorUnion->Colors[CrntSlot] = ColorIn2->Colors[i];
	    ColorTransIn2[i] = CrntSlot++;
	}
    }

    if (CrntSlot > 256)
    {
	FreeMapObject(ColorUnion);
	return((ColorMapObject *)NULL);
    }

    NewBitSize = BitSize(CrntSlot);
    RoundUpTo = (1 << NewBitSize);

    if (RoundUpTo != ColorUnion->ColorCount)
    {
	register GifColorType	*Map = ColorUnion->Colors;

	/*
	 * Zero out slots up to next power of 2.
	 * We know these slots exist because of the way ColorUnion's
	 * start dimension was computed.
	 */
	for (j = CrntSlot; j < RoundUpTo; j++)
	    Map[j].Red = Map[j].Green = Map[j].Blue = 0;

	/* perhaps we can shrink the map? */
	if (RoundUpTo < ColorUnion->ColorCount)
	    ColorUnion->Colors 
		= (GifColorType *)realloc(Map, sizeof(GifColorType)*RoundUpTo);
    }

    ColorUnion->ColorCount = RoundUpTo;
    ColorUnion->BitsPerPixel = NewBitSize;

    return(ColorUnion);
}

void ApplyTranslation(SavedImage *Image, GifPixelType Translation[])
/*
 * Apply a given color translation to the raster bits of an image
 */
{
    register int i;
    register int RasterSize = Image->ImageDesc.Height * Image->ImageDesc.Width;

    for (i = 0; i < RasterSize; i++)
	Image->RasterBits[i] = Translation[(int)Image->RasterBits[i]];
}

/******************************************************************************
* Extension record functions						      *
******************************************************************************/

void MakeExtension(SavedImage *New, int Function)
{
    New->Function = Function;
    /*
     * Someday we might have to deal with multiple extensions.
     */
}

int AddExtensionBlock(SavedImage *New, int Len, char ExtData[])
{
    ExtensionBlock	*ep;

    if (New->ExtensionBlocks == NULL)
	New->ExtensionBlocks = (ExtensionBlock *)malloc(sizeof(ExtensionBlock));
    else
	New->ExtensionBlocks =
	    (ExtensionBlock *)realloc(New->ExtensionBlocks,
		      sizeof(ExtensionBlock) * (New->ExtensionBlockCount + 1));

    if (New->ExtensionBlocks == NULL)
	return(GIF_ERROR);

    ep = &New->ExtensionBlocks[New->ExtensionBlockCount++];

    if ((ep->Bytes = (char *)malloc(ep->ByteCount = Len)) == NULL)
	return(GIF_ERROR);

    if (ExtData) {
	    memcpy(ep->Bytes, ExtData, Len);
        ep->Function = New->Function;
    }

    return(GIF_OK);
}

void FreeExtension(SavedImage *Image)
{
    ExtensionBlock	*ep;

    for (ep = Image->ExtensionBlocks;
	 ep < Image->ExtensionBlocks + Image->ExtensionBlockCount;
	 ep++)
	(void) free((char *)ep->Bytes);
    free((char *)Image->ExtensionBlocks);
    Image->ExtensionBlocks = NULL;
}

/******************************************************************************
* Image block allocation functions					      *
******************************************************************************/
SavedImage *MakeSavedImage(GifFileType *GifFile, SavedImage *CopyFrom)
/*
 * Append an image block to the SavedImages array  
 */
{
    SavedImage	*sp;

    if (GifFile->SavedImages == NULL)
	GifFile->SavedImages = (SavedImage *)malloc(sizeof(SavedImage));
    else
	GifFile->SavedImages = (SavedImage *)realloc(GifFile->SavedImages,
				sizeof(SavedImage) * (GifFile->ImageCount+1));

    if (GifFile->SavedImages == NULL)
	return((SavedImage *)NULL);
    else
    {
	sp = &GifFile->SavedImages[GifFile->ImageCount++];
	memset((char *)sp, '\0', sizeof(SavedImage));

	if (CopyFrom)
	{
	    memcpy((char *)sp, CopyFrom, sizeof(SavedImage));

	    /*
	     * Make our own allocated copies of the heap fields in the
	     * copied record.  This guards against potential aliasing
	     * problems.
	     */

	    /* first, the local color map */
	    if (sp->ImageDesc.ColorMap)
		sp->ImageDesc.ColorMap =
		    MakeMapObject(CopyFrom->ImageDesc.ColorMap->ColorCount,
				  CopyFrom->ImageDesc.ColorMap->Colors);

	    /* next, the raster */
	    sp->RasterBits = (char *)malloc(sizeof(GifPixelType)
				* CopyFrom->ImageDesc.Height
				* CopyFrom->ImageDesc.Width);
	    memcpy(sp->RasterBits,
		   CopyFrom->RasterBits,
		   sizeof(GifPixelType)
			* CopyFrom->ImageDesc.Height
			* CopyFrom->ImageDesc.Width);

	    /* finally, the extension blocks */
	    if (sp->ExtensionBlocks)
	    {
		sp->ExtensionBlocks
		    = (ExtensionBlock*)malloc(sizeof(ExtensionBlock)
					      * CopyFrom->ExtensionBlockCount);
		memcpy(sp->ExtensionBlocks,
		   CopyFrom->ExtensionBlocks,
		   sizeof(ExtensionBlock)
		   	* CopyFrom->ExtensionBlockCount);

		/*
		 * For the moment, the actual blocks can take their
		 * chances with free().  We'll fix this later. 
		 */
	    }
	}

	return(sp);
    }
}

void FreeSavedImages(GifFileType *GifFile)
{
    SavedImage	*sp;

    for (sp = GifFile->SavedImages;
	 sp < GifFile->SavedImages + GifFile->ImageCount;
	 sp++)
    {
	if (sp->ImageDesc.ColorMap)
	    FreeMapObject(sp->ImageDesc.ColorMap);

	if (sp->RasterBits)
	    free((char *)sp->RasterBits);

	if (sp->ExtensionBlocks)
	    FreeExtension(sp);
    }
    free((char *) GifFile->SavedImages);
}



