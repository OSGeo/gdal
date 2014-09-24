/*
 * vsis.c 
 */

/********/
/* USES */
/********/

/* libs ext. <>, our ""  */

/* global header (opt.) and vsis's prototypes "" */
#include "csf.h" 
#include "csfimpl.h" 


/* headers of this app. modules called */ 

/***************/
/* EXTERNALS   */
/***************/

/**********************/ 
/* LOCAL DECLARATIONS */
/**********************/ 

/*********************/ 
/* LOCAL DEFINITIONS */
/*********************/ 

/******************/
/* IMPLEMENTATION */
/******************/

/* test value scale on compatibility CSF version 1 and 2
 * RvalueScaleIs tests if the map's value scale is compatible
 * with a certain value scale. Here is list of compatible but
 * different value scales: 
 *   
 * VS_NOTDETERMINED: always returns 0
 *
 * VS_CLASSIFIED: VS_NOTDETERMINED
 *
 * VS_CONTINUOUS: VS_NOTDETERMINED
 *
 * VS_BOOLEAN: VS_CLASSIFIED, VS_NOTDETERMINED
 *
 * VS_NOMINAL: VS_CLASSIFIED, VS_NOTDETERMINED
 *
 * VS_ORDINAL: VS_CLASSIFIED, VS_NOTDETERMINED
 *
 * VS_LDD:  VS_CLASSIFIED, VS_NOTDETERMINED (only if cell representation is
 * UINT1 or INT2)
 *
 * VS_SCALAR:  VS_CONTINUOUS, VS_NOTDETERMINED
 *
 * VS_DIRECTION: none 
 *
 * returns
 * 0 if not compatible or if vs argument is VS_NOTDETERMINED or in case of
 * error, nonzero if 
 * compatible.
 *
 * Merrno
 * BAD_VALUESCALE
 *
 * EXAMPLE
 * .so examples/maskdump.tr
 */
int RvalueScaleIs(
	const MAP *m, /* a version 1 map handle */
	CSF_VS     vs) /* a version 2 value scale that is compatible with map's value
	               * scale yes or no?
	               */
{
	CSF_VS mapsVS = RgetValueScale(m);
	
	if (vs == VS_NOTDETERMINED)
		return 0;

	if (vs == mapsVS)
		return 1;

	switch(vs) {
	  case VS_CLASSIFIED: return mapsVS == VS_NOTDETERMINED;
	  case VS_CONTINUOUS: return mapsVS == VS_NOTDETERMINED;
	  case VS_LDD: 
	                { CSF_CR cr = RgetCellRepr(m);
	                  if (cr !=  CR_UINT1 && cr != CR_INT2)
	                   return 0;
	                } /* fall through */
	 case VS_BOOLEAN:
	 case VS_NOMINAL:
	 case VS_ORDINAL:  return mapsVS == VS_CLASSIFIED 
			    || mapsVS == VS_NOTDETERMINED;
	 case VS_SCALAR:   return mapsVS == VS_CONTINUOUS
	                    || mapsVS == VS_NOTDETERMINED;
	/* direction isn't compatible with anything */
	 case VS_DIRECTION: return 0;
	 default          : M_ERROR(BAD_VALUESCALE);
	                    return 0;
      }
}
