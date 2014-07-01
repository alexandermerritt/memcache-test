#! /usr/bin/env bash
set -e
set -u

POW_MIN=20
POW_MAX=23
THREADS_MIN=1
THREADS_MAX=1
COUNT=2000

TESTBIN='./test'

echo "Data put into files with name <objectSize>.<numThreads>"

for ((t=THREADS_MIN; t<=THREADS_MAX; t++))
do
    for ((p=POW_MIN; p<=POW_MAX; p++))
    do
        objectSize=$((1<<p))
        filename=${objectSize}.${t}
        [[ -e $filename ]] && continue # remove file to repeat experiment
        echo $objectSize bytes and $t threads
        $TESTBIN $t $objectSize $COUNT > $filename 2>&1
    done
done

