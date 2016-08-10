%header %{
    #ifndef SWIG
    typedef struct
    {
        SV *fct;
        SV *data;
    } SavedEnv;
    #endif
    int callback_d_cp_vp(double d, const char *cp, void *vp)
    {
        int count, ret;
        SavedEnv *env_ptr = (SavedEnv *)vp;
        dSP;
        ENTER;
        SAVETMPS;
        PUSHMARK(SP);
        XPUSHs(sv_2mortal(newSVnv(d)));
        XPUSHs(sv_2mortal(newSVpv(cp, 0)));
        if (env_ptr->data)
            XPUSHs(env_ptr->data);
        PUTBACK;
        count = call_sv(env_ptr->fct, G_SCALAR);
        SPAGAIN;
        if (count != 1) {
            fprintf(stderr, "The callback must return only one value.\n");
            return 0; /* interrupt */
        }
        ret = POPi;
        PUTBACK;
        FREETMPS;
        LEAVE;
        return ret;
    }
    #ifndef SWIG
    static SV *VSIStdoutSetRedirectionFct = &PL_sv_undef;
    #endif
    size_t callback_fwrite(const void *ptr, size_t size, size_t nmemb,
                           FILE *stream)
    {
        dSP;
        ENTER;
        SAVETMPS;
        PUSHMARK(SP);
        XPUSHs(sv_2mortal(newSVpv((const char*)ptr, size*nmemb)));
        PUTBACK;
        call_sv(VSIStdoutSetRedirectionFct, G_DISCARD);
        FREETMPS;
        LEAVE;
        return size*nmemb;
    }
%}
