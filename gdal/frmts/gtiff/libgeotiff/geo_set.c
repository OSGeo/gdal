/**********************************************************************
 *
 *  geo_set.c  -- Public routines for GEOTIFF GeoKey access.
 *
 *    Written By: Niles D. Ritter.
 *
 *  copyright (c) 1995   Niles D. Ritter
 *
 *  Permission granted to use this software, so long as this copyright
 *  notice accompanies any products derived therefrom.
 *
 **********************************************************************/

#include "geotiff.h"   /* public interface        */
#include "geo_tiffp.h" /* external TIFF interface */
#include "geo_keyp.h"  /* private interface       */

#include <assert.h>

/**
This function writes a geokey_t value to a GeoTIFF file.

@param gtif The geotiff information handle from GTIFNew().

@param keyID The geokey_t name (such as ProjectedCSTypeGeoKey).
This must come from the list of legal geokey_t values
(an enumeration) listed below.

@param val The <b>val</b> argument is a pointer to the
variable into which the value should be read.  The type of the variable
varies depending on the geokey_t given.  While there is no ready mapping
of geokey_t values onto types, in general code values are of type <i>short</i>,
citations are strings, and everything else is of type <i>double</i>.  Note
that pointer's to <i>int</i> should never be passed to GTIFKeyGet() for
integer values as they will be shorts, and the int's may not be properly
initialized (and will be grossly wrong on MSB systems).

@param index Indicates how far into the list of values
for this geokey to offset. Should normally be zero.

@param count Indicates how many values
to read.  At this time all keys except for strings have only one value,
so <b>index</b> should be zero, and <b>count</b> should be one.<p>

The <b>key</b> indicates the key name to be written to the
file and should from the geokey_t enumeration 
(eg. <tt>ProjectedCSTypeGeoKey</tt>).  The full list of possible geokey_t
values can be found in geokeys.inc, or in the online documentation for
GTIFKeyGet().<p>

The <b>type</b> should be one of TYPE_SHORT, TYPE_ASCII, or TYPE_DOUBLE and
will indicate the type of value being passed at the end of the argument
list (the key value).  The <b>count</b> should be one except for strings
when it should be the length of the string (or zero to for this to be
computed internally).  As a special case a <b>count</b> of -1 can be
used to request an existing key be deleted, in which no value is passed.<p>

The actual value is passed at the end of the argument list, and should be
a short, a double, or a char * value.  Note that short and double values
are passed by value rather than as pointers when count is 1, but as pointers
if count is larger than 1.<p>

Note that key values aren't actually flushed to the file until
GTIFWriteKeys() is called.  Till then 
the new values are just kept with the GTIF structure.<p>

<b>Example:</b><p>

<pre>
    GTIFKeySet(gtif, GTRasterTypeGeoKey, TYPE_SHORT, 1, 
               RasterPixelIsArea);
    GTIFKeySet(gtif, GTCitationGeoKey, TYPE_ASCII, 0, 
               "UTM 11 North / NAD27" );
</pre>

 */

int GTIFKeySet(GTIF *gtif, geokey_t keyID, tagtype_t type, int count,...)
{
    va_list ap;
    int index = gtif->gt_keyindex[ keyID ];
    int newvalues = 0;
    GeoKey *key;
    char *data = NULL;
    char *val = NULL;
    pinfo_t sval;
    double dval;

    va_start(ap, count);
    /* pass singleton keys by value */
    if (count>1 && type!=TYPE_ASCII) 
    {
        val = va_arg(ap, char*);
    }
    else if( count == -1 )
    {
        /* delete the indicated tag */
        va_end(ap);

        if( index < 1 )
            return 0;

        if (gtif->gt_keys[index].gk_type == TYPE_ASCII)
        {
            _GTIFFree (gtif->gt_keys[index].gk_data);
        }

        while( index < gtif->gt_num_keys )
        {
            _GTIFmemcpy( gtif->gt_keys + index, 
                         gtif->gt_keys + index + 1, 
                         sizeof(GeoKey) );
            gtif->gt_keyindex[gtif->gt_keys[index].gk_key] = index;
            index++;
        }

        gtif->gt_num_keys--;
        gtif->gt_nshorts -= sizeof(KeyEntry)/sizeof(pinfo_t);
        gtif->gt_keyindex[keyID] = 0;
        gtif->gt_flags |= FLAG_FILE_MODIFIED;

        return 1;
    }
    else switch (type)
    {
      case TYPE_SHORT:  sval=(pinfo_t) va_arg(ap, int); val=(char *)&sval;     break;
      case TYPE_DOUBLE: dval=va_arg(ap, dblparam_t); val=(char *)&dval;  break;
      case TYPE_ASCII: 
        val=va_arg(ap, char*);
        count = strlen(val) + 1; /* force = string length */
        break;
      default:
        assert( FALSE );
        break;
    }
    va_end(ap);
    
    /* We assume here that there are no multi-valued SHORTS ! */
    if (index)
    {
        /* Key already exists */
        key = gtif->gt_keys+index;
        if (type!=key->gk_type || count > key->gk_count)
        {
            /* need to reset data pointer */
            key->gk_type = type;
            key->gk_count = count;
            key->gk_size = _gtiff_size[ type ];
            newvalues = 1;
        }
    }
    else
    {
        /* We need to create the key */
        if (gtif->gt_num_keys == MAX_KEYS) return 0;
        key = gtif->gt_keys + ++gtif->gt_num_keys;
        index = gtif->gt_num_keys;
        gtif->gt_keyindex[ keyID ] = index;
        key->gk_key = keyID;
        key->gk_type = type;
        key->gk_count = count;
        key->gk_size = _gtiff_size[ type ];
        if ((geokey_t)gtif->gt_keymin > keyID)  gtif->gt_keymin=keyID;
        if ((geokey_t)gtif->gt_keymax < keyID)  gtif->gt_keymax=keyID;
        newvalues = 1;
    }

    if (newvalues)
    {
        switch (type)
        {
          case TYPE_SHORT:  
            if (count > 1) return 0;
            data = (char *)&key->gk_data; /* store value *in* data */
            break;
          case TYPE_DOUBLE:
            key->gk_data = (char *)(gtif->gt_double + gtif->gt_ndoubles);
            data = key->gk_data;
            gtif->gt_ndoubles += count;
            break;
          case TYPE_ASCII:
            break;
          default:
            va_end(ap);
            return 0;
        }
        gtif->gt_nshorts += sizeof(KeyEntry)/sizeof(pinfo_t);
    }

    /* this fixes a bug where if a request is made to write a duplicate
       key, we must initialize the data to a valid value.
       Bryan Wells (bryan@athena.bangor.autometric.com) */
        
    else /* no new values, but still have something to write */
    {
        switch (type)
        {
          case TYPE_SHORT:  
            if (count > 1) return 0;
            data = (char *)&key->gk_data; /* store value *in* data */
            break;
          case TYPE_DOUBLE:
            data = key->gk_data;
            break;
          case TYPE_ASCII:
            break;
          default:
            return 0;
        }
    }
        
    switch (type)
    {
      case TYPE_ASCII:
        /* throw away existing data and allocate room for new data */
        if (key->gk_data != 0)
        {
            _GTIFFree(key->gk_data);
        }
        key->gk_data = (char *)_GTIFcalloc(count);
        key->gk_count = count;
        data = key->gk_data;
        break;
      default:
        break;
    }

    _GTIFmemcpy(data, val, count*key->gk_size);
    
    gtif->gt_flags |= FLAG_FILE_MODIFIED;
    return 1;
}
