/*****************************************************************************
 * $Id$
 *
 * This module has a number of additions and improvements over the original
 * implementation to be suitable for usage in GDAL HDF driver.
 *
 * Andrey Kiselev <dron@ak4719.spb.edu> is responsible for all the changes.
 ****************************************************************************/

/*
Copyright (C) 1996 Hughes and Applied Research Corporation

Permission to use, modify, and distribute this software and its documentation 
for any purpose without fee is hereby granted, provided that the above 
copyright notice appear in all copies and that both that copyright notice and 
this permission notice appear in supporting documentation.
*/
/*****************************************************************************
REVISIONS:

Aug 31, 1999  Abe Taaheri    Changed memory allocation for utility strings to
                             the size of UTLSTR_MAX_SIZE.
			     Added error check for memory unavailibilty in 
			     several functions.
			     Added check for NULL metabuf returned from 
			     EHmeta... functions. NULL pointer retruned from 
			     EHmeta... functions indicate that memory could not
			     be allocated for metabuf.

June 05, 2003 Abe Taaheri / Bruce Beaumont

                            Changed MAXNREGIONS to 1024 to support MOPITT data
			    Supplied cast for compcode in call to 
			      SDsetcompress to avoid compiler error
			    Removed declaration for unused variable rstatus 
			      in SWwrrdfield
			    Removed initialization code for unused variables 
			      in SWwrrdfield
			    Removed declaration for unused variable tmpVal 
			      in SWdefboxregion
			    Added code in SWdefboxregion to check for index k
			      exceeding NSWATHREGN to avoid overwriting 
			      memory
			    Removed declaration for unused variable retchar 
			      in SWregionindex
			    Removed initialization code for unused variables 
			      in SWregionindex
			    Removed declarations for unused variables tstatus,
			      nfields, nflgs, and swathname in SWextractregion
			    Removed initialization code for unused variables 
			      in SWextractregion
			    Removed declaration for unused variable 
			      land_status in SWscan2longlat
			    Removed initialization code for unused variables 
			      in SWscan2longlat
			    Added clear (0) of timeflag in SWextractperiod if 
			      return status from SWextractregion is non-zero
			    Removed declarations for unused variables tstatus,
			      scandim, ndfields, ndflds, and swathname in 
			      SWregioninfo
			    Removed initialization code for unused variables 
			      in SWregioninfo
			    Added clear (0) of timeflag in SWperiodinfo if 
			      return status from SWregioninfo is non-zero
			    Removed declarations for unused variables size, 
			      nfields, nflds, nswath, idxsz, cornerlon, and 
			      cornerlat in SWdefscanregion
			    Removed initialization code for unused variables 
			      in SWdefscanregion
			    Removed declarations for unused variables dims2, 
			      rank, nt, swathname, dimlist, and buffer in 
			      SWupdateidxmap
			    Removed declaration for unused variable statmeta 
			      in SWgeomapinfo
******************************************************************************/

#include "mfhdf.h"
#include "hcomp.h"
#include "HdfEosDef.h"
#include <math.h>

#include "hdf4compat.h"

#define SWIDOFFSET 1048576


static int32 SWX1dcomb[512*3];
static int32 SWXSDcomb[512*5];
static char  SWXSDname[HDFE_NAMBUFSIZE];
static char  SWXSDdims[HDFE_DIMBUFSIZE];

/* This flag was added to allow the Time field to have different Dimensions
** than Longitude and Latitude and still be used for subsetting
** 23 June,1997  DaW
*/
static intn  timeflag = 0;


/* Added for routine that converts scanline to Lat/long
** for floating scene subsetting
** Jul 1999 DaW
*/
#define PI      3.141592653589793238
#define RADOE	6371.0		/* Radius of Earth in Km */

#define NSWATH 200
/* Swath Structure External Arrays */
struct swathStructure 
{
    int32 active;
    int32 IDTable;
    int32 VIDTable[3];
    int32 fid;
    int32 nSDS;
    int32 *sdsID;
    int32 compcode;
    intn  compparm[5];
    int32 tilecode;
    int32 tilerank;
    int32 tiledims[8];
};
static struct swathStructure SWXSwath[NSWATH];



#define NSWATHREGN 256
#define MAXNREGIONS 1024
struct swathRegion
{
    int32 fid;
    int32 swathID;
    int32 nRegions;
    int32 StartRegion[MAXNREGIONS];
    int32 StopRegion[MAXNREGIONS];
    int32 StartVertical[8];
    int32 StopVertical[8];
    int32 StartScan[8];
    int32 StopScan[8];
    char *DimNamePtr[8];
    intn band8flag;
    intn  scanflag;
};
static struct swathRegion *SWXRegion[NSWATHREGN];

/* define a macro for the string size of the utility strings. The value
   of 80 in previous version of this code was resulting in core dump (Array 
   Bounds Write and Array Bounds Read problem in SWfinfo function and the 
   functions called from there) for 7-8 dimensional fields where the 
   string length for "DimList" can exceed 80 characters, including " and 
   commas in the string. The length now is 512 which seems to be more 
   than enough to avoid the problem mentioned above. */
   
#define UTLSTR_MAX_SIZE 512

/* Swath Prototypes (internal routines) */
static intn SWchkswid(int32, char *, int32 *, int32 *, int32 *);
static int32 SWfinfo(int32, const char *, const char *, int32 *,
                     int32 [], int32 *, char *);
static intn SWdefinefield(int32, char *, char *, char *, int32, int32);
static intn SWwrrdattr(int32, char *, int32, int32, char *, VOIDP);
static intn SW1dfldsrch(int32, int32, const char *, const char *, int32 *,
                        int32 *, int32 *);
static intn SWSDfldsrch(int32, int32, const char *, int32 *, int32 *, 
                        int32 *, int32 *, int32 [], int32 *);
static intn SWwrrdfield(int32, const char *, const char *,
                        int32 [], int32 [], int32 [], VOIDP);
static int32 SWinqfields(int32, char *, char *, int32 [], int32 []);
static intn SWscan2longlat(int32, char *, VOIDP, int32 [], int32 [],
                           int32 *, int32, int32);


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWopen                                                           |
|                                                                             |
|  DESCRIPTION:                                                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  fid            int32               HDF-EOS file ID                         |
|                                                                             |
|  INPUTS:                                                                    |
|  filename       char                Filename                                |
|  access         intn                HDF access code                         |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
SWopen(char *filename, intn access)

{
    int32           fid /* HDF-EOS file ID */ ;

    /* Call EHopen to perform file access */
    /* ---------------------------------- */
    fid = EHopen(filename, access);

    return (fid);
}

/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWcreate                                                         |
|                                                                             |
|  DESCRIPTION: Creates a new swath structure and returns swath ID            |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  swathID        int32               Swath structure ID                      |
|                                                                             |
|  INPUTS:                                                                    |
|  fid            int32               File ID                                 |
|  swathname      char                Swath structure name                    |
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Aug 96   Joel Gales    Make metadata ODL compliant                         |
|  Sep 96   Joel Gales    Check swath name for length                         |
|  Mar 97   Joel Gales    Enlarge utlbuf to 512                               |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
SWcreate(int32 fid, char *swathname)
{
    intn            i;		/* Loop index */
    intn            nswathopen = 0;	/* # of swath structures open */
    intn            status = 0;	/* routine return status variable */

    uint8           access;	/* Read/Write file access code */

    int32           HDFfid;	/* HDF file id */
    int32           vgRef;	/* Vgroup reference number */
    int32           vgid[4];	/* Vgroup ID array */
    int32           swathID = -1;	/* HDF-EOS swath ID */

    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           idOffset = SWIDOFFSET;	/* Swath ID offset */
    int32           nSwath = 0;	/* Swath counter */

    char            name[80];	/* Vgroup name */
    char            class[80];	/* Vgroup class */
    char            errbuf[256];/* Buffer for error message */
    char            utlbuf[512];/* Utility buffer */
    char            utlbuf2[32];/* Utility buffer 2 */

    /*
     * Check HDF-EOS file ID, get back HDF file ID, SD interface ID  and
     * access code
     */
    status = EHchkfid(fid, swathname, &HDFfid, &sdInterfaceID, &access);


    /* Check swathname for length */
    /* -------------------------- */
    if ((intn) strlen(swathname) > VGNAMELENMAX)
    {
	status = -1;
	HEpush(DFE_GENAPP, "SWcreate", __FILE__, __LINE__);
	HEreport("Swathname \"%s\" must be less than %d characters.\n",
		 swathname, VGNAMELENMAX);
    }


    if (status == 0)
    {

	/* Determine number of swaths currently opened */
	/* ------------------------------------------- */
	for (i = 0; i < NSWATH; i++)
	{
	    nswathopen += SWXSwath[i].active;
	}


	/* Setup file interface */
	/* -------------------- */
	if (nswathopen < NSWATH)
	{

	    /* Check that swath has not been previously opened */
	    /* ----------------------------------------------- */
	    vgRef = -1;

	    while (1)
	    {
		vgRef = Vgetid(HDFfid, vgRef);

		/* If no more Vgroups then exist while loop */
		/* ---------------------------------------- */
		if (vgRef == -1)
		{
		    break;
		}

		/* Get name and class of Vgroup */
		/* ---------------------------- */
		vgid[0] = Vattach(HDFfid, vgRef, "r");
		Vgetname(vgid[0], name);
		Vgetclass(vgid[0], class);
		Vdetach(vgid[0]);

		/* If SWATH then increment # swath counter */
		/* --------------------------------------- */
		if (strcmp(class, "SWATH") == 0)
		{
		    nSwath++;
		}

		/* If swath already exist, return error */
		/* ------------------------------------ */
		if (strcmp(name, swathname) == 0 &&
		    strcmp(class, "SWATH") == 0)
		{
		    status = -1;
		    HEpush(DFE_GENAPP, "SWcreate", __FILE__, __LINE__);
		    HEreport("\"%s\" already exists.\n", swathname);
		    break;
		}
	    }


	    if (status == 0)
	    {

		/* Create Root Vgroup for Swath */
		/* ---------------------------- */
		vgid[0] = Vattach(HDFfid, -1, "w");


		/* Set Name and Class (SWATH) */
		/* -------------------------- */
		Vsetname(vgid[0], swathname);
		Vsetclass(vgid[0], "SWATH");



		/* Create Geolocation Fields Vgroup */
		/* -------------------------------- */
		vgid[1] = Vattach(HDFfid, -1, "w");
		Vsetname(vgid[1], "Geolocation Fields");
		Vsetclass(vgid[1], "SWATH Vgroup");
		Vinsert(vgid[0], vgid[1]);



		/* Create Data Fields Vgroup */
		/* ------------------------- */
		vgid[2] = Vattach(HDFfid, -1, "w");
		Vsetname(vgid[2], "Data Fields");
		Vsetclass(vgid[2], "SWATH Vgroup");
		Vinsert(vgid[0], vgid[2]);



		/* Create Attributes Vgroup */
		/* ------------------------ */
		vgid[3] = Vattach(HDFfid, -1, "w");
		Vsetname(vgid[3], "Swath Attributes");
		Vsetclass(vgid[3], "SWATH Vgroup");
		Vinsert(vgid[0], vgid[3]);



		/* Establish Swath in Structural MetaData Block */
		/* -------------------------------------------- */
		sprintf(utlbuf, "%s%ld%s%s%s",
			"\tGROUP=SWATH_", (long)nSwath + 1,
			"\n\t\tSwathName=\"", swathname, "\"\n");

		strcat(utlbuf, "\t\tGROUP=Dimension\n");
		strcat(utlbuf, "\t\tEND_GROUP=Dimension\n");
		strcat(utlbuf, "\t\tGROUP=DimensionMap\n");
		strcat(utlbuf, "\t\tEND_GROUP=DimensionMap\n");
		strcat(utlbuf, "\t\tGROUP=IndexDimensionMap\n");
		strcat(utlbuf, "\t\tEND_GROUP=IndexDimensionMap\n");
		strcat(utlbuf, "\t\tGROUP=GeoField\n");
		strcat(utlbuf, "\t\tEND_GROUP=GeoField\n");
		strcat(utlbuf, "\t\tGROUP=DataField\n");
		strcat(utlbuf, "\t\tEND_GROUP=DataField\n");
		strcat(utlbuf, "\t\tGROUP=MergedFields\n");
		strcat(utlbuf, "\t\tEND_GROUP=MergedFields\n");
		sprintf(utlbuf2, "%s%ld%s",
			"\tEND_GROUP=SWATH_", (long)nSwath + 1, "\n");
		strcat(utlbuf, utlbuf2);


		status = EHinsertmeta(sdInterfaceID, "", "s", 1001L,
				      utlbuf, NULL);
	    }
	}
	else
	{
	    /* Too many files opened */
	    /* --------------------- */
	    status = -1;
	    strcpy(errbuf,
		   "No more than %d swaths may be open simutaneously");
	    strcat(errbuf, " (%s)");
	    HEpush(DFE_DENIED, "SWcreate", __FILE__, __LINE__);
	    HEreport(errbuf, NSWATH, swathname);
	}


	/* Assign swathID # & Load swath and SWXSwath table entries */
	/* -------------------------------------------------------- */
	if (status == 0)
	{

	    for (i = 0; i < NSWATH; i++)
	    {
		if (SWXSwath[i].active == 0)
		{
		    /*
		     * Set swathID, Set swath entry active, Store root Vgroup
		     * ID, Store sub Vgroup IDs, Store HDF-EOS file ID
		     */
		    swathID = i + idOffset;
		    SWXSwath[i].active = 1;
		    SWXSwath[i].IDTable = vgid[0];
		    SWXSwath[i].VIDTable[0] = vgid[1];
		    SWXSwath[i].VIDTable[1] = vgid[2];
		    SWXSwath[i].VIDTable[2] = vgid[3];
		    SWXSwath[i].fid = fid;
		    status = 0;
		    break;
		}
	    }

	}
    }
    return (swathID);
}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWattach                                                         |
|                                                                             |
|  DESCRIPTION:  Attaches to an existing swath within the file.               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  swathID        int32               swath structure ID                      |
|                                                                             |
|  INPUTS:                                                                    |
|  fid            int32               HDF-EOS file ID                         |
|  swathname      char                swath structure name                    |
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Apr 99   David Wynne   Modified test for memory allocation check when no   |
|                         SDSs are in the Swath, NCR22513                     |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
SWattach(int32 fid, char *swathname)

{
    intn            i;		/* Loop index */
    intn            j;		/* Loop index */
    intn            nswathopen = 0;	/* # of swath structures open */
    intn            status;	/* routine return status variable */

    uint8           acs;	/* Read/Write file access code */

    int32           HDFfid;	/* HDF file id */
    int32           vgRef;	/* Vgroup reference number */
    int32           vgid[4];	/* Vgroup ID array */
    int32           swathID = -1;	/* HDF-EOS swath ID */
    int32          *tags;	/* Pnt to Vgroup object tags array */
    int32          *refs;	/* Pnt to Vgroup object refs array */
    int32           dum;	/* dummy varible */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           nObjects;	/* # of objects in Vgroup */
    int32           nSDS;	/* SDS counter */
    int32           index;	/* SDS index */
    int32           sdid;	/* SDS object ID */
    int32           idOffset = SWIDOFFSET;	/* Swath ID offset */

    char            name[80];	/* Vgroup name */
    char            class[80];	/* Vgroup class */
    char            errbuf[256];/* Buffer for error message */
    char            acsCode[1];	/* Read/Write access char: "r/w" */


    /* Check HDF-EOS file ID, get back HDF file ID and access code */
    /* ----------------------------------------------------------- */
    status = EHchkfid(fid, swathname, &HDFfid, &dum, &acs);


    if (status == 0)
    {
	/* Convert numeric access code to character */
	/* ---------------------------------------- */
	acsCode[0] = (acs == 1) ? 'w' : 'r';

	/* Determine number of swaths currently opened */
	/* ------------------------------------------- */
	for (i = 0; i < NSWATH; i++)
	{
	    nswathopen += SWXSwath[i].active;
	}

	/* If room for more ... */
	/* -------------------- */
	if (nswathopen < NSWATH)
	{

	    /* Search Vgroups for Swath */
	    /* ------------------------ */
	    vgRef = -1;

	    while (1)
	    {
		vgRef = Vgetid(HDFfid, vgRef);


		/* If no more Vgroups then exist while loop */
		/* ---------------------------------------- */
		if (vgRef == -1)
		{
		    break;
		}

		/* Get name and class of Vgroup */
		/* ---------------------------- */
		vgid[0] = Vattach(HDFfid, vgRef, "r");
		Vgetname(vgid[0], name);
		Vgetclass(vgid[0], class);


		/*
		 * If Vgroup with swathname and class SWATH found, load
		 * tables
		 */

		if (strcmp(name, swathname) == 0 &&
		    strcmp(class, "SWATH") == 0)
		{
		    /* Attach to "Fields" and "Swath Attributes" Vgroups */
		    /* ------------------------------------------------- */
		    tags = (int32 *) malloc(sizeof(int32) * 3);
		    if(tags == NULL)
		    { 
			HEpush(DFE_NOSPACE,"SWattach", __FILE__, __LINE__);
			return(-1);
		    }
		    refs = (int32 *) malloc(sizeof(int32) * 3);
		    if(refs == NULL)
		    { 
			HEpush(DFE_NOSPACE,"SWattach", __FILE__, __LINE__);
			free(tags);
			return(-1);
		    }
		    Vgettagrefs(vgid[0], tags, refs, 3);
		    vgid[1] = Vattach(HDFfid, refs[0], acsCode);
		    vgid[2] = Vattach(HDFfid, refs[1], acsCode);
		    vgid[3] = Vattach(HDFfid, refs[2], acsCode);
		    free(tags);
		    free(refs);

		    /* Setup External Arrays */
		    /* --------------------- */
		    for (i = 0; i < NSWATH; i++)
		    {
			/* Find empty entry in array */
			/* ------------------------- */
			if (SWXSwath[i].active == 0)
			{
			    /*
			     * Set swathID, Set swath entry active, Store
			     * root Vgroup ID, Store sub Vgroup IDs, Store
			     * HDF-EOS file ID
			     */
			    swathID = i + idOffset;
			    SWXSwath[i].active = 1;
			    SWXSwath[i].IDTable = vgid[0];
			    SWXSwath[i].VIDTable[0] = vgid[1];
			    SWXSwath[i].VIDTable[1] = vgid[2];
			    SWXSwath[i].VIDTable[2] = vgid[3];
			    SWXSwath[i].fid = fid;
			    break;
			}
		    }

		    /* Get SDS interface ID */
		    /* -------------------- */
		    status = SWchkswid(swathID, "SWattach", &dum,
				       &sdInterfaceID, &dum);


		    /* Access swath "Geolocation" SDS */
		    /* ------------------------------ */

		    /* Get # of entries within this Vgroup & search for SDS */
		    /* ---------------------------------------------------- */
		    nObjects = Vntagrefs(vgid[1]);

		    if (nObjects > 0)
		    {
			/* Get tag and ref # for Geolocation Vgroup objects */
			/* ------------------------------------------------ */
			tags = (int32 *) malloc(sizeof(int32) * nObjects);
			if(tags == NULL)
			{ 
			    HEpush(DFE_NOSPACE,"SWattach", __FILE__, __LINE__);
			    return(-1);
			}
			refs = (int32 *) malloc(sizeof(int32) * nObjects);
			if(refs == NULL)
			{ 
			    HEpush(DFE_NOSPACE,"SWattach", __FILE__, __LINE__);
			    free(tags);
			    return(-1);
			}
			Vgettagrefs(vgid[1], tags, refs, nObjects);

			/* Count number of SDS & allocate SDS ID array */
			/* ------------------------------------------- */
			nSDS = 0;
			for (j = 0; j < nObjects; j++)
			{
			    if (tags[j] == DFTAG_NDG)
			    {
				nSDS++;
			    }
			}
			SWXSwath[i].sdsID = (int32 *) calloc(nSDS, 4);
			if(SWXSwath[i].sdsID == NULL && nSDS != 0)
			{ 
			    HEpush(DFE_NOSPACE,"SWattach", __FILE__, __LINE__);
			    free(tags);
			    free(refs);
			    return(-1);
			}
			nSDS = 0;


			/* Fill SDS ID array */
			/* ----------------- */
			for (j = 0; j < nObjects; j++)
			{
			    /* If object is SDS then get id */
			    /* ---------------------------- */
			    if (tags[j] == DFTAG_NDG)
			    {
				index = SDreftoindex(sdInterfaceID, refs[j]);
				sdid = SDselect(sdInterfaceID, index);
				SWXSwath[i].sdsID[nSDS] = sdid;
				nSDS++;
				SWXSwath[i].nSDS++;
			    }
			}
			free(tags);
			free(refs);
		    }

		    /* Access swath "Data" SDS */
		    /* ----------------------- */

		    /* Get # of entries within this Vgroup & search for SDS */
		    /* ---------------------------------------------------- */
		    nObjects = Vntagrefs(vgid[2]);

		    if (nObjects > 0)
		    {
			/* Get tag and ref # for Data Vgroup objects */
			/* ----------------------------------------- */
			tags = (int32 *) malloc(sizeof(int32) * nObjects);
			if(tags == NULL)
			{ 
			    HEpush(DFE_NOSPACE,"SWattach", __FILE__, __LINE__);
			    return(-1);
			}
			refs = (int32 *) malloc(sizeof(int32) * nObjects);
			if(refs == NULL)
			{ 
			    HEpush(DFE_NOSPACE,"SWattach", __FILE__, __LINE__);
			    free(tags);
			    return(-1);
			}
			Vgettagrefs(vgid[2], tags, refs, nObjects);


			/* Count number of SDS & allocate SDS ID array */
			/* ------------------------------------------- */
			nSDS = 0;
			for (j = 0; j < nObjects; j++)
			{
			    if (tags[j] == DFTAG_NDG)
			    {
				nSDS++;
			    }
			}
			SWXSwath[i].sdsID = (int32 *)
			    realloc((void *) SWXSwath[i].sdsID,
				    (SWXSwath[i].nSDS + nSDS) * 4);
			if(SWXSwath[i].sdsID == NULL && nSDS != 0)
			{ 
			    HEpush(DFE_NOSPACE,"SWattach", __FILE__, __LINE__);
			    return(-1);
			}

			/* Fill SDS ID array */
			/* ----------------- */
			for (j = 0; j < nObjects; j++)
			{
			    /* If object is SDS then get id */
			    /* ---------------------------- */
			    if (tags[j] == DFTAG_NDG)
			    {
				index = SDreftoindex(sdInterfaceID, refs[j]);
				sdid = SDselect(sdInterfaceID, index);
				SWXSwath[i].sdsID[SWXSwath[i].nSDS] = sdid;
				SWXSwath[i].nSDS++;
			    }
			}
			free(tags);
			free(refs);
		    }
		    break;
		}

		/* Detach Vgroup if not desired Swath */
		/* ---------------------------------- */
		Vdetach(vgid[0]);
	    }

	    /* If Swath not found then set up error message */
	    /* -------------------------------------------- */
	    if (swathID == -1)
	    {
		HEpush(DFE_RANGE, "SWattach", __FILE__, __LINE__);
		HEreport("Swath: \"%s\" does not exist within HDF file.\n",
			 swathname);
	    }
	}
	else
	{
	    /* Too many files opened */
	    /* --------------------- */
	    swathID = -1;
	    strcpy(errbuf,
		   "No more than %d swaths may be open simutaneously");
	    strcat(errbuf, " (%s)");
	    HEpush(DFE_DENIED, "SWattach", __FILE__, __LINE__);
	    HEreport(errbuf, NSWATH, swathname);
	}

    }
    return (swathID);
}

/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWchkswid                                                        |
|                                                                             |
|  DESCRIPTION: Checks for valid swathID and returns file ID, SDS ID, and     |
|               swath Vgroup ID                                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|  routname       char                Name of routine calling SWchkswid       |
|                                                                             |
|  OUTPUTS:                                                                   |
|  fid            int32               File ID                                 |
|  sdInterfaceID  int32               SDS interface ID                        |
|  swVgrpID       int32               swath Vgroup ID                         |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
static intn
SWchkswid(int32 swathID, char *routname,
	  int32 * fid, int32 * sdInterfaceID, int32 * swVgrpID)

{
    intn            status = 0;	/* routine return status variable */
    uint8           access;	/* Read/Write access code */

    int32           idOffset = SWIDOFFSET;	/* Swath ID offset */

    char            message1[] =
    "Invalid swath id: %d in routine \"%s\".  ID must be >= %d and < %d.\n";
    char            message2[] =
    "Swath id %d in routine \"%s\" not active.\n";


    /* Check for valid swath id */
    /* ------------------------ */
    if (swathID < idOffset || swathID >= NSWATH + idOffset)
    {
	status = -1;
	HEpush(DFE_RANGE, "SWchkswid", __FILE__, __LINE__);
	HEreport(message1, swathID, routname, idOffset, NSWATH + idOffset);
    }
    else
    {
	/* Check for active swath ID */
	/* ------------------------- */
	if (SWXSwath[swathID % idOffset].active == 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "SWchkswid", __FILE__, __LINE__);
	    HEreport(message2, swathID, routname);
	}
	else
	{

	    /* Get file & SDS ids and Swath Vgroup */
	    /* ----------------------------------- */
	    status = EHchkfid(SWXSwath[swathID % idOffset].fid, " ", fid,
			      sdInterfaceID, &access);
	    *swVgrpID = SWXSwath[swathID % idOffset].IDTable;
	}
    }
    return (status);
}







/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWdefdim                                                         |
|                                                                             |
|  DESCRIPTION: Defines numerical value of dimension                          |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|  dimname        char                Dimension name to define                |
|  dim            int32               Dimemsion value                         |
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Dec 96   Joel Gales    Check that dim value >= 0                           |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
SWdefdim(int32 swathID, char *dimname, int32 dim)

{
    intn            status;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           swVgrpID;	/* Swath root Vgroup ID */
    int32           idOffset = SWIDOFFSET;	/* Swath ID offset */

    char            swathname[80] /* Swath name */ ;


    /* Check for valid swath id */
    /* ------------------------ */
    status = SWchkswid(swathID, "SWdefdim", &fid, &sdInterfaceID, &swVgrpID);


    /* Make sure dimension >= 0 */
    /* ------------------------ */
    if (dim < 0)
    {
	status = -1;
	HEpush(DFE_GENAPP, "SWdefdim", __FILE__, __LINE__);
	HEreport("Dimension value for \"%s\" less than zero: %d.\n",
		 dimname, dim);
    }


    /* Write Dimension to Structural MetaData */
    /* -------------------------------------- */
    if (status == 0)
    {
	Vgetname(SWXSwath[swathID % idOffset].IDTable, swathname);
	status = EHinsertmeta(sdInterfaceID, swathname, "s", 0L,
			      dimname, &dim);
    }
    return (status);
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWdiminfo                                                        |
|                                                                             |
|  DESCRIPTION: Returns size in bytes of named dimension                      |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  size           int32               Size of dimension                       |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure id                      |
|  dimname        char                Dimension name                          |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Aug 96   Joel Gales    Make metadata ODL compliant                         |
|  Jan 97   Joel Gales    Check for metadata error status from EHgetmetavalue |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
SWdiminfo(int32 swathID, char *dimname)

{
    intn            status;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           swVgrpID;	/* Swath root Vgroup ID */
    int32           size;	/* Dimension size */
    int32           idOffset = SWIDOFFSET;	/* Swath ID offset */


    char           *metabuf;	/* Pointer to structural metadata (SM) */
    char           *metaptrs[2];/* Pointers to begin and end of SM section */
    char            swathname[80];	/* Swath Name */
    char           *utlstr;	/* Utility string */


    /* Allocate space for utility string */
    /* --------------------------------- */
    utlstr = (char *) calloc(UTLSTR_MAX_SIZE, sizeof(char));
    if(utlstr == NULL)
    { 
	HEpush(DFE_NOSPACE,"SWdiminfo", __FILE__, __LINE__);
	return(-1);
    }
    /* Initialize return value */
    size = -1;

    /* Check Swath ID */
    status = SWchkswid(swathID, "SWdiminfo", &fid, &sdInterfaceID, &swVgrpID);

    if (status == 0)
    {
	/* Get swath name */
	Vgetname(SWXSwath[swathID % idOffset].IDTable, swathname);

	/* Get pointers to "Dimension" section within SM */
	metabuf = (char *) EHmetagroup(sdInterfaceID, swathname, "s",
				       "Dimension", metaptrs);

	if(metabuf == NULL)
	{
	    free(utlstr);
	    return(-1);
	}  

	/* Search for dimension name (surrounded by quotes) */
	sprintf(utlstr, "%s%s%s", "\"", dimname, "\"\n");
	metaptrs[0] = strstr(metaptrs[0], utlstr);

	/*
	 * If dimension found within swath structure then get dimension value
	 */
	if (metaptrs[0] < metaptrs[1] && metaptrs[0] != NULL)
	{
	    /* Set endptr at end of dimension definition entry */
	    metaptrs[1] = strstr(metaptrs[0], "\t\t\tEND_OBJECT");

	    status = EHgetmetavalue(metaptrs, "Size", utlstr);

	    if (status == 0)
	    {
		size = atol(utlstr);
	    }
	    else
	    {
		HEpush(DFE_GENAPP, "SWdiminfo", __FILE__, __LINE__);
		HEreport("\"Size\" string not found in metadata.\n");
	    }
	}
	else
	{
	    HEpush(DFE_GENAPP, "SWdiminfo", __FILE__, __LINE__);
	    HEreport("Dimension \"%s\" not found.\n", dimname);
	}

	free(metabuf);
    }
    free(utlstr);

    return (size);
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWmapinfo                                                        |
|                                                                             |
|  DESCRIPTION: Returns dimension mapping information                         |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure id                      |
|  geodim         char                geolocation dimension name              |
|  datadim        char                data dimension name                     |
|                                                                             |
|  OUTPUTS:                                                                   |
|  offset         int32               mapping offset                          |
|  increment      int32               mapping increment                       |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Aug 96   Joel Gales    Make metadata ODL compliant                         |
|  Jan 97   Joel Gales    Check for metadata error status from EHgetmetavalue |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
SWmapinfo(int32 swathID, char *geodim, char *datadim, int32 * offset,
	  int32 * increment)

{
    intn            status;	/* routine return status variable */
    intn            statmeta = 0;	/* EHgetmetavalue return status */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           swVgrpID;	/* Swath root Vgroup ID */
    int32           idOffset = SWIDOFFSET;	/* Swath ID offset */

    char           *metabuf;	/* Pointer to structural metadata (SM) */
    char           *metaptrs[2];/* Pointers to begin and end of SM section */
    char            swathname[80];	/* Swath Name */
    char           *utlstr;	/* Utility string */


    /* Allocate space for utility string */
    /* --------------------------------- */
    utlstr = (char *) calloc(UTLSTR_MAX_SIZE, sizeof(char));
    if(utlstr == NULL)
    { 
	HEpush(DFE_NOSPACE,"SWmapinfo", __FILE__, __LINE__);
	return(-1);
    }
    /* Initialize return values */
    *offset = -1;
    *increment = -1;

    /* Check Swath ID */
    status = SWchkswid(swathID, "SWmapinfo", &fid, &sdInterfaceID, &swVgrpID);

    if (status == 0)
    {
	/* Get swath name */
	Vgetname(SWXSwath[swathID % idOffset].IDTable, swathname);

	/* Get pointers to "DimensionMap" section within SM */
	metabuf = (char *) EHmetagroup(sdInterfaceID, swathname, "s",
				       "DimensionMap", metaptrs);
	if(metabuf == NULL)
	{
	    free(utlstr);
	    return(-1);
	}

	/* Search for mapping - GeoDim/DataDim (surrounded by quotes) */
	sprintf(utlstr, "%s%s%s%s%s", "\t\t\t\tGeoDimension=\"", geodim,
		"\"\n\t\t\t\tDataDimension=\"", datadim, "\"\n");
	metaptrs[0] = strstr(metaptrs[0], utlstr);

	/*
	 * If mapping found within swath structure then get offset and
	 * increment value
	 */
	if (metaptrs[0] < metaptrs[1] && metaptrs[0] != NULL)
	{
	    /* Get Offset */
	    statmeta = EHgetmetavalue(metaptrs, "Offset", utlstr);
	    if (statmeta == 0)
	    {
		*offset = atol(utlstr);
	    }
	    else
	    {
		status = -1;
		HEpush(DFE_GENAPP, "SWmapinfo", __FILE__, __LINE__);
		HEreport("\"Offset\" string not found in metadata.\n");
	    }


	    /* Get Increment */
	    statmeta = EHgetmetavalue(metaptrs, "Increment", utlstr);
	    if (statmeta == 0)
	    {
		*increment = atol(utlstr);
	    }
	    else
	    {
		status = -1;
		HEpush(DFE_GENAPP, "SWmapinfo", __FILE__, __LINE__);
		HEreport("\"Increment\" string not found in metadata.\n");
	    }
	}
	else
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "SWmapinfo", __FILE__, __LINE__);
	    HEreport("Mapping \"%s/%s\" not found.\n", geodim, datadim);
	}

	free(metabuf);
    }
    free(utlstr);
    return (status);
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWidxmapinfo                                                     |
|                                                                             |
|  DESCRIPTION: Returns indexed mapping information                           |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  gsize          int32               Number of index values (sz of geo dim)  |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure id                      |
|  geodim         char                geolocation dimension name              |
|  datadim        char                data dimension name                     |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|  index          int32               array of index values                   |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
SWidxmapinfo(int32 swathID, char *geodim, char *datadim, int32 index[])
{
    intn            status;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           swVgrpID;	/* Swath root Vgroup ID */
    int32           idOffset = SWIDOFFSET;	/* Swath ID offset */
    int32           vgid;	/* Swath Attributes Vgroup ID */
    int32           vdataID;	/* Index Mapping Vdata ID */
    int32           gsize = -1;	/* Size of geo dim */

    char            utlbuf[256];/* Utility buffer */


    /* Check Swath ID */
    status = SWchkswid(swathID, "SWidxmapinfo",
		       &fid, &sdInterfaceID, &swVgrpID);

    if (status == 0)
    {
	/* Find Index Mapping Vdata with Swath Attributes Vgroup */
	sprintf(utlbuf, "%s%s%s%s", "INDXMAP:", geodim, "/", datadim);
	vgid = SWXSwath[swathID % idOffset].VIDTable[2];
	vdataID = EHgetid(fid, vgid, utlbuf, 1, "r");

	/* If found then get geodim size & read index mapping values */
	if (vdataID != -1)
	{
	    gsize = SWdiminfo(swathID, geodim);

	    VSsetfields(vdataID, "Index");
	    VSread(vdataID, (uint8 *) index, 1, FULL_INTERLACE);
	    VSdetach(vdataID);
	}
	else
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "SWidxmapinfo", __FILE__, __LINE__);
	    HEreport("Index Mapping \"%s\" not found.\n", utlbuf);
	}
    }
    return (gsize);
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWcompinfo                                                       |
|                                                                             |
|  DESCRIPTION:                                                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                                                        |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32                                                       |
|  compcode       int32                                                       |
|  compparm       intn                                                        |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Oct 96   Joel Gales    Original Programmer                                 |
|  Jan 97   Joel Gales    Check for metadata error status from EHgetmetavalue |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
SWcompinfo(int32 swathID, char *fieldname, int32 * compcode, intn compparm[])
{
    intn            i;		/* Loop Index */
    intn            status;	/* routine return status variable */
    intn            statmeta = 0;	/* EHgetmetavalue return status */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           swVgrpID;	/* Swath root Vgroup ID */
    int32           idOffset = SWIDOFFSET;	/* Swath ID offset */

    char           *metabuf;	/* Pointer to structural metadata (SM) */
    char           *metaptrs[2];/* Pointers to begin and end of SM section */
    char            swathname[80];	/* Swath Name */
    char           *utlstr;     /* Utility string */

    char           *HDFcomp[5] = {"HDFE_COMP_NONE", "HDFE_COMP_RLE",
	"HDFE_COMP_NBIT", "HDFE_COMP_SKPHUFF",
    "HDFE_COMP_DEFLATE"};	/* Compression Codes */

    /* Allocate space for utility string */
    /* --------------------------------- */
    utlstr = (char *) calloc(UTLSTR_MAX_SIZE, sizeof(char));
    if(utlstr == NULL)
    { 
	HEpush(DFE_NOSPACE,"SWcompinfo", __FILE__, __LINE__);
	return(-1);
    }

    /* Check Swath ID */
    status = SWchkswid(swathID, "SWcompinfo",
		       &fid, &sdInterfaceID, &swVgrpID);

    if (status == 0)
    {
	/* Get swath name */
	Vgetname(SWXSwath[swathID % idOffset].IDTable, swathname);

	/* Get pointers to "DataField" section within SM */
	metabuf = (char *) EHmetagroup(sdInterfaceID, swathname, "s",
				       "DataField", metaptrs);
	if(metabuf == NULL)
	{
	    free(utlstr);
	    return(-1);
	}
	/* Search for field */
	sprintf(utlstr, "%s%s%s", "\"", fieldname, "\"\n");
	metaptrs[0] = strstr(metaptrs[0], utlstr);

	/* If not found then search in "GeoField" section */
	if (metaptrs[0] > metaptrs[1] || metaptrs[0] == NULL)
	{
	    free(metabuf);

	    /* Get pointers to "GeoField" section within SM */
	    metabuf = (char *) EHmetagroup(sdInterfaceID, swathname, "s",
					   "GeoField", metaptrs);
	    if(metabuf == NULL)
	    {
		free(utlstr);
		return(-1);
	    }
	    /* Search for field */
	    sprintf(utlstr, "%s%s%s", "\"", fieldname, "\"\n");
	    metaptrs[0] = strstr(metaptrs[0], utlstr);
	}


	/* If field found and user wants compression code ... */
	if (metaptrs[0] < metaptrs[1] && metaptrs[0] != NULL)
	{
	    if (compcode != NULL)
	    {
		/* Set endptr at end of field's definition entry */
		metaptrs[1] = strstr(metaptrs[0], "\t\t\tEND_OBJECT");

		/* Get compression type */
		statmeta = EHgetmetavalue(metaptrs, "CompressionType", utlstr);

		/*
		 * Default is no compression if "CompressionType" string not
		 * in metadata
		 */
		*compcode = HDFE_COMP_NONE;

		/* If compression code is found ... */
		if (statmeta == 0)
		{
		    /* Loop through compression types until match */
		    for (i = 0; i < 5; i++)
		    {
			if (strcmp(utlstr, HDFcomp[i]) == 0)
			{
			    *compcode = i;
			    break;
			}
		    }
		}
	    }

	    /* If user wants compression parameters ... */
	    if (compparm != NULL && compcode != NULL)
	    {
		/* Initialize to zero */
		for (i = 0; i < 4; i++)
		{
		    compparm[i] = 0.0;
		}

		/*
		 * Get compression parameters if NBIT or DEFLATE compression
		 */
		if (*compcode == HDFE_COMP_NBIT)
		{
		    statmeta =
			EHgetmetavalue(metaptrs, "CompressionParams", utlstr);
		    if (statmeta == 0)
		    {
			sscanf(utlstr, "(%d,%d,%d,%d)",
			       &compparm[0], &compparm[1],
			       &compparm[2], &compparm[3]);
		    }
		    else
		    {
			status = -1;
			HEpush(DFE_GENAPP, "SWcompinfo", __FILE__, __LINE__);
			HEreport(
				 "\"CompressionParams\" string not found in metadata.\n");
		    }
		}
		else if (*compcode == HDFE_COMP_DEFLATE)
		{
		    statmeta =
			EHgetmetavalue(metaptrs, "DeflateLevel", utlstr);
		    if (statmeta == 0)
		    {
			sscanf(utlstr, "%d", &compparm[0]);
		    }
		    else
		    {
			status = -1;
			HEpush(DFE_GENAPP, "SWcompinfo", __FILE__, __LINE__);
			HEreport(
			"\"DeflateLevel\" string not found in metadata.\n");
		    }
		}
	    }
	}
	else
	{
	    HEpush(DFE_GENAPP, "SWcompinfo", __FILE__, __LINE__);
	    HEreport("Fieldname \"%s\" not found.\n", fieldname);
	}

	free(metabuf);
    }
    free(utlstr);
    
    return (status);
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWfinfo                                                          |
|                                                                             |
|  DESCRIPTION: Returns field info                                            |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure id                      |
|  fieldtype      const char          fieldtype (geo or data)                 |
|  fieldname      const char          name of field                           |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|  rank           int32               rank of field (# of dims)               |
|  dims           int32               field dimensions                        |
|  numbertype     int32               field number type                       |
|  dimlist        char                field dimension list                    |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Aug 96   Joel Gales    Make metadata ODL compliant                         |
|  Jan 97   Joel Gales    Check for metadata error status from EHgetmetavalue |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
static int32
SWfinfo(int32 swathID, const char *fieldtype, const char *fieldname,
        int32 *rank, int32 dims[], int32 *numbertype, char *dimlist)

{
    intn            i;		/* Loop index */
    intn            j;		/* Loop index */
    intn            status;	/* routine return status variable */
    intn            statmeta = 0;	/* EHgetmetavalue return status */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           idOffset = SWIDOFFSET;	/* Swath ID offset */
    int32           fsize;	/* field size in bytes */
    int32           ndims = 0;	/* Number of dimensions */
    int32           slen[8];	/* Length of each entry in parsed string */
    int32           dum;	/* Dummy variable */
    int32           vdataID;	/* 1d field vdata ID */

    uint8          *buf;	/* One-Dim field buffer */

    char           *metabuf;	/* Pointer to structural metadata (SM) */
    char           *metaptrs[2];/* Pointers to begin and end of SM section */
    char            swathname[80];	/* Swath Name */
    char           *utlstr;	/* Utility string */
    char           *ptr[8];	/* String pointers for parsed string */
    char            dimstr[64];	/* Individual dimension entry string */


    /* Allocate space for utility string */
    /* --------------------------------- */
    utlstr = (char *) calloc(UTLSTR_MAX_SIZE, sizeof(char));
    if(utlstr == NULL)
    { 
	HEpush(DFE_NOSPACE,"SWfinfo", __FILE__, __LINE__);
	return(-1);
    }

    /* Initialize rank and numbertype to -1 (error) */
    /* -------------------------------------------- */
    *rank = -1;
    *numbertype = -1;

    /* Get HDF-EOS file ID and SDS interface ID */
    status = SWchkswid(swathID, "SWfinfo", &fid, &sdInterfaceID, &dum);

    /* Get swath name */
    Vgetname(SWXSwath[swathID % idOffset].IDTable, swathname);

    /* Get pointers to appropriate "Field" section within SM */
    if (strcmp(fieldtype, "Geolocation Fields") == 0)
    {
	metabuf = (char *) EHmetagroup(sdInterfaceID, swathname, "s",
				       "GeoField", metaptrs);
	
	if(metabuf == NULL)
	{
	    free(utlstr);
	    return(-1);
	}
    }
    else
    {
	metabuf = (char *) EHmetagroup(sdInterfaceID, swathname, "s",
				       "DataField", metaptrs);
	if(metabuf == NULL)
	{
	    free(utlstr);
	    return(-1);
	}
    }


    /* Search for field */
    sprintf(utlstr, "%s%s%s", "\"", fieldname, "\"\n");
    metaptrs[0] = strstr(metaptrs[0], utlstr);

    /* If field found ... */
    if (metaptrs[0] < metaptrs[1] && metaptrs[0] != NULL)
    {
	/* Get DataType string */
	statmeta = EHgetmetavalue(metaptrs, "DataType", utlstr);

	/* Convert to numbertype code */
	if (statmeta == 0)
	    *numbertype = EHnumstr(utlstr);
	else
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "SWfinfo", __FILE__, __LINE__);
	    HEreport("\"DataType\" string not found in metadata.\n");
	}


	/*
	 * Get DimList string and trim off leading and trailing parens "()"
	 */
	statmeta = EHgetmetavalue(metaptrs, "DimList", utlstr);

	if (statmeta == 0)
	{
	    memmove(utlstr, utlstr + 1, strlen(utlstr) - 2);
	    utlstr[strlen(utlstr) - 2] = 0;

	    /* Parse trimmed DimList string and get rank */
	    ndims = EHparsestr(utlstr, ',', ptr, slen);
	    *rank = ndims;
	}
	else
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "SWfinfo", __FILE__, __LINE__);
	    HEreport("\"DimList\" string not found in metadata.\n");
	}

	/* If dimension list is desired by user then initialize length to 0 */
	if (dimlist != NULL)
	{
	    dimlist[0] = 0;
	}

	/*
	 * Copy each entry in DimList and remove leading and trailing quotes,
	 * Get dimension sizes and concatanate dimension names to dimension
	 * list
	 */
	for (i = 0; i < ndims; i++)
	{
	    memcpy(dimstr, ptr[i] + 1, slen[i] - 2);
	    dimstr[slen[i] - 2] = 0;
	    dims[i] = SWdiminfo(swathID, dimstr);
	    if (dimlist != NULL)
	    {
		if (i > 0)
		{
		    strcat(dimlist, ",");
		}
		strcat(dimlist, dimstr);
	    }

	}


	/* Appendable Field Section */
	/* ------------------------ */
	if (dims[0] == 0)
	{
	    /* One-Dimensional Field */
	    if (*rank == 1)
	    {
		/* Get vdata ID */
		status = SW1dfldsrch(fid, swathID, fieldname, "r",
				     &dum, &vdataID, &dum);

		/* Get actual size of field */
		dims[0] = VSelts(vdataID);

		/*
		 * If size=1 then check where actual record of
		 * "initialization" record
		 */
		if (dims[0] == 1)
		{
		    /* Get record size and read 1st record */
		    fsize = VSsizeof(vdataID, (char *)fieldname);
		    buf = (uint8 *) calloc(fsize, 1);
		    if(buf == NULL)
		    { 
			HEpush(DFE_NOSPACE,"SWfinfo", __FILE__, __LINE__);
			free(utlstr);
			return(-1);
		    }
		    VSsetfields(vdataID, fieldname);
		    VSseek(vdataID, 0);
		    VSread(vdataID, (uint8 *) buf, 1, FULL_INTERLACE);

		    /* Sum up "bytes" in record */
		    for (i = 0, j = 0; i < fsize; i++)
		    {
			j += buf[i];
		    }

		    /*
		     * If filled with 255 then "initialization" record,
		     * actual number of records = 0
		     */
		    if (j == 255 * fsize)
		    {
			dims[0] = 0;
		    }

		    free(buf);
		}
		/* Detach from 1d field */
		VSdetach(vdataID);
	    }
	    else
	    {
		/* Get actual size of Multi-Dimensional Field */
		status = SWSDfldsrch(swathID, sdInterfaceID, fieldname,
				     &dum, &dum, &dum, &dum, dims,
				     &dum);
	    }
	}
    }
    free(metabuf);

    if (*rank == -1)
    {
	status = -1;
    }
    free(utlstr);
     
    return (status);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWfieldinfo                                                      |
|                                                                             |
|  DESCRIPTION: Wrapper arount SWfinfo                                        |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure id                      |
|  fieldname      const char          name of field                           |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|  rank           int32               rank of field (# of dims)               |
|  dims           int32               field dimensions                        |
|  numbertype     int32               field number type                       |
|  dimlist        char                field dimension list                    |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
SWfieldinfo(int32 swathID, const char *fieldname, int32 * rank, int32 dims[],
	    int32 * numbertype, char *dimlist)

{
    intn            status;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           swVgrpID;	/* Swath root Vgroup ID */


    /* Check for valid swath id */
    status = SWchkswid(swathID, "SWfieldinfo", &fid,
		       &sdInterfaceID, &swVgrpID);
    if (status == 0)
    {
	/* Check for field within Geolocatation Fields */
	status = SWfinfo(swathID, "Geolocation Fields", fieldname,
			 rank, dims, numbertype, dimlist);

	/* If not there then check within Data Fields */
	if (status == -1)
	{
	    status = SWfinfo(swathID, "Data Fields", fieldname,
			     rank, dims, numbertype, dimlist);
	}

	/* If not there either then can't be found */
	if (status == -1)
	{
	    HEpush(DFE_GENAPP, "SWfieldinfo", __FILE__, __LINE__);
	    HEreport("Fieldname \"%s\" not found.\n", fieldname);
	}
    }
    return (status);
}






/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWdefdimmap                                                      |
|                                                                             |
|  DESCRIPTION: Defines mapping between geolocation and data dimensions       |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|  geodim         char                Geolocation dimension                   |
|  datadim        char                Data dimension                          |
|  offset         int32               Mapping offset                          |
|  increment      int32               Mapping increment                       |
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
SWdefdimmap(int32 swathID, char *geodim, char *datadim, int32 offset,
	    int32 increment)

{
    intn            status;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           size;	/* Size of geo dim */
    int32           idOffset = SWIDOFFSET;	/* Swath ID offset */
    int32           dum;	/* Dummy variable */
    int32           metadata[2];/* Offset & Increment (passed to metadata) */

    char            mapname[80];/* Mapping name (geodim/datadim) */
    char            swathname[80];	/* Swath name */

    /* Check Swath ID */
    status = SWchkswid(swathID, "SWdefdimmap", &fid, &sdInterfaceID, &dum);

    if (status == 0)
    {

	/* Search Dimension Vdata for dimension entries */
	/* -------------------------------------------- */
	size = SWdiminfo(swathID, geodim);
	if (size == -1)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "SWdefdimmap", __FILE__, __LINE__);
	    HEreport("Geolocation dimension name: \"%s\" not found.\n",
		     geodim);
	}
	/* Data Dimension Search */
	/* --------------------- */
	if (status == 0)
	{
	    size = SWdiminfo(swathID, datadim);
	    if (size == -1)
	    {
		status = -1;
		HEpush(DFE_GENAPP, "SWdefdimmap", __FILE__, __LINE__);
		HEreport("Data dimension name: \"%s\" not found.\n",
			 datadim);
	    }
	}

	/* Write Dimension Map to Structural MetaData */
	/* ------------------------------------------ */
	if (status == 0)
	{
	    sprintf(mapname, "%s%s%s", geodim, "/", datadim);
	    metadata[0] = offset;
	    metadata[1] = increment;

	    Vgetname(SWXSwath[swathID % idOffset].IDTable, swathname);
	    status = EHinsertmeta(sdInterfaceID, swathname, "s", 1L,
				  mapname, metadata);

	}
    }
    return (status);
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWdefidxmap                                                      |
|                                                                             |
|  DESCRIPTION: Defines indexed (non-linear) mapping between geolocation      |
|               and data dimensions                                           |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|  geodim         char                Geolocation dimension                   |
|  datadim        char                Data dimension                          |
|  index          int32               Index mapping array                     |
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
SWdefidxmap(int32 swathID, char *geodim, char *datadim, int32 index[])

{
    intn            status;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           attrVgrpID;	/* Swath attribute ID */
    int32           vdataID;	/* Mapping Index Vdata ID */
    int32           gsize;	/* Size of geo dim */
    int32           dsize;	/* Size of data dim */
    int32           idOffset = SWIDOFFSET;	/* Swath ID offset */
    int32           dum;	/* Dummy variable */

    uint8          *buf;	/* Vdata field buffer */

    char            mapname[80];/* Mapping name (geodim/datadim) */
    char            swathname[80];	/* Swath name */
    char            utlbuf[256];/* Utility buffer */


    /* Check Swath ID */
    status = SWchkswid(swathID, "SWdefidxmap", &fid, &sdInterfaceID, &dum);
    if (status == 0)
    {
	/* Search Dimension Vdata for dimension entries */
	/* -------------------------------------------- */

	/* Geo Dimension Search */
	/* -------------------- */
       
	gsize = SWdiminfo(swathID, geodim);

	if (gsize == -1)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "SWdefidxmap", __FILE__, __LINE__);
	    HEreport("Geolocation dimension name: \"%s\" not found.\n",
		     geodim);
	}
	/* Data Dimension Search */
	/* --------------------- */
	if (status == 0)
	{
	    dsize = SWdiminfo(swathID, datadim);
	    if (dsize == -1)
	    {
		status = -1;
		HEpush(DFE_GENAPP, "SWdefidxmap", __FILE__, __LINE__);
		HEreport("Data dimension name: \"%s\" not found.\n",
			 datadim);
	    }
	}
	/* Define Index Vdata and Store Index Array */
	/* ---------------------------------------- */
	if (status == 0)
	{
	    /* Get attribute Vgroup ID and allocate data buffer */
	    /* ------------------------------------------------ */
	    attrVgrpID = SWXSwath[swathID % idOffset].VIDTable[2];
	    buf = (uint8 *) calloc(4 * gsize, 1);
	    if(buf == NULL)
	    { 
		HEpush(DFE_NOSPACE,"SWdefidxmap", __FILE__, __LINE__);
		return(-1);
	    }

	    /* Name: "INDXMAP:" + geodim + "/" + datadim */
	    sprintf(utlbuf, "%s%s%s%s", "INDXMAP:", geodim, "/", datadim);

	    vdataID = VSattach(fid, -1, "w");
	    VSsetname(vdataID, utlbuf);

	    /* Attribute Class */
	    VSsetclass(vdataID, "Attr0.0");

	    /* Fieldname is "Index" */
	    VSfdefine(vdataID, "Index", DFNT_INT32, gsize);
	    VSsetfields(vdataID, "Index");
	    memcpy(buf, index, 4 * gsize);

	    /* Write to vdata and free data buffer */
	    VSwrite(vdataID, buf, 1, FULL_INTERLACE);
	    free(buf);

	    /* Insert in Attribute Vgroup and detach Vdata */
	    Vinsert(attrVgrpID, vdataID);
	    VSdetach(vdataID);


	    /* Write to Structural Metadata */
	    sprintf(mapname, "%s%s%s", geodim, "/", datadim);
	    Vgetname(SWXSwath[swathID % idOffset].IDTable, swathname);
	    status = EHinsertmeta(sdInterfaceID, swathname, "s", 2L,
				  mapname, &dum);

	}
    }
    return (status);

}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWdefcomp                                                        |
|                                                                             |
|  DESCRIPTION: Defines compression type and parameters                       |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|  compcode       int32               compression code                        |
|  compparm       intn                compression parameters                  |
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Sep 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
SWdefcomp(int32 swathID, int32 compcode, intn compparm[])
{
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           swVgrpID;	/* Swath root Vgroup ID */
    int32           idOffset = SWIDOFFSET;	/* Swath ID offset */
    int32           sID;	/* swathID - offset */


    /* Check for valid swath id */
    status = SWchkswid(swathID, "SWdefcomp", &fid, &sdInterfaceID, &swVgrpID);

    if (status == 0)
    {
	sID = swathID % idOffset;

	/* Set compression code in compression exteral array */
	SWXSwath[sID].compcode = compcode;

	switch (compcode)
	{
	    /* Set NBIT compression parameters in compression external array */
	case HDFE_COMP_NBIT:

	    SWXSwath[sID].compparm[0] = compparm[0];
	    SWXSwath[sID].compparm[1] = compparm[1];
	    SWXSwath[sID].compparm[2] = compparm[2];
	    SWXSwath[sID].compparm[3] = compparm[3];

	    break;

	    /* Set GZIP compression parameter in compression external array */
	case HDFE_COMP_DEFLATE:

	    SWXSwath[sID].compparm[0] = compparm[0];

	    break;

	}
    }

    return (status);
}

/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWdefinefield                                                    |
|                                                                             |
|  DESCRIPTION: Defines geolocation or data field within swath structure      |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|  fieldtype      char                geo/data fieldtype                      |
|  fieldname      char                fieldname                               |
|  dimlist        char                Dimension list (comma-separated list)   |
|  numbertype     int32               field type                              |
|  merge          int32               merge code                              |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Aug 96   Joel Gales    Check name for length                               |
|  Sep 96   Joel Gales    Make string array "dimbuf" dynamic                  |
|  Oct 96   Joel Gales    Make sure total length of "merged" Vdata < 64       |
|  Jun 03   Abe Taaheri   Supplied cast comp_coder_t in call to SDsetcompress |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
static intn
SWdefinefield(int32 swathID, char *fieldtype, char *fieldname, char *dimlist,
	      int32 numbertype, int32 merge)

{
    intn            i;		/* Loop index */
    intn            status;	/* routine return status variable */
    intn            found;	/* utility found flag */
    intn            foundNT = 0;/* found number type flag */
    intn            foundAllDim = 1;	/* found all dimensions flag */
    intn            first = 1;	/* first entry flag */
    intn            fac;	/* Geo (-1), Data (+1) field factor */
    int32           cnt = 0;

    int32           fid;	/* HDF-EOS file ID */
    int32           vdataID;	/* Vdata ID */
    int32           vgid;	/* Geo/Data field Vgroup ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           sdid;	/* SDS object ID */
    int32           dimid;	/* SDS dimension ID */
    int32           recSize;	/* Vdata record size */
    int32           swVgrpID;	/* Swath root Vgroup ID */
    int32           dims[8];	/* Dimension size array */
    int32           dimsize;	/* Dimension size */
    int32           rank = 0;	/* Field rank */
    int32           slen[32];	/* String length array */
    int32           idOffset = SWIDOFFSET;	/* Swath ID offset */
    int32           compcode;	/* Compression code */
    int32           sID;	/* SwathID - offset */

    uint8          *oneDbuf;	/* Vdata record buffer */
    char           *dimbuf;	/* Dimension buffer */
    char           *comma;	/* Pointer to comma */
    char           *dimcheck;	/* Dimension check buffer */
    char            utlbuf[512];/* Utility buffer */
    char            utlbuf2[256];	/* Utility buffer 2 */
    char           *ptr[32];	/* String pointer array */
    char            swathname[80];	/* Swath name */
    char            errbuf1[128];	/* Error message buffer 1 */
    char            errbuf2[128];	/* Error message buffer 2 */
    char            compparmbuf[128];	/* Compression parmeter string buffer */

    char           *HDFcomp[5] = {"HDFE_COMP_NONE", "HDFE_COMP_RLE",
	"HDFE_COMP_NBIT", "HDFE_COMP_SKPHUFF",
    "HDFE_COMP_DEFLATE"};
    /* Compression code names */

    uint16          good_number[10] = {3, 4, 5, 6, 20, 21, 22, 23, 24, 25};
    /* Valid number types */
    comp_info       c_info;	/* Compression parameter structure */



    /* Setup error message strings */
    /* --------------------------- */
    strcpy(errbuf1, "SWXSDname array too small.\nPlease increase ");
    strcat(errbuf1, "size of HDFE_NAMBUFSIZE in \"HdfEosDef.h\".\n");
    strcpy(errbuf2, "SWXSDdims array too small.\nPlease increase ");
    strcat(errbuf2, "size of HDFE_DIMBUFSIZE in \"HdfEosDef.h\".\n");



    /*
     * Check for proper swath ID and return HDF-EOS file ID, SDinterface ID,
     * and swath root Vgroup ID
     */
    status = SWchkswid(swathID, "SWdefinefield",
		       &fid, &sdInterfaceID, &swVgrpID);


    if (status == 0)
    {
	/* Remove offset from swath ID & get swathname */
	sID = swathID % idOffset;
	Vgetname(swVgrpID, swathname);

	/* Allocate space for dimbuf, copy dimlist into it, & append comma */
	dimbuf = (char *) calloc(strlen(dimlist) + 64, 1);
	if(dimbuf == NULL)
	{ 
	    HEpush(DFE_NOSPACE,"SWdefinefield", __FILE__, __LINE__);
	    return(-1);
	}
	strcpy(dimbuf, dimlist);
	strcat(dimbuf, ",");

	/* Find comma */
	comma = strchr(dimbuf, ',');


	/*
	 * Loop through entries in dimension list to make sure they are
	 * defined in swath
	 */
	while (comma != NULL)
	{
	    /* Copy dimension list entry to dimcheck */
	    dimcheck = (char *) calloc(comma - dimbuf + 1, 1);
	    if(dimcheck == NULL)
	    { 
		HEpush(DFE_NOSPACE,"SWdefinefield", __FILE__, __LINE__);
		free(dimbuf);
		return(-1);
	    }
	    memcpy(dimcheck, dimbuf, comma - dimbuf);

	    /* Get dimension size */
	    dimsize = SWdiminfo(swathID, dimcheck);

	    /* if != -1 then sent found flag, store size and increment rank */
	    if (dimsize != -1)
	    {
		dims[rank] = dimsize;
		rank++;
	    }
	    else
	    {
		/*
		 * If dimension list entry not found - set error return
		 * status, append name to utility buffer for error report
		 */
		status = -1;
		foundAllDim = 0;
		if (first == 1)
		{
		    strcpy(utlbuf, dimcheck);
		}
		else
		{
		    strcat(utlbuf, ",");
		    strcat(utlbuf, dimcheck);
		}
		first = 0;
	    }

	    /*
	     * Go to next dimension entry, find next comma, & free up
	     * dimcheck buffer
	     */
            *comma = '\0';  /* zero out first comma  */
            comma++;
            comma = strchr(comma, ',');
            if (comma != NULL)
            {
               for (i=0; i<strlen(dimcheck) + 1; i++)
               {
                  dimbuf++;
                  cnt++;
               }
            }
	    free(dimcheck);
	}
        for(i=0; i<cnt; i++)
           dimbuf--;

	free(dimbuf);


	/* Check that UNLIMITED dimension is first dimension if present */
	/* ------------------------------------------------------------ */
	if (status == 0)
	{
	    for (i = 0; i < rank; i++)
	    {
		if (dims[i] == 0 && i != 0)
		{
		    status = -1;
		    HEpush(DFE_GENAPP, "SWdefinefield", __FILE__, __LINE__);
		    HEreport("UNLIMITED dimension must be first dimension.\n");
		}
	    }
	}


	/* Check fieldname length */
	/* ---------------------- */
	if (status == 0)
	{
/* ((intn) strlen(fieldname) > MAX_NC_NAME - 7)
** this was changed because HDF4.1r3 made a change in the
** hlimits.h file.  We have notidfied NCSA and asked to have 
** it made the same as in previous versions of HDF
** see ncr 26314.  DaW  Apr 2000
*/

	    if (((intn) strlen(fieldname) > VSNAMELENMAX && rank == 1) ||
		((intn) strlen(fieldname) > (256 - 7) && rank > 1))
	    {
		status = -1;
		HEpush(DFE_GENAPP, "SWdefinefield", __FILE__, __LINE__);
		HEreport("Fieldname \"%s\" too long.\n", fieldname);
	    }
	}




	/* Check for valid numbertype */
	/* -------------------------- */
	if (status == 0)
	{
	    for (i = 0; i < 10; i++)
	    {
		if (numbertype == good_number[i])
		{
		    foundNT = 1;
		}
	    }

	    if (foundNT == 0)
	    {
		HEpush(DFE_BADNUMTYPE, "SWdefinefield", __FILE__, __LINE__);
		HEreport("Invalid number type: %d (%s).\n",
			 numbertype, fieldname);
		status = -1;
	    }
	}


	/* Define Field */
	/* ------------ */
	if (status == 0)
	{
	    /* Set factor & get Field Vgroup id */
	    /* -------------------------------- */
	    if (strcmp(fieldtype, "Geolocation Fields") == 0)
	    {
		fac = -1;
		vgid = SWXSwath[sID].VIDTable[0];
	    }
	    else
	    {
		fac = +1;
		vgid = SWXSwath[sID].VIDTable[1];
	    }
	    /*
	     * Note: "fac" is used to destinguish geo fields from data fields
	     * so that they are not merged together
	     */


	    /* One D Fields */
	    /* ------------ */
	    if (rank == 1)
	    {
		/* No Compression for 1D (Vdata) fields */
		compcode = HDFE_COMP_NONE;


		/* If field non-appendable and merge set to AUTOMERGE ... */
		if (dims[0] != 0 && merge == HDFE_AUTOMERGE)
		{
		    i = 0;
		    found = 0;

		    /* Loop through previous entries in 1d combination array */
		    while (SWX1dcomb[3 * i] != 0)
		    {
			/* Get name of previous 1d combined field */
			vdataID = SWX1dcomb[3 * i + 2];
			VSgetname(vdataID, utlbuf);

			/*
			 * If dimension, field type (geo/data), and swath
			 * structure if current entry match a previous entry
			 * and combined name is less than max allowed then
			 * set "found" flag and exit loop
			 */
			if (SWX1dcomb[3 * i] == fac * dims[0] &&
			    SWX1dcomb[3 * i + 1] == swVgrpID &&
			    (intn) strlen(utlbuf) +
			    (intn) strlen(fieldname) + 1 <=
			    VSNAMELENMAX)
			{
			    found = 1;
			    break;
			}
			/* Increment loop index */
			i++;
		    }


		    if (found == 0)
		    {
			/*
			 * If no matching entry found then start new Vdata
			 * and store dimension size, swath root Vgroup ID,
			 * field Vdata and fieldname in external array
			 * "SWX1dcomb"
			 */
			vdataID = VSattach(fid, -1, "w");
			SWX1dcomb[3 * i] = fac * dims[0];
			SWX1dcomb[3 * i + 1] = swVgrpID;
			SWX1dcomb[3 * i + 2] = vdataID;
			VSsetname(vdataID, fieldname);
		    }
		    else
		    {
			/*
			 * If match then concatanate current fieldname to
			 * previous matching fieldnames
			 */
			strcat(utlbuf, ",");
			strcat(utlbuf, fieldname);
			VSsetname(vdataID, utlbuf);
		    }

		    /* Define field as field within Vdata */
		    VSfdefine(vdataID, fieldname, numbertype, 1);
		    Vinsert(vgid, vdataID);

		}
		else
		{
		    /* 1d No Merge Section */

		    /* Get new vdata ID and establish field within Vdata */
		    vdataID = VSattach(fid, -1, "w");
		    VSsetname(vdataID, fieldname);
		    VSfdefine(vdataID, fieldname, numbertype, 1);
		    VSsetfields(vdataID, fieldname);

		    recSize = VSsizeof(vdataID, fieldname);
		    if (dims[0] == 0)
		    {
			/*
			 * If appendable field then write single record
			 * filled with 255
			 */
			oneDbuf = (uint8 *) calloc(recSize, 1);
			if(oneDbuf == NULL)
			{ 
			    HEpush(DFE_NOSPACE,"SWdefinefield", __FILE__, __LINE__);
			    return(-1);
			}
			for (i = 0; i < recSize; i++)
			    oneDbuf[i] = 255;
			VSwrite(vdataID, oneDbuf, 1, FULL_INTERLACE);
		    }
		    else
		    {
			/*
			 * If non-appendable then write entire field with
			 * blank records
			 */
			oneDbuf = (uint8 *) calloc(recSize, dims[0]);
			if(oneDbuf == NULL)
			{ 
			    HEpush(DFE_NOSPACE,"SWdefinefield", __FILE__, __LINE__);
			    return(-1);
			}
			VSwrite(vdataID, oneDbuf, dims[0], FULL_INTERLACE);
		    }
		    free(oneDbuf);

		    /* Insert Vdata into field Vgroup & detach */
		    Vinsert(vgid, vdataID);
		    VSdetach(vdataID);

		}		/* End No Merge Section */

	    }			/* End 1d field Section */
	    else
	    {
		/* SDS Interface (Multi-dim fields) */
		/* -------------------------------- */

		/* Get current compression code */
		compcode = SWXSwath[sID].compcode;

		/*
		 * If rank is less than or equal to 3 (and greater than 1)
		 * and AUTOMERGE is set and the first dimension is not
		 * appendable and the compression code is set to none then
		 * ...
		 */
		if (rank <= 3 && merge == HDFE_AUTOMERGE && dims[0] != 0
		    && compcode == HDFE_COMP_NONE)
		{
		    /* Find first empty slot in external combination array */
		    /* --------------------------------------------------- */
		    i = 0;
		    while (SWXSDcomb[5 * i] != 0)
		    {
			i++;
		    }

		    /*
		     * Store dimensions (with geo/data factor), swath root
		     * Vgroup ID, and number type in external combination
		     * array "SWXSDcomb"
		     */

		    if (rank == 2)
		    {
			/* If 2-dim field then set lowest dimension to +/- 1 */
			SWXSDcomb[5 * i] = fac;
			SWXSDcomb[5 * i + 1] = fac * dims[0];
			SWXSDcomb[5 * i + 2] = fac * dims[1];
		    }
		    else
		    {
			SWXSDcomb[5 * i] = fac * dims[0];
			SWXSDcomb[5 * i + 1] = fac * dims[1];
			SWXSDcomb[5 * i + 2] = fac * dims[2];
		    }

		    SWXSDcomb[5 * i + 3] = swVgrpID;
		    SWXSDcomb[5 * i + 4] = numbertype;


		    /* Concatanate fieldname with combined name string */
		    /* ----------------------------------------------- */
		    if ((intn) strlen(SWXSDname) +
			(intn) strlen(fieldname) + 2 < HDFE_NAMBUFSIZE)
		    {
			strcat(SWXSDname, fieldname);
			strcat(SWXSDname, ",");
		    }
		    else
		    {
			/* SWXSDname array too small! */
			/* -------------------------- */
			HEpush(DFE_GENAPP, "SWdefinefield",
			       __FILE__, __LINE__);
			HEreport(errbuf1);
			status = -1;
			return (status);
		    }



		    /*
		     * If 2-dim field then set lowest dimension (in 3-dim
		     * array) to "ONE"
		     */
		    if (rank == 2)
		    {
			if ((intn) strlen(SWXSDdims) + 5 < HDFE_DIMBUFSIZE)
			{
			    strcat(SWXSDdims, "ONE,");
			}
			else
			{
			    /* SWXSDdims array too small! */
			    /* -------------------------- */
			    HEpush(DFE_GENAPP, "SWdefinefield",
				   __FILE__, __LINE__);
			    HEreport(errbuf2);
			    status = -1;
			    return (status);
			}

		    }

		    /*
		     * Concatanate field dimlist to merged dimlist and
		     * separate fields with semi-colon
		     */
		    if ((intn) strlen(SWXSDdims) +
			(intn) strlen(dimlist) + 2 < HDFE_DIMBUFSIZE)
		    {
			strcat(SWXSDdims, dimlist);
			strcat(SWXSDdims, ";");
		    }
		    else
		    {
			/* SWXSDdims array too small! */
			/* -------------------------- */
			HEpush(DFE_GENAPP, "SWdefinefield",
			       __FILE__, __LINE__);
			HEreport(errbuf2);
			status = -1;
			return (status);
		    }

		}		/* End Multi-Dim Merge Section */
		else
		{
		    /* Multi-Dim No Merge Section */
		    /* ========================== */

		    /* Create SDS dataset */
		    /* ------------------ */
		    sdid = SDcreate(sdInterfaceID, fieldname,
				    numbertype, rank, dims);


		    /* Store Dimension Names in SDS */
		    /* ---------------------------- */
		    rank = EHparsestr(dimlist, ',', ptr, slen);
		    for (i = 0; i < rank; i++)
		    {
			/* Dimension name = Swathname:Dimname */
			memcpy(utlbuf, ptr[i], slen[i]);
			utlbuf[slen[i]] = 0;
			strcat(utlbuf, ":");
			strcat(utlbuf, swathname);

			dimid = SDgetdimid(sdid, i);
			SDsetdimname(dimid, utlbuf);
		    }


		    /* Setup compression parameters */
		    if (compcode == HDFE_COMP_NBIT)
		    {
			c_info.nbit.nt = numbertype;
			c_info.nbit.sign_ext = SWXSwath[sID].compparm[0];
			c_info.nbit.fill_one = SWXSwath[sID].compparm[1];
			c_info.nbit.start_bit = SWXSwath[sID].compparm[2];
			c_info.nbit.bit_len = SWXSwath[sID].compparm[3];
		    }
		    else if (compcode == HDFE_COMP_SKPHUFF)
		    {
			c_info.skphuff.skp_size = (intn) DFKNTsize(numbertype);
		    }
		    else if (compcode == HDFE_COMP_DEFLATE)
		    {
			c_info.deflate.level = SWXSwath[sID].compparm[0];
		    }

		    /* If field is compressed then call SDsetcompress */
		    /* ---------------------------------------------- */
		    if (compcode != HDFE_COMP_NONE)
		    {
			status = SDsetcompress(sdid, (comp_coder_t) compcode, &c_info);
		    }


		    /* Attach to Vgroup */
		    /* ---------------- */
		    Vaddtagref(vgid, DFTAG_NDG, SDidtoref(sdid));


		    /* Store SDS dataset IDs */
		    /* --------------------- */
		    if (SWXSwath[sID].nSDS > 0)
		    {
			SWXSwath[sID].sdsID = (int32 *)
			    realloc((void *) SWXSwath[sID].sdsID,
				    (SWXSwath[sID].nSDS + 1) * 4);
			if(SWXSwath[sID].sdsID == NULL)
			{ 
			    HEpush(DFE_NOSPACE,"SWdefinefield", __FILE__, __LINE__);
			    return(-1);
			}

		    }
		    else
		    {
			SWXSwath[sID].sdsID = (int32 *) calloc(1, 4);
			if(SWXSwath[sID].sdsID == NULL)
			{ 
			    HEpush(DFE_NOSPACE,"SWdefinefield", __FILE__, __LINE__);
			    return(-1);
			}
		    }
		    SWXSwath[sID].sdsID[SWXSwath[sID].nSDS] = sdid;
		    SWXSwath[sID].nSDS++;

		}		/* End Multi-Dim No Merge Section */

	    }			/* End Multi-Dim Section */



	    /* Setup metadata string */
	    /* --------------------- */
	    sprintf(utlbuf, "%s%s%s", fieldname, ":", dimlist);


	    /* Setup compression metadata */
	    /* -------------------------- */
	    if (compcode != HDFE_COMP_NONE)
	    {
		sprintf(utlbuf2,
			"%s%s",
			":\n\t\t\t\tCompressionType=", HDFcomp[compcode]);

		switch (compcode)
		{
		case HDFE_COMP_NBIT:

		    sprintf(compparmbuf,
			    "%s%d,%d,%d,%d%s",
			    "\n\t\t\t\tCompressionParams=(",
			    SWXSwath[sID].compparm[0],
			    SWXSwath[sID].compparm[1],
			    SWXSwath[sID].compparm[2],
			    SWXSwath[sID].compparm[3], ")");
		    strcat(utlbuf2, compparmbuf);
		    break;


		case HDFE_COMP_DEFLATE:

		    sprintf(compparmbuf,
			    "%s%d",
			    "\n\t\t\t\tDeflateLevel=",
			    SWXSwath[sID].compparm[0]);
		    strcat(utlbuf2, compparmbuf);
		    break;
		}

		/* Concatanate compression parameters with compression code */
		strcat(utlbuf, utlbuf2);
	    }


	    /* Insert field metadata within File Structural Metadata */
	    /* ----------------------------------------------------- */
	    if (strcmp(fieldtype, "Geolocation Fields") == 0)
	    {
		status = EHinsertmeta(sdInterfaceID, swathname, "s", 3L,
				      utlbuf, &numbertype);
	    }
	    else
	    {
		status = EHinsertmeta(sdInterfaceID, swathname, "s", 4L,
				      utlbuf, &numbertype);
	    }

	}
    }

    /* If all dimensions not found then report error */
    /* --------------------------------------------- */
    if (foundAllDim == 0)
    {
	HEpush(DFE_GENAPP, "SWdefinefield", __FILE__, __LINE__);
	HEreport("Dimension(s): \"%s\" not found (%s).\n",
		 utlbuf, fieldname);
	status = -1;
    }

    return (status);
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWdefgeofield                                                    |
|                                                                             |
|  DESCRIPTION: Defines geolocation field within swath structure (wrapper)    |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|  fieldname      char                fieldname                               |
|  dimlist        char                Dimension list (comma-separated list)   |
|  numbertype     int32               field type                              |
|  merge          int32               merge code                              |
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
SWdefgeofield(int32 swathID, char *fieldname, char *dimlist,
	      int32 numbertype, int32 merge)
{
    intn            status;	/* routine return status variable */

    /* Call SWdefinefield routine */
    /* -------------------------- */
    status = SWdefinefield(swathID, "Geolocation Fields", fieldname, dimlist,
			   numbertype, merge);

    return (status);
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWdefdatafield                                                   |
|                                                                             |
|  DESCRIPTION: Defines data field within swath structure (wrapper)           |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|  fieldname      char                fieldname                               |
|  dimlist        char                Dimension list (comma-separated list)   |
|  numbertype     int32               field type                              |
|  merge          int32               merge code                              |
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
SWdefdatafield(int32 swathID, char *fieldname, char *dimlist,
	       int32 numbertype, int32 merge)
{
    intn            status;	/* routine return status variable */

    /* Call SWdefinefield routine */
    /* -------------------------- */
    status = SWdefinefield(swathID, "Data Fields", fieldname, dimlist,
			   numbertype, merge);

    return (status);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWwritegeometa                                                   |
|                                                                             |
|  DESCRIPTION: Defines structural metadata for pre-existing geolocation      |
|               field within swath structure                                  |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|  fieldname      char                fieldname                               |
|  dimlist        char                Dimension list (comma-separated list)   |
|  numbertype     int32               field type                              |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
SWwritegeometa(int32 swathID, char *fieldname, char *dimlist,
	       int32 numbertype)
{
    intn            status = 0;	/* routine return status variable */

    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           dum;	/* dummy variable */
    int32           idOffset = SWIDOFFSET;	/* Swath ID offset */

    char            utlbuf[256];/* Utility buffer */
    char            swathname[80];	/* Swath name */

    status = SWchkswid(swathID, "SWwritegeometa", &dum, &sdInterfaceID,
		       &dum);

    if (status == 0)
    {
	/* Setup and write field metadata */
	/* ------------------------------ */
	sprintf(utlbuf, "%s%s%s", fieldname, ":", dimlist);

	Vgetname(SWXSwath[swathID % idOffset].IDTable, swathname);
	status = EHinsertmeta(sdInterfaceID, swathname, "s", 3L,
			      utlbuf, &numbertype);
    }

    return (status);
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWwritedatameta                                                  |
|                                                                             |
|  DESCRIPTION: Defines structural metadata for pre-existing data             |
|               field within swath structure                                  |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|  fieldname      char                fieldname                               |
|  dimlist        char                Dimension list (comma-separated list)   |
|  numbertype     int32               field type                              |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
SWwritedatameta(int32 swathID, char *fieldname, char *dimlist,
		int32 numbertype)
{
    intn            status = 0;	/* routine return status variable */

    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           dum;	/* dummy variable */
    int32           idOffset = SWIDOFFSET;	/* Swath ID offset */

    char            utlbuf[256];/* Utility buffer */
    char            swathname[80];	/* Swath name */

    status = SWchkswid(swathID, "SWwritedatameta", &dum, &sdInterfaceID,
		       &dum);

    if (status == 0)
    {
	/* Setup and write field metadata */
	/* ------------------------------ */
	sprintf(utlbuf, "%s%s%s", fieldname, ":", dimlist);

	Vgetname(SWXSwath[swathID % idOffset].IDTable, swathname);
	status = EHinsertmeta(sdInterfaceID, swathname, "s", 4L,
			      utlbuf, &numbertype);
    }
    return (status);
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWwrrdattr                                                       |
|                                                                             |
|  DESCRIPTION:                                                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|  attrname       char                attribute name                          |
|  numbertype     int32               attribute HDF numbertype                |
|  count          int32               Number of attribute elements            |
|  wrcode         char                Read/Write Code "w/r"                   |
|  datbuf         void                I/O buffer                              |
|                                                                             |
|  OUTPUTS:                                                                   |
|  datbuf                                                                     |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Oct 96   Joel Gales    Get Attribute Vgroup ID from external array         |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
static intn
SWwrrdattr(int32 swathID, char *attrname, int32 numbertype, int32 count,
	   char *wrcode, VOIDP datbuf)

{
    intn            status;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           attrVgrpID;	/* Swath attribute ID */
    int32           dum;	/* dummy variable */
    int32           idOffset = SWIDOFFSET;	/* Swath ID offset */

    /* Check Swath id */
    status = SWchkswid(swathID, "SWwrrdattr", &fid, &dum, &dum);

    if (status == 0)
    {
	/* Get attribute Vgroup ID and call EHattr to perform I/O */
	/* ------------------------------------------------------ */
	attrVgrpID = SWXSwath[swathID % idOffset].VIDTable[2];
	status = EHattr(fid, attrVgrpID, attrname, numbertype, count,
			wrcode, datbuf);
    }
    return (status);
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWwriteattr                                                      |
|                                                                             |
|  DESCRIPTION: Writes/updates attribute in a swath.                          |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|  attrname       char                attribute name                          |
|  numbertype     int32               attribute HDF numbertype                |
|  count          int32               Number of attribute elements            |
|  datbuf         void                I/O buffer                              |
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
SWwriteattr(int32 swathID, char *attrname, int32 numbertype, int32 count,
	    VOIDP datbuf)
{
    intn            status = 0;	/* routine return status variable */

    /* Call SWwrrdattr routine to write attribute */
    /* ------------------------------------------ */
    status = SWwrrdattr(swathID, attrname, numbertype, count, "w", datbuf);

    return (status);
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWreadattr                                                       |
|                                                                             |
|  DESCRIPTION: Reads attribute from a swath.                                 |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|  attrname       char                attribute name                          |
|                                                                             |
|  OUTPUTS:                                                                   |
|  datbuf         void                I/O buffer                              |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
SWreadattr(int32 swathID, char *attrname, VOIDP datbuf)
{
    intn            status = 0;	/* routine return status variable */
    int32           dum = 0;	/* dummy variable */

    /* Call SWwrrdattr routine to read attribute */
    /* ----------------------------------------- */
    status = SWwrrdattr(swathID, attrname, dum, dum, "r", datbuf);

    return (status);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWattrinfo                                                       |
|                                                                             |
|  DESCRIPTION:                                                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|  attrname       char                attribute name                          |
|                                                                             |
|  OUTPUTS:                                                                   |
|  numbertype     int32               attribute HDF numbertype                |
|  count          int32               Number of attribute elements            |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Oct 96   Joel Gales    Get Attribute Vgroup ID from external array         |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
SWattrinfo(int32 swathID, char *attrname, int32 * numbertype, int32 * count)
{
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           attrVgrpID;	/* Swath attribute ID */
    int32           dum;	/* dummy variable */
    int32           idOffset = SWIDOFFSET;	/* Swath ID offset */

    /* Check for valid swath ID */
    /* ------------------------ */
    status = SWchkswid(swathID, "SWattrinfo", &fid, &dum, &dum);

    if (status == 0)
    {
	/* Get attribute Vgroup ID and call EHattrinfo */
	/* ------------------------------------------- */
	attrVgrpID = SWXSwath[swathID % idOffset].VIDTable[2];

	status = EHattrinfo(fid, attrVgrpID, attrname, numbertype,
			    count);
    }
    return (status);
}






/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWinqattrs                                                       |
|                                                                             |
|  DESCRIPTION:                                                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  nattr          int32               Number of attributes in swath struct    |
|                                                                             |
|  INPUTS:                                                                    |
|  swath ID       int32               swath structure ID                      |
|                                                                             |
|  OUTPUTS:                                                                   |
|  attrnames      char                Attribute names in swath struct         |
|                                     (Comma-separated list)                  |
|  strbufsize     int32               Attributes name list string length      |
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Oct 96   Joel Gales    Initialize nattr                                    |
|  Oct 96   Joel Gales    Get Attribute Vgroup ID from external array         |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
SWinqattrs(int32 swathID, char *attrnames, int32 * strbufsize)
{
    intn            status;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           attrVgrpID;	/* Swath attribute ID */
    int32           dum;	/* dummy variable */
    int32           nattr = 0;	/* Number of attributes */
    int32           idOffset = SWIDOFFSET;	/* Swath ID offset */

    /* Check Swath id */
    status = SWchkswid(swathID, "SWinqattrs", &fid, &dum, &dum);

    if (status == 0)
    {
	/* Get attribute Vgroup ID and call EHattrcat */
	/* ------------------------------------------ */
	attrVgrpID = SWXSwath[swathID % idOffset].VIDTable[2];

	nattr = EHattrcat(fid, attrVgrpID, attrnames, strbufsize);
    }

    return (nattr);
}

#define REMQUOTE \
\
memmove(utlstr, utlstr + 1, strlen(utlstr) - 2); \
utlstr[strlen(utlstr) - 2] = 0;


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWinqdims                                                        |
|                                                                             |
|  DESCRIPTION: Returns dimension names and values defined in swath structure |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  nDim           int32               Number of defined dimensions            |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|                                                                             |
|  OUTPUTS:                                                                   |
|  dimnames       char                Dimension names (comma-separated)       |
|  dims           int32               Dimension values                        |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Aug 96   Joel Gales    Make metadata ODL compliant                         |
|  Feb 97   Joel Gales    Set nDim to -1 if status = -1                       |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
SWinqdims(int32 swathID, char *dimnames, int32 dims[])

{
    intn            status;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           swVgrpID;	/* Swath root Vgroup ID */
    int32           size;	/* Dimension size */
    int32           idOffset = SWIDOFFSET;	/* Swath ID offset */
    int32           nDim = 0;	/* Number of dimensions */

    char           *metabuf;	/* Pointer to structural metadata (SM) */
    char           *metaptrs[2];/* Pointers to begin and end of SM section */
    char            swathname[80];	/* Swath Name */
    char           *utlstr;     /* Utility string */


    /* Allocate space for utility string */
    /* --------------------------------- */
    utlstr = (char *) calloc(UTLSTR_MAX_SIZE, sizeof(char));
    if(utlstr == NULL)
    { 
	HEpush(DFE_NOSPACE,"SWinqdims", __FILE__, __LINE__);
	return(-1);
    }

    /* Check for valid swath id */
    status = SWchkswid(swathID, "SWinqdims", &fid, &sdInterfaceID, &swVgrpID);

    if (status == 0)
    {
	/* If dimension names or sizes are desired ... */
	/* ------------------------------------------- */
	if (dimnames != NULL || dims != NULL)
	{
	    /* Get swath name */
	    Vgetname(SWXSwath[swathID % idOffset].IDTable, swathname);

	    /* Get pointers to "Dimension" section within SM */
	    metabuf = (char *) EHmetagroup(sdInterfaceID, swathname, "s",
					   "Dimension", metaptrs);
	    if(metabuf == NULL)
	    {
		free(utlstr);
		return(-1);
	    }
	    /* If dimension names are desired then "clear" name buffer */
	    if (dimnames != NULL)
	    {
		dimnames[0] = 0;
	    }


	    /* Begin loop through dimension entries in metadata */
	    /* ------------------------------------------------ */
	    while (1)
	    {
		/* Search for OBJECT string */
		metaptrs[0] = strstr(metaptrs[0], "\t\tOBJECT=");

		/* If found within "Dimension" metadata section ... */
		if (metaptrs[0] < metaptrs[1] && metaptrs[0] != NULL)
		{
		    /* Get Dimension Name (if desired) */
		    if (dimnames != NULL)
		    {
			/* Check 1st for old meta data then new */
			/* ------------------------------------ */
			EHgetmetavalue(metaptrs, "OBJECT", utlstr);

			/*
			 * If OBJECT value begins with double quote then old
			 * metadata, dimension name is OBJECT value.
			 * Otherwise search for "DimensionName" string
			 */
			if (utlstr[0] != '"')
			{
			    metaptrs[0] =
				strstr(metaptrs[0], "\t\t\t\tDimensionName=");
			    EHgetmetavalue(metaptrs, "DimensionName", utlstr);
			}

			/* Strip off double quotes */
			/* ----------------------- */
			REMQUOTE

			/* If not first name then add comma delimitor */
			    if (nDim > 0)
			{
			    strcat(dimnames, ",");
			}
			/* Add dimension name to dimension list */
			strcat(dimnames, utlstr);
		    }

		    /* Get Dimension Size (if desired) */
		    if (dims != NULL)
		    {
			EHgetmetavalue(metaptrs, "Size", utlstr);
			size = atol(utlstr);
			dims[nDim] = size;
		    }
		    /* Increment number of dimensions */
		    nDim++;
		}
		else
		    /* No more dimensions found */
		{
		    break;
		}
	    }
	    free(metabuf);
	}
    }


    /* Set nDim to -1 if error status exists */
    /* ------------------------------------- */
    if (status == -1)
    {
	nDim = -1;
    }
    free(utlstr);

    return (nDim);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWinqmaps                                                        |
|                                                                             |
|  DESCRIPTION: Returns dimension mappings and offsets and increments         |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  nMap           int32               Number of dimension mappings            |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|                                                                             |
|  OUTPUTS:                                                                   |
|  dimmaps        char                dimension mappings (comma-separated)    |
|  offset         int32               array of offsets                        |
|  increment      int32               array of increments                     |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Aug 96   Joel Gales    Make metadata ODL compliant                         |
|  Feb 97   Joel Gales    Set nMap to -1 if status = -1                       |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
SWinqmaps(int32 swathID, char *dimmaps, int32 offset[], int32 increment[])

{
    intn            status;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           swVgrpID;	/* Swath root Vgroup ID */
    int32           off;	/* Mapping Offset */
    int32           incr;	/* Mapping Increment */
    int32           idOffset = SWIDOFFSET;	/* Swath ID offset */
    int32           nMap = 0;	/* Number of mappings */

    char           *metabuf;	/* Pointer to structural metadata (SM) */
    char           *metaptrs[2];/* Pointers to begin and end of SM section */
    char            swathname[80];	/* Swath Name */
    char           *utlstr;     /* Utility string */


    /* Allocate space for utility string */
    /* --------------------------------- */
    utlstr = (char *) calloc(UTLSTR_MAX_SIZE, sizeof(char));
    if(utlstr == NULL)
    { 
	HEpush(DFE_NOSPACE,"SWinqmaps", __FILE__, __LINE__);
	return(-1);
    }

    /* Check for valid swath id */
    status = SWchkswid(swathID, "SWinqmaps", &fid, &sdInterfaceID, &swVgrpID);
    if (status == 0)
    {
	/* If mapping names or offsets or increments desired ... */
	/* ----------------------------------------------------- */
	if (dimmaps != NULL || offset != NULL || increment != NULL)
	{

	    /* Get swath name */
	    Vgetname(SWXSwath[swathID % idOffset].IDTable, swathname);

	    /* Get pointers to "DimensionMap" section within SM */
	    metabuf = (char *) EHmetagroup(sdInterfaceID, swathname, "s",
					   "DimensionMap", metaptrs);
	    if(metabuf == NULL)
	    {
		free(utlstr);
		return(-1);
	    }
	    /* If mapping names are desired then "clear" name buffer */
	    if (dimmaps != NULL)
	    {
		dimmaps[0] = 0;
	    }

	    /* Begin loop through mapping entries in metadata */
	    /* ---------------------------------------------- */
	    while (1)
	    {
		/* Search for OBJECT string */
		metaptrs[0] = strstr(metaptrs[0], "\t\tOBJECT=");

		/* If found within "DimensionMap" metadata section ... */
		if (metaptrs[0] < metaptrs[1] && metaptrs[0] != NULL)
		{
		    /* Get Geo & Data Dimensions (if desired) */
		    if (dimmaps != NULL)
		    {
			/* Get Geo Dim, remove quotes, add "/" */
			EHgetmetavalue(metaptrs, "GeoDimension", utlstr);
			REMQUOTE
			    strcat(utlstr, "/");

			/* if not first map then add comma delimitor */
			if (nMap > 0)
			{
			    strcat(dimmaps, ",");
			}

			/* Add to map list */
			strcat(dimmaps, utlstr);

			/* Get Data Dim, remove quotes */
			EHgetmetavalue(metaptrs, "DataDimension", utlstr);
			REMQUOTE

			/* Add to map list */
			    strcat(dimmaps, utlstr);
		    }

		    /* Get Offset (if desired) */
		    if (offset != NULL)
		    {
			EHgetmetavalue(metaptrs, "Offset", utlstr);
			off = atol(utlstr);
			offset[nMap] = off;
		    }

		    /* Get Increment (if desired) */
		    if (increment != NULL)
		    {
			EHgetmetavalue(metaptrs, "Increment", utlstr);
			incr = atol(utlstr);
			increment[nMap] = incr;
		    }

		    /* Increment number of maps */
		    nMap++;
		}
		else
		    /* No more mappings found */
		{
		    break;
		}
	    }
	    free(metabuf);
	}
    }


    /* Set nMap to -1 if error status exists */
    /* ------------------------------------- */
    if (status == -1)
    {
	nMap = -1;
    }
    free(utlstr);

    return (nMap);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWinqidxmaps                                                     |
|                                                                             |
|  DESCRIPTION: Returns indexed mappings and index sizes                      |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  nMap           int32               Number of indexed dimension mappings    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|                                                                             |
|  OUTPUTS:                                                                   |
|  idxmaps        char                indexed dimension mappings              |
|                                     (comma-separated)                       |
|  idxsizes       int32               Number of elements in each mapping      |
|                                                                             |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Aug 96   Joel Gales    Make metadata ODL compliant                         |
|  Feb 97   Joel Gales    Set nMap to -1 if status = -1                       |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
SWinqidxmaps(int32 swathID, char *idxmaps, int32 idxsizes[])

{
    intn            status;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           swVgrpID;	/* Swath root Vgroup ID */
    int32           idOffset = SWIDOFFSET;	/* Swath ID offset */
    int32           nMap = 0;	/* Number of mappings */

    char           *metabuf;	/* Pointer to structural metadata (SM) */
    char           *metaptrs[2];/* Pointers to begin and end of SM section */
    char            swathname[80];	/* Swath Name */
    char           *utlstr;     /* Utility string */
    char           *slash;	/* Pointer to slash */


    /* Allocate space for utility string */
    /* --------------------------------- */
    utlstr = (char *) calloc(UTLSTR_MAX_SIZE, sizeof(char));
    if(utlstr == NULL)
    { 
	HEpush(DFE_NOSPACE,"SWinqidxmaps", __FILE__, __LINE__);
	return(-1);
    }
    /* Check for valid swath id */
    status = SWchkswid(swathID, "SWinqidxmaps", &fid,
		       &sdInterfaceID, &swVgrpID);

    if (status == 0)
    {
	/* If mapping names or index sizes desired ... */
	/* ------------------------------------------- */
	if (idxmaps != NULL || idxsizes != NULL)
	{
	    /* Get swath name */
	    Vgetname(SWXSwath[swathID % idOffset].IDTable, swathname);

	    /* Get pointers to "IndexDimensionMap" section within SM */
	    metabuf = (char *) EHmetagroup(sdInterfaceID, swathname, "s",
					   "IndexDimensionMap", metaptrs);
	    if(metabuf == NULL)
	    {
		free(utlstr);
		return(-1);
	    }
	    /* If mapping names are desired then "clear" name buffer */
	    if (idxmaps != NULL)
	    {
		idxmaps[0] = 0;
	    }

	    /* Begin loop through mapping entries in metadata */
	    /* ---------------------------------------------- */
	    while (1)
	    {
		/* Search for OBJECT string */
		metaptrs[0] = strstr(metaptrs[0], "\t\tOBJECT=");

		/* If found within "IndexDimensionMap" metadata section ... */
		if (metaptrs[0] < metaptrs[1] && metaptrs[0] != NULL)
		{
		    /* Get Geo & Data Dimensions and # of indices */
		    if (idxmaps != NULL)
		    {
			/* Get Geo Dim, remove quotes, add "/" */
			EHgetmetavalue(metaptrs, "GeoDimension", utlstr);
			REMQUOTE
			    strcat(utlstr, "/");

			/* if not first map then add comma delimitor */
			if (nMap > 0)
			{
			    strcat(idxmaps, ",");
			}

			/* Add to map list */
			strcat(idxmaps, utlstr);


			/* Get Index size (if desired) */
			if (idxsizes != NULL)
			{
			    /* Parse off geo dimension and find its size */
			    slash = strchr(utlstr, '/');
			    *slash = 0;
			    idxsizes[nMap] = SWdiminfo(swathID, utlstr);
			}


			/* Get Data Dim, remove quotes */
			EHgetmetavalue(metaptrs, "DataDimension", utlstr);
			REMQUOTE

			/* Add to map list */
			    strcat(idxmaps, utlstr);
		    }

		    /* Increment number of maps */
		    nMap++;
		}
		else
		    /* No more mappings found */
		{
		    break;
		}
	    }
	    free(metabuf);
	}
    }


    /* Set nMap to -1 if error status exists */
    /* ------------------------------------- */
    if (status == -1)
    {
	nMap = -1;
    }
    free(utlstr);

    return (nMap);
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWinqfields                                                      |
|                                                                             |
|  DESCRIPTION: Returns fieldnames, ranks and numbertypes defined in swath.   |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  nFld           int32               Number of (geo/data) fields in swath    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|  fieldtype      char                field type (geo or data)                |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|  fieldlist      char                Field names (comma-separated)           |
|  rank           int32               Array of ranks                          |
|  numbertype     int32               Array of HDF number types               |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Aug 96   Joel Gales    Make metadata ODL compliant                         |
|  Feb 97   Joel Gales    Set nFld to -1 if status = -1                       |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
static int32
SWinqfields(int32 swathID, char *fieldtype, char *fieldlist, int32 rank[],
	    int32 numbertype[])

{
    intn            status;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           swVgrpID;	/* Swath root Vgroup ID */
    int32           idOffset = SWIDOFFSET;	/* Swath ID offset */
    int32           nFld = 0;	/* Number of mappings */
    int32           slen[8];	/* String length array */

    char           *metabuf;	/* Pointer to structural metadata (SM) */
    char           *metaptrs[2];/* Pointers to begin and end of SM section */
    char            swathname[80];	/* Swath Name */
    char           *utlstr;	/* Utility string */
    char           *utlstr2;	/* Utility string 2 */
    char           *ptr[8];	/* String pointer array */


    /* Allocate space for utility string */
    /* --------------------------------- */
    utlstr = (char *) calloc(UTLSTR_MAX_SIZE, sizeof(char));
    if(utlstr == NULL)
    { 
	HEpush(DFE_NOSPACE,"SWinqfields", __FILE__, __LINE__);
	return(-1);
    }

    utlstr2 = (char *) calloc(UTLSTR_MAX_SIZE, sizeof(char));
    if(utlstr2 == NULL)
    { 
	HEpush(DFE_NOSPACE,"SWinqfields", __FILE__, __LINE__);
	free(utlstr);
	return(-1);
    }

    /* Check for valid swath id */
    status = SWchkswid(swathID, "SWinqfields",
		       &fid, &sdInterfaceID, &swVgrpID);

    if (status == 0)
    {
	/* If field names, ranks,  or number types desired ... */
	/* --------------------------------------------------- */
	if (fieldlist != NULL || rank != NULL || numbertype != NULL)
	{
	    /* Get swath name */
	    Vgetname(SWXSwath[swathID % idOffset].IDTable, swathname);

	    /* Get pointers to "GeoField" or "DataField" section within SM */
	    if (strcmp(fieldtype, "Geolocation Fields") == 0)
	    {
		metabuf = (char *) EHmetagroup(sdInterfaceID, swathname, "s",
					       "GeoField", metaptrs);
		if(metabuf == NULL)
		{
		    free(utlstr);
		    free(utlstr2);
		    return(-1);
		}
		strcpy(utlstr2, "GeoFieldName");
	    }
	    else
	    {
		metabuf = (char *) EHmetagroup(sdInterfaceID, swathname, "s",
					       "DataField", metaptrs);
		if(metabuf == NULL)
		{
		    free(utlstr);
		    free(utlstr2);
		    return(-1);
		}
		strcpy(utlstr2, "DataFieldName");
	    }


	    /* If field names are desired then "clear" name buffer */
	    if (fieldlist != NULL)
	    {
		fieldlist[0] = 0;
	    }


	    /* Begin loop through mapping entries in metadata */
	    /* ---------------------------------------------- */
	    while (1)
	    {
		/* Search for OBJECT string */
		metaptrs[0] = strstr(metaptrs[0], "\t\tOBJECT=");

		/* If found within "Geo" or "Data" Field metadata section .. */
		if (metaptrs[0] < metaptrs[1] && metaptrs[0] != NULL)
		{
		    /* Get Fieldnames (if desired) */
		    if (fieldlist != NULL)
		    {
			/* Check 1st for old meta data then new */
			/* ------------------------------------ */
			EHgetmetavalue(metaptrs, "OBJECT", utlstr);

			/*
			 * If OBJECT value begins with double quote then old
			 * metadata, field name is OBJECT value. Otherwise
			 * search for "GeoFieldName" or "DataFieldName"
			 * string
			 */

			if (utlstr[0] != '"')
			{
			    strcpy(utlstr, "\t\t\t\t");
			    strcat(utlstr, utlstr2);
			    strcat(utlstr, "=");
			    metaptrs[0] = strstr(metaptrs[0], utlstr);
			    EHgetmetavalue(metaptrs, utlstr2, utlstr);
			}

			/* Strip off double quotes */
			/* ----------------------- */
			REMQUOTE


			/* Add to fieldlist */
			/* ---------------- */
			    if (nFld > 0)
			{
			    strcat(fieldlist, ",");
			}
			strcat(fieldlist, utlstr);

		    }
		    /* Get Numbertype */
		    if (numbertype != NULL)
		    {
			EHgetmetavalue(metaptrs, "DataType", utlstr);
			numbertype[nFld] = EHnumstr(utlstr);
		    }
		    /*
		     * Get Rank (if desired) by counting # of dimensions in
		     * "DimList" string
		     */
		    if (rank != NULL)
		    {
			EHgetmetavalue(metaptrs, "DimList", utlstr);
			rank[nFld] = EHparsestr(utlstr, ',', ptr, slen);
		    }
		    /* Increment number of fields */
		    nFld++;
		}
		else
		    /* No more fields found */
		{
		    break;
		}
	    }
	    free(metabuf);
	}
    }

    /* Set nFld to -1 if error status exists */
    /* ------------------------------------- */
    if (status == -1)
    {
	nFld = -1;
    }

    free(utlstr);
    free(utlstr2);

    return (nFld);
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWinqgeofields                                                   |
|                                                                             |
|  DESCRIPTION: Inquires about geo fields in swath                            |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  nflds          int32               Number of geo fields in swath           |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|                                                                             |
|  OUTPUTS:                                                                   |
|  fieldlist      char                Field names (comma-separated)           |
|  rank           int32               Array of ranks                          |
|  numbertype     int32               Array of HDF number types               |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
SWinqgeofields(int32 swathID, char *fieldlist, int32 rank[],
	       int32 numbertype[])
{

    int32           nflds;	/* Number of Geolocation fields */

    /* Call "SWinqfields" routine */
    /* -------------------------- */
    nflds = SWinqfields(swathID, "Geolocation Fields", fieldlist, rank,
			numbertype);

    return (nflds);

}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWinqdatafields                                                  |
|                                                                             |
|  DESCRIPTION: Inquires about data fields in swath                           |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  nflds          int32               Number of data fields in swath          |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|                                                                             |
|  OUTPUTS:                                                                   |
|  fieldlist      char                Field names (comma-separated)           |
|  rank           int32               Array of ranks                          |
|  numbertype     int32               Array of HDF number types               |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
SWinqdatafields(int32 swathID, char *fieldlist, int32 rank[],
		int32 numbertype[])
{

    int32           nflds;	/* Number of Data fields */

    /* Call "SWinqfields" routine */
    /* -------------------------- */
    nflds = SWinqfields(swathID, "Data Fields", fieldlist, rank,
			numbertype);

    return (nflds);

}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWnentries                                                       |
|                                                                             |
|  DESCRIPTION: Returns number of entries and string buffer size              |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  nEntries       int32               Number of entries                       |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|  entrycode      int32               Entry code                              |
|	                              HDFE_NENTDIM  (0)                       |
|	                              HDFE_NENTMAP  (1)                       |
|	                              HDFE_NENTIMAP (2)                       |
|	                              HDFE_NENTGFLD (3)                       |
|	                              HDFE_NENTDFLD (4)                       |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|  strbufsize     int32               Length of comma-separated list          |
|                                     (Does not include null-terminator       |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Aug 96   Joel Gales    Make metadata ODL compliant                         |
|  Feb 97   Joel Gales    Set nEntries to -1 if status = -1                   |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
SWnentries(int32 swathID, int32 entrycode, int32 * strbufsize)

{
    intn            status;	    /* routine return status variable */
    intn            i;		    /* Loop index */

    int32           fid;	    /* HDF-EOS file ID */
    int32           sdInterfaceID;  /* HDF SDS interface ID */
    int32           swVgrpID;	    /* Swath root Vgroup ID */
    int32           idOffset = SWIDOFFSET;	/* Swath ID offset */
    int32           nEntries = 0;   /* Number of entries */
    int32           metaflag;	    /* Old (0), New (1) metadata flag) */
    int32           nVal;	    /* Number of strings to search for */

    char           *metabuf = NULL; /* Pointer to structural metadata (SM) */
    char           *metaptrs[2];    /* Pointers to begin and end of SM section */
    char            swathname[80];  /* Swath Name */
    char           *utlstr;	    /* Utility string */
    char            valName[2][32]; /* Strings to search for */

    /* Allocate space for utility string */
    /* --------------------------------- */
    utlstr = (char *) calloc(UTLSTR_MAX_SIZE, sizeof(char));
    if(utlstr == NULL)
    { 
	HEpush(DFE_NOSPACE,"SWnemtries", __FILE__, __LINE__);
	return(-1);
    }
    /* Check for valid swath id */
    /* ------------------------ */
    status = SWchkswid(swathID, "SWnentries", &fid, &sdInterfaceID, &swVgrpID);

    if (status == 0)
    {
	/* Get swath name */
	Vgetname(SWXSwath[swathID % idOffset].IDTable, swathname);

	/* Zero out string buffer size */
	*strbufsize = 0;


	/*
	 * Get pointer to relevant section within SM and Get names of
	 * metadata strings to inquire about
	 */
	switch (entrycode)
	{
	case HDFE_NENTDIM:
	    /* Dimensions */
	    {
		metabuf = (char *) EHmetagroup(sdInterfaceID, swathname, "s",
					       "Dimension", metaptrs);
		if(metabuf == NULL)
		{
		    free(utlstr);
		    return(-1);
		}
		nVal = 1;
		strcpy(&valName[0][0], "DimensionName");
	    }
	    break;

	case HDFE_NENTMAP:
	    /* Dimension Maps */
	    {
		metabuf = (char *) EHmetagroup(sdInterfaceID, swathname, "s",
					       "DimensionMap", metaptrs);
		if(metabuf == NULL)
		{
		    free(utlstr);
		    return(-1);
		}
		nVal = 2;
		strcpy(&valName[0][0], "GeoDimension");
		strcpy(&valName[1][0], "DataDimension");
	    }
	    break;

	case HDFE_NENTIMAP:
	    /* Indexed Dimension Maps */
	    {
		metabuf = (char *) EHmetagroup(sdInterfaceID, swathname, "s",
					     "IndexDimensionMap", metaptrs);
		if(metabuf == NULL)
		{
		    free(utlstr);
		    return(-1);
		}
		nVal = 2;
		strcpy(&valName[0][0], "GeoDimension");
		strcpy(&valName[1][0], "DataDimension");
	    }
	    break;

	case HDFE_NENTGFLD:
	    /* Geolocation Fields */
	    {
		metabuf = (char *) EHmetagroup(sdInterfaceID, swathname, "s",
					       "GeoField", metaptrs);
		if(metabuf == NULL)
		{
		    free(utlstr);
		    return(-1);
		}
		nVal = 1;
		strcpy(&valName[0][0], "GeoFieldName");
	    }
	    break;

	case HDFE_NENTDFLD:
	    /* Data Fields */
	    {
		metabuf = (char *) EHmetagroup(sdInterfaceID, swathname, "s",
					       "DataField", metaptrs);
		if(metabuf == NULL)
		{
		    free(utlstr);
		    return(-1);
		}
		nVal = 1;
		strcpy(&valName[0][0], "DataFieldName");
	    }
	    break;
	}


	/*
	 * Check for presence of 'GROUP="' string If found then old metadata,
	 * search on OBJECT string
	 */
        if (metabuf)
        {
            metaflag = (strstr(metabuf, "GROUP=\"") == NULL) ? 1 : 0;
            if (metaflag == 0)
            {
                nVal = 1;
                strcpy(&valName[0][0], "\t\tOBJECT");
            }


            /* Begin loop through entries in metadata */
            /* -------------------------------------- */
            while (1)
            {
                /* Search for first string */
                strcpy(utlstr, &valName[0][0]);
                strcat(utlstr, "=");
                metaptrs[0] = strstr(metaptrs[0], utlstr);

                /* If found within relevant metadata section ... */
                if (metaptrs[0] < metaptrs[1] && metaptrs[0] != NULL)
                {
                    for (i = 0; i < nVal; i++)
                    {
                        /*
                         * Get all string values Don't count quotes
                         */
                        EHgetmetavalue(metaptrs, &valName[i][0], utlstr);
                        *strbufsize += strlen(utlstr) - 2;
                    }
                    /* Increment number of entries */
                    nEntries++;

                    /* Go to end of OBJECT */
                    metaptrs[0] = strstr(metaptrs[0], "END_OBJECT");
                }
                else
                    /* No more entries found */
                {
                    break;
                }
            }
            free(metabuf);
        }


	/* Count comma separators & slashes (if mappings) */
	/* ---------------------------------------------- */
	if (nEntries > 0)
	{
	    *strbufsize += nEntries - 1;
	    *strbufsize += (nVal - 1) * nEntries;
	}
    }


    /* Set nEntries to -1 if error status exists */
    /* ----------------------------------------- */
    if (status == -1)
	nEntries = -1;

    free(utlstr);

    return (nEntries);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWinqswath                                                       |
|                                                                             |
|  DESCRIPTION: Returns number and names of swath structures in file          |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  nSwath         int32               Number of swath structures in file      |
|                                                                             |
|  INPUTS:                                                                    |
|  filename       char                HDF-EOS filename                        |
|                                                                             |
|  OUTPUTS:                                                                   |
|  swathlist      char                List of swath names (comma-separated)   |
|  strbufsize     int32               Length of swathlist                     |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
SWinqswath(char *filename, char *swathlist, int32 * strbufsize)
{
    int32           nSwath;	/* Number of swath structures in file */

    /* Call "EHinquire" routine */
    /* ------------------------ */
    nSwath = EHinquire(filename, "SWATH", swathlist, strbufsize);

    return (nSwath);
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SW1dfldsrch                                                      |
|                                                                             |
|  DESCRIPTION: Retrieves information about a 1D field                        |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  fid            int32               HDF-EOS file ID                         |
|  swathID        int32               swath structure ID                      |
|  fieldname      const char          field name                              |
|  access         const char          Access code (w/r)                       |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|  vgidout        int32               Field (geo/data) vgroup ID              |
|  vdataIDout     int32               Field Vdata ID                          |
|  fldtype        int32               Field type                              |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
static intn
SW1dfldsrch(int32 fid, int32 swathID, const char *fieldname, const char *access,
	    int32 * vgidout, int32 * vdataIDout, int32 * fldtype)

{
    intn            status = 0;	/* routine return status variable */

    int32           sID;	/* SwathID - offset */
    int32           vgid;	/* Swath Geo or Data Vgroup ID */
    int32           vdataID;	/* 1d field vdata */
    int32           idOffset = SWIDOFFSET;	/* Swath ID offset */


    /* Compute "reduced" swath ID */
    /* -------------------------- */
    sID = swathID % idOffset;


    /* Get Geolocation Vgroup id and 1D field name Vdata id */
    /* ---------------------------------------------------- */
    vgid = SWXSwath[sID].VIDTable[0];
    vdataID = EHgetid(fid, vgid, fieldname, 1, access);
    *fldtype = 0;


    /*
     * If name not found in Geolocation Vgroup then detach Geolocation Vgroup
     * and search in Data Vgroup
     */
    if (vdataID == -1)
    {
	vgid = SWXSwath[sID].VIDTable[1];;
	vdataID = EHgetid(fid, vgid, fieldname, 1, access);
	*fldtype = 1;

	/* If field also not found in Data Vgroup then set error status */
	/* ------------------------------------------------------------ */
	if (vdataID == -1)
	{
	    status = -1;
	    vgid = -1;
	    vdataID = -1;
	}
    }
    *vgidout = vgid;
    *vdataIDout = vdataID;

    return (status);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWSDfldsrch                                                      |
|                                                                             |
|  DESCRIPTION: Retrieves information SDS field                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|  sdInterfaceID  int32               SD interface ID                         |
|  fieldname      const char          field name                              |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|  sdid           int32               SD element ID                           |
|  rankSDS        int32               Rank of SDS                             |
|  rankFld        int32               True rank of field (merging)            |
|  offset         int32               Offset of field within merged field     |
|  dims           int32               Dimensions of field                     |
|  solo           int32               Solo field flag                         |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Aug 96   Joel Gales    Make metadata ODL compliant                         |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
static intn
SWSDfldsrch(int32 swathID, int32 sdInterfaceID, const char *fieldname,
            int32 * sdid, int32 * rankSDS, int32 * rankFld, int32 * offset,
            int32 dims[], int32 * solo)
{
    intn            i;		/* Loop index */
    intn            status = -1;/* routine return status variable */

    int32           sID;	/* SwathID - offset */
    int32           idOffset = SWIDOFFSET;	/* Swath ID offset */
    int32           dum;	/* Dummy variable */
    int32           dums[128];	/* Dummy array */
    int32           attrIndex;	/* Attribute index */

    char            name[2048];	/* Merged-Field Names */
    char            swathname[80];	/* Swath Name */
    char           *utlstr;	/* Utility string */
    char           *metabuf;	/* Pointer to structural metadata (SM) */
    char           *metaptrs[2];/* Pointers to begin and end of SM section */
    char           *oldmetaptr;	/* Pointer within SM section */


    /* Allocate space for utility string */
    /* --------------------------------- */
    utlstr = (char *) calloc(UTLSTR_MAX_SIZE, sizeof(char));
    if(utlstr == NULL)
    { 
	HEpush(DFE_NOSPACE,"SWSDfldsrch", __FILE__, __LINE__);
	return(-1);
    }
    /* Set solo flag to 0 (no) */
    /* ----------------------- */
    *solo = 0;


    /* Compute "reduced" swath ID */
    /* -------------------------- */
    sID = swathID % idOffset;


    /* Loop through all SDSs in swath */
    /* ------------------------------ */
    for (i = 0; i < SWXSwath[sID].nSDS; i++)
    {
	/* If active SDS ... */
	/* ----------------- */
	if (SWXSwath[sID].sdsID[i] != 0)
	{
	    /* Get SDS ID, name, rankSDS, and dimensions */
	    /* ----------------------------------------- */
	    *sdid = SWXSwath[sID].sdsID[i];
	    SDgetinfo(*sdid, name, rankSDS, dims, &dum, &dum);
	    *rankFld = *rankSDS;

	    /* If merged field ... */
	    /* ------------------- */
	    if (strstr(name, "MRGFLD_") == &name[0])
	    {
		/* Get swath name */
		/* -------------- */
		Vgetname(SWXSwath[sID].IDTable, swathname);


		/* Get pointers to "MergedFields" section within SM */
		/* ------------------------------------------------ */
		metabuf = (char *) EHmetagroup(sdInterfaceID, swathname, "s",
					       "MergedFields", metaptrs);
		if(metabuf == NULL)
		{
		    free(utlstr);
		    return(-1);
		}

		/* Store metaptr in order to recover */
		/* --------------------------------- */
		oldmetaptr = metaptrs[0];


		/* Search for Merged field name */
		/* ---------------------------- */
		sprintf(utlstr, "%s%s%s", "MergedFieldName=\"",
			name, "\"\n");
		metaptrs[0] = strstr(metaptrs[0], utlstr);


		/* If not found check for old metadata */
		/* ----------------------------------- */
		if (metaptrs[0] == NULL)
		{
		    sprintf(utlstr, "%s%s%s", "OBJECT=\"", name, "\"\n");
		    metaptrs[0] = strstr(oldmetaptr, utlstr);
		}


		/* Get field list and strip off leading and trailing quotes */
		EHgetmetavalue(metaptrs, "FieldList", name);  /* not return status --xhua */
		memmove(name, name + 1, strlen(name) - 2);
		name[strlen(name) - 2] = 0;

		/* Search for desired field within merged field list */
		sprintf(utlstr, "%s%s%s", "\"", fieldname, "\"");
		dum = EHstrwithin(utlstr, name, ',');

		free(metabuf);
	    }
	    else
	    {
		/* If solo (unmerged) check if SDS name matches fieldname */
		/* ------------------------------------------------------ */
		dum = EHstrwithin(fieldname, name, ',');
		if (dum != -1)
		{
		    *solo = 1;
		    *offset = 0;
		}
	    }


	    /* If field found ... */
	    /* ------------------ */
	    if (dum != -1)
	    {
		status = 0;

		/* If merged field ... */
		/* ------------------- */
		if (*solo == 0)
		{
		    /* Get "Field Offsets" SDS attribute index */
		    /* --------------------------------------- */
		    attrIndex = SDfindattr(*sdid, "Field Offsets");

		    /*
		     * If attribute exists then get offset of desired field
		     * within merged field
		     */
		    if (attrIndex != -1)
		    {
			SDreadattr(*sdid, attrIndex, (VOIDP) dums);
			*offset = dums[dum];
		    }


		    /* Get "Field Dims" SDS attribute index */
		    /* ------------------------------------ */
		    attrIndex = SDfindattr(*sdid, "Field Dims");

		    /*
		     * If attribute exists then get 0th dimension of desired
		     * field within merged field
		     */
		    if (attrIndex != -1)
		    {
			SDreadattr(*sdid, attrIndex, (VOIDP) dums);
			dims[0] = dums[dum];

			/* If this dimension = 1 then field is really 2 dim */
			/* ------------------------------------------------ */
			if (dums[dum] == 1)
			{
			    *rankFld = 2;
			}
		    }
		}


		/* Break out of SDS loop */
		/* --------------------- */
		break;
	    }			/* End of found field section */
	}
	else
	{
	    /* First non-active SDS signifies no more, break out of SDS loop */
	    /* ------------------------------------------------------------- */
	    break;
	}
    }

    free(utlstr);

    return (status);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWwrrdfield                                                      |
|                                                                             |
|  DESCRIPTION: Writes/Reads fields                                           |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|  fieldname      const char          fieldname                               |
|  code           const char          Write/Read code (w/r)                   |
|  start          int32               start array                             |
|  stride         int32               stride array                            |
|  edge           int32               edge array                              |
|  datbuf         void                data buffer for read                    |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|  datbuf         void                data buffer for write                   |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Feb 97   Joel Gales    Stride = 1 HDF compression workaround               |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
static intn
SWwrrdfield(int32 swathID, const char *fieldname, const char *code,
	    int32 start[], int32 stride[], int32 edge[], VOIDP datbuf)

{
    intn            i;		/* Loop index */
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           vgid;	/* Swath Geo or Data Vgroup ID */
    int32           sdid;	/* SDS ID */
    int32           dum;	/* Dummy variable */
    int32           rankSDS;	/* Rank of SDS */
    int32           rankFld;	/* Rank of field */

    int32           vdataID;	/* 1d field vdata */
    int32           recsize;	/* Vdata record size */
    int32           fldsize;	/* Field size */
    int32           nrec;	/* Number of records in Vdata */

    int32           offset[8];	/* I/O offset (start) */
    int32           incr[8];	/* I/O incrment (stride) */
    int32           count[8];	/* I/O count (edge) */
    int32           dims[8];	/* Field/SDS dimensions */
    int32           mrgOffset;	/* Merged field offset */
    int32           nflds;	/* Number of fields in Vdata */
    int32           strideOne;	/* Strides = 1 flag */

    uint8          *buf;	/* I/O (transfer) buffer */
    uint8          *fillbuf;	/* Fill value buffer */

    char            attrName[80];	/* Name of fill value attribute */
    char           *ptr[64];	/* String pointer array */
    char            fieldlist[256];	/* Vdata field list */


    /* Check for valid swath ID */
    /* ------------------------ */
    status = SWchkswid(swathID, "SWwrrdfield", &fid, &sdInterfaceID, &dum);


    if (status == 0)
    {

	/* Check whether fieldname is in SDS (multi-dim field) */
	/* --------------------------------------------------- */
	status = SWSDfldsrch(swathID, sdInterfaceID, fieldname, &sdid,
			     &rankSDS, &rankFld, &mrgOffset, dims, &dum);

	/* Multi-Dimensional Field Section */
	/* ------------------------------- */
	if (status != -1)
	{
	    /* Set I/O offset Section */
	    /* ---------------------- */

	    /*
	     * If start == NULL (default) set I/O offset of 0th field to
	     * offset within merged field (if any) and the rest to 0
	     */
	    if (start == NULL)
	    {
		for (i = 0; i < rankSDS; i++)
		{
		    offset[i] = 0;
		}
		offset[0] = mrgOffset;
	    }
	    else
	    {
		/*
		 * ... otherwise set I/O offset to user values, adjusting the
		 * 0th field with the merged field offset (if any)
		 */
		if (rankFld == rankSDS)
		{
		    for (i = 0; i < rankSDS; i++)
		    {
			offset[i] = start[i];
		    }
		    offset[0] += mrgOffset;
		}
		else
		{
		    /*
		     * If field really 2-dim merged in 3-dim field then set
		     * 0th field offset to merge offset and then next two to
		     * the user values
		     */
		    for (i = 0; i < rankFld; i++)
		    {
			offset[i + 1] = start[i];
		    }
		    offset[0] = mrgOffset;
		}
	    }



	    /* Set I/O stride Section */
	    /* ---------------------- */

	    /*
	     * If stride == NULL (default) set I/O stride to 1
	     */
	    if (stride == NULL)
	    {
		for (i = 0; i < rankSDS; i++)
		{
		    incr[i] = 1;
		}
	    }
	    else
	    {
		/*
		 * ... otherwise set I/O stride to user values
		 */
		if (rankFld == rankSDS)
		{
		    for (i = 0; i < rankSDS; i++)
		    {
			incr[i] = stride[i];
		    }
		}
		else
		{
		    /*
		     * If field really 2-dim merged in 3-dim field then set
		     * 0th field stride to 1 and then next two to the user
		     * values.
		     */
		    for (i = 0; i < rankFld; i++)
		    {
			incr[i + 1] = stride[i];
		    }
		    incr[0] = 1;
		}
	    }



	    /* Set I/O count Section */
	    /* --------------------- */

	    /*
	     * If edge == NULL (default) set I/O count to number of remaining
	     * entries (dims - start) / increment.  Note that 0th field
	     * offset corrected for merged field offset (if any).
	     */
	    if (edge == NULL)
	    {
		for (i = 1; i < rankSDS; i++)
		{
		    count[i] = (dims[i] - offset[i]) / incr[i];
		}
		count[0] = (dims[0] - (offset[0] - mrgOffset)) / incr[0];
	    }
	    else
	    {
		/*
		 * ... otherwise set I/O count to user values
		 */
		if (rankFld == rankSDS)
		{
		    for (i = 0; i < rankSDS; i++)
		    {
			count[i] = edge[i];
		    }
		}
		else
		{
		    /*
		     * If field really 2-dim merged in 3-dim field then set
		     * 0th field count to 1 and then next two to the user
		     * values.
		     */
		    for (i = 0; i < rankFld; i++)
		    {
			count[i + 1] = edge[i];
		    }
		    count[0] = 1;
		}
	    }

	    /* Perform I/O with relevant HDF I/O routine */
	    /* ----------------------------------------- */
	    if (strcmp(code, "w") == 0)
	    {
		/* Set strideOne to true (1) */
		/* ------------------------- */
		strideOne = 1;


		/* If incr[i] != 1 set strideOne to false (0) */
		/* ------------------------------------------ */
		for (i = 0; i < rankSDS; i++)
		{
		    if (incr[i] != 1)
		    {
			strideOne = 0;
			break;
		    }
		}


		/*
		 * If strideOne is true use NULL parameter for stride. This
		 * is a work-around to HDF compression problem
		 */
		if (strideOne == 1)
		{
		    status = SDwritedata(sdid, offset, NULL, count,
					 (VOIDP) datbuf);
		}
		else
		{
		    status = SDwritedata(sdid, offset, incr, count,
					 (VOIDP) datbuf);
		}
	    }
	    else
	    {
		status = SDreaddata(sdid, offset, incr, count,
				    (VOIDP) datbuf);
	    }
	}			/* End of Multi-Dimensional Field Section */
	else
	{

	    /* One-Dimensional Field Section */
	    /* ----------------------------- */

	    /* Check fieldname within 1d field Vgroups */
	    /* --------------------------------------- */
	    status = SW1dfldsrch(fid, swathID, fieldname, code,
				 &vgid, &vdataID, &dum);

	    if (status != -1)
	    {

		/* Get number of records */
		/* --------------------- */
		nrec = VSelts(vdataID);


		/* Set offset, increment, & count */
		/* ------------------------------ */
		offset[0] = (start == NULL) ? 0 : start[0];
		incr[0] = (stride == NULL) ? 1 : stride[0];
		count[0] = (edge == NULL)
		    ? (nrec - offset[0]) / incr[0]
		    : edge[0];



		/* Write Section */
		/* ------------- */
		if (strcmp(code, "w") == 0)
		{
		    /* Get size of field and setup fill buffer */
		    /* --------------------------------------- */
		    fldsize = VSsizeof(vdataID, (char *)fieldname);
		    fillbuf = (uint8 *) calloc(fldsize, 1);
		    if(fillbuf == NULL)
		    { 
			HEpush(DFE_NOSPACE,"SWwrrdfield", __FILE__, __LINE__);
			return(-1);
		    }

		    /* Get size of record in Vdata and setup I/O buffer */
		    /* ------------------------------------------------ */
		    VSQueryvsize(vdataID, &recsize);
		    buf = (uint8 *) calloc(recsize, count[0] * incr[0]);
		    if(buf == NULL)
		    { 
			HEpush(DFE_NOSPACE,"SWwrrdfield", __FILE__, __LINE__);
			return(-1);
		    }


		    /* Get names and number of fields in each record  */
		    /* ---------------------------------------------- */
		    VSgetfields(vdataID, fieldlist);
		    dum = EHstrwithin(fieldname, fieldlist, ',');
		    nflds = EHparsestr(fieldlist, ',', ptr, NULL);


		    /* Get Merged Field Offset (if any) */
		    /* -------------------------------- */
		    if (nflds > 1)
		    {
			if (dum > 0)
			{
			    *(ptr[dum] - 1) = 0;
			    mrgOffset = VSsizeof(vdataID, fieldlist);
			    *(ptr[dum] - 1) = ',';
			}
			else
			{
			    mrgOffset = 0;
			}

			/* Read records to recover previously written data */
			status = VSsetfields(vdataID, fieldlist);
			status = VSseek(vdataID, offset[0]);
			nrec = VSread(vdataID, buf, count[0] * incr[0],
				      FULL_INTERLACE);
		    }
		    else
		    {
			mrgOffset = 0;
		    }



		    /* Fill buffer with "Fill" value (if any) */
		    /* -------------------------------------- */
		    strcpy(attrName, "_FV_");
		    strcat(attrName, fieldname);

		    status = SWreadattr(swathID, attrName, (char *) fillbuf);
		    if (status == 0)
		    {
			for (i = 0; i < count[0] * incr[0]; i++)
			{
			    memcpy(buf + i * recsize + mrgOffset,
				   fillbuf, fldsize);
			}
		    }


		    /* Write new data into buffer */
		    /* -------------------------- */
		    if (incr[0] == 1 && nflds == 1)
		    {
			memcpy(buf, datbuf, count[0] * recsize);
		    }
		    else
		    {
			for (i = 0; i < count[0]; i++)
			{
			    memcpy(buf + i * recsize * incr[0] + mrgOffset,
				   (uint8 *) datbuf + i * fldsize, fldsize);
			}
		    }


		    /* If append read last record */
		    /* -------------------------- */
		    if (offset[0] == nrec)
		    {
			/* abe added "status =" to next line 8/8/97 */
			status = VSseek(vdataID, offset[0] - 1);
			VSread(vdataID, fillbuf, 1, FULL_INTERLACE);
		    }
		    else
		    {
			status = VSseek(vdataID, offset[0]);
		    }


		    /* Write data into Vdata */
		    /* --------------------- */
		    nrec = VSwrite(vdataID, buf, count[0] * incr[0],
				   FULL_INTERLACE);

		    free(fillbuf);
                    if (status > 0)
                       status = 0;

		}		/* End Write Section */
		else
		{
		    /* Read Section */
		    /* ------------ */
		    status = VSsetfields(vdataID, fieldname);
		    fldsize = VSsizeof(vdataID, (char *)fieldname);
		    buf = (uint8 *) calloc(fldsize, count[0] * incr[0]);
		    if(buf == NULL)
		    { 
			HEpush(DFE_NOSPACE,"SWwrrdfield", __FILE__, __LINE__);
			return(-1);
		    }

		    (void) VSseek(vdataID, offset[0]);
		    (void) VSread(vdataID, buf, count[0] * incr[0],
			   FULL_INTERLACE);


		    /* Copy from input buffer to returned data buffer */
		    /* ---------------------------------------------- */
		    if (incr[0] == 1)
		    {
			memcpy(datbuf, buf, count[0] * fldsize);
		    }
		    else
		    {
			for (i = 0; i < count[0]; i++)
			{
			    memcpy((uint8 *) datbuf + i * fldsize,
				   buf + i * fldsize * incr[0], fldsize);
			}
		    }

		}		/* End Read Section */

		free(buf);
		VSdetach(vdataID);
	    }
	    else
	    {
		HEpush(DFE_GENAPP, "SWwrrdfield", __FILE__, __LINE__);
		HEreport("Fieldname \"%s\" does not exist.\n", fieldname);
	    }
	}			/* End One-D Field Section */

    }
    return (status);
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWwritefield                                                     |
|                                                                             |
|  DESCRIPTION: Writes data to field                                          |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|  fieldname      char                fieldname                               |
|  start          int32               start array                             |
|  stride         int32               stride array                            |
|  edge           int32               edge array                              |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|  data           void                data buffer for write                   |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
SWwritefield(int32 swathID, char *fieldname,
	     int32 start[], int32 stride[], int32 edge[], VOIDP data)

{
    intn            status = 0;	/* routine return status variable */

    status = SWwrrdfield(swathID, fieldname, "w", start, stride, edge,
			 data);
    return (status);
}






/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWreadfield                                                      |
|                                                                             |
|  DESCRIPTION: Reads data from field                                         |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|  fieldname      const char          fieldname                               |
|  start          int32               start array                             |
|  stride         int32               stride array                            |
|  edge           int32               edge array                              |
|  buffer         void                data buffer for read                    |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|     None                                                                    |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
SWreadfield(int32 swathID, const char *fieldname,
	    int32 start[], int32 stride[], int32 edge[], VOIDP buffer)

{
    intn            status = 0;	/* routine return status variable */

    status = SWwrrdfield(swathID, fieldname, "r", start, stride, edge,
			 buffer);
    return (status);
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWdefboxregion                                                   |
|                                                                             |
|  DESCRIPTION: Finds swath cross tracks within area of interest and returns  |
|               region ID                                                     |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  regionID       int32               Region ID                               |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               Swath structure ID                      |
|  cornerlon      float64  dec deg    Longitude of opposite corners of box    |
|  cornerlat      float64  dec deg    Latitude of opposite corners of box     |
|  mode           int32               Search mode                             |
|                                     HDFE_MIDPOINT - Use midpoint of Xtrack  |
|                                     HDFE_ENDPOINT - Use endpoints of Xtrack |
|                                     HDFE_ANYPOINT - Use all points of Xtrack|
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Oct 96   Joel Gales    Add ability to handle regions crossing date line    |
|  Dec 96   Joel Gales    Add multiple vertical subsetting capability         |
|  Jul 98   Abe Taaheri   Fixed core dump in SWregioninfo associated with     |
|                         SWXRegion[k]->nRegions exceeding MAXNREGIONS in     |
|                         this function                                       |
|  Aug 99   Abe Taaheri   Fixed the code so that all cross tracks or all      |
|                         points on the along track that fall inside the box  |
|                         are identified. At the same time added code to      |
|                         function "updatescene" to reject cases where there  |
|                         is single cross track in the box (for LANDSAT)      |
|  Jun 03   Abe Taaheri   Added a few lines to report error and return -1 if  |
|                         regionID exceeded NSWATHREGN                        |
|  Mar 04   Abe Taaheri   Added recognition for GeodeticLatitude              |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
SWdefboxregion(int32 swathID, float64 cornerlon[], float64 cornerlat[],
	       int32 mode)
{
    intn            i;		/* Loop index */
    intn            j;		/* Loop index */
    intn            k;		/* Loop index */
    
    intn            status;	/* routine return status variable */
    intn            statLon;	/* Status from SWfieldinfo for longitude */
    intn            statLat;	/* Status from SWfieldinfo for latitude */
    intn            statCoLat = -1;	/* Status from SWfieldinfo for
					 * Colatitude */
    intn            statGeodeticLat = -1; /* Status from SWfieldinfo for
					 * GeodeticLatitude */

    uint8           found = 0;	/* Found flag */
    uint8          *flag;	/* Pointer to track flag array */
    intn           validReg = -1; /* -1 is invalid validReg */ 
    
    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           swVgrpID;	/* Swath Vgroup ID */
    int32           rank;	/* Rank of geolocation fields */
    int32           nt;		/* Number type of geolocation fields */
    int32           dims[8];	/* Dimensions of geolocation fields */
    int32           nElem;	/* Number of elements to read */
    int32           bndflag;	/* +/-180 longitude boundary flag */
    int32           lonTest;	/* Longitude test flag */
    int32           latTest;	/* Latitude test flag */
    int32           start[2];	/* Start array (read) */
    int32           stride[2] = {1, 1};	/* Stride array (read) */
    int32           edge[2];	/* Edge array (read) */
    int32           regionID = -1;	/* Region ID (return) */
    int32           anyStart[2];/* ANYPOINT start array (read) */
    int32           anyEdge[2];	/* ANYPOINT edge array (read) */

    float32         temp32;	/* Temporary float32 variable */

    float64         lonTestVal;	/* Longitude test value */
    float64         latTestVal;	/* Latitude test value */
    float64         temp64;	/* Temporary float64 variable */

    char           *lonArr;	/* Longitude data array */
    char           *latArr;	/* Latitude data array */
    char            dimlist[256];	/* Dimension list (geolocation
					 * fields) */
    char            latName[17];/* Latitude field name */
    

    /* Check for valid swath ID */
    /* ------------------------ */
    status = SWchkswid(swathID, "SWdefboxregion", &fid, &sdInterfaceID,
		       &swVgrpID);


    /* Inclusion mode must be between 0 and 2 */
    /* -------------------------------------- */
    if (mode < 0 || mode > 2)
    {
	status = -1;
	HEpush(DFE_GENAPP, "SWdefboxregion", __FILE__, __LINE__);
	HEreport("Improper Inclusion Mode: %d.\n", mode);
    }


    if (status == 0)
    {
	/* Get "Longitude" field info */
	/* -------------------------- */
	statLon = SWfieldinfo(swathID, "Longitude", &rank, dims, &nt, dimlist);
	if (statLon != 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "SWdefboxregion", __FILE__, __LINE__);
	    HEreport("\"Longitude\" field not found.\n");
	}

	/* Get "Latitude" field info */
	/* -------------------------- */
	statLat = SWfieldinfo(swathID, "Latitude", &rank, dims, &nt, dimlist);
	if (statLat != 0)
	{
	    /* If not found check for "Colatitude" field info */
	    /* ---------------------------------------------- */
	    statCoLat = SWfieldinfo(swathID, "Colatitude", &rank, dims, &nt,
				    dimlist);
	    if (statCoLat != 0)
	    {
	      /* Check again for Geodeticlatitude */            
	      statGeodeticLat = SWfieldinfo(swathID, 
					    "GeodeticLatitude", &rank, 
					    dims, &nt, dimlist);
	      if (statGeodeticLat != 0)
		{
		  /* Neither "Latitude" nor "Colatitude" nor
		     "GeodeticLatitude" field found */
		  /* ----------------------------------------------- */
		  status = -1;
		  HEpush(DFE_GENAPP, "SWdefboxregion", __FILE__, __LINE__);
		  HEreport(
			   "Neither \"Latitude\" nor \"Colatitude\" nor \"GeodeticLatitude\" fields found.\n");
		}
	      else
		{
		  /* Latitude field is "GeodeticLatitude" */
		  /* ------------------------------ */
		  strcpy(latName, "GeodeticLatitude");
		}
	    }
	    else
	    {
		/* Latitude field is "Colatitude" */
		/* ------------------------------ */
		strcpy(latName, "Colatitude");
	    }
	}
	else
	{
	    /* Latitude field is "Latitude" */
	    /* ---------------------------- */
	    strcpy(latName, "Latitude");
	}


	if (status == 0)
	{
	    /* Search along entire "Track" dimension from beginning to end */
	    /* ----------------------------------------------------------- */
	    start[0] = 0;
	    edge[0] = dims[0];


	    /* If 1D geolocation fields then set mode to MIDPOINT */
	    /* -------------------------------------------------- */
	    if (rank == 1)
	    {
		mode = HDFE_MIDPOINT;
	    }


	    switch (mode)
	    {
		/* If MIDPOINT search single point in middle of "CrossTrack" */
		/* --------------------------------------------------------- */
	    case HDFE_MIDPOINT:

		start[1] = dims[1] / 2;
		edge[1] = 1;

		break;

		/* If ENDPOINT search 2 points at either end of "CrossTrack" */
		/* --------------------------------------------------------- */
	    case HDFE_ENDPOINT:

		start[1] = 0;
		stride[1] = dims[1] - 1;
		edge[1] = 2;

		break;

		/* If ANYPOINT do initial MIDPOINT search */
		/* -------------------------------------- */
	    case HDFE_ANYPOINT:

		start[1] = dims[1] / 2;
		edge[1] = 1;

		break;
	    }


	    /* Compute number of elements */
	    /* -------------------------- */
	    nElem = edge[0] * edge[1];


	    /* Allocate space for longitude and latitude (float64) */
	    /* --------------------------------------------------- */
	    lonArr = (char *) calloc(nElem, sizeof(float64));
	    if(lonArr == NULL)
	    { 
		HEpush(DFE_NOSPACE,"SWdefboxregion", __FILE__, __LINE__);
		return(-1);
	    }

	    latArr = (char *) calloc(nElem, sizeof(float64));
	    if(latArr == NULL)
	    { 
		HEpush(DFE_NOSPACE,"SWdefboxregion", __FILE__, __LINE__);
		free(lonArr);
		return(-1);
	    }


	    /* Allocate space for flag array (uint8) */
	    /* ------------------------------------- */
	    flag = (uint8 *) calloc(edge[0] + 1, 1);
	    if(flag == NULL)
	    { 
		HEpush(DFE_NOSPACE,"SWdefboxregion", __FILE__, __LINE__);
		free(lonArr);
		free(latArr);
		return(-1);
	    }


	    /* Read Longitude and Latitude fields */
	    /* ---------------------------------- */
	    status = SWreadfield(swathID, "Longitude",
				 start, stride, edge, lonArr);
	    status = SWreadfield(swathID, latName,
				 start, stride, edge, latArr);



	    /*
	     * If geolocation fields are FLOAT32 then cast each entry as
	     * FLOAT64
	     */
	    if (nt == DFNT_FLOAT32)
	    {
		for (i = nElem - 1; i >= 0; i--)
		{
		    memcpy(&temp32, lonArr + 4 * i, 4);
		    temp64 = (float64) temp32;
		    memcpy(lonArr + 8 * i, &temp64, 8);

		    memcpy(&temp32, latArr + 4 * i, 4);
		    temp64 = (float64) temp32;
		    memcpy(latArr + 8 * i, &temp64, 8);
		}
	    }   


	    /* Set boundary flag */
	    /* ----------------- */

	    /*
	     * This variable is set to 1 if the region of interest crosses
	     * the +/- 180 longitude boundary
	     */
	    bndflag = (cornerlon[0] < cornerlon[1]) ? 0 : 1;



	    /* Main Search Loop */
	    /* ---------------- */

	    /* For each track ... */
	    /* ------------------ */

	    for (i = 0; i < edge[0]; i++)
	    {   
		/* For each value from Cross Track ... */
		/* ----------------------------------- */
		for (j = 0; j < edge[1]; j++)
		{
		    /* Read in single lon & lat values from data buffers */
		    /* ------------------------------------------------- */
		    memcpy(&lonTestVal, &lonArr[8 * (i * edge[1] + j)], 8);
		    memcpy(&latTestVal, &latArr[8 * (i * edge[1] + j)], 8);


		    /* If longitude value > 180 convert to -180 to 180 range */
		    /* ----------------------------------------------------- */
		    if (lonTestVal > 180)
		    {
			lonTestVal = lonTestVal - 360;
		    }

		    /* If Colatitude value convert to latitude value */
		    /* --------------------------------------------- */
		    if (statCoLat == 0)
		    {
			latTestVal = 90 - latTestVal;
		    }


		    /* Test if lat value is within range */
		    /* --------------------------------- */
		    latTest = (latTestVal >= cornerlat[0] &&
			       latTestVal <= cornerlat[1]);


		    if (bndflag == 1)
		    {
			/*
			 * If boundary flag set test whether longitude value
			 * is outside region and then flip
			 */
			lonTest = (lonTestVal >= cornerlon[1] &&
				   lonTestVal <= cornerlon[0]);
			lonTest = 1 - lonTest;
		    }
		    else
		    {
			lonTest = (lonTestVal >= cornerlon[0] &&
				   lonTestVal <= cornerlon[1]);
		    }


		    /*
		     * If both longitude and latitude are within region set
		     * flag on for this track
		     */
		    if (lonTest + latTest == 2)
		    {
			flag[i] = 1;
			found = 1;
			break;
		    }
		}
	    }



	    /* ANYPOINT search */
	    /* --------------- */
	    if (mode == HDFE_ANYPOINT && rank > 1)
	    {
		free(lonArr);
		free(latArr);

		/* Allocate space for an entire single cross track */
		/* ----------------------------------------------- */
		lonArr = (char *) calloc(dims[1], sizeof(float64));
		if(lonArr == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"SWdefboxregion", __FILE__, __LINE__);
		    return(-1);
		}

		latArr = (char *) calloc(dims[1], sizeof(float64));
		if(latArr == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"SWdefboxregion", __FILE__, __LINE__);
		    free(lonArr);
		    return(-1);
		}


		/* Setup start and edge */
		/* -------------------- */
		anyStart[1] = 0;
		anyEdge[0] = 1;
		anyEdge[1] = dims[1];
	
               
		/* For each track starting from 0 */
		/* ------------------------------ */
		for (i = 0; i < edge[0]; i++)    
		{
               
		    /* If cross track not in region (with MIDPOINT search ... */
		    /* ------------------------------------------------------ */
		    if (flag[i] == 0)
		    {
			/* Setup track start */
			/* ----------------- */
			anyStart[0] = i;


			/* Read in lon and lat values for cross track */
			/* ------------------------------------------ */
			status = SWreadfield(swathID, "Longitude",
					   anyStart, NULL, anyEdge, lonArr);
			status = SWreadfield(swathID, latName,
					   anyStart, NULL, anyEdge, latArr);



			/*
			 * If geolocation fields are FLOAT32 then cast each
			 * entry as FLOAT64
			 */
			if (nt == DFNT_FLOAT32)
			{
			    for (j = dims[1] - 1; j >= 0; j--)
			    {
				memcpy(&temp32, lonArr + 4 * j, 4);
				temp64 = (float64) temp32;
				memcpy(lonArr + 8 * j, &temp64, 8);

				memcpy(&temp32, latArr + 4 * j, 4);
				temp64 = (float64) temp32;
				memcpy(latArr + 8 * j, &temp64, 8);
			    }
			}


			/* For each value from Cross Track ... */
			/* ----------------------------------- */
			for (j = 0; j < dims[1]; j++)
			{
			    /* Read in single lon & lat values from buffers */
			    /* -------------------------------------------- */
			    memcpy(&lonTestVal, &lonArr[8 * j], 8);
			    memcpy(&latTestVal, &latArr[8 * j], 8);


			    /* If lon value > 180 convert to -180 - 180 range */
			    /* ---------------------------------------------- */
			    if (lonTestVal > 180)
			    {
				lonTestVal = lonTestVal - 360;
			    }

			    /* If Colatitude value convert to latitude value */
			    /* --------------------------------------------- */
			    if (statCoLat == 0)
			    {
				latTestVal = 90 - latTestVal;
			    }


			    /* Test if lat value is within range */
			    /* --------------------------------- */
			    latTest = (latTestVal >= cornerlat[0] &&
				       latTestVal <= cornerlat[1]);


			    if (bndflag == 1)
			    {
				/*
				 * If boundary flag set test whether
				 * longitude value is outside region and then
				 * flip
				 */
				lonTest = (lonTestVal >= cornerlon[1] &&
					   lonTestVal <= cornerlon[0]);
				lonTest = 1 - lonTest;
			    }
			    else
			    {
				lonTest = (lonTestVal >= cornerlon[0] &&
					   lonTestVal <= cornerlon[1]);
			    }


			    /*
			     * If both longitude and latitude are within
			     * region set flag on for this track
			     */
			    if (lonTest + latTest == 2)
			    {
				flag[i] = 1;
				found = 1;
				break;
			    }
			}
		    }
		}
            }

	    /* If within region setup Region Structure */
	    /* --------------------------------------- */
	    if (found == 1)
	    {
		/* For all entries in SWXRegion array ... */
		/* -------------------------------------- */
		for (k = 0; k < NSWATHREGN; k++)
		{
		    /* If empty region ... */
		    /* ------------------- */
		    if (SWXRegion[k] == 0)
		    {
			/* Allocate space for region entry */
			/* ------------------------------- */
			SWXRegion[k] = (struct swathRegion *)
			    calloc(1, sizeof(struct swathRegion));
			if(SWXRegion[k] == NULL)
			{ 
			    HEpush(DFE_NOSPACE,"SWdefboxregion", __FILE__, __LINE__);
			    return(-1);
			}

			/* Store file and swath ID */
			/* ----------------------- */
			SWXRegion[k]->fid = fid;
			SWXRegion[k]->swathID = swathID;


			/* Set Start & Stop Vertical arrays to -1 */
			/* -------------------------------------- */
			for (j = 0; j < 8; j++)
			{
			    SWXRegion[k]->StartVertical[j] = -1;
			    SWXRegion[k]->StopVertical[j] = -1;
			    SWXRegion[k]->StartScan[j] = -1;
			    SWXRegion[k]->StopScan[j] = -1;
			}


			/* Set region ID */
			/* ------------- */
			regionID = k;
			break;
		    }
		}
		if (k >= NSWATHREGN)
		  {
		    HEpush(DFE_GENAPP, "SWdefboxregion", __FILE__, __LINE__);
		    HEreport(
			     "regionID exceeded NSWATHREGN.\n");
                    return (-1);
		  }

		/* Find start and stop of regions */
		/* ------------------------------ */

		/* Subtract previous flag value from current one */
		/* --------------------------------------------- */

		/*
		 * Transisition points will have flag value (+1) start or
		 * (255 = (uint8) -1) stop of region
		 */
		for (i = edge[0]; i > 0; i--)
		{
		    flag[i] -= flag[i - 1];
		}


		for (i = 0; i <= edge[0]; i++)
		{
		    /* Start of region */
		    /* --------------- */
		    if (flag[i] == 1)
		    {
			/* Increment (multiple) region counter */
			/* ----------------------------------- */
			j = ++SWXRegion[k]->nRegions;
			
			/* if SWXRegion[k]->nRegions greater than MAXNREGIONS */
			/* free allocated memory and return FAIL */
			
			if ((SWXRegion[k]->nRegions) > MAXNREGIONS)
			{
			    HEpush(DFE_GENAPP, "SWdefboxregion", __FILE__, __LINE__);
			    HEreport("SWXRegion[%d]->nRegions exceeds MAXNREGIONS= %d.\n", k, MAXNREGIONS);
			    free(lonArr);
			    free(latArr);
			    free(flag);
			    return(-1);
			}
			
			SWXRegion[k]->StartRegion[j - 1] = i;
		    }
		    
		    /* End of region */
		    /* ------------- */
		    if (flag[i] == 255)
		    {
			SWXRegion[k]->StopRegion[j - 1] = i - 1;
			validReg = 0;
		    }
		}
	    }
	    free(lonArr);
	    free(latArr);
	    free(flag);
	}
    }
    if(validReg==0)
    {
	return (regionID);
    }
    else
    {
	return (-1);
    }
    
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWregionindex                                                    |
|                                                                             |
|  DESCRIPTION: Finds swath cross tracks within area of interest and returns  |
|               region index and region ID                                    |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  regionID       int32               Region ID                               |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               Swath structure ID                      |
|  cornerlon      float64  dec deg    Longitude of opposite corners of box    |
|  cornerlat      float64  dec deg    Latitude of opposite corners of box     |
|  mode           int32               Search mode                             |
|                                     HDFE_MIDPOINT - Use midpoint of Xtrack  |
|                                     HDFE_ENDPOINT - Use endpoints of Xtrack |
|                                     HDFE_ANYPOINT - Use all points of Xtrack|
|                                                                             |
|  OUTPUTS:                                                                   |
|  geodim	  char		      geolocation track dimension             |
|  idxrange	  int32		      indices of region for along track dim.  |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Oct 96   Joel Gales    Add ability to handle regions crossing date line    |
|  Dec 96   Joel Gales    Add multiple vertical subsetting capability         |
|  Nov 97   Daw           Add multiple vertical subsetting capability         |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
SWregionindex(int32 swathID, float64 cornerlon[], float64 cornerlat[],
	       int32 mode, char *geodim, int32 idxrange[])
{
    intn            i;		/* Loop index */
    intn            j;		/* Loop index */
    intn            k;		/* Loop index */

    intn            l=0;	/* Loop index */
    intn            tmpVal = 0;     /* temp value for start region Delyth Jones*/
  /*intn            j1;  */     /* Loop index */
    intn            status;	/* routine return status variable */
    intn	    mapstatus;	/* status for type of mapping */
    intn            statLon;	/* Status from SWfieldinfo for longitude */
    intn            statLat;	/* Status from SWfieldinfo for latitude */
    intn            statCoLat = -1;	/* Status from SWfieldinfo for
					 * Colatitude */
    intn            statGeodeticLat = -1; /* Status from SWfieldinfo for
					 * GeodeticLatitude */

    uint8           found = 0;	/* Found flag */
    uint8          *flag;	/* Pointer to track flag array */
    intn           validReg = -1; /* -1 is invalid validReg */ 
    
    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           swVgrpID;	/* Swath Vgroup ID */
    int32           rank;	/* Rank of geolocation fields */
    int32           nt;		/* Number type of geolocation fields */
    int32           dims[8];	/* Dimensions of geolocation fields */
    int32           nElem;	/* Number of elements to read */
    int32           bndflag;	/* +/-180 longitude boundary flag */
    int32           lonTest;	/* Longitude test flag */
    int32           latTest;	/* Latitude test flag */
    int32           start[2];	/* Start array (read) */
    int32           stride[2] = {1, 1};	/* Stride array (read) */
    int32           edge[2];	/* Edge array (read) */
    int32           regionID = -1;	/* Region ID (return) */
    int32           anyStart[2];/* ANYPOINT start array (read) */
    int32           anyEdge[2];	/* ANYPOINT edge array (read) */

    float32         temp32;	/* Temporary float32 variable */

    float64         lonTestVal;	/* Longitude test value */
    float64         latTestVal;	/* Latitude test value */
    float64         temp64;	/* Temporary float64 variable */

    char           *lonArr;	/* Longitude data array */
    char           *latArr;	/* Latitude data array */
    char            dimlist[256];	/* Dimension list (geolocation
					 * fields) */
    char            latName[17];/* Latitude field name */


    /* Check for valid swath ID */
    /* ------------------------ */
    status = SWchkswid(swathID, "SWregionindex", &fid, &sdInterfaceID,
		       &swVgrpID);


    /* Inclusion mode must be between 0 and 2 */
    /* -------------------------------------- */
    if (mode < 0 || mode > 2)
    {
	status = -1;
	HEpush(DFE_GENAPP, "SWregionindex", __FILE__, __LINE__);
	HEreport("Improper Inclusion Mode: %d.\n", mode);
    }


    if (status == 0)
    {
	/* Get "Longitude" field info */
	/* -------------------------- */
	statLon = SWfieldinfo(swathID, "Longitude", &rank, dims, &nt, dimlist);
	if (statLon != 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "SWregionindex", __FILE__, __LINE__);
	    HEreport("\"Longitude\" field not found.\n");
	}

	/* Get "Latitude" field info */
	/* -------------------------- */
	statLat = SWfieldinfo(swathID, "Latitude", &rank, dims, &nt, dimlist);
	if (statLat != 0)
	{
	    /* If not found check for "Colatitude" field info */
	    /* ---------------------------------------------- */
	    statCoLat = SWfieldinfo(swathID, "Colatitude", &rank, dims, &nt,
				    dimlist);
	    if (statCoLat != 0)
	    {
	      /* Check again for Geodeticlatitude */            
	      statGeodeticLat = SWfieldinfo(swathID, 
					    "GeodeticLatitude", &rank, 
					    dims, &nt, dimlist);
	      if (statGeodeticLat != 0)
	        {
		  /* Neither "Latitude" nor "Colatitude" field found */
		  /* ----------------------------------------------- */
		  status = -1;
		  HEpush(DFE_GENAPP, "SWregionindex", __FILE__, __LINE__);
		  HEreport(
			   "Neither \"Latitude\" nor \"Colatitude\" fields found.\n");
		}
	      else
		{
		     /* Latitude field is "Colatitude" */
		     /* ------------------------------ */
		     strcpy(latName, "GeodeticLatitude");
		}
	    }
	    else
	    {
		/* Latitude field is "Colatitude" */
		/* ------------------------------ */
		strcpy(latName, "Colatitude");
	    }
	}
	else
	{
	    /* Latitude field is "Latitude" */
	    /* ---------------------------- */
	    strcpy(latName, "Latitude");
	}

        /* This line modifies the dimlist variable so only the along-track */
        /* dimension remains.                                              */
        /* --------------------------------------------------------------- */
        (void) strtok(dimlist,",");
        mapstatus = SWgeomapinfo(swathID,dimlist);
        (void) strcpy(geodim,dimlist);

	if (status == 0)
	{
	    /* Search along entire "Track" dimension from beginning to end */
	    /* ----------------------------------------------------------- */
	    start[0] = 0;
	    edge[0] = dims[0];


	    /* If 1D geolocation fields then set mode to MIDPOINT */
	    /* -------------------------------------------------- */
	    if (rank == 1)
	    {
		mode = HDFE_MIDPOINT;
	    }


	    switch (mode)
	    {
		/* If MIDPOINT search single point in middle of "CrossTrack" */
		/* --------------------------------------------------------- */
	    case HDFE_MIDPOINT:

		start[1] = dims[1] / 2;
		edge[1] = 1;

		break;

		/* If ENDPOINT search 2 points at either end of "CrossTrack" */
		/* --------------------------------------------------------- */
	    case HDFE_ENDPOINT:

		start[1] = 0;
		stride[1] = dims[1] - 1;
		edge[1] = 2;

		break;

		/* If ANYPOINT do initial MIDPOINT search */
		/* -------------------------------------- */
	    case HDFE_ANYPOINT:

		start[1] = dims[1] / 2;
		edge[1] = 1;

		break;
	    }


	    /* Compute number of elements */
	    /* -------------------------- */
	    nElem = edge[0] * edge[1];


	    /* Allocate space for longitude and latitude (float64) */
	    /* --------------------------------------------------- */
	    lonArr = (char *) calloc(nElem, sizeof(float64));
	    if(lonArr == NULL)
	    { 
		HEpush(DFE_NOSPACE,"SWregionindex", __FILE__, __LINE__);
		return(-1);
	    }
	    
	    latArr = (char *) calloc(nElem, sizeof(float64));
	    if(latArr == NULL)
	    { 
		HEpush(DFE_NOSPACE,"SWregionindex", __FILE__, __LINE__);
		free(lonArr);
		return(-1);
	    }


	    /* Allocate space for flag array (uint8) */
	    /* ------------------------------------- */
	    flag = (uint8 *) calloc(edge[0] + 1, 1);
	    if(flag == NULL)
	    { 
		HEpush(DFE_NOSPACE,"SWregionindex", __FILE__, __LINE__);
		free(lonArr);
		free(latArr);
		return(-1);
	    }


	    /* Read Longitude and Latitude fields */
	    /* ---------------------------------- */
	    status = SWreadfield(swathID, "Longitude",
				 start, stride, edge, lonArr);
	    status = SWreadfield(swathID, latName,
				 start, stride, edge, latArr);



	    /*
	     * If geolocation fields are FLOAT32 then cast each entry as
	     * FLOAT64
	     */
	    if (nt == DFNT_FLOAT32)
	    {
		for (i = nElem - 1; i >= 0; i--)
		{
		    memcpy(&temp32, lonArr + 4 * i, 4);
		    temp64 = (float64) temp32;
		    memcpy(lonArr + 8 * i, &temp64, 8);

		    memcpy(&temp32, latArr + 4 * i, 4);
		    temp64 = (float64) temp32;
		    memcpy(latArr + 8 * i, &temp64, 8);
		}
	    }


	    /* Set boundary flag */
	    /* ----------------- */

	    /*
	     * This variable is set to 1 if the region of interest crosses
	     * the +/- 180 longitude boundary
	     */
	    bndflag = (cornerlon[0] < cornerlon[1]) ? 0 : 1;



	    /* Main Search Loop */
	    /* ---------------- */

	    /* For each track ... */
	    /* ------------------ */
	    for (i = 0; i < edge[0]; i++)
	    {
		/* For each value from Cross Track ... */
		/* ----------------------------------- */
		for (j = 0; j < edge[1]; j++)
		{
		    /* Read in single lon & lat values from data buffers */
		    /* ------------------------------------------------- */
		    memcpy(&lonTestVal, &lonArr[8 * (i * edge[1] + j)], 8);
		    memcpy(&latTestVal, &latArr[8 * (i * edge[1] + j)], 8);


		    /* If longitude value > 180 convert to -180 to 180 range */
		    /* ----------------------------------------------------- */
		    if (lonTestVal > 180)
		    {
			lonTestVal = lonTestVal - 360;
		    }

		    /* If Colatitude value convert to latitude value */
		    /* --------------------------------------------- */
		    if (statCoLat == 0)
		    {
			latTestVal = 90 - latTestVal;
		    }


		    /* Test if lat value is within range */
		    /* --------------------------------- */
		    latTest = (latTestVal >= cornerlat[0] &&
			       latTestVal <= cornerlat[1]);


		    if (bndflag == 1)
		    {
			/*
			 * If boundary flag set test whether longitude value
			 * is outside region and then flip
			 */
			lonTest = (lonTestVal >= cornerlon[1] &&
				   lonTestVal <= cornerlon[0]);
			lonTest = 1 - lonTest;
		    }
		    else
		    {
			lonTest = (lonTestVal >= cornerlon[0] &&
				   lonTestVal <= cornerlon[1]);
		    }


		    /*
		     * If both longitude and latitude are within region set
		     * flag on for this track
		     */
		    if (lonTest + latTest == 2)
		    {
			flag[i] = 1;
			found = 1;
			break;
		    }
		}
	    }



	    /* ANYPOINT search */
	    /* --------------- */
	    if (mode == HDFE_ANYPOINT && rank > 1)
	    {
		free(lonArr);
		free(latArr);

		/* Allocate space for an entire single cross track */
		/* ----------------------------------------------- */
		lonArr = (char *) calloc(dims[1], sizeof(float64));
		if(lonArr == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"SWregionindex", __FILE__, __LINE__);
		    return(-1);
		}
		latArr = (char *) calloc(dims[1], sizeof(float64));
		if(latArr == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"SWregionindex", __FILE__, __LINE__);
		    free(lonArr);
		    return(-1);
		}

		/* Setup start and edge */
		/* -------------------- */
		anyStart[1] = 0;
		anyEdge[0] = 1;
		anyEdge[1] = dims[1];


		/* For each track ... */
		/* ------------------ */
		for (i = 0; i < edge[0]; i++)
		{

		    /* If cross track not in region (with MIDPOINT search ... */
		    /* ------------------------------------------------------ */
		    if (flag[i] == 0)
		    {
			/* Setup track start */
			/* ----------------- */
			anyStart[0] = i;


			/* Read in lon and lat values for cross track */
			/* ------------------------------------------ */
			status = SWreadfield(swathID, "Longitude",
					   anyStart, NULL, anyEdge, lonArr);
			status = SWreadfield(swathID, latName,
					   anyStart, NULL, anyEdge, latArr);



			/*
			 * If geolocation fields are FLOAT32 then cast each
			 * entry as FLOAT64
			 */
			if (nt == DFNT_FLOAT32)
			{
			    for (j = dims[1] - 1; j >= 0; j--)
			    {
				memcpy(&temp32, lonArr + 4 * j, 4);
				temp64 = (float64) temp32;
				memcpy(lonArr + 8 * j, &temp64, 8);

				memcpy(&temp32, latArr + 4 * j, 4);
				temp64 = (float64) temp32;
				memcpy(latArr + 8 * j, &temp64, 8);
			    }
			}


			/* For each value from Cross Track ... */
			/* ----------------------------------- */
			for (j = 0; j < dims[1]; j++)
			{
			    /* Read in single lon & lat values from buffers */
			    /* -------------------------------------------- */
			    memcpy(&lonTestVal, &lonArr[8 * j], 8);
			    memcpy(&latTestVal, &latArr[8 * j], 8);


			    /* If lon value > 180 convert to -180 - 180 range */
			    /* ---------------------------------------------- */
			    if (lonTestVal > 180)
			    {
				lonTestVal = lonTestVal - 360;
			    }

			    /* If Colatitude value convert to latitude value */
			    /* --------------------------------------------- */
			    if (statCoLat == 0)
			    {
				latTestVal = 90 - latTestVal;
			    }


			    /* Test if lat value is within range */
			    /* --------------------------------- */
			    latTest = (latTestVal >= cornerlat[0] &&
				       latTestVal <= cornerlat[1]);


			    if (bndflag == 1)
			    {
				/*
				 * If boundary flag set test whether
				 * longitude value is outside region and then
				 * flip
				 */
				lonTest = (lonTestVal >= cornerlon[1] &&
					   lonTestVal <= cornerlon[0]);
				lonTest = 1 - lonTest;
			    }
			    else
			    {
				lonTest = (lonTestVal >= cornerlon[0] &&
					   lonTestVal <= cornerlon[1]);
			    }


			    /*
			     * If both longitude and latitude are within
			     * region set flag on for this track
			     */
			    if (lonTest + latTest == 2)
			    {
				flag[i] = 1;
				found = 1;
				break;
			    }
			}
		    }
		}
	    }
	    /*
	    for (j1 = 0; j1 < edge[0]; j1++)
	      {
		idxrange[j1] = (int32) flag[j1];
	      }
	    */
	    /* If within region setup Region Structure */
	    /* --------------------------------------- */
	    if (found == 1)
	    {
		/* For all entries in SWXRegion array ... */
		/* -------------------------------------- */
		for (k = 0; k < NSWATHREGN; k++)
		{
		    /* If empty region ... */
		    /* ------------------- */
		    if (SWXRegion[k] == 0)
		    {
			/* Allocate space for region entry */
			/* ------------------------------- */
			SWXRegion[k] = (struct swathRegion *)
			    calloc(1, sizeof(struct swathRegion));
			if(SWXRegion[k] == NULL)
			{ 
			    HEpush(DFE_NOSPACE,"SWregionindex", __FILE__, __LINE__);
			    return(-1);
			}

			/* Store file and swath ID */
			/* ----------------------- */
			SWXRegion[k]->fid = fid;
			SWXRegion[k]->swathID = swathID;


			/* Set Start & Stop Vertical arrays to -1 */
			/* -------------------------------------- */
			for (j = 0; j < 8; j++)
			{
			    SWXRegion[k]->StartVertical[j] = -1;
			    SWXRegion[k]->StopVertical[j] = -1;
			    SWXRegion[k]->StartScan[j] = -1;
			    SWXRegion[k]->StopScan[j] = -1;
			}


			/* Set region ID */
			/* ------------- */
			regionID = k;
			break;
		    }
		}
		if (k >= NSWATHREGN)
		  {
		    HEpush(DFE_GENAPP, "SWregionindex", __FILE__, __LINE__);
		    HEreport(
			     "regionID exceeded NSWATHREGN.\n");
                    return (-1);
		  }

		/* Find start and stop of regions */
		/* ------------------------------ */

		/* Subtract previous flag value from current one */
		/* --------------------------------------------- */

		/*
		 * Transisition points will have flag value (+1) start or
		 * (255 = (uint8) -1) stop of region
		 */
		for (i = edge[0]; i > 0; i--)
		{
		    flag[i] -= flag[i - 1];
		}


		for (i = 0; i <= edge[0]; i++)
		{
		    /* Start of region */
		    /* --------------- */
		    if (flag[i] == 1)
		    {
			/* Delyth Jones Moved the increment of the region down
			   to next if statement j = ++SWXRegion[k]->nRegions; */

			/* using temp value, if not equal to stop region
			   invalid region otherwise ok Delyth Jones */
			tmpVal = i+1;
		    }

		    /* End of region */
		    /* ------------- */
		    if (flag[i] == 255)
		    {
			if( tmpVal!=i )
			{
			    /* Increment (multiple) region counter */
			    /* ----------------------------------- */
			    j = ++SWXRegion[k]->nRegions;
			    
                            if (mapstatus == 2)
                            {
                               l = i;
                               if ((tmpVal-1) % 2 == 1)
                               {
                                  tmpVal = tmpVal + 1;
                               }

                               if ((l-1) % 2 == 0)
                               {
                                  l = l - 1;
                               }
                            }
			    SWXRegion[k]->StartRegion[j - 1] = tmpVal-1;
                            idxrange[0] = tmpVal - 1;
			    SWXRegion[k]->StopRegion[j - 1] = l - 1;
                            idxrange[1] = l - 1;
			    validReg = 0;
			}
		    }
		    
		}

	    }
	    free(lonArr);
	    free(latArr);
	    free(flag);
	}
    }
    if(validReg==0)
    {
	return (regionID);
    }
    else
    {
	return (-1);
    }
    
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWdeftimeperiod                                                  |
|                                                                             |
|  DESCRIPTION: Finds swath cross tracks observed during time period and      |
|               returns  period ID                                            |
|                                                                             |
|               region ID                                                     |
|  DESCRIPTION:                                                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  periodID       int32               (Period ID) or (-1) if failed           |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               Swath structure ID                      |
|  starttime      float64 TAI sec     Start of time period                    |
|  stoptime       float64 TAI sec     Stop of time period                     |
|  mode           int32               Search mode                             |
|                                     HDFE_MIDPOINT - Use midpoint of Xtrack  |
|                                     HDFE_ENDPOINT - Use endpoints of Xtrack |
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
SWdeftimeperiod(int32 swathID, float64 starttime, float64 stoptime,
		int32 mode)
{

    intn            i;		    /* Loop index */
    intn            j;		    /* Loop index */
    intn            k = 0;		    /* Loop index */
    intn            status;	    /* routine return status variable */
    intn            statTime;	    /* Status from SWfieldinfo for time */

    uint8           found = 0;	    /* Found flag */

    int32           fid;	    /* HDF-EOS file ID */
    int32           sdInterfaceID;  /* HDF SDS interface ID */
    int32           swVgrpID;	    /* Swath Vgroup ID */
    int32           rank;	    /* Rank of geolocation fields */
    int32           nt;		    /* Number type of geolocation fields */
    int32           dims[8];	    /* Dimensions of geolocation fields */
    int32           start[2];	    /* Start array (read) */
    int32           stride[2] = {1, 1};	/* Stride array (read) */
    int32           edge[2];	    /* Edge array (read) */
    int32           periodID = -1;  /* Period ID (return) */
    int32           dum;	    /* Dummy (loop) variable */

    float64         time64Test;	    /* Time test value */
    float64        *time64 = NULL;  /* Time data array */

    char            dimlist[256];   /* Dimension list (geolocation fields) */

    /* Check for valid swath ID */
    /* ------------------------ */
    status = SWchkswid(swathID, "SWdeftimeperiod", &fid, &sdInterfaceID,
		       &swVgrpID);

    if (status == 0)
    {
	/* Get "Time" field info */
	/* --------------------- */
	statTime = SWfieldinfo(swathID, "Time", &rank, dims, &nt, dimlist);
	if (statTime != 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "SWdeftimeperiod", __FILE__, __LINE__);
	    HEreport("\"Time\" field not found.\n");
	}

	if (status == 0)
	{
	    /* Search along entire "Track" dimension from beginning to end */
	    /* ----------------------------------------------------------- */
	    start[0] = 0;
	    edge[0] = dims[0];


	    /* If 1D geolocation fields then set mode to MIDPOINT */
	    /* -------------------------------------------------- */
	    if (rank == 1)
	    {
		mode = HDFE_MIDPOINT;
	    }


	    switch (mode)
	    {

		/* If MIDPOINT search single point in middle of "CrossTrack" */
		/* --------------------------------------------------------- */
	    case HDFE_MIDPOINT:

		start[1] = dims[1] / 2;
		edge[1] = 1;


		/* Allocate space for time data */
		/* ---------------------------- */
		time64 = (float64 *) calloc(edge[0], 8);
		if(time64 == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"SWdeftimeperiod", __FILE__, __LINE__);
		    return(-1);
		}


		/* Read "Time" field */
		/* ----------------- */
		status = SWreadfield(swathID, "Time",
				     start, NULL, edge, time64);
		break;


		/* If ENDPOINT search 2 points at either end of "CrossTrack" */
		/* --------------------------------------------------------- */
	    case HDFE_ENDPOINT:
		start[1] = 0;
		stride[1] = dims[1] - 1;
		edge[1] = 2;


		/* Allocate space for time data */
		/* ---------------------------- */
		time64 = (float64 *) calloc(edge[0] * 2, 8);
		if(time64 == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"SWdeftimeperiod", __FILE__, __LINE__);
		    return(-1);
		}

		/* Read "Time" field */
		/* ----------------- */
		status = SWreadfield(swathID, "Time",
				     start, stride, edge, time64);
		break;

	    }

            if (time64)
            {
                /* For each track (from top) ... */
                /* ----------------------------- */
                for (i = 0; i < edge[0]; i++)
                {
                    /* For each value from Cross Track ... */
                    /* ----------------------------------- */
                    for (j = 0; j < edge[1]; j++)
                    {

                        /* Get time test value */
                        /* ------------------- */
                        time64Test = time64[i * edge[1] + j];


                        /* If within time period ... */
                        /* ------------------------- */
                        if (time64Test >= starttime &&
                            time64Test <= stoptime)
                        {
                            /* Set found flag */
                            /* -------------- */
                            found = 1;


                            /* For all entries in SWXRegion array ... */
                            /* -------------------------------------- */
                            for (k = 0; k < NSWATHREGN; k++)
                            {
                                /* If empty region ... */
                                /* ------------------- */
                                if (SWXRegion[k] == 0)
                                {
                                    /* Allocate space for region entry */
                                    /* ------------------------------- */
                                    SWXRegion[k] = (struct swathRegion *)
                                        calloc(1, sizeof(struct swathRegion));
                                    if(SWXRegion[k] == NULL)
                                    { 
                                        HEpush(DFE_NOSPACE,"SWdeftimeperiod", __FILE__, __LINE__);
                                        return(-1);
                                    }

                                    /* Store file and swath ID */
                                    /* ----------------------- */
                                    SWXRegion[k]->fid = fid;
                                    SWXRegion[k]->swathID = swathID;


                                    /* Set number of isolated regions to 1 */
                                    /* ----------------------------------- */
                                    SWXRegion[k]->nRegions = 1;


                                    /* Set start of region to first track found */
                                    /* ---------------------------------------- */
                                    SWXRegion[k]->StartRegion[0] = i;


                                    /* Set Start & Stop Vertical arrays to -1 */
                                    /* -------------------------------------- */
                                    for (dum = 0; dum < 8; dum++)
                                    {
                                        SWXRegion[k]->StartVertical[dum] = -1;
                                        SWXRegion[k]->StopVertical[dum] = -1;
                                        SWXRegion[k]->StartScan[dum] = -1;
                                        SWXRegion[k]->StopScan[dum] = -1;
                                    }


                                    /* Set period ID */
                                    /* ------------- */
                                    periodID = k;

                                    break;	/* Break from "k" loop */
                                }
                            }
                        }
                        if (found == 1)
                        {
                            break;	/* Break from "j" loop */
                        }
                    }
                    if (found == 1)
                    {
                        break;	/* Break from "i" loop */
                    }
                }



                /* Clear found flag */
                /* ---------------- */
                found = 0;


                /* For each track (from bottom) ... */
                /* -------------------------------- */
                for (i = edge[0] - 1; i >= 0; i--)
                {
                    /* For each value from Cross Track ... */
                    /* ----------------------------------- */
                    for (j = 0; j < edge[1]; j++)
                    {

                        /* Get time test value */
                        /* ------------------- */
                        time64Test = time64[i * edge[1] + j];


                        /* If within time period ... */
                        /* ------------------------- */
                        if (time64Test >= starttime &&
                            time64Test <= stoptime)
                        {
                            /* Set found flag */
                            /* -------------- */
                            found = 1;

                            /* Set start of region to first track found */
                            /* ---------------------------------------- */
                            SWXRegion[k]->StopRegion[0] = i;

                            break;	/* Break from "j" loop */
                        }
                    }
                    if (found == 1)
                    {
                        break;	/* Break from "i" loop */
                    }
                }

                free(time64);
            }
	}
    }

    return (periodID);
}

/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWextractregion                                                  |
|                                                                             |
|  DESCRIPTION: Retrieves data from specified region.                         |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               Swath structure ID                      |
|  regionID       int32               Region ID                               |
|  fieldname      char                Fieldname                               |
|  externalflag   int32               External geolocation fields flag        |
|                                     HDFE_INTERNAL (0)                       |
|                                     HDFE_EXTERNAL (1)                       |
|                                                                             |
|  OUTPUTS:                                                                   |
|  buffer         void                Data buffer containing subsetted region |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Aug 96   Joel Gales    Add vertical subsetting                             |
|  Oct 96   Joel Gales    Mapping offset value not read from SWmapinfo        |
|  Dec 96   Joel Gales    Vert Subset overwriting data buffer                 |
|  Dec 96   Joel Gales    Add multiple vertical subsetting capability         |
|  Mar 97   Joel Gales    Add support for index mapping                       |
|  Jul 99   DaW 	  Add support for floating scene subsetting 	      |
|  Feb 03   Terry Haran/                                                      |
|           Abe Taaheri   Forced map offset to 0 so that data is extracted    |
|                         without offset consideration. This will preserve    |
|                         original mapping between geofields and the data     |
|                         field.                                              |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
SWextractregion(int32 swathID, int32 regionID, char *fieldname,
		int32 externalflag, VOIDP buffer)

{
    intn            i;		/* Loop index */
    intn            j;		/* Loop index */
    intn            k;		/* Loop index */
    intn            l;		/* Loop index */
    intn            status;	/* routine return status variable */
    intn            long_status = 3;	/* routine return status variable    */
                                        /* for longitude                     */
    intn	    land_status = 3;	/* Used for L7 float scene sub.      */
    intn            statMap = -1;	/* Status from SWmapinfo  */

    uint8           found = 0;		/* Found flag */
    uint8           vfound = 0;		/* Found flag for vertical subsetting*/
                                        /*  --- xhua                         */
    uint8	    scene_cnt = 0;	/* Used for L7 float scene sub.      */
    uint8           detect_cnt = 0;     /* Used to convert scan to scanline  */
                                        /*  L7 float scene sub.              */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           swVgrpID;	/* Swath Vgroup ID */

    int32	    numtype = 0; /* Used for L7 float scene sub. */
    int32	    count = 0;   /* Used for L7 float scene sub. */
    int32           index;	/* Geo Dim Index */
    int32           nDim;	/* Number of dimensions */
    int32           slen[64];	/* String length array */
    int32           dum;	/* Dummy variable */
    int32           offset;	/* Mapping offset */
    int32           incr;	/* Mapping increment */
    int32           nXtrk;	/* Number of cross tracks */
    int32           scan_shift = 0; /* Used to take out partial scans */
    int32           dumdims[8];	/* Dimensions from SWfieldinfo */
    int32           start[8];	/* Start array for data read */
    int32           edge[8];	/* Edge array for data read */
    int32           dims[8];	/* Dimensions */
    int32           rank = 0;	/* Field rank */
    int32           rk = 0;	/* Field rank */
    int32           ntype = 0;	/* Field number type */
    int32           bufOffset;	/* Output buffer offset */
    int32           size;	/* Size of data buffer */
    int32           idxMapElem = -1;	/* Number of index map elements  */
    int32          *idxmap = NULL;	/* Pointer to index mapping array */

    int32	startscanline = 0;
    int32	stopscanline = 0;
    char	*dfieldlist = (char *)NULL;
    int32	strbufsize = 0;
    int32	dfrank[8];
    int32	numtype2[8];
    uint16	*buffer2 = (uint16 *)NULL;
    uint16	*tbuffer = (uint16 *)NULL;
    int32	dims2[8];
    int32	nt = 0;
    int32	startscandim = -1;
    int32	stopscandim = -1;
    int32	rank2 = 0;

    char            dimlist[256];	/* Dimension list */
    char            geodim[256];/* Geolocation field dimension list */
    char            tgeodim[256];/* Time field dimension list */
    char            dgeodim[256];/* Data field dimension list for subsetting */
    char            utlbuf[256];/* Utility buffer */
    char           *ptr[64];	/* String pointer array */



    /* Check for valid swath ID */
    /* ------------------------ */
    status = SWchkswid(swathID, "SWextractregion", &fid, &sdInterfaceID,
		       &swVgrpID);


    /* Check for valid region ID */
    /* ------------------------- */
    if (status == 0)
    {
	if (regionID < 0 || regionID >= NSWATHREGN)
	{
	    status = -1;
	    HEpush(DFE_RANGE, "SWextractregion", __FILE__, __LINE__);
	    HEreport("Invalid Region id: %d.\n", regionID);
	}
    }



    /* Check for active region ID */
    /* -------------------------- */
    if (status == 0)
    {
	if (SWXRegion[regionID] == 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "SWextractregion", __FILE__, __LINE__);
	    HEreport("Inactive Region ID: %d.\n", regionID);
	}
    }


   /* This code checks for the attribute detector_count */
   /* which is found in Landsat 7 files.  It is used    */
   /* for some of the loops.                            */
   /* ================================================= */
   if (SWXRegion[regionID]->scanflag == 1)
   {
      land_status = SWattrinfo(swathID, "detector_count", &numtype, &count);
      if (land_status == 0)
         land_status = SWreadattr(swathID, "detector_count", &detect_cnt);
   }

    /* Check that geo file and data file are same for INTERNAL subsetting */
    /* ------------------------------------------------------------------ */
    if (status == 0)
    {
	if (SWXRegion[regionID]->fid != fid && externalflag != HDFE_EXTERNAL)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "SWextractregion", __FILE__, __LINE__);
	    HEreport("Region is not defined for this file.\n");
	}
    }



    /* Check that geo swath and data swath are same for INTERNAL subsetting */
    /* -------------------------------------------------------------------- */
    if (status == 0)
    {
	if (SWXRegion[regionID]->swathID != swathID &&
	    externalflag != HDFE_EXTERNAL)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "SWextractregion", __FILE__, __LINE__);
	    HEreport("Region is not defined for this Swath.\n");
	}
    }



    /* Check for valid fieldname */
    /* ------------------------- */
    if (status == 0)
    {

	/* Get data field info */
	/* ------------------- */
	status = SWfieldinfo(swathID, fieldname, &rank,
			     dims, &ntype, dimlist);

	if (status != 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "SWextractregion", __FILE__, __LINE__);
	    HEreport("Field \"%s\" Not Found.\n", fieldname);
	}
    }


    /* No problems so proceed ... */
    /* -------------------------- */
    if (status == 0)
    {


	/* Initialize start and edge for all dimensions */
	/* -------------------------------------------- */
	for (j = 0; j < rank; j++)
	{
	    start[j] = 0;
	    edge[j] = dims[j];
	}


	/* Vertical Subset */
	/* --------------- */
	for (j = 0; j < 8; j++)
	{
	    /* If active vertical subset ... */
	    /* ----------------------------- */
	    if (SWXRegion[regionID]->StartVertical[j] != -1)
	    {

		/* Find vertical dimension within dimlist */
		/* -------------------------------------- */
		dum = EHstrwithin(SWXRegion[regionID]->DimNamePtr[j],
				  dimlist, ',');

		/* If dimension found ... */
		/* ---------------------- */
		if (dum != -1)
		{
		    /* Compute start and edge for vertical dimension */
		    /* --------------------------------------------- */
                    vfound = 1;                   /* xhua */
		    start[dum] = SWXRegion[regionID]->StartVertical[j];
		    edge[dum] = SWXRegion[regionID]->StopVertical[j] -
			SWXRegion[regionID]->StartVertical[j] + 1;
		}
		else
		{
		    /* Vertical dimension not found */
		    /* ---------------------------- */
		    status = -1;
		    HEpush(DFE_GENAPP, "SWextractregion", __FILE__, __LINE__);
		    HEreport("Vertical Dimension Not Found: \"%s\".\n",
			     SWXRegion[regionID]->DimNamePtr);
		}
	    }
	} /* End of Vertical Subset loop  */



	/* No problems so proceed ... */
	/* -------------------------- */
	if (status == 0)
	{
	    /* If non-vertical subset regions defined ... */
	    /* ------------------------------------------ */
	    if (SWXRegion[regionID]->nRegions > 0)    
	    {

		/* Get geolocation dimension name */
		/* ------------------------------ */
		status = SWfieldinfo(SWXRegion[regionID]->swathID,
				     "Longitude", &dum,
				     dumdims, &dum, geodim);
                long_status = status;

                /* If Time field being used, check for dimensions */
                /* ---------------------------------------------- */
                if (timeflag == 1)
                {
                   /* code change to fix time subset bug for Landsat7 */

                   status = SWfieldinfo(SWXRegion[regionID]->swathID,
                                        "Time", &dum,
                                        dumdims, &dum, tgeodim);
                   
                   if (strcmp(geodim, tgeodim) != 0)
                   {
                      strcpy(geodim, tgeodim);
                   }
                }
                timeflag = 0;

                /* If defscanregion being used, get dimensions    */
                /* of field being used                            */
                /* ---------------------------------------------- */
                if (SWXRegion[regionID]->scanflag == 1)
                {
                   (void) SWnentries(SWXRegion[regionID]->swathID,4,&strbufsize);
                   dfieldlist = (char *)calloc(strbufsize + 1, sizeof(char));
                   (void) SWinqdatafields(SWXRegion[regionID]->swathID,dfieldlist,dfrank,numtype2);
                   status = SWfieldinfo(SWXRegion[regionID]->swathID,dfieldlist,&dum,dumdims,&dum,dgeodim);

                   /* The dimensions have to be switched, because */
                   /* the mappings force a geodim and datadim     */
                   /* so to find the mapping, the dimensions must */
                   /* be switched, but the subsetting will still  */
                   /* be based on the correct dimensions          */
                   /* ------------------------------------------- */
                   if (strcmp(dgeodim,dimlist) != 0 || long_status == -1)
                   {
                      strcpy(geodim,dimlist);
                      strcpy(dimlist,dgeodim);
                   }
                }


		/* Get "Track" (first) Dimension from geo dimlist */
		/* ---------------------------------------------- */
		nDim = EHparsestr(geodim, ',', ptr, slen);
		geodim[slen[0]] = 0;


		/* Parse Data Field Dimlist & find mapping */
		/* --------------------------------------- */
		nDim = EHparsestr(dimlist, ',', ptr, slen);


		/* Loop through all dimensions and search for mapping */
		/* -------------------------------------------------- */
		for (i = 0; i < nDim; i++)
		{
		    memcpy(utlbuf, ptr[i], slen[i]);
		    utlbuf[slen[i]] = 0;
		    statMap = SWmapinfo(swathID, geodim, utlbuf,
					&offset, &incr);


                    /*
                     *  Force offset to 0.
                     *  We're not changing the mapping, so we want
                     *  the original offset to apply to the subsetted data.
                     *  Otherwise, bad things happen, such as subsetting
                     *  past the end of the original data, and being unable
                     *  to read the first <offset> elements of the
                     *  original data.
                     *  The offset is only important for aligning the
                     *  data with interpolated (incr > 0) or decimated
                     *  (incr < 0) geolocation information for the data.
                     */

                    offset = 0;


		    /* Mapping found */
		    /* ------------- */
		    if (statMap == 0)
		    {
			found = 1;
			index = i;
			break;
		    }
		}


		/* If mapping not found check for geodim within dimlist */
		/* ---------------------------------------------------- */
		if (found == 0)
		{
		    index = EHstrwithin(geodim, dimlist, ',');

		    /* Geo dimension found within subset field dimlist */
		    /* ----------------------------------------------- */
		    if (index != -1)
		    {
			found = 1;
			offset = 0;
			incr = 1;
		    }
		}



		/* If mapping not found check for indexed mapping */
		/* ---------------------------------------------- */
		if (found == 0)
		{
		    /* Get size of geo dim & allocate space of index mapping */
		    /* ----------------------------------------------------- */
		    dum = SWdiminfo(swathID, geodim);

                    /* For Landsat files, the index mapping has two values   */
                    /* for each point, a left and right point.  So for a 37  */
                    /* scene band file there are 2x2 points for each scene   */
                    /* meaning, 2x2x37 = 148 values.  The above function     */
                    /* only returns the number of values in the track        */
                    /* dimension.                                            */
                    /* ----------------------------------------------------- */
                    if(land_status == 0)
                       if(strcmp(fieldname, "Latitude") == 0 || 
                          strcmp(fieldname, "Longitude") == 0)
                       {
                          dum = dum * 2;
                       }
		    idxmap = (int32 *) calloc(dum, sizeof(int32));
		    if(idxmap == NULL)
		    { 
			HEpush(DFE_NOSPACE,"SWextractregion", __FILE__, __LINE__);
			return(-1);
		    }

		    /* Loop through all dimensions and search for mapping */
		    /* -------------------------------------------------- */
		    for (i = 0; i < nDim; i++)
		    {
			memcpy(utlbuf, ptr[i], slen[i]);
			utlbuf[slen[i]] = 0;

			idxMapElem =
			    SWidxmapinfo(swathID, geodim, utlbuf, idxmap);


			/* Mapping found */
			/* ------------- */
			if (idxMapElem != -1)
			{
			    found = 1;
			    index = i;
			    break;
			}
		    }
		}


		/* If regular mapping found ... */
		/* ---------------------------- */
		if (found == 1 && idxMapElem == -1)
		{
		    for (k = 0; k < SWXRegion[regionID]->nRegions; k++)
		    {
			if (k > 0)
			{
			    /* Compute size in bytes of previous region */
			    /* ---------------------------------------- */
			    size = edge[0];
			    for (j = 1; j < rank; j++)
			    {
				size *= edge[j];
			    }
			    size *= DFKNTsize(ntype);


			    /* Compute output buffer offset */
			    /* ---------------------------- */
			    bufOffset += size;
			}
			else
			{
			    /* Initialize output buffer offset */
			    /* ------------------------------- */
			    bufOffset = 0;
			}


			/* Compute number of cross tracks in region */
			/* ---------------------------------------- */
			nXtrk = SWXRegion[regionID]->StopRegion[k] -
			    SWXRegion[regionID]->StartRegion[k] + 1;


			/* Positive increment (geodim <= datadim) */
			/* -------------------------------------- */
			if (incr > 0)
			{
                           if (SWXRegion[regionID]->scanflag == 1)
                           {
                              start[index] = SWXRegion[regionID]->StartRegion[k]/incr;
                              if(SWXRegion[regionID]->band8flag == 2 || 
                                 SWXRegion[regionID]->band8flag == 3)
                              {
                                 start[index] = (SWXRegion[regionID]->StartRegion[k]+detect_cnt)/incr;
                                 status = SWfieldinfo(SWXRegion[regionID]->swathID,"scan_no",&rank,dims2,&nt,dimlist);
                                 buffer2 = (uint16 *)calloc(dims2[0], sizeof(uint16));
                                 status = SWreadfield(SWXRegion[regionID]->swathID,"scan_no",NULL,NULL,NULL,buffer2);
                                 if(incr == 1)
                                    start[index] = start[index] - (buffer2[0] * detect_cnt);
                                 else
                                    start[index] = start[index] - buffer2[0];
                                 free(buffer2);
                              }
                              scan_shift = nXtrk % incr;
                              if(scan_shift != 0)
                                 nXtrk = nXtrk - scan_shift;
			      edge[index] = nXtrk / incr;
                              if (nXtrk % incr != 0)
                                 edge[index]++;
                              if(long_status == -1 || incr == 1)
                              {
                                 scan_shift = nXtrk % detect_cnt;
                                 if(scan_shift != 0)
                                    edge[index] = nXtrk - scan_shift;
                              }

                           }
                           else
                           {
			      start[index] = SWXRegion[regionID]->StartRegion[k] * incr + offset;
			      edge[index] = nXtrk * incr - offset;
                           }
			}
			else
			{
			    /* Negative increment (geodim > datadim) */
			    /* ------------------------------------- */
			    start[index] = SWXRegion[regionID]->StartRegion[k]
				/ (-incr) + offset;
			    edge[index] = nXtrk / (-incr);

			    /*
			     * If Xtrk not exactly divisible by incr, round
			     * edge to next highest integer
			     */

			    if (nXtrk % (-incr) != 0)
			    {
				edge[index]++;
			    }
			}


			/* Read Data into output buffer */
			/* ---------------------------- */
			status = SWreadfield(swathID, fieldname,
					     start, NULL, edge,
					     (uint8 *) buffer + bufOffset);
		    }
		}
		else if (found == 1 && idxMapElem != -1)
		{
		    /* Indexed Mapping */
		    /* --------------- */
		    for (k = 0; k < SWXRegion[regionID]->nRegions; k++)
		    {
			if (k > 0)
			{
			    /* Compute size in bytes of previous region */
			    /* ---------------------------------------- */
			    size = edge[0];
			    for (j = 1; j < rank; j++)
			    {
				size *= edge[j];
			    }
			    size *= DFKNTsize(ntype);


			    /* Compute output buffer offset */
			    /* ---------------------------- */
			    bufOffset += size;
			}
			else
			{
			    /* Initialize output buffer offset */
			    /* ------------------------------- */
			    bufOffset = 0;
			}


			/* Compute start & edge from index mappings */
			/* ---------------------------------------- */
                        if (SWXRegion[regionID]->scanflag == 1 &&
                            (strcmp(fieldname, "Latitude") == 0 ||
                             strcmp(fieldname, "Longitude") == 0))
                        {
                           if (land_status == 0)
                              status = SWreadattr(swathID, "scene_count", &scene_cnt);
                           startscanline = SWXRegion[regionID]->StartRegion[k];
                           stopscanline = SWXRegion[regionID]->StopRegion[k];
                           if(SWXRegion[regionID]->band8flag == 2 || SWXRegion[regionID]->band8flag == 3)
                           {
                               status = SWfieldinfo(swathID,"scan_no",&rk,dims2,&nt,dimlist);
                               tbuffer = (uint16 *)calloc(dims2[0], sizeof(uint16));
                               status =SWreadfield(swathID,"scan_no",NULL,NULL,NULL,tbuffer);
                               startscanline = startscanline - ((tbuffer[0] * detect_cnt) - detect_cnt);
                               stopscanline = stopscanline - ((tbuffer[0] * detect_cnt) - 1);
                           }
                           if(SWXRegion[regionID]->band8flag == 2 ||
                              SWXRegion[regionID]->band8flag == 3)
                           {
                              if(startscandim == -1)
                                 if(startscanline < idxmap[0])
                                 {
                                    startscandim = 0;
                                    start[index] = 0;
                                    if(stopscanline > idxmap[scene_cnt * 2 - 1])
                                    {
                                       stopscandim = scene_cnt*2 - startscandim;
                                       edge[index] = scene_cnt*2 - startscandim;
                                    }
                                 }
                           }
                           j = 0;
                           for (l = 0; l < scene_cnt; l++)
                           {
                              if(idxmap[j] <= startscanline && idxmap[j+1] >= startscanline)
                                 if(startscandim == -1)
                                 {
                                    start[index] = j;
                                    startscandim = j;
                                 }
                              if(idxmap[j] <= stopscanline && idxmap[j+1] >= stopscanline)
                                 if(startscandim != -1)
                                 {
                                    edge[index] = j - start[index] + 2;
                                    stopscandim = j - start[index] + 1;
                                 }
                              j = j + 2;
                           }
                           if(SWXRegion[regionID]->band8flag == 1 ||
                              SWXRegion[regionID]->band8flag == 2)
                           {
                              if(startscandim == -1)
                                 if(startscanline < idxmap[0])
                                 {
                                    startscandim = 0;
                                    start[index] = 0;
                                 }
                              if(stopscandim == -1)
                                 if(stopscanline > idxmap[scene_cnt * 2 - 1])
                                 {
                                    stopscandim = scene_cnt*2 - start[index];
                                    edge[index] = scene_cnt*2 - start[index];
                                 }
                           }
                           if(SWXRegion[regionID]->band8flag == 2)
                           {
                              if(startscandim == -1)
                                 if(startscanline > idxmap[j - 1])
                                 {
                                    status = SWfieldinfo(SWXRegion[regionID]->swathID,"scan_no",&rank2,dims2,&nt,dimlist);
                                    buffer2 = (uint16 *)calloc(dims2[0], sizeof(uint16));
                                    status = SWreadfield(SWXRegion[regionID]->swathID,"scan_no",NULL,NULL,NULL,buffer2);
                                    startscanline = startscanline - (buffer2[0] * detect_cnt);
                                    stopscanline = stopscanline - (buffer2[0] * detect_cnt);
                                    free(buffer2);
                                    j = 0;
                                    for (l = 0; l < scene_cnt; l++)
                                    {
                                       if(idxmap[j] <= startscanline && idxmap[j+1] >= startscanline)
                                       {
                                          start[index] = j;
                                       }
                                       if(idxmap[j] <= stopscanline && idxmap[j+1] >= stopscanline)
                                          edge[index] = j - start[index] + 2;
                                       j = j + 2;
                                       if(idxmap[j] == 0  || idxmap[j+1] == 0)
                                          l = scene_cnt;
                                    }
 
                                 }
                           }

                        }
                        else if(SWXRegion[regionID]->scanflag == 1 &&
                                (strcmp(fieldname, "scene_center_latitude") == 0 ||
                                 strcmp(fieldname, "scene_center_longitude") == 0))
                        {
                           if (land_status == 0)
                              status = SWreadattr(swathID, "scene_count", &scene_cnt);
                           startscanline = SWXRegion[regionID]->StartRegion[k];
                           stopscanline = SWXRegion[regionID]->StopRegion[k];
                           if(startscanline < idxmap[0])
                           {
                              startscandim = 0;
                              start[index] = 0;
                           }
                           for (l = 0; l < scene_cnt-1; l++)
                           {
                              if(idxmap[l] <= startscanline && idxmap[l+1] >= startscanline)
                                 if(startscandim == -1)
                                 {
                                    start[index] = l;
                                    startscandim = l;
                                 }
                              if(idxmap[l] <= stopscanline && idxmap[l+1] >= stopscanline)
                                 if(stopscandim == -1)
                                 {
                                    edge[index] = l - start[index] + 2;
                                    stopscandim = l + 1;
                                 }
                            }
                            if(stopscandim == -1)
                            {
                               if(stopscanline > idxmap[scene_cnt - 1])
                               {
                                  edge[index] = scene_cnt - start[index];
                                  stopscandim = scene_cnt - 1;
                               }
                            }

                            if(SWXRegion[regionID]->band8flag == 1)
                            {
                               if(stopscandim == -1)
                                  if(stopscanline > idxmap[scene_cnt - 1])
                                  {
                                     edge[index] = scene_cnt - start[index];
                                     stopscandim = scene_cnt -1;
                                  }
                            }
                            if(SWXRegion[regionID]->band8flag == 2 ||
                               SWXRegion[regionID]->band8flag == 3)
                            {
                               if(startscandim == -1)
                               {
                                  if(startscanline < idxmap[0])
                                  {
                                     startscandim = 0;
                                     start[index] = 0;
                                     edge[index] = stopscandim - startscandim + 1;
                                  }
                               }
                               if(startscandim == -1)
                               {
                                  startscanline = SWXRegion[regionID]->StartScan[k] * detect_cnt - detect_cnt;
                                  stopscanline = SWXRegion[regionID]->StopScan[k] * detect_cnt - 1;
                                  for (l = 0; l < scene_cnt-1; l++)
                                  {
                                     if(idxmap[l] <= startscanline && idxmap[l+1] >= startscanline)
                                        start[index] = l;
                                     if(idxmap[l] <= stopscanline && idxmap[l+1] >= stopscanline)
                                        edge[index] = l - start[index] + 1;
                                  }
                               }
                           }
                        }
                        else
                        {
                           if (SWXRegion[regionID]->scanflag == 1 && 
                               strcmp(fieldname,dfieldlist) == 0)
                           {
                              start[index] = SWXRegion[regionID]->StartRegion[k];
                              edge[index] = SWXRegion[regionID]->StopRegion[k] - 
                                             SWXRegion[regionID]->StartRegion[k] + 1;
                              if(SWXRegion[regionID]->band8flag == 2 ||
				 SWXRegion[regionID]->band8flag == 3 )
                              {
                                 status = SWfieldinfo(SWXRegion[regionID]->swathID,"scan_no",&rank,dims2,&nt,dimlist);
                                 buffer2 = (uint16 *)calloc(dims2[0], sizeof(uint16));
                                 status = SWreadfield(SWXRegion[regionID]->swathID,"scan_no",NULL,NULL,NULL,buffer2);
                                 start[index] = start[index] - (buffer2[0] * detect_cnt - detect_cnt);
                                 free(buffer2);
                              }
                           }
                           else
                           {
			      start[index] = idxmap[SWXRegion[regionID]->StartRegion[k]];

			      edge[index] = idxmap[SWXRegion[regionID]->StopRegion[k]] -
			                 idxmap[SWXRegion[regionID]->StartRegion[k]] + 1;
                           }
                        }
			/* Read Data into output buffer */
			/* ---------------------------- */
			status = SWreadfield(swathID, fieldname,
					     start, NULL, edge,
					     buffer);
                        if (SWXRegion[regionID]->scanflag == 1)
                        {
                           
                           if (strcmp(fieldname,"Longitude") == 0)
                           {
                              status = SWscan2longlat(swathID, fieldname, buffer, start, 
                                              edge, idxmap, startscanline, stopscanline);
                           }
                           if (strcmp(fieldname,"Latitude") == 0)
                           {
                              status = SWscan2longlat(swathID, fieldname, buffer, start, 
                                              edge, idxmap, startscanline, stopscanline);
                           }
                        }
		    }
		}
		else if(vfound == 1)                          /* Vertical subsetting */
                {                                             /* found previously,   */
                   status = SWreadfield(swathID, fieldname,   /* perform the vertical*/
                                        start, NULL, edge,    /* subsetting.         */
                                        (uint8 *) buffer);    /* -- xhua             */
                }
                else
		{
		    /* Mapping not found */
		    /* ----------------- */
		    status = -1; 
		    HEpush(DFE_GENAPP, "SWextractregion", __FILE__, __LINE__);
		    HEreport("Mapping Not Defined for \"%s\" Dimension.\n",
			     geodim);
		}                                           
	    }
	    else
	    {
		/* Read Data (Vert SS only) */
		/* ------------------------ */
		status = SWreadfield(swathID, fieldname,
				     start, NULL, edge,
				     (uint8 *) buffer);
	    }
	}
    }

    /* Free index mappings if applicable */
    /* --------------------------------- */
    if (idxmap != NULL)
    {
	free(idxmap);
    }
    if(dfieldlist != NULL)
       free(dfieldlist);

    return (status);
}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWscan2longlat                                                   |
|                                                                             |
|  DESCRIPTION:  Convert scanline to Long/Lat for floating scene subsetting.  |
|                This will calculate/interpolate the long/lat for a given     |
|                scanline.                                                    |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               Swath structure ID                      |
|  fieldname      char                Fieldname                               |
|  buffer	  void		      Values to update                        |
|  start	  int32		                                              |
|  edge		  int32							      |
|  idxmap	  int32 *	      Buffer of index mapping values          |
|  startscanline  int32		      Start of scan region		      |
|  stopscanline   int32		      Stop of scan region		      |
|                                                                             |
|  OUTPUTS:                                                                   |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jul 99   DaW           Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
static intn
SWscan2longlat(int32 swathID, char *fieldname, VOIDP buffer, int32 start[], 
int32 edge[], int32 *idxmap, int32 startscanline, int32 stopscanline)
{

   enum corner {UL, UR, LL, LR};
   enum corner pos = UL;
   enum corner pos2 = UL;

   uint8	scene_cnt = 0;		/* Used to convert scan to scanline  */
                                        /*  L7 float scene sub.              */
   float32	*buffer2;
   float32	*bufferc;
   float32	deg2rad = PI/180.00;

   float32	p1_long = 0.0;	/* point 1, longitude */
   float32	p2_long = 0.0;	/* point 2, longitude */
   float32	pi_long = 0.0;	/* interpolated point, longitude */
   int32	scanline_p1 = 0;

   float32	p1_lat = 0.0;	/* point 1, latitude */
   float32	p2_lat = 0.0;	/* point 2, latitude */
   float32	pi_lat = 0.0;	/* interpolated point, latitude */
   int32	scanline_p2 = 0;


   float32	x_p1 = 0.0;		/* Cartesian coordinates */
   float32	y_p1 = 0.0;		/* point 1               */
   float32	z_p1 = 0.0;

   float32	x_p2 = 0.0;		/* Cartesian coordinates */
   float32	y_p2 = 0.0;		/* point 2		 */
   float32	z_p2 = 0.0;

   float32	x_pi = 0.0;		/* Cartesian coordinates */
   float32	y_pi = 0.0;		/* interpolated point    */
   float32	z_pi = 0.0;
   int32	scanline_pi = 0;

   intn		status = -1;

   int		i = 0;
   int		p1_long_l90_flag = 0;
   int		p1_long_g90_flag = 0;
   int		p2_long_l90_flag = 0;
   int		p2_long_g90_flag = 0;
   int		fieldflag = 0;

   int	numofval = 0;



   numofval = edge[0] * 2;


   buffer2 = (float32 *)calloc(numofval, sizeof(float32));
   bufferc = (float32 *)calloc(numofval, sizeof(float32));
   memmove(bufferc, buffer, numofval*sizeof(float32));

   (void) SWreadattr(swathID, "scene_count", &scene_cnt);

   if (strcmp(fieldname, "Longitude") == 0)
   {
      fieldflag = 1;
      status = SWreadfield(swathID, "Latitude", start, NULL, edge, buffer2);
   }
   else if (strcmp(fieldname, "Latitude") == 0)
   {
      fieldflag = 2;
      status = SWreadfield(swathID, "Longitude", start, NULL, edge, buffer2);
   }

   for(i=0; i<4; i++)
   {
      switch(pos)
      {
         case UL:
            if (fieldflag == 1)
            {
               p1_long = bufferc[0];
               p2_long = bufferc[2];
               p1_lat = buffer2[0];
               p2_lat = buffer2[2];
            }
            if (fieldflag == 2)
            {
               p1_long = buffer2[0];
               p2_long = buffer2[2];
               p1_lat = bufferc[0];
               p2_lat = bufferc[2];
            }
            scanline_p1 = idxmap[start[0]];
            scanline_p2 = idxmap[start[0]+1];
            scanline_pi = startscanline;
            pos = UR;
            break;
         case UR:
            if (fieldflag == 1)
            {
               p1_long = bufferc[1];
               p2_long = bufferc[3];
               p1_lat = buffer2[1];
               p2_lat = buffer2[3];
            }
            if (fieldflag == 2)
            {
               p1_long = buffer2[1];
               p2_long = buffer2[3];
               p1_lat = bufferc[1];
               p2_lat = bufferc[3];
            }
            scanline_p1 = idxmap[start[0]];
            scanline_p2 = idxmap[start[0]+1];
            scanline_pi = startscanline;
            pos = LL;
            break;
         case LL:
            if (fieldflag == 1)
            {
               p1_long = bufferc[numofval-4];
               p2_long = bufferc[numofval-2];
               p1_lat = buffer2[numofval-4];
               p2_lat = buffer2[numofval-2];
            }
            if (fieldflag == 2)
            {
               p1_long = buffer2[numofval-4];
               p2_long = buffer2[numofval-2];
               p1_lat = bufferc[numofval-4];
               p2_lat = bufferc[numofval-2];
            }
            scanline_p1 = idxmap[start[0] + edge[0] - 2];
            scanline_p2 = idxmap[start[0] + edge[0] - 1];
            scanline_pi = stopscanline;
            pos = LR;
            break;
         case LR:
            if (fieldflag == 1)
            {
               p1_long = bufferc[numofval-3];
               p2_long = bufferc[numofval-1];
               p1_lat = buffer2[numofval-3];
               p2_lat = buffer2[numofval-1];
            }
            if (fieldflag == 2)
            {
               p1_long = buffer2[numofval-3];
               p2_long = buffer2[numofval-1];
               p1_lat = bufferc[numofval-3];
               p2_lat = bufferc[numofval-1];
            }
            scanline_p1 = idxmap[start[0] + edge[0] - 2];
            scanline_p2 = idxmap[start[0] + edge[0] - 1];
            scanline_pi = stopscanline;
            break;
      }



   if (p1_long <= -90.0)
   {
      if (p2_long >= 90.0)
      {
         p1_long = p1_long + 180.0;
         p2_long = p2_long - 180.0;
         p1_long_l90_flag = 2;
      }
      else
      {
         p1_long = p1_long + 180.0;
         p1_long_l90_flag = 1;
      }
   }
   if (p1_long >= 90.0 && p1_long_l90_flag != 2)
   {
      if(p2_long <= -90.0)
      {
         p1_long = p1_long - 180.0;
         p2_long = p2_long + 180.0;
         p1_long_g90_flag = 2;
      }
      else
      {
         p1_long = p1_long - 90.0;
         p1_long_g90_flag = 1;
      }
   }
   if (p2_long <= -90.0)
   {
      if (p1_long < 0.0)
      {
         p2_long = p2_long + 90.0;
         p1_long = p1_long + 90.0;
         p2_long_l90_flag = 2;
      }
      else
      {
         p2_long = p2_long + 180.0;
         p2_long_l90_flag = 1;
      }
   }
   if (p2_long >= 90.0 && p1_long_l90_flag != 2)
   {
      p2_long = p2_long - 90.0;
      p2_long_g90_flag = 1;
   }


   x_p1 = RADOE * cos((p1_long*deg2rad)) * sin((p1_lat*deg2rad));
   y_p1 = RADOE * sin((p1_long*deg2rad)) * sin((p1_lat*deg2rad));
   z_p1 = RADOE * cos((p1_lat*deg2rad));

   
   x_p2 = RADOE * cos((p2_long*deg2rad)) * sin((p2_lat*deg2rad));
   y_p2 = RADOE * sin((p2_long*deg2rad)) * sin((p2_lat*deg2rad));
   z_p2 = RADOE * cos((p2_lat*deg2rad));

   x_pi = x_p1 + (x_p2 - x_p1)*(scanline_pi-scanline_p1)/(scanline_p2-scanline_p1);
   y_pi = y_p1 + (y_p2 - y_p1)*(scanline_pi-scanline_p1)/(scanline_p2-scanline_p1);
   z_pi = z_p1 + (z_p2 - z_p1)*(scanline_pi-scanline_p1)/(scanline_p2-scanline_p1); 

   if (fieldflag == 1)
   {
      pi_long = atan(y_pi/x_pi)*180.0/PI;
      if (p1_long_l90_flag == 1 || p2_long_l90_flag == 1)
      {
         pi_long = pi_long - 180.0;
         p1_long_l90_flag = 0;
	 p2_long_l90_flag = 0;
      }
      if (p1_long_g90_flag == 1 || p2_long_g90_flag == 1)
      {
         pi_long = pi_long + 90.0;
         p1_long_g90_flag = 0;
         p2_long_g90_flag = 0;
      }
      if (p1_long_l90_flag == 2)
      {
         if (pi_long > 0.0)
            pi_long = pi_long - 180.0;
         else if (pi_long < 0.0)
            pi_long = pi_long + 180.0;
         p1_long_l90_flag = 0;
      }
      if (p1_long_g90_flag == 2)
      {
         if (pi_long > 0.0)
            pi_long = pi_long - 180.0;
         else if (pi_long < 0.0)
            pi_long = pi_long + 180.0;
         p1_long_g90_flag = 0;
      }
      if (p2_long_l90_flag == 2)
      {
         pi_long = pi_long - 90.0;
         p2_long_l90_flag = 0;
      }



      switch(pos2)
      {
      case UL:
         bufferc[0] = pi_long;
         pos2 = UR;
         break;
      case UR:
         bufferc[1] = pi_long;
         pos2 = LL;
         break;
      case LL:
         if(stopscanline > idxmap[scene_cnt*2 - 1])
            break;
         bufferc[numofval-2] = pi_long;
         pos2 = LR;
         break;
      case LR:
         if(stopscanline > idxmap[scene_cnt*2 - 1])
            break;
         bufferc[numofval-1] = pi_long;
         break;
      }

    }
    if (fieldflag == 2)
    {
      pi_lat = atan((sqrt(x_pi*x_pi + y_pi*y_pi)/z_pi))*180.0/PI;
      switch(pos2)
      {
      case UL:
         bufferc[0] = pi_lat;
         pos2 = UR;
         break;
      case UR:
         bufferc[1] = pi_lat;
         pos2 = LL;
         break;
      case LL:
         if(stopscanline > idxmap[scene_cnt*2 - 1])
            break;
         bufferc[numofval-2] = pi_lat;
         pos2 = LR;
         break;
      case LR:
         if(stopscanline > idxmap[scene_cnt*2 - 1])
            break;
         bufferc[numofval-1] = pi_lat;
         break;
      }
   }
   }
   memmove(buffer, bufferc, numofval*sizeof(float32));
   free(buffer2);
   free(bufferc);
   return(status);
}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWextractperiod                                                  |
|                                                                             |
|  DESCRIPTION: Retrieves data from specified period.                         |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               Swath structure ID                      |
|  periodID       int32               Period ID                               |
|  fieldname      char                Fieldname                               |
|  externalflag   int32               External geolocation fields flag        |
|                                     HDFE_INTERNAL (0)                       |
|                                     HDFE_EXTERNAL (1)                       |
|                                                                             |
|  OUTPUTS:                                                                   |
|  buffer         void                Data buffer containing subsetted region |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Jun 03   Abe Taaheri   added clearing timeflag if SWextractregion failes   |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
SWextractperiod(int32 swathID, int32 periodID, char *fieldname,
		int32 externalflag, VOIDP buffer)

{
    intn            status;	/* routine return status variable */

    timeflag = 1;

    /* Call SWextractregion routine */
    /* ---------------------------- */
    status = SWextractregion(swathID, periodID, fieldname, externalflag,
			     (char *) buffer);
    if (status != 0) timeflag = 0; /*clear timeflag if SWextractregion failed*/
    return (status);
}







/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWdupregion                                                      |
|                                                                             |
|  DESCRIPTION: Duplicates a region                                           |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  newregionID    int32               New region ID                           |
|                                                                             |
|  INPUTS:                                                                    |
|  oldregionID    int32               Old region ID                           |
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jan 97   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
SWdupregion(int32 oldregionID)
{
    intn            i;		/* Loop index */

    int32           newregionID = -1;	/* New region ID */


    /* Find first empty (inactive) region */
    /* ---------------------------------- */
    for (i = 0; i < NSWATHREGN; i++)
    {
	if (SWXRegion[i] == 0)
	{
	    /* Allocate space for new swath region entry */
	    /* ----------------------------------------- */
	    SWXRegion[i] = (struct swathRegion *)
		calloc(1, sizeof(struct swathRegion));
	    if(SWXRegion[i] == NULL)
	    { 
		HEpush(DFE_NOSPACE,"SWdupregion", __FILE__, __LINE__);
		return(-1);
	    }

	    /* Copy old region structure data to new region */
	    /* -------------------------------------------- */
	    *SWXRegion[i] = *SWXRegion[oldregionID];


	    /* Define new region ID */
	    /* -------------------- */
	    newregionID = i;

	    break;
	}
    }

    return (newregionID);
}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWregioninfo                                                     |
|                                                                             |
|  DESCRIPTION: Returns size of region in bytes                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               Swath structure ID                      |
|  regionID       int32               Region ID                               |
|  fieldname      char                Fieldname                               |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|  ntype          int32               field number type                       |
|  rank           int32               field rank                              |
|  dims           int32               dimensions of field region              |
|  size           int32               size in bytes of field region           |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Aug 96   Joel Gales    Add vertical subsetting                             |
|  Dec 96   Joel Gales    Add multiple vertical subsetting capability         |
|  Mar 97   Joel Gales    Add support for index mapping                       |
|  Jul 99   DaW           Add support for floating scene subsetting           |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
SWregioninfo(int32 swathID, int32 regionID, char *fieldname,
	     int32 * ntype, int32 * rank, int32 dims[], int32 * size)

{
    intn            i;		/* Loop index */
    intn            j;		/* Loop index */
    intn            k;		/* Loop index */
    intn            l = 0;		/* Loop index */
    intn            status;	/* routine return status variable */
    intn            long_status = 3;	/* routine return status variable for longitude */
    intn            land_status = 3;    /* Used for L7 float scene sub.      */
    intn            statMap = -1;   /* Status from SWmapinfo  */
    
    uint8           found = 0;	/* Found flag */
    uint8           detect_cnt = 0;	/* Used for L7 float scene sub.      */

    int32           numtype = 0; /* Used for L7 float scene sub. */
    int32           count = 0;   /* Used for L7 float scene sub. */


    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           swVgrpID;	/* Swath Vgroup ID */

    int32           index;	/* Geo Dim Index */
    int32           nDim;	/* Number of dimensions */
    int32           slen[64];	/* String length array */
    int32           dum;	/* Dummy variable */
    int32           incr;	/* Mapping increment */
    int32           nXtrk = 0;	/* Number of cross tracks */
    int32	    scan_shift = 0; /* Used to take out partial scans */
    int32	    startscandim = -1;    /* Used for floating scene region size */
    int32	    stopscandim = -1;    /* Used for floating scene region size */
    int32           dumdims[8];	/* Dimensions from SWfieldinfo */
    int32           idxMapElem = -1;	/* Number of index map elements  */
    int32          *idxmap = NULL;	/* Pointer to index mapping array */
    int32	    datafld = 0;

   uint8	scene_cnt = 0;		/* Number of scenes in swath */
   int32	startscanline = 0;
   int32	stopscanline = 0;
   char		*dfieldlist = (char *)NULL;
   int32	strbufsize = 0;
   int32	dfrank[8];
   int32	numtype2[8];
   int32	rank2 = 0;
   int32	rk = 0;
   int32	dims2[8];
   int32	nt = 0;
   uint16	*buffer2 = (uint16 *)NULL;
   uint16	*tbuffer = (uint16 *)NULL;

    char            dimlist[256];	/* Dimension list */
    char            geodim[256];/* Geolocation field dimension list */
    char            tgeodim[256];/* Time Geolocation field dimension list */
    char            dgeodim[256];/* Data Subsetting field dimension list */
    char            utlbuf[256];/* Utility buffer */
    char           *ptr[64];	/* String pointer array */
    static const char errMesg[] = "Vertical Dimension Not Found: \"%s\".\n";



    /* Set region size to -1 */
    /* --------------------- */
    *size = -1;


    /* Check for valid swath ID */
    /* ------------------------ */
    status = SWchkswid(swathID, "SWregioninfo", &fid, &sdInterfaceID,
		       &swVgrpID);


    /* Check for valid region ID */
    /* ------------------------- */
    if (status == 0)
    {
	if (regionID < 0 || regionID >= NSWATHREGN)
	{
	    status = -1;
	    HEpush(DFE_RANGE, "SWregioninfo", __FILE__, __LINE__);
	    HEreport("Invalid Region id: %d.\n", regionID);
	}
    }



    /* Check for active region ID */
    /* -------------------------- */
    if (status == 0)
    {
	if (SWXRegion[regionID] == 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "SWregioninfo", __FILE__, __LINE__);
	    HEreport("Inactive Region ID: %d.\n", regionID);
	}
    }

   /* This code checks for the attribute detector_count */
   /* which is found in Landsat 7 files.  It is used    */
   /* for some of the loops.                            */
   /* ================================================= */
   if (SWXRegion[regionID]->scanflag == 1)
   {
      land_status = SWattrinfo(swathID, "detector_count", &numtype, &count);
      if (land_status == 0)
      {
         land_status = SWreadattr(swathID, "detector_count", &detect_cnt);
         land_status = SWreadattr(swathID, "scene_count", &scene_cnt);

      }
   }




    /* Check for valid fieldname */
    /* ------------------------- */
    if (status == 0)
    {
	/* Get data field info */
	/* ------------------- */
	status = SWfieldinfo(swathID, fieldname, rank,
			     dims, ntype, dimlist);

	if (status != 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "SWregioninfo", __FILE__, __LINE__);
	    HEreport("Field \"%s\" Not Found.\n", fieldname);
	}
    }



    /* No problems so proceed ... */
    /* -------------------------- */
    if (status == 0)
    {
	/* If non-vertical subset regions defined ... */
	/* ------------------------------------------ */
	if (SWXRegion[regionID]->nRegions > 0 || SWXRegion[regionID]->scanflag == 1)
	{

	    /* Get geolocation dimension name */
	    /* ------------------------------ */
	    status = SWfieldinfo(SWXRegion[regionID]->swathID,
				 "Longitude", &dum,
				 dumdims, &dum, geodim);
            long_status = status;

            /* If Time field being used, check for dimensions */
            /* ---------------------------------------------- */
            if (timeflag == 1)
            {
               /* code change to fix time subset bug for Landsat7 */

               status = SWfieldinfo(SWXRegion[regionID]->swathID,
                                    "Time", &dum,
                                    dumdims, &dum, tgeodim);
               
               if (strcmp(geodim, tgeodim) != 0)
               {
                  strcpy(geodim, tgeodim);
               }
            timeflag = 0;
            }

            /* If defscanregion being used, get dimensions    */
            /* of field being used                            */
            /* ---------------------------------------------- */
            if (SWXRegion[regionID]->scanflag == 1)
            {
               (void) SWnentries(SWXRegion[regionID]->swathID,4,&strbufsize);
               dfieldlist = (char *)calloc(strbufsize + 1, sizeof(char));
               (void) SWinqdatafields(SWXRegion[regionID]->swathID,dfieldlist,dfrank,numtype2);
               status = SWfieldinfo(SWXRegion[regionID]->swathID,dfieldlist,&dum,dumdims,&dum,dgeodim);

                  /* The dimensions have to be switched, because */
                  /* the mappings force a geodim and datadim     */
                  /* so to find the mapping, the dimensions must */
                  /* be switched, but the subsetting will still  */
                  /* be based on the correct dimensions          */
                  /* "long_status == -1" added for CAL file which   */
                  /* doesn't have a Traditional geolocation field   */
                  /* ---------------------------------------------- */
                  if (strcmp(dgeodim,dimlist) != 0 || long_status == -1)
                  {
                      strcpy(geodim,dimlist);
                      strcpy(dimlist,dgeodim);
                  }
            }


	    /* Get "Track" (first) Dimension from geo dimlist */
	    /* ---------------------------------------------- */
	    nDim = EHparsestr(geodim, ',', ptr, slen);
	    geodim[slen[0]] = 0;


	    /* Parse Data Field Dimlist & find mapping */
	    /* --------------------------------------- */
	    nDim = EHparsestr(dimlist, ',', ptr, slen);


	    /* Loop through all dimensions and search for mapping */
	    /* -------------------------------------------------- */
	    for (i = 0; i < nDim; i++)
	    {
		memcpy(utlbuf, ptr[i], slen[i]);
		utlbuf[slen[i]] = 0;
		statMap = SWmapinfo(swathID, geodim, utlbuf,
				    &dum, &incr);

		/* Mapping found */
		/* ------------- */
		if (statMap == 0)
		{
		    found = 1;
		    index = i;
		    break;
		}
	    }


	    /* If mapping not found check for geodim within dimlist */
	    /* ---------------------------------------------------- */
	    if (found == 0)
	    {
		index = EHstrwithin(geodim, dimlist, ',');

		/* Geo dimension found within subset field dimlist */
		/* ----------------------------------------------- */
		if (index != -1)
		{
		    found = 1;
		    incr = 1;
		}
	    }



	    /* If mapping not found check for indexed mapping */
	    /* ---------------------------------------------- */
	    if (found == 0)
	    {
		/* Get size of geo dim & allocate space of index mapping */
		/* ----------------------------------------------------- */
		dum = SWdiminfo(swathID, geodim);
		idxmap = (int32 *) calloc(dum, sizeof(int32));
		if(idxmap == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"SWregioninfo", __FILE__, __LINE__);
		    return(-1);
		}

		/* Loop through all dimensions and search for mapping */
		/* -------------------------------------------------- */
		for (i = 0; i < nDim; i++)
		{
		    memcpy(utlbuf, ptr[i], slen[i]);
		    utlbuf[slen[i]] = 0;

		    idxMapElem = SWidxmapinfo(swathID, geodim, utlbuf, idxmap);


		    /* Mapping found */
		    /* ------------- */
		    if (idxMapElem != -1)
		    {
			found = 1;
			index = i;
			break;
		    }
		}
	    }


	    /* Regular Mapping Found */
	    /* --------------------- */
	    if (found == 1 && idxMapElem == -1)
	    {
		dims[index] = 0;

		/* Loop through all regions */
		/* ------------------------ */
		for (k = 0; k < SWXRegion[regionID]->nRegions; k++)
		{
		    /* Get number of cross tracks in particular region */
		    /* ----------------------------------------------- */
		    nXtrk = SWXRegion[regionID]->StopRegion[k] -
			SWXRegion[regionID]->StartRegion[k] + 1;


		    /* If increment is positive (geodim <= datadim) ... */
		    /* ------------------------------------------------ */
		    if (incr > 0)
		    {
                        if (SWXRegion[regionID]->scanflag == 1)
                        {
                            scan_shift = nXtrk % incr;
                            if(scan_shift != 0)
                               nXtrk = nXtrk - scan_shift;
                            dims[index] += nXtrk/incr;
                            if(long_status == -1 || incr == 1)
                            {
                               scan_shift = nXtrk % detect_cnt;
                               if(scan_shift != 0)
                                  dims[index] = nXtrk - scan_shift;
                            }
                        }
			else
                        {
			   dims[index] += nXtrk * incr;
                        }
		    }
		    else
		    {
			/* Negative increment (geodim > datadim) */
			/* ------------------------------------- */
			dims[index] += nXtrk / (-incr);

			/*
			 * If Xtrk not exactly divisible by incr, round dims
			 * to next highest integer
			 */
			if (nXtrk % (-incr) != 0)
			{
			    dims[index]++;
			}
		    }
		}
	    }
	    else if (found == 1 && idxMapElem != -1)
	    {

		/* Indexed Mapping */
		/* --------------- */

		dims[index] = 0;

		/* Loop through all regions */
		/* ------------------------ */
		for (k = 0; k < SWXRegion[regionID]->nRegions; k++)
		{
                   j = 0;
                   if(SWXRegion[regionID]->scanflag == 1)
                   {
                      startscanline = SWXRegion[regionID]->StartRegion[k];
                      stopscanline = SWXRegion[regionID]->StopRegion[k];
                      if (strcmp(fieldname,dfieldlist) == 0)
                      {
                         dims[index] = stopscanline - startscanline + 1;
                         datafld = 1;
                      }
                      if (strcmp(fieldname, "Latitude") == 0 ||
                          strcmp(fieldname, "Longitude") == 0)
                      {
                         if(SWXRegion[regionID]->band8flag == 2 || SWXRegion[regionID]->band8flag == 3)
                         {
                            status = SWfieldinfo(swathID,"scan_no",&rk,dims2,&nt,dimlist);
                            tbuffer = (uint16 *)calloc(dims2[0], sizeof(uint16));
                            status =SWreadfield(swathID,"scan_no",NULL,NULL,NULL,tbuffer);
                            startscanline = startscanline - ((tbuffer[0] * detect_cnt) - detect_cnt);
                            stopscanline = stopscanline - ((tbuffer[0] * detect_cnt) - 1);
                         }
                         if(SWXRegion[regionID]->band8flag == 2 ||
                            SWXRegion[regionID]->band8flag == 3)
                         {
                            if(startscandim == -1)
                               if(startscanline < idxmap[0])
                               {
                                  startscandim = 0;
                                  dims[index] = 0;
                                  if(stopscanline > idxmap[scene_cnt *2 - 1])
                                  {
                                     stopscandim = scene_cnt*2 - startscandim;
                                     dims[index] = scene_cnt*2 - startscandim;
                                  }
                               }
                         }
                         for (l = 0; l < scene_cnt; l++)
                         {
                            if(idxmap[j] <= startscanline && idxmap[j+1] >= startscanline)
                               if(startscandim == -1)
                               {
                                  dims[index] = j;
                                  startscandim = j;
                               }
                            if(idxmap[j] <= stopscanline && idxmap[j+1] >= stopscanline)
                               if(startscandim != -1)
                               {
                                  dims[index] = j - startscandim + 2;
                                  stopscandim = j + 1;
                               }
                            j = j + 2;
                            if(idxmap[j] == 0  || idxmap[j+1] == 0)
                               l = scene_cnt;
                         }
                         if(SWXRegion[regionID]->band8flag == 1 ||
                            SWXRegion[regionID]->band8flag == 2)
                         {
                            if(stopscandim == -1)
                               if(stopscanline > idxmap[scene_cnt * 2 - 1])
                               {
                                  stopscandim = scene_cnt*2 - dims[index];
                                  dims[index] = scene_cnt*2 - dims[index];
                               }
                         }
                         if(SWXRegion[regionID]->band8flag == 3)
                         {
                            if(startscandim == -1)
                               if(startscanline < idxmap[0])
                               {
                                  startscandim = 0;
                                  if(stopscandim != -1)
                                     dims[index] = stopscandim - startscandim + 1;
                               }
                         }
                         if(SWXRegion[regionID]->band8flag == 2)
                         {
                            if(startscandim == -1)
                               if(startscanline > idxmap[j - 1])
                               {
                                  status = SWfieldinfo(SWXRegion[regionID]->swathID,"scan_no",&rank2,dims2,&nt,dimlist);
                                  buffer2 = (uint16 *)calloc(dims2[0], sizeof(uint16));
                                  status = SWreadfield(SWXRegion[regionID]->swathID,"scan_no",NULL,NULL,NULL,buffer2);
                                  startscanline = startscanline - (buffer2[0] * detect_cnt);
                                  stopscanline = stopscanline - (buffer2[0] * detect_cnt);
                                  free(buffer2);
                                  j = 0;
                                  for (l = 0; l < scene_cnt; l++)
                                  {
                                     if(idxmap[j] <= startscanline && idxmap[j+1] >= startscanline)
                                     {
                                        dims[index] = j;
                                        startscandim = j;
                                     }
                                     if(idxmap[j] <= stopscanline && idxmap[j+1] >= stopscanline)
                                        dims[index] = j - startscandim + 2;
                                     j = j + 2;
                                     if(idxmap[j] == 0  || idxmap[j+1] == 0)
                                        l = scene_cnt;
                                  }

                               }
                         }
                      }
                      if (strcmp(fieldname, "scene_center_latitude") == 0 ||
                          strcmp(fieldname, "scene_center_longitude") == 0)
                      {
                         startscanline = SWXRegion[regionID]->StartRegion[k];
                         stopscanline = SWXRegion[regionID]->StopRegion[k];
                         if(startscanline < idxmap[0])
                         {
                            startscandim = 0;
                            dims[index] = 0;
                         }
                         for (l = 0; l < scene_cnt-1; l++)
                         {
                            if(idxmap[l] <= startscanline && idxmap[l+1] >= startscanline)
                               if(startscandim == -1)
                               {
                                  dims[index] = l;
                                  startscandim = l;
                               }
                            if(idxmap[l] <= stopscanline && idxmap[l+1] >= stopscanline)
                            {
                               dims[index] = l - startscandim + 2;
                               stopscandim = l + 1;
                            }
                         }
                         if(stopscandim == -1)
                         {
                            if(stopscanline > idxmap[scene_cnt - 1])
                            {
                               dims[index] = scene_cnt - startscandim;
                               stopscandim = scene_cnt - 1;
                            }
                         }
                         if(SWXRegion[regionID]->band8flag == 1)
                         {
                            if(stopscandim == -1)
                               if(stopscanline > idxmap[scene_cnt - 1])
                               {
                                  dims[index] = scene_cnt - startscandim;
                                  stopscandim = scene_cnt - 1;
                               }
                         }
                         if(SWXRegion[regionID]->band8flag == 2 ||
                            SWXRegion[regionID]->band8flag == 3)
                         {
                            if(startscandim == -1)
                            {
                               if(startscanline < idxmap[0])
                               {
                                  startscandim = 0;
                                  dims[index] = stopscandim - startscandim + 1;
                               }
                            }
                            if(startscandim == -1)
                            {
                               startscanline = SWXRegion[regionID]->StartScan[k] * detect_cnt;
                               stopscanline = SWXRegion[regionID]->StopScan[k] * detect_cnt;
                               for (l = 0; l < scene_cnt-1; l++)
                               {
                                  if(idxmap[l] <= startscanline && idxmap[l+1] >= startscanline)
                                     dims[index] = l;
                                  if(idxmap[l] <= stopscanline && idxmap[l+1] >= stopscanline)
                                     dims[index] = l - dims[index] + 1;
                               }
                            }
                         }
                      }
                   }
                   else
                   {
                      if (datafld != 1)
                      {
		         /* Get number of cross tracks in particular region */
		         /* ----------------------------------------------- */
		         nXtrk = idxmap[SWXRegion[regionID]->StopRegion[k]] -
			         idxmap[SWXRegion[regionID]->StartRegion[k]] + 1;

		         dims[index] += nXtrk;
                      }
                    }
		}
	    }
	    else
	    {
		/* Mapping not found */
		/* ----------------- */
	        status = -1;  
		HEpush(DFE_GENAPP, "SWregioninfo",
		       __FILE__, __LINE__);
		HEreport(
			 "Mapping Not Defined for \"%s\" Dimension.\n",
			 geodim);
	    }
	}



	/* Vertical Subset */
	/* --------------- */
	if (status == 0 || status == -1)  /* check the vertical subset in any case -- xhua */
	{
	    for (j = 0; j < 8; j++)
	    {
		/* If active vertical subset ... */
		/* ----------------------------- */
		if (SWXRegion[regionID]->StartVertical[j] != -1)
		{

		    /* Find vertical dimension within dimlist */
		    /* -------------------------------------- */
		    index = EHstrwithin(SWXRegion[regionID]->DimNamePtr[j],
					dimlist, ',');

		    /* If dimension found ... */
		    /* ---------------------- */
		    if (index != -1)
		    {
			/* Compute dimension size */
			/* ---------------------- */
			dims[index] =
			    SWXRegion[regionID]->StopVertical[j] -
			    SWXRegion[regionID]->StartVertical[j] + 1;
		    }
		    else
		    {
			/* Vertical dimension not found */
			/* ---------------------------- */
			status = -1;
			*size = -1;
			HEpush(DFE_GENAPP, "SWregioninfo", __FILE__, __LINE__);
			HEreport(errMesg, SWXRegion[regionID]->DimNamePtr[j]);
		    }
		}
	    }



	    /* Compute size of region data buffer */
	    /* ---------------------------------- */
	    if (status == 0)
	    {
                if(idxMapElem == 1 && SWXRegion[regionID]->scanflag == 1 && land_status == 0)
                {
                    if(startscandim == dims[0])
                       dims[0] = scene_cnt*2 - startscandim;
                }

		/* Compute number of total elements */
		/* -------------------------------- */
		*size = dims[0];
		for (j = 1; j < *rank; j++)
		{
		    *size *= dims[j];
		}

		/* Multiply by size in bytes of numbertype */
		/* --------------------------------------- */
		*size *= DFKNTsize(*ntype);
	    }
	}
    }



    /* Free index mappings if applicable */
    /* --------------------------------- */
    if (idxmap != NULL)
    {
	free(idxmap);
    }
    if(dfieldlist != NULL)
       free(dfieldlist);

    return (status);
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWperiodinfo                                                     |
|                                                                             |
|  DESCRIPTION: Returns size in bytes of region                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               Swath structure ID                      |
|  periodID       int32               Period ID                               |
|  fieldname      char                Fieldname                               |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|  ntype          int32               field number type                       |
|  rank           int32               field rank                              |
|  dims           int32               dimensions of field region              |
|  size           int32               size in bytes of field region           |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Jun 03   Abe Taaheri   added clearing timeflag if SWregioninfo failes      |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
SWperiodinfo(int32 swathID, int32 periodID, char *fieldname,
	     int32 * ntype, int32 * rank, int32 dims[], int32 * size)
{
    intn            status;	/* routine return status variable */


    timeflag = 1;
    /* Call SWregioninfo */
    /* ----------------- */
    status = SWregioninfo(swathID, periodID, fieldname, ntype, rank,
			  dims, size);
    if (status != 0) timeflag = 0;/* clear timeflag if SWregioninfo failed */
    return (status);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWdefvrtregion                                                   |
|                                                                             |
|  DESCRIPTION: Finds elements of a monotonic field within a given range.     |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  regionID       int32               Region ID                               |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               Swath structure ID                      |
|  regionID       int32               Region ID                               |
|  vertObj        char                Vertical object to subset               |
|  range          float64             Vertical subsetting range               |
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Aug 96   Joel Gales    Original Programmer                                 |
|  Dec 96   Joel Gales    Add multiple vertical subsetting capability         |
|  May 97   Joel Gales    Check for supported field types                     |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/


/* Macro to initialize swath region entry */
/* -------------------------------------- */

/*
 * 1) Find empty (inactive) region. 2) Allocate space for region entry. 3)
 * Store file ID and swath ID. 4) Set region ID. 5) Initialize vertical
 * subset entries to -1.
 */

#define SETSWTHREG \
\
for (k = 0; k < NSWATHREGN; k++) \
{ \
    if (SWXRegion[k] == 0) \
    { \
        SWXRegion[k] = (struct swathRegion *) \
	  calloc(1, sizeof(struct swathRegion)); \
	SWXRegion[k]->fid = fid; \
	SWXRegion[k]->swathID = swathID; \
	regionID = k; \
	for (j=0; j<8; j++) \
        { \
             SWXRegion[k]->StartVertical[j] = -1; \
             SWXRegion[k]->StopVertical[j]  = -1; \
             SWXRegion[k]->StartScan[j] = -1; \
             SWXRegion[k]->StopScan[j]  = -1; \
             SWXRegion[k]->band8flag  = -1; \
        } \
	break; \
     } \
}


/* Macro to fill vertical subset entry */
/* ----------------------------------- */

/*
 * 1) Find empty (inactive) vertical region. 2) Set start of vertical region.
 * 3) Allocate space for name of vertical dimension. 4) Write vertical
 * dimension name.
 */

#define FILLVERTREG \
for (j=0; j<8; j++) \
{ \
    if (SWXRegion[regionID]->StartVertical[j] == -1) \
    { \
	SWXRegion[regionID]->StartVertical[j] = i; \
	SWXRegion[regionID]->DimNamePtr[j] = \
	    (char *) malloc(slen + 1); \
	memcpy(SWXRegion[regionID]->DimNamePtr[j], \
	       dimlist, slen + 1); \
	break; \
    } \
} \



int32
SWdefvrtregion(int32 swathID, int32 regionID, char *vertObj, float64 range[])
{
    intn            i;		/* Loop index */
    intn            j;		/* Loop index */
    intn            k;		/* Loop index */
    intn            status;	/* routine return status variable */

    uint8           found = 0;	/* Found flag */

    int16           vertINT16;	/* Temporary INT16 variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           swVgrpID;	/* Swath Vgroup ID */
    int32           slen;	/* String length */
    int32           rank;	/* Field rank */
    int32           nt;		/* Field numbertype */
    int32           dims[8];	/* Field dimensions */
    int32           size;	/* Size of numbertype in bytes */
    int32           vertINT32;	/* Temporary INT32 variable */

    float32         vertFLT32;	/* Temporary FLT32 variable */

    float64         vertFLT64;	/* Temporary FLT64 variable */

    char           *vertArr;	/* Pointer to vertical field data buffer */
    char            dimlist[256];	/* Dimension list */


    /* Check for valid swath ID */
    /* ------------------------ */
    status = SWchkswid(swathID, "SWdefvrtregion", &fid, &sdInterfaceID,
		       &swVgrpID);

    if (status == 0)
    {
	/* Copy first 4 characters of vertObj into dimlist */
	/* ----------------------------------------------- */
	memcpy(dimlist, vertObj, 4);
	dimlist[4] = 0;



	/* If first 4 characters of vertObj = "DIM:" ... */
	/* --------------------------------------------- */

	/* Vertical Object is dimension name */
	/* --------------------------------- */
	if (strcmp(dimlist, "DIM:") == 0)
	{
	    /* Get string length of vertObj (minus "DIM:) */
	    /* ------------------------------------------ */
	    slen = strlen(vertObj) - 4;


	    /* If regionID = -1 then setup swath region entry */
	    /* ---------------------------------------------- */
	    if (regionID == -1)
	    {
		SETSWTHREG;
	    }


	    /* Find first empty (inactive) vertical subset entry */
	    /* ------------------------------------------------- */
	    for (j = 0; j < 8; j++)
	    {
		if (SWXRegion[regionID]->StartVertical[j] == -1)
		{
		    /* Store start & stop of vertical region */
		    /* ------------------------------------- */
		    SWXRegion[regionID]->StartVertical[j] = (int32) range[0];
		    SWXRegion[regionID]->StopVertical[j] = (int32) range[1];

		    /* Store vertical dimension name */
		    /* ----------------------------- */
		    SWXRegion[regionID]->DimNamePtr[j] =
			(char *) malloc(slen + 1);
		    if(SWXRegion[regionID]->DimNamePtr[j] == NULL)
		    { 
			HEpush(DFE_NOSPACE,"SWdefvrtregion", __FILE__, __LINE__);
			return(-1);
		    }
		    memcpy(SWXRegion[regionID]->DimNamePtr[j],
			   vertObj + 4, slen + 1);
		    break;
		}
	    }
	}
	else
	{

	    /* Vertical Object is fieldname */
	    /* ---------------------------- */


	    /* Check for valid fieldname */
	    /* ------------------------- */
	    status = SWfieldinfo(swathID, vertObj, &rank, dims, &nt,
				 dimlist);

	    if (status != 0)
	    {
		status = -1;
		HEpush(DFE_GENAPP, "SWdefvrtregion", __FILE__, __LINE__);
		HEreport("Vertical Field: \"%s\" not found.\n", vertObj);
	    }



	    /* Check for supported field types */
	    /* ------------------------------- */
	    if (nt != DFNT_INT16 &&
		nt != DFNT_INT32 &&
		nt != DFNT_FLOAT32 &&
		nt != DFNT_FLOAT64)
	    {
		status = -1;
		HEpush(DFE_GENAPP, "SWdefvrtregion", __FILE__, __LINE__);
		HEreport("Fieldtype: %d not supported for vertical subsetting.\n", nt);
	    }



	    /* Check that vertical dimension is 1D */
	    /* ----------------------------------- */
	    if (status == 0)
	    {
		if (rank != 1)
		{
		    status = -1;
		    HEpush(DFE_GENAPP, "SWdefvrtregion", __FILE__, __LINE__);
		    HEreport("Vertical Field: \"%s\" must be 1-dim.\n",
			     vertObj);
		}
	    }


	    /* If no problems then continue */
	    /* ---------------------------- */
	    if (status == 0)
	    {
		/* Get string length of vertical dimension */
		/* --------------------------------------- */
		slen = strlen(dimlist);


		/* Get size in bytes of vertical field numbertype */
		/* ---------------------------------------------- */
		size = DFKNTsize(nt);


		/* Allocate space for vertical field */
		/* --------------------------------- */
		vertArr = (char *) calloc(dims[0], size);
		if(vertArr == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"SWdefvrtregion", __FILE__, __LINE__);
		    return(-1);
		}

		/* Read vertical field */
		/* ------------------- */
		status = SWreadfield(swathID, vertObj,
				     NULL, NULL, NULL, vertArr);



		switch (nt)
		{
		case DFNT_INT16:

		    for (i = 0; i < dims[0]; i++)
		    {
			/* Get single element of vertical field */
			/* ------------------------------------ */
			memcpy(&vertINT16, vertArr + i * size, size);


			/* If within range ... */
			/* ------------------- */
			if (vertINT16 >= range[0] &&
			    vertINT16 <= range[1])
			{
			    /* Set found flag */
			    /* -------------- */
			    found = 1;


			    /* If regionID=-1 then setup swath region entry */
			    /* -------------------------------------------- */
			    if (regionID == -1)
			    {
				SETSWTHREG;
			    }


			    /* Fill-in vertical region entries */
			    /* ------------------------------- */
			    FILLVERTREG;

			    break;
			}
		    }


		    /* If found read from "bottom" of data field */
		    /* ----------------------------------------- */
		    if (found == 1)
		    {
			for (i = dims[0] - 1; i >= 0; i--)
			{
			    /* Get single element of vertical field */
			    /* ------------------------------------ */
			    memcpy(&vertINT16, vertArr + i * size, size);


			    /* If within range ... */
			    /* ------------------- */
			    if (vertINT16 >= range[0] &&
				vertINT16 <= range[1])
			    {
				/* Set end of vertical region */
				/* -------------------------- */
				SWXRegion[regionID]->StopVertical[j] = i;
				break;
			    }
			}
		    }
		    else
		    {
			/* No vertical entries within region */
			/* --------------------------------- */
			status = -1;
			HEpush(DFE_GENAPP, "SWdefvrtregion",
			       __FILE__, __LINE__);
			HEreport("No vertical field entries within region.\n");
		    }
		    break;


		case DFNT_INT32:

		    for (i = 0; i < dims[0]; i++)
		    {
			/* Get single element of vertical field */
			/* ------------------------------------ */
			memcpy(&vertINT32, vertArr + i * size, size);


			/* If within range ... */
			/* ------------------- */
			if (vertINT32 >= range[0] &&
			    vertINT32 <= range[1])
			{
			    /* Set found flag */
			    /* -------------- */
			    found = 1;


			    /* If regionID=-1 then setup swath region entry */
			    /* -------------------------------------------- */
			    if (regionID == -1)
			    {
				SETSWTHREG;
			    }


			    /* Fill-in vertical region entries */
			    /* ------------------------------- */
			    FILLVERTREG;

			    break;
			}
		    }


		    /* If found read from "bottom" of data field */
		    /* ----------------------------------------- */
		    if (found == 1)
		    {
			for (i = dims[0] - 1; i >= 0; i--)
			{
			    /* Get single element of vertical field */
			    /* ------------------------------------ */
			    memcpy(&vertINT32, vertArr + i * size, size);


			    /* If within range ... */
			    /* ------------------- */
			    if (vertINT32 >= range[0] &&
				vertINT32 <= range[1])
			    {
				/* Set end of vertical region */
				/* -------------------------- */
				SWXRegion[regionID]->StopVertical[j] = i;
				break;
			    }
			}
		    }
		    else
		    {
			/* No vertical entries within region */
			/* --------------------------------- */
			status = -1;
			HEpush(DFE_GENAPP, "SWdefvrtregion",
			       __FILE__, __LINE__);
			HEreport("No vertical field entries within region.\n");
		    }
		    break;


		case DFNT_FLOAT32:

		    for (i = 0; i < dims[0]; i++)
		    {
			/* Get single element of vertical field */
			/* ------------------------------------ */
			memcpy(&vertFLT32, vertArr + i * size, size);


			/* If within range ... */
			/* ------------------- */
			if (vertFLT32 >= range[0] &&
			    vertFLT32 <= range[1])
			{
			    /* Set found flag */
			    /* -------------- */
			    found = 1;


			    /* If regionID=-1 then setup swath region entry */
			    /* -------------------------------------------- */
			    if (regionID == -1)
			    {
				SETSWTHREG;
			    }


			    /* Fill-in vertical region entries */
			    /* ------------------------------- */
			    FILLVERTREG;

			    break;
			}
		    }


		    /* If found read from "bottom" of data field */
		    /* ----------------------------------------- */
		    if (found == 1)
		    {
			for (i = dims[0] - 1; i >= 0; i--)
			{
			    /* Get single element of vertical field */
			    /* ------------------------------------ */
			    memcpy(&vertFLT32, vertArr + i * size, size);


			    /* If within range ... */
			    /* ------------------- */
			    if (vertFLT32 >= range[0] &&
				vertFLT32 <= range[1])
			    {
				/* Set end of vertical region */
				/* -------------------------- */
				SWXRegion[regionID]->StopVertical[j] = i;
				break;
			    }
			}
		    }
		    else
		    {
			/* No vertical entries within region */
			/* --------------------------------- */
			status = -1;
			HEpush(DFE_GENAPP, "SWdefvrtregion",
			       __FILE__, __LINE__);
			HEreport("No vertical field entries within region.\n");
		    }
		    break;


		case DFNT_FLOAT64:

		    for (i = 0; i < dims[0]; i++)
		    {
			/* Get single element of vertical field */
			/* ------------------------------------ */
			memcpy(&vertFLT64, vertArr + i * size, size);


			/* If within range ... */
			/* ------------------- */
			if (vertFLT64 >= range[0] &&
			    vertFLT64 <= range[1])
			{
			    /* Set found flag */
			    /* -------------- */
			    found = 1;


			    /* If regionID=-1 then setup swath region entry */
			    /* -------------------------------------------- */
			    if (regionID == -1)
			    {
				SETSWTHREG;
			    }


			    /* Fill-in vertical region entries */
			    /* ------------------------------- */
			    FILLVERTREG;

			    break;
			}
		    }


		    /* If found read from "bottom" of data field */
		    /* ----------------------------------------- */
		    if (found == 1)
		    {
			for (i = dims[0] - 1; i >= 0; i--)
			{
			    /* Get single element of vertical field */
			    /* ------------------------------------ */
			    memcpy(&vertFLT64, vertArr + i * size, size);

			    /* If within range ... */
			    /* ------------------- */
			    if (vertFLT64 >= range[0] &&
				vertFLT64 <= range[1])
			    {
				/* Set end of vertical region */
				/* -------------------------- */
				SWXRegion[regionID]->StopVertical[j] = i;
				break;
			    }
			}
		    }
		    else
		    {
			/* No vertical entries within region */
			/* --------------------------------- */
			status = -1;
			HEpush(DFE_GENAPP, "SWdefvrtregion",
			       __FILE__, __LINE__);
			HEreport("No vertical field entries within region.\n");
		    }
		    break;

		}		/* End of switch */
		free(vertArr);
	    }
	}
    }


    /* Set regionID to -1 if bad return status */
    /* --------------------------------------- */
    if (status == -1)
    {
	regionID = -1;
    }


    return (regionID);
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWdefscanregion                                                  |
|                                                                             |
|  DESCRIPTION: Initialize the region structure for Landsat 7 float scene     |
|               subset							      |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  regionID       int32               Region ID                               |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               Swath structure ID                      |
|  fieldname      char                Field name to subset                    |
|  range          float64             subsetting range                        |
|  mode		  int32		      HDFE_ENDPOINT, HDFE_MIDPOINT or         |
|				      HDFE_ANYPOINT                           |
|                                                                             |
|  OUTPUTS:                                                                   |
|  regionID	  int32		      Region ID                               |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jul 99   DaW 	  Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
SWdefscanregion(int32 swathID, char *fieldname, float64 range[], int32 mode)
{
    intn            j;		/* Loop index */
    intn            k;		/* Loop index */
    intn            status;	/* routine return status variable */
    intn            land_status = 3;	/* routine return status variable */
    intn	    band81flag = 0;
    intn	    band82flag = 0;
    intn	    band83flag = 0;
    uint8           detect_cnt = 0;     /* Used to convert scan to scanline  */
                                        /*  L7 float scene sub.              */
    uint8           scene_cnt = 0;


    int32           nmtype = 0; /* Used for L7 float scene sub. */
    int32           count = 0;   /* Used for L7 float scene sub. */
    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           swVgrpID;	/* Swath Vgroup ID */
    int32           slen;	/* String length */
    int32	    dfrank[8];  /* data fields rank */
    int32           rank;	/* Field rank */
    int32	    numtype[8]; /* number type of data fields */
    int32           nt;		/* Field numbertype */
    int32           dims[8];	/* Field dimensions */
    int32           dims2[8];	/* Field dimensions */
    int32	    strbufsize = 0; /* string buffer size */
    int32	    tmprange0 = 0;


    uint16	    *buffer = (uint16 *)NULL;
    int32	    *idxmap = (int32 *)NULL;

    int32	    dimsize = 0;

    int32           regionID = -1;	/* Region ID (return) */

    float64	    scan[2] = {0,0};
    float64	    original_scan[2] = {0,0};

    char            dimlist[256];	/* Dimension list */
    char	    swathname[80];
    char	    *dfieldlist = (char *)NULL;  /* data field list  */
    char	    *tfieldname = (char *)NULL;  /* temp field buffer  */
    char	    *band81 = (char *)NULL;
    char	    *band82 = (char *)NULL;
    char	    *band83 = (char *)NULL;


    /* Check for valid swath ID */
    /* ------------------------ */
    status = SWchkswid(swathID, "SWdefscanregion", &fid, &sdInterfaceID,
		       &swVgrpID);

   /* This code checks for the attribute detector_count */
   /* which is found in Landsat 7 files.  It is used    */
   /* for some of the loops. The other code checks if   */
   /* one scan is requested.                            */
   /* ================================================= */
      land_status = SWattrinfo(swathID, "detector_count", &nmtype, &count);
      if (land_status == 0)
      {
         scan[0] = range[0];
         scan[1] = range[1];
         original_scan[0] = range[0];
         original_scan[1] = range[1];

         land_status = SWreadattr(swathID, "scene_count", &scene_cnt);
         land_status = SWreadattr(swathID, "detector_count", &detect_cnt);
         if (range[0] == range[1])
         {
            range[0] = range[0] * detect_cnt - detect_cnt;
            range[1] = range[0] + detect_cnt - 1;
         }
         else
         {
            range[0] = range[0] * detect_cnt - detect_cnt;
            range[1] = range[1] * detect_cnt - 1;
         }

         Vgetname(SWXSwath[0].IDTable, swathname);
         band81 = strstr(swathname, "B81");
         if (band81 != (char *)NULL)
            band81flag = 1;
         band82 = strstr(swathname, "B82");
         if (band82 != (char *)NULL)
            band82flag = 1;
         band83 = strstr(swathname, "B83");
         if (band83 != (char *)NULL)
            band83flag = 1;
      }


    /* If fieldname is null then subsetting Landsat 7 */
    /* floating scene. Get data field name, assume    */
    /* only one data field in swath                   */
    /* ---------------------------------------------- */

    if (fieldname == (char *)NULL)
    {
       (void) SWnentries(swathID, 4, &strbufsize);
       dfieldlist = (char *)calloc(strbufsize + 1, sizeof(char));
       (void) SWinqdatafields(swathID, dfieldlist, dfrank, numtype);
       tfieldname = (char *)calloc(strbufsize + 1, sizeof(char));
       strcpy(tfieldname, dfieldlist);
    }
    else
    {
       slen = strlen(fieldname);
       tfieldname = (char *)calloc(slen + 1, sizeof(char));
       strcpy(tfieldname, fieldname);
    }

    /* Check for valid fieldname */
    /* ------------------------- */
    status = SWfieldinfo(swathID, tfieldname, &rank, dims, &nt,
	 dimlist);

    if (status != 0)
    {
	status = -1;
	HEpush(DFE_GENAPP, "SWdefscanregion", __FILE__, __LINE__);
	HEreport("Field: \"%s\" not found.\n", tfieldname);
    }


       /* Check if input range values are within range of */
       /* data field                                      */
       /* ----------------------------------------------- */
       if(status == 0)
       {
          status = SWfieldinfo(swathID, "scan_no", &rank, dims2, &nt, dimlist);
          buffer = (uint16 *)calloc(dims2[0], sizeof(uint16));
          status = SWreadfield(swathID,"scan_no", NULL, NULL, NULL, buffer);
          if(scan[0] > buffer[dims2[0]-1])
          {
             HEpush(DFE_GENAPP, "SWdefscanregion", __FILE__, __LINE__);
             HEreport("Range values not within bounds of data field\n");
             free(buffer);
             buffer = (uint16 *)NULL;
             if (dfieldlist != NULL)
                free(dfieldlist);
             free(tfieldname);
             return(-1);
          }
          if(scan[0] < buffer[0])
          {
             if(scan[1] < buffer[0])
             {
                HEpush(DFE_GENAPP, "SWdefscanregion", __FILE__, __LINE__);
                HEreport("Range values not within bounds of data field\n");
                free(buffer);
                buffer = (uint16 *)NULL;
                if (dfieldlist != NULL)
                   free(dfieldlist);
                free(tfieldname);
                return(-1);
             }
             else
             {
                scan[0] = buffer[0];
                range[0] = scan[0] * detect_cnt - detect_cnt;
             }
          }
          if(scan[1] > buffer[dims2[0] - 1])
          {
             scan[1] = buffer[dims2[0] - 1];
             range[1] = scan[1] * detect_cnt - 1;
          }
       }

       if(status == 0)
       {
          dimsize = SWdiminfo(swathID, "GeoTrack");
          if(dimsize > 0)
          {
             idxmap = (int32 *)calloc(dimsize, sizeof(int32));
             (void) SWidxmapinfo(swathID, "GeoTrack", "ScanLineTrack", idxmap);
             tmprange0 = range[0];
             if(band82flag != 1 && band83flag != 1)
             {
                if (range[1] > idxmap[scene_cnt*2 - 1])
                {
                   range[1] = idxmap[scene_cnt*2 - 1];
                   HEreport("Data length compared to geolocation length\n");
                }
             }
             if(band82flag == 1 || band83flag == 1)
             {
                tmprange0 = range[0] - (buffer[0] * detect_cnt - detect_cnt);
             }
             if(tmprange0 >= idxmap[scene_cnt * 2 - 1])
             {
                HEpush(DFE_GENAPP, "SWdefscanregion", __FILE__, __LINE__);
                HEreport(
            "Range values not within bounds of Latitude/Longitude field(s)\n");
                if (dfieldlist != NULL)
                   free(dfieldlist);
                free(tfieldname);
                free(buffer);
                free(idxmap);
                return(-1);
             }
          }
       }

    if (status == 0)
    {
          slen = strlen(tfieldname);

          SETSWTHREG;

	  /* Find first empty (inactive) vertical subset entry */
	  /* ------------------------------------------------- */
	  for (j = 0; j < 8; j++)
	  {
	     if (SWXRegion[regionID]->StartVertical[j] == -1)
	     {
		    /* Store start & stop of region          */
		    /* ------------------------------------- */
                SWXRegion[regionID]->StartScan[j] = (int32) original_scan[0];
                SWXRegion[regionID]->StopScan[j] = (int32) original_scan[1];
		    SWXRegion[regionID]->StartRegion[j] = (int32) range[0];
		    SWXRegion[regionID]->StopRegion[j] = (int32) range[1];
                    ++SWXRegion[regionID]->nRegions;
                    SWXRegion[regionID]->scanflag = 1;
                    if(band81flag == 1)
                       SWXRegion[regionID]->band8flag = 1;
                    if(band82flag == 1)
                       SWXRegion[regionID]->band8flag = 2;
                    if(band83flag == 1)
                       SWXRegion[regionID]->band8flag = 3;
                   break;
		}
	    }
       }


    /* Set regionID to -1 if bad return status */
    /* --------------------------------------- */
    if (status == -1)
    {
	regionID = -1;
    }

    if (dfieldlist != NULL)
       free(dfieldlist);
    free(tfieldname);
    if (buffer != NULL)
       free(buffer);
    if (idxmap != NULL)
    free(idxmap);

    return (regionID);
}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWsetfillvalue                                                   |
|                                                                             |
|  DESCRIPTION: Sets fill value for the specified field.                      |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|  fieldname      char                field name                              |
|  fillval        void                fill value                              |
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
SWsetfillvalue(int32 swathID, char *fieldname, VOIDP fillval)
{
    intn            status;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           swVgrpID;	/* Swath root Vgroup ID */
    int32           sdid;	/* SDS id */
    int32           nt;		/* Number type */
    int32           dims[8];	/* Dimensions array */
    int32           dum;	/* Dummy variable */
    int32           solo;	/* "Solo" (non-merged) field flag */

    char            name[80];	/* Fill value "attribute" name */

    /* Check for valid swath ID and get SDS interface ID */
    status = SWchkswid(swathID, "SWsetfillvalue",
		       &fid, &sdInterfaceID, &swVgrpID);

    if (status == 0)
    {
	/* Get field info */
	status = SWfieldinfo(swathID, fieldname, &dum, dims, &nt, NULL);

	if (status == 0)
	{
	    /* Get SDS ID and solo flag */
	    status = SWSDfldsrch(swathID, sdInterfaceID, fieldname,
				 &sdid, &dum, &dum, &dum,
				 dims, &solo);

	    /* If unmerged field then call HDF set field routine */
	    if (solo == 1)
	    {
		status = SDsetfillvalue(sdid, fillval);
	    }

	    /*
	     * Store fill value in attribute.  Name is given by fieldname
	     * prepended with "_FV_"
	     */
	    strcpy(name, "_FV_");
	    strcat(name, fieldname);
	    status = SWwriteattr(swathID, name, nt, 1, fillval);
	}
	else
	{
	    HEpush(DFE_GENAPP, "SWsetfillvalue", __FILE__, __LINE__);
	    HEreport("Fieldname \"%s\" does not exist.\n", fieldname);
	}
    }
    return (status);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWgetfillvalue                                                   |
|                                                                             |
|  DESCRIPTION: Retrieves fill value for a specified field.                   |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|  fieldname      char                field name                              |
|                                                                             |
|  OUTPUTS:                                                                   |
|  fillval        void                fill value                              |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
SWgetfillvalue(int32 swathID, char *fieldname, VOIDP fillval)
{
    intn            status;	/* routine return status variable */

    int32           nt;		/* Number type */
    int32           dims[8];	/* Dimensions array */
    int32           dum;	/* Dummy variable */

    char            name[80];	/* Fill value "attribute" name */

    /* Check for valid swath ID */
    status = SWchkswid(swathID, "SWgetfillvalue", &dum, &dum, &dum);

    if (status == 0)
    {
	/* Get field info */
	status = SWfieldinfo(swathID, fieldname, &dum, dims, &nt, NULL);

	if (status == 0)
	{
	    /* Read fill value attribute */
	    strcpy(name, "_FV_");
	    strcat(name, fieldname);
	    status = SWreadattr(swathID, name, fillval);
	}
	else
	{
	    HEpush(DFE_GENAPP, "SWgetfillvalue", __FILE__, __LINE__);
	    HEreport("Fieldname \"%s\" does not exist.\n", fieldname);
	}

    }
    return (status);
}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWdetach                                                         |
|                                                                             |
|  DESCRIPTION: Detachs swath structure and performs housekeeping             |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Aug 96   Joel Gales    Cleanup Region External Structure                   |
|  Sep 96   Joel Gales    Setup dim names for SDsetdimnane in dimbuf1 rather  |
|                         than utlstr                                         |
|  Nov 96   Joel Gales    Call SWchkgdid to check for proper swath ID         |
|  Dec 96   Joel Gales    Add multiple vertical subsetting garbage collection |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
SWdetach(int32 swathID)

{
    intn            i;		/* Loop index */
    intn            j;		/* Loop index */
    intn            k;		/* Loop index */
    intn            status = 0;	/* routine return status variable */
    intn            statusFill = 0;	/* return status from SWgetfillvalue */

    uint8          *buf;	/* Buffer for blank (initial) 1D records */

    int32           vdataID;	/* Vdata ID */
    int32          *namelen;	/* Pointer to name string length array */
    int32          *dimlen;	/* Pointer to dim string length array */
    int32           slen1[3];	/* String length array 1 */
    int32           slen2[3];	/* String length array 2 */
    int32           nflds;	/* Number of fields */
    int32           match[5];	/* Merged field match array */
    int32           cmbfldcnt;	/* Number of fields combined */
    int32           sdid;	/* SDS ID */
    int32           vgid;	/* Vgroup ID */
    int32           dims[3];	/* Dimension array */
    int32          *offset;	/* Pointer to merged field offset array */
    int32          *indvdims;	/* Pointer to merged field size array */
    int32           sdInterfaceID;	/* SDS interface ID */
    int32           sID;	/* Swath ID - offset */
    int32           nflds0;	/* Number of fields */
    int32          *namelen0;	/* Pointer to name string length array */
    int32           rank;	/* Rank of merged field */
    int32           truerank;	/* True rank of merged field */
    int32           idOffset = SWIDOFFSET;	/* Swath ID offset */
    int32           dum;	/* Dummy variable */

    char           *nambuf;	/* Pointer to name buffer */
    char          **nameptr;	/* Pointer to name string pointer array */
    char          **dimptr;	/* Pointer to dim string pointer array */
    char          **nameptr0;	/* Pointer to name string pointer array */
    char           *ptr1[3];	/* String pointer array */
    char           *ptr2[3];	/* String pointer array */
    char            dimbuf1[128];	/* Dimension buffer 1 */
    char            dimbuf2[128];	/* Dimension buffer 2 */
    char            swathname[VGNAMELENMAX + 1];	/* Swath name */
    char           *utlbuf;	/* Utility buffer */
    char            fillval[32];/* Fill value buffer */

    /* Check for proper swath ID and get SD interface ID */
    /* ------------------------------------------------- */
    status = SWchkswid(swathID, "SWdetach", &dum, &sdInterfaceID, &dum);

    if (status == 0)
    {
	/* Subtract off swath ID offset and get swath name */
	/* ----------------------------------------------- */
	sID = swathID % idOffset;
	Vgetname(SWXSwath[sID].IDTable, swathname);


	/* Create 1D "orphened" fields */
	/* --------------------------- */
	i = 0;

	/* Find "active" entries in 1d combination array */
	/* --------------------------------------------- */
	while (SWX1dcomb[3 * i] != 0)
	{
	    /* For fields defined within swath... */
	    /* ---------------------------------- */
	    if (SWX1dcomb[3 * i + 1] == SWXSwath[sID].IDTable)
	    {
		/* Get dimension size and vdata ID */
		/* ------------------------------- */
		dims[0] = abs(SWX1dcomb[3 * i]);
		vdataID = SWX1dcomb[3 * i + 2];

		/* Get fieldname (= vdata name) */
		/* ---------------------------- */
		nambuf = (char *) calloc(VSNAMELENMAX + 1, 1);
		if(nambuf == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"SWdetach", __FILE__, __LINE__);
		    return(-1);
		}

		VSgetname(vdataID, nambuf);

		/* Set field within vdata */
		/* ---------------------- */
		VSsetfields(vdataID, nambuf);

		/* Write (blank) records */
		/* --------------------- */
		buf = (uint8 *) calloc(VSsizeof(vdataID, nambuf), dims[0]);
		if(buf == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"SWdetach", __FILE__, __LINE__);
		    free(nambuf);
		    return(-1);
		}
		VSwrite(vdataID, buf, dims[0], FULL_INTERLACE);

		free(buf);
		free(nambuf);

		/* Detach Vdata */
		/* ------------ */
		VSdetach(vdataID);
	    }
	    i++;
	}


	/* SDS combined fields */
	/* ------------------- */
	if (strlen(SWXSDname) == 0)
	{
	    nflds = 0;

	    /* Allocate "dummy" arrays so free() doesn't bomb later */
	    /* ---------------------------------------------------- */
	    nameptr = (char **) calloc(1, sizeof(char *));
	    if(nameptr == NULL)
	    { 
		HEpush(DFE_NOSPACE,"SWdetach", __FILE__, __LINE__);
		return(-1);
	    }
	    namelen = (int32 *) calloc(1, sizeof(int32));
	    if(namelen == NULL)
	    { 
		HEpush(DFE_NOSPACE,"SWdetach", __FILE__, __LINE__);
		free(nameptr);
		return(-1);
	    }
	    nameptr0 = (char **) calloc(1, sizeof(char *));
	    if(nameptr0 == NULL)
	    { 
		HEpush(DFE_NOSPACE,"SWdetach", __FILE__, __LINE__);
		free(nameptr);
		free(namelen);
		return(-1);
	    }
	    namelen0 = (int32 *) calloc(1, sizeof(int32));
	    if(namelen0 == NULL)
	    { 
		HEpush(DFE_NOSPACE,"SWdetach", __FILE__, __LINE__);
		free(nameptr);
		free(namelen);
		free(nameptr0);
		return(-1);
	    }
	    dimptr = (char **) calloc(1, sizeof(char *));
	    if(dimptr == NULL)
	    { 
		HEpush(DFE_NOSPACE,"SWdetach", __FILE__, __LINE__);
		free(nameptr);
		free(namelen);
		free(nameptr0);
		free(namelen0);
		return(-1);
	    }
	    dimlen = (int32 *) calloc(1, sizeof(int32));
	    if(dimlen == NULL)
	    { 
		HEpush(DFE_NOSPACE,"SWdetach", __FILE__, __LINE__);
		free(nameptr);
		free(namelen);
		free(nameptr0);
		free(namelen0);
		free(dimptr);
		return(-1);
	    }
	    offset = (int32 *) calloc(1, sizeof(int32));
	    if(offset == NULL)
	    { 
		HEpush(DFE_NOSPACE,"SWdetach", __FILE__, __LINE__);
		free(nameptr);
		free(namelen);
		free(nameptr0);
		free(namelen0);
		free(dimptr);
		free(dimlen);
		return(-1);
	    }
	    indvdims = (int32 *) calloc(1, sizeof(int32));
	    if(indvdims == NULL)
	    { 
		HEpush(DFE_NOSPACE,"SWdetach", __FILE__, __LINE__);
		free(nameptr);
		free(namelen);
		free(nameptr0);
		free(namelen0);
		free(dimptr);
		free(dimlen);
		free(offset);
		return(-1);		    
	    }
	}
	else
	{
	    /*
	     * "Trim Off" trailing "," and ";" in SWXSDname & SWXSDdims
	     * respectively
	     */
	    SWXSDname[strlen(SWXSDname) - 1] = 0;
	    SWXSDdims[strlen(SWXSDdims) - 1] = 0;


	    /* Get number of fields from SWXSDname string */
	    /* ------------------------------------------ */
	    nflds = EHparsestr(SWXSDname, ',', NULL, NULL);


	    /* Allocate space for various dynamic arrays */
	    /* ----------------------------------------- */
	    nameptr = (char **) calloc(nflds, sizeof(char *));
	    if(nameptr == NULL)
	    { 
		HEpush(DFE_NOSPACE,"SWdetach", __FILE__, __LINE__);
		return(-1);
	    }
	    namelen = (int32 *) calloc(nflds, sizeof(int32));
	    if(namelen == NULL)
	    { 
		HEpush(DFE_NOSPACE,"SWdetach", __FILE__, __LINE__);
		free(nameptr);
		return(-1);
	    }
	    nameptr0 = (char **) calloc(nflds, sizeof(char *));
	    if(nameptr0 == NULL)
	    { 
		HEpush(DFE_NOSPACE,"SWdetach", __FILE__, __LINE__);
		free(nameptr);
		free(namelen);
		return(-1);
	    }
	    namelen0 = (int32 *) calloc(nflds, sizeof(int32));
	    if(namelen0 == NULL)
	    { 
		HEpush(DFE_NOSPACE,"SWdetach", __FILE__, __LINE__);
		free(nameptr);
		free(namelen);
		free(nameptr0);
		return(-1);
	    }
	    dimptr = (char **) calloc(nflds, sizeof(char *));
	    if(dimptr == NULL)
	    { 
		HEpush(DFE_NOSPACE,"SWdetach", __FILE__, __LINE__);
		free(nameptr);
		free(namelen);
		free(nameptr0);
		free(namelen0);
		return(-1);
	    }
	    dimlen = (int32 *) calloc(nflds, sizeof(int32));
	    if(dimlen == NULL)
	    { 
		HEpush(DFE_NOSPACE,"SWdetach", __FILE__, __LINE__);
		free(nameptr);
		free(namelen);
		free(nameptr0);
		free(namelen0);
		free(dimptr);
		return(-1);
	    }
	    offset = (int32 *) calloc(nflds, sizeof(int32));
	    if(offset == NULL)
	    { 
		HEpush(DFE_NOSPACE,"SWdetach", __FILE__, __LINE__);
		free(nameptr);
		free(namelen);
		free(nameptr0);
		free(namelen0);
		free(dimptr);
		free(dimlen);
		return(-1);
	    }
	    indvdims = (int32 *) calloc(nflds, sizeof(int32));
	    if(indvdims == NULL)
	    { 
		HEpush(DFE_NOSPACE,"SWdetach", __FILE__, __LINE__);
		free(nameptr);
		free(namelen);
		free(nameptr0);
		free(namelen0);
		free(dimptr);
		free(dimlen);
		free(offset);
		return(-1);		    
	    }


	    /* Parse SWXSDname and SWXSDdims strings */
	    /* ------------------------------------- */
	    nflds = EHparsestr(SWXSDname, ',', nameptr, namelen);
	    nflds = EHparsestr(SWXSDdims, ';', dimptr, dimlen);
	}


	/* Loop through all the fields */
	/* --------------------------- */
	for (i = 0; i < nflds; i++)
	{
	    /* If active entry and field is within swath to be detached ... */
	    /* ------------------------------------------------------------ */
	    if (SWXSDcomb[5 * i] != 0 &&
		SWXSDcomb[5 * i + 3] == SWXSwath[sID].IDTable)
	    {
		nambuf = (char *) calloc(strlen(SWXSDname) + 1, 1);
		if(nambuf == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"SWdetach", __FILE__, __LINE__);
		    return(-1);
		}
		utlbuf = (char *) calloc(2 * strlen(SWXSDname) + 7, 1);
		if(utlbuf == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"SWdetach", __FILE__, __LINE__);
		    free(nambuf);
		    return(-1);
		}
		/* Zero out dimbuf1 */
		/* ---------------- */
		for (k = 0; k < sizeof(dimbuf1); k++)
		{
		    dimbuf1[k] = 0;
		}


		/* Load array to match, name & parse dims */
		/* -------------------------------------- */
		memcpy(match, &SWXSDcomb[5 * i], 20);
		memcpy(nambuf, nameptr[i], namelen[i]);

		memcpy(dimbuf1, dimptr[i], dimlen[i]);
		dum = EHparsestr(dimbuf1, ',', ptr1, slen1);


		/* Separate combined (first) dimension from others */
		/* ----------------------------------------------- */
		dimbuf1[slen1[0]] = 0;

		offset[0] = 0;
		indvdims[0] = abs(match[0]);

		/*
		 * Loop through remaining fields to check for matches with
		 * current one
		 */
		for (j = i + 1, cmbfldcnt = 0; j < nflds; j++)
		{
		    if (SWXSDcomb[5 * j] != 0)
		    {
			/* Zero out dimbuf2 */
			/* ---------------- */
			for (k = 0; k < sizeof(dimbuf2); k++)
			{
			    dimbuf2[k] = 0;
			}

			/*
			 * Parse the dimensions and separate first for this
			 * entry
			 */
			memcpy(dimbuf2, dimptr[j], dimlen[j]);
			dum = EHparsestr(dimbuf2, ',', ptr2, slen2);
			dimbuf2[slen2[0]] = 0;


			/*
			 * If 2nd & 3rd dimension values and names (1st and
			 * 2nd for rank=2 array), swath ID, and numbertype
			 * are equal, then these fields can be combined.
			 */
			if (match[1] == SWXSDcomb[5 * j + 1] &&
			    match[2] == SWXSDcomb[5 * j + 2] &&
			    match[3] == SWXSDcomb[5 * j + 3] &&
			    match[4] == SWXSDcomb[5 * j + 4] &&
			    strcmp(dimbuf1 + slen1[0] + 1,
				   dimbuf2 + slen2[0] + 1) == 0)
			{
			    /* Add to combined dimension size */
			    /* ------------------------------ */
			    match[0] += SWXSDcomb[5 * j];

			    /* Concatanate name */
			    /* ---------------- */
			    strcat(nambuf, ",");
			    memcpy(nambuf + strlen(nambuf),
				   nameptr[j], namelen[j]);

			    /*
			     * Increment number of merged fields, store
			     * individual dims and dim offsets
			     */
			    cmbfldcnt++;
			    indvdims[cmbfldcnt] = abs(SWXSDcomb[5 * j]);
			    offset[cmbfldcnt] = offset[cmbfldcnt - 1] +
				indvdims[cmbfldcnt - 1];

			    /* Delete this field from combination list */
			    /* --------------------------------------- */
			    SWXSDcomb[5 * j] = 0;
			}
		    }
		}


		/* Create SDS */
		/* ---------- */

		/* Parse names string */
		/* ------------------ */
		nflds0 = EHparsestr(nambuf, ',', nameptr0, namelen0);

		if (abs(match[0]) == 1)
		{
		    /* Two Dimensional Array (no merging has occured) */
		    /* ---------------------------------------------- */
		    dims[0] = abs(match[1]);
		    dims[1] = abs(match[2]);

		    /* Create SDS */
		    /* ---------- */
		    rank = 2;
		    sdid = SDcreate(sdInterfaceID, nambuf,
				    SWXSDcomb[5 * i + 4], 2, dims);
		}
		else
		{
		    /* Three Dimensional Array */
		    /* ----------------------- */
		    dims[0] = abs(match[0]);
		    dims[1] = abs(match[1]);
		    dims[2] = abs(match[2]);

		    rank = 3;

		    /*
		     * If merged fields then form string consisting of
		     * "MRGFLD_" + 1st field in merge + ":" + entire merged
		     * field list and store in utlbuf. Then write to
		     * MergedField metadata section
		     */
		    if (cmbfldcnt > 0)
		    {
			strcpy(utlbuf, "MRGFLD_");
			memcpy(utlbuf + 7, nameptr0[0], namelen0[0]);
			utlbuf[7 + namelen0[0]] = 0;
			strcat(utlbuf, ":");
			strcat(utlbuf, nambuf);

			status = EHinsertmeta(sdInterfaceID, swathname, "s",
					      6L, utlbuf, NULL);
		    }
		    else
		    {
			/*
			 * If not merged field then store field name in
			 * utlbuf
			 */
			strcpy(utlbuf, nambuf);
		    }

		    /* Create SDS */
		    /* ---------- */
		    sdid = SDcreate(sdInterfaceID, utlbuf,
				    SWXSDcomb[5 * i + 4], 3, dims);


		    /*
		     * If merged field then store dimensions and offsets as
		     * SD attributes
		     */
		    if (cmbfldcnt > 0)
		    {
			SDsetattr(sdid, "Field Dims", DFNT_INT32,
				  cmbfldcnt + 1, (VOIDP) indvdims);

			SDsetattr(sdid, "Field Offsets", DFNT_INT32,
				  cmbfldcnt + 1, (VOIDP) offset);
		    }
		}



		/* Register Dimensions in SDS */
		/* -------------------------- */
		for (k = 0; k < rank; k++)
		{
		    if (rank == 2)
		    {
			/* Copy k+1th dimension into dimbuf2 if rank = 2 */
			/* --------------------------------------------- */
			memcpy(dimbuf2, ptr1[k + 1], slen1[k + 1]);
			dimbuf2[slen1[k + 1]] = 0;
		    }
		    else
		    {
			/* Copy kth dimension into dimbuf2 if rank > 2 */
			/* ------------------------------------------- */
			memcpy(dimbuf2, ptr1[k], slen1[k]);
			dimbuf2[slen1[k]] = 0;
		    }

		    /*
		     * If first dimension and merged field then generate
		     * dimension name consisting of "MRGDIM:" + swathname +
		     * dimension size
		     */
		    if (k == 0 && cmbfldcnt > 0)
		    {
			sprintf(dimbuf2, "%s%s_%ld", "MRGDIM:",
				swathname, (long)dims[0]);
		    }
		    else
		    {
			/* Otherwise concatanate swathname to dim name */
			/* ------------------------------------------- */
			strcat(dimbuf2, ":");
			strcat(dimbuf2, swathname);
		    }

		    /* Register dimensions using "SDsetdimname" */
		    /* ---------------------------------------- */
		    SDsetdimname(SDgetdimid(sdid, k), (char *) dimbuf2);
		}



		/* Write Fill Value */
		/* ---------------- */
		for (k = 0; k < nflds0; k++)
		{
		    /* Check if fill values has been set */
		    /* --------------------------------- */
		    memcpy(utlbuf, nameptr0[k], namelen0[k]);
		    utlbuf[namelen[k]] = 0;
		    statusFill = SWgetfillvalue(swathID, utlbuf, fillval);

		    if (statusFill == 0)
		    {
			/*
			 * If merged field then fill value must be stored
			 * manually using EHfillfld
			 */
			if (cmbfldcnt > 0)
			{
			    dims[0] = indvdims[k];
			    truerank = (dims[0] == 1) ? 2 : 3;
			    EHfillfld(sdid, rank, truerank,
				      DFKNTsize(match[4]), offset[k],
				      dims, fillval);
			}
			/*
			 * If single field then just use the HDF set fill
			 * function
			 */
			else
			{
			    status = SDsetfillvalue(sdid, fillval);
			}
		    }
		}


		/*
		 * Insert SDS within the appropriate Vgroup (geo or data) and
		 * "detach" newly-created SDS
		 */
		vgid = (match[0] < 0)
		    ? SWXSwath[sID].VIDTable[0]
		    : SWXSwath[sID].VIDTable[1];

		Vaddtagref(vgid, DFTAG_NDG, SDidtoref(sdid));
		SDendaccess(sdid);

		free(nambuf);
		free(utlbuf);
	    }
	}



	/* "Contract" 1dcomb array */
	/* ----------------------- */
	i = 0;
	while (SWX1dcomb[3 * i] != 0)
	{
	    if (SWX1dcomb[3 * i + 1] == SWXSwath[sID].IDTable)
	    {
		memcpy(&SWX1dcomb[3 * i],
		       &SWX1dcomb[3 * (i + 1)],
		       (512 - i - 1) * 3 * 4);
	    }
	    else
		i++;
	}


	/* "Contract" SDcomb array */
	/* ----------------------- */
	for (i = 0; i < nflds; i++)
	{
	    if (SWXSDcomb[5 * i + 3] == SWXSwath[sID].IDTable)
	    {
		if (i == (nflds - 1))
		{
		    SWXSDcomb[5 * i] = 0;
		    *(nameptr[i] - (nflds != 1)) = 0;
		    *(dimptr[i] - (nflds != 1)) = 0;
		}
		else
		{
		    memmove(&SWXSDcomb[5 * i],
			   &SWXSDcomb[5 * (i + 1)],
			   (512 - i - 1) * 5 * 4);

		    memmove(nameptr[i],
			   nameptr[i + 1],
			   nameptr[0] + 2048 - nameptr[i + 1] - 1);

		    memmove(dimptr[i],
			   dimptr[i + 1],
			   dimptr[0] + 2048 * 2 - dimptr[i + 1] - 1);
		}

		i--;
		nflds = EHparsestr(SWXSDname, ',', nameptr, namelen);
		nflds = EHparsestr(SWXSDdims, ';', dimptr, dimlen);
	    }
	}


	/* Replace trailing delimitors on SWXSDname & SWXSDdims */
	/* ---------------------------------------------------- */
	if (nflds != 0)
	{
	    strcat(SWXSDname, ",");
	    strcat(SWXSDdims, ";");
	}



	/* Free up a bunch of dynamically allocated arrays */
	/* ----------------------------------------------- */
	free(nameptr);
	free(namelen);
	free(nameptr0);
	free(namelen0);
	free(dimptr);
	free(dimlen);
	free(offset);
	free(indvdims);




	/* "Detach" from previously attached SDSs */
	/* -------------------------------------- */
	for (k = 0; k < SWXSwath[sID].nSDS; k++)
	{
	    SDendaccess(SWXSwath[sID].sdsID[k]);
	}
	free(SWXSwath[sID].sdsID);
	SWXSwath[sID].sdsID = 0;
	SWXSwath[sID].nSDS = 0;


	/* Detach Swath Vgroups */
	/* -------------------- */
	Vdetach(SWXSwath[sID].VIDTable[0]);
	Vdetach(SWXSwath[sID].VIDTable[1]);
	Vdetach(SWXSwath[sID].VIDTable[2]);
	Vdetach(SWXSwath[sID].IDTable);


	/* Delete entries from External Arrays */
	/* ----------------------------------- */
	SWXSwath[sID].active = 0;
	SWXSwath[sID].VIDTable[0] = 0;
	SWXSwath[sID].VIDTable[1] = 0;
	SWXSwath[sID].VIDTable[2] = 0;
	SWXSwath[sID].IDTable = 0;
	SWXSwath[sID].fid = 0;


	/* Free Region Pointers */
	/* -------------------- */
	for (k = 0; k < NSWATHREGN; k++)
	{
	    if (SWXRegion[k] != 0 &&
		SWXRegion[k]->swathID == swathID)
	    {
		for (i = 0; i < 8; i++)
		{
		    if (SWXRegion[k]->DimNamePtr[i] != 0)
		    {
			free(SWXRegion[k]->DimNamePtr[i]);
		    }
		}

		free(SWXRegion[k]);
		SWXRegion[k] = 0;
	    }
	}

    }
    return (status);
}

/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWclose                                                          |
|                                                                             |
|  DESCRIPTION: Closes HDF-EOS file                                           |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  fid            int32               File ID                                 |
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
SWclose(int32 fid)

{
    intn            status = 0;	/* routine return status variable */

    /* Call EHclose to perform file close */
    /* ---------------------------------- */
    status = EHclose(fid);

    return (status);
}

/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWupdatescene                                                    |
|                                                                             |
|  DESCRIPTION: Updates the StartRegion and StopRegion values                 |
|               for a specified region.                                       |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               Swath structure ID                      |
|  regionID       int32               Region ID                               |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Nov 98   Xinmin Hua    Original developing                                 |
|  Aug 99   Abe Taaheri   Added code to exclude regions that have the same    |
|                         start and stop.                                     |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
SWupdatescene(int32 swathID, int32 regionID)
{
    intn            k;          /* Loop index */
    int32           status;     /* routine return status variable */
 
    int32           fid;        /* HDF-EOS file ID */
    int32           sdInterfaceID;      /* HDF SDS interface ID */
    int32           swVgrpID;   /* Swath Vgroup ID */
 
    int32           startReg;   /* Indexed start region */
    int32           stopReg;    /* Indexed stop region */
    int32           index[MAXNREGIONS]; /* to store indicies when stop and 
					   start are different */
					   
    int32           ind;        /* index */
    int32           tempnRegions; /* temp number of regions */

    /* Check for valid swath ID */
    /* ------------------------ */
    status = SWchkswid(swathID, "SWupdatescene", &fid, &sdInterfaceID,
                       &swVgrpID);
 
 
    /* Check for valid region ID */
    /* ------------------------- */
    if (status == 0)
    {
        if (regionID < 0 || regionID >= NSWATHREGN)
        {
            status = -1;
            HEpush(DFE_RANGE, "SWupdatescene", __FILE__, __LINE__);
            HEreport("Invalid Region id: %d.\n", regionID);
        }
    }
 
    /* Check for active region ID */
    /* -------------------------- */
    if (status == 0)
    {
        if (SWXRegion[regionID] == 0)
        {
            status = -1;
            HEpush(DFE_GENAPP, "SWupdatescene", __FILE__, __LINE__);
            HEreport("Inactive Region ID: %d.\n", regionID);
        }
    }
  
    if (status == 0)
    {
	tempnRegions = SWXRegion[regionID]->nRegions;
	ind =0;
	
	for (k = 0; k < SWXRegion[regionID]->nRegions; k++)
        {
           startReg = SWXRegion[regionID]->StartRegion[k];
           stopReg = SWXRegion[regionID]->StopRegion[k];
	   if(startReg == stopReg)
	   {
	       /* reduce number of regions by 1, if tempnRegions is 0 issue
		  error and break from loop*/
	       tempnRegions -= 1;
	       
	       if(tempnRegions == 0)
	       {
		   /* first free allocated memory for SWXRegion[regionID] 
		      in the function SWdefboxregion and make regionID
		      inactive */
		   free(SWXRegion[regionID]);
		   SWXRegion[regionID] = 0;
		   status = -1;
		   HEpush(DFE_GENAPP, "SWupdatescene", __FILE__, __LINE__);
		   HEreport("Inactive Region ID: %d.\n", regionID);
		   break;
	       }
	   }
	   else
	   {
	       /* store index number of regions that have different start and
		  stop */
	       index[ind] = k;
	       ind += 1;
	   }
	}
	if (status != 0)
	{
	    return (status);
	}
	else
	{
	    SWXRegion[regionID]->nRegions = tempnRegions;
	}
	/* keep starts and stops that are different in the structure  */   
	for (k = 0; k < SWXRegion[regionID]->nRegions; k++)
	{
	    SWXRegion[regionID]->StartRegion[k] =
	      SWXRegion[regionID]->StartRegion[index[k]];
	    SWXRegion[regionID]->StopRegion[k] =
	      SWXRegion[regionID]->StopRegion[index[k]];
	}
	
    }

    
    if (status == 0)
    {
 
        for (k = 0; k < SWXRegion[regionID]->nRegions; k++)
        {
 
           startReg = SWXRegion[regionID]->StartRegion[k];
           stopReg = SWXRegion[regionID]->StopRegion[k];
 
           if(startReg % 2 == 1) {
 
              SWXRegion[regionID]->StartRegion[k] = ++startReg;
 
           }
           if(stopReg % 2 == 0) {
 
              SWXRegion[regionID]->StopRegion[k] = --stopReg;
 
           }
 
        }
 
    }
    
    return(status);
 
}

/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWupdateidxmap                                                   |
|                                                                             |
|  DESCRIPTION: Updates the map index for a specified region.                 |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  nout           int32               return Number of elements in output     |
|                                     index array if SUCCEED, (-1) FAIL       |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               Swath structure ID                      |
|  regionID       int32               Region ID                               |
|  indexin        int32               array of index values                   |
|                                                                             |
|  OUTPUTS:                                                                   |
|  indexout       int32               array of index values                   |
|  indicies	  int32		      array of start and stop in region       |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Aug 97   Abe Taaheri   Original Programmer                                 |
|  AUG 97   Abe Taaheri   Add support for index mapping                       |
|  Sep 99   DaW		  Add support for Floating Scene Subsetting Landsat 7 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
SWupdateidxmap(int32 swathID, int32 regionID, int32 indexin[], int32 indexout[], int32 indicies[])
{
    intn            i;          /* Loop index */
    intn            j;          /* Loop index */
    intn            k;          /* Loop index */
    int32           status;     /* routine return status variable */
    int32           land_status = 3;     /* routine return status variable */
 
    int32           fid;        /* HDF-EOS file ID */
    int32           sdInterfaceID;      /* HDF SDS interface ID */
    int32           swVgrpID;   /* Swath Vgroup ID */

    int32           numtype = 0; /* Used for L7 float scene sub. */
    int32           count = 0;   /* Used for L7 float scene sub. */
 
    int32           startReg = 0;   /* Indexed start region */
    int32           stopReg = 0;    /* Indexed stop region */
    int32           nout=-1;       /* Number of elements in output index array */
    int32	    indexoffset = 0;
    uint8	    scene_cnt = 0;	/* Used for L7 float scene sub.      */
    uint8           detect_cnt = 0;     /* Used to convert scan to scanline  */
    intn	    gtflag = 0;
    intn	    ngtflag = 0;
    int32           *buffer1 = (int32 *)NULL;
    int32           *buffer2 = (int32 *)NULL;
 
    /* Check for valid swath ID */
    /* ------------------------ */
    status = SWchkswid(swathID, "SWupdateidxmap", &fid, &sdInterfaceID,
                       &swVgrpID);
 
 
    /* Check for valid region ID */
    /* ------------------------- */
    if (status == 0)
    {
        if (regionID < 0 || regionID >= NSWATHREGN)
        {
            status = -1;
            HEpush(DFE_RANGE, "SWupdateidxmap", __FILE__, __LINE__);
            HEreport("Invalid Region id: %d.\n", regionID);
        }
    }
 
    /* Check for active region ID */
    /* -------------------------- */
    if (status == 0)
    {
        if (SWXRegion[regionID] == 0)
        {
            status = -1;
            HEpush(DFE_GENAPP, "SWextractregion", __FILE__, __LINE__);
            HEreport("Inactive Region ID: %d.\n", regionID);
        }
    }
 
    if (status == 0)
    {
	/* Loop through all regions */
	/* ------------------------ */
	for (k = 0; k < SWXRegion[regionID]->nRegions; k++)
	{
	    
	    /* fix overlap index mapping problem for Landsat 7 */
	    
	    startReg = SWXRegion[regionID]->StartRegion[k];
	    stopReg = SWXRegion[regionID]->StopRegion[k];
	    
	    
            if(SWXRegion[regionID]->scanflag == 1)
            {
               indicies[0] = -1;
               indicies[1] = -1;
               j = 0;
            /* This code checks for the attribute detector_count */
            /* which is found in Landsat 7 files.  It is used    */
            /* for some of the loops.                            */
            /* ================================================= */
               land_status = SWattrinfo(swathID, "scene_count", &numtype, &count);
               if (land_status == 0)
               {
                  land_status = SWreadattr(swathID, "scene_count", &scene_cnt);
                  land_status = SWreadattr(swathID, "detector_count", &detect_cnt);
               }

	      
	      /* calculate the offsets first */
	      buffer1 = (int32 *)calloc(74, sizeof(int32));
	      buffer2 = (int32 *)calloc(74, sizeof(int32));
	      
	      status = SWidxmapinfo(swathID,"GeoTrack",
				    "ScanLineTrack", (int32*)buffer1);
	      status = SWidxmapinfo(swathID,"UpperTrack",
				    "ScanLineTrack", (int32*)buffer2);
	      
	      indexoffset = buffer2[0] - buffer1[0];
	      free(buffer1);
	      free(buffer2);

               if(SWXRegion[regionID]->band8flag == -1)
               {
                  for(i=0; i<scene_cnt;i++)
                  {
                     if(indexin[j] <= startReg && indexin[j+1] >= startReg)
                        if(indicies[0] == -1)
                           indicies[0] = j;
                     if(indexin[j] <= stopReg && indexin[j+1] >= stopReg)
                        indicies[1] = j + 1;
                     j = j + 2;
                     if(indexin[j] == 0 || indexin[j+1] == 0)
                        i = scene_cnt;
                  }
                  if(indicies[0] == -1)
                  {
                     if(startReg <= indexin[0])
                        indicies[0] = 0;
                  }
                  if(indicies[0] == -1)
                  {
                     j = 0;
                     for(i=0; i<scene_cnt; i++)
                     {
                        if(indexin[j] <= startReg && indexin[j+1] >= startReg)
                           if(indicies[0] == -1)
                              indicies[0] = j;
                        j = j + 1;
                        if(indexin[j] == 0 || indexin[j+1] == 0)
                           i = scene_cnt;
                     }
                  }
                  if(indicies[1] == -1)
                  {
                     j = 0;
                     for(i=0; i<scene_cnt; i++)
                     {
                        if(indexin[j] <= stopReg && indexin[j+1] >= stopReg)
                           if(indicies[1] == -1)
                              indicies[1] = j + 1;
                        j = j + 1;
                        if(indexin[j] == 0 || indexin[j+1] == 0)
                           i = scene_cnt;
                     }
                  }
                  if(indicies[1] == -1)
                     if(stopReg > indexin[scene_cnt - 1])
                        indicies[1] = scene_cnt - 1;
               }

	       /* This section of code handles exceptions in Landsat 7  */
	       /* data.  The Band 8 data - multiple files, data gaps	*/
               /* ===================================================== */
               if(SWXRegion[regionID]->band8flag == 1 ||
                  SWXRegion[regionID]->band8flag == 2 ||
                  SWXRegion[regionID]->band8flag == 3)
               {
                  j = 0;
                  for(i=0; i<scene_cnt; i++)
                  {
                     j = j + 2;
                     if(indexin[j] == 0 || indexin[j+1] == 0)
                     {
                        if(indexin[j] == 0)
                           gtflag = 1;
                        else
                           ngtflag = 1;
                        i = scene_cnt;
                     }
                  }
                  j = 0;
                  if(gtflag == 1)
                  {
                     for(i=0; i<scene_cnt; i++)
                     {
		      if( startReg >= (indexin[j] + indexoffset - detect_cnt) && 
			  startReg <= (indexin[j+1] + indexoffset - detect_cnt) )
                           if(indicies[0] == -1)
                              indicies[0] = j;
		      if( stopReg >= (indexin[j] + indexoffset - detect_cnt ) && 
			  stopReg <= (indexin[j+1] + indexoffset - detect_cnt ) )
                           indicies[1] = j + 1;
                        j = j + 2;
                        if(indexin[j] == 0 || indexin[j+1] == 0)
                           i = scene_cnt;
                     }
                     if(SWXRegion[regionID]->band8flag == 1)
                     {
                        if(indicies[1] == -1)
                           if(stopReg > (indexin[j - 1] + indexoffset - detect_cnt))
                              indicies[1] = j - 1;
                     }
                     if(SWXRegion[regionID]->band8flag == 2 ||
                        SWXRegion[regionID]->band8flag == 3)
                     {
		       
                        if(startReg >= (indexin[j - 1] + indexoffset - detect_cnt))
                        {
                           indicies[0] = -1;
                           indicies[1] = -1;
                           /* status = SWfieldinfo(swathID, "scan_no", &rank, dims2, &nt, dimlist);
                           buffer = (uint16 *)calloc(dims2[0], sizeof(uint16));
                           status = SWreadfield(swathID,"scan_no", NULL, NULL, NULL, buffer);
                           indexoffset = buffer[0] * detect_cnt;
                           free(buffer);
                           startReg = startReg - (indexoffset - detect_cnt);
                           stopReg = stopReg - (indexoffset - 1); */
                           j = 0;
                           for(i=0; i<scene_cnt; i++)
                           {
			     if( startReg >= (indexin[j] + indexoffset - detect_cnt) && 
				 startReg <= (indexin[j+1] + indexoffset - detect_cnt) )
			       if(indicies[0] == -1)
				 indicies[0] = j;
			     if( stopReg >= (indexin[j] + indexoffset - detect_cnt ) && 
				 stopReg <= (indexin[j+1] + indexoffset - detect_cnt ) )
			       indicies[1] = j + 1;
                              j = j + 2;
                              if(indexin[j] == 0 || indexin[j+1] == 0)
                                 i = scene_cnt;
                           }
                        }

                        if(indicies[0] == -1)
                        {
                           j = 0;
                           for(i=0; i<scene_cnt; i++)
                           {
			     if( startReg >= (indexin[j] + indexoffset - detect_cnt) && 
				 startReg <= (indexin[j+1] + indexoffset - detect_cnt) )
			       if(indicies[0] == -1)
				 indicies[0] = j;
		      
			     j = j + 2;
			     if(indexin[j] == 0 || indexin[j+1] == 0)
			       i = scene_cnt;
                           }
                        }
                        if(indicies[1] == -1)
                           if(stopReg > (indexin[j - 1] + indexoffset - detect_cnt) )
                              indicies[1] = j - 1;
                     }
                     if(indicies[1] == -1)
                     {
                        j = 0;
                        for(i=0; i<scene_cnt; i++)
                        {
			  if( stopReg >= (indexin[j] + indexoffset - detect_cnt ) && 
			      stopReg <= (indexin[j+1] + indexoffset - detect_cnt ) )
			    indicies[1] = j;
			  j = j + 2;
			  if(indexin[j] == 0 || indexin[j+1] == 0)
			    i = scene_cnt;
                        }
                     }
                  }

                  if(ngtflag == 1)
                  {
                     for(i=0; i<scene_cnt; i++)
                     {
		      if( startReg >= indexin[j] && startReg <= indexin[j+1])
                           if(indicies[0] == -1)
                              indicies[0] = j;
		      if( stopReg >= indexin[j] && stopReg <= indexin[j+1])
                           indicies[1] = j + 1;
                        j = j + 2;
                        if(indexin[j] == 0 || indexin[j+1] == 0)
                           i = scene_cnt;
                     }
                     if(SWXRegion[regionID]->band8flag == 2)
                     {
                        if(startReg >= indexin[j] )
                        {
                           if(indicies[0] == -1)
                              indicies[0] = j;
                           if(indicies[1] == -1)
                              indicies[1] = j;
                        }
                        if(indicies[0] == -1)
                           if(startReg <= indexin[0])
                              indicies[0] = 0;
                        if(indicies[1] == -1)
                           if(stopReg > indexin[j])
                              indicies[1] = j;
                     }
                     if(indicies[0] == -1)
                     {
                        j = 0;
                        for(i=0; i<scene_cnt; i++)
                        {
			  if( startReg >= indexin[j] && startReg <= indexin[j+1])
			    indicies[0] = j;
                           j = j + 2;
                           if(indexin[j] == 0 || indexin[j+1] == 0)
                              i = scene_cnt;
                        }
                     }
                     if(indicies[1] == -1)
                     {
                        j = 0;
                        for(i=0; i<scene_cnt; i++)
                        {
		      if( stopReg >= indexin[j] && stopReg <= indexin[j+1])
                              indicies[1] = j;
                           j = j + 2;
                           if(indexin[j] == 0 || indexin[j+1] == 0)
                              i = scene_cnt;
                        }
                     }
                     if(indicies[1] == -1)
                     {
                        if(stopReg > indexin[j])
                           indicies[1] = j;
                     }
                  }
                  if(indicies[0] == -1)
                  {
                     if(startReg <= (indexin[0]+ indexoffset - detect_cnt) )
                        indicies[0] = 0;
                     if(indicies[1] == -1)
                        if(stopReg > (indexin[j] + indexoffset - detect_cnt))
                           indicies[1] = j;
                  }
               }
               if (indicies[1] == -1)
               {
                  if(SWXRegion[regionID]->band8flag == 2 ||
                     SWXRegion[regionID]->band8flag == 3)
                  {
                     if(stopReg < (indexin[0] + indexoffset - detect_cnt))
                     {
		       /*status = SWfieldinfo(swathID, "scan_no", &rank, dims2, &nt, dimlist);
                        buffer = (uint16 *)calloc(dims2[0], sizeof(uint16));
                        status = SWreadfield(swathID,"scan_no", NULL, NULL, NULL, buffer);
                        indexoffset = buffer[0] * detect_cnt;
                        free(buffer);
                        startReg = startReg + (indexoffset - detect_cnt);
                        stopReg = stopReg + (indexoffset - 1); */
                        if(stopReg >= (indexin[scene_cnt - 1] + indexoffset - detect_cnt))
                        {
                           indicies[1] = scene_cnt - 1;
                        }
                        else
                        {
                           j = 0;
                           for(i=0;i<scene_cnt;i++)
                           {
		      if( stopReg >= (indexin[j] + indexoffset - detect_cnt ) && 
			  stopReg <= (indexin[j+1] + indexoffset - detect_cnt ) )
			         indicies[1] = j;
			      j = j + 2;
                              if(indexin[j] == 0 || indexin[j+1] == 0)
                                 i = scene_cnt;
                           }
                        }	
                     }

                     if(startReg > (indexin[j - 1] + indexoffset - detect_cnt ))
                     {
                        indicies[0] = -1;
                        indicies[1] = -1;
                        /*status = SWfieldinfo(swathID, "scan_no", &rank, dims2, &nt, dimlist);
                        buffer = (uint16 *)calloc(dims2[0], sizeof(uint16));
                        status = SWreadfield(swathID,"scan_no", NULL, NULL, NULL, buffer);
                        indexoffset = buffer[0] * detect_cnt;
                        free(buffer);
                        startReg = startReg - (indexoffset - detect_cnt);
                        stopReg = stopReg - (indexoffset - 1);*/
                        j = 0;
                        for(i=0; i<scene_cnt; i++)
                        {
		      if( startReg >= (indexin[j] + indexoffset - detect_cnt) && 
			  startReg <= (indexin[j+1] + indexoffset - detect_cnt) )
                              if(indicies[0] == -1)
                                 indicies[0] = j;
		      if( stopReg >= (indexin[j] + indexoffset - detect_cnt ) && 
			  stopReg <= (indexin[j+1] + indexoffset - detect_cnt ) )
                              indicies[1] = j + 1;
                           j = j + 2;
                           if(indexin[j] == 0 || indexin[j+1] == 0)
                              i = scene_cnt;
                        }
                        if(indicies[0] == -1)
                           if(startReg < (indexin[0] +  indexoffset - detect_cnt))
                              indicies[0] = 0;
                        if(indicies[1] == -1)
                           if(stopReg > (indexin[j - 1] + indexoffset - detect_cnt))
                              indicies[1] = j - 1;
                     }
                  }
               }
            }		/* end of if for floating scene update */
            else
            {
	       /* If start of region is odd then increment */
	       /* ---------------------------------------- */
	       if (startReg % 2 == 1)
	       {
		  startReg++;
	       }
	    
	       /* If end of region is even then decrement */
	       /* --------------------------------------- */
	       if (stopReg % 2 == 0)
	       {
		  stopReg--;
	       }

               indicies[0]=startReg;
               indicies[1]=stopReg;
	   }
        }
	
	if (indexout != NULL)
	{ 
           if(SWXRegion[regionID]->scanflag == 1)
           {
              nout = (indicies[1] - indicies[0] + 1);
              j = 0;
              if (nout == 1)
                 indexout[0] = indexin[indicies[0]];
              for(i=0; i<nout;i++)
              {
                 indexout[i] = indexin[indicies[0] + i];
              }
           }
           else
           {
	      /* get new index values */
              /* ==================== */
	      for(i = startReg; i <= stopReg  ; i++)
	      {
	         indexout[i-startReg] = indexin[i];
	      }
	      nout = (stopReg - startReg) + 1;
           }
        }
        else
        {
           nout = indicies[1] - indicies[0] + 1;
	}
     }
   
    
    if(status == -1)
    {  
	return(status);
    }
    else
    {
        return(nout);
    }
    
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWgeomapinfo                                                     |
|                                                                             |
|  DESCRIPTION: Returns mapping information for dimension                     |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                2 for indexed mapping, 1 for regular    |
|                                     mapping, 0 if the dimension is not      |
|                                     and (-1) FAIL                           |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure id                      |
|  geodim         char                geolocation dimension name              |
|                                                                             |
|  OUTPUTS:                                                                   |
|                                                                             |
|  NONE                                                                       |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Aug 97   Abe Taaheri   Original Programmer                                 |
|  Sept 97  DaW           Modified return value so errors can be trapped      |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
SWgeomapinfo(int32 swathID, char *geodim)

{
    intn            status;	/* routine return status variable */
    
    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           swVgrpID;	/* Swath root Vgroup ID */
    int32           idOffset = SWIDOFFSET;	/* Swath ID offset */

    char           *metabufr;	/* Pointer to structural metadata (SM) */
    char           *metabufi;	/* Pointer to structural metadata (SM) */
    char           *metaptrsr[2];/* Pointers to begin and end of SM section */
    char           *metaptrsi[2];/* Pointers to begin and end of SM section */
    char            swathname[80];	/* Swath Name */
    char           *utlstrr;	 /* Utility string */
    char           *utlstri;	 /* Utility string */
   
    
    /* Allocate space for utility string */
    /* --------------------------------- */
    utlstrr = (char *) calloc(UTLSTR_MAX_SIZE, sizeof(char));
    if(utlstrr == NULL)
    { 
	HEpush(DFE_NOSPACE,"SWgeomapinfo", __FILE__, __LINE__);
	return(-1);
    }
    utlstri = (char *) calloc(UTLSTR_MAX_SIZE, sizeof(char));
    if(utlstri == NULL)
    { 
	HEpush(DFE_NOSPACE,"SWgeomapinfo", __FILE__, __LINE__);
	free(utlstrr);
	return(-1);
    }
    status = -1;

    /* Check for valid swath id */
    status = SWchkswid(swathID, "SWgeomapinfo", &fid, &sdInterfaceID, &swVgrpID);
    if (status == 0)
    {
	/* Get swath name */
	Vgetname(SWXSwath[swathID % idOffset].IDTable, swathname);

	/* Get pointers to "DimensionMap" section within SM */
	metabufr = (char *) EHmetagroup(sdInterfaceID, swathname, "s",
				       "DimensionMap", metaptrsr);

	if(metabufr == NULL)
	{
	    free(utlstrr);
	    free(utlstri);
	    return(-1);
	}
	/* Search for mapping - GeoDim/DataDim (surrounded by quotes) */
	sprintf(utlstrr, "%s%s%s", "\t\t\t\tGeoDimension=\"", geodim,
		"\"\n\t\t\t\tDataDimension=");
	metaptrsr[0] = strstr(metaptrsr[0], utlstrr);
	
	/* Get pointers to "IndexDimensionMap" section within SM */
	metabufi = (char *) EHmetagroup(sdInterfaceID, swathname, "s",
				       "IndexDimensionMap", metaptrsi);
	if(metabufi == NULL)
	{
	    free(utlstrr);
	    free(utlstri);
	    return(-1);
	}
	/* Search for mapping - GeoDim/DataDim (surrounded by quotes) */
	sprintf(utlstri, "%s%s%s", "\t\t\t\tGeoDimension=\"", geodim,
		"\"\n\t\t\t\tDataDimension=");
	metaptrsi[0] = strstr(metaptrsi[0], utlstri);

	/*
	** If regular mapping found add 1 to status
        ** If indexed mapping found add 2
        */
	if (metaptrsr[0] < metaptrsr[1] && metaptrsr[0] != NULL)
	{
	    status = status + 1;	    
        }

        if (metaptrsi[0] < metaptrsi[1] && metaptrsi[0] != NULL)
        {
           status = status + 2;	    
        }

	free(metabufr);
	free(metabufi);
    }

    free(utlstrr);
    free(utlstri);

    return (status);
}

/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: SWsdid                                                           |
|                                                                             |
|  DESCRIPTION: Returns SD element ID for swath field                         |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|  fieldname      const char          field name                              |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|  sdid           int32               SD element ID                           |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Oct 07   Andrey Kiselev  Original Programmer                               |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
SWsdid(int32 swathID, const char *fieldname, int32 *sdid)
{
    intn            status;	        /* routine return status variable */
    int32           fid;	        /* HDF-EOS file ID */
    int32           sdInterfaceID;      /* HDF SDS interface ID */
    int32           dum;	        /* Dummy variable */
    int32           dims[H4_MAX_VAR_DIMS]; /* Field/SDS dimensions */

    status = SWchkswid(swathID, "SWsdid", &fid, &sdInterfaceID, &dum);
    if (status != -1)
    {
        status = SWSDfldsrch(swathID, sdInterfaceID, fieldname,
                             sdid, &dum, &dum, &dum, dims, &dum);
    }

    return (status);
}

