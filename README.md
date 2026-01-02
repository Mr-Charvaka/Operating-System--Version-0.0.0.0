<div align="center">

# 👾 Retro Pixel OS  
### *A Hobby Operating System with a Pixel-Art Soul*

![Retro Pixel OS](https://img.shields.io/badge/OS-Retro%20Pixel-blueviolet?style=for-the-badge)
![Version](https://img.shields.io/badge/version-0.0.0.0-orange?style=for-the-badge)
![Architecture](https://img.shields.io/badge/arch-x86-lightgrey?style=for-the-badge)
![License](https://img.shields.io/badge/license-MIT-green?style=for-the-badge)
![Made With](https://img.shields.io/badge/made%20with-C%20%7C%20ASM%20%7C%20C++-red?style=for-the-badge)

🚀 **A retro-inspired operating system built from scratch**  
🖥️ **Pixel UI • Custom Kernel • Terminal • File Tools**  
🧠 **Perfect for OS dev learners & low-level hackers**

</div>

---

## 🧠 What is Retro Pixel OS?

**Retro Pixel OS** is a **from-scratch hobby operating system** designed to:
- Teach **low-level OS development**
- Recreate the **feel of early computing**
- Combine **pixel-art UI** with a **functional terminal**
- Run inside an emulator (QEMU)

---
⚠️ *Not meant for production use. This is a learning OS. But Who knows the Future...*
---
⚠️*This is not a linux Distro and I haven't used any C++ or C library, every Library for RetroPixelOS is solely writtern and hard-coded*
---

## ✨ Features

- Custom bootloader (writtern in Assembly)
- Kernel (written in C/C++)
- Pixel-based graphical interface
- Built-in terminal
- File manager (`fm`)
- Calculator (`calc`)
- Disk info (`df`)
- Runs on QEMU (x86)

---

## 🧰 Tech Stack

| Layer | Technology |
|-----|-----------|
| Bootloader | NASM |
| Kernel | C / C++ |
| Emulator | QEMU |
| Build Tools | GCC |
| Platform | Windows (WSL) |

---

## 🚀 Getting Started

### Step 1: Install WSL
```powershell
wsl --install
```

Restart your system and open **Ubuntu**.

---

### Step 2: Install Dependencies
```bash
sudo apt update && sudo apt install build-essential g++-multilib nasm qemu-system-x86 mtools python3 git -y
```

---

### Step 3: Clone Repository
```bash
git clone https://github.com/Mr-charvaka/Operating-System--Version-0.0.0.0.git
cd Operating-System--Version-0.0.0.0
```

---

### Step 4: Build
```bash
chmod +x build.sh
./build.sh
```

---

### Step 5: Run
```bash
qemu-system-i386 -drive format=raw,file=os.img -serial stdio -m 512
```

---

## 🎮 Terminal Commands

```bash
ls
calc 5 + 5
df
fm
```

---

## 🛣 Roadmap

- Keyboard driver
- Memory management
- Scheduler
- GUI improvements
- Filesystem support

---

## 📜 License

MIT License

---

⭐ **Star the repo if you like it — Happy Hacking!** 👾
