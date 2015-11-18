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
        int nIndex = gtif->gt_keyindex[ key ];
        GeoKey *keyptr;

        if (!nIndex) return 0;

        keyptr = gtif->gt_keys + nIndex;
        if (size) *size = (int) keyptr->gk_size;
        if (type) *type = keyptr->gk_type;

        return (int)keyptr->gk_count;
}

/** 

This function reads the value of a single GeoKey from a GeoTIFF file.

@param gtif The geotiff information handle from GTIFNew().

@param thekey The geokey_t name (such as ProjectedCSTypeGeoKey).
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
so <b>index</b> should be zero, and <b>count</b> should be one.

@return The GTIFKeyGet() function returns the number of values read.  Normally
this would be one if successful or zero if the key doesn't exist for this
file.

From geokeys.inc we see the following geokey_t values are possible:<p>

<pre>
-- 6.2.1 GeoTIFF Configuration Keys --

ValuePair(  GTModelTypeGeoKey,	1024) -- Section 6.3.1.1 Codes       --
ValuePair(  GTRasterTypeGeoKey,	1025) -- Section 6.3.1.2 Codes       --
ValuePair(  GTCitationGeoKey,	1026) -- documentation --

-- 6.2.2 Geographic CS Parameter Keys --

ValuePair(  GeographicTypeGeoKey,	2048) -- Section 6.3.2.1 Codes     --
ValuePair(  GeogCitationGeoKey,	2049) -- documentation             --
ValuePair(  GeogGeodeticDatumGeoKey,	2050) -- Section 6.3.2.2 Codes     --
ValuePair(  GeogPrimeMeridianGeoKey,	2051) -- Section 6.3.2.4 codes     --
ValuePair(  GeogLinearUnitsGeoKey,	2052) -- Section 6.3.1.3 Codes     --
ValuePair(  GeogLinearUnitSizeGeoKey,	2053) -- meters                    --
ValuePair(  GeogAngularUnitsGeoKey,	2054) -- Section 6.3.1.4 Codes     --
ValuePair(  GeogAngularUnitSizeGeoKey,	2055) -- radians                   --
ValuePair(  GeogEllipsoidGeoKey,	2056) -- Section 6.3.2.3 Codes     --
ValuePair(  GeogSemiMajorAxisGeoKey,	2057) -- GeogLinearUnits           --
ValuePair(  GeogSemiMinorAxisGeoKey,	2058) -- GeogLinearUnits           --
ValuePair(  GeogInvFlatteningGeoKey,	2059) -- ratio                     --
ValuePair(  GeogAzimuthUnitsGeoKey,	2060) -- Section 6.3.1.4 Codes     --
ValuePair(  GeogPrimeMeridianLongGeoKey,	2061) -- GeoAngularUnit            --

-- 6.2.3 Projected CS Parameter Keys --
--    Several keys have been renamed,--
--    and the deprecated names aliased for backward compatibility --

ValuePair(  ProjectedCSTypeGeoKey,	3072)     -- Section 6.3.3.1 codes   --
ValuePair(  PCSCitationGeoKey,	3073)     -- documentation           --
ValuePair(  ProjectionGeoKey,	3074)     -- Section 6.3.3.2 codes   --
ValuePair(  ProjCoordTransGeoKey,	3075)     -- Section 6.3.3.3 codes   --
ValuePair(  ProjLinearUnitsGeoKey,	3076)     -- Section 6.3.1.3 codes   --
ValuePair(  ProjLinearUnitSizeGeoKey,	3077)     -- meters                  --
ValuePair(  ProjStdParallel1GeoKey,	3078)     -- GeogAngularUnit --
ValuePair(  ProjStdParallelGeoKey,ProjStdParallel1GeoKey) -- ** alias **   --
ValuePair(  ProjStdParallel2GeoKey,	3079)     -- GeogAngularUnit --
ValuePair(  ProjNatOriginLongGeoKey,	3080)     -- GeogAngularUnit --
ValuePair(  ProjOriginLongGeoKey,ProjNatOriginLongGeoKey) -- ** alias **     --
ValuePair(  ProjNatOriginLatGeoKey,	3081)     -- GeogAngularUnit --
ValuePair(  ProjOriginLatGeoKey,ProjNatOriginLatGeoKey)   -- ** alias **     --
ValuePair(  ProjFalseEastingGeoKey,	3082)     -- ProjLinearUnits --
ValuePair(  ProjFalseNorthingGeoKey,	3083)     -- ProjLinearUnits --
ValuePair(  ProjFalseOriginLongGeoKey,	3084)     -- GeogAngularUnit --
ValuePair(  ProjFalseOriginLatGeoKey,	3085)     -- GeogAngularUnit --
ValuePair(  ProjFalseOriginEastingGeoKey,	3086)     -- ProjLinearUnits --
ValuePair(  ProjFalseOriginNorthingGeoKey,	3087)     -- ProjLinearUnits --
ValuePair(  ProjCenterLongGeoKey,	3088)     -- GeogAngularUnit --
ValuePair(  ProjCenterLatGeoKey,	3089)     -- GeogAngularUnit --
ValuePair(  ProjCenterEastingGeoKey,	3090)     -- ProjLinearUnits --
ValuePair(  ProjCenterNorthingGeoKey,	3091)     -- ProjLinearUnits --
ValuePair(  ProjScaleAtNatOriginGeoKey,	3092)     -- ratio   --
ValuePair(  ProjScaleAtOriginGeoKey,ProjScaleAtNatOriginGeoKey)  -- ** alias **   --
ValuePair(  ProjScaleAtCenterGeoKey,	3093)     -- ratio   --
ValuePair(  ProjAzimuthAngleGeoKey,	3094)     -- GeogAzimuthUnit --
ValuePair(  ProjStraightVertPoleLongGeoKey,	3095)     -- GeogAngularUnit --

 6.2.4 Vertical CS Keys 
   
ValuePair(  VerticalCSTypeGeoKey,	4096)  -- Section 6.3.4.1 codes   --
ValuePair(  VerticalCitationGeoKey,	4097)  -- documentation --
ValuePair(  VerticalDatumGeoKey,	4098)  -- Section 6.3.4.2 codes   --
ValuePair(  VerticalUnitsGeoKey,	4099)  -- Section 6.3.1 (.x) codes   --
</pre>
*/

int GTIFKeyGet(GTIF *gtif, geokey_t thekey, void *val, int nIndex, int count)
{
        int kindex = gtif->gt_keyindex[ thekey ];
        GeoKey *key;
        gsize_t size;
        char *data;
        tagtype_t type;

        if (!kindex) return 0;

        key = gtif->gt_keys+kindex;
        if (!count) count = (int) (key->gk_count - nIndex);
        if (count <=0) return 0;
        if (count > key->gk_count) count = (int) key->gk_count;
        size = key->gk_size;
        type = key->gk_type;

        if (count==1 && type==TYPE_SHORT) data = (char *)&key->gk_data;
        else data = key->gk_data;

        _GTIFmemcpy( val, data + nIndex*size, count*size );

        if (type==TYPE_ASCII)
           ((char *)val)[count-1] = '\0'; /* replace last char with NULL */

        return count;
}
