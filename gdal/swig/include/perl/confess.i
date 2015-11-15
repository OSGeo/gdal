%inline %{
    void do_confess(const char *error) {
        dSP;
        ENTER;
        SAVETMPS;
        PUSHMARK(SP);
        XPUSHs( sv_2mortal(newSVpv(error, 0)) );
        PUTBACK;
        call_pv("Carp::confess", G_DISCARD);
        FREETMPS;
        LEAVE;
    }
%}
