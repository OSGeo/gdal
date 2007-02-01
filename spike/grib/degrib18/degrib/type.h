/*****************************************************************************
 * type.h
 *
 * DESCRIPTION
 *    This file contains some simple common types used by this project.
 *
 * HISTORY
 *   12/2002 Arthur Taylor (MDL / RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
#ifndef TYPE_H
#define TYPE_H

#ifndef SINT4_TYPE
 #define SINT4_TYPE
/*   #ifdef _64Bit
 *   typedef of sInt4 and uInt4 determination now moved to config.h  
 *    A D T Aug 26, 2006
 *   Moved back to here with assumption that SIZEOF_LONG_INT is set
 *    by makefile.
 *    AATaylor 9/20/2006
 */

#if SIZEOF_LONG_INT != 4
  typedef signed int sInt4;
  typedef unsigned int uInt4;
 #else
  typedef signed long int sInt4;
  typedef unsigned long int uInt4;
 #endif
 typedef unsigned char uChar;
 typedef signed char sChar;
 typedef unsigned short int uShort2;
 typedef signed short int sShort2;
 /* Use size_t for unsigned int when you don't care the size (>= 2) */
 /* Use char (0, 1) for boolean */
#endif

/* #define LATLON_DECIMALS 6 */
typedef struct {
   double lat, lon;
} LatLon;

typedef struct {
   uChar f_valid;
   double X, Y;
} Point;

#endif
