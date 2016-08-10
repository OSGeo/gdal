/******************************************************************************
 * Copyright (c) 2008, Frank Warmerdam
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
 *  geo_simpletags.c  TIFF Interface module that just keeps track of the
 *                    tags in memory, without depending on libtiff.
 *
 *****************************************************************************/

#include "geotiff.h"    /* public GTIFF interface */
#include "geo_simpletags.h"

#include "geo_tiffp.h"  /* Private TIFF interface */
#include "geo_keyp.h"   /* Private GTIFF interface */

static int ST_TypeSize( int st_type );

static int        _GTIFGetField (tiff_t *tif, pinfo_t tag, int *count, void *value );
static int        _GTIFSetField (tiff_t *tif, pinfo_t tag, int  count, void *value );
static tagtype_t  _GTIFTagType  (tiff_t *tif, pinfo_t tag);

/*
 * Set up default TIFF handlers.
 */
void GTIFSetSimpleTagsMethods(TIFFMethod *method)
{
	if (!method) return;

	method->get = _GTIFGetField;
	method->set = _GTIFSetField;
	method->type = _GTIFTagType;
}

/* returns the value of TIFF tag <tag>, or if
 * the value is an array, returns an allocated buffer
 * containing the values. Allocate a copy of the actual
 * buffer, sized up for updating.
 */
static int _GTIFGetField (tiff_t *tif, pinfo_t tag, int *count, void *val )
{
    int item_size, data_type;
    void *internal_value, *ret_value;

    if( !ST_GetKey( (ST_TIFF*) tif, (int) tag, count, &data_type,
                    &internal_value ) )
        return 0;

    if( data_type != ST_TagType( tag ) )
        return 0;

    item_size = ST_TypeSize( data_type );

    ret_value = (char *)_GTIFcalloc( *count * item_size );
    if (!ret_value) return 0;

    _TIFFmemcpy( ret_value, internal_value,  item_size * *count );

    *(void **)val = ret_value;
    return 1;
}

/*
 * Set a GeoTIFF TIFF field.
 */
static int _GTIFSetField (tiff_t *tif, pinfo_t tag, int count, void *value )
{
    int st_type = ST_TagType( tag );

    return ST_SetKey( (ST_TIFF *) tif, (int) tag, count, st_type, value );
}

/*
 *  This routine is supposed to return the TagType of the <tag>
 *  TIFF tag. Unfortunately, "libtiff" does not provide this
 *  service by default, so we just have to "know" what type of tags
 *  we've got, and how many. We only define the ones Geotiff
 *  uses here, and others return UNKNOWN. The "tif" parameter
 *  is provided for those TIFF implementations that provide
 *  for tag-type queries.
 */
static tagtype_t  _GTIFTagType  (tiff_t *tif, pinfo_t tag)
{
	tagtype_t ttype;

	(void) tif; /* dummy reference */

	switch (tag)
	{
		case GTIFF_ASCIIPARAMS:    ttype=TYPE_ASCII; break;
		case GTIFF_PIXELSCALE:
		case GTIFF_TRANSMATRIX:
		case GTIFF_TIEPOINTS:
		case GTIFF_DOUBLEPARAMS:   ttype=TYPE_DOUBLE; break;
		case GTIFF_GEOKEYDIRECTORY: ttype=TYPE_SHORT; break;
		default: ttype = TYPE_UNKNOWN;
	}

	return ttype;
}

/************************************************************************/
/*                             ST_TagType()                             */
/************************************************************************/

int ST_TagType( int tag )
{
    switch (tag)
    {
      case GTIFF_ASCIIPARAMS:
        return STT_ASCII;

      case GTIFF_PIXELSCALE:
      case GTIFF_TRANSMATRIX:
      case GTIFF_TIEPOINTS:
      case GTIFF_DOUBLEPARAMS:
        return STT_DOUBLE;

      case GTIFF_GEOKEYDIRECTORY:
        return STT_SHORT;
    }

    return -1;
}


/************************************************************************/
/*                            ST_TypeSize()                             */
/************************************************************************/

static int ST_TypeSize( int st_type )

{
    if( st_type == STT_ASCII )
        return 1;
    else if( st_type == STT_SHORT )
        return 2;
    else if( st_type == STT_DOUBLE )
        return 8;
    else
        return 8;
}

/************************************************************************/
/*                             ST_Create()                              */
/************************************************************************/

ST_TIFF *ST_Create()

{
    return (ST_TIFF *) calloc(1,sizeof(ST_TIFF));
}

/************************************************************************/
/*                             ST_Destroy()                             */
/************************************************************************/

void ST_Destroy( ST_TIFF *st )

{
    int i;

    for( i = 0; i < st->key_count; i++ )
        free( st->key_list[i].data );

    if( st->key_list )
        free( st->key_list );
    free( st );
}

/************************************************************************/
/*                             ST_SetKey()                              */
/************************************************************************/

int ST_SetKey( ST_TIFF *st, int tag, int count, int st_type, void *data )

{
    int i, item_size = ST_TypeSize( st_type );

/* -------------------------------------------------------------------- */
/*      We should compute the length if we were not given a count       */
/* -------------------------------------------------------------------- */
    if (count == 0 && st_type == STT_ASCII )
    {
        count = (int)strlen((char*)data)+1;
    }

/* -------------------------------------------------------------------- */
/*      If we already have a value for this tag, replace it.            */
/* -------------------------------------------------------------------- */
    for( i = 0; i < st->key_count; i++ )
    {
        if( st->key_list[i].tag == tag )
        {
            free( st->key_list[i].data );
            st->key_list[i].count = count;
            st->key_list[i].type = st_type;
            /* +1 to make clang static analyzer not warn about potential malloc(0) */
            st->key_list[i].data = malloc(item_size*count+1);
            memcpy( st->key_list[i].data, data, count * item_size );
            return 1;
        }
    }

/* -------------------------------------------------------------------- */
/*      Otherwise, add a new entry.                                     */
/* -------------------------------------------------------------------- */
    st->key_count++;
    st->key_list = (ST_KEY *) realloc(st->key_list,
                                      sizeof(ST_KEY) * st->key_count);
    st->key_list[st->key_count-1].tag = tag;
    st->key_list[st->key_count-1].count = count;
    st->key_list[st->key_count-1].type = st_type;
    /* +1 to make clang static analyzer not warn about potential malloc(0) */
    st->key_list[st->key_count-1].data = malloc(item_size * count+1);
    memcpy( st->key_list[st->key_count-1].data, data, item_size * count );

    return 1;
}

/************************************************************************/
/*                             ST_GetKey()                              */
/************************************************************************/

int ST_GetKey( ST_TIFF *st, int tag, int *count,
               int *st_type, void **data_ptr )

{
    int i;

    for( i = 0; i < st->key_count; i++ )
    {
        if( st->key_list[i].tag == tag )
        {
            if( count )
                *count = st->key_list[i].count;
            if( st_type )
                *st_type = st->key_list[i].type;
            if( data_ptr )
                *data_ptr = st->key_list[i].data;
            return 1;
        }
    }

    return 0;
}
