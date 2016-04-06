#define type_switch_bb(sub,fd) switch (b1->GetRasterDataType()) {   \
    case GDT_Byte:                                                      \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<uint8_t,uint8_t>(b1, sub<uint8_t,uint8_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<uint8_t,uint16_t>(b1, sub<uint8_t,uint16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<uint8_t,int16_t>(b1, sub<uint8_t,int16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<uint8_t,uint32_t>(b1, sub<uint8_t,uint32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<uint8_t,int32_t>(b1, sub<uint8_t,int32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Float32:                                               \
            gma_two_bands_proc<uint8_t,float>(b1, sub<uint8_t,float>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Float64:                                               \
            gma_two_bands_proc<uint8_t,double>(b1, sub<uint8_t,double>, b2, &retval, arg, fd); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_UInt16:                                                    \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<uint16_t,uint8_t>(b1, sub<uint16_t,uint8_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<uint16_t,uint16_t>(b1, sub<uint16_t,uint16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<uint16_t,int16_t>(b1, sub<uint16_t,int16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<uint16_t,uint32_t>(b1, sub<uint16_t,uint32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<uint16_t,int32_t>(b1, sub<uint16_t,int32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Float32:                                               \
            gma_two_bands_proc<uint16_t,float>(b1, sub<uint16_t,float>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Float64:                                               \
            gma_two_bands_proc<uint16_t,double>(b1, sub<uint16_t,double>, b2, &retval, arg, fd); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Int16:                                                     \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<int16_t,uint8_t>(b1, sub<int16_t,uint8_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<int16_t,uint16_t>(b1, sub<int16_t,uint16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<int16_t,int16_t>(b1, sub<int16_t,int16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<int16_t,uint32_t>(b1, sub<int16_t,uint32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<int16_t,int32_t>(b1, sub<int16_t,int32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Float32:                                               \
            gma_two_bands_proc<int16_t,float>(b1, sub<int16_t,float>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Float64:                                               \
            gma_two_bands_proc<int16_t,double>(b1, sub<int16_t,double>, b2, &retval, arg, fd); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_UInt32:                                                    \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<uint32_t,uint8_t>(b1, sub<uint32_t,uint8_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<uint32_t,uint16_t>(b1, sub<uint32_t,uint16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<uint32_t,int16_t>(b1, sub<uint32_t,int16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<uint32_t,uint32_t>(b1, sub<uint32_t,uint32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<uint32_t,int32_t>(b1, sub<uint32_t,int32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Float32:                                               \
            gma_two_bands_proc<uint32_t,float>(b1, sub<uint32_t,float>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Float64:                                               \
            gma_two_bands_proc<uint32_t,double>(b1, sub<uint32_t,double>, b2, &retval, arg, fd); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Int32:                                                     \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<int32_t,uint8_t>(b1, sub<int32_t,uint8_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<int32_t,uint16_t>(b1, sub<int32_t,uint16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<int32_t,int16_t>(b1, sub<int32_t,int16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<int32_t,uint32_t>(b1, sub<int32_t,uint32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<int32_t,int32_t>(b1, sub<int32_t,int32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Float32:                                               \
            gma_two_bands_proc<int32_t,float>(b1, sub<int32_t,float>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Float64:                                               \
            gma_two_bands_proc<int32_t,double>(b1, sub<int32_t,double>, b2, &retval, arg, fd); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Float32:                                                   \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<float,uint8_t>(b1, sub<float,uint8_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<float,uint16_t>(b1, sub<float,uint16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<float,int16_t>(b1, sub<float,int16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<float,uint32_t>(b1, sub<float,uint32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<float,int32_t>(b1, sub<float,int32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Float32:                                               \
            gma_two_bands_proc<float,float>(b1, sub<float,float>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Float64:                                               \
            gma_two_bands_proc<float,double>(b1, sub<float,double>, b2, &retval, arg, fd); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Float64:                                                   \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<double,uint8_t>(b1, sub<double,uint8_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<double,uint16_t>(b1, sub<double,uint16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<double,int16_t>(b1, sub<double,int16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<double,uint32_t>(b1, sub<double,uint32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<double,int32_t>(b1, sub<double,int32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Float32:                                               \
            gma_two_bands_proc<double,float>(b1, sub<double,float>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Float64:                                               \
            gma_two_bands_proc<double,double>(b1, sub<double,double>, b2, &retval, arg, fd); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    default:                                                            \
        goto not_implemented_for_these_datatypes;                       \
    }

#define type_switch_bi(sub,fd) switch (b1->GetRasterDataType()) {   \
    case GDT_Byte:                                                      \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<uint8_t,uint8_t>(b1, sub<uint8_t,uint8_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<uint8_t,uint16_t>(b1, sub<uint8_t,uint16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<uint8_t,int16_t>(b1, sub<uint8_t,int16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<uint8_t,uint32_t>(b1, sub<uint8_t,uint32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<uint8_t,int32_t>(b1, sub<uint8_t,int32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_UInt16:                                                    \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<uint16_t,uint8_t>(b1, sub<uint16_t,uint8_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<uint16_t,uint16_t>(b1, sub<uint16_t,uint16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<uint16_t,int16_t>(b1, sub<uint16_t,int16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<uint16_t,uint32_t>(b1, sub<uint16_t,uint32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<uint16_t,int32_t>(b1, sub<uint16_t,int32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Int16:                                                     \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<int16_t,uint8_t>(b1, sub<int16_t,uint8_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<int16_t,uint16_t>(b1, sub<int16_t,uint16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<int16_t,int16_t>(b1, sub<int16_t,int16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<int16_t,uint32_t>(b1, sub<int16_t,uint32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<int16_t,int32_t>(b1, sub<int16_t,int32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_UInt32:                                                    \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<uint32_t,uint8_t>(b1, sub<uint32_t,uint8_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<uint32_t,uint16_t>(b1, sub<uint32_t,uint16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<uint32_t,int16_t>(b1, sub<uint32_t,int16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<uint32_t,uint32_t>(b1, sub<uint32_t,uint32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<uint32_t,int32_t>(b1, sub<uint32_t,int32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Int32:                                                     \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<int32_t,uint8_t>(b1, sub<int32_t,uint8_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<int32_t,uint16_t>(b1, sub<int32_t,uint16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<int32_t,int16_t>(b1, sub<int32_t,int16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<int32_t,uint32_t>(b1, sub<int32_t,uint32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<int32_t,int32_t>(b1, sub<int32_t,int32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Float32:                                                   \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<float,uint8_t>(b1, sub<float,uint8_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<float,uint16_t>(b1, sub<float,uint16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<float,int16_t>(b1, sub<float,int16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<float,uint32_t>(b1, sub<float,uint32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<float,int32_t>(b1, sub<float,int32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Float64:                                                   \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<double,uint8_t>(b1, sub<double,uint8_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<double,uint16_t>(b1, sub<double,uint16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<double,int16_t>(b1, sub<double,int16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<double,uint32_t>(b1, sub<double,uint32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<double,int32_t>(b1, sub<double,int32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    default:                                                            \
        goto not_implemented_for_these_datatypes;                       \
    }

#define type_switch_ib(sub,fd) switch (b1->GetRasterDataType()) {   \
    case GDT_Byte:                                                      \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<uint8_t,uint8_t>(b1, sub<uint8_t,uint8_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<uint8_t,uint16_t>(b1, sub<uint8_t,uint16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<uint8_t,int16_t>(b1, sub<uint8_t,int16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<uint8_t,uint32_t>(b1, sub<uint8_t,uint32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<uint8_t,int32_t>(b1, sub<uint8_t,int32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Float32:                                               \
            gma_two_bands_proc<uint8_t,float>(b1, sub<uint8_t,float>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Float64:                                               \
            gma_two_bands_proc<uint8_t,double>(b1, sub<uint8_t,double>, b2, &retval, arg, fd); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_UInt16:                                                    \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<uint16_t,uint8_t>(b1, sub<uint16_t,uint8_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<uint16_t,uint16_t>(b1, sub<uint16_t,uint16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<uint16_t,int16_t>(b1, sub<uint16_t,int16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<uint16_t,uint32_t>(b1, sub<uint16_t,uint32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<uint16_t,int32_t>(b1, sub<uint16_t,int32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Float32:                                               \
            gma_two_bands_proc<uint16_t,float>(b1, sub<uint16_t,float>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Float64:                                               \
            gma_two_bands_proc<uint16_t,double>(b1, sub<uint16_t,double>, b2, &retval, arg, fd); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Int16:                                                     \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<int16_t,uint8_t>(b1, sub<int16_t,uint8_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<int16_t,uint16_t>(b1, sub<int16_t,uint16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<int16_t,int16_t>(b1, sub<int16_t,int16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<int16_t,uint32_t>(b1, sub<int16_t,uint32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<int16_t,int32_t>(b1, sub<int16_t,int32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Float32:                                               \
            gma_two_bands_proc<int16_t,float>(b1, sub<int16_t,float>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Float64:                                               \
            gma_two_bands_proc<int16_t,double>(b1, sub<int16_t,double>, b2, &retval, arg, fd); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_UInt32:                                                    \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<uint32_t,uint8_t>(b1, sub<uint32_t,uint8_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<uint32_t,uint16_t>(b1, sub<uint32_t,uint16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<uint32_t,int16_t>(b1, sub<uint32_t,int16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<uint32_t,uint32_t>(b1, sub<uint32_t,uint32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<uint32_t,int32_t>(b1, sub<uint32_t,int32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Float32:                                               \
            gma_two_bands_proc<uint32_t,float>(b1, sub<uint32_t,float>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Float64:                                               \
            gma_two_bands_proc<uint32_t,double>(b1, sub<uint32_t,double>, b2, &retval, arg, fd); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Int32:                                                     \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<int32_t,uint8_t>(b1, sub<int32_t,uint8_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<int32_t,uint16_t>(b1, sub<int32_t,uint16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<int32_t,int16_t>(b1, sub<int32_t,int16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<int32_t,uint32_t>(b1, sub<int32_t,uint32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<int32_t,int32_t>(b1, sub<int32_t,int32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Float32:                                               \
            gma_two_bands_proc<int32_t,float>(b1, sub<int32_t,float>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Float64:                                               \
            gma_two_bands_proc<int32_t,double>(b1, sub<int32_t,double>, b2, &retval, arg, fd); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    default:                                                            \
        goto not_implemented_for_these_datatypes;                       \
    }

#define type_switch_ii(sub,fd) switch (b1->GetRasterDataType()) {   \
    case GDT_Byte:                                                      \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<uint8_t,uint8_t>(b1, sub<uint8_t,uint8_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<uint8_t,uint16_t>(b1, sub<uint8_t,uint16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<uint8_t,int16_t>(b1, sub<uint8_t,int16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<uint8_t,uint32_t>(b1, sub<uint8_t,uint32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<uint8_t,int32_t>(b1, sub<uint8_t,int32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_UInt16:                                                    \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<uint16_t,uint8_t>(b1, sub<uint16_t,uint8_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<uint16_t,uint16_t>(b1, sub<uint16_t,uint16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<uint16_t,int16_t>(b1, sub<uint16_t,int16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<uint16_t,uint32_t>(b1, sub<uint16_t,uint32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<uint16_t,int32_t>(b1, sub<uint16_t,int32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Int16:                                                     \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<int16_t,uint8_t>(b1, sub<int16_t,uint8_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<int16_t,uint16_t>(b1, sub<int16_t,uint16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<int16_t,int16_t>(b1, sub<int16_t,int16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<int16_t,uint32_t>(b1, sub<int16_t,uint32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<int16_t,int32_t>(b1, sub<int16_t,int32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_UInt32:                                                    \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<uint32_t,uint8_t>(b1, sub<uint32_t,uint8_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<uint32_t,uint16_t>(b1, sub<uint32_t,uint16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<uint32_t,int16_t>(b1, sub<uint32_t,int16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<uint32_t,uint32_t>(b1, sub<uint32_t,uint32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<uint32_t,int32_t>(b1, sub<uint32_t,int32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Int32:                                                     \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<int32_t,uint8_t>(b1, sub<int32_t,uint8_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<int32_t,uint16_t>(b1, sub<int32_t,uint16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<int32_t,int16_t>(b1, sub<int32_t,int16_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<int32_t,uint32_t>(b1, sub<int32_t,uint32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<int32_t,int32_t>(b1, sub<int32_t,int32_t>, b2, &retval, arg, fd); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    default:                                                            \
        goto not_implemented_for_these_datatypes;                       \
    }

#define type_switch_single(sub,fd) switch (b->GetRasterDataType()) {    \
    case GDT_Byte:                                                      \
        gma_proc_compute_value<uint8_t>(b, sub<uint8_t>, &retval, arg, fd); \
        break;                                                          \
    case GDT_UInt16:                                                    \
        gma_proc_compute_value<uint16_t>(b, sub<uint16_t>, &retval, arg, fd); \
        break;                                                          \
    case GDT_Int16:                                                     \
        gma_proc_compute_value<int16_t>(b, sub<int16_t>, &retval, arg, fd); \
        break;                                                          \
    case GDT_UInt32:                                                    \
        gma_proc_compute_value<uint32_t>(b, sub<uint32_t>, &retval, arg, fd); \
        break;                                                          \
    case GDT_Int32:                                                     \
        gma_proc_compute_value<int32_t>(b, sub<int32_t>, &retval, arg, fd); \
        break;                                                          \
    case GDT_Float32:                                                   \
        gma_proc_compute_value<float>(b, sub<float>, &retval, arg, fd); \
        break;                                                          \
    case GDT_Float64:                                                   \
        gma_proc_compute_value<double>(b, sub<double>, &retval, arg, fd); \
        break;                                                          \
    default:                                                            \
        goto not_implemented_for_this_datatype;                         \
    }

#define type_switch_single_i(sub,fd) switch (b->GetRasterDataType()) {  \
    case GDT_Byte:                                                      \
        gma_proc_compute_value<uint8_t>(b, sub<uint8_t>, &retval, arg, fd); \
        break;                                                          \
    case GDT_UInt16:                                                    \
        gma_proc_compute_value<uint16_t>(b, sub<uint16_t>, &retval, arg, fd); \
        break;                                                          \
    case GDT_Int16:                                                     \
        gma_proc_compute_value<int16_t>(b, sub<int16_t>, &retval, arg, fd); \
        break;                                                          \
    case GDT_UInt32:                                                    \
        gma_proc_compute_value<uint32_t>(b, sub<uint32_t>, &retval, arg, fd); \
        break;                                                          \
    case GDT_Int32:                                                     \
        gma_proc_compute_value<int32_t>(b, sub<int32_t>, &retval, arg, fd); \
        break;                                                          \
    default:                                                            \
        goto not_implemented_for_this_datatype;                         \
    }

#define type_switch_arg(sub) switch (b->GetRasterDataType()) {          \
    case GDT_Byte:                                                      \
        gma_with_arg_proc<uint8_t>(b, sub<uint8_t>, arg);               \
        break;                                                          \
    case GDT_UInt16:                                                    \
        gma_with_arg_proc<uint16_t>(b, sub<uint16_t>, arg);             \
        break;                                                          \
    case GDT_Int16:                                                     \
        gma_with_arg_proc<int16_t>(b, sub<int16_t>, arg);               \
        break;                                                          \
    case GDT_UInt32:                                                    \
        gma_with_arg_proc<uint32_t>(b, sub<uint32_t>, arg);             \
        break;                                                          \
    case GDT_Int32:                                                     \
        gma_with_arg_proc<int32_t>(b, sub<int32_t>, arg);               \
        break;                                                          \
    case GDT_Float32:                                                   \
        gma_with_arg_proc<float>(b, sub<float>, arg);                   \
        break;                                                          \
    case GDT_Float64:                                                   \
        gma_with_arg_proc<double>(b, sub<double>, arg);                 \
        break;                                                          \
    default:                                                            \
        goto not_implemented_for_this_datatype;                         \
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
