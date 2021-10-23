#include "csf.h"
#include "csfimpl.h"


static const char * const errolist[ERRORNO]={
"No error",
"File could not be opened or does not exist",
"File is not a PCRaster file",
"Wrong C.S.F.-version",
"Wrong byte order",
"Not enough memory",
"Illegal cell representation constant",
"Access denied",
"Row number to big",
"Column number to big",
"Map is not a raster file",
"Illegal conversion",
"No space on device to write",
"A write error occurred",
"Illegal handle",
"A read error occurred",
"Illegal access mode constant",
"Attribute not found",
"Attribute already in file",
"Cell size <= 0",
"Conflict between cell representation and value scale",
"Illegal value scale",
"XXXXXXXXXXXXXXXXXXXX",
"Angle < -0.5 pi or > 0.5 pi",
"Can't read as a boolean map",
"Can't write as a boolean map",
"Can't write as a ldd map",
"Can't use as a ldd map",
"Can't write to version 1 cell representation",
"Usetype is not version 2 cell representation, VS_LDD or VS_BOOLEAN"
};

/* write error message to stderr
 * Mperror writes the error message belonging to the current Merrno
 * value to stderr, prefixed by a userString, separated by a semicolon.
 *
 * example
 * .so examples/csfdump1.tr
 */
void Mperror(
	const char *userString) /* prefix string */
{
	(void)fprintf(stderr,"%s : %s\n", userString, errolist[Merrno]);
}

/* write error message to stderr and exits
 * Mperror first writes the error message belonging to the current Merrno
 * value to stderr, prefixed by userString, separated by a semicolon.
 * Then Mperror exits by calling exit() with the given exit code.
 *
 * returns
 * NEVER RETURNS!
 *
 * example
 * .so examples/csfdump2.tr
 */
void MperrorExit(
	const char *userString, /* prefix string */
	int exitCode) /* exit code */
{
	Mperror(userString);
	exit(exitCode);
}

/* error message 
 * MstrError returns the error message belonging to the current Merrno
 * value.
 * returns the error message belonging to the current Merrno
 *
 * example
 * .so examples/testcsf.tr
 */
const char *MstrError(void)
{	
	return(errolist[Merrno]);
}
