#include "csf.h"
#include "csfimpl.h"

/* reset Merrno variable
 * ResetMerrno sets the Merrno variable to NOERROR (0).
 *
 * example
 * .so examples/testcsf.tr
 */
void ResetMerrno(void)
{
	Merrno = NOERROR;
}
