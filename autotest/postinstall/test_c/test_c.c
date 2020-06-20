#include <stdio.h>

/* C API */
#include <gdal.h>
#include <cpl_conv.h>
#include <cpl_csv.h>
#include <cpl_error.h>
#include <cpl_http.h>
#include <cpl_minixml.h>
#include <cpl_multiproc.h>
#include <cpl_port.h>
#include <cpl_progress.h>
#include <cpl_string.h>
#include <cpl_time.h>
#include <cpl_virtualmem.h>
#include <cpl_vsi_error.h>
#include <cpl_vsi.h>

int main(int argc, char *argv[]) {
    printf("%s\n", GDALVersionInfo("RELEASE_NAME"));
    return(0);
}
