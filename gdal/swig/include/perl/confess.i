%inline %{
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
%}
