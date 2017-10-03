#!/bin/bash
#
# Copyright 2017, Data61
# Commonwealth Scientific and Industrial Research Organisation (CSIRO)
# ABN 41 687 119 230.
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(DATA61_BSD)

exit_code=0

for config in `/bin/ls -H -I "bamboo*" master-configs`
do
    echo $config
    make clean > /dev/null
    make $config > /dev/null
    make silentoldconfig > /dev/null
    env -i PATH="$PATH" make kernel_elf > /dev/null
    if [ $? -eq 0 ]
    then
        echo "pass"
    else
        exit_code=1
        echo "$config failed"
    fi
done

exit $exit_code
