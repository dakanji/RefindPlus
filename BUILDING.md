# BUILDING REFINDPLUS
<table>
 <tr>
   <td style="background-color: LightYellow;">
       <div>
       <h2>Table of Contents</h2>
       <ul>
           <li><a href="#remote-build-github">REMOTE BUILD (GITHUB)</a></li>
           <li><a href="#local-build-docker">LOCAL BUILD (DOCKER)</a></li>
           <li><a href="#local-build-mac-os">LOCAL BUILD (MAC OS)</a></li>
           <li><a href="#repository-sync">REPOSITORY SYNC</a></li>
       </ul>
       </div>
   </td>
 </tr>
</table>

## Remote Build (GitHub)

RefindPlus can be built by leveraging GitHub's `Workflow Artefact` creation and storage capabilities. \
A GitHub `Workflow Action` is included in this repository to facilitate this.
- Navigate to https://github.com/RefindPlusRepo/RefindPlus and fork the repository.
- Navigate to `https://github.com/YOUR_GITHUB_USERNAME_GOES_HERE/RefindPlus.git`.
  - Enable Github Actions
    - Click on the `Settings` navigation option
    - Select the `Actions` configuration option
    - Allow actions under the `Actions` tab
  - Trigger Github Action
    - Click on the `Actions` navigation option.
    - Select the `Build Artefacts` workflow option.
    - Trigger the workflow using the dropdown menu option.

Click the Workflow Action run instance when done, and look for `'Artifacts'` near the bottom of the page.

<details>
<summary><strong>Secure Boot Considerations</strong></summary>

The action will create a one-off private key and associated certificate by default. \
The created certificate is bundled with the generated artefacts for enrolment but the private key is not. \
This is because there is no way to securely transfer the private key (artefacts are publicly available).

Certificates are public keys and can therefore be shared along with the artefacts. \
Private keys cannot be disclosed however, and are therefore discarded after use. \
The implication is that a new certificate must be enrolled for each build.

This limitation can be avoided by providing the Workflow Action a certificate and private key instead. \
When these are unavailable, they will be generated on each run with the limitations described.

<details>
<summary><strong>Providing Certificate/Key</strong></summary>

Save a private key as a `SIG_RP_KEY` repository secret in your fork, as well as certificate data saved as a `SIG_RP_CRT` secret.

These repository secrets will be used each time the Workflow Action is run on your fork with the option to sign selected. \
Associated certificate files will always be bundled with artefacts generated but only need to be enrolled once.

> A `SIG_RP_CER` repository secret can be optionally stored for speed. \
> The `*.cer` data is derived from the `*.crt` data otherwise.

**See documentation** on how to store [**GITHUB REPOSITORY SECRETS**](https://docs.github.com/en/actions/how-tos/write-workflows/choose-what-workflows-do/use-secrets#creating-secrets-for-a-repository).

Note: Saving certificate data without private key data or saving the `*.key` data without `*.crt` data is invalid. \
Also, note that `SIG_RP_CRT`/`SIG_RP_CER` are public and could thus be stored as `repository variables` instead. \
However, for simplicity, the Workflow Action expects them set as repository secrets along with `SIG_RP_KEY`.

In summary, `SIG_RP_KEY` and `SIG_RP_CRT` repository secrets are needed as a minimum for this process. \
However, the automated signing option can be skipped and binaries manually signed later if preferred. \
Finally, the public certificates derived via the 'one-off' private keys may satisfy some use cases.

<details>
<summary><strong>Generate Public Certificate and Private Key</strong></summary>

At a Terminal prompt and with `OpenSSL` installed, create 10-Year validity `MySig.key/crt/cer` files by running:

```
$ openssl req -new -x509 -nodes -newkey rsa:2048 -keyout MySig.key -out MySig.crt -days 3650 -subj "/CN=MyKeyCert/"
$ openssl x509 -in MySig.crt -out MySig.cer -outform DER
```

Copy the three files to a secure location and adjust permissions to restrict read access to the `MySig.key` file. \
Preserve these files as they are needed to sign binaries in future with details consistent with what is signed now.

</details>

</details>

</details>


<br><br>

---

<br><br>

## Local Build (Docker)

RefindPlus can be built on any operating system environment that supports Docker virtualisation. \
A Docker image has been created by a third party developer and is available on the DockerHub website (https://hub.docker.com/r/xaionaro2/edk2-builder).

Please refer to that project's repository (https://github.com/xaionaro/edk2-builder-docker) for details and support on this option.

<br><br>

---

<br><br>

<!--
DA-TAG: High chance of need to recycle to flag as limited to Mac OS 26.x Tahoe/Older!
        Maybe Mac OS 27.x WhatEver/Older. Best case scenario (unless RUDK is patched)

> [!CAUTION]
>
> The process outlined below *HAS NOT* been verified on Mac OS 26.x Tahoe/Newer.
-->

## Local Build (Mac OS)

### Python

The build process requires Python 2 but Python was essentially removed from Mac OS in 12.x Monterey.\
If running this version of Mac OS or newer, download and install Python 2.7.18 from the Python website (https://www.python.org/downloads/release/python-2718).

> [!TIP]
>
> Python 2 is available by default on Mac OS 11.x Big Sur and older.

### Xcode

#### Base Installation

Download the version of Xcode for your Mac OS version from the Mac App Store and install.\
The third-party maintained XcodeReleases website (https://xcodereleases.com) provides convenient links to Xcode packages on Apple's servers.

#### Commandline Tools Installation

After installing Xcode, you will need to additionally install its commandline tools.\
To do this, at a Terminal prompt, enter:

```
$ xcode-select --install
```

### Additional Packages

#### Background

While Xcode provides a full development environment as well as a suite of different utilities, it does not provide all the tools required for TianoCore EDK II development as required to build RefindPlus on Mac OS natively.

These tools can be easily installed with package managers such as `MacPorts` or `HomeBrew`.

> [!NOTE]
>
> MacPorts supports Mac OS versions for a lot longer than HomeBrew. \
> However, HomeBrew typically tends to have more packages available.

If not already installed, follow installation instructions on the respective project websites:
- MacPorts: https://macports.org
- HomeBrew: https://brew.sh

#### Update Package Manager

Package Managers work best when up to date.

```
$ sudo port selfupdate && sudo port upgrade outdated
```
```
$ brew update && brew upgrade && brew cleanup
```

#### Install Build Assembler

The assembler used for TianoCore EDK II is the Netwide Assembler (NASM).

```
$ sudo port install nasm
```
```
$ brew install nasm
```

#### Install ACPI Compiler

ACPICA is required to compile code in ACPI Source Language for TianoCore EDK II firmware builds.

```
$ sudo port install acpica
```
```
$ brew install acpica
```

#### Install Image Converter

The ocmtoc utility converts the Mach-O image format generated on Mac OS to the PE/COFF format required by the UEFI specifications.

<details>
<summary><strong>Ocmtoc Notes</strong></summary>

Building RefindPlus on Mac OS specifically requires `ocmtoc v1.0.4/newer`. \
The `RefindPlusBuilder` script will use a copy bundled within `RefindPlusUDK` if missing.

This technically means installing ocmtoc **IS NOT** compulsory. \
It is preferable however, that such requirements are provided by the host system.

`ocmtoc` is only available as an HomeBrew package and only for Mac OS 11.x Big Sur and newer. \
However, pre-built `ocmtoc` files can be manually installed on Mac OS 10.9 Mavericks and newer.

Pre-built files can be found here: https://github.com/acidanthera/ocmtoc/releases. \
Save the file as `/usr/local/bin/mtoc` (Note `mtoc` name)

</details>

```
$ brew uninstall mtoc && brew install ocmtoc
```

### Prepare RefindPlus Environment

#### Fork the RefindPlus Repository

Navigate to `https://github.com/RefindPlusRepo/RefindPlus` and fork the repository.

#### Clone the Forked RefindPlus Repository

In Terminal, clone the forked `RefindPlus` repository into a `RefindPlus/Working` folder under your `Documents` directory as follows:

```
$ mkdir -p ~/Documents/RefindPlus && cd ~/Documents/RefindPlus
$ git clone https://github.com/YOUR_GITHUB_USERNAME_GOES_HERE/RefindPlus.git Working
$ cd ~/Documents/RefindPlus/Working && git checkout GOPFix
$ git remote add upstream https://github.com/RefindPlusRepo/RefindPlus.git
```

Your local RefindPlus repository will be under `Documents/RefindPlus/Working`.

### Prepare RefindPlusUDK Environment

#### Fork the RefindPlusUDK Repository

Navigate to `https://github.com/RefindPlusRepo/RefindPlusUDK` and fork the repository

#### Clone the Forked RefindPlusUDK Repository

In Terminal, clone the forked `RefindPlusUDK` repository into a `RefindPlus/edk2` folder under your `Documents` directory as follows:

```
$ mkdir -p ~/Documents/RefindPlus && cd ~/Documents/RefindPlus
$ git clone https://github.com/YOUR_GITHUB_USERNAME_GOES_HERE/RefindPlusUDK.git edk2
$ cd ~/Documents/RefindPlus/edk2 && git checkout rudk
$ git remote add upstream https://github.com/RefindPlusRepo/RefindPlusUDK.git
```

Your local RefindPlusUDK repository will be under `Documents/RefindPlus/edk2`.

### Run Build Script

- Navigate to your `/Documents/RefindPlus/edk2/000-BuildScript` folder in the Finder.
- Separately, open a new Terminal window.
  - Always use a new Terminal window when building.
- Type `chmod a+x` in Terminal, add a space, then drag the `RefindPlusBuilder.sh` file onto the Terminal window and press `Enter`.
  - This "chmod a+x" step is typically only required the first time the script file is ever run.
- Type `bash` in Terminal, add a space, then drag the `RefindPlusBuilder.sh` file to the Terminal window again and press `Enter`.
  - Enter a space followed by `--build-branch="BRANCH_NAME"` to the end of the line (build on `BRANCH_NAME`) before pressing `Enter`.
  - If nothing is entered, the script will build on the current checked out code.
  - Pass `--help` to the build script for guidance on script options

## Repository Sync

If some time has passed since your last build or since you initially created your repositories, you will need to ensure your repositories are aligned with the source repositories to incorporate updates added in the intervening period.

### OPTION 1: Scripted Sync (Recommended)

> [!NOTE]
>
> This option requires a previously prepared [RefindPlusUDK environment](https://github.com/RefindPlusRepo/RefindPlus/blob/GOPFix/BUILDING.md#prepare-udk2018-environment).

- Navigate to your `/Documents/RefindPlus/edk2/000-BuildScript` folder in the Finder.
- Separately, open a new Terminal window.
  - A new Terminal window is best for syncing.
- Type `chmod a+x`, add a space, then drag the `RepoUpdater.sh` file onto the Terminal window and press `Enter`.
  - This "chmod a+x" step is typically only required the first time the script file is ever run.
- Type `bash`, add a space, then drag the `RepoUpdater.sh` file to the Terminal window again and press `Enter`.

> [!TIP]
>
> If you get an error after running the script, try running it again as subsequent runs should realign things. \
> If the script still fails after a third attempt, try the manual sync steps outlined below instead.

### OPTION 2: Manual Sync

#### Sync RefindPlus Manually

```
$ cd ~/Documents/RefindPlus/Working && git checkout GOPFix
$ export REPO_URL="https://github.com/RefindPlusRepo/RefindPlus.git"
$ git remote get-url upstream 2>/dev/null | grep -q "${REPO_URL}" || (git remote add upstream "${REPO_URL}" 2>/dev/null || git remote set-url upstream "${REPO_URL}")
$ git reset --hard a2cc87f019c4de3a1237e2dc23f432c27cec5ec6 && git push origin HEAD -f
$ git pull --tags upstream GOPFix && git push
$ git push --tags origin -f && unset REPO_URL
```

#### Sync RefindPlusUDK Manually

```
$ cd ~/Documents/RefindPlus/edk2 && git checkout rudk
$ export REPO_URL="https://github.com/RefindPlusRepo/RefindPlusUDK.git"
$ git remote get-url upstream 2>/dev/null | grep -q "${REPO_URL}" || (git remote add upstream "${REPO_URL}" 2>/dev/null || git remote set-url upstream "${REPO_URL}")
$ git reset --hard a94082b4e5e42a1cfdcbab0516f9ecdbb596d201 && git push origin HEAD -f
$ git pull --tags upstream rudk && git push
$ git push --tags origin -f && unset REPO_URL
```

### OPTION 3: GitHub Sync

GitHub includes an interface for syncing forks.\
While, unlike Option 3, Options 1 and 2 will always leave your fork with a clean history consistent with the source repositories, some may find the GitHub interface convenient.
