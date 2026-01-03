import struct
import os

# Disk Geometry (Must match boot.asm)
SECTOR_SIZE = 512
RESERVED_SECTORS = 4096
NUM_FATS = 2
SECTORS_PER_FAT = 256
ROOT_ENTRIES = 512
ROOT_DIR_SECTORS = (ROOT_ENTRIES * 32) // SECTOR_SIZE

# Offsets
FAT_OFFSET = RESERVED_SECTORS * SECTOR_SIZE
ROOT_OFFSET = (RESERVED_SECTORS + NUM_FATS * SECTORS_PER_FAT) * SECTOR_SIZE
DATA_OFFSET = ROOT_OFFSET + (ROOT_DIR_SECTORS * SECTOR_SIZE)

IMG_FILE = "os.img"

def inject_file(f, filename_83, source_path, start_cluster):
    if not os.path.exists(source_path):
        print(f"Error: {source_path} not found.")
        return 0, 0
        
    with open(source_path, "rb") as src:
        data = src.read()
        
    size = len(data)
    sectors = (size + SECTOR_SIZE - 1) // SECTOR_SIZE
    
    # 1. Write Data
    f.seek(DATA_OFFSET + (start_cluster - 2) * SECTOR_SIZE)
    f.write(data)
    
    # 2. Update FAT Table
    f.seek(FAT_OFFSET + start_cluster * 2) 
    cluster = start_cluster
    for i in range(sectors - 1):
        f.write(struct.pack('<H', cluster + 1))
        cluster += 1
    f.write(struct.pack('<H', 0xFFFF))
    
    # 3. Create Directory Entry
    # For simplicity, append to root dir (find first empty)
    f.seek(ROOT_OFFSET)
    while True:
        entry_data = f.read(32)
        if not entry_data or entry_data[0] == 0:
            f.seek(-32, 1) if entry_data else f.seek(0, 2)
            break
            
    name, ext = filename_83.split('.')
    name = name.ljust(8)
    ext = ext.ljust(3)
    full_name = f"{name}{ext}".encode('ascii')
    
    entry = struct.pack('<11sBBBHHHHHHHL', 
        full_name, 0x20, 0, 0, 0, 0, 0, 0, 0, 0, start_cluster, size)
    f.write(entry)
    
    print(f"Injected {filename_83} ({size} bytes, {sectors} sectors) at cluster {start_cluster}.")
    return sectors, cluster + 1

def inject():
    if not os.path.exists(IMG_FILE):
        print(f"Error: {IMG_FILE} not found.")
        return
        
    with open(IMG_FILE, "r+b") as f:
        # Clear Root Dir and FAT (Optional, but safer for re-builds)
        f.seek(ROOT_OFFSET)
        f.write(b'\x00' * (ROOT_DIR_SECTORS * SECTOR_SIZE))
        f.seek(FAT_OFFSET)
        # Initialize FAT with media descriptor and root directory cluster
        f.write(b'\xF8\xFF\xFF\xFF') # FAT start (standard for FAT16/Hard Disk)
        # Clear the rest of the FAT sectors
        for _ in range(NUM_FATS):
            f.write(b'\x00' * (SECTORS_PER_FAT * SECTOR_SIZE - 4))
            if _ == 0: # Also clear second FAT
                 f.seek(FAT_OFFSET + SECTORS_PER_FAT * SECTOR_SIZE)
        
        current_cluster = 2
        
        # Inject Wallpaper
        sec, next_clust = inject_file(f, "WALL.BMP", "assets/wallpaper.bmp", current_cluster)
        current_cluster = next_clust
        
        # Inject Hello ELF
        if os.path.exists("apps/hello.elf"):
            sec, next_clust = inject_file(f, "HELLO.ELF", "apps/hello.elf", current_cluster)
            current_cluster = next_clust
            
        # Inject Init ELF
        if os.path.exists("apps/init.elf"):
            sec, next_clust = inject_file(f, "INIT.ELF", "apps/init.elf", current_cluster)
            current_cluster = next_clust

        # Inject Calc ELF
        if os.path.exists("apps/calc.elf"):
            sec, next_clust = inject_file(f, "CALC.ELF", "apps/calc.elf", current_cluster)
            current_cluster = next_clust

        # Inject DF ELF
        if os.path.exists("apps/df.elf"):
            sec, next_clust = inject_file(f, "DF.ELF", "apps/df.elf", current_cluster)
            current_cluster = next_clust

        # Inject FM ELF
        if os.path.exists("apps/fm.elf"):
            sec, next_clust = inject_file(f, "FM.ELF", "apps/fm.elf", current_cluster)
            current_cluster = next_clust

        # Inject Demo IPC ELF
        if os.path.exists("apps/demo_ipc.elf"):
            sec, next_clust = inject_file(f, "DEMO_IPC.ELF", "apps/demo_ipc.elf", current_cluster)
            current_cluster = next_clust

        # Inject POSIX Test ELF
        if os.path.exists("apps/posix_test.elf"):
            sec, next_clust = inject_file(f, "POSIX_T.ELF", "apps/posix_test.elf", current_cluster)
            current_cluster = next_clust

        # Inject POSIX Suite ELF
        if os.path.exists("apps/posix_suite.elf"):
            sec, next_clust = inject_file(f, "POSIX_SU.ELF", "apps/posix_suite.elf", current_cluster)
            current_cluster = next_clust

if __name__ == "__main__":
    inject()
