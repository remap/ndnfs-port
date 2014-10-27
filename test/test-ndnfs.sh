#!/bin/bash

for i in `seq 1 100`;
do
    ../build/cat-file -n /ndn/edu/ucla/remap/ndnfs/stupidfile.txt >> temp1.txt
done

(cat temp1.txt | grep Throughput) > ndnfs.txt

rm temp1.txt
