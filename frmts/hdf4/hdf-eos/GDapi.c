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
Jun  27, 2000  Abe Taaheri   Added support for EASE grid that uses
                             Behrmann Cylinderical Equal Area (BCEA) projection
Oct  23, 2000  Abe Taaheri   Updated for ISINUS projection, so that both codes
                             31 and 99 can be used for this projection.
Jan  15, 2003   Abe Taaheri  Modified for generalization of EASE Grid.

Jun  05, 2003 Bruce Beaumont / Abe Taaheri

                             Fixed SQUARE definition.
                             Added static projection number/name translation
                             Added projection table lookup in GDdefproj.
                             Removed projection table from GDdefproj
                             Added projection table lookup in GDprojinfo
                             Removed projection table from GDprojinfo
                             Added cast for compcode in call to SDsetcompress
                                in GDdeffield to avoid compiler errors
                             Removed declaration for unused variable endptr
                                in GDSDfldsrch
                             Removed initialization code for unused variables
                                in GDSDfldsrch
                             Removed declarations for unused variables
                                BCEA_scale, r0, s0, xMtr0, xMtr1, yMtr0,
                                and yMtr1 in GDll2ij
                             Removed initialization code for unused variables
                                in GDll2ij
                             Added code in GEO projection handling to allow
                                map to span dateline in GDll2ij
                             Changed "for each point" loop in GDll2ij to
                                return -2147483648.0 for xVal and yVal if
                                for_trans returned an error instead of
                                returning an error to the caller
                                (Note: MAXLONG is defined as 2147483647.0 in
                                 function cproj.c of GCTP)
                             Added code in GDij2ll to use for_trans to
                                translate the BCEA corner points from packed
                                degrees to meters
                             Removed declarations for unused variables
                                BCEA_scale, r0, s0, xMtr, yMtr, epsilon,
                                beta, qp_cea, kz_cea, eccen, eccen_sq,
                                phi1, sinphi1, cosphi1, lon, lat, xcor,
                                ycor, and nlatlon from GDij2ll
                             Removed initialization code for unused variables
                                in GDij2ll
                             Added declarations for xMtr0, yMtr0, xMtr1, and
                                yMtr1 in GDij2ll
                             Added special-case code for BCEA
                             Changed "for each point" loop in GDij2ll to
                                return PGSd_GCT_IN_ERROR (1.0e51) for
                                longitude and latitude values if inv_trans
                                returned an error instead of return an error
                                to the caller
                             Removed declaration for unused variable ii in
                                GDgetpixvalues
                             Removed declaration for unused variable
                                numTileDims in GDtileinfo
                             Added error message and error return at the
                                end of GDll2mm_cea
                             Added return statement to GDll2mm_cea
******************************************************************************/
#include "cpl_string.h"
#include "stdio.h"
#include "mfhdf.h"
#include "hcomp.h"
#include <math.h>
#include "HdfEosDef.h"

#include "hdf4compat.h"

#define	GDIDOFFSET 4194304
#define SQUARE(x)       ((x) * (x))   /* x**2 */


#define NGRID 200
/* Grid Structure External Arrays */
struct gridStructure
{
    int32 active;
    int32 IDTable;
    int32 VIDTable[2];
    int32 fid;
    int32 nSDS;
    int32 *sdsID;
    int32 compcode;
    intn  compparm[5];
    int32 tilecode;
    int32 tilerank;
    int32 tiledims[8];
};
static struct gridStructure GDXGrid[NGRID];



#define NGRIDREGN 256
struct gridRegion
{
    int32 fid;
    int32 gridID;
    int32 xStart;
    int32 xCount;
    int32 yStart;
    int32 yCount;
    int32 somStart;
    int32 somCount;
    float64 upleftpt[2];
    float64 lowrightpt[2];
    int32 StartVertical[8];
    int32 StopVertical[8];
    char *DimNamePtr[8];
};
static struct gridRegion *GDXRegion[NGRIDREGN];

/* define a macro for the string size of the utility strings and some dimension
   list strings. The value of 80 in the previous version of this code
   may not be enough in some cases. The length now is 512 which seems to
   be more than enough to hold larger strings. */

#define UTLSTR_MAX_SIZE 512

/* Static projection table */
static const struct {
    int32 projcode;
    const char *projname;
} Projections[] = {
    {GCTP_GEO,	   "GCTP_GEO"},
    {GCTP_UTM,	   "GCTP_UTM"},
    {GCTP_SPCS,	   "GCTP_SPCS"},
    {GCTP_ALBERS,  "GCTP_ALBERS"},
    {GCTP_LAMCC,   "GCTP_LAMCC"},
    {GCTP_MERCAT,  "GCTP_MERCAT"},
    {GCTP_PS,	   "GCTP_PS"},
    {GCTP_POLYC,   "GCTP_POLYC"},
    {GCTP_EQUIDC,  "GCTP_EQUIDC"},
    {GCTP_TM,	   "GCTP_TM"},
    {GCTP_STEREO,  "GCTP_STEREO"},
    {GCTP_LAMAZ,   "GCTP_LAMAZ"},
    {GCTP_AZMEQD,  "GCTP_AZMEQD"},
    {GCTP_GNOMON,  "GCTP_GNOMON"},
    {GCTP_ORTHO,   "GCTP_ORTHO"},
    {GCTP_GVNSP,   "GCTP_GVNSP"},
    {GCTP_SNSOID,  "GCTP_SNSOID"},
    {GCTP_EQRECT,  "GCTP_EQRECT"},
    {GCTP_MILLER,  "GCTP_MILLER"},
    {GCTP_VGRINT,  "GCTP_VGRINT"},
    {GCTP_HOM,	   "GCTP_HOM"},
    {GCTP_ROBIN,   "GCTP_ROBIN"},
    {GCTP_SOM,	   "GCTP_SOM"},
    {GCTP_ALASKA,  "GCTP_ALASKA"},
    {GCTP_GOOD,	   "GCTP_GOOD"},
    {GCTP_MOLL,	   "GCTP_MOLL"},
    {GCTP_IMOLL,   "GCTP_IMOLL"},
    {GCTP_HAMMER,  "GCTP_HAMMER"},
    {GCTP_WAGIV,   "GCTP_WAGIV"},
    {GCTP_WAGVII,  "GCTP_WAGVII"},
    {GCTP_OBLEQA,  "GCTP_OBLEQA"},
    {GCTP_ISINUS1, "GCTP_ISINUS1"},
    {GCTP_CEA,	   "GCTP_CEA"},
    {GCTP_BCEA,	   "GCTP_BCEA"},
    {GCTP_ISINUS,  "GCTP_ISINUS"},
    {-1,	   NULL}
};

/* Grid Function Prototypes (internal routines) */
static intn GDchkgdid(int32, const char *, int32 *, int32 *, int32 *);
static intn GDSDfldsrch(int32, int32, const char *, int32 *, int32 *,
                        int32 *, int32 *, int32 [], int32 *);
static intn GDwrrdfield(int32, const char *, const char *,
                        int32 [], int32 [], int32 [], VOIDP datbuf);
static intn GDwrrdattr(int32, const char *, int32, int32, const char *, VOIDP);
static intn GDwrrdtile(int32, const char *, const char *, int32 [], VOIDP);

/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDopen                                                           |
|                                                                             |
|  DESCRIPTION: Opens or creates HDF file in order to create, read, or write  |
|                a grid.                                                      |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  fid            int32               HDF-EOS file ID                         |
|                                                                             |
|  INPUTS:                                                                    |
|  filename       char                Filename                                |
|  l_access         intn                HDF l_access code                         |
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
GDopen(const char *filename, intn l_access)

{
    int32           fid /* HDF-EOS file ID */ ;

    /* Call EHopen to perform file l_access */
    /* ---------------------------------- */
    fid = EHopen(filename, l_access);

    return (fid);

}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDattach                                                         |
|                                                                             |
|  DESCRIPTION: Attaches to an existing grid within the file.                 |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  gridID         int32               grid structure ID                       |
|                                                                             |
|  INPUTS:                                                                    |
|  fid            int32               HDF-EOS file id                         |
|  gridname       char                grid structure name                     |
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
|  Sep 99   Abe Taaheri   Modified test for memory allocation check when no   |
|                         SDSs are in the grid, NCR24147                    |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
GDattach(int32 fid, const char *gridname)

{
    intn            i;		/* Loop index */
    intn            j;		/* Loop index */
    intn            ngridopen = 0;	/* # of grid structures open */
    intn            status;	/* routine return status variable */

    uint8           acs;	/* Read/Write file l_access code */

    int32           HDFfid;	/* HDF file id */
    int32           vgRef;	/* Vgroup reference number */
    int32           vgid[3];	/* Vgroup ID array */
    int32           gridID = -1;/* HDF-EOS grid ID */
    int32          *tags;	/* Pnt to Vgroup object tags array */
    int32          *refs;	/* Pnt to Vgroup object refs array */
    int32           dum;	/* dummy variable */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           nObjects;	/* # of objects in Vgroup */
    int32           nSDS;	/* SDS counter */
    int32           l_index;	/* SDS l_index */
    int32           sdid;	/* SDS object ID */
    int32           idOffset = GDIDOFFSET;	/* Grid ID offset */

    char            name[80];	/* Vgroup name */
    char            class[80];	/* Vgroup class */
    char            errbuf[256];/* Buffer for error message */
    char            acsCode[1];	/* Read/Write l_access char: "r/w" */


    /* Check HDF-EOS file ID, get back HDF file ID and l_access code */
    /* ----------------------------------------------------------- */
    status = EHchkfid(fid, gridname, &HDFfid, &dum, &acs);


    if (status == 0)
    {
	/* Convert numeric l_access code to character */
	/* ---------------------------------------- */

	acsCode[0] = (acs == 1) ? 'w' : 'r';

	/* Determine number of grids currently opened */
	/* ------------------------------------------- */
	for (i = 0; i < NGRID; i++)
	{
	    ngridopen += GDXGrid[i].active;
	}


	/* If room for more ... */
	/* -------------------- */
	if (ngridopen < NGRID)
	{

	    /* Search Vgroups for Grid */
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
		VgetnameSafe(vgid[0], name, sizeof(name));
		Vgetclass(vgid[0], class);


		/*
		 * If Vgroup with gridname and class GRID found, load tables
		 */

		if (strcmp(name, gridname) == 0 &&
		    strcmp(class, "GRID") == 0)
		{
		    /* Attach to "Data Fields" and "Grid Attributes" Vgroups */
		    /* ----------------------------------------------------- */
		    tags = (int32 *) malloc(sizeof(int32) * 2);
		    if(tags == NULL)
		    {
			HEpush(DFE_NOSPACE,"GDattach", __FILE__, __LINE__);
			return(-1);
		    }
		    refs = (int32 *) malloc(sizeof(int32) * 2);
		    if(refs == NULL)
		    {
			HEpush(DFE_NOSPACE,"GDattach", __FILE__, __LINE__);
			free(tags);
			return(-1);
		    }
		    Vgettagrefs(vgid[0], tags, refs, 2);
		    vgid[1] = Vattach(HDFfid, refs[0], acsCode);
		    vgid[2] = Vattach(HDFfid, refs[1], acsCode);
		    free(tags);
		    free(refs);


		    /* Setup External Arrays */
		    /* --------------------- */
		    for (i = 0; i < NGRID; i++)
		    {
			/* Find empty entry in array */
			/* ------------------------- */
			if (GDXGrid[i].active == 0)
			{
			    /*
			     * Set gridID, Set grid entry active, Store root
			     * Vgroup ID, Store sub Vgroup IDs, Store HDF-EOS
			     * file ID
			     */
			    gridID = i + idOffset;
			    GDXGrid[i].active = 1;
			    GDXGrid[i].IDTable = vgid[0];
			    GDXGrid[i].VIDTable[0] = vgid[1];
			    GDXGrid[i].VIDTable[1] = vgid[2];
			    GDXGrid[i].fid = fid;
			    break;
			}
		    }

		    /* Get SDS interface ID */
		    /* -------------------- */
		    status = GDchkgdid(gridID, "GDattach", &dum,
				       &sdInterfaceID, &dum);
            if( status < 0)
                return -1;

		    /* Get # of entries within Data Vgroup & search for SDS */
		    /* ---------------------------------------------------- */
		    nObjects = Vntagrefs(vgid[1]);

		    if (nObjects > 0)
		    {
			/* Get tag and ref # for Data Vgroup objects */
			/* ----------------------------------------- */
			tags = (int32 *) malloc(sizeof(int32) * nObjects);
			if(tags == NULL)
			{
			    HEpush(DFE_NOSPACE,"GDattach", __FILE__, __LINE__);
			    return(-1);
			}
			refs = (int32 *) malloc(sizeof(int32) * nObjects);
			if(refs == NULL)
			{
			    HEpush(DFE_NOSPACE,"GDattach", __FILE__, __LINE__);
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
			GDXGrid[i].sdsID = (int32 *) calloc(nSDS, 4);
			if(GDXGrid[i].sdsID == NULL && nSDS != 0)
			{
			    HEpush(DFE_NOSPACE,"GDattach", __FILE__, __LINE__);
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
				GDXGrid[i].sdsID[nSDS] = sdid;
				nSDS++;
				GDXGrid[i].nSDS++;
			    }
			}
			free(tags);
			free(refs);
		    }
		    break;
		}

		/* Detach Vgroup if not desired Grid */
		/* --------------------------------- */
		Vdetach(vgid[0]);
	    }

	    /* If Grid not found then set up error message */
	    /* ------------------------------------------- */
	    if (gridID == -1)
	    {
		HEpush(DFE_RANGE, "GDattach", __FILE__, __LINE__);
		HEreport("Grid: \"%s\" does not exist within HDF file.\n",
			 gridname);
	    }
	}
	else
	{
	    /* Too many files opened */
	    /* --------------------- */
	    gridID = -1;
	    strcpy(errbuf,
		   "No more than %d grids may be open simultaneously");
	    strcat(errbuf, " (%s)");
	    HEpush(DFE_DENIED, "GDattach", __FILE__, __LINE__);
	    HEreport(errbuf, NGRID, gridname);
	}

    }
    return (gridID);
}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDchkgdid                                                        |
|                                                                             |
|  DESCRIPTION:                                                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               grid structure ID                       |
|  routname       char                Name of routine calling GDchkgdid       |
|                                                                             |
|  OUTPUTS:                                                                   |
|  fid            int32               File ID                                 |
|  sdInterfaceID  int32               SDS interface ID                        |
|  gdVgrpID       int32               grid Vgroup ID                          |
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
static intn
GDchkgdid(int32 gridID, const char *routname,
	  int32 * fid, int32 * sdInterfaceID, int32 * gdVgrpID)
{
    intn            status = 0;	/* routine return status variable */
    uint8           l_access;	/* Read/Write l_access code */
    int32           gID;	/* Grid ID - offset */

    int32           idOffset = GDIDOFFSET;	/* Grid ID offset */

    static const char message1[] =
        "Invalid grid id: %d in routine \"%s\".  ID must be >= %d and < %d.\n";
    static const char message2[] =
        "Grid id %d in routine \"%s\" not active.\n";



    /* Check for valid grid id */

    if (gridID < idOffset || gridID >= NGRID + idOffset)
    {
	status = -1;
	HEpush(DFE_RANGE, "GDchkgdid", __FILE__, __LINE__);
	HEreport(message1, gridID, routname, idOffset, NGRID + idOffset);
    }
    else
    {

	/* Compute "reduced" ID */
	/* -------------------- */
	gID = gridID % idOffset;

	/* Check for active grid ID */
	/* ------------------------ */
	if (GDXGrid[gID].active == 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "GDchkgdid", __FILE__, __LINE__);
	    HEreport(message2, gridID, routname);
	}
	else
	{

	    /* Get file & SDS ids and Grid key */
	    /* -------------------------------- */
	    status = EHchkfid(GDXGrid[gID].fid, " ",
			      fid, sdInterfaceID, &l_access);
	    *gdVgrpID = GDXGrid[gID].IDTable;
	}
    }
    return (status);

}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDdiminfo                                                        |
|                                                                             |
|  DESCRIPTION: Retrieve size of specified dimension.                         |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  size           int32               Size of dimension                       |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               grid structure id                       |
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
GDdiminfo(int32 gridID, const char *dimname)

{
    intn            status;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           gdVgrpID;	/* Grid root Vgroup ID */
    int32           size;	/* Dimension size */
    int32           idOffset = GDIDOFFSET;	/* Grid ID offset */


    char           *metabuf;	/* Pointer to structural metadata (SM) */
    char           *metaptrs[2];/* Pointers to begin and end of SM section */
    char            gridname[80];	/* Grid Name */
    char           *utlstr;	/* Utility string */

    /* Allocate space for utility string */
    /* --------------------------------- */
    utlstr = (char *) calloc(UTLSTR_MAX_SIZE, sizeof(char));
    if(utlstr == NULL)
    {
	HEpush(DFE_NOSPACE,"GDdiminfo", __FILE__, __LINE__);
	return(-1);
    }
    /* Initialize return value */
    /* ----------------------- */
    size = -1;


    /* Check Grid ID */
    /* ------------- */
    status = GDchkgdid(gridID, "GDdiminfo", &fid, &sdInterfaceID, &gdVgrpID);


    if (status == 0)
    {
	/* Get grid name */
	/* ------------- */
	int gID = gridID % idOffset;
	if (gID >= NGRID)
	{
	   free(utlstr);
	   return -1;
	}
	VgetnameSafe(GDXGrid[gID].IDTable, gridname, sizeof(gridname));


	/* Get pointers to "Dimension" section within SM */
	/* --------------------------------------------- */
	metabuf = (char *) EHmetagroup(sdInterfaceID, gridname, "g",
				       "Dimension", metaptrs);

	if(metabuf == NULL)
	{
	    free(utlstr);
	    return(-1);
	}

	/* Search for dimension name (surrounded by quotes) */
	/* ------------------------------------------------ */
	snprintf(utlstr, UTLSTR_MAX_SIZE, "%s%s%s", "\"", dimname, "\"\n");
	metaptrs[0] = strstr(metaptrs[0], utlstr);

	/*
	 * If dimension found within grid structure then get dimension value
	 */
	if (metaptrs[0] < metaptrs[1] && metaptrs[0] != NULL)
	{
	    /* Set endptr at end of dimension definition entry */
	    /* ----------------------------------------------- */
	    metaptrs[1] = strstr(metaptrs[0], "\t\t\tEND_OBJECT");

	    status = EHgetmetavalue(metaptrs, "Size", utlstr);

	    if (status == 0)
	    {
		size = atoi(utlstr);
	    }
	    else
	    {
		HEpush(DFE_GENAPP, "GDdiminfo", __FILE__, __LINE__);
		HEreport("\"Size\" string not found in metadata.\n");
	    }
	}
	else
	{
	    HEpush(DFE_GENAPP, "GDdiminfo", __FILE__, __LINE__);
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
|  FUNCTION: GDgridinfo                                                       |
|                                                                             |
|  DESCRIPTION: Returns xdim, ydim and location of upper left and lower       |
|                right corners, in meters.                                    |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  fid            int32               File ID                                 |
|  gridname       char                Grid structure name                     |
|                                                                             |
|  OUTPUTS:                                                                   |
|  xdimsize       int32               Number of columns in grid               |
|  ydimsize       int32               Number of rows in grid                  |
|  upleftpt       float64             Location (m/deg) of upper left corner   |
|  lowrightpt     float64             Location (m/deg) of lower right corner  |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Jan 97   Joel Gales    Check for metadata error status from EHgetmetavalue |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
GDgridinfo(int32 gridID, int32 * xdimsize, int32 * ydimsize,
	   float64 upleftpt[], float64 lowrightpt[])

{
    intn            status = 0;	/* routine return status variable */
    intn            statmeta = 0;	/* EHgetmetavalue return status */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           gdVgrpID;	/* Grid root Vgroup ID */
    int32           idOffset = GDIDOFFSET;	/* Grid ID offset */


    char           *metabuf;	/* Pointer to structural metadata (SM) */
    char           *metaptrs[2];/* Pointers to begin and end of SM section */
    char            gridname[80];	/* Grid Name */
    char           *utlstr;	/* Utility string */

    /* Allocate space for utility string */
    /* --------------------------------- */
    utlstr = (char *) calloc(UTLSTR_MAX_SIZE, sizeof(char));
    if(utlstr == NULL)
    {
	HEpush(DFE_NOSPACE,"GDgridinfo", __FILE__, __LINE__);
	return(-1);
    }
    /* Check Grid ID */
    /* ------------- */
    status = GDchkgdid(gridID, "GDgridinfo", &fid, &sdInterfaceID, &gdVgrpID);

    if (status == 0)
    {
	/* Get grid name */
	/* ------------- */
	int gID = gridID % idOffset;
	if (gID >= NGRID)
	{
	   free(utlstr);
	   return -1;
	}
	VgetnameSafe(GDXGrid[gID].IDTable, gridname, sizeof(gridname));


	/* Get pointers to grid structure section within SM */
	/* ------------------------------------------------ */
	metabuf = (char *) EHmetagroup(sdInterfaceID, gridname, "g",
				       NULL, metaptrs);

	if(metabuf == NULL)
	{
	    free(utlstr);
	    return(-1);
	}


	/* Get xdimsize if requested */
	/* ------------------------- */
	if (xdimsize != NULL)
	{
	    statmeta = EHgetmetavalue(metaptrs, "XDim", utlstr);
	    if (statmeta == 0)
	    {
		*xdimsize = atoi(utlstr);
	    }
	    else
	    {
		status = -1;
		HEpush(DFE_GENAPP, "GDgridinfo", __FILE__, __LINE__);
		HEreport("\"XDim\" string not found in metadata.\n");
	    }
	}


	/* Get ydimsize if requested */
	/* ------------------------- */
	if (ydimsize != NULL)
	{
	    statmeta = EHgetmetavalue(metaptrs, "YDim", utlstr);
	    if (statmeta == 0)
	    {
		*ydimsize = atoi(utlstr);
	    }
	    else
	    {
		status = -1;
		HEpush(DFE_GENAPP, "GDgridinfo", __FILE__, __LINE__);
		HEreport("\"YDim\" string not found in metadata.\n");
	    }
	}


	/* Get upleftpt if requested */
	/* ------------------------- */
	if (upleftpt != NULL)
	{
	    statmeta = EHgetmetavalue(metaptrs, "UpperLeftPointMtrs", utlstr);
	    if (statmeta == 0)
	    {
		/* If value is "DEFAULT" then return zeros */
		/* --------------------------------------- */
		if (strcmp(utlstr, "DEFAULT") == 0)
		{
		    upleftpt[0] = 0;
		    upleftpt[1] = 0;
		}
		else
		{
		    sscanf(utlstr, "(%lf,%lf)",
			   &upleftpt[0], &upleftpt[1]);
		}
	    }
	    else
	    {
		status = -1;
		HEpush(DFE_GENAPP, "GDgridinfo", __FILE__, __LINE__);
		HEreport(
		  "\"UpperLeftPointMtrs\" string not found in metadata.\n");
	    }

	}

	/* Get lowrightpt if requested */
	/* --------------------------- */
	if (lowrightpt != NULL)
	{
	    statmeta = EHgetmetavalue(metaptrs, "LowerRightMtrs", utlstr);
	    if (statmeta == 0)
	    {
		/* If value is "DEFAULT" then return zeros */
		if (strcmp(utlstr, "DEFAULT") == 0)
		{
		    lowrightpt[0] = 0;
		    lowrightpt[1] = 0;
		}
		else
		{
		    sscanf(utlstr, "(%lf,%lf)",
			   &lowrightpt[0], &lowrightpt[1]);
		}
	    }
	    else
	    {
		status = -1;
		HEpush(DFE_GENAPP, "GDgridinfo", __FILE__, __LINE__);
		HEreport(
		      "\"LowerRightMtrs\" string not found in metadata.\n");
	    }
	}

	free(metabuf);
    }
    free(utlstr);
    return (status);
}







/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDprojinfo                                                       |
|                                                                             |
|  DESCRIPTION: Returns GCTP projection code, zone code, spheroid code        |
|                and projection parameters.                                   |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               Grid structure ID                       |
|                                                                             |
|  OUTPUTS:                                                                   |
|  projcode       int32               GCTP projection code                    |
|  zonecode       int32               UTM zone code                           |
|  spherecode     int32               GCTP spheroid code                      |
|  projparm       float64             Projection parameters                   |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Oct 96   Joel Gales    Add check for no projection code                    |
|  Jan 97   Joel Gales    Check for metadata error status from EHgetmetavalue |
|  Jun 00   Abe Taaheri   Added support for EASE grid                         |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
GDprojinfo(int32 gridID, int32 * projcode, int32 * zonecode,
	   int32 * spherecode, float64 projparm[])

{
    intn            i;		/* Loop index */
    intn            projx;		/* Loop index */
    intn            status = 0;	/* routine return status variable */
    intn            statmeta = 0;	/* EHgetmetavalue return status */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           gdVgrpID;	/* Grid root Vgroup ID */
    int32           idOffset = GDIDOFFSET;	/* Grid ID offset */


    char           *metabuf;	/* Pointer to structural metadata (SM) */
    char           *metaptrs[2];/* Pointers to begin and end of SM section */
    char            gridname[80];	/* Grid Name */
    char           *utlstr;	/* Utility string */
    char            fmt[96];	/* Format String */

    /* Allocate space for utility string */
    /* --------------------------------- */
    utlstr = (char *) calloc(UTLSTR_MAX_SIZE, sizeof(char));
    if(utlstr == NULL)
    {
	HEpush(DFE_NOSPACE,"GDprojinfo", __FILE__, __LINE__);
	return(-1);
    }

    /* Check Grid ID */
    /* ------------- */
    status = GDchkgdid(gridID, "GDprojinfo", &fid, &sdInterfaceID, &gdVgrpID);

    if (status == 0)
    {
	/* Get grid name */
	/* ------------- */
	int gID = gridID % idOffset;
	if (gID >= NGRID)
	{
	   free(utlstr);
	   return -1;
	}

	VgetnameSafe(GDXGrid[gID].IDTable, gridname, sizeof(gridname));


	/* Get pointers to grid structure section within SM */
	/* ------------------------------------------------ */
	metabuf = (char *) EHmetagroup(sdInterfaceID, gridname, "g",
				       NULL, metaptrs);

	if(metabuf == NULL)
	{
	    free(utlstr);
	    return(-1);
	}


	/* Get projcode if requested */
	/* ------------------------- */
	if (projcode != NULL)
	{
	    *projcode = -1;

	    statmeta = EHgetmetavalue(metaptrs, "Projection", utlstr);
	    if (statmeta == 0)
	    {
		/* Loop through projection codes until found */
		/* ----------------------------------------- */
		for (projx = 0; Projections[projx].projcode != -1; projx++)
		    if (strcmp(utlstr, Projections[projx].projname) == 0)
			break;
		if (Projections[projx].projname != NULL)
		    *projcode = Projections[projx].projcode;
	    }
	    else
	    {
		status = -1;
		HEpush(DFE_GENAPP, "GDprojinfo", __FILE__, __LINE__);
		HEreport("Projection Code not defined for \"%s\".\n",
			 gridname);

		if (projparm != NULL)
		{
		    for (i = 0; i < 13; i++)
		    {
			projparm[i] = -1;
		    }
		}
	    }
	}


	/* Get zonecode if requested */
	/* ------------------------- */
	if (projcode && zonecode != NULL)
	{
	    *zonecode = -1;


	    /* Zone code only relevant for UTM and State Code projections */
	    /* ---------------------------------------------------------- */
	    if (*projcode == GCTP_UTM || *projcode == GCTP_SPCS)
	    {
		statmeta = EHgetmetavalue(metaptrs, "ZoneCode", utlstr);
		if (statmeta == 0)
		{
		    *zonecode = atoi(utlstr);
		}
		else
		{
		    status = -1;
		    HEpush(DFE_GENAPP, "GDprojinfo", __FILE__, __LINE__);
		    HEreport("Zone Code not defined for \"%s\".\n",
			     gridname);
		}
	    }
	}


	/* Get projection parameters if requested */
	/* -------------------------------------- */
	if (projcode && projparm != NULL)
	{

	    /*
	     * Note: No projection parameters for GEO, UTM, and State Code
	     * projections
	     */
	    if (*projcode == GCTP_GEO || *projcode == GCTP_UTM ||
		*projcode == GCTP_SPCS)
	    {
		for (i = 0; i < 13; i++)
		{
		    projparm[i] = 0.0;
		}

	    }
	    else
	    {
		statmeta = EHgetmetavalue(metaptrs, "ProjParams", utlstr);

		if (statmeta == 0)
		{

		    /* Build format string to read projection parameters */
		    /* ------------------------------------------------- */
		    strcpy(fmt, "%lf,");
		    for (i = 1; i <= 11; i++)
			strcat(fmt, "%lf,");
		    strcat(fmt, "%lf");


		    /* Read parameters from numeric list */
		    /* --------------------------------- */
		    sscanf(&utlstr[1], fmt,
			   &projparm[0], &projparm[1],
			   &projparm[2], &projparm[3],
			   &projparm[4], &projparm[5],
			   &projparm[6], &projparm[7],
			   &projparm[8], &projparm[9],
			   &projparm[10], &projparm[11],
			   &projparm[12]);
		}
		else
		{
		    status = -1;
		    HEpush(DFE_GENAPP, "GDprojinfo", __FILE__, __LINE__);
		    HEreport("Projection parameters not defined for \"%s\".\n",
			     gridname);

		}
	    }
	}


	/* Get spherecode if requested */
	/* --------------------------- */
	if (projcode && spherecode != NULL)
	{
	    *spherecode = 0;

	    /* Note: Spherecode not defined for GEO projection */
	    /* ----------------------------------------------- */
	    if ((*projcode != GCTP_GEO))
	    {
		EHgetmetavalue(metaptrs, "SphereCode", utlstr);
		if (statmeta == 0)
		{
		    *spherecode = atoi(utlstr);
		}
	    }
	}
	free(metabuf);

    }
    free(utlstr);
    return (status);
}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDfieldinfo                                                      |
|                                                                             |
|  DESCRIPTION: Retrieve information about a specific geolocation or data     |
|                field in the grid.                                           |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               grid structure id                       |
|  fieldname      char                name of field                           |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|  rank           int32               rank of field (# of dims)               |
|  dims           int32               field dimensions                        |
|  numbertype     int32               field number type                       |
|  dimlist        char                field dimension list                    |
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
|  Feb 99   Abe Taaheri   Changed memcpy to memmove to avoid overlapping      |
|                         problem when copying strings                        |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
GDfieldinfo(int32 gridID, const char *fieldname, int32 * rank, int32 dims[],
	    int32 * numbertype, char *dimlist)

{
    intn            i;		    /* Loop index */
    intn            status;	    /* routine return status variable */
    intn            statmeta = 0;   /* EHgetmetavalue return status */

    int32           fid;	    /* HDF-EOS file ID */
    int32           sdInterfaceID;  /* HDF SDS interface ID */
    int32           idOffset = GDIDOFFSET;  /* Grid ID offset */
    int32           ndims = 0;	    /* Number of dimensions */
    int32           slen[8];	    /* Length of each entry in parsed string */
    int32           dum;	    /* Dummy variable */
    int32           xdim = 0;	    /* X dim size */
    int32           ydim = 0;	    /* Y dim size */
    int32           sdid;	    /* SDS id */

    char           *metabuf;	    /* Pointer to structural metadata (SM) */
    char           *metaptrs[2];    /* Pointers to begin and end of SM section */
    char            gridname[80];   /* Grid Name */
    char           *utlstr;	    /* Utility string */
    char           *ptr[8];	    /* String pointers for parsed string */
    char            dimstr[64];	    /* Individual dimension entry string */


    /* Allocate space for utility string */
    /* --------------------------------- */
    utlstr = (char *) calloc(UTLSTR_MAX_SIZE, sizeof(char));
    if(utlstr == NULL)
    {
	HEpush(DFE_NOSPACE,"GDfieldinfo", __FILE__, __LINE__);
	return(-1);
    }
    *rank = -1;
    *numbertype = -1;

    status = GDchkgdid(gridID, "GDfieldinfo", &fid, &sdInterfaceID, &dum);

    if (status == 0)
    {
	int gID = gridID % idOffset;
	if (gID >= NGRID)
	{
	    free(utlstr);
	    return -1;
    }
	VgetnameSafe(GDXGrid[gID].IDTable, gridname, sizeof(gridname));

	metabuf = (char *) EHmetagroup(sdInterfaceID, gridname, "g",
				       "DataField", metaptrs);
	if(metabuf == NULL)
	{
	    free(utlstr);
	    return(-1);
	}

    if (!metaptrs[0])
    {
        free(utlstr);
        free(metabuf);
        return -1;
    }

	/* Search for field */
	snprintf(utlstr, UTLSTR_MAX_SIZE, "%s%s%s", "\"", fieldname, "\"\n");
	metaptrs[0] = strstr(metaptrs[0], utlstr);

	/* If field found ... */
	if (metaptrs[0] < metaptrs[1] && metaptrs[0] != NULL)
	{

	    /* Set endptr at end of dimension definition entry */
	    metaptrs[1] = strstr(metaptrs[0], "\t\t\tEND_OBJECT");

	    /* Get DataType string */
	    statmeta = EHgetmetavalue(metaptrs, "DataType", utlstr);

	    /* Convert to numbertype code */
	    if (statmeta == 0)
		*numbertype = EHnumstr(utlstr);
	    else
	    {
		status = -1;
		HEpush(DFE_GENAPP, "GDfieldinfo", __FILE__, __LINE__);
		HEreport("\"DataType\" string not found in metadata.\n");
	    }

	    /*
	     * Get DimList string and trim off leading and trailing parens
	     * "()"
	     */
	    statmeta = EHgetmetavalue(metaptrs, "DimList", utlstr);

	    if (statmeta == 0)
	    {
		const size_t len = strlen(utlstr);
		if (len >= 2 && utlstr[0] == '(' && utlstr[len-1] == ')')
		{
		    memmove(utlstr, utlstr + 1, len - 2);
		    utlstr[len - 2] = '\0';
		}

		/* Parse trimmed DimList string and get rank */
		ndims = EHparsestr(utlstr, ',', ptr, CPL_ARRAYSIZE(ptr), slen, CPL_ARRAYSIZE(slen));
        if (ndims < 0)
        {
            status = -1;
            HEpush(DFE_NOSPACE, "GDfieldinfo", __FILE__, __LINE__);
        }
		*rank = ndims;
	    }
	    else
	    {
		status = -1;
		HEpush(DFE_GENAPP, "GDfieldinfo", __FILE__, __LINE__);
		HEreport("\"DimList\" string not found in metadata.\n");
	    }


	    if (status == 0)
	    {
		status = GDgridinfo(gridID, &xdim, &ydim, NULL, NULL);

        dims[0] = -1;
		for (i = 0; i < ndims; i++)
		{
		    memcpy(dimstr, ptr[i] + 1, slen[i] - 2);
		    dimstr[slen[i] - 2] = 0;

		    if (strcmp(dimstr, "XDim") == 0)
		    {
			dims[i] = xdim;
		    }
		    else if (strcmp(dimstr, "YDim") == 0)
		    {
			dims[i] = ydim;
		    }
		    else
		    {
			dims[i] = GDdiminfo(gridID, dimstr);
		    }


		    if (dimlist != NULL)
		    {
			if (i == 0)
			{
			    dimlist[0] = 0;
			}

			if (i > 0)
			{
			    strcat(dimlist, ",");
			}
			strcat(dimlist, dimstr);
		    }
		}


		if (dims[0] == 0)
		{
		    status = GDSDfldsrch(gridID, sdInterfaceID, fieldname,
					 &sdid, &dum, &dum, &dum, dims,
					 &dum);
		}
	    }
	}

	free(metabuf);
    }

    if (*rank == -1)
    {
	status = -1;

	HEpush(DFE_GENAPP, "GDfieldinfo", __FILE__, __LINE__);
	HEreport("Fieldname \"%s\" not found.\n", fieldname);
    }
    free(utlstr);
    return (status);
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDSDfldsrch                                                      |
|                                                                             |
|  DESCRIPTION: Retrieves information from SDS fields                         |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               grid structure ID                       |
|  sdInterfaceID  int32               SD interface ID                         |
|  fieldname      char                field name                              |
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
GDSDfldsrch(int32 gridID, int32 sdInterfaceID, const char *fieldname,
            int32 * sdid, int32 * rankSDS, int32 * rankFld, int32 * offset,
            int32 dims[], int32 * solo)
{
    intn            i;		/* Loop index */
    intn            status = -1;/* routine return status variable */

    int32           gID;	/* GridID - offset */
    int32           idOffset = GDIDOFFSET;	/* Grid ID offset */
    int32           dum;	/* Dummy variable */
    int32           dums[128];	/* Dummy array */
    int32           attrIndex;	/* Attribute l_index */

    char            name[2048];	/* Merged-Field Names */
    char            gridname[80];	/* Grid Name */
    char           *utlstr;/* Utility string */
    char           *metabuf;	/* Pointer to structural metadata (SM) */
    char           *metaptrs[2];/* Pointers to begin and end of SM section */
#ifdef broken_logic
    char           *oldmetaptr;	/* Pointer within SM section */
    char           *metaptr;	/* Pointer within SM section */
#endif

    /* Allocate space for utility string */
    /* --------------------------------- */
    utlstr = (char *) calloc(UTLSTR_MAX_SIZE, sizeof(char));
    if(utlstr == NULL)
    {
	HEpush(DFE_NOSPACE,"GDSDfldsrch", __FILE__, __LINE__);
	return(-1);
    }
    /* Set solo flag to 0 (no) */
    /* ----------------------- */
    *solo = 0;


    /* Compute "reduced" grid ID */
    /* ------------------------- */
    gID = gridID % idOffset;
    if (gID >= NGRID)
    {
        free(utlstr);
        return -1;
    }

    /* Loop through all SDSs in grid */
    /* ----------------------------- */
    for (i = 0; i < GDXGrid[gID].nSDS; i++)
    {
	/* If active SDS ... */
	/* ----------------- */
	if (GDXGrid[gID].sdsID[i] != 0)
	{
	    /* Get SDS ID, name, rankSDS, and dimensions */
	    /* ----------------------------------------- */
	    *sdid = GDXGrid[gID].sdsID[i];
	    SDgetinfo(*sdid, name, rankSDS, dims, &dum, &dum);
	    *rankFld = *rankSDS;


	    /* If merged field ... */
	    /* ------------------- */
	    if (strstr(name, "MRGFLD_") == &name[0])
	    {
		/* Get grid name */
		/* ------------- */
		VgetnameSafe(GDXGrid[gID].IDTable, gridname, sizeof(gridname));


		/* Get pointers to "MergedFields" section within SM */
		/* ------------------------------------------------ */
		metabuf = (char *) EHmetagroup(sdInterfaceID, gridname, "g",
					       "MergedFields", metaptrs);
		if(metabuf == NULL)
		{
		    free(utlstr);
		    return(-1);
		}

#ifdef broken_logic
		/* Initialize metaptr to beg. of section */
		/* ------------------------------------- */
		metaptr = metaptrs[0];


		/* Store metaptr in order to recover */
		/* --------------------------------- */
		oldmetaptr = metaptr;


		/* Search for Merged field name */
		/* ---------------------------- */
		snprintf(utlstr, UTLSTR_MAX_SIZE, "%s%s%s", "MergedFieldName=\"",
			name, "\"\n");
		metaptr = strstr(metaptr, utlstr);


		/* If not found check for old metadata */
		/* ----------------------------------- */
		if (metaptr == NULL)
		{
		    snprintf(utlstr, UTLSTR_MAX_SIZE, "%s%s%s", "OBJECT=\"", name, "\"\n");
		    metaptr = strstr(oldmetaptr, utlstr);
		}
#endif

        /* Get field list and strip off leading and trailing quotes */
        /* -------------------------------------------------------- */
        if (EHgetmetavalue(metaptrs, "FieldList", name) == 0)
        {
            const size_t len = strlen(name);
            if (len >= 2 && name[0] == '"' && name[len-1] == '"')
            {
                memmove(name, name + 1, strlen(name) - 2);
                name[strlen(name) - 2] = 0;
            }
        }
        else
        {
            name[0] = '\0';
        }

		/* Search for desired field within merged field list */
		/* ------------------------------------------------- */
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
|  FUNCTION: GDwrrdfield                                                      |
|                                                                             |
|  DESCRIPTION: Writes/Reads fields                                           |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               grid structure ID                       |
|  fieldname      char                fieldname                               |
|  code           char                Write/Read code (w/r)                   |
|  start          int32               start array                             |
|  stride         int32               stride array                            |
|  edge           int32               edge array                              |
|  datbuf         void                data buffer for read                    |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|  datbuf         void                data buffer for write                   |
|                                                                             |
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
GDwrrdfield(int32 gridID, const char *fieldname, const char *code,
	    int32 start[], int32 stride[], int32 edge[], VOIDP datbuf)

{
    intn            i;		/* Loop index */
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           sdid;	/* SDS ID */
    int32           dum;	/* Dummy variable */
    int32           rankSDS;	/* Rank of SDS */
    int32           rankFld;	/* Rank of field */

    int32           offset[8] = {0};	/* I/O offset (start) */
    int32           incr[8];	/* I/O increment (stride) */
    int32           count[8];	/* I/O count (edge) */
    int32           dims[8];	/* Field/SDS dimensions */
    int32           mrgOffset = 0;	/* Merged field offset */
    int32           strideOne;	/* Strides = 1 flag */

    for (i = 0; i < 8; ++i)
        incr[i] = 1;

    /* Check for valid grid ID */
    /* ----------------------- */
    status = GDchkgdid(gridID, "GDwrrdfield", &fid, &sdInterfaceID, &dum);
    if (status < 0)
        return -1;

    /* Check that field exists */
    /* ----------------------- */
    status = GDfieldinfo(gridID, fieldname, &rankSDS, dims, &dum, NULL);


	if (status != 0)
	{
	    HEpush(DFE_GENAPP, "GDwrrdfield", __FILE__, __LINE__);
	    HEreport("Fieldname \"%s\" does not exist.\n", fieldname);
	    status = -1;

	}


    if (status == 0)
    {
        status = GDSDfldsrch(gridID, sdInterfaceID, fieldname, &sdid,
                             &rankSDS, &rankFld, &mrgOffset, dims, &dum);
        if (status < 0)
            return -1;


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
        }

    return (status);
}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDreadfield                                                      |
|                                                                             |
|  DESCRIPTION: Reads data from a grid field.                                 |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               grid structure ID                       |
|  fieldname      char                fieldname                               |
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
GDreadfield(int32 gridID, const char *fieldname,
	    int32 start[], int32 stride[], int32 edge[], VOIDP buffer)

{
    intn            status = 0;	/* routine return status variable */

    status = GDwrrdfield(gridID, fieldname, "r", start, stride, edge,
			 buffer);
    return (status);
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDwrrdattr                                                       |
|                                                                             |
|  DESCRIPTION:                                                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               grid structure ID                       |
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
GDwrrdattr(int32 gridID, const char *attrname, int32 numbertype, int32 count,
	   const char *wrcode, VOIDP datbuf)

{
    intn            status;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           attrVgrpID;	/* Grid attribute ID */
    int32           dum;	/* dummy variable */
    int32           idOffset = GDIDOFFSET;	/* Grid ID offset */


    /* Check Grid id */
    status = GDchkgdid(gridID, "GDwrrdattr", &fid, &dum, &dum);

    if (status == 0)
    {
	/* Perform Attribute I/O */
	/* --------------------- */
	int gID = gridID % idOffset;
	if (gID >= NGRID)
	{
		return -1;
	}
	attrVgrpID = GDXGrid[gID].VIDTable[1];
	status = EHattr(fid, attrVgrpID, attrname, numbertype, count,
			wrcode, datbuf);
    }
    return (status);
}

/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDreadattr                                                       |
|                                                                             |
|  DESCRIPTION: Reads attribute from a grid.                                  |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               grid structure ID                       |
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
GDreadattr(int32 gridID, const char *attrname, VOIDP datbuf)
{
    intn            status = 0;	/* routine return status variable */
    int32           dum = 0;	/* dummy variable */

    /* Call GDwrrdattr routine to read attribute */
    /* ----------------------------------------- */
    status = GDwrrdattr(gridID, attrname, dum, dum, "r", datbuf);

    return (status);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDattrinfo                                                       |
|                                                                             |
|  DESCRIPTION:                                                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               grid structure ID                       |
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
GDattrinfo(int32 gridID, const char *attrname, int32 * numbertype, int32 * count)
{
    intn            status = 0;	/* routine return status variable */

    int32           fid = 0;	/* HDF-EOS file ID */
    int32           attrVgrpID;	/* Grid attribute ID */
    int32           dum;	/* dummy variable */
    int32           idOffset = GDIDOFFSET;	/* Grid ID offset */

    status = GDchkgdid(gridID, "GDattrinfo", &fid, &dum, &dum);
    if (status < 0)
        return -1;

    int gID = gridID % idOffset;
    if (gID >= NGRID)
    {
        return -1;
    }
    attrVgrpID = GDXGrid[gID].VIDTable[1];

    status = EHattrinfo(fid, attrVgrpID, attrname, numbertype,
			count);

    return (status);
}






/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDinqattrs                                                       |
|                                                                             |
|  DESCRIPTION:                                                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  nattr          int32               Number of attributes in swath struct    |
|                                                                             |
|  INPUTS:                                                                    |
|  grid ID        int32               grid structure ID                       |
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
GDinqattrs(int32 gridID, char *attrnames, int32 * strbufsize)
{
    intn            status;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           attrVgrpID;	/* Grid attribute ID */
    int32           dum;	/* dummy variable */
    int32           nattr = 0;	/* Number of attributes */
    int32           idOffset = GDIDOFFSET;	/* Grid ID offset */


    /* Check Grid id */
    status = GDchkgdid(gridID, "GDinqattrs", &fid, &dum, &dum);

    if (status == 0)
    {
	int gID = gridID % idOffset;
	if (gID >= NGRID)
	{
	    return -1;
    }
	attrVgrpID = GDXGrid[gID].VIDTable[1];
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
|  FUNCTION: GDinqfields                                                      |
|                                                                             |
|  DESCRIPTION: Retrieve information about all data fields defined in a grid. |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  nFld           int32               Number of fields in swath               |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               grid structure ID                       |
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
int32
GDinqfields(int32 gridID, char *fieldlist, int32 rank[],
	    int32 numbertype[])
{
    intn            status;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           gdVgrpID;	/* Grid root Vgroup ID */
    int32           idOffset = GDIDOFFSET;	/* Grid ID offset */
    int32           nFld = 0;	/* Number of mappings */
    int32           slen[8];	/* String length array */

    char           *metabuf;	/* Pointer to structural metadata (SM) */
    char           *metaptrs[2];/* Pointers to begin and end of SM section */
    char            gridname[80];	/* Grid Name */
    char           *utlstr;/* Utility string */
    char           *ptr[8];	/* String pointer array */

    /* Allocate space for utility string */
    /* --------------------------------- */
    utlstr = (char *) calloc(UTLSTR_MAX_SIZE, sizeof(char));
    if(utlstr == NULL)
    {
	HEpush(DFE_NOSPACE,"GDinqfields", __FILE__, __LINE__);
	return(-1);
    }
    /* Check for valid grid id */
    /* ----------------------- */
    status = GDchkgdid(gridID, "GDinqfields", &fid, &sdInterfaceID, &gdVgrpID);
    if (status == 0)
    {

	/* If field names, ranks,  or number types desired ... */
	/* --------------------------------------------------- */
	if (fieldlist != NULL || rank != NULL || numbertype != NULL)
	{
	    /* Get grid name */
	    /* ------------- */
	    int gID = gridID % idOffset;
	    if (gID >= NGRID)
	    {
	       free(utlstr);
	       return -1;
	    }
	    VgetnameSafe(GDXGrid[gID].IDTable, gridname, sizeof(gridname));


	    /* Get pointers to "DataField" section within SM */
	    /* --------------------------------------------- */
	    metabuf = (char *) EHmetagroup(sdInterfaceID, gridname, "g",
					   "DataField", metaptrs);
	    if(metabuf == NULL)
	    {
		free(utlstr);
		return(-1);
	    }


	    /* If field names are desired then "clear" name buffer */
	    /* --------------------------------------------------- */
	    if (fieldlist != NULL)
	    {
		fieldlist[0] = 0;
	    }


	    /* Begin loop through mapping entries in metadata */
	    /* ---------------------------------------------- */
	    while (1)
	    {
		/* Search for OBJECT string */
		/* ------------------------ */
		metaptrs[0] = strstr(metaptrs[0], "\t\tOBJECT=");


		/* If found within "Data" Field metadata section .. */
		/* ------------------------------------------------ */
		if (metaptrs[0] < metaptrs[1] && metaptrs[0] != NULL)
		{
		    /* Get Fieldnames (if desired) */
		    /* --------------------------- */
		    if (fieldlist != NULL)
		    {
			/* Check 1st for old meta data then new */
			/* ------------------------------------ */
			EHgetmetavalue(metaptrs, "OBJECT", utlstr);

			/*
			 * If OBJECT value begins with double quote then old
			 * metadata, field name is OBJECT value. Otherwise
			 * search for "DataFieldName" string
			 */

			if (utlstr[0] != '"')
			{
			    strcpy(utlstr, "\t\t\t\t");
			    strcat(utlstr, "DataFieldName");
			    strcat(utlstr, "=");
			    metaptrs[0] = strstr(metaptrs[0], utlstr);
			    EHgetmetavalue(metaptrs, "DataFieldName", utlstr);
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
			rank[nFld] = EHparsestr(utlstr, ',', ptr, CPL_ARRAYSIZE(ptr), slen, CPL_ARRAYSIZE(slen));
            if (rank[nFld] < 0)
            {
                status = -1;
                break;
            }
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
    return (nFld);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDnentries                                                       |
|                                                                             |
|  DESCRIPTION: Returns number of entries and descriptive string buffer       |
|                size for a specified entity.                                 |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  nEntries       int32               Number of entries                       |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               grid structure ID                       |
|  entrycode      int32               Entry code                              |
|	                              HDFE_NENTDIM  (0)                       |
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
GDnentries(int32 gridID, int32 entrycode, int32 * strbufsize)

{
    intn            status;	/* routine return status variable */
    intn            i;		/* Loop index */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           gdVgrpID;	/* Grid root Vgroup ID */
    int32           idOffset = GDIDOFFSET;	/* Grid ID offset */
    int32           nEntries = 0;	/* Number of entries */
    int32           metaflag;	/* Old (0), New (1) metadata flag) */
    int32           nVal = 0;	/* Number of strings to search for */

    char           *metabuf = NULL;	/* Pointer to structural metadata (SM) */
    char           *metaptrs[2] = {NULL, NULL};/* Pointers to begin and end of SM section */
    char            gridname[80];	/* Grid Name */
    char           *utlstr;/* Utility string */
    char            valName[2][32];	/* Strings to search for */

    memset(valName, 0, sizeof(valName));

    /* Allocate space for utility string */
    /* --------------------------------- */
    utlstr = (char *) calloc(UTLSTR_MAX_SIZE, sizeof(char));
    if(utlstr == NULL)
    {
	HEpush(DFE_NOSPACE,"GDnentries", __FILE__, __LINE__);
	return(-1);
    }
    status = GDchkgdid(gridID, "GDnentries", &fid, &sdInterfaceID, &gdVgrpID);

    if (status == 0)
    {
	/* Get grid name */
	int gID = gridID % idOffset;
	if (gID >= NGRID)
	{
	   free(utlstr);
	   return -1;
	}

	VgetnameSafe(GDXGrid[gID].IDTable, gridname, sizeof(gridname));

	/* Zero out string buffer size */
	*strbufsize = 0;


	/*
	 * Get pointer to  relevant section within SM and Get names of
	 * metadata strings to inquire about
	 */
	switch (entrycode)
	{
	case HDFE_NENTDIM:
	    {
		metabuf = (char *) EHmetagroup(sdInterfaceID, gridname, "g",
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

	case HDFE_NENTDFLD:
	    {
		metabuf = (char *) EHmetagroup(sdInterfaceID, gridname, "g",
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

	if (!metabuf || metaptrs[0] == NULL)
    {
        free(metabuf);
        free(utlstr);
        return -1;
    }

	/*
	 * Check for presence of 'GROUP="' string If found then old metadata,
	 * search on OBJECT string
	 */
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
		    if( utlstr[0] == '"' && utlstr[strlen(utlstr)-1] == '"' )
		        *strbufsize += (int32)strlen(utlstr) - 2;
		    else
		        *strbufsize += (int32)strlen(utlstr);
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
    {
	nEntries = -1;
    }

    free(utlstr);
    return (nEntries);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDinqgrid                                                        |
|                                                                             |
|  DESCRIPTION: Returns number and names of grid structures in file           |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  nGrid          int32               Number of grid structures in file       |
|                                                                             |
|  INPUTS:                                                                    |
|  filename       char                HDF-EOS filename                        |
|                                                                             |
|  OUTPUTS:                                                                   |
|  gridlist       char                List of grid names (comma-separated)    |
|  strbufsize     int32               Length of gridlist                      |
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
GDinqgrid(const char *filename, char *gridlist, int32 * strbufsize)
{
    int32           nGrid;	/* Number of grid structures in file */

    /* Call "EHinquire" routine */
    /* ------------------------ */
    nGrid = EHinquire(filename, "GRID", gridlist, strbufsize);

    return (nGrid);
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDgetfillvalue                                                   |
|                                                                             |
|  DESCRIPTION: Retrieves fill value for a specified field.                   |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               grid structure ID                       |
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
GDgetfillvalue(int32 gridID, const char *fieldname, VOIDP fillval)
{
    intn            status;	/* routine return status variable */

    int32           nt;		/* Number type */
    int32           dims[8];	/* Dimensions array */
    int32           dum;	/* Dummy variable */

    char            name[80];	/* Fill value "attribute" name */

    status = GDchkgdid(gridID, "GDgetfillvalue", &dum, &dum, &dum);

    /* Check for valid grid ID */
    if (status == 0)
    {
	/* Get field info */
	status = GDfieldinfo(gridID, fieldname, &dum, dims, &nt, NULL);

	if (status == 0)
	{
	    /* Read fill value attribute */
	    strcpy(name, "_FV_");
	    strcat(name, fieldname);
	    status = GDreadattr(gridID, name, fillval);
	}
	else
	{
	    HEpush(DFE_GENAPP, "GDgetfillvalue", __FILE__, __LINE__);
	    HEreport("Fieldname \"%s\" does not exist.\n", fieldname);
	}

    }
    return (status);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDdetach                                                         |
|                                                                             |
|  DESCRIPTION: Detaches from grid interface and performs file housekeeping.  |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               grid structure ID                       |
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
|  Sep 96   Joel Gales    Setup dim names for SDsetdimname in dimbuf1 rather  |
|                         that utlstr                                         |
|  Oct 96   Joel Gales    Detach Grid Vgroups                                 |
|  Oct 96   Joel Gales    "Detach" from SDS                                   |
|  Nov 96   Joel Gales    Call GDchkgdid to check for proper grid ID          |
|  Dec 96   Joel Gales    Add multiple vertical subsetting garbage collection |
|  Oct 98   Abe Taaheri   Added GDXRegion[k]->DimNamePtr[i] =0; after freeing |
|                         memory                                              |
|  Sep 99   Abe Taaheri   Changed memcpy to memmove because of overlapping    |
|                         source and destination for GDXSDcomb, nameptr, and  |
|                         dimptr. memcpy may cause unexpected results.        |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
GDdetach(int32 gridID)

{
    intn            i;		/* Loop index */
    intn            k;		/* Loop index */
    intn            status = 0;	/* routine return status variable */

    int32           sdInterfaceID;	/* SDS interface ID */
    int32           gID;	/* Grid ID - offset */
    int32           idOffset = GDIDOFFSET;	/* Grid ID offset */
    int32           dum;	/* Dummy variable */

    char            gridname[VGNAMELENMAX + 1];	/* Grid name */


    status = GDchkgdid(gridID, "GDdetach", &dum, &sdInterfaceID, &dum);

    if (status == 0)
    {
	gID = gridID % idOffset;
	if (gID >= NGRID)
	{
	   return -1;
	}
	VgetnameSafe(GDXGrid[gID].IDTable, gridname, sizeof(gridname));


	/* "Detach" from previously attached SDSs */
	/* -------------------------------------- */
	for (k = 0; k < GDXGrid[gID].nSDS; k++)
	{
	    SDendaccess(GDXGrid[gID].sdsID[k]);
	}
	free(GDXGrid[gID].sdsID);
	GDXGrid[gID].sdsID = 0;
	GDXGrid[gID].nSDS = 0;



	/* Detach Grid Vgroups */
	/* ------------------- */
	Vdetach(GDXGrid[gID].VIDTable[0]);
	Vdetach(GDXGrid[gID].VIDTable[1]);
	Vdetach(GDXGrid[gID].IDTable);

	GDXGrid[gID].active = 0;
	GDXGrid[gID].VIDTable[0] = 0;
	GDXGrid[gID].VIDTable[1] = 0;
	GDXGrid[gID].IDTable = 0;
	GDXGrid[gID].fid = 0;




	/* Free Region Pointers */
	/* -------------------- */
	for (k = 0; k < NGRIDREGN; k++)
	{
	    if (GDXRegion[k] != 0 &&
		GDXRegion[k]->gridID == gridID)
	    {
		for (i = 0; i < 8; i++)
		{
		    if (GDXRegion[k]->DimNamePtr[i] != 0)
		    {
			free(GDXRegion[k]->DimNamePtr[i]);
			GDXRegion[k]->DimNamePtr[i] = 0;
		    }
		}

		free(GDXRegion[k]);
		GDXRegion[k] = 0;
	    }
	}
    }
    return (status);
}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDclose                                                          |
|                                                                             |
|  DESCRIPTION: Closes file.                                                  |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  fid            int32               File ID                                 |
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
GDclose(int32 fid)

{
    intn            status = 0;	/* routine return status variable */

    /* Call EHclose to perform file close */
    /* ---------------------------------- */
    status = EHclose(fid);

    return (status);
}

/***********************************************
GDwrrdtile --
     This function is the underlying function below GDwritetile and
     GDreadtile.


Author--
Alexis Zubrow

********************************************************/

static intn
GDwrrdtile(int32 gridID, const char *fieldname, const char *code, int32 start[],
	   VOIDP datbuf)
{
    intn            i;		/* Loop index */
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           sdid;	/* SDS ID */

    int32           dum;	/* Dummy variable */
    int32           rankSDS;	/* Rank of SDS/Field */

    int32           dims[8];	/* Field/SDS dimensions */
    int32           tileFlags;	/* flag to determine if field is tiled */
    int32           numTileDims;/* number of tiles spanning a dimension */
    HDF_CHUNK_DEF   tileDef;	/* union holding tiling info. */


    /* Get gridID */
    status = GDchkgdid(gridID, "GDwrrdtile", &fid, &sdInterfaceID, &dum);
    if (status == 0)
    {

	/* Get field info */
	status = GDfieldinfo(gridID, fieldname, &rankSDS, dims, &dum, NULL);

	if (status == 0)
	{

        /* Check whether fieldname is in SDS (multi-dim field) */
        /* --------------------------------------------------- */
        status = GDSDfldsrch(gridID, sdInterfaceID, fieldname, &sdid,
                 &rankSDS, &dum, &dum, dims, &dum);
        if (status < 0)
            return -1;


        /*
         * Check for errors in parameters passed to GDwritetile or
         * GDreadtile
         */

        /* Check if untiled field */
        status = SDgetchunkinfo(sdid, &tileDef, &tileFlags);
        if (tileFlags == HDF_NONE)
        {
            HEpush(DFE_GENAPP, "GDwrrdtile", __FILE__, __LINE__);
            HEreport("Field \"%s\" is not tiled.\n", fieldname);
            status = -1;
            return (status);

        }

	    /*
	     * Check if rd/wr tilecoords are within the extent of the field
	     */
	    for (i = 0; i < rankSDS; i++)
	    {
		/*
		 * Calculate the number of tiles which span a dimension of
		 * the field
		 */
		numTileDims = dims[i] / tileDef.chunk_lengths[i];
		if ((start[i] >= numTileDims) || (start[i] < 0))
		{
		    /*
		     * ERROR INDICATING BEYOND EXTENT OF THAT DIMENSION OR
		     * NEGATIVE TILECOORDS
		     */
		    HEpush(DFE_GENAPP, "GDwrrdtile", __FILE__, __LINE__);
		    HEreport("Tilecoords for dimension \"%d\" ...\n", i);
		    HEreport("is beyond the extent of dimension length\n");
		    status = -1;

		}
	    }

	    if (status == -1)
	    {
		return (status);
	    }


	    /* Actually write/read to the field */

	    if (strcmp(code, "w") == 0)	/* write tile */
	    {
		status = SDwritechunk(sdid, start, (VOIDP) datbuf);
	    }
	    else if (strcmp(code, "r") == 0)	/* read tile */
	    {
		status = SDreadchunk(sdid, start, (VOIDP) datbuf);
	    }


	}

	/* Non-existent fieldname */
	else
	{
	    HEpush(DFE_GENAPP, "GDwrrdtile", __FILE__, __LINE__);
	    HEreport("Fieldname \"%s\" does not exist.\n", fieldname);
	    status = -1;
	}

    }

    return (status);
}

/***********************************************
GDtileinfo --
     This function queries the field to determine if it is tiled.  If it is
     tile, one can retrieve some of the characteristics of the tiles.

Author--  Alexis Zubrow

********************************************************/


intn
GDtileinfo(int32 gridID, const char *fieldname, int32 * tilecode, int32 * tilerank,
	   int32 tiledims[])

{
    intn            i;		/* Loop index */
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           sdid;	/* SDS ID */

    int32           dum;	/* Dummy variable */
    int32           rankSDS;	/* Rank of SDS/Field/tile */

    int32           dims[8];	/* Field/SDS dimensions */
    int32           tileFlags;	/* flag to determine if field is tiled */
    HDF_CHUNK_DEF   tileDef;	/* union holding tiling info. */


    /* Check if improper gridID */
    status = GDchkgdid(gridID, "GDtileinfo", &fid, &sdInterfaceID, &dum);
    if (status == 0)
    {

	/* Get field info */
	status = GDfieldinfo(gridID, fieldname, &rankSDS, dims, &dum, NULL);

	if (status == 0)
	{

        /* Check whether fieldname is in SDS (multi-dim field) */
        /* --------------------------------------------------- */
        status = GDSDfldsrch(gridID, sdInterfaceID, fieldname, &sdid,
                 &rankSDS, &dum, &dum, dims, &dum);
        if (status < 0)
            return -1;

        /* Query field for tiling information */
        status = SDgetchunkinfo(sdid, &tileDef, &tileFlags);

	    /* If field is untiled, return untiled flag */
	    if (tileFlags == HDF_NONE)
	    {
		*tilecode = HDFE_NOTILE;
		return (status);
	    }

	    /* IF field is tiled or tiled with compression */
	    else if ((tileFlags == HDF_CHUNK) ||
		     (tileFlags == (HDF_CHUNK | HDF_COMP)))
	    {
		if (tilecode != NULL)
		{
		    *tilecode = HDFE_TILE;
		}
		if (tilerank != NULL)
		{
		    *tilerank = rankSDS;
		}
		if (tiledims != NULL)
		{
		    /* Assign size of tile dimensions */
		    for (i = 0; i < rankSDS; i++)
		    {
			tiledims[i] = tileDef.chunk_lengths[i];
		    }
		}
	    }
	}

	/* Non-existent fieldname */
	else
	{
	    HEpush(DFE_GENAPP, "GDtileinfo", __FILE__, __LINE__);
	    HEreport("Fieldname \"%s\" does not exist.\n", fieldname);
	    status = -1;
	}

    }
    return (status);
}

/***********************************************
GDreadtile --
     This function reads one tile from a particular field.


Author--
Alexis Zubrow

********************************************************/

intn
GDreadtile(int32 gridID, const char *fieldname, int32 tilecoords[],
	   VOIDP tileData)
{
    char            code[] = "r";	/* read tile code */
    intn            status = 0;	/* routine return status variable */

    status = GDwrrdtile(gridID, fieldname, code, tilecoords, tileData);

    return (status);
}

/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDsdid                                                           |
|                                                                             |
|  DESCRIPTION: Returns SD element ID for grid field                          |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               grid structure ID                       |
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
GDsdid(int32 gridID, const char *fieldname, int32 *sdid)
{
    intn            status;	        /* routine return status variable */
    int32           fid;	        /* HDF-EOS file ID */
    int32           sdInterfaceID;      /* HDF SDS interface ID */
    int32           dum;	        /* Dummy variable */
    int32           dims[H4_MAX_VAR_DIMS]; /* Field/SDS dimensions */

    status = GDchkgdid(gridID, "GDsdid", &fid, &sdInterfaceID, &dum);
    if (status != -1)
    {
        status = GDSDfldsrch(gridID, sdInterfaceID, fieldname,
                             sdid, &dum, &dum, &dum, dims, &dum);
    }

    return (status);
}
