#!/bin/bash

for i in `seq 1 100`;
do
    ../build/cat-file -n /ndn/edu/ucla/remap/ndnfs/stupidfile.txt -r >> temp.txt
done

(cat temp.txt | grep Throughput) > repo.txt

rm temp.txt
