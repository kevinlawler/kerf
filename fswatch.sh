#!/bin/bash  
# use:  fswatch a.out | ./fswatch.sh ./a.out
(while read; do "$@"; done)
