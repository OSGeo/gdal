/**********************************************************************
 *
 *  geo_get.c  -- Public routines for GEOTIFF GeoKey access.
 *
 *    Written By: Niles D. Ritter.
 *
 *  copyright (c) 1995   Niles D. Ritter
 *
 *  Permission granted to use this software, so long as this copyright
 *  notice accompanies any products derived therefrom.
 *
 *  Revision History;
 *
 *    20 June, 1995      Niles D. Ritter         New
 *    3 July,  1995      Greg Martin             Fix strings and index
 *    6 July,  1995      Niles D. Ritter         Unfix indexing.
 *
 **********************************************************************/

#include "geotiff.h"   /* public interface        */
#include "geo_tiffp.h" /* external TIFF interface */
#include "geo_keyp.h"  /* private interface       */

/* return the Header info of this geotiff file */

void GTIFDirectoryInfo(GTIF *gtif, int version[3], int *keycount)
{
        if (version)
        {
                version[0]  = gtif->gt_version;
                version[1]  = gtif->gt_rev_major;
                version[2]  = gtif->gt_rev_minor;
        }
        if (keycount) *keycount = gtif->gt_num_keys;
}


int GTIFKeyInfo(GTIF *gtif, geokey_t key, int *size, tagtype_t* type)
{
        int index = gtif->gt_keyindex[ key ];
        GeoKey *keyptr;

        if (!index) return 0;

        keyptr = gtif->gt_keys + index;
        if (size) *size = (int) keyptr->gk_size;
        if (type) *type = keyptr->gk_type;

        return keyptr->gk_count;
}


/*
 * Get <count> values of Key <key>, starting with the <index>'th value.
 */
int GTIFKeyGet(GTIF *gtif, geokey_t thekey, void *val, int index, int count)
{
        int kindex = gtif->gt_keyindex[ thekey ];
        GeoKey *key;
        gsize_t size;
        char *data;
        tagtype_t type;

        if (!kindex) return 0;

        key = gtif->gt_keys+kindex;
        if (!count) count = key->gk_count - index;
        if (count <=0) return 0;
        if (count > key->gk_count) count = key->gk_count;
        size = key->gk_size;
        type = key->gk_type;

        if (count==1 && type==TYPE_SHORT) data = (char *)&key->gk_data;
        else data = key->gk_data;

        _GTIFmemcpy( val, data + index*size, count*size );

        if (type==TYPE_ASCII)
           ((char *)val)[count-1] = '\0'; /* replace last char with NULL */

        return count;
}
