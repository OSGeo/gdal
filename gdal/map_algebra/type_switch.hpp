#define type_switch_bb(sub,fd) switch (b1->GetRasterDataType()) {       \
    case GDT_Byte:                                                      \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte: {                                                \
            gma_two_bands_callback<uint8_t,uint8_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt16: {                                              \
            gma_two_bands_callback<uint8_t,uint16_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int16: {                                               \
            gma_two_bands_callback<uint8_t,int16_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt32: {                                              \
            gma_two_bands_callback<uint8_t,uint32_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int32: {                                               \
            gma_two_bands_callback<uint8_t,int32_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Float32: {                                             \
            gma_two_bands_callback<uint8_t,float> cb;                   \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Float64: {                                             \
            gma_two_bands_callback<uint8_t,double> cb;                  \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_UInt16:                                                    \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte: {                                                \
            gma_two_bands_callback<uint16_t,uint8_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt16: {                                              \
            gma_two_bands_callback<uint16_t,uint16_t> cb;               \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int16: {                                               \
            gma_two_bands_callback<uint16_t,int16_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt32: {                                              \
            gma_two_bands_callback<uint16_t,uint32_t> cb;               \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int32: {                                               \
            gma_two_bands_callback<uint16_t,int32_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Float32: {                                             \
            gma_two_bands_callback<uint16_t,float> cb;                  \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Float64: {                                             \
            gma_two_bands_callback<uint16_t,double> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Int16:                                                     \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte: {                                                \
            gma_two_bands_callback<int16_t,uint8_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt16: {                                              \
            gma_two_bands_callback<int16_t,uint16_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int16: {                                               \
            gma_two_bands_callback<int16_t,int16_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt32: {                                              \
            gma_two_bands_callback<int16_t,uint32_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int32: {                                               \
            gma_two_bands_callback<int16_t,int32_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Float32: {                                             \
            gma_two_bands_callback<int16_t,float> cb;                   \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Float64: {                                             \
            gma_two_bands_callback<int16_t,double> cb;                  \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_UInt32:                                                    \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte: {                                                \
            gma_two_bands_callback<uint32_t,uint8_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt16: {                                              \
            gma_two_bands_callback<uint32_t,uint16_t> cb;               \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int16: {                                               \
            gma_two_bands_callback<uint32_t,int16_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt32: {                                              \
            gma_two_bands_callback<uint32_t,uint32_t> cb;               \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int32: {                                               \
            gma_two_bands_callback<uint32_t,int32_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Float32: {                                             \
            gma_two_bands_callback<uint32_t,float> cb;                  \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Float64: {                                             \
            gma_two_bands_callback<uint32_t,double> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Int32:                                                     \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte: {                                                \
            gma_two_bands_callback<int32_t,uint8_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt16: {                                              \
            gma_two_bands_callback<int32_t,uint16_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int16: {                                               \
            gma_two_bands_callback<int32_t,int16_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt32: {                                              \
            gma_two_bands_callback<int32_t,uint32_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int32: {                                               \
            gma_two_bands_callback<int32_t,int32_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Float32: {                                             \
            gma_two_bands_callback<int32_t,float> cb;                   \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Float64: {                                             \
            gma_two_bands_callback<int32_t,double> cb;                  \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Float32:                                                   \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte: {                                                \
            gma_two_bands_callback<float,uint8_t> cb;                   \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt16: {                                              \
            gma_two_bands_callback<float,uint16_t> cb;                  \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int16: {                                               \
            gma_two_bands_callback<float,int16_t> cb;                   \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt32: {                                              \
            gma_two_bands_callback<float,uint32_t> cb;                  \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int32: {                                               \
            gma_two_bands_callback<float,int32_t> cb;                   \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Float32: {                                             \
            gma_two_bands_callback<float,float> cb;                     \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Float64: {                                             \
            gma_two_bands_callback<float,double> cb;                    \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Float64:                                                   \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte: {                                                \
            gma_two_bands_callback<double,uint8_t> cb;                  \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt16: {                                              \
            gma_two_bands_callback<double,uint16_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int16: {                                               \
            gma_two_bands_callback<double,int16_t> cb;                  \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt32: {                                              \
            gma_two_bands_callback<double,uint32_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int32: {                                               \
            gma_two_bands_callback<double,int32_t> cb;                  \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Float32: {                                             \
            gma_two_bands_callback<double,float> cb;                    \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Float64: {                                             \
            gma_two_bands_callback<double,double> cb;                   \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    default:                                                            \
        goto not_implemented_for_these_datatypes;                       \
    }

#define type_switch_bi(sub,fd) switch (b1->GetRasterDataType()) {       \
    case GDT_Byte:                                                      \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte: {                                                \
            gma_two_bands_callback<uint8_t,uint8_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt16: {                                              \
            gma_two_bands_callback<uint8_t,uint16_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int16: {                                               \
            gma_two_bands_callback<uint8_t,int16_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt32: {                                              \
            gma_two_bands_callback<uint8_t,uint32_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int32: {                                               \
            gma_two_bands_callback<uint8_t,int32_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_UInt16:                                                    \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte: {                                                \
            gma_two_bands_callback<uint16_t,uint8_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt16: {                                              \
            gma_two_bands_callback<uint16_t,uint16_t> cb;               \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int16: {                                               \
            gma_two_bands_callback<uint16_t,int16_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt32: {                                              \
            gma_two_bands_callback<uint16_t,uint32_t> cb;               \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int32: {                                               \
            gma_two_bands_callback<uint16_t,int32_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Int16:                                                     \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte: {                                                \
            gma_two_bands_callback<int16_t,uint8_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt16: {                                              \
            gma_two_bands_callback<int16_t,uint16_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int16: {                                               \
            gma_two_bands_callback<int16_t,int16_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt32: {                                              \
            gma_two_bands_callback<int16_t,uint32_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int32: {                                               \
            gma_two_bands_callback<int16_t,int32_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_UInt32:                                                    \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte: {                                                \
            gma_two_bands_callback<uint32_t,uint8_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt16: {                                              \
            gma_two_bands_callback<uint32_t,uint16_t> cb;               \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int16: {                                               \
            gma_two_bands_callback<uint32_t,int16_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt32: {                                              \
            gma_two_bands_callback<uint32_t,uint32_t> cb;               \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int32: {                                               \
            gma_two_bands_callback<uint32_t,int32_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Int32:                                                     \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte: {                                                \
            gma_two_bands_callback<int32_t,uint8_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt16: {                                              \
            gma_two_bands_callback<int32_t,uint16_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int16: {                                               \
            gma_two_bands_callback<int32_t,int16_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt32: {                                              \
            gma_two_bands_callback<int32_t,uint32_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int32: {                                               \
            gma_two_bands_callback<int32_t,int32_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Float32:                                                   \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte: {                                                \
            gma_two_bands_callback<float,uint8_t> cb;                   \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt16: {                                              \
            gma_two_bands_callback<float,uint16_t> cb;                  \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int16: {                                               \
            gma_two_bands_callback<float,int16_t> cb;                   \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt32: {                                              \
            gma_two_bands_callback<float,uint32_t> cb;                  \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int32: {                                               \
            gma_two_bands_callback<float,int32_t> cb;                   \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Float64:                                                   \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte: {                                                \
            gma_two_bands_callback<double,uint8_t> cb;                  \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt16: {                                              \
            gma_two_bands_callback<double,uint16_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int16: {                                               \
            gma_two_bands_callback<double,int16_t> cb;                  \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt32: {                                              \
            gma_two_bands_callback<double,uint32_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int32: {                                               \
            gma_two_bands_callback<double,int32_t> cb;                  \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    default:                                                            \
        goto not_implemented_for_these_datatypes;                       \
    }

#define type_switch_ib(sub,fd) switch (b1->GetRasterDataType()) {       \
    case GDT_Byte:                                                      \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte: {                                                \
            gma_two_bands_callback<uint8_t,uint8_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt16: {                                              \
            gma_two_bands_callback<uint8_t,uint16_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int16: {                                               \
            gma_two_bands_callback<uint8_t,int16_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt32: {                                              \
            gma_two_bands_callback<uint8_t,uint32_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int32: {                                               \
            gma_two_bands_callback<uint8_t,int32_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Float32: {                                             \
            gma_two_bands_callback<uint8_t,float> cb;                   \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Float64: {                                             \
            gma_two_bands_callback<uint8_t,double> cb;                  \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_UInt16:                                                    \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte: {                                                \
            gma_two_bands_callback<uint16_t,uint8_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt16: {                                              \
            gma_two_bands_callback<uint16_t,uint16_t> cb;               \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int16: {                                               \
            gma_two_bands_callback<uint16_t,int16_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt32: {                                              \
            gma_two_bands_callback<uint16_t,uint32_t> cb;               \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int32: {                                               \
            gma_two_bands_callback<uint16_t,int32_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Float32: {                                             \
            gma_two_bands_callback<uint16_t,float> cb;                  \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Float64: {                                             \
            gma_two_bands_callback<uint16_t,double> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Int16:                                                     \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte: {                                                \
            gma_two_bands_callback<int16_t,uint8_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt16: {                                              \
            gma_two_bands_callback<int16_t,uint16_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int16: {                                               \
            gma_two_bands_callback<int16_t,int16_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt32: {                                              \
            gma_two_bands_callback<int16_t,uint32_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int32: {                                               \
            gma_two_bands_callback<int16_t,int32_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Float32: {                                             \
            gma_two_bands_callback<int16_t,float> cb;                   \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Float64: {                                             \
            gma_two_bands_callback<int16_t,double> cb;                  \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_UInt32:                                                    \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte: {                                                \
            gma_two_bands_callback<uint32_t,uint8_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt16: {                                              \
            gma_two_bands_callback<uint32_t,uint16_t> cb;               \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int16: {                                               \
            gma_two_bands_callback<uint32_t,int16_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt32: {                                              \
            gma_two_bands_callback<uint32_t,uint32_t> cb;               \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int32: {                                               \
            gma_two_bands_callback<uint32_t,int32_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Float32: {                                             \
            gma_two_bands_callback<uint32_t,float> cb;                  \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Float64: {                                             \
            gma_two_bands_callback<uint32_t,double> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Int32:                                                     \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte: {                                                \
            gma_two_bands_callback<int32_t,uint8_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt16: {                                              \
            gma_two_bands_callback<int32_t,uint16_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int16: {                                               \
            gma_two_bands_callback<int32_t,int16_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt32: {                                              \
            gma_two_bands_callback<int32_t,uint32_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int32: {                                               \
            gma_two_bands_callback<int32_t,int32_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Float32: {                                             \
            gma_two_bands_callback<int32_t,float> cb;                   \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Float64: {                                             \
            gma_two_bands_callback<int32_t,double> cb;                  \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    default:                                                            \
        goto not_implemented_for_these_datatypes;                       \
    }

#define type_switch_ii(sub,fd) switch (b1->GetRasterDataType()) {       \
    case GDT_Byte:                                                      \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte: {                                                \
            gma_two_bands_callback<uint8_t,uint8_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt16: {                                              \
            gma_two_bands_callback<uint8_t,uint16_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int16: {                                               \
            gma_two_bands_callback<uint8_t,int16_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt32: {                                              \
            gma_two_bands_callback<uint8_t,uint32_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int32: {                                               \
            gma_two_bands_callback<uint8_t,int32_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_UInt16:                                                    \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte: {                                                \
            gma_two_bands_callback<uint16_t,uint8_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt16: {                                              \
            gma_two_bands_callback<uint16_t,uint16_t> cb;               \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int16: {                                               \
            gma_two_bands_callback<uint16_t,int16_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt32: {                                              \
            gma_two_bands_callback<uint16_t,uint32_t> cb;               \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int32: {                                               \
            gma_two_bands_callback<uint16_t,int32_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Int16:                                                     \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte: {                                                \
            gma_two_bands_callback<int16_t,uint8_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt16: {                                              \
            gma_two_bands_callback<int16_t,uint16_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int16: {                                               \
            gma_two_bands_callback<int16_t,int16_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt32: {                                              \
            gma_two_bands_callback<int16_t,uint32_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int32: {                                               \
            gma_two_bands_callback<int16_t,int32_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_UInt32:                                                    \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte: {                                                \
            gma_two_bands_callback<uint32_t,uint8_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt16: {                                              \
            gma_two_bands_callback<uint32_t,uint16_t> cb;               \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int16: {                                               \
            gma_two_bands_callback<uint32_t,int16_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt32: {                                              \
            gma_two_bands_callback<uint32_t,uint32_t> cb;               \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int32: {                                               \
            gma_two_bands_callback<uint32_t,int32_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Int32:                                                     \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte: {                                                \
            gma_two_bands_callback<int32_t,uint8_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt16: {                                              \
            gma_two_bands_callback<int32_t,uint16_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int16: {                                               \
            gma_two_bands_callback<int32_t,int16_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_UInt32: {                                              \
            gma_two_bands_callback<int32_t,uint32_t> cb;                \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        case GDT_Int32: {                                               \
            gma_two_bands_callback<int32_t,int32_t> cb;                 \
            cb.fct = sub;                                               \
            gma_two_bands_proc(b1, cb, b2, &retval, arg, fd);           \
            break;                                                      \
        }                                                               \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    default:                                                            \
        goto not_implemented_for_these_datatypes;                       \
    }

#define type_switch_single(sub,fd) switch (b->GetRasterDataType()) {    \
    case GDT_Byte: {                                                    \
        gma_compute_value_callback<uint8_t> cb;                         \
        cb.fct = sub;                                                   \
        gma_proc_compute_value(b, cb, &retval, arg, fd);                \
        break;                                                          \
    }                                                                   \
    case GDT_UInt16: {                                                  \
        gma_compute_value_callback<uint16_t> cb;                        \
        cb.fct = sub;                                                   \
        gma_proc_compute_value(b, cb, &retval, arg, fd);                \
        break;                                                          \
    }                                                                   \
    case GDT_Int16: {                                                   \
        gma_compute_value_callback<int16_t> cb;                         \
        cb.fct = sub;                                                   \
        gma_proc_compute_value(b, cb, &retval, arg, fd);                \
        break;                                                          \
    }                                                                   \
    case GDT_UInt32: {                                                  \
        gma_compute_value_callback<uint32_t> cb;                        \
        cb.fct = sub;                                                   \
        gma_proc_compute_value(b, cb, &retval, arg, fd);                \
        break;                                                          \
    }                                                                   \
    case GDT_Int32: {                                                   \
        gma_compute_value_callback<int32_t> cb;                         \
        cb.fct = sub;                                                   \
        gma_proc_compute_value(b, cb, &retval, arg, fd);                \
        break;                                                          \
    }                                                                   \
    case GDT_Float32: {                                                 \
        gma_compute_value_callback<float> cb;                           \
        cb.fct = sub;                                                   \
        gma_proc_compute_value(b, cb, &retval, arg, fd);                \
        break;                                                          \
    }                                                                   \
    case GDT_Float64: {                                                 \
        gma_compute_value_callback<double> cb;                          \
        cb.fct = sub;                                                   \
        gma_proc_compute_value(b, cb, &retval, arg, fd);                \
        break;                                                          \
    }                                                                   \
    default:                                                            \
        goto not_implemented_for_this_datatype;                         \
    }

#define type_switch_single_i(sub,fd) switch (b->GetRasterDataType()) {  \
    case GDT_Byte: {                                                    \
        gma_compute_value_callback<uint8_t> cb;                         \
        cb.fct = sub;                                                   \
        gma_proc_compute_value(b, cb, &retval, arg, fd);                \
        break;                                                          \
    }                                                                   \
    case GDT_UInt16: {                                                  \
        gma_compute_value_callback<uint16_t> cb;                        \
        cb.fct = sub;                                                   \
        gma_proc_compute_value(b, cb, &retval, arg, fd);                \
        break;                                                          \
    }                                                                   \
    case GDT_Int16: {                                                   \
        gma_compute_value_callback<int16_t> cb;                         \
        cb.fct = sub;                                                   \
        gma_proc_compute_value(b, cb, &retval, arg, fd);                \
        break;                                                          \
    }                                                                   \
    case GDT_UInt32: {                                                  \
        gma_compute_value_callback<uint32_t> cb;                        \
        cb.fct = sub;                                                   \
        gma_proc_compute_value(b, cb, &retval, arg, fd);                \
        break;                                                          \
    }                                                                   \
    case GDT_Int32: {                                                   \
        gma_compute_value_callback<int32_t> cb;                         \
        cb.fct = sub;                                                   \
        gma_proc_compute_value(b, cb, &retval, arg, fd);                \
        break;                                                          \
    }                                                                   \
    default:                                                            \
        goto not_implemented_for_this_datatype;                         \
    }

#define type_switch_arg(sub) switch (b->GetRasterDataType()) {  \
    case GDT_Byte: {                                            \
        gma_with_arg_callback<uint8_t> cb;                      \
        cb.fct = sub;                                           \
        gma_with_arg_proc(b, cb, arg);                          \
        break;                                                  \
    }                                                           \
    case GDT_UInt16: {                                          \
        gma_with_arg_callback<uint16_t> cb;                     \
        cb.fct = sub;                                           \
        gma_with_arg_proc(b, cb, arg);                          \
        break;                                                  \
    }                                                           \
    case GDT_Int16: {                                           \
        gma_with_arg_callback<int16_t> cb;                      \
        cb.fct = sub;                                           \
        gma_with_arg_proc(b, cb, arg);                          \
        break;                                                  \
    }                                                           \
    case GDT_UInt32: {                                          \
        gma_with_arg_callback<uint32_t> cb;                     \
        cb.fct = sub;                                           \
        gma_with_arg_proc(b, cb, arg);                          \
        break;                                                  \
    }                                                           \
    case GDT_Int32: {                                           \
        gma_with_arg_callback<int32_t> cb;                      \
        cb.fct = sub;                                           \
        gma_with_arg_proc(b, cb, arg);                          \
        break;                                                  \
    }                                                           \
    case GDT_Float32: {                                         \
        gma_with_arg_callback<float> cb;                        \
        cb.fct = sub;                                           \
        gma_with_arg_proc(b, cb, arg);                          \
        break;                                                  \
    }                                                           \
    case GDT_Float64: {                                         \
        gma_with_arg_callback<double> cb;                       \
        cb.fct = sub;                                           \
        gma_with_arg_proc(b, cb, arg);                          \
        break;                                                  \
    }                                                           \
    default:                                                    \
        goto not_implemented_for_this_datatype;                 \
    }

#define new_object(object,band,klass,arg) {             \
            switch (band->GetRasterDataType()) {        \
            case GDT_Byte:                              \
                object = new klass<uint8_t>(arg);       \
                break;                                  \
            case GDT_UInt16:                            \
                object = new klass<uint16_t>(arg);      \
                break;                                  \
            case GDT_Int16:                             \
                object = new klass<int16_t>(arg);       \
                break;                                  \
            case GDT_UInt32:                            \
                object = new klass<uint32_t>(arg);      \
                break;                                  \
            case GDT_Int32:                             \
                object = new klass<int32_t>(arg);       \
                break;                                  \
            case GDT_Float32:                           \
                object = new klass<float>(arg);         \
                break;                                  \
            case GDT_Float64:                           \
                object = new klass<double>(arg);        \
                break;                                  \
            }}
