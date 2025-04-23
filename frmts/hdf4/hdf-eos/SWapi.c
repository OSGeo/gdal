/*****************************************************************************
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
			     Added error check for memory unavailability in
			     several functions.
			     Added check for NULL metabuf returned from
			     EHmeta... functions. NULL pointer returned from
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
			    Added code in SWdefboxregion to check for l_index k
			      exceeding NSWATHREGN to avoid overwriting
			      memory
			    Removed declaration for unused variable retchar
			      in SWregionl_index
			    Removed initialization code for unused variables
			      in SWregionl_index
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

#include "cpl_port.h" /* for M_PI */
#include "cpl_string.h" /* for CPLsnprintf */

#include "mfhdf.h"
#include "hcomp.h"
#include "HdfEosDef.h"
#include <math.h>

#include "hdf4compat.h"

#define SWIDOFFSET 1048576


static int32 SWX1dcomb[512*3];

/* Added for routine that converts scanline to Lat/long
** for floating scene subsetting
** Jul 1999 DaW
*/
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
static intn SWchkswid(int32, const char *, int32 *, int32 *, int32 *);
static int32 SWfinfo(int32, const char *, const char *, int32 *,
                     int32 [], int32 *, char *);
static intn SWwrrdattr(int32, const char *, int32, int32, const char *, VOIDP);
static intn SW1dfldsrch(int32, int32, const char *, const char *, int32 *,
                        int32 *, int32 *);
static intn SWSDfldsrch(int32, int32, const char *, int32 *, int32 *,
                        int32 *, int32 *, int32 [], int32 *);
static intn SWwrrdfield(int32, const char *, const char *,
                        int32 [], int32 [], int32 [], VOIDP);
static int32 SWinqfields(int32, const char *, char *, int32 [], int32 []);

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
SWopen(const char *filename, intn i_access)

{
    int32           fid /* HDF-EOS file ID */ ;

    /* Call EHopen to perform file access */
    /* ---------------------------------- */
    fid = EHopen(filename, i_access);

    return (fid);
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
SWattach(int32 fid, const char *swathname)

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
    int32           dum;	/* dummy variable */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           nObjects;	/* # of objects in Vgroup */
    int32           nSDS;	/* SDS counter */
    int32           l_index;	/* SDS l_index */
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
				l_index = SDreftoindex(sdInterfaceID, refs[j]);
				sdid = SDselect(sdInterfaceID, l_index);
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
				l_index = SDreftoindex(sdInterfaceID, refs[j]);
				sdid = SDselect(sdInterfaceID, l_index);
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
		   "No more than %d swaths may be open simultaneously");
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
SWchkswid(int32 swathID, const char *routname,
	  int32 * fid, int32 * sdInterfaceID, int32 * swVgrpID)

{
    intn            status = 0;	/* routine return status variable */
    uint8           l_access;	/* Read/Write access code */

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
	int sID = swathID % idOffset;
	/* Check for active swath ID */
	/* ------------------------- */
	if (SWXSwath[sID].active == 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "SWchkswid", __FILE__, __LINE__);
	    HEreport(message2, swathID, routname);
	}
	else
	{

	    /* Get file & SDS ids and Swath Vgroup */
	    /* ----------------------------------- */
	    status = EHchkfid(SWXSwath[sID].fid, " ", fid,
			      sdInterfaceID, &l_access);
	    *swVgrpID = SWXSwath[sID].IDTable;
	}
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
SWdiminfo(int32 swathID, const char *dimname)

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
	int sID = swathID % idOffset;
	if (sID >= NSWATH)
	{
	    free(utlstr);
	    return -1;
	}
	Vgetname(SWXSwath[sID].IDTable, swathname);

	/* Get pointers to "Dimension" section within SM */
	metabuf = (char *) EHmetagroup(sdInterfaceID, swathname, "s",
				       "Dimension", metaptrs);

	if(metabuf == NULL)
	{
	    free(utlstr);
	    return(-1);
	}

	/* Search for dimension name (surrounded by quotes) */
	snprintf(utlstr, UTLSTR_MAX_SIZE, "%s%s%s", "\"", dimname, "\"\n");
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
		size = atoi(utlstr);
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
SWmapinfo(int32 swathID, const char *geodim, const char *datadim, int32 * offset,
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
	int sID = swathID % idOffset;
	if (sID >= NSWATH)
	{
	    free(utlstr);
	    return -1;
	}
	Vgetname(SWXSwath[sID].IDTable, swathname);

	/* Get pointers to "DimensionMap" section within SM */
	metabuf = (char *) EHmetagroup(sdInterfaceID, swathname, "s",
				       "DimensionMap", metaptrs);
	if(metabuf == NULL)
	{
	    free(utlstr);
	    return(-1);
	}

	/* Search for mapping - GeoDim/DataDim (surrounded by quotes) */
	snprintf(utlstr, UTLSTR_MAX_SIZE, "%s%s%s%s%s", "\t\t\t\tGeoDimension=\"", geodim,
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
		*offset = atoi(utlstr);
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
		*increment = atoi(utlstr);
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
|  DESCRIPTION: Returns l_indexed mapping information                           |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  gsize          int32               Number of l_index values (sz of geo dim)  |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure id                      |
|  geodim         char                geolocation dimension name              |
|  datadim        char                data dimension name                     |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|  l_index          int32               array of l_index values                   |
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
SWidxmapinfo(int32 swathID, const char *geodim, const char *datadim, int32 l_index[])
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
	snprintf(utlbuf, sizeof(utlbuf), "%s%s%s%s", "INDXMAP:", geodim, "/", datadim);
	int sID = swathID % idOffset;
	if (sID >= NSWATH)
	{
	    return -1;
	}
	vgid = SWXSwath[sID].VIDTable[2];
	vdataID = EHgetid(fid, vgid, utlbuf, 1, "r");

	/* If found then get geodim size & read l_index mapping values */
	if (vdataID != -1)
	{
	    gsize = SWdiminfo(swathID, geodim);

	    VSsetfields(vdataID, "Index");
	    VSread(vdataID, (uint8 *) l_index, 1, FULL_INTERLACE);
	    VSdetach(vdataID);
	}
	else
	{
	    /*status = -1;*/
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
SWcompinfo(int32 swathID, const char *fieldname, int32 * compcode, intn compparm[])
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

    const char           *HDFcomp[5] = {"HDFE_COMP_NONE", "HDFE_COMP_RLE",
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
	int sID = swathID % idOffset;
	if (sID >= NSWATH)
	{
	    free(utlstr);
	    return -1;
	}
	Vgetname(SWXSwath[sID].IDTable, swathname);

	/* Get pointers to "DataField" section within SM */
	metabuf = (char *) EHmetagroup(sdInterfaceID, swathname, "s",
				       "DataField", metaptrs);
	if(metabuf == NULL)
	{
	    free(utlstr);
	    return(-1);
	}
	/* Search for field */
	snprintf(utlstr, UTLSTR_MAX_SIZE, "%s%s%s", "\"", fieldname, "\"\n");
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
	    snprintf(utlstr, UTLSTR_MAX_SIZE, "%s%s%s", "\"", fieldname, "\"\n");
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
		    compparm[i] = 0;
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
    int sID = swathID % idOffset;
    if (sID >= NSWATH)
    {
        free(utlstr);
        return -1;
    }
    Vgetname(SWXSwath[sID].IDTable, swathname);

    /* Get pointers to appropriate "Field" section within SM */
    if (strcmp(fieldtype, "Geolocation Fields") == 0)
    {
	metabuf = EHmetagroup(sdInterfaceID, swathname, "s",
				       "GeoField", metaptrs);

	if(metabuf == NULL)
	{
	    free(utlstr);
	    return(-1);
	}
    }
    else
    {
	metabuf = EHmetagroup(sdInterfaceID, swathname, "s",
				       "DataField", metaptrs);
	if(metabuf == NULL)
	{
	    free(utlstr);
	    return(-1);
	}
    }


    /* Search for field */
    snprintf(utlstr, UTLSTR_MAX_SIZE, "%s%s%s", "\"", fieldname, "\"\n");
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
	 * Get dimension sizes and concatenate dimension names to dimension
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
|  DESCRIPTION: Wrapper around SWfinfo                                        |
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
SWwrrdattr(int32 swathID, const char *attrname, int32 numbertype, int32 count,
	   const char *wrcode, VOIDP datbuf)

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
	int sID = swathID % idOffset;
	if (sID >= NSWATH)
	{
	    return -1;
	}
	attrVgrpID = SWXSwath[sID].VIDTable[2];
	status = EHattr(fid, attrVgrpID, attrname, numbertype, count,
			wrcode, datbuf);
    }
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
SWreadattr(int32 swathID, const char *attrname, VOIDP datbuf)
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
SWattrinfo(int32 swathID, const char *attrname, int32 * numbertype, int32 * count)
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
	int sID = swathID % idOffset;
	if (sID >= NSWATH)
	{
	    return -1;
	}
	attrVgrpID = SWXSwath[sID].VIDTable[2];

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
	int sID = swathID % idOffset;
	if (sID >= NSWATH)
	{
	    return -1;
	}
	attrVgrpID = SWXSwath[sID].VIDTable[2];

	nattr = EHattrcat(fid, attrVgrpID, attrnames, strbufsize);
    }

    return (nattr);
}

#define REMQUOTE(x) do { \
    char* l_x = x; \
    const size_t l_x_len = strlen(l_x); \
    if (l_x_len >= 2 && l_x[0] == '"' && l_x[l_x_len - 1] == '"') {\
        memmove(l_x, l_x + 1, l_x_len - 2); \
        l_x[l_x_len - 2] = 0; \
    } \
  } while(0)


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
	    int sID = swathID % idOffset;
	    if (sID >= NSWATH)
	    {
	        free(utlstr);
	        return -1;
	    }
	    Vgetname(SWXSwath[sID].IDTable, swathname);

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
			REMQUOTE(utlstr);

			/* If not first name then add comma delimiter */
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
			size = atoi(utlstr);
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
	    int sID = swathID % idOffset;
	    if (sID >= NSWATH)
	    {
	        free(utlstr);
	        return -1;
	    }
	    Vgetname(SWXSwath[sID].IDTable, swathname);

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
			REMQUOTE(utlstr);
			strcat(utlstr, "/");

			/* If not first map then add comma delimiter. */
			if (nMap > 0)
			{
			    strcat(dimmaps, ",");
			}

			/* Add to map list */
			strcat(dimmaps, utlstr);

			/* Get Data Dim, remove quotes */
			EHgetmetavalue(metaptrs, "DataDimension", utlstr);
			REMQUOTE(utlstr);

			/* Add to map list */
			    strcat(dimmaps, utlstr);
		    }

		    /* Get Offset (if desired) */
		    if (offset != NULL)
		    {
			EHgetmetavalue(metaptrs, "Offset", utlstr);
			off = atoi(utlstr);
			offset[nMap] = off;
		    }

		    /* Get Increment (if desired) */
		    if (increment != NULL)
		    {
			EHgetmetavalue(metaptrs, "Increment", utlstr);
			incr = atoi(utlstr);
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
|  DESCRIPTION: Returns l_indexed mappings and l_index sizes                      |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  nMap           int32               Number of l_indexed dimension mappings    |
|                                                                             |
|  INPUTS:                                                                    |
|  swathID        int32               swath structure ID                      |
|                                                                             |
|  OUTPUTS:                                                                   |
|  idxmaps        char                l_indexed dimension mappings              |
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
	/* If mapping names or l_index sizes desired ... */
	/* ------------------------------------------- */
	if (idxmaps != NULL || idxsizes != NULL)
	{
	    /* Get swath name */
	    int sID = swathID % idOffset;
	    if (sID >= NSWATH)
	    {
	        free(utlstr);
	        return -1;
	    }
	    Vgetname(SWXSwath[sID].IDTable, swathname);

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
			REMQUOTE(utlstr);
			strcat(utlstr, "/");

			/* If not first map then add comma delimiter. */
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
			    if (slash) *slash = 0;
			    idxsizes[nMap] = SWdiminfo(swathID, utlstr);
			}


			/* Get Data Dim, remove quotes */
			EHgetmetavalue(metaptrs, "DataDimension", utlstr);
			REMQUOTE(utlstr);

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
SWinqfields(int32 swathID, const char *fieldtype, char *fieldlist, int32 rank[],
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
	    int sID = swathID % idOffset;
	    if (sID >= NSWATH)
	    {
	        free(utlstr);
	        free(utlstr2);
	        return -1;
	    }
	    Vgetname(SWXSwath[sID].IDTable, swathname);

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
			REMQUOTE(utlstr);


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
    int32           nVal = 0;	    /* Number of strings to search for */

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
	int sID = swathID % idOffset;
	if (sID >= NSWATH)
	{
	    free(utlstr);
	    return -1;
	}
	Vgetname(SWXSwath[sID].IDTable, swathname);

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
                        *strbufsize += (int32)strlen(utlstr) - 2;
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
SWinqswath(const char *filename, char *swathlist, int32 * strbufsize)
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
SW1dfldsrch(int32 fid, int32 swathID, const char *fieldname, const char *i_access,
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
    if (sID >= NSWATH)
    {
        return -1;
    }

    /* Get Geolocation Vgroup id and 1D field name Vdata id */
    /* ---------------------------------------------------- */
    vgid = SWXSwath[sID].VIDTable[0];
    vdataID = EHgetid(fid, vgid, fieldname, 1, i_access);
    *fldtype = 0;


    /*
     * If name not found in Geolocation Vgroup then detach Geolocation Vgroup
     * and search in Data Vgroup
     */
    if (vdataID == -1)
    {
	vgid = SWXSwath[sID].VIDTable[1];;
	vdataID = EHgetid(fid, vgid, fieldname, 1, i_access);
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
    int32           attrIndex;	/* Attribute l_index */

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
    if (sID >= NSWATH)
    {
        free(utlstr);
        return -1;
    }

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
		snprintf(utlstr, UTLSTR_MAX_SIZE, "%s%.480s%s", "MergedFieldName=\"",
			name, "\"\n");
		metaptrs[0] = strstr(metaptrs[0], utlstr);


		/* If not found check for old metadata */
		/* ----------------------------------- */
		if (metaptrs[0] == NULL)
		{
		    snprintf(utlstr, UTLSTR_MAX_SIZE, "%s%.480s%s", "OBJECT=\"", name, "\"\n");
		    metaptrs[0] = strstr(oldmetaptr, utlstr);
		}


		/* Get field list and strip off leading and trailing quotes */
		EHgetmetavalue(metaptrs, "FieldList", name);  /* not return status --xhua */
		memmove(name, name + 1, strlen(name) - 2);
		name[strlen(name) - 2] = 0;

		/* Search for desired field within merged field list */
		snprintf(utlstr, UTLSTR_MAX_SIZE, "%s%s%s", "\"", fieldname, "\"");
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
		    /* Get "Field Offsets" SDS attribute l_index */
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


		    /* Get "Field Dims" SDS attribute l_index */
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
    int32           incr[8];	/* I/O increment (stride) */
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
			free(fillbuf);
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
			/* status = */ VSsetfields(vdataID, fieldlist);
			/* status = */ VSseek(vdataID, offset[0]);
			nrec = VSread(vdataID, buf, count[0] * incr[0],
				      FULL_INTERLACE);
		    }
		    else
		    {
			mrgOffset = 0;
		    }



		    /* Fill buffer with "Fill" value (if any) */
		    /* -------------------------------------- */
                    snprintf( attrName, sizeof(attrName), "_FV_%s", fieldname);

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
SWgetfillvalue(int32 swathID, const char *fieldname, VOIDP fillval)
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
|  DESCRIPTION: Detaches swath structure and performs housekeeping            |
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
    intn            k;		/* Loop index */
    intn            status = 0;	/* routine return status variable */

    uint8          *buf;	/* Buffer for blank (initial) 1D records */

    int32           vdataID;	/* Vdata ID */
    int32           dims[3];	/* Dimension array */
    int32           sdInterfaceID;	/* SDS interface ID */
    int32           sID;	/* Swath ID - offset */
    int32           idOffset = SWIDOFFSET;	/* Swath ID offset */
    int32           dum;	/* Dummy variable */

    char            swathname[VGNAMELENMAX + 1];	/* Swath name */

    /* Check for proper swath ID and get SD interface ID */
    /* ------------------------------------------------- */
    status = SWchkswid(swathID, "SWdetach", &dum, &sdInterfaceID, &dum);

    if (status == 0)
    {
	/* Subtract off swath ID offset and get swath name */
	/* ----------------------------------------------- */
	sID = swathID % idOffset;
	if (sID >= NSWATH)
	{
	    return -1;
	}
	Vgetname(SWXSwath[sID].IDTable, swathname);


	/* Create 1D "orphaned" fields */
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
		char* nambuf = (char *) calloc(VSNAMELENMAX + 1, 1);
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
|  FUNCTION: SWgeomapinfo                                                     |
|                                                                             |
|  DESCRIPTION: Returns mapping information for dimension                     |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                2 for l_indexed mapping, 1 for regular    |
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
SWgeomapinfo(int32 swathID, const char *geodim)

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
	int sID = swathID % idOffset;
	if (sID >= NSWATH)
	{
	    free(utlstrr);
	    free(utlstri);
	    return -1;
	}
	Vgetname(SWXSwath[sID].IDTable, swathname);

	/* Get pointers to "DimensionMap" section within SM */
	metabufr = EHmetagroup(sdInterfaceID, swathname, "s",
				       "DimensionMap", metaptrsr);

	if(metabufr == NULL)
	{
	    free(utlstrr);
	    free(utlstri);
	    return(-1);
	}
	/* Search for mapping - GeoDim/DataDim (surrounded by quotes) */
	snprintf(utlstrr, UTLSTR_MAX_SIZE, "%s%s%s", "\t\t\t\tGeoDimension=\"", geodim,
		"\"\n\t\t\t\tDataDimension=");
	metaptrsr[0] = strstr(metaptrsr[0], utlstrr);

	/* Get pointers to "IndexDimensionMap" section within SM */
	metabufi = EHmetagroup(sdInterfaceID, swathname, "s",
				       "IndexDimensionMap", metaptrsi);
	if(metabufi == NULL)
	{
	    free(utlstrr);
	    free(utlstri);
	    return(-1);
	}
	/* Search for mapping - GeoDim/DataDim (surrounded by quotes) */
	snprintf(utlstri, UTLSTR_MAX_SIZE, "%s%s%s", "\t\t\t\tGeoDimension=\"", geodim,
		"\"\n\t\t\t\tDataDimension=");
	metaptrsi[0] = strstr(metaptrsi[0], utlstri);

	/*
	** If regular mapping found add 1 to status
        ** If l_indexed mapping found add 2
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
