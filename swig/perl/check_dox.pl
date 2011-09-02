# run to check what methods have been doxymented and which not
# also checks if something is in dox that maybe shouldn't be

report(check('GDAL'));
report(check('OGR'));
report(check('OSR'));

sub report {
    my($dox, $pm) = @_;
    for my $class (sort keys %$pm) {
	my @m;	
	for my $method (sort keys %{$pm->{$class}}) {
	    push @m, "  $method not in dox\n" unless $dox->{$class}{$method};
	}
	next unless @m;
	print "$class\n@m";
    }
    return;
    for my $class (sort keys %$dox) {
	my @m;	
	for my $method (sort keys %{$dox->{$class}}) {
	    push @m, "  $method not in pm\n" unless $pm->{$class}{$method};
	}
	next unless @m;
	print "$class\n@m";
    }
}

sub check {
    my($file) = @_;
    @gdal = `cat lib/Geo/$file.dox`;
    my %pm;
    my %dox;
    for (@gdal) {
	($_) = /(.*?)\(/ if /\(/;
	@l = split /\s+/;
	next unless @l;
	if ($l[$#l-1] eq '@class') {
	    $class = $l[$#l];
	    $class =~ s/\s+$//;
	    next;
	}
	if (/^##/) {	
	    ($method) = $l[$#l];
	    $method =~ s/^\\//;
	    $method =~ s/^[\$\@\%]//;
	    $dox{$class}{$method} = 1;
	    next;
	}
    }
    @gdal = `cat lib/Geo/$file.pm`;
    for (@gdal) {
	@l = split /\s+/;
	if (/^package/) {
	    $class = $l[$#l];
	    $class =~ s/;//;
	    $class =~ s/\s+$//;
	    next;
	}
	if (/^\*/) {	
	    $method = $l[0];
	    $method =~ s/\*//;
	    next if $method =~ /^_/;
	    next if $method =~ /^GDAL_/;
	    next if $method =~ /^swig_/;
	    next if $method =~ /^VSI/;
	    next if $method =~ /^SRS_/;
	    next if $method =~ /^[0-9A-Z_]+$/;
	    $pm{$class}{$method} = 1;
	    next;
	}
	if (/^sub/) {	
	    $method = $l[1];
	    next if $method =~ /^_/;
	    next if $method =~ /^GDAL_/;
	    next if $method =~ /^swig_/;
	    next if $method =~ /^VSI/;
	    next if $method =~ /^SRS_/;
	    next if $method =~ /^[0-9A-Z_]+$/;
	    $pm{$class}{$method} = 1;
	    next;
	}
    }
    return (\%dox, \%pm);
}
