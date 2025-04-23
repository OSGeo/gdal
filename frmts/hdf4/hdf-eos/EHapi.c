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

#include "cpl_port.h"
#include <errno.h>
#include "mfhdf.h"
#include "HdfEosDef.h"

/* Set maximum number of HDF-EOS files to HDF limit (MAX_FILE) */
#define NEOSHDF MAX_FILE
static intn  EHXmaxfilecount = 0;
static uint8 *EHXtypeTable = NULL;
static uint8 *EHXacsTable = NULL;
static int32 *EHXfidTable = NULL;
static int32 *EHXsdTable = NULL;

/* define a macro for the string size of the utility strings and some dimension
   list strings. The value in previous versions of this code may not be
   enough in some cases. The length now is 512 which seems to be more than
   enough to hold larger strings. */

#define UTLSTR_MAX_SIZE 512
#define UTLSTRSIZE  32000

#define EHIDOFFSET 524288

#define HDFEOSVERSION 2.12
#define HDFEOSVERSION1 "2.12"
#include "HDFEOSVersion.h"

#define MAX_RETRIES 10

/* Function Prototypes */
static intn EHreset_maxopenfiles(intn);
static intn EHget_maxopenfiles(intn *, intn *);
static intn EHget_numfiles(void);

/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: EHopen                                                           |
|                                                                             |
|  DESCRIPTION: Opens HDF-EOS file and returns file handle                    |
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
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Jul 96   Joel Gales    Add file id offset EHIDOFFSET                       |
|  Aug 96   Joel Gales    Add "END" statement to structural metadata          |
|  Sep 96   Joel Gales    Reverse order of Hopen ane SDstart statements       |
|                         for RDWR and READ access                            |
|  Oct 96   Joel Gales    Trap CREATE & RDWR (no write permission)            |
|                         access errors                                       |
|  Apr 97   Joel Gales    Fix problem with RDWR open when file previously     |
|                         open for READONLY access                            |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
EHopen(const char *filename, intn access)

{
    intn            i;		/* Loop index */
    intn            status = 0;	/* routine return status variable */
    intn            dum;	/* Dummy variable */
    intn            curr_max = 0;	/* maximum # of HDF files to open */
    intn            sys_limit = 0;	/* OS limit for maximum # of opened files */

    int32           HDFfid = 0;	/* HDF file ID */
    int32           fid = -1;	/* HDF-EOS file ID */
    int32           sdInterfaceID = 0;	/* HDF SDS interface ID */
    int32           attrIndex;	/* Structural Metadata attribute index */

    uint8           acs = 0;	/* Read (0) / Write (1) access code */

    char           *testname;	/* Test filename */
    char            errbuf[256];/* Error report buffer */
    char           *metabuf;	/* Pointer to structural metadata buffer */
    char            hdfeosVersion[32];	/* HDFEOS version string */

    intn            retryCount;

    /* Request the system allowed number of opened files */
    /* and increase HDFEOS file tables to the same size  */
    /* ------------------------------------------------- */
    if (EHget_maxopenfiles(&curr_max, &sys_limit) >= 0
        && curr_max < sys_limit)
    {
        if (EHreset_maxopenfiles(sys_limit) < 0)
        {
	    HEpush(DFE_ALROPEN, "EHopen", __FILE__, __LINE__);
	    HEreport("Can't set maximum opened files number to \"%d\".\n", curr_max);
	    return -1;
        }
    }

    /* Setup file interface */
    /* -------------------- */
    if (EHget_numfiles() < EHXmaxfilecount)
    {

	/*
	 * Check that file has not been previously opened for write access if
	 * current open request is not READONLY
	 */
	if (access != DFACC_READ)
	{
	    /* Loop through all files */
	    /* ---------------------- */
	    for (i = 0; i < EHXmaxfilecount; i++)
	    {
		/* if entry is active file opened for write access ... */
		/* --------------------------------------------------- */
		if (EHXtypeTable[i] != 0 && EHXacsTable[i] == 1)
		{
		    /* Get filename (testname) */
		    /* ----------------------- */
		    Hfidinquire(EHXfidTable[i], &testname, &dum, &dum);


		    /* if same as filename then report error */
		    /* ------------------------------------- */
		    if (strcmp(testname, filename) == 0)
		    {
			status = -1;
			fid = -1;
			HEpush(DFE_ALROPEN, "EHopen", __FILE__, __LINE__);
			HEreport("\"%s\" already open.\n", filename);
			break;
		    }
		}
	    }
	}
	if (status == 0)
	{
	    /* Create HDF-EOS file */
	    /* ------------------- */
	    switch (access)
	    {
	    case DFACC_CREATE:

		/* Get SDS interface ID */
		/* -------------------- */
		sdInterfaceID = SDstart(filename, DFACC_CREATE);

		/* If SDstart successful ... */
		/* ------------------------- */
		if (sdInterfaceID != -1)
		{
		    /* Set HDFEOS version number in file */
		    /* --------------------------------- */
		    snprintf(hdfeosVersion, sizeof(hdfeosVersion), "%s%s", "HDFEOS_V",
			    HDFEOSVERSION1);
		    SDsetattr(sdInterfaceID, "HDFEOSVersion", DFNT_CHAR8,
			      (int)strlen(hdfeosVersion), hdfeosVersion);


		    /* Get HDF file ID */
		    /* --------------- */
		    HDFfid = Hopen(filename, DFACC_RDWR, 0);

		    /* Set open access to write */
		    /* ------------------------ */
		    acs = 1;

		    /* Setup structural metadata */
		    /* ------------------------- */
		    metabuf = (char *) calloc(32000, 1);
		    if(metabuf == NULL)
		    {
			HEpush(DFE_NOSPACE,"EHopen", __FILE__, __LINE__);
			return(-1);
		    }

		    strcpy(metabuf, "GROUP=SwathStructure\n");
		    strcat(metabuf, "END_GROUP=SwathStructure\n");
		    strcat(metabuf, "GROUP=GridStructure\n");
		    strcat(metabuf, "END_GROUP=GridStructure\n");
		    strcat(metabuf, "GROUP=PointStructure\n");
		    strcat(metabuf, "END_GROUP=PointStructure\n");
		    strcat(metabuf, "END\n");

		    /* Write Structural metadata */
		    /* ------------------------- */
		    SDsetattr(sdInterfaceID, "StructMetadata.0",
			      DFNT_CHAR8, 32000, metabuf);
		    free(metabuf);
		} else
		{
		    /* If error in SDstart then report */
		    /* ------------------------------- */
		    fid = -1;
		    status = -1;
		    HEpush(DFE_FNF, "EHopen", __FILE__, __LINE__);
		    snprintf(errbuf, sizeof(errbuf), "%s%s%s", "\"", filename,
			    "\" cannot be created.");
		    HEreport("%s\n", errbuf);
		}

		break;

		/* Open existing HDF-EOS file for read/write access */
		/* ------------------------------------------------ */
	    case DFACC_RDWR:

		/* Get HDF file ID */
		/* --------------- */
#ifndef _PGS_OLDNFS
/* The following loop around the function Hopen is intended to deal with the NFS cache
   problem when opening file fails with errno = 150 or 151. When NFS cache is updated,
   this part of change is no longer necessary.              10/18/1999   */
                retryCount = 0;
                HDFfid = -1;
                while ((HDFfid == -1) && (retryCount < MAX_RETRIES))
                {
                HDFfid = Hopen(filename, DFACC_RDWR, 0);
                if((HDFfid == -1) && (errno == 150 || errno == 151))
                    {
                    HEpush(DFE_FNF, "EHopen", __FILE__, __LINE__);
                    snprintf(errbuf, sizeof(errbuf), "\"%s\" cannot be opened for READ/WRITE access, will retry %d times.", filename,  (MAX_RETRIES - retryCount - 1));
                    HEreport("%s\n", errbuf);
                    }
                retryCount++;
                }
#else
                HDFfid = Hopen(filename, DFACC_RDWR, 0);
#endif

		/* If Hopen successful ... */
		/* ----------------------- */
		if (HDFfid != -1)
		{
		    /* Get SDS interface ID */
		    /* -------------------- */
		    sdInterfaceID = SDstart(filename, DFACC_RDWR);

                    /* If SDstart successful ... */
                    /* ------------------------- */
                    if (sdInterfaceID != -1)
                    {
                       /* Set HDFEOS version number in file */
                       /* --------------------------------- */

		      attrIndex = SDfindattr(sdInterfaceID, "HDFEOSVersion");
		      if (attrIndex == -1)
			{
			  snprintf(hdfeosVersion, sizeof(hdfeosVersion), "%s%s", "HDFEOS_V",
				  HDFEOSVERSION1);
			  SDsetattr(sdInterfaceID, "HDFEOSVersion", DFNT_CHAR8,
				    (int)strlen(hdfeosVersion), hdfeosVersion);
			}
		       /* Set open access to write */
		       /* ------------------------ */
		       acs = 1;

		       /* Get structural metadata attribute ID */
		       /* ------------------------------------ */
		       attrIndex = SDfindattr(sdInterfaceID, "StructMetadata.0");

		       /* Write structural metadata if it doesn't exist */
		       /* --------------------------------------------- */
		       if (attrIndex == -1)
		       {
			  metabuf = (char *) calloc(32000, 1);
			  if(metabuf == NULL)
			  {
			      HEpush(DFE_NOSPACE,"EHopen", __FILE__, __LINE__);
			      return(-1);
			  }

			  strcpy(metabuf, "GROUP=SwathStructure\n");
			  strcat(metabuf, "END_GROUP=SwathStructure\n");
			  strcat(metabuf, "GROUP=GridStructure\n");
			  strcat(metabuf, "END_GROUP=GridStructure\n");
			  strcat(metabuf, "GROUP=PointStructure\n");
			  strcat(metabuf, "END_GROUP=PointStructure\n");
			  strcat(metabuf, "END\n");

			  SDsetattr(sdInterfaceID, "StructMetadata.0",
				  DFNT_CHAR8, 32000, metabuf);
			  free(metabuf);
		       }
                    } else
                    {
                        /* If error in SDstart then report */
                        /* ------------------------------- */
                        fid = -1;
                        status = -1;
                        HEpush(DFE_FNF, "EHopen", __FILE__, __LINE__);
                        snprintf(errbuf, sizeof(errbuf), "%s%s%s", "\"", filename,
                            "\" cannot be opened for read/write access.");
                        HEreport("%s\n", errbuf);
                    }
		} else
		{
		    /* If error in Hopen then report */
		    /* ----------------------------- */
		    fid = -1;
		    status = -1;
		    HEpush(DFE_FNF, "EHopen", __FILE__, __LINE__);
		    snprintf(errbuf, sizeof(errbuf), "%s%s%s", "\"", filename,
			    "\" cannot be opened for RDWR access.");
		    HEreport("%s\n", errbuf);
		}

		break;


		/* Open existing HDF-EOS file for read-only access */
		/* ----------------------------------------------- */
	    case DFACC_READ:

		/* Get HDF file ID */
		/* --------------- */
#ifndef _PGS_OLDNFS
/* The following loop around the function Hopen is intended to deal with the NFS cache
   problem when opening file fails with errno = 150 or 151. When NFS cache is updated,
   this part of change is no longer necessary.              10/18/1999   */
                retryCount = 0;
                HDFfid = -1;
                while ((HDFfid == -1) && (retryCount < MAX_RETRIES))
                {
                HDFfid = Hopen(filename, DFACC_READ, 0);
                if((HDFfid == -1) && (errno == 150 || errno == 151))
                    {
                    HEpush(DFE_FNF, "EHopen", __FILE__, __LINE__);
                    snprintf(errbuf, sizeof(errbuf), "\"%s\" cannot be opened for READONLY access, will retry %d times.", filename,  (MAX_RETRIES - retryCount - 1));
                    HEreport("%s\n", errbuf);
                    }
                retryCount++;
                }
#else
                HDFfid = Hopen(filename, DFACC_READ, 0);
#endif

		/* If file does not exist report error */
		/* ----------------------------------- */
		if (HDFfid == -1)
		{
		    fid = -1;
		    status = -1;
		    HEpush(DFE_FNF, "EHopen", __FILE__, __LINE__);
            snprintf(errbuf, sizeof(errbuf), "\"%s\" (opened for READONLY access) does not exist.", filename);
		    HEreport("%s\n", errbuf);
		} else
		{
		    /* If file exists then get SD interface ID */
		    /* --------------------------------------- */
		    sdInterfaceID = SDstart(filename, DFACC_RDONLY);

                        /* If SDstart successful ... */
                        /* ------------------------- */
                        if (sdInterfaceID != -1)
                        {

		           /* Set open access to read-only */
		           /* ---------------------------- */
		           acs = 0;
		         } else
                        {
                            /* If error in SDstart then report */
                            /* ------------------------------- */
                            fid = -1;
                            status = -1;
                            HEpush(DFE_FNF, "EHopen", __FILE__, __LINE__);
                            snprintf(errbuf, sizeof(errbuf), "%s%s%s", "\"", filename,
                            "\" cannot be opened for read access.");
                            HEreport("%s\n", errbuf);
                        }
		}

		break;

	    default:
		/* Invalid Access Code */
		/* ------------------- */
		fid = -1;
		status = -1;
		HEpush(DFE_BADACC, "EHopen", __FILE__, __LINE__);
		HEreport("Access Code: %d (%s).\n", access, filename);
	    }

	}
    } else
    {
	/* Too many files opened */
	/* --------------------- */
	status = -1;
	fid = -1;
	HEpush(DFE_TOOMANY, "EHopen", __FILE__, __LINE__);
	HEreport("No more than %d files may be open simultaneously (%s).\n",
		 EHXmaxfilecount, filename);
    }




    if (status == 0)
    {
	/* Initialize Vgroup Access */
	/* ------------------------ */
	Vstart(HDFfid);


	/* Assign HDFEOS fid # & Load HDF fid and sdInterfaceID tables */
	/* ----------------------------------------------------------- */
	for (i = 0; i < EHXmaxfilecount; i++)
	{
	    if (EHXtypeTable[i] == 0)
	    {
		fid = i + EHIDOFFSET;
		EHXacsTable[i] = acs;
		EHXtypeTable[i] = 1;
		EHXfidTable[i] = HDFfid;
		EHXsdTable[i] = sdInterfaceID;
		break;
	    }
	}

    }
    return (fid);
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: EHchkfid                                                         |
|                                                                             |
|  DESCRIPTION: Checks for valid file id and returns HDF file ID and          |
|               SD interface ID                                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  fid            int32               HDF-EOS file ID                         |
|  name           char                Structure name                          |
|                                                                             |
|  OUTPUTS:                                                                   |
|  HDFfid         int32               HDF File ID                             |
|  sdInterfaceID  int32               SDS interface ID                        |
|  access         uint8               access code                             |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Jul 96   Joel Gales    set status=-1 if failure                            |
|  Jul 96   Joel Gales    Add file id offset EHIDOFFSET                       |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
EHchkfid(int32 fid, const char *name, int32 * HDFfid, int32 * sdInterfaceID,
	 uint8 * access)

{
    intn            status = 0;	/* routine return status variable */
    intn            fid0;	/* HDFEOS file ID - Offset */


    /* Check for valid HDFEOS file ID range */
    /* ------------------------------------ */
    if (fid < EHIDOFFSET || fid > EHXmaxfilecount + EHIDOFFSET)
    {
	status = -1;
	HEpush(DFE_RANGE, "EHchkfid", __FILE__, __LINE__);
	HEreport("Invalid file id: %d.  ID must be >= %d and < %d (%s).\n",
		 fid, EHIDOFFSET, EHXmaxfilecount + EHIDOFFSET, name);
    } else
    {
	/* Compute "reduced" file ID */
	/* ------------------------- */
	fid0 = fid % EHIDOFFSET;


	/* Check that HDFEOS file ID is active */
	/* ----------------------------------- */
	if (EHXtypeTable[fid0] == 0)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "EHchkfid", __FILE__, __LINE__);
	    HEreport("File id %d not active (%s).\n", fid, name);
	} else
	{
	    /*
	     * Get HDF file ID, SD interface ID and file access from external
	     * arrays
	     */
	    *HDFfid = EHXfidTable[fid0];
	    *sdInterfaceID = EHXsdTable[fid0];
	    *access = EHXacsTable[fid0];
	}
    }

    return (status);
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: EHidinfo                                                         |
|                                                                             |
|  DESCRIPTION: Gets Hopen and SD interface IDs from HDF-EOS id               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  fid            int32               HDF-EOS file ID                         |
|                                                                             |
|  OUTPUTS:                                                                   |
|  HDFfid         int32               HDF File ID                             |
|  sdInterfaceID  int32               SDS interface ID                        |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jul 96   Joel Gales    Original Programmer                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
EHidinfo(int32 fid, int32 * HDFfid, int32 * sdInterfaceID)

{
    intn            status = 0;	/* routine return status variable */
    uint8           dum;	/* Dummy variable */

    /* Call EHchkfid to get HDF and SD interface IDs */
    /* --------------------------------------------- */
    status = EHchkfid(fid, "EHidinfo", HDFfid, sdInterfaceID, &dum);

    return (status);
}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: EHgetversion                                                     |
|                                                                             |
|  DESCRIPTION: Returns HDF-EOS version string                                |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  fid            int32               HDF-EOS file id                         |
|                                                                             |
|  OUTPUTS:                                                                   |
|  version        char                HDF-EOS version string                  |
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
intn
EHgetversion(int32 fid, char *version)
{
    intn            status = 0;	/* routine return status variable */

    uint8           access;	/* Access code */
    int32           dum;	/* Dummy variable */
    int32           sdInterfaceID = 0;	/* HDF SDS interface ID */
    int32           attrIndex;	/* HDFEOS version attribute index */
    int32           count;	/* Version string size */

    char            attrname[16];	/* Attribute name */


    /* Get SDS interface ID */
    /* -------------------- */
    status = EHchkfid(fid, "EHgetversion", &dum, &sdInterfaceID, &access);


    /* Get attribute index number */
    /* -------------------------- */
    attrIndex = SDfindattr(sdInterfaceID, "HDFEOSVersion");

    /* No such attribute */
    /* ----------------- */
    if (attrIndex < 0)
        return (-1);

    /* Get attribute size */
    /* ------------------ */
    status = SDattrinfo(sdInterfaceID, attrIndex, attrname, &dum, &count);

    /* Check return status */
    /* ------------------- */
    if (status < 0)
        return (-1);

    /* Read version attribute */
    /* ---------------------- */
    status = SDreadattr(sdInterfaceID, attrIndex, (VOIDP) version);


    /* Place string terminator on version string */
    /* ----------------------------------------- */
    version[count] = 0;


    return (status);
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: EHconvAng                                                        |
|                                                                             |
|  DESCRIPTION: Angle conversion Utility                                      |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  outAngle       float64             Output Angle value                      |
|                                                                             |
|  INPUTS:                                                                    |
|  inAngle        float64             Input Angle value                       |
|  code           intn                Conversion code                         |
!                                       HDFE_RAD_DEG (0)                      |
|                                       HDFE_DEG_RAD (1)                      |
|                                       HDFE_DMS_DEG (2)                      |
|                                       HDFE_DEG_DMS (3)                      |
|                                       HDFE_RAD_DMS (4)                      |
|                                       HDFE_DMS_RAD (5)                      |
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
|  Feb 97   Joel Gales    Correct "60" min & "60" sec in _DMS conversion      |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
float64
EHconvAng(float64 inAngle, intn code)
{
#define RADIANS_TO_DEGREES 180. / 3.14159265358979324
#define DEGREES_TO_RADIANS 3.14159265358979324 / 180.

    int32           min;	/* Truncated Minutes */
    int32           deg;	/* Truncated Degrees */

    float64         sec;	/* Seconds */
    float64         outAngle = 0.0;	/* Angle in desired units */

    switch (code)
    {

	/* Convert radians to degrees */
	/* -------------------------- */
    case HDFE_RAD_DEG:
	outAngle = inAngle * RADIANS_TO_DEGREES;
	break;


	/* Convert degrees to radians */
	/* -------------------------- */
    case HDFE_DEG_RAD:
	outAngle = inAngle * DEGREES_TO_RADIANS;
	break;


	/* Convert packed degrees to degrees */
	/* --------------------------------- */
    case HDFE_DMS_DEG:
	deg = (int32)(inAngle / 1000000);
	min = (int32)((inAngle - deg * 1000000) / 1000);
	sec = (inAngle - deg * 1000000 - min * 1000);
	outAngle = deg + min / 60.0 + sec / 3600.0;
	break;


	/* Convert degrees to packed degrees */
	/* --------------------------------- */
    case HDFE_DEG_DMS:
	deg = (int32)(inAngle);
	min = (int32)((inAngle - deg) * 60);
	sec = (inAngle - deg - min / 60.0) * 3600;

	if ((intn) sec == 60)
	{
	    sec = sec - 60;
	    min = min + 1;
	}
	if (min == 60)
	{
	    min = min - 60;
	    deg = deg + 1;
	}
	outAngle = deg * 1000000 + min * 1000 + sec;
	break;


	/* Convert radians to packed degrees */
	/* --------------------------------- */
    case HDFE_RAD_DMS:
	inAngle = inAngle * RADIANS_TO_DEGREES;
	deg = (int32)(inAngle);
	min = (int32)((inAngle - deg) * 60);
	sec = (inAngle - deg - min / 60.0) * 3600;

	if ((intn) sec == 60)
	{
	    sec = sec - 60;
	    min = min + 1;
	}
	if (min == 60)
	{
	    min = min - 60;
	    deg = deg + 1;
	}
	outAngle = deg * 1000000 + min * 1000 + sec;
	break;


	/* Convert packed degrees to radians */
	/* --------------------------------- */
    case HDFE_DMS_RAD:
	deg = (int32)(inAngle / 1000000);
	min = (int32)((inAngle - deg * 1000000) / 1000);
	sec = (inAngle - deg * 1000000 - min * 1000);
	outAngle = deg + min / 60.0 + sec / 3600.0;
	outAngle = outAngle * DEGREES_TO_RADIANS;
	break;
    }
    return (outAngle);
}

#undef TO_DEGREES
#undef TO_RADIANS


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: EHparsestr                                                       |
|                                                                             |
|  DESCRIPTION: String Parser Utility                                         |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  count          int32               Number of string entries                |
|                                                                             |
|  INPUTS:                                                                    |
|  instring       const char          Input string                            |
|  delim          const char          string delimiter                        |
|                                                                             |
|  OUTPUTS:                                                                   |
|  pntr           char *              Pointer array to beginning of each      |
|                                     string entry                            |
|  len            int32               Array of string entry lengths           |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Aug 96   Joel Gales    NULL pointer array returns count only               |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
EHparsestr(const char *instring, const char delim, char *pntr[], int32 len[])
{
    int32           i;		/* Loop index */
    int32           prevDelimPos = 0;	/* Previous delimiter position */
    int32           count;	/* Number of elements in string list */
    int32           slen;	/* String length */

    char           *delimitor;	/* Pointer to delimiter */


    /* Get length of input string list & Point to first delimiter */
    /* ---------------------------------------------------------- */
    slen = (int)strlen(instring);
    delimitor = strchr(instring, delim);

    /* If NULL string set count to zero otherwise set to 1 */
    /* --------------------------------------------------- */
    count = (slen == 0) ? 0 : 1;


    /* if string pointers are requested set first one to beginning of string */
    /* --------------------------------------------------------------------- */
    if (&pntr[0] != NULL)
    {
	pntr[0] = (char *)instring;
    }
    /* If delimiter not found ... */
    /* ---------------------------- */
    if (delimitor == NULL)
    {
	/* if string length requested then set to input string length */
	/* ---------------------------------------------------------- */
	if (len != NULL)
	{
	    len[0] = slen;
	}
    } else
	/* Delimiters Found */
	/* ---------------- */
    {
	/* Loop through all characters in string */
	/* ------------------------------------- */
	for (i = 1; i < slen; i++)
	{
	    /* If character is a delimiter ... */
	    /* ------------------------------- */
	    if (instring[i] == delim)
	    {

		/* If string pointer requested */
		/* --------------------------- */
		if (&pntr[0] != NULL)
		{
		    /* if requested then compute string length of entry */
		    /* ------------------------------------------------ */
		    if (len != NULL)
		    {
			len[count - 1] = i - prevDelimPos;
		    }
		    /* Point to beginning of string entry */
		    /* ---------------------------------- */
		    pntr[count] = (char *)instring + i + 1;
		}
		/* Reset previous delimiter position and increment counter */
		/* ------------------------------------------------------- */
		prevDelimPos = i + 1;
		count++;
	    }
	}

	/* Compute string length of last entry */
	/* ----------------------------------- */
	if (&pntr[0] != NULL && len != NULL)
	{
	    len[count - 1] = i - prevDelimPos;
	}
    }

    return (count);
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: EHstrwithin                                                      |
|                                                                             |
|  DESCRIPTION: Searches for string within target string                      |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  indx           int32               Element index (0 - based)               |
|                                                                             |
|  INPUTS:                                                                    |
|  target         const char          Target string                           |
|  search         const char          Search string                           |
|  delim          const char          Delimiter                               |
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
|  Jan 97   Joel Gales    Change ptr & slen to dynamic arrays                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
EHstrwithin(const char *target, const char *search, const char delim)
{
    intn            found = 0;	/* Target string found flag */

    int32           indx;	/* Loop index */
    int32           nentries;	/* Number of entries in search string */
    int32          *slen;	/* Pointer to string length array */

    char          **ptr;	/* Pointer to string pointer array */
    char            buffer[128];/* Buffer to hold "test" string entry */


    /* Count number of entries in search string list */
    /* --------------------------------------------- */
    nentries = EHparsestr(search, delim, NULL, NULL);


    /* Allocate string pointer and length arrays */
    /* ----------------------------------------- */
    ptr = (char **) calloc(nentries, sizeof(char *));
    if(ptr == NULL)
    {
	HEpush(DFE_NOSPACE,"EHstrwithin", __FILE__, __LINE__);
	return(-1);
    }
    slen = (int32 *) calloc(nentries, sizeof(int32));
    if(slen == NULL)
    {
	HEpush(DFE_NOSPACE,"EHstrwithin", __FILE__, __LINE__);
	free(ptr);
	return(-1);
    }


    /* Parse search string */
    /* ------------------- */
    nentries = EHparsestr(search, delim, ptr, slen);


    /* Loop through all elements in search string list */
    /* ----------------------------------------------- */
    for (indx = 0; indx < nentries; indx++)
    {
	/* Copy string entry into buffer */
	/* ----------------------------- */
	memcpy(buffer, ptr[indx], slen[indx]);
	buffer[slen[indx]] = 0;


	/* Compare target string with string entry */
	/* --------------------------------------- */
	if (strcmp(target, buffer) == 0)
	{
	    found = 1;
	    break;
	}
    }

    /* If not found set return to -1 */
    /* ----------------------------- */
    if (found == 0)
    {
	indx = -1;
    }
    free(slen);
    free(ptr);

    return (indx);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: EHloadliststr                                                    |
|                                                                             |
|  DESCRIPTION: Builds list string from string array                          |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  ptr            char                String pointer array                    |
|  nentries       int32               Number of string array elements         |
|  delim          char                Delimiter                               |
|                                                                             |
|  OUTPUTS:                                                                   |
|  liststr        char                Output list string                      |
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
EHloadliststr(char *ptr[], int32 nentries, char *liststr, char delim)
{
    intn            status = 0;	/* routine return status variable */

    int32           i;		/* Loop index */
    int32           slen;	/* String entry length */
    int32           off = 0;	/* Position of next entry along string list */
    char            dstr[2];    /* string version of input variable "delim" */

    dstr[0] = delim;
    dstr[1] = '\0';


    /* Loop through all entries in string array */
    /* ---------------------------------------- */
    for (i = 0; i < nentries; i++)
    {
	/* Get string length of string array entry */
	/* --------------------------------------- */
	slen = (int)strlen(ptr[i]);


	/* Copy string entry to string list */
	/* -------------------------------- */
	memcpy(liststr + off, ptr[i], slen + 1);


	/* Concatenate with delimiter */
	/* -------------------------- */
	if (i != nentries - 1)
	{
	    strcat(liststr, dstr);
	}
	/* Get position of next entry for string list */
	/* ------------------------------------------ */
	off += slen + 1;
    }

    return (status);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: EHgetid                                                          |
|                                                                             |
|  DESCRIPTION: Get Vgroup/Vdata ID from name                                 |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  outID          int32               Output ID                               |
|                                                                             |
|  INPUTS:                                                                    |
|  fid            int32               HDF-EOS file ID                         |
|  vgid           int32               Vgroup ID                               |
|  objectname     const char          object name                             |
|  code           intn                object code (0 - Vgroup, 1 - Vdata)     |
|  access         const char          access ("w/r")                          |
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
EHgetid(int32 fid, int32 vgid, const char *objectname, intn code,
        const char *access)
{
    intn            i;		/* Loop index */

    int32           nObjects;	/* # of objects in Vgroup */
    int32          *tags;	/* Pnt to Vgroup object tags array */
    int32          *refs;	/* Pnt to Vgroup object refs array */
    int32           id;		/* Object ID */
    int32           outID = -1;	/* Desired object ID */

    char            name[128];	/* Object name */


    /* Get Number of objects */
    /* --------------------- */
    nObjects = Vntagrefs(vgid);

    /* If objects exist ... */
    /* -------------------- */
    if (nObjects != 0)
    {

	/* Get tags and references of objects */
	/* ---------------------------------- */
	tags = (int32 *) malloc(sizeof(int32) * nObjects);
	if(tags == NULL)
	{
	    HEpush(DFE_NOSPACE,"EHgetid", __FILE__, __LINE__);
	    return(-1);
	}
	refs = (int32 *) malloc(sizeof(int32) * nObjects);
	if(refs == NULL)
	{
	    HEpush(DFE_NOSPACE,"EHgetid", __FILE__, __LINE__);
	    free(tags);
	    return(-1);
	}

	Vgettagrefs(vgid, tags, refs, nObjects);


	/* Vgroup ID Section */
	/* ----------------- */
	if (code == 0)
	{
	    /* Loop through objects */
	    /* -------------------- */
	    for (i = 0; i < nObjects; i++)
	    {

		/* If object is Vgroup ... */
		/* ----------------------- */
		if (*(tags + i) == DFTAG_VG)
		{

		    /* Get ID and name */
		    /* --------------- */
		    id = Vattach(fid, *(refs + i), access);
		    Vgetname(id, name);

		    /* If name equals desired object name get ID */
		    /* ----------------------------------------- */
		    if (strcmp(name, objectname) == 0)
		    {
			outID = id;
			break;
		    }
		    /* If not desired object then detach */
		    /* --------------------------------- */
		    Vdetach(id);
		}
	    }
	} else if (code == 1)
	{

	    /* Loop through objects */
	    /* -------------------- */
	    for (i = 0; i < nObjects; i++)
	    {

		/* If object is Vdata ... */
		/* ---------------------- */
		if (*(tags + i) == DFTAG_VH)
		{

		    /* Get ID and name */
		    /* --------------- */
		    id = VSattach(fid, *(refs + i), access);
		    VSgetname(id, name);

		    /* If name equals desired object name get ID */
		    /* ----------------------------------------- */
		    if (EHstrwithin(objectname, name, ',') != -1)
		    {
			outID = id;
			break;
		    }
		    /* If not desired object then detach */
		    /* --------------------------------- */
		    VSdetach(id);
		}
	    }
	}
	free(tags);
	free(refs);
    }
    return (outID);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: EHrevflds                                                        |
|                                                                             |
|  DESCRIPTION: Reverses elements in a string list                            |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  dimlist        char                Original dimension list                 |
|                                                                             |
|  OUTPUTS:                                                                   |
|  revdimlist     char                Reversed dimension list                 |
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
EHrevflds(const char *dimlist, char *revdimlist)
{
    intn            status = 0;	/* routine return status variable */

    int32           indx;	/* Loop index */
    int32           nentries;	/* Number of entries in search string */
    int32          *slen;	/* Pointer to string length array */

    char          **ptr;	/* Pointer to string pointer array */
    char           *tempPtr;	/* Temporary string pointer */
    char           *tempdimlist;/* Temporary dimension list */


    /* Copy dimlist into temp dimlist */
    /* ------------------------------ */
    tempdimlist = (char *) malloc(strlen(dimlist) + 1);
    if(tempdimlist == NULL)
    {
	HEpush(DFE_NOSPACE,"EHrevflds", __FILE__, __LINE__);
	return(-1);
    }
    strcpy(tempdimlist, dimlist);


    /* Count number of entries in search string list */
    /* --------------------------------------------- */
    nentries = EHparsestr(tempdimlist, ',', NULL, NULL);


    /* Allocate string pointer and length arrays */
    /* ----------------------------------------- */
    ptr = (char **) calloc(nentries, sizeof(char *));
    if(ptr == NULL)
    {
	HEpush(DFE_NOSPACE,"EHrevflds", __FILE__, __LINE__);
	free(tempdimlist);
	return(-1);
    }
    slen = (int32 *) calloc(nentries, sizeof(int32));
    if(slen == NULL)
    {
	HEpush(DFE_NOSPACE,"EHrevflds", __FILE__, __LINE__);
	free(ptr);
	free(tempdimlist);
	return(-1);
    }


    /* Parse search string */
    /* ------------------- */
    nentries = EHparsestr(tempdimlist, ',', ptr, slen);


    /* Reverse entries in string pointer array */
    /* --------------------------------------- */
    for (indx = 0; indx < nentries / 2; indx++)
    {
	tempPtr = ptr[indx];
	ptr[indx] = ptr[nentries - 1 - indx];
	ptr[nentries - 1 - indx] = tempPtr;
    }


    /* Replace comma delimiters by nulls */
    /* --------------------------------- */
    for (indx = 0; indx < nentries - 1; indx++)
    {
	*(ptr[indx] - 1) = 0;
    }


    /* Build new string list */
    /* --------------------- */
    status = EHloadliststr(ptr, nentries, revdimlist, ',');


    free(slen);
    free(ptr);
    free(tempdimlist);

    return (status);
}

/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: EHgetmetavalue                                                   |
|                                                                             |
|  DESCRIPTION: Returns metadata value                                        |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  metaptrs        char               Begin and end of metadata section       |
|  parameter      char                parameter to access                     |
|                                                                             |
|  OUTPUTS:                                                                   |
|  metaptr        char                Ptr to (updated) beginning of metadata  |
|  retstr         char                return string containing value          |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Jan 97   Joel Gales    Check string pointer against end of meta section    |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
EHgetmetavalue(char *metaptrs[], const char *parameter, char *retstr)
{
    intn            status = 0;	/* routine return status variable */

    int32           slen;	/* String length */
    char           *newline;	/* Position of new line character */
    char           *sptr;	/* string pointer within metadata */


    /* Get string length of parameter string + 1 */
    /* ----------------------------------------- */
    slen = (int)strlen(parameter) + 1;


    /* Build search string (parameter string + "=") */
    /* -------------------------------------------- */
    strcpy(retstr, parameter);
    strcat(retstr, "=");


    /* Search for string within metadata (beginning at metaptrs[0]) */
    /* ------------------------------------------------------------ */
    sptr = strstr(metaptrs[0], retstr);


    /* If string found within desired section ... */
    /* ------------------------------------------ */
    if (sptr != NULL && sptr < metaptrs[1])
    {
	/* Store position of string within metadata */
	/* ---------------------------------------- */
	metaptrs[0] = sptr;

	/* Find newline "\n" character */
	/* --------------------------- */
	newline = strchr(metaptrs[0], '\n');
	if (newline == NULL)
	{
		retstr[0] = 0;
		return -1;
	}

	/* Copy from "=" to "\n" (exclusive) into return string */
	/* ---------------------------------------------------- */
	memcpy(retstr, metaptrs[0] + slen, newline - metaptrs[0] - slen);

	/* Terminate return string with null */
	/* --------------------------------- */
	retstr[newline - metaptrs[0] - slen] = 0;
    } else
    {
	/*
	 * if parameter string not found within section, null return string
	 * and set status to -1.
	 */
	retstr[0] = 0;
	status = -1;
    }

    return (status);
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: EHmetagroup                                                      |
|                                                                             |
|  DESCRIPTION: Returns pointers to beginning and end of metadata group       |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  metabuf        char                Pointer to HDF-EOS object in metadata   |
|                                                                             |
|  INPUTS:                                                                    |
|  sdInterfaceID  int32               SDS interface ID                        |
|  structname     char                HDF-EOS structure name                  |
|  structcode     char                Structure code ("s/g/p")                |
|  groupname      char                Metadata group name                     |
|                                                                             |
|  OUTPUTS:                                                                   |
|  metaptrs       char                pointers to begin and end of metadata   |
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
char           *
EHmetagroup(int32 sdInterfaceID, const char *structname, const char *structcode,
	    const char *groupname, char *metaptrs[])
{
    intn            i;		/* Loop index */

    int32           attrIndex;	/* Structural metadata attribute index */
    int32           nmeta;	/* Number of 32000 byte metadata sections */
    int32           metalen;	/* Length of structural metadata */

    char           *metabuf;	/* Pointer (handle) to structural metadata */
    char           *endptr = NULL;	/* Pointer to end of metadata section */
    char           *metaptr;	/* Metadata pointer */
    char           *prevmetaptr;/* Previous position of metadata pointer */
    char           *utlstr;     /* Utility string */



     /* Allocate memory for utility string */
     /* ---------------------------------- */
    utlstr = (char *) calloc(UTLSTR_MAX_SIZE,sizeof(char));
    if(utlstr == NULL)
    {
        HEpush(DFE_NOSPACE,"EHEHmetagroup", __FILE__, __LINE__);

        return( NULL);
    }
    /* Determine number of structural metadata "sections" */
    /* -------------------------------------------------- */
    nmeta = 0;
    while (1)
    {
	/* Search for "StructMetadata.x" attribute */
	/* --------------------------------------- */
	snprintf(utlstr, UTLSTR_MAX_SIZE, "%s%d", "StructMetadata.", (int)nmeta);
	attrIndex = SDfindattr(sdInterfaceID, utlstr);


	/* If found then increment metadata section counter else exit loop */
	/* --------------------------------------------------------------- */
	if (attrIndex != -1)
	{
	    nmeta++;
	} else
	{
	    break;
	}
    }


    /* Allocate space for metadata (in units of 32000 bytes) */
    /* ----------------------------------------------------- */
    metabuf = (char *) calloc(32000 * nmeta, 1);

    if(metabuf == NULL)
    {
	HEpush(DFE_NOSPACE,"EHmetagroup", __FILE__, __LINE__);
	free(utlstr);
	return(metabuf);
    }


    /* Read structural metadata */
    /* ------------------------ */
    for (i = 0; i < nmeta; i++)
    {
	snprintf(utlstr, UTLSTR_MAX_SIZE, "%s%d", "StructMetadata.", i);
	attrIndex = SDfindattr(sdInterfaceID, utlstr);
	metalen = (int)strlen(metabuf);
	SDreadattr(sdInterfaceID, attrIndex, metabuf + metalen);
    }

    /* Determine length (# of characters) of metadata */
    /* ---------------------------------------------- */
    metalen = (int)strlen(metabuf);



    /* Find HDF-EOS structure "root" group in metadata */
    /* ----------------------------------------------- */

    /* Setup proper search string */
    /* -------------------------- */
    if (strcmp(structcode, "s") == 0)
    {
	strcpy(utlstr, "GROUP=SwathStructure");
    } else if (strcmp(structcode, "g") == 0)
    {
	strcpy(utlstr, "GROUP=GridStructure");
    } else if (strcmp(structcode, "p") == 0)
    {
	strcpy(utlstr, "GROUP=PointStructure");
    }
    /* Use string search routine (strstr) to move through metadata */
    /* ----------------------------------------------------------- */
    metaptr = strstr(metabuf, utlstr);



    /* Save current metadata pointer */
    /* ----------------------------- */
    prevmetaptr = metaptr;


    /* First loop for "old-style" (non-ODL) metadata string */
    /* ---------------------------------------------------- */
    if (strcmp(structcode, "s") == 0)
    {
	snprintf(utlstr, UTLSTR_MAX_SIZE, "%s%s", "SwathName=\"", structname);
    } else if (strcmp(structcode, "g") == 0)
    {
	snprintf(utlstr, UTLSTR_MAX_SIZE, "%s%s", "GridName=\"", structname);
    } else if (strcmp(structcode, "p") == 0)
    {
	snprintf(utlstr, UTLSTR_MAX_SIZE, "%s%s", "PointName=\"", structname);
    }
    /* Do string search */
    /* ---------------- */
    if (metaptr)
        metaptr = strstr(metaptr, utlstr);


    /*
     * If not found then return to previous position in metadata and look for
     * "new-style" (ODL) metadata string
     */
    if (metaptr == NULL)
    {
	snprintf(utlstr, UTLSTR_MAX_SIZE, "%s%s", "GROUP=\"", structname);
	metaptr = strstr(prevmetaptr, utlstr);
    }
    /* Find group within structure */
    /* --------------------------- */
    if (metaptr && groupname != NULL)
    {
	snprintf(utlstr, UTLSTR_MAX_SIZE, "%s%s", "GROUP=", groupname);
	metaptr = strstr(metaptr, utlstr);

	snprintf(utlstr, UTLSTR_MAX_SIZE, "%s%s", "\t\tEND_GROUP=", groupname);
	if (metaptr)
	    endptr = strstr(metaptr, utlstr);
	else
	    endptr = NULL;
    } else if (metaptr)
    {
	/* If groupname == NULL then find end of structure in metadata */
	/* ----------------------------------------------------------- */
	snprintf(utlstr, UTLSTR_MAX_SIZE, "%s", "\n\tEND_GROUP=");
	endptr = strstr(metaptr, utlstr);
    }


    /* Return beginning and ending pointers */
    /* ------------------------------------ */
    metaptrs[0] = metaptr;
    metaptrs[1] = endptr;

    free(utlstr);

    return (metabuf);
}


/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: EHattr                                                           |
|                                                                             |
|  DESCRIPTION: Reads/Writes attributes for HDF-EOS structures                |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  fid            int32               HDF-EOS file ID                         |
|  attrVgrpID     int32               Attribute Vgroup ID                     |
|  attrname       char                attribute name                          |
|  numbertype     int32               attribute HDF numbertype                |
|  count          int32               Number of attribute elements            |
|  wrcode         char                Read/Write Code "w/r"                   |
|  datbuf         void                I/O buffer                              |
|                                                                             |
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
|  Oct 96   Joel Gales    Pass Vgroup id as routine parameter                 |
|  Oct 96   Joel Gales    Remove Vdetach call                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
EHattr(int32 fid, int32 attrVgrpID, const char *attrname, int32 numbertype,
       int32 count, const char *wrcode, VOIDP datbuf)

{
    intn            status = 0;	/* routine return status variable */
    int32           vdataID;	/* Attribute Vdata ID */

    /*
     * Attributes are stored as Vdatas with name given by the user, class:
     * "Attr0.0" and fieldname: "AttrValues"
     */


    /* Get Attribute Vdata ID and "open" with appropriate I/O code */
    /* ----------------------------------------------------------- */
    vdataID = EHgetid(fid, attrVgrpID, attrname, 1, wrcode);

    /* Write Attribute Section */
    /* ----------------------- */
    if (strcmp(wrcode, "w") == 0)
    {
	/* Create Attribute Vdata (if it doesn't exist) */
	/* -------------------------------------------- */
	if (vdataID == -1)
	{
	    vdataID = VSattach(fid, -1, "w");
	    VSsetname(vdataID, attrname);
	    VSsetclass(vdataID, "Attr0.0");

	    VSfdefine(vdataID, "AttrValues", numbertype, count);
	    Vinsert(attrVgrpID, vdataID);
	}
	/* Write Attribute */
	/* --------------- */
	VSsetfields(vdataID, "AttrValues");
	(void) VSsizeof(vdataID, (char*) "AttrValues");
	VSwrite(vdataID, datbuf, 1, FULL_INTERLACE);

	VSdetach(vdataID);
    }
    /* Read Attribute Section */
    /* ---------------------- */
    if (strcmp(wrcode, "r") == 0)
    {
	/* If attribute doesn't exist report error */
	/* --------------------------------------- */
	if (vdataID == -1)
	{
	    status = -1;
	    HEpush(DFE_GENAPP, "EHattr", __FILE__, __LINE__);
	    HEreport("Attribute %s not defined.\n", attrname);
	} else
	{
	    VSsetfields(vdataID, "AttrValues");
	    (void) VSsizeof(vdataID, (char*) "AttrValues");
	    VSread(vdataID, datbuf, 1, FULL_INTERLACE);
	    VSdetach(vdataID);
	}
    }
    return (status);
}




/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: EHattrinfo                                                       |
|                                                                             |
|  DESCRIPTION: Returns numbertype and count of given HDF-EOS attribute       |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  fid            int32               HDF-EOS file ID                         |
|  attrVgrpID     int32               Attribute Vgroup ID                     |
|  attrname       char                attribute name                          |
|                                                                             |
|  OUTPUTS:                                                                   |
|  numbertype     int32               attribute HDF numbertype                |
|  count          int32               Number of attribute elements            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Oct 96   Joel Gales    Pass Vgroup id as routine parameter                 |
|  Oct 96   Joel Gales    Remove Vdetach call                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
EHattrinfo(int32 fid, int32 attrVgrpID, const char *attrname, int32 * numbertype,
	   int32 * count)

{
    intn            status = 0;	/* routine return status variable */
    int32           vdataID;	/* Attribute Vdata ID */

    /* Get Attribute Vdata ID */
    /* ---------------------- */
    vdataID = EHgetid(fid, attrVgrpID, attrname, 1, "r");

    /* If attribute not defined then report error */
    /* ------------------------------------------ */
    if (vdataID == -1)
    {
	status = -1;
	HEpush(DFE_GENAPP, "EHattr", __FILE__, __LINE__);
	HEreport("Attribute %s not defined.\n", attrname);
    } else
    {
	/* Get attribute info */
	/* ------------------ */
	VSsetfields(vdataID, "AttrValues");
	*count = VSsizeof(vdataID, (char*) "AttrValues");
	*numbertype = VFfieldtype(vdataID, 0);
	VSdetach(vdataID);
    }

    return (status);
}





/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: EHattrcat                                                        |
|                                                                             |
|  DESCRIPTION: Returns a listing of attributes within an HDF-EOS structure   |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  nattr          int32               Number of attributes in swath struct    |
|                                                                             |
|  INPUTS:                                                                    |
|  fid            int32               HDF-EOS file ID                         |
|  attrVgrpID     int32               Attribute Vgroup ID                     |
|  structcode     char                Structure Code ("s/g/p")                |
|                                                                             |
|  OUTPUTS:                                                                   |
|  attrnames      char                Attribute names in swath struct         |
|                                     (Comma-separated list)                  |
|  strbufsize     int32               Attributes name list string length      |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date     Programmer   Description                                         |
|  ======   ============  =================================================   |
|  Jun 96   Joel Gales    Original Programmer                                 |
|  Oct 96   Joel Gales    Pass Vgroup id as routine parameter                 |
|  Oct 96   Joel Gales    Remove Vdetach call                                 |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
EHattrcat(int32 fid, int32 attrVgrpID, char *attrnames, int32 * strbufsize)
{
    intn            i;		        /* Loop index */

    int32           nObjects;	        /* # of objects in Vgroup */
    int32          *tags;	        /* Pnt to Vgroup object tags array */
    int32          *refs;	        /* Pnt to Vgroup object refs array */
    int32           vdataID;	        /* Attribute Vdata ID */

    int32           nattr = 0;	        /* Number of attributes */
    int32           slen;	        /* String length */

    char            name[80];	        /* Attribute name */
    static const char indxstr[] = "INDXMAP:"; /* Index Mapping reserved
                                                 string */
    static const char fvstr[] = "_FV_";	/* Flag Value reserved string */
    static const char bsom[] = "_BLKSOM:";/* Block SOM Offset reserved string */


    /* Set string buffer size to 0 */
    /* --------------------------- */
    *strbufsize = 0;


    /* Get number of attributes within Attribute Vgroup */
    /* ------------------------------------------------ */
    nObjects = Vntagrefs(attrVgrpID);


    /* If attributes exist ... */
    /* ----------------------- */
    if (nObjects > 0)
    {
	/* Get tags and references of attribute Vdatas */
	/* ------------------------------------------- */
	tags = (int32 *) malloc(sizeof(int32) * nObjects);
	if(tags == NULL)
	{
	    HEpush(DFE_NOSPACE,"EHattrcat", __FILE__, __LINE__);
	    return(-1);
	}
	refs = (int32 *) malloc(sizeof(int32) * nObjects);
	if(refs == NULL)
	{
	    HEpush(DFE_NOSPACE,"EHattrcat", __FILE__, __LINE__);
	    free(tags);
	    return(-1);
	}

	Vgettagrefs(attrVgrpID, tags, refs, nObjects);

	/* Get attribute vdata IDs and names */
	/* --------------------------------- */
	for (i = 0; i < nObjects; i++)
	{
	    vdataID = VSattach(fid, *(refs + i), "r");
	    VSgetname(vdataID, name);

	    /*
	     * Don't return fill value, index mapping & block SOM attributes
	     */
	    if (memcmp(name, indxstr, strlen(indxstr)) != 0 &&
		memcmp(name, fvstr, strlen(fvstr)) != 0 &&
		memcmp(name, bsom, strlen(bsom)) != 0)
	    {
		/* Increment attribute counter and add name to list */
		/* ------------------------------------------------ */
		nattr++;
		if (attrnames != NULL)
		{
		    if (nattr == 1)
		    {
			strcpy(attrnames, name);
		    } else
		    {
			strcat(attrnames, ",");
			strcat(attrnames, name);
		    }
		}
		/* Increment attribute names string length */
		/* --------------------------------------- */
		slen = (nattr == 1) ? (int)strlen(name) : (int)strlen(name) + 1;
		*strbufsize += slen;
	    }
	    VSdetach(vdataID);
	}
	free(tags);
	free(refs);
    }
    return (nattr);
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: EHinquire                                                        |
|                                                                             |
|  DESCRIPTION: Returns number and names of HDF-EOS structures in file        |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  nobj           int32               Number of HDF-EOS structures in file    |
|                                                                             |
|  INPUTS:                                                                    |
|  filename       char                HDF-EOS filename                        |
|  type           char                Object Type ("SWATH/GRID/POINT")        |
|                                                                             |
|  OUTPUTS:                                                                   |
|  objectlist     char                List of object names (comma-separated)  |
|  strbufsize     int32               Length of objectlist                    |
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
EHinquire(const char *filename, const char *type, char *objectlist, int32 * strbufsize)
{
    int32           HDFfid;	/* HDF file ID */
    int32           vgRef;	/* Vgroup reference number */
    int32           vGrpID;	/* Vgroup ID */
    int32           nobj = 0;	/* Number of HDFEOS objects in file */
    int32           slen;	/* String length */

    char            name[512];	/* Object name */
    char            class[80];	/* Object class */


    /* Open HDFEOS file of read-only access */
    /* ------------------------------------ */
    HDFfid = Hopen(filename, DFACC_READ, 0);


    /* Start Vgroup Interface */
    /* ---------------------- */
    Vstart(HDFfid);


    /* If string buffer size is requested then zero out counter */
    /* -------------------------------------------------------- */
    if (strbufsize != NULL)
    {
	*strbufsize = 0;
    }
    /* Search for objects from beginning of HDF file */
    /* -------------------------------------------- */
    vgRef = -1;

    /* Loop through all objects */
    /* ------------------------ */
    while (1)
    {
	/* Get Vgroup reference number */
	/* --------------------------- */
	vgRef = Vgetid(HDFfid, vgRef);

	/* If no more then exist search loop */
	/* --------------------------------- */
	if (vgRef == -1)
	{
	    break;
	}
	/* Get Vgroup ID, name, and class */
	/* ------------------------------ */
	vGrpID = Vattach(HDFfid, vgRef, "r");
	Vgetname(vGrpID, name);
	Vgetclass(vGrpID, class);


	/* If object of desired type (SWATH, POINT, GRID) ... */
	/* -------------------------------------------------- */
	if (strcmp(class, type) == 0)
	{

	    /* Increment counter */
	    /* ----------------- */
	    nobj++;


	    /* If object list requested add name to list */
	    /* ----------------------------------------- */
	    if (objectlist != NULL)
	    {
		if (nobj == 1)
		{
		    strcpy(objectlist, name);
		} else
		{
		    strcat(objectlist, ",");
		    strcat(objectlist, name);
		}
	    }
	    /* Compute string length of object entry */
	    /* ------------------------------------- */
	    slen = (nobj == 1) ? (int)strlen(name) : (int)strlen(name) + 1;


	    /* If string buffer size is requested then increment buffer size */
	    /* ------------------------------------------------------------- */
	    if (strbufsize != NULL)
	    {
		*strbufsize += slen;
	    }
	}
	/* Detach Vgroup */
	/* ------------- */
	Vdetach(vGrpID);
    }

    /* "Close" Vgroup interface and HDFEOS file */
    /* ---------------------------------------- */
    Vend(HDFfid);
    Hclose(HDFfid);

    return (nobj);
}



/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: EHclose                                                          |
|                                                                             |
|  DESCRIPTION: Closes HDF-EOS file                                           |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|  fid            int32               HDF-EOS File ID                         |
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
|  Jul 96   Joel Gales    Add file id offset EHIDOFFSET                       |
|  Aug 96   Joel Gales    Add HE error report if file id out of bounds        |
|  Nov 96   Joel Gales    Add EHXacsTable array to "garbage collection"       |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
intn
EHclose(int32 fid)
{
    intn            status = 0;	/* routine return status variable */

    int32           HDFfid;	/* HDF file ID */
    int32           sdInterfaceID;	/* HDF SDS interface ID */
    int32           fid0;	/* HDF EOS file id - offset */


    /* Check for valid HDFEOS file ID range */
    /* ------------------------------------ */
    if (fid >= EHIDOFFSET && fid < EHXmaxfilecount + EHIDOFFSET)
    {
	/* Compute "reduced" file ID */
	/* ------------------------- */
	fid0 = fid % EHIDOFFSET;


	/* Get HDF file ID and SD interface ID */
	/* ----------------------------------- */
	HDFfid = EHXfidTable[fid0];
	sdInterfaceID = EHXsdTable[fid0];

	/* "Close" SD interface, Vgroup interface, and HDF file */
	/* ---------------------------------------------------- */
	status = SDend(sdInterfaceID);
	status = Vend(HDFfid);
	status = Hclose(HDFfid);

	/* Clear out external array entries */
	/* -------------------------------- */
	EHXtypeTable[fid0] = 0;
	EHXacsTable[fid0] = 0;
	EHXfidTable[fid0] = 0;
	EHXsdTable[fid0] = 0;
        if (EHget_numfiles() == 0)
        {
            free(EHXtypeTable);
            EHXtypeTable = NULL;
            free(EHXacsTable);
            EHXacsTable = NULL;
            free(EHXfidTable);
            EHXfidTable = NULL;
            free(EHXsdTable);
            EHXsdTable = NULL;
            EHXmaxfilecount = 0;
        }
    } else
    {
	status = -1;
	HEpush(DFE_RANGE, "EHclose", __FILE__, __LINE__);
	HEreport("Invalid file id: %d.  ID must be >= %d and < %d.\n",
		 fid, EHIDOFFSET, EHXmaxfilecount + EHIDOFFSET);
    }

    return (status);
}

/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: EHnumstr                                                         |
|                                                                             |
|  DESCRIPTION: Returns numerical type code of the given string               |
|               representation.                                               |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  numbertype     int32               numerical type code                     |
|                                                                             |
|  INPUTS:                                                                    |
|  strcode        const char          string representation of the type code  |
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
|  Nov 07   Andrey Kiselev  Original Programmer                               |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
int32
EHnumstr(const char *strcode)
{
    if (strcmp(strcode, "DFNT_UCHAR8") == 0)
        return DFNT_UCHAR8;
    else if (strcmp(strcode, "DFNT_CHAR8") == 0)
        return DFNT_CHAR8;
    else if (strcmp(strcode, "DFNT_FLOAT32") == 0)
        return DFNT_FLOAT32;
    else if (strcmp(strcode, "DFNT_FLOAT64") == 0)
        return DFNT_FLOAT64;
    else if (strcmp(strcode, "DFNT_INT8") == 0)
        return DFNT_INT8;
    else if (strcmp(strcode, "DFNT_UINT8") == 0)
        return DFNT_UINT8;
    else if (strcmp(strcode, "DFNT_INT16") == 0)
        return DFNT_INT16;
    else if (strcmp(strcode, "DFNT_UINT16") == 0)
        return DFNT_UINT16;
    else if (strcmp(strcode, "DFNT_INT32") == 0)
        return DFNT_INT32;
    else if (strcmp(strcode, "DFNT_UINT32") == 0)
        return DFNT_UINT32;
    else
        return DFNT_NONE;
}

/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: EHreset_maxopenfiles                                             |
|                                                                             |
|  DESCRIPTION: Change the allowed number of opened HDFEOS files.             |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  numbertype     intn                The current maximum number of opened    |
|                                     files allowed, or -1, if unable         |
|                                     to reset it.                            |
|                                                                             |
|  INPUTS:                                                                    |
|  strcode        intn                Requested number of opened files.       |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date        Programmer     Description                                    |
|  ==========   ============   ============================================== |
|  2013.04.03   Andrey Kiselev Original Programmer                            |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
static intn
EHreset_maxopenfiles(intn req_max)
{
    intn    ret_value;

    if (req_max <= EHXmaxfilecount)
        return EHXmaxfilecount;

    /* Falback to built-in NEOSHDF constant if           */
    /* SDreset_maxopenfiles() interface is not available */
    /* ------------------------------------------------- */
#ifdef HDF4_HAS_MAXOPENFILES
    ret_value = SDreset_maxopenfiles(req_max);
#else
    ret_value = NEOSHDF;
#endif /* HDF4_HAS_MAXOPENFILES */

    if (ret_value > 0)
    {
        EHXtypeTable = realloc(EHXtypeTable, ret_value * sizeof(*EHXtypeTable));
        memset(EHXtypeTable + EHXmaxfilecount, 0,
               (ret_value - EHXmaxfilecount) * sizeof(*EHXtypeTable));
        EHXacsTable = realloc(EHXacsTable, ret_value * sizeof(*EHXacsTable));
        memset(EHXacsTable + EHXmaxfilecount, 0,
               (ret_value - EHXmaxfilecount) * sizeof(*EHXacsTable));
        EHXfidTable = realloc(EHXfidTable, ret_value * sizeof(*EHXfidTable));
        memset(EHXfidTable + EHXmaxfilecount, 0,
               (ret_value - EHXmaxfilecount) * sizeof(*EHXfidTable));
        EHXsdTable = realloc(EHXsdTable, ret_value * sizeof(*EHXsdTable));
        memset(EHXsdTable + EHXmaxfilecount, 0,
               (ret_value - EHXmaxfilecount) * sizeof(*EHXsdTable));
        EHXmaxfilecount = ret_value;
    }

    return ret_value;
}

/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: EHget_maxopenfiles                                               |
|                                                                             |
|  DESCRIPTION: Request the allowed number of opened HDFEOS files and maximum |
|               number of opened files allowed in the system.                 |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  status         intn                return status (0) SUCCEED, (-1) FAIL    |
|                                                                             |
|  INPUTS:                                                                    |
|             None                                                            |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|  curr_max       intn                Current number of open files allowed.   |
|  sys_limit      intn                Maximum number of open files allowed    |
|                                     in the system.                          |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date        Programmer     Description                                    |
|  ==========   ============   ============================================== |
|  2013.04.03   Andrey Kiselev Original Programmer                            |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
static intn
EHget_maxopenfiles(intn *curr_max,
		   intn *sys_limit)
{
    intn ret_value = 0;

#ifdef HDF4_HAS_MAXOPENFILES
    ret_value = SDget_maxopenfiles(curr_max, sys_limit);
#else
    *sys_limit = NEOSHDF;
#endif /* HDF4_HAS_MAXOPENFILES */

    *curr_max = EHXmaxfilecount;

    return ret_value;
}

/*----------------------------------------------------------------------------|
|  BEGIN_PROLOG                                                               |
|                                                                             |
|  FUNCTION: EHget_numfiles                                                   |
|                                                                             |
|  DESCRIPTION: Request the number of HDFEOS files currently opened.          |
|                                                                             |
|                                                                             |
|  Return Value    Type     Units     Description                             |
|  ============   ======  =========   =====================================   |
|  nfileopen      intn                Number of HDFEOS files already opened.  |
|                                                                             |
|  INPUTS:                                                                    |
|             None                                                            |
|                                                                             |
|                                                                             |
|  OUTPUTS:                                                                   |
|             None                                                            |
|                                     in the system.                          |
|                                                                             |
|  NOTES:                                                                     |
|                                                                             |
|                                                                             |
|   Date        Programmer     Description                                    |
|  ==========   ============   ============================================== |
|  2013.04.03   Andrey Kiselev Original Programmer                            |
|                                                                             |
|  END_PROLOG                                                                 |
-----------------------------------------------------------------------------*/
static intn
EHget_numfiles()
{
    intn            i;		    /* Loop index */
    intn            nfileopen = 0;  /* # of HDF files open */

    if (EHXtypeTable)
    {
        /* Determine number of files currently opened */
        /* ------------------------------------------ */
        for (i = 0; i < EHXmaxfilecount; i++)
        {
            nfileopen += EHXtypeTable[i];
        }
    }

    return nfileopen;
}
