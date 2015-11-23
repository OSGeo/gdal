%header %{
    void do_confess(const char *error, int push_to_error_stack) {
        SV *sv = newSVpv(error, 0);
        if (push_to_error_stack) {
            AV* error_stack = get_av("Geo::GDAL::error", 0);
            av_push(error_stack, sv);
        } else {
            sv = sv_2mortal(sv);
        }
        dSP;
        ENTER;
        SAVETMPS;
        PUSHMARK(SP);
        XPUSHs( sv );
        PUTBACK;
        call_pv("Carp::confess", G_DISCARD);
        /*
        confess never returns, so these will not get executed:
        FREETMPS;
        LEAVE;
        */
    }
    #define OUT_OF_MEMORY "Out of memory."
    #define CALL_FAILED "Call failed. Possible reason is an index out of range, mathematical problem, or something else."
    #define NEED_DEF "A parameter which must be defined or not empty, is not."
    #define WRONG_CLASS "Object has a wrong class."
    #define NEED_REF "A parameter which must be a reference, is not."
    #define NEED_ARRAY_REF "A parameter/item which must be an array reference, is not."
    #define NEED_BINARY_DATA "A parameter which must be binary data, is not."
    #define NEED_CODE_REF "A parameter which must be an anonymous subroutine, is not."
    #define WRONG_ITEM_IN_ARRAY "An item in an array parameter has wrong type."
    #define ARRAY_TO_XML_FAILED "An array parameter cannot be converted to an XMLTree."
%}
