#include "private.hpp"

template<typename datatype1,typename datatype2> struct gma_two_bands_callback {
    typedef int (*type)(gma_band<datatype1>*, gma_block<datatype1>*, gma_band<datatype2>*, gma_band<datatype2>*, gma_object_t**, gma_object_t*);
    type fct;
};

gma_object_t *gma_spatial_decision(GDALRasterBand *b1, gma_spatial_decision_method_t method, GDALRasterBand *decision, GDALRasterBand *b2, gma_object_t *arg = NULL) {
    gma_object_t *retval = NULL;
    // b1 is changed, b2 is not
    if (b1->GetXSize() != b2->GetXSize() || b1->GetYSize() != b2->GetYSize()) {
        fprintf(stderr, "The sizes of the rasters should be the same.\n");
        return NULL;
    }
    switch (method) {
    case gma_method_if: // b1 = b2 if decision
        type_switch_bb(gma_assign_band, 0);
        break;
    default:
        goto unknown_method;
    }
    return retval;
not_implemented_for_these_datatypes:
    fprintf(stderr, "Not implemented for these datatypes <%i,%i>.\n", b1->GetRasterDataType(), b2->GetRasterDataType());
    return NULL;
unknown_method:
    fprintf(stderr, "Unknown method.\n");
    return NULL;
wrong_argument_class:
    fprintf(stderr, "Wrong class in argument.\n");
    return NULL;

}
