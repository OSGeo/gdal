/********/
/* USES */
/********/

/* libs ext. <>, our ""  */
#include "csf.h"

/* global header (opt.) and swapio's prototypes "" */
#include "csfimpl.h" 


/* headers of this app. modules called */ 

/***************/
/* EXTERNALS   */
/***************/

/**********************/ 
/* LOCAL DECLARATIONS */
/**********************/ 

/* typedef for swap functions (LIBRARY_INTERNAL)
 * typedef for swap functions 
 */
typedef void (*SWAP)(unsigned char *buf,  size_t n);
/*********************/ 
/* LOCAL DEFINITIONS */
/*********************/ 

/******************/
/* IMPLEMENTATION */
/******************/

/* check valid size of element (LIBRARY_INTERNAL)
 */
int CsfValidSize(size_t size) 
{
 	return size == 1 || size == 2 || size == 4 || size == 8;
}

#ifdef DEBUG
 size_t CsfWritePlain(void *buf, size_t size, size_t n, FILE  *f)
 {
  PRECOND(CsfValidSize(size));
  return fwrite(buf, size, n, f);
 }
 size_t CsfReadPlain(void *buf, size_t size, size_t n, FILE  *f)
 {
  PRECOND(CsfValidSize(size));
  return fread(buf, size, n, f);
 }
#endif

/* ARGSUSED */
static void Swap1(unsigned char * buf,  size_t n)
{
	/* do nothing */
}

static void Swap2(unsigned char *b,  size_t n)
{
	unsigned char tmp;
	size_t i;
	for (i=0; i < n; i++)
	{
	 /* 01 => 10 */
	 tmp = b[0]; b[0] = b[1]; b[1] = tmp;
	 b += 2;
	}
}

static void Swap4(unsigned char *b,  size_t n)
{
	unsigned char tmp;
	size_t i;
	for (i=0; i < n; i++)
	{
        /* 0123 => 3210 */
	tmp = b[0]; b[0] = b[3]; b[3] = tmp;
	tmp = b[1]; b[1] = b[2]; b[2] = tmp;
	b += 4;
        }
}

static void Swap8(unsigned char *b,  size_t n)
{
	unsigned char tmp;
	size_t i;
	for (i=0; i < n; i++)
	{
	/* 01234567 => 76543210 */
	tmp = b[0]; b[0] = b[7]; b[7] = tmp;
	tmp = b[1]; b[1] = b[6]; b[6] = tmp;
	tmp = b[2]; b[2] = b[5]; b[5] = tmp;
	tmp = b[3]; b[3] = b[4]; b[4] = tmp;
	b += 8;
	}
}

void CsfSwap(void *buf, size_t size, size_t n)
{
	SWAP l[9] = { NULL, Swap1, Swap2, NULL, Swap4,
                      NULL, NULL,  NULL,  Swap8};
        PRECOND(CsfValidSize(size));
	PRECOND(l[size] != NULL);
	
	l[size]((unsigned char *)buf,n);
}

size_t CsfWriteSwapped(void *buf, size_t size, size_t n, FILE  *f)
{
	CsfSwap(buf,size, n);
	return fwrite(buf, size, n,f);
}

size_t CsfReadSwapped(void *buf, size_t size, size_t n, FILE  *f)
{
	size_t r = fread(buf, size, n,f);
	CsfSwap(buf,size, r);
	return r;
}
