# delete all generated proto files
rm -rf proto/*.pb.*
# generate proto files again
make SimpleInterface.grpc.pb.cc SimpleInterface.pb.cc

#gcc -Wall src/vice.c `pkg-config fuse --cflags --libs` -o bin/vice
#gcc -Wall src/venus.c `pkg-config fuse --cflags --libs` -o bin/venus
