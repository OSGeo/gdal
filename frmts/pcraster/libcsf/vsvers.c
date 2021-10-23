#include "csf.h" 

/* global header (opt.) and vsvers's prototypes "" */


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

/* get version nr of value scale
 * returns
 * 0 for illegal value scale,
 * 1 version 1 value scale,
 * 2 version 2 value scale
 */
int RgetValueScaleVersion(
	const MAP *m) /* map handle */
{
	UINT2 vs = RgetValueScale(m);
	
	switch(vs) {
	  case VS_CLASSIFIED   : 
	  case VS_CONTINUOUS   : 
	  case VS_NOTDETERMINED: return 1;
	  case VS_LDD      : 
	  case VS_BOOLEAN  :
	  case VS_NOMINAL  :
	  case VS_ORDINAL  : 
	  case VS_SCALAR   :
	  case VS_DIRECTION: return 2;
	  default          : return 0;
      }
}
