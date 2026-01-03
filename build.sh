#!/bin/bash
set -e

echo "Building inside WSL..."

# Clean previous build
find src -name "*.o" -type f -delete
find apps -name "*.o" -type f -delete
rm -f os.img

# Compile Apps
echo "Compiling apps (C++)..."

# Function to build an app
build_app() {
    local name=$1
    echo "  Building apps/$name.cpp..."
    g++ -m32 -ffreestanding -fno-rtti -fno-exceptions -I apps/ -I src/include -c apps/$name.cpp -o apps/$name.o
    ld -m elf_i386 -T apps/linker.ld -o apps/$name.elf apps/$name.o
}

build_app "hello"
build_app "init"
build_app "calc"
build_app "df"
build_app "fm"
build_app "demo_ipc"
g++ -m32 -ffreestanding -fno-rtti -fno-exceptions -I apps/ -I src/include -c apps/posix_impl.cpp -o apps/posix_impl.o
echo "  Building apps/posix_test.cpp..."
g++ -m32 -ffreestanding -fno-rtti -fno-exceptions -I apps/ -I src/include -c apps/posix_test.cpp -o apps/posix_test.o
ld -m elf_i386 -T apps/linker.ld -o apps/posix_test.elf apps/posix_test.o apps/posix_impl.o

echo "  Building apps/posix_suite.cpp..."
g++ -m32 -ffreestanding -fno-rtti -fno-exceptions -I apps/ -I src/include -c apps/posix_suite.cpp -o apps/posix_suite.o
ld -m elf_i386 -T apps/linker.ld -o apps/posix_suite.elf apps/posix_suite.o apps/posix_impl.o


# Compile Bootloader
echo "Compiling boot.asm..."
nasm src/boot/boot.asm -f bin -o src/boot/boot.bin -I src/boot/

# Compile Kernel Entry
echo "Compiling kernel_entry.asm..."
nasm src/boot/kernel_entry.asm -f elf32 -o src/boot/kernel_entry.o

echo "Compiling interrupt.asm..."
nasm src/kernel/interrupt.asm -f elf32 -o src/kernel/interrupt.o

echo "Compiling process_asm.asm..."
nasm src/kernel/process_asm.asm -f elf32 -o src/kernel/process_asm.o

echo "Compiling gdt_asm.asm..."
nasm src/kernel/gdt_asm.asm -f elf32 -o src/kernel/gdt_asm.o

# Compile C Sources
# Compile OS Sources (All C++)
echo "Compiling Sources..."
find src -name "*.cpp" | while read -r file; do
    echo "  Compiling $file..."
    outfile="${file%.cpp}.o"
    g++ -ffreestanding -m32 -fno-pie -fstack-protector-strong -Os -fno-rtti -fno-exceptions -std=c++20 -g -I src/include -c "$file" -o "$outfile"
done

# Link
echo "Linking..."
# Generate list of object files
OBJ_FILES=$(find src -name "*.o" ! -name "kernel_entry.o")
ld -m elf_i386 -o src/kernel/kernel.bin -T linker.ld src/boot/kernel_entry.o $OBJ_FILES --oformat binary

# Create OS Image
echo "Creating os.img..."
cat src/boot/boot.bin src/kernel/kernel.bin > os.img
# Pad to 32MB for FAT16
truncate -s 32M os.img

# Inject Wallpaper
echo "Injecting Wallpaper..."
python3 inject_wallpaper.py

echo "Build Successful: os.img"
