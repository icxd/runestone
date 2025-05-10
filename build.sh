clang -g runestone.c main.c aarch64_macos_gas.c x86_64_linux_nasm.c
./a.out
as test.S -o test.o
ld -arch arm64 -lSystem -syslibroot $(xcrun --sdk macosx --show-sdk-path) -e _start -o test test.o
