#!/bin/bash -eu
for i in `seq 0 3`; do
	echo 1 > /dev/rtled$i
	RESULT=$?
	sleep 0.5
	[[ $RESULT > 0 ]] && echo $RESULT
done
for i in `seq 0 3`; do
	echo 0 > /dev/rtled$i
	RESULT=$?
	sleep 0.5
	[[ $RESULT > 0 ]] && echo $RESULT
done
