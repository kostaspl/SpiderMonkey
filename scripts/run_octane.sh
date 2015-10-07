#!/bin/bash

if [ $# -ne 2 ]
then
	echo "No or invalid arguments."
	echo "Syntax: $0 <Octane/index.js> <times_to_run>"
	exit 0
fi

echo "********************"
echo "Running Octane @ $1"
echo "Will run $2 times..."
echo "********************"

TOTAL_SCORE=0

for i in `seq 1 $2`;
do
	SCRIPT_DIR=`realpath $0`
	SCRIPT_DIR=`dirname $SCRIPT_DIR`

	OCTANE_SCORE=`$SCRIPT_DIR/../js/src/build_OPT.OBJ/dist/bin/js $1 | grep -i Score | awk '{print $4}'`
	
	echo "Run #$i score: $OCTANE_SCORE"
	TOTAL_SCORE=$(($TOTAL_SCORE + $OCTANE_SCORE))
done

echo "Average score: $(($TOTAL_SCORE / $2))"
