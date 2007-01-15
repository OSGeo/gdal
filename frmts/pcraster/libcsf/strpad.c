/********/
/* USES */
/********/

/* libs ext. <>, our ""  */
#include <string.h>

/* global header (opt.) and strpad's prototypes "" */
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
/* pad a string attribute with zeros (LIBRARY_INTERNAL)
 */
char *CsfStringPad(char *s, size_t reqSize)
{
	size_t l = strlen(s);
	PRECOND(l <= reqSize);
	(void)memset(s+l, '\0', reqSize-l);
	return s;
}
