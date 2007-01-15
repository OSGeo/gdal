/*
 * kernlcsf.c
 *    Functions to create  and maintain the csf-kernel
 *     runtime structures
 */

/**************************************************************************/
/*  KERNLCSF.C                                                            */
/*                                                                        */
/*                                                                        */
/**************************************************************************/

/********/
/* USES */
/********/
#include "csf.h"
#include "csfimpl.h"

#include <stdlib.h>

/***************/
/* EXTERNALS   */
/***************/

static MAP **mapList    = NULL;
static size_t mapListLen = 4;

/*********************/
/* LOCAL DEFINITIONS */
/*********************/

/**********************/
/* LOCAL DECLARATIONS */
/**********************/

/******************/
/* IMPLEMENTATION */
/******************/


/* close all open maps at exit  (LIBRARY_INTERNAL)
 * passed through atexit to c-library
 * exit code
 */
static void CsfCloseCsfKernel(void)
{
  size_t i;

  for(i = 0; i < mapListLen; i++)
   if(mapList[i] != NULL)
    if(Mclose(mapList[i]))
      (void)fprintf(stderr,"CSF_INTERNAL_ERROR: unable to close %s at exit\n",
        mapList[i]->fileName);
  CSF_FREE(mapList);
  mapList = NULL;
}

/* boot the CSF runtime library (LIBRARY_INTERNAL)
 * CsfBootCsfKernel creates the mapList and adds the function to
 *  close all files at a system exit
 *
 * NOTE
 * Note that CsfBootCsfKernel never returns if there isn't enough
 * memory to allocate an array of mapListLen pointers, or if
 * the atexit() call fails
 */
void CsfBootCsfKernel(void)
{
  POSTCOND(mapList == NULL);

  mapList = (MAP **)calloc(mapListLen,sizeof(MAP *));
  if (mapList == NULL) {
    (void)fprintf(stderr,"CSF_INTERNAL_ERROR: Not enough memory to use CSF-files\n");
    exit(1);
  }

  if (atexit(CsfCloseCsfKernel)) {
    (void)fprintf(stderr,"CSF_INTERNAL_ERROR: Impossible to close CSF-files automatically at exit\n");
    exit(1);
  }
}

/* check if the kernel is booted (LIBRARY_INTERNAL)
 * returns 0 if not, nonzero if already booted
 */
int CsfIsBootedCsfKernel(void)
{
  return(mapList != NULL);
}

/* put map in run time structure (LIBRARY_INTERNAL)
 * Every map opened or created is
 * registered in a list for verification
 * if functions get a valid map handle
 * passed and for automatic closing
 * at exit if the application forgets it.
 */
void CsfRegisterMap(
  MAP *m) /* map handle to be registered, the field m->mapListId is
           * initialized
           */
{
  size_t i=0;

  while (mapList[i] != NULL && i < mapListLen)
    i++;

  if(i == mapListLen)
  {
    size_t j;
    /* double size */
    mapListLen *=2;
    mapList=realloc(mapList,sizeof(MAP *)*mapListLen);
    if (mapList == NULL) {
     (void)fprintf(stderr,"CSF_INTERNAL_ERROR: Not enough memory to use CSF-files\n");
      exit(1);
    }
    /* initialize new part, i at begin */
    for(j=i; j < mapListLen; ++j)
      mapList[j]=0;
  }

  mapList[i] =   m;
  m->mapListId = i;
}

/* remove map from run time structure (LIBRARY_INTERNAL)
 * The map handle will become invalid.
 */
void CsfUnloadMap(
  MAP *m) /* map handle */
{
  POSTCOND(CsfIsValidMap(m));

  mapList[m->mapListId] = NULL;
  m->mapListId = -1;
}

/* check if the map handle is created via the csf kernel (LIBRARY_INTERNAL)
 */
int CsfIsValidMap(
  const MAP *m) /* map handle */
{
  return(CsfIsBootedCsfKernel() && m != NULL
          && m->mapListId >= 0 && ((size_t)m->mapListId) < mapListLen
    && mapList[m->mapListId] == m);
}
