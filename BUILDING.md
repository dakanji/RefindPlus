# Building RefindPlus on Mac OS
These are step-by-step instructions for setting up a bespoke Tianocore EDK II build environment for buidling the RefindPlus boot manager on Mac OS.

These steps have been verified on Mac OS v10.12 (Sierra) and Mac OS v10.14 (Mojave) and should also work on Mac OS v10.13 (High Sierra) and Mac OS v10.15 (Catalina).


## Activate Mac OS Development Tools

### Xcode
Download [Xcode](https://developer.apple.com/xcode) from the Mac App Store and install.

**NB:** Tested on Xcode v9.4.1 and v11.3 but should also work on v10.x and v12.x

### Xcode Commandline Tools
After installing Xcode, you will additionally need to install commandline tools.
To do this, at a Terminal prompt, enter:

```
$ xcode-select --install
```

### HomeBrew

#### Installing HomeBrew

While Xcode provides a full development environment as well as a suite of different utilities, it does not provide all the tools required for Tianocore EDK II development.

These tools can be obtained in a number of ways, but the two most popular ways are by using HomeBrew or MacPorts.

This guide focuses on `HomeBrew` but equivalent steps can be taken in MacPorts.

If you do not have HomeBrew already installed, go to the [HomeBrew Home Page](https://brew.sh) and follow the instructions.

Installation involves copying and pasting a one-line code string into Terminal and pressing `Enter`.

#### Update the PATH Environment Variable

Tools installed using HomeBrew are placed in `/usr/local/bin` which is non-standard to avoid conflicts with pre-installed tools.

The `PATH` environment variable must therefore be updated so newly installed tools are found.

```
$ export PATH=/usr/local/bin:$PATH
```

### Install the MTOC Utility with HomeBrew

The mtoc utility is required to convert the Mac OS Mach-O image format to the PE/COFF format required by the UEFI specifications.

```
$ brew install mtoc && brew upgrade mtoc
```

### Install the Netwide Assembler (NASM) with HomeBrew

The assembler used for Tianocore EDK II builds is the Netwide Assembler (NASM).

```
$ brew install nasm && brew upgrade nasm
```

### Install the ACPI Compiler with HomeBrew

The ASL compiler is required to build code in ACPI Source Language for Tianocore EDK II firmware builds.

```
$ brew install acpica && brew upgrade acpica
```

### Install the QEMU Emulator with HomeBrew (Optional)

The QEMU emulator from http://www.qemu.org/ is required to enable UEFI support on virtual machines.

```
$ brew install qemu && brew upgrade qemu
```

## Prepare the RefindPlus Environment
### Fork the RefindPlus Repository

Navigate to `https://github.com/dakanji/RefindPlus` and fork the repository

### Clone the Forked RefindPlus Repository

In Terminal, create a `RefindPlus` folder under your `Documents` folder, `cd` to that directory and clone the forked `RefindPlus` repository into a `Working` folder under the `RefindPlus` folder:

```
$ mkdir -p ~/Documents/RefindPlus
$ cd ~/Documents/RefindPlus
$ git clone https://github.com/YOUR_GITHUB_USERNAME/RefindPlus.git Working
$ cd ~/Documents/RefindPlus/Working
$ git checkout GOPFix
$ git remote add upstream https://github.com/dakanji/RefindPlus.git
```

**NB:** Replace `YOUR_GITHUB_USERNAME` above with your actual GitHub User Name.

Your local `RefindPlus` repository will be under `Documents/RefindPlus/Working`


## Prepare the Bespoke TianoCore EDK II (UDK2018) Environment
### Fork the RefindPlusUDK Repository

Navigate to `https://github.com/dakanji/RefindPlusUDK` and fork the repository

### Clone the Forked RefindPlusUDK Repository
In Terminal, clone the "RefindPlus Ready" TianoCore EDK II (UDK2018) repository into an `edk2` folder under your `RefindPlus` folder:

```
$ cd ~/Documents/RefindPlus
$ git clone https://github.com/YOUR_GITHUB_USERNAME/RefindPlusUDK.git edk2
$ cd ~/Documents/RefindPlus/edk2
$ git checkout rudk
$ git remote add upstream https://github.com/dakanji/RefindPlusUDK.git
```

**NB:** Replace `YOUR_GITHUB_USERNAME` above with your actual GitHub User Name.

Your local `RefindPlusUDK` repository will be under `Documents/RefindPlus/edk2`

## Build RefindPlus
- Navigate to your `/Documents/RefindPlus/edk2/000-BuildScript` folder in Finder
- Drag the `RefindPlusBuilder.sh` file into Terminal
  - Enter a space and `MyEdits`, or any other branch name, to the end of the line if you want to build on that branch
  - If nothing is entered, the script will build on the default `GOPFix` branch
- Press `Enter`


## Syncing Your Repositories with Upstream Repositories
### OPTION 1: Scripted Sync (Recommended)
- Navigate to your `/Documents/RefindPlus/edk2/000-BuildScript` folder in Finder
- Drag the `RepoUpdater.sh` file into Terminal
- Press `Enter`

**NB:** If you get an error after running the script, try running it again as subsequent runs should realign things.
If the script still fails after a third attempt, try the manual sync steps outlined below instead.

### OPTION 2: Manual Sync
#### RefindPlus
In Terminal, run the following commands:

```
$ cd ~/Documents/RefindPlus/Working
$ git checkout GOPFix
$ git reset --hard a2cc87f019c4de3a1237e2dc23f432c27cec5ec6
$ git push origin HEAD -f
$ git pull upstream GOPFix
$ git push
```

#### RefindPlusUDK
In Terminal, run the following commands:

```
$ cd ~/Documents/RefindPlus/edk2
$ git checkout rudk
$ git reset --hard a94082b4e5e42a1cfdcbab0516f9ecdbb596d201
$ git push origin HEAD -f
$ git pull upstream rudk
$ git push
```
