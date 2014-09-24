/*
 * moreattr.c 
 */

#include "csf.h"
#include "csfimpl.h"

#include <string.h> /* strlen */

/* get the size of the history attribute
 * returns
 *  the size of history buffer INCLUDING the termimating `\0`,
 *  or 0 if not available or in case of error
 */
size_t MgetHistorySize(MAP *m) /* the map to get it from */
{
	return (size_t)CsfAttributeSize(m, ATTR_ID_HISTORY);
}

/* get the size of the description attribute
 * returns
 *  the size of description buffer INCLUDING the termimating `\0`,
 *  or 0 if not available or in case of error
 */
size_t MgetDescriptionSize(MAP *m) /* the map to get it from */
{
	return (size_t)CsfAttributeSize(m, ATTR_ID_DESCRIPTION);
}

/* get the number of colour palette entries
 * MgetNrColourPaletteEntries returns the number of rgb tupels
 * of the colour palette. Each tupel is a sequence of 3 UINT2
 * words describing red, green and blue.
 * returns
 *  the number of rgb tupels,
 *  or 0 if not available or in case of error
 */
size_t MgetNrColourPaletteEntries(MAP *m) /* the map to get it from */
{
	size_t s = (size_t)CsfAttributeSize(m, ATTR_ID_COLOUR_PAL);
	POSTCOND( (s % (3*sizeof(UINT2))) == 0);
	return s / (3*sizeof(UINT2));
}

/* get the number of grey palette entries
 * MgetNrGreyPaletteEntries returns the number of grey tupels
 * of the grey palette. Each tupel is one UINT2
 * words describing the intensity: low, 0 is black, high is white.
 * returns
 *  the number of grey tupels,
 *  or 0 if not available or in case of error
 */
size_t MgetNrGreyPaletteEntries(MAP *m) /* the map to get it from */
{
	size_t s = (size_t)CsfAttributeSize(m, ATTR_ID_GREY_PAL);
	POSTCOND( (s % (sizeof(UINT2))) == 0);
	return s / (sizeof(UINT2));
}

/* get the description attribute
 * returns
 *  0 if not available or in case of error,
 * nonzero otherwise
 */
int MgetDescription(MAP *m,    /* the map to get it from */
		char *des) /* the  resulting description string */
{
	size_t size;
	return CsfGetAttribute(m, ATTR_ID_DESCRIPTION, sizeof(char), &size, des);
}

/* get the history attribute
 * returns
 *  0 if not available or in case of error,
 * nonzero otherwise
 */
int MgetHistory(MAP *m,    /* the map to get it from */
		char *history) /* the  resulting history string */
{
	size_t size;
	return CsfGetAttribute(m, ATTR_ID_HISTORY, sizeof(char), &size, history);
}

/* get the colour palette 
 * MgetColourPalette fills the pal argument with a number of rgb tupels
 * of the colour palette. Each tupel is a sequence of 3 UINT2
 * words describing red, green and blue. Thus if the map has 8
 * colour palette entries it puts 24 UINT2 values in pal.
 * returns
 *  0 if not available or in case of error,
 * nonzero otherwise
 */
int MgetColourPalette(MAP *m, /* the map to get it from */
	UINT2 *pal) /* the resulting palette */
{
	size_t size;
	return CsfGetAttribute(m, ATTR_ID_COLOUR_PAL, sizeof(UINT2), &size, pal);
}

/* get the grey palette 
 * MgetGreyPalette fills the pal argument with a number of grey tupels
 * of the grey palette. Each tupel is one UINT2
 * words describing the intensity: low, 0 is black, high is white.
 * returns
 *  0 if not available or in case of error,
 * nonzero otherwise
 */
int MgetGreyPalette(MAP *m, /* the map to get it from */
	UINT2 *pal) /* the resulting palette */
{
	size_t size;
	return CsfGetAttribute(m, ATTR_ID_GREY_PAL, sizeof(UINT2), &size, pal);
}

/* put the description attribute
 * MputDescription writes the description string to a map.
 * An existing description is overwritten.
 * returns
 *  0 if not available or in case of error,
 * nonzero otherwise
 */
int MputDescription(MAP *m,    /* the map to get it from */
		char *des) /* the  new description string. 
		            * Is a C-string, `\0`-terminated 
		            */
{
	return CsfUpdateAttribute(m, ATTR_ID_DESCRIPTION, sizeof(char), 
			strlen(des)+1, des);
}

/* put the history attribute
 * MputHistory writes the history string to a map.
 * An existing history is overwritten.
 * returns
 *  0 if not available or in case of error,
 * nonzero otherwise
 */
int MputHistory(MAP *m,    /* the map to get it from */
		char *history) /* the new history string 
		                * Is a C-string, `\0`-terminated 
		                */
{
	return CsfUpdateAttribute(m, ATTR_ID_HISTORY, sizeof(char), 
			strlen(history)+1, history);
}

/* put the colour palette 
 * MputColourPalette writes the pal argument that is filled 
 * with a number of rgb tupels
 * of the colour palette to the map. Each tupel is a sequence of 3 UINT2
 * words describing red, green and blue. Thus if the map has 8
 * colour palette entries it puts 24 UINT2 values in map palette.
 * An existing colour palette is overwritten.
 * returns
 *  0 if not available or in case of error,
 * nonzero otherwise
 */
int MputColourPalette(MAP *m, /* the map to get it from */
	UINT2 *pal, /* the new palette */
	size_t nrTupels) /* the number of 3 UINT2 words tupels of pal */
{
	return CsfUpdateAttribute(m, ATTR_ID_COLOUR_PAL, sizeof(UINT2), 
		nrTupels*3, pal);
}

/* put the grey palette 
 * MputColourPalette writes the pal argument that is filled 
 * with a number of grey tupels
 * of the grey palette. Each tupel is one UINT2
 * words describing the intensity: low, 0 is black, high is white.
 * An existing grey palette is overwritten.
 * returns
 *  0 if not available or in case of error,
 * nonzero otherwise
 */
int MputGreyPalette(MAP *m, /* the map to get it from */
	UINT2 *pal, /* the new grey palette */
	size_t nrTupels) /* the number of UINT2 words tupels of pal */
{
	return CsfUpdateAttribute(m, ATTR_ID_GREY_PAL, sizeof(UINT2), 
		nrTupels, pal);
}
