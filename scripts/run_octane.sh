#!/bin/bash

if [ $# -ne 3 ]
then
	echo "No or invalid arguments."
	echo "Syntax: $0 <dist/bin/js> <Octane/index.js> <times_to_run>"
	exit 0
fi

echo "********************"
echo "Running Octane"
echo "JS binary: `realpath $1`"
echo "Target: `realpath $2`"
echo "Will run $3 times..."
echo "********************"

TOTAL_SCORE=0

for i in `seq 1 $3`;
do
	OCTANE_SCORE=`$1 $2 | grep -i Score | awk '{print $4}'`
	
	echo "Run #$i score: $OCTANE_SCORE"
	TOTAL_SCORE=$(($TOTAL_SCORE + $OCTANE_SCORE))
done

echo "Average score: $(($TOTAL_SCORE / $3))"
