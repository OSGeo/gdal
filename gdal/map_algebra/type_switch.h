#define type_switch_bb(sub,fd,arg) switch (b1->GetRasterDataType()) {   \
    case GDT_Byte:                                                      \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<uint8_t,uint8_t>(b1, sub<uint8_t,uint8_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<uint8_t,uint16_t>(b1, sub<uint8_t,uint16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<uint8_t,int16_t>(b1, sub<uint8_t,int16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<uint8_t,uint32_t>(b1, sub<uint8_t,uint32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<uint8_t,int32_t>(b1, sub<uint8_t,int32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Float32:                                               \
            gma_two_bands_proc<uint8_t,float>(b1, sub<uint8_t,float>, b2, fd, arg); \
            break;                                                      \
        case GDT_Float64:                                               \
            gma_two_bands_proc<uint8_t,double>(b1, sub<uint8_t,double>, b2, fd, arg); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_UInt16:                                                    \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<uint16_t,uint8_t>(b1, sub<uint16_t,uint8_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<uint16_t,uint16_t>(b1, sub<uint16_t,uint16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<uint16_t,int16_t>(b1, sub<uint16_t,int16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<uint16_t,uint32_t>(b1, sub<uint16_t,uint32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<uint16_t,int32_t>(b1, sub<uint16_t,int32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Float32:                                               \
            gma_two_bands_proc<uint16_t,float>(b1, sub<uint16_t,float>, b2, fd, arg); \
            break;                                                      \
        case GDT_Float64:                                               \
            gma_two_bands_proc<uint16_t,double>(b1, sub<uint16_t,double>, b2, fd, arg); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Int16:                                                     \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<int16_t,uint8_t>(b1, sub<int16_t,uint8_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<int16_t,uint16_t>(b1, sub<int16_t,uint16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<int16_t,int16_t>(b1, sub<int16_t,int16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<int16_t,uint32_t>(b1, sub<int16_t,uint32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<int16_t,int32_t>(b1, sub<int16_t,int32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Float32:                                               \
            gma_two_bands_proc<int16_t,float>(b1, sub<int16_t,float>, b2, fd, arg); \
            break;                                                      \
        case GDT_Float64:                                               \
            gma_two_bands_proc<int16_t,double>(b1, sub<int16_t,double>, b2, fd, arg); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_UInt32:                                                    \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<uint32_t,uint8_t>(b1, sub<uint32_t,uint8_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<uint32_t,uint16_t>(b1, sub<uint32_t,uint16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<uint32_t,int16_t>(b1, sub<uint32_t,int16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<uint32_t,uint32_t>(b1, sub<uint32_t,uint32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<uint32_t,int32_t>(b1, sub<uint32_t,int32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Float32:                                               \
            gma_two_bands_proc<uint32_t,float>(b1, sub<uint32_t,float>, b2, fd, arg); \
            break;                                                      \
        case GDT_Float64:                                               \
            gma_two_bands_proc<uint32_t,double>(b1, sub<uint32_t,double>, b2, fd, arg); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Int32:                                                     \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<int32_t,uint8_t>(b1, sub<int32_t,uint8_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<int32_t,uint16_t>(b1, sub<int32_t,uint16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<int32_t,int16_t>(b1, sub<int32_t,int16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<int32_t,uint32_t>(b1, sub<int32_t,uint32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<int32_t,int32_t>(b1, sub<int32_t,int32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Float32:                                               \
            gma_two_bands_proc<int32_t,float>(b1, sub<int32_t,float>, b2, fd, arg); \
            break;                                                      \
        case GDT_Float64:                                               \
            gma_two_bands_proc<int32_t,double>(b1, sub<int32_t,double>, b2, fd, arg); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Float32:                                                   \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<float,uint8_t>(b1, sub<float,uint8_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<float,uint16_t>(b1, sub<float,uint16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<float,int16_t>(b1, sub<float,int16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<float,uint32_t>(b1, sub<float,uint32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<float,int32_t>(b1, sub<float,int32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Float32:                                               \
            gma_two_bands_proc<float,float>(b1, sub<float,float>, b2, fd, arg); \
            break;                                                      \
        case GDT_Float64:                                               \
            gma_two_bands_proc<float,double>(b1, sub<float,double>, b2, fd, arg); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Float64:                                                   \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<double,uint8_t>(b1, sub<double,uint8_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<double,uint16_t>(b1, sub<double,uint16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<double,int16_t>(b1, sub<double,int16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<double,uint32_t>(b1, sub<double,uint32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<double,int32_t>(b1, sub<double,int32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Float32:                                               \
            gma_two_bands_proc<double,float>(b1, sub<double,float>, b2, fd, arg); \
            break;                                                      \
        case GDT_Float64:                                               \
            gma_two_bands_proc<double,double>(b1, sub<double,double>, b2, fd, arg); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    default:                                                            \
        goto not_implemented_for_these_datatypes;                       \
    }

#define type_switch_bi(sub,fd,arg) switch (b1->GetRasterDataType()) {   \
    case GDT_Byte:                                                      \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<uint8_t,uint8_t>(b1, sub<uint8_t,uint8_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<uint8_t,uint16_t>(b1, sub<uint8_t,uint16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<uint8_t,int16_t>(b1, sub<uint8_t,int16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<uint8_t,uint32_t>(b1, sub<uint8_t,uint32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<uint8_t,int32_t>(b1, sub<uint8_t,int32_t>, b2, fd, arg); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_UInt16:                                                    \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<uint16_t,uint8_t>(b1, sub<uint16_t,uint8_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<uint16_t,uint16_t>(b1, sub<uint16_t,uint16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<uint16_t,int16_t>(b1, sub<uint16_t,int16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<uint16_t,uint32_t>(b1, sub<uint16_t,uint32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<uint16_t,int32_t>(b1, sub<uint16_t,int32_t>, b2, fd, arg); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Int16:                                                     \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<int16_t,uint8_t>(b1, sub<int16_t,uint8_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<int16_t,uint16_t>(b1, sub<int16_t,uint16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<int16_t,int16_t>(b1, sub<int16_t,int16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<int16_t,uint32_t>(b1, sub<int16_t,uint32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<int16_t,int32_t>(b1, sub<int16_t,int32_t>, b2, fd, arg); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_UInt32:                                                    \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<uint32_t,uint8_t>(b1, sub<uint32_t,uint8_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<uint32_t,uint16_t>(b1, sub<uint32_t,uint16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<uint32_t,int16_t>(b1, sub<uint32_t,int16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<uint32_t,uint32_t>(b1, sub<uint32_t,uint32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<uint32_t,int32_t>(b1, sub<uint32_t,int32_t>, b2, fd, arg); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Int32:                                                     \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<int32_t,uint8_t>(b1, sub<int32_t,uint8_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<int32_t,uint16_t>(b1, sub<int32_t,uint16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<int32_t,int16_t>(b1, sub<int32_t,int16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<int32_t,uint32_t>(b1, sub<int32_t,uint32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<int32_t,int32_t>(b1, sub<int32_t,int32_t>, b2, fd, arg); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Float32:                                                   \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<float,uint8_t>(b1, sub<float,uint8_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<float,uint16_t>(b1, sub<float,uint16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<float,int16_t>(b1, sub<float,int16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<float,uint32_t>(b1, sub<float,uint32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<float,int32_t>(b1, sub<float,int32_t>, b2, fd, arg); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Float64:                                                   \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<double,uint8_t>(b1, sub<double,uint8_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<double,uint16_t>(b1, sub<double,uint16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<double,int16_t>(b1, sub<double,int16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<double,uint32_t>(b1, sub<double,uint32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<double,int32_t>(b1, sub<double,int32_t>, b2, fd, arg); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    default:                                                            \
        goto not_implemented_for_these_datatypes;                       \
    }

#define type_switch_ib(sub,fd,arg) switch (b1->GetRasterDataType()) {   \
    case GDT_Byte:                                                      \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<uint8_t,uint8_t>(b1, sub<uint8_t,uint8_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<uint8_t,uint16_t>(b1, sub<uint8_t,uint16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<uint8_t,int16_t>(b1, sub<uint8_t,int16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<uint8_t,uint32_t>(b1, sub<uint8_t,uint32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<uint8_t,int32_t>(b1, sub<uint8_t,int32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Float32:                                               \
            gma_two_bands_proc<uint8_t,float>(b1, sub<uint8_t,float>, b2, fd, arg); \
            break;                                                      \
        case GDT_Float64:                                               \
            gma_two_bands_proc<uint8_t,double>(b1, sub<uint8_t,double>, b2, fd, arg); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_UInt16:                                                    \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<uint16_t,uint8_t>(b1, sub<uint16_t,uint8_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<uint16_t,uint16_t>(b1, sub<uint16_t,uint16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<uint16_t,int16_t>(b1, sub<uint16_t,int16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<uint16_t,uint32_t>(b1, sub<uint16_t,uint32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<uint16_t,int32_t>(b1, sub<uint16_t,int32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Float32:                                               \
            gma_two_bands_proc<uint16_t,float>(b1, sub<uint16_t,float>, b2, fd, arg); \
            break;                                                      \
        case GDT_Float64:                                               \
            gma_two_bands_proc<uint16_t,double>(b1, sub<uint16_t,double>, b2, fd, arg); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Int16:                                                     \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<int16_t,uint8_t>(b1, sub<int16_t,uint8_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<int16_t,uint16_t>(b1, sub<int16_t,uint16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<int16_t,int16_t>(b1, sub<int16_t,int16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<int16_t,uint32_t>(b1, sub<int16_t,uint32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<int16_t,int32_t>(b1, sub<int16_t,int32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Float32:                                               \
            gma_two_bands_proc<int16_t,float>(b1, sub<int16_t,float>, b2, fd, arg); \
            break;                                                      \
        case GDT_Float64:                                               \
            gma_two_bands_proc<int16_t,double>(b1, sub<int16_t,double>, b2, fd, arg); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_UInt32:                                                    \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<uint32_t,uint8_t>(b1, sub<uint32_t,uint8_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<uint32_t,uint16_t>(b1, sub<uint32_t,uint16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<uint32_t,int16_t>(b1, sub<uint32_t,int16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<uint32_t,uint32_t>(b1, sub<uint32_t,uint32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<uint32_t,int32_t>(b1, sub<uint32_t,int32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Float32:                                               \
            gma_two_bands_proc<uint32_t,float>(b1, sub<uint32_t,float>, b2, fd, arg); \
            break;                                                      \
        case GDT_Float64:                                               \
            gma_two_bands_proc<uint32_t,double>(b1, sub<uint32_t,double>, b2, fd, arg); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Int32:                                                     \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<int32_t,uint8_t>(b1, sub<int32_t,uint8_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<int32_t,uint16_t>(b1, sub<int32_t,uint16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<int32_t,int16_t>(b1, sub<int32_t,int16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<int32_t,uint32_t>(b1, sub<int32_t,uint32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<int32_t,int32_t>(b1, sub<int32_t,int32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Float32:                                               \
            gma_two_bands_proc<int32_t,float>(b1, sub<int32_t,float>, b2, fd, arg); \
            break;                                                      \
        case GDT_Float64:                                               \
            gma_two_bands_proc<int32_t,double>(b1, sub<int32_t,double>, b2, fd, arg); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    default:                                                            \
        goto not_implemented_for_these_datatypes;                       \
    }

#define type_switch_ii(sub,fd,arg) switch (b1->GetRasterDataType()) {   \
    case GDT_Byte:                                                      \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<uint8_t,uint8_t>(b1, sub<uint8_t,uint8_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<uint8_t,uint16_t>(b1, sub<uint8_t,uint16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<uint8_t,int16_t>(b1, sub<uint8_t,int16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<uint8_t,uint32_t>(b1, sub<uint8_t,uint32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<uint8_t,int32_t>(b1, sub<uint8_t,int32_t>, b2, fd, arg); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_UInt16:                                                    \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<uint16_t,uint8_t>(b1, sub<uint16_t,uint8_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<uint16_t,uint16_t>(b1, sub<uint16_t,uint16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<uint16_t,int16_t>(b1, sub<uint16_t,int16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<uint16_t,uint32_t>(b1, sub<uint16_t,uint32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<uint16_t,int32_t>(b1, sub<uint16_t,int32_t>, b2, fd, arg); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Int16:                                                     \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<int16_t,uint8_t>(b1, sub<int16_t,uint8_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<int16_t,uint16_t>(b1, sub<int16_t,uint16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<int16_t,int16_t>(b1, sub<int16_t,int16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<int16_t,uint32_t>(b1, sub<int16_t,uint32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<int16_t,int32_t>(b1, sub<int16_t,int32_t>, b2, fd, arg); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_UInt32:                                                    \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<uint32_t,uint8_t>(b1, sub<uint32_t,uint8_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<uint32_t,uint16_t>(b1, sub<uint32_t,uint16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<uint32_t,int16_t>(b1, sub<uint32_t,int16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<uint32_t,uint32_t>(b1, sub<uint32_t,uint32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<uint32_t,int32_t>(b1, sub<uint32_t,int32_t>, b2, fd, arg); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    case GDT_Int32:                                                     \
        switch (b2->GetRasterDataType()) {                              \
        case GDT_Byte:                                                  \
            gma_two_bands_proc<int32_t,uint8_t>(b1, sub<int32_t,uint8_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt16:                                                \
            gma_two_bands_proc<int32_t,uint16_t>(b1, sub<int32_t,uint16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int16:                                                 \
            gma_two_bands_proc<int32_t,int16_t>(b1, sub<int32_t,int16_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_UInt32:                                                \
            gma_two_bands_proc<int32_t,uint32_t>(b1, sub<int32_t,uint32_t>, b2, fd, arg); \
            break;                                                      \
        case GDT_Int32:                                                 \
            gma_two_bands_proc<int32_t,int32_t>(b1, sub<int32_t,int32_t>, b2, fd, arg); \
            break;                                                      \
        default:                                                        \
            goto not_implemented_for_these_datatypes;                   \
        }                                                               \
        break;                                                          \
    default:                                                            \
        goto not_implemented_for_these_datatypes;                       \
    }
