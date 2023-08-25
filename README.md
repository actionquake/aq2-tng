# AQ2-TNG

## Action Quake 2: The Next Generation

This is the primary repository for AQtion's Quake II library for Action Quake II.  It is a fork of the https://github.com/aq2-tng/aq2-tng/ code.

* Primary branch:  `aqtion`
* Alpha branch:  `aqtion-alpha`
* Downstream sync branch:  `bots`

## Releases

The [Releases](https://github.com/actionquake/aq2-tng/releases) page lists all of the releases handled by Github Actions.  The latest release is not always from the Primary branch.  If you need help, ask in the [forums](https://forums.aq2world.com) or in [Discord](https://discord.aq2world.com).

### Release artifacts
The following table will provide you with the correct artifact to download if you are planning on hosting a non-Docker image server, or hosting a listen server with your client.  This download is not needed for clients who just want to play, or if you are playing using AQtion on Steam.

| Operating System | Architecture | Artifact | Extension |
| ---- | ---- | ---- | ---- |
| Mac OS X | Intel (x86_64) | tng-darwin-x86_64.zip | .dylib
| Mac OS X | M1/M2 (ARM) | tng-darwin-arm64.zip | .dylib
| Windows 32-bit | x86 | tng-win-32.zip | .dll
| Windows 64-bit | x86_64 |  tng-win-64.zip | .dll
| Linux 64-bit (recent glibc) | x86_64 | tng-lin-x86_64.zip  | .so
| Linux 64-bit (old glibc) | x86_64 | tng-lin-x86_64-compat.zip  | .so
| Linux 64-bit | ARM | tng-lin-arm64.zip  | .so

Regarding glibc, essentially a 'recent glibc' would be built on whatever version the latest Ubuntu (currently 22.04) uses.  'Old glibc' would be **2.19** and is compatible with Ubuntu 16.04 and CentOS 7 and higher, for example.  The automated builds do not support any older versions, but you have the freedom to build the code yourself to fit your systems needs.