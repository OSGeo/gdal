%fragment("number_to_sv", "header") %{
    SV *number_to_sv(gma_number_t *nr) {
        if (nr->is_unsigned())
            return newSVuv(nr->value_as_unsigned());
        else if (nr->is_integer())
            return newSViv(nr->value_as_int());
        return newSVnv(nr->value_as_double());
    }
%}


%typemap(out,fragment="number_to_sv") gma_number_t * {
    $result = number_to_sv($1);
    sv_2mortal($result);
    argvi++;
}

%typemap(typecheck,precedence=200) gma_bins_t * {
    if (SvROK($input) && (SvTYPE(SvRV($input))==SVt_PVAV)) {
        AV *av = (AV*)SvRV($input);
        $1 = 1;
        for (int i = 0; i <= av_top_index(av); i++) {
             SV **sv = av_fetch(av, i, 0);
             if (SvROK(*sv)) {
                 $1 = 0;
                 break;
             }
        }
    } else
        $1 = 0;
}

%typemap(typecheck,precedence=201) gma_pair_t * {
    $1 = (SvROK($input) && (SvTYPE(SvRV($input))==SVt_PVAV) && av_top_index((AV*)SvRV($input))==1) ? 1 : 0;
}

%typemap(in) gma_bins_t * {
    AV *av = (AV*)SvRV($input);
    $1 = arg1->new_bins();
    for (int i = 0; i <= av_top_index(av); i++) {
        SV **sv = av_fetch(av, i, 0);
        $1->push(SvNV(*sv));
    }
}

%typemap(in) gma_pair_t * {
    AV *av = (AV*)SvRV($input);
    $1 = arg1->new_pair();
}

%typemap(out,fragment="number_to_sv") gma_histogram_t * {
    AV* av = (AV*)sv_2mortal((SV*)newAV());
    for (unsigned int i = 0; i < $1->size(); i++) {
        gma_pair_t *kv = (gma_pair_t *)$1->at(i);
        AV* av2 = newAV();
        /* kv is an interval=>number or number=>number */
            if (kv->first()->get_class() == gma_pair) {
                gma_pair_t *key = (gma_pair_t*)kv->first();
                SV *first = number_to_sv((gma_number_t*)key->first());
                SV *second = number_to_sv((gma_number_t*)key->second());
                SV *val = number_to_sv((gma_number_t*)kv->second());
                av_push(av2, first);
                av_push(av2, second);
                av_push(av2, val);
            } else {
                SV *first = number_to_sv((gma_number_t*)kv->first());
                SV *second = number_to_sv((gma_number_t*)kv->second());
                av_push(av2, first);
                av_push(av2, second);
            }
        av_push(av, newRV((SV*)av2));
    }
    $result = newRV((SV*)av);
    sv_2mortal($result);
    argvi++;
}
