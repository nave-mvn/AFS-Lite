FS_PATH=~/afs

#check if afs dir exists
if [ ! -d $FS_PATH ]; then
	mkdir $FS_PATH	
fi

if [[ $1 == '-m' ]]
then
	if [[ $2 == '-f' ]]
	then
		bin/venus -f $FS_PATH
	else
		bin/venus $FS_PATH
	fi
fi		

if [[ $1 == '-u' ]]
then
	fusermount -u $FS_PATH
	rm -rf $FS_PATH
fi
