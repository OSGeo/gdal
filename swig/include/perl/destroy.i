%define ALTERED_DESTROY(class, modulec, delete_class)
%feature("shadow") ~class()
%{
sub DESTROY {
    my $self = shift;
    unless ($self->isa('SCALAR')) {
        return unless $self->isa('HASH');
        $self = tied(%{$self});
        return unless defined $self;
    }
    my $code = $Geo::GDAL::stdout_redirection{$self};
    delete $Geo::GDAL::stdout_redirection{$self};
    delete $ITERATORS{$self};
    if (exists $OWNER{$self}) {
        Geo::modulec::delete_class($self);
        delete $OWNER{$self};
    }
    $self->RELEASE_PARENT;
    if ($code) {
        Geo::GDAL::VSIStdoutUnsetRedirection();
        $code->close;
    }

}
%}
%enddef
