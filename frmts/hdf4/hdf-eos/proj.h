/*
   0 = Geographic
   1 = Universal Transverse Mercator (UTM)
   2 = State Plane Coordinates
   3 = Albers Conical Equal Area
   4 = Lambert Conformal Conic
   5 = Mercator
   6 = Polar Stereographic
   7 = Polyconic
   8 = Equidistant Conic
   9 = Transverse Mercator
  10 = Stereographic
  11 = Lambert Azimuthal Equal Area
  12 = Azimuthal Equidistant
  13 = Gnomonic
  14 = Orthographic
  15 = General Vertical Near-Side Perspective
  16 = Sinusiodal
  17 = Equirectangular
  18 = Miller Cylindrical
  19 = Van der Grinten
  20 = (Hotine) Oblique Mercator 
  21 = Robinson
  22 = Space Oblique Mercator (SOM)
  23 = Alaska Conformal
  24 = Interrupted Goode Homolosine 
  25 = Mollweide
  26 = Interrupted Mollweide
  27 = Hammer
  28 = Wagner IV
  29 = Wagner VII
  30 = Oblated Equal Area
  31 = Integerized Sinusoidal Grid
  98 = User defined for Behrmann Cylindrical Equal-Area (of EASE Grid)
  99 = User defined for Integerized Sinusoidal Grid 
*/

#ifdef __cplusplus
extern "C" {
#endif

#define GEO 0
#define UTM 1
#define SPCS 2
#define ALBERS 3
#define LAMCC 4
#define MERCAT 5
#define PS 6
#define POLYC 7
#define EQUIDC 8
#define TM 9
#define STEREO 10
#define LAMAZ 11
#define AZMEQD 12
#define GNOMON 13
#define ORTHO 14
#define GVNSP 15
#define SNSOID 16
#define EQRECT 17
#define MILLER 18
#define VGRINT 19
#define HOM 20
#define ROBIN 21
#define SOM 22
#define ALASKA 23
#define GOODE 24
#define MOLL 25
#define IMOLL 26
#define HAMMER 27
#define WAGIV 28
#define WAGVII 29
#define OBEQA 30
#define ISINUS1 31
#define USDEF2 98
#define USDEF 99 

#define IN_BREAK -2
#define COEFCT 15		/*  projection coefficient count	     */
#define PROJCT 31		/*  projection count			     */
#define DATMCT 20		/*  datum count				     */

#define MAXPROJ 30		/*  Maximum projection number */
#define MAXUNIT 5		/*  Maximum unit code number */
#define GEO_TERM 0		/*  Array index for print-to-term flag */
#define GEO_FILE 1		/*  Array index for print-to-file flag */
#define GEO_TRUE 1		/*  True value for geometric true/false flags */
#define GEO_FALSE -1		/*  False val for geometric true/false flags */

#ifdef __cplusplus
}
#endif
