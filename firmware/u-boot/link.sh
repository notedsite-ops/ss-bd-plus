
if [ -e file_link.txt ];   then
	l=`awk 'END{print NR}' file_link.txt`
	for ((i=1; i<=$l; i++)); do
	
	 source=$(awk  -v nr=$i 'NR==nr {print $1}' file_link.txt)
	 dest=$(awk  -v nr=$i 'NR==nr {print $3}' file_link.txt)
		 
		  if [ ! -e $dest ] && [ "$dest" != "" ]; then
		      echo "$dest not exist"
		      cp $source $dest             #dest not exist, so copy from source file
		  elif [ "$dest" != "" ]; then

   	     diff $source $dest		  
   	     RES=$?
		     if [ $RES -ne 0 ]; then         
		       echo "$dest not the same as $source"
		       cp $source $dest 		      
		     fi 
		  fi		 
    done
fi
#mt8530_base_config success
