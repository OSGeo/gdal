void *GDALCreateGCPTransformer( int nGCPs, GDAL_GCP const * pGCPs,
                                     int nOrder, int bApproxOK);
 
int GDALGCPTransform    (
        void *pTransformArg,
        int     bDstToSrc,
        int     nPointCount,
        double *    x,
        double *    y,
        double *    z,
        int *   optional_int
    );
 
void GDALDestroyGCPTransformer (void *pTransformArg);