#ifdef __cplusplus
  extern "C" {
#endif

typedef void* gma_object_h;

gma_class_t gma_object_get_class(gma_object_h);

typedef void* gma_number_h;
typedef void* gma_pair_h;
typedef void* gma_bins_h;
typedef void* gma_histogram_h;
typedef void* gma_classifier_h;
typedef void* gma_cell_h;
typedef void* gma_cell_callback_h;
typedef void* gma_logical_operation_h;
typedef void* gma_hash_h;

#ifdef __cplusplus
  }
#endif
