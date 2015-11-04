ROOT_FS_PATH=~/afs-server-root

#check if afs_root dir exists
if [ ! -d $FS_PATH ]; then
	mkdir $ROOT_FS_PATH
	cp -r server_root_test/* $ROOT_FS_PATH
fi

make vice
bin/vice $ROOT_FS_PATH
