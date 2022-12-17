#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "ogr_srs_api.h"
#include "gtest_include.h"

namespace
{

// ---------------------------------------------------------------------------

TEST(proj_with_fork, test)
{
    OGRSpatialReferenceH hSRS = OSRNewSpatialReference(nullptr);
    // To open the DB in the parent
    OSRImportFromEPSG(hSRS, 32631);

    pid_t children[4];
    for (int i = 0; i < 4; i++)
    {
        children[i] = fork();
        if (children[i] < 0)
            exit(1);
        if (children[i] == 0)
        {
            for (int epsg = 32601; epsg <= 32661; epsg++)
            {
                EXPECT_EQ(OSRImportFromEPSG(hSRS, epsg), OGRERR_NONE);
                EXPECT_EQ(OSRImportFromEPSG(hSRS, epsg + 100), OGRERR_NONE);
            }
            _exit(0);
        }
    }

    for (int i = 0; i < 4; i++)
    {
        int status = 0;
        waitpid(children[i], &status, 0);
        EXPECT_EQ(status, 0);
    }

    OSRDestroySpatialReference(hSRS);
}

}  // namespace
