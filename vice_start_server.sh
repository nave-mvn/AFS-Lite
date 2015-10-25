ROOT_FS_PATH=~/afs-server-root

#check if afs_root dir exists
if [ -d $FS_PATH ]; then
	rm -rf $ROOT_FS_PATH
fi

mkdir $ROOT_FS_PATH
cp -r server_root_test/* $ROOT_FS_PATH

make vice
bin/vice $ROOT_FS_PATH
