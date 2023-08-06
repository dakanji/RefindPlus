# Building RefindPlus
## Building with Docker
RefindPlus can be built on environments that support Docker. A Docker image has been created by a third party developer and is available on [DockerHub](https://hub.docker.com/r/xaionaro2/edk2-builder). Please refer to https://github.com/xaionaro/edk2-builder-docker for details and support on this option.

NB: RefindPlus instances built using this Docker image may not be consistent with builds done by following the recommended steps outlined under `Building on macOS` below.

## Building on macOS
These are step-by-step instructions for setting up a bespoke TianoCore EDK II build environment and building the RefindPlus boot manager on macOS.

### Activate macOS Development Tools

#### Xcode
Download [Xcode](https://developer.apple.com/xcode) from the Mac App Store and install.

#### Xcode Commandline Tools
After installing Xcode, you will additionally need to install commandline tools.
To do this, at a Terminal prompt, enter:

```
$ xcode-select --install
```

#### HomeBrew

##### Installing HomeBrew

While Xcode provides a full development environment as well as a suite of different utilities, it does not provide all the tools required for TianoCore EDK II development.

This guide focuses on using HomeBrew to provide the required tools but equivalent steps can be taken in MacPorts and Fink which, unlike HomeBrew, maintain support for older versions of macOS. Just substitute in the equivalent commands as required.

##### Update the PATH Environment Variable for HomeBrew

Tools installed using HomeBrew are placed in `/usr/local/bin`. This non-standard location avoids conflicts with pre-installed tools but the `PATH` environment variable must be updated so newly installed tools are found.

```
$ export PATH=/usr/local/bin:$PATH
```

#### Install the MTOC or OCMTOC Utilities with HomeBrew

The mtoc utility is required to convert the macOS Mach-O image format to the PE/COFF format required by the UEFI specifications.

```
$ brew install mtoc && brew upgrade mtoc
```

or for macOS 11.x Big Sur and newer...

```
$ brew install ocmtoc && brew upgrade ocmtoc
```

#### Install the Netwide Assembler (NASM) with HomeBrew

The assembler used for TianoCore EDK II builds is the Netwide Assembler (NASM).

```
$ brew install nasm && brew upgrade nasm
```

#### Install the ACPI Compiler with HomeBrew

The ASL compiler is required to build code in ACPI Source Language for TianoCore EDK II firmware builds.

```
$ brew install acpica && brew upgrade acpica
```

#### Install the QEMU Emulator with HomeBrew (Optional)

The QEMU emulator from http://www.qemu.org/ is required to enable UEFI support on virtual machines.

```
$ brew install qemu && brew upgrade qemu
```

### Prepare the RefindPlus Environment
#### Fork the RefindPlus Repository

Navigate to `https://github.com/dakanji/RefindPlus` and fork the repository

#### Clone the Forked RefindPlus Repository

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


### Prepare the Bespoke TianoCore EDK II (UDK2018) Environment
#### Fork the RefindPlusUDK Repository

Navigate to `https://github.com/dakanji/RefindPlusUDK` and fork the repository

#### Clone the Forked RefindPlusUDK Repository
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

### Build RefindPlus
- Navigate to your `/Documents/RefindPlus/edk2/000-BuildScript` folder in Finder
- Open a new Terminal window
- Drag the `RefindPlusBuilder.sh` file onto the Terminal window
  - Enter a space and `MyEdits`, or any other branch name, to the end of the line if you want to build on that branch
  - If nothing is entered, the script will build on the default `GOPFix` branch
- Press `Enter`

### Syncing Your Repositories with Source Repositories
If a period of time has passed since your last build or since you initially created your repositories, you will need to ensure your repositories are aligned with the source repositories in order to incorporate changes made in the intervening period.

#### OPTION 1: Scripted Sync (Recommended)
- Navigate to your `/Documents/RefindPlus/edk2/000-BuildScript` folder in Finder
- Open a new Terminal window
- Drag the `RepoUpdater.sh` file onto the Terminal window
- Press `Enter`

**NB:** If you get an error after running the script, try running it again as subsequent runs should realign things.
If the script still fails after a third attempt, try the manual sync steps outlined below instead.

#### OPTION 2: Manual Sync
##### RefindPlus
In Terminal, run the following commands:

```
$ cd ~/Documents/RefindPlus/Working
$ git checkout GOPFix
$ git reset --hard a2cc87f019c4de3a1237e2dc23f432c27cec5ec6
$ git push origin HEAD -f
$ git pull upstream GOPFix
$ git push
```

##### RefindPlusUDK
In Terminal, run the following commands:

```
$ cd ~/Documents/RefindPlus/edk2
$ git checkout rudk
$ git reset --hard a94082b4e5e42a1cfdcbab0516f9ecdbb596d201
$ git push origin HEAD -f
$ git pull upstream rudk
$ git push
```

#### OPTION 3: GitHub Sync
GitHub now includes an interface for syncing forks. While Options 1 and 2 will leave the fork with a clean history consistent with the source repository, some may find the GitHub interface easier to use.
