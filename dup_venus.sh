FS_PATH=~/afs_dup

#check if afs dir exists
if [ ! -d $FS_PATH ]; then
	mkdir $FS_PATH	
fi

if [[ $1 == '-m' ]]
then
	if [[ $2 == '-f' ]]
	then
		bin/venus -f -s $FS_PATH 2
	else
		bin/venus -s $FS_PATH 2
	fi
fi		

if [[ $1 == '-u' ]]
then
	fusermount -u $FS_PATH
	rm -rf $FS_PATH
fi
