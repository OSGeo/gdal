#include <proj_api.h>
#include <stdio.h> /* for printf declaration */


#ifdef _WIN32

int main(int argc, char* argv[])
{
    printf("Test not yet ported on Win32\n");
    return 0;
}

#else

#include <pthread.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

volatile int run = 0;
volatile int started = 0;

void* thread_main(void* unused)
{
    projCtx p_proj_ctxt;
    projPJ p_WGS84_proj;
    projPJ p_OSGB36_proj;

    __sync_add_and_fetch(&started, 1);
    while(run == 0);

    p_proj_ctxt=pj_ctx_alloc();
    p_WGS84_proj=pj_init_plus_ctx(p_proj_ctxt,"+proj=longlat "
            "+ellps=WGS84 +datum=WGS84 +no_defs");
    p_OSGB36_proj=pj_init_plus_ctx(p_proj_ctxt,
            "+proj=longlat +ellps=airy +datum=OSGB36 +nadgrids=OSTN02_NTv2.gsb "
            "+no_defs");

    while(run)
    {
        double x, y;
        int proj_ret;

        x = -5.2*DEG_TO_RAD;
        y = 50*DEG_TO_RAD;
        proj_ret = pj_transform(p_WGS84_proj,
                                p_OSGB36_proj, 1, 1, &x, &y, NULL );
        x *= RAD_TO_DEG;
        y *= RAD_TO_DEG;
        /*printf("%.18f %.18f\n", x, y); */
        assert(proj_ret == 0);
        assert(fabs(x - -5.198965360936369962) < 1e-15);
        assert(fabs(y - 49.999396034285531698) < 1e-15);
    }

    return NULL;
}

int main(int argc, char* argv[])
{
    int i;
    pthread_t tid1, tid2;
    pthread_attr_t attr1, attr2;

    pthread_attr_init(&attr1);
    pthread_attr_init(&attr2);

    pthread_create(&tid1, &attr1, thread_main, NULL);
    pthread_create(&tid2, &attr2, thread_main, NULL);
    while(started != 2);
    run = 1;
    for(i=0;i<2;i++)
        sleep(1);
    run = 0;
    return 0;
}

#endif /* _WIN32 */
