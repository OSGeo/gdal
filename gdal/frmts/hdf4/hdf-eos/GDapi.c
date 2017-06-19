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

extern  void for_init(int32, int32, float64 *, int32, const char *, const char *, int32 *,
                      int32 (*for_trans[])());
extern  void inv_init(int32, int32, float64 *, int32, const char *, const char *, int32 *,
                      int32 (*inv_trans[])());

#define	GDIDOFFSET 4194304
#define SQUARE(x)       ((x) * (x))   /* x**2 */

static int32 GDXSDcomb[512*5];
static char  GDXSDname[HDFE_NAMBUFSIZE];
static char  GDXSDdims[HDFE_DIMBUFSIZE];


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

/* Compression Codes */
static const char * const HDFcomp[] = {
    "HDFE_COMP_NONE",
    "HDFE_COMP_RLE",
    "HDFE_COMP_NBIT",
    "HDFE_COMP_SKPHUFF",
    "HDFE_COMP_DEFLATE"
};

/* Origin Codes */
static const char * const originNames[] = {
    "HDFE_GD_UL",
    "HDFE_GD_UR",
    "HDFE_GD_LL",
    "HDFE_GD_LR"
};

/* Pixel Registration Codes */
static const char * const pixregNames[] = {
    "HDFE_CENTER",
    "HDFE_CORNER"
};

/* Grid Function Prototypes (internal routines) */
static intn GDchkgdid(int32, const char *, int32 *, int32 *, int32 *);
static intn GDSDfldsrch(int32, int32, const char *, int32 *, int32 *, 
                        int32 *, int32 *, int32 [], int32 *);
static intn GDwrrdfield(int32, const char *, const char *,
                        int32 [], int32 [], int32 [], VOIDP datbuf);
static intn GDwrrdattr(int32, const char *, int32, int32, const char *, VOIDP);
static intn GDll2ij(int32, int32, float64 [], int32, int32, int32, float64[],
                    float64[], int32, float64[], float64[], int32[], int32[],
                    float64[], float64[]);
static intn  GDgetdefaults(int32, int32, float64[], int32,
                           float64[], float64[]);
static intn GDtangentpnts(int32, float64[], float64[], float64[], float64[],
                          float64 [], int32 *);
static intn GDwrrdtile(int32, const char *, const char *, int32 [], VOIDP);
static intn GDll2mm_cea(int32,int32, int32, float64[], int32, int32,
                        float64[], float64[], int32, float64[],float64[],
                        float64[], float64[], float64 *, float64 *);

static intn GDmm2ll_cea(int32, int32, int32, float64[],	int32, int32,
                        float64[], float64[], int32, float64[], float64[],
                        float64[], float64[]);

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
|  FUNCTION: GDcreate                                                         |
|                                                                             |
|  DESCRIPTION: Creates a grid within the file.                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  gridID         int32               Grid structure ID                       |
|                                                                             |
|  INPUTS:                                                                    |
|  fid            int32               File ID                                 |
|  gridname       char                Grid structure name                     |
|  xdimsize       int32               Number of columns in grid               |
|  ydimsize       int32               Number of rows in grid                  |
|  upleftpt       float64             Location (m/deg) of upper left corner   |
|  lowrightpt     float64             Location (m/deg) of lower right corner  |
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
|  Aug 96   Joel Gales    Check grid name for ODL compliance                  |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
GDcreate(int32 fid, const char *gridname, int32 xdimsize, int32 ydimsize,
	 float64 upleftpt[], float64 lowrightpt[])
{
    intn            i;		/* Loop index */
    intn            ngridopen = 0;	/* # of grid structures open */
    intn            status = 0;	/* routine return status variable */

    uint8           l_access;	/* Read/Write file l_access code */

    int32           HDFfid;	/* HDF file id */
    int32           vgRef;	/* Vgroup reference number */
    int32           vgid[3];	/* Vgroup ID array */
    int32           gridID = -1;/* HDF-EOS grid ID */

    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           idOffset = GDIDOFFSET;	/* Grid ID offset */
    int32           nGrid = 0;	/* Grid counter */

    char            name[80];	/* Vgroup name */
    char            class[80];	/* Vgroup class */
    char            errbuf[256];/* Buffer for error message */
    char            utlbuf[1024];	/* Utility buffer */
    char            header[128];/* Structural metadata header string */
    char            footer[256];/* Structural metadata footer string */
    char            refstr1[128];	/* Upper left ref string (metadata) */
    char            refstr2[128];	/* Lower right ref string (metadata) */


    /*
     * Check HDF-EOS file ID, get back HDF file ID, SD interface ID  and
     * l_access code
     */
    status = EHchkfid(fid, gridname, &HDFfid, &sdInterfaceID, &l_access);


    /* Check gridname for length */
    /* ------------------------- */
    if ((intn) strlen(gridname) > VGNAMELENMAX)
    {
	status = -1;
	HEpush(DFE_GENAPP, "GDcreate", __FILE__, __LINE__);
	HEreport("Gridname \"%s\" must be less than %d characters.\n",
		 gridname, VGNAMELENMAX);
    }



    if (status == 0)
    {
	/* Determine number of grids currently opened */
	/* ------------------------------------------- */
	for (i = 0; i < NGRID; i++)
	{
	    ngridopen += GDXGrid[i].active;
	}


	/* Setup file interface */
	/* -------------------- */
	if (ngridopen < NGRID)
	{

	    /* Check that grid has not been previously opened */
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


		/* If GRID then increment # grid counter */
		/* ------------------------------------- */
		if (strcmp(class, "GRID") == 0)
		{
		    nGrid++;
		}


		/* If grid already exist, return error */
		/* ------------------------------------ */
		if (strcmp(name, gridname) == 0 &&
		    strcmp(class, "GRID") == 0)
		{
		    status = -1;
		    HEpush(DFE_GENAPP, "GDcreate", __FILE__, __LINE__);
		    HEreport("\"%s\" already exists.\n", gridname);
		    break;
		}
	    }


	    if (status == 0)
	    {
		/* Create Root Vgroup for Grid */
		/* ---------------------------- */
		vgid[0] = Vattach(HDFfid, -1, "w");


		/* Set Name and Class (GRID) */
		/* -------------------------- */
		Vsetname(vgid[0], gridname);
		Vsetclass(vgid[0], "GRID");



		/* Create Data Fields Vgroup */
		/* ------------------------- */
		vgid[1] = Vattach(HDFfid, -1, "w");
		Vsetname(vgid[1], "Data Fields");
		Vsetclass(vgid[1], "GRID Vgroup");
		Vinsert(vgid[0], vgid[1]);



		/* Create Attributes Vgroup */
		/* ------------------------ */
		vgid[2] = Vattach(HDFfid, -1, "w");
		Vsetname(vgid[2], "Grid Attributes");
		Vsetclass(vgid[2], "GRID Vgroup");
		Vinsert(vgid[0], vgid[2]);



		/* Establish Grid in Structural MetaData Block */
		/* -------------------------------------------- */
		snprintf(header, sizeof(header), "%s%d%s%s%s%s%d%s%s%d%s",
			"\tGROUP=GRID_", (int)(nGrid + 1),
			"\n\t\tGridName=\"", gridname, "\"\n",
			"\t\tXDim=", (int)xdimsize, "\n",
			"\t\tYDim=", (int)ydimsize, "\n");


		snprintf(footer, sizeof(footer), 
			"%s%s%s%s%s%s%s%d%s",
			"\t\tGROUP=Dimension\n",
			"\t\tEND_GROUP=Dimension\n",
			"\t\tGROUP=DataField\n",
			"\t\tEND_GROUP=DataField\n",
			"\t\tGROUP=MergedFields\n",
			"\t\tEND_GROUP=MergedFields\n",
			"\tEND_GROUP=GRID_", (int)(nGrid + 1), "\n");



		/* Build Ref point Col-Row strings */
		/* ------------------------------- */
		if (upleftpt == NULL ||
		    (upleftpt[0] == 0 && upleftpt[1] == 0 &&
		     lowrightpt[0] == 0 && lowrightpt[1] == 0))
		{
		    strcpy(refstr1, "DEFAULT");
		    strcpy(refstr2, "DEFAULT");
		}
		else
		{
		    CPLsnprintf(refstr1, sizeof(refstr1), "%s%f%s%f%s",
			    "(", upleftpt[0], ",", upleftpt[1], ")");

		    CPLsnprintf(refstr2, sizeof(refstr2), "%s%f%s%f%s",
			    "(", lowrightpt[0], ",", lowrightpt[1], ")");
		}

		snprintf(utlbuf, sizeof(utlbuf),
			"%s%s%s%s%s%s%s%s",
			header,
			"\t\tUpperLeftPointMtrs=", refstr1, "\n",
			"\t\tLowerRightMtrs=", refstr2, "\n",
			footer);

		status = EHinsertmeta(sdInterfaceID, "", "g", 1002L,
				      utlbuf, NULL);

	    }
	}
	else
	{
	    /* Too many files opened */
	    /* --------------------- */
	    status = -1;
	    strcpy(errbuf,
		   "No more than %d grids may be open simutaneously");
	    strcat(errbuf, " (%s)");
	    HEpush(DFE_DENIED, "GDcreate", __FILE__, __LINE__);
	    HEreport(errbuf, NGRID, gridname);
	}


	/* Assign gridID # & Load grid and GDXGrid.fid table entries */
	/* --------------------------------------------------------- */
	if (status == 0)
	{

	    for (i = 0; i < NGRID; i++)
	    {
		if (GDXGrid[i].active == 0)
		{
		    gridID = i + idOffset;
		    GDXGrid[i].active = 1;
		    GDXGrid[i].IDTable = vgid[0];
		    GDXGrid[i].VIDTable[0] = vgid[1];
		    GDXGrid[i].VIDTable[1] = vgid[2];
		    GDXGrid[i].fid = fid;
		    status = 0;
		    break;
		}
	    }

	}
    }
    return (gridID);
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
		Vgetname(vgid[0], name);
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
		   "No more than %d grids may be open simutaneously");
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
|  FUNCTION: GDdefdim                                                         |
|                                                                             |
|  DESCRIPTION: Defines a new dimension within the grid.                      |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               grid structure ID                       |
|  dimname        char                Dimension name to define                |
|  dim            int32               Dimension value                         |
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
GDdefdim(int32 gridID, const char *dimname, int32 dim)

{
    intn            status;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           gdVgrpID;	/* Grid root Vgroup ID */
    int32           idOffset = GDIDOFFSET;	/* Grid ID offset */

    char            gridname[80] /* Grid name */ ;


    /* Check for valid grid id */
    status = GDchkgdid(gridID, "GDdefinedim",
		       &fid, &sdInterfaceID, &gdVgrpID);


    /* Make sure dimension >= 0 */
    /* ------------------------ */
    if (dim < 0)
    {
	status = -1;
	HEpush(DFE_GENAPP, "GDdefdim", __FILE__, __LINE__);
	HEreport("Dimension value for \"%s\" less than zero: %d.\n",
		 dimname, dim);
    }


    /* Write Dimension to Structural MetaData */
    /* -------------------------------------- */
    if (status == 0)
    {
	Vgetname(GDXGrid[gridID % idOffset].IDTable, gridname);
        /* The cast to char* is somehow nasty since EHinsertmeta can modify */
        /* dimname, but not in this case since we call with metacode = 0 */
	status = EHinsertmeta(sdInterfaceID, gridname, "g", 0L,
			      (char*) dimname, &dim);
    }
    return (status);

}

/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDdefproj                                                        |
|                                                                             |
|  DESCRIPTION: Defines projection of grid.                                   |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               Grid structure ID                       |
|  projcode       int32               GCTP projection code                    |
|  zonecode       int32               UTM zone code                           |
|  spherecode     int32               GCTP spheriod code                      |
|  projparm       float64             Projection parameters                   |
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
|  Jun 00   Abe Taaheri   Added support for EASE grid                         |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
GDdefproj(int32 gridID, int32 projcode, int32 zonecode, int32 spherecode,
	  float64 projparm[])
{
    intn            i;		/* Loop index */
    intn	    projx;	/* Projection table l_index */
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           gdVgrpID;	/* Grid root Vgroup ID */
    int32           idOffset = GDIDOFFSET;	/* Grid ID offset */
    int32           slen;	/* String length */
    float64         EHconvAng();
    char            utlbuf[1024];	/* Utility Buffer */
    char            projparmbuf[512];	/* Projection parameter metadata
					 * string */
    char            gridname[80];	/* Grid Name */


    /* Check for valid grid id */
    /* ----------------------- */
    status = GDchkgdid(gridID, "GDdefproj", &fid, &sdInterfaceID, &gdVgrpID);

    if (status == 0)
    {
	/*
	 * If projection not GEO, UTM, or State Code build projection
	 * parameter string
	 */
	if (projcode != GCTP_GEO &&
	    projcode != GCTP_UTM &&
	    projcode != GCTP_SPCS)
	{

	    /* Begin projection parameter list with "(" */
	    strcpy(projparmbuf, "(");

	    for (i = 0; i < 13; i++)
	    {
		/* If projparm[i] = 0 ... */
		if (projparm[i] == 0.0)
		{
		    strcpy(utlbuf, "0,");
		}
		else
		{
		    /* if projparm[i] is integer ... */
		    if ((int32) projparm[i] == projparm[i])
		    {
			snprintf(utlbuf, sizeof(utlbuf), "%d%s",
				(int) projparm[i], ",");
		    }
		    /* else projparm[i] is non-zero floating point ... */
		    else
		    {
			CPLsnprintf(utlbuf, sizeof(utlbuf), "%f%s",	projparm[i], ",");
		    }
		}
		strcat(projparmbuf, utlbuf);
	    }
	    slen = (int)strlen(projparmbuf);

	    /* Add trailing ")" */
	    projparmbuf[slen - 1] = ')';
	}

	for (projx = 0; Projections[projx].projcode != -1; projx++)
	  {
	    if (projcode == Projections[projx].projcode)
	      {
		break;
	      }
	  }


	/* Build metadata string */
	/* --------------------- */
	if (projcode == GCTP_GEO)
	{
	    snprintf(utlbuf, sizeof(utlbuf),
		    "%s%s%s",
		    "\t\tProjection=", Projections[projx].projname, "\n");
	}
	else if (projcode == GCTP_UTM || projcode == GCTP_SPCS)
	{
	    snprintf(utlbuf, sizeof(utlbuf),
		    "%s%s%s%s%d%s%s%d%s",
		    "\t\tProjection=", Projections[projx].projname, "\n",
		    "\t\tZoneCode=", (int)zonecode, "\n",
		    "\t\tSphereCode=", (int)spherecode, "\n");
	}
	else
	{
	    snprintf(utlbuf, sizeof(utlbuf),
		    "%s%s%s%s%s%s%s%d%s",
		    "\t\tProjection=", Projections[projx].projname, "\n",
		    "\t\tProjParams=", projparmbuf, "\n",
		    "\t\tSphereCode=", (int)spherecode, "\n");
	}


	/* Insert in structural metadata */
	/* ----------------------------- */
	Vgetname(GDXGrid[gridID % idOffset].IDTable, gridname);
	status = EHinsertmeta(sdInterfaceID, gridname, "g", 101L,
			      utlbuf, NULL);
    }

    return (status);
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDblkSOMoffset                                                   |
|                                                                             |
|  DESCRIPTION: Writes Block SOM offset values                                |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               Grid structure ID                       |
|  offset         float32             Offset values                           |
|  count          int32               Number of offset values                 |
|  code           char                w/r code (w/r)                          |
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
|  Mar 99   David Wynne   Changed data type of offset array from int32 to     |
|                         float32, NCR 21197                                  |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
GDblkSOMoffset(int32 gridID, float32 offset[], int32 count, const char *code)
{
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           gdVgrpID;	/* Grid root Vgroup ID */
    int32           idOffset = GDIDOFFSET;	/* Grid ID offset */
    int32           projcode;	/* GCTP projection code */

    float64         projparm[13];	/* Projection parameters */

    char            utlbuf[128];/* Utility Buffer */
    char            gridname[80];	/* Grid Name */

    /* Check for valid grid id */
    status = GDchkgdid(gridID, "GDblkSOMoffset",
		       &fid, &sdInterfaceID, &gdVgrpID);

    if (status == 0)
    {
	/* Get projection parameters */
	status = GDprojinfo(gridID, &projcode, NULL, NULL, projparm);

	/* If SOM projection with projparm[11] non-zero ... */
	if (projcode == GCTP_SOM && projparm[11] != 0)
	{
	    Vgetname(GDXGrid[gridID % idOffset].IDTable, gridname);
	    snprintf(utlbuf, sizeof(utlbuf),"%s%s", "_BLKSOM:", gridname);

	    /* Write offset values as attribute */
	    if (strcmp(code, "w") == 0)
	    {
		status = GDwriteattr(gridID, utlbuf, DFNT_FLOAT32,
				     count, offset);
	    }
	    /* Read offset values from attribute */
	    else if (strcmp(code, "r") == 0)
	    {
		status = GDreadattr(gridID, utlbuf, offset);
	    }
	}
    }
    return (status);
}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDdefcomp                                                        |
|                                                                             |
|  DESCRIPTION: Defines compression type and parameters                       |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               grid structure ID                       |
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
GDdefcomp(int32 gridID, int32 compcode, intn compparm[])
{
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           gdVgrpID;	/* Grid root Vgroup ID */
    int32           idOffset = GDIDOFFSET;	/* Grid ID offset */
    int32           gID;	/* gridID - offset */

    /* Check for valid grid id */
    status = GDchkgdid(gridID, "GDdefcomp", &fid, &sdInterfaceID, &gdVgrpID);

    if (status == 0)
    {
	gID = gridID % idOffset;

	/* Set compression code in compression external array */
	GDXGrid[gID].compcode = compcode;

	switch (compcode)
	{
	    /* Set NBIT compression parameters in compression external array */
	case HDFE_COMP_NBIT:

	    GDXGrid[gID].compparm[0] = compparm[0];
	    GDXGrid[gID].compparm[1] = compparm[1];
	    GDXGrid[gID].compparm[2] = compparm[2];
	    GDXGrid[gID].compparm[3] = compparm[3];

	    break;

	    /* Set GZIP compression parameter in compression external array */
	case HDFE_COMP_DEFLATE:

	    GDXGrid[gID].compparm[0] = compparm[0];

	    break;

	}
    }

    return (status);
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDdeftile                                                        |
|                                                                             |
|  DESCRIPTION: Defines tiling parameters                                     |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               grid structure ID                       |
|  tilecode       int32               tile code                               |
|  tilerank       int32               number of tiling dimensions             |
|  tiledims       int32               tiling dimensions                       |
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
intn
GDdeftile(int32 gridID, int32 tilecode, int32 tilerank, int32 tiledims[])
{
    intn            i;		/* Loop index */
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           gdVgrpID;	/* Grid root Vgroup ID */
    int32           idOffset = GDIDOFFSET;	/* Grid ID offset */
    int32           gID;	/* gridID - offset */

    /* Check for valid grid id */
    /* ----------------------- */
    status = GDchkgdid(gridID, "GDdeftile", &fid, &sdInterfaceID, &gdVgrpID);


    if (status == 0)
    {
	gID = gridID % idOffset;

	for (i = 0; i < 8; i++)
	{
	    GDXGrid[gID].tiledims[i] = 0;
	}

	GDXGrid[gID].tilecode = tilecode;

	switch (tilecode)
	{
	case HDFE_NOTILE:

	    GDXGrid[gID].tilerank = 0;

	    break;


	case HDFE_TILE:

	    GDXGrid[gID].tilerank = tilerank;

	    for (i = 0; i < tilerank; i++)
	    {
		GDXGrid[gID].tiledims[i] = tiledims[i];

		if (GDXGrid[gID].tiledims[i] == 0)
		{
		    GDXGrid[gID].tiledims[i] = 1;
		}
	    }

	    break;

	}
    }

    return (status);
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDdeforigin                                                      |
|                                                                             |
|  DESCRIPTION: Defines the origin of the grid data.                          |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               grid structure ID                       |
|  origincode     int32               origin code                             |
|                                     HDFE_GD_UL (0)                          |
|                                     HDFE_GD_UR (1)                          |
|			              HDFE_GD_LL (2)                          |
|                                     HDFE_GD_LR (3)                          |
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
GDdeforigin(int32 gridID, int32 origincode)
{
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           gdVgrpID;	/* Grid root Vgroup ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           idOffset = GDIDOFFSET;	/* Grid ID offset */

    char            utlbuf[64];	/* Utility buffer */
    char            gridname[80];	/* Grid name */


    /* Check for valid grid id */
    status = GDchkgdid(gridID, "GDdeforigin",
		       &fid, &sdInterfaceID, &gdVgrpID);

    if (status == 0)
    {
	/* If proper origin code then write to structural metadata */
	/* ------------------------------------------------------- */
	if (origincode >= 0 && origincode < (int32)sizeof(originNames))
	{
	    snprintf(utlbuf, sizeof(utlbuf),"%s%s%s",
		    "\t\tGridOrigin=", originNames[origincode], "\n");

	    Vgetname(GDXGrid[gridID % idOffset].IDTable, gridname);
	    status = EHinsertmeta(sdInterfaceID, gridname, "g", 101L,
				  utlbuf, NULL);
	}
	else
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "GDdeforigin", __FILE__, __LINE__);
	    HEreport("Improper Grid Origin code: %d\n", origincode);
	}
    }

    return (status);
}

/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDdefpixreg                                                      |
|                                                                             |
|  DESCRIPTION: Defines pixel registration within grid cell.                  |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               grid structure ID                       |
|  pixregcode     int32               Pixel registration code                 |
|                                     HDFE_CENTER (0)                         |
|                                     HDFE_CORNER (1)                         |
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
GDdefpixreg(int32 gridID, int32 pixregcode)
{
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           gdVgrpID;	/* Grid root Vgroup ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           idOffset = GDIDOFFSET;	/* Grid ID offset */

    char            utlbuf[64];	/* Utility buffer */
    char            gridname[80];	/* Grid name */

    /* Check for valid grid id */
    status = GDchkgdid(gridID, "GDdefpixreg",
		       &fid, &sdInterfaceID, &gdVgrpID);

    if (status == 0)
    {
	/* If proper pix reg code then write to structural metadata */
	/* -------------------------------------------------------- */
	if (pixregcode >= 0 && pixregcode < (int32)sizeof(pixregNames))
	{
	    snprintf(utlbuf, sizeof(utlbuf),"%s%s%s",
		    "\t\tPixelRegistration=", pixregNames[pixregcode], "\n");

	    Vgetname(GDXGrid[gridID % idOffset].IDTable, gridname);
	    status = EHinsertmeta(sdInterfaceID, gridname, "g", 101L,
				  utlbuf, NULL);
	}
	else
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "GDdefpixreg", __FILE__, __LINE__);
	    HEreport("Improper Pixel Registration code: %d\n", pixregcode);
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
	Vgetname(GDXGrid[gridID % idOffset].IDTable, gridname);


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
	Vgetname(GDXGrid[gridID % idOffset].IDTable, gridname);


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
|  spherecode     int32               GCTP spheriod code                      |
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
	Vgetname(GDXGrid[gridID % idOffset].IDTable, gridname);


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
	if (zonecode != NULL)
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
	if (projparm != NULL)
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
	if (spherecode != NULL)
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
|  FUNCTION: GDorigininfo                                                     |
|                                                                             |
|  DESCRIPTION: Returns origin code                                           |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               Grid structure ID                       |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|  origincode     int32               grid origin code                        |
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
GDorigininfo(int32 gridID, int32 * origincode)
{
    intn            i;		/* Loop index */
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
	HEpush(DFE_NOSPACE,"GDorigininfo", __FILE__, __LINE__);
	return(-1);
    }
    /* Check Grid ID */
    /* ------------- */
    status = GDchkgdid(gridID, "GDorigininfo",
		       &fid, &sdInterfaceID, &gdVgrpID);


    /* Initialize pixreg code to -1 (in case of error) */
    /* ----------------------------------------------- */
    *origincode = -1;

    if (status == 0)
    {
	/* Set default origin code */
	/* ----------------------- */
	*origincode = 0;


	/* Get grid name */
	/* ------------- */
	Vgetname(GDXGrid[gridID % idOffset].IDTable, gridname);


	/* Get pointers to grid structure section within SM */
	/* ------------------------------------------------ */
	metabuf = (char *) EHmetagroup(sdInterfaceID, gridname, "g",
				       NULL, metaptrs);

	if(metabuf == NULL)
	{
	    free(utlstr);
	    return(-1);
	}  


	statmeta = EHgetmetavalue(metaptrs, "GridOrigin", utlstr);

	if (statmeta == 0)
	{
	    /*
	     * If "GridOrigin" string found in metadata then convert to
	     * numeric origin code (fixed added: Jan 97)
	     */
	    for (i = 0; i < (intn)sizeof(originNames); i++)
	    {
		if (strcmp(utlstr, originNames[i]) == 0)
		{
		    *origincode = i;
		    break;
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
|  FUNCTION: GDpixreginfo                                                     |
|                                                                             |
|  DESCRIPTION:                                                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               Grid structure ID                       |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|  pixregcode     int32               Pixel registration code                 |
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
|  Jan 97   Joel Gales    Check for metadata error status from EHgetmetavalue |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
GDpixreginfo(int32 gridID, int32 * pixregcode)
{
    intn            i;		/* Loop index */
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
	HEpush(DFE_NOSPACE,"GDpixreginfo", __FILE__, __LINE__);
	return(-1);
    }
    /* Check Grid ID */
    status = GDchkgdid(gridID, "GDpixreginfo",
		       &fid, &sdInterfaceID, &gdVgrpID);

    /* Initialize pixreg code to -1 (in case of error) */
    *pixregcode = -1;

    if (status == 0)
    {
	/* Set default pixreg code */
	*pixregcode = 0;

	/* Get grid name */
	Vgetname(GDXGrid[gridID % idOffset].IDTable, gridname);

	/* Get pointers to grid structure section within SM */
	metabuf = (char *) EHmetagroup(sdInterfaceID, gridname, "g",
				       NULL, metaptrs);

	if(metabuf == NULL)
	{
	    free(utlstr);
	    return(-1);
	}  


	statmeta = EHgetmetavalue(metaptrs, "PixelRegistration", utlstr);

	if (statmeta == 0)
	{
	    /*
	     * If "PixelRegistration" string found in metadata then convert
	     * to numeric origin code (fixed added: Jan 97)
	     */

	    for (i = 0; i < (intn)sizeof(pixregNames); i++)
	    {
		if (strcmp(utlstr, pixregNames[i]) == 0)
		{
		    *pixregcode = i;
		    break;
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
|  FUNCTION: GDcompinfo                                                       |
|                                                                             |
|  DESCRIPTION:                                                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                                                        |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32                                                       |
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
GDcompinfo(int32 gridID, const char *fieldname, int32 * compcode, intn compparm[])
{
    intn            i;		/* Loop index */
    intn            status = 0;	/* routine return status variable */
    intn            statmeta = 0;	/* EHgetmetavalue return status */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           gdVgrpID;	/* Grid root Vgroup ID */
    int32           idOffset = GDIDOFFSET;	/* Grid ID offset */


    char           *metabuf;	/* Pointer to structural metadata (SM) */
    char           *metaptrs[2];/* Pointers to begin and end of SM section */
    char            gridname[80];	/* Grid Name */
    char           *utlstr;/* Utility string */

    /* Allocate space for utility string */
    /* --------------------------------- */
    utlstr = (char *) calloc(UTLSTR_MAX_SIZE, sizeof(char));
    if(utlstr == NULL)
    { 
	HEpush(DFE_NOSPACE,"GDcompinfo", __FILE__, __LINE__);
	return(-1);
    }
    /* Check Grid ID */
    status = GDchkgdid(gridID, "GDcompinfo", &fid, &sdInterfaceID, &gdVgrpID);


    if (status == 0)
    {
	/* Get grid name */
	Vgetname(GDXGrid[gridID % idOffset].IDTable, gridname);

	/* Get pointers to "DataField" section within SM */
	metabuf = (char *) EHmetagroup(sdInterfaceID, gridname, "g",
				       "DataField", metaptrs);
	if(metabuf == NULL)
	{
	    free(utlstr);
	    return(-1);
	}  


	/* Search for field */
	snprintf(utlstr, UTLSTR_MAX_SIZE, "%s%s%s", "\"", fieldname, "\"\n");
	metaptrs[0] = strstr(metaptrs[0], utlstr);


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
		    for (i = 0; i < (intn)sizeof(HDFcomp); i++)
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
			HEpush(DFE_GENAPP, "GDcompinfo", __FILE__, __LINE__);
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
			HEpush(DFE_GENAPP, "GDcompinfo", __FILE__, __LINE__);
			HEreport(
			"\"DeflateLevel\" string not found in metadata.\n");
		    }
		}
	    }
	}
	else
	{
	    HEpush(DFE_GENAPP, "GDcompinfo", __FILE__, __LINE__);
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
    int32           xdim;	    /* X dim size */
    int32           ydim;	    /* Y dim size */
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

	Vgetname(GDXGrid[gridID % idOffset].IDTable, gridname);

	metabuf = (char *) EHmetagroup(sdInterfaceID, gridname, "g",
				       "DataField", metaptrs);
	if(metabuf == NULL)
	{
	    free(utlstr);
	    return(-1);
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
		memmove(utlstr, utlstr + 1, strlen(utlstr) - 2);
		utlstr[strlen(utlstr) - 2] = 0;

		/* Parse trimmed DimList string and get rank */
		ndims = EHparsestr(utlstr, ',', ptr, slen);
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
|  FUNCTION: GDdeffield                                                       |
|                                                                             |
|  DESCRIPTION: Defines a new data field within the grid.                     |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               grid structure ID                       |
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
|  Aug 96   Joel Gales    Check name for ODL compliance                       |
|  Sep 96   Joel Gales    Make string array "dimbuf" dynamic                  |
|  Sep 96   Joel Gales    Add support for Block SOM (MISR)                    |
|  Jan 97   Joel Gales    Add support for tiling                              |
|  Feb 99   Abe Taaheri   Changed strcpy to memmove to avoid overlapping      |
|                         problem when copying strings                        |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
GDdeffield(int32 gridID, const char *fieldname, const char *dimlist,
	   int32 numbertype, int32 merge)

{
    intn            i;		/* Loop index */
    intn            status;	/* routine return status variable */
    intn            found;	/* utility found flag */
    intn            foundNT = 0;/* found number type flag */
    intn            foundAllDim = 1;	/* found all dimensions flag */
    intn            first = 1;	/* first entry flag */

    int32           fid;	/* HDF-EOS file ID */
    int32           vgid;	/* Geo/Data field Vgroup ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           sdid;	/* SDS object ID */
    int32           dimid;	/* SDS dimension ID */
    int32           gdVgrpID;	/* Grid root Vgroup ID */
    int32           dims[8];	/* Dimension size array */
    int32           dimsize;	/* Dimension size */
    int32           rank = 0;	/* Field rank */
    int32           slen[32];	/* String length array */
    int32           idOffset = GDIDOFFSET;	/* Grid ID offset */
    int32           compcode;	/* Compression code */
    int32           tilecode;	/* Tiling code */
    int32           chunkFlag;	/* Chunking (Tiling) flag */
    int32           gID;	/* GridID - offset */
    int32           xdim;	/* Grid X dimension */
    int32           ydim;	/* Grid Y dimension */
    int32           projcode;	/* Projection Code */

    float64         projparm[13];	/* Projection Parameters */

    char           *dimbuf;	/* Dimension buffer */
    char           *dimlist0;	/* Auxiliary dimension list */
    char           *comma;	/* Pointer to comma */
    char           *dimcheck;	/* Dimension check buffer */
    char            utlbuf[512];/* Utility buffer */
    char            utlbuf2[256];	/* Utility buffer 1 */
    char           *ptr[32];	/* String pointer array */
    char            gridname[80];	/* Grid name */
    char            parmbuf[128];	/* Parameter string buffer */
    char            errbuf1[128];	/* Error buffer 1 */
    char            errbuf2[128];	/* Error buffer 2 */
    static const char errmsg1[] = "Dimension: %d (size: %d) not divisible by ";
    /* Tiling  error message part 1 */
    static const char errmsg2[] = "tile dimension (size:  %d).\n";
    /* Tiling  error message part 2 */
    char            errmsg[128];/* Tiling error message */

    /* Valid number types */
    static const uint16 good_number[10] = {
        3, 4, 5, 6, 20, 21, 22, 23, 24, 25
    };

    comp_info       c_info;	/* Compression parameter structure */

    HDF_CHUNK_DEF   chunkDef;	/* Tiling structure */

    memset(&c_info, 0, sizeof(c_info));
    memset(&chunkDef, 0, sizeof(chunkDef));


    /* Setup error message strings */
    /* --------------------------- */
    strcpy(errbuf1, "GDXSDname array too small.\nPlease increase ");
    strcat(errbuf1, "size of HDFE_NAMBUFSIZE in \"HdfEosDef.h\".\n");
    strcpy(errbuf2, "GDXSDdims array too small.\nPlease increase ");
    strcat(errbuf2, "size of HDFE_DIMBUFSIZE in \"HdfEosDef.h\".\n");


    /* Build tiling dimension error message */
    /* ------------------------------------ */
    strcpy(errmsg, errmsg1);
    strcat(errmsg, errmsg2);

    /*
     * Check for proper grid ID and return HDF-EOS file ID, SDinterface ID,
     * and grid root Vgroup ID
     */
    status = GDchkgdid(gridID, "GDdefinefield",
		       &fid, &sdInterfaceID, &gdVgrpID);


    if (status == 0)
    {
	/* Remove offset from grid ID & get gridname */
	gID = gridID % idOffset;
	Vgetname(GDXGrid[gID].IDTable, gridname);


	/* Allocate space for dimension buffer and auxiliary dimension list */
	/* ---------------------------------------------------------------- */
	dimbuf = (char *) calloc(strlen(dimlist) + 64, 1);
	if(dimbuf == NULL)
	{ 
	    HEpush(DFE_NOSPACE,"GDdeffield", __FILE__, __LINE__);
	    return(-1);
	}
	dimlist0 = (char *) calloc(strlen(dimlist) + 64, 1);
	if(dimlist0 == NULL)
	{ 
	    HEpush(DFE_NOSPACE,"GDdeffield", __FILE__, __LINE__);
	    free(dimbuf);
	    return(-1);
	}


	/* Get Grid and Projection info */
	/* ---------------------------- */
	status = GDgridinfo(gridID, &xdim, &ydim, NULL, NULL);
	status = GDprojinfo(gridID, &projcode, NULL, NULL, projparm);


	/* Setup Block Dimension if "Blocked" SOM projection */
	/* ------------------------------------------------- */
	if (projcode == GCTP_SOM && (int32) projparm[11] != 0)
	{
	    dimsize = GDdiminfo(gridID, "SOMBlockDim");

	    /* If "SOMBlockDim" not yet defined then do it */
	    if (dimsize == -1)
	    {
		GDdefdim(gridID, "SOMBlockDim", (int32) projparm[11]);
	    }

	    /* If not 1D field then prepend to dimension list */
	    if (strchr(dimlist, ',') != NULL)
	    {
		strcpy(dimbuf, "SOMBlockDim,");
		strcat(dimbuf, dimlist);
	    }
	    else
	    {
		strcpy(dimbuf, dimlist);
	    }
	}
	else
	{
	    /* If not "Blocked" SOM then just copy dim list to dim buffer */
	    strcpy(dimbuf, dimlist);
	}

	/*
	 * Copy dimension buffer to auxiliary dimlist and append a comma to
	 * end of dimension list.
	 */
	strcpy(dimlist0, dimbuf);
	strcat(dimbuf, ",");


	/* Find comma */
	/* ---------- */
	comma = strchr(dimbuf, ',');


	/*
	 * Loop through entries in dimension list to make sure they are
	 * defined in grid
	 */
	while (comma != NULL)
	{
	    /* Copy dimension list entry to dimcheck */
	    /* ------------------------------------- */
	    dimcheck = (char *) calloc(comma - dimbuf + 1, 1);
	    if(dimcheck == NULL)
	    { 
		HEpush(DFE_NOSPACE,"GDdeffield", __FILE__, __LINE__);
		free(dimbuf);
		free(dimlist0);
		return(-1);
	    }
	    memcpy(dimcheck, dimbuf, comma - dimbuf);


	    /* Get Dimension Size */
	    /* ------------------ */
	    if (strcmp(dimcheck, "XDim") == 0)
	    {
		/* If "XDim" then use xdim value for grid definition */
		/* ------------------------------------------------- */
		dimsize = xdim;
		found = 1;
		dims[rank] = dimsize;
		rank++;
	    }
	    else if (strcmp(dimcheck, "YDim") == 0)
	    {
		/* If "YDim" then use ydim value for grid definition */
		/* ------------------------------------------------- */
		dimsize = ydim;
		found = 1;
		dims[rank] = dimsize;
		rank++;
	    }
	    else
	    {
		/* "Regular" Dimension */
		/* ------------------- */
		dimsize = GDdiminfo(gridID, dimcheck);
		if (dimsize != -1)
		{
		    found = 1;
		    dims[rank] = dimsize;
		    rank++;
		}
		else
		{
		    found = 0;
		}
	    }


	    /*
	     * If dimension list entry not found - set error return status,
	     * append name to utility buffer for error report
	     */
	    if (found == 0)
	    {
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
	    memmove(dimbuf, comma + 1, strlen(comma)-1);
	    dimbuf[strlen(comma)-1]= 0;
	    comma = strchr(dimbuf, ',');
	    free(dimcheck);
	}
	free(dimbuf);

	/* Check fieldname length */
	/* ---------------------- */
	if (status == 0)
	{
/* if ((intn) strlen(fieldname) > MAX_NC_NAME - 7)
** this was changed because HDF4.1r3 made a change in the
** hlimits.h file.  We have notified NCSA and asked to have 
** it made the same as in previous versions of HDF
** see ncr 26314.  DaW  Apr 2000
*/
            if((intn) strlen(fieldname) > (256 - 7))
	    {
		status = -1;
		HEpush(DFE_GENAPP, "GDdefinefield", __FILE__, __LINE__);
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
		    break;
		}
	    }

	    if (foundNT == 0)
	    {
		HEpush(DFE_BADNUMTYPE, "GDdeffield", __FILE__, __LINE__);
		HEreport("Invalid number type: %d (%s).\n",
			 numbertype, fieldname);
		status = -1;
	    }
	}


	/* Define Field */
	/* ------------ */
	if (status == 0)
	{
	    /* Get Field Vgroup id, compression code, & tiling code */
	    /* -------------------------------------------------- */
	    vgid = GDXGrid[gID].VIDTable[0];
	    compcode = GDXGrid[gID].compcode;
	    tilecode = GDXGrid[gID].tilecode;


	    /* SDS Interface (Multi-dim fields) */
	    /* -------------------------------- */


	    /*
	     * If rank is less than or equal to 3 (and greater than 1) and
	     * AUTOMERGE is set and the first dimension is not appendable and
	     * the compression code is set to none then ...
	     */
	    if (rank >= 2 && rank <= 3 && merge == HDFE_AUTOMERGE &&
		dims[0] != 0 && compcode == HDFE_COMP_NONE &&
		tilecode == HDFE_NOTILE)
	    {

		/* Find first empty slot in external combination array */
		/* --------------------------------------------------- */
		i = 0;
		while (GDXSDcomb[5 * i] != 0)
		{
		    i++;
		}

		/*
		 * Store dimensions, grid root Vgroup ID, and number type in
		 * external combination array "GDXSDcomb"
		 */
		if (rank == 2)
		{
		    /* If 2-dim field then set lowest dimension to 1 */
		    /* --------------------------------------------- */
		    GDXSDcomb[5 * i] = 1;
		    GDXSDcomb[5 * i + 1] = dims[0];
		    GDXSDcomb[5 * i + 2] = dims[1];
		}
		else
		{
		    GDXSDcomb[5 * i] = dims[0];
		    GDXSDcomb[5 * i + 1] = dims[1];
		    GDXSDcomb[5 * i + 2] = dims[2];
		}

		GDXSDcomb[5 * i + 3] = gdVgrpID;
		GDXSDcomb[5 * i + 4] = numbertype;


		/* Concatenate fieldname with combined name string */
		/* ----------------------------------------------- */
		if ((intn) strlen(GDXSDname) +
		    (intn) strlen(fieldname) + 2 < HDFE_NAMBUFSIZE)
		{
		    strcat(GDXSDname, fieldname);
		    strcat(GDXSDname, ",");
		}
		else
		{
		    /* GDXSDname array too small! */
		    /* -------------------------- */
		    HEpush(DFE_GENAPP, "GDdefinefield",
			   __FILE__, __LINE__);
		    HEreport(errbuf1);
		    status = -1;
		    free(dimlist0);
		    return (status);
		}


		/*
		 * If 2-dim field then set lowest dimension (in 3-dim array)
		 * to "ONE"
		 */
		if (rank == 2)
		{
		    if ((intn) strlen(GDXSDdims) + 5 < HDFE_DIMBUFSIZE)
		    {
			strcat(GDXSDdims, "ONE,");
		    }
		    else
		    {
			/* GDXSDdims array too small! */
			/* -------------------------- */
			HEpush(DFE_GENAPP, "GDdefinefield",
			       __FILE__, __LINE__);
			HEreport(errbuf2);
			status = -1;
			free(dimlist0);
			return (status);
		    }
		}


		/*
		 * Concatanate field dimlist to merged dimlist and separate
		 * fields with semi-colon.
		 */
		if ((intn) strlen(GDXSDdims) +
		    (intn) strlen(dimlist0) + 2 < HDFE_DIMBUFSIZE)
		{
		    strcat(GDXSDdims, dimlist0);
		    strcat(GDXSDdims, ";");
		}
		else
		{
		    /* GDXSDdims array too small! */
		    /* -------------------------- */
		    HEpush(DFE_GENAPP, "GDdefinefield",
			   __FILE__, __LINE__);
		    HEreport(errbuf2);
		    status = -1;
		    free(dimlist0);
		    return (status);
		}

	    }			/* End Multi-Dim Merge Section */
	    else
	    {

		/* Multi-Dim No Merge Section */
		/* ========================== */


		/* Check that field dims are divisible by tile dims */
		/* ------------------------------------------------ */
		if (tilecode == HDFE_TILE)
		{
		    for (i = 0; i < GDXGrid[gID].tilerank; i++)
		    {
			if ((dims[i] % GDXGrid[gID].tiledims[i]) != 0)
			{
			    HEpush(DFE_GENAPP, "GDdeffield",
				   __FILE__, __LINE__);
			    HEreport(errmsg,
				     i, dims[i], GDXGrid[gID].tiledims[0]);
			    status = -1;
			}
		    }

		    if (status == -1)
		    {
			free(dimlist0);
			return (status);
		    }
		}


		/* Create SDS dataset */
		/* ------------------ */
		sdid = SDcreate(sdInterfaceID, fieldname,
				numbertype, rank, dims);


		/* Store Dimension Names in SDS */
		/* ---------------------------- */
		rank = EHparsestr(dimlist0, ',', ptr, slen);
		for (i = 0; i < rank; i++)
		{
		    /* Dimension name = Swathname:Dimname */
		    /* ---------------------------------- */
		    memcpy(utlbuf, ptr[i], slen[i]);
		    utlbuf[slen[i]] = 0;
		    strcat(utlbuf, ":");
		    strcat(utlbuf, gridname);

		    dimid = SDgetdimid(sdid, i);
		    SDsetdimname(dimid, utlbuf);
		}


		/* Setup Compression */
		/* ----------------- */
		if (compcode == HDFE_COMP_NBIT)
		{
		    c_info.nbit.nt = numbertype;
		    c_info.nbit.sign_ext = GDXGrid[gID].compparm[0];
		    c_info.nbit.fill_one = GDXGrid[gID].compparm[1];
		    c_info.nbit.start_bit = GDXGrid[gID].compparm[2];
		    c_info.nbit.bit_len = GDXGrid[gID].compparm[3];
		}
		else if (compcode == HDFE_COMP_SKPHUFF)
		{
		    c_info.skphuff.skp_size = (intn) DFKNTsize(numbertype);
		}
		else if (compcode == HDFE_COMP_DEFLATE)
		{
		    c_info.deflate.level = GDXGrid[gID].compparm[0];
		}


		/* If field is compressed w/o tiling then call SDsetcompress */
		/* --------------------------------------------------------- */
		if (compcode != HDFE_COMP_NONE && tilecode == HDFE_NOTILE)
		{
                    /* status = */ SDsetcompress(sdid, (comp_coder_t) compcode, &c_info);
		}


		/* Setup Tiling */
		/* ------------ */
		if (tilecode == HDFE_TILE)
		{
		    /* Tiling without Compression */
		    /* -------------------------- */
		    if (compcode == HDFE_COMP_NONE)
		    {

			/* Setup chunk lengths */
			/* ------------------- */
			for (i = 0; i < GDXGrid[gID].tilerank; i++)
			{
			    chunkDef.chunk_lengths[i] =
				GDXGrid[gID].tiledims[i];
			}

			chunkFlag = HDF_CHUNK;
		    }

		    /* Tiling with Compression */
		    /* ----------------------- */
		    else
		    {
			/* Setup chunk lengths */
			/* ------------------- */
			for (i = 0; i < GDXGrid[gID].tilerank; i++)
			{
			    chunkDef.comp.chunk_lengths[i] =
				GDXGrid[gID].tiledims[i];
			}


			/* Setup chunk flag & chunk compression type */
			/* ----------------------------------------- */
			chunkFlag = HDF_CHUNK | HDF_COMP;
			chunkDef.comp.comp_type = compcode;


			/* Setup chunk compression parameters */
			/* ---------------------------------- */
			if (compcode == HDFE_COMP_SKPHUFF)
			{
			    chunkDef.comp.cinfo.skphuff.skp_size =
				c_info.skphuff.skp_size;
			}
			else if (compcode == HDFE_COMP_DEFLATE)
			{
			    chunkDef.comp.cinfo.deflate.level =
				c_info.deflate.level;
			}
		    }

		    /* Call SDsetchunk routine */
		    /* ----------------------- */
		    /* status = */ SDsetchunk(sdid, chunkDef, chunkFlag);
		}


		/* Attach to Vgroup */
		/* ---------------- */
		Vaddtagref(vgid, DFTAG_NDG, SDidtoref(sdid));


		/* Store SDS dataset IDs */
		/* --------------------- */

		/* Allocate space for the SDS ID array */
		/* ----------------------------------- */
		if (GDXGrid[gID].nSDS > 0)
		{
		    /* Array already exists therefore reallocate */
		    /* ----------------------------------------- */
		    GDXGrid[gID].sdsID = (int32 *)
			realloc((void *) GDXGrid[gID].sdsID,
				(GDXGrid[gID].nSDS + 1) * 4);
		    if(GDXGrid[gID].sdsID == NULL)
		    { 
			HEpush(DFE_NOSPACE,"GDdeffield", __FILE__, __LINE__);
			free(dimlist0);
			return(-1);
		    }
		}
		else
		{
		    /* Array does not exist */
		    /* -------------------- */
		    GDXGrid[gID].sdsID = (int32 *) calloc(1, 4);
		    if(GDXGrid[gID].sdsID == NULL)
		    { 
			HEpush(DFE_NOSPACE,"GDdeffield", __FILE__, __LINE__);
			free(dimlist0);
			return(-1);
		    }
		}

		/* Store SDS ID in array & increment count */
		/* --------------------------------------- */
		GDXGrid[gID].sdsID[GDXGrid[gID].nSDS] = sdid;
		GDXGrid[gID].nSDS++;

	    }


	    /* Setup metadata string */
	    /* --------------------- */
	    snprintf(utlbuf, sizeof(utlbuf), "%s%s%s", fieldname, ":", dimlist0);


	    /* Setup compression metadata */
	    /* -------------------------- */
	    if (compcode != HDFE_COMP_NONE)
	    {
		snprintf(utlbuf2, sizeof(utlbuf2),
			"%s%s",
			":\n\t\t\t\tCompressionType=", HDFcomp[compcode]);

		switch (compcode)
		{
		case HDFE_COMP_NBIT:

		    snprintf(parmbuf, sizeof(parmbuf),
			    "%s%d,%d,%d,%d%s",
			    "\n\t\t\t\tCompressionParams=(",
			    GDXGrid[gID].compparm[0],
			    GDXGrid[gID].compparm[1],
			    GDXGrid[gID].compparm[2],
			    GDXGrid[gID].compparm[3], ")");
		    strcat(utlbuf2, parmbuf);
		    break;


		case HDFE_COMP_DEFLATE:

		    snprintf(parmbuf, sizeof(parmbuf),
			    "%s%d",
			    "\n\t\t\t\tDeflateLevel=",
			    GDXGrid[gID].compparm[0]);
		    strcat(utlbuf2, parmbuf);
		    break;
		}
		strcat(utlbuf, utlbuf2);
	    }




	    /* Setup tiling metadata */
	    /* --------------------- */
	    if (tilecode == HDFE_TILE)
	    {
		if (compcode == HDFE_COMP_NONE)
		{
		    snprintf(utlbuf2, sizeof(utlbuf2), "%s%d",
			    ":\n\t\t\t\tTilingDimensions=(",
			    (int)GDXGrid[gID].tiledims[0]);
		}
		else
		{
		    snprintf(utlbuf2, sizeof(utlbuf2), "%s%d",
			    "\n\t\t\t\tTilingDimensions=(",
			    (int)GDXGrid[gID].tiledims[0]);
		}

		for (i = 1; i < GDXGrid[gID].tilerank; i++)
		{
		    snprintf(parmbuf, sizeof(parmbuf), ",%d", (int)GDXGrid[gID].tiledims[i]);
		    strcat(utlbuf2, parmbuf);
		}
		strcat(utlbuf2, ")");
		strcat(utlbuf, utlbuf2);
	    }


	    /* Insert field metadata within File Structural Metadata */
	    /* ----------------------------------------------------- */
	    status = EHinsertmeta(sdInterfaceID, gridname, "g", 4L,
				  utlbuf, &numbertype);

	}
	free(dimlist0);

    }

    /* If all dimensions not found then report error */
    /* --------------------------------------------- */
    if (foundAllDim == 0)
    {
	HEpush(DFE_GENAPP, "GDdeffield", __FILE__, __LINE__);
	HEreport("Dimension(s): \"%s\" not found (%s).\n",
		 utlbuf, fieldname);
	status = -1;
    }

    return (status);
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDwritefieldmeta                                                 |
|                                                                             |
|  DESCRIPTION: Writes field meta data for an existing grid field not         |
|               defined within the grid API routine "GDdeffield".             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               grid structure ID                       |
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
GDwritefieldmeta(int32 gridID, const char *fieldname, const char *dimlist,
		 int32 numbertype)
{
    intn            status = 0;	/* routine return status variable */

    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           dum;	/* dummy variable */
    int32           idOffset = GDIDOFFSET;	/* Grid ID offset */

    char            utlbuf[256];/* Utility buffer */
    char            gridname[80];	/* Grid name */


    status = GDchkgdid(gridID, "GDwritefieldmeta", &dum, &sdInterfaceID,
		       &dum);

    if (status == 0)
    {
	snprintf(utlbuf, sizeof(utlbuf), "%s%s%s", fieldname, ":", dimlist);

	Vgetname(GDXGrid[gridID % idOffset].IDTable, gridname);
	status = EHinsertmeta(sdInterfaceID, gridname, "g", 4L,
			      utlbuf, &numbertype);
    }
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
		Vgetname(GDXGrid[gID].IDTable, gridname);


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
		EHgetmetavalue(metaptrs, "FieldList", name);
		memmove(name, name + 1, strlen(name) - 2);
		name[strlen(name) - 2] = 0;


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

    int32           offset[8];	/* I/O offset (start) */
    int32           incr[8];	/* I/O increment (stride) */
    int32           count[8];	/* I/O count (edge) */
    int32           dims[8];	/* Field/SDS dimensions */
    int32           mrgOffset;	/* Merged field offset */
    int32           strideOne;	/* Strides = 1 flag */


    /* Check for valid grid ID */
    /* ----------------------- */
    status = GDchkgdid(gridID, "GDwrrdfield", &fid, &sdInterfaceID, &dum);


    if (status == 0)
    {
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
    }

    return (status);
}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDwritefield                                                     |
|                                                                             |
|  DESCRIPTION: Writes data to a grid field.                                  |
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
GDwritefield(int32 gridID, const char *fieldname,
	     int32 start[], int32 stride[], int32 edge[], VOIDP data)

{
    intn            status = 0;	/* routine return status variable */

    status = GDwrrdfield(gridID, fieldname, "w", start, stride, edge,
			 data);
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
	attrVgrpID = GDXGrid[gridID % idOffset].VIDTable[1];
	status = EHattr(fid, attrVgrpID, attrname, numbertype, count,
			wrcode, datbuf);
    }
    return (status);
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDwriteattr                                                      |
|                                                                             |
|  DESCRIPTION: Writes/updates attribute in a grid.                           |
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
GDwriteattr(int32 gridID, const char *attrname, int32 numbertype, int32 count,
	    VOIDP datbuf)
{
    intn            status = 0;	/* routine return status variable */

    /* Call GDwrrdattr routine to write attribute */
    /* ------------------------------------------ */
    status = GDwrrdattr(gridID, attrname, numbertype, count, "w", datbuf);

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

    int32           fid;	/* HDF-EOS file ID */
    int32           attrVgrpID;	/* Grid attribute ID */
    int32           dum;	/* dummy variable */
    int32           idOffset = GDIDOFFSET;	/* Grid ID offset */

    status = GDchkgdid(gridID, "GDattrinfo", &fid, &dum, &dum);

    attrVgrpID = GDXGrid[gridID % idOffset].VIDTable[1];

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
	attrVgrpID = GDXGrid[gridID % idOffset].VIDTable[1];
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
|  FUNCTION: GDinqdims                                                        |
|                                                                             |
|  DESCRIPTION: Retrieve information about all dimensions defined in a grid.  |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  nDim           int32               Number of defined dimensions            |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               grid structure ID                       |
|                                                                             |
|  OUTPUTS:                                                                   |
|  dimnames       char                Dimension names (comma-separated)       |
|  dims           int32               Dimension values                        |
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
|  Feb 97   Joel Gales    Set nDim to -1 if status = -1                       |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
GDinqdims(int32 gridID, char *dimnames, int32 dims[])
{
    intn            status;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           gdVgrpID;	/* Grid root Vgroup ID */
    int32           size;	/* Dimension size */
    int32           idOffset = GDIDOFFSET;	/* Grid ID offset */
    int32           nDim = 0;	/* Number of dimensions */

    char           *metabuf;	/* Pointer to structural metadata (SM) */
    char           *metaptrs[2];/* Pointers to begin and end of SM section */
    char            gridname[80];	/* Grid Name */
    char           *utlstr;/* Utility string */

    /* Allocate space for utility string */
    /* --------------------------------- */
    utlstr = (char *) calloc(UTLSTR_MAX_SIZE, sizeof(char));
    if(utlstr == NULL)
    { 
	HEpush(DFE_NOSPACE,"GDinqdims", __FILE__, __LINE__);
	return(-1);
    }
    /* Check for valid grid id */
    /* ----------------------- */
    status = GDchkgdid(gridID, "GDinqdims", &fid, &sdInterfaceID, &gdVgrpID);

    if (status == 0)
    {
	/* If dimension names or sizes are requested */
	/* ----------------------------------------- */
	if (dimnames != NULL || dims != NULL)
	{
	    /* Get grid name */
	    /* ------------- */
	    Vgetname(GDXGrid[gridID % idOffset].IDTable, gridname);


	    /* Get pointers to "Dimension" section within SM */
	    /* --------------------------------------------- */
	    metabuf = (char *) EHmetagroup(sdInterfaceID, gridname, "g",
					   "Dimension", metaptrs);
	    if(metabuf == NULL)
	    {
		free(utlstr);
		return(-1);
	    }  
	    

	    /* If dimension names are requested then "clear" name buffer */
	    /* --------------------------------------------------------- */
	    if (dimnames != NULL)
	    {
		dimnames[0] = 0;
	    }

	    while (metaptrs[0] < metaptrs[1] && metaptrs[0] != NULL)
	    {
		strcpy(utlstr, "\t\tOBJECT=");
		metaptrs[0] = strstr(metaptrs[0], utlstr);
		if (metaptrs[0] < metaptrs[1] && metaptrs[0] != NULL)
		{
		    /* Get Dimension Name */
		    /* ------------------ */
		    if (dimnames != NULL)
		    {
			/* Check 1st for old meta data then new */
			/* ------------------------------------ */
			EHgetmetavalue(metaptrs, "OBJECT", utlstr);
			if (utlstr[0] != '"')
			{
			    metaptrs[0] =
				strstr(metaptrs[0], "\t\t\t\tDimensionName=");
			    EHgetmetavalue(metaptrs, "DimensionName", utlstr);
			}

			/* Strip off double quotes */
			/* ----------------------- */
			memmove(utlstr, utlstr + 1, strlen(utlstr) - 2);
			utlstr[strlen(utlstr) - 2] = 0;

			if (nDim > 0)
			{
			    strcat(dimnames, ",");
			}
			strcat(dimnames, utlstr);
		    }

		    /* Get Dimension Size */
		    /* ------------------ */
		    if (dims != NULL)
		    {
			EHgetmetavalue(metaptrs, "Size", utlstr);
			size = atoi(utlstr);
			dims[nDim] = size;
		    }
		    nDim++;
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
	    Vgetname(GDXGrid[gridID % idOffset].IDTable, gridname);


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
    char           *metaptrs[2];/* Pointers to begin and end of SM section */
    char            gridname[80];	/* Grid Name */
    char           *utlstr;/* Utility string */
    char            valName[2][32];	/* Strings to search for */

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
	Vgetname(GDXGrid[gridID % idOffset].IDTable, gridname);

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
|  FUNCTION: GDsetfillvalue                                                   |
|                                                                             |
|  DESCRIPTION: Sets fill value for the specified field.                      |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               grid structure ID                       |
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
GDsetfillvalue(int32 gridID, const char *fieldname, VOIDP fillval)
{
    intn            status;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           gdVgrpID;	/* Grid root Vgroup ID */
    int32           sdid;	/* SDS id */
    int32           nt;		/* Number type */
    int32           dims[8];	/* Dimensions array */
    int32           dum;	/* Dummy variable */
    int32           solo;	/* "Solo" (non-merged) field flag */

    char            name[80];	/* Fill value "attribute" name */

    /* Check for valid grid ID and get SDS interface ID */
    status = GDchkgdid(gridID, "GDsetfillvalue",
		       &fid, &sdInterfaceID, &gdVgrpID);

    if (status == 0)
    {
	/* Get field info */
	status = GDfieldinfo(gridID, fieldname, &dum, dims, &nt, NULL);

	if (status == 0)
	{
	    /* Get SDS ID and solo flag */
	    status = GDSDfldsrch(gridID, sdInterfaceID, fieldname,
				 &sdid, &dum, &dum, &dum,
				 dims, &solo);

	    /* If unmerged field then call HDF set field routine */
	    if (solo == 1)
	    {
                /* status = */ SDsetfillvalue(sdid, fillval);
	    }

	    /*
	     * Store fill value in attribute.  Name is given by fieldname
	     * prepended with "_FV_"
	     */
	    strcpy(name, "_FV_");
	    strcat(name, fieldname);
	    status = GDwriteattr(gridID, name, nt, 1, fillval);


	}
	else
	{
	    HEpush(DFE_GENAPP, "GDsetfillvalue", __FILE__, __LINE__);
	    HEreport("Fieldname \"%s\" does not exist.\n", fieldname);
	}
    }
    return (status);
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
    intn            j;		/* Loop index */
    intn            k;		/* Loop index */
    intn            status = 0;	/* routine return status variable */
    intn            statusFill = 0;	/* return status from GDgetfillvalue */

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
    int32           gID;	/* Grid ID - offset */
    int32           nflds0;	/* Number of fields */
    int32          *namelen0;	/* Pointer to name string length array */
    int32           rank;	/* Rank of merged field */
    int32           truerank;	/* True rank of merged field */
    int32           idOffset = GDIDOFFSET;	/* Grid ID offset */
    int32           dum;	/* Dummy variable */

    char           *nambuf;	/* Pointer to name buffer */
    char          **nameptr;	/* Pointer to name string pointer array */
    char          **dimptr;	/* Pointer to dim string pointer array */
    char          **nameptr0;	/* Pointer to name string pointer array */
    char           *ptr1[3];	/* String pointer array */
    char           *ptr2[3];	/* String pointer array */
    char            dimbuf1[128];	/* Dimension buffer 1 */
    char            dimbuf2[128];	/* Dimension buffer 2 */
    char            gridname[VGNAMELENMAX + 1];	/* Grid name */
    char           *utlbuf;	/* Utility buffer */
    char            fillval[32];/* Fill value buffer */



    status = GDchkgdid(gridID, "GDdetach", &dum, &sdInterfaceID, &dum);

    if (status == 0)
    {
	gID = gridID % idOffset;
	Vgetname(GDXGrid[gID].IDTable, gridname);

	/* SDS combined fields */
	/* ------------------- */
	if (strlen(GDXSDname) == 0)
	{
	    nflds = 0;

	    /* Allocate "dummy" arrays so free() doesn't bomb later */
	    /* ---------------------------------------------------- */
	    nameptr = (char **) calloc(1, sizeof(char *));
	    if(nameptr == NULL)
	    { 
		HEpush(DFE_NOSPACE,"GDdetach", __FILE__, __LINE__);
		return(-1);
	    }
	    namelen = (int32 *) calloc(1, sizeof(int32));
	    if(namelen == NULL)
	    { 
		HEpush(DFE_NOSPACE,"GDdetach", __FILE__, __LINE__);
		free(nameptr);
		return(-1);
	    }
	    nameptr0 = (char **) calloc(1, sizeof(char *));
	    if(nameptr0 == NULL)
	    { 
		HEpush(DFE_NOSPACE,"GDdetach", __FILE__, __LINE__);
		free(nameptr);
		free(namelen);
		return(-1);
	    }
	    namelen0 = (int32 *) calloc(1, sizeof(int32));
	    if(namelen0 == NULL)
	    { 
		HEpush(DFE_NOSPACE,"GDdetach", __FILE__, __LINE__);
		free(nameptr);
		free(namelen);
		free(nameptr0);
		return(-1);
	    }
	    dimptr = (char **) calloc(1, sizeof(char *));
	    if(dimptr == NULL)
	    { 
		HEpush(DFE_NOSPACE,"GDdetach", __FILE__, __LINE__);
		free(nameptr);
		free(namelen);
		free(nameptr0);
		free(namelen0);
		return(-1);
	    }
	    dimlen = (int32 *) calloc(1, sizeof(int32));
	    if(dimlen == NULL)
	    { 
		HEpush(DFE_NOSPACE,"GDdetach", __FILE__, __LINE__);
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
		HEpush(DFE_NOSPACE,"GDdetach", __FILE__, __LINE__);
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
		HEpush(DFE_NOSPACE,"GDdetach", __FILE__, __LINE__);
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
	     * "Trim Off" trailing "," and ";" in GDXSDname & GDXSDdims
	     * respectively
	     */
	    GDXSDname[strlen(GDXSDname) - 1] = 0;
	    GDXSDdims[strlen(GDXSDdims) - 1] = 0;


	    /* Get number of fields from GDXSDname string */
	    /* ------------------------------------------ */
	    nflds = EHparsestr(GDXSDname, ',', NULL, NULL);

	    /* Allocate space for various dynamic arrays */
	    /* ----------------------------------------- */
	    nameptr = (char **) calloc(nflds, sizeof(char *));
	    if(nameptr == NULL)
	    { 
		HEpush(DFE_NOSPACE,"GDdetach", __FILE__, __LINE__);
		return(-1);
	    }
	    namelen = (int32 *) calloc(nflds, sizeof(int32));
	    if(namelen == NULL)
	    { 
		HEpush(DFE_NOSPACE,"GDdetach", __FILE__, __LINE__);
		free(nameptr);
		return(-1);
	    }
	    nameptr0 = (char **) calloc(nflds, sizeof(char *));
	    if(nameptr0 == NULL)
	    { 
		HEpush(DFE_NOSPACE,"GDdetach", __FILE__, __LINE__);
		free(nameptr);
		free(namelen);
		return(-1);
	    }
	    namelen0 = (int32 *) calloc(nflds, sizeof(int32));
	    if(namelen0 == NULL)
	    { 
		HEpush(DFE_NOSPACE,"GDdetach", __FILE__, __LINE__);
		free(nameptr);
		free(namelen);
		free(nameptr0);
		return(-1);
	    }
	    dimptr = (char **) calloc(nflds, sizeof(char *));
	    if(dimptr == NULL)
	    { 
		HEpush(DFE_NOSPACE,"GDdetach", __FILE__, __LINE__);
		free(nameptr);
		free(namelen);
		free(nameptr0);
		free(namelen0);
		return(-1);
	    }
	    dimlen = (int32 *) calloc(nflds, sizeof(int32));
	    if(dimlen == NULL)
	    { 
		HEpush(DFE_NOSPACE,"GDdetach", __FILE__, __LINE__);
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
		HEpush(DFE_NOSPACE,"GDdetach", __FILE__, __LINE__);
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
		HEpush(DFE_NOSPACE,"GDdetach", __FILE__, __LINE__);
		free(nameptr);
		free(namelen);
		free(nameptr0);
		free(namelen0);
		free(dimptr);
		free(dimlen);
		free(offset);
		return(-1);
	    }

	    /* Parse GDXSDname and GDXSDdims strings */
	    /* ------------------------------------- */
	    nflds = EHparsestr(GDXSDname, ',', nameptr, namelen);
	    nflds = EHparsestr(GDXSDdims, ';', dimptr, dimlen);
	}


	for (i = 0; i < nflds; i++)
	{
	    if (GDXSDcomb[5 * i] != 0 &&
		GDXSDcomb[5 * i + 3] == GDXGrid[gID].IDTable)
	    {
		nambuf = (char *) calloc(strlen(GDXSDname) + 1, 1);
		if(nambuf == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"GDdetach", __FILE__, __LINE__);
		    free(nameptr);
		    free(namelen);
		    free(nameptr0);
		    free(namelen0);
		    free(dimptr);
		    free(dimlen);
		    free(offset);
		    return(-1);
		}
		utlbuf = (char *) calloc(strlen(GDXSDname) * 2 + 7, 1);
		if(utlbuf == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"GDdetach", __FILE__, __LINE__);
		    free(nambuf);
		    free(nameptr);
		    free(namelen);
		    free(nameptr0);
		    free(namelen0);
		    free(dimptr);
		    free(dimlen);
		    free(offset);
		    return(-1);
		}

		for (k = 0; k < (intn)sizeof(dimbuf1); k++)
		    dimbuf1[k] = 0;


		/* Load array to match, name & parse dims */
		/* -------------------------------------- */
		memcpy(match, &GDXSDcomb[5 * i], 20);
		memcpy(nambuf, nameptr[i], namelen[i]);

		memcpy(dimbuf1, dimptr[i], dimlen[i]);
		dum = EHparsestr(dimbuf1, ',', ptr1, slen1);


		/* Separate combined dimension from others */
		/* --------------------------------------- */
		dimbuf1[slen1[0]] = 0;

		offset[0] = 0;
		indvdims[0] = abs(match[0]);

		for (j = i + 1, cmbfldcnt = 0; j < nflds; j++)
		{
		    for (k = 0; k < (intn)sizeof(dimbuf2); k++)
			dimbuf2[k] = 0;
		    memcpy(dimbuf2, dimptr[j], dimlen[j]);
		    dum = EHparsestr(dimbuf2, ',', ptr2, slen2);
		    dimbuf2[slen2[0]] = 0;


		    if (GDXSDcomb[5 * j] != 0 &&
			strcmp(dimbuf1 + slen1[0],
			       dimbuf2 + slen2[0]) == 0 &&
			match[1] == GDXSDcomb[5 * j + 1] &&
			match[2] == GDXSDcomb[5 * j + 2] &&
			match[3] == GDXSDcomb[5 * j + 3] &&
			match[4] == GDXSDcomb[5 * j + 4])
		    {
			/* Add to combined dimension size */
			match[0] += GDXSDcomb[5 * j];

			/* Concatenate name */
			strcat(nambuf, ",");
			memcpy(nambuf + strlen(nambuf),
			       nameptr[j], namelen[j]);

			/* Store individual dims and dim offsets */
			cmbfldcnt++;
			indvdims[cmbfldcnt] = abs(GDXSDcomb[5 * j]);
			offset[cmbfldcnt] =
			    offset[cmbfldcnt - 1] + indvdims[cmbfldcnt - 1];

			GDXSDcomb[5 * j] = 0;
		    }
		}


		/* Create SDS */
		/* ---------- */
		nflds0 = EHparsestr(nambuf, ',', nameptr0, namelen0);

		if (abs(match[0]) == 1)
		{
		    for (k = 0; k < 2; k++)
			dims[k] = abs(match[k + 1]);

		    rank = 2;

		    sdid = SDcreate(sdInterfaceID, nambuf,
				    GDXSDcomb[5 * i + 4], 2, dims);
		}
		else
		{
		    for (k = 0; k < 3; k++)
			dims[k] = abs(match[k]);

		    rank = 3;

		    if (cmbfldcnt > 0)
		    {
			strcpy(utlbuf, "MRGFLD_");
			memcpy(utlbuf + 7, nameptr0[0], namelen0[0]);
			utlbuf[7 + namelen0[0]] = 0;
			strcat(utlbuf, ":");
			strcat(utlbuf, nambuf);

			status = EHinsertmeta(sdInterfaceID, gridname, "g",
					      6L, utlbuf, NULL);
		    }
		    else
		    {
			strcpy(utlbuf, nambuf);
		    }

		    sdid = SDcreate(sdInterfaceID, utlbuf,
				    GDXSDcomb[5 * i + 4], 3, dims);


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
			memcpy(dimbuf2, ptr1[k + 1], slen1[k + 1]);
			dimbuf2[slen1[k + 1]] = 0;
		    }
		    else
		    {
			memcpy(dimbuf2, ptr1[k], slen1[k]);
			dimbuf2[slen1[k]] = 0;
		    }


		    if (k == 0 && rank > 2 && cmbfldcnt > 0)
		    {
			snprintf(dimbuf2, sizeof(dimbuf2), "%s%s_%d", "MRGDIM:",
				gridname, (int)dims[0]);
		    }
		    else
		    {
			strcat(dimbuf2, ":");
			strcat(dimbuf2, gridname);
		    }
		    SDsetdimname(SDgetdimid(sdid, k), (char *) dimbuf2);
		}



		/* Write Fill Value */
		/* ---------------- */
		for (k = 0; k < nflds0; k++)
		{
		    memcpy(utlbuf, nameptr0[k], namelen0[k]);
		    utlbuf[namelen[k]] = 0;
		    statusFill = GDgetfillvalue(gridID, utlbuf, fillval);

		    if (statusFill == 0)
		    {
			if (cmbfldcnt > 0)
			{
			    dims[0] = indvdims[k];
			    truerank = (dims[0] == 1) ? 2 : 3;
			    EHfillfld(sdid, rank, truerank,
				      DFKNTsize(match[4]), offset[k],
				      dims, fillval);
			}
			else
			{
			    status = SDsetfillvalue(sdid, fillval);
			}
		    }
		}


		vgid = GDXGrid[gID].VIDTable[0];
		Vaddtagref(vgid, DFTAG_NDG, SDidtoref(sdid));
		SDendaccess(sdid);

		free(nambuf);
		free(utlbuf);

	    }
	}


	for (i = 0; i < nflds; i++)
	{
	    if (GDXSDcomb[5 * i + 3] == GDXGrid[gID].IDTable)
	    {
		if (i == (nflds - 1))
		{
		    GDXSDcomb[5 * i] = 0;
		    *(nameptr[i] - (nflds != 1)) = 0;
		    *(dimptr[i] - (nflds != 1)) = 0;
		}
		else
		{
		    /* memcpy(&GDXSDcomb[5 * i],
			   &GDXSDcomb[5 * (i + 1)],
			   (512 - i - 1) * 5 * 4);*/
		    memmove(&GDXSDcomb[5 * i],
			   &GDXSDcomb[5 * (i + 1)],
			   (512 - i - 1) * 5 * 4);
		   /* memcpy(nameptr[i],
			   nameptr[i + 1],
			   nameptr[0] + 2048 - nameptr[i + 1] - 1);*/
		    memmove(nameptr[i],
			   nameptr[i + 1],
			   nameptr[0] + 2048 - nameptr[i + 1] - 1);
		    /* memcpy(dimptr[i],
			   dimptr[i + 1],
			   dimptr[0] + 2048 * 2 - dimptr[i + 1] - 1);*/
		    memmove(dimptr[i],
			   dimptr[i + 1],
			   dimptr[0] + 2048 * 2 - dimptr[i + 1] - 1);
		}

		i--;
		nflds = EHparsestr(GDXSDname, ',', nameptr, namelen);
		nflds = EHparsestr(GDXSDdims, ';', dimptr, dimlen);
	    }
	}

	if (nflds != 0)
	{
	    strcat(GDXSDname, ",");
	    strcat(GDXSDdims, ";");
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


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDgetdefaults                                                    |
|                                                                             |
|  DESCRIPTION:                                                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  projcode       int32               GCTP projection code                    |
|  zonecode       int32               UTM zone code                           |
|  projparm       float64             Projection parameters                   |
|  spherecode     int32               GCTP spheriod code                      |
|  upleftpt       float64             upper left corner coordinates           |
|  lowrightpt     float64             lower right corner coordinates          |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|  upleftpt       float64             upper left corner coordinates           |
|  lowrightpt     float64             lower right corner coordinates          |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Aug 96   Joel Gales    Original Programmer                                 |
|  Sep 96   Raj Gejjaga   Fixed  bugs in Polar Stereographic and Goode        | |                         Homolosine default calculations.                    |
|  Sep 96   Raj Gejjaga   Added code to compute default boundary points       |
|                         for Lambert Azimuthal Polar and Equatorial          |
|                         projections.                                        |
|  Feb 97   Raj Gejjaga   Added code to compute default boundary points       |
|                         for Integerized Sinusoidal Grid.  Added error       |
|                         handling code.                                      |
|  Jun 00   Abe Taaheri   Added support for EASE grid                         |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
static intn
GDgetdefaults(int32 projcode, int32 zonecode, float64 projparm[],
	      int32 spherecode, float64 upleftpt[], float64 lowrightpt[])
{
    int32           errorcode = 0, status = 0;
    int32(*for_trans[100]) ();

    float64         lon, lat, plat, x, y;
    float64         plon, tlon, llon, rlon, pplon, LLon, LLat, RLon, RLat;
    

    /* invoke GCTP initialization routine */
    /* ---------------------------------- */
    for_init(projcode, zonecode, projparm, spherecode, NULL, NULL,
	     &errorcode, for_trans);

    /* Report error if any */
    /* ------------------- */
    if (errorcode != 0)
    {
	status = -1;
	HEpush(DFE_GENAPP, "GDgetdefaults", __FILE__, __LINE__);
	HEreport("GCTP Error: %d\n", errorcode);
	return (status);
    }

    /* Compute Default Boundary Points for EASE Grid          */
    /* Use Global coverage */
    /* ------------------------------------------------------ */
    if (projcode == GCTP_BCEA &&
	upleftpt[0] == 0 && upleftpt[1] == 0 &&
	lowrightpt[0] == 0 && lowrightpt[1] == 0)
    {
	upleftpt[0] = EHconvAng(EASE_GRID_DEFAULT_UPLEFT_LON, HDFE_DEG_DMS);
	upleftpt[1] = EHconvAng(EASE_GRID_DEFAULT_UPLEFT_LAT, HDFE_DEG_DMS);
	lowrightpt[0] = EHconvAng(EASE_GRID_DEFAULT_LOWRGT_LON, HDFE_DEG_DMS);
	lowrightpt[1] = EHconvAng(EASE_GRID_DEFAULT_LOWRGT_LAT, HDFE_DEG_DMS);
    }

/* Compute Default Boundary Points for CEA     */
   /* --------------------------------------------*/
   if (projcode == GCTP_CEA &&
      upleftpt[0] == 0 && upleftpt[1] == 0 &&
      lowrightpt[0] == 0 && lowrightpt[1] == 0)
    {
      LLon = EHconvAng(EASE_GRID_DEFAULT_UPLEFT_LON, HDFE_DEG_RAD);
      LLat = EHconvAng(EASE_GRID_DEFAULT_UPLEFT_LAT, HDFE_DEG_RAD);
      RLon = EHconvAng(EASE_GRID_DEFAULT_LOWRGT_LON, HDFE_DEG_RAD);
      RLat = EHconvAng(EASE_GRID_DEFAULT_LOWRGT_LAT, HDFE_DEG_RAD);
 
      errorcode = for_trans[projcode] (LLon, LLat, &x, &y);
      if (errorcode != 0)
      {
           status = -1;
           HEpush(DFE_GENAPP, "GDgetdefaults", __FILE__, __LINE__);
           HEreport("GCTP Error: %d\n", errorcode);
           return (status);
      }
        upleftpt[0] = x;
        upleftpt[1] = y;
 
      errorcode = for_trans[projcode] (RLon, RLat, &x, &y);
      if (errorcode != 0)
      {
           status = -1;
           HEpush(DFE_GENAPP, "GDgetdefaults", __FILE__, __LINE__);
           HEreport("GCTP Error: %d\n", errorcode);
           return (status);
      }
      lowrightpt[0] = x;
      lowrightpt[1] = y;
 
    }
 
    
    /* Compute Default Boundary Points for Polar Sterographic */
    /* ------------------------------------------------------ */
    if (projcode == GCTP_PS &&
	upleftpt[0] == 0 && upleftpt[1] == 0 &&
	lowrightpt[0] == 0 && lowrightpt[1] == 0)
    {
	/*
	 * Convert the longitude and latitude from the DMS to decimal degree
	 * format.
	 */
	plon = EHconvAng(projparm[4], HDFE_DMS_DEG);
	plat = EHconvAng(projparm[5], HDFE_DMS_DEG);

	/*
	 * Compute the longitudes at 90, 180 and 270 degrees from the central
	 * longitude.
	 */

	if (plon <= 0.0)
	{
	    tlon = 180.0 + plon;
	    pplon = plon + 360.0;
	}
	else
	{
	    tlon = plon - 180.0;
	    pplon = plon;
	}

	rlon = pplon + 90.0;
	if (rlon > 360.0)
	    rlon = rlon - 360;

	if (rlon > 180.0)
	    rlon = rlon - 360.0;

	if (rlon <= 0.0)
	    llon = 180.0 + rlon;
	else
	    llon = rlon - 180.0;


	/* Convert all four longitudes from decimal degrees to radians */
	plon = EHconvAng(plon, HDFE_DEG_RAD);
	tlon = EHconvAng(tlon, HDFE_DEG_RAD);
	llon = EHconvAng(llon, HDFE_DEG_RAD);
	rlon = EHconvAng(rlon, HDFE_DEG_RAD);

	errorcode = for_trans[projcode] (llon, 0.0, &x, &y);
	if (errorcode != 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "GDgetdefaults", __FILE__, __LINE__);
	    HEreport("GCTP Error: %d\n", errorcode);
	    return (status);
	}

	upleftpt[0] = x;
	
	errorcode = for_trans[projcode] (rlon, 0.0, &x, &y);
	if (errorcode != 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "GDgetdefaults", __FILE__, __LINE__);
	    HEreport("GCTP Error: %d\n", errorcode);
	    return (status);
	}

	lowrightpt[0] = x;
	
	/*
	 * Compute the upperleft and lowright y values based on the south or
	 * north polar projection
	 */

	if (plat < 0.0)
	{
	    errorcode = for_trans[projcode] (plon, 0.0, &x, &y);
	    if (errorcode != 0)
	    {
		status = -1;
		HEpush(DFE_GENAPP, "GDgetdefaults", __FILE__, __LINE__);
		HEreport("GCTP Error: %d\n", errorcode);
		return (status);
	    }

	    upleftpt[1] = y;
	    
	    errorcode = for_trans[projcode] (tlon, 0.0, &x, &y);
	    if (errorcode != 0)
	    {
		status = -1;
		HEpush(DFE_GENAPP, "GDgetdefaults", __FILE__, __LINE__);
		HEreport("GCTP Error: %d\n", errorcode);
		return (status);
	    }

	    lowrightpt[1] = y;
	    
	}
	else
	{
	    errorcode = for_trans[projcode] (tlon, 0.0, &x, &y);
	    if (errorcode != 0)
	    {
		status = -1;
		HEpush(DFE_GENAPP, "GDgetdefaults", __FILE__, __LINE__);
		HEreport("GCTP Error: %d\n", errorcode);
		return (status);
	    }

	    upleftpt[1] = y;
	    
	    errorcode = for_trans[projcode] (plon, 0.0, &x, &y);
	    if (errorcode != 0)
	    {
		status = -1;
		HEpush(DFE_GENAPP, "GDgetdefaults", __FILE__, __LINE__);
		HEreport("GCTP Error: %d\n", errorcode);
		return (status);
	    }

	    lowrightpt[1] = y;
	  
	}
    }


    /* Compute Default Boundary Points for Goode Homolosine */
    /* ---------------------------------------------------- */
    if (projcode == GCTP_GOOD &&
	upleftpt[0] == 0 && upleftpt[1] == 0 &&
	lowrightpt[0] == 0 && lowrightpt[1] == 0)
    {
	lon = EHconvAng(-180, HDFE_DEG_RAD);
	lat = 0.0;

	errorcode = for_trans[projcode] (lon, lat, &x, &y);
	if (errorcode != 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "GDgetdefaults", __FILE__, __LINE__);
	    HEreport("GCTP Error: %d\n", errorcode);
	    return (status);
	}

	upleftpt[0] = -fabs(x);
	lowrightpt[0] = +fabs(x);

	lat = EHconvAng(90, HDFE_DEG_RAD);

	errorcode = for_trans[projcode] (lon, lat, &x, &y);
	if (errorcode != 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "GDgetdefaults", __FILE__, __LINE__);
	    HEreport("GCTP Error: %d\n", errorcode);
	    return (status);
	}

	upleftpt[1] = +fabs(y);
	lowrightpt[1] = -fabs(y);
    }

    /* Compute Default Boundary Points for Lambert Azimuthal */
    /* ----------------------------------------------------- */
    if (projcode == GCTP_LAMAZ &&
	upleftpt[0] == 0 && upleftpt[1] == 0 &&
	lowrightpt[0] == 0 && lowrightpt[1] == 0)
    {
	/*
	 * Convert the longitude and latitude from the DMS to decimal degree
	 * format.
	 */
	plon = EHconvAng(projparm[4], HDFE_DMS_DEG);
	plat = EHconvAng(projparm[5], HDFE_DMS_DEG);

	/*
	 * Compute the longitudes at 90, 180 and 270 degrees from the central
	 * longitude.
	 */

	if (plon <= 0.0)
	{
	    tlon = 180.0 + plon;
	    pplon = plon + 360.0;
	}
	else
	{
	    tlon = plon - 180.0;
	    pplon = plon;
	}

	rlon = pplon + 90.0;
	if (rlon > 360.0)
	    rlon = rlon - 360;

	if (rlon > 180.0)
	    rlon = rlon - 360.0;

	if (rlon <= 0.0)
	    llon = 180.0 + rlon;
	else
	    llon = rlon - 180.0;

	/* Convert all four longitudes from decimal degrees to radians */
	plon = EHconvAng(plon, HDFE_DEG_RAD);
	tlon = EHconvAng(tlon, HDFE_DEG_RAD);
	llon = EHconvAng(llon, HDFE_DEG_RAD);
	rlon = EHconvAng(rlon, HDFE_DEG_RAD);

	errorcode = for_trans[projcode] (llon, 0.0, &x, &y);
	if (errorcode != 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "GDgetdefaults", __FILE__, __LINE__);
	    HEreport("GCTP Error: %d\n", errorcode);
	    return (status);
	}

	upleftpt[0] = x;

	errorcode = for_trans[projcode] (rlon, 0.0, &x, &y);
	if (errorcode != 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "GDgetdefaults", __FILE__, __LINE__);
	    HEreport("GCTP Error: %d\n", errorcode);
	    return (status);
	}

	lowrightpt[0] = x;

	/*
	 * Compute upperleft and lowerright values based on whether the
	 * projection is south polar, north polar or equatorial
	 */

	if (plat == -90.0)
	{
	    errorcode = for_trans[projcode] (plon, 0.0, &x, &y);
	    if (errorcode != 0)
	    {
		status = -1;
		HEpush(DFE_GENAPP, "GDgetdefaults", __FILE__, __LINE__);
		HEreport("GCTP Error: %d\n", errorcode);
		return (status);
	    }

	    upleftpt[1] = y;

	    errorcode = for_trans[projcode] (tlon, 0.0, &x, &y);
	    if (errorcode != 0)
	    {
		status = -1;
		HEpush(DFE_GENAPP, "GDgetdefaults", __FILE__, __LINE__);
		HEreport("GCTP Error: %d\n", errorcode);
		return (status);
	    }

	    lowrightpt[1] = y;
	}
	else if (plat == 90.0)
	{
	    errorcode = for_trans[projcode] (tlon, 0.0, &x, &y);
	    if (errorcode != 0)
	    {
		status = -1;
		HEpush(DFE_GENAPP, "GDgetdefaults", __FILE__, __LINE__);
		HEreport("GCTP Error: %d\n", errorcode);
		return (status);
	    }

	    upleftpt[1] = y;

	    errorcode = for_trans[projcode] (plon, 0.0, &x, &y);
	    if (errorcode != 0)
	    {
		status = -1;
		HEpush(DFE_GENAPP, "GDgetdefaults", __FILE__, __LINE__);
		HEreport("GCTP Error: %d\n", errorcode);
		return (status);
	    }

	    lowrightpt[1] = y;
	}
	else
	{
	    lat = EHconvAng(90, HDFE_DEG_RAD);
	    errorcode = for_trans[projcode] (plon, lat, &x, &y);
	    if (errorcode != 0)
	    {
		status = -1;
		HEpush(DFE_GENAPP, "GDgetdefaults", __FILE__, __LINE__);
		HEreport("GCTP Error: %d\n", errorcode);
		return (status);
	    }

	    upleftpt[1] = y;

	    lat = EHconvAng(-90, HDFE_DEG_RAD);
	    errorcode = for_trans[projcode] (plon, lat, &x, &y);
	    if (errorcode != 0)
	    {
		status = -1;
		HEpush(DFE_GENAPP, "GDgetdefaults", __FILE__, __LINE__);
		HEreport("GCTP Error: %d\n", errorcode);
		return (status);
	    }

	    lowrightpt[1] = y;
	}
    }

    /* Compute Default Boundary Points for Integerized Sinusoidal Grid */
    /* --------------------------------------------------------------- */
    if (((projcode == GCTP_ISINUS) || (projcode == GCTP_ISINUS1)) &&
	upleftpt[0] == 0 && upleftpt[1] == 0 &&
	lowrightpt[0] == 0 && lowrightpt[1] == 0)
    {
	/*
	 * Convert the longitude and latitude from the DMS to decimal degree
	 * format.
	 */
	plon = EHconvAng(projparm[4], HDFE_DMS_DEG);
	/*plat = EHconvAng(projparm[5], HDFE_DMS_DEG); */

	/*
	 * Compute the longitudes at 90, 180 and 270 degrees from the central
	 * longitude.
	 */

	if (plon <= 0.0)
	{
	    tlon = 180.0 + plon;
	    pplon = plon + 360.0;
	}
	else
	{
	    tlon = plon - 180.0;
	    pplon = plon;
	}

	rlon = pplon + 90.0;
	if (rlon > 360.0)
	    rlon = rlon - 360;

	if (rlon > 180.0)
	    rlon = rlon - 360.0;

	if (rlon <= 0.0)
	    llon = 180.0 + rlon;
	else
	    llon = rlon - 180.0;

	/* Convert all four longitudes from decimal degrees to radians */
	plon = EHconvAng(plon, HDFE_DEG_RAD);
	tlon = EHconvAng(tlon, HDFE_DEG_RAD);
	llon = EHconvAng(llon, HDFE_DEG_RAD);
	rlon = EHconvAng(rlon, HDFE_DEG_RAD);

	errorcode = for_trans[projcode] (llon, 0.0, &x, &y);
	if (errorcode != 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "GDgetdefaults", __FILE__, __LINE__);
	    HEreport("GCTP Error: %d\n", errorcode);
	    return (status);
	}

	upleftpt[0] = x;

	errorcode = for_trans[projcode] (rlon, 0.0, &x, &y);
	if (errorcode != 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "GDgetdefaults", __FILE__, __LINE__);
	    HEreport("GCTP Error: %d\n", errorcode);
	    return (status);
	}

	lowrightpt[0] = x;

	lat = EHconvAng(90, HDFE_DEG_RAD);
	errorcode = for_trans[projcode] (plon, lat, &x, &y);
	if (errorcode != 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "GDgetdefaults", __FILE__, __LINE__);
	    HEreport("GCTP Error: %d\n", errorcode);
	    return (status);
	}

	upleftpt[1] = y;

	lat = EHconvAng(-90, HDFE_DEG_RAD);
	errorcode = for_trans[projcode] (plon, lat, &x, &y);
	if (errorcode != 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "GDgetdefaults", __FILE__, __LINE__);
	    HEreport("GCTP Error: %d\n", errorcode);
	    return (status);
	}

	lowrightpt[1] = y;
    }
    return (errorcode);
}

/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDll2ij                                                          |
|                                                                             |
|  DESCRIPTION:                                                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  projcode       int32               GCTP projection code                    |
|  zonecode       int32               UTM zone code                           |
|  projparm       float64             Projection parameters                   |
|  spherecode     int32               GCTP spheriod code                      |
|  xdimsize       int32               xdimsize from GDcreate                  |
|  ydimsize       int32               ydimsize from GDcreate                  |
|  upleftpt       float64             upper left corner coordinates           |
|  lowrightpt     float64             lower right corner coordinates          |
|  npnts          int32               number of lon-lat points                |
|  longitude      float64             longitude array (radians)               |
|  latitude       float64             latitude array (radians)                |
|                                                                             |
|  OUTPUTS:                                                                   |
|  row            int32               Row array                               |
|  col            int32               Column array                            |
|  xval           float64             X value array                           |
|  yval           float64             Y value array                           |
|                                                                             |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Aug 96   Joel Gales    Return x and y values if requested                  |
|  Jun 00   Abe Taaheri   Added support for EASE grid                         |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
static intn
GDll2ij(int32 projcode, int32 zonecode, float64 projparm[],
	int32 spherecode, int32 xdimsize, int32 ydimsize,
	float64 upleftpt[], float64 lowrightpt[],
	int32 npnts, float64 longitude[], float64 latitude[],
	int32 row[], int32 col[], float64 xval[], float64 yval[])


{
    intn            i;		/* Loop index */
    intn            status = 0;	/* routine return status variable */

    int32           errorcode = 0;	/* GCTP error code */
    int32(*for_trans[100]) ();	/* GCTP function pointer */

    float64         xVal;	/* Scaled x distance */
    float64         yVal;	/* Scaled y distance */
    float64         xMtr;	/* X value in meters from GCTP */
    float64         yMtr;	/* Y value in meters from GCTP */
    float64         lonrad0;	/* Longitude in radians of upleft point */
    float64         latrad0 = 0;	/* Latitude in radians of upleft point */
    float64         lonrad;	/* Longitude in radians of point */
    float64         latrad;	/* Latitude in radians of point */
    float64         scaleX;	/* X scale factor */
    float64         scaleY;	/* Y scale factor */
    float64         EHconvAng();/* Angle conversion routine */
    float64         xMtr0 = 0, xMtr1, yMtr0 = 0, yMtr1;
    float64         lonrad1;	/* Longitude in radians of lowright point */

    /* If projection not GEO call GCTP initialization routine */
    /* ------------------------------------------------------ */
    if (projcode != GCTP_GEO)
    {
	for_init(projcode, zonecode, projparm, spherecode, NULL, NULL,
		 &errorcode, for_trans);

	/* Report error if any */
	/* ------------------- */
	if (errorcode != 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "GDll2ij", __FILE__, __LINE__);
	    HEreport("GCTP Error: %d\n", errorcode);
	}
    }


    if (status == 0)
    {
	/* GEO projection */
	/* -------------- */
	if (projcode == GCTP_GEO)
	{
	    /* Convert upleft and lowright X coords from DMS to radians */
	    /* -------------------------------------------------------- */
	    lonrad0 = EHconvAng(upleftpt[0], HDFE_DMS_RAD);
	    lonrad = EHconvAng(lowrightpt[0], HDFE_DMS_RAD);

	    /* Compute x scale factor */
	    /* ---------------------- */
	    scaleX = (lonrad - lonrad0) / xdimsize;


	    /* Convert upleft and lowright Y coords from DMS to radians */
	    /* -------------------------------------------------------- */
	    latrad0 = EHconvAng(upleftpt[1], HDFE_DMS_RAD);
	    latrad = EHconvAng(lowrightpt[1], HDFE_DMS_RAD);


	    /* Compute y scale factor */
	    /* ---------------------- */
	    scaleY = (latrad - latrad0) / ydimsize;
	}

	/* BCEA projection */
        /* -------------- */
	else if ( projcode == GCTP_BCEA)
	{
	    /* Convert upleft and lowright X coords from DMS to radians */
	    /* -------------------------------------------------------- */

	    lonrad0 = EHconvAng(upleftpt[0], HDFE_DMS_RAD);
	    lonrad = EHconvAng(lowrightpt[0], HDFE_DMS_RAD);

	    /* Convert upleft and lowright Y coords from DMS to radians */
	    /* -------------------------------------------------------- */
	    latrad0 = EHconvAng(upleftpt[1], HDFE_DMS_RAD);
	    latrad = EHconvAng(lowrightpt[1], HDFE_DMS_RAD);

	    /* Convert from lon/lat to meters(or whatever unit is, i.e unit
	       of r_major and r_minor) using GCTP */
	    /* ----------------------------------------- */
	    errorcode = for_trans[projcode] (lonrad0, latrad0, &xMtr0, &yMtr0);
	    
	    
	    /* Report error if any */
	    /* ------------------- */
	    if (errorcode != 0)
	    {
		status = -1;
		HEpush(DFE_GENAPP, "GDll2ij", __FILE__, __LINE__);
		HEreport("GCTP Error: %d\n", errorcode);
		return (status);
	    }

	    /* Convert from lon/lat to meters(or whatever unit is, i.e unit
	       of r_major and r_minor) using GCTP */
	    /* ----------------------------------------- */
	    errorcode = for_trans[projcode] (lonrad, latrad, &xMtr1, &yMtr1);
	    
	    
	    /* Report error if any */
	    /* ------------------- */
	    if (errorcode != 0)
	    {
		status = -1;
		HEpush(DFE_GENAPP, "GDll2ij", __FILE__, __LINE__);
		HEreport("GCTP Error: %d\n", errorcode);
		return (status);
	    }

	    /* Compute x scale factor */
	    /* ---------------------- */
	    scaleX = (xMtr1 - xMtr0) / xdimsize;

	    /* Compute y scale factor */
	    /* ---------------------- */
	    scaleY = (yMtr1 - yMtr0) / ydimsize;
	}
	else
	{
	    /* Non-GEO, Non_BCEA projections */
	    /* ---------------------------- */

	    /* Compute x & y scale factors */
	    /* --------------------------- */
	    scaleX = (lowrightpt[0] - upleftpt[0]) / xdimsize;
	    scaleY = (lowrightpt[1] - upleftpt[1]) / ydimsize;
	}



	/* Loop through all points */
	/* ----------------------- */
	for (i = 0; i < npnts; i++)
	{
	    /* Convert lon & lat from decimal degrees to radians */
	    /* ------------------------------------------------- */
	    lonrad = EHconvAng(longitude[i], HDFE_DEG_RAD);
	    latrad = EHconvAng(latitude[i], HDFE_DEG_RAD);


	    /* GEO projection */
	    /* -------------- */
	    if (projcode == GCTP_GEO)
	    {
	        /* allow map to span dateline */
	        lonrad0 = EHconvAng(upleftpt[0], HDFE_DMS_RAD);
	        lonrad1 = EHconvAng(lowrightpt[0], HDFE_DMS_RAD);
		/* if time-line is passed */
		if(lonrad < lonrad1)
		  {
		    if (lonrad < lonrad0) lonrad += 2.0 * M_PI;
		    if (lonrad > lonrad1) lonrad -= 2.0 * M_PI;
		  }

		/* Compute scaled distance to point from origin */
		/* -------------------------------------------- */
		xVal = (lonrad - lonrad0) / scaleX;
		yVal = (latrad - latrad0) / scaleY;
	    }
	    else
	    {
		/* Convert from lon/lat to meters using GCTP */
		/* ----------------------------------------- */
		errorcode = for_trans[projcode] (lonrad, latrad, &xMtr, &yMtr);

		
		/* Report error if any */
		/* ------------------- */
		if (errorcode != 0)
		{
		  /*status = -1;
		    HEpush(DFE_GENAPP, "GDll2ij", __FILE__, __LINE__);
		    HEreport("GCTP Error: %d\n", errorcode);
		    return (status); */                  /* Bruce Beaumont */
		    xVal = -2147483648.0;	         /* Bruce Beaumont */
		    yVal = -2147483648.0;	         /* Bruce Beaumont */
		}/* (Note: MAXLONG is defined as 2147483647.0 in
		    function cproj.c of GCTP) */
		else {
		  /* if projection is BCEA normalize x and y by cell size and
		     measure it from the upperleft corner of the grid */
		  
		  /* Compute scaled distance to point from origin */
		  /* -------------------------------------------- */
		  if(  projcode == GCTP_BCEA)
		    {
		      xVal = (xMtr - xMtr0) / scaleX;
		      yVal = (yMtr - yMtr0) / scaleY; 
		    }
		  else
		    {
		      xVal = (xMtr - upleftpt[0]) / scaleX;
		      yVal = (yMtr - upleftpt[1]) / scaleY;
		    }
		}
	    }


	    /* Compute row and col from scaled distance */
	    /* ---------------------------------------- */
	    col[i] = (int32) xVal;
	    row[i] = (int32) yVal;

	    /* Store scaled distances if requested */
	    /* ----------------------------------- */
	    if (xval != NULL)
	    {
		xval[i] = xVal;
	    }

	    if (yval != NULL)
	    {
		yval[i] = yVal;
	    }
	}
    }
    return (status);
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDrs2ll                                                          |
|                                                                             |
|  DESCRIPTION:  Converts EASE grid's (r,s) coordinates to longitude and      |
|                latitude (in decimal degrees).                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  projcode       int32               GCTP projection code                    |
|  projparm       float64             Projection parameters                   |
|  xdimsize       int32               xdimsize from GDcreate                  |
|  ydimsize       int32               ydimsize from GDcreate                  |
|  pixcen         int32               pixel center code                       |
|  npnts          int32               number of lon-lat points                |
|  s              int32               s coordinate                            |
|  r              int32               r coordinate                            |
|  pixcen         int32               Code from GDpixreginfo                  |
|  pixcnr         int32               Code from GDorigininfo                  |
|  upleft         float64             upper left corner coordinates (DMS)     |
|  lowright       float64             lower right corner coordinates (DMS)    |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|  longitude      float64             longitude array (decimal degrees)       |
|  latitude       float64             latitude array  (decimal degrees)       |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jul 00   Abe Taaheri   Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
GDrs2ll(int32 projcode, float64 projparm[],
	int32 xdimsize, int32 ydimsize,
	float64 upleft[], float64 lowright[],
	int32 npnts, float64 r[], float64 s[],
	float64 longitude[], float64 latitude[], int32 pixcen, int32 pixcnr)
{
    intn            i;		    /* Loop index */
    intn            status = 0;	    /* routine return status variable */

    int32           errorcode = 0;  /* GCTP error code */
    int32(*inv_trans[100]) ();	    /* GCTP function pointer */

    float64         pixadjX = 0.0;  /* Pixel adjustment (x) */
    float64         pixadjY = 0.0;  /* Pixel adjustment (y) */
    float64         lonrad;	    /* Longitude in radians of point */
    float64         latrad;	    /* Latitude in radians of point */
    float64         EHconvAng();    /* Angle conversion routine */
    float64         xMtr;	    /* X value in meters from GCTP */
    float64         yMtr;	    /* Y value in meters from GCTP */
    float64         epsilon;
    float64         beta;
    float64         qp_cea = 0;
    float64         kz_cea = 0;
    float64         eccen, eccen_sq;
    float64         phi1, sinphi1, cosphi1;
    float64         scaleX, scaleY;
    
    int32           zonecode=0;
    
    int32           spherecode=0;
    float64         lon[2],lat[2];
    float64         xcor[2], ycor[2];
    int32           nlatlon;

    /* If projection is BCEA define scale, r0 and s0 */
    if (projcode == GCTP_BCEA)
    {
	eccen_sq = 1.0 - SQUARE(projparm[1]/projparm[0]);
	eccen = sqrt(eccen_sq);
	if(eccen < 0.00001)
	  {
	    qp_cea = 2.0;
	  }
	else
	  {
	    qp_cea = 
	      (1.0 - eccen_sq)*((1.0/(1.0 - eccen_sq))-(1.0/(2.0*eccen))*
				log((1.0 - eccen)/(1.0 + eccen)));
	  }
	phi1 = EHconvAng(projparm[5],HDFE_DMS_RAD);
	cosphi1 = cos(phi1);
	sinphi1 = sin(phi1);
	kz_cea = cosphi1/(sqrt(1.0 - (eccen_sq*sinphi1*sinphi1)));
    }
    



    /* Compute adjustment of position within pixel */
    /* ------------------------------------------- */
    if (pixcen == HDFE_CENTER)
    {
	/* Pixel defined at center */
	/* ----------------------- */
	pixadjX = 0.5;
	pixadjY = 0.5;
    }
    else
    {
	switch (pixcnr)
	{
	case HDFE_GD_UL:
	    {
		/* Pixel defined at upper left corner */
		/* ---------------------------------- */
		pixadjX = 0.0;
		pixadjY = 0.0;
		break;
	    }

	case HDFE_GD_UR:
	    {
		/* Pixel defined at upper right corner */
		/* ----------------------------------- */
		pixadjX = 1.0;
		pixadjY = 0.0;
		break;
	    }

	case HDFE_GD_LL:
	    {
		/* Pixel defined at lower left corner */
		/* ---------------------------------- */
		pixadjX = 0.0;
		pixadjY = 1.0;
		break;
	    }

	case HDFE_GD_LR:
	    {
		/* Pixel defined at lower right corner */
		/* ----------------------------------- */
		pixadjX = 1.0;
		pixadjY = 1.0;
		break;
	    }

	}
    }

    /* If projection is BCEA call GCTP initialization routine */
    /* ------------------------------------------------------ */
    if (projcode == GCTP_BCEA)
    {
	
	inv_init(projcode, 0, projparm, 0, NULL, NULL,
		 &errorcode, inv_trans);
	
	/* Report error if any */
	/* ------------------- */
	if (errorcode != 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "GDrs2ll", __FILE__, __LINE__);
	    HEreport("GCTP Error: %d\n", errorcode);
	}
	else
	{
	    /* For each point ... */
	    /* ------------------ */
	  for (i = 0; i < npnts; i++)
	    {
	      /* Convert from EASE grid's (r,s) to lon/lat (radians) 
		 using GCTP */
	      /* --------------------------------------------------- */
	      nlatlon = 2;
	      lon[0] = upleft[0];
	      lon[1] = lowright[0];
	      lat[0] = upleft[1];
	      lat[1] = lowright[1];
	      status = 
		GDll2mm_cea(projcode,zonecode,spherecode,projparm,
			    xdimsize, ydimsize,
			    upleft, lowright, nlatlon,
			    lon, lat,
			    xcor, ycor, &scaleX, &scaleY);
	      
	      if (status == -1)
		{
		  HEpush(DFE_GENAPP, "GDrs2ll", __FILE__, __LINE__);
		  return (status);
		}
	      
	      xMtr = (r[i]/ scaleX + pixadjX - 0.5)* scaleX;
	      yMtr = - (s[i]/fabs(scaleY) + pixadjY - 0.5)* fabs(scaleY);

	      
	      /* allow .5 cell tolerance in arcsin function 
		 (used in bceainv function) so that grid
		 coordinates which are less than .5 cells
		 above 90.00N or below 90.00S are given lat of 90.00 
	      */

	      epsilon = 1 + 0.5 * (fabs(scaleY)/projparm[0]);
	      beta = 2.0 * (yMtr - projparm[7]) * kz_cea/(projparm[0] * qp_cea);
	      
	      if( fabs (beta) > epsilon)
		{
		  status = -1;
		  HEpush(DFE_GENAPP, "GDrs2ll", __FILE__, __LINE__);
		  HEreport("GCTP Error: %s %s %s\n", "grid coordinates",
			   "are more than .5 cells",
			   "above 90.00N or below 90.00S. ");
		  return (status);
		}
	      else if( beta <= -1)
		{
		  errorcode = inv_trans[projcode] (xMtr, 0.0,
						   &lonrad, &latrad);
		  latrad = - M_PI/2;
		}
	      else if( beta >= 1)
		{
		  errorcode = inv_trans[projcode] (xMtr, 0.0,
						   &lonrad, &latrad);
		  latrad = M_PI/2;
		}
	      else
		{
		  errorcode = inv_trans[projcode] (xMtr, yMtr,
						   &lonrad, &latrad);
		}
	      
	      /* Report error if any */
	      /* ------------------- */
	      if (errorcode != 0)
		{
		  status = -1;
		  HEpush(DFE_GENAPP, "GDrs2ll", __FILE__, __LINE__);
		  HEreport("GCTP Error: %d\n", errorcode);
		  return (status);
		}
	      
	      /* Convert from radians to decimal degrees */
	      /* --------------------------------------- */
	      longitude[i] = EHconvAng(lonrad, HDFE_RAD_DEG);
	      latitude[i] = EHconvAng(latrad, HDFE_RAD_DEG);
	    }
	}
    }



    
    return (status);
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: lamaxDxDtheta                                                    |
|                                                                             |
|  DESCRIPTION: Partial derivative along longitude line for Lambert Azimuthal |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|                 float64             Dx/D(theta) for LAMAZ projection        |
|                                                                             |
|  INPUTS:                                                                    |
|  parms          float64             Parameters defining partial derivative  |
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Nov 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
static float64
lamazDxDtheta(float64 parms[])
{
    float64         snTheta, sn2Theta, snTheta1, csTheta1, csLamda;

    snTheta = sin(EHconvAng(parms[0], HDFE_DEG_RAD));
    sn2Theta = sin(2 * EHconvAng(parms[0], HDFE_DEG_RAD));
    snTheta1 = sin(EHconvAng(parms[1], HDFE_DEG_RAD));
    csTheta1 = cos(EHconvAng(parms[1], HDFE_DEG_RAD));
    csLamda = cos(EHconvAng(parms[2], HDFE_DEG_RAD) -
		  EHconvAng(parms[3], HDFE_DEG_RAD));

    return (4 * snTheta +
	    (csTheta1 * csLamda * sn2Theta) +
	    (2 * snTheta1 * (1 + (snTheta * snTheta))));
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: lamaxDxDlamda                                                    |
|                                                                             |
|  DESCRIPTION: Partial derivative along latitude line for Lambert Azimuthal  |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|                 float64             Dx/D(lamda) for LAMAZ projection        |
|                                                                             |
|  INPUTS:                                                                    |
|  parms          float64             Parameters defining partial derivative  |
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Nov 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
static float64
lamazDxDlamda(float64 parms[])
{
    float64         snTheta, csTheta, snTheta1, csTheta1, csLamda;
    float64         cs, sn;

    snTheta = sin(EHconvAng(parms[2], HDFE_DEG_RAD));
    csTheta = cos(EHconvAng(parms[2], HDFE_DEG_RAD));
    snTheta1 = sin(EHconvAng(parms[1], HDFE_DEG_RAD));
    csTheta1 = cos(EHconvAng(parms[1], HDFE_DEG_RAD));
    csLamda = cos(EHconvAng(parms[0], HDFE_DEG_RAD) -
		  EHconvAng(parms[3], HDFE_DEG_RAD));

    cs = csTheta * csTheta1;
    sn = snTheta * snTheta1;

    return (cs + (2 * (1 + sn) + (cs * csLamda)) * csLamda);
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: lamaxDyDtheta                                                    |
|                                                                             |
|  DESCRIPTION: Partial derivative along longitude line for Lambert Azimuthal |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|                 float64             Dy/D(theta) for LAMAZ projection        |
|                                                                             |
|  INPUTS:                                                                    |
|  parms          float64             Parameters defining partial derivative  |
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Nov 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
static float64
lamazDyDtheta(float64 parms[])
{
    float64         snTheta, csTheta, snTheta1, csTheta1, csLamda;
    float64         sn2, cs2, sndiff;

    snTheta = sin(EHconvAng(parms[0], HDFE_DEG_RAD));
    csTheta = cos(EHconvAng(parms[0], HDFE_DEG_RAD));
    snTheta1 = sin(EHconvAng(parms[1], HDFE_DEG_RAD));
    csTheta1 = cos(EHconvAng(parms[1], HDFE_DEG_RAD));
    csLamda = cos(EHconvAng(parms[2], HDFE_DEG_RAD) -
		  EHconvAng(parms[3], HDFE_DEG_RAD));

    sn2 = snTheta1 * snTheta;
    cs2 = csTheta1 * csTheta;
    sndiff = snTheta1 - snTheta;

    return (cs2 * (sn2 * (1 + (csLamda * csLamda)) + 2) +
	    csLamda * (2 * (1 + sn2 * sn2) - (sndiff * sndiff)));
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: homDyDtheta                                                      |
|                                                                             |
|  DESCRIPTION: Partial derivative along longitude line for Oblique Mercator  |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|                 float64             Dx/D(theta) for HOM projection          |
|                                                                             |
|  INPUTS:                                                                    |
|  parms          float64             Parameters defining partial derivative  |
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Mar 97   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
static float64
homDyDtheta(float64 parms[])
{
    float64         tnTheta, tnTheta1, snLamda;

    tnTheta = tan(EHconvAng(parms[0], HDFE_DEG_RAD));
    tnTheta1 = tan(EHconvAng(parms[1], HDFE_DEG_RAD));
    snLamda = cos(EHconvAng(parms[2], HDFE_DEG_RAD) -
		  EHconvAng(parms[3], HDFE_DEG_RAD));

    return (tnTheta * snLamda + tnTheta1);
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDtangentpnts                                                    |
|                                                                             |
|  DESCRIPTION: Finds tangent points along lon/lat lines                      |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  projcode       int32               Projection code                         |
|  projparm       float64             Projection parameters                   |
|  cornerlon      float64  dec deg    Longitude of opposite corners of box    |
|  cornerlat      float64  dec deg    Latitude of opposite corners of box     |
|  longitude      float64  dec deg    Longitude of points to check            |
|  latitude       float64  dec deg    Latitude of points to check             |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|  npnts          int32               Number of points to check in subset     |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Nov 96   Joel Gales    Original Programmer                                 |
|  Mar 97   Joel Gales    Add support for LAMCC, POLYC, TM                    |
|  Aug 99   Abe Taaheri   Add support for ALBERS, and MERCAT projections.     |
|                         Also changed mistyped bisectParm[2] to              |
|                         bisectParm[3] for HOM projection.                   |
|  Jun 00   Abe Taaheri   Added support for EASE grid                         |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
static intn
GDtangentpnts(int32 projcode, float64 projparm[], float64 cornerlon[],
	      float64 cornerlat[], float64 longitude[], float64 latitude[],
	      int32 * npnts)
{
    intn            i;		/* Loop index */
    intn            status = 0;	/* routine return status variable */

    float64         lonrad;	/* Longitude (radians) */
    float64         latrad;	/* Latitude (radians) */
    float64         cs[4];	/* Cosine array */
    float64         sn[4];	/* Sine array */
    float64         csTest;	/* Cosine test value */
    float64         snTest;	/* Sine test value */
    float64         crs01;	/* Cross product */
    float64         crsTest[2];	/* Cross product array */
    float64         longPol;	/* Longitude beneath pole */
    float64         minLat;	/* Minimum latitude */
    float64         bisectParm[4];	/* Bisection parameters */
    float64         tanLat;	/* Tangent latitude */
    float64         tanLon;	/* Tangent longitude */
    float64         dotPrd;	/* Dot product */
    float64         centMerd;	/* Central Meridian */
    float64         orgLat;	/* Latitude of origin */
    float64         dpi;	/* Double precision pi */

#if 0
    float64         lamazDxDtheta();	/* Lambert Azimuthal Dx/Dtheta */
    float64         lamazDxDlamda();	/* Lambert Azimuthal Dx/Dlamda */
    float64         lamazDyDtheta();	/* Lambert Azimuthal Dy/Dtheta */
    float64         homDyDtheta();	/* Oblique Mercator  Dy/Dtheta */
#endif

    /* Compute pi (double precision) */
    /* ----------------------------- */
    dpi = atan(1.0) * 4;


    switch (projcode)
    {
      case GCTP_MERCAT:
	{
	    /* No need for tangent points, since MERCAT projection 
	       is rectangular */
	}
	break;
      case GCTP_BCEA:
	{
	    /* No need for tangent points, since BCEA projection 
	       is rectangular */
	}
	break;
     case GCTP_CEA:
      {
             /* No need for tangent points, since CEA projection
                is rectangular */
      }
      break;

      case GCTP_PS:
	{
	    /* Add "xy axis" points for Polar Stereographic if necessary */
	    /* --------------------------------------------------------- */


	    /* Get minimum of corner latitudes */
	    /* ------------------------------- */
	    minLat = (fabs(cornerlat[0]) <= fabs(cornerlat[1]))
		? cornerlat[0] : cornerlat[1];


	    /* Compute sine and cosine of corner longitudes */
	    /* -------------------------------------------- */
	    for (i = 0; i < 2; i++)
	    {
		lonrad = EHconvAng(cornerlon[i], HDFE_DEG_RAD);
		cs[i] = cos(lonrad);
		sn[i] = sin(lonrad);
	    }


	    /* Compute cross product */
	    /* --------------------- */
	    crs01 = cs[0] * sn[1] - cs[1] * sn[0];


	    /* Convert longitude beneath pole from DMS to DEG */
	    /* ---------------------------------------------- */
	    longPol = EHconvAng(projparm[4], HDFE_DMS_RAD);



	    for (i = 0; i < 4; i++)
	    {
		csTest = cos(longPol);
		snTest = sin(longPol);

		crsTest[0] = cs[0] * snTest - csTest * sn[0];
		crsTest[1] = cs[1] * snTest - csTest * sn[1];

		if ((crs01 > 0 && crsTest[0] > 0 && crsTest[1] < 0) ||
		    (crs01 < 0 && crsTest[0] < 0 && crsTest[1] < 0) ||
		    (crs01 < 0 && crsTest[0] > 0 && crsTest[1] < 0) ||
		    (crs01 < 0 && crsTest[0] > 0 && crsTest[1] > 0))
		{
		    longitude[*npnts] = EHconvAng(longPol, HDFE_RAD_DEG);
		    latitude[*npnts] = minLat;
		    (*npnts)++;
		}
		longPol += 0.5 * dpi;
	    }
	}
	break;


      case GCTP_LAMAZ:
	{
	    if ((int32) projparm[5] == +90000000 ||
		(int32) projparm[5] == -90000000)
	    {
		/* Add "xy axis" points for Polar Lambert Azimuthal */
		/* ------------------------------------------------ */
		minLat = (fabs(cornerlat[0]) <= fabs(cornerlat[1]))
		    ? cornerlat[0] : cornerlat[1];

		for (i = 0; i < 2; i++)
		{
		    lonrad = EHconvAng(cornerlon[i], HDFE_DEG_RAD);
		    cs[i] = cos(lonrad);
		    sn[i] = sin(lonrad);
		}
		crs01 = cs[0] * sn[1] - cs[1] * sn[0];

		longPol = EHconvAng(projparm[4], HDFE_DMS_RAD);
		for (i = 0; i < 4; i++)
		{
		    csTest = cos(longPol);
		    snTest = sin(longPol);

		    crsTest[0] = cs[0] * snTest - csTest * sn[0];
		    crsTest[1] = cs[1] * snTest - csTest * sn[1];

		    if ((crs01 > 0 && crsTest[0] > 0 && crsTest[1] < 0) ||
			(crs01 < 0 && crsTest[0] < 0 && crsTest[1] < 0) ||
			(crs01 < 0 && crsTest[0] > 0 && crsTest[1] < 0) ||
			(crs01 < 0 && crsTest[0] > 0 && crsTest[1] > 0))
		    {
			longitude[*npnts] = EHconvAng(longPol, HDFE_RAD_DEG);
			latitude[*npnts] = minLat;
			(*npnts)++;
		    }
		    longPol += 0.5 * dpi;
		}
	    }
	    else if ((int32) projparm[5] == 0)
	    {
		/* Add "Equator" points for Equatorial Lambert Azimuthal */
		/* ----------------------------------------------------- */
		if (cornerlat[0] * cornerlat[1] < 0)
		{
		    longitude[4] = cornerlon[0];
		    latitude[4] = 0;

		    longitude[5] = cornerlon[1];
		    latitude[5] = 0;

		    *npnts = 6;
		}
	    }
	    else
	    {
		/* Add tangent points for Oblique Lambert Azimuthal */
		/* ------------------------------------------------ */
		bisectParm[0] = EHconvAng(projparm[5], HDFE_DMS_DEG);
		bisectParm[2] = EHconvAng(projparm[4], HDFE_DMS_DEG);


		/* Tangent to y-axis along longitude */
		/* --------------------------------- */
		for (i = 0; i < 2; i++)
		{
		    bisectParm[1] = cornerlon[i];

		    if (EHbisect(lamazDxDtheta, bisectParm, 3,
				 cornerlat[0], cornerlat[1],
				 0.0001, &tanLat) == 0)
		    {
			longitude[*npnts] = cornerlon[i];
			latitude[*npnts] = tanLat;
			(*npnts)++;
		    }
		}

		/* Tangent to y-axis along latitude */
		/* -------------------------------- */
		for (i = 0; i < 2; i++)
		{
		    bisectParm[1] = cornerlat[i];

		    if (EHbisect(lamazDxDlamda, bisectParm, 3,
				 cornerlon[0], cornerlon[1],
				 0.0001, &tanLon) == 0)
		    {
			longitude[*npnts] = tanLon;
			latitude[*npnts] = cornerlat[i];
			(*npnts)++;
		    }
		}


		/* Tangent to x-axis along longitude */
		/* --------------------------------- */
		for (i = 0; i < 2; i++)
		{
		    bisectParm[1] = cornerlon[i];

		    if (EHbisect(lamazDyDtheta, bisectParm, 3,
				 cornerlat[0], cornerlat[1],
				 0.0001, &tanLat) == 0)
		    {
			longitude[*npnts] = cornerlon[i];
			latitude[*npnts] = tanLat;
			(*npnts)++;
		    }
		}

		/* Tangent to x-axis along latitude */
		/* -------------------------------- */
		for (i = 0; i < 2; i++)
		{
		    lonrad = EHconvAng(cornerlon[i], HDFE_DEG_RAD);
		    cs[i] = cos(lonrad);
		    sn[i] = sin(lonrad);
		}
		crs01 = cs[0] * sn[1] - cs[1] * sn[0];

		longPol = EHconvAng(projparm[4], HDFE_DMS_RAD);
		for (i = 0; i < 2; i++)
		{
		    csTest = cos(longPol);
		    snTest = sin(longPol);

		    crsTest[0] = cs[0] * snTest - csTest * sn[0];
		    crsTest[1] = cs[1] * snTest - csTest * sn[1];

		    if ((crs01 > 0 && crsTest[0] > 0 && crsTest[1] < 0) ||
			(crs01 < 0 && crsTest[0] < 0 && crsTest[1] < 0) ||
			(crs01 < 0 && crsTest[0] > 0 && crsTest[1] < 0) ||
			(crs01 < 0 && crsTest[0] > 0 && crsTest[1] > 0))
		    {
			longitude[*npnts] = EHconvAng(longPol, HDFE_RAD_DEG);
			latitude[*npnts] = cornerlat[0];
			(*npnts)++;
			longitude[*npnts] = EHconvAng(longPol, HDFE_RAD_DEG);
			latitude[*npnts] = cornerlat[1];
			(*npnts)++;
		    }
		    longPol += dpi;
		}
	    }
	}
	break;


      case GCTP_GOOD:
	{
	    /* Add "Equator" points for Goode Homolosine if necessary */
	    /* ------------------------------------------------------ */
	    if (cornerlat[0] * cornerlat[1] < 0)
	    {
		longitude[4] = cornerlon[0];
		latitude[4] = 0;

		longitude[5] = cornerlon[1];
		latitude[5] = 0;

		*npnts = 6;
	    }
	}
	break;


      case GCTP_LAMCC:
	{
	    /* Compute sine and cosine of corner longitudes */
	    /* -------------------------------------------- */
	    for (i = 0; i < 2; i++)
	    {
		lonrad = EHconvAng(cornerlon[i], HDFE_DEG_RAD);
		cs[i] = cos(lonrad);
		sn[i] = sin(lonrad);
	    }


	    /* Compute dot product */
	    /* ------------------- */
	    dotPrd = cs[0] * cs[1] + sn[0] * sn[1];


	    /* Convert central meridian (DMS to DEG) & compute sin & cos */
	    /* --------------------------------------------------------- */
	    centMerd = EHconvAng(projparm[4], HDFE_DMS_DEG);
	    lonrad = EHconvAng(centMerd, HDFE_DEG_RAD);
	    cs[1] = cos(lonrad);
	    sn[1] = sin(lonrad);


	    /* If box brackets central meridian ... */
	    /* ------------------------------------ */
	    if (cs[0] * cs[1] + sn[0] * sn[1] > dotPrd)
	    {
		latitude[4] = cornerlat[0];
		longitude[4] = centMerd;

		latitude[5] = cornerlat[1];
		longitude[5] = centMerd;

		*npnts = 6;
	    }
	}
	break;


      case GCTP_ALBERS:
	{
	    /* Compute sine and cosine of corner longitudes */
	    /* -------------------------------------------- */
	    for (i = 0; i < 2; i++)
	    {
		lonrad = EHconvAng(cornerlon[i], HDFE_DEG_RAD);
		cs[i] = cos(lonrad);
		sn[i] = sin(lonrad);
	    }


	    /* Compute dot product */
	    /* ------------------- */
	    dotPrd = cs[0] * cs[1] + sn[0] * sn[1];


	    /* Convert central meridian (DMS to DEG) & compute sin & cos */
	    /* --------------------------------------------------------- */
	    centMerd = EHconvAng(projparm[4], HDFE_DMS_DEG);
	    lonrad = EHconvAng(centMerd, HDFE_DEG_RAD);
	    cs[1] = cos(lonrad);
	    sn[1] = sin(lonrad);


	    /* If box brackets central meridian ... */
	    /* ------------------------------------ */
	    if (cs[0] * cs[1] + sn[0] * sn[1] > dotPrd)
	    {
		latitude[4] = cornerlat[0];
		longitude[4] = centMerd;

		latitude[5] = cornerlat[1];
		longitude[5] = centMerd;

		*npnts = 6;
	    }
	}
	break;


      case GCTP_POLYC:
	{
	    /* Compute sine and cosine of corner longitudes */
	    /* -------------------------------------------- */
	    for (i = 0; i < 2; i++)
	    {
		lonrad = EHconvAng(cornerlon[i], HDFE_DEG_RAD);
		cs[i] = cos(lonrad);
		sn[i] = sin(lonrad);
	    }


	    /* Compute dot product */
	    /* ------------------- */
	    dotPrd = cs[0] * cs[1] + sn[0] * sn[1];


	    /* Convert central meridian (DMS to DEG) & compute sin & cos */
	    /* --------------------------------------------------------- */
	    centMerd = EHconvAng(projparm[4], HDFE_DMS_DEG);
	    lonrad = EHconvAng(centMerd, HDFE_DEG_RAD);
	    cs[1] = cos(lonrad);
	    sn[1] = sin(lonrad);


	    /* If box brackets central meridian ... */
	    /* ------------------------------------ */
	    if (cs[0] * cs[1] + sn[0] * sn[1] > dotPrd)
	    {
		latitude[4] = cornerlat[0];
		longitude[4] = centMerd;

		latitude[5] = cornerlat[1];
		longitude[5] = centMerd;

		*npnts = 6;
	    }
	}
	break;


      case GCTP_TM:
	{
	    /* Compute sine and cosine of corner longitudes */
	    /* -------------------------------------------- */
	    for (i = 0; i < 2; i++)
	    {
		lonrad = EHconvAng(cornerlon[i], HDFE_DEG_RAD);
		cs[i] = cos(lonrad);
		sn[i] = sin(lonrad);
	    }


	    /* Compute dot product */
	    /* ------------------- */
	    dotPrd = cs[0] * cs[1] + sn[0] * sn[1];


	    for (i = -1; i <= 1; i++)
	    {
		centMerd = EHconvAng(projparm[4], HDFE_DMS_DEG);
		lonrad = EHconvAng(centMerd + 90 * i, HDFE_DEG_RAD);
		csTest = cos(lonrad);
		snTest = sin(lonrad);


		/* If box brackets meridian ... */
		/* ---------------------------- */
		if (csTest * cs[1] + snTest * sn[1] > dotPrd)
		{
		    latitude[*npnts] = cornerlat[0];
		    longitude[*npnts] = centMerd;
		    (*npnts)++;

		    latitude[*npnts] = cornerlat[1];
		    longitude[*npnts] = centMerd;
		    (*npnts)++;
		}
	    }



	    /* Compute sine and cosine of corner latitudes */
	    /* ------------------------------------------- */
	    for (i = 0; i < 2; i++)
	    {
		latrad = EHconvAng(cornerlat[i], HDFE_DEG_RAD);
		cs[i] = cos(latrad);
		sn[i] = sin(latrad);
	    }


	    /* Compute dot product */
	    /* ------------------- */
	    dotPrd = cs[0] * cs[1] + sn[0] * sn[1];


	    /* Convert origin latitude (DMS to DEG) & compute sin & cos */
	    /* -------------------------------------------------------- */
	    orgLat = EHconvAng(projparm[5], HDFE_DMS_DEG);
	    latrad = EHconvAng(orgLat, HDFE_DEG_RAD);
	    cs[1] = cos(latrad);
	    sn[1] = sin(latrad);


	    /* If box brackets origin latitude ... */
	    /* ----------------------------------- */
	    if (cs[0] * cs[1] + sn[0] * sn[1] > dotPrd)
	    {
		latitude[*npnts] = orgLat;
		longitude[*npnts] = cornerlon[0];
		(*npnts)++;

		latitude[*npnts] = orgLat;
		longitude[*npnts] = cornerlon[1];
		(*npnts)++;
	    }
	}
	break;


      case GCTP_HOM:
	{
	    /* Tangent to y-axis along longitude */
	    /* --------------------------------- */
	    if (projparm[12] == 0)
	    {
		cs[0] = cos(EHconvAng(projparm[8], HDFE_DMS_RAD));
		sn[0] = sin(EHconvAng(projparm[8], HDFE_DMS_RAD));
		cs[1] = cos(EHconvAng(projparm[9], HDFE_DMS_RAD));
		sn[1] = sin(EHconvAng(projparm[9], HDFE_DMS_RAD));
		cs[2] = cos(EHconvAng(projparm[10], HDFE_DMS_RAD));
		sn[2] = sin(EHconvAng(projparm[10], HDFE_DMS_RAD));
		cs[3] = cos(EHconvAng(projparm[11], HDFE_DMS_RAD));
		sn[3] = sin(EHconvAng(projparm[11], HDFE_DMS_RAD));

		bisectParm[3] = atan2(
			    (cs[1] * sn[3] * cs[0] - sn[1] * cs[3] * cs[2]),
			   (sn[1] * cs[3] * sn[2] - cs[1] * sn[3] * sn[0]));
		bisectParm[0] = atan(
		 (sin(bisectParm[3]) * sn[0] - cos(bisectParm[3]) * cs[0]) /
				     (sn[1] / cs[1]));
		bisectParm[2] = bisectParm[3] + 0.5 * dpi;
	    }
	    else
	    {
		cs[0] = cos(EHconvAng(projparm[3], HDFE_DMS_RAD));
		sn[0] = sin(EHconvAng(projparm[3], HDFE_DMS_RAD));
		cs[1] = cos(EHconvAng(projparm[4], HDFE_DMS_RAD));
		sn[1] = sin(EHconvAng(projparm[4], HDFE_DMS_RAD));

		bisectParm[0] = asin(cs[1] * sn[0]);
		bisectParm[2] = atan2(-cs[0], (-sn[1] * sn[0])) + 0.5 * dpi;
	    }

	    for (i = 0; i < 2; i++)
	    {
		bisectParm[1] = cornerlon[i];

		if (EHbisect(homDyDtheta, bisectParm, 3,
			     cornerlat[0], cornerlat[1],
			     0.0001, &tanLat) == 0)
		{
		    longitude[*npnts] = cornerlon[i];
		    latitude[*npnts] = tanLat;
		    (*npnts)++;
		}
	    }

	}
	break;
    }


    return (status);
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDdefboxregion                                                   |
|                                                                             |
|  DESCRIPTION: Defines region for subsetting in a grid.                      |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  regionID       int32               Region ID                               |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               Grid structure ID                       |
|  cornerlon      float64  dec deg    Longitude of opposite corners of box    |
|  cornerlat      float64  dec deg    Latitude of opposite corners of box     |
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
|  Oct 96   Joel Gales    "Clamp" subset region around grid                   |
|  Oct 96   Joel Gales    Fix "outside region" check                          |
|  Nov 96   Joel Gales    Add check for "tangent" points (GDtangentpnts)      |
|  Dec 96   Joel Gales    Trap if no projection code defined                  |
|  Dec 96   Joel Gales    Add multiple vertical subsetting capability         |
|  Mar 99   David Wynne   Fix for NCR 21195, allow subsetting of MISR SOM     |
|                         data sets                                           |
|  Jun 00   Abe Taaheri   Added support for EASE grid                         |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
GDdefboxregion(int32 gridID, float64 cornerlon[], float64 cornerlat[])
{
    intn            i;		    /* Loop index */
    intn            j;		    /* Loop index */
    intn            k;		    /* Loop index */
    intn            n;		    /* Loop index */
    intn            status = 0;	    /* routine return status variable */

    int32           fid;	    /* HDF-EOS file ID */
    int32           sdInterfaceID;  /* HDF SDS interface ID */
    int32           gdVgrpID;	    /* Grid root Vgroup ID */
    int32           regionID = -1;  /* Region ID */
    int32           xdimsize;	    /* XDim size */
    int32           ydimsize;	    /* YDim size */
    int32           projcode;	    /* Projection code */
    int32           zonecode;	    /* Zone code */
    int32           spherecode;	    /* Sphere code */
    int32           row[32];	    /* Row array */
    int32           col[32];	    /* Column array */
    int32           minCol = 0;	    /* Minimum column value */
    int32           minRow = 0;	    /* Minimum row value */
    int32           maxCol = 0;	    /* Maximum column value */
    int32           maxRow = 0;	    /* Maximum row value */
    int32           npnts;	    /* Number of boundary
                                       (edge & tangent) pnts */

    float64         longitude[32];  /* Longitude array */
    float64         latitude[32];   /* Latitude array */
    float64         upleftpt[2];    /* Upper left pt coordinates */
    float64         lowrightpt[2];  /* Lower right pt coordinates */
    float64         somupleftpt[2]; /* temporary Upper left pt coordinates
                                       for SOM projection */
    float64         somlowrightpt[2];   /* temporary Lower right pt
                                           coordinates for SOM projection */
    float64         projparm[16];   /* Projection parameters */
    float64         xscale;	    /* X scale */
    float64         yscale;	    /* Y scale */
    float64         lonrad0;	    /* Longitude of upper left point
                                       (radians) */
    float64         latrad0;	    /* Latitude of upper left point (radians) */
    float64         lonrad2;	    /* Longitude of point (radians) */
    float64         latrad2;	    /* Latitude of point (radians) */

    /* Used for SOM projection  */
    char            *utlbuf;
    char            *gridname;
    int32	    blockindexstart = -1;
    int32	    blockindexstop = -1;
    float32         offset[180];
    float64         templeftpt[2];
    float64         temprightpt[2];
    int32           idOffset = GDIDOFFSET;	/* Grid ID offset */
    float64         xmtr[2], ymtr[2];
    float64         lon[2],lat[2];
    float64         xcor[2], ycor[2];
    int32           nlatlon;
    float64         upleftpt_m[2];


    utlbuf = (char *)calloc(128, sizeof(char));
    if(utlbuf == NULL)
    { 
	HEpush(DFE_NOSPACE,"GDdefboxregion", __FILE__, __LINE__);
	return(-1);
    }
    gridname = (char *)calloc(128, sizeof(char));
    if(gridname == NULL)
    { 
	HEpush(DFE_NOSPACE,"GDdefboxregion", __FILE__, __LINE__);
	free(utlbuf);
	return(-1);
    }

    /* Check for valid grid ID */
    /* ----------------------- */
    status = GDchkgdid(gridID, "GDdefboxregion",
		       &fid, &sdInterfaceID, &gdVgrpID);


    if (status == 0)
    {
	/* Get grid info */
	/* ------------- */
	status = GDgridinfo(gridID, &xdimsize, &ydimsize,
			    upleftpt, lowrightpt);
	
	
	/* If error then bail */
	/* ------------------ */
	if (status != 0)
	{
	    regionID = -1;
	    free(utlbuf);
	    free(gridname);
	    return (regionID);
	}


	/* Get proj info */
	/* ------------- */
	status = GDprojinfo(gridID, &projcode, &zonecode,
			    &spherecode, projparm);


	/* If no projection code defined then bail */
	/* --------------------------------------- */
	if (projcode == -1)
	{
	    regionID = -1;
	    free(utlbuf);
	    free(gridname);
	    return (regionID);
	}


	/* Get default values for upleft and lowright if necessary */
	/* ------------------------------------------------------- */
	if (upleftpt[0] == 0 && upleftpt[1] == 0 &&
	    lowrightpt[0] == 0 && lowrightpt[1] == 0)
	{
	    status = GDgetdefaults(projcode, zonecode, projparm, spherecode,
				   upleftpt, lowrightpt);

	    /* If error then bail */
	    /* ------------------ */
	    if (status != 0)
	    {
		regionID = -1;
		free(utlbuf);
		free(gridname);
		return (regionID);
	    }
	}



	/* Fill-up longitude and latitude arrays */
	/* ------------------------------------- */
	longitude[0] = cornerlon[0];
	latitude[0] = cornerlat[0];

	longitude[1] = cornerlon[0];
	latitude[1] = cornerlat[1];

	longitude[2] = cornerlon[1];
	latitude[2] = cornerlat[0];

	longitude[3] = cornerlon[1];
	latitude[3] = cornerlat[1];

	npnts = 4;


	/* Find additional tangent points from GDtangentpnts */
	/* ------------------------------------------------- */
	status = GDtangentpnts(projcode, projparm, cornerlon, cornerlat,
			       longitude, latitude, &npnts);
 
        /* If SOM projection with projparm[11] non-zero ... */
        if (projcode == GCTP_SOM && projparm[11] != 0)
        {
            Vgetname(GDXGrid[gridID % idOffset].IDTable, gridname);
            snprintf(utlbuf, 128, "%s%s", "_BLKSOM:", gridname);
	    status = GDreadattr(gridID, utlbuf, offset);

            somupleftpt[0] = upleftpt[0];
            somupleftpt[1] = upleftpt[1];
            somlowrightpt[0]= lowrightpt[0];
            somlowrightpt[1] = lowrightpt[1];

            k = 0;
            n = 2;

            for (j = 0; j <= projparm[11] - 1; j++)
            {

		/* Convert from lon/lat to row/col */
		/* ------------------------------- */
		status = GDll2ij(projcode, zonecode, projparm, spherecode,
			 xdimsize, ydimsize, somupleftpt, somlowrightpt,
			 npnts, longitude, latitude, row, col, NULL, NULL);


	       /* Find min/max values for row & col */
	       /* --------------------------------- */
	       minCol = col[0];
	       minRow = row[0];
	       maxCol = col[0];
	       maxRow = row[0];
	       for (i = 1; i < npnts; i++)
	       {
	          if (col[i] < minCol)
	          {
		     minCol = col[i];
	          }

	          if (col[i] > maxCol)
	          {
		     maxCol = col[i];
	          }

	          if (row[i] < minRow)
	          {
		     minRow = row[i];
	          }

	          if (row[i] > maxRow)
	          {
		     maxRow = row[i];
	           }
	        }



	        /* "Clamp" if outside Grid */
	        /* ----------------------- */
	        minCol = (minCol < 0) ? 0 : minCol;
	        minRow = (minRow < 0) ? 0 : minRow;

	        maxCol = (maxCol >= xdimsize) ? xdimsize - 1 : maxCol;
	        maxRow = (maxRow >= ydimsize) ? ydimsize - 1 : maxRow;


	        /* Check whether subset region is outside grid region */
	        /* -------------------------------------------------- */
	        if (minCol >= xdimsize || minRow >= ydimsize || 
		    maxCol < 0 || maxRow < 0)
	        {
                   if ( blockindexstart == -1 && (projparm[11]) == j)
                   {
                      status = -1;
	              HEpush(DFE_GENAPP, "GDdefboxregion", __FILE__, __LINE__);
	              HEreport("Subset Region outside of Grid Region\n");
                      regionID = -1;
                   }
	        }
                else
                {
                   if (k == 0)
                   {
                      blockindexstart = j;
                      blockindexstop = j;
                      k = 1;
                   }
                   else
                   {
                      blockindexstop = j;
                   }
                }

                // E. Rouault: FIXME: was really abs(int) indented here ? Forcing the cast to int to please compilers
                templeftpt[0] = upleftpt[0] + ((offset[j]/xdimsize)*abs((int)(upleftpt[0] - lowrightpt[0]))) + abs((int)(upleftpt[0] - lowrightpt[0]))*(n-1);
                templeftpt[1] = upleftpt[1] + ((lowrightpt[1] - upleftpt[1]))*(n-1);

                temprightpt[0] = lowrightpt[0] + ((offset[j]/xdimsize)*abs((int)(lowrightpt[0] - upleftpt[0]))) + abs((int)(lowrightpt[0] - upleftpt[0]))*(n-1);
                temprightpt[1] = lowrightpt[1] + ((upleftpt[1] - lowrightpt[1]))*(n-1);

                somupleftpt[0] = templeftpt[0];
                somupleftpt[1] = templeftpt[1];

                somlowrightpt[0] = temprightpt[0];
                somlowrightpt[1] = temprightpt[1];
                n++;
             }
         }
         else
         {

	    /* Convert from lon/lat to row/col */
	    /* ------------------------------- */

	    status = GDll2ij(projcode, zonecode, projparm, spherecode,
			 xdimsize, ydimsize, upleftpt, lowrightpt,
			 npnts, longitude, latitude, row, col, NULL, NULL);

	    /* Find min/max values for row & col */
	    /* --------------------------------- */
	    minCol = col[0];
	    minRow = row[0];
	    maxCol = col[0];
	    maxRow = row[0];
	    for (i = 1; i < npnts; i++)
	    {
	       if (col[i] < minCol)
	       {
		  minCol = col[i];
	       }

	       if (col[i] > maxCol)
	       {
	          maxCol = col[i];
	       }

	       if (row[i] < minRow)
	       {
		  minRow = row[i];
	       }

	       if (row[i] > maxRow)
	       {
		  maxRow = row[i];
	       }
	    }



	    /* "Clamp" if outside Grid */
	    /* ----------------------- */
	    minCol = (minCol < 0) ? 0 : minCol;
	    minRow = (minRow < 0) ? 0 : minRow;

	    maxCol = (maxCol >= xdimsize) ? xdimsize - 1 : maxCol;
	    maxRow = (maxRow >= ydimsize) ? ydimsize - 1 : maxRow;


	    /* Check whether subset region is outside grid region */
	    /* -------------------------------------------------- */
	    if (minCol >= xdimsize || minRow >= ydimsize || maxCol < 0 || maxRow < 0)
	    {
	       status = -1;
	       HEpush(DFE_GENAPP, "GDdefboxregion", __FILE__, __LINE__);
	       HEreport("Subset Region outside of Grid Region\n");
	       regionID = -1;

	    }
         }
	    if (status == 0)
	    {
	       /* Store grid region info */
	       /* ---------------------- */
	       for (i = 0; i < NGRIDREGN; i++)
	       {
		/* Find first empty grid region */
		/* ---------------------------- */
		if (GDXRegion[i] == 0)
		{
		    /* Allocate space for grid region entry */
		    /* ------------------------------------ */
		    GDXRegion[i] = (struct gridRegion *)
			calloc(1, sizeof(struct gridRegion));
		    if(GDXRegion[i] == NULL)
		    { 
			HEpush(DFE_NOSPACE,"GDdefboxregion", __FILE__, __LINE__);
			free(utlbuf);
			free(gridname);
			return(-1);
		    }


		    /* Store file and grid ID */
		    /* ---------------------- */
		    GDXRegion[i]->fid = fid;
		    GDXRegion[i]->gridID = gridID;


		    /* Initialize vertical subset entries to -1 */
		    /* ---------------------------------------- */
		    for (j = 0; j < 8; j++)
		    {
			GDXRegion[i]->StartVertical[j] = -1;
			GDXRegion[i]->StopVertical[j] = -1;
		    }


		    /* Store start & count along x & y */
		    /* ------------------------------- */
		    GDXRegion[i]->xStart = minCol;
		    GDXRegion[i]->xCount = maxCol - minCol + 1;
		    GDXRegion[i]->yStart = minRow;
		    GDXRegion[i]->yCount = maxRow - minRow + 1;


		    /* Store upleft and lowright points of subset region */
		    /* ------------------------------------------------- */
		    if (projcode == GCTP_GEO )
		    {
			/* GEO projection */
			/* ------------------------ */

			/* Convert upleft & lowright lon from DMS to radians */
			/* ------------------------------------------------- */
			lonrad0 = EHconvAng(upleftpt[0], HDFE_DMS_RAD);
			lonrad2 = EHconvAng(lowrightpt[0], HDFE_DMS_RAD);

			/* Compute X scale */
			/* --------------- */
			xscale = (lonrad2 - lonrad0) / xdimsize;

			/* Convert upleft & lowright lat from DMS to radians */
			/* ------------------------------------------------- */
			latrad0 = EHconvAng(upleftpt[1], HDFE_DMS_RAD);
			latrad2 = EHconvAng(lowrightpt[1], HDFE_DMS_RAD);

			/* Compute Y scale */
			/* --------------- */
			yscale = (latrad2 - latrad0) / ydimsize;


			/* MinCol -> radians -> DMS -> upleftpt[0] */
			/* --------------------------------------- */
			GDXRegion[i]->upleftpt[0] =
			    EHconvAng(lonrad0 + xscale * minCol,
				      HDFE_RAD_DMS);


			/* MinRow -> radians -> DMS -> upleftpt[1] */
			/* --------------------------------------- */
			GDXRegion[i]->upleftpt[1] =
			    EHconvAng(latrad0 + yscale * minRow,
				      HDFE_RAD_DMS);


			/* MinCol + 1 -> radians -> DMS -> lowrightpt[0] */
			/* --------------------------------------------- */
			GDXRegion[i]->lowrightpt[0] =
			    EHconvAng(lonrad0 + xscale * (maxCol + 1),
				      HDFE_RAD_DMS);


			/* MinRow + 1 -> radians -> DMS -> lowrightpt[1] */
			/* --------------------------------------------- */
			GDXRegion[i]->lowrightpt[1] =
			    EHconvAng(latrad0 + yscale * (maxRow + 1),
				      HDFE_RAD_DMS);
		    }
		    else if (projcode == GCTP_BCEA)
		    {
			/* BCEA projection */
			/* -------------- */
		      nlatlon = 2;
		      lon[0] = upleftpt[0];
		      lon[1] = lowrightpt[0];
		      lat[0] = upleftpt[1];
		      lat[1] = lowrightpt[1];
		      status = 
			GDll2mm_cea(projcode,zonecode,spherecode,projparm,
				    xdimsize, ydimsize,
				    upleftpt, lowrightpt,nlatlon,
				    lon, lat,
				    xcor, ycor, &xscale, &yscale);
		      upleftpt_m[0] = xcor[0];
		      upleftpt_m[1] = ycor[0];
		      
		      
		      if (status == -1)
			{
			  HEpush(DFE_GENAPP, "GDdefboxregion", __FILE__, __LINE__);
			  free(utlbuf);
			  free(gridname);
			  return (status);
			}

			/* MinCol -> meters -> upleftpt[0] */
			/* ------------------------------- */
			xmtr[0] = upleftpt_m[0] + xscale * minCol;

			/* MinRow -> meters -> upleftpt[1] */
			/* ------------------------------- */
			ymtr[0] = upleftpt_m[1] + yscale * minRow;

			/* MinCol + 1 -> meters -> lowrightpt[0] */
			/* ------------------------------------- */
			xmtr[1] = upleftpt_m[0] + xscale * (maxCol + 1);

			/* MinRow + 1 -> meters -> lowrightpt[1] */
			/* ------------------------------------- */
			ymtr[1] = upleftpt_m[1] + yscale * (maxRow + 1);

			/* Convert upleft & lowright lon from DMS to radians */
			/* ------------------------------------------------- */
			npnts = 2;
			status = GDmm2ll_cea(projcode, zonecode, spherecode,
					     projparm, xdimsize, ydimsize,
					     upleftpt, lowrightpt, npnts,
					     xmtr,  ymtr, 
					     longitude, latitude);
			if (status == -1)
			  {
			    HEpush(DFE_GENAPP, "GDdefboxregion", __FILE__, __LINE__);
			    free(utlbuf);
			    free(gridname);
			    return (status);
			}
			GDXRegion[i]->upleftpt[0] = longitude[0];

			GDXRegion[i]->upleftpt[1] = latitude[0];

			GDXRegion[i]->lowrightpt[0] = longitude[1];

			GDXRegion[i]->lowrightpt[1] = latitude[1];
		    }
		    else if (projcode == GCTP_SOM)
                    {
		       /* Store start & count along x & y */
		       /* ------------------------------- */
		       GDXRegion[i]->xStart = 0;
		       GDXRegion[i]->xCount = xdimsize;
		       GDXRegion[i]->yStart = 0;
		       GDXRegion[i]->yCount = ydimsize;

                       GDXRegion[i]->somStart = blockindexstart;
                       GDXRegion[i]->somCount = blockindexstop - blockindexstart + 1;

		       /* Store upleft and lowright points of subset region */
		       /* ------------------------------------------------- */
                       if (blockindexstart == 0)
                       {
                          GDXRegion[i]->upleftpt[0] = upleftpt[0];
                          GDXRegion[i]->upleftpt[1] = upleftpt[1];
                          GDXRegion[i]->lowrightpt[0] = lowrightpt[0];
                          GDXRegion[i]->lowrightpt[1] = lowrightpt[1];
                       }
                       else
                       {
                          GDXRegion[i]->upleftpt[0] = 
			    (lowrightpt[0] - upleftpt[0])*
			    (offset[blockindexstart-1]/xdimsize) + upleftpt[0];
                          GDXRegion[i]->upleftpt[1] = 
			    (lowrightpt[1] - upleftpt[1])*
			    (blockindexstart+1-1) + upleftpt[1];

                          GDXRegion[i]->lowrightpt[0] = 
			    (lowrightpt[0] - upleftpt[0])*
			    (offset[blockindexstart-1]/xdimsize) + lowrightpt[0];
                          GDXRegion[i]->lowrightpt[1] = 
			    (lowrightpt[1] - upleftpt[1])*
			    (blockindexstart+1-1) + lowrightpt[1];

                       }
                    }
                    else
		    {
			/* Non-GEO, Non-BCEA projections */
			/* ---------------------------- */

			/* Compute X & Y scale */
			/* ------------------- */
			xscale = (lowrightpt[0] - upleftpt[0]) / xdimsize;
			yscale = (lowrightpt[1] - upleftpt[1]) / ydimsize;


			/* MinCol -> meters -> upleftpt[0] */
			/* ------------------------------- */
			GDXRegion[i]->upleftpt[0] = upleftpt[0] +
			    xscale * minCol;


			/* MinRow -> meters -> upleftpt[1] */
			/* ------------------------------- */
			GDXRegion[i]->upleftpt[1] = upleftpt[1] +
			    yscale * minRow;


			/* MinCol + 1 -> meters -> lowrightpt[0] */
			/* ------------------------------------- */
			GDXRegion[i]->lowrightpt[0] = upleftpt[0] +
			    xscale * (maxCol + 1);


			/* MinRow + 1 -> meters -> lowrightpt[1] */
			/* ------------------------------------- */
			GDXRegion[i]->lowrightpt[1] = upleftpt[1] +
			  yscale * (maxRow + 1);
		    }

		    /* Store region ID */
		    /* --------------- */
		    regionID = i;
		    break;
		}
	    
	}
      }
	    
    }
    free(utlbuf);
    free(gridname);
    return (regionID);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDregioninfo                                                     |
|                                                                             |
|  DESCRIPTION: Retrieves size of region in bytes.                            |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               Grid structure ID                       |
|  regionID       int32               Region ID                               |
|  fieldname      char                Fieldname                               |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|  ntype          int32               field number type                       |
|  rank           int32               field rank                              |
|  dims           int32               dimensions of field region              |
|  size           int32               size in bytes of field region           |
|  upleftpt       float64             Upper left corner coord for region      |
|  lowrightpt     float64             Lower right corner coord for region     |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Aug 96   Joel Gales    Add vertical subsetting                             |
|  Dec 96   Joel Gales    Add multiple vertical subsetting capability         |
|  Apr 99   David Wynne   Added support for MISR SOM projection, NCR 21195    |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
GDregioninfo(int32 gridID, int32 regionID, const char *fieldname,
	     int32 * ntype, int32 * rank, int32 dims[], int32 * size,
	     float64 upleftpt[], float64 lowrightpt[])
{
    intn            j;		/* Loop index */
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           gdVgrpID;	/* Grid root Vgroup ID */
    int32           l_index;	/* Dimension l_index */

    char            dimlist[256];	/* Dimension list */
    const char           *errMesg = "Vertical Dimension Not Found: \"%s\".\n";
    const char           *errM1 = "Both \"XDim\" and \"YDim\" must be present ";
    const char           *errM2 = "in the dimension list for \"%s\".\n";
    char            errbuf[256];/* Error buffer */


    /* Check for valid grid ID */
    /* ----------------------- */
    status = GDchkgdid(gridID, "GDregioninfo", &fid, &sdInterfaceID,
		       &gdVgrpID);


    /* Check for valid region ID */
    /* ------------------------- */
    if (status == 0)
    {
	if (regionID < 0 || regionID >= NGRIDREGN)
	{
	    status = -1;
	    HEpush(DFE_RANGE, "GDregioninfo", __FILE__, __LINE__);
	    HEreport("Invalid Region id: %d.\n", regionID);
	}
    }


    /* Check for active region ID */
    /* -------------------------- */
    if (status == 0)
    {
	if (GDXRegion[regionID] == 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "GDregioninfo", __FILE__, __LINE__);
	    HEreport("Inactive Region ID: %d.\n", regionID);
	}
    }



    /* Check that region defined for this file */
    /* --------------------------------------- */
    if (status == 0)
    {
	if (GDXRegion[regionID]->fid != fid)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "GDregioninfo", __FILE__, __LINE__);
	    HEreport("Region is not defined for this file.\n");
	}
    }


    /* Check that region defined for this grid */
    /* --------------------------------------- */
    if (status == 0)
    {
	if (GDXRegion[regionID]->gridID != gridID)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "GDregioninfo", __FILE__, __LINE__);
	    HEreport("Region is not defined for this Grid.\n");
	}
    }



    /* Check for valid fieldname */
    /* ------------------------- */
    if (status == 0)
    {
	status = GDfieldinfo(gridID, fieldname, rank, dims, ntype, dimlist);

	if (status != 0)
	{
	    /* Fieldname not found in grid */
	    /* --------------------------- */
	    status = -1;
	    HEpush(DFE_GENAPP, "GDregioninfo", __FILE__, __LINE__);
	    HEreport("Fieldname \"%s\" not found.\n",
		     fieldname);
	}
	else if (*rank == 1)
	{
	    /* Field is 1 dimensional */
	    /* ---------------------- */
	    status = -1;
	    HEpush(DFE_GENAPP, "GDregioninfo", __FILE__, __LINE__);
	    HEreport(
		     "One-Dimesional fields \"%s\" may not be subsetted.\n",
		     fieldname);
	}
	else
	{
	    /* "XDim" and/or "YDim" not found */
	    /* ------------------------------ */
	    if (EHstrwithin("XDim", dimlist, ',') == -1 ||
		EHstrwithin("YDim", dimlist, ',') == -1)
	    {
		status = -1;
		HEpush(DFE_GENAPP, "GDregioninfo", __FILE__, __LINE__);
		snprintf(errbuf, sizeof(errbuf), "%s%s", errM1, errM2);
		HEreport(errbuf, fieldname);
	    }
	}
    }



    /* If no problems ... */
    /* ------------------ */
    if (status == 0)
    {
        /* Check if SOM projection */
        /* ----------------------- */
        if (EHstrwithin("SOMBlockDim", dimlist, ',') == 0)
        {
            dims[EHstrwithin("SOMBlockDim", dimlist, ',')] =
                GDXRegion[regionID]->somCount;
        }

	/* Load XDim dimension from region entry */
	/* ------------------------------------- */
	if (GDXRegion[regionID]->xCount != 0)
	{
	    dims[EHstrwithin("XDim", dimlist, ',')] =
		GDXRegion[regionID]->xCount;
	}

	/* Load YDim dimension from region entry */
	/* ------------------------------------- */
	if (GDXRegion[regionID]->yCount != 0)
	{
	    dims[EHstrwithin("YDim", dimlist, ',')] =
		GDXRegion[regionID]->yCount;
	}


	/* Vertical Subset */
	/* --------------- */
	for (j = 0; j < 8; j++)
	{

	    /* If active vertical subset ... */
	    /* ----------------------------- */
	    if (GDXRegion[regionID]->StartVertical[j] != -1)
	    {
		/* Find vertical dimension within dimlist */
		/* -------------------------------------- */
		l_index = EHstrwithin(GDXRegion[regionID]->DimNamePtr[j],
				    dimlist, ',');

		/* If dimension found ... */
		/* ---------------------- */
		if (l_index != -1)
		{
		    /* Compute dimension size */
		    /* ---------------------- */
		    dims[l_index] =
			GDXRegion[regionID]->StopVertical[j] -
			GDXRegion[regionID]->StartVertical[j] + 1;
		}
		else
		{
		    /* Vertical dimension not found */
		    /* ---------------------------- */
		    status = -1;
		    *size = -1;
		    HEpush(DFE_GENAPP, "GDregioninfo",
			   __FILE__, __LINE__);
		    HEreport(errMesg,
			     GDXRegion[regionID]->DimNamePtr[j]);
		}
	    }
	}


	if (status == 0)
	{
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


	    /* Return upper left and lower right subset values */
	    /* ----------------------------------------------- */
	    upleftpt[0] = GDXRegion[regionID]->upleftpt[0];
	    upleftpt[1] = GDXRegion[regionID]->upleftpt[1];
	    lowrightpt[0] = GDXRegion[regionID]->lowrightpt[0];
	    lowrightpt[1] = GDXRegion[regionID]->lowrightpt[1];
	}
    }
    return (status);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDextractregion                                                  |
|                                                                             |
|  DESCRIPTION: Retrieves data from specified region.                         |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               Grid structure ID                       |
|  regionID       int32               Region ID                               |
|  fieldname      char                Fieldname                               |
|                                                                             |
|  OUTPUTS:                                                                   |
|  buffer         void                Data buffer containing subsetted region |
|                                                                             |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Aug 96   Joel Gales    Add vertical subsetting                             |
|  Dec 96   Joel Gales    Add multiple vertical subsetting capability         |
|  Apr 99   David Wynne   Add support for MISR SOM projection, NCR 21195      |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
GDextractregion(int32 gridID, int32 regionID, const char *fieldname,
		VOIDP buffer)
{
    intn            i;		/* Loop index */
    intn            j;		/* Loop index */
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           gdVgrpID;	/* Grid root Vgroup ID */
    int32           l_index;	/* Dimension l_index */
    int32           start[8];	/* Start array for data read */
    int32           edge[8];	/* Edge array for data read */
    int32           dims[8];	/* Dimensions */
    int32           rank = 0;	/* Field rank */
    int32           ntype;	/* Field number type */
    int32           origincode;	/* Pixel origin code */

    char            dimlist[256];	/* Dimension list */
    const char           *errMesg = "Vertical Dimension Not Found: \"%s\".\n";
    const char           *errM1 = "Both \"XDim\" and \"YDim\" must be present ";
    const char           *errM2 = "in the dimension list for \"%s\".\n";
    char            errbuf[256];/* Error buffer */


    /* Check for valid grid ID */
    /* ----------------------- */
    status = GDchkgdid(gridID, "GDextractregion", &fid, &sdInterfaceID,
		       &gdVgrpID);


    /* Check for valid region ID */
    /* ------------------------- */
    if (status == 0)
    {
	if (regionID < 0 || regionID >= NGRIDREGN)
	{
	    status = -1;
	    HEpush(DFE_RANGE, "GDextractregion", __FILE__, __LINE__);
	    HEreport("Invalid Region id: %d.\n", regionID);
	}
    }


    /* Check for active region ID */
    /* -------------------------- */
    if (status == 0)
    {
	if (GDXRegion[regionID] == 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "GDextractregion", __FILE__, __LINE__);
	    HEreport("Inactive Region ID: %d.\n", regionID);
	}
    }



    /* Check that region defined for this file */
    /* --------------------------------------- */
    if (status == 0)
    {
	if (GDXRegion[regionID]->fid != fid)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "GDextractregion", __FILE__, __LINE__);
	    HEreport("Region is not defined for this file.\n");
	}
    }


    /* Check that region defined for this grid */
    /* --------------------------------------- */
    if (status == 0)
    {
	if (GDXRegion[regionID]->gridID != gridID)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "GDextractregion", __FILE__, __LINE__);
	    HEreport("Region is not defined for this Grid.\n");
	}
    }



    /* Check for valid fieldname */
    /* ------------------------- */
    if (status == 0)
    {
	status = GDfieldinfo(gridID, fieldname, &rank, dims, &ntype, dimlist);

	if (status != 0)
	{
	    /* Fieldname not found in grid */
	    /* --------------------------- */
	    status = -1;
	    HEpush(DFE_GENAPP, "GDextractregion", __FILE__, __LINE__);
	    HEreport("Fieldname \"%s\" not found.\n",
		     fieldname);
	}
	else if (rank == 1)
	{
	    /* Field is 1 dimensional */
	    /* ---------------------- */
	    status = -1;
	    HEpush(DFE_GENAPP, "GDextractregion", __FILE__, __LINE__);
	    HEreport(
		     "One-Dimesional fields \"%s\" may not be subsetted.\n",
		     fieldname);
	}
	else
	{
	    /* "XDim" and/or "YDim" not found */
	    /* ------------------------------ */
	    if (EHstrwithin("XDim", dimlist, ',') == -1 ||
		EHstrwithin("YDim", dimlist, ',') == -1)
	    {
		status = -1;
		HEpush(DFE_GENAPP, "GDextractregion", __FILE__, __LINE__);
		snprintf(errbuf, sizeof(errbuf), "%s%s", errM1, errM2);
		HEreport(errbuf, fieldname);
	    }
	}
    }



    if (status == 0)
    {

	/* Get origin order info */
	/* --------------------- */
	status = GDorigininfo(gridID, &origincode);


	/* Initialize start & edge arrays */
	/* ------------------------------ */
	for (i = 0; i < rank; i++)
	{
	    start[i] = 0;
	    edge[i] = dims[i];
	}


	/* if MISR SOM projection, set start */
	/* & edge arrays for SOMBlockDim     */
	/* --------------------------------- */
	if (EHstrwithin("SOMBlockDim", dimlist, ',') == 0)
	{
	    l_index = EHstrwithin("SOMBlockDim", dimlist, ',');
	    edge[l_index] = GDXRegion[regionID]->somCount;
	    start[l_index] = GDXRegion[regionID]->somStart;
	}


	/* Set start & edge arrays for XDim */
	/* -------------------------------- */
	l_index = EHstrwithin("XDim", dimlist, ',');
	if (GDXRegion[regionID]->xCount != 0)
	{
	    edge[l_index] = GDXRegion[regionID]->xCount;
	    start[l_index] = GDXRegion[regionID]->xStart;
	}

	/* Adjust X-dim start if origin on right edge */
	/* ------------------------------------------ */
	if ((origincode & 1) == 1)
	{
	    start[l_index] = dims[l_index] - (start[l_index] + edge[l_index]);
	}


	/* Set start & edge arrays for YDim */
	/* -------------------------------- */
	l_index = EHstrwithin("YDim", dimlist, ',');
	if (GDXRegion[regionID]->yCount != 0)
	{
	    start[l_index] = GDXRegion[regionID]->yStart;
	    edge[l_index] = GDXRegion[regionID]->yCount;
	}

	/* Adjust Y-dim start if origin on lower edge */
	/* ------------------------------------------ */
	if ((origincode & 2) == 2)
	{
	    start[l_index] = dims[l_index] - (start[l_index] + edge[l_index]);
	}



	/* Vertical Subset */
	/* --------------- */
	for (j = 0; j < 8; j++)
	{
	    /* If active vertical subset ... */
	    /* ----------------------------- */
	    if (GDXRegion[regionID]->StartVertical[j] != -1)
	    {

		/* Find vertical dimension within dimlist */
		/* -------------------------------------- */
		l_index = EHstrwithin(GDXRegion[regionID]->DimNamePtr[j],
				    dimlist, ',');

		/* If dimension found ... */
		/* ---------------------- */
		if (l_index != -1)
		{
		    /* Compute start and edge for vertical dimension */
		    /* --------------------------------------------- */
		    start[l_index] = GDXRegion[regionID]->StartVertical[j];
		    edge[l_index] = GDXRegion[regionID]->StopVertical[j] -
			GDXRegion[regionID]->StartVertical[j] + 1;
		}
		else
		{
		    /* Vertical dimension not found */
		    /* ---------------------------- */
		    status = -1;
		    HEpush(DFE_GENAPP, "GDextractregion", __FILE__, __LINE__);
		    HEreport(errMesg,
			     GDXRegion[regionID]->DimNamePtr[j]);
		}
	    }
	}


	/* Read into data buffer */
	/* --------------------- */
	if (status == 0)
	{
	    status = GDreadfield(gridID, fieldname, start, NULL, edge, buffer);
	}
    }
    return (status);
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDdupregion                                                      |
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
|  Oct 98   Abe Taaheri   changed *GDXRegion[i] = *GDXRegion[oldregionID];    |
|                         to copy elements of structure one by one to avoid   |
|                         copying pointer for DimNamePtr to another place that|
|                         causes "Freeing Unallocated Memory" in purify when  |
|                         using GDdetach                                      |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
GDdupregion(int32 oldregionID)
{
    intn            i;		/* Loop index */
    intn            j;          /* Loop index */
    int32           slendupregion;
    int32           newregionID = -1;	/* New region ID */


    /* Find first empty (inactive) region */
    /* ---------------------------------- */
    for (i = 0; i < NGRIDREGN; i++)
    {
	if (GDXRegion[i] == 0)
	{
	    /* Allocate space for new grid region entry */
	    /* ---------------------------------------- */
	    GDXRegion[i] = (struct gridRegion *)
		calloc(1, sizeof(struct gridRegion));
	    if(GDXRegion[i] == NULL)
	    { 
		HEpush(DFE_NOSPACE,"GDdupregion", __FILE__, __LINE__);
		return(-1);
	    }


	    /* Copy old region structure data to new region */
	    /* -------------------------------------------- */
	                
            GDXRegion[i]->fid = GDXRegion[oldregionID]->fid;
            GDXRegion[i]->gridID = GDXRegion[oldregionID]->gridID;
            GDXRegion[i]->xStart = GDXRegion[oldregionID]->xStart;
            GDXRegion[i]->xCount = GDXRegion[oldregionID]->xCount;
            GDXRegion[i]->yStart = GDXRegion[oldregionID]->yStart;
            GDXRegion[i]->yCount = GDXRegion[oldregionID]->yCount;
            GDXRegion[i]->upleftpt[0] = GDXRegion[oldregionID]->upleftpt[0];
            GDXRegion[i]->upleftpt[1] = GDXRegion[oldregionID]->upleftpt[1];
            GDXRegion[i]->lowrightpt[0] = GDXRegion[oldregionID]->lowrightpt[0];
            GDXRegion[i]->lowrightpt[1] = GDXRegion[oldregionID]->lowrightpt[1];
            for (j = 0; j < 8; j++)
            {
                GDXRegion[i]->StartVertical[j] = GDXRegion[oldregionID]->StartVertical[j];
                GDXRegion[i]->StopVertical[j] = GDXRegion[oldregionID]->StopVertical[j];
            }
	    
            for (j=0; j<8; j++)
            {
                if(GDXRegion[oldregionID]->DimNamePtr[j] != NULL)
                {
                    slendupregion = (int)strlen(GDXRegion[oldregionID]->DimNamePtr[j]);
                    GDXRegion[i]->DimNamePtr[j] = (char *) malloc(slendupregion + 1);
                    strcpy(GDXRegion[i]->DimNamePtr[j],GDXRegion[oldregionID]->DimNamePtr[j]);
		}
            }
            

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
|  FUNCTION: GDdefvrtregion                                                   |
|                                                                             |
|  DESCRIPTION: Finds elements of a monotonic field within a vertical subset  |
|               region.                                                       |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  regionID       int32               Region ID                               |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               Grid structure ID                       |
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
|  Feb 97   Joel Gales    Store XDim, YDim, upleftpt, lowrightpt in GDXRegion |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
#define SETGRIDREG \
\
status = GDgridinfo(gridID, &xdimsize, &ydimsize, upleftpt, lowrightpt); \
for (k = 0; k < NGRIDREGN; k++) \
{ \
    if (GDXRegion[k] == 0) \
    { \
        GDXRegion[k] = (struct gridRegion *) \
	  calloc(1, sizeof(struct gridRegion)); \
	GDXRegion[k]->fid = fid; \
	GDXRegion[k]->gridID = gridID; \
	GDXRegion[k]->xStart = 0; \
	GDXRegion[k]->xCount = xdimsize; \
	GDXRegion[k]->yStart = 0; \
	GDXRegion[k]->yCount = ydimsize; \
	GDXRegion[k]->upleftpt[0] = upleftpt[0]; \
	GDXRegion[k]->upleftpt[1] = upleftpt[1]; \
	GDXRegion[k]->lowrightpt[0] = lowrightpt[0]; \
	GDXRegion[k]->lowrightpt[1] = lowrightpt[1]; \
	regionID = k; \
	for (j=0; j<8; j++) \
        { \
             GDXRegion[k]->StartVertical[j] = -1; \
             GDXRegion[k]->StopVertical[j]  = -1; \
        } \
	break; \
     } \
}

#define FILLVERTREG \
for (j=0; j<8; j++) \
{ \
    if (GDXRegion[regionID]->StartVertical[j] == -1) \
    { \
	GDXRegion[regionID]->StartVertical[j] = i; \
	GDXRegion[regionID]->DimNamePtr[j] = \
	    (char *) malloc(slen + 1); \
	memcpy(GDXRegion[regionID]->DimNamePtr[j], \
	       dimlist, slen + 1); \
	break; \
    } \
} \



int32
GDdefvrtregion(int32 gridID, int32 regionID, const char *vertObj, float64 range[])
{
    intn            i, j = 0, k, status;
    uint8           found = 0;

    int16           vertINT16;

    int32           fid, sdInterfaceID, slen;
    int32           gdVgrpID, rank, nt, dims[8], size;
    int32           vertINT32;
    int32           xdimsize;
    int32           ydimsize;

    float32         vertFLT32;
    float64         vertFLT64;
    float64         upleftpt[2];
    float64         lowrightpt[2];

    char           *vertArr;
    char           *dimlist;

    /* Allocate space for dimlist */
    /* --------------------------------- */
    dimlist = (char *) calloc(UTLSTR_MAX_SIZE, sizeof(char));
    if(dimlist == NULL)
    { 
	HEpush(DFE_NOSPACE,"GDdefvrtregion", __FILE__, __LINE__);
	return(-1);
    }
    /* Check for valid grid ID */
    /* ----------------------- */
    status = GDchkgdid(gridID, "GDdefvrtregion",
		       &fid, &sdInterfaceID, &gdVgrpID);

    if (status == 0)
    {
	memcpy(dimlist, vertObj, 4);
	dimlist[4] = 0;

	if (strcmp(dimlist, "DIM:") == 0)
	{
	    slen = (int)strlen(vertObj) - 4;
	    if (regionID == -1)
	    {
		SETGRIDREG;
	    }
	    for (j = 0; j < 8; j++)
	    {
		if (GDXRegion[regionID]->StartVertical[j] == -1)
		{
		    GDXRegion[regionID]->StartVertical[j] = (int32) range[0];
		    GDXRegion[regionID]->StopVertical[j] = (int32) range[1];
		    GDXRegion[regionID]->DimNamePtr[j] =
			(char *) malloc(slen + 1);
		    if(GDXRegion[regionID]->DimNamePtr[j] == NULL)
		    { 
			HEpush(DFE_NOSPACE,"GDdefvrtregion", __FILE__, __LINE__);
			free(dimlist);
			return(-1);
		    }
		    memcpy(GDXRegion[regionID]->DimNamePtr[j],
			   vertObj + 4, slen + 1);
		    break;
		}
	    }
	}
	else
	{
	    status = GDfieldinfo(gridID, vertObj, &rank, dims, &nt, dimlist);
	    if (status != 0)
	    {
		status = -1;
		HEpush(DFE_GENAPP, "GDdefvrtregion", __FILE__, __LINE__);
		HEreport("Vertical Field: \"%s\" not found.\n", vertObj);
	    }
	    else
	    {
		if (rank != 1)
		{
		    status = -1;
		    HEpush(DFE_GENAPP, "GDdefvrtregion", __FILE__, __LINE__);
		    HEreport("Vertical Field: \"%s\" must be 1-dim.\n",
			     vertObj);
		}
		else
		{
		    slen = (int)strlen(dimlist);
		    size = DFKNTsize(nt);
		    vertArr = (char *) calloc(dims[0], size);
		    if(vertArr == NULL)
		    { 
			HEpush(DFE_NOSPACE,"GDdefvrtregion", __FILE__, __LINE__);
			free(dimlist);
			return(-1);
		    }

		    status = GDreadfield(gridID, vertObj,
					 NULL, NULL, NULL, vertArr);

		    switch (nt)
		    {
		    case DFNT_INT16:

			for (i = 0; i < dims[0]; i++)
			{
			    memcpy(&vertINT16, vertArr + i * size, size);

			    if (vertINT16 >= range[0] &&
				vertINT16 <= range[1])
			    {
				found = 1;
				if (regionID == -1)
				{
				    SETGRIDREG;
				}
				FILLVERTREG;

				break;
			    }
			}

			if (found == 1)
			{
			    for (i = dims[0] - 1; i >= 0; i--)
			    {
				memcpy(&vertINT16, vertArr + i * size, size);

				if (vertINT16 >= range[0] &&
				    vertINT16 <= range[1])
				{
				    GDXRegion[regionID]->StopVertical[j] = i;
				    break;
				}
			    }
			}
			else
			{
			    status = -1;
			}
			break;


		    case DFNT_INT32:

			for (i = 0; i < dims[0]; i++)
			{
			    memcpy(&vertINT32, vertArr + i * size, size);

			    if (vertINT32 >= range[0] &&
				vertINT32 <= range[1])
			    {
				found = 1;
				if (regionID == -1)
				{
				    SETGRIDREG;
				}
				FILLVERTREG;

				break;
			    }
			}

			if (found == 1)
			{
			    for (i = dims[0] - 1; i >= 0; i--)
			    {
				memcpy(&vertINT32, vertArr + i * size, size);

				if (vertINT32 >= range[0] &&
				    vertINT32 <= range[1])
				{
				    GDXRegion[regionID]->StopVertical[j] = i;
				    break;
				}
			    }
			}
			else
			{
			    status = -1;
			}
			break;


		    case DFNT_FLOAT32:

			for (i = 0; i < dims[0]; i++)
			{
			    memcpy(&vertFLT32, vertArr + i * size, size);

			    if (vertFLT32 >= range[0] &&
				vertFLT32 <= range[1])
			    {
				found = 1;
				if (regionID == -1)
				{
				    SETGRIDREG;
				}
				FILLVERTREG;

				break;
			    }
			}

			if (found == 1)
			{
			    for (i = dims[0] - 1; i >= 0; i--)
			    {
				memcpy(&vertFLT32, vertArr + i * size, size);

				if (vertFLT32 >= range[0] &&
				    vertFLT32 <= range[1])
				{
				    GDXRegion[regionID]->StopVertical[j] = i;
				    break;
				}
			    }
			}
			else
			{
			    status = -1;
			}
			break;


		    case DFNT_FLOAT64:

			for (i = 0; i < dims[0]; i++)
			{
			    memcpy(&vertFLT64, vertArr + i * size, size);

			    if (vertFLT64 >= range[0] &&
				vertFLT64 <= range[1])
			    {
				found = 1;
				if (regionID == -1)
				{
				    SETGRIDREG;
				}
				FILLVERTREG;

				break;
			    }
			}

			if (found == 1)
			{
			    for (i = dims[0] - 1; i >= 0; i--)
			    {
				memcpy(&vertFLT64, vertArr + i * size, size);

				if (vertFLT64 >= range[0] &&
				    vertFLT64 <= range[1])
				{
				    GDXRegion[regionID]->StopVertical[j] = i;
				    break;
				}
			    }
			}
			else
			{
			    status = -1;
			}
			break;

		    }
		    free(vertArr);
		}
	    }
	}
    }
    if (status == -1)
    {
	regionID = -1;
    }
    free(dimlist);
    return (regionID);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDdeftimeperiod                                                  |
|                                                                             |
|  DESCRIPTION: Finds elements of the "Time" field within a given time        |
|               period.                                                       |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  periodID       int32               Period ID                               |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               Grid structure ID                       |
|  periodID       int32               Period ID                               |
|  starttime      float64 TAI sec     Start of time period                    |
|  stoptime       float64 TAI sec     Stop of time period                     |
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
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
GDdeftimeperiod(int32 gridID, int32 periodID, float64 starttime,
		float64 stoptime)
{
    float64         timerange[2];

    timerange[0] = starttime;
    timerange[1] = stoptime;

    periodID = GDdefvrtregion(gridID, periodID, "Time", timerange);

    return (periodID);
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDgetpixels                                                      |
|                                                                             |
|  DESCRIPTION: Finds row and columns for specified lon/lat values            |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               Grid structure ID                       |
|  nLonLat        int32               Number of lonlat values                 |
|  lonVal         float64  dec deg    Longitude values                        |
|  latVal         float64  dec deg    Latitude values                         |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|  pixRow         int32               Pixel rows                              |
|  pixCol         int32               Pixel columns                           |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Aug 96   Joel Gales    Original Programmer                                 |
|  Oct 96   Joel Gales    Set row/col to -1 if outside boundary               |
|  Mar 97   Joel Gales    Adjust row/col for CORNER pixel registration        |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
GDgetpixels(int32 gridID, int32 nLonLat, float64 lonVal[], float64 latVal[],
	    int32 pixRow[], int32 pixCol[])
{
    intn            i;		/* Loop index */
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           gdVgrpID;	/* Grid root Vgroup ID */

    int32           xdimsize;	/* Size of "XDim" */
    int32           ydimsize;	/* Size of "YDim" */
    int32           projcode;	/* GCTP projection code */
    int32           zonecode;	/* Zone code */
    int32           spherecode;	/* Sphere code */
    int32           origincode;	/* Origin code */
    int32           pixregcode;	/* Pixel registration code */

    float64         upleftpt[2];/* Upper left point */
    float64         lowrightpt[2];	/* Lower right point */
    float64         projparm[16];	/* Projection parameters */
    float64        *xVal;	/* Pointer to point x location values */
    float64        *yVal;	/* Pointer to point y location values */


    /* Check for valid grid ID */
    /* ----------------------- */
    status = GDchkgdid(gridID, "GDgetpixels", &fid, &sdInterfaceID, &gdVgrpID);

    if (status == 0)
    {
	/* Get grid info */
	/* ------------- */
	status = GDgridinfo(gridID, &xdimsize, &ydimsize,
			    upleftpt, lowrightpt);


	/* Get projection info */
	/* ------------------- */
	status = GDprojinfo(gridID, &projcode, &zonecode,
			    &spherecode, projparm);


	/* Get explicit upleftpt & lowrightpt if defaults are used */
	/* ------------------------------------------------------- */
	status = GDgetdefaults(projcode, zonecode, projparm, spherecode,
			       upleftpt, lowrightpt);


	/* Get pixel registration and origin info */
	/* -------------------------------------- */
	status = GDorigininfo(gridID, &origincode);
	status = GDpixreginfo(gridID, &pixregcode);


	/* Allocate space for x & y locations */
	/* ---------------------------------- */
	xVal = (float64 *) calloc(nLonLat, sizeof(float64));
	if(xVal == NULL)
	{ 
	    HEpush(DFE_NOSPACE,"GDgetpixels", __FILE__, __LINE__);
	    return(-1);
	}
	yVal = (float64 *) calloc(nLonLat, sizeof(float64));
	if(yVal == NULL)
	{ 
	    HEpush(DFE_NOSPACE,"GDgetpixels", __FILE__, __LINE__);
	    free(xVal);
	    return(-1);
	}


	/* Get pixRow, pixCol, xVal, & yVal */
	/* -------------------------------- */
	status = GDll2ij(projcode, zonecode, projparm, spherecode,
			 xdimsize, ydimsize, upleftpt, lowrightpt,
			 nLonLat, lonVal, latVal, pixRow, pixCol,
			 xVal, yVal);



	/* Loop through all lon/lat values */
	/* ------------------------------- */
	for (i = 0; i < nLonLat; i++)
	{
	    /* Adjust columns & rows for "corner" registered grids */
	    /* --------------------------------------------------- */
	    if (pixregcode == HDFE_CORNER)
	    {
		if (origincode == HDFE_GD_UL)
		{
		    if (xVal[i] - pixCol[i] > 0.5)
		    {
			++pixCol[i];
		    }

		    if (yVal[i] - pixRow[i] > 0.5)
		    {
			++pixRow[i];
		    }
		}
		else if (origincode == HDFE_GD_UR)
		{
		    if (xVal[i] - pixCol[i] <= 0.5)
		    {
			--pixCol[i];
		    }

		    if (yVal[i] - pixRow[i] > 0.5)
		    {
			++pixRow[i];
		    }
		}
		else if (origincode == HDFE_GD_LL)
		{
		    if (xVal[i] - pixCol[i] > 0.5)
		    {
			++pixCol[i];
		    }

		    if (yVal[i] - pixRow[i] <= 0.5)
		    {
			--pixRow[i];
		    }
		}
		else if (origincode == HDFE_GD_LR)
		{
		    if (xVal[i] - pixCol[i] <= 0.5)
		    {
			--pixCol[i];
		    }

		    if (yVal[i] - pixRow[i] <= 0.5)
		    {
			--pixRow[i];
		    }
		}
	    }


	    /* If outside grid boundaries then set to -1 */
	    /* ----------------------------------------- */
	    if (pixCol[i] < 0 || pixCol[i] >= xdimsize ||
		pixRow[i] < 0 || pixRow[i] >= ydimsize)
	    {
		pixCol[i] = -1;
		pixRow[i] = -1;
	    }
	}
	free(xVal);
	free(yVal);
    }
    return (status);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDgetpixvalues                                                   |
|                                                                             |
|  DESCRIPTION: Retrieves data from specified pixels.                         |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  size*nPixels   int32               Size of data buffer                     |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               Grid structure ID                       |
|  nPixels        int32               Number of pixels                        |
|  pixRow         int32               Pixel row numbers                       |
|  pixCol         int32               Pixel column numbers                    |
|  fieldname      char                Fieldname                               |
|                                                                             |
|  OUTPUTS:                                                                   |
|  buffer         void                Data buffer                             |
|                                                                             |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Aug 96   Joel Gales    Original Programmer                                 |
|  Oct 96   Joel Gales    Check for pixels outside boundaries (-1)            |
|  Mar 98   Abe Taaheri   revised to reduce overhead for rechecking           |
|                         for gridid, fieldname, etc in GDreadfield.          |
|  June 98  AT            fixed bug with 2-dim field merged in 3-dim field    |
|                         (for offset and count)                              |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
GDgetpixvalues(int32 gridID, int32 nPixels, int32 pixRow[], int32 pixCol[],
	       const char *fieldname, VOIDP buffer)
{
    intn            i;		/* Loop index */
    intn            j;		/* Loop index */
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           gdVgrpID;	/* Grid root Vgroup ID */

    int32           start[8];	/* GDreadfield start array */
    int32           edge[8];	/* GDreadfield edge array */
    int32           dims[8];	/* Field dimensions */
    int32           rank;	/* Field rank */
    int32           xdum = 0;	/* Location of "XDim" within field list */
    int32           ydum = 0;	/* Location of "YDim" within field list */
    int32           ntype;	/* Field number type */
    int32           origincode;	/* Origin code */
    int32           bufOffset;	/* Data buffer offset */
    int32           size = 0;	/* Size of returned data buffer for each
				 * value in bytes */
    int32           offset[8];	/* I/O offset (start) */
    int32           incr[8];	/* I/O increment (stride) */
    int32           count[8];	/* I/O count (edge) */
    int32           sdid;	/* SDS ID */
    int32           rankSDS;	/* Rank of SDS */
    int32           rankFld;	/* Rank of field */
    int32           dum;	/* Dummy variable */
    int32           mrgOffset;	/* Merged field offset */

    char           *dimlist;	/* Dimension list */



    /* Allocate space for dimlist */
    /* --------------------------------- */
    dimlist = (char *) calloc(UTLSTR_MAX_SIZE, sizeof(char));
    if(dimlist == NULL)
    { 
	HEpush(DFE_NOSPACE,"GDgetpixvalues", __FILE__, __LINE__);
	return(-1);
    }
    /* Check for valid grid ID */
    /* ----------------------- */
    status = GDchkgdid(gridID, "GDgetpixvalues",
		       &fid, &sdInterfaceID, &gdVgrpID);


    if (status == 0)
    {
	/* Get field list */
	/* -------------- */
	status = GDfieldinfo(gridID, fieldname, &rank, dims, &ntype, dimlist);


	/* Check for "XDim" & "YDim" in dimension list */
	/* ------------------------------------------- */
	if (status == 0)
	{
	    xdum = EHstrwithin("XDim", dimlist, ',');
	    ydum = EHstrwithin("YDim", dimlist, ',');

	    if (xdum == -1)
	    {
		status = -1;
		HEpush(DFE_GENAPP, "GDgetpixvalues", __FILE__, __LINE__);
		HEreport(
		     "\"XDim\" not present in dimlist for field: \"%s\".\n",
			 fieldname);
	    }


	    if (ydum == -1)
	    {
		status = -1;
		HEpush(DFE_GENAPP, "GDgetpixvalues", __FILE__, __LINE__);
		HEreport(
		     "\"YDim\" not present in dimlist for field: \"%s\".\n",
			 fieldname);
	    }
	}
	else
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "GDgetpixvalues", __FILE__, __LINE__);
	    HEreport("Fieldname \"%s\" not found.\n", fieldname);
	}


	if (status == 0)
	{

	    /* Get origin order info */
	    /* --------------------- */
	    status = GDorigininfo(gridID, &origincode);


	    /* Initialize start & edge arrays */
	    /* ------------------------------ */
	    for (i = 0; i < rank; i++)
	    {
		start[i] = 0;
		edge[i] = dims[i];
	    }


	    /* Compute size of data buffer for each pixel */
	    /* ------------------------------------------ */
	    edge[xdum] = 1;
	    edge[ydum] = 1;
	    size = edge[0];
	    for (j = 1; j < rank; j++)
	    {
		size *= edge[j];
	    }
	    size *= DFKNTsize(ntype);



	    /* If data values are requested ... */
	    /* -------------------------------- */
	    if (buffer != NULL)
	    {
		/* get sdid */
		status = GDSDfldsrch(gridID, sdInterfaceID, fieldname, &sdid,
				  &rankSDS, &rankFld, &mrgOffset, dims, &dum);

		/* Loop through all pixels */
		/* ----------------------- */
		for (i = 0; i < nPixels; i++)
		{
		    /* Conmpute offset within returned data buffer */
		    /* ------------------------------------------- */
		    bufOffset = size * i;


		    /* If pixel row & column OK ... */
		    /* ---------------------------- */
		    if (pixCol[i] != -1 && pixRow[i] != -1)
		    {
			start[xdum] = pixCol[i];
			start[ydum] = pixRow[i];


			/* Adjust X-dim start if origin on right edge */
			/* ------------------------------------------ */
			if ((origincode & 1) == 1)
			{
			    start[xdum] = dims[xdum] - (start[xdum] + 1);
			}


			/* Adjust Y-dim start if origin on lower edge */
			/* ------------------------------------------ */
			if ((origincode & 2) == 2)
			{
			    start[ydum] = dims[ydum] - (start[ydum] + 1);
			}

			/* Set I/O offset and count Section */
			/* ---------------------- */
			
			/*
			 * start and edge != NULL, set I/O offset and count to 
			 * user values, adjusting the
			 * 0th field with the merged field offset (if any)
			 */
			if (rankFld == rankSDS)
			{
			    for (j = 0; j < rankSDS; j++)
			    {
				offset[j] = start[j];
				count[j] = edge[j];
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
			    for (j = 0; j < rankFld; j++)
			    {
				offset[j + 1] = start[j];
				count[j + 1] = edge[j];
			    }
			    offset[0] = mrgOffset;
			    count[0] = 1;
			}
			
			
			
			/* Set I/O stride Section */
			/* ---------------------- */
			
			/* In original code stride entered as NULL.
			   Abe Taaheri June 12, 1998 */
			/*
			 * If stride == NULL (default) set I/O stride to 1
			 */
			for (j = 0; j < rankSDS; j++)
			{
			    incr[j] = 1;
			}
			

			/* Read into data buffer */
			/* --------------------- */
			status = SDreaddata(sdid,
					    offset, incr, count,
				 (VOIDP) ((uint8 *) buffer + bufOffset));
		    }
		}
	    }
	}
    }


    /* If successful return size of returned data in bytes */
    /* --------------------------------------------------- */
    if (status == 0)
    {
	free(dimlist);
	return (size * nPixels);
    }
    else
    {
	free(dimlist);
	return ((int32) status);
    }
}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDinterpolate                                                    |
|                                                                             |
|  DESCRIPTION: Performs bilinear interpolate on a set of xy values           |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  nRetn*nValues*  int32               Size of data buffer                    |
|  sizeof(float64)                                                            |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               Grid structure ID                       |
|  nValues        int32               Number of lon/lat points to interpolate |
|  xyValues       float64             XY values of points to interpolate      |
|  fieldname      char                Fieldname                               |
|                                                                             |
|  OUTPUTS:                                                                   |
|  interpVal      float64             Interpolated Data Values                |
|                                                                             |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Aug 96   Joel Gales    Original Programmer                                 |
|  Oct 96   Joel Gales    Fix array l_index problem with interpVal write        |
|  Apr 97   Joel Gales    Trap interpolation boundary out of bounds error     |
|  Jun 98   Abe Taaheri   changed the return value so that the Return Value   |
|                         is size in bytes for the data buffer which is       |
|                         float64.
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
GDinterpolate(int32 gridID, int32 nValues, float64 lonVal[], float64 latVal[],
	      const char *fieldname, float64 interpVal[])
{
    intn            i;		/* Loop index */
    intn            j;		/* Loop index */
    intn            k;		/* Loop index */
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           gdVgrpID;	/* Grid root Vgroup ID */
    int32           xdimsize;	/* XDim size */
    int32           ydimsize;	/* YDim size */
    int32           projcode;	/* Projection code */
    int32           zonecode;	/* Zone code */
    int32           spherecode;	/* Sphere code */
    int32           pixregcode;	/* Pixel registration code */
    int32           origincode;	/* Origin code */
    int32           dims[8];	/* Field dimensions */
    int32           numsize;	/* Size in bytes of number type */
    int32           rank;	/* Field rank */
    int32           xdum = 0;	/* Location of "XDim" within field list */
    int32           ydum = 0;	/* Location of "YDim" within field list */
    int32           ntype;	/* Number type */
    int32           dum;	/* Dummy variable */
    int32           size;	/* Size of returned data buffer for each
				 * value in bytes */
    int32           pixCol[4];	/* Pixel columns for 4 nearest neighbors */
    int32           pixRow[4];	/* Pixel rows for 4 nearest neighbors */
    int32           tDen;	/* Interpolation denominator value 1 */
    int32           uDen;	/* Interpolation denominator value 2 */
    int32           nRetn = 0;	/* Number of data values returned */

    float64         upleftpt[2];/* Upper left pt coordinates */
    float64         lowrightpt[2];	/* Lower right pt coordinates */
    float64         projparm[16];	/* Projection parameters */
    float64         xVal = 0.0;	/* "Exact" x location of interpolated point */
    float64         yVal = 0.0;	/* "Exact" y location of interpolated point */
    float64         tNum = 0.0;	/* Interpolation numerator value 1 */
    float64         uNum = 0.0;	/* Interpolation numerator value 2 */

    int16           i16[4];	/* Working buffer (int16) */
    int32           i32[4];	/* Working buffer (int132) */
    float32         f32[4];	/* Working buffer (float32) */
    float64         f64[4];	/* Working buffer (float64) */

    char           *pixVal;	/* Nearest neighbor values */
    char           *dimlist;	/* Dimension list */

    /* Allocate space for dimlist */
    /* --------------------------------- */
    dimlist = (char *) calloc(UTLSTR_MAX_SIZE, sizeof(char));
    if(dimlist == NULL)
    { 
	HEpush(DFE_NOSPACE,"GDinterpolate", __FILE__, __LINE__);
	return(-1);
    }
    /* Check for valid grid ID */
    /* ----------------------- */
    status = GDchkgdid(gridID, "GDinterpolate",
		       &fid, &sdInterfaceID, &gdVgrpID);


    /* If no problems ... */
    /* ------------------ */
    if (status == 0)
    {
	/* Get field information */
	/* --------------------- */
	status = GDfieldinfo(gridID, fieldname, &rank, dims, &ntype, dimlist);


	/* Check for "XDim" & "YDim" in dimension list */
	/* ------------------------------------------- */
	if (status == 0)
	{
	    xdum = EHstrwithin("XDim", dimlist, ',');
	    ydum = EHstrwithin("YDim", dimlist, ',');

	    if (xdum == -1)
	    {
		status = -1;
		HEpush(DFE_GENAPP, "GDinterpolate", __FILE__, __LINE__);
		HEreport(
		     "\"XDim\" not present in dimlist for field: \"%s\".\n",
			 fieldname);
	    }


	    if (ydum == -1)
	    {
		status = -1;
		HEpush(DFE_GENAPP, "GDinterpolate", __FILE__, __LINE__);
		HEreport(
		     "\"YDim\" not present in dimlist for field: \"%s\".\n",
			 fieldname);
	    }
	}
	else
	{
	    /* Fieldname not found in grid */
	    /* --------------------------- */
	    status = -1;
	    HEpush(DFE_GENAPP, "GDinterpolate", __FILE__, __LINE__);
	    HEreport("Fieldname \"%s\" not found.\n", fieldname);
	}


	/* If no problems ... */
	/* ------------------ */
	if (status == 0)
	{
	    /* Compute size of data buffer for each interpolated value */
	    /* ------------------------------------------------------- */
	    dims[xdum] = 1;
	    dims[ydum] = 1;
	    size = dims[0];
	    for (i = 1; i < rank; i++)
	    {
		size *= dims[i];
	    }
	    numsize = DFKNTsize(ntype);
	    size *= numsize;

	    nRetn = size / numsize;



	    /* If interpolated values are requested ... */
	    /* ---------------------------------------- */
	    if (interpVal != NULL)
	    {
		/* Get grid info */
		/* ------------- */
		status = GDgridinfo(gridID, &xdimsize, &ydimsize,
				    upleftpt, lowrightpt);


		/* Get projection info */
		/* ------------------- */
		status = GDprojinfo(gridID, &projcode, &zonecode,
				    &spherecode, projparm);


		/* Get explicit upleftpt & lowrightpt if defaults are used */
		/* ------------------------------------------------------- */
		status = GDgetdefaults(projcode, zonecode, projparm,
				       spherecode, upleftpt, lowrightpt);


		/* Get pixel registration and origin info */
		/* -------------------------------------- */
		status = GDpixreginfo(gridID, &pixregcode);
		status = GDorigininfo(gridID, &origincode);



		/* Loop through all interpolated points */
		/* ------------------------------------ */
		for (i = 0; i < nValues; i++)
		{
		    /* Get row & column of point pixel */
		    /* ------------------------------- */
		    status = GDll2ij(projcode, zonecode, projparm, spherecode,
				   xdimsize, ydimsize, upleftpt, lowrightpt,
				     1, &lonVal[i], &latVal[i],
				     pixRow, pixCol, &xVal, &yVal);


		    /* Get diff of interp. point from pixel location */
		    /* --------------------------------------------- */
		    if (pixregcode == HDFE_CENTER)
		    {
			tNum = xVal - (pixCol[0] + 0.5);
			uNum = yVal - (pixRow[0] + 0.5);
		    }
		    else if (origincode == HDFE_GD_UL)
		    {
			tNum = xVal - pixCol[0];
			uNum = yVal - pixRow[0];
		    }
		    else if (origincode == HDFE_GD_UR)
		    {
			tNum = xVal - (pixCol[0] + 1);
			uNum = yVal - pixRow[0];
		    }
		    else if (origincode == HDFE_GD_LL)
		    {
			tNum = xVal - pixCol[0];
			uNum = yVal - (pixRow[0] + 1);
		    }
		    else if (origincode == HDFE_GD_LR)
		    {
			tNum = xVal - (pixCol[0] + 1);
			uNum = yVal - (pixRow[0] + 1);
		    }


		    /* Get rows and columns of other nearest neighbor pixels */
		    /* ----------------------------------------------------- */
		    pixCol[1] = pixCol[0];
		    pixRow[3] = pixRow[0];

		    if (tNum >= 0)
		    {
			pixCol[2] = pixCol[0] + 1;
			pixCol[3] = pixCol[0] + 1;
		    }

		    if (tNum < 0)
		    {
			pixCol[2] = pixCol[0] - 1;
			pixCol[3] = pixCol[0] - 1;
		    }

		    if (uNum >= 0)
		    {
			pixRow[2] = pixRow[0] + 1;
			pixRow[1] = pixRow[0] + 1;
		    }

		    if (uNum < 0)
		    {
			pixRow[2] = pixRow[0] - 1;
			pixRow[1] = pixRow[0] - 1;
		    }


		    /* Get values of nearest neighbors  */
		    /* -------------------------------- */
		    pixVal = (char *) malloc(4 * size);
		    if(pixVal == NULL)
		    { 
			HEpush(DFE_NOSPACE,"GDinterpolate", __FILE__, __LINE__);
			free(dimlist);
			return(-1);
		    }
		    dum = GDgetpixvalues(gridID, 4, pixRow, pixCol,
					 fieldname, pixVal);


		    /* Trap interpolation boundary out of range error */
		    /* ---------------------------------------------- */
		    if (dum == -1)
		    {
			status = -1;
			HEpush(DFE_GENAPP, "GDinterpolate", __FILE__, __LINE__);
			HEreport("Interpolation boundary outside of grid.\n");
		    }
		    else
		    {

			/*
			 * Algorithm taken for Numerical Recipies in C, 2nd
			 * edition, Section 3.6
			 */

			/* Perform bilinear interpolation */
			/* ------------------------------ */
			tDen = pixCol[3] - pixCol[0];
			uDen = pixRow[1] - pixRow[0];

			switch (ntype)
			{
			case DFNT_INT16:


			    /* Loop through all returned data values */
			    /* ------------------------------------- */
			    for (j = 0; j < nRetn; j++)
			    {
				/* Copy 4 NN values into working array */
				/* ----------------------------------- */
				for (k = 0; k < 4; k++)
				{
				    memcpy(&i16[k],
					   pixVal + j * numsize + k * size,
					   sizeof(int16));
				}

				/* Compute interpolated value */
				/* -------------------------- */
				interpVal[i * nRetn + j] =
				    (1 - tNum / tDen) * (1 - uNum / uDen) *
				    i16[0] +
				    (tNum / tDen) * (1 - uNum / uDen) *
				    i16[3] +
				    (tNum / tDen) * (uNum / uDen) *
				    i16[2] +
				    (1 - tNum / tDen) * (uNum / uDen) *
				    i16[1];
			    }
			    break;


			case DFNT_INT32:

			    for (j = 0; j < nRetn; j++)
			    {
				for (k = 0; k < 4; k++)
				{
				    memcpy(&i32[k],
					   pixVal + j * numsize + k * size,
					   sizeof(int32));
				}

				interpVal[i * nRetn + j] =
				    (1 - tNum / tDen) * (1 - uNum / uDen) *
				    i32[0] +
				    (tNum / tDen) * (1 - uNum / uDen) *
				    i32[3] +
				    (tNum / tDen) * (uNum / uDen) *
				    i32[2] +
				    (1 - tNum / tDen) * (uNum / uDen) *
				    i32[1];
			    }
			    break;


			case DFNT_FLOAT32:

			    for (j = 0; j < nRetn; j++)
			    {
				for (k = 0; k < 4; k++)
				{
				    memcpy(&f32[k],
					   pixVal + j * numsize + k * size,
					   sizeof(float32));
				}

				interpVal[i * nRetn + j] =
				    (1 - tNum / tDen) * (1 - uNum / uDen) *
				    f32[0] +
				    (tNum / tDen) * (1 - uNum / uDen) *
				    f32[3] +
				    (tNum / tDen) * (uNum / uDen) *
				    f32[2] +
				    (1 - tNum / tDen) * (uNum / uDen) *
				    f32[1];
			    }
			    break;


			case DFNT_FLOAT64:

			    for (j = 0; j < nRetn; j++)
			    {
				for (k = 0; k < 4; k++)
				{
				    memcpy(&f64[k],
					   pixVal + j * numsize + k * size,
					   sizeof(float64));
				}

				interpVal[i * nRetn + j] =
				    (1 - tNum / tDen) * (1 - uNum / uDen) *
				    f64[0] +
				    (tNum / tDen) * (1 - uNum / uDen) *
				    f64[3] +
				    (tNum / tDen) * (uNum / uDen) *
				    f64[2] +
				    (1 - tNum / tDen) * (uNum / uDen) *
				    f64[1];
			    }
			    break;
			}
		    }
		    free(pixVal);
		}
	    }
	}
    }


    /* If successful return size of returned data in bytes */
    /* --------------------------------------------------- */
    if (status == 0)
    {
	/*always return size of float64 buffer */
	free(dimlist);
	return (nRetn  * nValues * sizeof(float64));
    }
    else
    {
	free(dimlist);
	return ((int32) status);
    }

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
GDwritetile --
     This function writes one tile to a particular field.


Author--
Alexis Zubrow

********************************************************/

intn
GDwritetile(int32 gridID, const char *fieldname, int32 tilecoords[],
	    VOIDP tileData)
{
    char            code[] = "w";	/* write tile code */
    intn            status = 0;	/* routine return status variable */

    status = GDwrrdtile(gridID, fieldname, code, tilecoords, tileData);

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
/***********************************************
GDsettilecache --
     This function sets the cache size for a tiled field.


Author--
Alexis Zubrow

********************************************************/

intn
GDsettilecache(int32 gridID, const char *fieldname, int32 maxcache, CPL_UNUSED int32 cachecode)
{

    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           sdid;	/* SDS ID */

    int32           dum;	/* Dummy variable */

    int32           dims[8];	/* Field/SDS dimensions */


    /* Check gridID */
    status = GDchkgdid(gridID, "GDwrrdtile", &fid, &sdInterfaceID, &dum);
    if (status == 0)
    {

	/* Get field info */
	status = GDfieldinfo(gridID, fieldname, &dum, dims, &dum, NULL);

	if (status == 0)
	{

	    /* Check whether fieldname is in SDS (multi-dim field) */
	    /* --------------------------------------------------- */
	    status = GDSDfldsrch(gridID, sdInterfaceID, fieldname, &sdid,
				 &dum, &dum, &dum, dims, &dum);


	    /* Check if maxcache is less than or equal to zero */
	    if (maxcache <= 0)
	    {
		HEpush(DFE_GENAPP, "GDsettilecache", __FILE__, __LINE__);
		HEreport("Improper maxcache \"%d\"... \n", maxcache);
		HEreport("maxcache must be greater than zero.\n");
		status = -1;
		return (status);
	    }


	    /* Set the number of tiles to cache */
	    /* Presently, the only cache flag allowed is 0 */
	    status = SDsetchunkcache(sdid, maxcache, 0);


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

/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDsettilecomp                                                    |
|                                                                             |
|  DESCRIPTION: Sets the tiling/compression parameters for the   specified    |
|               field. This can be called after GDsetfillvalue and assumes    |
|               that the field was defined with no compression/tiling  set    |
|               by GDdeftile or GDdefcomp.                                    |
|                                                                             |
|               This function replaces the following sequence:                |
|                  GDdefcomp                                                  |
|                  GDdeftile                                                  |
|                  GDdeffield                                                 |
|                  GDsetfillvalue                                             |
|               with:                                                         |
|                  GDdeffield                                                 |
|                  GDsetfillvalue                                             |
|                  GDsettilecomp                                              |
|               so that fill values will work correctly.                      |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  gridID         int32               grid structure ID                       |
|  fieldname      char                field name                              |
|  tilerank       int32               number of tiling dimensions             |
|  tiledims       int32               tiling dimensions                       |
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
|  Jun 98   MISR          Used GDsetfillvalue as a template and copied        |
|                         tiling/comp portions of GDdeffield.(NCR15866).      |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
GDsettilecomp(int32 gridID, const char *fieldname, int32 tilerank, int32*
               tiledims, int32 compcode, intn* compparm)
{
    intn            status;     /* routine return status variable */
 
    int32           fid;        /* HDF-EOS file ID */
    int32           sdInterfaceID;      /* HDF SDS interface ID */
    int32           gdVgrpID;   /* Grid root Vgroup ID */
    int             i;          /* Looping variable. */
    int32           sdid;       /* SDS id */
    int32           nt;         /* Number type */
    int32           dims[8];    /* Dimensions array */
    int32           dum;        /* Dummy variable */
    int32           solo;       /* "Solo" (non-merged) field flag */
    comp_info       c_info;     /* Compression parameter structure */
    HDF_CHUNK_DEF   chunkDef;   /* Tiling structure */
    int32           chunkFlag;  /* Chunking (Tiling) flag */
 
    c_info.nbit.nt = 0;
 
    /* Check for valid grid ID and get SDS interface ID */
    status = GDchkgdid(gridID, "GDsetfillvalue",
                       &fid, &sdInterfaceID, &gdVgrpID);
 
    if (status == 0)
    {
        /* Get field info */
        status = GDfieldinfo(gridID, fieldname, &dum, dims, &nt, NULL);
 
        if (status == 0)
        {
            /* Get SDS ID and solo flag */
            status = GDSDfldsrch(gridID, sdInterfaceID, fieldname,
                                 &sdid, &dum, &dum, &dum,
                                 dims, &solo);
            if (status !=0) {
              HEpush(DFE_GENAPP, "GDsettilecomp", __FILE__, __LINE__);
              HEreport("GDSDfldsrch failed\n", fieldname);
              return FAIL;
            }
            /* Tiling with Compression */
            /* ----------------------- */
 
 
            /* Setup Compression */
            /* ----------------- */
            if (compcode == HDFE_COMP_NBIT)
              {
                c_info.nbit.nt = nt;
                c_info.nbit.sign_ext = compparm[0];
                c_info.nbit.fill_one = compparm[1];
                c_info.nbit.start_bit = compparm[2];
                c_info.nbit.bit_len = compparm[3];
              }
            else if (compcode == HDFE_COMP_SKPHUFF)
              {
                c_info.skphuff.skp_size = (intn) DFKNTsize(nt);
              }
            else if (compcode == HDFE_COMP_DEFLATE)
              {
                c_info.deflate.level = compparm[0];
              }
 
            /* Setup chunk lengths */
            /* ------------------- */
            for (i = 0; i < tilerank; i++)
              {
                chunkDef.comp.chunk_lengths[i] = tiledims[i];
              }
 
            /* Setup chunk flag & chunk compression type */
            /* ----------------------------------------- */
            chunkFlag = HDF_CHUNK | HDF_COMP;
            chunkDef.comp.comp_type = compcode;
 
            /* Setup chunk compression parameters */
            /* ---------------------------------- */
            if (compcode == HDFE_COMP_SKPHUFF)
              {
                chunkDef.comp.cinfo.skphuff.skp_size =
                  c_info.skphuff.skp_size;
              }
            else if (compcode == HDFE_COMP_DEFLATE)
              {
                chunkDef.comp.cinfo.deflate.level =
                  c_info.deflate.level;
              }
            /* Call SDsetchunk routine */
            /* ----------------------- */
            status = SDsetchunk(sdid, chunkDef, chunkFlag);
            if (status ==FAIL) {
              HEpush(DFE_GENAPP, "GDsettilecomp", __FILE__, __LINE__);
              HEreport("Fieldname \"%s\" does not exist.\n",
                       fieldname);
              return status;
            }
        }
        else
        {
            HEpush(DFE_GENAPP, "GDsettilecomp", __FILE__, __LINE__);
            HEreport("Fieldname \"%s\" does not exist.\n", fieldname);
        }
    }
    return (status);
}

/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDll2mm_cea                                                      |
|                                                                             |
|  DESCRIPTION:                                                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  projcode       int32               GCTP projection code                    |
|  zonecode       int32               UTM zone code                           |
|  projparm       float64             Projection parameters                   |
|  spherecode     int32               GCTP spheriod code                      |
|  xdimsize       int32               xdimsize from GDcreate                  |
|  ydimsize       int32               ydimsize from GDcreate                  |
|  upleftpt       float64             upper left corner coordinates  (DMS)    |
|  lowrightpt     float64             lower right corner coordinates (DMS)    |
|  longitude      float64             longitude array (DMS)                   |
|  latitude       float64             latitude array (DMS)                    |
|  npnts          int32               number of lon-lat points                |
|                                                                             |
|  OUTPUTS:                                                                   |
|  x              float64             X value array                           |
|  y              float64             Y value array                           |
|  scaleX	  float64             X grid size                             |
|  scaley         float64             Y grid size                             |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Oct 02   Abe Taaheri   Added support for EASE grid                         |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
static intn GDll2mm_cea(int32 projcode,int32 zonecode, int32 spherecode,
		 float64 projparm[],
		 int32 xdimsize, int32 ydimsize,
		 float64 upleftpt[], float64 lowrightpt[], int32 npnts,
		 CPL_UNUSED float64 lon[],CPL_UNUSED float64 lat[],
		 float64 x[],float64 y[], float64 *scaleX,float64 *scaleY)
{
    intn            status = 0;	/* routine return status variable */
    int32           errorcode = 0;	/* GCTP error code */
    float64         xMtr0, xMtr1, yMtr0, yMtr1;
    float64         lonrad0;	/* Longitude in radians of upleft point */
    float64         latrad0;     /* Latitude in radians of upleft point */
    float64         lonrad;	/* Longitude in radians of point */
    float64         latrad;	/* Latitude in radians of point */
    int32(*for_trans[100]) ();	/* GCTP function pointer */

    if(npnts <= 0)
      {
	HEpush(DFE_GENAPP, " GDll2mm_cea", __FILE__, __LINE__);
	HEreport("Improper npnts value\"%d\"... \n", npnts);
	HEreport("npnts must be greater than zero.\n");
	status = -1;
	return (status);
      }
    if ( projcode == GCTP_BCEA)
      {
	for_init(projcode, zonecode, projparm, spherecode, NULL, NULL,
		 &errorcode, for_trans);
	/* Convert upleft and lowright X coords from DMS to radians */
	/* -------------------------------------------------------- */
	
	lonrad0 = EHconvAng(upleftpt[0], HDFE_DMS_RAD);
	lonrad = EHconvAng(lowrightpt[0], HDFE_DMS_RAD);
	
	/* Convert upleft and lowright Y coords from DMS to radians */
	/* -------------------------------------------------------- */
	latrad0 = EHconvAng(upleftpt[1], HDFE_DMS_RAD);
	latrad = EHconvAng(lowrightpt[1], HDFE_DMS_RAD);
	
	/* Convert from lon/lat to meters(or whatever unit is, i.e unit
	   of r_major and r_minor) using GCTP */
	/* ----------------------------------------- */
	errorcode = for_trans[projcode] (lonrad0, latrad0, &xMtr0, &yMtr0);
	x[0] = xMtr0;
	y[0] = yMtr0;
	
	/* Report error if any */
	/* ------------------- */
	if (errorcode != 0)
	  {
	    status = -1;
	    HEpush(DFE_GENAPP, "GDll2mm_cea", __FILE__, __LINE__);
	    HEreport("GCTP Error: %d\n", errorcode);
	    return (status);
	  }
	
	/* Convert from lon/lat to meters(or whatever unit is, i.e unit
	   of r_major and r_minor) using GCTP */
	/* ----------------------------------------- */
	errorcode = for_trans[projcode] (lonrad, latrad, &xMtr1, &yMtr1);
	x[1] = xMtr1;
	y[1] = yMtr1;
	
	/* Report error if any */
	/* ------------------- */
	if (errorcode != 0)
	  {
	    status = -1;
	    HEpush(DFE_GENAPP, "GDll2mm_cea", __FILE__, __LINE__);
	    HEreport("GCTP Error: %d\n", errorcode);
	    return (status);
	  }
	
	/* Compute x scale factor */
	/* ---------------------- */
	*scaleX = (xMtr1 - xMtr0) / xdimsize;
	
	/* Compute y scale factor */
	/* ---------------------- */
	*scaleY = (yMtr1 - yMtr0) / ydimsize;
      }
    else
      {
        status = -1;
        HEpush(DFE_GENAPP, "GDll2mm_cea", __FILE__, __LINE__);
        HEreport("Wrong projection code; this function is only for EASE grid");
        return (status);
      }
    return (0);
}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: GDmm2ll_cea                                                      |
|                                                                             |
|  DESCRIPTION:                                                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  projcode       int32               GCTP projection code                    |
|  zonecode       int32               UTM zone code                           |
|  projparm       float64             Projection parameters                   |
|  spherecode     int32               GCTP spheriod code                      |
|  xdimsize       int32               xdimsize from GDcreate                  |
|  ydimsize       int32               ydimsize from GDcreate                  |
|  upleftpt       float64             upper left corner coordinates (DMS)     |
|  lowrightpt     float64             lower right corner coordinates (DMS)    |
|  x              float64             X value array                           |
|  y              float64             Y value array                           |
|  npnts          int32               number of x-y points                    |
|                                                                             |
|  OUTPUTS:                                                                   |
|  longitude      float64             longitude array (DMS)                   |
|  latitude       float64             latitude array (DMS)                    |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Oct 02   Abe Taaheri   Added support for EASE grid                         |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
static intn GDmm2ll_cea(int32 projcode,int32 zonecode, int32 spherecode,
		 float64 projparm[],
		 CPL_UNUSED int32 xdimsize, CPL_UNUSED int32 ydimsize,
		 CPL_UNUSED float64 upleftpt[], CPL_UNUSED float64 lowrightpt[], int32 npnts,
		 float64 x[], float64 y[], 
		 float64 longitude[], float64 latitude[])
{
    intn            status = 0;	/* routine return status variable */
    int32           errorcode = 0;	/* GCTP error code */
    int32(*inv_trans[100]) ();	/* GCTP function pointer */
    int32 i;

    if(npnts <= 0)
      {
	HEpush(DFE_GENAPP, " GDmm2ll_cea", __FILE__, __LINE__);
	HEreport("Improper npnts value\"%d\"... \n", npnts);
	HEreport("npnts must be greater than zero.\n");
	status = -1;
	return (status);
      }
    if ( projcode == GCTP_BCEA)
      {
	inv_init(projcode, zonecode, projparm, spherecode, NULL, NULL,
		 &errorcode, inv_trans);
	
	/* Convert from meters(or whatever unit is, i.e unit
	   of r_major and r_minor) to lat/lon using GCTP */
	/* ----------------------------------------- */
	for(i=0; i<npnts; i++)
	  {
	    errorcode = 
	      inv_trans[projcode] (x[i], y[i],&longitude[i], &latitude[i]);
	    /* Report error if any */
	    /* ------------------- */
	    if (errorcode != 0)
	      {
		status = -1;
		HEpush(DFE_GENAPP, "GDmm2ll_cea", __FILE__, __LINE__);
		HEreport("GCTP Error: %d\n", errorcode);
		return (status);
	      }
	    longitude[i] = EHconvAng(longitude[i], HDFE_RAD_DMS);
	    latitude[i] = EHconvAng(latitude[i], HDFE_RAD_DMS);
	  }
      }
    else
      {
	/* Wrong projection code; this function is only for EASE grid */
      }
    return(status);
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
