# OPL JUMP — SMART FILTER

A community modification of Open PS2 Loader focused on improving the organization of the **APPS** section. It intelligently separates regular applications from PlayStation 1 game shortcuts that use **POPStarter**, while preserving OPL's familiar interface.

## Quick Start

### JUMP — PlayStation 1 Shortcuts
- Press **X** to launch the selected shortcut.
- Press **Triangle** to refresh the list.
- Press **START** to return.

### Smart APPS Filter
- Press **L3** in APPS to switch between regular applications and recognized PS1 shortcuts.
- **L3 off:** Shows regular applications and hides recognized PS1 shortcuts.
- **L3 on:** Shows only recognized PS1 shortcuts.

### Shortcut Recognition
Shortcuts are recognized by the `XX.` filename prefix, the `SB.` prefix, or a PS1 executable ID in the filename, such as `SLUS_123.45.GAME NAME.ELF`. Generic `.ELF` files are not automatically recognized as PS1 shortcuts.

## Features

- **JUMP page:** Organizes PlayStation 1 game shortcuts by source:
  - **BDM:** Devices managed through BDM, including USB and MX4SIO.
  - **HDD:** The PS2 internal hard drive.
  - **NETWORK:** PS1 shortcuts available through SMB from a shared PC folder.
- **Smart APPS filter:** Press **L3** to switch between regular applications and recognized PS1 shortcuts.
  - **L3 off:** Shows regular applications and hides recognized PS1 shortcuts.
  - **L3 on:** Shows only recognized PS1 shortcuts.
- **Three recognition criteria:** Both JUMP and the Smart APPS filter recognize PS1 shortcuts using:
  1. The `XX.` filename prefix.
  2. The `SB.` filename prefix.
  3. A PS1 executable ID in the filename, for example `SLUS_123.45.GAME NAME.ELF`.
- **Important:** A generic `.ELF` file is not automatically classified as a PS1 shortcut; it must match one of the criteria above.
- **Familiar interface:** Retains the usual Open PS2 Loader look and feel.
- **Multiple storage options:** Organizes shortcuts from different storage devices and network sources.

## Tested Compatibility

During OPL JUMP testing, the following configurations were checked:

- **USB:** Including two USB devices connected simultaneously.
- **Internal HDD:** Using the traditional PS2 HDD format.
- **SMB1:** Including tests with PS2 Server sharing a folder from a PC over the network.
- **MX4SIO:** Tested for handling game shortcuts.

These results apply to the devices and configurations available during development. They do not imply that every possible setup has been tested.

## Installation

1. Download `OPNPS2LD.ELF` from this repository's **Releases** section.
2. Back up your current OPL executable.
3. Place the downloaded file in the device or location from which you normally launch OPL, according to your setup.
4. Start OPL and test the JUMP page and the APPS filter.

**Important:** Keep a backup of your current executable and configuration before replacing any files.

## Credits and License

OPL JUMP is a community modification based on [Open PS2 Loader](https://github.com/ps2homebrew/Open-PS2-Loader).

The original project credits **Ifcaro and jimmikaelkael** and uses the **Academic Free License 3.0 (AFL-3.0)**. The `LICENSE` file included with the original codebase must be preserved.

This modification focuses on organizing and handling PS1 shortcuts while retaining OPL's familiar interface.

All credit due to the original project and its developers must be retained. **OPL JUMP is not an official Open PS2 Loader release.**

## Disclaimer

This project is shared for community use. Functionality may vary depending on the console, connected devices, and individual configuration.

For the Spanish documentation, see [README_ES.md](README_ES.md).


## Community Feedback: Official OPL Integration

Would you like to see the **JUMP page and Smart APPS Filter** integrated into the official Open PS2 Loader project?

OPL JUMP was created as a community modification to explore these features while preserving OPL's familiar interface. Before considering a formal proposal to the official OPL development team, I would like to hear the community's opinion.

**Would you support submitting a proposal to integrate these features into official OPL?**

Your feedback, suggestions, and ideas are welcome. Community interest will help determine whether it is worth preparing a formal proposal for consideration by the upstream developers.

*This is a community consultation only. No official integration has been proposed, approved, or announced at this stage.*

## Development Acknowledgments

**OPL JUMP was developed through a collaborative process involving human direction and AI-assisted development.**

Artificial intelligence was used as a tool to support code analysis, troubleshooting, and implementation. However, turning the idea into a working modification required weeks of hands-on development, repeated testing, debugging, and careful validation on real PlayStation 2 hardware — often late into the night.

The project reflects the time, persistence, and practical testing invested in refining the JUMP page and the smart APPS filter, while preserving OPL's familiar interface and existing functionality.

**Project direction, integration decisions, hardware testing, and final validation were led by the project author.** AI assistance contributed to the development process, but the working result was achieved through continued human oversight and real-world testing.

This acknowledgment recognizes the role of AI as a development aid while giving credit to the hands-on work and dedication behind OPL JUMP.
