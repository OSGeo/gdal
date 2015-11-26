/**********************************************************************
 *
 *  geokeys.h - Public registry for valid GEOTIFF GeoKeys.
 *
 *   Written By: Niles D. Ritter
 *
 *  copyright (c) 1995   Niles D. Ritter
 *
 *  Permission granted to use this software, so long as this copyright
 *  notice accompanies any products derived therefrom.
 **********************************************************************/

#ifndef LIBGEOTIFF_GEOKEYS_H_
#define LIBGEOTIFF_GEOKEYS_H_

/* The GvCurrentRevision number should be incremented whenever a 
 * new set of Keys are defined or modified in "geokeys.inc", and comments 
 * added to the "Revision History" section above. If only code
 * _values_ are augmented, the "GvCurrentMinorRev" number should
 * be incremented instead (see "geovalues.h"). Whenever the 
 * GvCurrentRevision is incremented, the GvCurrentMinorRev should
 * be reset to zero.
 *
 *
 * The Section Numbers below refer to the GeoTIFF Spec sections
 * in which these values are documented.
 *
 */
#define GvCurrentRevision  1  /* Final 1.0 Release */

#ifdef ValuePair
#  undef ValuePair
#endif
#define ValuePair(name,value)    name = value,

typedef enum {
   BaseGeoKey   =  1024,               /* First valid code */

#  include "geokeys.inc"         /* geokey database */

   ReservedEndGeoKey  =  32767,
   
   /* Key space available for Private or internal use */
   PrivateBaseGeoKey = 32768,    /* Consistent with TIFF Private tags */
   PrivateEndGeoKey  = 65535,    
   
   EndGeoKey = 65535             /* Largest Possible GeoKey ID */
} geokey_t;


#endif /* LIBGEOTIFF_GEOKEYS_H_ */
