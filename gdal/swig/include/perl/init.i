%perlcode %{

    # keeper maintains child -> parent relationships
    # child is kept as a key, i.e., string not the real object
    # parent is kept as the value, i.e., a real object
    # a child may have only one parent!
    # call these as Geo::GDAL::*

    my %keeper;
    my %notes;

    sub note {
        my ($object, $note) = @_;
        if (defined wantarray) {
            return unless $note{$object};
            return $notes{$object}{$note};
        }
        $notes{$object}{$note} = 1;
    }

    sub unnote {
        my ($object, $note) = @_;
        if ($note) {
            delete $notes{$object}{$note};
        } else {
            delete $notes{$object};
        }
    }

    sub keep {
        my ($child, $parent) = @_;
        $keeper{tied(%$child)} = $parent;
        return $child;
    }

    # this is called from RELEASE_PARENT, so the child is already the tied one
    sub unkeep {
        my ($child) = @_;
        delete $keeper{$child};
    }

    sub parent {
        my ($child) = @_;
        $child = tied(%$child) if $child->isa('HASH');
        return $keeper{$child};
    }

%}
