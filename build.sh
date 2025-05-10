set -xe 

mkdir -p build
clang -shared -o build/librs.so lib/*.c

if [ "$1" = "test" ]; then
  clang simple.c -Lbuild -lrs -o simple
  ./simple
  as simple.S -o simple.o
  ld -arch arm64 -lSystem -syslibroot $(xcrun --sdk macosx --show-sdk-path) \
    -e _start -o simple simple.o
fi
