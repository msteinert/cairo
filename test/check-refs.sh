#!/bin/bash

current_dir=$(pwd)

# Move to the reference directory as needed
if [ $(basename $current_dir) != 'test' ]; then
    if [ -d test ]; then
        cd test || exit 1
    fi
fi
if [ $(basename $current_dir) != 'reference' ]; then
    if [ -d reference ]; then
        cd reference || exit 2
    fi
fi

pdiff=../pdiff/perceptualdiff
if [ ! -e "${pdiff}" ]; then
    echo "Error:  requires ${pdiff} executable"
    exit 3
fi

for file in *.ref.png; do
    test=$(echo $file | cut -d'.' -f1)
    target=$(echo $file | cut -d'.' -f2)
    format=$(echo $file | cut -d'.' -f3)
    notes=""
    ref=""
    result=""

    if [ $target = 'base' ]; then
        # Ignore the base images for this script's purposes
        continue
    elif [ $target = 'ref' ]; then
        # This is actually the baseline reference image
        continue
    elif [ $format = 'ref' ]; then
        # This is either a format-specific reference, or a target-specific/format-generic image
        # In either case, compare it against the generic reference image
        ref="$test.ref.png"
    else
        # Prefer the target-specific/format-generic reference image, if available
	ref="$test.$target.ref.png"
	if [ ! -e $ref ]; then
            ref="$test.$format.ref.png"
	fi
    fi

    if [ -e $ref ]; then
        # Run perceptualdiff with minimum threshold
        pdiff_output=$($pdiff $ref $file -threshold 1)
        result=${pdiff_output%:*}
        notes=$(echo "${pdiff_output#*: }" | tail -n 1)
        if [ "$result" = "PASS" ] && [ "$notes" = "Images are binary identical" ]; then
	    printf "redundant: %s is binary identical to %s\n" $file $ref
            notes=""
        fi
    fi

done
