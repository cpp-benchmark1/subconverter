# Windows Build Setup Guide

This document provides step-by-step instructions to set up and build the project on a Windows system using `vcpkg`, MSYS2, and `cmake` compatibility adjustments.

---

## 1. Clone and Bootstrap vcpkg

From outside the project directory:

```bash
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
```

---

## 2. Install Visual Studio with C++ Support

Install **Visual Studio Installer** and select the **"Desktop Development with C++"** workload.

---

## 3. Set up Environment Variables for vcpkg

Add the path to your `vcpkg` installation directory to your environment variables. Example:

```
C:\Users\user\vcpkg
```

To make it permanent in the bash shell:

```bash
echo 'export PATH=$PATH:/c/Users/user/vcpkg' >> ~/.bashrc
source ~/.bashrc
```

---

## 4. Install Dependencies via vcpkg

From the project directory, run:

```bash
vcpkg.exe install pkgconf curl:x64-windows
```

---

## 5. Install MSYS2 and Required Packages

1. **Download and install MSYS2** from [https://www.msys2.org](https://www.msys2.org)

2. Open **MSYS2 MinGW 64-bit** and run the following:

```bash
pacman -Syu
pacman -S --needed \
  mingw-w64-x86_64-toolchain \
  mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-make \
  git
pacman -S --needed patch
pacman -S mingw-w64-x86_64-python-pip
pacman -S --needed mingw-w64-x86_64-python-pip mingw-w64-x86_64-python-gitpython
pacman -S --needed mingw-w64-x86_64-pcre2
```

---

## 6. Verify Installations

Check that required tools are correctly installed:

```bash
gcc --version
cmake --version
make --version
git --version
patch --version
pip --version
```

---

## 7. Set PIP Compatibility

To avoid pip restrictions:

```bash
export PIP_BREAK_SYSTEM_PACKAGES=1
```

Verify with:

```bash
PIP_BREAK_SYSTEM_PACKAGES=1 python -c "import pip, sys; print('pip ok, site-packages:', pip.__path__[0]); sys.exit(0)"
```

---

## 8. Install Python 3.x on Windows

Install Python 3.x from [https://www.python.org/downloads/windows/](https://www.python.org/downloads/windows/) and ensure it’s available at:

```
C:\Users\user\AppData\Local\Programs\Python\Python313
```

Then:

```bash
export PATH="/c/Users/user/AppData/Local/Programs/Python/Python313:$PATH"
```

---

## 9. Configure `cmake` Compatibility Wrapper

1. Create a wrapper directory:

```bash
mkdir -p "$HOME/cmake_wrap"
```

2. Create the wrapper script:

```bash
cat > "$HOME/cmake_wrap/cmake" << 'EOF'
#!/usr/bin/env bash
exec /mingw64/bin/cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 "$@"
EOF
```

3. Make it executable:

```bash
chmod +x "$HOME/cmake_wrap/cmake"
```

4. Add it to the beginning of your PATH:

```bash
export PATH="$HOME/cmake_wrap:$PATH"
```

5. Clear command cache:

```bash
hash -r
```

6. Verify wrapper is active:

```bash
which cmake
```

---

## 10. Persist Configuration in `.bashrc`

Append the following to `~/.bashrc`:

```bash
export PIP_BREAK_SYSTEM_PACKAGES=1
export PATH="/c/Users/user/AppData/Local/Programs/Python/Python313:$PATH"
export PATH="$HOME/cmake_wrap:$PATH"
cmake() { command cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 "$@" ; }
```

Then apply changes:

```bash
source ~/.bashrc
```

---

## 11. Build the Project

Navigate to the project directory, clean and build:

```bash
./scripts/build.windows.release.sh
```

