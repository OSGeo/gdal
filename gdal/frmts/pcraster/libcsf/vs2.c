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

/* test if valuescale/datatype is a CSF version 2 value scale
 * RvalueScale2 tests if the map's value scale is a version
 * 2 valuescale/datatype.
 * returns
 * 0 if the value is not in the above list, 1 if it does.
 *
 */
int RvalueScale2(
	CSF_VS    vs) /* value scale. ALL OF BELOW are accepted */
{
	switch(vs) {
	  case VS_LDD: 
	 case VS_BOOLEAN:
	 case VS_NOMINAL:
	 case VS_ORDINAL: 
	 case VS_SCALAR: 
	 case VS_DIRECTION: return 1;
	 default :       return 0;
      }
}
