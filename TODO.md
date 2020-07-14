# TODO List

These are mostly just thoughts on things that could/should be done at
some point.

## Derivations

For efficiency map should use an array instead of single linked list.

## Components

Convert to lists of components and lists of resources to indexed list
for speedier lookups

Merge rules support for components in perl

## Packages

Add rpmlist and package fetch support with curl.

Add a proper epoch attribute rather than storing it in the version
attribute. This will make it easier to generate RPM and Deb filenames.

Functions for clearing package attributes for: arch, version, release,
context, derivation, category. Note that name cannot be unset.

Support for setting the allowed rules for a package name or
version. Debian and Redhat rules are subtlely different. It would be
really nice to catch to be able to catch mistakes on the server when
processing a profile. Package level or list level?

Support for setting default architecture on a list or set

Support for setting list of allowed architectures for list or set

Convert to 'container' model: from_rpmlist, from_rpm_dir, from_rpm_db

Add support for failure modes to lcfgpkglist_merge_list and all
callers: immediate (default) and continue. Also come up with some way
to collect all errors in 'continue' mode

## Resources

tags - add unique, add set functions - union, difference, intersection

## Utils

Improve code quality.

## Common

Expose common constants in Perl - macros/functions for checking value?
