/*
Copyright (C) 1996 Hughes and Applied Research Corporation

Permission to use, modify, and distribute this software and its documentation 
for any purpose without fee is hereby granted, provided that the above 
copyright notice appear in all copies and that both that copyright notice and 
this permission notice appear in supporting documentation.

June 5, 2003 Abe Taaheri  Removed declarations for unused variables idOffset, 
			    nlevels, and pID in PTrecnum
			  Removed initialization code for unused variables in 
			    PTrecnum
			  Removed declarations for unused variables idOffset 
			    and vdataID in PTregionrecs
			  Removed initialization code for unused variables in 
			    PTregionrecs

*/


#include "hdf.h"
#include "cfortHdf.h"
#include "HdfEosDef.h"

#define	PTIDOFFSET 2097152


#define NPOINT 64
struct pointStructure
{
    int32 active;
    int32 IDTable;
    int32 VIDTable[3];
    int32 fid;
    int32 vdID[8];
};
struct pointStructure PTXPoint[NPOINT];



#define NPOINTREGN 256
struct pointRegion 
{
    int32 fid;
    int32 pointID;
    int32 nrec[8];
    int32 *recPtr[8];
};
struct pointRegion *PTXRegion[NPOINTREGN];



/* Point Prototype (internal routines) */
intn PTchkptid(int32, char *, int32 *, int32 *, int32 *);
intn PTlinkinfo(int32, int32, int32, char *mode, char *);
intn PTwrbckptr(int32, int32, int32, int32 []);
intn PTrdbckptr(int32, int32, int32, int32 []);
intn PTwrfwdptr(int32, int32);
intn PTrdfwdptr(int32, int32, int32, int32 []);
intn PTwritesetup(int32, int32, int32, int32, int32 *, int32 *);
int32 PTrecnum(int32, int32, int32, int32, int32, int32 []);
intn PTwrrdattr(int32, char *, int32, int32, char *, VOIDP);











/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTopen                                                           |
|                                                                             |
|  DESCRIPTION: Opens an HDF file and returns file ID.                        |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  fid            int32               HDF-EOS file ID                         |
|                                                                             |
|  INPUTS:                                                                    |
|  filename       char                Point Filename                          |
|  access code    intn                Access Code                             |
|                                        DFACC_CREATE                         |
|                                        DFACC_RDWR                           |
|                                        DFACC_READ                           |
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
PTopen(char *filename, intn access)

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
|  FUNCTION: PTcreate                                                         |
|                                                                             |
|  DESCRIPTION: Creates a new point data set and returns a handle.            |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  pointID        int32               Point structure ID                      |
|                                                                             |
|  INPUTS:                                                                    |
|  fid            int32               File ID                                 |
|  pointname      char                Point structure name                    |
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
|  Aug 96   Joel Gales    Check point name for length                         |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
PTcreate(int32 fid, char *pointname)


{
    intn            i;		/* Loop index */
    intn            npointopen = 0;	/* # of point structures open */
    intn            status = 0;	/* routine return status variable */

    uint8           access;	/* Read/Write file access code */
    uint8           zerobuf[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    /* Data buffer for "Level Written Vdata" */

    int32           HDFfid;	/* HDF file id */
    int32           vgRef;	/* Vgroup reference number */
    int32           vgid[4];	/* Vgroup ID array */
    int32           pointID = -1;	/* HDF-EOS point ID */

    int32           vdataID;	/* Vdata ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           idOffset = PTIDOFFSET;	/* Point ID offset */
    int32           nPoint = 0;	/* Point counter */

    char            name[80];	/* Vgroup name */
    char            class[80];	/* Vgroup class */
    char            utlbuf[512];/* Utility buffer */
    char            utlbuf2[128];	/* Utility buffer 2 */


    /*
     * Check HDF-EOS file ID, get back HDF file ID, SD interface ID  and
     * access code
     */
    status = EHchkfid(fid, pointname, &HDFfid, &sdInterfaceID, &access);


    /* Check pointname for length */
    /* -------------------------- */
    if ((intn) strlen(pointname) > VGNAMELENMAX)
    {
	status = -1;
	HEpush(DFE_GENAPP, "PTcreate", __FILE__, __LINE__);
	HEreport("Pointname \"%s\" must be less than %d characters.\n",
		 pointname, VGNAMELENMAX);
    }



    if (status == 0)
    {

	/* Determine number of points currently opened */
	/* ------------------------------------------- */
	for (i = 0; i < NPOINT; i++)
	{
	    npointopen += PTXPoint[i].active;
	}


	/* Setup file interface */
	/* -------------------- */
	if (npointopen < NPOINT)
	{

	    /* Check that point has not been previously opened */
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


		/* If POINT then increment # point counter */
		/* --------------------------------------- */
		if (strcmp(class, "POINT") == 0)
		{
		    nPoint++;
		}


		/* If point already exist, return error */
		/* ------------------------------------ */
		if (strcmp(name, pointname) == 0 &&
		    strcmp(class, "POINT") == 0)
		{
		    status = -1;
		    HEpush(DFE_GENAPP, "PTcreate", __FILE__, __LINE__);
		    HEreport("\"%s\" already exists.\n", pointname);
		    break;
		}
	    }


	    if (status == 0)
	    {


		/* Create Root Vgroup for Point */
		/* ---------------------------- */
		vgid[0] = Vattach(HDFfid, -1, "w");


		/* Set Name and Class (POINT) */
		/* -------------------------- */
		Vsetname(vgid[0], pointname);
		Vsetclass(vgid[0], "POINT");


		/* Create Level Written Vdata */
		/* -------------------------- */
		vdataID = VSattach(HDFfid, -1, "w");
		VSfdefine(vdataID, "LevelWritten", DFNT_UINT8, 1);
		VSsetfields(vdataID, "LevelWritten");
		VSwrite(vdataID, zerobuf, 8, FULL_INTERLACE);
		VSsetname(vdataID, "LevelWritten");
		Vinsert(vgid[0], vdataID);
		VSdetach(vdataID);



		/* Create Data Records Vgroup */
		/* -------------------------- */
		vgid[1] = Vattach(HDFfid, -1, "w");
		Vsetname(vgid[1], "Data Vgroup");
		Vsetclass(vgid[1], "POINT Vgroup");
		Vinsert(vgid[0], vgid[1]);



		/* Create Linkage Records Vgroup */
		/* ----------------------------- */
		vgid[2] = Vattach(HDFfid, -1, "w");
		Vsetname(vgid[2], "Linkage Vgroup");
		Vsetclass(vgid[2], "POINT Vgroup");
		Vinsert(vgid[0], vgid[2]);


		/* Create Point Attributes Vgroup */
		/* ------------------------------ */
		vgid[3] = Vattach(HDFfid, -1, "w");
		Vsetname(vgid[3], "Point Attributes");
		Vsetclass(vgid[3], "POINT Vgroup");
		Vinsert(vgid[0], vgid[3]);



		/* Establish Point in Structural MetaData Block */
		/* -------------------------------------------- */
		sprintf(utlbuf, "%s%d%s%s%s",
			"\tGROUP=POINT_", nPoint + 1,
			"\n\t\tPointName=\"", pointname, "\"\n");

		strcat(utlbuf, "\t\tGROUP=Level\n");
		strcat(utlbuf, "\t\tEND_GROUP=Level\n");
		strcat(utlbuf, "\t\tGROUP=LevelLink\n");
		strcat(utlbuf, "\t\tEND_GROUP=LevelLink\n");
		sprintf(utlbuf2, "%s%d%s",
			"\tEND_GROUP=POINT_", nPoint + 1, "\n");
		strcat(utlbuf, utlbuf2);


		status = EHinsertmeta(sdInterfaceID, "", "p", 1003L,
				      utlbuf, NULL);
	    }
	}
	else
	{
	    /* Too many files opened */
	    /* --------------------- */
	    status = -1;
	    strcpy(utlbuf,
		   "No more than %d points may be open simutaneously");
	    strcat(utlbuf, " (%s)");
	    HEpush(DFE_DENIED, "PTcreate", __FILE__, __LINE__);
	    HEreport(utlbuf, NPOINT, pointname);
	}


	/* Assign pointID # & Load point and PTXPoint table entries */
	/* -------------------------------------------------------- */
	if (status == 0)
	{
	    for (i = 0; i < NPOINT; i++)
	    {
		if (PTXPoint[i].active == 0)
		{
		    /*
		     * Set pointID, Set point entry active, Store root Vgroup
		     * ID, Store sub Vgroup IDs, Store HDF-EOS file ID
		     */
		    pointID = i + idOffset;
		    PTXPoint[i].active = 1;
		    PTXPoint[i].IDTable = vgid[0];
		    PTXPoint[i].VIDTable[0] = vgid[1];
		    PTXPoint[i].VIDTable[1] = vgid[2];
		    PTXPoint[i].VIDTable[2] = vgid[3];
		    PTXPoint[i].fid = fid;
		    status = 0;
		    break;
		}
	    }
	}
    }


    return (pointID);

}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTchkptid                                                        |
|                                                                             |
|  DESCRIPTION: Checks for valid pointID and returns file ID, SDS ID, and     |
|               point Vgroup ID                                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               point structure ID                      |
|  routname       char                Name of routine calling PTchkptid       |
|                                                                             |
|  OUTPUTS:                                                                   |
|  fid            int32               File ID                                 |
|  sdInterfaceID  int32               SDS interface ID                        |
|  ptVgroupID     int32               point Vgroup ID                         |
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
PTchkptid(int32 pointID, char *routname,
	  int32 * fid, int32 * sdInterfaceID, int32 * ptVgroupID)
{
    intn            status = 0;	/* routine return status variable */

    uint8           access;	/* Read/Write access code */

    int32           idOffset = PTIDOFFSET;	/* Point ID offset */

    char            message1[] =
    "Invalid point id: %d in routine \"%s\".  ID must be >= %d and < %d.\n";
    char            message2[] =
    "Point id %d in routine \"%s\" not active.\n";


    /* Check for valid point id */
    /* ------------------------ */
    if (pointID < idOffset || pointID >= NPOINT + idOffset)
    {
	status = -1;
	HEpush(DFE_RANGE, "PTchkptid", __FILE__, __LINE__);
	HEreport(message1, pointID, routname, idOffset, NPOINT + idOffset);
    }
    else
    {
	/* Check for active point ID */
	/* ------------------------- */
	if (PTXPoint[pointID % idOffset].active == 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "PTchkptid", __FILE__, __LINE__);
	    HEreport(message2, pointID, routname);
	}
	else
	{

	    /* Get file and Point key */
	    /* ----------------------- */
	    status = EHchkfid(PTXPoint[pointID % idOffset].fid, " ", fid,
			      sdInterfaceID, &access);
	    *ptVgroupID = PTXPoint[pointID % idOffset].IDTable;
	}
    }
    return (status);

}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTnrecs                                                          |
|                                                                             |
|  DESCRIPTION: Returns the number of records in a level.                     |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  nrec           int32               Number of records in level              |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               point structure ID                      |
|  level          int32               level number (0 - based)                |
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
PTnrecs(int32 pointID, int32 level)
{
    intn            status = 0;	/* routine return status variable */

    uint8           recChk;	/* Record check flag */

    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           idOffset = PTIDOFFSET;	/* Point ID offset */
    int32           ptVgrpID;	/* Point Vgroup ID */
    int32           nlevels;	/* Number of levels in point */
    int32           vdataID;	/* Level Vdata ID */
    int32           vdataID0;	/* LevelWritten Vdata ID */
    int32           tag;	/* Vdata tag */
    int32           ref;	/* Vdata reference */
    int32           nrec;	/* Number of records in level */


    /* Check for valid point ID */
    /* ------------------------ */
    status = PTchkptid(pointID, "PTnrecs", &fid, &sdInterfaceID, &ptVgrpID);


    if (status == 0)
    {
	/* Get number of levels */
	/* -------------------- */
	nlevels = Vntagrefs(ptVgrpID);


	/* Report error if no levels (vdatas) defined */
	/* ------------------------------------------ */
	if (nlevels == 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "PTnrecs", __FILE__, __LINE__);
	    HEreport("No Levels Defined for point ID: %d\n", pointID);
	}
	else if (nlevels < level)
	{
	    /* Report error if level # to large */
	    /* -------------------------------- */
	    status = -1;
	    HEpush(DFE_GENAPP, "PTnrecs", __FILE__, __LINE__);
	    HEreport("Only %d levels Defined for point ID: %d\n",
		     nlevels, pointID);
	}


	if (status == 0)
	{
	    /* Get level vdata ID */
	    /* ------------------ */
	    vdataID = PTXPoint[pointID % idOffset].vdID[level];


	    /* Get number of records in level */
	    /* ------------------------------ */
	    nrec = VSelts(vdataID);


	    /* If nrec = 1 check whether actual data has been written */
	    /* ------------------------------------------------------ */
	    if (nrec == 1)
	    {
		/* Attach to "LevelWritten" vdata */
		/* ------------------------------ */
		Vgettagref(ptVgrpID, 0, &tag, &ref);
		vdataID0 = VSattach(fid, ref, "r");


		/* Read record for desired level */
		/* ----------------------------- */
		VSseek(vdataID0, level);
		VSsetfields(vdataID0, "LevelWritten");
		VSread(vdataID0, &recChk, 1, FULL_INTERLACE);


		/* If level not yet written then reset nrec to 0 */
		/* --------------------------------------------- */
		if (recChk == 0)
		{
		    nrec = 0;
		}


		/* Detach for "LevelWritten" Vdata */
		/* ------------------------------- */
		VSdetach(vdataID0);
	    }
	}
    }
    return (nrec);

}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTnlevels                                                        |
|                                                                             |
|  DESCRIPTION: Returns the number of levels in a point data set.             |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  nlevels        int32               Number of levels in point structure     |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               point structure ID                      |
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
PTnlevels(int32 pointID)

{
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           idOffset = PTIDOFFSET;	/* Point ID offset */
    int32           ptVgrpID;	/* Point Vgroup ID */
    int32           nlevels;	/* Number of levels in point */


    /* Check for valid point ID */
    /* ------------------------ */
    status = PTchkptid(pointID, "PTnlevels", &fid, &sdInterfaceID, &ptVgrpID);


    /* Get number of levels (Number of entries in Data Vgroup */
    /* ------------------------------------------------------ */
    if (status == 0)
    {
	nlevels = Vntagrefs(PTXPoint[pointID % idOffset].VIDTable[0]);
    }
    return (nlevels);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTsizeof                                                         |
|                                                                             |
|  DESCRIPTION: Returns size in bytes for specified fields in a point         |
|               data set.                                                     |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  fldsz          int32               Size in bytes of fields                 |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               point structure ID                      |
|  fieldlist      char                field list (comma-separated)            |
|                                                                             |
|  OUTPUTS:                                                                   |
|  fldlevels      int32               Levels of fields                        |
|                                                                             |
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
PTsizeof(int32 pointID, char *fieldlist, int32 fldlevels[])
{
    intn            i;		/* Loop index */
    intn            j;		/* Loop index */
    intn            status = 0;	/* routine return status variable */

    uint8           found[VSFIELDMAX];	/* Field found flag array */

    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           idOffset = PTIDOFFSET;	/* Point ID offset */
    int32           ptVgrpID;	/* Point Vgroup ID */
    int32           nlevels;	/* Number of levels in point */
    int32           nflds;	/* Number of fields in fieldlist */
    int32           vdataID;	/* Vdata ID */
    int32           slen[VSFIELDMAX];	/* String length array */
    int32           fldsz = 0;	/* Record size in bytes */

    char           *pntr[VSFIELDMAX];	/* String pointer array */
    char            utlbuf[256];/* Utility Buffer */


    /* Check for valid point ID */
    /* ------------------------ */
    status = PTchkptid(pointID, "PTsizeof", &fid, &sdInterfaceID, &ptVgrpID);

    if (status == 0)
    {
	/* Get number of levels */
	/* -------------------- */
	nlevels = PTnlevels(pointID);


	/* Report error if no levels (vdatas) defined */
	/* ------------------------------------------ */
	if (nlevels == 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "PTsizeof", __FILE__, __LINE__);
	    HEreport("No Levels Defined for point ID: %d\n", pointID);
	}


	/* If no problems ... */
	/* ------------------ */
	if (status == 0)
	{
	    /* Parse field list */
	    /* ---------------- */
	    nflds = EHparsestr(fieldlist, ',', pntr, slen);


	    /* Initialize fldlevels & found arrays */
	    /* ----------------------------------- */
	    for (j = 0; j < nflds; j++)
	    {
		fldlevels[j] = -1;
	    }

	    for (j = 0; j < nflds; j++)
	    {
		found[j] = 0;
	    }


	    /* Loop through all levels in point */
	    /* -------------------------------- */
	    for (i = 0; i < nlevels; i++)
	    {
		/* Get level vdata ID */
		/* ------------------ */
		vdataID = PTXPoint[pointID % idOffset].vdID[i];


		/* Loop through all fields in fieldlist */
		/* ------------------------------------ */
		for (j = 0; j < nflds; j++)
		{
		    /* Copy field entry into utlbuf */
		    /* ---------------------------- */
		    memcpy(utlbuf, pntr[j], slen[j]);
		    utlbuf[slen[j]] = 0;


		    /* If field exists in level and not in a prevous one ... */
		    /* ----------------------------------------------------- */
		    if (VSfexist(vdataID, utlbuf) == 1 && found[j] == 0)
		    {
			/* Increment total field size */
			/* -------------------------- */
			fldsz += VSsizeof(vdataID, utlbuf);

			/* Store field level & set found flag */
			/* ---------------------------------- */
			fldlevels[j] = i;
			found[j] = 1;
		    }
		}
	    }
	}
    }
    return (fldsz);

}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTnfields                                                        |
|                                                                             |
|  DESCRIPTION: Returns number of fields defined in a level.                  |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  nflds                              Number of fields in a level             |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               point structure ID                      |
|  level          int32               level number (0 - based)                |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|  strbufsize     int32               String length of fieldlist              |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Oct 96   Joel Gales    Enlarge field string buffer                         |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
PTnfields(int32 pointID, int32 level, int32 * strbufsize)
{
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           idOffset = PTIDOFFSET;	/* Point ID offset */
    int32           ptVgrpID;	/* Point Vgroup ID */
    int32           nlevels;	/* Number of levels in point */
    int32           nflds;	/* Number of fields in fieldlist */
    int32           vdataID;	/* Vdata ID */

    char            fieldbuf[VSFIELDMAX * FIELDNAMELENMAX];
    /* Vdata fieldname buffer */


    /* Check for valid point ID */
    /* ------------------------ */
    status = PTchkptid(pointID, "PTnfields", &fid, &sdInterfaceID, &ptVgrpID);


    if (status == 0)
    {
	/* Get number of levels */
	/* -------------------- */
	nlevels = PTnlevels(pointID);


	/* Report error if no levels (vdatas) defined */
	/* ------------------------------------------ */
	if (nlevels == 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "PTnfields", __FILE__, __LINE__);
	    HEreport("No Levels Defined for point ID: %d\n", pointID);
	}
	else if (nlevels < level)
	{
	    /* Report error if level # to large */
	    /* -------------------------------- */
	    status = -1;
	    HEpush(DFE_GENAPP, "PTnfields", __FILE__, __LINE__);
	    HEreport("Only %d levels Defined for point ID: %d\n",
		     nlevels, pointID);
	}


	if (status == 0)
	{
	    /* Get level vdata ID */
	    /* ------------------ */
	    vdataID = PTXPoint[pointID % idOffset].vdID[level];


	    /* Get number of fields in level & fieldlist */
	    /* ----------------------------------------- */
	    nflds = VSgetfields(vdataID, fieldbuf);


	    /* Return fieldlist string size if requested */
	    /* ----------------------------------------- */
	    if (strbufsize != NULL)
	    {
		*strbufsize = strlen(fieldbuf);
	    }

	}
    }
    return (nflds);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTlevelindx                                                      |
|                                                                             |
|  DESCRIPTION: Returns index number for a named level.                       |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  n              int32               Level number (0 - based)                |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               point structure ID                      |
|  levelname      char                point level name                        |
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
PTlevelindx(int32 pointID, char *levelname)
{
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           idOffset = PTIDOFFSET;	/* Point ID offset */
    int32           ptVgrpID;	/* Point Vgroup ID */
    int32           nlevels;	/* Number of levels in point */
    int32           level = -1;	/* Level corresponding to levelname */

    char            name[VSNAMELENMAX];	/* Level name */


    /* Check for valid point id */
    /* ------------------------ */
    status = PTchkptid(pointID, "PTlevelindx",
		       &fid, &sdInterfaceID, &ptVgrpID);


    if (status == 0)
    {
	/* Get number of levels */
	/* -------------------- */
	nlevels = PTnlevels(pointID);


	/* Loop through all levels in point */
	/* -------------------------------- */
	for (level = 0; level < nlevels; level++)
	{
	    /* Get level name */
	    /* -------------- */
	    VSgetname(PTXPoint[pointID % idOffset].vdID[level], name);


	    /* If it matches input levelname then exit loop */
	    /* -------------------------------------------- */
	    if (strcmp(name, levelname) == 0)
	    {
		break;
	    }
	}
    }


    /* Levelname not found so set error status */
    /* --------------------------------------- */
    if (level == nlevels)
    {
	level = -1;
    }

    return (level);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTgetlevelname                                                   |
|                                                                             |
|  DESCRIPTION: Returns level name                                            |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  nflds                              Number of fields in a level             |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               point structure ID                      |
|  level          int32               level number (0 - based)                |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|  levelname      char                Level name                              |
|  strbufsize     int32               String length of fieldlist              |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Dec 96   Paul Harten   Original Programmer                                 |
|  Dec 96   Joel Gales    Modify to comform with coding standards             |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
PTgetlevelname(int32 pointID, int32 level, char *levelname, int32 * strbufsize)
{
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           idOffset = PTIDOFFSET;	/* Point ID offset */
    int32           ptVgrpID;	/* Point Vgroup ID */
    int32           nlevels;	/* Number of levels in point */

    char            name[VSNAMELENMAX];	/* Level name */


    /* Check for valid point id */
    /* ------------------------ */
    status = PTchkptid(pointID, "PTgetlevelname",
		       &fid, &sdInterfaceID, &ptVgrpID);


    if (status == 0)
    {
	/* Get number of levels */
	/* -------------------- */
	nlevels = PTnlevels(pointID);


	/* Report error if no levels (vdatas) defined */
	/* ------------------------------------------ */
	if (nlevels == 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "PTgetlevelname", __FILE__, __LINE__);
	    HEreport("No Levels Defined for point ID: %d\n", pointID);
	}
	else if (nlevels < level)
	{
	    /* Report error if level # to large */
	    /* -------------------------------- */
	    status = -1;
	    HEpush(DFE_GENAPP, "PTgetlevelname", __FILE__, __LINE__);
	    HEreport("Only %d levels Defined for point ID: %d\n",
		     nlevels, pointID);
	}


	if (status == 0)
	{
	    /* Get level name */
	    /* -------------- */
	    VSgetname(PTXPoint[pointID % idOffset].vdID[level], name);


	    /* Return name string length */
	    /* ------------------------- */
	    *strbufsize = strlen(name);


	    /* Return levelname if requested */
	    /* ----------------------------- */
	    if (levelname != NULL)
	    {
		strcpy(levelname, name);
	    }
	}
    }
    return (status);
}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTattach                                                         |
|                                                                             |
|  DESCRIPTION: Attaches to an existing point data set.                       |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  pointID        int32               point structure ID                      |
|                                                                             |
|  INPUTS:                                                                    |
|  fid            int32               HDF-EOS file ID                         |
|  pointname      char                point structure name                    |
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
|  Aug 96   Joel Gales    Initalize pointID to -1                             |
|  Sep 96   Joel Gales    File ID in EHgetid changed from HDFEOS file id to   |
|                         HDF file id                                         |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
PTattach(int32 fid, char *pointname)

{
    intn            i;		/* Loop index */
    intn            j;		/* Loop index */
    intn            npointopen = 0;	/* # of point structures open */
    intn            status = -1;/* routine return status variable */

    uint8           acs;	/* Read/Write file access code */

    int32           HDFfid;	/* HDF file id */
    int32           vgRef;	/* Vgroup reference number */
    int32           vgid[4];	/* Vgroup ID array */
    int32           pointID = -1;	/* HDF-EOS point ID */
    int32          *tags;	/* Pnt to Vgroup object tags array */
    int32          *refs;	/* Pnt to Vgroup object refs array */
    int32           dum;	/* dummy varible */
    int32           nlevels;	/* Number of levels in point */
    int32           vgidData;	/* Point data Vgroup ID */
    int32           tag;	/* Point Vdata tag */
    int32           ref;	/* Point Vdata ref */
    int32           idOffset = PTIDOFFSET;	/* Point ID offset */

    char            name[80];	/* Vgroup name */
    char            class[80];	/* Vgroup class */
    char            errbuf[256];/* Buffer for error message */
    char            acsCode[1];	/* Read/Write access char: "r/w" */




    /* Check HDF-EOS file ID, get back HDF file ID and access code */
    /* ----------------------------------------------------------- */
    status = EHchkfid(fid, pointname, &HDFfid, &dum, &acs);


    if (status == 0)
    {
	/* Convert numeric access code to character */
	/* ---------------------------------------- */
	acsCode[0] = (acs == 1) ? 'w' : 'r';

	/* Determine number of points currently opened */
	/* ------------------------------------------- */
	for (i = 0; i < NPOINT; i++)
	{
	    npointopen += PTXPoint[i].active;
	}

	/* If room for more ... */
	/* -------------------- */
	if (npointopen < NPOINT)
	{

	    /* Search Vgroups for Point */
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
		vgid[0] = Vattach(HDFfid, vgRef, acsCode);
		Vgetname(vgid[0], name);
		Vgetclass(vgid[0], class);


		/* If point found get vgroup & vdata ids */
		/* ------------------------------------- */
		if (strcmp(name, pointname) == 0 &&
		    strcmp(class, "POINT") == 0)
		{
		    status = 0;

		    /* Attach to Point Vgroups (Skip 1st entry (Vdata)) */
		    /* ------------------------------------------------ */
		    tags = (int32 *) malloc(sizeof(int32) * 4);
		    if(tags == NULL)
		    { 
			HEpush(DFE_NOSPACE,"PTattach", __FILE__, __LINE__);
			return(-1);
		    }
		    refs = (int32 *) malloc(sizeof(int32) * 4);
		    if(refs == NULL)
		    { 
			HEpush(DFE_NOSPACE,"PTattach", __FILE__, __LINE__);
			free(tags);
			return(-1);
		    }
		    Vgettagrefs(vgid[0], tags, refs, 4);
		    vgid[1] = Vattach(HDFfid, refs[1], acsCode);
		    vgid[2] = Vattach(HDFfid, refs[2], acsCode);
		    vgid[3] = Vattach(HDFfid, refs[3], acsCode);
		    free(tags);
		    free(refs);


		    /* Setup External Arrays */
		    /* --------------------- */
		    for (i = 0; i < NPOINT; i++)
		    {
			/* Find empty entry in array */
			/* ------------------------- */
			if (PTXPoint[i].active == 0)
			{
			    /*
			     * Set pointID, Set point entry active, Store
			     * root Vgroup ID, Store sub Vgroup IDs, Store
			     * HDF-EOS file ID.  Get number of levels.
			     */
			    pointID = i + idOffset;
			    PTXPoint[i].active = 1;
			    PTXPoint[i].IDTable = vgid[0];
			    PTXPoint[i].VIDTable[0] = vgid[1];
			    PTXPoint[i].VIDTable[1] = vgid[2];
			    PTXPoint[i].VIDTable[2] = vgid[3];
			    PTXPoint[i].fid = fid;
			    vgidData = vgid[1];
			    nlevels = Vntagrefs(vgidData);


			    /* Attach & Store level Vdata IDs */
			    /* ------------------------------ */
			    for (j = 0; j < nlevels; j++)
			    {
				Vgettagref(vgidData, j, &tag, &ref);
				PTXPoint[i].vdID[j] =
				    VSattach(HDFfid, ref, acsCode);

			    }
			    break;
			}
		    }
		    break;
		}
		/* Detach Vgroup if not desired Swath */
		/* ---------------------------------- */
		Vdetach(vgid[0]);

	    }

	    /* If Point not found then set up error message */
	    /* -------------------------------------------- */
	    if (status == -1)
	    {
		pointID = -1;
		strcpy(errbuf, "Point: \"%s\" does not exist ");
		strcat(errbuf, "within HDF file.\n");
		HEpush(DFE_RANGE, "PTattach", __FILE__, __LINE__);
		HEreport(errbuf, pointname);
	    }
	}
	else
	{
	    /* Too many files opened */
	    /* --------------------- */
	    status = -1;
	    pointID = -1;
	    strcpy(errbuf,
		   "No more than %d points may be open simutaneously");
	    strcat(errbuf, " (%s)");
	    HEpush(DFE_DENIED, "PTattach", __FILE__, __LINE__);
	    HEreport(errbuf, NPOINT, pointname);
	}
    }
    return (pointID);
}

/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTdeflevel                                                       |
|                                                                             |
|  DESCRIPTION: Defines a level within the point data set.                    |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               point structure ID                      |
|  levelname      char                name of level                           |
|  fieldlist      char                list of level fields (comma-separated)  |
|  fieldtype      int32               array of field types                    |
|  fieldorder     int32               array of field orders                   |
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
|  Aug 96   Joel Gales    Check level and field names for length              |
|  May 00   Abe Taaheri   Added few lines to check for errors in HDF functions|
|                         VSfdefine, VSsetfields, and VSsizeof                |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
PTdeflevel(int32 pointID, char *levelname,
	   char *fieldlist, int32 fieldtype[], int32 fieldorder[])


{
    intn            i;		/* Loop index */
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           idOffset = PTIDOFFSET;	/* Point ID offset */
    int32           ptVgrpID;	/* Point Vgroup ID */
    int32           nlevels;	/* Number of levels in point */
    int32           pID;	/* Point ID - offset */
    int32           nfields;	/* Number of fields in fieldlist */
    int32           vgid;	/* Vgroup ID */
    int32           vdataID;	/* Vdata ID */
    int32           order;	/* Field order */
    int32           slen[VSFIELDMAX];	/* String length array */
    int32           size;	/* Record size in bytes */
    int32           metadata[2];/* Metadata input array */
    int32           m1 = -1;	/* Minus one (fill value) */
    int32           dum;	/* Dummy variable */

    char            pointname[80];	/* Point name */
    char            utlbuf[256];/* Utility buffer */
    char           *zerobuf;	/* Pointer to zero (initial) Vdata record
				 * buffer */
    char           *pntr[VSFIELDMAX];	/* String pointer array */


    /* Check for valid point ID */
    /* ------------------------ */
    status = PTchkptid(pointID, "PTdeflevel", &fid, &sdInterfaceID, &ptVgrpID);


    /* Check levelname for length */
    /* -------------------------- */
    if ((intn) strlen(levelname) > VSNAMELENMAX)
    {
	status = -1;
	HEpush(DFE_GENAPP, "PTdeflevel", __FILE__, __LINE__);
	HEreport("Levelname \"%s\" more than %d characters.\n",
		 levelname, VSNAMELENMAX);
    }


    if (status == 0)
    {
	/* Compute "reduce" point ID */
	/* ------------------------- */
	pID = pointID % idOffset;


	/* Parse field list */
	/* ---------------- */
	nfields = EHparsestr(fieldlist, ',', pntr, slen);


	/* Loop through all entries in fieldlist */
	/* ------------------------------------- */
	for (i = 0; i < nfields; i++)
	{

	    /* Check for empty fields */
	    /* ---------------------- */
	    if (slen[i] == 0)
	    {
		status = -1;
		HEpush(DFE_GENAPP, "PTdeflevel", __FILE__, __LINE__);
		HEreport("\"Empty\" field in fieldlist: %s.\n",
			 fieldlist);
		break;
	    }


	    /* Check fieldname for length */
	    /* -------------------------- */
	    memcpy(utlbuf, pntr[i], slen[i]);
	    utlbuf[slen[i]] = 0;
	    if (slen[i] > FIELDNAMELENMAX)
	    {
		status = -1;
		HEpush(DFE_GENAPP, "PTdeflevel", __FILE__, __LINE__);
		HEreport("Fieldname \"%s\" more than %d characters.\n",
			 utlbuf, FIELDNAMELENMAX);
	    }
	}


	/* If no problems proceed ... */
	/* -------------------------- */
	if (status == 0)
	{
	    /* Get Data Vgroup ID */
	    /* ------------------ */
	    vgid = PTXPoint[pID].VIDTable[0];


	    /* Get number of levels in point */
	    /* ----------------------------- */
	    nlevels = Vntagrefs(vgid);


	    /* Get new vdata ID */
	    /* ---------------- */
	    vdataID = VSattach(fid, -1, "w");


	    /* Store Vdata ID in external array */
	    /* -------------------------------- */
	    PTXPoint[pID].vdID[nlevels] = vdataID;


	    /* For all fields in fieldlist ... */
	    /* ------------------------------- */
	    for (i = 0; i < nfields; i++)
	    {
		/* Copy fieldname into utlbuf */
		/* -------------------------- */
		memcpy(utlbuf, pntr[i], slen[i]);
		utlbuf[slen[i]] = 0;


		/* Get field order (change order = 0 to order = 1) */
		/* ----------------------------------------------- */
		order = fieldorder[i];
		if (order == 0)
		{
		    order = 1;
		}


		/* Define the field within vdata */
		/* ----------------------------- */
		status = VSfdefine(vdataID, utlbuf, fieldtype[i], order);
		if(status != 0)
		{
		    status = -1;
		    HEpush(DFE_GENAPP, "PTdeflevel", __FILE__, __LINE__);
		    HEreport("Cannot define %d th field. One probable cause can be exceeding of HDF's limits for MAX_ORDER and/or MAX_FIELD_SIZE \"%d\".\n",i+1,MAX_ORDER);
		    break;
		}
	    }

	    if(status == 0)
	    {
		/* Set all fields within vdata */
		/* --------------------------- */
		status = VSsetfields(vdataID, fieldlist);
		if(status != 0)
		{
		    status = -1;
		    HEpush(DFE_GENAPP, "PTdeflevel", __FILE__, __LINE__);
		    HEreport("Cannot set fields. Probably exceeded HDF's limit MAX_FIELD_SIZE \"%d\" for the fields.\n",MAX_FIELD_SIZE);
		}  
	    }

	    if(status == 0)
	    {
		/* Get size in bytes of vdata record */
		/* --------------------------------- */
		size = VSsizeof(vdataID, fieldlist);
		if(size <= 0)
		{
		    status = -1;
		    HEpush(DFE_GENAPP, "PTdeflevel", __FILE__, __LINE__);
		    HEreport("Size of Vdata is not greater than zero.\n");
		}  
	    }
 
	    if(status == 0)
	    {
		/* Write out empty buffer to establish vdata */
		/* ----------------------------------------- */
		zerobuf = (char *) calloc(size, 1);
		if(zerobuf == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"PTdeflevel", __FILE__, __LINE__);
		    return(-1);
		}
		VSwrite(vdataID, (VOIDP) zerobuf, 1, FULL_INTERLACE);
		free(zerobuf);
		
		
		/* Set name of vdata to levelname */
		/* ------------------------------ */
		VSsetname(vdataID, levelname);
		
		
		/* Insert within data Vgroup */
		/* ------------------------- */
		Vinsert(vgid, vdataID);
		
		
		
		/* Setup Back & Forward Pointer Vdatas */
		/* ----------------------------------- */
		
		/* If previous levels exist ... */
		/* ---------------------------- */
		if (nlevels > 0)
		{
		    /* Get Vgroup ID of Linkage Vgroup */
		    /* ------------------------------- */
		    vgid = PTXPoint[pID].VIDTable[1];
		    
		    
		    /* Get new vdata ID for BCKPOINTER Vdata */
		    /* ------------------------------------- */
		    vdataID = VSattach(fid, -1, "w");
		    
		    
		    /* Define & set BCKPOINTER field within BCKPOINTER Vdata */
		    /* ----------------------------------------------------- */
		    VSfdefine(vdataID, "BCKPOINTER", DFNT_INT32, 1);
		    VSsetfields(vdataID, "BCKPOINTER");
		    
		    
		    /* Get size in bytes of BCKPOINTER record */
		    /* -------------------------------------- */
		    size = VSsizeof(vdataID, "BCKPOINTER");
		    
		    
		    /* Write out empty buffer to establish vdata */
		    /* ----------------------------------------- */
		    zerobuf = (char *) calloc(size, 1);
		    if(zerobuf == NULL)
		    { 
			HEpush(DFE_NOSPACE,"PTdeflevel", __FILE__, __LINE__);
			return(-1);
		    }
		    VSwrite(vdataID, (VOIDP) zerobuf, 1, FULL_INTERLACE);
		    free(zerobuf);
		    
		    
		    /* Set name of BCKPOINTER Vdata */
		    /* ---------------------------- */
		    sprintf(utlbuf, "%s%d%s%d", "BCKPOINTER:", nlevels,
			    "->", nlevels - 1);
		    VSsetname(vdataID, utlbuf);
		    
		    
		    /* Insert BCKPOINTER Vdata in Linkage Vgroup */
		    /* ----------------------------------------- */
		    Vinsert(vgid, vdataID);
		    
		    
		    /* Detach BCKPOINTER Vdata */
		    /* ----------------------- */
		    VSdetach(vdataID);
		    
		    
		    /* Get new vdata ID for FWDPOINTER Vdata */
		    /* ------------------------------------- */
		    vdataID = VSattach(fid, -1, "w");
		    
		    
		    /* Define & set BEGIN & EXTENT field within FWDPOINTER
		       Vdata */
		    /* ------------------------------------------------- */
		    VSfdefine(vdataID, "BEGIN", DFNT_INT32, 1);
		    VSfdefine(vdataID, "EXTENT", DFNT_INT32, 1);
		    VSsetfields(vdataID, "BEGIN,EXTENT");
		    
		    
		    /* Get size in bytes of FWDPOINTER record */
		    /* -------------------------------------- */
		    size = VSsizeof(vdataID, "BEGIN,EXTENT");
		    
		    
		    /* Write out buffer (with -1 fill value) to establish 
		       vdata */
		    /* ------------------------------------------------- */
		    zerobuf = (char *) calloc(size, 1);
		    if(zerobuf == NULL)
		    { 
			HEpush(DFE_NOSPACE,"PTdeflevel", __FILE__, __LINE__);
			return(-1);
		    }
		    memcpy(zerobuf, &m1, 4);
		    VSwrite(vdataID, (VOIDP) zerobuf, 1, FULL_INTERLACE);
		    free(zerobuf);
		    
		    
		    /* Set name of FWDPOINTER Vdata */
		    /* ---------------------------- */
		    sprintf(utlbuf, "%s%d%s%d", "FWDPOINTER:", nlevels - 1,
			    "->", nlevels);
		    VSsetname(vdataID, utlbuf);
		    
		    
		    /* Insert FWDPOINTER Vdata in Linkage Vgroup */
		    /* ----------------------------------------- */
		    Vinsert(vgid, vdataID);
		    
		    
		    /* Detach FWDPOINTER Vdata */
		    /* ----------------------- */
		    VSdetach(vdataID);
		}
	    }
	}


	if (status == 0)
	{
	    /* Insert Point Level metadata */
	    /* --------------------------- */
	    Vgetname(PTXPoint[pointID % idOffset].IDTable, pointname);
	    status = EHinsertmeta(sdInterfaceID, pointname, "p", 10L,
				  levelname, &dum);


	    for (i = 0; i < nfields; i++)
	    {
		/* Concatenate fieldname with level name */
		/* ------------------------------------- */
		memcpy(utlbuf, pntr[i], slen[i]);
		utlbuf[slen[i]] = 0;
		strcat(utlbuf, ":");
		strcat(utlbuf, levelname);


		/* Get field order (change order = 0 to order = 1) */
		/* ----------------------------------------------- */
		order = fieldorder[i];
		if (order == 0)
		{
		    order = 1;
		}


		/* Load fieldtype and field order into metadata input array */
		/* -------------------------------------------------------- */
		metadata[0] = fieldtype[i];
		metadata[1] = order;


		/* Insert point field metadata */
		/* --------------------------- */
		status = EHinsertmeta(sdInterfaceID, pointname, "p", 11L,
				      utlbuf, metadata);

	    }

	}

    }
    return (status);
}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTdeflinkage                                                     |
|                                                                             |
|  DESCRIPTION: Defines link field to use between two levels.                 |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               point structure ID                      |
|  parent         char                parent level name                       |
|  child          char                child level name                        |
|  linkfield      char                linkage field name                      |
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
|  Nov 96   Joel Gales    Fix problem with spurious "Level Not Found" error   |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
PTdeflinkage(int32 pointID, char *parent, char *child, char *linkfield)
{
    intn            i;		/* Loop index */
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           idOffset = PTIDOFFSET;	/* Point ID offset */
    int32           ptVgrpID;	/* Point Vgroup ID */
    int32           nlevels;	/* Number of levels in point */
    int32           vdataID;	/* Vdata ID */
    int32           dum;	/* Dummy variable */
    int32           foundParent = -1;	/* Found parent level flag */
    int32           foundChild = -1;	/* Found child level flag */

    char            pointname[80];	/* Point name */
    char            utlbuf[256];/* Utility buffer */
    char           *mess1 =
    "Linkage Field \"%s\" not found in Parent Level: \"%s\".\n";
    char           *mess2 =
    "Linkage Field \"%s\" not found in Child Level: \"%s\".\n";


    /* Check for valid point ID */
    /* ------------------------ */
    status = PTchkptid(pointID, "PTdeflinkage",
		       &fid, &sdInterfaceID, &ptVgrpID);


    if (status == 0)
    {
	/* Get number of levels in point */
	/* ----------------------------- */
	nlevels = PTnlevels(pointID);


	/* For all levels ... */
	/* ------------------ */
	for (i = 0; i < nlevels; i++)
	{
	    /* Get level Vdata ID and name */
	    /* -------------------------- */
	    vdataID = PTXPoint[pointID % idOffset].vdID[i];
	    VSgetname(vdataID, utlbuf);


	    /* If equal to parent level name ... */
	    /* --------------------------------- */
	    if (strcmp(utlbuf, parent) == 0)
	    {
		/* Set found parent flag */
		/* --------------------- */
		foundParent = i;


		/* If linkfield exists in parent level then break ... */
		/* -------------------------------------------------- */
		if (VSfexist(vdataID, linkfield) != -1)
		{
		    break;
		}
		else
		{
		    /* ... else report error */
		    /* --------------------- */
		    status = -1;
		    HEpush(DFE_GENAPP, "PTdeflinkage", __FILE__, __LINE__);
		    HEreport(mess1, linkfield, parent);
		    break;
		}
	    }
	}


	/* For all levels ... */
	/* ------------------ */
	for (i = 0; i < nlevels; i++)
	{
	    /* Get level Vdata ID and name */
	    /* -------------------------- */
	    vdataID = PTXPoint[pointID % idOffset].vdID[i];
	    VSgetname(vdataID, utlbuf);


	    /* If equal to child level name ... */
	    /* -------------------------------- */
	    if (strcmp(utlbuf, child) == 0)
	    {
		/* Set found child flag */
		/* -------------------- */
		foundChild = i;


		/* If linkfield exists in child level then break ... */
		/* ------------------------------------------------- */
		if (VSfexist(vdataID, linkfield) != -1)
		{
		    break;
		}
		else
		{
		    /* ... else report error */
		    /* --------------------- */
		    status = -1;
		    HEpush(DFE_GENAPP, "PTdeflinkage", __FILE__, __LINE__);
		    HEreport(mess2, linkfield, child);
		    break;
		}
	    }
	}


	/* Report parent level not found if relevant */
	/* ----------------------------------------- */
	if (foundParent == -1)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "PTdeflinkage", __FILE__, __LINE__);
	    HEreport("Parent Level: \"%s\" not found.\n", parent);
	}


	/* Report child level not found if relevant */
	/* ---------------------------------------- */
	if (foundChild == -1)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "PTdeflinkage", __FILE__, __LINE__);
	    HEreport("Child Level: \"%s\" not found.\n", child);
	}



	/* Check that parent and child levels are adjacent */
	/* ----------------------------------------------- */
	if (foundParent != -1 && foundChild != -1 &&
	    foundChild - foundParent != 1)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "PTdeflinkage", __FILE__, __LINE__);
	    HEreport("Parent/Child Levels not adjacent \"%s/%s\".\n",
		     parent, child);
	}


	/* If no problems ... */
	/* ------------------ */
	if (status == 0)
	{
	    /* Insert linkage info in structural metadata */
	    /* ------------------------------------------ */
	    sprintf(utlbuf, "%s%s%s%s%s", parent, "/", child, ":", linkfield);

	    Vgetname(PTXPoint[pointID % idOffset].IDTable, pointname);
	    status = EHinsertmeta(sdInterfaceID, pointname, "p", 12L,
				  utlbuf, &dum);

	}

    }
    return (status);
}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTlinkinfo                                                       |
|                                                                             |
|  DESCRIPTION: Returns ("bck/fwd") linkage info                              |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               point structure ID                      |
|  level          int32               level number (0 - based)                |
|  mode           char                mode ("+/-")                            |
|  linkfield      char                linkage field                           |
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
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
PTlinkinfo(int32 pointID, int32 sdInterfaceID, int32 level,
	   char *mode, char *linkfield)
{
    intn            status = 0;	/* routine return status variable */

    int32           idOffset = PTIDOFFSET;	/* Point ID offset */
    int32           pID;	/* Point ID - offset */

    char           *metabuf;	/* Pointer to structural metadata (SM) */
    char           *metaptrs[2];/* Pointers to begin and end of SM section */
    char            name1[80];	/* Name string 1 */
    char            name2[80];	/* Name string 2 */
    char            utlbuf[256];/* Utility buffer */


    /* Compute "reduced" point ID */
    /* -------------------------- */
    pID = pointID % idOffset;


    /* Get point name */
    /* -------------- */
    Vgetname(PTXPoint[pID].IDTable, name1);


    /* Get level link structural metadata */
    /* ---------------------------------- */
    metabuf = (char *) EHmetagroup(sdInterfaceID, name1, "p",
				   "LevelLink", metaptrs);

	if(metabuf == NULL)
	{
	    return(-1);
	}
    if (strcmp(mode, "-") == 0)
    {
	/* If back link get names of previous & current levels */
	/* --------------------------------------------------- */
	VSgetname(PTXPoint[pID].vdID[level - 1], name1);
	VSgetname(PTXPoint[pID].vdID[level], name2);
    }
    else
    {
	/* If fwd link get names of current & following levels */
	/* --------------------------------------------------- */
	VSgetname(PTXPoint[pID].vdID[level], name1);
	VSgetname(PTXPoint[pID].vdID[level + 1], name2);
    }


    /* Search for parent name entry */
    /* ---------------------------- */
    sprintf(utlbuf, "%s%s", "\t\t\t\tParent=\"", name1);
    metaptrs[0] = strstr(metaptrs[0], utlbuf);


    /* If name found within linkage metadata ... */
    /* ----------------------------------------- */
    if (metaptrs[0] < metaptrs[1] || metaptrs[0] == NULL)
    {
	/* Get Linkage Field */
	/* ----------------- */
	EHgetmetavalue(metaptrs, "LinkField", linkfield);


	/* Remove double quotes */
	/* -------------------- */
	memmove(linkfield, linkfield + 1, strlen(linkfield) - 2);
	linkfield[strlen(linkfield) - 2] = 0;
    }
    else
    {
	/* If not found return error status */
	/* -------------------------------- */
	status = -1;
    }
    free(metabuf);

    return (status);
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTbcklinkinfo                                                    |
|                                                                             |
|  DESCRIPTION: Returns link field to previous level.                         |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               point structure ID                      |
|  level          int32               level number (0 - based)                |
|  linkfield      char                linkage field                           |
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
PTbcklinkinfo(int32 pointID, int32 level, char *linkfield)
{
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           ptVgrpID;	/* Point Vgroup ID */

    char           *mess =
    "No Back Linkage Defined between levels: %d and %d.\n";


    /* Check for valid point ID */
    /* ------------------------ */
    status = PTchkptid(pointID, "PTfwdlinkinfo",
		       &fid, &sdInterfaceID, &ptVgrpID);

    if (status == 0)
    {
	/* Check that level is within bounds for backlink info */
	/* --------------------------------------------------- */
	if (level > 0 && level < PTnlevels(pointID))
	{
	    /* Get linkfield */
	    /* ------------- */
	    status = PTlinkinfo(pointID, sdInterfaceID,
				level, "-", linkfield);
	}
	else
	{
	    /* Report error */
	    /* ------------ */
	    status = -1;
	    HEpush(DFE_GENAPP, "PTbcklinkinfo", __FILE__, __LINE__);
	    HEreport(mess, level, level - 1);
	}
    }
    return (status);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTfwdlinkinfo                                                    |
|                                                                             |
|  DESCRIPTION: Returns link field to following level.                        |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               point structure ID                      |
|  level          int32               level number (0 - based)                |
|  linkfield      char                linkage field                           |
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
PTfwdlinkinfo(int32 pointID, int32 level, char *linkfield)
{
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           ptVgrpID;	/* Point Vgroup ID */

    char           *mess =
    "No Forward Linkage Defined between levels: %d and %d.\n";

    /* Check for valid point ID */
    /* ------------------------ */
    status = PTchkptid(pointID, "PTfwdlinkinfo",
		       &fid, &sdInterfaceID, &ptVgrpID);


    if (status == 0)
    {
	/* Check that level is within bounds for fwdlink info */
	/* -------------------------------------------------- */
	if (level >= 0 && level < PTnlevels(pointID) - 1)
	{
	    /* Get linkfield */
	    /* ------------- */
	    status = PTlinkinfo(pointID, sdInterfaceID,
				level, "+", linkfield);
	}
	else
	{
	    /* Report error */
	    /* ------------ */
	    status = -1;
	    HEpush(DFE_GENAPP, "PTlinkinfo", __FILE__, __LINE__);
	    HEreport(mess, level, level + 1);
	}
    }
    return (status);
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTlevelinfo                                                      |
|                                                                             |
|  DESCRIPTION: Returns information about a given level.                      |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  nflds          int32                                                       |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               point structure ID                      |
|  level          int32               level number (0 - based)                |
|  fieldlist      char                List of fields in point                 |
|  fieldtype      int32               Field type array                        |
|  fieldorder     int32               Field order array                       |
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
PTlevelinfo(int32 pointID, int32 level, char *fieldlist, int32 fieldtype[],
	    int32 fieldorder[])
{
    intn            i;		/* Loop index */
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           ptVgrpID;	/* Point Vgroup ID */
    int32           idOffset = PTIDOFFSET;	/* Point ID offset */

    int32           vdataID;	/* Vdata ID */
    int32           nlevels;	/* Number of levels in point */
    int32           nflds;	/* Number of fields in fieldlist */


    /* Check for valid point ID */
    /* ------------------------ */
    status = PTchkptid(pointID, "PTlevelinfo",
		       &fid, &sdInterfaceID, &ptVgrpID);

    if (status == 0)
    {
	/* Get number of levels in point */
	/* ----------------------------- */
	nlevels = PTnlevels(pointID);


	/* Check for errors */
	/* ---------------- */
	if (nlevels == 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "PTlevelinfo", __FILE__, __LINE__);
	    HEreport("No Levels Defined for point ID: %d\n", pointID);
	}
	else if (nlevels < level)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "PTlevelinfo", __FILE__, __LINE__);
	    HEreport("Only %d levels Defined for point ID: %d\n",
		     nlevels, pointID);
	}


	/* If no problems ... */
	/* ------------------ */
	if (status == 0)
	{
	    /* Get vdata ID of point level */
	    /* --------------------------- */
	    vdataID = PTXPoint[pointID % idOffset].vdID[level];


	    /* Get number of fields and fieldnames */
	    /* ----------------------------------- */
	    nflds = VSgetfields(vdataID, fieldlist);


	    /* Loop through fields and get field type & field order */
	    /* ---------------------------------------------------- */
	    for (i = 0; i < nflds; i++)
	    {
		fieldtype[i] = VFfieldtype(vdataID, i);
		fieldorder[i] = VFfieldorder(vdataID, i);
	    }
	}
    }
    return (nflds);
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTinqpoint                                                       |
|                                                                             |
|  DESCRIPTION: Returns number and names of point structures in file          |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  nPoint         int32               Number of point structures in file      |
|                                                                             |
|  INPUTS:                                                                    |
|  filename       char                HDF-EOS filename                        |
|                                                                             |
|  OUTPUTS:                                                                   |
|  pointlist      char                List of point names (comma-separated)   |
|  strbufsize     int32               Length of pointlist                     |
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
PTinqpoint(char *filename, char *pointlist, int32 * strbufsize)
{
    int32           nPoint;	/* Number of point structures in file */

    /* Call EHinquire */
    /* -------------- */
    nPoint = EHinquire(filename, "POINT", pointlist, strbufsize);

    return (nPoint);
}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTwrbckptr                                                       |
|                                                                             |
|  DESCRIPTION: Writes back pointer records                                   |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               point structure ID                      |
|  level          int32               level index (0 - based)                 |
|  nrec           int32               number of records to read/write         |
|  recs           int32               array of record numbers                 |
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
PTwrbckptr(int32 pointID, int32 level, int32 nrec, int32 recs[])
{
    intn            i;		/* Loop index */
    intn            j;		/* Loop index */
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           idOffset = PTIDOFFSET;	/* Point ID offset */
    int32           ptVgrpID;	/* Point Vgroup ID */
    int32           vgid;	/* Linkage Vgroup ID */
    int32           pID;	/* point ID - offset */
    int32           sz;		/* Size in bytes of record */
    int32           nrecPrev;	/* Number of records in previous level */
    int32           nrecCurr;	/* Number of records in current level */
    int32           vID;	/* Vdata ID */

    char            utlbuf[256];/* Utility buffer */
    char           *bufPrev;	/* Data buffer for previous level */
    char           *bufCurr;	/* Data buffer for current level */
    char           *mess =
    "No Linkage Defined between levels: %d and %d.\n";
    /* Error message */


    /* Check for valid point ID */
    /* ------------------------ */
    status = PTchkptid(pointID, "PTwrbckptr", &fid, &sdInterfaceID, &ptVgrpID);

    if (status == 0)
    {
	/* Get "reduced" point ID */
	/* ---------------------- */
	pID = pointID % idOffset;

	/* BackPointer Section */
	/* ------------------- */
	if (level > 0)
	{
	    /* Get Back-Linkage Field (copy into utlbuf) */
	    /* ----------------------------------------- */
	    status = PTbcklinkinfo(pointID, level, utlbuf);

	    if (status == 0)
	    {
		/* Read Link Field from previous level */
		/* ----------------------------------- */
		vID = PTXPoint[pID].vdID[level - 1];
		VSsetfields(vID, utlbuf);
		nrecPrev = VSelts(vID);
		sz = VSsizeof(vID, utlbuf);
		bufPrev = (char *) calloc(nrecPrev * sz, 1);
		if(bufPrev == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"PTwrbckptr", __FILE__, __LINE__);
		    return(-1);
		}
		VSseek(vID, 0);
		VSread(vID, (uint8 *) bufPrev, nrecPrev, FULL_INTERLACE);


		/* Read Link Field from current level */
		/* ---------------------------------- */
		vID = PTXPoint[pID].vdID[level];
		VSsetfields(vID, utlbuf);
		nrecCurr = VSelts(vID);
		bufCurr = (char *) calloc(nrecCurr * sz, 1);
		if(bufCurr == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"PTwrbckptr", __FILE__, __LINE__);
		    free(bufPrev);
		    return(-1);
		}
		VSseek(vID, 0);
		VSread(vID, (uint8 *) bufCurr, nrecCurr, FULL_INTERLACE);


		/* Get ID of Linkage Vgroup */
		/* ------------------------ */
		vgid = PTXPoint[pID].VIDTable[1];


		/* Get ID of BCKPOINTER vdata */
		/* -------------------------- */
		sprintf(utlbuf, "%s%d%s%d",
			"BCKPOINTER:", level, "->", level - 1);
		vID = EHgetid(fid, vgid, utlbuf, 1, "w");
		VSsetfields(vID, "BCKPOINTER");


		/* Loop through input records */
		/* -------------------------- */
		for (i = 0; i < nrec; i++)
		{
		    /* Loop through records in previous level */
		    /* -------------------------------------- */
		    for (j = 0; j < nrecPrev; j++)
		    {
			/*
			 * If current link field matches link in previous
			 * level, then write record number within previous
			 * level (j).
			 */
			if (memcmp(bufPrev + sz * j,
				   bufCurr + sz * recs[i], sz) == 0)
			{
			    VSseek(vID, recs[i]);
			    VSwrite(vID, (uint8 *) & j, 1, FULL_INTERLACE);
			    break;
			}
		    }
		}

		/* Detach BCKPOINTER vdata */
		/* ----------------------- */
		VSdetach(vID);

		free(bufPrev);
		free(bufCurr);
	    }
	    else
	    {
		/* Report no linkage between levels error */
		/* -------------------------------------- */
		status = -1;
		HEpush(DFE_GENAPP, "PTwrbckptr", __FILE__, __LINE__);
		HEreport(mess, level, level - 1);
	    }
	}
    }
    return (status);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTrdbckptr                                                       |
|                                                                             |
|  DESCRIPTION: Reads back pointers                                           |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               point structure ID                      |
|  level          int32               level index (0 - based)                 |
|  nrec           int32               number of records to read/write         |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|  recs           int32               array of record numbers                 |
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
PTrdbckptr(int32 pointID, int32 level, int32 nrec, int32 recs[])
{
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           idOffset = PTIDOFFSET;	/* Point ID offset */
    int32           ptVgrpID;	/* Point Vgroup ID */
    int32           vgid;	/* Linkage Vgroup ID */
    int32           pID;	/* point ID - offset */
    int32           vID;	/* Vdata ID */

    char            utlbuf[256];/* Utility buffer */
    char           *mess =
    "No Linkage Defined between levels: %d and %d.\n";
    /* Error message */


    /* Check for valid point ID */
    /* ------------------------ */
    status = PTchkptid(pointID, "PTrdbckptr", &fid, &sdInterfaceID, &ptVgrpID);


    if (status == 0)
    {
	/* Get "reduced" point ID */
	/* ---------------------- */
	pID = pointID % idOffset;


	/* BackPointer Section */
	/* ------------------- */
	if (level > 0)
	{
	    /* Get Back-Linkage Field */
	    /* ---------------------- */
	    status = PTbcklinkinfo(pointID, level, utlbuf);


	    if (status == 0)
	    {
		/* Get ID of Linkage Vgroup */
		/* ------------------------ */
		vgid = PTXPoint[pID].VIDTable[1];


		/* Get ID of BCKPOINTER vdata */
		/* -------------------------- */
		sprintf(utlbuf, "%s%d%s%d",
			"BCKPOINTER:", level, "->", level - 1);
		vID = EHgetid(fid, vgid, utlbuf, 1, "r");
		VSsetfields(vID, "BCKPOINTER");


		/* Read in BCKPOINTER records */
		/* -------------------------- */
		VSseek(vID, 0);
		VSread(vID, (uint8 *) recs, nrec, FULL_INTERLACE);


		/* Detach BCKPOINTER vdata */
		/* ----------------------- */
		VSdetach(vID);
	    }
	    else
	    {
		/* Report no linkage between levels error */
		/* -------------------------------------- */
		status = -1;
		HEpush(DFE_GENAPP, "PTrdbckptr", __FILE__, __LINE__);
		HEreport(mess, level, level - 1);
	    }
	}
    }
    return (status);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTwrfwdptr                                                       |
|                                                                             |
|  DESCRIPTION: Write forward pointer records                                 |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               point structure ID                      |
|  level          int32               level index (0 - based)                 |
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
PTwrfwdptr(int32 pointID, int32 level)

{
    intn            i;		/* Loop index */
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           idOffset = PTIDOFFSET;	/* Point ID offset */
    int32           ptVgrpID;	/* Point Vgroup ID */
    int32           vgid;	/* Linkage Vgroup ID */
    int32           vIDFwd;	/* FWDPOINTER vdata ID */
    int32           nrec;	/* Number of records in FWDPOINTER vdata */
    int32          *recs;	/* Values of records in FWDPOINTER vdata */
    int32           fwd;	/* Forward pointer flag */
    int32           nlevels;	/* Number of levels in point */
    int32          *fwdBuf0;	/* Forward pointer buffer 0 */
    int32          *fwdBuf1;	/* Forward pointer buffer 1 */
    int32           max;	/* Maximun back pointer value */
    int32           begExt[2];	/* Begin & extent array */

    char            utlbuf[32];	/* Utility buffer */


    /* Check for valid point ID */
    /* ------------------------ */
    status = PTchkptid(pointID, "PTwrfwdptr", &fid, &sdInterfaceID, &ptVgrpID);

    if (status == 0)
    {
	/* Get number of levels in point */
	/* ----------------------------- */
	nlevels = PTnlevels(pointID);


	/* Get number of records in following level */
	/* ---------------------------------------- */
	nrec = (level < nlevels - 1) ? PTnrecs(pointID, level + 1) : -1;


	/* If records exist in current and following level... */
	/* -------------------------------------------------- */
	if (PTnrecs(pointID, level) > 0 && nrec > 0)
	{
	    /* Read back pointer records from following level */
	    /* ---------------------------------------------- */
	    recs = (int32 *) calloc(nrec, 4);
	    if(recs == NULL)
	    { 
		HEpush(DFE_NOSPACE,"PTwrfwdptr", __FILE__, __LINE__);
		return(-1);
	    }
	    status = PTrdbckptr(pointID, level + 1, nrec, recs);


	    /* Get ID of Linkage Vgroup */
	    /* ------------------------ */
	    vgid = PTXPoint[pointID % idOffset].VIDTable[1];


	    /* Get ID of FWDPOINTER vdata */
	    /* -------------------------- */
	    sprintf(utlbuf, "%s%d%s%d",
		    "FWDPOINTER:", level, "->", level + 1);
	    vIDFwd = EHgetid(fid, vgid, utlbuf, 1, "w");
	    VSsetfields(vIDFwd, "BEGIN,EXTENT");


	    /* Find Max BackPointer value */
	    /* -------------------------- */
	    max = recs[0];
	    for (i = 1; i < nrec; i++)
	    {
		if (recs[i] > max)
		{
		    max = recs[i];
		}
	    }


	    /* Fill Fwd Ptr buffers with -1 */
	    /* ---------------------------- */
	    fwdBuf0 = (int32 *) calloc(max + 1, 4);
	    if(fwdBuf0 == NULL)
	    { 
		HEpush(DFE_NOSPACE,"PTwrfwdptr", __FILE__, __LINE__);
		return(-1);
	    }
	    fwdBuf1 = (int32 *) calloc(max + 1, 4);
	    if(fwdBuf1 == NULL)
	    { 
		HEpush(DFE_NOSPACE,"PTwrfwdptr", __FILE__, __LINE__);
		free(fwdBuf0);
		return(-1);
	    }
	    for (i = 0; i <= max; i++)
	    {
		fwdBuf0[i] = -1;
		fwdBuf1[i] = -1;
	    }


	    /* Set forward pointer flag to 1 */
	    /* ----------------------------- */
	    fwd = 1;


	    /* Loop through all records */
	    /* ------------------------ */
	    for (i = 0; i < nrec; i++)
	    {
		/* If fwdBuf1 entry not yet written for rec[i] ... */
		/* ----------------------------------------------- */
		if (fwdBuf1[recs[i]] == -1)
		{
		    /* Set Buf0 to (possible) beginning of sequence */
		    /* -------------------------------------------- */
		    fwdBuf0[recs[i]] = i;


		    /* Set Buf1 to initial value of sequence */
		    /* ------------------------------------- */
		    fwdBuf1[recs[i]] = i;
		}
		else
		{
		    /* If numbers in sequence ... */
		    /* -------------------------- */
		    if (i - fwdBuf1[recs[i]] == 1)
		    {
			/* Set Buf1 to current value of sequence */
			/* ------------------------------------- */
			fwdBuf1[recs[i]] = i;
		    }
		    else
		    {
			/* Back pointers in following level not monotonic */
			/* ---------------------------------------------- */


			/* Set begin begExt[0] and extent begExt[1] to -1 */
			/* -------------------------------------- */
			begExt[0] = -1;
			begExt[1] = -1;


			/* Write begin/extent values to first (0th) record */
			/* ----------------------------------------------- */
			VSseek(vIDFwd, 0);
			VSwrite(vIDFwd, (uint8 *) begExt, 1, FULL_INTERLACE);


			/* Set forward pointer flag to 0 */
			/* ----------------------------- */
			fwd = 0;
			break;
		    }
		}
	    }



	    /* Back pointers in following level are monotonic */
	    /* ---------------------------------------------- */
	    if (fwd == 1)
	    {
		/* Write begin & entent for each record in current level */
		/* ----------------------------------------------------- */
		for (i = 0; i <= max; i++)
		{
		    begExt[0] = fwdBuf0[i];
		    begExt[1] = fwdBuf1[i] - fwdBuf0[i] + 1;

		    VSseek(vIDFwd, i);
		    VSwrite(vIDFwd, (uint8 *) begExt, 1, FULL_INTERLACE);
		}

	    }
	    free(fwdBuf0);
	    free(fwdBuf1);
	    free(recs);
	    VSdetach(vIDFwd);
	}
    }
    return (status);
}






/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTrdfwdptr                                                       |
|                                                                             |
|  DESCRIPTION: Read forward pointer records                                  |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               point structure ID                      |
|  level          int32               level index (0 - based)                 |
|  nrec           int32               number of records to read/write         |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|  recs           int32               array of record numbers                 |
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
PTrdfwdptr(int32 pointID, int32 level, int32 nrec, int32 recs[])
{
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           idOffset = PTIDOFFSET;	/* Point ID offset */
    int32           ptVgrpID;	/* Point Vgroup ID */
    int32           vgid;	/* Linkage Vgroup ID */
    int32           vIDFwd;	/* FWDPOINTER vdata ID */

    char            utlbuf[32];	/* Utility buffer */


    /* Check for valid point ID */
    /* ------------------------ */
    status = PTchkptid(pointID, "PTrdfwdptr", &fid, &sdInterfaceID, &ptVgrpID);


    if (status == 0)
    {
	/* Get ID of Linkage Vgroup */
	/* ------------------------ */
	vgid = PTXPoint[pointID % idOffset].VIDTable[1];


	/* Get ID of FWDPOINTER vdata */
	/* -------------------------- */
	sprintf(utlbuf, "%s%d%s%d", "FWDPOINTER:", level,
		"->", level + 1);
	vIDFwd = EHgetid(fid, vgid, utlbuf, 1, "r");


	/* Read BEGIN & EXTENT fields in first record */
	/* ------------------------------------------ */
	VSsetfields(vIDFwd, "BEGIN,EXTENT");
	VSseek(vIDFwd, 0);
	VSread(vIDFwd, (uint8 *) recs, 1, FULL_INTERLACE);


	/* If -1 then no forward pointers exist */
	/* ------------------------------------ */
	if (recs[0] == -1)
	{
	    status = -1;
	}
	else
	{
	    /* Read BEGIN & EXTENT fields for all records */
	    /* ------------------------------------------ */
	    VSseek(vIDFwd, 0);
	    VSread(vIDFwd, (uint8 *) recs, nrec, FULL_INTERLACE);
	}

	VSdetach(vIDFwd);
    }
    return (status);
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTwritesetup                                                     |
|                                                                             |
|  DESCRIPTION:                                                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  fid            int32               HDFEOS file ID                          |
|  ptVgrpID       int32               Point Vgroup ID                         |
|  vdataID        int32               vdata ID                                |
|  level          int32               level number (0 - based)                |
|  fieldlist      char                fieldlist to write (comma-separated)    |
|                                                                             |
|  OUTPUTS:                                                                   |
|  sz             int32               size of write buffer                    |
|  nrec           int32               Number of current record numbers        |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Sep 96   Joel Gales    Increase size of utlbuf to hold Vdata fieldnames    |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
PTwritesetup(int32 fid, int32 ptVgrpID, int32 vdataID, int32 level,
	     int32 * nrec, int32 * sz)
{
    intn            status = 0;	/* routine return status variable */

    uint8          *buf;	/* Data buffer */
    uint8           recChk;	/* Record check flag */

    int32           vdataID0;	/* Vdata ID */
    int32           tag;	/* Vdata tag */
    int32           ref;	/* Vdata reference */

    char            utlbuf[VSFIELDMAX * FIELDNAMELENMAX];
    /* Utility buffer */


    /* Get current number of records */
    /* ----------------------------- */
    *nrec = VSelts(vdataID);


    /* If # rec = 1 then check whether 1st record is initialization record */
    /* ------------------------------------------------------------------- */
    if (*nrec == 1)
    {
	/* Get reference and vdata ID of "LevelWritten" Vdata */
	/* -------------------------------------------------- */
	Vgettagref(ptVgrpID, 0, &tag, &ref);
	vdataID0 = VSattach(fid, ref, "w");


	/* Read record for desired level */
	/* ----------------------------- */
	VSseek(vdataID0, level);
	VSsetfields(vdataID0, "LevelWritten");
	VSread(vdataID0, &recChk, 1, FULL_INTERLACE);


	/* If level not yet written ... */
	/* ---------------------------- */
	if (recChk == 0)
	{
	    /* Set number of current records to 0 */
	    /* ---------------------------------- */
	    *nrec = 0;


	    /* Write "1" to "LevelWritten" record for this level */
	    /* ------------------------------------------------- */
	    recChk = 1;
	    VSseek(vdataID0, level);
	    VSwrite(vdataID0, &recChk, 1, FULL_INTERLACE);
	}

	/* Detach from "LevelWritten" Vdata */
	/* -------------------------------- */
	VSdetach(vdataID0);

    }


    /* Get record size and build buffer */
    /* -------------------------------- */
    VSgetfields(vdataID, utlbuf);
    VSsetfields(vdataID, utlbuf);
    *sz = VSsizeof(vdataID, utlbuf);
    buf = (uint8 *) calloc(*sz, 1);
    if(buf == NULL)
    { 
	HEpush(DFE_NOSPACE,"PTwritesetup", __FILE__, __LINE__);
	return(-1);
    }


    /* Setup for append */
    /* ---------------- */
    if (*nrec > 0)
    {
	VSseek(vdataID, *nrec - 1);
	VSread(vdataID, buf, 1, FULL_INTERLACE);
    }
    else
    {
	VSseek(vdataID, 0);
    }

    free(buf);
    return (status);
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION:  PTwritelevel                                                    |
|                                                                             |
|  DESCRIPTION: Writes (appends) full records to a level.                     |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               point structure ID                      |
|  level          int32               level number (0 - based)                |
|  nrec           int32               Number of records to write              |
|  data           void                data buffer                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|      None                                                                   |
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
PTwritelevel(int32 pointID, int32 level, int32 nrec, VOIDP data)

{
    intn            i;		/* Loop index */
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           idOffset = PTIDOFFSET;	/* Point ID offset */
    int32           ptVgrpID;	/* Point Vgroup ID */
    int32           nlevels;	/* Number of levels in point */
    int32           pID;	/* Point ID - offset */
    int32           vdataID;	/* Vdata ID */
    int32           rec0;	/* Current number of records */
    int32           sz;		/* Size of record */
    int32          *recs;	/* Pointer to record number buffer */


    /* Check for valid point ID */
    /* ------------------------ */
    status = PTchkptid(pointID, "PTwritelevel",
		       &fid, &sdInterfaceID, &ptVgrpID);


    /* If no problems ... */
    /* ------------------ */
    if (status == 0)
    {
	/* Get number of levels */
	/* -------------------- */
	nlevels = PTnlevels(pointID);


	/* Report error if no levels (vdatas) defined */
	/* ------------------------------------------ */
	if (nlevels == 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "PTwritelevel", __FILE__, __LINE__);
	    HEreport("No Levels Defined for point ID: %d\n", pointID);
	}
	else if (nlevels < level)
	{
	    /* Report error if level # to large */
	    /* -------------------------------- */
	    status = -1;
	    HEpush(DFE_GENAPP, "PTwritelevel", __FILE__, __LINE__);
	    HEreport("Only %d levels Defined for point ID: %d\n",
		     nlevels, pointID);
	}


	if (status == 0)
	{
	    /* Get vdata ID */
	    /* ------------ */
	    pID = pointID % idOffset;
	    vdataID = PTXPoint[pID].vdID[level];


	    /* Setup for write, return current # of records & record size */
	    /* ---------------------------------------------------------- */
	    PTwritesetup(fid, ptVgrpID, vdataID, level, &rec0, &sz);


	    /* Write data to point level vdata */
	    /* ------------------------------- */
	    VSwrite(vdataID, (uint8 *) data, nrec, FULL_INTERLACE);


	    /* Write BackPointers & FwdPointers */
	    /* -------------------------------- */
	    if (level > 0)
	    {
		recs = (int32 *) calloc(nrec, 4);
		if(recs == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"PTwritelevel", __FILE__, __LINE__);
		    return(-1);
		}
		for (i = 0; i < nrec; i++)
		{
		    recs[i] = i + rec0;
		}
		status = PTwrbckptr(pointID, level, nrec, recs);
		free(recs);

		status = PTwrfwdptr(pointID, level - 1);
	    }
	}
    }
    return (status);
}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTupdatelevel                                                    |
|                                                                             |
|  DESCRIPTION: Updates the specified fields and records of a level.          |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               point structure ID                      |
|  level          int32               level number (0 - based)                |
|  fieldlist      char                fieldlist to read (comma-separated)     |
|  nrec           int32               Number of records to read               |
|  recs           int32               array of record numbers to read         |
|  data           void                data buffer                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|  data                                                                       |
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
PTupdatelevel(int32 pointID, int32 level, char *fieldlist, int32 nrec,
	      int32 recs[], VOIDP data)
{
    intn            i;		/* Loop index */
    intn            j;		/* Loop index */
    intn            status = 0;	/* routine return status variable */

    uint8          *buf;	/* Pointer to R/W buffer */
    uint8          *ptr;	/* Pointer within R/W buffer */

    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           idOffset = PTIDOFFSET;	/* Point ID offset */
    int32           ptVgrpID;	/* Point Vgroup ID */
    int32           nlevels;	/* Number of levels in point */
    int32           nfields;	/* Number of fields in fieldlist */
    int32           vdataID;	/* Vdata ID */
    int32           slen[VSFIELDMAX];	/* String length array */
    int32           dum;	/* Dummy variable */
    int32           sz;		/* Offset counter */
    int32           slen2[VSFIELDMAX];	/* String length array 2 */
    int32          *offset;	/* Pointer to offset array */
    int32          *fldlen;	/* Pointer to field length array */
    int32          *recptr;	/* Pointer to record number array */
    int32           nallflds;	/* Total number of fields in record */

    char            utlbuf[256];/* Utility buffer */
    char           *pntr[VSFIELDMAX];	/* String pointer array */
    char            utlbuf2[256];	/* Utility buffer 2 */
    char            allfields[VSFIELDMAX * FIELDNAMELENMAX];
    /* Buffer to hold names of all fields in level */
    char           *ptrn2[VSFIELDMAX];	/* String pointer array 2 */


    /* Check for valid point ID */
    /* ------------------------ */
    status = PTchkptid(pointID, "PTupdatelevel",
		       &fid, &sdInterfaceID, &ptVgrpID);


    if (status == 0)
    {

	/* Get number of levels */
	/* -------------------- */
	nlevels = PTnlevels(pointID);


	/* Report error if no levels (vdatas) defined */
	/* ------------------------------------------ */
	if (nlevels == 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "PTupdatelevel", __FILE__, __LINE__);
	    HEreport("No Levels Defined for point ID: %d\n", pointID);
	}
	else if (nlevels < level)
	{
	    /* Report error if level # to large */
	    /* -------------------------------- */
	    status = -1;
	    HEpush(DFE_GENAPP, "PTupdatelevel", __FILE__, __LINE__);
	    HEreport("Only %d levels Defined for point ID: %d\n",
		     nlevels, pointID);
	}


	/* If no problems ... */
	/* ------------------ */
	if (status == 0)
	{
	    /* Get vdata ID */
	    /* ------------ */
	    vdataID = PTXPoint[pointID % idOffset].vdID[level];


	    /* Parse field list */
	    /* ---------------- */
	    nfields = EHparsestr(fieldlist, ',', pntr, slen);


	    /* Check that all fields in list exist in level */
	    /* -------------------------------------------- */
	    for (i = 0; i < nfields; i++)
	    {
		/* Copy each fieldname into utlbuf */
		/* ------------------------------- */
		memcpy(utlbuf, pntr[i], slen[i]);
		utlbuf[slen[i]] = 0;

		if (VSfexist(vdataID, utlbuf) != 1)
		{
		    status = -1;
		    HEpush(DFE_GENAPP, "PTreadlevel", __FILE__, __LINE__);
		    HEreport("Field: \"%s\" does not exist.\n",
			     utlbuf);
		}
	    }


	    /* If no problems ... */
	    /* ------------------ */
	    if (status == 0)
	    {
		/* Get names & total # of fields in level */
		/* -------------------------------------- */
		VSgetfields(vdataID, allfields);
		nallflds = EHparsestr(allfields, ',', ptrn2, slen2);


		/* Setup field offset and length arrays */
		/* ------------------------------------ */
		offset = (int32 *) calloc(nfields, 4);
		if(offset == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"PTupdatelevel", __FILE__, __LINE__);
		    return(-1);
		}
		fldlen = (int32 *) calloc(nfields, 4);
		if(fldlen == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"PTupdatelevel", __FILE__, __LINE__);
		    free(offset);
		    return(-1);
		}


		/* Loop through all fields in fieldlist */
		/* ------------------------------------ */
		for (i = 0; i < nfields; i++)
		{
		    /* Get field length of each field in fieldlist */
		    /* ------------------------------------------- */
		    memcpy(utlbuf, pntr[i], slen[i]);
		    utlbuf[slen[i]] = 0;
		    fldlen[i] = VSsizeof(vdataID, utlbuf);


		    sz = 0;

		    /* Loop through all fields in level */
		    /* -------------------------------- */
		    for (j = 0; j < nallflds; j++)
		    {
			/* Copy each fieldname into utlbuf2 */
			/* -------------------------------- */
			memcpy(utlbuf2, ptrn2[j], slen2[j]);
			utlbuf2[slen2[j]] = 0;


			/* Check for match with field in fieldlist */
			/* --------------------------------------- */
			if (strcmp(utlbuf, utlbuf2) == 0)
			{
			    /* If match then store offset */
			    /* -------------------------- */
			    offset[i] = sz;
			    break;
			}

			/* If no match then increment offset */
			/* --------------------------------- */
			sz += VSsizeof(vdataID, utlbuf2);
		    }
		}



		/* Establish fields to read & setup data buffer */
		/* -------------------------------------------- */
		VSsetfields(vdataID, allfields);
		buf = (uint8 *) calloc(VSsizeof(vdataID, allfields), 1);
		if(buf == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"PTupdatelevel", __FILE__, __LINE__);
		    return(-1);
		}


		/* Set ptr to updated data buffer */
		/* ------------------------------ */
		ptr = (uint8 *) data;


		/* Loop through all records to read */
		/* -------------------------------- */
		for (i = 0; i < nrec; i++)
		{
		    /* Read current record from level */
		    /* ------------------------------ */
		    VSseek(vdataID, recs[i]);
		    VSread(vdataID, (uint8 *) buf, 1, FULL_INTERLACE);


		    /* Loop through all fields to update */
		    /* --------------------------------- */
		    for (j = 0; j < nfields; j++)
		    {
			/* Copy data from updated data buffer & update ptr */
			/* ----------------------------------------------- */
			memcpy(buf + offset[j], ptr, fldlen[j]);
			ptr += fldlen[j];
		    }


		    /* Write updated record back to vdata */
		    /* ---------------------------------- */
		    VSseek(vdataID, recs[i]);
		    VSwrite(vdataID, (uint8 *) buf, 1, FULL_INTERLACE);
		}

		free(offset);
		free(fldlen);
		free(buf);



		/* Update Pointers to Previous Level */
		/* -------------------------------- */
		if (level > 0)
		{
		    /* Store back linkage field in utlbuf */
		    /* ---------------------------------- */
		    status = PTbcklinkinfo(pointID, level, utlbuf);


		    /* Check whether back linkage field is in fieldlist */
		    /* ------------------------------------------------ */
		    dum = EHstrwithin(utlbuf, fieldlist, ',');


		    /* If so then write back and forward pointers */
		    /* ------------------------------------------ */
		    if (dum != -1)
		    {
			/* Back pointers to previous level */
			/* ------------------------------- */
			status = PTwrbckptr(pointID, level, nrec, recs);

			/* Forward pointers from previous level */
			/* ------------------------------------ */
			status = PTwrfwdptr(pointID, level - 1);
		    }

		}


		/* Update Pointers to Next Level */
		/* ---------------------------- */
		if (level < PTnlevels(pointID) - 1 && dum != -1)
		{
		    /* Store forward linkage field in utlbuf */
		    /* ------------------------------------- */
		    status = PTfwdlinkinfo(pointID, level, utlbuf);


		    /* Check whether forward linkage field is in fieldlist */
		    /* --------------------------------------------------- */
		    dum = EHstrwithin(utlbuf, fieldlist, ',');


		    /* If so then write back and forward pointers */
		    /* ------------------------------------------ */
		    if (dum != -1)
		    {
			/* Get number of records in next level */
			/* ----------------------------------- */
			nrec = PTnrecs(pointID, level + 1);


			/* Fill recptr array with numbers btw 0 and nrec-1 */
			/* ----------------------------------------------- */
			recptr = (int32 *) calloc(nrec, 4);
			if(recptr == NULL)
			{ 
			    HEpush(DFE_NOSPACE,"PTupdatelevel", __FILE__, __LINE__);
			    return(-1);
			}
			for (i = 0; i < nrec; i++)
			{
			    recptr[i] = i;
			}
			/* Back pointers from next level */
			/* ----------------------------- */
			status = PTwrbckptr(pointID, level + 1, nrec, recptr);

			/* Forward pointers to next level */
			/* ------------------------------ */
			status = PTwrfwdptr(pointID, level);
			free(recptr);
		    }
		}

	    }

	}
    }
    return (status);

}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTreadlevel                                                      |
|                                                                             |
|  DESCRIPTION: Reads data from the specified fields and records of a level.  |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               point structure ID                      |
|  level          int32               level number (0 - based)                |
|  fieldlist      char                fieldlist to read (comma-separated)     |
|  nrec           int32               Number of records to read               |
|  recs           int32               array of record numbers to read         |
|  datbuf         void                data buffer                             |
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
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
PTreadlevel(int32 pointID, int32 level, char *fieldlist,
	    int32 nrec, int32 recs[], VOIDP datbuf)

{
    intn            i;		/* Loop index */
    intn            status = 0;	/* routine return status variable */

    uint8          *temPtr = (uint8 *) datbuf;	/* Temporary data pointer */

    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           idOffset = PTIDOFFSET;	/* Point ID offset */
    int32           ptVgrpID;	/* Point Vgroup ID */
    int32           nlevels;	/* Number of levels in point */
    int32           nfields;	/* Number of fields in fieldlist */
    int32           vdataID;	/* Vdata ID */
    int32           slen[VSFIELDMAX];	/* String length array */
    int32           sz;
    int32           maxrecno;

    char            utlbuf[256];/* Utility buffer */
    char           *pntr[VSFIELDMAX];	/* String pointer array */
    static char     msg[] = "Point record number: %d out of range.\n";


    /* Check for valid point ID */
    /* ------------------------ */
    status = PTchkptid(pointID, "PTreadlevel",
		       &fid, &sdInterfaceID, &ptVgrpID);


    if (status == 0)
    {
	/* Get number of levels in point */
	/* ----------------------------- */
	nlevels = PTnlevels(pointID);


	/* Report error if no levels defined */
	/* --------------------------------- */
	if (nlevels == 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "PTreadlevel", __FILE__, __LINE__);
	    HEreport("No Levels Defined for point ID: %d\n", pointID);
	}
	else if (nlevels < level)
	{
	    /* Report error if level out of bounds */
	    /* ----------------------------------- */
	    status = -1;
	    HEpush(DFE_GENAPP, "PTreadlevel", __FILE__, __LINE__);
	    HEreport("Only %d levels Defined for point ID: %d\n",
		     nlevels, pointID);
	}


	if (status == 0)
	{
	    /* Get level vdata ID */
	    /* ------------------ */
	    vdataID = PTXPoint[pointID % idOffset].vdID[level];


	    /* Parse field list */
	    /* ---------------- */
	    nfields = EHparsestr(fieldlist, ',', pntr, slen);


	    /* Check that all fields in list exist in level */
	    /* -------------------------------------------- */
	    for (i = 0; i < nfields; i++)
	    {
		memcpy(utlbuf, pntr[i], slen[i]);
		utlbuf[slen[i]] = 0;

		if (VSfexist(vdataID, utlbuf) != 1)
		{
		    status = -1;
		    HEpush(DFE_GENAPP, "PTreadlevel", __FILE__, __LINE__);
		    HEreport("Field: \"%s\" does not exist.\n",
			     utlbuf);
		}
	    }


	    /* If no problems ... */
	    /* ------------------ */
	    if (status == 0)
	    {
		/* Get size of record */
		/* ------------------ */
		sz = VSsizeof(vdataID, fieldlist);


		/* Get maximum record number */
		/* ------------------------- */
		maxrecno = VSelts(vdataID) - 1;


		/* Check that all requested records are in bounds */
		/* ---------------------------------------------- */
		for (i = 0; i < nrec; i++)
		{
		    if (recs[i] < 0 || recs[i] > maxrecno)
		    {
			status = -1;
			HEpush(DFE_GENAPP, "PTreadlevel", __FILE__, __LINE__);
			HEreport(msg, recs[i]);
			break;
		    }
		}


		/* If no problems ... */
		/* ------------------ */
		if (status == 0)
		{
		    /* Establish fields to read */
		    /* ------------------------ */
		    VSsetfields(vdataID, fieldlist);


		    /* If nrec = -1 then read all records in level */
		    /* ------------------------------------------- */
		    if (nrec == -1)
		    {
			VSread(vdataID, (uint8 *) datbuf,
			       maxrecno + 1, FULL_INTERLACE);
		    }
		    else
		    {
			for (i = 0; i < nrec; i++)
			{
			    /* Read each desired record one at a time */
			    /* -------------------------------------- */
			    VSseek(vdataID, recs[i]);
			    VSread(vdataID,
				   (uint8 *) (temPtr + (i * sz)),
				   1, FULL_INTERLACE);
			}
		    }
		}
	    }
	}
    }
    return (status);

}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTrecnum                                                         |
|                                                                             |
|  DESCRIPTION:                                                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  regionID       int32               Region ID                               |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               Point structure ID                      |
|  level          int32               level number (0 - based)                |
|  minlevel       int32               Minimum level number                    |
|  maxlevel       int32               Maximum level number                    |
|  nrec           int32               Number of records                       |
|  recs           int32               Array of record numbers                 |
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
|  Aug 96   Carol Tsai    Fixed the code in oder to return the correct size   | 
|                         of a defined region of interest from a set of       |
|                         fields in a single level when the function          |
|                         PTregioninfo is called                              | 
|                                                                             | 
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
PTrecnum(int32 pointID, int32 level, int32 minlevel, int32 maxlevel,
	 int32 nrec, int32 recs[])

{
    intn            i;		/* Loop index */
    intn            j;		/* Loop index */
    intn            k;		/* Loop index */
    intn            status = 0;	/* routine return status variable */
    intn            statFwd;	/* status from PTrdfwdptr */

    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           ptVgrpID;	/* Point Vgroup ID */
    int32           regionID;	/* Region ID */
    int32           num;	/* Utility number variable */
    int32           nFoll;	/* Number of records in following level */
    int32           nPrev;	/* Number of records in previous level */
    int32          *bckRecs;	/* Pointer to back pointer records */
    int32          *fwdRecs;	/* Pointer to forward pointer records */
    int32          *ptr;	/* Utility pointer */

    char           *flag;	/* Pointer to flag array */


    /* Check for valid point ID */
    /* ------------------------ */
    status = PTchkptid(pointID, "PTrecnum", &fid, &sdInterfaceID, &ptVgrpID);


    if (status == 0)
    {

	/* Setup External Region Variable */
	/* ------------------------------ */
	for (k = 0; k < NPOINTREGN; k++)
	{
	    /* Find empty slot */
	    /* --------------- */
	    if (PTXRegion[k] == 0)
	    {
		/* Allocate space for region structure */
		/* ----------------------------------- */
		PTXRegion[k] = (struct pointRegion *)
		    calloc(1, sizeof(struct pointRegion));
		if(PTXRegion[k] == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"PTrecnum", __FILE__, __LINE__);
		    return(-1);
		}


		/* Store file ID & point ID */
		/* ------------------------ */
		PTXRegion[k]->fid = fid;
		PTXRegion[k]->pointID = pointID;


		/* Store number of selected records */
		/* -------------------------------- */
		PTXRegion[k]->nrec[level] = nrec;


		/* Allocate space and write record numbers */
		/* --------------------------------------- */
		PTXRegion[k]->recPtr[level] = (int32 *) calloc(nrec, 4);
		if(PTXRegion[k]->recPtr[level] == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"PTrecnum", __FILE__, __LINE__);
		    return(-1);
		}

		for (i = 0; i < nrec; i++)
		{
		    *(PTXRegion[k]->recPtr[level] + i) = recs[i];
		}


		/* Establish region ID */
		/* ------------------- */
		regionID = k;
		break;
	    }
	}



	/* Propagate Downward */
	/* ------------------ */
	if (minlevel != -1)
	{
	    /* Loop through levels below current one to minimun one */
	    /* ---------------------------------------------------- */
	    for (j = level - 1; j >= minlevel; j--)
	    {
		/* Get number of records in (j+1)th level */
		/* -------------------------------------- */
		num = PTnrecs(pointID, j + 1);


		/* Read in back pointers for current level */
		/* --------------------------------------- */
		bckRecs = (int32 *) calloc(num, 4);
		if(bckRecs == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"PTrecnum", __FILE__, __LINE__);
		    return(-1);
		}
		status = PTrdbckptr(pointID, j + 1, num, bckRecs);


		/* Get number of records in jth level */
		/* ---------------------------------- */
		nPrev = PTnrecs(pointID, j);


		/* Allocate space for flag array */
		/* ----------------------------- */
		flag = (char *) calloc(nPrev, 1);
		if(flag == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"PTrecnum", __FILE__, __LINE__);
		    return(-1);
		}


		/* Loop through all selected records in (j+1)th level */
		/* -------------------------------------------------- */
		for (i = 0; i < PTXRegion[regionID]->nrec[j + 1]; i++)
		{
		    /* Flag corresponding records in previous level */
		    /* -------------------------------------------- */
		    k = *(PTXRegion[regionID]->recPtr[j + 1] + i);
                    /*
                     ** 1997-8-25 C. S. W. TSAI - Space Applications Corp.
                     ** Changed flag[bckRecs[k]] to flag[k] in order to 
                     ** return the correct siz of a defined region of 
                     ** interest from a set of fields in a single level
                     ** when the function PTregioninfo is called 
                    */ 
		    flag[bckRecs[k]] = 1;  
		}


		/* Compute number of corresponding records in previous level */
		/* --------------------------------------------------------- */
		num = 0;
		for (i = 0; i < nPrev; i++)
		{
		    num += flag[i];
		}


		/* Set number of records in jth level in region structure */
		/* ------------------------------------------------------ */
		PTXRegion[regionID]->nrec[j] = num;


		/* Allocate space for record numbers */
		/* --------------------------------- */
		PTXRegion[regionID]->recPtr[j] = (int32 *) calloc(num, 4);
		if(PTXRegion[regionID]->recPtr[j] == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"PTrecnum", __FILE__, __LINE__);
		    return(-1);
		}


		/* Fill in record numbers in region structure */
		/* ------------------------------------------ */
		num = 0;
		for (i = 0; i < nPrev; i++)
		{
		    if (flag[i] == 1)
		    {
			*(PTXRegion[regionID]->recPtr[j] + num) = i;
			num++;
		    }
		}
		free(flag);
		free(bckRecs);
	    }
	}


	/* Propagate Upward */
	/* ---------------- */
	if (maxlevel != -1)
	{
	    /* Loop through levels above current one to maximum one */
	    /* ---------------------------------------------------- */
	    for (j = level + 1; j <= maxlevel; j++)
	    {
		/* Get number of records in (j-1)th level */
		/* -------------------------------------- */
		num = PTnrecs(pointID, j - 1);


		/* Read in forward pointers to jth level */
		/* ------------------------------------- */
		fwdRecs = (int32 *) calloc(2 * num, 4);
		if(fwdRecs == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"PTrecnum", __FILE__, __LINE__);
		    return(-1);
		}
		statFwd = PTrdfwdptr(pointID, j - 1, num, fwdRecs);


		/* If forward records exist ... */
		/* ---------------------------- */
		if (statFwd == 0)
		{
		    /* Accumulate all extent values */
		    /* ---------------------------- */
		    for (i = 0; i < nrec; i++)
		    {
			PTXRegion[regionID]->nrec[j] +=
			    fwdRecs[2 * recs[i] + 1];
		    }


		    /* Allocate space for record numbers */
		    /* --------------------------------- */
		    PTXRegion[regionID]->recPtr[j] =
			(int32 *) calloc(PTXRegion[regionID]->nrec[j], 4);
		    if(PTXRegion[regionID]->recPtr[j] == NULL)
		    { 
			HEpush(DFE_NOSPACE,"PTrecnum", __FILE__, __LINE__);
			return(-1);
		    }


		    /* Assign all begin values */
		    /* ----------------------- */
		    ptr = PTXRegion[regionID]->recPtr[j];
		    for (i = 0; i < nrec; i++)
		    {
			for (k = 0; k < fwdRecs[2 * i + 1]; k++)
			{
			    *ptr++ = fwdRecs[2 * recs[i]] + k;
			}
		    }
		}
		else
		{
		    /* Get number of records in jth (following) level */
		    /* ---------------------------------------------- */
		    nFoll = PTnrecs(pointID, j);


		    /* Read in back pointers for following level */
		    /* ----------------------------------------- */
		    bckRecs = (int32 *) calloc(nFoll, 4);
		    if(bckRecs == NULL)
		    { 
			HEpush(DFE_NOSPACE,"PTrecnum", __FILE__, __LINE__);
			return(-1);
		    }
		    status = PTrdbckptr(pointID, j, nFoll, bckRecs);


		    /* Allocate space for flag array */
		    /* ----------------------------- */
		    flag = (char *) calloc(nFoll, 1);
		    if(flag == NULL)
		    { 
			HEpush(DFE_NOSPACE,"PTrecnum", __FILE__, __LINE__);
			return(-1);
		    }


		    /* Loop through all records in following level */
		    /* ------------------------------------------- */
		    for (i = 0; i < nFoll; i++)
		    {
			/* Loop through selected records in current level */
			/* ---------------------------------------------- */
			for (k = 0; k < PTXRegion[regionID]->nrec[j - 1]; k++)
			{
			    /*
			     * Flag records in following level pointing back
			     * to selected records in current level
			     */
			    if (*(PTXRegion[regionID]->recPtr[j - 1] + k) ==
				bckRecs[i])
			    {
				flag[i] = 1;
			    }
			}
		    }


		    /* Compute number of corresponding records */
		    /* --------------------------------------- */
		    num = 0;
		    for (i = 0; i < nFoll; i++)
		    {
			num += flag[i];
		    }


		    /* Set # of records in jth level in region structure */
		    /* ------------------------------------------------- */
		    PTXRegion[regionID]->nrec[j] = num;


		    /* Allocate space for record numbers */
		    /* --------------------------------- */
		    PTXRegion[regionID]->recPtr[j] = (int32 *) calloc(num, 4);
		    if(PTXRegion[regionID]->recPtr[j] == NULL)
		    { 
			HEpush(DFE_NOSPACE,"PTrecnum", __FILE__, __LINE__);
			return(-1);
		    }
		    

		    /* Fill in record numbers in region structure */
		    /* ------------------------------------------ */
		    num = 0;
		    for (i = 0; i < nFoll; i++)
		    {
			if (flag[i] == 1)
			{
			    *(PTXRegion[regionID]->recPtr[j] + num) = i;
			    num++;
			}
		    }
		    free(flag);
		    free(bckRecs);
		}
		free(fwdRecs);
	    }
	}

    }
    return (regionID);
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTgetrecnums                                                     |
|                                                                             |
|  DESCRIPTION: Returns corresponding record numbers in a related field.      |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               point structure ID                      |
|  inlevel        int32               level number of input records           |
|  outlevel       int32               level number of output records          |
|  inNrec         int32               Number of input records                 |
|  inRecs         int32               Array of input record numbers           |
|  outNrec        int32               Number of output records                |
|  outRecs        int32               Array of output record numbers          |
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
PTgetrecnums(int32 pointID, int32 inlevel, int32 outlevel,
	     int32 inNrec, int32 inRecs[], int32 * outNrec, int32 outRecs[])
{
    intn            i;		/* Loop index */
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           ptVgrpID;	/* Point Vgroup ID */
    int32          *recPtr;
    int32           regionID;

    int32           minlevel;
    int32           maxlevel;

    status = PTchkptid(pointID, "PTgetrecnums",
		       &fid, &sdInterfaceID, &ptVgrpID);

    if (status == 0)
    {
	if (outlevel < inlevel)
	{
	    minlevel = outlevel;
	    maxlevel = -1;
	}

	if (outlevel > inlevel)
	{
	    minlevel = -1;
	    maxlevel = outlevel;
	}

	regionID = PTrecnum(pointID, inlevel, minlevel, maxlevel,
			    inNrec, inRecs);
	*outNrec = PTXRegion[regionID]->nrec[outlevel];
	recPtr = PTXRegion[regionID]->recPtr[outlevel];

	for (i = 0; i < *outNrec; i++)
	{
	    outRecs[i] = *(recPtr + i);
	}


	/* Free region */
	/* ----------- */
	for (i = 0; i < 8; i++)
	{
	    if (PTXRegion[regionID]->recPtr[i] != 0)
	    {
		free(PTXRegion[regionID]->recPtr[i]);
	    }
	}
	free(PTXRegion[regionID]);
	PTXRegion[regionID] = 0;

    }
    return (status);
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTwrrdattr                                                       |
|                                                                             |
|  DESCRIPTION:                                                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               point structure ID                      |
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
intn
PTwrrdattr(int32 pointID, char *attrname, int32 numbertype, int32 count,
	   char *wrcode, VOIDP datbuf)

{
    intn            status;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           attrVgrpID;	/* Point attribute ID */
    int32           dum;	/* dummy variable */
    int32           idOffset = PTIDOFFSET;	/* Point ID offset */


    /* Check Point id */
    /* -------------- */
    status = PTchkptid(pointID, "PTwrrdattr", &fid, &dum, &dum);

    if (status == 0)
    {
	/* Perform Attribute I/O */
	/* --------------------- */
	attrVgrpID = PTXPoint[pointID % idOffset].VIDTable[2];
	status = EHattr(fid, attrVgrpID, attrname, numbertype, count,
			wrcode, datbuf);
    }
    return (status);
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTwriteattr                                                      |
|                                                                             |
|  DESCRIPTION: Writes/updates attribute in a point.                          |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               point structure ID                      |
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
PTwriteattr(int32 pointID, char *attrname, int32 numbertype, int32 count,
	    VOIDP datbuf)
{
    intn            status = 0;	/* routine return status variable */

    /* Call PTwrrdattr routine to write attribute */
    /* ------------------------------------------ */
    status = PTwrrdattr(pointID, attrname, numbertype, count, "w", datbuf);

    return (status);
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTreadattr                                                       |
|                                                                             |
|  DESCRIPTION: Reads attribute from a point.                                 |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               point structure ID                      |
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
PTreadattr(int32 pointID, char *attrname, VOIDP datbuf)
{
    intn            status = 0;	/* routine return status variable */
    int32           dum = 0;	/* dummy variable */


    /* Call PTwrrdattr routine to read attribute */
    /* ------------------------------------------ */
    status = PTwrrdattr(pointID, attrname, dum, dum, "r", datbuf);

    return (status);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTattrinfo                                                       |
|                                                                             |
|  DESCRIPTION:                                                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               point structure ID                      |
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
PTattrinfo(int32 pointID, char *attrname, int32 * numbertype, int32 * count)
{
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           attrVgrpID;	/* Point attribute ID */
    int32           dum;	/* dummy variable */
    int32           idOffset = PTIDOFFSET;	/* Point ID offset */


    status = PTchkptid(pointID, "PTattrinfo", &fid, &dum, &dum);

    attrVgrpID = PTXPoint[pointID % idOffset].VIDTable[2];

    status = EHattrinfo(fid, attrVgrpID, attrname, numbertype,
			count);

    return (status);
}






/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTinqattrs                                                       |
|                                                                             |
|  DESCRIPTION:                                                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  nattr          int32               Number of attributes in point struct    |
|                                                                             |
|  INPUTS:                                                                    |
|  point ID       int32               point structure ID                      |
|                                                                             |
|  OUTPUTS:                                                                   |
|  attrnames      char                Attribute names in point struct         |
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
PTinqattrs(int32 pointID, char *attrnames, int32 * strbufsize)
{
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file ID */
    int32           attrVgrpID;	/* Point attribute ID */
    int32           dum;	/* dummy variable */
    int32           idOffset = PTIDOFFSET;	/* Point ID offset */
    int32           nattr = 0;	/* Number of point attributes */


    /* Check Point id */
    status = PTchkptid(pointID, "PTinqattrs", &fid, &dum, &dum);

    if (status == 0)
    {
	attrVgrpID = PTXPoint[pointID % idOffset].VIDTable[2];
	nattr = EHattrcat(fid, attrVgrpID, attrnames, strbufsize);
    }

    return (nattr);
}






/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTdefboxregion                                                   |
|                                                                             |
|  DESCRIPTION: Define region of interest by latitude/longitude.              |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  regionID       int32               Region ID                               |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               Point structure ID                      |
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
|  Oct 96   Joel Gales    Add ability to handle regions crossing date line    |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
PTdefboxregion(int32 pointID, float64 cornerlon[], float64 cornerlat[])
{
    intn            i;		/* Loop index */
    intn            status;	/* routine return status variable */

    int32           sizeLon;	/* Size of Longitude field */
    int32           sizeLat;	/* Size of Latitude field */
    int32           sizeCoLat = -1;	/* Size of Colatitude field */
    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           ptVgrpID;	/* Point Vgroup ID */
    int32           nlevels;	/* Number of levels in point */

    int32           lonlev;	/* Level containing Longitude field */
    int32           latlev;	/* Level containing Latitude field */
    int32           collev;	/* Level containing Colatitude field */
    int32          *recs;	/* Pointer to record number array */
    int32          *recFound;	/* Pointer to record found flag array */
    int32           regionID = -1;	/* Region ID (returned) */
    int32           nFound = 0;	/* Number of records found */
    int32           nrec;	/* Number of records in geo field level */
    int32           bndflag;	/* +/-180 longitude boundary flag */
    int32           lonTest;	/* Longitude test flag */
    int32           latTest;	/* Latitude test flag */

    float32        *lon32;	/* Pointer to float32 longitude values */
    float32        *lat32;	/* Pointer to float32 latitude values */
    float32         lon32Test;	/* Longitude value to test */
    float32         lat32Test;	/* Latitude value to test */

    float64        *lon64;	/* Pointer to float64 longitude values */
    float64        *lat64;	/* Pointer to float64 latitude values */
    float64         lon64Test;	/* Longitude value to test */
    float64         lat64Test;	/* Latitude value to test */


    /* Check for valid point ID */
    /* ------------------------ */
    status = PTchkptid(pointID, "PTdefboxregion", &fid, &sdInterfaceID,
		       &ptVgrpID);


    if (status == 0)
    {
	/* Get byte size of Longitude field and its level */
	/* ---------------------------------------------- */
	sizeLon = PTsizeof(pointID, "Longitude", &lonlev);


	/* If Longitude field doesn't exist report error */
	/* --------------------------------------------- */
	if (sizeLon == 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "PTdefboxregion", __FILE__, __LINE__);
	    HEreport("\"Longitude\" field not found.\n");
	}


	/* Get byte size of Latitude field and its level */
	/* -------------------------------------------- */
	sizeLat = PTsizeof(pointID, "Latitude", &latlev);


	/* If Latitude field doesn't exist ... */
	/* ----------------------------------- */
	if (sizeLat == 0)
	{
	    /* Get byte size of Colatitude field and its level */
	    /* ----------------------------------------------- */
	    sizeCoLat = PTsizeof(pointID, "Colatitude", &collev);


	    /* If Colatitude field doesn't get exist report error */
	    /* -------------------------------------------------- */
	    if (sizeCoLat == 0)
	    {
		status = -1;
		HEpush(DFE_GENAPP, "PTdefboxregion", __FILE__, __LINE__);
		HEreport(
		 "Neither \"Latitude\" nor \"Colatitude\" fields found.\n");
	    }
	    else
	    {
		/* Check that Longitude and Colatitude are in same level */
		/* ----------------------------------------------------- */
		if (lonlev != collev)
		{
		    status = -1;
		    HEpush(DFE_GENAPP, "PTdefboxregion", __FILE__, __LINE__);
		    HEreport(
			     "\"Longitude\" & \"Coatitude\" must be in same level.\n");
		}
	    }
	}
	else
	{
	    /* Check that Longitude and Latitude are in same level */
	    /* --------------------------------------------------- */
	    if (lonlev != latlev)
	    {
		status = -1;
		HEpush(DFE_GENAPP, "PTdefboxregion", __FILE__, __LINE__);
		HEreport(
		   "\"Longitude\" & \"Latitude\" must be in same level.\n");
	    }
	}


	/* If no problem ... */
	/* ----------------- */
	if (status == 0)
	{
	    /* Get number of levels in point */
	    /* ----------------------------- */
	    nlevels = PTnlevels(pointID);


	    /* Get number of records in longitude level */
	    /* ---------------------------------------- */
	    nrec = PTnrecs(pointID, lonlev);


	    /* Allocate space for recs and recFound arrays */
	    /* ------------------------------------------- */
	    recs = (int32 *) calloc(nrec, 4);
	    if(recs == NULL)
	    { 
		HEpush(DFE_NOSPACE,"PTdefboxregion", __FILE__, __LINE__);
		return(-1);
	    }
	    recFound = (int32 *) calloc(nrec, 4);
	    if(recFound == NULL)
	    { 
		HEpush(DFE_NOSPACE,"PTdefboxregion", __FILE__, __LINE__);
		return(-1);
	    }

	    /* Load recs array from 0 to nrec-1 */
	    /* -------------------------------- */
	    for (i = 0; i < nrec; i++)
	    {
		recs[i] = i;
	    }


	    /* Set boundary flag */
	    /* ----------------- */

	    /*
	     * This variable is set to 1 if the region of interest crosses
	     * the +/- 180 longitude boundary
	     */
	    bndflag = (cornerlon[0] < cornerlon[1]) ? 0 : 1;


	    /* If geo fields float32 ... */
	    /* ------------------------- */
	    if (sizeLon == 4)
	    {
		/* Allocate space for lon & lat arrays */
		/* ----------------------------------- */
		lon32 = (float32 *) calloc(nrec, 4);
		if(lon32 == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"PTdefboxregion", __FILE__, __LINE__);
		    return(-1);
		}

		lat32 = (float32 *) calloc(nrec, 4);
		if(lat32 == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"PTdefboxregion", __FILE__, __LINE__);
		    free(lon32);
		    return(-1);
		}

		/* Read in Longitude data */
		/* ---------------------- */
		status = PTreadlevel(pointID, lonlev, "Longitude",
				     nrec, recs, (char *) lon32);


		/* Read in Latitude data */
		/* --------------------- */
		if (sizeLat != 0)
		{
		    status = PTreadlevel(pointID, lonlev, "Latitude",
					 nrec, recs, (char *) lat32);
		}
		else
		{
		    /* else read in Colatitude data */
		    /* ---------------------------- */
		    status = PTreadlevel(pointID, lonlev, "Colatitude",
					 nrec, recs, (char *) lat32);
		}

	    }


	    /* If geo fields float64 ... */
	    /* ------------------------- */
	    if (sizeLon == 8)
	    {
		/* Allocate space for lon & lat arrays */
		/* ----------------------------------- */
		lon64 = (float64 *) calloc(nrec, 8);
		if(lon64 == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"PTdefboxregion", __FILE__, __LINE__);
		    return(-1);
		}
		lat64 = (float64 *) calloc(nrec, 8);
		if(lat64 == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"PTdefboxregion", __FILE__, __LINE__);
		    free(lon64);
		    return(-1);
		}

		/* Read in Longitude data */
		/* ---------------------- */
		status = PTreadlevel(pointID, lonlev, "Longitude",
				     nrec, recs, (char *) lon64);


		/* Read in Latitude data */
		/* --------------------- */
		if (sizeLat != 0)
		{
		    status = PTreadlevel(pointID, lonlev, "Latitude",
					 nrec, recs, (char *) lat64);
		}
		else
		{
		    /* else read in Colatitude data */
		    /* ---------------------------- */
		    status = PTreadlevel(pointID, lonlev, "Colatitude",
					 nrec, recs, (char *) lat64);
		}
	    }



	    /* If geo fields float32 ... */
	    /* ------------------------- */
	    if (sizeLon == 4)
	    {
		for (i = 0; i < nrec; i++)
		{
		    lon32Test = lon32[i];
		    lat32Test = lat32[i];


		    /* If longitude value > 180 convert to -180 to 180 range */
		    /* ----------------------------------------------------- */
		    if (lon32Test > 180)
		    {
			lon32Test = lon32Test - 360;
		    }


		    /* If Colatitude value convert to latitude value */
		    /* --------------------------------------------- */
		    if (sizeCoLat > 0)
		    {
			lat32Test = 90 - lat32Test;
		    }


		    /* Test if lat value is within range */
		    /* --------------------------------- */
		    latTest = (lat32Test >= cornerlat[0] &&
			       lat32Test <= cornerlat[1]);


		    if (bndflag == 1)
		    {
			/*
			 * If boundary flag set test whether longitude value
			 * is outside region and then flip
			 */
			lonTest = (lon32Test >= cornerlon[1] &&
				   lon32Test <= cornerlon[0]);
			lonTest = 1 - lonTest;
		    }
		    else
		    {
			lonTest = (lon32Test >= cornerlon[0] &&
				   lon32Test <= cornerlon[1]);
		    }


		    /*
		     * If both longitude and latitude are within region set
		     * recFound flag
		     */
		    if (lonTest + latTest == 2)
		    {
			recFound[nFound] = i;
			nFound++;
		    }
		}

		free(lon32);
		free(lat32);
	    }



	    /* If geo fields float64 ... */
	    /* ------------------------- */
	    if (sizeLon == 8)
	    {
		for (i = 0; i < nrec; i++)
		{
		    lon64Test = lon64[i];
		    lat64Test = lat64[i];


		    /* If longitude value > 180 convert to -180 to 180 range */
		    /* ----------------------------------------------------- */
		    if (lon64Test > 180)
		    {
			lon64Test = lon64Test - 360;
		    }


		    /* If Colatitude value convert to latitude value */
		    /* --------------------------------------------- */
		    if (sizeCoLat > 0)
		    {
			lat64Test = 90 - lat64Test;
		    }


		    /* Test if lat value is within range */
		    /* --------------------------------- */
		    latTest = (lat64Test >= cornerlat[0] &&
			       lat64Test <= cornerlat[1]);


		    if (bndflag == 1)
		    {
			/*
			 * If boundary flag set test whether longitude value
			 * is outside region and then flip
			 */
			lonTest = (lon64Test >= cornerlon[1] &&
				   lon64Test <= cornerlon[0]);
			lonTest = 1 - lonTest;
		    }
		    else
		    {
			lonTest = (lon64Test >= cornerlon[0] &&
				   lon64Test <= cornerlon[1]);
		    }


		    /*
		     * If both longitude and latitude are within region set
		     * recFound flag
		     */
		    if (lonTest + latTest == 2)
		    {
			recFound[nFound] = i;
			nFound++;
		    }
		}

		free(lon64);
		free(lat64);
	    }


	    /* Propogate subsetted records to other levels */
	    /* ------------------------------------------- */
	    regionID = PTrecnum(pointID, lonlev, 0, nlevels - 1,
				nFound, recFound);


	    free(recFound);
	    free(recs);
	}
    }
    return (regionID);
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTdeftimeperiod                                                  |
|                                                                             |
|  DESCRIPTION: Define time period of interest.                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  periodID       int32               Period ID                               |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               Point structure ID                      |
|  starttime      float64 TAI sec     Start of time period                    |
|  stoptime       float64 TAI sec     Stop of time period                     |
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
PTdeftimeperiod(int32 pointID, float64 starttime, float64 stoptime)
{
    intn            i;		/* Loop index */
    intn            status;	/* routine return status variable */

    int32           sizeTime = -1;	/* Size of Time field */
    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           ptVgrpID;	/* Point Vgroup ID */
    int32           nlevels;	/* Number of levels in point */
    int32           tmelev;	/* Level containing Time field */
    int32          *recs;	/* Pointer to record number array */
    int32          *recFound;	/* Pointer to record found flag array */
    int32           regionID = -1;	/* Region ID (returned) */
    int32           nFound = 0;	/* Number of records found */
    int32           nrec;	/* Number of records in geo field level */

    float64        *time64;	/* Pointer to float64 time values */
    float64         time64Test;	/* Time value to test */


    /* Check for valid point ID */
    /* ------------------------ */
    status = PTchkptid(pointID, "PTdeftimeperiod",
		       &fid, &sdInterfaceID, &ptVgrpID);


    if (status == 0)
    {
	/* Get size in bytes of "Time" field */
	/* --------------------------------- */
	sizeTime = PTsizeof(pointID, "Time", &tmelev);


	/* If "Time" field not found report error */
	/* -------------------------------------- */
	if (sizeTime == 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "PTdeftimeperiod", __FILE__, __LINE__);
	    HEreport("\"Time\" field not found.\n");
	}



	if (status == 0)
	{
	    /* Get number of levels in point */
	    /* ----------------------------- */
	    nlevels = PTnlevels(pointID);


	    /* Get number of records in time level */
	    /* ----------------------------------- */
	    nrec = PTnrecs(pointID, tmelev);


	    /* Allocate space for recs and recFound arrays */
	    /* ------------------------------------------- */
	    recs = (int32 *) calloc(nrec, 4);
	    if(recs == NULL)
	    { 
		HEpush(DFE_NOSPACE,"PTdeftimeperiod", __FILE__, __LINE__);
		return(-1);
	    }
	    recFound = (int32 *) calloc(nrec, 4);
	    if(recFound == NULL)
	    { 
		HEpush(DFE_NOSPACE,"PTdeftimeperiod", __FILE__, __LINE__);
		free(recs);
		return(-1);
	    }

	    /* Load recs array from 0 to nrec-1 */
	    /* -------------------------------- */
	    for (i = 0; i < nrec; i++)
	    {
		recs[i] = i;
	    }

	    /* Allocate space for time array */
	    /* ----------------------------- */
	    time64 = (float64 *) calloc(nrec, 8);
	    if(time64 == NULL)
	    { 
		HEpush(DFE_NOSPACE,"PTdeftimeperiod", __FILE__, __LINE__);
		free(recs);
		free(recFound);
		return(-1);
	    }

	    /* Read Time field */
	    /* --------------- */
	    status = PTreadlevel(pointID, tmelev, "Time",
				 nrec, recs, (char *) time64);



	    /* For all records in level ... */
	    /* ---------------------------- */
	    for (i = 0; i < nrec; i++)
	    {
		time64Test = time64[i];


		/* Test whether time is within time period */
		/* --------------------------------------- */
		if (time64Test >= starttime &&
		    time64Test <= stoptime)
		{
		    recFound[nFound] = i;
		    nFound++;
		}
	    }

	    free(time64);
	}


	/* Propogate subsetted records to other levels */
	/* ------------------------------------------- */
	regionID = PTrecnum(pointID, tmelev, 0, nlevels - 1,
			    nFound, recFound);

	free(recs);
	free(recFound);
    }

    return (regionID);
}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTregioninfo                                                     |
|                                                                             |
|  DESCRIPTION: Returns size of defined region.                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                status (0 - SUCCEED, -1 - FAIL)         |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               Point structure ID                      |
|  regionID       int32               region ID                               |
|  level          int32               level # (0 - based)                     |
|  fieldlist      char *              List of fields to extract               |
|                                                                             |
|  OUTPUTS:                                                                   |
|  size           int32               Size of data buffer in bytes            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Dec 96   Joel Gales    Use VSfexist to check if fields exist in pointlevel |
|  Dec 96   Joel Gales    Use VSsizeof to get size of fields in pointlevel    |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
PTregioninfo(int32 pointID, int32 regionID, int32 level, char *fieldlist,
	     int32 * size)

{
    intn            i;		/* Loop index */
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           ptVgrpID;	/* Point Vgroup ID */
    int32           idOffset = PTIDOFFSET;	/* Point ID offset */
    int32           vdataID;	/* Vdata ID */
    int32           nflds;	/* Number of fields in fieldlist */
    int32           slen[VSFIELDMAX];	/* String length array */

    char           *ptr[VSFIELDMAX];	/* String pointer array */
    char            utlbuf[256];/* Utility Buffer */


    /* Initialize region size to -1 */
    /* ---------------------------- */
    *size = -1;


    /* Check for valid point ID */
    /* ------------------------ */
    status = PTchkptid(pointID, "PTregioninfo",
		       &fid, &sdInterfaceID, &ptVgrpID);

    if (status == 0)
    {
	/* Check for valid region ID */
	/* ------------------------- */
	if (regionID < 0 || regionID >= NPOINTREGN)
	{
	    status = -1;
	    HEpush(DFE_RANGE, "PTregioninfo", __FILE__, __LINE__);
	    HEreport("Invalid Region id: %d.\n", regionID);
	}
	else
	{
	    /* Check for active region ID */
	    /* -------------------------- */
	    if (PTXRegion[regionID] != 0)
	    {
		/* Get vdata ID for point level */
		/* ---------------------------- */
		vdataID = PTXPoint[pointID % idOffset].vdID[level];


		/* Parse fieldlist and get number of fields */
		/* ---------------------------------------- */
		nflds = EHparsestr(fieldlist, ',', ptr, slen);


		/* Loop through all fields ... */
		/* --------------------------- */
		for (i = 0; i < nflds; i++)
		{
		    /* Copy field entry into utlbuf */
		    /* ---------------------------- */
		    memcpy(utlbuf, ptr[i], slen[i]);
		    utlbuf[slen[i]] = 0;


		    /* Check whether field exists in level */
		    /* ----------------------------------- */
		    if (VSfexist(vdataID, utlbuf) == -1)
		    {
			status = -1;
			HEpush(DFE_GENAPP, "PTregioninfo", __FILE__, __LINE__);
			HEreport("Field \"%s\" not in level: %d.\n",
				 utlbuf, level);
			break;
		    }
		}


		/* If no problems get size of region in bytes */
		/* ------------------------------------------ */
		if (status == 0)
		{
		    *size = VSsizeof(vdataID, fieldlist) *
			PTXRegion[regionID]->nrec[level];
		}
	    }
	    else
	    {
		/* Report Inactive region ID error */
		/* ------------------------------- */
		status = -1;
		HEpush(DFE_GENAPP, "PTregioninfo", __FILE__, __LINE__);
		HEreport("Inactive Region ID: %d.\n", regionID);
	    }
	}
    }
    return (status);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTperiodinfo                                                     |
|                                                                             |
|  DESCRIPTION: Returns size of defined time period.                          |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                status (0 - SUCCEED, -1 - FAIL)         |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               Point structure ID                      |
|  periodID       int32               period ID                               |
|  level          int32               level # (0 - based)                     |
|  fieldlist      char *              List of fields to extract               |
|                                                                             |
|  OUTPUTS:                                                                   |
|  size           int32               Size of data buffer in bytes            |
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
PTperiodinfo(int32 pointID, int32 periodID, int32 level, char *fieldlist,
	     int32 * size)
{
    intn            status = 0;	/* routine return status variable */


    /* Call PTregioninfo routine */
    /* ------------------------- */
    status = PTregioninfo(pointID, periodID, level, fieldlist, size);

    return (status);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTregionrecs                                                     |
|                                                                             |
|  DESCRIPTION: Returns number of records and record #s in defined region.    |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                status (0 - SUCEED, -1 - FAIL)          |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               Point structure ID                      |
|  regionID       int32               region ID                               |
|  level          int32               level # (0 - based)                     |
|                                                                             |
|  OUTPUTS:                                                                   |
|  nrecs          int32               Number of recs in region and level      |
|  recs           int32               Record numbers in region and level      |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Dec 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
PTregionrecs(int32 pointID, int32 regionID, int32 level, int32 * nrec,
	     int32 recs[])
{
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           ptVgrpID;	/* Point Vgroup ID */


    /* Initialize number of records to -1 */
    /* ---------------------------------- */
    *nrec = -1;


    /* Check for valid point ID */
    /* ------------------------ */
    status = PTchkptid(pointID, "PTregionrecs",
		       &fid, &sdInterfaceID, &ptVgrpID);

    if (status == 0)
    {
	/* Check for valid region ID */
	/* ------------------------- */
	if (regionID < 0 || regionID >= NPOINTREGN)
	{
	    status = -1;
	    HEpush(DFE_RANGE, "PTregioninfo", __FILE__, __LINE__);
	    HEreport("Invalid Region id: %d.\n", regionID);
	}
	else
	{
	    /* Check for active region ID */
	    /* -------------------------- */
	    if (PTXRegion[regionID] != 0)
	    {

		/* Return number of records */
		/* ------------------------ */
		*nrec = PTXRegion[regionID]->nrec[level];


		/* Return record numbers if requested */
		/* ---------------------------------- */
		if (recs != NULL)
		{
		    memcpy(recs, PTXRegion[regionID]->recPtr[level],
			   *nrec * sizeof(int32));
		}
	    }
	    else
	    {
		/* Report Inactive region ID error */
		/* ------------------------------- */
		status = -1;
		HEpush(DFE_GENAPP, "PTregioninfo", __FILE__, __LINE__);
		HEreport("Inactive Region ID: %d.\n", regionID);
	    }
	}
    }
    return (status);
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTperiodrecs                                                     |
|                                                                             |
|  DESCRIPTION: Returns number of records and record #s in defined period.    |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                status (0 - SUCEED, -1 - FAIL)          |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               Point structure ID                      |
|  periodID       int32               period ID                               |
|  level          int32               level # (0 - based)                     |
|                                                                             |
|  OUTPUTS:                                                                   |
|  nrecs          int32               Number of recs in period and level      |
|  recs           int32               Record numbers in period and level      |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Dec 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
PTperiodrecs(int32 pointID, int32 periodID, int32 level, int32 * nrec,
	     int32 recs[])
{
    intn            status = 0;	/* routine return status variable */


    /* Call PTregionrecs routine */
    /* ------------------------- */
    status = PTregionrecs(pointID, periodID, level, nrec, recs);

    return (status);
}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTextractregion                                                  |
|                                                                             |
|  DESCRIPTION: Reads a region of interest  from a set of fields in a single  |
|               level.                                                        |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                status (0 - SUCCEED, -1 - FAIL)         |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               Point structure ID                      |
|  regionID       int32               region ID                               |
|  level          int32               level # (0 - based)                     |
|  fieldlist      char *              List of fields to extract               |
|                                                                             |
|  OUTPUTS:                                                                   |
|  buffer         void *              Buffer to read data                     |
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
PTextractregion(int32 pointID, int32 regionID, int32 level, char *fieldlist,
		VOIDP buffer)

{
    intn            i;		/* Loop index */
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           ptVgrpID;	/* Point Vgroup ID */
    int32           idOffset = PTIDOFFSET;	/* Point ID offset */
    int32           vdataID;	/* Vdata ID */
    int32           nflds;	/* Number of fields in fieldlist */
    int32           slen[VSFIELDMAX];	/* String length array */
    int32           nrec;
    int32          *recs;

    char           *ptr[VSFIELDMAX];	/* String pointer array */
    char            utlbuf[256];/* Utility Buffer */



    /* Check for valid point ID */
    /* ------------------------ */
    status = PTchkptid(pointID, "PTextractregion",
		       &fid, &sdInterfaceID, &ptVgrpID);

    if (status == 0)
    {
	/* Check for valid region ID */
	/* ------------------------- */
	if (regionID < 0 || regionID >= NPOINTREGN)
	{
	    status = -1;
	    HEpush(DFE_RANGE, "PTextractregion", __FILE__, __LINE__);
	    HEreport("Invalid Region id: %d.\n", regionID);
	}
	else
	{
	    /* Check for active region ID */
	    /* -------------------------- */
	    if (PTXRegion[regionID] != 0)
	    {
		/* Get vdata ID for point level */
		/* ---------------------------- */
		vdataID = PTXPoint[pointID % idOffset].vdID[level];


		/* Parse fieldlist and get number of fields */
		/* ---------------------------------------- */
		nflds = EHparsestr(fieldlist, ',', ptr, slen);


		/* Loop through all fields ... */
		/* --------------------------- */
		for (i = 0; i < nflds; i++)
		{
		    /* Copy field entry into utlbuf */
		    /* ---------------------------- */
		    memcpy(utlbuf, ptr[i], slen[i]);
		    utlbuf[slen[i]] = 0;


		    /* Check whether field exists in level */
		    /* ----------------------------------- */
		    if (VSfexist(vdataID, utlbuf) == -1)
		    {
			status = -1;
			HEpush(DFE_GENAPP, "PTextractregion",
			       __FILE__, __LINE__);
			HEreport("Field \"%s\" not in level: %d.\n",
				 utlbuf, level);
			break;
		    }
		}


		/* If no problems ... */
		/* ------------------ */
		if (status == 0)
		{
		    /* Get number of records in region and record numbers */
		    /* -------------------------------------------------- */
		    nrec = PTXRegion[regionID]->nrec[level];
		    recs = PTXRegion[regionID]->recPtr[level];


		    /* Read fields */
		    /* ----------- */
		    status = PTreadlevel(pointID, level, fieldlist,
					 nrec, recs, buffer);
		}

	    }
	    else
	    {
		/* Report Inactive region ID error */
		/* ------------------------------- */
		status = -1;
		HEpush(DFE_GENAPP, "PTextractregion", __FILE__, __LINE__);
		HEreport("Inactive Region ID: %d.\n", regionID);
	    }
	}
    }
    return (status);
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTextractperiod                                                  |
|                                                                             |
|  DESCRIPTION: Extract data from level record whose times are within a       |
|               given period as defined by PTdeftimeperiod.                   |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                status (0 - SUCCEED, -1 - FAIL)         |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               Point structure ID                      |
|  periodID       int32               Time period  ID                         |
|  level          int32               level # (0 - based)                     |
|  fieldlist      char *              List of fields to extract               |
|                                                                             |
|  OUTPUTS:                                                                   |
|  buffer         void *              Buffer to read data                     |
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
PTextractperiod(int32 pointID, int32 periodID, int32 level, char *fieldlist,
		VOIDP buffer)
{
    intn            status = 0;	/* routine return status variable */


    /* Call PTextractregion */
    /* -------------------- */
    status = PTextractregion(pointID, periodID, level, fieldlist,
			     (char *) buffer);

    return (status);
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTdefvrtregion                                                   |
|                                                                             |
|  DESCRIPTION: Find records within range                                     |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  regionID       int32               Period ID                               |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               Point structure ID                      |
|  regionID       int32               Region ID                               |
|  fieldname      char                Field to subset                         |
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
|  Jun 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
PTdefvrtregion(int32 pointID, int32 regionID, char *fieldname, float64 range[])
{
    intn            i;		/* Loop index */
    intn            status;	/* routine return status variable */

    int16           vertINT16 = 0;	/* Temporary INT16 variable */
    uint16          vertUINT16 = 0;	/* Temporary UINT16 variable */

    int32           sizeFld = -1;	/* Size of field */
    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           ptVgrpID;	/* Point Vgroup ID */
    int32           nlevels;	/* Number of levels in point */
    int32           fldlev;	/* Level containing field */
    int32           nflds;	/* Number of fields in level */
    int32           strbufsize;	/* Fieldlist string size */
    int32          *recs = (int32 *)NULL;	/* Pointer to record number array */
    int32          *recFound = (int32 *)NULL;	/* Pointer to record found flag array */
    int32          *fldtype = (int32 *)NULL;	/* Pointer to field type array */
    int32          *fldorder = (int32 *)NULL;	/* Pointer to field order array */
    int32           nFound = 0;	/* Number of records found */
    int32           nrec;	/* Number of records in geo field level */
    int32           size;	/* Size of fieldtype in bytes */
    int32           dum;	/* Dummy variable */
    int32           tmpregionID;/* Temporary region ID */
    int32           vertINT32 = 0;	/* Temporary INT32 variable */

    float32         vertFLOAT32 = 0.0;/* Temporary FLOAT32 variable */

    float64         vertFLOAT64 = 0.0;/* Temporary FLOAT64 variable */

    char           *fieldlist = (char *)NULL;	/* Level fieldlist */
    char           *vertArr = (char *)NULL;	/* Pointer to vertical field data buffer */


    /* Check for valid point ID */
    /* ------------------------ */
    status = PTchkptid(pointID, "PTdefvrtregion",
		       &fid, &sdInterfaceID, &ptVgrpID);


    if (status == 0)
    {
	/* Get size in bytes of field */
	/* -------------------------- */
	sizeFld = PTsizeof(pointID, fieldname, &fldlev);


	/* If field not found report error */
	/* ------------------------------- */
	if (sizeFld == 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "PTdefvrtregion", __FILE__, __LINE__);
	    HEreport("\"%s\" field not found.\n", fieldname);
	}


	if (status == 0)
	{
	    /* Get number of levels in point */
	    /* ----------------------------- */
	    nlevels = PTnlevels(pointID);


	    /* Load recs array from 0 to nrec-1 if new region */
	    /* ---------------------------------------------- */
	    if (regionID == -1)
	    {
		/* Get number of records in field level */
		/* ------------------------------------ */
		nrec = PTnrecs(pointID, fldlev);

		/* Allocate space for recs and recFound arrays */
		/* ------------------------------------------- */
		recs = (int32 *) calloc(nrec, 4);
		if(recs == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"PTdefvrtregion", __FILE__, __LINE__);
		    return(-1);
		}
		
		recFound = (int32 *) calloc(nrec, 4);
		if(recFound == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"PTdefvrtregion", __FILE__, __LINE__);
		    free(recs);
		    return(-1);
		}

		for (i = 0; i < nrec; i++)
		{
		    recs[i] = i;
		}
	    }
	    /* else load recs from region structure */
	    /* ------------------------------------ */
	    else
	    {
		nrec = PTXRegion[regionID]->nrec[fldlev];

		/* Allocate space for recs and recFound arrays */
		/* ------------------------------------------- */
		recs = (int32 *) calloc(nrec, 4);
		if(recs == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"PTdefvrtregion", __FILE__, __LINE__);
		    return(-1);
		}
		
		recFound = (int32 *) calloc(nrec, 4);
		if(recFound == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"PTdefvrtregion", __FILE__, __LINE__);
		    free(recs);
		    return(-1);
		}

		for (i = 0; i < nrec; i++)
		{
		    recs[i] = *(PTXRegion[regionID]->recPtr[fldlev] + i);
		}
	    }


	    /* Get information about vertical field */
	    /* ------------------------------------ */
	    nflds = PTnfields(pointID, fldlev, &strbufsize);
	    fieldlist = (char *) calloc(strbufsize + 1, 1);
	    if(fieldlist == NULL)
	    { 
		HEpush(DFE_NOSPACE,"PTdefvrtregion", __FILE__, __LINE__);
		free(recs);
		return(-1);
	    }
	    fldtype = (int32 *) calloc(nflds, 4);
	    if(fldtype == NULL)
	    { 
		HEpush(DFE_NOSPACE,"PTdefvrtregion", __FILE__, __LINE__);
		free(fieldlist);
		return(-1);
	    }
	    fldorder = (int32 *) calloc(nflds, 4);
	    if(fldorder == NULL)
	    { 
		HEpush(DFE_NOSPACE,"PTdefvrtregion", __FILE__, __LINE__);
		free(fieldlist);
		free(fldtype);
		return(-1);
	    }
	    nflds = PTlevelinfo(pointID, fldlev, fieldlist, fldtype, fldorder);
	    dum = EHstrwithin(fieldname, fieldlist, ',');


	    /* Check for supported field type */
	    /* ------------------------------ */
	    if (fldtype[dum] != DFNT_INT16 &&
		fldtype[dum] != DFNT_UINT16 &&
		fldtype[dum] != DFNT_INT32 &&
		fldtype[dum] != DFNT_FLOAT32 &&
		fldtype[dum] != DFNT_FLOAT64)
	    {
		status = -1;
		HEpush(DFE_GENAPP, "PTdefvrtregion", __FILE__, __LINE__);
		HEreport("Fieldtype: %d not supported for vertical subsetting.\n", fldtype[dum]);
	    }



	    /* Check that field not array */
	    /* -------------------------- */
	    if (fldorder[dum] > 1)
	    {
		status = -1;
		HEpush(DFE_GENAPP, "PTdefvrtregion", __FILE__, __LINE__);
		HEreport("Vertical field cannot be array.\n");
	    }



	    if (status == 0)
	    {
		/* Get size in bytes of vertical field numbertype */
		/* ---------------------------------------------- */
		size = DFKNTsize(fldtype[dum]);


		/* Allocate space for vertical field */
		/* --------------------------------- */
		vertArr = (char *) calloc(nrec, size);
		if(vertArr == NULL)
		{ 
		    HEpush(DFE_NOSPACE,"PTdefvrtregion", __FILE__, __LINE__);
		    return(-1);
		}

		/* Read vertical field */
		/* ------------------- */
		status = PTreadlevel(pointID, fldlev, fieldname,
				     nrec, recs, vertArr);


		switch (fldtype[dum])
		{
		case DFNT_INT16:

		    /* For all records in level ... */
		    /* ---------------------------- */
		    for (i = 0; i < nrec; i++)
		    {
			/* Get single element of vertical field */
			/* ------------------------------------ */
			memcpy(&vertINT16, vertArr + i * size, size);


			/* If within range ... */
			/* ------------------- */
			if (vertINT16 >= range[0] &&
			    vertINT16 <= range[1])
			{
			    recFound[nFound] = recs[i];
			    nFound++;
			}
		    }
		    break;


		case DFNT_UINT16:

		    /* For all records in level ... */
		    /* ---------------------------- */
		    for (i = 0; i < nrec; i++)
		    {
			/* Get single element of vertical field */
			/* ------------------------------------ */
			memcpy(&vertUINT16, vertArr + i * size, size);


			/* If within range ... */
			/* ------------------- */
			if (vertUINT16 >= range[0] &&
			    vertUINT16 <= range[1])
			{
			    recFound[nFound] = recs[i];
			    nFound++;
			}
		    }
		    break;


		case DFNT_INT32:

		    /* For all records in level ... */
		    /* ---------------------------- */
		    for (i = 0; i < nrec; i++)
		    {
			/* Get single element of vertical field */
			/* ------------------------------------ */
			memcpy(&vertINT32, vertArr + i * size, size);


			/* If within range ... */
			/* ------------------- */
			if (vertINT32 >= range[0] &&
			    vertINT32 <= range[1])
			{
			    recFound[nFound] = recs[i];
			    nFound++;
			}
		    }
		    break;



		case DFNT_FLOAT32:

		    /* For all records in level ... */
		    /* ---------------------------- */
		    for (i = 0; i < nrec; i++)
		    {
			/* Get single element of vertical field */
			/* ------------------------------------ */
			memcpy(&vertFLOAT32, vertArr + i * size, size);


			/* If within range ... */
			/* ------------------- */
			if (vertFLOAT32 >= range[0] &&
			    vertFLOAT32 <= range[1])
			{
			    recFound[nFound] = recs[i];
			    nFound++;
			}
		    }
		    break;



		case DFNT_FLOAT64:

		    /* For all records in level ... */
		    /* ---------------------------- */
		    for (i = 0; i < nrec; i++)
		    {
			/* Get single element of vertical field */
			/* ------------------------------------ */
			memcpy(&vertFLOAT64, vertArr + i * size, size);


			/* If within range ... */
			/* ------------------- */
			if (vertFLOAT64 >= range[0] &&
			    vertFLOAT64 <= range[1])
			{
			    recFound[nFound] = recs[i];
			    nFound++;
			}
		    }
		    break;



		}


		/* Propagate subsetted records to other levels */
		/* ------------------------------------------- */
		tmpregionID = PTrecnum(pointID, fldlev, 0, nlevels - 1,
				       nFound, recFound);


		if (regionID != -1)
		{
		    /* Copy old region structure data to new region */
		    /* -------------------------------------------- */
		    *PTXRegion[regionID] = *PTXRegion[tmpregionID];


		    /* Free temp region */
		    /* ---------------- */
		    for (i = 0; i < 8; i++)
		    {
			if (PTXRegion[tmpregionID]->recPtr[i] != 0)
			{
			    /* Copy record pointers */
			    /* -------------------- */
			    nrec = PTXRegion[regionID]->nrec[i];
			    PTXRegion[regionID]->recPtr[i] =
				(int32 *) calloc(nrec, 4);
			    if(PTXRegion[regionID]->recPtr[i] == NULL)
			    { 
				HEpush(DFE_NOSPACE,"PTdefvrtregion", __FILE__, __LINE__);
				return(-1);
			    }
			    memcpy(PTXRegion[regionID]->recPtr[i],
				   PTXRegion[tmpregionID]->recPtr[i],
				   4 * nrec);

			    free(PTXRegion[tmpregionID]->recPtr[i]);
			}
		    }
		    free(PTXRegion[tmpregionID]);
		    PTXRegion[tmpregionID] = 0;
		}
		else
		{
		    /* If no initial region ID make temp ID permanent */
		    /* ---------------------------------------------- */
		    regionID = tmpregionID;
		}


		free(vertArr);
	    }

	    free(fieldlist);
	    free(fldtype);
	    free(fldorder);

	    free(recs);
	    free(recFound);
	}


	/* Set regionID to -1 if bad return status */
	/* --------------------------------------- */
	if (status == -1)
	{
	    regionID = -1;
	}

    }
	return (regionID);
}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTdetach                                                         |
|                                                                             |
|  DESCRIPTION: Releases a point data set and frees memory.                   |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  pointID        int32               point structure ID                      |
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
|  May 97   Joel Gales    Check for non-null region record pointers before    |
|                         freeing.                                            |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
PTdetach(int32 pointID)

{
    intn            j;		/* Loop index */
    intn            k;		/* Loop index */
    intn            status = 0;	/* routine return status variable */

    int32           fid;	/* HDF-EOS file id */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           idOffset = PTIDOFFSET;	/* Point ID offset */
    int32           ptVgrpID;	/* Point Vgroup ID */
    int32           nlevels;	/* Number of levels in point */
    int32           pID;	/* Point ID - offset */


    /* Check for valid point ID */
    /* ------------------------ */
    status = PTchkptid(pointID, "PTdetach", &fid, &sdInterfaceID, &ptVgrpID);

    if (status == 0)
    {
	/* Get number of levels and "reduced point ID */
	/* ------------------------------------------ */
	nlevels = PTnlevels(pointID);
	pID = pointID % idOffset;


	/* Detach Point Vdatas */
	/* ------------------- */
	for (j = 0; j < nlevels; j++)
	{
	    VSdetach(PTXPoint[pID].vdID[j]);
	}


	/* Detach Point Vgroups */
	/* -------------------- */
	Vdetach(PTXPoint[pID].VIDTable[0]);
	Vdetach(PTXPoint[pID].VIDTable[1]);
	Vdetach(PTXPoint[pID].VIDTable[2]);
	Vdetach(PTXPoint[pID].IDTable);



	/* Clear entries from external arrays */
	/* ---------------------------------- */
	PTXPoint[pID].active = 0;
	PTXPoint[pID].VIDTable[0] = 0;
	PTXPoint[pID].VIDTable[1] = 0;
	PTXPoint[pID].VIDTable[2] = 0;
	PTXPoint[pID].IDTable = 0;
	PTXPoint[pID].fid = 0;
	for (j = 0; j < nlevels; j++)
	{
	    PTXPoint[pID].vdID[j] = 0;
	}



	/* Free Region Pointers */
	/* -------------------- */
	for (k = 0; k < NPOINTREGN; k++)
	{
	    if (PTXRegion[k] != 0 &&
		PTXRegion[k]->pointID == pointID)
	    {
		for (j = 0; j < 8; j++)
		{
		    if (PTXRegion[k]->recPtr[j] != 0)
		    {
			free(PTXRegion[k]->recPtr[j]);
		    }
		}

		free(PTXRegion[k]);
		PTXRegion[k] = 0;
	    }
	}

    }
    return (status);
}

/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: PTclose                                                          |
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
PTclose(int32 fid)

{
    intn            status = 0;	/* routine return status variable */

    /* Call EHclose to perform file close */
    /* ---------------------------------- */
    status = EHclose(fid);

    return (status);
}


/* HDF types used in FORTRAN bindings */

#if defined(DEC_ALPHA) || defined(IRIX) || defined(UNICOS) || defined(LINUX64) || defined(IA64) || defined(MACINTOSH) || defined(IBM6000)

#define INT32  INT
#define INT32V INTV
#define PINT32 PINT

#else

#define INT32  LONG
#define INT32V LONGV
#define PINT32 PLONG

#endif

/* FORTRAN bindings */

FCALLSCFUN2(INT32, PTopen, PTOPEN, ptopen, STRING, INT)
FCALLSCFUN2(INT32, PTcreate, PTCREATE, ptcreate, INT32, STRING)
FCALLSCFUN2(INT32, PTnrecs, PTNRECS, ptnrecs, INT32, INT32)
FCALLSCFUN1(INT32, PTnlevels, PTNLEVS, ptnlevs, INT32)
FCALLSCFUN3(INT32, PTsizeof, PTSIZEOF, ptsizeof, INT32, STRING,
	    INT32V)
FCALLSCFUN3(INT32, PTnfields, PTNFLDS, ptnflds, INT32, INT32,
	    PINT32)
FCALLSCFUN2(INT32, PTlevelindx, PTLEVIDX, ptlevidx, INT32, STRING)
FCALLSCFUN2(INT32, PTattach, PTATTACH, ptattach, INT32, STRING)
FCALLSCFUN5(INT, PTdeflevel, PTDEFLEV, ptdeflev, INT32, STRING, STRING,
	    INT32V, INT32V)
FCALLSCFUN4(INT, PTdeflinkage, PTDEFLINK, ptdeflink, INT32, STRING, STRING,
	    STRING)
FCALLSCFUN3(INT, PTbcklinkinfo, PTBLINKINFO, ptblinkinfo, INT32, INT32,
	    PSTRING)
FCALLSCFUN3(INT, PTfwdlinkinfo, PTFLINKINFO, ptflinkinfo, INT32, INT32,
	    PSTRING)
FCALLSCFUN5(INT, PTlevelinfo, PTLEVINFO, ptlevinfo, INT32, INT32,
	    PSTRING,
	    INT32V, INT32V)
FCALLSCFUN4(INT, PTwritelevel, PTWRLEV, ptwrlev, INT32, INT32,
	    INT32, PVOID)
FCALLSCFUN6(INT, PTupdatelevel, PTUPLEV, ptuplev, INT32, INT32, STRING,
	    INT32, INT32V, PVOID)
FCALLSCFUN6(INT, PTreadlevel, PTRDLEV, ptrdlev, INT32, INT32, STRING,
	    INT32, INT32V, PVOID)
FCALLSCFUN5(INT, PTwriteattr, PTWRATTR, ptwrattr, INT32, STRING, INT32,
	    INT32, PVOID)
FCALLSCFUN3(INT, PTreadattr, PTRDATTR, ptrdattr, INT32, STRING, PVOID)
FCALLSCFUN3(INT32, PTinqpoint, PTINQPOINT, ptinqpoint, STRING, PSTRING,
	    PINT32)
FCALLSCFUN4(INT, PTattrinfo, PTATTRINFO, ptattrinfo, INT32, STRING,
	    PINT32, PINT32)
FCALLSCFUN3(INT32, PTinqattrs, PTINQATTRS, ptinqattrs, INT32, PSTRING,
	    PINT32)
FCALLSCFUN7(INT, PTgetrecnums, PTGETRECNUMS, ptgetrecnums, INT32, INT32,
	    INT32, INT32, INT32V, PINT32, INT32V)
FCALLSCFUN4(INT, PTgetlevelname, PTGETLEVNAME, ptgetlevname, INT32,
	    INT32, PSTRING, PINT32)
FCALLSCFUN3(INT32, PTdefboxregion, PTDEFBOXREG, ptdefboxreg, INT32,
	    DOUBLEV, DOUBLEV)
FCALLSCFUN5(INT, PTregioninfo, PTREGINFO, ptreginfo, INT32, INT32,
	    INT32, STRING, PINT32)
FCALLSCFUN5(INT, PTextractregion, PTEXTREG, ptextreg, INT32, INT32,
	    INT32, STRING, PVOID)
FCALLSCFUN3(INT32, PTdeftimeperiod, PTDEFTMEPER, ptdeftmeper, INT32,
	    DOUBLE, DOUBLE)
FCALLSCFUN4(INT32, PTdefvrtregion, PTDEFVRTREG, ptdefvrtreg, INT32,
	    INT32, STRING, DOUBLEV)
FCALLSCFUN5(INT, PTperiodinfo, PTPERINFO, ptperinfo, INT32, INT32,
	    INT32, STRING, PINT32)
FCALLSCFUN5(INT, PTextractperiod, PTEXTPER, ptextper, INT32, INT32,
	    INT32, STRING, PVOID)
FCALLSCFUN5(INT, PTregionrecs, PTREGRECS, ptregrecs, INT32, INT32,
	    INT32, PINT32, INT32V)
FCALLSCFUN5(INT, PTperiodrecs, PTPERRECS, ptperrecs, INT32, INT32,
	    INT32, PINT32, INT32V)
FCALLSCFUN1(INT, PTdetach, PTDETACH, ptdetach, INT32)
FCALLSCFUN1(INT, PTclose, PTCLOSE, ptclose, INT32)
