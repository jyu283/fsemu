#!/bin/bash

sLocals='1 20 40 60 80 100'
sRestricts='N S'
tLocals='1 10 20 30 50'
depths='S M D A'

FILE=$1
Width='Moderate'


for sLocal in $sLocals
do
    for sRestrict in $sRestricts
    do
	for tLocal in $tLocals
	do
	    for depth in $depths
	    do	
		echo "NEXT WORKLOAD"
		./fsgen -F $FILE -W Depth= $depth sLocal= $sLocal sRestrict= $sRestrict tLocal= $tLocal Num= 1000 File= "S_${sLocal}_R_${sRestrict}_T_${tLocal}_D_${depth}_${Width}_Workload.txt" 
		#sleep 5
	    done
	    
	done
	
    done
    
done

