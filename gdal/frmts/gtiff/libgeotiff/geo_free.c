/**********************************************************************
 *
 *  geo_free.c  -- Public routines for GEOTIFF GeoKey access.
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


/**********************************************************************
 *
 *                        Public Routines
 *
 **********************************************************************/

/**

This function deallocates an existing GeoTIFF access handle previously
created with GTIFNew().  If the handle was
used to write GeoTIFF keys to the TIFF file, the
GTIFWriteKeys() function should be used
to flush results to the file before calling GTIFFree().  GTIFFree()
should be called before XTIFFClose() is
called on the corresponding TIFF file handle.<p>

*/

void GTIFFree(GTIF* gtif)
{
    int     i;

    if (!gtif) return;

    /* Free parameter arrays */
    if (gtif->gt_double) _GTIFFree (gtif->gt_double);
    if (gtif->gt_short) _GTIFFree (gtif->gt_short);

    /* Free GeoKey arrays */
    if (gtif->gt_keys)
    {
        for (i = 0; i < MAX_KEYS; i++)
        {
            if (gtif->gt_keys[i].gk_type == TYPE_ASCII)
            {
                _GTIFFree (gtif->gt_keys[i].gk_data);
            }
        }
        _GTIFFree (gtif->gt_keys);
    }
    if (gtif->gt_keyindex) _GTIFFree (gtif->gt_keyindex);

    _GTIFFree (gtif);
}
