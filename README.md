# GBA Net Boot
*A tool for testing GBA homebrew on real hardware using the 3DS's built-in GBA hardware*

GBA Net Boot is a tool for wirelessly receiving and booting GBA homebrew on your 3DS using its built-in GBA hardware. Using [GBA Net Send](https://github.com/joshbackstein/gba-net-send), you can send GBA homebrew ROMs from your computer to your 3DS over a wifi network. GBA Net Boot will receive your ROM, store it to the SD card, and boot into open_agb_firm for you to run your ROM in native GBA mode on your 3DS. This is meant to make testing GBA homebrew on official hardware as seamless and painless as possible.

## Disclaimer
This software has been tested many times without any issues. The software it depends on has also been tested many times. However, there is always a risk something might cause damage to your system or cause corruption or loss of data. Multiple parts of this process deal with lower levels of the system, especially the custom firmware and bootloader. I am not responsible for any damage to or destruction of your system, your data, or anything else.

## How to use
First, make sure you have gone through the setup.

Make sure you have wifi enabled and connected on your 3DS, then launch gba-net-boot. If wifi hasn't been set up yet, gba-net-boot will wait for your 3DS to connect to wifi.

After your 3DS is connected to wifi, gba-net-boot will wait for you to use the gba-net-send tool to send your ROM to your 3DS over the network. Your ROM will be saved to the root of your SD card as `rom.gba`.

When the transfer has completed and your ROM is saved to your SD card, gba-net-boot will automatically boot into open_agb_firm. From here, you can navigate to `rom.gba` on the root of your SD card and press `A` to launch it.

## Setup
First, install the [Luma3DS](https://github.com/LumaTeam/Luma3DS/) custom firmware using this [guide](https://3ds.hacks.guide/). The guide should help you install both Luma3DS and [Boot9strap](https://github.com/SciresM/boot9strap/).

Next, install [fastboot3DS](https://github.com/derrekr/fastboot3DS#quick-start-guide) using [OpenFirmInstaller](https://github.com/d0k3/OpenFirmInstaller).

After you have installed fastboot3DS, turn off your 3DS, then turn it back on while holding the `Home` button. This should show you the fastboot3DS splash screen and bring you to its menu. Looking at the bottom screen, go down to `Boot setup` and press `A`, then go down to `Enable FCRAM Boot` and press `A` to enable it. If done correctly, you should see a check mark next to that option. Now press `B`, then go up to `Continue boot` and press `A`.

Optional: By default, fastboot3DS assigns the `Start` button on boot to load [GodMode9](https://github.com/d0k3/GodMode9). If you want to disable this and use the default Luma3DS chainloader by holding `Start` on boot, go to `Boot setup` in the fastboot3DS boot menu (hold `Home` on boot to get there again), then go to `Setup [slot 1]`, then highlight `Disable [slot 2]` and press `A`. Then keep pressing `B` until you are back at the main menu and choose `Continue boot` again. Holding `Start` on boot should now allow you to use the Luma3DS chainloader.

Now that you have Luma3DS and fastboot3DS installed, let's put gba-net-boot on your SD card. In the `3ds` directory on your SD card, create a new directory called `gba-net-boot` and copy `gba-net-boot.3dsx` into it.

Before gba-net-boot will work, you also need to copy [open_agb_firm](https://github.com/profi200/open_agb_firm) to your SD card. For gba-net-boot to work properly, you need to copy the `open_agb_firm.firm` file to one of two directories on your SD card:
* `/3ds/gba-net-boot`
* `/luma/payloads`

When gba-net-boot looks for open_agb_firm, it first checks its current directory (`/3ds/gba-net-boot` if you followed this guide), then it checks the rest of the list in the order it is presented here. Without `open_agb_firm.firm` in one of those directories, gba-net-boot cannot reboot into open_agb_firm to allow you to test your ROM. If you want to use the Luma3DS chainloader to load open_agb_firm, use the `/luma/payloads` directory. If you want to set up a slot in fastboot3DS, it doesn't matter which one you choose. If you want gba-net-boot to use a different open_agb_firm from what Luma3DS or fastboot3DS use, you can copy one version of open_agb_firm into gba-net-boot's directory for gba-net-boot to use and another version in the appropriate directory for Luma3DS or fastboot3DS to use.

Finally, make sure your 3DS has wifi enabled and is connected to the same network as the computer you will be sending your ROM from.

To launch gba-net-boot, go to the 3DS Home Menu and launch the Homebrew Launcher, then find gba-net-boot and launch it. Refer to the other section for instructions on how to use it.

### Booting directly into the Homebrew Launcher
For extra convenience, as long as you're using Luma3DS **v12.0** or later, you can automatically boot straight into the Homebrew Launcher. To do this, boot your 3DS while holding `Select`. In the Luma3DS configuration menu, go to the `Homebrew autoboot` or `Hbmenu autoboot` option (depending on your Luma3DS version) and select `3DS`, then press `Start` to save and continue booting. Until you change this option back to `Off`, your 3DS should skip the 3DS Home Menu and bring you straight to the Homebrew Launcher when you turn it on.

### Booting directly into gba-net-boot
If you installed the [Homebrew Launcher Wrapper](https://github.com/PabloMK7/homebrew_launcher_dummy) as described in the guide, you can also boot straight into gba-net-boot, bypassing the 3DS Home Menu and Homebrew Launcher entirely, but there are some caveats to this:
* Pressing `Start` to exit gba-net-boot will just restart gba-net-boot instead of sending you back to the Homebrew Launcher or 3DS Home Menu, but you can still get back to the 3DS Home Menu by pressing the `Home` button
* If you go back to the 3DS Home Menu and try to launch the Homebrew Launcher from there, it will launch gba-net-boot instead
* This will remove your ability to load the Homebrew Launcher at all unless you have a second wrapper installed for it, which you might need to build yourself

If you still want to use this method, you will first need to get your 3DS set up to boot directly into the Homebrew Launcher as described above. Next, make a backup of the `boot.3dsx` file on your SD card, then copy `gba-net-boot.3dsx` to the root of your SD card and rename it to `boot.3dsx`. If you were keeping open_agb_firm in the same directory as gba-net-boot, you will also need to copy `open_agb_firm.firm` to the root of your SD card.

To restore the Homebrew Launcher, you just need to restore the backup you made of `boot.3dsx` to the root of your SD card. You can also remove the copy of `open_agb_firm.firm` you made there.

The reason the caveats mentioned above exist is because Luma3DS boots directly into the Homebrew Launcher by referencing the Title ID for the Homebrew Launcher Wrapper you installed to your 3DS Home Menu during the guide mentioned earlier. The Homebrew Launcher Wrapper looks for `boot.3dsx` on the root of your SD card and runs it. The `boot.3dsx` file is the Homebrew Launcher you would normally use to launch other `.3dsx` homebrew. By replacing `boot.3dsx` with gba-net-boot, the Homebrew Launcher Wrapper will run gba-net-boot instead, which means we've also lost our entry point to the Homebrew Launcher.

## How it works
When you run gba-net-send, it sends a UDP broadcast, which gba-net-boot listens for and responds to. When gba-net-send receives a response from gba-net-boot, gba-net-send establishes a TCP connection to gba-net-send to transfer your ROM. The ROM is saved to the root of your SD card as `rom.gba`.

After the transfer is completed and your ROM is saved, gba-net-boot copies open_agb_firm into a specific location in FCRAM and reboots the 3DS. Because we configured fastboot3DS to check for it and the FCRAM hasn't been cleared during the reboot, fastboot3DS sees that a firm (open_agb_firm in this case) has been loaded at this location and automatically boots into it.

## How to build
Install [devkitARM and 3ds-dev from devkitPro](https://devkitpro.org/wiki/Getting_Started), then run `make` in the project directory.

## Known issues
* If a transfer is interrupted, it will assume the transfer has completed successfully. File size needs to be communicated and checked.
* Sometimes the UDP broadcast or ack is dropped. If this happens, try sending the ROM again.
* Nothing is configurable right now. Configuration options are planned for the future.
* This is my first C project, so I'm sure some (probably most) of the code is probably ugly and/or follows bad practices.

## Thanks
* **[A9NC - ARM9 Netload Companion](https://github.com/d0k3/A9NC) devs** - code referenced for info on FCRAM firm boot
* **[That-Shortcut-Thingy](https://github.com/SUOlivia/That-Shortcut-Thingy) devs** - code referenced for info on FCRAM firm boot
* **SciresM** - clarification that `GSPGPU_FlushDataCache()` flushes CPU cache to RAM
* **TuxSH** - clarification that `GSPGPU_FlushDataCache()` flushes CPU cache to RAM
* **[Luma3DS](https://github.com/LumaTeam/Luma3DS) devs**
* **[fastboot3DS](https://github.com/derrekr/fastboot3DS) devs**
* **[libctru](https://github.com/devkitPro/libctru) devs**
* **[devkitPro](https://devkitpro.org/) contributors**
* **[3dbrew.org](https://www.3dbrew.org/) contributors**
* Everyone who has contributed to the homebrew scene
