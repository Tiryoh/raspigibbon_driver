#!/bin/bash -eu

for i in `seq 0 2`; do
	RESULT=`cat /dev/rtswitch$i`
	echo /dev/rtswitch$i $RESULT
done

