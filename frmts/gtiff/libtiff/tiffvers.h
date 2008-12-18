#define TIFFLIB_VERSION_STR "LIBTIFF, Version 4.0.0beta3\nCopyright (c) 1988-1996 Sam Leffler\nCopyright (c) 1991-1996 Silicon Graphics, Inc."
/*
 * This define can be used in code that requires
 * compilation-related definitions specific to a
 * version or versions of the library.  Runtime
 * version checking should be done based on the
 * string returned by TIFFGetVersion.
 */
#define TIFFLIB_VERSION 20081217

/*
 * This define contains the library version number encode as 
 * two digits each for major, minor and point.  So, for instance,
 * 3.9.8 would be 030908.  This define was only introduced in libtiff4. 
 */
#define TIFFLIB_RELEASE 040000


