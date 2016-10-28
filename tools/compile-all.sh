#!/bin/bash

exit_code=0

for config in `/bin/ls -H -I "bamboo*" configs`
do
    echo $config
    make clean > /dev/null
    make $config > /dev/null
    make silentoldconfig > /dev/null
    make kernel_elf > /dev/null
    if [ $? -eq 0 ]
    then
        echo "pass"
    else
        exit_code=1
        echo "$config failed"
    fi
done

exit $exit_code
